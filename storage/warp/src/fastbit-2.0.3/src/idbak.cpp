// $Id$
// Author: John Wu <John.Wu at ACM.org>
// Copyright (c) 2000-2016 the Regents of the University of California
//
// This file contains the implementation of the class ibis::bak
//
#if defined(_WIN32) && defined(_MSC_VER)
#pragma warning(disable:4786)   // some identifier longer than 256 characters
#endif
#include "ibin.h"
#include "part.h"
#include "column.h"
#include "resource.h"

#include <iterator>     // std::ostream_iterator

// construct a bitmap index from current data
ibis::bak::bak(const ibis::column* c, const char* f) : ibis::bin() {
    if (c == 0) return;  // nothing can be done
    col = c;

    try {
        if (f) { // f is not null
            read(f);
        }
        if (nobs == 0) {
            bakMap bmap;
            mapValues(f, bmap);
            construct(bmap);
            optionalUnpack(bits, col->indexSpec());

            if (ibis::gVerbose > 4) {
                ibis::util::logger lg;
                print(lg());
            }
        }
    }
    catch (...) {
        clear();
        throw;
    }
} // constructor

// read from a file
int ibis::bak::read(const char* f) {
    int ierr = -1;
    try {
        std::string fnm;
        indexFileName(fnm, f);
        if (ibis::index::isIndex(fnm.c_str(), ibis::index::BAK)) {
            ierr = ibis::bin::read(f);
        }
    }
    catch (...) {
        clear();
        throw;
    }
    return ierr;
} // ibis::bak::read

// locates the first bin that is just to the right of val or covers val
// return the smallest i such that maxval[i] >= val
uint32_t ibis::bak::locate(const double& val) const {
#if DEBUG+0 > 0 || _DEBUG+0 > 0
    ibis::util::logMessage("bak::locate", "searching for %g in an "
                           "array of %lu double(s) in the range of [%g, %g]",
                           val, static_cast<long unsigned>(minval.size()),
                           minval[0], maxval.back());
#endif
    // check the extreme cases -- use negative tests to capture abnormal
    // numbers
    if (minval.empty()) return 0;
    if (! (val > maxval[0])) {
        return 0;
    }
    else if (! (val <= maxval[nobs-1])) {
        return nobs;
    }

    // the normal cases -- two different search strategies
    if (nobs >= 8) { // binary search
        uint32_t i0 = 0, i1 = nobs, it = nobs/2;
        while (i0 < it) { // maxval[i1] >= val
            if (val <= maxval[it])
                i1 = it;
            else
                i0 = it;
            it = (i0 + i1) / 2;
        }
#if DEBUG+0 > 0 || _DEBUG+0 > 0
        ibis::util::logMessage("locate", "%g in [%g, %g) ==> %lu", val,
                               maxval[i0], maxval[i1],
                               static_cast<long unsigned>(i1));
#endif
        return i1;
    }
    else { // do linear search
        for (uint32_t i = 0; i < nobs; ++i) {
            if (val <= maxval[i]) {
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                if (i > 0)
                    ibis::util::logMessage("locate", "%g in [%g, %g) ==> %lu",
                                           val, maxval[i-1], maxval[i],
                                           static_cast<long unsigned>(i));
                else
                    ibis::util::logMessage("locate", "%g in (..., %g) ==> 0",
                                           val, maxval[i]);
#endif
                return i;
            }
        }
    }
    return nobs;
} // ibis::bak::locate

