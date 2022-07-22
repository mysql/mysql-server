// $Id$
// Author: John Wu <John.Wu at ACM.org>
// Copyright (c) 2006-2016 the Regents of the University of California

// This file contains the implementation of the class ibis::fuge.  It
// defines a two-level index where the coarse level use the interval
// encoding, but the fine level contains only the simple bins.
//
// The word interstice (a synonym of interval) when translated to german,
// the answers.com web site gives two words: Zwischenraum and Fuge.  Since
// the word Fuge is only four letter long, it is similar to many
// variantions of the index class names -- very tangentially related to the
// index it represents.

#if defined(_WIN32) && defined(_MSC_VER)
#pragma warning(disable:4786)   // some identifier longer than 256 characters
#endif
#include "ibin.h"
#include "part.h"
#include "column.h"
#include "resource.h"

#include <string>

#define FASTBIT_SYNC_WRITE 1
//
ibis::fuge::fuge(const ibis::column *c, const char *f)
    : ibis::bin(c, f) {
    if (c == 0) return; // nothing to do
    if (cbits.empty() || cbits.size()+1 != cbounds.size()) {
        if (fname != 0)
            readCoarse(f);
        else
            coarsen();
    }
    if (ibis::gVerbose > 4) {
        ibis::util::logger lg;
        print(lg());
    }
} // ibis::fuge::fuge

// generate an ibis::fuge from ibis::bin
ibis::fuge::fuge(const ibis::bin& rhs) : ibis::bin(rhs) {
    if (col == 0) return;
    if (nobs <= 1) return; // rhs does not contain an valid index

    try {
        coarsen();
    }
    catch (...) {
        for (unsigned i = 0; i < cbits.size(); ++ i)
            delete cbits[i];
        cbits.clear();
        cbounds.clear();
        coffset32.clear();
        coffset64.clear();
    }
    if (ibis::gVerbose > 4) {
        ibis::util::logger lg;
        print(lg());
    }
} // copy from ibis::bin

/// Reconstruct from content of a file.
/**
   The leading portion of the index file is the same as ibis::bin, which
   allows the constructor of the base class to work properly.  The content
   following the last bitvector in ibis::bin is as follows, @sa
   ibis::fuge::writeCoarse.
@code
   nc      (uint32_t)                   -- number of coarse bins.
   cbounds (unsigned[nc+1])             -- boundaries of the coarse bins.
   coffsets([nc-ceil(nc/2)+2])          -- starting positions (32/64-bit).
   cbits   (bitvector[nc-ceil(nc/2)+1]) -- bitvectors.
@endcode
 */
ibis::fuge::fuge(const ibis::column* c, ibis::fileManager::storage* st,
                 size_t start) : ibis::bin(c, st, start) {
    const char offsetsize = st->begin()[6];
    if (offsetsize != 8 && offsetsize != 4) {
        clear();
        return;
    }
    if (offsetsize == 8)
        start = offset64.back();
    else
        start = offset32.back();
    if (st->size() <= start)
        return; // no coarse bin

    uint32_t nc = *(reinterpret_cast<uint32_t*>(st->begin()+start));
    if (nc == 0 ||
        st->size() <= start + (sizeof(int32_t)+offsetsize)*(nc+1))
        return;

    size_t end;
    const uint32_t ncb = nc - (nc+1)/2 + 1;
    start += sizeof(uint32_t);
    end = start + sizeof(uint32_t)*(nc+1);
    if (end < st->size()) {
        array_t<uint32_t> tmp(st, start, end);
        cbounds.swap(tmp);
    }
    start = end;
    end += offsetsize * (nc+1);
    if (offsetsize == 8) {
        array_t<int64_t> tmp(st, start, end);
        coffset64.swap(tmp);
        coffset32.clear();
        if (coffset64.back() > static_cast<int64_t>(st->size())) {
            coffset64.swap(tmp);
            array_t<uint32_t> tmp2;
            cbounds.swap(tmp2);
            return;
        }
    }
    else {
        array_t<int32_t> tmp(st, start, end);
        coffset32.swap(tmp);
        coffset64.clear();
        if (coffset32.back() > static_cast<int32_t>(st->size())) {
            coffset32.swap(tmp);
            array_t<uint32_t> tmp2;
            cbounds.swap(tmp2);
            return;
        }
    }

    cbits.resize(ncb);
    for (unsigned i = 0; i < ncb; ++ i)
        cbits[i] = 0;

    if (st->isFileMap()) {
#if defined(FASTBIT_READ_BITVECTOR0)
        if (offsetsize == 8) {
            array_t<ibis::bitvector::word_t>
                a0(st, coffset64[0], coffset64[1]);
            cbits[0] = new ibis::bitvector(a0);
        }
        else {
            array_t<ibis::bitvector::word_t>
                a0(st, coffset32[0], coffset32[1]);
            cbits[0] = new ibis::bitvector(a0);
        }
        cbits[0]->sloppySize(nrows);
#endif
    }
    else { // all bytes in memory already
        if (offsetsize == 8) {
            for (unsigned i = 0; i < ncb; ++ i) {
                if (coffset64[i+1] > coffset64[i]) {
                    array_t<ibis::bitvector::word_t>
                        a(st, coffset64[i], coffset64[i+1]);
                    cbits[i] = new ibis::bitvector(a);
                    cbits[i]->sloppySize(nrows);
                }
            }
        }
        else {
            for (unsigned i = 0; i < ncb; ++ i) {
                if (coffset32[i+1] > coffset32[i]) {
                    array_t<ibis::bitvector::word_t>
                        a(st, coffset32[i], coffset32[i+1]);
                    cbits[i] = new ibis::bitvector(a);
                    cbits[i]->sloppySize(nrows);
                }
            }
        }
    }

    if (ibis::gVerbose > 2) {
        ibis::util::logger lg;
        lg() << "fuge[" << col->partition()->name() << '.' << col->name()
             << "]::ctor -- initialized an interval-equality index with "
             << nobs << " fine bin" << (nobs>1?"s":"") << " and " << nc
             << " coarse bin" << (nc>1?"s":"") << " for "
             << nrows << " row" << (nrows>1?"s":"")
             << " from a storage object @ " << st;
        if (ibis::gVerbose > 6) {
            lg() << "\n";
            print(lg());
        }
    }
} // ibis::fuge::fuge

int ibis::fuge::write(const char* dt) const {
    if (nobs <= 0) return -1;

    std::string fnm, evt;
    evt = "fuge";
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
    if (fdes < 0) { // try again
        ibis::fileManager::instance().flushFile(fnm.c_str());
        fdes = UnixOpen(fnm.c_str(), OPEN_WRITENEW, OPEN_FILEMODE);
        if (fdes < 0) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << evt << " failed to open \"" << fnm
                << "\" for writing ... " << (errno ? strerror(errno) : "??");
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

#ifdef FASTBIT_USE_LONG_OFFSETS
    const bool useoffset64 = true;
#else
    const bool useoffset64 = (8+getSerialSize() > 0x80000000UL);
#endif
    const bool haveCoarseBins = ((cbounds.empty() || cbits.empty()) == false);
    char header[] = "#IBIS\4\0\0";
    header[5] = (char)(haveCoarseBins ? ibis::index::FUGE
                       : ibis::index::BINNING);
    header[6] = (char)(useoffset64 ? 8 : 4);
    int ierr = UnixWrite(fdes, header, 8);
    if (ierr < 8) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt
            << " failed to write the 8-byte header, ierr = " << ierr;
        return -3;
    }
    if (useoffset64)
        ierr = ibis::bin::write64(fdes); // write the basic binned index
    else
        ierr = ibis::bin::write32(fdes); // write the basic binned index
    if (ierr >= 0 && haveCoarseBins) {
        if (useoffset64)
            ierr = writeCoarse64(fdes); // write the coarse level bins
        else
            ierr = writeCoarse32(fdes); // write the coarse level bins
    }

    if (ierr >= 0) {
#if defined(FASTBIT_SYNC_WRITE)
#if  _POSIX_FSYNC+0 > 0
        (void) UnixFlush(fdes); // write to disk
#elif defined(_WIN32) && defined(_MSC_VER)
        (void) _commit(fdes);
#endif
#endif
        const uint32_t nc = (cbounds.size()-1 <= cbits.size() ?
                             cbounds.size()-1 : cbits.size());
        LOGGER(ibis::gVerbose > 5)
            << evt << " wrote " << nobs << " fine bitmap" << (nobs>1?"s":"")
            << " and " << nc << " coarse bitmap" << (nc>1?"s":"")
            << " to " << fnm;
    }
    return ierr;
} // ibis::fuge::write

