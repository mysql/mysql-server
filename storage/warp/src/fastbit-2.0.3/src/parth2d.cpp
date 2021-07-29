// File $Id$
// Author: John Wu <John.Wu at ACM.org> Lawrence Berkeley National Laboratory
// Copyright (c) 2000-2016 the Regents of the University of California
//
// Implements ibis::part 2D histogram functions.
#include "index.h"      // ibis::index::divideCounts
#include "countQuery.h" // ibis::countQuery
#include "part.h"

#include <math.h>       // ceil, log, exp
#include <limits>       // std::numeric_limits
#include <typeinfo>     // typeid
#include <iomanip>      // setw, setprecision

// This file definte does not use the min and max macro.  Their presence
// could cause the calls to numeric_limits::min and numeric_limits::max to
// be misunderstood!
#undef max
#undef min

template <typename T1, typename T2>
long ibis::part::count2DBins(array_t<T1> &vals1,
                             const double &begin1, const double &end1,
                             const double &stride1,
                             array_t<T2> &vals2,
                             const double &begin2, const double &end2,
                             const double &stride2,
                             std::vector<uint32_t> &counts) const {
    const uint32_t dim2 = 1+
        static_cast<uint32_t>(floor((end2-begin2)/stride2));
    const uint32_t nr = (vals1.size() <= vals2.size() ?
                       vals1.size() : vals2.size());
#if defined(SORT_VALUES_BEFORE_COUNT)
    ibis::util::sortall(vals1, vals2);
// #else
//     if (counts.size() > 4096)
//      ibis::util::sortall(vals1, vals2);
#endif
    for (uint32_t ir = 0; ir < nr; ++ ir) {
        ++ counts[dim2 * static_cast<uint32_t>((vals1[ir]-begin1)/stride1) +
                  static_cast<uint32_t>((vals2[ir]-begin2)/stride2)];
    }
    return counts.size();
} // ibis::part::count2DBins

/// Count the number of values in 2D regular bins.
///
/// @note This function is intended to work with numerical values.  It
/// treats categorical values as unsigned ints.  Passing the name of text
/// column to this function will result in a negative return value.
///
/// @sa ibis::part::get1DDistribution
/// @sa ibis::table::getHistogram2D
long ibis::part::get2DDistribution(const char *constraints, const char *cname1,
                                   double begin1, double end1, double stride1,
                                   const char *cname2,
                                   double begin2, double end2, double stride2,
                                   std::vector<uint32_t> &counts) const {
    if (cname1 == 0 || *cname1 == 0 || (begin1 >= end1 && !(stride1 < 0.0)) ||
        (begin1 <= end1 && !(stride1 > 0.0)) ||
        cname2 == 0 || *cname2 == 0 || (begin2 >= end2 && !(stride2 < 0.0)) ||
        (begin2 <= end2 && !(stride2 > 0.0)))
        return -1L;

    const ibis::column* col1 = getColumn(cname1);
    const ibis::column* col2 = getColumn(cname2);
    if (col1 == 0 || col2 == 0)
        return -2L;

    ibis::horometer timer;
    if (ibis::gVerbose > 0) {
        LOGGER(ibis::gVerbose > 2)
            << "part[" << (m_name ? m_name : "")
            << "]::get2DDistribution attempting to compute a histogram of "
            << cname1 << " and " << cname2 << " with regular binning "
            << (constraints && *constraints ? "subject to " :
                "without constraints")
            << (constraints ? constraints : "");
        timer.start();
    }
    const uint32_t nbins =
        (1 + static_cast<uint32_t>(floor((end1 - begin1) / stride1))) *
        (1 + static_cast<uint32_t>(floor((end2 - begin2) / stride2)));
    if (counts.size() != nbins) {
        counts.resize(nbins);
        for (uint32_t i = 0; i < nbins; ++i)
            counts[i] = 0;
    }

    long ierr;
    ibis::bitvector hits;
    {
        ibis::countQuery qq(this);
        // add constraints on the two selected variables
        std::ostringstream oss;
        if (constraints != 0 && *constraints != 0)
            oss << "(" << constraints << ") AND ";
        oss << cname1 << " between " << std::setprecision(18) << begin1
            << " and " << std::setprecision(18) << end1 << " AND " << cname2
            << std::setprecision(18) << " between " << std::setprecision(18)
            << begin2 << " and " << std::setprecision(18) << end2;
        qq.setWhereClause(oss.str().c_str());

        ierr = qq.evaluate();
        if (ierr < 0)
            return ierr;
        ierr = qq.getNumHits();
        if (ierr <= 0)
            return ierr;
        hits.copy(*(qq.getHitVector()));
    }

    switch (col1->type()) {
    case ibis::BYTE:
    case ibis::SHORT:
    case ibis::INT: {
        array_t<int32_t>* vals1 = col1->selectInts(hits);
        if (vals1 == 0) {
            ierr = -4;
            break;
        }

        switch (col2->type()) {
        case ibis::BYTE:
        case ibis::SHORT:
        case ibis::INT: {
            array_t<int32_t>* vals2 = col2->selectInts(hits);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }
            ierr = count2DBins(*vals1, begin1, end1, stride1,
                               *vals2, begin2, end2, stride2, counts);
            delete vals2;
            break;}
        case ibis::UBYTE:
        case ibis::USHORT:
        case ibis::CATEGORY:
        case ibis::UINT: {
            array_t<uint32_t>* vals2 = col2->selectUInts(hits);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }
            ierr = count2DBins(*vals1, begin1, end1, stride1,
                               *vals2, begin2, end2, stride2, counts);
            delete vals2;
            break;}
        case ibis::ULONG:
        case ibis::LONG: {
            array_t<int64_t>* vals2 = col2->selectLongs(hits);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }
            ierr = count2DBins(*vals1, begin1, end1, stride1,
                               *vals2, begin2, end2, stride2, counts);
            delete vals2;
            break;}
        case ibis::FLOAT: {
            array_t<float>* vals2 = col2->selectFloats(hits);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }
            ierr = count2DBins(*vals1, begin1, end1, stride1,
                               *vals2, begin2, end2, stride2, counts);
            delete vals2;
            break;}
        case ibis::DOUBLE: {
            array_t<double>* vals2 = col2->selectDoubles(hits);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }
            ierr = count2DBins(*vals1, begin1, end1, stride1,
                               *vals2, begin2, end2, stride2, counts);
            delete vals2;
            break;}
        default: {
            LOGGER(ibis::gVerbose > 3)
                << "part::get2DDistribution -- can not "
                "handle column (" << cname2 << ") type "
                << ibis::TYPESTRING[(int)col2->type()];

            ierr = -3;
            break;}
        }
        delete vals1;
        break;}
    case ibis::UBYTE:
    case ibis::USHORT:
    case ibis::CATEGORY:
    case ibis::UINT: {
        array_t<uint32_t>* vals1 = col1->selectUInts(hits);
        if (vals1 == 0) {
            ierr = -4;
            break;
        }

        switch (col2->type()) {
        case ibis::BYTE:
        case ibis::SHORT:
        case ibis::INT: {
            array_t<int32_t>* vals2 = col2->selectInts(hits);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }
            ierr = count2DBins(*vals1, begin1, end1, stride1,
                               *vals2, begin2, end2, stride2, counts);
            delete vals2;
            break;}
        case ibis::UBYTE:
        case ibis::USHORT:
        case ibis::CATEGORY:
        case ibis::UINT: {
            array_t<uint32_t>* vals2 = col2->selectUInts(hits);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }
            ierr = count2DBins(*vals1, begin1, end1, stride1,
                               *vals2, begin2, end2, stride2, counts);
            delete vals2;
            break;}
        case ibis::ULONG:
        case ibis::LONG: {
            array_t<int64_t>* vals2 = col2->selectLongs(hits);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }
            ierr = count2DBins(*vals1, begin1, end1, stride1,
                               *vals2, begin2, end2, stride2, counts);
            delete vals2;
            break;}
        case ibis::FLOAT: {
            array_t<float>* vals2 = col2->selectFloats(hits);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }
            ierr = count2DBins(*vals1, begin1, end1, stride1,
                               *vals2, begin2, end2, stride2, counts);
            delete vals2;
            break;}
        case ibis::DOUBLE: {
            array_t<double>* vals2 = col2->selectDoubles(hits);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }
            ierr = count2DBins(*vals1, begin1, end1, stride1,
                               *vals2, begin2, end2, stride2, counts);
            delete vals2;
            break;}
        default: {
            LOGGER(ibis::gVerbose > 3)
                << "part::get2DDistribution -- can not "
                "handle column (" << cname2 << ") type "
                << ibis::TYPESTRING[(int)col2->type()];

            ierr = -3;
            break;}
        }
        delete vals1;
        break;}
    case ibis::ULONG:
    case ibis::LONG: {
        array_t<int64_t>* vals1 = col1->selectLongs(hits);
        if (vals1 == 0) {
            ierr = -4;
            break;
        }

        switch (col2->type()) {
        case ibis::BYTE:
        case ibis::SHORT:
        case ibis::INT: {
            array_t<int32_t>* vals2 = col2->selectInts(hits);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }
            ierr = count2DBins(*vals1, begin1, end1, stride1,
                               *vals2, begin2, end2, stride2, counts);
            delete vals2;
            break;}
        case ibis::UBYTE:
        case ibis::USHORT:
        case ibis::CATEGORY:
        case ibis::UINT: {
            array_t<uint32_t>* vals2 = col2->selectUInts(hits);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }
            ierr = count2DBins(*vals1, begin1, end1, stride1,
                               *vals2, begin2, end2, stride2, counts);
            delete vals2;
            break;}
        case ibis::ULONG:
        case ibis::LONG: {
            array_t<int64_t>* vals2 = col2->selectLongs(hits);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }
            ierr = count2DBins(*vals1, begin1, end1, stride1,
                               *vals2, begin2, end2, stride2, counts);
            delete vals2;
            break;}
        case ibis::FLOAT: {
            array_t<float>* vals2 = col2->selectFloats(hits);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }
            ierr = count2DBins(*vals1, begin1, end1, stride1,
                               *vals2, begin2, end2, stride2, counts);
            delete vals2;
            break;}
        case ibis::DOUBLE: {
            array_t<double>* vals2 = col2->selectDoubles(hits);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }
            ierr = count2DBins(*vals1, begin1, end1, stride1,
                               *vals2, begin2, end2, stride2, counts);
            delete vals2;
            break;}
        default: {
            LOGGER(ibis::gVerbose > 3)
                << "part::get2DDistribution -- can not "
                "handle column (" << cname2 << ") type "
                << ibis::TYPESTRING[(int)col2->type()];

            ierr = -3;
            break;}
        }
        delete vals1;
        break;}
    case ibis::FLOAT: {
        array_t<float>* vals1 = col1->selectFloats(hits);
        if (vals1 == 0) {
            ierr = -4;
            break;
        }

        switch (col2->type()) {
        case ibis::BYTE:
        case ibis::SHORT:
        case ibis::INT: {
            array_t<int32_t>* vals2 = col2->selectInts(hits);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }
            ierr = count2DBins(*vals1, begin1, end1, stride1,
                               *vals2, begin2, end2, stride2, counts);
            delete vals2;
            break;}
        case ibis::UBYTE:
        case ibis::USHORT:
        case ibis::CATEGORY:
        case ibis::UINT: {
            array_t<uint32_t>* vals2 = col2->selectUInts(hits);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }
            ierr = count2DBins(*vals1, begin1, end1, stride1,
                               *vals2, begin2, end2, stride2, counts);
            delete vals2;
            break;}
        case ibis::ULONG:
        case ibis::LONG: {
            array_t<int64_t>* vals2 = col2->selectLongs(hits);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }
            ierr = count2DBins(*vals1, begin1, end1, stride1,
                               *vals2, begin2, end2, stride2, counts);
            delete vals2;
            break;}
        case ibis::FLOAT: {
            array_t<float>* vals2 = col2->selectFloats(hits);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }
            ierr = count2DBins(*vals1, begin1, end1, stride1,
                               *vals2, begin2, end2, stride2, counts);
            delete vals2;
            break;}
        case ibis::DOUBLE: {
            array_t<double>* vals2 = col2->selectDoubles(hits);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }
            ierr = count2DBins(*vals1, begin1, end1, stride1,
                               *vals2, begin2, end2, stride2, counts);
            delete vals2;
            break;}
        default: {
            LOGGER(ibis::gVerbose > 3)
                << "part::get2DDistribution -- can not "
                "handle column (" << cname2 << ") type "
                << ibis::TYPESTRING[(int)col2->type()];

            ierr = -3;
            break;}
        }
        delete vals1;
        break;}
    case ibis::DOUBLE: {
        array_t<double>* vals1 = col1->selectDoubles(hits);
        if (vals1 == 0) {
            ierr = -4;
            break;
        }

        switch (col2->type()) {
        case ibis::BYTE:
        case ibis::SHORT:
        case ibis::INT: {
            array_t<int32_t>* vals2 = col2->selectInts(hits);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }
            ierr = count2DBins(*vals1, begin1, end1, stride1,
                               *vals2, begin2, end2, stride2, counts);
            delete vals2;
            break;}
        case ibis::UBYTE:
        case ibis::USHORT:
        case ibis::CATEGORY:
        case ibis::UINT: {
            array_t<uint32_t>* vals2 = col2->selectUInts(hits);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }
            ierr = count2DBins(*vals1, begin1, end1, stride1,
                               *vals2, begin2, end2, stride2, counts);
            delete vals2;
            break;}
        case ibis::ULONG:
        case ibis::LONG: {
            array_t<int64_t>* vals2 = col2->selectLongs(hits);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }
            ierr = count2DBins(*vals1, begin1, end1, stride1,
                               *vals2, begin2, end2, stride2, counts);
            delete vals2;
            break;}
        case ibis::FLOAT: {
            array_t<float>* vals2 = col2->selectFloats(hits);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }
            ierr = count2DBins(*vals1, begin1, end1, stride1,
                               *vals2, begin2, end2, stride2, counts);
            delete vals2;
            break;}
        case ibis::DOUBLE: {
            array_t<double>* vals2 = col2->selectDoubles(hits);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }
            ierr = count2DBins(*vals1, begin1, end1, stride1,
                               *vals2, begin2, end2, stride2, counts);
            delete vals2;
            break;}
        default: {
            LOGGER(ibis::gVerbose > 3)
                << "part::get2DDistribution -- can not "
                "handle column (" << cname2 << ") type "
                << ibis::TYPESTRING[(int)col2->type()];

            ierr = -3;
            break;}
        }
        delete vals1;
        break;}
    default: {
        LOGGER(ibis::gVerbose > 3)
            << "part::get2DDistribution -- can not "
            "handle column (" << cname1 << ") type "
            << ibis::TYPESTRING[(int)col1->type()];

        ierr = -3;
        break;}
    }
    if (ierr > 0 && ibis::gVerbose > 0) {
        timer.stop();
        logMessage("get2DDistribution", "computing the joint distribution of "
                   "column %s and %s%s%s took %g sec(CPU), %g sec(elapsed)",
                   cname1, cname2, (constraints ? " with restriction " : ""),
                   (constraints ? constraints : ""),
                   timer.CPUTime(), timer.realTime());
    }
    return ierr;
} // ibis::part::get2DDistribution

template <typename T1, typename T2>
long ibis::part::count2DWeights(array_t<T1> &vals1,
                                const double &begin1, const double &end1,
                                const double &stride1,
                                array_t<T2> &vals2,
                                const double &begin2, const double &end2,
                                const double &stride2,
                                array_t<double> &wts,
                                std::vector<double> &weights) const {
    const uint32_t dim2 = 1+
        static_cast<uint32_t>(floor((end2-begin2)/stride2));
    const uint32_t nr = (vals1.size() <= vals2.size() ?
                       vals1.size() : vals2.size());
#if defined(SORT_VALUES_BEFORE_COUNT)
    ibis::util::sortall(vals1, vals2);
// #else
//     if (counts.size() > 4096)
//      ibis::util::sortall(vals1, vals2);
#endif
    for (uint32_t ir = 0; ir < nr; ++ ir) {
        weights[dim2 * static_cast<uint32_t>((vals1[ir]-begin1)/stride1) +
                static_cast<uint32_t>((vals2[ir]-begin2)/stride2)]
            += wts[ir];
    }
    return weights.size();
} // ibis::part::count2DWeights

/// Count the weights of 2D regular bins.
///
/// @note This function is intended to work with numerical values.  It
/// treats categorical values as unsigned ints.  Passing the name of text
/// column to this function will result in a negative return value.
///
/// @sa ibis::part::get1DDistribution
/// @sa ibis::table::getHistogram2D
long ibis::part::get2DDistribution(const char *constraints, const char *cname1,
                                   double begin1, double end1, double stride1,
                                   const char *cname2,
                                   double begin2, double end2, double stride2,
                                   const char *wtname,
                                   std::vector<double> &weights) const {
    if (wtname == 0 || *wtname == 0 ||
        cname1 == 0 || *cname1 == 0 || (begin1 >= end1 && !(stride1 < 0.0)) ||
        (begin1 <= end1 && !(stride1 > 0.0)) ||
        cname2 == 0 || *cname2 == 0 || (begin2 >= end2 && !(stride2 < 0.0)) ||
        (begin2 <= end2 && !(stride2 > 0.0)))
        return -1L;

    const ibis::column* col1 = getColumn(cname1);
    const ibis::column* col2 = getColumn(cname2);
    const ibis::column* wcol = getColumn(wtname);
    if (col1 == 0 || col2 == 0 || wcol == 0)
        return -2L;

    ibis::horometer timer;
    if (ibis::gVerbose > 0) {
        LOGGER(ibis::gVerbose > 2)
            << "part[" << (m_name ? m_name : "")
            << "]::get2DDistribution attempting to compute a histogram of "
            << cname1 << " and " << cname2 << " with regular binning "
            << (constraints && *constraints ? "subject to " :
                "without constraints")
            << (constraints ? constraints : "") << " weighted with " << wtname;
        timer.start();
    }
    const uint32_t nbins =
        (1 + static_cast<uint32_t>(floor((end1 - begin1) / stride1))) *
        (1 + static_cast<uint32_t>(floor((end2 - begin2) / stride2)));
    if (weights.size() != nbins) {
        weights.resize(nbins);
        for (uint32_t i = 0; i < nbins; ++i)
            weights[i] = 0.0;
    }

    long ierr;
    ibis::bitvector hits;
    wcol->getNullMask(hits);
    {
        ibis::countQuery qq(this);

        // add constraints on the two selected variables
        std::ostringstream oss;
        if (constraints != 0 && *constraints != 0)
            oss << "(" << constraints << ") AND ";
        oss << cname1 << " between " << std::setprecision(18) << begin1
            << " and " << std::setprecision(18) << end1 << " AND " << cname2
            << std::setprecision(18) << " between " << std::setprecision(18)
            << begin2 << " and " << std::setprecision(18) << end2;
        qq.setWhereClause(oss.str().c_str());

        ierr = qq.evaluate();
        if (ierr < 0)
            return ierr;
        ierr = qq.getNumHits();
        if (ierr <= 0)
            return ierr;
        hits &= (*(qq.getHitVector()));
    }

    array_t<double> *wts = wcol->selectDoubles(hits);
    if (wts == 0) {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- part[" << (m_name ? m_name : "")
            << "]::get2DDistribution failed retrieve values from column "
            << wcol->name() << " as weights";
        return -3L;
    }
    switch (col1->type()) {
    case ibis::BYTE:
    case ibis::SHORT:
    case ibis::INT: {
        array_t<int32_t>* vals1 = col1->selectInts(hits);
        if (vals1 == 0) {
            ierr = -4;
            break;
        }

        switch (col2->type()) {
        case ibis::BYTE:
        case ibis::SHORT:
        case ibis::INT: {
            array_t<int32_t>* vals2 = col2->selectInts(hits);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }
            ierr = count2DWeights(*vals1, begin1, end1, stride1,
                                  *vals2, begin2, end2, stride2, *wts, weights);
            delete vals2;
            break;}
        case ibis::UBYTE:
        case ibis::USHORT:
        case ibis::CATEGORY:
        case ibis::UINT: {
            array_t<uint32_t>* vals2 = col2->selectUInts(hits);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }
            ierr = count2DWeights(*vals1, begin1, end1, stride1,
                                  *vals2, begin2, end2, stride2, *wts, weights);
            delete vals2;
            break;}
        case ibis::ULONG:
        case ibis::LONG: {
            array_t<int64_t>* vals2 = col2->selectLongs(hits);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }
            ierr = count2DWeights(*vals1, begin1, end1, stride1,
                                  *vals2, begin2, end2, stride2, *wts, weights);
            delete vals2;
            break;}
        case ibis::FLOAT: {
            array_t<float>* vals2 = col2->selectFloats(hits);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }
            ierr = count2DWeights(*vals1, begin1, end1, stride1,
                                  *vals2, begin2, end2, stride2, *wts, weights);
            delete vals2;
            break;}
        case ibis::DOUBLE: {
            array_t<double>* vals2 = col2->selectDoubles(hits);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }
            ierr = count2DWeights(*vals1, begin1, end1, stride1,
                                  *vals2, begin2, end2, stride2, *wts, weights);
            delete vals2;
            break;}
        default: {
            LOGGER(ibis::gVerbose > 3)
                << "part::get2DDistribution -- can not "
                "handle column (" << cname2 << ") type "
                << ibis::TYPESTRING[(int)col2->type()];

            ierr = -3;
            break;}
        }
        delete vals1;
        break;}
    case ibis::UBYTE:
    case ibis::USHORT:
    case ibis::CATEGORY:
    case ibis::UINT: {
        array_t<uint32_t>* vals1 = col1->selectUInts(hits);
        if (vals1 == 0) {
            ierr = -4;
            break;
        }

        switch (col2->type()) {
        case ibis::BYTE:
        case ibis::SHORT:
        case ibis::INT: {
            array_t<int32_t>* vals2 = col2->selectInts(hits);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }
            ierr = count2DWeights(*vals1, begin1, end1, stride1,
                                  *vals2, begin2, end2, stride2, *wts, weights);
            delete vals2;
            break;}
        case ibis::UBYTE:
        case ibis::USHORT:
        case ibis::CATEGORY:
        case ibis::UINT: {
            array_t<uint32_t>* vals2 = col2->selectUInts(hits);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }
            ierr = count2DWeights(*vals1, begin1, end1, stride1,
                                  *vals2, begin2, end2, stride2, *wts, weights);
            delete vals2;
            break;}
        case ibis::ULONG:
        case ibis::LONG: {
            array_t<int64_t>* vals2 = col2->selectLongs(hits);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }
            ierr = count2DWeights(*vals1, begin1, end1, stride1,
                                  *vals2, begin2, end2, stride2, *wts, weights);
            delete vals2;
            break;}
        case ibis::FLOAT: {
            array_t<float>* vals2 = col2->selectFloats(hits);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }
            ierr = count2DWeights(*vals1, begin1, end1, stride1,
                                  *vals2, begin2, end2, stride2, *wts, weights);
            delete vals2;
            break;}
        case ibis::DOUBLE: {
            array_t<double>* vals2 = col2->selectDoubles(hits);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }
            ierr = count2DWeights(*vals1, begin1, end1, stride1,
                                  *vals2, begin2, end2, stride2, *wts, weights);
            delete vals2;
            break;}
        default: {
            LOGGER(ibis::gVerbose > 3)
                << "part::get2DDistribution -- can not "
                "handle column (" << cname2 << ") type "
                << ibis::TYPESTRING[(int)col2->type()];

            ierr = -3;
            break;}
        }
        delete vals1;
        break;}
    case ibis::ULONG:
    case ibis::LONG: {
        array_t<int64_t>* vals1 = col1->selectLongs(hits);
        if (vals1 == 0) {
            ierr = -4;
            break;
        }

        switch (col2->type()) {
        case ibis::BYTE:
        case ibis::SHORT:
        case ibis::INT: {
            array_t<int32_t>* vals2 = col2->selectInts(hits);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }
            ierr = count2DWeights(*vals1, begin1, end1, stride1,
                                  *vals2, begin2, end2, stride2, *wts, weights);
            delete vals2;
            break;}
        case ibis::UBYTE:
        case ibis::USHORT:
        case ibis::CATEGORY:
        case ibis::UINT: {
            array_t<uint32_t>* vals2 = col2->selectUInts(hits);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }
            ierr = count2DWeights(*vals1, begin1, end1, stride1,
                                  *vals2, begin2, end2, stride2, *wts, weights);
            delete vals2;
            break;}
        case ibis::ULONG:
        case ibis::LONG: {
            array_t<int64_t>* vals2 = col2->selectLongs(hits);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }
            ierr = count2DWeights(*vals1, begin1, end1, stride1,
                                  *vals2, begin2, end2, stride2, *wts, weights);
            delete vals2;
            break;}
        case ibis::FLOAT: {
            array_t<float>* vals2 = col2->selectFloats(hits);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }
            ierr = count2DWeights(*vals1, begin1, end1, stride1,
                                  *vals2, begin2, end2, stride2, *wts, weights);
            delete vals2;
            break;}
        case ibis::DOUBLE: {
            array_t<double>* vals2 = col2->selectDoubles(hits);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }
            ierr = count2DWeights(*vals1, begin1, end1, stride1,
                                  *vals2, begin2, end2, stride2, *wts, weights);
            delete vals2;
            break;}
        default: {
            LOGGER(ibis::gVerbose > 3)
                << "part::get2DDistribution -- can not "
                "handle column (" << cname2 << ") type "
                << ibis::TYPESTRING[(int)col2->type()];

            ierr = -3;
            break;}
        }
        delete vals1;
        break;}
    case ibis::FLOAT: {
        array_t<float>* vals1 = col1->selectFloats(hits);
        if (vals1 == 0) {
            ierr = -4;
            break;
        }

        switch (col2->type()) {
        case ibis::BYTE:
        case ibis::SHORT:
        case ibis::INT: {
            array_t<int32_t>* vals2 = col2->selectInts(hits);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }
            ierr = count2DWeights(*vals1, begin1, end1, stride1,
                                  *vals2, begin2, end2, stride2, *wts, weights);
            delete vals2;
            break;}
        case ibis::UBYTE:
        case ibis::USHORT:
        case ibis::CATEGORY:
        case ibis::UINT: {
            array_t<uint32_t>* vals2 = col2->selectUInts(hits);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }
            ierr = count2DWeights(*vals1, begin1, end1, stride1,
                                  *vals2, begin2, end2, stride2, *wts, weights);
            delete vals2;
            break;}
        case ibis::ULONG:
        case ibis::LONG: {
            array_t<int64_t>* vals2 = col2->selectLongs(hits);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }
            ierr = count2DWeights(*vals1, begin1, end1, stride1,
                                  *vals2, begin2, end2, stride2, *wts, weights);
            delete vals2;
            break;}
        case ibis::FLOAT: {
            array_t<float>* vals2 = col2->selectFloats(hits);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }
            ierr = count2DWeights(*vals1, begin1, end1, stride1,
                                  *vals2, begin2, end2, stride2, *wts, weights);
            delete vals2;
            break;}
        case ibis::DOUBLE: {
            array_t<double>* vals2 = col2->selectDoubles(hits);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }
            ierr = count2DWeights(*vals1, begin1, end1, stride1,
                                  *vals2, begin2, end2, stride2, *wts, weights);
            delete vals2;
            break;}
        default: {
            LOGGER(ibis::gVerbose > 3)
                << "part::get2DDistribution -- can not "
                "handle column (" << cname2 << ") type "
                << ibis::TYPESTRING[(int)col2->type()];

            ierr = -3;
            break;}
        }
        delete vals1;
        break;}
    case ibis::DOUBLE: {
        array_t<double>* vals1 = col1->selectDoubles(hits);
        if (vals1 == 0) {
            ierr = -4;
            break;
        }

        switch (col2->type()) {
        case ibis::BYTE:
        case ibis::SHORT:
        case ibis::INT: {
            array_t<int32_t>* vals2 = col2->selectInts(hits);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }
            ierr = count2DWeights(*vals1, begin1, end1, stride1,
                                  *vals2, begin2, end2, stride2, *wts, weights);
            delete vals2;
            break;}
        case ibis::UBYTE:
        case ibis::USHORT:
        case ibis::CATEGORY:
        case ibis::UINT: {
            array_t<uint32_t>* vals2 = col2->selectUInts(hits);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }
            ierr = count2DWeights(*vals1, begin1, end1, stride1,
                                  *vals2, begin2, end2, stride2, *wts, weights);
            delete vals2;
            break;}
        case ibis::ULONG:
        case ibis::LONG: {
            array_t<int64_t>* vals2 = col2->selectLongs(hits);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }
            ierr = count2DWeights(*vals1, begin1, end1, stride1,
                                  *vals2, begin2, end2, stride2, *wts, weights);
            delete vals2;
            break;}
        case ibis::FLOAT: {
            array_t<float>* vals2 = col2->selectFloats(hits);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }
            ierr = count2DWeights(*vals1, begin1, end1, stride1,
                                  *vals2, begin2, end2, stride2, *wts, weights);
            delete vals2;
            break;}
        case ibis::DOUBLE: {
            array_t<double>* vals2 = col2->selectDoubles(hits);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }
            ierr = count2DWeights(*vals1, begin1, end1, stride1,
                                  *vals2, begin2, end2, stride2, *wts, weights);
            delete vals2;
            break;}
        default: {
            LOGGER(ibis::gVerbose > 3)
                << "part::get2DDistribution -- can not "
                "handle column (" << cname2 << ") type "
                << ibis::TYPESTRING[(int)col2->type()];

            ierr = -3;
            break;}
        }
        delete vals1;
        break;}
    default: {
        LOGGER(ibis::gVerbose > 3)
            << "part::get2DDistribution -- can not "
            "handle column (" << cname1 << ") type "
            << ibis::TYPESTRING[(int)col1->type()];

        ierr = -3;
        break;}
    }
    delete wts;
    if (ierr > 0 && ibis::gVerbose > 0) {
        timer.stop();
        logMessage("get2DDistribution", "computing the joint distribution of "
                   "column %s and %s%s%s took %g sec(CPU), %g sec(elapsed)",
                   cname1, cname2, (constraints ? " with restriction " : ""),
                   (constraints ? constraints : ""),
                   timer.CPUTime(), timer.realTime());
    }
    return ierr;
} // ibis::part::get2DDistribution

