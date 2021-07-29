// File $Id$
// Author: John Wu <John.Wu at ACM.org>
//         Lawrence Berkeley National Laboratory
// Copyright (c) 2000-2016 the Regents of the University of California
//
//  The implementation of class query.  It performs most of the query
//  processing functions, calls the data partition object for the actual
//  estimation work.
//
#if defined(_WIN32) && defined(_MSC_VER)
#pragma warning(disable:4786)   // some identifier longer than 256 characters
#endif
#include "query.h"      // class query (prototypes for all functions here)
#include "bundle.h"     // class bundle
#include "ibin.h"       // ibis::bin
#include "iroster.h"    // ibis::roster
#include "irelic.h"     // ibis::join::estimate
#include "bitvector64.h"

#include <stdio.h>      // remove()
#include <stdarg.h>     // vsprintf
#include <ctype.h>      // isspace, tolower

#include <memory>       // std::unique_ptr
#include <algorithm>    // std::sort
#include <sstream>      // std::ostringstream
#include <cmath>	// std::log

namespace ibis {
#if defined(TEST_SCAN_OPTIONS)
    extern int _scan_option;
#endif
}

/// Generate a weight based on estimated query processing costs.  This
/// function produces consistent result only for operators AND and OR.  It
/// assumes the cost of evaluating the negation to be zero.
double ibis::query::weight::operator()(const ibis::qExpr* ex) const {
    double res = dataset->nRows();
    switch (ex->getType()) {
    case ibis::qExpr::EXISTS: {
        res = (res>1.0 ? 1.0 : 0.0);
        break;}
    case ibis::qExpr::RANGE: {
        const ibis::qContinuousRange* tmp =
            reinterpret_cast<const ibis::qContinuousRange*>(ex);
        if (tmp != 0)
            res = dataset->estimateCost(*tmp);
        break;}
    case ibis::qExpr::DRANGE: {
        const ibis::qDiscreteRange* tmp =
            reinterpret_cast<const ibis::qDiscreteRange*>(ex);
        if (tmp != 0)
            res = dataset->estimateCost(*tmp);
        break;}
    case ibis::qExpr::INTHOD: {
        const ibis::qIntHod* tmp =
            reinterpret_cast<const ibis::qIntHod*>(ex);
        if (tmp != 0)
            res = dataset->estimateCost(*tmp);
        break;}
    case ibis::qExpr::UINTHOD: {
        const ibis::qUIntHod* tmp =
            reinterpret_cast<const ibis::qUIntHod*>(ex);
        if (tmp != 0)
            res = dataset->estimateCost(*tmp);
        break;}
    case ibis::qExpr::STRING: {
        const ibis::qString* tmp =
            reinterpret_cast<const ibis::qString*>(ex);
        if (tmp != 0)
            res = dataset->stringSearch(*tmp);
        break;}
    case ibis::qExpr::LIKE: {
        const ibis::qLike* tmp =
            reinterpret_cast<const ibis::qLike*>(ex);
        if (tmp != 0)
            res = dataset->patternSearch(*tmp);
        break;}
    default: { // most terms are evaluated through left and right children
        if (ex->getLeft()) {
            res = operator()(ex->getLeft());
            if (ex->getRight())
                res += operator()(ex->getRight());
        }
        else if (ex->getRight()) {
            res = operator()(ex->getRight());
        }
        break;}
    } // switch
    if (res < 0.0) // failed, give it an arbitrary number
        res = std::abs(res) * 2.5;
    return res;
} // ibis::query::weight::operator

///////////////////////////////////////////////////////////
// public functions of ibis::query
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
int ibis::query::setPartition(const part* tbl) {
    if (tbl == 0) return -1;
    if (tbl == mypart) return 0;
    if (tbl->nRows() == 0 || tbl->nColumns() == 0) return -1;

    // check the select clause against the new data partition
    if (! comps.empty()) {
        int ierr = comps.verify(*tbl);
        if (ierr != 0) {
            LOGGER(ibis::gVerbose >= 0)
                << "Warning -- query[" << myID << "]::setPartition can "
                "not assign the new partition " << tbl->name()
                << " because the function verify returned " << ierr;
            return -3;
        }
    }
    // check the where clause against the new partition
    if (! conds.empty()) {
        int ierr = conds.verify(*tbl);
        if (ierr != 0) {
            LOGGER(ibis::gVerbose > 2)
                << "Warning -- query[" << myID << "]::setPartition failed to "
                "find all names in \""
                << (conds.getString()?conds.getString():"<long expression>")
                << "\" in data partition " << tbl->name()
                << ", the function verify returned " << ierr;
            return -6;
        }
        if (conds.getExpr() == 0) {
            logWarning("setPartition", "The WHERE clause \"%s\" simplified to "
                       "an empty expression", conds.getString());
            return -5;
        }
    }

    writeLock control(this, "setPartition");
    if (dslock != 0) { // release the read lock on the data partition
        delete dslock;
        dslock = 0;
    }
    if (state == FULL_EVALUATE || state == BUNDLES_TRUNCATED ||
        state == HITS_TRUNCATED || state == QUICK_ESTIMATE) {
        dstime = 0;
        if (hits == sup) {
            delete hits;
            hits = 0;
            sup = 0;
        }
        else {
            delete hits;
            delete sup;
            hits = 0;
            sup = 0;
        }
        removeFiles();
    }

    mypart = tbl;
    if (! comps.empty()) {
        if (rids_in != 0 || conds.getExpr() != 0) {
            state = SPECIFIED;
            writeQuery();
        }
        else {
            state = SET_COMPONENTS;
        }
    }
    else {
        state = SET_PREDICATE;
    }
    if (ibis::gVerbose > 0) {
        logMessage("setPartition", "new data patition name %s", mypart->name());
    }
    return 0;
} // ibis::query::setPartition

/// Specifies the select clause for the query.  The select clause is a
/// string of column names separated by spaces, commas (,) or
/// semicolons(;).  Repeated calls to this function simply overwrite the
/// previous definition of the select clause.  If no select clause is
/// specified, the where clause alone determines whether record is a hit or
/// not.  The select clause will be reordered to make the plain column
/// names without functions appear before with functions.
int ibis::query::setSelectClause(const char* str) {
    if (str == 0 || *str == 0) return -2;
    if (*comps != 0 && stricmp(*comps, str) == 0) return 0;

    if (*str == '*' && str[1] == 0) {
        if (mypart != 0) {
            const ibis::table::stringArray sl = mypart->columnNames();
            ibis::selectClause sc(sl);
            writeLock control(this, "setSelectClause");
            comps.swap(sc);
        }
    }
    else {
        ibis::selectClause sc(str);
        if (mypart != 0) {
            int ierr = sc.verify(*mypart);
            if (ierr != 0) {
                LOGGER(ibis::gVerbose > 2)
                    << "Warning -- query[" << myID << "]::setSelectClause("
                    << str << ") failed to find column names in data partition "
                    << mypart->name();
                return -3;
            }
        }

        writeLock control(this, "setSelectClause");
        comps.swap(sc);
    }

    if (state == FULL_EVALUATE || state == BUNDLES_TRUNCATED ||
        state == HITS_TRUNCATED || state == QUICK_ESTIMATE) {
        dstime = 0;
        if (hits == sup) {
            delete hits;
            hits = 0;
            sup = 0;
        }
        else {
            delete hits;
            delete sup;
            hits = 0;
            sup = 0;
        }
        removeFiles();
    }

    if (rids_in || conds.getExpr() != 0) {
        state = SPECIFIED;
        writeQuery();
    }
    else {
        state = SET_COMPONENTS;
    }
    if (ibis::gVerbose > 1) {
        logMessage("setSelectClause", "SELECT %s", *comps);
    }
    return 0;
} // ibis::query::setSelectClause

/// Specify the where clause in the string form.
/// The where clause is a string representing a list of range conditions.
/// By SQL convention, an empty where clause matches all rows.
/// This function may be called multiple times and each invocation will
/// overwrite the previous where clause.
int ibis::query::setWhereClause(const char* str) {
    if (str == 0 || *str == static_cast<char>(0))
        str = "1=1"; // default string
    if (conds.getString() != 0 && stricmp(conds.getString(), str) == 0)
        return 0; // no change in where clause

    int ierr = 0;
    try {
        ibis::whereClause tmp(str);
        if (tmp.getExpr() == 0) {
            logWarning("setWhereClause", "failed to parse the WHERE clause "
                       "\"%s\"", str);
            return -5;
        }
        if (mypart != 0) {
            int ierr = tmp.verify(*mypart);
            if (ierr != 0) {
                LOGGER(ibis::gVerbose > 2)
                    << "Warning -- query[" << myID << "]::setWhereClause "
                    "failed to verify the where clause \"" << str
                    << "\" with partition " << mypart->name()
                    << ", the function verify returned " << ierr;
                ierr = -6;
            }
            if (tmp.getExpr() == 0) {
                LOGGER(ibis::gVerbose >= 0)
                    << "Warning -- query[" << myID << "]::setWhereClause "
                    "failed to simplify \"" << str
                    << "\" into a valid query expression";
                return -5;
            }
        }

        if (ibis::gVerbose > 2) {
            if (conds.getString() != 0)
                logMessage("setWhereClause", "replace previous condition "
                           "\"%s\" with \"%s\".", conds.getString(), str);
            else
                logMessage("setWhereClause", "add a new where clause \"%s\".",
                           str);
        }
        writeLock lck(this, "setWhereClause");
        conds.swap(tmp);

        if (state == FULL_EVALUATE || state == BUNDLES_TRUNCATED ||
            state == HITS_TRUNCATED || state == QUICK_ESTIMATE) {
            dstime = 0;
            if (hits == sup) {
                delete hits;
                hits = 0;
                sup = 0;
            }
            else {
                delete hits;
                delete sup;
                hits = 0;
                sup = 0;
            }
            removeFiles();
        }


        if (! comps.empty()) {
            state = SPECIFIED;
            writeQuery();
        }
        else {
            state = SET_PREDICATE;
        }

        if (ibis::gVerbose > 0) {
            ibis::util::logger lg;
            lg() << "query[" << myID << "]::setWhereClause -- where \""
                 << str << "\"";
            if (ibis::gVerbose > 3) {
                lg() << "\n  Translated the WHERE clause into: ";
                conds.getExpr()->printFull(lg());
            }
        }
    }
    catch (...) {
        logWarning("setWhereClause", "failed to parse the where clause "
                   "\"%s\"", str);
        ierr = -5;
    }
    return ierr;
} // ibis::query::setWhereClause

/// Specify the where clause as a set of conjunctive ranges.
/// This function accepts a set of range conditions expressed by the three
/// vectors.  The arrays are expected to be of the same size, and each
/// triplet <names[i], lbounds[i], rbounds[i]> are interpreted as
/// @code
/// names[i] between lbounds[i] and rbounds[i]
/// @endcode
/// The range conditions are joined together with the AND operator.
/// If vectors lbounds and rbounds are not the same size, then the missing
/// one is consider to represent an open boundary.  For example, if
/// lbounds[4] exists but not rbounds[4], they the range condition is
/// interpreted as
/// @code
/// lbounds[4] <= names[4]
/// @endcode
int ibis::query::setWhereClause(const std::vector<const char*>& names,
                                const std::vector<double>& lbounds,
                                const std::vector<double>& rbounds) {
    uint32_t nts = names.size();
    if (rbounds.size() <= lbounds.size()) {
        if (nts > lbounds.size())
            nts = lbounds.size();
    }
    else if (nts > rbounds.size()) {
        nts = rbounds.size();
    }
    if (nts == 0) return -4;

    ibis::qExpr *expr; // to hold the new conditions
    if (lbounds.size() > 0) {
        if (rbounds.size() > 0) {
            double lb = (lbounds[0] <= rbounds[0] ?
                         lbounds[0] : rbounds[0]);
            double rb = (lbounds[0] <= rbounds[0] ?
                         rbounds[0] : lbounds[0]);
            expr = new ibis::qContinuousRange(lb, ibis::qExpr::OP_LE, names[0],
                                              ibis::qExpr::OP_LE, rb);
        }
        else {
            expr = new ibis::qContinuousRange(names[0], ibis::qExpr::OP_GE,
                                              lbounds[0]);
        }
    }
    else {
        expr = new ibis::qContinuousRange(names[0], ibis::qExpr::OP_LE,
                                          rbounds[0]);
    }
    for (uint32_t i = 1; i < nts; ++ i) {
        ibis::qExpr *tmp = new ibis::qExpr(ibis::qExpr::LOGICAL_AND);
        tmp->setLeft(expr);
        expr = tmp;
        if (lbounds.size() > i) {
            if (rbounds.size() > i) {
                double lb = (lbounds[i] <= rbounds[i] ?
                             lbounds[i] : rbounds[i]);
                double rb = (lbounds[i] <= rbounds[i] ?
                             rbounds[i] : lbounds[i]);
                tmp = new ibis::qContinuousRange(lb, ibis::qExpr::OP_LE,
                                                 names[i],
                                                 ibis::qExpr::OP_LE, rb);
            }
            else {
                tmp = new ibis::qContinuousRange(names[i], ibis::qExpr::OP_GE,
                                                 lbounds[i]);
            }
        }
        else {
            tmp = new ibis::qContinuousRange(names[i], ibis::qExpr::OP_LE,
                                             rbounds[i]);
        }
        expr->setRight(tmp);
    }

    int ierr = 0;
    if (mypart != 0) {
        ibis::whereClause wc;
        wc.setExpr(expr);
        int ierr = wc.verify(*mypart);
        if (ierr != 0) {
            LOGGER(ibis::gVerbose >= 0)
                << "Warning -- query[" << myID << "]::setWhereClause failed "
                "to find some variable names in data partition "
                << mypart->name() << ", the function verify returned " << ierr;
            ierr = -6;
        }
        if (wc.getExpr() == 0) {
            LOGGER(ibis::gVerbose >= 0)
                << "Warning -- query[" << myID << "]::setWhereClause failed "
                "to simplify " << names.size() << " range condition"
                << (names.size() > 1 ? "s" : "")
                << " into a valid query expression";
            if (! comps.empty())
                state = SET_COMPONENTS;
            else
                state = UNINITIALIZED;
            delete expr;
            return -5;
        }
    }

    writeLock lck(this, "setWhereClause");
    if (state == FULL_EVALUATE || state == BUNDLES_TRUNCATED ||
        state == HITS_TRUNCATED || state == QUICK_ESTIMATE) {
        dstime = 0;
        if (hits == sup) {
            delete hits;
            hits = 0;
            sup = 0;
        }
        else {
            delete hits;
            delete sup;
            hits = 0;
            sup = 0;
        }
        removeFiles();
    }

    // assign the new query conditions to conds
    conds.setExpr(expr);
    delete expr;

    if (! comps.empty()) {
        state = SPECIFIED;
        writeQuery();
    }
    else {
        state = SET_PREDICATE;
    }
    LOGGER(ibis::gVerbose > 1)
        << "query[" << myID << "]::setWhereClause converted three arrays to \""
        << *(conds.getExpr()) << "\"";
    return ierr;
} // ibis::query::setWhereClause

/// Specify the where clause through a qExpr object.
/// This function accepts a user constructed query expression object.  It
/// can be used to bypass the parsing of where clause string.
///
/// @note The query object will hold a copy of the incoming object.
int ibis::query::setWhereClause(const ibis::qExpr* qx) {
    if (qx == 0) return -4;

    int ierr = 0;
    ibis::whereClause wc;
    wc.setExpr(qx);
    if (mypart != 0) {
        int ierr = wc.verify(*mypart);
        if (ierr != 0) {
            LOGGER(ibis::gVerbose >= 0)
                << "Warning -- query[" << myID << "]::setWhereClause failed "
                "to find some names used in the input qExpr "
                << static_cast<const void*>(qx) << " in data partition "
                << mypart->name() << ", the function verify returned " << ierr;
            ierr = -6;
        }
        if (wc.getExpr() == 0) {
            LOGGER(ibis::gVerbose >= 0)
                << "Warning -- query[" << myID << "]::setWhereClause failed "
                "to simplify the input qExpr " << static_cast<const void*>(qx)
                << " into a valid query expression";
            if (! comps.empty())
                state = SET_COMPONENTS;
            else
                state = UNINITIALIZED;
            return -5;
        }
    }
    if (ibis::gVerbose > 0 &&
        wc.getExpr()->nItems() <= static_cast<unsigned>(ibis::gVerbose)) {
        wc.resetString(); // regenerate the string form of the query expression
    }

    writeLock lck(this, "setWhereClause");
    wc.swap(conds);

    if (state == FULL_EVALUATE || state == BUNDLES_TRUNCATED ||
        state == HITS_TRUNCATED || state == QUICK_ESTIMATE) {
        dstime = 0;
        if (hits == sup) {
            delete hits;
            hits = 0;
            sup = 0;
        }
        else {
            delete hits;
            delete sup;
            hits = 0;
            sup = 0;
        }
        removeFiles();
    }

    if (! comps.empty()) {
        state = SPECIFIED;
        writeQuery();
    }
    else {
        state = SET_PREDICATE;
    }
    LOGGER(ibis::gVerbose > 1)
        << "query[" << myID
        << "]::setWhereClause accepted new query conditions \""
        << (conds.getString() ? conds.getString() : "<long expression>")
        << "\"";
    return ierr;
} // ibis::query::setWhereClause

/// Add a set of conditions to the existing where clause.  The new query
/// expression is joined with the existing conditions with the AND operator.
///
/// @note This object will have a copy of the incoming query expression.
int ibis::query::addConditions(const ibis::qExpr* qx) {
    if (qx == 0) return -4;

    int ierr = 0;
    {
        writeLock lck(this, "addConditions");
        conds.addExpr(qx);

        if (conds.getExpr() == 0) {
            LOGGER(ibis::gVerbose >= 0)
                << "Warning -- query[" << myID << "]::addConditions failed "
                "to combine the incoming qExpr " << static_cast<const void*>(qx)
                << " with the existing ones";
            if (! comps.empty())
                state = SET_COMPONENTS;
            else
                state = UNINITIALIZED;
            return -5;
        }
        if (ibis::gVerbose > 0 && conds.getExpr()->nItems() <=
            static_cast<unsigned>(ibis::gVerbose)) {
            conds.resetString(); // regenerate the string form
        }

        if (state == FULL_EVALUATE || state == BUNDLES_TRUNCATED ||
            state == HITS_TRUNCATED || state == QUICK_ESTIMATE) {
            dstime = 0;
            if (hits == sup) {
                delete hits;
                hits = 0;
                sup = 0;
            }
            else {
                delete hits;
                delete sup;
                hits = 0;
                sup = 0;
            }
            removeFiles();
        }

        if (! comps.empty()) {
            state = SPECIFIED;
            writeQuery();
        }
        else {
            state = SET_PREDICATE;
        }
    }
    if (mypart != 0) {
        int ierr = conds.verify(*mypart);
        if (ierr != 0) {
            LOGGER(ibis::gVerbose >= 0)
                << "Warning -- query[" << myID << "]::addConditions failed "
                "to find some names used in qExpr "
                << static_cast<const void*>(conds.getExpr())
                << " in data partition " << mypart->name()
                << ", the function verify returned " << ierr;
            ierr = -6;
        }
    }
    LOGGER(ibis::gVerbose > 1)
        << "query[" << myID
        << "]::addConditions accepted new query conditions \""
        << (conds.getString() ? conds.getString() : "<long expression>")
        << "\"";
    return ierr;
} // ibis::query::addConditions

/// Add a set of conditions to the existing where clause.  The new query
/// expression is joined with the existing conditions with the AND operator.
///
int ibis::query::addConditions(const char* qx) {
    if (qx == 0 || *qx == 0) return -4;

    int ierr = 0;
    {
        writeLock lck(this, "addConditions");
        conds.addConditions(qx);

        if (conds.getExpr() == 0) {
            LOGGER(ibis::gVerbose >= 0)
                << "Warning -- query[" << myID << "]::addConditions failed "
                "to combine the incoming qExpr \"" << qx
                << "\" with the existing ones";
            if (! comps.empty())
                state = SET_COMPONENTS;
            else
                state = UNINITIALIZED;
            return -5;
        }
        if (ibis::gVerbose > 0 && conds.getExpr()->nItems() <=
            static_cast<unsigned>(ibis::gVerbose)) {
            conds.resetString(); // regenerate the string form
        }

        if (state == FULL_EVALUATE || state == BUNDLES_TRUNCATED ||
            state == HITS_TRUNCATED || state == QUICK_ESTIMATE) {
            dstime = 0;
            if (hits == sup) {
                delete hits;
                hits = 0;
                sup = 0;
            }
            else {
                delete hits;
                delete sup;
                hits = 0;
                sup = 0;
            }
            removeFiles();
        }

        if (! comps.empty()) {
            state = SPECIFIED;
            writeQuery();
        }
        else {
            state = SET_PREDICATE;
        }
    }
    if (mypart != 0) {
        int ierr = conds.verify(*mypart);
        if (ierr != 0) {
            LOGGER(ibis::gVerbose >= 0)
                << "Warning -- query[" << myID << "]::addConditions failed "
                "to find some names used in qExpr "
                << static_cast<const void*>(conds.getExpr())
                << " in data partition " << mypart->name()
                << ", the function verify returned " << ierr;
            ierr = -6;
        }
    }
    LOGGER(ibis::gVerbose > 1)
        << "query[" << myID
        << "]::addConditions accepted new query conditions \""
        << (conds.getString() ? conds.getString() : "<long expression>")
        << "\"";
    return ierr;
} // ibis::query::addConditions

/// Specify a list of Row IDs for the query object.
/// Select the records with an RID in the list of RIDs.
///
/// @note The incoming RIDs are copied.
///
/// @note The RIDs and the where clause can be used together.  When they
/// are both specified, they are used in conjuction.  In other word, the
/// hits of the query will contain only the records that satisfy the where
/// clause and have an RID in the list of RIDs.
int ibis::query::setRIDs(const ibis::RIDSet& rids) {
    if (rids.empty()) return -7;

    writeLock lck(this, "setRIDs");
    if (rids_in != 0) delete rids_in;
    rids_in = new RIDSet();
    rids_in->deepCopy(rids);
    std::sort(rids_in->begin(), rids_in->end());
    //    ibis::util::sortRIDs(*rids_in);

    if (state == FULL_EVALUATE || state == BUNDLES_TRUNCATED ||
        state == HITS_TRUNCATED || state == QUICK_ESTIMATE) {
        dstime = 0;
        if (hits == sup) {
            delete hits;
            hits = 0;
            sup = 0;
        }
        else {
            delete hits;
            delete sup;
            hits = 0;
            sup = 0;
        }
        removeFiles();
    }

    if (! comps.empty()) {
        writeQuery();
        state = SPECIFIED;
    }
    else {
        state = SET_RIDS;
    }
    LOGGER(ibis::gVerbose > 0)
        << "query[" << myID << "]::setRIDs selected "
        << rids_in->size() << " RID(s) for an RID query";
    return 0;
} // ibis::query::setRIDs

/// Function to perform estimation.  It computes a lower bound and an upper
/// bound of hits.  It uses the indexes only.  If necessary it will build
/// new indexes.  The lower bound contains records that are definitely hits
/// and the upper bound contains all hits but may also contain some records
/// that are not hits.  We also call the records in the upper bound
/// candidates.
///
/// Returns 0 for success, a negative value for error.
int ibis::query::estimate() {
    if (mypart == 0 || mypart->nRows() == 0 || mypart->nColumns() == 0)
        return -1;
    std::string evt = "query[";
    evt += myID;
    evt += "]::estimate";
    if (rids_in == 0 && conds.empty() && comps.empty()) {
        // not ready for this yet
        LOGGER(ibis::gVerbose > 1)
            << "Warning -- " << evt << " must have either a valid query "
            "condition (the WHERE clause) or a list of RIDs";
        return -8;
    }
    LOGGER(ibis::gVerbose > 3)
        << evt << " -- starting to estimate query";

    double pcnt = ibis::fileManager::instance().pageCount();
    if (dstime != 0 && dstime != mypart->timestamp()) {
        // clear the current results and prepare for re-evaluation
        dstime = 0;
        if (hits == sup) {
            delete hits;
            hits = 0;
            sup = 0;
        }
        else {
            delete hits;
            delete sup;
            hits = 0;
            sup = 0;
        }
        removeFiles();
        state = SPECIFIED;
    }
    if (state < QUICK_ESTIMATE) {
        writeLock lck(this, "estimate");
        if (state < QUICK_ESTIMATE) {
            ibis::horometer timer;
            if (ibis::gVerbose > 0)
                timer.start();
            try {
                if (dslock == 0) { // acquire read lock on the dataset
                    dslock = new ibis::part::readLock(mypart, myID);
                    dstime = mypart->timestamp();
                }

#ifndef DONOT_REORDER_EXPRESSION
                if (conds.getExpr() != 0 && false == conds->directEval())
                    reorderExpr();
#endif
                getBounds(); // actual function to perform the estimation
                state = QUICK_ESTIMATE;
            }
            catch (const ibis::bad_alloc& e) {
                if (dslock != 0) {
                    delete dslock;
                    dslock = 0;
                }

                logError("estimate", "encountered a memory allocation error "
                         "(%s) while resolving \"%s\"", e.what(),
                         conds.getString());
                ibis::util::emptyCache();
                return -9;
            }
            catch (const std::exception& e) {
                if (dslock != 0) {
                    delete dslock;
                    dslock = 0;
                }

                logError("estimate", "encountered a std::exception "
                         "(%s) while resolving \"%s\"", e.what(),
                         conds.getString());
                ibis::util::emptyCache();
                return -9;
            }
            catch (const char* s) {
                if (dslock != 0) {
                    delete dslock;
                    dslock = 0;
                }

                logError("estimate", "encountered a string exception (%s) "
                         "while resolving \"%s\"", s, conds.getString());
                ibis::util::emptyCache();
                return -9;
            }
            catch (...) {
                if (dslock != 0) {
                    delete dslock;
                    dslock = 0;
                }

                logError("estimate", "encountered a unexpected exception "
                         "while resolving \"%s\"", conds.getString());
                ibis::util::emptyCache();
                return -9;
            }
            if (ibis::gVerbose > 0) {
                timer.stop();
                LOGGER(ibis::gVerbose > 0)
                    << evt << " -- time to compute the bounds: "
                    << timer.CPUTime() << " sec(CPU), "
                    << timer.realTime() << " sec(elapsed)";
            }
        }
    }

    if (hits==0 && sup==0) {
        logWarning("estimate", "failed to generate estimated hits");
    }
    else if (ibis::gVerbose > 0) {
        if (conds.getExpr()) {
            if (hits && sup && hits != sup) {
                LOGGER(ibis::gVerbose > 0)
                    << evt << " -- # of hits for query \"" 
                    << (conds.getString() ? conds.getString() :
                        "<long expression>")
                    << "\" is between " << hits->cnt() << " and " << sup->cnt();
            }
            else if (hits) {
                LOGGER(ibis::gVerbose > 0)
                    << evt << " -- # of hits for query \""
                    << (conds.getString() ? conds.getString() :
                        "<long expression>")
                    << "\" is " << hits->cnt();
            }
            else {
                if (sup == 0) {
                    sup = new ibis::bitvector;
                    mypart->getNullMask(*sup);
                }
                sup->adjustSize(0, mypart->nRows());
                hits = new ibis::bitvector;
                hits->set(0, sup->size());
                LOGGER(ibis::gVerbose > 0)
                    << "Warning -- " << evt << " failed to estimate the hits,"
                    " assume the number of hits to be in between 0 and "
                    << sup->cnt();
            }
        }
        else {
            LOGGER(ibis::gVerbose > 0)
                << evt << " -- # of hits for the OID query is "
                << hits->cnt();
        }
        if (ibis::gVerbose > 4) {
            pcnt = ibis::fileManager::instance().pageCount() - pcnt;
            LOGGER(pcnt > 0.0)
                << evt << " -- read(unistd.h) accessed " << pcnt
                << " pages during the execution of this function";
        }
        if ((rids_in != 0 || conds.getExpr() != 0) &&
            (ibis::gVerbose > 30 ||
             (ibis::gVerbose > 8 &&
              (1U<<ibis::gVerbose) >=
              (hits?hits->bytes():0)+(sup?sup->bytes():0)))) {
            if (hits == sup) {
                LOGGER(ibis::gVerbose >= 0) << "The hit vector" << *hits;
            }
            else {
                LOGGER(ibis::gVerbose >= 0 && hits != 0)
                    << "The sure hits" << *hits;
                LOGGER(ibis::gVerbose >= 0 && sup != 0)
                    << "The possible hit" << *sup;
            }
        }
    }
    return 0;
} // ibis::query::estimate

/// Return the number of records in the lower bound.
long ibis::query::getMinNumHits() const {
    readLock lck(this, "getMinNumHits");
    long nHits = (hits != 0 ? static_cast<long>(hits->cnt()) : -1);
    LOGGER(ibis::gVerbose > 11)
        << "query[" << myID << "]::getMinNumHits -- minHits = " << nHits;

    return nHits;
}

/// Return the number of records in the upper bound.
long ibis::query::getMaxNumHits() const {
    readLock lck(this, "getMaxNumHits");
    long nHits = (sup != 0 ? static_cast<long>(sup->cnt()) :
                  (hits ? static_cast<long>(hits->cnt()) : -1));
    LOGGER(ibis::gVerbose > 11)
        << "query[" << myID << "]::getMaxNumHits -- maxHits = " << nHits;
    return nHits;
}

