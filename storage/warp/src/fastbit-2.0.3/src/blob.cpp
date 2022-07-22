//File: $Id$
// Author: John Wu <John.Wu at ACM.org>
// Copyright (c) 2009-2016 the Regents of the University of California
///@file
/// Define the class ibis::blob.
#include "blob.h"
#include "part.h"       // ibis::part

#include <iomanip>      // std::setprecision, std::setw

#define FASTBIT_SYNC_WRITE 1

/// Contruct a blob by reading from a metadata file.
ibis::blob::blob(const part *prt, FILE *file) : ibis::column(prt, file) {
}

/// Construct a blob from a name.
ibis::blob::blob(const part *prt, const char *nm)
    : ibis::column(prt, ibis::BLOB, nm) {
}

/// Copy an existing column object of type ibis::BLOB.
ibis::blob::blob(const ibis::column &c) : ibis::column(c) {
    if (m_type != ibis::BLOB)
        throw "can not construct an ibis::blob from another type";
}

/// Write metadata about the column.
void ibis::blob::write(FILE *fptr) const {
    fprintf(fptr, "\nBegin Column\nname = %s\ndescription = %s\ntype = blob\n"
            "End Column\n", m_name.c_str(),
            (m_desc.empty() ? m_name.c_str() : m_desc.c_str()));
} // ibis::blob::write

/// Print information about this column.
void ibis::blob::print(std::ostream& out) const {
    out << m_name << ": " << m_desc << " (BLOB)";
} // ibis::blob::print

/// Append the content in @c df to the end of files in @c dt.  It returns
/// the number of rows appended or a negative number to indicate error
/// conditions.
long ibis::blob::append(const char* dt, const char* df, const uint32_t nold,
                        const uint32_t nnew, uint32_t nbuf, char* buf) {
    if (nnew == 0 || dt == 0 || df == 0 || *dt == 0 || *df == 0 ||
        dt == df || std::strcmp(dt, df) == 0)
        return 0;
    std::string evt = "blob[";
    if (thePart != 0)
        evt += thePart->name();
    else
        evt += "?";
    evt += '.';
    evt += m_name;
    evt += "]::append";

    const char spelem = 8; // starting positions are 8-byte intergers
    writeLock lock(this, evt.c_str());
    std::string datadest, spdest;
    std::string datasrc, spfrom;
    datadest += dt;
    datadest += FASTBIT_DIRSEP;
    datadest += m_name;
    datasrc += df;
    datasrc += FASTBIT_DIRSEP;
    datasrc += m_name;
    spdest = datadest;
    spdest += ".sp";
    spfrom = datasrc;
    spfrom += ".sp";
    LOGGER(ibis::gVerbose > 3)
        << evt << " -- source \"" << datasrc << "\" --> destination \""
        << datadest << "\", nold=" << nold << ", nnew=" << nnew;

    // rely on .sp file for existing data size
    int sdest = UnixOpen(spdest.c_str(), OPEN_READWRITE, OPEN_FILEMODE);
    if (sdest < 0) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " failed to open file \"" << spdest
            << "\" for append ... "
            << (errno ? strerror(errno) : "no free stdio stream");
        return -2;
    }
    ibis::util::guard gsdest = ibis::util::makeGuard(UnixClose, sdest);
#if defined(_WIN32) && defined(_MSC_VER)
    (void)_setmode(sdest, _O_BINARY);
#endif

    // verify the existing sizes of data file and start positions match
    long sj = UnixSeek(sdest, 0, SEEK_END);
    if (sj < 0 || sj % spelem != 0) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " expects file " << spdest
            << " to have a multiple of " << spelem << " bytes, but it is "
            << sj << ", will not continue with corrupt data files";
        return -3;
    }
    off_t ierr;
    const uint32_t nsold = sj / spelem;
    const uint32_t nold0 = (nsold > 1 ? nsold-1 : 0);
    int64_t dfsize = 0;
    if (nsold == 0) {
        ierr = UnixWrite(sdest, &dfsize, spelem);
        if (ierr < (int)spelem) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << evt << " expects to write " << spelem
                << " to " << spdest << ", but the write function returned "
                << ierr;
            return -4;
        }
    }
    else if (nold0 < nold) {
        LOGGER(ibis::gVerbose > 1)
            << evt << " -- data file " << spdest << " is expected to have"
            << nold+1 << " entries, but found only " << nsold
            << ", attempt to extend the file with the last value in it";

        ierr = UnixSeek(sdest, -spelem, SEEK_END);
        if (ierr < (int) (sj - spelem)) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << evt << " failed to seek to position "
                << sj-spelem << " in file " << spdest;
            return -5;
        }
        ierr = UnixRead(sdest, &dfsize, spelem);
        if (ierr < (int)spelem) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << evt << " failed to read the last "
                << spelem << " bytes from " << spdest;
            return -6;
        }
        for (unsigned j = nold0; j < nold; ++ j) {
            ierr = UnixWrite(sdest, &dfsize, spelem);
            if (ierr < (int)spelem) {
                LOGGER(ibis::gVerbose > 0)
                    << "Warning -- " << evt << " failed to write the value "
                    << dfsize << " to the end of " << spdest;
                return -7;
            }
        }
    }
    else if (nold0 > nold) {
        LOGGER(ibis::gVerbose > 1)
            << evt << " -- data file " << spdest << " is expected to have "
            << nold+1 << " entries, but found " << nsold
            << ", the extra entries will be overwritten";

        ierr = UnixSeek(sdest, spelem*nold, SEEK_SET);
        if (ierr < (int) (spelem*nold)) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << evt << " failed to seek to " << spelem*nold
                << " in file " << spdest;
            return -8;
        }
        ierr = UnixRead(sdest, &dfsize, spelem);
        if (ierr < (int)spelem) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << evt << " failed to read " << spelem
                << " bytes from position " << nold*spelem << " in file "
                << spdest;
            return -9;
        }
    }

    // .sp file pointer should at at spelem*(nold+1)
    ierr = UnixSeek(sdest, 0, SEEK_CUR);
    if ((unsigned) ierr != spelem*(nold+1)) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " expects file pointer to be at "
            << spelem*(nold+1) << ", but it is actually at " << ierr;
        return -10;
    }

    int ssrc = UnixOpen(spfrom.c_str(), OPEN_READONLY);
    if (ssrc < 0) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " failed to open file " << spfrom
            << " for reading -- "
            << (errno ? strerror(errno) : "no free stdio stream");
        return -11;
    }
    ibis::util::guard gssrc = ibis::util::makeGuard(UnixClose, ssrc);
#if defined(_WIN32) && defined(_MSC_VER)
    (void)_setmode(ssrc, _O_BINARY);
