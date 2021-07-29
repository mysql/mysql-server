// $Id$
// Author: John Wu <John.Wu at ACM.org>
// Copyright (c) 2000-2016 the Regents of the University of California
//
// This file contains the implementation of ibis::roster -- an list of indices
// that orders the column values in ascending order.
//
#if defined(_WIN32) && defined(_MSC_VER)
#pragma warning(disable:4786)   // some identifier longer than 256 characters
#endif
#include "part.h"
#include "column.h"
#include "iroster.h"
#include "utilidor.h"

#include <sstream>      // std::ostringstream
#include <algorithm>    // std::sort
#include <typeinfo>     // typeid

////////////////////////////////////////////////////////////////////////
// functions from ibis::iroster
//
/// Construct a roster list.  It attempts to read a roster list from the
/// specified directory.  If a roster list can not be read and dir is not
/// nil, this function will attempt to sort the existing data records to
/// build a roster list.
ibis::roster::roster(const ibis::column* c, const char* dir)
    : col(c), inddes(-1) {
    if (c == 0) return;  // nothing can be done
    (void) read(dir); // attempt to read the existing list

    if (ind.size() == 0 && inddes < 0) {
        // need to build a new roster list
        if (col->partition() == 0 || col->partition()->nRows() <
            ibis::fileManager::bytesFree() / (8+col->elementSize()))
            icSort(dir);        // in core sorting
        if (ind.size() == 0 && col->partition() != 0)
            oocSort(dir);       // out of core sorting
    }

    if (ibis::gVerbose > 6 && (ind.size() > 0 || inddes >= 0)) {
        ibis::util::logger lg;
        print(lg());
    }
} // constructor

/// Reconstruct from content of a @c fileManager::storage.
/// The content of the file (following the 8-byte header) is
/// the index array @c ind.
ibis::roster::roster(const ibis::column* c,
                     ibis::fileManager::storage* st,
                     uint32_t offset)
    : col(c), ind(st, offset, offset+sizeof(uint32_t)*c->partition()->nRows()),
      inddes(-1) {
    if (ibis::gVerbose > 6) {
        ibis::util::logger lg;
        print(lg());
    }
}

/// Write both .ind and .srt file.  The argument can be the name of the
/// ouput directory, then column name will be added.  If the last segment
/// of the name (before the last directory separator) matches the file name
/// of the column, it is assumed to be the data file name and only the
/// extension .ind and .srt will be added.
int ibis::roster::write(const char* df) const {
    if (ind.empty()) return -1;

    std::string fnm, evt;
    evt = "roster";
    if (col != 0 && ibis::gVerbose > 1) {
        evt += '[';
        evt += col->fullname();
        evt += ']';
    }
    evt += "::write";
    if (ibis::gVerbose > 1 && df != 0) {
        evt += '(';
        evt += df;
        evt += ')';
    }
    if (df == 0) {
        fnm = col->partition()->currentDataDir();
        fnm += FASTBIT_DIRSEP;
    }
    else {
        fnm = df;
        uint32_t pos = fnm.rfind(FASTBIT_DIRSEP);
        if (pos >= fnm.size()) pos = 0;
        else ++ pos;
        if (std::strcmp(fnm.c_str()+pos, col->name()) != 0)
            fnm += FASTBIT_DIRSEP;
    }
    off_t ierr = fnm.size();
    if (fnm[ierr-1] == FASTBIT_DIRSEP)
        fnm += col->name();
    ierr = fnm.size();
    if (fnm[ierr-4] != '.' || fnm[ierr-3] != 'i' ||
        fnm[ierr-2] != 'n' || fnm[ierr-1] != 'd')
        fnm += ".ind";

    int fdes = UnixOpen(fnm.c_str(), OPEN_WRITENEW, OPEN_FILEMODE);
    if (fdes < 0) {
        ibis::fileManager::instance().flushFile(fnm.c_str());
        fdes = UnixOpen(fnm.c_str(), OPEN_WRITENEW, OPEN_FILEMODE);
        if (fdes < 0) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << evt << " failed to open \"" << fnm
                << "\" for write ... "
                << (errno ? strerror(errno) : "no free stdio stream");
            return -2;
        }
    }
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

    ierr = UnixWrite(fdes, reinterpret_cast<const void*>(ind.begin()),
                     sizeof(uint32_t)*ind.size());
    LOGGER(ierr != sizeof(uint32_t)*ind.size() && ibis::gVerbose > 0)
        << "Warning -- " << evt << " expected to write "
        << sizeof(uint32_t)*ind.size()
        << " bytes but only wrote " << ierr;
    (void) UnixClose(fdes);

    return writeSorted(df);
} // ibis::roster::write

/// Write the sorted values into .srt file.  Attempt to read the whole
/// column into memory first.  If it fails to do so, it will read one value
/// at a time from the original data file.
int ibis::roster::writeSorted(const char *df) const {
    if (ind.empty() || col == 0 || col->partition() == 0) return -1;

    std::string fnm;
    if (df == 0) {
        fnm = col->partition()->currentDataDir();
        fnm += FASTBIT_DIRSEP;
    }
    else {
        fnm = df;
        uint32_t pos = fnm.rfind(FASTBIT_DIRSEP);
        if (pos >= fnm.size()) pos = 0;
        else ++ pos;
        if (std::strcmp(fnm.c_str()+pos, col->name()) != 0)
            fnm += FASTBIT_DIRSEP;
    }
    uint32_t ierr = fnm.size();
    if (fnm[ierr-1] == FASTBIT_DIRSEP)
        fnm += col->name();
    ierr = fnm.size();
    if (fnm[ierr-4] == '.' && fnm[ierr-3] == 'i' &&
        fnm[ierr-2] == 'n' && fnm[ierr-1] == 'd') {
        fnm[ierr-3] = 's';
        fnm[ierr-2] = 'r';
        fnm[ierr-1] = 't';
    }
    else if (fnm[ierr-4] != '.' || fnm[ierr-3] != 's' ||
             fnm[ierr-2] != 'r' || fnm[ierr-1] != 't') {
        fnm += ".srt";
    }

    if (ibis::util::getFileSize(fnm.c_str()) ==
        (off_t)(col->elementSize()*ind.size()))
        return 0;

    std::string evt;
    if (ibis::gVerbose > 1) {
        evt = "roster[";
        evt += col->fullname();
        evt += "]::writeSorted";
    }
    else {
        evt = "roster::writeSorted";
    }
    FILE *fptr = fopen(fnm.c_str(), "wb");
    if (fptr == 0) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- roster::writeSorted failed to fopen " << fnm
            << " for writing";
        return -3;
    }

    // data file name share most characters with .srt file
    fnm.erase(fnm.size()-4);
    switch (col->type()) {
    case ibis::UBYTE: {
        array_t<unsigned char> arr;
        ierr = ibis::fileManager::instance().getFile(fnm.c_str(), arr);
        if (ierr == 0) {
            for (uint32_t i = 0; i < ind.size(); ++ i) {
                fwrite(&(arr[ind[i]]), sizeof(unsigned char), 1, fptr);
            }
        }
        else {
            unsigned char tmp;
            FILE *fpts = fopen(fnm.c_str(), "rb");
            if (fpts != 0) {
                for (uint32_t i = 0; i < ind.size(); ++ i) {
                    ierr = fseek(fpts, sizeof(tmp)*ind[i], SEEK_SET);
                    ierr = fread(&tmp, sizeof(tmp), 1, fpts);
                    if (ierr > 0) {
                        ierr = fwrite(&tmp, sizeof(tmp), 1, fptr);
                        LOGGER(ierr < sizeof(tmp) && ibis::gVerbose >= 0)
                            << "Warning -- " << evt
                            << " failed to write value # " << i << " (" << tmp
                            << ") to " << fnm;
                    }
                    else {
                        LOGGER(ibis::gVerbose >= 0)
                            << "Warning -- " << evt
                            << " failed to read value # " << i
                            << " (ind[" << i << "]=" << ind[i] << ")";
                    }
                }
                ierr = 0;
            }
        }
        break;}
    case ibis::BYTE: {
        array_t<signed char> arr;
        ierr = ibis::fileManager::instance().getFile(fnm.c_str(), arr);
        if (ierr == 0) {
            for (uint32_t i = 0; i < ind.size(); ++ i) {
                fwrite(&(arr[ind[i]]), sizeof(char), 1, fptr);
            }
        }
        else {
            char tmp;
            FILE *fpts = fopen(fnm.c_str(), "rb");
            if (fpts != 0) {
                for (uint32_t i = 0; i < ind.size(); ++ i) {
                    ierr = fseek(fpts, sizeof(tmp)*ind[i], SEEK_SET);
                    ierr = fread(&tmp, sizeof(tmp), 1, fpts);
                    if (ierr > 0) {
                        ierr = fwrite(&tmp, sizeof(tmp), 1, fptr);
                        LOGGER (ierr < sizeof(tmp) && ibis::gVerbose >= 0)
                            << "Warning -- " << evt
                            << " failed to write value # " << i << " (" << tmp
                            << ") to " << fnm;
                    }
                    else {
                        LOGGER(ibis::gVerbose >= 0)
                            << "Warning -- " << evt
                            << " failed to read value # " << i
                            << " (ind[" << i << "]=" << ind[i] << ")";
                    }
                }
                ierr = 0;
            }
        }
        break;}
    case ibis::USHORT: {
        array_t<uint16_t> arr;
        ierr = ibis::fileManager::instance().getFile(fnm.c_str(), arr);
        if (ierr == 0) {
            for (uint32_t i = 0; i < ind.size(); ++ i) {
                fwrite(&(arr[ind[i]]), sizeof(uint16_t), 1, fptr);
            }
        }
        else {
            uint16_t tmp;
            FILE *fpts = fopen(fnm.c_str(), "rb");
            if (fpts != 0) {
                for (uint32_t i = 0; i < ind.size(); ++ i) {
                    ierr = fseek(fpts, sizeof(tmp)*ind[i], SEEK_SET);
                    ierr = fread(&tmp, sizeof(tmp), 1, fpts);
                    if (ierr > 0) {
                        ierr = fwrite(&tmp, sizeof(tmp), 1, fptr);
                        LOGGER(ierr < sizeof(tmp) && ibis::gVerbose >= 0)
                            << "Warning -- " << evt
                            << " failed to write value # " << i << " (" << tmp
                            << ") to " << fnm;
                    }
                    else {
                        LOGGER(ibis::gVerbose >= 0)
                            << "Warning -- " << evt
                            << " failed to read value # " << i
                            << " (ind[" << i << "]=" << ind[i] << ")";
                    }
                }
                ierr = 0;
            }
        }
        break;}
    case ibis::SHORT: {
        array_t<int16_t> arr;
        ierr = ibis::fileManager::instance().getFile(fnm.c_str(), arr);
        if (ierr == 0) {
            for (uint32_t i = 0; i < ind.size(); ++ i) {
                fwrite(&(arr[ind[i]]), sizeof(int16_t), 1, fptr);
            }
        }
        else {
            int16_t tmp;
            FILE *fpts = fopen(fnm.c_str(), "rb");
            if (fpts != 0) {
                for (uint32_t i = 0; i < ind.size(); ++ i) {
                    ierr = fseek(fpts, sizeof(tmp)*ind[i], SEEK_SET);
                    ierr = fread(&tmp, sizeof(tmp), 1, fpts);
                    if (ierr > 0) {
                        ierr = fwrite(&tmp, sizeof(tmp), 1, fptr);
                        LOGGER (ierr < sizeof(tmp) && ibis::gVerbose >= 0)
                            << "Warning -- " << evt
                            << " failed to write value # " << i << " (" << tmp
                            << ") to " << fnm;
                    }
                    else {
                        LOGGER(ibis::gVerbose >= 0)
                            << "Warning -- " << evt
                            << " failed to read value # " << i
                            << " (ind[" << i << "]=" << ind[i] << ")";
                    }
                }
                ierr = 0;
            }
        }
        break;}
    case ibis::UINT: {
        array_t<uint32_t> arr;
        ierr = ibis::fileManager::instance().getFile(fnm.c_str(), arr);
        if (ierr == 0) {
            for (uint32_t i = 0; i < ind.size(); ++ i) {
                fwrite(&(arr[ind[i]]), sizeof(uint32_t), 1, fptr);
            }
        }
        else {
            uint32_t tmp;
            FILE *fpts = fopen(fnm.c_str(), "rb");
            if (fpts != 0) {
                for (uint32_t i = 0; i < ind.size(); ++ i) {
                    ierr = fseek(fpts, sizeof(tmp)*ind[i], SEEK_SET);
                    ierr = fread(&tmp, sizeof(tmp), 1, fpts);
                    if (ierr > 0) {
                        ierr = fwrite(&tmp, sizeof(tmp), 1, fptr);
                        LOGGER(ierr < sizeof(tmp) && ibis::gVerbose >= 0)
                            << "Warning -- " << evt
                            << " failed to write value # " << i << " (" << tmp
                            << ") to " << fnm;
                    }
                    else {
                        LOGGER(ibis::gVerbose >= 0)
                            << "Warning -- " << evt
                            << " failed to read value # " << i
                            << " (ind[" << i << "]=" << ind[i] << ")";
                    }
                }
                ierr = 0;
            }
        }
        break;}
    case ibis::INT: {
        array_t<int32_t> arr;
        ierr = ibis::fileManager::instance().getFile(fnm.c_str(), arr);
        if (ierr == 0) {
            for (uint32_t i = 0; i < ind.size(); ++ i) {
                fwrite(&(arr[ind[i]]), sizeof(int32_t), 1, fptr);
            }
        }
        else {
            int32_t tmp;
            FILE *fpts = fopen(fnm.c_str(), "rb");
            if (fpts != 0) {
                for (uint32_t i = 0; i < ind.size(); ++ i) {
                    ierr = fseek(fpts, sizeof(tmp)*ind[i], SEEK_SET);
                    ierr = fread(&tmp, sizeof(tmp), 1, fpts);
                    if (ierr > 0) {
                        ierr = fwrite(&tmp, sizeof(tmp), 1, fptr);
                        LOGGER (ierr < sizeof(tmp) && ibis::gVerbose >= 0)
                            << "Warning -- " << evt
                            << " failed to write value # " << i << " (" << tmp
                            << ") to " << fnm;
                    }
                    else {
                        LOGGER(ibis::gVerbose >= 0)
                            << "Warning -- " << evt
                            << " failed to read value # " << i
                            << " (ind[" << i << "]=" << ind[i] << ")";
                    }
                }
                ierr = 0;
            }
        }
        break;}
    case ibis::ULONG: {
        array_t<uint64_t> arr;
        ierr = ibis::fileManager::instance().getFile(fnm.c_str(), arr);
        if (ierr == 0) {
            for (uint32_t i = 0; i < ind.size(); ++ i) {
                fwrite(&(arr[ind[i]]), sizeof(uint64_t), 1, fptr);
            }
        }
        else {
            uint64_t tmp;
            FILE *fpts = fopen(fnm.c_str(), "rb");
            if (fpts != 0) {
                for (uint32_t i = 0; i < ind.size(); ++ i) {
                    ierr = fseek(fpts, sizeof(tmp)*ind[i], SEEK_SET);
                    ierr = fread(&tmp, sizeof(tmp), 1, fpts);
                    if (ierr > 0) {
                        ierr = fwrite(&tmp, sizeof(tmp), 1, fptr);
                        LOGGER(ierr < sizeof(tmp) && ibis::gVerbose >= 0)
                            << "Warning -- " << evt
                            << " failed to write value # " << i << " (" << tmp
                            << ") to " << fnm;
                    }
                    else {
                        LOGGER(ibis::gVerbose >= 0)
                            << "Warning -- " << evt
                            << " failed to read value # " << i
                            << " (ind[" << i << "]=" << ind[i] << ")";
                    }
                }
                ierr = 0;
            }
        }
        break;}
    case ibis::LONG: {
        array_t<int64_t> arr;
        ierr = ibis::fileManager::instance().getFile(fnm.c_str(), arr);
        if (ierr == 0) {
            for (uint32_t i = 0; i < ind.size(); ++ i) {
                fwrite(&(arr[ind[i]]), sizeof(int64_t), 1, fptr);
            }
        }
        else {
            int64_t tmp;
            FILE *fpts = fopen(fnm.c_str(), "rb");
            if (fpts != 0) {
                for (uint32_t i = 0; i < ind.size(); ++ i) {
                    ierr = fseek(fpts, sizeof(tmp)*ind[i], SEEK_SET);
                    ierr = fread(&tmp, sizeof(tmp), 1, fpts);
                    if (ierr > 0) {
                        ierr = fwrite(&tmp, sizeof(tmp), 1, fptr);
                        LOGGER (ierr < sizeof(tmp) && ibis::gVerbose >= 0)
                            << "Warning -- " << evt
                            << " failed to write value # " << i << " (" << tmp
                            << ") to " << fnm;
                    }
                    else {
                        LOGGER(ibis::gVerbose >= 0)
                            << "Warning -- " << evt
                            << " failed to read value # " << i
                            << " (ind[" << i << "]=" << ind[i] << ")";
                    }
                }
                ierr = 0;
            }
        }
        break;}
    case ibis::FLOAT: {
        array_t<float> arr;
        ierr = ibis::fileManager::instance().getFile(fnm.c_str(), arr);
        if (ierr == 0) {
            for (uint32_t i = 0; i < ind.size(); ++ i) {
                fwrite(&(arr[ind[i]]), sizeof(float), 1, fptr);
            }
        }
        else {
            float tmp;
            FILE *fpts = fopen(fnm.c_str(), "rb");
            if (fpts != 0) {
                for (uint32_t i = 0; i < ind.size(); ++ i) {
                    ierr = fseek(fpts, sizeof(float)*ind[i], SEEK_SET);
                    ierr = fread(&tmp, sizeof(float), 1, fpts);
                    if (ierr > 0) {
                        ierr = fwrite(&tmp, sizeof(float), 1, fptr);
                        LOGGER(ierr < sizeof(float) && ibis::gVerbose > 0)
                            << "Warning -- " << evt
                            << "failed to write value # " << i
                            << " (" << tmp << ") to " << fnm;
                    }
                    else {
                        LOGGER(ibis::gVerbose > 0)
                            << "Warning -- " << evt
                            << "failed to read value # " << i
                            << "(ind[" << i << "]=" << ind[i] << ")";
                    }
                }
                ierr = 0;
            }
        }
        break;}
    case ibis::DOUBLE: {
        array_t<double> arr;
        ierr = ibis::fileManager::instance().getFile(fnm.c_str(), arr);
        if (ierr == 0) {
            for (uint32_t i = 0; i < ind.size(); ++ i) {
                fwrite(&(arr[ind[i]]), sizeof(double), 1, fptr);
            }
        }
        else {
            double tmp;
            FILE *fpts = fopen(fnm.c_str(), "rb");
            if (fpts != 0) {
                for (uint32_t i = 0; i < ind.size(); ++ i) {
                    ierr = fseek(fpts, sizeof(double)*ind[i], SEEK_SET);
                    ierr = fread(&tmp, sizeof(double), 1, fpts);
                    if (ierr > 0) {
                        ierr = fwrite(&tmp, sizeof(double), 1, fptr);
                        LOGGER(ierr < sizeof(double) && ibis::gVerbose > 0)
                            << "Warning -- " << evt
                            << "failed to write value # " << i << " ("
                            << tmp << ") to " << fnm;
                    }
                    else {
                        LOGGER(ibis::gVerbose > 0)
                            << "Warning -- " << evt
                            << "failed to read value # " << i
                            << "(ind[" << i << "]=" << ind[i] << ")";
                    }
                }
                ierr = 0;
            }
        }
        break;}
    default: {
        const int t = static_cast<int>(col->type());
        LOGGER(ibis::gVerbose > 0)      
            << "Warning -- " << evt << " does not support column type "
            << ibis::TYPESTRING[t] << "(" << t << ")";
        ierr = 0;
        break;}
    } // switch (col->type())
    fclose(fptr); // close the .srt file

    if (ierr == 0) {
        return 0;
    }
    else {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " failed to open data file "
            << fnm << " for reading";
        return ierr;
    }
} // ibis::roster::writeSorted