/// Extract the positions of candidates.  This is meant to be used after
/// calling the function estimate.  It will return the position the hits if
/// they are known already.  If no estimate is known or the query is formed
/// yet, it will return a negative number to indicate error.  Upon a
/// successful completion of this function, the return value should be the
/// rids.size().
long ibis::query::getCandidateRows(std::vector<uint32_t> &rids) const {
    if (hits == 0 && sup == 0)
        return -1; // no estimate yet

    const ibis::bitvector *tmp = (hits != 0 ? hits : sup);
    long ierr = tmp->cnt();
    try {
        rids.clear();
        rids.reserve(ierr);
        for (ibis::bitvector::indexSet is = tmp->firstIndexSet();
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
            << "query[" << myID
            << "]::getCandidateRows failed to extract the 1s in hits";
        return -2;
    }
} // ibis::countQuery::getCandidateRows

/// Computes the exact hits.  The same answer shall be computed whether
/// there is any index or not.  The argument evalSelect indicates whether
/// the select clause should be evaluated at the same time.  If its value
/// is true, the columns specified in the select clause will be retrieved
/// from disk and stored in the temporary location for this query.  If not,
/// the qualified values need to be retrieved by calling one of getRIDs,
/// getQualifiedInts, getQualifiedFloats, getQualifiedDoubles and similar
/// functions.  These functions work with one column at a time.  Note that
/// if the data is dynamically varying, the values retrived later could be
/// different from the values extracted while processing this function.
///
/// Returns 0 or a positive integer for success, a negative value for
/// error.  Note that when it returns 0, it indicates that the number of
/// hits is 0.  However, when it returns a positive value, the return value
/// may not be the number of hits.  This semantics is actually implemented
/// in the support functions such as computeHits, doEvaluate, and doScan.
///
/// @see getQualifiedInts
int ibis::query::evaluate(const bool evalSelect) {
    if (mypart == 0 || mypart->nRows() == 0 || mypart->nColumns() == 0)
        return -1;
    if (rids_in == 0 && conds.empty() && comps.empty()) {
        if (ibis::gVerbose > 1)
            logMessage("evaluate", "must have either a SELECT clause, "
                       "a WHERE clause, or a RID list");
        return -8;
    }
    if (ibis::gVerbose > 3) {
        logMessage("evaluate", "starting to evaluate the query for "
                   "user \"%s\"", user);
    }

    int ierr=-1;
    ibis::horometer timer;
    double pcnt = ibis::fileManager::instance().pageCount();
    writeLock lck(this, "evaluate");
    if ((state < FULL_EVALUATE) ||
        (dstime != 0 && dstime != mypart->timestamp())) {
        if (dstime != 0 && dstime != mypart->timestamp()) {
            // clear the current results and prepare for re-evaluation
            dstime = 0;
            if (hits == sup) {
                delete hits;
                hits = 0;
                sup = 0;
            }
            else {
                delete hits;
                delete sup;
                hits = 0;
                sup = 0;
            }
            removeFiles();
            state = SPECIFIED;
        }
        if (ibis::gVerbose > 0)
            timer.start();
        try {
            if (dslock != 0 && dstime == mypart->timestamp() && hits != 0 &&
                (sup == 0 || sup == hits)) { // nothing to do
                ierr = hits->sloppyCount();
            }
            else {
                if (dslock == 0) { // acquire read lock on mypart
                    dslock = new ibis::part::readLock(mypart, myID);
                    dstime = mypart->timestamp();
                }

                ierr = computeHits(); // do actual computation here
                if (ierr < 0) return ierr;
            }
            if (hits != 0 && hits->sloppyCount() > 0 && ! conds.empty()
                && ibis::gVerbose > 3) {
                const unsigned nb = hits->size();
                const unsigned nc = hits->cnt();
                const unsigned sz = hits->bytes();
                double cf = ibis::bitvector::clusteringFactor(nb, nc, sz);
                double rw = ibis::bitvector::randomSize(nb, nc);
                double eb = static_cast<double>(countPages(4))
                    * ibis::fileManager::pageSize();
                LOGGER(1)
                    << "query["<< myID << "]::evaluate -- the hit contains "
                    << nb << " bit" << (nb>1 ? "s" : "") << " with " << nc
                    << " bit" << (nc>1 ? "s" : "") << " set(=1) taking up "
                    << sz << " byte" << (sz>1 ? "s" : "")
                    << "; the estimated clustering factor is " << cf
                    << "; had the bits been randomly spread out, the expected "
                    "size would be " << rw << " bytes; estimated number of "
                    "bytes to be read in order to access 4-byte values is "
                    << eb;
            }
        }
        catch (...) {
            try {
                if (dslock != 0) { // temporarily give the read lock on data
                    delete dslock;
                    dslock = 0;
                }
                if (ibis::fileManager::iBeat() % 3 == 0) { // random delay
                    ibis::util::quietLock lock(&ibis::util::envLock);
#if defined(unix) || defined(__linux__) || defined(__CYGWIN__) || defined(__APPLE__) || defined(__FreeBSD)
                    LOGGER(ibis::gVerbose > 0)
                        << " .. out of memory, sleep for a second to see "
                        "if the situation changes";
                    sleep(1);
#endif
                }
                ibis::util::emptyCache();

                if (dslock == 0) { // acquire read lock on mypart
                    dslock = new ibis::part::readLock(mypart, myID);
                    dstime = mypart->timestamp();
                }
                ierr = computeHits(); // do actual computation here
                if (ierr < 0) return ierr;
            }
            catch (const ibis::bad_alloc& e) {
                if (dslock != 0) {
                    delete dslock;
                    dslock = 0;
                }
                logError("evaluate", "encountered a memory allocation error "
                         "(%s) while resolving \"%s\"", e.what(),
                         conds.getString());
                ibis::util::emptyCache();
                return -9;
            }
            catch (const std::exception& e) {
                if (dslock != 0) {
                    delete dslock;
                    dslock = 0;
                }

                logError("evaluate", "encountered a std::exception (%s) "
                         "while resolving \"%s\"", e.what(), conds.getString());
                ibis::util::emptyCache();
                return -9;
            }
            catch (const char *e) {
                if (dslock != 0) {
                    delete dslock;
                    dslock = 0;
                }

                logError("evaluate", "encountered a string exception (%s) "
                         "while resolving \"%s\"", e, conds.getString());
                ibis::util::emptyCache();
                return -9;
            }
            catch (...) {
                if (dslock != 0) {
                    delete dslock;
                    dslock = 0;
                }
                logError("evaluate", "encountered a unexpected exception "
                         "while resolving \"%s\"", conds.getString());
                ibis::util::emptyCache();
                return -9;
            }
        }
        if (ibis::gVerbose > 0) {
            const long unsigned nhits = hits->cnt();
            timer.stop();
            logMessage("evaluate", "time to compute the %lu "
                       "hit%s: %g sec(CPU), %g sec(elapsed).",
                       nhits, (nhits > 1 ? "s" : ""),
                       timer.CPUTime(), timer.realTime());
        }
        state = FULL_EVALUATE;
        writeQuery(); // record the current status
    }

    if (myDir && hits->sloppyCount() > 0 && evalSelect) {
        // generate the bundles
        writeHits();
        if (ibis::gVerbose > 1) timer.start();
        ibis::bundle* bdl = ibis::bundle::create(*this);
        if (bdl != 0) {
            bdl->write(*this);
            delete bdl;
            if (ibis::gVerbose > 1) {
                timer.stop();
                logMessage("evaluate", "time to read qualified values "
                           "and write to disk (%s) is "
                           "%g sec(CPU), %g sec(elapsed).", myDir,
                           timer.CPUTime(),
                           timer.realTime());
            }
        }

        state = FULL_EVALUATE;
        writeQuery(); // record the current status
        if (ibis::gVerbose > 0) {
            timer.stop();
            logMessage("evaluate", "time to compute the %lu "
                       "hits: %g sec(CPU), %g sec(elapsed).",
                       static_cast<long unsigned>(hits->cnt()),
                       timer.CPUTime(), timer.realTime());
        }
        else
            logWarning("evaluate", "failed to construct ibis::bundle");
    }

    if (dslock != 0) {
        // make sure the read lock on the data partition is released
        delete dslock;
        dslock = 0;
    }
    if (state != FULL_EVALUATE) {
        logWarning("evaluate", "failed to compute the hit vector");
        ierr = -9;
    }
    else if (hits == 0) {
        if (ibis::gVerbose > 0)
            logMessage("evaluate", "nHits = 0.");
    }
    else if (ibis::gVerbose > 0) {
        if (conds.getExpr() != 0) {
            if (! comps.empty())
                logMessage("evaluate", "user %s SELECT %s FROM %s WHERE "
                           "%s ==> %lu hit%s.", user, *comps, mypart->name(),
                           (conds.getString() ? conds.getString() :
                            "<long expression>"),
                           static_cast<long unsigned>(hits->cnt()),
                           (hits->cnt()>1?"s":""));
            else
                logMessage("evaluate", "user %s FROM %s WHERE %s ==> "
                           "%lu hit%s.", user, mypart->name(),
                           (conds.getString() ? conds.getString() :
                            "<long expression>"),
                           static_cast<long unsigned>(hits->cnt()),
                           (hits->cnt()>1?"s":""));
        }
        else if (rids_in != 0) {
            logMessage("evaluate", "user %s RID list of %lu elements ==> "
                       "%lu hit%s.", user,
                       static_cast<long unsigned>(rids_in->size()),
                       static_cast<long unsigned>(hits->cnt()),
                       (hits->cnt()>1?"s":""));
        }
        if (ibis::gVerbose > 3) {
            pcnt = ibis::fileManager::instance().pageCount() - pcnt;
            if (pcnt > 0.0)
                logMessage("evaluate", "read(unistd.h) accessed "
                           "%g pages during the execution of this function",
                           pcnt);
        }
        LOGGER((rids_in != 0 || conds.getExpr() != 0) &&
               (ibis::gVerbose > 30 ||
                (ibis::gVerbose > 8 &&
                 (1U<<ibis::gVerbose) >= hits->bytes())))
            << "The hit vector" << *hits;
    }
    return ierr;
} // ibis::query::evaluate

/// Compute the number of records in the exact solution.  This function
/// will return the number of hits based on the internally stored
/// information or other inexpensive options.  It will not perform a full
/// evaluation to compute the numbers of hits.  It is intended to be called
/// after calling ibis::query::evaluate.  The return value will be -1 if it
/// is not able to determine the number of hits.
long int ibis::query::getNumHits() const {
    long int nHits = -1;
    if (mypart != 0 && mypart->nRows() > 0) {
        if (state < QUICK_ESTIMATE)
            const_cast<query*>(this)->estimate();

        readLock lock(this, "getNumHits");
        if (conds.empty())
            nHits = mypart->nRows();
        else if (hits != 0 && (sup == 0 || sup == hits))
            nHits = static_cast<long int>(hits->cnt());
        else if (conds.getExpr() != 0 &&
                 dynamic_cast<const ibis::qRange*>(conds.getExpr()) != 0)
            nHits = mypart->countHits
                (*static_cast<const ibis::qRange*>(conds.getExpr()));
    }
    return nHits;
} // ibis::query::getNumHits

/// Extract the positions of the bits that are 1s in the solution.  This is
/// only valid after the query has been evaluated.  If it has not been
/// evaluated, it will return a negative number to indicate error.  Upon
/// a successful completion of this function, the return value should be
/// the rids.size().
long ibis::query::getHitRows(std::vector<uint32_t> &rids) const {
    if (hits == 0 || (sup != 0 && sup != hits))
        return -1; // no accurate solution yet

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
            << "query[" << myID
            << "]::getHitRows failed to extract the 1s in hits";
        return -2;
    }
} // ibis::countQuery::getHitRows

/// Count the number of hits.  Don't generate the hit vector if not already
/// there.  It only work for queries containing a single range condition.
/// Furthermore, this function does not obtain a read lock on the query or
/// the partition.  Therefore it is possible for another thread to modify
/// the query object while the evaluation is in progress.
long ibis::query::countHits() const {
    long int ierr = -1;
    if (hits != 0 && (sup == 0 || sup == hits))
        ierr = hits->cnt();
    else if (mypart != 0 && mypart->nRows() != 0 && conds.getExpr() != 0 &&
             dynamic_cast<const ibis::qRange*>(conds.getExpr()) != 0)
        ierr = mypart->countHits(*static_cast<const ibis::qRange*>
                                 (conds.getExpr()));
    else if (conds.empty())
        ierr = mypart->nRows();
    return ierr;
} // ibis::query::countHits

/// Re-order the results according to the new "ORDER BY" specification.  It
/// returns 0 if it completes successfully.  It returns a negative number
/// to indicate error.  If @c direction >= 0, sort the values in ascending
/// order, otherwise, sort them in descending order.
///
/// @note The results stored in ibis::bundle and ibis::query::result are
/// already ordered according to the columns specified in the select
/// clause.  One only needs to call this function to re-order the results
/// differently.
int ibis::query::orderby(const char *names) const {
    if (myDir == 0)
        return -10;
    if (state != FULL_EVALUATE || state != BUNDLES_TRUNCATED
        || state != HITS_TRUNCATED)
        return -11;
    ibis::horometer timer;
    if (ibis::gVerbose > 2)
        timer.start();
    ibis::bundle *bdl = ibis::bundle::create(*this);
    if (bdl != 0) {
        bdl->reorder(names);
        bdl->write(*this);
        delete bdl;
    }
    else {
        logWarning("orderby", "failed to create bundles");
        return -12;
    }
    if (ibis::gVerbose > 2) {
        timer.stop();
        logMessage("orderby", "reordered according to %s using %g sec(CPU), "
                   "%g sec(elapsed)", names, timer.CPUTime(),
                   timer.realTime());
    }
    return 0;
} // ibis::query::orderby

/// Truncate the results to provide the top-K rows.  It returns the
/// number of results kept, which is the smaller of the current number
/// of rows and the input argument @c keep.  A negative value is
/// returned in case of error, e.g., query has not been fully
/// specified.  If the 4th argument is true, the internal hit vector is
/// updated to match the truncated solution.  Otherwise, the internal
/// hit vector is left unchanged.  Since the functions getNumHits and
/// getQualifiedTTT uses this internal hit vector, it is generally a
/// good idea to update the hit vector.  On the other hand, one may
/// wish to avoid this update if the hit vector is to be kept for some
/// purpose.
long ibis::query::limit(const char *names, uint32_t keep, bool updateHits) {
    if (keep == 0)
        return -13L;
    if (myDir == 0)
        return -10L;
    long int ierr = 0;

    if (state == UNINITIALIZED || state == SET_COMPONENTS ||
        state == SET_RIDS || state == SET_PREDICATE) {
        ierr = -8;
        return ierr;
    }

    if (state == SPECIFIED || state == QUICK_ESTIMATE) {
        evaluate(true);
    }
    if (state != FULL_EVALUATE && state != BUNDLES_TRUNCATED &&
        state != HITS_TRUNCATED) {
        // failed to evaluate the query
        ierr = -9;
        return ierr;
    }

    ibis::horometer timer;
    if (ibis::gVerbose > 1)
        timer.start();
    ibis::bundle *bdl = ibis::bundle::create(*this);
    if (bdl != 0) {
        const uint32_t oldsize = bdl->size();
        ierr = bdl->truncate(names, keep);
        if (ierr >= 0 && oldsize >= static_cast<long unsigned>(ierr)) {
            if (updateHits) {
                ierr = mypart->evaluateRIDSet(*(bdl->getRIDs()), *hits);
                state = HITS_TRUNCATED;
            }
            else {
                state = BUNDLES_TRUNCATED;
            }
            bdl->write(*this);
        }
        delete bdl;
        if (ibis::gVerbose > 1) {
            timer.stop();
            logMessage("limit", "reordered according to %s using %g sec(CPU), "
                       "%g sec(elapsed), saved %ld bundles", names,
                       timer.CPUTime(), timer.realTime(), ierr);
        }
    }
    else  {
        logWarning("limit", "failed to create bundles");
        ierr = -12;
    }
    return ierr;
} // ibis::query::limit

/// FastBit has a built-in type called ibis::rid_t.  User may use it to
/// provide a global row identifier for each row.  We call such row
/// identifiers (RIDs) the extenal RIDs.  In many cases, there is no
/// external RIDs provided by the user, then there is still a set of
/// implicit RIDs numbered from 0 to nRows()-1.  This function will
/// retrieve the extenal RIDs if they are present, otherwise, it will
/// return the implicit RIDs.
///
/// @note the returned pointer will be null if the query does not have an
/// exact answer yet, has no hit, or is not associated with any data
/// partition.
ibis::RIDSet* ibis::query::getRIDs() const {
    if (mypart == 0 || mypart->nRows() == 0 || hits == 0 || hits->cnt() == 0)
        return 0;
    if (state != FULL_EVALUATE) {
        logWarning("getRIDs", "call evaluate() first");
        return 0;
    }

    readLock lck(this, "getRIDs");
    ibis::RIDSet* rids = readRIDs();
    bool gotRIDs = (rids != 0);
    if (gotRIDs && rids->size() == hits->cnt()) {
        ibis::RIDSet* tmp = rids;
        rids = new ibis::RIDSet;
        rids->deepCopy(*tmp);
        delete tmp;
    }
    else {
        gotRIDs = false;
        delete rids;
        rids = 0;
    }

    if (gotRIDs==false && mypart->explicitRIDs()) {
        // need to get RIDs from the part object
        ibis::part::readLock rock(mypart, "getRIDs");
        if (hits && (dstime == mypart->timestamp() || dstime == 0)) {
            rids = mypart->getRIDs(*hits);
            writeRIDs(rids);
            if (rids->size() != hits->cnt())
                logWarning("getRIDs", "retrieved %lu row IDs, but "
                           "expect %lu",
                           static_cast<long unsigned>(rids->size()),
                           static_cast<long unsigned>(hits->cnt()));
            else if (ibis::gVerbose > 5)
                logMessage("getRIDs", "retrieved %lu row IDs "
                           "(hits->cnt() = %lu)",
                           static_cast<long unsigned>(rids->size()),
                           static_cast<long unsigned>(hits->cnt()));
        }
        else {
            logWarning("getRIDs", "database has changed, "
                       "re-evaluate the query");
        }
    }
    else if (false==gotRIDs) {
        rids = new ibis::RIDSet;
        rids->reserve(hits->cnt());
        for (ibis::bitvector::indexSet is = hits->firstIndexSet();
             is.nIndices() > 0; ++ is) {
            ibis::rid_t tmp;
            const ibis::bitvector::word_t *ii = is.indices();
            if (is.isRange()) {
                for (ibis::bitvector::word_t j = *ii; j < ii[1]; ++j) {
                    tmp.value = j;
                    rids->push_back(tmp);
                }
            }
            else {
                for (unsigned j = 0; j < is.nIndices(); ++ j) {
                    tmp.value = ii[j];
                    rids->push_back(tmp);
                }
            }
        }
    }

    if (ibis::gVerbose > 6 && rids != 0)
        logMessage("getRIDs", "numRIDs = %lu",
                   static_cast<long unsigned>(rids->size()));
    return rids;
} // ibis::query::getRIDs

// During the full estimate, query object is expected to write down the
// bundles and the RIDs of qualified events in each file bundle.  This
// function returns the RID set of the bid'th (first one is zero'th) file
// bundle.
const ibis::RIDSet* ibis::query::getRIDsInBundle(const uint32_t bid) const {
    const ibis::RIDSet *rids = 0;
    if (comps.empty() || hits == 0 || hits->cnt() == 0)
        return rids;
    if (state != ibis::query::FULL_EVALUATE ||
        timestamp() != partition()->timestamp()) {
        logWarning("getRIDsInBundle", "query not fully evaluated or the "
                   "partition has changed since last evaluation.  Need to "
                   "call evaluate again.");
        return rids;
    }

    bool noBundles = true;
    if (myDir != 0) {
        char* name = new char[std::strlen(myDir)+16];
        sprintf(name, "%s%cbundles", myDir, FASTBIT_DIRSEP);
        noBundles = (ibis::util::getFileSize(name) == 0);
        delete [] name;
    }
    if (noBundles) { // attempt to create the bundles if no record of them
        const bool newlock = (dslock == 0);
        if (newlock) {
            dslock = new ibis::part::readLock(partition(), id());
        }
        ibis::bundle* bdtmp = ibis::bundle::create(*this);
        if (newlock) {
            delete dslock;
            dslock = 0;
        }

        if (bdtmp != 0) {
            if (ibis::gVerbose > 3)
                logMessage("getRIDsInBundle",
                           "successfully created file bundles");
            rids = bdtmp->getRIDs(bid);
            bdtmp->write(*this);
            delete bdtmp;
        }
        else {
            logWarning("getRIDsInBundle", "failed to genererate bundle");
        }
    }
    else if (myDir != 0) {
        ibis::query::readLock lck2(this, "getRIDsInBundle");
        rids = ibis::bundle::readRIDs(myDir, bid);
    }
    if (ibis::gVerbose > 3) {
        if (rids != 0)
            logMessage("getRIDsInBundle", "got %lu RID%s for file bundle %lu",
                       static_cast<long unsigned>(rids->size()),
                       (rids->size()>1?"s":""),
                       static_cast<long unsigned>(bid));
        else
            logWarning("getRIDsInBundle", "got no RIDs for file bundle %lu",
                       static_cast<long unsigned>(bid));
    }
    return rids;
} // ibis::query::getRIDsInBundle

/// The data type for row identifiers is ibis::rid_t, which can be treated
/// as unsigned 64-bit integer.  These identifiers (RIDs) can be either
/// provided by the user (external RIDs) or internally generated from row
/// positions (implicit RIDs).  If the user has not provided external RIDs,
/// then this function simply decodes the positions of the bits that are
/// marked 1 and places the positions in the output array.
///
/// The return value can be null if this query object is not associated
/// with a data partition or the mask contains no bit marked 1.
ibis::RIDSet* ibis::query::getRIDs(const ibis::bitvector& mask) const {
    ibis::RIDSet* ridset = 0;
    if (mypart == 0 || mypart->nRows() == 0 || mask.cnt() == 0)
        return ridset;

    ibis::part::readLock tmp(mypart, myID);
    ridset = mypart->getRIDs(mask);
    if (ridset == 0 || ridset->size() != mask.cnt())
        logWarning("getRIDs", "got %lu row IDs from partition %s, expected %lu",
                   static_cast<long unsigned>(ridset != 0 ? ridset->size() : 0),
                   mypart->name(),
                   static_cast<long unsigned>(mask.cnt()));
    else if (ibis::gVerbose > 5)
        logMessage("getRIDs", "retrieved %lu row IDs from partition %s",
                   static_cast<long unsigned>(ridset!=0 ? ridset->size() : 0),
                   mypart->name());
    return ridset;
} // ibis::query::getRIDs

/// An implicit casting will be performed if possible.  A null pointer will
/// be returned if the underlying values can not be safely cast into 32-bit
/// integers.
ibis::array_t<signed char>*
ibis::query::getQualifiedBytes(const char* colname) {
    if (state != FULL_EVALUATE || dstime != mypart->timestamp())
        evaluate();
    ibis::array_t<signed char>* res = 0;
    if (dstime == mypart->timestamp() && hits != 0) {
        readLock lck0(this, "getQualifiedBytes");
        res = mypart->selectBytes(colname, *hits);
        if (ibis::gVerbose > 2)
            logMessage("getQualifiedBytes", "got %lu integer value(s)",
                       static_cast<long unsigned>(res != 0 ? res->size(): 0));
    }
    return res;
} // ibis::query::getQualifiedBytes

/// An implicit casting will be performed if possible.  A null pointer will
/// be returned if the underlying values can not be safely cast into 32-bit
/// unsigned integers.
ibis::array_t<unsigned char>*
ibis::query::getQualifiedUBytes(const char* colname) {
    if (state != FULL_EVALUATE || dstime != mypart->timestamp())
        evaluate();
    ibis::array_t<unsigned char>* res = 0;
    if (dstime == mypart->timestamp() && hits != 0) {
        readLock lck0(this, "getQualifiedUBytes");
        res = mypart->selectUBytes(colname, *hits);
        if (ibis::gVerbose > 2)
            logMessage("getQualifiedUBytes", "got %lu integer value(s)",
                       static_cast<long unsigned>(res != 0 ? res->size() : 0));
    }
    return res;
} // ibis::query::getQualifiedUBytes

/// An implicit casting will be performed if possible.  A null pointer will
/// be returned if the underlying values can not be safely cast into 32-bit
/// integers.
ibis::array_t<int16_t>*
ibis::query::getQualifiedShorts(const char* colname) {
    if (state != FULL_EVALUATE || dstime != mypart->timestamp())
        evaluate();
    ibis::array_t<int16_t>* res = 0;
    if (dstime == mypart->timestamp() && hits != 0) {
        readLock lck0(this, "getQualifiedShorts");
        res = mypart->selectShorts(colname, *hits);
        if (ibis::gVerbose > 2)
            logMessage("getQualifiedShorts", "got %lu integer value(s)",
                       static_cast<long unsigned>(res != 0 ? res->size(): 0));
    }
    return res;
} // ibis::query::getQualifiedShorts

/// An implicit casting will be performed if possible.  A null pointer will
/// be returned if the underlying values can not be safely cast into 32-bit
/// unsigned integers.
ibis::array_t<uint16_t>*
ibis::query::getQualifiedUShorts(const char* colname) {
    if (state != FULL_EVALUATE || dstime != mypart->timestamp())
        evaluate();
    ibis::array_t<uint16_t>* res = 0;
    if (dstime == mypart->timestamp() && hits != 0) {
        readLock lck0(this, "getQualifiedUShorts");
        res = mypart->selectUShorts(colname, *hits);
        if (ibis::gVerbose > 2)
            logMessage("getQualifiedUShorts", "got %lu integer value(s)",
                       static_cast<long unsigned>(res != 0 ? res->size() : 0));
    }
    return res;
} // ibis::query::getQualifiedUShorts

/// An implicit casting will be performed if possible.  A null pointer will
/// be returned if the underlying values can not be safely cast into 32-bit
/// integers.
ibis::array_t<int32_t>*
ibis::query::getQualifiedInts(const char* colname) {
    if (state != FULL_EVALUATE || dstime != mypart->timestamp())
        evaluate();
    ibis::array_t<int32_t>* res = 0;
    if (dstime == mypart->timestamp() && hits != 0) {
        readLock lck0(this, "getQualifiedInts");
        res = mypart->selectInts(colname, *hits);
        if (ibis::gVerbose > 2)
            logMessage("getQualifiedInts", "got %lu integer value(s)",
                       static_cast<long unsigned>(res != 0 ? res->size(): 0));
    }
    return res;
} // ibis::query::getQualifiedInts

/// An implicit casting will be performed if possible.  A null pointer will
/// be returned if the underlying values can not be safely cast into 32-bit
/// unsigned integers.
ibis::array_t<uint32_t>*
ibis::query::getQualifiedUInts(const char* colname) {
    if (state != FULL_EVALUATE || dstime != mypart->timestamp())
        evaluate();
    ibis::array_t<uint32_t>* res = 0;
    if (dstime == mypart->timestamp() && hits != 0) {
        readLock lck0(this, "getQualifiedUInts");
        res = mypart->selectUInts(colname, *hits);
        if (ibis::gVerbose > 2)
            logMessage("getQualifiedUInts", "got %lu integer value(s)",
                       static_cast<long unsigned>(res != 0 ? res->size() : 0));
    }
    return res;
} // ibis::query::getQualifiedUInts

/// An implicit casting will be performed if possible.  A null pointer will
/// be returned if the underlying values can not be safely cast into 32-bit
/// integers.
ibis::array_t<int64_t>*
ibis::query::getQualifiedLongs(const char* colname) {
    if (state != FULL_EVALUATE || dstime != mypart->timestamp())
        evaluate();
    ibis::array_t<int64_t>* res = 0;
    if (dstime == mypart->timestamp() && hits != 0) {
        readLock lck0(this, "getQualifiedLongs");
        res = mypart->selectLongs(colname, *hits);
        if (ibis::gVerbose > 2)
            logMessage("getQualifiedLongs", "got %lu integer value(s)",
                       static_cast<long unsigned>(res != 0 ? res->size(): 0));
    }
    return res;
} // ibis::query::getQualifiedLongs

/// An implicit casting will be performed if possible.  A null pointer will
/// be returned if the underlying values can not be safely cast into 32-bit
/// unsigned integers.
ibis::array_t<uint64_t>* ibis::query::getQualifiedULongs(const char* colname) {
    if (state != FULL_EVALUATE || dstime != mypart->timestamp())
        evaluate();
    ibis::array_t<uint64_t>* res = 0;
    if (dstime == mypart->timestamp() && hits != 0) {
        readLock lck0(this, "getQualifiedULongs");
        res = mypart->selectULongs(colname, *hits);
        if (ibis::gVerbose > 2)
            logMessage("getQualifiedULongs", "got %lu integer value(s)",
                       static_cast<long unsigned>(res != 0 ? res->size() : 0));
    }
    return res;
} // ibis::query::getQualifiedULongs

/// An implicit casting will be performed if possible.  A null pointer will
/// be returned if the underlying values can not be safely cast into 32-bit
/// floating-point values.
ibis::array_t<float>*
ibis::query::getQualifiedFloats(const char* colname) {
    if (state != FULL_EVALUATE || dstime != mypart->timestamp())
        evaluate();
    ibis::array_t<float>* res = 0;
    if (dstime == mypart->timestamp() && hits != 0) {
        const bool newlock = (dslock == 0);
        if (newlock) {
            dslock = new ibis::part::readLock(mypart, myID);
        }
        readLock lck(this, "getQualifiedFloats");
        res = mypart->selectFloats(colname, *hits);

        if (newlock) {
            delete dslock;
            dslock = 0;
        }
        if (ibis::gVerbose > 2)
            logMessage("getQualifiedFloats", "got %lu float value(s)",
                       static_cast<long unsigned>(res!=0 ? res->size() : 0));
    }
    return res;
} // ibis::query::getQualifiedFloats

/// An implicit casting will be performed if the specified column is not of
/// type double.  Note that casting from 64-bit integers to double may
/// cause loss of precision; casting of 32-bit floating-point values to
/// 64-bit version may lead to spurious precision.
ibis::array_t<double>* ibis::query::getQualifiedDoubles(const char* colname) {
    if (state != FULL_EVALUATE || dstime != mypart->timestamp())
        evaluate();
    ibis::array_t<double>* res = 0;
    if (dstime == mypart->timestamp() && hits != 0) {
        const bool newlock = (dslock == 0);
        if (newlock) {
            dslock = new ibis::part::readLock(mypart, myID);
        }
        readLock lck(this, "getQualifiedDoubles");
        res = mypart->selectDoubles(colname, *hits);

        if (newlock) {
            delete dslock;
            dslock = 0;
        }
        if (ibis::gVerbose > 2)
            logMessage("getQualifiedDoubles", "got %lu double value(s)",
                       static_cast<long unsigned>(res!=0 ? res->size() : 0));
    }
    return res;
} // ibis::query::getQualifiedDoubles

/// The argument @c colname must be the name of a string-valued column,
/// otherwise a null pointer will be returned.
///
/// @note FastBit does not track the memory usage of neither std::vector
/// nor std::string.
std::vector<std::string>*
ibis::query::getQualifiedStrings(const char* colname) {
    if (state != FULL_EVALUATE || dstime != mypart->timestamp())
        evaluate();
    std::vector<std::string>* res = 0;
    if (dstime == mypart->timestamp() && hits != 0) {
        const bool newlock = (dslock == 0);
        if (newlock) {
            dslock = new ibis::part::readLock(mypart, myID);
        }
        readLock lck(this, "getQualifiedStrings");
        res = mypart->selectStrings(colname, *hits);

        if (newlock) {
            delete dslock;
            dslock = 0;
        }
        if (ibis::gVerbose > 2)
            logMessage("getQualifiedStrings", "got %lu double value(s)",
                       static_cast<long unsigned>(res!=0 ? res->size() : 0));
    }
    return res;
} // ibis::query::getQualifiedStrings

ibis::query::QUERY_STATE ibis::query::getState() const {
    if (ibis::gVerbose > 6) {
        switch (state) {
        case UNINITIALIZED:
            logMessage("getState", "UNINITIALIZED"); break;
        case SET_RIDS:
            logMessage("getState", "SET_RIDS"); break;
        case SET_COMPONENTS:
            logMessage("getState", "SET_COMPONENTS"); break;
        case SET_PREDICATE:
            logMessage("getState", "SET_PREDICATE"); break;
        case SPECIFIED:
            logMessage("getState", "SPECIFIED"); break;
        case QUICK_ESTIMATE:
            logMessage("getState", "QUICK_ESTIMATE"); break;
        case FULL_EVALUATE:
            logMessage("getState", "FULL_EVALUATE"); break;
        default:
            logMessage("getState", "UNKNOWN");
        }
    }
    return state;
} // ibis::query::getState

/// Expands where clause to preferred bounds.  This is to make sure the
/// function estimate will give exact answer.  It does nothing if there is
/// no preferred bounds in the indices.
void ibis::query::expandQuery() {
    if (conds.empty()) // no predicate clause specified
        return;

    writeLock lck(this, "expandQuery");
    if (dslock == 0) {
        dslock = new ibis::part::readLock(mypart, myID);
    }
    doExpand(conds.getExpr()); // do the actual work

    // rewrite the query expression string
    conds.resetString();

    // update the state of this query
    if (state == FULL_EVALUATE || state == BUNDLES_TRUNCATED ||
        state == HITS_TRUNCATED || state == QUICK_ESTIMATE) {
        if (hits == sup) {
            delete hits; hits = 0; sup = 0;
        }
        else {
            delete hits; hits = 0;
            delete sup; sup = 0;
        }
        state = SPECIFIED;
        removeFiles();
        dstime = 0;
    }
    else if (! comps.empty()) {
        state = SPECIFIED;
        writeQuery();
    }
} // ibis::query::expandQuery

/// Contracts where clause to preferred bounds. Similar to function
/// exandQuery, but makes the bounds of the range conditions narrower
/// rather than wider.
void ibis::query::contractQuery() {
    if (conds.empty()) // no predicate clause specified
        return;

    writeLock lck(this, "contractQuery");
    if (dslock == 0) {
        dslock = new ibis::part::readLock(mypart, myID);
    }
    doContract(conds.getExpr()); // do the actual work

    // rewrite the query expression string
    conds.resetString();

    // update the state of this query
    if (state == FULL_EVALUATE || state == BUNDLES_TRUNCATED ||
        state == HITS_TRUNCATED || state == QUICK_ESTIMATE) {
        if (hits == sup) {
            delete hits; hits = 0; sup = 0;
        }
        else {
            delete hits; hits = 0;
            delete sup; sup = 0;
        }
        state = SPECIFIED;
        removeFiles();
        dstime = 0;
    }
    else if (! comps.empty()) {
        state = SPECIFIED;
        writeQuery();
    }
} // ibis::query::contractQuery

/// Separate out the sub-expressions that are not simple.  This is
/// intended to allow the overall where clause to be evaluated in
/// separated steps, where the simple conditions are left for this
/// software to handle and the more complex ones are to be handled by
/// another software.  The set of conditions remain with this query
/// object and the conditions returned by this function are assumed to
/// be connected with the operator AND.  If the top-most operator in
/// the WHERE clause is not an AND operator, the whole clause will be
/// returned if it contains any conditions that is not simple,
/// otherwise, an empty string will be returned.
std::string ibis::query::removeComplexConditions() {
    std::string ret;
    if (conds.empty()) return ret;

    ibis::qExpr *simple, *tail;
    int ierr = conds->separateSimple(simple, tail);
    if (ierr == 0) { // a mixture of complex and simple conditions
        QUERY_STATE old = state;
        std::ostringstream oss0, oss1;
        simple->print(oss0);
        tail->print(oss1);
        LOGGER(ibis::gVerbose > 2)
            << "query::removeComplexConditions split \""
            << (conds.getString() ? conds.getString() : "<long expression>")
            << "\" into \"" << *simple << "\" ("
            << oss0.str() << ") AND \"" << *tail << "\" ("
            << oss1.str() << ")";

        delete simple;
        delete tail;
        ret = oss1.str();
        setWhereClause(oss0.str().c_str());
        if (old == QUICK_ESTIMATE)
            estimate();
        else if (old == FULL_EVALUATE)
            evaluate();
    }
    else if (ierr < 0) { // only complex conditions
        if (ibis::gVerbose > 2)
            logMessage("removeComplexConditions", "the whole WHERE clause "
                       "is considered complex, no simple conjunctive "
                       "range conditions can be separated out");
        ret = conds.getString();
        conds.clear();
        if (rids_in == 0) {
            if (sup != 0 && sup != hits) {
                delete sup;
                sup = 0;
            }
            if (hits == 0)
                hits = new ibis::bitvector;
            hits->set(1, mypart->nRows());
            state = FULL_EVALUATE;
        }
        else if (state == FULL_EVALUATE || state == BUNDLES_TRUNCATED ||
                 state == HITS_TRUNCATED || state == QUICK_ESTIMATE) {
            getBounds();
        }
    }
    // ierr > 0 indicates that there are only simple conditions, do nothing
    return ret;
} // ibis::query::removeComplexConditions

