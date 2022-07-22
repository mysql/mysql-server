// $Id$
// Author: John Wu <John.Wu at ACM.org>
// Copyright (c) 2006-2016 the Regents of the University of California
//
// This file contains the implementation of the class ibis::fuzz, a
// unbinned version of interval-equality encoded index.
//
// In fuzzy clustering/classification, there are extensive use of interval
// equality condition, hence the funny name.
//
#if defined(_WIN32) && defined(_MSC_VER)
#pragma warning(disable:4786)   // some identifiers longer than 256 characters
#endif
#include "irelic.h"
#include "part.h"

#include <sstream>      // std::ostringstream
#define FASTBIT_SYNC_WRITE 1

////////////////////////////////////////////////////////////////////////
ibis::fuzz::fuzz(const ibis::column *c, const char *f)
    : ibis::relic(c, f) {
    if (c == 0) return; // nothing to do
    if (cbits.empty() || cbits.size()+1 != cbounds.size()) {
        if (fname != 0)
            readCoarse(f);
        else
            coarsen();
    }
    if (ibis::gVerbose > 2) {
        const size_t nobs = bits.size();
        const size_t nc = cbounds.size();
        ibis::util::logger lg;
        lg() << "fuzz[" << col->partition()->name() << '.' << col->name()
             << "]::ctor -- initialized an interval-equality index with "
             << nobs << " fine bin" << (nobs>1?"s":"") << " and " << nc
             << " coarse bin" << (nc>1?"s":"") << " for "
             << nrows << " row" << (nrows>1?"s":"")
             << " from file " << (fname ? fname : f ? f : c->name());
        if (ibis::gVerbose > 6) {
            lg() << "\n";
            print(lg());
        }
    }
} // ibis::fuzz::fuzz

/// Reconstruct an index from the content of a storage object.
/**
   The leading portion of the index file is the same as ibis::relic, which
   allows the constructor of the base class to work properly.  The content
   following the last bitvector in ibis::relic is as follows, @sa
   ibis::fuzz::writeCoarse.
@code
   nc      (uint32_t)                   -- number of coarse bins.
   cbounds (uint32_t[nc+1])             -- boundaries of the coarse bins.
   coffsets([nc-ceil(nc/2)+2])          -- starting positions.
   cbits   (bitvector[nc-ceil(nc/2)+1]) -- bitvectors.
@endcode
 */