/// Write the content of ind to a file already open.
int ibis::roster::write(FILE* fptr) const {
    if (ind.empty()) return -1;
    uint32_t ierr = fwrite(reinterpret_cast<const void*>(ind.begin()),
                           sizeof(uint32_t), ind.size(), fptr);
    if (ierr != ind.size()) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- roster::write expected to write " << ind.size()
            << " words but only wrote " << ierr;
        return -5;
    }
    else {
        return 0;
    }
} // ibis::roster::write

int ibis::roster::read(const char* idxf) {
    std::string fnm;
    if (idxf == 0) {
        if (col == 0 || col->partition() == 0) return -1;

        fnm = col->partition()->currentDataDir();
        fnm += FASTBIT_DIRSEP;
    }
    else {
        fnm = idxf;
        uint32_t pos = fnm.rfind(FASTBIT_DIRSEP);
        if (pos >= fnm.size()) pos = 0;
        else ++ pos;
        if (std::strcmp(fnm.c_str()+pos, col->name()) != 0)
            fnm += FASTBIT_DIRSEP;
    }
    long ierr = fnm.size();
    if (fnm[ierr-1] == FASTBIT_DIRSEP)
        fnm += col->name();
    ierr = fnm.size();
    if (fnm[ierr-4] != '.' || fnm[ierr-3] != 'i' ||
        fnm[ierr-2] != 'n' || fnm[ierr-1] != 'd') {
        if (fnm[ierr-4] == '.' &&
            (fnm[ierr-3] == 'i' || fnm[ierr-3] == 's') &&
            (fnm[ierr-2] == 'd' || fnm[ierr-2] == 'r') &&
            (fnm[ierr-1] == 'x' || fnm[ierr-1] == 't'))
            fnm.erase(ierr-4);
        fnm += ".ind";
    }

    uint32_t nbytes = sizeof(uint32_t)*col->partition()->nRows();
    if (ibis::util::getFileSize(fnm.c_str()) != (off_t)nbytes)
        return -2;

    if (nbytes < ibis::fileManager::bytesFree()) {
        ind.read(fnm.c_str());
        LOGGER(ibis::gVerbose > 4)
            << "roster -- read the content of " << fnm << " into memory";
    }
    else {
        inddes = UnixOpen(fnm.c_str(), OPEN_READONLY);
        if (inddes < 0) {
            LOGGER(ibis::gVerbose > 0) 
                << "Warning -- roster::read failed to open " << fnm;
        }
        else {
            LOGGER(ibis::gVerbose > 4) 
                << "roster::read successfully openned file " << fnm
                << " for future read operations";
        }
#if defined(_WIN32) && defined(_MSC_VER)
        (void)_setmode(inddes, _O_BINARY);
#endif
    }
    return 0;
} // ibis::roster::read

int ibis::roster::read(ibis::fileManager::storage* st) {
    if (st == 0) return -1;
    array_t<uint32_t> tmp(st, 0, sizeof(uint32_t)*col->partition()->nRows());
    ind.swap(tmp);
    return 0;
} // ibis::roster::read

/// The in-core sorting function.  Reads the content of the specified file
/// into memrory and sort the values through a simple stable sorting
/// procedure.
void ibis::roster::icSort(const char* fin) {
    long ierr;
    std::string fnm;
    LOGGER(col->dataFileName(fnm, fin) == 0 && ibis::gVerbose > 2)
        << "roster::icSort can not generate data file name";

    ibis::horometer timer;
    if (ibis::gVerbose > 1) {
        timer.start();
        LOGGER(ibis::gVerbose > 1)
            << "roster::icSort attempt to sort the content of file ("
            << fnm << ") in memory";
    }

    array_t<uint32_t> indim;
    switch (col->type()) {
    case ibis::UBYTE: { // unsigned char
        array_t<unsigned char> val;
        if (! fnm.empty()) 
            ierr = ibis::fileManager::instance().getFile(fnm.c_str(), val);
        else
            ierr = col->getValuesArray(&val);
        if (ierr >= 0 && val.size() > 0) {
            const_cast<const array_t<unsigned char>&>(val).stableSort(indim);
#if DEBUG+0 > 1 || _DEBUG+0 > 1
            unsigned char tmp;
            uint32_t i = 0, j = 0;
            ibis::util::logger lg(4);
            const uint32_t n = ind.size();
            lg() << "DEBUG -- roster::icSort -- value, starting position, "
                "count\n";
            while (i < n) {
                tmp = val[ind[i]];
                ++ j;
                while (j < n && tmp == val[ind[j]])
                    ++ j;
                lg() << tmp << "\t" << i << "\t" << j - i << "\n";
                i = j;
            }
#endif
        }
        break;
    }
    case ibis::BYTE: { // signed char
        array_t<signed char> val;
        if (! fnm.empty())
            ierr = ibis::fileManager::instance().getFile(fnm.c_str(), val);
        else
            ierr = col->getValuesArray(&val);
        if (ierr >= 0 && val.size() > 0) {
            const_cast<const array_t<signed char>&>(val).stableSort(indim);
#if DEBUG+0 > 1 || _DEBUG+0 > 1
            char tmp;
            uint32_t i = 0, j = 0;
            ibis::util::logger lg(4);
            const uint32_t n = ind.size();
            lg() << "DEBUG -- roster::icSort -- value, starting position, "
                "count\n";
            while (i < n) {
                tmp = val[ind[i]];
                ++ j;
                while (j < n && tmp == val[ind[j]])
                    ++ j;
                lg() << tmp << "\t" << i << "\t" << j - i << "\n";
                i = j;
            }
#endif
        }
        break;
    }
    case ibis::USHORT: { // unsigned short int
        array_t<uint16_t> val;
        if (! fnm.empty())
            ierr = ibis::fileManager::instance().getFile(fnm.c_str(), val);
        else
            ierr = col->getValuesArray(&val);
        if (ierr >= 0 && val.size() > 0) {
            const_cast<const array_t<uint16_t>&>(val).stableSort(indim);
#if DEBUG+0 > 1 || _DEBUG+0 > 1
            uint16_t tmp;
            uint32_t i = 0, j = 0;
            ibis::util::logger lg(4);
            const uint32_t n = ind.size();
            lg() << "DEBUG -- roster::icSort -- value, starting "
                "position, count\n";
            while (i < n) {
                tmp = val[ind[i]];
                ++ j;
                while (j < n && tmp == val[ind[j]])
                    ++ j;
                lg() << tmp << "\t" << i << "\t" << j - i << "\n";
                i = j;
            }
#endif
        }
        break;
    }
    case ibis::SHORT: { // signed short int
        array_t<int16_t> val;
        if (! fnm.empty())
            ierr = ibis::fileManager::instance().getFile(fnm.c_str(), val);
        else
            ierr = col->getValuesArray(&val);
        if (ierr >= 0 && val.size() > 0) {
            const_cast<const array_t<int16_t>&>(val).stableSort(indim);
#if DEBUG+0 > 1 || _DEBUG+0 > 1
            int16_t tmp;
            uint32_t i = 0, j = 0;
            ibis::util::logger lg(4);
            const uint32_t n = ind.size();
            lg() << "DEBUG -- roster::icSort -- value, starting position, "
                "count\n";
            while (i < n) {
                tmp = val[ind[i]];
                ++ j;
                while (j < n && tmp == val[ind[j]])
                    ++ j;
                lg() << tmp << "\t" << i << "\t" << j - i << "\n";
                i = j;
            }
#endif
        }
        break;
    }
    case ibis::UINT: { // unsigned int
        array_t<uint32_t> val;
        if (! fnm.empty())
            ierr = ibis::fileManager::instance().getFile(fnm.c_str(), val);
        else
            ierr = col->getValuesArray(&val);
        if (ierr >= 0 && val.size() > 0) {
            const_cast<const array_t<uint32_t>&>(val).stableSort(indim);
#if DEBUG+0 > 1 || _DEBUG+0 > 1
            uint32_t tmp;
            uint32_t i = 0, j = 0;
            ibis::util::logger lg(4);
            const uint32_t n = ind.size();
            lg() << "DEBUG -- roster::icSort -- value, starting position, "
                "count\n";
            while (i < n) {
                tmp = val[ind[i]];
                ++ j;
                while (j < n && tmp == val[ind[j]])
                    ++ j;
                lg() << tmp << "\t" << i << "\t" << j - i << "\n";
                i = j;
            }
#endif
        }
        break;
    }
    case ibis::INT: { // signed int
        array_t<int32_t> val;
        if (! fnm.empty())
            ierr = ibis::fileManager::instance().getFile(fnm.c_str(), val);
        else
            ierr = col->getValuesArray(&val);
        if (ierr >= 0 && val.size() > 0) {
            const_cast<const array_t<int32_t>&>(val).stableSort(indim);
#if DEBUG+0 > 1 || _DEBUG+0 > 1
            int32_t tmp;
            uint32_t i = 0, j = 0;
            ibis::util::logger lg(4);
            const uint32_t n = ind.size();
            lg() << "DEBUG -- roster::icSort -- value, starting position, "
                "count\n";
            while (i < n) {
                tmp = val[ind[i]];
                ++ j;
                while (j < n && tmp == val[ind[j]])
                    ++ j;
                lg() << tmp << "\t" << i << "\t" << j - i << "\n";
                i = j;
            }
#endif
        }
        break;
    }
    case ibis::ULONG: { // unsigned long int
        array_t<uint64_t> val;
        if (! fnm.empty())
            ierr = ibis::fileManager::instance().getFile(fnm.c_str(), val);
        else
            ierr = col->getValuesArray(&val);
        if (ierr >= 0 && val.size() > 0) {
            const_cast<const array_t<uint64_t>&>(val).stableSort(indim);
#if DEBUG+0 > 1 || _DEBUG+0 > 1
            uint64_t tmp;
            uint32_t i = 0, j = 0;
            ibis::util::logger lg(4);
            const uint32_t n = ind.size();
            lg() << "DEBUG -- roster::icSort -- value, starting position, "
                "count\n";
            while (i < n) {
                tmp = val[ind[i]];
                ++ j;
                while (j < n && tmp == val[ind[j]])
                    ++ j;
                lg() << tmp << "\t" << i << "\t" << j - i << "\n";
                i = j;
            }
#endif
        }
        break;
    }
    case ibis::LONG: { // signed long int
        array_t<int64_t> val;
        if (! fnm.empty())
            ierr = ibis::fileManager::instance().getFile(fnm.c_str(), val);
        else
            ierr = col->getValuesArray(&val);
        if (ierr >= 0 && val.size() > 0) {
            const_cast<const array_t<int64_t>&>(val).stableSort(indim);
#if DEBUG+0 > 1 || _DEBUG+0 > 1
            int64_t tmp;
            uint32_t i = 0, j = 0;
            ibis::util::logger lg(4);
            const uint32_t n = ind.size();
            lg() << "DEBUG  -- roster::icSort -- value, starting position, "
                "count\n";
            while (i < n) {
                tmp = val[ind[i]];
                ++ j;
                while (j < n && tmp == val[ind[j]])
                    ++ j;
                lg() << tmp << "\t" << i << "\t" << j - i << "\n";
                i = j;
            }
#endif
        }
        break;
    }
    case ibis::FLOAT: { // float
        array_t<float> val;
        if (! fnm.empty())
            ierr = ibis::fileManager::instance().getFile(fnm.c_str(), val);
        else
            ierr = col->getValuesArray(&val);
        if (ierr >= 0 && val.size() > 0) {
            const_cast<const array_t<float>&>(val).stableSort(indim);
#if DEBUG+0 > 1 || _DEBUG+0 > 1
            float tmp;
            uint32_t i = 0, j = 0;
            ibis::util::logger lg(4);
            const uint32_t n = ind.size();
            lg() << "DEBUG -- roster::icSort -- value, starting position, "
                "count\n";
            while (i < n) {
                tmp = val[ind[i]];
                ++ j;
                while (j < n && tmp == val[ind[j]])
                    ++ j;
                lg() << tmp << "\t" << i << "\t" << j - i << "\n";
                i = j;
            }
#endif
        }
        break;
    }
    case ibis::DOUBLE: { // double
        array_t<double> val;
        if (! fnm.empty())
            ierr = ibis::fileManager::instance().getFile(fnm.c_str(), val);
        else
            ierr = col->getValuesArray(&val);
        if (ierr >= 0 && val.size() > 0) {
            const_cast<const array_t<double>&>(val).stableSort(indim);
#if DEBUG+0 > 1 || _DEBUG+0 > 1
            double tmp;
            uint32_t i = 0, j = 0;
            ibis::util::logger lg(4);
            const uint32_t n = ind.size();
            lg() << "DEBUG -- roster::icSort -- value, starting position, "
                "count\n";
            while (i < n) {
                tmp = val[ind[i]];
                ++ j;
                while (j < n && tmp == val[ind[j]])
                    ++ j;
                lg() << tmp << "\t" << i << "\t" << j - i << "\n";
                i = j;
            }
#endif
        }
        break;}
    case ibis::CATEGORY: { // no need for a separate index
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- roster can not generate additional index";
        break;}
    default: {
        ibis::util::logger lg;
        lg() << "roster -- failed to create a roster list for ";
        col->print(lg());
        break;}
    }

    if (indim.size() == col->partition()->nRows()) {
        ind.swap(indim);
        // write out the current content
        write(static_cast<const char*>(0)); // write .ind file
    }
    if (ibis::gVerbose > 2) {
        timer.stop();
        LOGGER(ibis::gVerbose > 2)
            << "roster::icSort -- in-core sorting of " << ind.size()
            << " numbers from " << fnm << " took " << timer.realTime()
            << " sec(elapsed)";
    }
    if (ibis::gVerbose > 4 &&
        (ibis::gVerbose > 30 || ((1U<<ibis::gVerbose) > ind.size()))) {
        ibis::util::logger lg;
        print(lg());
    }
} // ibis::roster::icSort

