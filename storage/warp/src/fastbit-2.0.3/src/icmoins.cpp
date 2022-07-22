// $Id$
// Author: John Wu <John.Wu at ACM.org>
// Copyright (c) 2000-2016 the Regents of the University of California
//
// This file contains the implementation of the class called ibis::moins
// -- the multicomponent range code on bins
//
// moins is a French word for "less"
//
#if defined(_WIN32) && defined(_MSC_VER)
#pragma warning(disable:4786)   // some identifier longer than 256 characters
#endif
#include "ibin.h"
#include "part.h"
#include "column.h"
#include "resource.h"

#define FASTBIT_SYNC_WRITE 1
////////////////////////////////////////////////////////////////////////
// functions of ibis::moins
//
/// Constructor.  Construct a bitmap index from current data.
ibis::moins::moins(const ibis::column* c, const char* f,
                   const uint32_t nb) : ibis::egale(c, f, nb) {
    if (c == 0) return; // nothing can be done
    try {
        convert();      // convert from equality code to range code
    }
    catch (...) {
        clear();
        throw;
    }

    if (ibis::gVerbose > 2) {
        ibis::util::logger lg;
        lg() << "moins[" << col->fullname() << "]::ctor -- initialized a "
             << nbases << "-component range index with "
             << nbits << " bitmap" << (nbits>1?"s":"");
        if (ibis::gVerbose > 6) {
            lg() << "\n";
            print(lg());
        }
    }
} // constructor

/// Constructor.  It takes known bounds and bases.
ibis::moins::moins(const ibis::column* c, const char* f,
                   const array_t<double>& bd, const array_t<uint32_t> bs)
    : ibis::egale(c, f, bd, bs) {
    try { // convert from multicomponent equality code to range code
        convert();
    }
    catch (...) {
        clear();
        throw;
    }

    if (ibis::gVerbose > 2) {
        ibis::util::logger lg;
        lg() << "moins[" << col->fullname() << "]::ctor -- initialized a "
             << nbases << "-component range index with "
             << nbits << " bitmap" << (nbits>1?"s":"");
        if (ibis::gVerbose > 6) {
            lg() << "\n";
            print(lg());
        }
    }
} // constructor

/// Constructor.  Converts an equality encoded index to multi-component
/// range encoding.
ibis::moins::moins(const ibis::bin& rhs, uint32_t nb) : ibis::egale(rhs, nb) {
    try {
        convert();
    }
    catch (...) {
        clear();
        throw;
    }

    if (ibis::gVerbose > 2) {
        ibis::util::logger lg;
        lg() << "moins[" << col->fullname()
             << "]::ctor -- converted a 1-component index into a "
             << nbases << "-component range index with "
             << nbits << " bitmap" << (nbits>1?"s":"");
        if (ibis::gVerbose > 6) {
            lg() << "\n";
            print(lg());
        }
    }
} // copy from an ibis::bin

/// Constructor.  Reconstruct an index from content of a storage object.
/// The content of the file (following the 8-byte header) is
///@code
/// nrows  (uint32_t)         -- number of bits in a bitvector
/// nobs   (uint32_t)         -- number of bins
/// nbits  (uint32_t)         -- number of bitvectors
/// bounds (double[nobs])     -- bind boundaries
/// maxval (double[nobs])     -- the maximum value in each bin
/// minval (double[nobs])     -- the minimum value in each bin
/// offset ([nbits+1])        -- starting position of the bitvectors
/// cnts   (uint32_t[nobs])   -- number of records in each bin
/// nbases (uint32_t)         -- number of components (size of array bases)
/// bases  (uint32_t[nbases]) -- the bases sizes
/// bitvectors                -- the bitvectors one after another
///@endcode
ibis::moins::moins(const ibis::column* c, ibis::fileManager::storage* st,
                   size_t start) : ibis::egale(c, st, start) {
    if (ibis::gVerbose > 2) {
        ibis::util::logger lg;
        lg() << "moins[" << col->fullname()
             << "]::ctor -- initialized a " << nbases
             << "-component interval index with " << nbits << " bitmap"
             << (nbits>1?"s":"") << " from a storage object @ " << st
             << " starting from position " << start;
        if (ibis::gVerbose > 6) {
            lg() << "\n";
            print(lg());
        }
    }
} // reconstruct data from content of a file