ibis::fuzz::fuzz(const ibis::column* c, ibis::fileManager::storage* st,
                 size_t start) : ibis::relic(c, st, start) {
    if (offset64.size() > bits.size())
        start = offset64.back();
    else if (offset32.size() > bits.size())
        start = offset32.back();
    else {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fuzz[" << col->partition()->name() << '.'
            << col->name() << "]::ctor can not proceed further without "
            "bitmap size information";
        clear();
        return;
    }
    if (st->size() <= start+12) return;

    uint32_t nc = *(reinterpret_cast<uint32_t*>(st->begin()+start));
    if (nc == 0 ||
        st->size() <= start + (sizeof(int32_t)+sizeof(uint32_t))*(nc+1))
        return;

    size_t end;
    const uint32_t ncb = nc - (nc+1)/2 + 1;
    start += sizeof(uint32_t);
    end = start + sizeof(uint32_t) * (nc+1);
    if (start+sizeof(uint32_t)*(nc+1) < st->size()) {
        array_t<uint32_t> tmp(st, start, end);
        cbounds.swap(tmp);
    }
    start = end;
    if (offset64.size() > bits.size()) {
        end += sizeof(int64_t) * (ncb+1);
        if (end < st->size()) {
            array_t<int64_t> tmp(st, start, end);
            coffset64.swap(tmp);
            if (coffset64.back() > static_cast<int64_t>(st->size())) {
                coffset64.swap(tmp);
                array_t<uint32_t> tmp2;
                cbounds.swap(tmp2);
                return;
            }
        }
        else {
            array_t<uint32_t> tmp2;
            cbounds.swap(tmp2);
            return;
        }
        coffset32.clear();
    }
    else {
        end += sizeof(int32_t)*(ncb+1);
        if (end < st->size()) {
            array_t<int32_t> tmp(st, start, end);
            coffset32.swap(tmp);
            if (coffset32.back() > static_cast<int32_t>(st->size())) {
                coffset32.swap(tmp);
                array_t<uint32_t> tmp2;
                cbounds.swap(tmp2);
                return;
            }
        }
        else {
            array_t<uint32_t> tmp2;
            cbounds.swap(tmp2);
            return;
        }
        coffset64.clear();
    }

    cbits.resize(ncb);
    for (unsigned i = 0; i < ncb; ++ i)
        cbits[i] = 0;

    if (ibis::gVerbose > 2) {
        const size_t nobs = bits.size();
        ibis::util::logger lg;
        lg() << "fuzz[" << col->partition()->name() << '.' << col->name()
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
} // ibis::fuzz::fuzz

long ibis::fuzz::append(const char* dt, const char* df, uint32_t nnew) {
    long ret = ibis::relic::append(dt, df, nnew);
    if (ret <= 0 || static_cast<uint32_t>(ret) != nnew)
        return ret;

    if (nrows == col->partition()->nRows())
        coarsen();
    return ret;
} // ibis::fuzz::append

/// Generate coarse bins from the fine-level bitmaps.  It fills the array
/// offset64, and divides the bitmaps into groups according to their sizes
/// (bytes).
void ibis::fuzz::coarsen() {
    if (vals.size() < 32) return; // don't construct the coarse level
    if (cbits.size() > 0 && (cbits.size()+1 == coffset32.size() ||
                             cbits.size()+1 == coffset64.size())) return;

    const uint32_t nbits = bits.size();
    if (offset64.size() != nbits+1) {
        if (offset32.size() == nbits+1) {
            offset64.resize(nbits+1);
            for (unsigned j = 0; j <= nbits; ++ j)
                offset64[j] = offset32[j];
        }
        else {
            offset64.resize(nbits+1);
            offset64[0] = 0;
            for (unsigned i = 0; i < nbits; ++ i)
                offset64[i+1] = offset64[i] + (bits[i] ? bits[i]->bytes() : 0U);
        }
    }

    unsigned ncoarse = 0;
    if (col != 0) { // user specified value
        const char* spec = col->indexSpec();
        if (spec != 0 && *spec != 0 && strstr(spec, "ncoarse=") != 0) {
            // number of coarse bins specified explicitly
            const char* tmp = 8+strstr(spec, "ncoarse=");
            unsigned j = strtol(tmp, 0, 0);
            if (j > 4)
                ncoarse = j;
        }
    }
    // default size based on the size of fine level index sf: sf(w-1)/N/sqrt(2)
    if (ncoarse < 5U && offset64.back() > offset64[0]+nrows/31U) {
        ncoarse = sizeof(ibis::bitvector::word_t);
        const int wm1 = ncoarse*8-1;
        const long sf = (offset64.back()-offset64[0]) / ncoarse;
        ncoarse = static_cast<unsigned>(wm1*sf/(sqrt(2.0)*nrows));
        const unsigned ncmax = (unsigned) sqrt(2.0 * vals.size());
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
    if (ncoarse < 5 || ncoarse >= vals.size()) return;

    const uint32_t nc2 = (ncoarse + 1) / 2;
    const uint32_t ncb = ncoarse - nc2 + 1; // # of coarse level bitmaps
    // partition the fine level bitmaps into groups with nearly equal
    // number of bytes
    cbounds.resize(ncoarse+1);
    cbounds[0] = 0;
    for (unsigned i = 1; i < ncoarse; ++ i) {
        int32_t target = offset64[cbounds[i-1]] +
            (offset64.back() - offset64[cbounds[i-1]]) / (ncoarse - i + 1);
        cbounds[i] = offset64.find(target);
        if (cbounds[i] > cbounds[i-1]+1 &&
            offset64[cbounds[i]]-target > target-offset64[cbounds[i]-1])
            -- (cbounds[i]);
        else if (cbounds[i] <= cbounds[i-1])
            cbounds[i] = cbounds[i-1]+1;
    }
    cbounds[ncoarse] = nbits; // end with the last fine level bitmap
    for (unsigned i = ncoarse-1; i > 0 && cbounds[i+1] < cbounds[i]; -- i)
        cbounds[i] = cbounds[i+1] - 1;
    if (ibis::gVerbose > 2) {
        ibis::util::logger lg;
        lg() << "fuzz[" << col->partition()->name() << '.'
             << col->name() << "]::coarsen will divide " << bits.size()
             << " bitmaps into " << ncoarse << " groups\n";
        for (unsigned i = 0; i < cbounds.size(); ++ i)
            lg() << cbounds[i] << " ";
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
} // ibis::fuzz::coarsen

void ibis::fuzz::activateCoarse() const {
    std::string evt = "fuzz";
    if (ibis::gVerbose > 0) {
        evt += '[';
        evt += col->partition()->name();
        evt += '.';
        evt += col->name();
        evt += ']';
    }
    evt += "::activateCoarse";
    const uint32_t nobs = cbits.size();
    bool missing = false; // any bits[i] missing (is 0)?
    ibis::column::mutexLock lock(col, evt.c_str());
    for (uint32_t i = 0; i < nobs && ! missing; ++ i)
        missing = (cbits[i] == 0);
    if (missing == false) return;

    if (coffset32.size() <= nobs && coffset64.size() <= nobs) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt
            << " can not proceed without coffset32 or coffset64";
    }
    else if (str) { // using a ibis::fileManager::storage as back store
        LOGGER(ibis::gVerbose > 8)
            << evt << " retrieving data from fileManager::storage(0x"
            << str << ")";

        if (coffset64.size() > nobs) {
            for (uint32_t i = 0; i < nobs; ++i) {
                if (cbits[i] == 0 && coffset64[i+1] > coffset64[i]) {
                    array_t<ibis::bitvector::word_t>
                        a(str, coffset64[i], coffset64[i+1]);
                    cbits[i] = new ibis::bitvector(a);
                    cbits[i]->sloppySize(nrows);
                }
            }
        }
        else {
            for (uint32_t i = 0; i < nobs; ++i) {
                if (cbits[i] == 0 && coffset32[i+1] > coffset32[i]) {
                    array_t<ibis::bitvector::word_t>
                        a(str, coffset32[i], coffset32[i+1]);
                    cbits[i] = new ibis::bitvector(a);
                    cbits[i]->sloppySize(nrows);
                }
            }
        }
    }
    else if (fname) { // using the named file directly
        int fdes = UnixOpen(fname, OPEN_READONLY);
        if (fdes < 0) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << evt << " failed to open file \"" << fname
                << "\" ... " << (errno ? strerror(errno) : "??");
            errno = 0;
            return;
        }

        LOGGER(ibis::gVerbose > 8)
            << evt << " retrieving data from file \"" << fname << "\"";
        IBIS_BLOCK_GUARD(UnixClose, fdes);
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
            if (coffset64.size() > nobs && coffset64[aj] > coffset64[i]) {
                const size_t start = coffset64[i];
                ibis::fileManager::storage *a0 = new
                    ibis::fileManager::storage(fdes, start,
                                               coffset64[aj]);
                while (i < aj) {
                    if (coffset64[i+1] > coffset64[i]) {
                        array_t<ibis::bitvector::word_t>
                            a1(a0, coffset64[i]-start, coffset64[i+1]-start);
                        bits[i] = new ibis::bitvector(a1);
                        bits[i]->sloppySize(nrows);
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                        LOGGER(ibis::gVerbose >= 0)
                            << "DEBUG -- " << evt << " activating bitvector "
                            << i << "by reading file " << fname
                            << "coffset64[" << i << "]= " << coffset64[i]
                            << ", coffset64[" << i+1 << "]= "
                            << coffset64[i+1];
#endif
                    }
                    ++ i;
                }
            }
            else if (coffset32.size() > nobs && coffset32[aj] > coffset32[i]) {
                const uint32_t start = coffset32[i];
                ibis::fileManager::storage *a0 = new
                    ibis::fileManager::storage(fdes, start,
                                               coffset32[aj]);
                while (i < aj) {
                    if (coffset32[i+1] > coffset32[i]) {
                        array_t<ibis::bitvector::word_t>
                            a1(a0, coffset32[i]-start, coffset32[i+1]-start);
                        bits[i] = new ibis::bitvector(a1);
                        bits[i]->sloppySize(nrows);
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                        LOGGER(ibis::gVerbose >= 0)
                            << "DEBUG -- " << evt << " activating bitvector "
                            << i << "by reading file " << fname
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
    }
    else {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " can not regenerate "
            "bitvectors without str or fname";
    }
} // ibis::fuzz::activateCoarse

void ibis::fuzz::activateCoarse(uint32_t i) const {
    if (i >= bits.size()) return;       // index out of range
    std::string evt = "fuzz";
    if (ibis::gVerbose > 0) {
        evt += '[';
        evt += col->partition()->name();
        evt += '.';
        evt += col->name();
        evt += ']';
    }
    evt += "::activateCoarse";
    ibis::column::mutexLock lock(col, evt.c_str());
    if (cbits[i] != 0) return;  // already active
    if (coffset32.size() <= cbits.size() && coffset64.size() <= cbits.size()) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " can not regenerate bitvector " << i
            << " without coffset32 or coffet64";
        return;
    }
    else if ((coffset64.size() > cbits.size() &&
              coffset64[i+1] <= coffset64[i]) &&
             (coffset32.size() > cbits.size() &&
              coffset32[i+1] <= coffset32[i])) {
        return;
    }
    if (str) { // using a ibis::fileManager::storage as back store
        LOGGER(ibis::gVerbose > 8)
            << evt << " retrieving bitvector " << i
            << " from fileManager::storage(0x" << str << ")";

        if (coffset64.size() > cbits.size()) {
            array_t<ibis::bitvector::word_t>
                a(str, coffset64[i], coffset64[i+1]);
            cbits[i] = new ibis::bitvector(a);
            cbits[i]->sloppySize(nrows);
        }
        else {
            array_t<ibis::bitvector::word_t>
                a(str, coffset32[i], coffset32[i+1]);
            cbits[i] = new ibis::bitvector(a);
            cbits[i]->sloppySize(nrows);
        }
    }
    else if (fname) { // using the named file directly
        int fdes = UnixOpen(fname, OPEN_READONLY);
        if (fdes < 0) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << evt << "failed to open file \""
                << fname << "\" ... " << (errno ? strerror(errno) : "??");
            errno = 0;
            return;
        }

        LOGGER(ibis::gVerbose > 8)
            << evt << " retrieving bitvector " << i
            << " from file \"" << fname << "\"";
        IBIS_BLOCK_GUARD(UnixClose, fdes);
#if defined(_WIN32) && defined(_MSC_VER)
        (void)_setmode(fdes, _O_BINARY);
#endif
        if (coffset64.size() > cbits.size()) {
            array_t<ibis::bitvector::word_t>
                a0(fdes, coffset64[i], coffset64[i+1]);
            cbits[i] = new ibis::bitvector(a0);
#if DEBUG+0 > 0 || _DEBUG+0 > 0
            LOGGER(ibis::gVerbose >= 0)
                << evt << " constructed bitvector " << i
                << " from range [" << coffset64[i]
                << ", "  << coffset64[i+1] << ") of file " << fname;
#endif
        }
        else {
            array_t<ibis::bitvector::word_t>
                a0(fdes, coffset32[i], coffset32[i+1]);
            cbits[i] = new ibis::bitvector(a0);
#if DEBUG+0 > 0 || _DEBUG+0 > 0
            LOGGER(ibis::gVerbose >= 0)
                << evt << " constructed bitvector " << i
                << " from range [" << coffset32[i]
                << ", "  << coffset32[i+1] << ") of file " << fname;
#endif
        }
        cbits[i]->sloppySize(nrows);
    }
    else {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " can not regenerate bitvector "
            << i << " without str or fname";
    }
} // ibis::fuzz::activateCoarse

void ibis::fuzz::activateCoarse(uint32_t i, uint32_t j) const {
    if (j > cbits.size())
        j = cbits.size();
    if (i >= j) // empty range
        return;
    std::string evt = "fuzz";
    if (ibis::gVerbose > 0) {
        evt += '[';
        evt += col->partition()->name();
        evt += '.';
        evt += col->name();
        evt += ']';
    }
    evt += "::activateCoarse";
    ibis::column::mutexLock lock(col, evt.c_str());

    while (i < j && cbits[i] != 0) ++ i;
    if (i >= j) return; // requested bitvectors active

    if (coffset32.size() <= cbits.size() && coffset64.size() <= cbits.size()) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt
            << " can not regenerate coarse-level bitvectors " << i
            << " -- " << j << " without coffset32 or coffset64";
    }
    else if (str) { // using an ibis::fileManager::storage as back store
        LOGGER(ibis::gVerbose > 8)
            << evt << "(" << i << ", " << j
            << ") retrieving data from fileManager::storage(0x"
            << str << ")";

        if (coffset64.size() > cbits.size()) {
            while (i < j) {
                if (cbits[i] == 0 && coffset64[i+1] > coffset64[i]) {
                    array_t<ibis::bitvector::word_t>
                        a(str, coffset64[i], coffset64[i+1]);
                    cbits[i] = new ibis::bitvector(a);
                    cbits[i]->sloppySize(nrows);
                }
                ++ i;
            }
        }
        else {
            while (i < j) {
                if (cbits[i] == 0 && coffset32[i+1] > coffset32[i]) {
                    array_t<ibis::bitvector::word_t>
                        a(str, coffset32[i], coffset32[i+1]);
                    cbits[i] = new ibis::bitvector(a);
                    cbits[i]->sloppySize(nrows);
                }
                ++ i;
            }
        }
    }
    else if (fname) { // using the named file directly
        int fdes = UnixOpen(fname, OPEN_READONLY);
        if (fdes < 0) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << evt << "failed to open file \""
                << fname << "\" ... " << (errno ? strerror(errno) : "??");
            errno = 0;
            return;
        }

        LOGGER(ibis::gVerbose > 8)
            << evt << "(" << i << ", " << j
            << ") retrieving data from file \"" << fname << "\"";
        IBIS_BLOCK_GUARD(UnixClose, fdes);
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
            if (coffset64.size() > cbits.size() &&
                coffset64[aj] > coffset64[i]) {
                const uint64_t start = coffset64[i];
                ibis::fileManager::storage *a0 = new
                    ibis::fileManager::storage(fdes, start,
                                               coffset64[aj]);
                while (i < aj) {
                    if (coffset64[i+1] > coffset64[i]) {
                        array_t<ibis::bitvector::word_t>
                            a1(a0, coffset64[i]-start, coffset64[i+1]-start);
                        cbits[i] = new ibis::bitvector(a1);
                        cbits[i]->sloppySize(nrows);
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                        LOGGER(ibis::gVerbose >= 0)
                            << evt << " constructed bitvector " << i
                            << " from range [" << coffset64[i] << ", "
                            << coffset64[i+1] << ") of file " << fname;
#endif
                    }
                    ++ i;
                }
            }
            else if (coffset32.size() > cbits.size() &&
                     coffset32[aj] > coffset32[i]) {
                const uint32_t start = coffset32[i];
                ibis::fileManager::storage *a0 = new
                    ibis::fileManager::storage(fdes, start,
                                               coffset32[aj]);
                while (i < aj) {
                    if (coffset32[i+1] > coffset32[i]) {
                        array_t<ibis::bitvector::word_t>
                            a1(a0, coffset32[i]-start, coffset32[i+1]-start);
                        cbits[i] = new ibis::bitvector(a1);
                        cbits[i]->sloppySize(nrows);
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                        LOGGER(ibis::gVerbose >= 0)
                            << evt << " constructed bitvector " << i
                            << " from range [" << coffset32[i] << ", "
                            << coffset32[i+1] << ") of file " << fname;
#endif
                    }
                    ++ i;
                }
            }
            i = aj; // always advance i
        }
    }
    else {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " can not regenerate bitvectors "
            << i << ", " << j << " without str or fname";
    }
} // ibis::fuzz::activateCoarse