/// The out-of-core sorting function.  Internally it uses four data files.
/// It eventully removes two of them and leaves only two with the extension
/// of @c .srt and @c .ind.  The two files have the same content as @c .ind
/// and @c .srt produced by the functions @c write and @c writeSorted.
void ibis::roster::oocSort(const char *fin) {
    if (ind.size() == col->partition()->nRows()) return;
    ind.clear(); // clear the index array.
    ibis::horometer timer;
    if (ibis::gVerbose > 1) {
        timer.start();
        LOGGER(ibis::gVerbose > 1)
            << "roster::oocSort attempt to sort the column " << col->name()
            << " out of core";
    }

    // nsrt is the name of the final sorted data file
    // nind is the name of the final index file
    // msrt is the intermediate sorted data file, will be removed later
    // mind is the intermediate index file, will be removed later
    std::string nsrt, nind, msrt, mind;
    if (fin == 0) {
        nind = col->partition()->currentDataDir();
        nind += FASTBIT_DIRSEP;
    }
    else {
        nind = fin;
        uint32_t pos = nind.rfind(FASTBIT_DIRSEP);
        if (pos >= nind.size()) pos = 0;
        else ++ pos;
        if (std::strcmp(nind.c_str()+pos, col->name()) != 0)
            nind += FASTBIT_DIRSEP;
    }
    long ierr = nind.size();
    if (nind[ierr-1] == FASTBIT_DIRSEP)
        nind += col->name();
    if (nind[ierr-4] != '.' || nind[ierr-3] != 'i' ||
        nind[ierr-2] != 'n' || nind[ierr-1] != 'd') {
        if (nind[ierr-4] == '.' &&
            (nind[ierr-3] == 'i' || nind[ierr-3] == 's') &&
            (nind[ierr-2] == 'd' || nind[ierr-2] == 'r') &&
            (nind[ierr-1] == 'x' || nind[ierr-1] == 't'))
            nind.erase(ierr-4);
        nind += ".ind";
    }
    const uint32_t nrows = col->partition()->nRows();
    if (ibis::util::getFileSize(nind.c_str()) ==
        (off_t)(sizeof(uint32_t) * nrows)) {
        // open the ind file in read only mode for future operaions.
        inddes = UnixOpen(nind.c_str(), OPEN_READONLY);
#if defined(_WIN32) && defined(_MSC_VER)
        (void)_setmode(inddes, _O_BINARY);
#endif
        return;
    }

    nsrt = nind;
    nsrt.erase(nsrt.size()-3);
    nsrt += "srt";
    std::string datafile = nind; // name of original data file
    datafile.erase(datafile.size()-4);

    mind = col->partition()->name();
    mind += ".cacheDirectory";
    const char *tmp = ibis::gParameters()[mind.c_str()];
    if (tmp != 0) {
        msrt = tmp;
        msrt += FASTBIT_DIRSEP;
        msrt += col->partition()->name();
        msrt += '.';
        msrt += col->name();
        mind = msrt;
        msrt += ".srt";
        mind += ".ind";
    }
    else {
        msrt = nsrt;
        mind = nind;
        msrt += "-tmp";
        mind += "-tmp";
    }
    // read 256K elements at a time
    const uint32_t mblock = PREFERRED_BLOCK_SIZE;
    array_t<uint32_t> ibuf1(mblock), ibuf2(mblock);

    ierr = nrows / mblock;
    const uint32_t nblock = ierr + (nrows > static_cast<uint32_t>(ierr)*mblock);
    ierr = 1;
    for (uint32_t i = nblock; i > 1; ++ierr, i>>=1);
    const bool isodd = (ierr%2 == 1);
    uint32_t stride = mblock;

    switch (col->type()) {
    case ibis::ULONG: {
        array_t<uint64_t> dbuf1(mblock), dbuf2(mblock);
        if (isodd) {
            ierr = oocSortBlocks(datafile.c_str(), nsrt.c_str(), nind.c_str(),
                                 mblock, dbuf1, dbuf2, ibuf1);
        }
        else {
            ierr = oocSortBlocks(datafile.c_str(), msrt.c_str(), mind.c_str(),
                                 mblock, dbuf1, dbuf2, ibuf1);
            if (ierr == 0)
                ierr = oocMergeBlocks(msrt.c_str(), nsrt.c_str(),
                                      mind.c_str(), nind.c_str(),
                                      mblock, stride, dbuf1, dbuf2,
                                      ibuf1, ibuf2);
            stride += stride;
        }
        while (ierr == 0 && stride < nrows) {
            ierr = oocMergeBlocks(nsrt.c_str(), msrt.c_str(),
                                  nind.c_str(), mind.c_str(),
                                  mblock, stride,
                                  dbuf1, dbuf2, ibuf1, ibuf2);
            if (ierr != 0) break;
            stride += stride;
            ierr = oocMergeBlocks(msrt.c_str(), nsrt.c_str(),
                                  mind.c_str(), nind.c_str(),
                                  mblock, stride,
                                  dbuf1, dbuf2, ibuf1, ibuf2);
            stride += stride;
        }
        break;}
    case ibis::LONG: {
        array_t<int64_t> dbuf1(mblock), dbuf2(mblock);
        if (isodd) {
            ierr = oocSortBlocks(datafile.c_str(), nsrt.c_str(), nind.c_str(),
                                 mblock, dbuf1, dbuf2, ibuf1);
        }
        else {
            ierr = oocSortBlocks(datafile.c_str(), msrt.c_str(), mind.c_str(),
                                 mblock, dbuf1, dbuf2, ibuf1);
            if (ierr == 0)
                ierr = oocMergeBlocks(msrt.c_str(), nsrt.c_str(),
                                      mind.c_str(), nind.c_str(),
                                      mblock, stride, dbuf1, dbuf2,
                                      ibuf1, ibuf2);
            stride += stride;
        }
        while (ierr == 0 && stride < nrows) {
            ierr = oocMergeBlocks(nsrt.c_str(), msrt.c_str(),
                                  nind.c_str(), mind.c_str(),
                                  mblock, stride,
                                  dbuf1, dbuf2, ibuf1, ibuf2);
            if (ierr != 0) break;
            stride += stride;
            ierr = oocMergeBlocks(msrt.c_str(), nsrt.c_str(),
                                  mind.c_str(), nind.c_str(),
                                  mblock, stride,
                                  dbuf1, dbuf2, ibuf1, ibuf2);
            stride += stride;
        }
        break;}
    case ibis::CATEGORY:
    case ibis::UINT: {
        array_t<uint32_t> dbuf1(mblock), dbuf2(mblock);
        if (isodd) {
            ierr = oocSortBlocks(datafile.c_str(), nsrt.c_str(), nind.c_str(),
                                 mblock, dbuf1, dbuf2, ibuf1);
        }
        else {
            ierr = oocSortBlocks(datafile.c_str(), msrt.c_str(), mind.c_str(),
                                 mblock, dbuf1, dbuf2, ibuf1);
            if (ierr == 0)
                ierr = oocMergeBlocks(msrt.c_str(), nsrt.c_str(),
                                      mind.c_str(), nind.c_str(),
                                      mblock, stride, dbuf1, dbuf2,
                                      ibuf1, ibuf2);
            stride += stride;
        }
        while (ierr == 0 && stride < nrows) {
            ierr = oocMergeBlocks(nsrt.c_str(), msrt.c_str(),
                                  nind.c_str(), mind.c_str(),
                                  mblock, stride,
                                  dbuf1, dbuf2, ibuf1, ibuf2);
            if (ierr != 0) break;
            stride += stride;
            ierr = oocMergeBlocks(msrt.c_str(), nsrt.c_str(),
                                  mind.c_str(), nind.c_str(),
                                  mblock, stride,
                                  dbuf1, dbuf2, ibuf1, ibuf2);
            stride += stride;
        }
        break;}
    case ibis::INT: {
        array_t<int32_t> dbuf1(mblock), dbuf2(mblock);
        if (isodd) {
            ierr = oocSortBlocks(datafile.c_str(), nsrt.c_str(), nind.c_str(),
                                 mblock, dbuf1, dbuf2, ibuf1);
        }
        else {
            ierr = oocSortBlocks(datafile.c_str(), msrt.c_str(), mind.c_str(),
                                 mblock, dbuf1, dbuf2, ibuf1);
            if (ierr == 0)
                ierr = oocMergeBlocks(msrt.c_str(), nsrt.c_str(),
                                      mind.c_str(), nind.c_str(),
                                      mblock, stride, dbuf1, dbuf2,
                                      ibuf1, ibuf2);
            stride += stride;
        }
        while (ierr == 0 && stride < nrows) {
            ierr = oocMergeBlocks(nsrt.c_str(), msrt.c_str(),
                                  nind.c_str(), mind.c_str(),
                                  mblock, stride,
                                  dbuf1, dbuf2, ibuf1, ibuf2);
            if (ierr != 0) break;
            stride += stride;
            ierr = oocMergeBlocks(msrt.c_str(), nsrt.c_str(),
                                  mind.c_str(), nind.c_str(),
                                  mblock, stride,
                                  dbuf1, dbuf2, ibuf1, ibuf2);
            stride += stride;
        }
        break;}
    case ibis::USHORT: {
        array_t<uint16_t> dbuf1(mblock), dbuf2(mblock);
        if (isodd) {
            ierr = oocSortBlocks(datafile.c_str(), nsrt.c_str(), nind.c_str(),
                                 mblock, dbuf1, dbuf2, ibuf1);
        }
        else {
            ierr = oocSortBlocks(datafile.c_str(), msrt.c_str(), mind.c_str(),
                                 mblock, dbuf1, dbuf2, ibuf1);
            if (ierr == 0)
                ierr = oocMergeBlocks(msrt.c_str(), nsrt.c_str(),
                                      mind.c_str(), nind.c_str(),
                                      mblock, stride, dbuf1, dbuf2,
                                      ibuf1, ibuf2);
            stride += stride;
        }
        while (ierr == 0 && stride < nrows) {
            ierr = oocMergeBlocks(nsrt.c_str(), msrt.c_str(),
                                  nind.c_str(), mind.c_str(),
                                  mblock, stride,
                                  dbuf1, dbuf2, ibuf1, ibuf2);
            if (ierr != 0) break;
            stride += stride;
            ierr = oocMergeBlocks(msrt.c_str(), nsrt.c_str(),
                                  mind.c_str(), nind.c_str(),
                                  mblock, stride,
                                  dbuf1, dbuf2, ibuf1, ibuf2);
            stride += stride;
        }
        break;}
    case ibis::SHORT: {
        array_t<int16_t> dbuf1(mblock), dbuf2(mblock);
        if (isodd) {
            ierr = oocSortBlocks(datafile.c_str(), nsrt.c_str(), nind.c_str(),
                                 mblock, dbuf1, dbuf2, ibuf1);
        }
        else {
            ierr = oocSortBlocks(datafile.c_str(), msrt.c_str(), mind.c_str(),
                                 mblock, dbuf1, dbuf2, ibuf1);
            if (ierr == 0)
                ierr = oocMergeBlocks(msrt.c_str(), nsrt.c_str(),
                                      mind.c_str(), nind.c_str(),
                                      mblock, stride, dbuf1, dbuf2,
                                      ibuf1, ibuf2);
            stride += stride;
        }
        while (ierr == 0 && stride < nrows) {
            ierr = oocMergeBlocks(nsrt.c_str(), msrt.c_str(),
                                  nind.c_str(), mind.c_str(),
                                  mblock, stride,
                                  dbuf1, dbuf2, ibuf1, ibuf2);
            if (ierr != 0) break;
            stride += stride;
            ierr = oocMergeBlocks(msrt.c_str(), nsrt.c_str(),
                                  mind.c_str(), nind.c_str(),
                                  mblock, stride,
                                  dbuf1, dbuf2, ibuf1, ibuf2);
            stride += stride;
        }
        break;}
    case ibis::UBYTE: {
        array_t<unsigned char> dbuf1(mblock), dbuf2(mblock);
        if (isodd) {
            ierr = oocSortBlocks(datafile.c_str(), nsrt.c_str(), nind.c_str(),
                                 mblock, dbuf1, dbuf2, ibuf1);
        }
        else {
            ierr = oocSortBlocks(datafile.c_str(), msrt.c_str(), mind.c_str(),
                                 mblock, dbuf1, dbuf2, ibuf1);
            if (ierr == 0)
                ierr = oocMergeBlocks(msrt.c_str(), nsrt.c_str(),
                                      mind.c_str(), nind.c_str(),
                                      mblock, stride, dbuf1, dbuf2,
                                      ibuf1, ibuf2);
            stride += stride;
        }
        while (ierr == 0 && stride < nrows) {
            ierr = oocMergeBlocks(nsrt.c_str(), msrt.c_str(),
                                  nind.c_str(), mind.c_str(),
                                  mblock, stride,
                                  dbuf1, dbuf2, ibuf1, ibuf2);
            if (ierr != 0) break;
            stride += stride;
            ierr = oocMergeBlocks(msrt.c_str(), nsrt.c_str(),
                                  mind.c_str(), nind.c_str(),
                                  mblock, stride,
                                  dbuf1, dbuf2, ibuf1, ibuf2);
            stride += stride;
        }
        break;}
    case ibis::BYTE: {
        array_t<signed char> dbuf1(mblock), dbuf2(mblock);
        if (isodd) {
            ierr = oocSortBlocks(datafile.c_str(), nsrt.c_str(), nind.c_str(),
                                 mblock, dbuf1, dbuf2, ibuf1);
        }
        else {
            ierr = oocSortBlocks(datafile.c_str(), msrt.c_str(), mind.c_str(),
                                 mblock, dbuf1, dbuf2, ibuf1);
            if (ierr == 0)
                ierr = oocMergeBlocks(msrt.c_str(), nsrt.c_str(),
                                      mind.c_str(), nind.c_str(),
                                      mblock, stride, dbuf1, dbuf2,
                                      ibuf1, ibuf2);
            stride += stride;
        }
        while (ierr == 0 && stride < nrows) {
            ierr = oocMergeBlocks(nsrt.c_str(), msrt.c_str(),
                                  nind.c_str(), mind.c_str(),
                                  mblock, stride,
                                  dbuf1, dbuf2, ibuf1, ibuf2);
            if (ierr != 0) break;
            stride += stride;
            ierr = oocMergeBlocks(msrt.c_str(), nsrt.c_str(),
                                  mind.c_str(), nind.c_str(),
                                  mblock, stride,
                                  dbuf1, dbuf2, ibuf1, ibuf2);
            stride += stride;
        }
        break;}
    case ibis::FLOAT: {
        array_t<float> dbuf1(mblock), dbuf2(mblock);
        if (isodd) {
            ierr = oocSortBlocks(datafile.c_str(), nsrt.c_str(), nind.c_str(),
                                 mblock, dbuf1, dbuf2, ibuf1);
        }
        else {
            ierr = oocSortBlocks(datafile.c_str(), msrt.c_str(), mind.c_str(),
                                 mblock, dbuf1, dbuf2, ibuf1);
            if (ierr == 0)
                ierr = oocMergeBlocks(msrt.c_str(), nsrt.c_str(),
                                      mind.c_str(), nind.c_str(),
                                      mblock, stride, dbuf1, dbuf2,
                                      ibuf1, ibuf2);
            stride += stride;
        }
        while (ierr == 0 && stride < nrows) {
            ierr = oocMergeBlocks(nsrt.c_str(), msrt.c_str(),
                                  nind.c_str(), mind.c_str(),
                                  mblock, stride,
                                  dbuf1, dbuf2, ibuf1, ibuf2);
            if (ierr != 0) break;
            stride += stride;
            ierr = oocMergeBlocks(msrt.c_str(), nsrt.c_str(),
                                  mind.c_str(), nind.c_str(),
                                  mblock, stride,
                                  dbuf1, dbuf2, ibuf1, ibuf2);
            stride += stride;
        }
        break;}
    case ibis::DOUBLE: {
        array_t<double> dbuf1(mblock), dbuf2(mblock);
        if (isodd) {
            ierr = oocSortBlocks(datafile.c_str(), nsrt.c_str(), nind.c_str(),
                                 mblock, dbuf1, dbuf2, ibuf1);
        }
        else {
            ierr = oocSortBlocks(datafile.c_str(), msrt.c_str(), mind.c_str(),
                                 mblock, dbuf1, dbuf2, ibuf1);
            if (ierr == 0)
                ierr = oocMergeBlocks(msrt.c_str(), nsrt.c_str(),
                                      mind.c_str(), nind.c_str(),
                                      mblock, stride, dbuf1, dbuf2,
                                      ibuf1, ibuf2);
            stride += stride;
        }
        while (ierr == 0 && stride < nrows) {
            ierr = oocMergeBlocks(nsrt.c_str(), msrt.c_str(),
                                  nind.c_str(), mind.c_str(),
                                  mblock, stride,
                                  dbuf1, dbuf2, ibuf1, ibuf2);
            if (ierr != 0) break;
            stride += stride;
            ierr = oocMergeBlocks(msrt.c_str(), nsrt.c_str(),
                                  mind.c_str(), nind.c_str(),
                                  mblock, stride,
                                  dbuf1, dbuf2, ibuf1, ibuf2);
            stride += stride;
        }
        break;}
    default: {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- roster::oocSort can not process column type "
            << static_cast<int>(col->type());
        break;}
    }

    remove(msrt.c_str());
    remove(mind.c_str());
    if (ierr < 0) {
        remove(nsrt.c_str());
        remove(nind.c_str());
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- roster::oocSort failed to complete the "
            "out-of-core sorting of " << datafile << ". ierr = "
            << ierr << ". all output files removed";
        return;
    }
    else if (ibis::gVerbose > 2) {
        timer.stop();
        LOGGER(ibis::gVerbose > 2)
            << "roster::oocSort out-of-core sorting (" << datafile << " -> "
            << nsrt << " (" << nind << ")) took " << timer.realTime()
            << " sec(elapsed)";
    }
    if (ibis::gVerbose > 4 &&
        (ibis::gVerbose > 30 || ((1U<<ibis::gVerbose) > ind.size()))) {
        ibis::util::logger lg;
        print(lg());
    }

    // open the ind file in read only mode for future operaions.
    inddes = UnixOpen(nind.c_str(), OPEN_READONLY);
#if defined(_WIN32) && defined(_MSC_VER)
    (void)_setmode(inddes, _O_BINARY);
#endif
} // ibis::roster::oocSort

