// File: $Id$
// Author: John Wu <John.Wu at ACM.org>
// Copyright (c) 2006-2016 the Regents of the University of California
#include "idirekte.h"
#include "part.h"

#include <typeinfo>     // std::typeid
#include <memory>       // std::unique_ptr

#define FASTBIT_SYNC_WRITE 1
/// Constructing a new ibis::direkte object from base data in a file.
/// Both arguments are expected to be valid pointers.
ibis::direkte::direkte(const ibis::column* c, const char* f)
    : ibis::index(c) {
    // attempt to read an index first
    int ierr = read(f);
    if (ierr == 0) return; // got an index from the file

    if (c == 0)
        return;
    if (c->type() == ibis::FLOAT ||
        c->type() == ibis::DOUBLE ||
        c->type() == ibis::TEXT) {
        LOGGER(ibis::gVerbose >= 0)
            << "Error -- direkte can only be used for columns with "
            "nonnegative integer values (current column " << c->name()
            << ", type=" << ibis::TYPESTRING[(int)c->type()] << ')';
        throw ibis::bad_alloc("wrong column type for ibis::direkte"
                              IBIS_FILE_LINE);
    }
    if (c->lowerBound() < 0.0 || c->upperBound() < 0.0) {
        const_cast<ibis::column*>(c)->computeMinMax();
        if (c->lowerBound() < 0.0 || c->upperBound() < 0.0) {
            LOGGER(ibis::gVerbose >= 0)
                << "Error -- direkte can only be used on nonnegative integer "
                "values, but the current minimal value is "
                << (c->lowerBound()<=c->upperBound() ? c->lowerBound() :
                    c->upperBound());
            throw ibis::bad_alloc("minimal value must >= 0 for ibis::direkte"
                                  IBIS_FILE_LINE);
        }
    }

    std::string dfname;
    dataFileName(dfname, f);
    if (c->type() == ibis::CATEGORY)
        dfname += ".int";

    switch (c->type()) {
    default: {
        LOGGER(ibis::gVerbose >= 0)
            << "Error -- direkte can only be used for columns with "
            "nonnegative integer values (current column " << c->name()
            << ", type=" << ibis::TYPESTRING[(int)c->type()] << ')';
        throw ibis::bad_alloc("wrong column type for ibis::direkte"
                              IBIS_FILE_LINE);}
    case ibis::BYTE: {
        ierr = construct0<signed char>(dfname.c_str());
        break;}
    case ibis::UBYTE: {
        ierr = construct0<unsigned char>(dfname.c_str());
        break;}
    case ibis::SHORT: {
        ierr = construct0<int16_t>(dfname.c_str());
        break;}
    case ibis::USHORT: {
        ierr = construct0<uint16_t>(dfname.c_str());
        break;}
    case ibis::INT: {
        ierr = construct0<int32_t>(dfname.c_str());
        break;}
    case ibis::UINT:
    case ibis::CATEGORY: {
        ierr = construct0<uint32_t>(dfname.c_str());
        break;}
    case ibis::LONG: {
        ierr = construct0<int64_t>(dfname.c_str());
        break;}
    case ibis::ULONG: {
        ierr = construct0<uint64_t>(dfname.c_str());
        break;}
    }
    if (ierr < 0) {
        LOGGER(ibis::gVerbose >= 0)
            << "Error -- direkte::construct0 failed with error code " << ierr;
        throw ibis::bad_alloc("direkte construction failure" IBIS_FILE_LINE);
    }
    if (ibis::gVerbose > 2) {
        ibis::util::logger lg;
        lg() << "direkte[" << col->fullname()
             << "]::ctor -- constructed a simple equality index with "
             << bits.size() << " bitmap" << (bits.size()>1?"s":"");
        if (ibis::gVerbose > 6) {
            lg() << "\n";
            print(lg());
        }
    }
} // ibis::direkte::direkte

/// Construct a dummy index.  All rows are marked as having the same value
/// with position popu.  This creates an index with (popu+1) bit vectors,
/// with the last one set to all 1s and the rest to be empty.
ibis::direkte::direkte(const ibis::column* c, uint32_t popu, uint32_t ntpl)
    : ibis::index(c) {
    if (c == 0 || popu == 0) return;
    try {
        if (ntpl == 0) {
            if (c->partition() != 0)
                ntpl = c->partition()->nRows();
            else
                return;
        }
        nrows = ntpl;
        bits.resize(1+popu);
        for (unsigned j = 0; j < popu; ++ j)
            bits[j] = 0;
        bits[popu] = new ibis::bitvector();
        // if (c != 0)
        //     c->getNullMask(*bits[popu]);
        // else
        //     bits[popu]->set(1, nrows);
        bits[popu]->set(1, nrows);
        if (ibis::gVerbose > 6) {
            ibis::util::logger lg;
            print(lg());
        }
    }
    catch (...) {
        LOGGER(ibis::gVerbose > 1)
            << "Warning -- direkte[" << col->fullname()
            << "]::ctor received an exception, cleaning up ...";
        clear();
        throw;
    }
} // constructor for dummy attributes

/// Construct an index from an integer array.  The values in the array @c
/// ind are assumed to be between 0 and card-1.  All values outside of this
/// range are ignored.
ibis::direkte::direkte(const ibis::column* c, uint32_t card,
                       array_t<uint32_t>& ind) : ibis::index(c) {
    if (card == 0) return;
    if (ind.empty()) return;

    try {
        bits.resize(card);
        for (uint32_t i = 0; i < card; ++i) {
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
                    << "DEBUG -- direkte[" << static_cast<void*>(this)
                    << "]::ctor ind[" << i << "]=" << ind[i]
                    << " >=" << card;
            }
#endif
        }
        for (uint32_t i = 0; i < card; ++i) {
            bits[i]->adjustSize(0, nrows);
        }
        if (ibis::gVerbose > 2) {
            ibis::util::logger lg;
            lg() << "direkte[";
            if (col != 0) {
                lg() << col->fullname();
            }
            else {
                lg() << "?.?";
            }
            lg() << "]::ctor -- constructed an equality index with "
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
            << "Warning -- direkte[" << static_cast<void*>(this)
            << "]::ctor received an exception, cleaning up ...";
        clear();
        throw;
    }
} // construct an index from an integer array

ibis::direkte::direkte(const ibis::column* c, ibis::fileManager::storage* st)
    : ibis::index(c, st) {
    read(st);
} // ibis::direkte::direkte

ibis::index* ibis::direkte::dup() const {
    return new ibis::direkte(*this);
}