/// The pair of triplets, (begin1, end1, stride1) and (begin2, end2,
/// stride2) define <tt>(1 + floor((end1-begin1)/stride1)) (1 +
/// floor((end2-begin2)/stride2))</tt> 2D bins.  The 2D bins are packed
/// into the 1D array bins in raster scan order, with the second dimension
/// as the faster varying dimensioin.
///
/// @note All bitmaps that are empty are left with size() = 0.  All other
/// bitmaps have the same size() as mask.size().  When use these returned
/// bitmaps, please make sure to NOT mix empty bitmaps with non-empty
/// bitmaps in bitwise logical operations!
///
/// @sa ibis::part::file1DBins.
template <typename T1, typename T2>
long ibis::part::fill2DBins(const ibis::bitvector &mask,
                            const array_t<T1> &vals1,
                            const double &begin1, const double &end1,
                            const double &stride1,
                            const array_t<T2> &vals2,
                            const double &begin2, const double &end2,
                            const double &stride2,
                            std::vector<ibis::bitvector> &bins) const {
    if ((end1-begin1) * (end2-begin2) > 1e9 * stride1 * stride2 ||
        (end1-begin1) * stride1 < 0.0 || (end2-begin2) * stride2 < 0.0)
        return -10L;
    const uint32_t nbin2 = (1 + static_cast<uint32_t>((end2-begin2)/stride2));
    const uint32_t nbins = (1 + static_cast<uint32_t>((end1-begin1)/stride1)) *
        nbin2;
    uint32_t nvals = (vals1.size() <= vals2.size() ? vals1.size() : vals2.size());
    if (mask.size() == nvals) {
        bins.resize(nbins);
        for (ibis::bitvector::indexSet is = mask.firstIndexSet();
             is.nIndices() > 0; ++ is) {
            const ibis::bitvector::word_t *idx = is.indices();
            if (is.isRange()) {
                for (uint32_t j = *idx; j < idx[1]; ++ j) {
                    const uint32_t ibin1 =
                        static_cast<uint32_t>((vals1[j]-begin1)/stride1);
                    const uint32_t ibin2 =
                        static_cast<uint32_t>((vals2[j]-begin2)/stride2);
                    bins[ibin1*nbin2+ibin2].setBit(j, 1);
                }
            }
            else {
                for (uint32_t k = 0; k < is.nIndices(); ++ k) {
                    const ibis::bitvector::word_t j = idx[k];
                    const uint32_t ibin1 =
                        static_cast<uint32_t>((vals1[j]-begin1)/stride1);
                    const uint32_t ibin2 =
                        static_cast<uint32_t>((vals2[j]-begin2)/stride2);
                    bins[ibin1*nbin2+ibin2].setBit(j, 1);
                }
            }
        }
        for (uint32_t i = 0; i < nbins; ++ i)
            if (bins[i].size() > 0)
                bins[i].adjustSize(0, mask.size());
    }
    else if (mask.cnt() == nvals) {
        bins.resize(nbins);
        uint32_t ivals = 0;
        for (ibis::bitvector::indexSet is = mask.firstIndexSet();
             is.nIndices() > 0; ++ is) {
            const ibis::bitvector::word_t *idx = is.indices();
            if (is.isRange()) {
                for (uint32_t j = *idx; j < idx[1]; ++j, ++ ivals) {
                    const uint32_t ibin1 =
                        static_cast<uint32_t>((vals1[ivals]-begin1)/stride1);
                    const uint32_t ibin2 =
                        static_cast<uint32_t>((vals2[ivals]-begin2)/stride2);
                    bins[ibin1*nbin2+ibin2].setBit(j, 1);
                }
            }
            else {
                for (uint32_t k = 0; k < is.nIndices(); ++ k, ++ ivals) {
                    const ibis::bitvector::word_t j = idx[k];
                    const uint32_t ibin1 =
                        static_cast<uint32_t>((vals1[ivals]-begin1)/stride1);
                    const uint32_t ibin2 =
                        static_cast<uint32_t>((vals2[ivals]-begin2)/stride2);
                    bins[ibin1*nbin2+ibin2].setBit(j, 1);
                }
            }
        }
        for (uint32_t i = 0; i < nbins; ++ i)
            if (bins[i].size() > 0)
                bins[i].adjustSize(0, mask.size());
    }
    else {
        return -11L;
    }
    return (long)nbins;
} // ibis::part::fill2DBins

/// A template function to resolve the second variable involved in the 2D
/// bins.  The actual binning work done in ibis::part::fill2DBins.
template <typename T1>
long ibis::part::fill2DBins2(const ibis::bitvector &mask,
                             const array_t<T1> &val1,
                             const double &begin1, const double &end1,
                             const double &stride1,
                             const ibis::column &col2,
                             const double &begin2, const double &end2,
                             const double &stride2,
                             std::vector<ibis::bitvector> &bins) const {
    long ierr = 0;
    switch (col2.type()) {
#ifdef FASTBIT_EXPAND_ALL_TYPES
    case ibis::BYTE: {
        array_t<signed char>* val2;
        if (mask.cnt() > (nEvents >> 4)) {
            val2 = new array_t<signed char>;
            ierr = col2.getValuesArray(val2);
            if (ierr < 0) {
                delete val2;
                val2 = col2.selectBytes(mask);
            }
        }
        else {
            val2 = col2.selectBytes(mask);
        }
        if (val2 == 0) return -6L;
        ierr = fill2DBins(mask, val1, begin1, end1, stride1,
                          *val2, begin2, end2, stride2, bins);
        delete val2;
        break;}
    case ibis::UBYTE: {
        array_t<unsigned char>* val2;
        if (mask.cnt() > (nEvents >> 4)) {
            val2 = new array_t<unsigned char>;
            ierr = col2.getValuesArray(val2);
            if (ierr < 0) {
                delete val2;
                val2 = col2.selectUBytes(mask);
            }
        }
        else {
            val2 = col2.selectUBytes(mask);
        }
        if (val2 == 0) return -6L;
        ierr = fill2DBins(mask, val1, begin1, end1, stride1,
                          *val2, begin2, end2, stride2, bins);
        delete val2;
        break;}
    case ibis::SHORT: {
        array_t<int16_t>* val2;
        if (mask.cnt() > (nEvents >> 4)) {
            val2 = new array_t<int16_t>;
            ierr = col2.getValuesArray(val2);
            if (ierr < 0) {
                delete val2;
                val2 = col2.selectShorts(mask);
            }
        }
        else {
            val2 = col2.selectShorts(mask);
        }
        if (val2 == 0) return -6L;
        ierr = fill2DBins(mask, val1, begin1, end1, stride1,
                          *val2, begin2, end2, stride2, bins);
        delete val2;
        break;}
    case ibis::USHORT: {
        array_t<uint16_t>* val2;
        if (mask.cnt() > (nEvents >> 4)) {
            val2 = new array_t<uint16_t>;
            ierr = col2.getValuesArray(val2);
            if (ierr < 0) {
                delete val2;
                val2 = col2.selectUShorts(mask);
            }
        }
        else {
            val2 = col2.selectUShorts(mask);
        }
        if (val2 == 0) return -6L;
        ierr = fill2DBins(mask, val1, begin1, end1, stride1,
                          *val2, begin2, end2, stride2, bins);
        delete val2;
        break;}
#endif
#ifndef FASTBIT_EXPAND_ALL_TYPES
    case ibis::BYTE:
    case ibis::SHORT:
#endif
    case ibis::INT: {
        array_t<int32_t>* val2;
        if (mask.cnt() > (nEvents >> 4)
#ifndef FASTBIT_EXPAND_ALL_TYPES
            && col2.type() == ibis::INT
#endif
            ) {
            val2 = new array_t<int32_t>;
            ierr = col2.getValuesArray(val2);
            if (ierr < 0) {
                delete val2;
                val2 = col2.selectInts(mask);
            }
        }
        else {
            val2 = col2.selectInts(mask);
        }
        if (val2 == 0) return -6L;
        ierr = fill2DBins(mask, val1, begin1, end1, stride1,
                          *val2, begin2, end2, stride2, bins);
        delete val2;
        break;}
#ifndef FASTBIT_EXPAND_ALL_TYPES
    case ibis::UBYTE:
    case ibis::USHORT:
#endif
    case ibis::CATEGORY:
    case ibis::UINT: {
        array_t<uint32_t>* val2;
        if (mask.cnt() > (nEvents >> 4) && col2.type() == ibis::UINT) {
            val2 = new array_t<uint32_t>;
            ierr = col2.getValuesArray(val2);
            if (ierr < 0) {
                delete val2;
                val2 = col2.selectUInts(mask);
            }
        }
        else {
            val2 = col2.selectUInts(mask);
        }
        if (val2 == 0) return -6L;
        ierr = fill2DBins(mask, val1, begin1, end1, stride1, 
                          *val2, begin2, end2, stride2, bins);
        delete val2;
        break;}
    case ibis::ULONG:
#ifdef FASTBIT_EXPAND_ALL_TYPES
        {
        array_t<uint64_t>* val2;
        if (mask.cnt() > (nEvents >> 4)) {
            val2 = new array_t<uint64_t>;
            ierr = col2.getValuesArray(val2);
            if (ierr < 0) {
                delete val2;
                val2 = col2.selectULongs(mask);
            }
        }
        else {
            val2 = col2.selectULongs(mask);
        }
        if (val2 == 0) return -6L;
        ierr = fill2DBins(mask, val1, begin1, end1, stride1,
                          *val2, begin2, end2, stride2, bins);
        delete val2;
        break;}
#endif
    case ibis::LONG: {
        array_t<int64_t>* val2;
        if (mask.cnt() > (nEvents >> 4)) {
            val2 = new array_t<int64_t>;
            ierr = col2.getValuesArray(val2);
            if (ierr < 0) {
                delete val2;
                val2 = col2.selectLongs(mask);
            }
        }
        else {
            val2 = col2.selectLongs(mask);
        }
        if (val2 == 0) return -6L;
        ierr = fill2DBins(mask, val1, begin1, end1, stride1,
                          *val2, begin2, end2, stride2, bins);
        delete val2;
        break;}
    case ibis::FLOAT: {
        array_t<float>* val2;
        if (mask.cnt() > (nEvents >> 4)) {
            val2 = new array_t<float>;
            ierr = col2.getValuesArray(val2);
            if (ierr < 0) {
                delete val2;
                val2 = col2.selectFloats(mask);
            }
        }
        else {
            val2 = col2.selectFloats(mask);
        }
        if (val2 == 0) return -6L;
        ierr = fill2DBins(mask, val1, begin1, end1, stride1,
                          *val2, begin2, end2, stride2, bins);
        delete val2;
        break;}
    case ibis::DOUBLE: {
        array_t<double>* val2;
        if (mask.cnt() > (nEvents >> 4)) {
            val2 = new array_t<double>;
            ierr = col2.getValuesArray(val2);
            if (ierr < 0) {
                delete val2;
                val2 = col2.selectDoubles(mask);
            }
        }
        else {
            val2 = col2.selectDoubles(mask);
        }
        if (val2 == 0) return -6L;
        ierr = fill2DBins(mask, val1, begin1, end1, stride1,
                          *val2, begin2, end2, stride2, bins);
        delete val2;
        break;}
    default: {
        LOGGER(ibis::gVerbose > 3)
            << "part::fill2DBins2 -- can not "
            "handle column (" << col2.name() << ") type "
            << ibis::TYPESTRING[(int)col2.type()];

        ierr = -5;
        break;}
    }
    return ierr;
} // ibis::part::fill2DBins2

/// This function only checks the validity of the column names and resolve
/// the first column involved.  The second column is resolved in function
/// ibis::part::fill2DBins2, and the finally binning work is performed in
/// ibis::part::fill2DBins.  Please refer to the documentation for
/// ibis::part::fill2DBins for more information about the return variable
/// bins.  The return value of this function is the number of elements in
/// array bins upon successful completion of this function, which should be
/// exactly @code
/// (1 + floor((end1-begin1)/stride1)) * (1 + floor((end2-begin2)/stride2)).
/// @endcode
/// This function returns a negative value to indicate errors.
///
/// @note This function is intended to work with numerical values.  It
/// treats categorical values as unsigned ints.  Passing the name of text
/// column to this function will result in a negative return value.
long ibis::part::get2DBins(const char *constraints, const char *cname1,
                           double begin1, double end1, double stride1,
                           const char *cname2,
                           double begin2, double end2, double stride2,
                           std::vector<ibis::bitvector> &bins) const {
    if (cname1 == 0 || *cname1 == 0 || (begin1 >= end1 && !(stride1 < 0.0)) ||
        (begin1 <= end1 && !(stride1 > 0.0)) ||
        cname2 == 0 || *cname2 == 0 || (begin2 >= end2 && !(stride2 < 0.0)) ||
        (begin2 <= end2 && !(stride2 > 0.0)))
        return -1L;

    const ibis::column* col1 = getColumn(cname1);
    const ibis::column* col2 = getColumn(cname2);
    if (col1 == 0 || col2 == 0)
        return -2L;

    ibis::horometer timer;
    if (ibis::gVerbose > 0) {
        LOGGER(ibis::gVerbose > 2)
            << "part[" << (m_name ? m_name : "")
            << "]::get2DDistribution attempting to compute a histogram of "
            << cname1 << " and " << cname2 << " with regular binning "
            << (constraints && *constraints ? "subject to " :
                "without constraints")
            << (constraints ? constraints : "");
        timer.start();
    }

    long ierr;
    ibis::bitvector mask;
    {
        ibis::countQuery qq(this);
        // add constraints on the two selected variables
        std::ostringstream oss;
        if (constraints != 0 && *constraints != 0)
            oss << "(" << constraints << ") AND ";
        oss << cname1 << " between " << std::setprecision(18) << begin1
            << " and " << std::setprecision(18) << end1 << " AND " << cname2
            << std::setprecision(18) << " between " << std::setprecision(18)
            << begin2 << " and " << std::setprecision(18) << end2;
        qq.setWhereClause(oss.str().c_str());

        ierr = qq.evaluate();
        if (ierr < 0)
            return ierr;
        ierr = qq.getNumHits();
        if (ierr <= 0)
            return ierr;

        mask.copy(*(qq.getHitVector()));
    }

    switch (col1->type()) {
#ifdef FASTBIT_EXPAND_ALL_TYPES
    case ibis::BYTE: {
        array_t<signed char>* val1;
        if (mask.cnt() > (nEvents >> 4)) {
            val1 = new array_t<signed char>;
            ierr = col1->getValuesArray(val1);
            if (ierr < 0) {
                delete val1;
                val1 = col1->selectBytes(mask);
            }
        }
        else {
            val1 = col1->selectBytes(mask);
        }
        if (val1 == 0) return -4L;
        ierr = fill2DBins2(mask, *val1, begin1, end1, stride1,
                           *col2, begin2, end2, stride2, bins);
        delete val1;
        break;}
    case ibis::UBYTE: {
        array_t<unsigned char>* val1;
        if (mask.cnt() > (nEvents >> 4)) {
            val1 = new array_t<unsigned char>;
            ierr = col1->getValuesArray(val1);
            if (ierr < 0) {
                delete val1;
                val1 = col1->selectUBytes(mask);
            }
        }
        else {
            val1 = col1->selectUBytes(mask);
        }
        if (val1 == 0) return -4L;
        ierr = fill2DBins2(mask, *val1, begin1, end1, stride1,
                           *col2, begin2, end2, stride2, bins);
        delete val1;
        break;}
    case ibis::SHORT: {
        array_t<int16_t>* val1;
        if (mask.cnt() > (nEvents >> 4)) {
            val1 = new array_t<int16_t>;
            ierr = col1->getValuesArray(val1);
            if (ierr < 0) {
                delete val1;
                val1 = col1->selectShorts(mask);
            }
        }
        else {
            val1 = col1->selectShorts(mask);
        }
        if (val1 == 0) return -4L;
        ierr = fill2DBins2(mask, *val1, begin1, end1, stride1,
                           *col2, begin2, end2, stride2, bins);
        delete val1;
        break;}
    case ibis::USHORT: {
        array_t<uint16_t>* val1;
        if (mask.cnt() > (nEvents >> 4)) {
            val1 = new array_t<uint16_t>;
            ierr = col1->getValuesArray(val1);
            if (ierr < 0) {
                delete val1;
                val1 = col1->selectUShorts(mask);
            }
        }
        else {
            val1 = col1->selectUShorts(mask);
        }
        if (val1 == 0) return -4L;
        ierr = fill2DBins2(mask, *val1, begin1, end1, stride1,
                           *col2, begin2, end2, stride2, bins);
        delete val1;
        break;}
#endif
#ifndef FASTBIT_EXPAND_ALL_TYPES
    case ibis::BYTE:
    case ibis::SHORT:
#endif
    case ibis::INT: {
        array_t<int32_t>* val1;
        if (mask.cnt() > (nEvents >> 4)
#ifndef FASTBIT_EXPAND_ALL_TYPES
            && col1->type() == ibis::INT
#endif
            ) {
            val1 = new array_t<int32_t>;
            ierr = col1->getValuesArray(val1);
            if (ierr < 0) {
                delete val1;
                val1 = col1->selectInts(mask);
            }
        }
        else {
            val1 = col1->selectInts(mask);
        }
        if (val1 == 0) return -4L;
        ierr = fill2DBins2(mask, *val1, begin1, end1, stride1,
                           *col2, begin2, end2, stride2, bins);
        delete val1;
        break;}
#ifndef FASTBIT_EXPAND_ALL_TYPES
    case ibis::UBYTE:
    case ibis::USHORT:
#endif
    case ibis::CATEGORY:
    case ibis::UINT: {
        array_t<uint32_t>* val1;
        if (mask.cnt() > (nEvents >> 4) && col1->type() == ibis::UINT) {
            val1 = new array_t<uint32_t>;
            ierr = col1->getValuesArray(val1);
            if (ierr < 0) {
                delete val1;
                val1 = col1->selectUInts(mask);
            }
        }
        else {
            val1 = col1->selectUInts(mask);
        }
        if (val1 == 0) return -4L;
        ierr = fill2DBins2(mask, *val1, begin1, end1, stride1, 
                           *col2, begin2, end2, stride2, bins);
        delete val1;
        break;}
    case ibis::ULONG:
#ifdef FASTBIT_EXPAND_ALL_TYPES
        {
        array_t<uint64_t>* val1;
        if (mask.cnt() > (nEvents >> 4)) {
            val1 = new array_t<uint64_t>;
            ierr = col1->getValuesArray(val1);
            if (ierr < 0) {
                delete val1;
                val1 = col1->selectULongs(mask);
            }
        }
        else {
            val1 = col1->selectULongs(mask);
        }
        if (val1 == 0) return -4L;
        ierr = fill2DBins2(mask, *val1, begin1, end1, stride1,
                           *col2, begin2, end2, stride2, bins);
        delete val1;
        break;}
#endif
    case ibis::LONG: {
        array_t<int64_t>* val1;
        if (mask.cnt() > (nEvents >> 4)) {
            val1 = new array_t<int64_t>;
            ierr = col1->getValuesArray(val1);
            if (ierr < 0) {
                delete val1;
                val1 = col1->selectLongs(mask);
            }
        }
        else {
            val1 = col1->selectLongs(mask);
        }
        if (val1 == 0) return -4L;
        ierr = fill2DBins2(mask, *val1, begin1, end1, stride1,
                           *col2, begin2, end2, stride2, bins);
        delete val1;
        break;}
    case ibis::FLOAT: {
        array_t<float>* val1;
        if (mask.cnt() > (nEvents >> 4)) {
            val1 = new array_t<float>;
            ierr = col1->getValuesArray(val1);
            if (ierr < 0) {
                delete val1;
                val1 = col1->selectFloats(mask);
            }
        }
        else {
            val1 = col1->selectFloats(mask);
        }
        if (val1 == 0) return -4L;
        ierr = fill2DBins2(mask, *val1, begin1, end1, stride1,
                           *col2, begin2, end2, stride2, bins);
        delete val1;
        break;}
    case ibis::DOUBLE: {
        array_t<double>* val1;
        if (mask.cnt() > (nEvents >> 4)) {
            val1 = new array_t<double>;
            ierr = col1->getValuesArray(val1);
            if (ierr < 0) {
                delete val1;
                val1 = col1->selectDoubles(mask);
            }
        }
        else {
            val1 = col1->selectDoubles(mask);
        }
        if (val1 == 0) return -4;
        ierr = fill2DBins2(mask, *val1, begin1, end1, stride1,
                           *col2, begin2, end2, stride2, bins);
        delete val1;
        break;}
    default: {
        LOGGER(ibis::gVerbose > 3)
            << "part::get2DBins -- can not "
            "handle column (" << cname1 << ") type "
            << ibis::TYPESTRING[(int)col1->type()];

        ierr = -3;
        break;}
    }
    if (ierr > 0 && ibis::gVerbose > 0) {
        timer.stop();
        logMessage("get2DBins", "computing the distribution of column "
                   "%s and %s%s%s took %g sec(CPU), %g sec(elapsed)",
                   cname1, cname2, (constraints ? " with restriction " : ""),
                   (constraints ? constraints : ""),
                   timer.CPUTime(), timer.realTime());
    }
    return ierr;
} // ibis::part::get2DBins

/// This version returns a vector of pointers to bitmaps.  Because the
/// empty bitmaps are left as null pointers, it can reduce the memory usage
/// and the execution time if the majority of the bins are empty.
template <typename T1, typename T2>
long ibis::part::fill2DBins(const ibis::bitvector &mask,
                            const array_t<T1> &vals1,
                            const double &begin1, const double &end1,
                            const double &stride1,
                            const array_t<T2> &vals2,
                            const double &begin2, const double &end2,
                            const double &stride2,
                            std::vector<ibis::bitvector*> &bins) const {
    if ((end1-begin1) * (end2-begin2) > 1e9 * stride1 * stride2 ||
        (end1-begin1) * stride1 < 0.0 || (end2-begin2) * stride2 < 0.0)
        return -10L;
    const uint32_t nbin2 = (1 + static_cast<uint32_t>((end2-begin2)/stride2));
    const uint32_t nbins = (1 + static_cast<uint32_t>((end1-begin1)/stride1)) *
        nbin2;
    uint32_t nvals = (vals1.size() <= vals2.size() ? vals1.size() : vals2.size());
    if (mask.size() == nvals) {
        bins.resize(nbins);
        for (uint32_t i = 0; i < nbins; ++ i)
            bins[i] = 0;
        for (ibis::bitvector::indexSet is = mask.firstIndexSet();
             is.nIndices() > 0; ++ is) {
            const ibis::bitvector::word_t *idx = is.indices();
            if (is.isRange()) {
                for (uint32_t j = *idx; j < idx[1]; ++ j) {
                    const uint32_t ibin =
                        nbin2 * static_cast<uint32_t>((vals1[j]-begin1)/stride1) +
                        static_cast<uint32_t>((vals2[j]-begin2)/stride2);
                    if (bins[ibin] == 0)
                        bins[ibin] = new ibis::bitvector;
                    bins[ibin]->setBit(j, 1);
                }
            }
            else {
                for (uint32_t k = 0; k < is.nIndices(); ++ k) {
                    const ibis::bitvector::word_t j = idx[k];
                    const uint32_t ibin =
                        nbin2*static_cast<uint32_t>((vals1[j]-begin1)/stride1)+
                        static_cast<uint32_t>((vals2[j]-begin2)/stride2);
                    if (bins[ibin] == 0)
                        bins[ibin] = new ibis::bitvector;
                    bins[ibin]->setBit(j, 1);
                }
            }
        }
        for (uint32_t i = 0; i < nbins; ++ i)
            if (bins[i] != 0)
                bins[i]->adjustSize(0, mask.size());
    }
    else if (mask.cnt() == nvals) {
        bins.resize(nbins);
        for (uint32_t i = 0; i < nbins; ++ i)
            bins[i] = 0;
        uint32_t ivals = 0;
        for (ibis::bitvector::indexSet is = mask.firstIndexSet();
             is.nIndices() > 0; ++ is) {
            const ibis::bitvector::word_t *idx = is.indices();
            if (is.isRange()) {
                for (uint32_t j = *idx; j < idx[1]; ++j, ++ ivals) {
                    const uint32_t ibin =       nbin2 * 
                        static_cast<uint32_t>((vals1[ivals]-begin1)/stride1) +
                        static_cast<uint32_t>((vals2[ivals]-begin2)/stride2);
                    if (bins[ibin] == 0)
                        bins[ibin] = new ibis::bitvector;
                    bins[ibin]->setBit(j, 1);
                }
            }
            else {
                for (uint32_t k = 0; k < is.nIndices(); ++ k, ++ ivals) {
                    const ibis::bitvector::word_t j = idx[k];
                    const uint32_t ibin = nbin2 *
                        static_cast<uint32_t>((vals1[ivals]-begin1)/stride1) +
                        static_cast<uint32_t>((vals2[ivals]-begin2)/stride2);
                    if (bins[ibin] == 0)
                        bins[ibin] = new ibis::bitvector;
                    bins[ibin]->setBit(j, 1);
                }
            }
        }
        for (uint32_t i = 0; i < nbins; ++ i)
            if (bins[i] != 0)
                bins[i]->adjustSize(0, mask.size());
    }
    else {
        return -11L;
    }
    return (long)nbins;
} // ibis::part::fill2DBins