/// Read the content of the named file.
int ibis::fuge::read(const char* f) {
    std::string fnm;
    indexFileName(fnm, f);

    int fdes = UnixOpen(fnm.c_str(), OPEN_READONLY);
    if (fdes < 0)
        return -1;

    char header[8];
#if defined(_WIN32) && defined(_MSC_VER)
    (void)_setmode(fdes, _O_BINARY);
#endif
    if (8 != UnixRead(fdes, static_cast<void*>(header), 8)) {
        UnixClose(fdes);
        return -2;
    }

    if (false == (header[0] == '#' && header[1] == 'I' &&
                  header[2] == 'B' && header[3] == 'I' &&
                  header[4] == 'S' &&
                  header[5] == static_cast<char>(ibis::index::FUGE) &&
                  (header[6] == 8 || header[6] == 4) &&
                  header[7] == static_cast<char>(0))) {
        if (ibis::gVerbose > 0) {
            ibis::util::logger lg;
            lg() << "Warning -- fuge[" << col->partition()->name() << '.'
                 << col->name() << "]::read the header from " << fnm << " (";
            printHeader(lg(), header);
            lg() << ") does not contain the expected values";
        }
        return -3;
    }

    clear(); // clear the existing content
    size_t begin, end;
    fname = ibis::util::strnewdup(fnm.c_str());
    str = 0;

    off_t ierr = UnixRead(fdes, static_cast<void*>(&nrows), sizeof(uint32_t));
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

    // initialized bits with nil pointers
    initBitmaps(fdes);

    // reading the coarse bins
    if (header[6] == 8) {
        coffset32.clear();
        ierr = UnixSeek(fdes, offset64.back(), SEEK_SET);
        if (ierr == offset64.back()) {
            uint32_t nc;
            ierr = UnixRead(fdes, &nc, sizeof(nc));
            if (ierr < (off_t)sizeof(nc)) {
                LOGGER(ibis::gVerbose > 0)
                    << "Warning -- fuge[" << col->partition()->name() << '.'
                    << col->name() << "]::read failed to read ncoarse from "
                    << fnm << " position " << offset64.back() << ", ierr = "
                    << ierr;
                clearCoarse();
                return -5;
            }
            begin = offset64.back() + sizeof(nc);
            end = begin + sizeof(uint32_t)*(nc+1);
            if (ierr > 0 && nc > 0) {
                array_t<uint32_t> tmp(fdes, begin, end);
                cbounds.swap(tmp);
            }
            begin = end;
            end += 8 * (nc+2-(nc+1)/2);
            if (cbounds.size() == nc+1) {
                array_t<int64_t> tmp(fdes, begin, end);
                coffset64.swap(tmp);
            }

            for (unsigned i = 0; i < cbits.size(); ++ i)
                delete cbits[i];
            cbits.resize(nc+1-(nc+1)/2);
            for (unsigned i = 0; i < nc+1-(nc+1)/2; ++ i)
                cbits[i] = 0;
        }
    }
    else {
        coffset64.clear();
        ierr = UnixSeek(fdes, offset32.back(), SEEK_SET);
        if (ierr == offset32.back()) {
            uint32_t nc;
            ierr = UnixRead(fdes, &nc, sizeof(nc));
            if (ierr < (off_t)sizeof(nc)) {
                LOGGER(ibis::gVerbose > 0)
                    << "Warning -- fuge[" << col->partition()->name() << '.'
                    << col->name() << "]::read failed to read ncoarse from "
                    << fnm << " position " << offset32.back() << ", ierr = "
                    << ierr;
                clearCoarse();
                return -6;
            }
            begin = offset32.back() + sizeof(nc);
            end = begin + sizeof(uint32_t)*(nc+1);
            if (ierr > 0 && nc > 0) {
                array_t<uint32_t> tmp(fdes, begin, end);
                cbounds.swap(tmp);
            }
            begin = end;
            end += sizeof(int32_t) * (nc+2-(nc+1)/2);
            if (cbounds.size() == nc+1) {
                array_t<int32_t> tmp(fdes, begin, end);
                coffset32.swap(tmp);
            }

            for (unsigned i = 0; i < cbits.size(); ++ i)
                delete cbits[i];
            cbits.resize(nc+1-(nc+1)/2);
            for (unsigned i = 0; i < nc+1-(nc+1)/2; ++ i)
                cbits[i] = 0;
        }
    }

    LOGGER(ibis::gVerbose > 3)
        << "fuge[" << col->partition()->name() << "." << col->name()
        << "]::read completed reading the header from " << fnm;
    return 0;
} // ibis::fuge::read

/// Read an index from the storage object.
int ibis::fuge::read(ibis::fileManager::storage* st) {
    if (st == 0) return -1;
    if (st->begin()[5] != static_cast<char>(FUGE)) return -3;
    int ierr = ibis::bin::read(st);
    if (ierr < 0) return ierr;
    const char offsetsize = (*st)[6];
    if (offsetsize != 8 && offsetsize != 4) return -2;
    clearCoarse();

    if (offsetsize == 8 &&
        str->size() > static_cast<uint64_t>(offset64.back())) {
        const uint32_t nc =
            *(reinterpret_cast<uint32_t*>(str->begin() + offset64.back()));
        const uint32_t ncb = nc + 1 - (nc+1) / 2;
        if (nc > 0 && str->size() > offset64.back() +
            (sizeof(int32_t)+sizeof(uint32_t))*(nc+1)) {
            uint32_t start;
            start = offset64.back() + 4;
            array_t<uint32_t> btmp(str, start, nc+1);
            cbounds.swap(btmp);

            start += sizeof(uint32_t)*(nc+1);
            array_t<int64_t> otmp(str, start, ncb+1);
            coffset64.swap(otmp);

            cbits.resize(ncb);
            for (unsigned i = 0; i < ncb; ++ i)
                cbits[i] = 0;
            if (! st->isFileMap()) {
                for (unsigned i = 0; i < ncb; ++ i) {
                    if (coffset64[i+1] > coffset64[i]) {
                        array_t<ibis::bitvector::word_t>
                            a(st, coffset64[i], coffset64[i+1]);
                        cbits[i] = new ibis::bitvector(a);
                        cbits[i]->sloppySize(nrows);
                    }
                }
            }
        }
        coffset32.clear();
    }
    else if (str->size() > static_cast<uint32_t>(offset32.back())) {
        const uint32_t nc =
            *(reinterpret_cast<uint32_t*>(str->begin() + offset32.back()));
        const uint32_t ncb = nc + 1 - (nc+1) / 2;
        if (nc > 0 && str->size() > offset32.back() +
            (sizeof(int32_t)+sizeof(uint32_t))*(nc+1)) {
            uint32_t start;
            start = offset32.back() + 4;
            array_t<uint32_t> btmp(str, start, nc+1);
            cbounds.swap(btmp);

            start += sizeof(uint32_t)*(nc+1);
            array_t<int32_t> otmp(str, start, ncb+1);
            coffset32.swap(otmp);

            cbits.resize(ncb);
            for (unsigned i = 0; i < ncb; ++ i)
                cbits[i] = 0;
            if (! st->isFileMap()) {
                for (unsigned i = 0; i < ncb; ++ i) {
                    if (coffset32[i+1] > coffset32[i]) {
                        array_t<ibis::bitvector::word_t>
                            a(st, coffset32[i], coffset32[i+1]);
                        cbits[i] = new ibis::bitvector(a);
                        cbits[i]->sloppySize(nrows);
                    }
                }
            }
        }
        coffset64.clear();
    }
    return 0;
} // ibis::fuge::read

// fill with zero bits or truncate
void ibis::fuge::adjustLength(uint32_t nr) {
    ibis::bin::adjustLength(nr); // the top level
    for (unsigned j = 0; j < cbits.size(); ++ j)
        if (cbits[j] != 0)
            cbits[j]->adjustSize(0, nr);
} // ibis::fuge::adjustLength

