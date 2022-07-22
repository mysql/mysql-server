// $Id$
// Author: John Wu <John.Wu at ACM.org>
// Copyright (c) 2000-2016 the Regents of the University of California
//
// This file contains the implementation of the classes ibis::bin.  The
// header for the class is in ibin.h.
//
#if defined(_WIN32) && defined(_MSC_VER)
#pragma warning(disable:4786)   // some identifier longer than 256 characters
#endif
#include "ibin.h"
#include "part.h"
#include "column.h"
#include "resource.h"
#include "bitvector64.h"

#include <algorithm>    // std::sort
#include <typeinfo>     // typeid
#include <sstream>      // std::ostringstream
#include <iomanip>      // std::setprecision

// The default number of bins if nothing else is specified
#ifndef IBIS_DEFAULT_NBINS
#define IBIS_DEFAULT_NBINS 10000
#endif

#define FASTBIT_SYNC_WRITE 1

/// Constructor.  Construct a bitmap index from current data.
ibis::bin::bin(const ibis::column* c, const char* f)
    : ibis::index(c), nobs(0) {
    try {
        if (f != 0 && 0 == read(f)) // try to read the file as an index file
           return;

        if (c == 0) return;  // nothing can be done
        if (c->isNumeric() == false) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- bin can only work on numerical values";
            return;
        }

        if (nobs == 0 && (f != 0 || c->partition() != 0))
            construct(f);
        if (nobs == 0) { // attempt to read all values through getValuesArray
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

        if (nobs > 0 && ibis::gVerbose > 2) {
            ibis::util::logger lg;
            lg() << "bin[" << col->fullname()
                 << "]::ctor -- initialization completed with "
                 << nobs << " bin" << (nobs>1?"s":"") << " for "
                 << nrows << " row" << (nrows>1?"s":"");
            if (ibis::gVerbose > 6) {
                lg() << "\n";
                print(lg());
            }
        }
    }
    catch (...) {
        LOGGER(ibis::gVerbose > 1)
            << "Warning -- column[" << col->name()
            << "]::bin::ctor encountered an exception, cleaning up ...";
        clear(); // need to call clear before rethrow the exception
        throw;
    }
} // constructor

/// Constructor.  Construct an index with the given bin boundaries.
ibis::bin::bin(const ibis::column* c, const char* f,
               const array_t<double>& bd)
    : ibis::index(c), nobs(0) {
    if (c == 0) return;
    if (c->isNumeric() == false) {
        LOGGER(ibis::gVerbose > 1)
            << "Warning -- bin can only work on numerical values";
        return;
    }
    try {
        binning(f, bd);
        const char* spec = col->indexSpec();
        if (spec == 0 || *spec == 0) {
            std::string idxnm;
            if (c->partition() != 0) {
                idxnm = c->partition()->name();
                idxnm += '.';
            }
            idxnm += c->name();
            idxnm += ".index";
            spec = ibis::gParameters()[idxnm.c_str()];
        }
        const bool reorder =
            (spec != 0 ? strstr(spec, "reorder") != 0 : false);
        if (reorder)
            binOrder(f);

        optionalUnpack(bits, col->indexSpec());
        if (ibis::gVerbose > 2) {
            ibis::util::logger lg;
            lg() << "bin[" << col->fullname()
                 << "]::ctor -- intialization completed with "
                 << nobs << " bin" << (nobs>1?"s":"") << " for "
                 << nrows << " row" << (nrows>1?"s":"");
            if (ibis::gVerbose > 6) {
                lg() << "\n";
                print(lg());
            }
        }
    }
    catch (...) {
        LOGGER(ibis::gVerbose > 1)
            << "Warning -- column[" << col->name()
            << "]::bin::ctor encountered an exception, cleaning up ...";
        clear();
        throw;
    }
} // constructor

/// Constructor.  Construct an index with the given bin boundaries.
ibis::bin::bin(const ibis::column* c, const char* f,
               const std::vector<double>& bd)
    : ibis::index(c), nobs(0) {
    if (c == 0) return;
    if (c->isNumeric() == false) {
        LOGGER(ibis::gVerbose > 1)
            << "Warning -- bin can only work on numerical values";
        return;
    }
    try {
        binning(f, bd);
        const char* spec = col->indexSpec();
        if (spec == 0 || *spec == 0) {
            std::string idxnm;
            if (c->partition()) {
                idxnm = c->partition()->name();
                idxnm += '.';
            }
            idxnm += c->name();
            idxnm += ".index";
            spec = ibis::gParameters()[idxnm.c_str()];
        }
        const bool reorder = (spec != 0 ? strstr(spec, "reorder") != 0 :
                              false);
        if (reorder)
            binOrder(f);
        optionalUnpack(bits, col->indexSpec());
        if (ibis::gVerbose > 2) {
            ibis::util::logger lg;
            lg() << "bin[" << col->fullname()
                 << "]::ctor -- intialization completed with "
                 << nobs << " bin" << (nobs>1?"s":"") << " for "
                 << nrows << " row" << (nrows>1?"s":"");
            if (ibis::gVerbose > 6) {
                lg() << "\n";
                print(lg());
            }
        }
    }
    catch (...) {
        LOGGER(ibis::gVerbose > 1)
            << "Warning -- column[" << col->name()
            << "]::bin::ctor encountered an exception, cleaning up ...";
        clear();
        throw;
    }
} // constructor

/// Copy constructor.  It performs a deep copy.
ibis::bin::bin(const ibis::bin& rhs)
    : ibis::index(rhs), nobs(rhs.nobs), bounds(rhs.bounds), maxval(rhs.maxval),
      minval(rhs.minval) {
    if (ibis::gVerbose > 2) {
        ibis::util::logger lg;
        lg() << "bin[" << (col ? col->name() : "?.?")
             << "]::ctor -- initialization completed copying "
             << nobs << " bin" << (nobs>1?"s":"") << " for "
             << nrows << " row" << (nrows>1?"s":"");
        if (ibis::gVerbose > 6) {
            lg() << "\n";
            print(lg());
        }
    }
} // copy constructor

/// Constructor.  Reconstruct from content of fileManager::storage.
///@code
/// The common portion of the index files writen by derived classes of bin
/// 8-byte header
/// nrows(uint32_t) -- number of bits in each bit vector
/// nobs (uint32_t) -- number of bit vectors
/// offsets (intxx_t[nobs+1]) -- the starting positions of the bit sequences
///       (i.e., bit vectors) plus the end position of the last one
/// (padding to ensure the next data element is on 8-byte boundary)
/// bounds (double[nobs]) -- the end positions (right sides) of the bins
/// maxval (double[nobs]) -- the maximum value of all values fall in the bin
/// minval (double[nobs]) -- the minimum value of all those fall in the bin
/// the bit sequences as bit vectors
///@encdoe
ibis::bin::bin(const ibis::column* c, ibis::fileManager::storage* st,
               size_t start)
    : ibis::index(c, st),
      nobs(*(reinterpret_cast<uint32_t*>(st->begin()+start+sizeof(uint32_t)))),
      bounds(st, 8*((start+(*st)[6]*(nobs+1)+2*sizeof(uint32_t)+7)/8),
             8*((start+(*st)[6]*(nobs+1)+2*sizeof(uint32_t)+7)/8) +
             sizeof(double)*nobs),
      maxval(st, 8*((start+(*st)[6]*(nobs+1)+2*sizeof(uint32_t)+7)/8) +
             sizeof(double)*nobs,
             8*((start+(*st)[6]*(nobs+1)+2*sizeof(uint32_t)+7)/8) +
             sizeof(double)*nobs*2),
      minval(st, 8*((start+(*st)[6]*(nobs+1)+2*sizeof(uint32_t)+7)/8) +
             sizeof(double)*nobs*2,
             8*((start+(*st)[6]*(nobs+1)+2*sizeof(uint32_t)+7)/8) +
             sizeof(double)*nobs*3) {
    try {
        nrows = *(reinterpret_cast<uint32_t*>(st->begin()+start));
// LOGGER(c->partition()->getState() == ibis::part::STABLE_STATE &&
//        nrows != c->partition()->nRows() && ibis::gVerbose > 2)
//     << "Warning -- bin[" << col->fullname() << "]::bin found nrows ("
//     << nrows << ") to be different from that of the data partition "
//     << c->partition()->name() << " (" << c->partition()->nRows() << ")";

        int ierr = initOffsets(st, start+2*sizeof(uint32_t), nobs);
        if (ierr < 0) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- bin[" << (col ? col->fullname() : "?.?")
                << "]::bin failed to initialize bitmap offsets"
                << " from storage object @ " << st << " with start = " << start
                << ", ierr = " << ierr;
            throw "bin::ctor failed to initOffsets from storage" IBIS_FILE_LINE;
        }

        initBitmaps(st);

        if (ibis::gVerbose > 2) {
            ibis::util::logger lg;
            lg() << "bin[" << (col ? col->fullname() : "?.?")
                 << "]::ctor -- initialization completed with "
                 << nobs << " bin" << (nobs>1?"s":"") << " for "
                 << nrows << " row" << (nrows>1?"s":"")
                 << " from a storage object @ " << st << " offset " << start;
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
} // ibis::bin::bin

/// Constructor.  Handle the common portion of multicomponent encodings.
///@code
/// nrows  (uint32_t)        -- number of bits in a bitvector
/// nobs   (uint32_t)        -- number of bins
/// nbits  (uint32_t)        -- number of bitvectors
///        padding to ensure bounds starts on multiple of 8.
/// bounds (double[nobs])    -- bind boundaries
/// maxval (double[nobs])    -- the maximum value in each bin
/// minval (double[nobs])    -- the minimum value in each bin
/// offset (intxx_t[nbits+1])-- starting position of the bitvectors
///@endcode
ibis::bin::bin(const ibis::column* c, const uint32_t nbits,
               ibis::fileManager::storage* st, size_t start)
    : ibis::index(c, st),
      nobs(*(reinterpret_cast<uint32_t*>(st->begin()+start+sizeof(uint32_t)))),
      bounds(st, 8*((7+start+3*sizeof(uint32_t))/8),
             8*((7+start+3*sizeof(uint32_t))/8)+nobs*sizeof(double)),
      maxval(st, 8*((7+start+3*sizeof(uint32_t))/8)+nobs*sizeof(double),
             8*((7+start+3*sizeof(uint32_t))/8)+2*nobs*sizeof(double)),
      minval(st, 8*((7+start+3*sizeof(uint32_t))/8)+2*nobs*sizeof(double),
             8*((7+start+3*sizeof(uint32_t))/8)+3*nobs*sizeof(double)) {
    nrows = *(reinterpret_cast<uint32_t*>(st->begin()+start));
    size_t offpos = 8*((7+start+3*sizeof(uint32_t))/8)+3*nobs*sizeof(double);
    int ierr = initOffsets(st, offpos, nbits);
    if (ierr < 0) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- bin[" << (col ? col->fullname() : 0)
            << "]::bin failed to initialize bitmap offsets from storage "
            << "object @ " << st << " with start = " << start
            << ", ierr = " << ierr;
        throw "bin::ctor failed to initOffsets from storage" IBIS_FILE_LINE;
    }
    initBitmaps(st);

    if (ibis::gVerbose > 2) {
        ibis::util::logger lg;
        lg() << "bin[" << (col ? col->fullname() : "?.?")
             << "]::ctor -- initialization completed with "
             << nobs << " bin" << (nobs>1?"s":"") << " for "
             << nrows << " row" << (nrows>1?"s":"")
             << " from a storage object @ " << st << " offset " << start;
        if (ibis::gVerbose > 6) {
            lg() << "\n";
            print(lg());
        }
    }
} // ibis::bin::bin

/// Reconstruct an object from keys and offsets.
ibis::bin::bin(const ibis::column* c, uint32_t nb, double *keys, int64_t *offs,
               uint32_t *bms)
    : ibis::index(0), nobs(nb) {
    col = c;
    {
        array_t<double> tmp1(keys, nb);
        array_t<double> tmp2(keys+nb, nb);
        tmp1.swap(minval);
        tmp2.swap(maxval);
    }
    bounds.resize(nobs);
    for (unsigned j = 0; j+1 < nb; ++ j) {
        bounds[j] = ibis::util::compactValue(maxval[j], minval[j+1]);
    }
    bounds.back() = DBL_MAX;
    initOffsets(offs, nb+1);

    ibis::fileManager::storage *wrapper =
        new ibis::fileManager::storage(reinterpret_cast<char*>(bms),
                                       static_cast<size_t>(offs[nb]*4));
    initBitmaps(wrapper);
    if (c != 0)
        nrows = c->nRows();

    if (ibis::gVerbose > 2) {
        ibis::util::logger lg;
        lg() << "bin[" << (col ? col->fullname() : "?.?")
             << "]::ctor -- initialization completed with "
             << nobs << " bin" << (nobs>1?"s":"") << " for "
             << nrows << " row" << (nrows>1?"s":"")
             << " with serialized bitmaps @ " << static_cast<void*>(bms);
        if (ibis::gVerbose > 6) {
            lg() << "\n";
            print(lg());
        }
    }
} // ibis::bin::bin

/// Reconstruct an object from keys and offsets.
ibis::bin::bin(const ibis::column* c, uint32_t nb, double *keys, int64_t *offs,
               void *bms, FastBitReadBitmaps rd)
    : ibis::index(0), nobs(nb) {
    col = c;
    {
        array_t<double> tmp1(keys, nb);
        array_t<double> tmp2(keys+nb, nb);
        tmp1.swap(minval);
        tmp2.swap(maxval);
    }
    bounds.resize(nobs);
    for (unsigned j = 0; j+1 < nb; ++ j) {
        bounds[j] = ibis::util::compactValue(maxval[j], minval[j+1]);
    }
    bounds.back() = DBL_MAX;
    initOffsets(offs, nb+1);
    initBitmaps(bms, rd);
    if (c != 0)
        nrows = c->nRows();

    if (ibis::gVerbose > 2) {
        ibis::util::logger lg;
        lg() << "bin[" << (col ? col->fullname() : "?.?")
             << "]::ctor -- initialization completed with "
             << nobs << " bin" << (nobs>1?"s":"") << " for "
             << nrows << " row" << (nrows>1?"s":"")
             << " from a FastBitReadBitmaps object @ " << bms;
        if (ibis::gVerbose > 6) {
            lg() << "\n";
            print(lg());
        }
    }
} // ibis::bin::bin

/// Reconstruct an object from keys and offsets.
ibis::bin::bin(const ibis::column* c, uint32_t nb, double *keys, int64_t *offs)
    : ibis::index(0), nobs(nb) {
    col = c;
    {
        array_t<double> tmp1(keys, nb);
        array_t<double> tmp2(keys+nb, nb);
        tmp1.swap(minval);
        tmp2.swap(maxval);
    }
    bounds.resize(nobs);
    for (unsigned j = 0; j+1 < nb; ++ j) {
        bounds[j] = ibis::util::compactValue(maxval[j], minval[j+1]);
    }
    bounds.back() = DBL_MAX;
    initOffsets(offs, nb+1);
    if (c != 0)
        nrows = c->nRows();

    if (ibis::gVerbose > 2) {
        ibis::util::logger lg;
        lg() << "bin[" << (col ? col->fullname() : "?.?")
             << "]::ctor -- initialization completed with "
             << nobs << " bin" << (nobs>1?"s":"") << " for "
             << nrows << " row" << (nrows>1?"s":"");
        if (ibis::gVerbose > 6) {
            lg() << "\n";
            print(lg());
        }
    }
} // ibis::bin::bin

ibis::index* ibis::bin::dup() const {
    return new ibis::bin(*this);
} // ibis::bin::dup

/// Read from a file named f.
int ibis::bin::read(const char* f) {
    std::string fnm;
    indexFileName(fnm, f);
    int fdes = UnixOpen(fnm.c_str(), OPEN_READONLY);
    if (fdes < 0)
        return -1;
    IBIS_BLOCK_GUARD(UnixClose, fdes);
#if defined(_WIN32) && defined(_MSC_VER)
    (void)_setmode(fdes, _O_BINARY);
#endif
    char header[8];
    if (8 != UnixRead(fdes, static_cast<void*>(header), 8)) {
        return -2;
    }

    if (!(header[0] == '#' && header[1] == 'I' &&
          header[2] == 'B' && header[3] == 'I' &&
          header[4] == 'S' &&
          (header[6] == 4 || header[6] == 8) &&
          header[7] == static_cast<char>(0))) {
        if (ibis::gVerbose > 0) {
            ibis::util::logger lg;
            lg() << "Warning -- bin[" << (col ? col->fullname() : "?.?")
                 << "]::read the header from " << fnm << " (";
            printHeader(lg(), header);
            lg() << ") does not contain the expected values";
        }
        return -3;
    }

    size_t begin, end;
    clear(); // clear the existing content
    fname = ibis::util::strnewdup(fnm.c_str());
    str = 0;

    long ierr = UnixRead(fdes, static_cast<void*>(&nrows), sizeof(uint32_t));
    if (ierr < static_cast<int>(sizeof(uint32_t))) {
        nrows = 0;
        return -4;
    }
    ierr = UnixRead(fdes, static_cast<void*>(&nobs), sizeof(uint32_t));
    if (ierr < static_cast<int>(sizeof(uint32_t))) {
        nrows = 0;
        nobs = 0;
        return -5;
    }
    begin = 8+2*sizeof(uint32_t);
    end = 8+2*sizeof(uint32_t)+(nobs+1)*header[6];
    ierr = initOffsets(fdes, header[6], begin, nobs);
    if (ierr < 0)
        return ierr;

    // read bounds
    begin = 8 * ((7 + end)/8);
    end = begin + sizeof(double)*nobs;
    {
        array_t<double> dbl(fname, fdes, begin, end);
        bounds.swap(dbl);
    }

    // read maxval
    begin = end;
    end += sizeof(double) * nobs;
    {
        array_t<double> dbl(fname, fdes, begin, end);
        maxval.swap(dbl);
    }

    // read minval
    begin = end;
    end += sizeof(double) * nobs;
    {
        array_t<double> dbl(fname, fdes, begin, end);
        minval.swap(dbl);
    }
    ibis::fileManager::instance().recordPages(0, end);
    initBitmaps(fdes);

    LOGGER(ibis::gVerbose > 3)
        << "bin[" << (col ? col->fullname() : "?.?") << "]::read(" << fnm
        << ") finished reading index header (type " << (int) header[5]
        << ") with nrows=" << nrows << " and nobs=" << nobs;
    return 0;
} // ibis::bin::read

/// Read from a file starting from an arbitrary @c start position.  This is
/// intended to be used by multi-level indices.  The size of bitmap offsets
/// are defined in header[6] and full index type is defined in header[5].
int ibis::bin::read(int fdes, size_t start,
                    const char *fn, const char *header) {
    if (fdes < 0) return -1;
    if (start != static_cast<size_t>(UnixSeek(fdes, start, SEEK_SET)))
        return -4;

    size_t begin, end;
    clear(); // clear the existing content
    if (fn != 0 && *fn != 0) {
        fname = ibis::util::strnewdup(fn);
    }
    else {
        fname = 0;
    }
    str = 0;

    off_t ierr = UnixRead(fdes, static_cast<void*>(&nrows), sizeof(uint32_t));
    if (ierr < static_cast<int>(sizeof(uint32_t))) {
        nrows = 0;
        return -3;
    }
    ierr = UnixRead(fdes, static_cast<void*>(&nobs), sizeof(uint32_t));
    if (ierr < static_cast<int>(sizeof(uint32_t))) {
        nrows = 0;
        nobs = 0;
        return -4;
    }
    begin = start + 2 * sizeof(uint32_t);
    end = start + 2*sizeof(uint32_t) + header[6]*(nobs+1);
    ierr = initOffsets(fdes, header[6], begin, nobs);
    if (ierr != 0)
        return ierr;

    // read bounds
    begin = 8 * ((end+7)/8);
    end = begin + sizeof(double)*nobs;
    {
        array_t<double> dbl(fname, fdes, begin, end);
        bounds.swap(dbl);
    }

    // read maxval
    begin = end;
    end += sizeof(double) * nobs;
    {
        array_t<double> dbl(fname, fdes, begin, end);
        maxval.swap(dbl);
    }

    // read minval
    begin = end;
    end += sizeof(double) * nobs;
    {
        array_t<double> dbl(fname, fdes, begin, end);
        minval.swap(dbl);
    }
    ibis::fileManager::instance().recordPages(start, end);
    initBitmaps(fdes);

    LOGGER(ibis::gVerbose > 3)
        << "bin[" << (col ? col->fullname() : "?.?") << "]::read(" << fdes
        << ", " << start << ") finished reading index header (type "
        << (int) header[5] << ") with nrows=" << nrows << " and nobs=" << nobs;
    return 0;
} // ibis::bin::read

/// Read from a reference counted piece of memory.
int ibis::bin::read(ibis::fileManager::storage* st) {
    if (st == 0) return -1;
    clear(); // clear the existing content
    str = st;

    nrows = *(reinterpret_cast<uint32_t*>(st->begin()+8));
    nobs = *(reinterpret_cast<uint32_t*>(st->begin()+8+sizeof(nrows)));
    uint32_t begin;
    begin = 8*(((*st)[6]*(nobs+1)+2*sizeof(uint32_t)+15)/8);
    { // boundds
        array_t<double> dbl(st, begin, begin+sizeof(double)*nobs);
        bounds.swap(dbl);
    }
    begin += sizeof(double) * nobs;
    { // maxval
        array_t<double> dbl(st, begin, begin+sizeof(double)*nobs);
        maxval.swap(dbl);
    }
    begin += sizeof(double) * nobs;
    { // minval
        array_t<double> dbl(st, begin, begin+sizeof(double)*nobs);
        minval.swap(dbl);
    }

    int ierr = initOffsets(st, 8+2*sizeof(uint32_t), nobs);
    if (ierr < 0)
        return ierr;
    initBitmaps(st);

    LOGGER(ibis::gVerbose > 3)
        << "bin[" << (col ? col->fullname() : "?.?") << "]::read(" << st
        << ") finished reading index header (type " << (int) (*st)[5]
        << ") with nrows=" << nrows << " and nobs=" << nobs;
    return 0;
} // ibis::bin::read

/// Fill the bitvectors with zeros so that they all contain nrows bits.
/// Truncate the bitvectors if they have more bits.
void ibis::bin::adjustLength(uint32_t nr) {
    if (nr == nrows) return;
    nrows = nr;
    array_t<bitvector*>::iterator it;
    for (it = bits.begin(); it != bits.end(); ++it) {
        if (*it) {
            (*it)->adjustSize(0, nr);
        }
    }
} // ibis::bin::adjustLength

/// Find the smallest i such that bounds[i] > val.
uint32_t ibis::bin::locate(const double& val) const {
#if DEBUG+0 > 1 || _DEBUG+0 > 1
    LOGGER(ibis::gVerbose > 1)
        << "bin::locate -- searching for " << val << " in an array of "
        << bounds.size() << " doubles in the range of [" << bounds.front()
        << ", " << bounds.back() << "]";
#endif
    // check the extreme cases -- use negative tests to capture abnormal
    // numbers
    if (bounds.empty()) {
        return 0;
    }
    else if (! (val >= bounds[0])) {
        return 0;
    }
    else if (! (val < bounds[nobs-1])) {
        if (bounds[nobs-1] < DBL_MAX)
            return nobs;
        else
            return nobs - 1;
    }

    // the normal cases -- two different search strategies
    if (nobs >= 8) { // binary search
        uint32_t i0 = 0, i1 = nobs, it = nobs/2;
        while (i0 < it) { // bounds[i1] >= val
            if (val < bounds[it])
                i1 = it;
            else
                i0 = it;
            it = (i0 + i1) / 2;
        }
#if DEBUG+0 > 1 || _DEBUG+0 > 1
        LOGGER(ibis::gVerbose > 5)
            << "bin::locate -- element " << i1 << " (" << bounds[i1]
            << ") out of " << bounds.size() << " is no less than " << val;
#endif
        LOGGER(ibis::gVerbose > 10)
            << "column[" << (col ? col->fullname() : "?.?")
            << "]::bin::locate -- "
            << std::setprecision(16) << val << " in ["
            << std::setprecision(16) << bounds[i0] << ", "
            << std::setprecision(16) << bounds[i1] << ") ==> " << i1;
        return i1;
    }
    else { // do linear search
        for (uint32_t i = 1; i < nobs; ++i) {
            if (val < bounds[i]) {
#if DEBUG+0 > 0 || _DEBUG+0 > 1
                LOGGER(ibis::gVerbose > 5)
                    << "bin[" << (col ? col->fullname() : "?.?")
                    << "]::locate -- element " << i << " (" << bounds[i]
                    << ") out of " << bounds.size() << " is no less than "
                    << val;
#endif
                LOGGER(ibis::gVerbose > 10)
                    << "column[" << (col ? col->fullname() : "?.?")
                    << "]::bin::locate -- " << std::setprecision(16) << val
                    << " in [" << std::setprecision(16) << bounds[i-1] << ", "
                    << std::setprecision(16) << bounds[i] << ") ==> " << i;
                return i;
            }
        }
    }
    return nobs;
} // ibis::bin::locate

/// This version of the binning function takes an external specified bin
/// boundaries -- if the array is too small to be valid, it uses the default
/// option.
/// @note This function does not attempt to clear the content of the
/// current data structure, the caller is responsible for this task!
void ibis::bin::binning(const char* f, const std::vector<double>& bd) {
    if (col == 0) return;
    if (bd.size() <= 2) {
        // bd has no valid values, parse bin spec in a minimal way
        setBoundaries(f);
    }
    else {
        bounds.resize(bd.size());
        for (uint32_t i = 0; i < bd.size(); ++ i)
            bounds[i] = bd[i];
        if (bounds.back() < DBL_MAX)
            bounds.push_back(DBL_MAX);
        nobs = bounds.size();
    }

    //binning(f); // binning without writing reordered values
    switch (col->type()) { // binning with reordering
    case ibis::DOUBLE:
        binningT<double>(f);
        break;
    case ibis::FLOAT:
        binningT<float>(f);
        break;
    case ibis::ULONG:
        binningT<uint64_t>(f);
        break;
    case ibis::LONG:
        binningT<int64_t>(f);
        break;
    case ibis::UINT:
        binningT<uint32_t>(f);
        break;
    case ibis::INT:
        binningT<int32_t>(f);
        break;
    case ibis::USHORT:
        binningT<uint16_t>(f);
        break;
    case ibis::SHORT:
        binningT<int16_t>(f);
        break;
    case ibis::UBYTE:
        binningT<unsigned char>(f);
        break;
    case ibis::BYTE:
        binningT<signed char>(f);
        break;
    default:
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- failed to bin column " << col->name()
            << " (type " << (int)(col->type()) << ", "
            << ibis::TYPESTRING[(int)(col->type())] << ')';
        throw ibis::bad_alloc("Unexpected data type for bin::binning"
                              IBIS_FILE_LINE);
    }
} // ibis::bin::binning (specified bin boundaries)

void ibis::bin::binning(const char* f, const array_t<double>& bd) {
    if (col == 0) return;
    if (bd.size() <= 2) {
        // bd has no valid values, parse bin spec in a minimal way
        setBoundaries(f);
    }
    else {
        bounds.deepCopy(bd);
        if (bounds.back() < DBL_MAX)
            bounds.push_back(DBL_MAX);
        nobs = bounds.size();
    }

    // binning(f); // binning without reordering
    switch (col->type()) { // binning with reordering
    case ibis::DOUBLE:
        binningT<double>(f);
        break;
    case ibis::FLOAT:
        binningT<float>(f);
        break;
    case ibis::ULONG:
        binningT<uint64_t>(f);
        break;
    case ibis::LONG:
        binningT<int64_t>(f);
        break;
    case ibis::UINT:
        binningT<uint32_t>(f);
        break;
    case ibis::INT:
        binningT<int32_t>(f);
        break;
    case ibis::USHORT:
        binningT<uint16_t>(f);
        break;
    case ibis::SHORT:
        binningT<int16_t>(f);
        break;
    case ibis::UBYTE:
        binningT<unsigned char>(f);
        break;
    case ibis::BYTE:
        binningT<signed char>(f);
        break;
    default:
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- failed to bin column " << col->name()
            << " (type " << (int)(col->type()) << ", "
            << ibis::TYPESTRING[(int)(col->type())] << ')';
        throw ibis::bad_alloc("Unexpected data type for bin::binning"
                              IBIS_FILE_LINE);
    }
} // ibis::bin::binning (specified bin boundaries)

/// This function actually reads the values of the data file and produces
/// the bitvectors for each bin.  The caller must have setup the bounds
/// already.
void ibis::bin::binning(const char* f) {
    if (col == 0) return;

    horometer timer;
    if (ibis::gVerbose > 4)
        timer.start();

    ibis::index::clear(); // delete the existing bitmaps
    // allocate space for min and max values and initialize them to extreme
    // values
    bits.resize(nobs);
    maxval.resize(nobs);
    minval.resize(nobs);
    for (uint32_t i = 0; i < nobs; ++i) {
        maxval[i] = -DBL_MAX;
        minval[i] = DBL_MAX;
        bits[i] = new ibis::bitvector;
    }

    std::string fnm; // name of the data file
    dataFileName(fnm, f);
    if (fnm.empty()) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- bin::binning failed to determine the data file "
            "name from \"" << (f ? f : "") << '"';
        return;
    }

    ibis::bitvector mask;
    col->getNullMask(mask);
    if (col->partition() != 0)
        nrows = col->partition()->nRows();
    else
        nrows = mask.size();
    if (nrows == 0) return;

    // need to do different things for different columns
    switch (col->type()) {
    case ibis::UINT: {// unsigned int
        array_t<uint32_t> val;
        if (! fnm.empty())
            ibis::fileManager::instance().getFile(fnm.c_str(), val);
        else
            col->getValuesArray(&val);
        if (val.size() <= 0) {
            col->logWarning("bin::binning", "failed to read %s",
                            fnm.c_str());
            throw ibis::bad_alloc("fail to read data file" IBIS_FILE_LINE);
        }
        else {
            nrows = val.size();
            if (nrows > mask.size())
                mask.adjustSize(nrows, nrows);
            ibis::bitvector::indexSet iset = mask.firstIndexSet();
            uint32_t nind = iset.nIndices();
            const ibis::bitvector::word_t *iix = iset.indices();
            while (nind) {
                if (iset.isRange()) { // a range
                    uint32_t k = (iix[1] < nrows ? iix[1] : nrows);
                    for (uint32_t i = *iix; i < k; ++i) {
                        uint32_t j = locate(val[i]);
                        if (j < nobs) {
                            bits[j]->setBit(i, 1);
                            if (minval[j] > val[i])
                                minval[j] = val[i];
                            if (maxval[j] < val[i])
                                maxval[j] = val[i];
                        }
                    }
                }
                else if (*iix+ibis::bitvector::bitsPerLiteral() < nrows) {
                    // a list of indices
                    for (uint32_t i = 0; i < nind; ++i) {
                        uint32_t k = iix[i];
                        uint32_t j = locate(val[k]);
                        if (j < nobs) {
                            bits[j]->setBit(k, 1);
                            if (minval[j] > val[k])
                                minval[j] = val[k];
                            if (maxval[j] < val[k])
                                maxval[j] = val[k];
                        }
                    }
                }
                else {
                    for (uint32_t i = 0; i < nind; ++i) {
                        uint32_t k = iix[i];
                        if (k < nrows) {
                            uint32_t j = locate(val[k]);
                            if (j < nobs) {
                                bits[j]->setBit(k, 1);
                                if (minval[j] > val[k])
                                    minval[j] = val[k];
                                if (maxval[j] < val[k])
                                    maxval[j] = val[k];
                            }
                        }
                    }
                }
                ++iset;
                nind = iset.nIndices();
                if (*iix >= nrows) nind = 0;
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
            col->logWarning("bin::binning", "failed to read %s",
                            fnm.c_str());
            throw ibis::bad_alloc("fail to read data file" IBIS_FILE_LINE);
        }
        else {
            nrows = val.size();
            if (nrows > mask.size())
                mask.adjustSize(nrows, nrows);
            ibis::bitvector::indexSet iset = mask.firstIndexSet();
            uint32_t nind = iset.nIndices();
            const ibis::bitvector::word_t *iix = iset.indices();
            while (nind) {
                if (iset.isRange()) { // a range
                    uint32_t k = (iix[1] < nrows ? iix[1] : nrows);
                    for (uint32_t i = *iix; i < k; ++i) {
                        uint32_t j = locate(val[i]);
                        if (j < nobs) {
                            bits[j]->setBit(i, 1);
                            if (minval[j] > val[i])
                                minval[j] = val[i];
                            if (maxval[j] < val[i])
                                maxval[j] = val[i];
                        }
                    }
                }
                else if (*iix+ibis::bitvector::bitsPerLiteral() < nrows) {
                    // a list of indices
                    for (uint32_t i = 0; i < nind; ++i) {
                        uint32_t k = iix[i];
                        uint32_t j = locate(val[k]);
                        if (j < nobs) {
                            bits[j]->setBit(k, 1);
                            if (minval[j] > val[k])
                                minval[j] = val[k];
                            if (maxval[j] < val[k])
                                maxval[j] = val[k];
                        }
                    }
                }
                else {
                    for (uint32_t i = 0; i < nind; ++i) {
                        uint32_t k = iix[i];
                        if (k < nrows) {
                            uint32_t j = locate(val[k]);
                            if (j < nobs) {
                                bits[j]->setBit(k, 1);
                                if (minval[j] > val[k])
                                    minval[j] = val[k];
                                if (maxval[j] < val[k])
                                    maxval[j] = val[k];
                            }
                        }
                    }
                }
                ++iset;
                nind = iset.nIndices();
                if (*iix >= nrows) nind = 0;
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
            col->logWarning("bin::binning", "failed to read %s",
                            fnm.c_str());
            throw ibis::bad_alloc("fail to read data file" IBIS_FILE_LINE);
        }
        else {
            nrows = val.size();
            if (nrows > mask.size())
                mask.adjustSize(nrows, nrows);
            ibis::bitvector::indexSet iset = mask.firstIndexSet();
            uint32_t nind = iset.nIndices();
            const ibis::bitvector::word_t *iix = iset.indices();
            while (nind) {
                if (iset.isRange()) { // a range
                    uint32_t k = (iix[1] < nrows ? iix[1] : nrows);
                    for (uint32_t i = *iix; i < k; ++i) {
                        uint32_t j = locate(val[i]);
#if defined(DBUG) || defined(_DEBUG)
                        LOGGER(ibis::gVerbose > 8 && i%1000==0)
                            << "DEBUG -- binning val[" << i << "] = "
                            << val[i] << " ==> bin " << j
                            << (j<nobs?"":" ***out-of-range***");
#endif
                        if (j < nobs) {
                            bits[j]->setBit(i, 1);
                            if (minval[j] > val[i])
                                minval[j] = val[i];
                            if (maxval[j] < val[i])
                                maxval[j] = val[i];
                        }
                    }
                }
                else if (*iix+ibis::bitvector::bitsPerLiteral() < nrows) {
                    // a list of indices
                    for (uint32_t i = 0; i < nind; ++i) {
                        uint32_t k = iix[i];
                        uint32_t j = locate(val[k]);
                        if (j < nobs) {
                            bits[j]->setBit(k, 1);
                            if (minval[j] > val[k])
                                minval[j] = val[k];
                            if (maxval[j] < val[k])
                                maxval[j] = val[k];
                        }
                    }
                }
                else {
                    for (uint32_t i = 0; i < nind; ++i) {
                        uint32_t k = iix[i];
                        if (k < nrows) {
                            uint32_t j = locate(val[k]);
                            if (j < nobs) {
                                bits[j]->setBit(k, 1);
                                if (minval[j] > val[k])
                                    minval[j] = val[k];
                                if (maxval[j] < val[k])
                                    maxval[j] = val[k];
                            }
                        }
                    }
                }
                ++iset;
                nind = iset.nIndices();
                if (*iix >= nrows) nind = 0;
            } // while (nind)

            // reset the nominal boundaries of the bins
            for (uint32_t i = 0; i < nobs-1; ++ i) {
                if (minval[i+1] < DBL_MAX && maxval[i] > -DBL_MAX)
                    bounds[i] = ibis::util::compactValue
                        (maxval[i], minval[i+1]);
            }
        }
        break;}
    case ibis::DOUBLE: {// (8-byte) floating-point values
        array_t<double> val;
        if (! fnm.empty())
            ibis::fileManager::instance().getFile(fnm.c_str(), val);
        else
            col->getValuesArray(&val);
        if (val.size() <= 0) {
            col->logWarning("bin::binning", "failed to read %s",
                            fnm.c_str());
            throw ibis::bad_alloc("fail to read data file" IBIS_FILE_LINE);
        }
        else {
            nrows = val.size();
            if (nrows > mask.size())
                mask.adjustSize(nrows, nrows);
            ibis::bitvector::indexSet iset = mask.firstIndexSet();
            uint32_t nind = iset.nIndices();
            const ibis::bitvector::word_t *iix = iset.indices();
            while (nind) {
                if (iset.isRange()) { // a range
                    uint32_t k = (iix[1] < nrows ? iix[1] : nrows);
                    for (uint32_t i = *iix; i < k; ++i) {
                        uint32_t j = locate(val[i]);
                        if (j < nobs) {
                            bits[j]->setBit(i, 1);
                            if (minval[j] > val[i])
                                minval[j] = val[i];
                            if (maxval[j] < val[i])
                                maxval[j] = val[i];
                        }
                    }
                }
                else if (*iix+ibis::bitvector::bitsPerLiteral() < nrows) {
                    // a list of indices
                    for (uint32_t i = 0; i < nind; ++i) {
                        uint32_t k = iix[i];
                        uint32_t j = locate(val[k]);
                        if (j < nobs) {
                            bits[j]->setBit(k, 1);
                            if (minval[j] > val[k])
                                minval[j] = val[k];
                            if (maxval[j] < val[k])
                                maxval[j] = val[k];
                        }
                    }
                }
                else {
                    for (uint32_t i = 0; i < nind; ++i) {
                        uint32_t k = iix[i];
                        if (k < nrows) {
                            uint32_t j = locate(val[k]);
                            if (j < nobs) {
                                bits[j]->setBit(k, 1);
                                if (minval[j] > val[k])
                                    minval[j] = val[k];
                                if (maxval[j] < val[k])
                                    maxval[j] = val[k];
                            }
                        }
                    }
                }
                ++iset;
                nind = iset.nIndices();
                if (*iix >= nrows) nind = 0;
            } // while (nind)

            // reset the nominal boundaries of the bins
            for (uint32_t i = 0; i < nobs-1; ++ i) {
                if (minval[i+1] < DBL_MAX && maxval[i] > -DBL_MAX)
                    bounds[i] = ibis::util::compactValue
                        (maxval[i], minval[i+1]);
            }
        }
        break;}
    case ibis::BYTE: {// (1-byte) integer values
        array_t<char> val;
        if (! fnm.empty())
            ibis::fileManager::instance().getFile(fnm.c_str(), val);
        else
            col->getValuesArray(&val);
        if (val.size() <= 0) {
            col->logWarning("bin::binning", "failed to read %s",
                            fnm.c_str());
            throw ibis::bad_alloc("fail to read data file" IBIS_FILE_LINE);
        }
        else {
            nrows = val.size();
            if (nrows > mask.size())
                mask.adjustSize(nrows, nrows);
            ibis::bitvector::indexSet iset = mask.firstIndexSet();
            uint32_t nind = iset.nIndices();
            const ibis::bitvector::word_t *iix = iset.indices();
            while (nind) {
                if (iset.isRange()) { // a range
                    uint32_t k = (iix[1] < nrows ? iix[1] : nrows);
                    for (uint32_t i = *iix; i < k; ++i) {
                        uint32_t j = locate(val[i]);
                        if (j < nobs) {
                            bits[j]->setBit(i, 1);
                            if (minval[j] > val[i])
                                minval[j] = val[i];
                            if (maxval[j] < val[i])
                                maxval[j] = val[i];
                        }
                    }
                }
                else if (*iix+ibis::bitvector::bitsPerLiteral() < nrows) {
                    // a list of indices
                    for (uint32_t i = 0; i < nind; ++i) {
                        uint32_t k = iix[i];
                        uint32_t j = locate(val[k]);
                        if (j < nobs) {
                            bits[j]->setBit(k, 1);
                            if (minval[j] > val[k])
                                minval[j] = val[k];
                            if (maxval[j] < val[k])
                                maxval[j] = val[k];
                        }
                    }
                }
                else {
                    for (uint32_t i = 0; i < nind; ++i) {
                        uint32_t k = iix[i];
                        if (k < nrows) {
                            uint32_t j = locate(val[k]);
                            if (j < nobs) {
                                bits[j]->setBit(k, 1);
                                if (minval[j] > val[k])
                                    minval[j] = val[k];
                                if (maxval[j] < val[k])
                                    maxval[j] = val[k];
                            }
                        }
                    }
                }
                ++iset;
                nind = iset.nIndices();
                if (*iix >= nrows) nind = 0;
            } // while (nind)

            // reset the nominal boundaries of the bins
            for (uint32_t i = 0; i < nobs-1; ++ i) {
                if (minval[i+1] < DBL_MAX && maxval[i] > -DBL_MAX)
                    bounds[i] = ibis::util::compactValue
                        (maxval[i], minval[i+1]);
            }
        }
        break;}
    case ibis::UBYTE: {// (1-byte) integer values
        array_t<unsigned char> val;
        if (! fnm.empty())
            ibis::fileManager::instance().getFile(fnm.c_str(), val);
        else
            col->getValuesArray(&val);
        if (val.size() <= 0) {
            col->logWarning("bin::binning", "failed to read %s",
                            fnm.c_str());
            throw ibis::bad_alloc("fail to read data file" IBIS_FILE_LINE);
        }
        else {
            nrows = val.size();
            if (nrows > mask.size())
                mask.adjustSize(nrows, nrows);
            ibis::bitvector::indexSet iset = mask.firstIndexSet();
            uint32_t nind = iset.nIndices();
            const ibis::bitvector::word_t *iix = iset.indices();
            while (nind) {
                if (iset.isRange()) { // a range
                    uint32_t k = (iix[1] < nrows ? iix[1] : nrows);
                    for (uint32_t i = *iix; i < k; ++i) {
                        uint32_t j = locate(val[i]);
                        if (j < nobs) {
                            bits[j]->setBit(i, 1);
                            if (minval[j] > val[i])
                                minval[j] = val[i];
                            if (maxval[j] < val[i])
                                maxval[j] = val[i];
                        }
                    }
                }
                else if (*iix+ibis::bitvector::bitsPerLiteral() < nrows) {
                    // a list of indices
                    for (uint32_t i = 0; i < nind; ++i) {
                        uint32_t k = iix[i];
                        uint32_t j = locate(val[k]);
                        if (j < nobs) {
                            bits[j]->setBit(k, 1);
                            if (minval[j] > val[k])
                                minval[j] = val[k];
                            if (maxval[j] < val[k])
                                maxval[j] = val[k];
                        }
                    }
                }
                else {
                    for (uint32_t i = 0; i < nind; ++i) {
                        uint32_t k = iix[i];
                        if (k < nrows) {
                            uint32_t j = locate(val[k]);
                            if (j < nobs) {
                                bits[j]->setBit(k, 1);
                                if (minval[j] > val[k])
                                    minval[j] = val[k];
                                if (maxval[j] < val[k])
                                    maxval[j] = val[k];
                            }
                        }
                    }
                }
                ++iset;
                nind = iset.nIndices();
                if (*iix >= nrows) nind = 0;
            } // while (nind)

            // reset the nominal boundaries of the bins
            for (uint32_t i = 0; i < nobs-1; ++ i) {
                if (minval[i+1] < DBL_MAX && maxval[i] > -DBL_MAX)
                    bounds[i] = ibis::util::compactValue
                        (maxval[i], minval[i+1]);
            }
        }
        break;}
    case ibis::SHORT: {// (2-byte) integer values
        array_t<int16_t> val;
        if (! fnm.empty())
            ibis::fileManager::instance().getFile(fnm.c_str(), val);
        else
            col->getValuesArray(&val);
        if (val.size() <= 0) {
            col->logWarning("bin::binning", "failed to read %s",
                            fnm.c_str());
            throw ibis::bad_alloc("fail to read data file" IBIS_FILE_LINE);
        }
        else {
            nrows = val.size();
            if (nrows > mask.size())
                mask.adjustSize(nrows, nrows);
            ibis::bitvector::indexSet iset = mask.firstIndexSet();
            uint32_t nind = iset.nIndices();
            const ibis::bitvector::word_t *iix = iset.indices();
            while (nind) {
                if (iset.isRange()) { // a range
                    uint32_t k = (iix[1] < nrows ? iix[1] : nrows);
                    for (uint32_t i = *iix; i < k; ++i) {
                        uint32_t j = locate(val[i]);
                        if (j < nobs) {
                            bits[j]->setBit(i, 1);
                            if (minval[j] > val[i])
                                minval[j] = val[i];
                            if (maxval[j] < val[i])
                                maxval[j] = val[i];
                        }
                    }
                }
                else if (*iix+ibis::bitvector::bitsPerLiteral() < nrows) {
                    // a list of indices
                    for (uint32_t i = 0; i < nind; ++i) {
                        uint32_t k = iix[i];
                        uint32_t j = locate(val[k]);
                        if (j < nobs) {
                            bits[j]->setBit(k, 1);
                            if (minval[j] > val[k])
                                minval[j] = val[k];
                            if (maxval[j] < val[k])
                                maxval[j] = val[k];
                        }
                    }
                }
                else {
                    for (uint32_t i = 0; i < nind; ++i) {
                        uint32_t k = iix[i];
                        if (k < nrows) {
                            uint32_t j = locate(val[k]);
                            if (j < nobs) {
                                bits[j]->setBit(k, 1);
                                if (minval[j] > val[k])
                                    minval[j] = val[k];
                                if (maxval[j] < val[k])
                                    maxval[j] = val[k];
                            }
                        }
                    }
                }
                ++iset;
                nind = iset.nIndices();
                if (*iix >= nrows) nind = 0;
            } // while (nind)

            // reset the nominal boundaries of the bins
            for (uint32_t i = 0; i < nobs-1; ++ i) {
                if (minval[i+1] < DBL_MAX && maxval[i] > -DBL_MAX)
                    bounds[i] = ibis::util::compactValue
                        (maxval[i], minval[i+1]);
            }
        }
        break;}
    case ibis::USHORT: {// (2-byte) integer values
        array_t<uint16_t> val;
        if (! fnm.empty())
            ibis::fileManager::instance().getFile(fnm.c_str(), val);
        else
            col->getValuesArray(&val);
        if (val.size() <= 0) {
            col->logWarning("bin::binning", "failed to read %s",
                            fnm.c_str());
            throw ibis::bad_alloc("fail to read data file" IBIS_FILE_LINE);
        }
        else {
            nrows = val.size();
            if (nrows > mask.size())
                mask.adjustSize(nrows, nrows);
            ibis::bitvector::indexSet iset = mask.firstIndexSet();
            uint32_t nind = iset.nIndices();
            const ibis::bitvector::word_t *iix = iset.indices();
            while (nind) {
                if (iset.isRange()) { // a range
                    uint32_t k = (iix[1] < nrows ? iix[1] : nrows);
                    for (uint32_t i = *iix; i < k; ++i) {
                        uint32_t j = locate(val[i]);
                        if (j < nobs) {
                            bits[j]->setBit(i, 1);
                            if (minval[j] > val[i])
                                minval[j] = val[i];
                            if (maxval[j] < val[i])
                                maxval[j] = val[i];
                        }
                    }
                }
                else if (*iix+ibis::bitvector::bitsPerLiteral() < nrows) {
                    // a list of indices
                    for (uint32_t i = 0; i < nind; ++i) {
                        uint32_t k = iix[i];
                        uint32_t j = locate(val[k]);
                        if (j < nobs) {
                            bits[j]->setBit(k, 1);
                            if (minval[j] > val[k])
                                minval[j] = val[k];
                            if (maxval[j] < val[k])
                                maxval[j] = val[k];
                        }
                    }
                }
                else {
                    for (uint32_t i = 0; i < nind; ++i) {
                        uint32_t k = iix[i];
                        if (k < nrows) {
                            uint32_t j = locate(val[k]);
                            if (j < nobs) {
                                bits[j]->setBit(k, 1);
                                if (minval[j] > val[k])
                                    minval[j] = val[k];
                                if (maxval[j] < val[k])
                                    maxval[j] = val[k];
                            }
                        }
                    }
                }
                ++iset;
                nind = iset.nIndices();
                if (*iix >= nrows) nind = 0;
            } // while (nind)

            // reset the nominal boundaries of the bins
            for (uint32_t i = 0; i < nobs-1; ++ i) {
                if (minval[i+1] < DBL_MAX && maxval[i] > -DBL_MAX)
                    bounds[i] = ibis::util::compactValue
                        (maxval[i], minval[i+1]);
            }
        }
        break;}
    case ibis::LONG: {// (8-byte) integer values
        array_t<int64_t> val;
        if (! fnm.empty())
            ibis::fileManager::instance().getFile(fnm.c_str(), val);
        else
            col->getValuesArray(&val);
        if (val.size() <= 0) {
            col->logWarning("bin::binning", "failed to read %s",
                            fnm.c_str());
            throw ibis::bad_alloc("fail to read data file" IBIS_FILE_LINE);
        }
        else {
            nrows = val.size();
            if (nrows > mask.size())
                mask.adjustSize(nrows, nrows);
            ibis::bitvector::indexSet iset = mask.firstIndexSet();
            uint32_t nind = iset.nIndices();
            const ibis::bitvector::word_t *iix = iset.indices();
            while (nind) {
                if (iset.isRange()) { // a range
                    uint32_t k = (iix[1] < nrows ? iix[1] : nrows);
                    for (uint32_t i = *iix; i < k; ++i) {
                        uint32_t j = locate(val[i]);
                        if (j < nobs) {
                            bits[j]->setBit(i, 1);
                            if (minval[j] > static_cast<double>(val[i]))
                                minval[j] = static_cast<double>(val[i]);
                            if (maxval[j] < static_cast<double>(val[i]))
                                maxval[j] = static_cast<double>(val[i]);
                        }
                    }
                }
                else if (*iix+ibis::bitvector::bitsPerLiteral() < nrows) {
                    // a list of indices
                    for (uint32_t i = 0; i < nind; ++i) {
                        uint32_t k = iix[i];
                        uint32_t j = locate(val[k]);
                        if (j < nobs) {
                            bits[j]->setBit(k, 1);
                            if (minval[j] > static_cast<double>(val[k]))
                                minval[j] = static_cast<double>(val[k]);
                            if (maxval[j] < static_cast<double>(val[k]))
                                maxval[j] = static_cast<double>(val[k]);
                        }
                    }
                }
                else {
                    for (uint32_t i = 0; i < nind; ++i) {
                        uint32_t k = iix[i];
                        if (k < nrows) {
                            uint32_t j = locate(val[k]);
                            if (j < nobs) {
                                bits[j]->setBit(k, 1);
                                if (minval[j] > static_cast<double>(val[k]))
                                    minval[j] = static_cast<double>(val[k]);
                                if (maxval[j] < static_cast<double>(val[k]))
                                    maxval[j] = static_cast<double>(val[k]);
                            }
                        }
                    }
                }
                ++iset;
                nind = iset.nIndices();
                if (*iix >= nrows) nind = 0;
            } // while (nind)

            // reset the nominal boundaries of the bins
            for (uint32_t i = 0; i < nobs-1; ++ i) {
                if (minval[i+1] < DBL_MAX && maxval[i] > -DBL_MAX)
                    bounds[i] = ibis::util::compactValue
                        (maxval[i], minval[i+1]);
            }
        }
        break;}
    case ibis::ULONG: {// (8-byte) integer values
        array_t<uint64_t> val;
        if (! fnm.empty())
            ibis::fileManager::instance().getFile(fnm.c_str(), val);
        else
            col->getValuesArray(&val);
        if (val.size() <= 0) {
            col->logWarning("bin::binning", "failed to read %s",
                            fnm.c_str());
            throw ibis::bad_alloc("fail to read data file" IBIS_FILE_LINE);
        }
        else {
            nrows = val.size();
            if (nrows > mask.size())
                mask.adjustSize(nrows, nrows);
            ibis::bitvector::indexSet iset = mask.firstIndexSet();
            uint32_t nind = iset.nIndices();
            const ibis::bitvector::word_t *iix = iset.indices();
            while (nind) {
                if (iset.isRange()) { // a range
                    uint32_t k = (iix[1] < nrows ? iix[1] : nrows);
                    for (uint32_t i = *iix; i < k; ++i) {
                        uint32_t j = locate(val[i]);
                        if (j < nobs) {
                            bits[j]->setBit(i, 1);
                            if (minval[j] > static_cast<double>(val[i]))
                                minval[j] = static_cast<double>(val[i]);
                            if (maxval[j] < static_cast<double>(val[i]))
                                maxval[j] = static_cast<double>(val[i]);
                        }
                    }
                }
                else if (*iix+ibis::bitvector::bitsPerLiteral() < nrows) {
                    // a list of indices
                    for (uint32_t i = 0; i < nind; ++i) {
                        uint32_t k = iix[i];
                        uint32_t j = locate(val[k]);
                        if (j < nobs) {
                            bits[j]->setBit(k, 1);
                            if (minval[j] > static_cast<double>(val[k]))
                                minval[j] = static_cast<double>(val[k]);
                            if (maxval[j] < static_cast<double>(val[k]))
                                maxval[j] = static_cast<double>(val[k]);
                        }
                    }
                }
                else {
                    for (uint32_t i = 0; i < nind; ++i) {
                        uint32_t k = iix[i];
                        if (k < nrows) {
                            uint32_t j = locate(val[k]);
                            if (j < nobs) {
                                bits[j]->setBit(k, 1);
                                if (minval[j] > static_cast<double>(val[k]))
                                    minval[j] = static_cast<double>(val[k]);
                                if (maxval[j] < static_cast<double>(val[k]))
                                    maxval[j] = static_cast<double>(val[k]);
                            }
                        }
                    }
                }
                ++iset;
                nind = iset.nIndices();
                if (*iix >= nrows) nind = 0;
            } // while (nind)

            // reset the nominal boundaries of the bins
            for (uint32_t i = 0; i < nobs-1; ++ i) {
                if (minval[i+1] < DBL_MAX && maxval[i] > -DBL_MAX)
                    bounds[i] = ibis::util::compactValue
                        (maxval[i], minval[i+1]);
            }
        }
        break;}
    case ibis::CATEGORY: // no need for a separate index
        col->logWarning("bin::binning", "no need for binning -- should "
                        "have a basic bitmap index already");
        clear();
        return;
    default:
        col->logWarning("bin::binning", "failed to create bins for "
                        "this type of column");
        return;
    }

    // make sure all bit vectors are the same size
    for (uint32_t i = 0; i < nobs; ++i) {
        bits[i]->adjustSize(0, nrows);
        //      bits[i]->compress();
    }

    // remove empty bins
    if (nobs > 0) {
        -- nobs;
        uint32_t k = 1;
        for (uint32_t i = 1; i < nobs; ++ i) {
            if (bits[i] != 0 && bits[i]->cnt() > 0) {
                if (i > k) {
                    // copy [i] to [k]
                    bounds[k] = bounds[i];
                    minval[k] = minval[i];
                    maxval[k] = maxval[i];
                    bits[k] = bits[i];
                }
                ++ k;
            }
            else { // delete the empty bitvector
                delete bits[i];
            }
        }
        if (nobs > k) {
            bounds[k] = bounds[nobs];
            minval[k] = minval[nobs];
            maxval[k] = maxval[nobs];
            bits[k] = bits[nobs];
            ++ k;
            bounds.resize(k);
            minval.resize(k);
            maxval.resize(k);
            bits.resize(k);
            nobs = k;
        }
        else {
            ++ nobs;
        }
    }

    // write info about the bins
    if (ibis::gVerbose > 2) {
        if (ibis::gVerbose > 4) {
            timer.stop();
            col->logMessage("bin::binning", "partitioned %lu values into "
                            "%lu bin(s) + 2 outside bins in %g "
                            "sec(elapsed)", static_cast<long unsigned>(nrows),
                            static_cast<long unsigned>(nobs-2),
                            timer.realTime());
        }
        else {
            col->logMessage("bin::binning", "partitioned %lu values into "
                            "%lu bin(s) + 2 outside bins",
                            static_cast<long unsigned>(nrows),
                            static_cast<long unsigned>(nobs-2));
        }
        if (ibis::gVerbose > 6) {
            ibis::util::logger lg;
            lg() << "\n[minval, maxval]\tbound\tcount\n";
            for (uint32_t i = 0; i < nobs; ++i)
                lg() << "[" << minval[i] << ", " << maxval[i] << "]\t"
                     << bounds[i] << "\t" << bits[i]->cnt() << "\n";
        }
    }
} // ibis::bin::binning

// binning with reordering
template <typename E>
void ibis::bin::binningT(const char* f) {
    if (col == 0) return;

    std::string evt="coumn[";
    evt += col->fullname();
    evt += "]::bin::binningT<";
    evt += typeid(E).name();
    evt += ">(";
    if (f != 0) {
        evt += f;
    }
    evt += ')';
    horometer timer;
    if (ibis::gVerbose > 4)
        timer.start();

    ibis::index::clear(); // delete the existing bitmaps
    // allocate space for min and max values and initialize them to extreme
    // values
    bits.resize(nobs);
    maxval.resize(nobs);
    minval.resize(nobs);
    for (uint32_t i = 0; i < nobs; ++i) {
        maxval[i] = -DBL_MAX;
        minval[i] = DBL_MAX;
        bits[i] = new ibis::bitvector;
    }

    std::string fnm; // name of the data file
    dataFileName(fnm, f);
    LOGGER(fnm.empty() && ibis::gVerbose > 2)
        << evt << " failed to determine the data file name from \""
        << (f ? f : "") << '"';

    ibis::bitvector mask;
    col->getNullMask(mask);
    if (col->partition() != 0)
        nrows = col->partition()->nRows();
    else
        nrows = mask.size();
    if (nrows == 0) return;

    array_t<E> val;
    std::vector<array_t<E>*> binned(nobs, 0); // binned version of values
    if (! fnm.empty())
        ibis::fileManager::instance().getFile(fnm.c_str(), val);
    else
        col->getValuesArray(&val);
    if (val.size() <= 0) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " failed to read " << fnm << " as "
            << typeid(E).name();
        throw ibis::bad_alloc("fail to read data file" IBIS_FILE_LINE);
    }
    else {
        nrows = val.size();
        if (nrows > mask.size())
            mask.adjustSize(nrows, nrows);
        ibis::bitvector::indexSet iset = mask.firstIndexSet();
        uint32_t nind = iset.nIndices();
        const ibis::bitvector::word_t *iix = iset.indices();
        while (nind) {
            if (iset.isRange()) { // a range
                uint32_t k = (iix[1] < nrows ? iix[1] : nrows);
                for (uint32_t i = *iix; i < k; ++i) {
                    uint32_t j = locate(val[i]);
                    if (j < nobs) {
                        bits[j]->setBit(i, 1);
                        if (minval[j] > val[i])
                            minval[j] = val[i];
                        if (maxval[j] < val[i])
                            maxval[j] = val[i];
                        if (binned[j] == 0)
                            binned[j] = new array_t<E>();
                        binned[j]->push_back(val[i]);
                    }
                }
            }
            else if (*iix+ibis::bitvector::bitsPerLiteral() < nrows) {
                // a list of indices
                for (uint32_t i = 0; i < nind; ++i) {
                    uint32_t k = iix[i];
                    uint32_t j = locate(val[k]);
                    if (j < nobs) {
                        bits[j]->setBit(k, 1);
                        if (minval[j] > val[k])
                            minval[j] = val[k];
                        if (maxval[j] < val[k])
                            maxval[j] = val[k];
                        if (binned[j] == 0)
                            binned[j] = new array_t<E>();
                        binned[j]->push_back(val[k]);
                    }
                }
            }
            else {
                for (uint32_t i = 0; i < nind; ++i) {
                    uint32_t k = iix[i];
                    if (k < nrows) {
                        uint32_t j = locate(val[k]);
                        if (j < nobs) {
                            bits[j]->setBit(k, 1);
                            if (minval[j] > val[k])
                                minval[j] = val[k];
                            if (maxval[j] < val[k])
                                maxval[j] = val[k];
                            if (binned[j] == 0)
                                binned[j] = new array_t<E>();
                            binned[j]->push_back(val[k]);
                        }
                    }
                }
            }
            ++ iset;
            nind = iset.nIndices();
            if (*iix >= nrows) nind = 0;
        } // while (nind)
    }

    // make sure all bit vectors are the same size
    for (uint32_t i = 0; i < nobs; ++i) {
        bits[i]->adjustSize(0, nrows);
    }

    // remove empty bins
    if (nobs > 0) {
        -- nobs;
        uint32_t k = 1;
        for (uint32_t i = 1; i < nobs; ++ i) {
            if (bits[i] != 0 && bits[i]->cnt() > 0) {
                if (i > k) {
                    // copy [i] to [k]
                    binned[k] = binned[i];
                    bounds[k] = bounds[i];
                    minval[k] = minval[i];
                    maxval[k] = maxval[i];
                    bits[k] = bits[i];
                }
                ++ k;
            }
            else { // delete the empty bitvector
                delete bits[i];
            }
        }
        if (nobs > k) {
            binned[k] = binned[nobs];
            bounds[k] = bounds[nobs];
            minval[k] = minval[nobs];
            maxval[k] = maxval[nobs];
            bits[k] = bits[nobs];
            ++ k;
            binned.resize(k);
            bounds.resize(k);
            minval.resize(k);
            maxval.resize(k);
            bits.resize(k);
            nobs = k;
        }
        else {
            ++ nobs;
        }
    }

    fnm += ".bin";
    int fdes = UnixOpen(fnm.c_str(), OPEN_WRITENEW, OPEN_FILEMODE);
    if (fdes >= 0) {
#if defined(_WIN32) && defined(_MSC_VER)
        (void)_setmode(fdes, _O_BINARY);
#endif
        long ierr = UnixWrite(fdes, &nobs, sizeof(nobs));
        if (ierr < (long)sizeof(nobs)) { // write operation failed
            (void) UnixClose(fdes);
            remove(fnm.c_str());
            return;
        }
        const uint32_t elem = sizeof(E);
        array_t<int32_t> pos(nobs+1);
        pos[0] = sizeof(nobs) + (nobs+1) * sizeof(int32_t);
        ierr = UnixSeek(fdes, pos[0], SEEK_SET);
        if (ierr != pos[0]) { // write operation failed
            (void) UnixClose(fdes);
            remove(fnm.c_str());
            return;
        }
        for (uint32_t i = 0; i < nobs; ++ i) {
            if (maxval[i] > minval[i])
                ierr = ibis::util::write(fdes, binned[i]->begin(),
                                         elem * binned[i]->size());
            delete binned[i];
            pos[i+1] = UnixSeek(fdes, 0, SEEK_CUR);
        }
        ierr = UnixSeek(fdes, sizeof(uint32_t), SEEK_SET);
        ierr = ibis::util::write(fdes, pos.begin(), sizeof(int32_t)*(nobs+1));
        ierr = UnixSeek(fdes, pos.back(), SEEK_SET);
        UnixClose(fdes);
        LOGGER(ibis::gVerbose > 3)
            << evt << " wrote bin-ordered values to " << fnm;
    }
    else {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- " << evt << " failed to write bin-ordered values to "
            << fnm;
        for (uint32_t i = 0; i < nobs; ++ i) {
            if (binned[i] != 0)
                delete binned[i];
        }
    }

    // write info about the bins
    if (ibis::gVerbose > 2) {
        ibis::util::logger lg;
        if (ibis::gVerbose > 4) {
            timer.stop();
            lg() << evt << " partitioned " << nrows << " values into " << nobs-2
                 << " bin(s) + 2 outside bins in " << timer.realTime()
                 << " sec(elapsed)";
        }
        else {
            lg() << evt << "partitioned " << nrows << " values into "
                 << nobs-2 << " bin(s) + 2 outside bins";
        }
        if (ibis::gVerbose > 6) {
            lg() << "[minval, maxval]\tbound\tcount\n";
            for (uint32_t i = 0; i < nobs; ++i)
                lg() << "[" << minval[i] << ", " << maxval[i] << "]\t"
                     << bounds[i] << "\t" << bits[i]->cnt() << "\n";
        }
    }
} // ibis::bin::binningT

/// Write bin-ordered values.
long ibis::bin::binOrder(const char* basename) const {
    long ierr = 0;
    switch (col->type()) { // binning with reordering
    case ibis::DOUBLE:
        ierr = binOrderT<double>(basename);
        break;
    case ibis::FLOAT:
        ierr = binOrderT<float>(basename);
        break;
    case ibis::ULONG:
        ierr = binOrderT<uint64_t>(basename);
        break;
    case ibis::LONG:
        ierr = binOrderT<int64_t>(basename);
        break;
    case ibis::UINT:
        ierr = binOrderT<uint32_t>(basename);
        break;
    case ibis::INT:
        ierr = binOrderT<int32_t>(basename);
        break;
    case ibis::USHORT:
        ierr = binOrderT<uint32_t>(basename);
        break;
    case ibis::SHORT:
        ierr = binOrderT<int16_t>(basename);
        break;
    case ibis::UBYTE:
        ierr = binOrderT<unsigned char>(basename);
        break;
    case ibis::BYTE:
        ierr = binOrderT<signed char>(basename);
        break;
    default:
        ibis::util::logMessage("Warning",
                               "failed to reorder column %s type %d",
                               col->name(), (int)(col->type()));
        ierr = -3;
    }
    return ierr;
} // ibis::bin::binOrder

/// Write bin-ordered values.
template <typename E>
long ibis::bin::binOrderT(const char* basename) const {
    long ierr = 0;
    if (nobs == 0)
        return ierr;

    std::string fnm;
    dataFileName(fnm, basename);
    array_t<E> basevals;
    ierr = ibis::fileManager::instance().getFile(fnm.c_str(), basevals);
    if (ierr != 0) {
        ierr = -1;
        return ierr;
    }

    fnm += ".bin";
    int fdes = UnixOpen(fnm.c_str(), OPEN_WRITENEW, OPEN_FILEMODE);
    if (fdes < 0) {
        LOGGER(ibis::gVerbose > -1)
            << "bin::binOrder is failed to open file \"" << fnm
            << "\" for writing";
        ierr = -2;
        return ierr;
    }
    std::ostringstream mesg;
    mesg << "column[" << (col ? col->fullname() : "?.?") << "]::bin::binOrder<"
         << typeid(E).name() << ">(" << fnm << ")";
    ibis::util::timer timer(mesg.str().c_str(), 3);

#if defined(_WIN32) && defined(_MSC_VER)
    (void)_setmode(fdes, _O_BINARY);
#endif
    ierr = UnixWrite(fdes, &nobs, sizeof(nobs));
    const uint32_t elem = sizeof(E);
    array_t<int32_t> pos(nobs+1);
    array_t<E> binned;
    binned.reserve(nrows / nobs);
    pos[0] = sizeof(nobs) + (nobs+1) * sizeof(int32_t);

    ierr = UnixSeek(fdes, pos[0], SEEK_SET);
    if (ierr != pos[0]) { // write operation failed
        (void) UnixClose(fdes);
        remove(fnm.c_str());
        return -3;
    }
    for (uint32_t i = 0; i < nobs; ++ i) {
        if (maxval[i] > minval[i] && bits[i] != 0) {
            binned.clear();
            for (ibis::bitvector::indexSet is = bits[i]->firstIndexSet();
                 is.nIndices() > 0; ++ is) {
                const ibis::bitvector::word_t *ind = is.indices();
                if (is.isRange()) {
                    for (unsigned j = *ind; j < ind[1]; ++ j)
                        binned.push_back(basevals[j]);
                }
                else {
                    for (unsigned j = 0; j < is.nIndices(); ++ j)
                        binned.push_back(basevals[ind[j]]);
                }
            }
            ierr = ibis::util::write(fdes, binned.begin(), elem*binned.size());
        }
        pos[i+1] = UnixSeek(fdes, 0, SEEK_CUR);
    }
    ierr = UnixSeek(fdes, sizeof(uint32_t), SEEK_SET);
    ierr = ibis::util::write(fdes, pos.begin(), sizeof(int32_t)*(nobs+1));
    ierr = UnixSeek(fdes, pos.back(), SEEK_SET);
    ierr = UnixClose(fdes);
    return ierr;
} // ibis::bin::binOrderT

// caller has to make sure jbin < nobs and bits[jbin] != 0.
template <typename E>
long ibis::bin::checkBin0(const ibis::qRange& cmp, uint32_t jbin,
                          ibis::bitvector& res) const {
    res.clear();
    long ierr = 0;
    std::string fnm;
    dataFileName(fnm);
    fnm += ".bin";
    if (ibis::util::getFileSize(fnm.c_str()) <=
        (off_t)(sizeof(int32_t)*(nobs+1))) {
        // bin file too small
        ierr = -1;
        return ierr;
    }

    int32_t pos[2];
    int fdes = UnixOpen(fnm.c_str(), OPEN_READONLY);
    if (fdes < 0) { // failed to open file
        ierr = -2;
        return ierr;
    }
#if defined(_WIN32) && defined(_MSC_VER)
    (void)_setmode(fdes, _O_BINARY);
#endif
    pos[0] = sizeof(uint32_t)+jbin*sizeof(int32_t);
    ierr = UnixSeek(fdes, pos[0], SEEK_SET);
    if (ierr != pos[0]) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- bin::checkBin0 failed to seek to " << pos[0]
            << " in " << fnm;
        UnixClose(fdes);
        return -3;
    }
    ierr = UnixRead(fdes, pos, 2*sizeof(int32_t));
    if (ierr < static_cast<long>(2*sizeof(int32_t)) || pos[1] <= pos[0]) {
        if (ibis::gVerbose > 0) {
            ibis::util::logger lg;
            lg() << "Warning -- bin::checkBin0 ";
            if (ierr < static_cast<long>(2*sizeof(int32_t)))
                lg() << "failed to read the starting position for bin "
                     << jbin << " from " << fnm;
            else if (pos[1] < pos[0])
                lg() << "encountered bad starting positions (" << pos[0] << ", "
                     << pos[1] << ") for bin " << jbin;
        }
        ierr = UnixClose(fdes);
        return ierr;
    }

    array_t<E> vals(fdes, pos[0], pos[1]); // read in the values
    UnixClose(fdes);
    if (vals.size() != bits[jbin]->cnt()) { // bad bin file?
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- bin::checkBin0 expected " << bits[jbin]->cnt()
            << " values, but got " << vals.size();
        ierr = -3;
        return ierr;
    }

    uint32_t ivals = 0;
    ibis::bitvector::indexSet is = bits[jbin]->firstIndexSet();
    while (is.nIndices() > 0) {
        const ibis::bitvector::word_t *iix = is.indices();
        if (is.isRange()) {
            for (uint32_t j = *iix; j < iix[1]; ++ j, ++ ivals)
                if (cmp.inRange(static_cast<double>(vals[ivals])))
                    res.setBit(j, 1);
        }
        else {
            for (uint32_t j = 0; j < is.nIndices(); ++ j, ++ ivals)
                if (cmp.inRange(static_cast<double>(vals[ivals])))
                    res.setBit(iix[j], 1);
        }
        ++ is;
    }
    res.adjustSize(0, nrows);
    ierr = res.cnt();
    return ierr;
} // ibis::bin::checkBin0

// For the encoding that does not store the entries in bin @c jbin as @c
// bits[jbin].
template <typename E>
long ibis::bin::checkBin1(const ibis::qRange& cmp, uint32_t jbin,
                          const ibis::bitvector& mask,
                          ibis::bitvector& res) const {
    res.clear();
    long ierr = 0;
    std::string fnm;
    dataFileName(fnm);
    fnm += ".bin";
    if (ibis::util::getFileSize(fnm.c_str()) <=
        (off_t)(sizeof(int32_t)*(nobs+1))) {
        // bin file too small
        ierr = -1;
        return ierr;
    }

    int32_t pos[2];
    int fdes = UnixOpen(fnm.c_str(), OPEN_READONLY);
    if (fdes < 0) { // failed to open file
        ierr = -2;
        return ierr;
    }
#if defined(_WIN32) && defined(_MSC_VER)
    (void)_setmode(fdes, _O_BINARY);
#endif
    pos[0] = sizeof(uint32_t)+jbin*sizeof(int32_t);
    ierr = UnixSeek(fdes, pos[0], SEEK_SET);
    if (ierr != pos[0]) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- bin::checkBin1 failed to seek to " << pos[0]
            << " in " << fnm;
        UnixClose(fdes);
        return -3;
    }
    ierr = UnixRead(fdes, pos, 2*sizeof(int32_t));
    if (ierr < static_cast<long>(2*sizeof(int32_t)) || pos[1] <= pos[0]) {
        if (ibis::gVerbose > 0) {
            ibis::util::logger lg;
            lg() << "Warning -- bin::checkBin1 ";
            if (ierr < static_cast<long>(2*sizeof(int32_t)))
                lg() << "failed to read the starting position for bin " << jbin
                     << " from " << fnm;
            else if (pos[1] < pos[0])
                lg() << "encountered bad starting position (" << pos[0] << ", "
                     << pos[1] << ") for bin " << jbin;
        }
        ierr = UnixClose(fdes);
        return ierr;
    }

    array_t<E> vals(fdes, pos[0], pos[1]); // read in the values
    UnixClose(fdes);
    if (vals.size() != mask.cnt()) { // bad bin file?
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- bin::checkBin1 expected " << mask.cnt()
            << " values, but got " << vals.size();
        ierr = -3;
        return ierr;
    }

    uint32_t ivals = 0;
    ibis::bitvector::indexSet is = mask.firstIndexSet();
    while (is.nIndices() > 0) {
        const ibis::bitvector::word_t *iix = is.indices();
        if (is.isRange()) {
            for (uint32_t j = *iix; j < iix[1]; ++ j, ++ ivals)
                if (cmp.inRange(static_cast<double>(vals[ivals])))
                    res.setBit(j, 1);
        }
        else {
            for (uint32_t j = 0; j < is.nIndices(); ++ j, ++ ivals)
                if (cmp.inRange(static_cast<double>(vals[ivals])))
                    res.setBit(iix[j], 1);
        }
        ++ is;
    }
    res.adjustSize(0, nrows);
    ierr = res.cnt();
    return ierr;
} // ibis::bin::checkBin1

long ibis::bin::checkBin(const ibis::qRange& cmp, uint32_t jbin,
                         ibis::bitvector& res) const {
    res.clear();
    if (col == 0) return -1;
    long ierr = 0;
    if (jbin > nobs) { // out of range, no hits
        return ierr;
    }
    if (bits[jbin] == 0)
        activate(jbin);
    if (bits[jbin] == 0) { // empty bin
        return ierr;
    }
    if (bits[jbin]->cnt() == 0) { // empty bin
        return ierr;
    }

    ibis::horometer timer;
    if (ibis::gVerbose > 4)
        timer.start();

    switch (col->type()) {
    case ibis::DOUBLE:
        ierr = checkBin0<double>(cmp, jbin, res);
        break;
    case ibis::FLOAT:
        ierr = checkBin0<float>(cmp, jbin, res);
        break;
    case ibis::ULONG:
        ierr = checkBin0<uint64_t>(cmp, jbin, res);
        break;
    case ibis::LONG:
        ierr = checkBin0<int64_t>(cmp, jbin, res);
        break;
    case ibis::UINT:
        ierr = checkBin0<uint32_t>(cmp, jbin, res);
        break;
    case ibis::INT:
        ierr = checkBin0<int32_t>(cmp, jbin, res);
        break;
    case ibis::USHORT:
        ierr = checkBin0<uint16_t>(cmp, jbin, res);
        break;
    case ibis::SHORT:
        ierr = checkBin0<int16_t>(cmp, jbin, res);
        break;
    case ibis::UBYTE:
        ierr = checkBin0<unsigned char>(cmp, jbin, res);
        break;
    case ibis::BYTE:
        ierr = checkBin0<signed char>(cmp, jbin, res);
        break;
    default:
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- failed to bin column " << col->name()
            << " (type " << (int)(col->type()) << ", "
            << ibis::TYPESTRING[(int)(col->type())] << ')';
        ierr = -4;
    }
    if (ibis::gVerbose > 4) {
        timer.stop();
        if (ierr < 0) 
            col->logWarning("bin::checkBin", "checking bin # %lu [%lu] took "
                            "%g sec(CPU), %g sec(elapsed).  Returning "
                            "error code %ld",
                            static_cast<long unsigned>(jbin),
                            static_cast<long unsigned>(bits[jbin]->cnt()),
                            timer.CPUTime(), timer.realTime(), ierr);
        else
            col->logMessage("bin::checkBin", "checking bin # %lu [%lu] took "
                            "%g sec(CPU), %g sec(elapsed).  Returning %ld",
                            static_cast<long unsigned>(jbin),
                            static_cast<long unsigned>(bits[jbin]->cnt()),
                            timer.CPUTime(), timer.realTime(), ierr);
    }
    else if (ierr < 0) {
        col->logWarning("bin::checkBin", "checking bin # %lu [%lu] took "
                        "%g sec(CPU), %g sec(elapsed).  Returning "
                        "error code %ld",
                        static_cast<long unsigned>(jbin),
                        static_cast<long unsigned>(bits[jbin]->cnt()),
                        timer.CPUTime(), timer.realTime(), ierr);
    }
    return ierr;
} // ibis::bin::checkBin

long ibis::bin::checkBin(const ibis::qRange& cmp, uint32_t jbin,
                         const ibis::bitvector& mask,
                         ibis::bitvector& res) const {
    res.clear();
    if (col == 0) return -1;
    long ierr = 0;
    if (jbin > nobs) { // out of range, no hits
        return ierr;
    }
    if (mask.size() != nrows || mask.cnt() == 0) {
        return ierr;
    }

    ibis::horometer timer;
    if (ibis::gVerbose > 4)
        timer.start();

    switch (col->type()) {
    case ibis::DOUBLE:
        ierr = checkBin1<double>(cmp, jbin, mask, res);
        break;
    case ibis::FLOAT:
        ierr = checkBin1<float>(cmp, jbin, mask, res);
        break;
    case ibis::ULONG:
        ierr = checkBin1<uint64_t>(cmp, jbin, mask, res);
        break;
    case ibis::LONG:
        ierr = checkBin1<int64_t>(cmp, jbin, mask, res);
        break;
    case ibis::UINT:
        ierr = checkBin1<uint32_t>(cmp, jbin, mask, res);
        break;
    case ibis::INT:
        ierr = checkBin1<int32_t>(cmp, jbin, mask, res);
        break;
    case ibis::USHORT:
        ierr = checkBin1<uint16_t>(cmp, jbin, mask, res);
        break;
    case ibis::SHORT:
        ierr = checkBin1<int16_t>(cmp, jbin, mask, res);
        break;
    case ibis::UBYTE:
        ierr = checkBin1<unsigned char>(cmp, jbin, mask, res);
        break;
    case ibis::BYTE:
        ierr = checkBin1<signed char>(cmp, jbin, mask, res);
        break;
    default:
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- failed to bin column " << col->name()
            << " (type " << (int)(col->type()) << ", "
            << ibis::TYPESTRING[(int)(col->type())] << ')';
        ierr = -4;
    }
    if (ibis::gVerbose > 4) {
        timer.stop();
        if (ierr < 0)
            col->logWarning("bin::checkBin", "checking bin # %lu (%lu) "
                            "took %g sec(CPU), %g sec(elapsed).  Returning "
                            "error code %ld",
                            static_cast<long unsigned>(jbin),
                            static_cast<long unsigned>(mask.cnt()),
                            timer.CPUTime(), timer.realTime(), ierr);
        else
            col->logMessage("bin::checkBin", "checking bin # %lu (%lu) "
                            "took %g sec(CPU), %g sec(elapsed). Returning "
                            "%ld",
                            static_cast<long unsigned>(jbin),
                            static_cast<long unsigned>(mask.cnt()),
                            timer.CPUTime(), timer.realTime(), ierr);
    }
    else {
        col->logWarning("bin::checkBin", "checking bin # %lu (%lu) "
                        "took %g sec(CPU), %g sec(elapsed).  Returning "
                        "error code %ld",
                        static_cast<long unsigned>(jbin),
                        static_cast<long unsigned>(mask.cnt()),
                        timer.CPUTime(), timer.realTime(), ierr);
    }
    return ierr;
} // ibis::bin::checkBin

template <typename E>
void ibis::bin::scanAndPartition(const array_t<E> &varr, unsigned eqw) {
    if (varr.empty() || col == 0) return;

    nrows = varr.size();
    uint32_t nbins = parseNbins(*col);
    if (eqw <= 1) { // simple binning
        E amin = varr[0];
        E amax = varr[0];
        for (size_t i = 1; i < varr.size(); ++ i) {
            if (amin > varr[i]) amin = varr[i];
            if (amax < varr[i]) amax = varr[i];
        }
        if (amin >= amax) { // a single value
            bits.resize(1);
            bits[0] = new ibis::bitvector;
            bits[0]->set(1, nrows);
            minval.resize(1);
            maxval.resize(1);
            minval[0] = amin;
            maxval[0] = amax;
            bounds.resize(1);
            bounds[1] = DBL_MAX;
            return;
        }

        if (sizeof(amin) >= 4) {
            amin = (E) ibis::util::compactValue
                (amin-0.5*(amax-amin)/nbins, amin);
            amax = (E) ibis::util::compactValue
                (amax, amax+0.5*(amax-amin)/nbins);
        }
        else {
            ++ amax;
        }
        double delta = (amax - amin) / nbins;
        bits.resize(nbins);
        minval.resize(nbins);
        maxval.resize(nbins);
        for (size_t i = 0; i < nbins; ++ i) {
            bits[i] = new ibis::bitvector;
            minval[i] = DBL_MAX;
            maxval[i] = -DBL_MAX;
        }
        for (size_t j = 0; j < varr.size(); ++ j) {
            uint32_t k = (uint32_t) ((varr[j] - amin) / delta);
            if (k < nbins) {
                bits[k]->setBit(j, 1);
                if (minval[k] > varr[j]) minval[k] = varr[j];
                if (maxval[k] < varr[j]) maxval[k] = varr[j];
            }
        }

        nbins = 0;
        for (size_t j = 0; j < nbins; ++ j) {
            if (bits[j]->cnt() > 0) {
                if (nbins < j) {
                    bits[nbins] = bits[j];
                    minval[nbins] = minval[j];
                    maxval[nbins] = maxval[j];
                }
                ++ nbins;
            }
            else {
                delete bits[j];
            }
        }
        bits.resize(nbins);
        minval.resize(nbins);
        maxval.resize(nbins);
        bounds.reserve(nbins);
        for (size_t i = 1; i < nbins; ++ i)
            bounds.push_back(ibis::util::compactValue
                             (maxval[i-1], minval[i]));
        bounds.push_back(DBL_MAX);
        return;
    }

    histogram hist; // a histogram
    mapValues(varr, hist);
    const uint32_t ncnt = hist.size();
    histogram::const_iterator it;
    if (ncnt > nbins * 3 / 2) { // more distinct values than the number of bins
        array_t<double> val(ncnt);
        array_t<uint32_t> cnt(ncnt);
        array_t<uint32_t> bnds(nbins);
        {
            uint32_t i = 0;
            for (it = hist.begin(); it != hist.end(); ++it, ++i) {
                cnt[i] = (*it).second;
                val[i] = (*it).first;
            }
        }
        hist.clear();
        divideCounts(bnds, cnt);

#if defined(DBUG) && DEBUG+0 > 2
        {
            ibis::util::logger lg(4);
            lg() << "expected bounds  size(" << bounds.size() << ")\n";
            for (array_t<uint32_t>::const_iterator itt = bnds.begin();
                 itt != bnds.end(); ++ itt)
                lg() << val[*itt] << " (" << *itt << ") ";
        }
#endif
        if (col->type() == ibis::FLOAT ||
            col->type() == ibis::DOUBLE) {
            // for floating-point values, try to round the boundaries
            if (bounds.empty()) {
                if (val[0] >= 0)
                    bounds.push_back(0.0);
                else
                    bounds.push_back(ibis::util::compactValue
                                     (val[0], -DBL_MAX));
            }
            else if (bounds.back() < val[0]) {
                bounds.push_back(ibis::util::compactValue
                                 (bounds.back(), val[0]));
            }
            for (array_t<uint32_t>::const_iterator ii = bnds.begin();
                 ii != bnds.end(); ++ii) {
                double tmp;
                if (*ii == 1) {
                    tmp = ibis::util::compactValue
                        (0.5*(val[0]+val[1]), val[1]);
                }
                else if (*ii < ncnt) {
                    tmp = ibis::util::compactValue(val[*ii-1], val[*ii]);
                }
                else {
                    tmp = col->upperBound();
                    if (val.back() <= tmp) {
                        tmp = ibis::util::compactValue(val.back(), tmp);
                    }
                    else {
                        tmp = ibis::util::compactValue(val.back(), DBL_MAX);
                    }
                }
                bounds.push_back(tmp);
            }
        }
        else { // for integer values, may need to narrow the heavy bins
            uint32_t avg = 0; // the average
            for (uint32_t i = 0; i < ncnt; ++ i)
                avg += cnt[i];
            avg /= nbins;
            bool skip = false;
            for (array_t<uint32_t>::const_iterator ii = bnds.begin();
                 ii != bnds.end() && *ii < ncnt; ++ii) {
                if (skip) {
                    skip = false;
                }
                else {
                    bounds.push_back(val[*ii]);
                    if (cnt[*ii] > avg) {
                        const uint32_t next = 1 + *ii;
                        if (next < cnt.size() && val[*ii]+1 < val[next]) {
                            // ensure the bin contain only a single value
                            bounds.push_back(val[*ii]+1);
                            skip = true;
                        }
                    }
                }
            }
        }
    }
    else if (ncnt > 1) { // enough bins for every value
        for (it = hist.begin(); it != hist.end(); ++ it)
            bounds.push_back((*it).first);
    }
    else if (ncnt > 0) { // one value only
        it = hist.begin();
        if (fabs((*it).first - 1) < 0.5) {
            bounds.push_back(0.0);
            bounds.push_back(2.0);
        }
        else {
            bounds.push_back(ibis::util::compactValue
                             ((*it).first, -DBL_MAX));
            bounds.push_back(ibis::util::compactValue
                             ((*it).first, DBL_MAX));
        }
    }
#if DEBUG+0 > 2 || _DEBUG+0 > 2
    {
        ibis::util::logger lg(4);
        lg() << "DEBUG - content of bounds in scanAndPartition: size("
             << bounds.size() << ")\n";
        for (array_t<double>::const_iterator it = bounds.begin();
             it != bounds.end(); ++ it)
            lg() << *it << " ";
    }
#endif
} // ibis::bin::scanAndPartition

/// Construct a binned bitmap index.  It reads data from disk.  The
/// arguement df can be the name of the directory containing the data or
/// the data file name.  The actual file name is determined by the function
/// ibis::column::dataFilename.
///
/// This construction function is designed to handle the full spectrum of
/// binning specifications.
void ibis::bin::construct(const char* df) {
    if (col == 0) return;

    const char* spec = col->indexSpec();
    if (spec == 0 || *spec == 0) {
        std::string idxnm(col->fullname());
        idxnm += ".index";
        spec = ibis::gParameters()[idxnm.c_str()];
    }
    const bool reorder = (spec != 0 ? strstr(spec, "reorder") != 0 :
                          false);
    std::string fname;
    if (0 == col->dataFileName(fname, df)) {
        LOGGER(ibis::gVerbose > 1)
            << "bin::construct can not determine the data file name "
            "for column " << col->name()
            << ", assume the data is already in memory";
    }

    bool grn = (spec == 0 || *spec == 0 || strstr(spec, "precision=") != 0 ||
                strstr(spec, "prec=") != 0 || strstr(spec, "automatic") != 0 ||
                strstr(spec, "default") != 0);
    if (grn == false && spec != 0 && *spec != 0) {
        const char *str = strstr(spec, "<binning ");
        if (str != 0) {    
            str += 9;
            while (str != 0 && isspace(*str) != 0) ++ str;
            grn = (*str == 0 || *str == '>' || (*str == '/' && str[1] == '>'));
        }
    }
    if (grn) {
        // <binning precision=d /> or <binning prec=d />
#if 0
        ibis::bak2 tmp(col, df);
        swap(tmp);

        // append DBL_MAX as the bin boundary at the end
        bounds.push_back(DBL_MAX);
        // insert an empty bit vector at the beginning of the list
        ibis::bitvector *tb = new ibis::bitvector;
        tb->set(0, bits[0]->size());
        bits.insert(bits.begin(), tb);
        minval.insert(minval.begin(), DBL_MAX);
        maxval.insert(maxval.begin(), -DBL_MAX);
        // there is now one more bins than before
        ++ nobs;
#else
        switch (col->type()) {
        case ibis::DOUBLE: {
            array_t<double> vals;
            int ierr;
            if (! fname.empty())
                ierr = ibis::fileManager::instance().getFile
                    (fname.c_str(), vals);
            else
                ierr = col->getValuesArray(&vals);
            if (ierr < 0)
                throw "bin::construct failed to read raw data" IBIS_FILE_LINE;
            construct(vals);
            break;}
        case ibis::FLOAT: {
            array_t<float> vals;
            int ierr;
            if (! fname.empty())
                ierr = ibis::fileManager::instance().getFile
                    (fname.c_str(), vals);
            else
                ierr = col->getValuesArray(&vals);
            if (ierr < 0)
                throw "bin::construct failed to read raw data" IBIS_FILE_LINE;
            construct(vals);
            break;}
        case ibis::ULONG: {
            array_t<uint64_t> vals;
            int ierr;
            if (! fname.empty())
                ierr = ibis::fileManager::instance().getFile
                    (fname.c_str(), vals);
            else
                ierr = col->getValuesArray(&vals);
            if (ierr < 0)
                throw "bin::construct failed to read raw data" IBIS_FILE_LINE;
            construct(vals);
            break;}
        case ibis::LONG: {
            array_t<int64_t> vals;
            int ierr;
            if (! fname.empty())
                ierr = ibis::fileManager::instance().getFile
                    (fname.c_str(), vals);
            else
                ierr = col->getValuesArray(&vals);
            if (ierr < 0)
                throw "bin::construct failed to read raw data" IBIS_FILE_LINE;
            construct(vals);
            break;}
        case ibis::UINT: {
            array_t<uint32_t> vals;
            int ierr;
            if (! fname.empty())
                ierr = ibis::fileManager::instance().getFile
                    (fname.c_str(), vals);
            else
                ierr = col->getValuesArray(&vals);
            if (ierr < 0)
                throw "bin::construct failed to read raw data" IBIS_FILE_LINE;
            construct(vals);
            break;}
        case ibis::INT: {
            array_t<int32_t> vals;
            int ierr;
            if (! fname.c_str())
                ierr = ibis::fileManager::instance().getFile
                    (fname.c_str(), vals);
            else
                ierr = col->getValuesArray(&vals);
            if (ierr < 0)
                throw "bin::construct failed to read raw data" IBIS_FILE_LINE;
            construct(vals);
            break;}
        case ibis::USHORT: {
            array_t<uint16_t> vals;
            int ierr;
            if (! fname.c_str())
                ierr = ibis::fileManager::instance().getFile
                    (fname.c_str(), vals);
            else
                ierr = col->getValuesArray(&vals);
            if (ierr < 0)
                throw "bin::construct failed to read raw data" IBIS_FILE_LINE;
            construct(vals);
            break;}
        case ibis::SHORT: {
            array_t<int16_t> vals;
            int ierr;
            if (! fname.empty())
                ierr = ibis::fileManager::instance().getFile
                    (fname.c_str(), vals);
            else
                ierr = col->getValuesArray(&vals);
            if (ierr < 0)
                throw "bin::construct failed to read raw data" IBIS_FILE_LINE;
            construct(vals);
            break;}
        case ibis::UBYTE: {
            array_t<unsigned char> vals;
            int ierr;
            if (! fname.c_str())
                ierr = ibis::fileManager::instance().getFile
                    (fname.c_str(), vals);
            else
                ierr = col->getValuesArray(&vals);
            if (ierr < 0)
                throw "bin::construct failed to read raw data" IBIS_FILE_LINE;
            construct(vals);
            break;}
        case ibis::BYTE: {
            array_t<signed char> vals;
            int ierr;
            if (! fname.empty())
                ierr = ibis::fileManager::instance().getFile
                    (fname.c_str(), vals);
            else
                ierr = col->getValuesArray(&vals);
            if (ierr < 0)
                throw "bin::construct failed to read raw data" IBIS_FILE_LINE;
            construct(vals);
            break;}
        default:
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- failed to bin column " << col->name()
                << " (type " << (int)(col->type()) << ", "
                << ibis::TYPESTRING[(int)(col->type())] << ')';
            throw "Unexpected data type for bin" IBIS_FILE_LINE;
        }
#endif
        if (reorder && ! fname.empty())
            binOrder(df);
    }
    else if (! fname.empty()) {
        // read the data file to determine the bin boundaries
        setBoundaries(df);
        if (reorder) {
            switch (col->type()) { // binning with reordering
            case ibis::DOUBLE:
                binningT<double>(df);
                break;
            case ibis::FLOAT:
                binningT<float>(df);
                break;
            case ibis::ULONG:
                binningT<uint64_t>(df);
                break;
            case ibis::LONG:
                binningT<int64_t>(df);
                break;
            case ibis::UINT:
                binningT<uint32_t>(df);
                break;
            case ibis::INT:
                binningT<int32_t>(df);
                break;
            case ibis::USHORT:
                binningT<uint16_t>(df);
                break;
            case ibis::SHORT:
                binningT<int16_t>(df);
                break;
            case ibis::UBYTE:
                binningT<unsigned char>(df);
                break;
            case ibis::BYTE:
                binningT<signed char>(df);
                break;
            default:
                LOGGER(ibis::gVerbose > 0)
                    << "Warning -- failed to bin column " << col->name()
                    << " (type " << (int)(col->type()) << ", "
                    << ibis::TYPESTRING[(int)(col->type())] << ')';
                throw "Unexpected data type for bin" IBIS_FILE_LINE;
            }
        }
        else {
            binning(df);
        }
    }
    else {
        switch (col->type()) {
        case ibis::DOUBLE: {
            array_t<double> vals;
            int ierr = col->getValuesArray(&vals);
            if (ierr < 0)
                throw "bin::construct failed to read raw data" IBIS_FILE_LINE;
            construct(vals);
            break;}
        case ibis::FLOAT: {
            array_t<float> vals;
            int ierr = col->getValuesArray(&vals);
            if (ierr < 0)
                throw "bin::construct failed to read raw data" IBIS_FILE_LINE;
            construct(vals);
            break;}
        case ibis::ULONG: {
            array_t<uint64_t> vals;
            int ierr = col->getValuesArray(&vals);
            if (ierr < 0)
                throw "bin::construct failed to read raw data" IBIS_FILE_LINE;
            construct(vals);
            break;}
        case ibis::LONG: {
            array_t<int64_t> vals;
            int ierr = col->getValuesArray(&vals);
            if (ierr < 0)
                throw "bin::construct failed to read raw data" IBIS_FILE_LINE;
            construct(vals);
            break;}
        case ibis::UINT: {
            array_t<uint32_t> vals;
            int ierr = col->getValuesArray(&vals);
            if (ierr < 0)
                throw "bin::construct failed to read raw data" IBIS_FILE_LINE;
            construct(vals);
            break;}
        case ibis::INT: {
            array_t<int32_t> vals;
            int ierr = col->getValuesArray(&vals);
            if (ierr < 0)
                throw "bin::construct failed to read raw data" IBIS_FILE_LINE;
            construct(vals);
            break;}
        case ibis::USHORT: {
            array_t<uint16_t> vals;
            int ierr = col->getValuesArray(&vals);
            if (ierr < 0)
                throw "bin::construct failed to read raw data" IBIS_FILE_LINE;
            construct(vals);
            break;}
        case ibis::SHORT: {
            array_t<int16_t> vals;
            int ierr = col->getValuesArray(&vals);
            if (ierr < 0)
                throw "bin::construct failed to read raw data" IBIS_FILE_LINE;
            construct(vals);
            break;}
        case ibis::UBYTE: {
            array_t<unsigned char> vals;
            int ierr = col->getValuesArray(&vals);
            if (ierr < 0)
                throw "bin::construct failed to read raw data" IBIS_FILE_LINE;
            construct(vals);
            break;}
        case ibis::BYTE: {
            array_t<signed char> vals;
            int ierr = col->getValuesArray(&vals);
            if (ierr < 0)
                throw "bin::construct failed to read raw data" IBIS_FILE_LINE;
            construct(vals);
            break;}
        default:
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- failed to bin column " << col->name()
                << " (type " << (int)(col->type()) << ", "
                << ibis::TYPESTRING[(int)(col->type())] << ')';
            throw "Unexpected data type for bin" IBIS_FILE_LINE;
        }
    }

    optionalUnpack(bits, col->indexSpec());
    nobs = bits.size();
    if (nobs > 0) {
        offset64.resize(nobs+1);
        offset64[0] = 0;
        for (unsigned j = 0; j < nobs; ++ j)
            offset64[j+1] = offset64[j] +
                (bits[j] != 0 ? bits[j]->getSerialSize() : 0);
    }
    if (ibis::gVerbose > 4) {
        ibis::util::logger lg;
        lg() << "bin[" << col->fullname() << "]::construct(" << (df ? df : "")
             << ") -- finished constructing a binned equality index with "
             << nobs << " bin" << (nobs>1?"s":"");
        if (ibis::gVerbose > 8) {
            lg() << "\n";
            print(lg());
        }
    }
} // ibis::bin::construct

// explicit instantiations of templated functions
template void ibis::bin::construct(const array_t<signed char>&);
template void ibis::bin::construct(const array_t<unsigned char>&);
template void ibis::bin::construct(const array_t<int16_t>&);
template void ibis::bin::construct(const array_t<uint16_t>&);
template void ibis::bin::construct(const array_t<int32_t>&);
template void ibis::bin::construct(const array_t<uint32_t>&);
template void ibis::bin::construct(const array_t<int64_t>&);
template void ibis::bin::construct(const array_t<uint64_t>&);
template void ibis::bin::construct(const array_t<float>&);
template void ibis::bin::construct(const array_t<double>&);

/// Construction function for in-memory data.  It reads the indexing option
/// from using the function ibis::column::indexSpec.
template <typename E>
void ibis::bin::construct(const array_t<E>& varr) {
    if (varr.empty()) return; // can not do anything with an empty array

    const char* spec = (col ? col->indexSpec() : static_cast<const char*>(0));
    bool grn = (spec == 0 || *spec == 0 || strstr(spec, "precision=") != 0 ||
                strstr(spec, "prec=") != 0 || strstr(spec, "automatic") != 0 ||
                strstr(spec, "default") != 0);
    if (grn == false && spec != 0 && *spec != 0) {
        const char *str = strstr(spec, "<binning ");
        if (str != 0) {
            str += 9;
            while (str != 0 && isspace(*str) != 0) ++ str;
            grn = (*str == 0 || *str == '>' || (*str == '/' && str[1] == '>'));
        }
    }
    if (grn) {
        // <binning precision=d /> or <binning prec=d />
        granuleMap gmap;
        mapGranules(varr, gmap);
        convertGranules(gmap);
        nrows = varr.size();
    }
    else { // other types of binning
        setBoundaries(varr);
        binning(varr);
    }
    optionalUnpack(bits, spec);
    if (ibis::gVerbose > 4) {
        ibis::util::logger lg;
        lg() << "bin[" << (col ? col->fullname() : "?.?")
             << "]::construct<" << typeid(E).name() << '[' << varr.size()
             << "]> -- finished constructing a binned equality index with "
             << nobs << " bin" << (nobs>1?"s":"");
        if (ibis::gVerbose > 8) {
            lg() << "\n";
            print(lg());
        }
    }
} // ibis::bin::construct

template <typename E>
void ibis::bin::setBoundaries(const array_t<E>& varr) {
    if (varr.empty()) return; // can not do anything

    unsigned eqw = parseScale(*col);

    bounds.clear();
    if (eqw >= 10) { // attempt to generate equal-weight bins
        scanAndPartition(varr, eqw);
    }
    else { // other type of specifications
        const char* spec = (col ? col->indexSpec() : 0);
        const char* str = (spec ? strstr(spec, "<binning ") : 0);
        E vmin = 0, vmax = 0; // to store the computed min/max from varr
        ibis::bitvector mask;
        mask.set(1, varr.size());
        if (str != 0) {
            // new style of bin specification <binning ... />
            // start=xx, end=xx, nbins=xx, scale=linear|log
            // (start, end, nbins, scale)(start, end, nbins, scale)...
            double r0=0.0, r1=0.0, tmp;
            uint32_t progress = 0;
            const char *ptr = 0;
            uint32_t nb = 1;
            str += 9;
            while (isspace(*str))
                ++ str;
            bool longSpec = (*str == '(');
            if (longSpec)
                ++ str;
            while (str != 0 && *str != 0 && *str != '/' && *str != '>') {
                std::string binfile;
                if (*str == 's' || *str == 'S') { // start=|scale=
                    ptr = str + 6;
                    if (*ptr == 'l' || *ptr == 'L') {
                        // assume scale=[linear | log]
                        eqw = 1 + (ptr[1]=='o' || ptr[1]=='O');
                        progress |= 8;
                    }
                    else if (isdigit(*ptr) || *ptr == '.' ||
                             *ptr == '+' || *ptr == '-') {
                        // assume start=...
                        char *ptr2 = 0;
                        tmp = strtod(ptr, &ptr2);
                        if (tmp == 0.0 && ptr == ptr2) {
                            if (r1 > r0)
                                r0 = r1;
                            else
                                r0 = (col ? col->lowerBound() :
                                      ibis::column::computeMin(varr, mask));
                            LOGGER(ibis::gVerbose > 1)
                                <<"Warning -- bin::setBoundaries encountered a "
                                "bad indexing option \"" << str
                                << "\", assume it to be \"start=" << r0 << "\"";
                        }
                        else {
                            r0 = tmp;
                            str = ptr2;
                        }
                        progress |= 1;
                    }
                    else if (isalpha(*ptr)) {
                        eqw = 0; // default to simple a linear scale
                        progress |= 8;
                    }
                    else { // bad options
                        if (r1 > r0)
                            r0 = r1;
                        else
                            r0 = (col ? col->lowerBound() :
                                  ibis::column::computeMin(varr, mask));
                        progress |= 1;
                        if (ibis::gVerbose > 1)
                            ibis::util::logMessage
                                ("index::setBoundaries",
                                 "bad option \"%s\", assume it to be "
                                 "\"start=%G\"", str, r0);
                    }
                }
                else if (*str == 'e' || *str == 'E') { // end=
                    char *ptr2 = 0;
                    ptr = str + 4;
                    r1 = strtod(ptr, &ptr2);
                    if (r1 == 0.0 && ptr == ptr2) {
                        r1 = (col ? col->upperBound() :
                              ibis::column::computeMax(varr, mask));
                        LOGGER(ibis::gVerbose > 1)
                            <<"Warning -- bin::setBoundaries encountered a "
                            "bad indexing option \"" << str
                            << "\", assume it to be \"end=" << r1 << "\"";
                    }
                    else {
                        str = ptr2;
                    }
                    progress |= 2;
                }
                else if (*str == 'n' || *str == 'N') { // nbins=
                    ptr = strchr(str, '=');
                    if (ptr)
                        nb = static_cast<uint32_t>(strtod(ptr+1, 0));
                    if (nb == 0)
                        nb = (longSpec ? 1 : IBIS_DEFAULT_NBINS);
                    progress |= 4;
                }
                else if (isdigit(*str) || *str == '.' ||
                         *str == '+' || *str == '-') {
                    // get a number, try start, end, and nbins successively
                    tmp = strtod(str, 0);
                    if ((progress&7) == 0) {
                        r0 = tmp;
                        progress |= 1;
                    }
                    else if ((progress&7) == 1) {
                        r1 = tmp;
                        progress |= 3;
                    }
                    else if ((progress&7) == 3) {
                        nb = static_cast<uint32_t>(tmp);
                        progress |= 7;
                    }
                    else {
                        LOGGER(ibis::gVerbose > 0)
                            << "Warning -- index::setBoundaries encountered "
                            "a syntax error: labeled elements must appear "
                            "after the unlabeled ones -- skipping value " << tmp;
                    }
                }
                else if (*str == 'l' || *str == 'L') {
                    eqw = 1 + (str[1]=='o' || str[1]=='O');
                    progress |= 8;
                }
                else if (strnicmp(str, "binFile=", 8) == 0 ||
                         strnicmp(str, "file=", 5) == 0) {
                    str += (*str=='b'?8:5);
                    int ierr =
                        ibis::util::readString(binfile, str, ",; \t()/>");
                    if (ierr >= 0 && ! binfile.empty())
                        progress |= 11;
                }
                str = strpbrk(str, ",; \t()/>");
                str += strspn(str, ",; \t"); // skip space
                bool add = (progress == 15);
                if (str == 0) { // end of string
                    add = 1;
                }
                else if (*str == '/' || *str == '>') {
                    add = 1;
                }
                else if (*str == ')' || *str == '(') {
                    if ((progress&3) == 3)
                        add = 1;
                }
                if (add) {
                    if (binfile.empty()) {
                        if ((progress & 1) == 0)
                            r0 = (col ? col->lowerBound() :
                                  ibis::column::computeMin(varr, mask));
                        if ((progress & 2) == 0)
                            r1 = (col ? col->upperBound() :
                                  ibis::column::computeMax(varr, mask));
                        if ((progress & 4) == 0)
                            nb = (longSpec ? 1 : IBIS_DEFAULT_NBINS);
                        if (r0 > r1 && (progress & 3) < 3) {
                            // no expected range
                            if (vmin == vmax && vmin == 0 &&
                                (vmin != varr[0] || vmin != varr.back())) {
                                // need to compute the actual min/max
                                vmin = varr[0];
                                vmax = varr[0];
                                for (uint32_t i = 1; i < varr.size(); ++ i) {
                                    if (vmin > varr[i])
                                        vmin = varr[i];
                                    if (vmax < varr[i])
                                        vmax = varr[i];
                                }
                            }
                            if ((progress & 1) == 0)
                                r0 = vmin;
                            if ((progress & 2) == 0)
                                r1 = vmax;
                        }
                        addBounds(r0, r1, nb, eqw);
                    }
                    else { // attempt to read the named file
                        if ((progress & 4) == 0)
                            readBinBoundaries(binfile.c_str(), 0);
                        else
                            readBinBoundaries(binfile.c_str(), nb);
                        binfile.erase();
                    }
                    progress = 0;
                }
                if (str != 0) {
                    if (*str == ')' || *str == '(') {
                        str += strspn(str, ",; \t)(");
                    }
                }
            }
        }
        else if ((str = (spec ? strstr(spec, "bins:") : 0)) != 0) {
            // use explicitly specified bin boundaries
            double r0, r1;
            const char* ptr = strchr(str, '[');
            while (ptr) {
                ++ ptr;
                while (isspace(*ptr)) // skip leading space
                    ++ ptr;
                str = strpbrk(ptr, ",; \t)");
                if (ptr == str) {
                    r0 = (col ? col->lowerBound() :
                          ibis::column::computeMin(varr, mask));
                }
                else {
                    r0 = strtod(ptr, 0);
                }
                if (str != 0) {
                    ptr = str + (*str != ')');
                    while (isspace(*ptr))
                        ++ ptr;
                    str = strpbrk(ptr, ",; \t)");
                    if (ptr == str) {
                        r1 = (col ? col->upperBound() :
                              ibis::column::computeMax(varr, mask));
                    }
                    else {
                        r1 = strtod(ptr, 0);
                    }
                }
                else {
                    r1 = (col ? col->upperBound() :
                          ibis::column::computeMax(varr, mask));
                }
                if (r0 > r1) { // no expected range
                    if (vmin == vmax && vmin == 0 &&
                        (0 != varr[0] || 0 != varr.back())) {
                        // need to compute the actual min/max
                        vmin = varr[0];
                        vmax = varr[0];
                        for (uint32_t i = 1; i < varr.size(); ++ i) {
                            if (vmin > varr[i])
                                vmin = varr[i];
                            if (vmax < varr[i])
                                vmax = varr[i];
                        }
                    }
                    r0 = vmin;
                    r1 = vmax;
                }

                uint32_t nb = 1;
                ptr = str + (*str != ')');
                while (isspace(*ptr))
                    ++ ptr;
                if (*ptr == 'n') {
                    ptr += strspn(ptr, "nobins= \t");
                    nb = static_cast<uint32_t>(strtod(ptr, 0));
                }
                else if (isdigit(*ptr) || *ptr == '.') {
                    nb = static_cast<uint32_t>(strtod(ptr, 0));
                }
                if (nb == 0)
                    nb = 1;

                // the bulk of the work is done here
                addBounds(r0, r1, nb, eqw);

                // ready for the next group
                ptr = strchr(ptr, '[');
            }
            bounds.push_back(ibis::util::compactValue
                             (bounds.back(), DBL_MAX));
        }
        else if (spec != 0) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- bin::binning encountered a bad index spec \""
                << spec << "\", do you mean \"<binning " << spec << "/>\"";
        }
        else {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- bin::binning does not know how to bin the "
                "given values";
        }
    }

    if (bounds.empty()) {
        if (eqw < 10) { // bad bin spec, try approximate equal depth bin
            eqw = 11;
            scanAndPartition(varr, eqw);
        }
        else if (col->lowerBound() >= col->upperBound()) {
            // has not set any thing yet
            double aver = 0.5 * (col->lowerBound() + col->upperBound());
            double diff = 0.5 * (col->lowerBound() - col->upperBound());
            if (! (fabs(diff) > 0.0) ||
                ! (fabs(diff) > DBL_EPSILON * aver))
                diff = 1.0;
            bounds.push_back(aver-diff);
            bounds.push_back(aver+diff);
        }
    }

    if (bounds.size() > 1) {
        // ensure bounds are sorted and are unique
        std::sort(bounds.begin(), bounds.end());
        const uint32_t nb1 = bounds.size();
#if DEBUG+0 > 0 || _DEBUG+0 > 0
        {
            ibis::util::logger lg(4);
            lg() << "DEBUG -- bin bounds before duplicate removal\n";
            for (unsigned i = 0; i < nb1; ++ i)
                lg() << bounds[i] << " ";
        }
#endif
        {
            unsigned i, j;
            for (i = 0, j = 1; j < nb1; ++ j) {
                if (bounds[j] > bounds[i]) {
                    if (j > i+1)
                        bounds[i+1] = bounds[j];
                    ++ i;
                }
                else {
                    LOGGER(ibis::gVerbose > 6)
                        << "bin::setBoundaries is to skip bounds[" << j
                        << "] (" << bounds[j] << ") because it is too close "
                        "to bounds[" << i << "] (" << bounds[i] << ")";
                }
            }
            bounds.resize(i+1);
        }
#if DEBUG+0 > 0 || _DEBUG+0 > 0
        {
            ibis::util::logger lg(4);
            lg() << "DEBUG -- bin bounds after duplicate removal\n";
            for (unsigned i = 0; i < bounds.size(); ++ i)
                lg() << bounds[i] << " ";
        }
#endif
    }

    if (! bounds.empty()) {
        // add DBL_MAX as the last value
        bounds.push_back(DBL_MAX);
    }

    nobs = bounds.size();
    if (ibis::gVerbose > 5) {
        ibis::util::logger ostr;
        ostr() << "bin::setBoundaries -- bounds[" << nobs << "] = {" << bounds[0];
        uint32_t nprt = (ibis::gVerbose > 30 ? nobs : (1 << ibis::gVerbose));
        if (nprt > nobs)
            nprt = nobs;
        for (uint32_t i = 1; i < nprt; ++ i)
            ostr() << ", " << bounds[i];
        if (nprt < nobs)
            ostr() << ", ... (" << nobs-nprt << " omitted)";
        ostr() << "}";
    }
} // ibis::bin::setBoundaries

template <typename E>
void ibis::bin::binning(const array_t<E>& varr, const array_t<double>& bd) {
    if (bd.size() <= 2) {
        // bd has no valid values, parse bin spec in a minimal way
        setBoundaries(varr);
    }
    else {
        bounds.deepCopy(bd);
        if (bounds.back() < DBL_MAX)
            bounds.push_back(DBL_MAX);
        nobs = bounds.size();
    }
    binning(varr); // actually perform the binning work
} // ibis::bin::binning (specified bin boundaries)

template <typename E>
void ibis::bin::binning(const array_t<E>& varr) {
    if (varr.size() <= 0) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- bin::binning can not proceed with an empty array";
        return;
    }
    horometer timer;
    if (ibis::gVerbose > 4)
        timer.start();

    ibis::index::clear(); // delete the existing bitmaps
    // allocate space for min and max values and initialize them to extreme
    // values
    nrows = varr.size();
    bits.resize(nobs);
    maxval.resize(nobs);
    minval.resize(nobs);
    for (uint32_t i = 0; i < nobs; ++i) {
        minval[i] = DBL_MAX;
        maxval[i] = -DBL_MAX;
        bits[i] = new ibis::bitvector;
    }

    // main loop -- going through every value
    for (uint32_t i = 0; i < nrows; ++i) {
        uint32_t j = locate(varr[i]);
        if (j < nobs) {
            bits[j]->setBit(i, 1);
            if (minval[j] > varr[i])
                minval[j] = varr[i];
            if (maxval[j] < varr[i])
                maxval[j] = varr[i];
        }
    }

    // make sure all bit vectors are the same size
    for (uint32_t i = 0; i < nobs; ++i) {
        if (bits[i]->cnt() > 0) {
            bits[i]->adjustSize(0, nrows);
            //  bits[i]->compress();
        }
        else { // delete the empty bitvector
            delete bits[i];
            bits[i]  = 0;
        }
    }

    // remove empty bins
    if (nobs > 0) {
        -- nobs;
        uint32_t k = 1;
        for (uint32_t i = 1; i < nobs; ++ i) {
            if (bits[i] != 0) {
                if (i > k) {
                    // copy [i] to [k]
                    bounds[k] = bounds[i];
                    minval[k] = minval[i];
                    maxval[k] = maxval[i];
                    bits[k] = bits[i];
                }
                ++ k;
            }
        }
        if (nobs > k) {
            bounds[k] = bounds[nobs];
            minval[k] = minval[nobs];
            maxval[k] = maxval[nobs];
            bits[k] = bits[nobs];
            ++ k;
            bounds.resize(k);
            minval.resize(k);
            maxval.resize(k);
            bits.resize(k);
            nobs = k;
        }
        else {
            ++ nobs;
        }
    }

    // write info about the bins
    if (ibis::gVerbose > 2) {
        ibis::util::logger lg;
        lg() << "bin::binning partitioned " << nrows << ' '
             << typeid(E).name() << " values into " << nobs-2
             << " bin(s) + 2 outside bins";
        if (ibis::gVerbose > 4) {
            timer.stop();
            ibis::util::logger lg;
            lg() << " in " << timer.realTime() << "sec(elapsed)";
        }
        if (ibis::gVerbose > 6) {
            lg() << "\n[minval, maxval]\tbound\tcount\n";
            for (uint32_t i = 0; i < nobs; ++i)
                lg() << "[" << minval[i] << ", " << maxval[i] << "]\t"
                     << bounds[i] << "\t" << (bits[i] ? bits[i]->cnt() : 0)
                     << "\n";
        }
    }
} // ibis::bin::binning

template <typename E>
void ibis::bin::mapGranules(const array_t<E>& val,
                            ibis::bin::granuleMap& gmap) const {
    if (val.empty()) return;
    gmap.clear();
    horometer timer;
    if (ibis::gVerbose > 4)
        timer.start();

    const uint32_t prec = parsePrec(*col); // the precision of mapped value
    const uint32_t nev = val.size();

    for (uint32_t i = 0; i < nev; ++i) {
        double key = ibis::util::coarsen(val[i], prec);
        ibis::bin::granuleMap::iterator it = gmap.find(key);
        ibis::bin::granule *grn = 0;
        if (it == gmap.end()) {
            grn = new ibis::bin::granule;
            gmap[key] = grn;
            grn->loce = new ibis::bitvector;
            grn->locm = new ibis::bitvector;
            grn->locp = new ibis::bitvector;
        }
        else {
            grn = (*it).second;
        }

        if (val[i] < key) {
            grn->locm->setBit(i, 1);
            if (grn->minm > val[i])
                grn->minm = val[i];
            if (grn->maxm < val[i])
                grn->maxm = val[i];
        }
        else if (val[i] == key) {
            grn->loce->setBit(i, 1);
        }
        else { // assume the incoming value is larger, which may include NaN
            grn->locp->setBit(i, 1);
            if (grn->minp > val[i])
                grn->minp = val[i];
            if (grn->maxp < val[i])
                grn->maxp = val[i];
        }
    }

    // make sure all bit vectors are the same size, deallocate the empty
    // ones
    for (ibis::bin::granuleMap::iterator it = gmap.begin();
         it != gmap.end(); ++ it) {
        if ((*it).second->loce->cnt() > 0) {
            (*it).second->loce->adjustSize(0, nev);
        }
        else {
            delete (*it).second->loce;
            (*it).second->loce = 0;
        }
        if ((*it).second->locm->cnt() > 0) {
            (*it).second->locm->adjustSize(0, nev);
        }
        else {
            delete (*it).second->locm;
            (*it).second->locm = 0;
        }
        if ((*it).second->locp->cnt() > 0) {
            (*it).second->locp->adjustSize(0, nev);
        }
        else {
            delete (*it).second->locp;
            (*it).second->locp = 0;
        }
    }

    // write info about the bins
    if (ibis::gVerbose > 2) {
        ibis::util::logger lg;
        lg() << "bin::mapGranules mapped " << nev << " values to " << gmap.size()
             << ' ' << prec << "-digit number" << (gmap.size() > 1 ? "s" : "");
        if (ibis::gVerbose > 4) {
            timer.stop();
            lg() << " in " << timer.realTime() << " sec(elapsed)";
        }
        if (ibis::gVerbose > 6) {
            printGranules(lg(), gmap);
        }
    }
} // ibis::bin::mapGranules

void ibis::bin::printGranules(std::ostream& out,
                              const ibis::bin::granuleMap& bmap) const {
    out << "bin::printGranules(" << bmap.size()
        << (bmap.size() > 1 ? " entries" : " entry")
        << ")\nkey: count=, count_, min_, max_, count^, min^, max^\n";
    if (ibis::gVerbose > 7)
        out << std::setprecision(18);
    else if (ibis::gVerbose > 5)
        out << std::setprecision(14);
    else if (ibis::gVerbose > 3)
        out << std::setprecision(10);
    uint32_t prt = (ibis::gVerbose > 30 ? bmap.size() : (1 << ibis::gVerbose));
    if (prt < 5) prt = 5;
    if (prt+1 >= bmap.size()) { // print all
        for (granuleMap::const_iterator it = bmap.begin();
             it != bmap.end(); ++ it) {
            out << (*it).first << ":\t";
            if ((*it).second->loce)
                out << (*it).second->loce->cnt();
            if ((*it).second->locm)
                out << ",\t" << (*it).second->locm->cnt()
                    << ",\t" << (*it).second->minm
                    << ",\t" << (*it).second->maxm;
            else
                out << ",\t,\t,\t";
            if ((*it).second->locp)
                out << ",\t" << (*it).second->locp->cnt()
                    << ",\t" << (*it).second->minp
                    << ",\t" << (*it).second->maxp << "\n";
            else
                out << ",\t,\t,\t\n";
        }
    }
    else { // print some
        granuleMap::const_iterator it = bmap.begin();
        for (uint32_t i = 0; i < prt; ++i, ++it) {
            out << (*it).first << ":\t";
            if ((*it).second->loce)
                out << (*it).second->loce->cnt();
            if ((*it).second->locm)
                out << ",\t" << (*it).second->locm->cnt()
                    << ",\t" << (*it).second->minm
                    << ",\t" << (*it).second->maxm;
            else
                out << ",\t,\t,\t";
            if ((*it).second->locp)
                out << ",\t" << (*it).second->locp->cnt()
                    << ",\t" << (*it).second->minp
                    << ",\t" << (*it).second->maxp << "\n";
            else
                out << ",\t,\t,\t\n";
        }
        prt =  bmap.size() - prt - 1;
        it = bmap.end();
        -- it;
        out << "...\n" << prt << (prt > 1 ? " entries" : " entry")
            << " omitted\n...\n";
        out << (*it).first << ":\t";
        if ((*it).second->loce)
            out << (*it).second->loce->cnt();
        if ((*it).second->locm)
            out << ",\t" << (*it).second->locm->cnt()
                << ",\t" << (*it).second->minm
                << ",\t" << (*it).second->maxm;
        else
            out << ",\t,\t,\t";
        if ((*it).second->locp)
            out << ",\t" << (*it).second->locp->cnt()
                << ",\t" << (*it).second->minp
                << ",\t" << (*it).second->maxp << "\n";
        else
            out << ",\t,\t,\t\n";
    }
    out << std::endl;
} //ibis::bin::printGranules

/// Convert the granule map into binned index.  The bitmaps that are not
/// empty are transferred to the array bits, and the empty bitmaps are
/// deleted.  Therefore, the content of gmap is no longer valid after
/// calling this function.  The only thing that could be done to the
/// granuleMap object is to free it.
void ibis::bin::convertGranules(ibis::bin::granuleMap& gmap) {
    // clear the existing content
    clear();
    // reserve space
    bits.reserve(gmap.size()*3);
    bounds.reserve(gmap.size()*3);
    minval.reserve(gmap.size()*3);
    maxval.reserve(gmap.size()*3);

    // copy the values
    for (granuleMap::iterator it = gmap.begin(); it != gmap.end(); ++it) {
        if ((*it).second->locm != 0 && (*it).second->locm->cnt() > 0) {
            if (maxval.size() > 0)
                bounds.push_back(ibis::util::compactValue
                                 (maxval.back(), (*it).second->minm));
            if (nrows < (*it).second->locm->size())
                nrows = (*it).second->locm->size();
            minval.push_back((*it).second->minm);
            maxval.push_back((*it).second->maxm);
            bits.push_back((*it).second->locm);
        }
        else {
            delete (*it).second->locm;
        }
        (*it).second->locm = 0;

        if ((*it).second->loce != 0 && (*it).second->loce->cnt() > 0) {
            if (maxval.size() > 0)
                bounds.push_back((*it).first);
            if (nrows < (*it).second->loce->size())
                nrows = (*it).second->loce->size();
            minval.push_back((*it).first);
            maxval.push_back((*it).first);
            bits.push_back((*it).second->loce);
        }
        else {
            delete (*it).second->loce;
        }
        (*it).second->loce = 0;

        if ((*it).second->locp != 0 && (*it).second->locp->cnt() > 0) {
            if (maxval.size() > 0)
                bounds.push_back(ibis::util::compactValue
                                 (maxval.back(), (*it).second->minp));
            if (nrows < (*it).second->locp->size())
                nrows = (*it).second->locp->size();
            minval.push_back((*it).second->minp);
            maxval.push_back((*it).second->maxp);
            bits.push_back((*it).second->locp);
        }
        else {
            delete (*it).second->locp;
        }
        (*it).second->locp = 0;

        delete (*it).second;
        (*it).second = 0;
    }
    // append DBL_MAX as the bin boundary at the end
    bounds.push_back(DBL_MAX);
    nobs = bits.size();
    offset64.resize(nobs+1);
    offset64[0] = 0;
    for (size_t j = 0; j < nobs; ++ j)
        offset64[j+1] = offset64[j] + bits[j]->getSerialSize();
    LOGGER(ibis::gVerbose > 4)
        << "bin::convertGranules converted " << gmap.size() << " granule"
        << (gmap.size()>1?"s":"") << " into " << nobs << " bin"
        << (nobs>1?"s":"");
} // ibis::bin::convertGranules

/// Parse the index specs to determine eqw and nbins.
///
/// Parse the index specification to determine the number of bins, returns
/// IBIS_DEFAULT_NBINS if it is not specified.
uint32_t ibis::bin::parseNbins(const ibis::column &c) {
    uint32_t nbins = 0;
    const char* bspec = c.indexSpec();
    const char* str = 0;
    if (bspec != 0) {
        str = strstr(bspec, "nbins=");
        if (str) {
            str += 6;
            nbins = static_cast<uint32_t>(strtod(str, 0));
        }
        else {
            str = strstr(bspec, "nbins =");
            if (str) {
                str += 7;
                nbins = static_cast<uint32_t>(strtod(str, 0));
            }
            else {
                str = strstr(bspec, "no=");
                if (str) {
                    str += 3;
                    nbins = static_cast<uint32_t>(strtod(str, 0));
                }
                else {
                    str = strstr(bspec, "no =");
                    if (str) {
                        str += 4;
                        nbins = static_cast<uint32_t>(strtod(str, 0));
                    }
                }
            }
        }
    }
    if (nbins == 0 && c.partition() != 0) {
        bspec = c.partition()->indexSpec();
        if (bspec != 0) {
            str = strstr(bspec, "nbins=");
            if (str) {
                str += 6;
                nbins = static_cast<uint32_t>(strtod(str, 0));
            }
            else {
                str = strstr(bspec, "nbins =");
                if (str) {
                    str += 7;
                    nbins = static_cast<uint32_t>(strtod(str, 0));
                }
                else {
                    str = strstr(bspec, "no=");
                    if (str) {
                        str += 3;
                        nbins = static_cast<uint32_t>(strtod(str, 0));
                    }
                    else {
                        str = strstr(bspec, "no =");
                        if (str) {
                            str += 4;
                            nbins = static_cast<uint32_t>(strtod(str, 0));
                        }
                    }
                }
            }
        }
    }
    if (nbins == 0) {
        std::string tmp;
        if (c.partition() != 0) {
            tmp = c.partition()->name();
            tmp += '.';
        }
        tmp += c.name();
        tmp += ".index";
        bspec = ibis::gParameters()[tmp.c_str()];
        if (bspec != 0) {
            str = strstr(bspec, "nbins=");
            if (str) {
                str += 6;
                nbins = static_cast<uint32_t>(strtod(str, 0));
            }
            else {
                str = strstr(bspec, "nbins =");
                if (str) {
                    str += 7;
                    nbins = static_cast<uint32_t>(strtod(str, 0));
                }
                else {
                    str = strstr(bspec, "no=");
                    if (str) {
                        str += 3;
                        nbins = static_cast<uint32_t>(strtod(str, 0));
                    }
                    else {
                        str = strstr(bspec, "no =");
                        if (str) {
                            str += 4;
                            nbins = static_cast<uint32_t>(strtod(str, 0));
                        }
                    }
                }
            }
        }
    }

    if (nbins == 0)
        nbins = IBIS_DEFAULT_NBINS;
    return nbins;
} // ibis::bin::parseNbins

/// Parse the specification about scaling.
///
/// Parse scale=xx (or the old form equalxx) in the index specification
/// - 0 -- simple linear scale (used when "scale=" is present, but is not
///        "scale=linear" or "scale=log")
/// - 1 -- equal length [linear scale]
/// - 2 -- equal ratio [log scale]
/// - 10 -- equal weight
/// - UINT_MAX -- default value if no index specification is found.
unsigned ibis::bin::parseScale(const ibis::column &c) {
    const char* bspec = c.indexSpec();
    if (bspec == 0) {
        if (c.partition() != 0)
            bspec = c.partition()->indexSpec();
        if (bspec == 0) {
            std::string tmp;
            if (c.partition() != 0) {
                tmp = c.partition()->name();
                tmp += '.';
            }
            tmp += c.name();
            tmp += ".index";
            bspec = ibis::gParameters()[tmp.c_str()];
        }
    }
    return parseScale(bspec);
} // ibis::bin::parseScale

unsigned ibis::bin::parseScale(const char* spec) {
    unsigned eq = UINT_MAX;
    if (spec && *spec) {
        const char* ptr;
        ptr = strstr(spec, "scale=");
        if (ptr != 0) { // scale=[linear|log]
            ptr += 6;
            if (*ptr == 'L' || *ptr == 'l') {
                if (ptr[1] == 'O' || ptr[1] == 'o')
                    eq = 2;
                else
                    eq = 1;
            }
            else {
                eq = 0;
            }
        }
        else if ((ptr = strstr(spec, "scale =")) != 0) {
            ptr += 7;
            if (*ptr == 'L' || *ptr == 'l') {
                if (ptr[1] == 'O' || ptr[1] == 'o')
                    eq = 2;
                else
                    eq = 1;
            }
            else {
                eq = 0;
            }
        }
        else if ((ptr = strstr(spec, "equal")) != 0 &&
                 strncmp(ptr, "equality", 8) != 0) {
            ptr += 5;
            ptr += strspn(ptr, "_- \t");
            if (strncmp(ptr, "ratio", 5) == 0) {
                eq = 2;
            }
            else if (strncmp(ptr, "weight", 6) == 0) {
                eq = 10;
            }
            else {
                eq = 0;
            }
        }
        else if ((ptr = strstr(spec, "log")) != 0) {
            ptr += 3;
            ptr += strspn(ptr, "_- \t");
            if (strncmp(ptr, "scale", 5) == 0) {
                eq = 2;
            }
        }
        else if (strnicmp(spec, "bins:", 4) == 0 || strchr(spec, '(') ||
                 strstr(spec, "start=") || strstr(spec, "end=") ||
                 strstr(spec, "ile=") ||
                 strstr(spec, "start =") || strstr(spec, "end =") ||
                 strstr(spec, "ile =")) {
            eq = 0; // a place holder for more complex options
        }
    }
    return eq;
} // ibis::bin::parseScale

/// Parse the index spec to extract precision.
unsigned ibis::bin::parsePrec(const ibis::column &c) {
    unsigned prec = 0;
    const char* bspec = c.indexSpec();
    const char* str = 0;
    if (bspec != 0) {
        str = strstr(bspec, "precision=");
        if (str) {
            str += 10;
        }
        else {
            str = strstr(bspec, "precision =");
            if (str) {
                str += 11;
            }
            else {
                str = strstr(bspec, "prec=");
                if (str) {
                    str += 5;
                }
                else if ((str = strstr(bspec, "prec =")) != 0) {
                    str += 6;
                }
            }
        }
        if (str && *str)
            prec = static_cast<unsigned>(strtod(str, 0));
    }
    if (prec == 0 && c.partition() != 0) {
        bspec = c.partition()->indexSpec();
        if (bspec != 0) {
            str = strstr(bspec, "precision=");
            if (str) {
                str += 10;
            }
            else {
                str = strstr(bspec, "precision =");
                if (str) {
                    str += 11;
                }
                else {
                    str = strstr(bspec, "prec=");
                    if (str) {
                        str += 5;
                    }
                    else if ((str = strstr(bspec, "prec =")) != 0) {
                        str += 6;
                    }
                }
            }
            if (str && *str)
                prec = static_cast<unsigned>(strtod(str, 0));
        }
    }
    if (prec == 0) {
        std::string tmp;
        if (c.partition() != 0) {
            tmp = c.partition()->name();
            tmp += '.';
        }
        tmp += c.name();
        tmp += ".index";
        bspec = ibis::gParameters()[tmp.c_str()];
        if (bspec != 0) {
            str = strstr(bspec, "precision=");
            if (str) {
                str += 10;
            }
            else {
                str = strstr(bspec, "precision =");
                if (str) {
                    str += 11;
                }
                else {
                    str = strstr(bspec, "prec=");
                    if (str) {
                        str += 5;
                    }
                    else if ((str = strstr(bspec, "prec =")) != 0) {
                        str += 6;
                    }
                }
            }
            if (str && *str)
                prec = static_cast<unsigned>(strtod(str, 0));
        }
    }
    if (prec == 0) // default to 2 digit precision
        prec = 2;
    return prec;
} // ibis::bin::parsePrec

// add bin boundaries to array bounds
void ibis::bin::addBounds(double lbd, double rbd, uint32_t nbins,
                          uint32_t eqw) {
    if (col == 0) return;
    double diff = rbd - lbd;
    if (!(diff > DBL_MIN)) { // rbd <= lbd, or invalid number
        if (fabs(lbd) < DBL_MAX)
            bounds.push_back(ibis::util::compactValue(lbd-0.5, lbd+0.5));
    }
    else if (nbins < 2) {
        bounds.push_back(ibis::util::compactValue(lbd, rbd));
    }
    else if (static_cast<uint32_t>(diff) <= nbins &&
             col->type() != ibis::FLOAT &&
             col->type() != ibis::DOUBLE) {
        for (long ib = static_cast<long>(lbd);
             ib <= static_cast<long>(rbd);
             ++ ib)
            bounds.push_back(ib);
    }
    else if (eqw > 1) { // equal ratio subdivisions
        if (col->type() != ibis::FLOAT &&
            col->type() != ibis::DOUBLE) { // integers
            // make sure bin boundaries are integers
            lbd = floor(lbd);
            bounds.push_back(lbd);
            if (diff < nbins*1.5) {
                for (uint32_t i = 1; i < static_cast<uint32_t>(diff); ++i)
                    bounds.push_back(lbd+i);
            }
            else {
                -- nbins;
                if (lbd < 1.0) lbd = 1.0;
                diff = pow(rbd / lbd, 1.0 / nbins);
                if (lbd <= bounds.back())
                    lbd *= diff;
                while (lbd < rbd && nbins > 0) {
                    double tmp = floor(lbd+0.5);
                    if (tmp > bounds.back()) {
                        bounds.push_back(tmp);
                        -- nbins;
                    }
                    else {
                        lbd = bounds.back() + 1;
                        bounds.push_back(lbd);
                        -- nbins;
                        if (nbins > 0) {
                            diff = pow(rbd / lbd, 1.0 / nbins);
                        }
                    }
                    lbd *= diff;
                }
            }
        }
        else if (!(fabs(lbd) > DBL_MIN) && !(fabs(rbd) > DBL_MIN)) {
            // both lbd and rbd are abnormal floating-point values
            bounds.push_back(0.0);
        }
        else if (lbd > DBL_MIN) { // both lbd and rbd are positive
            lbd = pow(10.0, floor(FLT_EPSILON+log10(lbd)));
            uint32_t ord = static_cast<uint32_t>(0.5+log10(rbd / lbd));
            if (static_cast<uint32_t>(ord*9.5) <= nbins) {
                // ten or more bins per order of magnitude
                if (ord > 0)
                    diff = pow(10.0, 1.0 / floor(0.5 + nbins / ord));
                else
                    diff = 2.0;
                const double fac0 = sqrt(1.0/diff);
                const double fac1 = sqrt(diff);
                bounds.push_back(lbd);
                lbd *= diff;
                while (lbd < rbd) {
                    bounds.push_back(ibis::util::compactValue
                                     (lbd*fac0, lbd*fac1));
                    lbd *= diff;
                }
            }
            else if (static_cast<uint32_t>(ord*8.5) <= nbins) {
                // nine bins per order of magnitude
                while (lbd*(1.0+DBL_EPSILON) < rbd) {
                    bounds.push_back(lbd);
                    if (2*lbd < rbd)
                        bounds.push_back(2*lbd);
                    if (3*lbd < rbd)
                        bounds.push_back(3*lbd);
                    if (4*lbd < rbd)
                        bounds.push_back(4*lbd);
                    if (5*lbd < rbd)
                        bounds.push_back(5*lbd);
                    if (6*lbd < rbd)
                        bounds.push_back(6*lbd);
                    if (7*lbd < rbd)
                        bounds.push_back(7*lbd);
                    if (8*lbd < rbd)
                        bounds.push_back(8*lbd);
                    if (9*lbd < rbd)
                        bounds.push_back(9*lbd);
                    lbd *= 10.0;
                }
            }
            else if (static_cast<uint32_t>(ord*7.5) <= nbins) {
                // eight bins per order of magnitude
                while (lbd*(1.0+DBL_EPSILON) < rbd) {
                    bounds.push_back(lbd);
                    if (2*lbd < rbd)
                        bounds.push_back(2*lbd);
                    if (3*lbd < rbd)
                        bounds.push_back(3*lbd);
                    if (4*lbd < rbd)
                        bounds.push_back(4*lbd);
                    if (5*lbd < rbd)
                        bounds.push_back(5*lbd);
                    if (6*lbd < rbd)
                        bounds.push_back(6*lbd);
                    if (7*lbd < rbd)
                        bounds.push_back(7*lbd);
                    if (8*lbd < rbd)
                        bounds.push_back(8*lbd);
                    lbd *= 10.0;
                }
            }
            else if (static_cast<uint32_t>(ord*6.5) <= nbins) {
                // seven bins per order of magnitude
                while (lbd*(1.0+DBL_EPSILON) < rbd) {
                    bounds.push_back(lbd);
                    if (2*lbd < rbd)
                        bounds.push_back(2*lbd);
                    if (3*lbd < rbd)
                        bounds.push_back(3*lbd);
                    if (4*lbd < rbd)
                        bounds.push_back(4*lbd);
                    if (5*lbd < rbd)
                        bounds.push_back(5*lbd);
                    if (6*lbd < rbd)
                        bounds.push_back(6*lbd);
                    if (8*lbd < rbd)
                        bounds.push_back(8*lbd);
                    lbd *= 10.0;
                }
            }
            else if (static_cast<uint32_t>(ord*5.5) <= nbins) {
                // six bins per order of manitude
                while (lbd*(1.0+DBL_EPSILON) < rbd) {
                    bounds.push_back(lbd);
                    if (2*lbd < rbd)
                        bounds.push_back(2*lbd);
                    if (3*lbd < rbd)
                        bounds.push_back(3*lbd);
                    if (4*lbd < rbd)
                        bounds.push_back(4*lbd);
                    if (5*lbd < rbd)
                        bounds.push_back(5*lbd);
                    if (8*lbd < rbd)
                        bounds.push_back(8*lbd);
                    lbd *= 10.0;
                }
            }
            else if (static_cast<uint32_t>(ord*4.5) <= nbins) {
                // five bins per order of magnitude
                while (lbd*(1.0+DBL_EPSILON) < rbd) {
                    bounds.push_back(lbd);
                    if (2*lbd < rbd)
                        bounds.push_back(2*lbd);
                    if (3*lbd < rbd)
                        bounds.push_back(3*lbd);
                    if (5*lbd < rbd)
                        bounds.push_back(5*lbd);
                    if (8*lbd < rbd)
                        bounds.push_back(8*lbd);
                    lbd *= 10.0;
                }
            }
            else if (static_cast<uint32_t>(ord*3.5) <= nbins) {
                // four bins per order of magnitude
                while (lbd*(1.0+DBL_EPSILON) < rbd) {
                    bounds.push_back(lbd);
                    if (2*lbd < rbd)
                        bounds.push_back(2*lbd);
                    if (3*lbd < rbd)
                        bounds.push_back(3*lbd);
                    if (5*lbd < rbd)
                        bounds.push_back(5*lbd);
                    lbd *= 10.0;
                }
            }
            else if (static_cast<uint32_t>(ord*2.5) <= nbins) {
                // three bins per order of magnitude
                while (lbd*(1.0+DBL_EPSILON) < rbd) {
                    bounds.push_back(lbd);
                    if (2*lbd < rbd)
                        bounds.push_back(2*lbd);
                    if (5*lbd < rbd)
                        bounds.push_back(5*lbd);
                    lbd *= 10.0;
                }
            }
            else if (static_cast<uint32_t>(ord*1.5) <= nbins) {
                // two bins per order of magnitude
                while (lbd*(1.0+DBL_EPSILON) < rbd) {
                    bounds.push_back(lbd);
                    if (5*lbd < rbd)
                        bounds.push_back(5*lbd);
                    lbd *= 10.0;
                }
            }
            else { // at least one bin per order of magnitude
                while (lbd*(1.0+DBL_EPSILON) < rbd) {
                    bounds.push_back(lbd);
                    lbd *= 10.0;
                }
            }
        }
        else if (rbd < -DBL_MIN) { // both lbd and rbd are negative
            rbd = -pow(10.0, floor(log10(-rbd)));
            uint32_t ord = static_cast<uint32_t>(0.5+log10(lbd / rbd));
            if (static_cast<uint32_t>(ord*9.5) <= nbins) {
                // ten or more bins per order of magnitude
                if (ord > 0)
                    diff = pow(10.0, 1.0 / floor(0.5 + nbins / ord));
                else
                    diff = 2.0;
                const double fac0 = sqrt(1.0/diff);
                const double fac1 = sqrt(diff);
                bounds.push_back(rbd);
                rbd *= diff;
                while (lbd < rbd) {
                    bounds.push_back(ibis::util::compactValue
                                     (rbd*fac0, rbd*fac1));
                    rbd *= diff;
                }
            }
            else if (static_cast<uint32_t>(ord*8.5) <= nbins) {
                // nine bins per order of magnitude
                while (rbd*(1.0+DBL_EPSILON) < lbd) {
                    bounds.push_back(rbd);
                    if (2*rbd > lbd) bounds.push_back(2*rbd);
                    if (3*rbd > lbd) bounds.push_back(3*rbd);
                    if (4*rbd > lbd) bounds.push_back(4*rbd);
                    if (5*rbd > lbd) bounds.push_back(5*rbd);
                    if (6*rbd > lbd) bounds.push_back(6*rbd);
                    if (7*rbd > lbd) bounds.push_back(7*rbd);
                    if (8*rbd > lbd) bounds.push_back(8*rbd);
                    if (9*rbd > lbd) bounds.push_back(9*rbd);
                    rbd *= 10.0;
                }
            }
            else if (static_cast<uint32_t>(ord*7.5) <= nbins) {
                // eight bins per order of magnitude
                while (rbd*(1.0+DBL_EPSILON) > lbd) {
                    bounds.push_back(rbd);
                    if (2*rbd > lbd) bounds.push_back(2*rbd);
                    if (3*rbd > lbd) bounds.push_back(3*rbd);
                    if (4*rbd > lbd) bounds.push_back(4*rbd);
                    if (5*rbd > lbd) bounds.push_back(5*rbd);
                    if (6*rbd > lbd) bounds.push_back(6*rbd);
                    if (7*rbd > lbd) bounds.push_back(7*rbd);
                    if (8*rbd > lbd) bounds.push_back(8*rbd);
                    rbd *= 10.0;
                }
            }
            else if (static_cast<uint32_t>(ord*6.5) <= nbins) {
                // seven bins per order of magnitude
                while (rbd*(1.0+DBL_EPSILON) > lbd) {
                    bounds.push_back(rbd);
                    if (2*rbd > lbd) bounds.push_back(2*rbd);
                    if (3*rbd > lbd) bounds.push_back(3*rbd);
                    if (4*rbd > lbd) bounds.push_back(4*rbd);
                    if (5*rbd > lbd) bounds.push_back(5*rbd);
                    if (6*rbd > lbd) bounds.push_back(6*rbd);
                    if (8*rbd > lbd) bounds.push_back(8*rbd);
                    rbd *= 10.0;
                }
            }
            else if (static_cast<uint32_t>(ord*5.5) <= nbins) {
                // six bins per order of magnitude
                while (rbd*(1.0+DBL_EPSILON) > lbd) {
                    bounds.push_back(rbd);
                    if (2*rbd > lbd) bounds.push_back(2*rbd);
                    if (3*rbd > lbd) bounds.push_back(3*rbd);
                    if (4*rbd > lbd) bounds.push_back(4*rbd);
                    if (5*rbd > lbd) bounds.push_back(5*rbd);
                    if (8*rbd > lbd) bounds.push_back(8*rbd);
                    rbd *= 10.0;
                }
            }
            else if (static_cast<uint32_t>(ord*4.5) <= nbins) {
                // five bins per order of magnitude
                while (rbd*(1.0+DBL_EPSILON) > lbd) {
                    bounds.push_back(rbd);
                    if (2*rbd > lbd) bounds.push_back(2*rbd);
                    if (3*rbd > lbd) bounds.push_back(3*rbd);
                    if (5*rbd > lbd) bounds.push_back(5*rbd);
                    if (8*rbd > lbd) bounds.push_back(8*rbd);
                    rbd *= 10.0;
                }
            }
            else if (static_cast<uint32_t>(ord*3.5) <= nbins) {
                // four bins per order of magnitude
                while (rbd*(1.0+DBL_EPSILON) > lbd) {
                    bounds.push_back(rbd);
                    if (2*rbd > lbd) bounds.push_back(2*rbd);
                    if (3*rbd > lbd) bounds.push_back(3*rbd);
                    if (5*rbd > lbd) bounds.push_back(5*rbd);
                    rbd *= 10.0;
                }
            }
            else if (static_cast<uint32_t>(ord*2.5) <= nbins) {
                // three bins per order of magnitude
                while (rbd*(1.0+DBL_EPSILON) > lbd) {
                    bounds.push_back(rbd);
                    if (2*rbd > lbd) bounds.push_back(2*rbd);
                    if (5*rbd > lbd) bounds.push_back(5*rbd);
                    rbd *= 10.0;
                }
            }
            else if (static_cast<uint32_t>(ord*1.5) <= nbins) {
                // two bins per order of magnitude
                while (rbd*(1.0+DBL_EPSILON) > lbd) {
                    bounds.push_back(rbd);
                    if (5*rbd > lbd)
                        bounds.push_back(5*rbd);
                    rbd *= 10.0;
                }
            }
            else { // at least one bin per order of magnitude
                while (rbd*(1.0+DBL_EPSILON) > lbd) {
                    bounds.push_back(rbd);
                    rbd *= 10.0;
                }
            }

            // make sure the left boundary is included
            if (bounds.back() > 0.5*lbd)
                bounds.push_back(ibis::util::compactValue(0.8*lbd, 2*lbd));
        }
        else if (rbd >= -lbd) { // rbd has large magnitude
            if (-lbd >= rbd * DBL_EPSILON) {
                // default value results the full precision of the double
                // precision floating-point values
                double sml = DBL_EPSILON*(-lbd);
                uint32_t nbm = static_cast<uint32_t>
                    (nbins * log(-lbd/sml) / (log(-lbd/sml) + log(rbd/sml)));
                uint32_t nbp = nbins - nbm - 1;
                if (nbp > nbm+1 || (nbp==nbm+1 && rbd >= -2.0*lbd)) {
                    // + side uses more bins than - side, use the
                    // difference between -lbd and rbd to determine a
                    // multiplying factor
                    sml = rbd * exp(nbp * log(-lbd/rbd) / (nbp-nbm));
                    double tmp = -lbd * (col->type() == ibis::FLOAT ?
                                         FLT_EPSILON : DBL_EPSILON);
                    if (sml >= -0.1 * lbd) // sml too large
                        sml = tmp;
                    else if (sml < tmp)
                        sml = tmp;
                }
                else if (col->type() == ibis::FLOAT) {
                    // for float type attempt to resolve only to the
                    // precision of the floating-point value
                    sml = rbd * FLT_EPSILON;
                }
                addBounds(lbd, -sml, nbm, eqw);
                bounds.push_back(0.0);
                addBounds(sml, rbd, nbp, eqw);
            }
            else { // the absolute value of lbd is very small
                if (lbd < 0.0)
                    bounds.push_back(ibis::util::compactValue(3*lbd, 0.2*lbd));
                bounds.push_back(0.0);
                if (col->type() == ibis::FLOAT)
                    addBounds(rbd*1e-6, rbd, nbins-1, eqw);
                else
                    addBounds(rbd*1e-10, rbd, nbins-1, eqw);
            }
        }
        else if (rbd >= -lbd * DBL_EPSILON) {
            // be default, resolve the double precision floating-point values
            double sml = rbd*DBL_EPSILON;
            uint32_t nbp = static_cast<uint32_t>
                (nbins*log(rbd/sml) / (log(-lbd/sml) + log(rbd/sml)));
            uint32_t nbm = nbins - nbp - 1;
            if (nbm > nbp+1 || (nbm == nbp+1 && -2.0*rbd >= lbd)) {
                sml = -lbd * exp(nbm * log(-rbd/lbd) / (nbm-nbp));
                double tmp = rbd * (col->type() == ibis::FLOAT ?
                                    FLT_EPSILON : DBL_EPSILON);
                if (sml >= 0.1 * rbd) // did not compute an appropriate sml
                    sml = tmp;
                else if (sml < tmp)
                    sml = tmp;
            }
            else if (col->type() == ibis::FLOAT) {
                sml = -lbd * FLT_EPSILON;
            }
            addBounds(lbd, -sml, nbm, eqw);
            bounds.push_back(0.0);
            addBounds(sml, rbd, nbp, eqw);
        }
        else { // the absolute value of rbd is very small
            if (col->type() == ibis::FLOAT)
                addBounds(lbd, lbd*FLT_EPSILON, nbins-1, eqw);
            else
                addBounds(lbd, lbd*DBL_EPSILON, nbins-1, eqw);
            bounds.push_back(0.0);
            bounds.push_back(ibis::util::compactValue(0.2*rbd, 3*rbd));
        }
    }
    else if (eqw == 1) { // equal length subdivisions with rounding
        if (col->type() == ibis::FLOAT ||
            col->type() == ibis::DOUBLE) {
            diff /= nbins;
            const double sf = pow(10.0, 1.0-floor(log10(diff)+0.5));
            diff = floor(0.5 + diff * sf) / sf;
            lbd = floor(lbd/diff) * diff;
            bounds.push_back(lbd);
            const long ib = static_cast<long>(lbd/diff);
            const long ie = static_cast<long>(0.5 + rbd/diff);
            for (long i = ib; i < ie; ++i) {
                bounds.push_back(ibis::util::compactValue
                                 (diff * (i+0.5), diff * (i+1.2)));
            }
        }
        else {
            // make sure bin boundaries are integers
            lbd = floor(lbd);
            bounds.push_back(lbd);
            if (diff < nbins * 3 / 2) {
                for (uint32_t i = 1; i < static_cast<uint32_t>(diff); ++i)
                    bounds.push_back(lbd+i);
            }
            else {
                diff /= nbins;
                for (uint32_t i = 1; i < nbins; ++i) {
                    const double tmp = ibis::util::compactValue
                        (lbd + diff * (i-0.5), lbd + diff * (i+0.2));
                    if (tmp <= rbd)
                        bounds.push_back(tmp);
                    else
                        i = nbins;
                }
            }
            rbd = floor(rbd);
            if (bounds.back() < rbd)
                bounds.push_back(rbd);
        }
    }
    else if (col->type() == ibis::FLOAT ||
             col->type() == ibis::DOUBLE) {
        // simple equal length subdivision without rounding
        bounds.push_back(lbd);
        for (uint32_t i = 1; i <= nbins; ++ i)
            bounds.push_back(((nbins-i)*lbd + i*rbd)/nbins);
    }
    else { // simple equal length subdivision without rounding
        diff = (diff + 1.0) / nbins;
        if (diff < 1.0) diff = 1.0;
        lbd = floor(lbd);
        while (lbd < rbd) {
            bounds.push_back(lbd);
            lbd += diff;
        }
    }
} // ibis::bin::addBounds

/// The optional argument @c nbins can either be set outside or set to
/// be the return value of function parseNbins.
void ibis::bin::scanAndPartition(const char* f, unsigned eqw, uint32_t nbins) {
    if (col == 0) return;

    histogram hist; // a histogram
    if (nbins <= 1)
        nbins = parseNbins(*col);

    mapValues(f, hist, (eqw==10 ? 0 : nbins));
    const uint32_t ncnt = hist.size();
    if (ncnt > nbins * 3 / 2) { // more distinct values than the number of bins
        array_t<double> val(ncnt);
        array_t<uint32_t> cnt(ncnt);
        array_t<uint32_t> bnds(nbins);
        {
            uint32_t i = 0;
            for (histogram::const_iterator it = hist.begin();
                 it != hist.end(); ++ it, ++ i) {
                cnt[i] = (*it).second;
                val[i] = (*it).first;
            }
        }
        hist.clear();
        divideCounts(bnds, cnt);

#if defined(DBUG) && DEBUG+0 > 2
        {
            ibis::util::logger lg(4);
            lg() << "expected bounds  size(" << bounds.size() << ")\n";
            for (array_t<uint32_t>::const_iterator itt = bnds.begin();
                 itt != bnds.end(); ++ itt)
                lg() << val[*itt] << " (" << *itt << ") ";
        }
#endif
        if (bounds.empty()) {
            if (val[0] >= 0)
                bounds.push_back(0.0);
            else
                bounds.push_back(ibis::util::compactValue
                                 (val[0], -DBL_MAX));
        }
        else if (bounds.back() < val[0]) {
            bounds.push_back(ibis::util::compactValue
                             (bounds.back(), val[0]));
        }
        if (col->type() == ibis::FLOAT ||
            col->type() == ibis::DOUBLE) {
#if DEBUG+0 > 0 || _DEBUG+0 > 0
            {
                ibis::util::logger lg(4);
                lg() << "scanAndPartition: raw bin boundaries\n"
                     << bounds.back();
                for (array_t<uint32_t>::const_iterator ii = bnds.begin();
                     ii != bnds.end(); ++ii)
                    if (*ii < val.size())
                        lg() << " " << val[*ii];
            }
#endif
            // for floating-point values, try to round the boundaries
            for (array_t<uint32_t>::const_iterator ii = bnds.begin();
                 ii != bnds.end(); ++ii) {
                double tmp;
                if (*ii == 1) {
                    tmp = ibis::util::compactValue
                        (0.5*(val[0]+val[1]), val[1]);
                }
                else if (*ii < ncnt) {
                    tmp = ibis::util::compactValue(val[*ii-1], val[*ii]);
                }
                else {
                    tmp = col->upperBound();
                    if (val.back() <= tmp) {
                        tmp = ibis::util::compactValue(val.back(), tmp);
                    }
                    else {
                        tmp = ibis::util::compactValue(val.back(), DBL_MAX);
                    }
                }
                bounds.push_back(tmp);
            }
#if DEBUG+0 > 0 || _DEBUG+0 > 0
            {
                ibis::util::logger lg(4);
                lg() << "scanAndPartition: actual bin boundaries\n"
                     << bounds[0];
                for (uint32_t ii = 1; ii < bounds.size(); ++ ii)
                    lg() << " " << bounds[ii];
            }
#endif
        }
        else { // for integer values, may need to narrow the heavy bins
            uint32_t avg = 0; // the average
            for (uint32_t i = 0; i < ncnt; ++ i)
                avg += cnt[i];
            avg /= nbins;
            bool skip = false;
            for (array_t<uint32_t>::const_iterator ii = bnds.begin();
                 ii != bnds.end() && *ii < ncnt; ++ii) {
                if (skip) {
                    skip = false;
                }
                else {
                    bounds.push_back(val[*ii]);
                    if (cnt[*ii] > avg) {
                        const uint32_t next = 1 + *ii;
                        if (next < cnt.size() && val[*ii]+1 < val[next]) {
                            // ensure the bin contain only a single value
                            bounds.push_back(val[*ii]+1);
                            skip = true;
                        }
                    }
                }
            }
        }
    }
    else if (ncnt > 1) { // one bin per value
        uint32_t threshold;
        if (ncnt >= nbins) {
            threshold = col->partition()->nRows();
        }
        else if (ncnt+ncnt <= nbins) { // enough room to double every value
            threshold = 0;
        }
        else { // there is room for additional bin boundaries
            array_t<uint32_t> tmp;
            tmp.reserve(ncnt);
            threshold = 0;
            for (histogram::const_iterator it = hist.begin();
                 it != hist.end(); ++ it) {
                tmp.push_back((*it).second);
                threshold += (*it).second;
            }
            threshold /= ncnt;
            std::sort(tmp.begin(), tmp.end());

            uint32_t j = ncnt + ncnt - nbins;
            while (j < ncnt && (tmp[j] == tmp[j-1] || tmp[j] < threshold))
                ++ j;
            if (j < ncnt)
                threshold = tmp[j];
            else
                threshold = col->partition()->nRows();
        }
        for (histogram::const_iterator it = hist.begin();
             it != hist.end(); ++ it) {
            if ((*it).second < threshold) {
                bounds.push_back((*it).first);
            }
            else {
                bounds.push_back((*it).first);
                bounds.push_back(ibis::util::incrDouble((*it).first));
            }
        }
    }
    else if (ncnt > 0) { // one value only
        histogram::const_iterator it = hist.begin();
        if (fabs((*it).first - 1) < 0.5) {
            bounds.push_back(0.0);
            bounds.push_back(2.0);
        }
        else {
            bounds.push_back(ibis::util::compactValue
                             ((*it).first, -DBL_MAX));
            bounds.push_back(ibis::util::compactValue
                             ((*it).first, DBL_MAX));
        }
    }
#if DEBUG+0 > 2 || _DEBUG+0 > 2
    {
        ibis::util::logger lg(4);
        lg() << "DEBUG - content of bounds in scanAndPartition: size("
             << bounds.size() << ")\n";
        for (array_t<double>::const_iterator it = bounds.begin();
             it != bounds.end(); ++ it)
            lg() << *it << " ";
    }
#endif
} // ibis::bin::scanAndPartition

/// Read a file containing a list of floating-point numbers.
///
/// If nb > 0, read first nb values or till the end of the file.  The file
/// contains one value in each line.  Sine this function only reads the
/// first value, the line may contain other thing after the value.  The
/// sharp '#' symbol is used to indicate comments in the file.
///
/// The file name can use either an absolute path or a relative path
/// (relative to the current data directory of the data partition).  The
/// file name could be specified throught binFile="filename" option of a
/// bin specification.
void ibis::bin::readBinBoundaries(const char *fnm, uint32_t nb) {
    if (fnm == 0 || *fnm == 0) return;

    char buf[MAX_LINE];
    FILE *fptr = fopen(fnm, "r");
    if (fptr == 0) {
        if (col != 0 && col->partition() != 0 &&
            col->partition()->currentDataDir() != 0) {
            std::string fullname = col->partition()->currentDataDir();
            fullname += FASTBIT_DIRSEP;
            fullname += fnm;
            fptr = fopen(fullname.c_str(), "r");
            if (fptr == 0) {
                col->logWarning("bin::readBinBoundaries",
                                "failed to open file \"%s\"",
                                fnm);
                return;
            }
        }
    }

    uint32_t cnt = 0;
    while (fgets(buf, MAX_LINE, fptr)) {
        char *tmp = strchr(buf, '#');
        if (tmp != 0) *tmp = 0;
        double val = strtod(buf, &tmp);
        if (tmp > buf) { // got a double value
            bounds.push_back(val);
            ++ cnt;
            if (nb > 0 && cnt >= nb) break;
        }
    } // while (fgets...
    fclose(fptr);
    LOGGER(ibis::gVerbose > 3)
        << "bin::readBinBoundaries got " << cnt << " value(s) from " << fnm;
} // ibis::bin::readBinBoundaries

/// Parse the index specification to determine the bin boundaries and store
/// the result in member variable bounds.
/// The bin specification can be of the following, where all fields are
/// optional.
/// <ul>
/// <li> \code equal([_-]?)[weight|length|ratio]) \endcode
/// <li> \code no=xx|nbins=xx|bins:(\[begin, end, no=xx\))+ \endcode
/// <li> \code <binning (start=begin end=end nbins=xx scale=[linear|log])* /> \endcode
/// <li> \code <binning binFile=file-name[, nbins=xx] /> \endcode
/// </ul>
///
/// The bin speficication can be read from the column object, the table
/// object containing the column, or the global ibis::gParameters object
/// under the name of @c table-name.column-name.index.  If no index
/// specification is found, this function attempts to generate approximate
/// equal weight bins.
///
/// @note If equal weight is specified, it takes precedence over other
/// specifications.
void ibis::bin::setBoundaries(const char* f) {
    if (col == 0) return;

    uint32_t eqw = parseScale(*col);

    bounds.clear();
    if (eqw >= 10) { // attempt to generate equal-weight bins
        scanAndPartition(f, eqw);
    }
    else { // other type of specifications
        const char* spec = col->indexSpec();
        const char* str = (spec ? strstr(spec, "<binning ") : 0);
        if (str != 0) {
            str += 9;
        }
        else if (spec != 0 && strncmp(spec, "bins:", 5) == 0) {
            str += 5;
        }
        else if (spec != 0 && *spec != 0) {
            str = spec;
        }
        if (str != 0) {
            // new style of bin specification <binning ... />
            // start=xx, end=xx, nbins=xx, scale=linear|log
            // (start, end, nbins, scale)(start, end, nbins, scale)...
            double r0=0.0, r1=0.0, tmp;
            uint32_t progress = 0;
            std::string binfile;
            const char *ptr = 0;
            uint32_t nb = 1;
            //str += 9;
            while (isspace(*str))
                ++ str;
            bool longSpec = (*str == '(');
            if (longSpec)
                ++ str;
            while (str != 0 && *str != 0 && *str != '/' && *str != '>') {
                if (*str == 's' || *str == 'S') { // start=|scale=
                    ptr = str + 5;
                    ptr += strspn(ptr, "= \t");
                    if (*ptr == 'l' || *ptr == 'L') {
                        // assume scale=[linear | log]
                        eqw = 1 + (ptr[1]=='o' || ptr[1]=='O');
                        progress |= 8;
                    }
                    else if (isdigit(*ptr) || *ptr == '.' ||
                             *ptr == '+' || *ptr == '-') {
                        // assume start=...
                        char *ptr2 = 0;
                        tmp = strtod(ptr, &ptr2);
                        if (tmp == 0.0 && ptr == ptr2) {
                            if (r1 > r0)
                                r0 = r1;
                            else
                                r0 = col->lowerBound();
                            LOGGER(ibis::gVerbose > 1)
                                <<"Warning -- bin::setBoundaries encountered a "
                                "bad indexing option \"" << str
                                << "\", assume it to be \"start=" << r0 << "\"";
                        }
                        else {
                            r0 = tmp;
                            str = ptr2;
                        }
                        progress |= 1;
                    }
                    else if (isalpha(*ptr)) {
                        eqw = parseScale(str);
                        progress |= 8;
                    }
                    else { // bad option
                        if (r1 > r0)
                            r0 = r1;
                        else
                            r0 = col->lowerBound();
                        progress |= 1;
                        LOGGER(ibis::gVerbose > 1)
                            <<"Warning -- bin::setBoundaries encountered a "
                            "bad indexing option \"" << str
                            << "\", assume it to be \"start=" << r0 << "\"";
                    }
                }
                else if (*str == 'e' || *str == 'E') { // end=
                    char *ptr2 = 0;
                    ptr = str + 4;
                    r1 = strtod(ptr, &ptr2);
                    if (r1 == 0.0 && ptr == ptr2) {
                        r1 = col->upperBound();
                        LOGGER(ibis::gVerbose > 1)
                            <<"Warning -- bin::setBoundaries encountered a "
                            "bad indexing option \"" << str
                            << "\", assume it to be \"end=" << r1 << "\"";
                    }
                    else {
                        str = ptr2;
                    }
                    progress |= 2;
                }
                else if (*str == 'n' || *str == 'N') { // nbins=
                    ptr = strchr(str, '=');
                    if (ptr)
                        nb = static_cast<uint32_t>(strtod(ptr+1, 0));
                    if (nb == 0)
                        nb = (longSpec ? 1 : IBIS_DEFAULT_NBINS);
                    progress |= 4;
                }
                else if (isdigit(*str) || *str == '.' ||
                         *str == '+' || *str == '-') {
                    // get a number, try start, end, and nbins successively
                    tmp = strtod(str, 0);
                    if ((progress&7) == 0) {
                        r0 = tmp;
                        progress |= 1;
                    }
                    else if ((progress&7) == 1) {
                        r1 = tmp;
                        progress |= 3;
                    }
                    else if ((progress&7) == 3) {
                        nb = static_cast<uint32_t>(tmp);
                        progress |= 7;
                    }
                    else {
                        LOGGER(ibis::gVerbose > 0)
                            << "Warning -- bin::setBoundaries found a "
                            "labeled element of bin spec before the "
                            "unlabeled ones -- skipping value " << tmp;
                    }
                }
                else if (*str == 'l' || *str == 'L') {
                    eqw = 1 + (str[1]=='o' || str[1]=='O');
                    progress |= 8;
                }
                else if (strnicmp(str, "binFile=", 8) == 0 ||
                         strnicmp(str, "file=", 5) == 0) {
                    str += (*str=='b'?8:5);
                    int ierr =
                        ibis::util::readString(binfile, str, ",; \t()/>");
                    if (ierr >= 0 && ! binfile.empty())
                        progress |= 11;
                }
                str = strpbrk(str, ",; \t()/>");
                if (str != 0 && *str != 0)
                    str += strspn(str, ",; \t"); // skip space
                bool add = (progress == 15);
                if (! add) {
                    if (str == 0) { // end of string
                        add = 1;
                    }
                    else if (*str == '/' || *str == '>') {
                        add = 1;
                    }
                    else if (*str == ')' || *str == '(') {
                        if ((progress&3) == 3)
                            add = 1;
                    }
                }
                if (add) {
                    if (binfile.empty()) {
                        if ((progress & 1) == 0)
                            r0 = col->lowerBound();
                        if ((progress & 2) == 0)
                            r1 = col->upperBound();
                        if ((progress & 4) == 0)
                            nb = (longSpec ? 1 : IBIS_DEFAULT_NBINS);
                        addBounds(r0, r1, nb, eqw);
                    }
                    else { // attempt to read the named file
                        if ((progress & 4) == 0)
                            readBinBoundaries(binfile.c_str(), 0);
                        else
                            readBinBoundaries(binfile.c_str(), nb);
                        binfile.erase();
                    }
                    progress = 0;
                }
                if (str != 0) { // skip end markers
                    if (*str == ')' || *str == '(') {
                        str += strspn(str, ",; \t)(");
                    }
                }
            }
        }
        else if (spec != 0 && (str = strstr(spec, "bins:")) != 0) {
            // use explicitly specified bin boundaries
            double r0, r1;
            const char* ptr = strchr(str, '[');
            while (ptr) {
                ++ ptr;
                while (isspace(*ptr)) // skip leading space
                    ++ ptr;
                str = strpbrk(ptr, ",; \t)");
                if (ptr == str) {
                    r0 = col->lowerBound();
                }
                else {
                    r0 = strtod(ptr, 0);
                }
                if (str != 0) {
                    ptr = str + (*str != ')');
                    while (isspace(*ptr))
                        ++ ptr;
                    str = strpbrk(ptr, ",; \t)");
                    if (ptr == str) {
                        r1 = col->upperBound();
                    }
                    else {
                        r1 = strtod(ptr, 0);
                    }
                }
                else {
                    r1 = col->upperBound();
                }
                uint32_t nb = 1;
                ptr = str + (*str != ')');
                while (isspace(*ptr))
                    ++ ptr;
                if (*ptr == 'n') {
                    ptr += strspn(ptr, "nobins= \t");
                    nb = static_cast<uint32_t>(strtod(ptr, 0));
                }
                else if (isdigit(*ptr) || *ptr == '.') {
                    nb = static_cast<uint32_t>(strtod(ptr, 0));
                }
                if (nb == 0)
                    nb = 1;

                // the bulk of the work is done here
                addBounds(r0, r1, nb, eqw);

                // ready for the next group
                ptr = strchr(ptr, '[');
            }
            bounds.push_back(ibis::util::compactValue(bounds.back(),
                                                      DBL_MAX));
        }
        else if (spec != 0) {
            col->logWarning("bin::binning", "expect bin spec to start with "
                            "<binning or bins: but found none \"%s\"", spec);
        }
        else {
            col->logWarning("bin::binning", "do not know how to bin");
        }
    }

    if (bounds.empty()) {
        if (eqw < 10) { // default bin spec, try approximate equal depth bin
            eqw = 11;
            scanAndPartition(f, eqw);
        }
        else if (col->lowerBound() >= col->upperBound()) {
            // has not set any thing yet
            double aver = 0.5 * (col->lowerBound() + col->upperBound());
            double diff = 0.5 * (col->lowerBound() - col->upperBound());
            if (! (fabs(diff) > 0.0) ||
                ! (fabs(diff) > DBL_EPSILON * aver))
                diff = 1.0;
            bounds.push_back(aver-diff);
            bounds.push_back(aver+diff);
        }
    }

    if (bounds.size() > 1) {
        // ensure bounds are sorted and are unique
        std::sort(bounds.begin(), bounds.end());
        const uint32_t nb1 = bounds.size();
#if DEBUG+0 > 0 || _DEBUG+0 > 0
        {
            ibis::util::logger lg(4);
            lg() << "DEBUG -- bin bounds before duplicate removal\n";
            for (unsigned i = 0; i < nb1; ++ i)
                lg() << bounds[i] << " ";
        }
#endif
        if (col->type() == ibis::DOUBLE || col->type() == ibis::FLOAT) {
            unsigned i, j;
            for (i = 0, j = 1; j < nb1; ++ j) {
                if (bounds[j] > bounds[i]) {
                    if (j > i+1)
                        bounds[i+1] = bounds[j];
                    ++ i;
                }
                else if (ibis::gVerbose > 6) {
                    col->logMessage("setBoundaries", "skipping bounds[%u]"
                                    "(%g) because it is too close to bounds"
                                    "[%u](%g) (diff=%g)", j, bounds[j],
                                    i, bounds[i], bounds[j]-bounds[i]);
                }
            }
            bounds.resize(i+1);
        }
        else {
            unsigned i, j;
            bounds[0] = static_cast<long int>(bounds[0]);
            for (i = 0, j = 1; j < nb1; ++ j) {
                bounds[j] = static_cast<long int>(bounds[j]);
                if (bounds[j] > bounds[i]) {
                    if (j > i+1)
                        bounds[i+1] = bounds[j];
                    ++ i;
                }
                else if (ibis::gVerbose > 6) {
                    col->logMessage("setBoundaries", "skipping bounds[%u]"
                                    "(%g) because it is too close to bounds"
                                    "[%u](%g) (diff=%g)", j, bounds[j],
                                    i, bounds[i], bounds[j]-bounds[i]);
                }
            }
            bounds.resize(i+1);
        }
#if DEBUG+0 > 0 || _DEBUG+0 > 0
        {
            ibis::util::logger lg(4);
            lg() << "DEBUG -- bin bounds after duplicate removal\n";
            for (unsigned i = 0; i < bounds.size(); ++ i)
                lg() << std::setprecision(16) << bounds[i] << " ";
        }
#endif
    }

    if (! bounds.empty()) {
        if (bounds.back() <= col->upperBound())
            bounds.back() = ibis::util::compactValue(bounds.back(), DBL_MAX);
        // if (col->type() == ibis::FLOAT) {
        //     // adjust the precision of boundaries to match the precision of
        //     // the attribute
        //     nobs = bounds.size();
        //     for (uint32_t i = 0; i < nobs; ++i)
        //      if (bounds[i] < FLT_MAX && bounds[i] > -FLT_MAX)
        //          bounds[i] = static_cast<double>
        //              (static_cast<float>(bounds[i]));
        // }

        // add DBL_MAX as the last value
        bounds.push_back(DBL_MAX);
    }

    nobs = bounds.size();
    if (ibis::gVerbose > 5) {
        std::ostringstream ostr;
        ostr << bounds[0];
        uint32_t nprt = (ibis::gVerbose > 30 ? nobs : (1 << ibis::gVerbose));
        if (nprt > nobs)
            nprt = nobs;
        for (uint32_t i = 1; i < nprt; ++ i)
            ostr << ", " << bounds[i];
        if (nprt < nobs)
            ostr << ", ... (" << nobs-nprt << " omitted)";
        col->logMessage("bin::setBoundaries", "bounds[%lu]={%s}",
                        static_cast<long unsigned>(nobs), ostr.str().c_str());
    }
} // ibis::bin::setBoundaries

// based on the current weights and the weights from another ibis::bin to
// decide the new bin boundaries.  The new bounaries are stored in bnds
void ibis::bin::setBoundaries(array_t<double>& bnds,
                              const ibis::bin& idx1,
                              const array_t<uint32_t> cnt1,
                              const array_t<uint32_t> cnt0) const {
    if (col == 0) return;
    uint32_t numbs = cnt1.size();
    bnds.clear();
    bnds.reserve(numbs);
    uint32_t i, res, weight = 0;
    for (i = 0; i < numbs; ++i) // count the total number of objects
        weight += cnt0[i] + cnt1[i];
    if (weight == 0) {
        if (ibis::gVerbose > 0)
            col->logMessage("bin::setBoundaries", "both cnt0[%lu] and "
                            "cnt1[%lu] contains only zero",
                            static_cast<long unsigned>(cnt0.size()),
                            static_cast<long unsigned>(cnt1.size()));
        bnds.copy(bounds);      // copy the current bounds
        return;
    }
    res = weight;
    if (numbs > 2)
        weight /= numbs - 2;
    else
        weight >>= 1; // two regular bins plus two side bins

    uint32_t cnt=0;
    i = 0;
    while (i < numbs && cnt0[i] + cnt1[i] == 0) // skip the empty bins
        ++i;
    if (i == 0)
        bnds.push_back(minval[0]<=idx1.minval[0] ? minval[0] :
                       idx1.minval[0]);
    else
        bnds.push_back(bounds[i-1]);

    while (i < numbs) { // the outer loop
        uint32_t tot=0;
        // accumulate enough events
        while (i < numbs) {
            tot = cnt0[i]+cnt1[i];
            res -= tot;
            if (cnt+tot < weight) {
                ++ i;
                cnt += tot;
            }
            else
                break;
        }
        if (i < numbs) { // normal cases
            if (cnt+tot == weight) { // exactly weight, take it
                bnds.push_back(bnds[i]);
                ++i;
                cnt = 0;
            }
            else if (tot>weight && minval[i]==maxval[i] &&
                     minval[i] == idx1.minval[i] &&
                     minval[i] == idx1.maxval[i]) { // isolated value
                if (3*cnt < weight) {
                    bnds.back() = bounds[i-1]; // extend preceding bin
                    bnds.push_back(bounds[i]);
                    ++i;
                }
                else {
                    bnds.push_back(bounds[i-1]);
                    bnds.push_back(bounds[i]);
                    ++i;
                }
                weight = (numbs > bnds.size() ? res / (numbs - bnds.size()) :
                          res);
                cnt = 0;
            }
            else if (minval[i] <= idx1.minval[i]) {
                if (maxval[i] > minval[i]) {
                    uint32_t seg1 = static_cast<uint32_t>
                        ((idx1.minval[i] - minval[i]) * cnt0[i] /
                         (maxval[i] - minval[i]));
                    if (cnt+seg1 >= weight) {
                        bnds.push_back(minval[i] + (weight-cnt) *
                                       (maxval[i] - minval[i]) / cnt0[i]);
                        cnt = tot - (weight - cnt);
                    }
                    else if (!(idx1.maxval[i] > idx1.minval[i])) {
                        double s1 = ibis::util::incrDouble(idx1.maxval[i]);
                        bnds.push_back(s1);
                        cnt = static_cast<uint32_t>
                            (cnt0[i] * (maxval[i] - s1) / (maxval[i] -
                                                           minval[i]));
                    }
                    else {
                        uint32_t seg2 = static_cast<uint32_t>
                            (((maxval[i] <= idx1.maxval[i] ? maxval[i] :
                               idx1.maxval[i]) - idx1.minval[i]) *
                             (cnt0[i] / (maxval[i] - minval[i]) +
                              cnt1[i] / (idx1.maxval[i] - idx1.minval[i])));
                        if (cnt+seg1+seg2 >= weight) {
                            bnds.push_back(idx1.minval[i] +
                                           (weight-seg1-cnt) /
                                           (cnt0[i] / (maxval[i] -
                                                       minval[i]) +
                                            cnt1[i] / (idx1.maxval[i] -
                                                       idx1.minval[i])));
                            cnt = tot - (weight - cnt);
                        }
                        else if (maxval[i] <= idx1.maxval[i]) {
                            if (weight > cnt + cnt0[i]) {
                                bnds.push_back(maxval[i] + (weight - cnt -
                                                            cnt0[i]) *
                                               (idx1.maxval[i] -
                                                idx1.minval[i]) / cnt1[i]);
                                cnt = tot - (weight - cnt);
                            }
                            else {
                                bnds.push_back(maxval[i]);
                                cnt = static_cast<uint32_t>
                                    ((idx1.maxval[i] - maxval[i]) * cnt1[i] /
                                     (idx1.maxval[i] - idx1.minval[i]));
                            }
                        }
                        else {
                            if (weight > cnt + cnt0[i]) {
                                bnds.push_back(idx1.maxval[i] +
                                               (weight - cnt - cnt1[i]) *
                                               (maxval[i] - minval[i]) /
                                               cnt0[i]);
                                cnt = tot - (weight - cnt);
                            }
                            else {
                                bnds.push_back(idx1.maxval[i]);
                                cnt = static_cast<uint32_t>
                                    ((maxval[i] - idx1.maxval[i]) * cnt0[i] /
                                     (maxval[i] - minval[i]));
                            }
                        }
                    }
                }
                else { // maxval[i] == minval[i]
                    double s1 = ibis::util::incrDouble(maxval[i]);
                    bnds.push_back(s1);
                    cnt = static_cast<uint32_t>
                        (cnt1[i] * (idx1.maxval[i] - s1) /
                         (idx1.maxval[i] - idx1.minval[i]));
                }
                ++ i;
            }
            else if (idx1.maxval[i] > idx1.minval[i]) {
                uint32_t seg1 = static_cast<uint32_t>
                    ((minval[i] - idx1.minval[i]) * cnt1[i] /
                     (idx1.maxval[i] - idx1.minval[i]));
                if (cnt+seg1 >= weight) {
                    bnds.push_back(idx1.minval[i] + (weight-cnt) *
                                   (idx1.maxval[i] - idx1.minval[i]) /
                                   cnt1[i]);
                    cnt = tot - (weight - cnt);
                }
                else if (! (maxval[i] > minval[i])) {
                    double s1 = ibis::util::incrDouble(maxval[i]);
                    bnds.push_back(s1);
                    cnt = static_cast<uint32_t>
                        (cnt1[i] * (idx1.maxval[i] - s1) / 
                         (idx1.maxval[i] - idx1.minval[i]));
                }
                else {
                    uint32_t seg2 = static_cast<uint32_t>
                        (((maxval[i] <= idx1.maxval[i] ?
                           maxval[i] : idx1.maxval[i]) - minval[i]) *
                         (cnt0[i] / (maxval[i] - minval[i]) +
                          cnt1[i] / (idx1.maxval[i] - idx1.minval[i])));
                    if (cnt+seg1+seg2 >= weight) {
                        bnds.push_back(minval[i] +
                                       (weight-seg1-cnt) /
                                       (cnt0[i] / (maxval[i] - minval[i]) +
                                        cnt1[i] / (idx1.maxval[i] -
                                                   idx1.minval[i])));
                        cnt = tot - (weight - cnt);
                    }
                    else if (maxval[i] <= idx1.maxval[i]) {
                        if (weight > cnt + cnt0[i]) {
                            bnds.push_back(maxval[i] + (weight - cnt -
                                                        cnt0[i]) *
                                           (idx1.maxval[i] -
                                            idx1.minval[i]) / cnt1[i]);
                            cnt = tot - (weight - cnt);
                        }
                        else {
                            bnds.push_back(maxval[i]);
                            cnt = static_cast<uint32_t>
                                ((idx1.maxval[i] - maxval[i]) * cnt1[i] /
                                 (idx1.maxval[i] - idx1.minval[i]));
                        }
                    }
                    else {
                        if (weight > cnt + cnt0[i]) {
                            bnds.push_back(idx1.maxval[i] +
                                           (weight - cnt - cnt1[i]) *
                                           (maxval[i] - minval[i]) /
                                           cnt0[i]);
                            cnt = tot - (weight - cnt);
                        }
                        else {
                            bnds.push_back(idx1.maxval[i]);
                            cnt = static_cast<uint32_t>
                                ((maxval[i] - idx1.maxval[i]) * cnt0[i] /
                                 (maxval[i] - minval[i]));
                        }
                    }
                }
                ++ i;
            }
            else {
                double s1 = ibis::util::incrDouble(idx1.maxval[i]);
                bnds.push_back(s1);
                cnt = static_cast<uint32_t>
                    (cnt0[i] * (maxval[i] - s1) / (maxval[i] - minval[i]));
            }
        } // if (i < numbs)
        else {
            i = numbs - 1;
            while (cnt0[i]==0 && cnt1[i]==0)
                -- i;
            double amax = (maxval[i] >= idx1.maxval[i] ?
                           maxval[i] : idx1.maxval[i]);
            bnds.push_back(ibis::util::incrDouble(amax));
            bnds.push_back(DBL_MAX);
            if (bnds.size() != numbs && ibis::gVerbose > 1) {
                // not the same number of bins
                col->logMessage("bin::setBoundaries", "combined two sets "
                                "of %lu bins into %lu bins",
                                static_cast<long unsigned>(numbs),
                                static_cast<long unsigned>(bnds.size()));
            }
            i = numbs;
        }
    }
}  // ibis::bin::setBoundaries

// Using the current weight of the bins to decide new bin boundaries
// *** This version is safe only if bin0 has exactly the same bin
// *** boundaries as this.
void ibis::bin::setBoundaries(array_t<double>& bnds,
                              const ibis::bin& bin0) const {
    if (col == 0) return;

    uint32_t i;
    bnds.resize(nobs);
    uint32_t eqw = parseScale(*col);
    const uint32_t weight =
        (bits[0]->size()+bin0.bits[0]->size())/(nobs>2?nobs-2:1);

    if (eqw > 0) { // attempt to generate equal-weight bins
        uint32_t j=0, cnt=0;
        double frac;
        i = 0;
        while (i < nobs && bits[i]->cnt() + bin0.bits[i]->cnt() == 0)
            ++i;
        bnds[0] = (minval[i]<=bin0.minval[i]?minval[i]:bin0.minval[i]);
        if (bnds[0] > bounds[0])
            bnds[0] = bounds[0];
        while (i < nobs) { // the outer loop
            uint32_t tot=0;
            // accumulate enough events
            while (i < nobs) {
                if (bits[i] != 0)
                    tot += bits[i]->cnt();
                else
                    tot = 0;
                if (i < bin0.nobs && bin0.bits[i] != 0)
                    tot += bin0.bits[i]->cnt();
                if (cnt+tot < weight) {
                    ++ i;
                    cnt += tot;
                }
                else
                    break;
            }
            if (i < nobs) {
                if (cnt+tot == weight) {
                    bnds[j] = bnds[i];
                    ++i;
                    ++j;
                    cnt = 0;
                }
                else if (tot>weight && minval[i]==maxval[i] &&
                         minval[i] == bin0.minval[i] &&
                         minval[i] == bin0.maxval[i]) { // isolated value
                    if (j == 0) {
                        if  (cnt > 0) {
                            bnds[0] = bounds[i-1];
                            bnds[1] = bounds[i];
                            j = 2;
                        }
                        else {
                            bnds[0] = bounds[i];
                            j = 1;
                        }
                        ++i;
                    }
                    else if (cnt == 0) {
                        bnds[j] = bounds[i];
                        ++i;
                        ++j;
                    }
                    else if (cnt+cnt < weight) {
                        bnds[j-1] = bounds[i-1];
                        bnds[j] = bounds[i];
                        ++i;
                        ++j;
                    }
                    else {
                        bnds[j] = bounds[i-1];
                        bnds[++j] = bounds[i];
                        ++i;
                        ++j;
                    }
                    cnt = 0;
                }
                else {
                    const double amin = (minval[i]<=bin0.minval[i]?
                                         minval[i] : bin0.minval[i]);
                    const double amax = (maxval[i]>=bin0.maxval[i]?
                                         maxval[i] : bin0.maxval[i]);
                    if (amax == amin) {
                        bnds[j] = bounds[i];
                        ++i;
                        ++j;
                        cnt = 0;
                    }
                    else {
                        uint32_t used = weight - cnt;
                        frac = used / tot;
                        bnds[j] = amin + (amax - amin) * frac;
                        ++j;
                        while (tot > used+weight) {
                            used += weight;
                            bnds[j] = amin + (amax - amin) * used / tot;
                            ++j;
                        }
                        if (tot == used+weight) {
                            cnt = 0;
                            bnds[j] = bounds[i];
                            ++j;
                        }
                        else {
                            cnt = tot - used;
                        }
                        ++i;
                    }
                }
            } // if (i < nobs)
            else {
                i = nobs - 1;
                while (bits[i]->cnt()==0 && bin0.bits[i]->cnt()==0)
                    -- i;
                const double amin = (minval[i]<=bin0.minval[i]?
                                     minval[i] : bin0.minval[i]);
                const double amax = (maxval[i]>=bin0.maxval[i]?
                                     maxval[i] : bin0.maxval[i]);
                if (j < nobs-1) { // have specified too few bins
                    col->logMessage("bin::setBoundaries", "last %lu bins "
                                    "are likely to be underweighted",
                                    static_cast<long unsigned>(nobs-j));
                    if (bnds[j-1] < amax) {
                        const double amin1 =
                            (amin>bnds[j-1]?amin:bnds[j-1]);
                        for (uint32_t k=j; k<nobs-1; ++k) {
                            bnds[k] = amin1 + (amax - amin1) * (k-j+1) /
                                (nobs-j);
                        }
                        bnds.back() = DBL_MAX;
                    }
                    else {
                        for (; j<nobs; ++j)
                            bnds[j] = DBL_MAX;
                    }
                }
                else {
                    bnds.back() = DBL_MAX;
                }
                i = nobs;
            }
        }
    }
    else { // simple equal size bins
        // record the minimimum value
        for (i = 0; i<nobs && bits[i]->cnt()==0; ++ i);
        if (i < nobs)
            bnds[0] = minval[i];
        else
            bnds[0] = DBL_MAX;
        for (i = 0; i<nobs && bin0.bits[i]->cnt()==0; ++ i);
        if (i < nobs)
            bnds[0] = (bnds[0]>bin0.minval[i]?bin0.minval[i]:bnds[0]);
        // record the maximum value
        for (i = nobs-1; i>0 && bits[i]->cnt()==0; -- i);
        if (i > 0)
            bnds.back() = maxval[i];
        else if (bits[0]->cnt() > 0)
            bnds.back() = maxval[0];
        else
            bnds.back() = -DBL_MAX;
        for (i = nobs-1;
             i>0 && (bin0.bits[i]==0 || bin0.bits[i]->cnt()==0);
             -- i);
        if (i > 0 && bnds.back() < bin0.maxval[i])
            bnds.back() = bin0.maxval[i];
        else if (i == 0 && bnds.back() < bin0.maxval[0])
            bnds.back() = bin0.maxval[0];
        bnds.back() *= 1.0 + DBL_EPSILON;

        // generate all the points in-between
        double diff = (bnds.back()-bnds[0])/(nobs>1?nobs-1:1);
        double sf = pow(10.0, 1.0-floor(log10(diff)+0.5));
        diff = floor(0.5 + diff*sf) / sf;
        const long ib = static_cast<long>(bnds[0]/diff);
        for (i=0; i<nobs; ++i)
            bnds[i] = ibis::util::compactValue(diff*(i+ib-0.3),
                                               diff*(i+ib+0.2));
    }
} // ibis::bin::setBoundaries

// This function overwrites the content of the array parts, but will make
// use of its size to determine the number of groups to divide the bitmaps.
// In keeping with the practice of leaving two extreme bins, the minimal
// number of bins useful is assumed to be 4.  If the size of array parts is
// less than 4, it will default to 15.
void ibis::bin::divideBitmaps(const array_t<bitvector*>& bms,
                              std::vector<unsigned>& parts) const {
    const uint32_t nbms = bms.size();
    unsigned nparts = (parts.size() < 4 ? 15 : parts.size());
    if (nparts > nbms)
        nparts = nbms;
    parts.resize(nparts);
    if (nparts < nbms) { // normal case
        uint32_t i, tot = 0;
        std::vector<uint32_t> tmp(nbms);
        for (i = 0; i < nbms; ++ i) {
            tot += bms[i]->bytes();
            tmp[i] = tot;
        }
        i = 1;
        parts[0] = 1;
        tot = tmp[nbms-2];
        while (i < nparts-2 && nparts-i < nbms-parts[i-1]) {
#if DEBUG+0 > 0 || _DEBUG+0 > 0
            LOGGER(ibis::gVerbose >= 0)
                << "divideBitmaps -- i=" << i << ", parts["
                << i-1 << "]=" << parts[i-1] << ", nparts-i-1="
                << nparts - i - 1 << ", tot-tmp[" << parts[i-1]-1
                << "]=" << tot-tmp[parts[i-1]-1];
#endif
            uint32_t target = tmp[parts[i-1]-1] + (tot - tmp[parts[i-1]-1]) /
                (nparts - i);
            uint32_t j = parts[i-1] + 1;  // move boundary forward by 1
            while (j+nparts-i-1 < nbms && tmp[j] < target)
                ++ j;
            if (j==parts[i-1]+1 || tmp[j]-target <= target-tmp[j-1]) {
                parts[i] = j;
            }
            else {
                parts[i] = j-1;
                j = j-1;
            }
            ++ i;
        }
        while (i < nparts) {
            parts[i] = nbms - nparts + i;
            ++ i;
        }
    }
    else { // every bitmap into its own group
        for (unsigned i = 0; i < nparts; ++i)
            parts[i] = i;
    }
    if (ibis::gVerbose > 5) {
        ibis::util::logger lg;
        lg() << "divideBitmaps -- divided " << nbms << " bitmaps into "
             << nparts << " groups\n";
        for (unsigned i = 0; i < nparts; ++ i)
            lg() << parts[i] << " ";
    }
} // ibis::bin::divideBitmaps

/// Write the index to the named directory or file.
int ibis::bin::write(const char* dt) const {
    if (nobs <= 0 || nrows <= 0) return -1;
    std::string fnm, evt;
    evt = "bin";
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

    try {
        if (str != 0 || fname != 0)
            activate();
    }
    catch (const std::exception& e) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt
            << " received a std::exception - " << e.what();
        return -2;
    }
    catch (const char* s) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt
            << " received a string exception - " << s;
        return -3;
    }
    catch (...) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt
            << " received a unknown exception";
        return -4;
    }

#ifdef FASTBIT_USE_LONG_OFFSETS
    const bool useoffset64 = true;
#else
    const bool useoffset64 = (8+getSerialSize() >= 0x80000000UL);
#endif
    int fdes = UnixOpen(fnm.c_str(), OPEN_WRITENEW, OPEN_FILEMODE);
    if (fdes < 0) {
        ibis::fileManager::instance().flushFile(fnm.c_str());
        fdes = UnixOpen(fnm.c_str(), OPEN_WRITENEW, OPEN_FILEMODE);
        if (fdes < 0) {
            const char* mesg;
            if (errno != 0)
                mesg = strerror(errno);
            else
                mesg = "no free stdio stream";
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << evt << " failed to open \"" << fnm
                << "\" for write ... " << mesg;
            return -5;
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

    char header[] = "#IBIS\0\0\0";
    header[5] = (char)ibis::index::BINNING;
    header[6] = (char)(useoffset64 ? 8 : 4);
    off_t ierr = UnixWrite(fdes, header, 8);
    if (ierr < 8) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt
            << " failed to write the 8-byte header, ierr = " << ierr;
        return -6;
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
            << evt << " wrote " << nobs << " bitmap" << (nobs>1?"s":"")
            << " to file " << fnm << " for " << nrows << " object"
            << (nrows>1?"s":"") << ", file size "
            << (useoffset64 ? offset64.back() : (int64_t)offset32.back());
    }
    return 0;
} // ibis::bin::write

/// Write the content to a file already open.
int ibis::bin::write32(int fdes) const {
    if (nobs <= 0) return -1;
    std::string evt = "bin";
    if (col != 0 && ibis::gVerbose > 1) {
        evt += '[';
        evt += col->fullname();
        evt += ']';
    }
    evt += "::write32";
    try {
        if (str != 0 || fname != 0)
            activate();
    }
    catch (const std::exception& e) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " received a std::exception - "
            << e.what();
        return -2;
    }
    catch (const char* s) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " received a string exception - " << s;
        return -3;
    }
    catch (...) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " received a unexpected exception";
        return -4;
    }

    const int32_t start = UnixSeek(fdes, 0, SEEK_CUR);
    if (start < 8) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << "(" << fdes
            << ") can not start at position " << start;
        return -7;
    }

    off_t ierr = UnixWrite(fdes, &nrows, sizeof(nrows));
    ierr += UnixWrite(fdes, &nobs, sizeof(nobs));
    if (ierr < (int)sizeof(nrows)*2) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << "(" << fdes
            << ") failed to write nrows (" << nrows << ") or nobs ("
            << nobs << "), ierr = " << ierr;
        return -8;
    }
    offset64.clear();
    offset32.resize(nobs+1);
    offset32[0] = ((start+sizeof(int32_t)*(nobs+1)+2*sizeof(uint32_t)+7)/8)*8;
    ierr  = UnixSeek(fdes, offset32[0], SEEK_SET);
    ierr += ibis::util::write(fdes, bounds.begin(), sizeof(double)*nobs);
    ierr += ibis::util::write(fdes, maxval.begin(), sizeof(double)*nobs);
    ierr += ibis::util::write(fdes, minval.begin(), sizeof(double)*nobs);
    offset32[0] += sizeof(double)*nobs*3;
    if (ierr < offset32[0]) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << "(" << fdes
            << ") expected to write the 1st bitmap at offset " << offset32[0]
            << ", but the current file position is " << ierr;
        (void) UnixSeek(fdes, start, SEEK_SET);
        return -9;
    }
    for (uint32_t i = 0; i < nobs; ++i) {
        if (bits[i] != 0)
            bits[i]->write(fdes);
        offset32[i+1] = UnixSeek(fdes, 0, SEEK_CUR);
    }
    ierr = UnixSeek(fdes, start+2*sizeof(uint32_t), SEEK_SET);
    if (ierr != (off_t) (start+2*sizeof(uint32_t))) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << "(" << fdes << ") failed to seek to "
            << start+2*sizeof(uint32_t) << ", ierr = " << ierr;
        (void) UnixSeek(fdes, start, SEEK_SET);
        return -10;
    }
    ierr = ibis::util::write(fdes, offset32.begin(), sizeof(int32_t)*(nobs+1));
    if (ierr < (off_t)(sizeof(int32_t)*(nobs+1))) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << "(" << fdes
            << ") failed to write " << nobs+1 << " bitmap positions"
            << " to file descriptor " << fdes << ", ierr = " << ierr;
        (void) UnixSeek(fdes, start, SEEK_SET);
        return -11;
    }
    ierr = UnixSeek(fdes, offset32[nobs], SEEK_SET); // move to the end
    return (ierr == offset32[nobs] ? 0 : -18);
} // ibis::bin::write32

/// write the content to a file already open.
int ibis::bin::write64(int fdes) const {
    if (nobs <= 0) return -1;
    std::string evt = "bin";
    if (col != 0 && ibis::gVerbose > 1) {
        evt += '[';
        evt += col->fullname();
        evt += ']';
    }
    evt += "::write64";
    try {
        if (str != 0 || fname != 0)
            activate();
    }
    catch (const std::exception& e) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " received a std::exception - "
            << e.what();
        return -2;
    }
    catch (const char* s) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " received a string exception - " << s;
        return -3;
    }
    catch (...) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " received a unexpected exception";
        return -4;
    }

    const int32_t start = UnixSeek(fdes, 0, SEEK_CUR);
    if (start < 8) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << "(" << fdes
            << ") can not start at position " << start;
        return -12;
    }

    off_t ierr = UnixWrite(fdes, &nrows, sizeof(nrows));
    ierr += UnixWrite(fdes, &nobs, sizeof(nobs));
    if (ierr < (int)(sizeof(nrows)*2)) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << "(" << fdes
            << ") failed to write nrows (" << nrows << ") or nobs ("
            << nobs << "), ierr = " << ierr;
        return -13;
    }
    offset32.clear();
    offset64.resize(nobs+1);
    offset64[0] = ((start+sizeof(int64_t)*(nobs+1)+2*sizeof(uint32_t)+7)/8)*8;
    ierr = UnixSeek(fdes, offset64[0], SEEK_SET);
    ierr += ibis::util::write(fdes, bounds.begin(), sizeof(double)*nobs);
    ierr += ibis::util::write(fdes, maxval.begin(), sizeof(double)*nobs);
    ierr += ibis::util::write(fdes, minval.begin(), sizeof(double)*nobs);
    offset64[0] += sizeof(double)*nobs*3;
    if (ierr != offset64[0]) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << "(" << fdes
            << ") expected the 1st bitmap to start at " << offset64[0]
            << ", but the current file position is " << ierr;
        (void) UnixSeek(fdes, start, SEEK_SET);
        return -14;
    }
    for (uint32_t i = 0; i < nobs; ++i) {
        if (bits[i] != 0)
            bits[i]->write(fdes);
        offset64[i+1] = UnixSeek(fdes, 0, SEEK_CUR);
    }
    ierr = UnixSeek(fdes, start+2*sizeof(uint32_t), SEEK_SET);
    if (ierr != (off_t) (start+2*sizeof(uint32_t))) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << "(" << fdes
            << ") failed to seek to " << start+2*sizeof(uint32_t)
            << ", ierr = " << ierr;
        (void) UnixSeek(fdes, start, SEEK_SET);
        return -15;
    }
    ierr = ibis::util::write(fdes, offset64.begin(), sizeof(int64_t)*(nobs+1));
    if (ierr < (off_t)(sizeof(int64_t)*(nobs+1))) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << "(" << fdes
            << ") failed to write " << nobs+1 << " bitmap positions"
            << " to file descriptor " << fdes << ", ierr = " << ierr;
        (void) UnixSeek(fdes, start, SEEK_SET);
        return -16;
    }
    ierr = UnixSeek(fdes, offset64[nobs], SEEK_SET); // move to the end
    return (ierr == offset64[nobs] ? 0 : -17);
} // ibis::bin::write64

int ibis::bin::write(ibis::array_t<double> &keys,
                     ibis::array_t<int64_t> &starts,
                     ibis::array_t<uint32_t> &bitmaps) const {
    keys.resize(0);
    if (nobs == 0) {
        starts.resize(0);
        bitmaps.resize(0);
        return 0;
    }

    keys.reserve(nobs+nobs);
    keys.copy(minval);
    keys.insert(keys.end(), maxval.begin(), maxval.end());
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
} // ibis::bin::write

void ibis::bin::serialSizes(uint64_t &wkeys, uint64_t &woffsets,
                            uint64_t &wbitmaps) const {
    if (nobs == 0) {
        wkeys = 0;
        woffsets = 0;
        wbitmaps = 0;
    }
    else {
        wkeys = nobs + nobs;
        woffsets = nobs + 1;
        wbitmaps = 0;
        for (unsigned j = 0; j < nobs; ++ j) {
            if (bits[j] != 0)
                wbitmaps += bits[j]->getSerialSize();
        }
        wbitmaps /= 4;
    }
} // ibis::bin::serialSizes

void ibis::bin::clear() {
    bounds.clear();
    minval.clear();
    maxval.clear();
    nobs = 0;
    ibis::index::clear();
} // ibis::bin::clear

void ibis::bin::binBoundaries(std::vector<double>& ret) const {
    ret.reserve(nobs + 1);
    for (uint32_t i = 0; i < nobs; ++ i)
        ret.push_back(bounds[i]);
} // ibis::bin::binBoundaries

void ibis::bin::binWeights(std::vector<uint32_t>& ret) const {
    try {
        activate(); // make sure all bitvectors are available
    }
    catch (const std::exception& e) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- bin::binWeights received a std::exception - "
            << e.what();
        return;
    }
    catch (const char* s) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- bin::binWeights received a string exception - " << s;
        return;
    }
    catch (...) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- bin::binWeights received a unknown exception";
        return;
    }

    ret.reserve(nobs + 1);
    for (uint32_t i = 0; i < nobs; ++ i)
        ret.push_back(bits[i] != 0 ? bits[i]->cnt() : 0);
} // ibis::bin::binWeights

// a simple function to test the speed of the bitvector operations
void ibis::bin::speedTest(std::ostream& out) const {
    if (nrows == 0) return;
    uint32_t nloops = 1000000000 / nrows;
    if (nloops < 2) nloops = 2;
    ibis::horometer timer;

    try {
        activate(); // make sure all bitvectors are available
    }
    catch (const std::exception& e) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- bin::speedTest received a standard exception - "
            << e.what();
        return;
    }
    catch (const char* s) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- bin::speedTest received a string exception - " << s;
        return;
    }
    catch (...) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- bin::speedTest received a unknown exception";
        return;
    }
    bool crossproduct = false;
    {
        std::string which;
        if (col != 0) {
            if (col->partition() != 0) {
                which = col->partition()->name();
                which += ".";
            }
            which += col->name();
            which += '.';
        }
        which += "measureCrossProduct";
        crossproduct = ibis::gParameters().isTrue(which.c_str());

        ibis::util::logger lg;
        lg() << "bin::speedTest testing the speed of "
             << (crossproduct ? "corss product operation" : "operator|")
             << "\n# bits, # 1s, # 1s, # bytes, # bytes, clustering factor, "
            "result 1s, result bytes, wall time";
    }
    if (crossproduct) {
        nloops = 2;
    }

    for (uint32_t i = 1; i < bits.size(); ++ i) {
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
                << "Warning -- bin::speedTest received a standard "
                "exception while calling operator | (i=" << i << ") - "
                << e.what();
            continue;
        }
        catch (const char* s) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- bin::speedTest received a string exception "
                "while calling operator | (i=" << i << ") - " << s;
            continue;
        }
        catch (...) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- bin::speedTest received an unexpected "
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
                << timer.realTime() / nloops << std::endl;
        }
        catch (...) {
        }
    }
} // ibis::bin::speedTest

// the printing function
void ibis::bin::print(std::ostream& out) const {
    if (bits.size() == 0) return;
    if (nrows == 0) return;

    // print only the first npr bins
    uint32_t npr = (ibis::gVerbose < 30 ? (1 << ibis::gVerbose) : nobs);
    npr = (npr > nobs ? nobs : npr) - 1;
    uint32_t omt = 0;

    // activate(); -- activate may invoke ioLock and cause deadlocking
    out << "index (equality encoded, binned) for "
        << (col ? col->fullname() : "?")
        << " contains " << nobs << " bitvectors for "
        << nrows << " objects \n";
    if (ibis::gVerbose > 3 || nobs == 1) { // print the long form
        uint32_t i, cnt = 0;
        if (ibis::gVerbose > 7)
            out << std::setprecision(18);
        else if (ibis::gVerbose > 5)
            out << std::setprecision(14);
        else
            out << std::setprecision(10);
        out << "0: ";
        if (bits[0]) {
            out << bits[0]->cnt();
            cnt += bits[0]->cnt();
        }
        else {
            out << "??";
        }
        out << "\t(..., " << bounds[0] << ")\t[" << minval[0]
            << ", " << maxval[0] << "]\n";
        if (nobs == 1) return;

        for (i = 1; i < npr; ++i) {
            if (bits[i] != 0) {
                out << i << ": " << bits[i]->cnt() << "\t["
                    << bounds[i-1] << ", " << bounds[i] << ")\t["
                    << minval[i] << ", " << maxval[i] << "]\n";
                cnt += bits[i]->cnt();
            }
            else {
                ++ omt;
            }
        }
        omt += nobs-1-npr;
        i = nobs-1;
        if (omt > 0) {
            out << " ...\t(" << omt << " omitted)\n";
        }

        out << i << ": ";
        if (bits[i] != 0) {
            out << bits[i]->cnt();
            cnt += bits[i]->cnt();
        }
        else {
            out << "??";
        }
        out << "\t[" << bounds[i-1] << ", " << bounds[i]
            << ")\t[" << minval[i] << ", " << maxval[i] << "]\n";
        for (i = 0; i < nobs; ++i) {
            if (bits[i] != 0 && nrows != bits[i]->size())
                out << "Warning -- bits[" << i << "] contains "
                    << bits[i]->size() << " bits, but expected " << nrows;
        }
        if (nrows < cnt) {
            out << "Warning -- There are a total " << cnt << " set bits out of "
                << nrows << " bits in an index for " << (col ? col->name() : "?")
                << "\n";
        }
        else if (nrows > cnt) {
            out << "There are a total " << cnt << " set bits out of " << nrows
                << " bits -- there are probably NULL values in column "
                << (col ? col->name() : "?") << "\n";
        }
    }
    else if (nobs > 0) { // the short form
        out << "The three columns are (1) center of bin, (2) bin weight, "
            "(3) bit vector size (bytes)\n";
        for (uint32_t i=0; i<npr; ++i) {
            if (bits[i]) {
                out.precision(12);
                out << 0.5*(minval[i]+maxval[i]) << '\t'
                    << bits[i]->cnt() << '\t' << bits[i]->bytes() << "\n";
            }
            else {
                ++ omt;
            }
        }
        omt = nobs - npr;
        if (omt > 0) {
            out << " ...\t(" << omt << " omitted)\n";
        }
    }
} // ibis::bin::print

// the given range is limited to the range of [lbound, rbound) with a
// maximum count of tot
void ibis::bin::print(std::ostream& out, const uint32_t tot,
                      const double& lbound, const double& rbound) const {
    if (nrows == 0) return;

    // print only the first npr bins
    uint32_t npr = (ibis::gVerbose < 30 ? (1 << ibis::gVerbose) : nobs);
    npr = (npr+npr >= nobs ? nobs : npr);
    uint32_t omt = 0;

    //activate(); -- may cause another invocation of ioLock
    if (ibis::gVerbose > 4) { // long format
        uint32_t i, cnt = 0;
        out << "\trange [" << lbound << ", " << rbound
            << ") is subdivided into " << nobs << " bins\n";
        if (bits[0] != 0) {
            out << "\t" << bits[0]->cnt() << "\t[" << lbound << ", "
                << bounds[0] << ")\t[" << minval[0] << ", " << maxval[0]
                << "]\n";
            cnt += bits[0]->cnt();
            if (nrows != bits[0]->size())
                out << "Warning: bits[0] contains "
                    << bits[0]->size()
                    << " bits, but " << nrows << " are expected\n";
        }
        for (i = 1; i < nobs; ++ i) {
            if (bits[i] == 0) {
                ++ omt;
                continue;
            }
            cnt += bits[i]->cnt();
            if (i < npr)
                out << "\t" << bits[i]->cnt() << "\t[" << bounds[i-1]
                    << ", " << bounds[i] << ")\t["
                    << minval[i] << ", " << maxval[i] << "]\n";
            else
                ++ omt;
            if (nrows != bits[i]->size())
                out << "Warning: bits[" << i << "] contains "
                    << bits[i]->size()
                    << " bits, but " << nrows << " are expected\n";
        }
        if (rbound != bounds.back())
            out << "Warning: rbound(" << rbound << ") should be the same as "
                << bounds.back() << ", but is not\n";
        if (cnt != tot)
            out << "Warning: " << tot << "bits are expected in [" << lbound
                << ", " << rbound << "), but " << cnt << " are found";
    }
    else { // short format
        for (uint32_t i=0; i<npr; ++ i) {
            if (bits[i] != 0 && bits[i]->cnt()) {
                out.precision(12);
                out << i << ": "
                    << (maxval[i]!=-DBL_MAX?maxval[i]:bounds[i]) << '\t'
                    << bits[i]->cnt() << '\t' << bits[i]->bytes() << "\n";
            }
            else {
                ++ omt;
            }
        }
        omt += nobs - npr;
    }
    if (omt > 0) {
        out << "\t ...\t(" << omt << " omitted)";
        out << std::endl;
    }
} // ibis::bin::print

// It automatically extends the number of bins and assigned the new ones
// with min and max values.  Used by ibis::category::append.
long ibis::bin::append(const array_t<uint32_t>& ind) {
    if (ind.empty()) return 0; // nothing to do
    uint32_t i;

    try {
        // only in-memory version of the bitvectors are safe for
        // modification
        activate(); // make sure all bitvectors are activated
    }
    catch (const std::exception& e) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- bin::append received a std::exception - "
            << e.what();
        return -1;
    }
    catch (const char* s) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- bin::append received a string exception - " << s;
        return -1;
    }
    catch (...) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- bin::append received a unknown exception";
        return -1;
    }

    // make copy of each bitvector
    for (i = 0; i < nobs; ++i) {
        if (bits[i]) {
            ibis::bitvector* tmp = bits[i];
            bits[i] = new ibis::bitvector(*tmp);
            delete tmp;
        }
    }

    for (i = 0; i < ind.size(); ++i, ++nrows) {
        uint32_t j = ind[i];
        if (j >= nobs) {
            for (uint32_t k = nobs; k <= j; ++k) {
                bits.push_back(new ibis::bitvector);
                maxval.push_back(k);
                minval.push_back(k);
                bounds.push_back(k+0.5);
            }
            nobs = j + 1;
        }
        bits[j]->setBit(nrows, 1);
    }
    for (i = 0; i < nobs; ++i)
        bits[i]->adjustSize(0, nrows);
    return ind.size();
} // ibis::bin::append

/// Create index for the data in df and append the result to the index in dt.
long ibis::bin::append(const char* dt, const char* df, uint32_t nnew) {
    if (col == 0) return -1;
    if (nnew == 0) return 0;

    const uint32_t nold =
        (std::strcmp(dt, col->partition()->currentDataDir()) == 0 ?
         col->partition()->nRows()-nnew : nrows);
    if (nrows != nold) { // recreate the new index
#ifdef APPEND_UPDATE_INDEXES
        LOGGER(ibis::gVerbose > 3)
            << "bin::append to build a new index for " << col->name()
            << " using data in " << dt;
        clear(); // clear the current content
        construct(dt);
#endif
        return nnew;
    }

    std::string fnm;
    indexFileName(fnm, df);
    ibis::bin* bin0=0;
    ibis::fileManager::storage* st0=0;
    long ierr = ibis::fileManager::instance().getFile(fnm.c_str(), &st0);
    if (ierr == 0 && st0 != 0) {
        const char* header = st0->begin();
        if (header[0] == '#' && header[1] == 'I' && header[2] == 'B' &&
            header[3] == 'I' && header[4] == 'S' &&
            header[5] == ibis::index::BINNING &&
            header[7] == static_cast<char>(0)) {
            bin0 = new ibis::bin(col, st0);
        }
        else {
            delete st0;
            st0 = 0;
            if (ibis::gVerbose > 5)
                col->logMessage("bin::append", "file \"%s\" has unexecpted "
                                "header -- it will be removed",
                                fnm.c_str());
            ibis::fileManager::instance().flushFile(fnm.c_str());
            remove(fnm.c_str());
        }
    }

    if (bin0 == 0) {
        if (col->type() == ibis::TEXT) {
            fnm.erase(fnm.size()-3);
            fnm += "int";
            if (ibis::util::getFileSize(fnm.c_str()) > 0)
                bin0 = new ibis::bin(col, fnm.c_str(), bounds);
            else {
                col->logWarning("bin::append", "file \"%s\" must exist "
                                "before calling this function",
                                fnm.c_str());
                ierr = -2;
                return ierr;
            }
        }
        else {
            bin0 = new ibis::bin(col, df, bounds);
        }
    }
    if (bin0 == 0) {
        return 0;
    }
    if (bits.empty() || nrows == 0) {
        swap(*bin0);
        delete bin0;
        ierr = nrows;
        return ierr;
    }

    try {
        activate();
    }
    catch (const std::exception& e) {
        col->logWarning("bin::append", "received a std::exception while "
                        "reading from %s - %s",
                        dt, e.what());
        clear();
    }
    catch (const char* s) {
        col->logWarning("bin::append", "received a string exception while "
                        "reading from %s - %s", dt, s);
        clear();
    }
    catch (...) {
        col->logWarning("bin::append", "received a unknown exception while "
                        "reading from %s", dt);
        clear();
    }

    try {
        bin0->activate();
    }
    catch (const std::exception& e) {
        col->logWarning("bin::append", "received a std::exception while "
                        "reading from %s - %s",
                        df, e.what());
        bin0->clear();
    }
    catch (const char* s) {
        col->logWarning("bin::append", "received a string exception while "
                        "reading from %s - %s", df, s);
        bin0->clear();
    }
    catch (...) {
        col->logWarning("bin::append", "received a unknown exception while "
                        "reading from %s", df);
        bin0->clear();
    }

    const uint32_t weight =
        (nobs>2 && bin0->nobs>2 ?
         (bits[0]->size()+bin0->bits[0]->size())/(nobs-2) : 0);
    bool samebounds = (weight > 0 && nobs == bin0->nobs);
    for (uint32_t i = 0; samebounds && i < nobs; ++ i)
        samebounds = (bounds[i] == bin0->bounds[i]);
    if (! samebounds) { // bounds different can not reuse
        delete bin0;
#ifdef APPEND_UPDATE_INDEXES
        if (ibis::gVerbose > 3)
            col->logMessage("bin::append", "the index in %s does not have "
                            "the same bin boundaries as the one in %s, "
                            "has to build new bins", dt, df);
        clear();        // clear the current content
        binning(dt);
        return nnew;
#else
        if (ibis::gVerbose > 1)
            col->logMessage("bin::append", "bin boundaries do NOT match, "
                            "can not append indices");
        return -6;
#endif
    }
    else if ((bits[0]->cnt()+(bits.back())->cnt()+bin0->bits[0]->cnt()+
              (bin0->bits.back())->cnt()) > weight+weight) {
        // the outside bins contain too may entries
#ifdef APPEND_UPDATE_INDEXES
        if (ibis::gVerbose > 3)
            col->logMessage("bin::append", "the combined index (from %s "
                            "and %s) has too many entries in the two end "
                            "bins, has to build new bins", dt, df);
        array_t<double> bnds;
        setBoundaries(bnds, *bin0); // generate new boudaries

        clear(); // clear the current content
        bin0->clear();

        binning(dt, bnds); // rebuild indices using new boundaries
        bin0->binning(df, bnds);
        bin0->write(df);
        delete bin0;
        return nnew;
#else
        delete bin0;
        if (ibis::gVerbose > 1)
            col->logMessage("bin::append", "bins are highly unbalanced, "
                            "choosing not to append indices");
        return -7;
#endif
    } // repartition the bins
    else { // no need to repartition
        if (ibis::gVerbose > 5)
            col->logMessage("bin::append", "appending the index from %s to "
                            "the one from %s", df, dt);
        ierr = append(*bin0);
        delete bin0;
        if (ierr == 0) {
            return nnew;
        }
        else {
            return ierr;
        }
    }
} // ibis::bin::append

long ibis::bin::append(const ibis::bin& tail) {
    uint32_t i;
    if (tail.col != col) return -1;
    if (tail.nobs != nobs) return -2;
    if (tail.bits.empty()) return -3;
    if (tail.bits[0]->size() != tail.bits[1]->size()) return -4;
    for (i = 0; i < nobs; ++i)
        if (tail.bounds[i] != bounds[i]) return -5;

    // generate the new minval, maxval
    uint32_t n0 = nrows, n1 = tail.nrows;
    array_t<double> min2, max2;
    min2.resize(nobs);
    max2.resize(nobs);
    for (i = 0; i < nobs; ++i) {
        if (tail.minval[i] <= minval[i])
            min2[i] = tail.minval[i];
        else
            min2[i] = minval[i];
        if (tail.maxval[i] >= maxval[i])
            max2[i] = tail.maxval[i];
        else
            max2[i] = maxval[i];
    }

    // replace the current content with the new one
    minval.swap(min2);
    maxval.swap(max2);

    // deal with the bit vectors
    const uint32_t nb = bits.size();
    array_t<bitvector*> bin2;
    bin2.resize(nobs);

    activate();
    tail.activate();
    for (i = 0; i < nb; ++i) {
        bin2[i] = new ibis::bitvector;
        bin2[i]->copy(*bits[i]);        // generate an in-memory copy
        *bin2[i] += *(tail.bits[i]);
    }
    bits.swap(bin2);
    nrows = n0 + n1;
    // clean up bin2
    for (i = 0; i < nb; ++i)
        delete bin2[i];

    if (ibis::gVerbose > 10) {
        ibis::util::logger lg;
        lg() << "\nNew combined index (append an index for " << n1
             << " objects to an index for " << n0 << " events\n" ;
        print(lg());
    }
    return 0;
} // ibis::bin::append

// convert the bitvector mask into bin indices -- used by
// ibis::category::selectUInt
ibis::array_t<uint32_t>* ibis::bin::indices(const ibis::bitvector& mask) const {
    ibis::bitvector* tmp = 0;
    std::map<uint32_t, uint32_t> ii;
    activate();

    for (uint32_t i=0; i<nobs; ++i) { // loop to generate ii
        if (bits[i]) {
            uint32_t nind = 0;
            tmp = mask & *(bits[i]);
            do {
                ibis::bitvector::indexSet is = tmp->firstIndexSet();
                const ibis::bitvector::word_t *iix = is.indices();
                nind = is.nIndices();
                if (is.isRange()) {
                    for (uint32_t j = *iix; j < iix[1]; ++j)
                        ii[j] = i;
                }
                else if (nind > 0) {
                    for  (uint32_t j = 0; j < nind; ++j)
                        ii[iix[j]] = i;
                }
            } while (nind > 0);
            delete tmp;
        }
    }

    if (ii.empty()) {
        return 0;
    }
    else {
        array_t<uint32_t>* ret = new array_t<uint32_t>(ii.size());
        std::map<uint32_t, uint32_t>::const_iterator it = ii.begin();
        for (uint32_t i = 0; i < ii.size(); ++i) {
            (*ret)[i] = (*it).second;
            ++it;
        }
        return ret;
    }
} // ibis::bin::indices

double ibis::bin::estimateCost(const ibis::qContinuousRange& expr) const {
    double ret = 0;
    uint32_t cand0=0, cand1=nobs, hit0=nobs, hit1=0;
    if (offset64.size() > bits.size()) {
        locate(expr, cand0, cand1, hit0, hit1);
        if (cand0 < cand1 && cand1 < offset64.size()) {
            const int64_t tot = offset64.back() - offset64[0];
            const int64_t mid = offset64[cand1] - offset64[cand0];
            if ((tot >> 1) >= mid)
                ret = mid;
            else
                ret = tot - mid;
        }
    }
    else if (offset32.size() > bits.size()) {
        locate(expr, cand0, cand1, hit0, hit1);
        if (cand0 < cand1 && cand1 < offset32.size()) {
            const int32_t tot = offset32.back() - offset32[0];
            const int32_t mid = offset32[cand1] - offset32[cand0];
            if ((tot >> 1) >= mid)
                ret = mid;
            else
                ret = tot - mid;
        }
    }
    if (col != 0 && (hit0 > cand0 || hit1 < cand1)) {
        if (nobs > 0) {
            const double ccheck = col->elementSize() * nrows / nobs;
            if (hit0 > cand0 && hit1 < cand1 && hit0 <= hit1)
                ret += 2.0 * ccheck;
            else
                ret += ccheck;
        }
        else {
            ret += col->elementSize() * nrows;
        }
    }
    return ret;
} // ibis::bin::estimateCost

double ibis::bin::estimateCost(const ibis::qDiscreteRange& expr) const {
    double ret = 0;
    const ibis::array_t<double>& vals = expr.getValues();
    if (offset64.size() > bits.size()) {
        std::vector<uint32_t> bins(vals.size());
        for (unsigned j = 0; j < vals.size(); ++ j)
            bins[j] = locate(vals[j]);
        std::sort(bins.begin(), bins.end());
        uint32_t last = bins[0];
        if (last < bits.size()) {
            ret = offset64[last+1] - offset64[last];
            for (unsigned j = 1; j < vals.size(); ++ j)
                if (bins[j] > last) {
                    last = bins[j];
                    if (bins[j] < bits.size())
                        ret += offset64[last+1] - offset64[last];
                }
        }
    }
    else if (offset32.size() > bits.size()) {
        std::vector<uint32_t> bins(vals.size());
        for (unsigned j = 0; j < vals.size(); ++ j)
            bins[j] = locate(vals[j]);
        std::sort(bins.begin(), bins.end());
        uint32_t last = bins[0];
        if (last < bits.size()) {
            ret = offset32[last+1] - offset32[last];
            for (unsigned j = 1; j < vals.size(); ++ j)
                if (bins[j] > last) {
                    last = bins[j];
                    if (bins[j] < bits.size())
                        ret += offset32[last+1] - offset32[last];
                }
        }
    }
    if (col != 0 && nobs > vals.size())
        ret += static_cast<double>(vals.size() * col->elementSize()) *
            nrows / nobs;
    else
        ret += col->elementSize() * nrows;
    return ret;
} // ibis::bin::estimateCost

long ibis::bin::evaluate(const ibis::qContinuousRange& expr,
                         ibis::bitvector& lower) const {
    if (nobs <= 0 || nrows == 0) {
        lower.set(0, nrows);
        return 0L;
    }

    // the following four variables describes a range where the solution lies
    // bitvectors in [hit0, hit1) ==> lower
    // bitvectors in [cand0, cand1) ==> upper
    // the four values are expected to be in the following order
    // cand0 <= hit0 <= hit1 <= cand1
    uint32_t cand0=0, hit0=0, hit1=0, cand1=0;
    locate(expr, cand0, cand1, hit0, hit1);
    if (hit1 < hit0)
        hit1 = hit0;
    sumBins(hit0, hit1, lower);
    long ierr0 = 0, ierr1 = 0;
    if (cand0 < hit0) {
        ibis::bitvector tmp;
        ierr0 = checkBin(expr, cand0, tmp);
        if (ierr0 >= 0) {
            lower |= tmp;
        }
    }
    if (cand1 > hit1 && hit1 < nobs) {
        if (ierr0 >= 0) {
            ibis::bitvector tmp;
            ierr1 = checkBin(expr, hit1, tmp);
            if (ierr1 >= 0) {
                lower |= tmp;
            }
        }
        else {
            ierr1 = ierr0;
        }
    }
    if (ierr0 < 0 || ierr1 < 0) {
        ibis::bitvector mask;
        if (ierr0 < 0) {
            if (bits[cand0] == 0)
                activate(cand0);
            if (bits[cand0] != 0)
                mask.copy(*(bits[cand0]));
        }
        if (ierr1 < 0) {
            if (bits[hit1] == 0)
                activate(hit1);
            if (bits[hit1] != 0) {
                if (mask.size() != bits[hit1]->size())
                    mask.copy(*(bits[hit1]));
                else
                    mask |= *(bits[hit1]);
            }
        }
        if (mask.size() <= nrows && mask.cnt() > 0) {
            ibis::bitvector delta;
            if (col != 0 && col->hasRawData())
                ierr1 = col->partition()->doScan(expr, mask, delta);
            else
                ierr1 = -4;
            if (ierr1 > 0) {
                if (delta.size() == lower.size()) {
                    lower |= delta;
                    ierr0 = lower.cnt();
                }
                else if (lower.size() == 0) {
                    lower.swap(delta);
                    ierr0 = lower.cnt();
                }
                else {
                    LOGGER(ibis::gVerbose > 0)
                        << "Warning -- bin::evaluate encountered an internal "
                        "problem: the result of doScan (" << delta.size()
                        << ", " << delta.cnt()
                        << ") does not match the result of sumBins ("
                        << lower.size() << ", " << lower.cnt() << ")";
                    ierr0 = -5;
                }
            }
            else if (ierr1 == 0) {
                ierr0 = lower.cnt();
            }
            else {
                ierr0 = ierr1;
            }
        }
        else if (ierr1 < 0) {
            ierr0 = ierr1;
        }
    }
    else {
        ierr0 = lower.cnt();
    }
#if DEBUG+0 > 0
    LOGGER(ibis::gVerbose >= 0)
        << "DEBUG -- bin::evaluate(" << expr << ")\nlower = \n" << lower;
#endif
    return ierr0;
} // ibis::bin::evaluate

/// Provide an estimation based on the current index.  Set bits in lower
/// are hits for certain, set bits in upper are candidates.  Set bits in
/// (upper - lower) should be checked to verifies which ones are actually
/// hits.  If the bitvector upper contain less bits than bitvector lower,
/// the content of upper is assumed to be the same as lower.
///
/// @note This function will not do anything if the estimated cost is high.
void ibis::bin::estimate(const ibis::qContinuousRange& expr,
                         ibis::bitvector& lower,
                         ibis::bitvector& upper) const {
    if (nobs <= 0 || nrows == 0) {
        lower.set(0, nrows);
        upper.clear();
        return;
    }

    // the following four variables describes a range where the solution lies
    // bitvectors in [hit0, hit1) ==> lower
    // bitvectors in [cand0, cand1) ==> upper
    // the four values are expected to be in the following order
    // cand0 <= hit0 <= hit1 <= cand1
    uint32_t cand0=0, hit0=0, hit1=0, cand1=0;
    locate(expr, cand0, cand1, hit0, hit1);
    if (hit1 < hit0)
        hit1 = hit0;
    // Oct. 11, 2001 --
    // tests show that simply use sumBits (which uses
    // ibis::bitvector::operator|= to perform logical OR operations) is a
    // reasonable choice compared to the alternative
    // see irelic.cpp for an example of the alternatives that have been
    // considered.
    // May 9, 2006 --
    // changed to sumBins to take advantage of automatic activation
    // Feb. 8, 2011 -- check the cost first before operating on the bitmaps
    double cost = 0.0; // the total cost of resolving the expression
#ifndef FASTBIT_ESTIMATION_IGNORE_COST
    if (offset64.size() > bits.size()) {
        if (cand0 < cand1 && cand1 < offset64.size()) {
            const int64_t tot = offset64.back() - offset64[0];
            const int64_t mid = offset64[cand1] - offset64[cand0];
            if ((tot >> 1) >= mid)
                cost = mid;
            else
                cost = tot - mid;
        }
    }
    else if (offset32.size() > bits.size()) {
        if (cand0 < cand1 && cand1 < offset32.size()) {
            const int32_t tot = offset32.back() - offset32[0];
            const int32_t mid = offset32[cand1] - offset32[cand0];
            if ((tot >> 1) >= mid)
                cost = mid;
            else
                cost = tot - mid;
        }
    }
    if (col != 0) {
        if (hit0 > cand0 && cand1 > hit1) {
            cost += (col->elementSize() * (double)nrows / nobs) * 2.0;
        }
        else if (hit0 > cand0 || cand1 > hit1) {
            cost += (col->elementSize() * (double)nrows / nobs);
        }
    }
#endif
    if (cand0 >= cand1) {
        lower.set(0, nrows);
        upper.set(0, nrows);
        LOGGER(ibis::gVerbose > 5)
            << "bin::estimate(" << expr << ") finds no hit";
    }
    else if (col != 0 && col->hasRawData() && cost > nrows*0.75) {
        lower.set(0, nrows);
        upper.set(1, nrows);
        LOGGER(ibis::gVerbose > 5)
            << "bin::estimate(" << expr << ") gives up to avoid costly "
            "operations involving the index";
    }
    else if (hit0 < hit1) {
        sumBins(hit0, hit1, lower);
        if (cand0 < hit0 || (cand1 > hit1 && hit1 < nobs)) {
            upper.copy(lower);
            if (cand0 < hit0) {
                if (bits[cand0] == 0)
                    activate(cand0);
                if (bits[cand0] != 0)
                    upper |= *(bits[cand0]);
            }
            if (cand1 > hit1 && hit1 < nobs) {
                if (bits[hit1] == 0)
                    activate(hit1);
                if (bits[hit1] != 0)
                    upper |= *(bits[hit1]);
            }
            LOGGER(ibis::gVerbose > 5)
                << "bin::estimate(" << expr << ") completed with lower.cnt() = "
                << lower.cnt() << ", upper.cnt() = " << upper.cnt();
        }
        else {
            upper.clear();
            LOGGER(ibis::gVerbose > 5)
                << "bin::estimate(" << expr << ") completed with "
                << lower.cnt() << " hit(s)";
        }
    }
    else {
        lower.set(0, nrows);
        sumBins(cand0, cand1, upper);
    }
#if DEBUG+0 > 0
    LOGGER(ibis::gVerbose >= 0)
        << "DEBUG -- bin::estimate(" << expr << ")\nlower = \n"
        << lower << "upper = \n" << upper;
#endif
} // ibis::bin::estimate

/// Compute an upper bound on the number of hits.
uint32_t ibis::bin::estimate(const ibis::qContinuousRange& expr) const {
    uint32_t cand0, cand1, nhits=0;
    if (nobs <= 0) return 0;

    locate(expr, cand0, cand1);
    // go through the list of bitvectors and count the number of hits
    if (cand1 <= cand0) {
        nhits = 0;
    }
    else if ((offset64.size() > nobs && offset64[cand1] - offset64[cand0]
              <= (offset64[nobs] - offset64[0])/2)
             || (offset32.size() > nobs && offset32[cand1] - offset32[cand0]
                 <= (offset32[nobs] - offset32[0])/2)
             || 2*(cand1-cand0) <= nobs) {
        if (col != 0 && col->hasRawData() &&
            ((offset64.size() > nobs &&
              offset64[cand1] - offset64[cand0] > 0.75*nrows) ||
             (offset32.size() > nobs &&
              offset32[cand1] - offset32[cand0] > 0.75*nrows))) {
            nhits = nrows; // give to avoid costly operations
            LOGGER(ibis::gVerbose > 5)
                << "bin::estimate(" << expr << ") gives up to avoid costly "
                "operations";
        }
        else {
            activate(cand0, cand1);
            for (uint32_t i=cand0; i < cand1; ++i) {
                if (bits[i])
                    nhits += bits[i]->cnt();
            }
        }
    }
    else if (col != 0 && col->hasRawData() &&
             ((offset64.size() > nobs && offset64.back()-offset64.front()
               -offset64[cand1]+offset64[cand0] > 0.75*nrows) ||
              (offset32.size() > nobs && offset32.back()-offset32.front()
               -offset32[cand1]+offset32[cand0]))) {
        nhits = nrows;
        LOGGER(ibis::gVerbose > 5)
            << "bin::estimate(" << expr << ") gives up to avoid costly "
            "operations";
    }
    else { // use complements
        nhits = 0;
        activate(0, cand0);
        for (uint32_t i = 0; i < cand0; ++i) {
            if (bits[i])
                nhits += bits[i]->cnt();
        }
        activate(cand1, nobs);
        for (uint32_t i = cand1; i < nobs; ++i) {
            if (bits[i])
                nhits += bits[i]->cnt();
        }
        nhits = nrows - nhits;
    }
    return nhits;
} // ibis::bin::estimate

// The bitvector iffy marks the position of rows that can not be decided
// using the current bitmap index.
// This function returns the fraction rows that are expected to satisfy the
// range condition
float ibis::bin::undecidable(const ibis::qContinuousRange &expr,
                             ibis::bitvector &iffy) const {
    float ret = 0.0;
    if (nobs <= 0)
        return ret;

    uint32_t cand0=0, hit0=0, hit1=0, cand1=0;
    iffy.set(0, nrows);
    locate(expr, cand0, cand1, hit0, hit1);
    if (cand1 <= cand0)
        return ret;

    if (cand0+1 == hit0) { // a boundary bin
        if (bits[cand0] == 0)
            activate(cand0);
        if (bits[cand0]) {
            iffy.copy(*(bits[cand0]));
            if (minval[cand0] < maxval[cand0]) {
                ret = static_cast<float>
                    ((maxval[cand0] - expr.leftBound()) /
                     (maxval[cand0] - minval[cand0]));
                if (ret == 0.0)
                    ret = FLT_EPSILON;
            }
        }
    }

    if (hit1+1 == cand1 && hit1 < nobs) {
        if (bits[hit1] == 0)
            activate(hit1);
        if (bits[hit1]) {
            iffy |= *(bits[hit1]);
            if (minval[hit1] < maxval[hit1]) {
                float tmp = static_cast<float>
                    ((expr.rightBound() - minval[hit1]) /
                     (maxval[hit1] - minval[hit1]));
                if (ret != 0.0)
                    ret = 0.5 * (ret + tmp);
            }
        }
    }
    return ret;
} // ibis::bin::undecidable

// expand range condition -- rely on the fact that the only operators used
// are LT, LE and EQ
int ibis::bin::expandRange(ibis::qContinuousRange& rng) const {
    uint32_t cand0, cand1;
    double left, right;
    int ret = 0;
    locate(rng, cand0, cand1);
    if (cand0 < nobs) {
        if ((rng.leftOperator() == ibis::qExpr::OP_LT &&
             rng.leftBound() >= minval[cand0]) ||
            (rng.leftOperator() == ibis::qExpr::OP_LE &&
             rng.leftBound() > minval[cand0])) {
            // decrease the left bound
            ++ ret;
            right = minval[cand0];
            if (cand0 > 0)
                left = maxval[cand0-1];
            else
                left = -DBL_MAX;
            rng.leftBound() = ibis::util::compactValue(left, right);
        }
        else if (rng.leftOperator() == ibis::qExpr::OP_EQ &&
                 (rng.leftBound() > minval[cand0] ||
                  rng.leftBound() < maxval[cand0])) {
            // expand the equality condition into a range condition
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
    if (cand1 > 0 &&
        ((rng.rightOperator() == ibis::qExpr::OP_LT &&
          rng.rightBound() > minval[cand1-1]) ||
         (rng.rightOperator() == ibis::qExpr::OP_LE &&
          rng.rightBound() >= minval[cand1-1]))) {
        // increase the right bound
        ++ ret;
        left = maxval[cand1-1];
        if (cand1 < nobs)
            right = minval[cand1];
        else
            right = DBL_MAX;
        rng.rightBound() = ibis::util::compactValue(left, right);
    }
    return ret;
} // ibis::bin::expandRange

// contract range condition -- rely on the fact that the only operators
// used are LT, LE and EQ
int ibis::bin::contractRange(ibis::qContinuousRange& rng) const {
    uint32_t cand0, cand1;
    double left, right;
    int ret = 0;
    locate(rng, cand0, cand1);
    if (cand0 < nobs) {
        if ((rng.leftOperator() == ibis::qExpr::OP_LT &&
             rng.leftBound() <= maxval[cand0]) ||
            (rng.leftOperator() == ibis::qExpr::OP_LE &&
             rng.leftBound() < maxval[cand0])) {
            // increase the left bound
            ++ ret;
            left = maxval[cand0];
            if (cand0+1 < nobs)
                right = minval[cand0+1];
            else
                right = DBL_MAX;
            rng.leftBound() = ibis::util::compactValue(left, right);
        }
        else if (rng.leftOperator() == ibis::qExpr::OP_EQ &&
                 (rng.leftBound() > minval[cand0] ||
                  rng.leftBound() < maxval[cand0])) {
            // reduce the equality to no value
            ++ ret;
            right = minval[cand0];
            if (cand0 > 0)
                left = maxval[cand0-1];
            else
                left = -DBL_MAX;
            rng.leftBound() = ibis::util::compactValue(left, right);
        }
    }
    if (cand1 > 0 &&
        ((rng.rightOperator() == ibis::qExpr::OP_LT &&
          rng.rightBound() > minval[cand1-1]) ||
         (rng.rightOperator() == ibis::qExpr::OP_LE &&
          rng.rightBound() >= minval[cand1-1]))) {
        // decrease the right bound
        ++ ret;
        right = minval[cand1-1];
        if (cand1 > 1)
            left = maxval[cand1-2];
        else
            left = -DBL_MAX;
        rng.rightBound() = ibis::util::compactValue(left, right);
    }
    return ret;
} // ibis::bin::contractRange

/// Locate the outer reaches of a continuous range expression.
void ibis::bin::locate(const ibis::qContinuousRange& expr, uint32_t& cand0,
                       uint32_t& cand1) const {
    uint32_t bin0 = (expr.leftOperator()!=ibis::qExpr::OP_UNDEFINED) ?
        locate(expr.leftBound()) : 0;
    uint32_t bin1 = (expr.rightOperator()!=ibis::qExpr::OP_UNDEFINED) ?
        locate(expr.rightBound()) : 0;
    switch (expr.leftOperator()) {
    default:
    case ibis::qExpr::OP_UNDEFINED:
        switch (expr.rightOperator()) {
        default:
        case ibis::qExpr::OP_UNDEFINED:
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- bin::locate encountered an ill-formed range "
                "condition";
            cand0 = 0;
            cand1 = 0;
            return;
        case ibis::qExpr::OP_LT:
            cand0 = 0;
            if (bin1 >= nobs) {
                cand1 = nobs;
            }
            else if (expr.rightBound() <= minval[bin1]) {
                cand1 = bin1;
            }
            else {
                cand1 = bin1 + 1;
            }
            break;
        case ibis::qExpr::OP_LE:
            cand0 = 0;
            if (bin1 >= nobs) {
                cand1 = nobs;
            }
            else if (expr.rightBound() < minval[bin1]) {
                cand1 = bin1;
            }
            else {
                cand1 = bin1 + 1;
            }
            break;
        case ibis::qExpr::OP_GT:
            cand1 = nobs;
            if (bin1 >= nobs) {
                cand0 = nobs;
            }
            else if (expr.rightBound() >= maxval[bin1]) {
                cand0 = bin1 + 1;
            }
            else {
                cand0 = bin1;
            }
            break;
        case ibis::qExpr::OP_GE:
            cand1 = nobs;
            if (bin1 >= nobs) {
                cand0 = nobs;
            }
            else if (expr.rightBound() > maxval[bin1]) {
                cand0 = bin1 + 1;
            }
            else {
                cand0 = bin1;
            }
            break;
        case ibis::qExpr::OP_EQ:
            if (bin1 >= nobs) {
                cand0 = cand1 = 0;
            }
            else if (expr.rightBound() <= maxval[bin1] &&
                     expr.rightBound() >= minval[bin1]) {
                cand0 = bin1;
                cand1 = bin1 + 1;
            }
            else {
                cand0 = cand1 = 0;
            }
            break;
        } // switch (expr.rightOperator())
        break; // case ibis::qExpr::OP_UNDEFINED
    case ibis::qExpr::OP_LT:
        if (bin0 >= nobs) {
            cand0 = nobs;
        }
        else if (expr.leftBound() > maxval[bin0]) {
            cand0 = bin0 + 1;
        }
        else {
            cand0 = bin0;
        }
        switch (expr.rightOperator()) {
        default:
        case ibis::qExpr::OP_UNDEFINED:
            cand1 = nobs;
            break;
        case ibis::qExpr::OP_LT:
            if (bin1 >= nobs) {
                cand1 = nobs;
            }
            else if (expr.rightBound() <= minval[bin1]) {
                cand1 = bin1;
            }
            else {
                cand1 = bin1 + 1;
            }
            break;
        case ibis::qExpr::OP_LE:
            if (bin1 >= nobs) {
                cand1 = nobs;
            }
            else if (expr.rightBound() < minval[bin1]) {
                cand1 = bin1;
            }
            else {
                cand1 = bin1 + 1;
            }
            break;
        case ibis::qExpr::OP_GT:
            cand1 = nobs;
            if (expr.rightBound() > expr.leftBound()) {
                if (bin1 >= nobs) {
                    cand0 = nobs;
                }
                else if (expr.rightBound() >= maxval[bin1]) {
                    cand0 = bin1 + 1;
                }
                else {
                    cand0 = bin1;
                }
            }
            break;
        case ibis::qExpr::OP_GE:
            cand1 = nobs;
            if (expr.rightBound() > expr.leftBound()) {
                if (bin1 >= nobs) {
                    cand0 = nobs;
                }
                else if (expr.rightBound() > maxval[bin1]) {
                    cand0 = bin1 + 1;
                }
                else if (expr.rightBound() <= minval[bin1]) {
                    cand0 = bin1;
                }
                else {
                    cand0 = bin1;
                }
            }
            break;
        case ibis::qExpr::OP_EQ:
            if (expr.rightBound() < expr.leftBound()) {
                if (bin1 >= nobs) {
                    cand0 = cand1 = 0;
                }
                else if (expr.rightBound() <= maxval[bin1] &&
                         expr.rightBound() >= minval[bin1]) {
                    cand0 = bin1;
                    cand1 = bin1 + 1;
                }
                else {
                    cand0 = cand1 = 0;
                }
            }
            else {
                cand0 = cand1 = 0;
            }
            break;
        } // switch (expr.rightOperator())
        break; // case ibis::qExpr::OP_LT
    case ibis::qExpr::OP_LE:
        if (bin0 >= nobs) {
            cand0 = nobs;
        }
        else if (expr.leftBound() > maxval[bin0]) {
            cand0 = bin0 + 1;
        }
        else {
            cand0 = bin0;
        }
        switch (expr.rightOperator()) {
        default:
        case ibis::qExpr::OP_UNDEFINED:
            cand1 = nobs;
            break;
        case ibis::qExpr::OP_LT:
            if (bin1 >= nobs) {
                cand1 = nobs;
            }
            else if (expr.rightBound() <= minval[bin1]) {
                cand1 = bin1;
            }
            else {
                cand1 = bin1 + 1;
            }
            break;
        case ibis::qExpr::OP_LE:
            if (bin1 >= nobs) {
                cand1 = nobs;
            }
            else if (expr.rightBound() < minval[bin1]) {
                cand1 = bin1;
            }
            else {
                cand1 = bin1 + 1;
            }
            break;
        case ibis::qExpr::OP_GT:
            cand1 = nobs;
            if (expr.rightBound() > expr.leftBound()) {
                if (bin1 >= nobs) {
                    cand0 = nobs;
                }
                else if (expr.rightBound() >= maxval[bin1]) {
                    cand0 = bin1 + 1;
                }
                else {
                    cand0 = bin1;
                }
            }
            break;
        case ibis::qExpr::OP_GE:
            cand1 = nobs;
            if (expr.rightBound() > expr.leftBound()) {
                if (bin1 >= nobs) {
                    cand0 = nobs;
                }
                else if (expr.rightBound() > maxval[bin1]) {
                    cand0 = bin1 + 1;
                }
                else {
                    cand0 = bin1;
                }
            }
            break;
        case ibis::qExpr::OP_EQ:
            if (expr.rightBound() <= expr.leftBound()) {
                if (bin1 >= nobs) {
                    cand0 = cand1 = 0;
                }
                else if (expr.rightBound() <= maxval[bin1] &&
                         expr.rightBound() >= minval[bin1]) {
                    cand0 = bin1;
                    cand1 = bin1 + 1;
                }
                else {
                    cand0 = cand1 = 0;
                }
            }
            else {
                cand0 = cand1 = 0;
            }
            break;
        } // switch (expr.rightOperator())
        break; // case ibis::qExpr::OP_LE
    case ibis::qExpr::OP_GT:
        if (bin0 >= nobs) {
            cand1 = nobs;
        }
        else if (expr.leftBound() <= minval[bin0]) {
            cand1 = bin0;
        }
        else {
            cand1 = bin0 + 1;
        }
        switch (expr.rightOperator()) {
        default:
        case ibis::qExpr::OP_UNDEFINED:
            cand0 = 0;
            break;
        case ibis::qExpr::OP_LT:
            cand0 = 0;
            if (expr.rightBound() < expr.leftBound()) {
                if (expr.rightBound() <= minval[bin1]) {
                    cand1 = bin1;
                }
                else {
                    cand1 = bin1 + 1;
                }
            }
            break;
        case ibis::qExpr::OP_LE:
            cand0 = 0;
            if (expr.rightBound() < expr.leftBound()) {
                if (bin1 >= nobs) {
                    cand1 = nobs;
                }
                else if (expr.rightBound() < minval[bin1]) {
                    cand1 = bin1;
                }
                else {
                    cand1 = bin1 + 1;
                }
            }
            break;
        case ibis::qExpr::OP_GT:
            if (bin1 >= nobs) {
                cand0 = nobs;
            }
            else if (expr.rightBound() >= maxval[bin1]) {
                cand0 = bin1 + 1;
            }
            else {
                cand0 = bin1;
            }
            break;
        case ibis::qExpr::OP_GE:
            if (bin1 >= nobs) {
                cand0 = nobs;
            }
            else if (expr.rightBound() > maxval[bin1]) {
                cand0 = bin1 + 1;
            }
            else {
                cand0 = bin1;
            }
            break;
        case ibis::qExpr::OP_EQ:
            if (expr.rightBound() < expr.leftBound()) {
                if (bin1 >= nobs) {
                    cand0 = cand1 = 0;
                }
                else if (expr.rightBound() <= maxval[bin1] &&
                         expr.rightBound() >= minval[bin1]) {
                    cand0 = bin1;
                    cand1 = bin1 + 1;
                }
                else {
                    cand0 = cand1 = 0;
                }
            }
            else {
                cand0 = cand1 = 0;
            }
            break;
        } // switch (expr.rightOperator())
        break; // case ibis::qExpr::OP_GT
    case ibis::qExpr::OP_GE:
        if (bin0 >= nobs) {
            cand1 = nobs;
        }
        else if (expr.leftBound() < minval[bin0]) {
            cand1 = bin0;
        }
        else {
            cand1 = bin0 + 1;
        }
        switch (expr.rightOperator()) {
        default:
        case ibis::qExpr::OP_UNDEFINED:
            cand0 = 0;
            break;
        case ibis::qExpr::OP_LT:
            cand0 = 0;
            if (expr.rightBound() <= expr.leftBound()) {
                if (bin1 >= nobs) {
                    cand1 = nobs;
                }
                else if (expr.rightBound() <= minval[bin1]) {
                    cand1 = bin1;
                }
                else {
                    cand1 = bin1 + 1;
                }
            }
            break;
        case ibis::qExpr::OP_LE:
            cand0 = 0;
            if (expr.rightBound() < expr.leftBound()) {
                if (expr.rightBound() < minval[bin1]) {
                    cand1 = bin1;
                }
                else {
                    cand1 = bin1 + 1;
                }
            }
            break;
        case ibis::qExpr::OP_GT:
            if (bin1 >= nobs) {
                cand0 = nobs;
            }
            else if (expr.rightBound() > maxval[bin1]) {
                cand0 = bin1 + 1;
            }
            else {
                cand0 = bin1;
            }
            break;
        case ibis::qExpr::OP_GE:
            if (bin1 >= nobs) {
                cand0 = nobs;
            }
            else if (expr.rightBound() > maxval[bin1]) {
                cand0 = bin1 + 1;
            }
            else {
                cand0 = bin1;
            }
            break;
        case ibis::qExpr::OP_EQ:
            if (expr.rightBound() <= expr.leftBound()) {
                if (bin1 >= nobs) {
                    cand0 = cand1 = 0;
                }
                else if (expr.rightBound() <= maxval[bin1] &&
                         expr.rightBound() >= minval[bin1]) {
                    cand0 = bin1;
                    cand1 = bin1 + 1;
                }
                else {
                    cand0 = cand1 = 0;
                }
            }
            else {
                cand0 = cand1 = 0;
            }
            break;
        } // switch (expr.rightOperator())
        break; // case ibis::qExpr::OP_GE
    case ibis::qExpr::OP_EQ:
        switch (expr.rightOperator()) {
        default:
        case ibis::qExpr::OP_UNDEFINED:
            if (bin0 >= nobs) {
                cand0 = cand1 = 0;
            }
            else if (expr.leftBound() <= maxval[bin0] &&
                     expr.leftBound() >= minval[bin0]) {
                cand0 = bin0;
                cand1 = bin0 + 1;
            }
            else {
                cand0 = cand1 = 0;
            }
            break;
        case ibis::qExpr::OP_LT:
            if (expr.leftBound() < expr.rightBound()) {
                if (bin1 >= nobs) {
                    cand0 = cand1 = 0;
                }
                else if (expr.leftBound() >= minval[bin0] &&
                         expr.leftBound() <= maxval[bin0]) {
                    cand0 = bin0;
                    cand1 = bin0 + 1;
                }
                else {
                    cand0 = cand1 = 0;
                }
            }
            else
                cand0 = cand1 = 0;
            break;
        case ibis::qExpr::OP_LE:
            if (expr.leftBound() <= expr.rightBound()) {
                if (bin1 >= nobs) {
                    cand0 = cand1 = 0;
                }
                else if (expr.leftBound() >= minval[bin0] &&
                         expr.leftBound() <= maxval[bin0]) {
                    cand0 = bin0;
                    cand1 = bin0 + 1;
                }
                else {
                    cand0 = cand1 = 0;
                }
            }
            else {
                cand0 = cand1 = 0;
            }
            break;
        case ibis::qExpr::OP_GT:
            if (expr.leftBound() > expr.rightBound()) {
                if (bin1 >= nobs) {
                }
                else if (expr.leftBound() >= minval[bin0] &&
                         expr.leftBound() <= maxval[bin0]) {
                    cand0 = bin0;
                    cand1 = bin0 + 1;
                }
                else
                    cand0 = cand1 = 0;
            }
            else
                cand0 = cand1 = 0;
            break;
        case ibis::qExpr::OP_GE:
            if (expr.leftBound() >= expr.rightBound()) {
                if (bin1 >= nobs) {
                    cand0 = cand1 = 0;
                }
                else if (expr.leftBound() >= minval[bin0] &&
                         expr.leftBound() <= maxval[bin0]) {
                    cand0 = bin0;
                    cand1 = bin0 + 1;
                }
                else {
                    cand0 = cand1 = 0;
                }
            }
            else {
                cand0 = cand1 = 0;
            }
            break;
        case ibis::qExpr::OP_EQ:
            if (expr.leftBound() == expr.rightBound()) {
                if (bin1 >= nobs) {
                    cand0 = cand1 = 0;
                }
                else if (expr.rightBound() <= maxval[bin1] &&
                         expr.rightBound() >= minval[bin1]) {
                    cand0 = bin1;
                    cand1 = bin1 + 1;
                }
                else {
                    cand0 = cand1 = 0;
                }
            }
            else {
                cand0 = cand1 = 0;
            }
            break;
        } // switch (expr.rightOperator())
        break; // case ibis::qExpr::OP_EQ
    } // switch (expr.leftOperator())
    if (ibis::gVerbose > 3) {
        std::ostringstream ostr;
        ostr << expr;
        double lc = (cand0 < nobs ?
                     (minval[cand0] < bounds[cand0] ?
                      minval[cand0] : bounds[cand0]) :
                     maxval.back());
        double uc = (cand1 <= nobs ?
                     (cand1 > cand0 ?
                      (maxval[cand1-1] < bounds[cand1-1]?
                       maxval[cand1-1] : bounds[cand1-1]) :
                      (cand0 < nobs ?
                       (maxval[cand0] < bounds[cand0] ?
                        maxval[cand0] : bounds[cand0]) :
                       bounds.back())) :
                     bounds.back());
        LOGGER(ibis::gVerbose > 0)
            << "bin::locate -- expr(" << ostr.str() << ") -> [" << cand0 << ", "
            << cand1 << ") (" << lc << ", " << uc << ")";
    }
} // locate cand0 and cand1

/// Locate the bins for all candidates and hits.
void ibis::bin::locate(const ibis::qContinuousRange& expr,
                       uint32_t& cand0, uint32_t& cand1,
                       uint32_t& hit0, uint32_t& hit1) const {
    uint32_t bin0 = (expr.leftOperator()!=ibis::qExpr::OP_UNDEFINED) ?
        locate(expr.leftBound()) : 0;
    uint32_t bin1 = (expr.rightOperator()!=ibis::qExpr::OP_UNDEFINED) ?
        locate(expr.rightBound()) : 0;
    switch (expr.leftOperator()) {
    default:
    case ibis::qExpr::OP_UNDEFINED:
        switch (expr.rightOperator()) {
        default:
        case ibis::qExpr::OP_UNDEFINED:
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- bin::locate encountered an ill-formed range "
                "condition";
            return;
        case ibis::qExpr::OP_LT:
            hit0 = 0;
            cand0 = 0;
            if (bin1 >= nobs) {
                hit1 = nobs;
                cand1 = nobs;
            }
            else if (expr.rightBound() > maxval[bin1]) {
                hit1 = bin1 + 1;
                cand1 = bin1 + 1;
            }
            else if (expr.rightBound() <= minval[bin1]) {
                hit1 = bin1;
                cand1 = bin1;
            }
            else {
                hit1 = bin1;
                cand1 = bin1 + 1;
            }
            break;
        case ibis::qExpr::OP_LE:
            hit0 = 0;
            cand0 = 0;
            if (bin1 >= nobs) {
                hit1 = nobs;
                cand1 = nobs;
            }
            else if (expr.rightBound() >= maxval[bin1]) {
                hit1 = bin1 + 1;
                cand1 = bin1 + 1;
            }
            else if (expr.rightBound() < minval[bin1]) {
                hit1 = bin1;
                cand1 = bin1;
            }
            else {
                hit1 = bin1;
                cand1 = bin1 + 1;
            }
            break;
        case ibis::qExpr::OP_GT:
            hit1 = nobs;
            cand1 = nobs;
            if (bin1 >= nobs) {
                hit0 = nobs;
                cand0 = nobs;
            }
            else if (expr.rightBound() >= maxval[bin1]) {
                hit0 = bin1 + 1;
                cand0 = bin1 + 1;
            }
            else if (expr.rightBound() < minval[bin1]) {
                hit0 = bin1;
                cand0 = bin1;
            }
            else {
                hit0 = bin1 + 1;
                cand0 = bin1;
            }
            break;
        case ibis::qExpr::OP_GE:
            hit1 = nobs;
            cand1 = nobs;
            if (bin1 >= nobs) {
                hit0 = nobs;
                cand0 = nobs;
            }
            else if (expr.rightBound() > maxval[bin1]) {
                hit0 = bin1 + 1;
                cand0 = bin1 + 1;
            }
            else if (expr.rightBound() <= minval[bin1]) {
                hit0 = bin1;
                cand0 = bin1;
            }
            else {
                hit0 = bin1 + 1;
                cand0 = bin1;
            }
            break;
        case ibis::qExpr::OP_EQ:
            if (bin1 >= nobs) {
                hit0 = 0;
                hit1 = 0;
                cand0 = 0;
                cand1 = 0;
            }
            else if (expr.rightBound() <= maxval[bin1] &&
                     expr.rightBound() >= minval[bin1]) {
                hit0 = bin1;
                hit1 = bin1;
                cand0 = bin1;
                cand1 = bin1 + 1;
                if (maxval[bin1] == minval[bin1])
                    hit1 = cand1;
            }
            else {
                hit0 = 0;
                hit1 = 0;
                cand0 = 0;
                cand1 = 0;
            }
            break;
        } // switch (expr.rightOperator())
        break; // case ibis::qExpr::OP_UNDEFINED
    case ibis::qExpr::OP_LT:
        if (bin0 >= nobs) {
            hit0 = nobs;
            cand0 = nobs;
        }
        else if (expr.leftBound() >= maxval[bin0]) {
            hit0 = bin0 + 1;
            cand0 = bin0 + 1;
        }
        else if (expr.leftBound() < minval[bin0]) {
            hit0 = bin0;
            cand0 = bin0;
        }
        else {
            hit0 = bin0 + 1;
            cand0 = bin0;
        }
        switch (expr.rightOperator()) {
        default:
        case ibis::qExpr::OP_UNDEFINED:
            hit1 = nobs;
            cand1 = nobs;
            break;
        case ibis::qExpr::OP_LT:
            if (bin1 >= nobs) {
                hit1 = nobs;
                cand1 = nobs;
            }
            else if (expr.rightBound() > maxval[bin1]) {
                hit1 = bin1 + 1;
                cand1 = bin1 + 1;
            }
            else if (expr.rightBound() <= minval[bin1]) {
                hit1 = bin1;
                cand1 = bin1;
            }
            else {
                hit1 = bin1;
                cand1 = bin1 + 1;
            }
            break;
        case ibis::qExpr::OP_LE:
            if (bin1 >= nobs) {
                hit1 = nobs;
                cand1 = nobs;
            }
            else if (expr.rightBound() >= maxval[bin1]) {
                hit1 = bin1 + 1;
                cand1 = bin1 + 1;
            }
            else if (expr.rightBound() < minval[bin1]) {
                hit1 = bin1;
                cand1 = bin1;
            }
            else {
                hit1 = bin1;
                cand1 = bin1 + 1;
            }
            break;
        case ibis::qExpr::OP_GT:
            if (expr.rightBound() > expr.leftBound()) {
                if (bin1 >= nobs) {
                    hit0 = nobs;
                    cand0 = nobs;
                }
                else if (expr.rightBound() >= maxval[bin1]) {
                    hit0 = bin1 + 1;
                    cand0 = bin1 + 1;
                }
                else if (expr.rightBound() < minval[bin1]) {
                    hit0 = bin1;
                    cand0 = bin1;
                }
                else {
                    hit0 = bin1 + 1;
                    cand0 = bin1;
                }
            }
            hit1 = nobs;
            cand1 = nobs;
            break;
        case ibis::qExpr::OP_GE:
            if (expr.rightBound() > expr.leftBound()) {
                if (bin1 >= nobs) {
                    hit0 = nobs;
                    cand0 = nobs;
                }
                else if (expr.rightBound() > maxval[bin1]) {
                    hit0 = bin1 + 1;
                    cand0 = bin1 + 1;
                }
                else if (expr.rightBound() <= minval[bin1]) {
                    hit0 = bin1;
                    cand0 = bin1;
                }
                else {
                    hit0 = bin1 + 1;
                    cand0 = bin1;
                }
            }
            hit1 = nobs;
            cand1 = nobs;
            break;
        case ibis::qExpr::OP_EQ:
            if (expr.rightBound() < expr.leftBound()) {
                if (bin1 >= nobs) {
                    hit0 = 0;
                    hit1 = 0;
                    cand0 = 0;
                    cand1 = 0;
                }
                else if (expr.rightBound() <= maxval[bin1] &&
                         expr.rightBound() >= minval[bin1]) {
                    hit0 = bin1;
                    hit1 = bin1;
                    cand0 = bin1;
                    cand1 = bin1 + 1;
                    if (maxval[bin1] == minval[bin1])
                        hit1 = cand1;
                }
                else {
                    hit0 = 0;
                    hit1 = 0;
                    cand0 = 0;
                    cand1 = 0;
                }
            }
            else {
                hit0 = 0;
                hit1 = 0;
                cand0 = 0;
                cand1 = 0;
            }
            break;
        } // switch (expr.rightOperator())
        break; // case ibis::qExpr::OP_LT
    case ibis::qExpr::OP_LE:
        if (bin0 >= nobs) {
            hit0 = nobs;
            cand0 = nobs;
        }
        else if (expr.leftBound() > maxval[bin0]) {
            hit0 = bin0 + 1;
            cand0 = bin0 + 1;
        }
        else if (expr.leftBound() <= minval[bin0]) {
            hit0 = bin0;
            cand0 = bin0;
        }
        else {
            hit0 = bin0 + 1;
            cand0 = bin0;
        }
        switch (expr.rightOperator()) {
        default:
        case ibis::qExpr::OP_UNDEFINED:
            hit1 = nobs;
            cand1 = nobs;
            break;
        case ibis::qExpr::OP_LT:
            if (bin1 >= nobs) {
                hit1 = nobs;
                cand1 = nobs;
            }
            else if (expr.rightBound() > maxval[bin1]) {
                hit1 = bin1 + 1;
                cand1 = bin1 + 1;
            }
            else if (expr.rightBound() <= minval[bin1]) {
                hit1 = bin1;
                cand1 = bin1;
            }
            else {
                hit1 = bin1;
                cand1 = bin1 + 1;
            }
            break;
        case ibis::qExpr::OP_LE:
            if (bin1 >= nobs) {
                hit1 = nobs;
                cand1 = nobs;
            }
            else if (expr.rightBound() >= maxval[bin1]) {
                hit1 = bin1 + 1;
                cand1 = bin1 + 1;
            }
            else if (expr.rightBound() < minval[bin1]) {
                hit1 = bin1;
                cand1 = bin1;
            }
            else {
                hit1 = bin1;
                cand1 = bin1 + 1;
            }
            break;
        case ibis::qExpr::OP_GT:
            if (expr.rightBound() > expr.leftBound()) {
                if (bin1 >= nobs) {
                    hit0 = nobs;
                    cand0 = nobs;
                }
                else if (expr.rightBound() >= maxval[bin1]) {
                    hit0 = bin1 + 1;
                    cand0 = bin1 + 1;
                }
                else if (expr.rightBound() < minval[bin1]) {
                    hit0 = bin1;
                    cand0 = bin1;
                }
                else {
                    hit0 = bin1 + 1;
                    cand0 = bin1;
                }
            }
            hit1 = nobs;
            cand1 = nobs;
            break;
        case ibis::qExpr::OP_GE:
            if (expr.rightBound() > expr.leftBound()) {
                if (bin1 >= nobs) {
                    hit0 = nobs;
                    cand0 = nobs;
                }
                else if (expr.rightBound() > maxval[bin1]) {
                    hit0 = bin1 + 1;
                    cand0 = bin1 + 1;
                }
                else if (expr.rightBound() <= minval[bin1]) {
                    hit0 = bin1;
                    cand0 = bin1;
                }
                else {
                    hit0 = bin1 + 1;
                    cand0 = bin1;
                }
            }
            hit1 = nobs;
            cand1 = nobs;
            break;
        case ibis::qExpr::OP_EQ:
            if (expr.rightBound() <= expr.leftBound()) {
                if (bin1 >= nobs) {
                    hit0 = 0;
                    hit1 = 0;
                    cand0 = 0;
                    cand1 = 0;
                }
                else if (expr.rightBound() <= maxval[bin1] &&
                         expr.rightBound() >= minval[bin1]) {
                    hit0 = bin1;
                    hit1 = bin1;
                    cand0 = bin1;
                    cand1 = bin1 + 1;
                    if (maxval[bin1] == minval[bin1])
                        hit1 = cand1;
                }
                else {
                    hit0 = 0;
                    hit1 = 0;
                    cand0 = 0;
                    cand1 = 0;
                }
            }
            else {
                hit0 = 0;
                hit1 = 0;
                cand0 = 0;
                cand1 = 0;
            }
            break;
        } // switch (expr.rightOperator())
        break; // case ibis::qExpr::OP_LE
    case ibis::qExpr::OP_GT:
        if (bin0 >= nobs) {
            hit1 = nobs;
            cand1 = nobs;
        }
        else if (expr.leftBound() > maxval[bin0]) {
            hit1 = bin0 + 1;
            cand1 = bin0 + 1;
        }
        else if (expr.leftBound() <= minval[bin0]) {
            hit1 = bin0;
            cand1 = bin0;
        }
        else {
            hit1 = bin0;
            cand1 = bin0 + 1;
        }
        switch (expr.rightOperator()) {
        default:
        case ibis::qExpr::OP_UNDEFINED:
            cand0 = 0;
            hit0 = 0;
            break;
        case ibis::qExpr::OP_LT:
            hit0 = 0;
            cand0 = 0;
            if (expr.rightBound() < expr.leftBound()) {
                if (expr.rightBound() > maxval[bin1]) {
                    hit1 = bin1 + 1;
                    cand1 = bin1 + 1;
                }
                else if (expr.rightBound() <= minval[bin1]) {
                    hit1 = bin1;
                    cand1 = bin1;
                }
                else {
                    hit1 = bin1;
                    cand1 = bin1 + 1;
                }
            }
            break;
        case ibis::qExpr::OP_LE:
            hit0 = 0;
            cand0 = 0;
            if (expr.rightBound() < expr.leftBound()) {
                if (bin1 >= nobs) {
                    hit1 = nobs;
                    cand1 = nobs;
                }
                else if (expr.rightBound() >= maxval[bin1]) {
                    hit1 = bin1 + 1;
                    cand1 = bin1 + 1;
                }
                else if (expr.rightBound() < minval[bin1]) {
                    hit1 = bin1;
                    cand1 = bin1;
                }
                else {
                    hit1 = bin1;
                    cand1 = bin1 + 1;
                }
            }
            break;
        case ibis::qExpr::OP_GT:
            if (bin1 >= nobs) {
                hit0 = nobs;
                cand0 = nobs;
            }
            else if (expr.rightBound() >= maxval[bin1]) {
                hit0 = bin1 + 1;
                cand0 = bin1 + 1;
            }
            else if (expr.rightBound() < minval[bin1]) {
                hit0 = bin1;
                cand0 = bin1;
            }
            else {
                hit0 = bin1 + 1;
                cand0 = bin1;
            }
            break;
        case ibis::qExpr::OP_GE:
            if (bin1 >= nobs) {
                hit0 = nobs;
                cand0 = nobs;
            }
            else if (expr.rightBound() > maxval[bin1]) {
                hit0 = bin1 + 1;
                cand0 = bin1 + 1;
            }
            else if (expr.rightBound() <= minval[bin1]) {
                hit0 = bin1;
                cand0 = bin1;
            }
            else {
                hit0 = bin1 + 1;
                cand0 = bin1;
            }
            break;
        case ibis::qExpr::OP_EQ:
            if (expr.rightBound() < expr.leftBound()) {
                if (bin1 >= nobs) {
                    hit0 = 0;
                    hit1 = 0;
                    cand0 = 0;
                    cand1 = 0;
                }
                else if (expr.rightBound() <= maxval[bin1] &&
                         expr.rightBound() >= minval[bin1]) {
                    hit0 = bin1;
                    hit1 = bin1;
                    cand0 = bin1;
                    cand1 = bin1 + 1;
                    if (maxval[bin1] == minval[bin1])
                        hit1 = cand1;
                }
                else {
                    hit0 = 0;
                    hit1 = 0;
                    cand0 = 0;
                    cand1 = 0;
                }
            }
            else {
                hit0 = 0;
                hit1 = 0;
                cand0 = 0;
                cand1 = 0;
            }
            break;
        } // switch (expr.rightOperator())
        break; // case ibis::qExpr::OP_GT
    case ibis::qExpr::OP_GE:
        if (bin0 >= nobs) {
            hit1 = nobs;
            cand1 = nobs;
        }
        else if (expr.leftBound() >= maxval[bin0]) {
            hit1 = bin0 + 1;
            cand1 = bin0 + 1;
        }
        else if (expr.leftBound() < minval[bin0]) {
            hit1 = bin0;
            cand1 = bin0;
        }
        else {
            hit1 = bin0;
            cand1 = bin0 + 1;
        }
        switch (expr.rightOperator()) {
        default:
        case ibis::qExpr::OP_UNDEFINED:
            hit0 = 0; cand0 = 0;
            break;
        case ibis::qExpr::OP_LT:
            hit0 = 0;
            cand0 = 0;
            if (expr.rightBound() <= expr.leftBound()) {
                if (bin1 >= nobs) {
                    hit1 = nobs;
                    cand1 = nobs;
                }
                else if (expr.rightBound() > maxval[bin1]) {
                    hit1 = bin1 + 1;
                    cand1 = bin1 + 1;
                }
                else if (expr.rightBound() <= minval[bin1]) {
                    hit1 = bin1;
                    cand1 = bin1;
                }
                else {
                    hit1 = bin1;
                    cand1 = bin1 + 1;
                }
            }
            break;
        case ibis::qExpr::OP_LE:
            hit0 = 0;
            cand0 = 0;
            if (expr.rightBound() < expr.leftBound()) {
                if (expr.rightBound() >= maxval[bin1]) {
                    hit1 = bin1 + 1;
                    cand1 = bin1 + 1;
                }
                else if (expr.rightBound() < minval[bin1]) {
                    hit1 = bin1;
                    cand1 = bin1;
                }
                else {
                    hit1 = bin1;
                    cand1 = bin1 + 1;
                }
            }
            break;
        case ibis::qExpr::OP_GT:
            if (bin1 >= nobs) {
                hit0 = nobs;
                cand0 = nobs;
            }
            else if (expr.rightBound() > maxval[bin1]) {
                hit0 = bin1 + 1;
                cand0 = bin1 + 1;
            }
            else if (expr.rightBound() <= minval[bin1]) {
                hit0 = bin1;
                cand0 = bin1;
            }
            else {
                hit0 = bin1 + 1;
                cand0 = bin1;
            }
            break;
        case ibis::qExpr::OP_GE:
            if (bin1 >= nobs) {
                hit0 = nobs;
                cand0 = nobs;
            }
            else if (expr.rightBound() > maxval[bin1]) {
                hit0 = bin1 + 1;
                cand0 = bin1 + 1;
            }
            else if (expr.rightBound() <= minval[bin1]) {
                hit0 = bin1;
                cand0 = bin1;
            }
            else {
                hit0 = bin1 + 1;
                cand0 = bin1;
            }
            break;
        case ibis::qExpr::OP_EQ:
            if (expr.rightBound() <= expr.leftBound()) {
                if (bin1 >= nobs) {
                    hit0 = 0;
                    hit1 = 0;
                    cand0 = 0;
                    cand1 = 0;
                }
                else if (expr.rightBound() <= maxval[bin1] &&
                         expr.rightBound() >= minval[bin1]) {
                    hit0 = bin1;
                    hit1 = bin1;
                    cand0 = bin1;
                    cand1 = bin1 + 1;
                    if (maxval[bin1] == minval[bin1])
                        hit1 = cand1;
                }
                else {
                    hit0 = 0;
                    hit1 = 0;
                    cand0 = 0;
                    cand1 = 0;
                }
            }
            else {
                hit0 = 0;
                hit1 = 0;
                cand0 = 0;
                cand1 = 0;
            }
            break;
        } // switch (expr.rightOperator())
        break; // case ibis::qExpr::OP_GE
    case ibis::qExpr::OP_EQ:
        switch (expr.rightOperator()) {
        default:
        case ibis::qExpr::OP_UNDEFINED:
            if (bin0 >= nobs) {
                hit0 = 0;
                hit1 = 0;
                cand0 = 0;
                cand1 = 0;
            }
            else if (expr.leftBound() <= maxval[bin0] &&
                     expr.leftBound() >= minval[bin0]) {
                hit0 = bin0;
                hit1 = bin0;
                cand0 = bin0;
                cand1 = bin0 + 1;
                if (maxval[bin0] == minval[bin0])
                    hit1 = cand1;
            }
            else {
                hit0 = 0;
                hit1 = 0;
                cand0 = 0;
                cand1 = 0;
            }
            break;
        case ibis::qExpr::OP_LT:
            if (expr.leftBound() < expr.rightBound()) {
                if (bin1 >= nobs) {
                    hit0 = 0;
                    hit1 = 0;
                    cand0 = 0;
                    cand1 = 0;
                }
                else if (expr.leftBound() >= minval[bin0] &&
                         expr.leftBound() <= maxval[bin0]) {
                    hit0 = bin0;
                    hit1 = bin0;
                    cand0 = bin0;
                    cand1 = bin0 + 1;
                    if (maxval[bin0] == minval[bin0])
                        hit1 = cand1;
                }
                else {
                    hit0 = 0;
                    hit1 = 0;
                    cand0 = 0;
                    cand1 = 0;
                }
            }
            else {
                hit0 = 0;
                hit1 = 0;
                cand0 = 0;
                cand1 = 0;
            }
            break;
        case ibis::qExpr::OP_LE:
            if (expr.leftBound() <= expr.rightBound()) {
                if (bin1 >= nobs) {
                    hit0 =  hit1 = cand0 = cand1 = 0;
                }
                else if (expr.leftBound() >= minval[bin0] &&
                         expr.leftBound() <= maxval[bin0]) {
                    hit0 = bin0;
                    hit1 = bin0;
                    cand0 = bin0;
                    cand1 = bin0 + 1;
                    if (maxval[bin0] == minval[bin0])
                        hit1 = cand1;
                }
                else {
                    hit0 = 0;
                    hit1 = 0;
                    cand0 = 0;
                    cand1 = 0;
                }
            }
            else {
                hit0 = 0;
                hit1 = 0;
                cand0 = 0;
                cand1 = 0;
            }
            break;
        case ibis::qExpr::OP_GT:
            if (expr.leftBound() > expr.rightBound()) {
                if (bin1 >= nobs) {
                }
                else if (expr.leftBound() >= minval[bin0] &&
                         expr.leftBound() <= maxval[bin0]) {
                    hit0 = bin0;
                    hit1 = bin0;
                    cand0 = bin0;
                    cand1 = bin0 + 1;
                    if (maxval[bin0] == minval[bin0])
                        hit1 = cand1;
                }
                else {
                    hit0 = 0;
                    hit1 = 0;
                    cand0 = 0;
                    cand1 = 0;
                }
            }
            else {
                hit0 = 0;
                hit1 = 0;
                cand0 = 0;
                cand1 = 0;
            }
            break;
        case ibis::qExpr::OP_GE:
            if (expr.leftBound() >= expr.rightBound()) {
                if (bin1 >= nobs) {
                    hit0 =  hit1 = cand0 = cand1 = 0;
                }
                else if (expr.leftBound() >= minval[bin0] &&
                         expr.leftBound() <= maxval[bin0]) {
                    hit0 = bin0;
                    hit1 = bin0;
                    cand0 = bin0;
                    cand1 = bin0 + 1;
                    if (maxval[bin0] == minval[bin0])
                        hit1 = cand1;
                }
                else {
                    hit0 = 0;
                    hit1 = 0;
                    cand0 = 0;
                    cand1 = 0;
                }
            }
            else {
                hit0 = 0;
                hit1 = 0;
                cand0 = 0;
                cand1 = 0;
            }
            break;
        case ibis::qExpr::OP_EQ:
            if (expr.leftBound() == expr.rightBound()) {
                if (bin1 >= nobs) {
                    hit0 =  hit1 = cand0 = cand1 = 0;
                }
                else if (expr.rightBound() <= maxval[bin1] &&
                         expr.rightBound() >= minval[bin1]) {
                    hit0 = bin1;
                    hit1 = bin1;
                    cand0 = bin1;
                    cand1 = bin1 + 1;
                    if (maxval[bin0] == minval[bin0])
                        hit1 = cand1;
                }
                else {
                    hit0 = 0;
                    hit1 = 0;
                    cand0 = 0;
                    cand1 = 0;
                }
            }
            else {
                hit0 = 0;
                hit1 = 0;
                cand0 = 0;
                cand1 = 0;
            }
            break;
        } // switch (expr.rightOperator())
        break; // case ibis::qExpr::OP_EQ
    } // switch (expr.leftOperator())
    if (ibis::gVerbose > 3) {
        std::ostringstream ostr;
        ostr << expr;
        double lc = (cand0 < nobs ?
                     (minval[cand0] < bounds[cand0] ?
                      minval[cand0] : bounds[cand0]) :
                     maxval.back());
        double lh = (hit0 < nobs ?
                     (minval[hit0]<bounds[hit0] ?
                      minval[hit0] : bounds[hit0]) :
                     bounds.back());
        double uh = (hit1 <= nobs ?
                     (hit1 > hit0 ?
                      (maxval[hit1-1] < bounds[hit1-1]?
                       maxval[hit1-1] : bounds[hit1-1]) :
                      (hit0 < nobs ?
                       (maxval[hit0] < bounds[hit0] ?
                        maxval[hit0] : bounds[hit0]) :
                       maxval.back())) :
                     maxval.back());
        double uc = (cand1 <= nobs ?
                     (cand1 > cand0 ?
                      (maxval[cand1-1] < bounds[cand1-1]?
                       maxval[cand1-1] : bounds[cand1-1]) :
                      (cand0 < nobs ?
                       (maxval[cand0] < bounds[cand0] ?
                        maxval[cand0] : bounds[cand0]) :
                       bounds.back())) :
                     bounds.back());
        LOGGER(ibis::gVerbose > 0)
            << "bin::locate -- expr(" << ostr.str() << ") -> [" << cand0
            << ':' << hit0 << ", " << hit1 << ':' << cand1 << ") (" << lc
            << ':' << lh << ", " << uh << ':' << uc << ')';
    }
} // locate cand0, cand1, hit0, and hit1

/// Compute the actual minimum value from the binned index.
double ibis::bin::getMin() const {
    double ret = DBL_MAX;
    for (uint32_t i = 0; i < nobs; ++ i)
        if (ret > minval[i]) {
            ret = minval[i];
            return ret;
        }
    return ret;
} // ibis::bin::getMin

/// Compute the actual maximum value from the binned index.
double ibis::bin::getMax() const {
    double ret = -DBL_MAX;
    for (uint32_t i = nobs; i > 0; ) {
        -- i;
        if (ret < maxval[i]) {
            ret = maxval[i];
            return ret;
        }
    }
    return ret;
} // ibis::bin::getMax

/// Compute the approximate value of the sum from the binned index.
double ibis::bin::getSum() const {
    double ret;
    bool here = true;
    if (col != 0) {
        const size_t nbv = col->elementSize()*nrows;
        if (str != 0)
            here = (str->bytes() < nbv);
        else if (offset64.size() > nobs)
            here = (static_cast<size_t>(offset64[nobs]) < nbv);
        else if (offset32.size() > nobs)
            here = (static_cast<uint32_t>(offset32[nobs]) < nbv);
    }
    else {
        here = false;
    }
    if (here) {
        ret = computeSum();
    }
    else { // indicate sum is not computed
        ibis::util::setNaN(ret);
    }
    return ret;
} // ibis::bin::getSum

/// Compute the approximate sum of all values using the binned index.
double ibis::bin::computeSum() const {
    double sum = 0;
    activate(); // need to activate all bitvectors
    for (uint32_t i = 0; i < nobs; ++ i)
        if (minval[i] <= maxval[i] && bits[i] != 0)
            sum += 0.5 * (minval[i] + maxval[i]) * bits[i]->cnt();
    return sum;
} // ibis::bin::computeSum

/// Compute the cumulative distribution from the binned index.
long ibis::bin::getCumulativeDistribution(std::vector<double>& bds,
                                          std::vector<uint32_t>& cts) const {
    bds.clear();
    cts.clear();
    long ierr = 0;
    binBoundaries(bds);
    if (bds.size() > 1) {
        binWeights(cts);
        if (bds.size() == cts.size()) {
            ierr = bds.size();
            // convert to cumulative distribution
            for (int i = 1; i < ierr; ++ i)
                cts[i] += cts[i-1];
            if (cts[ierr-1] <= cts[ierr-2]) {
                bds.resize(ierr - 1);
                cts.resize(ierr - 1);
                ierr = ierr - 1;
            }
            // if (bds.back() == DBL_MAX) {
            //     double tmp = col->upperBound();
            //     if (tmp < bds[ierr-2])
            //         tmp = bds[ierr-2];
            //     bds.back() = ibis::util::compactValue(tmp, tmp+tmp);
            // }
        }
        else {
            // don't match, delete the content
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- bin::getCumulativeDistribution received "
                "inconsistent results: bds[" << bds.size() << "] and cts["
                << cts.size() << "] have different sizes "
                "-- clearing these arrays";
#if DEBUG+0 > 1 || _DEBUG+0 > 1
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
            << "Warning -- bin::getCumulativeDistribution can not detrmine any "
            "bin boundaries";
        bds.clear();
        cts.clear();
        ierr = -1;
    }
    return ierr;
} // ibis::bin::getCumulativeDistribution

/// Compute a histogram from the binned index.
long ibis::bin::getDistribution(std::vector<double>& bds,
                                std::vector<uint32_t>& cts) const {
    bds.clear();
    cts.clear();
    long ierr = 0;
    binBoundaries(bds);
    if (bds.size() > 1) {
        binWeights(cts);
        if (bds.size() == cts.size()) {
            bds.resize(bds.size()-1); // remove the last value
            ierr = cts.size();
        }
        else {
            // don't match, delete the content
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- bin::getDistribution encountered an "
                "inconsistency: bds[" << bds.size() << "] and cts["
                << cts.size() << "] have different sizes -- clearing arrays";
#if DEBUG+0 > 1 || _DEBUG+0 > 1
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
            << "Warning -- bin::getDistribution can not determine any bin "
            "boundaries";
        bds.clear();
        cts.clear();
        ierr = -1;
    }
    return ierr;
} // ibis::bin::getDistribution

/// Evaluate the range join condition using the ibis::bin index.  Record
/// the definite hits in @c lower, and all possible hits in @c upper.
/// NOTE: @c upper includes all entries in @c lower.
void ibis::bin::estimate(const ibis::deprecatedJoin& expr,
                         ibis::bitvector64& lower,
                         ibis::bitvector64& upper) const {
    lower.clear();
    upper.clear();

    ibis::horometer timer;
    timer.start();
    activate();         // activate all bitvectors
    // Internally, upper stores the paires that can not be decided using
    // this index.  It is ORed with lower before exiting from this
    // function.
    if (expr.getRange() == 0) {
        equiJoin(lower, upper);
    }
    else if (expr.getRange()->termType() == ibis::math::NUMBER) {
        const double delta = fabs(expr.getRange()->eval());
        if (delta == 0.0) {
            equiJoin(lower, upper);
        }
        else {
            deprecatedJoin(delta, lower, upper);
        }
    }
    else {
        compJoin(expr.getRange(), lower, upper);
    }

    if (lower.size() != lower.size()) {
        if (lower.size() > 0)
            upper.set(0, lower.size());
        else
            lower.set(0, upper.size());
    }
    if (lower.size() == upper.size())
        upper |= lower;
    if (ibis::gVerbose > 1) {
        timer.stop();
        std::ostringstream ostr;
        ostr << expr << " produced [" << lower.cnt() << ", "
             << (upper.cnt()>=lower.cnt() ? upper.cnt() : lower.cnt())
             << "] hit(s)";
        ibis::util::logMessage
            ("bin::estimate(symmetric)",
             "processing %s took %g sec(CPU), %g sec(elapsed)",
             ostr.str().c_str(), timer.CPUTime(), timer.realTime());
    }
} // ibis::bin::estimate

void ibis::bin::estimate(const ibis::deprecatedJoin& expr,
                         const ibis::bitvector& mask,
                         ibis::bitvector64& lower,
                         ibis::bitvector64& upper) const {
    lower.clear();
    upper.clear();

    ibis::horometer timer;
    timer.start();
    activate();         // activate all bitvectors
    // Internally, upper stores the paires that can not be decided using
    // this index.  It is ORed with lower before exiting from this
    // function.
    if (expr.getRange() == 0) {
        equiJoin(mask, lower, upper);
    }
    else if (expr.getRange()->termType() == ibis::math::NUMBER) {
        const double delta = fabs(expr.getRange()->eval());
        if (delta == 0.0) {
            equiJoin(mask, lower, upper);
        }
        else {
            deprecatedJoin(delta, mask, lower, upper);
        }
    }
    else {
        compJoin(expr.getRange(), mask, lower, upper);
    }

    if (lower.size() != lower.size()) {
        if (lower.size() > 0)
            upper.set(0, lower.size());
        else
            lower.set(0, upper.size());
    }
    if (lower.size() == upper.size())
        upper |= lower;
    if (ibis::gVerbose > 1) {
        timer.stop();
        std::ostringstream ostr;
        ostr << expr << " produced [" << lower.cnt() << ", "
             << (upper.cnt()>=lower.cnt() ? upper.cnt() : lower.cnt())
             << "] hit(s)";
        ibis::util::logMessage
            ("bin::estimate(symmetric)",
             "processing %s took %g sec(CPU), %g sec(elapsed)",
             ostr.str().c_str(), timer.CPUTime(), timer.realTime());
    }
} // ibis::bin::estimate

void ibis::bin::estimate(const ibis::bin& idx2,
                         const ibis::deprecatedJoin& expr,
                         ibis::bitvector64& lower,
                         ibis::bitvector64& upper) const {
    lower.clear();
    upper.clear();
    if (col == 0 || idx2.col == 0) return; // nothing can be done

    ibis::horometer timer;
    timer.start();
    activate();         // activate all bitvectors
    idx2.activate();    // activate all bitvectors
    // Internally, upper stores the paires that can not be decided using
    // this index.  It is ORed with lower before exiting from this
    // function.
    if (expr.getRange() == 0) {
        equiJoin(idx2, lower, upper);
    }
    else if (expr.getRange()->termType() == ibis::math::NUMBER) {
        const double delta = fabs(expr.getRange()->eval());
        if (delta == 0.0) {
            equiJoin(idx2, lower, upper);
        }
        else {
            deprecatedJoin(idx2, delta, lower, upper);
        }
    }
    else {
        compJoin(idx2, expr.getRange(), lower, upper);
    }

    if (lower.size() != lower.size()) {
        if (lower.size() > 0)
            upper.set(0, lower.size());
        else
            lower.set(0, upper.size());
    }
    if (lower.size() == upper.size())
        upper |= lower;
    if (ibis::gVerbose > 1) {
        timer.stop();
        std::ostringstream ostr;
        ostr << expr << " produced [" << lower.cnt() << ", "
             << (upper.cnt()>=lower.cnt() ? upper.cnt() : lower.cnt())
             << "] hit(s)";
        ibis::util::logMessage
            ("bin::estimate", "processing %s took %g sec(CPU), "
             "%g sec(elapsed)", ostr.str().c_str(),
             timer.CPUTime(), timer.realTime());
    }
} // ibis::bin::estimate

void ibis::bin::estimate(const ibis::bin& idx2,
                         const ibis::deprecatedJoin& expr,
                         const ibis::bitvector& mask,
                         ibis::bitvector64& lower,
                         ibis::bitvector64& upper) const {
    lower.clear();
    upper.clear();
    if (col == 0 || idx2.col == 0) return; // nothing can be done

    ibis::horometer timer;
    timer.start();
    activate();         // activate all bitvectors
    idx2.activate();    // activate all bitvectors
    // Internally, upper stores the paires that can not be decided using
    // this index.  It is ORed with lower before exiting from this
    // function.
    if (expr.getRange() == 0) {
        equiJoin(idx2, mask, lower, upper);
    }
    else if (expr.getRange()->termType() == ibis::math::NUMBER) {
        const double delta = fabs(expr.getRange()->eval());
        if (delta == 0.0) {
            equiJoin(idx2, mask, lower, upper);
        }
        else {
            deprecatedJoin(idx2, delta, mask, lower, upper);
        }
    }
    else {
        compJoin(idx2, expr.getRange(), mask, lower, upper);
    }

    if (lower.size() != lower.size()) {
        if (lower.size() > 0)
            upper.set(0, lower.size());
        else
            lower.set(0, upper.size());
    }
    if (lower.size() == upper.size())
        upper |= lower;
    if (ibis::gVerbose > 1) {
        timer.stop();
        std::ostringstream ostr;
        ostr << expr << " produced [" << lower.cnt() << ", "
             << (upper.cnt()>=lower.cnt() ? upper.cnt() : lower.cnt())
             << "] hit(s)";
        ibis::util::logMessage
            ("bin::estimate", "processing %s took %g sec(CPU), "
             "%g sec(elapsed)", ostr.str().c_str(),
             timer.CPUTime(), timer.realTime());
    }
} // ibis::bin::estimate

void ibis::bin::estimate(const ibis::bin& idx2,
                         const ibis::deprecatedJoin& expr,
                         const ibis::bitvector& mask,
                         const ibis::qRange* const range1,
                         const ibis::qRange* const range2,
                         ibis::bitvector64& lower,
                         ibis::bitvector64& upper) const {
    if (mask.cnt() == 0) {
        uint64_t nb = mask.size();
        nb *= nb;
        lower.set(0, nb);
        upper.clear();
        return;
    }
    if (range1 == 0 && range2 == 0) {
        estimate(idx2, expr, mask, lower, upper);
        return;
    }

    horometer timer;
    if (ibis::gVerbose > 1) 
        timer.start();
    if (expr.getRange() == 0) {
        equiJoin(idx2, mask, range1, range2, lower, upper);
    }
    else if (expr.getRange()->termType() == ibis::math::NUMBER) {
        double dlt = fabs(expr.getRange()->eval());
        if (dlt == 0.0)
            equiJoin(idx2, mask, range1, range2, lower, upper);
        else
            deprecatedJoin(idx2, dlt, mask, range1, range2, lower, upper);
    }
    else {
        compJoin(idx2, expr.getRange(), mask, range1, range2, lower, upper);
    }
    if (upper.size() == lower.size() && lower.size() > 0)
        upper |= lower;
    if (ibis::gVerbose > 1) {
        timer.stop();
        std::ostringstream ostr;
        ostr << expr << " with a mask (" << mask.cnt() << ")";
        if (range1) {
            if (range2)
                ostr << ", " << *range1 << ", and " << *range2;
            else
                ostr << " and " << *range1;
        }
        else if (range2) {
            ostr << " and " << *range2;
        }
        ostr << " produced number of hits between " << lower.cnt()
             << " and "
             << (upper.cnt() > lower.cnt() ? upper.cnt() : lower.cnt());
        ibis::util::logMessage("bin::estimate", "processing %s, took %g "
                               "sec(CPU), %g sec(elapsed)",
                               ostr.str().c_str(), timer.CPUTime(),
                               timer.realTime());
    }
} // ibis::bin::estimate

int64_t ibis::bin::estimate(const ibis::bin& idx2,
                            const ibis::deprecatedJoin& expr,
                            const ibis::bitvector& mask,
                            const ibis::qRange* const range1,
                            const ibis::qRange* const range2) const {
    int64_t cnt = 0;
    if (mask.cnt() == 0) {
        return cnt;
    }

    horometer timer;
    if (ibis::gVerbose > 1) 
        timer.start();
    if (expr.getRange() == 0) {
        cnt = equiJoin(idx2, mask, range1, range2);
    }
    else if (expr.getRange()->termType() == ibis::math::NUMBER) {
        double dlt = fabs(expr.getRange()->eval());
        if (dlt == 0.0)
            cnt = equiJoin(idx2, mask, range1, range2);
        else
            cnt = deprecatedJoin(idx2, dlt, mask, range1, range2);
    }
    else {
        cnt = compJoin(idx2, expr.getRange(), mask, range1, range2);
    }
    if (ibis::gVerbose > 1) {
        timer.stop();
        std::ostringstream ostr;
        ostr << expr << " with a mask (" << mask.cnt() << ")";
        if (range1) {
            if (range2)
                ostr << ", " << *range1 << ", and " << *range2;
            else
                ostr << " and " << *range1;
        }
        else if (range2) {
            ostr << " and " << *range2;
        }
        ostr << " produced no more than " << cnt
             << (cnt>1 ? " hits" : " hit");
        ibis::util::logMessage("bin::estimate", "processing %s, took %g "
                               "sec(CPU), %g sec(elapsed)",
                               ostr.str().c_str(), timer.CPUTime(),
                               timer.realTime());
    }
    return cnt;
} // ibis::bin::estimate

int64_t ibis::bin::estimate(const ibis::bin& idx2,
                            const ibis::deprecatedJoin& expr,
                            const ibis::bitvector& mask) const {
    return estimate(idx2, expr, mask, 0, 0);
} // ibis::bin::estimate

int64_t ibis::bin::estimate(const ibis::bin& idx2,
                            const ibis::deprecatedJoin& expr) const {
    ibis::bitvector mask;
    if (col != 0)
        col->getNullMask(mask);
    else
        mask.set(1, nrows);
    if (idx2.col) {
        ibis::bitvector tmp;
        idx2.col->getNullMask(tmp);
        mask &= tmp;
    }
    return estimate(idx2, expr, mask);
} // ibis::bin::estimate

void ibis::bin::estimate(const ibis::deprecatedJoin& expr,
                         const ibis::bitvector& mask,
                         const ibis::qRange* const range1,
                         const ibis::qRange* const range2,
                         ibis::bitvector64& lower,
                         ibis::bitvector64& upper) const {
    if (mask.cnt() == 0) {
        uint64_t nb = mask.size();
        nb *= nb;
        lower.set(0, nb);
        upper.clear();
        return;
    }
    if (range1 == 0 && range2 == 0) {
        estimate(expr, mask, lower, upper);
        return;
    }

    horometer timer;
    if (ibis::gVerbose > 1) 
        timer.start();
    if (expr.getRange() == 0) {
        equiJoin(mask, range1, range2, lower, upper);
    }
    else if (expr.getRange()->termType() == ibis::math::NUMBER) {
        double dlt = fabs(expr.getRange()->eval());
        if (dlt == 0.0)
            equiJoin(mask, range1, range2, lower, upper);
        else
            deprecatedJoin(dlt, mask, range1, range2, lower, upper);
    }
    else {
        compJoin(expr.getRange(), mask, range1, range2, lower, upper);
    }
    if (upper.size() == lower.size() && lower.size() > 0)
        upper |= lower;
    if (ibis::gVerbose > 1) {
        timer.stop();
        std::ostringstream ostr;
        ostr << expr << " with a mask (" << mask.cnt() << ")";
        if (range1) {
            if (range2)
                ostr << ", " << *range1 << ", and " << *range2;
            else
                ostr << " and " << *range1;
        }
        else if (range2) {
            ostr << " and " << *range2;
        }
        ostr << " produced number of hits between " << lower.cnt()
             << " and "
             << (upper.cnt() > lower.cnt() ? upper.cnt() : lower.cnt());
        ibis::util::logMessage("bin::estimate", "processing %s, took %g "
                               "sec(CPU), %g sec(elapsed)",
                               ostr.str().c_str(), timer.CPUTime(),
                               timer.realTime());
    }
} // ibis::bin::estimate

int64_t ibis::bin::estimate(const ibis::deprecatedJoin& expr,
                            const ibis::bitvector& mask,
                            const ibis::qRange* const range1,
                            const ibis::qRange* const range2) const {
    int64_t cnt = 0;
    if (mask.cnt() == 0) {
        return cnt;
    }

    horometer timer;
    if (ibis::gVerbose > 1) 
        timer.start();
    if (expr.getRange() == 0) {
        cnt = equiJoin(mask, range1, range2);
    }
    else if (expr.getRange()->termType() == ibis::math::NUMBER) {
        double dlt = fabs(expr.getRange()->eval());
        if (dlt == 0.0)
            cnt = equiJoin(mask, range1, range2);
        else
            cnt = deprecatedJoin(dlt, mask, range1, range2);
    }
    else {
        cnt = compJoin(expr.getRange(), mask, range1, range2);
    }
    if (ibis::gVerbose > 1) {
        timer.stop();
        std::ostringstream ostr;
        ostr << expr << " with a mask (" << mask.cnt() << ")";
        if (range1) {
            if (range2)
                ostr << ", " << *range1 << ", and " << *range2;
            else
                ostr << " and " << *range1;
        }
        else if (range2) {
            ostr << " and " << *range2;
        }
        ostr << " produced no more than " << cnt
             << (cnt>1 ? " hits" : " hit");
        ibis::util::logMessage("bin::estimate", "processing %s, took %g "
                               "sec(CPU), %g sec(elapsed)",
                               ostr.str().c_str(), timer.CPUTime(),
                               timer.realTime());
    }
    return cnt;
} // ibis::bin::estimate

/// An equi-join on the same variable and using the same index.
void ibis::bin::equiJoin(ibis::bitvector64& sure,
                         ibis::bitvector64& iffy) const {
    LOGGER(ibis::gVerbose > 3)
        << "bin::equiJoin starts to process an equi-join between "
        << (col ? col->fullname() : "?.?") << " and "
        << (col ? col->fullname() : "?.?");

    uint32_t il1=0, il2=0, iu2=0;
    uint32_t ilc=0, iuc=0; // bits[ilc:iuc] is summed in cumu
    ibis::bitvector cumu;
    uint32_t tlast = time(0);
    while (il1 < nobs && il2 < nobs) {
        while (il1 < nobs && il2 < nobs &&
               !(maxval[il1] >= minval[il1] &&
                 maxval[il2] >= minval[il2] &&
                 maxval[il1] >= minval[il2] &&
                 maxval[il2] >= minval[il1])) {
            if (maxval[il1] >= minval[il1] &&
                maxval[il2] >= minval[il2]) {
                if (!(maxval[il1] >= minval[il2])) ++ il1;
                else ++ il2;
            }
            else {
                if (!(maxval[il1] >= minval[il1])) ++ il1;
                if (!(maxval[il2] >= minval[il2])) ++ il2;
            }
        }

        if (bits[il1] != 0 && bits[il1]->cnt() > 0 &&
            il1 < nobs && il2 < nobs) { // found some overlap
            if (minval[il1] == maxval[il1] &&
                minval[il1] == minval[il2] &&
                minval[il1] == maxval[il2] ) {
                // exactly equal, record the pairs in @c sure
                ibis::util::outerProduct
                    (*(bits[il1]), *(bits[il2]), sure);
            }
            else { // not exactly sure, put the result in iffy
                // bins [il2, iu2) from idx2 overlaps with bin il1 of *this
                for (iu2 = il2+1; iu2 < nobs &&
                         minval[iu2] <= maxval[il1]; ++ iu2);
                sumBins(il2, iu2, cumu, ilc, iuc);
                ibis::util::outerProduct(*(bits[il1]), cumu, iffy);
                ilc = il2;
                iuc = iu2;
            }
        }

        if (ibis::gVerbose > 1) {
            uint32_t tcurr = time(0);
            if (tcurr-59 > tlast) {
                std::ostringstream ostmp;
                ostmp << "TIME(" << tcurr
                      << "): just completed processing bin " << il1
                      << " out of " << nobs << ", sure.cnt()=" << sure.cnt()
                      << ", iffy.cnt()=" << iffy.cnt();
                ibis::util::logMessage("bin::equiJoin", "%s",
                                       ostmp.str().c_str());
                tlast = tcurr;
            }
        }
        ++ il1;
    }
} // ibis::bin::equiJoin

// A range join on the same variable.
void ibis::bin::deprecatedJoin(const double& delta,
                               ibis::bitvector64& sure,
                               ibis::bitvector64& iffy) const {
    LOGGER(ibis::gVerbose > 3)
        << "bin::deprecatedJoin starts processing a range-join ("
        << (col ? col->fullname() : "?.?") << " between "
        << (col ? col->fullname() : "?.?") << " - " << delta << " and "
        << (col ? col->fullname() : "?.?") << " + " << delta << ')';

    uint32_t il1=0, il2=0, iu2=0;
    uint32_t ilu=0, iuu=0; // bits[ilu:iuu] is summed in cumu
    uint32_t ilv=0, iuv=0; // bits[ilv:iuv] is summed in cumv
    ibis::bitvector cumu, cumv;
    uint32_t tlast = time(0);
    while (il1 < nobs && il2 < nobs) {
        while (il1 < nobs && il2 < nobs &&
               !(maxval[il1] >= minval[il1] &&
                 maxval[il2] >= minval[il2] &&
                 maxval[il1]+delta >= minval[il2] &&
                 maxval[il2]+delta >= minval[il1])) {
            if (maxval[il1] >= minval[il1] &&
                maxval[il2] >= minval[il2]) {
                if (!(maxval[il1]+delta >= minval[il2])) ++ il1;
                else ++ il2;
            }
            else {
                if (!(maxval[il1] >= minval[il1])) ++ il1;
                if (!(maxval[il2] >= minval[il2])) ++ il2;
            }
        }

        if (bits[il1] != 0 && bits[il1]->cnt() > 0 &&
            il1 < nobs && il2 < nobs) { // found some overlap
            // minval[iu2] > maxval[il1+delta
            for (iu2 = il2+1; iu2 < nobs &&
                     minval[iu2] <= maxval[il1]+delta; ++ iu2);
            uint32_t im2; // minval[im2] >= maxval[il1]-delta
            for (im2 = il2; im2 < nobs &&
                     minval[im2] < maxval[il1]-delta; ++ im2);
            uint32_t in2; // maxval[in2] > minval[il1]+delta
            for (in2 = il2; in2 < nobs &&
                     maxval[in2] <= minval[il1]+delta; ++ in2);
            if (im2 < in2) { // sure hits
                sumBins(im2, in2, cumv, ilv, iuv);
                ibis::util::outerProduct
                    (*(bits[il1]), cumv, sure);
                ilv = im2;
                iuv = in2;
            }
            if (il2 < im2 || in2 < iu2) { // need to update iffy
                if (il2+1 == im2 && in2 == iu2) {
                    // only bits[il2]
                    ibis::util::outerProduct(*(bits[il1]), *(bits[il2]), iffy);
                }
                else if (il2 == im2 && in2+1 == iu2) {
                    // only bits[in2]
                    ibis::util::outerProduct(*(bits[il1]), *(bits[in2]), iffy);
                }
                else if (il2+1 == im2 && in2+1 == iu2) {
                    // only bits[il2] and idex.bits[in2]
                    ibis::bitvector tmp(*(bits[il2]));
                    tmp |= *(bits[in2]);
                    ibis::util::outerProduct(*(bits[il1]), tmp, iffy);
                }
                else {
                    // need to put entries from multiple bins into iffy
                    if (ilu >= iuu ||
                        (in2 > im2 && (in2-im2) > (iu2-il2)/2 &&
                         (iuu < il2 ||
                          (iuu > il2 && (ilu <= il2 ? il2-ilu : ilu-il2) +
                           iu2 - iuu > (im2 - il2 + iu2 - in2))))) {
                        // copy cumv to cumu
                        cumu.copy(cumv);
                        ilu = ilv;
                        iuu = iuv;
                    }
                    sumBins(il2, iu2, cumu, ilu, iuu);
                    ibis::util::outerProduct(*(bits[il1]), cumu, iffy);
                    ilu = il2;
                    iuu = iu2;
                }
            }
        }

        if (ibis::gVerbose > 1) {
            uint32_t tcurr = time(0);
            if (tcurr-59 > tlast) {
                std::ostringstream ostmp;
                ostmp << "TIME(" << tcurr
                      << "): just completed processing bin " << il1
                      << " out of " << nobs << ", sure.cnt()=" << sure.cnt()
                      << ", iffy.cnt()=" << iffy.cnt();
                ibis::util::logMessage("bin::deprecatedJoin", "%s",
                                       ostmp.str().c_str());
                tlast = tcurr;
            }
        }
        ++ il1;
    }
} // ibis::bin::deprecatedJoin

/// A range join on the same column with a complex distance function.  This
/// implementation relys on the fact that the expression @c expr is
/// monotonic in each bin.  Since the user does not know the exact bin
/// boundaries, this essentially require the express @c expr be a monotonic
/// function overall.  If this monotonicity is not satisfyied, this
/// function may miss some hits!
void ibis::bin::compJoin(const ibis::math::term *expr,
                         ibis::bitvector64& sure,
                         ibis::bitvector64& iffy) const {
    ibis::index::barrel bar(const_cast<const ibis::math::term*>(expr));
    if (bar.size() == 0) {
        const double delta = fabs(expr->eval());
        if (delta > 0.0)
            deprecatedJoin(delta, sure, iffy);
        else
            equiJoin(sure, iffy);
        return;
    }
    else if (bar.size() != 1 && stricmp(bar.name(0), col->name()) != 0) {
        std::ostringstream ostr;
        ostr << *expr;
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- bin::compJoin cannot deal with complex range "
            "expression " << ostr.str();
        uint64_t npairs = static_cast<uint64_t>(nrows) * nrows;
        sure.set(0, npairs);
        iffy.set(1, npairs);
        return;
    }
    LOGGER(ibis::gVerbose > 3)
        << "bin::compJoin started processing range join "
        << (col ? col->fullname() : "?") << " between "
        << (col ? col->fullname() : "?") << " - " << *expr << " and "
        << (col ? col->fullname() : "?") << " + " << *expr;

    uint32_t il1=0, il2=0, iu2=0;
    uint32_t ilu=0, iuu=0; // bits[ilu:iuu] is summed in cumu
    uint32_t ilv=0, iuv=0; // bits[ilv:iuv] is summed in cumv
    ibis::bitvector cumu, cumv;
    uint32_t tlast = time(0);
    while (il1 < nobs && il2 < nobs) {
        double delta=0;
        while (il1 < nobs && il2 < nobs) {
            if (! (maxval[il1] >= minval[il1])) {
                ++ il1;
            }
            else if (! (maxval[il2] >= minval[il2])) {
                ++ il2;
            }
            else { // compute the delta value
                bar.setValue(0, minval[il1]);
                delta = fabs(expr->eval());
                if (maxval[il1] != minval[il1]) {
                    bar.setValue(0, maxval[il1]);
                    double tmp = fabs(expr->eval());
                    if (tmp > delta)
                        delta = tmp;
                }
                if (maxval[il2] + delta >= minval[il1]) {
                    if (minval[il2] <= maxval[il1] + delta)
                        break;
                    else
                        ++ il1;
                }
                else if (minval[il2] <= maxval[il1]) {
                    ++ il2;
                }
                else {
                    ++ il1;
                    ++ il2;
                }
            }
        }

        if (il1 < nobs && il2 < nobs && bits[il1] &&
            bits[il1]->cnt() > 0) { // found some overlap
            if (minval[il1] == maxval[il1]) {
                uint32_t im2; // minval[im2] >= maxval[il1]-delta
                for (im2 = il2+1; im2 < nobs &&
                         minval[im2] < maxval[il1]-delta; ++ im2);
                uint32_t in2; // maxval[in2] > minval[il1]+delta
                for (in2 = il2+1; in2 < nobs &&
                         maxval[in2] <= minval[il1]+delta; ++ in2);
                if (im2 < in2) { // sure hits
                    sumBins(im2, in2, cumv, ilv, iuv);
                    ibis::util::outerProduct(*(bits[il1]), cumv, sure);
                    ilv = im2;
                    iuv = in2;
                }
            }
            // minval[iu2] > maxval[il1+delta
            for (iu2 = il2+1; iu2 < nobs &&
                     minval[iu2] <= maxval[il1]+delta; ++ iu2);
            // not exactly sure, put the result in iffy
            sumBins(il2, iu2, cumu, ilu, iuu);
            ibis::util::outerProduct(*(bits[il1]), cumu, iffy);
            ilu = il2;
            iuu = iu2;
        }

        if (ibis::gVerbose > 1) {
            uint32_t tcurr = time(0);
            if (tcurr-59 > tlast) {
                std::ostringstream ostmp;
                ostmp << "TIME(" << tcurr
                      << "): just completed processing bin " << il1
                      << " out of " << nobs << ", sure.cnt()=" << sure.cnt()
                      << ", iffy.cnt()=" << iffy.cnt();
                ibis::util::logMessage("bin::compJoin", "%s",
                                       ostmp.str().c_str());
                tlast = tcurr;
            }
        }
        ++ il1;
    }
} // ibis::bin::compJoin

/// An equi-join on two different columns.
void ibis::bin::equiJoin(const ibis::bin& idx2,
                         ibis::bitvector64& sure,
                         ibis::bitvector64& iffy) const {
    LOGGER(ibis::gVerbose > 3)
        << "bin::equiJoin started processing an equi-join "
        << (col ? col->fullname() : "?.?") << " = "
        << (idx2.col ? idx2.col->fullname() : "?.?");

    uint32_t il1=0, il2=0, iu2=0;
    uint32_t ilc=0, iuc=0; // idx2.bits[ilc:iuc] is summed in cumu
    ibis::bitvector cumu;
    uint32_t tlast = time(0);
    while (il1 < nobs && il2 < idx2.nobs) {
        while (il1 < nobs && il2 < idx2.nobs &&
               !(maxval[il1] >= minval[il1] &&
                 idx2.maxval[il2] >= idx2.minval[il2] &&
                 maxval[il1] >= idx2.minval[il2] &&
                 idx2.maxval[il2] >= minval[il1])) {
            if (maxval[il1] >= minval[il1] &&
                idx2.maxval[il2] >= idx2.minval[il2]) {
                if (!(maxval[il1] >= idx2.minval[il2])) ++ il1;
                else ++ il2;
            }
            else {
                if (!(maxval[il1] >= minval[il1])) ++ il1;
                if (!(idx2.maxval[il2] >= idx2.minval[il2])) ++ il2;
            }
        }

        if (il1 < nobs && il2 < idx2.nobs && bits[il1] &&
            bits[il1]->cnt()) { // found some overlap
            if (minval[il1] == maxval[il1] &&
                minval[il1] == idx2.minval[il2] &&
                minval[il1] == idx2.maxval[il2] ) {
                // exactly equal, record the pairs in @c sure
                ibis::util::outerProduct
                    (*(bits[il1]), *(idx2.bits[il2]), sure);
            }
            else { // not exactly sure, put the result in iffy
                // bins [il2, iu2) from idx2 overlaps with bin il1 of *this
                for (iu2 = il2+1; iu2 < idx2.nobs &&
                         idx2.minval[iu2] <= maxval[il1]; ++ iu2);
                sumBins(il2, iu2, cumu, ilc, iuc);
                ibis::util::outerProduct(*(bits[il1]), cumu, iffy);
                ilc = il2;
                iuc = iu2;
            }
        }

        if (ibis::gVerbose > 1) {
            uint32_t tcurr = time(0);
            if (tcurr-59 > tlast) {
                std::ostringstream ostmp;
                ostmp << "TIME(" << tcurr
                      << "): just completed processing bin " << il1
                      << " out of " << nobs << ", sure.cnt()=" << sure.cnt()
                      << ", iffy.cnt()=" << iffy.cnt();
                ibis::util::logMessage("bin::equiJoin", "%s",
                                       ostmp.str().c_str());
                tlast = tcurr;
            }
        }
        ++ il1;
    }
} // ibis::bin::equiJoin

/// A range join on two different columns.
void ibis::bin::deprecatedJoin(const ibis::bin& idx2,
                               const double& delta,
                               ibis::bitvector64& sure,
                               ibis::bitvector64& iffy) const {
    LOGGER(ibis::gVerbose > 3)
        << "bin::deprecatedJoin starts to process range join "
        << (col ? col->fullname() : "?.?") << " between "
        << (col ? col->fullname() : "?.?") << " - " << delta << " and "
        << (col ? col->fullname() : "?.?") << " + " << delta;

    uint32_t il1=0, il2=0, iu2=0;
    uint32_t ilu=0, iuu=0; // idx2.bits[ilu:iuu] is summed in cumu
    uint32_t ilv=0, iuv=0; // idx2.bits[ilv:iuv] is summed in cumv
    ibis::bitvector cumu, cumv;
    uint32_t tlast = time(0);
    while (il1 < nobs && il2 < idx2.nobs) {
        while (il1 < nobs && il2 < idx2.nobs &&
               !(maxval[il1] >= minval[il1] &&
                 idx2.maxval[il2] >= idx2.minval[il2] &&
                 maxval[il1]+delta >= idx2.minval[il2] &&
                 idx2.maxval[il2]+delta >= minval[il1])) {
            if (maxval[il1] >= minval[il1] &&
                idx2.maxval[il2] >= idx2.minval[il2]) {
                if (!(maxval[il1]+delta >= idx2.minval[il2])) ++ il1;
                else ++ il2;
            }
            else {
                if (!(maxval[il1] >= minval[il1])) ++ il1;
                if (!(idx2.maxval[il2] >= idx2.minval[il2])) ++ il2;
            }
        }

        if (il1 < nobs && il2 < idx2.nobs && bits[il1] &&
            bits[il1]->cnt() > 0) { // found some overlap
            // idx2.minval[iu2] > maxval[il1+delta
            for (iu2 = il2+1; iu2 < idx2.nobs &&
                     idx2.minval[iu2] <= maxval[il1]+delta; ++ iu2);
            uint32_t im2; // idx2.minval[im2] >= maxval[il1]-delta
            for (im2 = il2; im2 < idx2.nobs &&
                     idx2.minval[im2] < maxval[il1]-delta; ++ im2);
            uint32_t in2; // idx2.maxval[in2] > minval[il1]+delta
            for (in2 = il2; in2 < idx2.nobs &&
                     idx2.maxval[in2] <= minval[il1]+delta; ++ in2);
            if (im2 < in2) { // sure hits
                idx2.sumBins(im2, in2, cumv, ilv, iuv);
                ibis::util::outerProduct
                    (*(bits[il1]), cumv, sure);
                ilv = im2;
                iuv = in2;
            }
            if (il2 < im2 || in2 < iu2) { // need to update iffy
                if (il2+1 == im2 && in2 == iu2) {
                    // only idx2.bits[il2]
                    ibis::util::outerProduct(*(bits[il1]), *(idx2.bits[il2]),
                                             iffy);
                }
                else if (il2 == im2 && in2+1 == iu2) {
                    // only idx2.bits[in2]
                    ibis::util::outerProduct(*(bits[il1]), *(idx2.bits[in2]),
                                             iffy);
                }
                else if (il2+1 == im2 && in2+1 == iu2) {
                    // only idx2.bits[il2] and idex.bits[in2]
                    ibis::bitvector tmp(*(idx2.bits[il2]));
                    tmp |= *(idx2.bits[in2]);
                    ibis::util::outerProduct(*(bits[il1]), tmp, iffy);
                }
                else {
                    // need to put entries from multiple bins into iffy
                    if (ilu >= iuu ||
                        (in2 > im2 && (in2-im2) > (iu2-il2)/2 &&
                         (iuu < il2 ||
                          (iuu > il2 && (ilu <= il2 ? il2-ilu : ilu-il2) +
                           iu2 - iuu > (im2 - il2 + iu2 - in2))))) {
                        // copy cumv to cumu
                        cumu.copy(cumv);
                        ilu = ilv;
                        iuu = iuv;
                    }
                    idx2.sumBins(il2, iu2, cumu, ilu, iuu);
                    ibis::util::outerProduct(*(bits[il1]), cumu, iffy);
                    ilu = il2;
                    iuu = iu2;
                }
            }
        }

        if (ibis::gVerbose > 1) {
            uint32_t tcurr = time(0);
            if (tcurr-59 > tlast) {
                std::ostringstream ostmp;
                ostmp << "TIME(" << tcurr
                      << "): just completed processing bin " << il1
                      << " out of " << nobs << ", sure.cnt()=" << sure.cnt()
                      << ", iffy.cnt()=" << iffy.cnt();
                ibis::util::logMessage("bin::deprecatedJoin", "%s",
                                       ostmp.str().c_str());
                tlast = tcurr;
            }
        }
        ++ il1;
    }
} // ibis::bin::deprecatedJoin

/// A range join between two different columns with a complex distance
/// function.  This implementation relys on the fact that the expression @c
/// expr is monotonic in each bin.  Since the user does not know the exact
/// bin boundaries, this essentially require the express @c expr be a
/// monotonic function overall.  If this monotonicity is not satisfyied,
/// this function may miss some hits!
void ibis::bin::compJoin(const ibis::bin& idx2,
                         const ibis::math::term *expr,
                         ibis::bitvector64& sure,
                         ibis::bitvector64& iffy) const {
    if (col == 0) return;
    ibis::index::barrel bar(const_cast<const ibis::math::term*>(expr));
    if (bar.size() == 0) {
        const double delta = fabs(expr->eval());
        if (delta > 0.0)
            deprecatedJoin(idx2, delta, sure, iffy);
        else
            equiJoin(idx2, sure, iffy);
        return;
    }
    else if (bar.size() > 1 || stricmp(bar.name(0), col->name()) != 0) {
        std::ostringstream ostr;
        ostr << *expr;
        col->logWarning("bin::compJoin", "failed to deal with complex range "
                        "expression %s", ostr.str().c_str());
        uint64_t npairs = static_cast<uint64_t>(nrows) * nrows;
        sure.set(0, npairs);
        iffy.set(1, npairs);
        return;
    }
    if (ibis::gVerbose > 3) {
        std::ostringstream ostr;
        ostr << *expr;
        ibis::util::logMessage
            ("bin::compJoin", "start processing a range join ("
             "%s between %s - %s and %s + %s)", col->name(),
             idx2.col->name(), ostr.str().c_str(),
             idx2.col->name(), ostr.str().c_str());
    }


    uint32_t il1=0, il2=0, iu2=0;
    uint32_t ilu=0, iuu=0; // idx2.bits[ilu:iuu] is summed in cumu
    uint32_t ilv=0, iuv=0; // idx2.bits[ilv:iuv] is summed in cumv
    ibis::bitvector cumu, cumv;
    uint32_t tlast = time(0);
    while (il1 < nobs && il2 < idx2.nobs) {
        double delta=0;
        while (il1 < nobs && il2 < idx2.nobs) {
            if (! (maxval[il1] >= minval[il1])) {
                ++ il1;
            }
            else if (! (idx2.maxval[il2] >= idx2.minval[il2])) {
                ++ il2;
            }
            else { // compute the delta value
                bar.setValue(0, minval[il1]);
                delta = fabs(expr->eval());
                if (maxval[il1] != minval[il1]) {
                    bar.setValue(0, maxval[il1]);
                    double tmp = fabs(expr->eval());
                    if (tmp > delta)
                        delta = tmp;
                }
                if (idx2.maxval[il2] + delta >= minval[il1]) {
                    if (idx2.minval[il2] <= maxval[il1] + delta)
                        break;
                    else
                        ++ il1;
                }
                else if (idx2.minval[il2] <= maxval[il1]) {
                    ++ il2;
                }
                else {
                    ++ il1;
                    ++ il2;
                }
            }
        }

        if (il1 < nobs && il2 < idx2.nobs && bits[il1] &&
            bits[il1]->cnt() > 0) { // found some overlap
            if (minval[il1] == maxval[il1]) {
                uint32_t im2; // idx2.minval[im2] >= maxval[il1]-delta
                for (im2 = il2+1; im2 < idx2.nobs &&
                         idx2.minval[im2] < maxval[il1]-delta; ++ im2);
                uint32_t in2; // idx2.maxval[in2] > minval[il1]+delta
                for (in2 = il2+1; in2 < idx2.nobs &&
                         idx2.maxval[in2] <= minval[il1]+delta; ++ in2);
                if (im2 < in2) { // sure hits
                    sumBins(im2, in2, cumv, ilv, iuv);
                    ibis::util::outerProduct(*(bits[il1]), cumv, sure);
                    ilv = im2;
                    iuv = in2;
                }
            }
            // idx2.minval[iu2] > maxval[il1+delta
            for (iu2 = il2+1; iu2 < idx2.nobs &&
                     idx2.minval[iu2] <= maxval[il1]+delta; ++ iu2);
            // not exactly sure, put the result in iffy
            sumBins(il2, iu2, cumu, ilu, iuu);
            ibis::util::outerProduct(*(bits[il1]), cumu, iffy);
            ilu = il2;
            iuu = iu2;
        }

        if (ibis::gVerbose > 1) {
            uint32_t tcurr = time(0);
            if (tcurr-59 > tlast) {
                std::ostringstream ostmp;
                ostmp << "TIME(" << tcurr
                      << "): just completed processing bin " << il1
                      << " out of " << nobs << ", sure.cnt()=" << sure.cnt()
                      << ", iffy.cnt()=" << iffy.cnt();
                ibis::util::logMessage("bin::compJoin", "%s",
                                       ostmp.str().c_str());
                tlast = tcurr;
            }
        }
        ++ il1;
    }
} // ibis::bin::compJoin

/// An equi-join on the same variable and using the same index.  This
/// version makes use of a mask and only examines records that are marked
/// 1.
void ibis::bin::equiJoin(const ibis::bitvector& mask,
                         ibis::bitvector64& sure,
                         ibis::bitvector64& iffy) const {
    uint32_t il1=0, il2=0, iu2=0;
    uint32_t ilc=0, iuc=0; // bits[ilc:iuc] is summed in cumu
    ibis::bitvector cumu, curr;
    LOGGER(ibis::gVerbose > 3)
        << "bin::equiJoin starts to process equi-join "
        << (col ? col->fullname() : "?.?") << " = "
        << (col ? col->fullname() : "?.?")
        << " with a mask of " << mask.cnt();

    uint32_t tlast = time(0);
    while (il1 < nobs && il2 < nobs) {
        while (il1 < nobs && il2 < nobs &&
               !(maxval[il1] >= minval[il1] &&
                 maxval[il2] >= minval[il2] &&
                 maxval[il1] >= minval[il2] &&
                 maxval[il2] >= minval[il1])) {
            if (maxval[il1] >= minval[il1] &&
                maxval[il2] >= minval[il2]) {
                if (!(maxval[il1] >= minval[il2])) ++ il1;
                else ++ il2;
            }
            else {
                if (!(maxval[il1] >= minval[il1])) ++ il1;
                if (!(maxval[il2] >= minval[il2])) ++ il2;
            }
        }

        if (il1 < nobs && il2 < nobs && bits[il1] &&
            bits[il1]->cnt() > 0) { // found some overlap
            curr.copy(mask);
            curr &= *(bits[il1]);
            if (curr.cnt() > 0) {
                if (minval[il1] == maxval[il1] &&
                    minval[il1] == minval[il2] &&
                    minval[il1] == maxval[il2] ) {
                    // exactly equal, record the pairs in @c sure
                    ibis::bitvector tmp(*(bits[il2]));
                    tmp &= mask;
                    if (tmp.cnt() > 0)
                        ibis::util::outerProduct(curr, tmp, sure);
                }
                else { // not exactly sure, put the result in iffy
                    // bins [il2, iu2) from idx2 overlaps with bin il1 of
                    // *this
                    for (iu2 = il2+1; iu2 < nobs &&
                             minval[iu2] <= maxval[il1]; ++ iu2);
                    sumBins(il2, iu2, cumu, ilc, iuc);
                    ibis::bitvector tmp(cumu);
                    tmp &= mask;
                    if (cumu.cnt() > 0)
                        ibis::util::outerProduct(curr, tmp, iffy);
                    ilc = il2;
                    iuc = iu2;
                }
            }
        }

        if (ibis::gVerbose > 1) {
            uint32_t tcurr = time(0);
            if (tcurr-59 > tlast) {
                std::ostringstream ostmp;
                ostmp << "TIME(" << tcurr
                      << "): just completed processing bin " << il1
                      << " out of " << nobs << ", sure.cnt()=" << sure.cnt()
                      << ", iffy.cnt()=" << iffy.cnt();
                ibis::util::logMessage("bin::equiJoin", "%s",
                                       ostmp.str().c_str());
                tlast = tcurr;
            }
        }
        ++ il1;
    }
} // ibis::bin::equiJoin

// A range join on the same variable.
void ibis::bin::deprecatedJoin(const double& delta,
                               const ibis::bitvector& mask,
                               ibis::bitvector64& sure,
                               ibis::bitvector64& iffy) const {
    if (delta <= 0.0) {
        equiJoin(mask, sure, iffy);
        return;
    }

    LOGGER(ibis::gVerbose > 3)
        << "bin::deprecatedJoin starts to process range join "
        << (col ? col->fullname() : "?.?") << " between "
        << (col ? col->fullname() : "?.?") << " - " << delta << " and "
        << (col ? col->fullname() : "?.?") << " + " << delta
        << " with a mask of " << mask.cnt();

    uint32_t il1=0, il2=0, iu2=0;
    uint32_t ilu=0, iuu=0; // bits[ilu:iuu] is summed in cumu
    uint32_t ilv=0, iuv=0; // bits[ilv:iuv] is summed in cumv
    ibis::bitvector cumu, cumv, curr;
    uint32_t tlast = time(0);
    for (; il1 < nobs && il2 < nobs; ++ il1) {
        while (il1 < nobs && il2 < nobs &&
               !(maxval[il1] >= minval[il1] &&
                 maxval[il2] >= minval[il2] &&
                 maxval[il1]+delta >= minval[il2] &&
                 maxval[il2]+delta >= minval[il1])) {
            if (maxval[il1] >= minval[il1] &&
                maxval[il2] >= minval[il2]) {
                if (!(maxval[il1]+delta >= minval[il2])) ++ il1;
                else ++ il2;
            }
            else {
                if (!(maxval[il1] >= minval[il1])) ++ il1;
                if (!(maxval[il2] >= minval[il2])) ++ il2;
            }
        }

        if (il1 < nobs && il2 < nobs && bits[il1] &&
            bits[il1]->cnt() > 0) { // found some overlap
            // minval[iu2] > maxval[il1+delta
            curr.copy(mask);
            curr &= *(bits[il1]);
            if (curr.cnt() == 0)
                continue;

            for (iu2 = il2+1; iu2 < nobs &&
                     minval[iu2] <= maxval[il1]+delta; ++ iu2);
            uint32_t im2; // minval[im2] >= maxval[il1]-delta
            for (im2 = il2; im2 < nobs &&
                     minval[im2] < maxval[il1]-delta; ++ im2);
            uint32_t in2; // maxval[in2] > minval[il1]+delta
            for (in2 = il2; in2 < nobs &&
                     maxval[in2] <= minval[il1]+delta; ++ in2);
            if (im2 < in2) { // sure hits
                sumBins(im2, in2, cumv, ilv, iuv);
                ibis::bitvector tmp(mask);
                tmp &= cumv;
                ibis::util::outerProduct(curr, tmp, sure);
                ilv = im2;
                iuv = in2;
            }
            if (il2 < im2 || in2 < iu2) { // need to update iffy
                if (il2+1 == im2 && in2 == iu2) {
                    // only bits[il2]
                    ibis::bitvector tmp(mask);
                    tmp &= *(bits[il2]);
                    ibis::util::outerProduct(curr, tmp, iffy);
                }
                else if (il2 == im2 && in2+1 == iu2) {
                    // only bits[in2]
                    ibis::bitvector tmp(mask);
                    tmp &= *(bits[in2]);
                    ibis::util::outerProduct(*(bits[il1]), tmp, iffy);
                }
                else if (il2+1 == im2 && in2+1 == iu2) {
                    // only bits[il2] and idex.bits[in2]
                    ibis::bitvector tmp(*(bits[il2]));
                    tmp |= *(bits[in2]);
                    tmp &= mask;
                    ibis::util::outerProduct(*(bits[il1]), tmp, iffy);
                }
                else {
                    // need to put entries from multiple bins into iffy
                    if (ilu >= iuu ||
                        (in2 > im2 && (in2-im2) > (iu2-il2)/2 &&
                         (iuu < il2 ||
                          (iuu > il2 && (ilu <= il2 ? il2-ilu : ilu-il2) +
                           iu2 - iuu > (im2 - il2 + iu2 - in2))))) {
                        // copy cumv to cumu
                        cumu.copy(cumv);
                        ilu = ilv;
                        iuu = iuv;
                    }
                    sumBins(il2, iu2, cumu, ilu, iuu);
                    ibis::bitvector tmp(mask);
                    tmp &= cumu;
                    ibis::util::outerProduct(*(bits[il1]), tmp, iffy);
                    ilu = il2;
                    iuu = iu2;
                }
            }
        }

        if (ibis::gVerbose > 1) {
            uint32_t tcurr = time(0);
            if (tcurr-59 > tlast) {
                std::ostringstream ostmp;
                ostmp << "TIME(" << tcurr
                      << "): just completed processing bin " << il1
                      << " out of " << nobs << ", sure.cnt()=" << sure.cnt()
                      << ", iffy.cnt()=" << iffy.cnt();
                ibis::util::logMessage("bin::deprecatedJoin", "%s",
                                       ostmp.str().c_str());
                tlast = tcurr;
            }
        }
    }
} // ibis::bin::deprecatedJoin

/// A range join on the same column with a complex distance function.  This
/// implementation relys on the fact that the expression @c expr is
/// monotonic in each bin.  Since the user does not know the exact bin
/// boundaries, this essentially require the express @c expr be a monotonic
/// function overall.  If this monotonicity is not satisfyied, this
/// function may miss some hits!
void ibis::bin::compJoin(const ibis::math::term *expr,
                         const ibis::bitvector& mask,
                         ibis::bitvector64& sure,
                         ibis::bitvector64& iffy) const {
    if (col == 0) return;
    ibis::index::barrel bar(const_cast<const ibis::math::term*>(expr));
    if (bar.size() == 0) {
        const double delta = fabs(expr->eval());
        if (delta > 0.0)
            deprecatedJoin(delta, mask, sure, iffy);
        else
            equiJoin(mask, sure, iffy);
        return;
    }
    else if (bar.size() != 1 && stricmp(bar.name(0), col->name()) != 0) {
        std::ostringstream ostr;
        ostr << *expr;
        col->logWarning("bin::compJoin", "failed to deal with complex range "
                        "expression %s", ostr.str().c_str());
        uint64_t npairs = static_cast<uint64_t>(nrows) * nrows;
        sure.set(0, npairs);
        iffy.set(1, npairs);
        return;
    }
    if (ibis::gVerbose > 3) {
        std::ostringstream ostr;
        ostr << *expr;
        ibis::util::logMessage
            ("bin::compJoin", "start processing a range join ("
             "%s between %s - %s and %s + %s) with mask size %lu",
             col->name(), col->name(), ostr.str().c_str(),
             col->name(), ostr.str().c_str(),
             static_cast<long unsigned>(mask.size()));
    }

    uint32_t il1=0, il2=0, iu2=0;
    uint32_t ilu=0, iuu=0; // bits[ilu:iuu] is summed in cumu
    uint32_t ilv=0, iuv=0; // bits[ilv:iuv] is summed in cumv
    ibis::bitvector cumu, cumv, curr;
    uint32_t tlast = time(0);
    for (; il1 < nobs && il2 < nobs; ++ il1) {
        double delta=0;
        while (il1 < nobs && il2 < nobs) {
            if (! (maxval[il1] >= minval[il1])) {
                ++ il1;
            }
            else if (! (maxval[il2] >= minval[il2])) {
                ++ il2;
            }
            else { // compute the delta value
                bar.setValue(0, minval[il1]);
                delta = fabs(expr->eval());
                if (maxval[il1] != minval[il1]) {
                    bar.setValue(0, maxval[il1]);
                    double tmp = fabs(expr->eval());
                    if (tmp > delta)
                        delta = tmp;
                }
                if (maxval[il2] + delta >= minval[il1]) {
                    if (minval[il2] <= maxval[il1] + delta)
                        break;
                    else
                        ++ il1;
                }
                else if (minval[il2] <= maxval[il1]) {
                    ++ il2;
                }
                else {
                    ++ il1;
                    ++ il2;
                }
            }
        }

        if (il1 < nobs && il2 < nobs && bits[il1] &&
            bits[il1]->cnt() > 0) { // found some overlap
            curr.copy(mask);
            curr &= *(bits[il1]);
            if (curr.cnt() == 0)
                continue;

            if (minval[il1] == maxval[il1]) {
                uint32_t im2; // minval[im2] >= maxval[il1]-delta
                for (im2 = il2+1; im2 < nobs &&
                         minval[im2] < maxval[il1]-delta; ++ im2);
                uint32_t in2; // maxval[in2] > minval[il1]+delta
                for (in2 = il2+1; in2 < nobs &&
                         maxval[in2] <= minval[il1]+delta; ++ in2);
                if (im2 < in2) { // sure hits
                    sumBins(im2, in2, cumv, ilv, iuv);
                    ibis::bitvector tmp(mask);
                    tmp &= cumv;
                    ibis::util::outerProduct(curr, tmp, sure);
                    ilv = im2;
                    iuv = in2;
                }
            }
            // minval[iu2] > maxval[il1+delta
            for (iu2 = il2+1; iu2 < nobs &&
                     minval[iu2] <= maxval[il1]+delta; ++ iu2);
            // not exactly sure, put the result in iffy
            sumBins(il2, iu2, cumu, ilu, iuu);
            ibis::bitvector cmk(mask);
            cmk &= cumu;
            ibis::util::outerProduct(curr, cmk, iffy);
            ilu = il2;
            iuu = iu2;
        }

        if (ibis::gVerbose > 1) {
            uint32_t tcurr = time(0);
            if (tcurr-59 > tlast) {
                std::ostringstream ostmp;
                ostmp << "TIME(" << tcurr
                      << "): just completed processing bin " << il1
                      << " out of " << nobs << ", sure.cnt()=" << sure.cnt()
                      << ", iffy.cnt()=" << iffy.cnt();
                ibis::util::logMessage("bin::compJoin", "%s"
                                       , ostmp.str().c_str());
                tlast = tcurr;
            }
        }
    }
} // ibis::bin::compJoin

void ibis::bin::equiJoin(const ibis::bitvector& mask,
                         const ibis::qRange* const range1,
                         const ibis::qRange* const range2,
                         ibis::bitvector64& sure,
                         ibis::bitvector64& iffy) const {
    if (mask.cnt() == 0) {
        uint64_t np = mask.size();
        np *= np;
        sure.set(0, np);
        iffy.clear();
        return;
    }

    if (col == 0) return;
    uint32_t il1=0, il2=0, iu2=0;
    uint32_t ilc=0, iuc=0; // bits[ilc:iuc] is summed in cumu
    ibis::bitvector cumu, curr;
    LOGGER(ibis::gVerbose > 3)
        << "bin::equiJoin starts to process equi-join "
        << (col ? col->fullname() : "?.?") << " = "
        << (col ? col->fullname() : "?.?") << " and a mask of "
        << mask.cnt();

    uint32_t nbmax = nobs;
    if (range1 || range2) {
        double amin = (range1 ? range1->leftBound() : col->getActualMin());
        double tmp = (range2 ? range2->leftBound() : amin);
        if (amin < tmp)
            amin = tmp;
        double amax = (range1 ? range1->rightBound() : col->getActualMax());
        tmp = (range2 ? range2->rightBound() : amax);
        if (amax > tmp)
            amax = tmp;
        il1 = bounds.find(amin);
        nbmax = bounds.find(amax);
        if (nbmax < nobs && minval[nbmax] <= amax)
            ++ nbmax;
    }
    il2 = il1;
    iu2 = il1;

    activate(il1, nbmax);
    uint32_t tlast = time(0);
    for (; il1 < nbmax && il2 < nbmax; ++ il1) {
        while (il1 < nbmax && il2 < nbmax &&
               !(maxval[il1] >= minval[il1] &&
                 maxval[il2] >= minval[il2] &&
                 maxval[il1] >= minval[il2] &&
                 maxval[il2] >= minval[il1])) {
            if (maxval[il1] >= minval[il1] &&
                maxval[il2] >= minval[il2]) {
                if (!(maxval[il1] >= minval[il2])) ++ il1;
                else ++ il2;
            }
            else {
                if (!(maxval[il1] >= minval[il1])) ++ il1;
                if (!(maxval[il2] >= minval[il2])) ++ il2;
            }
        }

        if (il1 < nbmax && il2 < nbmax && bits[il1] &&
            bits[il1]->cnt()) { // found some overlap
            curr.copy(mask);
            curr &= *(bits[il1]);
            if (curr.cnt() > 0) {
                if (minval[il1] == maxval[il1] &&
                    minval[il1] == minval[il2] &&
                    minval[il1] == maxval[il2]) {
                    if ((range1 == 0 || range1->inRange(minval[il1])) &&
                        (range2 == 0 || range2->inRange(minval[il2]))) {
                        // exactly equal, record the pairs in @c sure
                        ibis::bitvector tmp(*(bits[il2]));
                        tmp &= mask;
                        if (tmp.cnt() > 0)
                            ibis::util::outerProduct(curr, tmp, sure);
                    }
                }
                else { // not exactly sure, put the result in iffy
                    // bins [il2, iu2) from idx2 overlaps with bin il1 of
                    // *this
                    for (iu2 = il2+1; iu2 < nbmax &&
                             minval[iu2] <= maxval[il1]; ++ iu2);
                    sumBins(il2, iu2, cumu, ilc, iuc);
                    ibis::bitvector tmp(mask);
                    tmp &= cumu;
                    if (tmp.cnt() > 0)
                        ibis::util::outerProduct(curr, tmp, iffy);
                    ilc = il2;
                    iuc = iu2;
                }
            }
        }

        if (ibis::gVerbose > 1) {
            uint32_t tcurr = time(0);
            if (tcurr-59 > tlast) {
                std::ostringstream ostmp;
                ostmp << "TIME(" << tcurr
                      << "): just completed processing bin " << il1
                      << " out of " << nbmax << ", sure.cnt()=" << sure.cnt()
                      << ", iffy.cnt()=" << iffy.cnt();
                ibis::util::logMessage("bin::equiJoin", "%s",
                                       ostmp.str().c_str());
                tlast = tcurr;
            }
        }
    }
} // ibis::bin::equiJoin

// A range join on the same variable.
void ibis::bin::deprecatedJoin(const double& delta,
                               const ibis::bitvector& mask,
                               const ibis::qRange* const range1,
                               const ibis::qRange* const range2,
                               ibis::bitvector64& sure,
                               ibis::bitvector64& iffy) const {
    if (col == 0) return;
    if (mask.cnt() == 0) {
        uint64_t np = mask.size();
        np *= np;
        sure.set(0, np);
        iffy.clear();
        return;
    }

    if (delta <= 0.0) {
        equiJoin(mask, range1, range2, sure, iffy);
        return;
    }
    if (ibis::gVerbose > 3)
        ibis::util::logMessage
            ("bin::deprecatedJoin", "start processing a range-join ("
             "%s between %s - %g and %s + %g) with mask size %lu "
             "and %s explicit range constraint",
             col->name(), col->name(), delta, col->name(), delta,
             static_cast<long unsigned>(mask.cnt()), (range1 ? "an" : "no"));

    uint32_t il1=0, il2=0, iu2=0;
    uint32_t ilu=0, iuu=0; // bits[ilu:iuu] is summed in cumu
    uint32_t ilv=0, iuv=0; // bits[ilv:iuv] is summed in cumv

    uint32_t nbmax = nobs;
    if (range1 || range2) {
        double amin = (range1 ? range1->leftBound() : col->getActualMin());
        double amax = (range1 ? range1->rightBound() : col->getActualMax());
        il1 = bounds.find(amin);
        nbmax = bounds.find(amax);
        if (nbmax < nobs && minval[nbmax] <= amax)
            ++ nbmax;
    }
    il2 = il1;
    iu2 = il1;

    activate(il1, nbmax);
    ibis::bitvector cumu, cumv, curr;
    uint32_t tlast = time(0);
    for (; il1 < nbmax && il2 < nbmax; ++ il1) {
        while (il1 < nbmax && il2 < nbmax &&
               !(maxval[il1] >= minval[il1] &&
                 maxval[il2] >= minval[il2] &&
                 maxval[il1]+delta >= minval[il2] &&
                 maxval[il2]+delta >= minval[il1])) {
            if (maxval[il1] >= minval[il1] &&
                maxval[il2] >= minval[il2]) {
                if (!(maxval[il1]+delta >= minval[il2])) ++ il1;
                else ++ il2;
            }
            else {
                if (!(maxval[il1] >= minval[il1])) ++ il1;
                if (!(maxval[il2] >= minval[il2])) ++ il2;
            }
        }

        if (il1 < nbmax && il2 < nbmax && bits[il1] &&
            bits[il1]->cnt() > 0) { // found some overlap
            // minval[iu2] > maxval[il1+delta
            curr.copy(mask);
            curr &= *(bits[il1]);
            if (curr.cnt() == 0)
                continue;

            for (iu2 = il2+1; iu2 < nbmax &&
                     minval[iu2] <= maxval[il1]+delta; ++ iu2);
            uint32_t im2; // minval[im2] >= maxval[il1]-delta
            for (im2 = il2; im2 < nbmax &&
                     minval[im2] < maxval[il1]-delta; ++ im2);
            uint32_t in2; // maxval[in2] > minval[il1]+delta
            for (in2 = il2; in2 < nbmax &&
                     maxval[in2] <= minval[il1]+delta; ++ in2);
            if (im2 < in2) { // sure hits
                sumBins(im2, in2, cumv, ilv, iuv);
                ibis::bitvector tmp(mask);
                tmp &= cumv;
                ibis::util::outerProduct(curr, tmp, sure);
                ilv = im2;
                iuv = in2;
            }
            if (il2 < im2 || in2 < iu2) { // need to update iffy
                if (il2+1 == im2 && in2 == iu2) {
                    // only bits[il2]
                    ibis::bitvector tmp(mask);
                    tmp &= *(bits[il2]);
                    ibis::util::outerProduct(curr, tmp, iffy);
                }
                else if (il2 == im2 && in2+1 == iu2) {
                    // only bits[in2]
                    ibis::bitvector tmp(mask);
                    tmp &= *(bits[in2]);
                    ibis::util::outerProduct(*(bits[il1]), tmp, iffy);
                }
                else if (il2+1 == im2 && in2+1 == iu2) {
                    // only bits[il2] and idex.bits[in2]
                    ibis::bitvector tmp(*(bits[il2]));
                    tmp |= *(bits[in2]);
                    tmp &= mask;
                    ibis::util::outerProduct(*(bits[il1]), tmp, iffy);
                }
                else {
                    // need to put entries from multiple bins into iffy
                    if (ilu >= iuu ||
                        (in2 > im2 && (in2-im2) > (iu2-il2)/2 &&
                         (iuu < il2 ||
                          (iuu > il2 && (ilu <= il2 ? il2-ilu : ilu-il2) +
                           iu2 - iuu > (im2 - il2 + iu2 - in2))))) {
                        // copy cumv to cumu
                        cumu.copy(cumv);
                        ilu = ilv;
                        iuu = iuv;
                    }
                    sumBins(il2, iu2, cumu, ilu, iuu);
                    ibis::bitvector tmp(mask);
                    tmp &= cumu;
                    ibis::util::outerProduct(*(bits[il1]), tmp, iffy);
                    ilu = il2;
                    iuu = iu2;
                }
            }
        }

        if (ibis::gVerbose > 1) {
            uint32_t tcurr = time(0);
            if (tcurr-59 > tlast) {
                std::ostringstream ostmp;
                ostmp << "TIME(" << tcurr
                      << "): just completed processing bin " << il1
                      << " out of " << nbmax << ", sure.cnt()=" << sure.cnt()
                      << ", iffy.cnt()=" << iffy.cnt();
                ibis::util::logMessage("bin::deprecatedJoin", "%s",
                                       ostmp.str().c_str());
                tlast = tcurr;
            }
        }
    }
} // ibis::bin::deprecatedJoin

/// A range join on the same column with a complex distance function.  This
/// implementation relys on the fact that the expression @c expr is
/// monotonic in each bin.  Since the user does not know the exact bin
/// boundaries, this essentially require the express @c expr be a monotonic
/// function overall.  If this monotonicity is not satisfyied, this
/// function may miss some hits!
void ibis::bin::compJoin(const ibis::math::term *expr,
                         const ibis::bitvector& mask,
                         const ibis::qRange* const range1,
                         const ibis::qRange* const range2,
                         ibis::bitvector64& sure,
                         ibis::bitvector64& iffy) const {
    if (mask.cnt() == 0) {
        uint64_t np = mask.size();
        np *= np;
        sure.set(0, np);
        iffy.clear();
        return;
    }
    if (col == 0) return;

    ibis::index::barrel bar(const_cast<const ibis::math::term*>(expr));
    if (bar.size() == 0) {
        const double delta = fabs(expr->eval());
        if (delta > 0.0)
            deprecatedJoin(delta, mask, range1, range2, sure, iffy);
        else
            equiJoin(mask, range1, range2, sure, iffy);
        return;
    }
    else if (bar.size() != 1 && stricmp(bar.name(0), col->name()) != 0) {
        std::ostringstream ostr;
        ostr << *expr;
        col->logWarning("bin::compJoin", "failed to deal with complex range "
                        "expression %s", ostr.str().c_str());
        uint64_t npairs = static_cast<uint64_t>(nrows) * nrows;
        sure.set(0, npairs);
        iffy.set(1, npairs);
        return;
    }
    if (ibis::gVerbose > 3) {
        std::ostringstream ostr;
        ostr << *expr;
        ibis::util::logMessage
            ("bin::compJoin", "start processing a range join ("
             "%s between %s - %s and %s + %s) with mask size %lu and %s "
             "explicit constraint",
             col->name(), col->name(), ostr.str().c_str(),
             col->name(), ostr.str().c_str(),
             static_cast<long unsigned>(mask.size()),
             (range1 ? "an" : "no"));
    }

    uint32_t il1=0, il2=0, iu2=0;
    uint32_t ilu=0, iuu=0; // bits[ilu:iuu] is summed in cumu
    uint32_t ilv=0, iuv=0; // bits[ilv:iuv] is summed in cumv

    uint32_t nbmax = nobs;
    if (range1 || range2) {
        double amin = (range1 ? range1->leftBound() : col->getActualMin());
        double amax = (range1 ? range1->rightBound() : col->getActualMax());
        il1 = bounds.find(amin);
        nbmax = bounds.find(amax);
        if (nbmax < nobs && minval[nbmax] <= amax)
            ++ nbmax;
    }
    il2 = il1;
    iu2 = il1;
    activate(il1, nbmax);
    ibis::bitvector cumu, cumv, curr;
    uint32_t tlast = time(0);
    for (; il1 < nbmax && il2 < nbmax; ++ il1) {
        double delta=0;
        while (il1 < nbmax && il2 < nbmax) {
            if (! (maxval[il1] >= minval[il1])) {
                ++ il1;
            }
            else if (! (maxval[il2] >= minval[il2])) {
                ++ il2;
            }
            else { // compute the delta value
                bar.setValue(0, minval[il1]);
                delta = fabs(expr->eval());
                if (maxval[il1] != minval[il1]) {
                    bar.setValue(0, maxval[il1]);
                    double tmp = fabs(expr->eval());
                    if (tmp > delta)
                        delta = tmp;
                }
                if (maxval[il2] + delta >= minval[il1]) {
                    if (minval[il2] <= maxval[il1] + delta)
                        break;
                    else
                        ++ il1;
                }
                else if (minval[il2] <= maxval[il1]) {
                    ++ il2;
                }
                else {
                    ++ il1;
                    ++ il2;
                }
            }
        }

        if (il1 < nbmax && il2 < nbmax && bits[il1] &&
            bits[il1]->cnt() > 0) { // found some overlap
            curr.copy(mask);
            curr &= *(bits[il1]);
            if (curr.cnt() == 0)
                continue;

            if (minval[il1] == maxval[il1]) {
                uint32_t im2; // minval[im2] >= maxval[il1]-delta
                for (im2 = il2+1; im2 < nbmax &&
                         minval[im2] < maxval[il1]-delta; ++ im2);
                uint32_t in2; // maxval[in2] > minval[il1]+delta
                for (in2 = il2+1; in2 < nbmax &&
                         maxval[in2] <= minval[il1]+delta; ++ in2);
                if (im2 < in2) { // sure hits
                    sumBins(im2, in2, cumv, ilv, iuv);
                    ibis::bitvector tmp(mask);
                    tmp &= cumv;
                    ibis::util::outerProduct(curr, tmp, sure);
                    ilv = im2;
                    iuv = in2;
                }
            }
            // minval[iu2] > maxval[il1+delta
            for (iu2 = il2+1; iu2 < nbmax &&
                     minval[iu2] <= maxval[il1]+delta; ++ iu2);
            // not exactly sure, put the result in iffy
            sumBins(il2, iu2, cumu, ilu, iuu);
            ibis::bitvector cmk(mask);
            cmk &= cumu;
            ibis::util::outerProduct(curr, cmk, iffy);
            ilu = il2;
            iuu = iu2;
        }

        if (ibis::gVerbose > 1) {
            uint32_t tcurr = time(0);
            if (tcurr-59 > tlast) {
                std::ostringstream ostmp;
                ostmp << "TIME(" << tcurr
                      << "): just completed processing bin " << il1
                      << " out of " << nbmax << ", sure.cnt()=" << sure.cnt()
                      << ", iffy.cnt()=" << iffy.cnt();
                ibis::util::logMessage("bin::compJoin", "%s",
                                       ostmp.str().c_str());
                tlast = tcurr;
            }
        }
    }
} // ibis::bin::compJoin

int64_t ibis::bin::equiJoin(const ibis::bitvector& mask,
                            const ibis::qRange* const range1,
                            const ibis::qRange* const range2) const {
    int64_t cnt = 0;
    if (mask.cnt() == 0)
        return cnt;
    if (col == 0) return -1;

    uint32_t il1=0, il2=0, iu2=0;
    uint32_t ilc=0, iuc=0; // bits[ilc:iuc] is summed in cumu
    ibis::bitvector cumu, curr;
    if (ibis::gVerbose > 3)
        ibis::util::logMessage
            ("bin::equiJoin", "start processing an equi-join "
             "between %s and %s with mask size %lu and %s range constraint",
             col->name(), col->name(), static_cast<long unsigned>(mask.cnt()),
             (range1 ? "an" : "no"));

    uint32_t nbmax = nobs;
    if (range1 || range2) {
        double amin = (range1 ? range1->leftBound() : col->getActualMin());
        double tmp = (range2 ? range2->leftBound() : amin);
        if (amin < tmp)
            amin = tmp;
        double amax = (range1 ? range1->rightBound() : col->getActualMax());
        tmp = (range2 ? range2->rightBound() : amax);
        if (amax > tmp)
            amax = tmp;
        il1 = bounds.find(amin);
        nbmax = bounds.find(amax);
        if (nbmax < nobs && minval[nbmax] <= amax)
            ++ nbmax;
    }
    il2 = il1;
    iu2 = il1;

    activate(il1, nbmax);
    uint32_t tlast = time(0);
    while (il1 < nbmax && il2 < nbmax) {
        while (il1 < nbmax && il2 < nbmax &&
               !(maxval[il1] >= minval[il1] &&
                 maxval[il2] >= minval[il2] &&
                 maxval[il1] >= minval[il2] &&
                 maxval[il2] >= minval[il1])) {
            if (maxval[il1] >= minval[il1] &&
                maxval[il2] >= minval[il2]) {
                if (!(maxval[il1] >= minval[il2])) ++ il1;
                else ++ il2;
            }
            else {
                if (!(maxval[il1] >= minval[il1])) ++ il1;
                if (!(maxval[il2] >= minval[il2])) ++ il2;
            }
        }

        if (il1 < nbmax && il2 < nbmax && bits[il1] &&
            bits[il1]->cnt() > 0) { // found some overlap
            curr.copy(mask);
            curr &= *(bits[il1]);
            if (curr.cnt() > 0) {
                for (iu2 = il2+1; iu2 < nbmax &&
                         minval[iu2] <= maxval[il1]; ++ iu2);
                sumBins(il2, iu2, cumu, ilc, iuc);
                ibis::bitvector tmp(mask);
                tmp &= cumu;
                cnt += curr.cnt() * tmp.cnt();
                ilc = il2;
                iuc = iu2;
            }
        }

        if (ibis::gVerbose > 1) {
            uint32_t tcurr = time(0);
            if (tcurr-59 > tlast) {
                std::ostringstream ostmp;
                ostmp << "TIME(" << tcurr
                      << "): just completed processing bin " << il1
                      << " out of " << nbmax << ", current count ="
                      << cnt;
                ibis::util::logMessage("bin::equiJoin", "%s",
                                       ostmp.str().c_str());
                tlast = tcurr;
            }
        }
        ++ il1;
    }
    return cnt;
} // ibis::bin::equiJoin

// A range join on the same variable.
int64_t ibis::bin::deprecatedJoin(const double& delta,
                                  const ibis::bitvector& mask,
                                  const ibis::qRange* const range1,
                                  const ibis::qRange* const range2) const {
    int64_t cnt = 0;
    if (mask.cnt() == 0)
        return cnt;
    if (delta <= 0.0) {
        return equiJoin(mask, range1, range2);
    }
    if (col == 0) return -1;

    if (ibis::gVerbose > 3)
        ibis::util::logMessage
            ("bin::deprecatedJoin", "start processing a range-join ("
             "%s between %s - %g and %s + %g) with mask size %lu and %s "
             "range constraint",
             col->name(), col->name(), delta, col->name(), delta,
             static_cast<long unsigned>(mask.cnt()), (range1 ? "an" : "no"));

    uint32_t il1=0, il2=0, iu2=0;
    uint32_t ilu=0, iuu=0; // bits[ilu:iuu] is summed in cumu

    uint32_t nbmax = nobs;
    if (range1 || range2) {
        double amin = (range1 ? range1->leftBound() : col->getActualMin());
        double amax = (range1 ? range1->rightBound() : col->getActualMax());
        il1 = bounds.find(amin);
        nbmax = bounds.find(amax);
        if (nbmax < nobs && minval[nbmax] <= amax)
            ++ nbmax;
    }
    il2 = il1;
    iu2 = il1;

    activate(il1, nbmax);
    ibis::bitvector cumu, curr;
    uint32_t tlast = time(0);
    for (; il1 < nbmax && il2 < nbmax; ++ il1) {
        while (il1 < nbmax && il2 < nbmax &&
               !(maxval[il1] >= minval[il1] &&
                 maxval[il2] >= minval[il2] &&
                 maxval[il1]+delta >= minval[il2] &&
                 maxval[il2]+delta >= minval[il1])) {
            if (maxval[il1] >= minval[il1] &&
                maxval[il2] >= minval[il2]) {
                if (!(maxval[il1]+delta >= minval[il2])) ++ il1;
                else ++ il2;
            }
            else {
                if (!(maxval[il1] >= minval[il1])) ++ il1;
                if (!(maxval[il2] >= minval[il2])) ++ il2;
            }
        }

        if (il1 < nbmax && il2 < nbmax && bits[il1] &&
            bits[il1]->cnt() > 0) { // found some overlap
            // minval[iu2] > maxval[il1+delta
            curr.copy(mask);
            curr &= *(bits[il1]);
            if (curr.cnt() == 0)
                continue;

            for (iu2 = il2+1; iu2 < nbmax &&
                     minval[iu2] <= maxval[il1]+delta; ++ iu2);
            sumBins(il2, iu2, cumu, ilu, iuu);
            ibis::bitvector tmp(mask);
            tmp &= cumu;
            cnt += tmp.cnt() * curr.cnt();
            ilu = il2;
            iuu = iu2;
        }

        if (ibis::gVerbose > 1) {
            uint32_t tcurr = time(0);
            if (tcurr-59 > tlast) {
                std::ostringstream ostmp;
                ostmp << "TIME(" << tcurr
                      << "): just completed processing bin " << il1
                      << " out of " << nbmax << ", current count="
                      << cnt;
                ibis::util::logMessage("bin::deprecatedJoin", "%s",
                                       ostmp.str().c_str());
                tlast = tcurr;
            }
        }
    }
    return cnt;
} // ibis::bin::deprecatedJoin

/// A range join on the same column with a complex distance function.  This
/// implementation relys on the fact that the expression @c expr is
/// monotonic in each bin.  Since the user does not know the exact bin
/// boundaries, this essentially require the express @c expr be a monotonic
/// function overall.  If this monotonicity is not satisfyied, this
/// function may miss some hits!
int64_t ibis::bin::compJoin(const ibis::math::term *expr,
                            const ibis::bitvector& mask,
                            const ibis::qRange* const range1,
                            const ibis::qRange* const range2) const {
    int64_t cnt = 0;
    if (mask.cnt() == 0)
        return cnt;
    if (col == 0) return -1;

    ibis::index::barrel bar(const_cast<const ibis::math::term*>(expr));
    if (bar.size() == 0) {
        const double delta = fabs(expr->eval());
        if (delta > 0.0)
            return deprecatedJoin(delta, mask, range1, range2);
        else
            return equiJoin(mask, range1, range2);
    }
    else if (bar.size() != 1 && stricmp(bar.name(0), col->name()) != 0) {
        std::ostringstream ostr;
        ostr << *expr;
        col->logWarning("bin::compJoin", "failed to deal with complex range "
                        "expression %s", ostr.str().c_str());
        return -1;
    }
    if (ibis::gVerbose > 3) {
        std::ostringstream ostr;
        ostr << *expr;
        ibis::util::logMessage
            ("bin::compJoin", "start processing a range join ("
             "%s between %s - %s and %s + %s) with mask size %lu and %s "
             "range constraint",
             col->name(), col->name(), ostr.str().c_str(),
             col->name(), ostr.str().c_str(),
             static_cast<long unsigned>(mask.size()),
             (range1 ? "an" : "no"));
    }

    uint32_t il1=0, il2=0, iu2=0;
    uint32_t ilu=0, iuu=0; // bits[ilu:iuu] is summed in cumu

    uint32_t nbmax = nobs;
    if (range1 || range2) {
        double amin = (range1 ? range1->leftBound() : col->getActualMin());
        double amax = (range1 ? range1->rightBound() : col->getActualMax());
        il1 = bounds.find(amin);
        nbmax = bounds.find(amax);
        if (nbmax < nobs && minval[nbmax] <= amax)
            ++ nbmax;
    }
    activate(il1, nbmax);
    il2 = il1;
    iu2 = il1;
    ibis::bitvector cumu, curr;
    uint32_t tlast = time(0);
    for (; il1 < nbmax && il2 < nbmax; ++ il1) {
        double delta=0;
        while (il1 < nbmax && il2 < nbmax) {
            if (! (maxval[il1] >= minval[il1])) {
                ++ il1;
            }
            else if (! (maxval[il2] >= minval[il2])) {
                ++ il2;
            }
            else { // compute the delta value
                bar.setValue(0, minval[il1]);
                delta = fabs(expr->eval());
                if (maxval[il1] != minval[il1]) {
                    bar.setValue(0, maxval[il1]);
                    double tmp = fabs(expr->eval());
                    if (tmp > delta)
                        delta = tmp;
                }
                if (maxval[il2] + delta >= minval[il1]) {
                    if (minval[il2] <= maxval[il1] + delta)
                        break;
                    else
                        ++ il1;
                }
                else if (minval[il2] <= maxval[il1]) {
                    ++ il2;
                }
                else {
                    ++ il1;
                    ++ il2;
                }
            }
        }

        if (il1 < nbmax && il2 < nbmax && bits[il1] &&
            bits[il1]->cnt() > 0) { // found some overlap
            curr.copy(mask);
            curr &= *(bits[il1]);
            if (curr.cnt() == 0)
                continue;

            // minval[iu2] > maxval[il1+delta
            for (iu2 = il2+1; iu2 < nbmax &&
                     minval[iu2] <= maxval[il1]+delta; ++ iu2);
            // not exactly sure, put the result in iffy
            sumBins(il2, iu2, cumu, ilu, iuu);
            ibis::bitvector tmp(mask);
            tmp &= cumu;
            cnt += tmp.cnt() * curr.cnt();
            ilu = il2;
            iuu = iu2;
        }

        if (ibis::gVerbose > 1) {
            uint32_t tcurr = time(0);
            if (tcurr-59 > tlast) {
                std::ostringstream ostmp;
                ostmp << "TIME(" << tcurr
                      << "): just completed processing bin " << il1
                      << " out of " << nbmax << ", current count="
                      << cnt;
                ibis::util::logMessage("bin::compJoin", "%s",
                                       ostmp.str().c_str());
                tlast = tcurr;
            }
        }
    }
    return cnt;
} // ibis::bin::compJoin

/// An equi-join on two different columns.
void ibis::bin::equiJoin(const ibis::bin& idx2,
                         const ibis::bitvector& mask,
                         ibis::bitvector64& sure,
                         ibis::bitvector64& iffy) const {
    if (col == 0) return;
    if (ibis::gVerbose > 3)
        ibis::util::logMessage
            ("bin::equiJoin", "start processing an equi-join "
             "between %s and %s with mask size %lu", col->name(),
             idx2.col->name(), static_cast<long unsigned>(mask.cnt()));

    uint32_t il1=0, il2=0, iu2=0;
    uint32_t ilc=0, iuc=0; // idx2.bits[ilc:iuc] is summed in cumu
    ibis::bitvector cumu, curr;
    uint32_t tlast = time(0);
    for (; il1 < nobs && il2 < idx2.nobs; ++ il1) {
        while (il1 < nobs && il2 < idx2.nobs &&
               !(maxval[il1] >= minval[il1] &&
                 idx2.maxval[il2] >= idx2.minval[il2] &&
                 maxval[il1] >= idx2.minval[il2] &&
                 idx2.maxval[il2] >= minval[il1])) {
            if (maxval[il1] >= minval[il1] &&
                idx2.maxval[il2] >= idx2.minval[il2]) {
                if (!(maxval[il1] >= idx2.minval[il2])) ++ il1;
                else ++ il2;
            }
            else {
                if (!(maxval[il1] >= minval[il1])) ++ il1;
                if (!(idx2.maxval[il2] >= idx2.minval[il2])) ++ il2;
            }
        }

        if (il1 < nobs && il2 < idx2.nobs && bits[il1] &&
            bits[il1]->cnt() > 0) { // found some overlap
            curr.copy(mask);
            curr &= *(bits[il1]);
            if (curr.cnt() == 0)
                continue;

            if (minval[il1] == maxval[il1] &&
                minval[il1] == idx2.minval[il2] &&
                minval[il1] == idx2.maxval[il2] ) {
                // exactly equal, record the pairs in @c sure
                ibis::bitvector tmp(*(idx2.bits[il2]));
                tmp &= mask;
                if (tmp.cnt() > 0)
                    ibis::util::outerProduct(curr, tmp, sure);
            }
            else { // not exactly sure, put the result in iffy
                // bins [il2, iu2) from idx2 overlaps with bin il1 of *this
                for (iu2 = il2+1; iu2 < idx2.nobs &&
                         idx2.minval[iu2] <= maxval[il1]; ++ iu2);
                sumBins(il2, iu2, cumu, ilc, iuc);
                ibis::bitvector tmp(mask);
                tmp &= cumu;
                ibis::util::outerProduct(curr, tmp, iffy);
                ilc = il2;
                iuc = iu2;
            }
        }

        if (ibis::gVerbose > 1) {
            uint32_t tcurr = time(0);
            if (tcurr-59 > tlast) {
                std::ostringstream ostmp;
                ostmp << "TIME(" << tcurr
                      << "): just completed processing bin " << il1
                      << " out of " << nobs << ", sure.cnt()=" << sure.cnt()
                      << ", iffy.cnt()=" << iffy.cnt();
                ibis::util::logMessage("bin::equiJoin", "%s",
                                       ostmp.str().c_str());
                tlast = tcurr;
            }
        }
    }
} // ibis::bin::equiJoin

/// A range join on two different columns.
void ibis::bin::deprecatedJoin(const ibis::bin& idx2,
                               const double& delta,
                               const ibis::bitvector& mask,
                               ibis::bitvector64& sure,
                               ibis::bitvector64& iffy) const {
    if (col == 0) return;
    if (ibis::gVerbose > 3)
        ibis::util::logMessage
            ("bin::deprecatedJoin", "start processing a range-join ("
             "%s between %s - %g and %s + %g) with mask size %lu",
             col->name(), idx2.col->name(), delta,
             idx2.col->name(), delta, static_cast<long unsigned>(mask.cnt()));

    uint32_t il1=0, il2=0, iu2=0;
    uint32_t ilu=0, iuu=0; // idx2.bits[ilu:iuu] is summed in cumu
    uint32_t ilv=0, iuv=0; // idx2.bits[ilv:iuv] is summed in cumv
    ibis::bitvector cumu, cumv, curr;
    uint32_t tlast = time(0);
    for (; il1 < nobs && il2 < idx2.nobs; ++ il1) {
        while (il1 < nobs && il2 < idx2.nobs &&
               !(maxval[il1] >= minval[il1] &&
                 idx2.maxval[il2] >= idx2.minval[il2] &&
                 maxval[il1]+delta >= idx2.minval[il2] &&
                 idx2.maxval[il2]+delta >= minval[il1])) {
            if (maxval[il1] >= minval[il1] &&
                idx2.maxval[il2] >= idx2.minval[il2]) {
                if (!(maxval[il1]+delta >= idx2.minval[il2])) ++ il1;
                else ++ il2;
            }
            else {
                if (!(maxval[il1] >= minval[il1])) ++ il1;
                if (!(idx2.maxval[il2] >= idx2.minval[il2])) ++ il2;
            }
        }

        if (il1 < nobs && il2 < idx2.nobs && bits[il1] &&
            bits[il1]->cnt() > 0) { // found some overlap
            // idx2.minval[iu2] > maxval[il1+delta
            curr.copy(mask);
            curr &= *(bits[il1]);
            if (curr.cnt() == 0)
                continue;

            for (iu2 = il2+1; iu2 < idx2.nobs &&
                     idx2.minval[iu2] <= maxval[il1]+delta; ++ iu2);
            uint32_t im2; // idx2.minval[im2] >= maxval[il1]-delta
            for (im2 = il2; im2 < idx2.nobs &&
                     idx2.minval[im2] < maxval[il1]-delta; ++ im2);
            uint32_t in2; // idx2.maxval[in2] > minval[il1]+delta
            for (in2 = il2; in2 < idx2.nobs &&
                     idx2.maxval[in2] <= minval[il1]+delta; ++ in2);
            if (im2 < in2) { // sure hits
                idx2.sumBins(im2, in2, cumv, ilv, iuv);
                ibis::bitvector tmp(mask);
                tmp &= cumv;
                ibis::util::outerProduct(curr, tmp, sure);
                ilv = im2;
                iuv = in2;
            }
            if (il2 < im2 || in2 < iu2) { // need to update iffy
                if (il2+1 == im2 && in2 == iu2) {
                    // only idx2.bits[il2]
                    ibis::bitvector tmp(*(idx2.bits[il2]));
                    tmp &= mask;
                    ibis::util::outerProduct(curr, tmp, iffy);
                }
                else if (il2 == im2 && in2+1 == iu2) {
                    // only idx2.bits[in2]
                    ibis::bitvector tmp(*(idx2.bits[in2]));
                    tmp &= mask;
                    ibis::util::outerProduct(curr, tmp, iffy);
                }
                else if (il2+1 == im2 && in2+1 == iu2) {
                    // only idx2.bits[il2] and idex.bits[in2]
                    ibis::bitvector tmp(*(idx2.bits[il2]));
                    tmp |= *(idx2.bits[in2]);
                    tmp &= mask;
                    ibis::util::outerProduct(curr, tmp, iffy);
                }
                else {
                    // need to put entries from multiple bins into iffy
                    if (ilu >= iuu ||
                        (in2 > im2 && (in2-im2) > (iu2-il2)/2 &&
                         (iuu < il2 ||
                          (iuu > il2 && (ilu <= il2 ? il2-ilu : ilu-il2) +
                           iu2 - iuu > (im2 - il2 + iu2 - in2))))) {
                        // copy cumv to cumu
                        cumu.copy(cumv);
                        ilu = ilv;
                        iuu = iuv;
                    }
                    idx2.sumBins(il2, iu2, cumu, ilu, iuu);
                    ibis::bitvector tmp(mask);
                    tmp &= cumu;
                    ibis::util::outerProduct(curr, tmp, iffy);
                    ilu = il2;
                    iuu = iu2;
                }
            }
        }

        if (ibis::gVerbose > 1) {
            uint32_t tcurr = time(0);
            if (tcurr-59 > tlast) {
                std::ostringstream ostmp;
                ostmp << "TIME(" << tcurr
                      << "): just completed processing bin " << il1
                      << " out of " << nobs << ", sure.cnt()=" << sure.cnt()
                      << ", iffy.cnt()=" << iffy.cnt();
                ibis::util::logMessage("bin::deprecatedJoin", "%s",
                                       ostmp.str().c_str());
                tlast = tcurr;
            }
        }
    }
} // ibis::bin::deprecatedJoin

/// A range join between two different columns with a complex distance
/// function.  This implementation relys on the fact that the expression @c
/// expr is monotonic in each bin.  Since the user does not know the exact
/// bin boundaries, this essentially require the express @c expr be a
/// monotonic function overall.  If this monotonicity is not satisfyied,
/// this function may miss some hits!
void ibis::bin::compJoin(const ibis::bin& idx2,
                         const ibis::math::term *expr,
                         const ibis::bitvector& mask,
                         ibis::bitvector64& sure,
                         ibis::bitvector64& iffy) const {
    if (col == 0) return;
    ibis::index::barrel bar(const_cast<const ibis::math::term*>(expr));
    if (bar.size() == 0) {
        const double delta = fabs(expr->eval());
        if (delta > 0.0)
            deprecatedJoin(idx2, delta, sure, iffy);
        else
            equiJoin(idx2, sure, iffy);
        return;
    }
    else if (bar.size() > 1 || stricmp(bar.name(0), col->name()) != 0) {
        std::ostringstream ostr;
        ostr << *expr;
        col->logWarning("bin::compJoin", "failed to deal with complex range "
                        "expression %s", ostr.str().c_str());
        uint64_t npairs = static_cast<uint64_t>(nrows) * nrows;
        sure.set(0, npairs);
        iffy.set(1, npairs);
        return;
    }
    if (ibis::gVerbose > 3) {
        std::ostringstream ostr;
        ostr << *expr;
        ibis::util::logMessage
            ("bin::compJoin", "start processing a range join ("
             "%s between %s - %s and %s + %s) with mask size %lu",
             col->name(), idx2.col->name(), ostr.str().c_str(),
             idx2.col->name(), ostr.str().c_str(),
             static_cast<long unsigned>(mask.cnt()));
    }

    uint32_t il1=0, il2=0, iu2=0;
    uint32_t ilu=0, iuu=0; // idx2.bits[ilu:iuu] is summed in cumu
    uint32_t ilv=0, iuv=0; // idx2.bits[ilv:iuv] is summed in cumv
    ibis::bitvector cumu, cumv, curr;
    uint32_t tlast = time(0);
    for (; il1 < nobs && il2 < idx2.nobs; ++ il1) {
        double delta=0;
        while (il1 < nobs && il2 < idx2.nobs) {
            if (! (maxval[il1] >= minval[il1])) {
                ++ il1;
            }
            else if (! (idx2.maxval[il2] >= idx2.minval[il2])) {
                ++ il2;
            }
            else { // compute the delta value
                bar.setValue(0, minval[il1]);
                delta = fabs(expr->eval());
                if (maxval[il1] != minval[il1]) {
                    bar.setValue(0, maxval[il1]);
                    double tmp = fabs(expr->eval());
                    if (tmp > delta)
                        delta = tmp;
                }
                if (idx2.maxval[il2] + delta >= minval[il1]) {
                    if (idx2.minval[il2] <= maxval[il1] + delta)
                        break;
                    else
                        ++ il1;
                }
                else if (idx2.minval[il2] <= maxval[il1]) {
                    ++ il2;
                }
                else {
                    ++ il1;
                    ++ il2;
                }
            }
        }

        if (il1 < nobs && il2 < idx2.nobs && bits[il1] &&
            bits[il1]->cnt() > 0) { // found some overlap
            curr.copy(mask);
            curr &= *(bits[il1]);
            if (curr.cnt() == 0)
                continue;

            if (minval[il1] == maxval[il1]) {
                uint32_t im2; // idx2.minval[im2] >= maxval[il1]-delta
                for (im2 = il2+1; im2 < idx2.nobs &&
                         idx2.minval[im2] < maxval[il1]-delta; ++ im2);
                uint32_t in2; // idx2.maxval[in2] > minval[il1]+delta
                for (in2 = il2+1; in2 < idx2.nobs &&
                         idx2.maxval[in2] <= minval[il1]+delta; ++ in2);
                if (im2 < in2) { // sure hits
                    sumBins(im2, in2, cumv, ilv, iuv);
                    ibis::bitvector tmp(mask);
                    tmp &= cumv;
                    ibis::util::outerProduct(curr, tmp, sure);
                    ilv = im2;
                    iuv = in2;
                }
            }
            // idx2.minval[iu2] > maxval[il1+delta
            for (iu2 = il2+1; iu2 < idx2.nobs &&
                     idx2.minval[iu2] <= maxval[il1]+delta; ++ iu2);
            // not exactly sure, put the result in iffy
            sumBins(il2, iu2, cumu, ilu, iuu);
            ibis::bitvector cmk(mask);
            cmk &= cumu;
            ibis::util::outerProduct(curr, cmk, iffy);
            ilu = il2;
            iuu = iu2;
        }

        if (ibis::gVerbose > 1) {
            uint32_t tcurr = time(0);
            if (tcurr-59 > tlast) {
                std::ostringstream ostmp;
                ostmp << "TIME(" << tcurr
                      << "): just completed processing bin " << il1
                      << " out of " << nobs << ", sure.cnt()=" << sure.cnt()
                      << ", iffy.cnt()=" << iffy.cnt();
                ibis::util::logMessage("bin::compJoin", "%s",
                                       ostmp.str().c_str());
                tlast = tcurr;
            }
        }
    }
} // ibis::bin::compJoin

/// An equi-join on two different columns.
void ibis::bin::equiJoin(const ibis::bin& idx2,
                         const ibis::bitvector& mask,
                         const ibis::qRange* const range1,
                         const ibis::qRange* const range2,
                         ibis::bitvector64& sure,
                         ibis::bitvector64& iffy) const {
    if (col == 0) return;
    if (mask.cnt() == 0) {
        uint64_t np = mask.size();
        np *= np;
        sure.set(0, np);
        iffy.clear();
        return;
    }
    if (ibis::gVerbose > 3)
        ibis::util::logMessage
            ("bin::equiJoin", "start processing an equi-join "
             "between %s and %s with mask size %lu", col->name(),
             idx2.col->name(), static_cast<long unsigned>(mask.cnt()));

    uint32_t il1=0, il2=0, iu2=0;
    uint32_t ilc=0, iuc=0; // idx2.bits[ilc:iuc] is summed in cumu

    uint32_t nb1max = nobs;
    uint32_t nb2max = idx2.nobs;
    if (range1 || range2) {
        double amin = (range1 ? range1->leftBound() : col->getActualMin());
        double tmp = (range2 ? range2->leftBound() :
                      idx2.col->getActualMin());
        if (amin < tmp)
            amin = tmp;
        double amax = (range1 ? range1->rightBound() : col->getActualMax());
        tmp = (range2 ? range2->rightBound() : idx2.col->getActualMax());
        if (amax > tmp)
            amax = tmp;
        il1 = bounds.find(amin);
        nb1max = bounds.find(amax);
        if (nb1max < nobs && minval[nb1max] <= amax)
            ++ nb1max;
        il2 = idx2.bounds.find(amin);
        nb2max = bounds.find(amax);
        if (nb2max < idx2.nobs && idx2.minval[nb2max] <= amax)
            ++ nb2max;
    }
    idx2.activate(il2, nb2max);
    activate(il1, nb1max);
    iu2 = il2;
    ibis::bitvector cumu, curr;
    uint32_t tlast = time(0);
    for (;il1 < nb1max && il2 < nb2max; ++ il1) {
        while (il1 < nb1max && il2 < nb2max &&
               !(maxval[il1] >= minval[il1] &&
                 idx2.maxval[il2] >= idx2.minval[il2] &&
                 maxval[il1] >= idx2.minval[il2] &&
                 idx2.maxval[il2] >= minval[il1])) {
            if (maxval[il1] >= minval[il1] &&
                idx2.maxval[il2] >= idx2.minval[il2]) {
                if (!(maxval[il1] >= idx2.minval[il2])) ++ il1;
                else ++ il2;
            }
            else {
                if (!(maxval[il1] >= minval[il1])) ++ il1;
                if (!(idx2.maxval[il2] >= idx2.minval[il2])) ++ il2;
            }
        }

        if (il1 < nb1max && il2 < nb2max && bits[il1] &&
            bits[il1]->cnt() > 0) { // found some overlap
            curr.copy(mask);
            curr &= *(bits[il1]);
            if (curr.cnt() == 0)
                continue;

            if (minval[il1] == maxval[il1] &&
                minval[il1] == idx2.minval[il2] &&
                minval[il1] == idx2.maxval[il2]) {
                if ((range1 == 0 || range1->inRange(minval[il1])) &&
                    (range2 == 0 || range2->inRange(minval[il1]))) {
                    // exactly equal, record the pairs in @c sure
                    ibis::bitvector tmp(*(idx2.bits[il2]));
                    tmp &= mask;
                    if (tmp.cnt() > 0)
                        ibis::util::outerProduct(curr, tmp, sure);
                }
            }
            else { // not exactly sure, put the result in iffy
                // bins [il2, iu2) from idx2 overlaps with bin il1 of *this
                for (iu2 = il2+1; iu2 < nb2max &&
                         idx2.minval[iu2] <= maxval[il1]; ++ iu2);
                sumBins(il2, iu2, cumu, ilc, iuc);
                ibis::bitvector tmp(mask);
                tmp &= cumu;
                ibis::util::outerProduct(curr, tmp, iffy);
                ilc = il2;
                iuc = iu2;
            }
        }

        if (ibis::gVerbose > 1) {
            uint32_t tcurr = time(0);
            if (tcurr-59 > tlast) {
                std::ostringstream ostmp;
                ostmp << "TIME(" << tcurr
                      << "): just completed processing bin " << il1
                      << " out of " << nb1max << ", sure.cnt()="
                      << sure.cnt() << ", iffy.cnt()=" << iffy.cnt();
                ibis::util::logMessage("bin::equiJoin", "%s",
                                       ostmp.str().c_str());
                tlast = tcurr;
            }
        }
    }
} // ibis::bin::equiJoin

/// A range join on two different columns.
void ibis::bin::deprecatedJoin(const ibis::bin& idx2,
                               const double& delta,
                               const ibis::bitvector& mask,
                               const ibis::qRange* const range1,
                               const ibis::qRange* const range2,
                               ibis::bitvector64& sure,
                               ibis::bitvector64& iffy) const {
    if (mask.cnt() == 0) {
        uint64_t np = mask.size();
        np *= np;
        sure.set(0, np);
        iffy.clear();
        return;
    }
    if (col == 0) return;
    if (ibis::gVerbose > 3)
        ibis::util::logMessage
            ("bin::deprecatedJoin", "start processing a range-join ("
             "%s between %s - %g and %s + %g) with mask size %lu",
             col->name(), idx2.col->name(), delta,
             idx2.col->name(), delta, static_cast<long unsigned>(mask.cnt()));

    uint32_t il1=0, il2=0, iu2=0;
    uint32_t ilu=0, iuu=0; // idx2.bits[ilu:iuu] is summed in cumu
    uint32_t ilv=0, iuv=0; // idx2.bits[ilv:iuv] is summed in cumv

    uint32_t nb1max = nobs;
    uint32_t nb2max = idx2.nobs;
    if (range1 || range2) {
        double amin1 = (range1 ? range1->leftBound() : col->getActualMin());
        double amin2 = (range2 ? range2->leftBound() :
                        idx2.col->getActualMin());
        double amax1 = (range1 ? range1->rightBound() : col->getActualMax());
        double amax2 = (range2 ? range2->rightBound() :
                        idx2.col->getActualMax());
        double tmp = (amin1 >= amin2-delta ? amin1 : amin2-delta);
        il1 = bounds.find(tmp);
        tmp = (amax1 <= amax2+delta ? amax1 : amax2+delta);
        nb1max = bounds.find(tmp);
        if (nb1max < nobs && minval[nb1max] <= tmp)
            ++ nb1max;

        tmp = (amin2 >= amin1-delta ? amin2 : amin2-delta);
        il2 = idx2.bounds.find(tmp);
        tmp = (amax2 <= amax1+delta ? amax2 : amax1+delta);
        nb2max = bounds.find(tmp);
        if (nb2max < idx2.nobs && idx2.minval[nb2max] <= tmp)
            ++ nb2max;
    }
    idx2.activate(il2, nb2max);
    activate(il1, nb1max);
    iu2 = il2;
    ibis::bitvector cumu, cumv, curr;
    uint32_t tlast = time(0);
    for (; il1 < nb1max && il2 < nb2max; ++ il1) {
        while (il1 < nb1max && il2 < nb2max &&
               !(maxval[il1] >= minval[il1] &&
                 idx2.maxval[il2] >= idx2.minval[il2] &&
                 maxval[il1]+delta >= idx2.minval[il2] &&
                 idx2.maxval[il2]+delta >= minval[il1])) {
            if (maxval[il1] >= minval[il1] &&
                idx2.maxval[il2] >= idx2.minval[il2]) {
                if (!(maxval[il1]+delta >= idx2.minval[il2])) ++ il1;
                else ++ il2;
            }
            else {
                if (!(maxval[il1] >= minval[il1])) ++ il1;
                if (!(idx2.maxval[il2] >= idx2.minval[il2])) ++ il2;
            }
        }

        if (il1 < nb1max && il2 < nb2max && bits[il1] &&
            bits[il1]->cnt() > 0) { // found some overlap
            // idx2.minval[iu2] > maxval[il1+delta
            curr.copy(mask);
            curr &= *(bits[il1]);
            if (curr.cnt() == 0)
                continue;

            for (iu2 = il2+1; iu2 < nb2max &&
                     idx2.minval[iu2] <= maxval[il1]+delta; ++ iu2);
            uint32_t im2; // idx2.minval[im2] >= maxval[il1]-delta
            for (im2 = il2; im2 < nb2max &&
                     idx2.minval[im2] < maxval[il1]-delta; ++ im2);
            uint32_t in2; // idx2.maxval[in2] > minval[il1]+delta
            for (in2 = il2; in2 < nb2max &&
                     idx2.maxval[in2] <= minval[il1]+delta; ++ in2);
            if (im2 < in2) { // sure hits
                idx2.sumBins(im2, in2, cumv, ilv, iuv);
                ibis::bitvector tmp(mask);
                tmp &= cumv;
                ibis::util::outerProduct(curr, tmp, sure);
                ilv = im2;
                iuv = in2;
            }
            if (il2 < im2 || in2 < iu2) { // need to update iffy
                if (il2+1 == im2 && in2 == iu2) {
                    // only idx2.bits[il2]
                    ibis::bitvector tmp(*(idx2.bits[il2]));
                    tmp &= mask;
                    ibis::util::outerProduct(curr, tmp, iffy);
                }
                else if (il2 == im2 && in2+1 == iu2) {
                    // only idx2.bits[in2]
                    ibis::bitvector tmp(*(idx2.bits[in2]));
                    tmp &= mask;
                    ibis::util::outerProduct(curr, tmp, iffy);
                }
                else if (il2+1 == im2 && in2+1 == iu2) {
                    // only idx2.bits[il2] and idex.bits[in2]
                    ibis::bitvector tmp(*(idx2.bits[il2]));
                    tmp |= *(idx2.bits[in2]);
                    tmp &= mask;
                    ibis::util::outerProduct(curr, tmp, iffy);
                }
                else {
                    // need to put entries from multiple bins into iffy
                    if (ilu >= iuu ||
                        (in2 > im2 && (in2-im2) > (iu2-il2)/2 &&
                         (iuu < il2 ||
                          (iuu > il2 && (ilu <= il2 ? il2-ilu : ilu-il2) +
                           iu2 - iuu > (im2 - il2 + iu2 - in2))))) {
                        // copy cumv to cumu
                        cumu.copy(cumv);
                        ilu = ilv;
                        iuu = iuv;
                    }
                    idx2.sumBins(il2, iu2, cumu, ilu, iuu);
                    ibis::bitvector tmp(mask);
                    tmp &= cumu;
                    ibis::util::outerProduct(curr, tmp, iffy);
                    ilu = il2;
                    iuu = iu2;
                }
            }
        }

        if (ibis::gVerbose > 1) {
            uint32_t tcurr = time(0);
            if (tcurr-59 > tlast) {
                std::ostringstream ostmp;
                ostmp << "TIME(" << tcurr
                      << "): just completed processing bin " << il1
                      << " out of " << nb1max << ", sure.cnt()="
                      << sure.cnt() << ", iffy.cnt()=" << iffy.cnt();
                ibis::util::logMessage("bin::deprecatedJoin", "%s",
                                       ostmp.str().c_str());
                tlast = tcurr;
            }
        }
    }
} // ibis::bin::deprecatedJoin

/// A range join between two different columns with a complex distance
/// function.  This implementation relys on the fact that the expression @c
/// expr is monotonic in each bin.  Since the user does not know the exact
/// bin boundaries, this essentially require the express @c expr be a
/// monotonic function overall.  If this monotonicity is not satisfyied,
/// this function may miss some hits!
void ibis::bin::compJoin(const ibis::bin& idx2,
                         const ibis::math::term *expr,
                         const ibis::bitvector& mask,
                         const ibis::qRange* const range1,
                         const ibis::qRange* const range2,
                         ibis::bitvector64& sure,
                         ibis::bitvector64& iffy) const {
    if (mask.cnt() == 0) {
        uint64_t np = mask.size();
        np *= np;
        sure.set(0, np);
        iffy.clear();
        return;
    }
    if (col == 0) return;
    ibis::index::barrel bar(const_cast<const ibis::math::term*>(expr));
    if (bar.size() == 0) {
        const double delta = fabs(expr->eval());
        if (delta > 0.0)
            deprecatedJoin(idx2, delta, sure, iffy);
        else
            equiJoin(idx2, sure, iffy);
        return;
    }
    else if (bar.size() > 1 || stricmp(bar.name(0), col->name()) != 0) {
        std::ostringstream ostr;
        ostr << *expr;
        col->logWarning("bin::compJoin", "failed to deal with complex range "
                        "expression %s", ostr.str().c_str());
        uint64_t npairs = static_cast<uint64_t>(nrows) * nrows;
        sure.set(0, npairs);
        iffy.set(1, npairs);
        return;
    }
    if (ibis::gVerbose > 3) {
        std::ostringstream ostr;
        ostr << *expr;
        ibis::util::logMessage
            ("bin::compJoin", "start processing a range join ("
             "%s between %s - %s and %s + %s) with mask size %lu",
             col->name(), idx2.col->name(), ostr.str().c_str(),
             idx2.col->name(), ostr.str().c_str(),
             static_cast<long unsigned>(mask.cnt()));
    }

    uint32_t il1=0, il2=0, iu2=0;
    uint32_t ilu=0, iuu=0; // idx2.bits[ilu:iuu] is summed in cumu
    uint32_t ilv=0, iuv=0; // idx2.bits[ilv:iuv] is summed in cumv

    uint32_t nb1max = nobs;
    uint32_t nb2max = idx2.nobs;
    if (range1 || range2) {
        double amin = (range1 ? range1->leftBound() : col->getActualMin());
        double tmp = (range2 ? range2->leftBound() :
                      idx2.col->getActualMin());
        if (amin < tmp)
            amin = tmp;
        double amax = (range1 ? range1->rightBound() : col->getActualMax());
        tmp = (range2 ? range2->rightBound() : idx2.col->getActualMax());
        if (amax > tmp)
            amax = tmp;
        il1 = bounds.find(amin);
        nb1max = bounds.find(amax);
        if (nb1max < nobs && minval[nb1max] <= amax)
            ++ nb1max;
    }
    activate(il1, nb1max);
    idx2.activate(il2, nb2max);
    ibis::bitvector cumu, cumv, curr;
    uint32_t tlast = time(0);
    for (; il1 < nb1max && il2 < nb2max; ++ il1) {
        double delta=0;
        while (il1 < nb1max && il2 < nb2max) {
            if (! (maxval[il1] >= minval[il1])) {
                ++ il1;
            }
            else if (! (idx2.maxval[il2] >= idx2.minval[il2])) {
                ++ il2;
            }
            else { // compute the delta value
                bar.setValue(0, minval[il1]);
                delta = fabs(expr->eval());
                if (maxval[il1] != minval[il1]) {
                    bar.setValue(0, maxval[il1]);
                    double tmp = fabs(expr->eval());
                    if (tmp > delta)
                        delta = tmp;
                }
                if (idx2.maxval[il2] + delta >= minval[il1]) {
                    if (idx2.minval[il2] <= maxval[il1] + delta)
                        break;
                    else
                        ++ il1;
                }
                else if (idx2.minval[il2] <= maxval[il1]) {
                    ++ il2;
                }
                else {
                    ++ il1;
                    ++ il2;
                }
            }
        }

        if (il1 < nb1max && il2 < nb2max && bits[il1] &&
            bits[il1]->cnt() > 0) { // found some overlap
            curr.copy(mask);
            curr &= *(bits[il1]);
            if (curr.cnt() == 0)
                continue;

            if (minval[il1] == maxval[il1]) {
                uint32_t im2; // idx2.minval[im2] >= maxval[il1]-delta
                for (im2 = il2+1; im2 < nb2max &&
                         idx2.minval[im2] < maxval[il1]-delta; ++ im2);
                uint32_t in2; // idx2.maxval[in2] > minval[il1]+delta
                for (in2 = il2+1; in2 < nb2max &&
                         idx2.maxval[in2] <= minval[il1]+delta; ++ in2);
                if (im2 < in2) { // sure hits
                    sumBins(im2, in2, cumv, ilv, iuv);
                    ibis::bitvector tmp(mask);
                    tmp &= cumv;
                    ibis::util::outerProduct(curr, tmp, sure);
                    ilv = im2;
                    iuv = in2;
                }
            }
            // idx2.minval[iu2] > maxval[il1+delta
            for (iu2 = il2+1; iu2 < nb2max &&
                     idx2.minval[iu2] <= maxval[il1]+delta; ++ iu2);
            // not exactly sure, put the result in iffy
            sumBins(il2, iu2, cumu, ilu, iuu);
            ibis::bitvector cmk(mask);
            cmk &= cumu;
            ibis::util::outerProduct(curr, cmk, iffy);
            ilu = il2;
            iuu = iu2;
        }

        if (ibis::gVerbose > 1) {
            uint32_t tcurr = time(0);
            if (tcurr-59 > tlast) {
                std::ostringstream ostmp;
                ostmp << "TIME(" << tcurr
                      << "): just completed processing bin " << il1
                      << " out of " << nb1max << ", sure.cnt()="
                      << sure.cnt() << ", iffy.cnt()=" << iffy.cnt();
                ibis::util::logMessage("bin::compJoin", "%s",
                                       ostmp.str().c_str());
                tlast = tcurr;
            }
        }
    }
} // ibis::bin::compJoin

/// An equi-join on two different columns.
int64_t ibis::bin::equiJoin(const ibis::bin& idx2,
                            const ibis::bitvector& mask,
                            const ibis::qRange* const range1,
                            const ibis::qRange* const range2) const {
    int64_t cnt = 0;
    if (mask.cnt() == 0) {
        return cnt;
    }
    if (col == 0 || idx2.col == 0) return -1;

    if (ibis::gVerbose > 3)
        ibis::util::logMessage
            ("bin::equiJoin", "start processing an equi-join "
             "between %s and %s with mask size %lu", col->name(),
             idx2.col->name(), static_cast<long unsigned>(mask.cnt()));

    uint32_t il1=0, il2=0, iu2=0;
    uint32_t ilc=0, iuc=0; // idx2.bits[ilc:iuc] is summed in cumu

    uint32_t nb1max = nobs;
    uint32_t nb2max = idx2.nobs;
    if (range1 || range2) {
        double amin = (range1 ? range1->leftBound() : col->getActualMin());
        double tmp = (range2 ? range2->leftBound() :
                      idx2.col->getActualMin());
        if (amin < tmp)
            amin = tmp;
        double amax = (range1 ? range1->rightBound() : col->getActualMax());
        tmp = (range2 ? range2->rightBound() : idx2.col->getActualMax());
        if (amax > tmp)
            amax = tmp;
        il1 = bounds.find(amin);
        nb1max = bounds.find(amax);
        if (nb1max < nobs && minval[nb1max] <= amax)
            ++ nb1max;
        il2 = idx2.bounds.find(amin);
        nb2max = bounds.find(amax);
        if (nb2max < idx2.nobs && idx2.minval[nb2max] <= amax)
            ++ nb2max;
    }
    iu2 = il2;
    activate(il1, nb1max);
    idx2.activate(il2, nb2max);
    ibis::bitvector cumu, curr;
    uint32_t tlast = time(0);
    for (; il1 < nb1max && il2 < nb2max; ++ il1) {
        while (il1 < nb1max && il2 < nb2max &&
               !(maxval[il1] >= minval[il1] &&
                 idx2.maxval[il2] >= idx2.minval[il2] &&
                 maxval[il1] >= idx2.minval[il2] &&
                 idx2.maxval[il2] >= minval[il1])) {
            if (maxval[il1] >= minval[il1] &&
                idx2.maxval[il2] >= idx2.minval[il2]) {
                if (!(maxval[il1] >= idx2.minval[il2])) ++ il1;
                else ++ il2;
            }
            else {
                if (!(maxval[il1] >= minval[il1])) ++ il1;
                if (!(idx2.maxval[il2] >= idx2.minval[il2])) ++ il2;
            }
        }

        if (il1 < nb1max && il2 < nb2max && bits[il1] != 0 &&
            bits[il1]->cnt() > 0) { // found some overlap
            curr.copy(mask);
            curr &= *(bits[il1]);
            if (curr.cnt() == 0)
                continue;

            // bins [il2, iu2) from idx2 overlaps with bin il1 of *this
            for (iu2 = il2+1; iu2 < nb2max &&
                     idx2.minval[iu2] <= maxval[il1]; ++ iu2);
            sumBins(il2, iu2, cumu, ilc, iuc);
            ibis::bitvector tmp(mask);
            tmp &= cumu;
            cnt += tmp.cnt() * curr.cnt();
            ilc = il2;
            iuc = iu2;
        }

        if (ibis::gVerbose > 1) {
            uint32_t tcurr = time(0);
            if (tcurr-59 > tlast) {
                std::ostringstream ostmp;
                ostmp << "TIME(" << tcurr
                      << "): just completed processing bin " << il1
                      << " out of " << nb1max << ", current count="
                      << cnt;
                ibis::util::logMessage("bin::equiJoin", "%s",
                                       ostmp.str().c_str());
                tlast = tcurr;
            }
        }
    }
    return cnt;
} // ibis::bin::equiJoin

/// A range join on two different columns.
int64_t ibis::bin::deprecatedJoin(const ibis::bin& idx2,
                                  const double& delta,
                                  const ibis::bitvector& mask,
                                  const ibis::qRange* const range1,
                                  const ibis::qRange* const range2) const {
    int64_t cnt = 0;
    if (mask.cnt() == 0) {
        return cnt;
    }
    if (col == 0) return -1;
    if (delta <= 0.0)
        return equiJoin(idx2, mask, range1, range2);
    if (ibis::gVerbose > 3)
        ibis::util::logMessage
            ("bin::deprecatedJoin", "start processing a range-join ("
             "%s between %s - %g and %s + %g) with mask size %lu",
             col->name(), idx2.col->name(), delta,
             idx2.col->name(), delta, static_cast<long unsigned>(mask.cnt()));

    uint32_t il1=0, il2=0, iu2=0;
    uint32_t ilu=0, iuu=0; // idx2.bits[ilu:iuu] is summed in cumu

    uint32_t nb1max = nobs;
    uint32_t nb2max = idx2.nobs;
    if (range1 || range2) {
        double amin1 = (range1 ? range1->leftBound() : col->getActualMin());
        double amin2 = (range2 ? range2->leftBound() :
                        idx2.col->getActualMin());
        double amax1 = (range1 ? range1->rightBound() : col->getActualMax());
        double amax2 = (range2 ? range2->rightBound() :
                        idx2.col->getActualMax());
        double tmp = (amin1 >= amin2-delta ? amin1 : amin2-delta);
        il1 = bounds.find(tmp);
        tmp = (amax1 <= amax2+delta ? amax1 : amax2+delta);
        nb1max = bounds.find(tmp);
        if (nb1max < nobs && minval[nb1max] <= tmp)
            ++ nb1max;

        tmp = (amin2 >= amin1-delta ? amin2 : amin2-delta);
        il2 = idx2.bounds.find(tmp);
        tmp = (amax2 <= amax1+delta ? amax2 : amax1+delta);
        nb2max = bounds.find(tmp);
        if (nb2max < idx2.nobs && idx2.minval[nb2max] <= tmp)
            ++ nb2max;
    }
    iu2 = il2;
    activate(il1, nb1max);
    idx2.activate(il2, nb2max);
    ibis::bitvector cumu, curr;
    uint32_t tlast = time(0);
    for (; il1 < nb1max && il2 < nb2max; ++ il1) {
        while (il1 < nb1max && il2 < nb2max &&
               !(maxval[il1] >= minval[il1] &&
                 idx2.maxval[il2] >= idx2.minval[il2] &&
                 maxval[il1]+delta >= idx2.minval[il2] &&
                 idx2.maxval[il2]+delta >= minval[il1])) {
            if (maxval[il1] >= minval[il1] &&
                idx2.maxval[il2] >= idx2.minval[il2]) {
                if (!(maxval[il1]+delta >= idx2.minval[il2])) ++ il1;
                else ++ il2;
            }
            else {
                if (!(maxval[il1] >= minval[il1])) ++ il1;
                if (!(idx2.maxval[il2] >= idx2.minval[il2])) ++ il2;
            }
        }

        if (il1 < nb1max && il2 < nb2max && bits[il1] != 0 &&
            bits[il1]->cnt() > 0) { // found some overlap
            // idx2.minval[iu2] > maxval[il1+delta
            curr.copy(mask);
            curr &= *(bits[il1]);
            if (curr.cnt() == 0)
                continue;

            for (iu2 = il2+1; iu2 < nb2max &&
                     idx2.minval[iu2] <= maxval[il1]+delta; ++ iu2);
            idx2.sumBins(il2, iu2, cumu, ilu, iuu);
            ibis::bitvector tmp(mask);
            tmp &= cumu;
            cnt += curr.cnt() * tmp.cnt();
            ilu = il2;
            iuu = iu2;
        }

        if (ibis::gVerbose > 1) {
            uint32_t tcurr = time(0);
            if (tcurr-59 > tlast) {
                std::ostringstream ostmp;
                ostmp << "TIME(" << tcurr
                      << "): just completed processing bin " << il1
                      << " out of " << nb1max << ", current count="
                      << cnt;
                ibis::util::logMessage("bin::deprecatedJoin", "%s",
                                       ostmp.str().c_str());
                tlast = tcurr;
            }
        }
    }
    return cnt;
} // ibis::bin::deprecatedJoin

/// A range join between two different columns with a complex distance
/// function.  This implementation relys on the fact that the expression @c
/// expr is monotonic in each bin.  Since the user does not know the exact
/// bin boundaries, this essentially require the express @c expr be a
/// monotonic function overall.  If this monotonicity is not satisfyied,
/// this function may miss some hits!
int64_t ibis::bin::compJoin(const ibis::bin& idx2,
                            const ibis::math::term *expr,
                            const ibis::bitvector& mask,
                            const ibis::qRange* const range1,
                            const ibis::qRange* const range2) const {
    int64_t cnt = 0;
    if (mask.cnt() == 0) {
        return cnt;
    }
    if (col == 0 || idx2.col == 0) return -1;

    ibis::index::barrel bar(const_cast<const ibis::math::term*>(expr));
    if (bar.size() == 0) {
        const double delta = fabs(expr->eval());
        if (delta > 0.0)
            return deprecatedJoin(idx2, delta, mask, range1, range2);
        else
            return equiJoin(idx2, mask, range1, range2);
    }
    else if (bar.size() > 1 || stricmp(bar.name(0), col->name()) != 0) {
        std::ostringstream ostr;
        ostr << *expr;
        col->logWarning("bin::compJoin", "failed to deal with complex range "
                        "expression %s", ostr.str().c_str());
        return -1;
    }
    if (ibis::gVerbose > 3) {
        std::ostringstream ostr;
        ostr << *expr;
        ibis::util::logMessage
            ("bin::compJoin", "start processing a range join ("
             "%s between %s - %s and %s + %s) with mask size %lu",
             col->name(), idx2.col->name(), ostr.str().c_str(),
             idx2.col->name(), ostr.str().c_str(),
             static_cast<long unsigned>(mask.cnt()));
    }

    uint32_t il1=0, il2=0, iu2=0;
    uint32_t ilu=0, iuu=0; // idx2.bits[ilu:iuu] is summed in cumu

    uint32_t nb1max = nobs;
    uint32_t nb2max = idx2.nobs;
    if (range1 || range2) {
        double amin = (range1 ? range1->leftBound() : col->getActualMin());
        double tmp = (range2 ? range2->leftBound() :
                      idx2.col->getActualMin());
        if (amin < tmp)
            amin = tmp;
        double amax = (range1 ? range1->rightBound() : col->getActualMax());
        tmp = (range2 ? range2->rightBound() : idx2.col->getActualMax());
        if (amax > tmp)
            amax = tmp;
        il1 = bounds.find(amin);
        nb1max = bounds.find(amax);
        if (nb1max < nobs && minval[nb1max] <= amax)
            ++ nb1max;
    }
    activate(il1, nb1max);
    idx2.activate(il2, nb2max);
    ibis::bitvector cumu, curr;
    uint32_t tlast = time(0);
    for (; il1 < nb1max && il2 < nb2max; ++ il1) {
        double delta=0;
        while (il1 < nb1max && il2 < nb2max) {
            if (! (maxval[il1] >= minval[il1])) {
                ++ il1;
            }
            else if (! (idx2.maxval[il2] >= idx2.minval[il2])) {
                ++ il2;
            }
            else { // compute the delta value
                bar.setValue(0, minval[il1]);
                delta = fabs(expr->eval());
                if (maxval[il1] != minval[il1]) {
                    bar.setValue(0, maxval[il1]);
                    double tmp = fabs(expr->eval());
                    if (tmp > delta)
                        delta = tmp;
                }
                if (idx2.maxval[il2] + delta >= minval[il1]) {
                    if (idx2.minval[il2] <= maxval[il1] + delta)
                        break;
                    else
                        ++ il1;
                }
                else if (idx2.minval[il2] <= maxval[il1]) {
                    ++ il2;
                }
                else {
                    ++ il1;
                    ++ il2;
                }
            }
        }

        if (il1 < nb1max && il2 < nb2max && bits[il1] != 0 &&
            bits[il1]->cnt() > 0) { // found some overlap
            curr.copy(mask);
            curr &= *(bits[il1]);
            if (curr.cnt() == 0)
                continue;

            // idx2.minval[iu2] > maxval[il1+delta
            for (iu2 = il2+1; iu2 < nb2max &&
                     idx2.minval[iu2] <= maxval[il1]+delta; ++ iu2);
            sumBins(il2, iu2, cumu, ilu, iuu);
            ibis::bitvector tmp(mask);
            tmp &= cumu;
            cnt += tmp.cnt() * curr.cnt();
            ilu = il2;
            iuu = iu2;
        }

        if (ibis::gVerbose > 1) {
            uint32_t tcurr = time(0);
            if (tcurr-59 > tlast) {
                std::ostringstream ostmp;
                ostmp << "TIME(" << tcurr
                      << "): just completed processing bin " << il1
                      << " out of " << nb1max << ", current count="
                      << cnt;
                ibis::util::logMessage("bin::compJoin", "%s",
                                       ostmp.str().c_str());
                tlast = tcurr;
            }
        }
        ++ il1;
    }
    return cnt;
} // ibis::bin::compJoin

/// Compute the size of the serialized version of the index.  Return the
/// size in bytes.
size_t ibis::bin::getSerialSize() const throw () {
    size_t res = (nobs << 5) + 16;
    for (unsigned j = 0; j < nobs; ++ j)
        if (bits[j] != 0)
            res += bits[j]->getSerialSize();
    return res;
} // ibis::bin::getSerialSize

/// Extract values only.  This function requires the clustered version of
/// values to be present.  The clustered version is created with the option
/// 'reorder' in the binning specification.  Currently, the clustered
/// values are stored in a file with .bin extension.
template <typename T>
long ibis::bin::mergeValues(const ibis::qContinuousRange& cmp,
                            ibis::array_t<T>& vals) const {
    uint32_t c0, c1, h0, h1;
    locate(cmp, c0, c1, h0, h1);
    vals.clear();
    if (c0 >= c1)
        return 0;

    std::string fnm; // data file name
    dataFileName(fnm);
    fnm += ".bin";

    int fdes = UnixOpen(fnm.c_str(), OPEN_READONLY);
    if (fdes < 0) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- bin::mergeValues failed to open \""
            << fnm << " of column " << (col ? col->name() : "?");
        return -3;
    }
#if defined(_WIN32) && defined(_MSC_VER)
    (void)_setmode(fdes, _O_BINARY);
#endif
    IBIS_BLOCK_GUARD(UnixClose, fdes);
    uint32_t nbs;
    long ierr = UnixRead(fdes, &nbs, sizeof(nbs));
    if (ierr != (long)sizeof(nbs)) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- bin::mergeValues failed to read the first "
            "4-byte integer from \"" << fnm << "\"";
        return -4;
    }
    if (nbs != nobs) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- bin::mergeValues expects the number of bins in "
            << fnm << " to be " << nobs << ", but it is " << nbs;
        return -5;
    }
    if (c1 > nbs) c1 = nbs;
    if (h0 < c0) h0 = c0;
    if (h1 > c1) h1 = c1;

    ibis::array_t<uint32_t> offsets(fdes, 4*c0+4, 4*c1+8);
    if (offsets.size()+c0 <= c1) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- bin::mergeValues failed to read offsets from \""
            << fnm << "\" of column " << (col ? col->name() : "?")
            << " to evaluate \"" << cmp << '"';
        return -6;
    }

    const unsigned elm = sizeof(T);
    const uint32_t start = offsets.front();
    vals.reserve((offsets.back() - start)/elm);
    ierr = vals.read(fdes, start, offsets.back());
    if (ierr+start != offsets.back()) {
        return -7;
    }

    uint32_t jv = 0; // position to store the next value
    for (uint32_t j = 0; j < (offsets[h0-c0] - start)/elm; ++ j) {
        if (cmp.inRange(vals[j])) {
            vals[jv] = vals[j];
            ++ jv;
        }
    }
    if (jv < offsets[h0-c0]-start) { // need to copy values in the middle
        for (uint32_t j = (offsets[h0-c0]-start)/elm;
             j < (offsets[h1-c0]-start)/elm; ++ j, ++ jv)
            vals[jv] = vals[j];
    }
    else {
        jv += (offsets[h1-c0] - offsets[h0-c0]) / elm;
    }
    for (uint32_t j = (offsets[h1-c0]-start)/elm;
         j < (offsets.back()-start)/elm; ++ j) {
        if (cmp.inRange(vals[j])) {
            vals[jv] = vals[j];
            ++ jv;
        }
    }
    vals.resize(jv);
    return jv;
} // ibis::bin::mergeValues

/// Extract values and record the positions.  In order to generate the
/// output bit vector, this version requires effectively two copies of the
/// data because it has to place the values in the their original row
/// order.
template <typename T>
long ibis::bin::mergeValues(const ibis::qContinuousRange& cmp,
                            ibis::array_t<T>& vals,
                            ibis::bitvector& hits) const {
    uint32_t c0, c1, h0, h1;
    locate(cmp, c0, c1, h0, h1);
    vals.clear();
    hits.clear();
    if (c0 >= c1)
        return 0;

    std::string fnm; // data file name
    dataFileName(fnm);
    fnm += ".bin";

    int fdes = UnixOpen(fnm.c_str(), OPEN_READONLY);
    if (fdes < 0) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- bin::mergeValues failed to open \""
            << fnm << " of column " << (col ? col->name() : "?");
        return -3;
    }
#if defined(_WIN32) && defined(_MSC_VER)
    (void)_setmode(fdes, _O_BINARY);
#endif
    IBIS_BLOCK_GUARD(UnixClose, fdes);
    uint32_t nbs;
    long ierr = UnixRead(fdes, &nbs, sizeof(nbs));
    if (ierr != (long)sizeof(nbs)) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- bin::mergeValues failed to read the first "
            "4-byte integer from \"" << fnm << "\"";
        return -4;
    }
    if (nbs != nobs) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- bin::mergeValues expects the number of bins in "
            << fnm << " to be " << nobs << ", but it is " << nbs;
        return -5;
    }
    if (c1 > nbs) c1 = nbs;
    if (h0 < c0) h0 = c0;
    if (h1 > c1) h1 = c1;

    ibis::array_t<uint32_t> offsets(fdes, 4*c0+4, 4*c1+8);
    if (offsets.size()+c0 <= c1) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- bin::mergeValues failed to read offsets from \""
            << fnm << "\" of column " << (col ? col->name() : "?")
            << " to evaluate \"" << cmp << '"';
        return -6;
    }
    const unsigned elm = sizeof(T);
    const uint32_t start = offsets.front();
    vals.reserve((offsets.back() - start)/elm);
    ibis::array_t<T> buffer(fdes, start, offsets.back());
    if (buffer.size()*sizeof(T)+start != offsets.back()) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- bin::mgergeValues expected to read "
            << (offsets.back()-start)/sizeof(T) << " elements, but got "
            << buffer.size();
        return -7;
    }

    activate(c0, c1); // make sure all necessar bitmaps are in memory

    // a heap to organize the values
    ibis::util::heap< ibis::bin::valpos<T>,
        ibis::bin::comparevalpos<T> > hp;
    // values in the edge bins that satisfying the range condition
    ibis::array_t<T> v0, v1; 
    // position of the values in the edge bins
    ibis::bitvector p0, p1;
    if (c0 < h0 && offsets[1] > offsets[0] && bits[c0] != 0 &&
        bits[c0]->cnt() > 0) { // edge bin 0
        unsigned j = 0;
        for (ibis::bitvector::indexSet is = bits[c0]->firstIndexSet();
             is.nIndices() > 0; ++ is) {
            const ibis::bitvector::word_t *ix = is.indices();
            if (is.isRange()) {
                for (unsigned i = *ix; i < ix[1]; ++ i, ++ j) {
                    if (cmp.inRange(buffer[j])) {
                        v0.push_back(buffer[j]);
                        p0.setBit(i, 1);
                    }
                }
            }
            else {
                for (unsigned i = 0; i < is.nIndices(); ++ i, ++ j) {
                    if (cmp.inRange(buffer[j])) {
                        v0.push_back(buffer[j]);
                        p0.setBit(ix[i], 1);
                    }
                }
            }
        }

        if (p0.cnt() > 0)
            hp.push(new bin::valpos<T>(v0, p0));
    }
    if (c1 > h1 && offsets[c1-c0] > offsets[h1-c0] &&
        bits[h1] != 0 && bits[h1]->cnt() > 0) { // edge bin 1
        unsigned j = (offsets[h1-c0] - start) / elm;
        for (ibis::bitvector::indexSet is = bits[h1]->firstIndexSet();
             is.nIndices() > 0; ++ is) {
            const ibis::bitvector::word_t *ix = is.indices();
            if (is.isRange()) {
                for (unsigned i = *ix; i < ix[1]; ++ i, ++ j) {
                    if (cmp.inRange(buffer[j])) {
                        v1.push_back(buffer[j]);
                        p1.setBit(i, 1);
                    }
                }
            }
            else {
                for (unsigned i = 0; i < is.nIndices(); ++ i, ++ j) {
                    if (cmp.inRange(buffer[j])) {
                        v1.push_back(buffer[j]);
                        p1.setBit(ix[i], 1);
                    }
                }
            }
        }

        if (p1.cnt() > 0)
            hp.push(new bin::valpos<T>(v1, p1));
    }

    // add the middle bins to the heap
    uint32_t offset = (offsets[h0-c0] - start) / elm;
    for (unsigned ib = h0; ib < h1; ++ ib) {
        if (bits[ib] != 0 && bits[ib]->cnt() > 0) {
            bin::valpos<T> *vp = new bin::valpos<T>;
            vp->vals = &(buffer[offset]);
            vp->ind = bits[ib]->firstIndexSet();
            hp.push(vp);
        }
    }

    // use the heap to pick the next value
    while (hp.size() > 1) {
        bin::valpos<T>*const t = hp.top();
        if (t->ind.isRange()) { // add consecutive rows
            for (t->ji = t->ind.indices()[0];
                 t->ji < t->ind.indices()[1];
                 ++ t->ji, ++ t->jv)
                vals.push_back(t->vals[t->jv]);
            hits.adjustSize(0, t->ind.indices()[0]);
            hits.appendFill(1, t->ind.nIndices());

            ++ t->ind;
            if (t->ind.isRange())
                t->ji = *(t->ind.indices());
            else
                t->ji = 0;
        }
        else { // add one value
            vals.push_back(t->value());
            hits.setBit(t->ind.indices()[t->ji], 1);
            t->next();
        }

        hp.pop();
        if (t->ind.nIndices() > 0) {
            hp.push(t);
        }
        else {
            delete t;
        }
    } // while (hp.size() > 1)

    if (hp.size() > 0) { // one bitmap left
        bin::valpos<T>*const t = hp.top();
        ibis::bitvector::indexSet &s = t->ind;
        const ibis::bitvector::word_t *ix = s.indices();
        while (s.nIndices() > 0) {
            if (s.isRange()) {
                for (t->ji = *ix; t->ji < ix[1]; ++ t->ji, ++ t->jv)
                    vals.push_back(t->vals[t->jv]);
                hits.adjustSize(0, *ix);
                hits.appendFill(0, s.nIndices());
            }
            else {
                while (t->ji < s.nIndices()) {
                    vals.push_back(t->value());
                    hits.setBit(s.indices()[t->ji], 1);
                    ++ t->ji;
                    ++ t->jv;
                }
            }
            ++ s;
        }

        delete t;
    }

    hits.compress();
    hits.adjustSize(0, nrows);
    ierr = hits.size();
    return ierr;
} // ibis::bin::mergeValues

/// Select the rows that satisfy the range condition.  Output the values in
/// vals.  The values are in unspecified order to reduce the amount of
/// processing needed in this function -- this follows the spirit of SQL
/// standard.
///
/// @note This function only works with integers and floating-point values.
long ibis::bin::select(const ibis::qContinuousRange& cmp, void* vals) const {
    long ierr = -1;
    if (col == 0) return ierr;
    switch (col->type()) {
    case ibis::BYTE:
        ierr = mergeValues(cmp, *static_cast<array_t<signed char>*>(vals));
        break;
    case ibis::UBYTE:
        ierr = mergeValues(cmp, *static_cast<array_t<unsigned char>*>(vals));
        break;
    case ibis::SHORT:
        ierr = mergeValues(cmp, *static_cast<array_t<int16_t>*>(vals));
        break;
    case ibis::USHORT:
        ierr = mergeValues(cmp, *static_cast<array_t<uint16_t>*>(vals));
        break;
    case ibis::INT:
        ierr = mergeValues(cmp, *static_cast<array_t<int32_t>*>(vals));
        break;
    case ibis::UINT:
        ierr = mergeValues(cmp, *static_cast<array_t<uint32_t>*>(vals));
        break;
    case ibis::LONG:
        ierr = mergeValues(cmp, *static_cast<array_t<int64_t>*>(vals));
        break;
    case ibis::ULONG:
        ierr = mergeValues(cmp, *static_cast<array_t<uint64_t>*>(vals));
        break;
    case ibis::FLOAT:
        ierr = mergeValues(cmp, *static_cast<array_t<float>*>(vals));
        break;
    case ibis::DOUBLE:
        ierr = mergeValues(cmp, *static_cast<array_t<double>*>(vals));
        break;
    default:
        LOGGER(ibis::gVerbose > 2)
            << "Warning -- bin::select(" << cmp << ") can not work on "
            "column type " << ibis::TYPESTRING[(int)col->type()];
        break;
    }
    return ierr;
} // ibis::bin::select

/// Select the rows that satisfy the range condition.  Output the rows in
/// the bit vector hits and the corresponding values in vals.
/// @note This function only works with integers and floating-point values.
long ibis::bin::select(const ibis::qContinuousRange& cmp, void* vals,
                       ibis::bitvector& hits) const {
    long ierr = -1;
    if (col == 0) return ierr;

    std::string iname, bname;
    dataFileName(iname);
    bname = iname;
    bname += ".bin"; // bin file name
    iname += ".idx"; // index file name

    switch (col->type()) {
    case ibis::BYTE:
        ierr = mergeValues(cmp, *static_cast<array_t<signed char>*>(vals),
                           hits);
        break;
    case ibis::UBYTE:
        ierr = mergeValues(cmp, *static_cast<array_t<unsigned char>*>(vals),
                           hits);
        break;
    case ibis::SHORT:
        ierr = mergeValues(cmp, *static_cast<array_t<int16_t>*>(vals), hits);
        break;
    case ibis::USHORT:
        ierr = mergeValues(cmp, *static_cast<array_t<uint16_t>*>(vals), hits);
        break;
    case ibis::INT:
        ierr = mergeValues(cmp, *static_cast<array_t<int32_t>*>(vals), hits);
        break;
    case ibis::UINT:
        ierr = mergeValues(cmp, *static_cast<array_t<uint32_t>*>(vals), hits);
        break;
    case ibis::LONG:
        ierr = mergeValues(cmp, *static_cast<array_t<int64_t>*>(vals), hits);
        break;
    case ibis::ULONG:
        ierr = mergeValues(cmp, *static_cast<array_t<uint64_t>*>(vals), hits);
        break;
    case ibis::FLOAT:
        ierr = mergeValues(cmp, *static_cast<array_t<float>*>(vals), hits);
        break;
    case ibis::DOUBLE:
        ierr = mergeValues(cmp, *static_cast<array_t<double>*>(vals), hits);
        break;
    default:
        LOGGER(ibis::gVerbose > 2)
            << "Warning -- bin::select(" << cmp << ") can not work on "
            "column type " << ibis::TYPESTRING[(int)col->type()];
        break;
    }
    return ierr;
} // ibis::bin::select