uint32_t ibis::fuzz::estimate(const ibis::qContinuousRange& expr) const {
    ibis::bitvector bv;
    long ierr = evaluate(expr, bv);
    return static_cast<uint32_t>(ierr > 0 ? ierr : 0);
} // ibis::fuzz::estimate

long ibis::fuzz::coarseEstimate(uint32_t lo, uint32_t hi) const {
    long cost;
    const uint32_t mid = cbounds.size() / 2;
    if (lo >= cbounds.size() || lo >= hi) {
        cost = 0;
    }
    else if (hi > mid) {
        cost = (coffset64.size() > cbits.size()
                ? coffset64[hi-mid+1] - coffset64[hi-mid]
                : coffset32[hi-mid+1] - coffset32[hi-mid]);
        if (lo > hi-mid) {
            if (lo >= mid)
                cost += (coffset64.size() > cbits.size()
                         ? coffset64[lo-mid+1] - coffset64[lo-mid]
                         : coffset32[lo-mid+1] - coffset32[lo-mid]);
            else
                cost += (coffset64.size() > cbits.size()
                         ? coffset64[lo+1] - coffset64[lo]
                         : coffset32[lo+1] - coffset32[lo]);
        }
        else if (lo < hi-mid) {
            cost += (coffset64.size() > cbits.size()
                     ? coffset64[lo+1] - coffset64[lo]
                     : coffset32[lo+1] - coffset32[lo]);
        }
    }
    else if (hi < mid) {
        cost = (coffset64.size() > cbits.size()
                ? ((coffset64[lo+1] - coffset64[lo])
                   + (coffset64[hi+1] - coffset64[hi]))
                : ((coffset32[lo+1] - coffset32[lo])
                   + (coffset32[hi+1] - coffset32[hi])));
    }
    else { // hi == mid
        if (coffset64.size() > cbits.size()) {
            cost = coffset64[1] - coffset64[0];
            if (lo > 0) {
                cost += coffset64[lo+1] - coffset64[lo];
            }
        }
        else {
            cost = coffset32[1] - coffset32[0];
            if (lo > 0) {
                cost += coffset32[lo+1] - coffset32[lo];
            }
        }
    }
    return cost;
} // ibis::fuzz::coarseEstimate

