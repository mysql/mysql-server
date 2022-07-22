// $Id$
// Author: John Wu <John.Wu at ACM.org>
// Copyright (c) 2012-2016 the Regents of the University of California
//
// This file contains the implementation of the class called ibis::slice.
//
#if defined(_WIN32) && defined(_MSC_VER)
#pragma warning(disable:4786)   // some identifier longer than 256 characters
#endif
#include "irelic.h"
#include "part.h"
#include "column.h"
#include "resource.h"

#include <typeinfo>     // typeid

#define FASTBIT_SYNC_WRITE 1
////////////////////////////////////////////////////////////////////////
// functions from ibis::islice
//
ibis::slice::~slice() {
    clear();
}

/// Construct a bitmap index from current data.
ibis::slice::slice(const ibis::column* c, const char* f) : ibis::skive(0) {
    if (c == 0) return;  // nothing can be done
    try {
        if (! isSuitable(*c, f)) return;

        col = c;
        int ierr = construct(f);
        if (ierr < 0) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- slice[" << c->partition()->name() << '.'
                << c->name() << "]::ctor received a return value of "
                << ierr << " from function construct";
            clear();
        }

        if (ibis::gVerbose > 2) {
            ibis::util::logger lg;
            const uint32_t card = vals.size();
            const uint32_t nbits = bits.size();
            lg() << "slice[" << col->partition()->name() << '.' << col->name()
                 << "]::ctor -- constructed a bit-sliced index with " << nbits
                 << " bitmap" << (nbits>1?"s":"") << " on " << card
                 << " possible value" << (card>1?"s":"") << " and " << nrows
                 << " row" << (nrows>1?"s":"");
            if (ibis::gVerbose > 6) {
                lg() << "\n";
                print(lg());
            }
        }
    }
    catch (...) {
        LOGGER(ibis::gVerbose > 1)
            << "Warning -- slice[" << c->partition()->name() << '.'
            << c->name() << "]::ctor receiveed an exception, cleaning up ...";
        clear();
        throw;
    }
} // constructor

/// Reconstruct from content of a storage object.
/// The content of the file (following the 8-byte header) is
///@code
/// nrows(uint32_t)       -- the number of bits in each bit sequence
/// nobs (uint32_t)       -- the number of bit sequences
/// card (uint32_t)       -- the number of possible values, i.e., cardinality
/// (padding to ensure the next data element is on 8-byte boundary)
/// values (double[card]) -- the possible values as doubles
/// offset ([nobs+1])     -- the starting positions of the bit sequences (as
///                             bit vectors)
/// cnts (uint32_t[card]) -- the counts for each possible value
/// bitvectors            -- the bitvectors one after another
///@endcode
ibis::slice::slice(const ibis::column* c, ibis::fileManager::storage* st,
                   size_t start)
    : ibis::skive(c, st, start) {
    if (ibis::gVerbose > 2) {
        ibis::util::logger lg;
        const uint32_t card = vals.size();
        const uint32_t nbits = bits.size();
        lg() << "slice[" << col->partition()->name() << '.' << col->name()
             << "]::ctor -- intialized a bit-sliced index with " << nbits
             << " bitmap" << (nbits>1?"s":"") << " on " << card
             << " possible value" << (card>1?"s":"") << " and " << nrows
             << " row" << (nrows>1?"s":"") << " from storage object @ "
             << st << " offset " << start;
        if (ibis::gVerbose > 6) {
            lg() << "\n";
            print(lg());
        }
    }
}

// the argument is the name of the directory or the file name
int ibis::slice::write(const char* dt) const {
    if (vals.empty()) return -1;

    std::string fnm, evt;
    evt = "slice";
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
    char header[] = "#IBIS\11\0\0";
    header[5] = (char)ibis::index::SLICE;
    header[6] = (char)(useoffset64 ? 8 : 4);
    off_t ierr = UnixWrite(fdes, header, 8);
    if (ierr < 8) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt
            << " failed to write the 8-byte header, ierr = " << ierr;
        return -3;
    }
    if (useoffset64)
        ierr = write64(fdes);
    else
        ierr = write32(fdes);
    if (ierr >= 0) {
#if defined(FASTBIT_SYNC_WRITE)
#if _POSIX_FSYNC+0 > 0
        (void) UnixFlush(fdes); // write to disk
#elif defined(_WIN32) && defined(_MSC_VER)
        (void) _commit(fdes);
#endif
#endif

        LOGGER(ibis::gVerbose > 3)
            << evt << " wrote " << bits.size() << " bitmap"
            << (bits.size()>1?"s":"") << " to file " << fnm;
    }
    return ierr;
} // ibis::slice::write

