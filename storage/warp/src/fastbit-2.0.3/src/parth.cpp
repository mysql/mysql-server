// File $Id$
// Author: John Wu <John.Wu at ACM.org> Lawrence Berkeley National Laboratory
// Copyright (c) 2000-2016 the Regents of the University of California
//
// Implements ibis::part histogram functions.
#include "index.h"      // ibis::index::divideCounts
#include "countQuery.h" // ibis::countQuery
#include "part.h"

#include <math.h>       // ceil, sqrt
#include <limits>       // std::numeric_limits
#include <typeinfo>     // typeid
#include <memory>       // unique_ptr
#include <iomanip>      // setw, setprecision

// This file definte does not use the min and max macro.  Their presence
// could cause the calls to numeric_limits::min and numeric_limits::max to
// be misunderstood!
#undef max
#undef min

/// Count the number of records falling in the regular bins defined by the
/// <tt>begin:end:stride</tt> triplet.  The triplet defines
/// <tt> 1 + floor((end-begin)/stride) </tt> bins:
/// @code
/// [begin, begin+stride)
/// [begin+stride, begin+stride*2)
/// ...
/// [begin+stride*floor((end-begin)/stride), end].
/// @endcode
/// Note that the bins all have closed ends on the left, and open ends on
/// the right, except the last bin where both ends are closed.
///
/// When this function completes successfully, the array @c counts shall
/// have <tt> 1+floor((end-begin)/stride) </tt> elements, one for each bin.
/// The return value shall be the number of bins.  Any other value
/// indicates an error.  If array @c counts has the same size as the number
/// of bins on input, the count values will be added to the array.  This is
/// intended to be used to accumulate counts from different data
/// partitions.  If the array @c counts does not have the correct size, it
/// will be resized to the correct size and initialized to zero before
/// counting the the current data partition.
///
/// This function proceeds by first evaluate the constraints, then retrieve
/// the selected values, and finally count the number of records in each
/// bin.
///
/// The argument constraints can be nil (which is interpreted as "no
/// constraint"), but cname must be the name of a valid column in the data
/// partition.
///
/// @note This function is intended to work with numerical values.  It
/// treats categorical values as unsigned ints.  Passing the name of text
/// column to this function will result in a negative return value.
///
/// @sa ibis::table::getHistogram
long ibis::part::get1DDistribution(const char *constraints, const char *cname,
                                   double begin, double end, double stride,
                                   std::vector<uint32_t> &counts) const {
    if (cname == 0 || *cname == 0 || (begin >= end && !(stride < 0.0)) ||
        (begin <= end && !(stride > 0.0)))
        return -1L;

    const ibis::column* col = getColumn(cname);
    if (col == 0)
        return -2L;

    ibis::horometer timer;
    if (ibis::gVerbose > 0) {
        LOGGER(ibis::gVerbose > 2)
            << "part[" << (m_name ? m_name : "")
            << "]::get1DDistribution attempting to compute a histogram of "
            << cname << " with regular binning "
            << (constraints && *constraints ? " subject to " :
                " without constraints")
            << (constraints ? constraints : "");
        timer.start();
    }
    const uint32_t nbins = 1 + 
        static_cast<uint32_t>(floor((end - begin) / stride));
    if (counts.size() != nbins) {
        counts.resize(nbins);
        for (uint32_t i = 0; i < nbins; ++i)
            counts[i] = 0;
    }

    long ierr;
    ibis::bitvector mask;
    {
        ibis::countQuery qq(this);
        std::ostringstream oss;
        if (constraints != 0 && *constraints != 0)
            oss << "(" << constraints << ") AND ";
        oss << cname << " between " << std::setprecision(18) << begin
            << " and " << std::setprecision(18) << end;
        qq.setWhereClause(oss.str().c_str());

        ierr = qq.evaluate();
        if (ierr < 0)
            return ierr;
        ierr = qq.getNumHits();
        if (ierr <= 0)
            return ierr;
        mask.copy(*(qq.getHitVector()));
    }

    ierr = nbins;
    switch (col->type()) {
    case ibis::BYTE:
    case ibis::SHORT:
    case ibis::INT: {
        std::unique_ptr< array_t<int32_t> > vals(col->selectInts(mask));
        if (vals.get() != 0) {
            for (uint32_t i = 0; i < vals->size(); ++ i) {
                ++ counts[static_cast<uint32_t>(((*vals)[i] - begin) / stride)];
            }
        }
        else {
            ierr = -4;
        }
        break;}
    case ibis::CATEGORY:
    case ibis::UBYTE:
    case ibis::USHORT:
    case ibis::UINT: {
        std::unique_ptr< array_t<uint32_t> > vals(col->selectUInts(mask));
        if (vals.get() != 0) {
            for (uint32_t i = 0; i < vals->size(); ++ i) {
                ++ counts[static_cast<uint32_t>(((*vals)[i] - begin) / stride)];
            }
        }
        else {
            ierr = -4;
        }
        break;}
    case ibis::ULONG:
    case ibis::LONG: {
        std::unique_ptr< array_t<int64_t> > vals(col->selectLongs(mask));
        if (vals.get() != 0) {
            for (uint32_t i = 0; i < vals->size(); ++ i) {
                ++ counts[static_cast<uint32_t>(((*vals)[i] - begin) / stride)];
            }
        }
        else {
            ierr = -4;
        }
        break;}
    case ibis::FLOAT: {
        std::unique_ptr< array_t<float> > vals(col->selectFloats(mask));
        if (vals.get() != 0) {
            for (uint32_t i = 0; i < vals->size(); ++ i) {
                ++ counts[static_cast<uint32_t>(((*vals)[i] - begin) / stride)];
            }
        }
        else {
            ierr = -4;
        }
        break;}
    case ibis::DOUBLE: {
        std::unique_ptr< array_t<double> > vals(col->selectDoubles(mask));
        if (vals.get() != 0) {
            for (uint32_t i = 0; i < vals->size(); ++ i) {
                ++ counts[static_cast<uint32_t>(((*vals)[i] - begin) / stride)];
            }
        }
        else {
            ierr = -4;
        }
        break;}
    default: {
        LOGGER(ibis::gVerbose > 3)
            << "part::get1DDistribution -- can not "
            "handle column (" << cname << ") type "
            << ibis::TYPESTRING[(int)col->type()];

        ierr = -3;
        break;}
    }
    if (ierr > 0 && ibis::gVerbose > 0) {
        timer.stop();
        logMessage("get1DDistribution", "computing the distribution of column "
                   "%s%s%s took %g sec(CPU), %g sec(elapsed)",
                   cname, (constraints ? " with restriction " : ""),
                   (constraints ? constraints : ""),
                   timer.CPUTime(), timer.realTime());
    }
    return ierr;
} // ibis::part::get1DDistribution