template <typename T>
int ibis::direkte::construct0(const char* dfname) {
    if (col == 0) return -1;

    int ierr = 0;
    std::string evt = "direkte[";
    evt += col->fullname();
    evt += "]::construct0<";
    evt += typeid(T).name();
    evt += '>';
    array_t<T> vals;
    LOGGER(ibis::gVerbose > 4)
        << evt << " -- starting to process "
        << (dfname && *dfname ? dfname : "in-memory data");

    ibis::bitvector mask;
    col->getNullMask(mask);
    if (col->partition() != 0) {
        ibis::bitvector tmp;
        col->partition()->getNullMask(tmp);
        mask &= tmp;
    }
    if (col->partition() != 0)
        nrows = col->partition()->nRows();
    else
        nrows = mask.size();
    if (nrows == 0) return ierr;

    if (dfname && *dfname)
        ierr = ibis::fileManager::instance().getFile(dfname, vals);
    else
        ierr = col->getValuesArray(&vals);
    if (ierr == 0) { // got a pointer to the base data
        const uint32_t nbits = (uint32_t)col->upperBound() + 1;
#ifdef RESERVE_SPACE_BEFORE_CREATING_INDEX
        const uint32_t nset = (uint32_t)(nrows+nbits-1)/nbits;
#endif
        bits.resize(nbits);
        for (uint32_t i = 0; i < nbits; ++ i) {
            bits[i] = new ibis::bitvector();
#ifdef RESERVE_SPACE_BEFORE_CREATING_INDEX
            bits[i]->reserve(nbits, nset);
#endif
        }
        LOGGER(ibis::gVerbose > 6)
            << evt << " allocated " << nbits << " bitvector"
            << (nbits>1?"s":"");

        for (ibis::bitvector::indexSet is = mask.firstIndexSet();
             is.nIndices() > 0; ++ is) {
            const ibis::bitvector::word_t *iix = is.indices();
            const uint32_t nbits = bits.size();
            T vmax = bits.size();
            if (is.isRange()) {
                for (ibis::bitvector::word_t j = iix[0]; j < iix[1]; ++ j)
                    if (vmax < vals[j])
                        vmax = vals[j];
                if ((uint64_t)vmax > 0x7FFFFFFFU) {
                    LOGGER(ibis::gVerbose > 1)
                        << "Warning -- " << evt << " can not deal with value "
                        << vmax;
                    throw "direkte can not index values larger than 2^31";
                }
                if ((uint64_t)vmax > nbits) {
                    const uint32_t newsize =
                        (vmax+1U>nbits+nbits?vmax+1U:nbits+nbits);
                    bits.resize(newsize);
                    for (uint32_t i = nbits; i < newsize; ++ i)
                        bits[i] = new ibis::bitvector;
                }

                for (ibis::bitvector::word_t j = iix[0]; j < iix[1]; ++ j)
                    bits[vals[j]]->setBit(j, 1);
            }
            else {
                for (ibis::bitvector::word_t j = 0; j < is.nIndices(); ++ j)
                    if (vmax < vals[iix[j]])
                        vmax = vals[iix[j]];
                if ((uint64_t)vmax > 0x7FFFFFFFU) {
                    LOGGER(ibis::gVerbose > 1)
                        << "Warning -- " << evt << " can not deal with value "
                        << vmax;
                    throw "direkte can not index values larger than 2^31";
                }
                if ((uint64_t)vmax > nbits) {
                    const uint32_t newsize =
                        (vmax+1U>nbits+nbits?vmax+1U:nbits+nbits);
                    bits.resize(newsize);
                    for (uint32_t i = nbits; i < newsize; ++ i)
                        bits[i] = new ibis::bitvector;
                }

                for (ibis::bitvector::word_t j = 0; j < is.nIndices(); ++ j)
                    bits[vals[iix[j]]]->setBit(iix[j], 1);
            }
        }
    }
    else { // failed to read or memory map the data file, try to read the
           // values one at a time
        const unsigned elemsize = sizeof(T);
        uint32_t sz = ibis::util::getFileSize(dfname);
        if (sz == 0) {
            ierr = -1; // no data file
            return ierr;
        }

        LOGGER(ibis::gVerbose > 5)
            << evt << " -- starting to read the values from "
            << dfname << " one at a time";
        if (col->upperBound() > col->lowerBound()) {
            const uint32_t nbits = (uint32_t)col->upperBound() + 1;
#ifdef RESERVE_SPACE_BEFORE_CREATING_INDEX
            const uint32_t nset = (nrows + nbits - 1) / nbits;
#endif
            bits.resize(nbits);
            for (uint32_t i = 0; i < nbits; ++ i) {
                bits[i] = new ibis::bitvector();
#ifdef RESERVE_SPACE_BEFORE_CREATING_INDEX
                bits[i]->reserve(nbits, nset);
#endif
            }
        }
        sz /= elemsize;
        if (sz > nrows)
            sz = nrows;
        int fdes = UnixOpen(dfname, OPEN_READONLY);
        if (fdes < 0) {
            ierr = -2; // failed to open file for reading
            return ierr;
        }
        IBIS_BLOCK_GUARD(UnixClose, fdes);
#if defined(_WIN32) && defined(_MSC_VER)
        (void)_setmode(fdes, _O_BINARY);
#endif

        for (ibis::bitvector::indexSet is = mask.firstIndexSet();
             is.nIndices() > 0; ++ is) {
            T val;
            const ibis::bitvector::word_t *iix = is.indices();

            off_t pos = iix[0] * elemsize;
            ierr = UnixSeek(fdes, pos, SEEK_SET);
            if (ierr != pos) {
                LOGGER(ibis::gVerbose > 1)
                    << "Warning -- " << evt << " failed to seek to "
                    << pos << " in file " << dfname;
                clear();
                return -3;
            }

            if (is.isRange()) {
                for (ibis::bitvector::word_t j = iix[0]; j < iix[1]; ++ j) {
                    ierr = UnixRead(fdes, &val, elemsize);
                    if (ierr < static_cast<int>(elemsize)) {
                        clear();
                        return -4;
                    }

                    const uint32_t nbits = bits.size();
                    if (nbits <= static_cast<uint64_t>(val)) {
                        const uint32_t newsize =
                            (val+1U>=nbits+nbits?val+1U:nbits+nbits);
                        bits.resize(newsize);
                        for (uint32_t i = nbits; i < newsize; ++ i)
                            bits[i] = new ibis::bitvector;
                    }
                    bits[val]->setBit(j, 1);
                }
            }
            else {
                for (unsigned j = 0; j < is.nIndices(); ++ j) {
                    if (j > 0 && iix[j] > iix[j-1]+1U) {
                        pos = iix[j] * elemsize;
                        (void)UnixSeek(fdes, pos, SEEK_SET);
                    }

                    ierr = UnixRead(fdes, &val, elemsize);
                    if (ierr < static_cast<int>(elemsize)) {
                        clear();
                        return -5;
                    }

                    const uint32_t nbits = bits.size();
                    if (nbits <= static_cast<uint64_t>(val)) {
                        const uint32_t newsize =
                            (val+1U>=nbits+nbits?val+1U:nbits+nbits);
                        bits.resize(newsize);
                        for (uint32_t i = nbits; i < newsize; ++ i)
                            bits[i] = new ibis::bitvector;
                    }
                    bits[val]->setBit(iix[j], 1);
                }
            }
        }
    }

    // remove the empty bitvector at the end
    uint32_t last = bits.size();
    while (last > 0 && bits[last-1]->cnt() == 0) {
        -- last;
        delete bits[last];
    }
    bits.resize(last);
    // make sure all bitvectors are of the right size
    for (uint32_t i = 0; i < bits.size(); ++ i)
        bits[i]->adjustSize(0, nrows);
    return 0;
} // ibis::direkte::construct0

