// $Id$
// Author: John Wu <John.Wu at ACM.org>
// Copyright (c) 2004-2016 the Regents of the University of California
//
// This file implements a number of self join functions of the ibis::part
// class.  These functions are separated from the regular query evaluation
// functions becase they do not following the general pattern of the other
// query conditions.  Need to redesign the query class to provide a more
// uniform processing of queries.
#include "part.h"
#include "qExpr.h"
#include "iroster.h"
#include "bitvector64.h"
#include <sstream> // std::ostringstream

int64_t ibis::part::evaluateJoin(const ibis::deprecatedJoin& cmp,
                                  const ibis::bitvector64& trial,
                                  ibis::bitvector64& pairs) const {
    int64_t cnt;
    if (trial.cnt() == 0) {
        cnt = 0;
        pairs.set(0, trial.size());
    }
    else if (cmp.getRange() == 0) {
        cnt = equiJoin(cmp, trial, pairs);
    }
    else if (cmp.getRange()->termType() == ibis::math::NUMBER) {
        const double delta = fabs(cmp.getRange()->eval());
        if (delta > 0)
            cnt = deprecatedJoin(cmp, trial, pairs);
        else
            cnt = equiJoin(cmp, trial, pairs);
    }
    else {
        ibis::math::barrel bar(cmp.getRange());
        if (bar.size() == 0) { // a constant function
            const double delta = fabs(cmp.getRange()->eval());
            if (delta > 0)
                cnt = deprecatedJoin(cmp, trial, pairs);
            else
                cnt = equiJoin(cmp, trial, pairs);
        }
        else {
            cnt = compJoin(cmp, trial, pairs);
        }
    }
    return cnt;
} // ibis::part::evaluateJoin

/// Use the nested-loop join algorithm.  This is a front end that decide
/// which of the lower level routines to call.
int64_t ibis::part::loopJoin(const ibis::deprecatedJoin& cmp,
                              const ibis::bitvector& mask,
                              ibis::bitvector64& pairs) const {
    unsigned nvar = 0; // number of variables involved in @c cmp.
    pairs.clear();
    if (cmp.getRange()) { // use a ibis::math::barrel to count variables
        ibis::math::barrel bar;
        bar.recordVariable(cmp.getName1());
        bar.recordVariable(cmp.getName2());
        bar.recordVariable(cmp.getRange());
        nvar = bar.size();
    }
    else if (stricmp(cmp.getName1(), cmp.getName2()) != 0) {
        nvar = 2;
    }
    else {
        nvar = 1;
    }
    nvar *= 8;

    int64_t cnt = ibis::fileManager::instance().bytesFree() -
        static_cast<int64_t>(nEvents) * nvar;
    bool equijoin = false;
    if (cnt > 0) {
        if (cmp.getRange() == 0) {// equi-join
            cnt = equiJoinLoop1(cmp, mask, pairs);
            equijoin = true;
        }
        else if (cmp.getRange()->termType() == ibis::math::NUMBER) {
            const double delta = fabs(cmp.getRange()->eval());
            if (delta > 0) {
                cnt = deprecatedJoinLoop(cmp, mask, pairs);
                equijoin = false;
            }
            else {
                cnt = equiJoinLoop1(cmp, mask, pairs);
                equijoin = true;
            }
        }
        else {
            ibis::math::barrel bar(cmp.getRange());
            if (bar.size() == 0) { // a constant function
                const double delta = fabs(cmp.getRange()->eval());
                if (delta > 0) {
                    cnt = deprecatedJoinLoop(cmp, mask, pairs);
                    equijoin = false;
                }
                else {
                    cnt = equiJoinLoop1(cmp, mask, pairs);
                    equijoin = true;
                }
            }
            else {
                cnt = compJoinLoop(cmp, mask, pairs);
                equijoin = false;
            }
        }
    }
    else {
        cnt = -1;
    }

    if (cnt < 0) { // try to directly read the files
        if (equijoin)
            cnt = equiJoinLoop2(cmp, mask, pairs);
        else
            cnt = compJoinLoop(cmp, mask, pairs);
    }
    return cnt;
} // ibis::part::loopJoin

int64_t ibis::part::loopJoin(const ibis::deprecatedJoin& cmp,
                             const ibis::bitvector& mask) const {
    unsigned nvar = 0; // number of variables involved in @c cmp.
    if (cmp.getRange()) { // use a ibis::math::barrel to count variables
        ibis::math::barrel bar;
        bar.recordVariable(cmp.getName1());
        bar.recordVariable(cmp.getName2());
        bar.recordVariable(cmp.getRange());
        nvar = bar.size();
    }
    else if (stricmp(cmp.getName1(), cmp.getName2()) != 0) {
        nvar = 2;
    }
    else {
        nvar = 1;
    }
    nvar *= 8;

    int64_t cnt = ibis::fileManager::instance().bytesFree() -
        static_cast<int64_t>(nEvents) * nvar;
    bool equijoin = false;
    if (cnt > 0) {
        if (cmp.getRange() == 0) {// equi-join
            cnt = equiJoinLoop1(cmp, mask);
            equijoin = true;
        }
        else if (cmp.getRange()->termType() == ibis::math::NUMBER) {
            const double delta = fabs(cmp.getRange()->eval());
            if (delta > 0) {
                cnt = deprecatedJoinLoop(cmp, mask);
                equijoin = false;
            }
            else {
                cnt = equiJoinLoop1(cmp, mask);
                equijoin = true;
            }
        }
        else {
            ibis::math::barrel bar(cmp.getRange());
            if (bar.size() == 0) {
                const double delta = fabs(cmp.getRange()->eval());
                if (delta > 0) {
                    cnt = deprecatedJoinLoop(cmp, mask);
                    equijoin = false;
                }
                else {
                    cnt = equiJoinLoop1(cmp, mask);
                    equijoin = true;
                }
            }
            else {
                cnt = compJoinLoop(cmp, mask);
                equijoin = false;
            }
        }
    }
    else {
        cnt = -1;
    }

    if (cnt < 0) { // try to directly read the files
        if (equijoin)
            cnt = equiJoinLoop2(cmp, mask);
        else
            cnt = compJoinLoop(cmp, mask);
    }
    return cnt;
} // ibis::part::loopJoin

/// Check a set of pairs defined in @c trial.
int64_t ibis::part::equiJoin(const ibis::deprecatedJoin& cmp,
                              const ibis::bitvector64& trial,
                              ibis::bitvector64& result) const {
    const ibis::bitvector64::word_t nbits =
        static_cast<ibis::bitvector64::word_t>(nEvents)*nEvents;
    ibis::horometer timer;
    timer.start();
    result.clear();

    if (trial.size() > nbits) {
        std::ostringstream ostr;
        ostr << "expect it to have " << nbits << " bits, but it actually has "
             << trial.size();
        logWarning("equiJoin", "invalid trial vector, %s",
                   ostr.str().c_str());
        return -3;
    }

    ibis::part::barrel bar1(this), bar2(this);
    const uint32_t varind1 = bar1.recordVariable(cmp.getName1());
    const uint32_t varind2 = bar2.recordVariable(cmp.getName2());

    long ierr = bar1.open();
    if (ierr != 0) {
        logWarning("equiJoin", "failed to open variable %s",
                   cmp.getName1());
        return -1;
    }
    ierr = bar2.open();
    if (ierr != 0) {
        logWarning("equiJoin", "failed to open variable %s",
                   cmp.getName2());
        return -2;
    }

    for (ibis::bitvector64::indexSet ix = trial.firstIndexSet();
         ix.nIndices() > 0; ++ ix) {
        const ibis::bitvector64::word_t *ind = ix.indices();
        uint32_t ir = static_cast<uint32_t>(*ind / nEvents);
        ierr = bar1.seek(ir);
        if (ierr < 0) {
            logWarning("equiJoin", "failed to seek to row %lu for the left "
                       "side of join (nEvents = %lu)",
                       static_cast<long unsigned>(ir),
                       static_cast<long unsigned>(nEvents));
            break; // no more iterations
        }
        bar1.read();

        if (ix.isRange()) { // ix is a range
            uint32_t is = static_cast<uint32_t>
                (*ind - static_cast<ibis::bitvector64::word_t>(ir) * nEvents);
            ierr = bar2.seek(is);
            if (ierr < 0) {
                logWarning("equiJoin", "faild to seek to row %lu for the "
                           "right side of join (nEvents = %lu)",
                           static_cast<long unsigned>(is),
                           static_cast<long unsigned>(nEvents));
                break;
            }

            for (ibis::bitvector64::word_t i = *ind; i < ind[1]; ++ i) {
                is = static_cast<uint32_t>
                    (i - static_cast<ibis::bitvector64::word_t>(ir) * nEvents);
                if (is < nEvents) {
                    bar2.read();
                }
                else {
                    ++ ir;
                    bar1.read();

                    is -= nEvents;
                    ierr = bar2.seek(is);
                    if (ierr < 0) {
                        logWarning("equiJoin", "faild to seek to row %lu for "
                                   "the right side of join (nEvents = %lu)",
                                   static_cast<long unsigned>(is),
                                   static_cast<long unsigned>(nEvents));
                        break;
                    }
                    bar2.read();
                }
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                LOGGER(ibis::gVerbose >= 0)
                    << "DEBUG -- equiJoin examining "
                    << bar1.value(varind1) << " and "
                    << bar2.value(varind2) << " for pair ("
                    << static_cast<unsigned>(i/nEvents)
                    << ", " << static_cast<unsigned>(i%nEvents)
                    << ") [" << i << "]";
#endif
                if (bar1.value(varind1) == bar2.value(varind2))
                    result.setBit(i, 1);
            } // for (ibis::bitvector64::word_t i ...
        }
        else { // ix is a list of indices
            for (unsigned i = 0; i < ix.nIndices(); ++ i) {
                uint32_t is = static_cast<uint32_t>
                    (ind[i] -
                     static_cast<ibis::bitvector64::word_t>(ir) * nEvents);
                if (is >= nEvents) {
                    ++ ir;
                    bar1.read();
                    is -= nEvents;
                }
                ierr = bar2.seek(is);
                if (ierr < 0) {
                    logWarning("equiJoin", "faild to seek to row %lu for "
                               "the right side of join (nEvents = %lu)",
                               static_cast<long unsigned>(is),
                               static_cast<long unsigned>(nEvents));
                    break;
                }
                bar2.read();
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                ibis::bitvector64::word_t j = ind[i];
                LOGGER(ibis::gVerbose >= 0)
                    << "DEBUG -- equiJoin examining "
                    << bar1.value(varind1) << " and "
                    << bar2.value(varind2) << " for pair ("
                    << static_cast<unsigned>(j/nEvents)
                    << ", " << static_cast<unsigned>(j%nEvents)
                    << ") [" << j << "]";
#endif
                if (bar1.value(varind1) == bar2.value(varind2))
                    result.setBit(ind[i], 1);
            } // i
        }
    } // for (ibis::bitvector64::indexSet ix...

    result.adjustSize(0, nbits);
    if (ibis::gVerbose > 2) {
        timer.stop();
        std::ostringstream ostr;
        ostr << trial.cnt() << " pair(s) and produced "
             << result.cnt() << " hit(s)";
        logMessage("equiJoin", "equi-join(%s, %s) evaluated %s using "
                   "%g sec(CPU), %g sec(elapsed)",
                   cmp.getName1(), cmp.getName2(), ostr.str().c_str(),
                   timer.CPUTime(), timer.realTime());
    }
    return result.cnt();
} // ibis::part::equiJoin

/// Check a set of pairs defined in @c trial.
int64_t ibis::part::deprecatedJoin(const ibis::deprecatedJoin& cmp,
                               const ibis::bitvector64& trial,
                               ibis::bitvector64& result) const {
    const double delta = fabs(cmp.getRange()->eval());
    const ibis::bitvector64::word_t nbits =
        static_cast<ibis::bitvector64::word_t>(nEvents)*nEvents;
    ibis::horometer timer;
    timer.start();
    result.clear();

    if (trial.size() > nbits) {
        std::ostringstream ostr;
        ostr << "expect it to have " << nbits << " bits, but it actually has "
             << trial.size();
        logWarning("deprecatedJoin", "invalid trial vector, %s",
                   ostr.str().c_str());
        return -3;
    }

    ibis::part::barrel bar1(this), bar2(this);
    const uint32_t varind1 = bar1.recordVariable(cmp.getName1());
    const uint32_t varind2 = bar2.recordVariable(cmp.getName2());

    long ierr = bar1.open();
    if (ierr != 0) {
        logWarning("deprecatedJoin", "failed to open variable %s",
                   cmp.getName1());
        return -1;
    }
    ierr = bar2.open();
    if (ierr != 0) {
        logWarning("deprecatedJoin", "failed to open variable %s",
                   cmp.getName2());
        return -2;
    }

    for (ibis::bitvector64::indexSet ix = trial.firstIndexSet();
         ix.nIndices() > 0; ++ ix) {
        const ibis::bitvector64::word_t *ind = ix.indices();
        uint32_t ir = static_cast<uint32_t>(*ind / nEvents);
        ierr = bar1.seek(ir);
        if (ierr < 0) {
            logWarning("deprecatedJoin", "failed to seek to row %lu for the left "
                       "side of the range join (nEvents = %lu)",
                       static_cast<long unsigned>(ir),
                       static_cast<long unsigned>(nEvents));
            break;
        }
        bar1.read();

        if (ix.isRange()) { // ix is a range
            uint32_t is = *ind -
                static_cast<ibis::bitvector64::word_t>(ir) * nEvents;
            ierr = bar2.seek(is);
            if (ierr < 0) {
                logWarning("deprecatedJoin", "failed to seek to row %lu for the "
                           "right side of the range join (nEvents = %lu)",
                           static_cast<long unsigned>(is),
                           static_cast<long unsigned>(nEvents));
                break;
            }

            for (ibis::bitvector64::word_t i = *ind; i < ind[1]; ++ i) {
                is = static_cast<unsigned>
                    (i - static_cast<ibis::bitvector64::word_t>(ir) * nEvents);
                if (is < nEvents) {
                    bar2.read();
                }
                else {
                    ++ ir;
                    bar1.read();

                    is -= nEvents;
                    ierr = bar2.seek(is);
                    if (ierr < 0) {
                        logWarning("deprecatedJoin", "failed to seek to row %lu "
                                   "for the right side of the range join "
                                   "(nEvents = %lu)",
                                   static_cast<long unsigned>(is),
                                   static_cast<long unsigned>(nEvents));
                        break;
                    }

                    bar2.read();
                }
                const double v1 = bar1.value(varind1);
                const double v2 = bar2.value(varind2);
                if (v1 >= v2 - delta && v1 <= v2 + delta)
                    result.setBit(i, 1);
            } // for (ibis::bitvector64::word_t i ...
        }
        else { // ix is a list of indices
            for (unsigned i = 0; i < ix.nIndices(); ++ i) {
                uint32_t is = ind[i] -
                    static_cast<ibis::bitvector64::word_t>(ir) * nEvents;
                if (is >= nEvents) {
                    ++ ir;
                    bar1.read();
                    is -= nEvents;
                }
                ierr = bar2.seek(is);
                if (ierr < 0) {
                    logWarning("deprecatedJoin", "failed to seek to row %lu for "
                               "the right side of the range join (nEvents "
                               "= %lu)", static_cast<long unsigned>(is),
                               static_cast<long unsigned>(nEvents));
                    break;
                }

                bar2.read();
                const double v1 = bar1.value(varind1);
                const double v2 = bar2.value(varind2);
                if (v1 >= v2 - delta && v1 <= v2 + delta)
                    result.setBit(ind[i], 1);
            } // i
        }
    } // for (ibis::bitvector64::indexSet ix...

    result.adjustSize(0, nbits);
    if (ibis::gVerbose > 2) {
        timer.stop();
        std::ostringstream ostr;
        ostr << trial.cnt() << " pair(s) and produced "
             << result.cnt() << " hit(s)";
        logMessage("deprecatedJoin", "deprecatedJoin(%s, %s, %g) evaluated %s "
                   "using %g sec(CPU), %g sec(elapsed)",
                   cmp.getName1(), cmp.getName2(), delta,
                   ostr.str().c_str(), timer.CPUTime(), timer.realTime());
    }
    return result.cnt();
} // ibis::part::deprecatedJoin

