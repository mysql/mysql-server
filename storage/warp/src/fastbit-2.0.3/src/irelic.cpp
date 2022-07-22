// $Id$
// Author: John Wu <John.Wu at ACM.org>
// Copyright (c) 2000-2016 the Regents of the University of California
//
// This file contains the implementation of the classes defined in index.h
// The primary function from the database point of view is a function
// called estimate().  It evaluates a given range condition and produces
// two bit vectors representing the range where the actual solution lies.
// The bulk of the code is devoted to maintain and update the index called
// ibis::relic.
//
#if defined(_WIN32) && defined(_MSC_VER)
#pragma warning(disable:4786)   // some identifiers longer than 256 characters
#endif
#include "bitvector64.h"
#include "irelic.h"
#include "part.h"

#include <math.h>       // fabs
#include <sstream>      // std::ostringstream
#include <typeinfo>     // std::typeid

#define FASTBIT_SYNC_WRITE 1
////////////////////////////////////////////////////////////////////////
// functions from ibis::irelic
//
/// Construct a basic bitmap index.  It attempts to read an index from the
/// specified location.  If that fails it creates one from current data.
ibis::relic::relic(const ibis::column *c, const char *f)
    : ibis::index(c) {
    try {
        if (0 != f && 0 == read(f)) {
            return;
        }

        if (c == 0) return;  // nothing can be done
        if (vals.empty() && 
            c->type() != ibis::CATEGORY &&
            c->type() != ibis::TEXT &&
            c->type() != ibis::BLOB) {
            if (c->partition() != 0 || f != 0) {
                construct(f);
            }
            else {
                switch (c->type()) {
                case ibis::BYTE: {
                    array_t<signed char> ta;
                    if (0 <= c->getValuesArray(&ta))
                        construct(ta);
                    break;}
                case ibis::UBYTE: {
                    array_t<unsigned char> ta;
                    if (0 <= c->getValuesArray(&ta))
                        construct(ta);
                    break;}
                case ibis::SHORT: {
                    array_t<int16_t> ta;
                    if (0 <= c->getValuesArray(&ta))
                        construct(ta);
                    break;}
                case ibis::USHORT: {
                    array_t<uint16_t> ta;
                    if (0 <= c->getValuesArray(&ta))
                        construct(ta);
                    break;}
                case ibis::INT: {
                    array_t<int32_t> ta;
                    if (0 <= c->getValuesArray(&ta))
                        construct(ta);
                    break;}
                case ibis::UINT: {
                    array_t<uint32_t> ta;
                    if (0 <= c->getValuesArray(&ta))
                        construct(ta);
                    break;}
                case ibis::LONG: {
                    array_t<int64_t> ta;
                    if (0 <= c->getValuesArray(&ta))
                        construct(ta);
                    break;}
                case ibis::ULONG: {
                    array_t<uint64_t> ta;
                    if (0 <= c->getValuesArray(&ta))
                        construct(ta);
                    break;}
                case ibis::FLOAT: {
                    array_t<float> ta;
                    if (0 <= c->getValuesArray(&ta))
                        construct(ta);
                    break;}
                case ibis::DOUBLE: {
                    array_t<double> ta;
                    if (0 <= c->getValuesArray(&ta))
                        construct(ta);
                    break;}
                default: {
                    LOGGER(ibis::gVerbose > 1)
                        << "Warning -- relic::ctor"
                        << " does not support data type "
                        << ibis::TYPESTRING[static_cast<int>(c->type())];
                    break;}
                }
            }
        }
        if (! vals.empty() && ibis::gVerbose > 2) {
            ibis::util::logger lg;
            lg() << "relic[" << col->fullname()
                 << "]::ctor -- intialized an equality index with "
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
            << "Warning -- relic[" << col->fullname()
            << "]::ctor received an exception, cleaning up ...";
        clear();
        throw;
    }
} // constructor

/// Construct a dummy index.  All entries have the same value @c popu.
/// This is used to generate index for meta tags from STAR data.
ibis::relic::relic(const ibis::column* c, uint32_t popu, uint32_t ntpl)
    : ibis::index(c) {
    try {
        if (ntpl == 0)
            ntpl = c->partition()->nRows();
        nrows = ntpl;
        vals.resize(1);
        bits.resize(1);
        vals[0] = popu;
        bits[0] = new ibis::bitvector();
        bits[0]->set(1, ntpl);
        offset64.resize(2);
        offset64[0] = 0;
        offset64[1] = bits[0]->getSerialSize();
        offset32.clear();
        if (ibis::gVerbose > 5) {
            ibis::util::logger lg;
            print(lg());
        }
    }
    catch (...) {
        LOGGER(ibis::gVerbose > 1)
            << "Warning -- relic[" << (col ? col->fullname() : "?.?")
            << "]::ctor received an exception, cleaning up ...";
        clear();
        throw;
    }
} // constructor for dummy attributes

/// Construct an index from an integer array.  It assumes all values in @c
/// ind are less than @c card.  All values not less than @c card are
/// discarded and corresponding rows would be regarded as having NULL
/// values.
ibis::relic::relic(const ibis::column* c, uint32_t card,
                   array_t<uint32_t>& ind) : ibis::index(c) {
    if (ind.empty()) return;

    try {
        vals.resize(card);
        bits.resize(card);
        offset32.clear();
        offset64.resize(card+1);
        for (uint32_t i = 0; i < card; ++i) {
            vals[i] = i;
            bits[i] = new ibis::bitvector();
        }
        nrows = ind.size();
        for (uint32_t i = 0; i < nrows; ++i) {
            if (ind[i] < card) {
                bits[ind[i]]->setBit(i, 1);
            }
#if DEBUG+0 > 1 || _DEBUG+0 > 1
            else {
                LOGGER(ibis::gVerbose >= 0)
                    << "DEBUG -- relic[" << (col ? col->fullname() : "?.?")
                    << "]::ctor ind[" << i << "]=" << ind[i] << " >=" << card;
            }
#endif
        }

        offset64[0] = 0;
        for (uint32_t i = 0; i < card; ++i) {
            bits[i]->adjustSize(0, nrows);
            offset64[i+1] = offset64[i] + bits[i]->getSerialSize();
        }
        if (ibis::gVerbose > 2) {
            ibis::util::logger lg;
            lg() << "relic[" << (col ? col->fullname() : "?.?")
                 << "]::ctor -- constructed an equality index with "
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
            << "Warning -- relic[" << (col ? col->fullname() : "?.?")
            << "]::ctor received an exception, cleaning up ...";
        clear();
        throw;
    }
} // construct an index from an integer array

/// Reconstruct from content of fileManager::storage.
/**
   The content of the file (following the 8-byte header) is
@code
   - nrows(uint32_t)       -- number of bits in each bit sequences
   - nobs (uint32_t)       -- number of bit sequences
   - card (uint32_t)       -- the number of distinct values, i.e., cardinality
   - (padding to ensure the next data element is on 8-byte boundary)
   - values (double[card]) -- the values as doubles
   - offset ([nobs+1])     -- the starting positions of the bit sequences (as
                              bit vectors)
   - bitvectors            -- the bitvectors one after another
@endcode
*/
ibis::relic::relic(const ibis::column* c, ibis::fileManager::storage* st,
                   size_t start)
    : ibis::index(c, st),
      vals(st, 8*((3 * sizeof(uint32_t) + start + 7)/8),
           8*((3 * sizeof(uint32_t) + start + 7)/8 +
              *(reinterpret_cast<uint32_t*>(st->begin() + start +
                                            2*sizeof(uint32_t))))) {
    try {
        nrows = *(reinterpret_cast<uint32_t*>(st->begin()+start));
        size_t pos = start + sizeof(uint32_t);
        const uint32_t nobs =
            *(reinterpret_cast<uint32_t*>(st->begin()+pos));
        pos += sizeof(uint32_t);
        const uint32_t card =
            *(reinterpret_cast<uint32_t*>(st->begin()+pos));
        pos = 8*((pos+sizeof(uint32_t)+7)/8)+sizeof(double)*card;
        int ierr = initOffsets(st, pos, nobs);
        if (ierr < 0) {
            clear();
            return;
        }

        initBitmaps(st);
        if (ibis::gVerbose > 2) {
            ibis::util::logger lg;
            lg() << "relic[" << (col ? col->fullname() : "?.?")
                 << "]::ctor -- intialized an equality index with "
                 << bits.size() << " bitmap" << (bits.size()>1?"s":"")
                 << " for " << nrows << " row" << (nrows>1?"s":"")
                 << " from a storage object @ " << st << " starting at "
                 << start;
            if (ibis::gVerbose > 6) {
                lg() << "\n";
                print(lg());
            }
        }
    }
    catch (...) {
        LOGGER(ibis::gVerbose > 1)
            << "Warning -- relic[" << (col ? col->fullname() : "?.?")
            << "]::ctor received an exception, cleaning up ...";
        clear();
        throw;
    }
} // constructor

/// Reconstruct index from keys and offsets.
ibis::relic::relic(const ibis::column* c, uint32_t nb, double *kvs,
                   int64_t *offs) :
    ibis::index(0), vals(kvs, nb) {
    col = c;
    initOffsets(offs, nb+1);
    if (c != 0)
        nrows = c->nRows();
} // constructor

/// Reconstruct index from keys and offsets.
ibis::relic::relic(const ibis::column* c, uint32_t nb, double *kvs,
                   int64_t *offs, uint32_t *bms) :
    ibis::index(0), vals(kvs, nb) {
    col = 0;
    initOffsets(offs, nb+1);
    if (c != 0)
        nrows = c->nRows();

    ibis::fileManager::storage *mystr =
        new ibis::fileManager::storage(reinterpret_cast<char*>(bms),
                                       static_cast<size_t>(offs[nb]*4));
    initBitmaps(mystr);
    if (ibis::gVerbose > 2) {
        ibis::util::logger lg;
        lg() << "relic[" << (col ? col->fullname() : "?.?")
             << "]::ctor -- intialized an equality index with "
             << bits.size() << " bitmap" << (bits.size()>1?"s":"")
             << " for " << nrows << " row" << (nrows>1?"s":"")
             << " from a storage object @ " << static_cast<void*>(bms);
        if (ibis::gVerbose > 6) {
            lg() << "\n";
            print(lg());
        }
    }
} // constructor

/// Reconstruct index from keys and offsets.
ibis::relic::relic(const ibis::column* c, uint32_t nb, double *kvs,
                   int64_t *offs, void *bms, FastBitReadBitmaps rd) :
    ibis::index(0), vals(kvs, nb) {
    col = c;
    initOffsets(offs, nb+1);
    initBitmaps(bms, rd);
    if (c != 0)
        nrows = c->nRows();
    if (ibis::gVerbose > 2) {
        ibis::util::logger lg;
        lg() << "relic[" << (col ? col->fullname() : "?.?")
             << "]::ctor -- intialized an equality index with "
             << bits.size() << " bitmap" << (bits.size()>1?"s":"")
             << " for " << nrows << " row" << (nrows>1?"s":"")
             << " from a storage object @ " << static_cast<void*>(bms);
        if (ibis::gVerbose > 6) {
            lg() << "\n";
            print(lg());
        }
    }
} // constructor

/// Copy constructor.
ibis::relic::relic(const ibis::relic &rhs) : ibis::index(rhs), vals(rhs.vals) {
    if (ibis::gVerbose > 2) {
        ibis::util::logger lg;
        lg() << "relic[" << (col ? col->fullname() : "?.?")
             << "]::ctor -- intialized an equality index with "
             << bits.size() << " bitmap" << (bits.size()>1?"s":"")
             << " for " << nrows << " row" << (nrows>1?"s":"");
        if (ibis::gVerbose > 6) {
            lg() << "\n";
            print(lg());
        }
    }
} // copy constructor

ibis::index* ibis::relic::dup() const {
    return new ibis::relic(*this);
} // ibis::relic::dup

/// Write the content of the index to the specified location.  The actual
/// index file name is determined by the function indexFileName.
int ibis::relic::write(const char* dt) const {
    if (vals.empty() || bits.empty() || nrows == 0) return -1;
    std::string evt = "relic";
    if (ibis::gVerbose > 0 && col != 0) {
        evt += '[';
        evt += col->fullname();
        evt += ']';
    }
    evt += "::write";
    if (vals.size() != bits.size()) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " expects vals.size(" << vals.size()
            << ") and bits.size(" << bits.size()
            << ") to be the same, but they are not";
        return -1;
    }

    std::string fnm;
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
    if (fname != 0 && *fname != 0 && fnm.compare(fname) == 0) {
        activate(); // read everything into memory
        fname = 0; // break the link with the file
    }
    if (fname != 0 || str != 0)
        activate(); // activate all bitvectors

    int fdes = UnixOpen(fnm.c_str(), OPEN_WRITENEW, OPEN_FILEMODE);
    if (fdes < 0) {
        ibis::fileManager::instance().flushFile(fnm.c_str());
        fdes = UnixOpen(fnm.c_str(), OPEN_WRITENEW, OPEN_FILEMODE);
        if (fdes < 0) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << evt << "failed to open \"" << fnm
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

    off_t ierr = 0;
    const uint32_t nobs = vals.size();
#ifdef FASTBIT_USE_LONG_OFFSETS
    const bool useoffset64 = true;
#else
    const bool useoffset64 = (8+getSerialSize() > 0x80000000UL);
#endif
    char header[] = "#IBIS\7\0\0";
    header[5] = (char)ibis::index::RELIC;
    header[6] = (char)(useoffset64 ? 8 : 4);
    ierr = UnixWrite(fdes, header, 8);
    if (ierr < 8) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt 
            << " failed to write the 8-byte header, ierr = " << ierr;
        return -3;
    }
    if (useoffset64)
        ierr = write64(fdes); // write the bulk of the index file
    else
        ierr = write32(fdes); // write the bulk of the index file
    if (ierr >= 0) {
#if defined(FASTBIT_SYNC_WRITE)
#if _POSIX_FSYNC+0 > 0
        (void) UnixFlush(fdes); // write to disk
#elif defined(_WIN32) && defined(_MSC_VER)
        (void) _commit(fdes);
#endif
#endif

        LOGGER(ierr >= 0 && ibis::gVerbose > 5)
            << evt << " wrote " << nobs << " bitmap" << (nobs>1?"s":"")
            << " to " << fnm;
    }
    return ierr;
} // ibis::relic::write

/// Write the content to a file already opened
int ibis::relic::write32(int fdes) const {
    if (vals.empty() || bits.empty() || nrows == 0)
        return -4;

    std::string evt = "relic";
    if (ibis::gVerbose > 0 && col != 0) {
        evt += '[';
        evt += col->fullname();
        evt += ']';
    }
    evt += "::write32";
    const uint32_t nobs = (vals.size()<=bits.size()?vals.size():bits.size());
    const off_t start = UnixSeek(fdes, 0, SEEK_CUR);
    if (start < 8) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " seek(" << fdes
            << ", 0, SEEK_CUR) is expected to return a value >= 8, but it is "
            << start;
        return -5;
    }

    off_t ierr;
    ierr  = UnixWrite(fdes, &nrows, sizeof(uint32_t));
    ierr += UnixWrite(fdes, &nobs,  sizeof(uint32_t));
    ierr += UnixWrite(fdes, &nobs,  sizeof(uint32_t));
    if (ierr < 12) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " expects to write 3 4-byte words to "
            << fdes << ", but the number of byte wrote is " << ierr;
        (void) UnixSeek(fdes, start, SEEK_SET);
        return -6;
    }

    offset64.clear();
    offset32.resize(nobs+1);
    offset32[0] = 8*((7+start+3*sizeof(uint32_t))/8);
    ierr = UnixSeek(fdes, offset32[0], SEEK_SET);
    if (ierr != offset32[0]) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " seek(" << fdes << ", " << offset32[0]
            << ", SEEK_SET) returned " << ierr;
        (void) UnixSeek(fdes, start, SEEK_SET);
        return -7;
    }
    ierr = UnixWrite(fdes, vals.begin(), sizeof(double)*nobs);
    if (ierr < static_cast<off_t>(sizeof(double)*nobs)) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " expected to write "
            << sizeof(double)*nobs << " bytes to file descriptor "
            << fdes << ", but actually wrote " << ierr;
        (void) UnixSeek(fdes, start, SEEK_SET);
        return -8;
    }

    offset32[0] += (sizeof(double)+sizeof(int32_t)) * nobs + sizeof(int32_t);
    ierr = UnixSeek(fdes, sizeof(int32_t)*(nobs+1), SEEK_CUR);
    if (ierr != offset32[0]) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " attempting to seek to " << offset32[0]
            << " file descriptor " << fdes << " returned " << ierr;
        (void) UnixSeek(fdes, start, SEEK_SET);
        return -9;
    }
    for (uint32_t i = 0; i < nobs; ++i) {
        if (bits[i]) {
            bits[i]->write(fdes);
        }
        offset32[i+1] = UnixSeek(fdes, 0, SEEK_CUR);
    }

    const off_t offpos = 8*((start+sizeof(uint32_t)*3+7)/8)+sizeof(double)*nobs;
    ierr = UnixSeek(fdes, offpos, SEEK_SET);
    if (ierr != offpos) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " seek(" << fdes << ", " << offpos
            << ", SEEK_SET) returned " << ierr;
        (void) UnixSeek(fdes, start, SEEK_SET);
        return -10;
    }
    ierr = UnixWrite(fdes, offset32.begin(), sizeof(int32_t)*(nobs+1));
    if (ierr < static_cast<off_t>(sizeof(int32_t)*(nobs+1))) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " expected to write "
            << sizeof(int32_t)*(nobs+1) << " bytes to file descriptor "
            << fdes << ", but actually wrote " << ierr;
        (void) UnixSeek(fdes, start, SEEK_SET);
        return -11;
    }
    ierr = UnixSeek(fdes, offset32[nobs], SEEK_SET); // move to the end
    return (ierr == offset32[nobs] ? 0 : -12);
} // ibis::relic::write32

