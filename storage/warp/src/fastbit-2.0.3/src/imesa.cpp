// $Id$
// Author: John Wu <John.Wu at ACM.org>
// Copyright (c) 2000-2016 the Regents of the University of California
//
// This file contains the implementation of the classes ibis::mesa that
// defines a two-sided range encoding known as the interval encoding
//
#if defined(_WIN32) && defined(_MSC_VER)
#pragma warning(disable:4786)   // some identifier longer than 256 characters
#endif
#include "ibin.h"
#include "part.h"
#include "column.h"
#include "resource.h"

#include <string>
#define FASTBIT_SYNC_WRITE 1

/// Constructor.  Generate index from the base data.
ibis::mesa::mesa(const ibis::column* c, const char* f) : ibis::bin(c, f) {
    if (c == 0) return;
    if (nrows == 0) return;
    if (nobs <= 2) {
        clear();
        throw "mesa::ctor needs more bins but there are two or fewer bins"
            IBIS_FILE_LINE;
    }

    // b2 is the temporary storage for the bitvectors of ibis::bin object
    array_t<bitvector*> b2(nobs);
    for (uint32_t i=0; i<nobs; ++i) {// copy the pointers
        b2[i] = bits[i];
        bits[i] = 0;
    }
    try {
        uint32_t n2 = (nobs + 1) / 2;
        bits[0] = new ibis::bitvector;
        sumBits(b2, 0, n2, *(bits[0]));
        for (uint32_t i=1; i + n2 <= nobs; ++i) {
            bits[i] = new ibis::bitvector();
            bits[i]->copy(*bits[i-1]);
            *(bits[i]) -= *(b2[i-1]);
            *(bits[i]) |= *(b2[i+n2-1]);
        }
        for (uint32_t i = 0; i < nobs; ++ i) { // done with b2
            delete b2[i];
            b2[i] = 0; // change it to nil to avoid being deleted again
        }

        for (uint32_t i = 0; i+n2 <= nobs; ++i)
            bits[i]->decompress();
        optionalUnpack(bits, col->indexSpec());

        if (ibis::gVerbose > 2) {
            ibis::util::logger lg;
            lg()
                << "mesa[" << col->partition()->name() << '.' << col->name()
                << "]::ctor -- constructed an interval index with "
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
            << "]::mesa::ctor encountered an exception, cleaning up ...";
        for (uint32_t i = 0; i < nobs; ++ i) {
            delete b2[i];
        }
        clear();
        throw;
    }
} // constructor

/// Constructor.  Construct an interval encoded index from an equality
/// encoded index.
ibis::mesa::mesa(const ibis::bin& rhs) {
    if (rhs.nrows == 0) return;
    if (rhs.nobs <= 2) { // rhs does not contain a valid index
        throw ibis::bad_alloc("mesa::ctor needs more bins" IBIS_FILE_LINE);
    }
    try {
        uint32_t i, n2 = (rhs.nobs+1) / 2;
        col = rhs.col;
        nobs = rhs.nobs;
        bits.resize(nobs);
        nrows = rhs.nrows;
        bounds.deepCopy(rhs.bounds);
        maxval.deepCopy(rhs.maxval);
        minval.deepCopy(rhs.minval);

        //rhs.activate(); // make sure all bitvectors are here
        bits[0] = new ibis::bitvector;
        sumBits(rhs.bits, 0, n2, *bits[0]);
        bits[n2] = 0;

        for (i = 1; i + n2 <= nobs; ++i) {
            bits[i] = new ibis::bitvector;
            bits[i]->copy(*(bits[i-1]));
            *(bits[i]) -= *(rhs.bits[i-1]);
            *(bits[i]) |= *(rhs.bits[i+n2-1]);
            bits[i+n2-1] = 0;
        }

        for (i = 0; i+n2 <= nobs; ++i)
            bits[i]->decompress();
        optionalUnpack(bits, col->indexSpec());

        if (ibis::gVerbose > 2) {
            ibis::util::logger lg;
            lg() << "mesa[" << col->partition()->name() << '.' << col->name()
                 << "]::ctor -- converted an equality index into an "
                "interval index with "
                 << nobs << " bin" << (nobs>1?"s":"") << " for "
                 << nrows << " row" << (nrows>1?"s":"")
                 << " from an equality index";
            if (ibis::gVerbose > 6) {
                lg() << "\n";
                print(lg());
            }
        }
    }
    catch (...) {
        LOGGER(ibis::gVerbose > 1)
            << "Warning -- column[" << col->name()
            << "]::mesa::ctor encountered an exception, cleaning up ...";
        clear();
        throw;
    }
} // copy constructor (from bin)

/// Constructor.  Reconstruct an index from a storage object.  The layout
/// of data members are the same as ibis::bin.  The difference is that the
/// last half of the bit vectors are always empty.
ibis::mesa::mesa(const ibis::column* c, ibis::fileManager::storage* st,
                 size_t start) : ibis::bin(c, st, start) {
    if (ibis::gVerbose > 2) {
        ibis::util::logger lg;
        lg()
            << "mesa[" << col->partition()->name() << '.' << col->name()
            << "]::ctor -- intialized an interval index with "
            << nobs << " bin" << (nobs>1?"s":"") << " for "
            << nrows << " row" << (nrows>1?"s":"")
            << " from a storage object @ " << st;
        if (ibis::gVerbose > 6) {
            lg() << "\n";
            print(lg());
        }
    }
}

/// Write the content of the index the specified location.  The argument dt
/// can be either a directory name or a file name.  The actual file name to
/// use for exporting the index is determined by the function
/// indexFileName.  The layout of the member variables is the same as with
/// ibis::bin.
int ibis::mesa::write(const char* dt) const {
    if (nobs <= 0) return -1;

    std::string fnm, evt;
    indexFileName(fnm, dt);
    evt = "mesa";
    if (col != 0 && ibis::gVerbose > 1) {
        evt += '[';
        evt += col->fullname();
        evt += ']';
    }
    evt += "::write";
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
        activate(0U, nobs-(nobs-1)/2);

    int fdes = UnixOpen(fnm.c_str(), OPEN_WRITENEW, OPEN_FILEMODE);
    if (fdes < 0) { // try again
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
    const bool useoffset64 = (8+getSerialSize() >= 0x80000000UL);
#endif
    char header[] = "#IBIS\6\0\0";
    header[5] = (char)ibis::index::MESA;
    header[6] = (char)(useoffset64 ? 8 : 4);
    off_t ierr = UnixWrite(fdes, header, 8);
    if (ierr < 8) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt
            << " failed to write the 8-byte header, ierr = " << ierr;
        return -3;
    }

    if (useoffset64)
        ierr = ibis::bin::write64(fdes);
    else
        ierr = ibis::bin::write32(fdes);
    if (ierr >= 0) {
#if defined(FASTBIT_SYNC_WRITE)
#if _POSIX_FSYNC+0 > 0
        (void) UnixFlush(fdes); // write to disk
#elif defined(_WIN32) && defined(_MSC_VER)
        (void) _commit(fdes);
#endif
#endif

        LOGGER(ibis::gVerbose > 3)
            << evt << " wrote " << nobs << " bin"
            << (nobs>1?"s":"") << " to file " << fnm << "for " << nrows
            << " row" << (nrows>1?"s":"");
    }
    return ierr;
} // ibis::mesa::write

/// Constructor an new index from data file in the specified location.
/// This function first construct equality bins then convert to the
/// interval encoding.
void ibis::mesa::construct(const char* df) {
    ibis::bin::construct(df); // initial binning

    // b2 is the temporary storage for the bitvectors of ibis::bin object
    array_t<bitvector*> b2(nobs);
    for (uint32_t i=0; i<nobs; ++i) {// copy the pointers
        b2[i] = bits[i];
        bits[i] = 0;
    }
    try {
        uint32_t n2 = (nobs + 1) / 2;
        bits[0] = new ibis::bitvector;
        sumBits(b2, 0, n2, *(bits[0]));
        for (uint32_t i=1; i + n2 <= nobs; ++i) {
            bits[i] = new ibis::bitvector();
            bits[i]->copy(*bits[i-1]);
            *(bits[i]) -= *(b2[i-1]);
            *(bits[i]) |= *(b2[i+n2-1]);
        }
        for (uint32_t i = 0; i < nobs; ++ i) { // done with b2
            delete b2[i];
            b2[i] = 0; // change it to nil to avoid being deleted again
        }

        bits.resize(nobs-n2+1);
        for (uint32_t i = 0; i+n2 <= nobs; ++i) {
            bits[i]->compress();
        }

        if (ibis::gVerbose > 4) {
            ibis::util::logger lg;
            print(lg());
        }
    }
    catch (...) {
        LOGGER(ibis::gVerbose > 1)
            << "Warning -- mesa[" << col->partition()->name() << '.'
            << col->name() << "]::ctor received an exception, cleaning up ...";
        for (uint32_t i = 0; i < nobs; ++ i) {
            delete b2[i];
        }
        clear();
        throw;
    }
} // ibis::mesa::construct

void ibis::mesa::binBoundaries(std::vector<double>& ret) const {
    ret.resize(nobs+1);
    for (uint32_t i = 0; i < nobs; ++ i)
        ret.push_back(bounds[i]);
} // ibis::mesa::binBoundaries

void ibis::mesa::binWeights(std::vector<uint32_t>& ret) const {
    uint32_t i = 0, n2 = (nobs+1)/2;
    activate();
    ret.resize(nobs);
    while (i < nobs-n2) {
        ibis::bitvector *tmp = *(bits[i]) - *(bits[i+1]);
        ret[i] = tmp->cnt();
        delete tmp;
        ++ i;
    }
    if (n2+n2 > nobs) {
        ibis::bitvector *tmp = *(bits[0]) & *(bits[nobs-n2]);
        ret[i] = tmp->cnt();
        delete tmp;
        ++ i;
    }
    while (i < nobs) {
        ibis::bitvector *tmp = *(bits[i-n2+1]) - *(bits[i-n2]);
        ret[i] = tmp->cnt();
        delete tmp;
        ++ i;
    }
} //ibis::mesa::binWeights

// a simple function to test the speed of the bitvector operations
void ibis::mesa::speedTest(std::ostream& out) const {
    uint32_t i, nloops = 1000000000 / nrows;
    if (nloops < 2) nloops = 2;
    ibis::horometer timer;
    col->logMessage("range::speedTest", "testing the speed of operator -");
    activate(0U, nobs-(nobs-1)/2);

    for (i = 0; i < (nobs+1)/2-1; ++i) {
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
                << timer.CPUTime() / nloops << std::endl;
        }
    }
} // ibis::mesa::speedTest