/// Check a set of pairs defined in @c trial.
int64_t ibis::part::compJoin(const ibis::deprecatedJoin& cmp,
                              const ibis::bitvector64& trial,
                              ibis::bitvector64& result) const {
    const ibis::bitvector64::word_t nbits =
        static_cast<ibis::bitvector64::word_t>(nEvents)*nEvents;
    if (trial.size() > nbits) {
        std::ostringstream ostr;
        ostr << "expect it to have " << nbits << " bits, but it actually has "
             << trial.size();
        logWarning("compJoin", "invalid trial vector, %s",
                   ostr.str().c_str());
        return -3;
    }

    ibis::horometer timer;
    timer.start();
    result.clear();

    ibis::part::barrel bar1(this), bar2(this);
    const uint32_t varind1 = bar1.recordVariable(cmp.getName1());
    const uint32_t varind2 = bar2.recordVariable(cmp.getName2());
    bar1.recordVariable(cmp.getRange());

    long ierr = bar1.open();
    if (ierr != 0) {
        logWarning("compJoin", "failed to open variable %s",
                   cmp.getName1());
        return -1;
    }
    ierr = bar2.open();
    if (ierr != 0) {
        logWarning("compJoin", "failed to open variables %s, ...",
                   cmp.getName2());
        return -2;
    }

    for (ibis::bitvector64::indexSet ix = trial.firstIndexSet();
         ix.nIndices() > 0; ++ ix) {
        const ibis::bitvector64::word_t *ind = ix.indices();
        uint32_t ir = *ind / nEvents;
        ierr = bar1.seek(ir);
        if (ierr < 0) {
            logWarning("compJoin", "failed to seek to row %lu for the left "
                       "side of the join (nEvents = %lu)",
                       static_cast<long unsigned>(ir),
                       static_cast<long unsigned>(nEvents));
            break;
        }
        bar1.read();
        double tmp, lower, upper;
        tmp = bar1.value(varind1);
        upper = fabs(cmp.getRange()->eval());
        lower = tmp - upper;
        upper += tmp;

        if (ix.isRange()) { // ix is a range
            uint32_t is =*ind -
                static_cast<ibis::bitvector64::word_t>(ir) * nEvents;
            ierr = bar2.seek(is);
            if (ierr < 0) {
                logWarning("compJoin", "failed to seek to row %lu for the "
                           "right side of the join (nEvents = %lu)",
                           static_cast<long unsigned>(is),
                           static_cast<long unsigned>(nEvents));
                break;
            }

            for (ibis::bitvector64::word_t i = *ind; i < ind[1]; ++ i) {
                is = static_cast<unsigned>
                    (i - static_cast<ibis::bitvector64::word_t>(ir) * nEvents);
                if (is < nEvents) {
                    bar2.read();
                }
                else {
                    ++ ir;
                    bar1.read();

                    is -= nEvents;
                    ierr = bar2.seek(is);
                    if (ierr < 0) {
                        logWarning("compJoin", "failed to seek to row %lu for "
                                   "the right side of the join (nEvents=%lu)",
                                   static_cast<long unsigned>(is),
                                   static_cast<long unsigned>(nEvents));
                        break;
                    }
                    bar2.read();
                    tmp = bar1.value(varind1);
                    upper = fabs(cmp.getRange()->eval());
                    lower = tmp - upper;
                    upper += tmp;
                }
                const double v2 = bar2.value(varind2);
                if (v2 >= lower && v2 <= upper)
                    result.setBit(i, 1);
            } // for (ibis::bitvector64::word_t i ...
        }
        else { // ix is a list of indices
            for (unsigned i = 0; i < ix.nIndices(); ++ i) {
                uint32_t is = ind[i] -
                    static_cast<ibis::bitvector64::word_t>(ir) * nEvents;
                if (is >= nEvents) {
                    ++ ir;
                    bar1.read();
                    is -= nEvents;
                    upper = fabs(cmp.getRange()->eval());
                    lower = tmp - upper;
                    upper += tmp;
                }
                ierr = bar2.seek(is);
                if (ierr < 0) {
                    logWarning("compJoin", "failed to seek to row %lu for the "
                               "right side of the join (nEvents = %lu)",
                               static_cast<long unsigned>(is),
                               static_cast<long unsigned>(nEvents));
                    break;
                }

                bar2.read();
                const double v2 = bar2.value(varind2);
                if (v2 >= lower && v2 <= upper)
                    result.setBit(ind[i], 1);
            } // i
        }
    } // for (ibis::bitvector64::indexSet ix...

    result.adjustSize(0, nbits);
    if (ibis::gVerbose > 2) {
        timer.stop();
        std::ostringstream ostr;
        ostr << cmp << " evaluated " << trial.cnt() << " pair(s) and produced "
             << result.cnt() << " hit(s)";
        logMessage("compJoin", "%s using %g sec(CPU), %g sec(elapsed)",
                   ostr.str().c_str(), timer.CPUTime(), timer.realTime());
    }
    return result.cnt();
} // ibis::part::compJoin

/// Check a set of pairs defined in @c trial.  This version works on
/// multiple (conjunctive) join conditions.
int64_t
ibis::part::evaluateJoin(const std::vector<const ibis::deprecatedJoin*>& cmp,
                          const ibis::bitvector64& trial,
                          ibis::bitvector64& result) const {
    const ibis::bitvector64::word_t nbits =
        static_cast<ibis::bitvector64::word_t>(nEvents)*nEvents;
    if (cmp.empty() || trial.cnt() == 0) {
        result.set(0, nbits);
        return 0;
    }
    if (cmp.size() == 1) {
        return evaluateJoin(*(cmp[0]), trial, result);
    }
    if (trial.size() > nbits) {
        std::ostringstream ostr;
        ostr << "expect it to have " << nbits << " bits, but it actually has "
             << trial.size();
        logWarning("evaluateJoin", "invalid trial vector, %s",
                   ostr.str().c_str());
        return -3;
    }

    const unsigned ncmp = cmp.size();
    ibis::horometer timer;
    timer.start();
    result.clear();


    ibis::part::barrel bar1(this), bar2(this);
    std::vector<unsigned> varind1(ncmp);
    std::vector<unsigned> varind2(ncmp);
    for (unsigned i = 0; i < ncmp; ++ i) {
        varind1[i] = bar1.recordVariable(cmp[i]->getName1());
        bar1.recordVariable(cmp[i]->getRange());
        varind2[i] = bar2.recordVariable(cmp[i]->getName2());
    }

    long ierr = bar1.open();
    if (ierr != 0) {
        logWarning("evaluateJoin", "failed to open bar1 for %lu variables "
                   "(ierr=%ld)", static_cast<long unsigned>(bar1.size()),
                   ierr);
        return -1;
    }
    ierr = bar2.open(this);
    if (ierr != 0) {
        logWarning("evaluateJoin", "failed to open bar2 for %lu variables "
                   "(ierr=%ld)", static_cast<long unsigned>(bar2.size()),
                   ierr);
        return -2;
    }

    std::vector<double> lower(ncmp), upper(ncmp);
    for (ibis::bitvector64::indexSet ix = trial.firstIndexSet();
         ix.nIndices() > 0; ++ ix) {
        const ibis::bitvector64::word_t *ind = ix.indices();
        uint32_t ir = *ind / nEvents;
        ierr = bar1.seek(ir);
        if (ierr < 0) {
            logWarning("evaluateJoin", "failed to seek to row %lu for the "
                       "left side of the join (nEvents = %lu)",
                       static_cast<long unsigned>(ir),
                       static_cast<long unsigned>(nEvents));
            break;
        }
        bar1.read();
        for (unsigned k = 0; k < ncmp; ++ k) {
            double tmp = bar1.value(varind1[k]);
            if (cmp[k]->getRange() != 0) {
                double del = fabs(cmp[k]->getRange()->eval());
                lower[k] = tmp - del;
                upper[k] = tmp + del;
            }
            else {
                lower[k] = tmp;
                upper[k] = tmp;
            }
        }

        if (ix.isRange()) { // ix is a range
            uint32_t is = *ind -
                static_cast<ibis::bitvector64::word_t>(ir) * nEvents;
            ierr = bar2.seek(is);
            if (ierr < 0) {
                logWarning("evaluateJoin", "failed to seek to row %lu for the "
                           "right side of the join (nEvents = %lu)",
                           static_cast<long unsigned>(is),
                           static_cast<long unsigned>(nEvents));
                break;
            }

            for (ibis::bitvector64::word_t i = *ind; i < ind[1]; ++ i) {
                is = static_cast<unsigned>
                    (i - static_cast<ibis::bitvector64::word_t>(ir) * nEvents);
                if (is < nEvents) {
                    bar2.read();
                }
                else {
                    ++ ir;
                    bar1.read();
                    for (unsigned k = 0; k < ncmp; ++ k) {
                        double tmp = bar1.value(varind1[k]);
                        if (cmp[k]->getRange() != 0) {
                            double del = cmp[k]->getRange()->eval();
                            lower[k] = tmp - del;
                            upper[k] = tmp + del;
                        }
                        else {
                            lower[k] = tmp;
                            upper[k] = tmp;
                        }
                    }

                    is -= nEvents;
                    ierr = bar2.seek(is);
                    if (ierr < 0) {
                        logWarning("evaluateJoin", "failed to seek to row %lu "
                                   "for the right side of the join (nEvents "
                                   "= %lu)", static_cast<long unsigned>(is),
                                   static_cast<long unsigned>(nEvents));
                        break;
                    }

                    bar2.read();
                }
                bool ishit = true;
                for (unsigned k = 0; ishit && k < ncmp; ++ k) {
                    double tmp = bar2.value(varind2[k]);
                    ishit = (tmp >= lower[k] && tmp <= upper[k]);
                }
                if (ishit)
                    result.setBit(i, 1);
            } // for (ibis::bitvector64::word_t i ...
        }
        else { // ix is a list of indices
            for (unsigned i = 0; i < ix.nIndices(); ++ i) {
                uint32_t is = ind[i] -
                    static_cast<ibis::bitvector64::word_t>(ir) * nEvents;
                if (is >= nEvents) {
                    ++ ir;
                    bar1.read();
                    is -= nEvents;
                    for (unsigned k = 0; k < ncmp; ++ k) {
                        double tmp = bar1.value(varind1[k]);
                        if (cmp[k]->getRange() != 0) {
                            double del = fabs(cmp[k]->getRange()->eval());
                            lower[k] = tmp - del;
                            upper[k] = tmp + del;
                        }
                        else {
                            lower[k] = tmp;
                            upper[k] = tmp;
                        }
                    }

                }
                ierr = bar2.seek(is);
                if (ierr < 0) {
                    logWarning("evaluateJoin", "failed to seek to row %lu for "
                               "the right side of the join (nEvents = %lu)",
                               static_cast<long unsigned>(is),
                               static_cast<long unsigned>(nEvents));
                    break;
                }

                bar2.read();
                bool ishit = true;
                for (unsigned k = 0; ishit && k < ncmp; ++ k) {
                    double tmp = bar2.value(varind2[k]);
                    ishit = (tmp >= lower[k] && tmp <= upper[k]);
                }
                if (ishit)
                    result.setBit(ind[i], 1);
            } // i
        }
    } // for (ibis::bitvector64::indexSet ix...

    result.adjustSize(0, nbits);
    if (ibis::gVerbose > 2) {
        timer.stop();
        std::ostringstream ostr;
        ostr << "(" << *(cmp[0]);
        for (unsigned k = 1; k < ncmp; ++ k)
            ostr << " AND " << *(cmp[k]);
        ostr << ") evaluated " << trial.cnt() << " pair(s) and produced "
             << result.cnt() << " hit(s)";
        logMessage("evaluateJoin", "%s using %g sec(CPU), %g sec(elapsed)",
                   ostr.str().c_str(), timer.CPUTime(), timer.realTime());
    }
    return result.cnt();
} // ibis::part::evaluateJoin

int64_t
ibis::part::evaluateJoin(const std::vector<const ibis::deprecatedJoin*>& cmp,
                          const ibis::bitvector& mask) const {
    if (cmp.empty() || mask.cnt() == 0) {
        return 0;
    }
    if (cmp.size() == 1) {
        return evaluateJoin(*(cmp[0]), mask);
    }

    double cf = ibis::bitvector::clusteringFactor
        (mask.size(), mask.cnt(), mask.bytes());
    uint64_t np = mask.size();
    uint64_t mb = mask.cnt();
    np *= np;
    mb *= mb;
    double bvsize = 4 * ibis::bitvector64::markovSize(np, mb, cf);
    if (bvsize <= ibis::fileManager::bytesFree()) {
        // there is enough space for the two bitvectors
        ibis::bitvector64 trial, result;
        ibis::util::outerProduct(mask, mask, trial);
        return evaluateJoin(cmp, trial, result);
    }
    else {
        logWarning("evaluateJoin", "there isn't enough space to store "
                   "two expected bitvector64 objects for evaluating %u "
                   "join operators", (unsigned)cmp.size());
        return -1;
    }
} // ibis::part::evaluateJoin