////////////////////////////////////////////////////////////////////////////
//      The following functions are used internally by the query class
////////////////////////////////////////////////////////////////////////////
/// Constructor.  Generates a new query on the given data partition.
///
/// @arg uid the user name to be associated with the query object.  If this
///      arguemnt is a nil pointer, this function will call
///      ibis::util::userName to determine the current user name.
///
/// @arg et the data partition to be used for queried.  The data partition
///      to be queried could be altered with the function setPartition.
///
/// @arg pref a special prefix to be used for this query.  This prefix is
///      used primarily to identify the query object and to retrieve
///      configuration parameters that are intended for a special class of
///      queries.  For example, if recovery is desired, the user can define
///      pref.enableRecovery = true.
///      When this feature is enable, it is possible to also define
///      pref.purgeTempFiles = true
///      to tell the destructor of this class to remove the log file about
///      a query.  The default value if pref is a nil pointer, which
///      disables the logging feature.
ibis::query::query(const char* uid, const part* et, const char* pref) :
    user(ibis::util::strnewdup((uid && *uid) ? uid : ibis::util::userName())),
    state(UNINITIALIZED), hits(0), sup(0), dslock(0), myID(0),
    myDir(0), rids_in(0), mypart(et), dstime(0) {
    myID = newToken(user);
    lastError[0] = static_cast<char>(0);

    if (pthread_rwlock_init(&lock, 0) != 0) {
        strcpy(lastError, "pthread_rwlock_init() failed in "
               "query::query()");
        LOGGER(ibis::gVerbose >= 0) << "Warning -- " << lastError;
        throw ibis::util::strnewdup(lastError);
    }

    std::string name;
    if (pref) {
        name = pref;
        name += ".enableRecovery";
    }
    else {
        name = "query.enableRecovery";
    }
    if (pref != 0 || ibis::gParameters().isTrue(name.c_str())) {
        setMyDir(pref);
    }
    LOGGER(ibis::gVerbose > 4)
        << "query " << myID << " constructed for " << user;
} // constructor for new query

/// Constructor.  Reconstructs query from stored information in the named
/// directory @c dir.  This is only used for recovering from program
/// crashes, not intended for user to manually construct a query in a
/// directory.
///
/// @note that to enable recovery, the query objects must be constructed
/// with the recovery feature, which is enabled through a configuration
/// parameter prefix.enableRecovery = true.
ibis::query::query(const char* dir, const ibis::partList& tl) :
    user(0), state(UNINITIALIZED), hits(0), sup(0), dslock(0), myID(0),
    myDir(0), rids_in(0), mypart(0), dstime(0) {
    const char *ptr = strrchr(dir, FASTBIT_DIRSEP);
    if (ptr == 0) {
        myID = ibis::util::strnewdup(dir);
        myDir = new char[std::strlen(dir)+2];
        strcpy(myDir, dir);
    }
    else if (ptr[1] == static_cast<char>(0)) {
        // dir name ends with FASTBIT_DIRSEP
        myDir = ibis::util::strnewdup(dir);
        myDir[ptr-dir] = static_cast<char>(0);
        ptr = strrchr(myDir, FASTBIT_DIRSEP);
        if (ptr != 0) {
            myID = ibis::util::strnewdup(ptr+1);
        }
        else {
            myID = ibis::util::strnewdup(myDir);
        }
    }
    else { 
        myID = ibis::util::strnewdup(ptr+1);
        myDir = new char[std::strlen(dir)+2];
        strcpy(myDir, dir);
    }
    uint32_t j = std::strlen(myDir);
    myDir[j] = FASTBIT_DIRSEP;
    ++j;
    myDir[j] = static_cast<char>(0);

    readQuery(tl); // the directory must contain as least a query file
    if (state == QUICK_ESTIMATE) {
        state = SPECIFIED;
    }
    else if (state == FULL_EVALUATE) {
        try { // read the hit vector
            readHits();
            state = FULL_EVALUATE;
        }
        catch (...) { // failed to read the hit vector
            if (hits != 0) delete hits;
            hits = 0;
            sup = 0;
            if (!comps.empty() && (conds.getExpr() != 0 || rids_in != 0))
                state = SPECIFIED;
            else if (! comps.empty())
                state = SET_COMPONENTS;
            else if (conds.getExpr() != 0)
                state = SET_PREDICATE;
            else if (rids_in)
                state = SET_RIDS;
            else
                state = UNINITIALIZED;
        }
    }
    LOGGER(ibis::gVerbose > 4)
        << "query " << myID << " read from " << dir;
} // constructor from stored files

/// Desctructor
ibis::query::~query() {
    clear();
    delete [] myDir;
    delete [] myID;
    delete [] user;
    pthread_rwlock_destroy(&lock);
}

/// To generate a new query token.  A token contains 16 bytes.  These bytes
/// are a base-64 representation of three integers computed from (A) the
/// Fletcher chechsum of the user id, (B) the current time stamp reported
/// by the function @c time, and (C) a monotonically increasing counter
/// provided by the function ibis::util::serialNumber.
char* ibis::query::newToken(const char *uid) {
    uint32_t ta, tb, tc;
    char* name = new char[ibis::query::tokenLength()+1];
    name[ibis::query::tokenLength()] = 0;

    // compute the three components of the token
    if (uid != 0 && *uid != 0)
        // a checksum of the user name
        ta = ibis::util::checksum(uid, std::strlen(uid));
    else
        ta = 0;
#if (_XOPEN_SOURCE - 0) >= 500
    static uint32_t myhostid(gethostid());      // hostid from unistd.h
    ta ^= myhostid;
#endif
    {
        time_t tmp;
        time(&tmp);                     // the current time
        tb = static_cast<uint32_t>(tmp);
    }
    tc = ibis::util::serialNumber();    // a counter

    if (ibis::gVerbose > 6)
        ibis::util::logMessage("newToken", "constructing token from "
                               "uid %s (%lu), time %lu, sequence "
                               "number %lu", uid,
                               static_cast<long unsigned>(ta),
                               static_cast<long unsigned>(tb),
                               static_cast<long unsigned>(tc));

    // write out the three integers as 16 printable characters
    name[15] = ibis::util::charTable[63 & tc]; tc >>= 6;
    name[14] = ibis::util::charTable[63 & tc]; tc >>= 6;
    name[13] = ibis::util::charTable[63 & tc]; tc >>= 6;
    name[12] = ibis::util::charTable[63 & tc]; tc >>= 6;
    name[11] = ibis::util::charTable[63 & tc]; tc >>= 6;
    name[10] = ibis::util::charTable[63 & (tc | (tb<<2))]; tb >>= 4;
    name[9]  = ibis::util::charTable[63 & tb]; tb >>= 6;
    name[8]  = ibis::util::charTable[63 & tb]; tb >>= 6;
    name[7]  = ibis::util::charTable[63 & tb]; tb >>= 6;
    name[6]  = ibis::util::charTable[63 & tb]; tb >>= 6;
    name[5]  = ibis::util::charTable[63 & (tb | (ta<<4))]; ta >>= 2;
    name[4]  = ibis::util::charTable[63 & ta]; ta >>= 6;
    name[3]  = ibis::util::charTable[63 & ta]; ta >>= 6;
    name[2]  = ibis::util::charTable[63 & ta]; ta >>= 6;
    name[1]  = ibis::util::charTable[63 & ta]; ta >>= 6;
    // ensure the first byte is an alphabet
    if (ta > 9 && ta < 62) {
        name[0]  = ibis::util::charTable[ta];
    }
    else {
        // attempt to use the first alphabet of uid
        const char *tmp = uid;
        if (uid != 0 && *uid != 0)
            while (*tmp && !isalpha(*tmp)) ++ tmp;
        if (tmp != 0 && *tmp != 0) { // found an alphabet
            name[0] = *tmp;
        }
        else if (ta <= 9) {
            name[0] = ibis::util::charTable[ta*5+10];
        }
        else {
            ta -= 62;
            ta &= 31; // possible values [0:31]
            name[0] = ibis::util::charTable[ta+10];
        }
    }
    if (ibis::gVerbose > 3)
        ibis::util::logMessage("newToken", "generated new token \"%s\" "
                               "for user %s", name, uid);
    return name;
} // ibis::query::newToken

// is the given string a valid query token
// -- must have 16 characters
// -- must be all in charTable
bool ibis::query::isValidToken(const char* tok) {
    bool ret = (std::strlen(tok) == ibis::query::tokenLength());
    if (! ret) // if string length not 16, it can not be a valid token
        return ret;
    // necessary to prevent overstepping the bouds of array
    // ibis::util::charIndex
    ret = (tok[0] < 127) && (tok[1] < 127) && (tok[2] < 127) &&
        (tok[3] < 127) && (tok[4] < 127) && (tok[5] < 127) &&
        (tok[6] < 127) && (tok[7] < 127) && (tok[8] < 127) &&
        (tok[9] < 127) && (tok[10] < 127) && (tok[11] < 127) &&
        (tok[12] < 127) && (tok[13] < 127) && (tok[14] < 127) &&
        (tok[15] < 127);
    if (! ret)
        return ret;

    // convert 16 character to 3 integers
    uint32_t ta, tb, tc, tmp;
    tmp = ibis::util::charIndex[static_cast<unsigned>(tok[0])];
    if (tmp < 64) {
        ta = (tmp << 26);
    }
    else {
        ret = false;
        return ret;
    }
    tmp = ibis::util::charIndex[static_cast<unsigned>(tok[1])];
    if (tmp < 64) {
        ta |= (tmp << 20);
    }
    else {
        ret = false;
        return ret;
    }
    tmp = ibis::util::charIndex[static_cast<unsigned>(tok[2])];
    if (tmp < 64) {
        ta |= (tmp << 14);
    }
    else {
        ret = false;
        return ret;
    }
    tmp = ibis::util::charIndex[static_cast<unsigned>(tok[3])];
    if (tmp < 64) {
        ta |= (tmp << 8);
    }
    else {
        ret = false;
        return ret;
    }
    tmp = ibis::util::charIndex[static_cast<unsigned>(tok[4])];
    if (tmp < 64) {
        ta |= (tmp << 2);
    }
    else {
        ret = false;
        return ret;
    }
    tmp = ibis::util::charIndex[static_cast<unsigned>(tok[5])];
    if (tmp < 64) {
        ta |= (tmp >> 4);
        tb = (tmp << 28);
    }
    else {
        ret = false;
        return ret;
    }
    tmp = ibis::util::charIndex[static_cast<unsigned>(tok[6])];
    if (tmp < 64) {
        tb |= (tmp << 22);
    }
    else {
        ret = false;
        return ret;
    }
    tmp = ibis::util::charIndex[static_cast<unsigned>(tok[7])];
    if (tmp < 64) {
        tb |= (tmp << 16);
    }
    else {
        ret = false;
        return ret;
    }
    tmp = ibis::util::charIndex[static_cast<unsigned>(tok[8])];
    if (tmp < 64) {
        tb |= (tmp << 10);
    }
    else {
        ret = false;
        return ret;
    }
    tmp = ibis::util::charIndex[static_cast<unsigned>(tok[9])];
    if (tmp < 64) {
        tb |= (tmp << 4);
    }
    else {
        ret = false;
        return ret;
    }
    tmp = ibis::util::charIndex[static_cast<unsigned>(tok[10])];
    if (tmp < 64) {
        tb |= (tmp >> 2);
        tc = (tmp << 30);
    }
    else {
        ret = false;
        return ret;
    }
    tmp = ibis::util::charIndex[static_cast<unsigned>(tok[11])];
    if (tmp < 64) {
        tc |= (tmp << 24);
    }
    else {
        ret = false;
        return ret;
    }
    tmp = ibis::util::charIndex[static_cast<unsigned>(tok[12])];
    if (tmp < 64) {
        tc |= (tmp << 18);
    }
    else {
        ret = false;
        return ret;
    }
    tmp = ibis::util::charIndex[static_cast<unsigned>(tok[13])];
    if (tmp < 64) {
        tc |= (tmp << 12);
    }
    else {
        ret = false;
        return ret;
    }
    tmp = ibis::util::charIndex[static_cast<unsigned>(tok[14])];
    if (tmp < 64) {
        tc |= (tmp << 6);
    }
    else {
        ret = false;
        return ret;
    }
    tmp = ibis::util::charIndex[static_cast<unsigned>(tok[15])];
    if (tmp < 64) {
        tc |= tmp;
    }
    else {
        ret = false;
        return ret;
    }

    if (ibis::gVerbose > 8)
        ibis::util::logMessage("isValidToken", "convert token %s to three "
                               "integers %lu, %lu, %lu.",
                               tok, static_cast<long unsigned>(ta),
                               static_cast<long unsigned>(tb),
                               static_cast<long unsigned>(tc));

    long unsigned tm; // current time in seconds
    (void) time((time_t*)&tm);
    ret = (tm >= tb); // must be created in the past

    return ret;
} // ibis::query::isValidToken

/// To determine an directory for storing information about the query, such
/// as the where clause, the hits and so on.  It can also be used to
/// recover from a crash.
void ibis::query::setMyDir(const char *pref) {
    if (myDir != 0) return; // do not over write existing value

    const char* cacheDir = 0;
    if (pref == 0 || *pref == 0) {
        cacheDir = ibis::gParameters()["CacheDirectory"];
        if (cacheDir == 0)
            cacheDir = ibis::gParameters()["CacheDir"];
        if (cacheDir == 0)
            cacheDir = ibis::gParameters()["query.CacheDirectory"];
        if (cacheDir == 0)
            cacheDir = ibis::gParameters()["query.CacheDir"];
        if (cacheDir == 0)
            cacheDir = ibis::gParameters()["query.dataDir3"];
        if (cacheDir == 0)
            cacheDir = ibis::gParameters()["ibis.query.CacheDirectory"];
        if (cacheDir == 0)
            cacheDir = ibis::gParameters()["ibis.query.CacheDir"];
        if (cacheDir == 0)
            cacheDir = ibis::gParameters()["ibis.query.dataDir3"];
        if (cacheDir == 0)
            cacheDir = ibis::gParameters()
                ["GCA.coordinator.cacheDirectory"];
        if (cacheDir == 0)
            cacheDir = ibis::gParameters()["GCA.coordinator.cacheDir"];
    }
    else {
        std::string name = pref;
        name += ".cacheDirectory";
        cacheDir = ibis::gParameters()[name.c_str()];
        if (cacheDir == 0) {
            name = pref;
            name += ".cacheDir";
            cacheDir = ibis::gParameters()[name.c_str()];
        }
        if (cacheDir == 0) {
            name = pref;
            name += ".dataDir3";
            cacheDir = ibis::gParameters()[name.c_str()];
        }
        if (cacheDir == 0) {
            name = pref;
            name += ".query.cacheDirectory";
            cacheDir = ibis::gParameters()[name.c_str()];
        }
        if (cacheDir == 0) {
            name = pref;
            name += ".query.cacheDir";
            cacheDir = ibis::gParameters()[name.c_str()];
        }
        if (cacheDir == 0) {
            name = pref;
            name += ".query.dataDir3";
            cacheDir = ibis::gParameters()[name.c_str()];
        }
        if (cacheDir == 0) {
            name = "ibis.";
            name += pref;
            name += ".query.cacheDirectory";
            cacheDir = ibis::gParameters()[name.c_str()];
        }
        if (cacheDir == 0) {
            name = "ibis.";
            name += pref;
            name += ".query.cacheDir";
            cacheDir = ibis::gParameters()[name.c_str()];
        }
        if (cacheDir == 0) {
            name = "ibis.";
            name += pref;
            name += ".query.dataDir3";
            cacheDir = ibis::gParameters()[name.c_str()];
        }
        if (cacheDir == 0) {
            name = "GCA.";
            name += pref;
            name += ".coordinator.cacheDirectory";
            cacheDir = ibis::gParameters()[name.c_str()];
        }
        if (cacheDir == 0) {
            name = "GCA.";
            name += pref;
            name += ".coordinator.cacheDir";
            cacheDir = ibis::gParameters()[name.c_str()];
        }
    }
#if defined(unix)
    if (cacheDir == 0) {
        cacheDir = getenv("TMPDIR");
    }
#endif

    if (cacheDir) {
        if (std::strlen(cacheDir)+std::strlen(myID)+10<PATH_MAX) {
            myDir = new char[std::strlen(cacheDir)+std::strlen(myID)+3];
            sprintf(myDir, "%s%c%s", cacheDir, FASTBIT_DIRSEP, myID);
        }
        else {
            LOGGER(ibis::gVerbose >= 0)
                << "Warning -- CacheDirectory(\"" << cacheDir
                << "\") too long";
            throw "path for CacheDirectory is too long";
        }
    }
    else {
        myDir = new char[10+std::strlen(myID)];
        sprintf(myDir, ".ibis%c%s", FASTBIT_DIRSEP, myID);
    }
    uint32_t j = std::strlen(myDir);
    myDir[j] = FASTBIT_DIRSEP;
    myDir[j+1] = static_cast<char>(0);
    ibis::util::makeDir(myDir);
} /// ibis::query::setMyDir

/// This function prints a list of RIDs to the log file.
void ibis::query::printRIDs(const ibis::RIDSet& ridset) const {
    if (ibis::gVerbose < 0) return;

    int len = ridset.size();
    ibis::util::logger lg;
    ibis::RIDSet::const_iterator it = ridset.begin();
    lg() << "RID set length = " << len << std::endl;
    for (int i=0; i<len; ++i, ++it) {
        lg() << " [ " << (*it).num.run << ", " << (*it).num.event << " ] ";
        if (3 == i%4)
            lg() << std::endl;
    }
    if (len>0 && len%4!=0)
        lg() << std::endl;
} // ibis::query::printRIDs

/// Store the message into member variable lastError for later use.  This
/// function will truncate long messages because lastError is declared with
/// a fixed size of MAX_LINE+PATH_MAX.  If the incoming argument is a nil
/// pointer or an empty sttring, lastError will be set to be an empty
/// string as if clearErrorMessage is called.
void ibis::query::storeErrorMesg(const char *msg) const {
    if (msg == 0 || *msg == 0) {
        *lastError = 0;
    }
    else {
        size_t len = strlen(msg);
        if (len < MAX_LINE+PATH_MAX) {
            (void) strcpy(lastError, msg);
        }
        else {
            (void) strncpy(lastError, msg, MAX_LINE+PATH_MAX-1);
            lastError[MAX_LINE+PATH_MAX-1] = 0;
        }
    }
} // ibis::query::storeErrorMesg

// three error logging functions
void ibis::query::logError(const char* event, const char* fmt, ...) const {
    strcpy(lastError, "ERROR: ");

#if (defined(HAVE_VPRINTF) || defined(_WIN32)) && ! defined(DISABLE_VPRINTF)
    char* s = new char[std::strlen(fmt)+MAX_LINE];
    if (s != 0) {
        va_list args;
        va_start(args, fmt);
        vsprintf(s, fmt, args);
        va_end(args);

        (void) strncpy(lastError+7, s, MAX_LINE-7);
        {
            ibis::util::logger lg;
            lg() << " Error *** query[" << myID << "]::" << event << " -- "
                 << s;
            if (errno != 0)
                lg() << " ... " << strerror(errno);
        }
        throw s;
    }
    else {
#endif
        (void) strncpy(lastError+7, fmt, MAX_LINE-7);
        {
            ibis::util::logger lg;
            lg() << " Error *** query[" << myID << "]::" << event
                 << " -- " << fmt << " ...";
            if (errno != 0)
                lg() << " ... " << strerror(errno);
        }
        throw fmt;
#if (defined(HAVE_VPRINTF) || defined(_WIN32)) && ! defined(DISABLE_VPRINTF)
    }
#endif
} // ibis::query::logError

void ibis::query::logWarning(const char* event, const char* fmt, ...) const {
#if (defined(HAVE_VPRINTF) || defined(_WIN32)) && ! defined(DISABLE_VPRINTF)
    if (strnicmp(lastError, "ERROR", 5) != 0) {
        // last message was not an error, record this warning message
        strcpy(lastError, "Warning: ");
        va_list args;
        va_start(args, fmt);
        vsprintf(lastError+9, fmt, args);
        va_end(args);

        ibis::util::logger lg;
        lg() << "Warning -- query[" << myID << "]::" << event << " -- "
             << lastError+9;
        if (errno != 0) {
            if (errno != ENOENT)
                lg() << " ... " << strerror(errno);
            errno = 0;
        }
    }
    else {
        char* s = new char[std::strlen(fmt)+MAX_LINE];
        if (s != 0) {
            va_list args;
            va_start(args, fmt);
            vsprintf(s, fmt, args);
            va_end(args);

            ibis::util::logger lg;
            lg() << "Warning -- query[" << myID << "]::" << event
                 << " -- " << s;
            if (errno != 0) {
                if (errno != ENOENT)
                    lg() << " ... " << strerror(errno);
                errno = 0;
            }
            delete [] s;
        }
        else {
            FILE* fptr = ibis::util::getLogFile();
            ibis::util::ioLock lock;
            fprintf(fptr, "Warning -- query[%s]::%s -- ", myID, event);
            va_list args;
            va_start(args, fmt);
            vfprintf(fptr, fmt, args);
            va_end(args);
            fprintf(fptr, "\n");
            fflush(fptr);
        }
    }
#else
    if (strnicmp(lastError, "ERROR", 5) != 0) {
        UnixSnprintf(lastError, MAX_LINE+PATH_MAX, "Warning: %s", fmt);
    }

    ibis::util::logger lg;
    lg() << "Warning -- query[" << myID << "]::" << event
         << " -- " << fmt << " ...";
    if (errno != 0) {
        if (errno != ENOENT)
            lg() << " ... " << strerror(errno);
        errno = 0;
    }
#endif
} // ibis::query::logWarning

/// Used to print information about the progress or state of query
/// processing.  It prefixes each message with a query token.
void ibis::query::logMessage(const char* event, const char* fmt, ...) const {
    FILE *fptr = ibis::util::getLogFile();
    ibis::util::ioLock lck;
#if defined(FASTBIT_TIMED_LOG)
    char tstr[28];
    ibis::util::getLocalTime(tstr);
    fprintf(fptr, "%s   ", tstr);
#endif
    fprintf(fptr, "query[%s]::%s -- ", myID, event);
#if (defined(HAVE_VPRINTF) || defined(_WIN32)) && ! defined(DISABLE_VPRINTF)
    va_list args;
    va_start(args, fmt);
    vfprintf(fptr, fmt, args);
    va_end(args);
#else
    fprintf(fptr, "%s ...", fmt);
#endif
    fprintf(fptr, "\n");
    fflush(fptr);
} // ibis::query::logMessage

bool ibis::query::hasBundles() const {
    char ridfile[PATH_MAX];
    char bdlfile[PATH_MAX];
    strcpy(ridfile, dir());
    strcpy(bdlfile, dir());
    strcat(ridfile, "-rids");
    strcat(bdlfile, "bundles");
    if (ibis::util::getFileSize(ridfile) > 0 &&
        ibis::util::getFileSize(bdlfile) > 0) {
        return true;
    }
    else {
        return false;
    }
} // ibis::query::hasBundles

// reorder the query expression to minimize the work of evaluation
// (assuming the query expression is evaluated from left to right)
void ibis::query::reorderExpr() {
    ibis::query::weight wt(mypart);

    // call qExpr::reorder to do the actual work
    double ret = conds->reorder(wt);
    if (ibis::gVerbose > 5) {
        ibis::util::logger lg;
        lg() << "query[" << myID << "]:reorderExpr returns " << ret
             << ".  The new query expression is \n";
        conds.getExpr()->printFull(lg());
    }
} // ibis::query::reorderExpr

/// Compute the upper and lower bounds for range queries.
void ibis::query::getBounds() {
    if (ibis::gVerbose > 7)
        logMessage("getBounds", "compute upper and lower bounds of hits");

    ibis::bitvector mask;
    conds.getNullMask(*mypart, mask);
    if (! comps.empty()) {
        ibis::bitvector tmp;
        comps.getNullMask(*mypart, tmp);
        if (mask.size() > 0)
            mask &= tmp;
        else
            mask.swap(tmp);
    }

    if (rids_in != 0) { // RID list
        ibis::bitvector tmp;
        mypart->evaluateRIDSet(*rids_in, tmp);
        mask &= tmp;
    }

    if (conds.getExpr() != 0) { // range condition
        sup = new ibis::bitvector;
        hits = new ibis::bitvector;
        doEstimate(conds.getExpr(), *hits, *sup);
        if (sup->size() == hits->size() && sup->size() < mypart->nRows())
            sup->adjustSize(mypart->nRows(), mypart->nRows());
        if (hits->size() != mypart->nRows()) {
            logWarning("getBounds", "hits.size(%lu) differ from expected "
                       "value(%lu)", static_cast<long unsigned>(hits->size()),
                       static_cast<long unsigned>(mypart->nRows()));
            hits->setBit(mypart->nRows()-1, 0);
        }
        *hits &= mask;
        hits->compress();

        if (sup->size() == hits->size()) {
            *sup &= mask;
            sup->compress();
            if (ibis::gVerbose > 3)
                logMessage("getBounds", "number of hits in [%lu, %lu]",
                           static_cast<long unsigned>(hits->cnt()),
                           static_cast<long unsigned>(sup->cnt()));
        }
        else {
            delete sup;
            sup = hits;
        }
    }
    else { // everything is a hit
        hits = new ibis::bitvector(mask);
        sup = hits;
    }
} // ibis::query::getBounds

/// Use index only to come up with a upper bound and a lower bound.  Treats
/// nil term as matching every row to allow empty where clauses to be
/// interpreted as matching everything (to conform to SQL standard).
void ibis::query::doEstimate(const ibis::qExpr* term, ibis::bitvector& low,
                             ibis::bitvector& high) const {
    if (term == 0) {
        high.set(1, mypart->nRows());
        low.set(1, mypart->nRows());
        return;
    }
    LOGGER(ibis::gVerbose > 7)
        << "query[" << myID << "]::doEstimate -- starting to estimate "
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
    case ibis::qExpr::ANYANY:
        mypart->estimateMatchAny
            (*(reinterpret_cast<const ibis::qAnyAny*>(term)), low, high);
        break;
    case ibis::qExpr::COMPRANGE: {
        const ibis::compRange &cr =
            *(reinterpret_cast<const ibis::compRange*>(term));
        if (cr.isConstant()) {
            const bool tf = cr.inRange();
            high.set(tf, mypart->nRows());
            low.set(tf, mypart->nRows());
        }
        else {// can not estimate complex range condition yet
            high.set(1, mypart->nRows());
            low.set(0, mypart->nRows());
        }
        break;}
    default:
        if (term->isConstant() && term->getType() == ibis::qExpr::MATHTERM) {
            const ibis::math::term &mt =
                *(reinterpret_cast<const ibis::math::term*>(term));
            const bool tf = mt.isTrue();
            high.set(tf, mypart->nRows());
            low.set(tf, mypart->nRows());
        }
        else {
            if (ibis::gVerbose > 2)
                logMessage("doEstimate", "failed to estimate query term of "
                           "unknown type, presume every row is a possible hit");
            high.set(1, mypart->nRows());
            low.set(0, mypart->nRows());
        }
    }
#if defined(DEBUG) || defined(_DEBUG)
    LOGGER(ibis::gVerbose > 4)
        << "query[" << myID << "]::doEstimate("
        << static_cast<const void*>(term) << ": " << *term
        << ") --> [" << low.cnt() << ", " << high.cnt() << "]";
#if DEBUG + 0 > 1 || _DEBUG + 0 > 1
    LOGGER(ibis::gVerbose > 5) << "low \n" << low
                           << "\nhigh \n" << high;
#else
    LOGGER(ibis::gVerbose > 30 ||
           ((low.bytes()+high.bytes()) < (2U << ibis::gVerbose)))
        << "low \n" << low << "\nhigh \n" << high;
#endif
#else
    LOGGER(ibis::gVerbose > 4)
        << "query[" << myID << "]::doEstimate("
        << static_cast<const void*>(term) << ": " << *term
        << ") --> [" << low.cnt() << ", "
        << (high.size()==low.size() ? high.cnt() : low.cnt()) << "]";
#endif
} // ibis::query::doEstimate

/// Generate the hit vector.  Make sure mypart is set before calling this
/// function.
int ibis::query::computeHits() {
    if (ibis::gVerbose > 7) {
        ibis::util::logger lg;
        lg() << "query[" << myID << "]::computeHits -- "
            "starting to compute hits for the query";
        if (conds.getExpr() != 0)
            lg() << " \"" << *conds.getExpr() << "\"";
    }

    int ierr = 0;
    if (hits == 0) { // have not performed an estimate
        ibis::bitvector mask;
        conds.getNullMask(*mypart, mask);
        if (! comps.empty()) {
            ibis::bitvector tmp;
            comps.getNullMask(*mypart, tmp);
            if (mask.size() > 0) {
                mask &= tmp;
            }
            else {
                mask.swap(tmp);
            }
        }

        if (rids_in != 0) { // has a RID list
            ibis::bitvector tmp;
            mypart->evaluateRIDSet(*rids_in, tmp);
            mask &= tmp;
        }

        if (conds.getExpr() != 0) { // a normal range query
            dstime = mypart->timestamp();
            hits = new ibis::bitvector;
#ifndef DONOT_REORDER_EXPRESSION
            if (! conds->directEval())
                reorderExpr();
#endif
            delete sup;
            sup = 0;
            ierr = doEvaluate(conds.getExpr(), mask, *hits);
            if (ierr < 0)
                return ierr - 20;
            hits->compress();
            sup = hits;
        }
        else {
            hits = new ibis::bitvector(mask);
            if (hits == 0) return -1;
        }
    }

    if (sup == 0) { // already have the exact answer
        sup = hits; // copy the pointer to make other operations easier
    }
    else if (sup->size() < hits->size()) {      
        delete sup;
        sup = hits;
    }
    else if (sup != hits) { // need to actually examine the data files involved
        (*sup) -= (*hits);
        if (sup->sloppyCount() > 0) {
            ibis::bitvector delta;
            ierr = doScan(conds.getExpr(), *sup, delta);
            if (ierr > 0) {
                delete sup;  // no longer need it
                *hits |= delta;
                sup = hits;
            }
            else if (ierr < 0) {
                (*sup) |= (*hits);
                return ierr - 20;
            }
        }
    }

    if ((rids_in != 0 || conds.getExpr() != 0) &&
        (ibis::gVerbose > 30 ||
         (ibis::gVerbose > 4 && (1U<<ibis::gVerbose) >= hits->bytes()))) {
        ibis::util::logger lg;
        lg() << "query::computeHits: hit vector" << *hits << "\n";
        if (ibis::gVerbose > 19) {
            ibis::bitvector::indexSet is = hits->firstIndexSet();
            lg() << "row numbers of the hits\n";
            while (is.nIndices()) {
                const ibis::bitvector::word_t *ii = is.indices();
                if (is.isRange()) {
                    lg() << *ii << " -- " << ii[1];
                }
                else {
                    for (unsigned i=0; i < is.nIndices(); ++ i)
                        lg() << ii[i] << " ";
                }
                lg() << "\n";
                ++ is;
            }
        }
    }
    return ierr;
} // ibis::query::computeHits

/// Perform a simple sequential scan.
/// Return a (new) bitvector that contains the result of directly scan
/// the raw data to determine what records satisfy the user specified
/// conditions.  It is mostly used for testing purposes.  It can be
/// called any time after the where clause is set, and does not change
/// the state of the current query.
long ibis::query::sequentialScan(ibis::bitvector& res) const {
    if (conds.empty())
        return -8;

    long ierr;
    readLock lock(this, "sequentialScan"); // read lock on query
    ibis::part::readLock lds(mypart, myID); // read lock on data
    ibis::horometer timer;
    if (ibis::gVerbose > 2)
        timer.start();
    try {
        ibis::bitvector msk;
        conds.getNullMask(*mypart, msk);
        ierr = doScan(conds.getExpr(), msk, res);
        if (ierr < 0)
            return ierr - 20;
    }
    catch (const ibis::bad_alloc& e) {
        ierr = -1;
        res.clear();
        logError("sequentialScan", "encountered a memory allocation error "
                 "(%s) while resolving \"%s\"", e.what(), conds.getString());
        ibis::util::emptyCache();
    }
    catch (const std::exception& e) {
        ierr = -2;
        res.clear();
        logError("sequentialScan", "encountered an exception (%s) "
                 "while resolving \"%s\"", e.what(), conds.getString());
        ibis::util::emptyCache();
    }
    catch (const char *e) {
        ierr = -3;
        res.clear();
        logError("sequentialScan", "encountered a string exception (%s) "
                 "while resolving \"%s\"", e, conds.getString());
        ibis::util::emptyCache();
    }
    catch (...) {
        ierr = -4;
        res.clear();
        logError("sequentialScan", "encountered unexpected exception "
                 "while resolving \"%s\"", conds.getString());
        ibis::util::emptyCache();
    }

    if (ierr >= 0 && ibis::gVerbose > 2) {
        timer.stop();
        ibis::util::logger lg;
        lg() << "query[" << myID << "]::sequentialScan produced " << ierr
             << " hit" << (ierr>1?"s":"") << " in " << timer.CPUTime()
             << " sec(CPU), " << timer.realTime() << " sec(elapsed)";
        if (ibis::gVerbose > 3 && hits != 0 && state == FULL_EVALUATE) {
            ibis::bitvector diff;
            diff.copy(*hits);
            diff ^= res;
#if DEBUG+0 > 1 || _DEBUG+0 > 1
            lg() << "\nExisting result\n";
            diff.print(lg());
            lg() << "\nsequentialScan result\n";
            res.print(lg());
            lg() << "\nXOR result\n";
            diff.print(lg());
#endif
            if (diff.cnt()) {
                lg() << "\nWarning -- query[" << myID
                     << "]::sequentialScan produced " << diff.cnt()
                     << " hit" << (diff.cnt()>1?"s":"")
                     << " that are different from the previous evaluation";
                if (ibis::gVerbose > 5) {
                    uint32_t maxcnt = (ibis::gVerbose > 30 ? mypart->nRows()
                                       : (1U << ibis::gVerbose));
                    if (maxcnt > diff.cnt())
                        maxcnt = diff.cnt();
                    uint32_t cnt = 0;
                    ibis::bitvector::indexSet is = diff.firstIndexSet();

                    lg() << "\n  row numbers of mismatching hits\n";
                    while (is.nIndices() && cnt < maxcnt) {
                        const ibis::bitvector::word_t *ii = is.indices();
                        if (is.isRange()) {
                            lg() << *ii << " -- " << ii[1];
                        }
                        else {
                            for (unsigned i=0; i < is.nIndices(); ++ i)
                                lg() << ii[i] << " ";
                        }
                        cnt += is.nIndices();
                        lg() << "\n";
                        ++ is;
                    }
                    if (cnt < diff.cnt())
                        lg() << "... (" << diff.cnt() - cnt
                             << " rows skipped)\n";
                }
            }
        }
    }
    return ierr;
} // ibis::query::sequentialScan