/// Write the index to the specified location.  The argument to this
/// function can be a directory or a file.  The actual index file name is
/// determined by the function indexFileName.
int ibis::moins::write(const char* dt) const {
    if (nobs == 0) return -1;

    std::string fnm, evt;
    evt = "moins";
    if (col != 0 && ibis::gVerbose > 1) {
        evt += '[';
        evt += col->fullname();
        evt += ']';
    }
    evt += "::write";
    indexFileName(fnm, dt);
    if (ibis::gVerbose > 1) {
        evt += '(';
        evt += fnm;
        evt += ')';
    }
    if (fnm.empty()) {
        return 0;
    }
    else if (0 != str && 0 != str->filename() &&
             0 == fnm.compare(str->filename())) {
        return 0;
    }
    else if (fname != 0 && *fname != 0 && 0 == fnm.compare(fname)) {
        return 0;
    }
    ibis::fileManager::instance().flushFile(fnm.c_str());

    if (fname != 0 || str != 0)
        activate();

    int fdes = UnixOpen(fnm.c_str(), OPEN_WRITENEW, OPEN_FILEMODE);
    if (fdes < 0) {
        ibis::fileManager::instance().flushFile(fnm.c_str());
        fdes = UnixOpen(fnm.c_str(), OPEN_WRITENEW, OPEN_FILEMODE);
        if (fdes < 0) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << evt << " failed to open \"" << fnm
                << "\" for write";
            return -2;
        }
    }
    IBIS_BLOCK_GUARD(UnixClose, fdes);
#if defined(_WIN32) && defined(_MSC_VER)
    (void)_setmode(fdes, _O_BINARY);
#endif
#if defined(HAVE_FLOCK)
    ibis::util::flock flck(fdes);
    if (flck.isLocked() == false) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " failed to acquire an exclusive lock "
            "on file " << fnm << " for writing, another thread must be "
            "writing the index now";
        return -6;
    }
#endif

#ifdef FASTBIT_USE_LONG_OFFSETS
    const bool useoffset64 = true;
#else
    const bool useoffset64 = (8+getSerialSize() > 0x80000000UL);
#endif
    char header[] = "#IBIS\16\0\0";
    header[5] = (char)ibis::index::MOINS;
    header[6] = (char) (useoffset64 ? 8 : 4);
    off_t ierr = UnixWrite(fdes, header, 8);
    if (ierr < 8) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt
            << " failed to write the 8-byte header, ierr = " << ierr;
        return -3;
    }
    if (useoffset64)
        ierr = ibis::egale::write64(fdes); // use the function ibis::egale
    else
        ierr = ibis::egale::write32(fdes); // use the function ibis::egale
    if (ierr >= 0) {
#if defined(FASTBIT_SYNC_WRITE)
#if _POSIX_FSYNC+0 > 0
        (void) UnixFlush(fdes); // write to disk
#elif defined(_WIN32) && defined(_MSC_VER)
        (void) _commit(fdes);
#endif
#endif
        LOGGER(ibis::gVerbose > 3)
            << evt << " wrote " << nbits << " bitmap"
            << (nbits>1?"s":"") << " to file " << fnm << " for " << nrows
            << " object" << (nrows>1?"s":"");
    }
    return ierr;
} // ibis::moins::write

/// Convert from the multicomponent equality encoding to the multicomponent
/// range encoding.
void ibis::moins::convert() {
    //activate();
    // count the number of bitvectors to genreate
    uint32_t i;
    nbits = bases[0];
    nbases = bases.size();
    for (i = 0; nrows == 0 && i < bits.size(); ++ i)
        if (bits[i])
            nrows = bits[i]->size();
    for (i = 1; i < nbases; ++i)
        nbits += bases[i];
    nbits -= nbases;
    LOGGER(ibis::gVerbose > 4)
        << "moins[" << col->fullname()
        << "]::convert -- converting " << nobs << "-bin "
        << nbases << "-component index from equality encoding to "
        "interval encoding (using " << nbits << " bitvectors)";

    // store the current bitvectors in simple
    array_t<bitvector*> simple(nbits, 0);
    bits.swap(simple);
    // generate the correct bitmaps
    uint32_t offe = 0, offr = 0;
    for (i = 0; i < nbases; ++i) {
        // copy the first bitvector of the component
        if (simple[offe])
            bits[offr] = simple[offe];
        else {
            bits[offr] = new ibis::bitvector();
            bits[offr]->set(0, nrows);
        }
        ++ offr;
        ++ offe;
        for (uint32_t j = 1; j+2 < bases[i]; ++j) {
            if (simple[offe]) { // add bit vector for the new bin
                bits[offr] = *(bits[offr-1]) | *(simple[offe]);
                delete simple[offe];
            }
            else { // copy the previous bit vector
                bits[offr] = new ibis::bitvector(*(bits[offr-1]));
            }
            ++ offr;
            ++ offe;
        }
        if (bases[i] > 2) {
            delete simple[offe];
            ++ offe;
            bits[offr] = simple[offe];
            if (bits[offr]) bits[offr]->flip();
            ++ offe;
            ++ offr;
        }
        else if (bases[i] > 1) {
            delete simple[offe];
            ++ offe;
        }
    }
    simple.clear();
    for (i = 0; i < nbits; ++i) {
        if (bits[i] != 0) {
            bits[i]->compress();
        }
        else {
            bits[i] = new ibis::bitvector();
            bits[i]->set(0, nrows);
        }
    }

    optionalUnpack(bits, col->indexSpec());
} // ibis::moins::convert