// the printing function
void ibis::fuge::print(std::ostream& out) const {
    const uint32_t nc = (cbounds.empty() ? 0U : cbounds.size()-1);
    const uint32_t ncb = nc+1 - (nc+1)/2;
    out << "index (binned interval-equality code) for "
        << col->partition()->name() << '.' << col->name()
        << " contains " << nc << " coarse bin" << (nc > 1 ? "s" : "")
        << ", " << nobs << " fine bins for " << nrows << " objects \n";
    uint32_t nprt = (ibis::gVerbose < 30 ? 1 << ibis::gVerbose : bits.size());
    uint32_t omitted = 0;
    uint32_t end;
    if (nc > 0 && cbits.size() == ncb) {
        for (unsigned j = 0; j < nc; ++ j) {
            out << "Coarse bin " << j << ", [" << cbounds[j] << ", "
                << cbounds[j+1] << ")";
            if (j < ncb && cbits[j] != 0)
                out << "\t{[" << bounds[j] << ", " << cbounds[j+(nc+1)/2]
                    << ")\t" << cbits[j]->cnt() << "\t" << cbits[j]->bytes()
                    << "}";
            out << "\n";
            end = (cbounds[j+1] <= cbounds[j]+nprt ?
                   cbounds[j+1] : cbounds[j]+nprt);
            for (unsigned i = cbounds[j]; i < end; ++ i) {
                out << "\t" << i << ": ";
                if (i > 0)
                    out << "[" << bounds[i-1];
                else
                    out << "(...";
                out << ", " << bounds[i] << ")\t[" << minval[i]
                    << ", " << maxval[i] << "]";
                if (bits[i] != 0)
                    out << "\t" << bits[i]->cnt() << "\t"
                        << bits[i]->bytes();
                out << "\n";
            }
            if (cbounds[j+1] > end) {
                out << "\t...\n";
                omitted += (cbounds[j+1] - end);
            }
        }
        if (omitted > 0)
            out << "\tfine level bins omitted: " << omitted << "\n";
    }
    else {
        end = (nobs <= nprt ? nobs : nprt);
        for (unsigned i = 0; i < end; ++ i) {
            out << "\t" << i << ": ";
            if (i > 0)
                out << "[" << bounds[i-1];
            else
                out << "(...";
            out << ", " << bounds[i] << ")\t[" << minval[i]
                << ", " << maxval[i] << "]";
            if (bits[i] != 0)
                out << "\t" << bits[i]->cnt() << "\t"
                    << bits[i]->bytes();
            out << "\n";
        }
        if (end < nobs)
            out << "\tbins omitted: " << nobs - end << "\n";
    }
    out << std::endl;
} // ibis::fuge::print

long ibis::fuge::append(const char* dt, const char* df, uint32_t nnew) {
    long ret = ibis::bin::append(dt, df, nnew);
    if (ret <= 0 || static_cast<uint32_t>(ret) != nnew)
        return ret;

    if (nrows == col->partition()->nRows()) {
        clearCoarse();
        coarsen();
    }
    return ret;
} // ibis::fuge::append

long ibis::fuge::append(const ibis::fuge& tail) {
    long ret = ibis::bin::append(tail);
    if (ret < 0) return ret;

    clearCoarse();
    coarsen();
    return ret;
} // ibis::fuge::append

long ibis::fuge::evaluate(const ibis::qContinuousRange& expr,
                          ibis::bitvector& lower) const {
    if (col == 0 || col->partition() == 0) return -1;
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
} // ibis::fuge::evaluate

// compute the lower and upper bound of the hit vector for the range
// expression
void ibis::fuge::estimate(const ibis::qContinuousRange& expr,
                          ibis::bitvector& lower,
                          ibis::bitvector& upper) const {
    if (bits.empty()) {
        lower.set(0, nrows);
        upper.set(1, nrows);
        return;
    }

    // bins in the range of [hit0, hit1) are hits
    // bins in the range of [cand0, cand1) are candidates
    uint32_t cand0, hit0, hit1, cand1;
    locate(expr, cand0, cand1, hit0, hit1);
    if (cand0 >= cand1 || cand1 == 0 || cand0 >= nobs) { // no hits at all
        lower.set(0, nrows);
        upper.clear();
        return;
    }
    else if (hit0 >= hit1) { // no sure hits, but some candidates
        lower.set(0, nrows);
        if (bits[cand0] == 0)
            activate(cand0);
        if (bits[cand0] != 0)
            upper.copy(*bits[cand0]);
        else
            upper.clear();
    }

    const uint32_t ncoarse = (cbounds.empty() ? 0U : cbounds.size()-1);
    if (hit0+3 >= hit1 || ncoarse == 0
        || ((cbits.size()+1) != coffset32.size() &&
            (cbits.size()+1) != coffset64.size())
        || cbits.size() != (ncoarse-(ncoarse+1)/2+1)) {
        // use the fine level bitmaps only
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
        }
        else {
            upper.clear();
        }
        return;
    }

    // see whether the coarse bins could help
    const uint32_t c0 = cbounds.find(hit0);
    const uint32_t c1 = cbounds.find(hit1);
    if (ibis::gVerbose > 4) {
        ibis::util::logger lg;
        lg() << "fuge::evaluate(" << expr << ") hit0=" << hit0
             << ", hit1=" << hit1;
        if (c0 < cbounds.size())
            lg() << ", cbounds[" << c0 << "]=" << cbounds[c0];
        else
            lg() << ", cbounds[" << cbounds.size()-1 << "]=" << cbounds.back();
        if (c1 < cbounds.size())
            lg() << ", cbounds[" << c1 << "]=" << cbounds[c1];
        else
            lg() << ", c1=" << c1 << ", bits.size()=" << bits.size();
    }
    if (c0 >= c1) { // within the same coarse bin
        long tmp = coarseEstimate(c1-1, c1);
        long tmpf;
        if (offset64.size() > bits.size()) {
            tmp += (offset64[hit0] - offset64[cbounds[c1-1]])
                + (offset64[cbounds[c1]] - offset64[hit1]);
            tmpf = offset64[hit1] - offset64[hit0];
        }
        else {
            tmp += (offset32[hit0] - offset32[cbounds[c1-1]])
                + (offset32[cbounds[c1]] - offset32[hit1]);
            tmpf = offset32[hit1] - offset32[hit0];
        }
        if (tmpf <= static_cast<long>(0.99*tmp)) {
            sumBins(hit0, hit1, lower);
        }
        else {
            coarseEvaluate(c1-1, c1, lower);
            if (hit0 > cbounds[c1-1]) {
                ibis::bitvector bv;
                sumBins(cbounds[c1-1], hit0, bv);
                lower -= bv;
            }
            if (cbounds[c1] > hit1) {
                ibis::bitvector bv;
                sumBins(hit1, cbounds[c1], bv);
                lower -= bv;
            }
        }
    }
    else {// general case: need to evaluate 5 options
        unsigned option = 2; // option 2 [direct | - | direct]
        long cost = coarseEstimate(c0, c1-1);
        if (offset64.size() > bits.size())
            cost += (offset64[cbounds[c0]] - offset64[hit0])
                + (offset64[hit1] - offset64[cbounds[c1-1]]);
        else
            cost += (offset32[cbounds[c0]] - offset32[hit0])
                + (offset32[hit1] - offset32[cbounds[c1-1]]);
        long tmp;
        if (c0 > 0) {   // option 3: [complement | - | direct]
            tmp = coarseEstimate(c0-1, c1-1);
            if (offset64.size() > bits.size())
                tmp += (offset64[hit0] - offset64[cbounds[c0-1]])
                    + (offset64[hit1] - offset64[cbounds[c1-1]]);
            else
                tmp += (offset32[hit0] - offset32[cbounds[c0-1]])
                    + (offset32[hit1] - offset32[cbounds[c1-1]]);
            if (tmp < cost) {
                cost = tmp;
                option = 3;
            }
        }
        // option 4: [direct | - | complement]
        tmp = coarseEstimate((c0>0 ? c0-1 : 0), c1);
        if (offset64.size() > bits.size())
            tmp += (offset64[cbounds[c0]] - offset64[hit0])
                + (offset64[cbounds[c1]] - offset64[hit1]);
        else
            tmp += (offset32[cbounds[c0]] - offset32[hit0])
                + (offset32[cbounds[c1]] - offset32[hit1]);
        if (tmp < cost) {
            cost = tmp;
            option = 4;
        }
        if (c0 > 0) { // option 5: [complement | - | complement]
            tmp = coarseEstimate(c0-1, c1);
            if (offset64.size() > bits.size())
                tmp += (offset64[hit0] - offset64[cbounds[c0-1]])
                    + (offset64[cbounds[c1]] - offset64[hit1]);
            else
                tmp += (offset32[hit0] - offset32[cbounds[c0-1]])
                    + (offset32[cbounds[c1]] - offset32[hit1]);
            if (tmp < cost) {
                cost = tmp;
                option = 5;
            }
        }
        // option 0 and 1: fine level only
        if (offset64.size() > bits.size())
            tmp = (offset64[hit1] - offset64[hit0] <=
                   (offset64.back()-offset64[hit1])
                   +(offset64[hit0]-offset64[0]) ?
                   offset64[hit1] - offset64[hit0] :
                   (offset64.back()-offset64[hit1])
                   +(offset64[hit0]-offset64[0]));
        else
            tmp = (offset32[hit1] - offset32[hit0] <=
                   (offset32.back()-offset32[hit1])
                   +(offset32[hit0]-offset32[0]) ?
                   offset32[hit1] - offset32[hit0] :
                   (offset32.back()-offset32[hit1])
                   +(offset32[hit0]-offset32[0]));
        if (cost > static_cast<long>(0.99*tmp)) { // slightly prefer 0/1
            cost = tmp;
            option = 1;
        }
        switch (option) {
        default:
        case 1: // use fine level only
            sumBins(hit0, hit1, lower);
            break;
        case 2: // direct | - | direct
            coarseEvaluate(c0, c1-1, lower);
            if (hit0 < cbounds[c0])
                addBins(hit0, cbounds[c0], lower); // left edge bin
            if (cbounds[c1-1] < hit1)
                addBins(cbounds[c1-1], hit1, lower); // right edge bin
            break;
        case 3: // complement | - | direct
            coarseEvaluate(c0-1, c1-1, lower);
            if (cbounds[c0-1] < hit0) { // left edge bin, complement
                ibis::bitvector bv;
                sumBins(cbounds[c0-1], hit0, bv);
                lower -= bv;
            }
            if (cbounds[c1-1] < hit1)
                addBins(cbounds[c1-1], hit1, lower); // right edge bin
            break;
        case 4: // direct | - | complement
            coarseEvaluate(c0, c1, lower);
            if (hit0 < cbounds[c0])
                addBins(hit0, cbounds[c0], lower); // left edge bin
            if (cbounds[c1] > hit1) { // right edge bin
                ibis::bitvector bv;
                sumBins(hit1, cbounds[c1], bv);
                lower -= bv;
            }
            break;
        case 5: // complement | - | complement
            coarseEvaluate(c0-1, c1, lower);
            if (hit0 > cbounds[c0-1]) { // left edge bin
                ibis::bitvector bv;
                sumBins(cbounds[c0-1], hit0, bv);
                lower -= bv;
            }
            if (cbounds[c1] > hit1) { // right edge bin
                ibis::bitvector bv;
                sumBins(hit1, cbounds[c1], bv);
                lower -= bv;
            }
        }
    }

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
    }
    else {
        upper.clear();
    }
} // ibis::fuge::estimate