// the printing function
void ibis::slice::print(std::ostream& out) const {
    out << "index(slice) for " << col->partition()->name() << '.'
        << col->name() << " contains " << bits.size() << " bitvectors for "
        << nrows << " objects \n";
    const uint32_t nobs = bits.size();
    if (nobs > 0) { // the short form
        out << "bitvector information (number of set bits, number "
            << "of bytes)\n";
        for (uint32_t i=0; i<nobs; ++i) {
            if (bits[i])
                out << i << '\t' << bits[i]->cnt() << '\t'
                    << bits[i]->bytes() << "\n";
        }
    }
    if (ibis::gVerbose > 6) { // also print the list of possible values
        out << "possible values, number of apparences\n";
        for (uint32_t i=0; i<vals.size(); ++i) {
            out.precision(12);
            out << vals[i] << '\t' << cnts[i] << "\n";
        }
    }
    out << "\n";
} // ibis::slice::print

/// Are values of the given column suitable for a bit-sliced index?
/// Returns true for yes.
///
/// The bit-sliced index can only be used for unsigned integers.  This
/// function will return false if the column type is not integer or the
/// integer values are not all non-negative.
bool ibis::slice::isSuitable(const ibis::column &col, const char *fd) {
    bool res = col.isUnsignedInteger();
    if (res) return res;

    res = col.isSignedInteger();
    if (! res) return res;

    double lo = col.lowerBound();
    double hi = col.upperBound();
    res = (lo <= hi && lo >= 0.0);
    if (res) return res;

    if (lo > hi)
        const_cast<ibis::column&>(col).computeMinMax(fd);

    lo = col.lowerBound();
    hi = col.upperBound();
    res = (lo <= hi && lo >= 0.0);
    return res;
} // ibis::slice::isSuitable

/// Template function to work with a specific column type.
template<typename T> int
ibis::slice::constructT(const char* f) {
    if (col == 0 || col->partition() == 0) return -1;
    if (col->partition()->nRows() == 0) return 0;
    nrows = col->partition()->nRows();

    array_t<T> val;
    std::string fnm; // name of the data file
    dataFileName(fnm, f);
    int ierr;
    if (! fnm.empty())
        ierr = ibis::fileManager::instance().getFile(fnm.c_str(), val);
    else
        ierr = col->getValuesArray(&val);
    if (ierr < 0 || val.empty()) {
        LOGGER(ibis::gVerbose > 1)
            << "Warning -- slice::construct<" << typeid(T).name()
            << "> failed to read the data file \"" << fnm
            << "\", getFile returned " << ierr;
        return -1;
    }

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

    nrows = val.size();
    if (val.size() > mask.size()) {
        LOGGER(ibis::gVerbose > 1)
            << "slice::construct<" << typeid(T).name()
            << "> found the data file \"" << fnm
            << "\" to contain more elements (" << val.size()
            << ") then expected (" << mask.size()
            << "), adjust mask size";
        mask.adjustSize(nrows, nrows);
    }

    ibis::bitvector::indexSet iset = mask.firstIndexSet();
    uint32_t nind = iset.nIndices();
    const ibis::bitvector::word_t *iix = iset.indices();
    while (nind) {
        if (iset.isRange()) { // a range
            uint32_t k = (iix[1] < nrows ? iix[1] : nrows);
            for (uint32_t ir = *iix; ir < k; ++ ir) {
                uint64_t iv = val[ir];
                unsigned ii = bits.size();
                if ((T) iv != val[ir] || (iv >> ii) > 0)
                    return -2;

                ii = 0;
                ++ cnts[iv];
                while (iv > 0) {
                    if ((iv & 1) != 0)
                        bits[ii]->setBit(ir, 1);
                    iv >>= 1;
                    ++ ii;
                }
            }
        }
        else if (*iix+ibis::bitvector::bitsPerLiteral() < nrows) {
            // a list of indices
            for (uint32_t i = 0; i < nind; ++i) {
                uint32_t ir = iix[i];
                //setBit(ir, val[ir]);
                uint64_t iv = val[ir];
                unsigned ii = bits.size();
                if ((T) iv != val[ir] || (iv >> ii) > 0)
                    return -3;

                ii = 0;
                ++ cnts[iv];
                while (iv > 0) {
                    if ((iv & 1) != 0)
                        bits[ii]->setBit(ir, 1);
                    iv >>= 1;
                    ++ ii;
                }
            }
        }
        else {
            for (uint32_t i = 0; i < nind; ++i) {
                uint32_t ir = iix[i];
                if (ir < nrows) {
                    //setBit(ir, val[ir]);
                    uint64_t iv = val[ir];
                    unsigned ii = bits.size();
                    if ((T) iv != val[ir] || (iv >> ii) > 0)
                        return -3;

                    ii = 0;
                    ++ cnts[iv];
                    while (iv > 0) {
                        if ((iv & 1) != 0)
                            bits[ii]->setBit(ir, 1);
                        iv >>= 1;
                        ++ ii;
                    }
                }
            }
        }

        ++ iset;
        nind = iset.nIndices();
        if (*iix >= nrows)
            nind = 0;
    } // while (nind)

    nrows = mask.size();
    return 0;
} // ibis::slice::constructT