// This function reads the data file and records the locations of the values
// in bakMap
void ibis::bak::mapValues(const char* f, ibis::bak::bakMap& bmap) const {
    if (col == 0) return;

    horometer timer;
    if (ibis::gVerbose > 4)
        timer.start();

    const unsigned prec = parsePrec(*col); // the precision of mapped value

    std::string fnm; // name of the data file
    dataFileName(fnm, f);
    if (fnm.empty()) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- bak::mapValues failed to determine the data file "
            "name from \"" << (f ? f : "") << '"';
        return;
    }

    uint32_t nev;
    ibis::bitvector mask;
    col->getNullMask(mask);
    if (col->partition() != 0)
        nev = col->partition()->nRows();
    else
        nev = mask.size();
    if (nev == 0) return;

    // need to use different types of array_t for different columns
    switch (col->type()) {
    case ibis::TEXT:
    case ibis::UINT: {// unsigned int
        array_t<uint32_t> val;
        if (! fnm.empty())
            ibis::fileManager::instance().getFile(fnm.c_str(), val);
        else
            col->getValuesArray(&val);
        if (val.size() <= 0) {
            col->logWarning("bak::mapValues", "failed to read %s",
                            fnm.c_str());
        }
        else {
            bmap.clear();
            nev = val.size();
            if (nev > mask.size())
                mask.adjustSize(nev, nev);
            ibis::bitvector::indexSet iset = mask.firstIndexSet();
            uint32_t nind = iset.nIndices();
            const ibis::bitvector::word_t *iix = iset.indices();
            while (nind) {
                if (iset.isRange()) { // a range
                    uint32_t k = (iix[1] < nev ? iix[1] : nev);
                    for (uint32_t i = *iix; i < k; ++i) {
                        double key = ibis::util::coarsen(val[i], prec);
                        ibis::bak::grain& grn = bmap[key];
                        if (grn.loc == 0)
                            grn.loc = new ibis::bitvector;
                        grn.loc->setBit(i, 1);
                        if (grn.min > val[i]) grn.min = val[i];
                        if (grn.max < val[i]) grn.max = val[i];
                    }
                }
                else if (*iix+ibis::bitvector::bitsPerLiteral() < nev) {
                    // a list of indices
                    for (uint32_t i = 0; i < nind; ++i) {
                        uint32_t k = iix[i];
                        double key = ibis::util::coarsen(val[k], prec);
                        ibis::bak::grain& grn = bmap[key];
                        if (grn.loc == 0) grn.loc = new ibis::bitvector;
                        grn.loc->setBit(k, 1);
                        if (grn.min > val[k]) grn.min = val[k];
                        if (grn.max < val[k]) grn.max = val[k];
                    }
                }
                else {
                    for (uint32_t i = 0; i < nind; ++i) {
                        uint32_t k = iix[i];
                        if (k < nev) {
                            double key = ibis::util::coarsen(val[k], prec);
                            ibis::bak::grain& grn = bmap[key];
                            if (grn.loc == 0)
                                grn.loc = new ibis::bitvector;
                            grn.loc->setBit(k, 1);
                            if (grn.min > val[k]) grn.min = val[k];
                            if (grn.max < val[k]) grn.max = val[k];
                        }
                    }
                }
                ++iset;
                nind = iset.nIndices();
                if (*iix >= nev) nind = 0;
            } // while (nind)
        }
        break;}
    case ibis::INT: {// signed int
        array_t<int32_t> val;
        if (! fnm.empty())
            ibis::fileManager::instance().getFile(fnm.c_str(), val);
        else
            col->getValuesArray(&val);
        if (val.size() <= 0) {
            col->logWarning("bak::mapValues", "failed to read %s",
                            fnm.c_str());
        }
        else {
            nev = val.size();
            if (nev > mask.size())
                mask.adjustSize(nev, nev);
            ibis::bitvector::indexSet iset = mask.firstIndexSet();
            uint32_t nind = iset.nIndices();
            const ibis::bitvector::word_t *iix = iset.indices();
            while (nind) {
                if (iset.isRange()) { // a range
                    uint32_t k = (iix[1] < nev ? iix[1] : nev);
                    for (uint32_t i = *iix; i < k; ++i) {
                        double key = ibis::util::coarsen(val[i], prec);
                        ibis::bak::grain& grn = bmap[key];
                        if (grn.loc == 0)
                            grn.loc = new ibis::bitvector;
                        grn.loc->setBit(i, 1);
                        if (grn.min > val[i]) grn.min = val[i];
                        if (grn.max < val[i]) grn.max = val[i];
                    }
                }
                else if (*iix+ibis::bitvector::bitsPerLiteral() < nev) {
                    // a list of indices
                    for (uint32_t i = 0; i < nind; ++i) {
                        uint32_t k = iix[i];
                        double key = ibis::util::coarsen(val[k], prec);
                        ibis::bak::grain& grn = bmap[key];
                        if (grn.loc == 0)
                            grn.loc = new ibis::bitvector;
                        grn.loc->setBit(k, 1);
                        if (grn.min > val[k]) grn.min = val[k];
                        if (grn.max < val[k]) grn.max = val[k];
                    }
                }
                else {
                    for (uint32_t i = 0; i < nind; ++i) {
                        uint32_t k = iix[i];
                        if (k < nev) {
                            double key = ibis::util::coarsen(val[k], prec);
                            ibis::bak::grain& grn = bmap[key];
                            if (grn.loc == 0)
                                grn.loc = new ibis::bitvector;
                            grn.loc->setBit(k, 1);
                            if (grn.min > val[k]) grn.min = val[k];
                            if (grn.max < val[k]) grn.max = val[k];
                        }
                    }
                }
                ++iset;
                nind = iset.nIndices();
                if (*iix >= nev) nind = 0;
            } // while (nind)
        }
        break;}
    case ibis::FLOAT: {// (4-byte) floating-point values
        array_t<float> val;
        if (! fnm.empty())
            ibis::fileManager::instance().getFile(fnm.c_str(), val);
        else
            col->getValuesArray(&val);
        if (val.size() <= 0) {
            col->logWarning("bak::mapValues", "failed to read %s",
                            fnm.c_str());
        }
        else {
            nev = val.size();
            if (nev > mask.size())
                mask.adjustSize(nev, nev);
            ibis::bitvector::indexSet iset = mask.firstIndexSet();
            uint32_t nind = iset.nIndices();
            const ibis::bitvector::word_t *iix = iset.indices();
            while (nind) {
                if (iset.isRange()) { // a range
                    uint32_t k = (iix[1] < nev ? iix[1] : nev);
                    for (uint32_t i = *iix; i < k; ++i) {
                        double key = ibis::util::coarsen(val[i], prec);
                        ibis::bak::grain& grn = bmap[key];
                        if (grn.loc == 0)
                            grn.loc = new ibis::bitvector;
                        grn.loc->setBit(i, 1);
                        if (grn.min > val[i]) grn.min = val[i];
                        if (grn.max < val[i]) grn.max = val[i];
                    }
                }
                else if (*iix+ibis::bitvector::bitsPerLiteral() < nev) {
                    // a list of indices
                    for (uint32_t i = 0; i < nind; ++i) {
                        uint32_t k = iix[i];
                        double key = ibis::util::coarsen(val[k], prec);
                        ibis::bak::grain& grn = bmap[key];
                        if (grn.loc == 0)
                            grn.loc = new ibis::bitvector;
                        grn.loc->setBit(k, 1);
                        if (grn.min > val[k]) grn.min = val[k];
                        if (grn.max < val[k]) grn.max = val[k];
                    }
                }
                else {
                    for (uint32_t i = 0; i < nind; ++i) {
                        uint32_t k = iix[i];
                        if (k < nev) {
                            double key = ibis::util::coarsen(val[k], prec);
                            ibis::bak::grain& grn = bmap[key];
                            if (grn.loc == 0)
                                grn.loc = new ibis::bitvector;
                            grn.loc->setBit(k, 1);
                            if (grn.min > val[k]) grn.min = val[k];
                            if (grn.max < val[k]) grn.max = val[k];
                        }
                    }
                }
                ++iset;
                nind = iset.nIndices();
                if (*iix >= nev) nind = 0;
            } // while (nind)
        }
        break;}
    case ibis::DOUBLE: {// (8-byte) floating-point values
        array_t<double> val;
        if (! fnm.empty())
            ibis::fileManager::instance().getFile(fnm.c_str(), val);
        else
            col->getValuesArray(&val);
        if (val.size() <= 0) {
            col->logWarning("bak::mapValues", "failed to read %s",
                            fnm.c_str());
        }
        else {
            nev = val.size();
            if (nev > mask.size())
                mask.adjustSize(nev, nev);
            ibis::bitvector::indexSet iset = mask.firstIndexSet();
            uint32_t nind = iset.nIndices();
            const ibis::bitvector::word_t *iix = iset.indices();
            while (nind) {
                if (iset.isRange()) { // a range
                    uint32_t k = (iix[1] < nev ? iix[1] : nev);
                    for (uint32_t i = *iix; i < k; ++i) {
                        double key = ibis::util::coarsen(val[i], prec);
                        ibis::bak::grain& grn = bmap[key];
                        if (grn.loc == 0)
                            grn.loc = new ibis::bitvector;
                        grn.loc->setBit(i, 1);
                        if (grn.min > val[i]) grn.min = val[i];
                        if (grn.max < val[i]) grn.max = val[i];
                    }
                }
                else if (*iix+ibis::bitvector::bitsPerLiteral() < nev) {
                    // a list of indices
                    for (uint32_t i = 0; i < nind; ++i) {
                        uint32_t k = iix[i];
                        double key = ibis::util::coarsen(val[k], prec);
                        ibis::bak::grain& grn = bmap[key];
                        if (grn.loc == 0)
                            grn.loc = new ibis::bitvector;
                        grn.loc->setBit(k, 1);
                        if (grn.min > val[k]) grn.min = val[k];
                        if (grn.max < val[k]) grn.max = val[k];
                    }
                }
                else {
                    for (uint32_t i = 0; i < nind; ++i) {
                        uint32_t k = iix[i];
                        if (k < nev) {
                            double key = ibis::util::coarsen(val[k], prec);
                            ibis::bak::grain& grn = bmap[key];
                            if (grn.loc == 0)
                                grn.loc = new ibis::bitvector;
                            grn.loc->setBit(k, 1);
                            if (grn.min > val[k]) grn.min = val[k];
                            if (grn.max < val[k]) grn.max = val[k];
                        }
                    }
                }
                ++iset;
                nind = iset.nIndices();
                if (*iix >= nev) nind = 0;
            } // while (nind)
        }
        break;}
    case ibis::CATEGORY: // no need for a separate index
        col->logWarning("bak::mapValues", "no need for binning -- should have "
                        "a basic bitmap index already");
        return;
    default:
        col->logWarning("bak::mapValues", "failed to create bins for "
                        "this type of column");
        return;
    }

    // make sure all bit vectors are the same size
    for (ibis::bak::bakMap::iterator it = bmap.begin();
         it != bmap.end(); ++ it) {
        (*it).second.loc->adjustSize(0, nev);
        //      (*it).second.loc->compress();
    }

    // write out the current content
    if (ibis::gVerbose > 2) {
        if (ibis::gVerbose > 4) {
            timer.stop();
            col->logMessage("bak::mapValues", "mapped %lu values to %lu "
                            "%u-digit number%s in %g sec(elapsed)",
                            static_cast<long unsigned>(nev),
                            static_cast<long unsigned>(bmap.size()),
                            prec, (bmap.size() > 1 ? "s" : ""),
                            timer.realTime());
        }
        else {
            col->logMessage("bak::mapValues", "mapped %lu values to %lu "
                            "%lu-digit number%s",
                            static_cast<long unsigned>(nev),
                            static_cast<long unsigned>(bmap.size()), prec,
                            (bmap.size() > 1 ? "s" : ""));
        }
        if (ibis::gVerbose > 6) {
            ibis::util::logger lg;
            printMap(lg(), bmap);
        }
    }
} // ibis::bak::mapValues