template <typename T>
int ibis::direkte::construct(const char* dfname) {
    if (col == 0) return -1;

    int ierr = 0;
    array_t<T> vals;
    LOGGER(ibis::gVerbose > 4)
        << "direkte[" << col->fullname()
        << "]::construct -- starting to process file " << dfname << " as "
        << typeid(T).name();
    ibis::bitvector mask;
    col->getNullMask(mask);
    if (col->partition() != 0)
        nrows = col->partition()->nRows();
    else
        nrows = mask.size();
    if (nrows == 0) return ierr;
    if (dfname && *dfname)
        ierr = ibis::fileManager::instance().getFile(dfname, vals);
    else
        ierr = col->getValuesArray(&vals);
    if (ierr == 0) { // got a pointer to the base data
        if (col->upperBound() > col->lowerBound()) {
            const uint32_t nbits = (uint32_t)col->upperBound() + 1;
#ifdef RESERVE_SPACE_BEFORE_CREATING_INDEX
            const uint32_t nset = (uint32_t)(nrows+nbits-1)/nbits;
#endif
            bits.resize(nbits);
            for (uint32_t i = 0; i < nbits; ++ i) {
                bits[i] = new ibis::bitvector();
#ifdef RESERVE_SPACE_BEFORE_CREATING_INDEX
                bits[i]->reserve(nbits, nset);
#endif
            }
            LOGGER(ibis::gVerbose > 6)
                << "direkte[" << (col ? col->fullname() : "?.?")
                << "]::construct finished allocating " << nbits
                << " bitvectors";
        }
        // if (vals.size() > nrows)
        //     vals.resize(nrows);

        for (ibis::bitvector::indexSet iset = mask.firstIndexSet();
             iset.nIndices() > 0; ++ iset) {
            const ibis::bitvector::word_t *iis = iset.indices();
            if (iset.isRange()) { // a range
                for (uint32_t j = *iis; j < iis[1]; ++ j) {
                    const uint32_t nbits = bits.size();
                    if (nbits <= static_cast<uint32_t>(vals[j])) {
                        const uint32_t newsize = vals[j]+1;
                        bits.resize(newsize);
                        for (uint32_t i = nbits; i < newsize; ++ i)
                            bits[i] = new ibis::bitvector;
                    }
                    bits[vals[j]]->setBit(j, 1);
                }
            }
            else {
                for (uint32_t i = 0; i < iset.nIndices(); ++ i) {
                    const ibis::bitvector::word_t j = iis[i];
                    const uint32_t nbits = bits.size();
                    if (nbits <= static_cast<uint32_t>(vals[j])) {
                        const uint32_t newsize = vals[j]+1;
                        bits.resize(newsize);
                        for (uint32_t i = nbits; i < newsize; ++ i)
                            bits[i] = new ibis::bitvector;
                    }
                    bits[vals[j]]->setBit(j, 1);
                }
            }
        }
    }
    else if (dfname && *dfname) {
        // failed to read or memory map the data file, try to read the
        // values one at a time
        const unsigned elemsize = sizeof(T);
        uint32_t sz = ibis::util::getFileSize(dfname);
        if (sz == 0) {
            ierr = -1; // no data file
            return ierr;
        }

        LOGGER(ibis::gVerbose > 5)
            << "direkte[" << col->fullname()
            << "]::construct -- starting to read the values from "
            << dfname << " one at a time";
        if (col->upperBound() > col->lowerBound()) {
            const uint32_t nbits = (uint32_t)col->upperBound() + 1;
#ifdef RESERVE_SPACE_BEFORE_CREATING_INDEX
            const uint32_t nset = (nrows + nbits - 1) / nbits;
#endif
            bits.resize(nbits);
            for (uint32_t i = 0; i < nbits; ++ i) {
                bits[i] = new ibis::bitvector();
#ifdef RESERVE_SPACE_BEFORE_CREATING_INDEX
                bits[i]->reserve(nbits, nset);
#endif
            }
        }
        sz /= elemsize;
        if (sz > nrows)
            sz = nrows;
        int fdes = UnixOpen(dfname, OPEN_READONLY);
        if (fdes < 0) {
            ierr = -2; // failed to open file for reading
            return ierr;
        }
        IBIS_BLOCK_GUARD(UnixClose, fdes);
#if defined(_WIN32) && defined(_MSC_VER)
        (void)_setmode(fdes, _O_BINARY);
#endif

        for (ibis::bitvector::indexSet iset = mask.firstIndexSet();
             iset.nIndices() > 0; ++ iset) {
            const ibis::bitvector::word_t *iis = iset.indices();
            if (iset.isRange()) { // a range
                ierr = UnixSeek(fdes, *iis * elemsize, SEEK_SET);
                for (uint32_t j = *iis; j < iis[1]; ++ j) {
                    T val;
                    ierr = UnixRead(fdes, &val, elemsize);
                    if (ierr < static_cast<int>(elemsize)) {
                        clear();
                        return -3;
                    }

                    const uint32_t nbits = bits.size();
                    if (nbits <= static_cast<uint32_t>(val)) {
                        const uint32_t newsize = val + 1;
                        bits.resize(newsize);
                        for (uint32_t i = nbits; i < newsize; ++ i)
                            bits[i] = new ibis::bitvector;
                    }
                    bits[val]->setBit(j, 1);
                }
            }
            else {
                for (uint32_t i = 0; i < iset.nIndices(); ++ i) {
                    const ibis::bitvector::word_t j = iis[i];
                    T val;
                    ierr = UnixSeek(fdes, j * elemsize, SEEK_SET);
                    if (ierr < 0 || static_cast<unsigned>(ierr) != j*elemsize) {
                        clear();
                        return -4;
                    }
                    ierr = UnixRead(fdes, &val, elemsize);
                    if (ierr < static_cast<int>(elemsize)) {
                        clear();
                        return -5;
                    }

                    const uint32_t nbits = bits.size();
                    if (nbits <= static_cast<uint32_t>(val)) {
                        const uint32_t newsize = val + 1;
                        bits.resize(newsize);
                        for (uint32_t i = nbits; i < newsize; ++ i)
                            bits[i] = new ibis::bitvector;
                    }
                    bits[val]->setBit(j, 1);
                }
            }
        }
    }

    // make sure all bitvectors are of the right size
    for (uint32_t i = 0; i < bits.size(); ++ i)
        bits[i]->adjustSize(0, nrows);
    return 0;
} // ibis::direkte::construct