/// Write the content to a file already opened
int ibis::relic::write64(int fdes) const {
    if (vals.empty() || bits.empty() || nrows == 0)
        return -4;

    std::string evt = "relic";
    if (ibis::gVerbose > 0 && col != 0) {
        evt += '[';
        evt += col->fullname();
        evt += ']';
    }
    evt += "::write64";
    const uint32_t nobs = (vals.size()<=bits.size()?vals.size():bits.size());
    const off_t start = UnixSeek(fdes, 0, SEEK_CUR);
    if (start < 8) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " seek(" << fdes
            << ", 0, SEEK_CUR) is expected to return a value >= 8, but it is "
            << start;
        return -5;
    }

    off_t ierr;
    ierr  = UnixWrite(fdes, &nrows, sizeof(uint32_t));
    ierr += UnixWrite(fdes, &nobs,  sizeof(uint32_t));
    ierr += UnixWrite(fdes, &nobs,  sizeof(uint32_t));
    if (ierr < 12) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " expects to write 3 4-byte words to "
            << fdes << ", but the number of byte wrote is " << ierr;
        (void) UnixSeek(fdes, start, SEEK_SET);
        return -6;
    }

    offset32.clear();
    offset64.resize(nobs+1);
    offset64[0] = 8*((7+start+3*sizeof(uint32_t))/8);
    ierr = UnixSeek(fdes, offset64[0], SEEK_SET);
    if (ierr != offset64[0]) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " seek(" << fdes << ", " << offset64[0]
            << ", SEEK_SET) returned " << ierr;
        (void) UnixSeek(fdes, start, SEEK_SET);
        return -7;
    }
    ierr = ibis::util::write(fdes, vals.begin(), sizeof(double)*nobs);
    if (ierr < static_cast<off_t>(sizeof(double)*nobs)) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " expected to write "
            << sizeof(double)*nobs << " bytes to file descriptor " << fdes
            << ", but actually wrote " << ierr;
        (void) UnixSeek(fdes, start, SEEK_SET);
        return -8;
    }

    offset64[0] += (sizeof(double)+sizeof(int64_t)) * nobs + sizeof(int64_t);
    ierr = UnixSeek(fdes, sizeof(int64_t)*(nobs+1), SEEK_CUR);
    if (ierr != offset64[0]) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " attempting to seek to " << offset64[0]
            << " file descriptor " << fdes << " returned " << ierr;
        (void) UnixSeek(fdes, start, SEEK_SET);
        return -9;
    }
    for (uint32_t i = 0; i < nobs; ++i) {
        if (bits[i]) {
            bits[i]->write(fdes);
        }
        offset64[i+1] = UnixSeek(fdes, 0, SEEK_CUR);
    }

    const off_t offpos = 8*((start+sizeof(uint32_t)*3+7)/8)+sizeof(double)*nobs;
    ierr = UnixSeek(fdes, offpos, SEEK_SET);
    if (ierr != offpos) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " seek(" << fdes << ", " << offpos
            << ", SEEK_SET) returned " << ierr;
        (void) UnixSeek(fdes, start, SEEK_SET);
        return -10;
    }
    ierr = ibis::util::write(fdes, offset64.begin(), sizeof(int64_t)*(nobs+1));
    if (ierr < static_cast<off_t>(sizeof(int64_t)*(nobs+1))) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " expected to write "
            << sizeof(int64_t)*(nobs+1) << " bytes to file descriptor "
            << fdes << ", but actually wrote " << ierr;
        (void) UnixSeek(fdes, start, SEEK_SET);
        return -11;
    }
    ierr = UnixSeek(fdes, offset64[nobs], SEEK_SET); // move to the end
    return (ierr == offset64[nobs] ? 0 : -12);
} // ibis::relic::write64

int ibis::relic::write(ibis::array_t<double> &kvs,
                       ibis::array_t<int64_t> &starts,
                       ibis::array_t<uint32_t> &bitmaps) const {
    const uint32_t nobs = (vals.size()<=bits.size()?vals.size():bits.size());
    if (nobs == 0) {
        kvs.resize(0);
        starts.resize(0);
        bitmaps.resize(0);
        return 0;
    }

    kvs.copy(vals);
    starts.resize(nobs+1);
    starts[0] = 0;
    for (unsigned j = 0; j < nobs; ++ j) { // iterate over bitmaps
        if (bits[j] != 0) {
            ibis::array_t<ibis::bitvector::word_t> tmp;
            bits[j]->write(tmp);
            bitmaps.insert(bitmaps.end(), tmp.begin(), tmp.end());
        }
        starts[j+1] = bitmaps.size();
    }
    return 0;
} // ibis::relic::write

void ibis::relic::serialSizes(uint64_t &wkeys, uint64_t &woffsets,
                              uint64_t &wbitmaps) const {
    const uint32_t nobs = (vals.size()<=bits.size()?vals.size():bits.size());
    if (nobs == 0) {
        wkeys = 0;
        woffsets = 0;
        wbitmaps = 0;
    }
    else {
        wkeys = nobs;
        woffsets = nobs + 1;
        wbitmaps = 0;
        for (unsigned j = 0; j < nobs; ++ j) {
            if (bits[j] != 0)
                wbitmaps += bits[j]->getSerialSize();
        }
        wbitmaps /= 4;
    }
} // ibis::relic::serialSizes

/// Read the index contained from the speficied location.
int ibis::relic::read(const char* f) {
    std::string fnm;
    indexFileName(fnm, f);

    int fdes = UnixOpen(fnm.c_str(), OPEN_READONLY);
    if (fdes < 0) return -1;

    char header[8];
    IBIS_BLOCK_GUARD(UnixClose, fdes);
#if defined(_WIN32) && defined(_MSC_VER)
    (void)_setmode(fdes, _O_BINARY);
#endif
    if (8 != UnixRead(fdes, static_cast<void*>(header), 8)) {
        return -2;
    }

    if (false == (header[0] == '#' && header[1] == 'I' &&
                  header[2] == 'B' && header[3] == 'I' &&
                  header[4] == 'S' &&
                  (header[5] == RELIC || header[5] == BYLT ||
                   header[5] == FADE || header[5] == SBIAD ||
                   header[5] == SAPID || header[5] == FUZZ ||
                   header[5] == SLICE || header[5] == ZONA) &&
                  (header[6] == 8 || header[6] == 4) &&
                  header[7] == static_cast<char>(0))) {
        if (ibis::gVerbose > 0) {
            ibis::util::logger lg;
            lg() << "Warning -- relic[" << (col ? col->fullname() : "?.?")
                 << "]::read the header from " << fnm << " (";
            printHeader(lg(), header);
            lg() << ") does not contain the expected values";
        }
        return -3;
    }

    uint32_t dim[3];
    size_t begin, end;
    clear(); // clear the current content
    fname = ibis::util::strnewdup(fnm.c_str());

    int ierr = UnixRead(fdes, static_cast<void*>(dim), 3*sizeof(uint32_t));
    if (ierr < static_cast<int>(3*sizeof(uint32_t))) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- relic[" << (col ? col->fullname() : "?.?")
            << "]::read failed to read the size inforamtion from index file "
            << fnm;
        return -4;
    }

    nrows = dim[0];
    // read vals
    begin = 8*((3*sizeof(uint32_t) + 15) / 8);
    end = begin + dim[2] * sizeof(double);
    {
        // try to use memory map to enable reading only the needed values
        array_t<double> dbl(fname, fdes, begin, end);
        vals.swap(dbl);
    }
    // read the offsets
    begin = end;
    end += header[6] * (dim[1] + 1);
    ierr = initOffsets(fdes, header[6], begin, dim[1]);
    ibis::fileManager::instance().recordPages(0, end);
#if DEBUG+0 > 0 || _DEBUG+0 > 0
    if (ibis::gVerbose > 5) {
        unsigned nprt = (ibis::gVerbose < 30 ? (1 << ibis::gVerbose) : dim[1]);
        if (nprt > dim[1])
            nprt = dim[1];
        ibis::util::logger lg(4);
        lg() << "DEBUG -- relic[" << (col ? col->fullname() : "?.?")
             << "]::read(" << f << ") got nobs = " << dim[1]
             << ", card = " << dim[2]
             << ", the offsets of the bit vectors are\n";
        if (header[6] == 8) {
            for (unsigned i = 0; i < nprt; ++ i)
                lg() << offset64[i] << " ";
        }
        else {
            for (unsigned i = 0; i < nprt; ++ i)
                lg() << offset32[i] << " ";
        }
        if (nprt < dim[1])
            lg() << "... (skipping " << dim[1]-nprt << ") ... ";
        if (header[6] == 8)
            lg() << offset64[dim[1]];
        else
            lg() << offset32[dim[1]];
    }
#endif

    initBitmaps(fdes);
    LOGGER(ibis::gVerbose > 3)
        << "relic[" << (col ? col->fullname() : "?.?")
        << "]::read finished reading the header from " << fnm;
    return 0;
} // ibis::relic::read

/// Reconstruct an index from a piece of consecutive memory.
int ibis::relic::read(ibis::fileManager::storage* st) {
    if (st == 0) return -1;
    ibis::index::clear();

    if (st->begin()[5] != RELIC && st->begin()[5] != BYLT &&
        st->begin()[5] != FADE && st->begin()[5] != SBIAD &&
        st->begin()[5] != SAPID && st->begin()[5] != FUZZ &&
        st->begin()[5] != SLICE && st->begin()[5] != ZONA)
        return -3;

    nrows = *(reinterpret_cast<uint32_t*>(st->begin()+8));
    uint32_t pos = 8 + sizeof(uint32_t);
    const uint32_t nobs = *(reinterpret_cast<uint32_t*>(st->begin()+pos));
    pos += sizeof(uint32_t);
    const uint32_t card = *(reinterpret_cast<uint32_t*>(st->begin()+pos));
    pos += sizeof(uint32_t) + 7;
    {
        array_t<double> dbl(st, 8*(pos/8), 8*(pos/8 + card));
        vals.swap(dbl);
    }
    int ierr = initOffsets(st, 8*(pos/8) + sizeof(double)*card, nobs);
    if (ierr < 0)
        return ierr;

    initBitmaps(st);
    LOGGER(ibis::gVerbose > 3)
        << "relic[" << (col ? col->fullname() : "?.?")
        << "]::read finished reading the header from a storage object @ " << st;
    return 0;
} // ibis::relic::read

void ibis::relic::clear() {
    vals.clear();
    ibis::index::clear();
} // ibis::relic::clear