// the printing function
void ibis::mesa::print(std::ostream& out) const {
    uint32_t n2 = (nobs+1)/2;
    out << "index (interval encoded) for ibis::column " << col->name()
        << " contains " << n2 << " bitvectors for "
        << nrows << " objects \n";
    if (ibis::gVerbose > 4) { // the long format
        out << "number of bits: " << nrows << "\n";
        if (bits[0])
            out << "0 - " << n2 << ": (..., " << bounds[n2-1] << "),\t"
                << bits[0]->cnt() << "\n";
        for (uint32_t i=0; i<nobs-n2; ++i) {
            if (bits[i+1] == 0) continue; // skip if bits[i+1] is not set
            out << i+1 << " - " << i+n2 << ": [" << bounds[i] << ", "
                << bounds[i+n2] << "),\t" << bits[i+1]->cnt() << "\n";
            //out << *(bits[i+1]);
            if (bits[i+1]->size() != nrows)
                out << "bits[" << i+1 << "] should have " << nrows
                    << "bits, but actually has " << bits[i+1]->size() << "\n";
        }
// the following code segment need to allocate and deallocate bitvectors which
//      needs to invoke more print statement that might cause iolock problems
//      bitvector* tmp;
//      tmp = *(bits[0]) - *(bits[1]);
//      out << "0: " << bits[0]->cnt() << "\t(..., " << bounds[n2-1] << "),\t"
//          << tmp->cnt() << "\t(..., " << bounds[0] << ");\t[" << minval[0]
//          << ", " << maxval[0] << "]\n";
//      delete tmp;
//      uint32_t i = 1, cnt = nrows;
//      while (i < nobs-n2) {
//          tmp = *(bits[i]) - *(bits[i+1]);
//          out << i << ": " << bits[i]->cnt() << "\t[" << bounds[i-1] << ", "
//              << bounds[i+n2-1] << ")\t" << tmp->cnt() << "\t["
//              << bounds[i-1] << ", " << bounds[i]
//              << ");\t[" << minval[i] << ", " << maxval[i] << "]\n";
//          delete tmp;
//          if (cnt != bits[i]->size())
//              out << "Warning: bits[" << i << "] contains "
//                  << bits[i]->size()
//                  << " bits, but " << cnt << " are expected\n";
//          ++i;
//      }
//      if (n2+n2 > nobs) {
//          tmp = *(bits[0]) & *(bits[nobs-n2]);
//          out << i << ": " << bits[i]->cnt() << "\t[" << bounds[i-1] << ", "
//              << bounds[i+n2-1] << ")\t" << tmp->cnt() << "\t["
//              << bounds[i-1] << ", " << bounds[i]
//              << ");\t[" << minval[i] << ", " << maxval[i] << "]\n";
//          delete tmp;
//          if (cnt != bits[i]->size())
//              out << "Warning: bits[" << i << "] contains "
//                  << bits[i]->size()
//                  << " bits, but " << cnt << " are expected\n";
//          ++i;
//      }
//      while (i < nobs) {
//          tmp = *(bits[i-n2+1]) - *(bits[i-n2]);
//          out << i << ": " << tmp->cnt() << "\t["
//              << bounds[i-1] << ", " << bounds[i]
//              << ");\t[" << minval[i] << ", " << maxval[i] << "]\n";
//          ++i;
//      }
    }
    else { // the short format
        out << "The three columns are (1) center of bin, (2) bin weight, "
            "and (3) bit vector size (bytes)\n";
        for (uint32_t i=0; i<=nobs-n2; ++i) {
            if (bits[i] && bits[i]->cnt()) {
                out.precision(12);
                out << 0.5*(minval[i]+maxval[i]) << '\t'
                    << bits[i]->cnt() << '\t' << bits[i]->bytes() << "\n";
            }
        }
    }
    out << std::endl;
} // ibis::mesa::print

