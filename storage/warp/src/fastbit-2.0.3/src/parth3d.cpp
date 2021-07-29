// File $Id$
// Author: John Wu <John.Wu at ACM.org> Lawrence Berkeley National Laboratory
// Copyright (c) 2000-2016 the Regents of the University of California
//
// Implements ibis::part 3D histogram functions.
#include "index.h"      // ibis::index::divideCounts
#include "countQuery.h" // ibis::countQuery
#include "part.h"

#include <math.h>       // ceil, floor, log, ...
#include <limits>       // std::numeric_limits
#include <typeinfo>     // typeid
#include <iomanip>      // setw, setprecision

// This file definte does not use the min and max macro.  Their presence
// could cause the calls to numeric_limits::min and numeric_limits::max to
// be misunderstood!
#undef max
#undef min

template <typename T1, typename T2, typename T3>
long ibis::part::count3DBins(const array_t<T1> &vals1,
                             const double &begin1, const double &end1,
                             const double &stride1,
                             const array_t<T2> &vals2,
                             const double &begin2, const double &end2,
                             const double &stride2,
                             const array_t<T3> &vals3,
                             const double &begin3, const double &end3,
                             const double &stride3,
                             std::vector<uint32_t> &counts) const {
    LOGGER(ibis::gVerbose > 5)
        << "part::count3DBins<" << typeid(T1).name() << ", "
        << typeid(T2).name() << ", " << typeid(T3).name() << ">("
        << "vals1[" << vals1.size() << "], " << begin1 << ", "
        << end1 << ", " << stride1
        << ", vals2[" << vals2.size() << "], " << begin2 << ", "
        << end2 << ", " << stride2
        << ", vals3[" << vals3.size() << "], " << begin3 << ", "
        << end3 << ", " << stride3 << ", counts[" << counts.size()
        << "]) ... ("
        << 1 + static_cast<uint32_t>(floor((end1-begin1)/stride1))
        << ", "
        << 1 + static_cast<uint32_t>(floor((end2-begin2)/stride2))
        << ", "
        << 1 + static_cast<uint32_t>(floor((end3-begin3)/stride3))
        << ")";
    const uint32_t dim3 = 1 +
        static_cast<uint32_t>(floor((end3 - begin3)/stride3));
    const uint32_t dim2 = 1 +
        static_cast<uint32_t>(floor((end2 - begin2)/stride2));
    const uint32_t nr = (vals1.size() <= vals2.size() ?
                       (vals1.size() <= vals3.size() ?
                        vals1.size() : vals3.size()) :
                       (vals2.size() <= vals3.size() ?
                        vals2.size() : vals3.size()));
    for (uint32_t ir = 0; ir < nr; ++ ir) {
        const uint32_t pos =
            (static_cast<uint32_t>((vals1[ir]-begin1)/stride1) * dim2 +
             static_cast<uint32_t>((vals2[ir]-begin2)/stride2)) * dim3 +
            static_cast<uint32_t>((vals3[ir]-begin3)/stride3);
        ++ counts[pos];
#if (defined(_DEBUG) && _DEBUG+0 > 1) || (defined(DEBUG) && DEBUG+0 > 1)
        LOGGER(ibis::gVerbose > 5)
            << "DEBUG -- count3DBins -- vals1[" << ir << "]=" << vals1[ir]
            << ", vals2[" << ir << "]=" << vals2[ir]
            << ", vals3[" << ir << "]=" << vals3[ir]
            << " --> bin (" << static_cast<uint32_t>((vals1[ir]-begin1)/stride1)
            << ", " << static_cast<uint32_t>((vals2[ir]-begin2)/stride2)
            << ", " << static_cast<uint32_t>((vals3[ir]-begin3)/stride3)
            << ") counts[" << pos << "]=" << counts[pos];
#endif
    }
    return counts.size();
} // ibis::part::count3DBins