void ibis::bak::printMap(std::ostream& out,
                         const ibis::bak::bakMap& bmap) const {
    out << "bak::printMap(" << bmap.size()
        << (bmap.size() > 1 ? " entries" : " entry")
        << " [key, min, max, count]"
        << std::endl;
    uint32_t prt = (ibis::gVerbose > 30 ? bmap.size() : (1 << ibis::gVerbose));
    if (prt < 5) prt = 5;
    if (prt+1 >= bmap.size()) { // print all
        for (ibis::bak::bakMap::const_iterator it = bmap.begin();
             it != bmap.end(); ++ it)
            out << (*it).first << "\t" << (*it).second.min << "\t"
                << (*it).second.max << "\t" << (*it).second.loc->cnt() << "\n";
    }
    else { // print some
        ibis::bak::bakMap::const_iterator it = bmap.begin();
        for (uint32_t i = 0; i < prt; ++i, ++it)
            out << (*it).first << "\t" << (*it).second.min << "\t"
                << (*it).second.max << "\t" << (*it).second.loc->cnt() << "\n";
        prt =  bmap.size() - prt - 1;
        it = bmap.end();
        -- it;
        out << "...\n" << prt << (prt > 1 ? " entries" : " entry")
            << " omitted\n...\n";
        out << (*it).first << "\t"
            << (*it).second.min << "\t"
            << (*it).second.max << "\t"
            << (*it).second.loc->cnt() << "\n";
    }
    out << std::endl;
} //ibis::bak::printMap