/// Construct a new index in memory.  This function generates the basic
/// bitmap index that contains one bitmap per distinct value.  The string f
/// can be the name of the index file (the corresponding data file is
/// assumed to be without the '.idx' suffix), the name of the data file, or
/// the directory contain the data file.  The bitmaps may be optionally
/// uncompressed based on the directives in the indexing specification
/// returned by ibis::column::indexSpec.
void ibis::relic::construct(const char* f) {
    if (col == 0) return;

    VMap bmap;
    try {
        mapValues(f, bmap);
    }
    catch (...) { // need to clean up bmap
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- relic[" << col->fullname()
            << "]::construct reclaiming storage allocated to bitvectors ("
            << bmap.size() << ")";

        for (VMap::iterator it = bmap.begin(); it != bmap.end(); ++ it)
            delete (*it).second;
        bmap.clear();
        ibis::fileManager::instance().signalMemoryAvailable();
        throw; // rethrow the exception
    }
    if (bmap.empty()) return; // bmap is empty

    uint32_t i;
    // copy the pointer in VMap into two linear structures
    const uint32_t nobs = bmap.size();
    bits.resize(nobs);
    vals.resize(nobs);
    VMap::const_iterator it;
    for (it = bmap.begin(); nrows == 0 && it != bmap.end(); ++ it)
        if ((*it).second)
            nrows = (*it).second->size();
    for (i = 0, it = bmap.begin(); i < nobs; ++ it, ++ i) {
        vals[i] = (*it).first;
        bits[i] = (*it).second;
    }
    optionalUnpack(bits, col->indexSpec());

    // write out the current content
    if (ibis::gVerbose > 6) {
        ibis::util::logger lg;
        print(lg());
    }
} // ibis::relic::construct

/// Construct an index from in-memory values.  The type @c E is intended to
/// be element types supported in column.h.  It only generates the basic
/// equality encoded index.  Only part of the indexing option that is
/// checked in this function is the directives for compression.
template <typename E>
void ibis::relic::construct(const array_t<E>& arr) {
    VMap bmap;
    nrows = arr.size();
    try {
        mapValues(arr, bmap);
    }
    catch (...) { // need to clean up bmap
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- relic[" << (col ? col->fullname() : "?.?")
            << "]::construct<" << typeid(E).name()
            << "> reclaiming storage allocated to bitvectors ("
            << bmap.size() << ")";

        for (VMap::iterator it = bmap.begin(); it != bmap.end(); ++ it)
            delete (*it).second;
        bmap.clear();
        ibis::fileManager::instance().signalMemoryAvailable();
        throw;
    }
    if (bmap.empty()) return; // bmap is empty

    // copy the pointer in VMap into two linear structures
    const uint32_t nobs = bmap.size();
    bits.resize(nobs);
    vals.resize(nobs);
    uint32_t i;
    VMap::const_iterator it;
    for (i = 0, it = bmap.begin(); i < nobs; ++it, ++i) {
        vals[i] = (*it).first;
        bits[i] = (*it).second;
    }
    optionalUnpack(bits, (col ? col->indexSpec() : ""));

    // write out the current content
    if (ibis::gVerbose > 2) {
        ibis::util::logger lg;
        lg() << "relic[" << (col ? col->fullname() : "?.?")
             << "]::construct<" << typeid(E).name() << "[" << arr.size()
             << "]> -- built an equality index with "
             << bits.size() << " bitmap" << (bits.size()>1?"s":"")
             << " for " << nrows << " row" << (nrows>1?"s":"");
        if (ibis::gVerbose > 6) {
            lg() << "\n";
            print(lg());
        }
    }
} // ibis::relic::construct

// explicit instantiation of construct function
template void ibis::relic::construct(const array_t<signed char>& arr);
template void ibis::relic::construct(const array_t<unsigned char>& arr);
template void ibis::relic::construct(const array_t<int16_t>& arr);
template void ibis::relic::construct(const array_t<uint16_t>& arr);
template void ibis::relic::construct(const array_t<int32_t>& arr);
template void ibis::relic::construct(const array_t<uint32_t>& arr);
template void ibis::relic::construct(const array_t<int64_t>& arr);
template void ibis::relic::construct(const array_t<uint64_t>& arr);
template void ibis::relic::construct(const array_t<float>& arr);
template void ibis::relic::construct(const array_t<double>& arr);

// a simple function to test the speed of the bitvector operations
void ibis::relic::speedTest(std::ostream& out) const {
    if (nrows == 0) return;
    uint32_t nloops = 1000000000 / nrows;
    if (nloops < 2) nloops = 2;
    ibis::horometer timer;

    try {
        activate(); // activate all bitvectors
    }
    catch (const std::exception& e) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- relic::speedTest received a standard exception - "
            << e.what();
        return;
    }
    catch (const char* s) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- relic::speedTest received a string exception - "
            << s;
        return;
    }
    catch (...) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- relic::speedTest received a unexpected exception";
        return;
    }
    bool crossproduct = false;
    if (col != 0) {
        std::string which = col->fullname();
        which += ".measureCrossProduct";
        crossproduct = ibis::gParameters().isTrue(which.c_str());
    }
    if (crossproduct) {
        out << "relic::speedTest -- testing the speed of cross "
            "product operation\n# bits, # 1s, # 1s, # bytes, "
            "# bytes, clustering factor, result 1s, result "
            "bytes, wall time";
        nloops = 2;
    }
    else {
        out << "relic::speedTest -- testing the speed of operator "
            "|\n# bits, # 1s, # 1s, # bytes, # bytes, "
            "clustering factor, result 1s, result bytes, "
            "wall time";
    }

    for (unsigned i = 1; i < bits.size(); ++i) {
        if (bits[i-1] == 0 || bits[i] == 0)
            continue;
        int64_t ocnt, osize; // the size of the output of operation
        ibis::bitvector* tmp;
        try {
            tmp = *(bits[i-1]) | *(bits[i]);
            osize = tmp->bytes();
            ocnt = tmp->cnt();
            delete tmp;
        }
        catch (const std::exception& e) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- relic::speedTest received a standard "
                "exception while calling operator | (i=" << i << ") - "
                << e.what();
            continue;
        }
        catch (const char* s) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- relic::speedTest received a string "
                "exception while calling operator | (i=" << i << ") - "
                << s;
            continue;
        }
        catch (...) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- relic::speedTest received a unexpected "
                "exception while calling operator | (i=" << i << ')';
            continue;
        }

        try {
            const double cf = ibis::bitvector::clusteringFactor
                (bits[i]->size(), bits[i]->cnt(), bits[i]->bytes());
            timer.start();
            if (crossproduct) {
                for (uint32_t j=0; j<nloops; ++j) {
                    ibis::bitvector64 t64;
                    ibis::util::outerProduct(*(bits[i-1]), *(bits[i]), t64);
                    osize = t64.bytes();
                    ocnt = t64.cnt();
                }
            }
            else {
                for (uint32_t j=0; j<nloops; ++j) {
                    tmp = *(bits[i-1]) | *(bits[i]);
                    delete tmp;
                }
            }
            timer.stop();
            ibis::util::ioLock lock;
            out << bits[i]->size() << ", "
                << bits[i-1]->cnt() << ", " << bits[i]->cnt() << ", "
                << bits[i-1]->bytes() << ", " << bits[i]->bytes() << ", "
                << cf << ", " << ocnt << ", " << osize << ", "
                << timer.realTime() / nloops << "\n";
        }
        catch (...) {
        }
    }
} // ibis::relic::speedTest

/// The printing function.
void ibis::relic::print(std::ostream& out) const {
    if (vals.size() != bits.size() || bits.empty())
        return;

    out << "the basic bitmap index for " << (col ? col->fullname() : "?.?")
        << " contains " << bits.size() << " bitvectors for "
        << nrows << " objects";
    const uint32_t nobs = bits.size();
    uint32_t skip = 0;
    if (ibis::gVerbose <= 0) {
        skip = nobs;
    }
    else if ((nobs >> (2*ibis::gVerbose)) > 2) {
        skip = static_cast<uint32_t>
            (ibis::util::compactValue
             (static_cast<double>(nobs >> (1+2*ibis::gVerbose)),
              static_cast<double>(nobs >> (2*ibis::gVerbose))));
        if (skip < 1)
            skip = 1;
    }
    if (skip < 1)
        skip = 1;
    if (skip > 1) {
        out << " (printing 1 out of every " << skip << ")";
    }
    out << "\n";

    for (uint32_t i=0; i<nobs; i += skip) {
        if (bits[i]) {
            out << i << ":\t";
            out.precision(12);
            out << vals[i] << "\t" << bits[i]->cnt()
                << "\t" << bits[i]->bytes() << "\n";
        }
        else if (ibis::gVerbose > 7) {
            out << i << ":\t";
            out.precision(12);
            out << vals[i] << " ... \n";
        }
    }
    if ((nobs-1) % skip) {
        if (bits[nobs-1]) {
            out << nobs-1 << ":\t";
            out << vals[nobs-1] << "\t" << bits[nobs-1]->cnt()
                << "\t" << bits[nobs-1]->bytes() << "\n";
        }
        else if (ibis::gVerbose > 7) {
            out << nobs-1 << ":\t";
            out << vals[nobs-1] << " ... \n";
        }
    }
    out << "\n";
} // ibis::relic::print

/// Convert the bitvector mask into bin numbers.
ibis::array_t<uint32_t>*
ibis::relic::keys(const ibis::bitvector& mask) const {
    if (mask.cnt() == 0) // nothing to do
        return 0;

    ibis::bitvector* tmp = 0;
    uint32_t nobs = bits.size();
    std::map<uint32_t, uint32_t> ii;

    activate(); // need all bitvectors to be in memory
    for (uint32_t i = 0; i < nobs; ++i) { // loop to generate ii
        if (bits[i]) {
            if (bits[i]->size() == mask.size()) {
                uint32_t nind = 0;
                tmp = mask & *(bits[i]);
                ibis::bitvector::indexSet is = tmp->firstIndexSet();
                const ibis::bitvector::word_t *iix = is.indices();
                nind = is.nIndices();
                while (nind > 0) {
                    if (is.isRange()) {
                        for (uint32_t j = *iix; j < iix[1]; ++j)
                            ii[j] = static_cast<uint32_t>(vals[i]);
                    }
                    else if (nind > 0) {
                        for  (uint32_t j = 0; j < nind; ++j)
                            ii[iix[j]] = static_cast<uint32_t>(vals[i]);
                    }

                    ++ is;
                    nind =is.nIndices();
                }
                delete tmp;
            }
            else {
                LOGGER(ibis::gVerbose > 2) 
                    << "relic::keys -- bits[" << i << "]->size()="
                    << bits[i]->size() << ", mask.size()=" << mask.size();
            }
        }
        else {
            LOGGER(ibis::gVerbose > 4)
                << "relic::keys -- bits[" << i << "] can not be activated";
        }
    }

    array_t<uint32_t>* ret = new array_t<uint32_t>(ii.size());
    std::map<uint32_t, uint32_t>::const_iterator it = ii.begin();
    for (uint32_t i = 0; i < ii.size(); ++i) {
        (*ret)[i] = (*it).second;
        ++ it;
    }
    LOGGER(ibis::gVerbose > 0 && ret->empty())
        << "Warning -- relic::keys failed to compute the keys, most "
        "likely because the index has changed";
    return ret;
} // ibis::relic::keys

/// Append a list of integers.  The integers are treated as bin numbers.
/// This function is primarily used by ibis::category::append().
long ibis::relic::append(const array_t<uint32_t>& ind) {
    if (ind.empty()) return 0; // nothing to do
    uint32_t i, nobs = bits.size();
    activate(); // need all bitvectors to be in memory
    for (i = 0; i < ind.size(); ++ i, ++ nrows) {
        const uint32_t j = ind[i];
        if (j >= nobs) { // extend the number of bitvectors
            for (uint32_t k = nobs; k <= j; ++ k) {
                bits.push_back(new ibis::bitvector);
                vals.push_back(k);
            }
            nobs = bits.size();
        }
        bits[j]->setBit(nrows, 1);
    }

    uint32_t nset = 0;
    for (i = 0; i < nobs; ++i) {
        bits[i]->adjustSize(0, nrows);
        nset += bits[i]->cnt();
    }
    LOGGER(ibis::gVerbose > 0 && nset != nrows)
        << "Warning -- relic::append new index contains " << nset
        << " bits, but it is expected to be " << nrows;
    return ind.size();
} // ibis::relic::append

/// Create an index based on data in df and append the result to the index
/// in dt.
long ibis::relic::append(const char* dt, const char* df, uint32_t nnew) {
    if (col == 0 || dt == 0 || *dt == 0 || df == 0 || *df == 0 || nnew == 0)
        return -1L;    
    const uint32_t nold =
        (std::strcmp(dt, col->partition()->currentDataDir()) == 0 ?
         col->partition()->nRows()-nnew : nrows);
    if (nrows != nold) { // recreate the new index
#ifdef APPEND_UPDATE_INDEXES
        LOGGER(ibis::gVerbose > 3)
            << "relic::append to build a new index for " << col->name()
            << " using data in " << dt;
        clear(); // clear the current content
        construct(dt);
#endif
        return nnew;
    }

    std::string fnm;
    indexFileName(fnm, df);
    ibis::relic* bin0=0;
    ibis::fileManager::storage* st0=0;
    int ierr = ibis::fileManager::instance().getFile(fnm.c_str(), &st0);
    if (ierr == 0 && st0 != 0) {
        const char* header = st0->begin();
        if (header[0] == '#' && header[1] == 'I' && header[2] == 'B' &&
            header[3] == 'I' && header[4] == 'S' &&
            header[5] == ibis::index::RELIC &&
            (header[6] == 8 || header[6] == 4) &&
            header[7] == static_cast<char>(0)) {
            bin0 = new ibis::relic(col, st0);
        }
        else {
            LOGGER(ibis::gVerbose > 5)
                << "Warning -- relic::append found file \"" << fnm
                << "\" to have a unexecpted header -- it will be removed";
            ibis::fileManager::instance().flushFile(fnm.c_str());
            remove(fnm.c_str());
        }
    }
    if (bin0 == 0) {
        if (col->type() == ibis::TEXT) {
            fnm.erase(fnm.size()-3);
            fnm += "int";
            if (ibis::util::getFileSize(fnm.c_str()) > 0)
                bin0 = new ibis::relic(col, fnm.c_str());
            else {
                LOGGER(ibis::gVerbose > 0)
                    << "Warning -- relic::append can not find file \"" << fnm
                    << '"';
                ierr = -2;
                return ierr;
            }
        }
        else {
            bin0 = new ibis::relic(col, df);
        }
    }

    if (bin0) {
        ierr = append(*bin0);
        delete bin0;
        if (ierr == 0) {
            //write(dt); // write out the new content
            return nnew;
        }
        else {
            return ierr;
        }
    }
    else {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- relic::append failed to generate index with "
            "data from " << df;
        return -6;
    }
} // ibis::relic::append