/// Performing equi-join using nested loops.  It uses @c
/// ibis::fileManager::storage class to read all records into memory before
/// performing any operation.  The input attributes will be treated as
/// either 4-byte integers or 8-byte integers.
//  @note
/// For floating-point values, this approach could production incorrect
/// answers for NaN, Inf and some abnormal numbers.  However, this approach
/// simplifies the implementation and may speedup comparisons.
int64_t ibis::part::equiJoinLoop1(const ibis::deprecatedJoin& cmp,
                                   const ibis::bitvector& mask,
                                   ibis::bitvector64& pairs) const {
    long ierr = -1;
    int64_t cnt = 0;
    ibis::horometer timer;
    timer.start();

    const ibis::column *col1 = getColumn(cmp.getName1());
    const ibis::column *col2 = getColumn(cmp.getName2());
    unsigned elm1 = col1->elementSize();
    if (elm1 != 4 && elm1 != 8) {
        logWarning("equiJoinLoop1", "can not proceed.  Element size(%u) "
                   "must be 4-byte or 8-byte", elm1);
        return cnt;
    }
    std::string sfn1;
    const char *dfn1 = col1->dataFileName(sfn1);
    long unsigned tlast = time(0);

    if (col1 == col2) { // the same column
        ibis::bitvector bv;
        col1->getNullMask(bv);
        bv &= mask;
        ibis::bitvector::indexSet ix1, ix2;
        if (elm1 == 4) {
            array_t<uint32_t> arr;
            ierr = fileManager::instance().getFile(dfn1, arr);
            if (ierr == 0) {
                for (ix1 = bv.firstIndexSet(); ix1.nIndices() > 0; ++ ix1) {
                    const ibis::bitvector::word_t *ind1 = ix1.indices();
                    if (ix1.isRange()) { // a range of indices
                        for (unsigned i = *ind1; i < ind1[1]; ++ i) {
                            ibis::bitvector64::word_t pos =
                                static_cast<uint64_t>(i) * nEvents;
                            for (ix2 = bv.firstIndexSet();
                                 ix2.nIndices() > 0;
                                 ++ ix2) {
                                const ibis::bitvector::word_t *ind2 =
                                    ix2.indices();
                                if (ix2.isRange()) { // a range
                                    // skip 0 to i
                                    for (unsigned j =
                                             (*ind2<=i ? i+1 : *ind2);
                                         j < ind2[1]; ++ j) {
                                        if (arr[i] == arr[j])
                                            pairs.setBit(pos+j, 1);
                                    }
                                }
                                else { // a list
                                    for (unsigned j = 0; j < ix2.nIndices();
                                         ++ j) {
                                        if (ind2[j] > i &&
                                            arr[i] == arr[ind2[j]]) 
                                            pairs.setBit(pos+ind2[j], 1);
                                    }
                                }
                            } // for (ix2 =...

                            if (ibis::gVerbose > 1) {
                                long unsigned tcurr = time(0);
                                if (tcurr-59 > tlast) {
                                    std::ostringstream ostmp;
                                    ostmp << "TIME(" << tcurr
                                          << "): just completed row "
                                          << i << " of " << nEvents
                                          << ", got " << pairs.cnt()
                                          << " hit(s)";
                                    logMessage("equiJoinLoop1", "%s",
                                               ostmp.str().c_str());
                                    tlast = tcurr;
                                }
                            }
                        }
                    }
                    else { // a list of indices
                        for (unsigned i = 0; i < ix1.nIndices(); ++ i) {
                            ibis::bitvector64::word_t pos =
                                (uint64_t)ind1[i] * nEvents;
                            for (ix2 = bv.firstIndexSet();
                                 ix2.nIndices() > 0;
                                 ++ ix2) {
                                const ibis::bitvector::word_t *ind2 =
                                    ix2.indices();
                                if (ix2.isRange()) { // a range
                                    // skip 0 to ind1[i]
                                    for (unsigned j =
                                             (*ind2<=ind1[i] ? ind1[i]+1 :
                                              *ind2);
                                         j < ind2[1]; ++ j) {
                                        if (arr[ind1[i]] == arr[j])
                                            pairs.setBit(pos+j, 1);
                                    }
                                }
                                else { // a list
                                    for (unsigned j = 0; j < ix2.nIndices();
                                         ++ j) {
                                        if (ind2[j] > ind1[i] &&
                                            arr[ind1[i]] == arr[ind2[j]])
                                            pairs.setBit(pos+ind2[j], 1);
                                    }
                                }
                            } // for (ix2 =...

                            if (ibis::gVerbose > 1) {
                                long unsigned tcurr = time(0);
                                if (tcurr-59 > tlast) {
                                    std::ostringstream ostmp;
                                    ostmp << "TIME(" << tcurr
                                          << "): just completed row "
                                          << ind1[i] << " of " << nEvents
                                          << ", got " << pairs.cnt()
                                          << " hit(s)";
                                    logMessage("equiJoinLoop1", "%s",
                                               ostmp.str().c_str());
                                    tlast = tcurr;
                                }
                            }
                        }
                    }
                } // for (ix1 =...
                cnt = pairs.cnt();
            }
            else {
                logWarning("equiJoinLoop1", "failed to read the data file %s",
                           dfn1);
                ierr = -11;
            }
        }
        else { // element size must be 8 bytes
            array_t<uint64_t> arr;
            ierr = fileManager::instance().getFile(dfn1, arr);
            if (ierr == 0) {
                for (ix1 = bv.firstIndexSet(); ix1.nIndices() > 0; ++ ix1) {
                    const ibis::bitvector::word_t *ind1 = ix1.indices();
                    if (ix1.isRange()) { // a range of indices
                        for (unsigned i = *ind1;
                             i < ind1[1]; ++ i) {
                            ibis::bitvector64::word_t pos =
                                static_cast<uint64_t>(i) * nEvents;
                            for (ix2 = bv.firstIndexSet();
                                 ix2.nIndices() > 0;
                                 ++ ix2) {
                                const ibis::bitvector::word_t *ind2 =
                                    ix2.indices();
                                if (ix2.isRange()) { // a range
                                    // skip 0 to i
                                    for (unsigned j =
                                             (*ind2<=i ? i+1 : *ind2);
                                         j < ind2[1]; ++ j) {
                                        if (arr[i] == arr[j])
                                            pairs.setBit(pos+j, 1);
                                    }
                                }
                                else { // a list
                                    for (unsigned j = 0; j < ix2.nIndices();
                                         ++ j) {
                                        if (ind2[j] > i &&
                                            arr[i] == arr[ind2[j]]) 
                                            pairs.setBit(pos+ind2[j], 1);
                                    }
                                }
                            } // for (ix2 =...

                            if (ibis::gVerbose > 1) {
                                long unsigned tcurr = time(0);
                                if (tcurr-59 > tlast) {
                                    std::ostringstream ostmp;
                                    ostmp << "TIME(" << tcurr
                                          << "): just completed row "
                                          << i << " of " << nEvents
                                          << ", got " << pairs.cnt()
                                          << " hit(s)";
                                    logMessage("equiJoinLoop1", "%s",
                                               ostmp.str().c_str());
                                    tlast = tcurr;
                                }
                            }
                        }
                    }
                    else { // a list of indices
                        for (unsigned i = 0; i < ix1.nIndices(); ++ i) {
                            ibis::bitvector64::word_t pos =
                                static_cast<uint64_t>(ind1[i]) * nEvents;
                            for (ix2 = bv.firstIndexSet();
                                 ix2.nIndices() > 0;
                                 ++ ix2) {
                                const ibis::bitvector::word_t *ind2 =
                                    ix2.indices();
                                if (ix2.isRange()) { // a range
                                    // skip 0 to i
                                    for (unsigned j =
                                             (*ind2<=ind1[i] ? ind1[i]+1 :
                                              *ind2);
                                         j < ind2[1]; ++ j) {
                                        if (arr[ind1[i]] == arr[j])
                                            pairs.setBit(pos+j, 1);
                                    }
                                }
                                else { // a list
                                    for (unsigned j = 0; j < ix2.nIndices();
                                         ++ j) {
                                        if (ind2[j] > ind1[i] &&
                                            arr[ind1[i]] == arr[ind2[j]])
                                            pairs.setBit(pos+ind2[j], 1);
                                    }
                                }
                            } // for (ix2 =...

                            if (ibis::gVerbose > 1) {
                                long unsigned tcurr = time(0);
                                if (tcurr-59 > tlast) {
                                    std::ostringstream ostmp;
                                    ostmp << "TIME(" << tcurr
                                          << "): just completed row "
                                          << ind1[i] << " of " << nEvents
                                          << ", got " << pairs.cnt()
                                          << " hit(s)";
                                    logMessage("equiJoinLoop1", "%s",
                                               ostmp.str().c_str());
                                    tlast = tcurr;
                                }
                            }
                        }
                    }
                } // for (ix1 =...
                cnt = pairs.cnt();
            }
            else {
                logWarning("equiJoinLoop1", "failed to read the data file %s",
                           dfn1);
                ierr = -12;
            }
        }
    }
    else if (col1->type() == col2->type() ||
             (col1->type() != ibis::FLOAT &&
              col1->type() != ibis::DOUBLE &&
              col2->type() != ibis::FLOAT &&
              col2->type() != ibis::DOUBLE)) {
        // columns of the same type or both are 4-byte integers
        // treat both arrays of integers and directly compare the integers
        ibis::bitvector bv1, bv2;
        col1->getNullMask(bv1);
        col2->getNullMask(bv2);
        bv1 &= mask;
        bv2 &= mask;
        ibis::bitvector::indexSet ix1, ix2;
        std::string sfn2;
        const char *dfn2 = col2->dataFileName(sfn2);
        if (elm1 == 4) {
            array_t<uint32_t> arr1, arr2;
            ierr = fileManager::instance().getFile(dfn1, arr1);
            ierr |= fileManager::instance().getFile(dfn2, arr2);
            if (ierr == 0) {
                for (ix1 = bv1.firstIndexSet(); ix1.nIndices() > 0; ++ ix1) {
                    const ibis::bitvector::word_t *ind1 = ix1.indices();
                    if (ix1.isRange()) { // a range of indices
                        for (unsigned i = *ind1; i < ind1[1]; ++ i) {
                            const ibis::bitvector::word_t val1 = arr1[i];
                            ibis::bitvector64::word_t pos =
                                static_cast<uint64_t>(i) * nEvents;
                            for (ix2 = bv2.firstIndexSet();
                                 ix2.nIndices() > 0;
                                 ++ ix2) {
                                const ibis::bitvector::word_t *ind2 =
                                    ix2.indices();
                                if (ix2.isRange()) { // a range
                                    for (unsigned j = *ind2;
                                         j < ind2[1]; ++ j) {
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                                        logMessage("equiJoinLoop1",
                                                   "val1[%lu]=%lu, "
                                                   "val2[%lu]=%lu, %s",
                                                   static_cast<long unsigned>(i),
                                                   static_cast<long unsigned>(val1),
                                                   static_cast<long unsigned>(j),
                                                   static_cast<long unsigned>(arr2[j]),
                                                   (val1 == arr2[j] ? "equal" :
                                                    "not equal"));
#endif
                                        if (val1 == arr2[j])
                                            pairs.setBit(pos+j, 1);
                                    }
                                }
                                else { // a list
                                    for (unsigned j = 0; j < ix2.nIndices();
                                         ++ j) {
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                                        logMessage("equiJoinLoop1",
                                                   "val1[%lu]=%lu, val2[%lu]=%lu,"
                                                   " %s",
                                                   static_cast<long unsigned>(i),
                                                   static_cast<long unsigned>(val1),
                                                   static_cast<long unsigned>(ind2[j]),
                                                   static_cast<long unsigned>(arr2[ind2[j]]),
                                                   (val1 == arr2[ind2[j]] ?
                                                    "equal" :
                                                    "not equal"));
#endif
                                        if (val1 == arr2[ind2[j]]) 
                                            pairs.setBit(pos+ind2[j], 1);
                                    }
                                }
                            } // for (ix2 =...

                            if (ibis::gVerbose > 1) {
                                long unsigned tcurr = time(0);
                                if (tcurr-59 > tlast) {
                                    std::ostringstream ostmp;
                                    ostmp << "TIME(" << tcurr
                                          << "): just completed row "
                                          << i << " of " << nEvents
                                          << ", got " << pairs.cnt()
                                          << " hit(s)";
                                    logMessage("equiJoinLoop1", "%s",
                                               ostmp.str().c_str());
                                    tlast = tcurr;
                                }
                            }
                        }
                    }
                    else { // a list of indices
                        for (unsigned i = 0; i < ix1.nIndices(); ++ i) {
                            const ibis::bitvector::word_t val1 = arr1[ind1[i]];
                            ibis::bitvector64::word_t pos =
                                static_cast<uint64_t>(ind1[i]) * nEvents;
                            for (ix2 = bv2.firstIndexSet();
                                 ix2.nIndices() > 0;
                                 ++ ix2) {
                                const ibis::bitvector::word_t *ind2 =
                                    ix2.indices();
                                if (ix2.isRange()) { // a range
                                    for (unsigned j = *ind2;
                                         j < ind2[1]; ++ j) {
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                                        logMessage("equiJoinLoop1",
                                                   "val1[%lu]=%lu, val2[%lu]=%lu,"
                                                   " %s",
                                                   static_cast<long unsigned>(ind1[i]),
                                                   static_cast<long unsigned>(val1),
                                                   static_cast<long unsigned>(j),
                                                   static_cast<long unsigned>(arr2[j]),
                                                   (val1 == arr2[j] ? "equal" :
                                                    "not equal"));
#endif
                                        if (val1 == arr2[j])
                                            pairs.setBit(pos+j, 1);
                                    }
                                }
                                else { // a list
                                    for (unsigned j = 0; j < ix2.nIndices();
                                         ++ j) {
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                                        logMessage("equiJoinLoop1",
                                                   "val1[%lu]=%lu, val2[%lu]=%lu,"
                                                   " %s",
                                                   static_cast<long unsigned>(ind1[i]),
                                                   static_cast<long unsigned>(val1),
                                                   static_cast<long unsigned>(ind2[j]),
                                                   static_cast<long unsigned>(arr2[ind2[j]]),
                                                   (val1 == arr2[ind2[j]] ?
                                                    "equal" :
                                                    "not equal"));
#endif
                                        if (val1 == arr2[ind2[j]])
                                            pairs.setBit(pos+ind2[j], 1);
                                    }
                                }
                            } // for (ix2 =...

                            if (ibis::gVerbose > 1) {
                                long unsigned tcurr = time(0);
                                if (tcurr-59 > tlast) {
                                    std::ostringstream ostmp;
                                    ostmp << "TIME(" << tcurr
                                          << "): just completed row "
                                          << ind1[i] << " of " << nEvents
                                          << ", got " << pairs.cnt()
                                          << " hit(s)";
                                    logMessage("equiJoinLoop1", "%s",
                                               ostmp.str().c_str());
                                    tlast = tcurr;
                                }
                            }
                        }
                    }
                } // for (ix1 =...
                cnt = pairs.cnt();
            }
            else {
                logWarning("equiJoinLoop1", "failed to read the data file %s",
                           dfn1);
                ierr = -13;
            }
        }
        else { // element size must be 8 bytes
            array_t<uint64_t> arr1, arr2;
            ierr = fileManager::instance().getFile(dfn1, arr1);
            ierr |= fileManager::instance().getFile(dfn2, arr2);
            if (ierr == 0) {
                for (ix1 = bv1.firstIndexSet(); ix1.nIndices() > 0; ++ ix1) {
                    const ibis::bitvector::word_t *ind1 = ix1.indices();
                    if (ix1.isRange()) { // a range of indices
                        for (unsigned i = *ind1;
                             i < ind1[1]; ++ i) {
                            ibis::bitvector64::word_t pos =
                                static_cast<uint64_t>(i) * nEvents;
                            for (ix2 = bv2.firstIndexSet();
                                 ix2.nIndices() > 0;
                                 ++ ix2) {
                                const ibis::bitvector::word_t *ind2 =
                                    ix2.indices();
                                if (ix2.isRange()) { // a range
                                    for (unsigned j = *ind2;
                                         j < ind2[1]; ++ j) {
                                        if (arr1[i] == arr2[j])
                                            pairs.setBit(pos+j, 1);
                                    }
                                }
                                else { // a list
                                    for (unsigned j = 0; j < ix2.nIndices();
                                         ++ j) {
                                        if (arr1[i] == arr2[ind2[j]]) 
                                            pairs.setBit(pos+ind2[j], 1);
                                    }
                                }
                            } // for (ix2 =...

                            if (ibis::gVerbose > 1) {
                                long unsigned tcurr = time(0);
                                if (tcurr-59 > tlast) {
                                    std::ostringstream ostmp;
                                    ostmp << "TIME(" << tcurr
                                          << "): just completed row "
                                          << i << " of " << nEvents
                                          << ", got " << pairs.cnt()
                                          << " hit(s)";
                                    logMessage("equiJoinLoop1", "%s",
                                               ostmp.str().c_str());
                                    tlast = tcurr;
                                }
                            }
                        }
                    }
                    else { // a list of indices
                        for (unsigned i = 0; i < ix1.nIndices(); ++ i) {
                            ibis::bitvector64::word_t pos =
                                static_cast<uint64_t>(ind1[i]) * nEvents;
                            for (ix2 = bv2.firstIndexSet();
                                 ix2.nIndices() > 0;
                                 ++ ix2) {
                                const ibis::bitvector::word_t *ind2 =
                                    ix2.indices();
                                if (ix2.isRange()) { // a range
                                    for (unsigned j = *ind2;
                                         j < ind2[1]; ++ j) {
                                        if (arr1[ind1[i]] == arr2[j])
                                            pairs.setBit(pos+j, 1);
                                    }
                                }
                                else { // a list
                                    for (unsigned j = 0; j < ix2.nIndices();
                                         ++ j) {
                                        if (arr1[ind1[i]] == arr2[ind2[j]])
                                            pairs.setBit(pos+ind2[j], 1);
                                    }
                                }
                            } // for (ix2 =...

                            if (ibis::gVerbose > 1) {
                                long unsigned tcurr = time(0);
                                if (tcurr-59 > tlast) {
                                    std::ostringstream ostmp;
                                    ostmp << "TIME(" << tcurr
                                          << "): just completed row "
                                          << ind1[i] << " of " << nEvents
                                          << ", got " << pairs.cnt()
                                          << " hit(s)";
                                    logMessage("equiJoinLoop1", "%s",
                                               ostmp.str().c_str());
                                    tlast = tcurr;
                                }
                            }
                        }
                    }
                } // for (ix1 =...
                cnt = pairs.cnt();
            }
            else {
                logWarning("equiJoinLoop1", "failed to read the data file %s",
                           dfn1);
                ierr = -14;
            }
        }
    }
    else {
        logWarning("equiJoinLoop1", "Not implemented equi-join of different "
                   "data types yet (%s:%s, %s:%s)", col1->name(),
                   ibis::TYPESTRING[col1->type()], col2->name(),
                   ibis::TYPESTRING[col2->type()]);
    }

    pairs.adjustSize(0, static_cast<ibis::bitvector64::word_t>(nEvents)*
                     static_cast<ibis::bitvector64::word_t>(nEvents));
    if (ibis::gVerbose > 2) {
        timer.stop();
        std::ostringstream ostr;
        ostr << cnt << " hits ";
        logMessage("equiJoinLoop", "in-memory equi-join(%s, %s) produced %s "
                   "using %g sec(CPU), %g sec(elapsed)", cmp.getName1(),
                   cmp.getName2(), ostr.str().c_str(), timer.CPUTime(),
                   timer.realTime());
    }

    return cnt;
} // ibis::part::equiJoinLoop1

