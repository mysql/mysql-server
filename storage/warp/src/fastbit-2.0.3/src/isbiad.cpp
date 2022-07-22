// $Id$
// Author: John Wu <John.Wu at ACM.org>
// Copyright (c) 2000-2016 the Regents of the University of California
//
// This file contains the implementation of the class called ibis::sbiad.
//
// The word sbiad is the Italian translation of the English word fade.
//
// fade  -- multicomponent range-encoded bitmap index
// sbiad -- multicomponent interval-encoded bitmap index
// sapid -- multicomponent equality-encoded bitmap index
//
#if defined(_WIN32) && defined(_MSC_VER)
#pragma warning(disable:4786)   // some identifier longer than 256 characters
#endif
#include "irelic.h"
#include "part.h"
#include "column.h"
#include "resource.h"

#define FASTBIT_SYNC_WRITE 1
////////////////////////////////////////////////////////////////////////
// functions of ibis::sbiad
//
/// Constructor.  If an index exists in the specified location, it will be
/// read, otherwise a new bitmap index will be created from current data.
ibis::sbiad::sbiad(const ibis::column* c, const char* f, const uint32_t nbase)
    : ibis::fade(0) {
    if (c == 0) return;  // nothing can be done
    col = c;
    try {
        if (c->partition()->nRows() < 1000000) {
            construct1(f, nbase); // uses more temporary space
        }
        else {
            construct2(f, nbase); // scan data twice
        }
        if (ibis::gVerbose > 2) {
            ibis::util::logger lg;
            lg() << "sbiad[" << col->partition()->name() << '.' << col->name()
                 << "]::ctor -- constructed a " << bases.size()
                 << "-component interval index with "
                 << bits.size() << " bitmap" << (bits.size()>1?"s":"")
                 << " for " << nrows << " row" << (nrows>1?"s":"");
            if (ibis::gVerbose > 6) {
                lg() << "\n";
                print(lg());
            }
        }
    }
    catch (...) {
        LOGGER(ibis::gVerbose > 1)
            << "Warning -- sbiad[" << col->partition()->name() << '.'
            << col->name() << "]::ctor received an exception, cleaning up ...";
        clear();
        throw;
    }
} // constructor

/// Reconstruct an index from a storage object.
/// The content of the file (following the 8-byte header) is as follows:
///@code
/// nrows(uint32_t)         -- the number of bits in a bit sequence
/// nobs (uint32_t)         -- the number of bit sequences
/// card (uint32_t)         -- the number of distinct values, i.e., cardinality
/// (padding to ensure the next data element is on 8-byte boundary)
/// values (double[card])   -- the distinct values as doubles
/// offset([nobs+1])        -- the starting positions of the bit sequences
///                              (as bit vectors)
/// nbases(uint32_t)        -- the number of components (bases) used
/// cnts (uint32_t[card])   -- the counts for each distinct value
/// bases(uint32_t[nbases]) -- the bases sizes
/// bitvectors              -- the bitvectors one after another
///@endcode
ibis::sbiad::sbiad(const ibis::column* c, ibis::fileManager::storage* st,
                   size_t start) : ibis::fade(c, st, start) {
    if (ibis::gVerbose > 2) {
        ibis::util::logger lg;
        lg() << "sbiad[" << col->partition()->name() << '.' << col->name()
             << "]::ctor -- initialized a " << bases.size()
             << "-component interval index with "
             << bits.size() << " bitmap" << (bits.size()>1?"s":"")
             << " for " << nrows << " row" << (nrows>1?"s":"")
             << " from a storage object @ " << st;
        if (ibis::gVerbose > 6) {
            lg() << "\n";
            print(lg());
        }
    }
} // reconstruct data from content of a file

// the argument is the name of the directory or the file name
int ibis::sbiad::write(const char* dt) const {
    if (vals.empty()) return -1;

    std::string fnm, evt;
    evt = "sbiad";
    if (col != 0 && ibis::gVerbose > 1) {
        evt += '[';
        evt += col->fullname();
        evt += ']';
    }
    evt += "::write";
    if (ibis::gVerbose > 1 && dt != 0) {
        evt += '(';
        evt += dt;
        evt += ')';
    }
    indexFileName(fnm, dt);
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
        activate(); // need all bitvectors

    int fdes = UnixOpen(fnm.c_str(), OPEN_WRITENEW, OPEN_FILEMODE);
    if (fdes < 0) {
        ibis::fileManager::instance().flushFile(fnm.c_str());
        fdes = UnixOpen(fnm.c_str(), OPEN_WRITENEW, OPEN_FILEMODE);
        if (fdes < 0) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << evt << " failed to open \"" << fnm
                << "\" for writing";
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
    const bool useoffset64 = (getSerialSize()+8 > 0x80000000UL);
#endif
    char header[] = "#IBIS\13\0\0";
    header[5] = (char)ibis::index::SBIAD;
    header[6] = (char)(useoffset64 ? 8 : 4);
    off_t ierr = UnixWrite(fdes, header, 8);
    if (ierr < 8) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt
            << " failed to write the 8-byte header, ierr = " << ierr;
        return -3;
    }
    if (useoffset64)
        ierr = ibis::fade::write64(fdes);
    else
        ierr = ibis::fade::write32(fdes);

    if (ierr >= 0) {
#if defined(FASTBIT_SYNC_WRITE)
#if _POSIX_FSYNC+0 > 0
        (void) UnixFlush(fdes); // write to disk
#elif defined(_WIN32) && defined(_MSC_VER)
        (void) _commit(fdes);
#endif
#endif
        LOGGER(ierr >= 0 && ibis::gVerbose > 5)
            << evt << " wrote " << bits.size() << " bitmap"
            << (bits.size()>1?"s":"") << " to " << fnm;
    }
    return ierr;
} // ibis::sbiad::write