long ibis::mesa::append(const char* dt, const char* df, uint32_t nnew) {
    const uint32_t nold =
        (std::strcmp(dt, col->partition()->currentDataDir()) == 0 ?
         col->partition()->nRows()-nnew : nrows);
    if (nold != nrows) {
#ifdef APPEND_UPDATE_INDEXES
        clear();
        construct(dt);
#endif
        return nnew;
    }

    std::string fnm;
    indexFileName(fnm, df);
    ibis::mesa* bin0=0;
    ibis::fileManager::storage* st0=0;
    long ierr = ibis::fileManager::instance().getFile(fnm.c_str(), &st0);
    if (ierr == 0 && st0 != 0) {
        const char* header = st0->begin();
        if (header[0] == '#' && header[1] == 'I' && header[2] == 'B' &&
            header[3] == 'I' && header[4] == 'S' &&
            header[5] == ibis::index::MESA &&
            header[7] == static_cast<char>(0)) {
            bin0 = new ibis::mesa(col, st0);
        }
        else {
            if (ibis::gVerbose > 5)
                col->logMessage("mesa::append", "file \"%s\" has unexecpted "
                                "header -- it will be removed", fnm.c_str());
            ibis::fileManager::instance().flushFile(fnm.c_str());
            remove(fnm.c_str());
        }
    }
    if (bin0 == 0) {
        ibis::bin bin1(col, df, bounds);
        bin0 = new ibis::mesa(bin1);
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
        col->logWarning("mesa::append", "failed to generate index with "
                        "data from %s", df);
        return -6;
    }
} // ibis::mesa::append