int64_t ibis::part::equiJoinLoop1(const ibis::deprecatedJoin& cmp,
                                   const ibis::bitvector& mask) const {
    long ierr = -1;
    int64_t cnt = 0;
    ibis::horometer timer;
    timer.start();

    const ibis::column *col1 = getColumn(cmp.getName1());
    const ibis::column *col2 = getColumn(cmp.getName2());
    unsigned elm1 = col1->elementSize();
    if (elm1 != 4 && elm1 != 8) {
        logWarning("equiJoinLoop1", "can not proceed.  Element size(%u) "
                   "must be 4-byte or 8-byte", elm1);
        return cnt;
    }
    std::string sfn1;
    const char *dfn1 = col1->dataFileName(sfn1);
    long unsigned tlast = time(0);

    if (col1 == col2) { // the same column
        ibis::bitvector bv;
        col1->getNullMask(bv);
        bv &= mask;
        ibis::bitvector::indexSet ix1, ix2;
        if (elm1 == 4) {
            array_t<uint32_t> arr;
            ierr = fileManager::instance().getFile(dfn1, arr);
            if (ierr == 0) {
                for (ix1 = bv.firstIndexSet(); ix1.nIndices() > 0; ++ ix1) {
                    const ibis::bitvector::word_t *ind1 = ix1.indices();
                    if (ix1.isRange()) { // a range of indices
                        for (unsigned i = *ind1;
                             i < ind1[1]; ++ i) {
                            for (ix2 = bv.firstIndexSet();
                                 ix2.nIndices() > 0;
                                 ++ ix2) {
                                const ibis::bitvector::word_t *ind2 =
                                    ix2.indices();
                                if (ix2.isRange()) { // a range
                                    // skip 0 to i
                                    for (unsigned j = *ind2;
                                         j < ind2[1]; ++ j) {
                                        cnt += (arr[i] == arr[j]);
                                    }
                                }
                                else { // a list
                                    for (unsigned j = 0; j < ix2.nIndices();
                                         ++ j) {
                                        cnt += (arr[i] == arr[ind2[j]]);
                                    }
                                }
                            } // for (ix2 =...

                            if (ibis::gVerbose > 1) {
                                long unsigned tcurr = time(0);
                                if (tcurr-59 > tlast) {
                                    std::ostringstream ostmp;
                                    ostmp << "TIME(" << tcurr
                                          << "): just completed row "
                                          << i << " of " << nEvents
                                          << ", got " << cnt
                                          << " hit(s)";
                                    logMessage("equiJoinLoop1", "%s",
                                               ostmp.str().c_str());
                                    tlast = tcurr;
                                }
                            }
                        }
                    }
                    else { // a list of indices
                        for (unsigned i = 0; i < ix1.nIndices(); ++ i) {
                            for (ix2 = bv.firstIndexSet();
                                 ix2.nIndices() > 0;
                                 ++ ix2) {
                                const ibis::bitvector::word_t *ind2 =
                                    ix2.indices();
                                if (ix2.isRange()) { // a range
                                    // skip 0 to ind1[i]
                                    for (unsigned j = *ind2;
                                         j < ind2[1]; ++ j) {
                                        cnt += (arr[ind1[i]] == arr[j]);
                                    }
                                }
                                else { // a list
                                    for (unsigned j = 0; j < ix2.nIndices();
                                         ++ j) {
                                        cnt += (arr[ind1[i]] == arr[ind2[j]]);
                                    }
                                }
                            } // for (ix2 =...

                            if (ibis::gVerbose > 1) {
                                long unsigned tcurr = time(0);
                                if (tcurr-59 > tlast) {
                                    std::ostringstream ostmp;
                                    ostmp << "TIME(" << tcurr
                                          << "): just completed row "
                                          << ind1[i] << " of " << nEvents
                                          << ", got " << cnt
                                          << " hit(s)";
                                    logMessage("equiJoinLoop1", "%s",
                                               ostmp.str().c_str());
                                    tlast = tcurr;
                                }
                            }
                        }
                    }
                } // for (ix1 =...
            }
            else {
                logWarning("equiJoinLoop1", "failed to read the data file %s",
                           dfn1);
                ierr = -11;
            }
        }
        else { // element size must be 8 bytes
            array_t<uint64_t> arr;
            ierr = fileManager::instance().getFile(dfn1, arr);
            if (ierr == 0) {
                for (ix1 = bv.firstIndexSet(); ix1.nIndices() > 0; ++ ix1) {
                    const ibis::bitvector::word_t *ind1 = ix1.indices();
                    if (ix1.isRange()) { // a range of indices
                        for (unsigned i = *ind1;
                             i < ind1[1]; ++ i) {
                            for (ix2 = bv.firstIndexSet();
                                 ix2.nIndices() > 0;
                                 ++ ix2) {
                                const ibis::bitvector::word_t *ind2 =
                                    ix2.indices();
                                if (ix2.isRange()) { // a range
                                    // skip 0 to i
                                    for (unsigned j = *ind2;
                                         j < ind2[1]; ++ j) {
                                        cnt += (arr[i] == arr[j]);
                                    }
                                }
                                else { // a list
                                    for (unsigned j = 0; j < ix2.nIndices();
                                         ++ j) {
                                        cnt += (arr[i] == arr[ind2[j]]);
                                    }
                                }
                            } // for (ix2 =...

                            if (ibis::gVerbose > 1) {
                                long unsigned tcurr = time(0);
                                if (tcurr-59 > tlast) {
                                    std::ostringstream ostmp;
                                    ostmp << "TIME(" << tcurr
                                          << "): just completed row "
                                          << i << " of " << nEvents
                                          << ", got " << cnt
                                          << " hit(s)";
                                    logMessage("equiJoinLoop1", "%s",
                                               ostmp.str().c_str());
                                    tlast = tcurr;
                                }
                            }
                        }
                    }
                    else { // a list of indices
                        for (unsigned i = 0; i < ix1.nIndices(); ++ i) {
                            for (ix2 = bv.firstIndexSet();
                                 ix2.nIndices() > 0;
                                 ++ ix2) {
                                const ibis::bitvector::word_t *ind2 =
                                    ix2.indices();
                                if (ix2.isRange()) { // a range
                                    // skip 0 to i
                                    for (unsigned j = *ind2;
                                         j < ind2[1]; ++ j) {
                                        cnt += (arr[ind1[i]] == arr[j]);
                                    }
                                }
                                else { // a list
                                    for (unsigned j = 0; j < ix2.nIndices();
                                         ++ j) {
                                        cnt += (arr[ind1[i]] == arr[ind2[j]]);
                                    }
                                }
                            } // for (ix2 =...

                            if (ibis::gVerbose > 1) {
                                long unsigned tcurr = time(0);
                                if (tcurr-59 > tlast) {
                                    std::ostringstream ostmp;
                                    ostmp << "TIME(" << tcurr
                                          << "): just completed row "
                                          << ind1[i] << " of " << nEvents
                                          << ", got " << cnt
                                          << " hit(s)";
                                    logMessage("equiJoinLoop1", "%s",
                                               ostmp.str().c_str());
                                    tlast = tcurr;
                                }
                            }
                        }
                    }
                } // for (ix1 =...
            }
            else {
                logWarning("equiJoinLoop1", "failed to read the data file %s",
                           dfn1);
                ierr = -12;
            }
        }
    }
    else if (col1->type() == col2->type() ||
             (col1->type() != ibis::FLOAT &&
              col1->type() != ibis::DOUBLE &&
              col2->type() != ibis::FLOAT &&
              col2->type() != ibis::DOUBLE)) {
        // columns of the same type or both are 4-byte integers
        // treat both arrays of integers and directly compare the integers
        ibis::bitvector bv1, bv2;
        col1->getNullMask(bv1);
        col2->getNullMask(bv2);
        bv1 &= mask;
        bv2 &= mask;
        ibis::bitvector::indexSet ix1, ix2;
        std::string sfn2;
        const char *dfn2 = col2->dataFileName(sfn2);
        if (elm1 == 4) {
            array_t<uint32_t> arr1, arr2;
            ierr = fileManager::instance().getFile(dfn1, arr1);
            ierr |= fileManager::instance().getFile(dfn2, arr2);
            if (ierr == 0) {
                for (ix1 = bv1.firstIndexSet(); ix1.nIndices() > 0; ++ ix1) {
                    const ibis::bitvector::word_t *ind1 = ix1.indices();
                    if (ix1.isRange()) { // a range of indices
                        for (unsigned i = *ind1;
                             i < ind1[1]; ++ i) {
                            for (ix2 = bv2.firstIndexSet();
                                 ix2.nIndices() > 0;
                                 ++ ix2) {
                                const ibis::bitvector::word_t *ind2 =
                                    ix2.indices();
                                if (ix2.isRange()) { // a range
                                    for (unsigned j = *ind2;
                                         j < ind2[1]; ++ j) {
                                        cnt += (arr1[i] == arr2[j]);
                                    }
                                }
                                else { // a list
                                    for (unsigned j = 0; j < ix2.nIndices();
                                         ++ j) {
                                        cnt += (arr1[i] == arr2[ind2[j]]);
                                    }
                                }
                            } // for (ix2 =...

                            if (ibis::gVerbose > 1) {
                                long unsigned tcurr = time(0);
                                if (tcurr-59 > tlast) {
                                    std::ostringstream ostmp;
                                    ostmp << "TIME(" << tcurr
                                          << "): just completed row "
                                          << i << " of " << nEvents
                                          << ", got " << cnt
                                          << " hit(s)";
                                    logMessage("equiJoinLoop1", "%s",
                                               ostmp.str().c_str());
                                    tlast = tcurr;
                                }
                            }
                        }
                    }
                    else { // a list of indices
                        for (unsigned i = 0; i < ix1.nIndices(); ++ i) {
                            for (ix2 = bv2.firstIndexSet();
                                 ix2.nIndices() > 0;
                                 ++ ix2) {
                                const ibis::bitvector::word_t *ind2 =
                                    ix2.indices();
                                if (ix2.isRange()) { // a range
                                    for (unsigned j = *ind2;
                                         j < ind2[1]; ++ j) {
                                        cnt += (arr1[ind1[i]] == arr2[j]);
                                    }
                                }
                                else { // a list
                                    for (unsigned j = 0; j < ix2.nIndices();
                                         ++ j) {
                                        cnt += (arr1[ind1[i]]==arr2[ind2[j]]);
                                    }
                                }
                            } // for (ix2 =...

                            if (ibis::gVerbose > 1) {
                                long unsigned tcurr = time(0);
                                if (tcurr-59 > tlast) {
                                    std::ostringstream ostmp;
                                    ostmp << "TIME(" << tcurr
                                          << "): just completed row "
                                          << ind1[i] << " of " << nEvents
                                          << ", got " << cnt
                                          << " hit(s)";
                                    logMessage("equiJoinLoop1", "%s",
                                               ostmp.str().c_str());
                                    tlast = tcurr;
                                }
                            }
                        }
                    }
                } // for (ix1 =...
            }
            else {
                logWarning("equiJoinLoop1", "failed to read the data file %s",
                           dfn1);
                ierr = -13;
            }
        }
        else { // element size must be 8 bytes
            array_t<uint64_t> arr1, arr2;
            ierr = fileManager::instance().getFile(dfn1, arr1);
            ierr |= fileManager::instance().getFile(dfn2, arr2);
            if (ierr == 0) {
                for (ix1 = bv1.firstIndexSet(); ix1.nIndices() > 0; ++ ix1) {
                    const ibis::bitvector::word_t *ind1 = ix1.indices();
                    if (ix1.isRange()) { // a range of indices
                        for (unsigned i = *ind1; i < ind1[1]; ++ i) {
                            for (ix2 = bv2.firstIndexSet();
                                 ix2.nIndices() > 0;
                                 ++ ix2) {
                                const ibis::bitvector::word_t *ind2 =
                                    ix2.indices();
                                if (ix2.isRange()) { // a range
                                    for (unsigned j = *ind2;
                                         j < ind2[1]; ++ j) {
                                        cnt += (arr1[i] == arr2[j]);
                                    }
                                }
                                else { // a list
                                    for (unsigned j = 0; j < ix2.nIndices();
                                         ++ j) {
                                        cnt += (arr1[i] == arr2[ind2[j]]);
                                    }
                                }
                            } // for (ix2 =...

                            if (ibis::gVerbose > 1) {
                                long unsigned tcurr = time(0);
                                if (tcurr-59 > tlast) {
                                    std::ostringstream ostmp;
                                    ostmp << "TIME(" << tcurr
                                          << "): just completed row "
                                          << i << " of " << nEvents
                                          << ", got " << cnt
                                          << " hit(s)";
                                    logMessage("equiJoinLoop1", "%s",
                                               ostmp.str().c_str());
                                    tlast = tcurr;
                                }
                            }
                        }
                    }
                    else { // a list of indices
                        for (unsigned i = 0; i < ix1.nIndices(); ++ i) {
                            for (ix2 = bv2.firstIndexSet();
                                 ix2.nIndices() > 0;
                                 ++ ix2) {
                                const ibis::bitvector::word_t *ind2 =
                                    ix2.indices();
                                if (ix2.isRange()) { // a range
                                    for (unsigned j = *ind2;
                                         j < ind2[1]; ++ j) {
                                        cnt += (arr1[ind1[i]] == arr2[j]);
                                    }
                                }
                                else { // a list
                                    for (unsigned j = 0; j < ix2.nIndices();
                                         ++ j) {
                                        cnt += (arr1[ind1[i]]==arr2[ind2[j]]);
                                    }
                                }
                            } // for (ix2 =...

                            if (ibis::gVerbose > 1) {
                                long unsigned tcurr = time(0);
                                if (tcurr-59 > tlast) {
                                    std::ostringstream ostmp;
                                    ostmp << "TIME(" << tcurr
                                          << "): just completed row "
                                          << ind1[i] << " of " << nEvents
                                          << ", got " << cnt
                                          << " hit(s)";
                                    logMessage("equiJoinLoop1", "%s",
                                               ostmp.str().c_str());
                                    tlast = tcurr;
                                }
                            }
                        }
                    }
                } // for (ix1 =...
            }
            else {
                logWarning("equiJoinLoop1", "failed to read the data file %s",
                           dfn1);
                ierr = -14;
            }
        }
    }
    else {
        logWarning("equiJoinLoop1", "Not implemented equi-join of different "
                   "data types yet (%s:%s, %s:%s)", col1->name(),
                   ibis::TYPESTRING[col1->type()], col2->name(),
                   ibis::TYPESTRING[col2->type()]);
    }

    if (ibis::gVerbose > 2) {
        timer.stop();
        std::ostringstream ostr;
        ostr << cnt << " hits ";
        logMessage("equiJoinLoop", "in-memory equi-join(%s, %s) produced %s "
                   "using %g sec(CPU), %g sec(elapsed)", cmp.getName1(),
                   cmp.getName2(), ostr.str().c_str(), timer.CPUTime(),
                   timer.realTime());
    }

    return cnt;
} // ibis::part::equiJoinLoop1

/// This implementation of the nested-loop equi-join uses @c
/// ibis::part::barrel to read the data files.  This uses less memory than
/// ibis::part::equiJoinLoop1.  It casts every attributes into double,
/// which will cause the comparisons to be slower on some machines.
int64_t ibis::part::equiJoinLoop2(const ibis::deprecatedJoin& cmp,
                                   const ibis::bitvector& mask,
                                   ibis::bitvector64& pairs) const {
    ibis::horometer timer;
    timer.start();

    ibis::part::barrel bar1(this), bar2(this);
    const uint32_t varind1 = bar1.recordVariable(cmp.getName1());
    const uint32_t varind2 = bar2.recordVariable(cmp.getName2());

    long ierr = bar1.open();
    if (ierr != 0) {
        logWarning("equiJoinLoop2", "failed to open variable %s",
                   cmp.getName1());
        return -1;
    }
    ierr = bar2.open();
    if (ierr != 0) {
        logWarning("equiJoinLoop2", "failed to open variable %s",
                   cmp.getName2());
        return -2;
    }

    ibis::bitvector msk1(mask), msk2(mask);
    bar1.getNullMask(msk1);
    bar2.getNullMask(msk2);

    long unsigned tlast = time(0);
    for (ibis::bitvector::indexSet ix1 = msk1.firstIndexSet();
         ix1.nIndices() > 0; ++ ix1) {
        const ibis::bitvector::word_t *ind1 = ix1.indices();
        if (ix1.isRange()) { // ix1 is a range
            ierr = bar1.seek(*ind1);
            if (ierr < 0) {
                logWarning("equiJoinLoop2", "failed to seek to row %lu for "
                           "the left side of the join",
                           static_cast<long unsigned>(*ind1));
                break;
            }

            for (unsigned i = *ind1; i < ind1[1]; ++ i) {
                const ibis::bitvector64::word_t pos =
                    static_cast<uint64_t>(nEvents) * i;
                bar1.read();
                const double left = bar1.value(varind1);
                for (ibis::bitvector::indexSet ix2 = msk2.firstIndexSet();
                     ix2.nIndices() > 0; ++ ix2) {
                    const ibis::bitvector::word_t *ind2 = ix2.indices();
                    if (ix2.isRange()) { // ix2 is a range
                        const ibis::bitvector::word_t j0 = *ind2;
                        ierr = bar2.seek(j0);
                        if (ierr < 0) {
                            logWarning("equiJoinLoop2", "failed to seek to "
                                       "row %lu for the right side of the join",
                                       static_cast<long unsigned>(j0));
                            break;
                        }

                        for (unsigned j = j0; j < ind2[1]; ++ j) {
                            bar2.read();
                            if (left == bar2.value(varind2))
                                pairs.setBit(pos+j, 1);
                        } // j
                    }
                    else { // ix2 is a list of indices
                        for (unsigned j = 0; j < ix2.nIndices(); ++ j) {
                            ierr = bar2.seek(ind2[j]);
                            if (ierr < 0) {
                                logWarning("equiJoinLoop2",
                                           "failed to seek to row %lu for "
                                           "the right side of the join",
                                           static_cast<long unsigned>(ind2[j]));
                                break;
                            }

                            bar2.read();
                            if (left == bar2.value(varind2))
                                pairs.setBit(pos+ind2[j], 1);
                        } // j
                    }
                } // for (ibis::bitvector::indexSet ix2

                if (ibis::gVerbose > 1) {
                    long unsigned tcurr = time(0);
                    if (tcurr-59 > tlast) {
                        std::ostringstream ostmp;
                        ostmp << "TIME(" << tcurr
                              << "): just completed row "
                              << i << " of " << nEvents
                              << ", got " << pairs.cnt()
                              << " hit(s)";
                        logMessage("equiJoinLoop2", "%s",
                                   ostmp.str().c_str());
                        tlast = tcurr;
                    }
                }
            } // for (unsigned i ...
        }
        else { // ix1 is a list of indices
            for (unsigned i = 0; i < ix1.nIndices(); ++ i) {
                ierr = bar1.seek(ind1[i]);
                if (ierr < 0) {
                    logWarning("equiJoinLoop2", "failed to seek to row %lu for "
                               "the left side of the join",
                               static_cast<long unsigned>(ind1[i]));
                    break;
                }

                bar1.read();
                const double left = bar1.value(varind1);
                const ibis::bitvector64::word_t pos =
                    static_cast<uint64_t>(nEvents) * ind1[i];
                for (ibis::bitvector::indexSet ix2 = msk2.firstIndexSet();
                     ix2.nIndices() > 0; ++ ix2) {
                    const ibis::bitvector::word_t *ind2 = ix2.indices();
                    if (ix2.isRange()) { // ix2 is a range
                        const ibis::bitvector::word_t j0 = *ind2;
                        ierr = bar2.seek(j0);
                        if (ierr < 0) {
                            logWarning("equiJoinLoop2", "failed to seek to "
                                       "row %lu for the right side of the join",
                                       static_cast<long unsigned>(j0));
                            break;
                        }

                        for (unsigned j = j0; j < ind2[1]; ++ j) {
                            bar2.read();
                            if (left == bar2.value(varind2))
                                pairs.setBit(pos+j, 1);
                        } // j
                    }
                    else { // ix2 is a list of indices
                        for (unsigned j = 0; j < ix2.nIndices(); ++ j) {
                            ierr = bar2.seek(ind2[j]);
                            if (ierr < 0) {
                                logWarning("equiJoinLoop2",
                                           "failed to seek to row %lu for "
                                           "the right side of the join",
                                           static_cast<long unsigned>(ind2[j]));
                                break;
                            }

                            bar2.read();
                            if (left == bar2.value(varind2))
                                pairs.setBit(pos + ind2[j], 1);
                        } // j
                    }
                } // ix2

                if (ibis::gVerbose > 1) {
                    long unsigned tcurr = time(0);
                    if (tcurr-59 > tlast) {
                        std::ostringstream ostmp;
                        ostmp << "TIME(" << tcurr
                              << "): just completed row "
                              << ind1[i] << " of " << nEvents
                              << ", got " << pairs.cnt()
                              << " hit(s)";
                        logMessage("equiJoinLoop2", "%s",
                                   ostmp.str().c_str());
                        tlast = tcurr;
                    }
                }
            } // i
        }
    } // for (ibis::bitvector::indexSet ix1...

    pairs.adjustSize(0, static_cast<ibis::bitvector64::word_t>(nEvents)*
                     static_cast<ibis::bitvector64::word_t>(nEvents));
    if (ibis::gVerbose > 2) {
        timer.stop();
        std::ostringstream ostr;
        ostr << pairs.cnt() << " hit(s)";
        logMessage("equiJoinLoop2", "equi-join(%s, %s) produced %s "
                   "using %g sec(CPU), %g sec(elapsed)", cmp.getName1(),
                   cmp.getName2(), ostr.str().c_str(), timer.CPUTime(),
                   timer.realTime());
    }
    return pairs.cnt();
} // ibis::part::equiJoinLoop2