long ibis::fuzz::coarseEvaluate(uint32_t lo, uint32_t hi,
                                ibis::bitvector &res) const {
    const uint32_t mid = cbounds.size() / 2;
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
} // ibis::fuzz::coarseEvaluate

double ibis::fuzz::estimateCost(const ibis::qContinuousRange& expr) const {
    double res = static_cast<double>(col->elementSize() * nrows);
    if (bits.empty() || (offset64.empty() && offset32.empty())) {
        return res;
    }

    // values in the range [hit0, hit1) satisfy the query
    uint32_t hit0, hit1;
    locate(expr, hit0, hit1);
    if (hit1 <= hit0 || hit0 >= bits.size()) {
        res = 0.0;
        return res;
    }
    if (hit0 == 0 && hit1 >= bits.size()) {
        res = 0.0;
        return res;
    }

    const uint32_t ncoarse = (cbounds.empty() ? 0U : cbounds.size()-1);
    const long fine = (offset64.size() > bits.size()
                       ? (offset64[hit1] - offset64[hit0] <=
                          ((offset64.back() - offset64[hit1])
                           + (offset64[hit0] - offset64[0]))
                          ? offset64[hit1] - offset64[hit0]
                          : ((offset64.back() - offset64[hit1])
                             + (offset64[hit0] - offset64[0])))
                       : (offset32[hit1] - offset32[hit0] <=
                          ((offset32.back() - offset32[hit1])
                           + (offset32[hit0] - offset32[0]))
                          ? offset32[hit1] - offset32[hit0]
                          : ((offset32.back() - offset32[hit1])
                             + (offset32[hit0] - offset32[0]))));
    if (hit0+3 >= hit1 || ncoarse == 0 ||
        (cbits.size()+1 != coffset32.size()
         && cbits.size()+1 != coffset64.size())) {
        res = fine;
        return res;
    }

    // see whether the coarse bins could help
    const uint32_t c0 = cbounds.find(hit0);
    const uint32_t c1 = cbounds.find(hit1);
    if (c0 >= c1) { // within the same coarse bin
        // complement
        long tmp = coarseEstimate(c1-1, c1)
            + (offset64.size() > bits.size()
               ? ((offset64[hit0] - offset64[cbounds[c1-1]])
                  + (offset64[cbounds[c1]] - offset64[hit1]))
               : ((offset32[hit0] - offset32[cbounds[c1-1]])
                  + (offset32[cbounds[c1]] - offset32[hit1])));
        if (tmp/100 >= fine/99)
            res = fine;
        else
            res = tmp;
    }
    else { // general case: need to evaluate 5 options
        // option 2: [direct | - | direct]
        long cost = coarseEstimate(c0, c1-1)
            + (offset64.size() > bits.size()
               ? ((offset64[cbounds[c0]] - offset64[hit0])
                  + (offset64[hit1] - offset64[cbounds[c1-1]]))
               : ((offset32[cbounds[c0]] - offset32[hit0])
                  + (offset32[hit1] - offset32[cbounds[c1-1]])));
        // option 3: [complement | - | direct]
        long tmp;
        if (c0 > 0) {
            tmp = coarseEstimate(c0-1, c1-1)
                + (offset64.size() > bits.size()
                   ? ((offset64[hit0] - offset64[cbounds[c0-1]])
                      + (offset64[hit1] - offset64[cbounds[c1-1]]))
                   : ((offset32[hit0] - offset32[cbounds[c0-1]])
                      + (offset32[hit1] - offset32[cbounds[c1-1]])));
            if (tmp < cost)
                cost = tmp;
        }
        // option 4: [direct | - | complement]
        tmp = coarseEstimate(c0, c1)
            + (offset64.size() > bits.size()
               ? ((offset64[cbounds[c0]] - offset64[hit0])
                  + (offset64[cbounds[c1]] - offset64[hit1]))
               : ((offset32[cbounds[c0]] - offset32[hit0])
                  + (offset32[cbounds[c1]] - offset32[hit1])));
        if (tmp < cost)
            cost = tmp;
        // option 5: [complement | - | complement]
        if (c0 > 0) {
            tmp = coarseEstimate(c0-1, c1)
                + (offset64.size() > bits.size()
                   ? ((offset64[hit0] - offset64[cbounds[c0-1]])
                      + (offset64[cbounds[c1]] - offset64[hit1]))
                   : ((offset32[hit0] - offset32[cbounds[c0-1]])
                      + (offset32[cbounds[c1]] - offset32[hit1])));
            if (tmp < cost)
                cost = tmp;
        }
        // option 1: fine level only
        if (cost/100 >= fine/99) // slightly prefer 1
            res = fine;
        else
            res = cost;
    }
    return res;
} // ibis::fuzz::estimateCost