/// Read the content of file @c src one block at a time, sort each block
/// and write it to file @c dest.  At the same time produce an index array
/// and write it to file @c ind.  The block size is determined by @c mblock.
template <typename T>
long ibis::roster::oocSortBlocks(const char *src, const char *dest,
                                 const char *ind, const uint32_t mblock,
                                 array_t<T>& dbuf1, array_t<T>& dbuf2,
                                 array_t<uint32_t>& ibuf) const {
    int fdsrc = UnixOpen(src, OPEN_READONLY);
    if (fdsrc < 0) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- oocSortBlocks failed to open " << src
            << " for reading";
        return -1;
    }
#if defined(_WIN32) && defined(_MSC_VER)
    (void)_setmode(fdsrc, _O_BINARY);
#endif
    IBIS_BLOCK_GUARD(UnixClose, fdsrc);
    int fddes = UnixOpen(dest, OPEN_WRITENEW, OPEN_FILEMODE);
    if (fddes < 0) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- oocSortBlocks failed to open " << dest
            << " for writing";
        return -2;
    }
#if defined(_WIN32) && defined(_MSC_VER)
    (void)_setmode(fddes, _O_BINARY);
#endif
    IBIS_BLOCK_GUARD(UnixClose, fddes);
    int fdind = UnixOpen(ind, OPEN_WRITENEW, OPEN_FILEMODE);
    if (fdind < 0) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- oocSortBlocks failed to open " << ind
            << " for writing";
        return -3;
    }
#if defined(_WIN32) && defined(_MSC_VER)
    (void)_setmode(fdind, _O_BINARY);
#endif
    IBIS_BLOCK_GUARD(UnixClose, fdind);

    const uint32_t szi = sizeof(uint32_t);
    const uint32_t szd = sizeof(T);
    const uint32_t nrows = col->partition()->nRows();
    ibis::horometer timer;
    timer.start();
    ibuf.resize(mblock);
    dbuf1.resize(mblock);
    dbuf2.resize(mblock);
    long ierr = 0;
    for (uint32_t i = 0; ierr == 0 && i < nrows; i += mblock) {
        LOGGER(ibis::gVerbose > 12)
            << "roster::oocSortBlocks -- sorting block " << i;

        const uint32_t block = (i+mblock <= nrows ? mblock : nrows-i);
        ierr = dbuf1.read(fdsrc, i*szd, (i+block)*szd);
        if (static_cast<uint32_t>(ierr) != block*szd) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- oocSortBlocks expected to read " << block*szd
                << " bytes from " << src << " at " << i*szd << ", but only got "
                << ierr;
            ierr = -11;
            break;
        }
        for (uint32_t j = 0; j < block; ++ j)
            ibuf[j] = j;
        ibuf.resize(block);
        dbuf1.sort(ibuf);

        // the indices need to be shifted by @c i.  Sorted values in @c dbuf2
        for (uint32_t j = 0; j < block; ++ j) {
            dbuf2[j] = dbuf1[ibuf[j]];
            ibuf[j] += i;
        }
        // write the sorted values.
        ierr = UnixWrite(fddes, dbuf2.begin(), szd*block);
        if (static_cast<uint32_t>(ierr) != block*szd) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- oocSortBlocks expected to write " << block*szd
                << " bytes to " << dest << " at " << i*szd
                << ", but only wrote " << ierr;
            ierr = -12;
            break;
        }
        // write the indices.
        ierr = UnixWrite(fdind, ibuf.begin(), block*szi);
        if (static_cast<uint32_t>(ierr) != block*szi) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- oocSortBlocks expected to write " << block*szi
                << " bytes to " << i*szi << " at " << i*szi
                << ", but only wrote " << ierr;
            ierr = -12;
            break;
        }
        else {
            ierr = 0;
        }
    }

#if defined(_WIN32) && defined(_MSC_VER)
    _commit(fddes);
    _commit(fdind);
#endif
    if (ierr < 0) { // remove the output files
        remove(ind);
        remove(dest);
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- roster::oocSortBlocks failed with ierr = " << ierr;
    }
    else if (ibis::gVerbose > 3) {
        ierr = 0;
        timer.stop();
        double speed = 1e-6 * (szd + szd + szi) * nrows;
        speed /= (timer.realTime() > 1.0e-6 ? timer.realTime() : 1.0e-6);
        LOGGER(ibis::gVerbose > 3)
            << "roster::oocSortBlocks completed sorting all (" << mblock
            << ") blocks of " << src << ", wrote results to " << dest
            << " and " << ind << ", used " << timer.realTime()
            << " sec with " << speed << " MB/s";
    }
    return ierr;
} // ibis::roster::oocSortBlocks

/// Merge two consecutive blocks of size @c stride from file @c dsrc and
/// write the results into a new file called @c dout.  An index file is
/// rearranged along with the data values.  The input index file is @c isrc
/// and the output index file is @c iout.  The content of the files are
/// read into memory one block at a time and the block size is defined by
/// @c mblock.  The temporary work arrays are passed in by the caller to
/// make sure there is no chance of running out of memory within this
/// function.
template <typename T>
long ibis::roster::oocMergeBlocks(const char *dsrc, const char *dout,
                                  const char *isrc, const char *iout,
                                  const uint32_t mblock,
                                  const uint32_t stride,
                                  array_t<T>& dbuf1,
                                  array_t<T>& dbuf2,
                                  array_t<uint32_t>& ibuf1,
                                  array_t<uint32_t>& ibuf2) const {
    const int fdsrc = UnixOpen(dsrc, OPEN_READONLY);
    if (fdsrc < 0) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- oocMergeBlocks failed to open " << dsrc
            << " for reading";
        return -1;
    }
#if defined(_WIN32) && defined(_MSC_VER)
    (void)_setmode(fdsrc, _O_BINARY);
#endif
    IBIS_BLOCK_GUARD(UnixClose, fdsrc);
    const int fisrc = UnixOpen(isrc, OPEN_READONLY);
    if (fisrc < 0) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- oocMergeBlocks failed to open " << isrc
            << " for reading";
        return -2;
    }
#if defined(_WIN32) && defined(_MSC_VER)
    (void)_setmode(fisrc, _O_BINARY);
#endif
    IBIS_BLOCK_GUARD(UnixClose, fisrc);
    const int fdout = UnixOpen(dout, OPEN_WRITENEW, OPEN_FILEMODE);
    if (fdout < 0) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- oocMergeBlocks failed to open " << dout
            << " for writing";
        return -3;
    }
#if defined(_WIN32) && defined(_MSC_VER)
    (void)_setmode(fdout, _O_BINARY);
#endif
    IBIS_BLOCK_GUARD(UnixClose, fdout);
    const int fiout = UnixOpen(iout, OPEN_WRITENEW, OPEN_FILEMODE);
    if (fiout < 0) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- oocMergeBlocks failed to open " << iout
            << " for writing";
        return -4;
    }
#if defined(_WIN32) && defined(_MSC_VER)
    (void)_setmode(fiout, _O_BINARY);