#endif

    // a buffer object is always decleared, but may be only 1-byte in size
    ibis::fileManager::buffer<char> dbuff((nbuf != 0));
    if (nbuf == 0) {
        nbuf = dbuff.size();
        buf = dbuff.address();
    }
    if (nbuf <= (unsigned)spelem) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " can not continue because of "
            "insufficient amount of available buffer space";
        return -1;
    }
    if ((unsigned long)nold+nnew >= (unsigned long)(INT_MAX / spelem)) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " can not continue because the "
            "resulting .sp will be too large";
        return -1;
    }

    const uint32_t nspbuf = nbuf / spelem;
    uint64_t *spbuf = (uint64_t*) buf;
    int64_t dj = 0;
    uint32_t nnew0 = 0;
    for (uint32_t j = 0; j <= nnew; j += nspbuf) {
        ierr = UnixRead(ssrc, spbuf, nbuf);
        if (ierr <= 0) {
            LOGGER(ierr < 0 && ibis::gVerbose > 0)
                << "Warning -- " << evt << " failed to read from "
                << spfrom << ", function read returned " << ierr;
            break;
        }
        int iread = ierr;
        if (j == 0) {
            dj = dfsize - *spbuf;
            iread -= spelem;
            for (int i = 0; i < iread/spelem; ++ i)
                spbuf[i] = spbuf[i+1] + dj;
        }
        else {
            for (int i = 0; i < iread/spelem; ++ i)
                spbuf[i] += dj;
        }
        off_t iwrite = UnixWrite(sdest, spbuf, iread);
        if (iwrite < iread) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << evt << " expects to write " << iread
                << " byte" << (iread>1?"s":"") << ", but only wrote " << iwrite;
            return -12;
        }
        nnew0 = iwrite / spelem;
    }
    // explicit close the read source file to reduce the number of open files
    (void) UnixClose(ssrc);
    gssrc.dismiss();

#if defined(FASTBIT_SYNC_WRITE)
#if _POSIX_FSYNC+0 > 0
    (void) UnixFlush(sdest); // write to disk
#elif defined(_WIN32) && defined(_MSC_VER)
    (void) _commit(sdest);
#endif
#endif
    // close the destination .sp file just in case we need to truncate it
    UnixClose(sdest);
    gsdest.dismiss();
    if (sj > static_cast<long>(spelem*(nold+nnew0))) {
        LOGGER(ibis::gVerbose > 3)
            << evt << " truncating extra bytes in file " << spdest;
        ierr = truncate(spdest.c_str(), spelem*(nold+nnew0));
    }
    LOGGER(ibis::gVerbose > 4)
        << evt << " appended " << nnew0 << " element" << (nnew0>1?"s":"")
        << " from " << spfrom << " to " << spdest;

    // open destination data file
    int ddest = UnixOpen(datadest.c_str(), OPEN_APPENDONLY, OPEN_FILEMODE);
    if (ddest < 0) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " failed to open file \"" << datadest
            << "\" for append ... "
            << (errno ? strerror(errno) : "no free stdio stream");
        return -13;
    }
    // this statement guarantees UnixClose will be called on ddest upon
    // termination of this function
    ibis::util::guard gddest = ibis::util::makeGuard(UnixClose, ddest);
#if defined(_WIN32) && defined(_MSC_VER)
    (void)_setmode(ddest, _O_BINARY);
#endif
    dj = UnixSeek(ddest, 0, SEEK_END);
    if (dj != dfsize) {
        if (dj < dfsize) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << evt << " expects " << datadest
                << " to have " << dfsize << " byte" << (dfsize>1 ? "s" : "")
                << ", but it actually has " << dj;
            return -14;
        }
        else {
            dj = UnixSeek(ddest, dfsize, SEEK_SET);
            if (dj != dfsize) {
                LOGGER(ibis::gVerbose > 0)
                    << "Warning -- " << evt << " failed to seek to " << dfsize
                    << " in file " << datadest << ", function seek returned "
                    << dj;
                return -15;
            }
            else {
                LOGGER(ibis::gVerbose > 1)
                    << evt << " will overwrite the content after position "
                    << dfsize << " in file " << datadest;
            }
        }
    }

    int dsrc = UnixOpen(datasrc.c_str(), OPEN_READONLY);
    if (dsrc < 0) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " failed to open file \"" << datasrc
            << "\" for reading ... "
            << (errno ? strerror(errno) : "no free stdio stream");
        return -16;
    }
    ibis::util::guard gdsrc = ibis::util::makeGuard(UnixClose, dsrc);
#if defined(_WIN32) && defined(_MSC_VER)
    (void)_setmode(dsrc, _O_BINARY);
#endif
    while ((ierr = UnixRead(dsrc, buf, nbuf)) > 0) {
        int iwrite = UnixWrite(ddest, buf, ierr);
        LOGGER(ibis::gVerbose > 1 && iwrite < ierr)
            << "Warning -- " << evt << " expects to write " << ierr
            << " byte" << (ierr > 1 ? "s" : "") << ", but only wrote "
            << iwrite;
    }
#if defined(FASTBIT_SYNC_WRITE)
#if  _POSIX_FSYNC+0 > 0
    (void) UnixFlush(ddest); // write to disk
#elif defined(_WIN32) && defined(_MSC_VER)
    (void) _commit(ddest);
#endif
#endif
    (void) UnixClose(dsrc);
    gdsrc.dismiss();
    (void) UnixClose(ddest);
    gddest.dismiss();
    LOGGER(ibis::gVerbose > 4)
        << evt << " appended " << nnew0 << " row" << (nnew0>1?"s":"");

    //////////////////////////////////////////////////
    // deals with the masks
    std::string filename;
    filename = datasrc;
    filename += ".msk";
    ibis::bitvector mapp;
    try {mapp.read(filename.c_str());} catch (...) {/* ok to continue */}
    mapp.adjustSize(nnew0, nnew0);
    LOGGER(ibis::gVerbose > 7)
        << evt << " mask file \"" << filename << "\" contains "
        << mapp.cnt() << " set bits out of " << mapp.size()
        << " total bits";

    filename = datadest;
    filename += ".msk";
    ibis::bitvector mtot;
    try {mtot.read(filename.c_str());} catch (...) {/* ok to continue */}
    mtot.adjustSize(nold0, nold);
    LOGGER(ibis::gVerbose > 7)
        << evt << " mask file \"" << filename << "\" contains " << mtot.cnt()
        << " set bits out of " << mtot.size() << " total bits before append";

    mtot += mapp; // append the new ones at the end
    if (mtot.size() != nold+nnew0) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " expects the combined mask to have "
            << nold+nnew0 << " bits, but has " << mtot.size();
        mtot.adjustSize(nold+nnew0, nold+nnew0);
    }
    if (mtot.cnt() != mtot.size()) {
        mtot.write(filename.c_str());
        if (ibis::gVerbose > 6) {
            logMessage("append", "mask file \"%s\" indicates %lu valid "
                       "records out of %lu", filename.c_str(),
                       static_cast<long unsigned>(mtot.cnt()),
                       static_cast<long unsigned>(mtot.size()));
#if DEBUG+0 > 0 || _DEBUG+0 > 0
            LOGGER(ibis::gVerbose >= 0) << mtot;
#endif
        }
    }
    else {
        remove(filename.c_str()); // no need to have the file
        if (ibis::gVerbose > 6)
            logMessage("append", "mask file \"%s\" removed, all "
                       "%lu records are valid", filename.c_str(),
                       static_cast<long unsigned>(mtot.size()));
    }
    if (thePart == 0 || thePart->currentDataDir() == 0)
        return nnew0;
    if (std::strcmp(dt, thePart->currentDataDir()) == 0) {
        // update the mask stored internally
        mutexLock lck(this, "column::append");
        mask_.swap(mtot);
    }

    return nnew0;
} // ibis::blob::append