/// Get a bitvector containing all rows satisfying the query
/// condition. The resulting bitvector inculdes both active rows and
/// inactive rows.
long ibis::query::getExpandedHits(ibis::bitvector& res) const {
    long ierr;
    readLock lock(this, "getExpandedHits"); // don't change query
    if (mypart == 0 || mypart->nRows() == 0) {
        res.clear();
        ierr = -1;
    }
    else if (conds.getExpr() != 0) {
        ibis::part::readLock lock2(mypart, myID); // don't change data
        ierr = doEvaluate(conds.getExpr(), res);
    }
    else if (rids_in != 0) {
        ibis::part::readLock lock2(mypart, myID);
        ierr = mypart->evaluateRIDSet(*rids_in, res);
    }
    else {
        res.clear();
        ierr = -8;
    }
    return ierr;
} // ibis::query::getExpandedHits

/// Perform a sequential scan.
/// Reads the data partition to resolve the query expression.
int ibis::query::doScan(const ibis::qExpr* term,
                        ibis::bitvector& ht) const {
    int ierr = 0;
    if (term == 0) return ierr;
    if (term == 0) { // no hits
        ht.set(0, mypart->nRows());
        return ierr;
    }
    LOGGER(ibis::gVerbose > 7)
        << "query::[" << myID
        << "]::doScan -- reading data to resolve " << *term;

    switch (term->getType()) {
    case ibis::qExpr::LOGICAL_NOT: {
        ierr = doScan(term->getLeft(), ht);
        if (ierr >= 0) {
            ht.flip();
            ierr = ht.cnt();
        }
        break;
    }
    case ibis::qExpr::LOGICAL_AND: {
        ierr = doScan(term->getLeft(), ht);
        if (ierr > 0) {
            ibis::bitvector b1;
            ierr = doScan(term->getRight(), ht, b1);
            if (ierr >= 0)
                ht.swap(b1);
        }
        break;
    }
    case ibis::qExpr::LOGICAL_OR: {
        ierr = doScan(term->getLeft(), ht);
        if (ierr >= 0 && ht.cnt() < ht.size()) {
            ibis::bitvector b1;
            ierr = doScan(term->getRight(), b1);
            if (ierr > 0)
                ht |= b1;
            if (ierr >= 0)
                ierr = ht.sloppyCount();
        }
        break;
    }
    case ibis::qExpr::LOGICAL_XOR: {
        ierr = doScan(term->getLeft(), ht);
        if (ierr >= 0) {
            ibis::bitvector b1;
            ierr = doScan(term->getRight(), b1);
            if (ierr > 0)
                ht ^= b1;
            ierr = ht.sloppyCount();
        }
        break;
    }
    case ibis::qExpr::LOGICAL_MINUS: {
        ierr = doScan(term->getLeft(), ht);
        if (ierr > 0) {
            ibis::bitvector b1;
            ierr = doScan(term->getRight(), ht, b1);
            if (ierr >= 0)
                ht -= b1;
            ierr = ht.sloppyCount();
        }
        break;
    }
    case ibis::qExpr::EXISTS: {
        const ibis::qExists *qex = reinterpret_cast<const ibis::qExists*>(term);
        if (qex != 0 && mypart->getColumn(qex->colName())) { // does exist
            mypart->getNullMask(ht);
        }
        else { // does not exist
            ht.set(0, mypart->nRows());
        }
        ierr = ht.sloppyCount();
        break;}
    case ibis::qExpr::RANGE:
        ierr = mypart->doScan
            (*(reinterpret_cast<const ibis::qContinuousRange*>(term)), ht);
        break;
    case ibis::qExpr::DRANGE:
        ierr = mypart->doScan
            (*(reinterpret_cast<const ibis::qDiscreteRange*>(term)), ht);
        break;
    case ibis::qExpr::INTHOD:
        ierr = mypart->doScan
            (*(reinterpret_cast<const ibis::qIntHod*>(term)), ht);
        break;
    case ibis::qExpr::UINTHOD:
        ierr = mypart->doScan
            (*(reinterpret_cast<const ibis::qUIntHod*>(term)), ht);
        break;
    case ibis::qExpr::ANYANY:
        ierr = mypart->matchAny
            (*(reinterpret_cast<const ibis::qAnyAny*>(term)), ht);
        break;
    case ibis::qExpr::STRING:
        ierr = mypart->stringSearch
            (*(reinterpret_cast<const ibis::qString*>(term)), ht);
        break;
    case ibis::qExpr::ANYSTRING:
        ierr = mypart->stringSearch
            (*(reinterpret_cast<const ibis::qAnyString*>(term)), ht);
        break;
    case ibis::qExpr::KEYWORD:
        ierr = mypart->keywordSearch
            (*(reinterpret_cast<const ibis::qKeyword*>(term)), ht);
        break;
    case ibis::qExpr::ALLWORDS:
        ierr = mypart->keywordSearch
            (*(reinterpret_cast<const ibis::qAllWords*>(term)), ht);
        break;
    case ibis::qExpr::LIKE:
        ierr = mypart->patternSearch
            (*(reinterpret_cast<const ibis::qLike*>(term)), ht);
        break;
    case ibis::qExpr::COMPRANGE: {
        const ibis::compRange &cr =
            *(reinterpret_cast<const ibis::compRange*>(term));
        if (cr.isConstant()) {
            if (cr.inRange()) {
                ht.set(1, mypart->nRows());
                ierr = mypart->nRows();
            }
            else {
                ht.set(0, mypart->nRows());
                ierr = 0;
            }
        }
        else {
            ibis::bitvector mask;
            mask.set(1, mypart->nRows());
            ierr = mypart->doScan(cr, mask, ht);
        }
        break;}
    case ibis::qExpr::MATHTERM: { // arithmetic expressions as true/false
        const ibis::math::term &mt =
            *reinterpret_cast<const ibis::math::term*>(term);
        if (mt.isConstant()) {
            if (mt.isTrue()) {
                ht.set(1, mypart->nRows());
                ierr = mypart->nRows();
            }
            else {
                ht.set(0, mypart->nRows());
                ierr = 0;
            }
        }
        else {
            ibis::bitvector mask;
            mask.set(1, mypart->nRows());
            ierr = mypart->doScan(mt, mask, ht);
        }
        break;}
    case ibis::qExpr::TOPK:
    case ibis::qExpr::DEPRECATEDJOIN: { // pretend every row qualifies
        ht.set(1, mypart->nRows());
        ierr = -2;
        break;
    }
    default:
        logWarning("doScan", "failed to evaluate query term of "
                   "unknown type");
        ierr = -1;
    }
    if (ierr < 0) // no confirmed hits
        ht.set(0, mypart->nRows());
    return ierr;
} // ibis::query::doScan

/// Masked sequential scan.
/// Reads the data partition to resolve the query conditions.
int ibis::query::doScan(const ibis::qExpr* term, const ibis::bitvector& mask,
                        ibis::bitvector& ht) const {
    int ierr = 0;
    if (term == 0) return ierr;
    if (mask.cnt() == 0) { // no hits
        ht.set(0, mask.size());
        return ierr;
    }
    LOGGER(ibis::gVerbose > 5)
        << "query::[" << myID << "]::doScan -- reading data to resolve "
        << *term << " with mask.size() = " << mask.size()
        << " and mask.cnt() = " << mask.cnt();

    switch (term->getType()) {
    case ibis::qExpr::LOGICAL_NOT: {
        ierr = doScan(term->getLeft(), mask, ht);
        if (ierr >= 0) {
            std::unique_ptr<ibis::bitvector> tmp(mask - ht);
            ht.copy(*tmp);
            ierr = ht.cnt();
        }
        break;
    }
    case ibis::qExpr::LOGICAL_AND: {
        ierr = doScan(term->getLeft(), mask, ht);
        if (ierr > 0) {
            ibis::bitvector b1;
            ierr = doScan(term->getRight(), ht, b1);
            if (ierr >= 0)
                ht.swap(b1);
        }
        break;
    }
    case ibis::qExpr::LOGICAL_OR: {
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
            ibis::bitvector b1;
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
        break;
    }
    case ibis::qExpr::LOGICAL_XOR: {
        ierr = doScan(term->getLeft(), mask, ht);
        if (ierr >= 0) {
            ibis::bitvector b1;
            ierr = doScan(term->getRight(), mask, b1);
            if (ierr > 0)
                ht ^= b1;
            ierr = ht.sloppyCount();
        }
        break;
    }
    case ibis::qExpr::LOGICAL_MINUS: {
        ierr = doScan(term->getLeft(), mask, ht);
        if (ierr > 0) {
            ibis::bitvector b1;
            ierr = doScan(term->getRight(), ht, b1);
            if (ierr > 0)
                ht -= b1;
            ierr = ht.sloppyCount();
        }
        break;
    }
    case ibis::qExpr::EXISTS: {
        const ibis::qExists *qex = reinterpret_cast<const ibis::qExists*>(term);
        if (qex != 0 && mypart->getColumn(qex->colName())) { // does exist
            mypart->getNullMask(ht);
        }
        else { // does not exist
            ht.set(0, mypart->nRows());
        }
        ierr = ht.sloppyCount();
        break;}
    case ibis::qExpr::RANGE: {
#if defined(TEST_SCAN_OPTIONS)
        // there are five ways to perform the scan

        // (1) use the input mask directly.  In this case, ht would be the
        // answer, no more operations are required.  This will access
        // mask.cnt() rows and perform (mask.cnt() - (1-frac)*cnt1) setBit
        // operations to generate ht.

        // (2) use the mask for the index only (the first assignment of
        // iffy) and perform the positive comparisons.  This will access
        // cnt0 rows, perform cnt0*frac setBit operations and perform two
        // bitwise logical operations to generate ht (ht = mask - (iffy -
        // res)).

        // (3) use the mask for the index only and perform the negative
        // comparisons.  This will access cnt0 rows, perform cnt0*(1-frac)
        // setBit operations and perform one bitwise logical operation to
        // generate ht (ht = mask - res).

        // (4) use the combined mask (the second assignment of variable
        // iffy) and perform the positive comparisons.  This will access
        // cnt1 rows, perform cnt1*frac setBit operations and perform two
        // bitwise logical operations to generate ht (ht = mask - (iffy -
        // res)).

        // (5) use the combined mask and perform the negative comparisons.
        // This will access cnt1 rows, perform cnt1*(1-frac) setBit
        // operations, and one bitwise logical operation to generate ht (ht
        // = mask - res).

        // For the initial implementation (Feb 14, 2004), only options (4)
        // and (5) are considered.  To differentiate them, we need to
        // evaluate the difference in the bitwise logical operations in
        // terms of setBit operations.  To do this, we assume the cost of
        // each bitwise minus operation is proportional to the total size of
        // the two operands and each setBit operation is equivalent to
        // operating on two words in the bitwise logical operation.  To
        // compare options (4) and (5), we now need to estimate the size of
        // bitvector res.  To estimate the size of res, we assume it has the
        // same clustering factor as iffy and use frac to compute its bit
        // density.   The clustering factor of iffy can be computed by
        // assuming the bits in the bitvector are generated from a simple
        // Markov process.
        // Feb 17, 2004
        // Due to some unexpected difficulties in estimating the clustering
        // factor, we will implement only option (4) for now.
        // Feb 18, 2004
        // The cost difference between options (4) and (5) is
        // option (4) -- 2*sizeof(word_t)*iffy.cnt()*frac + iffy.bytes() +
        // markovSize(iffy.size(), frac*iffy.cnt(),
        // clusteringFactor(iffy.size(), iffy.cnt(), iffy.bytes()))
        // option (5) -- 2*sizeof(word_t)*iffy.cnt()*(1-frac)
        // Feb 19, 2004
        // To properly determine the parameters to choosing the difference
        // options, implement all five choices and use an global variable
        // ibis::_scan_option to determine the choice to use.  Once a
        // reasonable set of parameters are determined, we will remove the
        // global variable.
        ibis::horometer timer;
        timer.start();
        switch (ibis::_scan_option) {
        default:
        case 1: { // option 1 -- the original implementation use this one
            ierr = mypart->doScan
                (*(reinterpret_cast<const ibis::qRange*>(term)), mask, ht);
            break;
        }
        case 2: { // option 2
            ibis::bitvector iffy;
            float frac = mypart->getUndecidable
                (*(reinterpret_cast<const ibis::qContinuousRange*>(term)),
                 iffy);
            const uint32_t cnt0 = iffy.cnt();
            if (cnt0 > 0) {
                ierr = mypart->doScan
                    (*(reinterpret_cast<const ibis::qRange*>(term)),
                     iffy, ht);
                if (ierr >= 0) {
                    iffy -= ht;
                    ht.copy(mask);
                    ht -= iffy;
                }
            }
            else { // no row eliminated
                ht.copy(mask);
            }
            break;
        }
        case 3: { // option 3
            ibis::bitvector iffy;
            float frac = mypart->getUndecidable
                (*(reinterpret_cast<const ibis::qContinuousRange*>(term)),
                 iffy);
            const uint32_t cnt0 = iffy.cnt();
            if (cnt0 > 0) {
                ibis::bitvector comp;
                ierr = mypart->negativeScan
                    (*(reinterpret_cast<const ibis::qRange*>(term)),
                     comp, iffy);
                if (ierr >= 0) {
                    ht.copy(mask);
                    ht -= comp;
                }
            }
            else { // no row eliminated
                ht.copy(mask);
            }
            break;
        }
        case 4: { // option 4
            ibis::bitvector iffy;
            float frac = mypart->getUndecidable
                (*(reinterpret_cast<const ibis::qContinuousRange*>(term)),
                 iffy);
            const uint32_t cnt0 = iffy.cnt();
            if (cnt0 > 0) {
                iffy &= mask;
                const uint32_t cnt1 = iffy.cnt();
                if (cnt1 > 0) {
                    ierr = mypart->doScan
                        (*(reinterpret_cast<const ibis::qRange*>(term)),
                         iffy, ht);
                    if (ierr >= 0) {
                        iffy -= ht;
                        ht.copy(mask);
                        ht -= iffy;
                    }
                }
                else { // no row eliminated
                    ht.copy(mask);
                }
            }
            else { // no row eliminated
                ht.copy(mask);
            }
            break;
        }
        case 5: { // option 5
            ibis::bitvector iffy;
            float frac = mypart->getUndecidable
                (*(reinterpret_cast<const ibis::qContinuousRange*>(term)),
                 iffy);
            const uint32_t cnt0 = iffy.cnt();
            if (cnt0 > 0) {
                iffy &= mask;
                const uint32_t cnt1 = iffy.cnt();
                const double fudging=2*sizeof(ibis::bitvector::word_t);
//              const double cf = ibis::bitvector::clusteringFactor
//                  (iffy.size(), iffy.cnt(), iffy.bytes());
                if (cnt1 > 0) {
                    ibis::bitvector comp;
                    ierr = mypart->negativeScan
                        (*(reinterpret_cast<const ibis::qRange*>(term)),
                         comp, iffy);
                    if (ierr >= 0) {
                        ht.copy(mask);
                        ht -= comp;
                    }
                }
                else { // no row eliminated
                    ht.copy(mask);
                }
            }
            else { // no row eliminated
                ht.copy(mask);
            }
            break;
        }
        } // end of switch (ibis::_scan_option)
        timer.stop();
        logMessage("doScan", "Evaluating range condition (option %d) took "
                   "%g sec elapsed time", ibis::_scan_option,
                   timer.realTime());
#else
//      // 05/11/05: commented out because it only works for ranges joined
//      // together with AND operators! TODO: need something better
//      // a combined version of option 4 and 5
//      ibis::bitvector iffy;
//      float frac = mypart->getUndecidable
//          (*(reinterpret_cast<const ibis::qContinuousRange*>(term)),
//           iffy);
//      const uint32_t cnt0 = iffy.cnt();
//      if (cnt0 > 0) {
//          iffy &= mask;
//          const uint32_t cnt1 = iffy.cnt();
//          if (cnt1 == 0) { // no row eliminated
//              ht.copy(mask);
//          }
//          else if (cnt1 >= cnt0) { // directly use the input mask
//              mypart->doScan
//                  (*(reinterpret_cast<const ibis::qRange*>(term)),
//                   mask, ht);
//          }
//          else if (static_cast<int>(frac+frac) > 0) {
//              // negative evaluation -- option 5
//              ibis::bitvector comp;
//              mypart->negativeScan
//                  (*(reinterpret_cast<const ibis::qRange*>(term)),
//                   comp, iffy);
//              ht.copy(mask);
//              ht -= comp;
//          }
//          else {
//              // direct evaluation -- option 4
//              mypart->doScan
//                  (*(reinterpret_cast<const ibis::qRange*>(term)),
//                   iffy, ht);
//              iffy -= ht;
//              ht.copy(mask);
//              ht -= iffy;
//          }
//      }
//      else { // no row eliminated
//          ht.copy(mask);
//      }
        ierr = mypart->doScan
            (*(reinterpret_cast<const ibis::qRange*>(term)), mask, ht);
#endif
        break;
    }
    case ibis::qExpr::DRANGE: {
        ierr = mypart->doScan
            (*(reinterpret_cast<const ibis::qDiscreteRange*>(term)), mask, ht);
        break;
    }
    case ibis::qExpr::INTHOD: {
        ierr = mypart->doScan
            (*(reinterpret_cast<const ibis::qIntHod*>(term)), mask, ht);
        break;
    }
    case ibis::qExpr::UINTHOD: {
        ierr = mypart->doScan
            (*(reinterpret_cast<const ibis::qUIntHod*>(term)), mask, ht);
        break;
    }
    case ibis::qExpr::ANYANY: {
        ierr = mypart->matchAny
            (*(reinterpret_cast<const ibis::qAnyAny*>(term)), mask, ht);
        break;
    }
    case ibis::qExpr::STRING: {
        ierr = mypart->stringSearch
            (*(reinterpret_cast<const ibis::qString*>(term)), ht);
        if (ierr >= 0) {
            ht &= mask;
            ierr = ht.cnt();
        }
        break;
    }
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
            ierr = ht.cnt();
        }
        break;
    }
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
        break;
    }
    case ibis::qExpr::MATHTERM: {
        const ibis::math::term &mt =
            *(reinterpret_cast<const ibis::math::term*>(term));
        if (mt.isConstant()) {
            if (mt.isTrue()) {
                ht.copy(mask);
                ierr = mask.cnt();
            }
            else {
                ht.set(0, mask.size());
                ierr = 0;
            }
        }
        else {
            ierr = mypart->doScan(mt, mask, ht);
        }
        break;
    }
    case ibis::qExpr::TOPK:
    case ibis::qExpr::DEPRECATEDJOIN: { // pretend every row qualifies
        ht.copy(mask);
        ierr = -2;
        break;
    }
    default: {
        logWarning("doScan", "failed to evaluate query term of "
                   "unknown type");
        ht.set(0, mypart->nRows());
        ierr = -1;
        break;}
    }
    if (ierr < 0) // no confirmed hits
        ht.set(0, mypart->nRows());
#if defined(DEBUG) || defined(_DEBUG)
    ibis::util::logger lg(4);
    lg() << "query[" << myID << "]::doScan("
         << static_cast<const void*>(term) << ": " << *term
         << ") --> " << ht.cnt() << ", ierr = " << ierr << "\n";
#if DEBUG + 0 > 1 || _DEBUG + 0 > 1
    lg() << "ht \n" << ht;
#else
    if (ibis::gVerbose > 30 || (ht.bytes() < (2 << ibis::gVerbose)))
        lg() << "ht \n" << ht;
#endif
#else
    LOGGER(ibis::gVerbose > 4)
        << "query[" << myID << "]::doScan("
        << static_cast<const void*>(term) << ": " << *term
        << ") --> " << ht.cnt() << ", ierr = " << ierr;
#endif
    return ierr;
} // ibis::query::doScan

/// Evaluate the query expression.
/// Combines the operations on index and the sequential scan in one
/// function.
///
/// It returns a non-negative value to indicate success and a negative
/// value to indicate error.  Note that a return value of zero indicates
/// that no hits is found.  However, a positive return value does not
/// necessarily mean the number of hits is greater than zero, it simply
/// means that it will take some work to figure out the actually number of
/// hits.
int ibis::query::doEvaluate(const ibis::qExpr* term,
                            ibis::bitvector& ht) const {
    if (term == 0) { // match everything
        ht.set(1, mypart->nRows());
        return mypart->nRows();
    }
    LOGGER(ibis::gVerbose > 5)
        << "query[" << myID << "]::doEvaluate -- starting to evaluate "
        << *term;

    int ierr = 0;
    switch (term->getType()) {
    case ibis::qExpr::LOGICAL_NOT: {
        ierr = doEvaluate(term->getLeft(), ht);
        if (ierr >= 0) {
            ht.flip();
            ierr = ht.sloppyCount();
        }
        break;
    }
    case ibis::qExpr::LOGICAL_AND: {
        ierr = doEvaluate(term->getLeft(), ht);
        if (ierr > 0) {
            ibis::bitvector b1;
            ierr = doEvaluate(term->getRight(), ht, b1);
            if (ierr >= 0)
                ht.swap(b1);
            else
                ht.clear();
        }
        break;
    }
    case ibis::qExpr::LOGICAL_OR: {
        ierr = doEvaluate(term->getLeft(), ht);
        if (ierr >= 0) { //  && ht.cnt() < ht.size()
            ibis::bitvector b1;
            ierr = doEvaluate(term->getRight(), b1);
            if (ierr > 0)
                ht |= b1;
            if (ierr >= 0)
                ierr = ht.sloppyCount();
        }
        break;
    }
    case ibis::qExpr::LOGICAL_XOR: {
        ierr = doEvaluate(term->getLeft(), ht);
        if (ierr >= 0) {
            ibis::bitvector b1;
            ierr = doEvaluate(term->getRight(), b1);
            if (ierr >= 0) {
                ht ^= b1;
                ierr = ht.sloppyCount();
            }
            else
                ht.clear();
        }
        break;
    }
    case ibis::qExpr::LOGICAL_MINUS: {
        ierr = doEvaluate(term->getLeft(), ht);
        if (ierr > 0) {
            ibis::bitvector b1;
            ierr = doEvaluate(term->getRight(), ht, b1);
            if (ierr >= 0) {
                ht -= b1;
                ierr = ht.sloppyCount();
            }
        }
        break;
    }
    case ibis::qExpr::EXISTS: {
        const ibis::qExists *qex = reinterpret_cast<const ibis::qExists*>(term);
        if (qex != 0 && mypart->getColumn(qex->colName())) { // does exist
            mypart->getNullMask(ht);
        }
        else { // does not exist
            ht.set(0, mypart->nRows());
        }
        ierr = ht.sloppyCount();
        break;}
    case ibis::qExpr::RANGE: {
        ibis::bitvector tmp;
        tmp.set(1, mypart->nRows());
        ierr = mypart->evaluateRange
            (*(reinterpret_cast<const ibis::qContinuousRange*>(term)),
             tmp, ht);
        if (ierr < 0) {
            ierr = mypart->estimateRange
                (*(reinterpret_cast<const ibis::qContinuousRange*>(term)),
                 ht, tmp);
            if (ierr >= 0 && ht.size() == tmp.size() && ht.cnt() < tmp.cnt()) {
                // estimateRange produced two bounds as the solution, need to
                // scan some entries to determine exactly which satisfy the
                // condition
                tmp -= ht; // tmp now contains entries to be scanned
                ibis::bitvector res;
                ierr = mypart->doScan
                    (*(reinterpret_cast<const ibis::qRange*>(term)), tmp, res);
                if (ierr > 0)
                    ht |= res;
                ierr = ht.sloppyCount();
            }
        }
        break;
    }
    case ibis::qExpr::DRANGE: { // call evalauteRange, use doScan on failure
        ierr = mypart->evaluateRange
            (*(reinterpret_cast<const ibis::qDiscreteRange*>(term)),
             mypart->getMaskRef(), ht);
        if (ierr < 0) { // revert to estimate and scan
            ibis::bitvector tmp;
            ierr = mypart->estimateRange
                (*(reinterpret_cast<const ibis::qDiscreteRange*>(term)),
                 ht, tmp);
            if (ierr >= 0 && ht.size() == tmp.size() && ht.cnt() < tmp.cnt()) {
                // estimateRange produced two bounds as the solution, need to
                // scan some entries to determine exactly which satisfy the
                // condition
                tmp -= ht; // tmp now contains entries to be scanned
                ibis::bitvector res;
                ierr = mypart->doScan
                    (*(reinterpret_cast<const ibis::qRange*>(term)), tmp, res);
                if (ierr >= 0)
                    ht |= res;
                ierr = ht.sloppyCount();
            }
        }
        break;
    }
    case ibis::qExpr::INTHOD: { // call evalauteRange, use doScan on failure
        ierr = mypart->evaluateRange
            (*(reinterpret_cast<const ibis::qIntHod*>(term)),
             mypart->getMaskRef(), ht);
        break;
    }
    case ibis::qExpr::UINTHOD: { // call evalauteRange, use doScan on failure
        ierr = mypart->evaluateRange
            (*(reinterpret_cast<const ibis::qUIntHod*>(term)),
             mypart->getMaskRef(), ht);
        break;
    }
    case ibis::qExpr::STRING: {
        ierr = mypart->stringSearch
            (*(reinterpret_cast<const ibis::qString*>(term)), ht);
        break;
    }
    case ibis::qExpr::ANYSTRING:
        ierr = mypart->stringSearch
            (*(reinterpret_cast<const ibis::qAnyString*>(term)), ht);
        break;
    case ibis::qExpr::KEYWORD:
        ierr = mypart->keywordSearch
            (*(reinterpret_cast<const ibis::qKeyword*>(term)), ht);
        break;
    case ibis::qExpr::ALLWORDS:
        ierr = mypart->keywordSearch
            (*(reinterpret_cast<const ibis::qAllWords*>(term)), ht);
        break;
    case ibis::qExpr::LIKE: {
        ierr = mypart->patternSearch
            (*(reinterpret_cast<const ibis::qLike*>(term)), ht);
        break;
    }
    case ibis::qExpr::COMPRANGE: {
        const ibis::compRange &cr =
            *(reinterpret_cast<const ibis::compRange*>(term));
        if (cr.isConstant()) {
            if (cr.inRange()) {
                ht.set(1, mypart->nRows());
                ierr = mypart->nRows();
            }
            else {
                ht.set(0, mypart->nRows());
                ierr = 0;
            }
        }
        else {
            ierr = mypart->doScan(cr, ht);
        }
        break;
    }
    case ibis::qExpr::MATHTERM: {
        const ibis::math::term &mt =
            *(reinterpret_cast<const ibis::math::term*>(term));
        if (mt.isConstant()) {
            if (mt.isTrue()) {
                ht.set(1, mypart->nRows());
                ierr = mypart->nRows();
            }
            else {
                ht.set(0, mypart->nRows());
                ierr = 0;
            }
        }
        else {
            ibis::bitvector mask;
            mask.set(1, mypart->nRows());
            ierr = mypart->doScan(mt, mask, ht);
        }
        break;
    }
    case ibis::qExpr::ANYANY: {
        const ibis::qAnyAny *tmp =
            reinterpret_cast<const ibis::qAnyAny*>(term);
        ibis::bitvector more;
        mypart->estimateMatchAny(*tmp, ht, more);
        if (ht.size() == more.size() && ht.cnt() < more.cnt()) {
            more -= ht;
            if (more.sloppyCount() > 0) {
                ibis::bitvector res;
                mypart->matchAny(*tmp, res, more);
                ht |= res;
            }
        }
        ierr = ht.cnt();
        break;
    }
    case ibis::qExpr::TOPK:
    case ibis::qExpr::DEPRECATEDJOIN: { // pretend every row qualifies
        ht.set(1, mypart->nRows());
        ierr = mypart->nRows();
        break;
    }
    default:
        logWarning("doEvaluate", "failed to evaluate query term of "
                   "unknown type, presume every row is a hit");
        ht.set(0, mypart->nRows());
        ierr = -1;
    }
#if defined(DEBUG) || defined(_DEBUG)
    ibis::util::logger lg(4);
    lg() << "query[" << myID << "]::doEvaluate("
         << static_cast<const void*>(term) << ": " << *term
         << ") --> " << ht.cnt() << ", ierr = " << ierr << "\n";
#if DEBUG + 0 > 1 || _DEBUG + 0 > 1
    lg() << "ht \n" << ht;
#else
    if (ibis::gVerbose > 30 || (ht.bytes() < (2 << ibis::gVerbose)))
        lg() << "ht \n" << ht;
#endif
#else
    LOGGER(ibis::gVerbose > 4)
        << "query[" << myID << "]::doEvaluate("
        << static_cast<const void*>(term) << ": " << *term
        << ") --> " << ht.cnt() << ", ierr = " << ierr;
#endif
    return ierr;
} // ibis::query::doEvaluate

/// Evaluate the query expression with mask.
/// Combines the operations on index and the sequential scan in one function.
int ibis::query::doEvaluate(const ibis::qExpr* term,
                            const ibis::bitvector& mask,
                            ibis::bitvector& ht) const {
    if (term == 0) { // all hits
        ht.copy(mask);
        return mypart->nRows();
    }
    if (mask.cnt() == 0) { // no hits
        ht.set(0, mask.size());
        return 0;
    }
    LOGGER(ibis::gVerbose > 7)
        << "query[" << myID << "]::doEvaluate -- starting to evaluate "
        << *term;

    int ierr = 0;
    switch (term->getType()) {
    case ibis::qExpr::LOGICAL_NOT: {
        ierr = doEvaluate(term->getLeft(), mask, ht);
        if (ierr >= 0) {
            ht.flip();
            ht &= mask;
            ierr = ht.sloppyCount();
        }
        break;
    }
    case ibis::qExpr::LOGICAL_AND: {
        ierr = doEvaluate(term->getLeft(), mask, ht);
        if (ierr > 0) {
            ibis::bitvector b1;
            ierr = doEvaluate(term->getRight(), ht, b1);
            if (ierr >= 0)
                ht.swap(b1);
        }
        break;
    }
    case ibis::qExpr::LOGICAL_OR: {
        ierr = doEvaluate(term->getLeft(), mask, ht);
        if (ierr >= 0) {// && ht.cnt() < mask.cnt()
            ibis::bitvector b1;
            ierr = doEvaluate(term->getRight(), mask, b1);
            if (ierr > 0)
                ht |= b1;
            if (ierr >= 0)
                ierr = ht.sloppyCount();
        }
        break;
    }
    case ibis::qExpr::LOGICAL_XOR: {
        ierr = doEvaluate(term->getLeft(), mask, ht);
        if (ierr >= 0) {
            ibis::bitvector b1;
            ierr = doEvaluate(term->getRight(), mask, b1);
            if (ierr >= 0) {
                ht ^= b1;
                ierr = ht.sloppyCount();
            }
        }
        break;
    }
    case ibis::qExpr::LOGICAL_MINUS: {
        ierr = doEvaluate(term->getLeft(), mask, ht);
        if (ierr > 0) {
            ibis::bitvector b1;
            ierr = doEvaluate(term->getRight(), ht, b1);
            if (ierr >= 0) {
                ht -= b1;
                ierr = ht.sloppyCount();
            }
        }
        break;
    }
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
                ierr = ht.sloppyCount();
            }
        }
        break;
    }
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
                ierr = ht.sloppyCount();
            }
        }
        break;
    }
    case ibis::qExpr::INTHOD: { // try evaluateRange, then doScan
        ierr = mypart->evaluateRange
            (*(reinterpret_cast<const ibis::qIntHod*>(term)), mask, ht);
        if (ierr < 0) { // revert to estimate and scan
            ibis::bitvector tmp;
            ierr = mypart->estimateRange
                (*(reinterpret_cast<const ibis::qIntHod*>(term)),
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
                        (*(reinterpret_cast<const ibis::qIntHod*>(term)),
                         tmp, res);
                    if (ierr > 0)
                        ht |= res;
                }
                ierr = ht.sloppyCount();
            }
        }
        break;
    }
    case ibis::qExpr::UINTHOD: { // try evaluateRange, then doScan
        ierr = mypart->evaluateRange
            (*(reinterpret_cast<const ibis::qUIntHod*>(term)), mask, ht);
        if (ierr < 0) { // revert to estimate and scan
            ibis::bitvector tmp;
            ierr = mypart->estimateRange
                (*(reinterpret_cast<const ibis::qUIntHod*>(term)),
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
                        (*(reinterpret_cast<const ibis::qUIntHod*>(term)),
                         tmp, res);
                    if (ierr > 0)
                        ht |= res;
                }
                ierr = ht.sloppyCount();
            }
        }
        break;
    }
    case ibis::qExpr::STRING: {
        ierr = mypart->stringSearch
            (*(reinterpret_cast<const ibis::qString*>(term)), ht);
        if (ierr > 0) {
            ht &= mask;
            ierr = ht.sloppyCount();
        }
        break;
    }
    case ibis::qExpr::ANYSTRING:
        ierr = mypart->stringSearch
            (*(reinterpret_cast<const ibis::qAnyString*>(term)), ht);
        if (ierr > 0) {
            ht &= mask;
            ierr = ht.sloppyCount();
        }
        break;
    case ibis::qExpr::KEYWORD:
        ierr = mypart->keywordSearch
            (*(reinterpret_cast<const ibis::qKeyword*>(term)), ht);
        if (ierr > 0) {
            ht &= mask;
            ierr = ht.sloppyCount();
        }
        break;
    case ibis::qExpr::ALLWORDS:
        ierr = mypart->keywordSearch
            (*(reinterpret_cast<const ibis::qAllWords*>(term)), ht);
        if (ierr > 0) {
            ht &= mask;
            ierr = ht.sloppyCount();
        }
        break;
    case ibis::qExpr::LIKE: {
        ierr = mypart->patternSearch
            (*(reinterpret_cast<const ibis::qLike*>(term)), ht);
        if (ierr > 0) {
            ht &= mask;
            ierr = ht.sloppyCount();
        }
        break;
    }
    case ibis::qExpr::COMPRANGE: {
        const ibis::compRange &cr =
            *(reinterpret_cast<const ibis::compRange*>(term));
        if (cr.isConstant()) {
            if (cr.inRange()) {
                ht.copy(mask);
                ierr = ht.sloppyCount();
            }
            else {
                ht.set(0, mask.size());
                ierr = 0;
            }
        }
        else {
            ierr = mypart->doScan(cr, mask, ht);
        }
        break;
    }
    case ibis::qExpr::MATHTERM: {
        const ibis::math::term &mt =
            *(reinterpret_cast<const ibis::math::term*>(term));
        if (mt.isConstant()) {
            if (mt.isTrue()) {
                ht.copy(mask);
                ierr = mask.sloppyCount();
            }
            else {
                ht.set(0, mask.size());
                ierr = 0;
            }
        }
        else {
            ierr = mypart->doScan(mt, mask, ht);
        }
        break;
    }
    case ibis::qExpr::ANYANY: {
        const ibis::qAnyAny *tmp =
            reinterpret_cast<const ibis::qAnyAny*>(term);
        ibis::bitvector more;
        ierr = mypart->estimateMatchAny(*tmp, ht, more);
        ht &= mask;
        if (ht.size() == more.size() && ht.cnt() < more.cnt()) {
            more -= ht;
            more &= mask;
            if (more.sloppyCount() > 0) {
                ibis::bitvector res;
                mypart->matchAny(*tmp, more, res);
                ht |= res;
            }
        }
        ierr = ht.cnt();
        break;
    }
    case ibis::qExpr::TOPK:
    case ibis::qExpr::DEPRECATEDJOIN: { // pretend every row qualifies
        ht.copy(mask);
        ierr = ht.sloppyCount();
        break;
    }
    default:
        logWarning("doEvaluate", "failed to evaluate a query term of "
                   "unknown type, copy the mask as the solution");
        ht.set(0, mask.size());
        ierr = -1;
    }