/// The printing function.
void ibis::direkte::print(std::ostream& out) const {
    if (ibis::gVerbose < 0) return;
    const uint32_t nobs = bits.size();
    if (nobs > 0) {
        out << "The direct bitmap index for " << (col ? col->name() : "?")
            << " contains " << nobs << " bit vector" << (nobs > 1 ? "s" : "");
        uint32_t skip = 0;
        if (ibis::gVerbose <= 0) {
            skip = nobs;
        }
        else if ((nobs >> 2*ibis::gVerbose) > 2) {
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

        for (uint32_t i=0; i<nobs; i += skip) {
            if (bits[i]) {
                out << "\n" << i << "\t" << bits[i]->cnt() << "\t"
                    << bits[i]->bytes()
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                    << "\t" << bits[i]->size()
#endif
                   ;
            }
        }
        if ((nobs-1) % skip) {
            if (bits[nobs-1]) {
                out << "\n" << nobs-1 << "\t" << bits[nobs-1]->cnt()
                    << "\t" << bits[nobs-1]->bytes();
            }
        }
    }
    else {
        out << "The direct bitmap index @" << static_cast<const void*>(this)
            << " is empty\n";
    }
    out << std::endl;
} // ibis::direkte::print

/// Write the direct bitmap index to a file.
int ibis::direkte::write(const char* dt) const {
    std::string fnm, evt;
    evt = "direkte";
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
        const ibis::fileManager::roFile *rof =
            dynamic_cast<const ibis::fileManager::roFile*>(str);
        if (rof != 0) {
            activate();
            if (const_cast<ibis::fileManager::roFile *>(rof)->disconnectFile()
                >= 0) {
                fname = 0;
            }
            else {
                LOGGER(ibis::gVerbose > 0)
                    << "Warning -- " << evt << " can not overwrite the index "
                    "file \"" << fnm
                    << "\" while it is used as a read-only file map";
                return 0;
            }
        }
        else { // everything in memory, fname was set by a mistake
            fname = 0;
        }
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
        ibis::fileManager::instance().flushFile(fnm.c_str());
        fdes = UnixOpen(fnm.c_str(), OPEN_WRITENEW, OPEN_FILEMODE);
        if (fdes < 0) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << evt << " failed to open \"" << fnm
                << "\" for writing ... " << (errno ? strerror(errno) : 0);
            errno = 0;
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
    const uint32_t nobs = bits.size();

#ifdef FASTBIT_USE_LONG_OFFSETS
    const bool useoffset64 = true;
#else
    const bool useoffset64 = (8+getSerialSize() > 0x80000000UL);
#endif
    char header[] = "#IBIS\0\0\0";
    header[5] = (char)ibis::index::DIREKTE;
    header[6] = (char)(useoffset64 ? 8 : 4);
    ierr = UnixWrite(fdes, header, 8);
    if (ierr < 8) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " failed to write the 8-byte header, "
            "ierr = " << ierr;
        return -3;
    }
    ierr  = UnixWrite(fdes, &nrows, sizeof(uint32_t));
    ierr += UnixWrite(fdes, &nobs,  sizeof(uint32_t));
    if (ierr < 8) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " failed to write nrows and nobs, "
            "ierr = " << ierr;
        return -4;
    }
    offset64.resize(nobs+1);
    offset64[0] = 16 + header[6]*(nobs+1);
    ierr = UnixSeek(fdes, header[6]*(nobs+1), SEEK_CUR);
    if (ierr != offset64[0]) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " failed to seek to " << offset64[0]
            << ", ierr = " << ierr;
        return -5;
    }
    for (uint32_t i = 0; i < nobs; ++ i) {
        if (bits[i] != 0) {
            if (bits[i]->cnt() > 0)
                bits[i]->write(fdes);
        }
        offset64[i+1] = UnixSeek(fdes, 0, SEEK_CUR);
    }
    ierr = UnixSeek(fdes, 16, SEEK_SET);
    if (ierr != 16) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " failed to seek to offset 16, ierr = "
            << ierr;
        return -6;
    }
    if (useoffset64) {
        ierr = UnixWrite(fdes, offset64.begin(), 8*(nobs+1));
        offset32.clear();
    }
    else {
        offset32.resize(nobs+1);
        for (unsigned j = 0; j <= nobs; ++ j)
            offset32[j] = offset64[j];
        ierr = UnixWrite(fdes, offset32.begin(), 4*(nobs+1));
        offset64.clear();
    }
    if (ierr < (off_t)(header[6]*(nobs+1))) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " failed to write bitmap offsets, "
            "ierr = " << ierr;
        return -7;
    }
#if defined(FASTBIT_SYNC_WRITE)
#if _POSIX_FSYNC+0 > 0
    (void) UnixFlush(fdes); // write to disk
#elif defined(_WIN32) && defined(_MSC_VER)
    (void) _commit(fdes);
#endif
#endif

    LOGGER(ibis::gVerbose > 5)
        << evt << " wrote " << nobs
        << " bitmap" << (nobs>1?"s":"") << " to " << fnm;
    return 0;
} // ibis::direkte::write

int ibis::direkte::write(ibis::array_t<double> &keys,
                         ibis::array_t<int64_t> &starts,
                         ibis::array_t<uint32_t> &bitmaps) const {
    const uint32_t nobs = bits.size();
    keys.resize(0);
    if (nobs == 0) {
        starts.resize(0);
        bitmaps.resize(0);
        return 0;
    }

    keys.resize(nobs);
    starts.resize(nobs+1);
    starts[0] = 0;
    for (unsigned j = 0; j < nobs; ++ j) { // iterate over bitmaps
        if (bits[j] != 0) {
            ibis::array_t<ibis::bitvector::word_t> tmp;
            bits[j]->write(tmp);
            bitmaps.insert(bitmaps.end(), tmp.begin(), tmp.end());
        }
        starts[j+1] = bitmaps.size();
        keys[j] = j;
    }
    return 0;
} // ibis::direkte::write

void ibis::direkte::serialSizes(uint64_t &wkeys, uint64_t &woffsets,
                                uint64_t &wbitmaps) const {
    const uint32_t nobs = bits.size();
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
} // ibis::direkte::serialSizes

/// Read index from the specified location.
int ibis::direkte::read(const char* f) {
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
                  header[5] == static_cast<char>(ibis::index::DIREKTE) &&
                  (header[6] == 8 || header[6] == 4) &&
                  header[7] == static_cast<char>(0))) {
        if (ibis::gVerbose > 0) {
            ibis::util::logger lg;
            lg() << "Warning -- direkte[" << (col ? col->fullname() : "?.?")
                 << "]::read the header from " << fnm << " (";
            printHeader(lg(), header);
            lg() << ") does not contain the expected values";
        }
        return -3;
    }

    uint32_t dim[2];
    size_t begin, end;
    ibis::index::clear(); // clear the current bit vectors
    fname = ibis::util::strnewdup(fnm.c_str());

    off_t ierr = UnixRead(fdes, static_cast<void*>(dim), 2*sizeof(uint32_t));
    if (ierr < static_cast<int>(2*sizeof(uint32_t))) {
        return -4;
    }
    nrows = dim[0];
    // read offsets
    begin = 8 + 2*sizeof(uint32_t);
    end = 8 + 2*sizeof(uint32_t) + header[6] * (dim[1] + 1);
    ierr = initOffsets(fdes, header[6], begin, dim[1]);
    if (ierr < 0)
        return ierr;
    ibis::fileManager::instance().recordPages(0, end);
#if DEBUG+0 > 1 || _DEBUG+0 > 1
    if (ibis::gVerbose > 5) {
        unsigned nprt = (ibis::gVerbose < 30 ? (1 << ibis::gVerbose) : dim[1]);
        if (nprt > dim[1])
            nprt = dim[1];
        ibis::util::logger lg;
        lg() << "DEBUG -- direkte[" << (col ? col->fullname() : "?.?")
             << "]::read(" << fnm << ") got nobs = " << dim[1]
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
    str = 0;
    LOGGER(ibis::gVerbose > 3)
        << "direkte[" << (col ? col->name() : "?.?") << "]::read(" << fnm
        << ") finished reading index header with nrows=" << nrows
        << " and bits.size()=" << bits.size();
    return 0;
} // ibis::direkte::read