long ibis::mesa::append(const ibis::mesa& tail) {
    uint32_t i;
    if (tail.col != col) return -1;
    if (tail.nobs != nobs) return -2;
    if (tail.bits.empty()) return -3;
    if (tail.bits[0]->size() != tail.bits[1]->size()) return -4;
    for (i = 0; i < nobs; ++i)
        if (tail.bounds[i] != bounds[i]) return -5;

    // generate the new minval, maxval and bits
    uint32_t n=0;
    array_t<double> min2, max2;
    array_t<bitvector*> bin2;
    min2.resize(nobs);
    max2.resize(nobs);
    bin2.resize(nobs);
    nrows += tail.nrows;
    activate(0U, nobs-(nobs-1)/2);
    tail.activate(0U, tail.nobs-(tail.nobs-1)/2);
    for (i = 0; i < nobs; ++i) {
        if (tail.minval[i] <= minval[i])
            min2[i] = tail.minval[i];
        else
            min2[i] = minval[i];
        if (tail.maxval[i] >= maxval[i])
            max2[i] = tail.maxval[i];
        else
            max2[i] = maxval[i];
        if (bits[i] != 0 && tail.bits[i] != 0) {
            bin2[i] = new ibis::bitvector;
            bin2[i]->copy(*bits[i]);
            *bin2[i] += *(tail.bits[i]);
            if (n) {
                if (n != bin2[i]->size())
                    col->logWarning("mesa::append", "bitmap %ld is expected "
                                    "to have %lu bits but actually has %lu",
                                    static_cast<long>(i),
                                    static_cast<long unsigned>(n),
                                    static_cast<long unsigned>
                                    (bin2[i]->size()));
            }
            else {
                n = bin2[i]->size();
            }
        }
        else {
            bin2[i] = 0;
        }
    }

    // replace the current content with the new one
    minval.swap(min2);
    maxval.swap(max2);
    bits.swap(bin2);
    // clearup bin2
    for (i = 0; i < nobs; ++i)
        delete bin2[i];

    return 0;
} // ibis::mesa::append

