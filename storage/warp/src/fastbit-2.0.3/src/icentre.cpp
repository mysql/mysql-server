// $Id$
// Author: John Wu <John.Wu at ACM.org>
// Copyright (c) 2000-2016 the Regents of the University of California
//
// This file contains the implementation of the class called ibis::entre
// -- the multicomponent interval code on bins
//
// entre is French word for "in between"
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
// functions of ibis::entre
//
// construct a bitmap index from current data
ibis::entre::entre(const ibis::column* c, const char* f,
                   const uint32_t nb) : ibis::egale(c, f, nb) {
    if (c == 0) return; // nothing can be done
    try {
        convert();      // convert from equality code to interval code

        if (ibis::gVerbose > 2) {
            ibis::util::logger lg;
            lg() << "entre[" << col->fullname() << "]::ctor -- initialized a "
                 << nbases << "-component interval index with "
                 << nbits << " bitmap" << (nbits>1?"s":"");
            if (ibis::gVerbose > 6) {
                lg() << "\n";
                print(lg());
            }
        }
    }
    catch (...) {
        clear();
        throw;
    }
} // constructor

// a constructor that takes known bounds and bases
ibis::entre::entre(const ibis::column* c, const char* f,
                   const array_t<double>& bd, const array_t<uint32_t> bs)
    : ibis::egale(c, f, bd, bs) {
    try { // convert from multicomponent equality code to interval code
        convert();

        if (ibis::gVerbose > 4) {
            ibis::util::logger lg;
            lg() << "entre[" << col->fullname() << "]::ctor -- constructed a "
                 << nbases << "-component interval index with "
                 << nbits << " bitmap" << (nbits>1?"s":"");
            if (ibis::gVerbose > 6) {
                lg() << "\n";
                print(lg());
            }
        }
    }
    catch (...) {
        clear();
        throw;
    }
} // constructor

ibis::entre::entre(const ibis::bin& rhs, uint32_t nb) : ibis::egale(rhs, nb) {
    try {
        convert();

        if (ibis::gVerbose > 4) {
            ibis::util::logger lg;
            lg() << "entre[" << col->fullname() << "]::ctor -- constructed a "
                 << nbases << "-component interval index with "
                 << nbits << " bitmap" << (nbits>1?"s":"");
            if (ibis::gVerbose > 6) {
                lg() << "\n";
                print(lg());
            }
        }
    }
    catch (...) {
        clear();
        throw;
    }
} // copy from an ibis::bin