/// This version of the constructor take one pass throught the data.  It
/// constructs a ibis::index::VMap first, then construct the sbiad from the
/// VMap.  It uses more computer memory than the two-pass version, but will
/// probably run a little faster.
void ibis::sbiad::construct1(const char* f, const uint32_t nbase) {
    VMap bmap; // a map between values and their position
    try {
        mapValues(f, bmap);
    }
    catch (...) { // need to clean up bmap
        LOGGER(ibis::gVerbose >= 0)
            << "sbiad::construct reclaiming storage "
            "allocated to bitvectors (" << bmap.size() << ")";

        for (VMap::iterator it = bmap.begin(); it != bmap.end(); ++ it)
            delete (*it).second;
        bmap.clear();
        ibis::fileManager::instance().signalMemoryAvailable();
        throw;
    }
    if (bmap.empty()) return;
    nrows = (*(bmap.begin())).second->size();
    if (nrows != col->partition()->nRows()) {
        for (VMap::iterator it = bmap.begin(); it != bmap.end(); ++ it)
            delete (*it).second;
        bmap.clear();
        ibis::fileManager::instance().signalMemoryAvailable();

        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- sbiad::construct1 the bitvectors "
            "do not have the expected size(" << col->partition()->nRows()
            << "). stopping..";
        throw ibis::bad_alloc("sbiad::construct1 failed due to incorrect "
                              "bitvector sizes" IBIS_FILE_LINE);
    }

    // convert bmap into the current data structure
    // fill the arrays vals and cnts
    const uint32_t card = bmap.size();
    vals.reserve(card);
    cnts.reserve(card);
    for (VMap::const_iterator it = bmap.begin(); it != bmap.end(); ++it) {
        vals.push_back((*it).first);
        cnts.push_back((*it).second->cnt());
    }
    // fill the array bases
    setBases(bases, card, nbase);
    // count the number of bitvectors to genreate
    const uint32_t nb = bases.size();
    uint32_t nobs = 0;
    uint32_t i;
    for (i = 0; i < nb; ++i)
        nobs += bases[i];
    // allocate enough bitvectors in bits
    bits.resize(nobs);
    for (i = 0; i < nobs; ++i)
        bits[i] = 0;
    if (ibis::gVerbose > 5) {
        col->logMessage("sbiad::construct", "initialized the array of "
                        "bitvectors, start converting %lu bitmaps into %lu-"
                        "component range code (with %lu bitvectors)",
                        static_cast<long unsigned>(vals.size()),
                        static_cast<long unsigned>(nb),
                        static_cast<long unsigned>(nobs));
    }

    // converting to multi-level equality encoding first
    i = 0;
    for (VMap::const_iterator it = bmap.begin(); it != bmap.end();
         ++it, ++i) {
        uint32_t offset = 0;
        uint32_t ii = i;
        for (uint32_t j = 0; j < nb; ++j) {
            uint32_t k = ii % bases[j];
            if (bits[offset+k]) {
                *(bits[offset+k]) |= *((*it).second);
            }
            else {
                bits[offset+k] = new ibis::bitvector();
                bits[offset+k]->copy(*((*it).second));
                // expected to be operated on more than 64 times
                if (vals.size() > 64*bases[j])
                    bits[offset+k]->decompress();
            }
            ii /= bases[j];
            offset += bases[j];
        }

        delete (*it).second; // no longer need the bitmap
    }
    for (i = 0; i < nobs; ++i) {
        if (bits[i] == 0) {
            bits[i] = new ibis::bitvector();
            bits[i]->set(0, nrows);
        }
    }
#if DEBUG+0 > 0 || _DEBUG+0 > 0
    if (ibis::gVerbose > 11) {
        LOGGER(ibis::gVerbose >= 0)
            << "DEBUG -- sbiad::construct1 converted"
            << bmap.size() << " bitmaps for each distinct value into "
            << bits.size() << bases.size()
            << "-component equality encoded bitmaps";
    }
#endif
    // sum up the bitvectors according to the interval-encoding
    array_t<bitvector*> beq;
    beq.swap(bits);
    try { // use a try block to ensure the bitvectors in beq are freed
        uint32_t ke = 0;
        bits.clear();
        for (i = 0; i < nb; ++i) {
            if (bases[i] > 2) {
                nobs = (bases[i] - 1) / 2;
                bits.push_back(new ibis::bitvector);
                bits.back()->copy(*(beq[ke]));
                if (nobs > 64)
                    bits.back()->decompress();
                for (uint32_t j = ke+1; j <= ke+nobs; ++j)
                    *(bits.back()) |= *(beq[j]);
                bits.back()->compress();
                for (uint32_t j = 1; j < bases[i]-nobs; ++j) {
                    bits.push_back(*(bits.back()) - *(beq[ke+j-1]));
                    *(bits.back()) |= *(beq[ke+j+nobs]);
                    bits.back()->compress();
                }
                for (uint32_t j = ke; j < ke+bases[i]; ++j) {
                    delete beq[j];
                    beq[j] = 0;
                }
            }
            else {
                bits.push_back(beq[ke]);
                if (bases[i] > 1) {
                    delete beq[ke+1];
                    beq[ke+1] = 0;
                }
            }
            ke += bases[i];
        }
    }
    catch (...) {
        LOGGER(ibis::gVerbose > 1)
            << "Warning -- column::[" << col->name()
            << "]::construct1 encountered an exception while converting "
            "to inverval encoding, cleaning up ...";
        for (uint32_t i = 0; i < beq.size(); ++ i)
            delete beq[i];
        throw;
    }
    beq.clear();
#if DEBUG+0 > 0 || _DEBUG+0 > 0
    if (ibis::gVerbose > 11) {
        LOGGER(ibis::gVerbose >= 0)
            << "DEBUG -- sbiad::construct1 completed "
            << "converting equality encoding to interval encoding";
    }
#endif
    optionalUnpack(bits, col->indexSpec());

    // write out the current content
    if (ibis::gVerbose > 8) {
        ibis::util::logger lg;
        print(lg());
    }
} // ibis::sbiad::construct1

/// Assign bit values for a given key value.  Assume that the array @c vals
/// is initialized properly, this function converts the value @cval into a
/// set of bits to be stored in the bit vectors contained in @c bits.
///
/// @note to be used by @c construct2 to build a new ibis::sbiad index.
void ibis::sbiad::setBit(const uint32_t i, const double val) {
    if (val > vals.back()) return;
    if (val < vals[0]) return;

    // perform a binary search to locate position of val in vals
    uint32_t ii = 0, jj = vals.size() - 1;
    uint32_t kk = (ii + jj) / 2;
    while (kk > ii) {
        if (vals[kk] < val) {
            ii = kk;
            kk = (kk + jj) / 2;
        }
        else if (vals[kk] > val) {
            jj = kk;
            kk = (ii + kk) / 2;
        }
        else {
            ii = kk;
            jj = kk;
        }
    }

    if (vals[jj] == val) { // vals[jj] is the same as val
        kk = jj;
    }
    else if (vals[ii] == val) { // vals[ii] is the same as val
        kk = ii;
    }
    else { // doesn't match a know value -- shouldn't be
        return;
    }

    // now we know what bitvectors to modify
    const uint32_t nb = bases.size();
    uint32_t offset = 0; // offset into bits
    for (ii = 0; ii < nb; ++ ii) {
        jj = kk % bases[ii];
        bits[offset+jj]->setBit(i, 1);
        kk /= bases[ii];
        offset += bases[ii];
    }
} // ibis::sbiad::setBit