/// Write the index to the named directory or file.
int ibis::bak::write(const char* dt) const {
    if (nobs <= 0) return -1;

    return ibis::bin::write(dt);
} // ibis::bak::write

// covert the hash structure in bakMap into the array structure in ibis::bin
// NOTE: the pointers to bitvectors in bakMap is copied! Those pointers
// should NOT be deleted when freeing the bakMap!
void ibis::bak::construct(ibis::bak::bakMap& bmap) {
    // clear the existing content
    clear();

    // initialize the arrays
    nobs = bmap.size();
    bits.resize(nobs);
    bounds.resize(nobs);
    minval.resize(nobs);
    maxval.resize(nobs);

    // copy the values
    ibis::bak::bakMap::iterator ib = bmap.begin();
    for (uint32_t i = 0; i < nobs; ++ i, ++ib) {
        bits[i] = (*ib).second.loc;
        bounds[i] = (*ib).first;
        minval[i] = (*ib).second.min;
        maxval[i] = (*ib).second.max;
        if (nrows == 0 && (*ib).second.loc != 0)
            nrows = (*ib).second.loc->size();
        (*ib).second.loc = 0;
    }
} // ibis::bak::construct

void ibis::bak::binBoundaries(std::vector<double>& ret) const {
    ret.clear();
    if (nobs == 0) return;
    ret.reserve(nobs+1);
    ret.push_back(ibis::util::compactValue(-DBL_MAX, minval[0]));
    for (uint32_t i = 0; i < nobs-1; ++i)
        ret.push_back(ibis::util::compactValue
                      (ibis::util::incrDouble(maxval[i]),
                       minval[i+1]));
    ret.push_back(ibis::util::compactValue
                  (ibis::util::incrDouble(maxval.back()), DBL_MAX));
} // ibis::bak::bakBoundaries()