/// Write the content of BLOBs packed into two arrays va1 and va2.  All
/// BLOBs are packed together one after another in va1 and their starting
/// positions are stored in va2.  The last element of va2 is the total
/// number of bytes in va1.  The array va2 is expected to hold (nnew+1)
/// 64-bit integers.
///
/// @note The array va2 is modified in this function to have a starting
/// position that is the end of the existing data file.
long ibis::blob::writeData(const char* dir, uint32_t nold, uint32_t nnew,
                           ibis::bitvector& mask, const void *va1, void *va2) {
    if (nnew == 0 || va1 == 0 || va2 == 0 || dir == 0 || *dir == 0)
        return 0;

    std::string evt = "blob[";
    if (thePart != 0)
        evt += thePart->name();
    else
        evt += "?";
    evt += '.';
    evt += m_name;
    evt += "]::writeData";

    const char spelem = 8; // starting positions are 8-byte intergers
    int64_t *sparray = static_cast<int64_t*>(va2);
    int ierr;
    int64_t dfsize = 0;
    std::string datadest, spdest;
    datadest += dir;
    datadest += FASTBIT_DIRSEP;
    datadest += m_name;
    spdest = datadest;
    spdest += ".sp";
    LOGGER(ibis::gVerbose > 3)
        << evt << " starting to write " << nnew << " blob" << (nnew>1?"s":"")
        << " to \"" << datadest << "\", nold=" << nold;

    // rely on .sp file for existing data size
    int sdest = UnixOpen(spdest.c_str(), OPEN_READWRITE, OPEN_FILEMODE);
    if (sdest < 0) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " failed to open file \"" << spdest
            << "\" for append ... "
            << (errno ? strerror(errno) : "no free stdio stream");
        return -2;
    }
    ibis::util::guard gsdest = ibis::util::makeGuard(UnixClose, sdest);
#if defined(_WIN32) && defined(_MSC_VER)
    (void)_setmode(sdest, _O_BINARY);
#endif

    // make sure there are right number of start positions
    long sj = UnixSeek(sdest, 0, SEEK_END);
    if (sj < 0 || sj % spelem != 0) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " expects file " << spdest
            << " to have a multiple of " << spelem << " bytes, but it is "
            << sj << ", will not continue with corrupt data files";
        return -3;
    }
    const uint32_t nsold = sj / spelem;
    const uint32_t nold0 = (nsold > 1 ? nsold-1 : 0);
    if (nsold == 0) {
        ierr = UnixWrite(sdest, &dfsize, spelem);
        if (ierr < (int)spelem) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << evt << " expects to write " << spelem
                << " to " << spdest << ", but the write function returned "
                << ierr;
            return -4;
        }
    }
    else if (nold0 < nold) {
        LOGGER(ibis::gVerbose > 1)
            << evt << " -- data file " << spdest << " is expected to have"
            << nold+1 << " entries, but found only " << nsold
            << ", attempt to extend the file with the last value in it";

        ierr = UnixSeek(sdest, -spelem, SEEK_END);
        if (ierr < (int) (sj - spelem)) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << evt << " failed to seek to position "
                << sj-spelem << " in file " << spdest;
            return -5;
        }
        ierr = UnixRead(sdest, &dfsize, spelem);
        if (ierr < (int)spelem) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << evt << " failed to read the last "
                << spelem << " bytes from " << spdest;
            return -6;
        }
        for (unsigned j = nold0; j < nold; ++ j) {
            ierr = UnixWrite(sdest, &dfsize, spelem);
            if (ierr < (int)spelem) {
                LOGGER(ibis::gVerbose > 0)
                    << "Warning -- " << evt << " failed to write the value "
                    << dfsize << " to the end of " << spdest;
                return -7;
            }
        }
    }
    else if (nold0 > nold) {
        LOGGER(ibis::gVerbose > 1)
            << evt << " -- data file " << spdest << " is expected to have "
            << nold+1 << " entries, but found " << nsold
            << ", the extra entries will be overwritten";

        ierr = UnixSeek(sdest, spelem*nold, SEEK_SET);
        if (ierr < (int) (spelem*nold)) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << evt << " failed to seek to " << spelem*nold
                << " in file " << spdest;
            return -8;
        }
        ierr = UnixRead(sdest, &dfsize, spelem);
        if (ierr < (int)spelem) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << evt << " failed to read " << spelem
                << " bytes from position " << nold*spelem << " in file "
                << spdest;
            return -9;
        }
    }

    // .sp file pointer should at at spelem*(nold+1)
    ierr = UnixSeek(sdest, 0, SEEK_CUR);
    if ((unsigned) ierr != spelem*(nold+1)) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " expects file pointer to be at "
            << spelem*(nold+1) << ", but it is actually at " << ierr;
        return -10;
    }

    if (dfsize != *sparray) {
        int64_t offset = dfsize - *sparray;
        for (unsigned j = 0; j <= nnew; ++ j)
            sparray[j] += offset;
    }
    int64_t dj = UnixWrite(sdest, sparray+1, spelem*nnew);
#if defined(FASTBIT_SYNC_WRITE)
#if  _POSIX_FSYNC+0 > 0
    (void) UnixFlush(sdest); // write to disk
#elif defined(_WIN32) && defined(_MSC_VER)
    (void) _commit(sdest);
#endif
#endif
    if (dj < static_cast<long>(spelem*nnew)) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " expects to write " << spelem*nnew
            << " bytes to " << spdest << ", but the function write returned "
            << dj;
        return -11;
    }

    // close the destination .sp file just in case we need to truncate it
    UnixClose(sdest);
    gsdest.dismiss();
    if (sj > static_cast<long>(spelem*(nold+nnew))) {
        LOGGER(ibis::gVerbose > 3)
            << evt << " truncating extra bytes in file " << spdest;
        ierr = truncate(spdest.c_str(), spelem*(nold+nnew));
    }
    LOGGER(ibis::gVerbose > 4)
        << evt << " appended " << nnew << " element" << (nnew>1?"s":"")
        << " to " << spdest;

    // open destination data file
    int ddest = UnixOpen(datadest.c_str(), OPEN_APPENDONLY, OPEN_FILEMODE);
    if (ddest < 0) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " failed to open file \"" << datadest
            << "\" for append ... "
            << (errno ? strerror(errno) : "no free stdio stream");
        return -13;
    }
    // this statement guarantees UnixClose will be called on ddest upon
    // termination of this function
    IBIS_BLOCK_GUARD(UnixClose, ddest);
#if defined(_WIN32) && defined(_MSC_VER)
    (void)_setmode(ddest, _O_BINARY);
#endif
    dj = UnixSeek(ddest, 0, SEEK_END);
    if (dj != dfsize) {
        if (dj < dfsize) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << evt << " expects " << datadest
                << " to have " << dfsize << " byte" << (dfsize>1 ? "s" : "")
                << ", but it actually has " << dj;
            return -14;
        }
        else {
            dj = UnixSeek(ddest, dfsize, SEEK_SET);
            if (dj != dfsize) {
                LOGGER(ibis::gVerbose > 0)
                    << "Warning -- " << evt << " failed to seek to " << dfsize
                    << " in file " << datadest << ", function seek returned "
                    << dj;
                return -15;
            }
            else {
                LOGGER(ibis::gVerbose > 1)
                    << evt << " will overwrite the content after position "
                    << dfsize << " in file " << datadest;
            }
        }
    }

    dfsize = sparray[nnew] - *sparray;
    dj = UnixWrite(ddest, va1, dfsize);
#if defined(FASTBIT_SYNC_WRITE)
#if _POSIX_FSYNC+0 > 0
    (void) UnixFlush(ddest); // write to disk
#elif defined(_WIN32) && defined(_MSC_VER)
    (void) _commit(ddest);
#endif
#endif
    if (dj < dfsize) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " expects to write " << dfsize
            << " byte" << (dfsize>1?"s":"") << " to " << datadest
            << ", but the function write returned " << dj;
    }
    LOGGER(ibis::gVerbose > 4)
        << evt << " appended " << nnew << " row" << (nnew>1?"s":"");

    //////////////////////////////////////////////////
    // deals with the masks
    mask.adjustSize(nold0, nold);
    mask.adjustSize(nold+nnew, nold+nnew);

    return nnew;
} // ibis::blob::writeData

