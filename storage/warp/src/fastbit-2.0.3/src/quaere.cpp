// File: $Id$
// Author: John Wu <John.Wu at ACM.org>
// Copyright (c) 2010-2016 the Regents of the University of California
#include "jnatural.h"   // ibis::jNatural
#include "jrange.h"     // ibis::jRange
#include "filter.h"     // ibis::filter
#include "fromClause.h" // ibis::fromClause
#include "whereClause.h"// ibis::whereClause

#include <memory>       // std::unique_ptr

/// Create a query object using the global datasets.
ibis::quaere* ibis::quaere::create(const char* sel, const char* from,
                                    const char* where) {
    return ibis::quaere::create(sel, from, where, ibis::datasets);
}

/// Generate a query expression.  This function takes three arguments known
/// as the select clause, the from clause and the where clause.  It expects
/// a valid where clause, but the select clause and the from clause could
/// be blank strings or left as nil pointers.  If the select clause is
/// undefined, the default operation is to count the number of hits.  If
/// the from clause is not specified, it will attempt to use all the data
/// partitions stored in the prts.  If the where clause is not specified,
/// the query is assumed to select every row (following the SQL
/// convension).
///
/// @note If more than one data partition was used in specifying the query,
/// the column names should be fully qualified in the form of
/// "part-name.column-name".  If a dot ('.') is not present or the string
/// before the dot is not the name of a data partition, the whole string is
/// taken to be a column name.  In which case, the lookup proceeds from the
/// list of data partitions one at a time.  A nil pointer will be returned
/// if any name is not associated with a known column.
ibis::quaere*
ibis::quaere::create(const char* sel, const char* fr, const char* wh,
                     const ibis::partList& prts) {
    if (prts.empty()) return 0;
    std::string sql;
    if (fr != 0 && *fr != 0) {
        sql += "From ";
        sql += fr;
    }
    if (wh != 0 && *wh != 0) {
        sql += " Where ";
        sql += wh;
    }

    int ierr;
    try {
        ibis::selectClause sc;
        if (sel == 0 || *sel == 0) {
        }
        else if (*sel == '*' && sel[1] == 0) {
            const ibis::table::stringArray sl = prts[0]->columnNames();
            ibis::selectClause sc1(sl);
            sc.swap(sc1);
        }
        else {
            int ierr = sc.parse(sel);
            LOGGER(ierr < 0 && ibis::gVerbose > 0)
                << "Warning -- quaere::create failed to parse \"" << sel
                << "\" into a selectClause, ierr = " << ierr;
        }
        ibis::fromClause fc(fr);
        ibis::whereClause wc(wh);
        if (wc.empty()) {
            LOGGER(ibis::gVerbose >= 2)
                << "Warning -- quaere::create(" << sql
                << ") has an empty where clause";
            return 0;
        }

        std::set<std::string> plist;
        wc.getExpr()->getTableNames(plist);
        if (plist.empty() || (plist.size() == 1 && plist.begin()->empty())) {
            // a simple filter
            return new ibis::filter
                (&sc, reinterpret_cast<const constPartList*>(&prts), &wc);
        }
        else if (plist.size() == 1) { // one table name
            std::set<std::string>::const_iterator pit = plist.begin();
            ibis::part *pt = ibis::findDataset(pit->c_str(), prts);
            if (pt == 0) {
                LOGGER(ibis::gVerbose >= 0)
                    << "Warning -- quaere::create(" << sql
                    << ") can't find a data partition known as " << *pit;
                return 0;
            }
            else {
                ibis::constPartList pl(1);
                pl[0] = pt;
                return new ibis::filter(&sc, &pl, &wc);
            }
        }
        else if (plist.size() == 2) { // two table names
            // note that the names are in alphabetical order, and all names
            // including the aliases are treated as different according to
            // their literal values.  Even though a may be an aliases for
            // table aLongName, they are treated as different names.
            std::set<std::string>::const_iterator pit = plist.begin();
            const char *pr = pit->c_str();
            ++ pit;
            const char *ps = pit->c_str();

            const char *rpr = fc.realName(pr);
            if (rpr == 0 || *rpr == 0) {
                LOGGER(ibis::gVerbose >= 0)
                    << "Warning -- quaere::create(" << sql
                    << ") can't find a data partition known as " << pr;
                return 0;
            }
            ibis::part *partr = ibis::findDataset(rpr, prts);
            if (partr == 0) {
                LOGGER(ibis::gVerbose >= 0)
                    << "Warning -- quaere::create(" << sql
                    << ") can't find a data partition named " << rpr << " ("
                    << pr << ")";
                return 0;
            }

            const char *rps = fc.realName(ps);
            if (rps == 0 || *rps == 0) {
                LOGGER(ibis::gVerbose >= 0)
                    << "Warning -- quaere::create(" << sql
                    << ") can't find a data partition known as " << ps;
                return 0;
            }
            ibis::part *parts = ibis::findDataset(rps, prts);
            if (parts == 0) {
                LOGGER(ibis::gVerbose >= 0)
                    << "Warning -- quaere::create(" << sql
                    << ") can't find a data partition named " << rps << " ("
                    << ps << ")";
                return 0;
            }
            if (partr == parts && stricmp(ps, rps) == 0) {
                ps = pr;
                rps = rpr;
                pr = partr->name();
                rpr = partr->name();
            }
            else {
                if (stricmp(pr, rpr) == 0) // retrieve the alias
                    pr = fc.alias(rpr);
                if (stricmp(ps, rps) == 0)
                    ps = fc.alias(rps);
            }

            std::unique_ptr<ibis::qExpr> condr;
            std::unique_ptr<ibis::qExpr> conds;
            std::unique_ptr<ibis::qExpr> condj;
            ibis::qExpr::termTableList ttl;
            wc.getExpr()->getConjunctiveTerms(ttl);
            for (size_t j = 0; j < ttl.size() ; ++ j) {
                if (ttl[j].tnames.size() == 0 ||
                    (ttl[j].tnames.size() == 1 &&
                     ttl[j].tnames.begin()->empty())) { // no table name
                    ierr = ibis::whereClause::verifyExpr
                        (ttl[j].term, *partr, &sc);
                    if (ierr == 0) { // definitely for partr
                        if (condr.get() != 0) {
                            ibis::qExpr *tmp =
                                new ibis::qExpr(ibis::qExpr::LOGICAL_AND);
                            tmp->setLeft(condr.release());
                            tmp->setRight(ttl[j].term->dup());
                            condr.reset(tmp);
                        }
                        else {
                            condr.reset(ttl[j].term->dup());
                        }
                    }
                    else {
                        ierr = ibis::whereClause::verifyExpr
                            (ttl[j].term, *parts, &sc);
                        if (ierr == 0) {
                            if (conds.get() != 0) {
                                ibis::qExpr *tmp =
                                    new ibis::qExpr(ibis::qExpr::LOGICAL_AND);
                                tmp->setLeft(conds.release());
                                tmp->setRight(ttl[j].term->dup());
                                conds.reset(tmp);
                            }
                            else {
                                conds.reset(ttl[j].term->dup());
                            }
                        }
                        else {
                            LOGGER(ibis::gVerbose > 1)
                                << "Warning -- quaere::create failed to "
                                "associate " << *(ttl[j].term)
                                << " with either " << pr << " or " << ps
                                << ", discard the term";
                        }
                    }
                }
                else if (ttl[j].tnames.size() == 1) { // one table name
                    pit = ttl[j].tnames.begin();
                    if (stricmp(pit->c_str(), pr) == 0 ||
                             stricmp(pit->c_str(), rpr) == 0) {
                        if (condr.get() != 0) {
                            ibis::qExpr *tmp =
                                new ibis::qExpr(ibis::qExpr::LOGICAL_AND);
                            tmp->setLeft(condr.release());
                            tmp->setRight(ttl[j].term->dup());
                            condr.reset(tmp);
                        }
                        else {
                            condr.reset(ttl[j].term->dup());
                        }
                    }
                    else if (stricmp(pit->c_str(), ps) == 0 ||
                             stricmp(pit->c_str(), rps) == 0) {
                        if (conds.get() != 0) {
                            ibis::qExpr *tmp =
                                new ibis::qExpr(ibis::qExpr::LOGICAL_AND);
                            tmp->setLeft(conds.release());
                            tmp->setRight(ttl[j].term->dup());
                            conds.reset(tmp);
                        }
                        else {
                            conds.reset(ttl[j].term->dup());
                        }
                    }
                    else {
                        LOGGER(ibis::gVerbose > 1)
                            << "Warning -- quaere::create discards condition "
                            << *(ttl[j].term) << " due to unknown name "
                            << *pit;
                    }
                }
                else if (ttl[j].tnames.size() == 2) { // two names in this term
                    pit = ttl[j].tnames.begin();
                    const char *tpr = pit->c_str();
                    ++ pit;
                    const char *tps = pit->c_str();
                    if (*tpr == 0) {
                        const char *tmp = tpr;
                        tpr = tps;
                        tps = tmp;
                    }
                    else if (fc.size() >= 2 && partr != parts) {
                        tpr = fc.realName(tpr);
                        tps = fc.realName(tps);
                    }

                    if (tps == 0 || *tps == 0) {
                        if (stricmp(tpr, pr) == 0 || stricmp(tpr, rpr) == 0) {
                            if (condr.get() != 0) {
                                ibis::qExpr *tmp =
                                    new ibis::qExpr(ibis::qExpr::LOGICAL_AND);
                                tmp->setLeft(condr.release());
                                tmp->setRight(ttl[j].term->dup());
                                condr.reset(tmp);
                            }
                            else {
                                condr.reset(ttl[j].term->dup());
                            }
                        }
                        else if (pr == ps || stricmp(tpr, ps) == 0 ||
                                 stricmp(tpr, rps) == 0) {
                            if (conds.get() != 0) {
                                ibis::qExpr *tmp =
                                    new ibis::qExpr(ibis::qExpr::LOGICAL_AND);
                                tmp->setLeft(conds.release());
                                tmp->setRight(ttl[j].term->dup());
                                conds.reset(tmp);
                            }
                            else {
                                conds.reset(ttl[j].term->dup());
                            }
                        }
                        else {
                            LOGGER(ibis::gVerbose >= 0)
                                << "Warning -- quaere::create encounters an "
                                "internal error, the where clause \"" << wh
                                << "\" is supposed to involve " << pr
                                << " and " << ps << ", but "
                                << *(ttl[j].term) << " involves table " << tpr;
                        }
                    }
                    else if (((stricmp(tpr, pr) == 0 ||
                               stricmp(tpr, rpr) == 0) &&
                              (stricmp(tps, ps) == 0 ||
                               stricmp(tps, rps) == 0)) ||
                             ((stricmp(tpr, ps) == 0 ||
                               stricmp(tpr, rps) == 0) &&
                              (stricmp(tps, pr) == 0 ||
                               stricmp(tps, rpr) == 0))) {
                        if (condj.get() != 0) {
                            ibis::qExpr *tmp =
                                new ibis::qExpr(ibis::qExpr::LOGICAL_AND);
                            tmp->setLeft(condj.release());
                            tmp->setRight(ttl[j].term->dup());
                            condj.reset(tmp);
                        }
                        else {
                            condj.reset(ttl[j].term->dup());
                        }
                    }
                    else {
                        LOGGER(ibis::gVerbose >= 0)
                            << "Warning -- quaere::create encounters an "
                            "internal error, the where clause \"" << wh
                            << "\" is supposed to involve " << pr
                            << " and " << ps << ", but "
                            << *(ttl[j].term) << " involves tables " << tpr
                            << " and " << tps;
                    }
                }
                else {
                    LOGGER(ibis::gVerbose >= 0)
                        << "Warning -- quaere::create encounters an internal "
                        "error, the where clause \"" << wh
                        << "\" to said to involve 2 tables overall, but "
                        "the condition " << *(ttl[j].term)
                        << " actually involves " << ttl[j].tnames.size();
                }
            } // for (j ...

            if (fc.getJoinCondition() != 0) {
                if (condj.get() != 0) {
                    ibis::qExpr *tmp =
                        new ibis::qExpr(ibis::qExpr::LOGICAL_AND);
                    tmp->setLeft(condj.release());
                    tmp->setRight(fc.getJoinCondition()->dup());
                    condj.reset(tmp);
                }
                else {
                    condj.reset(fc.getJoinCondition()->dup());
                }
            }
            if (condr.get() == 0 && conds.get() == 0 && condj.get() == 0) {
                LOGGER(ibis::gVerbose > 0)
                    << "Warning -- quaere::create(" << sql
                    << ") fails to extract any condition";
            }
            else if (condj.get() == 0) {
                if (partr == parts) {
                    // actually the same table
                    ibis::constPartList pl(1);
                    pl[0] = partr;
                    return new ibis::filter(&sc, &pl, &wc);
                }
                else {
                    LOGGER(ibis::gVerbose > 0)
                        << "Warning -- quaere::create(" << sql
                        << ") expects a join condition, but found none";
                }
            }
            else if (condj->getType() == ibis::qExpr::COMPRANGE) {
                const ibis::compRange &cr =
                    *static_cast<ibis::compRange*>(condj.get());
                if (cr.getLeft() != 0 && cr.getRight() != 0 &&
                    cr.getTerm3() == 0 &&
                    static_cast<const ibis::math::term*>
                    (cr.getLeft())->termType()
                    == ibis::math::VARIABLE &&
                    static_cast<const ibis::math::term*>
                    (cr.getRight())->termType()
                    == ibis::math::VARIABLE) {
                    const ibis::math::variable &varr =
                        *static_cast<const ibis::math::variable*>
                        (cr.getLeft());
                    const ibis::math::variable &vars =
                        *static_cast<const ibis::math::variable*>
                        (cr.getRight());
                    const std::string& tnr =
                        ibis::qExpr::extractTableName(varr.variableName());
                    const std::string& tns =
                        ibis::qExpr::extractTableName(vars.variableName());
                    if (stricmp(tnr.c_str(), pr) != 0 &&
                        stricmp(tnr.c_str(), rpr) != 0) { // swap names
                        ibis::part* tmpp = partr;
                        partr = parts;
                        parts = tmpp;
                        ibis::qExpr *tmpq = condr.release();
                        condr = std::move(conds);
                        conds.reset(tmpq);
                        fc.reorderNames(tnr.c_str(), tns.c_str());
                    }
                    ibis::column *colr = partr->getColumn(varr.variableName());
                    if (colr == 0) {
                        LOGGER(ibis::gVerbose >= 0)
                            << "Warning -- quaere::create(" << sql
                            << ") can't find a column named "
                            << varr.variableName() << " in data partition "
                            << partr->name() << " (" << pr << ")";
                        return 0;
                    }
                    ibis::column *cols = parts->getColumn(vars.variableName());
                    if (cols == 0) {
                        LOGGER(ibis::gVerbose >= 0)
                            << "Warning -- quaere::create(" << sql
                            << ") can't find a column named "
                            << vars.variableName() << " in data partition "
                            << parts->name() << " (" << ps << ")";
                        return 0;
                    }
                    return new ibis::jNatural(partr, parts, colr, cols,
                                              condr.get(), conds.get(),
                                              &sc, &fc, sql.c_str());
                }
                else if (cr.getLeft() != 0 && cr.getRight() != 0 &&
                         cr.getTerm3() != 0 &&
                         static_cast<const ibis::math::term*>
                         (cr.getLeft())->termType()
                         == ibis::math::OPERATOR &&
                         static_cast<const ibis::math::term*>
                         (cr.getRight())->termType()
                         == ibis::math::VARIABLE &&
                         static_cast<const ibis::math::term*>
                         (cr.getTerm3())->termType()
                         == ibis::math::OPERATOR &&
                         ((static_cast<const ibis::math::term*>
                           (cr.getLeft()->getLeft())->termType()
                           == ibis::math::VARIABLE &&
                           static_cast<const ibis::math::term*>
                           (cr.getLeft()->getRight())->termType()
                           == ibis::math::NUMBER) ||
                          (static_cast<const ibis::math::term*>
                           (cr.getLeft()->getLeft())->termType()
                           == ibis::math::NUMBER &&
                           static_cast<const ibis::math::term*>
                           (cr.getLeft()->getRight())->termType()
                           == ibis::math::VARIABLE)) &&
                         ((static_cast<const ibis::math::term*>
                           (cr.getTerm3()->getLeft())->termType()
                           == ibis::math::VARIABLE &&
                           static_cast<const ibis::math::term*>
                           (cr.getTerm3()->getRight())->termType()
                           == ibis::math::NUMBER) ||
                          (static_cast<const ibis::math::term*>
                           (cr.getTerm3()->getLeft())->termType()
                           == ibis::math::NUMBER &&
                           static_cast<const ibis::math::term*>
                           (cr.getTerm3()->getRight())->termType()
                           == ibis::math::VARIABLE))) {
                    // a.c between b.d+delta1 and b.d+delta2
                    const ibis::math::variable &varr1 =
                        *static_cast<const ibis::math::variable*>
                        ((static_cast<const ibis::math::term*>
                          (cr.getLeft()->getLeft())->termType()
                          == ibis::math::VARIABLE) ?
                         cr.getLeft()->getLeft() :
                         cr.getLeft()->getRight());
                    const ibis::math::variable &varr2 =
                        *static_cast<const ibis::math::variable*>
                        ((static_cast<const ibis::math::term*>
                          (cr.getTerm3()->getLeft())->termType()
                          == ibis::math::VARIABLE) ?
                         cr.getTerm3()->getLeft() :
                         cr.getTerm3()->getRight());
                    if (varr1.variableName() != varr2.variableName() &&
                        stricmp(varr1.variableName(), varr2.variableName())
                        != 0) {
                        // the two column names must be equivalent
                        const char* str1 = varr1.variableName();
                        const char* str2 = varr2.variableName();
                        const char* ptr1 = strchr(str1, '.');
                        const char* ptr2 = strchr(str2, '.');
                        if (ptr1 < str1 || ptr2 < str2) {
                            return 0;
                        }
                        std::string p1, p2;
                        while (str1 < ptr1) {
                            p1 += tolower(*str1);
                            ++ str1;
                        }
                        while (str2 < ptr2) {
                            p2 += tolower(*str2);
                            ++ str2;
                        }
                        for (++ ptr1, ++ ptr2; *ptr1 != 0 && *ptr2 != 0;
                             ++ ptr1, ++ ptr2) {
                            if (*ptr1 != *ptr2 &&
                                toupper(*ptr1) != toupper(*ptr2)) {
                                LOGGER(ibis::gVerbose >= 0)
                                    << "Warning -- quaere::create(" << sql
                                    << ") expects same column names, but got \""
                                    << varr1.variableName() << "\" and \""
                                    << varr2.variableName() << "\"";
                                return 0;
                            }
                        }
                        if (*ptr1 != 0 || *ptr1 != 0) {
                            LOGGER(ibis::gVerbose >= 0)
                                << "Warning -- quaere::create(" << sql
                                << ") expects same column names, but got \""
                                << varr1.variableName() << "\" and \""
                                << varr2.variableName() << "\"";
                            return 0;
                        }
                        if (p1.size() != p2.size() || p1.compare(p2) != 0) {
                            ptr1 = fc.realName(p1.c_str());
                            ptr2 = fc.realName(p2.c_str());
                            if (stricmp(ptr1, ptr2) != 0) {
                                LOGGER(ibis::gVerbose >= 0)
                                    << "Warning -- quaere::create(" << sql
                                    << ") expects same column names, but got \""
                                    << varr1.variableName() << "\" and \""
                                    << varr2.variableName() << "\"";
                                return 0;
                            }
                        }
                    }
                    const ibis::math::variable &vars =
                        *static_cast<const ibis::math::variable*>
                        (cr.getRight());
                    const std::string& tnr =
                        ibis::qExpr::extractTableName(varr1.variableName());
                    const std::string& tns =
                        ibis::qExpr::extractTableName(vars.variableName());
                    if (stricmp(tnr.c_str(), pr) != 0 &&
                        stricmp(tnr.c_str(), rpr) != 0) { // swap
                        ibis::part* tmpp = partr;
                        partr = parts;
                        parts = tmpp;
                        ibis::qExpr *tmpq = condr.release();
                        condr = std::move(conds);
                        conds.reset(tmpq);
                        fc.reorderNames(tnr.c_str(), tns.c_str());
                    }

                    ibis::column *colr = partr->getColumn(varr1.variableName());
                    if (colr == 0) {
                        LOGGER(ibis::gVerbose >= 0)
                            << "Warning -- quaere::create(" << sql
                            << ") can't find a column named "
                            << varr1.variableName() << " in data partition "
                            << partr->name() << " (" << pr << ")";
                        return 0;
                    }
                    ibis::column *cols = parts->getColumn(vars.variableName());
                    if (cols == 0) {
                        LOGGER(ibis::gVerbose >= 0)
                            << "Warning -- quaere::create(" << sql
                            << ") can't find a column named "
                            << vars.variableName() << " in data partition "
                            << parts->name() << " (" << ps << ")";
                        return 0;
                    }

                    ibis::math::barrel bar;
                    bar.recordVariable(&varr1);
                    bar.recordVariable(&varr2);
                    double delta1 = static_cast<const ibis::math::term*>
                        (cr.getLeft())->eval();
                    double delta2 = static_cast<const ibis::math::term*>
                        (cr.getTerm3())->eval();
                    if ((cr.leftOperator() == ibis::qExpr::OP_LE ||
                         cr.leftOperator() == ibis::qExpr::OP_LT) &&
                        (cr.rightOperator() == ibis::qExpr::OP_LE ||
                         cr.rightOperator() == ibis::qExpr::OP_LT)) {
                        if (cr.leftOperator() == ibis::qExpr::OP_LT)
                            delta1 = ibis::util::incrDouble(delta1);
                        if (cr.rightOperator() == ibis::qExpr::OP_LT)
                            delta2 = ibis::util::decrDouble(delta2);
                    }
                    else if ((cr.leftOperator() == ibis::qExpr::OP_GE ||
                              cr.leftOperator() == ibis::qExpr::OP_GT) &&
                             (cr.rightOperator() == ibis::qExpr::OP_GE ||
                              cr.rightOperator() == ibis::qExpr::OP_GT)) {
                        if (cr.leftOperator() == ibis::qExpr::OP_GT)
                            delta1 = ibis::util::decrDouble(delta1);
                        if (cr.rightOperator() == ibis::qExpr::OP_GT)
                            delta2 = ibis::util::incrDouble(delta2);
                        double tmp = delta1;
                        delta1 = delta2;
                        delta2 = tmp;
                    }
                    else {
                        LOGGER(ibis::gVerbose >= 0)
                            << "Warning -- quaere::create(" << sql
                            << ") can't handle join condition \"" << cr
                            << "\"";
                        return 0;
                    }

                    return new ibis::jRange(*parts, *partr, *cols, *colr,
                                            delta1, delta2, conds.get(),
                                            condr.get(), &sc, &fc, sql.c_str());
                }
                else if (cr.getLeft() != 0 && cr.getRight() != 0 &&
                         cr.getTerm3() != 0 &&
                         static_cast<const ibis::math::term*>
                         (cr.getLeft())->termType()
                         == ibis::math::VARIABLE &&
                         static_cast<const ibis::math::term*>
                         (cr.getRight())->termType()
                         == ibis::math::VARIABLE &&
                         static_cast<const ibis::math::term*>
                         (cr.getTerm3())->termType()
                         == ibis::math::OPERATOR &&
                         ((static_cast<const ibis::math::term*>
                           (cr.getTerm3()->getLeft())->termType()
                           == ibis::math::VARIABLE &&
                           static_cast<const ibis::math::term*>
                           (cr.getTerm3()->getRight())->termType()
                           == ibis::math::NUMBER) ||
                          (static_cast<const ibis::math::term*>
                           (cr.getTerm3()->getLeft())->termType()
                           == ibis::math::NUMBER &&
                           static_cast<const ibis::math::term*>
                           (cr.getTerm3()->getRight())->termType()
                           == ibis::math::VARIABLE))) {
                    // a.c between b.d and b.d+delta2 (delta1=0)
                    const ibis::math::variable &varr1 =
                        *static_cast<const ibis::math::variable*>
                        (cr.getLeft());
                    const ibis::math::variable &varr2 =
                        *static_cast<const ibis::math::variable*>
                        ((static_cast<const ibis::math::term*>
                          (cr.getTerm3()->getLeft())->termType()
                          == ibis::math::VARIABLE) ?
                         cr.getTerm3()->getLeft() :
                         cr.getTerm3()->getRight());
                    if (varr1.variableName() != varr2.variableName() &&
                        stricmp(varr1.variableName(), varr2.variableName())
                        != 0) {
                        // the two column names must be equivalent
                        const char* str1 = varr1.variableName();
                        const char* str2 = varr2.variableName();
                        const char* ptr1 = strchr(str1, '.');
                        const char* ptr2 = strchr(str2, '.');
                        if (ptr1 < str1 || ptr2 < str2) {
                            return 0;
                        }
                        std::string p1, p2;
                        while (str1 < ptr1) {
                            p1 += tolower(*str1);
                            ++ str1;
                        }
                        while (str2 < ptr2) {
                            p2 += tolower(*str2);
                            ++ str2;
                        }
                        for (++ ptr1, ++ ptr2; *ptr1 != 0 && *ptr2 != 0;
                             ++ ptr1, ++ ptr2) {
                            if (*ptr1 != *ptr2 &&
                                toupper(*ptr1) != toupper(*ptr2)) {
                                LOGGER(ibis::gVerbose >= 0)
                                    << "Warning -- quaere::create(" << sql
                                    << ") expects same column names, but got \""
                                    << varr1.variableName() << "\" and \""
                                    << varr2.variableName() << "\"";
                                return 0;
                            }
                        }
                        if (*ptr1 != 0 || *ptr1 != 0) {
                            LOGGER(ibis::gVerbose >= 0)
                                << "Warning -- quaere::create(" << sql
                                << ") expects same column names, but got \""
                                << varr1.variableName() << "\" and \""
                                << varr2.variableName() << "\"";
                            return 0;
                        }
                        if (p1.size() != p2.size() || p1.compare(p2) != 0) {
                            ptr1 = fc.realName(p1.c_str());
                            ptr2 = fc.realName(p2.c_str());
                            if (stricmp(ptr1, ptr2) != 0) {
                                LOGGER(ibis::gVerbose >= 0)
                                    << "Warning -- quaere::create(" << sql
                                    << ") expects same column names, but got \""
                                    << varr1.variableName() << "\" and \""
                                    << varr2.variableName() << "\"";
                                return 0;
                            }
                        }
                    }
                    const ibis::math::variable &vars =
                        *static_cast<const ibis::math::variable*>
                        (cr.getRight());
                    const std::string& tnr =
                        ibis::qExpr::extractTableName(varr1.variableName());
                    const std::string& tns =
                        ibis::qExpr::extractTableName(vars.variableName());
                    if (stricmp(tnr.c_str(), pr) != 0 &&
                        stricmp(tnr.c_str(), rpr) != 0) { // swap
                        ibis::part* tmpp = partr;
                        partr = parts;
                        parts = tmpp;
                        ibis::qExpr *tmpq = condr.release();
                        condr = std::move(conds);
                        conds.reset(tmpq);
                        fc.reorderNames(tnr.c_str(), tns.c_str());
                    }

                    ibis::column *colr = partr->getColumn(varr1.variableName());
                    if (colr == 0) {
                        LOGGER(ibis::gVerbose >= 0)
                            << "Warning -- quaere::create(" << sql
                            << ") can't find a column named "
                            << varr1.variableName() << " in data partition "
                            << partr->name() << " (" << pr << ")";
                        return 0;
                    }
                    ibis::column *cols = parts->getColumn(vars.variableName());
                    if (cols == 0) {
                        LOGGER(ibis::gVerbose >= 0)
                            << "Warning -- quaere::create(" << sql
                            << ") can't find a column named "
                            << vars.variableName() << " in data partition "
                            << parts->name() << " (" << ps << ")";
                        return 0;
                    }

                    ibis::math::barrel bar;
                    bar.recordVariable(&varr2);
                    double delta1 = 0;
                    double delta2 = static_cast<const ibis::math::term*>
                        (cr.getTerm3())->eval();
                    if ((cr.leftOperator() == ibis::qExpr::OP_LE ||
                         cr.leftOperator() == ibis::qExpr::OP_LT) &&
                        (cr.rightOperator() == ibis::qExpr::OP_LE ||
                         cr.rightOperator() == ibis::qExpr::OP_LT)) {
                        if (cr.leftOperator() == ibis::qExpr::OP_LT)
                            delta1 = ibis::util::incrDouble(delta1);
                        if (cr.rightOperator() == ibis::qExpr::OP_LT)
                            delta2 = ibis::util::decrDouble(delta2);
                    }
                    else if ((cr.leftOperator() == ibis::qExpr::OP_GE ||
                              cr.leftOperator() == ibis::qExpr::OP_GT) &&
                             (cr.rightOperator() == ibis::qExpr::OP_GE ||
                              cr.rightOperator() == ibis::qExpr::OP_GT)) {
                        if (cr.leftOperator() == ibis::qExpr::OP_GT)
                            delta1 = ibis::util::decrDouble(delta1);
                        if (cr.rightOperator() == ibis::qExpr::OP_GT)
                            delta2 = ibis::util::incrDouble(delta2);
                        double tmp = delta1;
                        delta1 = delta2;
                        delta2 = tmp;
                    }
                    else {
                        LOGGER(ibis::gVerbose >= 0)
                            << "Warning -- quaere::create(" << sql
                            << ") can't handle join condition \"" << cr
                            << "\"";
                        return 0;
                    }

                    return new ibis::jRange(*parts, *partr, *cols, *colr,
                                            delta1, delta2, conds.get(),
                                            condr.get(), &sc, &fc, sql.c_str());
                }
                else if (cr.getLeft() != 0 && cr.getRight() != 0 &&
                         cr.getTerm3() != 0 &&
                         static_cast<const ibis::math::term*>
                         (cr.getLeft())->termType()
                         == ibis::math::OPERATOR &&
                         static_cast<const ibis::math::term*>
                         (cr.getRight())->termType()
                         == ibis::math::VARIABLE &&
                         static_cast<const ibis::math::term*>
                         (cr.getTerm3())->termType()
                         == ibis::math::VARIABLE &&
                         ((static_cast<const ibis::math::term*>
                           (cr.getLeft()->getLeft())->termType()
                           == ibis::math::VARIABLE &&
                           static_cast<const ibis::math::term*>
                           (cr.getLeft()->getRight())->termType()
                           == ibis::math::NUMBER) ||
                          (static_cast<const ibis::math::term*>
                           (cr.getLeft()->getLeft())->termType()
                           == ibis::math::NUMBER &&
                           static_cast<const ibis::math::term*>
                           (cr.getLeft()->getRight())->termType()
                           == ibis::math::VARIABLE))) {
                    // a.c between b.d+delta1 and b.d (delta2 = 0)
                    const ibis::math::variable &varr1 =
                        *static_cast<const ibis::math::variable*>
                        ((static_cast<const ibis::math::term*>
                          (cr.getLeft()->getLeft())->termType()
                          == ibis::math::VARIABLE) ?
                         cr.getLeft()->getLeft() :
                         cr.getLeft()->getRight());
                    const ibis::math::variable &varr2 =
                        *static_cast<const ibis::math::variable*>
                        (cr.getTerm3());
                    if (varr1.variableName() != varr2.variableName() &&
                        stricmp(varr1.variableName(), varr2.variableName())
                        != 0) {
                        // the two column names must be equivalent
                        const char* str1 = varr1.variableName();
                        const char* str2 = varr2.variableName();
                        const char* ptr1 = strchr(str1, '.');
                        const char* ptr2 = strchr(str2, '.');
                        if (ptr1 < str1 || ptr2 < str2) {
                            return 0;
                        }
                        std::string p1, p2;
                        while (str1 < ptr1) {
                            p1 += tolower(*str1);
                            ++ str1;
                        }
                        while (str2 < ptr2) {
                            p2 += tolower(*str2);
                            ++ str2;
                        }
                        for (++ ptr1, ++ ptr2; *ptr1 != 0 && *ptr2 != 0;
                             ++ ptr1, ++ ptr2) {
                            if (*ptr1 != *ptr2 &&
                                toupper(*ptr1) != toupper(*ptr2)) {
                                LOGGER(ibis::gVerbose >= 0)
                                    << "Warning -- quaere::create(" << sql
                                    << ") expects same column names, but got \""
                                    << varr1.variableName() << "\" and \""
                                    << varr2.variableName() << "\"";
                                return 0;
                            }
                        }
                        if (*ptr1 != 0 || *ptr1 != 0) {
                            LOGGER(ibis::gVerbose >= 0)
                                << "Warning -- quaere::create(" << sql
                                << ") expects same column names, but got \""
                                << varr1.variableName() << "\" and \""
                                << varr2.variableName() << "\"";
                            return 0;
                        }
                        if (p1.size() != p2.size() || p1.compare(p2) != 0) {
                            ptr1 = fc.realName(p1.c_str());
                            ptr2 = fc.realName(p2.c_str());
                            if (stricmp(ptr1, ptr2) != 0) {
                                LOGGER(ibis::gVerbose >= 0)
                                    << "Warning -- quaere::create(" << sql
                                    << ") expects same column names, but got \""
                                    << varr1.variableName() << "\" and \""
                                    << varr2.variableName() << "\"";
                                return 0;
                            }
                        }
                    }
                    const ibis::math::variable &vars =
                        *static_cast<const ibis::math::variable*>
                        (cr.getRight());
                    const std::string& tnr =
                        ibis::qExpr::extractTableName(varr1.variableName());
                    const std::string& tns =
                        ibis::qExpr::extractTableName(vars.variableName());
                    if (stricmp(tnr.c_str(), pr) != 0 &&
                        stricmp(tnr.c_str(), rpr) != 0) { // swap
                        ibis::part* tmpp = partr;
                        partr = parts;
                        parts = tmpp;
                        ibis::qExpr *tmpq = condr.release();
                        condr = std::move(conds);
                        conds.reset(tmpq);
                        fc.reorderNames(tnr.c_str(), tns.c_str());
                    }

                    ibis::column *colr = partr->getColumn(varr1.variableName());
                    if (colr == 0) {
                        LOGGER(ibis::gVerbose >= 0)
                            << "Warning -- quaere::create(" << sql
                            << ") can't find a column named "
                            << varr1.variableName() << " in data partition "
                            << partr->name() << " (" << pr << ")";
                        return 0;
                    }
                    ibis::column *cols = parts->getColumn(vars.variableName());
                    if (cols == 0) {
                        LOGGER(ibis::gVerbose >= 0)
                            << "Warning -- quaere::create(" << sql
                            << ") can't find a column named "
                            << vars.variableName() << " in data partition "
                            << parts->name() << " (" << ps << ")";
                        return 0;
                    }

                    ibis::math::barrel bar;
                    bar.recordVariable(&varr1);
                    double delta1 = static_cast<const ibis::math::term*>
                        (cr.getLeft())->eval();
                    double delta2 = 0.0;
                    if ((cr.leftOperator() == ibis::qExpr::OP_LE ||
                         cr.leftOperator() == ibis::qExpr::OP_LT) &&
                        (cr.rightOperator() == ibis::qExpr::OP_LE ||
                         cr.rightOperator() == ibis::qExpr::OP_LT)) {
                        if (cr.leftOperator() == ibis::qExpr::OP_LT)
                            delta1 = ibis::util::incrDouble(delta1);
                        if (cr.rightOperator() == ibis::qExpr::OP_LT)
                            delta2 = ibis::util::decrDouble(delta2);
                    }
                    else if ((cr.leftOperator() == ibis::qExpr::OP_GE ||
                              cr.leftOperator() == ibis::qExpr::OP_GT) &&
                             (cr.rightOperator() == ibis::qExpr::OP_GE ||
                              cr.rightOperator() == ibis::qExpr::OP_GT)) {
                        if (cr.leftOperator() == ibis::qExpr::OP_GT)
                            delta1 = ibis::util::decrDouble(delta1);
                        if (cr.rightOperator() == ibis::qExpr::OP_GT)
                            delta2 = ibis::util::incrDouble(delta2);
                        double tmp = delta1;
                        delta1 = delta2;
                        delta2 = tmp;
                    }
                    else {
                        LOGGER(ibis::gVerbose >= 0)
                            << "Warning -- quaere::create(" << sql
                            << ") can't handle join condition \"" << cr
                            << "\"";
                        return 0;
                    }

                    return new ibis::jRange(*parts, *partr, *cols, *colr,
                                            delta1, delta2, conds.get(),
                                            condr.get(), &sc, &fc, sql.c_str());
                }
                else if (cr.getLeft() != 0 && cr.getRight() != 0 &&
                         cr.getTerm3() != 0 &&
                         static_cast<const ibis::math::term*>
                         (cr.getLeft())->termType()
                         == ibis::math::NUMBER &&
                         static_cast<const ibis::math::term*>
                         (cr.getRight())->termType()
                         == ibis::math::OPERATOR &&
                         static_cast<const ibis::math::term*>
                         (cr.getTerm3())->termType()
                         == ibis::math::NUMBER &&
                         static_cast<const ibis::math::term*>
                         (cr.getRight()->getLeft())->termType()
                         == ibis::math::VARIABLE &&
                         static_cast<const ibis::math::term*>
                         (cr.getRight()->getRight())->termType()
                         == ibis::math::VARIABLE &&
                         static_cast<const ibis::math::bediener*>
                         (cr.getRight())->getOperator()
                         == ibis::math::MINUS) {
                    // delta1 <= a.c-b.d <= delta2
                    const ibis::math::variable &varr =
                        *static_cast<const ibis::math::variable*>
                        (cr.getRight()->getLeft());
                    const ibis::math::variable &vars =
                        *static_cast<const ibis::math::variable*>
                        (cr.getRight()->getRight());

                    const std::string& tnr =
                        ibis::qExpr::extractTableName(varr.variableName());
                    const std::string& tns =
                        ibis::qExpr::extractTableName(vars.variableName());
                    if (stricmp(tnr.c_str(), pr) != 0 &&
                        stricmp(tnr.c_str(), rpr) != 0) { // swap _r and _s
                        ibis::part* tmpp = partr;
                        partr = parts;
                        parts = tmpp;
                        ibis::qExpr *tmpq = condr.release();
                        condr = std::move(conds);
                        conds.reset(tmpq);
                        fc.reorderNames(tnr.c_str(), tns.c_str());
                    }
                    ibis::column *colr = partr->getColumn(varr.variableName());
                    if (colr == 0) {
                        LOGGER(ibis::gVerbose >= 0)
                            << "Warning -- quaere::create(" << sql
                            << ") can't find a column named "
                            << varr.variableName() << " in data partition "
                            << partr->name() << " (" << pr << ")";
                        return 0;
                    }
                    ibis::column *cols = parts->getColumn(vars.variableName());
                    if (cols == 0) {
                        LOGGER(ibis::gVerbose >= 0)
                            << "Warning -- quaere::create(" << sql
                            << ") can't find a column named "
                            << vars.variableName() << " in data partition "
                            << parts->name() << " (" << ps << ")";
                        return 0;
                    }

                    double delta1 = static_cast<const ibis::math::term*>
                        (cr.getLeft())->eval();
                    double delta2 = static_cast<const ibis::math::term*>
                        (cr.getTerm3())->eval();
                    if ((cr.leftOperator() == ibis::qExpr::OP_LE ||
                         cr.leftOperator() == ibis::qExpr::OP_LT) &&
                        (cr.rightOperator() == ibis::qExpr::OP_LE ||
                         cr.rightOperator() == ibis::qExpr::OP_LT)) {
                        if (cr.leftOperator() == ibis::qExpr::OP_LT)
                            delta1 = ibis::util::incrDouble(delta1);
                        if (cr.rightOperator() == ibis::qExpr::OP_LT)
                            delta2 = ibis::util::decrDouble(delta2);
                    }
                    else if ((cr.leftOperator() == ibis::qExpr::OP_GE ||
                              cr.leftOperator() == ibis::qExpr::OP_GT) &&
                             (cr.rightOperator() == ibis::qExpr::OP_GE ||
                              cr.rightOperator() == ibis::qExpr::OP_GT)) {
                        if (cr.leftOperator() == ibis::qExpr::OP_GT)
                            delta1 = ibis::util::decrDouble(delta1);
                        if (cr.rightOperator() == ibis::qExpr::OP_GT)
                            delta2 = ibis::util::incrDouble(delta2);
                        double tmp = delta1;
                        delta1 = delta2;
                        delta2 = tmp;
                    }
                    else {
                        LOGGER(ibis::gVerbose >= 0)
                            << "Warning -- quaere::create(" << sql
                            << ") can't handle join condition \"" << cr
                            << "\"";
                        return 0;
                    }

                    return new ibis::jRange(*partr, *parts, *colr, *cols,
                                            delta1, delta2, condr.get(),
                                            conds.get(), &sc, &fc, sql.c_str());
                }
                else {
                    LOGGER(ibis::gVerbose > 0)
                        << "Warning -- quaere::create(" << sql
                        << ") can not handle join expression \"" << *condj
                        << "\" yet.";
                    return 0;
                }
            }
            else {
                LOGGER(ibis::gVerbose >= 0)
                    << "Warning -- quaere::create(" << sql
                    << ") connot process join with multiple conditions yet";
            }
        }
        else { // more than two tables
            LOGGER(ibis::gVerbose >= 0)
                << "Warning -- quaere::create(" << sql
                << ") does not work with more than two tables";
        }
    }
    catch (std::exception &e) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- quaere::create(" << sql
            << ") failed due to an exception -- " << e.what();
    }
    catch (const char* s) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- quaere::create(" << sql
            << ") failed due to an exception -- " << s;
    }
    catch (...) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- quaere::create(" << sql
            << ") failed due to an unexpected exception";
    }
    return 0;
} // ibis::quaere::create