/// This version returns a vector of pointers to bitmaps.  Because the
/// empty bitmaps are left as null pointers, it can reduce the memory usage
/// and the execution time if the majority of the bins are empty.
template <typename T1>
long ibis::part::fill2DBins2(const ibis::bitvector &mask,
                             const array_t<T1> &val1,
                             const double &begin1, const double &end1,
                             const double &stride1,
                             const ibis::column &col2,
                             const double &begin2, const double &end2,
                             const double &stride2,
                             std::vector<ibis::bitvector*> &bins) const {
    long ierr = 0;
    switch (col2.type()) {
#ifdef FASTBIT_EXPAND_ALL_TYPES
    case ibis::BYTE: {
        array_t<signed char>* val2;
        if (mask.cnt() > (nEvents >> 4)) {
            val2 = new array_t<signed char>;
            ierr = col2.getValuesArray(val2);
            if (ierr < 0) {
                delete val2;
                val2 = col2.selectBytes(mask);
            }
        }
        else {
            val2 = col2.selectBytes(mask);
        }
        if (val2 == 0) return -6L;
        ierr = fill2DBins(mask, val1, begin1, end1, stride1,
                          *val2, begin2, end2, stride2, bins);
        delete val2;
        break;}
    case ibis::UBYTE: {
        array_t<unsigned char>* val2;
        if (mask.cnt() > (nEvents >> 4)) {
            val2 = new array_t<unsigned char>;
            ierr = col2.getValuesArray(val2);
            if (ierr < 0) {
                delete val2;
                val2 = col2.selectUBytes(mask);
            }
        }
        else {
            val2 = col2.selectUBytes(mask);
        }
        if (val2 == 0) return -6L;
        ierr = fill2DBins(mask, val1, begin1, end1, stride1,
                          *val2, begin2, end2, stride2, bins);
        delete val2;
        break;}
    case ibis::SHORT: {
        array_t<int16_t>* val2;
        if (mask.cnt() > (nEvents >> 4)) {
            val2 = new array_t<int16_t>;
            ierr = col2.getValuesArray(val2);
            if (ierr < 0) {
                delete val2;
                val2 = col2.selectShorts(mask);
            }
        }
        else {
            val2 = col2.selectShorts(mask);
        }
        if (val2 == 0) return -6L;
        ierr = fill2DBins(mask, val1, begin1, end1, stride1,
                          *val2, begin2, end2, stride2, bins);
        delete val2;
        break;}
    case ibis::USHORT: {
        array_t<uint16_t>* val2;
        if (mask.cnt() > (nEvents >> 4)) {
            val2 = new array_t<uint16_t>;
            ierr = col2.getValuesArray(val2);
            if (ierr < 0) {
                delete val2;
                val2 = col2.selectUShorts(mask);
            }
        }
        else {
            val2 = col2.selectUShorts(mask);
        }
        if (val2 == 0) return -6L;
        ierr = fill2DBins(mask, val1, begin1, end1, stride1,
                          *val2, begin2, end2, stride2, bins);
        delete val2;
        break;}
#endif
#ifndef FASTBIT_EXPAND_ALL_TYPES
    case ibis::BYTE:
    case ibis::SHORT:
#endif
    case ibis::INT: {
        array_t<int32_t>* val2;
        if (mask.cnt() > (nEvents >> 4)
#ifndef FASTBIT_EXPAND_ALL_TYPES
            && col2.type() == ibis::INT
#endif
            ) {
            val2 = new array_t<int32_t>;
            ierr = col2.getValuesArray(val2);
            if (ierr < 0) {
                delete val2;
                val2 = col2.selectInts(mask);
            }
        }
        else {
            val2 = col2.selectInts(mask);
        }
        if (val2 == 0) return -6L;
        ierr = fill2DBins(mask, val1, begin1, end1, stride1,
                          *val2, begin2, end2, stride2, bins);
        delete val2;
        break;}
#ifndef FASTBIT_EXPAND_ALL_TYPES
    case ibis::UBYTE:
    case ibis::USHORT:
#endif
    case ibis::CATEGORY:
    case ibis::UINT: {
        array_t<uint32_t>* val2;
        if (mask.cnt() > (nEvents >> 4) && col2.type() == ibis::UINT) {
            val2 = new array_t<uint32_t>;
            ierr = col2.getValuesArray(val2);
            if (ierr < 0) {
                delete val2;
                val2 = col2.selectUInts(mask);
            }
        }
        else {
            val2 = col2.selectUInts(mask);
        }
        if (val2 == 0) return -6L;
        ierr = fill2DBins(mask, val1, begin1, end1, stride1, 
                          *val2, begin2, end2, stride2, bins);
        delete val2;
        break;}
    case ibis::ULONG:
#ifdef FASTBIT_EXPAND_ALL_TYPES
        {
        array_t<uint64_t>* val2;
        if (mask.cnt() > (nEvents >> 4)) {
            val2 = new array_t<uint64_t>;
            ierr = col2.getValuesArray(val2);
            if (ierr < 0) {
                delete val2;
                val2 = col2.selectULongs(mask);
            }
        }
        else {
            val2 = col2.selectULongs(mask);
        }
        if (val2 == 0) return -6L;
        ierr = fill2DBins(mask, val1, begin1, end1, stride1,
                          *val2, begin2, end2, stride2, bins);
        delete val2;
        break;}
#endif
    case ibis::LONG: {
        array_t<int64_t>* val2;
        if (mask.cnt() > (nEvents >> 4)) {
            val2 = new array_t<int64_t>;
            ierr = col2.getValuesArray(val2);
            if (ierr < 0) {
                delete val2;
                val2 = col2.selectLongs(mask);
            }
        }
        else {
            val2 = col2.selectLongs(mask);
        }
        if (val2 == 0) return -6L;
        ierr = fill2DBins(mask, val1, begin1, end1, stride1,
                          *val2, begin2, end2, stride2, bins);
        delete val2;
        break;}
    case ibis::FLOAT: {
        array_t<float>* val2;
        if (mask.cnt() > (nEvents >> 4)) {
            val2 = new array_t<float>;
            ierr = col2.getValuesArray(val2);
            if (ierr < 0) {
                delete val2;
                val2 = col2.selectFloats(mask);
            }
        }
        else {
            val2 = col2.selectFloats(mask);
        }
        if (val2 == 0) return -6L;
        ierr = fill2DBins(mask, val1, begin1, end1, stride1,
                          *val2, begin2, end2, stride2, bins);
        delete val2;
        break;}
    case ibis::DOUBLE: {
        array_t<double>* val2;
        if (mask.cnt() > (nEvents >> 4)) {
            val2 = new array_t<double>;
            ierr = col2.getValuesArray(val2);
            if (ierr < 0) {
                delete val2;
                val2 = col2.selectDoubles(mask);
            }
        }
        else {
            val2 = col2.selectDoubles(mask);
        }
        if (val2 == 0) return -6L;
        ierr = fill2DBins(mask, val1, begin1, end1, stride1,
                          *val2, begin2, end2, stride2, bins);
        delete val2;
        break;}
    default: {
        LOGGER(ibis::gVerbose > 3)
            << "part::fill2DBins2 -- can not "
            "handle column (" << col2.name() << ") type "
            << ibis::TYPESTRING[(int)col2.type()];

        ierr = -5;
        break;}
    }
    return ierr;
} // ibis::part::fill2DBins2

/// This version returns a vector of pointers to bitmaps.  Because the
/// empty bitmaps are left as null pointers, it can reduce the memory usage
/// and the execution time if the majority of the bins are empty.
///
/// @note This function is intended to work with numerical values.  It
/// treats categorical values as unsigned ints.  Passing the name of text
/// column to this function will result in a negative return value.
long ibis::part::get2DBins(const char *constraints, const char *cname1,
                           double begin1, double end1, double stride1,
                           const char *cname2,
                           double begin2, double end2, double stride2,
                           std::vector<ibis::bitvector*> &bins) const {
    if (cname1 == 0 || *cname1 == 0 || (begin1 >= end1 && !(stride1 < 0.0)) ||
        (begin1 <= end1 && !(stride1 > 0.0)) ||
        cname2 == 0 || *cname2 == 0 || (begin2 >= end2 && !(stride2 < 0.0)) ||
        (begin2 <= end2 && !(stride2 > 0.0)))
        return -1L;

    const ibis::column* col1 = getColumn(cname1);
    const ibis::column* col2 = getColumn(cname2);
    if (col1 == 0 || col2 == 0)
        return -2L;

    ibis::horometer timer;
    if (ibis::gVerbose > 0) {
        LOGGER(ibis::gVerbose > 2)
            << "part[" << (m_name ? m_name : "")
            << "]::get2DDistribution attempting to compute a histogram of "
            << cname1 << " and " << cname2 << " with regular binning "
            << (constraints && *constraints ? "subject to " :
                "without constraints")
            << (constraints ? constraints : "");
        timer.start();
    }

    long ierr;
    ibis::bitvector mask;
    {
        ibis::countQuery qq(this);
        // add constraints on the two selected variables
        std::ostringstream oss;
        if (constraints != 0 && *constraints != 0)
            oss << "(" << constraints << ") AND ";
        oss << cname1 << " between " << std::setprecision(18) << begin1
            << " and " << std::setprecision(18) << end1 << " AND " << cname2
            << std::setprecision(18) << " between " << std::setprecision(18)
            << begin2 << " and " << std::setprecision(18) << end2;
        qq.setWhereClause(oss.str().c_str());

        ierr = qq.evaluate();
        if (ierr < 0)
            return ierr;
        ierr = qq.getNumHits();
        if (ierr <= 0)
            return ierr;

        mask.copy(*(qq.getHitVector()));
    }

    switch (col1->type()) {
#ifdef FASTBIT_EXPAND_ALL_TYPES
    case ibis::BYTE: {
        array_t<signed char>* val1;
        if (mask.cnt() > (nEvents >> 4)) {
            val1 = new array_t<signed char>;
            ierr = col1->getValuesArray(val1);
            if (ierr < 0) {
                delete val1;
                val1 = col1->selectBytes(mask);
            }
        }
        else {
            val1 = col1->selectBytes(mask);
        }
        if (val1 == 0) return -4L;
        ierr = fill2DBins2(mask, *val1, begin1, end1, stride1,
                           *col2, begin2, end2, stride2, bins);
        delete val1;
        break;}
    case ibis::UBYTE: {
        array_t<unsigned char>* val1;
        if (mask.cnt() > (nEvents >> 4)) {
            val1 = new array_t<unsigned char>;
            ierr = col1->getValuesArray(val1);
            if (ierr < 0) {
                delete val1;
                val1 = col1->selectUBytes(mask);
            }
        }
        else {
            val1 = col1->selectUBytes(mask);
        }
        if (val1 == 0) return -4L;
        ierr = fill2DBins2(mask, *val1, begin1, end1, stride1,
                           *col2, begin2, end2, stride2, bins);
        delete val1;
        break;}
    case ibis::SHORT: {
        array_t<int16_t>* val1;
        if (mask.cnt() > (nEvents >> 4)) {
            val1 = new array_t<int16_t>;
            ierr = col1->getValuesArray(val1);
            if (ierr < 0) {
                delete val1;
                val1 = col1->selectShorts(mask);
            }
        }
        else {
            val1 = col1->selectShorts(mask);
        }
        if (val1 == 0) return -4L;
        ierr = fill2DBins2(mask, *val1, begin1, end1, stride1,
                           *col2, begin2, end2, stride2, bins);
        delete val1;
        break;}
    case ibis::USHORT: {
        array_t<uint16_t>* val1;
        if (mask.cnt() > (nEvents >> 4)) {
            val1 = new array_t<uint16_t>;
            ierr = col1->getValuesArray(val1);
            if (ierr < 0) {
                delete val1;
                val1 = col1->selectUShorts(mask);
            }
        }
        else {
            val1 = col1->selectUShorts(mask);
        }
        if (val1 == 0) return -4L;
        ierr = fill2DBins2(mask, *val1, begin1, end1, stride1,
                           *col2, begin2, end2, stride2, bins);
        delete val1;
        break;}
#endif
#ifndef FASTBIT_EXPAND_ALL_TYPES
    case ibis::BYTE:
    case ibis::SHORT:
#endif
    case ibis::INT: {
        array_t<int32_t>* val1;
        if (mask.cnt() > (nEvents >> 4)
#ifndef FASTBIT_EXPAND_ALL_TYPES
            && col1->type() == ibis::INT
#endif
            ) {
            val1 = new array_t<int32_t>;
            ierr = col1->getValuesArray(val1);
            if (ierr < 0) {
                delete val1;
                val1 = col1->selectInts(mask);
            }
        }
        else {
            val1 = col1->selectInts(mask);
        }
        if (val1 == 0) return -4L;
        ierr = fill2DBins2(mask, *val1, begin1, end1, stride1,
                           *col2, begin2, end2, stride2, bins);
        delete val1;
        break;}
#ifndef FASTBIT_EXPAND_ALL_TYPES
    case ibis::UBYTE:
    case ibis::USHORT:
#endif
    case ibis::CATEGORY:
    case ibis::UINT: {
        array_t<uint32_t>* val1;
        if (mask.cnt() > (nEvents >> 4) && col1->type() == ibis::UINT) {
            val1 = new array_t<uint32_t>;
            ierr = col1->getValuesArray(val1);
            if (ierr < 0) {
                delete val1;
                val1 = col1->selectUInts(mask);
            }
        }
        else {
            val1 = col1->selectUInts(mask);
        }
        if (val1 == 0) return -4L;
        ierr = fill2DBins2(mask, *val1, begin1, end1, stride1, 
                           *col2, begin2, end2, stride2, bins);
        delete val1;
        break;}
    case ibis::ULONG:
#ifdef FASTBIT_EXPAND_ALL_TYPES
        {
        array_t<uint64_t>* val1;
        if (mask.cnt() > (nEvents >> 4)) {
            val1 = new array_t<uint64_t>;
            ierr = col1->getValuesArray(val1);
            if (ierr < 0) {
                delete val1;
                val1 = col1->selectULongs(mask);
            }
        }
        else {
            val1 = col1->selectULongs(mask);
        }
        if (val1 == 0) return -4L;
        ierr = fill2DBins2(mask, *val1, begin1, end1, stride1,
                           *col2, begin2, end2, stride2, bins);
        delete val1;
        break;}
#endif
    case ibis::LONG: {
        array_t<int64_t>* val1;
        if (mask.cnt() > (nEvents >> 4)) {
            val1 = new array_t<int64_t>;
            ierr = col1->getValuesArray(val1);
            if (ierr < 0) {
                delete val1;
                val1 = col1->selectLongs(mask);
            }
        }
        else {
            val1 = col1->selectLongs(mask);
        }
        if (val1 == 0) return -4L;
        ierr = fill2DBins2(mask, *val1, begin1, end1, stride1,
                           *col2, begin2, end2, stride2, bins);
        delete val1;
        break;}
    case ibis::FLOAT: {
        array_t<float>* val1;
        if (mask.cnt() > (nEvents >> 4)) {
            val1 = new array_t<float>;
            ierr = col1->getValuesArray(val1);
            if (ierr < 0) {
                delete val1;
                val1 = col1->selectFloats(mask);
            }
        }
        else {
            val1 = col1->selectFloats(mask);
        }
        if (val1 == 0) return -4L;
        ierr = fill2DBins2(mask, *val1, begin1, end1, stride1,
                           *col2, begin2, end2, stride2, bins);
        delete val1;
        break;}
    case ibis::DOUBLE: {
        array_t<double>* val1;
        if (mask.cnt() > (nEvents >> 4)) {
            val1 = new array_t<double>;
            ierr = col1->getValuesArray(val1);
            if (ierr < 0) {
                delete val1;
                val1 = col1->selectDoubles(mask);
            }
        }
        else {
            val1 = col1->selectDoubles(mask);
        }
        if (val1 == 0) return -4;
        ierr = fill2DBins2(mask, *val1, begin1, end1, stride1,
                           *col2, begin2, end2, stride2, bins);
        delete val1;
        break;}
    default: {
        LOGGER(ibis::gVerbose > 3)
            << "part::get2DBins -- can not "
            "handle column (" << cname1 << ") type "
            << ibis::TYPESTRING[(int)col1->type()];

        ierr = -3;
        break;}
    }
    if (ierr > 0 && ibis::gVerbose > 0) {
        timer.stop();
        logMessage("get2DBins", "computing the distribution of column "
                   "%s and %s%s%s took %g sec(CPU), %g sec(elapsed)",
                   cname1, cname2, (constraints ? " with restriction " : ""),
                   (constraints ? constraints : ""),
                   timer.CPUTime(), timer.realTime());
    }
    return ierr;
} // ibis::part::get2DBins

/// This version returns a vector of pointers to bitmaps.  Because the
/// empty bitmaps are left as null pointers, it can reduce the memory usage
/// and the execution time if the majority of the bins are empty.
template <typename T1, typename T2>
long ibis::part::fill2DBinsWeighted(const ibis::bitvector &mask,
                                    const array_t<T1> &vals1,
                                    const double &begin1, const double &end1,
                                    const double &stride1,
                                    const array_t<T2> &vals2,
                                    const double &begin2, const double &end2,
                                    const double &stride2,
                                    const array_t<double> &wts,
                                    std::vector<double> &weights,
                                    std::vector<ibis::bitvector*> &bins) const {
    if ((end1-begin1) * (end2-begin2) > 1e9 * stride1 * stride2 ||
        (end1-begin1) * stride1 < 0.0 || (end2-begin2) * stride2 < 0.0)
        return -10L;
    const uint32_t nbin2 = (1 + static_cast<uint32_t>((end2-begin2)/stride2));
    const uint32_t nbins = (1 + static_cast<uint32_t>((end1-begin1)/stride1)) *
        nbin2;
    uint32_t nvals = (vals1.size() <= vals2.size() ? vals1.size() : vals2.size());
    if (mask.size() == nvals && wts.size() == nvals) {
        bins.resize(nbins);
        weights.resize(nbins);
        for (uint32_t i = 0; i < nbins; ++ i) {
            weights[i] = 0.0;
            bins[i] = 0;
        }
        for (ibis::bitvector::indexSet is = mask.firstIndexSet();
             is.nIndices() > 0; ++ is) {
            const ibis::bitvector::word_t *idx = is.indices();
            if (is.isRange()) {
                for (uint32_t j = *idx; j < idx[1]; ++ j) {
                    const uint32_t ibin =
                        nbin2 * static_cast<uint32_t>((vals1[j]-begin1)/stride1) +
                        static_cast<uint32_t>((vals2[j]-begin2)/stride2);
                    if (bins[ibin] == 0)
                        bins[ibin] = new ibis::bitvector;
                    bins[ibin]->setBit(j, 1);
                    weights[ibin] += wts[j];
                }
            }
            else {
                for (uint32_t k = 0; k < is.nIndices(); ++ k) {
                    const ibis::bitvector::word_t j = idx[k];
                    const uint32_t ibin =
                        nbin2*static_cast<uint32_t>((vals1[j]-begin1)/stride1)+
                        static_cast<uint32_t>((vals2[j]-begin2)/stride2);
                    if (bins[ibin] == 0)
                        bins[ibin] = new ibis::bitvector;
                    bins[ibin]->setBit(j, 1);
                    weights[ibin] += wts[j];
                }
            }
        }
        for (uint32_t i = 0; i < nbins; ++ i)
            if (bins[i] != 0)
                bins[i]->adjustSize(0, mask.size());
    }
    else if (mask.cnt() == nvals && wts.size() == nvals) {
        bins.resize(nbins);
        weights.resize(nbins);
        for (uint32_t i = 0; i < nbins; ++ i) {
            weights[i] = 0.0;
            bins[i] = 0;
        }
        uint32_t ivals = 0;
        for (ibis::bitvector::indexSet is = mask.firstIndexSet();
             is.nIndices() > 0; ++ is) {
            const ibis::bitvector::word_t *idx = is.indices();
            if (is.isRange()) {
                for (uint32_t j = *idx; j < idx[1]; ++j, ++ ivals) {
                    const uint32_t ibin =       nbin2 * 
                        static_cast<uint32_t>((vals1[ivals]-begin1)/stride1) +
                        static_cast<uint32_t>((vals2[ivals]-begin2)/stride2);
                    if (bins[ibin] == 0)
                        bins[ibin] = new ibis::bitvector;
                    bins[ibin]->setBit(j, 1);
                    weights[ibin] += wts[ivals];
                }
            }
            else {
                for (uint32_t k = 0; k < is.nIndices(); ++ k, ++ ivals) {
                    const ibis::bitvector::word_t j = idx[k];
                    const uint32_t ibin = nbin2 *
                        static_cast<uint32_t>((vals1[ivals]-begin1)/stride1) +
                        static_cast<uint32_t>((vals2[ivals]-begin2)/stride2);
                    if (bins[ibin] == 0)
                        bins[ibin] = new ibis::bitvector;
                    bins[ibin]->setBit(j, 1);
                    weights[ibin] += wts[ivals];
                }
            }
        }
        for (uint32_t i = 0; i < nbins; ++ i)
            if (bins[i] != 0)
                bins[i]->adjustSize(0, mask.size());
    }
    else {
        return -11L;
    }
    return (long)nbins;
} // ibis::part::fill2DBins

/// This version returns a vector of pointers to bitmaps.  Because the
/// empty bitmaps are left as null pointers, it can reduce the memory usage
/// and the execution time if the majority of the bins are empty.
template <typename T1> long
ibis::part::fill2DBinsWeighted2(const ibis::bitvector &mask,
                                const array_t<T1> &val1,
                                const double &begin1, const double &end1,
                                const double &stride1,
                                const ibis::column &col2,
                                const double &begin2, const double &end2,
                                const double &stride2,
                                const array_t<double> &wts,
                                std::vector<double> &weights,
                                std::vector<ibis::bitvector*> &bins) const {
    long ierr = 0;
    switch (col2.type()) {
#ifdef FASTBIT_EXPAND_ALL_TYPES
    case ibis::BYTE: {
        array_t<signed char>* val2;
        if (mask.cnt() > (nEvents >> 4)) {
            val2 = new array_t<signed char>;
            ierr = col2.getValuesArray(val2);
            if (ierr < 0) {
                delete val2;
                val2 = col2.selectBytes(mask);
            }
        }
        else {
            val2 = col2.selectBytes(mask);
        }
        if (val2 == 0) return -6L;
        ierr = fill2DBinsWeighted(mask, val1, begin1, end1, stride1,
                                  *val2, begin2, end2, stride2,
                                  wts, weights, bins);
        delete val2;
        break;}
    case ibis::UBYTE: {
        array_t<unsigned char>* val2;
        if (mask.cnt() > (nEvents >> 4)) {
            val2 = new array_t<unsigned char>;
            ierr = col2.getValuesArray(val2);
            if (ierr < 0) {
                delete val2;
                val2 = col2.selectUBytes(mask);
            }
        }
        else {
            val2 = col2.selectUBytes(mask);
        }
        if (val2 == 0) return -6L;
        ierr = fill2DBinsWeighted(mask, val1, begin1, end1, stride1,
                                  *val2, begin2, end2, stride2,
                                  wts, weights, bins);
        delete val2;
        break;}
    case ibis::SHORT: {
        array_t<int16_t>* val2;
        if (mask.cnt() > (nEvents >> 4)) {
            val2 = new array_t<int16_t>;
            ierr = col2.getValuesArray(val2);
            if (ierr < 0) {
                delete val2;
                val2 = col2.selectShorts(mask);
            }
        }
        else {
            val2 = col2.selectShorts(mask);
        }
        if (val2 == 0) return -6L;
        ierr = fill2DBinsWeighted(mask, val1, begin1, end1, stride1,
                                  *val2, begin2, end2, stride2,
                                  wts, weights, bins);
        delete val2;
        break;}
    case ibis::USHORT: {
        array_t<uint16_t>* val2;
        if (mask.cnt() > (nEvents >> 4)) {
            val2 = new array_t<uint16_t>;
            ierr = col2.getValuesArray(val2);
            if (ierr < 0) {
                delete val2;
                val2 = col2.selectUShorts(mask);
            }
        }
        else {
            val2 = col2.selectUShorts(mask);
        }
        if (val2 == 0) return -6L;
        ierr = fill2DBinsWeighted(mask, val1, begin1, end1, stride1,
                                  *val2, begin2, end2, stride2,
                                  wts, weights, bins);
        delete val2;
        break;}
#endif
#ifndef FASTBIT_EXPAND_ALL_TYPES
    case ibis::BYTE:
    case ibis::SHORT:
#endif
    case ibis::INT: {
        array_t<int32_t>* val2;
        if (mask.cnt() > (nEvents >> 4)
#ifndef FASTBIT_EXPAND_ALL_TYPES
            && col2.type() == ibis::INT
#endif
            ) {
            val2 = new array_t<int32_t>;
            ierr = col2.getValuesArray(val2);
            if (ierr < 0) {
                delete val2;
                val2 = col2.selectInts(mask);
            }
        }
        else {
            val2 = col2.selectInts(mask);
        }
        if (val2 == 0) return -6L;
        ierr = fill2DBinsWeighted(mask, val1, begin1, end1, stride1,
                                  *val2, begin2, end2, stride2,
                                  wts, weights, bins);
        delete val2;
        break;}
#ifndef FASTBIT_EXPAND_ALL_TYPES
    case ibis::UBYTE:
    case ibis::USHORT:
#endif
    case ibis::CATEGORY:
    case ibis::UINT: {
        array_t<uint32_t>* val2;
        if (mask.cnt() > (nEvents >> 4) && col2.type() == ibis::UINT) {
            val2 = new array_t<uint32_t>;
            ierr = col2.getValuesArray(val2);
            if (ierr < 0) {
                delete val2;
                val2 = col2.selectUInts(mask);
            }
        }
        else {
            val2 = col2.selectUInts(mask);
        }
        if (val2 == 0) return -6L;
        ierr = fill2DBinsWeighted(mask, val1, begin1, end1, stride1, 
                                  *val2, begin2, end2, stride2,
                                  wts, weights, bins);
        delete val2;
        break;}
    case ibis::ULONG:
#ifdef FASTBIT_EXPAND_ALL_TYPES
        {
        array_t<uint64_t>* val2;
        if (mask.cnt() > (nEvents >> 4)) {
            val2 = new array_t<uint64_t>;
            ierr = col2.getValuesArray(val2);
            if (ierr < 0) {
                delete val2;
                val2 = col2.selectULongs(mask);
            }
        }
        else {
            val2 = col2.selectULongs(mask);
        }
        if (val2 == 0) return -6L;
        ierr = fill2DBinsWeighted(mask, val1, begin1, end1, stride1,
                                  *val2, begin2, end2, stride2,
                                  wts, weights, bins);
        delete val2;
        break;}
#endif
    case ibis::LONG: {
        array_t<int64_t>* val2;
        if (mask.cnt() > (nEvents >> 4)) {
            val2 = new array_t<int64_t>;
            ierr = col2.getValuesArray(val2);
            if (ierr < 0) {
                delete val2;
                val2 = col2.selectLongs(mask);
            }
        }
        else {
            val2 = col2.selectLongs(mask);
        }
        if (val2 == 0) return -6L;
        ierr = fill2DBinsWeighted(mask, val1, begin1, end1, stride1,
                                  *val2, begin2, end2, stride2,
                                  wts, weights, bins);
        delete val2;
        break;}
    case ibis::FLOAT: {
        array_t<float>* val2;
        if (mask.cnt() > (nEvents >> 4)) {
            val2 = new array_t<float>;
            ierr = col2.getValuesArray(val2);
            if (ierr < 0) {
                delete val2;
                val2 = col2.selectFloats(mask);
            }
        }
        else {
            val2 = col2.selectFloats(mask);
        }
        if (val2 == 0) return -6L;
        ierr = fill2DBinsWeighted(mask, val1, begin1, end1, stride1,
                                  *val2, begin2, end2, stride2,
                                  wts, weights, bins);
        delete val2;
        break;}
    case ibis::DOUBLE: {
        array_t<double>* val2;
        if (mask.cnt() > (nEvents >> 4)) {
            val2 = new array_t<double>;
            ierr = col2.getValuesArray(val2);
            if (ierr < 0) {
                delete val2;
                val2 = col2.selectDoubles(mask);
            }
        }
        else {
            val2 = col2.selectDoubles(mask);
        }
        if (val2 == 0) return -6L;
        ierr = fill2DBinsWeighted(mask, val1, begin1, end1, stride1,
                                  *val2, begin2, end2, stride2,
                                  wts, weights, bins);
        delete val2;
        break;}
    default: {
        LOGGER(ibis::gVerbose > 3)
            << "part::fill2DBinsWeighted2 -- can not "
            "handle column (" << col2.name() << ") type "
            << ibis::TYPESTRING[(int)col2.type()];

        ierr = -5;
        break;}
    }
    return ierr;
} // ibis::part::fill2DBinsWeighted2