#if defined(DEBUG) || defined(_DEBUG)
    ibis::util::logger lg(4);
    lg() << "query[" << myID << "]::doEvaluate("
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
        << "query[" << myID << "]::doEvaluate("
        << static_cast<const void*>(term) << ": " << *term
        << ", mask.cnt()=" << mask.cnt() << ") --> " << ht.cnt()
        << ", ierr = " << ierr;
#endif
    return ierr;
} // ibis::query::doEvaluate

/// A function to read the query file in a directory -- used by the
/// constructor that takes a directory name as the argument
/// the file contains:
/// - user id
/// - dataset name
/// - list of components
/// - query state
/// - time stamp on the dataset
/// - query condition or <NULL>
/// - list of OIDs
void ibis::query::readQuery(const ibis::partList& tl) {
    if (myDir == 0)
        return;

    char* ptr;
    char fn[MAX_LINE];
    strcpy(fn, myDir);
    strcat(fn, "query");

    FILE* fptr = fopen(fn, "r");
    if (fptr == 0) {
        logWarning("readQuery", "failed to open query file \"%s\" ... %s", fn,
                   (errno ? strerror(errno) : "no free stdio stream"));
        clear(); // clear the files and directory
        return;
    }

    IBIS_BLOCK_GUARD(fclose, fptr);
    // user id
    if (0 == fgets(fn, MAX_LINE, fptr)) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- query::readQuery failed to read user id from "
            << myDir << "query";
        return;
    }
    delete [] user;
    ptr = fn + std::strlen(fn);
    -- ptr;
    while (isspace(*ptr)) {
        *ptr = 0;
        -- ptr;
    }
    user = ibis::util::strnewdup(fn);

    // data partition names
    if (0 == fgets(fn, MAX_LINE, fptr)) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- query::readQuery failed to read the data partition "
            "name from " << myDir << "query";
        return;
    }

    ptr = fn + std::strlen(fn);
    -- ptr;
    while (isspace(*ptr)) {
        *ptr = 0;
        -- ptr;
    }
    for (uint32_t j = 0; j < tl.size(); ++ j) {
        if (stricmp(fn, tl[j]->name()) == 0) {
            mypart = tl[j];
            break;
        }
    }
    if (mypart == 0) { // partition name is not valid
        state = UNINITIALIZED;
        delete [] user;
        user = 0;
        return;
    }

    // select clause
    if (0 == fgets(fn, MAX_LINE, fptr)) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- query::readQuery failed to read the select clause "
            "from " << myDir << "query";
        return;
    }

    ptr = fn + std::strlen(fn);
    -- ptr;
    while (isspace(*ptr)) {
        *ptr = 0;
        -- ptr;
    }
    if (strnicmp(fn, "<NULL>", 6))
        setSelectClause(fn);

    // data partition state (read as an integer)
    int ierr;
    if (1 > fscanf(fptr, "%d", &ierr)) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- query::readQuery failed to read the query state "
            "from " << myDir << "query";
        return;
    }
    state = (QUERY_STATE) ierr;

    // time stamp (read as an integer)
    if (1 != fscanf(fptr, "%ld", &dstime))  {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- query::readQuery failed to read the time stamp "
            "from " << myDir << "query";
        return;
    }

    // where clause or RID list
    if (0 == fgets(fn, MAX_LINE, fptr)) { // skip the END_OF_LINE character
        return;
    }
    if (0 == fgets(fn, MAX_LINE, fptr)) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- query::readQuery failed to read the where clause "
            "from " << myDir << "query";
        return;
    }
    ptr = fn + std::strlen(fn);
    -- ptr;
    while (isspace(*ptr)) {
        *ptr = 0;
        -- ptr;
    }
    if (std::strcmp(fn, "<NULL>")) { // not NONE
        setWhereClause(fn);
    }
    else { // read the remaining part of the file to fill rids_in
        if (rids_in != 0)
            rids_in->clear();
        else
            rids_in = new RIDSet();
        unsigned tmp[2];
        while (fscanf(fptr, "%u %u", tmp, tmp+1) == 2) {
            ibis::rid_t rid;
            rid.num.run = *tmp;
            rid.num.event = tmp[1];
            rids_in->push_back(rid);
        }
    }
} // ibis::query::readQuery

/// Write the content of the current query into a file.
void ibis::query::writeQuery() {
    if (myDir == 0)
        return;

    char fn[PATH_MAX];
    strcpy(fn, myDir);
    strcat(fn, "query");

    FILE* fptr = fopen(fn, "w");
    if (fptr == 0) {
        logWarning("writeQuery", "failed to open file \"%s\" ... %s", fn,
                   (errno ? strerror(errno) : "no free stdio stream"));
        return;
    }

    if (! comps.empty())
        fprintf(fptr, "%s\n%s\n%s\n%d\n", user, mypart->name(),
                *comps, (int)state);
    else
        fprintf(fptr, "%s\n%s\n<NULL>\n%d\n", user, mypart->name(),
                (int)state);
    fprintf(fptr, "%ld\n", dstime);
    if (conds.getString() != 0) {
        fprintf(fptr, "%s\n", conds.getString());
    }
    else if (conds.getExpr() != 0) {
        std::ostringstream ostr;
        ostr << *conds.getExpr();
        fprintf(fptr, "%s\n", ostr.str().c_str());
    }
    else {
        fprintf(fptr, "<NULL>\n");
    }
    if (rids_in) {
        ibis::RIDSet::const_iterator it;
        for (it = rids_in->begin(); it != rids_in->end(); ++it) {
            fprintf(fptr, "%lu %lu\n",
                    static_cast<long unsigned>((*it).num.run),
                    static_cast<long unsigned>((*it).num.event));
        }
    }
    fclose(fptr);
} // ibis::query::writeQuery

void ibis::query::readHits() {
    if (myDir == 0)
        return;

    char fn[PATH_MAX];
    strcpy(fn, myDir);
    strcat(fn, "hits");
    if (hits == 0)
        hits = new ibis::bitvector;
    hits->read(fn);
    sup = hits;
} // ibis::query::readHits

void ibis::query::writeHits() const {
    if (hits != 0 && myDir != 0) {
        char fn[PATH_MAX];
        strcpy(fn, myDir);
        strcat(fn, "hits");
        hits->write(fn); // write hit vector
    }
} // ibis::query::writeHits

/// Read RIDs from the file named "-rids".  Return a pointer to
/// ibis::RIDSet.
ibis::RIDSet* ibis::query::readRIDs() const {
    if (myDir == 0)
        return 0;

    char fn[PATH_MAX];
    strcpy(fn, myDir);
    strcat(fn, "-rids");

    ibis::RIDSet* rids = new ibis::RIDSet();
    int ierr = ibis::fileManager::instance().getFile(fn, *rids);
    if (ierr != 0) {
        logWarning("readRIDs", "failed to open file \"%s\"", fn);
        remove(fn); // attempt to remove it
        delete rids;
        rids = 0;
    }
    else {
#if DEBUG+0 > 0 || _DEBUG+0 > 0
        ibis::util::logger lg(4);
        lg() << "query[" << myID << "::readRIDs() got " << rids->size() << "\n";
        for (ibis::RIDSet::const_iterator it = rids->begin();
             it != rids->end(); ++it)
            lg() << (*it) << "\n";
#endif
        if (rids->size() == 0) {
            delete rids;
            rids = 0;
        }
    }
    return rids;
} // ibis::query::readRIDs

/// Write the list of RIDs to a file named "-rids".
void ibis::query::writeRIDs(const ibis::RIDSet* rids) const {
    if (rids != 0 && myDir != 0) {
        char *fn = new char[std::strlen(myDir) + 8];
        strcpy(fn, myDir);
        strcat(fn, "-rids");
        rids->write(fn);
        delete [] fn;
    }
} // ibis::query::writeRIDs

/// It re-initializes the select clause and the where clause to blank.
void ibis::query::clear() {
    LOGGER(ibis::gVerbose > 4)
        << "query[" << myID << "]::clear -- clearing stored information";

    writeLock lck(this, "clear");
    comps.clear();
    // clear all pointers to in-memory resrouces
    delete rids_in;
    rids_in = 0;

    if (hits == sup) { // remove bitvectors
        delete hits;
        hits = 0;
        sup = 0;
    }
    else {
        delete hits;
        delete sup;
        hits = 0;
        sup = 0;
    }
    if (dslock != 0) { // remove read lock on the associated data partition
        delete dslock;
        dslock = 0;
    }

    if (myDir) {
        ibis::fileManager::instance().flushDir(myDir);
        std::string pnm = "query.";
        pnm += myID;
        pnm += ".purgeTempFiles";
        if (ibis::gParameters().isTrue(pnm.c_str())) {
            ibis::util::removeDir(myDir);
            LOGGER(ibis::gVerbose > 6)
                << "query[" << myID << "]::clear removed " << myDir;
        }
    }
} // ibis::query::clear

void ibis::query::removeFiles() {
    if (dslock != 0) { // release read lock on the data partition
        delete dslock;
        dslock = 0;
    }

    if (myDir == 0) return;
    // remove all files generated for this query and recreate the directory
    uint32_t len = std::strlen(myDir);
    char* fname = new char[len + 16];
    strcpy(fname, myDir);
    strcat(fname, "query");
    if (0 == remove(fname)) {
        if (ibis::gVerbose > 6)
            logMessage("clear", "removed %s", fname);
    }
    else if (errno != ENOENT || ibis::gVerbose > 7)
        logMessage("clear", "failed to remove %s ... %s", fname,
                   strerror(errno));

    strcpy(fname+len, "hits");
    ibis::fileManager::instance().flushFile(fname);
    if (0 == remove(fname)) {
        if (ibis::gVerbose > 6)
            logMessage("clear", "removed %s", fname);
    }
    else if (errno != ENOENT || ibis::gVerbose > 7)
        logMessage("clear", "failed to remove %s ... %s", fname,
                   strerror(errno));

    strcpy(fname+len, "-rids");
    ibis::fileManager::instance().flushFile(fname);
    if (0 == remove(fname)) {
        if (ibis::gVerbose > 6)
            logMessage("clear", "removed %s", fname);
    }
    else if (errno != ENOENT || ibis::gVerbose > 7)
        logMessage("clear", "failed to remove %s ... %s", fname,
                   strerror(errno));

    strcpy(fname+len, "fids");
    ibis::fileManager::instance().flushFile(fname);
    if (0 == remove(fname)) {
        if (ibis::gVerbose > 6)
            logMessage("clear", "removed %s", fname);
    }
    else if (errno != ENOENT || ibis::gVerbose > 7)
        logMessage("clear", "failed to remove %s ... %s", fname,
                   strerror(errno));

    strcpy(fname+len, "bundles");
    ibis::fileManager::instance().flushFile(fname);
    if (0 == remove(fname)) {
        if (ibis::gVerbose > 6)
            logMessage("clear", "removed %s", fname);
    }
    else if (errno != ENOENT || ibis::gVerbose > 7)
        logMessage("clear", "failed to remove %s ... %s", fname,
                   strerror(errno));
    delete [] fname;
} // ibis::query::removeFiles

void ibis::query::printSelected(std::ostream& out) const {
    if (comps.empty()) return;
    if (state == FULL_EVALUATE || state == BUNDLES_TRUNCATED ||
        state == HITS_TRUNCATED) {
        ibis::bundle* bdl = 0;
        if (hits != 0)
            if (hits->cnt() > 0)
                bdl = ibis::bundle::create(*this);
        if (bdl != 0) {
            bdl->print(out);
            bdl->write(*this);
            delete bdl;
        }
        else {
            logWarning("printSelected", "failed to construct ibis::bundle");
        }
    }
    else {
        logWarning("printSelected", "must perform full estimate before "
                   "calling this function");
    }
} // ibis::query::printSelected

void ibis::query::printSelectedWithRID(std::ostream& out) const {
    if (state == FULL_EVALUATE || state == BUNDLES_TRUNCATED ||
        state == HITS_TRUNCATED) {
        ibis::bundle* bdl = 0;
        if (hits != 0)
            if (hits->cnt() > 0)
                bdl = ibis::bundle::create(*this);
        if (bdl != 0) {
            bdl->printAll(out);
            bdl->write(*this);
            delete bdl;
        }
        else {
            logWarning("printSelectedWithRID",
                       "failed to construct ibis::bundle");
        }
    }
    else {
        logWarning("printSelectedWithRID", "must perform full estimate "
                   "before calling this function");
    }
} // ibis::query::printSelectedWithRID

uint32_t ibis::query::countPages(unsigned wordsize) const {
    uint32_t res = 0;
    if (hits == 0)
        return res;
    if (hits->cnt() == 0)
        return res;
    if (wordsize == 0)
        return res;

    // words per page
    const uint32_t wpp = ibis::fileManager::pageSize() / wordsize;
    uint32_t last;  // the position of the last entry encountered
    ibis::bitvector::indexSet ix = hits->firstIndexSet();
    last = *(ix.indices());
    if (ibis::gVerbose < 8) {
        while (ix.nIndices() > 0) {
            const ibis::bitvector::word_t *ind = ix.indices();
            const uint32_t p0 = *ind / wpp;
            res += (last < p0*wpp); // last not on the current page
            if (ix.isRange()) {
                res += (ind[1] / wpp - p0);
                last = ind[1];
            }
            else {
                last = ind[ix.nIndices()-1];
                res += (last / wpp > p0);
            }
            ++ ix;
        }
    }
    else {
        ibis::util::logger lg;
        lg() << "query[" << myID << "]::countPages(" << wordsize
             << ") page numbers: ";
        for (uint32_t i = 0; ix.nIndices() > 0 && (i >> ibis::gVerbose) == 0;
             ++ i) {
            const ibis::bitvector::word_t *ind = ix.indices();
            const uint32_t p0 = *ind / wpp;
            if (last < p0*wpp) { // last not on the current page
                lg() << last/wpp << " ";
                ++ res;
            }
            if (ix.isRange()) {
                const unsigned mp = (ind[1]/wpp - p0);
                if (mp > 1) {
                    lg() << p0 << "*" << mp << " ";
                }
                else if (mp > 0) {
                    lg() << p0 << " ";
                }
                res += mp;
                last = ind[1];
            }
            else {
                last = ind[ix.nIndices()-1];
                if (last / wpp > p0) {
                    lg() << p0 << " ";
                    ++ res;
                }
            }
            ++ ix;
        }
        if (ix.nIndices() > 0)
            lg() << " ...";
    }
    return res;
} // ibis::query::countPages

int ibis::query::doExpand(ibis::qExpr* exp0) const {
    int ret = 0;
    switch (exp0->getType()) {
    case ibis::qExpr::LOGICAL_AND:
    case ibis::qExpr::LOGICAL_OR:
    case ibis::qExpr::LOGICAL_XOR: { // binary operators
        ret = doExpand(exp0->getLeft());
        ret += doExpand(exp0->getRight());
        break;
    }
    case ibis::qExpr::LOGICAL_NOT: { // negation operator
        ret = doContract(exp0->getLeft());
        break;
    }
    case ibis::qExpr::RANGE: { // a range condition
        ibis::qContinuousRange* range =
            reinterpret_cast<ibis::qContinuousRange*>(exp0);
        ibis::column* col = mypart->getColumn(range->colName());
        ret = col->expandRange(*range);
        break;
    }
    default:
        break;
    }
    return ret;
} // ibis::query::doExpand

int ibis::query::doContract(ibis::qExpr* exp0) const {
    int ret = 0;
    switch (exp0->getType()) {
    case ibis::qExpr::LOGICAL_AND:
    case ibis::qExpr::LOGICAL_OR:
    case ibis::qExpr::LOGICAL_XOR: { // binary operators
        ret = doContract(exp0->getLeft());
        ret += doContract(exp0->getRight());
        break;
    }
    case ibis::qExpr::LOGICAL_NOT: { // negation operator
        ret = doExpand(exp0->getLeft());
        break;
    }
    case ibis::qExpr::RANGE: { // a range condition
        ibis::qContinuousRange* range =
            reinterpret_cast<ibis::qContinuousRange*>(exp0);
        ibis::column* col = mypart->getColumn(range->colName());
        ret = col->contractRange(*range);
        break;
    }
    default:
        break;
    }
    return ret;
} // ibis::query::doContract

/// Process the join operation and return the number of pairs.
/// This function only counts the number of hits; it does produce the
/// actual tuples for the results of join.  Additionally, it performs only
/// self-join, i.e., join a partition with itself.  This is only meant to
/// test some algorithms for evaluating joins.
int64_t ibis::query::processJoin() {
    int64_t ret = 0;
    if (conds.empty()) return ret;
    //     if (state != ibis::query::FULL_EVALUATE ||
    //  timestamp() != partition()->timestamp())
    //  evaluate();
    if (hits == 0 || hits->cnt() == 0) return ret; // no hits

    ibis::horometer timer;
    std::vector<const ibis::deprecatedJoin*> terms;

    // extract all deprecatedJoin objects from the root of the expression
    // tree to the first operator that is not AND
    conds->extractDeprecatedJoins(terms);
    if (terms.empty())
        return ret;

    // put those can be evaluated with indices at the end of the list
    uint32_t ii = 0;
    uint32_t jj = terms.size() - 1;
    while (ii < jj) {
        if (terms[jj]->getRange() == 0)
            -- jj;
        else if (terms[jj]->getRange()->termType() ==
                 ibis::math::NUMBER)
            -- jj;
        else {
            ibis::math::barrel baj(terms[jj]->getRange());
            if (baj.size() == 0 ||
                (baj.size() == 1 &&
                 0 == stricmp(baj.name(0), terms[jj]->getName1()))) {
                -- jj;
            }
            else if (terms[ii]->getRange() != 0 &&
                     terms[ii]->getRange()->termType() !=
                     ibis::math::NUMBER) {
                ibis::math::barrel bai(terms[ii]->getRange());
                if (bai.size() > 1 ||
                    (bai.size() == 1 &&
                     0 != stricmp(bai.name(0), terms[ii]->getName1()))) {
                    ++ ii;
                }
                else { // swap
                    const ibis::deprecatedJoin *tmp = terms[ii];
                    terms[ii] = terms[jj];
                    terms[jj] = tmp;
                    ++ ii;
                    -- jj;
                }
            }
            else { // swap
                const ibis::deprecatedJoin *tmp = terms[ii];
                terms[ii] = terms[jj];
                terms[jj] = tmp;
                ++ ii;
                -- jj;
            }
        }
    }

    const ibis::bitvector64::word_t npairs =
        static_cast<ibis::bitvector64::word_t>(mypart->nRows()) *
        mypart->nRows();
    // retrieve two column pointers for future operations
    const ibis::column *col1 = mypart->getColumn(terms.back()->getName1());
    const ibis::column *col2 = mypart->getColumn(terms.back()->getName2());
    while ((col1 == 0 || col2 == 0) && terms.size() > 0) {
        std::ostringstream ostr;
        ostr << *(terms.back());
        logWarning("processJoin", "either %s or %s from partition %s is not a "
                   "valid column name in partition %s",
                   terms.back()->getName1(), terms.back()->getName2(),
                   ostr.str().c_str(), mypart->name());
        terms.resize(terms.size()-1); // remove the invalid term
        if (terms.size() > 0) {
            col1 = mypart->getColumn(terms.back()->getName1());
            col2 = mypart->getColumn(terms.back()->getName2());
        }
    }
    if (terms.empty()) {
        logWarning("processJoin", "nothing left in the std::vector terms");
        ret = -1;
        return ret;
    }
    std::ostringstream outstr;
    outstr << "processed (" << *(terms.back());
    for (ii = 1; ii < terms.size(); ++ ii)
        outstr << " AND " << *(terms[ii]);

    ibis::bitvector mask;
    // generate a mask
    col1->getNullMask(mask);
    if (col2 != col1) {
        ibis::bitvector tmp;
        col2->getNullMask(tmp);
        mask &= tmp;
    }
    if (sup != 0 && sup->cnt() > hits->cnt()) {
        mask &= *sup;
    }
    else {
        mask &= *hits;
    }

    int64_t cnt = 0;
    { // OPTION 0 -- directly read the values
        ibis::horometer watch;
        watch.start();
        cnt = mypart->evaluateJoin(terms, mask);
        watch.stop();
        //logMessage("processJoin", "OPTION 0 -- loop join computes %lld "
        //         "hits in %g seconds", cnt, watch.realTime());
        if (cnt >= 0) {
            LOGGER(ibis::gVerbose >= 0)
                << "processJoin with OPTION 0 -- loop join computed "
                << cnt << " hits, took " << watch.realTime() << " sec";
        }
    }
    { // OPTION 1 -- sort-merge join
        ibis::horometer watch;
        watch.start();
        cnt = sortJoin(terms, mask);
        watch.stop();
        LOGGER(ibis::gVerbose >= 0)
            << "processJoin with OPTION 1 -- sort-merge join computed "
            << cnt << " hits, took " << watch.realTime() << " sec";
    }

    ibis::column::indexLock idy1(col1, "processJoin");
    ibis::column::indexLock idy2(col2, "processJoin");
    const ibis::index *idx1 = idy1.getIndex();
    const ibis::index *idx2 = idy2.getIndex();
    const ibis::qRange *range1 = conds->findRange(col1->name());
    const ibis::qRange *range2 = conds->findRange(col2->name());
    if (idx1 != 0 && idx2 != 0 && terms.size() == 1 &&
        idx1->type() == ibis::index::RELIC &&
        idx2->type() == ibis::index::RELIC) {
        // OPTION 2 -- using relic indexes to count the number of hits only
        ibis::horometer tm1, tm2;
        tm1.start();
        int64_t cnt2 = reinterpret_cast<const ibis::relic*>(idx1)->
            estimate(*reinterpret_cast<const ibis::relic*>(idx2),
                     *(terms.back()), mask);
        tm1.stop();
        // OPTION 3 -- use relic indexes to count the number of hits only
        tm2.start();
        int64_t cnt3 = reinterpret_cast<const ibis::relic*>(idx1)->
            estimate(*reinterpret_cast<const ibis::relic*>(idx2),
                     *(terms.back()), mask, range1, range2);
        tm2.stop();

        ibis::util::logger lg;
        lg() << "processJoin with OPTION 2 -- basic bitmap index + "
            "bitmap mask ";
        if (terms.size() == 1)
            lg() << "computed ";
        else
            lg() << "estimated (baed on " << *(terms.back())
                 << ") to be no more than ";
        lg() << cnt2 << " hits, took " << tm1.realTime() << " sec\n"
             << "processJoin with OPTION 3 -- basic bitmap index + "
            "bitmap mask and ";
        if (range1) {
            if (range2) {
                lg() << "two range constraints (" << *range1
                     << " and " << *range2 << ")";
            }
            else {
                lg() << "one range constraint (" << *range1 << ")";
            }
        }
        else if (range2) {
            lg() << "one range constraint (" << *range2 << ")";
        }
        else {
            lg() << "no range constraint";
        }
        if (terms.size() == 1)
            lg() << " computed ";
        else
            lg() << "estimated (baed on " << *(terms.back())
                 << ") to be no more than ";
        lg() << cnt3 << " hits, took " << tm2.realTime() << " sec";
    }
    if (idx1 != 0 && idx2 != 0 && terms.size() > 1 &&
        idx1->type() == ibis::index::RELIC &&
        idx2->type() == ibis::index::RELIC) {
        // OPTION 2 -- using relic indexes to count the number of hits only
        // multiple join operators
        ibis::horometer tm1, tm2;
        ibis::bitvector64 low, high;
        bool approx2 = false;
        tm1.start();
        reinterpret_cast<const ibis::relic*>(idx1)->
            estimate(*reinterpret_cast<const ibis::relic*>(idx2),
                     *(terms.back()), mask, low, high);
        if (high.size() != low.size()) // more important to keep high
            high.swap(low);
        for (int i = terms.size() - 1; i > 0 && low.cnt()>0;) {
            -- i;
            const char *name1 = terms[i]->getName1();
            const ibis::column *c1 = mypart->getColumn(name1);
            const char *name2 = terms[i]->getName2();
            const ibis::column *c2 = mypart->getColumn(name2);
            if (c1 == 0 || c2 == 0) {
                approx2 = true;
                break;
            }

            ibis::column::indexLock ilck1(c1, "processJoin");
            ibis::column::indexLock ilck2(c2, "processJoin");
            const ibis::index *ix1 = ilck1.getIndex();
            const ibis::index *ix2 = ilck2.getIndex();
            if (ix1 && ix2 && ix1->type() == ibis::index::RELIC &&
                ix2->type() == ibis::index::RELIC) {
                ibis::bitvector64 tmp;
                reinterpret_cast<const ibis::relic*>(ix1)->
                    estimate(*reinterpret_cast<const ibis::relic*>(ix2),
                             *(terms[i]), mask, tmp, high);
                if (tmp.cnt() > 0 && tmp.size() == low.size())
                    low &= tmp;
                else
                    low.clear();
            }
            else {
                approx2 = true;
                break;
            }
        }
        int64_t cnt2 = low.cnt();
        tm1.stop();
        // OPTION 3 -- use relic indexes to count the number of hits only
        // multiple join operators
        tm2.start();
        bool approx3 = false;
        reinterpret_cast<const ibis::relic*>(idx1)->
            estimate(*reinterpret_cast<const ibis::relic*>(idx2),
                     *(terms.back()), mask, range1, range2, low, high);
        if (high.size() != low.size()) // more important to keep high
            high.swap(low);
        for (int i = terms.size() - 1; i > 0 && low.cnt()>0;) {
            -- i;
            const char *name1 = terms[i]->getName1();
            const ibis::column *c1 = mypart->getColumn(name1);
            const char *name2 = terms[i]->getName2();
            const ibis::column *c2 = mypart->getColumn(name2);
            if (c1 == 0 || c2 == 0) {
                approx3 = true;
                break;
            }

            const ibis::qRange *r1 = conds->findRange(c1->name());
            const ibis::qRange *r2 = conds->findRange(c2->name());
            ibis::column::indexLock ilck1(c1, "processJoin");
            ibis::column::indexLock ilck2(c2, "processJoin");
            const ibis::index *ix1 = ilck1.getIndex();
            const ibis::index *ix2 = ilck2.getIndex();
            if (ix1 && ix2 && ix1->type() == ibis::index::RELIC &&
                ix2->type() == ibis::index::RELIC) {
                ibis::bitvector64 tmp;
                reinterpret_cast<const ibis::relic*>(ix1)->
                    estimate(*reinterpret_cast<const ibis::relic*>(ix2),
                             *(terms[i]), mask, r1, r2, tmp, high);
                if (tmp.cnt() > 0 && tmp.size() == low.size())
                    low &= tmp;
                else
                    low.clear();
            }
            else {
                approx3 = true;
                break;
            }
        }
        int64_t cnt3 = low.cnt();
        tm2.stop();

        ibis::util::logger lg;
        lg() << "processJoin with OPTION 2 -- basic bitmap index + "
            "bitmap mask ";
        if (approx2)
            lg() << "estimated to be no more than ";
        else
            lg() << "computed ";
        lg() << cnt2 << " hits, took "
             << tm1.realTime() << " sec\n"
             << "processJoin with OPTION 3 -- basic bitmap index + "
            "bitmap mask and additional range constraints";
        if (approx3)
            lg() << " estimated to be no more than ";
        else
            lg() << " computed ";
        lg() << cnt3 << " hits, took " << tm2.realTime() << " sec";
    }
    if (idx1 != 0 && idx2 != 0 && terms.size() == 1 &&
        idx1->type() == ibis::index::BINNING &&
        idx2->type() == ibis::index::BINNING) {
        // OPTION 2 -- using binned indexes to count the number of hits only
        ibis::horometer tm1, tm2;
        tm1.start();
        int64_t cnt2 = reinterpret_cast<const ibis::bin*>(idx1)->
            estimate(*reinterpret_cast<const ibis::bin*>(idx2),
                     *(terms.back()), mask);
        tm1.stop();
        // OPTION 3 -- use the simple bin indexes to count the number of hits
        tm2.start();
        int64_t cnt3 = reinterpret_cast<const ibis::bin*>(idx1)->
            estimate(*reinterpret_cast<const ibis::bin*>(idx2),
                     *(terms.back()), mask, range1, range2);
        tm2.stop();

        ibis::util::logger lg;
        lg() << "processJoin with OPTION 2 -- basic binned index + "
            "bitmap mask estimated the maximum hits to be " << cnt2
             << ", took " << tm1.realTime() << " sec\n"
             << "processJoin with OPTION 3 -- basic binned index + "
            "bitmap mask and additional range constraints"
            " estimated the maximum hits to be " << cnt3
             << ", took " << tm2.realTime() << " sec";
    }
    if (idx1 != 0 && idx2 != 0 && terms.size() > 1 &&
        idx1->type() == ibis::index::BINNING &&
        idx2->type() == ibis::index::BINNING) {
        // OPTION 2 -- using indexes to count the number of hits for
        // multiple join operators
        ibis::horometer tm1, tm2;
        ibis::bitvector64 low, high;
        tm1.start();
        reinterpret_cast<const ibis::bin*>(idx1)->
            estimate(*reinterpret_cast<const ibis::bin*>(idx2),
                     *(terms.back()), mask, low, high);
        if (high.size() != low.size()) // more important to keep high
            high.swap(low);
        for (int i = terms.size()-1; i > 0 && high.cnt() > 0;) {
            -- i;
            const char *name1 = terms[i]->getName1();
            const ibis::column *c1 = mypart->getColumn(name1);
            const char *name2 = terms[i]->getName2();
            const ibis::column *c2 = mypart->getColumn(name2);
            if (c1 == 0 || c2 == 0) {
                logWarning("processJoin", "either %s or %s is not a "
                           "column name", name1, name2);
            }

            ibis::column::indexLock ilck1(c1, "processJoin");
            ibis::column::indexLock ilck2(c2, "processJoin");
            const ibis::index *ix1 = ilck1.getIndex();
            const ibis::index *ix2 = ilck2.getIndex();
            if (ix1 && ix2 && ix1->type() == ibis::index::BINNING &&
                ix2->type() == ibis::index::BINNING) {
                ibis::bitvector64 tmp;
                reinterpret_cast<const ibis::bin*>(ix1)->
                    estimate(*reinterpret_cast<const ibis::bin*>(ix2),
                             *(terms[i]), mask, low, tmp);
                if (tmp.cnt() > 0) {
                    if (tmp.size() == high.size())
                        high &= tmp;
                    else
                        high &= low;
                }
                else {
                    high &= low;
                }
            }
            else {
                logWarning("processJoin", "either %s or %s has no binned "
                           "index", name1, name2);
            }
        }
        int64_t cnt2 = high.cnt();
        tm1.stop();
        low.clear();
        high.clear();
        // OPTION 3 -- use binned indexes to count the number of hits only,
        // multiple join operators
        tm2.start();
        reinterpret_cast<const ibis::bin*>(idx1)->
            estimate(*reinterpret_cast<const ibis::bin*>(idx2),
                     *(terms.back()), mask, range1, range2, low, high);
        if (high.size() != low.size()) // more important to keep high
            high.swap(low);
        for (int i = terms.size()-1; i > 0 && high.cnt() > 0;) {
            -- i;
            const char *name1 = terms[i]->getName1();
            const ibis::column *c1 = mypart->getColumn(name1);
            const char *name2 = terms[i]->getName2();
            const ibis::column *c2 = mypart->getColumn(name2);
            if (c1 == 0 || c2 == 0) {
                logWarning("processJoin", "either %s or %s is not a "
                           "column name", name1, name2);
            }

            const ibis::qRange *r1 = conds->findRange(c1->name());
            const ibis::qRange *r2 = conds->findRange(c2->name());
            ibis::column::indexLock ilck1(c1, "processJoin");
            ibis::column::indexLock ilck2(c2, "processJoin");
            const ibis::index *ix1 = ilck1.getIndex();
            const ibis::index *ix2 = ilck2.getIndex();
            if (ix1 && ix2 && ix1->type() == ibis::index::BINNING &&
                ix2->type() == ibis::index::BINNING) {
                ibis::bitvector64 tmp;
                reinterpret_cast<const ibis::bin*>(ix1)->
                    estimate(*reinterpret_cast<const ibis::bin*>(ix2),
                             *(terms[i]), mask, r1, r2, low, tmp);
                if (tmp.cnt() > 0) {
                    if (tmp.size() == high.size())
                        high &= tmp;
                    else
                        high &= low;
                }
                else {
                    high &= low;
                }
            }
            else {
                logWarning("processJoin", "either %s or %s has no binned "
                           "index", name1, name2);
            }
        }
        int64_t cnt3 = high.cnt();
        tm2.stop();

        ibis::util::logger lg;
        lg() << "processJoin with OPTION 2 -- basic binned index + "
            "bitmap mask estimated the maximum hits to be " << cnt2
             << ", took " << tm1.realTime() << " sec\n"
             << "processJoin with OPTION 3 -- basic binned index + "
            "bitmap mask and ";
        if (range1) {
            if (range2) {
                lg() << "two range constraints (" << *range1
                     << " and " << *range2 << ")";
            }
            else {
                lg() << "one range constraint (" << *range1 << ")";
            }
        }
        else if (range2) {
            lg() << "one range constraint (" << *range2 << ")";
        }
        else {
            lg() << "no range constraint";
        }
        lg() << " estimated the maximum hits to be " << cnt3;
        if (terms.size() > 1)
            lg() << " (based on " << *(terms.back()) << ")";
        lg() << ", took " << tm2.realTime() << " sec";
    }

    bool symm = false;
    { // use a block to limit the scopes of the two barrel variables
        ibis::math::barrel bar1, bar2;
        for (uint32_t i = 0; i < terms.size(); ++ i) {
            bar1.recordVariable(terms[i]->getName1());
            bar1.recordVariable(terms[i]->getRange());
            bar2.recordVariable(terms[i]->getName2());
        }
        symm = bar1.equivalent(bar2);
    }

    // OPTION 4 -- the intended main option that combines the index
    // operations with brute-fore scans.  Since it uses a large bitvector64
    // as a mask, we need to make sure it can be safely generated in
    // memory.
    {
        double cf = ibis::bitvector::clusteringFactor
            (mask.size(), mask.cnt(), mask.bytes());
        uint64_t mb = mask.cnt();
        double bv64size = 8*ibis::bitvector64::markovSize
            (npairs, mb*mb, cf);
        if (bv64size > 2.0*ibis::fileManager::bytesFree() ||
            bv64size > ibis::fileManager::bytesFree() +
            ibis::fileManager::bytesInUse()) {
            logWarning("processJoin", "the solution vector for a join of "
                       "%lu x %lu (out of %lu x %lu) is expected to take %g "
                       "bytes and can not be fit into available memory",
                       static_cast<long unsigned>(mask.cnt()),
                       static_cast<long unsigned>(mask.cnt()),
                       static_cast<long unsigned>(mask.size()),
                       static_cast<long unsigned>(mask.size()),
                       bv64size);
            return cnt;
        }
    }

    timer.start();
    uint64_t estimated = 0;
    ibis::bitvector64 surepairs, iffypairs;
    if (terms.size() == 1) {
        if (idx1 != 0 && idx2 != 0 &&
            idx1->type() == ibis::index::RELIC &&
            idx2->type() == ibis::index::RELIC) {
            reinterpret_cast<const ibis::relic*>(idx1)->
                estimate(*reinterpret_cast<const ibis::relic*>(idx2),
                         *(terms.back()), mask, range1, range2,
                         surepairs, iffypairs);
        }
        else if (idx1 != 0 && idx2 != 0 &&
                 idx1->type() == ibis::index::BINNING &&
                 idx2->type() == ibis::index::BINNING) {
            if (symm)
                reinterpret_cast<const ibis::bin*>(idx1)->estimate
                    (*(terms.back()), mask, range1, range2,
                     surepairs, iffypairs);
            else
                reinterpret_cast<const ibis::bin*>(idx1)->estimate
                    (*reinterpret_cast<const ibis::bin*>(idx2),
                     *(terms.back()), mask, range1, range2,
                     surepairs, iffypairs);
        }
        else {
            surepairs.set(0, npairs);
            iffypairs.set(1, npairs);
        }

        if (iffypairs.size() != npairs)
            iffypairs.set(0, npairs);
        if (surepairs.size() != npairs)
            surepairs.set(0, npairs);
        estimated = iffypairs.cnt();
        if (surepairs.cnt() > 0 || iffypairs.cnt() > 0) {
            ibis::bitvector64 tmp;
            ibis::util::outerProduct(mask, mask, tmp);
            //    std::cout << "TEMP surepairs.size() = " << surepairs.size()
            //        << " surepairs.cnt() = " << surepairs.cnt() << std::endl;
            //    std::cout << "TEMP iffypairs.size() = " << iffypairs.size()
            //        << " iffypairs.cnt() = " << iffypairs.cnt() << std::endl;
            //    std::cout << "TEMP tmp.size() = " << tmp.size()
            //        << ", tmp.cnt() = " << tmp.cnt() << std::endl;
            surepairs &= tmp;
            iffypairs &= tmp;
            iffypairs -= surepairs;
#if DEBUG+0 > 2 || _DEBUG+0 > 2
            if (surepairs.cnt() > 0) { // verify the pairs in surepairs
                int64_t ct1 = mypart->evaluateJoin(*(terms.back()),
                                                   surepairs, tmp);
                if (ct1 > 0 && tmp.size() == surepairs.size())
                    tmp ^= surepairs;
                if (tmp.cnt() != 0) {
                    std::ostringstream ostr;
                    ostr << tmp.cnt();
                    logWarning("processJoin", "some (%s) surepairs are "
                               "not correct", ostr.str().c_str());
                    const unsigned nrows = mypart->nRows();
                    ibis::util::logger lg(4);
                    for (ibis::bitvector64::indexSet ix =
                             tmp.firstIndexSet();
                         ix.nIndices() > 0; ++ ix) {
                        const ibis::bitvector64::word_t *ind = ix.indices();
                        if (ix.isRange()) {
                            for (ibis::bitvector64::word_t i = *ind;
                                 i < ind[1]; ++ i)
                                lg() << i/nrows << ",\t" << i%nrows << "\n";
                        }
                        else {
                            for (uint32_t i = 0; i < ix.nIndices(); ++ i)
                                lg() << ind[i]/nrows << ",\t"
                                     << ind[i]%nrows << "\n";
                        }
                    }
                }
            }
#endif
            if (iffypairs.cnt() <
                static_cast<uint64_t>(mask.cnt())*mask.cnt()) {
                int64_t ct2 = mypart->evaluateJoin(*(terms.back()),
                                                   iffypairs, tmp);
                if (ct2 > 0 && tmp.size() == surepairs.size())
                    surepairs |= tmp;
#if DEBUG+0 > 1 || _DEBUG+0 > 1
                ct2 = mypart->evaluateJoin(*(terms.back()), mask, iffypairs);
                if (ct2 > 0 && iffypairs.size() == surepairs.size())
                    iffypairs ^= surepairs;
                if (iffypairs.cnt() == 0) {
                    logMessage("processJoin", "bruteforce scan produced "
                               "the same results as indexed scan");
                }
                else {
                    std::ostringstream ostr;
                    ostr << iffypairs.cnt();
                    logWarning("processJoin", "bruteforce scan produced %s "
                               "different results from the indexed scan",
                               ostr.str().c_str());
                    const unsigned nrows = mypart->nRows();
                    ibis::util::logger lg(4);
                    for (ibis::bitvector64::indexSet ix =
                             iffypairs.firstIndexSet();
                         ix.nIndices() > 0; ++ ix) {
                        const ibis::bitvector64::word_t *ind = ix.indices();
                        if (ix.isRange()) {
                            for (ibis::bitvector64::word_t i = *ind;
                                 i < ind[1]; ++ i)
                                lg() << i/nrows << ",\t" << i%nrows << "\n";
                        }
                        else {
                            for (uint32_t i = 0; i < ix.nIndices(); ++ i)
                                lg() << ind[i]/nrows << ",\t"
                                     << ind[i]%nrows << "\n";
                        }
                    }
                }
#endif
            }
            else {
                mypart->evaluateJoin(*(terms.back()), mask, surepairs);
            }
        }
    }
    else { // more than one join term to be processed
        if (idx1 != 0 && idx2 != 0 &&
            idx1->type() == ibis::index::BINNING &&
            idx2->type() == ibis::index::BINNING) {
            if (symm)
                reinterpret_cast<const ibis::bin*>(idx1)->estimate
                    (*(terms.back()), mask, range1, range2,
                     surepairs, iffypairs);
            else
                reinterpret_cast<const ibis::bin*>(idx1)->estimate
                    (*reinterpret_cast<const ibis::bin*>(idx2),
                     *(terms.back()), mask, range1, range2,
                     surepairs, iffypairs);
        }
        else if (idx1 != 0 && idx2 != 0 &&
                 idx1->type() == ibis::index::RELIC &&
                 idx2->type() == ibis::index::RELIC) {
            reinterpret_cast<const ibis::relic*>(idx1)->
                estimate(*reinterpret_cast<const ibis::relic*>(idx2),
                         *(terms.back()), mask, range1, range2,
                         surepairs, iffypairs);
        }
        else {
            surepairs.set(0, npairs);
            ibis::util::outerProduct(mask, mask, iffypairs);
        }

        if (iffypairs.size() != npairs)
            iffypairs.set(0, npairs);
        if (surepairs.size() != npairs)
            surepairs.set(0, npairs);
        iffypairs |= surepairs;
        estimated = iffypairs.cnt();
        if (iffypairs.cnt() < static_cast<uint64_t>(mask.cnt())*mask.cnt()) {
            if (iffypairs.cnt() == surepairs.cnt())
                // the last term has been evaluated accurately, remove it
                terms.resize(terms.size() - 1);
            int64_t ct4 = mypart->evaluateJoin(terms, iffypairs, surepairs);
            if (ct4 < 0) {
                logWarning("processJoin",
                           "evaluateJoin failed with error code %ld",
                           static_cast<long int>(ct4));
            }
#if DEBUG+0 > 1 || _DEBUG+0 > 1
            ct4 = mypart->evaluateJoin(terms, mask, iffypairs);
            if (ct4 > 0 && iffypairs.size() == surepairs.size())
                iffypairs ^= surepairs;
            if (iffypairs.cnt() == 0) {
                logMessage("processJoin", "verified the indexed scan "
                           "results with bruteforce scan");
            }
            else {
                std::ostringstream ostr;
                ostr << iffypairs.cnt();
                logWarning("processJoin", "bruteforce scan produced %s "
                           "different results from the indexed scan",
                           ostr.str().c_str());
                const unsigned nrows = mypart->nRows();
                ibis::util::logger lg(4);
                for (ibis::bitvector64::indexSet ix =
                         iffypairs.firstIndexSet();
                     ix.nIndices() > 0; ++ ix) {
                    const ibis::bitvector64::word_t *ind = ix.indices();
                    if (ix.isRange()) {
                        for (ibis::bitvector64::word_t i = *ind;
                             i < ind[1]; ++ i)
                            lg() << i/nrows << ",\t" << i%nrows << "\n";
                    }
                    else {
                        for (uint32_t i = 0; i < ix.nIndices(); ++ i)
                            lg() << ind[i]/nrows << ",\t"
                                 << ind[i]%nrows << "\n";
                    }
                }
            }
#endif
        }
        else {
            mypart->evaluateJoin(terms, mask, surepairs);
        }
    }

    ret = surepairs.cnt();
    timer.stop();
    //     logMessage("processJoin", "OPTION 4 -- indexed join computes %lld "
    //         "hits in %g seconds", ret, timer.realTime());
    //     logMessage("processJoin", "OPTION 4 -- indexed join computes %lld "
    //         "hits in %g seconds", ret, timer.realTime());
    LOGGER(ibis::gVerbose >= 0)
        << "processJoin with OPTION 4 -- index scan (estimated <= "
        << estimated << ") followed by pair-masked loop join computed "
        << ret << (ret > 1 ? " hits" : " hit") << ", took "
        << timer.realTime() << " sec";

    if (cnt == ret) {
        if (ibis::gVerbose > 4)
            logMessage("processJoin", "merge join algorithm produced the "
                       "same number of hits as the indexed/sequential scan");
    }
    else {
        std::ostringstream o2;
        o2 << cnt << " hit" << (cnt>1?"s":"") << " rather than " << ret
           << " as produced from the indexed/sequential scan";
        logWarning("processJoin", "merge join algorithm produced %s",
                   o2.str().c_str());
    }

    if (ibis::gVerbose > 0) {
        outstr << "), got " << ret << (ret > 1 ? " hits" : " hit");
        logMessage("processJoin", "%s, used %g sec(CPU), %g sec(elapsed)",
                   outstr.str().c_str(), timer.CPUTime(), timer.realTime());
    }
    return ret;
} // ibis::query::processJoin