/// Reconstruct an index from a piece of consecutive memory.
int ibis::direkte::read(ibis::fileManager::storage* st) {
    if (st == 0) return -1;
    clear();

    if (st->begin()[5] != ibis::index::DIREKTE)
        return -3;

    const char offsetsize = st->begin()[6];
    nrows = *(reinterpret_cast<uint32_t*>(st->begin()+8));
    uint32_t pos = 8 + sizeof(uint32_t);
    const uint32_t nobs = *(reinterpret_cast<uint32_t*>(st->begin()+pos));
    pos += sizeof(uint32_t);
    if (offsetsize == 8) {
        array_t<int64_t> offs(st, pos, pos+8*nobs+8);
        offset64.copy(offs);
    }
    else if (offsetsize == 4) {
        array_t<int32_t> offs(st, pos, pos+4*nobs+4);
        offset32.copy(offs);
    }
    else {
        clear();
        return -2;
    }

    initBitmaps(st);
    LOGGER(ibis::gVerbose > 3)
        << "direkte[" << (col ? col->name() : "?.?") << "]::read(" << st
        << ") finished reading index header with nrows=" << nrows
        << " and bits.size()=" << bits.size();
    return 0;
} // ibis::direkte::read

/// Change the key values to a new set of numbers.  This is used after a
/// categorical value column changes it dictionary and we need to reshuffle
/// the bitmaps but not the actual content in any bitmap.  The incoming
/// argument is expected to be an array of exactly the same number of
/// elements as the number of bitmaps in this index.
///
/// Return the number of bit vectors after successfully remapped the keys.
/// Otherwise return a negative number.
int ibis::direkte::remapKeys(const ibis::array_t<uint32_t> &o2n) {
    if (bits.empty()) return 0;
    if (bits.size() != o2n.size()) return -1;

    const std::string evt = "direkte::remapKeys";
    uint32_t nb = o2n[0];
    for (unsigned j = 1; j < o2n.size(); ++ j) {
        if (o2n[j] > nb)
            nb = o2n[j];
    }
    nb += 1; // the number of new bit vectors
    ibis::array_t<ibis::bitvector*> newbits(nb, 0);

    activate(); // make sure all bit vectors are in memory
    for (unsigned j = 0; j < o2n.size(); ++ j) {
        if (bits[j] != 0 && bits[j]->sloppyCount() > 0) {
            if (newbits[o2n[j]] == 0) {
                newbits[o2n[j]] = new ibis::bitvector;
                if (newbits[o2n[j]] != 0) {
                    newbits[o2n[j]]->copy(*(bits[j]));
                }
                else {
                    ibis::util::clear(newbits);
                    LOGGER(ibis::gVerbose > 0)
                        << "Warning -- " << evt << " failed to allocate a new "
                        "bitvector for key value " << o2n[j];
                    return -2;
                }
            }
            else {
                ibis::util::clear(newbits);
                LOGGER(ibis::gVerbose > 0)
                    << "Warning -- " << evt << " encountered duplicate mapped "
                    "values " << o2n[j];
                return -3;
            }
        }
    }

    offset32.clear();
    offset64.resize(nb+1);
    offset64[0] = 0;
    for (unsigned j = 0; j < nb; ++ j) {
        offset64[j+1] = offset64[j] + (newbits[j] ? newbits[j]->bytes() : 0U);
    }
    newbits.swap(bits);
    ibis::util::clear(newbits);

    if (str != 0) {
        if (str->filename() != 0)
            ibis::fileManager::instance().flushFile(str->filename());
        else
            delete str;
    }
    return write(0);
} // ibis::direkte::remapKeys

/// Convert the bitvector into integer values.
void ibis::direkte::ints(ibis::array_t<uint32_t> &res) const {
    res.clear();
    res.insert(res.end(), nrows, 0);
    std::unique_ptr<ibis::bitvector> tmp;
    uint32_t nobs = bits.size();

    activate(); // need all bitvectors to be in memory
    for (uint32_t i = 0; i < nobs; ++i) { // work over each bitmap
        if (bits[i]) {
            ibis::bitvector::indexSet is = bits[i]->firstIndexSet();
            const ibis::bitvector::word_t *iix = is.indices();
            uint32_t nind = is.nIndices();
            while (nind > 0) {
                if (is.isRange()) {
                    for (uint32_t j = *iix; j < iix[1]; ++j) {
                        res[j] = i;
                    }
                }
                else if (nind > 0) {
                    for  (uint32_t j = 0; j < nind; ++j) {
                        res[iix[j]] = i;
                    }
                }

                ++ is;
                nind =is.nIndices();
            }
        }
    }
} // ibis::direkte::keys

/// Convert the bitvector mask into key values.
ibis::array_t<uint32_t>*
ibis::direkte::keys(const ibis::bitvector& mask) const {
    std::unique_ptr< ibis::array_t<uint32_t> > res(new ibis::array_t<uint32_t>);
    if (mask.cnt() == 0) // nothing to do
        return res.release();

    std::unique_ptr<ibis::bitvector> tmp;
    uint32_t nobs = bits.size();
    ibis::array_t<uint32_t> ires;
    res->reserve(mask.cnt());
    ires.reserve(mask.cnt());

    activate(); // need all bitvectors to be in memory
    for (uint32_t i = 0; i < nobs; ++i) { // loop to fill res and ires
        if (bits[i]) {
            if (bits[i]->size() == mask.size()) {
                uint32_t nind = 0;
                tmp.reset(mask & *(bits[i]));
                ibis::bitvector::indexSet is = tmp->firstIndexSet();
                const ibis::bitvector::word_t *iix = is.indices();
                nind = is.nIndices();
                while (nind > 0) {
                    if (is.isRange()) {
                        for (uint32_t j = *iix; j < iix[1]; ++j) {
                            res->push_back(i);
                            ires.push_back(j);
                        }
                    }
                    else if (nind > 0) {
                        for  (uint32_t j = 0; j < nind; ++j) {
                            res->push_back(i);
                            ires.push_back(iix[j]);
                        }
                    }

                    ++ is;
                    nind =is.nIndices();
                }
            }
            else {
                LOGGER(ibis::gVerbose > 2)
                    << "Warning -- direkte[" << (col ? col->fullname() : "?.?")
                    << "]::keys bits[" << i << "]->size() = "
                    << bits[i]->size() << ", but mask.size() = " << mask.size();
            }
        }
        else {
            LOGGER(ibis::gVerbose > 4)
                << "Warning -- direkte[" << (col ? col->fullname() : "?.?")
                << "]::keys bits[" << i << "] is nil";
        }
    }

    ibis::util::sortKeys(ires, *res);
    LOGGER(res->empty() && ibis::gVerbose > 1)
        << "Warning -- direkte[" << (col ? col->fullname() : "?.?")
        << "]::keys failed to compute the keys most likely "
        "because the index does not have the same number of rows as data";
    return res.release();
} // ibis::direkte::keys