int64_t ibis::part::equiJoinLoop2(const ibis::deprecatedJoin& cmp,
                                  const ibis::bitvector& mask) const {
    int64_t cnt = 0;
    ibis::horometer timer;
    timer.start();

    ibis::part::barrel bar1(this), bar2(this);
    const uint32_t varind1 = bar1.recordVariable(cmp.getName1());
    const uint32_t varind2 = bar2.recordVariable(cmp.getName2());

    long ierr = bar1.open();
    if (ierr != 0) {
        logWarning("equiJoinLoop2", "failed to open variable %s",
                   cmp.getName1());
        return -1;
    }
    ierr = bar2.open();
    if (ierr != 0) {
        logWarning("equiJoinLoop2", "failed to open variable %s",
                   cmp.getName2());
        return -2;
    }

    ibis::bitvector msk1(mask), msk2(mask);
    bar1.getNullMask(msk1);
    bar2.getNullMask(msk2);

    long unsigned tlast = time(0);
    for (ibis::bitvector::indexSet ix1 = msk1.firstIndexSet();
         ix1.nIndices() > 0; ++ ix1) {
        const ibis::bitvector::word_t *ind1 = ix1.indices();
        if (ix1.isRange()) { // ix1 is a range
            ierr = bar1.seek(*ind1);
            if (ierr < 0) {
                logWarning("equiJoinLoop2", "failed to seek to row %lu for "
                           "the left side of the join",
                           static_cast<long unsigned>(*ind1));
                break;
            }

            for (unsigned i = *ind1; i < ind1[1]; ++ i) {
                bar1.read();
                const double left = bar1.value(varind1);
                for (ibis::bitvector::indexSet ix2 = msk2.firstIndexSet();
                     ix2.nIndices() > 0; ++ ix2) {
                    const ibis::bitvector::word_t *ind2 = ix2.indices();
                    if (ix2.isRange()) { // ix2 is a range
                        const ibis::bitvector::word_t j0 = *ind2;
                        ierr = bar2.seek(j0);
                        if (ierr < 0) {
                            logWarning("equiJoinLoop2", "failed to seek to "
                                       "row %lu for the right side of the join",
                                       static_cast<long unsigned>(j0));
                            break;
                        }

                        for (unsigned j = j0; j < ind2[1]; ++ j) {
                            bar2.read();
                            cnt += (left == bar2.value(varind2));
                        } // j
                    }
                    else { // ix2 is a list of indices
                        for (unsigned j = 0; j < ix2.nIndices(); ++ j) {
                            ierr = bar2.seek(ind2[j]);
                            if (ierr < 0) {
                                logWarning("equiJoinLoop2",
                                           "failed to seek to row %lu for "
                                           "the right side of the join",
                                           static_cast<long unsigned>(ind2[j]));
                                break;
                            }

                            bar2.read();
                            cnt += (left == bar2.value(varind2));
                        } // j
                    }
                } // for (ibis::bitvector::indexSet ix2

                if (ibis::gVerbose > 1) {
                    long unsigned tcurr = time(0);
                    if (tcurr-59 > tlast) {
                        std::ostringstream ostmp;
                        ostmp << "TIME(" << tcurr
                              << "): just completed row "
                              << i << " of " << nEvents
                              << ", got " << cnt
                              << " hit(s)";
                        logMessage("equiJoinLoop2", "%s",
                                   ostmp.str().c_str());
                        tlast = tcurr;
                    }
                }
            } // for (unsigned i ...
        }
        else { // ix1 is a list of indices
            for (unsigned i = 0; i < ix1.nIndices(); ++ i) {
                ierr = bar1.seek(ind1[i]);
                if (ierr < 0) {
                    logWarning("equiJoinLoop2", "failed to seek to row %lu for "
                               "the left side of the join",
                               static_cast<long unsigned>(ind1[i]));
                    break;
                }
                bar1.read();
                const double left = bar1.value(varind1);
                for (ibis::bitvector::indexSet ix2 = msk2.firstIndexSet();
                     ix2.nIndices() > 0; ++ ix2) {
                    const ibis::bitvector::word_t *ind2 = ix2.indices();
                    if (ix2.isRange()) { // ix2 is a range
                        const ibis::bitvector::word_t j0 = *ind2;
                        ierr = bar2.seek(j0);
                        if (ierr < 0) {
                            logWarning("equiJoinLoop2", "failed to seek to "
                                       "row %lu for the right side of the join",
                                       static_cast<long unsigned>(j0));
                            break;
                        }

                        for (unsigned j = j0;
                             j < ind2[1]; ++ j) {
                            bar2.read();
                            cnt += (left == bar2.value(varind2));
                        } // j
                    }
                    else { // ix2 is a list of indices
                        for (unsigned j = 0; j < ix2.nIndices(); ++ j) {
                            ierr = bar2.seek(ind2[j]);
                            if (ierr < 0) {
                                logWarning("equiJoinLoop2",
                                           "failed to seek to row %lu for "
                                           "the right side of the join",
                                           static_cast<long unsigned>(ind2[j]));
                                break;
                            }

                            bar2.read();
                            cnt += (left == bar2.value(varind2));
                        } // j
                    }
                } // ix2

                if (ibis::gVerbose > 1) {
                    long unsigned tcurr = time(0);
                    if (tcurr-59 > tlast) {
                        std::ostringstream ostmp;
                        ostmp << "TIME(" << tcurr
                              << "): just completed row "
                              << ind1[i] << " of " << nEvents
                              << ", got " << cnt
                              << " hit(s)";
                        logMessage("equiJoinLoop2", "%s",
                                   ostmp.str().c_str());
                        tlast = tcurr;
                    }
                }
            } // i
        }
    } // for (ibis::bitvector::indexSet ix1...

    if (ibis::gVerbose > 2) {
        timer.stop();
        std::ostringstream ostr;
        ostr << cnt << " hit(s)";
        logMessage("equiJoinLoop2", "equi-join(%s, %s) produced %s "
                   "using %g sec(CPU), %g sec(elapsed)", cmp.getName1(),
                   cmp.getName2(), ostr.str().c_str(), timer.CPUTime(),
                   timer.realTime());
    }
    return cnt;
} // ibis::part::equiJoinLoop2

/// A nested loop version of range join, with a fixed range.  This version
/// requires both attributes to be in memory.  Because the operations of
/// pairs.setBit is a lot slower in the blocked version.  This
/// implementation uses a simple loop to iterate over both attributes.
template <class type1, class type2>
void ibis::part::deprecatedJoinLoop(const array_t<type1>& arr1,
                                const ibis::bitvector& msk1,
                                const array_t<type2>& arr2,
                                const ibis::bitvector& msk2,
                                const double delta,
                                ibis::bitvector64& pairs) const {
    long unsigned tlast = time(0);
    for (ibis::bitvector::indexSet ix1 = msk1.firstIndexSet();
         ix1.nIndices() > 0; ++ ix1) {
        const ibis::bitvector::word_t *ind1 = ix1.indices();
        if (ix1.isRange()) { // ix1 is a range
            for (unsigned i = *ind1; i < ind1[1]; ++ i) {
                const type2 upper = static_cast<type2>(arr1[i] + delta);
                // in case of under flow, set to 0
                const type2 lower = 
                    static_cast<type2>(arr1[i]-delta) <= upper ?
                    static_cast<type2>(arr1[i]-delta) : 0;
                const ibis::bitvector64::word_t pos =
                    static_cast<ibis::bitvector64::word_t>(nEvents) * i;

                for (ibis::bitvector::indexSet ix2 = msk2.firstIndexSet();
                     ix2.nIndices() > 0; ++ ix2) {
                    const ibis::bitvector::word_t *ind2 = ix2.indices();
                    if (ix2.isRange()) {
                        for (unsigned j = *ind2;
                             j < ind2[1]; ++ j) {
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                            ibis::util::logMessage
                                ("DEBUG", "deprecatedJoinLoop val1[%u]=%g (lo=%g, "
                                 "hi=%g), arr2[%u]=%g", i, (double)arr1[i],
                                 (double)lower, (double)upper, j,
                                 (double) arr2[j]);
#endif
                            if (arr2[j] >= lower &&
                                arr2[j] <= upper) {
                                pairs.setBit(pos+j, 1);
                            }
                        }
                    }
                    else {
                        for (unsigned j = 0; j < ix2.nIndices(); ++ j) {
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                            ibis::util::logMessage
                                ("DEBUG", "deprecatedJoinLoop arr1[%u]=%g (lo=%g, "
                                 "hi=%g), arr2[%u]=%g", i, (double)arr1[i],
                                 (double)lower, (double)upper, ind2[j],
                                 (double) arr2[ind2[j]]);
#endif
                            if (arr2[ind2[j]] >= lower &&
                                arr2[ind2[j]] <= upper) {
                                pairs.setBit(pos + ind2[j], 1);
                            }
                        }
                    }
                }

                if (ibis::gVerbose > 1) {
                    long unsigned tcurr = time(0);
                    if (tcurr-59 > tlast) {
                        std::ostringstream ostmp;
                        ostmp << "TIME(" << tcurr
                              << "): just completed row "
                              << i << " of " << nEvents
                              << ", got " << pairs.cnt()
                              << " hit(s)";
                        logMessage("deprecatedJoinLoop", "%s",
                                   ostmp.str().c_str());
                        tlast = tcurr;
                    }
                }
            }
        }
        else { // ix1 is a list of indices
            for (unsigned i = 0; i < ix1.nIndices(); ++ i) {
                const ibis::bitvector64::word_t pos =
                    static_cast<ibis::bitvector64::word_t>(nEvents) * ind1[i];
                const type2 upper = static_cast<type2>(arr1[ind1[i]] + delta);
                // in case of under flow (wrap arround), set to 0
                const type2 lower = 
                    static_cast<type2>(arr1[ind1[i]] - delta) <= upper ?
                    static_cast<type2>(arr1[ind1[i]] - delta) : 0;

                for (ibis::bitvector::indexSet ix2 = msk2.firstIndexSet();
                     ix2.nIndices() > 0; ++ ix2) {
                    const ibis::bitvector::word_t *ind2 = ix2.indices();
                    if (ix2.isRange()) {
                        for (unsigned j = *ind2; j < ind2[1]; ++ j) {
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                            ibis::util::logMessage
                                ("DEBUG", "deprecatedJoinLoop arr1[%u]=%g (lo=%g, "
                                 "hi=%g), arr2[%u]=%g", ind1[i],
                                 (double)arr1[ind1[i]],
                                 (double)lower, (double)upper, j,
                                 (double) arr2[j]);
#endif
                            if (arr2[j] >= lower && arr2[j] <= upper)
                                pairs.setBit(pos+j, 1);
                        }
                    }
                    else {
                        for (unsigned j = 0; j < ix2.nIndices(); ++ j) {
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                            ibis::util::logMessage
                                ("DEBUG", "deprecatedJoinLoop arr1[%u]=%g (lo=%g, "
                                 "hi=%g), arr2[%u]=%g", ind1[i],
                                 (double)arr1[ind1[i]],
                                 (double)lower, (double)upper, ind2[j],
                                 (double) arr2[ind2[j]]);
#endif
                            if (arr2[ind2[j]] >= lower &&
                                arr2[ind2[j]] <= upper)
                                pairs.setBit(pos+ind2[j], 1);
                        }
                    }
                }

                if (ibis::gVerbose > 1) {
                    long unsigned tcurr = time(0);
                    if (tcurr-59 > tlast) {
                        std::ostringstream ostmp;
                        ostmp << "TIME(" << tcurr
                              << "): just completed row "
                              << ind1[i] << " of " << nEvents
                              << ", got " << pairs.cnt()
                              << " hit(s)";
                        logMessage("deprecatedJoinLoop", "%s",
                                   ostmp.str().c_str());
                        tlast = tcurr;
                    }
                }
            } // i
        }
    } // for (ibis::bitvector::indexSet ix1

    // make sure the output bitvector has the correct number of bits.
    pairs.adjustSize(0, static_cast<ibis::bitvector64::word_t>(nEvents)*
                     static_cast<ibis::bitvector64::word_t>(nEvents));
} // ibis::part::deprecatedJoinLoop

// TODO: figure out how to do the block version ?
template <class type1, class type2>
int64_t ibis::part::deprecatedJoinLoop(const array_t<type1>& arr1,
                                   const ibis::bitvector& msk1,
                                   const array_t<type2>& arr2,
                                   const ibis::bitvector& msk2,
                                   const double delta) const {
    int64_t cnt = 0;
    long unsigned tlast = time(0);
    for (ibis::bitvector::indexSet ix1 = msk1.firstIndexSet();
         ix1.nIndices() > 0; ++ ix1) {
        const ibis::bitvector::word_t *ind1 = ix1.indices();
        if (ix1.isRange()) { // ix1 is a range
            for (unsigned i = *ind1; i < ind1[1]; ++ i) {
                const type2 upper = static_cast<type2>(arr1[i] + delta);
                // most likely unsigned integer underflow, set to 0
                const type2 lower = 
                    static_cast<type2>(arr1[i]-delta) < upper ?
                    static_cast<type2>(arr1[i]-delta) : 0;

                for (ibis::bitvector::indexSet ix2 = msk2.firstIndexSet();
                     ix2.nIndices() > 0; ++ ix2) {
                    const ibis::bitvector::word_t *ind2 = ix2.indices();
                    if (ix2.isRange()) {
                        for (unsigned j = *ind2; j < ind2[1]; ++ j) {
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                            ibis::util::logMessage
                                ("DEBUG", "deprecatedJoinLoop arr1[%u]=%g (lo=%g, "
                                 "hi=%g), arr2[%u]=%g", i,
                                 (double)arr1[i],
                                 (double)lower, (double)upper, j,
                                 (double) arr2[j]);
#endif
                            cnt += (arr2[j] >= lower && arr2[j] <= upper);
                        }
                    }
                    else {
                        for (unsigned j = 0; j < ix2.nIndices(); ++ j) {
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                            ibis::util::logMessage
                                ("DEBUG", "deprecatedJoinLoop arr1[%u]=%g (lo=%g, "
                                 "hi=%g), arr2[%u]=%g", i,
                                 (double)arr1[i],
                                 (double)lower, (double)upper, ind2[j],
                                 (double) arr2[ind2[j]]);
#endif
                            cnt += (arr2[ind2[j]] >= lower &&
                                    arr2[ind2[j]] <= upper);
                        }
                    }
                }

                if (ibis::gVerbose > 1) {
                    long unsigned tcurr = time(0);
                    if (tcurr-59 > tlast) {
                        std::ostringstream ostmp;
                        ostmp << "TIME(" << tcurr
                              << "): just completed row "
                              << i << " of " << nEvents
                              << ", got " << cnt
                              << " hit(s)";
                        logMessage("deprecatedJoinLoop", "%s",
                                   ostmp.str().c_str());
                        tlast = tcurr;
                    }
                }
            }
        }
        else { // ix1 is a list of indices
            for (unsigned i = 0; i < ix1.nIndices(); ++ i) {
                const type2 upper = static_cast<type2>(arr1[ind1[i]] + delta);
                // likely unsigned integer underflow, set to 0
                const type2 lower = 
                    static_cast<type2>(arr1[ind1[i]] - delta) < upper ?
                    static_cast<type2>(arr1[ind1[i]] - delta) : 0;

                for (ibis::bitvector::indexSet ix2 = msk2.firstIndexSet();
                     ix2.nIndices() > 0; ++ ix2) {
                    const ibis::bitvector::word_t *ind2 = ix2.indices();
                    if (ix2.isRange()) {
                        for (unsigned j = *ind2; j < ind2[1]; ++ j) {
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                            ibis::util::logMessage
                                ("DEBUG", "deprecatedJoinLoop arr1[%u]=%g (lo=%g, "
                                 "hi=%g), arr2[%u]=%g", ind1[i],
                                 (double)arr1[ind1[i]],
                                 (double)lower, (double)upper, j,
                                 (double) arr2[j]);
#endif
                            cnt += (arr2[j] >= lower && arr2[j] <= upper);
                        }
                    }
                    else {
                        for (unsigned j = 0; j < ix2.nIndices(); ++ j) {
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                            ibis::util::logMessage
                                ("DEBUG", "deprecatedJoinLoop arr1[%u]=%g (lo=%g, "
                                 "hi=%g), arr2[%u]=%g", ind1[i],
                                 (double)arr1[ind1[i]],
                                 (double)lower, (double)upper, ind2[j],
                                 (double) arr2[ind2[j]]);
#endif
                            cnt += (arr2[ind2[j]] >= lower &&
                                    arr2[ind2[j]] <= upper);
                        }
                    }
                }

                if (ibis::gVerbose > 1) {
                    long unsigned tcurr = time(0);
                    if (tcurr-59 > tlast) {
                        std::ostringstream ostmp;
                        ostmp << "TIME(" << tcurr
                              << "): just completed row "
                              << ind1[i] << " of " << nEvents
                              << ", got " << cnt
                              << " hit(s)";
                        logMessage("deprecatedJoinLoop", "%s",
                                   ostmp.str().c_str());
                        tlast = tcurr;
                    }
                }
            } // i
        }
    } // for (ibis::bitvector::indexSet ix1
    return cnt;
} // ibis::part::deprecatedJoinLoop