/// A simple function to test the speed of the bitvector operations.
void ibis::moins::speedTest(std::ostream& out) const {
    if (nrows == 0) return;
    uint32_t i, nloops = 1000000000 / nrows;
    if (nloops < 2) nloops = 2;
    ibis::horometer timer;
    col->logMessage("moins::speedTest", "testing the speed of operator -");

    activate();
    for (i = 0; i < bits.size()-1; ++i) {
        ibis::bitvector* tmp;
        tmp = *(bits[i+1]) - *(bits[i]);
        delete tmp;

        timer.start();
        for (uint32_t j=0; j<nloops; ++j) {
            tmp = *(bits[i+1]) - *(bits[i]);
            delete tmp;
        }
        timer.stop();
        {
            ibis::util::ioLock lock;
            out << bits[i]->size() << " "
                << static_cast<double>(bits[i]->bytes() + bits[i+1]->bytes())
                * 4.0 / static_cast<double>(bits[i]->size()) << " "
                << bits[i]->cnt() << " " << bits[i+1]->cnt() << " "
                << timer.realTime() / nloops << std::endl;
        }
    }
} // ibis::moins::speedTest

// the printing function
void ibis::moins::print(std::ostream& out) const {
    out << col->fullname() << ".index(MCBin range code ncomp=" << bases.size()
        << " nbins=" << nobs << ") contains " << bits.size()
        << " bitmaps for " << nrows
        << " objects\nThe base sizes: ";
    for (uint32_t i = 0; i < nbases; ++ i)
        out << bases[i] << ' ';
    out << "\nbitvector information (number of set bits, number of "
        "bytes)\n";
    for (uint32_t i = 0; i < nbits; ++ i) {
        if (bits[i]) {
            out << i << '\t' << bits[i]->cnt() << '\t'
                << bits[i]->bytes() << "\n";
        }
    }
    if (ibis::gVerbose > 7) { // also print the list of distinct values
        out << "bin boundary, [minval, maxval] in bin, number of records\n";
        for (uint32_t i = 0; i < nobs; ++ i) {
            out.precision(12);
            out << bounds[i] << "\t[" << minval[i] << ", " << maxval[i]
                << "]\t" << cnts[i] << "\n";
        }
    }
    out << std::endl;
} // ibis::moins::print

// create index based data in dt -- have to start from data directly
long ibis::moins::append(const char* dt, const char* df, uint32_t nnew) {
    const uint32_t nold = (strcmp(dt, col->partition()->currentDataDir()) == 0 ?
                           col->partition()->nRows()-nnew : nrows);
    std::string ff, ft;
    dataFileName(ff, df);
    dataFileName(ft, dt);
    uint32_t sf = ibis::util::getFileSize(ff.c_str());
    uint32_t st = ibis::util::getFileSize(ft.c_str());
    if (sf >= (st >> 1) || nold != nrows) {
        clear();
        ibis::egale::construct(dt); // the new index on the combined data
        convert();      // convert to range code
    }
    else { // attempt to make use of the existing index
        // first bin the new data using the same bin boundaries
        ibis::moins idxf(col, df, bounds, bases);
        uint32_t tot = 0;
        for (uint32_t i=0; i < nobs; ++i) {
            tot += cnts[i] + idxf.cnts[i];
        }
        uint32_t outside = cnts[0] + idxf.cnts[0] + cnts.back() +
            idxf.cnts.back();
        if (outside > tot / nobs) { // need to rescan the data
            array_t<double> bnds;
            setBoundaries(bnds, idxf, idxf.cnts, cnts);
            clear();            // clear the current content
            binning(dt, bnds);  // generate the new bin boundaries
        }
        else { // don't rescan the data
            ibis::bin::append(idxf); // simply concatenate the bit vectors
            // update min, max and cnts
            for (uint32_t i = 0; i < nobs; ++i) {
                cnts[i] += idxf.cnts[i];
                if (minval[i] > idxf.minval[i])
                    minval[i] = idxf.minval[i];
                if (maxval[i] < idxf.maxval[i])
                    maxval[i] = idxf.maxval[i];
            }
        }
    }
    //write(dt);                // write out the new content
    return nnew;
} // ibis::moins::append