long ibis::mesa::evaluate(const ibis::qContinuousRange& expr,
                           ibis::bitvector& lower) const {
    long ierr = 0;
    if (nobs <= 0) {
        lower.set(0, nrows);
        return ierr;
    }

    // the following four variables describes a range where the solution lies
    // bitvectors in [hit0, hit1) ==> lower
    // bitvectors in [cand0, cand1) ==> upper
    // the four values are expected to be in the following order
    // cand0 <= hit0 <= hit1 <= cand1
    uint32_t cand0=0, hit0=0, hit1=0, cand1=0;
    locate(expr, cand0, cand1, hit0, hit1);
    if (cand0 >= cand1) {
        lower.set(0, nrows);
        return ierr;
    }

    const uint32_t n2 = (nobs+1) / 2;
    // compute the bitvector lower
    if (hit0 >= hit1) {
        lower.set(0, nrows);
    }
    else if (hit1 >= n2) {
        if (bits[hit1-n2] == 0)
            activate(hit1-n2);
        if (bits[hit1-n2] != 0)
            lower.copy(*(bits[hit1-n2]));
        else
            lower.set(0, nrows);
        if (hit0 > hit1-n2) {
            if (hit0 >= n2) {
                if (bits[hit0-n2] == 0)
                    activate(hit0-n2);
                if (bits[hit0-n2] != 0)
                    lower -= *(bits[hit0-n2]);
            }
            else {
                if (bits[hit0] == 0)
                    activate(hit0);
                if (bits[hit0] != 0)
                    lower &= *(bits[hit0]);
                else
                    lower.set(0, lower.size());
            }
        }
        else if (hit0 < hit1-n2) {
            if (bits[hit0] == 0)
                activate(hit0);
            if (bits[hit0] != 0)
                lower |= *(bits[hit0]);
        }
    }
    else {
        if (bits[hit0] == 0)
            activate(hit0);
        if (bits[hit0] != 0)
            lower.copy(*(bits[hit0]));
        else
            lower.set(0, nrows);
        if (hit1 <= nobs - n2) {
            if (bits[hit1] == 0)
                activate(hit1);
            if (bits[hit1] != 0)
                lower -= *(bits[hit1]);
        }
    }

    if (cand0+1 == hit0) { // candidate bin cand0
        if (hit0+n2 <= nobs) {
            activate(cand0, hit0+1);
            if (bits[cand0] != 0) {
                ibis::bitvector tmp(*(bits[cand0]));
                if (bits[hit0] != 0)
                    tmp -= *(bits[hit0]);

                if (tmp.cnt() > 0) {
                    ibis::bitvector res;
                    ierr = checkBin(expr, cand0, tmp, res);
                    if (ierr > 0)
                        lower |= res;
                    else if (ierr < 0)
                        return ierr;
                }
            }
        }
        else if (cand0 >= n2) {
            activate(cand0-n2, hit0-n2+1);
            if (hit0 < nobs && bits[hit0-n2] != 0) {
                ibis::bitvector tmp(*(bits[hit0-n2]));
                if (bits[cand0-n2] != 0)
                    tmp -= *(bits[cand0-n2]);

                if (tmp.cnt() > 0) {
                    ibis::bitvector res;
                    ierr = checkBin(expr, cand0, tmp, res);
                    if (ierr > 0)
                        lower |= res;
                    else if (ierr < 0)
                        return ierr;
                }
            }
            else if (hit0 >= nobs) { // use null mask as 
                ibis::bitvector tmp;
                col->getNullMask(tmp);
                if (bits[cand0-n2] != 0)
                    tmp -= *(bits[cand0-n2]);
                activate(0);
                if (bits[0] != 0)
                    tmp -= *(bits[0]);

                if (tmp.cnt() > 0) {
                    ibis::bitvector res;
                    ierr = checkBin(expr, cand0, tmp, res);
                    if (ierr > 0)
                        lower |= res;
                    else if (ierr < 0)
                        return ierr;
                }
            }
        }
        else { // cand0=n2-1, hit0=n2, the special middle bin
            activate(0);
            activate(cand0);
            if (bits[0] != 0 && bits[cand0] != 0) {
                ibis::bitvector tmp(*(bits[0]));
                tmp &= *(bits[cand0]);

                if (tmp.cnt() > 0) {
                    ibis::bitvector res;
                    ierr = checkBin(expr, cand0, tmp, res);
                    if (ierr > 0)
                        lower |= res;
                    else if (ierr < 0)
                        return ierr;
                }
            }
        }
    }
    if (hit1+1 == cand1) { // candidate bin hit1
        if (cand1+n2 <= nobs) {
            activate(hit1, cand1+1);
            if (bits[hit1] != 0) {
                ibis::bitvector tmp(*(bits[hit1]));
                if (bits[cand1] != 0)
                    tmp -= *(bits[cand1]);

                if (tmp.cnt() > 0) {
                    ibis::bitvector res;
                    ierr = checkBin(expr, hit1, tmp, res);
                    if (ierr > 0)
                        lower |= res;
                    else if (ierr < 0)
                        return ierr;
                }
            }
        }
        else if (hit1 >= n2) {
            activate(hit1-n2, cand1-n2+1);
            if (cand1 < nobs && bits[cand1-n2] != 0) {
                ibis::bitvector tmp(*(bits[cand1-n2]));
                if (bits[hit1-n2] != 0)
                    tmp -= *(bits[hit1-n2]);

                if (tmp.cnt() > 0) {
                    ibis::bitvector res;
                    ierr = checkBin(expr, hit1, tmp, res);
                    if (ierr > 0)
                        lower |= res;
                    else if (ierr < 0)
                        return ierr;
                }
            }
            else if (cand1 >= nobs) { // use null mask as 
                ibis::bitvector tmp;
                col->getNullMask(tmp);
                if (bits[hit1-n2] != 0)
                    tmp -= *(bits[hit1-n2]);
                activate(0);
                if (bits[0] != 0)
                    tmp -= *(bits[0]);

                if (tmp.cnt() > 0) {
                    ibis::bitvector res;
                    ierr = checkBin(expr, hit1, tmp, res);
                    if (ierr > 0)
                        lower |= res;
                    else if (ierr < 0)
                        return ierr;
                }
            }
        }
        else { // hit1=n2-1, cand1=n2, the special middle bin
            activate(0);
            activate(hit1);
            if (bits[0] != 0 && bits[hit1] != 0) {
                ibis::bitvector tmp(*(bits[0]));
                tmp &= *(bits[hit1]);

                if (tmp.cnt() > 0) {
                    ibis::bitvector res;
                    ierr = checkBin(expr, hit1, tmp, res);
                    if (ierr > 0)
                        lower |= res;
                    else if (ierr < 0)
                        return ierr;
                }
            }
        }
    }
    ierr = lower.cnt();
    return ierr;
} // ibis::mesa::evaluate