/// Append tail to this index.  This function first convert *this into a
/// map then back into the linear data structure.
long ibis::relic::append(const ibis::relic& tail) {
    if (tail.col != col) return -1;
    if (tail.bits.empty()) return -3;

    activate(); // need all bitvectors;
    tail.activate();

    unsigned i;
    uint32_t nobs = bits.size();
    const uint32_t n0 = nrows;
    std::map<double, ibis::bitvector*> bmap;
    std::map<double, ibis::bitvector*>::iterator it;
    // copy *this into bmap, make another copy just in case of the current
    // bitmap index is built on top of file maps
    for (i = 0; i < nobs; ++i) {
        ibis::bitvector* tmp = new ibis::bitvector();
        if (bits[i] != 0) {
            tmp->copy(*(bits[i]));
            bmap[vals[i]] = tmp;
        }
        else {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- relic::append -- bits[" << i << "] (<==> "
                << vals[i] << ") is nil, assume it is no longer needed";
            delete tmp;
        }
    }
    clear(); // clear the current content

    // combine the two sets of bitmaps
    for (i = 0; i < tail.vals.size(); ++i) {
        if (tail.bits[i] != 0 && tail.bits[i]->size() > 0) {
            it = bmap.find(tail.vals[i]);
            if (it != bmap.end()) {
                (*it).second->operator+=(*(tail.bits[i]));
            }
            else if (n0 > 0) {
                ibis::bitvector* tmp = new ibis::bitvector();
                tmp->set(0, n0);
                tmp->operator+=(*(tail.bits[i]));
                bmap[tail.vals[i]] = tmp;
            }
            else {
                ibis::bitvector* tmp = new ibis::bitvector();
                tmp->copy(*(tail.bits[i]));
                bmap[tail.vals[i]] = tmp;
            }
        }
    }

    // nobs --> count only those values that actually appeared
    nobs = 0;
    uint32_t nset = 0;
    const uint32_t totbits = n0 + tail.nrows;
    for (it = bmap.begin(); it != bmap.end(); ++it) {
        (*it).second->adjustSize(0, totbits);
        nobs += ((*it).second->cnt() > 0);
        nset += (*it).second->cnt();
        LOGGER(ibis::gVerbose > 18)
            << "relic::append -- value "<< (*it).first << " appeared "
            << (*it).second->cnt() << " times out of " << totbits;
    }
    LOGGER(nset != totbits && ibis::gVerbose > 0)
        << "Warning -- relic::append created a new index for " << nset
        << " objects (!= bitmap length " << totbits << ")";
    nrows = totbits;
    // convert from bmap to the linear structure
    bits.resize(nobs);
    vals.resize(nobs);
    for (i = 0, it = bmap.begin(); it != bmap.end(); ++it) {
        if ((*it).second->cnt() > 0) {
            vals[i] = (*it).first;
            bits[i] = (*it).second;
            ++ i;
        }
        else {
            delete (*it).second;
        }
    }

    if (ibis::gVerbose > 10) {
        ibis::util::logger lg;
        lg() << "\nNew combined index (append an index for " << tail.nrows
             << " objects to an index for " << n0 << " events\n" ;
        print(lg());
    }
    return 0;
} // ibis::relic::append

/// Find the smallest i such that vals[i] > val.
uint32_t ibis::relic::locate(const double& val) const {
    // check the extreme cases -- use negative tests to capture abnormal
    // numbers
    const uint32_t nval = vals.size();
    if (nval <= 0) return 0;
    if (! (val >= vals[0])) {
        return 0;
    }
    else if (! (val < vals[nval-1])) {
        if (vals[nval-1] < DBL_MAX)
            return nval;
        else
            return nval - 1;
    }

    // the normal cases -- two different search strategies
    if (nval >= 8) { // binary search
        uint32_t i0 = 0, i1 = nval, it = nval/2;
        while (i0 < it) { // vals[i1] >= val
            if (val < vals[it])
                i1 = it;
            else
                i0 = it;
            it = (i0 + i1) / 2;
        }
#if DEBUG+0 > 1 || _DEBUG+0 > 1
        ibis::util::logMessage("locate", "%g in [%g, %g) ==> %lu", val,
                               vals[i0], vals[i1],
                               static_cast<long unsigned>(i1));
#endif
        return i1;
    }
    else { // do linear search
        for (uint32_t i = 0; i < nval; ++i) {
            if (val < vals[i]) {
#if DEBUG+0 > 1 || _DEBUG+0 > 1
                if (i > 0)
                    ibis::util::logMessage("locate", "%g in [%g, %g) ==> %lu",
                                           val, vals[i-1], vals[i],
                                           static_cast<long unsigned>(i));
                else
                    ibis::util::logMessage("locate", "%g in (..., %g) ==> 0",
                                           val, vals[i]);
#endif
                return i;
            }
        }
    }
    return vals.size();
} // ibis::relic::locate

/// Locate the bitmaps covered by the range expression.  Bitmaps hit0
/// (inclusive) through hit1 (execlusive) correspond to values satisfy the
/// range expression expr.
void ibis::relic::locate(const ibis::qContinuousRange& expr, uint32_t& hit0,
                         uint32_t& hit1) const {
    const uint32_t nval = vals.size();
    uint32_t bin0 = (expr.leftOperator()!=ibis::qExpr::OP_UNDEFINED) ?
        locate(expr.leftBound()) : 0;
    uint32_t bin1 = (expr.rightOperator()!=ibis::qExpr::OP_UNDEFINED) ?
        locate(expr.rightBound()) : 0;
    switch (expr.leftOperator()) {
    case ibis::qExpr::OP_LT:
        hit0 = bin0;
        switch (expr.rightOperator()) {
        default:
        case ibis::qExpr::OP_UNDEFINED:
            hit1 = nval;
            break;
        case ibis::qExpr::OP_LT:
            if (bin1 > 0)
                hit1 = bin1 - (expr.rightBound() == vals[bin1-1]);
            else
                hit1 = 0;
            break;
        case ibis::qExpr::OP_LE:
            hit1 = bin1;
            break;
        case ibis::qExpr::OP_GT:
            hit1 = nval;
            if (expr.rightBound() > expr.leftBound())
                hit0 = bin1;
            break;
        case ibis::qExpr::OP_GE:
            hit1 = nval;
            if (expr.rightBound() > expr.leftBound()) {
                if (bin1 > 0)
                    hit0 = bin1 - (expr.rightBound() == vals[bin1-1]);
                else
                    hit0 = 0;
            }
            break;
        case ibis::qExpr::OP_EQ:
            if (expr.rightBound() < expr.leftBound()) {
                if (bin1 > nval || bin1 == 0) {
                    hit0 = 0;
                    hit1 = 0;
                }
                else if (expr.rightBound() == vals[bin1-1]) {
                    hit0 = bin1 - 1;
                    hit1 = bin1;
                }
                else {
                    hit0 = 0;
                    hit1 = 0;
                }
            }
            else {
                hit0 = 0;
                hit1 = 0;
            }
            break;
        } // switch (expr.rightOperator())
        break; // case ibis::qExpr::OP_LT
    case ibis::qExpr::OP_LE:
        if (bin0 > 0)
            hit0 = bin0 - (expr.leftBound() == vals[bin0-1]);
        else
            hit0 = 0;
        switch (expr.rightOperator()) {
        default:
        case ibis::qExpr::OP_UNDEFINED:
            hit1 = nval;
            break;
        case ibis::qExpr::OP_LT:
            if (bin1 > 0)
                hit1 = bin1 - (expr.rightBound() == vals[bin1-1]);
            else
                hit1 = 0;
            break;
        case ibis::qExpr::OP_LE:
            hit1 = bin1;
            break;
        case ibis::qExpr::OP_GT:
            hit1 = nval;
            if (expr.rightBound() > expr.leftBound())
                hit0 = bin1;
            break;
        case ibis::qExpr::OP_GE:
            hit1 = nval;
            if (expr.rightBound() > expr.leftBound()) {
                if (bin1 > 0)
                    hit0 = bin1 - (expr.rightBound() == vals[bin1-1]);
                else
                    hit0 = 0;
            }
            break;
        case ibis::qExpr::OP_EQ:
            if (expr.rightBound() <= expr.leftBound()) {
                if (bin1 > nval || bin1 == 0) {
                    hit0 = 0;
                    hit1 = 0;
                }
                else if (expr.rightBound() == vals[bin1-1]) {
                    hit0 = bin1 - 1;
                    hit1 = bin1;
                }
                else {
                    hit0 = 0;
                    hit1 = 0;
                }
            }
            else {
                hit0 = 0;
                hit1 = 0;
            }
            break;
        } // switch (expr.rightOperator())
        break; // case ibis::qExpr::OP_LE
    case ibis::qExpr::OP_GT:
        if (bin0 > 0)
            hit1 = bin0 - (expr.leftBound() == vals[bin0-1]);
        else
            hit1 = 0;
        switch (expr.rightOperator()) {
        default:
        case ibis::qExpr::OP_UNDEFINED:
            hit0 = 0;
            break;
        case ibis::qExpr::OP_LT:
            hit0 = 0;
            if (expr.rightBound() < expr.leftBound()) {
                if (bin1 > 0)
                    hit1 = bin1 - (expr.rightBound() == vals[bin1-1]);
                else
                    hit1 = 0;
            }
            break;
        case ibis::qExpr::OP_LE:
            hit0 = 0;
            if (expr.rightBound() < expr.leftBound())
                hit1 = bin1 ;
            break;
        case ibis::qExpr::OP_GT:
            hit0 = bin1;
            break;
        case ibis::qExpr::OP_GE:
            if (bin1 > 0)
                hit0 = bin1 - (expr.rightBound() == vals[bin1-1]);
            else
                hit0 = 0;
            break;
        case ibis::qExpr::OP_EQ:
            if (expr.rightBound() < expr.leftBound()) {
                if (bin1 > nval || bin1 == 0) {
                    hit0 = 0;
                    hit1 = 0;
                }
                else if (expr.rightBound() == vals[bin1-1]) {
                    hit0 = bin1 - 1;
                    hit1 = bin1;
                }
                else {
                    hit0 = 0;
                    hit1 = 0;
                }
            }
            else {
                hit0 = 0;
                hit1 = 0;
            }
            break;
        } // switch (expr.rightOperator())
        break; // case ibis::qExpr::OP_GT
    case ibis::qExpr::OP_GE:
        hit1 = bin0;
        switch (expr.rightOperator()) {
        default:
        case ibis::qExpr::OP_UNDEFINED:
            hit0 = 0;
            break;
        case ibis::qExpr::OP_LT:
            hit0 = 0;
            if (expr.rightBound() <= expr.leftBound()) {
                if (bin1 > 0)
                    hit1 = bin1 - (expr.rightBound() == vals[bin1-1]);
                else
                    hit1 = 0;
            }
            break;
        case ibis::qExpr::OP_LE:
            hit0 = 0;
            if (expr.rightBound() < expr.leftBound())
                hit1 = bin1;
            break;
        case ibis::qExpr::OP_GT:
            hit0 = bin1;
            break;
        case ibis::qExpr::OP_GE:
            if (bin1 > 0)
                hit0 = bin1 - (expr.rightBound() == vals[bin1-1]);
            else
                hit0 = 0;
            break;
        case ibis::qExpr::OP_EQ:
            if (expr.rightBound() <= expr.leftBound()) {
                if (bin1 > nval || bin1 == 0) {
                    hit0 = 0;
                    hit1 = 0;
                }
                else if (expr.rightBound() == vals[bin1-1]) {
                    hit0 = bin1 - 1;
                    hit1 = bin1;
                }
                else {
                    hit0 = 0;
                    hit1 = 0;
                }
            }
            else {
                hit0 = 0;
                hit1 = 0;
            }
            break;
        } // switch (expr.rightOperator())
        break; // case ibis::qExpr::OP_GE
    case ibis::qExpr::OP_EQ:
        switch (expr.rightOperator()) {
        default:
        case ibis::qExpr::OP_UNDEFINED:
            if (bin0 > nval || bin0 == 0) {
                hit0 = 0;
                hit1 = 0;
            }
            else if (expr.leftBound() == vals[bin0-1]) {
                hit0 = bin0 - 1;
                hit1 = bin0;
            }
            else {
                hit0 = 0;
                hit1 = 0;
            }
            break;
        case ibis::qExpr::OP_LT:
            if (expr.leftBound() < expr.rightBound()) {
                if (bin0 > nval || bin0 == 0) {
                    hit0 = 0;
                    hit1 = 0;
                }
                else if (expr.leftBound() == vals[bin0-1]) {
                    hit0 = bin0- 1;
                    hit1 = bin0;
                }
                else {
                    hit0 = 0;
                    hit1 = 0;
                }
            }
            else {
                hit0 = 0;
                hit1 = 0;
            }
            break;
        case ibis::qExpr::OP_LE:
            if (expr.leftBound() <= expr.rightBound()) {
                if (bin0 > nval || bin0 == 0) {
                    hit0 = 0;
                    hit1 = 0;
                }
                else if (expr.leftBound() == vals[bin0-1]) {
                    hit0 = bin0 - 1;
                    hit1 = bin0;
                }
                else {
                    hit0 = 0;
                    hit1 = 0;
                }
            }
            else {
                hit0 = 0;
                hit1 = 0;
            }
            break;
        case ibis::qExpr::OP_GT:
            if (expr.leftBound() > expr.rightBound()) {
                if (bin0 > nval || bin0 == 0) {
                    hit0 = 0;
                    hit1 = 0;
                }
                else if (expr.leftBound() == vals[bin0-1]) {
                    hit0 = bin0 - 1;
                    hit1 = bin0;
                }
                else {
                    hit0 = 0;
                    hit1 = 0;
                }
            }
            else {
                hit0 = 0;
                hit1 = 0;
            }
            break;
        case ibis::qExpr::OP_GE:
            if (expr.leftBound() >= expr.rightBound()) {
                if (bin0 > nval || bin0 == 0) {
                    hit0 = 0;
                    hit1 = 0;
                }
                else if (expr.leftBound() == vals[bin0-1]) {
                    hit0 = bin0 - 1;
                    hit1 = bin0;
                }
                else {
                    hit0 = 0;
                    hit1 = 0;
                }
            }
            else {
                hit0 = 0;
                hit1 = 0;
            }
            break;
        case ibis::qExpr::OP_EQ:
            if (expr.leftBound() == expr.rightBound()) {
                if (bin0 > nval || bin0 == 0) {
                    hit0 = 0;
                    hit1 = 0;
                }
                else if (expr.rightBound() <= vals[bin0-1]) {
                    hit0 = bin1 - 1;
                    hit1 = bin1;
                }
                else {
                    hit0 = 0;
                    hit1 = 0;
                }
            }
            else {
                hit0 = 0;
                hit1 = 0;
            }
            break;
        } // switch (expr.rightOperator())
        break; // case ibis::qExpr::OP_EQ
    default:
    case ibis::qExpr::OP_UNDEFINED:
        switch (expr.rightOperator()) {
        default:
        case ibis::qExpr::OP_UNDEFINED:
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- relic::locate encounters a unknown operator";
            hit0 = 0;
            hit1 = 0;
            return;
        case ibis::qExpr::OP_LT:
            hit0 = 0;
            if (bin1 > 0)
                hit1 = bin1 - (expr.rightBound() == vals[bin1-1]);
            else
                hit1 = 0;
            break;
        case ibis::qExpr::OP_LE:
            hit0 = 0;
            hit1 = bin1;
            break;
        case ibis::qExpr::OP_GT:
            hit1 = nval;
            hit0 = bin1;
            break;
        case ibis::qExpr::OP_GE:
            hit1 = nval;
            if (bin1 > 0)
                hit0 = bin1 - (expr.rightBound() == vals[bin1-1]);
            else
                hit0 = 0;
            break;
        case ibis::qExpr::OP_EQ:
            if (bin1 > nval || bin1 == 0) {
                hit0 = 0;
                hit1 = 0;
            }
            else if (expr.rightBound() == vals[bin1-1]) {
                hit0 = bin1 - 1;
                hit1 = bin1;
            }
            else {
                hit0 = 0;
                hit1 = 0;
            }
            break;
        } // switch (expr.rightOperator())
        break; // case ibis::qExpr::OP_UNDEFINED
    } // switch (expr.leftOperator())
    LOGGER(ibis::gVerbose > 5)
        << "relic::locate -- expr(" << expr << ") -> [" << hit0 << ", "
        << hit1 << ")";
} // ibis::relic::locate

long ibis::relic::select(const ibis::qContinuousRange& expr,
                         void* vals) const {
    uint32_t h0, h1;
    locate(expr, h0, h1);
    return mergeValues(h0, h1, vals);
} // ibis::relic::select

long ibis::relic::select(const ibis::qContinuousRange& expr, void* vals,
                         ibis::bitvector& hits) const {
    uint32_t h0, h1;
    locate(expr, h0, h1);
    sumBins(h0, h1, hits);
    return mergeValues(h0, h1, vals);
} // ibis::relic::select

/// Compute the hits as a @c bitvector.
long ibis::relic::evaluate(const ibis::qContinuousRange& expr,
                           ibis::bitvector& lower) const {
    // values in the range [hit0, hit1) satisfy the query
    uint32_t hit0, hit1;
    if (bits.empty()) {
        lower.set(0, nrows);
        return 0L;
    }

    locate(expr, hit0, hit1);
    sumBins(hit0, hit1, lower);
    return lower.cnt();
} // ibis::relic::evaluate

/// Return the number of hits satisfying the given continuous range
/// expression.
uint32_t ibis::relic::estimate(const ibis::qContinuousRange& expr) const {
    if (bits.empty()) return 0;

    uint32_t h0, h1;
    uint32_t nhits = 0;
    locate(expr, h0, h1);
    activate(h0, h1);
    for (uint32_t i=h0; i<h1; ++i) {
        nhits += bits[i]->cnt();
    }
    return nhits;
} // ibis::relic::estimate

/// Estimate the cost of resolving the continuous range expression.  The
/// answer is in the number of bytes needed from this index.
double ibis::relic::estimateCost(const ibis::qContinuousRange& expr) const {
    double ret = 0.0;
    uint32_t h0, h1;
    locate(expr, h0, h1);
    if (h0 >= h1) {
        ret = 0.0;
    }
    else if (offset64.size() > bits.size() && offset64.size() > h1 ) {
        if (h1 > h0 + 1) {
            const int64_t tot = offset64.back() - offset64[0];
            const int64_t mid = offset64[h1] - offset64[h0];
            if ((tot >> 1) >= mid)
                ret = mid;
            else
                ret = tot - mid;
        }
        else {// extra discount for a single bitmap
            ret = 0.5 * (offset64[h1] - offset64[h0]);
        }
    }
    else if (offset32.size() > bits.size() && offset32.size() > h1 ) {
        if (h1 > h0 + 1) {
            const int32_t tot = offset32.back() - offset32[0];
            const int32_t mid = offset32[h1] - offset32[h0];
            if ((tot >> 1) >= mid)
                ret = mid;
            else
                ret = tot - mid;
        }
        else {// extra discount for a single bitmap
            ret = 0.5 * (offset32[h1] - offset32[h0]);
        }
    }
    else if (h1 > h0 + 1) {
        if (h1 > bits.size())
            h1 = bits.size();
        for (uint32_t i = h0; i < h1; ++ i)
            if (bits[i] != 0)
                ret += bits[i]->bytes();
    }
    else if (h0 < bits.size() && bits[h0] != 0) {
        ret = 0.5 * bits[h0]->bytes();
    }
    return ret;
} // ibis::relic::estimateCost

/// Estimate the cost of resolving the discrete range expression.  The
/// answer is in the number of bytes needed from this index.
double ibis::relic::estimateCost(const ibis::qDiscreteRange& expr) const {
    double ret = 0.0;
    const ibis::array_t<double>& varr = expr.getValues();
    if (offset64.size() > bits.size()) {
        for (unsigned j = 0; j < varr.size(); ++ j) {
            uint32_t itmp = locate(varr[j]);
            if (itmp < bits.size())
                ret += offset64[itmp+1] - offset64[itmp];
        }
    }
    else if (offset32.size() > bits.size()) {
        for (unsigned j = 0; j < varr.size(); ++ j) {
            uint32_t itmp = locate(varr[j]);
            if (itmp < bits.size())
                ret += offset32[itmp+1] - offset32[itmp];
        }
    }
    else {
        for (unsigned j = 0; j < varr.size(); ++ j) {
            uint32_t itmp = locate(varr[j]);
            if (itmp < bits.size() && bits[itmp] != 0 )
                ret += bits[itmp]->bytes();
        }
    }
    return ret;
} // ibis::relic::estimateCost

/// Resolve a discrete range condition.  The answer is a bitvector marking
/// the rows satisfying the range conditions.
long ibis::relic::evaluate(const ibis::qDiscreteRange& expr,
                           ibis::bitvector& answer) const {
    const ibis::array_t<double>& varr = expr.getValues();
    answer.set(0, nrows);
    for (unsigned i = 0; i < varr.size(); ++ i) {
        unsigned int itmp = locate(varr[i]);
        if (itmp > 0 && vals[itmp-1] == varr[i]) {
            -- itmp;
            if (bits[itmp] == 0)
                activate(itmp);
            if (bits[itmp])
                answer |= *(bits[itmp]);
        }
    }
    return answer.cnt();
} // ibis::relic::evaluate

/// Compute the number of hits satisfying the discrete range expression.
uint32_t ibis::relic::estimate(const ibis::qDiscreteRange& expr) const {
    const ibis::array_t<double>& varr = expr.getValues();
    uint32_t cnt = 0;
    for (unsigned i = 0; i < varr.size(); ++ i) {
        unsigned int itmp = locate(varr[i]);
        if (itmp > 0 && vals[itmp-1] == varr[i]) {
            -- itmp;
            if (bits[itmp] == 0)
                activate(itmp);
            if (bits[itmp])
                cnt += bits[itmp]->cnt();
        }
    }
    return cnt;
} // ibis::relic::evaluate

/// Return all distinct values as the bin boundaries.
void ibis::relic::binBoundaries(std::vector<double>& b) const {
    b.resize(vals.size());
    for (uint32_t i = 0; i < vals.size(); ++ i)
        b[i] = vals[i];
} // ibis::relic::binBoundaries

/// Return the exact count for each distinct value.
void ibis::relic::binWeights(std::vector<uint32_t>& c) const {
    activate(); // need to make sure all bitmaps are available
    c.resize(vals.size());
    for (uint32_t i = 0; i < vals.size(); ++ i)
        c[i] = bits[i]->cnt();
} // ibis::relic::binWeights

/// Compute the sum of all values of the column indexed.
double ibis::relic::getSum() const {
    double ret;
    bool here = false;
    if (col != 0) {
        const uint32_t nbv = col->elementSize() * col->partition()->nRows();
        if (str != 0)
            here = (str->bytes() < nbv);
        else if (offset64.size() > bits.size())
            here = (static_cast<uint32_t>(offset64[bits.size()]) < nbv);
        else if (offset32.size() > bits.size())
            here = (static_cast<uint32_t>(offset32[bits.size()]) < nbv);
    }
    if (here) {
        ret = computeSum();
    }
    else { // indicate sum is not computed
        ibis::util::setNaN(ret);
    }
    return ret;
} // ibis::relic::getSum

/// Compute the sum of all values of the column indexed.
double ibis::relic::computeSum() const {
    double sum = 0;
    activate(); // need to activate all bitvectors
    for (uint32_t i = 0; i < bits.size(); ++ i)
        if (bits[i] != 0)
            sum += vals[i] * bits[i]->cnt();
    return sum;
} // ibis::relic::computeSum

/// Compute a cumulative distribition.
long ibis::relic::getCumulativeDistribution(std::vector<double>& bds,
                                            std::vector<uint32_t>& cts) const {
    bds.clear();
    cts.clear();
    long ierr = 0;
    binBoundaries(bds);
    if (bds.size() > 0) {
        binWeights(cts);
        if (bds.size() == cts.size()) {
            // convert to cumulative distribution
            // cts[i] = number of values less than bds[i]
            uint32_t cnt = cts[0];
            cts[0] = 0;
            for (uint32_t i = 1; i < bds.size(); ++ i) {
                uint32_t tmp = cts[i] + cnt;
                cts[i] = cnt;
                cnt = tmp;
            }
            bds.push_back(ibis::util::compactValue
                          (bds.back(), bds.back()+bds.back()));
            cts.push_back(cnt);
            ierr = bds.size();
        }
        else {
            // don't match, delete the content
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- relic::getCumulativeDistribution -- bds["
                << bds.size() << "] and cts[" << cts.size()
                << "] sizes do not match";
#if DEBUG+0 > 1 || _DEBUG+0 > 1
            {
                ibis::util::logger lg(4);
                lg() << "DEBUG -- bds array:\n";
                for (uint32_t i = 0; i < bds.size(); ++ i)
                    lg() << bds[i] << " ";
                lg() << "\nDEBUG -- cts array:\n";
                for (uint32_t i = 0; i < cts.size(); ++ i)
                    lg() << cts[i] << " ";
                lg() << "\n";
            }
#endif
            bds.clear();
            cts.clear();
            ierr = -2;
        }
    }
    else {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- relic::getCumulativeDistribution can not find "
            "bin boundaries, probably not data";
        bds.clear();
        cts.clear();
        ierr = -1;
    }
    return ierr;
} // ibis::relic::getCumulativeDistribution

/// Compute a histogram.
long ibis::relic::getDistribution(std::vector<double>& bds,
                                  std::vector<uint32_t>& cts) const {
    bds.clear();
    cts.clear();
    long ierr = 0;
    binBoundaries(bds);
    if (bds.size() > 0) {
        binWeights(cts);
        if (bds.size() == cts.size()) {
            for (uint32_t i = 0; i+1 < bds.size(); ++ i)
                bds[i] = bds[i+1];
            bds.resize(bds.size()-1);
            ierr = cts.size();
        }
        else {
            // don't match, delete the content
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- relic::getDistribution -- bds["
                << bds.size() << "] and cts[" << cts.size()
                << "] sizes do not match";
#if DEBUG+0 > 0 || _DEBUG+0 > 1
            {
                ibis::util::logger lg(4);
                lg() << "DEBUG -- bds array:\n";
                for (uint32_t i = 0; i < bds.size(); ++ i)
                    lg() << bds[i] << " ";
                lg() << "\nDEBUG -- cts array:\n";
                for (uint32_t i = 0; i < cts.size(); ++ i)
                    lg() << cts[i] << " ";
            }
#endif
            bds.clear();
            cts.clear();
            ierr = -2;
        }
    }
    else {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- relic::getDistribution can not find bin "
            "boundaries, probably not data";
        bds.clear();
        cts.clear();
        ierr = -1;
    }
    return ierr;
} // ibis::relic::getDistribution

/// @note It is assumed that @c range1 is for column 1 in the join
/// expression and @c range2 is for column 2 in the join expression.  No
/// name matching is performed.
void ibis::relic::estimate(const ibis::relic& idx2,
                           const ibis::deprecatedJoin& expr,
                           const ibis::bitvector& mask,
                           const ibis::qRange* const range1,
                           const ibis::qRange* const range2,
                           ibis::bitvector64& lower,
                           ibis::bitvector64& upper) const {
    lower.clear();
    upper.clear();
    if (col == 0 || idx2.col == 0) // can not do anything useful
        return;
    if (mask.cnt() == 0)
        return;
    if (range1 == 0 && range2 == 0) {
        estimate(idx2, expr, mask, lower, upper);
        return;
    }

    int64_t cnt = 0;
    horometer timer;
    if (ibis::gVerbose > 1)
        timer.start();

    if (expr.getRange() == 0) {
        cnt = equiJoin(idx2, mask, range1, range2, lower);
    }
    else if (expr.getRange()->termType() == ibis::math::NUMBER) {
        const double delta = fabs(expr.getRange()->eval());
        if (delta == 0.0)
            cnt = equiJoin(idx2, mask, range1, range2, lower);
        else
            cnt = deprecatedJoin(idx2, mask, range1, range2, delta, lower);
    }
    else {
        cnt = compJoin(idx2, mask, range1, range2, *(expr.getRange()), lower);
    }
    if (ibis::gVerbose > 1) {
        timer.stop();
        std::ostringstream ostr;
        ostr << expr;
        ostr << " with a mask (" << mask.cnt() << ")";
        if (range1) {
            if (range2)
                ostr << ", " << *range1 << ", and " << *range2;
            else
                ostr << " and " << *range1;
        }
        else if (range2) {
            ostr << " and " << *range2;
        }
        if (cnt >= 0) {
            ostr << " produced " << cnt << " hit" << (cnt>1 ? "s" : "")
                 << "(result bitvector size " << lower.bytes() << " bytes)";
            ibis::util::logMessage
                ("relic::estimate", "processing %s took %g sec(CPU), "
                 "%g sec(elapsed)",
                 ostr.str().c_str(), timer.CPUTime(), timer.realTime());
        }
        else if (col != 0 && col->partition() != 0) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- relic::estimate could not effectively evaluate "
                << ostr.str() << ", reverting to simple scans";
            cnt = col->partition()->evaluateJoin(expr, mask, lower);
            upper.clear();
        }
        else {
            lower.set(0, nrows * idx2.nrows);
            upper.set(1, nrows * idx2.nrows);
        }
    }
} // ibis::relic::estimate

int64_t ibis::relic::estimate(const ibis::relic& idx2,
                              const ibis::deprecatedJoin& expr,
                              const ibis::bitvector& mask,
                              const ibis::qRange* const range1,
                              const ibis::qRange* const range2) const {
    if (col == 0 || idx2.col == 0) // can not do anything useful
        return -1;
    if (mask.cnt() == 0)
        return 0;
    if (range1 == 0 && range2 == 0)
        return estimate(idx2, expr, mask);

    int64_t cnt = 0;
    horometer timer;
    if (ibis::gVerbose > 1)
        timer.start();

    if (expr.getRange() == 0) {
        cnt = equiJoin(idx2, mask, range1, range2);
    }
    else if (expr.getRange()->termType() == ibis::math::NUMBER) {
        const double delta = fabs(expr.getRange()->eval());
        if (delta == 0.0)
            cnt = equiJoin(idx2, mask, range1, range2);
        else
            cnt = deprecatedJoin(idx2, mask, range1, range2, delta);
    }
    else {
        cnt = compJoin(idx2, mask, range1, range2, *(expr.getRange()));
    }
    if (ibis::gVerbose > 1) {
        timer.stop();
        std::ostringstream ostr;
        ostr << expr;
        ostr << " with a mask (" << mask.cnt() << ")";
        if (range1) {
            if (range2)
                ostr << ", " << *range1 << ", and " << *range2;
            else
                ostr << " and " << *range1;
        }
        else if (range2) {
            ostr << " and " << *range2;
        }
        if (cnt >= 0) {
            ostr << " produced " << cnt << " hit" << (cnt>1 ? "s" : "");
            ibis::util::logMessage
                ("relic::estimate", "processing %s took %g sec(CPU), "
                 "%g sec(elapsed)",
                 ostr.str().c_str(), timer.CPUTime(), timer.realTime());
        }
        else {
            ibis::util::logMessage("Warning", "relic::estimate could not "
                                   "effectively process %s, revert to simple "
                                   "scan",
                                   ostr.str().c_str());
            cnt = col->partition()->evaluateJoin(expr, mask);
        }
    }
    return cnt;
} // ibis::relic::estimate

void ibis::relic::estimate(const ibis::relic& idx2,
                           const ibis::deprecatedJoin& expr,
                           const ibis::bitvector& mask,
                           ibis::bitvector64& lower,
                           ibis::bitvector64& upper) const {
    lower.clear();
    upper.clear();
    if (col == 0 || idx2.col == 0) // can not do anything useful
        return;
    if (mask.cnt() == 0)
        return;

    int64_t cnt = 0;
    horometer timer;
    if (ibis::gVerbose > 1)
        timer.start();

    if (expr.getRange() == 0) {
        cnt = equiJoin(idx2, mask, lower);
    }
    else if (expr.getRange()->termType() == ibis::math::NUMBER) {
        const double delta = fabs(expr.getRange()->eval());
        if (delta == 0.0)
            cnt = equiJoin(idx2, mask, lower);
        else
            cnt = deprecatedJoin(idx2, mask, delta, lower);
    }
    else {
        cnt = compJoin(idx2, mask, *(expr.getRange()), lower);
    }
    if (ibis::gVerbose > 1) {
        timer.stop();
        std::ostringstream ostr;
        ostr << expr << " with a mask (" << mask.cnt() << ")";
        if (cnt >= 0) {
            ostr << " produced " << cnt << " hit" << (cnt>1 ? "s" : "")
                 << "(result bitvector size " << lower.bytes() << " bytes)";
            ibis::util::logMessage
                ("relic::estimate", "processing %s took %g sec(CPU), "
                 "%g sec(elapsed)",
                 ostr.str().c_str(), timer.CPUTime(), timer.realTime());
        }
        else {
            ibis::util::logMessage("Warning", "relic::estimate could not "
                                   "effectively evaluate %s, revert to "
                                   "simple scan",
                                   ostr.str().c_str());
            cnt = col->partition()->evaluateJoin(expr, mask, lower);
        }
    }
} // ibis::relic::estimate

int64_t ibis::relic::estimate(const ibis::relic& idx2,
                              const ibis::deprecatedJoin& expr,
                              const ibis::bitvector& mask) const {
    if (col == 0 || idx2.col == 0) // can not do anything useful
        return -1;
    if (mask.cnt() == 0)
        return 0;

    int64_t cnt = 0;
    horometer timer;
    if (ibis::gVerbose > 1)
        timer.start();

    if (expr.getRange() == 0) {
        cnt = equiJoin(idx2, mask);
    }
    else if (expr.getRange()->termType() == ibis::math::NUMBER) {
        const double delta = fabs(expr.getRange()->eval());
        if (delta == 0.0)
            cnt = equiJoin(idx2, mask);
        else
            cnt = deprecatedJoin(idx2, mask, delta);
    }
    else {
        cnt = compJoin(idx2, mask, *(expr.getRange()));
    }
    if (ibis::gVerbose > 1) {
        timer.stop();
        std::ostringstream ostr;
        ostr << expr << " with a mask (" << mask.cnt() << ")";
        if (cnt >= 0) {
            ostr  << " produced " << cnt << " hit" << (cnt>1 ? "s" : "");
            ibis::util::logMessage("relic::estimate", "processing %s took %g "
                                   "sec(CPU), %g sec(elapsed)",
                                   ostr.str().c_str(), timer.CPUTime(),
                                   timer.realTime());
        }
        else {
            ibis::util::logMessage("Warning", "relic::estimate could not "
                                   "effectively evaluate %s, revert to "
                                   "simply scan",
                                   ostr.str().c_str());
            cnt = col->partition()->evaluateJoin(expr, mask);
        }
    }
    return cnt;
} // ibis::relic::estimate

/// Evaluate an equi-join using two ibis::relic indices.  The restriction
/// is presented as a bit mask.

// Implementation note:
// 2006/02/25 -- use a very simple implementation right now, the outer
// products are computed one at a time
int64_t ibis::relic::equiJoin(const ibis::relic& idx2,
                              const ibis::bitvector& mask,
                              ibis::bitvector64& hits) const {
    hits.clear();
    if (mask.cnt() == 0) return 0;

    uint32_t ib1 = 0; // index over vals in idx1
    uint32_t ib2 = 0; // index over vals in idx2
    const uint32_t nb1 = vals.size();
    const uint32_t nb2 = idx2.vals.size();

    ibis::horometer timer;
    if (ibis::gVerbose > 3) {
        timer.start();
        LOGGER(ibis::gVerbose > 3)
            << "relic::equiJoin starting to evaluate join("
            << col->name() << ", " << idx2.col->name() << ") using " << name()
            << " indices";
    }

    activate(); // need all bitvectors in memory
    idx2.activate(); // need all bitvectors in memory
    while (ib1 < nb1 && ib2 < nb2) {
        while (ib1 < nb1 && vals[ib1] < idx2.vals[ib2])
            ++ ib1;
        if (ib1 >= nb1) break;
        while (ib2 < nb2 && vals[ib1] > idx2.vals[ib2])
            ++ ib2;
        if (ib2 >= nb2) break;
        if (vals[ib1] == idx2.vals[ib2]) { // found a match
            ibis::bitvector tmp1;
            if (bits[ib1]) {
                tmp1.copy(mask);
                tmp1 &= *(bits[ib1]);
            }
            if (tmp1.cnt() > 0) {
                ibis::bitvector tmp2;
                if (idx2.bits[ib2]) {
                    tmp2.copy(mask);
                    tmp2 &= *(idx2.bits[ib2]);
                    if (tmp2.cnt() > 0) { // add the outer product
                        ibis::util::outerProduct(tmp1, tmp2, hits);
                    }
                }
            }

            // move on to the next value
            ++ ib1;
            ++ ib2;
        }
    } // while (ib1 < nb1 && ib2 < nb2)

    if (ibis::gVerbose > 3) {
        uint64_t cnt = hits.cnt();
        timer.stop();
        LOGGER(ibis::gVerbose > 3)
            << "relic::equiJoin completed evaluating join("
            << col->name() << ", " << idx2.col->name() << ") produced " << cnt
            << (cnt>1 ? " hits" : " hit") << " in "
            << timer.realTime() << " sec elapsed time";
    }
    return hits.cnt();
} // ibis::relic::equiJoin

int64_t ibis::relic::equiJoin(const ibis::relic& idx2,
                              const ibis::bitvector& mask) const {
    if (mask.cnt() == 0) return 0;

    uint32_t ib1 = 0; // index over vals in idx1
    uint32_t ib2 = 0; // index over vals in idx2
    const uint32_t nb1 = vals.size();
    const uint32_t nb2 = idx2.vals.size();
    int64_t cnt = 0;

    ibis::horometer timer;
    if (ibis::gVerbose > 3) {
        timer.start();
        LOGGER(ibis::gVerbose > 3)
            << "relic::equiJoin starting to evaluate join("
            << col->name() << ", " << idx2.col->name() << ") using " << name()
            << " indices";
    }

    activate(); // need all bitvectors in memory
    idx2.activate(); // need all bitvectors in memory
    while (ib1 < nb1 && ib2 < nb2) {
        while (ib1 < nb1 && vals[ib1] < idx2.vals[ib2])
            ++ ib1;
        if (ib1 >= nb1) break;
        while (ib2 < nb2 && vals[ib1] > idx2.vals[ib2])
            ++ ib2;
        if (ib2 >= nb2) break;
        if (vals[ib1] == idx2.vals[ib2]) { // found a match
            ibis::bitvector tmp1;
            if (bits[ib1]) {
                tmp1.copy(mask);
                tmp1 &= *(bits[ib1]);
            }
            if (tmp1.cnt() > 0) {
                ibis::bitvector tmp2;
                if (idx2.bits[ib2]) {
                    tmp2.copy(mask);
                    tmp2 &= *(idx2.bits[ib2]);
                    cnt += tmp1.cnt() * tmp2.cnt();
                }
            }

            // move on to the next value
            ++ ib1;
            ++ ib2;
        }
    } // while (ib1 < nb1 && ib2 < nb2)

    if (ibis::gVerbose > 3) {
        timer.stop();
        LOGGER(ibis::gVerbose > 3)
            << "relic::equiJoin completed evaluating join("
            << col->name() << ", " << idx2.col->name() << ") produced " << cnt
            << (cnt>1 ? " hits" : " hit") << " in "
            << timer.realTime() << " sec elapsed time";
    }
    return cnt;
} // ibis::relic::equiJoin

/// Evaluate an equi-join with explicit restrictions on the join columns.
int64_t ibis::relic::equiJoin(const ibis::relic& idx2,
                              const ibis::bitvector& mask,
                              const ibis::qRange* const range1,
                              const ibis::qRange* const range2,
                              ibis::bitvector64& hits) const {
    if (col == 0 || idx2.col == 0) return -1L;
    hits.clear();
    if (mask.cnt() == 0) return 0;

    ibis::horometer timer;
    if (ibis::gVerbose > 3) {
        timer.start();
        LOGGER(ibis::gVerbose > 3)
            << "relic::equiJoin starting to evaluate join("
            << (col ? col->name() : "?.?") << ", "
            << (idx2.col ? idx2.col->name() : "?.?") << ") using " << name()
            << " indexes";
    }

    // use the range restrictions to figure out what bitvectors to activate
    uint32_t ib1=0; // the first (begin) bitvector to be used from *this
    uint32_t ib1e=0; // the last+1 (end) bitvector to be used from *this
    uint32_t ib2=0;
    uint32_t ib2e=0;
    if (range1 == 0) {
        ib1 = 0;
        ib1e = bits.size();
    }
    else if (range1->getType() == ibis::qExpr::RANGE) {
        locate(*reinterpret_cast<const ibis::qContinuousRange*>(range1),
               ib1, ib1e);
    }
    else {
        ibis::qContinuousRange tmp(range1->leftBound(), ibis::qExpr::OP_LE,
                                   col->name(), ibis::qExpr::OP_LE,
                                   range1->rightBound());
        locate(tmp, ib1, ib1e);
    }
    if (range2 == 0) {
        ib2 = 0;
        ib2e = idx2.bits.size();
    }
    else if (range2->getType() == ibis::qExpr::RANGE) {
        locate(*reinterpret_cast<const ibis::qContinuousRange*>(range2),
               ib2, ib2e);
    }
    else {
        ibis::qContinuousRange tmp(range2->leftBound(), ibis::qExpr::OP_LE,
                                   idx2.col->name(), ibis::qExpr::OP_LE,
                                   range2->rightBound());
        locate(tmp, ib2, ib2e);
    }

    activate(ib1, ib1e); // need bitvectors in memory
    idx2.activate(ib2, ib2e); // need bitvectors in memory
    while (ib1 < ib1e && ib2 < ib2e) {
        while (ib1 < ib1e && vals[ib1] < idx2.vals[ib2])
            ++ ib1;
        if (ib1 >= ib1e) break;
        while (ib2 < ib2e && vals[ib1] > idx2.vals[ib2])
            ++ ib2;
        if (ib2 >= ib2e) break;
        if (vals[ib1] == idx2.vals[ib2]) { // found a match
            if ((range1 == 0 || range1->inRange(vals[ib1])) &&
                (range2 == 0 || range2->inRange(vals[ib1]))) {
                ibis::bitvector tmp1;
                if (bits[ib1]) {
                    tmp1.copy(mask);
                    tmp1 &= *(bits[ib1]);
                }
                if (tmp1.cnt() > 0) {
                    ibis::bitvector tmp2;
                    if (idx2.bits[ib2]) {
                        tmp2.copy(mask);
                        tmp2 &= *(idx2.bits[ib2]);
                        if (tmp2.cnt() > 0) { // add the outer product
                            ibis::util::outerProduct(tmp1, tmp2, hits);
                        }
                    }
                }
            }
            // move on to the next value
            ++ ib1;
            ++ ib2;
        }
    } // while (ib1 < ib1e && ib2 < ib2e)

    if (ibis::gVerbose > 3) {
        uint64_t cnt = hits.cnt();
        timer.stop();
        LOGGER(ibis::gVerbose > 3)
            << "relic::equiJoin completed evaluating join("
            << col->name() << ", " << idx2.col->name() << ") produced " << cnt
            << (cnt>1 ? " hits" : " hit") << " in "
            << timer.realTime() << " sec elapsed time";
    }
    return hits.cnt();
} // ibis::relic::equiJoin

/// Evaluate an equi-join with explicit restrictions on the join columns.
int64_t ibis::relic::equiJoin(const ibis::relic& idx2,
                              const ibis::bitvector& mask,
                              const ibis::qRange* const range1,
                              const ibis::qRange* const range2) const {
    if (col == 0 || idx2.col == 0) return -1L;
    int64_t cnt = 0;
    if (mask.cnt() == 0) return cnt;

    ibis::horometer timer;
    if (ibis::gVerbose > 3) {
        timer.start();
        LOGGER(ibis::gVerbose > 3)
            << "relic::equiJoin starting to evaluate join("
            << col->name() << ", " << idx2.col->name() << ") using " << name()
            << " indices";
    }

    // use the range restrictions to figure out what bitvectors to activate
    uint32_t ib1=0; // the first (begin) bitvector to be used from *this
    uint32_t ib1e=0; // the last+1 (end) bitvector to be used from *this
    uint32_t ib2=0;
    uint32_t ib2e=0;
    if (range1 == 0) {
        ib1 = 0;
        ib1e = bits.size();
    }
    else if (range1->getType() == ibis::qExpr::RANGE) {
        locate(*reinterpret_cast<const ibis::qContinuousRange*>(range1),
               ib1, ib1e);
    }
    else {
        ibis::qContinuousRange tmp(range1->leftBound(), ibis::qExpr::OP_LE,
                                   col->name(), ibis::qExpr::OP_LE,
                                   range1->rightBound());
        locate(tmp, ib1, ib1e);
    }
    if (range2 == 0) {
        ib2 = 0;
        ib2e = idx2.bits.size();
    }
    else if (range2->getType() == ibis::qExpr::RANGE) {
        locate(*reinterpret_cast<const ibis::qContinuousRange*>(range2),
               ib2, ib2e);
    }
    else {
        ibis::qContinuousRange tmp(range2->leftBound(), ibis::qExpr::OP_LE,
                                   idx2.col->name(), ibis::qExpr::OP_LE,
                                   range2->rightBound());
        locate(tmp, ib2, ib2e);
    }

    activate(ib1, ib1e); // need bitvectors in memory
    idx2.activate(ib2, ib2e); // need bitvectors in memory
    while (ib1 < ib1e && ib2 < ib2e) {
        while (ib1 < ib1e && vals[ib1] < idx2.vals[ib2])
            ++ ib1;
        if (ib1 >= ib1e) break;
        while (ib2 < ib2e && vals[ib1] > idx2.vals[ib2])
            ++ ib2;
        if (ib2 >= ib2e) break;
        if (vals[ib1] == idx2.vals[ib2]) { // found a match
            if ((range1 == 0 || range1->inRange(vals[ib1])) &&
                (range2 == 0 || range2->inRange(vals[ib1]))) {
                ibis::bitvector tmp1;
                if (bits[ib1]) {
                    tmp1.copy(mask);
                    tmp1 &= *(bits[ib1]);
                }
                if (tmp1.cnt() > 0) {
                    ibis::bitvector tmp2;
                    if (idx2.bits[ib2]) {
                        tmp2.copy(mask);
                        tmp2 &= *(idx2.bits[ib2]);
                        cnt += tmp1.cnt() * tmp2.cnt();
                    }
                }
            }
            // move on to the next value
            ++ ib1;
            ++ ib2;
        }
    } // while (ib1 < ib1e && ib2 < ib2e)

    if (ibis::gVerbose > 3) {
        timer.stop();
        LOGGER(ibis::gVerbose > 3)
            << "relic::equiJoin completed evaluating join("
            << col->name() << ", " << idx2.col->name() << ") produced " << cnt
            << (cnt>1 ? " hits" : " hit") << " in "
            << timer.realTime() << " sec elapsed time";
    }
    return cnt;
} // ibis::relic::equiJoin

/// @note If the input value @c delta is less than zero it is treated as
/// equal to zero (0).
int64_t ibis::relic::deprecatedJoin(const ibis::relic& idx2,
                               const ibis::bitvector& mask,
                               const double& delta,
                               ibis::bitvector64& hits) const {
    if (col == 0 || idx2.col == 0) return -1L;
    hits.clear();
    if (mask.cnt() == 0) return 0;

    if (delta <= 0)
        return equiJoin(idx2, mask, hits);

    uint32_t ib2s = 0; // idx2.vals[ib2s] + delta >= vals[ib1]
    uint32_t ib2e = 0; // idx2.vals[ib2e] - delta > vals[ib1]
    const uint32_t nb1 = vals.size();
    const uint32_t nb2 = idx2.vals.size();
    ibis::horometer timer;
    if (ibis::gVerbose > 3) {
        timer.start();
        LOGGER(ibis::gVerbose > 3)
            << "relic::deprecatedJoin starting to evaluate join("
            << col->name() << ", " << idx2.col->name()
            << ", " << delta << ") using " << name() << " indices";
    }

    activate(); // need all bitvectors in memory
    idx2.activate(); // need all bitvectors in memory
    for (uint32_t ib1 = 0; ib1 < nb1; ++ ib1) {
        if (bits[ib1] == 0) continue;
        ibis::bitvector tmp1 = mask;
        tmp1 &= *(bits[ib1]);
        if (tmp1.cnt() == 0) continue;

        const double lo = vals[ib1] - delta;
        const double hi = vals[ib1] + delta;
        // ib2s catch up with vals[ib1]-delta
        while (ib2s < nb2 && idx2.vals[ib2s] < lo)
            ++ ib2s;
        // ib2s catch up with vals[ib1]+delta
        if (ib2e <= ib2s)
            ib2e = ib2s;
        while (ib2e < nb2 && idx2.vals[ib2e] <= hi)
            ++ ib2e;

        if (ib2e > ib2s) {
            ibis::bitvector tmp2;
            idx2.sumBins(ib2s, ib2e, tmp2);
            tmp2 &= mask;
            if (tmp2.cnt() > 0) {
                ibis::util::outerProduct(tmp1, tmp2, hits);
            }
        }
    } // for (uint32_t ib1...

    if (ibis::gVerbose > 3) {
        uint64_t cnt = hits.cnt();
        timer.stop();
        LOGGER(ibis::gVerbose > 3)
            << "relic::deprecatedJoin completed evaluating join("
            << col->name() << ", " << idx2.col->name()
            << ", " << delta << ") produced " << cnt
            << (cnt>1 ? " hits" : " hit") << " in "
            << timer.realTime() << " sec elapsed time";
    }
    return hits.cnt();
} // ibis::relic::deprecatedJoin

int64_t ibis::relic::deprecatedJoin(const ibis::relic& idx2,
                               const ibis::bitvector& mask,
                               const double& delta) const {
    if (col == 0 || idx2.col == 0) return -1L;
    int64_t cnt = 0;
    if (mask.cnt() == 0) return cnt;

    if (delta <= 0)
        return equiJoin(idx2, mask);

    uint32_t ib2s = 0; // idx2.vals[ib2s] + delta >= vals[ib1]
    uint32_t ib2e = 0; // idx2.vals[ib2e] - delta > vals[ib1]
    const uint32_t nb1 = vals.size();
    const uint32_t nb2 = idx2.vals.size();

    ibis::horometer timer;
    if (ibis::gVerbose > 3) {
        timer.start();
        LOGGER(ibis::gVerbose > 3)
            << "relic::deprecatedJoin starting to evaluate join("
            << col->name() << ", " << idx2.col->name()
            << ", " << delta << ") using " << name() << " indices";
    }

    activate(); // need all bitvectors in memory
    idx2.activate(); // need all bitvectors in memory
    for (uint32_t ib1 = 0; ib1 < nb1; ++ ib1) {
        if (bits[ib1] == 0) continue;
        ibis::bitvector tmp1 = mask;
        tmp1 &= *(bits[ib1]);
        if (tmp1.cnt() == 0) continue;

        const double lo = vals[ib1] - delta;
        const double hi = vals[ib1] + delta;
        // ib2s catch up with vals[ib1]-delta
        while (ib2s < nb2 && idx2.vals[ib2s] < lo)
            ++ ib2s;
        // ib2s catch up with vals[ib1]+delta
        if (ib2e <= ib2s)
            ib2e = ib2s;
        while (ib2e < nb2 && idx2.vals[ib2e] <= hi)
            ++ ib2e;

        if (ib2e > ib2s) {
            ibis::bitvector tmp2;
            idx2.sumBins(ib2s, ib2e, tmp2);
            tmp2 &= mask;
            cnt += tmp1.cnt() * tmp2.cnt();
        }
    } // for (uint32_t ib1...

    if (ibis::gVerbose > 3) {
        timer.stop();
        LOGGER(ibis::gVerbose > 3)
            << "relic::deprecatedJoin completed evaluating join("
            << col->name() << ", " << idx2.col->name()
            << ", " << delta << ") produced " << cnt
            << (cnt>1 ? " hits" : " hit") << " in "
            << timer.realTime() << " sec elapsed time";
    }
    return cnt;
} // ibis::relic::deprecatedJoin

/// @note If the input value @c delta is less than zero it is treated as
/// equal to zero (0).
int64_t ibis::relic::deprecatedJoin(const ibis::relic& idx2,
                               const ibis::bitvector& mask,
                               const ibis::qRange* const range1,
                               const ibis::qRange* const range2,
                               const double& delta,
                               ibis::bitvector64& hits) const {
    if (col == 0 || idx2.col == 0) return -1L;
    hits.clear();
    if (mask.cnt() == 0) return 0;

    if (delta <= 0)
        return equiJoin(idx2, mask, range1, range2, hits);
    if (range2 != 0 && range2->getType() != ibis::qExpr::RANGE) {
        col->logMessage("relic::deprecatedJoin", "current implementation does "
                        "more work than necessary because if can not "
                        "handle discrete range restrictions on %s!",
                        idx2.col->name());
    }

    ibis::horometer timer;
    if (ibis::gVerbose > 3) {
        timer.start();
        LOGGER(ibis::gVerbose > 3)
            << "relic::deprecatedJoin starting to evaluate join("
            << col->name() << ", " << idx2.col->name()
            << ", " << delta << ") using " << name() << " indices";
    }

    uint32_t nb1s = 0;
    uint32_t nb1e = 0;
    uint32_t nb2s = 0;
    uint32_t nb2e = 0;
    const uint32_t nb2 = idx2.vals.size();
    if (range1 == 0) {
        nb1s = 0;
        nb1e = bits.size();
    }
    else if (range1->getType() == ibis::qExpr::RANGE) {
        locate(*reinterpret_cast<const ibis::qContinuousRange*>(range1),
               nb1s, nb1e);
    }
    else {
        ibis::qContinuousRange tmp(range1->leftBound(), ibis::qExpr::OP_LE,
                                   col->name(), ibis::qExpr::OP_LE,
                                   range1->rightBound());
        locate(tmp, nb1s, nb1e);
    }
    if (range2 == 0) {
        nb2s = 0;
        nb2e = idx2.bits.size();
    }
    else if (range2->getType() == ibis::qExpr::RANGE) {
        locate(*reinterpret_cast<const ibis::qContinuousRange*>(range2),
               nb2s, nb2e);
    }
    else {
        ibis::qContinuousRange tmp(range2->leftBound(), ibis::qExpr::OP_LE,
                                   idx2.col->name(), ibis::qExpr::OP_LE,
                                   range2->rightBound());
        locate(tmp, nb2s, nb2e);
    }
    uint32_t ib2s = nb2s; // idx2.vals[ib2s] + delta >= vals[ib1]
    uint32_t ib2e = nb2s; // idx2.vals[ib2e] - delta > vals[ib1]
    activate(nb1s, nb1e); // need bitvectors in memory
    idx2.activate(nb2s, nb2e); // need bitvectors in memory
    for (uint32_t ib1 = nb1s; ib1 < nb1e; ++ ib1) {
        if (bits[ib1] == 0 || !(range1 == 0 || range1->inRange(vals[ib1])))
            continue;
        ibis::bitvector tmp1 = mask;
        tmp1 &= *(bits[ib1]);
        if (tmp1.cnt() == 0) continue;

        const double lo = vals[ib1] - delta;
        const double hi = vals[ib1] + delta;
        // ib2s catch up with vals[ib1]-delta
        while (ib2s < nb2 && idx2.vals[ib2s] < lo)
            ++ ib2s;
        // ib2s catch up with vals[ib1]+delta
        if (ib2e <= ib2s)
            ib2e = ib2s;
        while (ib2e < nb2 && idx2.vals[ib2e] <= hi)
            ++ ib2e;

        // this only work if range2 is a qContinuousRange
        if (ib2e > ib2s) {
            ibis::bitvector tmp2;
            idx2.sumBins(ib2s, ib2e, tmp2);
            tmp2 &= mask;
            if (tmp2.cnt() > 0) {
                ibis::util::outerProduct(tmp1, tmp2, hits);
            }
        }
    } // for (uint32_t ib1...

    if (ibis::gVerbose > 3) {
        uint64_t cnt = hits.cnt();
        timer.stop();
        LOGGER(ibis::gVerbose > 3)
            << "relic::deprecatedJoin completed evaluating join("
            << col->name() << ", " << idx2.col->name()
            << ", " << delta << ") produced " << cnt
            << (cnt>1 ? " hits" : " hit") << " in "
            << timer.realTime() << " sec elapsed time";
    }
    return hits.cnt();
} // ibis::relic::deprecatedJoin

int64_t ibis::relic::deprecatedJoin(const ibis::relic& idx2,
                               const ibis::bitvector& mask,
                               const ibis::qRange* const range1,
                               const ibis::qRange* const range2,
                               const double& delta) const {
    if (col == 0 || idx2.col == 0) return -1L;
    int64_t cnt = 0;
    if (mask.cnt() == 0) return cnt;

    if (delta <= 0)
        return equiJoin(idx2, mask, range1, range2);
    if (range2 != 0 && range2->getType() != ibis::qExpr::RANGE) {
        col->logMessage("relic::deprecatedJoin", "current implementation does "
                        "more work than necessary because if can not "
                        "handle discrete range restrictions on %s!",
                        idx2.col->name());
    }

    ibis::horometer timer;
    if (ibis::gVerbose > 3) {
        timer.start();
        LOGGER(ibis::gVerbose > 3)
            << "relic::deprecatedJoin starting to evaluate join("
            << col->name() << ", " << idx2.col->name()
            << ", " << delta << ") using " << name() << " indices";
    }

    uint32_t nb1s = 0;
    uint32_t nb1e = 0;
    uint32_t nb2s = 0;
    uint32_t nb2e = 0;
    const uint32_t nb2 = idx2.vals.size();
    if (range1 == 0) {
        nb1s = 0;
        nb1e = bits.size();
    }
    else if (range1->getType() == ibis::qExpr::RANGE) {
        locate(*reinterpret_cast<const ibis::qContinuousRange*>(range1),
               nb1s, nb1e);
    }
    else {
        ibis::qContinuousRange tmp(range1->leftBound(), ibis::qExpr::OP_LE,
                                   col->name(), ibis::qExpr::OP_LE,
                                   range1->rightBound());
        locate(tmp, nb1s, nb1e);
    }
    if (range2 == 0) {
        nb2s = 0;
        nb2e = idx2.bits.size();
    }
    else if (range2->getType() == ibis::qExpr::RANGE) {
        locate(*reinterpret_cast<const ibis::qContinuousRange*>(range2),
               nb2s, nb2e);
    }
    else {
        ibis::qContinuousRange tmp(range2->leftBound(), ibis::qExpr::OP_LE,
                                   idx2.col->name(), ibis::qExpr::OP_LE,
                                   range2->rightBound());
        locate(tmp, nb2s, nb2e);
    }
    uint32_t ib2s = nb2s; // idx2.vals[ib2s] + delta >= vals[ib1]
    uint32_t ib2e = nb2s; // idx2.vals[ib2e] - delta > vals[ib1]
    activate(nb1s, nb1e); // need bitvectors in memory
    idx2.activate(nb2s, nb2e); // need bitvectors in memory
    for (uint32_t ib1 = nb1s; ib1 < nb1e; ++ ib1) {
        if (bits[ib1] == 0 || ! (range1 == 0 || range1->inRange(vals[ib1])))
            continue;
        ibis::bitvector tmp1 = mask;
        tmp1 &= *(bits[ib1]);
        if (tmp1.cnt() == 0) continue;

        const double lo = vals[ib1] - delta;
        const double hi = vals[ib1] + delta;
        // ib2s catch up with vals[ib1]-delta
        while (ib2s < nb2 && idx2.vals[ib2s] < lo)
            ++ ib2s;
        // ib2s catch up with vals[ib1]+delta
        if (ib2e <= ib2s)
            ib2e = ib2s;
        while (ib2e < nb2 && idx2.vals[ib2e] <= hi)
            ++ ib2e;

        // this only work if range2 is a qContinuousRange
        if (ib2e > ib2s) {
            ibis::bitvector tmp2;
            idx2.sumBins(ib2s, ib2e, tmp2);
            tmp2 &= mask;
            cnt += tmp2.cnt() * tmp1.cnt();
        }
    } // for (uint32_t ib1...

    if (ibis::gVerbose > 3) {
        timer.stop();
        LOGGER(ibis::gVerbose > 3)
            << "relic::deprecatedJoin completed evaluating join("
            << col->name() << ", " << idx2.col->name()
            << ", " << delta << ") produced " << cnt
            << (cnt>1 ? " hits" : " hit") << " in "
            << timer.realTime() << " sec elapsed time";
    }
    return cnt;
} // ibis::relic::deprecatedJoin

/// @note If the input value @c delta is less than zero it is treated as
/// equal to zero (0).
int64_t ibis::relic::compJoin(const ibis::relic& idx2,
                              const ibis::bitvector& mask,
                              const ibis::math::term& delta,
                              ibis::bitvector64& hits) const {
    if (col == 0 || idx2.col == 0) return -1L;
    hits.clear();
    if (mask.cnt() == 0) return 0;

    ibis::math::barrel bar(&delta);
    if (bar.size() == 1 &&
        stricmp(bar.name(0), col->name()) == 0) {
        // continue
    }
    else if (bar.size() < 1) { // no variable involved
        return deprecatedJoin(idx2, mask, delta.eval(), hits);
    }
    else { // can not evaluate the join effectively here
        return -1;
    }

    ibis::horometer timer;
    if (ibis::gVerbose > 3) {
        timer.start();
        LOGGER(ibis::gVerbose > 3)
            << "relic::compJoin starting to evaluate join("
            << col->name() << ", " << idx2.col->name()
            << ", " << delta << ") using " << name() << " indices";
    }
    const uint32_t nb1 = vals.size();
    activate(); // need all bitvectors in memory
    idx2.activate(); // need all bitvectors in memory
    for (uint32_t ib1 = 0; ib1 < nb1; ++ ib1) {
        if (bits[ib1] == 0) continue;
        ibis::bitvector tmp1 = mask;
        tmp1 &= *(bits[ib1]);
        if (tmp1.cnt() == 0) continue;

        bar.value(0) = vals[ib1];
        const double dt = fabs(delta.eval());
        const double lo = vals[ib1] - dt;
        const double hi = ibis::util::incrDouble(vals[ib1] + dt);
        uint32_t ib2s = idx2.vals.find(lo);
        uint32_t ib2e = idx2.vals.find(hi);
        if (ib2e > ib2s) {
            ibis::bitvector tmp2;
            idx2.sumBins(ib2s, ib2e, tmp2);
            tmp2 &= mask;
            if (tmp2.cnt() > 0) {
                ibis::util::outerProduct(tmp1, tmp2, hits);
            }
        }
    } // for (uint32_t ib1...

    if (ibis::gVerbose > 3) {
        uint64_t cnt = hits.cnt();
        timer.stop();
        LOGGER(ibis::gVerbose > 3)
            << "relic::compJoin completed evaluating join("
            << col->name() << ", " << idx2.col->name()
            << ", " << delta << ") produced " << cnt
            << (cnt>1 ? " hits" : " hit") << " in "
            << timer.realTime() << " sec elapsed time";
    }
    return hits.cnt();
} // ibis::relic::compJoin

int64_t ibis::relic::compJoin(const ibis::relic& idx2,
                              const ibis::bitvector& mask,
                              const ibis::math::term& delta) const {
    if (col == 0 || idx2.col == 0) return -1L;
    int64_t cnt = 0;
    if (mask.cnt() == 0) return cnt;

    ibis::math::barrel bar(&delta);
    if (bar.size() == 1 &&
        stricmp(bar.name(0), col->name()) == 0) {
        // continue
    }
    else if (bar.size() < 1) { // no variable involved
        return deprecatedJoin(idx2, mask, delta.eval());
    }
    else { // can not evaluate the join effectively here
        return -1;
    }

    ibis::horometer timer;
    if (ibis::gVerbose > 3) {
        timer.start();
        LOGGER(ibis::gVerbose > 3)
            << "relic::compJoin starting to evaluate join("
            << col->name() << ", " << idx2.col->name()
            << ", " << delta << ") using " << name() << " indices";
    }
    const uint32_t nb1 = vals.size();
    activate(); // need all bitvectors in memory
    idx2.activate(); // need all bitvectors in memory
    for (uint32_t ib1 = 0; ib1 < nb1; ++ ib1) {
        if (bits[ib1] == 0) continue;
        ibis::bitvector tmp1 = mask;
        tmp1 &= *(bits[ib1]);
        if (tmp1.cnt() == 0) continue;

        bar.value(0) = vals[ib1];
        const double dt = fabs(delta.eval());
        const double lo = vals[ib1] - dt;
        const double hi = ibis::util::incrDouble(vals[ib1] + dt);
        uint32_t ib2s = idx2.vals.find(lo);
        uint32_t ib2e = idx2.vals.find(hi);
        if (ib2e > ib2s) {
            ibis::bitvector tmp2;
            idx2.sumBins(ib2s, ib2e, tmp2);
            tmp2 &= mask;
            cnt += tmp1.cnt() * tmp2.cnt();
        }
    } // for (uint32_t ib1...

    if (ibis::gVerbose > 3) {
        timer.stop();
        LOGGER(ibis::gVerbose > 3)
            << "relic::compJoin completed evaluating join("
            << col->name() << ", " << idx2.col->name()
            << ", " << delta << ") produced " << cnt
            << (cnt>1 ? " hits" : " hit") << " in "
            << timer.realTime() << " sec elapsed time";
    }
    return cnt;
} // ibis::relic::compJoin

/// Compute the size of the index in a file.
size_t ibis::relic::getSerialSize() const throw() {
    size_t res = 24 + 8 * (bits.size() + vals.size());
    for (unsigned j = 0; j < bits.size(); ++ j)
        if (bits[j] != 0)
            res += bits[j]->getSerialSize();
    return res;
} // ibis::relic::getSerialSize

/// Merge the values in different bitmaps into a single list.  The values
/// in the list appears in the order of original rows where they came from.
/// This function returns the number of elements in the output array upon
/// successful completion.
long ibis::relic::mergeValues(uint32_t ib, uint32_t je, void* res) const {
    long ierr = -1;
    if (je > bits.size()) je = bits.size();
    if (ib >= je) return 0;

    const size_t nv = je - ib;
    activate(ib, je); // activate all necessary bitmaps
    ibis::array_t<const ibis::bitvector*> ps(nv);
    for (unsigned j = 0; j < nv; ++ j)
        ps[j] = bits[ib+j];

    switch (col->type()) {
    default:
        break;
    case ibis::BYTE: {
        array_t<signed char> vs(nv);
        for (unsigned j = ib; j < je; ++ j)
            vs[j-ib] = vals[j];
        ierr = mergeValuesT(vs, ps, *static_cast<array_t<signed char>*>(res));
        break;}
    case ibis::UBYTE: {
        array_t<unsigned char> vs(nv);
        for (unsigned j = ib; j < je; ++ j)
            vs[j-ib] = vals[j];
        ierr = mergeValuesT(vs, ps, *static_cast<array_t<unsigned char>*>(res));
        break;}
    case ibis::SHORT: {
        array_t<int16_t> vs(nv);
        for (unsigned j = ib; j < je; ++ j)
            vs[j-ib] = vals[j];
        ierr = mergeValuesT(vs, ps, *static_cast<array_t<int16_t>*>(res));
        break;}
    case ibis::USHORT: {
        array_t<uint16_t> vs(nv);
        for (unsigned j = ib; j < je; ++ j)
            vs[j-ib] = vals[j];
        ierr = mergeValuesT(vs, ps, *static_cast<array_t<uint16_t>*>(res));
        break;}
    case ibis::INT:{
        array_t<int32_t> vs(nv);
        for (unsigned j = ib; j < je; ++ j)
            vs[j-ib] = vals[j];
        ierr = mergeValuesT(vs, ps, *static_cast<array_t<int32_t>*>(res));
        break;}
    case ibis::UINT:{
        array_t<uint32_t> vs(nv);
        for (unsigned j = ib; j < je; ++ j)
            vs[j-ib] = vals[j];
        ierr = mergeValuesT(vs, ps, *static_cast<array_t<uint32_t>*>(res));
        break;}
    case ibis::LONG:{
        array_t<int64_t> vs(nv);
        for (unsigned j = ib; j < je; ++ j)
            vs[j-ib] = vals[j];
        ierr = mergeValuesT(vs, ps, *static_cast<array_t<int64_t>*>(res));
        break;}
    case ibis::ULONG:{
        array_t<uint64_t> vs(nv);
        for (unsigned j = ib; j < je; ++ j)
            vs[j-ib] = vals[j];
        ierr = mergeValuesT(vs, ps, *static_cast<array_t<uint64_t>*>(res));
        break;}
    case ibis::FLOAT:{
        array_t<float> vs(nv);
        for (unsigned j = ib; j < je; ++ j)
            vs[j-ib] = vals[j];
        ierr = mergeValuesT(vs, ps, *static_cast<array_t<float>*>(res));
        break;}
    case ibis::DOUBLE:{
        array_t<double> vs(vals, ib, je);
        ierr = mergeValuesT(vs, ps, *static_cast<array_t<double>*>(res));
        break;}
    }
    return ierr;
} // ibis::relic::mergeValues

/// A template function to merge list of values and a list of positions.
/// Return the number of elements in the results or an error number.
template<typename T> long
ibis::relic::mergeValuesT(const array_t<T>& vs,
                          const array_t<const bitvector*>& ps,
                          array_t<T>& res) {
    res.clear();
    const size_t nv = (vs.size() <= ps.size() ? vs.size() : ps.size());
    long ierr = 0;
    if (nv == 0) {
        ; // nothing to do
    }
    else if (nv == 1) { // all values are the same
        const unsigned nres = ps[0]->cnt();
        const T v = vs[0];
        res.resize(nres);
        for (unsigned i = 0; i < nres; ++ i)
            res[i] = v;
    }
    else if (nv == 2) { // two different values
        const unsigned nres = ps[0]->cnt() + ps[1]->cnt();
        res.reserve(nres);

        const T v0 = vs[0];
        const T v1 = vs[1];
        ibis::bitvector::indexSet idx0 = ps[0]->firstIndexSet();
        ibis::bitvector::indexSet idx1 = ps[1]->firstIndexSet();
        while (idx0.nIndices() > 0 && idx1.nIndices() > 0) {
            const ibis::bitvector::word_t* iptr0 = idx0.indices();
            const ibis::bitvector::word_t* iptr1 = idx1.indices();
            if (idx0.isRange()) {
                if (*iptr0 < *iptr1) {
                    for (unsigned j = 0; j < idx0.nIndices(); ++ j)
                        res.push_back(v0);
                    ++ idx0;
                }
                else {
                    for (unsigned j = 0; j < idx1.nIndices(); ++ j)
                        res.push_back(v1);
                    ++ idx1;
                }
            }
            else if (idx1.isRange()) {
                if (*iptr0 < *iptr1) {
                    for (unsigned j = 0; j < idx0.nIndices(); ++ j)
                        res.push_back(v0);
                    ++ idx0;
                }
                else {
                    for (unsigned j = 0; j < idx1.nIndices(); ++ j)
                        res.push_back(v1);
                    ++ idx1;
                }
            }
            else if (iptr0[idx0.nIndices()-1] < *iptr1) {
                for (unsigned j = 0; j < idx0.nIndices(); ++ j)
                    res.push_back(v0);
                ++ idx0;
            }
            else if (*iptr0 > iptr1[idx1.nIndices()-1]) {
                for (unsigned j = 0; j < idx1.nIndices(); ++ j)
                    res.push_back(v1);
                ++ idx1;
            }
            else {
                unsigned j0 = 0;
                unsigned j1 = 0;
                while (j0 < idx0.nIndices() && j1 < idx1.nIndices()) {
                    if (iptr0[j0] < iptr1[j1]) {
                        res.push_back(v0);
                        ++ j0;
                    }
                    else {
                        res.push_back(v1);
                        ++ j1;
                    }
                }
                while (j0 < idx0.nIndices()) {
                    res.push_back(v0);
                    ++ j0;
                }
                while (j1 < idx1.nIndices()) {
                    res.push_back(v1);
                    ++ j1;
                }
                ++ idx0;
                ++ idx1;
            }
        }
        while (idx0.nIndices() > 0) {
            for (unsigned j = 0; j < idx0.nIndices(); ++ j)
                res.push_back(v0);
            ++ idx0;
        }
        while (idx1.nIndices() > 0) {
            for (unsigned j = 0; j < idx1.nIndices(); ++ j)
                res.push_back(v1);
            ++ idx1;
        }
        ierr = res.size();
    }
    else { // the general case with arbitray number of values
        // vp holds the actual values and their associated positions
        // hp organize the values according to their positions using a min-heap
        std::vector< ibis::relic::valpos<T> > vp(nv);
        ibis::util::heap< ibis::relic::valpos<T>,
                          ibis::relic::comparevalpos<T> > hp;
        hp.reserve(nv); // reserve enough space for every value
        for (unsigned iv = 0; iv < nv; ++ iv) {
            if (ps[iv] != 0 && ps[iv]->cnt() > 0) {
                vp[iv].ind = ps[iv]->firstIndexSet();
                vp[iv].val = vs[iv];
                hp.push(&(vp[iv]));
            }
        }
        // use the heap to pick the next value as long as there are more
        // than one value
        while (hp.size() > 1) {
            ibis::relic::valpos<T>*const t = hp.top();
            if (t->ind.isRange()) { // add a consecutive range of rows
                res.insert(res.end(), t->ind.nIndices(), t->val);
                ++ t->ind;
                if (t->ind.isRange())
                    t->j = *(t->ind.indices());
                else
                    t->j = 0;
            }
            else { // add one value
                res.push_back(t->val);
                t->next();
            }

            hp.pop(); // remove the current value from the heap
            if (t->ind.nIndices() > 0) { // add it back to the heap
                hp.push(t);
            }
        } // while (hp.size() > 1)

        if (hp.size() > 0) {
            ibis::relic::valpos<T>*const t = hp.top();
            ibis::bitvector::indexSet& s = t->ind;
            while (s.nIndices() > 0) {
                if (s.isRange()) {
                    res.insert(res.end(), s.nIndices(), t->val);
                    ++ s;
                }
                else {
                    res.insert(res.end(), s.nIndices() - t->j, t->val);
                    ++ s;
                    t->j = 0;
                }
            }
        }
    }

    ierr = res.size();
    LOGGER(ibis::gVerbose > 3)
        << "relic::mergeValuesT<" << typeid(T).name()
        << "> -- merged " << nv << " value" << (nv > 1 ? "s" : "")
        << ", produced a result array of size " << ierr;
    return ierr;
} // ibis::relic::mergeValuesT