/// This version returns a vector of pointers to bitmaps.  Because the
/// empty bitmaps are left as null pointers, it can reduce the memory usage
/// and the execution time if the majority of the bins are empty.
long ibis::part::get2DBins(const char *constraints, const char *cname1,
                           double begin1, double end1, double stride1,
                           const char *cname2,
                           double begin2, double end2, double stride2,
                           const char *wtname,
                           std::vector<double> &weights,
                           std::vector<ibis::bitvector*> &bins) const {
    if (wtname == 0 || *wtname == 0 ||
        cname1 == 0 || *cname1 == 0 || (begin1 >= end1 && !(stride1 < 0.0)) ||
        (begin1 <= end1 && !(stride1 > 0.0)) ||
        cname2 == 0 || *cname2 == 0 || (begin2 >= end2 && !(stride2 < 0.0)) ||
        (begin2 <= end2 && !(stride2 > 0.0)))
        return -1L;

    const ibis::column* col1 = getColumn(cname1);
    const ibis::column* col2 = getColumn(cname2);
    const ibis::column* wcol = getColumn(wtname);
    if (col1 == 0 || col2 == 0 || wcol == 0)
        return -2L;

    ibis::horometer timer;
    if (ibis::gVerbose > 0) {
        LOGGER(ibis::gVerbose > 2)
            << "part[" << (m_name ? m_name : "")
            << "]::get2DDistribution attempting to compute a histogram of "
            << cname1 << " and " << cname2 << " with regular binning "
            << (constraints && *constraints ? "subject to " :
                "without constraints")
            << (constraints ? constraints : "");
        timer.start();
    }

    long ierr;
    ibis::bitvector mask;
    wcol->getNullMask(mask);
    {
        ibis::countQuery qq(this);
        // add constraints on the two selected variables
        std::ostringstream oss;
        if (constraints != 0 && *constraints != 0)
            oss << "(" << constraints << ") AND ";
        oss << cname1 << " between " << std::setprecision(18) << begin1
            << " and " << std::setprecision(18) << end1 << " AND " << cname2
            << std::setprecision(18) << " between " << std::setprecision(18)
            << begin2 << " and " << std::setprecision(18) << end2;
        qq.setWhereClause(oss.str().c_str());

        ierr = qq.evaluate();
        if (ierr < 0)
            return ierr;
        ierr = qq.getNumHits();
        if (ierr <= 0)
            return ierr;

        mask &= (*(qq.getHitVector()));
    }

    array_t<double> *wts;
    if (mask.cnt() > (nEvents >> 4)) {
        ibis::bitvector tmp;
        tmp.set(1, nEvents);
        wts = wcol->selectDoubles(tmp);
    }
    else {
        wts = wcol->selectDoubles(mask);
    }
    if (wts == 0) {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- part[" << (m_name ? m_name : "")
            << "]::get2DBins failed retrieve values from column "
            << wcol->name() << " as weights";
        return -3L;
    }

    switch (col1->type()) {
#ifdef FASTBIT_EXPAND_ALL_TYPES
    case ibis::BYTE: {
        array_t<signed char>* val1;
        if (mask.cnt() > (nEvents >> 4)) {
            val1 = new array_t<signed char>;
            ierr = col1->getValuesArray(val1);
            if (ierr < 0) {
                delete val1;
                val1 = col1->selectBytes(mask);
            }
        }
        else {
            val1 = col1->selectBytes(mask);
        }
        if (val1 == 0) return -4L;
        ierr = fill2DBinsWeighted2(mask, *val1, begin1, end1, stride1,
                                   *col2, begin2, end2, stride2,
                                   *wts, weights, bins);
        delete val1;
        break;}
    case ibis::UBYTE: {
        array_t<unsigned char>* val1;
        if (mask.cnt() > (nEvents >> 4)) {
            val1 = new array_t<unsigned char>;
            ierr = col1->getValuesArray(val1);
            if (ierr < 0) {
                delete val1;
                val1 = col1->selectUBytes(mask);
            }
        }
        else {
            val1 = col1->selectUBytes(mask);
        }
        if (val1 == 0) return -4L;
        ierr = fill2DBinsWeighted2(mask, *val1, begin1, end1, stride1,
                                   *col2, begin2, end2, stride2,
                                   *wts, weights, bins);
        delete val1;
        break;}
    case ibis::SHORT: {
        array_t<int16_t>* val1;
        if (mask.cnt() > (nEvents >> 4)) {
            val1 = new array_t<int16_t>;
            ierr = col1->getValuesArray(val1);
            if (ierr < 0) {
                delete val1;
                val1 = col1->selectShorts(mask);
            }
        }
        else {
            val1 = col1->selectShorts(mask);
        }
        if (val1 == 0) return -4L;
        ierr = fill2DBinsWeighted2(mask, *val1, begin1, end1, stride1,
                                   *col2, begin2, end2, stride2,
                                   *wts, weights, bins);
        delete val1;
        break;}
    case ibis::USHORT: {
        array_t<uint16_t>* val1;
        if (mask.cnt() > (nEvents >> 4)) {
            val1 = new array_t<uint16_t>;
            ierr = col1->getValuesArray(val1);
            if (ierr < 0) {
                delete val1;
                val1 = col1->selectUShorts(mask);
            }
        }
        else {
            val1 = col1->selectUShorts(mask);
        }
        if (val1 == 0) return -4L;
        ierr = fill2DBinsWeighted2(mask, *val1, begin1, end1, stride1,
                                   *col2, begin2, end2, stride2,
                                   *wts, weights, bins);
        delete val1;
        break;}
#endif
#ifndef FASTBIT_EXPAND_ALL_TYPES
    case ibis::BYTE:
    case ibis::SHORT:
#endif
    case ibis::INT: {
        array_t<int32_t>* val1;
        if (mask.cnt() > (nEvents >> 4)
#ifndef FASTBIT_EXPAND_ALL_TYPES
            && col1->type() == ibis::INT
#endif
            ) {
            val1 = new array_t<int32_t>;
            ierr = col1->getValuesArray(val1);
            if (ierr < 0) {
                delete val1;
                val1 = col1->selectInts(mask);
            }
        }
        else {
            val1 = col1->selectInts(mask);
        }
        if (val1 == 0) return -4L;
        ierr = fill2DBinsWeighted2(mask, *val1, begin1, end1, stride1,
                                   *col2, begin2, end2, stride2,
                                   *wts, weights, bins);
        delete val1;
        break;}
#ifndef FASTBIT_EXPAND_ALL_TYPES
    case ibis::UBYTE:
    case ibis::USHORT:
#endif
    case ibis::CATEGORY:
    case ibis::UINT: {
        array_t<uint32_t>* val1;
        if (mask.cnt() > (nEvents >> 4) && col1->type() == ibis::UINT) {
            val1 = new array_t<uint32_t>;
            ierr = col1->getValuesArray(val1);
            if (ierr < 0) {
                delete val1;
                val1 = col1->selectUInts(mask);
            }
        }
        else {
            val1 = col1->selectUInts(mask);
        }
        if (val1 == 0) return -4L;
        ierr = fill2DBinsWeighted2(mask, *val1, begin1, end1, stride1, 
                                   *col2, begin2, end2, stride2,
                                   *wts, weights, bins);
        delete val1;
        break;}
    case ibis::ULONG:
#ifdef FASTBIT_EXPAND_ALL_TYPES
        {
        array_t<uint64_t>* val1;
        if (mask.cnt() > (nEvents >> 4)) {
            val1 = new array_t<uint64_t>;
            ierr = col1->getValuesArray(val1);
            if (ierr < 0) {
                delete val1;
                val1 = col1->selectULongs(mask);
            }
        }
        else {
            val1 = col1->selectULongs(mask);
        }
        if (val1 == 0) return -4L;
        ierr = fill2DBinsWeighted2(mask, *val1, begin1, end1, stride1,
                                   *col2, begin2, end2, stride2,
                                   *wts, weights, bins);
        delete val1;
        break;}
#endif
    case ibis::LONG: {
        array_t<int64_t>* val1;
        if (mask.cnt() > (nEvents >> 4)) {
            val1 = new array_t<int64_t>;
            ierr = col1->getValuesArray(val1);
            if (ierr < 0) {
                delete val1;
                val1 = col1->selectLongs(mask);
            }
        }
        else {
            val1 = col1->selectLongs(mask);
        }
        if (val1 == 0) return -4L;
        ierr = fill2DBinsWeighted2(mask, *val1, begin1, end1, stride1,
                                   *col2, begin2, end2, stride2,
                                   *wts, weights, bins);
        delete val1;
        break;}
    case ibis::FLOAT: {
        array_t<float>* val1;
        if (mask.cnt() > (nEvents >> 4)) {
            val1 = new array_t<float>;
            ierr = col1->getValuesArray(val1);
            if (ierr < 0) {
                delete val1;
                val1 = col1->selectFloats(mask);
            }
        }
        else {
            val1 = col1->selectFloats(mask);
        }
        if (val1 == 0) return -4L;
        ierr = fill2DBinsWeighted2(mask, *val1, begin1, end1, stride1,
                                   *col2, begin2, end2, stride2,
                                   *wts, weights, bins);
        delete val1;
        break;}
    case ibis::DOUBLE: {
        array_t<double>* val1;
        if (mask.cnt() > (nEvents >> 4)) {
            val1 = new array_t<double>;
            ierr = col1->getValuesArray(val1);
            if (ierr < 0) {
                delete val1;
                val1 = col1->selectDoubles(mask);
            }
        }
        else {
            val1 = col1->selectDoubles(mask);
        }
        if (val1 == 0) return -4;
        ierr = fill2DBinsWeighted2(mask, *val1, begin1, end1, stride1,
                                   *col2, begin2, end2, stride2,
                                   *wts, weights, bins);
        delete val1;
        break;}
    default: {
        LOGGER(ibis::gVerbose > 3)
            << "part::get2DBins -- can not "
            "handle column (" << cname1 << ") type "
            << ibis::TYPESTRING[(int)col1->type()];

        ierr = -3;
        break;}
    }
    if (ierr > 0 && ibis::gVerbose > 0) {
        timer.stop();
        logMessage("get2DBins", "computing the distribution of column "
                   "%s and %s%s%s took %g sec(CPU), %g sec(elapsed)",
                   cname1, cname2, (constraints ? " with restriction " : ""),
                   (constraints ? constraints : ""),
                   timer.CPUTime(), timer.realTime());
    }
    return ierr;
} // ibis::part::get2DBins

/// Adaptive binning through regularly spaced bins.  It goes through the
/// arrays twice, once to compute the actual minimum and maximum values and
/// once to count the entries in each bins.  It produces three sets of
/// bins: the 1-D bins for vals1 and vals2, and a 2-D bin at a high
/// resolution.  It then combine the 1-D bins to form nearly equal-weight
/// bins and use that grouping to decide how to combine the 2-D bins to
/// form the final output.
///
/// @note The number of fine bins used internally is dynamically determined
/// based on the number of desired bins on output, nb1 and nb2, as well as
/// the number of records in vals1 and vals2.
///
/// @note The output number of bins may not be exactly nb1*nb2.  Here are
/// some of the reasons.
/// - If either nb1 or nb2 is less than or equal to one, it is set 100.
/// - If either nb1 or nb2 is larger than 2048, it may be reset to a
///   smaller value so that the number of records in each bin might be about
///   cubic root of the total number of records on input.
/// - It may be necessary to use a few more or less bins (along each
///   dimension) to avoid grouping very popular values with very unpopular
///   values into the same bin.
template <typename T1, typename T2> long
ibis::part::adaptive2DBins(const array_t<T1> &vals1,
                           const array_t<T2> &vals2,
                           uint32_t nb1, uint32_t nb2,
                           std::vector<double> &bounds1,
                           std::vector<double> &bounds2,
                           std::vector<uint32_t> &counts) {
    const uint32_t nrows = (vals1.size() <= vals2.size() ?
                            vals1.size() : vals2.size());
    if (nrows == 0) {
        bounds1.clear();
        bounds2.clear();
        counts.clear();
        return 0L;
    }

    T1 vmin1, vmax1;
    T2 vmin2, vmax2;
    vmin1 = vals1[0];
    vmax1 = vals1[0];
    vmin2 = vals2[0];
    vmax2 = vals2[0];
    for (uint32_t i = 1; i < nrows; ++ i) {
        if (vmin1 > vals1[i])
            vmin1 = vals1[i];
        if (vmax1 < vals1[i])
            vmax1 = vals1[i];
        if (vmin2 > vals2[i])
            vmin2 = vals2[i];
        if (vmax2 < vals2[i])
            vmax2 = vals2[i];
    }
    if (vmin1 >= vmax1) { // vals1 has only one single value
        bounds1.resize(2);
        bounds1[0] = vmin1;
        bounds1[1] = ibis::util::incrDouble(static_cast<double>(vmin1));
        if (vmin2 >= vmax2) { // vals2 has only one single value as well
            bounds2.resize(2);
            bounds2[0] = vmin2;
            bounds2[1] = ibis::util::incrDouble(static_cast<double>(vmin2));
            counts.resize(1);
            counts[0] = nrows;
        }
        else { // one-dimensional adaptive binning
            adaptiveFloats(vals2, vmin2, vmax2, nb2, bounds2, counts);
        }
        return counts.size();
    }
    else if (vmin2 >= vmax2) { // vals2 has one one single value, bin vals2
        bounds2.resize(2);
        bounds2[0] = vmin2;
        bounds2[1] = ibis::util::incrDouble(static_cast<double>(vmin2));
        return adaptiveFloats(vals1, vmin1, vmax1, nb1, bounds1, counts);
    }

    // normal case, both vals1 and vals2 have multiple distinct values
    // ==> nrows > 1
    std::string mesg;
    {
        std::ostringstream oss;
        oss << "part::adaptive2DBins<" << typeid(T1).name() << ", "
            << typeid(T2).name() << ">";
        mesg = oss.str();
    }
    ibis::util::timer atimer(mesg.c_str(), 3);
    if (nb1 <= 1) nb1 = 100;
    if (nb2 <= 1) nb2 = 100;
    double tmp = exp(log((double)nrows)/3.0);
    if (nb1 > 2048 && (double)nb1 > tmp) {
        if (nrows > 10000000)
            nb1 = static_cast<uint32_t>(0.5 + tmp);
        else
            nb1 = 2048;
    }
    if (nb2 > 2048 && (double)nb2 > tmp) {
        if (nrows > 10000000)
            nb2 = static_cast<uint32_t>(0.5 + tmp);
        else
            nb2 = 2048;
    }
    tmp = exp(log((double)nrows/(double)(nb1*nb2))/3.0);
    if (tmp < 2.0) tmp = 2.0;
    const uint32_t nfine1 = static_cast<uint32_t>(0.5 + tmp * nb1);
    const uint32_t nfine2 = static_cast<uint32_t>(0.5 + tmp * nb2);
    const double scale1 = (1.0 - nfine1 * DBL_EPSILON) *
        ((double)nfine1 / (double)(vmax1 - vmin1));
    const double scale2 = (1.0 - nfine2 * DBL_EPSILON) *
        ((double)nfine2 / (double)(vmax2 - vmin2));
    LOGGER(ibis::gVerbose > 3)
        << mesg << " internally uses " << nfine1 << " x " << nfine2
        << " uniform bins for " << nrows << " records in the range of ["
        << vmin1 << ", " << vmax1 << "] x [" << vmin2 << ", " << vmax2
        << "], expected final bins to be [" << nb1 << "] x [" << nb2 << ']';

    array_t<uint32_t> cnts1(nfine1,0), cnts2(nfine2,0), cntsa(nfine1*nfine2,0);
    // loop to count values in fine bins
    for (uint32_t i = 0; i < nrows; ++ i) {
        const uint32_t j1 = static_cast<uint32_t>((vals1[i]-vmin1)*scale1);
        const uint32_t j2 = static_cast<uint32_t>((vals2[i]-vmin2)*scale2);
        ++ cnts1[j1];
        ++ cnts2[j2];
        ++ cntsa[j1*nfine2+j2];
#if defined(DEBUG) || defined(_DEBUG)
        if (j1 >= nfine1 || j2 >= nfine2) {
            ibis::util::logger lg;
            if (j1 >= nfine1)
                lg() << "DEBUG -- Warning -- j1 (" << j1
                     << ") is out of bound (>=" << nfine1
                     << ") in " << mesg;
            if (j2 >= nfine2)
                lg() << "DEBUG -- Warning -- j2 (" << j2
                     << ") is out of bound (>=" << nfine2
                     << ") in " << mesg;
        }
#endif
    }
    // divide the fine bins into final bins
    array_t<uint32_t> bnds1(nb1), bnds2(nb2);
    ibis::index::divideCounts(bnds1, cnts1);
    ibis::index::divideCounts(bnds2, cnts2);
    nb1 = bnds1.size(); // the final size
    nb2 = bnds2.size();
    LOGGER(ibis::gVerbose > 4)
        << mesg << " is to use " << nb1 << " x " << nb2
        << " adaptive bins for a 2D histogram";

    bounds1.resize(nb1+1);
    bounds1[0] = vmin1;
    for (uint32_t i = 0; i < nb1; ++ i)
        bounds1[i+1] = vmin1 + bnds1[i] / scale1;

    bounds2.resize(nb2+1);
    bounds2[0] = vmin2;
    for (uint32_t i = 0; i < nb2; ++ i)
        bounds2[i+1] = vmin2 + bnds2[i] / scale2;

    counts.resize(nb1*nb2);
    counts[0] = 0;
    for (uint32_t i1 = 0; i1 < bnds1[0]; ++ i1) {
        uint32_t off1 = i1 * nfine2;
        for (uint32_t i2 = off1; i2 < off1+bnds2[0]; ++ i2) {
            counts[0] += cntsa[i2];
        }
    }
    for (uint32_t j2 = 1; j2 < nb2; ++ j2) {
        counts[j2] = 0;
        for (uint32_t i1 = 0; i1 < bnds1[0]; ++ i1) {
            uint32_t off1 = i1 * nfine2;
            for (uint32_t i2 = off1+bnds2[j2-1]; i2 < off1+bnds2[j2]; ++ i2)
                counts[j2] += cntsa[i2];
        }
    }
    for (uint32_t j1 = 1; j1 < nb1; ++ j1) {
        uint32_t joff = j1 * nb2;
        counts[joff] = 0;
        for (uint32_t i1 = bnds1[j1-1]; i1 < bnds1[j1]; ++ i1) {
            uint32_t ioff = i1 * nfine2;
            for (uint32_t i2 = ioff; i2 < ioff+bnds2[0]; ++ i2)
                counts[joff] += cntsa[i2];
        }

        for (uint32_t j2 = 1; j2 < nb2; ++ j2) {
            ++ joff;
            counts[joff] = 0;
            for (uint32_t i1 = bnds1[j1-1]; i1 < bnds1[j1]; ++ i1) {
                uint32_t ioff = i1 * nfine2;
                for (uint32_t i2 = ioff+bnds2[j2-1]; i2 < ioff+bnds2[j2]; ++ i2)
                    counts[joff] += cntsa[i2];
            }
        }
    }

    LOGGER(ibis::gVerbose > 5)
        << "DEBUG -- " << mesg << " completed with bnds1(" << bnds1.size()
        << ") and bnds2(" << bnds2.size() << "), ready for clean up";
    return counts.size();
} // ibis::part::adaptive2DBins

/// The user only specify the name of the variables/columns and the number
/// of bins for each variable.  This function is free to decide where to
/// place the bin boundaries to count the bins as fast as possible.  If the
/// indexes are available and are smaller than the raw data files, then the
/// indexes are used to compute the histogram, otherwise, it reads the raw
/// data files into memory and count the number of records in each bin.
///
/// Bin @c i1 in the first dimension is defined as
/// @code bounds1[i1] <= cname1 < bounds1[i1+1] @endcode
/// and bin @c i2 in the second dimension is defined as
/// @code bounds2[i2] <= cname2 < bounds2[i2+1] @endcode.
/// The 2D bins are linearized in @c counts with the second dimension as the
/// faster varying dimension.
///
/// The return value is the number of bins, i.e., the size of array counts.
/// Normally, the number of bins should be @code nb1 * nb2 @endcode.  For
/// example, if the indexes are used, but there are less bins in the indexes
/// than nb1 or nb2, then the number of bins in the indexes will be used.
///
/// The last three arguments bounds1, bounds2, and counts are for output
/// only.  Their input values are ignored.
///
/// The argument option can be either "index", "data" or "uniform".  The
/// option "index" indicates the user prefer to use indexes to compute
/// histograms.  The indexes will be used in this case if they exist
/// already.  If either "data" or "uniform" is specified, it will attempt
/// to use the base data to compute a histogram, with "uniform" indicating
/// a equally spaced (uniform) bins and the other indicating adaptive bins.
/// If the option is none of above choices, this function will choose one
/// based on their relative sizes.
///
/// @note The number of bins are not guaranteed to be the nb1 and nb2.  The
/// adaptive procedure may decide to use a few bins more or less than
/// specified in each dimension.
///
/// @sa get2DDistributionA.
/// @sa get2DDistributionU.
/// @sa get2DDistributionI.
long ibis::part::get2DDistribution(const char *cname1, const char *cname2,
                                   uint32_t nb1, uint32_t nb2,
                                   std::vector<double> &bounds1,
                                   std::vector<double> &bounds2,
                                   std::vector<uint32_t> &counts,
                                   const char* const option) const {
    if (cname1 == 0 || *cname1 == 0 || cname2 == 0 || *cname2 == 0)
        return -1L;

    const ibis::column* col1 = getColumn(cname1);
    const ibis::column* col2 = getColumn(cname2);
    if (col1 == 0 || col2 == 0)
        return -2L;

    const long idx1 = col1->indexSize();
    const long idx2 = col2->indexSize();
    const int elem1 = col1->elementSize();
    const int elem2 = col2->elementSize();
    if ((elem1 <= 0 && idx1 <= 0) || (elem2 <= 0 && idx2 <= 0))
        // string values must be indexed
        return -3L;

    if (option != 0 && (*option == 'i' || *option == 'I') &&
        idx1 > 0 && idx2 > 0) {
        // use indexes
        return get2DDistributionI(*col1, *col2, nb1, nb2,
                                  bounds1, bounds2, counts);
    }
    else if (option != 0 && (*option == 'd' || *option == 'D') &&
             elem1 > 0 && elem2 > 0) {
        // use base data with adaptive bins
        return get2DDistributionA(*col1, *col2, nb1, nb2,
                                  bounds1, bounds2, counts);
    }
    else if (option != 0 && (*option == 'u' || *option == 'U') &&
             elem1 > 0 && elem2 > 0) {
        // use base data with uniform bins
        return get2DDistributionU(*col1, *col2, nb1, nb2,
                                  bounds1, bounds2, counts);
    }
    else if ((elem1 <= 0 || elem2 <= 0) ||
             (idx1 > 0 && idx2 > 0 && ((double)idx1*nb2+(double)idx2*nb1)*0.1 <
              static_cast<double>(elem1+elem2)*nEvents)) {
        // use indexes because they exist and are much smaller in sizes
        return get2DDistributionI(*col1, *col2, nb1, nb2,
                                  bounds1, bounds2, counts);
    }
    else {
        // use base data with adaptive bins
        return get2DDistributionA(*col1, *col2, nb1, nb2,
                                  bounds1, bounds2, counts);
    }
} // ibis::part::get2DDistribution

/// Compute a set of adaptive bins based on a fine level uniform bins.
long ibis::part::get2DDistributionA(const ibis::column &col1,
                                    const ibis::column &col2,
                                    uint32_t nb1, uint32_t nb2,
                                    std::vector<double> &bounds1,
                                    std::vector<double> &bounds2,
                                    std::vector<uint32_t> &counts) const {
    ibis::horometer timer;
    if (ibis::gVerbose > 0) {
        LOGGER(ibis::gVerbose > 2)
            << "part[" << (m_name ? m_name : "")
            << "]::get2DDistributionA attempting to compute a " << nb1 << " x "
            << nb2 << " histogram of " << col1.name() << " and "
            << col2.name() << " using base data";
        timer.start();
    }

    ibis::bitvector mask;
    col1.getNullMask(mask);
    if (mask.size() == nEvents) {
        ibis::bitvector tmp;
        col2.getNullMask(tmp);
        mask &= tmp;
    }
    else {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- part[" << m_name
            << "]::get2DDistributionA - null mask of " << col1.name()
            << " has " << mask.size() << " bits, but " << nEvents
            << " are expected";
        return -5L;
    }
    if (mask.cnt() == 0) {
        LOGGER(ibis::gVerbose > 1)
            << "part[" << m_name
            << "]::get2DDistributionA - null mask contains only 0 ";
        bounds1.resize(0);
        bounds2.resize(0);
        counts.resize(0);
        return 0L;
    }

    long ierr;
    switch (col1.type()) {
    case ibis::BYTE:
    case ibis::SHORT:
    case ibis::INT: {
        array_t<int32_t>* vals1 = col1.selectInts(mask);
        if (vals1 == 0) {
            ierr = -4;
            break;
        }

        switch (col2.type()) {
        case ibis::BYTE:
        case ibis::SHORT:
        case ibis::INT: {
            array_t<int32_t>* vals2 = col2.selectInts(mask);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }
            ierr = adaptive2DBins(*vals1, *vals2, nb1, nb2,
                                  bounds1, bounds2, counts);
            delete vals2;
            for (uint32_t i = 0; i < bounds2.size(); ++ i)
                bounds2[i] = ceil(bounds2[i]);
            break;}
        case ibis::UBYTE:
        case ibis::USHORT:
        case ibis::CATEGORY:
        case ibis::UINT: {
            array_t<uint32_t>* vals2 = col2.selectUInts(mask);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }
            ierr = adaptive2DBins(*vals1, *vals2, nb1, nb2,
                                  bounds1, bounds2, counts);
            delete vals2;
            for (uint32_t i = 0; i < bounds2.size(); ++ i)
                bounds2[i] = ceil(bounds2[i]);
            break;}
        case ibis::ULONG:
        case ibis::LONG: {
            array_t<int64_t>* vals2 = col2.selectLongs(mask);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }
            ierr = adaptive2DBins(*vals1, *vals2, nb1, nb2,
                                  bounds1, bounds2, counts);
            delete vals2;
            for (uint32_t i = 0; i < bounds2.size(); ++ i)
                bounds2[i] = ceil(bounds2[i]);
            break;}
        case ibis::FLOAT: {
            array_t<float>* vals2 = col2.selectFloats(mask);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }
            ierr = adaptive2DBins(*vals1, *vals2, nb1, nb2,
                                  bounds1, bounds2, counts);
            delete vals2;
            break;}
        case ibis::DOUBLE: {
            array_t<double>* vals2 = col2.selectDoubles(mask);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }
            ierr = adaptive2DBins(*vals1, *vals2, nb1, nb2,
                                  bounds1, bounds2, counts);
            delete vals2;
            break;}
        default: {
            LOGGER(ibis::gVerbose > 3)
                << "part::get2DDistributionA -- can not "
                "handle column (" << col2.name() << ") type "
                << ibis::TYPESTRING[(int)col2.type()];

            ierr = -3;
            break;}
        }
        delete vals1;
        for (uint32_t i = 0; i < bounds1.size(); ++ i)
            bounds1[i] = ceil(bounds1[i]);
        break;}
    case ibis::UBYTE:
    case ibis::USHORT:
    case ibis::CATEGORY:
    case ibis::UINT: {
        array_t<uint32_t>* vals1 = col1.selectUInts(mask);
        if (vals1 == 0) {
            ierr = -4;
            break;
        }

        switch (col2.type()) {
        case ibis::BYTE:
        case ibis::SHORT:
        case ibis::INT: {
            array_t<int32_t>* vals2 = col2.selectInts(mask);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }
            ierr = adaptive2DBins(*vals1, *vals2, nb1, nb2,
                                  bounds1, bounds2, counts);
            delete vals2;
            for (uint32_t i = 0; i < bounds2.size(); ++ i)
                bounds2[i] = ceil(bounds2[i]);
            break;}
        case ibis::UBYTE:
        case ibis::USHORT:
        case ibis::CATEGORY:
        case ibis::UINT: {
            array_t<uint32_t>* vals2 = col2.selectUInts(mask);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }
            ierr = adaptive2DBins(*vals1, *vals2, nb1, nb2,
                                  bounds1, bounds2, counts);
            delete vals2;
            for (uint32_t i = 0; i < bounds2.size(); ++ i)
                bounds2[i] = ceil(bounds2[i]);
            break;}
        case ibis::ULONG:
        case ibis::LONG: {
            array_t<int64_t>* vals2 = col2.selectLongs(mask);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }
            ierr = adaptive2DBins(*vals1, *vals2, nb1, nb2,
                                  bounds1, bounds2, counts);
            for (uint32_t i = 0; i < bounds2.size(); ++ i)
                bounds2[i] = ceil(bounds2[i]);
            delete vals2;
            break;}
        case ibis::FLOAT: {
            array_t<float>* vals2 = col2.selectFloats(mask);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }
            ierr = adaptive2DBins(*vals1, *vals2, nb1, nb2,
                                  bounds1, bounds2, counts);
            delete vals2;
            break;}
        case ibis::DOUBLE: {
            array_t<double>* vals2 = col2.selectDoubles(mask);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }
            ierr = adaptive2DBins(*vals1, *vals2, nb1, nb2,
                                  bounds1, bounds2, counts);
            delete vals2;
            break;}
        default: {
            LOGGER(ibis::gVerbose > 3)
                << "part::get2DDistributionA -- can not "
                "handle column (" << col2.name() << ") type "
                << ibis::TYPESTRING[(int)col2.type()];

            ierr = -3;
            break;}
        }
        delete vals1;
        for (uint32_t i = 0; i < bounds1.size(); ++ i)
            bounds1[i] = ceil(bounds1[i]);
        break;}
    case ibis::ULONG:
    case ibis::LONG: {
        array_t<int64_t>* vals1 = col1.selectLongs(mask);
        if (vals1 == 0) {
            ierr = -4;
            break;
        }

        switch (col2.type()) {
        case ibis::BYTE:
        case ibis::SHORT:
        case ibis::INT: {
            array_t<int32_t>* vals2 = col2.selectInts(mask);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }
            ierr = adaptive2DBins(*vals1, *vals2, nb1, nb2,
                                  bounds1, bounds2, counts);
            delete vals2;
            for (uint32_t i = 0; i < bounds2.size(); ++ i)
                bounds2[i] = ceil(bounds2[i]);
            break;}
        case ibis::UBYTE:
        case ibis::USHORT:
        case ibis::CATEGORY:
        case ibis::UINT: {
            array_t<uint32_t>* vals2 = col2.selectUInts(mask);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }
            ierr = adaptive2DBins(*vals1, *vals2, nb1, nb2,
                                  bounds1, bounds2, counts);
            delete vals2;
            for (uint32_t i = 0; i < bounds2.size(); ++ i)
                bounds2[i] = ceil(bounds2[i]);
            break;}
        case ibis::ULONG:
        case ibis::LONG: {
            array_t<int64_t>* vals2 = col2.selectLongs(mask);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }
            ierr = adaptive2DBins(*vals1, *vals2, nb1, nb2,
                                  bounds1, bounds2, counts);
            delete vals2;
            for (uint32_t i = 0; i < bounds2.size(); ++ i)
                bounds2[i] = ceil(bounds2[i]);
            break;}
        case ibis::FLOAT: {
            array_t<float>* vals2 = col2.selectFloats(mask);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }
            ierr = adaptive2DBins(*vals1, *vals2, nb1, nb2,
                                  bounds1, bounds2, counts);
            delete vals2;
            break;}
        case ibis::DOUBLE: {
            array_t<double>* vals2 = col2.selectDoubles(mask);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }
            ierr = adaptive2DBins(*vals1, *vals2, nb1, nb2,
                                  bounds1, bounds2, counts);
            delete vals2;
            break;}
        default: {
            LOGGER(ibis::gVerbose > 3)
                << "part::get2DDistributionA -- can not "
                "handle column (" << col2.name() << ") type "
                << ibis::TYPESTRING[(int)col2.type()];

            ierr = -3;
            break;}
        }
        delete vals1;
        for (uint32_t i = 0; i < bounds1.size(); ++ i)
            bounds1[i] = ceil(bounds1[i]);
        break;}
    case ibis::FLOAT: {
        array_t<float>* vals1 = col1.selectFloats(mask);
        if (vals1 == 0) {
            ierr = -4;
            break;
        }

        switch (col2.type()) {
        case ibis::BYTE:
        case ibis::SHORT:
        case ibis::INT: {
            array_t<int32_t>* vals2 = col2.selectInts(mask);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }
            ierr = adaptive2DBins(*vals1, *vals2, nb1, nb2,
                                  bounds1, bounds2, counts);
            delete vals2;
            for (uint32_t i = 0; i < bounds2.size(); ++ i)
                bounds2[i] = ceil(bounds2[i]);
            break;}
        case ibis::UBYTE:
        case ibis::USHORT:
        case ibis::CATEGORY:
        case ibis::UINT: {
            array_t<uint32_t>* vals2 = col2.selectUInts(mask);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }
            ierr = adaptive2DBins(*vals1, *vals2, nb1, nb2,
                                  bounds1, bounds2, counts);
            delete vals2;
            for (uint32_t i = 0; i < bounds2.size(); ++ i)
                bounds2[i] = ceil(bounds2[i]);
            break;}
        case ibis::ULONG:
        case ibis::LONG: {
            array_t<int64_t>* vals2 = col2.selectLongs(mask);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }
            ierr = adaptive2DBins(*vals1, *vals2, nb1, nb2,
                                  bounds1, bounds2, counts);
            delete vals2;
            for (uint32_t i = 0; i < bounds2.size(); ++ i)
                bounds2[i] = ceil(bounds2[i]);
            break;}
        case ibis::FLOAT: {
            array_t<float>* vals2 = col2.selectFloats(mask);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }
            ierr = adaptive2DBins(*vals1, *vals2, nb1, nb2,
                                  bounds1, bounds2, counts);
            delete vals2;
            break;}
        case ibis::DOUBLE: {
            array_t<double>* vals2 = col2.selectDoubles(mask);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }
            ierr = adaptive2DBins(*vals1, *vals2, nb1, nb2,
                                  bounds1, bounds2, counts);
            delete vals2;
            break;}
        default: {
            LOGGER(ibis::gVerbose > 3)
                << "part::get2DDistributionA -- can not "
                "handle column (" << col2.name() << ") type "
                << ibis::TYPESTRING[(int)col2.type()];

            ierr = -3;
            break;}
        }
        delete vals1;
        break;}
    case ibis::DOUBLE: {
        array_t<double>* vals1 = col1.selectDoubles(mask);
        if (vals1 == 0) {
            ierr = -4;
            break;
        }

        switch (col2.type()) {
        case ibis::BYTE:
        case ibis::SHORT:
        case ibis::INT: {
            array_t<int32_t>* vals2 = col2.selectInts(mask);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }
            ierr = adaptive2DBins(*vals1, *vals2, nb1, nb2,
                                  bounds1, bounds2, counts);
            delete vals2;
            for (uint32_t i = 0; i < bounds2.size(); ++ i)
                bounds2[i] = ceil(bounds2[i]);
            break;}
        case ibis::UBYTE:
        case ibis::USHORT:
        case ibis::CATEGORY:
        case ibis::UINT: {
            array_t<uint32_t>* vals2 = col2.selectUInts(mask);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }
            ierr = adaptive2DBins(*vals1, *vals2, nb1, nb2,
                                  bounds1, bounds2, counts);
            delete vals2;
            for (uint32_t i = 0; i < bounds2.size(); ++ i)
                bounds2[i] = ceil(bounds2[i]);
            break;}
        case ibis::ULONG:
        case ibis::LONG: {
            array_t<int64_t>* vals2 = col2.selectLongs(mask);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }
            ierr = adaptive2DBins(*vals1, *vals2, nb1, nb2,
                                  bounds1, bounds2, counts);
            delete vals2;
            for (uint32_t i = 0; i < bounds2.size(); ++ i)
                bounds2[i] = ceil(bounds2[i]);
            break;}
        case ibis::FLOAT: {
            array_t<float>* vals2 = col2.selectFloats(mask);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }
            ierr = adaptive2DBins(*vals1, *vals2, nb1, nb2,
                                  bounds1, bounds2, counts);
            delete vals2;
            break;}
        case ibis::DOUBLE: {
            array_t<double>* vals2 = col2.selectDoubles(mask);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }
            ierr = adaptive2DBins(*vals1, *vals2, nb1, nb2,
                                  bounds1, bounds2, counts);
            delete vals2;
            break;}
        default: {
            LOGGER(ibis::gVerbose > 3)
                << "part::get2DDistributionA -- can not "
                "handle column (" << col2.name() << ") type "
                << ibis::TYPESTRING[(int)col2.type()];

            ierr = -3;
            break;}
        }
        delete vals1;
        break;}
    default: {
        LOGGER(ibis::gVerbose > 3)
            << "part::get2DDistributionA -- can not "
            "handle column (" << col1.name() << ") type "
            << ibis::TYPESTRING[(int)col1.type()];

        ierr = -3;
        break;}
    }
    if (ibis::gVerbose > 0) {
        timer.stop();
        ibis::util::logger(0)()
            << "part[" << (m_name ? m_name : "")
            << "]::get2DDistributionA completed filling a " << nb1 << " x "
            << nb2 << " histogram on " << col1.name() << " and "
            << col2.name() << " with " << counts.size() << " cell"
            << (counts.size() > 1 ? "s" : "") << " using " << timer.CPUTime()
            << " sec (CPU), " << timer.realTime() << " sec (elapsed)";
    }
    return ierr;
} // ibis::part::get2DDistributionA

