// $Id$
// Author: John Wu <John.Wu at ACM.org>
// Copyright (c) 2000-2016 the Regents of the University of California
//
// This file contains the implementation of the class called ibis::sapid.
//
// The word sapid is the closest English word to the Italian word sbiad
// in terms of edit distance.
//
// fade  -- multicomponent range-encoded bitmap index
// sbiad -- multicomponent interval-encoded bitmap index
// sapid -- multicomponent equality-encoded bitmap index
//
// Definition of the word sapid according to Webster's Revised Unabridged
// Dictionary
//  sapid a. [L. sapidus, fr. sapere to taste: cf. F. sapide. See Sapient,
//  Savor.] Having the power of affecting the organs of taste; possessing
//  savor, or flavor.
//
//  Camels, to make the water sapid, do raise the mud with their feet. -- Sir
//  T. Browne.
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
// functions of ibis::sapid
//
/// Constructor.  If a bitmap index is present in the specified location,
/// its header will be read into memory, otherwise a new bitmap index is
/// created from current data.
ibis::sapid::sapid(const ibis::column* c, const char* f, const uint32_t nbase)
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
            lg() << "sapid[" << col->partition()->name() << '.' << col->name()
                 << "]::ctor -- constructed a " << bases.size()
                 << "-component equality index with "
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
            << "Warning -- sapid[" << col->partition()->name() << '.'
            << col->name() << "]::ctor received an exception, cleaning up ...";
        clear();
        throw;
    }
} // constructor

/// Reconstruct an index from the content of a storage object.
/// The content of the file (following the 8-byte header) is
///@code
/// nrows(uint32_t)         -- the number of bits of a bit sequence
/// nobs (uint32_t)         -- the number of bit sequences
/// card (uint32_t)         -- the number of distinct values, i.e., cardinality
/// (padding to ensure the next data element is on 8-byte boundary)
/// values (double[card])   -- the distinct values as doubles
/// offset([nobs+1])        -- the starting positions of the bit sequences (as
///                             bit vectors)
/// nbases(uint32_t)        -- the number of components (bases) used
/// cnts (uint32_t[card])   -- the counts for each distinct value
/// bases(uint32_t[nbases]) -- the bases sizes
/// bitvectors              -- the bitvectors one after another
///@endcode
ibis::sapid::sapid(const ibis::column* c, ibis::fileManager::storage* st,
                   size_t start) : ibis::fade(c, st, start) {
    if (ibis::gVerbose > 2) {
        ibis::util::logger lg;
        lg() << "sapid[" << col->partition()->name() << '.' << col->name()
             << "]::ctor -- initialized a " << bases.size()
             << "-component equality index with "
             << bits.size() << " bitmap" << (bits.size()>1?"s":"")
             << " for " << nrows << " row" << (nrows>1?"s":"")
             << " from a storage object @ " << st;
        if (ibis::gVerbose > 6) {
            lg() << "\n";
            print(lg());
        }
    }
} // reconstruct data from content of a file