// fill the offsets array, and divide the bitmaps into groups according to
// the sizes (bytes) of the bitmaps
void ibis::fuge::coarsen() {
    const uint32_t nbits = bits.size();
    if (offset32.size() != nbits+1) {
        offset32.resize(nbits+1);
        offset32[0] = 0;
        for (unsigned i = 0; i < nbits; ++ i)
            offset32[i+1] = offset32[i] + (bits[i] ? bits[i]->bytes() : 0U);
    }
    if (nobs < 32) return; // don't construct the coarse level
    if (cbits.size() > 0) return; // assume coarse bin already exist

    // default size based on the size of fine level index sf: sf(w-1)/N/sqrt(2)
    unsigned ncoarse = 0;
    if (col != 0) { // limit the scope of variables
        const char* spec = col->indexSpec();
        if (spec != 0 && *spec != 0 && strstr(spec, "ncoarse=") != 0) {
            // number of coarse bins specified explicitly
            const char* tmp = 8+strstr(spec, "ncoarse=");
            unsigned j = strtol(tmp, 0, 0);
            if (j > 4)
                ncoarse = j;
        }
    }
    if (ncoarse < 5U && offset32.back() >
        offset32[0]+static_cast<int32_t>(nrows/31)) {
        ncoarse = sizeof(ibis::bitvector::word_t);
        const int wm1 = ncoarse*8 - 1;
        const long sf = (offset32.back()-offset32[0]) / ncoarse;
        ncoarse = static_cast<unsigned>(wm1*sf/(sqrt(2.0)*nrows));
        const unsigned ncmax = (unsigned) sqrt(2.0 * nobs);
        if (ncoarse < ncmax) {
            const double obj1 = (sf+(ncoarse+1-ceil(0.5*ncoarse))*nrows/wm1)
                *(sf*0.5/ncoarse+2.0*nrows/wm1);
            const double obj2 = (sf+(ncoarse+2-ceil(0.5*ncoarse+0.5))*nrows/wm1)
                *(sf*0.5/(ncoarse+1.0)+2.0*nrows/wm1);
            ncoarse += (obj2 < obj1);
        }
        else {
            ncoarse = ncmax;
        }
    }
    if (ncoarse < 5 || ncoarse >= nobs) return;

    const uint32_t nc2 = (ncoarse + 1) / 2;
    const uint32_t ncb = ncoarse - nc2 + 1; // # of coarse level bitmaps
    // partition the fine level bitmaps into groups with nearly equal
    // number of bytes
    cbounds.resize(ncoarse+1);
    cbounds[0] = 0;
    for (unsigned i = 1; i < ncoarse; ++ i) {
        int32_t target = offset32[cbounds[i-1]] +
            (offset32.back() - offset32[cbounds[i-1]]) / (ncoarse - i + 1);
        cbounds[i] = offset32.find(target);
        if (cbounds[i] > cbounds[i-1]+1 &&
            offset32[cbounds[i]]-target > target-offset32[cbounds[i]-1])
            -- (cbounds[i]);
        else if (cbounds[i] <= cbounds[i-1])
            cbounds[i] = cbounds[i-1]+1;
    }
    cbounds[ncoarse] = nbits; // end with the last fine level bitmap
    for (unsigned i = ncoarse-1; i > 0 && cbounds[i+1] < cbounds[i]; -- i)
        cbounds[i] = cbounds[i+1] - 1;
    if (ibis::gVerbose > 2) {
        ibis::util::logger lg;
        lg() << "fuge::coarsen will divide " << bits.size()
             << " bitmaps into " << ncoarse << " groups\n";
        for (unsigned i = 0; i < cbounds.size(); ++ i)
            lg() << cbounds[i] << " ";
        lg() << "\n";
    }
    // fill cbits
    for (unsigned i = 0; i < cbits.size(); ++ i) {
        delete cbits[i];
        cbits[i] = 0;
    }
    cbits.resize(ncb);
    cbits[0] = new ibis::bitvector();
    sumBins(0, cbounds[nc2], *(cbits[0]));
    for (unsigned i = 1; i < ncb; ++ i) {
        ibis::bitvector front, back;
        sumBins(cbounds[i-1], cbounds[i], front);
        sumBins(cbounds[i-1+nc2], cbounds[i+nc2], back);
        cbits[i] = new ibis::bitvector(*(cbits[i-1]));
        *(cbits[i]) -= front;
        *(cbits[i]) |= back;
    }

    // fill coffsets
    coffset64.resize(ncb+1);
    coffset64[0] = 0;
    for (unsigned i = 0; i < ncb; ++ i) {
        cbits[i]->compress();
        coffset64[i+1] = coffset64[i] + cbits[i]->bytes();
    }
} // ibis::fuge::coarsen