/// Read the base data, then count how many values fall in each bin.  The
/// binns are defined with regular spacing.
long ibis::part::get2DDistributionU(const ibis::column &col1,
                                    const ibis::column &col2,
                                    uint32_t nb1, uint32_t nb2,
                                    std::vector<double> &bounds1,
                                    std::vector<double> &bounds2,
                                    std::vector<uint32_t> &counts) const {
    ibis::horometer timer;
    if (ibis::gVerbose > 0) {
        LOGGER(ibis::gVerbose > 2)
            << "part[" << (m_name ? m_name : "")
            << "]::get2DDistributionU attempting to compute a " << nb1
            << " x " << nb2 << " histogram of " << col1.name() << " and "
            << col2.name() << " using base data";
        timer.start();
    }
    uint32_t nbmax = static_cast<uint32_t>(0.5*sqrt((double)nEvents));
    if (nbmax < 1000) nbmax = 1000;
    if (nb1 <= 1) nb1 = 100;
    else if (nb1 > nbmax) nb1 = nbmax;
    if (nb2 <= 1) nb2 = 100;
    else if (nb2 > nbmax) nb2 = nbmax;
    const double begin1 = col1.getActualMin();
    const double begin2 = col2.getActualMin();
    const double end1 = col1.getActualMax();
    const double end2 = col2.getActualMax();
    if (end1 <= begin1) { // a single bin
        bounds1.resize(2);
        bounds1[0] = begin1;
        bounds1[1] = end1;
        if (end2 <= begin2) {
            bounds2.resize(2);
            bounds2[0] = begin2;
            bounds2[1] = end2;
            counts.resize(1);
            counts[0] = nEvents;
            return 1L;
        }
        else { // col1 has 1 distinct value
            double stride2 = ((col2.isFloat() ? ibis::util::incrDouble(end2) :
                               end2+1) - begin2) / nb2;
            bounds2.resize(nb2+1);
            for (uint32_t i = 0; i <= nb2; ++ i)
                bounds2[i] = begin2 + i * stride2;
            return get1DDistribution(0, col2.name(), begin2, end2, stride2,
                                     counts);
        }
    }
    else if (end2 <= begin2) { // col2 has 1 distinct value
        bounds2.resize(2);
        bounds2[0] = begin2;
        bounds2[1] = end2;
        double stride1 = ((col1.isFloat() ? ibis::util::incrDouble(end1) :
                           end1+1) - begin1) / nb1;
        bounds1.resize(nb1+1);
        for (uint32_t i = 0; i < nb1; ++ i)
            bounds1[i] = begin1 + i * stride1;
        return get1DDistribution(0, col1.name(), begin1, end1, stride1,
                                 counts);
    }

    // normal case -- both columns have more than one distinct value
    double stride1;
    double stride2;
    if (col1.isFloat()) {
        stride1 = (end1 - begin1) / nb1;
        stride1 = ibis::util::compactValue2(stride1, stride1*(1.0+0.75/nb1));
    }
    else if (end1 > begin1 + nb1*1.25) {
        stride1 = (1.0 + end1 - begin1) / nb1;
    }
    else {
        nb1 = static_cast<uint32_t>(1 + end1 - begin1);
        stride1 = 1.0;
    }
    if (col2.isFloat()) {
        stride2 = (end2 - begin2) / nb2;
        stride2 = ibis::util::compactValue2(stride2, stride2*(1.0+0.75/nb2));
    }
    else if (end2 > begin2 + nb2*1.25) {
        stride2 = (1.0 + end2 - begin2) / nb2;
    }
    else {
        nb2 = static_cast<uint32_t>(1.0 + end2 - begin2);
        stride2 = 1.0;
    }
    const uint32_t nbins =
        (1 + static_cast<uint32_t>(floor((end1 - begin1) / stride1))) *
        (1 + static_cast<uint32_t>(floor((end2 - begin2) / stride2)));
    if (nbins != nb1 * nb2) {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- part[" << m_name
            << "]::get2DDistributionU - nbins (" << nbins
            << ") is expected to be the product of nb1 (" << nb1
            << ") and nb2 (" << nb2 << "), but is actually " << nbins;
        return -4L;
    }

    ibis::bitvector mask;
    col1.getNullMask(mask);
    if (mask.size() == nEvents) {
        ibis::bitvector tmp;
        col2.getNullMask(tmp);
        mask &= tmp;
    }
    else {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- part[" << m_name
            << "]::get2DDistributionU - null mask of " << col1.name()
            << " has " << mask.size() << " bits, but " << nEvents
            << " are expected";
        return -5L;
    }
    if (mask.cnt() == 0) {
        LOGGER(ibis::gVerbose > 1)
            << "part[" << m_name
            << "]::get2DDistributionU - null mask contains only 0 ";
        bounds1.resize(0);
        bounds2.resize(0);
        counts.resize(0);
        return 0L;
    }

    long ierr;
    counts.resize(nbins);
    for (uint32_t i = 0; i < nbins; ++i)
        counts[i] = 0;
    bounds1.resize(nb1+1);
    for (uint32_t i = 0; i <= nb1; ++ i)
        bounds1[i] = begin1 + i * stride1;
    bounds2.resize(nb2+1);
    for (uint32_t i = 0; i <= nb2; ++ i)
        bounds2[i] = begin2 + i * stride2;

    switch (col1.type()) {
    case ibis::BYTE:
    case ibis::SHORT:
    case ibis::INT: {
        array_t<int32_t>* vals1 = col1.selectInts(mask);
        if (vals1 == 0) {
            ierr = -4;
            break;
        }

        switch (col2.type()) {
        case ibis::BYTE:
        case ibis::SHORT:
        case ibis::INT: {
            array_t<int32_t>* vals2 = col2.selectInts(mask);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }
            ierr = count2DBins(*vals1, begin1, end1, stride1,
                               *vals2, begin2, end2, stride2, counts);
            delete vals2;
            break;}
        case ibis::UBYTE:
        case ibis::USHORT:
        case ibis::CATEGORY:
        case ibis::UINT: {
            array_t<uint32_t>* vals2 = col2.selectUInts(mask);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }
            ierr = count2DBins(*vals1, begin1, end1, stride1,
                               *vals2, begin2, end2, stride2, counts);
            delete vals2;
            break;}
        case ibis::ULONG:
        case ibis::LONG: {
            array_t<int64_t>* vals2 = col2.selectLongs(mask);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }
            ierr = count2DBins(*vals1, begin1, end1, stride1,
                               *vals2, begin2, end2, stride2, counts);
            delete vals2;
            break;}
        case ibis::FLOAT: {
            array_t<float>* vals2 = col2.selectFloats(mask);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }
            ierr = count2DBins(*vals1, begin1, end1, stride1,
                               *vals2, begin2, end2, stride2, counts);
            delete vals2;
            break;}
        case ibis::DOUBLE: {
            array_t<double>* vals2 = col2.selectDoubles(mask);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }
            ierr = count2DBins(*vals1, begin1, end1, stride1,
                               *vals2, begin2, end2, stride2, counts);
            delete vals2;
            break;}
        default: {
            LOGGER(ibis::gVerbose > 3)
                << "part::get2DDistributionU -- can not "
                "handle column (" << col2.name() << ") type "
                << ibis::TYPESTRING[(int)col2.type()];

            ierr = -3;
            break;}
        }
        delete vals1;
        break;}
    case ibis::UBYTE:
    case ibis::USHORT:
    case ibis::CATEGORY:
    case ibis::UINT: {
        array_t<uint32_t>* vals1 = col1.selectUInts(mask);
        if (vals1 == 0) {
            ierr = -4;
            break;
        }

        switch (col2.type()) {
        case ibis::BYTE:
        case ibis::SHORT:
        case ibis::INT: {
            array_t<int32_t>* vals2 = col2.selectInts(mask);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }
            ierr = count2DBins(*vals1, begin1, end1, stride1,
                               *vals2, begin2, end2, stride2, counts);
            delete vals2;
            break;}
        case ibis::UBYTE:
        case ibis::USHORT:
        case ibis::CATEGORY:
        case ibis::UINT: {
            array_t<uint32_t>* vals2 = col2.selectUInts(mask);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }
            ierr = count2DBins(*vals1, begin1, end1, stride1,
                               *vals2, begin2, end2, stride2, counts);
            delete vals2;
            break;}
        case ibis::ULONG:
        case ibis::LONG: {
            array_t<int64_t>* vals2 = col2.selectLongs(mask);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }
            ierr = count2DBins(*vals1, begin1, end1, stride1,
                               *vals2, begin2, end2, stride2, counts);
            delete vals2;
            break;}
        case ibis::FLOAT: {
            array_t<float>* vals2 = col2.selectFloats(mask);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }
            ierr = count2DBins(*vals1, begin1, end1, stride1,
                               *vals2, begin2, end2, stride2, counts);
            delete vals2;
            break;}
        case ibis::DOUBLE: {
            array_t<double>* vals2 = col2.selectDoubles(mask);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }
            ierr = count2DBins(*vals1, begin1, end1, stride1,
                               *vals2, begin2, end2, stride2, counts);
            delete vals2;
            break;}
        default: {
            LOGGER(ibis::gVerbose > 3)
                << "part::get2DDistributionU -- can not "
                "handle column (" << col2.name() << ") type "
                << ibis::TYPESTRING[(int)col2.type()];

            ierr = -3;
            break;}
        }
        delete vals1;
        break;}
    case ibis::ULONG:
    case ibis::LONG: {
        array_t<int64_t>* vals1 = col1.selectLongs(mask);
        if (vals1 == 0) {
            ierr = -4;
            break;
        }

        switch (col2.type()) {
        case ibis::BYTE:
        case ibis::SHORT:
        case ibis::INT: {
            array_t<int32_t>* vals2 = col2.selectInts(mask);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }
            ierr = count2DBins(*vals1, begin1, end1, stride1,
                               *vals2, begin2, end2, stride2, counts);
            delete vals2;
            break;}
        case ibis::UBYTE:
        case ibis::USHORT:
        case ibis::CATEGORY:
        case ibis::UINT: {
            array_t<uint32_t>* vals2 = col2.selectUInts(mask);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }
            ierr = count2DBins(*vals1, begin1, end1, stride1,
                               *vals2, begin2, end2, stride2, counts);
            delete vals2;
            break;}
        case ibis::ULONG:
        case ibis::LONG: {
            array_t<int64_t>* vals2 = col2.selectLongs(mask);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }
            ierr = count2DBins(*vals1, begin1, end1, stride1,
                               *vals2, begin2, end2, stride2, counts);
            delete vals2;
            break;}
        case ibis::FLOAT: {
            array_t<float>* vals2 = col2.selectFloats(mask);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }
            ierr = count2DBins(*vals1, begin1, end1, stride1,
                               *vals2, begin2, end2, stride2, counts);
            delete vals2;
            break;}
        case ibis::DOUBLE: {
            array_t<double>* vals2 = col2.selectDoubles(mask);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }
            ierr = count2DBins(*vals1, begin1, end1, stride1,
                               *vals2, begin2, end2, stride2, counts);
            delete vals2;
            break;}
        default: {
            LOGGER(ibis::gVerbose > 3)
                << "part::get2DDistributionU -- can not "
                "handle column (" << col2.name() << ") type "
                << ibis::TYPESTRING[(int)col2.type()];

            ierr = -3;
            break;}
        }
        delete vals1;
        break;}
    case ibis::FLOAT: {
        array_t<float>* vals1 = col1.selectFloats(mask);
        if (vals1 == 0) {
            ierr = -4;
            break;
        }

        switch (col2.type()) {
        case ibis::BYTE:
        case ibis::SHORT:
        case ibis::INT: {
            array_t<int32_t>* vals2 = col2.selectInts(mask);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }
            ierr = count2DBins(*vals1, begin1, end1, stride1,
                               *vals2, begin2, end2, stride2, counts);
            delete vals2;
            break;}
        case ibis::UBYTE:
        case ibis::USHORT:
        case ibis::CATEGORY:
        case ibis::UINT: {
            array_t<uint32_t>* vals2 = col2.selectUInts(mask);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }
            ierr = count2DBins(*vals1, begin1, end1, stride1,
                               *vals2, begin2, end2, stride2, counts);
            delete vals2;
            break;}
        case ibis::ULONG:
        case ibis::LONG: {
            array_t<int64_t>* vals2 = col2.selectLongs(mask);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }
            ierr = count2DBins(*vals1, begin1, end1, stride1,
                               *vals2, begin2, end2, stride2, counts);
            delete vals2;
            break;}
        case ibis::FLOAT: {
            array_t<float>* vals2 = col2.selectFloats(mask);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }
            ierr = count2DBins(*vals1, begin1, end1, stride1,
                               *vals2, begin2, end2, stride2, counts);
            delete vals2;
            break;}
        case ibis::DOUBLE: {
            array_t<double>* vals2 = col2.selectDoubles(mask);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }
            ierr = count2DBins(*vals1, begin1, end1, stride1,
                               *vals2, begin2, end2, stride2, counts);
            delete vals2;
            break;}
        default: {
            LOGGER(ibis::gVerbose > 3)
                << "part::get2DDistributionU -- can not "
                "handle column (" << col2.name() << ") type "
                << ibis::TYPESTRING[(int)col2.type()];

            ierr = -3;
            break;}
        }
        delete vals1;
        break;}
    case ibis::DOUBLE: {
        array_t<double>* vals1 = col1.selectDoubles(mask);
        if (vals1 == 0) {
            ierr = -4;
            break;
        }

        switch (col2.type()) {
        case ibis::BYTE:
        case ibis::SHORT:
        case ibis::INT: {
            array_t<int32_t>* vals2 = col2.selectInts(mask);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }
            ierr = count2DBins(*vals1, begin1, end1, stride1,
                               *vals2, begin2, end2, stride2, counts);
            delete vals2;
            break;}
        case ibis::UBYTE:
        case ibis::USHORT:
        case ibis::CATEGORY:
        case ibis::UINT: {
            array_t<uint32_t>* vals2 = col2.selectUInts(mask);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }
            ierr = count2DBins(*vals1, begin1, end1, stride1,
                               *vals2, begin2, end2, stride2, counts);
            delete vals2;
            break;}
        case ibis::ULONG:
        case ibis::LONG: {
            array_t<int64_t>* vals2 = col2.selectLongs(mask);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }
            ierr = count2DBins(*vals1, begin1, end1, stride1,
                               *vals2, begin2, end2, stride2, counts);
            delete vals2;
            break;}
        case ibis::FLOAT: {
            array_t<float>* vals2 = col2.selectFloats(mask);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }
            ierr = count2DBins(*vals1, begin1, end1, stride1,
                               *vals2, begin2, end2, stride2, counts);
            delete vals2;
            break;}
        case ibis::DOUBLE: {
            array_t<double>* vals2 = col2.selectDoubles(mask);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }
            ierr = count2DBins(*vals1, begin1, end1, stride1,
                               *vals2, begin2, end2, stride2, counts);
            delete vals2;
            break;}
        default: {
            LOGGER(ibis::gVerbose > 3)
                << "part::get2DDistributionU -- can not "
                "handle column (" << col2.name() << ") type "
                << ibis::TYPESTRING[(int)col2.type()];

            ierr = -3;
            break;}
        }
        delete vals1;
        break;}
    default: {
        LOGGER(ibis::gVerbose > 3)
            << "part::get2DDistributionU -- can not "
            "handle column (" << col1.name() << ") type "
            << ibis::TYPESTRING[(int)col1.type()];

        ierr = -3;
        break;}
    }
    if (ibis::gVerbose > 0) {
        timer.stop();
        ibis::util::logger(0)()
            << "part[" << (m_name ? m_name : "")
            << "]::get2DDistributionU completed filling a " << nb1 << " x "
            << nb2 << " histogram on " << col1.name() << " and "
            << col2.name() << " with " << counts.size() << " cell"
            << (counts.size() > 1 ? "s" : "") << " using " << timer.CPUTime()
            << " sec (CPU), " << timer.realTime() << " sec (elapsed)";
    }
    return ierr;
} // ibis::part::get2DDistributionU