// Convert to a range [ib, ie) such that bits[ib:ie-1] contains the solution
void ibis::direkte::locate(const ibis::qContinuousRange& expr,
                           uint32_t& ib, uint32_t& ie) const {
    ib = static_cast<uint32_t>(expr.leftBound()>0.0 ? expr.leftBound() : 0.0);
    ie = static_cast<uint32_t>(expr.rightBound()>0.0 ? expr.rightBound() : 0.0);

    switch (expr.leftOperator()) {
    case ibis::qExpr::OP_LT: {
        ib += (expr.leftBound() >= ib);
        switch (expr.rightOperator()) {
        case ibis::qExpr::OP_LT: {
            if (expr.rightBound()>ie)
                ++ ie;
            break;}
        case ibis::qExpr::OP_LE: {
            ++ ie;
            break;}
        case ibis::qExpr::OP_GT: {
            if (ib < ie+1)
                ib = ie + 1;
            ie = bits.size();
            break;}
        case ibis::qExpr::OP_GE: {
            if (expr.rightBound() > ie)
                ++ ie;
            if (ib < ie)
                ib = ie;
            ie = bits.size();
            break;}
        case ibis::qExpr::OP_EQ: {
            if (expr.leftBound() < expr.rightBound() &&
                ie == expr.rightBound()) {
                ib = ie;
                ++ ie;
            }
            else {
                ie = ib;
            }
            break;}
        default: {
            ie = bits.size();
            break;}
        }
        break;}
    case ibis::qExpr::OP_LE: {
        ib += (expr.leftBound() > ib);
        switch (expr.rightOperator()) {
        case ibis::qExpr::OP_LT: {
            if (expr.rightBound()>ie)
                ++ ie;
            break;}
        case ibis::qExpr::OP_LE: {
            ++ ie;
            break;}
        case ibis::qExpr::OP_GT: {
            if (ib < ie+1)
                ib = ie+1;
            ie = bits.size();
            break;}
        case ibis::qExpr::OP_GE: {
            if (expr.rightBound() > ie)
                ++ ie;
            if (ib < ie)
                ib = ie;
            ie = bits.size();
            break;}
        case ibis::qExpr::OP_EQ: {
            if (expr.rightBound() >= expr.leftBound() &&
                ie == expr.rightBound()) {
                ib = ie;
                ++ ie;
            }
            else {
                ie = ib;
            }
            break;}
        default: {
            ie = bits.size();
            break;}
        }
        break;}
    case ibis::qExpr::OP_GT: {
        ib += (expr.leftBound() > ib);
        switch (expr.rightOperator()) {
        case ibis::qExpr::OP_LT: {
            if (expr.rightBound() > ie)
                ++ ie;
            if (ib < ie)
                ie = ib;
            ib = 0;
            break;}
        case ibis::qExpr::OP_LE: {
            ++ ie;
            if (ib < ie)
                ie = ib;
            ib = 0;
            break;}
        case ibis::qExpr::OP_GT: {
            uint32_t tmp = ie+1;
            ie = ib;
            ib = tmp;
            break;}
        case ibis::qExpr::OP_GE: {
            uint32_t tmp = (expr.rightBound()>ie ? ie+1 : ie);
            ie = ib;
            ib = tmp;
            break;}
        case ibis::qExpr::OP_EQ: {
            if (expr.rightBound() > expr.leftBound() &&
                expr.rightBound() == ie) {
                ib = ie;
                ++ ie;
            }
            else {
                ie = ib;
            }
            break;}
        default: {
            ie = ib;
            ib = 0;
            break;}
        }
        break;}
    case ibis::qExpr::OP_GE: {
        ib += (expr.leftBound() >= ib);
        switch (expr.rightOperator()) {
        case ibis::qExpr::OP_LT: {
            if (expr.rightBound() > ie)
                ++ ie;
            if (ib < ie)
                ie = ib;
            ib = 0;
            break;}
        case ibis::qExpr::OP_LE: {
            ++ ie;
            if (ib < ie)
                ie = ib;
            ib = 0;
            break;}
        case ibis::qExpr::OP_GT: {
            uint32_t tmp = ie+1;
            ie = ib+1;
            ib = tmp;
            break;}
        case ibis::qExpr::OP_GE: {
            uint32_t tmp = (expr.rightBound()<=ie ? ie : ie+1);
            ie = ib+1;
            ib = tmp;
            break;}
        case ibis::qExpr::OP_EQ: {
            if (expr.leftBound() >= expr.rightBound()) {
                ib = ie;
                ++ ie;
            }
            else {
                ie = ib;
            }  
            break;}
        default: {
            ie = ib;
            ib = 0;
            break;}
        }
        break;}
    case ibis::qExpr::OP_EQ: {
        if (expr.leftBound() == ib) {
            switch (expr.rightOperator()) {
            case ibis::qExpr::OP_LT: {
                if (expr.leftBound() < expr.rightBound())
                    ie = ib+1;
                else
                    ie = ib;
                break;}
            case ibis::qExpr::OP_LE: {
                if (expr.leftBound() <= expr.rightBound())
                    ie = ib+1;
                else
                    ie = ib;
                break;}
            case ibis::qExpr::OP_GT: {
                if (expr.leftBound() > expr.rightBound())
                    ie = ib+1;
                else
                    ie = ib;
                break;}
            case ibis::qExpr::OP_GE: {
                if (expr.leftBound() >= expr.rightBound())
                    ie = ib+1;
                else
                    ie = ib;
                break;}
            case ibis::qExpr::OP_EQ: {
                if (expr.leftBound() == expr.rightBound())
                    ie = ib+1;
                else
                    ie = ib;
                break;}
            default: {
                ie = ib+1;
                break;}
            }
        }
        else {
            ie = ib;
        }
        break;}
    default: {
        switch (expr.rightOperator()) {
        case ibis::qExpr::OP_LT: {
            ib = 0;
            if (expr.rightBound()>ie)
                ++ ie;
            break;}
        case ibis::qExpr::OP_LE: {
            ib = 0;
            ++ ie;
            break;}
        case ibis::qExpr::OP_GT: {
            ib = ie + 1;
            ie = bits.size();
            break;}
        case ibis::qExpr::OP_GE: {
            ib = (expr.rightBound() == ie ? ie : ie+1);
            ie = bits.size();
            break;}
        case ibis::qExpr::OP_EQ: {
            if (expr.rightBound() == ie) {
                ib = ie;
                ++ ie;
            }
            else {
                ie = ib;
            }
            break;}
        default: {
            // nothing specified, match all
            LOGGER(ibis::gVerbose > -1)
                << "Warning -- direkte::locate encounters a unknown operator "
                "in a qContinuousQuery object";
            ib = 0;
            ie = bits.size();
            break;}
        }
        break;}
    }
} // ibis::direkte::locate

long ibis::direkte::evaluate(const ibis::qContinuousRange& expr,
                             ibis::bitvector& lower) const {
    uint32_t ib, ie;
    locate(expr, ib, ie);
    sumBins(ib, ie, lower);
    return lower.cnt();
} // ibis::direkte::evaluate

void ibis::direkte::estimate(const ibis::qContinuousRange& expr,
                             ibis::bitvector& lower,
                             ibis::bitvector& upper) const {
    upper.clear();
    uint32_t ib, ie;
    locate(expr, ib, ie);
    sumBins(ib, ie, lower);
} // ibis::direkte::estimate

uint32_t ibis::direkte::estimate(const ibis::qContinuousRange& expr) const {
    uint32_t ib, ie, cnt;
    locate(expr, ib, ie);
    activate(ib, ie);
    cnt = 0;
    for (uint32_t j = ib; j < ie; ++ j)
        if (bits[j])
            cnt += bits[j]->cnt();
    return cnt;
} // ibis::direkte::estimate