/// Compute the weight in each regularly-spaced bin.  The bins are defined
/// by the  <tt>begin:end:stride</tt> triplet, which defines
/// <tt> 1 + floor((end-begin)/stride) </tt> bins:
/// @code
/// [begin, begin+stride)
/// [begin+stride, begin+stride*2)
/// ...
/// [begin+stride*floor((end-begin)/stride), end].
/// @endcode
/// Note that the bins all have closed ends on the left, and open ends on
/// the right, except the last bin where both ends are closed.
///
/// When this function completes successfully, the array @c weights shall
/// have <tt> 1+floor((end-begin)/stride) </tt> elements, one for each bin.
/// The return value shall be the number of bins.  Any other value
/// indicates an error.  If array @c weights has the same size as the number
/// of bins on input, the weight values will be added to the array.  This is
/// intended to be used to accumulate weights from different data
/// partitions.  If the array @c weights does not have the correct size, it
/// will be resized to the correct size and initialized to zero before
/// counting the the current data partition.
///
/// This function proceeds by first evaluate the constraints, then retrieve
/// the selected values, and finally computing the weights in each bin.
///
/// The constraints can be nil, which is interpreted as "no constraint",
/// however both cname and wtname must be valid column names of this data
/// partition.  Futhermore, both column must be numerical values, not
/// string values.
///
///
/// @note This function is intended to work with numerical values.  It
/// treats categorical values as unsigned ints.  Passing the name of text
/// column to this function will result in a negative return value.
long ibis::part::get1DDistribution(const char *constraints, const char *bname,
                                   double begin, double end, double stride,
                                   const char *wtname,
                                   std::vector<double> &weights) const {
    if (bname == 0 || *bname == 0 ||
        wtname == 0 || *wtname == 0 ||
        (begin >= end && !(stride < 0.0)) ||
        (begin <= end && !(stride > 0.0)))
        return -1L;

    const ibis::column* bcol = getColumn(bname);
    const ibis::column* wcol = getColumn(wtname);
    if (bcol == 0 || wcol == 0)
        return -2L;

    ibis::horometer timer;
    if (ibis::gVerbose > 0) {
        LOGGER(ibis::gVerbose > 2)
            << "part[" << (m_name ? m_name : "")
            << "]::get1DDistribution attempting to compute a histogram of "
            << bname << " with regular binning "
            << (constraints && *constraints ? " subject to " :
                " without constraints")
            << (constraints ? constraints : "") << " weighted with " << wtname;
        timer.start();
    }
    const uint32_t nbins = 1 + 
        static_cast<uint32_t>(floor((end - begin) / stride));
    if (weights.size() != nbins) {
        weights.resize(nbins);
        for (uint32_t i = 0; i < nbins; ++ i)
            weights[i] = 0.0;
    }

    long ierr;
    ibis::bitvector mask;
    wcol->getNullMask(mask);
    {  // use a block to limit the scope of query object
        ibis::countQuery qq(this);
        std::ostringstream oss;
        if (constraints != 0 && *constraints != 0)
            oss << "(" << constraints << ") AND ";
        oss << bname << " between " << std::setprecision(18) << begin
            << " and " << std::setprecision(18) << end;
        qq.setWhereClause(oss.str().c_str());

        ierr = qq.evaluate();
        if (ierr < 0)
            return ierr;
        ierr = qq.getNumHits();
        if (ierr <= 0)
            return ierr;

        mask &= (*(qq.getHitVector()));
    }

    ierr = nbins;
    std::unique_ptr< array_t<double> > wts(wcol->selectDoubles(mask));
    if (wts.get() == 0) {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- part[" << (m_name ? m_name : "")
            << "]::get1DDistribution failed retrieve values from column "
            << wcol->name() << " as weights";
        return -3L;
    }

    switch (bcol->type()) {
    case ibis::BYTE:
    case ibis::SHORT:
    case ibis::INT: {
        std::unique_ptr< array_t<int32_t> > vals(bcol->selectInts(mask));
        if (vals.get() != 0) {
            for (uint32_t i = 0; i < vals->size(); ++ i) {
                weights[static_cast<uint32_t>(((*vals)[i] - begin) / stride)]
                    += (*wts)[i];
            }
        }
        else {
            ierr = -4;
        }
        break;}
    case ibis::CATEGORY:
    case ibis::UBYTE:
    case ibis::USHORT:
    case ibis::UINT: {
        std::unique_ptr< array_t<uint32_t> > vals(bcol->selectUInts(mask));
        if (vals.get() != 0) {
            for (uint32_t i = 0; i < vals->size(); ++ i) {
                weights[static_cast<uint32_t>(((*vals)[i] - begin) / stride)]
                    += (*wts)[i];
            }
        }
        else {
            ierr = -4;
        }
        break;}
    case ibis::ULONG:
    case ibis::LONG: {
        std::unique_ptr< array_t<int64_t> > vals(bcol->selectLongs(mask));
        if (vals.get() != 0) {
            for (uint32_t i = 0; i < vals->size(); ++ i) {
                weights[static_cast<uint32_t>(((*vals)[i] - begin) / stride)]
                    += (*wts)[i];
            }
        }
        else {
            ierr = -4;
        }
        break;}
    case ibis::FLOAT: {
        std::unique_ptr< array_t<float> > vals(bcol->selectFloats(mask));
        if (vals.get() != 0) {
            for (uint32_t i = 0; i < vals->size(); ++ i) {
                weights[static_cast<uint32_t>(((*vals)[i] - begin) / stride)]
                    += (*wts)[i];
            }
        }
        else {
            ierr = -4;
        }
        break;}
    case ibis::DOUBLE: {
        std::unique_ptr< array_t<double> > vals(bcol->selectDoubles(mask));
        if (vals.get() != 0) {
            for (uint32_t i = 0; i < vals->size(); ++ i) {
                weights[static_cast<uint32_t>(((*vals)[i] - begin) / stride)]
                    += (*wts)[i];
            }
        }
        else {
            ierr = -4;
        }
        break;}
    default: {
        LOGGER(ibis::gVerbose > 3)
            << "part::get1DDistribution -- can not "
            "handle column (" << bname << ") type "
            << ibis::TYPESTRING[(int)bcol->type()];

        ierr = -3;
        break;}
    }

    if (ierr > 0 && ibis::gVerbose > 0) {
        timer.stop();
        logMessage("get1DDistribution", "computing the distribution of column "
                   "%s%s%s took %g sec(CPU), %g sec(elapsed)",
                   bname, (constraints ? " with restriction " : ""),
                   (constraints ? constraints : ""),
                   timer.CPUTime(), timer.realTime());
    }
    return ierr;
} // ibis::part::get1DDistribution

/// The number of bins defined by the given (begin, end, stride)-triplet is
/// <tt> 1 + floor((end-begin)/stride) </tt>, with the following bin
/// boundaries,
/// @code
/// [begin, begin+stride)
/// [begin+stride, begin+stride*2)
/// ...
/// [begin+stride*floor((end-begin)/stride), end].
/// @endcode
///
/// This function detects two error conditions.  It returns -11 to indicate
/// that mask and the number of values do not match.  Normally, the number
/// of elements in vals is either mask.size() or mask.cnt().  It returns
/// -10 if the triplet (begin, end, stride) does not define a valid set of
/// bins or defines more than 1 billion bins.  Upon successful completion
/// of this function, the return value is the number of bins,
/// i.e. bins.size().
///
/// @note All bitmaps that are empty are left with size() = 0.  All other
/// bitmaps have the same size() as mask.size().  When use these returned
/// bitmaps, please make sure to NOT mix empty bitmaps with non-empty
/// bitmaps in bitwise logical operations!
template <typename T1>
long ibis::part::fill1DBins(const ibis::bitvector &mask,
                            const array_t<T1> &vals,
                            const double &begin, const double &end,
                            const double &stride,
                            std::vector<ibis::bitvector> &bins) const {
    if ((end-begin) > 1e9 * stride || (end-begin) * stride < 0.0)
        return -10L;
    const uint32_t nbins = 1 + static_cast<uint32_t>((end-begin)/stride);
    if (mask.size() == vals.size()) {
        bins.resize(nbins);
        for (ibis::bitvector::indexSet is = mask.firstIndexSet();
             is.nIndices() > 0; ++ is) {
            const ibis::bitvector::word_t *idx = is.indices();
            if (is.isRange()) {
                for (uint32_t j = *idx; j < idx[1]; ++ j) {
                    const uint32_t ibin =
                        static_cast<uint32_t>((vals[j]-begin)/stride);
                    bins[ibin].setBit(j, 1);
                }
            }
            else {
                for (uint32_t k = 0; k < is.nIndices(); ++ k) {
                    const ibis::bitvector::word_t j = idx[k];
                    const uint32_t ibin =
                        static_cast<uint32_t>((vals[j]-begin)/stride);
                    bins[ibin].setBit(j, 1);
                }
            }
        }
        for (uint32_t i = 0; i < nbins; ++ i)
            if (bins[i].size() > 0)
                bins[i].adjustSize(0, mask.size());
    }
    else if (mask.cnt() == vals.size()) {
        bins.resize(nbins);
        uint32_t ivals = 0;
        for (ibis::bitvector::indexSet is = mask.firstIndexSet();
             is.nIndices() > 0; ++ is) {
            const ibis::bitvector::word_t *idx = is.indices();
            if (is.isRange()) {
                for (uint32_t j = *idx; j < idx[1]; ++j, ++ ivals) {
                    const uint32_t ibin =
                        static_cast<uint32_t>((vals[ivals]-begin)/stride);
                    bins[ibin].setBit(j, 1);
                }
            }
            else {
                for (uint32_t k = 0; k < is.nIndices(); ++ k, ++ ivals) {
                    const ibis::bitvector::word_t j = idx[k];
                    const uint32_t ibin =
                        static_cast<uint32_t>((vals[ivals]-begin)/stride);
                    bins[ibin].setBit(j, 1);
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
} // ibis::part::fill1DBins

/// The actual binning operations are performed in function template
/// ibis::part::fill1DBins.  The normal return value is the number of
/// bitmaps stored in bins.  Note that the empty bitmaps in bins all share
/// the same underlying storage.  The caller should avoid mixing these
/// empty bitmaps with others.
///
///
/// @note This function is intended to work with numerical values.  It
/// treats categorical values as unsigned ints.  Passing the name of text
/// column to this function will result in a negative return value.
///
/// @sa ibis::part::fill1DBins
long ibis::part::get1DBins(const char *constraints, const char *cname,
                           double begin, double end, double stride,
                           std::vector<ibis::bitvector> &bins) const {
    if (cname == 0 || *cname == 0 || (begin >= end && !(stride < 0.0)) ||
        (begin <= end && !(stride > 0.0)))
        return -1L;

    const ibis::column* col = getColumn(cname);
    if (col == 0)
        return -2L;

    ibis::horometer timer;
    if (ibis::gVerbose > 0) {
        LOGGER(ibis::gVerbose > 2)
            << "part[" << (m_name ? m_name : "")
            << "]::get1DBins attempting to compute a histogram of "
            << cname << " with regular binning "
            << (constraints && *constraints ? "subject to " :
                "without constraints")
            << (constraints ? constraints : "");
        timer.start();
    }

    long ierr;
    ibis::bitvector mask;
    {
        ibis::countQuery qq(this);
        std::ostringstream oss;
        if (constraints != 0 && *constraints != 0)
            oss << "(" << constraints << ") AND ";
        oss << cname << " between " << std::setprecision(18) << begin
            << " and " << std::setprecision(18) << end;
        qq.setWhereClause(oss.str().c_str());

        ierr = qq.evaluate();
        if (ierr < 0)
            return ierr;
        ierr = qq.getNumHits();
        if (ierr <= 0)
            return ierr;
        mask.copy(*(qq.getHitVector()));
    }

    switch (col->type()) {
    case ibis::BYTE: {
        array_t<signed char>* vals;
        if (mask.cnt() > (nEvents >> 4)) {
            vals = new array_t<signed char>;
            ierr = col->getValuesArray(vals);
            if (ierr < 0) {
                delete vals;
                vals = col->selectBytes(mask);
            }
        }
        else {
            vals = col->selectBytes(mask);
        }
        if (vals != 0) {
            ierr = fill1DBins(mask, *vals, begin, end, stride, bins);
            delete vals;
        }
        else {
            ierr = -4;
        }
        break;}
    case ibis::UBYTE: {
        array_t<unsigned char>* vals;
        if (mask.cnt() > (nEvents >> 4)) {
            vals = new array_t<unsigned char>;
            ierr = col->getValuesArray(vals);
            if (ierr < 0) {
                delete vals;
                vals = col->selectUBytes(mask);
            }
        }
        else {
            vals = col->selectUBytes(mask);
        }
        if (vals != 0) {
            ierr = fill1DBins(mask, *vals, begin, end, stride, bins);
            delete vals;
        }
        else {
            ierr = -4;
        }
        break;}
    case ibis::SHORT: {
        array_t<int16_t>* vals;
        if (mask.cnt() > (nEvents >> 4)) {
            vals = new array_t<int16_t>;
            ierr = col->getValuesArray(vals);
            if (ierr < 0) {
                delete vals;
                vals = col->selectShorts(mask);
            }
        }
        else {
            vals = col->selectShorts(mask);
        }
        if (vals != 0) {
            ierr = fill1DBins(mask, *vals, begin, end, stride, bins);
            delete vals;
        }
        else {
            ierr = -4;
        }
        break;}
    case ibis::USHORT: {
        array_t<uint16_t>* vals;
        if (mask.cnt() > (nEvents >> 4)) {
            vals = new array_t<uint16_t>;
            ierr = col->getValuesArray(vals);
            if (ierr < 0) {
                delete vals;
                vals = col->selectUShorts(mask);
            }
        }
        else {
            vals = col->selectUShorts(mask);
        }
        if (vals != 0) {
            ierr = fill1DBins(mask, *vals, begin, end, stride, bins);
            delete vals;
        }
        else {
            ierr = -4;
        }
        break;}
    case ibis::INT: {
        array_t<int32_t>* vals;
        if (mask.cnt() > (nEvents >> 4)) {
            vals = new array_t<int32_t>;
            ierr = col->getValuesArray(vals);
            if (ierr < 0) {
                delete vals;
                vals = col->selectInts(mask);
            }
        }
        else {
            vals = col->selectInts(mask);
        }
        if (vals != 0) {
            ierr = fill1DBins(mask, *vals, begin, end, stride, bins);
            delete vals;
        }
        else {
            ierr = -4;
        }
        break;}
    case ibis::CATEGORY:
    case ibis::UINT: {
        array_t<uint32_t>* vals;
        if (col->type() == ibis::UINT && mask.cnt() > (nEvents >> 4)) {
            vals = new array_t<uint32_t>;
            ierr = col->getValuesArray(vals);
            if (ierr < 0) {
                delete vals;
                vals = col->selectUInts(mask);
            }
        }
        else {
            vals = col->selectUInts(mask);
        }
        if (vals != 0) {
            ierr = fill1DBins(mask, *vals, begin, end, stride, bins);
            delete vals;
        }
        else {
            ierr = -4;
        }
        break;}
    case ibis::LONG: {
        array_t<int64_t>* vals;
        if (mask.cnt() > (nEvents >> 4)) {
            vals = new array_t<int64_t>;
            ierr = col->getValuesArray(vals);
            if (ierr < 0) {
                delete vals;
                vals = col->selectLongs(mask);
            }
        }
        else {
            vals = col->selectLongs(mask);
        }
        if (vals != 0) {
            ierr = fill1DBins(mask, *vals, begin, end, stride, bins);
            delete vals;
        }
        else {
            ierr = -4;
        }
        break;}
    case ibis::ULONG: {
        array_t<uint64_t>* vals;
        if (mask.cnt() > (nEvents >> 4)) {
            vals = new array_t<uint64_t>;
            ierr = col->getValuesArray(vals);
            if (ierr < 0) {
                delete vals;
                vals = col->selectULongs(mask);
            }
        }
        else {
            vals = col->selectULongs(mask);
        }
        if (vals != 0) {
            ierr = fill1DBins(mask, *vals, begin, end, stride, bins);
            delete vals;
        }
        else {
            ierr = -4;
        }
        break;}
    case ibis::FLOAT: {
        array_t<float>* vals;
        if (mask.cnt() > (nEvents >> 4)) {
            vals = new array_t<float>;
            ierr = col->getValuesArray(vals);
            if (ierr < 0) {
                delete vals;
                vals = col->selectFloats(mask);
            }
        }
        else {
            vals = col->selectFloats(mask);
        }
        if (vals != 0) {
            ierr = fill1DBins(mask, *vals, begin, end, stride, bins);
            delete vals;
        }
        else {
            ierr = -4;
        }
        break;}
    case ibis::DOUBLE: {
        array_t<double>* vals;
        if (mask.cnt() > (nEvents >> 4)) {
            vals = new array_t<double>;
            ierr = col->getValuesArray(vals);
            if (ierr < 0) {
                delete vals;
                vals = col->selectDoubles(mask);
            }
        }
        else {
            vals = col->selectDoubles(mask);
        }
        if (vals != 0) {
            ierr = fill1DBins(mask, *vals, begin, end, stride, bins);
            delete vals;
        }
        else {
            ierr = -4;
        }
        break;}
    default: {
        LOGGER(ibis::gVerbose > 3)
            << "part::get1DBins -- can not "
            "handle column (" << cname << ") type "
            << ibis::TYPESTRING[(int)col->type()];

        ierr = -3;
        break;}
    }
    if (ierr > 0 && ibis::gVerbose > 0) {
        timer.stop();
        logMessage("get1DBins", "computing the distribution of column "
                   "%s%s%s took %g sec(CPU), %g sec(elapsed)",
                   cname, (constraints ? " with restriction " : ""),
                   (constraints ? constraints : ""),
                   timer.CPUTime(), timer.realTime());
    }
    return ierr;
} // ibis::part::get1DBins

/// This version returns a vector of pointers to bitvectors.  It can reduce
/// the memory usage and reduce execution time if the majority of the bins
/// are empty.
template <typename T1>
long ibis::part::fill1DBins(const ibis::bitvector &mask,
                            const array_t<T1> &vals,
                            const double &begin, const double &end,
                            const double &stride,
                            std::vector<ibis::bitvector*> &bins) const {
    if ((end-begin) > 1e9 * stride || (end-begin) * stride < 0.0)
        return -10L;
    const uint32_t nbins = 1 + static_cast<uint32_t>((end-begin)/stride);
    if (mask.size() == vals.size()) {
        bins.resize(nbins);
        for (uint32_t i = 0; i < nbins; ++ i)
            bins[i] = 0;
        for (ibis::bitvector::indexSet is = mask.firstIndexSet();
             is.nIndices() > 0; ++ is) {
            const ibis::bitvector::word_t *idx = is.indices();
            if (is.isRange()) {
                for (uint32_t j = *idx; j < idx[1]; ++ j) {
                    const uint32_t ibin =
                        static_cast<uint32_t>((vals[j]-begin)/stride);
                    if (bins[ibin] == 0)
                        bins[ibin] = new ibis::bitvector;
                    bins[ibin]->setBit(j, 1);
                }
            }
            else {
                for (uint32_t k = 0; k < is.nIndices(); ++ k) {
                    const ibis::bitvector::word_t j = idx[k];
                    const uint32_t ibin =
                        static_cast<uint32_t>((vals[j]-begin)/stride);
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
    else if (mask.cnt() == vals.size()) {
        bins.resize(nbins);
        for (uint32_t i = 0; i < nbins; ++ i)
            bins[i] = 0;
        uint32_t ivals = 0;
        for (ibis::bitvector::indexSet is = mask.firstIndexSet();
             is.nIndices() > 0; ++ is) {
            const ibis::bitvector::word_t *idx = is.indices();
            if (is.isRange()) {
                for (uint32_t j = *idx; j < idx[1]; ++j, ++ ivals) {
                    const uint32_t ibin =
                        static_cast<uint32_t>((vals[ivals]-begin)/stride);
                    if (bins[ibin] == 0)
                        bins[ibin] = new ibis::bitvector;
                    bins[ibin]->setBit(j, 1);
                }
            }
            else {
                for (uint32_t k = 0; k < is.nIndices(); ++ k, ++ ivals) {
                    const ibis::bitvector::word_t j = idx[k];
                    const uint32_t ibin =
                        static_cast<uint32_t>((vals[ivals]-begin)/stride);
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
} // ibis::part::fill1DBins

/// This version returns a vector of pointers to bit vectors.  It can
/// reduce memory usage and reduce execution time if the majority of the
/// bins are empty.
long ibis::part::get1DBins(const char *constraints, const char *cname,
                           double begin, double end, double stride,
                           std::vector<ibis::bitvector*> &bins) const {
    if (cname == 0 || *cname == 0 || (begin >= end && !(stride < 0.0)) ||
        (begin <= end && !(stride > 0.0)))
        return -1L;

    const ibis::column* col = getColumn(cname);
    if (col == 0)
        return -2L;

    ibis::horometer timer;
    if (ibis::gVerbose > 0) {
        LOGGER(ibis::gVerbose > 2)
            << "part[" << (m_name ? m_name : "")
            << "]::get1DBins attempting to compute a histogram of "
            << cname << " with regular binning "
            << (constraints && *constraints ? "subject to " :
                "without constraints")
            << (constraints ? constraints : "");
        timer.start();
    }

    long ierr;
    ibis::bitvector mask;
    {
        ibis::countQuery qq(this);
        std::ostringstream oss;
        if (constraints != 0 && *constraints != 0)
            oss << "(" << constraints << ") AND ";
        oss << cname << " between " << std::setprecision(18) << begin
            << " and " << std::setprecision(18) << end;
        qq.setWhereClause(oss.str().c_str());

        ierr = qq.evaluate();
        if (ierr < 0)
            return ierr;
        ierr = qq.getNumHits();
        if (ierr <= 0)
            return ierr;
        mask.copy(*(qq.getHitVector()));
    }

    switch (col->type()) {
    case ibis::BYTE: {
        array_t<signed char>* vals;
        if (mask.cnt() > (nEvents >> 4)) {
            vals = new array_t<signed char>;
            ierr = col->getValuesArray(vals);
            if (ierr < 0) {
                delete vals;
                vals = col->selectBytes(mask);
            }
        }
        else {
            vals = col->selectBytes(mask);
        }
        if (vals != 0) {
            ierr = fill1DBins(mask, *vals, begin, end, stride, bins);
            delete vals;
        }
        else {
            ierr = -4;
        }
        break;}
    case ibis::UBYTE: {
        array_t<unsigned char>* vals;
        if (mask.cnt() > (nEvents >> 4)) {
            vals = new array_t<unsigned char>;
            ierr = col->getValuesArray(vals);
            if (ierr < 0) {
                delete vals;
                vals = col->selectUBytes(mask);
            }
        }
        else {
            vals = col->selectUBytes(mask);
        }
        if (vals != 0) {
            ierr = fill1DBins(mask, *vals, begin, end, stride, bins);
            delete vals;
        }
        else {
            ierr = -4;
        }
        break;}
    case ibis::SHORT: {
        array_t<int16_t>* vals;
        if (mask.cnt() > (nEvents >> 4)) {
            vals = new array_t<int16_t>;
            ierr = col->getValuesArray(vals);
            if (ierr < 0) {
                delete vals;
                vals = col->selectShorts(mask);
            }
        }
        else {
            vals = col->selectShorts(mask);
        }
        if (vals != 0) {
            ierr = fill1DBins(mask, *vals, begin, end, stride, bins);
            delete vals;
        }
        else {
            ierr = -4;
        }
        break;}
    case ibis::USHORT: {
        array_t<uint16_t>* vals;
        if (mask.cnt() > (nEvents >> 4)) {
            vals = new array_t<uint16_t>;
            ierr = col->getValuesArray(vals);
            if (ierr < 0) {
                delete vals;
                vals = col->selectUShorts(mask);
            }
        }
        else {
            vals = col->selectUShorts(mask);
        }
        if (vals != 0) {
            ierr = fill1DBins(mask, *vals, begin, end, stride, bins);
            delete vals;
        }
        else {
            ierr = -4;
        }
        break;}
    case ibis::INT: {
        array_t<int32_t>* vals;
        if (mask.cnt() > (nEvents >> 4)) {
            vals = new array_t<int32_t>;
            ierr = col->getValuesArray(vals);
            if (ierr < 0) {
                delete vals;
                vals = col->selectInts(mask);
            }
        }
        else {
            vals = col->selectInts(mask);
        }
        if (vals != 0) {
            ierr = fill1DBins(mask, *vals, begin, end, stride, bins);
            delete vals;
        }
        else {
            ierr = -4;
        }
        break;}
    case ibis::CATEGORY:
    case ibis::UINT: {
        array_t<uint32_t>* vals;
        if (col->type() == ibis::UINT && mask.cnt() > (nEvents >> 4)) {
            vals = new array_t<uint32_t>;
            ierr = col->getValuesArray(vals);
            if (ierr < 0) {
                delete vals;
                vals = col->selectUInts(mask);
            }
        }
        else {
            vals = col->selectUInts(mask);
        }
        if (vals != 0) {
            ierr = fill1DBins(mask, *vals, begin, end, stride, bins);
            delete vals;
        }
        else {
            ierr = -4;
        }
        break;}
    case ibis::LONG: {
        array_t<int64_t>* vals;
        if (mask.cnt() > (nEvents >> 4)) {
            vals = new array_t<int64_t>;
            ierr = col->getValuesArray(vals);
            if (ierr < 0) {
                delete vals;
                vals = col->selectLongs(mask);
            }
        }
        else {
            vals = col->selectLongs(mask);
        }
        if (vals != 0) {
            ierr = fill1DBins(mask, *vals, begin, end, stride, bins);
            delete vals;
        }
        else {
            ierr = -4;
        }
        break;}
    case ibis::ULONG: {
        array_t<uint64_t>* vals;
        if (mask.cnt() > (nEvents >> 4)) {
            vals = new array_t<uint64_t>;
            ierr = col->getValuesArray(vals);
            if (ierr < 0) {
                delete vals;
                vals = col->selectULongs(mask);
            }
        }
        else {
            vals = col->selectULongs(mask);
        }
        if (vals != 0) {
            ierr = fill1DBins(mask, *vals, begin, end, stride, bins);
            delete vals;
        }
        else {
            ierr = -4;
        }
        break;}
    case ibis::FLOAT: {
        array_t<float>* vals;
        if (mask.cnt() > (nEvents >> 4)) {
            vals = new array_t<float>;
            ierr = col->getValuesArray(vals);
            if (ierr < 0) {
                delete vals;
                vals = col->selectFloats(mask);
            }
        }
        else {
            vals = col->selectFloats(mask);
        }
        if (vals != 0) {
            ierr = fill1DBins(mask, *vals, begin, end, stride, bins);
            delete vals;
        }
        else {
            ierr = -4;
        }
        break;}
    case ibis::DOUBLE: {
        array_t<double>* vals;
        if (mask.cnt() > (nEvents >> 4)) {
            vals = new array_t<double>;
            ierr = col->getValuesArray(vals);
            if (ierr < 0) {
                delete vals;
                vals = col->selectDoubles(mask);
            }
        }
        else {
            vals = col->selectDoubles(mask);
        }
        if (vals != 0) {
            ierr = fill1DBins(mask, *vals, begin, end, stride, bins);
            delete vals;
        }
        else {
            ierr = -4;
        }
        break;}
    default: {
        LOGGER(ibis::gVerbose > 3)
            << "part::get1DBins -- can not "
            "handle column (" << cname << ") type "
            << ibis::TYPESTRING[(int)col->type()];

        ierr = -3;
        break;}
    }
    if (ierr > 0 && ibis::gVerbose > 0) {
        timer.stop();
        logMessage("get1DBins", "computing the distribution of column "
                   "%s%s%s took %g sec(CPU), %g sec(elapsed)",
                   cname, (constraints ? " with restriction " : ""),
                   (constraints ? constraints : ""),
                   timer.CPUTime(), timer.realTime());
    }
    return ierr;
} // ibis::part::get1DBins

/// Mark the positions of records falling in each bin and computed the
/// total weight in each bins.  This version returns a vector of pointers
/// to bitvectors.  It can reduce the memory usage and reduce execution
/// time if the majority of the bins are empty.
/// @note Assumes wts.size() == vals.size().
template <typename T1> long
ibis::part::fill1DBinsWeighted(const ibis::bitvector &mask,
                               const array_t<T1> &vals,
                               const double &begin, const double &end,
                               const double &stride,
                               const array_t<double> &wts,
                               std::vector<double> &weights,
                               std::vector<ibis::bitvector*> &bins) const {
    if ((end-begin) > 1e9 * stride || (end-begin) * stride < 0.0)
        return -10L;
    const uint32_t nbins = 1 + static_cast<uint32_t>((end-begin)/stride);

    if (mask.size() == vals.size() && vals.size() == wts.size()) {
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
                        static_cast<uint32_t>((vals[j]-begin)/stride);
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
                        static_cast<uint32_t>((vals[j]-begin)/stride);
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
    else if (mask.cnt() == vals.size() && vals.size() == wts.size()) {
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
                    const uint32_t ibin =
                        static_cast<uint32_t>((vals[ivals]-begin)/stride);
                    if (bins[ibin] == 0)
                        bins[ibin] = new ibis::bitvector;
                    bins[ibin]->setBit(j, 1);
                    weights[ibin] += wts[ivals];
                }
            }
            else {
                for (uint32_t k = 0; k < is.nIndices(); ++ k, ++ ivals) {
                    const ibis::bitvector::word_t j = idx[k];
                    const uint32_t ibin =
                        static_cast<uint32_t>((vals[ivals]-begin)/stride);
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
} // ibis::part::fill1DBinsWeighted

/// This version returns a vector of pointers to bit vectors.  It can
/// reduce memory usage and reduce execution time if the majority of the
/// bins are empty.
long ibis::part::get1DBins(const char *constraints, const char *cname,
                           double begin, double end, double stride,
                           const char *wtname,
                           std::vector<double> &weights,
                           std::vector<ibis::bitvector*> &bins) const {
    if (wtname == 0 || *wtname == 0 || cname == 0 || *cname == 0 ||
        (begin >= end && !(stride < 0.0)) || (begin <= end && !(stride > 0.0)))
        return -1L;

    const ibis::column* col = getColumn(cname);
    const ibis::column* wcol = getColumn(wtname);
    if (col == 0 || wcol == 0)
        return -2L;

    ibis::horometer timer;
    if (ibis::gVerbose > 0) {
        LOGGER(ibis::gVerbose > 2)
            << "part[" << (m_name ? m_name : "")
            << "]::get1DBins attempting to compute a histogram of "
            << cname << " with regular binning "
            << (constraints && *constraints ? "subject to " :
                "without constraints")
            << (constraints ? constraints : "") << " weighted with " << wtname;
        timer.start();
    }

    long ierr;
    ibis::bitvector mask;
    wcol->getNullMask(mask);
    { // use a block to limit the lifespan of the query object
        ibis::countQuery qq(this);
        std::ostringstream oss;
        if (constraints != 0 && *constraints != 0)
            oss << "(" << constraints << ") AND ";
        oss << cname << " between " << std::setprecision(18) << begin
            << " and " << std::setprecision(18) << end;
        qq.setWhereClause(oss.str().c_str());

        ierr = qq.evaluate();
        if (ierr < 0)
            return ierr;
        ierr = qq.getNumHits();
        if (ierr <= 0)
            return ierr;

        mask &= (*(qq.getHitVector()));
    }

    array_t<double>* wts;
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
            << "]::get1DDistribution failed retrieve values from column "
            << wcol->name() << " as weights";
        return -3L;
    }

    switch (col->type()) {
    case ibis::BYTE: {
        array_t<signed char>* vals;
        if (mask.cnt() > (nEvents >> 4)) {
            vals = new array_t<signed char>;
            ierr = col->getValuesArray(vals);
            if (ierr < 0) {
                delete vals;
                vals = col->selectBytes(mask);
            }
        }
        else {
            vals = col->selectBytes(mask);
        }
        if (vals != 0) {
            ierr = fill1DBinsWeighted(mask, *vals, begin, end, stride,
                                      *wts, weights, bins);
            delete vals;
        }
        else {
            ierr = -4;
        }
        break;}
    case ibis::UBYTE: {
        array_t<unsigned char>* vals;
        if (mask.cnt() > (nEvents >> 4)) {
            vals = new array_t<unsigned char>;
            ierr = col->getValuesArray(vals);
            if (ierr < 0) {
                delete vals;
                vals = col->selectUBytes(mask);
            }
        }
        else {
            vals = col->selectUBytes(mask);
        }
        if (vals != 0) {
            ierr = fill1DBinsWeighted(mask, *vals, begin, end, stride,
                                      *wts, weights, bins);
            delete vals;
        }
        else {
            ierr = -4;
        }
        break;}
    case ibis::SHORT: {
        array_t<int16_t>* vals;
        if (mask.cnt() > (nEvents >> 4)) {
            vals = new array_t<int16_t>;
            ierr = col->getValuesArray(vals);
            if (ierr < 0) {
                delete vals;
                vals = col->selectShorts(mask);
            }
        }
        else {
            vals = col->selectShorts(mask);
        }
        if (vals != 0) {
            ierr = fill1DBinsWeighted(mask, *vals, begin, end, stride,
                                      *wts, weights, bins);
            delete vals;
        }
        else {
            ierr = -4;
        }
        break;}
    case ibis::USHORT: {
        array_t<uint16_t>* vals;
        if (mask.cnt() > (nEvents >> 4)) {
            vals = new array_t<uint16_t>;
            ierr = col->getValuesArray(vals);
            if (ierr < 0) {
                delete vals;
                vals = col->selectUShorts(mask);
            }
        }
        else {
            vals = col->selectUShorts(mask);
        }
        if (vals != 0) {
            ierr = fill1DBinsWeighted(mask, *vals, begin, end, stride,
                                      *wts, weights, bins);
            delete vals;
        }
        else {
            ierr = -4;
        }
        break;}
    case ibis::INT: {
        array_t<int32_t>* vals;
        if (mask.cnt() > (nEvents >> 4)) {
            vals = new array_t<int32_t>;
            ierr = col->getValuesArray(vals);
            if (ierr < 0) {
                delete vals;
                vals = col->selectInts(mask);
            }
        }
        else {
            vals = col->selectInts(mask);
        }
        if (vals != 0) {
            ierr = fill1DBinsWeighted(mask, *vals, begin, end, stride,
                                      *wts, weights, bins);
            delete vals;
        }
        else {
            ierr = -4;
        }
        break;}
    case ibis::CATEGORY:
    case ibis::UINT: {
        array_t<uint32_t>* vals;
        if (col->type() == ibis::UINT && mask.cnt() > (nEvents >> 4)) {
            vals = new array_t<uint32_t>;
            ierr = col->getValuesArray(vals);
            if (ierr < 0) {
                delete vals;
                vals = col->selectUInts(mask);
            }
        }
        else {
            vals = col->selectUInts(mask);
        }
        if (vals != 0) {
            ierr = fill1DBinsWeighted(mask, *vals, begin, end, stride,
                                      *wts, weights, bins);
            delete vals;
        }
        else {
            ierr = -4;
        }
        break;}
    case ibis::LONG: {
        array_t<int64_t>* vals;
        if (mask.cnt() > (nEvents >> 4)) {
            vals = new array_t<int64_t>;
            ierr = col->getValuesArray(vals);
            if (ierr < 0) {
                delete vals;
                vals = col->selectLongs(mask);
            }
        }
        else {
            vals = col->selectLongs(mask);
        }
        if (vals != 0) {
            ierr = fill1DBinsWeighted(mask, *vals, begin, end, stride,
                                      *wts, weights, bins);
            delete vals;
        }
        else {
            ierr = -4;
        }
        break;}
    case ibis::ULONG: {
        array_t<uint64_t>* vals;
        if (mask.cnt() > (nEvents >> 4)) {
            vals = new array_t<uint64_t>;
            ierr = col->getValuesArray(vals);
            if (ierr < 0) {
                delete vals;
                vals = col->selectULongs(mask);
            }
        }
        else {
            vals = col->selectULongs(mask);
        }
        if (vals != 0) {
            ierr = fill1DBinsWeighted(mask, *vals, begin, end, stride,
                                      *wts, weights, bins);
            delete vals;
        }
        else {
            ierr = -4;
        }
        break;}
    case ibis::FLOAT: {
        array_t<float>* vals;
        if (mask.cnt() > (nEvents >> 4)) {
            vals = new array_t<float>;
            ierr = col->getValuesArray(vals);
            if (ierr < 0) {
                delete vals;
                vals = col->selectFloats(mask);
            }
        }
        else {
            vals = col->selectFloats(mask);
        }
        if (vals != 0) {
            ierr = fill1DBinsWeighted(mask, *vals, begin, end, stride,
                                      *wts, weights, bins);
            delete vals;
        }
        else {
            ierr = -4;
        }
        break;}
    case ibis::DOUBLE: {
        array_t<double>* vals;
        if (mask.cnt() > (nEvents >> 4)) {
            vals = new array_t<double>;
            ierr = col->getValuesArray(vals);
            if (ierr < 0) {
                delete vals;
                vals = col->selectDoubles(mask);
            }
        }
        else {
            vals = col->selectDoubles(mask);
        }
        if (vals != 0) {
            ierr = fill1DBinsWeighted(mask, *vals, begin, end, stride,
                                      *wts, weights, bins);
            delete vals;
        }
        else {
            ierr = -4;
        }
        break;}
    default: {
        LOGGER(ibis::gVerbose > 3)
            << "part::get1DBins -- can not "
            "handle column (" << cname << ") type "
            << ibis::TYPESTRING[(int)col->type()];

        ierr = -3;
        break;}
    }
    delete wts;
    if (ierr > 0 && ibis::gVerbose > 0) {
        timer.stop();
        logMessage("get1DBins", "computing the distribution of column "
                   "%s%s%s took %g sec(CPU), %g sec(elapsed)",
                   cname, (constraints ? " with restriction " : ""),
                   (constraints ? constraints : ""),
                   timer.CPUTime(), timer.realTime());
    }
    return ierr;
} // ibis::part::get1DBins

/// The caller specify the number of bins, but not the where to place the
/// bins.  The bounds array contains one more element than the counts array
/// and all the bins defined by the bounds are closed ranges.  More
/// specifically, the number of elements with values between
/// @code [bounds[i], bounds[i+1]) @endcode
/// is stored in @c counts[i].  Note that the lower bound of a range is
/// included in the bin, but the upper bound of a bin is excluded from the
/// bin.
/// @note The output number of bins may not be the input value nbin.
long ibis::part::get1DDistribution(const char* cname, uint32_t nbin,
                                   std::vector<double> &bounds,
                                   std::vector<uint32_t> &counts) const {
    if (cname == 0 || *cname == 0 || nEvents == 0) {
        return -1;
    }

    const ibis::column* col = getColumn(cname);
    if (col == 0) {
        return -2;
    }

    return get1DDistribution(*col, nbin, bounds, counts);
} // ibis::part::get1DDistribution

/// Calls function ibis::column::getDistribution to create the internal
/// histogram first, then pack them into a smaller number of bins if
/// necessary.
/// @note The output number of bins may not be the input value nbin.
long ibis::part::get1DDistribution(const ibis::column &col, uint32_t nbin,
                                   std::vector<double> &bounds,
                                   std::vector<uint32_t> &counts) const {
    const double amin = col.getActualMin();
    const double amax = col.getActualMax();
    long ierr = col.getDistribution(bounds, counts);
    if (ierr < 0) return ierr;

    if (static_cast<unsigned>(ierr) > nbin*3/2) {
        // too many bins returned, combine some of them
        ibis::fileManager::buffer<double> bbs(nbin+1);
        ibis::fileManager::buffer<uint32_t> cts(nbin+1);
        double* pbbs = bbs.address();
        uint32_t* pcts = cts.address();
        if (pbbs != 0 && pcts != 0) {
            ierr = packDistribution(bounds, counts, nbin, pbbs, pcts);
            if (ierr > 1) { // use the packed bins
                bounds.resize(ierr+1);
                bounds[0] = amin;
                for (int i = 0; i < ierr; ++ i)
                    bounds[i+1] = pbbs[i];
                bounds[ierr] = (col.isFloat() ? ibis::util::incrDouble(amax) :
                                floor(amax)+1.0);
                counts.resize(ierr);
                for (int i = 0; i < ierr; ++ i)
                    counts[i] = pcts[i];
                return ierr;
            }
        }
    }

    if (counts[0] > 0) { // add the actual minimal as the bounds[0]
        bounds.reserve(counts.size()+1);
        bounds.resize(bounds.size()+1);
        for (uint32_t i = bounds.size()-1; i > 0; -- i)
            bounds[i] = bounds[i-1];
        bounds[0] = amin;
    }
    else {
        const uint32_t nc = counts.size() - 1;
        for (uint32_t i = 0; i < nc; ++ i)
            counts[i] = counts[i+1];
        counts.resize(nc);
    }
    if (counts.back() > 0) { // add the largest values as the end of last bin
        if (amax - bounds.back() >= 0.0) {
            if (col.isFloat()) {
                double tmp;
                if (bounds.size() > 1)
                    tmp = ibis::util::compactValue
                        (amax, amax + (bounds[bounds.size()-1] -
                                       bounds[bounds.size()-2]));
                else
                    tmp = ibis::util::incrDouble(amax);
                bounds.push_back(tmp);
            }
            else {
                bounds.push_back(floor(amax) + 1.0);
            }
        }
        else {
            bounds.push_back(ibis::util::compactValue(bounds.back(), DBL_MAX));
        }
    }
    else {
        counts.resize(counts.size()-1);
    }
    return counts.size();
} // ibis::part::get1DDistribution

/// @note The output number of bins may not be the input value nbins.
long ibis::part::get1DDistribution(const char* constraints,
                                   const char* cname, uint32_t nbins,
                                   std::vector<double> &bounds,
                                   std::vector<uint32_t> &counts) const {
    if (cname == 0 || *cname == 0 || nEvents == 0) {
        return -1L;
    }

    const ibis::column* col = getColumn(cname);
    if (col == 0)
        return -2L;
    if (constraints == 0 || *constraints == 0 || *constraints == '*')
        return get1DDistribution(*col, nbins, bounds, counts);

    ibis::horometer timer;
    if (ibis::gVerbose > 0) {
        LOGGER(ibis::gVerbose > 2)
            << "part[" << (m_name ? m_name : "")
            << "]::get1DDistribution attempting to compute a histogram of "
            << cname << " with adaptive binning subject to " << constraints;
        timer.start();
    }

    long ierr;
    ibis::bitvector mask;
    col->getNullMask(mask);
    {
        ibis::countQuery qq(this);
        ierr = qq.setWhereClause(constraints);
        if (ierr < 0) {
            return -4;
        }

        ierr = qq.evaluate();
        if (ierr < 0) {
            return -5;
        }
        if (qq.getNumHits() == 0) {
            bounds.clear();
            counts.clear();
            return 0;
        }

        mask &= (*(qq.getHitVector()));
        LOGGER(ibis::gVerbose > 1)
            << "part[" << (m_name ? m_name : "")
            << "]::get1DDistribution -- the constraints \"" << constraints
            << "\" selects " << mask.cnt() << " record"
            << (mask.cnt() > 1 ? "s" : "") << " out of " << nEvents;
    }

    switch (col->type()) {
    case ibis::BYTE: {
        array_t<signed char> *vals = col->selectBytes(mask);
        if (vals == 0) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- part[" << (m_name ? m_name : "")
                << "]::get1DDistribution expected to retrieve "
                << mask.cnt() << (mask.cnt() > 1 ? "bytes" : "byte")
                << ", but got nothing";
            return -5;
        }
        else if (vals->size() != mask.cnt()) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- part[" << (m_name ? m_name : "")
                << "]::get1DDistribution expected to retrieve "
                << mask.cnt() << (mask.cnt() > 1 ? "bytes" : "byte")
                << ", but got " << vals->size() << " instead";
            delete vals;
            return -6;
        }

        ierr = adaptiveInts<signed char>(*vals, (char)-128, (char)127,
                                         nbins, bounds, counts);
        delete vals;
        break;}
    case ibis::UBYTE: {
        array_t<unsigned char> *vals = col->selectUBytes(mask);
        if (vals == 0) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- part[" << (m_name ? m_name : "")
                << "]::get1DDistribution expected to retrieve "
                << mask.cnt() << (mask.cnt() > 1 ? "bytes" : "byte")
                << ", but got nothing";
            return -5;
        }
        else if (vals->size() != mask.cnt()) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- part[" << (m_name ? m_name : "")
                << "]::get1DDistribution expected to retrieve "
                << mask.cnt() << (mask.cnt() > 1 ? "bytess" : "byte")
                << ", but got " << vals->size() << " instead";
            delete vals;
            return -6;
        }

        ierr = adaptiveInts<unsigned char>
            (*vals, (unsigned char)0, (unsigned char)255,
             nbins, bounds, counts);
        delete vals;
        break;}
    case ibis::SHORT: {
        array_t<int16_t> *vals = col->selectShorts(mask);
        if (vals == 0) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- part[" << (m_name ? m_name : "")
                << "]::get1DDistribution expected to retrieve "
                << mask.cnt() << (mask.cnt() > 1 ? "int16_ts" : "int16_t")
                << ", but got nothing";
            return -5;
        }
        else if (vals->size() != mask.cnt()) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- part[" << (m_name ? m_name : "")
                << "]::get1DDistribution expected to retrieve "
                << mask.cnt() << (mask.cnt() > 1 ? "int16_ts" : "int16_t")
                << ", but got " << vals->size() << " instead";
            delete vals;
            return -6;
        }

        int16_t vmin = (int16_t)-32768;
        int16_t vmax = (int16_t)32767;
        if (vals->size() < static_cast<uint32_t>(vmax)) {
            // compute the actual min and max
            vmin = (*vals)[0];
            vmax = (*vals)[1];
            for (uint32_t i = 1; i < vals->size(); ++ i) {
                if ((*vals)[i] > vmax)
                    vmax = (*vals)[i];
                if ((*vals)[i] < vmin)
                    vmin = (*vals)[i];
            }
        }
        ierr = adaptiveInts(*vals, vmin, vmax, nbins, bounds, counts);
        delete vals;
        break;}
    case ibis::USHORT: {
        array_t<uint16_t> *vals = col->selectUShorts(mask);
        if (vals == 0) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- part[" << (m_name ? m_name : "")
                << "]::get1DDistribution expected to retrieve "
                << mask.cnt() << (mask.cnt() > 1 ? "uint16_ts" : "uint16_t")
                << ", but got nothing";
            return -5;
        }
        else if (vals->size() != mask.cnt()) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- part[" << (m_name ? m_name : "")
                << "]::get1DDistribution expected to retrieve "
                << mask.cnt() << (mask.cnt() > 1 ? "uint16_ts" : "uint16_t")
                << ", but got " << vals->size() << " instead";
            delete vals;
            return -6;
        }

        uint16_t vmin = 0;
        uint16_t vmax = (uint16_t)65535;
        if (vals->size() < 32767) {
            // compute the actual min and max
            vmin = (*vals)[0];
            vmax = (*vals)[1];
            for (uint32_t i = 1; i < vals->size(); ++ i) {
                if ((*vals)[i] > vmax)
                    vmax = (*vals)[i];
                if ((*vals)[i] < vmin)
                    vmin = (*vals)[i];
            }
        }
        ierr = adaptiveInts(*vals, vmin, vmax, nbins, bounds, counts);
        delete vals;
        break;}
    case ibis::INT: {
        array_t<int32_t> *vals = col->selectInts(mask);
        if (vals == 0) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- part[" << (m_name ? m_name : "")
                << "]::get1DDistribution expected to retrieve "
                << mask.cnt() << (mask.cnt() > 1 ? "int32_ts" : "int32_t")
                << ", but got nothing";
            return -5;
        }
        else if (vals->size() != mask.cnt()) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- part[" << (m_name ? m_name : "")
                << "]::get1DDistribution expected to retrieve "
                << mask.cnt() << (mask.cnt() > 1 ? "int32_ts" : "int32_t")
                << ", but got " << vals->size() << " instead";
            delete vals;
            return -6;
        }

        int32_t vmin = (*vals)[0];
        int32_t vmax = (*vals)[0];
        for (uint32_t i = 1; i < vals->size(); ++ i) {
            if ((*vals)[i] > vmax)
                vmax = (*vals)[i];
            if ((*vals)[i] < vmin)
                vmin = (*vals)[i];
        }
        if (static_cast<uint32_t>(vmax-vmin) < vals->size())
            ierr = adaptiveInts(*vals, vmin, vmax, nbins, bounds, counts);
        else
            ierr = adaptiveFloats(*vals, vmin, vmax, nbins, bounds, counts);
        delete vals;
        break;}
    case ibis::CATEGORY:
    case ibis::UINT: {
        array_t<uint32_t> *vals = col->selectUInts(mask);
        if (vals == 0) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- part[" << (m_name ? m_name : "")
                << "]::get1DDistribution expected to retrieve "
                << mask.cnt() << (mask.cnt() > 1 ? "uint32_ts" : "uint32_t")
                << ", but got nothing";
            return -5;
        }
        else if (vals->size() != mask.cnt()) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- part[" << (m_name ? m_name : "")
                << "]::get1DDistribution expected to retrieve "
                << mask.cnt() << (mask.cnt() > 1 ? "uint32_ts" : "uint32_t")
                << ", but got " << vals->size() << " instead";
            delete vals;
            return -6;
        }

        uint32_t vmin = (*vals)[0];
        uint32_t vmax = (*vals)[0];
        for (uint32_t i = 1; i < vals->size(); ++ i) {
            if ((*vals)[i] > vmax)
                vmax = (*vals)[i];
            if ((*vals)[i] < vmin)
                vmin = (*vals)[i];
        }
        if (vmax-vmin < vals->size())
            ierr = adaptiveInts(*vals, vmin, vmax, nbins, bounds, counts);
        else
            ierr = adaptiveFloats(*vals, vmin, vmax, nbins, bounds, counts);
        delete vals;
        break;}
    case ibis::LONG: {
        array_t<int64_t> *vals = col->selectLongs(mask);
        if (vals == 0) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- part[" << (m_name ? m_name : "")
                << "]::get1DDistribution expected to retrieve "
                << mask.cnt() << (mask.cnt() > 1 ? "int64_ts" : "int64_t")
                << ", but got nothing";
            return -5;
        }
        else if (vals->size() != mask.cnt()) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- part[" << (m_name ? m_name : "")
                << "]::get1DDistribution expected to retrieve "
                << mask.cnt() << (mask.cnt() > 1 ? "int64_ts" : "int64_t")
                << ", but got " << vals->size() << " instead";
            delete vals;
            return -6;
        }

        int64_t vmin = (*vals)[0];
        int64_t vmax = (*vals)[0];
        for (uint32_t i = 1; i < vals->size(); ++ i) {
            if ((*vals)[i] > vmax)
                vmax = (*vals)[i];
            if ((*vals)[i] < vmin)
                vmin = (*vals)[i];
        }
        if (vmax-vmin < static_cast<int64_t>(vals->size()))
            ierr = adaptiveInts(*vals, vmin, vmax, nbins, bounds, counts);
        else
            ierr = adaptiveFloats(*vals, vmin, vmax, nbins, bounds, counts);
        delete vals;
        break;}
    case ibis::ULONG: {
        array_t<uint64_t> *vals = col->selectULongs(mask);
        if (vals == 0) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- part[" << (m_name ? m_name : "")
                << "]::get1DDistribution expected to retrieve "
                << mask.cnt() << (mask.cnt() > 1 ? "uint64_ts" : "uint64_t")
                << ", but got nothing";
            return -5;
        }
        else if (vals->size() != mask.cnt()) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- part[" << (m_name ? m_name : "")
                << "]::get1DDistribution expected to retrieve "
                << mask.cnt() << (mask.cnt() > 1 ? "uint64_ts" : "uint64_t")
                << ", but got " << vals->size() << " instead";
            delete vals;
            return -6;
        }

        uint64_t vmin = (*vals)[0];
        uint64_t vmax = (*vals)[0];
        for (uint32_t i = 1; i < vals->size(); ++ i) {
            if ((*vals)[i] > vmax)
                vmax = (*vals)[i];
            if ((*vals)[i] < vmin)
                vmin = (*vals)[i];
        }
        if (vmax-vmin < static_cast<uint64_t>(vals->size()))
            ierr = adaptiveInts(*vals, vmin, vmax, nbins, bounds, counts);
        else
            ierr = adaptiveFloats(*vals, vmin, vmax, nbins, bounds, counts);
        delete vals;
        break;}
    case ibis::FLOAT: {
        array_t<float> *vals = col->selectFloats(mask);
        if (vals == 0) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- part[" << (m_name ? m_name : "")
                << "]::get1DDistribution expected to retrieve "
                << mask.cnt() << (mask.cnt() > 1 ? "floats" : "float")
                << ", but got nothing";
            return -5;
        }
        else if (vals->size() != mask.cnt()) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- part[" << (m_name ? m_name : "")
                << "]::get1DDistribution expected to retrieve "
                << mask.cnt() << (mask.cnt() > 1 ? "floats" : "float")
                << ", but got " << vals->size() << " instead";
            delete vals;
            return -6;
        }

        float vmin = (*vals)[0];
        float vmax = (*vals)[0];
        for (uint32_t i = 1; i < vals->size(); ++ i) {
            if ((*vals)[i] > vmax)
                vmax = (*vals)[i];
            if ((*vals)[i] < vmin)
                vmin = (*vals)[i];
        }
        ierr = adaptiveFloats(*vals, vmin, vmax, nbins, bounds, counts);
        delete vals;
        break;}
    case ibis::DOUBLE: {
        array_t<double> *vals = col->selectDoubles(mask);
        if (vals == 0) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- part[" << (m_name ? m_name : "")
                << "]::get1DDistribution expected to retrieve "
                << mask.cnt() << (mask.cnt() > 1 ? "doubles" : "double")
                << ", but got nothing";
            return -5;
        }
        else if (vals->size() != mask.cnt()) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- part[" << (m_name ? m_name : "")
                << "]::get1DDistribution expected to retrieve "
                << mask.cnt() << (mask.cnt() > 1 ? "doubles" : "double")
                << ", but got " << vals->size() << " instead";
            delete vals;
            return -6;
        }

        double vmin = (*vals)[0];
        double vmax = (*vals)[0];
        for (uint32_t i = 1; i < vals->size(); ++ i) {
            if ((*vals)[i] > vmax)
                vmax = (*vals)[i];
            if ((*vals)[i] < vmin)
                vmin = (*vals)[i];
        }
        ierr = adaptiveFloats(*vals, vmin, vmax, nbins, bounds, counts);
        delete vals;
        break;}
    default: {
        LOGGER(ibis::gVerbose > 1)
            << "Warning -- part[" << (m_name ? m_name : "")
            << "]::get1DDistribution does not currently support column type "
            << ibis::TYPESTRING[(int) col->type()];
        return -7;}
    } // switch (col->type())
    if (ibis::gVerbose > 0) {
        timer.stop();
        ibis::util::logger lg;
        lg() << "part[" << (m_name ? m_name : "")
                    << "]::get1DDistribution computed histogram of column "
                    << cname;
        if (constraints != 0 && *constraints != 0)
            lg() << " subject to " << constraints;
        lg() << " in " << timer.CPUTime() << " sec(CPU), "
                    << timer.realTime() << " sec(elapsed)";
    }

    return ierr;
} // ibis::part::get1DDistribution

/// The adaptive binning function for integer values.  It is intended for
/// values within a relatively narrow range.  The input arguments vmin and
/// vmax must be the correct minimum and maximum values -- it uses the
/// minimum and maximum valuse to decided whether an exact histogram can be
/// used internally; incorrect values for vmin or vmax may cuase this
/// function to misbehave!
///
/// It counts the frequency of each distinct value before deciding how to
/// produce the equal-weight bins for output.  Because it has the most
/// detailed information possible, the output bins are mostly to be about
/// equal.  This comes with a cost of a detailed frequency count, which
/// takes time and memory space to compute.
///
/// @note The output number of bins may not be the input value nbins
/// because of following reasons.
/// - If nbins is 0 or 1, it is set to 1000 in this function.
/// - If nbins is larger than 2/3rds of the number of distinct values as
/// indicated by vmin and vmax, each value will have its own bin.
/// - In other cases, this function calls the function
/// ibis::index::divideCounts to determine how to coalesce different fine
/// bins into nbins bins on output.  However, it is possible that the
/// function ibis::index::divideCounts may have trouble produce exactly
/// nbins as requested.
template <typename T> long
ibis::part::adaptiveInts(const array_t<T> &vals, const T vmin, const T vmax,
                         uint32_t nbins, std::vector<double> &bounds,
                         std::vector<uint32_t> &counts) {
    if (vals.size() == 0) {
        return 0L;
    }
    if (vmin >= vmax) { // same min and max
        bounds.resize(2);
        counts.resize(1);
        bounds[0] = vmin;
        bounds[1] = vmin+1;
        counts[0] = vals.size();
        return 1L;
    }

    uint32_t nfine = static_cast<uint32_t>(1 + (vmax-vmin));
    LOGGER(ibis::gVerbose > 4)
        << "part::adaptiveInts<" << typeid(T).name() << "> counting "
        << nfine << " distinct values to compute " << nbins
        << " adaptively binned histogram in the range of [" << vmin
        << ", " << vmax << "]";

    array_t<uint32_t> fcnts(nfine, 0U);
    for (uint32_t i = 0; i < vals.size(); ++ i)
        ++ fcnts[(size_t)(vals[i]-vmin)];

    if (nbins <= 1) // too few bins, use 1000
        nbins = 1000;
    if (nbins > (nfine+nfine)/3) {
        bounds.resize(nfine+1);
        counts.resize(nfine);
        nbins = nfine;
        for (uint32_t i = 0; i < nfine; ++ i) {
            bounds[i] = static_cast<double>(vmin + i);
            counts[i] = fcnts[i];
        }
        bounds[nfine] = static_cast<double>(vmax+1);
    }
    else {
        array_t<uint32_t> fbnds(nbins);
        ibis::index::divideCounts(fbnds, fcnts);
        nbins = fbnds.size();
        bounds.resize(nbins+1);
        counts.resize(nbins);
        if (fcnts[0] > 0) {
            bounds[0] = static_cast<double>(vmin);
        }
        else {
            bool nonzero = false;
            for (uint32_t i = 0; i < fbnds[0]; ++ i) {
                if (fcnts[i] != 0) {
                    nonzero = true;
                    bounds[0] = static_cast<double>(vmin+i);
                }
            }
            if (! nonzero) // impossible
                bounds[0] = static_cast<double>(vmin);
        }
        bounds[1] = static_cast<double>(vmin+fbnds[0]);
        counts[0] = 0;
        for (uint32_t i = 0; i < fbnds[0]; ++ i)
            counts[0] += fcnts[i];
        for (uint32_t j = 1; j < nbins; ++ j) {
            bounds[j+1] = static_cast<double>(vmin+fbnds[j]);
            counts[j] = 0;
            for (uint32_t i = fbnds[j-1]; i < fbnds[j]; ++ i)
                counts[j] += fcnts[i];
        }
    }
    return nbins;
} // ibis::part::adaptiveInts

/// The adaptive binning function for floats and integers in wide ranges.
/// This function first constructs a number of fine uniform bins and then
/// merge the fine bins to generate nearly equal-weight bins.  This is
/// likely to produce final bins that are not as equal in their weights as
/// those produced from ibis::part::adaptiveInts, but because it usually
/// does less work and takes less time.
////
/// The number of fine bins used is the larger one of 8 times the number of
/// desired bins and the geometric mean of the number of desired bins and
/// the number of records in vals.
///
/// @note This function still relies on the caller to compute vmin and
/// vmax, but it assumes there are many distinct values in each bin.
///
/// @note The output number of bins may not be the input value nbins for
/// the following reasons.
/// - If nbins is 0 or 1, it is reset to 1000 in this function;
/// - if nbins is greater than 1/4 of vals.size(), it is set ot
///   vals.size()/4;
/// - in all other cases, the final number of bins is determine by the
///   function that partitions the fine bins into coarse bins,
///   ibis::index::divideCounts.  This partition process may not produce
///   exactly nbins bins.
///
/// @sa ibis::part::adaptiveInts.
template <typename T> long
ibis::part::adaptiveFloats(const array_t<T> &vals, const T vmin,
                           const T vmax, uint32_t nbins,
                           std::vector<double> &bounds,
                           std::vector<uint32_t> &counts) {
    if (vals.size() == 0) {
        return 0L;
    }
    if (vmax == vmin) {
        bounds.resize(2);
        counts.resize(1);
        bounds[0] = vmin;
        bounds[1] = ibis::util::incrDouble(vmin);
        counts[0] = vals.size();
        return 1L;
    }

    if (nbins <= 1)
        nbins = 1000;
    else if (nbins > 2048 && nbins > (vals.size() >> 2))
        nbins = (vals.size() >> 2);
    const uint32_t nfine = (vals.size()>8*nbins) ? static_cast<uint32_t>
        (sqrt(static_cast<double>(vals.size()) * nbins)) : 8*nbins;
    // try to make sure the 2nd bin boundary do not round down to a value
    // that is actually included in the 1st bin
    double scale = (1.0 - nfine * DBL_EPSILON) *
        ((double)nfine / (double)(vmax - vmin));
    LOGGER(ibis::gVerbose > 4)
        << "part::adaptiveFloats<" << typeid(T).name() << "> using "
        << nfine << " fine bins to compute " << nbins
        << " adaptively binned histogram in the range of [" << vmin
        << ", " << vmax << "] with fine bin size " << 1.0/scale;

    array_t<uint32_t> fcnts(nfine, 0);
    for (uint32_t i = 0; i < vals.size(); ++ i)
        ++ fcnts[static_cast<uint32_t>((vals[i]-vmin)*scale)];

    array_t<uint32_t> fbnds(nbins);
    ibis::index::divideCounts(fbnds, fcnts);
    nbins = fbnds.size();
    bounds.resize(nbins+1);
    counts.resize(nbins);
    bounds[0] = vmin;
    bounds[1] = vmin + 1.0 / static_cast<double>(scale);
    counts[0] = 0;
    for (uint32_t i = 0; i < fbnds[0]; ++ i)
        counts[0] += fcnts[i];
    for (uint32_t j = 1; j < nbins; ++ j) {
        bounds[j+1] = vmin + static_cast<double>(j+1) / scale;
        counts[j] = 0;
        for (uint32_t i = fbnds[j-1]; i < fbnds[j]; ++ i)
            counts[j] += fcnts[i];
    }
    return nbins;
} // ibis::part::adaptiveFloats

/// Bins the given values so that each each bin is nearly equal weight.
/// Instead of counting the number entries in each bin return bitvectors
/// that mark the positions of the records.  This version is for integer
/// values in relatively narrow ranges.  It will count each distinct value
/// separately, which gives the most accurate information for deciding how
/// to produce equal-weight bins.  If there are many dictinct values, this
/// function will require considerable amount of internal memory to count
/// each distinct value.
///
/// On successful completion of this function, the return value is the
/// number of bins used.  If the input array is empty, it returns 0 without
/// modifying the content of the output arrays, bounds and detail.  Either
/// mask and vals have the same number of elements, or vals has as many
/// elements as the number of ones (1) in mask, otherwise this function
/// will return -51.
///
/// @sa ibis::part::adaptiveInts
template <typename T> long
ibis::part::adaptiveIntsDetailed(const ibis::bitvector &mask,
                                 const array_t<T> &vals,
                                 const T vmin, const T vmax, uint32_t nbins,
                                 std::vector<double> &bounds,
                                 std::vector<ibis::bitvector> &detail) {
    if (mask.size() != vals.size() && mask.cnt() != vals.size())
        return -51L;
    if (vals.size() == 0) {
        return 0L;
    }
    if (vmin >= vmax) { // same min and max
        bounds.resize(2);
        detail.resize(1);
        bounds[0] = vmin;
        bounds[1] = vmin+1;
        detail[0].copy(mask);
        return 1L;
    }

    uint32_t nfine = static_cast<uint32_t>(1 + (vmax-vmin));
    LOGGER(ibis::gVerbose > 4)
        << "part::adaptiveIntsDetailed<" << typeid(T).name()
        << "> counting " << nfine << " distinct values to compute " << nbins
        << " adaptively binned histogram in the range of [" << vmin
        << ", " << vmax << "]";

    array_t<uint32_t> fcnts(nfine, 0U);
    std::vector<ibis::bitvector*> pos(nfine);
    for (uint32_t i = 0; i < nfine; ++ i)
        pos[i] = new ibis::bitvector;
    if (mask.cnt() == vals.size()) {
        uint32_t j = 0; // index into array vals
        for (ibis::bitvector::indexSet is = mask.firstIndexSet();
             is.nIndices() > 0; ++ is) {
            const uint32_t nind = is.nIndices();
            const ibis::bitvector::word_t *idx = is.indices();
            if (is.isRange()) {
                for (uint32_t i = *idx; i < idx[1]; ++ i) {
                    const T ifine = vals[j] - vmin;
                    ++ j;
                    ++ fcnts[ifine];
                    pos[ifine]->setBit(i, 1);
                }
            }
            else {
                for (uint32_t i = 0; i < nind; ++ i) {
                    const T ifine = vals[j] - vmin;
                    ++ j;
                    ++ fcnts[ifine];
                    pos[ifine]->setBit(idx[i], 1);
                }
            }
        }
    }
    else {
        for (ibis::bitvector::indexSet is = mask.firstIndexSet();
             is.nIndices() > 0; ++ is) {
            const uint32_t nind = is.nIndices();
            const ibis::bitvector::word_t *idx = is.indices();
            if (is.isRange()) {
                for (uint32_t i = *idx; i < idx[1]; ++ i) {
                    const T ifine = vals[i] - vmin;
                    ++ fcnts[ifine];
                    pos[ifine]->setBit(i, 1);
                }
            }
            else {
                for (uint32_t i = 0; i < nind; ++ i) {
                    const ibis::bitvector::word_t j = idx[i];
                    const T ifine = vals[j] - vmin;
                    ++ fcnts[ifine];
                    pos[ifine]->setBit(j, 1);
                }
            }
        }
    }
    for (uint32_t i = 0; i < nfine; ++ i)
        pos[i]->adjustSize(0, mask.size());

    if (nbins <= 1) // too few bins, use 1000
        nbins = 1000;
    if (nbins > (nfine+nfine)/3) {
        bounds.resize(nfine+1);
        detail.resize(nfine);
        nbins = nfine;
        for (uint32_t i = 0; i < nfine; ++ i) {
            bounds[i] = static_cast<double>(vmin + i);
            detail[i].swap(*pos[i]);
        }
        bounds[nfine] = static_cast<double>(vmax+1);
    }
    else {
        array_t<uint32_t> fbnds(nbins);
        ibis::index::divideCounts(fbnds, fcnts);
        nbins = fbnds.size();
        bounds.resize(nbins+1);
        detail.resize(nbins);
        if (fcnts[0] > 0) {
            bounds[0] = static_cast<double>(vmin);
        }
        else {
            bool nonzero = false;
            for (uint32_t i = 0; i < fbnds[0]; ++ i) {
                if (fcnts[i] != 0) {
                    nonzero = true;
                    bounds[0] = static_cast<double>(vmin+i);
                }
            }
            if (! nonzero) // should never be true
                bounds[0] = static_cast<double>(vmin);
        }
        bounds[1] = static_cast<double>(vmin+fbnds[0]);
        if (fbnds[0] > 1) {
            ibis::index::sumBits(pos, 0, fbnds[0], detail[0]);
            detail[0].compress();
        }
        else {
            detail[0].swap(*pos[0]);
        }
        for (uint32_t j = 1; j < nbins; ++ j) {
            bounds[j+1] = static_cast<double>(vmin+fbnds[j]);
            if (fbnds[j] > fbnds[j-1]+1) {
                ibis::index::sumBits(pos, fbnds[j-1], fbnds[j], detail[j]);
                detail[j].compress();
            }
            else {
                detail[j].swap(*pos[fbnds[j-1]]);
            }
        }
    }

    for (uint32_t i = 0; i < nfine; ++ i)
        delete pos[i];
    return detail.size();
} // ibis::part::adaptiveIntsDetailed

/// Bins the given values so that each each bin is nearly equal weight.
/// Instead of counting the number entries in each bin return bitvectors
/// that mark the positions of the records.  This version is for
/// floating-point values and integer values with wide ranges.  This
/// function first bins the values into a relatively large number of fine
/// equal-width bins and then coalesce nearby fines bins to for nearly
/// equal-weight bins.  The final bins produced this way are less likely to
/// be very uniform in their weights, but it requires less internal work
/// space and therefore may be faster than
/// ibis::part::adaptiveIntsDetailed.
///
/// @sa ibis::part::adapativeFloats
template <typename T> long
ibis::part::adaptiveFloatsDetailed(const ibis::bitvector &mask,
                                   const array_t<T> &vals, const T vmin,
                                   const T vmax, uint32_t nbins,
                                   std::vector<double> &bounds,
                                   std::vector<ibis::bitvector> &detail) {
    if (mask.size() != vals.size() && mask.cnt() != vals.size())
        return -51L;
    if (vals.size() == 0) {
        return 0L;
    }
    if (vmax == vmin) {
        bounds.resize(2);
        detail.resize(1);
        bounds[0] = vmin;
        bounds[1] = ibis::util::incrDouble(vmin);
        detail[0].copy(mask);
        return 1L;
    }

    if (nbins <= 1) // default to 1000 bins
        nbins = 1000;
    else if (nbins > 2048 && nbins > (vals.size() >> 2))
        nbins = (vals.size() >> 2);
    const uint32_t nfine = (vals.size()>8*nbins) ? static_cast<uint32_t>
        (sqrt(static_cast<double>(vals.size()) * nbins)) : 8*nbins;
    // try to make sure the 2nd bin boundary do not round down to a value
    // that is actually included in the 1st bin
    double scale = 1.0 /
        (ibis::util::incrDouble((double)vmin + (double)(vmax - vmin) /
                                nfine) - vmin);
    LOGGER(ibis::gVerbose > 4)
        << "part::adaptiveFloatsDetailed<" << typeid(T).name()
        << "> using " << nfine << " fine bins to compute " << nbins
        << " adaptively binned histogram in the range of [" << vmin
        << ", " << vmax << "] with fine bin size " << 1.0/scale;

    array_t<uint32_t> fcnts(nfine, 0);
    std::vector<ibis::bitvector*> pos(nfine);
    for (uint32_t i = 0; i < nfine; ++ i)
        pos[i] = new ibis::bitvector;
    if (mask.cnt() == vals.size()) {
        uint32_t j = 0; // index into array vals
        for (ibis::bitvector::indexSet is = mask.firstIndexSet();
             is.nIndices() > 0; ++ is) {
            const uint32_t nind = is.nIndices();
            const ibis::bitvector::word_t *idx = is.indices();
            if (is.isRange()) {
                for (uint32_t i = *idx; i < idx[1]; ++ i) {
                    const uint32_t ifine =
                        static_cast<uint32_t>((vals[j]-vmin)*scale);
                    ++ j;
                    ++ fcnts[ifine];
                    pos[ifine]->setBit(i, 1);
                }
            }
            else {
                for (uint32_t i = 0; i < nind; ++ i) {
                    const uint32_t ifine =
                        static_cast<uint32_t>((vals[j]-vmin)*scale);
                    ++ j;
                    ++ fcnts[ifine];
                    pos[ifine]->setBit(idx[i], 1);
                }
            }
        }
    }
    else {
        for (ibis::bitvector::indexSet is = mask.firstIndexSet();
             is.nIndices() > 0; ++ is) {
            const uint32_t nind = is.nIndices();
            const ibis::bitvector::word_t *idx = is.indices();
            if (is.isRange()) {
                for (uint32_t i = *idx; i < idx[1]; ++ i) {
                    const uint32_t ifine =
                        static_cast<uint32_t>((vals[i]-vmin)*scale);
                    ++ fcnts[ifine];
                    pos[ifine]->setBit(i, 1);
                }
            }
            else {
                for (uint32_t i = 0; i < nind; ++ i) {
                    const ibis::bitvector::word_t j = idx[i];
                    const uint32_t ifine =
                        static_cast<uint32_t>((vals[j]-vmin)*scale);
                    ++ fcnts[ifine];
                    pos[ifine]->setBit(j, 1);
                }
            }
        }
    }
    for (uint32_t i = 0; i < nfine; ++ i)
        pos[i]->adjustSize(0, mask.size());

    array_t<uint32_t> fbnds(nbins);
    ibis::index::divideCounts(fbnds, fcnts);
    nbins = fbnds.size();
    bounds.resize(nbins+1);
    detail.resize(nbins);
    bounds[0] = vmin;
    bounds[1] = vmin + 1.0 / scale;
    if (fbnds[0] > 1) {
        ibis::index::sumBits(pos, 0, fbnds[0], detail[0]);
        detail[0].compress();
    }
    else {
        detail[0].swap(*pos[0]);
    }
    for (uint32_t j = 1; j < nbins; ++ j) {
        bounds[j+1] = vmin + static_cast<double>(j+1) / scale;
        if (fbnds[j+1] > fbnds[j]+1) {
            ibis::index::sumBits(pos, fbnds[j-1], fbnds[j], detail[j]);
            detail[j].compress();
        }
        else {
            detail[j].swap(*pos[fbnds[j-1]]);
        }
    }

    for (uint32_t i = 0; i < nfine; ++ i)
        delete pos[i];
    return detail.size();
} // ibis::part::adaptiveFloatsDetailed

/// This function makes use of an existing index to produce bitmaps
/// representing a set of bins defined by bnds.  Following the private
/// convention used in FastBit, there are two open bins at the two ends.
int ibis::part::coarsenBins(const ibis::column &col, uint32_t nbin,
                            std::vector<double> &bnds,
                            std::vector<ibis::bitvector*> &btmp) const {
    ibis::column::indexLock lock(&col, "part::coarsenBins");
    const ibis::index* idx = lock.getIndex();
    if (idx == 0) {
        LOGGER(ibis::gVerbose > 2)
            << "part[" << (m_name ? m_name : "")
            << "]::coarsenBins can not proceed with index for "
            << col.name();
        return -1;
    }

    array_t<uint32_t> wbnds(nbin);
    // retrieve bins used by idx
    std::vector<double> idxbin;
    idx->binBoundaries(idxbin);
    const double maxval = col.getActualMax();
    while (idxbin.size() > 1 && idxbin.back() >= maxval)
        idxbin.pop_back();
    if (idxbin.empty()) { // too few bins to be interesting
        LOGGER(ibis::gVerbose > 2)
            << "part[" << (m_name ? m_name : "")
            << "]::coarsenBins can not proceed because column "
            << col.name() << " has either no valid values or a single value";
        return -2;
    }
    if (idxbin.size() > nbin*3/2) { // coarsen the bins
        std::vector<uint32_t> idxwgt;
        idx->binWeights(idxwgt);
        if (idxwgt.size() < idxbin.size()) {
            LOGGER(ibis::gVerbose > 2)
                << "part[" << (m_name ? m_name : "")
                << "]::coarsenBins failed to count the values of "
                << col.name();
            return -3;
        }

        array_t<uint32_t> wgt2(idxwgt.size());
        std::copy(idxwgt.begin(), idxwgt.end(), wgt2.begin());

        ibis::index::divideCounts(wbnds, wgt2);
        while (wbnds.size() > 1 && wbnds[wbnds.size()-2] >= idxbin.size())
            wbnds.pop_back();
        if (wbnds.size() < 2) {
            LOGGER(ibis::gVerbose > 2)
                << "part[" << (m_name ? m_name : "")
                << "]::coarsenBins failed to divide the values into "
                << nbin << " bins";
            return -4;
        }
    }
    else { // no need to coarsen anything
        wbnds.resize(idxbin.size());
        for (unsigned i = 0; i < idxbin.size(); ++ i)
            wbnds[i] = i+1;
    }

    bnds.resize(wbnds.size());
    btmp.reserve(wbnds.size());
    // first bin: open to the left
    bnds[0] = idxbin[wbnds[0]];
    ibis::qContinuousRange rng(col.name(), ibis::qExpr::OP_LT, bnds[0]);
    ibis::bitvector bv;
    LOGGER(ibis::gVerbose > 5)
        << "part[" << (m_name ? m_name : "")
        << "]::coarsenBins evaluating " << rng << " for bin 0 in "
        << col.name();
    long ierr = idx->evaluate(rng, bv);
    if (ierr < 0) {
        LOGGER(ibis::gVerbose > 2)
            << "part[" << (m_name ? m_name : "")
            << "]::coarsenBins failed to evaluate query " << rng
            << ", ierr=" << ierr;
        return -6;
    }
    btmp.push_back(new ibis::bitvector(bv));

    // middle bins: two-sided, inclusive left, exclusive right
    rng.leftOperator() = ibis::qExpr::OP_LE;
    rng.rightOperator() = ibis::qExpr::OP_LT;
    for (unsigned i = 1; i < wbnds.size()-1; ++ i) {
        rng.leftBound() = idxbin[wbnds[i-1]];
        rng.rightBound() = idxbin[wbnds[i]];
        bnds[i] = idxbin[wbnds[i]];
        LOGGER(ibis::gVerbose > 5)
            << "part[" << (m_name ? m_name : "")
            << "]::coarsenBins evaluating " << rng << " for bin "
            << i << " in " << col.name();

        ierr = idx->evaluate(rng, bv);
        if (ierr < 0) {
            LOGGER(ibis::gVerbose > 2)
                << "part[" << (m_name ? m_name : "")
                << "]::coarsenBins failed to evaluate query " << rng
                << ", ierr=" << ierr;
            return -6;
        }

        btmp.push_back(new ibis::bitvector(bv));
    }
    bnds.resize(wbnds.size()-1); // remove the last element

    // last bin: open to the right
    rng.rightOperator() = ibis::qExpr::OP_UNDEFINED;
    rng.leftBound() = idxbin[wbnds[wbnds.size()-2]];
    LOGGER(ibis::gVerbose > 5)
        << "part[" << (m_name ? m_name : "")
        << "]::coarsenBins evaluating " << rng << " for bin "
        << wbnds.size()-1 << " in " << col.name();
    ierr = idx->evaluate(rng, bv);
    if (ierr < 0) {
        LOGGER(ibis::gVerbose > 2)
            << "part[" << (m_name ? m_name : "")
            << "]::coarsenBins failed to evaluate query " << rng
            << ", ierr=" << ierr;
        return -6;
    }

    btmp.push_back(new ibis::bitvector(bv));
    return btmp.size();
} // ibis::part::coarsenBins

/// Based on the column type, decide how to retrieve the values and
/// invokethe lower level support functions.
long ibis::part::get1DBins_(const ibis::bitvector &mask,
                            const ibis::column &col,
                            uint32_t nbin, std::vector<double> &bounds,
                            std::vector<ibis::bitvector> &bins,
                            const char *mesg) const {
    if (mask.cnt() == 0) return 0L;
    if (mask.size() != nEvents) return -6L;
    if (mesg == 0 || *mesg == 0)
        mesg = ibis::util::userName();
    LOGGER(ibis::gVerbose > 3)
        << mesg << " -- invoking get1DBins_ on column " << col.name()
        << " type " << ibis::TYPESTRING[(int)col.type()] << "(" << col.type()
        << ") with mask of " << mask.cnt() << " out of " << mask.size();

    long ierr;
    switch (col.type()) {
    default: {
        LOGGER(ibis::gVerbose > 0)
            << mesg << " -- can not work with column " << col.name()
            << " of type " << ibis::TYPESTRING[(int)col.type()] << "("
            << col.type() << ")";
        ierr = -7;
        break;}
    case ibis::BYTE: {
        signed char vmin, vmax;
        array_t<signed char> *vals=0;
        ibis::fileManager::ACCESS_PREFERENCE acc = accessHint(mask, 1);
        if (acc == ibis::fileManager::PREFER_READ) {
            vals = new array_t<signed char>;
            ierr = col.getValuesArray(vals);
            if (ierr < 0) {
                LOGGER(ibis::gVerbose > 1)
                    << "Warning - " << mesg
                    << " failed to retrieve any values for column "
                    << col.name();
                delete vals;
                return -8L;
            }
            else if (vals->size() != nEvents) {
                LOGGER(ibis::gVerbose > 1)
                    << "Warning -- " << mesg << " expected to retrieve "
                    << nEvents << " byte" << (nEvents > 1 ? "s" : "")
                    << ", but got " << vals->size();
                delete vals;
                return -9L;
            }
            vmin = 127; //std::numeric_limits<char>::max();
            vmax = -128; //std::numeric_limits<char>::lowest();
            for (ibis::bitvector::indexSet is = mask.firstIndexSet();
                 is.nIndices() > 0; ++ is) {
                const ibis::bitvector::word_t nind = is.nIndices();
                const ibis::bitvector::word_t *idx = is.indices();
                if (is.isRange()) {
                    for (ibis::bitvector::word_t ii = *idx;
                         ii < idx[1]; ++ ii) {
                        if (vmin > (*vals)[ii])
                            vmin = (*vals)[ii];
                        if (vmax < (*vals)[ii])
                            vmax = (*vals)[ii];
                    }
                }
                else {
                    for (uint32_t ii = 0; ii < nind; ++ ii) {
                        const uint32_t jj = idx[ii];
                        if (vmin > (*vals)[jj])
                            vmin = (*vals)[jj];
                        if (vmax < (*vals)[jj])
                            vmax = (*vals)[jj];
                    }
                }
            }
        }
        else {
            ibis::bitvector::word_t nsel = mask.cnt();
            vals = col.selectBytes(mask);
            if (vals == 0) {
                LOGGER(ibis::gVerbose > 1)
                    << "Warning -- " << mesg
                    << " failed to retrieve any values for column "
                    << col.name();
                return -10L;
            }
            else if (vals->size() != nsel) {
                LOGGER(ibis::gVerbose > 1)
                    << "Warning -- " << mesg << " expected to retrieve " << nsel
                    << " byte" << (nsel > 1 ? "s" : "") << ", but got "
                    << vals->size();
                delete vals;
                return -11L;
            }
            vmin = (*vals)[0];
            vmax = (*vals)[0];
            for (uint32_t i = 1; i < nsel; ++ i) {
                if (vmin > (*vals)[i])
                    vmin = (*vals)[i];
                if (vmax < (*vals)[i])
                    vmax = (*vals)[i];
            }
        }
        ierr = adaptiveIntsDetailed(mask, *vals, vmin, vmax, nbin,
                                    bounds, bins);
        delete vals;
        break;}
    case ibis::UBYTE: {
        unsigned char vmin, vmax;
        array_t<unsigned char> *vals=0;
        ibis::fileManager::ACCESS_PREFERENCE acc = accessHint(mask, 1);
        if (acc == ibis::fileManager::PREFER_READ) {
            vals = new array_t<unsigned char>;
            ierr = col.getValuesArray(vals);
            if (ierr < 0) {
                LOGGER(ibis::gVerbose > 1)
                    << "Warning -- " << mesg
                    << " failed to retrieve any values for column "
                    << col.name();
                delete vals;
                return -12L;
            }
            else if (vals->size() != nEvents) {
                LOGGER(ibis::gVerbose > 1)
                    << "Warning -- " << mesg << " expected to retrieve "
                    << nEvents << " byte" << (nEvents > 1 ? "s" : "")
                    << ", but got " << vals->size();
                delete vals;
                return -13L;
            }
            vmin = 255;
            vmax = 0;
            for (ibis::bitvector::indexSet is = mask.firstIndexSet();
                 is.nIndices() > 0; ++ is) {
                const ibis::bitvector::word_t nind = is.nIndices();
                const ibis::bitvector::word_t *idx = is.indices();
                if (is.isRange()) {
                    for (ibis::bitvector::word_t ii = *idx;
                         ii < idx[1]; ++ ii) {
                        if (vmin > (*vals)[ii])
                            vmin = (*vals)[ii];
                        if (vmax < (*vals)[ii])
                            vmax = (*vals)[ii];
                    }
                }
                else {
                    for (uint32_t ii = 0; ii < nind; ++ ii) {
                        const uint32_t jj = idx[ii];
                        if (vmin > (*vals)[jj])
                            vmin = (*vals)[jj];
                        if (vmax < (*vals)[jj])
                            vmax = (*vals)[jj];
                    }
                }
            }
        }
        else {
            ibis::bitvector::word_t nsel = mask.cnt();
            vals = col.selectUBytes(mask);
            if (vals == 0) {
                LOGGER(ibis::gVerbose > 1)
                    << "Warning -- " << mesg
                    << " failed to retrieve any values for column "
                    << col.name();
                return -14L;
            }
            else if (vals->size() != nsel) {
                LOGGER(ibis::gVerbose > 1)
                    << "Warning -- " << mesg << " expected to retrieve " << nsel
                    << " byte" << (nsel > 1 ? "s" : "") << ", but got "
                    << vals->size();
                delete vals;
                return -15L;
            }
            vmin = (*vals)[0];
            vmax = (*vals)[0];
            for (uint32_t i = 1; i < nsel; ++ i) {
                if (vmin > (*vals)[i])
                    vmin = (*vals)[i];
                if (vmax < (*vals)[i])
                    vmax = (*vals)[i];
            }
        }
        ierr = adaptiveIntsDetailed(mask, *vals, vmin, vmax, nbin,
                                    bounds, bins);
        delete vals;
        break;}
    case ibis::SHORT: {
        int16_t vmin, vmax;
        array_t<int16_t> *vals=0;
        ibis::fileManager::ACCESS_PREFERENCE acc = accessHint(mask, 1);
        if (acc == ibis::fileManager::PREFER_READ) {
            vals = new array_t<int16_t>;
            ierr = col.getValuesArray(vals);
            if (ierr < 0) {
                LOGGER(ibis::gVerbose > 1)
                    << "Warning -- " << mesg
                    << " failed to retrieve any values for column "
                    << col.name();
                delete vals;
                return -16L;
            }
            else if (vals->size() != nEvents) {
                LOGGER(ibis::gVerbose > 1)
                    << "Warning -- " << mesg << " expected to retrieve "
                    << nEvents << " byte" << (nEvents > 1 ? "s" : "")
                    << ", but got " << vals->size();
                delete vals;
                return -17L;
            }
            vmin = std::numeric_limits<int16_t>::max();
            vmax = std::numeric_limits<int16_t>::min();
            for (ibis::bitvector::indexSet is = mask.firstIndexSet();
                 is.nIndices() > 0; ++ is) {
                const ibis::bitvector::word_t nind = is.nIndices();
                const ibis::bitvector::word_t *idx = is.indices();
                if (is.isRange()) {
                    for (ibis::bitvector::word_t ii = *idx;
                         ii < idx[1]; ++ ii) {
                        if (vmin > (*vals)[ii])
                            vmin = (*vals)[ii];
                        if (vmax < (*vals)[ii])
                            vmax = (*vals)[ii];
                    }
                }
                else {
                    for (uint32_t ii = 0; ii < nind; ++ ii) {
                        const uint32_t jj = idx[ii];
                        if (vmin > (*vals)[jj])
                            vmin = (*vals)[jj];
                        if (vmax < (*vals)[jj])
                            vmax = (*vals)[jj];
                    }
                }
            }
        }
        else {
            ibis::bitvector::word_t nsel = mask.cnt();
            vals = col.selectShorts(mask);
            if (vals == 0) {
                LOGGER(ibis::gVerbose > 1)
                    << "Warning -- " << mesg
                    << " failed to retrieve any values for column "
                    << col.name();
                return -18L;
            }
            else if (vals->size() != nsel) {
                LOGGER(ibis::gVerbose > 1)
                    << "Warning -- " << mesg << " expected to retrieve " << nsel
                    << " byte" << (nsel > 1 ? "s" : "") << ", but got "
                    << vals->size();
                delete vals;
                return -19L;
            }
            vmin = (*vals)[0];
            vmax = (*vals)[0];
            for (uint32_t i = 1; i < nsel; ++ i) {
                if (vmin > (*vals)[i])
                    vmin = (*vals)[i];
                if (vmax < (*vals)[i])
                    vmax = (*vals)[i];
            }
        }
        ierr = adaptiveIntsDetailed(mask, *vals, vmin, vmax, nbin,
                                    bounds, bins);
        delete vals;
        break;}
    case ibis::USHORT: {
        uint16_t vmin, vmax;
        array_t<uint16_t> *vals=0;
        ibis::fileManager::ACCESS_PREFERENCE acc = accessHint(mask, 1);
        if (acc == ibis::fileManager::PREFER_READ) {
            vals = new array_t<uint16_t>;
            ierr = col.getValuesArray(vals);
            if (ierr < 0) {
                LOGGER(ibis::gVerbose > 1)
                    << "Warning -- " << mesg
                    << " failed to retrieve any values for column "
                    << col.name();
                delete vals;
                return -20L;
            }
            else if (vals->size() != nEvents) {
                LOGGER(ibis::gVerbose > 1)
                    << "Warning -- " << mesg << " expected to retrieve "
                    << nEvents << " byte" << (nEvents > 1 ? "s" : "")
                    << ", but got " << vals->size();
                delete vals;
                return -21L;
            }
            vmin = std::numeric_limits<uint16_t>::max();
            vmax = 0;
            for (ibis::bitvector::indexSet is = mask.firstIndexSet();
                 is.nIndices() > 0; ++ is) {
                const ibis::bitvector::word_t nind = is.nIndices();
                const ibis::bitvector::word_t *idx = is.indices();
                if (is.isRange()) {
                    for (ibis::bitvector::word_t ii = *idx;
                         ii < idx[1]; ++ ii) {
                        if (vmin > (*vals)[ii])
                            vmin = (*vals)[ii];
                        if (vmax < (*vals)[ii])
                            vmax = (*vals)[ii];
                    }
                }
                else {
                    for (uint32_t ii = 0; ii < nind; ++ ii) {
                        const uint32_t jj = idx[ii];
                        if (vmin > (*vals)[jj])
                            vmin = (*vals)[jj];
                        if (vmax < (*vals)[jj])
                            vmax = (*vals)[jj];
                    }
                }
            }
        }
        else {
            ibis::bitvector::word_t nsel = mask.cnt();
            vals = col.selectUShorts(mask);
            if (vals == 0) {
                LOGGER(ibis::gVerbose > 1)
                    << "Warning -- " << mesg
                    << " failed to retrieve any values for column "
                    << col.name();
                return -22L;
            }
            else if (vals->size() != nsel) {
                LOGGER(ibis::gVerbose > 1)
                    << "Warning -- " << mesg << " expected to retrieve " << nsel
                    << " byte" << (nsel > 1 ? "s" : "") << ", but got "
                    << vals->size();
                delete vals;
                return -23L;
            }
            vmin = (*vals)[0];
            vmax = (*vals)[0];
            for (uint32_t i = 1; i < nsel; ++ i) {
                if (vmin > (*vals)[i])
                    vmin = (*vals)[i];
                if (vmax < (*vals)[i])
                    vmax = (*vals)[i];
            }
        }
        ierr = adaptiveIntsDetailed(mask, *vals, vmin, vmax, nbin,
                                    bounds, bins);
        delete vals;
        break;}
    case ibis::INT: {
        int32_t vmin, vmax;
        array_t<int32_t> *vals=0;
        ibis::fileManager::ACCESS_PREFERENCE acc = accessHint(mask, 1);
        if (acc == ibis::fileManager::PREFER_READ) {
            vals = new array_t<int32_t>;
            ierr = col.getValuesArray(vals);
            if (ierr < 0) {
                LOGGER(ibis::gVerbose > 1)
                    << "Warning -- " << mesg
                    << " failed to retrieve any values for column "
                    << col.name();
                delete vals;
                return -24L;
            }
            else if (vals->size() != nEvents) {
                LOGGER(ibis::gVerbose > 1)
                    << "Warning -- " << mesg << " expected to retrieve "
                    << nEvents << " byte" << (nEvents > 1 ? "s" : "")
                    << ", but got " << vals->size();
                delete vals;
                return -25L;
            }
            vmin = std::numeric_limits<int32_t>::max();
            vmax = std::numeric_limits<int32_t>::min();
            for (ibis::bitvector::indexSet is = mask.firstIndexSet();
                 is.nIndices() > 0; ++ is) {
                const ibis::bitvector::word_t nind = is.nIndices();
                const ibis::bitvector::word_t *idx = is.indices();
                if (is.isRange()) {
                    for (ibis::bitvector::word_t ii = *idx;
                         ii < idx[1]; ++ ii) {
                        if (vmin > (*vals)[ii])
                            vmin = (*vals)[ii];
                        if (vmax < (*vals)[ii])
                            vmax = (*vals)[ii];
                    }
                }
                else {
                    for (uint32_t ii = 0; ii < nind; ++ ii) {
                        const uint32_t jj = idx[ii];
                        if (vmin > (*vals)[jj])
                            vmin = (*vals)[jj];
                        if (vmax < (*vals)[jj])
                            vmax = (*vals)[jj];
                    }
                }
            }
        }
        else {
            ibis::bitvector::word_t nsel = mask.cnt();
            vals = col.selectInts(mask);
            if (vals == 0) {
                LOGGER(ibis::gVerbose > 1)
                    << "Warning -- " << mesg
                    << " failed to retrieve any values for column "
                    << col.name();
                return -26L;
            }
            else if (vals->size() != nsel) {
                LOGGER(ibis::gVerbose > 1)
                    << "Warning -- " << mesg << " expected to retrieve " << nsel
                    << " byte" << (nsel > 1 ? "s" : "") << ", but got "
                    << vals->size();
                delete vals;
                return -27L;
            }
            vmin = (*vals)[0];
            vmax = (*vals)[0];
            for (uint32_t i = 1; i < nsel; ++ i) {
                if (vmin > (*vals)[i])
                    vmin = (*vals)[i];
                if (vmax < (*vals)[i])
                    vmax = (*vals)[i];
            }
        }
        if (static_cast<uint32_t>(vmax-vmin) < vals->size()) {
            ierr = adaptiveIntsDetailed(mask, *vals, vmin, vmax, nbin,
                                        bounds, bins);
        }
        else {
            ierr = adaptiveFloatsDetailed(mask, *vals, vmin, vmax, nbin,
                                          bounds, bins);
            for (uint32_t i = 0; i < bounds.size(); ++ i)
                bounds[i] = ceil(bounds[i]);
        }
        delete vals;
        break;}
    case ibis::CATEGORY:
    case ibis::UINT: {
        uint32_t vmin, vmax;
        array_t<uint32_t> *vals=0;
        ibis::fileManager::ACCESS_PREFERENCE acc = accessHint(mask, 1);
        if (acc == ibis::fileManager::PREFER_READ) {
            vals = new array_t<uint32_t>;
            ierr = col.getValuesArray(vals);
            if (ierr < 0) {
                LOGGER(ibis::gVerbose > 1)
                    << "Warning -- " << mesg
                    << " failed to retrieve any values for column "
                    << col.name();
                delete vals;
                return -28L;
            }
            else if (vals->size() != nEvents) {
                LOGGER(ibis::gVerbose > 1)
                    << "Warning -- " << mesg << " expected to retrieve "
                    << nEvents << " byte" << (nEvents > 1 ? "s" : "")
                    << ", but got " << vals->size();
                delete vals;
                return -29L;
            }
            vmin = std::numeric_limits<uint32_t>::max();
            vmax = 0;
            for (ibis::bitvector::indexSet is = mask.firstIndexSet();
                 is.nIndices() > 0; ++ is) {
                const ibis::bitvector::word_t nind = is.nIndices();
                const ibis::bitvector::word_t *idx = is.indices();
                if (is.isRange()) {
                    for (ibis::bitvector::word_t ii = *idx;
                         ii < idx[1]; ++ ii) {
                        if (vmin > (*vals)[ii])
                            vmin = (*vals)[ii];
                        if (vmax < (*vals)[ii])
                            vmax = (*vals)[ii];
                    }
                }
                else {
                    for (uint32_t ii = 0; ii < nind; ++ ii) {
                        const uint32_t jj = idx[ii];
                        if (vmin > (*vals)[jj])
                            vmin = (*vals)[jj];
                        if (vmax < (*vals)[jj])
                            vmax = (*vals)[jj];
                    }
                }
            }
        }
        else {
            ibis::bitvector::word_t nsel = mask.cnt();
            vals = col.selectUInts(mask);
            if (vals == 0) {
                LOGGER(ibis::gVerbose > 1)
                    << "Warning -- " << mesg
                    << " failed to retrieve any values for column "
                    << col.name();
                return -30L;
            }
            else if (vals->size() != nsel) {
                LOGGER(ibis::gVerbose > 1)
                    << "Warning -- " << mesg << " expected to retrieve " << nsel
                    << " byte" << (nsel > 1 ? "s" : "") << ", but got "
                    << vals->size();
                delete vals;
                return -31L;
            }
            vmin = (*vals)[0];
            vmax = (*vals)[0];
            for (uint32_t i = 1; i < nsel; ++ i) {
                if (vmin > (*vals)[i])
                    vmin = (*vals)[i];
                if (vmax < (*vals)[i])
                    vmax = (*vals)[i];
            }
        }
        if (vmax - vmin < vals->size()) {
            ierr = adaptiveIntsDetailed(mask, *vals, vmin, vmax, nbin,
                                        bounds, bins);
        }
        else {
            ierr = adaptiveFloatsDetailed(mask, *vals, vmin, vmax, nbin,
                                          bounds, bins);
            for (uint32_t i = 0; i < bounds.size(); ++ i)
                bounds[i] = ceil(bounds[i]);
        }
        delete vals;
        break;}
    case ibis::LONG: {
        int64_t vmin, vmax;
        array_t<int64_t> *vals=0;
        ibis::fileManager::ACCESS_PREFERENCE acc = accessHint(mask, 1);
        if (acc == ibis::fileManager::PREFER_READ) {
            vals = new array_t<int64_t>;
            ierr = col.getValuesArray(vals);
            if (ierr < 0) {
                LOGGER(ibis::gVerbose > 1)
                    << "Warning -- " << mesg
                    << " failed to retrieve any values for column "
                    << col.name();
                delete vals;
                return -32L;
            }
            else if (vals->size() != nEvents) {
                LOGGER(ibis::gVerbose > 1)
                    << "Warning -- " << mesg << " expected to retrieve "
                    << nEvents << " byte" << (nEvents > 1 ? "s" : "")
                    << ", but got " << vals->size();
                delete vals;
                return -33L;
            }
            vmin = std::numeric_limits<int64_t>::max();
            vmax = std::numeric_limits<int64_t>::min();
            for (ibis::bitvector::indexSet is = mask.firstIndexSet();
                 is.nIndices() > 0; ++ is) {
                const ibis::bitvector::word_t nind = is.nIndices();
                const ibis::bitvector::word_t *idx = is.indices();
                if (is.isRange()) {
                    for (ibis::bitvector::word_t ii = *idx;
                         ii < idx[1]; ++ ii) {
                        if (vmin > (*vals)[ii])
                            vmin = (*vals)[ii];
                        if (vmax < (*vals)[ii])
                            vmax = (*vals)[ii];
                    }
                }
                else {
                    for (uint32_t ii = 0; ii < nind; ++ ii) {
                        const uint32_t jj = idx[ii];
                        if (vmin > (*vals)[jj])
                            vmin = (*vals)[jj];
                        if (vmax < (*vals)[jj])
                            vmax = (*vals)[jj];
                    }
                }
            }
        }
        else {
            ibis::bitvector::word_t nsel = mask.cnt();
            vals = col.selectLongs(mask);
            if (vals == 0) {
                LOGGER(ibis::gVerbose > 1)
                    << "Warning -- " << mesg
                    << " failed to retrieve any values for column "
                    << col.name();
                return -34L;
            }
            else if (vals->size() != nsel) {
                LOGGER(ibis::gVerbose > 1)
                    << "Warning -- " << mesg << " expected to retrieve " << nsel
                    << " byte" << (nsel > 1 ? "s" : "") << ", but got "
                    << vals->size();
                delete vals;
                return -35L;
            }
            vmin = (*vals)[0];
            vmax = (*vals)[0];
            for (uint32_t i = 1; i < nsel; ++ i) {
                if (vmin > (*vals)[i])
                    vmin = (*vals)[i];
                if (vmax < (*vals)[i])
                    vmax = (*vals)[i];
            }
        }
        if (static_cast<uint32_t>(vmax-vmin) < vals->size()) {
            ierr = adaptiveIntsDetailed(mask, *vals, vmin, vmax, nbin,
                                        bounds, bins);
        }
        else {
            ierr = adaptiveFloatsDetailed(mask, *vals, vmin, vmax, nbin,
                                          bounds, bins);
            for (uint32_t i = 0; i < bounds.size(); ++ i)
                bounds[i] = ceil(bounds[i]);
        }
        delete vals;
        break;}
    case ibis::ULONG: {
        uint64_t vmin, vmax;
        array_t<uint64_t> *vals=0;
        ibis::fileManager::ACCESS_PREFERENCE acc = accessHint(mask, 1);
        if (acc == ibis::fileManager::PREFER_READ) {
            vals = new array_t<uint64_t>;
            ierr = col.getValuesArray(vals);
            if (ierr < 0) {
                LOGGER(ibis::gVerbose > 1)
                    << "Warning -- " << mesg
                    << " failed to retrieve any values for column "
                    << col.name();
                delete vals;
                return -36L;
            }
            else if (vals->size() != nEvents) {
                LOGGER(ibis::gVerbose > 1)
                    << "Warning -- " << mesg << " expected to retrieve "
                    << nEvents << " byte" << (nEvents > 1 ? "s" : "")
                    << ", but got " << vals->size();
                delete vals;
                return -37L;
            }
            vmin = std::numeric_limits<uint64_t>::max();
            vmax = 0;
            for (ibis::bitvector::indexSet is = mask.firstIndexSet();
                 is.nIndices() > 0; ++ is) {
                const ibis::bitvector::word_t nind = is.nIndices();
                const ibis::bitvector::word_t *idx = is.indices();
                if (is.isRange()) {
                    for (ibis::bitvector::word_t ii = *idx;
                         ii < idx[1]; ++ ii) {
                        if (vmin > (*vals)[ii])
                            vmin = (*vals)[ii];
                        if (vmax < (*vals)[ii])
                            vmax = (*vals)[ii];
                    }
                }
                else {
                    for (uint32_t ii = 0; ii < nind; ++ ii) {
                        const uint32_t jj = idx[ii];
                        if (vmin > (*vals)[jj])
                            vmin = (*vals)[jj];
                        if (vmax < (*vals)[jj])
                            vmax = (*vals)[jj];
                    }
                }
            }
        }
        else {
            ibis::bitvector::word_t nsel = mask.cnt();
            vals = col.selectULongs(mask);
            if (vals == 0) {
                LOGGER(ibis::gVerbose > 1)
                    << "Warning -- " << mesg
                    << " failed to retrieve any values for column "
                    << col.name();
                return -38L;
            }
            else if (vals->size() != nsel) {
                LOGGER(ibis::gVerbose > 1)
                    << "Warning -- " << mesg << " expected to retrieve " << nsel
                    << " byte" << (nsel > 1 ? "s" : "") << ", but got "
                    << vals->size();
                delete vals;
                return -39L;
            }
            vmin = (*vals)[0];
            vmax = (*vals)[0];
            for (uint32_t i = 1; i < nsel; ++ i) {
                if (vmin > (*vals)[i])
                    vmin = (*vals)[i];
                if (vmax < (*vals)[i])
                    vmax = (*vals)[i];
            }
        }
        if (vmax - vmin < vals->size()) {
            ierr = adaptiveIntsDetailed(mask, *vals, vmin, vmax, nbin,
                                        bounds, bins);
        }
        else {
            ierr = adaptiveFloatsDetailed(mask, *vals, vmin, vmax, nbin,
                                          bounds, bins);
            for (uint32_t i = 0; i < bounds.size(); ++ i)
                bounds[i] = ceil(bounds[i]);
        }
        delete vals;
        break;}
    case ibis::FLOAT: {
        float vmin, vmax;
        array_t<float> *vals=0;
        ibis::fileManager::ACCESS_PREFERENCE acc = accessHint(mask, 1);
        if (acc == ibis::fileManager::PREFER_READ) {
            vals = new array_t<float>;
            ierr = col.getValuesArray(vals);
            if (ierr < 0) {
                LOGGER(ibis::gVerbose > 1)
                    << "Warning -- " << mesg
                    << " failed to retrieve any values for column "
                    << col.name();
                delete vals;
                return -40L;
            }
            else if (vals->size() != nEvents) {
                LOGGER(ibis::gVerbose > 1)
                    << "Warning -- " << mesg << " expected to retrieve "
                    << nEvents << " byte" << (nEvents > 1 ? "s" : "")
                    << ", but got " << vals->size();
                delete vals;
                return -41L;
            }
            vmin = std::numeric_limits<float>::max();
            vmax = -std::numeric_limits<float>::max();
            for (ibis::bitvector::indexSet is = mask.firstIndexSet();
                 is.nIndices() > 0; ++ is) {
                const ibis::bitvector::word_t nind = is.nIndices();
                const ibis::bitvector::word_t *idx = is.indices();
                if (is.isRange()) {
                    for (ibis::bitvector::word_t ii = *idx;
                         ii < idx[1]; ++ ii) {
                        if (vmin > (*vals)[ii])
                            vmin = (*vals)[ii];
                        if (vmax < (*vals)[ii])
                            vmax = (*vals)[ii];
                    }
                }
                else {
                    for (uint32_t ii = 0; ii < nind; ++ ii) {
                        const uint32_t jj = idx[ii];
                        if (vmin > (*vals)[jj])
                            vmin = (*vals)[jj];
                        if (vmax < (*vals)[jj])
                            vmax = (*vals)[jj];
                    }
                }
            }
        }
        else {
            ibis::bitvector::word_t nsel = mask.cnt();
            vals = col.selectFloats(mask);
            if (vals == 0) {
                LOGGER(ibis::gVerbose > 1)
                    << "Warning -- " << mesg
                    << " failed to retrieve any values for column "
                    << col.name();
                return -42L;
            }
            else if (vals->size() != nsel) {
                LOGGER(ibis::gVerbose > 1)
                    << "Warning -- " << mesg << " expected to retrieve " << nsel
                    << " byte" << (nsel > 1 ? "s" : "") << ", but got "
                    << vals->size();
                delete vals;
                return -43L;
            }
            vmin = (*vals)[0];
            vmax = (*vals)[0];
            for (uint32_t i = 1; i < nsel; ++ i) {
                if (vmin > (*vals)[i])
                    vmin = (*vals)[i];
                if (vmax < (*vals)[i])
                    vmax = (*vals)[i];
            }
        }
        ierr = adaptiveFloatsDetailed(mask, *vals, vmin, vmax, nbin,
                                      bounds, bins);
        delete vals;
        break;}
    case ibis::DOUBLE: {
        double vmin, vmax;
        array_t<double> *vals=0;
        ibis::fileManager::ACCESS_PREFERENCE acc = accessHint(mask, 1);
        if (acc == ibis::fileManager::PREFER_READ) {
            vals = new array_t<double>;
            ierr = col.getValuesArray(vals);
            if (ierr < 0) {
                LOGGER(ibis::gVerbose > 1)
                    << "Warning -- " << mesg
                    << " failed to retrieve any values for column "
                    << col.name();
                delete vals;
                return -44L;
            }
            else if (vals->size() != nEvents) {
                LOGGER(ibis::gVerbose > 1)
                    << "Warning -- " << mesg << " expected to retrieve "
                    << nEvents << " byte" << (nEvents > 1 ? "s" : "")
                    << ", but got " << vals->size();
                delete vals;
                return -45L;
            }
            vmin = std::numeric_limits<double>::max();
            vmax = -std::numeric_limits<double>::max();
            for (ibis::bitvector::indexSet is = mask.firstIndexSet();
                 is.nIndices() > 0; ++ is) {
                const ibis::bitvector::word_t nind = is.nIndices();
                const ibis::bitvector::word_t *idx = is.indices();
                if (is.isRange()) {
                    for (ibis::bitvector::word_t ii = *idx;
                         ii < idx[1]; ++ ii) {
                        if (vmin > (*vals)[ii])
                            vmin = (*vals)[ii];
                        if (vmax < (*vals)[ii])
                            vmax = (*vals)[ii];
                    }
                }
                else {
                    for (uint32_t ii = 0; ii < nind; ++ ii) {
                        const uint32_t jj = idx[ii];
                        if (vmin > (*vals)[jj])
                            vmin = (*vals)[jj];
                        if (vmax < (*vals)[jj])
                            vmax = (*vals)[jj];
                    }
                }
            }
        }
        else {
            ibis::bitvector::word_t nsel = mask.cnt();
            vals = col.selectDoubles(mask);
            if (vals == 0) {
                LOGGER(ibis::gVerbose > 1)
                    << "Warning -- " << mesg
                    << " failed to retrieve any values for column "
                    << col.name();
                return -46L;
            }
            else if (vals->size() != nsel) {
                LOGGER(ibis::gVerbose > 1)
                    << "Warning -- " << mesg << " expected to retrieve " << nsel
                    << " byte" << (nsel > 1 ? "s" : "") << ", but got "
                    << vals->size();
                delete vals;
                return -47L;
            }
            vmin = (*vals)[0];
            vmax = (*vals)[0];
            for (uint32_t i = 1; i < nsel; ++ i) {
                if (vmin > (*vals)[i])
                    vmin = (*vals)[i];
                if (vmax < (*vals)[i])
                    vmax = (*vals)[i];
            }
        }
        ierr = adaptiveFloatsDetailed(mask, *vals, vmin, vmax, nbin,
                                      bounds, bins);
        delete vals;
        break;}
    }
#if defined(_DEBUG) || defined(DEBUG)
    if (ibis::gVerbose > 5) {
        ibis::util::logger lg(4);
        lg() << "part::get1DBins_ completed for " << mesg
                    << ", memory in use = "
                    << ibis::fileManager::instance().bytesInUse();
        if (ibis::gVerbose > 7) {
            lg() << "\nCurrent status of the file manager:";
            ibis::fileManager::instance().printStatus(lg());
        }
    }
#endif
    return ierr;
} // ibis::part::get1DBins_

/// If the string constraints is nil or an empty string or starting with an
/// asterisk (*), it is assumed every valid record of the named column is
/// used.  Arrays bounds1 and bins are both for output only.  Upon
/// successful completion of this function, the return value shall be the
/// number of bins actually used.  A return value of 0 indicates no record
/// satisfy the constraints.  A negative return indicates error.
///
/// @sa ibis::part::get1DDistribution
/// @sa ibis::part::part::adaptiveInts
long ibis::part::get1DBins(const char *constraints, const char *cname1,
                           uint32_t nb1, std::vector<double> &bounds1,
                           std::vector<ibis::bitvector> &bins) const {
    if (cname1 == 0 || *cname1 == 0) return -1L;
    ibis::column *col1 = getColumn(cname1);
    if (col1 == 0) return -2L;
    std::string mesg;
    {
        std::ostringstream oss;
        oss << "part[" << (m_name ? m_name : "") << "]::get1DBins("
            << cname1 << ", " << nb1 << ")";
        mesg = oss.str();
    }
    ibis::util::timer atimer(mesg.c_str(), 1);
    ibis::bitvector mask;
    long ierr;
    col1->getNullMask(mask);
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

    ierr = get1DBins_(mask, *col1, nb1, bounds1, bins, mesg.c_str());
    return ierr;
} // ibis::part::get1DBins

///  The array @c bounds defines the following bins:
/// @code
/// (..., bounds[0]) [bounds[0], bounds[1]) ... [bounds.back(), ...).
/// @endcode
/// or alternatively,
/// @verbatim
/// bin 0: (..., bounds[0]) -> counts[0]
/// bin 1: [bounds[0], bounds[1]) -> counts[1]
/// bin 2: [bounds[1], bounds[2]) -> counts[2]
/// bin 3: [bounds[2], bounds[3]) -> counts[3]
/// ...
/// @endverbatim
/// In other word, @c bounds[n] defines (n+1) bins, with two open bins
/// at the two ends.  The array @c counts contains the number of rows
/// fall into each bin.  On a successful return from this function, the
/// return value of this function is the number of bins defined, which
/// is the same as the size of array @c counts but one larger than the
/// size of array @c bounds.
///
/// Return the number of bins (i.e., counts.size()) on success.
long
ibis::part::getDistribution(const char *name,
                            std::vector<double> &bounds,
                            std::vector<uint32_t> &counts) const {
    long ierr = -1;
    columnList::const_iterator it = columns.find(name);
    if (it != columns.end()) {
        ierr = (*it).second->getDistribution(bounds, counts);
        if (ierr < 0)
            ierr -= 10;
    }
    return ierr;
} // ibis::part::getDistribution

/// Because most of the binning scheme leaves two bins for overflow, one
/// for values less than the expected minimum and one for values greater
/// than the expected maximum.  The minimum number of bins expected is
/// four (4).  This function will return error code -1 if the value of nbc
/// is less than 4.
long
ibis::part::getDistribution
(const char *name, uint32_t nbc, double *bounds, uint32_t *counts) const {
    if (nbc < 4) // work space too small
        return -1;
    std::vector<double> bds;
    std::vector<uint32_t> cts;
    long mbc = getDistribution(name, bds, cts);
#if DEBUG+0 > 1 || _DEBUG+0 > 1
    {
        ibis::util::logger lg(4);
        lg() << "DEBUG -- getDistribution(" << name
                  << ") returned ierr=" << mbc << ", bds.size()="
                  << bds.size() << ", cts.size()=" << cts.size() << "\n";
        if (mbc > 0 && bds.size()+1 == cts.size() &&
            static_cast<uint32_t>(mbc) == cts.size()) {
            lg() << "(..., " << bds[0] << ")\t" << cts[0] << "\n";
            for (int i = 1; i < mbc-1; ++ i) {
                lg() << "[" << bds[i-1] << ", "<< bds[i] << ")\t"
                          << cts[i] << "\n";
            }
            lg() << "[" << bds.back() << ", ...)\t" << cts.back()
                      << "\n";
        }
    }
#endif
    mbc = packDistribution(bds, cts, nbc, bounds, counts);
    return mbc;
} // ibis::part::getDistribution

/// Compute the distribution of the named variable under the specified
/// constraints.  If the input array @c bounds contains distinct values in
/// ascending order, the array will be used as bin boundaries.  Otherwise,
/// the bin boundaries are automatically determined by this function.  The
/// basic rule for determining the number of bins is that if there are less
/// than 10,000 distinct values, than every value is counted separatly,
/// otherwise 1000 bins will be used and each bin will contain roughly the
/// same number of records.
///
/// @note Deprecated.
long
ibis::part::getDistribution(const char *constraints,
                            const char *name,
                            std::vector<double> &bounds,
                            std::vector<uint32_t> &counts) const {
    long ierr = -1;
    columnList::const_iterator it = columns.find(name);
    if (it == columns.end())
        return ierr;

    ibis::horometer timer;
    if (ibis::gVerbose > 0) {
        LOGGER(ibis::gVerbose > 2)
            << "part[" << (m_name ? m_name : "")
            << "]::getDistribution attempting to compute a histogram of "
            << name << (constraints && *constraints ? " subject to " :
                        " without constraints")
            << (constraints ? constraints : "");
        timer.start();
    }
    if (constraints == 0 || *constraints == 0 || *constraints == '*') {
        ierr = (*it).second->getDistribution(bounds, counts);
        if (ierr > 0 && ibis::gVerbose > 0) {
            timer.stop();
            logMessage("getDistribution",
                       "computing the distribution of column %s took %g "
                       "sec(CPU), %g sec(elapsed)",
                       (*it).first, timer.CPUTime(), timer.realTime());
        }
        return ierr;
    }

    ibis::bitvector mask;
    const ibis::column *col = (*it).second;
    col->getNullMask(mask);
    if (constraints != 0 && *constraints != 0) {
        ibis::countQuery q(this);
        q.setWhereClause(constraints);
        ierr = q.evaluate();
        if (ierr < 0) {
            ierr = -2;
            return ierr;
        }

        mask &= (*(q.getHitVector()));
        if (mask.cnt() == 0) {
            if (ibis::gVerbose > 2)
                logMessage("getDistribution", "no record satisfied the "
                           "user specified constraints \"%s\"", constraints);
            return 0;
        }
    }
    bool usebnds = ! bounds.empty();
    for (uint32_t i = 1; usebnds && i < bounds.size(); ++ i)
        usebnds = (bounds[i] > bounds[i-1]);

    if (usebnds) { // use the input bin boundaries
        switch ((*it).second->type()) {
        case ibis::SHORT:
        case ibis::BYTE:
        case ibis::INT: {
            array_t<int32_t> *vals = col->selectInts(mask);
            if (vals == 0) {
                ierr = -4;
                break;
            }
            array_t<int32_t> bnds(bounds.size());
            for (uint32_t i = 0; i < bounds.size(); ++ i)
                bnds[i] = static_cast<int32_t>(bounds[i]);
            ibis::index::mapValues<int32_t>(*vals, bnds, counts);
            delete vals;
            break;}
        case ibis::USHORT:
        case ibis::UBYTE:
        case ibis::UINT:
        case ibis::CATEGORY: {
            array_t<uint32_t> *vals = col->selectUInts(mask);
            if (vals == 0) {
                ierr = -4;
                break;
            }
            array_t<uint32_t> bnds(bounds.size());
            for (uint32_t i = 0; i < bounds.size(); ++ i)
                bnds[i] = static_cast<uint32_t>(bounds[i]);
            ibis::index::mapValues<uint32_t>(*vals, bnds, counts);
            delete vals;
            break;}
        case ibis::FLOAT: {
            array_t<float> *vals = col->selectFloats(mask);
            if (vals == 0) {
                ierr = -4;
                break;
            }
            array_t<float> bnds(bounds.size());
            for (uint32_t i = 0; i < bounds.size(); ++ i)
                bnds[i] = static_cast<float>(bounds[i]);
            ibis::index::mapValues<float>(*vals, bnds, counts);
            delete vals;
            break;}
        case ibis::DOUBLE: {
            array_t<double> *vals = col->selectDoubles(mask);
            if (vals == 0) {
                ierr = -4;
                break;
            }
            array_t<double> bnds(bounds.size());
            for (uint32_t i = 0; i < bounds.size(); ++ i)
                bnds[i] = bounds[i];
            ibis::index::mapValues<double>(*vals, bnds, counts);
            delete vals;
            break;}
        default: {
            ierr = -3;
            logWarning("getDistribution",
                       "can not handle column type %d",
                       static_cast<int>((*it).second->type()));
            break;}
        }
    }
    else { // need to determine bin boundaries in this function
        ibis::index::histogram hist;
        bounds.clear();
        counts.clear();
        switch ((*it).second->type()) {
        case ibis::SHORT:
        case ibis::BYTE:
        case ibis::INT: {
            array_t<int32_t> *vals = col->selectInts(mask);
            if (vals == 0) {
                ierr = -4;
                break;
            }
            ibis::index::mapValues<int32_t>(*vals, hist);
            delete vals;
            break;}
        case ibis::USHORT:
        case ibis::UBYTE:
        case ibis::UINT:
        case ibis::CATEGORY: {
            array_t<uint32_t> *vals = col->selectUInts(mask);
            if (vals == 0) {
                ierr = -4;
                break;
            }
            ibis::index::mapValues<uint32_t>(*vals, hist);
            delete vals;
            break;}
        case ibis::FLOAT: {
            array_t<float> *vals = col->selectFloats(mask);
            if (vals == 0) {
                ierr = -4;
                break;
            }
            ibis::index::mapValues<float>(*vals, hist);
            delete vals;
            break;}
        case ibis::DOUBLE: {
            array_t<double> *vals = col->selectDoubles(mask);
            if (vals == 0) {
                ierr = -4;
                break;
            }
            ibis::index::mapValues<double>(*vals, hist);
            delete vals;
            break;}
        default: {
            ierr = -3;
            logWarning("getDistribution",
                       "can not handle column type %d",
                       static_cast<int>((*it).second->type()));
            break;}
        }

        if (hist.size() == 1) { // special case of a single value
            ibis::index::histogram::const_iterator it1 =
                hist.begin();
            bounds.resize(2);
            counts.resize(3);
            bounds[0] = (*it1).first;
            bounds[1] = ((*it1).first + 1.0);
            counts[0] = 0;
            counts[1] = (*it1).second;
            counts[2] = 0;
        }
        else if (hist.size() < 10000) {
            // convert the histogram into two arrays
            bounds.reserve(mask.cnt());
            counts.reserve(mask.cnt()+1);
            ibis::index::histogram::const_iterator it1 =
                hist.begin();
            counts.push_back((*it1).second);
            for (++ it1; it1 != hist.end(); ++ it1) {
                bounds.push_back((*it1).first);
                counts.push_back((*it1).second);
            }
        }
        else if (hist.size() > 0) { // too many values, reduce to 1000 bins
            array_t<double> vals(hist.size());
            array_t<uint32_t> cnts(hist.size());
            vals.clear();
            cnts.clear();
            for (ibis::index::histogram::const_iterator it1 =
                     hist.begin();
                 it1 != hist.end(); ++ it1) {
                vals.push_back((*it1).first);
                cnts.push_back((*it1).second);
            }
            array_t<uint32_t> dvd(1000);
            ibis::index::divideCounts(dvd, cnts);
            for (uint32_t i = 0; i < dvd.size(); ++ i) {
                uint32_t cnt = 0;
                for (uint32_t j = (i>0?dvd[i-1]:0); j < dvd[i]; ++ j)
                    cnt += cnts[j];
                counts.push_back(cnt);
                if (i > 0) {
                    double bd;
                    if (dvd[i] < vals.size())
                        bd = ibis::util::compactValue
                            (vals[dvd[i]-1], vals[dvd[i]]);
                    else
                        bd = ibis::util::compactValue
                            (vals.back(), DBL_MAX);
                    bounds.push_back(bd);
                }
            }
        }
    }
    if (ierr >= 0)
        ierr = counts.size();
    if (ierr > 0 && ibis::gVerbose > 0) {
        timer.stop();
        logMessage("getDistribution",
                   "computing the distribution of "
                   "column %s with restriction \"%s\" took %g "
                   "sec(CPU), %g sec(elapsed)", (*it).first,
                   constraints, timer.CPUTime(), timer.realTime());
    }
    if (ierr < 0)
        ierr -= 10;
    return ierr;
} // ibis::part::getDistribution

/// Because most of the binning scheme leaves two bins for overflow, one
/// for values less than the expected minimum and one for values greater
/// than the expected maximum.  The minimum number of bins expected is
/// four (4).  This function will return error code -1 if the value of nbc
/// is less than 4.
///
/// @note Deprecated.
long
ibis::part::getDistribution
(const char *constraints, const char *name, uint32_t nbc,
 double *bounds, uint32_t *counts) const {
    if (nbc < 4) // work space too small
        return -1;

    std::vector<double> bds;
    std::vector<uint32_t> cts;
    bool useinput = true;
    for (uint32_t i = 1; i < nbc && useinput; ++ i)
        useinput = (bounds[i] > bounds[i-1]);
    if (useinput) {
        bds.resize(nbc);
        for (uint32_t i = 0; i < nbc; ++i)
            bds[i] = bounds[i];
    }
    long mbc = getDistribution(constraints, name, bds, cts);
#if DEBUG+0 > 1 || _DEBUG+0 > 1
    {
        ibis::util::logger lg(4);
        lg() << "DEBUG -- getDistribution(" << name << ", "
                    << constraints << ") returned ierr=" << mbc
                    << ", bds.size()=" << bds.size() << ", cts.size()="
                    << cts.size() << "\n";
        if (mbc > 0 && bds.size()+1 == cts.size() &&
            static_cast<uint32_t>(mbc) == cts.size()) {
            lg() << "(..., " << bds[0] << ")\t" << cts[0] << "\n";
            for (int i = 1; i < mbc-1; ++ i) {
                lg() << "[" << bds[i-1] << ", "<< bds[i] << ")\t"
                          << cts[i] << "\n";
            }
            lg() << "[" << bds.back() << ", ...)\t" << cts.back()
                      << "\n";
        }
    }
#endif
    mbc = packDistribution(bds, cts, nbc, bounds, counts);
    return mbc;
} // ibis::part::getDistribution

/// It returns the number of entries in arrays @c bounds and @c counts.
/// The content of @c counts[i] will be the number of records in the named
/// column that are less than @c bounds[i].  The last element in array @c
/// bounds is larger than returned by function getColumnMax.
///
/// @note Deprecated.
long
ibis::part::getCumulativeDistribution(const char *name,
                                      std::vector<double> &bounds,
                                      std::vector<uint32_t> &counts) const {
    long ierr = -1;
    columnList::const_iterator it = columns.find(name);
    if (it != columns.end()) {
        ierr = (*it).second->getCumulativeDistribution(bounds, counts);
        if (ierr < 0)
            ierr -= 10;
    }
    return ierr;
} // ibis::part::getCumulativeDistribution

/// The actual number of elements filled by this function is the return
/// value, which is guaranteed to be no larger than the input value of @c
/// nbc.
///
/// @note Because most of the binning scheme leaves two bins for overflow,
/// one for values less than the expected minimum and one for values
/// greater than the expected maximum.  The minimum number of bins expected
/// is four (4).  This function will return error code -1 if the value of
/// nbc is less than 4.
///
/// @note Deprecated.
long
ibis::part::getCumulativeDistribution
(const char *name, uint32_t nbc, double *bounds, uint32_t *counts) const {
    if (nbc < 4) // work space too small
        return -1;
    std::vector<double> bds;
    std::vector<uint32_t> cts;
    long mbc = getCumulativeDistribution(name, bds, cts);
#if DEBUG+0 > 1 || _DEBUG+0 > 1
    {
        ibis::util::logger lg(4);
        lg() << "DEBUG -- getCumulativeDistribution(" << name
                  << ") returned ierr=" << mbc << "\n";
        if (mbc > 0)
            lg() << "histogram\n(bound,\tcount)\n";
        for (int i = 0; i < mbc; ++ i) {
            lg() << bds[i] << ",\t" << cts[i] << "\n";
        }
    }
#endif
    mbc = packCumulativeDistribution(bds, cts, nbc, bounds, counts);
    return mbc;
} // ibis::part::getCumulativeDistribution

/// @note  The constraints have the same syntax as the where-clause of
/// the queries.  Here are two examples, "a < 5 and 3.5 >= b >= 1.9" and
/// "a * a + b * b > 55 and sqrt(c) > 2."
/// @note This function does not accept user input bin boundaries.
///
/// @note Deprecated.
long
ibis::part::getCumulativeDistribution(const char *constraints,
                                      const char *name,
                                      std::vector<double> &bounds,
                                      std::vector<uint32_t> &counts) const {
    long ierr = -1;
    columnList::const_iterator it = columns.find(name);
    if (it == columns.end())
        return ierr;

    ibis::horometer timer;
    if (ibis::gVerbose > 0) {
        LOGGER(ibis::gVerbose > 2)
            << "part[" << (m_name ? m_name : "")
            << "]::getCumulativeDistribution attempting to compute the "
            "cummulative distribution of "
            << name << (constraints && *constraints ? " subject to " :
                        " without constraints")
            << (constraints ? constraints : "");
        timer.start();
    }
    if (constraints == 0 || *constraints == 0 || *constraints == '*') {
        ierr = (*it).second->getCumulativeDistribution(bounds, counts);
        if (ierr > 0 && ibis::gVerbose > 0) {
            timer.stop();
            logMessage("getCumulativeDistribution",
                       "computing the distribution of column %s took %g "
                       "sec(CPU), %g sec(elapsed)",
                       (*it).first, timer.CPUTime(), timer.realTime());
        }
    }
    else {
        ibis::bitvector hits;
        const ibis::column *col = (*it).second;
        {
            ibis::countQuery q(this);
            q.setWhereClause(constraints);
            ierr = q.evaluate();
            if (ierr < 0)
                return ierr;
            hits.copy(*(q.getHitVector()));
            if (hits.cnt() == 0)
                return 0;
        }
        ibis::index::histogram hist;
        bounds.clear();
        counts.clear();
        if (hits.cnt() > 0) {
            switch ((*it).second->type()) {
            case ibis::SHORT:
            case ibis::BYTE:
            case ibis::INT: {
                array_t<int32_t> *vals = col->selectInts(hits);
                if (vals == 0) {
                    ierr = -4;
                    break;
                }
                ibis::index::mapValues<int32_t>(*vals, hist);
                delete vals;
                break;}
            case ibis::USHORT:
            case ibis::UBYTE:
            case ibis::UINT:
            case ibis::CATEGORY: {
                array_t<uint32_t> *vals = col->selectUInts(hits);
                if (vals == 0) {
                    ierr = -4;
                    break;
                }
                ibis::index::mapValues<uint32_t>(*vals, hist);
                delete vals;
                break;}
            case ibis::FLOAT: {
                array_t<float> *vals = col->selectFloats(hits);
                if (vals == 0) {
                    ierr = -4;
                    break;
                }
                ibis::index::mapValues<float>(*vals, hist);
                delete vals;
                break;}
            case ibis::DOUBLE: {
                array_t<double> *vals = col->selectDoubles(hits);
                if (vals == 0) {
                    ierr = -4;
                    break;
                }
                ibis::index::mapValues<double>(*vals, hist);
                delete vals;
                break;}
            default: {
                ierr = -3;
                logWarning("getCumulativeDistribution",
                           "can not handle column type %d",
                           static_cast<int>((*it).second->type()));
                break;}
            }

            if (hist.empty()) {
                if (ierr >= 0)
                    ierr = -7;
            }
            else if (hist.size() < 10000) {
                // convert the histogram into cumulative distribution
                bounds.reserve(hits.cnt()+1);
                counts.reserve(hits.cnt()+1);
                counts.push_back(0);
                for (ibis::index::histogram::const_iterator hit =
                         hist.begin();
                     hit != hist.end(); ++ hit) {
                    bounds.push_back((*hit).first);
                    counts.push_back((*hit).second + counts.back());
                }
                bounds.push_back(ibis::util::compactValue
                                 (bounds.back(), DBL_MAX));
            }
            else { // too many values, reduce to 1000 bins
                array_t<double> vals(hist.size());
                array_t<uint32_t> cnts(hist.size());
                vals.clear();
                cnts.clear();
                for (ibis::index::histogram::const_iterator hit =
                         hist.begin();
                     hit != hist.end(); ++ hit) {
                    vals.push_back((*hit).first);
                    cnts.push_back((*hit).second);
                }
                array_t<uint32_t> dvd(1000);
                ibis::index::divideCounts(dvd, cnts);
                bounds.push_back(vals[0]);
                counts.push_back(0);
                for (uint32_t i = 0; i < dvd.size(); ++ i) {
                    uint32_t cnt = counts.back();
                    for (uint32_t j = (i>0?dvd[i-1]:0); j < dvd[i]; ++ j)
                        cnt += cnts[j];
                    counts.push_back(cnt);
                    double bd;
                    if (dvd[i] < vals.size())
                        bd = ibis::util::compactValue(vals[dvd[i]-1],
                                                      vals[dvd[i]]);
                    else
                        bd = ibis::util::compactValue
                            (vals.back(), DBL_MAX);
                    bounds.push_back(bd);
                }
            }
        }
        if (ierr >= 0)
            ierr = counts.size();
        if (ierr > 0 && ibis::gVerbose > 0) {
            timer.stop();
            logMessage("getCumulativeDistribution",
                       "computing the distribution of "
                       "column %s with restriction \"%s\" took %g "
                       "sec(CPU), %g sec(elapsed)", (*it).first,
                       constraints, timer.CPUTime(), timer.realTime());
        }
    }
    if (ierr < 0)
        ierr -= 10;

    return ierr;
} // ibis::part::getCumulativeDistribution

/// Because most of the binning scheme leaves two bins for overflow, one
/// for values less than the expected minimum and one for values greater
/// than the expected maximum.  The minimum number of bins expected is
/// four (4).  This function will return error code -1 if the value of nbc
/// is less than 4.
long
ibis::part::getCumulativeDistribution
(const char *constraints, const char *name, uint32_t nbc,
 double *bounds, uint32_t *counts) const {
    if (nbc < 4) // work space too small
        return -1;

    std::vector<double> bds;
    std::vector<uint32_t> cts;
    long mbc = getCumulativeDistribution(constraints, name, bds, cts);
    mbc = packCumulativeDistribution(bds, cts, nbc, bounds, counts);
    return mbc;
} // ibis::part::getCumulativeDistribution

long ibis::part::packDistribution
(const std::vector<double> &bds, const std::vector<uint32_t> &cts,
 uint32_t nbc, double *bptr, uint32_t *cptr) const {
    uint32_t mbc = bds.size();
    if (mbc <= 0)
        return mbc;
    if (static_cast<uint32_t>(mbc+1) != cts.size()) {
        ibis::util::logMessage
            ("Warning", "packDistribution expects the size "
             "of bds[%lu] to be the one less than that of "
             "cts[%lu], but it is not",
             static_cast<long unsigned>(bds.size()),
             static_cast<long unsigned>(cts.size()));
        return -1;
    }
    if (nbc < 2) {
        ibis::util::logMessage
            ("Warning", "a binned distribution needs "
             "two arrays of size at least 2, caller has "
             "provided two arrays of size %lu",
             static_cast<long unsigned>(nbc));
        return -2;
    }
    if (static_cast<uint32_t>(mbc) <= nbc) {
        // copy the values in bds and cts
        for (uint32_t i = 0; i < mbc; ++ i) {
            bptr[i] = bds[i];
            cptr[i] = cts[i];
        }
        cptr[mbc] = cts[mbc];
        ++ mbc;
    }
    else { // make the distribution fit the given space
        // the first entry is always copied
        bptr[0] = bds[0];
        cptr[0] = cts[0];

        uint32_t top = 0; // the total number of entries to be redistributed
        uint32_t cnt = 0; // entries already redistributed
        for (uint32_t k = 1; k < mbc; ++ k)
            top += cts[k];
        uint32_t i = 1; // index to output bins
        uint32_t j = 1; // index to input bins
        while (i < nbc-1 && nbc+j < mbc+i) {
            uint32_t next = j + 1;
            uint32_t tgt = (top - cnt) / (nbc-i-1);
            bptr[i] = bds[j];
            cptr[i] = cts[j];
            while (cptr[i] < tgt && nbc+next <= mbc+i) {
                cptr[i] += cts[next];
                ++ next;
            }
#if DEBUG+0 > 1 || _DEBUG+0 > 1
            LOGGER(ibis::gVerbose >= 0)
                << "DEBUG -- i=" << i << ", j = " << j << ", bds[j]="
                << bds[j] << ", next=" << next << ", bds[next]="
                << bds[next] << ", cts[next]=" << cts[next];
#endif
            j = next;
            ++ i;
        }
        ++ j;
        if (mbc - j > nbc - i)
            j = 1 + mbc - nbc + i;
        // copy the last few bins
        while (i < nbc && j < static_cast<uint32_t>(mbc)) {
            bptr[i] = bds[j];
            cptr[i] = cts[j];
            ++ i;
            ++ j;
        }
        if (j == static_cast<uint32_t>(mbc) && i < nbc) {
            cptr[i] = cts[mbc];
            mbc = i + 1;
        }
        else {
            mbc = i;
        }
    }
    return mbc;
} // ibis::part::packDistribution

long ibis::part::packCumulativeDistribution
(const std::vector<double> &bds, const std::vector<uint32_t> &cts,
 uint32_t nbc, double *bptr, uint32_t *cptr) const {
    long mbc = bds.size();
    if (mbc <= 0)
        return mbc;
    if (static_cast<uint32_t>(mbc) != cts.size()) {
        ibis::util::logMessage
            ("Warning", "packCumulativeDistribution expects "
             "the size of bds[%lu] to be the same as that "
             "of cts[%lu], but they are not",
             static_cast<long unsigned>(bds.size()),
             static_cast<long unsigned>(cts.size()));
        return -1;
    }
    if (nbc < 2) {
        ibis::util::logMessage
            ("Warning", "a cumulative distribution needs "
             "two arrays of size at least 2, caller has "
             "provided two arrays of size %lu",
             static_cast<long unsigned>(nbc));
        return -2;
    }
    if (static_cast<uint32_t>(mbc) <= nbc) {
        // copy the values in bds and cts
        for (int i = 0; i < mbc; ++ i) {
            bptr[i] = bds[i];
            cptr[i] = cts[i];
        }
    }
//     else if (static_cast<uint32_t>(mbc) <= nbc+nbc-3) {
//      // less than two values in a bin on average
//      uint32_t start = nbc + nbc - mbc - 2;
//      for (uint32_t i = 0; i <= start; ++ i) {
//          bptr[i] = bds[i];
//          cptr[i] = cts[i];
//      }
//      for (uint32_t i = start+1; i < nbc-1; ++ i) {
//          uint32_t j = i + i - start;
//          bptr[i] = bds[j];
//          cptr[i] = cts[j];
//      }
//      bptr[nbc-1] = bds[mbc-1];
//      cptr[nbc-1] = cts[mbc-1];
//      mbc = nbc; // mbc is the return value
//     }
    else { // make the distribution fit the given space
        // the first entries are always copied
        bptr[0] = bds[0];
        cptr[0] = cts[0];
        bptr[1] = bds[1];
        cptr[1] = cts[1];

        uint32_t top = cts[mbc-2];
        uint32_t i = 2, j = 1;
        while (i < nbc-1 && nbc+j < mbc+i-1) {
            uint32_t next = j + 1;
            uint32_t tgt = cts[j] + (top - cts[j]) / (nbc-i-1);
            while (cts[next] < tgt && nbc+next <= mbc+i-1)
                ++ next;
#if DEBUG+0 > 1 || _DEBUG+0 > 1
            LOGGER(ibis::gVerbose >= 0)
                << "DEBUG -- i=" << i << ", next=" << next << ", bds[next]="
                << bds[next] << ", cts[next]=" << cts[next];
#endif
            bptr[i] = bds[next];
            cptr[i] = cts[next];
            j = next;
            ++ i;
        }
        ++ j;
        if (mbc - j > nbc - i)
            j = mbc - nbc + i;
        while (i < nbc && j < static_cast<uint32_t>(mbc)) {
            bptr[i] = bds[j];
            cptr[i] = cts[j];
            ++ i;
            ++ j;
        }
        mbc = i;
    }
    return mbc;
} // ibis::part::packCumulativeDistribution

// explicit instantiation
template long ibis::part::adaptiveFloats
(const array_t<signed char> &, const signed char, const signed char, uint32_t,
 std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptiveFloats
(const array_t<unsigned char> &, const unsigned char, const unsigned char,
 uint32_t, std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptiveFloats
(const array_t<int16_t> &, const int16_t, const int16_t, uint32_t,
 std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptiveFloats
(const array_t<uint16_t> &, const uint16_t, const uint16_t, uint32_t,
 std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptiveFloats
(const array_t<int32_t> &, const int32_t, const int32_t, uint32_t,
 std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptiveFloats
(const array_t<uint32_t> &, const uint32_t, const uint32_t, uint32_t,
 std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptiveFloats
(const array_t<int64_t> &, const int64_t, const int64_t, uint32_t,
 std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptiveFloats
(const array_t<uint64_t> &, const uint64_t, const uint64_t, uint32_t,
 std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptiveFloats
(const array_t<float> &, const float, const float, uint32_t,
 std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptiveFloats
(const array_t<double> &, const double, const double, uint32_t,
 std::vector<double> &, std::vector<uint32_t> &);
// The compilers may believe that they require the following two
// functions, however, they are never actually used.  You can safely
// ignore the warning about converting floats/doubles to integers
// originated from these two functions.
template long ibis::part::adaptiveInts
(const array_t<float> &, const float, const float, uint32_t,
 std::vector<double> &, std::vector<uint32_t> &);
template long ibis::part::adaptiveInts
(const array_t<double> &, const double, const double, uint32_t,
 std::vector<double> &, std::vector<uint32_t> &);