void ibis::bak::binWeights(std::vector<uint32_t>& ret) const {
    activate(); // make sure all bitvectors are available
    ret.resize(nobs+1);
    ret[0] = 0;
    for (uint32_t i=0; i<nobs; ++i)
        if (bits[i])
            ret[i+1] = bits[i]->cnt();
        else
            ret[i+1] = 0;
} // ibis::bak::bakWeights()

// the printing function
void ibis::bak::print(std::ostream& out) const {
    if (nrows == 0) return;

    // activate(); -- active may invoke ioLock which causes problems
    out << "index (equality encoding on reduced precision values) for "
        << col->fullname() << " contains " << nobs << " bitvectors for "
        << nrows << " objects \n";
    if (ibis::gVerbose > 0) {
        uint32_t prt = (ibis::gVerbose > 30 ? nobs : (1 << ibis::gVerbose));
        if (prt < 5) prt = 5;
        if (prt+prt+1 >= nobs) { // print all
            for (uint32_t i = 0; i < nobs; ++ i) {
                if (bits[i]) {
                    out << bounds[i] << "\t" << minval[i] << "\t"
                        << maxval[i] << "\t" << bits[i]->cnt() << "\n";
                    if (bits[i]->size() != nrows)
                        out << "ERROR: bits[" << i << "]->size("
                            << bits[i]->size()
                            << ") differs from nrows ("
                            << nrows << ")\n";
                }
                else {
                    out << bounds[i] << "\t" << minval[i] << "\t"
                        << maxval[i] << "\n";
                }
            }
        }
        else {
            for (uint32_t i = 0; i < prt; ++ i) {
                if (bits[i]) {
                    out << bounds[i] << "\t" << minval[i] << "\t"
                        << maxval[i] << "\t" << bits[i]->cnt() << "\n";
                    if (bits[i]->size() != nrows)
                        out << "ERROR: bits[" << i << "]->size("
                            << bits[i]->size()
                            << ") differs from nrows ("
                            << nrows << ")\n";
                }
                else {
                    out << bounds[i] << "\t" << minval[i] << "\t"
                        << maxval[i] << "\n";
                }
            }
            prt = nobs - prt - 1;
            out << "...\n" << prt << (prt > 1 ? " entries" : " entry")
                << " omitted\n...\n";
            if (bits.back()) {
                out << bounds.back() << "\t" << minval.back() << "\t"
                    << maxval.back() << "\t" << bits.back()->cnt() << "\n";
                if (bits.back()->size() != nrows)
                    out << "ERROR: bits[" << nobs-1 << "]->size("
                        << bits.back()->size()
                        << ") differs from nrows ("
                        << nrows << ")\n";
            }
            else {
                out << bounds.back() << "\t" << minval.back() << "\t"
                    << maxval.back() << "\n";
            }
        }
    }
    out << std::endl;
} // ibis::bak::print()