// The merge sort join algorithm.
int64_t ibis::query::sortJoin(const ibis::deprecatedJoin& cmp,
                              const ibis::bitvector& mask) const {
    int64_t cnt = 0;
    if (cmp.getRange() == 0)
        cnt = sortEquiJoin(cmp, mask);
    else if (cmp.getRange()->termType() == ibis::math::NUMBER) {
        const double delta = fabs(cmp.getRange()->eval());
        if (delta > 0)
            cnt = sortRangeJoin(cmp, mask);
        else
            cnt = sortEquiJoin(cmp, mask);
    }
    else {
        ibis::math::barrel bar(cmp.getRange());
        if (bar.size() == 0) {
            const double delta = fabs(cmp.getRange()->eval());
            if (delta > 0)
                cnt = sortRangeJoin(cmp, mask);
            else
                cnt = sortEquiJoin(cmp, mask);
        }
        else {
            cnt = mypart->evaluateJoin(cmp, mask);
        }
    }
    return cnt;
} // ibis::query::sortJoin

int64_t
ibis::query::sortJoin(const std::vector<const ibis::deprecatedJoin*>& terms,
                      const ibis::bitvector& mask) const {
    if (terms.size() > 1) {
        if (myDir == 0) {
            logWarning("sortJoin", "failed to create a directory to store "
                       "temporary files needed for the sort-merge join "
                       "algorithm.  Use loop join instead.");
            return mypart->evaluateJoin(terms, mask);
        }

        int64_t cnt = mask.cnt();
        for (uint32_t i = 0; i < terms.size() && cnt > 0; ++ i) {
            std::string pairfile = myDir;
            pairfile += terms[i]->getName1();
            pairfile += '-';
            pairfile += terms[i]->getName2();
            pairfile += ".pairs";
            if (terms[i]->getRange() == 0) {
                sortEquiJoin(*(terms[i]), mask, pairfile.c_str());
            }
            else if (terms[i]->getRange()->termType() ==
                     ibis::math::NUMBER) {
                const double delta = fabs(terms[i]->getRange()->eval());
                if (delta > 0)
                    sortRangeJoin(*(terms[i]), mask, pairfile.c_str());
                else
                    sortEquiJoin(*(terms[i]), mask, pairfile.c_str());
            }
            else {
                ibis::math::barrel bar(terms[i]->getRange());
                if (bar.size() == 0) {
                    const double delta = fabs(terms[i]->getRange()->eval());
                    if (delta > 0)
                        sortRangeJoin(*(terms[i]), mask, pairfile.c_str());
                    else
                        sortEquiJoin(*(terms[i]), mask, pairfile.c_str());
                }
                else {
                    mypart->evaluateJoin(*(terms[i]), mask,
                                         pairfile.c_str());
                }
            }

            // sort the newly generated pairs
            orderPairs(pairfile.c_str());
            // merge the sorted pairs with the existing list
            cnt = mergePairs(pairfile.c_str());
        }
        return cnt;
    }
    else if (terms.size() == 1) {
        return sortJoin(*(terms.back()), mask);
    }
    else {
        return 0;
    }
} // ibis::query::sortJoin

/// Assume the two input arrays are sorted in ascending order, count the
/// number of elements that match.  Note that both template arguments
/// should be elemental types or they must support operators == and < with
/// mixed types.
template <typename T1, typename T2>
int64_t ibis::query::countEqualPairs(const array_t<T1>& val1,
                                     const array_t<T2>& val2) const {
    int64_t cnt = 0;
    uint32_t i1 = 0, i2 = 0;
    const uint32_t n1 = val1.size();
    const uint32_t n2 = val2.size();
    while (i1 < n1 && i2 < n2) {
        uint32_t j1, j2;
        if (val1[i1] < val2[i2]) { // move i1 to catch up
            for (++ i1; i1 < n1 && val1[i1] < val2[i2]; ++ i1);
        }
        if (val2[i2] < val1[i1]) { // move i2 to catch up
            for (++ i2; i2 < n2 && val2[i2] < val1[i1]; ++ i2);
        }
        if (i1 < n1 && i2 < n2 && val1[i1] == val2[i2]) {
            // found two equal values
            // next, find out how many values are equal
            for (j1 = i1+1; j1 < n1 && val1[j1] == val1[i1]; ++ j1);
            for (j2 = i2+1; j2 < n2 && val2[i2] == val2[j2]; ++ j2);
#if defined(DEBUG) || defined(_DEBUG)
            LOGGER(ibis::gVerbose > 5)
                << "DEBUG -- query::countEqualPairs found "
                << "val1[" << i1 << ":" << j1 << "] (" << val1[i1]
                << ") equals to val2[" << i2 << ":" << j2
                << "] (" << val2[i2] << ")";
#endif
            cnt += (j1 - i1) * (j2 - i2);
            i1 = j1;
            i2 = j2;
        }
    } // while (i1 < n1 && i2 < n2)
    return cnt;
} // ibis::query::countEqualPairs

// two specialization for comparing signed and unsigned integers.
template <>
int64_t ibis::query::countEqualPairs(const array_t<int32_t>& val1,
                                     const array_t<uint32_t>& val2) const {
    int64_t cnt = 0;
    uint32_t i1 = val1.find(val2.front()); // position of the first value >= 0
    uint32_t i2 = 0;
    const uint32_t n1 = val1.size();
    const uint32_t n2 = val2.find(val1.back()+1U);
    while (i1 < n1 && i2 < n2) {
        uint32_t j1, j2;
        if (static_cast<unsigned>(val1[i1]) < val2[i2]) {
            // move i1 to catch up
            for (++ i1;
                 i1 < n1 && static_cast<unsigned>(val1[i1]) < val2[i2];
                 ++ i1);
        }
        if (val2[i2] < static_cast<unsigned>(val1[i1])) {
            // move i2 to catch up
            for (++ i2;
                 i2 < n2 && val2[i2] < static_cast<unsigned>(val1[i1]);
                 ++ i2);
        }
        if (i1 < n1 && i2 < n2 &&
            static_cast<unsigned>(val1[i1]) == val2[i2]) {
            // found two equal values
            // next, find out how many values are equal
            for (j1 = i1+1; j1 < n1 && val1[j1] == val1[i1]; ++ j1);
            for (j2 = i2+1; j2 < n2 && val2[i2] == val2[j2]; ++ j2);
            cnt += (j1 - i1) * (j2 - i2);
            i1 = j1;
            i2 = j2;
        }
    } // while (i1 < n1 && i2 < n2)
    return cnt;
} // ibis::query::countEqualPairs

template <>
int64_t ibis::query::countEqualPairs(const array_t<uint32_t>& val1,
                                     const array_t<int32_t>& val2) const {
    int64_t cnt = 0;
    uint32_t i1 = 0, i2 = val2.find(val1.front());
    const uint32_t n1 = val1.find(val2.back()+1U);
    const uint32_t n2 = val2.size();
    while (i1 < n1 && i2 < n2) {
        uint32_t j1, j2;
        if (val1[i1] < static_cast<unsigned>(val2[i2])) {
            // move i1 to catch up
            for (++ i1;
                 i1 < n1 && val1[i1] < static_cast<unsigned>(val2[i2]);
                 ++ i1);
        }
        if (static_cast<unsigned>(val2[i2]) < val1[i1]) {
            // move i2 to catch up
            for (++ i2;
                 i2 < n2 && static_cast<unsigned>(val2[i2]) < val1[i1];
                 ++ i2);
        }
        if (i1 < n1 && i2 < n2 &&
            val1[i1] == static_cast<unsigned>(val2[i2])) {
            // found two equal values
            // next, find out how many values are equal
            for (j1 = i1+1; j1 < n1 && val1[j1] == val1[i1]; ++ j1);
            for (j2 = i2+1; j2 < n2 && val2[i2] == val2[j2]; ++ j2);
#if defined(DEBUG) || defined(_DEBUG)
            LOGGER(ibis::gVerbose > 5)
                << "DEBUG -- query::countEqualPairs found "
                << "val1[" << i1 << ":" << j1 << "] (" << val1[i1]
                << ") equals to val2[" << i2 << ":" << j2
                << "] (" << val2[i2] << ")";
#endif
            cnt += (j1 - i1) * (j2 - i2);
            i1 = j1;
            i2 = j2;
        }
    } // while (i1 < n1 && i2 < n2)
    return cnt;
} // ibis::query::countEqualPairs

/// Assume the two input arrays are sorted in ascending order, count the
/// number of elements that are with delta of each other.  Note that both
/// template arguments should be elemental types or they must support
/// operators -, +, == and < with mixed types.
template <typename T1, typename T2>
int64_t ibis::query::countDeltaPairs(const array_t<T1>& val1,
                                     const array_t<T2>& val2,
                                     const T1& delta) const {
    if (delta <= 0)
        return countEqualPairs(val1, val2);

    int64_t cnt = 0;
    uint32_t i1 = 0, i2 = 0;
    const uint32_t n1 = val1.size();
    for (uint32_t i = 0; i < val2.size() && i1 < n1; ++ i) {
        const T1 hi = static_cast<T1>(val2[i] + delta);
        // pressume integer underflow, set it to 0
        const T1 lo = (static_cast<T1>(val2[i] - delta)<hi ?
                       static_cast<T1>(val2[i] - delta) : 0);
        // move i1 to catch up with lo
        while (i1 < n1 && val1[i1] < lo)
            ++ i1;
        // move i2 to catch up with hi
        if (i1 > i2)
            i2 = i1;
        while (i2 < n1 && val1[i2] <= hi)
            ++ i2;
        cnt += i2 - i1;
    } // for ..
#if defined(DEBUG) || defined(_DEBUG)
    ibis::util::logger lg(4);
    lg() << "DEBUG -- countDeltaPairs val1=[";
    for (uint32_t ii = 0; ii < val1.size(); ++ ii)
        lg() << val1[ii] << ' ';
    lg() << "]\n" << "DEBUG -- countDeltaPairs val2=[";
    for (uint32_t ii = 0; ii < val2.size(); ++ ii)
        lg() << val2[ii] << ' ';
    lg() << "]\nDEBUG -- cnt=" << cnt;
#endif
    return cnt;
} // ibis::query::countDeltaPairs

template <>
int64_t ibis::query::countDeltaPairs(const array_t<uint32_t>& val1,
                                     const array_t<int32_t>& val2,
                                     const uint32_t& delta) const {
    int64_t cnt = 0;
    uint32_t i1 = 0, i2 = 0;
    const uint32_t n1 = val1.find(val2.back()+1U+delta);
    for (uint32_t i = val2.find(static_cast<int>(val1.front()-delta));
         i < val2.size() && i1 < n1; ++ i) {
        const unsigned lo = (static_cast<unsigned>(val2[i]) >= delta ?
                             val2[i] - delta : 0);
        const unsigned hi = val2[i] + delta;
        // move i1 to catch up with lo
        while (i1 < n1 && val1[i1] < lo)
            ++ i1;
        // move i2 to catch up with hi
        if (i1 > i2)
            i2 = i1;
        while (i2 < n1 && val1[i2] <= hi)
            ++ i2;
        cnt += i2 - i1;
#if defined(DEBUG) || defined(_DEBUG)
        LOGGER(ibis::gVerbose > 5)
            << "DEBUG -- query::countDeltaPairs found "
            << "val2[" << i << "] (" << val2[i]
            << ") in the range of val1[" << i1 << ":" << i2
            << "] (" << val1[i1] << " -- " << val1[i2] << ")";
#endif
    } // for ..
    return cnt;
} // ibis::query::countDeltaPairs

template <>
int64_t ibis::query::countDeltaPairs(const array_t<int32_t>& val1,
                                     const array_t<uint32_t>& val2,
                                     const int32_t& delta) const {
    if (delta <= 0)
        return countEqualPairs(val1, val2);

    int64_t cnt = 0;
    uint32_t i1 = val1.find(val2.front()-delta);
    uint32_t i2 = 0;
    const uint32_t n1 = val1.size();
    const uint32_t n2 = val2.find(UINT_MAX);
    for (uint32_t i = 0; i < n2 && i1 < n1; ++ i) {
        const int lo = val2[i] - delta;
        const int hi = val2[i] + delta;
        // move i1 to catch up with lo
        while (i1 < n1 && val1[i1] < lo)
            ++ i1;
        // move i2 to catch up with hi
        if (i1 > i2)
            i2 = i1;
        while (i2 < n1 && val1[i2] <= hi)
            ++ i2;
        cnt += i2 - i1;
    } // for ..
    return cnt;
} // ibis::query::countDeltaPairs

template <typename T1, typename T2>
int64_t ibis::query::recordEqualPairs(const array_t<T1>& val1,
                                      const array_t<T2>& val2,
                                      const array_t<uint32_t>& ind1,
                                      const array_t<uint32_t>& ind2,
                                      const char* filename) const {
    if (filename == 0 || *filename == 0)
        return countEqualPairs(val1, val2);
    int fdes = UnixOpen(filename, OPEN_WRITENEW, OPEN_FILEMODE);
    if (fdes < 0) {
        logWarning("recordEqualPairs",
                   "failed to open file \"%s\" for writing", filename);
        return countEqualPairs(val1, val2);
    }
#if defined(_WIN32) && defined(_MSC_VER)
    (void)_setmode(fdes, _O_BINARY);
#endif

    off_t ierr;
    int64_t cnt = 0;
    uint32_t idbuf[2];
    const uint32_t idsize = sizeof(idbuf);
    uint32_t i1 = 0, i2 = 0;
    const uint32_t n1 = val1.size();
    const uint32_t n2 = val2.size();
    while (i1 < n1 && i2 < n2) {
        uint32_t j1, j2;
        if (val1[i1] < val2[i2]) { // move i1 to catch up
            for (++ i1; i1 < n1 && val1[i1] < val2[i2]; ++ i1);
        }
        if (val2[i2] < val1[i1]) { // move i2 to catch up
            for (++ i2; i2 < n2 && val2[i2] < val1[i1]; ++ i2);
        }
        if (i1 < n1 && i2 < n2 && val1[i1] == val2[i2]) {
            // found two equal values
            // next, find out how many values are equal
            for (j1 = i1+1; j1 < n1 && val1[j1] == val1[i1]; ++ j1);
            for (j2 = i2+1; j2 < n2 && val2[i2] == val2[j2]; ++ j2);
            if (ind1.size() == val1.size() && ind2.size() == val2.size()) {
                for (uint32_t ii = i1; ii < j1; ++ ii) {
                    idbuf[0] = ind1[ii];
                    for (uint32_t jj = i2; jj < j2; ++ jj) {
                        idbuf[1] = ind2[jj];
                        ierr = UnixWrite(fdes, idbuf, idsize);
                        LOGGER(ibis::gVerbose > 0 &&
                               (ierr < 0 || (unsigned)ierr != idsize))
                            << "Warning -- query::recordEqualPairs failed to "
                            "write (" << idbuf[0] << ", " << idbuf[1]
                            << ") to " << filename;
                    }
                }
            }
            else {
                for (idbuf[0] = i1; idbuf[0] < j1; ++ idbuf[0])
                    for (idbuf[1] = i2; idbuf[1] < j2; ++ idbuf[1]) {
                        ierr = UnixWrite(fdes, idbuf, idsize);
                        LOGGER(ibis::gVerbose > 0 &&
                               (ierr < 0 || (unsigned)ierr != idsize))
                            << "Warning -- query::recordEqualPairs failed to "
                            "write (" << idbuf[0] << ", " << idbuf[1]
                            << ") to " << filename;
                    }
            }
#if defined(DEBUG) || defined(_DEBUG)
            LOGGER(ibis::gVerbose > 5)
                << "DEBUG -- query::recordEqualPairs found "
                << "val1[" << i1 << ":" << j1 << "] (" << val1[i1]
                << ") equals to val2[" << i2 << ":" << j2
                << "] (" << val2[i2] << ")";
#endif
            cnt += (j1 - i1) * (j2 - i2);
            i1 = j1;
            i2 = j2;
        }
    } // while (i1 < n1 && i2 < n2)
    UnixClose(fdes);
    return cnt;
} // ibis::query::recordEqualPairs

template <>
int64_t ibis::query::recordEqualPairs(const array_t<uint32_t>& val1,
                                      const array_t<int32_t>& val2,
                                      const array_t<uint32_t>& ind1,
                                      const array_t<uint32_t>& ind2,
                                      const char* filename) const {
    if (filename == 0 || *filename == 0)
        return countEqualPairs(val1, val2);
    int fdes = UnixOpen(filename, OPEN_WRITENEW, OPEN_FILEMODE);
    if (fdes < 0) {
        logWarning("recordEqualPairs",
                   "failed to open file \"%s\" for writing", filename);
        return countEqualPairs(val1, val2);
    }
#if defined(_WIN32) && defined(_MSC_VER)
    (void)_setmode(fdes, _O_BINARY);
#endif

    off_t ierr;
    int64_t cnt = 0;
    uint32_t idbuf[2];
    const uint32_t idsize = sizeof(idbuf);
    uint32_t i1 = 0, i2 = val2.find(val1.front());
    const uint32_t n1 = val1.find(val2.back()+1U);
    const uint32_t n2 = val2.size();
    while (i1 < n1 && i2 < n2) {
        uint32_t j1, j2;
        if (val1[i1] < (unsigned) val2[i2]) { // move i1 to catch up
            for (++ i1; i1 < n1 && val1[i1] < (unsigned) val2[i2]; ++ i1);
        }
        if ((unsigned) val2[i2] < val1[i1]) { // move i2 to catch up
            for (++ i2; i2 < n2 && (unsigned) val2[i2] < val1[i1]; ++ i2);
        }
        if (i1 < n1 && i2 < n2 && val1[i1] == (unsigned) val2[i2]) {
            // found two equal values
            // next, find out how many values are equal
            for (j1 = i1+1; j1 < n1 && val1[j1] == val1[i1]; ++ j1);
            for (j2 = i2+1; j2 < n2 && val2[i2] == val2[j2]; ++ j2);
            if (ind1.size() == val1.size() && ind2.size() == val2.size()) {
                for (uint32_t ii = i1; ii < j1; ++ ii) {
                    idbuf[0] = ind1[ii];
                    for (uint32_t jj = i2; jj < j2; ++ jj) {
                        idbuf[1] = ind2[jj];
                        ierr = UnixWrite(fdes, idbuf, idsize);
                        LOGGER(ibis::gVerbose > 0 &&
                               (ierr < 0 || (unsigned)ierr != idsize))
                            << "Warning -- query::recordEqualPairs failed to "
                            "write (" << idbuf[0] << ", " << idbuf[1]
                            << ") to " << filename;
                    }
                }
            }
            else {
                for (idbuf[0] = i1; idbuf[0] < j1; ++ idbuf[0])
                    for (idbuf[1] = i2; idbuf[1] < j2; ++ idbuf[1]) {
                        ierr = UnixWrite(fdes, idbuf, idsize);
                        LOGGER(ibis::gVerbose > 0 &&
                               (ierr < 0 || (unsigned)ierr != idsize))
                            << "Warning -- query::recordEqualPairs failed to "
                            "write (" << idbuf[0] << ", " << idbuf[1]
                            << ") to " << filename;
                    }
            }
            cnt += (j1 - i1) * (j2 - i2);
            i1 = j1;
            i2 = j2;
        }
    } // while (i1 < n1 && i2 < n2)
    UnixClose(fdes);
    return cnt;
} // ibis::query::recordEqualPairs