/// Specify a natural join operation.  This is equivalent to SQL statement
///
/// "From partr Join parts Using(colname) Where condr And conds"
///
/// @note Conditions specified in condr is for partr only, and conds is for
/// parts only.  If no conditions are specified, all valid records in the
/// partition will participate in the natural join.
///
/// @note The select clause should have fully qualified column names.
/// Unqualified column names will assumed to be searched in partr first and
/// then in parts.
ibis::quaere*
ibis::quaere::create(const ibis::part* partr, const ibis::part* parts,
                     const char* colname, const char* condr,
                     const char* conds, const char* sel) {
    return new ibis::jNatural(partr, parts, colname, condr, conds, sel);
} // ibis::quaere::create

/// Find a dataset with the given name.  If the named data partition is
/// found, a point to the data partition is returned, otherwise, a nil
/// pointer is returned.  If the name is nil, a nil pointer will be
/// returned.
ibis::part* ibis::findDataset(const char* pn) {
    if (pn == 0 || *pn == 0) return 0;

    static ibis::partAssoc ordered_;
    { // a scope to limit the mutex lock
        ibis::util::mutexLock lock(&ibis::util::envLock, "findDataset");
        if (ordered_.size() != ibis::datasets.size()) {
            ordered_.clear();
            for (size_t j = 0; j < ibis::datasets.size(); ++ j)
                ordered_[ibis::datasets[j]->name()] = ibis::datasets[j];
        }
    }

    ibis::partAssoc::iterator it = ordered_.find(pn);
    if (it != ordered_.end()) {
        return it->second;
    }
    else {
        return 0;
    }
} // ibis::findDataset

/// Find a dataset with the given name among the given list.  It performs a
/// linear search.
ibis::part* ibis::findDataset(const char* pn, const ibis::partList& prts) {
    if (&prts == &ibis::datasets)
        return ibis::findDataset(pn);
    if (pn == 0 || *pn == 0)
        return 0;

    for (ibis::partList::const_iterator it = prts.begin();
         it != prts.end(); ++ it) {
        if (stricmp((*it)->name(), pn) == 0)
            return *it;
    }
    return 0;
} // ibis::findDataset