/// Write information about the coarse bins.  It assume 32-bit bitmap
/// offsets.  This function is intended to be called after calling
/// ibis::bin::write, however, it does not check for this fact!
int ibis::fuge::writeCoarse32(int fdes) const {
    if (cbounds.empty() || cbits.empty() || nrows == 0)
        return -14;

    const off_t start = UnixSeek(fdes, 0, SEEK_CUR);
    if (start <= 8) return -15;

    off_t ierr;
    const uint32_t nc = cbounds.size()-1;
    const uint32_t nb = cbits.size();
    coffset64.clear();
    coffset32.resize(nb+1);
    ierr  = UnixWrite(fdes, &nc, sizeof(uint32_t));
    ierr += UnixWrite(fdes, cbounds.begin(), sizeof(uint32_t)*(nc+1));
    if (ierr < (off_t)(sizeof(uint32_t)*(nc+2))) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fuge[" << col->partition()->name() << "."
            << col->name() << "]::writeCoarse32(" << fdes
            << ") failed expected to write " << sizeof(uint32_t)*(nc+2)
            << " bytes, but the function write returned " << ierr;
        (void) UnixSeek(fdes, start, SEEK_SET);
        return -16;
    }
    ierr = UnixSeek(fdes, sizeof(int32_t)*(nb+1), SEEK_CUR);
    coffset32[0] = start + sizeof(uint32_t)*(nc+2) + sizeof(int32_t)*(nb+1);
    if (ierr != coffset32[0]) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fuge[" << col->partition()->name() << "."
            << col->name() << "]::writeCoarse32(" << fdes
            << ") expected the file pointer to be at "
            << coffset32[0] << ", but actually at " << ierr;
        (void) UnixSeek(fdes, start, SEEK_SET);
        return -17;
    }
    for (unsigned i = 0; i < nb; ++ i) {
        if (cbits[i] != 0)
            cbits[i]->write(fdes);
        coffset32[i+1] = UnixSeek(fdes, 0, SEEK_CUR);
    }
    ierr = start + sizeof(uint32_t)*(nc+2);
    if (ierr != UnixSeek(fdes, ierr, SEEK_SET)) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fuge[" << col->partition()->name() << "."
            << col->name() << "]::writeCoarse32(" << fdes
            << ") failed to seek to " << ierr;
        (void) UnixSeek(fdes, start, SEEK_SET);
        return -18;
    }
    ierr = UnixWrite(fdes, coffset32.begin(), sizeof(int32_t)*(nb+1));
    if (ierr < static_cast<off_t>(sizeof(int32_t)*(nb+1))) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fuge[" << col->partition()->name() << "."
            << col->name() << "]::writeCoarse32(" << fdes
            << ") expected to write " << sizeof(int32_t)*(nb+1)
            << " bytes, but the function write returned " << ierr;
        (void) UnixSeek(fdes, start, SEEK_SET);
        return -18;
    }
    ierr = UnixSeek(fdes, coffset32.back(), SEEK_SET);
    return (ierr == coffset32.back() ? 0 : -19);
} // ibis::fuge::writeCoarse32

/// Write information about the coarse bins.  It assume 64-bit bitmap
/// offsets.  This function is intended to be called after calling
/// ibis::bin::write, however, it does not check for this fact!
int ibis::fuge::writeCoarse64(int fdes) const {
    if (cbounds.empty() || cbits.empty() || nrows == 0)
        return -14;

    const off_t start = UnixSeek(fdes, 0, SEEK_CUR);
    if (start <= 8) return -15;

    off_t ierr;
    const uint32_t nc = cbounds.size()-1;
    const uint32_t nb = cbits.size();
    coffset32.clear();
    coffset64.resize(nb+1);
    ierr  = UnixWrite(fdes, &nc, sizeof(uint32_t));
    ierr += UnixWrite(fdes, cbounds.begin(), sizeof(uint32_t)*(nc+1));
    if (ierr < static_cast<off_t>(sizeof(uint32_t)*(nc+2))) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fuge[" << col->partition()->name() << "."
            << col->name() << "]::writeCoarse64(" << fdes
            << ") failed expected to write " << sizeof(uint32_t)*(nc+2)
            << " bytes, but the function write returned " << ierr;
        (void) UnixSeek(fdes, start, SEEK_SET);
        return -16;
    }
    ierr = UnixSeek(fdes, sizeof(int64_t)*(nb+1), SEEK_CUR);
    coffset64[0] = start + sizeof(uint32_t)*(nc+2) + sizeof(int64_t)*(nb+1);
    if (ierr != coffset64[0]) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fuge[" << col->partition()->name() << "."
            << col->name() << "]::writeCoarse64(" << fdes
            << ") expected the file pointer to be at "
            << coffset64[0] << ", but actually at " << ierr;
        (void) UnixSeek(fdes, start, SEEK_SET);
        return -17;
    }
    for (unsigned i = 0; i < nb; ++ i) {
        if (cbits[i] != 0)
            cbits[i]->write(fdes);
        coffset64[i+1] = UnixSeek(fdes, 0, SEEK_CUR);
    }
    ierr = start + sizeof(uint32_t)*(nc+2);
    if (ierr != UnixSeek(fdes, ierr, SEEK_SET)) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fuge[" << col->partition()->name() << "."
            << col->name() << "]::writeCoarse64(" << fdes
            << ") failed to seek to " << ierr;
        (void) UnixSeek(fdes, start, SEEK_SET);
        return -18;
    }
    ierr = UnixWrite(fdes, coffset64.begin(), sizeof(int64_t)*(nb+1));
    if (ierr < static_cast<off_t>(sizeof(int64_t)*(nb+1))) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fuge[" << col->partition()->name() << "."
            << col->name() << "]::writeCoarse64(" << fdes
            << ") expected to write " << sizeof(int64_t)*(nb+1)
            << " bytes, but the function write returned " << ierr;
        (void) UnixSeek(fdes, start, SEEK_SET);
        return -18;
    }
    ierr = UnixSeek(fdes, coffset64.back(), SEEK_SET);
    return (ierr == coffset64.back() ? 0 : -19);
} // ibis::fuge::writeCoarse64

/// Reading information about the coarse bins.  To be used after calling
/// ibis::bin::read.
int ibis::fuge::readCoarse(const char* fn) {
    std::string fnm;
    indexFileName(fnm, fn);

    // check to make sure either offset32 or offset64 is ready for use
    if (offset64.size() <= bits.size() &&
        offset32.size() <= bits.size()) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fuge[" << col->partition()->name() << "."
            << col->name() << "]::readCoarse(" << fnm << ") can not proceed "
            "because neither offset64 nor offset32 is set correctly";
        return -1;
    }
    const bool useoffset64 = (offset64.size() > bits.size());
    int fdes = UnixOpen(fnm.c_str(), OPEN_READONLY);
    if (fdes < 0) return -2;
    IBIS_BLOCK_GUARD(UnixClose, fdes);
#if defined(_WIN32) && defined(_MSC_VER)
    (void)_setmode(fdes, _O_BINARY);
#endif

    long ierr;
    if (useoffset64) {
        ierr = UnixSeek(fdes, offset64.back(), SEEK_SET);
        if (ierr == offset64.back()) {
            uint32_t nc;
            size_t begin, end;
            ierr = UnixRead(fdes, &nc, sizeof(nc));
            if (ierr <= 0 || static_cast<uint32_t>(ierr) != sizeof(nc)) {
                return -3;
            }

            begin = offset64.back() + sizeof(nc);
            end = begin + sizeof(uint32_t)*(nc+1);
            if (ierr > 0 && nc > 0) {
                array_t<uint32_t> tmp(fdes, begin, end);
                cbounds.swap(tmp);
            }
            const uint32_t ncb = nc+1-(nc+1)/2;
            begin = end;
            end += sizeof(int64_t) * (ncb+1);
            if (cbounds.size() == nc+1) {
                array_t<int64_t> tmp(fdes, begin, end);
                coffset64.swap(tmp);
            }

            for (unsigned i = 0; i < cbits.size(); ++ i)
                delete cbits[i];
            cbits.resize(ncb);
            for (unsigned i = 0; i < ncb; ++ i)
                cbits[i] = 0;
        }
        else {
            clearCoarse();
        }
    }
    else {
        ierr = UnixSeek(fdes, offset32.back(), SEEK_SET);
        if (ierr == offset32.back()) {
            uint32_t nc, begin, end;
            ierr = UnixRead(fdes, &nc, sizeof(nc));
            if (ierr <= 0 || static_cast<uint32_t>(ierr) != sizeof(nc)) {
                return -4;
            }

            begin = offset32.back() + sizeof(nc);
            end = begin + sizeof(uint32_t)*(nc+1);
            if (ierr > 0 && nc > 0) {
                array_t<uint32_t> tmp(fdes, begin, end);
                cbounds.swap(tmp);
            }
            const uint32_t ncb = nc+1-(nc+1)/2;
            begin = end;
            end += sizeof(int32_t) * (ncb+1);
            if (cbounds.size() == nc+1) {
                array_t<int32_t> tmp(fdes, begin, end);
                coffset32.swap(tmp);
            }

            for (unsigned i = 0; i < cbits.size(); ++ i)
                delete cbits[i];
            cbits.resize(ncb);
            for (unsigned i = 0; i < ncb; ++ i)
                cbits[i] = 0;
        }
        else {
            clearCoarse();
        }
    }

    LOGGER(ibis::gVerbose > 6)
        << "pack[" << col->partition()->name() << "." << col->name()
        << "]::read completed reading the header from " << fnm;
    return 0;
} // ibis::fuge::readCoarse