int64_t ibis::part::deprecatedJoinLoop(const ibis::deprecatedJoin& cmp,
                                   const ibis::bitvector& mask,
                                   ibis::bitvector64& pairs) const {
    ibis::horometer timer;
    timer.start();

    const double delta = fabs(cmp.getRange()->eval());
    ibis::column* col1 = getColumn(cmp.getName1());
    ibis::column* col2 = getColumn(cmp.getName2());
    ibis::bitvector bv1, bv2;
    col1->getNullMask(bv1);
    col2->getNullMask(bv2);
    bv1 &= mask;
    bv2 &= mask;
    std::string sfn1, sfn2;
    const char *dfn1 = col1->dataFileName(sfn1);
    const char *dfn2 = col2->dataFileName(sfn2);
    switch (col1->type()) {
    case ibis::DOUBLE: {
        array_t<double> arr1;
        int ierr = ibis::fileManager::instance().getFile(dfn1, arr1);
        if (ierr != 0) {
            logWarning("deprecatedJoinLoop", "failed to retrieve the content "
                       "of %s", dfn1);
            return -1;
        }
        switch (col2->type()) {
        case ibis::DOUBLE: {
            array_t<double> arr2;
            ierr = ibis::fileManager::instance().getFile(dfn2, arr2);
            if (ierr == 0) {
                deprecatedJoinLoop(arr1, bv1, arr2, bv2, delta, pairs);
            }
            else {
                logWarning("deprecatedJoinLoop", "failed to retrieve the content "
                           "of %s", dfn2);
            }
            break;}
        case ibis::FLOAT: {
            array_t<float> arr2;
            ierr = ibis::fileManager::instance().getFile(dfn2, arr2);
            if (ierr == 0) {
                deprecatedJoinLoop(arr1, bv1, arr2, bv2, delta, pairs);
            }
            else {
                logWarning("deprecatedJoinLoop", "failed to retrieve the content "
                           "of %s", dfn2);
                return -2;
            }
            break;}
        case ibis::TEXT:
        case ibis::UINT: {
            array_t<uint32_t> arr2;
            ierr = ibis::fileManager::instance().getFile(dfn2, arr2);
            if (ierr == 0) {
                deprecatedJoinLoop(arr1, bv1, arr2, bv2, delta, pairs);
            }
            else {
                logWarning("deprecatedJoinLoop", "failed to retrieve the content "
                           "of %s", dfn2);
                return -2;
            }
            break;}
        case ibis::INT: {
            array_t<int32_t> arr2;
            ierr = ibis::fileManager::instance().getFile(dfn2, arr2);
            if (ierr == 0) {
                deprecatedJoinLoop(arr1, bv1, arr2, bv2, delta, pairs);
            }
            else {
                logWarning("deprecatedJoinLoop", "failed to retrieve the content "
                           "of %s", dfn2);
                return -2;
            }
            break;}
        default: {
            logWarning("deprecatedJoinLoop", "can not process column %s:%s",
                       col2->name(), ibis::TYPESTRING[col2->type()]);
            return -2;}
        } // switch (col1->type())
        break;}
    case ibis::FLOAT: {
        array_t<float> arr1;
        int ierr = ibis::fileManager::instance().getFile(dfn1, arr1);
        if (ierr != 0) {
            logWarning("deprecatedJoinLoop", "failed to retrieve the content "
                       "of %s", dfn1);
            return -1;
        }
        switch (col2->type()) {
        case ibis::DOUBLE: {
            array_t<double> arr2;
            ierr = ibis::fileManager::instance().getFile(dfn2, arr2);
            if (ierr == 0) {
                deprecatedJoinLoop(arr1, bv1, arr2, bv2, delta, pairs);
            }
            else {
                logWarning("deprecatedJoinLoop", "failed to retrieve the content "
                           "of %s", dfn2);
                return -2;
            }
            break;}
        case ibis::FLOAT: {
            array_t<float> arr2;
            ierr = ibis::fileManager::instance().getFile(dfn2, arr2);
            if (ierr == 0) {
                deprecatedJoinLoop(arr1, bv1, arr2, bv2, delta, pairs);
            }
            else {
                logWarning("deprecatedJoinLoop", "failed to retrieve the content "
                           "of %s", dfn2);
                return -2;
            }
            break;}
        case ibis::TEXT:
        case ibis::UINT: {
            array_t<uint32_t> arr2;
            ierr = ibis::fileManager::instance().getFile(dfn2, arr2);
            if (ierr == 0) {
                deprecatedJoinLoop(arr1, bv1, arr2, bv2, delta, pairs);
            }
            else {
                logWarning("deprecatedJoinLoop", "failed to retrieve the content "
                           "of %s", dfn2);
                return -2;
            }
            break;}
        case ibis::INT: {
            array_t<int32_t> arr2;
            ierr = ibis::fileManager::instance().getFile(dfn2, arr2);
            if (ierr == 0) {
                deprecatedJoinLoop(arr1, bv1, arr2, bv2, delta, pairs);
            }
            else {
                logWarning("deprecatedJoinLoop", "failed to retrieve the content "
                           "of %s", dfn2);
                return -2;
            }
            break;}
        default: {
            logWarning("deprecatedJoinLoop", "can not process column %s:%s",
                       col2->name(), ibis::TYPESTRING[col2->type()]);
            return -3;}
        } // switch (col1->type())
        break;}
    case ibis::TEXT:
    case ibis::UINT: {
        array_t<uint32_t> arr1;
        int ierr = ibis::fileManager::instance().getFile(dfn1, arr1);
        if (ierr != 0) {
            logWarning("deprecatedJoinLoop", "failed to retrieve the content "
                       "of %s", dfn1);
            return -1;
        }
        switch (col2->type()) {
        case ibis::DOUBLE: {
            array_t<double> arr2;
            ierr = ibis::fileManager::instance().getFile(dfn2, arr2);
            if (ierr == 0) {
                deprecatedJoinLoop(arr1, bv1, arr2, bv2, delta, pairs);
            }
            else {
                logWarning("deprecatedJoinLoop", "failed to retrieve the content "
                           "of %s", dfn2);
                return -2;
            }
            break;}
        case ibis::FLOAT: {
            array_t<float> arr2;
            ierr = ibis::fileManager::instance().getFile(dfn2, arr2);
            if (ierr == 0) {
                deprecatedJoinLoop(arr1, bv1, arr2, bv2, delta, pairs);
            }
            else {
                logWarning("deprecatedJoinLoop", "failed to retrieve the content "
                           "of %s", dfn2);
                return -2;
            }
            break;}
        case ibis::TEXT:
        case ibis::UINT: {
            array_t<uint32_t> arr2;
            ierr = ibis::fileManager::instance().getFile(dfn2, arr2);
            if (ierr == 0) {
                deprecatedJoinLoop(arr1, bv1, arr2, bv2, delta, pairs);
            }
            else {
                logWarning("deprecatedJoinLoop", "failed to retrieve the content "
                           "of %s", dfn2);
                return -2;
            }
            break;}
        case ibis::INT: {
            array_t<int32_t> arr2;
            ierr = ibis::fileManager::instance().getFile(dfn2, arr2);
            if (ierr == 0) {
                deprecatedJoinLoop(arr1, bv1, arr2, bv2, delta, pairs);
            }
            else {
                logWarning("deprecatedJoinLoop", "failed to retrieve the content "
                           "of %s", dfn2);
                return -2;
            }
            break;}
        default: {
            logWarning("deprecatedJoinLoop", "can not process column %s:%s",
                       col2->name(), ibis::TYPESTRING[col2->type()]);
            return -3;}
        } // switch (col1->type())
        break;}
    case ibis::INT: {
        array_t<int32_t> arr1;
        int ierr = ibis::fileManager::instance().getFile(dfn1, arr1);
        if (ierr != 0) {
            logWarning("deprecatedJoinLoop", "failed to retrieve the content "
                       "of %s", dfn1);
            return -1;
        }
        switch (col2->type()) {
        case ibis::DOUBLE: {
            array_t<double> arr2;
            ierr = ibis::fileManager::instance().getFile(dfn2, arr2);
            if (ierr == 0) {
                deprecatedJoinLoop(arr1, bv1, arr2, bv2, delta, pairs);
            }
            else {
                logWarning("deprecatedJoinLoop", "failed to retrieve the content "
                           "of %s", dfn2);
                return -2;
            }
            break;}
        case ibis::FLOAT: {
            array_t<float> arr2;
            ierr = ibis::fileManager::instance().getFile(dfn2, arr2);
            if (ierr == 0) {
                deprecatedJoinLoop(arr1, bv1, arr2, bv2, delta, pairs);
            }
            else {
                logWarning("deprecatedJoinLoop", "failed to retrieve the content "
                           "of %s", dfn2);
                return -2;
            }
            break;}
        case ibis::TEXT:
        case ibis::UINT: {
            array_t<uint32_t> arr2;
            ierr = ibis::fileManager::instance().getFile(dfn2, arr2);
            if (ierr == 0) {
                deprecatedJoinLoop(arr1, bv1, arr2, bv2, delta, pairs);
            }
            else {
                logWarning("deprecatedJoinLoop", "failed to retrieve the content "
                           "of %s", dfn2);
                return -2;
            }
            break;}
        case ibis::INT: {
            array_t<int32_t> arr2;
            ierr = ibis::fileManager::instance().getFile(dfn2, arr2);
            if (ierr == 0) {
                deprecatedJoinLoop(arr1, bv1, arr2, bv2, delta, pairs);
            }
            else {
                logWarning("deprecatedJoinLoop", "failed to retrieve the content "
                           "of %s", dfn2);
                return -2;
            }
            break;}
        default: {
            logWarning("deprecatedJoinLoop", "can not process column %s:%s",
                       col2->name(), ibis::TYPESTRING[col2->type()]);
            return -3;}
        } // switch (col1->type())
        break;}
    default: {
        logWarning("deprecatedJoinLoop", "can not process column %s:%s",
                   col1->name(), ibis::TYPESTRING[col1->type()]);
        return -3;}
    } // switch (col1->type())

    if (ibis::gVerbose > 2) {
        timer.stop();
        std::ostringstream ostr;
        ostr << pairs.cnt() << " hit(s)";
        logMessage("deprecatedJoinLoop", "deprecatedJoin(%s, %s, %g) produced %s "
                   "in %g second(CPU), %g sec(elapsed)",
                   cmp.getName1(), cmp.getName2(), delta, ostr.str().c_str(),
                   timer.CPUTime(), timer.realTime());
    }
    return pairs.cnt();
} // ibis::part::deprecatedJoinLoop

int64_t ibis::part::deprecatedJoinLoop(const ibis::deprecatedJoin& cmp,
                                   const ibis::bitvector& mask) const {
    ibis::horometer timer;
    timer.start();

    const double delta = fabs(cmp.getRange()->eval());
    ibis::column* col1 = getColumn(cmp.getName1());
    ibis::column* col2 = getColumn(cmp.getName2());
    ibis::bitvector bv1, bv2;
    col1->getNullMask(bv1);
    col2->getNullMask(bv2);
    bv1 &= mask;
    bv2 &= mask;
    std::string sfn1, sfn2;
    const char *dfn1 = col1->dataFileName(sfn1);
    const char *dfn2 = col2->dataFileName(sfn2);
    int64_t cnt = 0;
    switch (col1->type()) {
    case ibis::DOUBLE: {
        array_t<double> arr1;
        int ierr = ibis::fileManager::instance().getFile(dfn1, arr1);
        if (ierr != 0) {
            logWarning("deprecatedJoinLoop", "failed to retrieve the content "
                       "of %s", dfn1);
            return -1;
        }
        switch (col2->type()) {
        case ibis::DOUBLE: {
            array_t<double> arr2;
            ierr = ibis::fileManager::instance().getFile(dfn2, arr2);
            if (ierr == 0) {
                cnt = deprecatedJoinLoop(arr1, bv1, arr2, bv2, delta);
            }
            else {
                logWarning("deprecatedJoinLoop", "failed to retrieve the content "
                           "of %s", dfn2);
                return -2;
            }
            break;}
        case ibis::FLOAT: {
            array_t<float> arr2;
            ierr = ibis::fileManager::instance().getFile(dfn2, arr2);
            if (ierr == 0) {
                cnt = deprecatedJoinLoop(arr1, bv1, arr2, bv2, delta);
            }
            else {
                logWarning("deprecatedJoinLoop", "failed to retrieve the content "
                           "of %s", dfn2);
                return -2;
            }
            break;}
        case ibis::TEXT:
        case ibis::UINT: {
            array_t<uint32_t> arr2;
            ierr = ibis::fileManager::instance().getFile(dfn2, arr2);
            if (ierr == 0) {
                cnt = deprecatedJoinLoop(arr1, bv1, arr2, bv2, delta);
            }
            else {
                logWarning("deprecatedJoinLoop", "failed to retrieve the content "
                           "of %s", dfn2);
                return -2;
            }
            break;}
        case ibis::INT: {
            array_t<int32_t> arr2;
            ierr = ibis::fileManager::instance().getFile(dfn2, arr2);
            if (ierr == 0) {
                cnt = deprecatedJoinLoop(arr1, bv1, arr2, bv2, delta);
            }
            else {
                logWarning("deprecatedJoinLoop", "failed to retrieve the content "
                           "of %s", dfn2);
                return -2;
            }
            break;}
        default: {
            logWarning("deprecatedJoinLoop", "can not process column %s:%s",
                       col2->name(), ibis::TYPESTRING[col2->type()]);
            return -3;}
        } // switch (col1->type())
        break;}
    case ibis::FLOAT: {
        array_t<float> arr1;
        int ierr = ibis::fileManager::instance().getFile(dfn1, arr1);
        if (ierr != 0) {
            logWarning("deprecatedJoinLoop", "failed to retrieve the content "
                       "of %s", dfn1);
            return -1;
        }
        switch (col2->type()) {
        case ibis::DOUBLE: {
            array_t<double> arr2;
            ierr = ibis::fileManager::instance().getFile(dfn2, arr2);
            if (ierr == 0) {
                cnt = deprecatedJoinLoop(arr1, bv1, arr2, bv2, delta);
            }
            else {
                logWarning("deprecatedJoinLoop", "failed to retrieve the content "
                           "of %s", dfn2);
                return -2;
            }
            break;}
        case ibis::FLOAT: {
            array_t<float> arr2;
            ierr = ibis::fileManager::instance().getFile(dfn2, arr2);
            if (ierr == 0) {
                cnt = deprecatedJoinLoop(arr1, bv1, arr2, bv2, delta);
            }
            else {
                logWarning("deprecatedJoinLoop", "failed to retrieve the content "
                           "of %s", dfn2);
                return -2;
            }
            break;}
        case ibis::TEXT:
        case ibis::UINT: {
            array_t<uint32_t> arr2;
            ierr = ibis::fileManager::instance().getFile(dfn2, arr2);
            if (ierr == 0) {
                cnt = deprecatedJoinLoop(arr1, bv1, arr2, bv2, delta);
            }
            else {
                logWarning("deprecatedJoinLoop", "failed to retrieve the content "
                           "of %s", dfn2);
                return -2;
            }
            break;}
        case ibis::INT: {
            array_t<int32_t> arr2;
            ierr = ibis::fileManager::instance().getFile(dfn2, arr2);
            if (ierr == 0) {
                cnt = deprecatedJoinLoop(arr1, bv1, arr2, bv2, delta);
            }
            else {
                logWarning("deprecatedJoinLoop", "failed to retrieve the content "
                           "of %s", dfn2);
                return -2;
            }
            break;}
        default: {
            logWarning("deprecatedJoinLoop", "can not process column %s:%s",
                       col2->name(), ibis::TYPESTRING[col2->type()]);
            return -3;}
        } // switch (col1->type())
        break;}
    case ibis::TEXT:
    case ibis::UINT: {
        array_t<uint32_t> arr1;
        int ierr = ibis::fileManager::instance().getFile(dfn1, arr1);
        if (ierr != 0) {
            logWarning("deprecatedJoinLoop", "failed to retrieve the content "
                       "of %s", dfn1);
            return -1;
        }
        switch (col2->type()) {
        case ibis::DOUBLE: {
            array_t<double> arr2;
            ierr = ibis::fileManager::instance().getFile(dfn2, arr2);
            if (ierr == 0) {
                cnt = deprecatedJoinLoop(arr1, bv1, arr2, bv2, delta);
            }
            else {
                logWarning("deprecatedJoinLoop", "failed to retrieve the content "
                           "of %s", dfn2);
                return -2;
            }
            break;}
        case ibis::FLOAT: {
            array_t<float> arr2;
            ierr = ibis::fileManager::instance().getFile(dfn2, arr2);
            if (ierr == 0) {
                cnt = deprecatedJoinLoop(arr1, bv1, arr2, bv2, delta);
            }
            else {
                logWarning("deprecatedJoinLoop", "failed to retrieve the content "
                           "of %s", dfn2);
                return -2;
            }
            break;}
        case ibis::TEXT:
        case ibis::UINT: {
            array_t<uint32_t> arr2;
            ierr = ibis::fileManager::instance().getFile(dfn2, arr2);
            if (ierr == 0) {
                cnt = deprecatedJoinLoop(arr1, bv1, arr2, bv2, delta);
            }
            else {
                logWarning("deprecatedJoinLoop", "failed to retrieve the content "
                           "of %s", dfn2);
                return -2;
            }
            break;}
        case ibis::INT: {
            array_t<int32_t> arr2;
            ierr = ibis::fileManager::instance().getFile(dfn2, arr2);
            if (ierr == 0) {
                cnt = deprecatedJoinLoop(arr1, bv1, arr2, bv2, delta);
            }
            else {
                logWarning("deprecatedJoinLoop", "failed to retrieve the content "
                           "of %s", dfn2);
                return -2;
            }
            break;}
        default: {
            logWarning("deprecatedJoinLoop", "can not process column %s:%s",
                       col2->name(), ibis::TYPESTRING[col2->type()]);
            return -3;}
        } // switch (col1->type())
        break;}
    case ibis::INT: {
        array_t<int32_t> arr1;
        int ierr = ibis::fileManager::instance().getFile(dfn1, arr1);
        if (ierr != 0) {
            logWarning("deprecatedJoinLoop", "failed to retrieve the content "
                       "of %s", dfn1);
            return -1;
        }
        switch (col2->type()) {
        case ibis::DOUBLE: {
            array_t<double> arr2;
            ierr = ibis::fileManager::instance().getFile(dfn2, arr2);
            if (ierr == 0) {
                cnt = deprecatedJoinLoop(arr1, bv1, arr2, bv2, delta);
            }
            else {
                logWarning("deprecatedJoinLoop", "failed to retrieve the content "
                           "of %s", dfn2);
                return -2;
            }
            break;}
        case ibis::FLOAT: {
            array_t<float> arr2;
            ierr = ibis::fileManager::instance().getFile(dfn2, arr2);
            if (ierr == 0) {
                cnt = deprecatedJoinLoop(arr1, bv1, arr2, bv2, delta);
            }
            else {
                logWarning("deprecatedJoinLoop", "failed to retrieve the content "
                           "of %s", dfn2);
                return -2;
            }
            break;}
        case ibis::TEXT:
        case ibis::UINT: {
            array_t<uint32_t> arr2;
            ierr = ibis::fileManager::instance().getFile(dfn2, arr2);
            if (ierr == 0) {
                cnt = deprecatedJoinLoop(arr1, bv1, arr2, bv2, delta);
            }
            else {
                logWarning("deprecatedJoinLoop", "failed to retrieve the content "
                           "of %s", dfn2);
                return -2;
            }
            break;}
        case ibis::INT: {
            array_t<int32_t> arr2;
            ierr = ibis::fileManager::instance().getFile(dfn2, arr2);
            if (ierr == 0) {
                cnt = deprecatedJoinLoop(arr1, bv1, arr2, bv2, delta);
            }
            else {
                logWarning("deprecatedJoinLoop", "failed to retrieve the content "
                           "of %s", dfn2);
                return -2;
            }
            break;}
        default: {
            logWarning("deprecatedJoinLoop", "can not process column %s:%s",
                       col2->name(), ibis::TYPESTRING[col2->type()]);
            return -3;}
        } // switch (col1->type())
        break;}
    default: {
        logWarning("deprecatedJoinLoop", "can not process column %s:%s",
                   col1->name(), ibis::TYPESTRING[col1->type()]);
        return -3;}
    } // switch (col1->type())

    if (ibis::gVerbose > 2) {
        timer.stop();
        std::ostringstream ostr;
        ostr << cnt << " hit(s)";
        logMessage("deprecatedJoinLoop", "deprecatedJoin(%s, %s, %g) produced %s "
                   "in %g second(CPU), %g sec(elapsed)",
                   cmp.getName1(), cmp.getName2(), delta, ostr.str().c_str(),
                   timer.CPUTime(), timer.realTime());
    }
    return cnt;
} // ibis::part::deprecatedJoinLoop

