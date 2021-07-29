// File $Id$
// Author: John Wu <John.Wu at ACM.org> Lawrence Berkeley National Laboratory
// Copyright (c) 2009-2016 the Regents of the University of California
//
// Implements ibis::part::get3DBins that returns vector<bitvector>.
#include "countQuery.h" // ibis::countQuery
#include "part.h"
#include <math.h>       // floor
#include <typeinfo>     // typeid
#include <iomanip>      // setw, setprecision

/// The three triplets, (begin1, end1, stride1), (begin2, end2, stride2),
/// and (begin3, end3, stride3), defines <tt> (1 + floor((end1 - begin1) /
/// stride1)) (1 + floor((end2 - begin2) / stride2)) (1 + floor((end3 -
/// begin3) / stride3)) </tt> 3D bins.  The 3D bins are packed into the 1D
/// array bins in raster scan order, with the 3rd dimension as the fastest
/// varying dimension and the 1st dimension as the slowest varying dimension.
///
/// @note All bitmaps that are empty are left with size() = 0.  All other
/// bitmaps have the same size() as mask.size().  When use these returned
/// bitmaps, please make sure to NOT mix empty bitmaps with non-empty
/// bitmaps in bitwise logical operations!
///
/// @sa ibis::part::fill1DBins, ibis::part::fill2DBins.
template <typename T1, typename T2, typename T3>
long ibis::part::fill3DBins(const ibis::bitvector &mask,
                            const array_t<T1> &vals1,
                            const double &begin1, const double &end1,
                            const double &stride1,
                            const array_t<T2> &vals2,
                            const double &begin2, const double &end2,
                            const double &stride2,
                            const array_t<T3> &vals3,
                            const double &begin3, const double &end3,
                            const double &stride3,
                            std::vector<ibis::bitvector> &bins) const {
    if ((end1-begin1) * (end2-begin2) * (end3-begin3) >
        1e9 * stride1 * stride2 * stride3 ||
        (end1-begin1) * stride1 < 0.0 || (end2-begin2) * stride2 < 0.0 ||
        (end3-begin3) * stride3 < 0.0)
        return -10L;
    LOGGER(ibis::gVerbose > 5)
        << "part::fill3DBins<" << typeid(T1).name() << ", "
        << typeid(T2).name() << ", " << typeid(T3).name() << ">("
        << "vals1[" << vals1.size() << "], " << begin1 << ", "
        << end1 << ", " << stride1
        << ", vals2[" << vals2.size() << "], " << begin2 << ", "
        << end2 << ", " << stride2
        << ", vals3[" << vals3.size() << "], " << begin3 << ", "
        << end3 << ", " << stride3 << ", bins[" << bins.size()
        << "]) ... ("
        << 1 + static_cast<uint32_t>(floor((end1-begin1)/stride1))
        << ", "
        << 1 + static_cast<uint32_t>(floor((end2-begin2)/stride2))
        << ", "
        << 1 + static_cast<uint32_t>(floor((end3-begin3)/stride3))
        << ")";
    const uint32_t nbin3 = (1 + static_cast<uint32_t>((end3-begin3)/stride3));
    const uint32_t nbin23 = (1 + static_cast<uint32_t>((end2-begin2)/stride2)) *
        nbin3;
    const uint32_t nbins = (1 + static_cast<uint32_t>((end1-begin1)/stride1)) *
        nbin23;
    uint32_t nvals = (vals1.size() <= vals2.size() ?
                    (vals1.size() <= vals3.size() ?
                     vals1.size() : vals3.size()) :
                    (vals2.size() <= vals3.size() ?
                     vals2.size() : vals3.size()));
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
                    const uint32_t ibin3 =
                        static_cast<uint32_t>((vals3[j]-begin3)/stride3);
                    bins[ibin1*nbin23+ibin2*nbin3+ibin3].setBit(j, 1);
                }
            }
            else {
                for (uint32_t k = 0; k < is.nIndices(); ++ k) {
                    const ibis::bitvector::word_t j = idx[k];
                    const uint32_t ibin1 =
                        static_cast<uint32_t>((vals1[j]-begin1)/stride1);
                    const uint32_t ibin2 =
                        static_cast<uint32_t>((vals2[j]-begin2)/stride2);
                    const uint32_t ibin3 =
                        static_cast<uint32_t>((vals3[j]-begin3)/stride3);
                    bins[ibin1*nbin23+ibin2*nbin3+ibin3].setBit(j, 1);
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
                    const uint32_t ibin3 =
                        static_cast<uint32_t>((vals3[ivals]-begin3)/stride3);
                    bins[ibin1*nbin23+ibin2*nbin3+ibin3].setBit(j, 1);
                }
            }
            else {
                for (uint32_t k = 0; k < is.nIndices(); ++ k, ++ ivals) {
                    const ibis::bitvector::word_t j = idx[k];
                    const uint32_t ibin1 =
                        static_cast<uint32_t>((vals1[ivals]-begin1)/stride1);
                    const uint32_t ibin2 =
                        static_cast<uint32_t>((vals2[ivals]-begin2)/stride2);
                    const uint32_t ibin3 =
                        static_cast<uint32_t>((vals3[ivals]-begin3)/stride3);
                    bins[ibin1*nbin23+ibin2*nbin3+ibin3].setBit(j, 1);
#if (defined(_DEBUG) && _DEBUG+0 > 1) || (defined(DEBUG) && DEBUG+0 > 1)
                    const uint32_t pos = ibin1*nbin23+ibin2*nbin3+ibin3;
                    LOGGER(ibis::gVerbose > 5)
                        << "DEBUG -- fill3DBins -- vals1[" << ivals << "]="
                        << vals1[ivals]
                        << ", vals2[" << ivals << "]=" << vals2[ivals]
                        << ", vals3[" << ivals << "]=" << vals3[ivals]
                        << " --> bin ("
                        << static_cast<uint32_t>((vals1[ivals]-begin1)/stride1)
                        << ", "
                        << static_cast<uint32_t>((vals2[ivals]-begin2)/stride2)
                        << ", "
                        << static_cast<uint32_t>((vals3[ivals]-begin3)/stride3)
                        << ") bins[" << pos << "]=" << bins[pos].cnt();
#endif
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
    return nbins;
} // ibis::part::fill3DBins

/// Resolve the 3rd column involved in the 3D bins.  The finally binning
/// work is performed by ibis::part::fill3DBins.
template <typename T1, typename T2>
long ibis::part::fill3DBins3(const ibis::bitvector &mask,
                             const array_t<T1> &val1,
                             const double &begin1, const double &end1,
                             const double &stride1,
                             const array_t<T2> &val2,
                             const double &begin2, const double &end2,
                             const double &stride2,
                             const ibis::column &col3,
                             const double &begin3, const double &end3,
                             const double &stride3,
                             std::vector<ibis::bitvector> &bins) const {
    long ierr = 0;
    switch (col3.type()) {
#ifdef FASTBIT_EXPAND_ALL_TYPES
    case ibis::BYTE: {
        array_t<signed char>* val3;
        if (mask.cnt() > (nEvents >> 4)) {
            val3 = new array_t<signed char>;
            ierr = col3.getValuesArray(val3);
            if (ierr < 0) {
                delete val3;
                val3 = col3.selectBytes(mask);
            }
        }
        else {
            val3 = col3.selectBytes(mask);
        }
        if (val3 == 0) return -8L;
        ierr = fill3DBins(mask, val1, begin1, end1, stride1,
                          val2, begin2, end2, stride2,
                          *val3, begin3, end3, stride3, bins);
        delete val3;
        break;}
    case ibis::UBYTE: {
        array_t<unsigned char>* val3;
        if (mask.cnt() > (nEvents >> 4)) {
            val3 = new array_t<unsigned char>;
            ierr = col3.getValuesArray(val3);
            if (ierr < 0) {
                delete val3;
                val3 = col3.selectUBytes(mask);
            }
        }
        else {
            val3 = col3.selectUBytes(mask);
        }
        if (val3 == 0) return -8L;
        ierr = fill3DBins(mask, val1, begin1, end1, stride1,
                          val2, begin2, end2, stride2,
                          *val3, begin3, end3, stride3, bins);
        delete val3;
        break;}
    case ibis::SHORT: {
        array_t<int16_t>* val3;
        if (mask.cnt() > (nEvents >> 4)) {
            val3 = new array_t<int16_t>;
            ierr = col3.getValuesArray(val3);
            if (ierr < 0) {
                delete val3;
                val3 = col3.selectShorts(mask);
            }
        }
        else {
            val3 = col3.selectShorts(mask);
        }
        if (val3 == 0) return -8L;
        ierr = fill3DBins(mask, val1, begin1, end1, stride1,
                          val2, begin2, end2, stride2,
                          *val3, begin3, end3, stride3, bins);
        delete val3;
        break;}
    case ibis::USHORT: {
        array_t<uint16_t>* val3;
        if (mask.cnt() > (nEvents >> 4)) {
            val3 = new array_t<uint16_t>;
            ierr = col3.getValuesArray(val3);
            if (ierr < 0) {
                delete val3;
                val3 = col3.selectUShorts(mask);
            }
        }
        else {
            val3 = col3.selectUShorts(mask);
        }
        if (val3 == 0) return -8L;
        ierr = fill3DBins(mask, val1, begin1, end1, stride1,
                          val2, begin2, end2, stride2,
                          *val3, begin3, end3, stride3, bins);
        delete val3;
        break;}
#endif
#ifndef FASTBIT_EXPAND_ALL_TYPES
    case ibis::BYTE:
    case ibis::SHORT:
#endif
    case ibis::INT: {
        array_t<int32_t>* val3;
        if (mask.cnt() > (nEvents >> 4)
#ifndef FASTBIT_EXPAND_ALL_TYPES
            && col3.type() == ibis::INT
#endif
            ) {
            val3 = new array_t<int32_t>;
            ierr = col3.getValuesArray(val3);
            if (ierr < 0) {
                delete val3;
                val3 = col3.selectInts(mask);
            }
        }
        else {
            val3 = col3.selectInts(mask);
        }
        if (val3 == 0) return -8L;
        ierr = fill3DBins(mask, val1, begin1, end1, stride1,
                          val2, begin2, end2, stride2,
                          *val3, begin3, end3, stride3, bins);
        delete val3;
        break;}
#ifndef FASTBIT_EXPAND_ALL_TYPES
    case ibis::UBYTE:
    case ibis::USHORT:
#endif
    case ibis::CATEGORY:
    case ibis::UINT: {
        array_t<uint32_t>* val3;
        if (mask.cnt() > (nEvents >> 4) && col3.type() == ibis::UINT) {
            val3 = new array_t<uint32_t>;
            ierr = col3.getValuesArray(val3);
            if (ierr < 0) {
                delete val3;
                val3 = col3.selectUInts(mask);
            }
        }
        else {
            val3 = col3.selectUInts(mask);
        }
        if (val3 == 0) return -8L;
        ierr = fill3DBins(mask, val1, begin1, end1, stride1, 
                          val2, begin2, end2, stride2,
                          *val3, begin3, end3, stride3, bins);
        delete val3;
        break;}
    case ibis::ULONG:
#ifdef FASTBIT_EXPAND_ALL_TYPES
        {
        array_t<uint64_t>* val3;
        if (mask.cnt() > (nEvents >> 4)) {
            val3 = new array_t<uint64_t>;
            ierr = col3.getValuesArray(val3);
            if (ierr < 0) {
                delete val3;
                val3 = col3.selectULongs(mask);
            }
        }
        else {
            val3 = col3.selectULongs(mask);
        }
        if (val3 == 0) return -8L;
        ierr = fill3DBins(mask, val1, begin1, end1, stride1,
                          val2, begin2, end2, stride2,
                          *val3, begin3, end3, stride3, bins);
        delete val3;
        break;}
#endif
    case ibis::LONG: {
        array_t<int64_t>* val3;
        if (mask.cnt() > (nEvents >> 4)) {
            val3 = new array_t<int64_t>;
            ierr = col3.getValuesArray(val3);
            if (ierr < 0) {
                delete val3;
                val3 = col3.selectLongs(mask);
            }
        }
        else {
            val3 = col3.selectLongs(mask);
        }
        if (val3 == 0) return -8L;
        ierr = fill3DBins(mask, val1, begin1, end1, stride1,
                          val2, begin2, end2, stride2,
                          *val3, begin3, end3, stride3, bins);
        delete val3;
        break;}
    case ibis::FLOAT: {
        array_t<float>* val3;
        if (mask.cnt() > (nEvents >> 4)) {
            val3 = new array_t<float>;
            ierr = col3.getValuesArray(val3);
            if (ierr < 0) {
                delete val3;
                val3 = col3.selectFloats(mask);
            }
        }
        else {
            val3 = col3.selectFloats(mask);
        }
        if (val3 == 0) return -8L;
        ierr = fill3DBins(mask, val1, begin1, end1, stride1,
                          val2, begin2, end2, stride2,
                          *val3, begin3, end3, stride3, bins);
        delete val3;
        break;}
    case ibis::DOUBLE: {
        array_t<double>* val3;
        if (mask.cnt() > (nEvents >> 4)) {
            val3 = new array_t<double>;
            ierr = col3.getValuesArray(val3);
            if (ierr < 0) {
                delete val3;
                val3 = col3.selectDoubles(mask);
            }
        }
        else {
            val3 = col3.selectDoubles(mask);
        }
        if (val3 == 0) return -8L;
        ierr = fill3DBins(mask, val1, begin1, end1, stride1,
                          val2, begin2, end2, stride2,
                          *val3, begin3, end3, stride3, bins);
        delete val3;
        break;}
    default: {
        LOGGER(ibis::gVerbose > 3)
            << "part::fill3DBins3 -- can not "
            "handle column (" << col3.name() << ") type "
            << ibis::TYPESTRING[(int)col3.type()];

        ierr = -7;
        break;}
    }
    return ierr;
} // ibis::part::fill3DBins3

/// Resolve the 2nd column of the 3D bins.  It invokes
/// ibis::part::fill3DBins3 to resolve the 3rd dimension and finally
/// ibis::part::fill3DBins to perform the actual binning.
template <typename T1>
long ibis::part::fill3DBins2(const ibis::bitvector &mask,
                             const array_t<T1> &val1,
                             const double &begin1, const double &end1,
                             const double &stride1,
                             const ibis::column &col2,
                             const double &begin2, const double &end2,
                             const double &stride2,
                             const ibis::column &col3,
                             const double &begin3, const double &end3,
                             const double &stride3,
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
        ierr = fill3DBins3(mask, val1, begin1, end1, stride1,
                           *val2, begin2, end2, stride2,
                           col3, begin3, end3, stride3, bins);
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
        ierr = fill3DBins3(mask, val1, begin1, end1, stride1,
                           *val2, begin2, end2, stride2,
                           col3, begin3, end3, stride3, bins);
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
        ierr = fill3DBins3(mask, val1, begin1, end1, stride1,
                           *val2, begin2, end2, stride2,
                           col3, begin3, end3, stride3, bins);
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
        ierr = fill3DBins3(mask, val1, begin1, end1, stride1,
                           *val2, begin2, end2, stride2,
                           col3, begin3, end3, stride3, bins);
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
#ifdef FASTBIT_EXPAND_ALL_TYPES
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
        ierr = fill3DBins3(mask, val1, begin1, end1, stride1,
                           *val2, begin2, end2, stride2,
                           col3, begin3, end3, stride3, bins);
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
        ierr = fill3DBins3(mask, val1, begin1, end1, stride1, 
                           *val2, begin2, end2, stride2,
                           col3, begin3, end3, stride3, bins);
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
        ierr = fill3DBins3(mask, val1, begin1, end1, stride1,
                           *val2, begin2, end2, stride2,
                           col3, begin3, end3, stride3, bins);
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
        ierr = fill3DBins3(mask, val1, begin1, end1, stride1,
                           *val2, begin2, end2, stride2,
                           col3, begin3, end3, stride3, bins);
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
        ierr = fill3DBins3(mask, val1, begin1, end1, stride1,
                           *val2, begin2, end2, stride2,
                           col3, begin3, end3, stride3, bins);
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
        ierr = fill3DBins3(mask, val1, begin1, end1, stride1,
                           *val2, begin2, end2, stride2,
                           col3, begin3, end3, stride3, bins);
        delete val2;
        break;}
    default: {
        LOGGER(ibis::gVerbose > 3)
            << "part::fill3DBins2 -- can not "
            "handle column (" << col2.name() << ") type "
            << ibis::TYPESTRING[(int)col2.type()];

        ierr = -5;
        break;}
    }
    return ierr;
} // ibis::part::fill3DBins2