void ibis::fuge::clearCoarse() {
    const unsigned nb = cbits.size();
    for (unsigned i = 0; i < nb; ++ i)
        delete cbits[i];

    cbits.clear();
    cbounds.clear();
    coffset32.clear();
    coffset64.clear();
} // ibis::fuge::clearCoarse

void ibis::fuge::activateCoarse() const {
    std::string mesg = "fuge";
    if (ibis::gVerbose > 0) {
        mesg += '[';
        mesg += col->partition()->name();
        mesg += '.';
        mesg += col->name();
        mesg += ']';
    }
    mesg += "::activateCoarse";
    const uint32_t nobs = cbits.size();
    bool missing = false; // any bits[i] missing (is 0)?
    ibis::column::mutexLock lock(col, mesg.c_str());
    for (uint32_t i = 0; i < nobs && ! missing; ++ i)
        missing = (cbits[i] == 0);
    if (missing == false) return;

    if ((coffset64.size() <= nobs || coffset64[0] <= offset64.back()) &&
        (coffset32.size() <= nobs || coffset32[0] <= offset32.back())) {
        col->logWarning("fuge::activateCoarse", "no records of coffset32 or "
                        "coffset64, can not regenerate the bitvectors");
    }
    else if (coffset64.size() > nobs && coffset64.back() > coffset64.front()) {
        if (str != 0) { // using a ibis::fileManager::storage as back store
            LOGGER(ibis::gVerbose > 8)
                << mesg << " retrieving data from fileManager::storage(0x"
                << str << ")";

            for (uint32_t i = 0; i < nobs; ++i) {
                if (cbits[i] == 0 && coffset64[i+1] > coffset64[i]) {
// #if DEBUG+0 > 1 || _DEBUG+0 > 1
//                  LOGGER(ibis::gVerbose > 5)
//                      << "DEBUG -- " << mesg << " activating bitvector "
//                      << i << " from a raw storage ("
//                      << static_cast<const void*>(str->begin())
//                      << "), coffset64[" << i << "]= " << coffset64[i]
//                      << ", coffset64[" << i+1 << "]= " << coffset64[i+1];
// #endif
                    array_t<ibis::bitvector::word_t>
                        a(str, coffset64[i], coffset64[i+1]);
                    cbits[i] = new ibis::bitvector(a);
                    cbits[i]->sloppySize(nrows);
                }
            }
        }
        else if (fname != 0) { // using the named file directly
            int fdes = UnixOpen(fname, OPEN_READONLY);
            if (fdes >= 0) {
                LOGGER(ibis::gVerbose > 8)
                    << mesg << " retrieving data from file \"" << fname << "\"";

#if defined(_WIN32) && defined(_MSC_VER)
                (void)_setmode(fdes, _O_BINARY);
#endif
                uint32_t i = 0;
                while (i < nobs) {
                    // skip to next empty bit vector
                    while (i < nobs && cbits[i] != 0)
                        ++ i;
                    // the last bitvector to activate. can not be larger
                    // than j
                    uint32_t aj = (i<nobs ? i + 1 : nobs);
                    while (aj < nobs && cbits[aj] == 0)
                        ++ aj;
                    if (coffset64[aj] > coffset64[i]) {
                        const uint32_t start = coffset64[i];
                        ibis::fileManager::storage *a0 = new
                            ibis::fileManager::storage(fdes, start,
                                                       coffset64[aj]);
                        while (i < aj) {
                            if (coffset64[i+1] > coffset64[i]) {
                                array_t<ibis::bitvector::word_t>
                                    a1(a0, coffset64[i]-start,
                                       coffset64[i+1]-start);
                                bits[i] = new ibis::bitvector(a1);
                                bits[i]->sloppySize(nrows);
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                                LOGGER(ibis::gVerbose >= 0)
                                    << "DEBUG -- " << mesg
                                    << " activating bitvector " << i
                                    << "by reading file " << fname
                                    << "coffset64[" << i << "]= "
                                    << coffset64[i]
                                    << ", coffset64[" << i+1 << "]= "
                                    << coffset64[i+1];
#endif
                            }
                            ++ i;
                        }
                    }
                    i = aj; // always advance i
                }
                UnixClose(fdes);
            }
            else {
                LOGGER(ibis::gVerbose > 0)
                    << "Warning -- " << mesg << " failed to open file \""
                    << fname << "\" for reading";
            }
        }
        else {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << mesg << "can not regenerate bitvectors "
                "because neither str or fname is specified";
        }
    }
    else if (str != 0) { // using a ibis::fileManager::storage as back store
        LOGGER(ibis::gVerbose > 8)
            << mesg << " retrieving data from fileManager::storage(0x"
            << str << ")";

        for (uint32_t i = 0; i < nobs; ++i) {
            if (cbits[i] == 0 && coffset32[i+1] > coffset32[i]) {
// #if DEBUG+0 > 1 || _DEBUG+0 > 1
//              LOGGER(ibis::gVerbose >= 0)
//                  << "DEBUG -- " << mesg << " activating bitvector "
//                  << i << " from a raw storage ("
//                  << static_cast<const void*>(str->begin())
//                  << "), coffset32[" << i << "]= " << coffset32[i]
//                  << ", coffset32[" << i+1 << "]= " << coffset32[i+1];
// #endif
                array_t<ibis::bitvector::word_t>
                    a(str, coffset32[i], coffset32[i+1]);
                cbits[i] = new ibis::bitvector(a);
                cbits[i]->sloppySize(nrows);
            }
        }
    }
    else if (fname != 0) { // using the named file directly
        int fdes = UnixOpen(fname, OPEN_READONLY);
        if (fdes >= 0) {
            LOGGER(ibis::gVerbose > 8)
                << mesg << " retrieving data from file \"" << fname << "\"";

#if defined(_WIN32) && defined(_MSC_VER)
            (void)_setmode(fdes, _O_BINARY);
#endif
            uint32_t i = 0;
            while (i < nobs) {
                // skip to next empty bit vector
                while (i < nobs && cbits[i] != 0)
                    ++ i;
                // the last bitvector to activate.  can not be larger than
                // j
                uint32_t aj = (i<nobs ? i + 1 : nobs);
                while (aj < nobs && cbits[aj] == 0)
                    ++ aj;
                if (coffset32[aj] > coffset32[i]) {
                    const uint32_t start = coffset32[i];
                    ibis::fileManager::storage *a0 = new
                        ibis::fileManager::storage(fdes, start,
                                                   coffset32[aj]);
                    while (i < aj) {
                        if (coffset32[i+1] > coffset32[i]) {
                            array_t<ibis::bitvector::word_t>
                                a1(a0, coffset32[i]-start,
                                   coffset32[i+1]-start);
                            bits[i] = new ibis::bitvector(a1);
                            bits[i]->sloppySize(nrows);
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                            LOGGER(ibis::gVerbose >= 0)
                                << "DEBUG -- " << mesg
                                << " activating bitvector " << i
                                << "by reading file " << fname
                                << "coffset32[" << i << "]= " << coffset32[i]
                                << ", coffset32[" << i+1 << "]= "
                                << coffset32[i+1];
#endif
                        }
                        ++ i;
                    }
                }
                i = aj; // always advance i
            }
            UnixClose(fdes);
        }
        else {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << mesg << " failed to open file \""
                << fname << "\" for reading";
        }
    }
    else {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << mesg << "can not regenerate bitvectors because "
            "neither str or fname is specified";
    }
} // ibis::fuge::activateCoarse