/// compute the bitvector that is the answer for the query x = b
void ibis::moins::evalEQ(ibis::bitvector& res, uint32_t b) const {
    if (b >= nobs) {
        res.set(0, nrows);
    }
    else {
        uint32_t offset = 0;
        res.set(1, nrows);
        for (uint32_t i=0; i < bases.size(); ++i) {
            uint32_t k = b % bases[i];
            if (k+1 < bases[i] || bases[i] == 1) {
                const uint32_t j = offset + k;
                if (bits[j] == 0)
                    activate(j);
                if (bits[j]) {
                    res &= *(bits[j]);
                }
                else {
                    res.set(0, nrows);
                }
            }
            if (k > 0) {
                const uint32_t j = offset + k - 1;
                if (bits[j] == 0)
                    activate(j);
                if (bits[j])
                    res -= *(bits[j]);
            }
            offset += (bases[i] > 1 ? bases[i] - 1 : bases[i]);
            b /= bases[i];
        }
    }
} // evalEQ

/// compute the bitvector that is the answer for the query x <= b
void ibis::moins::evalLE(ibis::bitvector& res, uint32_t b) const {
    if (b+1 >= nobs) {
        res.set(1, nrows);
    }
    else {
        uint32_t i = 0; // index into components
        uint32_t offset = 0;
        // skip till the first component that isn't the maximum value
        while (i < bases.size() && b % bases[i] == bases[i]-1) {
            offset += (bases[i] > 1 ? bases[i] - 1 : bases[i]);
            b /= bases[i];
            ++ i;
        }
        // copy the first non-maximum component
        if (i < bases.size()) {
            const uint32_t j = offset+(b % bases[i]);
            if (bits[j] == 0)
                activate(j);
            if (bits[j]) {
                res.copy(*(bits[offset+(b%bases[i])]));
            }
            else {
                res.set(0, nrows);
                col->logWarning("moins::evalLE",
                                "failed to activate bits[%lu]",
                                static_cast<long unsigned>(j));
            }
            offset += (bases[i] > 1 ? bases[i] - 1 : bases[i]);
            b /= bases[i];
        }
        else {
            res.set(1, nrows);
        }
        ++ i;
        // deal with the remaining components
        while (i < bases.size()) {
            uint32_t k = b % bases[i];
            if (k+1 < bases[i] || bases[i] == 1) {
                const uint32_t j = offset + k;
                if (bits[j] == 0)
                    activate(j);
                if (bits[j])
                    res &= *(bits[j]);
                else
                    res.set(0, res.size());
            }
            if (k > 0) {
                const uint32_t j = offset + k - 1;
                if (bits[j] == 0)
                    activate(j);
                if (bits[j])
                    res |= *(bits[j]);
            }
            offset += (bases[i] > 1 ? bases[i] - 1 : bases[i]);
            b /= bases[i];
            ++ i;
        }
    }
} // evalLE