/// This function defines exactly @code
/// (1 + floor((end1-begin1)/stride1)) *
/// (1 + floor((end2-begin2)/stride2)) *
/// (1 + floor((end3-begin3)/stride3))
/// @endcode regularly spaced bins.
/// On successful completion of this function, the return value shall be
/// the number of bins.  Any other value indicates an error.
///
/// @note This function is intended to work with numerical values.  It
/// treats categorical values as unsigned ints.  Passing the name of text
/// column to this function will result in a negative return value.
///
/// @sa ibis::part::get1DDistribution
/// @sa ibis::table::getHistogram2D
long ibis::part::get3DDistribution(const char *constraints, const char *cname1,
                                   double begin1, double end1, double stride1,
                                   const char *cname2,
                                   double begin2, double end2, double stride2,
                                   const char *cname3,
                                   double begin3, double end3, double stride3,
                                   std::vector<uint32_t> &counts) const {
    if (cname1 == 0 || *cname1 == 0 || (begin1 >= end1 && !(stride1 < 0.0)) ||
        (begin1 <= end1 && !(stride1 > 0.0)) ||
        cname2 == 0 || *cname2 == 0 || (begin2 >= end2 && !(stride2 < 0.0)) ||
        (begin2 <= end2 && !(stride2 > 0.0)) ||
        cname3 == 0 || *cname3 == 0 || (begin3 >= end3 && !(stride3 < 0.0)) ||
        (begin3 <= end3 && !(stride3 > 0.0)))
        return -1L;

    const ibis::column* col1 = getColumn(cname1);
    const ibis::column* col2 = getColumn(cname2);
    const ibis::column* col3 = getColumn(cname3);
    if (col1 == 0 || col2 == 0 || col3 == 0)
        return -2L;

    ibis::horometer timer;
    if (ibis::gVerbose > 0) {
        LOGGER(ibis::gVerbose > 2)
            << "part[" << (m_name ? m_name : "")
            << "]::get3DDistribution attempting to compute a histogram of "
            << cname1 << ", " << cname2 << ", and " << cname3
            << " with regular binning "
            << (constraints && *constraints ? "subject to " :
                "without constraints")
            << (constraints ? constraints : "");
        timer.start();
    }
    const uint32_t nbins =
        (1 + static_cast<uint32_t>(floor((end1 - begin1) / stride1))) *
        (1 + static_cast<uint32_t>(floor((end2 - begin2) / stride2))) *
        (1 + static_cast<uint32_t>(floor((end3 - begin3) / stride3)));
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
            << " and " << std::setprecision(18) << end1
            << " AND " << cname2 << " between " << std::setprecision(18)
            << begin2 << " and " << std::setprecision(18) << end2
            << " AND " << cname3 << " between " << std::setprecision(18)
            << begin3 << " and " << std::setprecision(18) << end3;
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

            switch (col3->type()) {
            case ibis::BYTE:
            case ibis::SHORT:
            case ibis::INT: {
                array_t<int32_t>* vals3 =
                    col3->selectInts(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            case ibis::UBYTE:
            case ibis::USHORT:
            case ibis::CATEGORY:
            case ibis::UINT: {
                array_t<uint32_t>* vals3 =
                    col3->selectUInts(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            case ibis::ULONG:
            case ibis::LONG: {
                array_t<int64_t>* vals3 =
                    col3->selectLongs(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            case ibis::FLOAT: {
                array_t<float>* vals3 =
                    col3->selectFloats(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            case ibis::DOUBLE: {
                array_t<double>* vals3 =
                    col3->selectDoubles(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            default: {
                LOGGER(ibis::gVerbose > 3)
                    << "part::get3DDistribution -- can not "
                    "handle column (" << cname3 << ") type "
                    << ibis::TYPESTRING[(int)col3->type()];

                ierr = -3;
                break;}
            }
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

            switch (col3->type()) {
            case ibis::BYTE:
            case ibis::SHORT:
            case ibis::INT: {
                array_t<int32_t>* vals3 =
                    col3->selectInts(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            case ibis::UBYTE:
            case ibis::USHORT:
            case ibis::CATEGORY:
            case ibis::UINT: {
                array_t<uint32_t>* vals3 =
                    col3->selectUInts(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            case ibis::ULONG:
            case ibis::LONG: {
                array_t<int64_t>* vals3 =
                    col3->selectLongs(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            case ibis::FLOAT: {
                array_t<float>* vals3 =
                    col3->selectFloats(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            case ibis::DOUBLE: {
                array_t<double>* vals3 =
                    col3->selectDoubles(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            default: {
                LOGGER(ibis::gVerbose > 3)
                    << "part::get3DDistribution -- can not "
                    "handle column (" << cname3 << ") type "
                    << ibis::TYPESTRING[(int)col3->type()];

                ierr = -3;
                break;}
            }
            delete vals2;
            break;}
        case ibis::ULONG:
        case ibis::LONG: {
            array_t<int64_t>* vals2 = col2->selectLongs(hits);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }

            switch (col3->type()) {
            case ibis::BYTE:
            case ibis::SHORT:
            case ibis::INT: {
                array_t<int32_t>* vals3 =
                    col3->selectInts(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            case ibis::UBYTE:
            case ibis::USHORT:
            case ibis::CATEGORY:
            case ibis::UINT: {
                array_t<uint32_t>* vals3 =
                    col3->selectUInts(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            case ibis::ULONG:
            case ibis::LONG: {
                array_t<int64_t>* vals3 =
                    col3->selectLongs(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            case ibis::FLOAT: {
                array_t<float>* vals3 =
                    col3->selectFloats(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            case ibis::DOUBLE: {
                array_t<double>* vals3 =
                    col3->selectDoubles(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            default: {
                LOGGER(ibis::gVerbose > 3)
                    << "part::get3DDistribution -- can not "
                    "handle column (" << cname3 << ") type "
                    << ibis::TYPESTRING[(int)col3->type()];

                ierr = -3;
                break;}
            }
            delete vals2;
            break;}
        case ibis::FLOAT: {
            array_t<float>* vals2 = col2->selectFloats(hits);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }

            switch (col3->type()) {
            case ibis::BYTE:
            case ibis::SHORT:
            case ibis::INT: {
                array_t<int32_t>* vals3 =
                    col3->selectInts(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            case ibis::UBYTE:
            case ibis::USHORT:
            case ibis::CATEGORY:
            case ibis::UINT: {
                array_t<uint32_t>* vals3 =
                    col3->selectUInts(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            case ibis::ULONG:
            case ibis::LONG: {
                array_t<int64_t>* vals3 =
                    col3->selectLongs(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            case ibis::FLOAT: {
                array_t<float>* vals3 =
                    col3->selectFloats(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            case ibis::DOUBLE: {
                array_t<double>* vals3 =
                    col3->selectDoubles(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            default: {
                LOGGER(ibis::gVerbose > 3)
                    << "part::get3DDistribution -- can not "
                    "handle column (" << cname3 << ") type "
                    << ibis::TYPESTRING[(int)col3->type()];

                ierr = -3;
                break;}
            }
            delete vals2;
            break;}
        case ibis::DOUBLE: {
            array_t<double>* vals2 = col2->selectDoubles(hits);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }

            switch (col3->type()) {
            case ibis::BYTE:
            case ibis::SHORT:
            case ibis::INT: {
                array_t<int32_t>* vals3 =
                    col3->selectInts(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            case ibis::UBYTE:
            case ibis::USHORT:
            case ibis::CATEGORY:
            case ibis::UINT: {
                array_t<uint32_t>* vals3 =
                    col3->selectUInts(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            case ibis::ULONG:
            case ibis::LONG: {
                array_t<int64_t>* vals3 =
                    col3->selectLongs(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            case ibis::FLOAT: {
                array_t<float>* vals3 =
                    col3->selectFloats(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            case ibis::DOUBLE: {
                array_t<double>* vals3 =
                    col3->selectDoubles(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            default: {
                LOGGER(ibis::gVerbose > 3)
                    << "part::get3DDistribution -- can not "
                    "handle column (" << cname3 << ") type "
                    << ibis::TYPESTRING[(int)col3->type()];

                ierr = -3;
                break;}
            }
            delete vals2;
            break;}
        default: {
            LOGGER(ibis::gVerbose > 3)
                << "part::get3DDistribution -- can not "
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

            switch (col3->type()) {
            case ibis::BYTE:
            case ibis::SHORT:
            case ibis::INT: {
                array_t<int32_t>* vals3 =
                    col3->selectInts(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            case ibis::UBYTE:
            case ibis::USHORT:
            case ibis::CATEGORY:
            case ibis::UINT: {
                array_t<uint32_t>* vals3 =
                    col3->selectUInts(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            case ibis::ULONG:
            case ibis::LONG: {
                array_t<int64_t>* vals3 =
                    col3->selectLongs(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            case ibis::FLOAT: {
                array_t<float>* vals3 =
                    col3->selectFloats(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            case ibis::DOUBLE: {
                array_t<double>* vals3 =
                    col3->selectDoubles(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            default: {
                LOGGER(ibis::gVerbose > 3)
                    << "part::get3DDistribution -- can not "
                    "handle column (" << cname3 << ") type "
                    << ibis::TYPESTRING[(int)col3->type()];

                ierr = -3;
                break;}
            }
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

            switch (col3->type()) {
            case ibis::BYTE:
            case ibis::SHORT:
            case ibis::INT: {
                array_t<int32_t>* vals3 =
                    col3->selectInts(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            case ibis::UBYTE:
            case ibis::USHORT:
            case ibis::CATEGORY:
            case ibis::UINT: {
                array_t<uint32_t>* vals3 =
                    col3->selectUInts(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            case ibis::ULONG:
            case ibis::LONG: {
                array_t<int64_t>* vals3 =
                    col3->selectLongs(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            case ibis::FLOAT: {
                array_t<float>* vals3 =
                    col3->selectFloats(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            case ibis::DOUBLE: {
                array_t<double>* vals3 =
                    col3->selectDoubles(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            default: {
                LOGGER(ibis::gVerbose > 3)
                    << "part::get3DDistribution -- can not "
                    "handle column (" << cname3 << ") type "
                    << ibis::TYPESTRING[(int)col3->type()];

                ierr = -3;
                break;}
            }
            delete vals2;
            break;}
        case ibis::ULONG:
        case ibis::LONG: {
            array_t<int64_t>* vals2 = col2->selectLongs(hits);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }

            switch (col3->type()) {
            case ibis::BYTE:
            case ibis::SHORT:
            case ibis::INT: {
                array_t<int32_t>* vals3 =
                    col3->selectInts(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            case ibis::UBYTE:
            case ibis::USHORT:
            case ibis::CATEGORY:
            case ibis::UINT: {
                array_t<uint32_t>* vals3 =
                    col3->selectUInts(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            case ibis::ULONG:
            case ibis::LONG: {
                array_t<int64_t>* vals3 =
                    col3->selectLongs(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            case ibis::FLOAT: {
                array_t<float>* vals3 =
                    col3->selectFloats(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            case ibis::DOUBLE: {
                array_t<double>* vals3 =
                    col3->selectDoubles(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            default: {
                LOGGER(ibis::gVerbose > 3)
                    << "part::get3DDistribution -- can not "
                    "handle column (" << cname3 << ") type "
                    << ibis::TYPESTRING[(int)col3->type()];

                ierr = -3;
                break;}
            }
            delete vals2;
            break;}
        case ibis::FLOAT: {
            array_t<float>* vals2 = col2->selectFloats(hits);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }

            switch (col3->type()) {
            case ibis::BYTE:
            case ibis::SHORT:
            case ibis::INT: {
                array_t<int32_t>* vals3 =
                    col3->selectInts(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            case ibis::UBYTE:
            case ibis::USHORT:
            case ibis::CATEGORY:
            case ibis::UINT: {
                array_t<uint32_t>* vals3 =
                    col3->selectUInts(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            case ibis::ULONG:
            case ibis::LONG: {
                array_t<int64_t>* vals3 =
                    col3->selectLongs(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            case ibis::FLOAT: {
                array_t<float>* vals3 =
                    col3->selectFloats(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            case ibis::DOUBLE: {
                array_t<double>* vals3 =
                    col3->selectDoubles(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            default: {
                LOGGER(ibis::gVerbose > 3)
                    << "part::get3DDistribution -- can not "
                    "handle column (" << cname3 << ") type "
                    << ibis::TYPESTRING[(int)col3->type()];

                ierr = -3;
                break;}
            }
            delete vals2;
            break;}
        case ibis::DOUBLE: {
            array_t<double>* vals2 = col2->selectDoubles(hits);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }

            switch (col3->type()) {
            case ibis::BYTE:
            case ibis::SHORT:
            case ibis::INT: {
                array_t<int32_t>* vals3 =
                    col3->selectInts(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            case ibis::UBYTE:
            case ibis::USHORT:
            case ibis::CATEGORY:
            case ibis::UINT: {
                array_t<uint32_t>* vals3 =
                    col3->selectUInts(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            case ibis::ULONG:
            case ibis::LONG: {
                array_t<int64_t>* vals3 =
                    col3->selectLongs(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            case ibis::FLOAT: {
                array_t<float>* vals3 =
                    col3->selectFloats(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            case ibis::DOUBLE: {
                array_t<double>* vals3 =
                    col3->selectDoubles(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            default: {
                LOGGER(ibis::gVerbose > 3)
                    << "part::get3DDistribution -- can not "
                    "handle column (" << cname3 << ") type "
                    << ibis::TYPESTRING[(int)col3->type()];

                ierr = -3;
                break;}
            }
            delete vals2;
            break;}
        default: {
            LOGGER(ibis::gVerbose > 3)
                << "part::get3DDistribution -- can not "
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

            switch (col3->type()) {
            case ibis::BYTE:
            case ibis::SHORT:
            case ibis::INT: {
                array_t<int32_t>* vals3 =
                    col3->selectInts(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            case ibis::UBYTE:
            case ibis::USHORT:
            case ibis::CATEGORY:
            case ibis::UINT: {
                array_t<uint32_t>* vals3 =
                    col3->selectUInts(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            case ibis::ULONG:
            case ibis::LONG: {
                array_t<int64_t>* vals3 =
                    col3->selectLongs(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            case ibis::FLOAT: {
                array_t<float>* vals3 =
                    col3->selectFloats(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            case ibis::DOUBLE: {
                array_t<double>* vals3 =
                    col3->selectDoubles(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            default: {
                LOGGER(ibis::gVerbose > 3)
                    << "part::get3DDistribution -- can not "
                    "handle column (" << cname3 << ") type "
                    << ibis::TYPESTRING[(int)col3->type()];

                ierr = -3;
                break;}
            }
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

            switch (col3->type()) {
            case ibis::BYTE:
            case ibis::SHORT:
            case ibis::INT: {
                array_t<int32_t>* vals3 =
                    col3->selectInts(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            case ibis::UBYTE:
            case ibis::USHORT:
            case ibis::CATEGORY:
            case ibis::UINT: {
                array_t<uint32_t>* vals3 =
                    col3->selectUInts(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            case ibis::ULONG:
            case ibis::LONG: {
                array_t<int64_t>* vals3 =
                    col3->selectLongs(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            case ibis::FLOAT: {
                array_t<float>* vals3 =
                    col3->selectFloats(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            case ibis::DOUBLE: {
                array_t<double>* vals3 =
                    col3->selectDoubles(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            default: {
                LOGGER(ibis::gVerbose > 3)
                    << "part::get3DDistribution -- can not "
                    "handle column (" << cname3 << ") type "
                    << ibis::TYPESTRING[(int)col3->type()];

                ierr = -3;
                break;}
            }
            delete vals2;
            break;}
        case ibis::ULONG:
        case ibis::LONG: {
            array_t<int64_t>* vals2 = col2->selectLongs(hits);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }

            switch (col3->type()) {
            case ibis::BYTE:
            case ibis::SHORT:
            case ibis::INT: {
                array_t<int32_t>* vals3 =
                    col3->selectInts(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            case ibis::UBYTE:
            case ibis::USHORT:
            case ibis::CATEGORY:
            case ibis::UINT: {
                array_t<uint32_t>* vals3 =
                    col3->selectUInts(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            case ibis::ULONG:
            case ibis::LONG: {
                array_t<int64_t>* vals3 =
                    col3->selectLongs(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            case ibis::FLOAT: {
                array_t<float>* vals3 =
                    col3->selectFloats(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            case ibis::DOUBLE: {
                array_t<double>* vals3 =
                    col3->selectDoubles(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            default: {
                LOGGER(ibis::gVerbose > 3)
                    << "part::get3DDistribution -- can not "
                    "handle column (" << cname3 << ") type "
                    << ibis::TYPESTRING[(int)col3->type()];

                ierr = -3;
                break;}
            }
            delete vals2;
            break;}
        case ibis::FLOAT: {
            array_t<float>* vals2 = col2->selectFloats(hits);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }

            switch (col3->type()) {
            case ibis::BYTE:
            case ibis::SHORT:
            case ibis::INT: {
                array_t<int32_t>* vals3 =
                    col3->selectInts(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            case ibis::UBYTE:
            case ibis::USHORT:
            case ibis::CATEGORY:
            case ibis::UINT: {
                array_t<uint32_t>* vals3 =
                    col3->selectUInts(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            case ibis::ULONG:
            case ibis::LONG: {
                array_t<int64_t>* vals3 =
                    col3->selectLongs(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            case ibis::FLOAT: {
                array_t<float>* vals3 =
                    col3->selectFloats(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            case ibis::DOUBLE: {
                array_t<double>* vals3 =
                    col3->selectDoubles(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            default: {
                LOGGER(ibis::gVerbose > 3)
                    << "part::get3DDistribution -- can not "
                    "handle column (" << cname3 << ") type "
                    << ibis::TYPESTRING[(int)col3->type()];

                ierr = -3;
                break;}
            }
            delete vals2;
            break;}
        case ibis::DOUBLE: {
            array_t<double>* vals2 = col2->selectDoubles(hits);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }

            switch (col3->type()) {
            case ibis::BYTE:
            case ibis::SHORT:
            case ibis::INT: {
                array_t<int32_t>* vals3 =
                    col3->selectInts(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            case ibis::UBYTE:
            case ibis::USHORT:
            case ibis::CATEGORY:
            case ibis::UINT: {
                array_t<uint32_t>* vals3 =
                    col3->selectUInts(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            case ibis::ULONG:
            case ibis::LONG: {
                array_t<int64_t>* vals3 =
                    col3->selectLongs(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            case ibis::FLOAT: {
                array_t<float>* vals3 =
                    col3->selectFloats(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            case ibis::DOUBLE: {
                array_t<double>* vals3 =
                    col3->selectDoubles(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            default: {
                LOGGER(ibis::gVerbose > 3)
                    << "part::get3DDistribution -- can not "
                    "handle column (" << cname3 << ") type "
                    << ibis::TYPESTRING[(int)col3->type()];

                ierr = -3;
                break;}
            }
            delete vals2;
            break;}
        default: {
            LOGGER(ibis::gVerbose > 3)
                << "part::get3DDistribution -- can not "
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

            switch (col3->type()) {
            case ibis::BYTE:
            case ibis::SHORT:
            case ibis::INT: {
                array_t<int32_t>* vals3 =
                    col3->selectInts(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            case ibis::UBYTE:
            case ibis::USHORT:
            case ibis::CATEGORY:
            case ibis::UINT: {
                array_t<uint32_t>* vals3 =
                    col3->selectUInts(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            case ibis::ULONG:
            case ibis::LONG: {
                array_t<int64_t>* vals3 =
                    col3->selectLongs(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            case ibis::FLOAT: {
                array_t<float>* vals3 =
                    col3->selectFloats(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            case ibis::DOUBLE: {
                array_t<double>* vals3 =
                    col3->selectDoubles(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            default: {
                LOGGER(ibis::gVerbose > 3)
                    << "part::get3DDistribution -- can not "
                    "handle column (" << cname3 << ") type "
                    << ibis::TYPESTRING[(int)col3->type()];

                ierr = -3;
                break;}
            }
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

            switch (col3->type()) {
            case ibis::BYTE:
            case ibis::SHORT:
            case ibis::INT: {
                array_t<int32_t>* vals3 =
                    col3->selectInts(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            case ibis::UBYTE:
            case ibis::USHORT:
            case ibis::CATEGORY:
            case ibis::UINT: {
                array_t<uint32_t>* vals3 =
                    col3->selectUInts(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            case ibis::ULONG:
            case ibis::LONG: {
                array_t<int64_t>* vals3 =
                    col3->selectLongs(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            case ibis::FLOAT: {
                array_t<float>* vals3 =
                    col3->selectFloats(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            case ibis::DOUBLE: {
                array_t<double>* vals3 =
                    col3->selectDoubles(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            default: {
                LOGGER(ibis::gVerbose > 3)
                    << "part::get3DDistribution -- can not "
                    "handle column (" << cname3 << ") type "
                    << ibis::TYPESTRING[(int)col3->type()];

                ierr = -3;
                break;}
            }
            delete vals2;
            break;}
        case ibis::ULONG:
        case ibis::LONG: {
            array_t<int64_t>* vals2 = col2->selectLongs(hits);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }

            switch (col3->type()) {
            case ibis::BYTE:
            case ibis::SHORT:
            case ibis::INT: {
                array_t<int32_t>* vals3 =
                    col3->selectInts(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            case ibis::UBYTE:
            case ibis::USHORT:
            case ibis::CATEGORY:
            case ibis::UINT: {
                array_t<uint32_t>* vals3 =
                    col3->selectUInts(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            case ibis::ULONG:
            case ibis::LONG: {
                array_t<int64_t>* vals3 =
                    col3->selectLongs(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            case ibis::FLOAT: {
                array_t<float>* vals3 =
                    col3->selectFloats(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            case ibis::DOUBLE: {
                array_t<double>* vals3 =
                    col3->selectDoubles(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            default: {
                LOGGER(ibis::gVerbose > 3)
                    << "part::get3DDistribution -- can not "
                    "handle column (" << cname3 << ") type "
                    << ibis::TYPESTRING[(int)col3->type()];

                ierr = -3;
                break;}
            }
            delete vals2;
            break;}
        case ibis::FLOAT: {
            array_t<float>* vals2 = col2->selectFloats(hits);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }

            switch (col3->type()) {
            case ibis::BYTE:
            case ibis::SHORT:
            case ibis::INT: {
                array_t<int32_t>* vals3 =
                    col3->selectInts(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            case ibis::UBYTE:
            case ibis::USHORT:
            case ibis::CATEGORY:
            case ibis::UINT: {
                array_t<uint32_t>* vals3 =
                    col3->selectUInts(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            case ibis::ULONG:
            case ibis::LONG: {
                array_t<int64_t>* vals3 =
                    col3->selectLongs(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            case ibis::FLOAT: {
                array_t<float>* vals3 =
                    col3->selectFloats(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            case ibis::DOUBLE: {
                array_t<double>* vals3 =
                    col3->selectDoubles(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            default: {
                LOGGER(ibis::gVerbose > 3)
                    << "part::get3DDistribution -- can not "
                    "handle column (" << cname3 << ") type "
                    << ibis::TYPESTRING[(int)col3->type()];

                ierr = -3;
                break;}
            }
            delete vals2;
            break;}
        case ibis::DOUBLE: {
            array_t<double>* vals2 = col2->selectDoubles(hits);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }

            switch (col3->type()) {
            case ibis::BYTE:
            case ibis::SHORT:
            case ibis::INT: {
                array_t<int32_t>* vals3 =
                    col3->selectInts(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            case ibis::UBYTE:
            case ibis::USHORT:
            case ibis::CATEGORY:
            case ibis::UINT: {
                array_t<uint32_t>* vals3 =
                    col3->selectUInts(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            case ibis::ULONG:
            case ibis::LONG: {
                array_t<int64_t>* vals3 =
                    col3->selectLongs(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            case ibis::FLOAT: {
                array_t<float>* vals3 =
                    col3->selectFloats(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            case ibis::DOUBLE: {
                array_t<double>* vals3 =
                    col3->selectDoubles(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            default: {
                LOGGER(ibis::gVerbose > 3)
                    << "part::get3DDistribution -- can not "
                    "handle column (" << cname3 << ") type "
                    << ibis::TYPESTRING[(int)col3->type()];

                ierr = -3;
                break;}
            }
            delete vals2;
            break;}
        default: {
            LOGGER(ibis::gVerbose > 3)
                << "part::get3DDistribution -- can not "
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

            switch (col3->type()) {
            case ibis::BYTE:
            case ibis::SHORT:
            case ibis::INT: {
                array_t<int32_t>* vals3 =
                    col3->selectInts(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            case ibis::UBYTE:
            case ibis::USHORT:
            case ibis::CATEGORY:
            case ibis::UINT: {
                array_t<uint32_t>* vals3 =
                    col3->selectUInts(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            case ibis::ULONG:
            case ibis::LONG: {
                array_t<int64_t>* vals3 =
                    col3->selectLongs(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            case ibis::FLOAT: {
                array_t<float>* vals3 =
                    col3->selectFloats(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            case ibis::DOUBLE: {
                array_t<double>* vals3 =
                    col3->selectDoubles(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            default: {
                LOGGER(ibis::gVerbose > 3)
                    << "part::get3DDistribution -- can not "
                    "handle column (" << cname3 << ") type "
                    << ibis::TYPESTRING[(int)col3->type()];

                ierr = -3;
                break;}
            }
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

            switch (col3->type()) {
            case ibis::BYTE:
            case ibis::SHORT:
            case ibis::INT: {
                array_t<int32_t>* vals3 =
                    col3->selectInts(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            case ibis::UBYTE:
            case ibis::USHORT:
            case ibis::CATEGORY:
            case ibis::UINT: {
                array_t<uint32_t>* vals3 =
                    col3->selectUInts(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            case ibis::ULONG:
            case ibis::LONG: {
                array_t<int64_t>* vals3 =
                    col3->selectLongs(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            case ibis::FLOAT: {
                array_t<float>* vals3 =
                    col3->selectFloats(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            case ibis::DOUBLE: {
                array_t<double>* vals3 =
                    col3->selectDoubles(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            default: {
                LOGGER(ibis::gVerbose > 3)
                    << "part::get3DDistribution -- can not "
                    "handle column (" << cname3 << ") type "
                    << ibis::TYPESTRING[(int)col3->type()];

                ierr = -3;
                break;}
            }
            delete vals2;
            break;}
        case ibis::ULONG:
        case ibis::LONG: {
            array_t<int64_t>* vals2 = col2->selectLongs(hits);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }

            switch (col3->type()) {
            case ibis::BYTE:
            case ibis::SHORT:
            case ibis::INT: {
                array_t<int32_t>* vals3 =
                    col3->selectInts(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            case ibis::UBYTE:
            case ibis::USHORT:
            case ibis::CATEGORY:
            case ibis::UINT: {
                array_t<uint32_t>* vals3 =
                    col3->selectUInts(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            case ibis::ULONG:
            case ibis::LONG: {
                array_t<int64_t>* vals3 =
                    col3->selectLongs(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            case ibis::FLOAT: {
                array_t<float>* vals3 =
                    col3->selectFloats(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            case ibis::DOUBLE: {
                array_t<double>* vals3 =
                    col3->selectDoubles(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            default: {
                LOGGER(ibis::gVerbose > 3)
                    << "part::get3DDistribution -- can not "
                    "handle column (" << cname3 << ") type "
                    << ibis::TYPESTRING[(int)col3->type()];

                ierr = -3;
                break;}
            }
            delete vals2;
            break;}
        case ibis::FLOAT: {
            array_t<float>* vals2 = col2->selectFloats(hits);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }

            switch (col3->type()) {
            case ibis::BYTE:
            case ibis::SHORT:
            case ibis::INT: {
                array_t<int32_t>* vals3 =
                    col3->selectInts(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            case ibis::UBYTE:
            case ibis::USHORT:
            case ibis::CATEGORY:
            case ibis::UINT: {
                array_t<uint32_t>* vals3 =
                    col3->selectUInts(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            case ibis::ULONG:
            case ibis::LONG: {
                array_t<int64_t>* vals3 =
                    col3->selectLongs(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            case ibis::FLOAT: {
                array_t<float>* vals3 =
                    col3->selectFloats(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            case ibis::DOUBLE: {
                array_t<double>* vals3 =
                    col3->selectDoubles(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            default: {
                LOGGER(ibis::gVerbose > 3)
                    << "part::get3DDistribution -- can not "
                    "handle column (" << cname3 << ") type "
                    << ibis::TYPESTRING[(int)col3->type()];

                ierr = -3;
                break;}
            }
            delete vals2;
            break;}
        case ibis::DOUBLE: {
            array_t<double>* vals2 = col2->selectDoubles(hits);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }

            switch (col3->type()) {
            case ibis::BYTE:
            case ibis::SHORT:
            case ibis::INT: {
                array_t<int32_t>* vals3 =
                    col3->selectInts(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            case ibis::UBYTE:
            case ibis::USHORT:
            case ibis::CATEGORY:
            case ibis::UINT: {
                array_t<uint32_t>* vals3 =
                    col3->selectUInts(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            case ibis::ULONG:
            case ibis::LONG: {
                array_t<int64_t>* vals3 =
                    col3->selectLongs(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            case ibis::FLOAT: {
                array_t<float>* vals3 =
                    col3->selectFloats(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            case ibis::DOUBLE: {
                array_t<double>* vals3 =
                    col3->selectDoubles(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DBins(*vals1, begin1, end1, stride1,
                                   *vals2, begin2, end2, stride2,
                                   *vals3, begin3, end3, stride3, counts);
                delete vals3;
                break;}
            default: {
                LOGGER(ibis::gVerbose > 3)
                    << "part::get3DDistribution -- can not "
                    "handle column (" << cname3 << ") type "
                    << ibis::TYPESTRING[(int)col3->type()];

                ierr = -3;
                break;}
            }
            delete vals2;
            break;}
        default: {
            LOGGER(ibis::gVerbose > 3)
                << "part::get3DDistribution -- can not "
                "handle column (" << cname2 << ") type "
                << ibis::TYPESTRING[(int)col2->type()];

            ierr = -3;
            break;}
        }
        delete vals1;
        break;}
    default: {
        LOGGER(ibis::gVerbose > 3)
            << "part::get3DDistribution -- can not "
            "handle column (" << cname1 << ") type "
            << ibis::TYPESTRING[(int)col1->type()];

        ierr = -3;
        break;}
    }
    if (ierr > 0 && ibis::gVerbose > 0) {
        timer.stop();
        logMessage("get3DDistribution", "computing the joint distribution of "
                   "columns %s, %s, and %s%s%s took %g sec(CPU), %g "
                   "sec(elapsed)", cname1, cname2, cname2,
                   (constraints ? " with restriction " : ""),
                   (constraints ? constraints : ""),
                   timer.CPUTime(), timer.realTime());
    }
    return ierr;
} // ibis::part::get3DDistribution

template <typename T1, typename T2, typename T3>
long ibis::part::count3DWeights(const array_t<T1> &vals1,
                                const double &begin1, const double &end1,
                                const double &stride1,
                                const array_t<T2> &vals2,
                                const double &begin2, const double &end2,
                                const double &stride2,
                                const array_t<T3> &vals3,
                                const double &begin3, const double &end3,
                                const double &stride3,
                                const array_t<double> &wts,
                                std::vector<double> &weights) const {
    LOGGER(ibis::gVerbose > 5)
        << "part::count3DBins<" << typeid(T1).name() << ", "
        << typeid(T2).name() << ", " << typeid(T3).name() << ">("
        << "vals1[" << vals1.size() << "], " << begin1 << ", "
        << end1 << ", " << stride1
        << ", vals2[" << vals2.size() << "], " << begin2 << ", "
        << end2 << ", " << stride2
        << ", vals3[" << vals3.size() << "], " << begin3 << ", "
        << end3 << ", " << stride3 << ", weights[" << weights.size()
        << "]) ... ("
        << 1 + static_cast<uint32_t>(floor((end1-begin1)/stride1))
        << ", "
        << 1 + static_cast<uint32_t>(floor((end2-begin2)/stride2))
        << ", "
        << 1 + static_cast<uint32_t>(floor((end3-begin3)/stride3))
        << ")";
    const uint32_t dim3 = 1 +
        static_cast<uint32_t>(floor((end3 - begin3)/stride3));
    const uint32_t dim2 = 1 +
        static_cast<uint32_t>(floor((end2 - begin2)/stride2));
    const uint32_t nr = (vals1.size() <= vals2.size() ?
                       (vals1.size() <= vals3.size() ?
                        vals1.size() : vals3.size()) :
                       (vals2.size() <= vals3.size() ?
                        vals2.size() : vals3.size()));
    for (uint32_t ir = 0; ir < nr; ++ ir) {
        const uint32_t pos =
            (static_cast<uint32_t>((vals1[ir]-begin1)/stride1) * dim2 +
             static_cast<uint32_t>((vals2[ir]-begin2)/stride2)) * dim3 +
            static_cast<uint32_t>((vals3[ir]-begin3)/stride3);
        weights[pos] += wts[ir];
#if (defined(_DEBUG) && _DEBUG+0 > 1) || (defined(DEBUG) && DEBUG+0 > 1)
        LOGGER(ibis::gVerbose > 5)
            << "DEBUG -- count3DBins -- vals1[" << ir << "]=" << vals1[ir]
            << ", vals2[" << ir << "]=" << vals2[ir]
            << ", vals3[" << ir << "]=" << vals3[ir]
            << " --> bin (" << static_cast<uint32_t>((vals1[ir]-begin1)/stride1)
            << ", " << static_cast<uint32_t>((vals2[ir]-begin2)/stride2)
            << ", " << static_cast<uint32_t>((vals3[ir]-begin3)/stride3)
            << ") wts[" << ir << "]=" << wts[ir]
            << ", weights[" << pos << "]=" << weights[pos];
#endif
    }
    return weights.size();
} // ibis::part::count3DWeights

/// This function defines exactly @code
/// (1 + floor((end1-begin1)/stride1)) *
/// (1 + floor((end2-begin2)/stride2)) *
/// (1 + floor((end3-begin3)/stride3))
/// @endcode regularly spaced bins.
/// On successful completion of this function, the return value shall be
/// the number of bins.  Any other value indicates an error.
///
/// @sa ibis::part::get1DDistribution
/// @sa ibis::table::getHistogram2D
long ibis::part::get3DDistribution(const char *constraints, const char *cname1,
                                   double begin1, double end1, double stride1,
                                   const char *cname2,
                                   double begin2, double end2, double stride2,
                                   const char *cname3,
                                   double begin3, double end3, double stride3,
                                   const char *wtname,
                                   std::vector<double> &weights) const {
    if (wtname == 0 || *wtname == 0 ||
        cname1 == 0 || *cname1 == 0 || (begin1 >= end1 && !(stride1 < 0.0)) ||
        (begin1 <= end1 && !(stride1 > 0.0)) ||
        cname2 == 0 || *cname2 == 0 || (begin2 >= end2 && !(stride2 < 0.0)) ||
        (begin2 <= end2 && !(stride2 > 0.0)) ||
        cname3 == 0 || *cname3 == 0 || (begin3 >= end3 && !(stride3 < 0.0)) ||
        (begin3 <= end3 && !(stride3 > 0.0)))
        return -1L;

    const ibis::column* col1 = getColumn(cname1);
    const ibis::column* col2 = getColumn(cname2);
    const ibis::column* col3 = getColumn(cname3);
    const ibis::column* wcol = getColumn(wtname);
    if (col1 == 0 || col2 == 0 || col3 == 0 || wcol == 0)
        return -2L;

    ibis::horometer timer;
    if (ibis::gVerbose > 0) {
        LOGGER(ibis::gVerbose > 2)
            << "part[" << (m_name ? m_name : "")
            << "]::get3DDistribution attempting to compute a histogram of "
            << cname1 << ", " << cname2 << ", and " << cname3
            << " with regular binning "
            << (constraints && *constraints ? "subject to " :
                "without constraints")
            << (constraints ? constraints : "") << " weighted with " << wtname;
        timer.start();
    }
    const uint32_t nbins =
        (1 + static_cast<uint32_t>(floor((end1 - begin1) / stride1))) *
        (1 + static_cast<uint32_t>(floor((end2 - begin2) / stride2))) *
        (1 + static_cast<uint32_t>(floor((end3 - begin3) / stride3)));
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
            << " and " << std::setprecision(18) << end1
            << " AND " << cname2 << " between " << std::setprecision(18)
            << begin2 << " and " << std::setprecision(18) << end2
            << " AND " << cname3 << " between " << std::setprecision(18)
            << begin3 << " and " << std::setprecision(18) << end3;
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
            << "]::get3DDistribution failed retrieve values from column "
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

            switch (col3->type()) {
            case ibis::BYTE:
            case ibis::SHORT:
            case ibis::INT: {
                array_t<int32_t>* vals3 =
                    col3->selectInts(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            case ibis::UBYTE:
            case ibis::USHORT:
            case ibis::CATEGORY:
            case ibis::UINT: {
                array_t<uint32_t>* vals3 =
                    col3->selectUInts(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            case ibis::ULONG:
            case ibis::LONG: {
                array_t<int64_t>* vals3 =
                    col3->selectLongs(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            case ibis::FLOAT: {
                array_t<float>* vals3 =
                    col3->selectFloats(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            case ibis::DOUBLE: {
                array_t<double>* vals3 =
                    col3->selectDoubles(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            default: {
                LOGGER(ibis::gVerbose > 3)
                    << "part::get3DDistribution -- can not "
                    "handle column (" << cname3 << ") type "
                    << ibis::TYPESTRING[(int)col3->type()];

                ierr = -3;
                break;}
            }
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

            switch (col3->type()) {
            case ibis::BYTE:
            case ibis::SHORT:
            case ibis::INT: {
                array_t<int32_t>* vals3 =
                    col3->selectInts(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            case ibis::UBYTE:
            case ibis::USHORT:
            case ibis::CATEGORY:
            case ibis::UINT: {
                array_t<uint32_t>* vals3 =
                    col3->selectUInts(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            case ibis::ULONG:
            case ibis::LONG: {
                array_t<int64_t>* vals3 =
                    col3->selectLongs(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            case ibis::FLOAT: {
                array_t<float>* vals3 =
                    col3->selectFloats(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            case ibis::DOUBLE: {
                array_t<double>* vals3 =
                    col3->selectDoubles(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            default: {
                LOGGER(ibis::gVerbose > 3)
                    << "part::get3DDistribution -- can not "
                    "handle column (" << cname3 << ") type "
                    << ibis::TYPESTRING[(int)col3->type()];

                ierr = -3;
                break;}
            }
            delete vals2;
            break;}
        case ibis::ULONG:
        case ibis::LONG: {
            array_t<int64_t>* vals2 = col2->selectLongs(hits);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }

            switch (col3->type()) {
            case ibis::BYTE:
            case ibis::SHORT:
            case ibis::INT: {
                array_t<int32_t>* vals3 =
                    col3->selectInts(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            case ibis::UBYTE:
            case ibis::USHORT:
            case ibis::CATEGORY:
            case ibis::UINT: {
                array_t<uint32_t>* vals3 =
                    col3->selectUInts(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            case ibis::ULONG:
            case ibis::LONG: {
                array_t<int64_t>* vals3 =
                    col3->selectLongs(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            case ibis::FLOAT: {
                array_t<float>* vals3 =
                    col3->selectFloats(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            case ibis::DOUBLE: {
                array_t<double>* vals3 =
                    col3->selectDoubles(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            default: {
                LOGGER(ibis::gVerbose > 3)
                    << "part::get3DDistribution -- can not "
                    "handle column (" << cname3 << ") type "
                    << ibis::TYPESTRING[(int)col3->type()];

                ierr = -3;
                break;}
            }
            delete vals2;
            break;}
        case ibis::FLOAT: {
            array_t<float>* vals2 = col2->selectFloats(hits);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }

            switch (col3->type()) {
            case ibis::BYTE:
            case ibis::SHORT:
            case ibis::INT: {
                array_t<int32_t>* vals3 =
                    col3->selectInts(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            case ibis::UBYTE:
            case ibis::USHORT:
            case ibis::CATEGORY:
            case ibis::UINT: {
                array_t<uint32_t>* vals3 =
                    col3->selectUInts(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            case ibis::ULONG:
            case ibis::LONG: {
                array_t<int64_t>* vals3 =
                    col3->selectLongs(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            case ibis::FLOAT: {
                array_t<float>* vals3 =
                    col3->selectFloats(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            case ibis::DOUBLE: {
                array_t<double>* vals3 =
                    col3->selectDoubles(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            default: {
                LOGGER(ibis::gVerbose > 3)
                    << "part::get3DDistribution -- can not "
                    "handle column (" << cname3 << ") type "
                    << ibis::TYPESTRING[(int)col3->type()];

                ierr = -3;
                break;}
            }
            delete vals2;
            break;}
        case ibis::DOUBLE: {
            array_t<double>* vals2 = col2->selectDoubles(hits);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }

            switch (col3->type()) {
            case ibis::BYTE:
            case ibis::SHORT:
            case ibis::INT: {
                array_t<int32_t>* vals3 =
                    col3->selectInts(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            case ibis::UBYTE:
            case ibis::USHORT:
            case ibis::CATEGORY:
            case ibis::UINT: {
                array_t<uint32_t>* vals3 =
                    col3->selectUInts(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            case ibis::ULONG:
            case ibis::LONG: {
                array_t<int64_t>* vals3 =
                    col3->selectLongs(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            case ibis::FLOAT: {
                array_t<float>* vals3 =
                    col3->selectFloats(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            case ibis::DOUBLE: {
                array_t<double>* vals3 =
                    col3->selectDoubles(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            default: {
                LOGGER(ibis::gVerbose > 3)
                    << "part::get3DDistribution -- can not "
                    "handle column (" << cname3 << ") type "
                    << ibis::TYPESTRING[(int)col3->type()];

                ierr = -3;
                break;}
            }
            delete vals2;
            break;}
        default: {
            LOGGER(ibis::gVerbose > 3)
                << "part::get3DDistribution -- can not "
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

            switch (col3->type()) {
            case ibis::BYTE:
            case ibis::SHORT:
            case ibis::INT: {
                array_t<int32_t>* vals3 =
                    col3->selectInts(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            case ibis::UBYTE:
            case ibis::USHORT:
            case ibis::CATEGORY:
            case ibis::UINT: {
                array_t<uint32_t>* vals3 =
                    col3->selectUInts(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            case ibis::ULONG:
            case ibis::LONG: {
                array_t<int64_t>* vals3 =
                    col3->selectLongs(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            case ibis::FLOAT: {
                array_t<float>* vals3 =
                    col3->selectFloats(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            case ibis::DOUBLE: {
                array_t<double>* vals3 =
                    col3->selectDoubles(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            default: {
                LOGGER(ibis::gVerbose > 3)
                    << "part::get3DDistribution -- can not "
                    "handle column (" << cname3 << ") type "
                    << ibis::TYPESTRING[(int)col3->type()];

                ierr = -3;
                break;}
            }
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

            switch (col3->type()) {
            case ibis::BYTE:
            case ibis::SHORT:
            case ibis::INT: {
                array_t<int32_t>* vals3 =
                    col3->selectInts(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            case ibis::UBYTE:
            case ibis::USHORT:
            case ibis::CATEGORY:
            case ibis::UINT: {
                array_t<uint32_t>* vals3 =
                    col3->selectUInts(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            case ibis::ULONG:
            case ibis::LONG: {
                array_t<int64_t>* vals3 =
                    col3->selectLongs(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            case ibis::FLOAT: {
                array_t<float>* vals3 =
                    col3->selectFloats(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            case ibis::DOUBLE: {
                array_t<double>* vals3 =
                    col3->selectDoubles(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            default: {
                LOGGER(ibis::gVerbose > 3)
                    << "part::get3DDistribution -- can not "
                    "handle column (" << cname3 << ") type "
                    << ibis::TYPESTRING[(int)col3->type()];

                ierr = -3;
                break;}
            }
            delete vals2;
            break;}
        case ibis::ULONG:
        case ibis::LONG: {
            array_t<int64_t>* vals2 = col2->selectLongs(hits);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }

            switch (col3->type()) {
            case ibis::BYTE:
            case ibis::SHORT:
            case ibis::INT: {
                array_t<int32_t>* vals3 =
                    col3->selectInts(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            case ibis::UBYTE:
            case ibis::USHORT:
            case ibis::CATEGORY:
            case ibis::UINT: {
                array_t<uint32_t>* vals3 =
                    col3->selectUInts(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            case ibis::ULONG:
            case ibis::LONG: {
                array_t<int64_t>* vals3 =
                    col3->selectLongs(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            case ibis::FLOAT: {
                array_t<float>* vals3 =
                    col3->selectFloats(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            case ibis::DOUBLE: {
                array_t<double>* vals3 =
                    col3->selectDoubles(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            default: {
                LOGGER(ibis::gVerbose > 3)
                    << "part::get3DDistribution -- can not "
                    "handle column (" << cname3 << ") type "
                    << ibis::TYPESTRING[(int)col3->type()];

                ierr = -3;
                break;}
            }
            delete vals2;
            break;}
        case ibis::FLOAT: {
            array_t<float>* vals2 = col2->selectFloats(hits);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }

            switch (col3->type()) {
            case ibis::BYTE:
            case ibis::SHORT:
            case ibis::INT: {
                array_t<int32_t>* vals3 =
                    col3->selectInts(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            case ibis::UBYTE:
            case ibis::USHORT:
            case ibis::CATEGORY:
            case ibis::UINT: {
                array_t<uint32_t>* vals3 =
                    col3->selectUInts(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            case ibis::ULONG:
            case ibis::LONG: {
                array_t<int64_t>* vals3 =
                    col3->selectLongs(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            case ibis::FLOAT: {
                array_t<float>* vals3 =
                    col3->selectFloats(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            case ibis::DOUBLE: {
                array_t<double>* vals3 =
                    col3->selectDoubles(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            default: {
                LOGGER(ibis::gVerbose > 3)
                    << "part::get3DDistribution -- can not "
                    "handle column (" << cname3 << ") type "
                    << ibis::TYPESTRING[(int)col3->type()];

                ierr = -3;
                break;}
            }
            delete vals2;
            break;}
        case ibis::DOUBLE: {
            array_t<double>* vals2 = col2->selectDoubles(hits);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }

            switch (col3->type()) {
            case ibis::BYTE:
            case ibis::SHORT:
            case ibis::INT: {
                array_t<int32_t>* vals3 =
                    col3->selectInts(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            case ibis::UBYTE:
            case ibis::USHORT:
            case ibis::CATEGORY:
            case ibis::UINT: {
                array_t<uint32_t>* vals3 =
                    col3->selectUInts(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            case ibis::ULONG:
            case ibis::LONG: {
                array_t<int64_t>* vals3 =
                    col3->selectLongs(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            case ibis::FLOAT: {
                array_t<float>* vals3 =
                    col3->selectFloats(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            case ibis::DOUBLE: {
                array_t<double>* vals3 =
                    col3->selectDoubles(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            default: {
                LOGGER(ibis::gVerbose > 3)
                    << "part::get3DDistribution -- can not "
                    "handle column (" << cname3 << ") type "
                    << ibis::TYPESTRING[(int)col3->type()];

                ierr = -3;
                break;}
            }
            delete vals2;
            break;}
        default: {
            LOGGER(ibis::gVerbose > 3)
                << "part::get3DDistribution -- can not "
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

            switch (col3->type()) {
            case ibis::BYTE:
            case ibis::SHORT:
            case ibis::INT: {
                array_t<int32_t>* vals3 =
                    col3->selectInts(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            case ibis::UBYTE:
            case ibis::USHORT:
            case ibis::CATEGORY:
            case ibis::UINT: {
                array_t<uint32_t>* vals3 =
                    col3->selectUInts(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            case ibis::ULONG:
            case ibis::LONG: {
                array_t<int64_t>* vals3 =
                    col3->selectLongs(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            case ibis::FLOAT: {
                array_t<float>* vals3 =
                    col3->selectFloats(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            case ibis::DOUBLE: {
                array_t<double>* vals3 =
                    col3->selectDoubles(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            default: {
                LOGGER(ibis::gVerbose > 3)
                    << "part::get3DDistribution -- can not "
                    "handle column (" << cname3 << ") type "
                    << ibis::TYPESTRING[(int)col3->type()];

                ierr = -3;
                break;}
            }
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

            switch (col3->type()) {
            case ibis::BYTE:
            case ibis::SHORT:
            case ibis::INT: {
                array_t<int32_t>* vals3 =
                    col3->selectInts(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            case ibis::UBYTE:
            case ibis::USHORT:
            case ibis::CATEGORY:
            case ibis::UINT: {
                array_t<uint32_t>* vals3 =
                    col3->selectUInts(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            case ibis::ULONG:
            case ibis::LONG: {
                array_t<int64_t>* vals3 =
                    col3->selectLongs(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            case ibis::FLOAT: {
                array_t<float>* vals3 =
                    col3->selectFloats(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            case ibis::DOUBLE: {
                array_t<double>* vals3 =
                    col3->selectDoubles(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            default: {
                LOGGER(ibis::gVerbose > 3)
                    << "part::get3DDistribution -- can not "
                    "handle column (" << cname3 << ") type "
                    << ibis::TYPESTRING[(int)col3->type()];

                ierr = -3;
                break;}
            }
            delete vals2;
            break;}
        case ibis::ULONG:
        case ibis::LONG: {
            array_t<int64_t>* vals2 = col2->selectLongs(hits);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }

            switch (col3->type()) {
            case ibis::BYTE:
            case ibis::SHORT:
            case ibis::INT: {
                array_t<int32_t>* vals3 =
                    col3->selectInts(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            case ibis::UBYTE:
            case ibis::USHORT:
            case ibis::CATEGORY:
            case ibis::UINT: {
                array_t<uint32_t>* vals3 =
                    col3->selectUInts(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            case ibis::ULONG:
            case ibis::LONG: {
                array_t<int64_t>* vals3 =
                    col3->selectLongs(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            case ibis::FLOAT: {
                array_t<float>* vals3 =
                    col3->selectFloats(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            case ibis::DOUBLE: {
                array_t<double>* vals3 =
                    col3->selectDoubles(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            default: {
                LOGGER(ibis::gVerbose > 3)
                    << "part::get3DDistribution -- can not "
                    "handle column (" << cname3 << ") type "
                    << ibis::TYPESTRING[(int)col3->type()];

                ierr = -3;
                break;}
            }
            delete vals2;
            break;}
        case ibis::FLOAT: {
            array_t<float>* vals2 = col2->selectFloats(hits);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }

            switch (col3->type()) {
            case ibis::BYTE:
            case ibis::SHORT:
            case ibis::INT: {
                array_t<int32_t>* vals3 =
                    col3->selectInts(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            case ibis::UBYTE:
            case ibis::USHORT:
            case ibis::CATEGORY:
            case ibis::UINT: {
                array_t<uint32_t>* vals3 =
                    col3->selectUInts(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            case ibis::ULONG:
            case ibis::LONG: {
                array_t<int64_t>* vals3 =
                    col3->selectLongs(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            case ibis::FLOAT: {
                array_t<float>* vals3 =
                    col3->selectFloats(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            case ibis::DOUBLE: {
                array_t<double>* vals3 =
                    col3->selectDoubles(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            default: {
                LOGGER(ibis::gVerbose > 3)
                    << "part::get3DDistribution -- can not "
                    "handle column (" << cname3 << ") type "
                    << ibis::TYPESTRING[(int)col3->type()];

                ierr = -3;
                break;}
            }
            delete vals2;
            break;}
        case ibis::DOUBLE: {
            array_t<double>* vals2 = col2->selectDoubles(hits);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }

            switch (col3->type()) {
            case ibis::BYTE:
            case ibis::SHORT:
            case ibis::INT: {
                array_t<int32_t>* vals3 =
                    col3->selectInts(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            case ibis::UBYTE:
            case ibis::USHORT:
            case ibis::CATEGORY:
            case ibis::UINT: {
                array_t<uint32_t>* vals3 =
                    col3->selectUInts(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            case ibis::ULONG:
            case ibis::LONG: {
                array_t<int64_t>* vals3 =
                    col3->selectLongs(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            case ibis::FLOAT: {
                array_t<float>* vals3 =
                    col3->selectFloats(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            case ibis::DOUBLE: {
                array_t<double>* vals3 =
                    col3->selectDoubles(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            default: {
                LOGGER(ibis::gVerbose > 3)
                    << "part::get3DDistribution -- can not "
                    "handle column (" << cname3 << ") type "
                    << ibis::TYPESTRING[(int)col3->type()];

                ierr = -3;
                break;}
            }
            delete vals2;
            break;}
        default: {
            LOGGER(ibis::gVerbose > 3)
                << "part::get3DDistribution -- can not "
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

            switch (col3->type()) {
            case ibis::BYTE:
            case ibis::SHORT:
            case ibis::INT: {
                array_t<int32_t>* vals3 =
                    col3->selectInts(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            case ibis::UBYTE:
            case ibis::USHORT:
            case ibis::CATEGORY:
            case ibis::UINT: {
                array_t<uint32_t>* vals3 =
                    col3->selectUInts(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            case ibis::ULONG:
            case ibis::LONG: {
                array_t<int64_t>* vals3 =
                    col3->selectLongs(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            case ibis::FLOAT: {
                array_t<float>* vals3 =
                    col3->selectFloats(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            case ibis::DOUBLE: {
                array_t<double>* vals3 =
                    col3->selectDoubles(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            default: {
                LOGGER(ibis::gVerbose > 3)
                    << "part::get3DDistribution -- can not "
                    "handle column (" << cname3 << ") type "
                    << ibis::TYPESTRING[(int)col3->type()];

                ierr = -3;
                break;}
            }
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

            switch (col3->type()) {
            case ibis::BYTE:
            case ibis::SHORT:
            case ibis::INT: {
                array_t<int32_t>* vals3 =
                    col3->selectInts(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            case ibis::UBYTE:
            case ibis::USHORT:
            case ibis::CATEGORY:
            case ibis::UINT: {
                array_t<uint32_t>* vals3 =
                    col3->selectUInts(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            case ibis::ULONG:
            case ibis::LONG: {
                array_t<int64_t>* vals3 =
                    col3->selectLongs(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            case ibis::FLOAT: {
                array_t<float>* vals3 =
                    col3->selectFloats(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            case ibis::DOUBLE: {
                array_t<double>* vals3 =
                    col3->selectDoubles(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            default: {
                LOGGER(ibis::gVerbose > 3)
                    << "part::get3DDistribution -- can not "
                    "handle column (" << cname3 << ") type "
                    << ibis::TYPESTRING[(int)col3->type()];

                ierr = -3;
                break;}
            }
            delete vals2;
            break;}
        case ibis::ULONG:
        case ibis::LONG: {
            array_t<int64_t>* vals2 = col2->selectLongs(hits);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }

            switch (col3->type()) {
            case ibis::BYTE:
            case ibis::SHORT:
            case ibis::INT: {
                array_t<int32_t>* vals3 =
                    col3->selectInts(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            case ibis::UBYTE:
            case ibis::USHORT:
            case ibis::CATEGORY:
            case ibis::UINT: {
                array_t<uint32_t>* vals3 =
                    col3->selectUInts(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            case ibis::ULONG:
            case ibis::LONG: {
                array_t<int64_t>* vals3 =
                    col3->selectLongs(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            case ibis::FLOAT: {
                array_t<float>* vals3 =
                    col3->selectFloats(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            case ibis::DOUBLE: {
                array_t<double>* vals3 =
                    col3->selectDoubles(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            default: {
                LOGGER(ibis::gVerbose > 3)
                    << "part::get3DDistribution -- can not "
                    "handle column (" << cname3 << ") type "
                    << ibis::TYPESTRING[(int)col3->type()];

                ierr = -3;
                break;}
            }
            delete vals2;
            break;}
        case ibis::FLOAT: {
            array_t<float>* vals2 = col2->selectFloats(hits);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }

            switch (col3->type()) {
            case ibis::BYTE:
            case ibis::SHORT:
            case ibis::INT: {
                array_t<int32_t>* vals3 =
                    col3->selectInts(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            case ibis::UBYTE:
            case ibis::USHORT:
            case ibis::CATEGORY:
            case ibis::UINT: {
                array_t<uint32_t>* vals3 =
                    col3->selectUInts(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            case ibis::ULONG:
            case ibis::LONG: {
                array_t<int64_t>* vals3 =
                    col3->selectLongs(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            case ibis::FLOAT: {
                array_t<float>* vals3 =
                    col3->selectFloats(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            case ibis::DOUBLE: {
                array_t<double>* vals3 =
                    col3->selectDoubles(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            default: {
                LOGGER(ibis::gVerbose > 3)
                    << "part::get3DDistribution -- can not "
                    "handle column (" << cname3 << ") type "
                    << ibis::TYPESTRING[(int)col3->type()];

                ierr = -3;
                break;}
            }
            delete vals2;
            break;}
        case ibis::DOUBLE: {
            array_t<double>* vals2 = col2->selectDoubles(hits);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }

            switch (col3->type()) {
            case ibis::BYTE:
            case ibis::SHORT:
            case ibis::INT: {
                array_t<int32_t>* vals3 =
                    col3->selectInts(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            case ibis::UBYTE:
            case ibis::USHORT:
            case ibis::CATEGORY:
            case ibis::UINT: {
                array_t<uint32_t>* vals3 =
                    col3->selectUInts(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            case ibis::ULONG:
            case ibis::LONG: {
                array_t<int64_t>* vals3 =
                    col3->selectLongs(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            case ibis::FLOAT: {
                array_t<float>* vals3 =
                    col3->selectFloats(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            case ibis::DOUBLE: {
                array_t<double>* vals3 =
                    col3->selectDoubles(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            default: {
                LOGGER(ibis::gVerbose > 3)
                    << "part::get3DDistribution -- can not "
                    "handle column (" << cname3 << ") type "
                    << ibis::TYPESTRING[(int)col3->type()];

                ierr = -3;
                break;}
            }
            delete vals2;
            break;}
        default: {
            LOGGER(ibis::gVerbose > 3)
                << "part::get3DDistribution -- can not "
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

            switch (col3->type()) {
            case ibis::BYTE:
            case ibis::SHORT:
            case ibis::INT: {
                array_t<int32_t>* vals3 =
                    col3->selectInts(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            case ibis::UBYTE:
            case ibis::USHORT:
            case ibis::CATEGORY:
            case ibis::UINT: {
                array_t<uint32_t>* vals3 =
                    col3->selectUInts(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            case ibis::ULONG:
            case ibis::LONG: {
                array_t<int64_t>* vals3 =
                    col3->selectLongs(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            case ibis::FLOAT: {
                array_t<float>* vals3 =
                    col3->selectFloats(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            case ibis::DOUBLE: {
                array_t<double>* vals3 =
                    col3->selectDoubles(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            default: {
                LOGGER(ibis::gVerbose > 3)
                    << "part::get3DDistribution -- can not "
                    "handle column (" << cname3 << ") type "
                    << ibis::TYPESTRING[(int)col3->type()];

                ierr = -3;
                break;}
            }
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

            switch (col3->type()) {
            case ibis::BYTE:
            case ibis::SHORT:
            case ibis::INT: {
                array_t<int32_t>* vals3 =
                    col3->selectInts(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            case ibis::UBYTE:
            case ibis::USHORT:
            case ibis::CATEGORY:
            case ibis::UINT: {
                array_t<uint32_t>* vals3 =
                    col3->selectUInts(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            case ibis::ULONG:
            case ibis::LONG: {
                array_t<int64_t>* vals3 =
                    col3->selectLongs(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            case ibis::FLOAT: {
                array_t<float>* vals3 =
                    col3->selectFloats(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            case ibis::DOUBLE: {
                array_t<double>* vals3 =
                    col3->selectDoubles(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            default: {
                LOGGER(ibis::gVerbose > 3)
                    << "part::get3DDistribution -- can not "
                    "handle column (" << cname3 << ") type "
                    << ibis::TYPESTRING[(int)col3->type()];

                ierr = -3;
                break;}
            }
            delete vals2;
            break;}
        case ibis::ULONG:
        case ibis::LONG: {
            array_t<int64_t>* vals2 = col2->selectLongs(hits);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }

            switch (col3->type()) {
            case ibis::BYTE:
            case ibis::SHORT:
            case ibis::INT: {
                array_t<int32_t>* vals3 =
                    col3->selectInts(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            case ibis::UBYTE:
            case ibis::USHORT:
            case ibis::CATEGORY:
            case ibis::UINT: {
                array_t<uint32_t>* vals3 =
                    col3->selectUInts(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            case ibis::ULONG:
            case ibis::LONG: {
                array_t<int64_t>* vals3 =
                    col3->selectLongs(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            case ibis::FLOAT: {
                array_t<float>* vals3 =
                    col3->selectFloats(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            case ibis::DOUBLE: {
                array_t<double>* vals3 =
                    col3->selectDoubles(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            default: {
                LOGGER(ibis::gVerbose > 3)
                    << "part::get3DDistribution -- can not "
                    "handle column (" << cname3 << ") type "
                    << ibis::TYPESTRING[(int)col3->type()];

                ierr = -3;
                break;}
            }
            delete vals2;
            break;}
        case ibis::FLOAT: {
            array_t<float>* vals2 = col2->selectFloats(hits);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }

            switch (col3->type()) {
            case ibis::BYTE:
            case ibis::SHORT:
            case ibis::INT: {
                array_t<int32_t>* vals3 =
                    col3->selectInts(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            case ibis::UBYTE:
            case ibis::USHORT:
            case ibis::CATEGORY:
            case ibis::UINT: {
                array_t<uint32_t>* vals3 =
                    col3->selectUInts(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            case ibis::ULONG:
            case ibis::LONG: {
                array_t<int64_t>* vals3 =
                    col3->selectLongs(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            case ibis::FLOAT: {
                array_t<float>* vals3 =
                    col3->selectFloats(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            case ibis::DOUBLE: {
                array_t<double>* vals3 =
                    col3->selectDoubles(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            default: {
                LOGGER(ibis::gVerbose > 3)
                    << "part::get3DDistribution -- can not "
                    "handle column (" << cname3 << ") type "
                    << ibis::TYPESTRING[(int)col3->type()];

                ierr = -3;
                break;}
            }
            delete vals2;
            break;}
        case ibis::DOUBLE: {
            array_t<double>* vals2 = col2->selectDoubles(hits);
            if (vals2 == 0) {
                ierr = -5;
                break;
            }

            switch (col3->type()) {
            case ibis::BYTE:
            case ibis::SHORT:
            case ibis::INT: {
                array_t<int32_t>* vals3 =
                    col3->selectInts(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            case ibis::UBYTE:
            case ibis::USHORT:
            case ibis::CATEGORY:
            case ibis::UINT: {
                array_t<uint32_t>* vals3 =
                    col3->selectUInts(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            case ibis::ULONG:
            case ibis::LONG: {
                array_t<int64_t>* vals3 =
                    col3->selectLongs(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            case ibis::FLOAT: {
                array_t<float>* vals3 =
                    col3->selectFloats(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            case ibis::DOUBLE: {
                array_t<double>* vals3 =
                    col3->selectDoubles(hits);
                if (vals3 == 0) {
                    ierr = -6;
                    break;
                }
                ierr = count3DWeights(*vals1, begin1, end1, stride1,
                                      *vals2, begin2, end2, stride2,
                                      *vals3, begin3, end3, stride3,
                                      *wts, weights);
                delete vals3;
                break;}
            default: {
                LOGGER(ibis::gVerbose > 3)
                    << "part::get3DDistribution -- can not "
                    "handle column (" << cname3 << ") type "
                    << ibis::TYPESTRING[(int)col3->type()];

                ierr = -3;
                break;}
            }
            delete vals2;
            break;}
        default: {
            LOGGER(ibis::gVerbose > 3)
                << "part::get3DDistribution -- can not "
                "handle column (" << cname2 << ") type "
                << ibis::TYPESTRING[(int)col2->type()];

            ierr = -3;
            break;}
        }
        delete vals1;
        break;}
    default: {
        LOGGER(ibis::gVerbose > 3)
            << "part::get3DDistribution -- can not "
            "handle column (" << cname1 << ") type "
            << ibis::TYPESTRING[(int)col1->type()];

        ierr = -3;
        break;}
    }
    delete wts;
    if (ierr > 0 && ibis::gVerbose > 0) {
        timer.stop();
        logMessage("get3DDistribution", "computing the joint distribution of "
                   "columns %s, %s, and %s%s%s took %g sec(CPU), %g "
                   "sec(elapsed)", cname1, cname2, cname2,
                   (constraints ? " with restriction " : ""),
                   (constraints ? constraints : ""),
                   timer.CPUTime(), timer.realTime());
    }
    return ierr;
} // ibis::part::get3DDistribution

/// Adaptive binning through regularly spaced bins.
///
/// @note Here are the special cases that are different from
/// ibis::part::adaptive2DBins.
/// - If the number of desired bins along any of the three dimensions,
///   nb1, nb2, or nb3, is zero (0) or one (1), it is set to 32.  If all
///   three dimensions are using 32 bins, there is a total of 32,768 bins
///   altogether.
/// - If the number of desired bins along any of the three dimensions, nb1,
///   nb2, or nb3, is greater than 128, it may be reduced to about
///   fourth root of the number of records in input.
///
/// @sa ibis::part::adaptive2DBins
template <typename T1, typename T2, typename T3> long
ibis::part::adaptive3DBins(const array_t<T1> &vals1,
                           const array_t<T2> &vals2,
                           const array_t<T3> &vals3,
                           uint32_t nb1, uint32_t nb2, uint32_t nb3,
                           std::vector<double> &bounds1,
                           std::vector<double> &bounds2,
                           std::vector<double> &bounds3,
                           std::vector<uint32_t> &counts) {
    const uint32_t nrows = (vals1.size() <= vals2.size() ?
                            (vals1.size() <= vals3.size() ?
                             vals1.size() : vals3.size()) :
                            (vals2.size() <= vals3.size() ?
                             vals2.size() : vals3.size()));
    bounds1.clear();
    bounds2.clear();
    bounds3.clear();
    counts.clear();
    if (nrows == 0)
        return 0L;

    T1 vmin1, vmax1;
    T2 vmin2, vmax2;
    T3 vmin3, vmax3;
    vmin1 = vals1[0];
    vmax1 = vals1[0];
    vmin2 = vals2[0];
    vmax2 = vals2[0];
    vmin3 = vals3[0];
    vmax3 = vals3[0];
    for (uint32_t i = 1; i < nrows; ++ i) {
        if (vmin1 > vals1[i])
            vmin1 = vals1[i];
        if (vmax1 < vals1[i])
            vmax1 = vals1[i];
        if (vmin2 > vals2[i])
            vmin2 = vals2[i];
        if (vmax2 < vals2[i])
            vmax2 = vals2[i];
        if (vmin3 > vals3[i])
            vmin3 = vals3[i];
        if (vmax3 < vals3[i])
            vmax3 = vals3[i];
    }
    // degenerate cases where one of the three dimensions has only one
    // single distinct value --- DO NOT use these special cases to compute
    // lower dimensional histograms because of the extra computations of
    // minimum and maximum values.
    if (vmin1 >= vmax1) { // vals1 has only one single value
        bounds1.resize(2);
        bounds1[0] = vmin1;
        bounds1[1] = ibis::util::incrDouble(static_cast<double>(vmin1));
        if (vmin2 >= vmax2) { // vals2 has only one single value as well
            bounds2.resize(2);
            bounds2[0] = vmin2;
            bounds2[1] = ibis::util::incrDouble(static_cast<double>(vmin2));
            if (vmin3 >= vmax3) { // vals3 has only one single value
                bounds3[0] = vmin3;
                bounds3[1] = ibis::util::incrDouble(static_cast<double>(vmin3));
                counts.resize(1);
                counts[0] = nrows;
            }
            else { // one-dimensional adaptive binning
                if (sizeof(T3) >= 4)
                    adaptiveFloats(vals3, vmin3, vmax3, nb3, bounds3, counts);
                else
                    adaptiveInts(vals3, vmin3, vmax3, nb3, bounds3, counts);
            }
        }
        else { // one-dimensional adaptive binning
            if (vmin3 >= vmax3) {
                bounds3.resize(2);
                bounds3[0] = vmin3;
                bounds3[1] = ibis::util::incrDouble(static_cast<double>(vmin3));
                if (sizeof(T2) >= 4)
                    adaptiveFloats(vals2, vmin2, vmax2, nb2, bounds2, counts);
                else
                    adaptiveInts(vals2, vmin2, vmax2, nb2, bounds2, counts);
            }
            else {
                adaptive2DBins(vals2, vals3, nb2, nb3,
                               bounds2, bounds3, counts);
            }
        }
        return counts.size();
    }
    else if (vmin2 >= vmax2) { // vals2 has one one single value, bin vals2
        bounds2.resize(2);
        bounds2[0] = vmin2;
        bounds2[1] = ibis::util::incrDouble(static_cast<double>(vmin2));
        if (vmin3 >= vmax3) { // vals3 has only one single value
            bounds3.resize(2);
            bounds3[0] = vmin3;
            bounds3[1] = ibis::util::incrDouble(static_cast<double>(vmin3));
            if (sizeof(T1) >= 4)
                adaptiveFloats(vals1, vmin1, vmax1, nb1, bounds1, counts);
            else
                adaptiveInts(vals1, vmin1, vmax1, nb1, bounds1, counts);
        }
        else {
            adaptive2DBins(vals1, vals3, nb1, nb3, bounds1, bounds3, counts);
        }
        return counts.size();
    }
    else if (vmin3 >= vmax3) { // vals3 has only one distinct value
        bounds3.resize(2);
        bounds3[0] = vmin3;
        bounds3[1] = ibis::util::incrDouble(static_cast<double>(vmin3));
        return adaptive2DBins(vals1, vals2, nb1, nb2, bounds1, bounds2, counts);
    }

    // normal case,  vals1, vals2, and vals3 have multiple distinct values
    // ==> nrows > 1
    std::string mesg;
    {
        std::ostringstream oss;
        oss << "part::adaptive3DBins<" << typeid(T1).name() << ", "
            << typeid(T2).name() << ", " << typeid(T3).name() << ">";
        mesg = oss.str();
    }
    ibis::util::timer atimer(mesg.c_str(), 3);
    if (nb1 <= 1) nb1 = 32;
    if (nb2 <= 1) nb2 = 32;
    if (nb3 <= 1) nb2 = 32;
    double tmp = exp(log((double)nrows)*0.25);
    if (nb1 > 128 && nb1 > (uint32_t)tmp) {
        if (nrows > 10000000)
            nb1 = static_cast<uint32_t>(0.5 + tmp);
        else
            nb1 = 128;
    }
    if (nb2 > 128 && nb2 > (uint32_t)tmp) {
        if (nrows > 10000000)
            nb2 = static_cast<uint32_t>(0.5 + tmp);
        else
            nb2 = 128;
    }
    if (nb3 > 128 && nb3 > (uint32_t)tmp) {
        if (nrows > 10000000)
            nb3 = static_cast<uint32_t>(0.5 + tmp);
        else
            nb3 = 128;
    }
    tmp = exp(log((double)nrows/((double)nb1*nb2*nb3))*0.25);
    if (tmp < 2.0) tmp = 2.0;
    const uint32_t nfine1 = static_cast<uint32_t>(0.5 + tmp * nb1);
    const uint32_t nfine2 = static_cast<uint32_t>(0.5 + tmp * nb2);
    const uint32_t nfine3 = static_cast<uint32_t>(0.5 + tmp * nb3);
    // try to make sure the 2nd bin boundary do not round down to a value
    // that is actually included in the 1st bin
    const double scale1 = (1.0 - nfine1 * DBL_EPSILON) *
        ((double) nfine1 / (double)(vmax1 - vmin1));
    const double scale2 = (1.0 - nfine2 * DBL_EPSILON) *
        ((double)nfine2 / (double)(vmax2 - vmin2));
    const double scale3 = (1.0 - nfine3 * DBL_EPSILON) *
        ((double)nfine3 / (double)(vmax3 - vmin3));
    LOGGER(ibis::gVerbose > 3)
        << mesg << " internally uses "<< nfine1 << " x " << nfine2 << " x " 
        << nfine3 << " uniform bins for " << nrows
        << " records in the range of [" << vmin1 << ", " << vmax1
        << "] x [" << vmin2 << ", " << vmax2 << "]"
        << "] x [" << vmin3 << ", " << vmax3 << "]";

    array_t<uint32_t> cnts1(nfine1,0), cnts2(nfine2,0), cnts3(nfine2,0),
        cntsa(nfine1*nfine2*nfine3,0);
    // loop to count values in fine bins
    for (uint32_t i = 0; i < nrows; ++ i) {
        const uint32_t j1 = static_cast<uint32_t>((vals1[i]-vmin1)*scale1);
        const uint32_t j2 = static_cast<uint32_t>((vals2[i]-vmin2)*scale2);
        const uint32_t j3 = static_cast<uint32_t>((vals3[i]-vmin3)*scale3);
        ++ cnts1[j1];
        ++ cnts2[j2];
        ++ cnts3[j3];
        ++ cntsa[(j1*nfine2+j2)*nfine3+j3];
    }
    // divide the fine bins into final bins
    array_t<uint32_t> bnds1(nb1), bnds2(nb2), bnds3(nb3);
    ibis::index::divideCounts(bnds1, cnts1);
    ibis::index::divideCounts(bnds2, cnts2);
    ibis::index::divideCounts(bnds3, cnts3);
    nb1 = bnds1.size(); // the final size
    nb2 = bnds2.size();
    nb3 = bnds3.size();
    LOGGER(ibis::gVerbose > 4)
        << mesg << " is to use " << nb1 << " x " << nb2 << " x "
        << nb3 << " advative bins for a 3D histogram";

    // insert the value 0 as the first element of bnds[123]
    bnds1.resize(nb1+1);
    bounds1.resize(nb1+1);
    for (uint32_t i = nb1; i > 0; -- i) {
        bnds1[i] = bnds1[i-1];
        bounds1[i] = vmin1 + bnds1[i-1] / scale1;
    }
    bnds1[0] = 0;
    bounds1[0] = vmin1;

    bnds2.resize(nb2+1);
    bounds2.resize(nb2+1);
    for (uint32_t i = nb2; i > 0; -- i) {
        bnds2[i] = bnds2[i-1];
        bounds2[i] = vmin2 + bnds2[i-1] / scale2;
    }
    bnds2[0] = 0;
    bounds2[0] = vmin2;

    bnds3.resize(nb3+1);
    bounds3.resize(nb3+1);
    for (uint32_t i = nb3; i > 0; -- i) {
        bnds3[i] = bnds3[i-1];
        bounds3[i] = vmin3 + bnds3[i-1] / scale3;
    }
    bnds3[0] = 0;
    bounds3[0] = vmin3;
#if defined(_DEBUG) || defined(DEBUG)
    if (ibis::gVerbose > 4) {
        ibis::util::logger lg;
        lg() << "DEBUG -- " << mesg
                    << " scale1 = " << std::setprecision(18) << scale1
                    << ", scale2 = " << std::setprecision(18) << scale2
                    << ", scale3 = " << std::setprecision(18) << scale3
                    << "\n  bounds1[" << bounds1.size()
                    << "]: " << bounds1[0];
        for (uint32_t i = 1; i < bounds1.size(); ++ i)
            lg() << ", " << bounds1[i];
        lg() << "\n  bounds2[" << bounds2.size()
                    << "]: " << bounds2[0];
        for (uint32_t i = 1; i < bounds2.size(); ++ i)
            lg() << ", " << bounds2[i];
        lg() << "\n  bounds3[" << bounds3.size()
                    << "]: " << bounds3[0];
        for (uint32_t i = 1; i < bounds3.size(); ++ i)
            lg() << ", " << bounds3[i];
        lg() << "\n  bnds1[" << bnds1.size()
                    << "]: " << bnds1[0];
        for (uint32_t i = 1; i < bnds1.size(); ++ i)
            lg() << ", " << bnds1[i];
        lg() << "\n  bnds2[" << bnds2.size()
                    << "]: " << bnds2[0];
        for (uint32_t i = 1; i < bnds2.size(); ++ i)
            lg() << ", " << bnds2[i];
        lg() << "\n  bnds3[" << bnds3.size()
                    << "]: " << bnds3[0];
        for (uint32_t i = 1; i < bnds3.size(); ++ i)
            lg() << ", " << bnds3[i];
    }
#endif

    counts.resize(nb1*nb2*nb3);
    for (uint32_t j1 = 0; j1 < nb1; ++ j1) { // j1
        const uint32_t joff1 = j1 * nb2;
        for (uint32_t j2 = 0; j2 < nb2; ++ j2) { // j2
            const uint32_t joff2 = (joff1 + j2) * nb3;
            for (uint32_t j3 = 0; j3 < nb3; ++ j3) { // j3
                uint32_t &tmp = (counts[joff2+j3]);
                tmp = 0;
                for (uint32_t i1 = bnds1[j1]; i1 < bnds1[j1+1]; ++ i1) {
                    const uint32_t ioff1 = i1 * nfine2;
                    for (uint32_t i2 = ioff1 + bnds2[j2];
                         i2 < ioff1 + bnds2[j2+1]; ++ i2) {
                        const uint32_t ioff2 = i2 * nfine3;
                        for (uint32_t i3 = ioff2 + bnds3[j3];
                             i3 < ioff2 + bnds3[j3+1]; ++ i3)
                            tmp += cntsa[i3];
                    } // i2
                } // i1
            } // j3
        } // j2
    } // j1

    return counts.size();
} // ibis::part::adaptive3DBins

/// Upon successful completion of this function, the return value shall be
/// the number of bins produced, which is equal to the number of elements
/// in array counts.
/// Error codes:
/// - -1: one or more of the column names are nil strings;
/// - -2: one or more column names are not present in the data partition;
/// - -5: error in column masks;
/// - [-100, -160]: error detected by get3DDistributionA.
long ibis::part::get3DDistribution(const char *cname1, const char *cname2,
                                   const char *cname3,
                                   uint32_t nb1, uint32_t nb2, uint32_t nb3,
                                   std::vector<double> &bounds1,
                                   std::vector<double> &bounds2,
                                   std::vector<double> &bounds3,
                                   std::vector<uint32_t> &counts,
                                   const char* const option) const {
    if (cname1 == 0 || *cname1 == 0 ||
        cname2 == 0 || *cname2 == 0 ||
        cname3 == 0 || *cname3 == 0) return -1L;

    const ibis::column* col1 = getColumn(cname1);
    const ibis::column* col2 = getColumn(cname2);
    const ibis::column* col3 = getColumn(cname3);
    if (col1 == 0 || col2 == 0 || col3 == 0)
        return -2L;

    ibis::bitvector mask;
    col1->getNullMask(mask);
    if (mask.size() == nEvents) {
        ibis::bitvector tmp;
        col2->getNullMask(tmp);
        mask &= tmp;
        col3->getNullMask(tmp);
        mask &= tmp;
        if (mask.cnt() == 0) {
            bounds1.clear();
            bounds2.clear();
            bounds3.clear();
            counts.clear();
            return 0L;
        }
    }
    else {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- part[" << m_name
            << "]::get3DDistributionA - null mask of " << col1->name()
            << " has " << mask.size() << " bits, but " << nEvents
            << " are expected";
        return -5L;
    }

    long ierr = get3DDistributionA(mask, *col1, *col2, *col3, nb1, nb2, nb3,
                                   bounds1, bounds2, bounds3, counts);
    if (ierr <= 0)
        ierr -= 100;
    return ierr;
} // ibis::part::get3DDistribution

/// Upon successful completion of this function, the return value shall be
/// the number of bins produced, which is equal to the number of elements
/// in array counts.
/// Error codes:
/// - -1: one or more of the column names are nil strings;
/// - -2: one or more column names are not present in the data partition;
/// - -3: contraints contain invalid expressions or invalid column names;
/// - -4: contraints can not be evaluated correctly;
/// - [-100, -160]: error detected by get3DDistributionA.
long ibis::part::get3DDistribution(const char *constraints,
                                   const char *cname1, const char *cname2,
                                   const char *cname3,
                                   uint32_t nb1, uint32_t nb2, uint32_t nb3,
                                   std::vector<double> &bounds1,
                                   std::vector<double> &bounds2,
                                   std::vector<double> &bounds3,
                                   std::vector<uint32_t> &counts) const {
    if (cname1 == 0 || *cname1 == 0 ||
        cname2 == 0 || *cname2 == 0 ||
        cname3 == 0 || *cname3 == 0) return -1L;
    if (constraints == 0 || *constraints == 0 || *constraints == '*')
        return get3DDistribution(cname1, cname2, cname3, nb1, nb2, nb3,
                                 bounds1, bounds2, bounds3, counts);

    const ibis::column* col1 = getColumn(cname1);
    const ibis::column* col2 = getColumn(cname2);
    const ibis::column* col3 = getColumn(cname3);
    if (col1 == 0 || col2 == 0 || col3 == 0)
        return -2L;

    long ierr;
    ibis::bitvector mask;
    col1->getNullMask(mask);
    { // a block for finding out which records satisfy the constraints
        ibis::countQuery qq(this);
        ierr = qq.setWhereClause(constraints);
        if (ierr < 0)
            return -3L;

        ierr = qq.evaluate();
        if (ierr < 0)
            return -4L;
        if (qq.getNumHits() == 0) {
            bounds1.clear();
            bounds2.clear();
            bounds3.clear();
            counts.clear();
            return 0;
        }

        mask &= (*(qq.getHitVector()));
        ibis::bitvector tmp;
        col2->getNullMask(tmp);
        mask &= tmp;
        col3->getNullMask(tmp);
        mask &= tmp;
        LOGGER(ibis::gVerbose > 1)
            << "part[" << (m_name ? m_name : "") << "]::get3DDistribution"
            << " -- the constraints \"" << constraints << "\" selects "
            << mask.cnt() << " record" << (mask.cnt() > 1 ? "s" : "")
            << " out of " << nEvents;
    }

    ierr = get3DDistributionA(mask, *col1, *col2, *col3, nb1, nb2, nb3,
                              bounds1, bounds2, bounds3, counts);
    if (ierr <= 0)
        ierr -= 100;
    return ierr;
} // ibis::part::get3DDistribution

/// Compute 3D distribution with adaptive bins.  It is layered on top of
/// three templated functions, get3DDistributionA1, get3DDistributionA2,
/// and adaptive3DBins.  The last function, which is a class function if
/// ibis::part, performs the actual counting, the others are mainly
/// responsible for retrieving values from disk.
///
/// This function either returns a negative between -1 and -11 to indicate
/// error detected here, or a value returned by get3DDistributionA1.  On
/// successful completion of this function, it should return the number of
/// bins in array counts, which should be exactly
/// @code bounds1.size() * bounds2.size() * bounds3.size(). @endcode
long ibis::part::get3DDistributionA(const ibis::bitvector &mask,
                                    const ibis::column &col1,
                                    const ibis::column &col2,
                                    const ibis::column &col3,
                                    uint32_t nb1, uint32_t nb2, uint32_t nb3,
                                    std::vector<double> &bounds1,
                                    std::vector<double> &bounds2,
                                    std::vector<double> &bounds3,
                                    std::vector<uint32_t> &counts) const {
    long ierr = -1;
    switch (col1.type()) {
    default: {
        LOGGER(ibis::gVerbose > 1)
            << "part[" << (m_name ? m_name : "")
            << "]::get3DDistributionA -- does not suport column type "
            << ibis::TYPESTRING[(int)col1.type()] << " for column "
            << col1.name();
        ierr = -1;
        break;}
#ifdef FASTBIT_EXPAND_ALL_TYPES
    case ibis::BYTE: {
        array_t<signed char>* vals1 = col1.selectBytes(mask);
        if (vals1 != 0) {
            ierr = get3DDistributionA1(mask, *vals1, col2, col3, nb1, nb2, nb3,
                                       bounds1, bounds2, bounds3, counts);
            delete vals1;
            for (uint32_t i = 0; i < bounds1.size(); ++ i)
                bounds1[i] = ceil(bounds1[i]);
        }
        else {
            ierr = -2;
        }
        break;}
    case ibis::UBYTE: {
        array_t<unsigned char>* vals1 = col1.selectUBytes(mask);
        if (vals1 != 0) {
            ierr = get3DDistributionA1(mask, *vals1, col2, col3, nb1, nb2, nb3,
                                       bounds1, bounds2, bounds3, counts);
            delete vals1;
            for (uint32_t i = 0; i < bounds1.size(); ++ i)
                bounds1[i] = ceil(bounds1[i]);
        }
        else {
            ierr = -3;
        }
        break; }
    case ibis::SHORT: {
        array_t<int16_t>* vals1 = col1.selectShorts(mask);
        if (vals1 != 0) {
            ierr = get3DDistributionA1(mask, *vals1, col2, col3, nb1, nb2, nb3,
                                       bounds1, bounds2, bounds3, counts);
            delete vals1;
            for (uint32_t i = 0; i < bounds1.size(); ++ i)
                bounds1[i] = ceil(bounds1[i]);
        }
        else {
            ierr = -4;
        }
        break;}
    case ibis::USHORT: {
        array_t<uint16_t>* vals1 = col1.selectUShorts(mask);
        if (vals1 != 0) {
            ierr = get3DDistributionA1(mask, *vals1, col2, col3, nb1, nb2, nb3,
                                       bounds1, bounds2, bounds3, counts);
            delete vals1;
            for (uint32_t i = 0; i < bounds1.size(); ++ i)
                bounds1[i] = ceil(bounds1[i]);
        }
        else {
            ierr = -5;
        }
        break;}
#endif
#ifndef FASTBIT_EXPAND_ALL_TYPES
    case ibis::BYTE:
    case ibis::SHORT:
#endif
    case ibis::INT: {
        array_t<int32_t>* vals1 = col1.selectInts(mask);
        if (vals1 != 0) {
            ierr = get3DDistributionA1(mask, *vals1, col2, col3, nb1, nb2, nb3,
                                       bounds1, bounds2, bounds3, counts);
            delete vals1;
            for (uint32_t i = 0; i < bounds1.size(); ++ i)
                bounds1[i] = ceil(bounds1[i]);
        }
        else {
            ierr = -6;
        }
        break;}
#ifndef FASTBIT_EXPAND_ALL_TYPES
    case ibis::UBYTE:
    case ibis::USHORT:
#endif
    case ibis::CATEGORY:
    case ibis::UINT: {
        array_t<uint32_t>* vals1 = col1.selectUInts(mask);
        if (vals1 != 0) {
            ierr = get3DDistributionA1(mask, *vals1, col2, col3, nb1, nb2, nb3,
                                       bounds1, bounds2, bounds3, counts);
            delete vals1;
            for (uint32_t i = 0; i < bounds1.size(); ++ i)
                bounds1[i] = ceil(bounds1[i]);
        }
        else {
            ierr = -7;
        }
        break;}
    case ibis::ULONG:
#ifdef FASTBIT_EXPAND_ALL_TYPES
        {
        array_t<uint64_t>* vals1 = col1.selectULongs(mask);
        if (vals1 != 0) {
            ierr = get3DDistributionA1(mask, *vals1, col2, col3, nb1, nb2, nb3,
                                       bounds1, bounds2, bounds3, counts);
            delete vals1;
            for (uint32_t i = 0; i < bounds1.size(); ++ i)
                bounds1[i] = ceil(bounds1[i]);
        }
        else {
            ierr = -9;
        }
        break;}
#endif
    case ibis::LONG: {
        array_t<int64_t>* vals1 = col1.selectLongs(mask);
        if (vals1 != 0) {
            ierr = get3DDistributionA1(mask, *vals1, col2, col3, nb1, nb2, nb3,
                                       bounds1, bounds2, bounds3, counts);
            delete vals1;
            for (uint32_t i = 0; i < bounds1.size(); ++ i)
                bounds1[i] = ceil(bounds1[i]);
        }
        else {
            ierr = -8;
        }
        break;}
    case ibis::FLOAT: {
        array_t<float>* vals1 = col1.selectFloats(mask);
        if (vals1 != 0) {
            ierr = get3DDistributionA1(mask, *vals1, col2, col3, nb1, nb2, nb3,
                                       bounds1, bounds2, bounds3, counts);
            delete vals1;
        }
        else {
            ierr = -10;
        }
        break;}
    case ibis::DOUBLE: {
        array_t<double>* vals1 = col1.selectDoubles(mask);
        if (vals1 != 0) {
            ierr = get3DDistributionA1(mask, *vals1, col2, col3, nb1, nb2, nb3,
                                       bounds1, bounds2, bounds3, counts);
            delete vals1;
        }
        else {
            ierr = -11;
        }
        break;}
    } // col1.type()
    return ierr;
} // ibis::part::get3DDistributionA

/// Read the value of the second column.  Call get3DDistributionA2 to
/// process the next column and eventually compute the histogram.
/// This function may return a value between -20 and -30 to indicate an
/// error, or a value returned by get3DDistributionA2.
template <typename E1>
long ibis::part::get3DDistributionA1(const ibis::bitvector &mask,
                                     const array_t<E1> &vals1,
                                     const ibis::column &col2,
                                     const ibis::column &col3,
                                     uint32_t nb1, uint32_t nb2, uint32_t nb3,
                                     std::vector<double> &bounds1,
                                     std::vector<double> &bounds2,
                                     std::vector<double> &bounds3,
                                     std::vector<uint32_t> &counts) const {
    long ierr = -20;
    switch (col2.type()) {
    default:
        LOGGER(ibis::gVerbose > 1)
            << "part[" << (m_name ? m_name : "")
            << "]::get3DDistributionA -- does not suport column type "
            << ibis::TYPESTRING[(int)col2.type()] << " for column "
            << col2.name();
        ierr = -20;
        break;
#ifdef FASTBIT_EXPAND_ALL_TYPES
    case ibis::BYTE: {
        array_t<signed char>* vals2 = col2.selectBytes(mask);
        if (vals2 != 0) {
            ierr = get3DDistributionA2(mask, vals1, *vals2, col3, nb1, nb2,
                                       nb3, bounds1, bounds2, bounds3, counts);
            delete vals2;
            for (uint32_t i = 0; i < bounds2.size(); ++ i)
                bounds2[i] = ceil(bounds2[i]);
        }
        else {
            ierr = -21;
        }
        break;}
    case ibis::UBYTE: {
        array_t<unsigned char>* vals2 = col2.selectUBytes(mask);
        if (vals2 != 0) {
            ierr = get3DDistributionA2(mask, vals1, *vals2, col3, nb1, nb2,
                                       nb3, bounds1, bounds2, bounds3, counts);
            delete vals2;
            for (uint32_t i = 0; i < bounds2.size(); ++ i)
                bounds2[i] = ceil(bounds2[i]);
        }
        else {
            ierr = -22;
        }
        break; }
    case ibis::SHORT: {
        array_t<int16_t>* vals2 = col2.selectShorts(mask);
        if (vals2 != 0) {
            ierr = get3DDistributionA2(mask, vals1, *vals2, col3, nb1, nb2,
                                       nb3, bounds1, bounds2, bounds3, counts);
            delete vals2;
            for (uint32_t i = 0; i < bounds2.size(); ++ i)
                bounds2[i] = ceil(bounds2[i]);
        }
        else {
            ierr = -23;
        }
        break;}
    case ibis::USHORT: {
        array_t<uint16_t>* vals2 = col2.selectUShorts(mask);
        if (vals2 != 0) {
            ierr = get3DDistributionA2(mask, vals1, *vals2, col3, nb1, nb2,
                                       nb3, bounds1, bounds2, bounds3, counts);
            delete vals2;
            for (uint32_t i = 0; i < bounds2.size(); ++ i)
                bounds2[i] = ceil(bounds2[i]);
        }
        else {
            ierr = -24;
        }
        break;}
#endif
#ifndef FASTBIT_EXPAND_ALL_TYPES
    case ibis::BYTE:
    case ibis::SHORT:
#endif
    case ibis::INT: {
        array_t<int32_t>* vals2 = col2.selectInts(mask);
        if (vals2 != 0) {
            ierr = get3DDistributionA2(mask, vals1, *vals2, col3, nb1, nb2,
                                       nb3, bounds1, bounds2, bounds3, counts);
            delete vals2;
            for (uint32_t i = 0; i < bounds2.size(); ++ i)
                bounds2[i] = ceil(bounds2[i]);
        }
        else {
            ierr = -25;
        }
        break;}
#ifndef FASTBIT_EXPAND_ALL_TYPES
    case ibis::UBYTE:
    case ibis::USHORT:
#endif
    case ibis::CATEGORY:
    case ibis::UINT: {
        array_t<uint32_t>* vals2 = col2.selectUInts(mask);
        if (vals2 != 0) {
            ierr = get3DDistributionA2(mask, vals1, *vals2, col3, nb1, nb2,
                                       nb3, bounds1, bounds2, bounds3, counts);
            delete vals2;
            for (uint32_t i = 0; i < bounds2.size(); ++ i)
                bounds2[i] = ceil(bounds2[i]);
        }
        else {
            ierr = -26;
        }
        break;}
    case ibis::ULONG:
#ifdef FASTBIT_EXPAND_ALL_TYPES
        {
        array_t<uint64_t>* vals2 = col2.selectULongs(mask);
        if (vals2 != 0) {
            ierr = get3DDistributionA2(mask, vals1, *vals2, col3, nb1, nb2,
                                       nb3, bounds1, bounds2, bounds3, counts);
            delete vals2;
            for (uint32_t i = 0; i < bounds2.size(); ++ i)
                bounds2[i] = ceil(bounds2[i]);
        }
        else {
            ierr = -28;
        }
        break;}
#endif
    case ibis::LONG: {
        array_t<int64_t>* vals2 = col2.selectLongs(mask);
        if (vals2 != 0) {
            ierr = get3DDistributionA2(mask, vals1, *vals2, col3, nb1, nb2,
                                       nb3, bounds1, bounds2, bounds3, counts);
            delete vals2;
            for (uint32_t i = 0; i < bounds2.size(); ++ i)
                bounds2[i] = ceil(bounds2[i]);
        }
        else {
            ierr = -27;
        }
        break;}
    case ibis::FLOAT: {
        array_t<float>* vals2 = col2.selectFloats(mask);
        if (vals2 != 0) {
            ierr = get3DDistributionA2(mask, vals1, *vals2, col3, nb1, nb2,
                                       nb3, bounds1, bounds2, bounds3, counts);
            delete vals2;
        }
        else {
            ierr = -29;
        }
        break;}
    case ibis::DOUBLE: {
        array_t<double>* vals2 = col2.selectDoubles(mask);
        if (vals2 != 0) {
            ierr = get3DDistributionA2(mask, vals1, *vals2, col3, nb1, nb2,
                                       nb3, bounds1, bounds2, bounds3, counts);
            delete vals2;
        }
        else {
            ierr = -30;
        }
        break;}
    } // col2.type()
    return ierr;
} // ibis::part::get3DDistributionA1

/// Read the values of the third column.  Call the actual adaptive
/// binning function adaptive3DBins to compute the histogram.
/// Return the number of bins in the histogram or a negative value in the
/// range of -40 to -60 to indicate errors.
template <typename E1, typename E2>
long ibis::part::get3DDistributionA2(const ibis::bitvector &mask,
                                     const array_t<E1> &vals1,
                                     const array_t<E2> &vals2,
                                     const ibis::column &col3,
                                     uint32_t nb1, uint32_t nb2, uint32_t nb3,
                                     std::vector<double> &bounds1,
                                     std::vector<double> &bounds2,
                                     std::vector<double> &bounds3,
                                     std::vector<uint32_t> &counts) const {
    long ierr = -40;
    switch (col3.type()) {
    default:
        LOGGER(ibis::gVerbose > 1)
            << "part[" << (m_name ? m_name : "")
            << "]::get3DDistributionA -- does not suport column type "
            << ibis::TYPESTRING[(int)col3.type()] << " for column "
            << col3.name();
        ierr = -40;
        break;
#ifdef FASTBIT_EXPAND_ALL_TYPES
  case ibis::BYTE: {
        array_t<signed char>* vals3 = col3.selectBytes(mask);
        if (vals3 != 0) {
            try {
                ierr = adaptive3DBins(vals1, vals2, *vals3, nb1, nb2, nb3,
                                      bounds1, bounds2, bounds3, counts);
                for (uint32_t i = 0; i < bounds3.size(); ++ i)
                    bounds3[i] = ceil(bounds3[i]);
            }
            catch (...) {
                ierr = -51;
            }
            delete vals3;
        }
        else {
            ierr = -41;
        }
        break;}
    case ibis::UBYTE: {
        array_t<unsigned char>* vals3 = col3.selectUBytes(mask);
        if (vals3 != 0) {
            try {
                ierr = adaptive3DBins(vals1, vals2, *vals3, nb1, nb2, nb3,
                                      bounds1, bounds2, bounds3, counts);
                for (uint32_t i = 0; i < bounds3.size(); ++ i)
                    bounds3[i] = ceil(bounds3[i]);
            }
            catch (...) {
                ierr = -52;
            }
            delete vals3;
        }
        else {
            ierr = -42;
        }
        break; }
    case ibis::SHORT: {
        array_t<int16_t>* vals3 = col3.selectShorts(mask);
        if (vals3 != 0) {
            try {
                ierr = adaptive3DBins(vals1, vals2, *vals3, nb1, nb2, nb3,
                                      bounds1, bounds2, bounds3, counts);
                for (uint32_t i = 0; i < bounds3.size(); ++ i)
                    bounds3[i] = ceil(bounds3[i]);
            }
            catch (...) {
                ierr = -53;
            }
            delete vals3;
        }
        else {
            ierr = -43;
        }
        break;}
    case ibis::USHORT: {
        array_t<uint16_t>* vals3 = col3.selectUShorts(mask);
        if (vals3 != 0) {
            try {
                ierr = adaptive3DBins(vals1, vals2, *vals3, nb1, nb2, nb3,
                                      bounds1, bounds2, bounds3, counts);
                for (uint32_t i = 0; i < bounds3.size(); ++ i)
                    bounds3[i] = ceil(bounds3[i]);
            }
            catch (...) {
                ierr = -54;
            }
            delete vals3;
        }
        else {
            ierr = -44;
        }
        break;}
#endif
#ifndef FASTBIT_EXPAND_ALL_TYPES
    case ibis::BYTE:
    case ibis::SHORT:
#endif
    case ibis::INT: {
        array_t<int32_t>* vals3 = col3.selectInts(mask);
        if (vals3 != 0) {
            try {
                ierr = adaptive3DBins(vals1, vals2, *vals3, nb1, nb2, nb3,
                               bounds1, bounds2, bounds3, counts);
                for (uint32_t i = 0; i < bounds3.size(); ++ i)
                    bounds3[i] = ceil(bounds3[i]);
            }
            catch (...) {
                ierr = -55;
            }
            delete vals3;
        }
        else {
            ierr = -45;
        }
        break;}
#ifndef FASTBIT_EXPAND_ALL_TYPES
    case ibis::UBYTE:
    case ibis::USHORT:
#endif
    case ibis::CATEGORY:
    case ibis::UINT: {
        array_t<uint32_t>* vals3 = col3.selectUInts(mask);
        if (vals3 != 0) {
            try {
                ierr = adaptive3DBins(vals1, vals2, *vals3, nb1, nb2, nb3,
                                      bounds1, bounds2, bounds3, counts);
                for (uint32_t i = 0; i < bounds3.size(); ++ i)
                    bounds3[i] = ceil(bounds3[i]);
            }
            catch (...) {
                ierr = -56;
            }
            delete vals3;
        }
        else {
            ierr = -46;
        }
        break;}
    case ibis::ULONG:
#ifdef FASTBIT_EXPAND_ALL_TYPES
        {
        array_t<uint64_t>* vals3 = col3.selectULongs(mask);
        if (vals3 != 0) {
            try {
                ierr = adaptive3DBins(vals1, vals2, *vals3, nb1, nb2, nb3,
                                      bounds1, bounds2, bounds3, counts);
                for (uint32_t i = 0; i < bounds3.size(); ++ i)
                    bounds3[i] = ceil(bounds3[i]);
            }
            catch (...) {
                ierr = -58;
            }
            delete vals3;
        }
        else {
            ierr = -48;
        }
        break;}
#endif
    case ibis::LONG: {
        array_t<int64_t>* vals3 = col3.selectLongs(mask);
        if (vals3 != 0) {
            try {
                ierr = adaptive3DBins(vals1, vals2, *vals3, nb1, nb2, nb3,
                                      bounds1, bounds2, bounds3, counts);
                for (uint32_t i = 0; i < bounds3.size(); ++ i)
                    bounds3[i] = ceil(bounds3[i]);
            }
            catch (...) {
                ierr = -57;
            }
            delete vals3;
        }
        else {
            ierr = -47;
        }
        break;}
    case ibis::FLOAT: {
        array_t<float>* vals3 = col3.selectFloats(mask);
        if (vals3 != 0) {
            try {
                ierr = adaptive3DBins(vals1, vals2, *vals3, nb1, nb2, nb3,
                                      bounds1, bounds2, bounds3, counts);
            }
            catch (...) {
                ierr = -59;
            }
            delete vals3;
        }
        else {
            ierr = -49;
        }
        break;}
    case ibis::DOUBLE: {
        array_t<double>* vals3 = col3.selectDoubles(mask);
        if (vals3 != 0) {
            try {
                ierr = adaptive3DBins(vals1, vals2, *vals3, nb1, nb2, nb3,
                                      bounds1, bounds2, bounds3, counts);
            }
            catch (...) {
                ierr = -60;
            }
            delete vals3;
        }
        else {
            ierr = -50;
        }
        break;}
    } // col3.type()
    return ierr;
} // ibis::part::get3DDistributionA2

/// If the string constraints is nil or an empty string or starting with an
/// asterisk (*), it is assumed every valid record of the named column is
/// used.  Arrays bounds1 and bins are both for output only.  Upon
/// successful completion of this function, the return value shall be the
/// number of bins actually used.  A return value of 0 indicates no record
/// satisfy the constraints.  A negative return indicates error.
///
/// @sa ibis::part::get2DDistribution
long ibis::part::get3DBins(const char *constraints, const char *cname1,
                           const char *cname2, const char *cname3,
                           uint32_t nb1, uint32_t nb2, uint32_t nb3,
                           std::vector<double> &bounds1,
                           std::vector<double> &bounds2,
                           std::vector<double> &bounds3,
                           std::vector<ibis::bitvector> &bins) const {
    if (cname1 == 0 || *cname1 == 0 || cname2 == 0 || *cname2 == 0
        || cname3 == 0 || *cname3 == 0) return -1L;
    ibis::column *col1 = getColumn(cname1);
    ibis::column *col2 = getColumn(cname2);
    ibis::column *col3 = getColumn(cname3);
    if (col1 == 0 || col2 == 0 || col3 == 0) return -2L;
    std::string mesg;
    {
        std::ostringstream oss;
        oss << "part[" << (m_name ? m_name : "") << "]::get3DBins("
            << cname1 << ", " << cname2 << ", " << cname3 << ", "
            << nb1 << ", " << nb2 << ", " << nb3 << ")";
        mesg = oss.str();
    }
    ibis::util::timer atimer(mesg.c_str(), 1);
    ibis::bitvector mask;
    long ierr;
    {
        col1->getNullMask(mask);
        ibis::bitvector tmp;
        col2->getNullMask(tmp);
        mask &= tmp;
        col3->getNullMask(tmp);
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
        const uint32_t nrows = mask.cnt();
        if (nb1 <= 1) nb1 = 32;
        if (nb2 <= 1) nb2 = 32;
        if (nb3 <= 1) nb2 = 32;
        double tmp = exp(log((double)nrows)*0.25);
        if (nb1 > 128 && nb1 > (uint32_t)tmp) {
            if (nrows > 10000000)
                nb1 = static_cast<uint32_t>(0.5 + tmp);
            else
                nb1 = 128;
        }
        if (nb2 > 128 && nb2 > (uint32_t)tmp) {
            if (nrows > 10000000)
                nb2 = static_cast<uint32_t>(0.5 + tmp);
            else
                nb2 = 128;
        }
        if (nb3 > 128 && nb3 > (uint32_t)tmp) {
            if (nrows > 10000000)
                nb3 = static_cast<uint32_t>(0.5 + tmp);
            else
                nb3 = 128;
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

    std::vector<ibis::bitvector> bins3;
    ierr = get1DBins_(mask, *col3, nb3, bounds3, bins3, mesg.c_str());
    if (ierr <= 0) {
        LOGGER(ibis::gVerbose > 0)
            << mesg << " -- get1DBins_ on " << cname3 << " failed with error "
            << ierr;
        return ierr;
    }

    ierr = ibis::util::intersect(bins1, bins2, bins3, bins);
    return ierr;
} // ibis::part::get3DBins