/// Extract the blobs from the rows marked 1 in the mask.  It returns a
/// vector of opaque objects and internally uses selectRawBytes.
///
/// A negative value will be returned in case of error.
std::vector<ibis::opaque>*
ibis::blob::selectOpaques(const ibis::bitvector& mask) const {
    if (mask.cnt() == 0)
        return new std::vector<ibis::opaque>;
    if (thePart == 0)
        return 0;
    if (mask.size() > thePart->nRows())
        return 0;

    const char* dir = thePart->currentDataDir();
    if (dir == 0 || *dir == 0)
        return 0;

    ibis::array_t<char> buffer;
    ibis::array_t<uint64_t> positions;
    int ierr = selectRawBytes(mask, buffer, positions);
    if (ierr < 0) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- blob[" << name() << "]::selectOpaques failed to "
            "read the underlying data";
        return 0;
    }

    // turn buffer into opaque objects
    std::vector<ibis::opaque> *res = new std::vector<ibis::opaque>;
    if (positions.size() < 2)
        return res;

    res->resize(positions.size()-1);
    for (size_t j = 0; j < positions.size()-1; ++ j) {
        (*res)[j].copy(buffer.begin()+positions[j],
                       positions[j+1]-positions[j]);
    }
    return res;
} // ibis::blob::selectOpaques

/// Count the number of bytes in the blobs selected by the mask.  This
/// function can be used to compute the memory requirement before actually
/// retrieving the blobs.
///
/// It returns a negative number in case of error.
long ibis::blob::countRawBytes(const ibis::bitvector& mask) const {
    if (mask.cnt() == 0)
        return 0;
    if (thePart == 0)
        return -1;
    if (mask.size() > thePart->nRows())
        return -2;

    const char* dir = thePart->currentDataDir();
    if (dir == 0 || *dir == 0)
        return -3;

    std::string spfile = dir;
    spfile += FASTBIT_DIRSEP;
    spfile += m_name;
    spfile += ".sp";
    array_t<int64_t> starts;
    long sum = 0;
    int ierr = ibis::fileManager::instance().getFile(spfile.c_str(), starts);
    if (ierr >= 0) {
        if (starts.size() <= thePart->nRows())
            starts.clear();
    }
    else {
        starts.clear();
    }

    if (starts.size() > mask.size()) { // start positions are usable
        for (ibis::bitvector::indexSet ix = mask.firstIndexSet();
             ix.nIndices() > 0; ++ ix) {
            const ibis::bitvector::word_t *idx = ix.indices();
            if (ix.isRange()) {
                sum += starts[idx[1]] - starts[*idx];
            }
            else {
                for (unsigned jdx = 0; jdx < ix.nIndices(); ++ jdx) {
                    sum += starts[idx[jdx]+1] - starts[idx[jdx]];
                }
            }
        }
    }
    else { // have to open the .sp file to read the starting positions
        int fsp = UnixOpen(spfile.c_str(), OPEN_READONLY);
        if (fsp < 0) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- blob::countRawBytes failed to open file "
                << spfile << " for reading ... "
                << (errno ? strerror(errno) : "no free stdio stream");
            return -4;
        }
        IBIS_BLOCK_GUARD(UnixClose, fsp);
#if defined(_WIN32) && defined(_MSC_VER)
        (void)_setmode(fsp, _O_BINARY);
#endif

        const char spelem = 8;
        long pos;
        for (ibis::bitvector::indexSet ix = mask.firstIndexSet();
             ix.nIndices() > 0; ++ ix) {
            const ibis::bitvector::word_t *idx = ix.indices();
            if (ix.isRange()) {
                int64_t start, end;
                pos = *idx * spelem;
                ierr = UnixSeek(fsp, pos, SEEK_SET);
                if (ierr != pos) {
                    LOGGER(ibis::gVerbose > 0)
                        << "Warning -- blob::countRawBytes failed to seek to "
                        << pos << " in " << spfile;
                    return -5;
                }
                ierr = UnixRead(fsp, &start, spelem);
                if (ierr < spelem) {
                    LOGGER(ibis::gVerbose > 0)
                        << "Warning -- blob::countRawBytes failed to read "
                        << spelem << " bytes from position " << pos
                        << " in " << spfile;
                    return -6;
                }
                pos = idx[1] * spelem; 
                ierr = UnixSeek(fsp, pos, SEEK_SET);
                if (ierr != pos) {
                    LOGGER(ibis::gVerbose > 0)
                        << "Warning -- blob::countRawBytes failed to seek to "
                        << pos << " in " << spfile;
                    return -7;
                }
                ierr = UnixRead(fsp, &end, spelem);
                if (ierr < spelem) {
                    LOGGER(ibis::gVerbose > 0)
                        << "Warning -- blob::countRawBytes failed to read "
                        << spelem << " bytes from position " << pos
                        << " in " << spfile;
                    return -8;
                }
                sum += (end - start);
            }
            else {
                int64_t buf[2];
                for (unsigned jdx = 0; jdx < ix.nIndices(); ++ jdx) {
                    pos = idx[jdx] * spelem;
                    ierr = UnixSeek(fsp, pos, SEEK_SET);
                    if (ierr != pos) {
                        LOGGER(ibis::gVerbose > 0)
                            << "Warning -- blob::countRawBytes failed to "
                            "seek to" << pos << " in " << spfile;
                        return -9;
                    }
                    ierr = UnixRead(fsp, buf, sizeof(buf));
                    if (ierr < (int)sizeof(buf)) {
                        LOGGER(ibis::gVerbose > 0)
                            << "Warning -- blob::countRawBytes failed to"
                            " read " << sizeof(buf) << " bytes from position "
                            << pos << " in " << spfile;
                        return -10;
                    }
                    sum += (buf[1] - buf[0]);
                }
            }
        }
    }
    return sum;
} // ibis::blob::countRawBytes