template <>
int64_t ibis::query::recordEqualPairs(const array_t<int32_t>& val1,
                                      const array_t<uint32_t>& val2,
                                      const array_t<uint32_t>& ind1,
                                      const array_t<uint32_t>& ind2,
                                      const char* filename) const {
    if (filename == 0 || *filename == 0)
        return countEqualPairs(val1, val2);
    int fdes = UnixOpen(filename, OPEN_WRITENEW, OPEN_FILEMODE);
    if (fdes < 0) {
        logWarning("recordEqualPairs",
                   "failed to open file \"%s\" for writing", filename);
        return countEqualPairs(val1, val2);
    }
#if defined(_WIN32) && defined(_MSC_VER)
    (void)_setmode(fdes, _O_BINARY);
#endif

    int ierr;
    int64_t cnt = 0;
    uint32_t idbuf[2];
    const uint32_t idsize = sizeof(idbuf);
    uint32_t i1 = val1.find(val2.front()), i2 = 0;
    const uint32_t n1 = val1.size();
    const uint32_t n2 = val2.find(val1.back()+1U);
    while (i1 < n1 && i2 < n2) {
        uint32_t j1, j2;
        if (static_cast<unsigned>(val1[i1]) < val2[i2]) {
            // move i1 to catch up
            for (++ i1;
                 i1 < n1 && static_cast<unsigned>(val1[i1]) < val2[i2];
                 ++ i1);
        }
        if (val2[i2] < static_cast<unsigned>(val1[i1])) {
            // move i2 to catch up
            for (++ i2;
                 i2 < n2 && val2[i2] < static_cast<unsigned>(val1[i1]);
                 ++ i2);
        }
        if (i1 < n1 && i2 < n2 &&
            static_cast<unsigned>(val1[i1]) == val2[i2]) {
            // found two equal values
            // next, find out how many values are equal
            for (j1 = i1+1; j1 < n1 && val1[j1] == val1[i1]; ++ j1);
            for (j2 = i2+1; j2 < n2 && val2[i2] == val2[j2]; ++ j2);
            if (ind1.size() == val1.size() && ind2.size() == val2.size()) {
                for (uint32_t ii = i1; ii < j1; ++ ii) {
                    idbuf[0] = ind1[ii];
                    for (uint32_t jj = i2; jj < j2; ++ jj) {
                        idbuf[1] = ind2[jj];
                        ierr = UnixWrite(fdes, idbuf, idsize);
                        LOGGER(ibis::gVerbose > 0 &&
                               (ierr < 0 || (unsigned)ierr != idsize))
                            << "Warning -- query::recordEqualPairs failed to "
                            "write (" << idbuf[0] << ", " << idbuf[1]
                            << ") to " << filename;
                    }
                }
            }
            else {
                for (idbuf[0] = i1; idbuf[0] < i2; ++ idbuf[0])
                    for (idbuf[1] = j1; idbuf[1] < j2; ++ idbuf[1]) {
                        ierr = UnixWrite(fdes, idbuf, idsize);
                        LOGGER(ibis::gVerbose > 0 &&
                               (ierr < 0 || (unsigned)ierr != idsize))
                            << "Warning -- query::recordEqualPairs failed to "
                            "write (" << idbuf[0] << ", " << idbuf[1]
                            << ") to " << filename;
                    }
            }
            cnt += (j1 - i1) * (j2 - i2);
            i1 = j1;
            i2 = j2;
        }
    } // while (i1 < n1 && i2 < n2)
    UnixClose(fdes);
    return cnt;
} // ibis::query::recordEqualPairs

template <typename T1, typename T2>
int64_t ibis::query::recordDeltaPairs(const array_t<T1>& val1,
                                      const array_t<T2>& val2,
                                      const array_t<uint32_t>& ind1,
                                      const array_t<uint32_t>& ind2,
                                      const T1& delta,
                                      const char* filename) const {
    if (filename == 0 || *filename == 0)
        return countDeltaPairs(val1, val2, delta);
    if (delta <= 0)
        return recordEqualPairs(val1, val2, ind1, ind2, filename);
    int fdes = UnixOpen(filename, OPEN_WRITENEW, OPEN_FILEMODE);
    if (fdes < 0) {
        logWarning("recordDeltaPairs",
                   "failed to open file \"%s\" for writing", filename);
        return countDeltaPairs(val1, val2, delta);
    }
#if defined(_WIN32) && defined(_MSC_VER)
    (void)_setmode(fdes, _O_BINARY);
#endif

#if defined(DEBUG) || defined(_DEBUG)
    for (uint32_t i = 0; i < ind1.size(); ++ i)
        if (ind1[i] > mypart->nRows())
            logWarning("recordDeltaPairs", "ind1[%lu] = %lu is out of "
                       "range (shoud be < %lu)", static_cast<long unsigned>(i),
                       static_cast<long unsigned>(ind1[i]),
                       static_cast<long unsigned>(mypart->nRows()));
    for (uint32_t i = 0; i < ind2.size(); ++ i)
        if (ind2[i] > mypart->nRows())
            logWarning("recordDeltaPairs", "ind2[%lu] = %lu is out of "
                       "range (shoud be < %lu)", static_cast<long unsigned>(i),
                       static_cast<long unsigned>(ind2[i]),
                       static_cast<long unsigned>(mypart->nRows()));
#endif
    off_t ierr;
    int64_t cnt = 0;
    uint32_t idbuf[2];
    const uint32_t idsize = sizeof(idbuf);
    uint32_t i1 = 0, i2 = 0;
    const uint32_t n1 = val1.size();
    for (uint32_t i = 0; i < val2.size() && i1 < n1; ++ i) {
        const T1 hi = static_cast<T1>(val2[i] + delta);
        // presume integer underflow, set it to 0
        const T1 lo = (static_cast<T1>(val2[i] - delta) < hi ?
                       static_cast<T1>(val2[i] - delta) : 0);
        // move i1 to catch up with lo
        while (i1 < n1 && val1[i1] < lo)
            ++ i1;
        // move i2 to catch up with hi
        if (i1 > i2)
            i2 = i1;
        while (i2 < n1 && val1[i2] <= hi)
            ++ i2;

        idbuf[1] = (ind2.size() == val2.size() ? ind2[i] : i);
        if (ind1.size() == val1.size()) {
            for (uint32_t jj = i1; jj < i2; ++ jj) {
                idbuf[0] = ind1[jj];
                ierr = UnixWrite(fdes, idbuf, idsize);
                LOGGER(ibis::gVerbose > 0 &&
                       (ierr < 0 || (unsigned)ierr != idsize))
                    << "Warning -- query::recordDeltaPairs failed to "
                    "write (" << idbuf[0] << ", " << idbuf[1]
                    << ") to " << filename;
#if defined(DEBUG) || defined(_DEBUG)
                if (idbuf[0] != ind1[jj] || idbuf[1] !=
                    (ind2.size() == val2.size() ? ind2[i] : i) ||
                    idbuf[0] >= mypart->nRows() ||
                    idbuf[1] >= mypart->nRows()) {
                    logWarning("recordDeltaPairs", "idbuf (%lu, %lu) differs "
                               "from expected (%lu, %lu) or is out of range",
                               static_cast<long unsigned>(idbuf[0]),
                               static_cast<long unsigned>(idbuf[1]),
                               static_cast<long unsigned>(ind1[jj]),
                               static_cast<long unsigned>
                               (ind2.size() == val2.size() ? ind2[i] : i));
                }
#endif
            }
        }
        else {
            for (idbuf[0] = i1; idbuf[0] < i2 && idbuf[0] < n1; ++ idbuf[0]) {
                ierr = UnixWrite(fdes, idbuf, idsize);
                LOGGER(ibis::gVerbose > 0 &&
                       (ierr < 0 || (unsigned)ierr != idsize))
                    << "Warning -- query::recordDeltaPairs failed to "
                    "write (" << idbuf[0] << ", " << idbuf[1]
                    << ") to " << filename;
            }
        }
        cnt += i2 - i1;
    } // for ..
    UnixClose(fdes);
#if defined(DEBUG) || defined(_DEBUG)
    ibis::util::logger lg(4);
    lg() << "DEBUG -- recordDeltaPairs val1=[";
    for (uint32_t ii = 0; ii < val1.size(); ++ ii)
        lg() << val1[ii] << ' ';
    lg() << "]\n";
    if (ind1.size() == val1.size()) {
        lg() << "DEBUG -- recordDeltaPairs ind1=[";
        for (uint32_t ii = 0; ii < ind1.size(); ++ ii)
            lg() << ind1[ii] << ' ';
        lg() << "]\n";
    }
    lg() << "DEBUG -- recordDeltaPairs val2=[";
    for (uint32_t ii = 0; ii < val2.size(); ++ ii)
        lg() << val2[ii] << ' ';
    lg() << "]\n";
    if (ind2.size() == val2.size()) {
        lg() << "DEBUG -- recordDeltaPairs ind2=[";
        for (uint32_t ii = 0; ii < ind2.size(); ++ ii)
            lg() << ind2[ii] << ' ';
        lg() << "]\n";
    }
    lg() << "DEBUG -- cnt=" << cnt;
#endif
    return cnt;
} // ibis::query::recordDeltaPairs

template <>
int64_t ibis::query::recordDeltaPairs(const array_t<uint32_t>& val1,
                                      const array_t<int32_t>& val2,
                                      const array_t<uint32_t>& ind1,
                                      const array_t<uint32_t>& ind2,
                                      const uint32_t& delta,
                                      const char* filename) const {
    if (filename == 0 || *filename == 0)
        return countDeltaPairs(val1, val2, delta);
    if (delta <= 0)
        return recordEqualPairs(val1, val2, ind1, ind2, filename);
    int fdes = UnixOpen(filename, OPEN_WRITENEW, OPEN_FILEMODE);
    if (fdes < 0) {
        logWarning("recordDeltaPairs",
                   "failed to open file \"%s\" for writing", filename);
        return countDeltaPairs(val1, val2, delta);
    }
#if defined(_WIN32) && defined(_MSC_VER)
    (void)_setmode(fdes, _O_BINARY);
#endif

    off_t ierr;
    int64_t cnt = 0;
    uint32_t idbuf[2];
    const uint32_t idsize = sizeof(idbuf);
    uint32_t i1 = 0, i2 = 0;
    const uint32_t n1 = val1.size();
    for (uint32_t i = val2.find(static_cast<int>(val1.front()-delta));
         i < val2.size() && i1 < n1; ++ i) {
        const unsigned lo = static_cast<unsigned>
            (val2[i] > static_cast<int>(delta) ? val2[i] - delta : 0);
        const unsigned hi = static_cast<unsigned>(val2[i] + delta);
        // move i1 to catch up with lo
        while (i1 < n1 && val1[i1] < lo)
            ++ i1;
        // move i2 to catch up with hi
        if (i1 > i2)
            i2 = i1;
        while (i2 < n1 && val1[i2] <= hi)
            ++ i2;
        idbuf[1] = (ind2.size() == val2.size() ? ind2[i] : i);
        if (ind1.size() == val1.size()) {
            for (uint32_t jj = i1; jj < i2; ++ jj) {
                idbuf[0] = ind1[jj];
                ierr = UnixWrite(fdes, idbuf, idsize);
                LOGGER(ibis::gVerbose > 0 &&
                       (ierr < 0 || (unsigned)ierr != idsize))
                    << "Warning -- query::recordDeltaPairs failed to "
                    "write (" << idbuf[0] << ", " << idbuf[1]
                    << ") to " << filename;
            }
        }
        else {
            for (idbuf[0] = i1; idbuf[0] < i2 && idbuf[0] < n1; ++ idbuf[0]) {
                ierr = UnixWrite(fdes, idbuf, idsize);
                LOGGER(ibis::gVerbose > 0 &&
                       (ierr < 0 || (unsigned)ierr != idsize))
                    << "Warning -- query::recordDeltaPairs failed to "
                    "write (" << idbuf[0] << ", " << idbuf[1]
                    << ") to " << filename;
            }
        }
        cnt += i2 - i1;
    } // for ..
    UnixClose(fdes);
    return cnt;
} // ibis::query::recordDeltaPairs

template <>
int64_t ibis::query::recordDeltaPairs(const array_t<int32_t>& val1,
                                      const array_t<uint32_t>& val2,
                                      const array_t<uint32_t>& ind1,
                                      const array_t<uint32_t>& ind2,
                                      const int32_t& delta,
                                      const char* filename) const {
    if (filename == 0 || *filename == 0)
        return countDeltaPairs(val1, val2, delta);
    if (delta <= 0)
        return recordEqualPairs(val1, val2, ind1, ind2, filename);
    int fdes = UnixOpen(filename, OPEN_WRITENEW, OPEN_FILEMODE);
    if (fdes < 0) {
        logWarning("recordDeltaPairs",
                   "failed to open file \"%s\" for writing", filename);
        return countDeltaPairs(val1, val2, delta);
    }
#if defined(_WIN32) && defined(_MSC_VER)
    (void)_setmode(fdes, _O_BINARY);
#endif

    off_t ierr;
    int64_t cnt = 0;
    uint32_t idbuf[2];
    const uint32_t idsize = sizeof(idbuf);
    uint32_t i1 = 0, i2 = 0;
    const uint32_t n1 = val1.size();
    for (uint32_t i = 0;
         i < val2.find(static_cast<unsigned>(val1.back()+delta)) && i1 < n1;
         ++ i) {
        const int lo = static_cast<int>(val2[i] - delta);
        const int hi = static_cast<int>(val2[i] + delta);
        // move i1 to catch up with lo
        while (i1 < n1 && val1[i1] < lo)
            ++ i1;
        // move i2 to catch up with hi
        if (i1 > i2)
            i2 = i1;
        while (i2 < n1 && val1[i2] <= hi)
            ++ i2;
        idbuf[1] = (ind2.size() == val2.size() ? ind2[i] : i);
        if (ind1.size() == val1.size()) {
            for (uint32_t jj = i1; jj < i2; ++ jj) {
                idbuf[0] = ind1[jj];
                ierr = UnixWrite(fdes, idbuf, idsize);
                LOGGER(ibis::gVerbose > 0 &&
                       (ierr < 0 || (unsigned)ierr != idsize))
                    << "Warning -- query::recordDeltaPairs failed to "
                    "write (" << idbuf[0] << ", " << idbuf[1]
                    << ") to " << filename;
            }
        }
        else {
            for (idbuf[0] = i1; idbuf[0] < i2 && idbuf[0] < n1; ++ idbuf[0]) {
                ierr = UnixWrite(fdes, idbuf, idsize);
                LOGGER(ibis::gVerbose > 0 &&
                       (ierr < 0 || (unsigned)ierr != idsize))
                    << "Warning -- query::recordDeltaPairs failed to "
                    "write (" << idbuf[0] << ", " << idbuf[1]
                    << ") to " << filename;
            }
        }
        cnt += i2 - i1;
    } // for ..
    UnixClose(fdes);
    return cnt;
} // ibis::query::recordDeltaPairs

/// Performing an equi-join by sorting the selected values first.  This
/// version reads the values marked to be 1 in the bitvector @c mask and
/// performs the actual operation of counting the number of pairs with
/// equal values in memory.
int64_t ibis::query::sortEquiJoin(const ibis::deprecatedJoin& cmp,
                                  const ibis::bitvector& mask) const {
    ibis::horometer timer;
    if (ibis::gVerbose > 2)
        timer.start();

    long ierr = 0;
    const ibis::column *col1 = mypart->getColumn(cmp.getName1());
    const ibis::column *col2 = mypart->getColumn(cmp.getName2());
    if (col1 == 0) {
        logWarning("sortEquiJoin", "can not find the named column (%s)",
                   cmp.getName1());
        return -1;
    }
    if (col2 == 0) {
        logWarning("sortEquiJoin", "can not find the named column (%s)",
                   cmp.getName2());
        return -2;
    }
    int64_t cnt = 0;

    switch (col1->type()) {
    case ibis::INT: {
        array_t<int32_t> val1;
        {
            array_t<uint32_t> ind1;
            ierr = col1->selectValues(mask, &val1, ind1);
            if (ierr < 0)
                return ierr;
        }
        std::sort(val1.begin(), val1.end());
        switch (col2->type()) {
        case ibis::INT: {
            array_t<int32_t> val2;
            {
                array_t<uint32_t> ind2;
                ierr = col2->selectValues(mask, &val2, ind2);
                if (ierr < 0)
                    return ierr;
            }
            std::sort(val2.begin(), val2.end());
            cnt = countEqualPairs(val1, val2);
            break;}
        case ibis::UINT:
        case ibis::CATEGORY: {
            array_t<uint32_t> val2;
            {
                array_t<uint32_t> ind2;
                ierr = col2->selectValues(mask, &val2, ind2);
                if (ierr < 0)
                    return ierr;
            }
            std::sort(val2.begin(), val2.end());
            cnt = countEqualPairs(val1, val2);
            break;}
        case ibis::FLOAT: {
            array_t<float> val2;
            {
                array_t<uint32_t> ind2;
                ierr = col2->selectValues(mask, &val2, ind2);
                if (ierr < 0)
                    return ierr;
            }
            std::sort(val2.begin(), val2.end());
            cnt = countEqualPairs(val1, val2);
            break;}
        case ibis::DOUBLE: {
            array_t<double> val2;
            {
                array_t<uint32_t> ind2;
                ierr = col2->selectValues(mask, &val2, ind2);
                if (ierr < 0)
                    return ierr;
            }
            std::sort(val2.begin(), val2.end());
            cnt = countEqualPairs(val1, val2);
            break;}
        default:
            logWarning("sortEquiJoin", "column %s has a unsupported type %d",
                       cmp.getName2(), static_cast<int>(col2->type()));
        }
        break;}
    case ibis::UINT:
    case ibis::CATEGORY: {
        array_t<uint32_t> val1;
        {
            array_t<uint32_t> ind1;
            ierr = col1->selectValues(mask, &val1, ind1);
            if (ierr < 0)
                return ierr;
        }
        std::sort(val1.begin(), val1.end());
        switch (col2->type()) {
        case ibis::INT: {
            array_t<int32_t> val2;
            {
                array_t<uint32_t> ind2;
                ierr = col2->selectValues(mask, &val2, ind2);
                if (ierr < 0)
                    return ierr;
            }
            std::sort(val2.begin(), val2.end());
            cnt = countEqualPairs(val1, val2);
            break;}
        case ibis::UINT:
        case ibis::CATEGORY: {
            array_t<uint32_t> val2;
            {
                array_t<uint32_t> ind2;
                ierr = col2->selectValues(mask, &val2, ind2);
                if (ierr < 0)
                    return ierr;
            }
            std::sort(val2.begin(), val2.end());
            cnt = countEqualPairs(val1, val2);
            break;}
        case ibis::FLOAT: {
            array_t<float> val2;
            {
                array_t<uint32_t> ind2;
                ierr = col2->selectValues(mask, &val2, ind2);
                if (ierr < 0)
                    return ierr;
            }
            std::sort(val2.begin(), val2.end());
            cnt = countEqualPairs(val1, val2);
            break;}
        case ibis::DOUBLE: {
            array_t<double> val2;
            {
                array_t<uint32_t> ind2;
                ierr = col2->selectValues(mask, &val2, ind2);
                if (ierr < 0)
                    return ierr;
            }
            std::sort(val2.begin(), val2.end());
            cnt = countEqualPairs(val1, val2);
            break;}
        default:
            logWarning("sortEquiJoin", "column %s has a unsupported type %d",
                       cmp.getName2(), static_cast<int>(col2->type()));
        }
        break;}
    case ibis::FLOAT: {
        array_t<float> val1;
        {
            array_t<uint32_t> ind1;
            ierr = col1->selectValues(mask, &val1, ind1);
            if (ierr < 0)
                return ierr;
        }
        std::sort(val1.begin(), val1.end());
        switch (col2->type()) {
        case ibis::INT: {
            array_t<int32_t> val2;
            {
                array_t<uint32_t> ind2;
                ierr = col2->selectValues(mask, &val2, ind2);
                if (ierr < 0)
                    return ierr;
            }
            std::sort(val2.begin(), val2.end());
            cnt = countEqualPairs(val1, val2);
            break;}
        case ibis::UINT:
        case ibis::CATEGORY: {
            array_t<uint32_t> val2;
            {
                array_t<uint32_t> ind2;
                ierr = col2->selectValues(mask, &val2, ind2);
                if (ierr < 0)
                    return ierr;
            }
            std::sort(val2.begin(), val2.end());
            cnt = countEqualPairs(val1, val2);
            break;}
        case ibis::FLOAT: {
            array_t<float> val2;
            {
                array_t<uint32_t> ind2;
                ierr = col2->selectValues(mask, &val2, ind2);
                if (ierr < 0)
                    return ierr;
            }
            std::sort(val2.begin(), val2.end());
            cnt = countEqualPairs(val1, val2);
            break;}
        case ibis::DOUBLE: {
            array_t<double> val2;
            {
                array_t<uint32_t> ind2;
                ierr = col2->selectValues(mask, &val2, ind2);
                if (ierr < 0)
                    return ierr;
            }
            std::sort(val2.begin(), val2.end());
            cnt = countEqualPairs(val1, val2);
            break;}
        default:
            logWarning("sortEquiJoin", "column %s has a unsupported type %d",
                       cmp.getName2(), static_cast<int>(col2->type()));
        }
        break;}
    case ibis::DOUBLE: {
        array_t<double> val1;
        {
            array_t<uint32_t> ind1;
            ierr = col1->selectValues(mask, &val1, ind1);
            if (ierr < 0)
                return ierr;
        }
        std::sort(val1.begin(), val1.end());
        switch (col2->type()) {
        case ibis::INT: {
            array_t<int32_t> val2;
            {
                array_t<uint32_t> ind2;
                ierr = col2->selectValues(mask, &val2, ind2);
                if (ierr < 0)
                    return ierr;
            }
            std::sort(val2.begin(), val2.end());
            cnt = countEqualPairs(val1, val2);
            break;}
        case ibis::UINT:
        case ibis::CATEGORY: {
            array_t<uint32_t> val2;
            {
                array_t<uint32_t> ind2;
                ierr = col2->selectValues(mask, &val2, ind2);
                if (ierr < 0)
                    return ierr;
            }
            std::sort(val2.begin(), val2.end());
            cnt = countEqualPairs(val1, val2);
            break;}
        case ibis::FLOAT: {
            array_t<float> val2;
            {
                array_t<uint32_t> ind2;
                ierr = col2->selectValues(mask, &val2, ind2);
                if (ierr < 0)
                    return ierr;
            }
            std::sort(val2.begin(), val2.end());
            cnt = countEqualPairs(val1, val2);
            break;}
        case ibis::DOUBLE: {
            array_t<double> val2;
            {
                array_t<uint32_t> ind2;
                ierr = col2->selectValues(mask, &val2, ind2);
                if (ierr < 0)
                    return ierr;
            }
            std::sort(val2.begin(), val2.end());
            cnt = countEqualPairs(val1, val2);
            break;}
        default:
            logWarning("sortEquiJoin", "column %s has a unsupported type %d",
                       cmp.getName2(), static_cast<int>(col2->type()));
        }
        break;}
    default:
        logWarning("sortEquiJoin", "column %s has a unsupported type %d",
                   cmp.getName1(), static_cast<int>(col1->type()));
    }

    if (ibis::gVerbose > 2) {
        timer.stop();
        std::ostringstream ostr;
        ostr << cnt << " hit" << (cnt>1 ? "s" : "");
        logMessage("sortEquiJoin", "equi-join(%s, %s) produced %s in "
                   "%g sec(CPU), %g sec(elapsed)",
                   cmp.getName1(), cmp.getName2(), ostr.str().c_str(),
                   timer.CPUTime(), timer.realTime());
    }
    return cnt;
} // ibis::query::sortEquiJoin

/// Performing a range join by sorting the selected values.  The sorting is
/// performed through @c std::sort algorithm.
int64_t ibis::query::sortRangeJoin(const ibis::deprecatedJoin& cmp,
                                   const ibis::bitvector& mask) const {
    ibis::horometer timer;
    if (ibis::gVerbose > 2)
        timer.start();

    long ierr = 0;
    int64_t cnt = 0;
    const ibis::column *col1 = mypart->getColumn(cmp.getName1());
    const ibis::column *col2 = mypart->getColumn(cmp.getName2());
    if (col1 == 0) {
        logWarning("sortRangeJoin", "can not find the named column (%s)",
                   cmp.getName1());
        return -1;
    }
    if (col2 == 0) {
        logWarning("sortRangeJoin", "can not find the named column (%s)",
                   cmp.getName2());
        return -2;
    }

    switch (col1->type()) {
    case ibis::INT: {
        const int32_t delta =
            static_cast<int32_t>(fabs(cmp.getRange()->eval()));
        array_t<int32_t> val1;
        {
            array_t<uint32_t> ind1;
            ierr = col1->selectValues(mask, &val1, ind1);
            if (ierr < 0)
                return ierr;
        }
        std::sort(val1.begin(), val1.end());
        switch (col2->type()) {
        case ibis::INT: {
            array_t<int32_t> val2;
            {
                array_t<uint32_t> ind2;
                ierr = col2->selectValues(mask, &val2, ind2);
                if (ierr < 0)
                    return ierr;
            }
            std::sort(val2.begin(), val2.end());
            cnt = countDeltaPairs(val1, val2, delta);
            break;}
        case ibis::UINT:
        case ibis::CATEGORY: {
            array_t<uint32_t> val2;
            {
                array_t<uint32_t> ind2;
                ierr = col2->selectValues(mask, &val2, ind2);
                if (ierr < 0)
                    return ierr;
            }
            std::sort(val2.begin(), val2.end());
            cnt = countDeltaPairs(val1, val2, delta);
            break;}
        case ibis::FLOAT: {
            array_t<float> val2;
            {
                array_t<uint32_t> ind2;
                ierr = col2->selectValues(mask, &val2, ind2);
                if (ierr < 0)
                    return ierr;
            }
            std::sort(val2.begin(), val2.end());
            cnt = countDeltaPairs(val1, val2, delta);
            break;}
        case ibis::DOUBLE: {
            array_t<double> val2;
            {
                array_t<uint32_t> ind2;
                ierr = col2->selectValues(mask, &val2, ind2);
                if (ierr < 0)
                    return ierr;
            }
            std::sort(val2.begin(), val2.end());
            cnt = countDeltaPairs(val1, val2, delta);
            break;}
        default:
            logWarning("sortRangeJoin",
                       "column %s has a unsupported type %d",
                       cmp.getName2(), static_cast<int>(col2->type()));
        }
        break;}
    case ibis::UINT:
    case ibis::CATEGORY: {
        const uint32_t delta =
            static_cast<uint32_t>(fabs(cmp.getRange()->eval()));
        array_t<uint32_t> val1;
        {
            array_t<uint32_t> ind1;
            ierr = col1->selectValues(mask, &val1, ind1);
            if (ierr < 0)
                return ierr;
        }
        std::sort(val1.begin(), val1.end());
        switch (col2->type()) {
        case ibis::INT: {
            array_t<int32_t> val2;
            {
                array_t<uint32_t> ind2;
                ierr = col2->selectValues(mask, &val2, ind2);
                if (ierr < 0)
                    return ierr;
            }
            std::sort(val2.begin(), val2.end());
            cnt = countDeltaPairs(val1, val2, delta);
            break;}
        case ibis::UINT:
        case ibis::CATEGORY: {
            array_t<uint32_t> val2;
            {
                array_t<uint32_t> ind2;
                ierr = col2->selectValues(mask, &val2, ind2);
                if (ierr < 0)
                    return ierr;
            }
            std::sort(val2.begin(), val2.end());
            cnt = countDeltaPairs(val1, val2, delta);
            break;}
        case ibis::FLOAT: {
            array_t<float> val2;
            {
                array_t<uint32_t> ind2;
                ierr = col2->selectValues(mask, &val2, ind2);
                if (ierr < 0)
                    return ierr;
            }
            std::sort(val2.begin(), val2.end());
            cnt = countDeltaPairs(val1, val2, delta);
            break;}
        case ibis::DOUBLE: {
            array_t<double> val2;
            {
                array_t<uint32_t> ind2;
                ierr = col2->selectValues(mask, &val2, ind2);
                if (ierr < 0)
                    return ierr;
            }
            std::sort(val2.begin(), val2.end());
            cnt = countDeltaPairs(val1, val2, delta);
            break;}
        default:
            logWarning("sortRangeJoin",
                       "column %s has a unsupported type %d",
                       cmp.getName2(), static_cast<int>(col2->type()));
        }
        break;}
    case ibis::FLOAT: {
        const float delta = static_cast<float>(fabs(cmp.getRange()->eval()));
        array_t<float> val1;
        {
            array_t<uint32_t> ind1;
            ierr = col1->selectValues(mask, &val1, ind1);
            if (ierr < 0)
                return ierr;
        }
        std::sort(val1.begin(), val1.end());
        switch (col2->type()) {
        case ibis::INT: {
            array_t<int32_t> val2;
            {
                array_t<uint32_t> ind2;
                ierr = col2->selectValues(mask, &val2, ind2);
                if (ierr < 0)
                    return ierr;
            }
            std::sort(val2.begin(), val2.end());
            cnt = countDeltaPairs(val1, val2, delta);
            break;}
        case ibis::UINT:
        case ibis::CATEGORY: {
            array_t<uint32_t> val2;
            {
                array_t<uint32_t> ind2;
                ierr = col2->selectValues(mask, &val2, ind2);
                if (ierr < 0)
                    return ierr;
            }
            std::sort(val2.begin(), val2.end());
            cnt = countDeltaPairs(val1, val2, delta);
            break;}
        case ibis::FLOAT: {
            array_t<float> val2;
            {
                array_t<uint32_t> ind2;
                ierr = col2->selectValues(mask, &val2, ind2);
                if (ierr < 0)
                    return ierr;
            }
            std::sort(val2.begin(), val2.end());
            cnt = countDeltaPairs(val1, val2, delta);
            break;}
        case ibis::DOUBLE: {
            array_t<double> val2;
            {
                array_t<uint32_t> ind2;
                ierr = col2->selectValues(mask, &val2, ind2);
                if (ierr < 0)
                    return ierr;
            }
            std::sort(val2.begin(), val2.end());
            cnt = countDeltaPairs(val1, val2, delta);
            break;}
        default:
            logWarning("sortRangeJoin",
                       "column %s has a unsupported type %d",
                       cmp.getName2(), static_cast<int>(col2->type()));
        }
        break;}
    case ibis::DOUBLE: {
        const double delta = fabs(cmp.getRange()->eval());
        array_t<double> val1;
        {
            array_t<uint32_t> ind1;
            ierr = col1->selectValues(mask, &val1, ind1);
            if (ierr < 0)
                return ierr;
        }
        std::sort(val1.begin(), val1.end());
        switch (col2->type()) {
        case ibis::INT: {
            array_t<int32_t> val2;
            {
                array_t<uint32_t> ind2;
                ierr = col2->selectValues(mask, &val2, ind2);
                if (ierr < 0)
                    return ierr;
            }
            std::sort(val2.begin(), val2.end());
            cnt = countDeltaPairs(val1, val2, delta);
            break;}
        case ibis::UINT:
        case ibis::CATEGORY: {
            array_t<uint32_t> val2;
            {
                array_t<uint32_t> ind2;
                ierr = col2->selectValues(mask, &val2, ind2);
                if (ierr < 0)
                    return ierr;
            }
            std::sort(val2.begin(), val2.end());
            cnt = countDeltaPairs(val1, val2, delta);
            break;}
        case ibis::FLOAT: {
            array_t<float> val2;
            {
                array_t<uint32_t> ind2;
                ierr = col2->selectValues(mask, &val2, ind2);
                if (ierr < 0)
                    return ierr;
            }
            std::sort(val2.begin(), val2.end());
            cnt = countDeltaPairs(val1, val2, delta);
            break;}
        case ibis::DOUBLE: {
            array_t<double> val2;
            {
                array_t<uint32_t> ind2;
                ierr = col2->selectValues(mask, &val2, ind2);
                if (ierr < 0)
                    return ierr;
            }
            std::sort(val2.begin(), val2.end());
            cnt = countDeltaPairs(val1, val2, delta);
            break;}
        default:
            logWarning("sortRangeJoin",
                       "column %s has a unsupported type %d",
                       cmp.getName2(), static_cast<int>(col2->type()));
        }
        break;}
    default:
        logWarning("sortRangeJoin", "column %s has a unsupported type %d",
                   cmp.getName1(), static_cast<int>(col1->type()));
    }

    if (ibis::gVerbose > 2) {
        timer.stop();
        std::ostringstream ostr;
        ostr << cnt << " hit" << (cnt>1 ? "s" : "");
        logMessage("sortRangeJoin", "range join(%s, %s, %g) produced %s "
                   "in %g sec(CPU), %g sec(elapsed)",
                   cmp.getName1(), cmp.getName2(),
                   fabs(cmp.getRange()->eval()), ostr.str().c_str(),
                   timer.CPUTime(), timer.realTime());
    }
    return cnt;
} // ibis::query::sortRangeJoin