long ibis::direkte::evaluate(const ibis::qDiscreteRange& expr,
                             ibis::bitvector& lower) const {
    const ibis::array_t<double>& varr = expr.getValues();
    lower.set(0, nrows);
    for (unsigned i = 0; i < varr.size(); ++ i) {
        unsigned int tmp = static_cast<unsigned int>(varr[i]);
        if (tmp < bits.size()) {
            if (bits[tmp] == 0)
                activate(tmp);
            if (bits[tmp])
                lower |= *(bits[tmp]);
        }
    }
    return lower.cnt();
} // ibis::direkte::evaluate

void ibis::direkte::estimate(const ibis::qDiscreteRange& expr,
                             ibis::bitvector& lower,
                             ibis::bitvector& upper) const {
    const ibis::array_t<double>& varr = expr.getValues();
    upper.clear();
    lower.set(0, nrows);
    for (unsigned i = 0; i < varr.size(); ++ i) {
        unsigned int tmp = static_cast<unsigned int>(varr[i]);
        if (tmp < bits.size()) {
            if (bits[tmp] == 0)
                activate(tmp);
            if (bits[tmp])
                lower |= *(bits[tmp]);
        }
    }
} // ibis::direkte::estimate

uint32_t ibis::direkte::estimate(const ibis::qDiscreteRange& expr) const {
    uint32_t res = 0;
    const ibis::array_t<double>& varr = expr.getValues();
    for (unsigned i = 0; i < varr.size(); ++ i) {
        unsigned int tmp = static_cast<unsigned int>(varr[i]);
        if (tmp < bits.size()) {
            if (bits[tmp] == 0)
                activate(tmp);
            if (bits[tmp])
                res += bits[tmp]->cnt();
        }
    }
    return res;
} // ibis::direkte::estimate

double ibis::direkte::estimateCost(const ibis::qContinuousRange& expr) const {
    double cost = 0.0;
    uint32_t ib, ie;
    locate(expr, ib, ie);
    if (ib < ie) {
        if (offset64.size() > bits.size()) {
            const int32_t tot = offset64.back() - offset64[0];
            if (ie < offset64.size()) {
                const int32_t mid = offset64[ie] - offset64[ib];
                if ((tot >> 1) >= mid)
                    cost = mid;
                else
                    cost = tot - mid;
            }
            else if (ib < offset64.size()) {
                const int32_t mid = offset64.back() - offset64[ib];
                if ((tot >> 1) >= mid)
                    cost = mid;
                else
                    cost = tot - mid;
            }
        }
        else if (offset32.size() > bits.size()) {
            const int32_t tot = offset32.back() - offset32[0];
            if (ie < offset32.size()) {
                const int32_t mid = offset32[ie] - offset32[ib];
                if ((tot >> 1) >= mid)
                    cost = mid;
                else
                    cost = tot - mid;
            }
            else if (ib < offset32.size()) {
                const int32_t mid = offset32.back() - offset32[ib];
                if ((tot >> 1) >= mid)
                    cost = mid;
                else
                    cost = tot - mid;
            }
        }
        else {
            const unsigned elm = (col ? col->elementSize() : 4U);
            if (elm > 0)
                cost = (double)elm * nrows;
            else
                cost = 4.0 * nrows;
        }
    }
    return cost;
} // ibis::direkte::estimateCost

double ibis::direkte::estimateCost(const ibis::qDiscreteRange& expr) const {
    double cost = 0;
    const ibis::array_t<double>& varr = expr.getValues();
    for (uint32_t j = 0; j < varr.size(); ++ j) {
        uint32_t ind = static_cast<uint32_t>(varr[j]);
        if (ind+1 < offset64.size() && ind < bits.size())
            cost += offset64[ind+1] - offset64[ind];
        else if (ind+1 < offset32.size() && ind < bits.size())
            cost += offset32[ind+1] - offset32[ind];
    }
    return cost;
} // ibis::direkte::estimateCost

/// Append the index in df to the one in dt.  If the index in df exists,
/// then it will be used, otherwise it simply creates a new index using the
/// data in dt.
long ibis::direkte::append(const char* dt, const char* df, uint32_t nnew) {
    if (col == 0 || dt == 0 || *dt == 0 || df == 0 || *df == 0 || nnew == 0)
        return -1L;    

    const uint32_t nold =
        (std::strcmp(dt, col->partition()->currentDataDir()) == 0 ?
         col->partition()->nRows()-nnew : nrows);
    long ierr;
    if (nrows == nold) { // can make use of the existing index
        std::string dfidx;
        indexFileName(dfidx, df);
        ibis::direkte* idxf = 0;
        ibis::fileManager::storage* stdf = 0;
        ierr = ibis::fileManager::instance().getFile(dfidx.c_str(), &stdf);
        if (ierr == 0 && stdf != 0) {
            const char* header = stdf->begin();
            if (header[0] == '#' && header[1] == 'I' && header[2] == 'B' &&
                header[3] == 'I' && header[4] == 'S' &&
                header[5] == ibis::index::DIREKTE &&
                (header[6] == 8 || header[6] == 4) &&
                header[7] == static_cast<char>(0)) {
                idxf = new ibis::direkte(col, stdf);
            }
            else {
                LOGGER(ibis::gVerbose > 5)
                    << "Warning -- direkte[" << col->fullname()
                    << "]::append -- file " << dfidx
                    << " has a unexpected header";
                remove(dfidx.c_str());
            }
        }
        if (idxf != 0 && idxf->nrows == nnew) {
            if (nold == 0) {
                nrows = idxf->nrows;
                str = idxf->str; idxf->str = 0;
                fname = 0;
                offset64.swap(idxf->offset64);
                offset32.swap(idxf->offset32);
                bits.swap(idxf->bits);
                delete idxf;
                //ierr = write(dt);
                return nnew;
            }

            activate(); // make sure all bitvectors are in memory
            if (bits.size() < idxf->bits.size()) {
                bits.reserve(idxf->bits.size());
            }
            uint32_t j = 0;
            while (j < idxf->bits.size()) {
                if (j >= bits.size()) {
                    bits.push_back(new ibis::bitvector);
                    bits[j]->set(0, nold);
                }
                if (idxf->bits[j] != 0) {
                    *(bits[j]) += *(idxf->bits[j]);
                }
                else {
                    bits[j]->adjustSize(nold, nold+nnew);
                }
                ++ j;
            }
            while (j < bits.size()) {
                if (bits[j] != 0)
                    bits[j]->adjustSize(nold, nold+nnew);
                ++ j;
            }

            delete idxf;
            //ierr = write(dt);
            return nnew;
        }
    }

    LOGGER(ibis::gVerbose > 4)
        << "direkte[" << col->fullname()
        << "]::append to recreate the index with the data from " << dt;
    clear();
    std::string dfname;
    dataFileName(dfname, dt);
    if (col->type() == ibis::CATEGORY)
        dfname += ".int";

    switch (col->type()) {
    default: {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- direkte can only be used "
            "for columns with integer values (current column " << col->name()
            << ", type=" <<  ibis::TYPESTRING[(int)col->type()] << ")";
        ierr = -2;
        return ierr;}
    case ibis::BYTE: {
        ierr = construct0<signed char>(dfname.c_str());
        break;}
    case ibis::UBYTE: {
        ierr = construct0<unsigned char>(dfname.c_str());
        break;}
    case ibis::SHORT: {
        ierr = construct0<int16_t>(dfname.c_str());
        break;}
    case ibis::USHORT: {
        ierr = construct0<uint16_t>(dfname.c_str());
        break;}
    case ibis::INT: {
        ierr = construct0<int32_t>(dfname.c_str());
        break;}
    case ibis::UINT:
    case ibis::CATEGORY: {
        ierr = construct0<uint32_t>(dfname.c_str());
        break;}
    case ibis::LONG: {
        ierr = construct0<int64_t>(dfname.c_str());
        break;}
    case ibis::ULONG: {
        ierr = construct0<uint64_t>(dfname.c_str());
        break;}
    }
    if (ierr < 0) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- direkte::construct failed with error code "
            << ierr;
    }
    else {
        if (ibis::gVerbose > 4) {
            ibis::util::logger lg;
            print(lg());
        }
        //ierr = write(dt);
        ierr = nnew;
    }
    return ierr;
} // ibis::direkte::append