/// Evaluate the range join that involves an arithematic expressions as the
/// difference.  It associates all variables in the arithematic expression
/// with the left relation, i.e., @c cmp.getName1().  It casts all values
/// to double.  Not suitable for processing RIDs and other 8-byte integer
/// values.
int64_t ibis::part::compJoinLoop(const ibis::deprecatedJoin& cmp,
                                  const ibis::bitvector& mask,
                                  ibis::bitvector64& pairs) const {
    ibis::horometer timer;
    timer.start();

    ibis::part::barrel bar1(this), bar2(this);
    const uint32_t varind1 = bar1.recordVariable(cmp.getName1());
    const uint32_t varind2 = bar2.recordVariable(cmp.getName2());
    bar1.recordVariable(cmp.getRange());

    long ierr = bar1.open(this);
    if (ierr != 0) {
        logWarning("compJoinLoop", "failed to open bar1 for %lu variable%s "
                   "(ierr=%ld)", static_cast<long unsigned>(bar1.size()),
                   (bar1.size()>1?"s":""), ierr);
        return -1;
    }
    ierr = bar2.open(this);
    if (ierr != 0) {
        logWarning("compJoinLoop", "failed to open bar2 for %lu variable%s "
                   "(ierr=%ld)", static_cast<long unsigned>(bar2.size()),
                   (bar2.size()>1?"s":""), ierr);
        return -2;
    }

    ibis::bitvector msk1, msk2;
    msk1.copy(mask);
    msk2.copy(mask);
    bar1.getNullMask(msk1);
    bar2.getNullMask(msk2);

    long unsigned tlast = time(0);
    for (ibis::bitvector::indexSet ix1 = msk1.firstIndexSet();
         ix1.nIndices() > 0; ++ ix1) {
        const ibis::bitvector::word_t *ind1 = ix1.indices();
        if (ix1.isRange()) { // ix1 is a range
            ierr = bar1.seek(*ind1);
            if (ierr < 0) {
                logWarning("compJoinLoop", "failed to seek to row %lu for "
                           "the left side of the join",
                           static_cast<long unsigned>(*ind1));
                break;
            }

            for (unsigned i = *ind1; i < ind1[1]; ++ i) {
                bar1.read();
                double tmp = bar1.value(varind1);
                const double delta = fabs(cmp.getRange()->eval());
                const double lower = tmp - delta;
                const double upper = tmp + delta;
                const ibis::bitvector64::word_t pos =
                    static_cast<ibis::bitvector64::word_t>(nEvents) * i;
                for (ibis::bitvector::indexSet ix2 = msk2.firstIndexSet();
                     ix2.nIndices() > 0; ++ ix2) {
                    const ibis::bitvector::word_t *ind2 = ix2.indices();
                    if (ix2.isRange()) { // ix2 is a range
                        const ibis::bitvector::word_t j0 = *ind2;
                        ierr = bar2.seek(j0);
                        if (ierr < 0) {
                            logWarning("compJoinLoop", "failed to seek to "
                                       "row %lu for the right side of the join",
                                       static_cast<long unsigned>(j0));
                            break;
                        }

                        for (unsigned j = j0;
                             j < ind2[1]; ++ j) {
                            bar2.read();
                            tmp = bar2.value(varind2);
                            if (tmp >= lower && tmp <= upper)
                                pairs.setBit(pos + j, 1);
                        }
                    }
                    else { // ix2 is a list of indices
                        for (unsigned j = 0; j < ix2.nIndices(); ++ j) {
                            ierr = bar2.seek(ind2[j]);
                            if (ierr < 0) {
                                logWarning("compJoinLoop",
                                           "failed to seek to row %lu for "
                                           "the right side of the join",
                                           static_cast<long unsigned>(ind2[j]));
                                break;
                            }

                            bar2.read();
                            tmp = bar2.value(varind2);
                            if (tmp >= lower && tmp <= upper)
                                pairs.setBit(pos + ind2[j], 1);
                        }
                    }
                }

                if (ibis::gVerbose > 1) {
                    long unsigned tcurr = time(0);
                    if (tcurr-59 > tlast) {
                        std::ostringstream ostmp;
                        ostmp << "TIME(" << tcurr
                              << "): just completed row "
                              << i << " of " << nEvents
                              << ", got " << pairs.cnt()
                              << " hit(s)";
                        logMessage("compJoinLoop", "%s",
                                   ostmp.str().c_str());
                        tlast = tcurr;
                    }
                }
            }
        }
        else { // ix1 is a list of indices
            for (unsigned i = 0; i < ix1.nIndices(); ++ i) {
                ierr = bar1.seek(ind1[i]);
                if (ierr < 0) {
                    logWarning("compJoinLoop", "failed to seek to row %lu for "
                               "the left side of the join",
                               static_cast<long unsigned>(ind1[i]));
                    break;
                }

                bar1.read();
                double tmp = bar1.value(varind1);
                const double delta = fabs(cmp.getRange()->eval());
                const double lower = tmp - delta;
                const double upper = tmp + delta;
                const ibis::bitvector64::word_t pos =
                    static_cast<ibis::bitvector64::word_t>(nEvents) * ind1[i];
                for (ibis::bitvector::indexSet ix2 = msk2.firstIndexSet();
                     ix2.nIndices() > 0; ++ ix2) {
                    const ibis::bitvector::word_t *ind2 = ix2.indices();
                    if (ix2.isRange()) { // ix2 is a range
                        const ibis::bitvector::word_t j0 = *ind2;
                        ierr = bar2.seek(j0);
                        if (ierr < 0) {
                            logWarning("compJoinLoop", "failed to seek to "
                                       "row %lu for the right side of the join",
                                       static_cast<long unsigned>(j0));
                            break;
                        }

                        for (unsigned j = j0; j < ind2[1]; ++ j) {
                            bar2.read();
                            tmp = bar2.value(varind2);
                            if (tmp >= lower && tmp <= upper)
                                pairs.setBit(pos + j, 1);
                        } // j
                    }
                    else { // ix2 is a list of indices
                        for (unsigned j = 0; j < ix2.nIndices(); ++ j) {
                            ierr = bar2.seek(ind2[j]);
                            if (ierr < 0) {
                                logWarning("compJoinLoop",
                                           "failed to seek to row %lu for "
                                           "the right side of the join",
                                           static_cast<long unsigned>(ind2[j]));
                                break;
                            }

                            bar2.read();
                            tmp = bar2.value(varind2);
                            if (tmp >= lower && tmp <= upper)
                                pairs.setBit(pos + ind2[j], 1);
                        } // j
                    }
                }

                if (ibis::gVerbose > 1) {
                    long unsigned tcurr = time(0);
                    if (tcurr-59 > tlast) {
                        std::ostringstream ostmp;
                        ostmp << "TIME(" << tcurr
                              << "): just completed row "
                              << i << " of " << nEvents
                              << ", got " << pairs.cnt()
                              << " hit(s)";
                        logMessage("compJoinLoop", "%s",
                                   ostmp.str().c_str());
                        tlast = tcurr;
                    }
                }
            }
        }
    } // for (ibis::bitvector::indexSet ix1 =...

    pairs.adjustSize(0, static_cast<ibis::bitvector64::word_t>(nEvents)*
                     static_cast<ibis::bitvector64::word_t>(nEvents));

    if (ibis::gVerbose > 2) {
        timer.stop();
        std::ostringstream ostr;
        ostr << cmp << " producted " << pairs.cnt() << " hit(s)";
        logMessage("compJoinLoop", "%s took %g sec(CPU), %g sec(elapsed)",
                   ostr.str().c_str(), timer.CPUTime(), timer.realTime());
    }
    return pairs.cnt();
} // ibis::part::compJoinLoop

int64_t ibis::part::compJoinLoop(const ibis::deprecatedJoin& cmp,
                                  const ibis::bitvector& mask) const {
    ibis::horometer timer;
    timer.start();

    ibis::part::barrel bar1(this), bar2(this);
    const uint32_t varind1 = bar1.recordVariable(cmp.getName1());
    const uint32_t varind2 = bar2.recordVariable(cmp.getName2());
    bar1.recordVariable(cmp.getRange());

    long ierr = bar1.open(this);
    if (ierr != 0) {
        logWarning("compJoinLoop", "failed to open bar1 for %lu variable%s "
                   "(ierr=%ld)", static_cast<long unsigned>(bar1.size()),
                   (bar1.size()>1?"s":""), ierr);
        return -1;
    }
    ierr = bar2.open(this);
    if (ierr != 0) {
        logWarning("compJoinLoop", "failed to open bar2 for %lu variable%s "
                   "(ierr=%ld)", static_cast<long unsigned>(bar2.size()),
                   (bar2.size()>1?"s":""), ierr);
        return -2;
    }

    ibis::bitvector msk1, msk2;
    msk1.copy(mask);
    msk2.copy(mask);
    bar1.getNullMask(msk1);
    bar2.getNullMask(msk2);

    int64_t cnt = 0;
    long unsigned tlast = time(0);
    for (ibis::bitvector::indexSet ix1 = msk1.firstIndexSet();
         ix1.nIndices() > 0; ++ ix1) {
        const ibis::bitvector::word_t *ind1 = ix1.indices();
        if (ix1.isRange()) { // ix1 is a range
            ierr = bar1.seek(*ind1);
            if (ierr < 0) {
                logWarning("compJoinLoop", "failed to seek to row %lu for "
                           "the left side of the join",
                           static_cast<long unsigned>(*ind1));
                break;
            }

            for (unsigned i = *ind1; i < ind1[1]; ++ i) {
                bar1.read();
                double tmp = bar1.value(varind1);
                const double delta = fabs(cmp.getRange()->eval());
                const double lower = tmp - delta;
                const double upper = tmp + delta;
                for (ibis::bitvector::indexSet ix2 = msk2.firstIndexSet();
                     ix2.nIndices() > 0; ++ ix2) {
                    const ibis::bitvector::word_t *ind2 = ix2.indices();
                    if (ix2.isRange()) { // ix2 is a range
                        const ibis::bitvector::word_t j0 = *ind2;
                        ierr = bar2.seek(j0);
                        if (ierr < 0) {
                            logWarning("compJoinLoop", "failed to seek to "
                                       "row %lu for the right side of the join",
                                       static_cast<long unsigned>(j0));
                            break;
                        }

                        for (unsigned j = j0; j < ind2[1]; ++ j) {
                            bar2.read();
                            tmp = bar2.value(varind2);
                            cnt += (tmp >= lower && tmp <= upper);
                        }
                    }
                    else { // ix2 is a list of indices
                        for (unsigned j = 0; j < ix2.nIndices(); ++ j) {
                            ierr = bar2.seek(ind2[j]);
                            if (ierr < 0) {
                                logWarning("compJoinLoop",
                                           "failed to seek to row %lu for "
                                           "the right side of the join",
                                           static_cast<long unsigned>(ind2[j]));
                                break;
                            }

                            bar2.read();
                            tmp = bar2.value(varind2);
                            cnt += (tmp >= lower && tmp <= upper);
                        }
                    }
                }

                if (ibis::gVerbose > 1) {
                    long unsigned tcurr = time(0);
                    if (tcurr-59 > tlast) {
                        std::ostringstream ostmp;
                        ostmp << "TIME(" << tcurr
                              << "): just completed row "
                              << i << " of " << nEvents
                              << ", got " << cnt
                              << " hit(s)";
                        logMessage("compJoinLoop", "%s",
                                   ostmp.str().c_str());
                        tlast = tcurr;
                    }
                }
            }
        }
        else { // ix1 is a list of indices
            for (unsigned i = 0; i < ix1.nIndices(); ++ i) {
                ierr = bar1.seek(ind1[i]);
                if (ierr < 0) {
                    logWarning("compJoinLoop", "failed to seek to row %lu for "
                               "the left side of the join",
                               static_cast<long unsigned>(ind1[i]));
                    break;
                }

                bar1.read();
                double tmp = bar1.value(varind1);
                const double delta = fabs(cmp.getRange()->eval());
                const double lower = tmp - delta;
                const double upper = tmp + delta;
                for (ibis::bitvector::indexSet ix2 = msk2.firstIndexSet();
                     ix2.nIndices() > 0; ++ ix2) {
                    const ibis::bitvector::word_t *ind2 = ix2.indices();
                    if (ix2.isRange()) { // ix2 is a range
                        const ibis::bitvector::word_t j0 = *ind2;
                        ierr = bar2.seek(j0);
                        if (ierr < 0) {
                            logWarning("compJoinLoop", "failed to seek to "
                                       "row %lu for the right side of the join",
                                       static_cast<long unsigned>(j0));
                            break;
                        }

                        for (unsigned j = j0; j < ind2[1]; ++ j) {
                            bar2.read();
                            tmp = bar2.value(varind2);
                            cnt += (tmp >= lower && tmp <= upper);
                        } // j
                    }
                    else { // ix2 is a list of indices
                        for (unsigned j = 0; j < ix2.nIndices(); ++ j) {
                            ierr = bar2.seek(ind2[j]);
                            if (ierr < 0) {
                                logWarning("compJoinLoop",
                                           "failed to seek to row %lu for "
                                           "the right side of the join",
                                           static_cast<long unsigned>(ind2[j]));
                                break;
                            }

                            bar2.read();
                            tmp = bar2.value(varind2);
                            cnt += (tmp >= lower && tmp <= upper);
                        } // j
                    }
                }

                if (ibis::gVerbose > 1) {
                    long unsigned tcurr = time(0);
                    if (tcurr-59 > tlast) {
                        std::ostringstream ostmp;
                        ostmp << "TIME(" << tcurr
                              << "): just completed row "
                              << i << " of " << nEvents
                              << ", got " << cnt
                              << " hit(s)";
                        logMessage("compJoinLoop", "%s",
                                   ostmp.str().c_str());
                        tlast = tcurr;
                    }
                }
            }
        }
    } // for (ibis::bitvector::indexSet ix1 =...

    if (ibis::gVerbose > 2) {
        timer.stop();
        std::ostringstream ostr;
        ostr << cmp << " producted " << cnt << " hit(s)";
        logMessage("compJoinLoop", "%s took %g sec(CPU), %g sec(elapsed)",
                   ostr.str().c_str(), timer.CPUTime(), timer.realTime());
    }
    return cnt;
} // ibis::part::compJoinLoop