/// Compute the hits as a @c bitvector.
long ibis::fuzz::evaluate(const ibis::qContinuousRange& expr,
                          ibis::bitvector& lower) const {
    if (bits.empty()) { // empty index
        lower.set(0, nrows);
        return 0L;
    }

    // values in the range [hit0, hit1) satisfy the query
    uint32_t hit0, hit1;
    locate(expr, hit0, hit1);
    if (hit1 <= hit0 || hit0 >= bits.size()) {
        lower.set(0, nrows);
        return 0L;
    }
    if (hit0 == 0 && hit1 >= bits.size()) {
        col->getNullMask(lower);
        return lower.cnt();
    }

    if (hit0+1 == hit1) { // equality condition
        if (bits[hit0] == 0)
            activate(hit0);
        if (bits[hit0] != 0)
            lower.copy(*(bits[hit0]));
        else
            lower.set(0, nrows);
        return lower.cnt();
    }
    const uint32_t ncoarse = (cbounds.empty() ? 0U : cbounds.size()-1);
    if (hit0+3 >= hit1 || ncoarse == 0 ||
        ((cbits.size()+1) != coffset32.size() &&
         (cbits.size()+1) != coffset64.size())) {
        // no more than three bitmaps involved, or don't know the sizes
        sumBins(hit0, hit1, lower);
        return lower.cnt();
    }

    const long finec = (offset64.size() > bits.size()
                        ? (offset64[hit1] - offset64[hit0] <=
                           ((offset64.back() - offset64[hit1])
                            + (offset64[hit0] - offset64[0]))
                           ? offset64[hit1] - offset64[hit0]
                           : ((offset64.back() - offset64[hit1])
                              + (offset64[hit0] - offset64[0])))
                        : (offset32[hit1] - offset32[hit0] <=
                           ((offset32.back() - offset32[hit1])
                            + (offset32[hit0] - offset32[0]))
                           ? offset32[hit1] - offset32[hit0]
                           : ((offset32.back() - offset32[hit1])
                              + (offset32[hit0] - offset32[0]))));
    // see whether the coarse bins could help
    const uint32_t c0 = cbounds.find(hit0);
    const uint32_t c1 = cbounds.find(hit1);
    if (ibis::gVerbose > 4) {
        ibis::util::logger lg;
        lg() << "fuzz[" << col->partition()->name() << '.'
             << col->name() << "]::evaluate(" << expr << ") hit0=" << hit0
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
        long tmp = coarseEstimate(c1-1, c1)
            + (offset64.size() > bits.size()
               ? ((offset64[hit0] - offset64[cbounds[c1-1]])
                  + (offset64[cbounds[c1]] - offset64[hit1]))
               : ((offset32[hit0] - offset32[cbounds[c1-1]])
                  + (offset32[cbounds[c1]] - offset32[hit1])));
        if (finec <= static_cast<long>(0.99*tmp)) {
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
        long cost = coarseEstimate(c0, c1-1)
            + (offset64.size() > bits.size()
               ? ((offset64[cbounds[c0]] - offset64[hit0])
                  + (offset64[hit1] - offset64[cbounds[c1-1]]))
               : ((offset32[cbounds[c0]] - offset32[hit0])
                  + (offset32[hit1] - offset32[cbounds[c1-1]])));
        long tmp;
        if (c0 > 0) {   // option 3: [complement | - | direct]
            tmp = coarseEstimate(c0-1, c1-1)
                + (offset64.size() > bits.size()
                   ? ((offset64[hit0] - offset64[cbounds[c0-1]])
                      + (offset64[hit1] - offset64[cbounds[c1-1]]))
                   : ((offset32[hit0] - offset32[cbounds[c0-1]])
                      + (offset32[hit1] - offset32[cbounds[c1-1]])));
            if (tmp < cost) {
                cost = tmp;
                option = 3;
            }
        }
        // option 4: [direct | - | complement]
        tmp = coarseEstimate((c0>0 ? c0-1 : 0), c1)
            + (offset64.size() > bits.size()
               ? ((offset64[cbounds[c0]] - offset64[hit0])
                  + (offset64[cbounds[c1]] - offset64[hit1]))
               : ((offset32[cbounds[c0]] - offset32[hit0])
                  + (offset32[cbounds[c1]] - offset32[hit1])));
        if (tmp < cost) {
            cost = tmp;
            option = 4;
        }
        if (c0 > 0) { // option 5: [complement | - | complement]
            tmp = coarseEstimate(c0-1, c1)
                + (offset64.size() > bits.size()
                   ? ( (offset64[hit0] - offset64[cbounds[c0-1]])
                       + (offset64[cbounds[c1]] - offset64[hit1]))
                   : ( (offset32[hit0] - offset32[cbounds[c0-1]])
                       + (offset32[cbounds[c1]] - offset32[hit1])));
            if (tmp < cost) {
                cost = tmp;
                option = 5;
            }
        }
        // option 0 and 1: fine level only
        tmp = finec;
        if (cost > static_cast<long>(0.99*tmp)) { // slightly prefer 0/1
            cost = tmp;
            option = 1;
        }
        switch (option) {
        default:
        case 1: // use fine level only
            LOGGER(ibis::gVerbose > 7)
                << "fuzz[" << col->partition()->name() << '.'
                << col->name() << "]::evaluate(" << expr
                << ") using only fine level bit vectors [" << hit0
                << ", " << hit1 << ")";
            sumBins(hit0, hit1, lower);
            break;
        case 2: // direct | - | direct
            LOGGER(ibis::gVerbose > 7)
                << "fuzz[" << col->partition()->name() << '.'
                << col->name() << "]::evaluate(" << expr
                << ") using coarse bit vectors [" << c0 << ", " << c1-1
                << ") plus fine bit vectors [" << hit0 << ", "
                << cbounds[c0] << ") plus [" << cbounds[c1-1] << ", "
                << hit1 << ")";
            coarseEvaluate(c0, c1-1, lower);
            if (hit0 < cbounds[c0])
                addBins(hit0, cbounds[c0], lower); // left edge bin
            if (cbounds[c1-1] < hit1)
                addBins(cbounds[c1-1], hit1, lower); // right edge bin
            break;
        case 3: // complement | - | direct
            LOGGER(ibis::gVerbose > 7)
                << "fuzz[" << col->partition()->name() << '.'
                << col->name() << "]::evaluate(" << expr
                << ") using coarse bit vectors [" << c0-1 << ", " << c1-1
                << ") minus fine bit vectors [" << cbounds[c0-1] << ", "
                << hit0 << ") plus [" << cbounds[c1-1] << ", "
                << hit1 << ")";
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
            LOGGER(ibis::gVerbose > 7)
                << "fuzz[" << col->partition()->name() << '.'
                << col->name() << "]::evaluate(" << expr
                << ") using coarse bit vectors [" << c0 << ", " << c1
                << ") plus fine bit vectors [" << hit0 << ", "
                << cbounds[c0] << ") minus [" << hit1 << ", "
                << cbounds[c1] << ")";
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
            LOGGER(ibis::gVerbose > 7)
                << "fuzz[" << col->partition()->name() << '.'
                << col->name() << "]::evaluate(" << expr
                << ") using coarse bit vectors [" << c0-1 << ", " << c1
                << ") minus fine bit vectors [" << cbounds[c0-1] << ", "
                << hit0 << ") minus [" << hit1 << ", "
                << cbounds[c1] << ")";
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
    return lower.cnt();
} // ibis::fuzz::evaluate

/// Write the content of the index to the specified location.  The incoming
/// argument can be the name of a directory or a file.  The actual index
/// file name is determined by the function indexFileName.
int ibis::fuzz::write(const char* dt) const {
    if (vals.empty()) return -1;

    std::string fnm, evt;
    evt = "fuzz";
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
        activate(); // activate all bitvectors

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

    int32_t ierr = 0;
#ifdef FASTBIT_USE_LONG_OFFSETS
    const bool useoffset64 = true;
#else
    const bool useoffset64 = (8+getSerialSize() > 0x80000000UL);
#endif
    const bool haveCoarseBins = ! (cbits.empty() || cbounds.empty());
    char header[] = "#IBIS\7\0\0";
    header[5] = (char)(haveCoarseBins ? ibis::index::FUZZ : ibis::index::RELIC);
    header[6] = (char)(useoffset64 ? 8 : 4);
    ierr = UnixWrite(fdes, header, 8);
    if (ierr < 8) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt
            << " failed to write the 8-byte header, ierr = " << ierr;
        return -3;
    }
    if (useoffset64) {
        ierr = ibis::relic::write64(fdes);
        if (ierr >= 0 && haveCoarseBins)
            ierr = writeCoarse64(fdes);
    }
    else {
        ierr = ibis::relic::write32(fdes); // write the bulk of the index file
        if (ierr >= 0 && haveCoarseBins)
            ierr = writeCoarse32(fdes); // write the coarse level bins
    }
    if (ierr == 0) {
#if defined(FASTBIT_SYNC_WRITE)
#if _POSIX_FSYNC+0 > 0
        (void) UnixFlush(fdes); // write to disk
#elif defined(_WIN32) && defined(_MSC_VER)
        (void) _commit(fdes);
#endif
#endif

        const uint32_t nobs = vals.size();
        const uint32_t nc = (cbounds.size()-1 <= cbits.size() ?
                             cbounds.size()-1 : cbits.size());
        LOGGER(ibis::gVerbose > 5)
            << evt << " wrote " << nobs << " fine bitmap" << (nobs>1?"s":"")
            << " and " << nc << " coarse bitmap" << (nc>1?"s":"")
            << " to " << fnm;
    }
    return ierr;
} // ibis::fuzz::write

/// Write the coarse bins to an open file.  This function is to be called
/// after calling ibis::relic::write32, however, it does not check for this
/// fact!
int ibis::fuzz::writeCoarse32(int fdes) const {
    if (cbounds.empty() || cbits.empty() || nrows == 0)
        return -4;
    std::string evt = "fuzz";
    if (ibis::gVerbose > 0) {
        evt += '[';
        evt += col->partition()->name();
        evt += '.';
        evt += col->name();
        evt += ']';
    }
    evt += "::writeCoarse32";

    off_t ierr;
    const uint32_t nc = cbounds.size()-1;
    const uint32_t nb = cbits.size();

    ierr =  UnixWrite(fdes, &nc, sizeof(nc));
    ierr += UnixWrite(fdes, cbounds.begin(), sizeof(uint32_t)*(nc+1));
    if (ierr < static_cast<off_t>(sizeof(uint32_t)*(nc+2))) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " failed to write "
            << sizeof(uint32_t)*(nc+2) << " bytes to file descriptor "
            << fdes << ", ierr = " << ierr;
        return -5;
    }

    coffset64.clear();
    coffset32.resize(nb+1);
    coffset32[0] = UnixSeek(fdes, sizeof(int32_t)*(nb+1), SEEK_CUR);
    for (unsigned i = 0; i < nb; ++ i) {
        if (cbits[i] != 0)
            cbits[i]->write(fdes);
        coffset32[i+1] = UnixSeek(fdes, 0, SEEK_CUR);
    }

    const off_t pos = coffset32[0] - sizeof(int32_t) * (nb+1);
    ierr = UnixSeek(fdes, pos, SEEK_SET);
    if (ierr != pos) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " failed to seek to "
            << pos << " in file descriptor " << fdes
            << ", ierr = " << ierr;
        return -6;
    }
    
    ierr = UnixWrite(fdes, coffset32.begin(), sizeof(int32_t)*(nb+1));
    if (ierr < static_cast<off_t>(sizeof(int32_t)*(nb+1))) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " failed to write " << nb+1
            << " 4-byte bitmap offsets to file descriptor " << fdes
            << ", ierr = " << ierr;
        return -7;
    }
    ierr = UnixSeek(fdes, coffset32.back(), SEEK_SET);
    return (ierr == coffset32.back() ? 0 : -9);
} // ibis::fuzz::writeCoarse32