/// Perform equi-join by sorting the selected values.  This version reads
/// the values marked to be 1 in the bitvector @c mask.  It writes the
/// the pairs satisfying the join condition to a file name @c pairfile.
int64_t ibis::query::sortEquiJoin(const ibis::deprecatedJoin& cmp,
                                  const ibis::bitvector& mask,
                                  const char* pairfile) const {
    if (pairfile == 0 || *pairfile == 0)
        return sortEquiJoin(cmp, mask);

    ibis::horometer timer;
    if (ibis::gVerbose > 2)
        timer.start();

    long ierr = 0;
    const ibis::column *col1 = mypart->getColumn(cmp.getName1());
    const ibis::column *col2 = mypart->getColumn(cmp.getName2());
    if (col1 == 0) {
        logWarning("sortEquiJoin", "can not find the named column (%s)",
                   cmp.getName1());
        return -1;
    }
    if (col2 == 0) {
        logWarning("sortEquiJoin", "can not find the named column (%s)",
                   cmp.getName2());
        return -2;
    }
    int64_t cnt = 0;

    switch (col1->type()) {
    case ibis::INT: {
        array_t<int32_t> val1;
        array_t<uint32_t> ind1;
        ierr = col1->selectValues(mask, &val1, ind1);
        if (ierr < 0)
            return ierr;
        { // to limit the scope of tmp;
            array_t<int32_t> tmp(val1.size());
            array_t<uint32_t> itmp(val1.size());
            array_t<int32_t>::stableSort(val1, ind1, tmp, itmp);
        }
        switch (col2->type()) {
        case ibis::INT: {
            array_t<int32_t> val2;
            array_t<uint32_t> ind2;
            ierr = col2->selectValues(mask, &val2, ind2);
            if (ierr < 0)
                return ierr;
            {
                array_t<int32_t> tmp(val2.size());
                array_t<uint32_t> itmp(val2.size());
                array_t<int32_t>::stableSort(val2, ind2, tmp, itmp);
            }
            cnt = recordEqualPairs(val1, val2, ind1, ind2, pairfile);
            break;}
        case ibis::UINT:
        case ibis::CATEGORY: {
            array_t<uint32_t> val2;
            array_t<uint32_t> ind2;
            ierr = col2->selectValues(mask, &val2, ind2);
            if (ierr < 0)
                return ierr;
            {
                array_t<uint32_t> tmp(val2.size());
                array_t<uint32_t> itmp(val2.size());
                array_t<uint32_t>::stableSort(val2, ind2, tmp, itmp);
            }
            cnt = recordEqualPairs(val1, val2, ind1, ind2, pairfile);
            break;}
        case ibis::FLOAT: {
            array_t<float> val2;
            array_t<uint32_t> ind2;
            ierr = col2->selectValues(mask, &val2, ind2);
            if (ierr < 0)
                return ierr;
            {
                array_t<float> tmp(val2.size());
                array_t<uint32_t> itmp(val2.size());
                array_t<float>::stableSort(val2, ind2, tmp, itmp);
            }
            cnt = recordEqualPairs(val1, val2, ind1, ind2, pairfile);
            break;}
        case ibis::DOUBLE: {
            array_t<double> val2;
            array_t<uint32_t> ind2;
            ierr = col2->selectValues(mask, &val2, ind2);
            if (ierr < 0)
                return ierr;
            {
                array_t<double> tmp(val2.size());
                array_t<uint32_t> itmp(val2.size());
                array_t<double>::stableSort(val2, ind2, tmp, itmp);
            }
            cnt = recordEqualPairs(val1, val2, ind1, ind2, pairfile);
            break;}
        default:
            logWarning("sortEquiJoin", "column %s has a unsupported type %d",
                       cmp.getName2(), static_cast<int>(col2->type()));
        }
        break;}
    case ibis::UINT:
    case ibis::CATEGORY: {
        array_t<uint32_t> val1;
        array_t<uint32_t> ind1;
        ierr = col1->selectValues(mask, &val1, ind1);
        if (ierr < 0)
            return ierr;
        {
            array_t<uint32_t> tmp(val1.size());
            array_t<uint32_t> itmp(val1.size());
            array_t<uint32_t>::stableSort(val1, ind1, tmp, itmp);
        }
        switch (col2->type()) {
        case ibis::INT: {
            array_t<int32_t> val2;
            array_t<uint32_t> ind2;
            ierr = col2->selectValues(mask, &val2, ind2);
            if (ierr < 0)
                return ierr;
            {
                array_t<int32_t> tmp(val2.size());
                array_t<uint32_t> itmp(val2.size());
                array_t<int32_t>::stableSort(val2, ind2, tmp, itmp);
            }
            cnt = recordEqualPairs(val1, val2, ind1, ind2, pairfile);
            break;}
        case ibis::UINT:
        case ibis::CATEGORY: {
            array_t<uint32_t> val2;
            array_t<uint32_t> ind2;
            ierr = col2->selectValues(mask, &val2, ind2);
            if (ierr < 0)
                return ierr;
            {
                array_t<uint32_t> tmp(val2.size());
                array_t<uint32_t> itmp(val2.size());
                array_t<uint32_t>::stableSort(val2, ind2, tmp, itmp);
            }
            cnt = recordEqualPairs(val1, val2, ind1, ind2, pairfile);
            break;}
        case ibis::FLOAT: {
            array_t<float> val2;
            array_t<uint32_t> ind2;
            ierr = col2->selectValues(mask, &val2, ind2);
            if (ierr < 0)
                return ierr;
            {
                array_t<float> tmp(val2.size());
                array_t<uint32_t> itmp(val2.size());
                array_t<float>::stableSort(val2, ind2, tmp, itmp);
            }
            cnt = recordEqualPairs(val1, val2, ind1, ind2, pairfile);
            break;}
        case ibis::DOUBLE: {
            array_t<double> val2;
            array_t<uint32_t> ind2;
            ierr = col2->selectValues(mask, &val2, ind2);
            if (ierr < 0)
                return ierr;
            {
                array_t<double> tmp(val2.size());
                array_t<uint32_t> itmp(val2.size());
                array_t<double>::stableSort(val2, ind2, tmp, itmp);
            }
            cnt = recordEqualPairs(val1, val2, ind1, ind2, pairfile);
            break;}
        default:
            logWarning("sortEquiJoin", "column %s has a unsupported type %d",
                       cmp.getName2(), static_cast<int>(col2->type()));
        }
        break;}
    case ibis::FLOAT: {
        array_t<float> val1;
        array_t<uint32_t> ind1;
        ierr = col1->selectValues(mask, &val1, ind1);
        if (ierr < 0)
            return ierr;
        {
            array_t<float> tmp(val1.size());
            array_t<uint32_t> itmp(val1.size());
            array_t<float>::stableSort(val1, ind1, tmp, itmp);
        }
        switch (col2->type()) {
        case ibis::INT: {
            array_t<int32_t> val2;
            array_t<uint32_t> ind2;
            ierr = col2->selectValues(mask, &val2, ind2);
            if (ierr < 0)
                return ierr;
            {
                array_t<int32_t> tmp(val2.size());
                array_t<uint32_t> itmp(val2.size());
                array_t<int32_t>::stableSort(val2, ind2, tmp, itmp);
            }
            cnt = recordEqualPairs(val1, val2, ind1, ind2, pairfile);
            break;}
        case ibis::UINT:
        case ibis::CATEGORY: {
            array_t<uint32_t> val2;
            array_t<uint32_t> ind2;
            ierr = col2->selectValues(mask, &val2, ind2);
            if (ierr < 0)
                return ierr;
            {
                array_t<uint32_t> tmp(val2.size());
                array_t<uint32_t> itmp(val2.size());
                array_t<uint32_t>::stableSort(val2, ind2, tmp, itmp);
            }
            cnt = recordEqualPairs(val1, val2, ind1, ind2, pairfile);
            break;}
        case ibis::FLOAT: {
            array_t<float> val2;
            array_t<uint32_t> ind2;
            ierr = col2->selectValues(mask, &val2, ind2);
            if (ierr < 0)
                return ierr;
            {
                array_t<float> tmp(val2.size());
                array_t<uint32_t> itmp(val2.size());
                array_t<float>::stableSort(val2, ind2, tmp, itmp);
            }
            cnt = recordEqualPairs(val1, val2, ind1, ind2, pairfile);
            break;}
        case ibis::DOUBLE: {
            array_t<double> val2;
            array_t<uint32_t> ind2;
            ierr = col2->selectValues(mask, &val2, ind2);
            if (ierr < 0)
                return ierr;
            {
                array_t<double> tmp(val2.size());
                array_t<uint32_t> itmp(val2.size());
                array_t<double>::stableSort(val2, ind2, tmp, itmp);
            }
            cnt = recordEqualPairs(val1, val2, ind1, ind2, pairfile);
            break;}
        default:
            logWarning("sortEquiJoin", "column %s has a unsupported type %d",
                       cmp.getName2(), static_cast<int>(col2->type()));
        }
        break;}
    case ibis::DOUBLE: {
        array_t<double> val1;
        array_t<uint32_t> ind1;
        ierr = col1->selectValues(mask, &val1, ind1);
        if (ierr < 0)
            return ierr;
        {
            array_t<double> tmp(val1.size());
            array_t<uint32_t> itmp(val1.size());
            array_t<double>::stableSort(val1, ind1, tmp, itmp);
        }
        switch (col2->type()) {
        case ibis::INT: {
            array_t<int32_t> val2;
            array_t<uint32_t> ind2;
            ierr = col2->selectValues(mask, &val2, ind2);
            if (ierr < 0)
                return ierr;
            {
                array_t<int32_t> tmp(val2.size());
                array_t<uint32_t> itmp(val2.size());
                array_t<int32_t>::stableSort(val2, ind2, tmp, itmp);
            }
            cnt = recordEqualPairs(val1, val2, ind1, ind2, pairfile);
            break;}
        case ibis::UINT:
        case ibis::CATEGORY: {
            array_t<uint32_t> val2;
            array_t<uint32_t> ind2;
            ierr = col2->selectValues(mask, &val2, ind2);
            if (ierr < 0)
                return ierr;
            {
                array_t<uint32_t> tmp(val2.size());
                array_t<uint32_t> itmp(val2.size());
                array_t<uint32_t>::stableSort(val2, ind2, tmp, itmp);
            }
            cnt = recordEqualPairs(val1, val2, ind1, ind2, pairfile);
            break;}
        case ibis::FLOAT: {
            array_t<float> val2;
            array_t<uint32_t> ind2;
            ierr = col2->selectValues(mask, &val2, ind2);
            if (ierr < 0)
                return ierr;
            {
                array_t<float> tmp(val2.size());
                array_t<uint32_t> itmp(val2.size());
                array_t<float>::stableSort(val2, ind2, tmp, itmp);
            }
            cnt = recordEqualPairs(val1, val2, ind1, ind2, pairfile);
            break;}
        case ibis::DOUBLE: {
            array_t<double> val2;
            array_t<uint32_t> ind2;
            ierr = col2->selectValues(mask, &val2, ind2);
            if (ierr < 0)
                return ierr;
            {
                array_t<double> tmp(val2.size());
                array_t<uint32_t> itmp(val2.size());
                array_t<double>::stableSort(val2, ind2, tmp, itmp);
            }
            cnt = recordEqualPairs(val1, val2, ind1, ind2, pairfile);
            break;}
        default:
            logWarning("sortEquiJoin", "column %s has a unsupported type %d",
                       cmp.getName2(), static_cast<int>(col2->type()));
        }
        break;}
    default:
        logWarning("sortEquiJoin", "column %s has a unsupported type %d",
                   cmp.getName1(), static_cast<int>(col1->type()));
    }

    if (ibis::gVerbose > 2) {
        timer.stop();
        std::ostringstream ostr;
        ostr << cnt << " hit" << (cnt>1 ? "s" : "");
        logMessage("sortEquiJoin", "equi-join(%s, %s) produced %s in "
                   "%g sec(CPU), %g sec(elapsed)",
                   cmp.getName1(), cmp.getName2(), ostr.str().c_str(),
                   timer.CPUTime(), timer.realTime());
    }
    return cnt;
} // ibis::query::sortEquiJoin

/// Performing range join by sorting the selected values.
int64_t ibis::query::sortRangeJoin(const ibis::deprecatedJoin& cmp,
                                   const ibis::bitvector& mask,
                                   const char* pairfile) const {
    if (pairfile == 0 || *pairfile == 0)
        return sortRangeJoin(cmp, mask);

    ibis::horometer timer;
    if (ibis::gVerbose > 2)
        timer.start();

    long ierr = 0;
    int64_t cnt = 0;
    const ibis::column *col1 = mypart->getColumn(cmp.getName1());
    const ibis::column *col2 = mypart->getColumn(cmp.getName2());
    if (col1 == 0) {
        logWarning("sortRangeJoin", "can not find the named column (%s)",
                   cmp.getName1());
        return -1;
    }
    if (col2 == 0) {
        logWarning("sortRangeJoin", "can not find the named column (%s)",
                   cmp.getName2());
        return -2;
    }

    switch (col1->type()) {
    case ibis::INT: {
        const int32_t delta =
            static_cast<int32_t>(fabs(cmp.getRange()->eval()));
        array_t<int32_t> val1;
        array_t<uint32_t> ind1;
        ierr = col1->selectValues(mask, &val1, ind1);
        if (ierr < 0)
            return ierr;
        {
            array_t<int32_t> tmp(val1.size());
            array_t<uint32_t> itmp(val1.size());
            array_t<int32_t>::stableSort(val1, ind1, tmp, itmp);
        }
        switch (col2->type()) {
        case ibis::INT: {
            array_t<int32_t> val2;
            array_t<uint32_t> ind2;
            ierr = col2->selectValues(mask, &val2, ind2);
            if (ierr < 0)
                return ierr;
            {
                array_t<int32_t> tmp(val2.size());
                array_t<uint32_t> itmp(val2.size());
                array_t<int32_t>::stableSort(val2, ind2, tmp, itmp);
            }
            cnt = recordDeltaPairs(val1, val2, ind1, ind2, delta, pairfile);
            break;}
        case ibis::UINT:
        case ibis::CATEGORY: {
            array_t<uint32_t> val2;
            array_t<uint32_t> ind2;
            ierr = col2->selectValues(mask, &val2, ind2);
            if (ierr < 0)
                return ierr;
            {
                array_t<uint32_t> tmp(val2.size());
                array_t<uint32_t> itmp(val2.size());
                array_t<uint32_t>::stableSort(val2, ind2, tmp, itmp);
            }
            cnt = recordDeltaPairs(val1, val2, ind1, ind2, delta, pairfile);
            break;}
        case ibis::FLOAT: {
            array_t<float> val2;
            array_t<uint32_t> ind2;
            ierr = col2->selectValues(mask, &val2, ind2);
            if (ierr < 0)
                return ierr;
            {
                array_t<float> tmp(val2.size());
                array_t<uint32_t> itmp(val2.size());
                array_t<float>::stableSort(val2, ind2, tmp, itmp);
            }
            cnt = recordDeltaPairs(val1, val2, ind1, ind2, delta, pairfile);
            break;}
        case ibis::DOUBLE: {
            array_t<double> val2;
            array_t<uint32_t> ind2;
            ierr = col2->selectValues(mask, &val2, ind2);
            if (ierr < 0)
                return ierr;
            {
                array_t<double> tmp(val2.size());
                array_t<uint32_t> itmp(val2.size());
                array_t<double>::stableSort(val2, ind2, tmp, itmp);
            }
            cnt = recordDeltaPairs(val1, val2, ind1, ind2, delta, pairfile);
            break;}
        default:
            logWarning("sortRangeJoin",
                       "column %s has a unsupported type %d",
                       cmp.getName2(), static_cast<int>(col2->type()));
        }
        break;}
    case ibis::UINT:
    case ibis::CATEGORY: {
        const uint32_t delta =
            static_cast<uint32_t>(fabs(cmp.getRange()->eval()));
        array_t<uint32_t> val1;
        array_t<uint32_t> ind1;
        ierr = col1->selectValues(mask, &val1, ind1);
        if (ierr < 0)
            return ierr;
#if DEBUG+0 > 1 || _DEBUG+0 > 1
        for (uint32_t i = 0; i < ind1.size(); ++ i)
            if (ind1[i] > mypart->nRows())
                logWarning("sortRangeJoin", "before sorting: ind1[%lu] = %lu "
                           "is out of range (shoud be < %lu)",
                           static_cast<long unsigned>(i),
                           static_cast<long unsigned>(ind1[i]),
                           static_cast<long unsigned>(mypart->nRows()));
#endif
        {
            array_t<uint32_t> tmp(val1.size());
            array_t<uint32_t> itmp(val1.size());
            array_t<uint32_t>::stableSort(val1, ind1, tmp, itmp);
        }
#if DEBUG+0 > 0 || _DEBUG+0 > 0
        for (uint32_t i = 0; i < ind1.size(); ++ i)
            if (ind1[i] > mypart->nRows())
                logWarning("sortRangeJoin", "after sorting: ind1[%lu] = %lu "
                           "is out of range (shoud be < %lu)",
                           static_cast<long unsigned>(i),
                           static_cast<long unsigned>(ind1[i]),
                           static_cast<long unsigned>(mypart->nRows()));
#endif
        switch (col2->type()) {
        case ibis::INT: {
            array_t<int32_t> val2;
            array_t<uint32_t> ind2;
            ierr = col2->selectValues(mask, &val2, ind2);
            if (ierr < 0)
                return ierr;
            {
                array_t<int32_t> tmp(val2.size());
                array_t<uint32_t> itmp(val2.size());
                array_t<int32_t>::stableSort(val2, ind2, tmp, itmp);
            }
            cnt = recordDeltaPairs(val1, val2, ind1, ind2, delta, pairfile);
            break;}
        case ibis::UINT:
        case ibis::CATEGORY: {
            array_t<uint32_t> val2;
            array_t<uint32_t> ind2;
            ierr = col2->selectValues(mask, &val2, ind2);
            if (ierr < 0)
                return ierr;
            {
                array_t<uint32_t> tmp(val2.size());
                array_t<uint32_t> itmp(val2.size());
                array_t<uint32_t>::stableSort(val2, ind2, tmp, itmp);
            }
            cnt = recordDeltaPairs(val1, val2, ind1, ind2, delta, pairfile);
            break;}
        case ibis::FLOAT: {
            array_t<float> val2;
            array_t<uint32_t> ind2;
            ierr = col2->selectValues(mask, &val2, ind2);
            if (ierr < 0)
                return ierr;
            {
                array_t<float> tmp(val2.size());
                array_t<uint32_t> itmp(val2.size());
                array_t<float>::stableSort(val2, ind2, tmp, itmp);
            }
            cnt = recordDeltaPairs(val1, val2, ind1, ind2, delta, pairfile);
            break;}
        case ibis::DOUBLE: {
            array_t<double> val2;
            array_t<uint32_t> ind2;
            ierr = col2->selectValues(mask, &val2, ind2);
            if (ierr < 0)
                return ierr;
            {
                array_t<double> tmp(val2.size());
                array_t<uint32_t> itmp(val2.size());
                array_t<double>::stableSort(val2, ind2, tmp, itmp);
            }
            cnt = recordDeltaPairs(val1, val2, ind1, ind2, delta, pairfile);
            break;}
        default:
            logWarning("sortRangeJoin",
                       "column %s has a unsupported type %d",
                       cmp.getName2(), static_cast<int>(col2->type()));
        }
        break;}
    case ibis::FLOAT: {
        const float delta = static_cast<float>(fabs(cmp.getRange()->eval()));
        array_t<float> val1;
        array_t<uint32_t> ind1;
        ierr = col1->selectValues(mask, &val1, ind1);
        if (ierr < 0)
            return ierr;
        {
            array_t<float> tmp(val1.size());
            array_t<uint32_t> itmp(val1.size());
            array_t<float>::stableSort(val1, ind1, tmp, itmp);
        }
        switch (col2->type()) {
        case ibis::INT: {
            array_t<int32_t> val2;
            array_t<uint32_t> ind2;
            ierr = col2->selectValues(mask, &val2, ind2);
            if (ierr < 0)
                return ierr;
            {
                array_t<int32_t> tmp(val2.size());
                array_t<uint32_t> itmp(val2.size());
                array_t<int32_t>::stableSort(val2, ind2, tmp, itmp);
            }
            cnt = recordDeltaPairs(val1, val2, ind1, ind2, delta, pairfile);
            break;}
        case ibis::UINT:
        case ibis::CATEGORY: {
            array_t<uint32_t> val2;
            array_t<uint32_t> ind2;
            ierr = col2->selectValues(mask, &val2, ind2);
            if (ierr < 0)
                return ierr;
            {
                array_t<uint32_t> tmp(val2.size());
                array_t<uint32_t> itmp(val2.size());
                array_t<uint32_t>::stableSort(val2, ind2, tmp, itmp);
            }
            cnt = recordDeltaPairs(val1, val2, ind1, ind2, delta, pairfile);
            break;}
        case ibis::FLOAT: {
            array_t<float> val2;
            array_t<uint32_t> ind2;
            ierr = col2->selectValues(mask, &val2, ind2);
            if (ierr < 0)
                return ierr;
            {
                array_t<float> tmp(val2.size());
                array_t<uint32_t> itmp(val2.size());
                array_t<float>::stableSort(val2, ind2, tmp, itmp);
            }
            cnt = recordDeltaPairs(val1, val2, ind1, ind2, delta, pairfile);
            break;}
        case ibis::DOUBLE: {
            array_t<double> val2;
            array_t<uint32_t> ind2;
            ierr = col2->selectValues(mask, &val2, ind2);
            if (ierr < 0)
                return ierr;
            {
                array_t<double> tmp(val2.size());
                array_t<uint32_t> itmp(val2.size());
                array_t<double>::stableSort(val2, ind2, tmp, itmp);
            }
            cnt = recordDeltaPairs(val1, val2, ind1, ind2, delta, pairfile);
            break;}
        default:
            logWarning("sortRangeJoin",
                       "column %s has a unsupported type %d",
                       cmp.getName2(), static_cast<int>(col2->type()));
        }
        break;}
    case ibis::DOUBLE: {
        const double delta = fabs(cmp.getRange()->eval());
        array_t<double> val1;
        array_t<uint32_t> ind1;
        ierr = col1->selectValues(mask, &val1, ind1);
        if (ierr < 0)
            return ierr;
        {
            array_t<double> tmp(val1.size());
            array_t<uint32_t> itmp(val1.size());
            array_t<double>::stableSort(val1, ind1, tmp, itmp);
        }
        switch (col2->type()) {
        case ibis::INT: {
            array_t<int32_t> val2;
            array_t<uint32_t> ind2;
            ierr = col2->selectValues(mask, &val2, ind2);
            if (ierr < 0)
                return ierr;
            {
                array_t<int32_t> tmp(val2.size());
                array_t<uint32_t> itmp(val2.size());
                array_t<int32_t>::stableSort(val2, ind2, tmp, itmp);
            }
            cnt = recordDeltaPairs(val1, val2, ind1, ind2, delta, pairfile);
            break;}
        case ibis::UINT:
        case ibis::CATEGORY: {
            array_t<uint32_t> val2;
            array_t<uint32_t> ind2;
            ierr = col2->selectValues(mask, &val2, ind2);
            if (ierr < 0)
                return ierr;
            {
                array_t<uint32_t> tmp(val2.size());
                array_t<uint32_t> itmp(val2.size());
                array_t<uint32_t>::stableSort(val2, ind2, tmp, itmp);
            }
            cnt = recordDeltaPairs(val1, val2, ind1, ind2, delta, pairfile);
            break;}
        case ibis::FLOAT: {
            array_t<float> val2;
            array_t<uint32_t> ind2;
            ierr = col2->selectValues(mask, &val2, ind2);
            if (ierr < 0)
                return ierr;
            {
                array_t<float> tmp(val2.size());
                array_t<uint32_t> itmp(val2.size());
                array_t<float>::stableSort(val2, ind2, tmp, itmp);
            }
            cnt = recordDeltaPairs(val1, val2, ind1, ind2, delta, pairfile);
            break;}
        case ibis::DOUBLE: {
            array_t<double> val2;
            array_t<uint32_t> ind2;
            ierr = col2->selectValues(mask, &val2, ind2);
            if (ierr < 0)
                return ierr;
            {
                array_t<double> tmp(val2.size());
                array_t<uint32_t> itmp(val2.size());
                array_t<double>::stableSort(val2, ind2, tmp, itmp);
            }
            cnt = recordDeltaPairs(val1, val2, ind1, ind2, delta, pairfile);
            break;}
        default:
            logWarning("sortRangeJoin",
                       "column %s has a unsupported type %d",
                       cmp.getName2(), static_cast<int>(col2->type()));
        }
        break;}
    default:
        logWarning("sortRangeJoin", "column %s has a unsupported type %d",
                   cmp.getName1(), static_cast<int>(col1->type()));
    }

    if (ibis::gVerbose > 2) {
        timer.stop();
        std::ostringstream ostr;
        ostr << cnt << " hit" << (cnt>1 ? "s" : "");
        logMessage("sortRangeJoin", "range join(%s, %s, %g) produced %s in "
                   "%g sec(CPU), %g sec(elapsed)",
                   cmp.getName1(), cmp.getName2(),
                   fabs(cmp.getRange()->eval()), ostr.str().c_str(),
                   timer.CPUTime(), timer.realTime());
    }
    return cnt;
} // ibis::query::sortRangeJoin

/// Sort the content of the file as ibis::rid_t.  It reads the content
/// of the file one block at a time during the initial sorting of the
/// blocks.  It then merges the sorted blocks to produce a overall sorted
/// file.  Note that ibis::rid_t is simply a pair of integers.  Sinc the
/// pairs are recorded as pairs of integers too, this should work.
void ibis::query::orderPairs(const char *pfile) const {
    if (pfile == 0 || *pfile == 0)
        return;
    uint32_t npairs = ibis::util::getFileSize(pfile);
    int fdes = UnixOpen(pfile, OPEN_READWRITE, OPEN_FILEMODE);
    long ierr;
    if (fdes < 0) {
        logWarning("orderPairs", "failed to open %s for sorting", pfile);
        return;
    }
#if defined(_WIN32) && defined(_MSC_VER)
    (void)_setmode(fdes, _O_BINARY);
#endif
#if defined(DEBUG) || defined(_DEBUG)
    if (ibis::fileManager::instance().bytesFree() > npairs) {
#endif
        npairs /= sizeof(ibis::rid_t);
        try {
            const long unsigned nbytes = npairs * sizeof(ibis::rid_t);
            array_t<ibis::rid_t> tmp(npairs);
            ierr = UnixRead(fdes, tmp.begin(), nbytes);
            if (ierr >= static_cast<long>(nbytes)) {
                std::sort(tmp.begin(), tmp.end());
                ierr = UnixSeek(fdes, 0, SEEK_SET);
                ierr = UnixWrite(fdes, tmp.begin(), nbytes);
                if (ierr != static_cast<long>(nbytes))
                    logWarning("orderPairs", "expected to write %lu bytes "
                               "to %s, but only wrote %ld",
                               nbytes, pfile, ierr);
                UnixClose(fdes);
                return;
            }
            else {
                logMessage("orderPairs", "failed to read all %lu bytes "
                           "from %s in one shot (ierr=%ld), "
                           "will use out-of-core sorting",
                           nbytes, pfile, ierr);
            }
        }
        catch (...) {
            logMessage("orderPairs", "received an exception (like because "
                       "there is not enough memory to read the whole "
                       "content of %s), will use out-of-core sorting",
                       pfile);
        }
#if defined(DEBUG) || defined(_DEBUG)
    }
#endif
#if defined(DEBUG) || defined(_DEBUG)
    const uint32_t mblock = PREFERRED_BLOCK_SIZE / (2*sizeof(uint32_t));
    array_t<ibis::rid_t> buf1(mblock), buf2(mblock);
    // the initial sorting of the blocks.
    bool more = true;
    while (more) {
        ierr = UnixRead(fdes, buf1.begin(), mblock*sizeof(ibis::rid_t));
        if (ierr > 0) {
            long bytes = ierr;
            ierr /= sizeof(ibis::rid_t);
            npairs += ierr;
            buf1.resize(ierr);
            buf1.stableSort(buf2);  // sort the block
            // write back the sorted values
            ierr = UnixSeek(fdes, -bytes, SEEK_CUR);
            if (ierr == -1) {
                logWarning("orderPairs", "UnixSeek on %s encountered an "
                           "error, can not proceed anymore",
                           pfile);
                UnixClose(fdes);
                return;
            }
            ierr = UnixWrite(fdes, buf1.begin(), bytes);
            if (ierr != bytes) {
                logWarning("orderPairs", "expected to write %ld bytes, "
                           "but actually wrote %d", bytes, ierr);
            }
        }
        else {
            more = false;
        }
    }
    UnixClose(fdes);
    if (ibis::gVerbose > 6)
        logMessage("orderPairs", "complete sorting file %s in blocks of "
                   "size %lu (total %lu)", pfile,
                   static_cast<long unsigned>(mblock),
                   static_cast<long unsigned>(npairs));

    // merge the sorted blocks
    const uint32_t totbytes = npairs*sizeof(ibis::rid_t);
    const uint32_t bytes = mblock * sizeof(ibis::rid_t);
    uint32_t stride = bytes;
    array_t<ibis::rid_t> buf3(mblock);
    std::string tmpfile(pfile);
    tmpfile += "-tmp";
    const char *name1 = pfile;
    const char *name2 = tmpfile.c_str();
    while (stride < totbytes) {
        if (ibis::gVerbose > 6)
            logMessage("orderPairs",
                       "merging block from %lu bytes apart in %s",
                       static_cast<long unsigned>(stride), pfile);
        ibis::roster::mergeBlock2<ibis::rid_t>(name1, name2, stride,
                                               buf1, buf2, buf3);
        const char *stmp = name1;
        name1 = name2;
        name2 = stmp;
        stride += stride;
    }
    remove(name2); // remove the temp file
    if (name1 != pfile) {
        rename(name1, name2);
    }
#else
    logWarning("orderPairs", "out-of-core version does not work yet");
#endif
} // ibis::query::orderPairs

int64_t ibis::query::mergePairs(const char *pfile) const {
    if (pfile == 0 || *pfile == 0)
        return 0;

    uint32_t buf1[2], buf2[2];
    const uint32_t idsize = sizeof(buf1);
    int64_t cnt = ibis::util::getFileSize(pfile);
    cnt /= idsize;
    if (cnt <= 0)
        return cnt;

    std::string oldfile(myDir);
    std::string outfile(myDir);
    oldfile += "oldpairs";
    outfile += "pairs";
    const uint32_t incnt = cnt;
    const uint32_t oldcnt = ibis::util::getFileSize(outfile.c_str()) / idsize;
    if (oldcnt == 0) {
        // the output file does not exist, simply copy the input file to
        // the output file
        ibis::util::copy(outfile.c_str(), pfile);
        return cnt;
    }

    long ierr = rename(outfile.c_str(), oldfile.c_str());
    if (ierr != 0) {
        logWarning("mergePairs", "failed to rename \"%s\" to \"%s\"",
                   outfile.c_str(), oldfile.c_str());
        return -1;
    }
    cnt = 0;
    int indes = UnixOpen(pfile, OPEN_READONLY);
    if (indes < 0) {
        logWarning("mergePairs", "failed to open %s for reading", pfile);
        return -2;
    }

    int outdes = UnixOpen(outfile.c_str(), OPEN_WRITENEW, OPEN_FILEMODE);
    if (outdes < 0) {
        logWarning("mergePairs", "failed to open %s for writing",
                   outfile.c_str());
        UnixClose(indes);
        return -3;
    }

    int olddes = UnixOpen(oldfile.c_str(), OPEN_READONLY);
    if (olddes < 0) {
        logWarning("mergePairs", "failed to open %s for reading",
                   oldfile.c_str());
        UnixClose(outdes);
        UnixClose(indes);
        return -4;
    }
#if defined(_WIN32) && defined(_MSC_VER)
    (void)_setmode(indes, _O_BINARY);
    (void)_setmode(outdes, _O_BINARY);
    (void)_setmode(olddes, _O_BINARY);
#endif

    ierr = UnixRead(indes, buf1, idsize);
    ierr += UnixRead(olddes, buf2, idsize);
    while (ierr >= static_cast<int>(idsize)) {
        while (ierr >= static_cast<int>(idsize) &&
               (buf1[0] < buf2[0] ||
                (buf1[0] == buf2[0] && buf1[1] < buf2[1]))) {
#if defined(DEBUG) || defined(_DEBUG)
            uint32_t tmp[2];
            ierr = UnixRead(indes, tmp, idsize);
            if (tmp[0] < buf1[0] ||
                (tmp[0] == buf1[0] && tmp[1] < buf1[1]))
                logWarning("mergePairs", "%s not sorted -- pairs (%lu, %lu) "
                           "and (%lu, %lu) out of order",
                           pfile, static_cast<long unsigned>(buf1[0]),
                           static_cast<long unsigned>(buf1[1]),
                           static_cast<long unsigned>(tmp[0]),
                           static_cast<long unsigned>(tmp[1]));
            buf1[0] = tmp[0];
            buf1[1] = tmp[1];
#else
            ierr = UnixRead(indes, buf1, idsize);
#endif
        }
        while (ierr >= static_cast<int>(idsize) &&
               (buf1[0] > buf2[0] ||
                (buf1[0] == buf2[0] && buf1[1] > buf2[1]))) {
#if defined(DEBUG) || defined(_DEBUG)
            uint32_t tmp[2];
            ierr = UnixRead(olddes, tmp, idsize);
            if (tmp[0] < buf2[0] ||
                (tmp[0] == buf2[0] && tmp[1] < buf2[1]))
                logWarning("mergePairs", "%s not sorted -- pairs (%lu, %lu) "
                           "and (%lu, %lu) out of order", oldfile.c_str(),
                           static_cast<long unsigned>(buf2[0]),
                           static_cast<long unsigned>(buf2[1]),
                           static_cast<long unsigned>(tmp[0]),
                           static_cast<long unsigned>(tmp[1]));
            buf2[0] = tmp[0];
            buf2[1] = tmp[1];
#else
            ierr = UnixRead(olddes, buf2, idsize);
#endif
        }
        if (ierr >= static_cast<int>(idsize) &&
            buf1[0] == buf2[0] && buf1[1] == buf2[1]) {
            ierr = UnixWrite(outdes, buf1, idsize);
            if (ierr >= static_cast<int>(idsize)) {
                ++ cnt;
            }
            else {
                logWarning("mergePairs", "failed to write %ld-th pair to %s",
                           static_cast<long>(cnt), outfile.c_str());
                ierr = UnixSeek(outdes, cnt*idsize, SEEK_SET);
            }

            ierr = UnixRead(indes, buf1, idsize);
            if (ierr >= static_cast<int>(idsize))
                ierr = UnixRead(olddes, buf2, idsize);
        }
    } // while (ierr >= idsize)

    UnixClose(olddes);
    UnixClose(outdes);
    UnixClose(indes);
    remove(oldfile.c_str());
    if (ibis::gVerbose > 4)
        logMessage("mergePairs", "comparing %lu pairs from \"%s\" with "
                   "%lu pairs in \"pairs\" produced %lu common ones",
                   static_cast<long unsigned>(incnt), pfile,
                   static_cast<long unsigned>(oldcnt),
                   static_cast<long unsigned>(cnt));
    return cnt;
} // ibis::query::mergePairs