/// Generate a new sbiad index by passing through the data twice.
/// - (1) scan the data to generate a list of distinct values and their count.
/// - (2) scan the data a second time to produce the bit vectors.
void ibis::sbiad::construct2(const char* f, const uint32_t nbase) {
    { // a block to limit the scope of hst
        histogram hst;
        mapValues(f, hst); // scan the data to produce the histogram
        if (hst.empty())   // no data, of course no index
            return;

        // convert histogram into two arrays
        const uint32_t nhst = hst.size();
        vals.resize(nhst);
        cnts.resize(nhst);
        histogram::const_iterator it = hst.begin();
        for (uint32_t i = 0; i < nhst; ++i) {
            vals[i] = (*it).first;
            cnts[i] = (*it).second;
            ++ it;
        }
    }

    // determine the base sizes
    setBases(bases, vals.size(), nbase);
    const uint32_t nb = bases.size();
    int ierr;

    // allocate the correct number of bitvectors
    uint32_t nobs = 0;
    for (uint32_t ii = 0; ii < nb; ++ii)
        nobs += bases[ii];
    bits.resize(nobs);
    for (uint32_t ii = 0; ii < nobs; ++ii)
        bits[ii] = new ibis::bitvector;

    std::string fnm; // name of the data file
    dataFileName(fnm, f);

    nrows = col->partition()->nRows();
    ibis::bitvector mask;
    {   // name of mask file associated with the data file
        array_t<ibis::bitvector::word_t> arr;
        std::string mname(fnm);
        mname += ".msk";
        if (ibis::fileManager::instance().getFile(mname.c_str(), arr) == 0)
            mask.copy(ibis::bitvector(arr)); // convert arr to a bitvector
        else
            mask.set(1, nrows); // default mask
    }

    // need to do different things for different columns
    switch (col->type()) {
    case ibis::TEXT:
    case ibis::UINT: {// unsigned int
        array_t<uint32_t> val;
        if (! fnm.empty())
            ierr = ibis::fileManager::instance().getFile(fnm.c_str(), val);
        else
            ierr = col->getValuesArray(&val);
        if (ierr < 0 || val.empty()) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- sbiad::construct2 failed to retrieve any value";
            break;
        }

        if (val.size() > mask.size()) {
            col->logWarning("sbiad::construct", "the data file \"%s\" "
                            "contains more elements (%lu) then expected "
                            "(%lu)", fnm.c_str(),
                            static_cast<long unsigned>(val.size()),
                            static_cast<long unsigned>(mask.size()));
            mask.adjustSize(nrows, nrows);
        }
        ibis::bitvector::indexSet iset = mask.firstIndexSet();
        uint32_t nind = iset.nIndices();
        const ibis::bitvector::word_t *iix = iset.indices();
        while (nind) {
            if (iset.isRange()) { // a range
                uint32_t k = (iix[1] < nrows ? iix[1] : nrows);
                for (uint32_t i = *iix; i < k; ++i)
                    setBit(i, val[i]);
            }
            else if (*iix+ibis::bitvector::bitsPerLiteral() < nrows) {
                // a list of indices
                for (uint32_t i = 0; i < nind; ++i) {
                    uint32_t k = iix[i];
                    setBit(k, val[k]);
                }
            }
            else {
                for (uint32_t i = 0; i < nind; ++i) {
                    uint32_t k = iix[i];
                    if (k < nrows)
                        setBit(k, val[k]);
                }
            }
            ++iset;
            nind = iset.nIndices();
            if (*iix >= nrows)
                nind = 0;
        } // while (nind)
        break;}
    case ibis::INT: {// signed int
        array_t<int32_t> val;
        if (! fnm.empty())
            ierr = ibis::fileManager::instance().getFile(fnm.c_str(), val);
        else
            ierr = col->getValuesArray(&val);
        if (ierr < 0 || val.empty()) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- sbiad::construct2 failed to retrieve any value";
            break;
        }

        if (val.size() > mask.size()) {
            col->logWarning("sbiad::construct", "the data file \"%s\" "
                            "contains more elements (%lu) then expected "
                            "(%lu)", fnm.c_str(),
                            static_cast<long unsigned>(val.size()),
                            static_cast<long unsigned>(mask.size()));
            mask.adjustSize(nrows, nrows);
        }
        ibis::bitvector::indexSet iset = mask.firstIndexSet();
        uint32_t nind = iset.nIndices();
        const ibis::bitvector::word_t *iix = iset.indices();
        while (nind) {
            if (iset.isRange()) { // a range
                uint32_t k = (iix[1] < nrows ? iix[1] : nrows);
                for (uint32_t i = *iix; i < k; ++i)
                    setBit(i, val[i]);
            }
            else if (*iix+ibis::bitvector::bitsPerLiteral() < nrows) {
                // a list of indices
                for (uint32_t i = 0; i < nind; ++i) {
                    uint32_t k = iix[i];
                    setBit(k, val[k]);
                }
            }
            else {
                for (uint32_t i = 0; i < nind; ++i) {
                    uint32_t k = iix[i];
                    if (k < nrows)
                        setBit(k, val[k]);
                }
            }
            ++iset;
            nind = iset.nIndices();
            if (*iix >= nrows)
                nind = 0;
        } // while (nind)
        break;}
    case ibis::ULONG: {// unsigned long int
        array_t<uint64_t> val;
        if (! fnm.empty())
            ierr = ibis::fileManager::instance().getFile(fnm.c_str(), val);
        else
            ierr = col->getValuesArray(&val);
        if (ierr < 0 || val.empty()) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- sbiad::construct2 failed to retrieve any value";
            break;
        }

        if (val.size() > mask.size()) {
            col->logWarning("sbiad::construct", "the data file \"%s\" "
                            "contains more elements (%lu) then expected "
                            "(%lu)", fnm.c_str(),
                            static_cast<long unsigned>(val.size()),
                            static_cast<long unsigned>(mask.size()));
            mask.adjustSize(nrows, nrows);
        }
        ibis::bitvector::indexSet iset = mask.firstIndexSet();
        uint32_t nind = iset.nIndices();
        const ibis::bitvector::word_t *iix = iset.indices();
        while (nind) {
            if (iset.isRange()) { // a range
                uint32_t k = (iix[1] < nrows ? iix[1] : nrows);
                for (uint32_t i = *iix; i < k; ++i)
                    setBit(i, val[i]);
            }
            else if (*iix+ibis::bitvector::bitsPerLiteral() < nrows) {
                // a list of indices
                for (uint32_t i = 0; i < nind; ++i) {
                    uint32_t k = iix[i];
                    setBit(k, val[k]);
                }
            }
            else {
                for (uint32_t i = 0; i < nind; ++i) {
                    uint32_t k = iix[i];
                    if (k < nrows)
                        setBit(k, val[k]);
                }
            }
            ++iset;
            nind = iset.nIndices();
            if (*iix >= nrows)
                nind = 0;
        } // while (nind)
        break;}
    case ibis::LONG: {// signed long int
        array_t<int64_t> val;
        if (! fnm.empty())
            ierr = ibis::fileManager::instance().getFile(fnm.c_str(), val);
        else
            ierr = col->getValuesArray(&val);
        if (ierr < 0 || val.empty()) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- sbiad::construct2 failed to retrieve any value";
            break;
        }

        if (val.size() > mask.size()) {
            col->logWarning("sbiad::construct", "the data file \"%s\" "
                            "contains more elements (%lu) then expected "
                            "(%lu)", fnm.c_str(),
                            static_cast<long unsigned>(val.size()),
                            static_cast<long unsigned>(mask.size()));
            mask.adjustSize(nrows, nrows);
        }
        ibis::bitvector::indexSet iset = mask.firstIndexSet();
        uint32_t nind = iset.nIndices();
        const ibis::bitvector::word_t *iix = iset.indices();
        while (nind) {
            if (iset.isRange()) { // a range
                uint32_t k = (iix[1] < nrows ? iix[1] : nrows);
                for (uint32_t i = *iix; i < k; ++i)
                    setBit(i, val[i]);
            }
            else if (*iix+ibis::bitvector::bitsPerLiteral() < nrows) {
                // a list of indices
                for (uint32_t i = 0; i < nind; ++i) {
                    uint32_t k = iix[i];
                    setBit(k, val[k]);
                }
            }
            else {
                for (uint32_t i = 0; i < nind; ++i) {
                    uint32_t k = iix[i];
                    if (k < nrows)
                        setBit(k, val[k]);
                }
            }
            ++iset;
            nind = iset.nIndices();
            if (*iix >= nrows)
                nind = 0;
        } // while (nind)
        break;}
    case ibis::USHORT: {// unsigned short int
        array_t<uint16_t> val;
        if (! fnm.empty())
            ierr = ibis::fileManager::instance().getFile(fnm.c_str(), val);
        else
            ierr = col->getValuesArray(&val);
        if (ierr < 0 || val.empty()) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- sbiad::construct2 failed to retrieve any value";
            break;
        }

        if (val.size() > mask.size()) {
            col->logWarning("sbiad::construct", "the data file \"%s\" "
                            "contains more elements (%lu) then expected "
                            "(%lu)", fnm.c_str(),
                            static_cast<long unsigned>(val.size()),
                            static_cast<long unsigned>(mask.size()));
            mask.adjustSize(nrows, nrows);
        }
        ibis::bitvector::indexSet iset = mask.firstIndexSet();
        uint32_t nind = iset.nIndices();
        const ibis::bitvector::word_t *iix = iset.indices();
        while (nind) {
            if (iset.isRange()) { // a range
                uint32_t k = (iix[1] < nrows ? iix[1] : nrows);
                for (uint32_t i = *iix; i < k; ++i)
                    setBit(i, val[i]);
            }
            else if (*iix+ibis::bitvector::bitsPerLiteral() < nrows) {
                // a list of indices
                for (uint32_t i = 0; i < nind; ++i) {
                    uint32_t k = iix[i];
                    setBit(k, val[k]);
                }
            }
            else {
                for (uint32_t i = 0; i < nind; ++i) {
                    uint32_t k = iix[i];
                    if (k < nrows)
                        setBit(k, val[k]);
                }
            }
            ++iset;
            nind = iset.nIndices();
            if (*iix >= nrows)
                nind = 0;
        } // while (nind)
        break;}
    case ibis::SHORT: {// signed short int
        array_t<int16_t> val;
        if (! fnm.empty())
            ierr = ibis::fileManager::instance().getFile(fnm.c_str(), val);
        else
            ierr = col->getValuesArray(&val);
        if (ierr < 0 || val.empty()) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- sbiad::construct2 failed to retrieve any value";
            break;
        }

        if (val.size() > mask.size()) {
            col->logWarning("sbiad::construct", "the data file \"%s\" "
                            "contains more elements (%lu) then expected "
                            "(%lu)", fnm.c_str(),
                            static_cast<long unsigned>(val.size()),
                            static_cast<long unsigned>(mask.size()));
            mask.adjustSize(nrows, nrows);
        }
        ibis::bitvector::indexSet iset = mask.firstIndexSet();
        uint32_t nind = iset.nIndices();
        const ibis::bitvector::word_t *iix = iset.indices();
        while (nind) {
            if (iset.isRange()) { // a range
                uint32_t k = (iix[1] < nrows ? iix[1] : nrows);
                for (uint32_t i = *iix; i < k; ++i)
                    setBit(i, val[i]);
            }
            else if (*iix+ibis::bitvector::bitsPerLiteral() < nrows) {
                // a list of indices
                for (uint32_t i = 0; i < nind; ++i) {
                    uint32_t k = iix[i];
                    setBit(k, val[k]);
                }
            }
            else {
                for (uint32_t i = 0; i < nind; ++i) {
                    uint32_t k = iix[i];
                    if (k < nrows)
                        setBit(k, val[k]);
                }
            }
            ++iset;
            nind = iset.nIndices();
            if (*iix >= nrows)
                nind = 0;
        } // while (nind)
        break;}
    case ibis::UBYTE: {// unsigned char
        array_t<unsigned char> val;
        if (! fnm.empty())
            ierr = ibis::fileManager::instance().getFile(fnm.c_str(), val);
        else
            ierr = col->getValuesArray(&val);
        if (ierr < 0 || val.empty()) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- sbiad::construct2 failed to retrieve any value";
            break;
        }

        if (val.size() > mask.size()) {
            col->logWarning("sbiad::construct", "the data file \"%s\" "
                            "contains more elements (%lu) then expected "
                            "(%lu)", fnm.c_str(),
                            static_cast<long unsigned>(val.size()),
                            static_cast<long unsigned>(mask.size()));
            mask.adjustSize(nrows, nrows);
        }
        ibis::bitvector::indexSet iset = mask.firstIndexSet();
        uint32_t nind = iset.nIndices();
        const ibis::bitvector::word_t *iix = iset.indices();
        while (nind) {
            if (iset.isRange()) { // a range
                uint32_t k = (iix[1] < nrows ? iix[1] : nrows);
                for (uint32_t i = *iix; i < k; ++i)
                    setBit(i, val[i]);
            }
            else if (*iix+ibis::bitvector::bitsPerLiteral() < nrows) {
                // a list of indices
                for (uint32_t i = 0; i < nind; ++i) {
                    uint32_t k = iix[i];
                    setBit(k, val[k]);
                }
            }
            else {
                for (uint32_t i = 0; i < nind; ++i) {
                    uint32_t k = iix[i];
                    if (k < nrows)
                        setBit(k, val[k]);
                }
            }
            ++iset;
            nind = iset.nIndices();
            if (*iix >= nrows)
                nind = 0;
        } // while (nind)
        break;}
    case ibis::BYTE: {// signed char
        array_t<signed char> val;
        if (! fnm.empty())
            ierr = ibis::fileManager::instance().getFile(fnm.c_str(), val);
        else
            ierr = col->getValuesArray(&val);
        if (ierr < 0 || val.empty()) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- sbiad::construct2 failed to retrieve any value";
            break;
        }

        if (val.size() > mask.size()) {
            col->logWarning("sbiad::construct", "the data file \"%s\" "
                            "contains more elements (%lu) then expected "
                            "(%lu)", fnm.c_str(),
                            static_cast<long unsigned>(val.size()),
                            static_cast<long unsigned>(mask.size()));
            mask.adjustSize(nrows, nrows);
        }
        ibis::bitvector::indexSet iset = mask.firstIndexSet();
        uint32_t nind = iset.nIndices();
        const ibis::bitvector::word_t *iix = iset.indices();
        while (nind) {
            if (iset.isRange()) { // a range
                uint32_t k = (iix[1] < nrows ? iix[1] : nrows);
                for (uint32_t i = *iix; i < k; ++i)
                    setBit(i, val[i]);
            }
            else if (*iix+ibis::bitvector::bitsPerLiteral() < nrows) {
                // a list of indices
                for (uint32_t i = 0; i < nind; ++i) {
                    uint32_t k = iix[i];
                    setBit(k, val[k]);
                }
            }
            else {
                for (uint32_t i = 0; i < nind; ++i) {
                    uint32_t k = iix[i];
                    if (k < nrows)
                        setBit(k, val[k]);
                }
            }
            ++iset;
            nind = iset.nIndices();
            if (*iix >= nrows)
                nind = 0;
        } // while (nind)
        break;}
    case ibis::FLOAT: {// (4-byte) floating-point values
        array_t<float> val;
        if (! fnm.empty())
            ierr = ibis::fileManager::instance().getFile(fnm.c_str(), val);
        else
            ierr = col->getValuesArray(&val);
        if (ierr < 0 || val.empty()) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- sbiad::construct2 failed to retrieve any value";
            break;
        }

        if (val.size() > mask.size()) {
            col->logWarning("sbiad::construct", "the data file \"%s\" "
                            "contains more elements (%lu) then expected "
                            "(%lu)", fnm.c_str(),
                            static_cast<long unsigned>(val.size()),
                            static_cast<long unsigned>(mask.size()));
            mask.adjustSize(nrows, nrows);
        }
        ibis::bitvector::indexSet iset = mask.firstIndexSet();
        uint32_t nind = iset.nIndices();
        const ibis::bitvector::word_t *iix = iset.indices();
        while (nind) {
            if (iset.isRange()) { // a range
                uint32_t k = (iix[1] < nrows ? iix[1] : nrows);
                for (uint32_t i = *iix; i < k; ++i)
                    setBit(i, val[i]);
            }
            else if (*iix+ibis::bitvector::bitsPerLiteral() < nrows) {
                // a list of indices
                for (uint32_t i = 0; i < nind; ++i) {
                    uint32_t k = iix[i];
                    setBit(k, val[k]);
                }
            }
            else {
                for (uint32_t i = 0; i < nind; ++i) {
                    uint32_t k = iix[i];
                    if (k < nrows)
                        setBit(k, val[k]);
                }
            }
            ++iset;
            nind = iset.nIndices();
            if (*iix >= nrows)
                nind = 0;
        } // while (nind)
        break;}
    case ibis::DOUBLE: {// (8-byte) floating-point values
        array_t<double> val;
        if (! fnm.empty())
            ierr = ibis::fileManager::instance().getFile(fnm.c_str(), val);
        else
            ierr = col->getValuesArray(&val);
        if (ierr < 0 || val.empty()) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- sbiad::construct2 failed to retrieve any value";
            break;
        }

        if (val.size() > mask.size()) {
            col->logWarning("sbiad::construct", "the data file \"%s\" "
                            "contains more elements (%lu) then expected "
                            "(%lu)", fnm.c_str(),
                            static_cast<long unsigned>(val.size()),
                            static_cast<long unsigned>(mask.size()));
            mask.adjustSize(nrows, nrows);
        }
        ibis::bitvector::indexSet iset = mask.firstIndexSet();
        uint32_t nind = iset.nIndices();
        const ibis::bitvector::word_t *iix = iset.indices();
        while (nind) {
            if (iset.isRange()) { // a range
                uint32_t k = (iix[1] < nrows ? iix[1] : nrows);
                for (uint32_t i = *iix; i < k; ++i)
                    setBit(i, val[i]);
            }
            else if (*iix+ibis::bitvector::bitsPerLiteral() < nrows) {
                // a list of indices
                for (uint32_t i = 0; i < nind; ++i) {
                    uint32_t k = iix[i];
                    setBit(k, val[k]);
                }
            }
            else {
                for (uint32_t i = 0; i < nind; ++i) {
                    uint32_t k = iix[i];
                    if (k < nrows)
                        setBit(k, val[k]);
                }
            }
            ++iset;
            nind = iset.nIndices();
            if (*iix >= nrows)
                nind = 0;
        } // while (nind)
        break;}
    case ibis::CATEGORY: // no need for a separate index
        col->logWarning("sbiad::ctor", "no need for another index");
        return;
    default:
        col->logWarning("sbiad::ctor", "failed to create bit sbiad index "
                        "for column type %s",
                        ibis::TYPESTRING[(int)col->type()]);
        return;
    }

    // make sure all bit vectors are the same size
    for (uint32_t i = 0; i < nobs; ++i) {
        bits[i]->adjustSize(0, nrows);
    }
    // sum up the bitvectors according to interval-encoding
    array_t<bitvector*> beq;
    beq.swap(bits);
    try {    
        uint32_t ke = 0;
        bits.clear();
        for (uint32_t i = 0; i < nb; ++i) {
            if (bases[i] > 2) {
                nobs = (bases[i] - 1) / 2;
                bits.push_back(new ibis::bitvector);
                bits.back()->copy(*(beq[ke]));
                if (nobs > 64)
                    bits.back()->decompress();
                for (uint32_t j = ke+1; j <= ke+nobs; ++j)
                    *(bits.back()) |= *(beq[j]);
                bits.back()->compress();
                for (uint32_t j = 1; j < bases[i]-nobs; ++j) {
                    bits.push_back(*(bits.back()) - *(beq[ke+j-1]));
                    *(bits.back()) |= *(beq[ke+j+nobs]);
                    bits.back()->compress();
                }
                for (uint32_t j = ke; j < ke+bases[i]; ++j) {
                    delete beq[j];
                    beq[j] = 0;
                }
            }
            else {
                bits.push_back(beq[ke]);
                if (bases[i] > 1) {
                    delete beq[ke+1];
                    beq[ke+1] = 0;
                }
            }
            ke += bases[i];
        }
    }
    catch (...) {
        LOGGER(ibis::gVerbose > 1)
            << "Warning -- column::[" << col->name()
            << "]::construct2 encountered an exception while converting "
            "to inverval encoding, cleaning up ...";
        for (uint32_t i = 0; i < beq.size(); ++ i)
            delete beq[i];
        throw;
    }
    beq.clear();
    optionalUnpack(bits, col->indexSpec());

    // write out the current content
    if (ibis::gVerbose > 8) {
        ibis::util::logger lg;
        print(lg());
    }
} // ibis::sbiad::construct2