/// Write the coarse bins to an open file.  This function is to be called
/// after calling ibis::relic::write64, however, it does not check for this
/// fact!
int ibis::fuzz::writeCoarse64(int fdes) const {
    if (cbounds.empty() || cbits.empty() || nrows == 0)
        return -4;
    std::string evt = "fuzz";
    if (ibis::gVerbose > 0) {
        evt += '[';
        evt += col->partition()->name();
        evt += '.';
        evt += col->name();
        evt += ']';
    }
    evt += "::writeCoarse64";

    off_t ierr;
    const uint32_t nc = cbounds.size()-1;
    const uint32_t nb = cbits.size();

    ierr =  UnixWrite(fdes, &nc, sizeof(nc));
    ierr += UnixWrite(fdes, cbounds.begin(), sizeof(uint32_t)*(nc+1));
    if (ierr < static_cast<off_t>(sizeof(uint32_t)*(nc+2))) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " failed to write "
            << sizeof(uint32_t)*(nc+2) << " bytes to file descriptor "
            << fdes << ", ierr = " << ierr;
        return -5;
    }

    coffset32.clear();
    coffset64.resize(nb+1);
    coffset64[0] = UnixSeek(fdes, sizeof(int64_t)*(nb+1), SEEK_CUR);
    for (unsigned i = 0; i < nb; ++ i) {
        if (cbits[i] != 0)
            cbits[i]->write(fdes);
        coffset64[i+1] = UnixSeek(fdes, 0, SEEK_CUR);
    }

    const off_t pos = coffset64[0] - sizeof(int64_t) * (nb+1);
    ierr = UnixSeek(fdes, pos, SEEK_SET);
    if (ierr != pos) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " failed to seek to "
            << pos << " in file descriptor " << fdes
            << ", ierr = " << ierr;
        return -6;
    }
    
    ierr = UnixWrite(fdes, coffset64.begin(), sizeof(int64_t)*(nb+1));
    if (ierr < static_cast<off_t>(sizeof(int64_t)*(nb+1))) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " failed to write " << nb+1
            << " 8-byte bitmap offsets to file descriptor " << fdes
            << ", ierr = " << ierr;
        return -7;
    }
    ierr = UnixSeek(fdes, coffset64.back(), SEEK_SET);
    return (ierr == coffset64.back() ? 0 : -9);
} // ibis::fuzz::writeCoarse64