/// Write the content of the index to the specified location.  The argument
/// is the name of a directory or a file name.  It is used by the function
/// indexFileName to determine the actual index file name.
int ibis::sapid::write(const char* dt) const {
    if (vals.empty()) return -1;

    std::string fnm, evt;
    evt = "sapid";
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
        activate();

    int fdes = UnixOpen(fnm.c_str(), OPEN_WRITENEW, OPEN_FILEMODE);
    if (fdes < 0) { // try again
        ibis::fileManager::instance().flushFile(fnm.c_str());
        fdes = UnixOpen(fnm.c_str(), OPEN_WRITENEW, OPEN_FILEMODE);
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " failed to open \"" << fnm
            << "\" for writing";
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

#ifdef FASTBIT_USE_LONG_OFFSETS
    const bool useoffset64 = true;
#else
    const bool useoffset64 = (getSerialSize()+8 > 0x80000000UL);
#endif
    char header[] = "#IBIS\14\0\0";
    header[5] = (char)ibis::index::SAPID;
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
} // ibis::sapid::write

/// This version of the constructor take one pass throught the data.  It
/// constructs an ibis::index::VMap first, then constructs the sapid from
/// the VMap.  It uses more computer memory than the two-pass version.
void ibis::sapid::construct1(const char* f, const uint32_t nbase) {
    VMap bmap; // a map between values and their position
    try {
        mapValues(f, bmap);
    }
    catch (...) { // need to clean up bmap
        if (ibis::gVerbose > 0) {
            LOGGER(ibis::gVerbose >= 0)
                << "sapid::construct reclaiming storage "
                "allocated to bitvectors (" << bmap.size() << ")";
        }
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

        if (ibis::gVerbose > -1) {
            LOGGER(ibis::gVerbose >= 0)
                << "Warning -- sapid::construct1 the bitvectors "
                "do not have the expected size(" << col->partition()->nRows()
                << "). stopping..";
        }
        throw ibis::bad_alloc("sapid::construct1 failed due to incorrect "
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
        col->logMessage("sapid::construct", "initialized the array of "
                        "bitvectors, start converting %lu bitmaps into %lu-"
                        "component range code (with %lu bitvectors)",
                        static_cast<long unsigned>(vals.size()),
                        static_cast<long unsigned>(nb),
                        static_cast<long unsigned>(nobs));
    }

    // generate the correct bitmaps
    i = 0;
    if (nb > 1) {
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
#if DEBUG+0 > 0 || _DEBUG+0 > 0
            if (ibis::gVerbose > 11 && (i & 255) == 255) {
                LOGGER(ibis::gVerbose >= 0) << i << " ... ";
            }
#endif
        }
        for (i = 0; i < nobs; ++i) {
            if (bits[i] == 0) {
                bits[i] = new ibis::bitvector();
                bits[i]->set(0, nrows);
            }
            else {
                bits[i]->compress();
            }
        }
    }
    else { // one component -- only need to copy the pointers
        for (VMap::const_iterator it = bmap.begin(); it != bmap.end();
             ++it, ++i)
            bits[i] = (*it).second;
    }
    bmap.clear();
#if DEBUG+0 > 0 || _DEBUG+0 > 0
    if (ibis::gVerbose > 11) {
        LOGGER(ibis::gVerbose >= 0) << vals.size() << " DONE";
    }
#endif
    optionalUnpack(bits, col->indexSpec());

    // write out the current content
    if (ibis::gVerbose > 4) {
        ibis::util::logger lg;
        print(lg());
    }
} // construct1

// assume that the array vals is initialized properly, this function
// converts the value val into a set of bits to be stored in the bitvectors
// contained in bits
// **** to be used by construct2() to build a new ibis::sapid index ****
void ibis::sapid::setBit(const uint32_t i, const double val) {
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
    else {
        return;
    }

    // now we know what bitvectors to modify
    const uint32_t nb = bases.size();
    uint32_t offset = 0; // offset into bits
    for (ii = 0; ii < nb; ++ ii) {
        jj = kk % bases[ii];
        bits[offset+jj]->setBit(i, 1);
        offset += bases[ii];
        kk /= bases[ii];
    }
} // setBit

// generate a new sapid index by passing through the data twice
// (1) scan the data to generate a list of distinct values and their counts
// (2) scan the data a second time to record the locations in bit vectors
void ibis::sapid::construct2(const char* f, const uint32_t nbase) {
    { // a block to limit the scope of hst
        histogram hst;
        mapValues(f, hst); // scan the data to produce the histogram
        if (hst.empty())   // no data, of course no index
            return;

        // convert histogram into two arrays
        uint32_t tmp = hst.size();
        vals.resize(tmp);
        cnts.resize(tmp);
        histogram::const_iterator it = hst.begin();
        for (uint32_t i = 0; i < tmp; ++i) {
            vals[i] = (*it).first;
            cnts[i] = (*it).second;
            ++ it;
        }
    }

    // determie the bases
    setBases(bases, vals.size(), nbase);
    const uint32_t nb = bases.size();

    // allocate the correct number of bitvectors
    uint32_t nobs = 0;
    for (uint32_t tmp = 0; tmp < nb; ++tmp)
        nobs += bases[tmp];
    bits.resize(nobs);
    for (uint32_t i = 0; i < nobs; ++i)
        bits[i] = new ibis::bitvector;

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

    int ierr;
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
                << "Warning -- sapid::construct2 failed to retrieve any value";
            break;
        }

        if (val.size() > mask.size()) {
            col->logWarning("sapid::construct", "the data file \"%s\" "
                            "contains more elements (%lu) then expected "
                            "(%lu)", fnm.c_str(),
                            static_cast<long unsigned>(val.size()),
                            static_cast<long unsigned>(mask.size()));
            mask.adjustSize(nrows, nrows);
        }
        ibis::bitvector::indexSet iset = mask.firstIndexSet();
        unsigned nind = iset.nIndices();
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
                << "Warning -- sapid::construct2 failed to retrieve any value";
            break;
        }

        if (val.size() > mask.size()) {
            col->logWarning("sapid::construct", "the data file \"%s\" "
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
                << "Warning -- sapid::construct2 failed to retrieve any value";
            break;
        }

        if (val.size() > mask.size()) {
            col->logWarning("sapid::construct", "the data file \"%s\" "
                            "contains more elements (%lu) then expected "
                            "(%lu)", fnm.c_str(),
                            static_cast<long unsigned>(val.size()),
                            static_cast<long unsigned>(mask.size()));
            mask.adjustSize(nrows, nrows);
        }
        ibis::bitvector::indexSet iset = mask.firstIndexSet();
        unsigned nind = iset.nIndices();
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
    case ibis::LONG: {// signed int
        array_t<int64_t> val;
        if (! fnm.empty())
            ierr = ibis::fileManager::instance().getFile(fnm.c_str(), val);
        else
            ierr = col->getValuesArray(&val);
        if (ierr < 0 || val.empty()) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- sapid::construct2 failed to retrieve any value";
            break;
        }

        if (val.size() > mask.size()) {
            col->logWarning("sapid::construct", "the data file \"%s\" "
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
                << "Warning -- sapid::construct2 failed to retrieve any value";
            break;
        }

        if (val.size() > mask.size()) {
            col->logWarning("sapid::construct", "the data file \"%s\" "
                            "contains more elements (%lu) then expected "
                            "(%lu)", fnm.c_str(),
                            static_cast<long unsigned>(val.size()),
                            static_cast<long unsigned>(mask.size()));
            mask.adjustSize(nrows, nrows);
        }
        ibis::bitvector::indexSet iset = mask.firstIndexSet();
        unsigned nind = iset.nIndices();
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
                << "Warning -- sapid::construct2 failed to retrieve any value";
            break;
        }

        if (val.size() > mask.size()) {
            col->logWarning("sapid::construct", "the data file \"%s\" "
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
                << "Warning -- sapid::construct2 failed to retrieve any value";
            break;
        }

        if (val.size() > mask.size()) {
            col->logWarning("sapid::construct", "the data file \"%s\" "
                            "contains more elements (%lu) then expected "
                            "(%lu)", fnm.c_str(),
                            static_cast<long unsigned>(val.size()),
                            static_cast<long unsigned>(mask.size()));
            mask.adjustSize(nrows, nrows);
        }
        ibis::bitvector::indexSet iset = mask.firstIndexSet();
        unsigned nind = iset.nIndices();
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
                << "Warning -- sapid::construct2 failed to retrieve any value";
            break;
        }

        if (val.size() > mask.size()) {
            col->logWarning("sapid::construct", "the data file \"%s\" "
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
                << "Warning -- sapid::construct2 failed to retrieve any value";
            break;
        }

        if (val.size() > mask.size()) {
            col->logWarning("sapid::construct", "the data file \"%s\" "
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
                << "Warning -- sapid::construct2 failed to retrieve any value";
            break;
        }

        if (val.size() > mask.size()) {
            col->logWarning("sapid::construct", "the data file \"%s\" "
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
        col->logWarning("sapid::ctor", "no need for another index");
        return;
    default:
        col->logWarning("sapid::ctor", "failed to create bit sapid index "
                        "for this type of column");
        return;
    }

    // make sure all bit vectors are the same size
    for (uint32_t i = 0; i < nobs; ++i) {
        bits[i]->adjustSize(0, nrows);
        bits[i]->compress();
    }
    optionalUnpack(bits, col->indexSpec());

    // write out the current content
    if (ibis::gVerbose > 4) {
        ibis::util::logger lg;
        print(lg());
    }
} // ibis::sapid::construct2

// a simple function to test the speed of the bitvector operations
void ibis::sapid::speedTest(std::ostream& out) const {
    if (nrows == 0) return;
    uint32_t i, nloops = 1000000000 / nrows;
    if (nloops < 2) nloops = 2;
    ibis::horometer timer;
    col->logMessage("sapid::speedTest", "testing the speed of operator -");

    activate(); // retrieve all bitvectors
    for (i = 0; i < bits.size()-1; ++i) {
        ibis::bitvector* tmp;
        tmp = *(bits[i+1]) | *(bits[i]);
        delete tmp;

        timer.start();
        for (uint32_t j=0; j<nloops; ++j) {
            tmp = *(bits[i+1]) | *(bits[i]);
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
} // ibis::sapid::speedTest

// the printing function
void ibis::sapid::print(std::ostream& out) const {
    out << "index(multicomponent equality ncomp=" << bases.size() << ") for "
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
} // ibis::sapid::print

// create index based data in dt -- have to start from data directly
long ibis::sapid::append(const char* dt, const char* df, uint32_t nnew) {
    const uint32_t nb = bases.size();
    clear();            // clear the current content
    construct2(dt, nb); // build a new index by scanning data twice
    //write(dt);                // write out the new content
    return nnew;
} // ibis::sapid::append

// add up bits[ib:ie-1] and to res -- must execute \sum_{i=ib}^{ie}, can
// not use compelement
void ibis::sapid::addBits_(uint32_t ib, uint32_t ie,
                           ibis::bitvector& res) const {
    const uint32_t nobs = bits.size();
    if (res.size() == 0) {
        res.set(0, nrows);
    }
    if (ie > nobs)
        ie = nobs;
    if (ib >= ie || ib >= nobs) {
        return;
    }
    else if (ib == 0 && ie == nobs) {
        res.set(1, nrows);
        return;
    }

    horometer timer;
    bool decmp = false;
    if (ibis::gVerbose > 4)
        timer.start();
    activate(ib, ie);

    // first determine whether or not to decompres the result
    if (ie-ib>64) {
        decmp = true;
    }
    else if (ie - ib > 3) {
        uint32_t tot = 0;
        for (uint32_t i = ib; i < ie; ++i) {
            if (bits[i])
                tot += bits[i]->bytes();
        }
        if (tot > (nrows >> 2))
            decmp = true;
        else if (tot > (nrows >> 3) && ie-ib > 4)
            decmp = true;
    }
    if (decmp) { // use decompressed res
        if (ibis::gVerbose > 5)
            ibis::util::logMessage("sapid", "addBits(%lu, %lu) using "
                                   "uncompressed bitvector",
                                   static_cast<long unsigned>(ib),
                                   static_cast<long unsigned>(ie));
        if (bits[ib] != 0)
            res |= *(bits[ib]);
        res.decompress();
        for (uint32_t i = ib+1; i < ie; ++i) {
            if (bits[i])
                res |= *(bits[i]);
        }
    }
    else { // use compressed res
        if (ibis::gVerbose > 5) 
            ibis::util::logMessage("sapid", "addBits(%lu, %lu) using "
                                   "compressed bitvector",
                                   static_cast<long unsigned>(ib),
                                   static_cast<long unsigned>(ie));
        // first determine an good evaluation order (ind)
        std::vector<uint32_t> ind;
        uint32_t i, j, k;
        ind.reserve(ie-ib);
        for (i = ib; i < ie; ++i)
            if (bits[i])
                ind.push_back(i);
        // sort ind according the size of the bitvectors (insertion sort)
        for (i = 0; i < ind.size()-1; ++i) {
            k = i + 1;
            for (j = k+1; j < ind.size(); ++j)
                if (bits[ind[j]]->bytes() < bits[ind[k]]->bytes())
                    k = j;
            if (bits[ind[i]]->bytes() > bits[ind[k]]->bytes()) {
                j = ind[i];
                ind[i] = ind[k];
                ind[k] = j;
            }
            else {
                ++ i;
                if (bits[ind[i]]->bytes() > bits[ind[k]]->bytes()) {
                    j = ind[i];
                    ind[i] = ind[k];
                    ind[k] = j;
                }
            }
        }
        // evaluate according the order ind
        for (i = 0; i < ind.size(); ++i)
            res |= *(bits[ind[i]]);
    }

    if (ibis::gVerbose > 4) {
        timer.stop();
        ibis::util::logMessage("sapid", "addBits(%lu, %lu) took %g "
                               "sec(CPU), %g sec(elapsed).",
                               static_cast<long unsigned>(ib),
                               static_cast<long unsigned>(ie),
                               timer.CPUTime(), timer.realTime());
    }
} // addBits_

// compute the bitvector that is the answer for the query x = b
void ibis::sapid::evalEQ(ibis::bitvector& res, uint32_t b) const {
    if (b >= vals.size()) {
        res.set(0, nrows);
    }
    else {
        uint32_t offset = 0;
        res.set(1, nrows);
        for (uint32_t i=0; i < bases.size(); ++i) {
            const uint32_t k = offset + (b % bases[i]);
            if (bits[k] == 0)
                activate(k);
            if (bits[k] != 0)
                res &= *(bits[k]);
            else
                res.set(0, res.size());
            offset += bases[i];
            b /= bases[i];
        }
    }
} // evalEQ

// compute the bitvector that is the answer for the query x <= b
void ibis::sapid::evalLE(ibis::bitvector& res, uint32_t b) const {
    if (b+1 >= vals.size()) {
        res.set(1, nrows);
    }
    else {
        uint32_t i = 0; // index into components
        uint32_t offset = 0;
        // skip till the first component that isn't the maximum value
        while (i < bases.size() && b % bases[i] == bases[i]-1) {
            offset += bases[i];
            b /= bases[i];
            ++ i;
        }
        // the first non-maximum component
        if (i < bases.size()) {
            const uint32_t k = b % bases[i];
            res.set(0, nrows);
            if (k+k <= bases[i]) {
                addBins(offset, offset+k+1, res);
            }
            else {
                addBins(offset+k+1, offset+bases[i], res);
                res.flip();
            }
            offset += bases[i];
            b /= bases[i];
        }
        else {
            res.set(1, nrows);
        }
        ++ i;
        // deal with the remaining components
        while (i < bases.size()) {
            const uint32_t k = b % bases[i];
            const uint32_t j = offset + k;
            if (k+1 < bases[i]) {
                if (bits[j] == 0)
                    activate(j);
                if (bits[j])
                    res &= *(bits[j]);
                else
                    res.set(0, res.size());
            }
            if (k > 0) {
                if (k+k <= bases[i]) {
                    addBins(offset, j, res);
                }
                else {
                    ibis::bitvector tmp;
                    addBins(j, offset+bases[i], tmp);
                    tmp.flip();
                    res |= tmp;
                }
            }
            offset += bases[i];
            b /= bases[i];
            ++ i;
        }
    }
} // evalLE

// compute the bitvector that answers the query b0 < x <= b1
void ibis::sapid::evalLL(ibis::bitvector& res,
                         uint32_t b0, uint32_t b1) const {
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
        uint32_t k0, k1;
        uint32_t i = 0;
        uint32_t offset = 0;
        res.clear(); // clear the current content of res
        // skip till the first component that isn't the maximum value
        while (i < bases.size()) {
            k0 = b0 % bases[i];
            k1 = b1 % bases[i];
            if (k0 == bases[i]-1 && k1 == bases[i]-1) {
                offset += bases[i];
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
            if (k0 <= k1) {
                if (k0+k0 <= bases[i]) {
                    addBins(offset, offset+k0+1, low);
                }
                else if (k0+1 < bases[i]) {
                    addBins(offset+k0+1, offset+bases[i], low);
                    low.flip();
                }
                else {
                    low.set(1, nrows);
                }
                if (k1+1 >= bases[i]) {
                    res.set(1, nrows);
                }
                else if (k0 < k1) {
                    if (k1+k1 <= k0+bases[i]) {
                        res = low;
                        addBins(offset+k0+1, offset+k1+1, res);
                    }
                    else {
                        addBins(offset+k1+1, offset+bases[i], res);
                        res.flip();
                    }
                }
                else {
                    res = low;
                }
            }
            else { // k0 > k1
                if (k1+k1 <= bases[i]) {
                    addBins(offset, offset+k1+1, res);
                }
                else if (k1+1 < bases[i]) {
                    addBins(offset+k1+1, offset+bases[i], res);
                    res.flip();
                }
                else {
                    res.set(1, nrows);
                }
                if (k0+1 >= bases[i]) {
                    low.set(1, nrows);
                }
                else if (k0+k0 <= k1+bases[i]) {
                    low = res;
                    addBins(offset+k1+1, offset+k0+1, low);
                }
                else {
                    addBins(offset+k0+1, offset+bases[i], low);
                    low.flip();
                }
            }
            offset += bases[i];
            b0 /= bases[i];
            b1 /= bases[i];
        }
        else { // i >= bases.size()
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
                if (k0+1 < bases[i]) {
                    if (bits[offset+k0] == 0)
                        activate(offset+k0);
                    low &= *(bits[offset+k0]);
                }
                if (k1+1 < bases[i]) {
                    if (bits[offset+k1] == 0)
                        activate(offset+k1);
                    res &= *(bits[offset+k1]);
                }
                ibis::bitvector tmp;
                if (k0 <= k1) {
                    if (k0 > 0) {
                        if (k0+k0 <= bases[i]) {
                            addBins(offset, offset+k0, tmp);
                        }
                        else {
                            addBins(offset+k0, offset+bases[i], tmp);
                            tmp.flip();
                        }
                        low |= tmp;
                    }
                    if (k1 > k0) {
                        if (k1+k1 <= k0+bases[i]) {
                            if (k0 > 0)
                                res |= tmp;
                            addBins(offset+k0, offset+k1, res);
                        }
                        else {
                            tmp.clear();
                            addBins(offset+k1, offset+bases[i], tmp);
                            tmp.flip();
                            res |= tmp;
                        }
                    }
                    else if (k0 > 0) { // k0 == k1
                        res |= tmp;
                    }
                }
                else { // k0 > k1
                    if (k1 > 0) {
                        if (k1+k1 <= bases[i]) {
                            addBins(offset, offset+k1, tmp);
                        }
                        else {
                            addBins(offset+k1, offset+bases[i], tmp);
                            tmp.flip();
                        }
                        res |= tmp;
                    }
                    if (k0+k0 <= k1+bases[i]) {
                        if (k1 > 0)
                            low |= tmp;
                        addBins(offset+k1, offset+k0, low);
                    }
                    else {
                        tmp.clear();
                        addBins(offset+k0, offset+bases[i], tmp);
                        tmp.flip();
                        low |= tmp;
                    }
                }
                offset += bases[i];
            }
            else { // the more significant components are the same
                res -= low;
                low.clear(); // no longer need low
                while (i < bases.size()) {
                    k1 = b1 % bases[i];
                    const uint32_t j = offset+k1;
                    if (bits[j] == 0)
                        activate(j);
                    if (bits[j] != 0)
                        res &= *(bits[j]);
                    else
                        res.set(0, res.size());
                    offset += bases[i];
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

// provide an estimation based on the current index.  Set bits in lower are
// hits for certain, set bits in upper are candidates.  Set bits in (upper
// - lower) should be checked to verifies which are actually hits.  If the
// bitvector upper contain less bits than bitvector lower (upper.size() <
// lower.size()), the content of upper is assumed to be the same as lower.
long ibis::sapid::evaluate(const ibis::qContinuousRange& expr,
                           ibis::bitvector& lower) const {
    if (bits.empty()) {
        lower.set(0, nrows);
        return 0L;
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
    else { // (hit0-1, hit1-1]
        evalLL(lower, hit0-1, hit1-1);
    }
    return lower.cnt();
} // ibis::sapid::evaluate

// Evaluate a set of discrete range conditions.
long ibis::sapid::evaluate(const ibis::qDiscreteRange& expr,
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
} // ibis::sapid::evaluate