/// compute the bitvector that answers the query b0 < x <= b1
void ibis::moins::evalLL(ibis::bitvector& res, uint32_t b0, uint32_t b1) const {
    if (b0 >= b1) { // no hit
        res.set(0, nrows);
    }
    else if (b1+1 >= nobs) { // x > b0
        evalLE(res, b0);
        res.flip();
    }
    else { // the general case
        // res temporarily stores the result of x <= b1
        ibis::bitvector low; // x <= b0
        uint32_t k0, k1;
        uint32_t i = 0;
        uint32_t offset = 0;
        // skip till the first component that isn't the maximum value
        while (i < bases.size()) {
            k0 = b0 % bases[i];
            k1 = b1 % bases[i];
            if (k0 == bases[i]-1 && k1 == bases[i]-1) {
                offset += (bases[i] > 1 ? bases[i] - 1 : bases[i]);
                b0 /= bases[i];
                b1 /= bases[i];
                ++ i;
            }
            else {
                break;
            }
        }
        // the first (least-significant) non-maximum component
        if (i < bases.size()) {
            k0 = b0 % bases[i];
            k1 = b1 % bases[i];
            if (k0+1 < bases[i]) {
                const uint32_t j = offset + k0;
                if (bits[j] == 0)
                    activate(j);
                if (bits[j])
                    low = *(bits[j]);
                else
                    low.set(0, nrows);
            }
            else {
                low.set(1, nrows);
            }
            if (k1+1 < bases[i]) {
                const uint32_t j = offset + k1;
                if (bits[j] == 0)
                    activate(j);
                if (bits[j])
                    res = *(bits[j]);
                else
                    res.set(0, nrows);
            }
            else {
                res.set(1, nrows);
            }
            offset += (bases[i] > 1 ? bases[i] - 1 : bases[i]);
            b0 /= bases[i];
            b1 /= bases[i];
        }
        else {
            res.set(0, nrows);
        }
        ++ i;
        // deal with the remaining components
        while (i < bases.size()) {
            if (b1 > b0) { // low and res have to be processed separately
                k0 = b0 % bases[i];
                k1 = b1 % bases[i];
                b0 /= bases[i];
                b1 /= bases[i];
                if (k0+1 < bases[i] || bases[i] == 1) {
                    const uint32_t j = offset + k0;
                    if (bits[j] == 0)
                        activate(j);
                    if (bits[j])
                        low &= *(bits[j]);
                    else
                        low.set(0, low.size());
                }
                if (k1+1 < bases[i] || bases[i] == 1) {
                    const uint32_t j = offset + k1;
                    if (bits[j] == 0)
                        activate(j);
                    if (bits[j])
                        res &= *(bits[j]);
                    else
                        res.set(0, low.size());
                }
                if (k0 > 0) {
                    const uint32_t j = offset + k0 - 1;
                    if (bits[j] == 0)
                        activate(j);
                    if (bits[j])
                        low |= *(bits[j]);
                }
                if (k1 > 0) {
                    const uint32_t j = offset + k1 - 1;
                    if (bits[j] == 0)
                        activate(j);
                    if (bits[j])
                        res |= *(bits[j]);
                }
                offset += (bases[i] > 1 ? bases[i] - 1 : bases[i]);
            }
            else { // the more significant components are the same
                // LOGGER(ibis::gVerbose > 5)
                //     << "res: " << res << "\nlow: " << low;
                // ibis::bitvector tmp(res);
                res -= low;
                // LOGGER(ibis::gVerbose > 5)
                //     << "res-low: " << res;
                // low.flip(); // NOT
                // tmp &= low;
                // low = res;
                // low.flip();
                // tmp &= low;
                // LOGGER(ibis::gVerbose > 5)
                //     << "tmp (expected to all 0s): " << tmp;
                low.clear(); // no longer need low
                while (i < bases.size()) {
                    k1 = b1 % bases[i];
                    if (k1+1 < bases[i] || bases[i] == 1) {
                        const uint32_t j = offset + k1;
                        if (bits[j] == 0)
                            activate(j);
                        if (bits[j])
                            res &= *(bits[j]);
                        else
                            res.set(0, res.size());
                    }
                    if (k1 > 0) {
                        const uint32_t j = offset + k1 - 1;
                        if (bits[j] == 0)
                            activate(j);
                        if (bits[j])
                            res -= *(bits[j]);
                    }
                    offset += (bases[i] > 1 ? bases[i] - 1 : bases[i]);
                    b1 /= bases[i];
                    ++ i;
                }
            }
            ++ i;
        }
        if (low.size() == res.size()) { // subtract low from res
            res -= low;
            low.clear();
        }
    }
} // evalLL

long ibis::moins::evaluate(const ibis::qContinuousRange& expr,
                           ibis::bitvector& lower) const {
    ibis::bitvector tmp;
    estimate(expr, lower, tmp);
    if (tmp.size() == lower.size() && tmp.cnt() > lower.cnt()) {
        if (col == 0 || col->hasRawData() == false) return -1;

        tmp -= lower;
        ibis::bitvector delta;
        col->partition()->doScan(expr, tmp, delta);
        if (delta.size() == lower.size() && delta.cnt() > 0)
            lower |= delta;
    }
    return lower.cnt();
} // ibis::moins::evaluate