// this function simply recreates the index using the current data in dt
// directory
long ibis::bak::append(const char* dt, const char* df, uint32_t nnew) {
    if (nnew == 0)
        return 0;

    clear(); // clear the current content and rebuild index in dt
    bakMap bmap;
    mapValues(dt, bmap);
    construct(bmap);
    optionalUnpack(bits, col->indexSpec());
    //write(dt); // record the new index

    if (ibis::gVerbose > 2) {
        ibis::util::logger lg;
        print(lg());
    }
    return nnew;
} // ibis::bak::append

// expand range condition -- rely on the fact that the only operators used
// are LT, LE and EQ
int ibis::bak::expandRange(ibis::qContinuousRange& rng) const {
    uint32_t cand0, cand1;
    double left, right;
    int ret = 0;
    ibis::bin::locate(rng, cand0, cand1);
    if (rng.leftOperator() == ibis::qExpr::OP_LT) {
        if (cand0 < minval.size() && rng.leftBound() >= minval[cand0]) {
            // reduce left bound
            ++ ret;
            right = minval[cand0];
            if (cand0 > 0)
                left = maxval[cand0-1];
            else
                left = -DBL_MAX;
            rng.leftBound() = ibis::util::compactValue(left, right);
        }
    }
    else if (rng.leftOperator() == ibis::qExpr::OP_LE) {
        if (cand0 < minval.size() && rng.leftBound() > minval[cand0]) {
            // reduce left bound
            ++ ret;
            right = minval[cand0];
            if (cand0 > 0)
                left = maxval[cand0-1];
            else
                left = -DBL_MAX;
            rng.leftBound() = ibis::util::compactValue(left, right);
        }
    }
    else if (rng.leftOperator() == ibis::qExpr::OP_EQ) {
        if (cand0 < minval.size() && minval[cand0] < maxval[cand0] &&
            rng.leftBound() >= minval[cand0] &&
            rng.leftBound() <= maxval[cand0]) {
            // change equality condition to a two-sided range condition
            ++ ret;
            right = minval[cand0];
            if (cand0 > 0)
                left = maxval[cand0-1];
            else
                left = -DBL_MAX;
            rng.leftOperator() = ibis::qExpr::OP_LE;
            rng.leftBound() = ibis::util::compactValue(left, right);
            left = maxval[cand0];
            if (cand0+1 < minval.size())
                right = minval[cand0+1];
            else
                right = DBL_MAX;
            rng.rightOperator() = ibis::qExpr::OP_LE;
            rng.rightBound() = ibis::util::compactValue(left, right);
        }
    }

    if (rng.rightOperator() == ibis::qExpr::OP_LT) {
        if (cand1 > 0 && rng.rightBound() <= maxval[cand1-1]) {
            // increase right bound
            ++ ret;
            left = maxval[cand1-1];
            if (cand1 < minval.size())
                right = minval[cand1];
            else
                right = DBL_MAX;
            rng.rightBound() = ibis::util::compactValue(left, right);
        }
    }
    else if (rng.rightOperator() == ibis::qExpr::OP_LE) {
        if (cand1 > 0 && rng.rightBound() < maxval[cand1-1]) {
            // increase right bound
            ++ ret;
            left = maxval[cand1-1];
            if (cand1 < minval.size())
                right = minval[cand1];
            else
                right = DBL_MAX;
            rng.rightBound() = ibis::util::compactValue(left, right);
        }
    }
    return ret;
} // ibis::bak::expandRange