// a simple function to test the speed of the bitvector operations
void ibis::sbiad::speedTest(std::ostream& out) const {
    if (nrows == 0) return;
    uint32_t i, nloops = 1000000000 / nrows;
    if (nloops < 2) nloops = 2;
    ibis::horometer timer;
    col->logMessage("sbiad::speedTest", "testing the speed of operator -");

    activate(); // need all bitvectors
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
                << timer.realTime() / nloops << "\n";
        }
    }
} // ibis::sbiad::speedTest

// the printing function
void ibis::sbiad::print(std::ostream& out) const {
    out << "index(multicomponent interval ncomp=" << bases.size() << ") for "
        << col->partition()->name() << '.' << col->name() << " contains "
        << bits.size() << " bitvectors for " << nrows
        << " objects with " << vals.size()
        << " distinct values\nThe base sizes: ";
    for (uint32_t i=0; i<bases.size(); ++i)
        out << bases[i] << ' ';
    const uint32_t nobs = bits.size();
    out << "\nbitvector information (number of set bits, number of "
        "bytes)\n";
    for (uint32_t i=0; i<nobs; ++i) {
        if (bits[i]) {
            out << i << '\t' << bits[i]->cnt() << '\t'
                << bits[i]->bytes() << "\n";
        }
    }
    if (ibis::gVerbose > 6) { // also print the list of distinct values
        out << "distinct values, number of apparences\n";
        for (uint32_t i=0; i<vals.size(); ++i) {
            out.precision(12);
            out << vals[i] << '\t' << cnts[i] << "\n";
        }
    }
    out << "\n";
} // ibis::sbiad::print