void ibis::fuge::activateCoarse(uint32_t i) const {
    if (i >= bits.size()) return;       // index out of range
    if (cbits[i] != 0) return;  // already active
    std::string mesg = "fuge";
    if (ibis::gVerbose > 0) {
        mesg += '[';
        mesg += col->partition()->name();
        mesg += '.';
        mesg += col->name();
        mesg += ']';
    }
    mesg += "::activateCoarse";
    ibis::column::mutexLock lock(col, mesg.c_str());
    if (cbits[i] != 0) return;  // already active
    if ((coffset64.size() <= cbits.size() || coffset64[0] <= offset64.back()) &&
        (coffset32.size() <= cbits.size() || coffset32[0] <= offset32.back())) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << mesg << "can not regenerate bitvector "
            << i << " because there is no records of offsets";
        return;
    }
    if (coffset64.size() > cbits.size()) { // use coffset64
        if (coffset64[i+1] <= coffset64[i]) {
            return;
        }
        else if (str != 0) { // using a ibis::fileManager::storage as back store
            LOGGER(ibis::gVerbose > 8)
                << mesg << "(" << i << ") retrieving data from "
                "fileManager::storage(0x" << str << ")";

            array_t<ibis::bitvector::word_t>
                a(str, coffset64[i], coffset64[i+1]);
            cbits[i] = new ibis::bitvector(a);
            cbits[i]->sloppySize(nrows);
#if DEBUG+0 > 0 || _DEBUG+0 > 0
            LOGGER(ibis::gVerbose >= 0)
                << "fuge::activateCoarse(" << i
                << ") constructed a bitvector from range ["
                << coffset64[i] << ", " << coffset64[i+1]
                << ") of a storage at "
                << static_cast<const void*>(str->begin());
#endif
        }
        else if (fname != 0) { // using the named file directly
            int fdes = UnixOpen(fname, OPEN_READONLY);
            if (fdes >= 0) {
                LOGGER(ibis::gVerbose > 8)
                    << mesg << "(" << i << ") retrieving data from file \""
                    << fname << "\"";

#if defined(_WIN32) && defined(_MSC_VER)
                (void)_setmode(fdes, _O_BINARY);
#endif
                array_t<ibis::bitvector::word_t> a0(fdes, coffset64[i],
                                                    coffset64[i+1]);
                cbits[i] = new ibis::bitvector(a0);
                cbits[i]->sloppySize(nrows);
                UnixClose(fdes);
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                LOGGER(ibis::gVerbose >= 0)
                    << "DEBUG -- " << mesg << "(" << i
                    << ") constructed a bitvector from range ["
                    << coffset64[i] << ", " << coffset64[i+1] << ") of file "
                    << fname;
#endif
            }
            else {
                LOGGER(ibis::gVerbose > 0)
                    << "Warning -- " << mesg << "(" << i
                    << ") failed to open file \"" << fname << '"';
            }
        }
        else {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << mesg << "(" << i << ") found neither "
                "str or fname needed to regenerate the bitmap";
        }
    }
    else if (coffset32[i+1] <= coffset32[i]) {
        return;
    }
    else if (str != 0) { // using a ibis::fileManager::storage as back store
        LOGGER(ibis::gVerbose > 8)
            << mesg << "(" << i << ") retrieving data from "
            "fileManager::storage(0x" << str << ")";

        array_t<ibis::bitvector::word_t> a(str, coffset32[i], coffset32[i+1]);
        cbits[i] = new ibis::bitvector(a);
        cbits[i]->sloppySize(nrows);
#if DEBUG+0 > 0 || _DEBUG+0 > 0
        LOGGER(ibis::gVerbose >= 0)
            << "fuge::activateCoarse(" << i
            << ") constructed a bitvector from range ["
            << coffset32[i] << ", " << coffset32[i+1] << ") of a storage at "
            << static_cast<const void*>(str->begin());
#endif
    }
    else if (fname != 0) { // using the named file directly
        int fdes = UnixOpen(fname, OPEN_READONLY);
        if (fdes >= 0) {
            LOGGER(ibis::gVerbose > 8)
                << mesg << "(" << i << ") retrieving data from file \""
                << fname << "\"";

#if defined(_WIN32) && defined(_MSC_VER)
            (void)_setmode(fdes, _O_BINARY);
#endif
            array_t<ibis::bitvector::word_t> a0(fdes, coffset32[i],
                                                coffset32[i+1]);
            cbits[i] = new ibis::bitvector(a0);
            cbits[i]->sloppySize(nrows);
            UnixClose(fdes);
#if DEBUG+0 > 0 || _DEBUG+0 > 0
            LOGGER(ibis::gVerbose >= 0)
                << "DEBUG -- " << mesg << "(" << i
                << ") constructed a bitvector from range ["
                << coffset32[i] << ", " << coffset32[i+1] << ") of file "
                << fname;
#endif
        }
        else {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << mesg << "(" << i
                << ") failed to open file \"" << fname << '"';
        }
    }
    else {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << mesg << "(" << i
            << ") found neither str or fname needed to regenerate the bitmap";
    }
} // ibis::fuge::activateCoarse

void ibis::fuge::activateCoarse(uint32_t i, uint32_t j) const {
    if (j > cbits.size())
        j = cbits.size();
    if (i >= j) // empty range
        return;
    std::string mesg = "fuge";
    if (ibis::gVerbose > 0) {
        mesg += '[';
        mesg += col->partition()->name();
        mesg += '.';
        mesg += col->name();
        mesg += ']';
    }
    mesg += "::activateCoarse";
    ibis::column::mutexLock lock(col, mesg.c_str());

    while (i < j && cbits[i] != 0) ++ i;
    if (i >= j) return; // all bitvectors active

    if ((coffset64.size() <= cbits.size() || coffset64[0] <= offset64.back()) &&
        (coffset32.size() <= cbits.size() || coffset32[0] <= offset32.back())) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << mesg << '(' << i << ", " << j
            << ") can not proceed for lacking of offset information";
    }
    else if (coffset64.size() > cbits.size()) {
        if (str != 0) { // using an ibis::fileManager::storage as back store
            LOGGER(ibis::gVerbose > 8)
                << mesg << '(' << i << ", " << j
                << ") retrieving data from fileManager::storage(0x"
                << str << ')';

            while (i < j) {
                if (cbits[i] == 0 && coffset64[i+1] > coffset64[i]) {
                    array_t<ibis::bitvector::word_t>
                        a(str, coffset64[i], coffset64[i+1]);
                    cbits[i] = new ibis::bitvector(a);
                    cbits[i]->sloppySize(nrows);
#if DEBUG+0 > 1 || _DEBUG+0 > 1
                    LOGGER(ibis::gVerbose >= 5)
                        << "DEBUG -- " << mesg << " constructed bitvector "
                        << i << " from range [" << coffset64[i] << ", "
                        << coffset64[i+1] << ") of a storage at "
                        << static_cast<const void*>(str->begin());
#endif
                }
                ++ i;
            }
        }
        else if (fname != 0) { // using the named file directly
            if (coffset64[j] > coffset64[i]) {
                int fdes = UnixOpen(fname, OPEN_READONLY);
                if (fdes >= 0) {
                    LOGGER(ibis::gVerbose > 8)
                        << mesg << '(' << i << ", " << j
                        << ") retrieving data from file \"" << fname << "\"";
#if defined(_WIN32) && defined(_MSC_VER)
                    (void)_setmode(fdes, _O_BINARY);
#endif
                    while (i < j) {
                        // skip to next empty bit vector
                        while (i < j && cbits[i] != 0)
                            ++ i;
                        // the last bitvector to activate. can not be larger
                        // than j
                        uint32_t aj = (i<j ? i + 1 : j);
                        while (aj < j && cbits[aj] == 0)
                            ++ aj;
                        if (coffset64[aj] > coffset64[i]) {
                            const uint32_t start = coffset64[i];
                            ibis::fileManager::storage *a0 = new
                                ibis::fileManager::storage
                                (fdes, start, coffset64[aj]);
                            while (i < aj) {
                                if (coffset64[i+1] > coffset64[i]) {
                                    array_t<ibis::bitvector::word_t>
                                        a1(a0, coffset64[i]-start,
                                           coffset64[i+1]-start);
                                    cbits[i] = new ibis::bitvector(a1);
                                    cbits[i]->sloppySize(nrows);
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                                    LOGGER(ibis::gVerbose >= 5)
                                        << mesg << " constructed bitvector "
                                        << i << " from range [" << coffset64[i]
                                        << ", " << coffset64[i+1]
                                        << ") of file " << fname;
#endif
                                }
                                ++ i;
                            }
                        }
                        i = aj; // always advance i
                    }
                    UnixClose(fdes);
                }
                else {
                    LOGGER(ibis::gVerbose > 0)
                        << "Warning -- " << mesg << '(' << i << ", " << j
                        << ") failed to open file \"" << fname << '"';
                }
            }
        }
        else {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << mesg <<  '(' << i << ", " << j
                << ") can not proceed without str or fname";
        }
    }
    else if (str != 0) { // using an ibis::fileManager::storage as back store
        LOGGER(ibis::gVerbose > 8)
            << mesg << '(' << i << ", " << j
            << ") retrieving data from fileManager::storage(0x"
            << str << ")";

        while (i < j) {
            if (cbits[i] == 0 && coffset32[i+1] > coffset32[i]) {
                array_t<ibis::bitvector::word_t>
                    a(str, coffset32[i], coffset32[i+1]);
                cbits[i] = new ibis::bitvector(a);
                cbits[i]->sloppySize(nrows);
#if DEBUG+0 > 1 || _DEBUG+0 > 1
                LOGGER(ibis::gVerbose >= 5)
                    << "DEBUG -- " << mesg << " constructed bitvector "
                    << i << " from positions " << coffset32[i] << " - "
                    << coffset32[i+1] << " of a storage at "
                    << static_cast<const void*>(str->begin());
#endif
            }
            ++ i;
        }
    }
    else if (fname != 0) { // using the named file directly
        if (coffset32[j] > coffset32[i]) {
            int fdes = UnixOpen(fname, OPEN_READONLY);
            if (fdes >= 0) {
                LOGGER(ibis::gVerbose > 8)
                    << mesg << '(' << i << ", " << j
                    << ") retrieving data from file \"" << fname << "\"";
#if defined(_WIN32) && defined(_MSC_VER)
                (void)_setmode(fdes, _O_BINARY);
#endif
                while (i < j) {
                    // skip to next empty bit vector
                    while (i < j && cbits[i] != 0)
                        ++ i;
                    // the last bitvector to activate. can not be larger
                    // than j
                    uint32_t aj = (i<j ? i + 1 : j);
                    while (aj < j && cbits[aj] == 0)
                        ++ aj;
                    if (coffset32[aj] > coffset32[i]) {
                        const uint32_t start = coffset32[i];
                        ibis::fileManager::storage *a0 = new
                            ibis::fileManager::storage(fdes, start,
                                                       coffset32[aj]);
                        while (i < aj) {
                            if (coffset32[i+1] > coffset32[i]) {
                                array_t<ibis::bitvector::word_t>
                                    a1(a0, coffset32[i]-start,
                                       coffset32[i+1]-start);
                                cbits[i] = new ibis::bitvector(a1);
                                cbits[i]->sloppySize(nrows);
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                                LOGGER(ibis::gVerbose >= 0)
                                    << mesg << " constructed bitvector " << i
                                    << " from range [" << coffset32[i]
                                    << ", " << coffset32[i+1]
                                    << ") of file " << fname;
#endif
                            }
                            ++ i;
                        }
                    }
                    i = aj; // always advance i
                }
                UnixClose(fdes);
            }
            else {
                LOGGER(ibis::gVerbose > 0)
                    << "Warning -- " << mesg << '(' << i << ", " << j
                    << ") failed to open file \"" << fname << '"';
            }
        }
    }
    else {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << mesg <<  '(' << i << ", " << j
            << ") can not proceed without str or fname";
    }
} // ibis::fuge::activateCoarse