/// Append tail to this index.  The incoming index must be for the same
/// column as this one.
long ibis::direkte::append(const ibis::direkte& tail) {
    if (tail.col != col) return -1;
    if (tail.bits.empty()) return 0;

    activate(); // need all bitvectors;
    tail.activate();

    const unsigned long ntot = nrows + tail.nrows;
    if (bits.size() == tail.bits.size()) { // perfect matching
        for (unsigned j = 0; j < bits.size(); ++ j) {
            if (bits[j] != 0) {
                if (bits[j]->size() != nrows)
                    bits[j]->adjustSize(0, nrows);
                if (tail.bits[j] != 0)
                    *(bits[j]) += *(tail.bits[j]);
            }
            else {
                bits[j] = new ibis::bitvector;
                bits[j]->set(nrows, 0);
                if (tail.bits[j] != 0)
                    *(bits[j]) += *(tail.bits[j]);
            }
            bits[j]->adjustSize(0, ntot);
        }
    }
    else if (bits.size() < tail.bits.size()) {
        const size_t nold = bits.size();
        for (unsigned j = 0; j < nold; ++ j) {
            if (bits[j] != 0) {
                if (bits[j]->size() != nrows)
                    bits[j]->adjustSize(0, nrows);
                if (tail.bits[j] != 0)
                    *(bits[j]) += *(tail.bits[j]);
            }
            else {
                bits[j] = new ibis::bitvector;
                bits[j]->set(nrows, 0);
                if (tail.bits[j] != 0)
                    *(bits[j]) += *(tail.bits[j]);
            }
            bits[j]->adjustSize(0, ntot);
        }

        bits.resize(tail.bits.size());
        for (unsigned j = nold; j < tail.bits.size(); ++ j) {
            if (tail.bits[j] != 0) {
                bits[j] = new ibis::bitvector;
                bits[j]->set(nrows, 0);
                *(bits[j]) += *(tail.bits[j]);
                bits[j]->adjustSize(0, ntot);
            }
            else {
                bits[j] = 0;
            }
        }
    }
    else {
        const size_t nold = tail.bits.size();
        for (unsigned j = 0; j < nold; ++ j) {
            if (bits[j] != 0) {
                if (bits[j]->size() != nrows)
                    bits[j]->adjustSize(0, nrows);
                if (tail.bits[j] != 0)
                    *(bits[j]) += *(tail.bits[j]);
            }
            else {
                bits[j] = new ibis::bitvector;
                bits[j]->set(nrows, 0);
                if (tail.bits[j] != 0)
                    *(bits[j]) += *(tail.bits[j]);
            }
            bits[j]->adjustSize(0, ntot);
        }

        for (unsigned j = nold; j < bits.size(); ++ j) {
            if (bits[j] != 0) {
                bits[j]->adjustSize(0, ntot);
            }
        }
    }

    nrows += tail.nrows;
    LOGGER(nrows != ntot && ibis::gVerbose >= 0) 
        << "Warning -- direkte[" << (col ? col->fullname() : "?.?")
        << "]::append the combined index has more 2^32 rows (too many rows)";
    if (ibis::gVerbose > 10) {
        ibis::util::logger lg;
        lg() << "\nAfter appending " << tail.nrows
             << " rows to this index, the total number of rows is "
             << nrows << "\n";
        print(lg());
    }
    return 0;
} // ibis::direkte::append

/// Append a list of integers.  The integers are treated as bin numbers.
/// This function is primarily used by ibis::category::append.
long ibis::direkte::append(const array_t<uint32_t>& ind) {
    if (ind.empty()) return 0; // nothing to do
    uint32_t i, nobs = bits.size();
    activate(); // need all bitvectors to be in memory
    for (i = 0; i < ind.size(); ++ i, ++ nrows) {
        const uint32_t j = ind[i];
        if (j >= nobs) { // extend the number of bitvectors
            for (uint32_t k = nobs; k <= j; ++ k) {
                bits.push_back(new ibis::bitvector);
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
    LOGGER(nset != nrows && ibis::gVerbose > 1)
        << "Warning -- direkte[" << (col ? col->fullname() : "?.?")
        << "]::append found the new index contains " << nset
        << " objects but the bitmap length is " << nrows;
    return ind.size();
} // ibis::direkte::append

double ibis::direkte::getSum() const {
    double ret = 0;
    activate(); // need all bitvectors
    for (unsigned j = 0; j < bits.size(); ++ j) {
        if (bits[j])
            ret += j * bits[j]->cnt();
    }
    return ret;
} // ibis::direkte::getSum

void ibis::direkte::binBoundaries(std::vector<double>& bb) const {
    bb.resize(bits.size());
    for (uint32_t i = 0; i < bits.size(); ++ i)
        bb[i] = i;
} // ibis::direkte::binBoundaries

void ibis::direkte::binWeights(std::vector<uint32_t>& cnts) const {
    activate();
    cnts.resize(bits.size());
    for (uint32_t j = 0; j < bits.size(); ++ j) {
        if (bits[j])
            cnts[j] = bits[j]->cnt();
        else
            cnts[j] = 0;
    }
} // ibis::direkte::binWeights

long ibis::direkte::getCumulativeDistribution
(std::vector<double>& bds, std::vector<uint32_t>& cts) const {
    activate();
    bds.resize(bits.size());
    cts.resize(bits.size());
    uint32_t sum = 0;
    for (uint32_t j = 0; j < bits.size(); ++ j) {
        bds[j] = j;
        cts[j] = sum;
        if (bits[j])
            sum += bits[j]->cnt();
    }
    return cts.size();
} // ibis::direkte::getCumulativeDistribution

long ibis::direkte::getDistribution
(std::vector<double>& bds, std::vector<uint32_t>& cts) const {
    activate();
    bds.reserve(bits.size());
    cts.reserve(bits.size());
    for (uint32_t j = 0; j < bits.size(); ++ j) {
        if (bits[j]) {
            cts.push_back(bits[j]->cnt());
            bds.push_back(j+1);
        }
    }
    bds.pop_back();
    return cts.size();
} // ibis::direkte::getDistribution

/// Estimate the size of the index file.  The index file contains primarily
/// the bitmaps.
size_t ibis::direkte::getSerialSize() const throw () {
    size_t res = 16;
    for (unsigned j = 0; j < bits.size(); ++ j)
        if (bits[j] != 0)
            res += bits[j]->getSerialSize();
    if (res + ((1+bits.size()) << 2) <= 0x80000000) {
        res += ((1+bits.size()) << 2);
    }
    else {
        res += ((1+bits.size()) << 3);
    }
    return res;
} // ibis::direkte::getSerialSize