/// Evaluate a number of range joins together.  It assumes all elements of
/// @c cmp are valid pointers and does not perform any validity checks.
int64_t ibis::part::loopJoin(const std::vector<const ibis::deprecatedJoin*>& cmp,
                              const ibis::bitvector& mask,
                              ibis::bitvector64& pairs) const {
    if (cmp.empty()) { // nothing to do
        pairs.set(0, static_cast<ibis::bitvector64::word_t>(nEvents)*
                  static_cast<ibis::bitvector64::word_t>(nEvents));
        return 0;
    }
    else if (cmp.size() == 1) { // special case, processed elsewhere
        return loopJoin(*(cmp[0]), mask, pairs);
    }

    ibis::horometer timer;
    timer.start();

    const unsigned ncmp = cmp.size();
    ibis::part::barrel bar1(this), bar2(this);
    std::vector<unsigned> varind1(ncmp);
    std::vector<unsigned> varind2(ncmp);
    for (unsigned i = 0; i < ncmp; ++ i) {
        varind1[i] = bar1.recordVariable(cmp[i]->getName1());
        varind2[i] = bar2.recordVariable(cmp[i]->getName2());
        if (cmp[i]->getRange() != 0)
            bar1.recordVariable(cmp[i]->getRange());
    }

    long ierr = bar1.open(this);
    if (ierr != 0) {
        logWarning("loopJoin", "failed to open bar1 for %lu variable%s "
                   "(ierr=%ld)", static_cast<long unsigned>(bar1.size()),
                   (bar1.size()>1?"s":""), ierr);
        return -1;
    }
    ierr = bar2.open(this);
    if (ierr != 0) {
        logWarning("loopJoin", "failed to open bar2 for %lu variable%s "
                   "(ierr=%ld)", static_cast<long unsigned>(bar2.size()),
                   (bar2.size()>1?"s":""), ierr);
        return -2;
    }

    ibis::bitvector msk1, msk2;
    msk1.copy(mask);
    msk2.copy(mask);
    bar1.getNullMask(msk1);
    bar2.getNullMask(msk2);

    long unsigned tlast = time(0);
    std::vector<double> lower(ncmp), upper(ncmp);
    for (ibis::bitvector::indexSet ix1 = msk1.firstIndexSet();
         ix1.nIndices() > 0; ++ ix1) {
        const ibis::bitvector::word_t *ind1 = ix1.indices();
        if (ix1.isRange()) { // ix1 is a range
            ierr = bar1.seek(*ind1);
            if (ierr < 0) {
                logWarning("loopJoin", "failed to seek to row %lu for "
                           "the left side of the join",
                           static_cast<long unsigned>(*ind1));
                break;
            }

            for (unsigned i = *ind1; i < ind1[1]; ++ i) {
                bar1.read();
                const ibis::bitvector64::word_t pos =
                    static_cast<ibis::bitvector64::word_t>(nEvents) * i;
                for (unsigned k = 0; k < ncmp; ++ k) {
                    double tmp = bar1.value(varind1[k]);
                    if (cmp[k]->getRange() != 0) {
                        double delta = fabs(cmp[k]->getRange()->eval());
                        lower[k] = tmp - delta;
                        upper[k] = tmp + delta;
                    }
                    else {
                        lower[k] = tmp;
                        upper[k] = tmp;
                    }
                }

                for (ibis::bitvector::indexSet ix2 = msk2.firstIndexSet();
                     ix2.nIndices() > 0; ++ ix2) {
                    const ibis::bitvector::word_t *ind2 = ix2.indices();
                    if (ix2.isRange()) { // ix2 is a range
                        const ibis::bitvector::word_t j0 = *ind2;
                        ierr = bar2.seek(j0);
                        if (ierr < 0) {
                            logWarning("loopJoin", "failed to seek to "
                                       "row %lu for the right side of the join",
                                       static_cast<long unsigned>(j0));
                            break;
                        }

                        for (unsigned j = j0;
                             j < ind2[1]; ++ j) {
                            bar2.read();
                            bool ishit = true;
                            for (unsigned k = 0; ishit && k < ncmp; ++ k) {
                                double tmp = bar2.value(varind2[k]);
                                ishit = (tmp >= lower[k] &&
                                         tmp <= upper[k]);
                            }
                            if (ishit)
                                pairs.setBit(pos + j, 1);
                        }
                    }
                    else { // ix2 is a list of indices
                        for (unsigned j = 0; j < ix2.nIndices(); ++ j) {
                            ierr = bar2.seek(ind2[j]);
                            if (ierr < 0) {
                                logWarning("loopJoin",
                                           "failed to seek to row %lu for "
                                           "the right side of the join",
                                           static_cast<long unsigned>(ind2[j]));
                                break;
                            }

                            bar2.read();
                            bool ishit = true;
                            for (unsigned k = 0; ishit && k < ncmp; ++ k) {
                                double tmp = bar2.value(varind2[k]);
                                ishit = (tmp >= lower[k] &&
                                         tmp <= upper[k]);
                            }
                            if (ishit)
                                pairs.setBit(pos + ind2[j], 1);
                        }
                    }
                }

                if (ibis::gVerbose > 1) {
                    long unsigned tcurr = time(0);
                    if (tcurr-59 > tlast) {
                        std::ostringstream ostmp;
                        ostmp << "TIME(" << tcurr
                              << "): just completed row "
                              << i << " of " << nEvents
                              << ", got " << pairs.cnt()
                              << " hit(s)";
                        logMessage("loopJoin", "%s",
                                   ostmp.str().c_str());
                        tlast = tcurr;
                    }
                }
            }
        }
        else { // ix1 is a list of indices
            for (unsigned i = 0; i < ix1.nIndices(); ++ i) {
                const ibis::bitvector64::word_t pos =
                    static_cast<ibis::bitvector64::word_t>(nEvents) * ind1[i];
                ierr = bar1.seek(ind1[i]);
                if (ierr < 0) {
                    logWarning("loopJoin", "failed to seek to row %lu for "
                               "the left side of the join",
                               static_cast<long unsigned>(ind1[i]));
                    break;
                }

                bar1.read();
                for (unsigned k = 0; k < ncmp; ++ k) {
                    double tmp = bar1.value(varind1[k]);
                    if (cmp[k]->getRange() != 0) {
                        double delta = fabs(cmp[k]->getRange()->eval());
                        lower[k] = tmp - delta;
                        upper[k] = tmp + delta;
                    }
                    else {
                        lower[k] = tmp;
                        upper[k] = tmp;
                    }
                }

                for (ibis::bitvector::indexSet ix2 = msk2.firstIndexSet();
                     ix2.nIndices() > 0; ++ ix2) {
                    const ibis::bitvector::word_t *ind2 = ix2.indices();
                    if (ix2.isRange()) { // ix2 is a range
                        const ibis::bitvector::word_t j0 = *ind2;
                        ierr = bar2.seek(j0);
                        if (ierr < 0) {
                            logWarning("loopJoin", "failed to seek to "
                                       "row %lu for the right side of the join",
                                       static_cast<long unsigned>(j0));
                            break;
                        }

                        for (unsigned j = j0;
                             j < ind2[1]; ++ j) {
                            bar2.read();
                            bool ishit = true;
                            for (unsigned k = 0; ishit && k < ncmp; ++ k) {
                                double tmp = bar2.value(varind2[k]);
                                ishit = (tmp >= lower[k] &&
                                         tmp <= upper[k]);
                            }
                            if (ishit)
                                pairs.setBit(pos + j, 1);
                        } // j
                    }
                    else { // ix2 is a list of indices
                        for (unsigned j = 0; j < ix2.nIndices(); ++ j) {
                            ierr = bar2.seek(ind2[j]);
                            if (ierr < 0) {
                                logWarning("loopJoin",
                                           "failed to seek to row %lu for "
                                           "the right side of the join",
                                           static_cast<long unsigned>(ind2[j]));
                                break;
                            }

                            bar2.read();
                            bool ishit = true;
                            for (unsigned k = 0; ishit && k < ncmp; ++ k) {
                                double tmp = bar2.value(varind2[k]);
                                ishit = (tmp >= lower[k] &&
                                         tmp <= upper[k]);
                            }
                            if (ishit)
                                pairs.setBit(pos + ind2[j], 1);
                        } // j
                    }
                }

                if (ibis::gVerbose > 1) {
                    long unsigned tcurr = time(0);
                    if (tcurr-59 > tlast) {
                        std::ostringstream ostmp;
                        ostmp << "TIME(" << tcurr
                              << "): just completed row "
                              << ind1[i] << " of " << nEvents
                              << ", got " << pairs.cnt()
                              << " hit(s)";
                        logMessage("loopJoin", "%s",
                                   ostmp.str().c_str());
                        tlast = tcurr;
                    }
                }
            }
        }
    } // for (ibis::bitvector::indexSet ix1 =...

    pairs.adjustSize(0, static_cast<ibis::bitvector64::word_t>(nEvents)*
                     static_cast<ibis::bitvector64::word_t>(nEvents));
    if (ibis::gVerbose > 2) {
        timer.stop();
        std::ostringstream ostr;
        ostr << '(' << *(cmp[0]);
        for (unsigned i = 1; i < ncmp; ++ i)
            ostr << " AND " << *(cmp[i]);
        ostr << ") producted " << pairs.cnt() << " hit(s)";
        logMessage("loopJoin", "%s took %g sec(CPU), %g sec(elapsed)",
                   ostr.str().c_str(), timer.CPUTime(), timer.realTime());
    }
    return pairs.cnt();
} // ibis::part::loopJoin

int64_t ibis::part::loopJoin(const std::vector<const ibis::deprecatedJoin*>& cmp,
                              const ibis::bitvector& mask) const {
    if (cmp.empty()) { // nothing to do
        return 0;
    }
    else if (cmp.size() == 1) { // special case, processed elsewhere
        return loopJoin(*(cmp[0]), mask);
    }

    ibis::horometer timer;
    timer.start();

    const unsigned ncmp = cmp.size();
    ibis::part::barrel bar1(this), bar2(this);
    std::vector<unsigned> varind1(ncmp);
    std::vector<unsigned> varind2(ncmp);
    for (unsigned i = 0; i < ncmp; ++ i) {
        varind1[i] = bar1.recordVariable(cmp[i]->getName1());
        varind2[i] = bar2.recordVariable(cmp[i]->getName2());
        if (cmp[i]->getRange() != 0)
            bar1.recordVariable(cmp[i]->getRange());
    }

    long ierr = bar1.open(this);
    if (ierr != 0) {
        logWarning("loopJoin", "failed to open bar1 for %lu variable%s "
                   "(ierr=%ld)", static_cast<long unsigned>(bar1.size()),
                   (bar1.size()>1?"s":""), ierr);
        return -1;
    }
    ierr = bar2.open(this);
    if (ierr != 0) {
        logWarning("loopJoin", "failed to open bar2 for %lu variable%s "
                   "(ierr=%ld)", static_cast<long unsigned>(bar2.size()),
                   (bar2.size()>1?"s":""), ierr);
        return -2;
    }

    ibis::bitvector msk1, msk2;
    msk1.copy(mask);
    msk2.copy(mask);
    bar1.getNullMask(msk1);
    bar2.getNullMask(msk2);

    int64_t cnt = 0;
    long unsigned tlast = time(0);
    std::vector<double> lower(ncmp), upper(ncmp);
    for (ibis::bitvector::indexSet ix1 = msk1.firstIndexSet();
         ix1.nIndices() > 0; ++ ix1) {
        const ibis::bitvector::word_t *ind1 = ix1.indices();
        if (ix1.isRange()) { // ix1 is a range
            ierr = bar1.seek(*ind1);
            if (ierr < 0) {
                logWarning("loopJoin", "failed to seek to row %lu for "
                           "the left side of the join",
                           static_cast<long unsigned>(*ind1));
                break;
            }

            for (unsigned i = *ind1; i < ind1[1]; ++ i) {
                bar1.read();
                for (unsigned k = 0; k < ncmp; ++ k) {
                    double tmp = bar1.value(varind1[k]);
                    if (cmp[k]->getRange() != 0) {
                        double delta = fabs(cmp[k]->getRange()->eval());
                        lower[k] = tmp - delta;
                        upper[k] = tmp + delta;
                    }
                    else {
                        lower[k] = tmp;
                        upper[k] = tmp;
                    }
                }

                for (ibis::bitvector::indexSet ix2 = msk2.firstIndexSet();
                     ix2.nIndices() > 0; ++ ix2) {
                    const ibis::bitvector::word_t *ind2 = ix2.indices();
                    if (ix2.isRange()) { // ix2 is a range
                        const ibis::bitvector::word_t j0 = *ind2;
                        ierr = bar2.seek(j0);
                        if (ierr < 0) {
                            logWarning("loopJoin", "failed to seek to "
                                       "row %lu for the right side of the join",
                                       static_cast<long unsigned>(j0));
                            break;
                        }

                        for (unsigned j = j0;
                             j < ind2[1]; ++ j) {
                            bar2.read();
                            bool ishit = true;
                            for (unsigned k = 0; ishit && k < ncmp; ++ k) {
                                double tmp = bar2.value(varind2[k]);
                                ishit = (tmp >= lower[k] &&
                                         tmp <= upper[k]);
                            }
                            cnt += (ishit);
                        }
                    }
                    else { // ix2 is a list of indices
                        for (unsigned j = 0; j < ix2.nIndices(); ++ j) {
                            ierr = bar2.seek(ind2[j]);
                            if (ierr < 0) {
                                logWarning("loopJoin",
                                           "failed to seek to row %lu for "
                                           "the right side of the join",
                                           static_cast<long unsigned>(ind2[j]));
                                break;
                            }

                            bar2.read();
                            bool ishit = true;
                            for (unsigned k = 0; ishit && k < ncmp; ++ k) {
                                double tmp = bar2.value(varind2[k]);
                                ishit = (tmp >= lower[k] &&
                                         tmp <= upper[k]);
                            }
                            cnt += (ishit);
                        }
                    }
                }

                if (ibis::gVerbose > 1) {
                    long unsigned tcurr = time(0);
                    if (tcurr-59 > tlast) {
                        std::ostringstream ostmp;
                        ostmp << "TIME(" << tcurr
                              << "): just completed row "
                              << i << " of " << nEvents
                              << ", got " << cnt
                              << " hit(s)";
                        logMessage("loopJoin", "%s",
                                   ostmp.str().c_str());
                        tlast = tcurr;
                    }
                }
            }
        }
        else { // ix1 is a list of indices
            for (unsigned i = 0; i < ix1.nIndices(); ++ i) {
                ierr = bar1.seek(ind1[i]);
                if (ierr < 0) {
                    logWarning("loopJoin", "failed to seek to row %lu for "
                               "the left side of the join",
                               static_cast<long unsigned>(ind1[i]));
                    break;
                }

                bar1.read();
                for (unsigned k = 0; k < ncmp; ++ k) {
                    double tmp = bar1.value(varind1[k]);
                    if (cmp[k]->getRange() != 0) {
                        double delta = fabs(cmp[k]->getRange()->eval());
                        lower[k] = tmp - delta;
                        upper[k] = tmp + delta;
                    }
                    else {
                        lower[k] = tmp;
                        upper[k] = tmp;
                    }
                }

                for (ibis::bitvector::indexSet ix2 = msk2.firstIndexSet();
                     ix2.nIndices() > 0; ++ ix2) {
                    const ibis::bitvector::word_t *ind2 = ix2.indices();
                    if (ix2.isRange()) { // ix2 is a range
                        const ibis::bitvector::word_t j0 = *ind2;
                        ierr = bar2.seek(j0);
                        if (ierr < 0) {
                            logWarning("loopJoin", "failed to seek to "
                                       "row %lu for the right side of the join",
                                       static_cast<long unsigned>(j0));
                            break;
                        }

                        for (unsigned j = j0;
                             j < ind2[1]; ++ j) {
                            bar2.read();
                            bool ishit = true;
                            for (unsigned k = 0; ishit && k < ncmp; ++ k) {
                                double tmp = bar2.value(varind2[k]);
                                ishit = (tmp >= lower[k] &&
                                         tmp <= upper[k]);
                            }
                            cnt += (ishit);
                        } // j
                    }
                    else { // ix2 is a list of indices
                        for (unsigned j = 0; j < ix2.nIndices(); ++ j) {
                            ierr = bar2.seek(ind2[j]);
                            if (ierr < 0) {
                                logWarning("loopJoin",
                                           "failed to seek to row %lu for "
                                           "the right side of the join",
                                           static_cast<long unsigned>(ind2[j]));
                                break;
                            }

                            bar2.read();
                            bool ishit = true;
                            for (unsigned k = 0; ishit && k < ncmp; ++ k) {
                                double tmp = bar2.value(varind2[k]);
                                ishit = (tmp >= lower[k] &&
                                         tmp <= upper[k]);
                            }
                            cnt += (ishit);
                        } // j
                    }
                }

                if (ibis::gVerbose > 1) {
                    long unsigned tcurr = time(0);
                    if (tcurr-59 > tlast) {
                        std::ostringstream ostmp;
                        ostmp << "TIME(" << tcurr
                              << "): just completed row "
                              << ind1[i] << " of " << nEvents
                              << ", got " << cnt
                              << " hit(s)";
                        logMessage("loopJoin", "%s",
                                   ostmp.str().c_str());
                        tlast = tcurr;
                    }
                }
            }
        }
    } // for (ibis::bitvector::indexSet ix1 =...

    if (ibis::gVerbose > 2) {
        timer.stop();
        std::ostringstream ostr;
        ostr << '(' << *(cmp[0]);
        for (unsigned i = 1; i < ncmp; ++ i)
            ostr << " AND " << *(cmp[i]);
        ostr << ") producted " << cnt << " hit(s)";
        logMessage("loopJoin", "%s took %g sec(CPU), %g sec(elapsed)",
                   ostr.str().c_str(), timer.CPUTime(), timer.realTime());
    }
    return cnt;
} // ibis::part::loopJoin