#endif
    IBIS_BLOCK_GUARD(UnixClose, fiout);

    ibis::horometer timer;
    timer.start();
    ibuf1.resize(mblock);
    ibuf2.resize(mblock);
    dbuf1.resize(mblock);
    dbuf2.resize(mblock);

    const uint32_t szd = sizeof(T);
    const uint32_t szi = sizeof(uint32_t);
    const uint32_t bszd = szd*mblock;
    const uint32_t bszi = szi*mblock;
    const uint32_t nrows = col->partition()->nRows();
    long ierr = nrows / mblock;
    const uint32_t nblock = ierr + (nrows > mblock * ierr);

    ierr = 0;
    for (uint32_t i0 = 0; ierr == 0 && i0 < nrows; i0 += 2*stride) {
        uint32_t i1 = i0 + stride;
        if (i1 < nrows) { // have two large blocks to merge
            // logically we are working with two large blocks next to each
            // other.  The first one [i0:i1] is guaranteed to have @c stride
            // elements and the second one [i1:i2] may have less.
            const uint32_t i2 = (i1+stride <= nrows ? i1+stride : nrows);
            uint32_t i01 = i0; // index for pages within the first block
            uint32_t i12 = i1; // index for pages within the second block
            uint32_t j01 = 0;
            uint32_t j12 = 0;
            uint32_t block = (i12+mblock <= i2 ? mblock : i2 - i12);
            dbuf2.resize(block);
            ibuf2.resize(block);
            uint32_t cszd = block * szd;
            uint32_t cszi = block * szi;
            uint32_t szdi1 = i01 * szd;
            uint32_t szii1 = i01 * szi;
            uint32_t szdi2 = i12 * szd;
            uint32_t szii2 = i12 * szi;

            // read two pages from the input data file and two pages from
            // the input index file
            ierr = dbuf1.read(fdsrc, szdi1, szdi1+bszd);
            if (static_cast<uint32_t>(ierr) != bszd) {
                LOGGER(ibis::gVerbose > 1)
                    << "Warning -- oocMergeBlocks failed to read " << bszd
                    << " bytes at " << szdi1 << " from " << dsrc;
                ierr = -19;
            }
            else {
                ierr = dbuf2.read(fdsrc, szdi2, szdi2+cszd);
                if (static_cast<uint32_t>(ierr) != cszd) {
                    LOGGER(ibis::gVerbose > 1)
                        << "Warning -- oocMergeBlocks failed to read " << cszd
                        << " bytes at " << szdi2 << " from " << dsrc;
                    ierr = -20;
                }
                else {
                    ierr = ibuf1.read(fisrc, szii1, szii1+bszi);
                    if (static_cast<uint32_t>(ierr) != bszi) {
                        LOGGER(ibis::gVerbose > 1)
                            << "Warning -- oocMergeBlocks failed to read "
                            << bszi << "bytes at " << szii1 << " from "
                            << isrc;
                        ierr = -21;
                    }
                    else {
                        ierr = ibuf2.read(fisrc, szii2, szii2+cszi);
                        if (static_cast<uint32_t>(ierr) != cszi) {
                            LOGGER(ibis::gVerbose > 1)
                                << "Warning -- oocMergeBlocks failed to read "
                                << cszi << " bytes at " << szii2 << " from "
                                << isrc;
                            ierr = -22;
                        }
                        else {
                            ierr = 0;
                        }
                    }
                }
            }

            // loop over all pages in the two consecutive blocks
            while (ierr == 0 && (i01 < i1 || i12 < i2)) {
                if (i01 < i1 && i12 < i2) { // both blocks have pages left
                    while (j01 < mblock && j12 < block) { // both pages useful
                        if (dbuf1[j01] <= dbuf2[j12]) { // output j01
                            ierr = UnixWrite(fdout, &(dbuf1[j01]), szd);
                            if (static_cast<uint32_t>(ierr) != szd) {
                                LOGGER(ibis::gVerbose > 1)
                                    << "Warning -- oocMergeBlocks failed to "
                                    "write data value # " << (i01+j01)
                                    << " to " << dout;
                                ierr = -23;
                                break;
                            }
                            ierr = UnixWrite(fiout, &(ibuf1[j01]), szi);
                            if (static_cast<uint32_t>(ierr) != szi) {
                                LOGGER(ibis::gVerbose > 1)
                                    << "Warning -- oocMergeBlocks failed to "
                                    "write data value # " << (i01+j01)
                                    << " to " << iout;
                                ierr = -24;
                                break;
                            }
                            ierr = 0;
                            ++ j01;
                        }
                        else { // output the value at j12
                            ierr = UnixWrite(fdout, &(dbuf2[j12]), szd);
                            if (static_cast<uint32_t>(ierr) != szd) {
                                LOGGER(ibis::gVerbose > 1)
                                    << "Warning -- oocMergeBlocks failed to "
                                    "write data value # " << (i12+j12)
                                    << " to " << dout;
                                ierr = -25;
                                break;
                            }
                            ierr = UnixWrite(fiout, &(ibuf2[j12]), szi);
                            if (static_cast<uint32_t>(ierr) != szi) {
                                LOGGER(ibis::gVerbose > 1)
                                    << "Warning -- oocMergeBlocks failed to "
                                    "write data value # " << (i12+j12)
                                    << " to " << iout;
                                ierr = -26;
                                break;
                            }
                            ierr = 0;
                            ++ j12;
                        }
                    } // j01 < mblock && j12 < block
                }
                else if (i01 < i1) { // block still have more pages
                    for (; j01 < mblock; ++ j01) { // output all elements
                        ierr = UnixWrite(fdout, &(dbuf1[j01]), szd);
                        if (static_cast<uint32_t>(ierr) != szd) {
                            LOGGER(ibis::gVerbose > 1)
                                << "Warning -- oocMergeBlocks failed to "
                                "write data value # " << (i01+j01)
                                << " to " << dout;
                            ierr = -27;
                            break;
                        }
                        ierr = UnixWrite(fiout, &(ibuf1[j01]), szi);
                        if (static_cast<uint32_t>(ierr) != szi) {
                            LOGGER(ibis::gVerbose > 1)
                                << "Warning -- oocMergeBlocks failed to "
                                "write data value # " << (i01+j01)
                                << " to " << iout;
                            ierr = -28;
                            break;
                        }
                        ierr = 0;
                    } // j01
                }
                else { // i12 < i2, block two has more pages
                    for (; j12 < block; ++ j12) { // output all elements
                        ierr = UnixWrite(fdout, &(dbuf2[j12]), szd);
                        if (static_cast<uint32_t>(ierr) != szd) {
                            LOGGER(ibis::gVerbose > 1)
                                << "Warning -- oocMergeBlocks failed to "
                                "write data value # " << (i12+j12)
                                << " to " << dout;
                            ierr = -29;
                            break;
                        }
                        ierr = UnixWrite(fiout, &(ibuf2[j12]), szi);
                        if (static_cast<uint32_t>(ierr) != szi) {
                            LOGGER(ibis::gVerbose > 1)
                                << "Warning -- oocMergeBlocks failed to "
                                "write data value # " << (i12+j12)
                                << " to " << iout;
                            ierr = -30;
                            break;
                        }
                        ierr = 0;
                    } // j12
                }

                if (ierr == 0) {
                    if (j01 >= mblock) { // read next page in block one
                        i01 += mblock;
                        if (i01 < i1) {
                            szdi1 += bszd;
                            szii1 += bszi;
                            j01 = 0;
                            ierr = dbuf1.read(fdsrc, szdi1, szdi1+bszd);
                            if (static_cast<uint32_t>(ierr) != bszd) {
                                LOGGER(ibis::gVerbose > 1)
                                    << "Warning -- oocMergeBlocks failed to read "
                                    << bszd << " bytes at " << szdi1 << " from "
                                    << dsrc;
                                ierr = -31;
                            }
                            else {
                                ierr = ibuf1.read(fisrc, szii1, szii1+bszi);
                                if (static_cast<uint32_t>(ierr) != bszi) {
                                    LOGGER(ibis::gVerbose > 1)
                                        << "Warning -- oocMergeBlocks failed to "
                                        "read " << bszi << " bytes at " << szii1
                                        << " from " << isrc;
                                    ierr = -33;
                                }
                                else {
                                    ierr = 0;
                                }
                            }
                        }
                    }
                    if (j12 >= block) { // read next page in block two
                        j12 = 0;
                        i12 += block;
                        if (i12 < i2) {
                            szdi2 += cszd;
                            szii2 += cszi;
                            block = (i12+mblock <= i2 ? mblock : i2 - i12);
                            cszd = szd * block;
                            cszi = szi * block;
                            ierr = dbuf2.read(fdsrc, szdi2, szdi2+cszd);
                            if (static_cast<uint32_t>(ierr) != cszd) {
                                LOGGER(ibis::gVerbose > 1)
                                    << "Warning -- oocMergeBlocks failed to read "
                                    << cszd << " bytes at " << szdi2 << " from "
                                    << dsrc;
                                ierr = -32;
                            }
                            else {
                                ierr = ibuf2.read(fisrc, szii2, szii2+cszi);
                                if (static_cast<uint32_t>(ierr) != cszi) {
                                    LOGGER(ibis::gVerbose > 1)
                                        << "Warning -- oocMergeBlocks failed to "
                                        "read " << cszi << " bytes at " << szii2
                                        << " from " << isrc;
                                    ierr = -34;
                                }
                                else {
                                    ierr = 0;
                                }
                            }
                        }
                    }
                }
            }
        }
        else { // only one block remain in the input files, copy the block
            for (uint32_t i = i0; i+mblock <= nrows; i += mblock) {
                // copy of the pages in the last block
                const uint32_t szdi = szd * i;
                const uint32_t szii = szi * i;
                ierr = dbuf1.read(fdsrc, szdi, szdi + bszd);
                if (static_cast<uint32_t>(ierr) != bszd) {
                    LOGGER(ibis::gVerbose > 1)
                        << "Warning -- oocMergeBlocks failed to read " << bszd
                        << " bytes at " << szdi << " from " << dsrc;
                    ierr = -11;
                    break;
                }
                ierr = UnixWrite(fdout, dbuf1.begin(), bszd);
                if (static_cast<uint32_t>(ierr) != bszd) {
                    LOGGER(ibis::gVerbose > 1)
                        << "Warning -- oocMergeBlocks failed to write " << bszd
                        << " bytes at " << szdi << " to " << dout;
                    ierr = -12;
                    break;
                }
                ierr = ibuf1.read(fisrc, szii, szii + bszi);
                if (static_cast<uint32_t>(ierr) != bszd) {
                    LOGGER(ibis::gVerbose > 1)
                        << "Warning -- oocMergeBlocks failed to read " << bszi
                        << " bytes at " << szii << " from " << isrc;
                    ierr = -13;
                    break;
                }
                ierr = UnixWrite(fiout, ibuf1.begin(), bszi);
                if (static_cast<uint32_t>(ierr) != bszi) {
                    LOGGER(ibis::gVerbose > 1)
                        << "Warning -- oocMergeBlocks failed to write " << bszi
                        << " bytes at " << szii << " from " << iout;
                    ierr = -14;
                    break;
                }
                ierr = 0;
            } // i
            if (ierr == 0 && nblock > nrows / mblock) {
                // copy the last page that is partially full
                const uint32_t szdi = szd * mblock * (nblock - 1);
                const uint32_t szii = szi * mblock * (nblock - 1);
                const uint32_t block = nrows - mblock * (nblock - 1);
                const uint32_t cszd = block * szd;
                const uint32_t cszi = block * szi;
                dbuf1.resize(block);
                ibuf1.resize(block);
                ierr = dbuf1.read(fdsrc, szdi, szdi + cszd);
                if (static_cast<uint32_t>(ierr) != cszd) {
                    LOGGER(ibis::gVerbose > 1)
                        << "Warning -- oocMergeBlocks failed to read " << cszd
                        << " bytes at " << szdi << " from " << dsrc;
                    ierr = -15;
                }
                else {
                    ierr = UnixWrite(fdout, dbuf1.begin(), cszd);
                    if (static_cast<uint32_t>(ierr) != cszd) {
                        LOGGER(ibis::gVerbose > 1)
                            << "Warning -- oocMergeBlocks failed to read "
                            << cszd << "bytes at " << szdi << " from " << dout;
                        ierr = -16;
                    }
                    else {
                        ierr = ibuf1.read(fisrc, szii, szii + cszi);
                        if (static_cast<uint32_t>(ierr) != cszi) {
                            LOGGER(ibis::gVerbose > 1)
                                << "Warning -- oocMergeBlocks failed to read "
                                << cszi << "bytes at " << szii << " from "
                                << isrc;
                            ierr = -17;
                        }
                        else {
                            ierr = UnixWrite(fiout, ibuf1.begin(), cszi);
                            if (static_cast<uint32_t>(ierr) != cszi) {
                                LOGGER(ibis::gVerbose > 1)
                                    << "Warning -- oocMergeBlocks failed to "
                                    "write " << cszi << " bytes at " << szii
                                    << " from " << iout;
                                ierr = -18;
                            }
                            else {
                                ierr = 0;
                            }
                        }
                    }
                }
            } // left overs
        }
    } // i0

#if defined(_WIN32) && defined(_MSC_VER)
    _commit(fiout);
    _commit(fdout);
#endif

    if (ierr != 0) { // remove the output in case of error
        remove(dout);
        remove(iout);
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- roster::oocMergeBlocks failed with ierr = "
            << ierr;
    }
    else if (ibis::gVerbose > 3) {
        ierr = 0;
        timer.stop();
        double speed = 2e-6 * ((szd+szi) * nrows);
        speed /= (timer.realTime() > 1e-6 ? timer.realTime() : 1e-6);
        LOGGER(ibis::gVerbose > 3)
            << "roster::oocMergeBlocks completed merging blocks of size "
            << stride << ", written output to " << dout << " (" << iout
            << "), used " << timer.realTime() << " sec with " << speed
            << " MB/s";
    }
    return ierr;
} //ibis::roster::oocMergeBlocks

template <typename T>
long ibis::roster::mergeBlock2(const char *dsrc, const char *dout,
                               const uint32_t segment, array_t<T>& buf1,
                               array_t<T>& buf2, array_t<T>& buf3) {
    const int fdsrc = UnixOpen(dsrc, OPEN_READONLY);
    if (fdsrc < 0) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- roster::mergeBlock2 failed to open " << dsrc
            << " for reading";
        return -1;
    }
    IBIS_BLOCK_GUARD(UnixClose, fdsrc);
    const int fdout = UnixOpen(dout, OPEN_WRITENEW, OPEN_FILEMODE);
    if (fdout < 0) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- roster::mergeBlock2 failed to open " << dout
            << " for writing";
        return -2;
    }
    IBIS_BLOCK_GUARD(UnixClose, fdout);
#if defined(_WIN32) && defined(_MSC_VER)
    (void)_setmode(fdsrc, _O_BINARY);
    (void)_setmode(fdout, _O_BINARY);
#endif

    uint32_t mblock = (buf1.size() <= buf2.size() ? buf1.size() : buf2.size());
    if (mblock > buf3.size())
        mblock = buf3.size();
    buf1.resize(mblock);
    buf2.resize(mblock);
    buf3.resize(mblock);
    std::less<T> cmp;

    ibis::horometer timer;
    timer.start();

    long ierr = 0;
    bool more = true;
    uint32_t totread = 0;
    const uint32_t szd = sizeof(T);
    const uint32_t bszd = sizeof(T)*mblock;

    for (uint32_t i0 = 0; more; i0 += 2*segment) {
        const uint32_t i1 = i0 + segment;
        ierr = UnixSeek(fdsrc, i1*szd, SEEK_SET);
        if (ierr == 0) {        // have two segments to merge
            // logically we are working with two large blocks next to each
            // other.  The first one [i0:i1] is guaranteed to have @c segment
            // elements and the second one [i1:i2] may have less.
            uint32_t i2 = i1+segment;
            uint32_t i01 = i0; // index for pages within the first block
            uint32_t i12 = i1; // index for pages within the second block
            uint32_t j01 = 0;
            uint32_t j12 = 0;
            uint32_t block2 = mblock;
            uint32_t szdi1 = i01 * szd;
            uint32_t szdi2 = i12 * szd;

            // read two pages from the input data file and two pages from
            // the input index file
            ierr = buf1.read(fdsrc, szdi1, szdi1+bszd);
            if (ierr != static_cast<long>(bszd)) {
                LOGGER(ibis::gVerbose > 1)
                    << "Warning -- roster::mergeBlock2 failed to read " << bszd
                    << " bytes at " << szdi1 << " from " << dsrc;
                ierr = -3;
                more = false;
                break;
            }
            totread += ierr;
            ierr = buf2.read(fdsrc, szdi2, szdi2+bszd);
            if (ierr >= 0) {
                block2 = ierr / szd;
                i2 = i12 + block2;
                more = (i01+mblock < i1);
                totread += ierr;
            }
            else {
                LOGGER(ibis::gVerbose > 1)
                    << "Warning -- mergeBlock2 failed to read " << bszd
                    << " bytes at " << szdi2 << " from " << dsrc;
                ierr = -4;
                more = false;
                break;
            }

            // loop over all pages in the two consecutive blocks
            while (more && i01 < i1 && i12 < i2) {
                buf3.clear();
                for (uint32_t i3 = 0; i3 < mblock; ++ i3) {
                    if (j01 < mblock && j12 < block2) {
                        if (cmp(buf2[j12], buf1[j01])) {
                            buf3.push_back(buf2[j12]);
                            ++ j12;
                        }
                        else {
                            buf3.push_back(buf1[j01]);
                            ++ j01;
                        }
                    }
                    else if (j01 < mblock) {
                        buf3.push_back(buf1[j01]);
                        ++ j01;
                    }
                    else if (j12 < block2) {
                        buf3.push_back(buf2[j12]);
                        ++ j12;
                    }
                    else {
                        break;
                    }
                    if (j01 >= mblock && i01+mblock < i1) {
                        // read next block from segment 0
                        i01 += mblock;
                        szdi1 += bszd;
                        ierr = buf1.read(fdsrc, szdi1, szdi1+bszd);
                        if (ierr != static_cast<long>(bszd)) {
                            LOGGER(ibis::gVerbose > 1)
                                << "Warning -- roster::mergeBlock2 failed to "
                                "read " << bszd << " bytes at " << szdi1
                                << " from " << dsrc;
                            ierr = -5;
                            more = false;
                            break;
                        }
                        totread += ierr;
                    }
                    if (block2==mblock && j12 >= mblock) {
                        i12 += mblock;
                        szdi2 += bszd;
                        ierr = buf1.read(fdsrc, szdi2, szdi2+bszd);
                        if (ierr >= 0) {
                            block2 = ierr / szd;
                            i2 = i12 + block2;
                            more = (i01+mblock < i1);
                            totread += ierr;
                        }
                        else {
                            LOGGER(ibis::gVerbose > 1)
                                << "Warning -- roster::mergeBlock2 failed to "
                                "read " << bszd << "bytes at " << szdi2
                                << " from " << dsrc;
                            ierr = -6;
                            more = false;
                            break;
                        }
                    }
                }
                ierr = UnixWrite(fdout, buf3.begin(), buf3.size()*szd);
            }
            buf3.resize(mblock);
        }
        else { // only one segment remain in the input file, copy the segment
            long nread;
            while ((nread = UnixRead(fdsrc, buf1.begin(), bszd) > 0)) {
                ierr = UnixWrite(fdout, buf1.begin(), nread);
                totread += nread;
            }
            more = false;
        }
    } // i0

#if defined(_WIN32) && defined(_MSC_VER)
    _commit(fdout);