// contract range condition -- rely on the fact that the only operators used
// are LT, LE and EQ
int ibis::bak::contractRange(ibis::qContinuousRange& rng) const {
    uint32_t cand0, cand1;
    double left, right;
    int ret = 0;
    ibis::bin::locate(rng, cand0, cand1);
    if (rng.leftOperator() == ibis::qExpr::OP_LT) {
        if (cand0 < minval.size() && rng.leftBound() <= maxval[cand0]) {
            // increase left bound
            ++ ret;
            left = maxval[cand0];
            if (cand0+1 < minval.size())
                right = minval[cand0+1];
            else
                right = DBL_MAX;
            rng.leftBound() = ibis::util::compactValue(left, right);
        }
    }
    else if (rng.leftOperator() == ibis::qExpr::OP_LE) {
        if (cand0 < minval.size() && rng.leftBound() < maxval[cand0]) {
            // increase left bound
            ++ ret;
            left = maxval[cand0];
            if (cand0+1 < minval.size())
                right = minval[cand0+1];
            else
                right = DBL_MAX;
            rng.leftBound() = ibis::util::compactValue(left, right);
        }
    }
    else if (rng.leftOperator() == ibis::qExpr::OP_EQ) {
        if (cand0 < minval.size() && minval[cand0] < maxval[cand1] &&
            rng.leftBound() >= minval[cand0] &&
            rng.leftBound() <= maxval[cand0]) {
            // make an equality with empty results
            ++ ret;
            right = minval[cand0];
            if (cand0 > 0)
                left = maxval[cand0-1];
            else
                left = -DBL_MAX;
            rng.leftBound() = ibis::util::compactValue(left, right);
        }
    }

    if (rng.rightOperator() == ibis::qExpr::OP_LT) {
        if (cand1 > 0 && rng.rightBound() > minval[cand1-1]) {
            // decrease right bound
            ++ ret;
            right = minval[cand1-1];
            if (cand1 > 1)
                left = maxval[cand1-2];
            else
                left = -DBL_MAX;
            rng.leftBound() = ibis::util::compactValue(left, right);
        }
    }
    else if (rng.rightOperator() == ibis::qExpr::OP_LE) {
        if (cand1 > 0 && rng.rightBound() >= minval[cand1-1]) {
            // decrease right bound
            ++ ret;
            right = minval[cand1-1];
            if (cand1 > 1)
                left = maxval[cand1-2];
            else
                left = -DBL_MAX;
            rng.leftBound() = ibis::util::compactValue(left, right);
        }
    }
    return ret;
} // ibis::bak::contractRange