long ibis::part::get2DDistributionI(const ibis::column &col1,
                                    const ibis::column &col2,
                                    uint32_t nb1, uint32_t nb2,
                                    std::vector<double> &bounds1,
                                    std::vector<double> &bounds2,
                                    std::vector<uint32_t> &counts) const {
    ibis::horometer timer;
    if (ibis::gVerbose > 0) {
        LOGGER(ibis::gVerbose > 2)
            << "part[" << (m_name ? m_name : "")
            << "]::get2DDistributionI attempting to compute a " << nb1
            << " x " << nb2 << " histogram of " << col1.name() << " and "
            << col2.name() << " using indexes";
        timer.start();
    }

    uint32_t nbmax = static_cast<uint32_t>(0.5*sqrt((double)nEvents));
    if (nbmax < 1000) nbmax = 1000;
    if (nb1 <= 1) nb1 = 100;
    else if (nb1 > nbmax) nb1 = nbmax;
    if (nb2 <= 1) nb2 = 100;
    else if (nb2 > nbmax) nb2 = nbmax;
    const double begin1 = col1.getActualMin();
    const double begin2 = col2.getActualMin();
    const double end1 = col1.getActualMax();
    const double end2 = col2.getActualMax();
    if (end1 <= begin1) { // col1 has one distinct value
        bounds1.resize(2);
        bounds1[0] = begin1;
        bounds1[1] = end1;
        if (end2 <= begin2) { // col2 has one distinct value
            bounds2.resize(2);
            bounds2[0] = begin2;
            bounds2[1] = end2;
            counts.resize(1);
            counts[0] = nEvents;
            return 1L;
        }
        else { // col1 has 1 distinct value, but not col2
            return get1DDistribution(col2, nb2, bounds2, counts);
        }
    }
    else if (end2 <= begin2) { // col2 has 1 distinct value
        bounds2.resize(2);
        bounds2[0] = begin2;
        bounds2[1] = end2;
        return get1DDistribution(col1, nb1, bounds2, counts);
    }

    // normal case -- both columns have more than one distinct value
    ibis::column::indexLock idxlock1(&col1, "get2DDistributionI");
    const ibis::index* idx1 = idxlock1.getIndex();
    if (idx1 == 0) {
        LOGGER(ibis::gVerbose > 2)
            << "part[" << (m_name ? m_name : "")
            << "]::get2DDistributionI can not proceed with index for "
            << col1.name();
        return -1L;
    }

    array_t<uint32_t> w1bnds(nb1);
    std::vector<double> idx1bin;
    idx1->binBoundaries(idx1bin);
    while (idx1bin.size() > 1 && idx1bin.back() >= end1)
        idx1bin.pop_back();
    if (idx1bin.empty()) {
        LOGGER(ibis::gVerbose > 2)
            << "part[" << (m_name ? m_name : "")
            << "]::get2DDistributionI can not proceed because column "
            << col1.name() << " contains no valid values or only one value";
        return -2L;
    }
    else if (idx1bin.size() > nb1*3/2) {
        std::vector<uint32_t> idx1wgt;
        idx1->binWeights(idx1wgt);
        if (idx1bin.size() > idx1wgt.size()) {
            LOGGER(ibis::gVerbose > 2)
                << "part[" << (m_name ? m_name : "")
                << "]::get2DDistributionI can not count the number of values "
                "in column " << col1.name();
            return -3L;
        }

        array_t<uint32_t> wgt2(idx1wgt.size());
        std::copy(idx1wgt.begin(), idx1wgt.end(), wgt2.begin());
        
        ibis::index::divideCounts(w1bnds, wgt2);
        while (w1bnds.size() > 1 && w1bnds[w1bnds.size()-2] >= idx1bin.size())
            w1bnds.pop_back();
        if (w1bnds.size() < 2) {
            LOGGER(ibis::gVerbose > 2)
                << "part[" << (m_name ? m_name : "")
                << "]::get2DDistributionI can not divide " << idx1bin.size()
                << "bins into " << nb1 << " coarser bins";
            return -4L;
        }
    }
    else {
        w1bnds.resize(idx1bin.size());
        for (unsigned i = 0; i < idx1bin.size(); ++ i)
            w1bnds[i] = i+1;
    }

    bounds1.resize(w1bnds.size()+1);
    bounds1[0] = begin1;
    for (unsigned i = 1; i < w1bnds.size(); ++ i)
        bounds1[i] = idx1bin[w1bnds[i-1]];
    bounds1.back() = (col1.isFloat() ? ibis::util::incrDouble(end1) : end1+1);

    std::vector<ibis::bitvector*> bins2;
    try {
        long ierr = coarsenBins(col2, nb2, bounds2, bins2);
        if (ierr < 0) {
            LOGGER(ibis::gVerbose > 2)
                << "part[" << (m_name ? m_name : "")
                << "]::get2DDistributionI can not coarsen bins of "
                << col2.name() << ", ierr=" << ierr;
            return -5L;
        }
        else {
            bounds2.resize(bins2.size()+1);
            double prev = begin2;
            for (unsigned i = 0; i < bins2.size(); ++ i) {
                double tmp = bounds2[i];
                bounds2[i] = prev;
                prev = tmp;
            }
            bounds2.back() = (col2.isFloat() ? ibis::util::incrDouble(end2) :
                              end2+1);
        }

        counts.resize((bounds1.size()-1) * bins2.size());
        ibis::qContinuousRange rng1(col1.name(), ibis::qExpr::OP_LT,
                                    bounds1[1]);
        ibis::bitvector bv;
        LOGGER(ibis::gVerbose > 3)
            << "part[" << (m_name ? m_name : "")
            << "]::get2DDistributionI evaluating " << rng1
            << " for bin 0 in " << col1.name();
        ierr = idx1->evaluate(rng1, bv);
        if (ierr < 0) {
            LOGGER(ibis::gVerbose > 2)
                << "part[" << (m_name ? m_name : "")
                << "]::get2DDistributionI failed to evaluate range condition \""
                << rng1 << "\", ierr=" << ierr;
            return -6L;
        }

        if (ierr > 0) {
            for (unsigned i = 0; i < bins2.size(); ++ i) {
                counts[i] = bv.count(*bins2[i]);
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                ibis::bitvector *tmp = bv  &(*bins2[i]);
                if (tmp != 0) {
                    if (tmp->cnt() != counts[i]) {
                        LOGGER(ibis::gVerbose >= 0)
                            << "Warning -- function bitvector::count "
                            "did not produce correct answer";
                        (void) bv.count(*bins2[i]);
                    }
                    delete tmp;
                }
                else {
                    LOGGER(ibis::gVerbose >= 0)
                        << "Warning -- bitwise AND didnot produce a valid "
                        "bitvector";
                }
#endif
            }
        }
        else { // no records
            for (unsigned i = 0; i < bins2.size(); ++ i)
                counts[i] = 0;
        }

        rng1.leftOperator() = ibis::qExpr::OP_LE;
        rng1.rightOperator() = ibis::qExpr::OP_LT;
        for (unsigned j = 1; j < bounds1.size()-2; ++ j) {
            uint32_t jc = j * bins2.size();
            rng1.leftBound() = bounds1[j];
            rng1.rightBound() = bounds1[j+1];
            LOGGER(ibis::gVerbose > 4)
                << "part[" << (m_name ? m_name : "")
                << "]::get2DDistributionI evaluating " << rng1
                << " for bin " << j << " in " << col1.name();
            ierr = idx1->evaluate(rng1, bv);
            if (ierr < 0) {
                LOGGER(ibis::gVerbose > 2)
                    << "part[" << (m_name ? m_name : "")
                    << "]::get2DDistributionI failed to evaluate \""
                    << rng1 << "\", ierr=" << ierr;
                return -6L;
            }
            if (ierr > 0) {
                for (unsigned i = 0; i < bins2.size(); ++ i) {
                    counts[jc + i] = bv.count(*bins2[i]);
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                    ibis::bitvector *tmp = bv  &(*bins2[i]);
                    if (tmp != 0) {
                        if (tmp->cnt() != counts[jc+i]) {
                            LOGGER(ibis::gVerbose >= 0)
                                << "Warning -- function bitvector::count "
                                "did not produce correct answer";
                            (void) bv.count(*bins2[i]);
                        }
                        delete tmp;
                    }
                    else {
                        LOGGER(ibis::gVerbose >= 0)
                            << "Warning -- bitwise AND didnot produce an "
                            "invalid bitvector";
                    }
#endif
                }
            }
            else {
                for (unsigned i = 0; i < bins2.size(); ++ i)
                    counts[jc + i] = 0;
            }
        }

        rng1.rightOperator() = ibis::qExpr::OP_UNDEFINED;
        rng1.leftBound() = bounds1[bounds1.size()-2];
        LOGGER(ibis::gVerbose > 4)
            << "part[" << (m_name ? m_name : "")
            << "]::get2DDistributionI evaluating " << rng1
            << " for bin " << bounds1.size()-1 << " in " << col1.name();
        ierr = idx1->evaluate(rng1, bv);
        if (ierr < 0) {
            LOGGER(ibis::gVerbose > 2)
                << "part[" << (m_name ? m_name : "")
                << "]::get2DDistributionI failed to evaluate range condition \""
                << rng1 << "\", ierr=" << ierr;
            return -6L;
        }
        if (ierr > 0) {
            const uint32_t jc = (bounds1.size()-2) * bins2.size();
            for (unsigned i = 0; i < bins2.size(); ++ i) {
                counts[jc + i] = bv.count(*bins2[i]);
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                ibis::bitvector *tmp = bv  &(*bins2[i]);
                if (tmp != 0) {
                    if (tmp->cnt() != counts[jc+i]) {
                        LOGGER(ibis::gVerbose >= 0)
                            << "Warning -- function bitvector::count "
                            "did not produce correct answer, entering it for "
                            "debugging purpose ...";
                        (void) bv.count(*bins2[i]);
                    }
                    delete tmp;
                }
                else {
                    LOGGER(ibis::gVerbose >= 0)
                        << "Warning -- bitwise AND didnot produce an invalid "
                        "bitvector";
                }
#endif
            }
        }
        else {
            const uint32_t jc = (bounds1.size()-2) * bins2.size();
            for (unsigned i = 0; i < bins2.size(); ++ i)
                counts[jc + i] = 0;
        }

        // clean up bins2
        for (unsigned i = 0; i < bins2.size(); ++ i) {
            delete bins2[i];
            bins2[i] = 0;
        }
    }
    catch (...) {
        // clean up bins2
        for (unsigned i = 0; i < bins2.size(); ++ i) {
            delete bins2[i];
            bins2[i] = 0;
        }

        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- part[" << (m_name ? m_name : "")
            << "]::get2DDistributionI received an exception, stopping";
        return -7L;
    }

    if (ibis::gVerbose > 0) {
        timer.stop();
        ibis::util::logger(0)()
            << "part[" << (m_name ? m_name : "")
            << "]::get2DDistributionI completed filling a " << nb1 << " x "
            << nb2 << " histogram on " << col1.name() << " and "
            << col2.name() << " with " << counts.size() << " cell"
            << (counts.size() > 1 ? "s" : "") << " using " << timer.CPUTime()
            << " sec (CPU), " << timer.realTime() << " sec (elapsed)";
    }
    return counts.size();
} // ibis::part::get2DDistributionI

/// The caller specifies only the number of bins, but let this function
/// decide where to place the bin boundaries.  This function attempts to
/// make sure the 1D bins for each dimension are equal-weight bins, which
/// is likely to produce evenly distributed 2D bins but does not guarantee
/// the uniformity.  It uses the templated function adaptive2DBins, which
/// starts with a set of regularly spaced bins and coalesces the regular
/// bins to produce the desired number of bins.
///
/// @note It return the number of actual bins used on success.  Caller
/// needs to check the sizes of bounds1 and bounds2 for the actual bin
/// bounaries.
long ibis::part::get2DDistribution(const char *constraints,
                                   const char *name1, const char *name2,
                                   uint32_t nb1, uint32_t nb2,
                                   std::vector<double> &bounds1,
                                   std::vector<double> &bounds2,
                                   std::vector<uint32_t> &counts) const {
    if (constraints == 0 || *constraints == 0 || *constraints == '*')
        // unconditional histogram
        return get2DDistribution(name1, name2, nb1, nb2,
                                 bounds1, bounds2, counts);

    long ierr = -1;
    columnList::const_iterator it1 = columns.find(name1);
    columnList::const_iterator it2 = columns.find(name2);
    if (it1 == columns.end() || it2 == columns.end()) {
        if (it1 == columns.end())
            logWarning("get2DDistribution", "%s is not a known column name",
                       name1);
        if (it2 == columns.end())
            logWarning("get2DDistribution", "%s is not a known column name",
                       name2);
        return ierr;
    }

    const ibis::column *col1 = (*it1).second;
    const ibis::column *col2 = (*it2).second;
    ibis::horometer timer;
    if (ibis::gVerbose > 0) {
        LOGGER(ibis::gVerbose > 2)
            << "part[" << (m_name ? m_name : "")
            << "]::get2DDistribution attempting to compute a " << nb1 << " x "
            << nb2 << " histogram on " << name1 << " and " << name2
            << " subject to \"" << (constraints ? constraints : "") << "\"";
        timer.start();
    }

    ibis::bitvector mask;
    col1->getNullMask(mask);
    {
        ibis::bitvector tmp;
        col2->getNullMask(tmp);
        mask &= tmp;
    }
    if (constraints != 0 && *constraints != 0) {
        ibis::countQuery q(this);
        q.setWhereClause(constraints);
        ierr = q.evaluate();
        if (ierr < 0)
            return ierr;
        const ibis::bitvector *hits = q.getHitVector();
        if (hits->cnt() == 0) // nothing to do any more
            return 0;
        mask &= (*hits);
        LOGGER(ibis::gVerbose > 1)
            << "part[" << (m_name ? m_name : "")
            << "]::get2DDistribution -- the constraints \"" << constraints
            << "\" selects " << mask.cnt() << " record"
            << (mask.cnt() > 1 ? "s" : "") << " out of " << nEvents;
    }

    counts.clear();
    switch (col1->type()) {
    case ibis::SHORT:
    case ibis::BYTE:
    case ibis::INT: {
        array_t<int32_t> *val1 = col1->selectInts(mask);
        if (val1 == 0) {
            ierr = -4;
            break;
        }
        array_t<int32_t> bnd1;
        switch (col2->type()) {
        case ibis::SHORT:
        case ibis::BYTE:
        case ibis::INT: {
            array_t<int32_t> *val2 = col2->selectInts(mask);
            if (val2 == 0) {
                ierr = -5;
                break;
            }

            ierr = adaptive2DBins(*val1, *val2, nb1, nb2,
                                  bounds1, bounds2, counts);
            delete val2;
            for (uint32_t i = 0; i < bounds2.size(); ++ i)
                bounds2[i] = ceil(bounds2[i]);
            break;}
        case ibis::USHORT:
        case ibis::UBYTE:
        case ibis::UINT:
        case ibis::CATEGORY: {
            array_t<uint32_t> *val2 = col2->selectUInts(mask);
            if (val2 == 0) {
                ierr = -5;
                break;
            }

            ierr = adaptive2DBins(*val1, *val2, nb1, nb2,
                                  bounds1, bounds2, counts);
            delete val2;
            for (uint32_t i = 0; i < bounds2.size(); ++ i)
                bounds2[i] = ceil(bounds2[i]);
            break;}
        case ibis::LONG: {
            array_t<int64_t> *val2 = col2->selectLongs(mask);
            if (val2 == 0) {
                ierr = -5;
                break;
            }

            ierr = adaptive2DBins(*val1, *val2, nb1, nb2,
                                  bounds1, bounds2, counts);
            delete val2;
            for (uint32_t i = 0; i < bounds2.size(); ++ i)
                bounds2[i] = ceil(bounds2[i]);
            break;}
        case ibis::ULONG: {
            array_t<uint64_t> *val2 = col2->selectULongs(mask);
            if (val2 == 0) {
                ierr = -5;
                break;
            }

            ierr = adaptive2DBins(*val1, *val2, nb1, nb2,
                                  bounds1, bounds2, counts);
            delete val2;
            for (uint32_t i = 0; i < bounds2.size(); ++ i)
                bounds2[i] = ceil(bounds2[i]);
            break;}
        case ibis::FLOAT: {
            array_t<float> *val2 = col2->selectFloats(mask);
            if (val2 == 0) {
                ierr = -5;
                break;
            }

            ierr = adaptive2DBins(*val1, *val2, nb1, nb2,
                                  bounds1, bounds2, counts);
            delete val2;
            break;}
        case ibis::DOUBLE: {
            array_t<double> bnd2;
            array_t<double> *val2 = col2->selectDoubles(mask);
            if (val2 == 0) {
                ierr = -5;
                break;
            }

            ierr = adaptive2DBins(*val1, *val2, nb1, nb2,
                                  bounds1, bounds2, counts);
            delete val2;
            break;}
        default: {
            ierr = -3;
            logWarning("get2DDistribution", "can not handle column type %d",
                       static_cast<int>(col2->type()));
            break;}
        }
        delete val1;
        for (uint32_t i = 0; i < bounds1.size(); ++ i)
            bounds1[i] = ceil(bounds1[i]);
        break;}
    case ibis::USHORT:
    case ibis::UBYTE:
    case ibis::UINT:
    case ibis::CATEGORY: {
        array_t<uint32_t> *val1 = col1->selectUInts(mask);
        if (val1 == 0) {
            ierr = -4;
            break;
        }

        array_t<uint32_t> bnd1;
        switch (col2->type()) {
        case ibis::SHORT:
        case ibis::BYTE:
        case ibis::INT: {
            array_t<int32_t> *val2 = col2->selectInts(mask);
            if (val2 == 0) {
                ierr = -5;
                break;
            }

            ierr = adaptive2DBins(*val1, *val2, nb1, nb2,
                                  bounds1, bounds2, counts);
            delete val2;
            for (uint32_t i = 0; i < bounds2.size(); ++ i)
                bounds2[i] = ceil(bounds2[i]);
            break;}
        case ibis::USHORT:
        case ibis::UBYTE:
        case ibis::UINT:
        case ibis::CATEGORY: {
            array_t<uint32_t> *val2 = col2->selectUInts(mask);
            if (val2 == 0) {
                ierr = -5;
                break;
            }

            ierr = adaptive2DBins(*val1, *val2, nb1, nb2,
                                  bounds1, bounds2, counts);
            delete val2;
            for (uint32_t i = 0; i < bounds2.size(); ++ i)
                bounds2[i] = ceil(bounds2[i]);
            break;}
        case ibis::LONG: {
            array_t<int64_t> *val2 = col2->selectLongs(mask);
            if (val2 == 0) {
                ierr = -5;
                break;
            }

            ierr = adaptive2DBins(*val1, *val2, nb1, nb2,
                                  bounds1, bounds2, counts);
            delete val2;
            for (uint32_t i = 0; i < bounds2.size(); ++ i)
                bounds2[i] = ceil(bounds2[i]);
            break;}
        case ibis::ULONG: {
            array_t<uint64_t> *val2 = col2->selectULongs(mask);
            if (val2 == 0) {
                ierr = -5;
                break;
            }

            ierr = adaptive2DBins(*val1, *val2, nb1, nb2,
                                  bounds1, bounds2, counts);
            delete val2;
            for (uint32_t i = 0; i < bounds2.size(); ++ i)
                bounds2[i] = ceil(bounds2[i]);
            break;}
        case ibis::FLOAT: {
            array_t<float> *val2 = col2->selectFloats(mask);
            if (val2 == 0) {
                ierr = -5;
                break;
            }

            ierr = adaptive2DBins(*val1, *val2, nb1, nb2,
                                  bounds1, bounds2, counts);
            delete val2;
            break;}
        case ibis::DOUBLE: {
            array_t<double> *val2 = col2->selectDoubles(mask);
            if (val2 == 0) {
                ierr = -5;
                break;
            }

            ierr = adaptive2DBins(*val1, *val2, nb1, nb2,
                                  bounds1, bounds2, counts);
            delete val2;
            break;}
        default: {
            ierr = -3;
            logWarning("get2DDistribution", "can not handle column type %d",
                       static_cast<int>(col2->type()));
            break;}
        }
        delete val1;
        for (uint32_t i = 0; i < bounds1.size(); ++ i)
            bounds1[i] = ceil(bounds1[i]);
        break;}
    case ibis::LONG: {
        array_t<int64_t> *val1 = col1->selectLongs(mask);
        if (val1 == 0) {
            ierr = -4;
            break;
        }
        array_t<int32_t> bnd1;
        switch (col2->type()) {
        case ibis::SHORT:
        case ibis::BYTE:
        case ibis::INT: {
            array_t<int32_t> *val2 = col2->selectInts(mask);
            if (val2 == 0) {
                ierr = -5;
                break;
            }

            ierr = adaptive2DBins(*val1, *val2, nb1, nb2,
                                  bounds1, bounds2, counts);
            delete val2;
            for (uint32_t i = 0; i < bounds2.size(); ++ i)
                bounds2[i] = ceil(bounds2[i]);
            break;}
        case ibis::USHORT:
        case ibis::UBYTE:
        case ibis::UINT:
        case ibis::CATEGORY: {
            array_t<uint32_t> *val2 = col2->selectUInts(mask);
            if (val2 == 0) {
                ierr = -5;
                break;
            }

            ierr = adaptive2DBins(*val1, *val2, nb1, nb2,
                                  bounds1, bounds2, counts);
            delete val2;
            for (uint32_t i = 0; i < bounds2.size(); ++ i)
                bounds2[i] = ceil(bounds2[i]);
            break;}
        case ibis::LONG: {
            array_t<int64_t> *val2 = col2->selectLongs(mask);
            if (val2 == 0) {
                ierr = -5;
                break;
            }

            ierr = adaptive2DBins(*val1, *val2, nb1, nb2,
                                  bounds1, bounds2, counts);
            delete val2;
            for (uint32_t i = 0; i < bounds2.size(); ++ i)
                bounds2[i] = ceil(bounds2[i]);
            break;}
        case ibis::ULONG: {
            array_t<uint64_t> *val2 = col2->selectULongs(mask);
            if (val2 == 0) {
                ierr = -5;
                break;
            }

            ierr = adaptive2DBins(*val1, *val2, nb1, nb2,
                                  bounds1, bounds2, counts);
            delete val2;
            for (uint32_t i = 0; i < bounds2.size(); ++ i)
                bounds2[i] = ceil(bounds2[i]);
            break;}
        case ibis::FLOAT: {
            array_t<float> *val2 = col2->selectFloats(mask);
            if (val2 == 0) {
                ierr = -5;
                break;
            }

            ierr = adaptive2DBins(*val1, *val2, nb1, nb2,
                                  bounds1, bounds2, counts);
            delete val2;
            break;}
        case ibis::DOUBLE: {
            array_t<double> bnd2;
            array_t<double> *val2 = col2->selectDoubles(mask);
            if (val2 == 0) {
                ierr = -5;
                break;
            }

            ierr = adaptive2DBins(*val1, *val2, nb1, nb2,
                                  bounds1, bounds2, counts);
            delete val2;
            break;}
        default: {
            ierr = -3;
            logWarning("get2DDistribution", "can not handle column type %d",
                       static_cast<int>(col2->type()));
            break;}
        }
        delete val1;
        for (uint32_t i = 0; i < bounds1.size(); ++ i)
            bounds1[i] = ceil(bounds1[i]);
        break;}
    case ibis::ULONG: {
        array_t<uint64_t> *val1 = col1->selectULongs(mask);
        if (val1 == 0) {
            ierr = -4;
            break;
        }

        array_t<uint32_t> bnd1;
        switch (col2->type()) {
        case ibis::SHORT:
        case ibis::BYTE:
        case ibis::INT: {
            array_t<int32_t> *val2 = col2->selectInts(mask);
            if (val2 == 0) {
                ierr = -5;
                break;
            }

            ierr = adaptive2DBins(*val1, *val2, nb1, nb2,
                                  bounds1, bounds2, counts);
            delete val2;
            for (uint32_t i = 0; i < bounds2.size(); ++ i)
                bounds2[i] = ceil(bounds2[i]);
            break;}
        case ibis::USHORT:
        case ibis::UBYTE:
        case ibis::UINT:
        case ibis::CATEGORY: {
            array_t<uint32_t> *val2 = col2->selectUInts(mask);
            if (val2 == 0) {
                ierr = -5;
                break;
            }

            ierr = adaptive2DBins(*val1, *val2, nb1, nb2,
                                  bounds1, bounds2, counts);
            delete val2;
            for (uint32_t i = 0; i < bounds2.size(); ++ i)
                bounds2[i] = ceil(bounds2[i]);
            break;}
        case ibis::LONG: {
            array_t<int64_t> *val2 = col2->selectLongs(mask);
            if (val2 == 0) {
                ierr = -5;
                break;
            }

            ierr = adaptive2DBins(*val1, *val2, nb1, nb2,
                                  bounds1, bounds2, counts);
            delete val2;
            for (uint32_t i = 0; i < bounds2.size(); ++ i)
                bounds2[i] = ceil(bounds2[i]);
            break;}
        case ibis::ULONG: {
            array_t<uint64_t> *val2 = col2->selectULongs(mask);
            if (val2 == 0) {
                ierr = -5;
                break;
            }

            ierr = adaptive2DBins(*val1, *val2, nb1, nb2,
                                  bounds1, bounds2, counts);
            delete val2;
            for (uint32_t i = 0; i < bounds2.size(); ++ i)
                bounds2[i] = ceil(bounds2[i]);
            break;}
        case ibis::FLOAT: {
            array_t<float> *val2 = col2->selectFloats(mask);
            if (val2 == 0) {
                ierr = -5;
                break;
            }

            ierr = adaptive2DBins(*val1, *val2, nb1, nb2,
                                  bounds1, bounds2, counts);
            delete val2;
            break;}
        case ibis::DOUBLE: {
            array_t<double> *val2 = col2->selectDoubles(mask);
            if (val2 == 0) {
                ierr = -5;
                break;
            }

            ierr = adaptive2DBins(*val1, *val2, nb1, nb2,
                                  bounds1, bounds2, counts);
            delete val2;
            break;}
        default: {
            ierr = -3;
            logWarning("get2DDistribution", "can not handle column type %d",
                       static_cast<int>(col2->type()));
            break;}
        }
        delete val1;
        for (uint32_t i = 0; i < bounds1.size(); ++ i)
            bounds1[i] = ceil(bounds1[i]);
        break;}
    case ibis::FLOAT: {
        array_t<float> *val1 = col1->selectFloats(mask);
        if (val1 == 0) {
            ierr = -4;
            break;
        }

        array_t<float> bnd1;
        switch (col2->type()) {
        case ibis::SHORT:
        case ibis::BYTE:
        case ibis::INT: {
            array_t<int32_t> *val2 = col2->selectInts(mask);
            if (val2 == 0) {
                ierr = -5;
                break;
            }

            ierr = adaptive2DBins(*val1, *val2, nb1, nb2,
                                  bounds1, bounds2, counts);
            delete val2;
            for (uint32_t i = 0; i < bounds2.size(); ++ i)
                bounds2[i] = ceil(bounds2[i]);
            break;}
        case ibis::USHORT:
        case ibis::UBYTE:
        case ibis::UINT:
        case ibis::CATEGORY: {
            array_t<uint32_t> *val2 = col2->selectUInts(mask);
            if (val2 == 0) {
                ierr = -5;
                break;
            }

            ierr = adaptive2DBins(*val1, *val2, nb1, nb2,
                                  bounds1, bounds2, counts);
            for (uint32_t i = 0; i < bounds2.size(); ++ i)
                bounds2[i] = ceil(bounds2[i]);
            delete val2;
            break;}
        case ibis::LONG: {
            array_t<int64_t> *val2 = col2->selectLongs(mask);
            if (val2 == 0) {
                ierr = -5;
                break;
            }

            ierr = adaptive2DBins(*val1, *val2, nb1, nb2,
                                  bounds1, bounds2, counts);
            delete val2;
            for (uint32_t i = 0; i < bounds2.size(); ++ i)
                bounds2[i] = ceil(bounds2[i]);
            break;}
        case ibis::ULONG: {
            array_t<uint64_t> *val2 = col2->selectULongs(mask);
            if (val2 == 0) {
                ierr = -5;
                break;
            }

            ierr = adaptive2DBins(*val1, *val2, nb1, nb2,
                                  bounds1, bounds2, counts);
            delete val2;
            for (uint32_t i = 0; i < bounds2.size(); ++ i)
                bounds2[i] = ceil(bounds2[i]);
            break;}
        case ibis::FLOAT: {
            array_t<float> *val2 = col2->selectFloats(mask);
            if (val2 == 0) {
                ierr = -5;
                break;
            }

            ierr = adaptive2DBins(*val1, *val2, nb1, nb2,
                                  bounds1, bounds2, counts);
            delete val2;
            break;}
        case ibis::DOUBLE: {
            array_t<double> *val2 = col2->selectDoubles(mask);
            if (val2 == 0) {
                ierr = -5;
                break;
            }

            ierr = adaptive2DBins(*val1, *val2, nb1, nb2,
                                  bounds1, bounds2, counts);
            delete val2;
            break;}
        default: {
            ierr = -3;
            logWarning("get2DDistribution", "can not handle column type %d",
                       static_cast<int>(col2->type()));
            break;}
        }
        delete val1;
        break;}
    case ibis::DOUBLE: {
        array_t<double> *val1 = col1->selectDoubles(mask);
        if (val1 == 0) {
            ierr = -4;
            break;
        }

        array_t<double> bnd1;
        switch (col2->type()) {
        case ibis::SHORT:
        case ibis::BYTE:
        case ibis::INT: {
            array_t<int32_t> *val2 = col2->selectInts(mask);
            if (val2 == 0) {
                ierr = -5;
                break;
            }

            ierr = adaptive2DBins(*val1, *val2, nb1, nb2,
                                  bounds1, bounds2, counts);
            delete val2;
            for (uint32_t i = 0; i < bounds2.size(); ++ i)
                bounds2[i] = ceil(bounds2[i]);
            break;}
        case ibis::USHORT:
        case ibis::UBYTE:
        case ibis::UINT:
        case ibis::CATEGORY: {
            array_t<uint32_t> *val2 = col2->selectUInts(mask);
            if (val2 == 0) {
                ierr = -5;
                break;
            }

            ierr = adaptive2DBins(*val1, *val2, nb1, nb2,
                                  bounds1, bounds2, counts);
            delete val2;
            for (uint32_t i = 0; i < bounds2.size(); ++ i)
                bounds2[i] = ceil(bounds2[i]);
            break;}
        case ibis::LONG: {
            array_t<int64_t> *val2 = col2->selectLongs(mask);
            if (val2 == 0) {
                ierr = -5;
                break;
            }

            ierr = adaptive2DBins(*val1, *val2, nb1, nb2,
                                  bounds1, bounds2, counts);
            delete val2;
            for (uint32_t i = 0; i < bounds2.size(); ++ i)
                bounds2[i] = ceil(bounds2[i]);
            break;}
        case ibis::ULONG: {
            array_t<uint64_t> *val2 = col2->selectULongs(mask);
            if (val2 == 0) {
                ierr = -5;
                break;
            }

            ierr = adaptive2DBins(*val1, *val2, nb1, nb2,
                                  bounds1, bounds2, counts);
            delete val2;
            for (uint32_t i = 0; i < bounds2.size(); ++ i)
                bounds2[i] = ceil(bounds2[i]);
            break;}
        case ibis::FLOAT: {
            array_t<float> *val2 = col2->selectFloats(mask);
            if (val2 == 0) {
                ierr = -5;
                break;
            }

            ierr = adaptive2DBins(*val1, *val2, nb1, nb2,
                                  bounds1, bounds2, counts);
            delete val2;
            break;}
        case ibis::DOUBLE: {
            array_t<double> *val2 = col2->selectDoubles(mask);
            if (val2 == 0) {
                ierr = -5;
                break;
            }

            ierr = adaptive2DBins(*val1, *val2, nb1, nb2,
                                  bounds1, bounds2, counts);
            delete val2;
            break;}
        default: {
            ierr = -3;
            logWarning("get2DDistribution", "can not handle column type %d",
                       static_cast<int>(col2->type()));
            break;}
        }
        delete val1;
        break;}
    default: {
        ierr = -3;
        logWarning("get2DDistribution", "can not handle column type %d",
                   static_cast<int>(col1->type()));
        break;}
    }

    if ((bounds1.size()-1) * (bounds2.size()-1) == counts.size())
        ierr = counts.size();
    else
        ierr = -2;
    if (ierr > 0 && ibis::gVerbose > 0) {
        timer.stop();
        logMessage("get2DDistribution",
                   "computing the joint distribution of column %s and "
                   "%s%s%s took %g sec(CPU), %g sec(elapsed)",
                   (*it1).first, (*it2).first,
                   (constraints ? " with restriction " : ""),
                   (constraints ? constraints : ""),
                   timer.CPUTime(), timer.realTime());
    }
    return ierr;
} // ibis::part::get2DDistribution

/// The old implementation that uses binary lookup.  For floating-point
/// values, this function will go through the intermediate arrays three
/// times, once to compute the actual minimum and maximum values, once to
/// count the 1D distributions, and finally to count the number of values
/// in the 2D bins.  The last step is more expensive then the first two
/// because it involves two binary searches, one on each each set of the
/// boundaries.
long ibis::part::old2DDistribution(const char *constraints,
                                   const char *name1, const char *name2,
                                   uint32_t nb1, uint32_t nb2,
                                   std::vector<double> &bounds1,
                                   std::vector<double> &bounds2,
                                   std::vector<uint32_t> &counts) const {
    if (constraints == 0 || *constraints == 0 || *constraints == '*')
        return get2DDistribution(name1, name2, nb1, nb2,
                                 bounds1, bounds2, counts);

    long ierr = -1;
    columnList::const_iterator it1 = columns.find(name1);
    columnList::const_iterator it2 = columns.find(name2);
    if (it1 == columns.end() || it2 == columns.end()) {
        if (it1 == columns.end())
            logWarning("old2DDistribution", "%s is not a known column name",
                       name1);
        if (it2 == columns.end())
            logWarning("old2DDistribution", "%s is not a known column name",
                       name2);
        return ierr;
    }

    const ibis::column *col1 = (*it1).second;
    const ibis::column *col2 = (*it2).second;
    ibis::horometer timer;
    if (ibis::gVerbose > 0) {
        LOGGER(ibis::gVerbose > 2)
            << "part[" << (m_name ? m_name : "")
            << "]::old2DDistribution attempting to compute a " << nb1
            << " x " << nb2 << " histogram on "
            << name1 << " and " << name2 << " subject to \""
            << (constraints ? constraints : "") << "\"";
        timer.start();
    }

    ibis::bitvector mask;
    col1->getNullMask(mask);
    {
        ibis::bitvector tmp;
        col2->getNullMask(tmp);
        mask &= tmp;
    }
    if (constraints != 0 && *constraints != 0) {
        ibis::countQuery q(this);
        q.setWhereClause(constraints);
        ierr = q.evaluate();
        if (ierr < 0)
            return ierr;
        const ibis::bitvector *hits = q.getHitVector();
        if (hits->cnt() == 0) // nothing to do any more
            return 0;
        mask &= (*hits);
        LOGGER(ibis::gVerbose > 1)
            << "part[" << (m_name ? m_name : "")
            << "]::old2DDistribution -- the constraints \"" << constraints
            << "\" selects " << mask.cnt() << " record"
            << (mask.cnt() > 1 ? "s" : "") << " out of " << nEvents;
    }

    counts.clear();
    switch (col1->type()) {
    case ibis::SHORT:
    case ibis::BYTE:
    case ibis::INT: {
        array_t<int32_t> *val1 = col1->selectInts(mask);
        if (val1 == 0) {
            ierr = -4;
            break;
        }
        array_t<int32_t> bnd1;
        switch (col2->type()) {
        case ibis::SHORT:
        case ibis::BYTE:
        case ibis::INT: {
            array_t<int32_t> *val2 = col2->selectInts(mask);
            if (val2 == 0) {
                ierr = -5;
                break;
            }

            array_t<int32_t> bnd2;
            ibis::part::mapValues(*val1, *val2, nb1, nb2, bnd1, bnd2, counts);
            delete val2;
            bounds1.resize(bnd1.size());
            bounds2.resize(bnd2.size());
            for (uint32_t i = 0; i < bnd1.size(); ++ i)
                bounds1[i] = bnd1[i];
            for (uint32_t i = 0; i < bnd2.size(); ++ i)
                bounds2[i] = bnd2[i];
            break;}
        case ibis::USHORT:
        case ibis::UBYTE:
        case ibis::UINT:
        case ibis::CATEGORY: {
            array_t<uint32_t> *val2 = col2->selectUInts(mask);
            if (val2 == 0) {
                ierr = -5;
                break;
            }

            array_t<uint32_t> bnd2;
            ibis::part::mapValues(*val1, *val2, nb1, nb2, bnd1, bnd2, counts);
            delete val2;
            bounds1.resize(bnd1.size());
            bounds2.resize(bnd2.size());
            for (uint32_t i = 0; i < bnd1.size(); ++ i)
                bounds1[i] = bnd1[i];
            for (uint32_t i = 0; i < bnd2.size(); ++ i)
                bounds2[i] = bnd2[i];
            break;}
        case ibis::FLOAT: {
            array_t<float> *val2 = col2->selectFloats(mask);
            if (val2 == 0) {
                ierr = -5;
                break;
            }

            array_t<float> bnd2;
            ibis::part::mapValues(*val1, *val2, nb1, nb2, bnd1, bnd2, counts);
            delete val2;
            bounds1.resize(bnd1.size());
            bounds2.resize(bnd2.size());
            for (uint32_t i = 0; i < bnd1.size(); ++ i)
                bounds1[i] = bnd1[i];
            for (uint32_t i = 0; i < bnd2.size(); ++ i)
                bounds2[i] = bnd2[i];
            break;}
        case ibis::DOUBLE: {
            array_t<double> bnd2;
            array_t<double> *val2 = col2->selectDoubles(mask);
            if (val2 == 0) {
                ierr = -5;
                break;
            }

            ibis::part::mapValues(*val1, *val2, nb1, nb2, bnd1, bnd2, counts);
            delete val2;
            bounds1.resize(bnd1.size());
            for (uint32_t i = 0; i < bnd1.size(); ++ i)
                bounds1[i] = bnd1[i];
            bounds2.resize(bnd2.size());
            for (uint32_t i = 0; i < bnd2.size(); ++ i)
                bounds2[i] = bnd2[i];
            break;}
        default: {
            ierr = -3;
            logWarning("old2DDistribution",
                       "can not handle column type %d",
                       static_cast<int>(col2->type()));
            break;}
        }
        delete val1;
        break;}
    case ibis::USHORT:
    case ibis::UBYTE:
    case ibis::UINT:
    case ibis::CATEGORY: {
        array_t<uint32_t> *val1 = col1->selectUInts(mask);
        if (val1 == 0) {
            ierr = -4;
            break;
        }

        array_t<uint32_t> bnd1;
        switch (col2->type()) {
        case ibis::SHORT:
        case ibis::BYTE:
        case ibis::INT: {
            array_t<int32_t> *val2 = col2->selectInts(mask);
            if (val2 == 0) {
                ierr = -5;
                break;
            }

            array_t<int32_t> bnd2;
            ibis::part::mapValues(*val1, *val2, nb1, nb2, bnd1, bnd2, counts);
            delete val2;
            bounds1.resize(bnd1.size());
            bounds2.resize(bnd2.size());
            for (uint32_t i = 0; i < bnd1.size(); ++ i)
                bounds1[i] = bnd1[i];
            for (uint32_t i = 0; i < bnd2.size(); ++ i)
                bounds2[i] = bnd2[i];
            break;}
        case ibis::USHORT:
        case ibis::UBYTE:
        case ibis::UINT:
        case ibis::CATEGORY: {
            array_t<uint32_t> *val2 = col2->selectUInts(mask);
            if (val2 == 0) {
                ierr = -5;
                break;
            }

            array_t<uint32_t> bnd2;
            ibis::part::mapValues(*val1, *val2, nb1, nb2, bnd1, bnd2, counts);
            delete val2;
            bounds1.resize(bnd1.size());
            bounds2.resize(bnd2.size());
            for (uint32_t i = 0; i < bnd1.size(); ++ i)
                bounds1[i] = bnd1[i];
            for (uint32_t i = 0; i < bnd2.size(); ++ i)
                bounds2[i] = bnd2[i];
            break;}
        case ibis::FLOAT: {
            array_t<float> *val2 = col2->selectFloats(mask);
            if (val2 == 0) {
                ierr = -5;
                break;
            }

            array_t<float> bnd2;
            ibis::part::mapValues(*val1, *val2, nb1, nb2, bnd1, bnd2, counts);
            delete val2;
            bounds1.resize(bnd1.size());
            bounds2.resize(bnd2.size());
            for (uint32_t i = 0; i < bnd1.size(); ++ i)
                bounds1[i] = bnd1[i];
            for (uint32_t i = 0; i < bnd2.size(); ++ i)
                bounds2[i] = bnd2[i];
            break;}
        case ibis::DOUBLE: {
            array_t<double> *val2 = col2->selectDoubles(mask);
            if (val2 == 0) {
                ierr = -5;
                break;
            }

            array_t<double> bnd2;
            ibis::part::mapValues(*val1, *val2, nb1, nb2, bnd1, bnd2, counts);
            delete val2;
            bounds1.resize(bnd1.size());
            for (uint32_t i = 0; i < bnd1.size(); ++ i)
                bounds1[i] = bnd1[i];
            bounds2.resize(bnd2.size());
            for (uint32_t i = 0; i < bnd2.size(); ++ i)
                bounds2[i] = bnd2[i];
            break;}
        default: {
            ierr = -3;
            logWarning("old2DDistribution",
                       "can not handle column type %d",
                       static_cast<int>(col2->type()));
            break;}
        }
        delete val1;
        break;}
    case ibis::FLOAT: {
        array_t<float> *val1 = col1->selectFloats(mask);
        if (val1 == 0) {
            ierr = -4;
            break;
        }

        array_t<float> bnd1;
        switch (col2->type()) {
        case ibis::SHORT:
        case ibis::BYTE:
        case ibis::INT: {
            array_t<int32_t> *val2 = col2->selectInts(mask);
            if (val2 == 0) {
                ierr = -5;
                break;
            }

            array_t<int32_t> bnd2;
            ibis::part::mapValues(*val1, *val2, nb1, nb2, bnd1, bnd2, counts);
            delete val2;
            bounds1.resize(bnd1.size());
            bounds2.resize(bnd2.size());
            for (uint32_t i = 0; i < bnd1.size(); ++ i)
                bounds1[i] = bnd1[i];
            for (uint32_t i = 0; i < bnd2.size(); ++ i)
                bounds2[i] = bnd2[i];
            break;}
        case ibis::USHORT:
        case ibis::UBYTE:
        case ibis::UINT:
        case ibis::CATEGORY: {
            array_t<uint32_t> *val2 = col2->selectUInts(mask);
            if (val2 == 0) {
                ierr = -5;
                break;
            }

            array_t<uint32_t> bnd2;
            ibis::part::mapValues(*val1, *val2, nb1, nb2, bnd1, bnd2, counts);
            delete val2;
            bounds1.resize(bnd1.size());
            bounds2.resize(bnd2.size());
            for (uint32_t i = 0; i < bnd1.size(); ++ i)
                bounds1[i] = bnd1[i];
            for (uint32_t i = 0; i < bnd2.size(); ++ i)
                bounds2[i] = bnd2[i];
            break;}
        case ibis::FLOAT: {
            array_t<float> *val2 = col2->selectFloats(mask);
            if (val2 == 0) {
                ierr = -5;
                break;
            }

            array_t<float> bnd2;
            ibis::part::mapValues(*val1, *val2, nb1, nb2, bnd1, bnd2, counts);
            delete val2;
            bounds1.resize(bnd1.size());
            bounds2.resize(bnd2.size());
            for (uint32_t i = 0; i < bnd1.size(); ++ i)
                bounds1[i] = bnd1[i];
            for (uint32_t i = 0; i < bnd2.size(); ++ i)
                bounds2[i] = bnd2[i];
            break;}
        case ibis::DOUBLE: {
            array_t<double> *val2 = col2->selectDoubles(mask);
            if (val2 == 0) {
                ierr = -5;
                break;
            }

            array_t<double> bnd2;
            ibis::part::mapValues(*val1, *val2, nb1, nb2, bnd1, bnd2, counts);
            delete val2;
            bounds1.resize(bnd1.size());
            for (uint32_t i = 0; i < bnd1.size(); ++ i)
                bounds1[i] = bnd1[i];
            bounds2.resize(bnd2.size());
            for (uint32_t i = 0; i < bnd2.size(); ++ i)
                bounds2[i] = bnd2[i];
            break;}
        default: {
            ierr = -3;
            logWarning("old2DDistribution",
                       "can not handle column type %d",
                       static_cast<int>(col2->type()));
            break;}
        }
        delete val1;
        break;}
    case ibis::DOUBLE: {
        array_t<double> *val1 = col1->selectDoubles(mask);
        if (val1 == 0) {
            ierr = -4;
            break;
        }

        array_t<double> bnd1;
        switch (col2->type()) {
        case ibis::SHORT:
        case ibis::BYTE:
        case ibis::INT: {
            array_t<int32_t> *val2 = col2->selectInts(mask);
            if (val2 == 0) {
                ierr = -5;
                break;
            }

            array_t<int32_t> bnd2;
            ibis::part::mapValues(*val1, *val2, nb1, nb2, bnd1, bnd2, counts);
            delete val2;
            bounds1.resize(bnd1.size());
            bounds2.resize(bnd2.size());
            for (uint32_t i = 0; i < bnd1.size(); ++ i)
                bounds1[i] = bnd1[i];
            for (uint32_t i = 0; i < bnd2.size(); ++ i)
                bounds2[i] = bnd2[i];
            break;}
        case ibis::USHORT:
        case ibis::UBYTE:
        case ibis::UINT:
        case ibis::CATEGORY: {
            array_t<uint32_t> *val2 = col2->selectUInts(mask);
            if (val2 == 0) {
                ierr = -5;
                break;
            }

            array_t<uint32_t> bnd2;
            ibis::part::mapValues(*val1, *val2, nb1, nb2, bnd1, bnd2, counts);
            delete val2;
            bounds1.resize(bnd1.size());
            bounds2.resize(bnd2.size());
            for (uint32_t i = 0; i < bnd1.size(); ++ i)
                bounds1[i] = bnd1[i];
            for (uint32_t i = 0; i < bnd2.size(); ++ i)
                bounds2[i] = bnd2[i];
            break;}
        case ibis::FLOAT: {
            array_t<float> *val2 = col2->selectFloats(mask);
            if (val2 == 0) {
                ierr = -5;
                break;
            }

            array_t<float> bnd2;
            ibis::part::mapValues(*val1, *val2, nb1, nb2, bnd1, bnd2, counts);
            delete val2;
            bounds1.resize(bnd1.size());
            bounds2.resize(bnd2.size());
            for (uint32_t i = 0; i < bnd1.size(); ++ i)
                bounds1[i] = bnd1[i];
            for (uint32_t i = 0; i < bnd2.size(); ++ i)
                bounds2[i] = bnd2[i];
            break;}
        case ibis::DOUBLE: {
            array_t<double> *val2 = col2->selectDoubles(mask);
            if (val2 == 0) {
                ierr = -5;
                break;
            }

            array_t<double> bnd2;
            ibis::part::mapValues(*val1, *val2, nb1, nb2, bnd1, bnd2, counts);
            delete val2;
            bounds1.resize(bnd1.size());
            for (uint32_t i = 0; i < bnd1.size(); ++ i)
                bounds1[i] = bnd1[i];
            bounds2.resize(bnd2.size());
            for (uint32_t i = 0; i < bnd2.size(); ++ i)
                bounds2[i] = bnd2[i];
            break;}
        default: {
            ierr = -3;
            logWarning("old2DDistribution",
                       "can not handle column type %d",
                       static_cast<int>(col2->type()));
            break;}
        }
        delete val1;
        break;}
    default: {
        ierr = -3;
        logWarning("old2DDistribution",
                   "can not handle column type %d",
                   static_cast<int>(col1->type()));
        break;}
    }

    if ((bounds1.size()-1) * (bounds2.size()-1) == counts.size())
        ierr = counts.size();
    else
        ierr = -2;
    if (ierr > 0 && ibis::gVerbose > 0) {
        timer.stop();
        logMessage("old2DDistribution",
                   "computing the joint distribution of column %s and "
                   "%s%s%s took %g sec(CPU), %g sec(elapsed)",
                   (*it1).first, (*it2).first,
                   (constraints ? " with restriction " : ""),
                   (constraints ? constraints : ""),
                   timer.CPUTime(), timer.realTime());
    }
    return ierr;
} // ibis::part::old2DDistribution

/// If the string constraints is nil or an empty string or starting with an
/// asterisk (*), it is assumed every valid record of the named column is
/// used.  Arrays bounds1 and bins are both for output only.  Upon
/// successful completion of this function, the return value shall be the
/// number of bins actually used.  A return value of 0 indicates no record
/// satisfy the constraints.  A negative return indicates error.
///
/// @sa ibis::part::get2DDistribution
long ibis::part::get2DBins(const char *constraints,
                           const char *cname1, const char *cname2,
                           uint32_t nb1, uint32_t nb2,
                           std::vector<double> &bounds1,
                           std::vector<double> &bounds2,
                           std::vector<ibis::bitvector> &bins) const {
    if (cname1 == 0 || *cname1 == 0 || cname2 == 0 || *cname2 == 0) return -1L;
    ibis::column *col1 = getColumn(cname1);
    ibis::column *col2 = getColumn(cname2);
    if (col1 == 0 || col2 == 0) return -2L;
    std::string mesg;
    {
        std::ostringstream oss;
        oss << "part[" << (m_name ? m_name : "") << "]::get2DBins("
            << cname1 << ", " << cname2 << ", " << nb1 << ", " << nb2 << ")";
        mesg = oss.str();
    }
    ibis::util::timer atimer(mesg.c_str(), 1);
    ibis::bitvector mask;
    long ierr;
    col1->getNullMask(mask);
    {
        ibis::bitvector tmp;
        col2->getNullMask(tmp);
        mask &= tmp;
    }
    if (constraints != 0 && *constraints != 0 && *constraints != '*') {
        // process the constraints to compute the mask
        ibis::countQuery qq(this);
        ierr = qq.setWhereClause(constraints);
        if (ierr < 0)
            return -4L;
        ierr = qq.evaluate();
        if (ierr < 0)
            return -5L;

        if (qq.getNumHits() == 0) {
            bounds1.clear();
            bins.clear();
            return 0L;
        }
        mask &= (*(qq.getHitVector()));
        LOGGER(ibis::gVerbose > 1)
            << mesg << " -- constraints \"" << constraints << "\" select "
            << mask.cnt() << " record" << (mask.cnt() > 1 ? "s" : "")
            << " out of " << nEvents;
    }

    if (mask.cnt() > 1) { // determine the number of bins to use
        if (nb1 <= 1) nb1 = 100;
        if (nb2 <= 1) nb2 = 100;
        const uint32_t nrows = mask.cnt();
        double tmp = exp(log((double)nrows)/3.0);
        if (nb1 > 2048 && (double)nb1 > tmp) {
            if (nrows > 10000000)
                nb1 = static_cast<uint32_t>(0.5 + tmp);
            else
                nb1 = 2048;
        }
        if (nb2 > 2048 && (double)nb2 > tmp) {
            if (nrows > 10000000)
                nb2 = static_cast<uint32_t>(0.5 + tmp);
            else
                nb2 = 2048;
        }
    }

    std::vector<ibis::bitvector> bins1;
    ierr = get1DBins_(mask, *col1, nb1, bounds1, bins1, mesg.c_str());
    if (ierr <= 0) {
        LOGGER(ibis::gVerbose > 0)
            << mesg << " -- get1DBins_ on " << cname1 << " failed with error "
            << ierr;
        return ierr;
    }

    std::vector<ibis::bitvector> bins2;
    ierr = get1DBins_(mask, *col2, nb2, bounds2, bins2, mesg.c_str());
    if (ierr <= 0) {
        LOGGER(ibis::gVerbose > 0)
            << mesg << " -- get1DBins_ on " << cname2 << " failed with error "
            << ierr;
        return ierr;
    }

    ierr = ibis::util::intersect(bins1, bins2, bins);
    return ierr;
} // ibis::part::get2DBins

/// The templated function to decide the bin boundaries and count the number
/// of values fall in each bin.  This function differs from the one used by
/// getJointDistribution in that the bounds are defined with only closed
/// bins.
///
/// @note It goes through each data value twice, once to count each
/// individial values and once to put them into the specified bins.
///
/// @note The results of first counting may take up more memory than the
/// input data!
template <typename E1, typename E2>
void ibis::part::mapValues(array_t<E1> &val1, array_t<E2> &val2,
                           uint32_t nb1, uint32_t nb2,
                           array_t<E1> &bnd1, array_t<E2> &bnd2,
                           std::vector<uint32_t> &cnts) {
    if (val1.size() == 0 || val2.size() == 0 || val1.size() != val2.size())
        return;
    const uint32_t nr = (val1.size() <= val2.size() ?
                       val1.size() : val2.size());
    ibis::horometer timer;
    if (ibis::gVerbose > 3) {
        LOGGER(ibis::gVerbose > 4)
            << "part::mapValues(" << typeid(E1).name() << "["
            << val1.size() << "], " << typeid(E2).name() << "["
            << val2.size() << "], " << nb1 << ", " << nb2 << ") starting ...";
        timer.start();
    }
#if defined(SORT_VALUES_BEFORE_COUNT)
    ibis::util::sortall(val1, val2);
// #else
//     if (nb1*nb2 > 4096)
//      ibis::util::sortall(val1, val2);
#endif
    equalWeightBins(val1, nb1, bnd1);
    equalWeightBins(val2, nb2, bnd2);
    if (ibis::gVerbose > 3) {
        timer.stop();
        LOGGER(ibis::gVerbose >= 0)
            << "part::mapValues(" << typeid(E1).name() << "["
            << val1.size() << "], " << typeid(E2).name() << "["
            << val2.size() << "], " << nb1 << ", " << nb2
            << ") spent " << timer.CPUTime() << " sec(CPU), "
            << timer.realTime() << " sec(elapsed) to determine bin boundaries";
        timer.start();
    }

    const uint32_t nbnd1 = bnd1.size() - 1;
    const uint32_t nbnd2 = bnd2.size() - 1;
    cnts.resize(nbnd2 * nbnd1);
    for (uint32_t i = 0; i < nbnd2 * nbnd1; ++ i)
        cnts[i] = 0;

    for (uint32_t i = 0; i < nr; ++ i) {
        const uint32_t j1 = bnd1.find(val1[i]);
        const uint32_t j2 = bnd2.find(val2[i]);
        ++ cnts[(j1 - (bnd1[j1]>val1[i]))*nbnd2 + j2 - (bnd2[j2]>val2[i])];
    }
    if (ibis::gVerbose > 3) {
        timer.stop();
        LOGGER(true)
            << "part::mapValues(" << typeid(E1).name() << "["
            << val1.size() << "], " << typeid(E2).name() << "["
            << val2.size() << "], " << nb1 << ", " << nb2
            << ") spent " << timer.CPUTime() << " sec(CPU), "
            << timer.realTime() << " sec(elapsed) to count the number "
            "of values in each bin";
    }
} // ibis::part::mapValues

template <typename T>
void ibis::part::equalWeightBins(const array_t<T> &vals, uint32_t nbins,
                                 array_t<T> &bounds) {
    typename std::map<T, uint32_t> hist;
    ibis::part::mapValues(vals, hist);
    const uint32_t ncard = hist.size();
    array_t<uint32_t> ctmp;
    array_t<T> vtmp;
    ctmp.reserve(ncard);
    vtmp.reserve(ncard);
    for (typename std::map<T, uint32_t>::const_iterator it = hist.begin();
         it != hist.end(); ++ it) {
        vtmp.push_back((*it).first);
        ctmp.push_back((*it).second);
    }
    hist.clear();

    array_t<uint32_t> hbnd(nbins);
    ibis::index::divideCounts(hbnd, ctmp);
    bounds.clear();
    bounds.reserve(hbnd.size()+1);
    bounds.push_back(vtmp[0]);
    for (uint32_t i = 0; i < hbnd.size() && hbnd[i] < ncard; ++ i)
        bounds.push_back(vtmp[hbnd[i]]);

    if (bounds.size() > 1) {
        T end1 = bounds.back() - bounds[bounds.size()-2];
        T end2 = vtmp.back() + end1;
        end1 += bounds.back();
        bounds.push_back(end1 > vtmp.back() ? end1 : end2);
    }
    else {
        bounds.push_back(vtmp.back()+1);
    }
} // ibis::part::equalWeightBins

template <typename T>
void ibis::part::mapValues(const array_t<T> &vals,
                           std::map<T, uint32_t> &hist) {
    for (uint32_t i = 0; i < vals.size(); ++ i) {
        typename std::map<T, uint32_t>::iterator it = hist.find(vals[i]);
        if (it != hist.end())
            ++ (*it).second;
        else
            hist.insert(std::make_pair(vals[i], 1));
    }
} // ibis::part::mapValues

/// Explicit specialization for float arrays.  Goes through the data twice,
/// once to find the actual min and max values, and once to place the
/// values in ten times as many bins as desired.  It then coalesces the
/// finer bins into desired number of bins.
template <>
void ibis::part::equalWeightBins(const array_t<float> &vals,
                                 uint32_t nbins, array_t<float> &bounds) {
    float amax = vals[0];
    float amin = vals[0];
    // first compute the actual min and max
    for (unsigned i = 1; i < vals.size(); ++ i) {
        if (amax < vals[i]) amax = vals[i];
        if (amin > vals[i]) amin = vals[i];
    }
    if (amin >= amax) {  // a single value
        bounds.resize(2);
        bounds[0] = amin;
        bounds[1] = ibis::util::compactValue(amin, DBL_MAX);
        return;
    }
    if (nbins <= 1) nbins = 16;
    uint32_t nb2 = nbins * 10;
    const float stride =
        ibis::util::compactValue2((amax - amin) / nb2,
                                  (amax - amin) * (nb2 + 0.75) / nb2);
    array_t<uint32_t> cnts(nb2, 0U);
    for (unsigned i = 0; i < vals.size(); ++ i)
        ++ cnts[(unsigned) ((vals[i]-amin)/stride)];

    array_t<uint32_t> hbnd(nbins);
    ibis::index::divideCounts(hbnd, cnts);
    bounds.clear();
    bounds.reserve(hbnd.size()+1);
    bounds.push_back(amin);
    for (uint32_t i = 0; i < hbnd.size() && hbnd[i] < nb2; ++ i)
        bounds.push_back(amin + stride *hbnd[i]);
    bounds.push_back(amin+stride*nb2);
} // ibis::part::equalWeightBins

/// Explicit specialization for double arrays.  Goes through the data
/// twice, once to find the actual min and max values, and once to place
/// the values in ten times as many bins as desired.  It then coalesces the
/// finer bins into desired number of bins.
template <>
void ibis::part::equalWeightBins(const array_t<double> &vals,
                                 uint32_t nbins, array_t<double> &bounds) {
    double amax = vals[0];
    double amin = vals[0];
    // first compute the actual min and max
    for (unsigned i = 1; i < vals.size(); ++ i) {
        if (amax < vals[i]) amax = vals[i];
        if (amin > vals[i]) amin = vals[i];
    }
    if (amin >= amax) {  // a single value
        bounds.resize(2);
        bounds[0] = amin;
        bounds[1] = ibis::util::compactValue(amin, DBL_MAX);
        return;
    }
    if (nbins <= 1) nbins = 16;
    uint32_t nb2 = nbins * 10;
    const double stride =
        ibis::util::compactValue2((amax - amin) / nb2,
                                  (amax - amin) * (nb2 + 0.75) / nb2);
    array_t<uint32_t> cnts(nb2, 0U);
    for (unsigned i = 0; i < vals.size(); ++ i)
        ++ cnts[(unsigned) ((vals[i]-amin)/stride)];

    array_t<uint32_t> hbnd(nbins);
    ibis::index::divideCounts(hbnd, cnts);
    bounds.clear();
    bounds.reserve(hbnd.size()+1);
    bounds.push_back(amin);
    for (uint32_t i = 0; i < hbnd.size() && hbnd[i] < nb2; ++ i)
        bounds.push_back(amin + stride *hbnd[i]);
    bounds.push_back(amin+stride*nb2);
} // ibis::part::equalWeightBins

/// It returns three arrays, @c bounds1, @c bounds2, and @c counts.  The
/// arrays @c bounds1 and@c bounds2 defines two sets of bins one for each
/// variable.  Together they define
/// @code (bounds1.size()+1) (bounds2.size()+1) @endcode
/// bins for the 2-D joint distributions.
///
/// On successful completion of this function, it return the number of
/// bins.
///
/// @note The arrays @c bounds1 and @c bounds2 are used if they contain
/// values in ascending order.  If they are empty or their values are not
/// in ascending order, then a simple linear binning will be used.  By
/// default, no more than 256 bins are used for each variable.
///
/// @note Deprecated.
long
ibis::part::getJointDistribution(const char *constraints,
                                 const char *name1, const char *name2,
                                 std::vector<double> &bounds1,
                                 std::vector<double> &bounds2,
                                 std::vector<uint32_t> &counts) const {
    long ierr = -1;
    columnList::const_iterator it1 = columns.find(name1);
    columnList::const_iterator it2 = columns.find(name2);
    if (it1 == columns.end() || it2 == columns.end()) {
        if (it1 == columns.end())
            logWarning("getJointDistribution", "%s is not a known column name",
                       name1);
        if (it2 == columns.end())
            logWarning("getJointDistribution", "%s is not a known column name",
                       name2);
        return ierr;
    }

    const ibis::column *col1 = (*it1).second;
    const ibis::column *col2 = (*it2).second;
    ibis::horometer timer;
    if (ibis::gVerbose > 0) {
        LOGGER(ibis::gVerbose > 2)
            << "part[" << (m_name ? m_name : "")
            << "]::getJointDistribution attempting to compute a histogram of "
            << name1 << " and " << name2
            << (constraints && *constraints ? " subject to " :
                " without constraints")
            << (constraints ? constraints : "");
        timer.start();
    }
    ibis::bitvector mask;
    col1->getNullMask(mask);
    {
        ibis::bitvector tmp;
        col2->getNullMask(tmp);
        mask &= tmp;
    }
    if (constraints != 0 && *constraints != 0) {
        ibis::countQuery q(this);
        q.setWhereClause(constraints);
        ierr = q.evaluate();
        if (ierr < 0)
            return ierr;
        const ibis::bitvector *hits = q.getHitVector();
        if (hits->cnt() == 0) // nothing to do any more
            return 0;
        mask &= (*hits);
    }

    counts.clear();
    switch (col1->type()) {
    case ibis::SHORT:
    case ibis::BYTE:
    case ibis::INT: {
        array_t<int32_t> *val1 = col1->selectInts(mask);
        if (val1 == 0) {
            ierr = -4;
            break;
        }
        array_t<int32_t> bnd1;
        if (bounds1.size() > 0) {
            bnd1.resize(bounds1.size());
            for (uint32_t i = 0; i < bounds1.size(); ++ i)
                bnd1[i] = static_cast<int32_t>(bounds1[i]);
        }
        switch (col2->type()) {
        case ibis::SHORT:
        case ibis::BYTE:
        case ibis::INT: {
            array_t<int32_t> *val2 = col2->selectInts(mask);
            if (val2 == 0) {
                ierr = -5;
                break;
            }

            array_t<int32_t> bnd2;
            if (bounds2.size() > 0) {
                bnd2.resize(bounds2.size());
                for (uint32_t i = 0; i < bounds2.size(); ++ i)
                    bnd2[i] = static_cast<int32_t>(bounds2[i]);
            }
            ibis::index::mapValues(*val1, *val2, bnd1, bnd2, counts);
            delete val2;
            bounds1.resize(bnd1.size());
            bounds2.resize(bnd2.size());
            for (uint32_t i = 0; i < bnd1.size(); ++ i)
                bounds1[i] = bnd1[i];
            for (uint32_t i = 0; i < bnd2.size(); ++ i)
                bounds2[i] = bnd2[i];
            break;}
        case ibis::USHORT:
        case ibis::UBYTE:
        case ibis::UINT:
        case ibis::CATEGORY: {
            array_t<uint32_t> *val2 = col2->selectUInts(mask);
            if (val2 == 0) {
                ierr = -5;
                break;
            }

            array_t<uint32_t> bnd2;
            if (bounds2.size() > 0) {
                bnd2.resize(bounds2.size());
                for (uint32_t i = 0; i < bounds2.size(); ++ i)
                    bnd2[i] = static_cast<uint32_t>(bounds2[i]);
            }
            ibis::index::mapValues(*val1, *val2, bnd1, bnd2, counts);
            delete val2;
            bounds1.resize(bnd1.size());
            bounds2.resize(bnd2.size());
            for (uint32_t i = 0; i < bnd1.size(); ++ i)
                bounds1[i] = bnd1[i];
            for (uint32_t i = 0; i < bnd2.size(); ++ i)
                bounds2[i] = bnd2[i];
            break;}
        case ibis::FLOAT: {
            array_t<float> *val2 = col2->selectFloats(mask);
            if (val2 == 0) {
                ierr = -5;
                break;
            }

            array_t<float> bnd2;
            if (bounds2.size() > 0) {
                bnd2.resize(bounds2.size());
                for (uint32_t i = 0; i < bounds2.size(); ++ i)
                    bnd2[i] = static_cast<float>(bounds2[i]);
            }
            ibis::index::mapValues(*val1, *val2, bnd1, bnd2, counts);
            delete val2;
            bounds1.resize(bnd1.size());
            bounds2.resize(bnd2.size());
            for (uint32_t i = 0; i < bnd1.size(); ++ i)
                bounds1[i] = bnd1[i];
            for (uint32_t i = 0; i < bnd2.size(); ++ i)
                bounds2[i] = bnd2[i];
            break;}
        case ibis::DOUBLE: {
            array_t<double> bnd2;
            array_t<double> *val2 = col2->selectDoubles(mask);
            if (val2 == 0) {
                ierr = -5;
                break;
            }

            if (bounds2.size() > 0) {
                bnd2.resize(bounds2.size());
                for (uint32_t i = 0; i < bounds2.size(); ++ i)
                    bnd2[i] = static_cast<double>(bounds2[i]);
            }
            ibis::index::mapValues(*val1, *val2, bnd1, bnd2, counts);
            delete val2;
            bounds1.resize(bnd1.size());
            for (uint32_t i = 0; i < bnd1.size(); ++ i)
                bounds1[i] = bnd1[i];
            bounds2.resize(bnd2.size());
            for (uint32_t i = 0; i < bnd2.size(); ++ i)
                bounds2[i] = bnd2[i];
            break;}
        default: {
            ierr = -3;
            logWarning("getJointDistribution",
                       "can not handle column type %d",
                       static_cast<int>(col2->type()));
            break;}
        }
        delete val1;
        break;}
    case ibis::USHORT:
    case ibis::UBYTE:
    case ibis::UINT:
    case ibis::CATEGORY: {
        array_t<uint32_t> *val1 = col1->selectUInts(mask);
        if (val1 == 0) {
            ierr = -4;
            break;
        }

        array_t<uint32_t> bnd1;
        if (bounds1.size() > 0) {
            bnd1.resize(bounds1.size());
            for (unsigned i = 0; i < bounds1.size(); ++ i)
                bnd1[i] = static_cast<uint32_t>(bounds1[i]);
        }
        switch (col2->type()) {
        case ibis::SHORT:
        case ibis::BYTE:
        case ibis::INT: {
            array_t<int32_t> *val2 = col2->selectInts(mask);
            if (val2 == 0) {
                ierr = -5;
                break;
            }

            array_t<int32_t> bnd2;
            if (bounds2.size() > 0) {
                bnd2.resize(bounds2.size());
                for (uint32_t i = 0; i < bounds2.size(); ++ i)
                    bnd2[i] = static_cast<int32_t>(bounds2[i]);
            }
            ibis::index::mapValues(*val1, *val2, bnd1, bnd2, counts);
            delete val2;
            bounds1.resize(bnd1.size());
            bounds2.resize(bnd2.size());
            for (uint32_t i = 0; i < bnd1.size(); ++ i)
                bounds1[i] = bnd1[i];
            for (uint32_t i = 0; i < bnd2.size(); ++ i)
                bounds2[i] = bnd2[i];
            break;}
        case ibis::USHORT:
        case ibis::UBYTE:
        case ibis::UINT:
        case ibis::CATEGORY: {
            array_t<uint32_t> *val2 = col2->selectUInts(mask);
            if (val2 == 0) {
                ierr = -5;
                break;
            }

            array_t<uint32_t> bnd2;
            if (bounds2.size() > 0) {
                bnd2.resize(bounds2.size());
                for (uint32_t i = 0; i < bounds2.size(); ++ i)
                    bnd2[i] = static_cast<uint32_t>(bounds2[i]);
            }
            ibis::index::mapValues(*val1, *val2, bnd1, bnd2, counts);
            delete val2;
            bounds1.resize(bnd1.size());
            bounds2.resize(bnd2.size());
            for (uint32_t i = 0; i < bnd1.size(); ++ i)
                bounds1[i] = bnd1[i];
            for (uint32_t i = 0; i < bnd2.size(); ++ i)
                bounds2[i] = bnd2[i];
            break;}
        case ibis::FLOAT: {
            array_t<float> *val2 = col2->selectFloats(mask);
            if (val2 == 0) {
                ierr = -5;
                break;
            }

            array_t<float> bnd2;
            if (bounds2.size() > 0) {
                bnd2.resize(bounds2.size());
                for (uint32_t i = 0; i < bounds2.size(); ++ i)
                    bnd2[i] = static_cast<float>(bounds2[i]);
            }
            ibis::index::mapValues(*val1, *val2, bnd1, bnd2, counts);
            delete val2;
            bounds1.resize(bnd1.size());
            bounds2.resize(bnd2.size());
            for (uint32_t i = 0; i < bnd1.size(); ++ i)
                bounds1[i] = bnd1[i];
            for (uint32_t i = 0; i < bnd2.size(); ++ i)
                bounds2[i] = bnd2[i];
            break;}
        case ibis::DOUBLE: {
            array_t<double> bnd2;
            array_t<double> *val2 = col2->selectDoubles(mask);
            if (val2 == 0) {
                ierr = -5;
                break;
            }

            if (bounds2.size() > 0) {
                bnd2.resize(bounds2.size());
                for (uint32_t i = 0; i < bounds2.size(); ++ i)
                    bnd2[i] = static_cast<double>(bounds2[i]);
            }
            ibis::index::mapValues(*val1, *val2, bnd1, bnd2, counts);
            delete val2;
            bounds1.resize(bnd1.size());
            for (uint32_t i = 0; i < bnd1.size(); ++ i)
                bounds1[i] = bnd1[i];
            bounds2.resize(bnd2.size());
            for (uint32_t i = 0; i < bnd2.size(); ++ i)
                bounds2[i] = bnd2[i];
            break;}
        default: {
            ierr = -3;
            logWarning("getJointDistribution",
                       "can not handle column type %d",
                       static_cast<int>(col2->type()));
            break;}
        }
        delete val1;
        break;}
    case ibis::FLOAT: {
        array_t<float> *val1 = col1->selectFloats(mask);
        if (val1 == 0) {
            ierr = -4;
            break;
        }

        array_t<float> bnd1;
        if (bounds1.size() > 0) {
            bnd1.resize(bounds1.size());
            for (uint32_t i = 0; i < bounds1.size(); ++ i)
                bnd1[i] = static_cast<float>(bounds1[i]);
        }
        switch (col2->type()) {
        case ibis::SHORT:
        case ibis::BYTE:
        case ibis::INT: {
            array_t<int32_t> *val2 = col2->selectInts(mask);
            if (val2 == 0) {
                ierr = -5;
                break;
            }

            array_t<int32_t> bnd2;
            if (bounds2.size() > 0) {
                bnd2.resize(bounds2.size());
                for (uint32_t i = 0; i < bounds2.size(); ++ i)
                    bnd2[i] = static_cast<int32_t>(bounds2[i]);
            }
            ibis::index::mapValues(*val1, *val2, bnd1, bnd2, counts);
            delete val2;
            bounds1.resize(bnd1.size());
            bounds2.resize(bnd2.size());
            for (uint32_t i = 0; i < bnd1.size(); ++ i)
                bounds1[i] = bnd1[i];
            for (uint32_t i = 0; i < bnd2.size(); ++ i)
                bounds2[i] = bnd2[i];
            break;}
        case ibis::USHORT:
        case ibis::UBYTE:
        case ibis::UINT:
        case ibis::CATEGORY: {
            array_t<uint32_t> *val2 = col2->selectUInts(mask);
            if (val2 == 0) {
                ierr = -5;
                break;
            }

            array_t<uint32_t> bnd2;
            if (bounds2.size() > 0) {
                bnd2.resize(bounds2.size());
                for (uint32_t i = 0; i < bounds2.size(); ++ i)
                    bnd2[i] = static_cast<uint32_t>(bounds2[i]);
            }
            ibis::index::mapValues(*val1, *val2, bnd1, bnd2, counts);
            delete val2;
            bounds1.resize(bnd1.size());
            bounds2.resize(bnd2.size());
            for (uint32_t i = 0; i < bnd1.size(); ++ i)
                bounds1[i] = bnd1[i];
            for (uint32_t i = 0; i < bnd2.size(); ++ i)
                bounds2[i] = bnd2[i];
            break;}
        case ibis::FLOAT: {
            array_t<float> *val2 = col2->selectFloats(mask);
            if (val2 == 0) {
                ierr = -5;
                break;
            }

            array_t<float> bnd2;
            if (bounds2.size() > 0) {
                bnd2.resize(bounds2.size());
                for (uint32_t i = 0; i < bounds2.size(); ++ i)
                    bnd2[i] = static_cast<float>(bounds2[i]);
            }
            ibis::index::mapValues(*val1, *val2, bnd1, bnd2, counts);
            delete val2;
            bounds1.resize(bnd1.size());
            bounds2.resize(bnd2.size());
            for (uint32_t i = 0; i < bnd1.size(); ++ i)
                bounds1[i] = bnd1[i];
            for (uint32_t i = 0; i < bnd2.size(); ++ i)
                bounds2[i] = bnd2[i];
            break;}
        case ibis::DOUBLE: {
            array_t<double> bnd2;
            array_t<double> *val2 = col2->selectDoubles(mask);
            if (val2 == 0) {
                ierr = -5;
                break;
            }

            if (bounds2.size() > 0) {
                bnd2.resize(bounds2.size());
                for (uint32_t i = 0; i < bounds2.size(); ++ i)
                    bnd2[i] = static_cast<double>(bounds2[i]);
            }
            ibis::index::mapValues(*val1, *val2, bnd1, bnd2, counts);
            delete val2;
            bounds1.resize(bnd1.size());
            for (uint32_t i = 0; i < bnd1.size(); ++ i)
                bounds1[i] = bnd1[i];
            bounds2.resize(bnd2.size());
            for (uint32_t i = 0; i < bnd2.size(); ++ i)
                bounds2[i] = bnd2[i];
            break;}
        default: {
            ierr = -3;
            logWarning("getJointDistribution",
                       "can not handle column type %d",
                       static_cast<int>(col2->type()));
            break;}
        }
        delete val1;
        break;}
    case ibis::DOUBLE: {
        array_t<double> *val1 = col1->selectDoubles(mask);
        if (val1 == 0) {
            ierr = -4;
            break;
        }

        array_t<double> bnd1;
        if (bounds1.size() > 0) {
            bnd1.resize(bounds1.size());
            for (uint32_t i = 0; i < bounds1.size(); ++ i)
                bnd1[i] = bounds1[i];
        }
        switch (col2->type()) {
        case ibis::SHORT:
        case ibis::BYTE:
        case ibis::INT: {
            array_t<int32_t> *val2 = col2->selectInts(mask);
            if (val2 == 0) {
                ierr = -5;
                break;
            }

            array_t<int32_t> bnd2;
            if (bounds2.size() > 0) {
                bnd2.resize(bounds2.size());
                for (uint32_t i = 0; i < bounds2.size(); ++ i)
                    bnd2[i] = static_cast<int32_t>(bounds2[i]);
            }
            ibis::index::mapValues(*val1, *val2, bnd1, bnd2, counts);
            delete val2;
            bounds1.resize(bnd1.size());
            bounds2.resize(bnd2.size());
            for (uint32_t i = 0; i < bnd1.size(); ++ i)
                bounds1[i] = bnd1[i];
            for (uint32_t i = 0; i < bnd2.size(); ++ i)
                bounds2[i] = bnd2[i];
            break;}
        case ibis::USHORT:
        case ibis::UBYTE:
        case ibis::UINT:
        case ibis::CATEGORY: {
            array_t<uint32_t> *val2 = col2->selectUInts(mask);
            if (val2 == 0) {
                ierr = -5;
                break;
            }

            array_t<uint32_t> bnd2;
            if (bounds2.size() > 0) {
                bnd2.resize(bounds2.size());
                for (uint32_t i = 0; i < bounds2.size(); ++ i)
                    bnd2[i] = static_cast<uint32_t>(bounds2[i]);
            }
            ibis::index::mapValues(*val1, *val2, bnd1, bnd2, counts);
            delete val2;
            bounds1.resize(bnd1.size());
            bounds2.resize(bnd2.size());
            for (uint32_t i = 0; i < bnd1.size(); ++ i)
                bounds1[i] = bnd1[i];
            for (uint32_t i = 0; i < bnd2.size(); ++ i)
                bounds2[i] = bnd2[i];
            break;}
        case ibis::FLOAT: {
            array_t<float> *val2 = col2->selectFloats(mask);
            if (val2 == 0) {
                ierr = -5;
                break;
            }

            array_t<float> bnd2;
            if (bounds2.size() > 0) {
                bnd2.resize(bounds2.size());
                for (uint32_t i = 0; i < bounds2.size(); ++ i)
                    bnd2[i] = static_cast<float>(bounds2[i]);
            }
            ibis::index::mapValues(*val1, *val2, bnd1, bnd2, counts);
            delete val2;
            bounds1.resize(bnd1.size());
            bounds2.resize(bnd2.size());
            for (uint32_t i = 0; i < bnd1.size(); ++ i)
                bounds1[i] = bnd1[i];
            for (uint32_t i = 0; i < bnd2.size(); ++ i)
                bounds2[i] = bnd2[i];
            break;}
        case ibis::DOUBLE: {
            array_t<double> bnd2;
            array_t<double> *val2 = col2->selectDoubles(mask);
            if (val2 == 0) {
                ierr = -5;
                break;
            }

            if (bounds2.size() > 0) {
                bnd2.resize(bounds2.size());
                for (uint32_t i = 0; i < bounds2.size(); ++ i)
                    bnd2[i] = static_cast<double>(bounds2[i]);
            }
            ibis::index::mapValues(*val1, *val2, bnd1, bnd2, counts);
            delete val2;
            bounds1.resize(bnd1.size());
            for (uint32_t i = 0; i < bnd1.size(); ++ i)
                bounds1[i] = bnd1[i];
            bounds2.resize(bnd2.size());
            for (uint32_t i = 0; i < bnd2.size(); ++ i)
                bounds2[i] = bnd2[i];
            break;}
        default: {
            ierr = -3;
            logWarning("getJointDistribution",
                       "can not handle column type %d",
                       static_cast<int>(col2->type()));
            break;}
        }
        delete val1;
        break;}
    default: {
        ierr = -3;
        logWarning("getJointDistribution",
                   "can not handle column type %d",
                   static_cast<int>(col1->type()));
        break;}
    }

    if ((bounds1.size()+1) * (bounds2.size()+1) == counts.size())
        ierr = counts.size();
    else
        ierr = -2;
    if (ierr > 0 && ibis::gVerbose > 0) {
        timer.stop();
        logMessage("getJointDistribution",
                   "computing the joint distribution of "
                   "column %s and %s%s%s took %g "
                   "sec(CPU), %g sec(elapsed)",
                   (*it1).first, (*it2).first,
                   (constraints ? " with restriction " : ""),
                   (constraints ? constraints : ""),
                   timer.CPUTime(), timer.realTime());
    }
    return ierr;
} // ibis::part::getJointDistribution

// explicit instantiation
template long ibis::part::adaptive2DBins
(const array_t<signed char> &, const array_t<signed char> &, uint32_t, uint32_t,
 std::vector<double> &, std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptive2DBins
(const array_t<signed char> &, const array_t<unsigned char> &, uint32_t, uint32_t,
 std::vector<double> &, std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptive2DBins
(const array_t<signed char> &, const array_t<int16_t> &, uint32_t, uint32_t,
 std::vector<double> &, std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptive2DBins
(const array_t<signed char> &, const array_t<uint16_t> &, uint32_t, uint32_t,
 std::vector<double> &, std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptive2DBins
(const array_t<signed char> &, const array_t<int32_t> &, uint32_t, uint32_t,
 std::vector<double> &, std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptive2DBins
(const array_t<signed char> &, const array_t<uint32_t> &, uint32_t, uint32_t,
 std::vector<double> &, std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptive2DBins
(const array_t<signed char> &, const array_t<int64_t> &, uint32_t, uint32_t,
 std::vector<double> &, std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptive2DBins
(const array_t<signed char> &, const array_t<uint64_t> &, uint32_t, uint32_t,
 std::vector<double> &, std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptive2DBins
(const array_t<signed char> &, const array_t<float> &, uint32_t, uint32_t,
 std::vector<double> &, std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptive2DBins
(const array_t<signed char> &, const array_t<double> &, uint32_t, uint32_t,
 std::vector<double> &, std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptive2DBins
(const array_t<unsigned char> &, const array_t<signed char> &, uint32_t, uint32_t,
 std::vector<double> &, std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptive2DBins
(const array_t<unsigned char> &, const array_t<unsigned char> &, uint32_t, uint32_t,
 std::vector<double> &, std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptive2DBins
(const array_t<unsigned char> &, const array_t<int16_t> &, uint32_t, uint32_t,
 std::vector<double> &, std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptive2DBins
(const array_t<unsigned char> &, const array_t<uint16_t> &, uint32_t, uint32_t,
 std::vector<double> &, std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptive2DBins
(const array_t<unsigned char> &, const array_t<int32_t> &, uint32_t, uint32_t,
 std::vector<double> &, std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptive2DBins
(const array_t<unsigned char> &, const array_t<uint32_t> &, uint32_t, uint32_t,
 std::vector<double> &, std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptive2DBins
(const array_t<unsigned char> &, const array_t<int64_t> &, uint32_t, uint32_t,
 std::vector<double> &, std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptive2DBins
(const array_t<unsigned char> &, const array_t<uint64_t> &, uint32_t, uint32_t,
 std::vector<double> &, std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptive2DBins
(const array_t<unsigned char> &, const array_t<float> &, uint32_t, uint32_t,
 std::vector<double> &, std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptive2DBins
(const array_t<unsigned char> &, const array_t<double> &, uint32_t, uint32_t,
 std::vector<double> &, std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptive2DBins
(const array_t<int16_t> &, const array_t<signed char> &, uint32_t, uint32_t,
 std::vector<double> &, std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptive2DBins
(const array_t<int16_t> &, const array_t<unsigned char> &, uint32_t, uint32_t,
 std::vector<double> &, std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptive2DBins
(const array_t<int16_t> &, const array_t<int16_t> &, uint32_t, uint32_t,
 std::vector<double> &, std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptive2DBins
(const array_t<int16_t> &, const array_t<uint16_t> &, uint32_t, uint32_t,
 std::vector<double> &, std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptive2DBins
(const array_t<int16_t> &, const array_t<int32_t> &, uint32_t, uint32_t,
 std::vector<double> &, std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptive2DBins
(const array_t<int16_t> &, const array_t<uint32_t> &, uint32_t, uint32_t,
 std::vector<double> &, std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptive2DBins
(const array_t<int16_t> &, const array_t<int64_t> &, uint32_t, uint32_t,
 std::vector<double> &, std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptive2DBins
(const array_t<int16_t> &, const array_t<uint64_t> &, uint32_t, uint32_t,
 std::vector<double> &, std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptive2DBins
(const array_t<int16_t> &, const array_t<float> &, uint32_t, uint32_t,
 std::vector<double> &, std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptive2DBins
(const array_t<int16_t> &, const array_t<double> &, uint32_t, uint32_t,
 std::vector<double> &, std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptive2DBins
(const array_t<uint16_t> &, const array_t<signed char> &, uint32_t, uint32_t,
 std::vector<double> &, std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptive2DBins
(const array_t<uint16_t> &, const array_t<unsigned char> &, uint32_t, uint32_t,
 std::vector<double> &, std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptive2DBins
(const array_t<uint16_t> &, const array_t<int16_t> &, uint32_t, uint32_t,
 std::vector<double> &, std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptive2DBins
(const array_t<uint16_t> &, const array_t<uint16_t> &, uint32_t, uint32_t,
 std::vector<double> &, std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptive2DBins
(const array_t<uint16_t> &, const array_t<int32_t> &, uint32_t, uint32_t,
 std::vector<double> &, std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptive2DBins
(const array_t<uint16_t> &, const array_t<uint32_t> &, uint32_t, uint32_t,
 std::vector<double> &, std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptive2DBins
(const array_t<uint16_t> &, const array_t<int64_t> &, uint32_t, uint32_t,
 std::vector<double> &, std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptive2DBins
(const array_t<uint16_t> &, const array_t<uint64_t> &, uint32_t, uint32_t,
 std::vector<double> &, std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptive2DBins
(const array_t<uint16_t> &, const array_t<float> &, uint32_t, uint32_t,
 std::vector<double> &, std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptive2DBins
(const array_t<uint16_t> &, const array_t<double> &, uint32_t, uint32_t,
 std::vector<double> &, std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptive2DBins
(const array_t<int32_t> &, const array_t<signed char> &, uint32_t, uint32_t,
 std::vector<double> &, std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptive2DBins
(const array_t<int32_t> &, const array_t<unsigned char> &, uint32_t, uint32_t,
 std::vector<double> &, std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptive2DBins
(const array_t<int32_t> &, const array_t<int16_t> &, uint32_t, uint32_t,
 std::vector<double> &, std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptive2DBins
(const array_t<int32_t> &, const array_t<uint16_t> &, uint32_t, uint32_t,
 std::vector<double> &, std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptive2DBins
(const array_t<int32_t> &, const array_t<int32_t> &, uint32_t, uint32_t,
 std::vector<double> &, std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptive2DBins
(const array_t<int32_t> &, const array_t<uint32_t> &, uint32_t, uint32_t,
 std::vector<double> &, std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptive2DBins
(const array_t<int32_t> &, const array_t<int64_t> &, uint32_t, uint32_t,
 std::vector<double> &, std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptive2DBins
(const array_t<int32_t> &, const array_t<uint64_t> &, uint32_t, uint32_t,
 std::vector<double> &, std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptive2DBins
(const array_t<int32_t> &, const array_t<float> &, uint32_t, uint32_t,
 std::vector<double> &, std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptive2DBins
(const array_t<int32_t> &, const array_t<double> &, uint32_t, uint32_t,
 std::vector<double> &, std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptive2DBins
(const array_t<uint32_t> &, const array_t<signed char> &, uint32_t, uint32_t,
 std::vector<double> &, std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptive2DBins
(const array_t<uint32_t> &, const array_t<unsigned char> &, uint32_t, uint32_t,
 std::vector<double> &, std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptive2DBins
(const array_t<uint32_t> &, const array_t<int16_t> &, uint32_t, uint32_t,
 std::vector<double> &, std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptive2DBins
(const array_t<uint32_t> &, const array_t<uint16_t> &, uint32_t, uint32_t,
 std::vector<double> &, std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptive2DBins
(const array_t<uint32_t> &, const array_t<int32_t> &, uint32_t, uint32_t,
 std::vector<double> &, std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptive2DBins
(const array_t<uint32_t> &, const array_t<uint32_t> &, uint32_t, uint32_t,
 std::vector<double> &, std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptive2DBins
(const array_t<uint32_t> &, const array_t<int64_t> &, uint32_t, uint32_t,
 std::vector<double> &, std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptive2DBins
(const array_t<uint32_t> &, const array_t<uint64_t> &, uint32_t, uint32_t,
 std::vector<double> &, std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptive2DBins
(const array_t<uint32_t> &, const array_t<float> &, uint32_t, uint32_t,
 std::vector<double> &, std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptive2DBins
(const array_t<uint32_t> &, const array_t<double> &, uint32_t, uint32_t,
 std::vector<double> &, std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptive2DBins
(const array_t<int64_t> &, const array_t<signed char> &, uint32_t, uint32_t,
 std::vector<double> &, std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptive2DBins
(const array_t<int64_t> &, const array_t<unsigned char> &, uint32_t, uint32_t,
 std::vector<double> &, std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptive2DBins
(const array_t<int64_t> &, const array_t<int16_t> &, uint32_t, uint32_t,
 std::vector<double> &, std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptive2DBins
(const array_t<int64_t> &, const array_t<uint16_t> &, uint32_t, uint32_t,
 std::vector<double> &, std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptive2DBins
(const array_t<int64_t> &, const array_t<int32_t> &, uint32_t, uint32_t,
 std::vector<double> &, std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptive2DBins
(const array_t<int64_t> &, const array_t<uint32_t> &, uint32_t, uint32_t,
 std::vector<double> &, std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptive2DBins
(const array_t<int64_t> &, const array_t<int64_t> &, uint32_t, uint32_t,
 std::vector<double> &, std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptive2DBins
(const array_t<int64_t> &, const array_t<uint64_t> &, uint32_t, uint32_t,
 std::vector<double> &, std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptive2DBins
(const array_t<int64_t> &, const array_t<float> &, uint32_t, uint32_t,
 std::vector<double> &, std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptive2DBins
(const array_t<int64_t> &, const array_t<double> &, uint32_t, uint32_t,
 std::vector<double> &, std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptive2DBins
(const array_t<uint64_t> &, const array_t<signed char> &, uint32_t, uint32_t,
 std::vector<double> &, std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptive2DBins
(const array_t<uint64_t> &, const array_t<unsigned char> &, uint32_t, uint32_t,
 std::vector<double> &, std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptive2DBins
(const array_t<uint64_t> &, const array_t<int16_t> &, uint32_t, uint32_t,
 std::vector<double> &, std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptive2DBins
(const array_t<uint64_t> &, const array_t<uint16_t> &, uint32_t, uint32_t,
 std::vector<double> &, std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptive2DBins
(const array_t<uint64_t> &, const array_t<int32_t> &, uint32_t, uint32_t,
 std::vector<double> &, std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptive2DBins
(const array_t<uint64_t> &, const array_t<uint32_t> &, uint32_t, uint32_t,
 std::vector<double> &, std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptive2DBins
(const array_t<uint64_t> &, const array_t<int64_t> &, uint32_t, uint32_t,
 std::vector<double> &, std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptive2DBins
(const array_t<uint64_t> &, const array_t<uint64_t> &, uint32_t, uint32_t,
 std::vector<double> &, std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptive2DBins
(const array_t<uint64_t> &, const array_t<float> &, uint32_t, uint32_t,
 std::vector<double> &, std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptive2DBins
(const array_t<uint64_t> &, const array_t<double> &, uint32_t, uint32_t,
 std::vector<double> &, std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptive2DBins
(const array_t<float> &, const array_t<signed char> &, uint32_t, uint32_t,
 std::vector<double> &, std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptive2DBins
(const array_t<float> &, const array_t<unsigned char> &, uint32_t, uint32_t,
 std::vector<double> &, std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptive2DBins
(const array_t<float> &, const array_t<int16_t> &, uint32_t, uint32_t,
 std::vector<double> &, std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptive2DBins
(const array_t<float> &, const array_t<uint16_t> &, uint32_t, uint32_t,
 std::vector<double> &, std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptive2DBins
(const array_t<float> &, const array_t<int32_t> &, uint32_t, uint32_t,
 std::vector<double> &, std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptive2DBins
(const array_t<float> &, const array_t<uint32_t> &, uint32_t, uint32_t,
 std::vector<double> &, std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptive2DBins
(const array_t<float> &, const array_t<int64_t> &, uint32_t, uint32_t,
 std::vector<double> &, std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptive2DBins
(const array_t<float> &, const array_t<uint64_t> &, uint32_t, uint32_t,
 std::vector<double> &, std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptive2DBins
(const array_t<float> &, const array_t<float> &, uint32_t, uint32_t,
 std::vector<double> &, std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptive2DBins
(const array_t<float> &, const array_t<double> &, uint32_t, uint32_t,
 std::vector<double> &, std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptive2DBins
(const array_t<double> &, const array_t<signed char> &, uint32_t, uint32_t,
 std::vector<double> &, std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptive2DBins
(const array_t<double> &, const array_t<unsigned char> &, uint32_t, uint32_t,
 std::vector<double> &, std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptive2DBins
(const array_t<double> &, const array_t<int16_t> &, uint32_t, uint32_t,
 std::vector<double> &, std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptive2DBins
(const array_t<double> &, const array_t<uint16_t> &, uint32_t, uint32_t,
 std::vector<double> &, std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptive2DBins
(const array_t<double> &, const array_t<int32_t> &, uint32_t, uint32_t,
 std::vector<double> &, std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptive2DBins
(const array_t<double> &, const array_t<uint32_t> &, uint32_t, uint32_t,
 std::vector<double> &, std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptive2DBins
(const array_t<double> &, const array_t<int64_t> &, uint32_t, uint32_t,
 std::vector<double> &, std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptive2DBins
(const array_t<double> &, const array_t<uint64_t> &, uint32_t, uint32_t,
 std::vector<double> &, std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptive2DBins
(const array_t<double> &, const array_t<float> &, uint32_t, uint32_t,
 std::vector<double> &, std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptive2DBins
(const array_t<double> &, const array_t<double> &, uint32_t, uint32_t,
 std::vector<double> &, std::vector<double> &, std::vector<uint32_t> &);