#endif

    if (ierr > 0)
        ierr = 0;
    if (ibis::gVerbose > 3) {
        timer.stop();
        double speed = timer.realTime();
        if (speed < 1.0e-6)
            speed = 1.0e-6;
        speed *= 2e-6 * totread;
        LOGGER(ibis::gVerbose > 3)
            << "roster::mergeBlock2 completed merging blocks of size "
            << segment << ", written output to " << dout << ", used "
            << timer.realTime() << " sec with " << speed << " MB/s";
    }
    return ierr;
} //ibis::roster::mergeBlock2

/// Print a terse message about the roster.   If the roster list is not
/// initialized correctly, it prints a warning message.
void ibis::roster::print(std::ostream& out) const {
    if (col != 0 && (ind.size() == col->partition()->nRows() || inddes >= 0))
        out << "a roster list for " << col->partition()->name() << '.'
            << col->name() << " with " << ind.size() << " row"
            << (ind.size() > 1 ? "s" : "");
    else
        out << "an empty roster list";
} // ibis::roster::print

uint32_t ibis::roster::size() const {
    return ((ind.size() == col->partition()->nRows() || inddes >= 0) ?
            col->partition()->nRows() : 0);
} // ibis::roster::size

/// Return the smallest i such that val >= val[ind[i]].
uint32_t ibis::roster::locate(const double& v) const {
    uint32_t hit = ind.size();
    if (hit == 0) return hit;

    std::string fnm; // name of the data file
    fnm = col->partition()->currentDataDir();
    fnm += col->name();
    int ierr = 0;

    switch (col->type()) {
    case ibis::UBYTE: { // unsigned char
        array_t<unsigned char> val;
        ierr = ibis::fileManager::instance().getFile(fnm.c_str(), val);
        if (ierr == 0 && val.size() == ind.size()) {
            unsigned char bnd = static_cast<unsigned char>(v);
            if (bnd < v)
                ++ bnd;
            hit = val.find(ind, bnd);
        }
        else {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- roster::locate expected ind.size(" << ind.size()
                << ") and data array (" << val.size() << ") to be the same";
        }
        break;
    }
    case ibis::BYTE: { // signed char
        array_t<signed char> val;
        ierr = ibis::fileManager::instance().getFile(fnm.c_str(), val);
        if (ierr == 0 && val.size() == ind.size()) {
            char bnd = static_cast<signed char>(v);
            if (bnd < v)
                ++ bnd;
            hit = val.find(ind, bnd);
        }
        else {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- roster::locate expected ind.size(" << ind.size()
                << ") and val.size(" << val.size() << ") to be the esame";
        }
        break;
    }
    case ibis::USHORT: { // unsigned short int
        array_t<uint16_t> val;
        ierr = ibis::fileManager::instance().getFile(fnm.c_str(), val);
        if (ierr == 0 && val.size() == ind.size()) {
            uint16_t bnd = static_cast<uint16_t>(v);
            if (bnd < v)
                ++ bnd;
            hit = val.find(ind, bnd);
        }
        else {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- roster::locate expected ind.size(" << ind.size()
                << ") and val.size(" << val.size() << ") to be the esame";
        }
        break;
    }
    case ibis::SHORT: { // signed short int
        array_t<int16_t> val;
        ierr = ibis::fileManager::instance().getFile(fnm.c_str(), val);
        if (ierr == 0 && val.size() == ind.size()) {
            int16_t bnd = static_cast<int16_t>(v);
            if (bnd < v)
                ++ bnd;
            hit = val.find(ind, bnd);
        }
        else {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- roster::locate expected ind.size(" << ind.size()
                << ") and val.size(" << val.size() << ") to be the esame";
        }
        break;
    }
    case ibis::UINT: { // unsigned int
        array_t<uint32_t> val;
        ierr = ibis::fileManager::instance().getFile(fnm.c_str(), val);
        if (ierr == 0 && val.size() == ind.size()) {
            uint32_t bnd = static_cast<uint32_t>(v);
            if (bnd < v)
                ++ bnd;
            hit = val.find(ind, bnd);
        }
        else {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- roster::locate expected ind.size(" << ind.size()
                << ") and val.size(" << val.size() << ") to be the esame";
        }
        break;
    }
    case ibis::INT: { // signed int
        array_t<int32_t> val;
        ierr = ibis::fileManager::instance().getFile(fnm.c_str(), val);
        if (ierr == 0 && val.size() == ind.size()) {
            int32_t bnd = static_cast<int32_t>(v);
            if (bnd < v)
                ++ bnd;
            hit = val.find(ind, bnd);
        }
        else {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- roster::locate expected ind.size(" << ind.size()
                << ") and val.size(" << val.size() << ") to be the esame";
        }
        break;
    }
    case ibis::ULONG: { // unsigned long int
        array_t<uint64_t> val;
        ierr = ibis::fileManager::instance().getFile(fnm.c_str(), val);
        if (ierr == 0 && val.size() == ind.size()) {
            uint64_t bnd = static_cast<uint64_t>(v);
            if (bnd < v)
                ++ bnd;
            hit = val.find(ind, bnd);
        }
        else {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- roster::locate expected ind.size(" << ind.size()
                << ") and val.size(" << val.size() << ") to be the esame";
        }
        break;
    }
    case ibis::LONG: { // signed long int
        array_t<int64_t> val;
        ierr = ibis::fileManager::instance().getFile(fnm.c_str(), val);
        if (ierr == 0 && val.size() == ind.size()) {
            int64_t bnd = static_cast<int64_t>(v);
            if (bnd < v)
                ++ bnd;
            hit = val.find(ind, bnd);
        }
        else {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- roster::locate expected ind.size(" << ind.size()
                << ") and val.size(" << val.size() << ") to be the esame";
        }
        break;
    }
    case ibis::FLOAT: { // float
        array_t<float> val;
        ierr = ibis::fileManager::instance().getFile(fnm.c_str(), val);
        if (ierr == 0 && val.size() == ind.size()) {
            float bnd = static_cast<float>(v);
            hit = val.find(ind, bnd);
        }
        else {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- roster::locate expected ind.size(" << ind.size()
                << ") and val.size(" << val.size() << ") to be the esame";
        }
        break;
    }
    case ibis::DOUBLE: { // double
        array_t<double> val;
        ierr = ibis::fileManager::instance().getFile(fnm.c_str(), val);
        if (ierr == 0 && val.size() == ind.size()) {
            hit = val.find(ind, v);
        }
        else {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- roster::locate expected ind.size(" << ind.size()
                << ") and val.size(" << val.size() << ") to be the esame";
        }
        break;
    }
    default: {
        ibis::util::logger lg;
        lg() << "Warning -- roster[" << col->partition()->name() << "."
             << col->name() << "]::locate -- no roster list for column type "
             << ibis::TYPESTRING[static_cast<int>(col->type())];
        break;}
    }

    return hit;
} // ibis::roster::locate

/// In-core searching function.  Attempts to read .ind and .srt into
/// memory.  Returns a negative value if it fails to read the necessary
/// data files into memory.  Returns 0 if there is no hits, a positive
/// number if there are some hits.
///
/// @note This function only adds more positions to pos.  The caller needs
/// to initialize the output array if necessary.
template <typename T> int
ibis::roster::icSearch(const ibis::array_t<T>& vals,
                       std::vector<uint32_t>& pos) const {
    int ierr;
    std::string evt;
    if (ibis::gVerbose > 3) {
        evt = "roster[";
        evt += col->fullname();
        evt += "]::icSearch<";
        evt += typeid(T).name();
        evt += '>';
    }
    else {
        evt = "roster::icSearch";
    }
    const uint32_t nrows = col->partition()->nRows();
    if (ind.size() != nrows) { // not a valid index array
        if (col->partition()->currentDataDir() != 0) { // one more try
            ierr = const_cast<ibis::roster*>(this)->read((const char*)0);
        }
        else {
            ierr = -1;
        }
        if (ierr < 0 || ind.size() != nrows) {
            LOGGER(ibis::gVerbose > 3)
                << "Warning -- " << evt << " can not continue with ind["
                << ind.size() << "], need ind to have " << nrows << " rows";
            return -1;
        }
    }

    std::string fname = col->partition()->currentDataDir();
    fname += FASTBIT_DIRSEP;
    fname += col->name();
    int len = fname.size();
    fname += ".srt";

    uint32_t iv = 0; // for vals
    uint32_t it = 0; // for tmp
    array_t<T> tmp;
    const uint32_t nvals = vals.size();

    LOGGER(ibis::gVerbose > 4)
        << evt << " attempt to read the content of " << fname
        << " and locate " << vals.size() << " value"
        << (vals.size()>1?"s":"");
    ierr = ibis::fileManager::instance().getFile(fname.c_str(), tmp);
    if (ierr == 0) { // got the sorted values
        while (iv < nvals && it < nrows) {
            // move iv so that vals[iv] is not less than tmp[it]
            // while (iv < nvals && vals[iv] < tmp[it])
            //     ++ iv;
            if (vals[iv] < tmp[it]) {
                iv = ibis::util::find(vals, tmp[it], iv);
                if (iv >= nvals)
                    break;
            }
            // move it so that tmp[it] is not less than vals[iv]
            // while (it < nrows && vals[iv] > tmp[it])
            //     ++ it;
            if (vals[iv] > tmp[it])
                it = ibis::util::find(tmp, vals[iv], it);

            while (it < nrows && vals[iv] == tmp[it]) { // found a match
                pos.push_back(ind[it]);
                ++ it;
            }
        }

        LOGGER(ibis::gVerbose > 4)
            << evt << " read the content of sorted data file " << fname
            << " and found " << pos.size() << " match"
            << (pos.size()>1?"es":"");
        return 0;
    }
    else {
        LOGGER(ibis::gVerbose > 3)
            << evt << " failed to read data file " << fname
            << ", see whether the base data file is usable";
    }

    // try to read the base data
    fname.erase(len);
    ierr = ibis::fileManager::instance().getFile(fname.c_str(), tmp);
    if (ierr == 0) { // got the base data in memory
        while (iv < nvals && it < nrows) {
            // move iv so that vals[iv] is not less than tmp[ind[it]]
            // while (iv < nvals && vals[iv] < tmp[ind[it]])
            //     ++ iv;
            if (vals[iv] < tmp[ind[it]]) {
                iv = ibis::util::find(vals, tmp[ind[it]], iv);
                if (iv >= nvals)
                    break;
            }
            // move it so that tmp[ind[it]] is not less than vals[iv]
            // while (it < nrows && vals[iv] > tmp[ind[it]])
            //     ++ it;
            if (vals[iv] > tmp[ind[it]])
                it = ibis::util::find(tmp, ind, vals[iv], it);

            // found a match
            if (it < nrows && vals[iv] == tmp[ind[it]]) {
                do {
                    pos.push_back(ind[it]);
                    ++ it;
                } while (it < nrows && vals[iv] == tmp[ind[it]]);
                ++ iv;
            }
        }

        LOGGER(ibis::gVerbose > 4)
            << evt << " read the content of base data file " << fname
            << " and found " << pos.size() << " match"
            << (pos.size()>1?"es":"");
        ierr = 0;
    }
    else {
        LOGGER(ibis::gVerbose > 1)
            << "Warning -- " << evt
            << " failed to read data files " << fname << ".srt and " << fname;
        ierr = -2;
    }
    return ierr;
} // ibis::roster::icSearch

/// Out-of-core search function.  It requires at least .ind file to be in
/// memory.  Need to implement a version that can read both .ind and .srt
/// files during search.
///
/// @note This function only adds more positions to pos.  The caller needs
/// to initialize the output array as necessary.
template <typename T> int
ibis::roster::oocSearch(const ibis::array_t<T>& vals,
                        std::vector<uint32_t>& pos) const {
    const uint32_t nvals = vals.size();
    const uint32_t nrows = col->partition()->nRows();
    // attempt to ensure the sorted values are available
    int ierr = writeSorted(static_cast<const char*>(0));
    if (ierr < 0) {
        if (col->partition()->currentDataDir() != 0) { // one more try
            ierr = const_cast<ibis::roster*>(this)->read((const char*)0);
        }
        else {
            ierr = -1;
        }
        if (ierr < 0 || ind.size() != nrows) {
            return ierr;
        }
    }

    std::string evt;
    if (ibis::gVerbose > 3) {
        evt = "roster[";
        evt += col->fullname();
        evt += "]::oocSearch<";
        evt += typeid(T).name();
        evt += '>';
    }
    else {
        evt = "roster::oocSearch";
    }
    std::string fname = col->partition()->currentDataDir();
    fname += FASTBIT_DIRSEP;
    fname += col->name();
    int len = fname.size();
    fname += ".srt";
    LOGGER(ibis::gVerbose > 4)
        << evt << " attempt to read the content of " << fname
        << " to locate " << vals.size() << " value"
        << (vals.size()>1?"s":"");

    int srtdes = UnixOpen(fname.c_str(), OPEN_READONLY);
    if (srtdes < 0) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " failed to open the file "
            << fname;
        return -5;
    }
    IBIS_BLOCK_GUARD(UnixClose, srtdes);
#if defined(_WIN32) && defined(_MSC_VER)
    (void)_setmode(srtdes, _O_BINARY);
#endif

    uint32_t iv = 0; // index for vals
    uint32_t ir = 0; // index for the rows to be read
    const unsigned int tbytes = sizeof(T);

    ibis::fileManager::buffer<T> mybuf;
    char *cbuf = reinterpret_cast<char*>(mybuf.address());
    const uint32_t ncbuf = tbytes * mybuf.size();
    const uint32_t nbuf = mybuf.size();
    if (nbuf > 0 && ind.size() == nrows) {
        // each read operation fills the buffer, use in-memory ind array
        while (iv < nvals && ir < nrows) {
            ierr = UnixRead(srtdes, cbuf, ncbuf);
            if (ierr < static_cast<int>(tbytes)) {
                return -6;
            }

            const T* curr = reinterpret_cast<const T*>(cbuf);
            const T* end = curr + ierr / tbytes;
            while (curr < end) {
                while (iv < nvals && vals[iv] < *curr)
                    ++ iv;
                if (iv >= nvals) {
                    return (pos.size() > 0);
                }
                while (curr < end && vals[iv] > *curr) {
                    ++ curr;
                    ++ ir;
                }
                while (curr < end && vals[iv] == *curr) {
                    pos.push_back(ind[ir]);
                    ++ curr;
                    ++ ir;
                }
            }
        }

        LOGGER(ibis::gVerbose > 4)
            << evt << " read the content of " << fname
            << " and found " << pos.size() << " match"
            << (pos.size()>1?"es":"");
        return (pos.size() > 0);
    }

    if (inddes < 0) {
        fname.erase(len);
        fname += ".ind";
        inddes = UnixOpen(fname.c_str(), OPEN_READONLY);
        if (inddes < 0) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << evt << " failed to open index file "
                << fname;
            return -7;
        }
    }
#if defined(_WIN32) && defined(_MSC_VER)
    (void)_setmode(inddes, _O_BINARY);
#endif
    if (nbuf > 0 && inddes > 0) {
        // bulk read, also need to read ind array
        while (iv < nvals && ir < nrows) {
            ierr = UnixRead(srtdes, cbuf, ncbuf);
            if (ierr < static_cast<int>(tbytes)) {
                return -8;
            }

            const T* curr = reinterpret_cast<const T*>(cbuf);
            const T* end = curr + ierr / tbytes;
            while (curr < end) {
                while (iv < nvals && vals[iv] < *curr)
                    ++ iv;
                if (iv >= nvals) {
                    return (pos.size() > 0);
                }
                while (curr < end && vals[iv] > *curr) {
                    ++ curr;
                    ++ ir;
                }
                while (curr < end && vals[iv] == *curr) {
                    uint32_t tmp;
                    ierr = UnixSeek(inddes, ir*sizeof(tmp), SEEK_SET);
                    ierr = UnixRead(inddes, &tmp, sizeof(tmp));
                    if (ierr <= 0) {
                        LOGGER(ibis::gVerbose > 1)
                            << "Warning -- " << evt << " failed to "
                            "read index value # " << ir;
                        return -9;
                    }
                    pos.push_back(tmp);
                    ++ curr;
                    ++ ir;
                }
            }
        }
    }
    else { // read one value at a time, very slow!
        cbuf = 0;

        T curr;
        ierr = UnixRead(srtdes, &curr, tbytes);
        if (ierr < static_cast<int>(tbytes)) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- " << evt << " failed to read value # "
                << ir << " from the sorted file";
            return -10;
        }

        while (iv < nvals && ir < nrows) {
            while (iv < nvals && vals[iv] < curr)
                ++ iv;
            if (iv >= nvals)
                return (pos.size() > 0);

            while (ir < nrows && vals[iv] > curr) {
                ierr = UnixRead(srtdes, &curr, tbytes);
                if (ierr < static_cast<int>(tbytes)) {
                    LOGGER(ibis::gVerbose > 1)
                        << "Warning -- " << evt << " failed to read value # "
                        << ir << " from the sorted file";
                    return -11;
                }
                ++ ir;
            }
            while (ir < nrows && vals[iv] == curr) {
                if (ind.size() == nrows) {
                    pos.push_back(ind[ir]);
                }
                else {
                    uint32_t tmp;
                    ierr = UnixSeek(inddes, ir*sizeof(tmp), SEEK_SET);
                    ierr = UnixRead(inddes, &tmp, sizeof(tmp));
                    if (ierr <= 0) {
                        LOGGER(ibis::gVerbose > 1)
                            << "Warning -- " << evt << " failed to read "
                            "index value # " << ir;
                        return -12;
                    }
                    pos.push_back(tmp);
                }
                ierr = UnixRead(srtdes, &curr, tbytes);
                if (ierr < static_cast<int>(tbytes)) {
                    LOGGER(ibis::gVerbose > 1)
                        << "Warning -- " << evt << " failed to read "
                        "value # " << ir << " from the sorted file";
                    return -13;
                }
                ++ ir;
            }
        }
    }

    LOGGER(ibis::gVerbose > 4)
        << evt << " read the content of " << fname
        << " and found " << pos.size() << " match"
        << (pos.size()>1?"es":"");
    return (pos.size() > 0);
} // ibis::roster::oocSearch

/// In-core searching function.  Attempts to read .ind and .srt into
/// memory.  Returns a negative value if it fails to read the necessary
/// data files into memory.
/// @note This function only adds more positions to pos.  The caller needs
/// to initialize the output array if necessary.
template <typename T> int
ibis::roster::icSearch(const std::vector<T>& vals,
                       std::vector<uint32_t>& pos) const {
    int ierr;
    std::string evt;
    if (ibis::gVerbose > 3) {
        evt = "roster[";
        evt += col->fullname();
        evt += "]::icSearch<";
        evt += typeid(T).name();
        evt += '>';
    }
    else {
        evt = "roster::icSearch";
    }
    const uint32_t nrows = col->partition()->nRows();
    if (ind.size() != nrows) { // not a valid index array
        if (col->partition()->currentDataDir() != 0) { // one more try
            ierr = const_cast<ibis::roster*>(this)->read((const char*)0);
        }
        else {
            ierr = -1;
        }
        if (ierr < 0 || ind.size() != nrows) {
            LOGGER(ibis::gVerbose > 3)
                << "Warning -- " << evt << " can not continue with ind["
                << ind.size() << "], need ind to have " << nrows << " rows";
            return -1;
        }
    }

    std::string fname = col->partition()->currentDataDir();
    fname += FASTBIT_DIRSEP;
    fname += col->name();
    int len = fname.size();
    fname += ".srt";

    uint32_t iv = 0;
    uint32_t it = 0;
    array_t<T> tmp;
    const uint32_t nvals = vals.size();

    LOGGER(ibis::gVerbose > 4)
        << evt << " attempt to read the content of " << fname
        << " to locate " << vals.size() << " value"
        << (vals.size()>1?"s":"");
    ierr = ibis::fileManager::instance().getFile(fname.c_str(), tmp);
    if (ierr == 0) { // got the sorted values
        while (iv < nvals && it < nrows) {
            // move iv so that vals[iv] is not less than tmp[it]
            if (vals[iv] < tmp[it]) {
                iv = ibis::util::find(vals, tmp[it], iv);
                if (iv >= nvals)
                    return (pos.size() > 0);
            }
            // move it so that tmp[it] is not less than vals[iv]
            if (vals[iv] > tmp[it])
                it = ibis::util::find(tmp, vals[iv], it);

            while (it < nrows && vals[iv] == tmp[it]) { // found a match
                pos.push_back(ind[it]);
                ++ it;
            }
        }

        LOGGER(ibis::gVerbose > 4)
            << evt << " read the content of " << fname
            << " and found " << pos.size() << " match"
            << (pos.size()>1?"es":"");
        return (pos.size() > 0);
    }
    else {
        LOGGER(ibis::gVerbose > 3)
            << evt << " failed to read data file " << fname
            << ", see whether the base data file is usable";
    }

    // try to read the base data
    fname.erase(len);
    ierr = ibis::fileManager::instance().getFile(fname.c_str(), tmp);
    if (ierr == 0) { // got the base data in memory
        while (iv < nvals && it < nrows) {
            // move iv so that vals[iv] is not less than tmp[ind[it]]
            if (vals[iv] < tmp[ind[it]]) {
                iv = ibis::util::find(vals, tmp[ind[it]], iv);
                if (iv >= nvals)
                    return (pos.size() > 0);
            }
            // move it so that tmp[ind[it]] is not less than vals[iv]
            if (vals[iv] > tmp[ind[it]])
                it = ibis::util::find(tmp, ind, vals[iv], it);

            // found a match
            if (it < nrows && vals[iv] == tmp[ind[it]]) {
                do {
                    pos.push_back(ind[it]);
                    ++ it;
                } while (it < nrows && vals[iv] == tmp[ind[it]]);
                ++ iv;
            }
        }
    }
    else {
        LOGGER(ibis::gVerbose > 1)
            << "Warning -- " << evt << " failed to read data files "
            << fname << ".srt and " << fname;
        return -2;
    }
    return (pos.size() > 0);
} // ibis::roster::icSearch

/// Out-of-core search function.  It requires at least .ind file to be in
/// memory.  Need to implement a version that can read both .ind and .srt
/// files during search.
/// @note This function only adds more positions to pos.  The caller needs
/// to initialize the output array if necessary.
template <typename T> int
ibis::roster::oocSearch(const std::vector<T>& vals,
                        std::vector<uint32_t>& pos) const {
    const uint32_t nvals = vals.size();
    const uint32_t nrows = col->partition()->nRows();
    // attempt to ensure the sorted values are available
    int ierr = writeSorted(static_cast<const char*>(0));
    if (ierr < 0) {
        if (col->partition()->currentDataDir() != 0) { // one more try
            ierr = const_cast<ibis::roster*>(this)->read((const char*)0);
        }
        else {
            ierr = -1;
        }
        if (ierr < 0 || ind.size() != nrows) {
            return ierr;
        }
    }

    std::string evt;
    if (ibis::gVerbose > 3) {
        evt = "roster[";
        evt += col->fullname();
        evt += "]::oocSearch<";
        evt += typeid(T).name();
        evt += '>';
    }
    else {
        evt = "roster::oocSearch";
    }
    std::string fname = col->partition()->currentDataDir();
    fname += FASTBIT_DIRSEP;
    fname += col->name();
    int len = fname.size();
    fname += ".srt";
    LOGGER(ibis::gVerbose > 4)
        << evt << " attempt to read the content of " << fname
        << " to locate " << vals.size() << " value"
        << (vals.size()>1?"s":"");

    int srtdes = UnixOpen(fname.c_str(), OPEN_READONLY);
    if (srtdes < 0) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " failed to open the file "
            << fname;
        return -5;
    }
    IBIS_BLOCK_GUARD(UnixClose, srtdes);
#if defined(_WIN32) && defined(_MSC_VER)
    (void)_setmode(srtdes, _O_BINARY);
#endif

    uint32_t iv = 0; // index for vals
    uint32_t ir = 0; // index for the rows to be read
    const unsigned int tbytes = sizeof(T);

    ibis::fileManager::buffer<T> mybuf;
    char *cbuf = reinterpret_cast<char*>(mybuf.address());
    const uint32_t ncbuf = tbytes * mybuf.size();
    const uint32_t nbuf = mybuf.size();
    if (nbuf > 0 && ind.size() == nrows) {
        // each read operation fills the buffer, use in-memory ind array
        while (iv < nvals && ir < nrows) {
            ierr = UnixRead(srtdes, cbuf, ncbuf);
            if (ierr < static_cast<int>(tbytes)) {
                return -6;
            }

            const T* curr = reinterpret_cast<const T*>(cbuf);
            const T* end = curr + ierr / tbytes;
            while (curr < end) {
                while (iv < nvals && vals[iv] < *curr)
                    ++ iv;
                if (iv >= nvals) {
                    return 0;
                }
                while (curr < end && vals[iv] > *curr) {
                    ++ curr;
                    ++ ir;
                }
                while (curr < end && vals[iv] == *curr) {
                    pos.push_back(ind[ir]);
                    ++ curr;
                    ++ ir;
                }
            }
        }

        LOGGER(ibis::gVerbose > 4)
            << evt << " read the content of " << fname
            << " and found " << pos.size() << " match"
            << (pos.size()>1?"es":"");
        return (pos.size() > 0);
    }

    if (inddes < 0) {
        fname.erase(len);
        fname += ".ind";
        inddes = UnixOpen(fname.c_str(), OPEN_READONLY);
        if (inddes < 0) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- " << evt << " failed to open index file "
                << fname;
            return -7;
        }
    }
#if defined(_WIN32) && defined(_MSC_VER)
    (void)_setmode(inddes, _O_BINARY);
#endif
    if (nbuf > 0 && inddes > 0) {
        // bulk read, also need to read ind array
        while (iv < nvals && ir < nrows) {
            ierr = UnixRead(srtdes, cbuf, ncbuf);
            if (ierr < static_cast<int>(tbytes)) {
                return -8;
            }

            const T* curr = reinterpret_cast<const T*>(cbuf);
            const T* end = curr + ierr / tbytes;
            while (curr < end) {
                while (iv < nvals && vals[iv] < *curr)
                    ++ iv;
                if (iv >= nvals) {
                    return 0;
                }
                while (curr < end && vals[iv] > *curr) {
                    ++ curr;
                    ++ ir;
                }
                while (curr < end && vals[iv] == *curr) {
                    uint32_t tmp;
                    ierr = UnixSeek(inddes, ir*sizeof(tmp), SEEK_SET);
                    ierr = UnixRead(inddes, &tmp, sizeof(tmp));
                    if (ierr <= 0) {
                        LOGGER(ibis::gVerbose > 1)
                            << "Warning -- " << evt << " failed to read "
                            "index value # " << ir;
                        return -9;
                    }
                    pos.push_back(tmp);
                    ++ curr;
                    ++ ir;
                }
            }
        }
    }
    else { // read one value at a time, very slow!
        cbuf = 0;

        T curr;
        ierr = UnixRead(srtdes, &curr, tbytes);
        if (ierr < static_cast<int>(tbytes)) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- " << evt << " failed to read value # "
                << ir << " from the sorted file";
            return -10;
        }

        while (iv < nvals && ir < nrows) {
            while (iv < nvals && vals[iv] < curr)
                ++ iv;
            if (iv >= nvals)
                return 0;

            while (ir < nrows && vals[iv] > curr) {
                ierr = UnixRead(srtdes, &curr, tbytes);
                if (ierr < static_cast<int>(tbytes)) {
                    LOGGER(ibis::gVerbose > 1)
                        << "Warning -- " << evt << " failed to read value # "
                        << ir << " from the sorted file";
                    return -11;
                }
                ++ ir;
            }
            while (ir < nrows && vals[iv] == curr) {
                if (ind.size() == nrows) {
                    pos.push_back(ind[ir]);
                }
                else {
                    uint32_t tmp;
                    ierr = UnixSeek(inddes, ir*sizeof(tmp), SEEK_SET);
                    ierr = UnixRead(inddes, &tmp, sizeof(tmp));
                    if (ierr <= 0) {
                        LOGGER(ibis::gVerbose > 1)
                            << "Warning -- " << evt << " failed to read "
                            "index value #" << ir;
                        return -12;
                    }
                    pos.push_back(tmp);
                }
                ierr = UnixRead(srtdes, &curr, tbytes);
                if (ierr < static_cast<int>(tbytes)) {
                    LOGGER(ibis::gVerbose > 1)
                        << "Warning -- " << evt << " failed to read "
                        "value #" << ir << " from the sorted file";
                    return -13;
                }
                ++ ir;
            }
        }
    }

    LOGGER(ibis::gVerbose > 4)
        << evt << " read the content of " << fname
        << " and found " << pos.size() << " match"
        << (pos.size()>1?"es":"");
    return (pos.size() > 0);
} // ibis::roster::oocSearch

/// Error code:
/// - -1: incorrect type of @c vals.
/// - -2: internal error, no column associated with the @c roster object.
/// - -3: failed both in-core and out-of-core search operations.
template <typename T> int
ibis::roster::locate(const ibis::array_t<T>& vals,
                     std::vector<uint32_t>& positions) const {
    int ierr = 0;
    if (col == 0 || (ind.size() != col->partition()->nRows() && inddes < 0)) {
        ierr = -2;
        return ierr;
    }
    if (col->elementSize() != static_cast<int>(sizeof(T))) {
        ierr = -1;
        return ierr;
    }

    positions.clear();
    ierr = icSearch(vals, positions);
    if (ierr < 0) {
        LOGGER(ibis::gVerbose > 1)
            << "roster[" << col->partition()->name() << "." << col->name()
            << "]::locate<" << typeid(T).name() << ">(" << vals.size()
            << ") failed icSearch with ierr = " << ierr
            << ", attempting oocSearch";

        positions.clear();
        ierr = oocSearch(vals, positions);
        if (ierr < 0) {
            LOGGER(ibis::gVerbose >= 0)
                << "roster[" << col->partition()->name() << "." << col->name()
                << "]::locate<" << typeid(T).name() << ">("
                << vals.size() << ") failed oocSearch with ierr = " << ierr;
            return -3;
        }
    }
    return ierr;
} // ibis::roster::locate

/// This explicit specialization of the locate function does not require
/// column type to match the incoming data type.  Instead, it casts the
/// incoming data type explicitly before performing any comparisons.
template <> int
ibis::roster::locate(const ibis::array_t<double>& vals,
                     ibis::bitvector& positions) const {
    int ierr = 0;
    if (col == 0 || (ind.size() != col->partition()->nRows() && inddes < 0)) {
        ierr = -2;
        return ierr;
    }

    std::string evt;
    if (ibis::gVerbose > 1) {
        std::ostringstream oss;
        oss << "roster[" << col->partition()->name() << '.' << col->name()
            << "]::locate<double>(" << vals.size() << ')';
        evt = oss.str();
    }
    else {
        evt = "roster::locate";
    }
    ibis::util::timer mytime(evt.c_str(), 3);
    std::vector<uint32_t> ipos; // integer positions
    switch (col->type()) {
    default: {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- " << evt << " -- no roster list for column type "
            << ibis::TYPESTRING[static_cast<int>(col->type())];
        break;}
    case ibis::BYTE: {
        ierr = locate2<double, signed char>(vals, ipos);
        break;}
    case ibis::UBYTE: {
        ierr = locate2<double, unsigned char>(vals, ipos);
        break;}
    case ibis::SHORT: {
        ierr = locate2<double, int16_t>(vals, ipos);
        break;}
    case ibis::USHORT: {
        ierr = locate2<double, uint16_t>(vals, ipos);
        break;}
    case ibis::INT: {
        ierr = locate2<double, int32_t>(vals, ipos);
        break;}
    case ibis::UINT: {
        ierr = locate2<double, uint32_t>(vals, ipos);
        break;}
    case ibis::LONG: {
        ierr = locate2<double, int64_t>(vals, ipos);
        break;}
    case ibis::ULONG: {
        ierr = locate2<double, uint64_t>(vals, ipos);
        break;}
    case ibis::FLOAT: {
        ierr = locate2<double, float>(vals, ipos);
        break;}
    case ibis::DOUBLE: {
        ierr = locate<double>(vals, ipos);
        break;}
    }

    if (ipos.size() >= (col->partition()->nRows() >> 7)) {
        positions.set(0, col->partition()->nRows());
        positions.decompress();
        for (std::vector<uint32_t>::const_iterator it = ipos.begin();
             it != ipos.end(); ++ it)
            positions.setBit(*it, 1);
    }
    else {
        std::sort(ipos.begin(), ipos.end());
        for (std::vector<uint32_t>::const_iterator it = ipos.begin();
             it != ipos.end(); ++ it)
            positions.setBit(*it, 1);
        positions.adjustSize(0, col->partition()->nRows());
    }

    return ierr;
} // ibis::roster::locate

/// Cast the incoming values into the type of the column (myT) and then
/// locate the positions of the records that match one of the values.
template <typename inT, typename myT> int
ibis::roster::locate2(const ibis::array_t<inT>& vals,
                      std::vector<uint32_t>& positions) const {
    int ierr;
    if (std::strcmp(typeid(inT).name(), typeid(myT).name()) != 0) {
        std::vector<myT> myvals; // copy values to the correct type
        myvals.reserve(vals.size());
        for (uint32_t j = 0; j < vals.size(); ++ j) {
            myT tmp = static_cast<myT>(vals[j]);
            if (static_cast<inT>(tmp) == vals[j])
                myvals.push_back(tmp);
        }
        ierr = locate<myT>(myvals, positions);
    }
    else {
        ierr = locate<inT>(vals, positions);
    }
    return ierr;
} // ibis::roster::locate2

template <typename T> int
ibis::roster::locate(const ibis::array_t<T>& vals,
                     ibis::bitvector& positions) const {
    int ierr = 0;
    if (col == 0 || (ind.size() != col->partition()->nRows() && inddes < 0)) {
        ierr = -2;
        return ierr;
    }
    if (col->elementSize() != static_cast<int>(sizeof(T))) {
        ierr = -1;
        return ierr;
    }
    positions.clear();
    if (vals.empty())
        return ierr;

    std::string evt;
    if (ibis::gVerbose > 1) {
        std::ostringstream oss;
        oss << "roster[" << col->partition()->name() << '.' << col->name()
            << "]::locate<" << typeid(T).name()<< ">("
            << vals.size() << ')';
        evt = oss.str();
    }
    else {
        evt = "roster::locate";
    }
    ibis::util::timer mytime(evt.c_str(), 3);
    std::vector<uint32_t> ipos; // integer positions
    ierr = icSearch(vals, ipos);
    if (ierr < 0) {
        LOGGER(ibis::gVerbose > 1)
            << evt << " failed icSearch with ierr = " << ierr
            << ", attempting oocSearch";

        ipos.clear();
        ierr = oocSearch(vals, ipos);
        if (ierr < 0) {
            LOGGER(ibis::gVerbose >= 0)
                << "Warning -- " << evt << " failed oocSearch with ierr = "
                << ierr;
            return -3;
        }
    }

    if (ipos.size() >= (col->partition()->nRows() >> 7)) {
        positions.set(0, col->partition()->nRows());
        positions.decompress();
        for (std::vector<uint32_t>::const_iterator it = ipos.begin();
             it != ipos.end(); ++ it)
            positions.setBit(*it, 1);
    }
    else {
        std::sort(ipos.begin(), ipos.end());
        for (std::vector<uint32_t>::const_iterator it = ipos.begin();
             it != ipos.end(); ++ it)
            positions.setBit(*it, 1);
        positions.adjustSize(0, col->partition()->nRows());
    }

    return ierr;
} // ibis::roster::locate

/// Error code:
/// - -1: incorrect type of @c vals.
/// - -2: internal error, no column associated with the @c roster object.
/// - -3: failed both in-core and out-of-core search operations.
template <typename T> int
ibis::roster::locate(const std::vector<T>& vals,
                     std::vector<uint32_t>& positions) const {
    int ierr = 0;
    if (col == 0 || (ind.size() != col->partition()->nRows() && inddes < 0)) {
        ierr = -2;
        return ierr;
    }
    if (col->elementSize() != static_cast<int>(sizeof(T))) {
        ierr = -1;
        return ierr;
    }

    std::string evt;
    if (ibis::gVerbose > 1) {
        std::ostringstream oss;
        oss << "roster[" << col->partition()->name() << '.' << col->name()
            << "]::locate<" << typeid(T).name()<< ">(" << vals.size() << ')';
        evt = oss.str();
    }
    else {
        evt = "roster::locate";
    }
    positions.clear();
    ierr = icSearch(vals, positions);
    if (ierr < 0) {
        LOGGER(ibis::gVerbose > 1)
            << evt << " failed icSearch with ierr = " << ierr
            << ", attempting oocSearch";

        positions.clear();
        ierr = oocSearch(vals, positions);
        if (ierr < 0) {
            LOGGER(ibis::gVerbose >= 0)
                << "Warning -- " << evt << " failed oocSearch with ierr = "
                << ierr;
            return -3;
        }
    }
    return ierr;
} // ibis::roster::locate

template <typename T> int
ibis::roster::locate(const std::vector<T>& vals,
                     ibis::bitvector& positions) const {
    int ierr = 0;
    if (vals.empty())
        return ierr;
    if (col == 0 || (ind.size() != col->partition()->nRows() && inddes < 0)) {
        ierr = -2;
        return ierr;
    }
    if (col->elementSize() != static_cast<int>(sizeof(T))) {
        ierr = -1;
        return ierr;
    }
    positions.clear();

    std::string evt;
    if (ibis::gVerbose > 1) {
        std::ostringstream oss;
        oss << "roster[" << col->fullname()
            << "]::locate<" << typeid(T).name()<< ">(" << vals.size() << ')';
        evt = oss.str();
    }
    else {
        evt = "roster::locate";
    }
    ibis::util::timer mytime(evt.c_str(), 3);
    std::vector<uint32_t> ipos; // integer positions
    ierr = icSearch(vals, ipos);
    if (ierr < 0) {
        LOGGER(ibis::gVerbose > 1)
            << evt << " failed icSearch with ierr = " << ierr
            << ", attempting oocSearch";

        ipos.clear();
        ierr = oocSearch(vals, ipos);
        if (ierr < 0) {
            LOGGER(ibis::gVerbose >= 0)
                << "Warning -- " << evt << " failed oocSearch with ierr = "
                << ierr;
            return -3;
        }
    }

    if (ipos.size() >= (col->partition()->nRows() >> 7)) {
        positions.set(0, col->partition()->nRows());
        positions.decompress();
        for (std::vector<uint32_t>::const_iterator it = ipos.begin();
             it != ipos.end(); ++ it)
            positions.setBit(*it, 1);
    }
    else {
        std::sort(ipos.begin(), ipos.end());
        for (std::vector<uint32_t>::const_iterator it = ipos.begin();
             it != ipos.end(); ++ it)
            positions.setBit(*it, 1);
        positions.adjustSize(0, col->partition()->nRows());
    }

    return ierr;
} // ibis::roster::locate

/// Cast the incoming values into the type of the column (myT) and then
/// locate the positions of the records that match one of the values.
template <typename inT, typename myT> int
ibis::roster::locate2(const std::vector<inT>& vals,
                      std::vector<uint32_t>& positions) const {
    int ierr;
    if (std::strcmp(typeid(inT).name(), typeid(myT).name()) != 0) {
        std::vector<myT> myvals; // copy values to the correct type
        myvals.reserve(vals.size());
        for (uint32_t j = 0; j < vals.size(); ++ j) {
            myT tmp = static_cast<myT>(vals[j]);
            if (static_cast<inT>(tmp) == vals[j])
                myvals.push_back(tmp);
        }
        ierr = locate<myT>(myvals, positions);
    }
    else {
        ierr = locate<inT>(vals, positions);
    }
    return ierr;
} // ibis::roster::locate2

/// This explicit specialization of the locate function does not require
/// column type to match the incoming data type.  Instead, it casts the
/// incoming data type explicitly before performing any comparisons.
template <> int
ibis::roster::locate(const std::vector<double>& vals,
                     ibis::bitvector& positions) const {
    int ierr = 0;
    if (col == 0 || (ind.size() != col->partition()->nRows() && inddes < 0)) {
        ierr = -2;
        return ierr;
    }

    std::string evt;
    if (ibis::gVerbose >= 0) {
        std::ostringstream oss;
        oss << "roster[" << col->fullname()
            << "]::locate<double>(" << vals.size() << ')';
        evt = oss.str();
    }
    ibis::util::timer mytime(evt.c_str(), 3);
    std::vector<uint32_t> ipos; // integer positions
    switch (col->type()) {
    default: {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- " << evt << " -- no roster list for column type "
            << ibis::TYPESTRING[static_cast<int>(col->type())];
        break;}
    case ibis::BYTE: {
        ierr = locate2<double, signed char>(vals, ipos);
        break;}
    case ibis::UBYTE: {
        ierr = locate2<double, unsigned char>(vals, ipos);
        break;}
    case ibis::SHORT: {
        ierr = locate2<double, int16_t>(vals, ipos);
        break;}
    case ibis::USHORT: {
        ierr = locate2<double, uint16_t>(vals, ipos);
        break;}
    case ibis::INT: {
        ierr = locate2<double, int32_t>(vals, ipos);
        break;}
    case ibis::UINT: {
        ierr = locate2<double, uint32_t>(vals, ipos);
        break;}
    case ibis::LONG: {
        ierr = locate2<double, int64_t>(vals, ipos);
        break;}
    case ibis::ULONG: {
        ierr = locate2<double, uint64_t>(vals, ipos);
        break;}
    case ibis::FLOAT: {
        ierr = locate2<double, float>(vals, ipos);
        break;}
    case ibis::DOUBLE: {
        ierr = locate<double>(vals, ipos);
        break;}
    }

    if (ipos.size() >= (col->partition()->nRows() >> 7)) {
        positions.set(0, col->partition()->nRows());
        positions.decompress();
        for (std::vector<uint32_t>::const_iterator it = ipos.begin();
             it != ipos.end(); ++ it)
            positions.setBit(*it, 1);
    }
    else {
        std::sort(ipos.begin(), ipos.end());
        for (std::vector<uint32_t>::const_iterator it = ipos.begin();
             it != ipos.end(); ++ it)
            positions.setBit(*it, 1);
        positions.adjustSize(0, col->partition()->nRows());
    }

    return ierr;
} // ibis::roster::locate

// explicit template instantiation
template long ibis::roster::mergeBlock2(const char*, const char*,
                                        const uint32_t,
                                        array_t<ibis::rid_t>&,
                                        array_t<ibis::rid_t>&,
                                        array_t<ibis::rid_t>&);
template int
ibis::roster::locate(const std::vector<unsigned char>& vals,
                     std::vector<uint32_t>& positions) const;
template int
ibis::roster::locate(const std::vector<signed char>& vals,
                     std::vector<uint32_t>& positions) const;
template int
ibis::roster::locate(const std::vector<uint16_t>& vals,
                     std::vector<uint32_t>& positions) const;
template int
ibis::roster::locate(const std::vector<int16_t>& vals,
                     std::vector<uint32_t>& positions) const;
template int
ibis::roster::locate(const std::vector<uint32_t>& vals,
                     std::vector<uint32_t>& positions) const;
template int
ibis::roster::locate(const std::vector<int32_t>& vals,
                     std::vector<uint32_t>& positions) const;
template int
ibis::roster::locate(const std::vector<uint64_t>& vals,
                     std::vector<uint32_t>& positions) const;
template int
ibis::roster::locate(const std::vector<int64_t>& vals,
                     std::vector<uint32_t>& positions) const;
template int
ibis::roster::locate(const std::vector<float>& vals,
                     std::vector<uint32_t>& positions) const;
template int
ibis::roster::locate(const std::vector<double>& vals,
                     std::vector<uint32_t>& positions) const;
template int
ibis::roster::locate(const std::vector<unsigned char>& vals,
                     ibis::bitvector& positions) const;
template int
ibis::roster::locate(const std::vector<signed char>& vals,
                     ibis::bitvector& positions) const;
template int
ibis::roster::locate(const std::vector<uint16_t>& vals,
                     ibis::bitvector& positions) const;
template int
ibis::roster::locate(const std::vector<int16_t>& vals,
                     ibis::bitvector& positions) const;
template int
ibis::roster::locate(const std::vector<uint32_t>& vals,
                     ibis::bitvector& positions) const;
template int
ibis::roster::locate(const std::vector<int32_t>& vals,
                     ibis::bitvector& positions) const;
template int
ibis::roster::locate(const std::vector<uint64_t>& vals,
                     ibis::bitvector& positions) const;
template int
ibis::roster::locate(const std::vector<int64_t>& vals,
                     ibis::bitvector& positions) const;
template int
ibis::roster::locate(const std::vector<float>& vals,
                     ibis::bitvector& positions) const;
template int
ibis::roster::locate(const ibis::array_t<unsigned char>& vals,
                     std::vector<uint32_t>& positions) const;
template int
ibis::roster::locate(const ibis::array_t<signed char>& vals,
                     std::vector<uint32_t>& positions) const;
template int
ibis::roster::locate(const ibis::array_t<uint16_t>& vals,
                     std::vector<uint32_t>& positions) const;
template int
ibis::roster::locate(const ibis::array_t<int16_t>& vals,
                     std::vector<uint32_t>& positions) const;
template int
ibis::roster::locate(const ibis::array_t<uint32_t>& vals,
                     std::vector<uint32_t>& positions) const;
template int
ibis::roster::locate(const ibis::array_t<int32_t>& vals,
                     std::vector<uint32_t>& positions) const;
template int
ibis::roster::locate(const ibis::array_t<uint64_t>& vals,
                     std::vector<uint32_t>& positions) const;
template int
ibis::roster::locate(const ibis::array_t<int64_t>& vals,
                     std::vector<uint32_t>& positions) const;
template int
ibis::roster::locate(const ibis::array_t<float>& vals,
                     std::vector<uint32_t>& positions) const;
template int
ibis::roster::locate(const ibis::array_t<double>& vals,
                     std::vector<uint32_t>& positions) const;
template int
ibis::roster::locate(const ibis::array_t<unsigned char>& vals,
                     ibis::bitvector& positions) const;
template int
ibis::roster::locate(const ibis::array_t<signed char>& vals,
                     ibis::bitvector& positions) const;
template int
ibis::roster::locate(const ibis::array_t<uint16_t>& vals,
                     ibis::bitvector& positions) const;
template int
ibis::roster::locate(const ibis::array_t<int16_t>& vals,
                     ibis::bitvector& positions) const;
template int
ibis::roster::locate(const ibis::array_t<uint32_t>& vals,
                     ibis::bitvector& positions) const;
template int
ibis::roster::locate(const ibis::array_t<int32_t>& vals,
                     ibis::bitvector& positions) const;
template int
ibis::roster::locate(const ibis::array_t<uint64_t>& vals,
                     ibis::bitvector& positions) const;
template int
ibis::roster::locate(const ibis::array_t<int64_t>& vals,
                     ibis::bitvector& positions) const;
template int
ibis::roster::locate(const ibis::array_t<float>& vals,
                     ibis::bitvector& positions) const;