/// Reconstruct an index from the content of a storage object.
/// The content of the file (following the 8-byte header) is
///@code
/// nrows  (uint32_t)         -- number of bits in each bitvector
/// nobs   (uint32_t)         -- number of bins
/// nbits  (uint32_t)         -- number of bitvectors
///        padding to ensure bounds starts on multiple of 8.
/// bounds (double[nobs])     -- bind boundaries
/// maxval (double[nobs])     -- the maximum value in each bin
/// minval (double[nobs])     -- the minimum value in each bin
/// offset ([nbits+1])        -- starting position of the bitvectors
/// cnts   (uint32_t[nobs])   -- number of records in each bin
/// nbases (uint32_t)         -- number of components (size of array bases)
/// bases  (uint32_t[nbases]) -- the bases sizes
/// bitvectors                -- the bitvectors one after another
///@endcode
ibis::entre::entre(const ibis::column* c, ibis::fileManager::storage* st,
                   size_t start) : ibis::egale(c, st, start) {
    if (ibis::gVerbose > 2) {
        ibis::util::logger lg;
        lg() << "entre[" << col->fullname()
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

// the argument is the name of the directory or the file name
int ibis::entre::write(const char* dt) const {
    if (nobs == 0) return -1;

    std::string fnm, evt;
    evt = "entre";
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
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " can not overwrite the index file \""
            << fnm << "\" while it is used as a read-only file map";
        return 0;
    }
    else if (fname != 0 && *fname != 0 && 0 == fnm.compare(fname)) {
        activate(); // read everything into memory
        fname = 0; // break the link with the named file
    }
    ibis::fileManager::instance().flushFile(fnm.c_str());

    if (fname != 0 || str != 0)
        activate();

    int fdes = UnixOpen(fnm.c_str(), OPEN_WRITENEW, OPEN_FILEMODE);
    if (fdes < 0) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " failed to open \"" << fnm
            << "\" for write";
        return -2;
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

    const bool useoffset64 = (8+getSerialSize() > 0x80000000UL);
    char header[] = "#IBIS\17\0\0";
    header[5] = (char)ibis::index::ENTRE;
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
} // ibis::entre::write

/// Convert from the multicomponent equality encoding to the multicomponent
/// interval encoding.
///
/// @note For a bases of size 2, only one (the first) bit vector is saved.
void ibis::entre::convert() {
    //activate();
    uint32_t i, offe = 0;
    for (i = 0; nrows == 0 && i < bits.size(); ++ i)
        if (bits[i])
            nrows = bits[i]->size();
    nbases = bases.size();
    LOGGER(ibis::gVerbose > 4)
        << "entre[" << col->fullname()
        << "]::convert -- converting " << nobs << "-bin "
        << nbases << "-component index from equality encoding to "
        "interval encoding (using " << nbits << " bitvectors)";

    // store the current bitvectors in simple
    array_t<bitvector*> simple(nbits, 0);
    bits.swap(simple);
    bits.clear();
    for (i = 0; i < nbases; ++i) {
        if (bases[i] > 2) {
            const uint32_t nb2 = (bases[i] - 1) / 2;
            bits.push_back(new ibis::bitvector);
            if (simple[offe] != 0)
                bits.back()->copy(*(simple[offe]));
            else
                bits.back()->set(0, nrows);
            if (nb2 > 64)
                bits.back()->decompress();
            for (uint32_t j = offe+1; j <= offe+nb2; ++j)
                if (simple[j])
                    *(bits.back()) |= *(simple[j]);
            bits.back()->compress();
            for (uint32_t j = 1; j+nb2 < bases[i]; ++j) {
                if (simple[offe+j-1])
                    bits.push_back(*(bits.back()) - *(simple[offe+j-1]));
                if (simple[offe+j+nb2])
                    *(bits.back()) |= *(simple[offe+j+nb2]);
                bits.back()->compress();
            }
            for (uint32_t j = offe; j < offe+bases[i]; ++j)
                delete simple[j];
        }
        else { // bases[i] <= 2
            // only one basis vector is saved!
            bits.push_back(simple[offe]);
            if (bases[i] > 1)
                delete simple[offe+1];
        }
        offe += bases[i];
    }
    nbits = bits.size();
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
} // convert

/// A simple function to test the speed of the bitvector operations.
void ibis::entre::speedTest(std::ostream& out) const {
    if (nrows == 0) return;
    uint32_t i, nloops = 1000000000 / nrows;
    if (nloops < 2) nloops = 2;
    ibis::horometer timer;
    col->logMessage("entre::speedTest", "testing the speed of operator &");

    activate();
    for (i = 0; i < bits.size()-1; ++i) {
        ibis::bitvector* tmp;
        tmp = *(bits[i+1]) & *(bits[i]);
        delete tmp;

        timer.start();
        for (uint32_t j=0; j<nloops; ++j) {
            tmp = *(bits[i+1]) & *(bits[i]);
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
} // ibis::entre::speedTest

/// The printing function.
void ibis::entre::print(std::ostream& out) const {
    out << col->fullname()
        << ".index(MCBin interval code ncomp=" << bases.size()
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
} // ibis::entre::print

// create index based data in dt -- have to start from data directly
long ibis::entre::append(const char* dt, const char* df, uint32_t nnew) {
    const uint32_t nold = (strcmp(dt, col->partition()->currentDataDir()) == 0 ?
                           col->partition()->nRows()-nnew : nrows);
    std::string ff, ft;
    dataFileName(ff, df);
    dataFileName(ft, dt);
    uint32_t sf = ibis::util::getFileSize(ff.c_str());
    uint32_t st = ibis::util::getFileSize(ft.c_str());
    if (sf >= (st >> 1) || nrows != nold) {
        clear();
        ibis::egale::construct(dt); // the new index on the combined data
        convert();      // convert to interval code
    }
    else { // attempt to make use of the existing index
        // first bin the new data using the same bin boundaries
        ibis::entre idxf(col, df, bounds, bases);
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
    //(void) write(dt);         // write out the new content
    return nnew;
} // ibis::entre::append

// compute the bitvector that is the answer for the query x = b
void ibis::entre::evalEQ(ibis::bitvector& res, uint32_t b) const {
#if DEBUG+0 > 0 || _DEBUG+0 > 0
    LOGGER(ibis::gVerbose >= 0)
        << "DEBUG -- entre::evalEQ(" << b << ")...";
#endif
    if (b >= nobs) {
        res.set(0, nrows);
    }
    else {
        uint32_t nb2;
        uint32_t offset = 0;
        res.set(1, nrows);
        for (uint32_t i = 0; i < bases.size(); ++i) {
            uint32_t k = b % bases[i];
            if (bases[i] > 2) {
                ibis::bitvector *tmp;
                nb2 = (bases[i]-1) / 2;
                if (k+1+nb2 < bases[i]) {
                    const uint32_t j1 = offset + k;
                    const uint32_t j2 = offset + k + 1;
                    activate(j1, j2+1);
                    if (bits[j1]) {
                        if (bits[j2])
                            tmp = *(bits[j1]) - *(bits[j2]);
                        else
                            tmp = new ibis::bitvector(*bits[j1]);
                    }
                    else {
                        tmp = 0;
                    }
                }
                else if (k > nb2) {
                    const uint32_t j1 = offset + k - nb2 - 1;
                    const uint32_t j2 = offset + k - nb2;
                    activate(j1, j2+1);
                    if (bits[j2]) {
                        if (bits[j1])
                            tmp = *(bits[j2]) - *(bits[j1]);
                        else
                            tmp = new ibis::bitvector(*(bits[j2]));
                    }
                    else {
                        tmp = 0;
                    }
                }
                else { // k = nb2
                    const uint32_t j = offset + k;
                    if (bits[offset] == 0)
                        activate(offset);
                    if (bits[j] == 0)
                        activate(j);
                    if (bits[offset] && bits[j])
                        tmp = *(bits[offset]) & *(bits[j]);
                    else
                        tmp = 0;
                }
                if (tmp) {
                    res &= *tmp;
                    delete tmp;
                }
                else if (res.cnt()) { // no need to continue
                    res.set(0, res.size());
                    i = bases.size();
                }
                offset += bases[i] - nb2;
            }
            else { // bases[i] <= 2
                if (bits[offset] == 0)
                    activate(offset);
                if (bits[offset]) {
                    if (0 == k) {
                        res &= *(bits[offset]);
                    }
                    else {
                        res -= *(bits[offset]);
                    }
                }
                else if (0 == k) {
                    res.set(0, res.size());
                }
                ++ offset;
            }
            b /= bases[i];
        }
    }
} // evalEQ

// compute the bitvector that is the answer for the query x <= b
void ibis::entre::evalLE(ibis::bitvector& res, uint32_t b) const {
#if DEBUG+0 > 0 || _DEBUG+0 > 0
    LOGGER(ibis::gVerbose >= 0)
        << "DEBUG -- entre::evalLE(" << b << ")...";
#endif
    if (b+1 >= nobs) { // everything covered
        res.set(1, nrows);
    }
    else {
        uint32_t k, nb2;
        uint32_t i = 0; // index into components
        uint32_t offset = 0;
        // skip till the first component that isn't  the maximum value
        while (i < bases.size() && b % bases[i] == bases[i]-1) {
            offset += (bases[i]>2 ? bases[i]-(bases[i]-1)/2 : 1);
            b /= bases[i];
            ++ i;
        }
        // copy the first non-maximum component
        if (i < bases.size()) {
            k = b % bases[i];
            if (bits[offset] == 0)
                activate(offset);
            if (bits[offset])
                res.copy(*(bits[offset]));
            else
                res.set(0, nrows);
            if (bases[i] > 2) {
                nb2 = (bases[i]-1)/2;
                if (k < nb2) { // [0, nb2] - [k+1, k+1+nb1)
                    const uint32_t j = offset + k + 1;
                    if (bits[j] == 0)
                        activate(j);
                    if (bits[j])
                        res -= *(bits[j]);
                }
                else if (k > nb2) { // [0, nb2] | [k-nb2, k]
                    const uint32_t j = offset + k - nb2;
                    if (bits[j] == 0)
                        activate(j);
                    if (bits[j])
                        res |= *(bits[j]);
                }
                offset += bases[i] - nb2;
            }
            else { // bases[i] <= 2
                if (k != 0)
                    res.set(1, res.size());
                ++ offset;
            }
            b /= bases[i];
        }
        else { // includes everything
            res.set(1, nrows);
        }
        ++ i;
        // deal with the remaining components
        while (i < bases.size()) {
            k = b % bases[i];
            nb2 = (bases[i] - 1) / 2;
            if (bases[i] > 2) {
                if (k < nb2) {
                    // current result & with bit vector representing == k
                    const uint32_t j = offset + k;
                    activate(j, j+2);
                    if (bits[j]) { // [j, j+nb2] - [j+1, j+nb]
                        res &= *(bits[j]);
                        if (bits[j+1])
                            res -= *(bits[j+1]);
                    }
                    else if (res.cnt()) { // [j, j+nb2] is empty
                        res.set(0, res.size());
                    }
                    if (k > 0) { // | with bit vectors representing < k
                        if (bits[offset] == 0)
                            activate(offset);
                        if (bits[offset]) { // | ([0, nb2] - [k, k+nb2])
                            if (bits[j]) {
                                ibis::bitvector *tmp =
                                    *(bits[offset]) - *(bits[j]);
                                res |= *tmp;
                                delete tmp;
                            }
                            else {
                                res |= *(bits[offset]);
                            }
                        }
                    }
                }
                else if (k > nb2) {
                    // res & = k | < k
                    // == res & ([k-nb2, k] - [k-nb2-1, k-1])
                    //        | [k-nb2-1, k-1] | [0, nb2]
                    // == res & [k-nb2, k] | [k-nb2-1, k-1] | [0, nb2]
                    if (k < bases[i]) { // & [k-nb2, k]
                        const uint32_t j = offset + k - nb2;
                        if (bits[j] == 0)
                            activate(j);
                        if (bits[j])
                            res &= *(bits[j]);
                        else if (res.cnt())
                            res.set(0, res.size());
                    }

                    // | [k-nb2-1, k-1]
                    const uint32_t j = offset+k-nb2-1;
                    if (bits[j] == 0)
                        activate(j);
                    if (bits[j])
                        res |= *(bits[j]);
                    if (k > nb2+1) { //  | [0, nb2]
                        if (bits[offset] == 0)
                            activate(offset);
                        if (bits[offset])
                            res |= *(bits[offset]);
                    }
                }
                else { // k = nb2
                    // res & ([0, nb2] & [nb2, nb2+nb2])
                    //     | ([0, nb2] - [nb2, nb2+nb2])
                    // == (res & [0, nb2]) | ([0, nb2] - [nb2, nb2+nb2])
                    if (bits[offset] == 0)
                        activate(offset);
                    if (bits[offset]) // & [0, nb2]
                        res &= *(bits[offset]);
                    else if (res.cnt())
                        res.set(0, res.size());

                    const uint32_t j = offset + k;
                    if (bits[j] == 0)
                        activate(j);
                    if (bits[offset] && bits[j]) {
                        ibis::bitvector *tmp = *(bits[offset]) - *(bits[j]);
                        res |= *tmp;
                        delete tmp;
                    }
                }
                offset += (bases[i] - nb2);
            }
            else { // bases[i] <= 2, only one basis vector is stored
                if (bits[offset] == 0)
                    activate(offset);
                if (bits[offset]) {
                    if (k == 0)
                        res &= *(bits[offset]);
                    else
                        res |= *(bits[offset]);
                }
                else if (k == 0) {
                    res.set(0, res.size());
                }
                ++ offset;
            }
            b /= bases[i];
            ++ i;
        }
    }
} // evalLE

// compute the bitvector that answers the query b0 < x <= b1
void ibis::entre::evalLL(ibis::bitvector& res,
                         uint32_t b0, uint32_t b1) const {
#if DEBUG+0 > 0 || _DEBUG+0 > 0
    LOGGER(ibis::gVerbose >= 0)
        << "DEBUG -- entre::evalLL(" << b0 << ", " << b1 << ")...";
#endif
    if (b0 >= b1) { // no hit
        res.set(0, nrows);
    }
    else if (b1+1 >= nobs) { // x > b0
        evalLE(res, b0);
        res.flip();
    }
    else { // the intended general case
        // res temporarily stores the result of x <= b1
        ibis::bitvector low; // x <= b0
        uint32_t k0, k1, nb2;
        uint32_t i = 0;
        uint32_t offset = 0;
        // skip till the first component that isn't the maximum value
        while (i < bases.size()) {
            k0 = b0 % bases[i];
            k1 = b1 % bases[i];
            if (k0 == bases[i]-1 && k1 == bases[i]-1) {
                offset += (bases[i]>2 ? bases[i] - (bases[i]-1)/2 : 1);
                b0 /= bases[i];
                b1 /= bases[i];
                ++ i;
            }
            else {
                break;
            }
        }
        // the first non-maximum component
        if (i < bases.size()) {
            k0 = b0 % bases[i];
            k1 = b1 % bases[i];
            if (bases[i] > 2) {
                nb2 = (bases[i]-1) / 2;
                if (k0+1 < bases[i]) {
                    if (bits[offset] == 0)
                        activate(offset);
                    if (bits[offset])
                        low.copy(*(bits[offset]));
                    else
                        low.set(0, nrows);

                    if (k0 < nb2) {
                        const uint32_t j = offset + k0 + 1;
                        if (bits[j] == 0)
                            activate(j);
                        if (bits[j])
                            low -= *(bits[j]);
                    }
                    else if (k0 > nb2) {
                        const uint32_t j = offset + k0 - nb2;
                        if (bits[j] == 0)
                            activate(j);
                        if (bits[j])
                            low |= *(bits[j]);
                    }
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                    if (ibis::gVerbose > 30 ||
                        (low.bytes() < (1U << ibis::gVerbose))) {
                        LOGGER(ibis::gVerbose >= 0)
                            << "DEBUG -- entre::evalLL: low "
                            "(component[" << i << "] <= " << k0 << ") "
                            << low;
                    }
#endif
                }
                else {
                    low.set(1, nrows);
                }
                if (k1+1 < bases[i]) {
                    if (bits[offset] == 0)
                        activate(offset);
                    if (bits[offset])
                        res.copy(*(bits[offset]));
                    else
                        res.set(0, nrows);
                    if (k1 < nb2) {
                        const uint32_t j = offset + k1 + 1;
                        if (bits[j] == 0)
                            activate(j);
                        if (bits[j])
                            res -= *(bits[j]);
                    }
                    else if (k1 > nb2) {
                        const uint32_t j = offset + k1 - nb2;
                        if (bits[j] == 0)
                            activate(j);
                        if (bits[j])
                            res |= *(bits[j]);
                    }
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                    if (ibis::gVerbose > 30 ||
                        (res.bytes() < (1U << ibis::gVerbose))) {
                        LOGGER(ibis::gVerbose >= 0)
                            << "DEBUG -- entre::evalLL: high "
                            "(component[" << i << "] <= " << k1 << ") "
                            << res;
                    }
#endif
                }
                else {
                    res.set(1, nrows);
                }
                offset += bases[i] - nb2;
            }
            else { // bases[i] <= 2, only one basis vector is stored
                if (k0 == 0) {
                    if (bits[offset] == 0)
                        activate(offset);
                    if (bits[offset])
                        low = *(bits[offset]);
                    else
                        low.set(0, nrows);
                }
                else {
                    low.set(1, nrows);
                }
                if (k1 == 0) {
                    if (bits[offset] == 0)
                        activate(offset);
                    if (bits[offset])
                        res = *(bits[offset]);
                    else
                        res.set(0, nrows);
                }
                else {
                    res.set(1, nrows);
                }
                ++ offset;
            }
            b0 /= bases[i];
            b1 /= bases[i];
        }
        else {
            res.set(0, nrows);
        }
        ++ i;
        // deal with the remaining components
        while (i < bases.size()) {
            if (b1 > b0) { // low and res has to be separated
                k0 = b0 % bases[i];
                k1 = b1 % bases[i];
                b0 /= bases[i];
                b1 /= bases[i];
                if (bases[i] > 2) {
                    nb2 = (bases[i] - 1) / 2;
                    // update low according to k0
                    if (k0 < nb2) {
                        activate(offset+k0, offset+k0+2);
                        if (bits[offset+k0])
                            low &= *(bits[offset+k0]) ;
                        else if (low.cnt())
                            low.set(0, low.size());
                        if (bits[offset+k0+1])
                            low -= *(bits[offset+k0+1]);

                        if (k0 > 0) {
                            if (bits[offset] == 0)
                                activate(offset);
                            if (bits[offset]) {
                                if (bits[offset+k0]) {
                                    ibis::bitvector *tmp =
                                        *(bits[offset]) - *(bits[offset+k0]);
                                    low |= *tmp;
                                    delete tmp;
                                }
                                else {
                                    low |= *(bits[offset]);
                                }
                            }
                        }
                    }
                    else if (k0 > nb2) {
                        if (k0+1 < bases[i]) {
                            const uint32_t j = offset + k0 - nb2;
                            if (bits[j] == 0)
                                activate(j);
                            if (bits[j])
                                low &= *(bits[offset+k0-nb2]);
                            else if (low.cnt())
                                low.set(0, low.size());
                        }

                        const uint32_t j = offset + k0 - nb2 - 1;
                        if (bits[j] == 0)
                            activate(j);
                        if (bits[j])
                            low |= *(bits[j]);
                        if (k0-nb2-1 > 0) {
                            if (bits[offset] == 0)
                                activate(offset);
                            if (bits[offset])
                                low |= *(bits[offset]);
                        }
                    }
                    else { // k0 = nb2
                        if (bits[offset] == 0)
                            activate(offset);
                        if (bits[offset])
                            low &= *(bits[offset]);
                        else if (low.cnt())
                            low.set(0, low.size());

                        if (bits[offset]) {
                            const uint32_t j = offset + k0;
                            if (bits[j] == 0)
                                activate(j);
                            if (bits[j]) {
                                ibis::bitvector *tmp =
                                    *(bits[offset]) - *(bits[j]);
                                low |= *tmp;
                                delete tmp;
                            }
                            else {
                                low |= *(bits[offset]);
                            }
                        }
                    }
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                    if (ibis::gVerbose > 30 ||
                        (low.bytes() < (1U << ibis::gVerbose))) {
                        LOGGER(ibis::gVerbose >= 0)
                            << "DEBUG -- entre::evalLL: low "
                            "(component[" << i << "] <= " << k0 << ") "
                            << low;
                    }
#endif
                    // update res according to k1
                    if (k1 < nb2) {
                        activate(offset+k1, offset+k1+2);
                        if (bits[offset+k1])
                            res &= *(bits[offset+k1]);
                        else if (res.cnt())
                            res.set(0, res.size());
                        if (bits[offset+k1+1])
                            res -= *(bits[offset+k1+1]);
                        if (k1 > 0) {
                            if (bits[offset] == 0)
                                activate(offset);
                            if (bits[offset]) {
                                if (bits[offset+k1]) {
                                    ibis::bitvector *tmp =
                                        *(bits[offset]) - *(bits[offset+k1]);
                                    res |= *tmp;
                                    delete tmp;
                                }
                                else {
                                    res |= *(bits[offset]);
                                }
                            }
                        }
                    }
                    else if (k1 > nb2) {
                        if (k1+1 < bases[i]) {
                            const uint32_t j = offset+k1-nb2;
                            if (bits[j] == 0)
                                activate(j);
                            if (bits[j])
                                res &= *(bits[j]);
                            else if (res.cnt())
                                res.set(0, res.size());
                        }

                        const uint32_t j = offset+k1-nb2-1;
                        if (bits[j] == 0)
                            activate(j);
                        if (bits[j])
                            res |= *(bits[j]);
                        if (k1-nb2-1 > 0) {
                            if (bits[offset] == 0)
                                activate(offset);
                            if (bits[offset])
                                res |= *(bits[offset]);
                        }
                    }
                    else { // k1 = nb2
                        if (bits[offset] == 0)
                            activate(offset);
                        if (bits[offset]) {
                            const uint32_t j = offset + k1;
                            if (bits[j] == 0)
                                activate(j);
                            if (bits[j]) {
                                res &= *(bits[offset]);
                                ibis::bitvector *tmp =
                                    *(bits[offset]) - *(bits[j]);
                                res |= *tmp;
                                delete tmp;
                            }
                            else { // res & bits[offset] | bits[offset]
                                res.copy(*bits[offset]);
                            }
                        }
                        else if (res.cnt()) {
                            res.set(0, res.size());
                        }
                    }
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                    if (ibis::gVerbose > 30 ||
                        (res.bytes() < (1U << ibis::gVerbose))) {
                        LOGGER(ibis::gVerbose >= 0)
                            << "DEBUG -- entre::evalLL: high "
                            "(component[" << i << "] <= " << k1 << ") "
                            << res;
                    }
#endif
                    offset += (bases[i] - nb2);
                }
                else { // bases[i] <= 2
                    if (bits[offset] == 0)
                        activate(offset);
                    if (bits[offset]) {
                        // update low according to k0
                        if (k0 == 0)
                            low &= *(bits[offset]);
                        else
                            low |= *(bits[offset]);
                        // update res according to k1
                        if (k1 == 0)
                            res &= *(bits[offset]);
                        else
                            res |= *(bits[offset]);
                    }
                    else {
                        if (k0 == 0)
                            low.set(0, low.size());
                        if (k1 == 0)
                            res.set(0, res.size());
                    }
                    ++ offset;
                }
            }
            else { // the more significant components are the same
                res -= low;
                low.clear(); // no longer need low
                while (i < bases.size()) {
                    k1 = b1 % bases[i];
                    if (bases[i] > 2) {
                        nb2 = (bases[i]-1) / 2;
                        if (k1+1+nb2 < bases[i]) {
                            activate(offset+k1, offset+k1+2);
                            if (bits[offset+k1])
                                res &= *(bits[offset+k1]);
                            else
                                res.set(0, res.size());
                            if (bits[offset+k1+1])
                                res -= *(bits[offset+k1+1]);
                        }
                        else if (k1 >= nb2+1) {
                            activate(offset+k1-nb2-1, offset+k1-nb2+1);
                            if (bits[offset+k1-nb2])
                                res &= *(bits[offset+k1-nb2]);
                            else
                                res.set(0, res.size());
                            if (bits[offset+k1-nb2-1])
                                res -= *(bits[offset+k1-nb2-1]);
                        }
                        else { // k1 = nb2
                            if (bits[offset] == 0)
                                activate(offset);
                            if (bits[offset+k1] == 0)
                                activate(offset+k1);
                            if (bits[offset] && bits[offset+k1]) {
                                res &= *(bits[offset]);
                                res &= *(bits[offset+k1]);
                            }
                            else {
                                res.set(0, res.size());
                            }
                        }
                        offset += bases[i] - nb2;
                    }
                    else { // bases[i] <= 2
                        if (bits[offset] == 0)
                            activate(offset);
                        if (0 == k1) {
                            if (bits[offset])
                                res &= *(bits[offset]);
                            else if (res.cnt())
                                res.set(0, res.size());
                        }
                        else if (bits[offset]) {
                            res -= *(bits[offset]);
                        }
                        ++ offset;
                    }
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                    if (ibis::gVerbose > 30 ||
                        (res.bytes() < (1U << ibis::gVerbose))) {
                        LOGGER(ibis::gVerbose >= 0)
                            << "DEBUG -- entre::evalLL: res "
                            "(component[" << i << "] <= " << k1 << ") "
                            << res;
                    }
#endif
                    b1 /= bases[i];
                    ++ i;
                } // while (i < bases.size())
            }
            ++ i;
        }
        if (low.size() == res.size()) { // subtract low from res
            res -= low;
            low.clear();
        }
    }
} // evalLL

long ibis::entre::evaluate(const ibis::qContinuousRange& expr,
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
} // ibis::entre::evaluate

// provide an estimation based on the current index
// set bits in lower are hits for certain, set bits in upper are candidates
// set bits in (upper - lower) should be checked to verifies which are
// actually hits
// if the bitvector upper contain less bits than bitvector lower
// (upper.size() < lower.size()), the content of upper is assumed to be the
// same as lower.
void ibis::entre::estimate(const ibis::qContinuousRange& expr,
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
    else if (hit0 == 0 && hit1 >= bounds.size()) {
        lower.set(1, nrows);
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
    else if (cand0 == 0 && cand1 >= bounds.size()) {
        upper.set(1, nrows);
    }
    else if (cand0+1 == cand1) { // equal to one single value
        evalEQ(upper, cand0);
    }
    else if (cand0 == hit0 && cand1 == hit1+1) {
        evalEQ(upper, hit1);
        upper |= lower;
    }
    else if (cand0+1 == hit0 && cand1 == hit1) {
        evalEQ(upper, cand0);
        upper |= lower;
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
} // ibis::entre::estimate

// compute an upper bound on the number of hits
uint32_t ibis::entre::estimate(const ibis::qContinuousRange& expr) const {
    uint32_t cand0, cand1, cnt;
    if (nobs <= 0) return 0;

    locate(expr, cand0, cand1);
    // accumulate the bits in range [cand0, cand1)
    if (cand1 <= cand0) {
        cnt = 0;
    }
    else if (cand0 == 0 && cand1 >= bounds.size()) {
        cnt = nrows;
    }
    else if (cand0+1 == cand1) { // equal to one single value
        ibis::bitvector upper;
        evalEQ(upper, cand0);
        cnt = upper.cnt();
    }
    else if (cand0 == 0) { // < cand1
        ibis::bitvector upper;
        evalLE(upper, cand1-1);
        cnt = upper.cnt();
    }
    else if (cand1 == nobs) {
        // >= cand0 (translates to NOT (<= cand0-1))
        ibis::bitvector upper;
        evalLE(upper, cand0-1);
        cnt = upper.size() - upper.cnt();
    }
    else { // (cand0-1, cand1-1]
        ibis::bitvector upper;
        evalLL(upper, cand0-1, cand1-1);
        cnt = upper.cnt();
    }
    return cnt;
} // ibis::entre::estimate

double ibis::entre::getSum() const {
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
} // ibis::entre::getSum

double ibis::entre::computeSum() const {
    double sum = 0;
    for (uint32_t i = 0; i < nobs; ++ i) {
        ibis::bitvector tmp;
        evalEQ(tmp, i);
        uint32_t cnt = tmp.cnt();
        if (cnt > 0)
            sum += 0.5 * (minval[i] + maxval[i]) * cnt;
    }
    return sum;
} // ibis::entre::computeSum