// create index based data in dt -- have to start from data directly
long ibis::sbiad::append(const char* dt, const char* df, uint32_t nnew) {
    const uint32_t nb = bases.size();
    clear();            // clear the current content
    construct2(dt, nb); // scan data twice to build the new index
    //write(dt);                // write out the new content
    return nnew;
} // ibis::sbiad::append

// compute the bitvector that represents the answer for x = b
void ibis::sbiad::evalEQ(ibis::bitvector& res, uint32_t b) const {
#if DEBUG+0 > 0 || _DEBUG+0 > 0
    LOGGER(ibis::gVerbose >= 0)
        << "DEBUG -- sbiad::evalEQ(" << b << ")...";
#endif
    if (b >= vals.size()) {
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
                    if (bits[offset+k] == 0)
                        activate(offset+k);
                    if (bits[offset+k]) {
                        if (bits[offset+k+1] == 0)
                            activate(offset+k+1);
                        if (bits[offset+k+1])
                            tmp = *(bits[offset+k]) -
                                *(bits[offset+k+1]);
                        else
                            tmp = new ibis::bitvector(*(bits[offset+k]));
                    }
                    else {
                        tmp = 0;
                    }
                }
                else if (k > nb2) {
                    if (bits[offset+k-nb2] == 0)
                        activate(offset+k-nb2);
                    if (bits[offset+k-nb2]) {
                        if (bits[offset+k-nb2-1] == 0)
                            activate(offset+k-nb2-1);
                        if (bits[offset+k-nb2-1])
                            tmp = *(bits[offset+k-nb2]) -
                                *(bits[offset+k-nb2-1]);
                        else
                            tmp = new ibis::bitvector(*(bits[offset+k-nb2]));
                    }
                    else {
                        tmp = 0;
                    }
                }
                else {
                    if (bits[offset] == 0)
                        activate(offset);
                    if (bits[offset+k] == 0)
                        activate(offset+k);
                    if (bits[offset] && bits[offset+k])
                        tmp = *(bits[offset]) & *(bits[offset+k]);
                    else
                        tmp = 0;
                }
                if (tmp)
                    res &= *tmp;
                else
                    res.set(0, res.size());
                delete tmp;
                offset += bases[i] - nb2;
            }
            else {
                if (bits[offset] == 0)
                    activate(offset);
                if (0 == k) {
                    if (bits[offset])
                        res &= *(bits[offset]);
                    else
                        res.set(0, res.size());
                }
                else if (bits[offset]) {
                    res -= *(bits[offset]);
                }
                ++ offset;
            }
            b /= bases[i];
        }
    }
} // evalEQ