// provide an estimation based on the current index
// set bits in lower are hits for certain, set bits in upper are candidates
// set bits in (upper - lower) should be checked to verifies which are
// actually hits
// if the bitvector upper contain less bits than bitvector lower (upper.size()
// < lower.size()), the content
// of upper is assumed to be the same as lower.
void ibis::moins::estimate(const ibis::qContinuousRange& expr,
                           ibis::bitvector& lower,
                           ibis::bitvector& upper) const {
    // values in the range [hit0, hit1) satisfy the query expression
    uint32_t hit0, hit1, cand0, cand1;
    if (nobs <= 0) {
        lower.set(0, nrows);
        upper.clear();
        return;
    }

    locate(expr, cand0, cand1, hit0, hit1);
    // actually accumulate the bits in the range [hit0, hit1)
    if (hit1 <= hit0) {
        lower.set(0, nrows);
    }
    else if (hit0+1 == hit1) { // equal to one single value
        evalEQ(lower, hit0);
    }
    else if (hit0 == 0) { // < hit1
        evalLE(lower, hit1-1);
    }
    else if (hit1 == nobs) { // >= hit0 (translates to NOT (<= hit0-1))
        evalLE(lower, hit0-1);
        lower.flip();
    }
    else { // (hit0-1, hit1-1]
        evalLL(lower, hit0-1, hit1-1);
    }
    // accumulate the bits in range [cand0, cand1)
    if (cand0 == hit0 && cand1 == hit1) {
        upper.clear();
    }
    else if (cand1 <= cand0) {
        upper.set(0, nrows);
    }
    else if (cand0+1 == cand1) { // equal to one single value
        evalEQ(upper, cand0);
    }
    else if (cand0 == 0) { // < cand1
        evalLE(upper, cand1-1);
    }
    else if (cand1 == nobs) {
        // >= cand0 (translates to NOT (<= cand0-1))
        evalLE(upper, cand0-1);
        upper.flip();
    }
    else { // (cand0-1, cand1-1]
        evalLL(upper, cand0-1, cand1-1);
    }
} // ibis::moins::estimate()

// compute an upper bound on the number of hits
uint32_t ibis::moins::estimate(const ibis::qContinuousRange& expr) const {
    if (nobs <= 0) return 0;

    uint32_t cand0, cand1;
    ibis::bitvector upper;
    locate(expr, cand0, cand1);
    // accumulate the bits in range [cand0, cand1)
    if (cand1 <= cand0) {
        upper.set(0, nrows);
    }
    else if (cand0+1 == cand1) { // equal to one single value
        evalEQ(upper, cand0);
    }
    else if (cand0 == 0) { // < cand1
        evalLE(upper, cand1-1);
    }
    else if (cand1 == nobs) {
        // >= cand0 (translates to NOT (<= cand0-1))
        evalLE(upper, cand0-1);
        upper.flip();
    }
    else { // (cand0-1, cand1-1]
        evalLL(upper, cand0-1, cand1-1);
    }
    return upper.cnt();
} // ibis::moins::estimate

double ibis::moins::getSum() const {
    double ret;
    bool here = true;
    { // a small test block to evaluate variable here
        const uint32_t nbv = col->elementSize()*col->partition()->nRows();
        if (str != 0)
            here = (str->bytes() * (nbases+1) < nbv);
        else if (offset64.size() > nbits)
            here = (offset64[nbits] * (nbases+1) < nbv);
        else if (offset32.size() > nbits)
            here = (offset32[nbits] * (nbases+1) < nbv);
    }
    if (here) {
        ret = computeSum();
    }
    else { // indicate sum is not computed
        ibis::util::setNaN(ret);
    }
    return ret;
} // ibis::moins::getSum

double ibis::moins::computeSum() const {
    double sum = 0;
    for (uint32_t i = 0; i < nobs; ++ i) {
        ibis::bitvector tmp;
        evalEQ(tmp, i);
        uint32_t cnt = tmp.cnt();
        if (cnt > 0)
            sum += 0.5 * (minval[i] + maxval[i]) * cnt;
    }
    return sum;
} // ibis::moins::computeSum