/// Generate a new bit-sliced index.  This version works with the values
/// directly without checking 
int ibis::slice::construct(const char* f) {
    clear();
    if (col == 0 || col->partition() == 0) return -1;
    if (col->partition()->nRows() == 0) return 0;
    if (! (col->lowerBound() >= 0.0 && col->lowerBound() <= col->upperBound()))
        const_cast<ibis::column*>(col)->computeMinMax(f);
    if (! (col->lowerBound() >= 0.0 && col->lowerBound() <= col->upperBound()))
        return -4;

    uint64_t upp = static_cast<uint64_t>(col->upperBound());
    if ((double)upp != col->upperBound())
        return -5;

    ++ upp;
    cnts.resize(upp);
    vals.resize(upp);
    for (uint64_t j = 0; j < upp; ++ j) {
        cnts[j] = 0;
        vals[j] = j;
    }
    -- upp;
    // compute the number of bitvectors
    uint32_t nobs = 0;
    while (upp > 0) {
        upp >>= 1;
        ++ nobs;
    }
    if (nobs == 0)
        nobs = 1;
    bits.resize(nobs);
    for (uint32_t i = 0; i < nobs; ++i)
        bits[i] = new ibis::bitvector;

    int ierr = -6;
    // need to do different things for different columns
    switch (col->type()) {
    case ibis::ULONG: {// unsigned long int
        ierr = constructT<uint64_t>(f);
        break;}
    case ibis::LONG: {// signed long int
        ierr = constructT<int64_t>(f);
        break;}
    case ibis::CATEGORY:
    case ibis::UINT: {// unsigned int
        ierr = constructT<uint32_t>(f);
        break;}
    case ibis::INT: {// signed int
        ierr = constructT<int32_t>(f);
        break;}
    case ibis::USHORT: {// unsigned short int
        ierr = constructT<uint16_t>(f);
        break;}
    case ibis::SHORT: {// signed short int
        ierr = constructT<int16_t>(f);
        break;}
    case ibis::UBYTE: {// unsigned char
        ierr = constructT<unsigned char>(f);
        break;}
    case ibis::BYTE: {// signed char
        ierr = constructT<signed char>(f);
        break;}
    case ibis::FLOAT: {// (4-byte) floating-point values
        ierr = constructT<float>(f);
        break;}
    case ibis::DOUBLE: {// (8-byte) floating-point values
        ierr = constructT<double>(f);
        break;}
    default:
        col->logWarning("slice::ctor", "failed to create bit slice index "
                        "for this type of column");
        break;
    }

    if (ierr < 0) return ierr;

    // make sure all bit vectors are the same size
    for (uint32_t i = 0; i < nobs; ++i) {
        bits[i]->adjustSize(0, nrows);
        bits[i]->compress();
    }

    optionalUnpack(bits, col->indexSpec()); // uncompress the bitmaps
    // write out the current content to standard output
    if (ibis::gVerbose > 4) {
        ibis::util::logger lg;
        print(lg());
    }
    return 0;
} // ibis::slice::construct

// create index based data in dt -- have to start from data directly
long ibis::slice::append(const char* dt, const char* df, uint32_t nnew) {
    clear();            // clear the current content
    construct(dt);      // generate the new version of the index
    //write(dt);                // write out the new content
    return nnew;
} // ibis::slice::append