/// Read an index from the specified location.  The incoming argument can
/// be the name of a directory or a file.  The actual index file name is
/// determined by the function indexFileName.
int ibis::fuzz::read(const char* f) {
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
                  header[5] == static_cast<char>(FUZZ) &&
                  (header[6] == 8 || header[6] == 4) &&
                  header[7] == static_cast<char>(0))) {
        if (ibis::gVerbose > 0) {
            ibis::util::logger lg;
            lg() << "Warning -- fuzz[" << col->partition()->name() << '.'
                 << col->name() << "]::read the header from " << fnm << " (";
            printHeader(lg(), header);
            lg() << ") does not contain the expected values";
        }
        return -3;
    }

    uint32_t dim[3];
    size_t begin, end;
    clear(); // clear the current content
    fname = ibis::util::strnewdup(fnm.c_str());

    off_t ierr = UnixRead(fdes, static_cast<void*>(dim), 3*sizeof(uint32_t));
    if (ierr < static_cast<int>(3*sizeof(uint32_t))) {
        return -4;
    }
    nrows = dim[0];
    // read vals
    begin = 8*((3*sizeof(uint32_t) + 15) / 8);
    end = begin + dim[2] * sizeof(double);
    {
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
        ibis::util::logger lg;
        lg() << "DEBUG -- fuzz[" << col->partition()->name() << '.'
             << col->name() << "]::read(" << fnm
             << ") got nobs = " << dim[1] << ", card = " << dim[2]
             << ", the offsets of the bit vectors are\n";
        if (offset64.size() > bits.size()) {
            for (unsigned i = 0; i < nprt; ++ i)
                lg() << offset64[i] << " ";
            if (nprt < dim[1])
                lg() << "... (skipping " << dim[1]-nprt << ") ... ";
            lg() << offset64[dim[1]] << "\n";
        }
        else {
            for (unsigned i = 0; i < nprt; ++ i)
                lg() << offset32[i] << " ";
            if (nprt < dim[1])
                lg() << "... (skipping " << dim[1]-nprt << ") ... ";
            lg() << offset32[dim[1]] << "\n";
        }
    }
#endif

    initBitmaps(fdes);

    // reading the coarse bins
    if (offset64.size() > dim[1]) {
        ierr = UnixSeek(fdes, offset64.back(), SEEK_SET);
        if (ierr != offset64.back()) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- fuzz[" << col->partition()->name() << '.'
                << col->name() << "]::read(" << fnm << ") failed to seek to "
                << offset64.back() << ", ierr = " << ierr;
            return -4;
        }
    }
    else {
        ierr = UnixSeek(fdes, offset32.back(), SEEK_SET);
        if (ierr != offset32.back()) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- fuzz[" << col->partition()->name() << '.'
                << col->name() << "]::read(" << fnm << ") failed to seek to "
                << offset32.back() << ", ierr = " << ierr;
            return -4;
        }
    }

    uint32_t nc;
    ierr = UnixRead(fdes, &nc, sizeof(nc));
    if (ierr < (off_t) sizeof(nc)) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fuzz[" << col->partition()->name() << '.'
            << col->name() << "]:read(" << fnm
            << ") failed to read the number of coarse bins, ierr = " << ierr;
        return -6;
    }

    if (header[6] == 8) {
        begin = offset64.back() + sizeof(nc);
        end = begin + sizeof(uint32_t)*(nc+1);
        if (ierr > 0 && nc > 0) {
            array_t<uint32_t> tmp(fdes, begin, end);
            cbounds.swap(tmp);
        }
        begin = end;
        end += sizeof(int64_t) * (nc+1);
        if (cbounds.size() == nc+1) {
            array_t<int64_t> tmp(fdes, begin, end);
            coffset64.swap(tmp);
        }
        coffset32.clear();
    }
    else {
        begin = offset32.back() + sizeof(nc);
        end = begin + sizeof(uint32_t)*(nc+1);
        if (ierr > 0 && nc > 0) {
            array_t<uint32_t> tmp(fdes, begin, end);
            cbounds.swap(tmp);
        }
        begin = end;
        end += sizeof(int32_t) * (nc+1);
        if (cbounds.size() == nc+1) {
            array_t<int32_t> tmp(fdes, begin, end);
            coffset32.swap(tmp);
        }
        coffset64.clear();
    }

    for (unsigned i = 0; i < cbits.size(); ++ i)
        delete cbits[i];
    cbits.resize(nc);
    for (unsigned i = 0; i < nc; ++ i)
        cbits[i] = 0;

    LOGGER(ibis::gVerbose > 7)
        << "fuzz[" << col->partition()->name() << '.' << col->name()
        << "::read(" << fnm << ") -- finished reading the header";
    return 0;
} // ibis::fuzz::read

/// Reading information about the coarse bins.  To be used after calling
/// ibis::relic::read, which happens in the constructor.
int ibis::fuzz::readCoarse(const char* fn) {
    std::string fnm;
    indexFileName(fnm, fn);

    int fdes = UnixOpen(fnm.c_str(), OPEN_READONLY);
    if (fdes < 0) return -1;
    IBIS_BLOCK_GUARD(UnixClose, fdes);
#if defined(_WIN32) && defined(_MSC_VER)
    (void)_setmode(fdes, _O_BINARY);
#endif

    off_t ierr;
    if (offset64.size() > bits.size()) {
        ierr = UnixSeek(fdes, offset64.back(), SEEK_SET);
        if (ierr != offset64.back()) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- fuzz[" << col->partition()->name() << '.'
                << col->name() << "]::readCoarse failed to seek to "
                << offset64.back() << ", ierr = " << ierr;
            return -1;
        }
    }
    else {
        ierr = UnixSeek(fdes, offset32.back(), SEEK_SET);
        if (ierr != offset32.back()) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- fuzz[" << col->partition()->name() << '.'
                << col->name() << "]::readCoarse failed to seek to "
                << offset32.back() << ", ierr = " << ierr;
            return -2;
        }
    }

    uint32_t nc;
    size_t begin, end;
    ierr = UnixRead(fdes, &nc, sizeof(nc));
    if (ierr < (off_t)sizeof(nc)) {
        return -3;
    }
    if (nc == 0) {
        cbits.clear();
        cbounds.clear();
        coffset32.clear();
        coffset64.clear();
        return 0;
    }

    const uint32_t nb = nc + 1 - (nc+1)/2;
    if (offset64.size() > bits.size()) { // 64-bit offsets
        begin = offset64.back() + sizeof(nc);
        end = begin + sizeof(uint32_t)*(nc+1);
        {
            array_t<uint32_t> tmp(fdes, begin, end);
            cbounds.swap(tmp);
        }
        begin = end;
        end += sizeof(int64_t) * (nb+1);
        {
            array_t<int64_t> tmp(fdes, begin, end);
            coffset64.swap(tmp);
        }
        coffset32.clear();
    }
    else {
        begin = offset32.back() + sizeof(nc);
        end = begin + sizeof(uint32_t)*(nc+1);
        {
            array_t<uint32_t> tmp(fdes, begin, end);
            cbounds.swap(tmp);
        }
        begin = end;
        end += sizeof(int32_t) * (nb+1);
        {
            array_t<int32_t> tmp(fdes, begin, end);
            coffset32.swap(tmp);
        }
        coffset64.clear();
    }

    for (unsigned i = 0; i < cbits.size(); ++ i)
        delete cbits[i];
    cbits.resize(nb);
    for (unsigned i = 0; i < nb; ++ i)
        cbits[i] = 0;

    LOGGER(ibis::gVerbose > 6)
        << "fuzz[" << col->partition()->name() << '.' << col->name()
        << "]::readCoarse(" << fnm
        << ") -- finished reading the metadta about the coarse bins";
    return 0;
} // ibis::fuzz::readCoarse