/// This function calls ibis::part::fill3DBins and other helper functions
/// to compute the 3D bins.  On successful completion, it returns the
/// number of elements in variable bins.  In other word, it returns the
/// number of bins generated, which should be exactly @code
/// (1 + floor((end1-begin1)/stride1)) *
/// (1 + floor((end2-begin2)/stride2)) *
/// (1 + floor((end3-begin3)/stride3))
/// @endcode
/// It returns a negative value to indicate error.  Please refer to the
/// documentation of ibis::part::fill3DBins for additional information
/// about the objects returned in bins.
///
/// @note This function is intended to work with numerical values.  It
/// treats categorical values as unsigned ints.  Passing the name of text
/// column to this function will result in a negative return value.
long ibis::part::get3DBins(const char *constraints, const char *cname1,
                           double begin1, double end1, double stride1,
                           const char *cname2,
                           double begin2, double end2, double stride2,
                           const char *cname3,
                           double begin3, double end3, double stride3,
                           std::vector<ibis::bitvector> &bins) const {
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
            << "]::get3DBins attempting to compute a histogram of "
            << cname1 << ", " << cname2 << ", and " << cname3
            << " with regular binning "
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
        if (ierr <= 0) return ierr;

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
        ierr = fill3DBins2(mask, *val1, begin1, end1, stride1,
                           *col2, begin2, end2, stride2,
                           *col3, begin3, end3, stride3, bins);
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
        ierr = fill3DBins2(mask, *val1, begin1, end1, stride1,
                           *col2, begin2, end2, stride2,
                           *col3, begin3, end3, stride3, bins);
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
        ierr = fill3DBins2(mask, *val1, begin1, end1, stride1,
                           *col2, begin2, end2, stride2,
                           *col3, begin3, end3, stride3, bins);
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
        ierr = fill3DBins2(mask, *val1, begin1, end1, stride1,
                           *col2, begin2, end2, stride2,
                           *col3, begin3, end3, stride3, bins);
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
        ierr = fill3DBins2(mask, *val1, begin1, end1, stride1,
                           *col2, begin2, end2, stride2,
                           *col3, begin3, end3, stride3, bins);
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
        ierr = fill3DBins2(mask, *val1, begin1, end1, stride1, 
                           *col2, begin2, end2, stride2,
                           *col3, begin3, end3, stride3, bins);
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
        ierr = fill3DBins2(mask, *val1, begin1, end1, stride1,
                           *col2, begin2, end2, stride2,
                           *col3, begin3, end3, stride3, bins);
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
        ierr = fill3DBins2(mask, *val1, begin1, end1, stride1,
                           *col2, begin2, end2, stride2,
                           *col3, begin3, end3, stride3, bins);
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
        ierr = fill3DBins2(mask, *val1, begin1, end1, stride1,
                           *col2, begin2, end2, stride2,
                           *col3, begin3, end3, stride3, bins);
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
        ierr = fill3DBins2(mask, *val1, begin1, end1, stride1,
                           *col2, begin2, end2, stride2,
                           *col3, begin3, end3, stride3, bins);
        delete val1;
        break;}
    default: {
        LOGGER(ibis::gVerbose > 3)
            << "part::get3DBins -- can not "
            "handle column (" << cname1 << ") type "
            << ibis::TYPESTRING[(int)col1->type()];

        ierr = -3;
        break;}
    }
    if (ierr > 0 && ibis::gVerbose > 0) {
        timer.stop();
        logMessage("get3DBins", "computing the distribution of column "
                   "%s, %s and %s%s%s took %g sec(CPU), %g sec(elapsed)",
                   cname1, cname2, cname3,
                   (constraints ? " with restriction " : ""),
                   (constraints ? constraints : ""),
                   timer.CPUTime(), timer.realTime());
    }
    return ierr;
} // ibis::part::get3DBins