long ibis::fuge::coarseEstimate(uint32_t lo, uint32_t hi) const {
    long cost;
    const unsigned mid = cbounds.size() / 2;
    if (lo >= cbounds.size() || lo >= hi) {
        cost = 0;
    }
    else if (hi > mid) {
        if (coffset64.size() > cbits.size()) {
            cost = coffset64[hi-mid+1] - coffset64[hi-mid];
            if (lo > hi-mid) {
                if (lo >= mid)
                    cost += coffset64[lo-mid+1] - coffset64[lo-mid];
                else
                    cost += coffset64[lo+1] - coffset64[lo];
            }
            else if (lo < hi-mid) {
                cost += coffset64[lo+1] - coffset64[lo];
            }
        }
        else {
            cost = coffset32[hi-mid+1] - coffset32[hi-mid];
            if (lo > hi-mid) {
                if (lo >= mid)
                    cost += coffset32[lo-mid+1] - coffset32[lo-mid];
                else
                    cost += coffset32[lo+1] - coffset32[lo];
            }
            else if (lo < hi-mid) {
                cost += coffset32[lo+1] - coffset32[lo];
            }
        }
    }
    else if (hi < mid) {
        if (coffset64.size() > cbits.size())
            cost = (coffset64[lo+1] - coffset64[lo])
                + (coffset64[hi+1] - coffset64[hi]);
        else
            cost = (coffset32[lo+1] - coffset32[lo])
                + (coffset32[hi+1] - coffset32[hi]);
    }
    else if (coffset64.size() > cbits.size()) { // hi == mid
        cost = coffset64[1] - coffset64[0];
        if (lo > 0) {
            cost += coffset64[lo+1] - coffset64[lo];
        }
    }
    else { // hi == mid
        cost = coffset32[1] - coffset32[0];
        if (lo > 0) {
            cost += coffset32[lo+1] - coffset32[lo];
        }
    }
    return cost;
} // ibis::fuge::coarseEstimate

long ibis::fuge::coarseEvaluate(uint32_t lo, uint32_t hi,
                                ibis::bitvector &res) const {
    const unsigned mid = cbounds.size() / 2;
    if (lo >= cbounds.size() || lo >= hi) {
        res.set(0, nrows);
    }
    else if (lo+1 == hi) { // two consecutive bitmaps used
        if (hi < cbits.size()) {
            activateCoarse(lo, hi+1);
            if (cbits[lo] != 0) {
                res.copy(*(cbits[lo]));
                if (cbits[hi] != 0)
                    res -= *(cbits[hi]);
            }
            else {
                res.set(0, nrows);
            }
        }
        else {
            activateCoarse(lo-mid, lo-mid+2);
            if (cbits[hi-mid] != 0) {
                res.copy(*(cbits[hi-mid]));
                if (cbits[lo-mid] != 0)
                    res -= *(cbits[lo-mid]);
            }
            else {
                res.set(0, nrows);
            }
        }
    }
    else if (hi > mid) {
        if (cbits[hi-mid] == 0)
            activateCoarse(hi-mid);
        if (cbits[hi-mid] != 0)
            res.copy(*(cbits[hi-mid]));
        else
            res.set(0, nrows);
        if (lo > hi-mid) {
            if (lo >= mid) {
                if (cbits[lo-mid] == 0)
                    activateCoarse(lo-mid);
                if (cbits[lo-mid] != 0)
                    res -= *(cbits[lo-mid]);
            }
            else {
                if (cbits[lo] == 0)
                    activateCoarse(lo);
                if (cbits[lo] != 0)
                    res &= *(cbits[lo]);
                else
                    res.set(0, nrows);
            }
        }
        else if (lo < hi-mid) {
            if (cbits[lo] == 0)
                activateCoarse(lo);
            if (cbits[lo] != 0)
                res |= *(cbits[lo]);
        }
    }
    else if (hi < mid) {
        if (cbits[lo] == 0)
            activateCoarse(lo);
        if (cbits[lo] != 0) {
            res.copy(*(cbits[lo]));
            if (cbits[hi] == 0)
                activateCoarse(hi);
            if (cbits[hi] != 0)
                res -= *(cbits[hi]);
        }
        else {
            res.set(0, nrows);
        }
    }
    else { // hi == mid
        if (cbits[0] == 0)
            activateCoarse(0);
        if (cbits[0] != 0)
            res.copy(*(cbits[0]));
        else
            res.set(0, nrows);
        if (lo > 0) {
            if (cbits[lo] == 0)
                activateCoarse(lo);
            if (cbits[lo] != 0)
                res &= *(cbits[lo]);
        }
    }
    return res.size();
} // ibis::fuge::coarseEvaluate

/// Estimate the size of the serialized version of the index.  Return the
/// size in bytes.
size_t ibis::fuge::getSerialSize() const throw () {
    size_t res = (nobs << 5) + 24 + 4*bounds.size() + 8*cbits.size();
    for (unsigned j = 0; j < nobs; ++ j)
        if (bits[j] != 0)
            res += bits[j]->getSerialSize();
    for (unsigned j = 0; j < cbits.size(); ++ j)
        if (cbits[j] != 0)
            res += cbits[j]->getSerialSize();
    return res;
} // ibis::fuge::getSerialSize