/// Reconstruct an index from a storage object.
int ibis::fuzz::read(ibis::fileManager::storage* st) {
    if (st == 0) return -1;
    if (st->begin()[5] != ibis::index::FUZZ) return -3;
    clear();

    const char offsetsize = st->begin()[6];
    nrows = *(reinterpret_cast<uint32_t*>(st->begin()+8));
    size_t end;
    size_t pos = 8 + sizeof(uint32_t);
    const uint32_t nobs = *(reinterpret_cast<uint32_t*>(st->begin()+pos));
    pos += sizeof(uint32_t);
    const uint32_t card = *(reinterpret_cast<uint32_t*>(st->begin()+pos));
    pos += sizeof(uint32_t) + 7;
    pos = (pos / 8) * 8;
    end = pos + sizeof(double)*card;
    {
        array_t<double> dbl(st, pos, end);
        vals.swap(dbl);
    }
    int ierr = initOffsets(st, pos + sizeof(double)*card, nobs);
    if (ierr < 0)
        return ierr;

    initBitmaps(st);

    if (!(offsetsize == 8 &&
          str->size() > static_cast<size_t>(offset64.back())) ||
        (offsetsize == 4 &&
         str->size() > static_cast<size_t>(offset32.back()))) 
        return 0;

    const uint32_t nc =
        *(reinterpret_cast<uint32_t*>
          (str->begin() +
           (offsetsize == 8 ? (size_t) offset64.back()
            : (size_t)offset32.back())));

    if (nc == 0
        || (offsetsize == 8 &&
            str->size() < static_cast<uint32_t>(offset32.back()) +
            (sizeof(int64_t)+sizeof(uint32_t))*(nc+1))
        || (offsetsize == 4 &&
            str->size() < static_cast<uint32_t>(offset32.back()) +
            (sizeof(int32_t)+sizeof(uint32_t))*(nc+1)))
        return 0;

    uint32_t start;
    if (offsetsize == 8)
        start = offset64.back() + 4;
    else
        start = offset32.back() + 4;
    end = start + sizeof(uint32_t) * (nc+1);
    array_t<uint32_t> btmp(str, start, end);
    cbounds.swap(btmp);

    const uint32_t nb = nc + 1 - (nc+1)/2;
    start = end;
    end += offsetsize * (nb+1);
    if (offsetsize == 8) {
        array_t<int64_t> otmp(str, start, end);
        coffset64.swap(otmp);
        coffset32.clear();
    }
    else {
        array_t<int32_t> otmp(str, start, end);
        coffset32.swap(otmp);
        coffset64.clear();
    }

    for (unsigned j = 0; j < cbits.size(); ++ j)
        delete cbits[j];
    cbits.resize(nb);
    for (unsigned i = 0; i < nb; ++ i)
        cbits[i] = 0;
    return 0;
} // ibis::fuzz::read

void ibis::fuzz::clear() {
    clearCoarse();
    ibis::relic::clear();
} // ibis::fuzz::clear

void ibis::fuzz::clearCoarse() {
    const unsigned nb = cbits.size();
    for (unsigned i = 0; i < nb; ++ i)
        delete cbits[i];

    cbits.clear();
    cbounds.clear();
    coffset32.clear();
    coffset64.clear();
} // ibis::fuzz::clearCoarse

// the printing function
void ibis::fuzz::print(std::ostream& out) const {
    if (vals.size() != bits.size() || bits.empty())
        return;

    const uint32_t nc = (cbounds.empty() ? 0U : cbounds.size()-1);
    const uint32_t ncb = nc+1 - (nc+1)/2;
    out << "the interval-equality encoded bitmap index for "
        << col->partition()->name() << '.'
        << col->name() << " contains " << nc << " coarse bin"
        << (nc>1 ? "s" : "") << " and " << bits.size()
        << " fine bit vectors for " << nrows << " objects\n";
    uint32_t nprt = (ibis::gVerbose < 30 ? 1 << ibis::gVerbose : bits.size());
    uint32_t omitted = 0;
    uint32_t end;
    if (nc > 0 && cbits.size() == ncb) {
        // has coarse bins
        for (unsigned j = 0; j < nc; ++ j) {
            out << "Coarse bin " << j << ", [" << cbounds[j] << ", "
                << cbounds[j+1] << ")";
            if (j < ncb && cbits[j] != 0)
                out << "\t{[" << cbounds[j] << ", " << cbounds[j+(nc+1)/2]
                    << ")\t" << cbits[j]->cnt() << "\t" << cbits[j]->bytes()
                    << "}\n";
            else
                out << "\n";
            end = (cbounds[j+1] <= cbounds[j]+nprt ?
                   cbounds[j+1] : cbounds[j]+nprt);
            for (unsigned i = cbounds[j]; i < end; ++ i) {
                if (bits[i]) {
                    out << "\t" << i << ":\t";
                    out.precision(12);
                    out << vals[i] << "\t" << bits[i]->cnt()
                        << "\t" << bits[i]->bytes() << "\n";
                }
                else {
                    ++ omitted;
                }
            }
            if (cbounds[j+1] > end && nprt > 0) {
                out << "\t...\n";
                omitted += (cbounds[j+1] - end);
            }
        }
        if (nprt > 0 && omitted > 0)
            out << "\tfine level bitmaps omitted: " << omitted << "\n";
    }
    else { // no coarse bins
        const uint32_t nobs = bits.size();
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
            out << " (printing 1 out of every " << skip << ")\n";
        }

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
    }
    out << "\n";
} // ibis::fuzz::print

/// Estiamte the size of the index in a file.
size_t ibis::fuzz::getSerialSize() const throw() {
    size_t res = 40 + 8 * (bits.size() + vals.size())
        + 12 * (cbits.size());
    for (unsigned j = 0; j < bits.size(); ++ j)
        if (bits[j] != 0)
            res += bits[j]->getSerialSize();
    for (unsigned j = 0; j < cbits.size(); ++ j)
        if (cbits[j] != 0)
            res += cbits[j]->getSerialSize();
    return res;
} // ibis::fuzz::getSerialSize