// compute the bitvector that is the answer for the query x <= b
void ibis::sbiad::evalLE(ibis::bitvector& res, uint32_t b) const {
#if DEBUG+0 > 0 || _DEBUG+0 > 0
    LOGGER(ibis::gVerbose >= 0)
        << "DEBUG -- sbiad::evalLE(" << b << ")...";
#endif
    if (b+1 >= vals.size()) {
        res.set(1, nrows);
    }
    else {
        uint32_t k, nb2;
        uint32_t i = 0; // index into components
        uint32_t offset = 0;
        // skip till the first component that isn't the maximum value
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
                if (k < nb2) {
                    const uint32_t j = offset+k+1;
                    if (bits[j] == 0)
                        activate(j);
                    if (bits[j])
                        res -= *(bits[j]);
                }
                else if (k > nb2) {
                    const uint32_t j = offset+k-nb2;
                    if (bits[j] == 0)
                        activate(j);
                    if (bits[j])
                        res |= *(bits[j]);
                }
                offset += bases[i] - nb2;
            }
            else {
                if (k != 0)
                    res.flip();
                ++ offset;
            }
            b /= bases[i];
        }
        else {
            res.set(1, nrows);
        }
        ++ i;
        // deal with the remaining components
        while (i < bases.size()) {
            k = b % bases[i];
            nb2 = (bases[i] - 1) / 2;
            if (bases[i] > 2) {
                if (k < nb2) {
                    ibis::bitvector* tmp;
                    if (bits[offset+k] == 0)
                        activate(offset+k);
                    if (bits[offset+k])
                        res &= *(bits[offset+k]);
                    else
                        res.set(0, res.size());
                    if (bits[offset+k+1] == 0)
                        activate(offset+k+1);
                    if (bits[offset+k+1])
                        res -= *(bits[offset+k+1]);
                    if (k > 0) {
                        if (bits[offset] == 0)
                            activate(offset);
                        if (bits[offset]) {
                            if (bits[offset+k]) {
                                tmp = *(bits[offset]) - *(bits[offset+k]);
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
                    if (k+1 < bases[i]) {
                        if (bits[offset+k-nb2] == 0)
                            activate(offset+k-nb2);
                        if (bits[offset+k-nb2])
                            res &= *(bits[offset+k-nb2]);
                        else
                            res.set(0, res.size());
                    }
                    if (bits[offset+k-nb2-1] == 0)
                        activate(offset+k-nb2-1);
                    if (bits[offset+k-nb2-1])
                        res |= *(bits[offset+k-nb2-1]);
                    if (k > nb2+1) {
                        if (bits[offset] == 0)
                            activate(offset);
                        if (bits[offset])
                            res |= *(bits[offset]);
                    }
                }
                else { // k = nb2
                    if (bits[offset] == 0)
                        activate(offset);
                    if (bits[offset]) {
                        if (bits[offset+k] == 0)
                            activate(offset+k);
                        if (bits[offset+k]) {
                            res &= *(bits[offset]);
                            ibis::bitvector* tmp = *(bits[offset]) -
                                *(bits[offset+k]);
                            res |= *tmp;
                            delete tmp;
                        }
                        else {
                            res.copy(*(bits[offset]));
                        }
                    }
                    else {
                        res.set(0, res.size());
                    }
                }
                offset += (bases[i] - nb2);
            }
            else {
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
void ibis::sbiad::evalLL(ibis::bitvector& res,
                         uint32_t b0, uint32_t b1) const {
#if DEBUG+0 > 0 || _DEBUG+0 > 0
    LOGGER(ibis::gVerbose >= 0)
        << "DEBUG -- sbiad::evalLL(" << b0 << ", " << b1 << ")...";
#endif
    if (b0 >= b1) { // no hit
        res.set(0, nrows);
    }
    else if (b1+1 >= vals.size()) { // x > b0
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
                        if (bits[offset+k0+1] == 0)
                            activate(offset+k0+1);
                        if (bits[offset+k0+1] != 0)
                            low -= *(bits[offset+k0+1]);
                    }
                    else if (k0 > nb2) {
                        if (bits[offset+k0-nb2] == 0)
                            activate(offset+k0-nb2);
                        if (bits[offset+k0-nb2] != 0)
                            low |= *(bits[offset+k0-nb2]);
                    }
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                    if (ibis::gVerbose > 30 ||
                        (low.bytes() < (1U << ibis::gVerbose))) {
                        LOGGER(ibis::gVerbose >= 0)
                            << "DEBUG -- sbiad::evalLL: low "
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
                        if (bits[offset+k1+1] == 0)
                            activate(offset+k1+1);
                        if (bits[offset+k1+1] != 0)
                            res -= *(bits[offset+k1+1]);
                    }
                    else if (k1 > nb2) {
                        if (bits[offset+k1-nb2] == 0)
                            activate(offset+k1-nb2);
                        if (bits[offset+k1-nb2] != 0)
                            res |= *(bits[offset+k1-nb2]);
                    }
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                    if (ibis::gVerbose > 30 ||
                        (res.bytes() < (1U << ibis::gVerbose))) {
                        LOGGER(ibis::gVerbose >= 0)
                            << "DEBUG -- sbiad::evalLL: high "
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
            else { // bases[i] >= 2
                if (k0 == 0) {
                    if (bits[offset] == 0)
                        activate(offset);
                    if (bits[offset] != 0)
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
                    if (bits[offset] != 0)
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
            ibis::bitvector* tmp;
            if (b1 > b0) { // low and res has to be separated
                k0 = b0 % bases[i];
                k1 = b1 % bases[i];
                b0 /= bases[i];
                b1 /= bases[i];
                if (bases[i] > 2) {
                    nb2 = (bases[i] - 1) / 2;
                    // update low according to k0
                    if (k0+nb2+1 < bases[i]) {
                        if (bits[offset+k0] == 0)
                            activate(offset+k0);
                        if (bits[offset+k0] != 0)
                            low &= *(bits[offset+k0]);
                        else
                            low.set(0, low.size());
                        if (bits[offset+k0+1] == 0)
                            activate(offset+k0+1);
                        if (bits[offset+k0+1] != 0)
                            low -= *(bits[offset+k0+1]);
                        if (k0 > 0) {
                            if (bits[offset] == 0)
                                activate(offset);
                            if (bits[offset] != 0) {
                                if (bits[offset+k0] == 0)
                                    activate(offset+k0);
                                if (bits[offset+k0] != 0) {
                                    tmp = *(bits[offset])
                                        - *(bits[offset+k0]);
                                    low |= *tmp;
                                    delete tmp;
                                }
                            }
                            else {
                                low |= *(bits[offset]);
                            }
                        }
                    }
                    else if (k0 > nb2) {
                        if (k0+1 < bases[i]) {
                            if (bits[offset+k0-nb2] == 0)
                                activate(offset+k0-nb2);
                            if (bits[offset+k0-nb2] != 0)
                                low &= *(bits[offset+k0-nb2]);
                            else
                                low.set(0, low.size());
                        }
                        if (bits[offset+k0-nb2-1] == 0)
                            activate(offset+k0-nb2-1);
                        if (bits[offset+k0-nb2-1] != 0)
                            low |= *(bits[offset+k0-nb2-1]);
                        if (k0 > nb2+1) {
                            if (bits[offset] == 0)
                                activate(offset);
                            if (bits[offset] != 0)
                                low |= *(bits[offset]);
                        }
                    }
                    else { // k0 = nb2
                        // res &= (bits[offset] & bits[offset+k0])
                        // res |= (bits[offset] - bits[offset+k0])
                        if (bits[offset] == 0)
                            activate(offset);
                        if (bits[offset] != 0) {
                            if (bits[offset+k0] == 0)
                                activate(offset+k0);
                            if (bits[offset+k0]) {
                                low &= *(bits[offset]);
                                tmp = *(bits[offset]) - *(bits[offset+k0]);
                                low |= *tmp;
                                delete tmp;
                            }
                            else {
                                low.copy(*(bits[offset]));
                            }
                        }
                        else {
                            low.set(0, low.size());
                        }
                    }
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                    if (ibis::gVerbose > 30 ||
                        (low.bytes() < (1U << ibis::gVerbose))) {
                        LOGGER(ibis::gVerbose >= 0)
                            << "DEBUG -- sbiad::evalLL: low "
                            "(component[" << i << "] <= " << k0 << ") "
                            << low;
                    }
#endif
                    // update res according to k1
                    if (k1+nb2+1 < bases[i]) {
                        if (bits[offset+k1] == 0)
                            activate(offset+k1);
                        if (bits[offset+k1] != 0)
                            res &= *(bits[offset+k1]);
                        else
                            res.set(0, res.size());
                        if (bits[offset+k1+1] == 0)
                            activate(offset+k1+1);
                        if (bits[offset+k1+1] != 0)
                            res -= *(bits[offset+k1+1]);
                        if (k1 > 0) {
                            if (bits[offset] == 0)
                                activate(offset);
                            if (bits[offset]) {
                                if (bits[offset+k1]) {
                                    tmp = *(bits[offset])
                                        - *(bits[offset+k1]);
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
                            if (bits[offset+k1-nb2] == 0)
                                activate(offset+k1-nb2);
                            if (bits[offset+k1-nb2] != 0)
                                res &= *(bits[offset+k1-nb2]);
                            else
                                res.set(0, res.size());
                        }
                        if (bits[offset+k1-nb2-1] == 0)
                            activate(offset+k1-nb2-1);
                        if (bits[offset+k1-nb2-1] != 0)
                            res |= *(bits[offset+k1-nb2-1]);
                        if (k1 > nb2+1) {
                            if (bits[offset] == 0)
                                activate(offset);
                            if (bits[offset] != 0)
                                res |= *(bits[offset]);
                        }
                    }
                    else { // k1 = nb2
                        if (bits[offset] == 0)
                            activate(offset);
                        if (bits[offset] != 0) {
                            if (bits[offset+k1] == 0)
                                activate(offset+k1);
                            if (bits[offset+k1] != 0) {
                                res &= *(bits[offset]);
                                tmp = *(bits[offset]) - *(bits[offset+k1]);
                                res |= *tmp;
                                delete tmp;
                            }
                            else {
                                res.copy(*bits[offset]);
                            }
                        }
                        else {
                            res.set(0, res.size());
                        }
                    }
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                    if (ibis::gVerbose > 30 ||
                        (res.bytes() < (1U << ibis::gVerbose))) {
                        LOGGER(ibis::gVerbose >= 0)
                            << "DEBUG -- sbiad::evalLL: high "
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
                        if (k0 == 0)
                            low &= *(bits[offset]);
                        else
                            low |= *(bits[offset]);
                        if (k1 == 0)
                            res &= *(bits[offset]);
                        else
                            res |= *(bits[offset]);
                    }
                    else {
                        if (k0 == 0)
                            low.set(0, low.size());
                        if (k1 == 0)
                            res.set(0, low.size());
                    }
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
                            if (bits[offset+k1] == 0)
                                activate(offset+k1);
                            if (bits[offset+k1] != 0) {
                                if (bits[offset+k1+1] == 0)
                                    activate(offset+k1+1);
                                if (bits[offset+k1+1] != 0)
                                    tmp = *(bits[offset+k1]) -
                                        *(bits[offset+k1+1]);
                                else
                                    tmp = new
                                        ibis::bitvector(*(bits[offset+k1]));
                            }
                            else {
                                tmp = 0;
                            }
                        }
                        else if (k1 > nb2) {
                            if (bits[offset+k1-nb2] == 0)
                                activate(offset+k1-nb2);
                            if (bits[offset+k1-nb2] != 0) {
                                if (bits[offset+k1-nb2-1] == 0)
                                    activate(offset+k1-nb2-1);
                                if (bits[offset+k1-nb2-1] != 0)
                                    tmp = *(bits[offset+k1-nb2]) -
                                        *(bits[offset+k1-nb2-1]);
                                else
                                    tmp = new ibis::bitvector
                                        (*(bits[offset+k1-nb2]));
                            }
                            else {
                                tmp = 0;
                            }
                        }
                        else { // k1 = nb2
                            if (bits[offset] == 0)
                                activate(offset);
                            if (bits[offset] != 0) {
                                if (bits[offset+k1] == 0)
                                    activate(offset+k1);
                                if (bits[offset+k1] != 0)
                                    tmp = *(bits[offset]) &
                                        *(bits[offset+k1]);
                                else
                                    tmp = new
                                        ibis::bitvector(*(bits[offset]));
                            }
                            else {
                                tmp = 0;
                            }
                        }
                        if (tmp) {
                            res &= *tmp;
                            delete tmp;
                        }
                        else {
                            res.set(0, res.size());
                        }
                        offset += bases[i] - nb2;
                    }
                    else { // bases[i] <= 2
                        if (bits[offset] == 0)
                            activate(offset);
                        if (bits[offset] != 0) {
                            if (k1 == 0)
                                res &= *(bits[offset]);
                            else
                                res -= *(bits[offset]);
                        }
                        else if (k1 == 0) {
                            res.set(0, res.size());
                        }
                        ++ offset;
                    }
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                    if (ibis::gVerbose > 30 ||
                        (res.bytes() < (1U << ibis::gVerbose))) {
                        LOGGER(ibis::gVerbose >= 0)
                            << "DEBUG -- sbiad::evalLL: res "
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

// Evaluate the query expression
long ibis::sbiad::evaluate(const ibis::qContinuousRange& expr,
                           ibis::bitvector& lower) const {
    if (bits.empty()) {
        lower.set(0, nrows);
        return 0;
    }

    // values in the range [hit0, hit1) satisfy the query expression
    uint32_t hit0, hit1;
    locate(expr, hit0, hit1);

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
    else if (hit1 == vals.size()) { // >= hit0 (as NOT (<= hit0-1))
        evalLE(lower, hit0-1);
        lower.flip();
    }
    else { // need to go through most bitvectors four times
        evalLL(lower, hit0-1, hit1-1);  // (hit0-1, hit1-1]
    }
    return lower.cnt();
} // ibis::sbiad::evaluate

// Evaluate a set of discrete range conditions.
long ibis::sbiad::evaluate(const ibis::qDiscreteRange& expr,
                           ibis::bitvector& lower) const {
    const ibis::array_t<double>& varr = expr.getValues();
    lower.set(0, nrows);
    for (unsigned i = 0; i < varr.size(); ++ i) {
        unsigned int itmp = locate(varr[i]);
        if (itmp > 0 && vals[itmp-1] == varr[i]) {
            -- itmp;
            ibis::bitvector tmp;
            evalEQ(tmp, itmp);
            if (tmp.size() == lower.size())
                lower |= tmp;
        }
    }
    return lower.cnt();
} // ibis::sbiad::evaluate