/// Extract the blobs from the rows marked 1 in the mask.  Upon successful
/// completion, the buffer will contain all the raw bytes packed together,
/// positions will contain the starting positions of each blobs, and the
/// return value will be the number of blobs retrieved.  Even though the
/// positions are 64-bit integers, because the buffer has to fit in memory,
/// it is not possible to retrieve very large objects this way.  The number
/// of bytes in buffer is limited to be less than half of the free memory
/// available and this limite is hardcoded into this function.  To
/// determine how much memory would be needed by the buffer to full
/// retrieve all blobs marked 1, use function ibis::blob::countRawBytes.
///
/// A negative value will be returned in case of error.
int ibis::blob::selectRawBytes(const ibis::bitvector& mask,
                               ibis::array_t<char>& buffer,
                               ibis::array_t<uint64_t>& positions) const {
    buffer.clear();
    positions.clear();
    if (mask.cnt() == 0)
        return 0;
    if (thePart == 0)
        return -1;
    if (mask.size() > thePart->nRows())
        return -2;

    const char* dir = thePart->currentDataDir();
    if (dir == 0 || *dir == 0)
        return -3;

    std::string datafile = dir;
    datafile += FASTBIT_DIRSEP;
    datafile += m_name;
    std::string spfile = datafile;
    spfile += ".sp";

    // we intend for buffer to not use more than bufferlimit number of bytes.
    const int64_t bufferlimit = buffer.capacity() +
        (ibis::fileManager::bytesFree() >> 1);
    array_t<int64_t> starts;
    int ierr = ibis::fileManager::instance().getFile(spfile.c_str(), starts);
    if (ierr >= 0) {
        if (starts.size() <= thePart->nRows())
            starts.clear();
    }
    else {
        starts.clear();
    }

    try {
        uint32_t sum = 0;
        positions.reserve(mask.size()+1);
        if (starts.size() > mask.size()) { // array starts usable
            // first determine the size of buffer
            bool smll = true;
            for (ibis::bitvector::indexSet ix = mask.firstIndexSet();
                 ix.nIndices() > 0 && smll; ++ ix) {
                const ibis::bitvector::word_t *idx = ix.indices();
                if (ix.isRange()) {
                    if (sum + (starts[idx[1]] - starts[*idx]) <= bufferlimit) {
                        sum += (starts[idx[1]] - starts[*idx]);
                    }
                    else {
                        for (unsigned jdx = *idx; smll && jdx < idx[1];
                             ++ jdx) {
                            if (sum + (starts[jdx+1] - starts[jdx]) <=
                                bufferlimit)
                                sum += (starts[jdx+1] - starts[jdx]);
                            else
                                smll = false;
                        }
                    }
                }
                else {
                    for (unsigned jdx = 0; jdx < ix.nIndices() && smll;
                         ++ jdx) {
                        if (sum + (starts[idx[jdx]+1] - starts[idx[jdx]]) <=
                            bufferlimit)
                            sum += starts[idx[jdx]+1] - starts[idx[jdx]];
                        else
                            smll = false;
                    }
                }
            }

            // reserve space for buffer
            buffer.reserve(sum);
            // attempt to put all bytes of datafile into an array_t
            array_t<char> raw;
            ierr = ibis::fileManager::instance().getFile(datafile.c_str(), raw);
            if (ierr < 0) {
                raw.clear();
                LOGGER(ibis::gVerbose > 3)
                    << "blob::countRawBytes getFile(" << datafile
                    << ") returned " << ierr << ", will explicit read the file";
            }
            else if (raw.size() < (unsigned) starts.back()) {
                raw.clear();
                LOGGER(ibis::gVerbose > 3)
                    << "blob::countRawBytes getFile(" << datafile
                    << " returned an array with " << raw.size()
                    << " bytes, but " << starts.back()
                    << " are expected, will try explicitly reading the file";
            }
            if (smll) {
                if (raw.size() >= (unsigned) starts.back())
                    ierr = extractAll(mask, buffer, positions, raw, starts);
                else
                    ierr = extractAll(mask, buffer, positions,
                                      datafile.c_str(), starts);
            }
            else {
                if (raw.size() >= (unsigned) starts.back())
                    ierr = extractSome(mask, buffer, positions, raw, starts,
                                       sum);
                else
                    ierr = extractSome(mask, buffer, positions,
                                       datafile.c_str(), starts, sum);
            }
        }
        else { // have to open the .sp file to read the starting positions
            buffer.reserve(bufferlimit);
            ierr = extractSome(mask, buffer, positions, datafile.c_str(),
                               spfile.c_str(), bufferlimit);
        }
    }
    catch (const std::exception& e) {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- blob::selectRawBytes (" << datafile
            << ") terminating due to a std::exception -- " << e.what();
        ierr = -4;
    }
    catch (const char* s) {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- blob::selectRawBytes (" << datafile
            << ") terminating due to a string exception -- " << s;
        ierr = -5;
    }
    catch (...) {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- blob::selectRawBytes (" << datafile
            << ") terminating due to a unknown exception";
        ierr = -6;
    }

    if (ierr >= 0)
        ierr = (positions.size() > 1 ? positions.size() - 1 : 0);
    return ierr;
} // ibis::blob::selectRawBytes

/// Extract entries marked 1 in mask from raw to buffer.  Fill positions to
/// indicate the start and end positions of each raw binary object.  Caller
/// has determined that there is sufficient amount of space to perform this
/// operations and have reserved enough space for buffer.  Even though that
/// may not be a guarantee, we proceed as if it is.
int ibis::blob::extractAll(const ibis::bitvector& mask,
                           ibis::array_t<char>& buffer,
                           ibis::array_t<uint64_t>& positions,
                           const ibis::array_t<char>& raw,
                           const ibis::array_t<int64_t>& starts) const {
    positions.resize(1);
    positions[0] = 0;
    for (ibis::bitvector::indexSet ix = mask.firstIndexSet();
         ix.nIndices() > 0; ++ ix) {
        const ibis::bitvector::word_t *ids = ix.indices();
        if (ix.isRange()) {
            buffer.insert(buffer.end(), raw.begin()+starts[*ids],
                          raw.begin()+starts[ids[1]]);
            for (unsigned j = *ids; j < ids[1]; ++ j) {
                positions.push_back(positions.back()+(starts[j+1]-starts[j]));
            }
        }
        else {
            for (unsigned j = 0; j < ix.nIndices(); ++ j) {
                buffer.insert(buffer.end(), raw.begin()+starts[ids[j]],
                              raw.begin()+starts[1+ids[j]]);
                positions.push_back(positions.back() +
                                    (starts[1+ids[j]]-starts[ids[j]]));
            }
        }
    }
    return (starts.size()-1);
} // ibis::blob::extractAll

/// Extract entries marked 1 in mask from raw to buffer subject to a limit
/// on the buffer size.  Fill positions to indicate the start and end
/// positions of each raw binary object.  Caller has determined that there
/// is the amount of space to perform this operations and have reserved
/// enough space for buffer.  Even though that may not be a guarantee, we
/// proceed as if it is.
int ibis::blob::extractSome(const ibis::bitvector& mask,
                            ibis::array_t<char>& buffer,
                            ibis::array_t<uint64_t>& positions,
                            const ibis::array_t<char>& raw,
                            const ibis::array_t<int64_t>& starts,
                            const uint32_t limit) const {
    positions.resize(1);
    positions[0] = 0;
    for (ibis::bitvector::indexSet ix = mask.firstIndexSet();
         ix.nIndices() > 0 && buffer.size() < limit; ++ ix) {
        const ibis::bitvector::word_t *ids = ix.indices();
        if (ix.isRange()) {
            for (unsigned j = *ids; j < ids[1] && buffer.size() < limit; ++ j) {
                buffer.insert(buffer.end(), raw.begin()+starts[j],
                              raw.begin()+starts[j+1]);
                positions.push_back(positions.back()+(starts[j+1]-starts[j]));
            }
        }
        else {
            for (unsigned j = 0; j < ix.nIndices() && buffer.size() < limit;
                 ++ j) {
                buffer.insert(buffer.end(), raw.begin()+starts[ids[j]],
                              raw.begin()+starts[1+ids[j]]);
                positions.push_back(positions.back() +
                                    (starts[1+ids[j]]-starts[ids[j]]));
            }
        }
    }
    return (starts.size()-1);
} // ibis::blob::extractSome

/// Retrieve all binary objects marked 1 in the mask.  The caller has
/// reserved enough space for buffer and positions.  This function simply
/// needs to open rawfile and read the content into buffer.  It also
/// assigns values in starts to mark the boundaries of the binary objects.
int ibis::blob::extractAll(const ibis::bitvector& mask,
                           ibis::array_t<char>& buffer,
                           ibis::array_t<uint64_t>& positions,
                           const char* rawfile,
                           const ibis::array_t<int64_t>& starts) const {
    int fdes = UnixOpen(rawfile, OPEN_READONLY);
    if (fdes < 0) {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- blob::extractAll failed to open " << rawfile
            << " for reading ... "
            << (errno ? strerror(errno) : "no free stdio stream");
        return -11;
    }
    IBIS_BLOCK_GUARD(UnixClose, fdes);
#if defined(_WIN32) && defined(_MSC_VER)
    (void)_setmode(fdes, _O_BINARY);
#endif

    positions.resize(1);
    positions[0] = 0;
    int64_t ierr;
    for (ibis::bitvector::indexSet ix = mask.firstIndexSet();
         ix.nIndices() > 0; ++ ix) {
        const ibis::bitvector::word_t *ids = ix.indices();
        if (ix.isRange()) {
            ierr = UnixSeek(fdes, starts[*ids], SEEK_SET);
            if (ierr != starts[*ids]) {
                LOGGER(ibis::gVerbose >= 0)
                    << "Warning -- blob::extractAll failed to seek to position "
                    << starts[*ids] << " in " << rawfile
                    << " to retrieve record # " << *ids << " -- " << ids[1];
                return -12;
            }

            const int64_t bytes = starts[ids[1]] - starts[*ids];
            const uint32_t bsize = buffer.size();
            buffer.resize(bsize+bytes);
            ierr = UnixRead(fdes, buffer.begin()+bsize, bytes);
            if (ierr < bytes) {
                LOGGER(ibis::gVerbose >= 0)
                    << "Warning -- blob::extractAll expects to read " << bytes
                    << " byte" << (bytes>1?"s":"")
                    << ", but the read function returned " << ierr
                    << ", (reading started at " << starts[*ids]
                    << " in " << rawfile << ")";
                return -13;
            }
            for (unsigned j = *ids; j < ids[1]; ++ j) {
                positions.push_back(positions.back()+(starts[j+1]-starts[j]));
            }
        }
        else {
            for (unsigned j = 0; j < ix.nIndices(); ++ j) {
                int64_t curr = starts[ids[j]];
                ierr = UnixSeek(fdes, curr, SEEK_SET);
                if (ierr != curr) {
                    LOGGER(ibis::gVerbose >= 0)
                        << "Warning -- blob::extractAll failed to seek to "
                        << curr << " in " << rawfile
                        << " to retrieve record # " << ids[j];
                    return -14;
                }

                const int64_t bytes = starts[1+ids[j]] - starts[ids[j]];
                const uint32_t bsize = buffer.size();
                buffer.resize(bsize+bytes);
                ierr = UnixRead(fdes, buffer.begin()+bsize, bytes);
                if (ierr < bytes) {
                    LOGGER(ibis::gVerbose >= 0)
                        << "Warning -- blob::extractAll expects to read "
                        << bytes << " byte" << (bytes>1?"s":"")
                        << ", but the read function returned " << ierr
                        << ", (reading started at " << curr
                        << " in " << rawfile << ")";
                    return -15;
                }
                positions.push_back(positions.back() + bytes);
            }
        }
    }
    return (positions.size()-1);
} // ibis::blob::extractAll

/// Retrieve binary objects marked 1 in the mask subject to the specified
/// limit on buffer size.  The caller has reserved enough space for buffer
/// and positions.  This function simply needs to open rawfile and read the
/// content into buffer.  It also assigns values in starts to mark the
/// boundaries of the binary objects.
int ibis::blob::extractSome(const ibis::bitvector& mask,
                            ibis::array_t<char>& buffer,
                            ibis::array_t<uint64_t>& positions,
                            const char* rawfile,
                            const ibis::array_t<int64_t>& starts,
                            const uint32_t limit) const {
    int fdes = UnixOpen(rawfile, OPEN_READONLY);
    if (fdes < 0) {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- blob::extractSome failed to open " << rawfile
            << " for reading ... "
            << (errno ? strerror(errno) : "no free stdio stream");
        return -11;
    }
    IBIS_BLOCK_GUARD(UnixClose, fdes);
#if defined(_WIN32) && defined(_MSC_VER)
    (void)_setmode(fdes, _O_BINARY);
#endif

    int64_t ierr;
    positions.resize(1);
    positions[0] = 0;
    for (ibis::bitvector::indexSet ix = mask.firstIndexSet();
         ix.nIndices() > 0 && buffer.size() < limit; ++ ix) {
        const ibis::bitvector::word_t *ids = ix.indices();
        if (ix.isRange()) {
            ierr = UnixSeek(fdes, starts[*ids], SEEK_SET);
            if (ierr != starts[*ids]) {
                LOGGER(ibis::gVerbose >= 0)
                    << "Warning -- blob::extractSome failed to seek to "
                    << starts[*ids] << " in " << rawfile
                    << " to retrieve record # " << *ids << " -- " << ids[1];
                return -12;
            }

            for (unsigned j = *ids; j < ids[1] && buffer.size() < limit; ++ j) {
                const int64_t bytes = starts[j+1] - starts[j];
                const uint32_t bsize = buffer.size();
                buffer.resize(bsize+bytes);
                ierr = UnixRead(fdes, buffer.begin()+bsize, bytes);
                if (ierr < bytes) {
                    LOGGER(ibis::gVerbose >= 0)
                        << "Warning -- blob::extractSome expects to read "
                        << bytes << " byte" << (bytes>1?"s":"")
                        << ", but the read function returned " << ierr
                        << ", (reading started at " << starts[*ids]
                        << " in " << rawfile << ")";
                    return -13;
                }
                positions.push_back(bsize+bytes);
            }
        }
        else {
            for (unsigned j = 0; j < ix.nIndices() && buffer.size() < limit;
                 ++ j) {
                int64_t curr = starts[ids[j]];
                ierr = UnixSeek(fdes, curr, SEEK_SET);
                if (ierr != curr) {
                    LOGGER(ibis::gVerbose >= 0)
                        << "Warning -- blob::extractSome failed to seek to "
                        << curr << " in " << rawfile
                        << " to retrieve record # " << ids[j];
                    return -14;
                }

                const int64_t bytes = starts[1+ids[j]] - starts[ids[j]];
                const uint32_t bsize = buffer.size();
                buffer.resize(bsize+bytes);
                ierr = UnixRead(fdes, buffer.begin()+bsize, bytes);
                if (ierr < bytes) {
                    LOGGER(ibis::gVerbose >= 0)
                        << "Warning -- blob::extractSome expects to read "
                        << bytes << " byte" << (bytes>1?"s":"")
                        << ", but the read function returned " << ierr
                        << ", (reading started at " << curr
                        << " in " << rawfile << ")";
                    return -15;
                }
                positions.push_back(positions.back() + bytes);
            }
        }
    }
    return (positions.size()-1);
} // ibis::blob::extractSome

/// Retrieve binary objects marked 1 in the mask subject to the specified
/// limit on buffer size.  The caller has reserved enough space for buffer
/// and positions.  This function needs to open both rawfile and spfile.
/// It reads starting positions in spfile to determine where to read the
/// content from rawfile into buffer.  It also assigns values in starts to
/// mark the boundaries of the binary objects in buffer.
int ibis::blob::extractSome(const ibis::bitvector& mask,
                            ibis::array_t<char>& buffer,
                            ibis::array_t<uint64_t>& positions,
                            const char* rawfile,
                            const char* spfile,
                            const uint32_t limit) const {
    // sdes - for spfile
    int sdes = UnixOpen(spfile, OPEN_READONLY);
    if (sdes < 0) {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- blob::extractSome failed to open " << spfile
            << " for reading ... "
            << (errno ? strerror(errno) : "no free stdio stream");
        return -11;
    }
    IBIS_BLOCK_GUARD(UnixClose, sdes);
#if defined(_WIN32) && defined(_MSC_VER)
    (void)_setmode(sdes, _O_BINARY);
#endif

    // rdes - for rawfile
    int rdes = UnixOpen(rawfile, OPEN_READONLY);
    if (rdes < 0) {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- blob::extractSome failed to open " << rawfile
            << " for reading ... "
            << (errno ? strerror(errno) : "no free stdio stream");
        return -12;
    }
    IBIS_BLOCK_GUARD(UnixClose, rdes);
#if defined(_WIN32) && defined(_MSC_VER)
    (void)_setmode(rdes, _O_BINARY);
#endif

    positions.resize(1);
    positions[0] = 0;
    int64_t ierr, stmp[2];
    for (ibis::bitvector::indexSet ix = mask.firstIndexSet();
         ix.nIndices() > 0; ++ ix) {
        const ibis::bitvector::word_t *ids = ix.indices();
        if (ix.isRange()) {
            stmp[0] = 8 * *ids;
            ierr = UnixSeek(sdes, stmp[0], SEEK_SET);
            if (ierr != stmp[0]) {
                LOGGER(ibis::gVerbose >= 0)
                    << "Warning -- blob::extractSome failed to seek to "
                    << stmp[0] << " in " << spfile
                    << " to retrieve the starting positions for blob " << *ids;
                return -13;
            }
            ierr = UnixRead(sdes, stmp, 16);
            if (ierr < 16) {
                LOGGER(ibis::gVerbose >= 0)
                    << "Warning -- blob::extractSome failed to read start "
                    "and end positions for blob " << *ids << " from " << spfile;
                return -14;
            }
            ierr = UnixSeek(rdes, stmp[0], SEEK_SET);
            if (ierr != stmp[0]) {
                LOGGER(ibis::gVerbose >= 0)
                    << "Warning -- blob::extractSome failed to seek to "
                    << stmp[0] << " in " << rawfile
                    << " to retrieve record # " << *ids << " -- " << ids[1];
                return -15;
            }

            for (unsigned j = *ids; j < ids[1]; ++ j) {
                const int64_t bytes = stmp[1] - stmp[0];
                const uint32_t bsize = buffer.size();
                if (bsize+bytes > limit)
                    return (positions.size()-1);

                buffer.resize(bsize+bytes);
                ierr = UnixRead(rdes, buffer.begin()+bsize, bytes);
                if (ierr < bytes) {
                    LOGGER(ibis::gVerbose >= 0)
                        << "Warning -- blob::extractSome expects to read "
                        << bytes << " byte" << (bytes>1?"s":"") << " from "
                        << rawfile << ", but the read function returned "
                        << ierr;
                    return -16;
                }
                positions.push_back(bsize+bytes);

                if (j+1 < ids[1]) { // read next end positions
                    stmp[0] = stmp[1];
                    ierr = UnixRead(sdes, stmp+1, 8);
                    if (ierr < 8) {
                        LOGGER(ibis::gVerbose >= 0)
                            << "Warning -- blob::extractSome failed to read "
                            "the ending position of blob " << j+1 << " from "
                            << spfile;
                        return -17;
                    }
                }
            }
        }
        else {
            for (unsigned j = 0; j < ix.nIndices(); ++ j) {
                stmp[0] = 8 * ids[j];
                ierr = UnixSeek(sdes, stmp[0], SEEK_SET);
                if (ierr != stmp[0]) {
                    LOGGER(ibis::gVerbose >= 0)
                        << "Warning -- blob::extractSome failed to seek to "
                        << stmp[0] << " in " << spfile
                        << " to retrieve positions of blob " << ids[j];
                    return -18;
                }
                ierr = UnixRead(sdes, stmp, 16);
                if (ierr < 16) {
                    LOGGER(ibis::gVerbose >= 0)
                        << "Warning -- blob::extractSome failed to read "
                        "start and end positions of blob " << ids[j]
                        << " from " << spfile;
                    return -19;
                }
                const int64_t bytes = stmp[1] - stmp[0];
                const uint32_t bsize = buffer.size();
                if (bsize+bytes > limit) {
                    return (positions.size()-1);
                }

                ierr = UnixSeek(rdes, stmp[0], SEEK_SET);
                if (ierr != stmp[0]) {
                    LOGGER(ibis::gVerbose >= 0)
                        << "Warning -- blob::extractSome failed to seek to "
                        << stmp[0] << " in " << rawfile
                        << " to retrieve blob " << ids[j];
                    return -20;
                }

                buffer.resize(bsize+bytes);
                ierr = UnixRead(rdes, buffer.begin()+bsize, bytes);
                if (ierr < bytes) {
                    LOGGER(ibis::gVerbose >= 0)
                        << "Warning -- blob::extractSome expects to read "
                        << bytes << " byte" << (bytes>1?"s":"")
                        << ", but the read function returned " << ierr
                        << ", (reading started at " << stmp[0]
                        << " in " << rawfile << ")";
                    return -21;
                }
                positions.push_back(positions.back() + bytes);
            }
        }
    }
    return (positions.size()-1);
} // ibis::blob::extractSome

int ibis::blob::getOpaque(uint32_t ind, ibis::opaque &opq) const {
    char *buf = 0;
    uint64_t sz = 0;
    int ierr = getBlob(ind, buf, sz);
    if (ierr >= 0)
        opq.assign(buf, sz);
    return ierr;
} // ibis::blob::getOpaque

/// Extract a single binary object.  This function is only defined for
/// ibis::blob, therefore the caller must explicitly cast a column* to
/// blob*.  It needs to access two files, a file for start positions and
/// another for raw binary data.  Thus it has a large startup cost
/// associated with opening the files and seeking to the right places on
/// disk.  If there is enough memory available, it will attempt to make
/// these files available for later invocations of this function by making
/// their content available through array_t objects.  If it fails to create
/// the desired array_t objects, it will fall back to use explicit I/O
/// function calls.
int ibis::blob::getBlob(uint32_t ind, char *&buf, uint64_t &size)
    const {
    if (thePart == 0) return -1;
    if (ind > thePart->nRows()) return -2;
    const char* dir = thePart->currentDataDir();
    if (dir == 0 || *dir == 0)
        return -3;

    std::string datafile = dir;
    datafile += FASTBIT_DIRSEP;
    datafile += m_name;
    std::string spfile = datafile;
    spfile += ".sp";
    array_t<int64_t> starts;
    int ierr = ibis::fileManager::instance().getFile(spfile.c_str(), starts);
    if (ierr >= 0) {
        if (starts.size() <= thePart->nRows())
            starts.clear();
    }
    else {
        starts.clear();
    }

    if (starts.size() > thePart->nRows()) {
        if (starts[ind+1] <= starts[ind]) {
            size = 0;
            return 0;
        }

        uint64_t diff = starts[ind+1]-starts[ind];
        if (buf == 0 || size < diff) {
            delete buf;
            buf = new char[diff];
        }
        size = diff;

        array_t<char> bytes;
        ierr = ibis::fileManager::instance().getFile(datafile.c_str(), bytes);
        if (ierr >= 0) {
            if (bytes.size() >= (size_t)starts[ind+1]) {
                std::copy(bytes.begin()+starts[ind],
                          bytes.begin()+starts[ind+1], buf);
            }
            else {
                ierr = readBlob(ind, buf, size, starts, datafile.c_str());
            }
        }
        else {
            ierr = readBlob(ind, buf, size, starts, datafile.c_str());
        }
    }
    else {
        ierr = readBlob(ind, buf, size, spfile.c_str(), datafile.c_str());
    }
    return ierr;
} // ibis::blob::getBlob

/// Read a single binary object.  The starting position is available in an
/// array_t object.  It only needs to explicitly open the data file to
/// read.
int ibis::blob::readBlob(uint32_t ind, char *&buf, uint64_t &size,
                         const array_t<int64_t> &starts, const char *datafile)
    const {
    if (starts[ind+1] <= starts[ind]) {
        size = 0;
        return 0;
    }
    uint64_t diff = starts[ind+1]-starts[ind];
    if (buf == 0 || size < diff) {
        delete buf;
        buf = new char[diff];
    }
    if (buf == 0)
        return -10;

    int fdes = UnixOpen(datafile, OPEN_READONLY);
    if (fdes < 0) {
        LOGGER(ibis::gVerbose > 1)
            << "Warning -- blob::readBlob failed to open " << datafile
            << " for reading ... "
            << (errno ? strerror(errno) : "no free stdio stream");
        return -11;
    }
    IBIS_BLOCK_GUARD(UnixClose, fdes);
#if defined(_WIN32) && defined(_MSC_VER)
    (void)_setmode(fdes, _O_BINARY);
#endif

    off_t ierr = UnixSeek(fdes, starts[ind], SEEK_SET);
    if (ierr != starts[ind]) {
        LOGGER(ibis::gVerbose > 1)
            << "Warning -- blob::readBlob(" << ind << ") failed to seek to "
            << starts[ind] << " in " << datafile << ", seek returned "
            << ierr;
        return -12;
    }

    ierr = UnixRead(fdes, buf, diff);
    if (ierr < (off_t)diff) {
        LOGGER(ibis::gVerbose > 1)
            << "Warning -- blob::readBlob(" << ind << ") failed to read "
            << diff << " byte" << (diff>1?"s":"") << " from " << datafile
            << ", read returned " << ierr;
        return -13;
    }
    size = diff;
    if (size == diff)
        ierr = 0;
    else
        ierr = -14;
    return ierr;
} // ibis::blob::readBlob

/// Read a single binary object.  This function opens both starting
/// position file and data file explicitly.
int ibis::blob::readBlob(uint32_t ind, char *&buf, uint64_t &size,
                         const char *spfile, const char *datafile) const {
    int64_t starts[2];
    const uint32_t spelem = 8;
    int sdes = UnixOpen(spfile, OPEN_READONLY);
    if (sdes < 0) {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- blob::readBlob failed to open " << spfile
            << " for reading ... "
            << (errno ? strerror(errno) : "no free stdio stream");
        return -15;
    }
    IBIS_BLOCK_GUARD(UnixClose, sdes);
#if defined(_WIN32) && defined(_MSC_VER)
    (void)_setmode(sdes, _O_BINARY);
#endif
    off_t ierr = UnixSeek(sdes, ind*spelem, SEEK_SET);
    if (ierr != (off_t)(ind*spelem)) {
        LOGGER(ibis::gVerbose > 1)
            << "Warning -- blob::readBlob(" << ind << ") failed to seek to "
            << ind*spelem << " in " << spfile << ", seek returned " << ierr;
        return -16;
    }
    ierr = UnixRead(sdes, starts, sizeof(starts));
    if (ierr < (off_t)sizeof(starts)) {
        LOGGER(ibis::gVerbose > 1)
            << "Warning -- blob::readBlob(" << ind << ") failed to read "
            << sizeof(starts) << " bytes from " << ind*spelem << " in "
            << spfile << ", read returned " << ierr;
        return -17;
    }

    if (starts[1] <= starts[0]) {
        size = 0;
        return 0;
    }
    uint64_t diff = starts[1]-starts[0];
    if (buf == 0 || size < diff) {
        delete buf;
        buf = new char[diff];
    }
    if (buf == 0)
        return -10;

    int fdes = UnixOpen(datafile, OPEN_READONLY);
    if (fdes < 0) {
        LOGGER(ibis::gVerbose > 1)
            << "Warning -- blob::readBlob failed to open " << datafile
            << " for reading ... "
            << (errno ? strerror(errno) : "no free stdio stream");
        return -11;
    }
    IBIS_BLOCK_GUARD(UnixClose, fdes);
#if defined(_WIN32) && defined(_MSC_VER)
    (void)_setmode(fdes, _O_BINARY);
#endif

    ierr = UnixSeek(fdes, *starts, SEEK_SET);
    if (ierr != *starts) {
        LOGGER(ibis::gVerbose > 1)
            << "Warning -- blob::readBlob(" << ind << ") failed to seek to "
            << *starts << " in " << datafile << ", seek returned "
            << ierr;
        return -12;
    }

    ierr = UnixRead(fdes, buf, diff);
    if (ierr < (off_t)diff) {
        LOGGER(ibis::gVerbose > 1)
            << "Warning -- blob::readBlob(" << ind << ") failed to read "
            << diff << " byte" << (diff>1?"s":"") << " from " << datafile
            << ", read returned " << ierr;
        return -13;
    }
    size = diff;
    if (size == diff)
        ierr = 0;
    else
        ierr = -14;
    return ierr;
} // ibis::blob::readBlob

/// Copy the byte array into this opaque object.  Do not change the
/// incoming arguments.  The caller is still responsible for freeing the
/// pointer ptr.
///
/// It returns 0 upon successful completion of the copy operation,
/// otherwise, it returns a negative number to indicate error.  In case of
/// error, the existing content is not changed.
int ibis::opaque::copy(const void* ptr, uint64_t len) {
    if (len == 0 || ptr == 0) {
        delete [] buf_;
        buf_ = 0;
        len_ = 0;
        return 0;
    }

    try {
        char *tmp = new char[len];
        if (tmp == 0) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- opaque::copy failed to allocate " << len
                << " character" << (len>1?"s":"");
            return -2;
        }

        (void) memcpy(tmp, ptr, len);
        delete [] buf_;
        buf_ = tmp;
        len_ = len;
        return 0;
    }
    catch (...) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- opaque::copy failed to allocate " << len
            << " character" << (len>1?"s":"");
        return -1;
    }
} // ibis::opaque::copy

/// Print an opaque object to an output stream.
std::ostream& operator<<(std::ostream& out, const ibis::opaque& opq) {
    const char *buf = opq.address();
    if (buf == 0) {
        out << "    (empty binary object)";
        return out;
    }

    if (opq.size() > 3) {
        out << "0x" << std::setprecision(2) << std::setw(2) << std::setfill('0')
            << std::hex << static_cast<short>(buf[0])
            << std::setprecision(2) << std::setw(2) << std::setfill('0')
            << static_cast<short>(buf[1])
            << std::setprecision(2) << std::setw(2) << std::setfill('0')
            << static_cast<short>(buf[2])
            << std::setprecision(2) << std::setw(2) << std::setfill('0')
            << static_cast<short>(buf[3]) << std::dec;
        if (opq.size() > 4) {
            out << "... (" << opq.size()-4 << " skipped)";
        }
    }
    else if (opq.size() == 3) {
        out << "0x" << std::setprecision(2) << std::setw(2) << std::setfill('0')
            << std::hex << static_cast<short>(buf[0])
            << std::setprecision(2) << std::setw(2) << std::setfill('0')
            << static_cast<short>(buf[1])
            << std::setprecision(2) << std::setw(2) << std::setfill('0')
            << static_cast<short>(buf[2]) << std::dec;
    }
    else if (opq.size() == 2) {
        out << "0x" << std::setprecision(2) << std::setw(2) << std::setfill('0')
            << std::hex << static_cast<short>(buf[0])
            << std::setprecision(2) << std::setw(2) << std::setfill('0')
            << static_cast<short>(buf[1]) << std::dec;
    }
    else if (opq.size() == 1) {
        out << "0x" << std::setprecision(2) << std::setw(2) << std::setfill('0')
            << std::hex << static_cast<short>(buf[0]) << std::dec;
    }
    else {
        out << "    (empty binary object)";
    }
    return out;
}