// provide an estimation based on the current index
// set bits in lower are hits for certain, set bits in upper are candidates
// set bits in (upper - lower) should be checked to verifies which are
// actually hits
void ibis::mesa::estimate(const ibis::qContinuousRange& expr,
                          ibis::bitvector& lower,
                          ibis::bitvector& upper) const {
    if (nobs <= 0) {
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
    if (nobs <= 0) {
        lower.set(0, nrows);
        upper.clear();
        return;
    }

    const uint32_t n2 = (nobs+1) / 2;
    // compute the bitvector lower
    if (hit0 >= hit1) {
        lower.set(0, nrows);
    }
    else if (hit1 >= n2) {
        if (bits[hit1-n2] == 0)
            activate(hit1-n2);
        if (bits[hit1-n2] != 0)
            lower.copy(*(bits[hit1-n2]));
        else
            lower.set(0, nrows);
        if (hit0 > hit1-n2) {
            if (hit0 >= n2) {
                if (bits[hit0-n2] == 0)
                    activate(hit0-n2);
                if (bits[hit0-n2] != 0)
                    lower -= *(bits[hit0-n2]);
            }
            else {
                if (bits[hit0] == 0)
                    activate(hit0);
                if (bits[hit0] != 0)
                    lower &= *(bits[hit0]);
                else
                    lower.set(0, lower.size());
            }
        }
        else if (hit0 < hit1-n2) {
            if (bits[hit0] == 0)
                activate(hit0);
            if (bits[hit0] != 0)
                lower |= *(bits[hit0]);
        }
    }
    else {
        if (bits[hit0] == 0)
            activate(hit0);
        if (bits[hit0] != 0)
            lower.copy(*(bits[hit0]));
        else
            lower.set(0, nrows);
        if (hit1 <= nobs - n2) {
            if (bits[hit1] == 0)
                activate(hit1);
            if (bits[hit1] != 0)
                lower -= *(bits[hit1]);
        }
    }

    // compute the bitvector upper
    if (hit0 == cand0 && hit1 == cand1) {
        upper.clear(); // clear upper to indicate lower is accurate
    }
    else if (cand0 >= cand1) {
        upper.set(0, lower.size());
    }
    else if (cand1 >= n2) {
        if (bits[cand1-n2] == 0)
            activate(cand1-n2);
        if (bits[cand1-n2] != 0)
            upper.copy(*(bits[cand1-n2]));
        else
            upper.set(0, nrows);
        if (cand0 + n2 > cand1) {
            if (cand0 >= n2) {
                if (bits[cand0-n2] == 0)
                    activate(cand0-n2);
                if (bits[cand0-n2] != 0)
                    upper -= *(bits[cand0-n2]);
            }
            else {
                if (bits[cand0] == 0)
                    activate(cand0);
                if (bits[cand0] != 0)
                    upper &= *(bits[cand0]);
                else
                    upper.set(0, upper.size());
            }
        }
        else if (cand0 < cand1 - n2) {
            if (bits[cand0] == 0)
                activate(cand0);
            if (bits[cand0] != 0)
                upper |= *(bits[cand0]);
        }
    }
    else {
        if (bits[cand0] == 0)
            activate(cand0);
        if (bits[cand0] != 0)
            upper.copy(*bits[cand0]);
        else
            upper.set(0, nrows);
        if (hit1 <= nobs - n2) {
            if (bits[cand1] == 0)
                activate(cand1);
            if (bits[cand1] != 0)
                upper -= *(bits[cand1]);
        }
    }
} // ibis::mesa::estimate

// return an upper bound on the number of hits
uint32_t ibis::mesa::estimate(const ibis::qContinuousRange& expr) const {
    if (nobs <= 0) return 0;

    uint32_t cand0=0, cand1=0, nhits=0;
    locate(expr, cand0, cand1);

    // compute the upper bound of number of hits
    if (cand1 > cand0) {
        uint32_t n2 = (nobs + 1) / 2;
        if (cand1 <= n2) {
            if (bits[cand0] == 0)
                activate(cand0);
            if (bits[cand0] != 0)
                nhits = bits[cand0]->cnt();
            if (bits[cand1-1] == 0)
                activate(cand1-1);
            if (bits[cand1-1] != 0)
                nhits += bits[cand1-1]->cnt();
            // sum the two bitvector, this is not meant to be an accurate
            // estimate, but a reflection of the expected amount of work
        }
        else {
            if (cand0 + n2 > cand1) {
                if (bits[cand1-n2-1] == 0)
                    activate(cand1-n2-1);
                if (bits[cand1-n2-1] != 0)
                    nhits = bits[cand1-n2-1]->cnt();
            }
            else {
                if (bits[cand0] == 0)
                    activate(cand0);
                if (bits[cand0] != 0)
                    nhits = bits[cand0]->cnt();
                if (bits[cand1-n2-1] == 0)
                    activate(cand1-n2-1);
                if (bits[cand1-n2-1] != 0)
                    nhits += bits[cand1-n2-1]->cnt();
            }
        }
    }
    return nhits;
} // ibis::mesa::estimate

// ***should implement a more efficient version***
float ibis::mesa::undecidable(const ibis::qContinuousRange& expr,
                              ibis::bitvector& iffy) const {
    float ret = 0;
    ibis::bitvector tmp;
    estimate(expr, tmp, iffy);
    if (iffy.size() == tmp.size())
        iffy -= tmp;
    else
        iffy.set(0, tmp.size());

    if (iffy.cnt() > 0) {
        uint32_t cand0=0, hit0=0, hit1=0, cand1=0;
        locate(expr, cand0, cand1, hit0, hit1);
        if (cand0+1 == hit0 && maxval[cand0] > minval[cand0]) {
            ret = (maxval[cand0] - expr.leftBound()) /
                (maxval[cand0] - minval[cand0]);
            if (ret < FLT_EPSILON)
                ret = FLT_EPSILON;
        }
        if (hit1+1 == cand1 && maxval[hit1] > minval[hit1]) {
            if (ret > 0)
                ret = 0.5 * (ret + (expr.rightBound() - minval[hit1]) /
                             (maxval[hit1] - minval[hit1]));
            else
                ret = (expr.rightBound() - minval[hit1]) /
                    (maxval[hit1] - minval[hit1]);
            if (ret < FLT_EPSILON)
                ret = FLT_EPSILON;
        }
    }
    return ret;
} // ibis::mesa::undecidable

double ibis::mesa::getSum() const {
    double ret;
    bool here = true;
    { // a small test block to evaluate variable here
        const size_t nbv = col->elementSize()*col->partition()->nRows();
        if (str != 0)
            here = (str->bytes()*3 < nbv);
        else if (offset64.size() > nobs)
            here = (static_cast<uint64_t>(offset64[nobs]*3) < nbv);
        else if (offset32.size() > nobs)
            here = (static_cast<uint32_t>(offset32[nobs]*3) < nbv);
    }
    if (here) {
        ret = computeSum();
    }
    else { // indicate sum is not computed
        ibis::util::setNaN(ret);
    }
    return ret;
} // ibis::mesa::getSum

double ibis::mesa::computeSum() const {
    double sum = 0;
    uint32_t i = 0;
    const uint32_t n2 = (nobs+1)/2;
    activate(0U, nobs-(nobs-1)/2); // need to activate all bitvectors
    while (i < nobs-n2) {
        if (minval[i] <= maxval[i]) {
            ibis::bitvector *tmp = *(bits[i]) - *(bits[i+1]);
            sum += 0.5 * (minval[i] + maxval[i]) * tmp->cnt();
            delete tmp;
        }
        ++ i;
    }
    if (n2+n2 > nobs) {
        if (minval[i] <= maxval[i]) {
            ibis::bitvector *tmp = *(bits[0]) & *(bits[nobs-n2]);
            sum += 0.5 * (minval[i]+maxval[i]) * tmp->cnt();
            delete tmp;
        }
        ++ i;
    }
    while (i < nobs) {
        if (minval[i] <= maxval[i]) {
            ibis::bitvector *tmp = *(bits[i-n2+1]) - *(bits[i-n2]);
            sum += 0.5 * (minval[i]+maxval[i]) * tmp->cnt();
            delete tmp;
        }
        ++ i;
    }
    return sum;
} // ibis::mesa::computeSum

/// Estimate the size of serialized version of the index.  The estimation
/// assumes the bitmap offsets are stored in 8-byte offsets.  The size is
/// measured in bytes.
size_t ibis::mesa::getSerialSize() const throw () {
    size_t res = (nobs << 5) + 16;
    for (unsigned j = 0; j < nobs; ++ j)
        if (bits[j] != 0)
            res += bits[j]->getSerialSize();
    return res;
} // ibis::mesa::getSerialSize
