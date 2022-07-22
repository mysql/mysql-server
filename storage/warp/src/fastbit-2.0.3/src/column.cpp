//File: $Id$
// Author: John Wu <John.Wu at ACM.org>
// Copyright (c) 2000-2016 the Regents of the University of California
//
// This file contains implementation of the functions defined in column.h
//
#include "resource.h"   // ibis::resource, ibis::gParameters()
#include "category.h"   // ibis::text, ibis::category
#include "column.h"     // ibis::column
#include "part.h"       // ibis::part
#include "iroster.h"    // ibis::roster
#include "irelic.h"     // ibis::relic
#include "ibin.h"       // ibis::bin

#include <stdarg.h>     // vsprintf
#include <ctype.h>      // tolower
#include <math.h>       // log

#include <limits>       // std::numeric_limits
#include <typeinfo>     // typeid
#include <memory>       // std::unique_ptr

#if defined(_WIN32) && defined(_MSC_VER)
#pragma warning(disable:4786)   // some identifier longer than 256 characters
// needed for numeric_limits<>::max, min function calls
#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif
#endif

#define FASTBIT_SYNC_WRITE 1

// constants defined for type name and type code used in the metadata file
static const char* _ibis_TYPESTRING_local[] = {
    "UNKNOWN", "OID", "BYTE", "UBYTE", "SHORT", "USHORT", "INT", "UINT",
    "LONG", "ULONG", "FLOAT", "DOUBLE", "BIT", "CATEGORY", "TEXT", "BLOB", "UDT"
};
FASTBIT_CXX_DLLSPEC const char** ibis::TYPESTRING = _ibis_TYPESTRING_local;

/// Construct a new column object based on type and name.
ibis::column::column(const ibis::part* tbl, ibis::TYPE_T t,
                     const char* name, const char* desc,
                     double low, double high) :
    thePart(tbl), m_type(t), m_name(name), m_desc(desc), m_bins(""),
    m_sorted(false), lower(low), upper(high), m_utscribe(0), dataflag(0),
    idx(0), idxcnt() {
    if (0 != pthread_rwlock_init(&rwlock, 0)) {
        throw "column::ctor failed to initialize the rwlock" IBIS_FILE_LINE;
    }
    if (0 != pthread_mutex_init
        (&mutex, static_cast<const pthread_mutexattr_t*>(0))) {
        throw "column::ctor failed to initialize the mutex" IBIS_FILE_LINE;
    }
    if (m_desc.empty()) m_desc = name;
    if (ibis::gVerbose > 5 && !m_name.empty()) {
        ibis::util::logger lg;
        lg() << "initialized column " << fullname() << " @ "
             << this << " (" << ibis::TYPESTRING[(int)m_type] << ')';
    }
    if (thePart == 0) {
        (void) ibis::fileManager::instance();
    }
} // ibis::column::column

/// Reconstitute a column from the content of a file.
/// Read the basic information about a column from file.
///
///@note
/// Assume the calling program has read "Begin Property/Column" already.
///
///@note
/// A well-formed column must have a valid name, i.e., ! m_name.empty().
ibis::column::column(const part* tbl, FILE* file)
    : thePart(tbl), m_type(UINT), m_sorted(false), lower(DBL_MAX),
      upper(-DBL_MAX), m_utscribe(0), dataflag(0), idx(0), idxcnt() {
    char buf[MAX_LINE];
    char *s1;
    char *s2;

    if (0 != pthread_rwlock_init(&rwlock, 0)) {
        throw "column::ctor failed to initialize the rwlock" IBIS_FILE_LINE;
    }
    if (0 != pthread_mutex_init
        (&mutex, static_cast<const pthread_mutexattr_t *>(0))) {
        throw "column::ctor failed to initialize the mutex" IBIS_FILE_LINE;
    }
    if (thePart == 0) {
        (void) ibis::fileManager::instance();
    }

    bool badType = false;
    // read the column entry of the metadata file
    // assume the calling program has read "Begin Property/Column" already
    do {
        s1 = fgets(buf, MAX_LINE, file);
        if (s1 == 0) {
            ibis::util::logMessage("Warning", "column::ctor reached "
                                   "end-of-file while reading a column");
            return;
        }
        if (std::strlen(buf) + 1 >= MAX_LINE) {
            ibis::util::logMessage("Warning", "column::ctor may "
                                   "have encountered a line that has more "
                                   "than %d characters", MAX_LINE);
        }

        s1 = strchr(buf, '=');
        if (s1!=0 && s1[1]!=static_cast<char>(0)) ++s1;
        else s1 = 0;

        if (buf[0] == '#') {
            // skip the comment line
        }
        else if (strnicmp(buf, "name", 4) == 0 ||
                 strnicmp(buf, "Property_name", 13) == 0) {
            s2 = ibis::util::getString(s1);
            m_name = s2;
            delete [] s2;
        }
        else if (strnicmp(buf, "description", 11) == 0 ||
                 strnicmp(buf, "Property_description", 20) == 0) {
            s2 = ibis::util::getString(s1);
            m_desc = s2;
            delete [] s2;
        }
        else if (strnicmp(buf, "minimum", 7) == 0) {
            s1 += strspn(s1, " \t=\'\"");
            lower = strtod(s1, 0);
        }
        else if (strnicmp(buf, "maximum", 7) == 0) {
            s1 += strspn(s1, " \t=\'\"");
            upper = strtod(s1, 0);
        }
        else if (strnicmp(buf, "Bins:", 5) == 0) {
            s1 = buf + 5;
            s1 += strspn(s1, " \t");
            s2 = s1 + std::strlen(s1) - 1;
            while (s2>=s1 && isspace(*s2)) {
                *s2 = static_cast<char>(0);
                --s2;
            }
#if defined(INDEX_SPEC_TO_LOWER)
            s2 = s1 + std::strlen(s1) - 1;
            while (s2 >= s1) {
                *s2 = tolower(*s2);
                -- s2;
            }
#endif
            m_bins = s1;
        }
        else if (strnicmp(buf, "Index", 5) == 0) {
            s1 = ibis::util::getString(s1);
#if defined(INDEX_SPEC_TO_LOWER)
            s2 = s1 + std::strlen(s1) - 1;
            while (s2 >= s1) {
                *s2 = tolower(*s2);
                -- s2;
            }
#endif
            m_bins = s1;
            delete [] s1;
        }
        else if (strnicmp(buf, "sorted", 6) == 0 && s1 != 0 && *s1 != 0) {
            while (s1 != 0 && *s1 != 0 && isspace(*s1))
                ++ s1;
            if (s1 != 0 && *s1 != 0)
                m_sorted = ibis::resource::isStringTrue(s1);
        }
        else if (strnicmp(buf, "Property_data_type", 18) == 0 ||
                 strnicmp(buf, "data_type", 9) == 0 ||
                 strnicmp(buf, "type", 4) == 0) {
            s1 += strspn(s1, " \t=\'\"");

            switch (*s1) {
            case 'i':
            case 'I': { // can only be INT
                m_type = ibis::INT;
                break;}
            case 'u':
            case 'U': { // likely unsigned type, but maybe UNKNOWN or UDT
                m_type = ibis::UNKNOWN_TYPE;
                if (s1[1] == 's' || s1[1] == 'S') { // USHORT
                    m_type = ibis::USHORT;
                }
                else if (s1[1] == 'b' || s1[1] == 'B' ||
                         s1[1] == 'c' || s1[1] == 'C') { // UBYTE
                    m_type = ibis::UBYTE;
                }
                else if (s1[1] == 'i' || s1[1] == 'I') { // UINT
                    m_type = ibis::UINT;
                }
                else if (s1[1] == 'l' || s1[1] == 'L') { // ULONG
                    m_type = ibis::ULONG;
                }
                else if (s1[1] == 'd' || s1[1] == 'd') { // UDT
                    m_type = ibis::UDT;
                }
                else if (strnicmp(s1, "unsigned", 8) == 0) { // unsigned xx
                    s1 += 8; // skip "unsigned"
                    s1 += strspn(s1, " \t=\'\""); // skip space
                    if (*s1 == 's' || *s1 == 'S') { // USHORT
                        m_type = ibis::USHORT;
                    }
                    else if (*s1 == 'b' || *s1 == 'B' ||
                             *s1 == 'c' || *s1 == 'C') { // UBYTE
                        m_type = ibis::UBYTE;
                    }
                    else if (*s1 == 0 || *s1 == 'i' || *s1 == 'I') { // UINT
                        m_type = ibis::UINT;
                    }
                    else if (*s1 == 'l' || *s1 == 'L') { // ULONG
                        m_type = ibis::ULONG;
                    }
                }
                break;}
            case 'r':
            case 'R': { // FLOAT
                m_type = ibis::FLOAT;
                break;}
            case 'f':
            case 'F': {// FLOAT
                m_type = ibis::FLOAT;
                break;}
            case 'd':
            case 'D': { // DOUBLE
                m_type = ibis::DOUBLE;
                break;}
            case 'c':
            case 'C':
            case 'k':
            case 'K': { // KEY
                m_type = ibis::CATEGORY;
                break;}
            case 's':
            case 'S': { // default to string, but could be short
                m_type = ibis::TEXT;
                if (s1[1] == 'h' || s1[1] == 'H')
                    m_type = ibis::SHORT;
                break;}
            case 't':
            case 'T': {
                m_type = ibis::TEXT;
                break;}
            case 'a':
            case 'A': { // UBYTE
                m_type = ibis::UBYTE;
                break;}
            case 'b':
            case 'B': { // BYTE/BIT/BLOB
                if (s1[1] == 'l' || s1[1] == 'L')
                    m_type = ibis::BLOB;
                else if (s1[1] == 'i' || s1[1] == 'I')
                    m_type = ibis::BIT;
                else
                    m_type = ibis::BYTE;
                break;}
            case 'g':
            case 'G': { // USHORT
                m_type = ibis::USHORT;
                break;}
            case 'H':
            case 'h': { // short, half word
                m_type = ibis::SHORT;
                break;}
            case 'l':
            case 'L': { // LONG (int64_t)
                m_type = ibis::LONG;
                break;}
            case 'v':
            case 'V': { // unsigned long (uint64_t)
                m_type = ibis::ULONG;
                break;}
            case 'q':
            case 'Q': { // BLOB
                m_type = ibis::BLOB;
                break;}
            default: {
                LOGGER(ibis::gVerbose > 1)
                    << "Warning -- column::ctor encountered "
                    "unknown data type \"" << s1 << "\"";
                badType = true;
                break;}
            }
        }
        else if (strnicmp(buf, "End", 3) && ibis::gVerbose > 4){
            ibis::util::logMessage("column::column",
                                   "skipping line:\n%s", buf);
        }
    } while (strnicmp(buf, "End", 3));

    if (m_name.empty() || badType) {
        ibis::util::logMessage("Warning",
                               "column specification does not have a "
                               "valid name or type");
        m_name.erase(); // make sure the name is empty
    }
    if (ibis::gVerbose > 5 && !m_name.empty()) {
        ibis::util::logger lg;
        lg() << "read info about column " << fullname() << " @ " << this
             << " (" << ibis::TYPESTRING[(int)m_type] << ')';
    }
} // ibis::column::column

/// The copy constructor.
///
/// @warning The rwlock can not be copied.
///
/// @warning The index is duplicated.
///
/// @note this function is only used to copy a column without an index
/// object and has not been used due to the above two limitations.
ibis::column::column(const ibis::column& rhs) :
    thePart(rhs.thePart), mask_(rhs.mask_), m_type(rhs.m_type),
    m_name(rhs.m_name), m_desc(rhs.m_desc), m_bins(rhs.m_bins),
    m_sorted(rhs.m_sorted), lower(rhs.lower), upper(rhs.upper),
    m_utscribe(rhs.m_utscribe), dataflag(0),
    idx(rhs.idx!=0 ? rhs.idx->dup() : 0), idxcnt() {
    if (pthread_rwlock_init(&rwlock, 0)) {
        throw "column::ctor failed to initialize the rwlock" IBIS_FILE_LINE;
    }
    if (pthread_mutex_init(&mutex, 0)) {
        throw "column::ctor failed to initialize the mutex" IBIS_FILE_LINE;
    }
    if (thePart == 0) {
        (void) ibis::fileManager::instance();
    }
    if (ibis::gVerbose > 5 && !m_name.empty()) {
        ibis::util::logger lg;
        lg() << "made a new copy of column " << fullname() << " @ " << this
             << " (" << ibis::TYPESTRING[(int)m_type] << ')';
    }
} // copy constructor

/// Destructor.  It acquires a write lock to make sure all other operations
/// have completed.
ibis::column::~column() {
    LOGGER(ibis::gVerbose > 5 && !m_name.empty())
        << "clearing column " << fullname() << " @ " << this;
    { // must not be used for anything else
        writeLock wk(this, "~column");
        delete idx;
    }

    pthread_mutex_destroy(&mutex);
    pthread_rwlock_destroy(&rwlock);
} // destructor

/// Write the current content to the metadata file -part.txt of the data
/// partition.
void ibis::column::write(FILE* file) const {
    fputs("\nBegin Column\n", file);
    fprintf(file, "name = \"%s\"\n", (const char*)m_name.c_str());
    if (! m_desc.empty()) {
        if (m_desc.size() > MAX_LINE-60)
            const_cast<std::string&>(m_desc).erase(MAX_LINE-60);
        fprintf(file, "description =\"%s\"\n", m_desc.c_str());
    }
    fprintf(file, "data_type = \"%s\"\n", ibis::TYPESTRING[(int)m_type]);
    if (upper >= lower) {
        switch (m_type) {
        case BYTE:
        case SHORT:
        case INT:
            fprintf(file, "minimum = %ld\n", static_cast<long>(lower));
            fprintf(file, "maximum = %ld\n", static_cast<long>(upper));
            break;
        case FLOAT:
            fprintf(file, "minimum = %.8g\n", lower);
            fprintf(file, "maximum = %.8g\n", upper);
            break;
        case DOUBLE:
        case ULONG:
        case LONG:
            fprintf(file, "minimum = %.15g\n", lower);
            fprintf(file, "maximum = %.15g\n", upper);
            break;
        default: // no min/max
            break;
        case UBYTE:
        case USHORT:
        case UINT:
            fprintf(file, "minimum = %lu\n",
                    static_cast<long unsigned>(lower));
            fprintf(file, "maximum = %lu\n",
                    static_cast<long unsigned>(upper));
            break;
        }
    }
    if (! m_bins.empty())
        fprintf(file, "index = %s\n", m_bins.c_str());
    if (m_sorted)
        fprintf(file, "sorted = true\n");
    fputs("End Column\n", file);
} // ibis::column::write

/// Write the index into three arrays.
int ibis::column::indexWrite(ibis::array_t<double> &keys,
                             ibis::array_t<int64_t> &starts,
                             ibis::array_t<uint32_t> &bitmaps) const {
    if (idx != 0)
        return idx->write(keys, starts, bitmaps);
    else
        return -1;
} // ibis::column::writeIndex

/// Compute the sizes (in number of elements) of three arrays that would be
/// produced by writeIndex.
void ibis::column::indexSerialSizes(uint64_t &wkeys, uint64_t &woffsets,
                                    uint64_t &wbitmaps) const {
    if (idx != 0) {
        idx->serialSizes(wkeys, woffsets, wbitmaps);
    }
    else {
        wkeys = 0;
        woffsets = 0;
        wbitmaps = 0;
    }
} // ibis::column::indexSerialSizes

const char* ibis::column::indexSpec() const {
    return (m_bins.empty() ? (thePart ? thePart->indexSpec() : 0)
            : m_bins.c_str());
}

uint32_t ibis::column::numBins() const {
    uint32_t nBins = 0;
    //      if (idx)
    //          nBins = idx->numBins();
    if (nBins == 0) { //  read the no= field in m_bins
        const char* str = strstr(m_bins.c_str(), "no=");
        if (str == 0) {
            str = strstr(m_bins.c_str(), "NO=");
            if (str == 0) {
                str = strstr(m_bins.c_str(), "No=");
            }
            if (str == 0 && thePart != 0 && thePart->indexSpec() != 0) {
                str = strstr(thePart->indexSpec(), "no=");
                if (str == 0) {
                    str = strstr(thePart->indexSpec(), "NO=");
                    if (str == 0)
                        str = strstr(thePart->indexSpec(), "No=");
                }
            }
        }
        if (str) {
            str += 3;
            nBins = strtol(str, 0, 0);
        }
    }
    if (nBins == 0)
        nBins = 10;
    return nBins;
} // ibis::column::numBins

/// Compute the actual min/max values.  It actually goes through all the
/// values.  This function reads the data in the active data directory and
/// modifies the member variables to record the actual min/max.
void ibis::column::computeMinMax() {
    std::string sname;
    const char* name = dataFileName(sname);
    if (name != 0) {
        ibis::bitvector msk;
        getNullMask(msk);
        actualMinMax(name, msk, lower, upper, m_sorted);
    }
} // ibis::column::computeMinMax

/// Compute the actual min/max values.  It actually goes through all the
/// values.  This function reads the data in the given directory and
/// modifies the member variables to record the actual min/max.
void ibis::column::computeMinMax(const char *dir) {
    std::string sname;
    const char* name = dataFileName(sname, dir);
    ibis::bitvector msk;
    getNullMask(msk);
    actualMinMax(name, msk, lower, upper, m_sorted);
} // ibis::column::computeMinMax

/// Compute the actual min/max of the data in directory @c dir.  Report the
/// actual min/max found back through output arguments @c min and @c max.
/// This version does not modify the min/max recorded in this column
/// object.
void ibis::column::computeMinMax(const char *dir, double &min,
                                 double &max, bool &asc) const {
    std::string sname;
    const char* name = dataFileName(sname, dir);
    ibis::bitvector msk;
    getNullMask(msk);
    actualMinMax(name, msk, min, max, asc);
} // ibis::column::computeMinMax

/// Compute the actual minimum and maximum values.  Given a data file name,
/// read its content to compute the actual minimum and the maximum of the
/// data values.  Only deal with four types of values, unsigned int, signed
/// int, float and double.
void ibis::column::actualMinMax(const char *name, const ibis::bitvector& mask,
                                double &min, double &max, bool &asc) const {
    std::string evt = "column";
    if (ibis::gVerbose > 2) {
        evt += '[';
        evt += fullname();
        evt += ']';
    }
    evt += "::actualMinMax";

    switch (m_type) {
    case ibis::UBYTE: {
        array_t<unsigned char> val;
        int ierr;
        if (name != 0 && *name != 0)
            ierr = ibis::fileManager::instance().getFile(name, val);
        else
            ierr = getValuesArray(&val);
        if (ierr != 0) {
            min = DBL_MAX;
            max = -DBL_MAX;
            LOGGER(ibis::gVerbose > 3)
                << "Warning -- " << evt << "failed to retrieve file " << name;
            return;
        }

        actualMinMax(val, mask, min, max, asc);
        break;}
    case ibis::BYTE: {
        array_t<signed char> val;
        int ierr;
        if (name != 0 && *name != 0)
            ierr = ibis::fileManager::instance().getFile(name, val);
        else
            ierr = getValuesArray(&val);
        if (ierr != 0) {
            min = DBL_MAX;
            max = -DBL_MAX;
            LOGGER(ibis::gVerbose > 3)
                << "Warning -- " << evt << "failed to retrieve file " << name;
            return;
        }
        actualMinMax(val, mask, min, max, asc);
        break;}
    case ibis::USHORT: {
        array_t<uint16_t> val;
        int ierr;
        if (name != 0 && *name != 0)
            ierr = ibis::fileManager::instance().getFile(name, val);
        else
            ierr = getValuesArray(&val);
        if (ierr != 0) {
            min = DBL_MAX;
            max = -DBL_MAX;
            LOGGER(ibis::gVerbose > 3)
                << "Warning -- " << evt << "failed to retrieve file " << name;
            return;
        }

        actualMinMax(val, mask, min, max, asc);
        break;}
    case ibis::SHORT: {
        array_t<int16_t> val;
        int ierr;
        if (name != 0 && *name != 0)
            ierr = ibis::fileManager::instance().getFile(name, val);
        else
            ierr = getValuesArray(&val);
        if (ierr != 0) {
            min = DBL_MAX;
            max = -DBL_MAX;
            LOGGER(ibis::gVerbose > 3)
                << "Warning -- " << evt << "failed to retrieve file " << name;
            return;
        }

        actualMinMax(val, mask, min, max, asc);
        break;}
    case ibis::UINT: {
        array_t<uint32_t> val;
        int ierr;
        if (name != 0 && *name != 0)
            ierr = ibis::fileManager::instance().getFile(name, val);
        else
            ierr = getValuesArray(&val);
        if (ierr != 0) {
            min = DBL_MAX;
            max = -DBL_MAX;
            LOGGER(ibis::gVerbose > 3)
                << "Warning -- " << evt << "failed to retrieve file " << name;
            return;
        }

        actualMinMax(val, mask, min, max, asc);
        break;}
    case ibis::INT: {
        array_t<int32_t> val;
        int ierr;
        if (name != 0 && *name != 0)
            ierr = ibis::fileManager::instance().getFile(name, val);
        else
            ierr = getValuesArray(&val);
        if (ierr != 0) {
            min = DBL_MAX;
            max = -DBL_MAX;
            LOGGER(ibis::gVerbose > 3)
                << "Warning -- " << evt << "failed to retrieve file " << name;
            return;
        }

        actualMinMax(val, mask, min, max, asc);
        break;}
    case ibis::ULONG: {
        array_t<uint64_t> val;
        int ierr;
        if (name != 0 && *name != 0)
            ierr = ibis::fileManager::instance().getFile(name, val);
        else
            ierr = getValuesArray(&val);
        if (ierr != 0) {
            min = DBL_MAX;
            max = -DBL_MAX;
            LOGGER(ibis::gVerbose > 3)
                << "Warning -- " << evt << "failed to retrieve file " << name;
            return;
        }

        actualMinMax(val, mask, min, max, asc);
        break;}
    case ibis::LONG: {
        array_t<int64_t> val;
        int ierr;
        if (name != 0 && *name != 0)
            ierr = ibis::fileManager::instance().getFile(name, val);
        else
            ierr = getValuesArray(&val);
        if (ierr != 0) {
            min = DBL_MAX;
            max = -DBL_MAX;
            LOGGER(ibis::gVerbose > 3)
                << "Warning -- " << evt << "failed to retrieve file " << name;
            return;
        }

        actualMinMax(val, mask, min, max, asc);
        break;}
    case ibis::FLOAT: {
        array_t<float> val;
        int ierr;
        if (name != 0 && *name != 0)
            ierr = ibis::fileManager::instance().getFile(name, val);
        else
            ierr = getValuesArray(&val);
        if (ierr != 0) {
            min = DBL_MAX;
            max = -DBL_MAX;
            LOGGER(ibis::gVerbose > 3)
                << "Warning -- " << evt << "failed to retrieve file " << name;
            return;
        }

        actualMinMax(val, mask, min, max, asc);
        break;}
    case ibis::DOUBLE: {
        array_t<double> val;
        int ierr;
        if (name != 0 && *name != 0)
            ierr = ibis::fileManager::instance().getFile(name, val);
        else
            ierr = getValuesArray(&val);
        if (ierr != 0) {
            min = DBL_MAX;
            max = -DBL_MAX;
            LOGGER(ibis::gVerbose > 3)
                << "Warning -- " << evt << "failed to retrieve file " << name;
            return;
        }

        actualMinMax(val, mask, min, max, asc);
        break;}
    default:
        LOGGER(ibis::gVerbose > 2)
            << evt << " can not handle column type "
            << ibis::TYPESTRING[static_cast<int>(m_type)]
            << ", only support int, uint, float, double";
        max = -DBL_MAX;
        min = DBL_MAX;
        asc = false;
    } // switch(m_type)
} // ibis::column::actualMinMax

/// Name of the data file in the given data directory.  If the directory
/// name is not given, the directory is assumed to be the current data
/// directory of the data partition.  There is no need for the caller to
/// free the pointer returned by this function.  Upon successful completion
/// of this function, it returns fname.c_str(); otherwise, it returns the
/// nil pointer.
const char*
ibis::column::dataFileName(std::string& fname, const char *dir) const {
    if (m_name.empty())
        return 0;
    if ((dir == 0 || *dir == 0) && thePart != 0)
        dir = thePart->currentDataDir();
    if (dir == 0 || *dir == 0)
        return 0;

    fname = dir;
    bool needtail = true;
    size_t jtmp = fname.rfind(FASTBIT_DIRSEP);
    if (jtmp < fname.size() && jtmp+m_name.size() < fname.size()) {
        if (strnicmp(fname.c_str()+jtmp+1, m_name.c_str(), m_name.size())
            == 0) {
            if (fname.size() == jtmp+5+m_name.size() &&
                std::strcmp(fname.c_str()+jtmp+1+m_name.size(), ".idx") == 0) {
                fname.erase(jtmp+1+m_name.size());
                needtail = false;
            }
            needtail = (fname.size() != jtmp+1+m_name.size());
        }
    }
    if (needtail) {
        if (fname[fname.size()-1] != FASTBIT_DIRSEP)
            fname += FASTBIT_DIRSEP;
        fname += m_name;
    }
    return fname.c_str();
} // ibis::column::dataFileName

/// Name of the NULL mask file.
/// On successful completion of this function, the return value is the
/// result of fname.c_str(); otherwise the return value is a nil pointer to
/// indicate error.
const char* ibis::column::nullMaskName(std::string& fname) const {
    if (thePart == 0 || thePart->currentDataDir() == 0 || m_name.empty())
        return 0;

    fname = thePart->currentDataDir();
    fname += FASTBIT_DIRSEP;
    fname += m_name;
    fname += ".msk";
    return fname.c_str();
} // ibis::column::nullMaskName

/// If there is a null mask stored already, return a shallow copy of it in
/// mask.  Otherwise, find out the size of the data file first, if the
/// actual content of the null mask file has less bits, assume the mask is
/// for the leading portion of the data file and the remaining portion of
/// the data file is valid (not null).
void ibis::column::getNullMask(ibis::bitvector& mask) const {
    if (thePart != 0 ? (mask_.size() == thePart->nRows()) :
        (mask_.size() > 0)) {
        ibis::bitvector tmp(mask_);
        mask.swap(tmp);
#if DEBUG+0 > 0 || _DEBUG+0 > 0
        logMessage("getNullMask", "copying an existing mask(%lu, %lu)",
                   static_cast<long unsigned>(mask.cnt()),
                   static_cast<long unsigned>(mask.size()));
#endif
        return;
    }

    ibis::util::mutexLock lock(&mutex, "column::getNullMask");
    if (m_type == ibis::OID) {
        if (thePart != 0) {
            const_cast<column*>(this)->mask_.set(1, thePart->nRows());
            mask.set(1, thePart->nRows());
        }
        else {
            array_t<ibis::rid_t> vals;
            if (0 == getValuesArray(&vals)) {
                const_cast<column*>(this)->mask_.set(1, vals.size());
                mask.set(1, vals.size());
            }
        }
#if DEBUG+0 > 0 || _DEBUG+0 > 0
        logMessage("getNullMask", "asking for the mask of OIDs (%lu, %lu)",
                   static_cast<long unsigned>(mask.cnt()),
                   static_cast<long unsigned>(mask.size()));
#endif
    }
    else {
        Stat_T st;
        std::string sname;
        const char* fnm = 0;
        array_t<ibis::bitvector::word_t> arr;
        fnm = dataFileName(sname);
        if (fnm != 0 && UnixStat(fnm, &st) == 0) {
            const uint32_t elm = elementSize();
            uint32_t sz = (elm > 0 ? st.st_size / elm :  thePart->nRows());

            // get the null mask file name and read the file
            fnm = nullMaskName(sname);
            int ierr = -1;
            try {
                ierr = ibis::fileManager::instance().getFile
                    (fnm, arr, ibis::fileManager::PREFER_READ);
                if (ierr == 0) {
                    mask.copy(ibis::bitvector(arr));
                }
                else {
                    mask.set(1, sz);
                }
            }
            catch (...) {
                mask.set(1, sz);
            }

            if (mask.size() != thePart->nRows() &&
                thePart->getStateNoLocking() == ibis::part::STABLE_STATE) {
                mask.adjustSize(sz, thePart->nRows());
                ibis::fileManager::instance().flushFile(fnm);
                mask.write(fnm);
                LOGGER(ibis::gVerbose > 1)
                    << "Warning -- column[" << fullname()
                    << "]::getNullMask constructed a new mask with "
                    << mask.cnt() << " out of " << mask.size()
                    << " set bits, wrote to " << fnm;
            }
            LOGGER(ibis::gVerbose > 5)
                << "column[" << fullname()
                << "]::getNullMask -- get null mask (" << mask.cnt() << ", "
                << mask.size() << ") [st.st_size=" << st.st_size
                << ", sz=" << sz << ", ierr=" << ierr << "]";
        }
        else if (thePart != 0) { // no data file, assume every value is valid
            mask.set(1, thePart->nRows());
        }
        else {
            uint32_t sz = 0;
            switch (m_type) {
            default:
                break;
            case ibis::BYTE: {
                array_t<signed char> vals;
                getValuesArray(&vals);
                sz = vals.size();
                break;}
            case ibis::UBYTE: {
                array_t<unsigned char> vals;
                getValuesArray(&vals);
                sz = vals.size();
                break;}
            case ibis::SHORT: {
                array_t<int16_t> vals;
                getValuesArray(&vals);
                sz = vals.size();
                break;}
            case ibis::USHORT: {
                array_t<uint16_t> vals;
                getValuesArray(&vals);
                sz = vals.size();
                break;}
            case ibis::INT: {
                array_t<int32_t> vals;
                getValuesArray(&vals);
                sz = vals.size();
                break;}
            case ibis::UINT: {
                array_t<uint32_t> vals;
                getValuesArray(&vals);
                sz = vals.size();
                break;}
            case ibis::LONG: {
                array_t<int64_t> vals;
                getValuesArray(&vals);
                sz = vals.size();
                break;}
            case ibis::ULONG: {
                array_t<uint64_t> vals;
                getValuesArray(&vals);
                sz = vals.size();
                break;}
            case ibis::FLOAT: {
                array_t<float> vals;
                getValuesArray(&vals);
                sz = vals.size();
                break;}
            case ibis::DOUBLE: {
                array_t<double> vals;
                getValuesArray(&vals);
                sz = vals.size();
                break;}
            case ibis::TEXT:
            case ibis::CATEGORY: {
                std::vector<std::string> vals;
                getValuesArray(&vals);
                sz = vals.size();
                break;}
            }
            mask.set(1, sz);
        }

        ibis::bitvector tmp(mask);
        const_cast<column*>(this)->mask_.swap(tmp);
    }
    LOGGER(ibis::gVerbose > 6)
        << "column[" << fullname()
        << "]::getNullMask -- mask size = " << mask.size() << ", cnt = "
        << mask.cnt();
} // ibis::column::getNullMask

/// Change the null mask to the user specified one.  The incoming mask
/// should have as many bits as the number of rows in the data partition.
/// Upon a successful completion of this function, the return value is >=
/// 0, otherwise it is less than 0.
int ibis::column::setNullMask(const ibis::bitvector& msk) {
    if (thePart == 0 || msk.size() == thePart->nRows()) {
        ibis::util::mutexLock lock(&mutex, "column::setNullMask");
        mask_.copy(msk);
        LOGGER(ibis::gVerbose > 5)
            << "column[" << fullname() << "]::setNullMask -- mask_.size()="
            << mask_.size() << ", mask_.cnt()=" << mask_.cnt();
        return mask_.cnt();
    }
    else {
        LOGGER(ibis::gVerbose > 0)
            << "Warning - column::setNullMask expected msk.size to be "
            << thePart->nRows() << " but the actual size is "
            << msk.size();
        return -1;
    }
} // ibis::column::setNullMask

/// Return all rows of the column as an array_t object.  Caller is
/// responsible for deleting the returned object.
ibis::array_t<int32_t>* ibis::column::getIntArray() const {
    ibis::array_t<int32_t>* array = 0;
    if (dataflag < 0) {
    }
    else if (m_type == INT || m_type == UINT) {
        array = new array_t<int32_t>;
        std::string sname;
        const char* fnm = dataFileName(sname);
        if (fnm == 0) return array;

        //ibis::part::readLock lock(thePart, "column::getIntArray");
        int ierr = ibis::fileManager::instance().getFile(fnm, *array);
        if (ierr != 0) {
            logWarning("getIntArray",
                       "the file manager faild to retrieve the content of"
                       " the data file \"%s\"", fnm);
        }
    }
    else {
        logWarning("getIntArray", "incompatible data type");
    }
    return array;
} // ibis::column::getIntArray

/// Return all rows of the column as an array_t object.
ibis::array_t<float>* ibis::column::getFloatArray() const {
    ibis::array_t<float>* array = 0;
    if (dataflag < 0) {
    }
    else if (m_type == FLOAT) {
        array = new array_t<float>;
        std::string sname;
        const char* fnm = dataFileName(sname);
        if (fnm == 0) return array;

        //ibis::part::readLock lock(thePart, "column::getFloatArray");
        int ierr = ibis::fileManager::instance().getFile(fnm, *array);
        if (ierr != 0) {
            logWarning("getFloatArray",
                       "the file manager faild to retrieve the content of"
                       " the data file \"%s\"", fnm);
        }
    }
    else {
        logWarning("getFloatArray()", " incompatible data type");
    }
    return array;
} // ibis::column::getFloatArray

/// Return all rows of the column as an array_t object.
ibis::array_t<double>* ibis::column::getDoubleArray() const {
    ibis::array_t<double>* array = 0;
    if (dataflag < 0) {
    }
    else if (m_type == DOUBLE) {
        array = new array_t<double>;
        std::string sname;
        const char* fnm = dataFileName(sname);
        if (fnm == 0) return array;

        //ibis::part::readLock lock(thePart, "column::getDoubleArray");
        int ierr = ibis::fileManager::instance().getFile(fnm, *array);
        if (ierr != 0) {
            logWarning("getDoubleArray",
                       "the file manager faild to retrieve the content of"
                       " the data file \"%s\"", fnm);
        }
    }
    else {
        logWarning("getDoubleArray", "incompatible data type");
    }
    return array;
} // ibis::column::getDoubleArray

/// Copy all rows of the column into an array_t object.
/// The incoming argument must be array_t<Type>*.  This function 
/// explicitly casts @c vals into one of the ten supported numerical data
/// types.  If the incoming argument is not of the correct type, this cast
/// operatioin can will have unpredictable consequence.
/// 
/// It returns 0 to indicate success, and a negative number to indicate
/// error.  If @c vals is nil, no values is copied, this function
/// essentially tests whether the values are accessible: >= 0 yes, < 0 no.
int ibis::column::getValuesArray(void* vals) const {
    if (dataflag < 0) return -1;
    int ierr = 0;
    ibis::fileManager::storage *tmp = getRawData();
    if (tmp != 0) {
        if (vals == 0) return ierr; // return 0 to indicate data in memory

        switch (m_type) {
        case ibis::BYTE: {
            array_t<char> ta(tmp);
            static_cast<array_t<char>*>(vals)->swap(ta);
            break;}
        case ibis::UBYTE: {
            array_t<unsigned char> ta(tmp);
            static_cast<array_t<unsigned char>*>(vals)->swap(ta);
            break;}
        case ibis::SHORT: {
            array_t<int16_t> ta(tmp);
            static_cast<array_t<int16_t>*>(vals)->swap(ta);
            break;}
        case ibis::USHORT: {
            array_t<uint16_t> ta(tmp);
            static_cast<array_t<uint16_t>*>(vals)->swap(ta);
            break;}
        case ibis::INT: {
            array_t<int32_t> ta(tmp);
            static_cast<array_t<int32_t>*>(vals)->swap(ta);
            break;}
        case ibis::UINT: {
            array_t<uint32_t> ta(tmp);
            static_cast<array_t<uint32_t>*>(vals)->swap(ta);
            break;}
        case ibis::LONG: {
            array_t<int64_t> ta(tmp);
            static_cast<array_t<int64_t>*>(vals)->swap(ta);
            break;}
        case ibis::ULONG: {
            array_t<uint64_t> ta(tmp);
            static_cast<array_t<uint64_t>*>(vals)->swap(ta);
            break;}
        case ibis::FLOAT: {
            array_t<float> ta(tmp);
            static_cast<array_t<float>*>(vals)->swap(ta);
            break;}
        case ibis::DOUBLE: {
            array_t<double> ta(tmp);
            static_cast<array_t<double>*>(vals)->swap(ta);
            break;}
        case ibis::OID: {
            array_t<ibis::rid_t> ta(tmp);
            static_cast<array_t<ibis::rid_t>*>(vals)->swap(ta);
            break;}
        default: {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- column::getValuesArray(" << vals
                << ") does not support data type "
                << ibis::TYPESTRING[static_cast<int>(m_type)];
            ierr = -2;
            break;}
        }
    }
    else {
        ierr = -3;
    }
    return ierr;
} // ibis::column::getValuesArray

/// Does the raw data file exist?
bool ibis::column::hasRawData() const {
    if (dataflag == 0) {
        std::string sname;
        const char* name = dataFileName(sname);
        if (name == 0) return false;

        const unsigned elm = elementSize();
        if (elm == 0) return true;
        return (elm*nRows() == ibis::util::getFileSize(name));
    }
    else {
        return (dataflag > 0);
    }
} // ibis::column::hasRawData

/// Return the content of base data file as a storage object.
ibis::fileManager::storage* ibis::column::getRawData() const {
    if (dataflag < 0) return 0;

    std::string sname;
    const char *fnm = dataFileName(sname);
    if (fnm == 0) {
        dataflag = -1;
        return 0;
    }

    ibis::fileManager::storage *res = 0;
    int ierr = ibis::fileManager::instance().getFile(fnm, &res);
    if (ierr != 0) {
        logWarning("getRawData",
                   "the file manager faild to retrieve the content "
                   "of the file \"%s\"", fnm);
        dataflag = -1;
        delete res;
        res = 0;
    }
    return res;
} // ibis::column::getRawData

/// Retrieve selected 1-byte integer values.  Note that unsigned
/// integers are simply treated as signed integers.
///
/// @note The caller is responsible for freeing the returned array from any
/// of the selectTypes functions.
ibis::array_t<signed char>*
ibis::column::selectBytes(const ibis::bitvector& mask) const {
    std::unique_ptr< ibis::array_t<signed char> >
        array(new array_t<signed char>);
    const uint32_t tot = mask.cnt();
    if (dataflag < 0 || tot == 0)
        return array.release();

    ibis::horometer timer;
    if (ibis::gVerbose > 4)
        timer.start();
    std::string sname;
    const char* fnm = dataFileName(sname);
    if (fnm == 0 || *fnm == 0) {
        dataflag = -1;
        return array.release();
    }

    if (m_type == ibis::BYTE || m_type == ibis::UBYTE) {
#if defined(FASTBIT_PREFER_READ_ALL)
        array_t<signed char> prop;
        ibis::fileManager::ACCESS_PREFERENCE apref =
            thePart != 0 ? thePart->accessHint(mask, sizeof(char))
            : ibis::fileManager::MMAP_LARGE_FILES;

        int ierr = ibis::fileManager::instance().getFile(fnm, prop, apref);
        if (ierr != 0) {
            logWarning("selectBytes",
                       "the file manager faild to retrieve the content of"
                       " the data file \"%s\"", fnm);
            return array.release();
        }

        uint32_t i = 0;
        const uint32_t nprop = prop.size();
        if (tot >= nprop) {
            ibis::array_t<signed char> tmp(prop);
            array->swap(tmp);
            i = nprop;
        }
        else {
            array->resize(tot);
            ibis::bitvector::indexSet index = mask.firstIndexSet();
            if (nprop >= mask.size()) { // no need to check loop bounds
                while (index.nIndices() > 0) {
                    const ibis::bitvector::word_t *idx0 = index.indices();
                    if (index.isRange()) {
                        for (uint32_t j = *idx0; j < idx0[1]; ++j, ++i) {
                            (*array)[i] = (prop[j]);
                        }
                    }
                    else {
                        for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                            (*array)[i] = (prop[idx0[j]]);
                        }
                    }
                    ++ index;
                }
            }
            else { // need to check loop bounds against nprop
                while (index.nIndices() > 0) {
                    const ibis::bitvector::word_t *idx0 = index.indices();
                    if (*idx0 >= nprop) break;
                    if (index.isRange()) {
                        for (uint32_t j = *idx0;
                             j < (idx0[1]<=nprop ? idx0[1] : nprop);
                             ++j, ++i) {
                            (*array)[i] = (prop[j]);
                        }
                    }
                    else {
                        for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                            if (idx0[j] < nprop)
                                (*array)[i] = (prop[idx0[j]]);
                            else
                                break;
                        }
                    }
                    ++ index;
                }
            }
        }
        if (i != tot) {
            array->resize(i);
            logWarning("selectBytes", "expected to retrieve %lu elements "
                       "but only got %lu", static_cast<long unsigned>(tot),
                       static_cast<long unsigned>(i));
        }
#else
        long ierr = selectValuesT(fnm, mask, *array);
        if (ierr < 0) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- column["
                << (thePart!=0 ? thePart->name() : "") << "." << m_name
                << "]::selectValuesT failed with error code " << ierr;
            array->clear();
        }
#endif
    }
    else {
        logWarning("selectBytes", "incompatible data type");
    }
    if (ibis::gVerbose > 4) {
        timer.stop();
        long unsigned cnt = mask.cnt();
        logMessage("selectBytes", "retrieving %lu integer%s "
                   "took %g sec(CPU), %g sec(elapsed)",
                   static_cast<long unsigned>(cnt), (cnt > 1 ? "s" : ""),
                   timer.CPUTime(), timer.realTime());
    }
    return array.release();
} // ibis::column::selectBytes

/// Return selected rows of the column in an array_t object.
/// @note The caller is responsible for freeing the returned array from any
/// of the selectTypes functions.
ibis::array_t<unsigned char>*
ibis::column::selectUBytes(const ibis::bitvector& mask) const {
    std::unique_ptr< ibis::array_t<unsigned char> >
        array(new array_t<unsigned char>);
    const uint32_t tot = mask.cnt();
    if (dataflag < 0 || tot == 0)
        return array.release();

    ibis::horometer timer;
    if (ibis::gVerbose > 4)
        timer.start();
    std::string sname;
    const char* fnm = dataFileName(sname);
    if (fnm == 0) {
        dataflag = -1;
        return array.release();
    }

    if (m_type == ibis::BYTE || m_type == ibis::UBYTE) {
#if defined(FASTBIT_PREFER_READ_ALL)
        array_t<unsigned char> prop;
        ibis::fileManager::ACCESS_PREFERENCE apref =
            thePart != 0 ? thePart->accessHint(mask, sizeof(unsigned char))
            : ibis::fileManager::MMAP_LARGE_FILES;

        int ierr = ibis::fileManager::instance().getFile(fnm, prop, apref);
        if (ierr != 0) {
            logWarning("selectUBytes",
                       "the file manager faild to retrieve the content of"
                       " the data file \"%s\"", fnm);
            return array.release();
        }

        const uint32_t nprop = prop.size();
        uint32_t i = 0;
        if (tot >= nprop) {
            ibis::array_t<unsigned char> tmp(prop);
            array->swap(tmp);
            i = nprop;
        }
        else {
            array->resize(tot);
            ibis::bitvector::indexSet index = mask.firstIndexSet();
            if (nprop >= mask.size()) { // no need to check loop bounds
                while (index.nIndices() > 0) {
                    const ibis::bitvector::word_t *idx0 = index.indices();
                    if (index.isRange()) {
                        for (uint32_t j = *idx0; j < idx0[1]; ++j, ++i) {
                            (*array)[i] = (prop[j]);
                        }
                    }
                    else {
                        for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                            (*array)[i] = (prop[idx0[j]]);
                        }
                    }
                    ++ index;
                }
            }
            else { // need to check loop bounds against nprop
                while (index.nIndices() > 0) {
                    const ibis::bitvector::word_t *idx0 = index.indices();
                    if (*idx0 >= nprop) break;
                    if (index.isRange()) {
                        for (uint32_t j = *idx0;
                             j < (idx0[1]<=nprop ? idx0[1] : nprop);
                             ++j, ++i) {
                            (*array)[i] = (prop[j]);
                        }
                    }
                    else {
                        for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                            if (idx0[j] < nprop)
                                (*array)[i] = (prop[idx0[j]]);
                            else
                                break;
                        }
                    }
                    ++ index;
                }
            }
        }
        if (i != tot) {
            array->resize(i);
            logWarning("selectUBytes", "expected to retrieve %lu elements "
                       "but only got %lu", static_cast<long unsigned>(tot),
                       static_cast<long unsigned>(i));
        }
#else
        long ierr = selectValuesT(fnm, mask, *array);
        if (ierr < 0) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- column["
                << (thePart!=0 ? thePart->name() : "") << "." << m_name
                << "]::selectValuesT failed with error code " << ierr;
            array->clear();
        }
#endif
    }
    else {
        logWarning("selectUBytes", "incompatible data type");
    }
    if (ibis::gVerbose > 4) {
        timer.stop();
        long unsigned cnt = mask.cnt();
        logMessage("selectUBytes", "retrieving %lu integer%s "
                   "took %g sec(CPU), %g sec(elapsed)",
                   static_cast<long unsigned>(cnt), (cnt > 1 ? "s" : ""),
                   timer.CPUTime(), timer.realTime());
    }
    return array.release();
} // ibis::column::selectUBytes

/// Return selected rows of the column in an array_t object.
/// Can convert all integers 2-byte or less in length.  Note that unsigned
/// integers are simply treated as signed integers.  Shoter types
/// of signed integers are treated correctly as positive values.
/// @note The caller is responsible for freeing the returned array from any
/// of the selectTypes functions.
ibis::array_t<int16_t>*
ibis::column::selectShorts(const ibis::bitvector& mask) const {
    std::unique_ptr< ibis::array_t<int16_t> > array(new array_t<int16_t>);
    const uint32_t tot = mask.cnt();
    if (dataflag < 0 || tot == 0)
        return array.release();

    ibis::horometer timer;
    if (ibis::gVerbose > 4)
        timer.start();
    std::string sname;
    const char* fnm = dataFileName(sname);
    if (fnm == 0) {
        dataflag = -1;
        return array.release();
    }

    if (m_type == ibis::SHORT || m_type == ibis::USHORT) {
#if defined(FASTBIT_PREFER_READ_ALL)
        array_t<int16_t> prop;
        ibis::fileManager::ACCESS_PREFERENCE apref =
            thePart->accessHint(mask, sizeof(int16_t));

        int ierr = ibis::fileManager::instance().getFile(fnm, prop, apref);
        if (ierr != 0) {
            logWarning("selectShorts",
                       "the file manager faild to retrieve the content of"
                       " the data file \"%s\"", fnm);
            return array.release();
        }

        const uint32_t nprop = prop.size();
        uint32_t i = 0;
        if (tot >= nprop) {
            ibis::array_t<int16_t> tmp(prop);
            array->swap(tmp);
            i = nprop;
        }
        else {
            array->resize(tot);
            ibis::bitvector::indexSet index = mask.firstIndexSet();
            if (nprop >= mask.size()) { // no need to check loop bounds
                while (index.nIndices() > 0) {
                    const ibis::bitvector::word_t *idx0 = index.indices();
                    if (index.isRange()) {
                        for (uint32_t j = *idx0; j < idx0[1]; ++j, ++i) {
                            (*array)[i] = (prop[j]);
                        }
                    }
                    else {
                        for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                            (*array)[i] = (prop[idx0[j]]);
                        }
                    }
                    ++ index;
                }
            }
            else { // need to check loop bounds against nprop
                while (index.nIndices() > 0) {
                    const ibis::bitvector::word_t *idx0 = index.indices();
                    if (*idx0 >= nprop) break;
                    if (index.isRange()) {
                        for (uint32_t j = *idx0;
                             j < (idx0[1]<=nprop ? idx0[1] : nprop);
                             ++j, ++i) {
                            (*array)[i] = (prop[j]);
                        }
                    }
                    else {
                        for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                            if (idx0[j] < nprop)
                                (*array)[i] = (prop[idx0[j]]);
                            else
                                break;
                        }
                    }
                    ++ index;
                }
            }
        }
        if (i != tot) {
            array->resize(i);
            logWarning("selectShorts", "expected to retrieve %lu elements "
                       "but only got %lu", static_cast<long unsigned>(tot),
                       static_cast<long unsigned>(i));
        }
#else
        long ierr = selectValuesT(fnm, mask, *array);
        if (ierr < 0) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- column["
                << (thePart!=0 ? thePart->name() : "") << "." << m_name
                << "]::selectValuesT failed with error code " << ierr;
            array->clear();
        }
#endif
    }
    else if (m_type == ibis::BYTE) {
        array_t<signed char> prop;
        ibis::fileManager::ACCESS_PREFERENCE apref =
            thePart != 0 ? thePart->accessHint(mask, sizeof(char))
            : ibis::fileManager::MMAP_LARGE_FILES;

        int ierr = ibis::fileManager::instance().getFile(fnm, prop, apref);
        if (ierr != 0) {
            logWarning("selectShorts",
                       "the file manager faild to retrieve the content of"
                       " the data file \"%s\"", fnm);
            return array.release();
        }

        uint32_t i = 0;
        array->resize(tot);
        const uint32_t nprop = prop.size();
        ibis::bitvector::indexSet index = mask.firstIndexSet();
        if (nprop >= mask.size()) { // no need to check loop bounds
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (index.isRange()) {
                    for (uint32_t j = *idx0; j < idx0[1]; ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        (*array)[i] = (prop[idx0[j]]);
                    }
                }
                ++ index;
            }
        }
        else { // need to check loop bounds against nprop
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (*idx0 >= nprop) break;
                if (index.isRange()) {
                    for (uint32_t j = *idx0;
                         j < (idx0[1]<=nprop ? idx0[1] : nprop);
                         ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        if (idx0[j] < nprop)
                            (*array)[i] = (prop[idx0[j]]);
                        else
                            break;
                    }
                }
                ++ index;
            }
        }
        if (i != tot) {
            array->resize(i);
            logWarning("selectShorts", "expected to retrieve %lu elements "
                       "but only got %lu", static_cast<long unsigned>(tot),
                       static_cast<long unsigned>(i));
        }
    }
    else if (m_type == ibis::UBYTE) {
        array_t<unsigned char> prop;
        ibis::fileManager::ACCESS_PREFERENCE apref =
            thePart != 0 ? thePart->accessHint(mask, sizeof(char))
            : ibis::fileManager::MMAP_LARGE_FILES;

        int ierr = ibis::fileManager::instance().getFile(fnm, prop, apref);
        if (ierr != 0) {
            logWarning("selectShorts",
                       "the file manager faild to retrieve the content of"
                       " the data file \"%s\"", fnm);
            return array.release();
        }

        uint32_t i = 0;
        array->resize(tot);
        const uint32_t nprop = prop.size();
        ibis::bitvector::indexSet index = mask.firstIndexSet();
        if (nprop >= mask.size()) { // no need to check loop bounds
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (index.isRange()) {
                    for (uint32_t j = *idx0; j < idx0[1]; ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        (*array)[i] = (prop[idx0[j]]);
                    }
                }
                ++ index;
            }
        }
        else { // need to check loop bounds against nprop
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (*idx0 >= nprop) break;
                if (index.isRange()) {
                    for (uint32_t j = *idx0;
                         j < (idx0[1]<=nprop ? idx0[1] : nprop);
                         ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        if (idx0[j] < nprop)
                            (*array)[i] = (prop[idx0[j]]);
                        else
                            break;
                    }
                }
                ++ index;
            }
        }
        if (i != tot) {
            array->resize(i);
            logWarning("selectShorts", "expected to retrieve %lu elements "
                       "but only got %lu", static_cast<long unsigned>(tot),
                       static_cast<long unsigned>(i));
        }
    }
    else {
        logWarning("selectShorts", "incompatible data type");
    }
    if (ibis::gVerbose > 4) {
        timer.stop();
        long unsigned cnt = mask.cnt();
        logMessage("selectShorts", "retrieving %lu integer%s "
                   "took %g sec(CPU), %g sec(elapsed)",
                   static_cast<long unsigned>(cnt), (cnt > 1 ? "s" : ""),
                   timer.CPUTime(), timer.realTime());
    }
    return array.release();
} // ibis::column::selectShorts

/// Return selected rows of the column in an array_t object.
/// @note The caller is responsible for freeing the returned array from any
/// of the selectTypes functions.
ibis::array_t<uint16_t>*
ibis::column::selectUShorts(const ibis::bitvector& mask) const {
    std::unique_ptr< ibis::array_t<uint16_t> > array(new array_t<uint16_t>);
    const uint32_t tot = mask.cnt();
    if (dataflag < 0 || tot == 0)
        return array.release();

    ibis::horometer timer;
    if (ibis::gVerbose > 4)
        timer.start();
    std::string sname;
    const char* fnm = dataFileName(sname);
    if (fnm == 0) {
        dataflag = -1;
        return array.release();
    }

    if (m_type == ibis::SHORT || m_type == ibis::USHORT) {
#if defined(FASTBIT_PREFER_READ_ALL)
        array_t<uint16_t> prop;
        ibis::fileManager::ACCESS_PREFERENCE apref =
            thePart->accessHint(mask, sizeof(uint16_t));

        int ierr = ibis::fileManager::instance().getFile(fnm, prop, apref);
        if (ierr != 0) {
            logWarning("selectUShorts",
                       "the file manager faild to retrieve the content of"
                       " the data file \"%s\"", fnm);
            return array.release();
        }

        const uint32_t nprop = prop.size();
        uint32_t i = 0;
        if (tot > nprop) {
            ibis::array_t<uint16_t> tmp(prop);
            array->swap(tmp);
            i = nprop;
        }
        else {
            array->resize(tot);
            ibis::bitvector::indexSet index = mask.firstIndexSet();
            if (nprop >= mask.size()) { // no need to check loop bounds
                while (index.nIndices() > 0) {
                    const ibis::bitvector::word_t *idx0 = index.indices();
                    if (index.isRange()) {
                        for (uint32_t j = *idx0; j < idx0[1]; ++j, ++i) {
                            (*array)[i] = (prop[j]);
                        }
                    }
                    else {
                        for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                            (*array)[i] = (prop[idx0[j]]);
                        }
                    }
                    ++ index;
                }
            }
            else { // need to check loop bounds against nprop
                while (index.nIndices() > 0) {
                    const ibis::bitvector::word_t *idx0 = index.indices();
                    if (*idx0 >= nprop) break;
                    if (index.isRange()) {
                        for (uint32_t j = *idx0;
                             j < (idx0[1]<=nprop ? idx0[1] : nprop);
                             ++j, ++i) {
                            (*array)[i] = (prop[j]);
                        }
                    }
                    else {
                        for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                            if (idx0[j] < nprop)
                                (*array)[i] = (prop[idx0[j]]);
                            else
                                break;
                        }
                    }
                    ++ index;
                }
            }
        }
        if (i != tot) {
            array->resize(i);
            logWarning("selectUShorts", "expected to retrieve %lu elements "
                       "but only got %lu", static_cast<long unsigned>(tot),
                       static_cast<long unsigned>(i));
        }
#else
        long ierr = selectValuesT(fnm, mask, *array);
        if (ierr < 0) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- column["
                << (thePart!=0 ? thePart->name() : "") << "." << m_name
                << "]::selectValuesT failed with error code " << ierr;
            array->clear();
        }
#endif
    }
    else if (m_type == ibis::BYTE) {
        array_t<signed char> prop;
        ibis::fileManager::ACCESS_PREFERENCE apref =
            thePart != 0 ? thePart->accessHint(mask, sizeof(char))
            : ibis::fileManager::MMAP_LARGE_FILES;

        int ierr = ibis::fileManager::instance().getFile(fnm, prop, apref);
        if (ierr != 0) {
            logWarning("selectUShorts",
                       "the file manager faild to retrieve the content of"
                       " the data file \"%s\"", fnm);
            return array.release();
        }

        uint32_t i = 0;
        array->resize(tot);
        const uint32_t nprop = prop.size();
        ibis::bitvector::indexSet index = mask.firstIndexSet();
        if (nprop >= mask.size()) { // no need to check loop bounds
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (index.isRange()) {
                    for (uint32_t j = *idx0; j < idx0[1]; ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        (*array)[i] = (prop[idx0[j]]);
                    }
                }
                ++ index;
            }
        }
        else { // need to check loop bounds against nprop
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (*idx0 >= nprop) break;
                if (index.isRange()) {
                    for (uint32_t j = *idx0;
                         j < (idx0[1]<=nprop ? idx0[1] : nprop);
                         ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        if (idx0[j] < nprop)
                            (*array)[i] = (prop[idx0[j]]);
                        else
                            break;
                    }
                }
                ++ index;
            }
        }
        if (i != tot) {
            array->resize(i);
            logWarning("selectUShorts", "expected to retrieve %lu elements "
                       "but only got %lu", static_cast<long unsigned>(tot),
                       static_cast<long unsigned>(i));
        }
    }
    else if (m_type == ibis::UBYTE) {
        array_t<unsigned char> prop;
        ibis::fileManager::ACCESS_PREFERENCE apref =
            thePart != 0 ? thePart->accessHint(mask, sizeof(char))
            : ibis::fileManager::MMAP_LARGE_FILES;

        int ierr = ibis::fileManager::instance().getFile(fnm, prop, apref);
        if (ierr != 0) {
            logWarning("selectUShorts",
                       "the file manager faild to retrieve the content of"
                       " the data file \"%s\"", fnm);
            return array.release();
        }

        uint32_t i = 0;
        array->resize(tot);
        const uint32_t nprop = prop.size();
        ibis::bitvector::indexSet index = mask.firstIndexSet();
        if (nprop >= mask.size()) { // no need to check loop bounds
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (index.isRange()) {
                    for (uint32_t j = *idx0; j < idx0[1]; ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        (*array)[i] = (prop[idx0[j]]);
                    }
                }
                ++ index;
            }
        }
        else { // need to check loop bounds against nprop
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (*idx0 >= nprop) break;
                if (index.isRange()) {
                    for (uint32_t j = *idx0;
                         j < (idx0[1]<=nprop ? idx0[1] : nprop);
                         ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        if (idx0[j] < nprop)
                            (*array)[i] = (prop[idx0[j]]);
                        else
                            break;
                    }
                }
                ++ index;
            }
        }
        if (i != tot) {
            array->resize(i);
            logWarning("selectUShorts", "expected to retrieve %lu elements "
                       "but only got %lu", static_cast<long unsigned>(tot),
                       static_cast<long unsigned>(i));
        }
    }
    else {
        logWarning("selectUShorts", "incompatible data type");
    }
    if (ibis::gVerbose > 4) {
        timer.stop();
        long unsigned cnt = mask.cnt();
        logMessage("selectUShorts", "retrieving %lu integer%s "
                   "took %g sec(CPU), %g sec(elapsed)",
                   static_cast<long unsigned>(cnt), (cnt > 1 ? "s" : ""),
                   timer.CPUTime(), timer.realTime());
    }
    return array.release();
} // ibis::column::selectUShorts

/// Return selected rows of the column in an array_t object.
/// @note The caller is responsible for freeing the returned array from any
/// of the selectTypes functions.
ibis::array_t<int32_t>*
ibis::column::selectInts(const ibis::bitvector& mask) const {
    std::unique_ptr< ibis::array_t<int32_t> > array(new array_t<int32_t>);
    const uint32_t tot = mask.cnt();
    if (dataflag < 0 || tot == 0)
        return array.release();

    ibis::horometer timer;
    if (ibis::gVerbose > 4)
        timer.start();
    std::string sname;
    const char* fnm = dataFileName(sname);
    if (fnm == 0) {
        dataflag = -1;
        return array.release();
    }

    if (m_type == ibis::INT || m_type == ibis::UINT ||
        m_type == ibis::CATEGORY || m_type == ibis::TEXT) {
#if defined(FASTBIT_PREFER_READ_ALL)
        array_t<int32_t> prop;
        ibis::fileManager::ACCESS_PREFERENCE apref =
            thePart != 0 ? thePart->accessHint(mask, sizeof(int32_t))
            : ibis::fileManager::MMAP_LARGE_FILES;

        int ierr = ibis::fileManager::instance().getFile(fnm, prop, apref);
        if (ierr != 0) {
            logWarning("selectInts",
                       "the file manager faild to retrieve the content of"
                       " the data file \"%s\"", fnm);
            return array.release();
        }

        const long unsigned nprop = prop.size();
#if DEBUG+0 > 0 || _DEBUG+0 > 0
        logMessage("DEBUG", "selectInts mask.size(%lu) and nprop=%lu",
                   static_cast<long unsigned>(mask.size()), nprop);
#endif
        uint32_t i = 0;
        if (tot >= nprop) {
            ibis::array_t<int32_t> tmp(prop);
            array->swap(tmp);
            i = nprop;
        }
        else {
            array->resize(tot);
            ibis::bitvector::indexSet index = mask.firstIndexSet();
            if (nprop >= mask.size()) { // no need to check loop bounds
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                logMessage("DEBUG", "entering unchecked loops");
#endif
                while (index.nIndices() > 0) {
                    const ibis::bitvector::word_t *idx0 = index.indices();
                    if (index.isRange()) {
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                        logMessage("DEBUG", "copying range [%lu, %lu), i=%lu",
                                   static_cast<long unsigned>(*idx0),
                                   static_cast<long unsigned>(idx0[1]),
                                   static_cast<long unsigned>(i));
#endif
                        for (uint32_t j = *idx0; j < idx0[1]; ++j, ++i) {
                            (*array)[i] = (prop[j]);
                        }
                    }
                    else {
                        for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                            logMessage("DEBUG", "copying value %lu to i=%lu",
                                       static_cast<long unsigned>(idx0[j]),
                                       static_cast<long unsigned>(i));
#endif
                            (*array)[i] = (prop[idx0[j]]);
                        }
                    }
                    ++ index;
                }
            }
            else { // need to check loop bounds against nprop
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                logMessage("DEBUG", "entering checked loops");
#endif
                while (index.nIndices() > 0) {
                    const ibis::bitvector::word_t *idx0 = index.indices();
                    if (*idx0 >= nprop) break;
                    if (index.isRange()) {
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                        logMessage("DEBUG", "copying range [%lu, %lu), i=%lu",
                                   static_cast<long unsigned>(*idx0),
                                   static_cast<long unsigned>
                                   (idx0[1]<=nprop ? idx0[1] : nprop),
                                   static_cast<long unsigned>(i));
#endif
                        for (uint32_t j = *idx0;
                             j < (idx0[1]<=nprop ? idx0[1] : nprop);
                             ++j, ++i) {
                            (*array)[i] = (prop[j]);
                        }
                    }
                    else {
                        for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                            logMessage("DEBUG", "copying value %lu to i=%lu",
                                       static_cast<long unsigned>(idx0[j]),
                                       static_cast<long unsigned>(i));
#endif
                            if (idx0[j] < nprop)
                                (*array)[i] = (prop[idx0[j]]);
                            else
                                break;
                        }
                    }
                    ++ index;
                }
            }
        }
        if (i != tot) {
            array->resize(i);
            logWarning("selectInts", "expected to retrieve %lu elements "
                       "but only got %lu", static_cast<long unsigned>(tot),
                       static_cast<long unsigned>(i));
        }
#else
        long ierr = selectValuesT(fnm, mask, *array);
        if (ierr < 0) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- column["
                << (thePart!=0 ? thePart->name() : "") << "." << m_name
                << "]::selectValuesT failed with error code " << ierr;
            array->clear();
        }
#endif
    }
    else if (m_type == ibis::SHORT) {
        array_t<int16_t> prop;
        ibis::fileManager::ACCESS_PREFERENCE apref =
            thePart->accessHint(mask, sizeof(int16_t));

        int ierr = ibis::fileManager::instance().getFile(fnm, prop, apref);
        if (ierr != 0) {
            logWarning("selectInts",
                       "the file manager faild to retrieve the content of"
                       " the data file \"%s\"", fnm);
            return array.release();
        }

        uint32_t i = 0;
        array->resize(tot);
        const uint32_t nprop = prop.size();
        ibis::bitvector::indexSet index = mask.firstIndexSet();
        if (nprop >= mask.size()) { // no need to check loop bounds
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (index.isRange()) {
                    for (uint32_t j = *idx0; j < idx0[1]; ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        (*array)[i] = (prop[idx0[j]]);
                    }
                }
                ++ index;
            }
        }
        else { // need to check loop bounds against nprop
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (*idx0 >= nprop) break;
                if (index.isRange()) {
                    for (uint32_t j = *idx0;
                         j < (idx0[1]<=nprop ? idx0[1] : nprop);
                         ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        if (idx0[j] < nprop)
                            (*array)[i] = (prop[idx0[j]]);
                        else
                            break;
                    }
                }
                ++ index;
            }
        }
        if (i != tot) {
            array->resize(i);
            logWarning("selectInts", "expected to retrieve %lu elements "
                       "but only got %lu", static_cast<long unsigned>(tot),
                       static_cast<long unsigned>(i));
        }
    }
    else if (m_type == ibis::USHORT) {
        array_t<uint16_t> prop;
        ibis::fileManager::ACCESS_PREFERENCE apref =
            thePart != 0 ? thePart->accessHint(mask, sizeof(uint16_t))
            : ibis::fileManager::MMAP_LARGE_FILES;

        int ierr = ibis::fileManager::instance().getFile(fnm, prop, apref);
        if (ierr != 0) {
            logWarning("selectInts",
                       "the file manager faild to retrieve the content of"
                       " the data file \"%s\"", fnm);
            return array.release();
        }

        uint32_t i = 0;
        array->resize(tot);
        const uint32_t nprop = prop.size();
        ibis::bitvector::indexSet index = mask.firstIndexSet();
        if (nprop >= mask.size()) { // no need to check loop bounds
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (index.isRange()) {
                    for (uint32_t j = *idx0; j < idx0[1]; ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        (*array)[i] = (prop[idx0[j]]);
                    }
                }
                ++ index;
            }
        }
        else { // need to check loop bounds against nprop
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (*idx0 >= nprop) break;
                if (index.isRange()) {
                    for (uint32_t j = *idx0;
                         j < (idx0[1]<=nprop ? idx0[1] : nprop);
                         ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        if (idx0[j] < nprop)
                            (*array)[i] = (prop[idx0[j]]);
                        else
                            break;
                    }
                }
                ++ index;
            }
        }
        if (i != tot) {
            array->resize(i);
            logWarning("selectInts", "expected to retrieve %lu elements "
                       "but only got %lu", static_cast<long unsigned>(tot),
                       static_cast<long unsigned>(i));
        }
    }
    else if (m_type == ibis::BYTE) {
        array_t<signed char> prop;
        ibis::fileManager::ACCESS_PREFERENCE apref =
            thePart != 0 ? thePart->accessHint(mask, sizeof(char))
            : ibis::fileManager::MMAP_LARGE_FILES;

        int ierr = ibis::fileManager::instance().getFile(fnm, prop, apref);
        if (ierr != 0) {
            logWarning("selectInts",
                       "the file manager faild to retrieve the content of"
                       " the data file \"%s\"", fnm);
            return array.release();
        }

        uint32_t i = 0;
        array->resize(tot);
        const uint32_t nprop = prop.size();
        ibis::bitvector::indexSet index = mask.firstIndexSet();
        if (nprop >= mask.size()) { // no need to check loop bounds
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (index.isRange()) {
                    for (uint32_t j = *idx0; j < idx0[1]; ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        (*array)[i] = (prop[idx0[j]]);
                    }
                }
                ++ index;
            }
        }
        else { // need to check loop bounds against nprop
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (*idx0 >= nprop) break;
                if (index.isRange()) {
                    for (uint32_t j = *idx0;
                         j < (idx0[1]<=nprop ? idx0[1] : nprop);
                         ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        if (idx0[j] < nprop)
                            (*array)[i] = (prop[idx0[j]]);
                        else
                            break;
                    }
                }
                ++ index;
            }
        }
        if (i != tot) {
            array->resize(i);
            logWarning("selectInts", "expected to retrieve %lu elements "
                       "but only got %lu", static_cast<long unsigned>(tot),
                       static_cast<long unsigned>(i));
        }
    }
    else if (m_type == ibis::UBYTE) {
        array_t<unsigned char> prop;
        ibis::fileManager::ACCESS_PREFERENCE apref =
            thePart != 0 ? thePart->accessHint(mask, sizeof(char))
            : ibis::fileManager::MMAP_LARGE_FILES;

        int ierr = ibis::fileManager::instance().getFile(fnm, prop, apref);
        if (ierr != 0) {
            logWarning("selectInts",
                       "the file manager faild to retrieve the content of"
                       " the data file \"%s\"", fnm);
            return array.release();
        }

        uint32_t i = 0;
        array->resize(tot);
        const uint32_t nprop = prop.size();
        ibis::bitvector::indexSet index = mask.firstIndexSet();
        if (nprop >= mask.size()) { // no need to check loop bounds
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (index.isRange()) {
                    for (uint32_t j = *idx0; j < idx0[1]; ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        (*array)[i] = (prop[idx0[j]]);
                    }
                }
                ++ index;
            }
        }
        else { // need to check loop bounds against nprop
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (*idx0 >= nprop) break;
                if (index.isRange()) {
                    for (uint32_t j = *idx0;
                         j < (idx0[1]<=nprop ? idx0[1] : nprop);
                         ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        if (idx0[j] < nprop)
                            (*array)[i] = (prop[idx0[j]]);
                        else
                            break;
                    }
                }
                ++ index;
            }
        }
        if (i != tot) {
            array->resize(i);
            logWarning("selectInts", "expected to retrieve %lu elements "
                       "but only got %lu", static_cast<long unsigned>(tot),
                       static_cast<long unsigned>(i));
        }
    }
    else {
        logWarning("selectInts", "incompatible data type");
    }
    if (ibis::gVerbose > 4) {
        timer.stop();
        long unsigned cnt = mask.cnt();
        logMessage("selectInts", "retrieving %lu integer%s "
                   "took %g sec(CPU), %g sec(elapsed)",
                   static_cast<long unsigned>(cnt), (cnt > 1 ? "s" : ""),
                   timer.CPUTime(), timer.realTime());
    }
    return array.release();
} // ibis::column::selectInts

/// Return selected rows of the column in an array_t object.
/// Can be called on columns of unsigned integral types, UINT, CATEGORY,
/// USHORT, and UBYTE.
/// @note The caller is responsible for freeing the returned array from any
/// of the selectTypes functions.
ibis::array_t<uint32_t>*
ibis::column::selectUInts(const ibis::bitvector& mask) const {
    std::unique_ptr< ibis::array_t<uint32_t> > array(new array_t<uint32_t>);
    const uint32_t tot = mask.cnt();
    if (dataflag < 0 || tot == 0)
        return array.release();

    ibis::horometer timer;
    if (ibis::gVerbose > 4)
        timer.start();
    std::string sname;
    const char* fnm = dataFileName(sname);
    if (fnm == 0) {
        dataflag = -1;
        return array.release();
    }

    if (m_type == ibis::UINT || m_type == ibis::CATEGORY ||
        m_type == ibis::TEXT) {
#if defined(FASTBIT_PREFER_READ_ALL)
        array_t<uint32_t> prop;
        ibis::fileManager::ACCESS_PREFERENCE apref =
            thePart != 0 ? thePart->accessHint(mask, sizeof(uint32_t))
            : ibis::fileManager::MMAP_LARGE_FILES;

        int ierr = ibis::fileManager::instance().getFile(fnm, prop, apref);
        if (ierr != 0) {
            logWarning("selectUInts",
                       "the file manager faild to retrieve the content of"
                       " the data file \"%s\"", fnm);
            return array.release();
        }

        const uint32_t nprop = prop.size();
        uint32_t i = 0;
        if (tot >= nprop) {
            ibis::array_t<uint32_t> tmp(prop);
            array->swap(tmp);
            i = nprop;
        }
        else {
            array->resize(tot);
            ibis::bitvector::indexSet index = mask.firstIndexSet();
            if (nprop >= mask.size()) {
                while (index.nIndices() > 0) {
                    const ibis::bitvector::word_t *idx0 = index.indices();
                    if (index.isRange()) {
                        for (uint32_t j = *idx0; j<idx0[1]; ++j, ++i) {
                            (*array)[i] = (prop[j]);
                        }
                    }
                    else {
                        for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                            (*array)[i] = (prop[idx0[j]]);
                        }
                    }
                    ++ index;
                }
            }
            else { // need to check loop bounds against nprop
                while (index.nIndices() > 0) {
                    const ibis::bitvector::word_t *idx0 = index.indices();
                    if (*idx0 >= nprop) break;
                    if (index.isRange()) {
                        for (uint32_t j = *idx0;
                             j < (idx0[1]<=nprop ? idx0[1] : nprop);
                             ++j, ++i) {
                            (*array)[i] = (prop[j]);
                        }
                    }
                    else {
                        for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                            if (idx0[j] < nprop)
                                (*array)[i] = (prop[idx0[j]]);
                            else
                                break;
                        }
                    }
                    ++ index;
                }
            }
        }
        if (i != tot) {
            array->resize(i);
            logWarning("selectUInts", "expected to retrieve %lu elements "
                       "but only got %lu", static_cast<long unsigned>(tot),
                       static_cast<long unsigned>(i));
        }
#else
        long ierr = selectValuesT(fnm, mask, *array);
        if (ierr < 0) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- column["
                << (thePart!=0 ? thePart->name() : "") << "." << m_name
                << "]::selectValuesT failed with error code " << ierr;
            array->clear();
        }
#endif
    }
    else if (m_type == USHORT) {
        array_t<uint16_t> prop;
        ibis::fileManager::ACCESS_PREFERENCE apref =
            thePart != 0 ? thePart->accessHint(mask, sizeof(uint16_t))
            : ibis::fileManager::MMAP_LARGE_FILES;

        int ierr = ibis::fileManager::instance().getFile(fnm, prop, apref);
        if (ierr != 0) {
            logWarning("selectUInts",
                       "the file manager faild to retrieve the content of"
                       " the data file \"%s\"", fnm);
            return array.release();
        }

        uint32_t i = 0;
        array->resize(tot);
        const uint32_t nprop = prop.size();
        ibis::bitvector::indexSet index = mask.firstIndexSet();
        if (nprop >= mask.size()) {
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (index.isRange()) {
                    for (uint32_t j = *idx0; j<idx0[1]; ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        (*array)[i] = (prop[idx0[j]]);
                    }
                }
                ++ index;
            }
        }
        else { // need to check loop bounds against nprop
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (*idx0 >= nprop) break;
                if (index.isRange()) {
                    for (uint32_t j = *idx0;
                         j < (idx0[1]<=nprop ? idx0[1] : nprop);
                         ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        if (idx0[j] < nprop)
                            (*array)[i] = (prop[idx0[j]]);
                        else
                            break;
                    }
                }
                ++ index;
            }
        }

        if (i != tot) {
            array->resize(i);
            logWarning("selectUInts", "expected to retrieve %lu elements "
                       "but only got %lu", static_cast<long unsigned>(tot),
                       static_cast<long unsigned>(i));
        }
    }
    else if (m_type == UBYTE) {
        array_t<unsigned char> prop;
        ibis::fileManager::ACCESS_PREFERENCE apref =
            thePart != 0 ? thePart->accessHint(mask, sizeof(unsigned char))
            : ibis::fileManager::MMAP_LARGE_FILES;

        int ierr = ibis::fileManager::instance().getFile(fnm, prop, apref);
        if (ierr != 0) {
            logWarning("selectUInts",
                       "the file manager faild to retrieve the content of"
                       " the data file \"%s\"", fnm);
            return array.release();
        }

        uint32_t i = 0;
        array->resize(tot);
        const uint32_t nprop = prop.size();
        ibis::bitvector::indexSet index = mask.firstIndexSet();
        if (nprop >= mask.size()) {
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (index.isRange()) {
                    for (uint32_t j = *idx0; j<idx0[1]; ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        (*array)[i] = (prop[idx0[j]]);
                    }
                }
                ++ index;
            }
        }
        else { // need to check loop bounds against nprop
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (*idx0 >= nprop) break;
                if (index.isRange()) {
                    for (uint32_t j = *idx0;
                         j < (idx0[1]<=nprop ? idx0[1] : nprop);
                         ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        if (idx0[j] < nprop)
                            (*array)[i] = (prop[idx0[j]]);
                        else
                            break;
                    }
                }
                ++ index;
            }
        }

        if (i != tot) {
            array->resize(i);
            logWarning("selectUInts", "expected to retrieve %lu elements "
                       "but only got %lu", static_cast<long unsigned>(tot),
                       static_cast<long unsigned>(i));
        }
    }
    else {
        logWarning("selectUInts", "incompatible data type");
    }
    if (ibis::gVerbose > 4) {
        timer.stop();
        long unsigned cnt = mask.cnt();
        logMessage("selectUInts", "retrieving %lu unsigned integer%s "
                   "took %g sec(CPU), %g sec(elapsed)",
                   static_cast<long unsigned>(cnt), (cnt > 1 ? "s" : ""),
                   timer.CPUTime(), timer.realTime());
    }
    return array.release();
} // ibis::column::selectUInts

/// Return selected rows of the column in an array_t object.
/// Can be called on all integral types.  Note that 64-byte unsigned
/// integers are simply treated as signed integers.  This may cause the
/// values to be interperted incorrectly.  Shorter version of unsigned
/// integers are treated correctly as positive values.
/// @note The caller is responsible for freeing the returned array from any
/// of the selectTypes functions.
ibis::array_t<int64_t>*
ibis::column::selectLongs(const ibis::bitvector& mask) const {
    std::unique_ptr< array_t<int64_t> > array(new array_t<int64_t>);
    const uint32_t tot = mask.cnt();
    if (dataflag < 0 || tot == 0)
        return array.release();

    ibis::horometer timer;
    if (ibis::gVerbose > 4)
        timer.start();
    std::string sname;
    const char* fnm = dataFileName(sname);
    if (fnm == 0) {
        dataflag = -1;
        return array.release();
    }

    if (m_type == ibis::LONG || m_type == ibis::ULONG) {
#if defined(FASTBIT_PREFER_READ_ALL)
        array_t<int64_t> prop;
        ibis::fileManager::ACCESS_PREFERENCE apref =
            thePart != 0 ? thePart->accessHint(mask, sizeof(int64_t))
            : ibis::fileManager::MMAP_LARGE_FILES;

        int ierr = ibis::fileManager::instance().getFile(fnm, prop, apref);
        if (ierr != 0) {
            logWarning("selectLongs",
                       "the file manager faild to retrieve the content of"
                       " the data file \"%s\"", fnm);
            return array.release();
        }

        const long unsigned nprop = prop.size();
#if DEBUG+0 > 0 || _DEBUG+0 > 0
        logMessage("DEBUG", "selectLongs mask.size(%lu) and nprop=%lu",
                   static_cast<long unsigned>(mask.size()), nprop);
#endif
        uint32_t i = 0;
        if (tot >= nprop) {
            ibis::array_t<int64_t> tmp(prop);
            array->swap(tmp);
            i = nprop;
        }
        else {
            array->resize(tot);
            ibis::bitvector::indexSet index = mask.firstIndexSet();
            if (nprop >= mask.size()) { // no need to check loop bounds
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                logMessage("DEBUG", "entering unchecked loops");
#endif
                while (index.nIndices() > 0) {
                    const ibis::bitvector::word_t *idx0 = index.indices();
                    if (index.isRange()) {
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                        logMessage("DEBUG", "copying range [%lu, %lu), i=%lu",
                                   static_cast<long unsigned>(*idx0),
                                   static_cast<long unsigned>(idx0[1]),
                                   static_cast<long unsigned>(i));
#endif
                        for (uint32_t j = *idx0; j < idx0[1]; ++j, ++i) {
                            (*array)[i] = (prop[j]);
                        }
                    }
                    else {
                        for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                            logMessage("DEBUG", "copying value %lu to i=%lu",
                                       static_cast<long unsigned>(idx0[j]),
                                       static_cast<long unsigned>(i));
#endif
                            (*array)[i] = (prop[idx0[j]]);
                        }
                    }
                    ++ index;
                }
            }
            else { // need to check loop bounds against nprop
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                logMessage("DEBUG", "entering checked loops");
#endif
                while (index.nIndices() > 0) {
                    const ibis::bitvector::word_t *idx0 = index.indices();
                    if (*idx0 >= nprop) break;
                    if (index.isRange()) {
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                        logMessage("DEBUG", "copying range [%lu, %lu), i=%lu",
                                   static_cast<long unsigned>(*idx0),
                                   static_cast<long unsigned>
                                   (idx0[1]<=nprop ? idx0[1] : nprop),
                                   static_cast<long unsigned>(i));
#endif
                        for (uint32_t j = *idx0;
                             j < (idx0[1]<=nprop ? idx0[1] : nprop);
                             ++j, ++i) {
                            (*array)[i] = (prop[j]);
                        }
                    }
                    else {
                        for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                            logMessage("DEBUG", "copying value %lu to i=%lu",
                                       static_cast<long unsigned>(idx0[j]),
                                       static_cast<long unsigned>(i));
#endif
                            if (idx0[j] < nprop)
                                (*array)[i] = (prop[idx0[j]]);
                            else
                                break;
                        }
                    }
                    ++ index;
                }
            }
        }
        if (i != tot) {
            array->resize(i);
            logWarning("selectLongs", "expected to retrieve %lu elements "
                       "but only got %lu", static_cast<long unsigned>(tot),
                       static_cast<long unsigned>(i));
        }
#else
        long ierr = selectValuesT(fnm, mask, *array);
        if (ierr < 0) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- column["
                << (thePart!=0 ? thePart->name() : "") << "." << m_name
                << "]::selectValuesT failed with error code " << ierr;
            array->clear();
        }
#endif
    }
    else if (m_type == ibis::UINT || m_type == ibis::CATEGORY ||
             m_type == ibis::TEXT) {
        array_t<uint32_t> prop;
        ibis::fileManager::ACCESS_PREFERENCE apref =
            thePart != 0 ? thePart->accessHint(mask, sizeof(uint32_t))
            : ibis::fileManager::MMAP_LARGE_FILES;

        int ierr = ibis::fileManager::instance().getFile(fnm, prop, apref);
        if (ierr != 0) {
            logWarning("selectLongs",
                       "the file manager faild to retrieve the content of"
                       " the data file \"%s\"", fnm);
            return array.release();
        }

        uint32_t i = 0;
        array->resize(tot);
        const long unsigned nprop = prop.size();
#if DEBUG+0 > 0 || _DEBUG+0 > 0
        logMessage("DEBUG", "selectLongs mask.size(%lu) and nprop=%lu",
                   static_cast<long unsigned>(mask.size()), nprop);
#endif
        ibis::bitvector::indexSet index = mask.firstIndexSet();
        if (nprop >= mask.size()) { // no need to check loop bounds
#if DEBUG+0 > 0 || _DEBUG+0 > 0
            logMessage("DEBUG", "entering unchecked loops");
#endif
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (index.isRange()) {
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                    logMessage("DEBUG", "copying range [%lu, %lu), i=%lu",
                               static_cast<long unsigned>(*idx0),
                               static_cast<long unsigned>(idx0[1]),
                               static_cast<long unsigned>(i));
#endif
                    for (uint32_t j = *idx0; j < idx0[1]; ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                        logMessage("DEBUG", "copying value %lu to i=%lu",
                                   static_cast<long unsigned>(idx0[j]),
                                   static_cast<long unsigned>(i));
#endif
                        (*array)[i] = (prop[idx0[j]]);
                    }
                }
                ++ index;
            }
        }
        else { // need to check loop bounds against nprop
#if DEBUG+0 > 0 || _DEBUG+0 > 0
            logMessage("DEBUG", "entering checked loops");
#endif
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (*idx0 >= nprop) break;
                if (index.isRange()) {
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                    logMessage("DEBUG", "copying range [%lu, %lu), i=%lu",
                               static_cast<long unsigned>(*idx0),
                               static_cast<long unsigned>
                               (idx0[1]<=nprop ? idx0[1] : nprop),
                               static_cast<long unsigned>(i));
#endif
                    for (uint32_t j = *idx0;
                         j < (idx0[1]<=nprop ? idx0[1] : nprop);
                         ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                        logMessage("DEBUG", "copying value %lu to i=%lu",
                                   static_cast<long unsigned>(idx0[j]),
                                   static_cast<long unsigned>(i));
#endif
                        if (idx0[j] < nprop)
                            (*array)[i] = (prop[idx0[j]]);
                        else
                            break;
                    }
                }
                ++ index;
            }
        }
        if (i != tot) {
            array->resize(i);
            logWarning("selectLongs", "expected to retrieve %lu elements "
                       "but only got %lu", static_cast<long unsigned>(tot),
                       static_cast<long unsigned>(i));
        }
    }
    else if (m_type == ibis::INT) {
        array_t<int32_t> prop;
        ibis::fileManager::ACCESS_PREFERENCE apref =
            thePart != 0 ? thePart->accessHint(mask, sizeof(int32_t))
            : ibis::fileManager::MMAP_LARGE_FILES;

        int ierr = ibis::fileManager::instance().getFile(fnm, prop, apref);
        if (ierr != 0) {
            logWarning("selectLongs",
                       "the file manager faild to retrieve the content of"
                       " the data file \"%s\"", fnm);
            return array.release();
        }

        uint32_t i = 0;
        array->resize(tot);
        const long unsigned nprop = prop.size();
#if DEBUG+0 > 0 || _DEBUG+0 > 0
        logMessage("DEBUG", "selectLongs mask.size(%lu) and nprop=%lu",
                   static_cast<long unsigned>(mask.size()), nprop);
#endif
        ibis::bitvector::indexSet index = mask.firstIndexSet();
        if (nprop >= mask.size()) { // no need to check loop bounds
#if DEBUG+0 > 0 || _DEBUG+0 > 0
            logMessage("DEBUG", "entering unchecked loops");
#endif
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (index.isRange()) {
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                    logMessage("DEBUG", "copying range [%lu, %lu), i=%lu",
                               static_cast<long unsigned>(*idx0),
                               static_cast<long unsigned>(idx0[1]),
                               static_cast<long unsigned>(i));
#endif
                    for (uint32_t j = *idx0; j < idx0[1]; ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                        logMessage("DEBUG", "copying value %lu to i=%lu",
                                   static_cast<long unsigned>(idx0[j]),
                                   static_cast<long unsigned>(i));
#endif
                        (*array)[i] = (prop[idx0[j]]);
                    }
                }
                ++ index;
            }
        }
        else { // need to check loop bounds against nprop
#if DEBUG+0 > 0 || _DEBUG+0 > 0
            logMessage("DEBUG", "entering checked loops");
#endif
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (*idx0 >= nprop) break;
                if (index.isRange()) {
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                    logMessage("DEBUG", "copying range [%lu, %lu), i=%lu",
                               static_cast<long unsigned>(*idx0),
                               static_cast<long unsigned>
                               (idx0[1]<=nprop ? idx0[1] : nprop),
                               static_cast<long unsigned>(i));
#endif
                    for (uint32_t j = *idx0;
                         j < (idx0[1]<=nprop ? idx0[1] : nprop);
                         ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                        logMessage("DEBUG", "copying value %lu to i=%lu",
                                   static_cast<long unsigned>(idx0[j]),
                                   static_cast<long unsigned>(i));
#endif
                        if (idx0[j] < nprop)
                            (*array)[i] = (prop[idx0[j]]);
                        else
                            break;
                    }
                }
                ++ index;
            }
        }
        if (i != tot) {
            array->resize(i);
            logWarning("selectLongs", "expected to retrieve %lu elements "
                       "but only got %lu", static_cast<long unsigned>(tot),
                       static_cast<long unsigned>(i));
        }
    }
    else if (m_type == ibis::USHORT) {
        array_t<uint16_t> prop;
        ibis::fileManager::ACCESS_PREFERENCE apref =
            thePart != 0 ? thePart->accessHint(mask, sizeof(uint16_t))
            : ibis::fileManager::MMAP_LARGE_FILES;

        int ierr = ibis::fileManager::instance().getFile(fnm, prop, apref);
        if (ierr != 0) {
            logWarning("selectLongs",
                       "the file manager faild to retrieve the content of"
                       " the data file \"%s\"", fnm);
            return array.release();
        }

        uint32_t i = 0;
        array->resize(tot);
        const uint32_t nprop = prop.size();
        ibis::bitvector::indexSet index = mask.firstIndexSet();
        if (nprop >= mask.size()) { // no need to check loop bounds
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (index.isRange()) {
                    for (uint32_t j = *idx0; j < idx0[1]; ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        (*array)[i] = (prop[idx0[j]]);
                    }
                }
                ++ index;
            }
        }
        else { // need to check loop bounds against nprop
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (*idx0 >= nprop) break;
                if (index.isRange()) {
                    for (uint32_t j = *idx0;
                         j < (idx0[1]<=nprop ? idx0[1] : nprop);
                         ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        if (idx0[j] < nprop)
                            (*array)[i] = (prop[idx0[j]]);
                        else
                            break;
                    }
                }
                ++ index;
            }
        }
        if (i != tot) {
            array->resize(i);
            logWarning("selectLongs", "expected to retrieve %lu elements "
                       "but only got %lu", static_cast<long unsigned>(tot),
                       static_cast<long unsigned>(i));
        }
    }
    else if (m_type == SHORT) {
        array_t<int16_t> prop;
        ibis::fileManager::ACCESS_PREFERENCE apref =
            thePart != 0 ? thePart->accessHint(mask, sizeof(int16_t))
            : ibis::fileManager::MMAP_LARGE_FILES;

        int ierr = ibis::fileManager::instance().getFile(fnm, prop, apref);
        if (ierr != 0) {
            logWarning("selectLongs",
                       "the file manager faild to retrieve the content of"
                       " the data file \"%s\"", fnm);
            return array.release();
        }

        uint32_t i = 0;
        array->resize(tot);
        const uint32_t nprop = prop.size();
        ibis::bitvector::indexSet index = mask.firstIndexSet();
        if (nprop >= mask.size()) { // no need to check loop bounds
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (index.isRange()) {
                    for (uint32_t j = *idx0; j < idx0[1]; ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        (*array)[i] = (prop[idx0[j]]);
                    }
                }
                ++ index;
            }
        }
        else { // need to check loop bounds against nprop
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (*idx0 >= nprop) break;
                if (index.isRange()) {
                    for (uint32_t j = *idx0;
                         j < (idx0[1]<=nprop ? idx0[1] : nprop);
                         ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        if (idx0[j] < nprop)
                            (*array)[i] = (prop[idx0[j]]);
                        else
                            break;
                    }
                }
                ++ index;
            }
        }
        if (i != tot) {
            array->resize(i);
            logWarning("selectLongs", "expected to retrieve %lu elements "
                       "but only got %lu", static_cast<long unsigned>(tot),
                       static_cast<long unsigned>(i));
        }
    }
    else if (m_type == UBYTE) {
        array_t<unsigned char> prop;
        ibis::fileManager::ACCESS_PREFERENCE apref =
            thePart != 0 ? thePart->accessHint(mask, sizeof(unsigned char))
            : ibis::fileManager::MMAP_LARGE_FILES;

        int ierr = ibis::fileManager::instance().getFile(fnm, prop, apref);
        if (ierr != 0) {
            logWarning("selectLongs",
                       "the file manager faild to retrieve the content of"
                       " the data file \"%s\"", fnm);
            return array.release();
        }

        uint32_t i = 0;
        array->resize(tot);
        const uint32_t nprop = prop.size();
        ibis::bitvector::indexSet index = mask.firstIndexSet();
        if (nprop >= mask.size()) { // no need to check loop bounds
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (index.isRange()) {
                    for (uint32_t j = *idx0; j < idx0[1]; ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        (*array)[i] = (prop[idx0[j]]);
                    }
                }
                ++ index;
            }
        }
        else { // need to check loop bounds against nprop
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (*idx0 >= nprop) break;
                if (index.isRange()) {
                    for (uint32_t j = *idx0;
                         j < (idx0[1]<=nprop ? idx0[1] : nprop);
                         ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        if (idx0[j] < nprop)
                            (*array)[i] = (prop[idx0[j]]);
                        else
                            break;
                    }
                }
                ++ index;
            }
        }
        if (i != tot) {
            array->resize(i);
            logWarning("selectLongs", "expected to retrieve %lu elements "
                       "but only got %lu", static_cast<long unsigned>(tot),
                       static_cast<long unsigned>(i));
        }
    }
    else if (m_type == BYTE) {
        array_t<signed char> prop;
        ibis::fileManager::ACCESS_PREFERENCE apref =
            thePart != 0 ? thePart->accessHint(mask, sizeof(char))
            : ibis::fileManager::MMAP_LARGE_FILES;

        int ierr = ibis::fileManager::instance().getFile(fnm, prop, apref);
        if (ierr != 0) {
            logWarning("selectLongs",
                       "the file manager faild to retrieve the content of"
                       " the data file \"%s\"", fnm);
            return array.release();
        }

        uint32_t i = 0;
        array->resize(tot);
        const uint32_t nprop = prop.size();
        ibis::bitvector::indexSet index = mask.firstIndexSet();
        if (nprop >= mask.size()) { // no need to check loop bounds
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (index.isRange()) {
                    for (uint32_t j = *idx0; j < idx0[1]; ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        (*array)[i] = (prop[idx0[j]]);
                    }
                }
                ++ index;
            }
        }
        else { // need to check loop bounds against nprop
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (*idx0 >= nprop) break;
                if (index.isRange()) {
                    for (uint32_t j = *idx0;
                         j < (idx0[1]<=nprop ? idx0[1] : nprop);
                         ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        if (idx0[j] < nprop)
                            (*array)[i] = (prop[idx0[j]]);
                        else
                            break;
                    }
                }
                ++ index;
            }
        }
        if (i != tot) {
            array->resize(i);
            logWarning("selectLongs", "expected to retrieve %lu elements "
                       "but only got %lu", static_cast<long unsigned>(tot),
                       static_cast<long unsigned>(i));
        }
    }
    else {
        logWarning("selectLongs", "incompatible data type");
    }
    if (ibis::gVerbose > 4) {
        timer.stop();
        long unsigned cnt = mask.cnt();
        logMessage("selectLongs", "retrieving %lu integer%s "
                   "took %g sec(CPU), %g sec(elapsed)",
                   static_cast<long unsigned>(cnt), (cnt > 1 ? "s" : ""),
                   timer.CPUTime(), timer.realTime());
    }
    return array.release();
} // ibis::column::selectLongs

/// Return selected rows of the column in an array_t object.
/// Can be called on all unsigned integral types.
/// @note The caller is responsible for freeing the returned array from any
/// of the selectTypes functions.
ibis::array_t<uint64_t>*
ibis::column::selectULongs(const ibis::bitvector& mask) const {
    std::unique_ptr< ibis::array_t<uint64_t> > array(new array_t<uint64_t>);
    const uint32_t tot = mask.cnt();
    if (dataflag < 0 || tot == 0)
        return array.release();

    ibis::horometer timer;
    if (ibis::gVerbose > 4)
        timer.start();
    std::string sname;
    const char* fnm = dataFileName(sname);
    if (fnm == 0) {
        dataflag = -1;
        return array.release();
    }

    if (m_type == ibis::ULONG) {
#if defined(FASTBIT_PREFER_READ_ALL)
        array_t<uint64_t> prop;
        ibis::fileManager::ACCESS_PREFERENCE apref =
            thePart != 0 ? thePart->accessHint(mask, sizeof(uint64_t))
            : ibis::fileManager::MMAP_LARGE_FILES;

        int ierr = ibis::fileManager::instance().getFile(fnm, prop, apref);
        if (ierr != 0) {
            logWarning("selectULongs",
                       "the file manager faild to retrieve the content of"
                       " the data file \"%s\"", fnm);
            return array.release();
        }

        const long unsigned nprop = prop.size();
#if DEBUG+0 > 0 || _DEBUG+0 > 0
        logMessage("DEBUG", "selectULongs mask.size(%lu) and nprop=%lu",
                   static_cast<long unsigned>(mask.size()), nprop);
#endif
        uint32_t i = 0;
        if (tot >= nprop) {
            ibis::array_t<uint64_t> tmp(prop);
            array->swap(tmp);
            i = nprop;
        }
        else {
            array->resize(tot);
            ibis::bitvector::indexSet index = mask.firstIndexSet();
            if (nprop >= mask.size()) { // no need to check loop bounds
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                logMessage("DEBUG", "entering unchecked loops");
#endif
                while (index.nIndices() > 0) {
                    const ibis::bitvector::word_t *idx0 = index.indices();
                    if (index.isRange()) {
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                        logMessage("DEBUG", "copying range [%lu, %lu), i=%lu",
                                   static_cast<long unsigned>(*idx0),
                                   static_cast<long unsigned>(idx0[1]),
                                   static_cast<long unsigned>(i));
#endif
                        for (uint32_t j = *idx0; j < idx0[1]; ++j, ++i) {
                            (*array)[i] = (prop[j]);
                        }
                    }
                    else {
                        for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                            logMessage("DEBUG", "copying value %lu to i=%lu",
                                       static_cast<long unsigned>(idx0[j]),
                                       static_cast<long unsigned>(i));
#endif
                            (*array)[i] = (prop[idx0[j]]);
                        }
                    }
                    ++ index;
                }
            }
            else { // need to check loop bounds against nprop
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                logMessage("DEBUG", "entering checked loops");
#endif
                while (index.nIndices() > 0) {
                    const ibis::bitvector::word_t *idx0 = index.indices();
                    if (*idx0 >= nprop) break;
                    if (index.isRange()) {
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                        logMessage("DEBUG", "copying range [%lu, %lu), i=%lu",
                                   static_cast<long unsigned>(*idx0),
                                   static_cast<long unsigned>
                                   (idx0[1]<=nprop ? idx0[1] : nprop),
                                   static_cast<long unsigned>(i));
#endif
                        for (uint32_t j = *idx0;
                             j < (idx0[1]<=nprop ? idx0[1] : nprop);
                             ++j, ++i) {
                            (*array)[i] = (prop[j]);
                        }
                    }
                    else {
                        for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                            logMessage("DEBUG", "copying value %lu to i=%lu",
                                       static_cast<long unsigned>(idx0[j]),
                                       static_cast<long unsigned>(i));
#endif
                            if (idx0[j] < nprop)
                                (*array)[i] = (prop[idx0[j]]);
                            else
                                break;
                        }
                    }
                    ++ index;
                }
            }
        }
        if (i != tot) {
            array->resize(i);
            logWarning("selectULongs", "expected to retrieve %lu elements "
                       "but only got %lu", static_cast<long unsigned>(tot),
                       static_cast<long unsigned>(i));
        }
#else
        long ierr = selectValuesT(fnm, mask, *array);
        if (ierr < 0) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- column["
                << (thePart!=0 ? thePart->name() : "") << "." << m_name
                << "]::selectValuesT failed with error code " << ierr;
            array->clear();
        }
#endif
    }
    else if (m_type == ibis::UINT || m_type == ibis::CATEGORY ||
             m_type == ibis::TEXT) {
        array_t<uint32_t> prop;
        ibis::fileManager::ACCESS_PREFERENCE apref =
            thePart != 0 ? thePart->accessHint(mask, sizeof(uint32_t))
            : ibis::fileManager::MMAP_LARGE_FILES;

        int ierr = ibis::fileManager::instance().getFile(fnm, prop, apref);
        if (ierr != 0) {
            logWarning("selectULongs",
                       "the file manager faild to retrieve the content of"
                       " the data file \"%s\"", fnm);
            return array.release();
        }

        uint32_t i = 0;
        array->resize(tot);
        const long unsigned nprop = prop.size();
#if DEBUG+0 > 0 || _DEBUG+0 > 0
        logMessage("DEBUG", "selectULongs mask.size(%lu) and nprop=%lu",
                   static_cast<long unsigned>(mask.size()), nprop);
#endif
        ibis::bitvector::indexSet index = mask.firstIndexSet();
        if (nprop >= mask.size()) { // no need to check loop bounds
#if DEBUG+0 > 0 || _DEBUG+0 > 0
            logMessage("DEBUG", "entering unchecked loops");
#endif
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (index.isRange()) {
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                    logMessage("DEBUG", "copying range [%lu, %lu), i=%lu",
                               static_cast<long unsigned>(*idx0),
                               static_cast<long unsigned>(idx0[1]),
                               static_cast<long unsigned>(i));
#endif
                    for (uint32_t j = *idx0; j < idx0[1]; ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                        logMessage("DEBUG", "copying value %lu to i=%lu",
                                   static_cast<long unsigned>(idx0[j]),
                                   static_cast<long unsigned>(i));
#endif
                        (*array)[i] = (prop[idx0[j]]);
                    }
                }
                ++ index;
            }
        }
        else { // need to check loop bounds against nprop
#if DEBUG+0 > 0 || _DEBUG+0 > 0
            logMessage("DEBUG", "entering checked loops");
#endif
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (*idx0 >= nprop) break;
                if (index.isRange()) {
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                    logMessage("DEBUG", "copying range [%lu, %lu), i=%lu",
                               static_cast<long unsigned>(*idx0),
                               static_cast<long unsigned>
                               (idx0[1]<=nprop ? idx0[1] : nprop),
                               static_cast<long unsigned>(i));
#endif
                    for (uint32_t j = *idx0;
                         j < (idx0[1]<=nprop ? idx0[1] : nprop);
                         ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                        logMessage("DEBUG", "copying value %lu to i=%lu",
                                   static_cast<long unsigned>(idx0[j]),
                                   static_cast<long unsigned>(i));
#endif
                        if (idx0[j] < nprop)
                            (*array)[i] = (prop[idx0[j]]);
                        else
                            break;
                    }
                }
                ++ index;
            }
        }
        if (i != tot) {
            array->resize(i);
            logWarning("selectULongs", "expected to retrieve %lu elements "
                       "but only got %lu", static_cast<long unsigned>(tot),
                       static_cast<long unsigned>(i));
        }
    }
    else if (m_type == ibis::USHORT) {
        array_t<uint16_t> prop;
        ibis::fileManager::ACCESS_PREFERENCE apref =
            thePart != 0 ? thePart->accessHint(mask, sizeof(uint16_t))
            : ibis::fileManager::MMAP_LARGE_FILES;

        int ierr = ibis::fileManager::instance().getFile(fnm, prop, apref);
        if (ierr != 0) {
            logWarning("selectULongs",
                       "the file manager faild to retrieve the content of"
                       " the data file \"%s\"", fnm);
            return array.release();
        }

        uint32_t i = 0;
        array->resize(tot);
        const uint32_t nprop = prop.size();
        ibis::bitvector::indexSet index = mask.firstIndexSet();
        if (nprop >= mask.size()) { // no need to check loop bounds
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (index.isRange()) {
                    for (uint32_t j = *idx0; j < idx0[1]; ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        (*array)[i] = (prop[idx0[j]]);
                    }
                }
                ++ index;
            }
        }
        else { // need to check loop bounds against nprop
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (*idx0 >= nprop) break;
                if (index.isRange()) {
                    for (uint32_t j = *idx0;
                         j < (idx0[1]<=nprop ? idx0[1] : nprop);
                         ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        if (idx0[j] < nprop)
                            (*array)[i] = (prop[idx0[j]]);
                        else
                            break;
                    }
                }
                ++ index;
            }
        }
        if (i != tot) {
            array->resize(i);
            logWarning("selectULongs", "expected to retrieve %lu elements "
                       "but only got %lu", static_cast<long unsigned>(tot),
                       static_cast<long unsigned>(i));
        }
    }
    else if (m_type == UBYTE) {
        array_t<unsigned char> prop;
        ibis::fileManager::ACCESS_PREFERENCE apref =
            thePart != 0 ? thePart->accessHint(mask, sizeof(unsigned char))
            : ibis::fileManager::MMAP_LARGE_FILES;

        int ierr = ibis::fileManager::instance().getFile(fnm, prop, apref);
        if (ierr != 0) {
            logWarning("selectULongs",
                       "the file manager faild to retrieve the content of"
                       " the data file \"%s\"", fnm);
            return array.release();
        }

        uint32_t i = 0;
        array->resize(tot);
        const uint32_t nprop = prop.size();
        ibis::bitvector::indexSet index = mask.firstIndexSet();
        if (nprop >= mask.size()) { // no need to check loop bounds
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (index.isRange()) {
                    for (uint32_t j = *idx0; j < idx0[1]; ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        (*array)[i] = (prop[idx0[j]]);
                    }
                }
                ++ index;
            }
        }
        else { // need to check loop bounds against nprop
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (*idx0 >= nprop) break;
                if (index.isRange()) {
                    for (uint32_t j = *idx0;
                         j < (idx0[1]<=nprop ? idx0[1] : nprop);
                         ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        if (idx0[j] < nprop)
                            (*array)[i] = (prop[idx0[j]]);
                        else
                            break;
                    }
                }
                ++ index;
            }
        }
        if (i != tot) {
            array->resize(i);
            logWarning("selectULongs", "expected to retrieve %lu elements "
                       "but only got %lu", static_cast<long unsigned>(tot),
                       static_cast<long unsigned>(i));
        }
    }
    else {
        logWarning("selectULongs", "incompatible data type");
    }
    if (ibis::gVerbose > 4) {
        timer.stop();
        long unsigned cnt = mask.cnt();
        logMessage("selectULongs", "retrieving %lu integer%s "
                   "took %g sec(CPU), %g sec(elapsed)",
                   static_cast<long unsigned>(cnt), (cnt > 1 ? "s" : ""),
                   timer.CPUTime(), timer.realTime());
    }
    return array.release();
} // ibis::column::selectULongs

/// Put selected values of a float column into an array.
///
/// @note Only performs safe conversion.  Conversions from 32-bit integers,
/// 64-bit integers and 64-bit floating-point values are not allowed.  A
/// nil array will be returned if the current column can not be converted.
/// @note The caller is responsible for freeing the returned array from any
/// of the selectTypes functions.
ibis::array_t<float>*
ibis::column::selectFloats(const ibis::bitvector& mask) const {
    std::unique_ptr< array_t<float> > array(new array_t<float>);
    const uint32_t tot = mask.cnt();
    if (dataflag < 0 || tot == 0)
        return array.release();

    ibis::horometer timer;
    if (ibis::gVerbose > 4)
        timer.start();
    std::string sname;
    const char* fnm = dataFileName(sname);
    if (fnm == 0) {
        dataflag = -1;
        return array.release();
    }

    if (m_type == FLOAT) {
#if defined(FASTBIT_PREFER_READ_ALL)
        array_t<float> prop;
        ibis::fileManager::ACCESS_PREFERENCE apref =
            thePart != 0 ? thePart->accessHint(mask, sizeof(float))
            : ibis::fileManager::MMAP_LARGE_FILES;

        int ierr = ibis::fileManager::instance().getFile(fnm, prop, apref);
        if (ierr != 0) {
            logWarning("selectFloats",
                       "the file manager faild to retrieve the content of"
                       " the data file \"%s\"", fnm);
            return array.release();
        }

        const uint32_t nprop = prop.size();
        uint32_t i = 0;
        if (tot >= nprop) {
            ibis::array_t<float> tmp(prop);
            array->swap(tmp);
            i = nprop;
        }
        else {
            array->resize(tot);
            ibis::bitvector::indexSet index = mask.firstIndexSet();
            if (nprop >= mask.size()) { // no need to check loop bounds
                while (index.nIndices() > 0) {
                    const ibis::bitvector::word_t *idx0 = index.indices();
                    if (index.isRange()) {
                        for (uint32_t j = *idx0; j<idx0[1]; ++j, ++i) {
                            (*array)[i] = (prop[j]);
                        }
                    }
                    else {
                        for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                            (*array)[i] = (prop[idx0[j]]);
                        }
                    }
                    ++ index;
                }
            }
            else { // check loop bounds against nprop
                while (index.nIndices() > 0) {
                    const ibis::bitvector::word_t *idx0 = index.indices();
                    if (*idx0 >= nprop) break;
                    if (index.isRange()) {
                        for (uint32_t j = *idx0;
                             j < (idx0[1]<=nprop ? idx0[1] : nprop);
                             ++j, ++i) {
                            (*array)[i] = (prop[j]);
                        }
                    }
                    else {
                        for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                            if (idx0[j] < nprop)
                                (*array)[i] = (prop[idx0[j]]);
                            else
                                break;
                        }
                    }
                    ++ index;
                }
            }
        }
        if (i != tot) {
            array->resize(i);
            logWarning("selectFloats", "expected to retrieve %lu elements "
                       "but only got %lu", static_cast<long unsigned>(tot),
                       static_cast<long unsigned>(i));
        }
#else
        long ierr = selectValuesT(fnm, mask, *array);
        if (ierr < 0) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- column["
                << (thePart!=0 ? thePart->name() : "") << "." << m_name
                << "]::selectValuesT failed with error code " << ierr;
            array->clear();
        }
#endif
    }
    else if (m_type == ibis::USHORT) {
        array_t<uint16_t> prop;
        ibis::fileManager::ACCESS_PREFERENCE apref =
            thePart != 0 ? thePart->accessHint(mask, sizeof(uint16_t))
            : ibis::fileManager::MMAP_LARGE_FILES;

        int ierr = ibis::fileManager::instance().getFile(fnm, prop, apref);
        if (ierr != 0) {
            logWarning("selectFloats",
                       "the file manager faild to retrieve the content of"
                       " the data file \"%s\"", fnm);
            return array.release();
        }

        uint32_t i = 0;
        array->resize(tot);
        const uint32_t nprop = prop.size();
        ibis::bitvector::indexSet index = mask.firstIndexSet();
        if (nprop >= mask.size()) { // no need to check loop bounds
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (index.isRange()) {
                    for (uint32_t j = *idx0; j < idx0[1]; ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        (*array)[i] = (prop[idx0[j]]);
                    }
                }
                ++ index;
            }
        }
        else { // need to check loop bounds against nprop
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (*idx0 >= nprop) break;
                if (index.isRange()) {
                    for (uint32_t j = *idx0;
                         j < (idx0[1]<=nprop ? idx0[1] : nprop);
                         ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        if (idx0[j] < nprop)
                            (*array)[i] = (prop[idx0[j]]);
                        else
                            break;
                    }
                }
                ++ index;
            }
        }
        if (i != tot) {
            array->resize(i);
            logWarning("selectFloats", "expected to retrieve %lu elements "
                       "but only got %lu", static_cast<long unsigned>(tot),
                       static_cast<long unsigned>(i));
        }
    }
    else if (m_type == SHORT) {
        array_t<int16_t> prop;
        ibis::fileManager::ACCESS_PREFERENCE apref =
            thePart != 0 ? thePart->accessHint(mask, sizeof(int16_t))
            : ibis::fileManager::MMAP_LARGE_FILES;

        int ierr = ibis::fileManager::instance().getFile(fnm, prop, apref);
        if (ierr != 0) {
            logWarning("selectFloats",
                       "the file manager faild to retrieve the content of"
                       " the data file \"%s\"", fnm);
            return array.release();
        }

        uint32_t i = 0;
        array->resize(tot);
        const uint32_t nprop = prop.size();
        ibis::bitvector::indexSet index = mask.firstIndexSet();
        if (nprop >= mask.size()) { // no need to check loop bounds
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (index.isRange()) {
                    for (uint32_t j = *idx0; j < idx0[1]; ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        (*array)[i] = (prop[idx0[j]]);
                    }
                }
                ++ index;
            }
        }
        else { // need to check loop bounds against nprop
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (*idx0 >= nprop) break;
                if (index.isRange()) {
                    for (uint32_t j = *idx0;
                         j < (idx0[1]<=nprop ? idx0[1] : nprop);
                         ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        if (idx0[j] < nprop)
                            (*array)[i] = (prop[idx0[j]]);
                        else
                            break;
                    }
                }
                ++ index;
            }
        }
        if (i != tot) {
            array->resize(i);
            logWarning("selectFloats", "expected to retrieve %lu elements "
                       "but only got %lu", static_cast<long unsigned>(tot),
                       static_cast<long unsigned>(i));
        }
    }
    else if (m_type == UBYTE) {
        array_t<unsigned char> prop;
        ibis::fileManager::ACCESS_PREFERENCE apref =
            thePart != 0 ? thePart->accessHint(mask, sizeof(unsigned char))
            : ibis::fileManager::MMAP_LARGE_FILES;

        int ierr = ibis::fileManager::instance().getFile(fnm, prop, apref);
        if (ierr != 0) {
            logWarning("selectFloats",
                       "the file manager faild to retrieve the content of"
                       " the data file \"%s\"", fnm);
            return array.release();
        }

        uint32_t i = 0;
        array->resize(tot);
        const uint32_t nprop = prop.size();
        ibis::bitvector::indexSet index = mask.firstIndexSet();
        if (nprop >= mask.size()) { // no need to check loop bounds
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (index.isRange()) {
                    for (uint32_t j = *idx0; j < idx0[1]; ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        (*array)[i] = (prop[idx0[j]]);
                    }
                }
                ++ index;
            }
        }
        else { // need to check loop bounds against nprop
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (*idx0 >= nprop) break;
                if (index.isRange()) {
                    for (uint32_t j = *idx0;
                         j < (idx0[1]<=nprop ? idx0[1] : nprop);
                         ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        if (idx0[j] < nprop)
                            (*array)[i] = (prop[idx0[j]]);
                        else
                            break;
                    }
                }
                ++ index;
            }
        }
        if (i != tot) {
            array->resize(i);
            logWarning("selectFloats", "expected to retrieve %lu elements "
                       "but only got %lu", static_cast<long unsigned>(tot),
                       static_cast<long unsigned>(i));
        }
    }
    else if (m_type == BYTE) {
        array_t<signed char> prop;
        ibis::fileManager::ACCESS_PREFERENCE apref =
            thePart != 0 ? thePart->accessHint(mask, sizeof(char))
            : ibis::fileManager::MMAP_LARGE_FILES;

        int ierr = ibis::fileManager::instance().getFile(fnm, prop, apref);
        if (ierr != 0) {
            logWarning("selectFloats",
                       "the file manager faild to retrieve the content of"
                       " the data file \"%s\"", fnm);
            return array.release();
        }

        uint32_t i = 0;
        array->resize(tot);
        const uint32_t nprop = prop.size();
        ibis::bitvector::indexSet index = mask.firstIndexSet();
        if (nprop >= mask.size()) { // no need to check loop bounds
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (index.isRange()) {
                    for (uint32_t j = *idx0; j < idx0[1]; ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        (*array)[i] = (prop[idx0[j]]);
                    }
                }
                ++ index;
            }
        }
        else { // need to check loop bounds against nprop
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (*idx0 >= nprop) break;
                if (index.isRange()) {
                    for (uint32_t j = *idx0;
                         j < (idx0[1]<=nprop ? idx0[1] : nprop);
                         ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        if (idx0[j] < nprop)
                            (*array)[i] = (prop[idx0[j]]);
                        else
                            break;
                    }
                }
                ++ index;
            }
        }
        if (i != tot) {
            array->resize(i);
            logWarning("selectFloats", "expected to retrieve %lu elements "
                       "but only got %lu", static_cast<long unsigned>(tot),
                       static_cast<long unsigned>(i));
        }
    }
    else {
        logWarning("selectFloats", "incompatible data type");
    }
    if (ibis::gVerbose > 4) {
        timer.stop();
        long unsigned cnt = mask.cnt();
        logMessage("selectFloats", "retrieving %lu float value%s "
                   "took %g sec(CPU), %g sec(elapsed)",
                   static_cast<long unsigned>(cnt), (cnt > 1 ? "s" : ""),
                   timer.CPUTime(), timer.realTime());
    }
    return array.release();
} // ibis::column::selectFloats

/// Put the selected values into an array as doubles.
///
/// @note Any numerical values can be converted to doubles, however for
/// 64-bit integers this conversion may cause lose of precision.
///
/// @note The caller is responsible for freeing the returned array from any
/// of the selectTypes functions.
ibis::array_t<double>*
ibis::column::selectDoubles(const ibis::bitvector& mask) const {
    std::unique_ptr< array_t<double> > array(new array_t<double>);
    const uint32_t tot = mask.cnt();
    if (dataflag < 0 || tot == 0)
        return array.release();

    ibis::horometer timer;
    if (ibis::gVerbose > 4)
        timer.start();
    std::string sname;
    const char* fnm = dataFileName(sname);
    if (fnm == 0) {
        dataflag = -1;
        return array.release();
    }

    switch(m_type) {
    case ibis::ULONG: {
        array_t<uint64_t> prop;
        ibis::fileManager::ACCESS_PREFERENCE apref =
            thePart != 0 ? thePart->accessHint(mask, sizeof(uint64_t))
            : ibis::fileManager::MMAP_LARGE_FILES;

        int ierr = ibis::fileManager::instance().getFile(fnm, prop, apref);
        if (ierr != 0) {
            logWarning("selectDoubles"
                       "the file manager faild to retrieve the content of"
                       " the data file \"%s\"", fnm);
            return array.release();
        }

        uint32_t i = 0;
        array->resize(tot);
        const uint32_t nprop = prop.size();
        ibis::bitvector::indexSet index = mask.firstIndexSet();
        if (nprop >= mask.size()) {
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (index.isRange()) {
                    for (uint32_t j = *idx0; j<idx0[1]; ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        (*array)[i] = (prop[idx0[j]]);
                    }
                }
                ++ index;
            }
        }
        else {
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (*idx0 >= nprop) break;
                if (index.isRange()) {
                    for (uint32_t j = *idx0;
                         j<(idx0[1]<=nprop ? idx0[1] : nprop);
                         ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        if (idx0[j] < nprop)
                            (*array)[i] = (prop[idx0[j]]);
                        else
                            break;
                    }
                }
                ++ index;
            }
        }

        if (i != tot) {
            array->resize(i);
            logWarning("selectDoubles", "expected to retrieve %lu elements "
                       "but only got %lu", static_cast<long unsigned>(tot),
                       static_cast<long unsigned>(i));
        }
        else if (ibis::gVerbose > 4) {
            timer.stop();
            long unsigned cnt = mask.cnt();
            logMessage("selectDoubles", "retrieving %lu unsigned integer%s "
                       "took %g sec(CPU), %g sec(elapsed)",
                       static_cast<long unsigned>(cnt), (cnt > 1 ? "s" : ""),
                       timer.CPUTime(), timer.realTime());
        }
        break;}
    case ibis::LONG: {
        array_t<int64_t> prop;
        ibis::fileManager::ACCESS_PREFERENCE apref =
            thePart != 0 ? thePart->accessHint(mask, sizeof(int64_t))
            : ibis::fileManager::MMAP_LARGE_FILES;

        int ierr = ibis::fileManager::instance().getFile(fnm, prop, apref);
        if (ierr != 0) {
            logWarning("selectDoubles"
                       "the file manager faild to retrieve the content of"
                       " the data file \"%s\"", fnm);
            return array.release();
        }

        uint32_t i = 0;
        array->resize(tot);
        const uint32_t nprop = prop.size();
        ibis::bitvector::indexSet index = mask.firstIndexSet();
        if (nprop >= mask.size()) {
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (index.isRange()) {
                    for (uint32_t j = *idx0; j<idx0[1]; ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        (*array)[i] = (prop[idx0[j]]);
                    }
                }
                ++ index;
            }
        }
        else {
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (*idx0 >= nprop) break;
                if (index.isRange()) {
                    for (uint32_t j = *idx0;
                         j<(idx0[1]<=nprop ? idx0[1] : nprop);
                         ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        if (idx0[j] < nprop)
                            (*array)[i] = (prop[idx0[j]]);
                        else
                            break;
                    }
                }
                ++ index;
            }
        }

        if (i != tot) {
            array->resize(i);
            logWarning("selectDoubles", "expected to retrieve %lu elements "
                       "but only got %lu", static_cast<long unsigned>(tot),
                       static_cast<long unsigned>(i));
        }
        else if (ibis::gVerbose > 4) {
            timer.stop();
            long unsigned cnt = mask.cnt();
            logMessage("selectDoubles", "retrieving %lu integer%s "
                       "took %g sec(CPU), %g sec(elapsed)",
                       static_cast<long unsigned>(cnt), (cnt > 1 ? "s" : ""),
                       timer.CPUTime(), timer.realTime());
        }
        break;}
    case ibis::CATEGORY:
    case ibis::UINT: {
        array_t<uint32_t> prop;
        ibis::fileManager::ACCESS_PREFERENCE apref =
            thePart != 0 ? thePart->accessHint(mask, sizeof(uint32_t))
            : ibis::fileManager::MMAP_LARGE_FILES;

        int ierr = ibis::fileManager::instance().getFile(fnm, prop, apref);
        if (ierr != 0) {
            logWarning("selectDoubles"
                       "the file manager faild to retrieve the content of"
                       " the data file \"%s\"", fnm);
            return array.release();
        }

        uint32_t i = 0;
        array->resize(tot);
        const uint32_t nprop = prop.size();
        ibis::bitvector::indexSet index = mask.firstIndexSet();
        if (nprop >= mask.size()) {
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (index.isRange()) {
                    for (uint32_t j = *idx0; j<idx0[1]; ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        (*array)[i] = (prop[idx0[j]]);
                    }
                }
                ++ index;
            }
        }
        else {
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (*idx0 >= nprop) break;
                if (index.isRange()) {
                    for (uint32_t j = *idx0;
                         j<(idx0[1]<=nprop ? idx0[1] : nprop);
                         ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        if (idx0[j] < nprop)
                            (*array)[i] = (prop[idx0[j]]);
                        else
                            break;
                    }
                }
                ++ index;
            }
        }

        if (i != tot) {
            array->resize(i);
            logWarning("selectDoubles", "expected to retrieve %lu elements "
                       "but only got %lu", static_cast<long unsigned>(tot),
                       static_cast<long unsigned>(i));
        }
        else if (ibis::gVerbose > 4) {
            timer.stop();
            long unsigned cnt = mask.cnt();
            logMessage("selectDoubles", "retrieving %lu unsigned integer%s "
                       "took %g sec(CPU), %g sec(elapsed)",
                       static_cast<long unsigned>(cnt), (cnt > 1 ? "s" : ""),
                       timer.CPUTime(), timer.realTime());
        }
        break;}
    case ibis::INT: {
        array_t<int32_t> prop;
        ibis::fileManager::ACCESS_PREFERENCE apref =
            thePart != 0 ? thePart->accessHint(mask, sizeof(int32_t))
            : ibis::fileManager::MMAP_LARGE_FILES;

        int ierr = ibis::fileManager::instance().getFile(fnm, prop, apref);
        if (ierr != 0) {
            logWarning("selectDoubles"
                       "the file manager faild to retrieve the content of"
                       " the data file \"%s\"", fnm);
            return array.release();
        }

        uint32_t i = 0;
        array->resize(tot);
        const uint32_t nprop = prop.size();
        ibis::bitvector::indexSet index = mask.firstIndexSet();
        if (nprop >= mask.size()) {
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (index.isRange()) {
                    for (uint32_t j = *idx0; j<idx0[1]; ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        (*array)[i] = (prop[idx0[j]]);
                    }
                }
                ++ index;
            }
        }
        else {
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (*idx0 >= nprop) break;
                if (index.isRange()) {
                    for (uint32_t j = *idx0;
                         j<(idx0[1]<=nprop ? idx0[1] : nprop);
                         ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        if (idx0[j] < nprop)
                            (*array)[i] = (prop[idx0[j]]);
                        else
                            break;
                    }
                }
                ++ index;
            }
        }

        if (i != tot) {
            array->resize(i);
            logWarning("selectDoubles", "expected to retrieve %lu elements "
                       "but only got %lu", static_cast<long unsigned>(tot),
                       static_cast<long unsigned>(i));
        }
        else if (ibis::gVerbose > 4) {
            timer.stop();
            long unsigned cnt = mask.cnt();
            logMessage("selectDoubles", "retrieving %lu integer%s "
                       "took %g sec(CPU), %g sec(elapsed)",
                       static_cast<long unsigned>(cnt), (cnt > 1 ? "s" : ""),
                       timer.CPUTime(), timer.realTime());
        }
        break;}
    case ibis::USHORT: {
        array_t<uint16_t> prop;
        ibis::fileManager::ACCESS_PREFERENCE apref =
            thePart != 0 ? thePart->accessHint(mask, sizeof(int16_t))
            : ibis::fileManager::MMAP_LARGE_FILES;

        int ierr = ibis::fileManager::instance().getFile(fnm, prop, apref);
        if (ierr != 0) {
            logWarning("selectDoubles"
                       "the file manager faild to retrieve the content of"
                       " the data file \"%s\"", fnm);
            return array.release();
        }

        uint32_t i = 0;
        array->resize(tot);
        const uint32_t nprop = prop.size();
        ibis::bitvector::indexSet index = mask.firstIndexSet();
        if (nprop >= mask.size()) {
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (index.isRange()) {
                    for (uint32_t j = *idx0; j<idx0[1]; ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        (*array)[i] = (prop[idx0[j]]);
                    }
                }
                ++ index;
            }
        }
        else {
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (*idx0 >= nprop) break;
                if (index.isRange()) {
                    for (uint32_t j = *idx0;
                         j<(idx0[1]<=nprop ? idx0[1] : nprop);
                         ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        if (idx0[j] < nprop)
                            (*array)[i] = (prop[idx0[j]]);
                        else
                            break;
                    }
                }
                ++ index;
            }
        }

        if (i != tot) {
            array->resize(i);
            logWarning("selectDoubles", "expected to retrieve %lu elements "
                       "but only got %lu", static_cast<long unsigned>(tot),
                       static_cast<long unsigned>(i));
        }
        else if (ibis::gVerbose > 4) {
            timer.stop();
            long unsigned cnt = mask.cnt();
            logMessage("selectDoubles", "retrieving %lu unsigned short "
                       "integer%s took %g sec(CPU), %g sec(elapsed)",
                       static_cast<long unsigned>(cnt), (cnt > 1 ? "s" : ""),
                       timer.CPUTime(), timer.realTime());
        }
        break;}
    case ibis::SHORT: {
        array_t<int16_t> prop;
        ibis::fileManager::ACCESS_PREFERENCE apref =
            thePart != 0 ? thePart->accessHint(mask, sizeof(int16_t))
            : ibis::fileManager::MMAP_LARGE_FILES;

        int ierr = ibis::fileManager::instance().getFile(fnm, prop, apref);
        if (ierr != 0) {
            logWarning("selectDoubles"
                       "the file manager faild to retrieve the content of"
                       " the data file \"%s\"", fnm);
            return array.release();
        }

        uint32_t i = 0;
        array->resize(tot);
        const uint32_t nprop = prop.size();
        ibis::bitvector::indexSet index = mask.firstIndexSet();
        if (nprop >= mask.size()) {
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (index.isRange()) {
                    for (uint32_t j = *idx0; j<idx0[1]; ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        (*array)[i] = (prop[idx0[j]]);
                    }
                }
                ++ index;
            }
        }
        else {
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (*idx0 >= nprop) break;
                if (index.isRange()) {
                    for (uint32_t j = *idx0;
                         j<(idx0[1]<=nprop ? idx0[1] : nprop);
                         ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        if (idx0[j] < nprop)
                            (*array)[i] = (prop[idx0[j]]);
                        else
                            break;
                    }
                }
                ++ index;
            }
        }

        if (i != tot) {
            array->resize(i);
            logWarning("selectDoubles", "expected to retrieve %lu elements "
                       "but only got %lu", static_cast<long unsigned>(tot),
                       static_cast<long unsigned>(i));
        }
        else if (ibis::gVerbose > 4) {
            timer.stop();
            long unsigned cnt = mask.cnt();
            logMessage("selectDoubles", "retrieving %lu short integer%s "
                       "took %g sec(CPU), %g sec(elapsed)",
                       static_cast<long unsigned>(cnt), (cnt > 1 ? "s" : ""),
                       timer.CPUTime(), timer.realTime());
        }
        break;}
    case ibis::UBYTE: {
        array_t<unsigned char> prop;
        ibis::fileManager::ACCESS_PREFERENCE apref =
            thePart != 0 ? thePart->accessHint(mask, sizeof(unsigned char))
            : ibis::fileManager::MMAP_LARGE_FILES;

        int ierr = ibis::fileManager::instance().getFile(fnm, prop, apref);
        if (ierr != 0) {
            logWarning("selectDoubles"
                       "the file manager faild to retrieve the content of"
                       " the data file \"%s\"", fnm);
            return array.release();
        }

        uint32_t i = 0;
        array->resize(tot);
        const uint32_t nprop = prop.size();
        ibis::bitvector::indexSet index = mask.firstIndexSet();
        if (nprop >= mask.size()) {
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (index.isRange()) {
                    for (uint32_t j = *idx0; j<idx0[1]; ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        (*array)[i] = (prop[idx0[j]]);
                    }
                }
                ++ index;
            }
        }
        else {
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (*idx0 >= nprop) break;
                if (index.isRange()) {
                    for (uint32_t j = *idx0;
                         j<(idx0[1]<=nprop ? idx0[1] : nprop);
                         ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        if (idx0[j] < nprop)
                            (*array)[i] = (prop[idx0[j]]);
                        else
                            break;
                    }
                }
                ++ index;
            }
        }

        if (i != tot) {
            array->resize(i);
            logWarning("selectDoubles", "expected to retrieve %lu elements "
                       "but only got %lu", static_cast<long unsigned>(tot),
                       static_cast<long unsigned>(i));
        }
        else if (ibis::gVerbose > 4) {
            timer.stop();
            long unsigned cnt = mask.cnt();
            logMessage("selectDoubles", "retrieving %lu unsigned 1-byte "
                       "integer%s took %g sec(CPU), %g sec(elapsed)",
                       static_cast<long unsigned>(cnt), (cnt > 1 ? "s" : ""),
                       timer.CPUTime(), timer.realTime());
        }
        break;}
    case ibis::BYTE: {
        array_t<signed char> prop;
        ibis::fileManager::ACCESS_PREFERENCE apref =
            thePart != 0 ? thePart->accessHint(mask, sizeof(char))
            : ibis::fileManager::MMAP_LARGE_FILES;

        int ierr = ibis::fileManager::instance().getFile(fnm, prop, apref);
        if (ierr != 0) {
            logWarning("selectDoubles"
                       "the file manager faild to retrieve the content of"
                       " the data file \"%s\"", fnm);
            return array.release();
        }

        uint32_t i = 0;
        array->resize(tot);
        const uint32_t nprop = prop.size();
        ibis::bitvector::indexSet index = mask.firstIndexSet();
        if (nprop >= mask.size()) {
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (index.isRange()) {
                    for (uint32_t j = *idx0; j<idx0[1]; ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        (*array)[i] = (prop[idx0[j]]);
                    }
                }
                ++ index;
            }
        }
        else {
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (*idx0 >= nprop) break;
                if (index.isRange()) {
                    for (uint32_t j = *idx0;
                         j<(idx0[1]<=nprop ? idx0[1] : nprop);
                         ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        if (idx0[j] < nprop)
                            (*array)[i] = (prop[idx0[j]]);
                        else
                            break;
                    }
                }
                ++ index;
            }
        }

        if (i != tot) {
            array->resize(i);
            logWarning("selectDoubles", "expected to retrieve %lu elements "
                       "but only got %lu", static_cast<long unsigned>(tot),
                       static_cast<long unsigned>(i));
        }
        else if (ibis::gVerbose > 4) {
            timer.stop();
            long unsigned cnt = mask.cnt();
            logMessage("selectDoubles", "retrieving %lu 1-byte integer%s "
                       "took %g sec(CPU), %g sec(elapsed)",
                       static_cast<long unsigned>(cnt), (cnt > 1 ? "s" : ""),
                       timer.CPUTime(), timer.realTime());
        }
        break;}
    case ibis::FLOAT: {
        array_t<float> prop;
        ibis::fileManager::ACCESS_PREFERENCE apref =
            thePart != 0 ? thePart->accessHint(mask, sizeof(float))
            : ibis::fileManager::MMAP_LARGE_FILES;

        int ierr = ibis::fileManager::instance().getFile(fnm, prop, apref);
        if (ierr != 0) {
            logWarning("selectDoubles"
                       "the file manager faild to retrieve the content of"
                       " the data file \"%s\"", fnm);
            return array.release();
        }

        uint32_t i = 0;
        array->resize(tot);
        const uint32_t nprop = prop.size();
        ibis::bitvector::indexSet index = mask.firstIndexSet();
        if (nprop >= mask.size()) {
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (index.isRange()) {
                    for (uint32_t j = *idx0; j<idx0[1]; ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        (*array)[i] = (prop[idx0[j]]);
                    }
                }
                ++ index;
            }
        }
        else {
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (*idx0 >= nprop) break;
                if (index.isRange()) {
                    for (uint32_t j = *idx0;
                         j<(idx0[1]<=nprop ? idx0[1] : nprop);
                         ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        if (idx0[j] < nprop)
                            (*array)[i] = (prop[idx0[j]]);
                        else
                            break;
                    }
                }
                ++ index;
            }
        }

        if (i != tot) {
            array->resize(i);
            logWarning("selectDoubles", "expected to retrieve %lu elements "
                       "but only got %lu", static_cast<long unsigned>(tot),
                       static_cast<long unsigned>(i));
        }
        else if (ibis::gVerbose > 4) {
            timer.stop();
            long unsigned cnt = mask.cnt();
            logMessage("selectDoubles", "retrieving %lu float value%s "
                       "took %g sec(CPU), %g sec(elapsed)",
                       static_cast<long unsigned>(cnt), (cnt > 1 ? "s" : ""),
                       timer.CPUTime(), timer.realTime());
        }
        break;}
    case ibis::DOUBLE: {
#if defined(FASTBIT_PREFER_READ_ALL)
        array_t<double> prop;
        ibis::fileManager::ACCESS_PREFERENCE apref =
            thePart != 0 ? thePart->accessHint(mask, sizeof(double))
            : ibis::fileManager::MMAP_LARGE_FILES;

        int ierr = ibis::fileManager::instance().getFile(fnm, prop, apref);
        if (ierr != 0) {
            logWarning("selectDoubles"
                       "the file manager faild to retrieve the content of"
                       " the data file \"%s\"", fnm);
            return array.release();
        }

        const uint32_t nprop = prop.size();
        uint32_t i = 0;
        if (tot >= nprop) {
            ibis::array_t<double> tmp(prop);
            array->swap(tmp);
            i = nprop;
        }
        else {
            array->resize(tot);
            ibis::bitvector::indexSet index = mask.firstIndexSet();
            if (nprop >= mask.size()) {
                while (index.nIndices() > 0) {
                    const ibis::bitvector::word_t *idx0 = index.indices();
                    if (index.isRange()) {
                        for (uint32_t j = *idx0; j<idx0[1]; ++j, ++i) {
                            (*array)[i] = (prop[j]);
                        }
                    }
                    else {
                        for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                            (*array)[i] = (prop[idx0[j]]);
                        }
                    }
                    ++ index;
                }
            }
            else {
                while (index.nIndices() > 0) {
                    const ibis::bitvector::word_t *idx0 = index.indices();
                    if (*idx0 >= nprop) break;
                    if (index.isRange()) {
                        for (uint32_t j = *idx0;
                             j<(idx0[1]<=nprop ? idx0[1] : nprop);
                             ++j, ++i) {
                            (*array)[i] = (prop[j]);
                        }
                    }
                    else {
                        for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                            if (idx0[j] < nprop)
                                (*array)[i] = (prop[idx0[j]]);
                            else
                                break;
                        }
                    }
                    ++ index;
                }
            }
        }
        if (i != tot) {
            array->resize(i);
            logWarning("selectDoubles", "expected to retrieve %lu elements "
                       "but only got %lu", static_cast<long unsigned>(tot),
                       static_cast<long unsigned>(i));
        }
        else if (ibis::gVerbose > 4) {
            timer.stop();
            long unsigned cnt = mask.cnt();
            logMessage("selectDoubles", "retrieving %lu double value%s "
                       "took %g sec(CPU), %g sec(elapsed)",
                       static_cast<long unsigned>(cnt), (cnt > 1 ? "s" : ""),
                       timer.CPUTime(), timer.realTime());
        }
#else
        long ierr = selectValuesT(fnm, mask, *array);
        if (ierr < 0) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- column["
                << (thePart!=0 ? thePart->name() : "") << "." << m_name
                << "]::selectValuesT failed with error code " << ierr;
            array->clear();
        }
#endif
        break;}
    default: {
        logWarning("selectDoubles", "incompatible data type");
        break;}
    }
    return array.release();
} // ibis::column::selectDoubles

/// Add a custom format for the column to be interpretted as unix time stamps.
void ibis::column::setTimeFormat(const char *nv) {
    if (nv == 0 || *nv == 0) return;
    if (m_utscribe != 0) {
        delete m_utscribe;
    }

    ibis::resource::vList vlst;
    ibis::resource::parseNameValuePairs(nv, vlst);
    const char *fmt = vlst["FORMAT_UNIXTIME_LOCAL"];
    if (fmt != 0 && *fmt != 0) {
        m_utscribe = new ibis::column::unixTimeScribe(fmt);
        return;
    }

    fmt = vlst["FORMAT_UNIXTIME_GMT"];
    if (fmt == 0 || *fmt == 0)
        fmt = vlst["FORMAT_UNIXTIME_UTC"];
    if (fmt != 0 && *fmt != 0) {
        m_utscribe = new ibis::column::unixTimeScribe(fmt, "GMT");
        return;
    }

    fmt = vlst["FORMAT_UNIXTIME"];
    if (fmt == 0 || *fmt == 0)
        fmt = vlst["FORMAT_DATE"];
    if (fmt == 0 || *fmt == 0)
        fmt = vlst["DATE_FORMAT"];
    if (fmt != 0 && *fmt != 0) {
        const char *tz = vlst["tzname"];
        if (tz == 0 || *tz == 0)
            tz = vlst["timezone"];
        if (tz != 0 && (*tz == 'g' || *tz == 'G' || *tz == 'u' || *tz == 'U')) {
            m_utscribe = new ibis::column::unixTimeScribe(fmt, "GMT");
        }
        else {
            m_utscribe = new ibis::column::unixTimeScribe(fmt);
        }
    }
    else {
        LOGGER(ibis::gVerbose > 2)
            << "column::setTimeFormat did not find a value format for unix "
            << "time in \"" << nv << '"';
    }
} // ibis::column::setTimeFormat

void ibis::column::setTimeFormat(const unixTimeScribe &rhs) {
    (*m_utscribe) = rhs;
} // ibis::column::setTimeFormat

/// Select values marked in the bitvector @c mask.  Pack them into the
/// output array @c vals.
///
/// Upon a successful executation, it returns the number of values
/// selected.  If it returns zero (0), the contents of @c vals is not
/// modified.  If it returns a negative number, the contents of arrays @c
/// vals is not guaranteed to be in any particular state.
template <typename T>
long ibis::column::selectValuesT(const char* dfn,
                                 const bitvector& mask,
                                 ibis::array_t<T>& vals) const {
    vals.clear();
    long ierr = -1;
    if (dataflag < 0)
        return ierr;

    const long unsigned tot = mask.cnt();
    if (mask.cnt() == 0) return ierr;
    std::string evt = "column[";
    evt += fullname();
    evt += "]::selectValuesT<";
    evt += typeid(T).name();
    evt += '>';

    LOGGER(ibis::gVerbose > 5)
        << evt << " -- selecting " << tot << " out of " << mask.size()
        << " values from " << (dfn ? dfn : "memory");
    if (tot == mask.size()) { // read all values
        if (dfn != 0 && *dfn != 0)
            ierr = ibis::fileManager::instance().getFile(dfn, vals);
        else
            ierr = getValuesArray(&vals);

        if (ierr >= 0)
            ierr = vals.size();
        return ierr;
    }

    try {
        vals.reserve(tot);
    }
    catch (...) {
        LOGGER(ibis::gVerbose > 1)
            << "Warning -- " << evt
            << " failed to allocate space for vals[" << tot << "]";
        return -2;
    }
    array_t<T> incore; // make the raw storage more friendly
    if (dfn != 0 && *dfn != 0) {
        const off_t sz = ibis::util::getFileSize(dfn);
        if (sz != (off_t)(sizeof(T)*mask.size())) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << evt << " expected file " << dfn
                << " to have " << (sizeof(T)*mask.size()) << " bytes, but got "
                << sz;
            return -4;
        }
        // attempt to read the whole file into memory
        ibis::fileManager::ACCESS_PREFERENCE apref =
            thePart != 0 ? thePart->accessHint(mask, sizeof(T))
            : ibis::fileManager::MMAP_LARGE_FILES;
        ierr = ibis::fileManager::instance().tryGetFile(dfn, incore, apref);
    }
    else {
        ierr = getValuesArray(&incore);
        if (ierr < 0) {
            return -3;
        }
        else if (incore.size() != mask.size()) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << evt << " expected " << mask.size()
                << " elements in memory, but got " << incore.size();
            return -4;
        }
    }

    if (ierr >= 0) { // the file is in memory
        // the content of raw is automatically deallocated through the
        // destructor of ibis::fileManager::incore
        const uint32_t nr = (incore.size() <= mask.size() ?
                             incore.size() : mask.size());
        for (ibis::bitvector::indexSet ix = mask.firstIndexSet();
             ix.nIndices() > 0; ++ ix) {
            const ibis::bitvector::word_t *ixval = ix.indices();
            if (ix.isRange()) {
                const uint32_t stop = (ixval[1] <= nr ? ixval[1] : nr);
                for (uint32_t i = *ixval; i < stop; ++ i) {
                    vals.push_back(incore[i]);
                }
            }
            else {
                for (uint32_t j = 0; j < ix.nIndices(); ++ j) {
                    if (ixval[j] < nr) {
                        vals.push_back(incore[ixval[j]]);
                    }
                    else {
                        break;
                    }
                }
            }
        }
        LOGGER(ibis::gVerbose > 4)
            << "column[" << m_name << "]::selectValuesT got "
            << vals.size() << " values (" << tot << " wanted) from an "
            "in-memory version of file " << (dfn && *dfn ? dfn : "??")
            << " as " << typeid(T).name();
    }
    else { // has to use UnixRead family of functions
        int fdes = UnixOpen(dfn, OPEN_READONLY);
        if (fdes < 0) {
            logWarning("selectValuesT", "failed to open file %s, ierr=%d",
                       dfn, fdes);
            return fdes;
        }
#if defined(_WIN32) && defined(_MSC_VER)
        (void)_setmode(fdes, _O_BINARY);
#endif
        IBIS_BLOCK_GUARD(UnixClose, fdes);
        LOGGER(ibis::gVerbose > 5)
            << "column[" << fullname() << "]::selectValuesT opened file " << dfn
            << " with file descriptor " << fdes << " for reading "
            << typeid(T).name();
        int32_t pos = UnixSeek(fdes, 0L, SEEK_END) / sizeof(T);
        if (pos < 0) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << evt
                << " failed to seek to the end of file " << dfn;
            return -4;
        }

        const uint32_t nr = (pos <= static_cast<int32_t>(thePart->nRows()) ?
                             pos : thePart->nRows());
        for (ibis::bitvector::indexSet ix = mask.firstIndexSet();
             ix.nIndices() > 0; ++ ix) {
            const ibis::bitvector::word_t *ixval = ix.indices();
            if (ix.isRange()) {
                // read the whole group in one-shot
                pos = UnixSeek(fdes, *ixval * sizeof(T), SEEK_SET);
                const uint32_t nelm = (ixval[1]-ixval[0] <= nr-vals.size() ?
                                       ixval[1]-ixval[0] : nr-vals.size());
                ierr = ibis::util::read(fdes, vals.begin()+vals.size(),
                                        nelm*sizeof(T));
                if (ierr > 0) {
                    ierr /=  sizeof(T);
                    vals.resize(vals.size() + ierr);
                    ibis::fileManager::instance().recordPages(pos, pos+ierr);
                    LOGGER(static_cast<uint32_t>(ierr) != nelm &&
                           ibis::gVerbose > 0)
                        << "Warning -- " << evt << " expected to read "
                        << nelm << "consecutive elements (of " << sizeof(T)
                        << " bytes each) from " << dfn
                        << ", but actually read " << ierr;
                }
                else {
                    LOGGER(ibis::gVerbose > 0)
                        << "Warning -- " << evt << " failed to read at "
                        << UnixSeek(fdes, 0L, SEEK_CUR) << " in file "
                        << dfn;
                }
            }
            else {
                // read each value separately
                for (uint32_t j = 0; j < ix.nIndices(); ++j) {
                    const int32_t target = ixval[j] * sizeof(T);
                    pos = UnixSeek(fdes, target, SEEK_SET);
                    if (pos == target) {
                        T tmp;
                        ierr = UnixRead(fdes, &tmp, sizeof(tmp));
                        if (ierr == sizeof(tmp)) {
                            vals.push_back(tmp);
                        }
                        else {
                            LOGGER(ibis::gVerbose > 0)
                                << "Warning -- " << evt << " failed to read "
                                << sizeof(tmp) << "-byte data from offset "
                                << target << " in file \"" << dfn << "\"";
                        }
                    }
                    else {
                        LOGGER(ibis::gVerbose > 0)
                            << "Warning -- " << evt << " failed to seek to the "
                            "expected location in file \"" << dfn
                            << "\" (actual " << pos << ", expected " << target
                            << ")";
                    }
                }
            }
        } // for (ibis::bitvector::indexSet...

        if (ibis::gVerbose > 4)
            logMessage("selectValuesT", "got %lu values (%lu wanted) from "
                       "reading file %s",
                       static_cast<long unsigned>(vals.size()),
                       static_cast<long unsigned>(tot), dfn);
    }

    ierr = vals.size();
    LOGGER(vals.size() != tot && ibis::gVerbose > 0)
        << "Warning -- " << evt << " got " << ierr << " out of "
        << tot << " values from " << (dfn && *dfn ? dfn : "memory");
    return ierr;
} // ibis::column::selectValuesT

/// Select the values marked in the bitvector @c mask.  Pack them into the
/// output array @c vals and fill the array @c inds with the positions of
/// the values selected.
///
/// Upon a successful executation, it returns the number of values
/// selected.  If it returns zero (0), the contents of @c vals and @c inds
/// are not modified.  If it returns a negative number, the contents of
/// arrays @c vals and @c inds are not guaranteed to be in particular
/// state.
template <typename T>
long ibis::column::selectValuesT(const char *dfn,
                                 const bitvector& mask,
                                 ibis::array_t<T>& vals,
                                 ibis::array_t<uint32_t>& inds) const {
    vals.clear();
    inds.clear();
    long ierr = 0;
    const long unsigned tot = mask.cnt();
    if (mask.cnt() == 0)
        return ierr;

    std::string evt = "column[";
    evt += fullname();
    evt += "]::selectValuesT<";
    evt += typeid(T).name();
    evt += '>';
    LOGGER(ibis::gVerbose > 5)
        << evt << " -- selecting " << tot << " out of " << mask.size()
        << " values from " << (dfn ? dfn : "memory");

    try { // make sure we've got enough space for the results
        vals.reserve(tot);
        inds.reserve(tot);
    }
    catch (...) {
        LOGGER(ibis::gVerbose > 1)
            << "Warning -- " << evt
            << " failed to allocate space for vals[" << tot
            << "] and inds[" << tot << "]";
        return -2;
    }

    array_t<T> incore;
    if (dfn != 0 && *dfn != 0) {
        const off_t sz = ibis::util::getFileSize(dfn);
        if (sz != (off_t)(sizeof(T)*mask.size())) {
            dataflag = -1;
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << evt << " expected file " << dfn
                << " to have " << (sizeof(T)*mask.size()) << " bytes, but got "
                << sz;
            return -4;
        }
        // attempt to read the whole file into memory
        ibis::fileManager::ACCESS_PREFERENCE apref =
            thePart != 0 ? thePart->accessHint(mask, sizeof(T))
            : ibis::fileManager::MMAP_LARGE_FILES;
        ierr = ibis::fileManager::instance().tryGetFile(dfn, incore, apref);
    }
    else {
        ierr = getValuesArray(&incore);
        if (ierr < 0) {
            dataflag = -1;
            return -3;
        }
        else if (incore.size() != mask.size()) {
            dataflag = -1;
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << evt << " expected " << mask.size()
                << " elements in memory, but got " << incore.size();
            return -4;
        }
    }

    if (ierr >= 0) { // the file is in memory
        // the content of raw is automatically deallocated through the
        // destructor of incore
        const uint32_t nr = (incore.size() <= mask.size() ?
                             incore.size() : mask.size());
        for (ibis::bitvector::indexSet ix = mask.firstIndexSet();
             ix.nIndices() > 0; ++ ix) {
            const ibis::bitvector::word_t *ixval = ix.indices();
            if (ix.isRange()) {
                const uint32_t stop = (ixval[1] <= nr ? ixval[1] : nr);
                for (uint32_t i = *ixval; i < stop; ++ i) {
                    vals.push_back(incore[i]);
                    inds.push_back(i);
                }
            }
            else {
                for (uint32_t j = 0; j < ix.nIndices(); ++ j) {
                    if (ixval[j] < nr) {
                        vals.push_back(incore[ixval[j]]);
                        inds.push_back(ixval[j]);
                    }
                    else {
                        break;
                    }
                }
            }
        }
        LOGGER(ibis::gVerbose > 4)
            << "column[" << m_name << "]::selectValuesT got "
            << vals.size() << " values (" << tot << " wanted) from an "
            "in-memory version of file " << (dfn && *dfn ? dfn : "??")
            << " as " << typeid(T).name();
    }
    else { // has to use UnixRead family of functions
        int fdes = UnixOpen(dfn, OPEN_READONLY);
        if (fdes < 0) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << evt << " failed to open file "
                << dfn << ", ierr=" << fdes;
            return fdes;
        }
#if defined(_WIN32) && defined(_MSC_VER)
        (void)_setmode(fdes, _O_BINARY);
#endif
        IBIS_BLOCK_GUARD(UnixClose, fdes);
        LOGGER(ibis::gVerbose > 5)
            << "column[" << (thePart?thePart->name():"")
            << '.' << m_name << "]::selectValuesT opened file " << dfn
            << " with file descriptor " << fdes << " for reading "
            << typeid(T).name();
        int32_t pos = UnixSeek(fdes, 0L, SEEK_END) / sizeof(T);
        if (pos < 0) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << evt << " failed to seek to the end of file "
                << dfn;
            return -4;
        }

        const uint32_t nr = (pos <= static_cast<int32_t>(thePart->nRows()) ?
                             pos : thePart->nRows());
        for (ibis::bitvector::indexSet ix = mask.firstIndexSet();
             ix.nIndices() > 0; ++ ix) {
            const ibis::bitvector::word_t *ixval = ix.indices();
            if (ix.isRange()) {
                // read the whole group in one-shot
                pos = UnixSeek(fdes, *ixval * sizeof(T), SEEK_SET);
                const uint32_t nelm = (ixval[1]-ixval[0] <= nr-vals.size() ?
                                       ixval[1]-ixval[0] : nr-vals.size());
                ierr = ibis::util::read(fdes, vals.begin()+vals.size(),
                                        nelm*sizeof(T));
                if (ierr > 0) {
                    ierr /=  sizeof(T);
                    vals.resize(vals.size() + ierr);
                    for (int i = 0; i < ierr; ++ i)
                        inds.push_back(i + *ixval);
                    ibis::fileManager::instance().recordPages(pos, pos+ierr);
                    LOGGER(static_cast<uint32_t>(ierr) != nelm &&
                           ibis::gVerbose > 0)
                        << "Warning -- " << evt << " expected to read "
                        << nelm << "consecutive elements (of " << sizeof(T)
                        << " bytes each) from " << dfn
                        << ", but actually read " << ierr;
                }
                else {
                    LOGGER(ibis::gVerbose > 0)
                        << "Warning -- " << evt << " failed to read at "
                        << UnixSeek(fdes, 0L, SEEK_CUR) << " in file "
                        << dfn;
                }
            }
            else {
                // read each value separately
                for (uint32_t j = 0; j < ix.nIndices(); ++j) {
                    const int32_t target = ixval[j] * sizeof(T);
                    pos = UnixSeek(fdes, target, SEEK_SET);
                    if (pos == target) {
                        T tmp;
                        ierr = UnixRead(fdes, &tmp, sizeof(tmp));
                        if (ierr == sizeof(tmp)) {
                            vals.push_back(tmp);
                            inds.push_back(ixval[j]);
                        }
                        else {
                            LOGGER(ibis::gVerbose > 0)
                                << "Warning -- " << evt << " failed to read "
                                << sizeof(tmp) << "-byte data from offset "
                                << target << " in file \"" << dfn << "\"";
                        }
                    }
                    else {
                        LOGGER(ibis::gVerbose > 0)
                            << "Warning -- " << evt << " failed to seek to the "
                            "expected location in file \"" << dfn
                            << "\" (actual " << pos << ", expected " << target
                            << ")";
                    }
                }
            }
        } // for (ibis::bitvector::indexSet...

        LOGGER(ibis::gVerbose > 4)
            << evt << " -- got " << vals.size() << " values (" << tot
            << " wanted) from file " << dfn;
    }

    ierr = (vals.size() <= inds.size() ? vals.size() : inds.size());
    vals.resize(ierr);
    inds.resize(ierr);
    LOGGER(vals.size() != tot && ibis::gVerbose > 0)
        << "Warning -- " << evt << " got " << ierr << " out of "
        << tot << " values from " << (dfn && *dfn ? dfn : "memory");
    return ierr;
} // ibis::column::selectValuesT

/// Return selected rows of the column in an array_t object.
/// The caller must provide the correct array_t<type>* for vals!  No type
/// casting is performed in this function.  Only elementary numerical types
/// are supported.
long ibis::column::selectValues(const bitvector& mask, void* vals) const {
    if (vals == 0) return -1L;
    if (dataflag < 0 || thePart == 0) return -2L;
    std::string sname;
    const char *dfn = dataFileName(sname);

    switch (m_type) {
    case ibis::BYTE:
        return selectValuesT
            (dfn, mask, *static_cast<array_t<signed char>*>(vals));
    case ibis::UBYTE:
        return selectValuesT
            (dfn, mask, *static_cast<array_t<unsigned char>*>(vals));
    case ibis::SHORT:
        return selectValuesT
            (dfn, mask, *static_cast<array_t<int16_t>*>(vals));
    case ibis::USHORT:
        return selectValuesT
            (dfn, mask, *static_cast<array_t<uint16_t>*>(vals));
    case ibis::INT:
        return selectValuesT
            (dfn, mask, *static_cast<array_t<int32_t>*>(vals));
    case ibis::UINT:
        return selectValuesT
            (dfn, mask, *static_cast<array_t<uint32_t>*>(vals));
    case ibis::LONG:
        return selectValuesT
            (dfn, mask, *static_cast<array_t<int64_t>*>(vals));
    case ibis::ULONG:
        return selectValuesT
            (dfn, mask, *static_cast<array_t<uint64_t>*>(vals));
    case ibis::FLOAT:
        return selectValuesT
            (dfn, mask, *static_cast<array_t<float>*>(vals));
    case ibis::DOUBLE:
        return selectValuesT
            (dfn, mask, *static_cast<array_t<double>*>(vals));
    case ibis::OID:
        return selectValuesT
            (dfn, mask, *static_cast<array_t<ibis::rid_t>*>(vals));
    case ibis::CATEGORY: {
        if (dfn != 0 && *dfn != 0) {
            sname += ".int";
            dfn = sname.c_str();
            return selectValuesT
                (dfn, mask, *static_cast<array_t<uint32_t>*>(vals));
        }
        else {
            return -4L;
        }
    }
    default:
        LOGGER(ibis::gVerbose > 1)
            << "Warning -- column[" << fullname()
            << "]::selectValues is not able to handle data type "
            << ibis::TYPESTRING[(int)m_type];
        return -5L;
    }
} // ibis::column::selectValues

/// Return selected rows of the column in an array_t object along with
/// their positions.
/// The caller must provide the correct array_t<type>* for vals!  No type
/// casting is performed in this function.  Only elementary numerical types
/// are supported.
long ibis::column::selectValues(const bitvector& mask, void* vals,
                                ibis::array_t<uint32_t>& inds) const {
    if (vals == 0)
        return -1L;
    if (dataflag < 0 || thePart == 0) return -2L;
    std::string sname;
    const char *dfn = dataFileName(sname);

    switch (m_type) {
    case ibis::BYTE:
        return selectValuesT
            (dfn, mask, *static_cast<array_t<signed char>*>(vals), inds);
    case ibis::UBYTE:
        return selectValuesT
            (dfn, mask, *static_cast<array_t<unsigned char>*>(vals), inds);
    case ibis::SHORT:
        return selectValuesT
            (dfn, mask, *static_cast<array_t<int16_t>*>(vals), inds);
    case ibis::USHORT:
        return selectValuesT
            (dfn, mask, *static_cast<array_t<uint16_t>*>(vals), inds);
    case ibis::INT:
        return selectValuesT
            (dfn, mask, *static_cast<array_t<int32_t>*>(vals), inds);
    case ibis::UINT:
        return selectValuesT
            (dfn, mask, *static_cast<array_t<uint32_t>*>(vals), inds);
    case ibis::LONG:
        return selectValuesT
            (dfn, mask, *static_cast<array_t<int64_t>*>(vals), inds);
    case ibis::ULONG:
        return selectValuesT
            (dfn, mask, *static_cast<array_t<uint64_t>*>(vals), inds);
    case ibis::FLOAT:
        return selectValuesT
            (dfn, mask, *static_cast<array_t<float>*>(vals), inds);
    case ibis::DOUBLE:
        return selectValuesT
            (dfn, mask, *static_cast<array_t<double>*>(vals), inds);
    case ibis::OID:
        return selectValuesT
            (dfn, mask, *static_cast<array_t<ibis::rid_t>*>(vals), inds);
    case ibis::CATEGORY: {
        sname += ".int";
        dfn = sname.c_str();
        return selectValuesT
            (dfn, mask, *static_cast<array_t<uint32_t>*>(vals), inds);}
    default:
        LOGGER(ibis::gVerbose > 1)
            << "Warning -- column[" << fullname()
            << "]::selectValues is not able to handle data type "
            << ibis::TYPESTRING[(int)m_type];
        return -4L;
    }
} // ibis::column::selectValues

/// Extract the values masked 1 and convert them to strings.
template <typename T>
long ibis::column::selectToStrings(const char* dfn, const bitvector& mask,
                                   std::vector<std::string>& str) const {
    ibis::array_t<T> tmp;
    long ierr = selectValuesT<T>(dfn, mask, tmp);
    if (ierr <= 0) {
        str.clear();
        return ierr;
    }
    LOGGER(tmp.size() != mask.cnt() && ibis::gVerbose > 1)
        << "Warning -- column[" << fullname() << "]::selectToStrings<"
        << typeid(T).name() << "> retrieved " << tmp.size() << " value"
        << (tmp.size()>1?"s":"") << ", but expected " << mask.cnt();

    try {
        str.resize(tmp.size());
        for (size_t ii = 0; ii < tmp.size(); ++ ii) {
            std::ostringstream oss;
            oss << tmp[ii];
            str[ii] = oss.str();
        }
    }
    catch (...) {
        ierr = -5;
        LOGGER(ibis::gVerbose > 1)
            << "Warning -- column[" << fullname() << "]::selectToStrings<"
            << typeid(T).name() << "> failed to convert " << tmp.size()
            << " value" << (tmp.size()>1?"s":"") << " into string"
            << (tmp.size()>1?"s":"");
    }
    return ierr;
} // ibis::column::selectToStrings

template <> long ibis::column::selectToStrings<signed char>
(const char* dfn, const bitvector& mask, std::vector<std::string>& str) const {
    ibis::array_t<signed char> tmp;
    long ierr = selectValuesT<signed char>(dfn, mask, tmp);
    if (ierr <= 0) {
        str.clear();
        return ierr;
    }
    LOGGER(tmp.size() != mask.cnt() && ibis::gVerbose > 1)
        << "Warning -- column[" << fullname()
        << "]::selectToStrings<char> retrieved " << tmp.size() << " value"
        << (tmp.size()>1?"s":"") << ", but expected " << mask.cnt();

    try {
        str.resize(tmp.size());
        for (size_t ii = 0; ii < tmp.size(); ++ ii) {
            std::ostringstream oss;
            oss << (int)tmp[ii];
            str[ii] = oss.str();
        }
    }
    catch (...) {
        ierr = -5;
        LOGGER(ibis::gVerbose > 1)
            << "Warning -- column[" << fullname() 
            << "]::selectToStrings<char> failed to convert "
            << tmp.size() << " value" << (tmp.size()>1?"s":"")
            << " into string" << (tmp.size()>1?"s":"");
    }
    return ierr;
} // ibis::column::selectToStrings

template <> long ibis::column::selectToStrings<unsigned char>
(const char* dfn, const bitvector& mask, std::vector<std::string>& str) const {
    ibis::array_t<unsigned char> tmp;
    long ierr = selectValuesT<unsigned char>(dfn, mask, tmp);
    if (ierr <= 0) {
        str.clear();
        return ierr;
    }
    LOGGER(tmp.size() != mask.cnt() && ibis::gVerbose > 1)
        << "Warning -- column[" << fullname()
        << "]::selectToStrings<unsigned char> retrieved "
        << tmp.size() << " value" << (tmp.size()>1?"s":"") << ", but expected "
        << mask.cnt();

    try {
        str.resize(tmp.size());
        for (size_t ii = 0; ii < tmp.size(); ++ ii) {
            std::ostringstream oss;
            oss << (unsigned)tmp[ii];
            str[ii] = oss.str();
        }
    }
    catch (...) {
        ierr = -5;
        LOGGER(ibis::gVerbose > 1)
            << "Warning -- column[" << fullname()
            << "]::selectToStrings<unsigned char> failed to convert "
            << tmp.size() << " value" << (tmp.size()>1?"s":"")
            << " into string" << (tmp.size()>1?"s":"");
    }
    return ierr;
} // ibis::column::selectToStrings

/// Return the selected rows as strings.  This version returns a
/// std::vector<std::string>, which provides wholly self-contained string
/// values.  It may take more memory than necessary, and the memory usage
/// of std::string is not tracked by FastBit.  The advantage is that it
/// should work regardless of the actual data type of the column.
std::vector<std::string>*
ibis::column::selectStrings(const bitvector& mask) const {
    std::vector<std::string> *res = 0;
    if (dataflag < 0 || thePart == 0) return 0;
    std::string sname;
    const char *dfn = dataFileName(sname);
    if (dfn == 0 || *dfn == 0) {
        dataflag = -1;
        return res;
    }

    res = new std::vector<std::string>(mask.cnt());
    if (mask.cnt() == 0) return res;

    long ierr = 0;
    switch (m_type) {
    case ibis::BYTE:
        ierr = selectToStrings<signed char>(dfn, mask, *res);
        break;
    case ibis::UBYTE:
        ierr = selectToStrings<unsigned char>(dfn, mask, *res);
        break;
    case ibis::SHORT:
        ierr = selectToStrings<int16_t>(dfn, mask, *res);
        break;
    case ibis::USHORT:
        ierr = selectToStrings<uint16_t>(dfn, mask, *res);
        break;
    case ibis::INT:
        ierr = selectToStrings<int32_t>(dfn, mask, *res);
        break;
    case ibis::UINT:
        ierr = selectToStrings<uint32_t>(dfn, mask, *res);
        break;
    case ibis::LONG:
        ierr = selectToStrings<int64_t>(dfn, mask, *res);
        break;
    case ibis::ULONG:
        ierr = selectToStrings<uint64_t>(dfn, mask, *res);
        break;
    case ibis::FLOAT:
        ierr = selectToStrings<float>(dfn, mask, *res);
        break;
    case ibis::DOUBLE:
        ierr = selectToStrings<double>(dfn, mask, *res);
        break;
    case ibis::OID:
        ierr = selectToStrings<ibis::rid_t>(dfn, mask, *res);
        break;
    default:
        LOGGER(ibis::gVerbose > 1)
            << "Warning -- column["
            << (thePart!=0 ? thePart->name() : "") << "." << m_name
            << "]::selectStrings is not able to handle data type "
            << ibis::TYPESTRING[(int)m_type];
        ierr = -2L;
    }

    if (ierr <= 0) {
        delete res;
        res = 0;

        LOGGER(ibis::gVerbose > 1)
            << "Warning -- column["
            << (thePart!=0 ? thePart->name() : "") << "." << m_name
            << "]::selectStrings failed with error code " << ierr;
    }
    return res;
} // ibis::column::selectStrings

std::vector<ibis::opaque>*
ibis::column::selectOpaques(const bitvector& mask) const {
    LOGGER(ibis::gVerbose >= 0)
        << "Warning -- column[" << (thePart!=0 ? thePart->name() : "")
        << "." << m_name << "]::selectOpaque not yet implemented";
    return 0;
} // ibis::column::selectOpaques

int ibis::column::getOpaque(uint32_t irow, ibis::opaque &opq) const {
    if (dataflag < 0 || thePart == 0) {
        return -2;
    }
    else if (irow > thePart->nRows()) {
        return -3;
    }

    ibis::fileManager::storage *tmp = getRawData();
    if (tmp == 0) {
        dataflag = -1;
        return -4;
    }

    int ierr = 0;
    switch (m_type) {
    case ibis::BYTE: {
        array_t<char> ta(tmp);
        if (ta.size() > irow) {
            opq.copy(&(ta[irow]), sizeof(char));
        }
        else {
            ierr = -5;
        }
        break;}
    case ibis::UBYTE: {
        array_t<unsigned char> ta(tmp);
        if (ta.size() > irow) {
            opq.copy((const char*)(&(ta[irow])), sizeof(char));
        }
        else {
            ierr = -5;
        }
        break;}
    case ibis::SHORT: {
        array_t<int16_t> ta(tmp);
        if (ta.size() > irow) {
            opq.copy((const char*)(&(ta[irow])), sizeof(int16_t));
        }
        else {
            ierr = -5;
        }
        break;}
    case ibis::USHORT: {
        array_t<uint16_t> ta(tmp);
        if (ta.size() > irow) {
            opq.copy((const char*)(&(ta[irow])), sizeof(uint16_t));
        }
        else {
            ierr = -5;
        }
        break;}
    case ibis::INT: {
        array_t<int32_t> ta(tmp);
        if (ta.size() > irow) {
            opq.copy((const char*)(&(ta[irow])), sizeof(int32_t));
        }
        else {
            ierr = -5;
        }
        break;}
    case ibis::UINT: {
        array_t<uint32_t> ta(tmp);
        if (ta.size() > irow) {
            opq.copy((const char*)(&(ta[irow])), sizeof(int32_t));
        }
        else {
            ierr = -5;
        }
        break;}
    case ibis::LONG: {
        array_t<int64_t> ta(tmp);
        if (ta.size() > irow) {
            opq.copy((const char*)(&(ta[irow])), sizeof(int64_t));
        }
        else {
            ierr = -5;
        }
        break;}
    case ibis::ULONG: {
        array_t<uint64_t> ta(tmp);
        if (ta.size() > irow) {
            opq.copy((const char*)(&(ta[irow])), sizeof(uint64_t));
        }
        else {
            ierr = -5;
        }
        break;}
    case ibis::FLOAT: {
        array_t<float> ta(tmp);
        if (ta.size() > irow) {
            opq.copy((const char*)(&(ta[irow])), sizeof(float));
        }
        else {
            ierr = -5;
        }
        break;}
    case ibis::DOUBLE: {
        array_t<double> ta(tmp);
        if (ta.size() > irow) {
            opq.copy((const char*)(&(ta[irow])), sizeof(double));
        }
        else {
            ierr = -5;
        }
        break;}
    case ibis::OID: {
        array_t<ibis::rid_t> ta(tmp);
        if (ta.size() > irow) {
            opq.copy((const char*)(&(ta[irow])), sizeof(ibis::rid_t));
        }
        else {
            ierr = -5;
        }
        break;}
    default: {
        LOGGER(ibis::gVerbose > 1)
            << "Warning -- column::getOpaque does not support data type "
            << ibis::TYPESTRING[static_cast<int>(m_type)];
        ierr = -6;
        break;}
    }
    return ierr;
} // ibis::column::getOpaque

/// Select the values satisfying the specified range condition.
long ibis::column::selectValues(const ibis::qContinuousRange& cond,
                                void* vals) const {
    if (dataflag < 0 || thePart == 0) return -2;
    if (thePart->nRows() == 0) return 0;

    long ierr = -1;
    if (idx != 0 || (indexSize() >> 2) < thePart->nRows()) {
        ibis::column::indexLock lock(this, "selectValues");
        if (idx != 0 &&
            idx->estimateCost(cond) < (thePart->nRows() >> 2))
            ierr = idx->select(cond, vals);
    }
    if (ierr < 0) {
        ibis::bitvector nm;
        getNullMask(nm);
        ierr = thePart->doScan(cond, nm, vals);
    }
    return ierr;
} // ibis::column::selectValues

/// Generate a SQL style fully qualified name of the form
/// part-name.column-name.  If the part-name is not available, it will
/// simply return the current column name.  If the part-name is available,
/// but this column's name is empty, the column name part will be filled
/// with a single question mark.
std::string ibis::column::fullname() const {
    if (thePart != 0) {
        std::string fn;
        fn = thePart->name();
        fn += '.';
        if (m_name.empty())
            fn += '?';
        else
            fn += m_name;
        return fn;
    }
    else if (!m_name.empty()) {
        return m_name;
    }
    else {
        return std::string("?");
    }
} // ibis::column::fullname

// only write some information about the column
void ibis::column::print(std::ostream& out) const {
    out << m_name.c_str() << ": " << m_desc.c_str()
        << " (" << ibis::TYPESTRING[(int)m_type] << ") [" << lower << ", "
        << upper << "]";
    if (m_utscribe != 0)
        out << "{" << m_utscribe->format_ << ", " << m_utscribe->timezone_ << "}";
} // ibis::column::print

// three error logging functions
void ibis::column::logError(const char* event, const char* fmt, ...) const {
#if (defined(HAVE_VPRINTF) || defined(_WIN32)) && ! defined(DISABLE_VPRINTF)
    char* s = new char[std::strlen(fmt)+MAX_LINE];
    if (s != 0) {
        va_list args;
        va_start(args, fmt);
        vsprintf(s, fmt, args);
        va_end(args);

        { // make sure the message is written before throwing
            ibis::util::logger lg;
            lg() << " Error *** column["
                 << (thePart != 0 ? thePart->name() : "")
                 << '.' << m_name.c_str() << "]("
                 << ibis::TYPESTRING[(int)m_type] << ")::" << event
                 << " -- " << s;
            if (errno != 0)
                lg() << " ... " << strerror(errno);
        }
        throw s;
    }
    else {
#endif
        {
            ibis::util::logger lg;
            lg() <<  " Error *** column["
                 << (thePart != 0 ? thePart->name() : "")
                 << '.' << m_name.c_str() << "]("
                 << ibis::TYPESTRING[(int)m_type] << ")::" << event
                 << " -- " << fmt;
            if (errno != 0)
                lg() << " ... " << strerror(errno);
        }
        throw fmt;
#if (defined(HAVE_VPRINTF) || defined(_WIN32)) && ! defined(DISABLE_VPRINTF)
    }
#endif
} // ibis::column::logError

void ibis::column::logWarning(const char* event, const char* fmt, ...) const {
    if (ibis::gVerbose < 0)
        return;
    char tstr[28];
    ibis::util::getLocalTime(tstr);
    FILE* fptr = ibis::util::getLogFile();

    ibis::util::ioLock lock;
    fprintf(fptr, "%s\nWarning -- column[%s.%s](%s)::%s -- ",
            tstr, (thePart!=0 ? thePart->name() : ""), m_name.c_str(),
            ibis::TYPESTRING[(int)m_type], event);

#if (defined(HAVE_VPRINTF) || defined(_WIN32)) && ! defined(DISABLE_VPRINTF)
    va_list args;
    va_start(args, fmt);
    vfprintf(fptr, fmt, args);
    va_end(args);
#else
    fprintf(fptr, "%s ... ", fmt);
#endif
    if (errno != 0) {
        if (errno != ENOENT)
            fprintf(fptr, " ... %s", strerror(errno));
        errno = 0;
    }
    fprintf(fptr, "\n");
    fflush(fptr);
} // ibis::column::logWarning

void ibis::column::logMessage(const char* event, const char* fmt, ...) const {
    FILE* fptr = ibis::util::getLogFile();
    ibis::util::ioLock lock;
#if defined(FASTBIT_TIMED_LOG)
    char tstr[28];
    ibis::util::getLocalTime(tstr);
    fprintf(fptr, "%s   ", tstr);
#endif
    fprintf(fptr, "column[%s.%s](%s)::%s -- ",
            (thePart != 0 ? thePart->name() : ""),
            m_name.c_str(), ibis::TYPESTRING[(int)m_type], event);
#if (defined(HAVE_VPRINTF) || defined(_WIN32)) && ! defined(DISABLE_VPRINTF)
    va_list args;
    va_start(args, fmt);
    vfprintf(fptr, fmt, args);
    va_end(args);
#else
    fprintf(fptr, "%s ...", fmt);
#endif
    fprintf(fptr, "\n");
    fflush(fptr);
} // ibis::column::logMessage

int ibis::column::attachIndex(double *keys, uint64_t nkeys,
                              int64_t *offsets, uint64_t noffsets,
                              void *bms, FastBitReadBitmaps rd) const {
    if (keys == 0 || nkeys == 0 || offsets == 0 || noffsets == 0 ||
        bms == 0 || rd == 0)
        return -1;

    unloadIndex();

    std::string evt(fullname());
    evt += "::attachIndex";
    softWriteLock lock(this, evt.c_str());
    if (lock.isLocked() && 0 == idx) {
        if (nkeys == 2*(noffsets-1)) {
            idx = new ibis::bin(this, static_cast<uint32_t>(noffsets-1),
                                keys, offsets, bms, rd);
            if (mask_.size() == 0 && idx != 0 && idx->getNRows() > 0) {
                const_cast<ibis::bitvector&>(mask_).set(1, idx->getNRows());

                if (ibis::gVerbose > 4) {
                    ibis::util::logger lg;
                    lg() << evt << " reconstructed index from " << nkeys
                         << " key" << (nkeys>1?"s":"") << noffsets-1
                         << " bitmap" << (noffsets>2?"s":"") << " stored at "
                         << bms << "\n";
                    idx->print(lg());
                }
            }
            return 0;
        }
        else if (nkeys+1 == noffsets) {
            idx = new ibis::relic(this, static_cast<uint32_t>(nkeys),
                                  keys, offsets, bms, rd);
            if (mask_.size() == 0 && idx != 0 && idx->getNRows() > 0) {
                const_cast<ibis::bitvector&>(mask_).set(1, idx->getNRows());

                if (ibis::gVerbose > 4) {
                    ibis::util::logger lg;
                    lg() << evt << " reconstructed index from " << nkeys
                         << " key" << (nkeys>1?"s":"") << noffsets-1
                         << " bitmap" << (noffsets>2?"s":"") << " stored at "
                         << bms << "\n";
                    idx->print(lg());
                }
            }
            return 0;
        }
        else {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << evt << " encounters mismatching nkeys ("
                << nkeys << ") and noffsets (" << noffsets << ')';
            return -2;
        }
    }
    else if (idx != 0) {
        return 1; // someone else has set the index
    }
    else {
        return -3; // failed to acquire the necessary lock on the object
    }
} // ibis::column::attachIndex

int ibis::column::attachIndex(double *keys, uint64_t nkeys,
                              int64_t *offsets, uint64_t noffsets,
                              uint32_t *bms, uint64_t nbms) const {
    if (keys == 0 || nkeys == 0 || offsets == 0 || noffsets == 0 ||
        bms == 0 || nbms == 0 || offsets[noffsets-1] > nbms)
        return -1;

    unloadIndex();

    std::string evt(fullname());
    evt += "::attachIndex";
    softWriteLock lock(this, evt.c_str());
    if (lock.isLocked() && 0 == idx) {
        if (nkeys == 2*(noffsets-1)) {
            idx = new ibis::bin(this, static_cast<uint32_t>(noffsets-1),
                                keys, offsets, bms);
            if (mask_.size() == 0 && idx != 0 && idx->getNRows() > 0)
                const_cast<ibis::bitvector&>(mask_).set(1, idx->getNRows());
            return 0;
        }
        else if (nkeys+1 == noffsets) {
            idx = new ibis::relic(this, static_cast<uint32_t>(nkeys),
                                  keys, offsets, bms);
            if (mask_.size() == 0 && idx != 0 && idx->getNRows() > 0)
                const_cast<ibis::bitvector&>(mask_).set(1, idx->getNRows());
            return 0;
        }
        else {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << evt << " encounters mismatching nkeys ("
                << nkeys << ") and noffsets (" << noffsets << ')';
            return -2;
        }
    }
    else if (idx != 0) {
        return 1; // someone else has set the index
    }
    else {
        return -3; // failed to acquire the necessary lock on the object
    }
} // ibis::column::attachIndex

/// Load the index associated with the column.
/// @param iopt This option is passed to ibis::index::create to be used if a
/// new index is to be created.
/// @param ropt This option is passed to ibis::index::create to control the
/// reading operations for reconstitute the index object from an index file.
///
/// @note Accesses to this function are serialized through a write lock on
/// the column.  It blocks while acquire the write lock.
void ibis::column::loadIndex(const char* iopt, int ropt) const throw () {
    if ((idx != 0 && !idx->empty()) || (thePart != 0 && thePart->nRows() == 0))
        return;
    if (iopt == 0 || *iopt == static_cast<char>(0))
        iopt = indexSpec(); // index spec of the column
    if ((iopt == 0 || *iopt == static_cast<char>(0)) && thePart != 0)
        iopt = thePart->indexSpec(); // index spec of the table
    if (iopt == 0 || *iopt == static_cast<char>(0)) {
        // attempt to retrieve the value of tableName.columnName.index for
        // the index specification in the global resource
        std::string idxnm;
        if (thePart != 0) {
            idxnm = thePart->name();
            idxnm += '.';
        }
        idxnm += m_name;
        idxnm += ".index";
        iopt = ibis::gParameters()[idxnm.c_str()];
    }
    if (iopt != 0) {
        // no index is to be used if the index specification start
        // with "noindex", "null" or "none".
        if (strncmp(iopt, "noindex", 7) == 0 ||
            strncmp(iopt, "null", 4) == 0 ||
            strncmp(iopt, "none", 4) == 0) {
            return;
        }
    }

    std::string evt = "column";
    if (ibis::gVerbose > 1) {
        evt += '[';
        evt += fullname();
        evt += ']';
    }
    evt += "::loadIndex";
    writeLock lock(this, evt.c_str());
    if (idx != 0) {
        if (idx->empty()) {
            delete idx;
            idx = 0;
        }
        else {
            return;
        }
    }

    ibis::index* tmp = 0;
    try { // if an index is not available, create one
        LOGGER(ibis::gVerbose > 4)
            << evt << " -- loading the index from "
            << (thePart != 0 ?
                (thePart->currentDataDir() ? thePart->currentDataDir()
                 : "memory") : "memory");
        if (tmp == 0) {
            tmp = ibis::index::create(this,
                                      thePart ? thePart->currentDataDir() : 0,
                                      iopt, ropt);
        }
        if (thePart != 0 && tmp != 0 && tmp->getNRows()
#if defined(FASTBIT_REBUILD_INDEX_ON_SIZE_MISMATCH)
            !=
#else
            >
#endif
            thePart->nRows()) {
            LOGGER(ibis::gVerbose > 2)
                << evt << " an index with nRows=" << tmp->getNRows()
                << ", but the data partition nRows=" << thePart->nRows()
                << ", try to recreate the index";
            delete tmp;
            // create a brand new index from data in the current working
            // directory
            tmp = ibis::index::create(this, static_cast<const char*>(0), iopt);
            if (tmp != 0 && tmp->getNRows() != thePart->nRows()) {
                LOGGER(ibis::gVerbose > 0)
                    << "Warning -- " << evt
                    <<" created an index with nRows=" << tmp->getNRows()
                    << ", but the data partition nRows=" << thePart->nRows()
                    << ", failed on retry!";
                delete tmp;
                tmp = 0;
            }
        }
        if (tmp != 0) {
            if (ibis::gVerbose > 10) {
                ibis::util::logger lg;
                tmp->print(lg());
            }

            ibis::util::mutexLock lck2(&mutex, "loadIndex");
            if (! (lower <= upper)) { // use negation to catch NaNs
                const_cast<ibis::column*>(this)->lower = tmp->getMin();
                const_cast<ibis::column*>(this)->upper = tmp->getMax();
            }
            if (idx == 0) {
                idx = tmp;
            }
            else if (idx != tmp) { // another thread has created an index
                LOGGER(ibis::gVerbose >= 0)
                    << evt << " found an index (" << idx->name()
                    << ") for this column after building another one ("
                    << tmp->name() << "), discarding the new one";
                delete tmp;
            }
            return;
        }
    }
    catch (const char* s) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " received the following exception\n"
            << s;
        delete tmp;
    }
    catch (const std::exception& e) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " received the following exception\n"
            << e.what();
        delete tmp;
    }
    catch (...) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " received a unexpected exception";
        delete tmp;
    }

    // final error handling -- remove left over files
    if (thePart != 0) {
        purgeIndexFile();
        std::string key = thePart->name();
        key += '.';
        key += m_name;
        key += ".retryIndexOnFailure";
        if (! ibis::gParameters().isTrue(key.c_str())) {
            // don't try to build index any more
            const_cast<column*>(this)->m_bins = "noindex";
            thePart->updateMetaData();
        }
    }
} // ibis::column::loadIndex

/// Unload the index associated with the column.
/// This function requires a write lock just like loadIndex.  However, it
/// will simply return to the caller if it fails to acquire the lock.
void ibis::column::unloadIndex() const {
    if (0 == idx) return;

    softWriteLock lock(this, "unloadIndex");
    if (lock.isLocked() && 0 != idx) {
        const uint32_t idxc = idxcnt();
        if (0 == idxc) {
            delete idx;
            idx = 0;
            LOGGER(ibis::gVerbose > 4)
                << "column[" << fullname()
                << "]::unloadIndex successfully removed the index";
        }
        else {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- column[" << fullname() 
                << "]::unloadIndex failed because idxcnt ("
                << idxc << ") is not zero";
        }
    }
} // ibis::column::unloadIndex

void ibis::column::preferredBounds(std::vector<double>& tmp) const {
    indexLock lock(this, "preferredBounds");
    if (idx != 0) {
        idx->binBoundaries(tmp);
        if (tmp.back() == DBL_MAX) // remove the last element
            tmp.resize(tmp.size()-1);
    }
    else {
        tmp.clear();
    }
} // ibis::column::preferredBounds

void ibis::column::binWeights(std::vector<uint32_t>& tmp) const {
    indexLock lock(this, "binWeights");
    if (idx != 0) {
        idx->binWeights(tmp);
    }
    else {
        tmp.clear();
    }
} // ibis::column::binWeights

/// Compute the index size (in bytes).  Return a negative value if the
/// index is not in memory and the index file does not exist.
long ibis::column::indexSize() const {
    if (idx != 0) { // index in memory
        return idx->sizeInBytes();
    }
    else { // use the file size
        std::string sname;
        if (dataFileName(sname) == 0) return -1;

        sname += ".idx";
        readLock lock(this, "indexSize");
        return ibis::util::getFileSize(sname.c_str());
    }
} // ibis::column::indexSize

/// Compute the number of rows captured by the index of this column.  This
/// function loads the metadata about the index into memory through
/// ibis::column::indexLock.
uint32_t ibis::column::indexedRows() const {
    indexLock lock(this, "indexedRows");
    if (idx != 0) {
        return idx->getNRows();
    }
    else {
        return 0;
    }
} // ibis::column::indexedRows

/// Perform a set of built-in tests to determine the speed of common
/// operations.
void ibis::column::indexSpeedTest() const {
    indexLock lock(this, "indexSpeedTest");
    if (idx != 0) {
        ibis::util::logger lg;
        idx->speedTest(lg());
    }
} // ibis::column::indexSpeedTest

/// Purge the index files assocated with the current column.
void ibis::column::purgeIndexFile(const char *dir) const {
    if (dir == 0 && (thePart == 0 || thePart->currentDataDir() == 0))
        return;
    delete idx;
    idx = 0;

    std::string fnm = (dir ? dir :
                       thePart != 0 ? thePart->currentDataDir() : ".");
    if (fnm[fnm.size()-1] != FASTBIT_DIRSEP)
        fnm += FASTBIT_DIRSEP;
    fnm += m_name;
    const unsigned len = fnm.size() + 1;
    fnm += ".idx";
    ibis::fileManager::instance().flushFile(fnm.c_str());
    remove(fnm.c_str());
    fnm.erase(len);
    fnm += "bin";
    ibis::fileManager::instance().flushFile(fnm.c_str());
    remove(fnm.c_str());
    if (m_type == ibis::TEXT) {
        fnm.erase(len);
        fnm += "terms";
        ibis::fileManager::instance().flushFile(fnm.c_str());
        remove(fnm.c_str());
        fnm.erase(len);
    }
#ifdef IBIS_PURGE_CAT_INDEX
    else if (m_type == ibis::CATEGORY) {
        fnm.erase(fnm.size() - 3);
        fnm += "dic";
        ibis::fileManager::instance().flushFile(fnm.c_str());
        remove(fnm.c_str());
        fnm.erase(fnm.size() - 3);
        fnm += "int";
        ibis::fileManager::instance().flushFile(fnm.c_str());
        remove(fnm.c_str());
    }
#endif
} // ibis::column::purgeIndexFile

// expand range condition so that the boundaris are on the bin boundaries
int ibis::column::expandRange(ibis::qContinuousRange& rng) const {
    int ret = 0;
    indexLock lock(this, "expandRange");
    if (idx != 0) {
        ret = idx->expandRange(rng);
    }
    return ret;
} // ibis::column::expandRange

// expand range condition so that the boundaris are on the bin boundaries
int ibis::column::contractRange(ibis::qContinuousRange& rng) const {
    int ret = 0;
    indexLock lock(this, "contractRange");
    if (idx != 0) {
        ret = idx->contractRange(rng);
    }
    return ret;
} // ibis::column::contractRange

/// Compute the exact answer.  Attempts to use the index if one is
/// available, otherwise use the base data.
///
/// Return a negative value to indicate error, 0 to indicate no hit, and
/// positive value to indicate there are zero or more hits.
long ibis::column::evaluateRange(const ibis::qContinuousRange& cmp,
                                 const ibis::bitvector& mask,
                                 ibis::bitvector& low) const {
    long ierr = 0;
    low.clear(); // clear the existing content
    if (thePart == 0)
        return -9;

    std::string evt = "column[";
    evt += fullname();
    evt += "]::evaluateRange";
    if (ibis::gVerbose > 0) {
        std::ostringstream oss;
        oss << '(' << cmp;
        if (ibis::gVerbose > 3)
            oss << ", mask(" << mask.cnt() << ", " << mask.size() << ')';
        oss << ')';
        evt += oss.str();
    }

    if (cmp.leftOperator() == ibis::qExpr::OP_UNDEFINED &&
        cmp.rightOperator() == ibis::qExpr::OP_UNDEFINED) {
        getNullMask(low);
        low &= mask;
        return low.sloppyCount();
    }

    if (m_type == ibis::OID || m_type == ibis::TEXT) {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- " << evt
            << " -- the range condition is not applicable on the column type "
            << ibis::TYPESTRING[(int)m_type];
        ierr = -4;
        return ierr;
    }
    if (! cmp.overlap(lower, upper)) {
        low.set(0, mask.size());
        return 0;
    }

    ibis::util::timer mytimer(evt.c_str(), 4);
    try {
        ibis::bitvector high;
        { // use a block to limit the scope of index lock
            indexLock lock(this, evt.c_str());
            if (idx != 0) {
                if (dataflag == 0) {
                    std::string dfname;
                    const char *str = dataFileName(dfname);
                    if (str == 0) {
                        dataflag = (hasRawData() ? 1 : -1);
                    }
                    else {
                        off_t fs = ibis::util::getFileSize(str);
                        if (fs < 0) {
                            dataflag = -1;
                        }
                        else if (nRows() * elementSize() == (off_t)fs) {
                            dataflag = 1;
                        }
                        else {
                            dataflag = -1;
                        }
                    }
                }
                if (dataflag < 0) {
                    idx->estimate(cmp, low, high);
                }
                else {
                    // Using the index only if the cost of using the index
                    // (icost) is less than the cost of using the sequential
                    // scan (scost).  Both costs are estimated based on the
                    // expected number of bytes to be accessed.
                    const double icost = idx->estimateCost(cmp);
                    const double scost = ibis::fileManager::pageSize() *
                        ibis::part::countPages(mask, elementSize()) +
                        8.0 * mask.size() / ibis::fileManager::pageSize();
                    LOGGER(ibis::gVerbose > 2)
                        << evt << " -- estimated cost with index = "
                        << icost << ", with sequential scan = " << scost;
                    if (icost < scost) {
                        idx->estimate(cmp, low, high);
                    }
                }
            }
            else if (m_sorted && dataflag >= 0) {
                ierr = searchSorted(cmp, low);
                if (ierr < 0)
                    low.clear();
            }
        }
        if (low.size() != mask.size() && m_sorted && dataflag >= 0) {
            ierr = searchSorted(cmp, low);
            if (ierr < 0)
                low.clear();
        }
        if (low.size() != mask.size()) { // short index
            if (high.size() != low.size())
                high.copy(low);
            high.adjustSize(mask.size(), mask.size());
            low.adjustSize(0, mask.size());
        }
        low &= mask;
        if (low.size() == high.size()) { // computed high
            ibis::bitvector b2;
            high &= mask;
            high -= low;
            if (high.sloppyCount() > 0) { // need scan
                ierr = thePart->doScan(cmp, high, b2);
                if (ierr >= 0) {
                    low |= b2;
                    ierr = low.sloppyCount();
                }
                else {
                    low.clear();
                }
            }
            else {
                ierr = low.sloppyCount();
            }
        }
        else if (ierr >= 0) {
            ierr = low.sloppyCount();
        }

        LOGGER(ibis::gVerbose > 3)
            << evt << " completed with ierr = " << ierr;
        LOGGER(ibis::gVerbose > 8)
            << evt << " result --\n" << low;
        return ierr;
    }
    catch (std::exception &se) {
        LOGGER(ibis::gVerbose > 0)
            << evt << " received a std::exception -- " << se.what();
    }
    catch (const char* str) {
        LOGGER(ibis::gVerbose > 0)
            << evt << " received a string exception -- " << str;
    }
    catch (...) {
        LOGGER(ibis::gVerbose > 0)
            << evt << " received a unanticipated excetpion";
    }

    // Common exception handling -- retry the basic options
    low.clear();
    unloadIndex();
    if (ibis::fileManager::iBeat() % 3 == 0) { // some delay
        ibis::util::quietLock lock(&ibis::util::envLock);
#if defined(__unix__) || defined(__linux__) || defined(__CYGWIN__) || defined(__APPLE__) || defined(__FreeBSD)
        sleep(1);
#endif
    }
    thePart->emptyCache();
    if (m_sorted) {
        ierr = searchSorted(cmp, low);
    }
    else {
        indexLock lock(this, evt.c_str());
        if (idx != 0) {
            ierr = idx->evaluate(cmp, low);
            if (low.size() < mask.size()) {
                ibis::bitvector high, delta;
                high.adjustSize(low.size(), mask.size());
                high.flip();
                ierr = thePart->doScan(cmp, high, delta);
                low |= delta;
            }
            low &= mask;
        }
        else {
            ierr = thePart->doScan(cmp, mask, low);
        }
    }

    LOGGER(ibis::gVerbose > 3)
        << evt << " completed the fallback option with ierr = " << ierr;
    return ierr;
} // ibis::column::evaluateRange

/// Evaluate a range condition and retrieve the selected values.  This is a
/// combination of evaluateRange and selectTypes.  This combination allows
/// some optimizations to reduce the I/O operations.
///
/// Note the fourth argument vals must be valid pointer to the correct
/// type.  The acceptable types are as follows (same as required by
/// in-memory data partitions):
/// - if the column type has a fixed size such as integers and
///   floating-point values, vals must be a pointer to an ibis::array_t
///   with the matching integer type or floating-point type
/// - if the column type is one of the string values, such as TEXT or
///   CATEGORY, vals must be a pointer to std::vector<std::string>.
///
/// If vals is a nil pointer, this function simply calls evaluateRange.
long ibis::column::evaluateAndSelect(const ibis::qContinuousRange& cmp,
                                     const ibis::bitvector& mask,
                                     void* vals, ibis::bitvector& low) const {
    if (vals == 0)
        return evaluateRange(cmp, mask, low);
    if (thePart == 0)
        return -9;

    std::string evt = "column[";
    evt += fullname();
    evt += "]::evaluateAndSelect";
    if (ibis::gVerbose > 0) {
        std::ostringstream oss;
        oss << '(' << cmp;
        if (ibis::gVerbose > 3)
            oss << ", mask(" << mask.cnt() << ", " << mask.size() << ')';
        oss << ')';
        evt += oss.str();
    }

    long ierr = 0;
    low.clear(); // clear the existing content
    if (m_type == ibis::OID || m_type == ibis::TEXT) {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- " << evt
            << " -- the range condition is not applicable on the column type "
            << ibis::TYPESTRING[(int)m_type];
        ierr = -4;
        return ierr;
    }
    if (! cmp.overlap(lower, upper)) {
        low.set(0, mask.size());
        return 0;
    }

    try {
        if (mask.size() == mask.cnt()) { // directly use index
            indexLock lock(this, "evaluateAndSelect");
            if (idx != 0 && idx->getNRows() == thePart->nRows()) {
                const double icost = idx->estimateCost(cmp);
                const double scost = ibis::fileManager::pageSize() *
                    ibis::part::countPages(mask, elementSize());
                LOGGER(ibis::gVerbose > 2)
                    << evt << " -- estimated cost with index = "
                    << icost << ", with sequential scan = " << scost;
                if (icost < scost)
                    ierr = idx->select(cmp, vals, low);
                else
                    ierr = thePart->doScan(cmp, mask, vals, low);
            }
            else {
                ierr = thePart->doScan(cmp, mask, vals, low);
            }
        }
        if (low.size() != mask.size()) { // separate evaluate and select
            ierr = evaluateRange(cmp, mask, low);
            if (ierr > 0)
                ierr = selectValues(low, vals);
        }

        LOGGER(ibis::gVerbose > 3)
            << evt << " completed with ierr = " << ierr;
    }
    catch (std::exception &se) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " received a std::exception -- "
            << se.what();
        ierr = -3;
    }
    catch (const char* str) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " received a string exception -- "
            << str;
        ierr = -2;
    }
    catch (...) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " received a unanticipated excetpion";
        ierr = -1;
    }
    return ierr;
} // ibis::column::evaluateAndSelect

long ibis::column::evaluateRange(const ibis::qDiscreteRange& cmp,
                                 const ibis::bitvector& mask,
                                 ibis::bitvector& low) const {
    long ierr = -1;
    if (cmp.getValues().empty()) {
        low.set(0, mask.size());
        return 0;
    }
    if (thePart == 0)
        return -9;
    std::string evt = "column[";
    evt += fullname();
    evt += "]::evaluateRange";
    if (ibis::gVerbose > 0) {
        std::ostringstream oss;
        oss << '(' << cmp;
        if (ibis::gVerbose > 3)
            oss << ", mask(" << mask.cnt() << ", " << mask.size() << ')';
        oss << ')';
        evt += oss.str();
    }

    if (m_type == ibis::OID || m_type == ibis::TEXT) {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- " << evt << " not applicable on the column type "
            << ibis::TYPESTRING[(int)m_type];
        ierr = -4;
        return ierr;
    }
    if (m_type != ibis::FLOAT && m_type != ibis::DOUBLE &&
        cmp.getValues().size() ==
        1+(cmp.getValues().back()-cmp.getValues().front())) {
        bool convert = (! hasRoster()); // no roster
        if (false == convert) {
            // has roster, prefer coversion only if the index is very small
            convert = (indexSize() < (thePart->nRows() >> 2));
        }
        if (convert) {
            // a special case -- actually a continuous range
            ibis::qContinuousRange
                cr(cmp.getValues().front(), ibis::qExpr::OP_LE,
                   cmp.colName(), ibis::qExpr::OP_LE, cmp.getValues().back());
            return evaluateRange(cr, mask, low);
        }
    }
    if (! cmp.overlap(lower, upper)) {
        low.set(0, mask.size());
        return 0;
    }

    ibis::util::timer mytimer(evt.c_str(), 4);
    try {
        indexLock lock(this, evt.c_str());
        if (idx != 0) {
            const unsigned elem = elementSize();
            const double idxcost = idx->estimateCost(cmp) *
                (1.0 + log((double)cmp.nItems()));
            if (m_sorted && idxcost >= 6.0*mask.cnt()) {
                ierr = searchSorted(cmp, low);
                if (ierr >= 0) {
                    low &= mask;
                    ierr = low.sloppyCount();
                }
            }
            if (ierr < 0 && hasRoster() && idxcost >= (elem+4.0) * 
                (mask.cnt()+mask.size()/ibis::fileManager::pageSize())) {
                // using a sorted list may be faster
                ibis::roster ros(this);
                if (ros.size() == thePart->nRows()) {
                    ierr = ros.locate(cmp.getValues(), low);
                    if (ierr >= 0) {
                        low &= mask;
                        return low.sloppyCount();
                    }
                }
            }
            if (ierr < 0 && idxcost <= ibis::fileManager::pageSize() *
                ibis::part::countPages(mask, elem)) {
                // the normal indexing option
                ierr = idx->evaluate(cmp, low);
                if (ierr >= 0) {
                    if (low.size() < mask.size()) { // short index, scan
                        ibis::bitvector b1, b2;
                        b1.appendFill(0, low.size());
                        b1.appendFill(1, mask.size()-low.size());
                        ierr = thePart->doScan(cmp, b1, b2);
                        if (ierr >= 0) {
                            low.adjustSize(0, mask.size());
                            low |= b2;
                        }
                    }
                    low &= mask;
                }
            }
        }
        // fall back options
        if (ierr < 0) {
            if (m_sorted) {
                ierr = searchSorted(cmp, low);
                if (ierr >= 0) {
                    low &= mask;
                    ierr = low.sloppyCount();
                }
            }
        }
        if (ierr < 0) {
            LOGGER(ibis::gVerbose > 4)
                << "INFO -- " << evt << ": the cost of using roster ~ "
                << (thePart->nRows()+cmp.nItems())*0.15
                << ", the cost of using scan ~ "
                << (2.0+log((double)cmp.nItems()))*mask.cnt();
            if (hasRoster() &&
                (thePart->nRows()+cmp.nItems())*0.15 <
                (2.0+log((double)cmp.nItems()))*mask.cnt()) {
                ibis::roster ros(this);
                if (ros.size() == thePart->nRows()) {
                    ierr = ros.locate(cmp.getValues(), low);
                    if (ierr >= 0) {
                        low &= mask;
                        ierr = low.sloppyCount();
                    }
                }
            }
        }
        if (ierr < 0)
            ierr = thePart->doScan(cmp, mask, low);

        LOGGER(ibis::gVerbose > 3)
            << evt << " completed with ierr = " << ierr;
        return ierr;
    }
    catch (std::exception &se) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning-- " << evt << " received a std::exception -- "
            << se.what();
    }
    catch (const char* str) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " received a string exception -- "
            << str;
    }
    catch (...) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning" << evt << " received a unanticipated excetpion";
    }

    // Common exception handling -- retry the basic options
    low.clear();
    unloadIndex();
    if (thePart != 0) {
        if (ibis::fileManager::iBeat() % 3 == 0) { // some delay
            ibis::util::quietLock lock(&ibis::util::envLock);
#if defined(__unix__) || defined(__linux__) || defined(__CYGWIN__) || defined(__APPLE__) || defined(__FreeBSD)
            sleep(1);
#endif
        }
        thePart->emptyCache();
        if (m_sorted) {
            ierr = searchSorted(cmp, low);
        }
        else {
            indexLock lock(this, evt.c_str());
            if (idx != 0) {
                idx->evaluate(cmp, low);
                if (low.size() < mask.size()) {
                    ibis::bitvector high, delta;
                    high.adjustSize(low.size(), mask.size());
                    high.flip();
                    ierr = thePart->doScan(cmp, high, delta);
                    low |= delta;
                }
                low &= mask;
            }
            else {
                ierr = thePart->doScan(cmp, mask, low);
            }
        }
    }
    else {
        ierr = -3;
    }

    LOGGER(ibis::gVerbose > 3)
        << evt << " completed the fallback option with ierr = " << ierr;
    return ierr;
} // ibis::column::evaluateRange

// use the index to generate the hit list and the candidate list
long ibis::column::estimateRange(const ibis::qContinuousRange& cmp,
                                 ibis::bitvector& low,
                                 ibis::bitvector& high) const {
    long ierr = 0;
    if (thePart == 0)
        return -9;
    if (cmp.leftOperator() == ibis::qExpr::OP_UNDEFINED &&
        cmp.rightOperator() == ibis::qExpr::OP_UNDEFINED) {
        low.copy(mask_);
        high.copy(mask_);
        return 0;
    }
    if (! cmp.overlap(lower, upper)) {
        high.set(0, thePart->nRows());
        low.set(0, thePart->nRows());
        return 0;
    }

    try {
        indexLock lock(this, "estimateRange");
        if (idx != 0) {
            idx->estimate(cmp, low, high);
            if (low.size() != thePart->nRows()) {
                if (high.size() == low.size()) {
                    high.adjustSize(thePart->nRows(), thePart->nRows());
                }
                else if (high.size() == 0) {
                    high.copy(low);
                    high.adjustSize(thePart->nRows(), thePart->nRows());
                }
                low.adjustSize(0, thePart->nRows());
            }
        }
        else if (thePart != 0) {
            low.set(0, thePart->nRows());
            getNullMask(high);
        }
        else {
            ierr = -1;
        }

        LOGGER(ibis::gVerbose > 4)
            << "column[" << fullname() << "]::estimateRange(" << cmp
            << ") completed with ierr = " << ierr;
        return ierr;
    }
    catch (std::exception &se) {
        logWarning("estimateRange", "received a std::exception -- %s",
                   se.what());
    }
    catch (const char* str) {
        logWarning("estimateRange", "received a string exception -- %s",
                   str);
    }
    catch (...) {
        logWarning("estimateRange", "received a unanticipated excetpion");
    }

    // Common exception handling -- no estimate can be provided
    unloadIndex();
    //purgeIndexFile();
    if (thePart != 0) {
        low.set(0, thePart->nRows());
        getNullMask(high);
    }
    else {
        ierr = -2;
    }
    return -ierr;
} // ibis::column::estimateRange

/// Use the index of the column to compute an upper bound on the number of
/// hits.  If no index can be computed, it will return the number of rows
/// as the upper bound.
long ibis::column::estimateRange(const ibis::qContinuousRange& cmp) const {
    if (! cmp.overlap(lower, upper))
        return 0;

    long ret = (thePart != 0 ? thePart->nRows() : LONG_MAX);
    if (cmp.leftOperator() == ibis::qExpr::OP_UNDEFINED &&
        cmp.rightOperator() == ibis::qExpr::OP_UNDEFINED)
        return ret;

    try {
        indexLock lock(this, "estimateRange");
        if (idx != 0)
            ret = idx->estimate(cmp);
        else
            ret = -1;
        return ret;
    }
    catch (std::exception &se) {
        logWarning("estimateRange", "received a std::exception -- %s",
                   se.what());
    }
    catch (const char* str) {
        logWarning("estimateRange", "received a string exception -- %s",
                   str);
    }
    catch (...) {
        logWarning("estimateRange", "received a unanticipated excetpion");
    }

    unloadIndex();
    //purgeIndexFile();
    return ret;
} // ibis::column::estimateRange

/// Compute an upper bound on the number of hits.
/// Estimating hits for a discrete range is actually done with
/// evaluateRange.
long ibis::column::estimateRange(const ibis::qDiscreteRange& cmp,
                                 ibis::bitvector& low,
                                 ibis::bitvector& high) const {
    high.clear();
    return evaluateRange(cmp, thePart->getMaskRef(), low);
} // ibis::column::estimateRange

double ibis::column::estimateCost(const ibis::qContinuousRange& cmp) const {
    if (cmp.leftOperator() == ibis::qExpr::OP_UNDEFINED &&
        cmp.rightOperator() == ibis::qExpr::OP_UNDEFINED)
        return 0.0;
    if (! cmp.overlap(lower, upper))
        return 0.0;

    double ret;
    indexLock lock(this, "estimateCost");
    if (idx != 0) {
        ret = idx->estimateCost(cmp);
    }
    else {
        ret = elementSize();
        ret = static_cast<double>(thePart != 0 ? thePart->nRows() :
                                  0xFFFFFFFFU) * (ret > 0.0 ? ret : 32.0);
    }
    return ret;
} // ibis::column::estimateCost

double ibis::column::estimateCost(const ibis::qDiscreteRange& cmp) const {
    if (! cmp.overlap(lower, upper))
        return 0.0;

    double ret;
    indexLock lock(this, "estimateCost");
    if (idx != 0) {
        ret = idx->estimateCost(cmp);
    }
    else {
        ret = elementSize();
        ret = static_cast<double>(thePart != 0 ? thePart->nRows() :
                                  0xFFFFFFFFU) * (ret > 0.0 ? ret : 32.0);
        double width = 1.0 + (cmp.rightBound()<upper?cmp.rightBound():upper)
            - (cmp.leftBound()>lower?cmp.leftBound():lower);
        if (upper > lower && width >= 1.0 && width < (1.0+upper-lower)) {
            ret *= width / (upper - lower);
        }
    }
    return ret;
} // ibis::column::estimateCost

/// Compute the locations of the rows can not be decided by the index.
/// Returns the fraction of rows might satisfy the specified range
/// condition.  If no index, nothing can be decided.
float ibis::column::getUndecidable(const ibis::qContinuousRange& cmp,
                                   ibis::bitvector& iffy) const {
    if (cmp.leftOperator() == ibis::qExpr::OP_UNDEFINED &&
        cmp.rightOperator() == ibis::qExpr::OP_UNDEFINED)
        return 0.0;

    float ret = 1.0;
    try {
        indexLock lock(this, "getUndecidable");
        if (idx != 0) {
            ret = idx->undecidable(cmp, iffy);
        }
        else {
            getNullMask(iffy);
            ret = 1.0; // everything might satisfy the condition
        }
        return ret; // normal return
    }
    catch (std::exception &se) {
        logWarning("getUndecidable", "received a std::exception -- %s",
                   se.what());
        ret = 1.0; // everything is undecidable by index
    }
    catch (const char* str) {
        logWarning("getUndecidable", "received a string exception -- %s",
                   str);
        ret = 1.0;
    }
    catch (...) {
        logWarning("getUndecidable", "received a unanticipated excetpion");
        ret = 1.0;
    }

    unloadIndex();
    //purgeIndexFile();
    getNullMask(iffy);
    return ret;
} // ibis::column::getUndecidable

// use the index to compute a upper bound on the number of hits
long ibis::column::estimateRange(const ibis::qDiscreteRange& cmp) const {
    if (! cmp.overlap(lower, upper))
        return 0;

    long ret = (thePart != 0 ? thePart->nRows() : LONG_MAX);
    try {
        indexLock lock(this, "estimateRange");
        if (idx != 0)
            ret = idx->estimate(cmp);
        return ret;
    }
    catch (std::exception &se) {
        logWarning("estimateRange", "received a std::exception -- %s",
                   se.what());
    }
    catch (const char* str) {
        logWarning("estimateRange", "received a string exception -- %s",
                   str);
    }
    catch (...) {
        logWarning("estimateRange", "received a unanticipated excetpion");
    }

    unloadIndex();
    //purgeIndexFile();
    return ret;
} // ibis::column::estimateRange

// compute the rows that can not be decided by the index, if no index,
// nothing can be decided.
float ibis::column::getUndecidable(const ibis::qDiscreteRange& cmp,
                                   ibis::bitvector& iffy) const {
    float ret = 1.0;
    try {
        indexLock lock(this, "getUndecidable");
        if (idx != 0) {
            ret = idx->undecidable(cmp, iffy);
        }
        else {
            getNullMask(iffy);
            ret = 1.0; // everything might satisfy the condition
        }
        return ret; // normal return
    }
    catch (std::exception &se) {
        logWarning("getUndecidable", "received a std::exception -- %s",
                   se.what());
        ret = 1.0; // everything is undecidable by index
    }
    catch (const char* str) {
        logWarning("getUndecidable", "received a string exception -- %s",
                   str);
        ret = 1.0;
    }
    catch (...) {
        logWarning("getUndecidable", "received a unanticipated excetpion");
        ret = 1.0;
    }

    unloadIndex();
    //purgeIndexFile();
    getNullMask(iffy);
    return ret;
} // ibis::column::getUndecidable

long ibis::column::evaluateRange(const ibis::qIntHod& cmp,
                                 const ibis::bitvector& mask,
                                 ibis::bitvector& low) const {
    long ierr = -1;
    if (cmp.getValues().empty()) {
        low.set(0, mask.size());
        return 0;
    }
    if (thePart == 0)
        return -9;
    std::string evt = "column[";
    evt += fullname();
    evt += "]::evaluateRange";
    if (ibis::gVerbose > 0) {
        std::ostringstream oss;
        oss << '(' << cmp;
        if (ibis::gVerbose > 3)
            oss << ", mask(" << mask.cnt() << ", " << mask.size() << ')';
        oss << ')';
        evt += oss.str();
    }
    if (m_type == ibis::OID || m_type == ibis::TEXT) {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- " << evt
            << " -- condition is not applicable on the column type "
            << ibis::TYPESTRING[(int)m_type];
        ierr = -4;
        return ierr;
    }

    ibis::util::timer mytimer(evt.c_str(), 4);
    try {
        ierr = -1;
        if (m_sorted) {
            ierr = searchSorted(cmp, low);
            if (ierr > 0) {
                low &= mask;
                ierr = low.sloppyCount();
            }
        }
        else if (hasRoster() &&
                 (thePart->nRows()+cmp.nItems())*0.15 <
                 (2.0+log((double)cmp.nItems()))*mask.cnt()) {
            // use a sorted list
            ibis::roster ros(this);
            if (ros.size() == thePart->nRows()) {
                ierr = ros.locate(cmp.getValues(), low);
                if (ierr > 0) {
                    low &= mask;
                    ierr = low.sloppyCount();
                }
            }
        }
        if (ierr < 0) {
            ierr = thePart->doScan(cmp, mask, low);
        }

        LOGGER(ibis::gVerbose > 3)
            << evt << " completed with ierr = " << ierr;
        return ierr;
    }
    catch (std::exception &se) {
        logWarning("evaluateRange", "received a std::exception -- %s",
                   se.what());
    }
    catch (const char* str) {
        logWarning("evaluateRange", "received a string exception -- %s",
                   str);
    }
    catch (...) {
        logWarning("evaluateRange", "received a unanticipated excetpion");
    }

    // Common exception handling
    low.clear();
    unloadIndex();
    ierr = -3;
    return ierr;
} // ibis::column::evaluateRange

/// Estimating hits for a discrete range.  Does nothing useful in this
/// implementation.
long ibis::column::estimateRange(const ibis::qIntHod& cmp,
                                 ibis::bitvector& low,
                                 ibis::bitvector& high) const {
    if (thePart != 0) {
        low.set(0, thePart->nRows());
        thePart->getNullMask(high);
    }
    return high.sloppyCount();
} // ibis::column::estimateRange

double ibis::column::estimateCost(const ibis::qIntHod& cmp) const {
    double ret = elementSize();
    ret = static_cast<double>(thePart != 0 ? thePart->nRows() :
                              0xFFFFFFFFU) * (ret > 0.0 ? ret : 32.0);
    return ret;
} // ibis::column::estimateCost

/// A dummy function to estimate the number of possible hits.  It always
/// returns the number of rows in the data partition.
long ibis::column::estimateRange(const ibis::qIntHod& cmp) const {
    long ret = (thePart != 0 ? thePart->nRows() : LONG_MAX);
    return ret;
} // ibis::column::estimateRange

/// A dummy implementation.  It always return 1.0 to indicate everything
/// rows is undecidable.
float ibis::column::getUndecidable(const ibis::qIntHod& cmp,
                                   ibis::bitvector& iffy) const {
    float ret = 1.0;
    return ret; // normal return
} // ibis::column::getUndecidable

long ibis::column::evaluateRange(const ibis::qUIntHod& cmp,
                                 const ibis::bitvector& mask,
                                 ibis::bitvector& low) const {
    long ierr = -1;
    if (cmp.getValues().empty()) {
        low.set(0, mask.size());
        return 0;
    }
    if (thePart == 0)
        return -9;

    std::string evt = "column[";
    evt += fullname();
    evt += "]::evaluateRange";
    if (ibis::gVerbose > 0) {
        std::ostringstream oss;
        oss << '(' << cmp;
        if (ibis::gVerbose > 3)
            oss << ", mask(" << mask.cnt() << ", " << mask.size() << ')';
        oss << ')';
        evt += oss.str();
    }
    if (m_type == ibis::OID || m_type == ibis::TEXT) {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- " << evt
            << " -- condition is not applicable on the column type "
            << ibis::TYPESTRING[(int)m_type];
        ierr = -4;
        return ierr;
    }

    ibis::util::timer mytimer(evt.c_str(), 4);
    try {
        ierr = -1;
        if (m_sorted) {
            ierr = searchSorted(cmp, low);
            if (ierr > 0) {
                low &= mask;
                ierr = low.sloppyCount();
            }
        }
        else if (hasRoster() &&
                 (thePart->nRows()+cmp.nItems())*0.15 <
                 (2.0+log((double)cmp.nItems()))*mask.cnt()) {
            // use a sorted list
            ibis::roster ros(this);
            if (ros.size() == thePart->nRows()) {
                ierr = ros.locate(cmp.getValues(), low);
                if (ierr > 0) {
                    low &= mask;
                    ierr = low.sloppyCount();
                }
            }
        }
        if (ierr < 0) {
            ierr = thePart->doScan(cmp, mask, low);
        }

        LOGGER(ibis::gVerbose > 3)
            << evt << " completed with ierr = " << ierr;
        return ierr;
    }
    catch (std::exception &se) {
        logWarning("evaluateRange", "received a std::exception -- %s",
                   se.what());
    }
    catch (const char* str) {
        logWarning("evaluateRange", "received a string exception -- %s",
                   str);
    }
    catch (...) {
        logWarning("evaluateRange", "received a unanticipated excetpion");
    }

    // Common exception handling
    low.clear();
    unloadIndex();
    ierr = -3;
    return ierr;
} // ibis::column::evaluateRange

/// Estimating hits for a discrete range.   Does nothing in this implementation.
long ibis::column::estimateRange(const ibis::qUIntHod& cmp,
                                 ibis::bitvector& low,
                                 ibis::bitvector& high) const {
    if (thePart != 0) {
        low.set(0, thePart->nRows());
        thePart->getNullMask(high);
    }
    return high.sloppyCount();
} // ibis::column::estimateRange

double ibis::column::estimateCost(const ibis::qUIntHod& cmp) const {
    double ret = elementSize();
    ret = static_cast<double>(thePart != 0 ? thePart->nRows() :
                              0xFFFFFFFFU) * (ret > 0.0 ? ret : 32.0);
    return ret;
} // ibis::column::estimateCost

/// A dummy function to estimate the number of possible hits.  It always
/// returns the number of rows in the data partition.
long ibis::column::estimateRange(const ibis::qUIntHod& cmp) const {
    long ret = (thePart != 0 ? thePart->nRows() : LONG_MAX);
    return ret;
} // ibis::column::estimateRange

/// A dummy implementation.  It always return 1.0 to indicate everything
/// rows is undecidable.
float ibis::column::getUndecidable(const ibis::qUIntHod& cmp,
                                   ibis::bitvector& iffy) const {
    float ret = 1.0;
    return ret; // normal return
} // ibis::column::getUndecidable

long ibis::column::stringSearch(const char*, ibis::bitvector&) const {
    LOGGER(ibis::gVerbose > 0)
        << "Warning -- column[" << (thePart ? thePart->name() : "") << '.'
        << m_name << "]::stringSearch is not supported on column type "
        << ibis::TYPESTRING[(int)m_type];
    return -1;
}

long ibis::column::stringSearch(const char*) const {
    return (thePart ? (long)thePart->nRows() : INT_MAX);
}

long ibis::column::stringSearch(const std::vector<std::string>&,
                                ibis::bitvector&) const {
    LOGGER(ibis::gVerbose > 0)
        << "Warning -- column[" << (thePart ? thePart->name() : "") << '.'
        << m_name << "]::stringSearch is not supported on column type "
        << ibis::TYPESTRING[(int)m_type];
    return -1;
}

long ibis::column::stringSearch(const std::vector<std::string>&) const {
    return (thePart ? (long)thePart->nRows() : INT_MAX);
}

long ibis::column::keywordSearch(const char*, ibis::bitvector&) const {
    LOGGER(ibis::gVerbose > 0)
        << "Warning -- column[" << (thePart ? thePart->name() : "") << '.'
        << m_name << "]::keywordSearch is not supported by the plain old "
        "column class";
    return -1;
}

long ibis::column::keywordSearch(const char*) const {
    return (thePart ? (long)thePart->nRows() : INT_MAX);
}

long ibis::column::keywordSearch(const std::vector<std::string>&,
                                 ibis::bitvector&) const {
    LOGGER(ibis::gVerbose > 0)
        << "Warning -- column[" << (thePart ? thePart->name() : "") << '.'
        << m_name << "]::keywordSearch is not supported on column type "
        << ibis::TYPESTRING[(int)m_type];
    return -1;
}

long ibis::column::keywordSearch(const std::vector<std::string>&) const {
    return (thePart ? (long)thePart->nRows() : INT_MAX);
}

long ibis::column::patternSearch(const char*) const {
    return (thePart ? (long)thePart->nRows() : INT_MAX);
}

long ibis::column::patternSearch(const char*, ibis::bitvector &) const {
    LOGGER(ibis::gVerbose > 0)
        << "Warning -- column[" << (thePart ? thePart->name() : "") << '.'
        << m_name << "]::patternSearch is not supported by the plain old "
        "column class";
    return -1;
}

/// Append the content of file in @c df to end of file in @c dt.  It
/// returns the number of rows appended or a negative number to indicate
/// error.
///
/// @note The directories @c dt and @c df can not be same.
///
/// @note This function does not update the mininimum and the maximum of
/// the column.
long ibis::column::append(const char* dt, const char* df,
                          const uint32_t nold, const uint32_t nnew,
                          uint32_t nbuf, char* buf) {
    if (nnew == 0 || dt == 0 || df == 0 || *dt == 0 || *df == 0 ||
        df == dt || std::strcmp(dt, df) == 0)
        return 0;
    std::string evt = "column[";
    evt += fullname();
    evt += "]::append";
    int elem = elementSize();
    if (elem <= 0) {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- " << evt << " can not continue because "
            "elementSize() is not a positive number";
        return -1;
    }
    else if (static_cast<uint64_t>((nold+nnew))*elem >= 0x80000000LU) {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- " << evt
            << " -- the new data file will have more than 2GB, nold="
            << nold << ", nnew=" << nnew << ", elementSize()=" << elem;
        return -2;
    }

    long ierr;
    writeLock lock(this, evt.c_str());
    std::string to;
    std::string from;
    to += dt;
    to += FASTBIT_DIRSEP;
    to += m_name;
    from += df;
    from += FASTBIT_DIRSEP;
    from += m_name;
    LOGGER(ibis::gVerbose > 3)
        << evt << " -- source \"" << from << "\" --> destination \""
        << to << "\", nold=" << nold << ", nnew=" << nnew;

    // open destination file, position the file pointer
    int dest = UnixOpen(to.c_str(), OPEN_WRITEADD, OPEN_FILEMODE);
    if (dest < 0) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt <<  " failed to open file \"" << to
            << "\" for append ... "
            << (errno ? strerror(errno) : "no free stdio stream");
        return -3;
    }
#if defined(_WIN32) && defined(_MSC_VER)
    (void)_setmode(dest, _O_BINARY);
#endif
    IBIS_BLOCK_GUARD(UnixClose, dest);
    size_t j = UnixSeek(dest, 0, SEEK_END);
    size_t sz = elem*nold, nnew0 = 0;
    uint32_t nold0 = j / elem;
    if (nold > nold0) { // existing destination smaller than expected
        memset(buf, 0, nbuf);
        while (j < sz) {
            uint32_t diff = sz - j;
            if (diff > nbuf)
                diff = nbuf;
            ierr = ibis::util::write(dest, buf, diff);
            j += diff;
        }
    }
    long ret = UnixSeek(dest, sz, SEEK_SET);
    if (ret < static_cast<long>(sz)) {
        // can not move file pointer to the expected location
        LOGGER(ibis::gVerbose > 0)
            << "Warning" << evt << " failed to seek to " << sz << " in " << to
            << ", seek returned " << ret;
        return -4;
    }

    ret = 0;    // to count the number of bytes written
    int src = UnixOpen(from.c_str(), OPEN_READONLY); // open the files
    if (src >= 0) { // open the source file, copy it
#if defined(_WIN32) && defined(_MSC_VER)
        (void)_setmode(src, _O_BINARY);
#endif
        IBIS_BLOCK_GUARD(UnixClose, src);
        const uint32_t tgt = nnew * elem;
        long iread=1, iwrite;
        while (static_cast<uint32_t>(ret) < tgt &&
               (iread = ibis::util::read(src, buf, nbuf)) > 0) {
            if (iread + ret > static_cast<long>(tgt)) {
                // write at most tgt bytes
                LOGGER(ibis::gVerbose > 1)
                    << evt << " -- read " << iread << " bytes from " << from
                    << ", but expected " << (tgt-ret) << ", will use first "
                    << (tgt-ret) << " bytes";
                iread = tgt - ret;
            }
            iwrite = ibis::util::write(dest, buf, iread);
            if (iwrite != iread) {
                logWarning("append", "Only wrote %ld out of %ld bytes to "
                           "\"%s\" after written %ld elements",
                           static_cast<long>(iwrite), static_cast<long>(iread),
                           to.c_str(), ret);
            }
            ret += (iwrite>0 ? iwrite : 0);
        }

        m_sorted = false; // assume no longer sorted
        LOGGER(ibis::gVerbose > 8)
            << evt << " -- copied " << ret << " bytes from \"" << from
            << "\" to \"" << to << "\"";
    }
    else if (ibis::gVerbose > 0) { // can not open source file, write 0
        logWarning("append", "failed to open file \"%s\" for reading ... "
                   "%s\nwill write zeros in its place",
                   from.c_str(),
                   (errno ? strerror(errno) : "no free stdio stream"));
    }
    j = UnixSeek(dest, 0, SEEK_CUR);
    sz = elem * (nold + nnew);
    nnew0 = (j / elem) - nold;
    if (j < sz) {
        memset(buf, 0, nbuf);
        while (j < sz) {
            uint32_t diff = sz - j;
            if (diff > nbuf)
                diff = nbuf;
            ierr = ibis::util::write(dest, buf, diff);
            j += diff;
        }
    }
#if defined(FASTBIT_SYNC_WRITE)
#if _POSIX_FSYNC+0 > 0
    (void) UnixFlush(dest); // write to disk
#elif defined(_WIN32) && defined(_MSC_VER)
    (void) _commit(dest);
#endif
#endif
    if (j != sz) {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- " << evt << " file \"" << to << "\" size (" << j
            << ") differs from the expected value " << sz;
        if (j > sz) //truncate the file to the expected size
            ierr = truncate(to.c_str(), sz);
    }
    else if (ibis::gVerbose > 10) {
        logMessage("append", "size of \"%s\" is %lu as expected", to.c_str(),
                   static_cast<long unsigned>(j));
    }

    ret /= elem;        // convert to the number of elements written
    LOGGER(ibis::gVerbose > 4)
        << evt << " appended " << ret << " row" << (ret>1?"s":"");
    if (m_type == ibis::OID) {
        return ret;
    }

    //////////////////////////////////////////////////
    // deals with the masks
    std::string filename;
    filename = from;
    filename += ".msk";
    ibis::bitvector mapp;
    try {mapp.read(filename.c_str());} catch (...) {/* ok to continue */}
    mapp.adjustSize(nnew0, nnew);
    LOGGER(ibis::gVerbose > 7)
        << evt << " mask file \"" << filename << "\" contains "
        << mapp.cnt() << " set bits out of " << mapp.size()
        << " total bits";

    filename = to;
    filename += ".msk";
    ibis::bitvector mtot;
    try {mtot.read(filename.c_str());} catch (...) {/* ok to continue */}
    mtot.adjustSize(nold0, nold);
    LOGGER(ibis::gVerbose > 7)
        << evt << " mask file \"" << filename << "\" contains " << mtot.cnt()
        << " set bits out of " << mtot.size() << " total bits before append";

    mtot += mapp; // append the new ones at the end
    if (mtot.size() != nold+nnew) {
        if (ibis::gVerbose > 0)
            logWarning("append", "combined mask (%lu-bits) is expected to "
                       "have %lu bits, but it is not.  Will force it to "
                       "the expected size",
                       static_cast<long unsigned>(mtot.size()),
                       static_cast<long unsigned>(nold+nnew));
        mtot.adjustSize(nold+nnew, nold+nnew);
    }
    if (mtot.cnt() != mtot.size()) {
        mtot.write(filename.c_str());
        if (ibis::gVerbose > 6) {
            logMessage("append", "mask file \"%s\" indicates %lu valid "
                       "records out of %lu", filename.c_str(),
                       static_cast<long unsigned>(mtot.cnt()),
                       static_cast<long unsigned>(mtot.size()));
#if DEBUG+0 > 0 || _DEBUG+0 > 0
            LOGGER(ibis::gVerbose > 0) << mtot;
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
        return ret;
    if (std::strcmp(dt, thePart->currentDataDir()) == 0) {
        // update the mask stored internally
        ibis::util::mutexLock lck(&mutex, "column::append");
        mask_.swap(mtot);
    }

    //////////////////////////////////////////////////
    // deal with the index
    ibis::index* ind = 0;
    j = filename.size()-1;
    filename[j] = 'x'; // msk --> idx
    -- j;
    filename[j] = 'd';
    -- j;
    filename[j] = 'i';
    j = ibis::util::getFileSize(filename.c_str());
    if (thePart->getState() == ibis::part::TRANSITION_STATE) {
        if (thePart->currentDataDir() != 0) {
            // the active directory may have up to date indices
            std::string ff = thePart->currentDataDir();
            ff += FASTBIT_DIRSEP;
            ff += m_name;
            ff += ".idx";
            Stat_T st;
            if (UnixStat(ff.c_str(), &st) == 0) {
                if (st.st_atime >= thePart->timestamp()) {
                    // copy the fresh index file
                    ibis::util::copy(filename.c_str(), ff.c_str());
                    if (ibis::gVerbose > 6)
                        logMessage("append",
                                   "copied index file \"%s\" to \"%s\"",
                                   ff.c_str(), filename.c_str());
                }
                else if (j > 0) { // remove the stale file
                    remove(filename.c_str());
                }
            }
            else if (j > 0) { // remove the stale index file
                remove(filename.c_str());
            }
        }
    }
    else if (thePart->nRows() > 0) {
        if (j > 0) { // the idx file exists
            ind = ibis::index::create(this, dt);
            if (ind && ind->getNRows() == nold) {
                // existing file maps successfully into an index
                ierr = ind->append(dt, df, nnew);
                // the append operation have forced the index into memory,
                // remove record of the old index file
                ibis::fileManager::instance().flushFile(filename.c_str());
                if (static_cast<uint32_t>(ierr) == nnew) { // success
                    ind->write(dt);     // record the updated index
                    if (ibis::gVerbose > 6)
                        logMessage("append", "successfully extended the "
                                   "index in %s", dt);
                    if (ibis::gVerbose > 8) {
                        ibis::util::logger lg;
                        ind->print(lg());
                    }
                    delete ind;
                }
                else {                  // failed to append
                    delete ind;
                    remove(filename.c_str());
                    if (ibis::gVerbose > 4)
                        logMessage("append", "failed to extend the index "
                                   "(code: %ld), removing file \"%s\"",
                                   ierr, filename.c_str());
                }
            }
#ifdef APPEND_UPDATE_INDEXES
            else { // directly create the new indices
                ind = ibis::index::create(this, dt);
                if (ind != 0 && ibis::gVerbose > 6)
                    logMessage("append", "successfully created the "
                               "index in %s", dt);
                if (ibis::gVerbose > 8) {
                    ibis::util::logger lg;
                    ind->print(lg());
                }
                delete ind;
                ind = ibis::index::create(this, df);
                if (ind != 0 && ibis::gVerbose > 6)
                    logMessage("append", "successfully created the "
                               "index in %s", df);
                delete ind;
            }
#else
            else { // clean up the stale index
                delete ind;
                ibis::fileManager::instance().flushFile(filename.c_str());
                // simply remove the existing index file
                remove(filename.c_str());
            }
#endif
        }
#ifdef APPEND_UPDATE_INDEXES
        else { // directly create the indices
            ind = ibis::index::create(this, dt);
            if (ind != 0 && ibis::gVerbose > 6)
                logMessage("append", "successfully created the "
                           "index in %s", dt);
            if (ibis::gVerbose > 8) {
                ibis::util::logger lg;
                ind->print(lg());
            }
            delete ind;
            ind = ibis::index::create(this, df);
            if (ind != 0 && ibis::gVerbose > 6)
                logMessage("append", "successfully created the "
                           "index in %s", df);
            delete ind;
        }
#endif
    }
#ifdef APPEND_UPDATE_INDEXES
    else { // dt and df contains the same data
        ind = ibis::index::create(this, dt);
        if (ind) {
            if (ibis::gVerbose > 6)
                logMessage("append", "successfully created the "
                           "index in %s (also wrote to %s)", dt, df);
            ind->write(df);
            if (ibis::gVerbose > 8) {
                ibis::util::logger lg;
                ind->print(lg());
            }
            delete ind;
        }
    }
#endif
    return ret;
} // ibis::column::append

/// Convert string values in the opened file to a list of integers with the
/// aid of a dictionary.
/// - return 0 if there is no more elements in file.
/// - return a positive value if more bytes remain in the file.
/// - return a negative value if an error is encountered during the read
///   operation.
long ibis::column::string2int(int fptr, dictionary& dic,
                              uint32_t nbuf, char* buf,
                              array_t<uint32_t>& out) const {
    out.clear(); // clear the current integer list
    long ierr = 1;
    int64_t nread = ibis::util::read(fptr, buf, nbuf);
    ibis::fileManager::instance().recordPages(0, nread);
    if (nread <= 0) { // nothing is read, end-of-file or error ?
        if (nread == 0) {
            ierr = 0;
        }
        else {
            logWarning("string2int", "failed to read (read returned %ld)",
                       static_cast<long>(nread));
            ierr = -1;
        }
        return ierr;
    }
    if (nread < static_cast<int32_t>(nbuf)) {
        // end-of-file, make sure the last string is terminated properly
        if (buf[nread-1]) {
            buf[nread] = static_cast<char>(0);
            ++ nread;
        }
    }

    const char* last = buf + nread;
    const char* endchar = buf;  // points to the next NULL character
    const char* str = buf;      // points to the next string

    while (endchar < last && *endchar != static_cast<char>(0)) ++ endchar;
    if (endchar >= last) {
        logWarning("string2int", "encountered a string longer than %ld bytes",
                   static_cast<long>(nread));
        return -2;
    }

    while (endchar < last) { // *endchar == 0
        uint32_t ui = dic.insert(str);
        out.push_back(ui);
        ++ endchar; // skip over one NULL character
        str = endchar;
        while (endchar < last && *endchar != static_cast<char>(0)) ++ endchar;
    }

    if (endchar > str) { // need to move the file pointer backward
        long off = endchar - str;
        ierr = UnixSeek(fptr, -off, SEEK_CUR);
        if (ierr < 0) {
            logWarning("string2int", "failed to move file pointer back %ld "
                       "bytes (ierr=%ld)", off, ierr);
            ierr = -3;
        }
    }
    if (ierr >= 0)
        ierr = out.size();
    if (ibis::gVerbose > 4 && ierr >= 0)
        logMessage("string2int", "converted %ld string%s to integer%s",
                   ierr, (ierr>1?"s":""), (ierr>1?"s":""));
    return ierr;
} // ibis::column::string2int

/// Append the records in vals to the current working dataset.  The 'void*'
/// in this function follows the convention of the function getValuesArray
/// (not writeData), i.e., for the ten fixed-size elementary data types, it
/// is array_t<type>* and for string-valued columns it is
/// std::vector<std::string>*.
///
/// Return the number of entries actually written to disk or a negative
/// number to indicate error conditions.
long ibis::column::append(const void* vals, const ibis::bitvector& msk) {
    if (thePart == 0 || thePart->name() == 0 || thePart->currentDataDir() == 0)
        return -1L;
    if (m_name.empty()) return -2L;

    long ierr;
    writeLock lock(this, "appendValues");
    switch (m_type) {
    case ibis::BYTE:
        ierr = appendValues(* static_cast<const array_t<signed char>*>(vals),
                            msk);
        break;
    case ibis::UBYTE:
        ierr = appendValues(* static_cast<const array_t<unsigned char>*>(vals),
                            msk);
        break;
    case ibis::SHORT:
        ierr = appendValues(* static_cast<const array_t<int16_t>*>(vals), msk);
        break;
    case ibis::USHORT:
        ierr = appendValues(* static_cast<const array_t<uint16_t>*>(vals), msk);
        break;
    case ibis::INT:
        ierr = appendValues(* static_cast<const array_t<int32_t>*>(vals), msk);
        break;
    case ibis::UINT:
        ierr = appendValues(* static_cast<const array_t<uint32_t>*>(vals), msk);
        break;
    case ibis::LONG:
        ierr = appendValues(* static_cast<const array_t<int64_t>*>(vals), msk);
        break;
    case ibis::ULONG:
        ierr = appendValues(* static_cast<const array_t<uint64_t>*>(vals), msk);
        break;
    case ibis::FLOAT:
        ierr = appendValues(* static_cast<const array_t<float>*>(vals), msk);
        break;
    case ibis::DOUBLE:
        ierr = appendValues(* static_cast<const array_t<double>*>(vals), msk);
        break;
    case ibis::CATEGORY:
    case ibis::TEXT:
        ierr = appendStrings
            (* static_cast<const std::vector<std::string>*>(vals), msk);
        break;
    default:
        ierr = -3L;
        LOGGER(ibis::gVerbose > 1)
            << "Warning -- column[" << fullname()
            << "]::append can not handle type " << (int) m_type
            << " (" << ibis::TYPESTRING[(int)m_type] << ')';
        break;
    } // siwthc (m_type)
    return ierr;
} // ibis::column::append

/// This function attempts to fill the data file with NULL values if the
/// existing data file is shorter than expected.  It writes the data in
/// vals and extends the existing validity mask.
template <typename T>
long ibis::column::appendValues(const array_t<T>& vals,
                                const ibis::bitvector& msk) {
    std::string evt = "column[";
    evt += fullname();
    evt += "]::appendValues<";
    evt += typeid(T).name();
    evt += '>';
    std::string fn = thePart->currentDataDir();
    fn += FASTBIT_DIRSEP;
    fn += m_name;
    int curr = UnixOpen(fn.c_str(), OPEN_WRITEADD, OPEN_FILEMODE);
    if (curr < 0) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " failed to open file " << fn
            << " for writing -- " << (errno != 0 ? strerror(errno) : "??");
        return -5L;
    }
#if defined(_WIN32) && defined(_MSC_VER)
    (void)_setmode(curr, _O_BINARY);
#endif
    IBIS_BLOCK_GUARD(UnixClose, curr);

    long ierr = 0;
    const unsigned elem = sizeof(T);
    off_t oldsz = UnixSeek(curr, 0, SEEK_END);
    ibis::util::mutexLock lock(&mutex, evt.c_str());
    if (oldsz < 0)
        oldsz = 0;
    else
        oldsz = oldsz / elem;
    if (static_cast<uint32_t>(oldsz) < thePart->nRows()) {
        mask_.adjustSize(oldsz, thePart->nRows());
        while (static_cast<uint32_t>(oldsz) < thePart->nRows()) {
            const uint32_t nw =
                ((uint32_t)(thePart->nRows()-oldsz) <= vals.size() ?
                 (uint32_t)(thePart->nRows()-oldsz) : vals.size());
            ierr = ibis::util::write(curr, vals.begin(), nw*elem);
            if (ierr < static_cast<long>(nw*elem)) {
                LOGGER(ibis::gVerbose >= 0)
                    << "Warning -- " << evt << " failed to write " << nw*elem
                    << " bytes to " << fn << ", the write function returned "
                    << ierr;
                return -6L;
            }
        }
    }
    else if (static_cast<uint32_t>(oldsz) > thePart->nRows()) {
        mask_.adjustSize(thePart->nRows(), thePart->nRows());
        UnixSeek(curr, elem * thePart->nRows(), SEEK_SET);
    }

    ierr = ibis::util::write(curr, vals.begin(), vals.size()*elem);
    if (ierr < static_cast<long>(vals.size() * elem)) {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- " << evt << " failed to write " << vals.size()*elem
            << " bytes to " << fn << ", the write function returned " << ierr;
        return -7L;
    }

    LOGGER(ibis::gVerbose > 2)
        << evt << " successfully added " << vals.size() << " element"
        << (vals.size()>1?"s":"") << " to " << fn;

    ierr = vals.size();
    mask_ += msk;
    mask_.adjustSize(thePart->nRows()+vals.size(),
                     thePart->nRows()+vals.size());
    if (mask_.cnt() < mask_.size()) {
        fn += ".msk";
        mask_.write(fn.c_str());
    }
    return ierr;
} // ibis::column::appendValues

/// This function attempts to fill the existing data file with null values
/// based on the content of the validity mask.   It then write strings in
/// vals and extends the validity mask.
long ibis::column::appendStrings(const std::vector<std::string>& vals,
                                 const ibis::bitvector& msk) {
    std::string evt = "column[";
    evt += fullname();
    evt += "]::appendStrings";

    ibis::util::mutexLock lock(&mutex, evt.c_str());
    std::string fn = thePart->currentDataDir();
    fn += FASTBIT_DIRSEP;
    fn += m_name;
    int curr = UnixOpen(fn.c_str(), OPEN_APPENDONLY, OPEN_FILEMODE);
    if (curr < 0) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " failed to open file " << fn
            << " for writing -- " << (errno != 0 ? strerror(errno) : "??");
        return -5L;
    }
#if defined(_WIN32) && defined(_MSC_VER)
    (void)_setmode(curr, _O_BINARY);
#endif
    IBIS_BLOCK_GUARD(UnixClose, curr);

    long ierr = 0;
    if (mask_.size() < thePart->nRows()) {
        char tmp[128];
        for (unsigned j = 0; j < 128; ++ j)
            tmp[j] = 0;
        for (unsigned j = mask_.size(); j < thePart->nRows(); j += 128) {
            const long nw = (thePart->nRows()-j <= 128 ?
                             thePart->nRows()-j : 128);
            ierr = UnixWrite(curr, tmp, nw);
            if (ierr < nw) {
                LOGGER(ibis::gVerbose >= 0)
                    << "Warning -- " << evt << " failed to write " << nw
                    << " bytes to " << fn << ", the write function returned "
                    << ierr;
                return -6L;
            }
        }
        mask_.adjustSize(0, thePart->nRows());
    }

    for (ierr = 0; ierr < static_cast<long>(vals.size()); ++ ierr) {
        long jerr = UnixWrite(curr, vals[ierr].c_str(), 1+vals[ierr].size());
        if (jerr < static_cast<long>(1 + vals[ierr].size())) {
            LOGGER(ibis::gVerbose >= 0)
                << "Warning -- " << evt << " failed to write "
                << 1+vals[ierr].size() << " bytes to " << fn
                << ", the write function returned " << jerr;
            return -7L;
        }
    }

    LOGGER(ibis::gVerbose > 2)
        << evt << " successfully added " << vals.size() << " string"
        << (vals.size()>1?"s":"") << " to " << fn;
    mask_ += msk;
    mask_.adjustSize(thePart->nRows()+vals.size(),
                     thePart->nRows()+vals.size());
    if (mask_.cnt() < mask_.size()) {
        fn += ".msk";
        mask_.write(fn.c_str());
    }
    return ierr;
} // ibis::column::appendStrings

/// Write the content in array va1 to directory dir.  Extend the mask.
/// The void* is internally cast into a pointer to the fixed-size
/// elementary data types according to the type of column.  Therefore,
/// there is no way this function can handle string values.
/// - Normally: record the content in array va1 to the directory dir.
/// - Special case 1: the OID column writes the second array va2 only.
/// - Special case 2: for string values, va2 is recasted to be the number
///   of bytes in va1.
///
/// Return the number of entries actually written to file.  If writing was
/// completely successful, the return value should match nnew.  It also
/// extends the mask.  Write out the mask if not all the bits are set.
long ibis::column::writeData(const char *dir, uint32_t nold, uint32_t nnew,
                             ibis::bitvector& mask, const void *va1,
                             void *va2) {
    long ierr = 0;
    if (dir == 0 || nnew  == 0 || va1 == 0) return ierr;

    std::string evt = "column[";
    evt += fullname();
    evt += "]::writeData";

    uint32_t nact = 0;
    char fn[PATH_MAX];
    uint32_t ninfile=0;
    sprintf(fn, "%s%c%s", dir, FASTBIT_DIRSEP, m_name.c_str());
    ibis::fileManager::instance().flushFile(fn);

    FILE *fdat = fopen(fn, "ab");
    if (fdat == 0) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " failed to open \"" << fn
            << "\" for writing ... "
            << (errno ? strerror(errno) : "no free stdio stream");
        return ierr;
    }

    // Part I: write the content of val
    ninfile = ftell(fdat);
    if (m_type == ibis::UINT) {
        const unsigned int tmp = 4294967295U;
        const unsigned int elem = sizeof(unsigned int);
        if (ninfile != nold*elem) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << evt << " exptects file \"" << fn
                << "\" to have " << nold*elem << "bytes but it found only "
                << ninfile;
            if (ninfile > (nold+nnew)*elem) {
                // need to truncate the file
                fclose(fdat);
                ierr = truncate(fn, (nold+nnew)*elem);
                fdat = fopen(fn, "ab");
            }
            else if (ninfile < nold*elem) {
                ninfile /= elem;
                for (uint32_t i = ninfile; i < nold; ++ i) {
                    // write NULL values
                    ierr = fwrite(&tmp, elem, 1, fdat);
                    LOGGER(ierr == 0 && ibis::gVerbose >= 0)
                        << "Warning -- " << evt << " failed to write to \""
                        << fn << "\", fwrite returned " << ierr;
                }
            }
            ierr = fseek(fdat, nold*elem, SEEK_SET);
        }
        if (ninfile > nold)
            ninfile = nold;

        const unsigned int *arr = reinterpret_cast<const unsigned int*>(va1);
        unsigned int il, iu;
        il = arr[0];
        iu = arr[0];

        for (uint32_t i=1; i<nnew; ++i) {
            if (arr[i] > iu) {
                iu = arr[i];
            }
            else if (arr[i] < il) {
                il = arr[i];
            }
        }
        if (nold <= 0) {
            lower = il;
            upper = iu;
        }
        else {
            if (lower > il) lower = il;
            if (upper < iu) upper = iu;
        }

        nact = fwrite(arr, elem, nnew, fdat);
        fclose(fdat);
        LOGGER(nact < nnew && ibis::gVerbose > 0)
            << "Warning -- " << evt  << "expected to write " << nnew
            << " unsigned ints to \"" << fn << "\", but only wrote " << nact;
    }
    else if (m_type == ibis::INT) {
        // data type is int -- signed integers
        const int tmp = 2147483647;
        const unsigned int elem = sizeof(int);
        if (ninfile != nold*elem) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- " << evt << "expected file \"" << fn
                << "\" to have " << nold*elem << "bytes but it has "
                << ninfile;
            if (ninfile > (nold+nnew)*elem) {
                fclose(fdat);
                ierr = truncate(fn, (nold+nnew)*elem);
                fdat = fopen(fn, "ab");
            }
            else if (ninfile < nold*elem) {
                ninfile /= elem;
                for (uint32_t i = ninfile; i < nold; ++ i) {
                    // write NULLs
                    ierr = fwrite(&tmp, elem, 1, fdat);
                    LOGGER(ierr == 0 && ibis::gVerbose >= 0)
                        << "Warning -- " << evt << " failed to write to \""
                        << fn << "\", fwrite returned " << ierr;
                }
            }
            ierr = fseek(fdat, nold*elem, SEEK_SET);
        }
        if (ninfile > nold)
            ninfile = nold;

        const int *arr = reinterpret_cast<const int*>(va1);
        int il, iu;
        il = arr[0];
        iu = arr[0];

        for (uint32_t i = 1; i < nnew; ++ i) {
            if (arr[i] > iu) {
                iu = arr[i];
            }
            else if (arr[i] < il) {
                il = arr[i];
            }
        }
        if (nold <= 0) {
            lower = il;
            upper = iu;
        }
        else {
            if (lower > il) lower = il;
            if (upper < iu) upper = iu;
        }

        nact = fwrite(arr, elem, nnew, fdat);
        fclose(fdat);
        LOGGER(nact < nnew && ibis::gVerbose > 0)
            << "Warning -- " << evt << " expected to write " << nnew
            << " ints to \"" << fn << "\", but only wrote " << nact;
    }
    else if (m_type == ibis::USHORT) {
        // data type is unsigned 2-byte integer
        const unsigned short int tmp = 65535;
        const unsigned int elem = sizeof(unsigned short int);
        if (ninfile != nold*elem) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << evt << " exptected file \"" << fn
                << "\" to have " << nold*elem << " bytes but it has "
                << ninfile;
            if (ninfile > (nold+nnew)*elem) {
                // need to truncate the file
                fclose(fdat);
                ierr = truncate(fn, (nold+nnew)*elem);
                fdat = fopen(fn, "ab");
            }
            else if (ninfile < nold*elem) {
                ninfile /= elem;
                for (uint32_t i = ninfile; i < nold; ++ i) {
                    // write NULL values
                    ierr = fwrite(&tmp, elem, 1, fdat);
                    LOGGER(ierr == 0 && ibis::gVerbose >= 0)
                        << "Warning -- " << evt << " failed to write to \"" << fn
                        << "\", fwrite returned " << ierr;
                }
            }
            ierr = fseek(fdat, nold*elem, SEEK_SET);
        }
        if (ninfile > nold)
            ninfile = nold;

        const unsigned short int *arr =
            reinterpret_cast<const unsigned short int*>(va1);
        unsigned short int il, iu;
        il = arr[0];
        iu = arr[0];

        for (uint32_t i=1; i<nnew; ++i) {
            if (arr[i] > iu) {
                iu = arr[i];
            }
            else if (arr[i] < il) {
                il = arr[i];
            }
        }
        if (nold <= 0) {
            lower = il;
            upper = iu;
        }
        else {
            if (lower > il) lower = il;
            if (upper < iu) upper = iu;
        }

        nact = fwrite(arr, elem, nnew, fdat);
        fclose(fdat);
        LOGGER(nact < nnew && ibis::gVerbose > 0)
            << "Warning -- " << evt << " expected to write " << nnew
            << " unsigned ints to \"" << fn << "\", but only wrote " << nact;
    }
    else if (m_type == ibis::SHORT) {
        // data type is int -- signed short (2-byte) integers
        const short int tmp = 32767;
        const unsigned int elem = sizeof(short int);
        if (ninfile != nold*elem) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << evt << " expected file \"" << fn
                << "\" to have " << nold*elem << " bytes but it has "
                << ninfile;
            if (ninfile > (nold+nnew)*elem) {
                fclose(fdat);
                ierr = truncate(fn, (nold+nnew)*elem);
                fdat = fopen(fn, "ab");
            }
            else if (ninfile < nold*elem) {
                ninfile /= elem;
                for (uint32_t i = ninfile; i < nold; ++ i) {
                    // write NULLs
                    ierr = fwrite(&tmp, elem, 1, fdat);
                    LOGGER(ierr == 0 && ibis::gVerbose >= 0)
                        << "Warning -- " << evt << " failed to write to \""
                        << fn << "\", fwrite returned " << ierr;
                }
            }
            ierr = fseek(fdat, nold*elem, SEEK_SET);
        }
        if (ninfile > nold)
            ninfile = nold;

        const short int *arr = reinterpret_cast<const short int*>(va1);
        short int il, iu;
        il = arr[0];
        iu = arr[0];

        for (uint32_t i = 1; i < nnew; ++ i) {
            if (arr[i] > iu) {
                iu = arr[i];
            }
            else if (arr[i] < il) {
                il = arr[i];
            }
        }
        if (nold <= 0) {
            lower = il;
            upper = iu;
        }
        else {
            if (lower > il) lower = il;
            if (upper < iu) upper = iu;
        }

        nact = fwrite(arr, elem, nnew, fdat);
        fclose(fdat);
        LOGGER(nact < nnew && ibis::gVerbose > 0)
            << "Warning -- " << evt << " expected to write " << nnew
            << " short ints to \"" << fn << "\", but only wrote " << nact;
    }
    else if (m_type == ibis::UBYTE) {
        // data type is 1-byte integer
        const unsigned char tmp = 255;
        const unsigned int elem = sizeof(unsigned char);
        if (ninfile != nold*elem) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << evt << " expected file \"" << fn
                << "\" to have " << nold*elem <<  " bytes but it has "
                << ninfile;
            if (ninfile > (nold+nnew)*elem) {
                // need to truncate the file
                fclose(fdat);
                ierr = truncate(fn, (nold+nnew)*elem);
                fdat = fopen(fn, "ab");
            }
            else if (ninfile < nold*elem) {
                ninfile /= elem;
                for (uint32_t i = ninfile; i < nold; ++ i) {
                    // write NULL values
                    ierr = fwrite(&tmp, elem, 1, fdat);
                    LOGGER(ierr == 0 && ibis::gVerbose >= 0)
                        << "Warning -- " << evt << " failed to write to \""
                        << fn <<"\", fwrite returned " << ierr;
                }
            }
            ierr = fseek(fdat, nold*elem, SEEK_SET);
        }
        if (ninfile > nold)
            ninfile = nold;

        const unsigned char *arr = reinterpret_cast<const unsigned char*>(va1);
        unsigned char il, iu;
        il = arr[0];
        iu = arr[0];

        for (uint32_t i=1; i<nnew; ++i) {
            if (arr[i] > iu) {
                iu = arr[i];
            }
            else if (arr[i] < il) {
                il = arr[i];
            }
        }
        if (nold <= 0) {
            lower = il;
            upper = iu;
        }
        else {
            if (lower > il) lower = il;
            if (upper < iu) upper = iu;
        }

        nact = fwrite(arr, elem, nnew, fdat);
        fclose(fdat);
        LOGGER(nact < nnew && ibis::gVerbose > 0)
            << "Warning -- " << evt << " expected to write "<< nnew
            << " unsigned short ints to \"" << fn << "\", but only wrote "
            << nact;
    }
    else if (m_type == ibis::BYTE) {
        // data type is 1-byte signed integers
        const signed char tmp = 127;
        const unsigned int elem = sizeof(signed char);
        if (ninfile != nold*elem) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << evt << " expected file \"" << fn
                << "\" to have " << nold*elem << " bytes but it has "
                << ninfile;
            if (ninfile > (nold+nnew)*elem) {
                fclose(fdat);
                ierr = truncate(fn, (nold+nnew)*elem);
                fdat = fopen(fn, "ab");
            }
            else if (ninfile < nold*elem) {
                ninfile /= elem;
                for (uint32_t i = ninfile; i < nold; ++ i) {
                    // write NULLs
                    ierr = fwrite(&tmp, elem, 1, fdat);
                    LOGGER(ierr == 0 && ibis::gVerbose >= 0)
                        << "Warning -- " << evt << " failed to write to \""
                        << fn << "\", fwrite returned " << ierr;
                }
            }
            ierr = fseek(fdat, nold*elem, SEEK_SET);
        }
        if (ninfile > nold)
            ninfile = nold;

        const signed char *arr = reinterpret_cast<const signed char*>(va1);
        signed char il, iu;
        il = arr[0];
        iu = arr[0];

        for (uint32_t i = 1; i < nnew; ++ i) {
            if (arr[i] > iu) {
                iu = arr[i];
            }
            else if (arr[i] < il) {
                il = arr[i];
            }
        }
        if (nold <= 0) {
            lower = il;
            upper = iu;
        }
        else {
            if (lower > il) lower = il;
            if (upper < iu) upper = iu;
        }

        nact = fwrite(arr, elem, nnew, fdat);
        fclose(fdat);
        LOGGER(nact < nnew && ibis::gVerbose > 0)
            << "Warning -- " << evt << " expected to write " << nnew
            << " 8-bit ints to \"" << fn << "\", but only wrote " << nact;
    }
    else if (m_type == ibis::FLOAT) {
        // data type is float -- single precision floating-point values
        // #if INT_MAX == 0x7FFFFFFFL
        //      const int tmp = 0x7F800001; // NaN on a SUN workstation
        // #else
        //      const int tmp = INT_MAX;        // likely also a NaN
        // #endif
        const float tmp = FASTBIT_FLOAT_NULL;
        const unsigned int elem = sizeof(float);
        if (ninfile != nold*elem) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << evt << " expected file \"" << fn
                << "\" to have " << nold*elem << " bytes but it has "
                << ninfile;
            if (ninfile < nold*elem) {
                ninfile /= elem;
                for (uint32_t i = ninfile; i < nold; ++ i) {
                    // write NULLs
                    ierr = fwrite(&tmp, elem, 1, fdat);
                    LOGGER(ierr == 0 && ibis::gVerbose >= 0)
                        << "Warning -- " << evt << " failed to write to \""
                        << fn << "\", fwrite returned " << ierr;
                }
            }
            else if (ninfile > (nold+nnew)*elem) {
                fclose(fdat);
                ierr = truncate(fn, (nold+nnew)*elem);
                fdat = fopen(fn, "ab");
            }
            ierr = fseek(fdat, nold*elem, SEEK_SET);
        }
        if (ninfile > nold)
            ninfile = nold;

        const float *arr = reinterpret_cast<const float*>(va1);
        float il, iu;
        il = arr[0];
        iu = arr[0];

        for (uint32_t i = 1; i < nnew; ++ i) {
            if (arr[i] > iu) {
                iu = arr[i];
            }
            else if (arr[i] < il) {
                il = arr[i];
            }
        }
        if (nold <= 0) {
            lower = il;
            upper = iu;
        }
        else {
            if (lower > il) lower = il;
            if (upper < iu) upper = iu;
        }

        nact = fwrite(arr, elem, nnew, fdat);
        fclose(fdat);
        LOGGER(nact < nnew && ibis::gVerbose > 0)
            << "Warning -- " << evt << " expected to write " << nnew
            << " floats to \"" << fn << "\", but only wrote " << nact;
    }
    else if (m_type == ibis::DOUBLE) {
        // data type is double -- double precision floating-point values
        // #if INT_MAX == 0x7FFFFFFFL
        //      const int tmp[2]={0x7FFF0000, 0x00000001}; // NaN on a SUN workstation
        // #else
        //      const int tmp[2]={INT_MAX, INT_MAX};
        // #endif
        const double tmp = FASTBIT_DOUBLE_NULL;
        const unsigned int elem = sizeof(double);
        if (ninfile != nold*elem) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- " << evt << " expected file \"" << fn
                << "\" to have " << nold*elem << " bytes but it has "
                << ninfile;
            if (ninfile < nold*elem) {
                ninfile /= elem;
                for (uint32_t i = nact; i < nold; ++ i) {
                    // write NULLs
                    ierr = fwrite(&tmp, elem, 1, fdat);
                    LOGGER(ierr == 0 && ibis::gVerbose >= 0)
                        << "Warning -- " << evt << " failed to write to \""
                        << fn << "\", fwrite returned " << ierr;
                }
            }
            else if (ninfile > (nold+nnew)*elem) {
                fclose(fdat);
                ierr = truncate(fn, (nold+nnew)*elem);
                fdat = fopen(fn, "ab");
            }
            ierr = fseek(fdat, nold*elem, SEEK_SET);
        }
        if (ninfile > nold)
            ninfile = nold;

        const double *arr = reinterpret_cast<const double*>(va1);

        for (uint32_t i = 0; i < nnew; ++ i) {
            if (arr[i] > upper) {
                upper = arr[i];
            }
            if (arr[i] < lower) {
                lower = arr[i];
            }
        }

        nact = fwrite(arr, elem, nnew, fdat);
        fclose(fdat);
        LOGGER(nact < nnew && ibis::gVerbose > 0)
            << "Warning " << evt << " expected to write " << nnew
            << " doubles to \"" << fn << "\", but only wrote " << nact;
    }
    else if (m_type == ibis::OID) {
        // OID is formed from two unsigned ints, i.e., both va1 and va2 are
        // used here
        // use logError to terminate this function upon any error
        if (va2 == 0 || va1 == 0) {
            fclose(fdat);
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << evt
                << " needs both components of OID to be valid";
            return 0;
        }
        else if (ninfile != 8*nold) {
            fclose(fdat);
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << evt << " expected OID file \"" << fn
                << "\" to have " << 8*nold << " bytes, but it has "
                << ninfile;
            return 0;
        }
        else {
            const unsigned int *rn = reinterpret_cast<const unsigned*>(va1);
            const unsigned int *en = reinterpret_cast<const unsigned*>(va2);
            for (nact = 0; nact < nnew; ++ nact) {
                ierr = fwrite(rn+nact, sizeof(unsigned), 1, fdat);
                ierr += fwrite(en+nact, sizeof(unsigned), 1, fdat);
                if (ierr != 2) {
                    fclose(fdat);
                    LOGGER(ibis::gVerbose > 0)
                        << "Warning -- " << evt << " failed to write new OID # "
                        << nact << "to \"" << fn << "\", fwrite returned "
                        << ierr;
                    break;
                }
            }
            fclose(fdat);
            LOGGER(nact != nnew && ibis::gVerbose > 0)
                << "Warning -- " << evt << " expected nact(=" << nact
                << ") to be the same as nnew(=" << nnew
                << ") for the OID column, remove \"" << fn << "\"";
                (void) remove(fn);
                nact = 0;
            return nact;
        }
    }
    else if (m_type == ibis::CATEGORY ||
             m_type == ibis::TEXT) {
        // data type TEXT/CATEGORY -- string valued columns to check the
        // size properly, we will have to go through the whole file.  To
        // avoid that expense, only do a minimum amount of checking
        uint32_t oldbytes = ninfile;
        if (nold > 0) { // check with mask file for ninfile
            char tmp[1024];
            (void) memset(tmp, 0, 1024);
            ninfile = mask.size();
            if (nold > ninfile) {
                LOGGER(ibis::gVerbose > 2)
                    << evt << " adding " << (nold-ninfile)
                    << " null string(s) (mask.size()=" << ninfile
                    << ", nold=" << nold << ")";
                for (uint32_t i = ninfile; i < nold; i += 1024)
                    fwrite(tmp, 1, (nold-i>1024)?1024:(nold-i), fdat);
            }
        }
        else {
            ninfile = 0;
        }

        const char* arr = reinterpret_cast<const char*>(va1);
        const uint32_t nbytes =
            *reinterpret_cast<const uint32_t*>(va2);
        nact = fwrite(arr, 1, nbytes, fdat);
        fclose(fdat);
        if (nact != nbytes) { // no easy way to recover
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << evt << " expected to write " << nbytes
                << " bytes to \"" << fn << "\", but only wrote " << nact;
            ierr = truncate(fn, oldbytes);
            nact = 0;
        }
        else {
            LOGGER(ibis::gVerbose > 7)
                << evt << " wrote " << nact << " bytes of strings";
            nact = nnew;
        }
    }
    else {
        fclose(fdat);
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << "does not yet supported type "
            << ibis::TYPESTRING[(int)(m_type)];
        return 0;
    }

    if (ibis::gVerbose > 5) {
        ibis::util::logger lg;
        lg() << evt << " wrote " << nact << " entr" << (nact>1?"ies":"y")
             << " of type " << ibis::TYPESTRING[(int)m_type]
             << " (expected " << nnew << ") to " << fn << "\n";
        if (ibis::gVerbose > 16)
            lg() << *this;
    }

    // part II: append new bits to update the null mask
    strcat(fn, ".msk");
    mask.adjustSize(ninfile, nold);
    mask.adjustSize(nact+nold, nnew+nold);
    if (mask.cnt() < mask.size()) {
        mask.write(fn);
        LOGGER(ibis::gVerbose > 8)
            << evt << " wrote the new null mask to \"" << fn << "\" with "
            << mask.cnt() << " set bits out of " << mask.size();
    }
    else if (ibis::util::getFileSize(fn) > 0) {
        (void) remove(fn);
    }
    ibis::fileManager::instance().flushFile(fn);
    return nact;
} // ibis::column::writeData

/// Write the selected records to the specified directory.
/// Save only the rows marked 1.  Replace the data file in @c dest.
/// Return the number of rows written to the new file or a negative number
/// to indicate error.
long ibis::column::saveSelected(const ibis::bitvector& sel, const char *dest,
                                char *buf, uint32_t nbuf) {
    const int elm = elementSize();
    if (thePart == 0 || thePart->currentDataDir() == 0 || elm <= 0)
        return -1;

    long ierr = 0;
    ibis::fileManager::buffer<char> mybuf(buf != 0);
    if (buf == 0) {
        nbuf = mybuf.size();
        buf = mybuf.address();
    }
    if (buf == 0) {
        throw new ibis::bad_alloc("saveSelected cannot allocate workspace"
                                  IBIS_FILE_LINE);
    }

    if (dest == 0 || dest == thePart->currentDataDir() ||
        std::strcmp(dest, thePart->currentDataDir()) == 0) { // same directory
        std::string fname = thePart->currentDataDir();
        if (! fname.empty())
            fname += FASTBIT_DIRSEP;
        fname += m_name;
        ibis::bitvector current;
        getNullMask(current);

        writeLock lock(this, "saveSelected");
        if (idx != 0) {
            const uint32_t idxc = idxcnt();
            if (0 == idxc) {
                delete idx;
                idx = 0;
                purgeIndexFile(thePart->currentDataDir());
            }
            else {
                logWarning("saveSelected", "index files are in-use, "
                           "should not overwrite data files");
                return -2;
            }
        }
        ibis::fileManager::instance().flushFile(fname.c_str());
        FILE* fptr = fopen(fname.c_str(), "r+b");
        if (fptr == 0) {
            if (ibis::gVerbose > -1)
                logWarning("saveSelected", "failed to open file \"%s\"",
                           fname.c_str());
            return -3;
        }

        off_t pos = 0; // position to write the next byte
        for (ibis::bitvector::indexSet ix = sel.firstIndexSet();
             ix.nIndices() > 0; ++ ix) {
            const ibis::bitvector::word_t *idx = ix.indices();
            if (ix.isRange()) {
                if ((uint32_t) pos < elm * *idx) {
                    const off_t endpos = idx[1] * elm;
                    for (off_t j = *idx * elm; j < endpos; j += nbuf) {
                        fflush(fptr); // prepare for reading
                        ierr = fseek(fptr, j, SEEK_SET);
                        if (ierr != 0) {
                            if (ibis::gVerbose > 0)
                                logWarning("saveSelected", "failed to seek to "
                                           "%lu in file \"%s\"",
                                           static_cast<long unsigned>(j),
                                           fname.c_str());
                            ierr = -4;
                            fclose(fptr);
                            return ierr;
                        }

                        off_t nbytes = (j+(off_t)nbuf <= endpos ?
                                        nbuf : endpos-j);
                        ierr = fread(buf, 1, nbytes, fptr);
                        if (ierr < 0) {
                            if (ibis::gVerbose > 0)
                                logWarning("saveSelected", "failed to read "
                                           "file \"%s\" at position %lu, "
                                           "fill buffer with 0",
                                           fname.c_str(),
                                           static_cast<long unsigned>(j));
                            ierr = 0;
                        }
                        for (; ierr < nbytes; ++ ierr)
                            buf[ierr] = 0;

                        fflush(fptr); // prepare to write
                        ierr = fseek(fptr, pos, SEEK_SET);
                        ierr += fwrite(buf, 1, nbytes, fptr);
                        if (ierr < nbytes) {
                            if (ibis::gVerbose > 0)
                                logWarning("saveSelected", "failed to write "
                                           "%lu bytes to file \"%s\" at "
                                           "position %lu",
                                           static_cast<long unsigned>(nbytes),
                                           fname.c_str(),
                                           static_cast<long unsigned>(pos));
                        }
                        pos += nbytes;
                    } // for (off_t j...
                }
                else { // don't need to write anything here
                    pos += elm * (idx[1] - *idx);
                }
            }
            else {
                fflush(fptr);
                ierr = fseek(fptr, *idx * elm, SEEK_SET);
                if (ierr != 0) {
                    if (ibis::gVerbose > 0)
                        logWarning("saveSelected", "failed to seek to "
                                   "%lu in file \"%s\"",
                                   static_cast<long unsigned>(*idx * elm),
                                   fname.c_str());
                    ierr = -5;
                    fclose(fptr);
                    return ierr;
                }
                const off_t nread = elm * (idx[ix.nIndices()-1] - *idx + 1);
                ierr = fread(buf, 1, nread, fptr);
                if (ierr < 0) {
                    if (ibis::gVerbose > 0)
                        logWarning("saveSelected", "failed to read "
                                   "file \"%s\" at position %lu, "
                                   "fill buffer with 0",
                                   fname.c_str(), ierr);
                    ierr = 0;
                }
                for (; ierr < nread; ++ ierr)
                    buf[ierr] = static_cast<char>(0);

                fflush(fptr); // prepare to write
                ierr = fseek(fptr, pos, SEEK_SET);
                for (uint32_t j = 0; j < ix.nIndices(); ++ j) {
                    ierr = fwrite(buf + elm * (idx[j] - *idx), 1, elm, fptr);
                    if (ierr < elm) {
                        if (ibis::gVerbose > 0)
                            logWarning("saveSelected", "failed to write a "
                                       "%d-byte element to %lu in file \"%s\"",
                                       elm, static_cast<long unsigned>(pos),
                                       fname.c_str());
                    }
                    pos += elm;
                }
            }
        }
        fclose(fptr);
        ierr = truncate(fname.c_str(), pos);
        ierr = static_cast<long>(pos / elm);
        if (ibis::gVerbose > 1)
            logMessage("saveSelected", "rewrote data file %s with %ld row%s",
                       fname.c_str(), ierr, (ierr > 1 ? "s" : ""));

        ibis::bitvector bv;
        current.subset(sel, bv);
        fname += ".msk";

        ibis::util::mutexLock mtx(&mutex, "saveSelected");
        mask_.swap(bv);
        if (mask_.size() > mask_.cnt())
            mask_.write(fname.c_str());
        else
            remove(fname.c_str());
        if (ibis::gVerbose > 3)
            logMessage("saveSelected", "new column mask %lu out of %lu",
                       static_cast<long unsigned>(mask_.cnt()),
                       static_cast<long unsigned>(mask_.size()));
    }
    else { // different directory
        std::string sfname = thePart->currentDataDir();
        std::string dfname = dest;
        if (! sfname.empty()) sfname += FASTBIT_DIRSEP;
        if (! dfname.empty()) dfname += FASTBIT_DIRSEP;
        sfname += m_name;
        dfname += m_name;

        purgeIndexFile(dest);
        readLock lock(this, "saveSelected");
        FILE* sfptr = fopen(sfname.c_str(), "rb");
        if (sfptr == 0) {
            if (ibis::gVerbose > 0)
                logWarning("saveSelected", "failed to open file \"%s\" for "
                           "reading", sfname.c_str());
            return -6;
        }
        ibis::fileManager::instance().flushFile(dfname.c_str());
        FILE* dfptr = fopen(dfname.c_str(), "wb");
        if (dfptr == 0) {
            if (ibis::gVerbose > 0)
                logWarning("saveSelected", "failed to open file \"%s\" for "
                           "writing", dfname.c_str());
            fclose(sfptr);
            return -7;
        }

        for (ibis::bitvector::indexSet ix = sel.firstIndexSet();
             ix.nIndices() > 0; ++ ix) {
            const ibis::bitvector::word_t *idx = ix.indices();
            ierr = fseek(sfptr, *idx * elm, SEEK_SET);
            if (ierr != 0) {
                if (ibis::gVerbose > 0)
                    logWarning("saveSelected", "failed to seek to %ld in "
                               "file \"%s\"", static_cast<long>(*idx * elm),
                               sfname.c_str());
                fclose(sfptr);
                fclose(dfptr);
                return -8;
            }

            if (ix.isRange()) {
                const off_t endblock = idx[1] * elm;
                for (off_t j = *idx * elm; j < endblock; j += nbuf) {
                    const off_t nbytes =
                        elm * (j+(off_t)nbuf <= endblock ? nbuf : endblock-j);
                    ierr = fread(buf, 1, nbytes, sfptr);
                    if (ierr < 0) {
                        if (ibis::gVerbose > 0)
                            logWarning("saveSelected", "failed to read from "
                                       "\"%s\" at position %lu, fill buffer "
                                       "with 0", sfname.c_str(),
                                       static_cast<long unsigned>(j));
                        ierr = 0;
                    }
                    for (; ierr < nbytes; ++ ierr)
                        buf[ierr] = static_cast<char>(0);
                    ierr = fwrite(buf, 1, nbytes, dfptr);
                    if (ierr < nbytes && ibis::gVerbose > 0)
                        logWarning("saveSelected", "expected to write %lu "
                                   "bytes to \"%s\", but only wrote %ld",
                                   static_cast<long unsigned>(nbytes),
                                   dfname.c_str(), static_cast<long>(ierr));
                }
            }
            else {
                const off_t nbytes = elm * (idx[ix.nIndices()-1] - *idx + 1);
                ierr = fread(buf, 1, nbytes, sfptr);
                if (ierr < 0) {
                    if (ibis::gVerbose > 0)
                        logWarning("saveSelected", "failed to read from "
                                   "\"%s\" at position %lu, fill buffer "
                                   "with 0", sfname.c_str(),
                                   static_cast<long unsigned>(*idx * elm));
                    ierr = 0;
                }
                for (; ierr < nbytes; ++ ierr)
                    buf[ierr] = static_cast<char>(0);
                for (uint32_t j = 0; j < ix.nIndices(); ++ j) {
                    ierr = fwrite(buf + elm * (idx[j] - *idx), 1, elm, dfptr);
                    if (ierr < elm && ibis::gVerbose > 0)
                        logWarning("saveSelected", "expected to write a "
                                   "%d-byte element to \"%s\", but only "
                                   "wrote %d byte(s)", elm,
                                   dfname.c_str(), static_cast<int>(ierr));
                }
            }
        }
        if (ibis::gVerbose > 1)
            logMessage("saveSelected", "copied %ld row%s from %s to %s",
                       ierr, (ierr > 1 ? "s" : ""), sfname.c_str(),
                       dfname.c_str());

        ibis::bitvector current, bv;
        getNullMask(current);
        current.subset(sel, bv);
        dfname += ".msk";
        if (bv.size() != bv.cnt())
            bv.write(dfname.c_str());
        else
            remove(dfname.c_str());
        if (ibis::gVerbose > 3)
            logMessage("saveSelected", "saved new mask (%lu out of %lu) to %s",
                       static_cast<long unsigned>(bv.cnt()),
                       static_cast<long unsigned>(bv.size()),
                       dfname.c_str());
    }

    return ierr;
} // ibis::column::saveSelected

/// Truncate the number of records in the named dir to nent.  It truncates
/// file if more entries are in the current file, and it adds more NULL
/// values if the current file is shorter.  The null mask is adjusted
/// accordingly.
long ibis::column::truncateData(const char* dir, uint32_t nent,
                                ibis::bitvector& mask) const {
    long ierr = 0;
    if (dir == 0)
        return -1;
    char fn[MAX_LINE];
#if defined(AHVE_SNPRINTF)
    ierr = UnixSnprintf(fn, MAX_LINE, "%s%c%s", dir, FASTBIT_DIRSEP,
                        m_name.c_str());
#else
    ierr = sprintf(fn, "%s%c%s", dir, FASTBIT_DIRSEP, m_name.c_str());
#endif
    if (ierr <= 0 || ierr > MAX_LINE) {
        logWarning("truncateData", "failed to generate data file name, "
                   "name (%s%c%s) too long", dir, FASTBIT_DIRSEP,
                   m_name.c_str());
        return -2;
    }

    uint32_t nact = 0; // number of valid entries left in the file
    uint32_t nbyt = 0; // number of bytes in the file to be left
    char buf[MAX_LINE];
    if (m_type == ibis::CATEGORY ||
        m_type == ibis::TEXT) {
        // character strings -- need to read the content file
        array_t<char> *arr = new array_t<char>;
        ierr = ibis::fileManager::instance().getFile(fn, *arr);
        if (ierr == 0) {
            uint32_t cnt = 0;
            const char *end = arr->end();
            const char *ptr = arr->begin();
            while (cnt < nent && ptr < end) {
                cnt += (*ptr == 0);
                ++ ptr;
            }
            nact = cnt;
            nbyt = ptr - arr->begin();
            delete arr; // no longer need the array_t
            ibis::fileManager::instance().flushFile(fn);

            if (cnt < nent) { // current file does not have enough entries
                memset(buf, 0, MAX_LINE);
                FILE *fptr = fopen(fn, "ab");
                while (cnt < nent) {
                    uint32_t nb = nent - cnt;
                    if (nb > MAX_LINE)
                        nb = MAX_LINE;
                    ierr = fwrite(buf, 1, nb, fptr);
                    if (static_cast<uint32_t>(ierr) != nb) {
                        logWarning("truncateData", "expected to write "
                                   "%lu bytes to \"%s\", but only wrote "
                                   "%ld", static_cast<long unsigned>(nb),
                                   fn, ierr);
                        if (ierr == 0) {
                            ierr = -1;
                            break;
                        }
                    }
                    cnt += ierr;
                }
                nbyt = ftell(fptr);
                fclose(fptr);
            }
            ierr = (ierr>=0 ? 0 : -1);
        }
        else {
            logWarning("truncateData", "failed to open \"%s\" using the "
                       "file manager, ierr=%ld", fn, ierr);
            FILE *fptr = fopen(fn, "rb+"); // open for read and write
            if (fptr != 0) {
                uint32_t cnt = 0;
                while (cnt < nent) {
                    ierr = fread(buf, 1, MAX_LINE, fptr);
                    if (ierr == 0) break;
                    int i = 0;
                    for (i = 0; cnt < nent && i < MAX_LINE; ++ i)
                        cnt += (buf[i] == 0);
                    nbyt += i;
                }
                nact = cnt;

                if (cnt < nent) { // need to write more null characters
                    memset(buf, 0, MAX_LINE);
                    while (cnt < nent) {
                        uint32_t nb = nent - cnt;
                        if (nb > MAX_LINE)
                            nb = MAX_LINE;
                        ierr = fwrite(buf, 1, nb, fptr);
                        if (static_cast<uint32_t>(ierr) != nb) {
                            logWarning("truncateData", "expected to write "
                                       "%lu bytes to \"%s\", but only wrote "
                                       "%ld", static_cast<long unsigned>(nb),
                                       fn, ierr);
                            if (ierr == 0) {
                                ierr = -1;
                                break;
                            }
                        }
                        cnt += ierr;
                    }
                    nbyt = ftell(fptr);
                }
                fclose(fptr);
                ierr = (ierr >= 0 ? 0 : -1);
            }
            else {
                logWarning("truncateData", "failed to open \"%s\" with "
                           "fopen, file probably does not exist or has "
                           "wrong perssions", fn);
                ierr = -1;
            }
        }
    }
    else { // other fixed size columns
        const uint32_t elm = elementSize();
        nbyt = ibis::util::getFileSize(fn);
        nact = nbyt / elm;
        if (nact < nent) { // needs to write more entries to the file
            FILE *fptr = fopen(fn, "ab");
            if (fptr != 0) {
                uint32_t cnt = nact;
                memset(buf, 0, MAX_LINE);
                while (cnt < nent) {
                    uint32_t nb = (nent - cnt) * elm;
                    if (nb > MAX_LINE)
                        nb = ((MAX_LINE / elm) * elm);
                    ierr = fwrite(buf, 1, nb, fptr);
                    if (static_cast<uint32_t>(ierr) != nb) {
                        logWarning("truncateData", "expected to write "
                                   "%lu bytes to \"%s\", but only wrote "
                                   "%ld", static_cast<long unsigned>(nb),
                                   fn, ierr);
                        if (ierr == 0) {
                            ierr = -1;
                            break;
                        }
                    }
                    cnt += ierr;
                }
                nbyt = ftell(fptr);
                fclose(fptr);
                ierr = (ierr >= 0 ? 0 : -1);
            }
            else {
                logWarning("truncateData", "failed to open \"%s\" with "
                           "fopen, make sure the directory exist and has "
                           "right perssions", fn);
                ierr = -1;
            }
        }
    }

    // actually tuncate the file here
    if (ierr == 0) {
        ierr = truncate(fn, nbyt);
        if (ierr != 0) {
            logWarning("truncateData", "failed to truncate \"%s\" to "
                       "%lu bytes, ierr=%ld", fn,
                       static_cast<long unsigned>(nbyt), ierr);
            ierr = -2;
        }
        else {
            ierr = nent;
            if (ibis::gVerbose > 8)
                logMessage("truncateData", "successfully trnncated \"%s\" "
                           "to %lu bytes (%lu records)", fn,
                           static_cast<long unsigned>(nbyt),
                           static_cast<long unsigned>(nent));
        }
    }

    // dealing with the null mask
    strcat(fn, ".msk");
    mask.adjustSize(nact, nent);
    if (mask.cnt() < mask.size()) {
        mask.write(fn);
        if (ibis::gVerbose > 7)
            logMessage("truncateData", "null mask in \"%s\" contains %lu "
                       "set bits and %lu total bits", fn,
                       static_cast<long unsigned>(mask.cnt()),
                       static_cast<long unsigned>(mask.size()));
    }
    else if (ibis::util::getFileSize(fn) > 0) {
        (void) remove(fn);
    }
    return ierr;
} // ibis::column::truncateData

/// Cast the incoming array into the specified type T before writing the
/// values to the file for this column.  This function uses assignment
/// statements to perform the casting operations.  Warning: this function
/// does not check that the cast values are equal to the incoming values!
template <typename T>
long ibis::column::castAndWrite(const array_t<double>& vals,
                                ibis::bitvector& mask, const T special) {
    array_t<T> tmp(mask.size());
    ibis::bitvector::word_t jtmp = 0;
    ibis::bitvector::word_t jvals = 0;
    for (ibis::bitvector::indexSet is = mask.firstIndexSet();
         is.nIndices() > 0; ++ is) {
        const ibis::bitvector::word_t *idx = is.indices();
        while (jtmp < *idx) {
            tmp[jtmp] = special;
            ++ jtmp;
        }
        if (is.isRange()) {
            while (jtmp < idx[1]) {
                if (lower > vals[jvals])
                    lower = vals[jvals];
                if (upper < vals[jvals])
                    upper = vals[jvals];
                tmp[jtmp] = vals[jvals];
                ++ jvals;
                ++ jtmp;
            }
        }
        else {
            for (unsigned i = 0; i < is.nIndices(); ++ i) {
                while (jtmp < idx[i]) {
                    tmp[jtmp] = special;
                    ++ jtmp;
                }
                if (lower > vals[jvals])
                    lower = vals[jvals];
                if (upper < vals[jvals])
                    upper = vals[jvals];
                tmp[jtmp] = vals[jvals];
                ++ jvals;
                ++ jtmp;
            }
        }
    }
    while (jtmp < mask.size()) {
        tmp[jtmp] = special;
        ++ jtmp;
    }
    long ierr = writeData(thePart->currentDataDir(), 0, mask.size(), mask,
                          tmp.begin(), 0);
    return ierr;
} // ibis::column::castAndWrite
                              
template <typename T>
void ibis::column::actualMinMax(const array_t<T>& vals,
                                const ibis::bitvector& mask,
                                double& min, double& max, bool &asc) {
    asc = true;
    min = DBL_MAX;
    max = - DBL_MAX;
    if (vals.empty() || mask.cnt() == 0) return;

    T amin = std::numeric_limits<T>::max();
    T amax = std::numeric_limits<T>::min();
    if (amax > 0) amax = -amin;
    T aprev = amax;
    for (ibis::bitvector::indexSet ix = mask.firstIndexSet();
         ix.nIndices() > 0; ++ ix) {
        const ibis::bitvector::word_t *idx = ix.indices();
        if (ix.isRange()) {
            ibis::bitvector::word_t last = (idx[1] <= vals.size() ?
                                            idx[1] : vals.size());
            for (uint32_t i = *idx; i < last; ++ i) {
                amin = (amin > vals[i] ? vals[i] : amin);
                amax = (amax < vals[i] ? vals[i] : amax);
                if (asc)
                    asc = (vals[i] >= aprev);
                aprev = vals[i];
            }
        }
        else {
            for (uint32_t i = 0; i < ix.nIndices() && idx[i] < vals.size();
                 ++ i) {
                amin = (amin > vals[idx[i]] ? vals[idx[i]] : amin);
                amax = (amax < vals[idx[i]] ? vals[idx[i]] : amax);
                if (asc)
                    asc = (vals[idx[i]] >= aprev);
                aprev = vals[idx[i]];
            }
        }
    }

    min = static_cast<double>(amin);
    max = static_cast<double>(amax);
    LOGGER(ibis::gVerbose > 5)
        << "actualMinMax<" << typeid(T).name() << "> -- vals.size() = "
        << vals.size() << ", mask.cnt() = " << mask.cnt()
        << ", min = " << min << ", max = " << max << ", asc = " << asc;
} // ibis::column::actualMinMax

template <typename T>
T ibis::column::computeMin(const array_t<T>& vals,
                           const ibis::bitvector& mask) {
    T res = std::numeric_limits<T>::max();
    if (vals.empty() || mask.cnt() == 0) return res;

    for (ibis::bitvector::indexSet ix = mask.firstIndexSet();
         ix.nIndices() > 0; ++ ix) {
        const ibis::bitvector::word_t *idx = ix.indices();
        if (ix.isRange()) {
            ibis::bitvector::word_t last = (idx[1] <= vals.size() ?
                                            idx[1] : vals.size());
            for (uint32_t i = *idx; i < last; ++ i) {
                res = (res > vals[i] ? vals[i] : res);
            }
        }
        else {
            for (uint32_t i = 0; i < ix.nIndices() && idx[i] < vals.size();
                 ++ i) {
                res = (res > vals[idx[i]] ? vals[idx[i]] : res);
            }
        }
    }
    return res;
} // ibis::column::computeMin

template <typename T>
T ibis::column::computeMax(const array_t<T>& vals,
                           const ibis::bitvector& mask) {
    T res = std::numeric_limits<T>::min();
    if (res > 0) res = -std::numeric_limits<T>::max();
    if (vals.empty() || mask.cnt() == 0) return res;

    for (ibis::bitvector::indexSet ix = mask.firstIndexSet();
         ix.nIndices() > 0; ++ ix) {
        const ibis::bitvector::word_t *idx = ix.indices();
        if (ix.isRange()) {
            ibis::bitvector::word_t last = (idx[1] <= vals.size() ?
                                            idx[1] : vals.size());
            for (uint32_t i = *idx; i < last; ++ i) {
                res = (res < vals[i] ? vals[i] : res);
            }
        }
        else {
            for (uint32_t i = 0; i < ix.nIndices() && idx[i] < vals.size();
                 ++ i) {
                res = (res < vals[idx[i]] ? vals[idx[i]] : res);
            }
        }
    }
    return res;
} // ibis::column::computeMax

template <typename T>
double ibis::column::computeSum(const array_t<T>& vals,
                                const ibis::bitvector& mask) {
    double res = 0.0;
    if (vals.empty() || mask.cnt() == 0) return res;

    for (ibis::bitvector::indexSet ix = mask.firstIndexSet();
         ix.nIndices() > 0; ++ ix) {
        const ibis::bitvector::word_t *idx = ix.indices();
        if (ix.isRange()) {
            ibis::bitvector::word_t last = (idx[1] <= vals.size() ?
                                            idx[1] : vals.size());
            for (uint32_t i = *idx; i < last; ++ i) {
                res += vals[i];
            }
        }
        else {
            for (uint32_t i = 0; i < ix.nIndices() && idx[i] < vals.size();
                 ++ i) {
                res += vals[idx[i]];
            }
        }
    }
    return res;
} // ibis::column::computeSum

// actually go through values and determine the min/max values, the min is
// recordeed as lowerBound and the max is recorded as the upperBound
double ibis::column::computeMin() const {
    double ret = DBL_MAX;
    if (thePart->nRows() == 0) return ret;

    ibis::bitvector mask;
    getNullMask(mask);
    if (mask.cnt() == 0) return ret;

    std::string sname;
    const char* name = dataFileName(sname);
    if (name == 0) return ret;

    switch (m_type) {
    case ibis::UBYTE: {
        array_t<unsigned char> val;
        int ierr = ibis::fileManager::instance().getFile(name, val);
        if (ierr != 0) {
            logWarning("computeMin", "failed to retrieve file %s", name);
        }
        else {
            ret = computeMin(val, mask);
        }
        break;}
    case ibis::BYTE: {
        array_t<signed char> val;
        int ierr = ibis::fileManager::instance().getFile(name, val);
        if (ierr != 0) {
            logWarning("computeMin", "failed to retrieve file %s", name);
        }
        else {
            ret = computeMin(val, mask);
        }
        break;}
    case ibis::USHORT: {
        array_t<uint16_t> val;
        int ierr = ibis::fileManager::instance().getFile(name, val);
        if (ierr != 0) {
            logWarning("computeMin", "failed to retrieve file %s", name);
        }
        else {
            ret = computeMin(val, mask);
        }
        break;}
    case ibis::SHORT: {
        array_t<int16_t> val;
        int ierr = ibis::fileManager::instance().getFile(name, val);
        if (ierr != 0) {
            logWarning("computeMin", "failed to retrieve file %s", name);
        }
        else {
            ret = computeMin(val, mask);
        }
        break;}
    case ibis::UINT: {
        array_t<uint32_t> val;
        int ierr = ibis::fileManager::instance().getFile(name, val);
        if (ierr != 0) {
            logWarning("computeMin", "failed to retrieve file %s", name);
        }
        else {
            ret = computeMin(val, mask);
        }
        break;}
    case ibis::INT: {
        array_t<int32_t> val;
        int ierr = ibis::fileManager::instance().getFile(name, val);
        if (ierr != 0) {
            logWarning("computeMin", "failed to retrieve file %s", name);
        }
        else {
            ret = computeMin(val, mask);
        }
        break;}
    case ibis::ULONG: {
        array_t<uint64_t> val;
        int ierr = ibis::fileManager::instance().getFile(name, val);
        if (ierr != 0) {
            logWarning("computeMin", "failed to retrieve file %s", name);
        }
        else {
            ret = computeMin(val, mask);
        }
        break;}
    case ibis::LONG: {
        array_t<int64_t> val;
        int ierr = ibis::fileManager::instance().getFile(name, val);
        if (ierr != 0) {
            logWarning("computeMin", "failed to retrieve file %s", name);
        }
        else {
            ret = computeMin(val, mask);
        }
        break;}
    case ibis::FLOAT: {
        array_t<float> val;
        int ierr = ibis::fileManager::instance().getFile(name, val);
        if (ierr != 0) {
            logWarning("computeMin", "failed to retrieve file %s", name);
        }
        else {
            ret = computeMin(val, mask);
        }
        break;}
    case ibis::DOUBLE: {
        array_t<double> val;
        int ierr = ibis::fileManager::instance().getFile(name, val);
        if (ierr != 0) {
            logWarning("computeMin", "failed to retrieve file %s", name);
        }
        else {
            ret = computeMin(val, mask);
        }
        break;}
    default:
        LOGGER(ibis::gVerbose > 1)
            << "Warning -- column[" << fullname()
            << "]::computeMin can not work with column type "
            << ibis::TYPESTRING[(int)m_type];
    } // switch(m_type)

    return ret;
} // ibis::column::computeMin

// actually go through values and determine the min/max values, the min is
// recordeed as lowerBound and the max is recorded as the upperBound
double ibis::column::computeMax() const {
    double res = -DBL_MAX;
    if (thePart->nRows() == 0) return res;

    ibis::bitvector mask;
    getNullMask(mask);
    if (mask.cnt() == 0) return res;

    std::string sname;
    const char* name = dataFileName(sname);
    if (name == 0) return res;

    switch (m_type) {
    case ibis::UBYTE: {
        array_t<unsigned char> val;
        int ierr = ibis::fileManager::instance().getFile(name, val);
        if (ierr != 0) {
            logWarning("computeMax", "failed to retrieve file %s", name);
        }
        else {
            res = computeMax(val, mask);
        }
        break;}
    case ibis::BYTE: {
        array_t<signed char> val;
        int ierr = ibis::fileManager::instance().getFile(name, val);
        if (ierr != 0) {
            logWarning("computeMax", "failed to retrieve file %s", name);
        }
        else {
            res = computeMax(val, mask);
        }
        break;}
    case ibis::USHORT: {
        array_t<uint16_t> val;
        int ierr = ibis::fileManager::instance().getFile(name, val);
        if (ierr != 0) {
            logWarning("computeMax", "failed to retrieve file %s", name);
        }
        else {
            res = computeMax(val, mask);
        }
        break;}
    case ibis::SHORT: {
        array_t<int16_t> val;
        int ierr = ibis::fileManager::instance().getFile(name, val);
        if (ierr != 0) {
            logWarning("computeMax", "failed to retrieve file %s", name);
        }
        else {
            res = computeMax(val, mask);
        }
        break;}
    case ibis::UINT: {
        array_t<uint32_t> val;
        int ierr = ibis::fileManager::instance().getFile(name, val);
        if (ierr != 0) {
            logWarning("computeMax", "failed to retrieve file %s", name);
        }
        else {
            res = computeMax(val, mask);
        }
        break;}
    case ibis::INT: {
        array_t<int32_t> val;
        int ierr = ibis::fileManager::instance().getFile(name, val);
        if (ierr != 0) {
            logWarning("computeMax", "failed to retrieve file %s", name);
        }
        else {
            res = computeMax(val, mask);
        }
        break;}
    case ibis::ULONG: {
        array_t<uint64_t> val;
        int ierr = ibis::fileManager::instance().getFile(name, val);
        if (ierr != 0) {
            logWarning("computeMax", "failed to retrieve file %s", name);
        }
        else {
            res = computeMax(val, mask);
        }
        break;}
    case ibis::LONG: {
        array_t<int64_t> val;
        int ierr = ibis::fileManager::instance().getFile(name, val);
        if (ierr != 0) {
            logWarning("computeMax", "failed to retrieve file %s", name);
        }
        else {
            res = computeMax(val, mask);
        }
        break;}
    case ibis::FLOAT: {
        array_t<float> val;
        int ierr = ibis::fileManager::instance().getFile(name, val);
        if (ierr != 0) {
            logWarning("computeMax", "failed to retrieve file %s", name);
        }
        else {
            res = computeMax(val, mask);
        }
        break;}
    case ibis::DOUBLE: {
        array_t<double> val;
        int ierr = ibis::fileManager::instance().getFile(name, val);
        if (ierr != 0) {
            logWarning("computeMax", "failed to retrieve file %s", name);
        }
        else {
            res = computeMax(val, mask);
        }
        break;}
    default:
        LOGGER(ibis::gVerbose > 1)
            << "Warning -- column[" << fullname()
            << "]::computeMax can not work with column type "
            << ibis::TYPESTRING[(int)m_type];
    } // switch(m_type)

    return res;
} // ibis::column::computeMax

double ibis::column::computeSum() const {
    double ret = 0;
    if (thePart->nRows() == 0) return ret;

    ibis::bitvector mask;
    getNullMask(mask);
    if (mask.cnt() == 0) return ret;

    std::string sname;
    const char* name = dataFileName(sname);
    if (name == 0) return ret;

    switch (m_type) {
    case ibis::UBYTE: {
        array_t<unsigned char> val;
        int ierr = ibis::fileManager::instance().getFile(name, val);
        if (ierr != 0) {
            logWarning("computeSum", "failed to retrieve file %s", name);
            ibis::util::setNaN(ret);
        }
        else {
            ret = computeSum(val, mask);
        }
        break;}
    case ibis::BYTE: {
        array_t<signed char> val;
        int ierr = ibis::fileManager::instance().getFile(name, val);
        if (ierr != 0) {
            logWarning("computeSum", "failed to retrieve file %s", name);
            ibis::util::setNaN(ret);
        }
        else {
            ret = computeSum(val, mask);
        }
        break;}
    case ibis::USHORT: {
        array_t<uint16_t> val;
        int ierr = ibis::fileManager::instance().getFile(name, val);
        if (ierr != 0) {
            logWarning("computeSum", "failed to retrieve file %s", name);
            ibis::util::setNaN(ret);
        }
        else {
            ret = computeSum(val, mask);
        }
        break;}
    case ibis::SHORT: {
        array_t<int16_t> val;
        int ierr = ibis::fileManager::instance().getFile(name, val);
        if (ierr != 0) {
            logWarning("computeSum", "failed to retrieve file %s", name);
            ibis::util::setNaN(ret);
        }
        else {
            ret = computeSum(val, mask);
        }
        break;}
    case ibis::UINT: {
        array_t<uint32_t> val;
        int ierr = ibis::fileManager::instance().getFile(name, val);
        if (ierr != 0) {
            logWarning("computeSum", "failed to retrieve file %s", name);
            ibis::util::setNaN(ret);
        }
        else {
            ret = computeSum(val, mask);
        }
        break;}
    case ibis::INT: {
        array_t<int32_t> val;
        int ierr = ibis::fileManager::instance().getFile(name, val);
        if (ierr != 0) {
            logWarning("computeSum", "failed to retrieve file %s", name);
            ibis::util::setNaN(ret);
        }
        else {
            ret = computeSum(val, mask);
        }
        break;}
    case ibis::ULONG: {
        array_t<uint64_t> val;
        int ierr = ibis::fileManager::instance().getFile(name, val);
        if (ierr != 0) {
            logWarning("computeSum", "failed to retrieve file %s", name);
            ibis::util::setNaN(ret);
        }
        else {
            ret = computeSum(val, mask);
        }
        break;}
    case ibis::LONG: {
        array_t<int64_t> val;
        int ierr = ibis::fileManager::instance().getFile(name, val);
        if (ierr != 0) {
            logWarning("computeSum", "failed to retrieve file %s", name);
            ibis::util::setNaN(ret);
        }
        else {
            ret = computeSum(val, mask);
        }
        break;}
    case ibis::FLOAT: {
        array_t<float> val;
        int ierr = ibis::fileManager::instance().getFile(name, val);
        if (ierr != 0) {
            logWarning("computeSum", "failed to retrieve file %s", name);
            ibis::util::setNaN(ret);
        }
        else {
            ret = computeSum(val, mask);
        }
        break;}
    case ibis::DOUBLE: {
        array_t<double> val;
        int ierr = ibis::fileManager::instance().getFile(name, val);
        if (ierr != 0) {
            logWarning("computeSum", "failed to retrieve file %s", name);
            ibis::util::setNaN(ret);
        }
        else {
            ret = computeSum(val, mask);
        }
        break;}
    default:
        LOGGER(ibis::gVerbose > 1)
            << "Warning -- column[" << fullname()
            << "]::computeSum can not work with column type "
            << ibis::TYPESTRING[(int)m_type];
    } // switch(m_type)

    return ret;
} // ibis::column::computeSum

double ibis::column::getActualMin() const {
    double ret;
    indexLock lock(this, "getActualMin");
    if (idx != 0) {
        ret = idx->getMin();
        if (! (ret < 0.0 || ret >= 0.0))
            ret = computeMin();
    }
    else {
        ret = computeMin();
    }
    return ret;
} // ibis::column::getActualMin

double ibis::column::getActualMax() const {
    double ret;
    indexLock lock(this, "getActualMax");
    if (idx != 0) {
        ret = idx->getMax();
        if (! (ret < 0.0 || ret >= 0.0))
            ret = computeMax();
    }
    else {
        ret = computeMax();
    }
    return ret;
} // ibis::column::getActualMax

double ibis::column::getSum() const {
    double ret;
    indexLock lock(this, "getSum");
    if (idx != 0) {
        ret = idx->getSum();
        if (! (ret < 0.0 || ret >= 0.0))
            ret = computeSum();
    }
    else {
        ret = computeSum();
    }
    return ret;
} // ibis::column::getSum

long ibis::column::getCumulativeDistribution
(std::vector<double>& bds, std::vector<uint32_t>& cts) const {
    indexLock lock(this, "getCumulativeDistribution");
    long ierr = -1;
    if (idx != 0) {
        ierr = idx->getCumulativeDistribution(bds, cts);
        if (ierr < 0)
            ierr += -10;
    }
    return ierr;
} // ibis::column::getCumulativeDistribution

long ibis::column::getDistribution
(std::vector<double>& bds, std::vector<uint32_t>& cts) const {
    indexLock lock(this, "getDistribution");
    long ierr = -1;
    if (idx != 0) {
        ierr = idx->getDistribution(bds, cts);
        if (ierr < 0)
            ierr += -10;
    }
    return ierr;
} // ibis::column::getDistribution

/// Has an index been built for this column?  Returns true for yes, false
/// for no.
///
/// @note This function assumes a proper index is available if the index
/// file has more than 20 bytes.  Therefore, this is a guarantee that the
/// index file actually has a properly formatted index.
bool ibis::column::hasIndex() const {
    if (idx != 0) return true;
    std::string idxfile;
    if (dataFileName(idxfile) == 0) return false;
    idxfile += ".idx";
    Stat_T buf;
    if (UnixStat(idxfile.c_str(), &buf) != 0) return false;
    return (buf.st_size > 20);
} // ibis::column::hasIndex

/// Is there a roster list built for this column?  Returns true for yes,
/// false for no.
///
/// @note This function checks to make sure the both .srt and .ind files
/// are present and of the expected sizes.
bool ibis::column::hasRoster() const {
    if (thePart == 0 || thePart->currentDataDir() == 0) return false;
    const unsigned elm = elementSize();
    if (elm == 0) return false;

    std::string fname;
    if (0 == dataFileName(fname)) return false;
    Stat_T buf;
    const unsigned fnlen = fname.size();
    fname += ".srt";
    if (UnixStat(fname.c_str(), &buf) != 0) return false;
    if (buf.st_size != elm * thePart->nRows()) return false;

    fname.erase(fnlen);
    fname += ".ind";
    if (UnixStat(fname.c_str(), &buf) != 0) return false;
    return ((unsigned long)buf.st_size == sizeof(uint32_t) * thePart->nRows());
} // ibis::column::hasRoster

/// Change the flag m_sorted.  If the flag m_sorted is set to true, the
/// caller should have sorted the data file.  Incorrect flag will lead to
/// wrong answers to queries.  This operation invokes a write lock on the
/// column object.
void ibis::column::isSorted(bool iss) {
    writeLock lock(this, "isSorted");
    m_sorted = iss;
} // ibis::column::isSorted

int ibis::column::searchSorted(const ibis::qContinuousRange& rng,
                               ibis::bitvector& hits) const {
    if (rng.leftOperator() == ibis::qExpr::OP_UNDEFINED &&
        rng.rightOperator() == ibis::qExpr::OP_UNDEFINED) {
        getNullMask(hits);
        return hits.sloppyCount();
    }

    std::string dfname;
    LOGGER(dataFileName(dfname) == 0 && ibis::gVerbose > 2)
        << "column[" << fullname() << "]::searchSorted(" << rng
        << ") failed to determine the data file name";

    int ierr;
    switch (m_type) {
    case ibis::BYTE: {
        array_t<signed char> vals;
        if (! dfname.empty()) {
            ierr = ibis::fileManager::instance().getFile(dfname.c_str(), vals);
            if (ierr == 0) {
                ierr = searchSortedICC(vals, rng, hits);
            }
            else {
                ierr = searchSortedOOCC<signed char>(dfname.c_str(), rng, hits);
            }
        }
        else {
            ierr = getValuesArray(&vals);
            if (ierr == 0)
                ierr = searchSortedICC(vals, rng, hits);
        }
        break;}
    case ibis::UBYTE: {
        array_t<unsigned char> vals;
        if (! dfname.empty()) {
            ierr = ibis::fileManager::instance().getFile(dfname.c_str(), vals);
            if (ierr == 0) {
                ierr = searchSortedICC(vals, rng, hits);
            }
            else {
                ierr = searchSortedOOCC<unsigned char>
                    (dfname.c_str(), rng, hits);
            }
        }
        else {
            ierr = getValuesArray(&vals);
            if (ierr == 0)
                ierr = searchSortedICC(vals, rng, hits);
        }
        break;}
    case ibis::SHORT: {
        array_t<int16_t> vals;
        if (! dfname.empty()) {
            ierr = ibis::fileManager::instance().getFile(dfname.c_str(), vals);
            if (ierr == 0) {
                ierr = searchSortedICC(vals, rng, hits);
            }
            else {
                ierr = searchSortedOOCC<int16_t>(dfname.c_str(), rng, hits);
            }
        }
        else {
            ierr = getValuesArray(&vals);
            if (ierr == 0)
                ierr = searchSortedICC(vals, rng, hits);
        }
        break;}
    case ibis::USHORT: {
        array_t<uint16_t> vals;
        if (! dfname.empty()) {
            ierr = ibis::fileManager::instance().getFile(dfname.c_str(), vals);
            if (ierr == 0) {
                ierr = searchSortedICC(vals, rng, hits);
            }
            else {
                ierr = searchSortedOOCC<uint16_t>(dfname.c_str(), rng, hits);
            }
        }
        else {
            ierr = getValuesArray(&vals);
            if (ierr == 0)
                ierr = searchSortedICC(vals, rng, hits);
        }
        break;}
    case ibis::INT: {
        array_t<int32_t> vals;
        if (! dfname.c_str()) {
            ierr = ibis::fileManager::instance().getFile(dfname.c_str(), vals);
            if (ierr == 0) {
                ierr = searchSortedICC(vals, rng, hits);
            }
            else {
                ierr = searchSortedOOCC<int32_t>(dfname.c_str(), rng, hits);
            }
        }
        else {
            ierr = getValuesArray(&vals);
            if (ierr == 0)
                ierr = searchSortedICC(vals, rng, hits);
        }
        break;}
    case ibis::UINT: {
        array_t<uint32_t> vals;
        if (! dfname.c_str()) {
            ierr = ibis::fileManager::instance().getFile(dfname.c_str(), vals);
            if (ierr == 0) {
                ierr = searchSortedICC(vals, rng, hits);
            }
            else {
                ierr = searchSortedOOCC<uint32_t>(dfname.c_str(), rng, hits);
            }
        }
        else {
            ierr = getValuesArray(&vals);
            if (ierr == 0)
                ierr = searchSortedICC(vals, rng, hits);
        }
        break;}
    case ibis::LONG: {
        array_t<int64_t> vals;
        if (! dfname.c_str()) {
            ierr = ibis::fileManager::instance().getFile(dfname.c_str(), vals);
            if (ierr == 0) {
                ierr = searchSortedICC(vals, rng, hits);
            }
            else {
                ierr = searchSortedOOCC<int64_t>(dfname.c_str(), rng, hits);
            }
        }
        else {
            ierr = getValuesArray(&vals);
            if (ierr == 0)
                ierr = searchSortedICC(vals, rng, hits);
        }
        break;}
    case ibis::ULONG: {
        array_t<uint64_t> vals;
        if (! dfname.c_str()) {
            ierr = ibis::fileManager::instance().getFile(dfname.c_str(), vals);
            if (ierr == 0) {
                ierr = searchSortedICC(vals, rng, hits);
            }
            else {
                ierr = searchSortedOOCC<uint64_t>(dfname.c_str(), rng, hits);
            }
        }
        else {
            ierr = getValuesArray(&vals);
            if (ierr == 0)
                ierr = searchSortedICC(vals, rng, hits);
        }
        break;}
    case ibis::FLOAT: {
        array_t<float> vals;
        if (! dfname.c_str()) {
            ierr = ibis::fileManager::instance().getFile(dfname.c_str(), vals);
            if (ierr == 0) {
                ierr = searchSortedICC(vals, rng, hits);
            }
            else {
                ierr = searchSortedOOCC<float>(dfname.c_str(), rng, hits);
            }
        }
        else {
            ierr = getValuesArray(&vals);
            if (ierr == 0)
                ierr = searchSortedICC(vals, rng, hits);
        }
        break;}
    case ibis::DOUBLE: {
        array_t<double> vals;
        if (! dfname.c_str()) {
            ierr = ibis::fileManager::instance().getFile(dfname.c_str(), vals);
            if (ierr == 0) {
                ierr = searchSortedICC(vals, rng, hits);
            }
            else {
                ierr = searchSortedOOCC<double>(dfname.c_str(), rng, hits);
            }
        }
        else {
            ierr = getValuesArray(&vals);
            if (ierr == 0)
                ierr = searchSortedICC(vals, rng, hits);
        }
        break;}
    default: {
        LOGGER(ibis::gVerbose > 1)
            << "Warning -- column[" << fullname() << "]::searchSorted(" << rng
            << ") does not yet support column type "
            << ibis::TYPESTRING[(int)m_type];
        ierr = -5;
        break;}
    } // switch (m_type)
    return (ierr < 0 ? ierr : 0);
} // ibis::column::searchSorted

int ibis::column::searchSorted(const ibis::qDiscreteRange& rng,
                               ibis::bitvector& hits) const {
    std::string dfname;
    LOGGER(dataFileName(dfname) == 0 && ibis::gVerbose > 2)
        << "column[" << fullname() << "]::searchSorted(" << rng.colName()
        << "IN ...) failed to determine the data file name";

    int ierr;
    switch (m_type) {
    case ibis::BYTE: {
        array_t<signed char> vals;
        if (! dfname.empty()) {
            ierr = ibis::fileManager::instance().getFile(dfname.c_str(), vals);
            if (ierr == 0) {
                ierr = searchSortedICD(vals, rng, hits);
            }
            else {
                ierr = searchSortedOOCD<signed char>(dfname.c_str(), rng, hits);
            }
        }
        else {
            ierr = getValuesArray(&vals);
            if (ierr == 0)
                ierr = searchSortedICD(vals, rng, hits);
        }
        break;}
    case ibis::UBYTE: {
        array_t<unsigned char> vals;
        if (! dfname.empty()) {
            ierr = ibis::fileManager::instance().getFile(dfname.c_str(), vals);
            if (ierr == 0) {
                ierr = searchSortedICD(vals, rng, hits);
            }
            else {
                ierr = searchSortedOOCD<unsigned char>
                    (dfname.c_str(), rng, hits);
            }
        }
        else {
            ierr = getValuesArray(&vals);
            if (ierr == 0)
                ierr = searchSortedICD(vals, rng, hits);
        }
        break;}
    case ibis::SHORT: {
        array_t<int16_t> vals;
        if (! dfname.empty()) {
            ierr = ibis::fileManager::instance().getFile(dfname.c_str(), vals);
            if (ierr == 0) {
                ierr = searchSortedICD(vals, rng, hits);
            }
            else {
                ierr = searchSortedOOCD<int16_t>(dfname.c_str(), rng, hits);
            }
        }
        else {
            ierr = getValuesArray(&vals);
            if (ierr == 0)
                ierr = searchSortedICD(vals, rng, hits);
        }
        break;}
    case ibis::USHORT: {
        array_t<uint16_t> vals;
        if (! dfname.empty()) {
            ierr = ibis::fileManager::instance().getFile(dfname.c_str(), vals);
            if (ierr == 0) {
                ierr = searchSortedICD(vals, rng, hits);
            }
            else {
                ierr = searchSortedOOCD<uint16_t>(dfname.c_str(), rng, hits);
            }
        }
        else {
            ierr = getValuesArray(&vals);
            if (ierr == 0)
                ierr = searchSortedICD(vals, rng, hits);
        }
        break;}
    case ibis::INT: {
        array_t<int32_t> vals;
        if (! dfname.empty()) {
            ierr = ibis::fileManager::instance().getFile(dfname.c_str(), vals);
            if (ierr == 0) {
                ierr = searchSortedICD(vals, rng, hits);
            }
            else {
                ierr = searchSortedOOCD<int32_t>(dfname.c_str(), rng, hits);
            }
        }
        else {
            ierr = getValuesArray(&vals);
            if (ierr == 0)
                ierr = searchSortedICD(vals, rng, hits);
        }
        break;}
    case ibis::UINT: {
        array_t<uint32_t> vals;
        if (! dfname.empty()) {
            ierr = ibis::fileManager::instance().getFile(dfname.c_str(), vals);
            if (ierr == 0) {
                ierr = searchSortedICD(vals, rng, hits);
            }
            else {
                ierr = searchSortedOOCD<uint32_t>(dfname.c_str(), rng, hits);
            }
        }
        else {
            ierr = getValuesArray(&vals);
            if (ierr == 0)
                ierr = searchSortedICD(vals, rng, hits);
        }
        break;}
    case ibis::LONG: {
        array_t<int64_t> vals;
        if (! dfname.empty()) {
            ierr = ibis::fileManager::instance().getFile(dfname.c_str(), vals);
            if (ierr == 0) {
                ierr = searchSortedICD(vals, rng, hits);
            }
            else {
                ierr = searchSortedOOCD<int64_t>(dfname.c_str(), rng, hits);
            }
        }
        else {
            ierr = getValuesArray(&vals);
            if (ierr == 0)
                ierr = searchSortedICD(vals, rng, hits);
        }
        break;}
    case ibis::ULONG: {
        array_t<uint64_t> vals;
        if (! dfname.empty()) {
            ierr = ibis::fileManager::instance().getFile(dfname.c_str(), vals);
            if (ierr == 0) {
                ierr = searchSortedICD(vals, rng, hits);
            }
            else {
                ierr = searchSortedOOCD<uint64_t>(dfname.c_str(), rng, hits);
            }
        }
        else {
            ierr = getValuesArray(&vals);
            if (ierr == 0)
                ierr = searchSortedICD(vals, rng, hits);
        }
        break;}
    case ibis::FLOAT: {
        array_t<float> vals;
        if (! dfname.empty()) {
            ierr = ibis::fileManager::instance().getFile(dfname.c_str(), vals);
            if (ierr == 0) {
                ierr = searchSortedICD(vals, rng, hits);
            }
            else {
                ierr = searchSortedOOCD<float>(dfname.c_str(), rng, hits);
            }
        }
        else {
            ierr = getValuesArray(&vals);
            if (ierr == 0)
                ierr = searchSortedICD(vals, rng, hits);
        }
        break;}
    case ibis::DOUBLE: {
        array_t<double> vals;
        if (! dfname.empty()) {
            ierr = ibis::fileManager::instance().getFile(dfname.c_str(), vals);
            if (ierr == 0) {
                ierr = searchSortedICD(vals, rng, hits);
            }
            else {
                ierr = searchSortedOOCD<double>(dfname.c_str(), rng, hits);
            }
        }
        else {
            ierr = getValuesArray(&vals);
            if (ierr == 0)
                ierr = searchSortedICD(vals, rng, hits);
        }
        break;}
    default: {
        LOGGER(ibis::gVerbose > 1)
            << "Warning -- column[" << fullname() << "]::searchSorted("
            << rng.colName() << " IN ...) "
            << "does not yet support column type "
            << ibis::TYPESTRING[(int)m_type];
        ierr = -5;
        break;}
    } // switch (m_type)
    return (ierr < 0 ? ierr : 0);
} // ibis::column::searchSorted

int ibis::column::searchSorted(const ibis::qIntHod& rng,
                               ibis::bitvector& hits) const {
    std::string dfname;
    LOGGER(dataFileName(dfname) == 0 && ibis::gVerbose > 2)
        << "column[" << fullname() << "]::searchSorted(" << rng.colName()
        << "IN ...) failed to determine the data file name";

    int ierr;
    switch (m_type) {
    case ibis::BYTE: {
        array_t<signed char> vals;
        if (! dfname.empty()) {
            ierr = ibis::fileManager::instance().getFile(dfname.c_str(), vals);
            if (ierr == 0) {
                ierr = searchSortedICD(vals, rng, hits);
            }
            else {
                ierr = searchSortedOOCD<signed char>(dfname.c_str(), rng, hits);
            }
        }
        else {
            ierr = getValuesArray(&vals);
            if (ierr == 0)
                ierr = searchSortedICD(vals, rng, hits);
        }
        break;}
    case ibis::UBYTE: {
        array_t<unsigned char> vals;
        if (! dfname.empty()) {
            ierr = ibis::fileManager::instance().getFile(dfname.c_str(), vals);
            if (ierr == 0) {
                ierr = searchSortedICD(vals, rng, hits);
            }
            else {
                ierr = searchSortedOOCD<unsigned char>
                    (dfname.c_str(), rng, hits);
            }
        }
        else {
            ierr = getValuesArray(&vals);
            if (ierr == 0)
                ierr = searchSortedICD(vals, rng, hits);
        }
        break;}
    case ibis::SHORT: {
        array_t<int16_t> vals;
        if (! dfname.empty()) {
            ierr = ibis::fileManager::instance().getFile(dfname.c_str(), vals);
            if (ierr == 0) {
                ierr = searchSortedICD(vals, rng, hits);
            }
            else {
                ierr = searchSortedOOCD<int16_t>(dfname.c_str(), rng, hits);
            }
        }
        else {
            ierr = getValuesArray(&vals);
            if (ierr == 0)
                ierr = searchSortedICD(vals, rng, hits);
        }
        break;}
    case ibis::USHORT: {
        array_t<uint16_t> vals;
        if (! dfname.empty()) {
            ierr = ibis::fileManager::instance().getFile(dfname.c_str(), vals);
            if (ierr == 0) {
                ierr = searchSortedICD(vals, rng, hits);
            }
            else {
                ierr = searchSortedOOCD<uint16_t>(dfname.c_str(), rng, hits);
            }
        }
        else {
            ierr = getValuesArray(&vals);
            if (ierr == 0)
                ierr = searchSortedICD(vals, rng, hits);
        }
        break;}
    case ibis::INT: {
        array_t<int32_t> vals;
        if (! dfname.empty()) {
            ierr = ibis::fileManager::instance().getFile(dfname.c_str(), vals);
            if (ierr == 0) {
                ierr = searchSortedICD(vals, rng, hits);
            }
            else {
                ierr = searchSortedOOCD<int32_t>(dfname.c_str(), rng, hits);
            }
        }
        else {
            ierr = getValuesArray(&vals);
            if (ierr == 0)
                ierr = searchSortedICD(vals, rng, hits);
        }
        break;}
    case ibis::UINT: {
        array_t<uint32_t> vals;
        if (! dfname.empty()) {
            ierr = ibis::fileManager::instance().getFile(dfname.c_str(), vals);
            if (ierr == 0) {
                ierr = searchSortedICD(vals, rng, hits);
            }
            else {
                ierr = searchSortedOOCD<uint32_t>(dfname.c_str(), rng, hits);
            }
        }
        else {
            ierr = getValuesArray(&vals);
            if (ierr == 0)
                ierr = searchSortedICD(vals, rng, hits);
        }
        break;}
    case ibis::LONG: {
        array_t<int64_t> vals;
        if (! dfname.empty()) {
            ierr = ibis::fileManager::instance().getFile(dfname.c_str(), vals);
            if (ierr == 0) {
                ierr = searchSortedICD(vals, rng, hits);
            }
            else {
                ierr = searchSortedOOCD<int64_t>(dfname.c_str(), rng, hits);
            }
        }
        else {
            ierr = getValuesArray(&vals);
            if (ierr == 0)
                ierr = searchSortedICD(vals, rng, hits);
        }
        break;}
    case ibis::ULONG: {
        array_t<uint64_t> vals;
        if (! dfname.empty()) {
            ierr = ibis::fileManager::instance().getFile(dfname.c_str(), vals);
            if (ierr == 0) {
                ierr = searchSortedICD(vals, rng, hits);
            }
            else {
                ierr = searchSortedOOCD<uint64_t>(dfname.c_str(), rng, hits);
            }
        }
        else {
            ierr = getValuesArray(&vals);
            if (ierr == 0)
                ierr = searchSortedICD(vals, rng, hits);
        }
        break;}
    case ibis::FLOAT: {
        array_t<float> vals;
        if (! dfname.empty()) {
            ierr = ibis::fileManager::instance().getFile(dfname.c_str(), vals);
            if (ierr == 0) {
                ierr = searchSortedICD(vals, rng, hits);
            }
            else {
                ierr = searchSortedOOCD<float>(dfname.c_str(), rng, hits);
            }
        }
        else {
            ierr = getValuesArray(&vals);
            if (ierr == 0)
                ierr = searchSortedICD(vals, rng, hits);
        }
        break;}
    case ibis::DOUBLE: {
        array_t<double> vals;
        if (! dfname.empty()) {
            ierr = ibis::fileManager::instance().getFile(dfname.c_str(), vals);
            if (ierr == 0) {
                ierr = searchSortedICD(vals, rng, hits);
            }
            else {
                ierr = searchSortedOOCD<double>(dfname.c_str(), rng, hits);
            }
        }
        else {
            ierr = getValuesArray(&vals);
            if (ierr == 0)
                ierr = searchSortedICD(vals, rng, hits);
        }
        break;}
    default: {
        LOGGER(ibis::gVerbose > 1)
            << "Warning -- column[" << fullname() << "]::searchSorted("
            << rng.colName() << " IN ...) does not yet support column type "
            << ibis::TYPESTRING[(int)m_type];
        ierr = -5;
        break;}
    } // switch (m_type)
    return (ierr < 0 ? ierr : 0);
} // ibis::column::searchSorted

int ibis::column::searchSorted(const ibis::qUIntHod& rng,
                               ibis::bitvector& hits) const {
    std::string dfname;
    LOGGER(dataFileName(dfname) == 0 && ibis::gVerbose > 2)
        << "column[" << fullname() << "]::searchSorted(" << rng.colName()
        << "IN ...) failed to determine the data file name";

    int ierr;
    switch (m_type) {
    case ibis::BYTE: {
        array_t<signed char> vals;
        if (! dfname.empty()) {
            ierr = ibis::fileManager::instance().getFile(dfname.c_str(), vals);
            if (ierr == 0) {
                ierr = searchSortedICD(vals, rng, hits);
            }
            else {
                ierr = searchSortedOOCD<signed char>(dfname.c_str(), rng, hits);
            }
        }
        else {
            ierr = getValuesArray(&vals);
            if (ierr == 0)
                ierr = searchSortedICD(vals, rng, hits);
        }
        break;}
    case ibis::UBYTE: {
        array_t<unsigned char> vals;
        if (! dfname.empty()) {
            ierr = ibis::fileManager::instance().getFile(dfname.c_str(), vals);
            if (ierr == 0) {
                ierr = searchSortedICD(vals, rng, hits);
            }
            else {
                ierr = searchSortedOOCD<unsigned char>
                    (dfname.c_str(), rng, hits);
            }
        }
        else {
            ierr = getValuesArray(&vals);
            if (ierr == 0)
                ierr = searchSortedICD(vals, rng, hits);
        }
        break;}
    case ibis::SHORT: {
        array_t<int16_t> vals;
        if (! dfname.empty()) {
            ierr = ibis::fileManager::instance().getFile(dfname.c_str(), vals);
            if (ierr == 0) {
                ierr = searchSortedICD(vals, rng, hits);
            }
            else {
                ierr = searchSortedOOCD<int16_t>(dfname.c_str(), rng, hits);
            }
        }
        else {
            ierr = getValuesArray(&vals);
            if (ierr == 0)
                ierr = searchSortedICD(vals, rng, hits);
        }
        break;}
    case ibis::USHORT: {
        array_t<uint16_t> vals;
        if (! dfname.empty()) {
            ierr = ibis::fileManager::instance().getFile(dfname.c_str(), vals);
            if (ierr == 0) {
                ierr = searchSortedICD(vals, rng, hits);
            }
            else {
                ierr = searchSortedOOCD<uint16_t>(dfname.c_str(), rng, hits);
            }
        }
        else {
            ierr = getValuesArray(&vals);
            if (ierr == 0)
                ierr = searchSortedICD(vals, rng, hits);
        }
        break;}
    case ibis::INT: {
        array_t<int32_t> vals;
        if (! dfname.empty()) {
            ierr = ibis::fileManager::instance().getFile(dfname.c_str(), vals);
            if (ierr == 0) {
                ierr = searchSortedICD(vals, rng, hits);
            }
            else {
                ierr = searchSortedOOCD<int32_t>(dfname.c_str(), rng, hits);
            }
        }
        else {
            ierr = getValuesArray(&vals);
            if (ierr == 0)
                ierr = searchSortedICD(vals, rng, hits);
        }
        break;}
    case ibis::UINT: {
        array_t<uint32_t> vals;
        if (! dfname.empty()) {
            ierr = ibis::fileManager::instance().getFile(dfname.c_str(), vals);
            if (ierr == 0) {
                ierr = searchSortedICD(vals, rng, hits);
            }
            else {
                ierr = searchSortedOOCD<uint32_t>(dfname.c_str(), rng, hits);
            }
        }
        else {
            ierr = getValuesArray(&vals);
            if (ierr == 0)
                ierr = searchSortedICD(vals, rng, hits);
        }
        break;}
    case ibis::LONG: {
        array_t<int64_t> vals;
        if (! dfname.empty()) {
            ierr = ibis::fileManager::instance().getFile(dfname.c_str(), vals);
            if (ierr == 0) {
                ierr = searchSortedICD(vals, rng, hits);
            }
            else {
                ierr = searchSortedOOCD<int64_t>(dfname.c_str(), rng, hits);
            }
        }
        else {
            ierr = getValuesArray(&vals);
            if (ierr == 0)
                ierr = searchSortedICD(vals, rng, hits);
        }
        break;}
    case ibis::ULONG: {
        array_t<uint64_t> vals;
        if (! dfname.empty()) {
            ierr = ibis::fileManager::instance().getFile(dfname.c_str(), vals);
            if (ierr == 0) {
                ierr = searchSortedICD(vals, rng, hits);
            }
            else {
                ierr = searchSortedOOCD<uint64_t>(dfname.c_str(), rng, hits);
            }
        }
        else {
            ierr = getValuesArray(&vals);
            if (ierr == 0)
                ierr = searchSortedICD(vals, rng, hits);
        }
        break;}
    case ibis::FLOAT: {
        array_t<float> vals;
        if (! dfname.empty()) {
            ierr = ibis::fileManager::instance().getFile(dfname.c_str(), vals);
            if (ierr == 0) {
                ierr = searchSortedICD(vals, rng, hits);
            }
            else {
                ierr = searchSortedOOCD<float>(dfname.c_str(), rng, hits);
            }
        }
        else {
            ierr = getValuesArray(&vals);
            if (ierr == 0)
                ierr = searchSortedICD(vals, rng, hits);
        }
        break;}
    case ibis::DOUBLE: {
        array_t<double> vals;
        if (! dfname.empty()) {
            ierr = ibis::fileManager::instance().getFile(dfname.c_str(), vals);
            if (ierr == 0) {
                ierr = searchSortedICD(vals, rng, hits);
            }
            else {
                ierr = searchSortedOOCD<double>(dfname.c_str(), rng, hits);
            }
        }
        else {
            ierr = getValuesArray(&vals);
            if (ierr == 0)
                ierr = searchSortedICD(vals, rng, hits);
        }
        break;}
    default: {
        LOGGER(ibis::gVerbose > 1)
            << "Warning -- column[" << fullname() << "]::searchSorted("
            << rng.colName() << " IN ...) does not yet support column type "
            << ibis::TYPESTRING[(int)m_type];
        ierr = -5;
        break;}
    } // switch (m_type)
    return (ierr < 0 ? ierr : 0);
} // ibis::column::searchSorted

template<typename T>
int ibis::column::searchSortedICC(const array_t<T>& vals,
                                  const ibis::qContinuousRange& rng,
                                  ibis::bitvector& hits) const {
    if (rng.leftOperator() == ibis::qExpr::OP_UNDEFINED &&
        rng.rightOperator() == ibis::qExpr::OP_UNDEFINED) {
        getNullMask(hits);
        return hits.sloppyCount();
    }

    hits.clear();
    uint32_t iloc, jloc;
    T ival = (rng.leftOperator() == ibis::qExpr::OP_UNDEFINED ? 0 :
              static_cast<T>(rng.leftBound()));
    if (rng.leftOperator() == ibis::qExpr::OP_LE ||
        rng.leftOperator() == ibis::qExpr::OP_GT)
        ibis::util::round_up(rng.leftBound(), ival);
    T jval = (rng.rightOperator() == ibis::qExpr::OP_UNDEFINED ? 0 :
              static_cast<T>(rng.rightBound()));
    if (rng.rightOperator() == ibis::qExpr::OP_GE ||
        rng.rightOperator() == ibis::qExpr::OP_LT)
        ibis::util::round_up(rng.rightBound(), jval);

    switch (rng.leftOperator()) {
    case ibis::qExpr::OP_LT: {
        switch (rng.rightOperator()) {
        case ibis::qExpr::OP_LT: {
            if (ival < jval) {
                iloc = vals.find_upper(ival);
                jloc = vals.find(jval);
                if (iloc < jloc) {
                    hits.set(0, iloc);
                    hits.adjustSize(jloc, vals.size());
                }
            }
            else {
                hits.set(0, vals.size());
            }
            break;}
        case ibis::qExpr::OP_LE: {
            if (ival < jval) {
                iloc = vals.find_upper(ival);
                jloc = vals.find_upper(jval);
                if (iloc < jloc) {
                    hits.set(0, iloc);
                    hits.adjustSize(jloc, vals.size());
                }
            }
            else {
                hits.set(0, vals.size());
            }
            break;}
        case ibis::qExpr::OP_GT: {
            if (ival >= jval) {
                iloc = vals.find_upper(ival);
                if (iloc < vals.size()) {
                    hits.appendFill(0, iloc);
                    hits.adjustSize(vals.size(), vals.size());
                }
                else {
                    hits.set(0, vals.size());
                }
            }
            else {
                iloc = vals.find_upper(jval);
                if (iloc < vals.size()) {
                    hits.set(0, iloc);
                    hits.adjustSize(vals.size(), vals.size());
                }
                else {
                    hits.set(0, vals.size());
                }
            }
            break;}
        case ibis::qExpr::OP_GE: {
            if (ival >= jval) {
                iloc = vals.find_upper(ival);
                if (iloc < vals.size()) {
                    hits.set(0, iloc);
                    hits.adjustSize(vals.size(), vals.size());
                }
                else {
                    hits.set(0, vals.size());
                }
            }
            else {
                iloc = vals.find(jval);
                if (iloc < vals.size()) {
                    hits.set(0, iloc);
                    hits.adjustSize(vals.size(), vals.size());
                }
                else {
                    hits.set(0, vals.size());
                }
            }
            break;}
        case ibis::qExpr::OP_EQ: {
            if (rng.rightBound() > rng.leftBound()) {
                iloc = vals.find(jval);
                if (iloc < vals.size() && vals[iloc] == rng.rightBound()) {
                    for (jloc = iloc+1;
                         jloc < vals.size() && vals[jloc] == vals[iloc];
                         ++ jloc);
                    hits.set(0, iloc);
                    hits.adjustSize(jloc, vals.size());
                }
                else {
                    hits.set(0, vals.size());
                }
            }
            else {
                hits.set(0, vals.size());
            }
            break;}
        case ibis::qExpr::OP_UNDEFINED:
        default: {
            iloc = vals.find_upper(ival);
            if (iloc < vals.size()) {
                hits.set(0, iloc);
                hits.adjustSize(vals.size(), vals.size());
            }
            else {
                hits.set(0, vals.size());
            }
            break;}
        } // switch (rng.rightOperator())
        break;}
    case ibis::qExpr::OP_LE: {
        switch (rng.rightOperator()) {
        case ibis::qExpr::OP_LT: {
            if (ival < jval) {
                iloc = vals.find(ival);
                jloc = vals.find(jval);
                if (iloc < jloc) {
                    hits.set(0, iloc);
                    hits.adjustSize(jloc, vals.size());
                }
            }
            else {
                hits.set(0, vals.size());
            }
            break;}
        case ibis::qExpr::OP_LE: {
            if (ival <= jval) {
                iloc = vals.find(ival);
                jloc = vals.find_upper(jval);
                if (iloc < jloc) {
                    hits.set(0, iloc);
                    hits.adjustSize(jloc, vals.size());
                }
            }
            else {
                hits.set(0, vals.size());
            }
            break;}
        case ibis::qExpr::OP_GT: {
            if (ival > jval) {
                iloc = vals.find(ival);
                if (iloc < vals.size()) {
                    hits.appendFill(0, iloc);
                    hits.adjustSize(vals.size(), vals.size());
                }
                else {
                    hits.set(0, vals.size());
                }
            }
            else {
                iloc = vals.find_upper(jval);
                if (iloc < vals.size()) {
                    hits.set(0, iloc);
                    hits.adjustSize(vals.size(), vals.size());
                }
                else {
                    hits.set(0, vals.size());
                }
            }
            break;}
        case ibis::qExpr::OP_GE: {
            if (ival >= jval) {
                iloc = vals.find(ival);
                if (iloc < vals.size()) {
                    hits.set(0, iloc);
                    hits.adjustSize(vals.size(), vals.size());
                }
                else {
                    hits.set(0, vals.size());
                }
            }
            else {
                iloc = vals.find(jval);
                if (iloc < vals.size()) {
                    hits.set(0, iloc);
                    hits.adjustSize(vals.size(), vals.size());
                }
                else {
                    hits.set(0, vals.size());
                }
            }
            break;}
        case ibis::qExpr::OP_EQ: {
            if (rng.rightBound() >= rng.leftBound()) {
                iloc = vals.find(jval);
                if (iloc < vals.size() && vals[iloc] == rng.rightBound()) {
                    for (jloc = iloc+1;
                         jloc < vals.size() && vals[jloc] == vals[iloc];
                         ++ jloc);
                    hits.set(0, iloc);
                    hits.adjustSize(jloc, vals.size());
                }
                else {
                    hits.set(0, vals.size());
                }
            }
            else {
                hits.set(0, vals.size());
            }
            break;}
        case ibis::qExpr::OP_UNDEFINED:
        default: {
            iloc = vals.find(ival);
            if (iloc < vals.size()) {
                hits.set(0, iloc);
                hits.adjustSize(vals.size(), vals.size());
            }
            else {
                hits.set(0, vals.size());
            }
            break;}
        } // switch (rng.rightOperator())
        break;}
    case ibis::qExpr::OP_GT: {
        switch (rng.rightOperator()) {
        case ibis::qExpr::OP_LT: {
            if (ival <= jval) {
                iloc = vals.find(ival);
                if (iloc > 0) {
                    hits.adjustSize(iloc, vals.size());
                }
                else {
                    hits.set(0, vals.size());
                }
            }
            else {
                iloc = vals.find(jval);
                if (iloc > 0) {
                    hits.adjustSize(iloc, vals.size());
                }
                else {
                    hits.set(0, vals.size());
                }
            }
            break;}
        case ibis::qExpr::OP_LE: {
            if (ival < jval) {
                iloc = vals.find(ival);
                if (iloc > 0) {
                    hits.adjustSize(iloc, vals.size());
                }
                else {
                    hits.set(0, vals.size());
                }
            }
            else {
                iloc = vals.find_upper(jval);
                if (iloc > 0) {
                    hits.adjustSize(iloc, vals.size());
                }
                else {
                    hits.set(0, vals.size());
                }
            }
            break;}
        case ibis::qExpr::OP_GT: {
            if (jval < ival) {
                iloc = vals.find_upper(jval);
                jloc = vals.find(ival);
                if (jloc > iloc) {
                    hits.set(0, iloc);
                    hits.adjustSize(jloc, vals.size());
                }
                else {
                    hits.set(0, vals.size());
                }
            }
            else {
                hits.set(0, vals.size());
            }
            break;}
        case ibis::qExpr::OP_GE: {
            if (jval < ival) {
                iloc = vals.find(jval);
                jloc = vals.find(ival);
                if (jloc > iloc) {
                    hits.set(0, iloc);
                    hits.adjustSize(jloc, vals.size());
                }
                else {
                    hits.set(0, vals.size());
                }
            }
            else {
                hits.set(0, vals.size());
            }
            break;}
        case ibis::qExpr::OP_EQ: {
            if (rng.rightBound() > rng.leftBound()) {
                iloc = vals.find(jval);
                if (iloc < vals.size() && vals[iloc] == rng.rightBound()) {
                    jloc = vals.find_upper(jval);
                    hits.set(0, iloc);
                    hits.adjustSize(jloc, vals.size());
                }
                else {
                    hits.set(0, vals.size());
                }
            }
            else {
                hits.set(0, vals.size());
            }
            break;}
        case ibis::qExpr::OP_UNDEFINED:
        default: {
            iloc = vals.find(ival);
            hits.adjustSize(iloc, vals.size());
            break;}
        } // switch (rng.rightOperator())
        break;}
    case ibis::qExpr::OP_GE: {
        switch (rng.rightOperator()) {
        case ibis::qExpr::OP_LT: {
            if (ival < jval) {
                iloc = vals.find_upper(ival);
                if (iloc > 0) {
                    hits.adjustSize(iloc, vals.size());
                }
                else {
                    hits.set(0, vals.size());
                }
            }
            else {
                iloc = vals.find(jval);
                if (iloc > 0) {
                    hits.adjustSize(iloc, vals.size());
                }
                else {
                    hits.set(0, vals.size());
                }
            }
            break;}
        case ibis::qExpr::OP_LE: {
            if (ival <= jval) {
                iloc = vals.find_upper(ival);
                if (iloc > 0) {
                    hits.adjustSize(iloc, vals.size());
                }
                else {
                    hits.set(0, vals.size());
                }
            }
            else {
                iloc = vals.find_upper(jval);
                if (iloc > 0) {
                    hits.adjustSize(iloc, vals.size());
                }
                else {
                    hits.set(0, vals.size());
                }
            }
            break;}
        case ibis::qExpr::OP_GT: {
            if (jval < ival) {
                iloc = vals.find_upper(jval);
                jloc = vals.find_upper(ival);
                if (jloc > iloc) {
                    hits.set(0, iloc);
                    hits.adjustSize(jloc, vals.size());
                }
                else {
                    hits.set(0, vals.size());
                }
            }
            else {
                hits.set(0, vals.size());
            }
            break;}
        case ibis::qExpr::OP_GE: {
            if (jval <= ival) {
                iloc = vals.find(jval);
                jloc = vals.find_upper(ival);
                if (jloc > iloc) {
                    hits.set(0, iloc);
                    hits.adjustSize(jloc, vals.size());
                }
                else {
                    hits.set(0, vals.size());
                }
            }
            else {
                hits.set(0, vals.size());
            }
            break;}
        case ibis::qExpr::OP_EQ: {
            if (rng.rightBound() >= rng.leftBound()) {
                iloc = vals.find(jval);
                if (iloc < vals.size() && vals[iloc] == rng.rightBound()) {
                    jloc = vals.find_upper(jval);
                    hits.set(0, iloc);
                    hits.adjustSize(jloc, vals.size());
                }
                else {
                    hits.set(0, vals.size());
                }
            }
            else {
                hits.set(0, vals.size());
            }
            break;}
        case ibis::qExpr::OP_UNDEFINED:
        default: {
            iloc = vals.find_upper(ival);
            hits.adjustSize(iloc, vals.size());
            break;}
        } // switch (rng.rightOperator())
        break;}
    case ibis::qExpr::OP_EQ: {
        switch (rng.rightOperator()) {
        case ibis::qExpr::OP_LT: {
            if (rng.leftBound() < rng.rightBound()) {
                iloc = vals.find(ival);
                if (iloc < vals.size() && vals[iloc] == rng.leftBound()) {
                    jloc = vals.find_upper(ival);
                    hits.set(0, iloc);
                    hits.adjustSize(jloc, vals.size());
                }
                else {
                    hits.set(0, vals.size());
                }
            }
            else {
                hits.set(0, vals.size());
            }
            break;}
        case ibis::qExpr::OP_LE: {
            if (rng.leftBound() <= rng.rightBound()) {
                iloc = vals.find(ival);
                if (iloc < vals.size() && vals[iloc] == rng.leftBound()) {
                    jloc = vals.find_upper(ival);
                    hits.set(0, iloc);
                    hits.adjustSize(jloc, vals.size());
                }
                else {
                    hits.set(0, vals.size());
                }
            }
            else {
                hits.set(0, vals.size());
            }
            break;}
        case ibis::qExpr::OP_GT: {
            if (rng.leftBound() > rng.rightBound()) {
                iloc = vals.find(ival);
                if (iloc < vals.size() && vals[iloc] == rng.leftBound()) {
                    jloc = vals.find_upper(ival);
                    hits.set(0, iloc);
                    hits.adjustSize(jloc, vals.size());
                }
                else {
                    hits.set(0, vals.size());
                }
            }
            else {
                hits.set(0, vals.size());
            }
            break;}
        case ibis::qExpr::OP_GE: {
            if (rng.leftBound() >= rng.rightBound()) {
                iloc = vals.find(ival);
                if (iloc < vals.size() && vals[iloc] == rng.leftBound()) {
                    jloc = vals.find_upper(ival);
                    hits.set(0, iloc);
                    hits.adjustSize(jloc, vals.size());
                }
                else {
                    hits.set(0, vals.size());
                }
            }
            else {
                hits.set(0, vals.size());
            }
            break;}
        case ibis::qExpr::OP_EQ: {
            if (rng.leftBound() == rng.rightBound()) {
                iloc = vals.find(ival);
                if (iloc < vals.size() && vals[iloc] == rng.leftBound()) {
                    jloc = vals.find_upper(ival);
                    hits.set(0, iloc);
                    hits.adjustSize(jloc, vals.size());
                }
                else {
                    hits.set(0, vals.size());
                }
            }
            else {
                hits.set(0, vals.size());
            }
            break;}
        case ibis::qExpr::OP_UNDEFINED:
        default: {
            iloc = vals.find(ival);
            if (iloc < vals.size() && vals[iloc] == rng.leftBound()) {
                jloc = vals.find_upper(ival);
                hits.set(0, iloc);
                hits.adjustSize(jloc, vals.size());
            }
            else {
                hits.set(0, vals.size());
            }
            break;}
        } // switch (rng.rightOperator())
        break;}
    case ibis::qExpr::OP_UNDEFINED:
    default: {
        switch (rng.rightOperator()) {
        case ibis::qExpr::OP_LT: {
            jloc = vals.find(jval);
            hits.adjustSize(jloc, vals.size());
            break;}
        case ibis::qExpr::OP_LE: {
            jloc = vals.find_upper(jval);
            hits.adjustSize(jloc, vals.size());
            break;}
        case ibis::qExpr::OP_GT: {
            jloc = vals.find_upper(jval);
            if (jloc < vals.size()) {
                hits.set(0, jloc);
                hits.adjustSize(vals.size(), vals.size());
            }
            else {
                hits.set(0, vals.size());
            }
            break;}
        case ibis::qExpr::OP_GE: {
            jloc = vals.find(jval);
            if (jloc < vals.size()) {
                hits.set(0, jloc);
                hits.adjustSize(vals.size(), vals.size());
            }
            else {
                hits.set(0, vals.size());
            }
            break;}
        case ibis::qExpr::OP_EQ: {
            iloc = vals.find(jval);
            if (iloc < vals.size() && vals[iloc] == rng.rightBound()) {
                jloc = vals.find_upper(jval);
                hits.set(0, iloc);
                hits.adjustSize(jloc, vals.size());
            }
            else {
                hits.set(0, vals.size());
            }
            break;}
        case ibis::qExpr::OP_UNDEFINED:
        default: {
            getNullMask(hits);
            break;}
        } // switch (rng.rightOperator())
        break;}
    } // switch (rng.leftOperator())
    return 0;
} // ibis::column::searchSortedICC

// explicit instantiation
template int ibis::column::searchSortedICC
(const array_t<signed char>&, const ibis::qContinuousRange&,
 ibis::bitvector&) const;
template int ibis::column::searchSortedICC
(const array_t<unsigned char>&, const ibis::qContinuousRange&,
 ibis::bitvector&) const;
template int ibis::column::searchSortedICC
(const array_t<int16_t>&, const ibis::qContinuousRange&,
 ibis::bitvector&) const;
template int ibis::column::searchSortedICC
(const array_t<uint16_t>&, const ibis::qContinuousRange&,
 ibis::bitvector&) const;
template int ibis::column::searchSortedICC
(const array_t<int32_t>&, const ibis::qContinuousRange&,
 ibis::bitvector&) const;
template int ibis::column::searchSortedICC
(const array_t<uint32_t>&, const ibis::qContinuousRange&,
 ibis::bitvector&) const;
template int ibis::column::searchSortedICC
(const array_t<int64_t>&, const ibis::qContinuousRange&,
 ibis::bitvector&) const;
template int ibis::column::searchSortedICC
(const array_t<uint64_t>&, const ibis::qContinuousRange&,
 ibis::bitvector&) const;
template int ibis::column::searchSortedICC
(const array_t<float>&, const ibis::qContinuousRange&,
 ibis::bitvector&) const;
template int ibis::column::searchSortedICC
(const array_t<double>&, const ibis::qContinuousRange&,
 ibis::bitvector&) const;

/// The backup option for searchSortedIC.  This function opens the named
/// file and reads its content one word at a time, which is likely to be
/// very slow.  It does assume the content of the file is sorted in
/// ascending order and perform binary searches.
template<typename T>
int ibis::column::searchSortedOOCC(const char* fname,
                                   const ibis::qContinuousRange& rng,
                                   ibis::bitvector& hits) const {
    if (rng.leftOperator() == ibis::qExpr::OP_UNDEFINED &&
        rng.rightOperator() == ibis::qExpr::OP_UNDEFINED) {
        getNullMask(hits);
        return hits.sloppyCount();
    }

    int fdes = UnixOpen(fname, OPEN_READONLY);
    if (fdes < 0) {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- column[" << fullname() << "]::searchSortedOOCC<"
            << typeid(T).name() << ">(" << fname << ", " << rng
            << ") failed to open the named data file, errno = " << errno
            << strerror(errno);
        return -1;
    }
#if defined(_WIN32) && defined(_MSC_VER)
    (void)_setmode(fdes, _O_BINARY);
#endif
    IBIS_BLOCK_GUARD(UnixClose, fdes);

    int ierr = UnixSeek(fdes, 0, SEEK_END);
    if (ierr < 0) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- column[" << fullname() << "]::searchSortedOOCC<"
            << typeid(T).name() << ">(" << fname << ", " << rng
            << ") failed to seek to the end of file";
        return -2;
    }
    const uint32_t nrows = ierr / sizeof(T);
    const uint32_t sz = sizeof(T);
    hits.clear();
    uint32_t iloc, jloc;
    T ival = (rng.leftOperator() == ibis::qExpr::OP_UNDEFINED ? 0 :
              static_cast<T>(rng.leftBound()));
    if (rng.leftOperator() == ibis::qExpr::OP_LE ||
        rng.leftOperator() == ibis::qExpr::OP_GT)
        ibis::util::round_up(rng.leftBound(), ival);

    T jval = (rng.rightOperator() == ibis::qExpr::OP_UNDEFINED ? 0 :
              static_cast<T>(rng.rightBound()));
    if (rng.rightOperator() == ibis::qExpr::OP_GE ||
        rng.rightOperator() == ibis::qExpr::OP_LT)
        ibis::util::round_up(rng.rightBound(), jval);

    switch (rng.leftOperator()) {
    case ibis::qExpr::OP_LT: {
        switch (rng.rightOperator()) {
        case ibis::qExpr::OP_LT: {
            if (ival < jval) {
                iloc = findUpper<T>(fdes, nrows, ival);
                jloc = findLower<T>(fdes, nrows, jval);
                if (iloc < jloc) {
                    hits.set(0, iloc);
                    hits.adjustSize(jloc, nrows);
                }
            }
            else {
                hits.set(0, nrows);
            }
            break;}
        case ibis::qExpr::OP_LE: {
            if (ival < jval) {
                iloc = findUpper<T>(fdes, nrows, ival);
                jloc = findUpper<T>(fdes, nrows, jval);
                if (iloc < jloc) {
                    hits.set(0, iloc);
                    hits.adjustSize(jloc, nrows);
                }
            }
            else {
                hits.set(0, nrows);
            }
            break;}
        case ibis::qExpr::OP_GT: {
            if (ival >= jval) {
                iloc = findUpper<T>(fdes, nrows, ival);
                if (iloc < nrows) {
                    hits.appendFill(0, iloc);
                    hits.adjustSize(nrows, nrows);
                }
                else {
                    hits.set(0, nrows);
                }
            }
            else {
                iloc = findUpper<T>(fdes, nrows, jval);
                if (iloc < nrows) {
                    hits.set(0, iloc);
                    hits.adjustSize(nrows, nrows);
                }
                else {
                    hits.set(0, nrows);
                }
            }
            break;}
        case ibis::qExpr::OP_GE: {
            if (ival >= jval) {
                iloc = findUpper<T>(fdes, nrows, ival);
                if (iloc < nrows) {
                    hits.set(0, iloc);
                    hits.adjustSize(nrows, nrows);
                }
                else {
                    hits.set(0, nrows);
                }
            }
            else {
                iloc = findLower<T>(fdes, nrows, jval);
                if (iloc < nrows) {
                    hits.set(0, iloc);
                    hits.adjustSize(nrows, nrows);
                }
                else {
                    hits.set(0, nrows);
                }
            }
            break;}
        case ibis::qExpr::OP_EQ: {
            if (rng.rightBound() > rng.leftBound()) {
                T tmp;
                iloc = findLower<T>(fdes, nrows, jval);
                ierr = UnixSeek(fdes, iloc*sz, SEEK_SET);
                ierr = ibis::util::read(fdes, &tmp, sz);
                if (iloc < nrows && ierr == (int) sz &&
                    tmp == rng.rightBound()) {
                    for (jloc = iloc+1; jloc < nrows; ++ jloc) {
                        ierr = ibis::util::read(fdes, &tmp, sz);
                        if (ierr < (int)sz || tmp != jval) break;
                    }
                    hits.set(0, iloc);
                    hits.adjustSize(jloc, nrows);
                    ibis::fileManager::instance().recordPages(iloc*sz,
                                                              jloc*sz+sz);
                }
                else {
                    hits.set(0, nrows);
                    ibis::fileManager::instance().recordPages(iloc*sz,
                                                              iloc*sz+sz);
                }
            }
            else {
                hits.set(0, nrows);
            }
            break;}
        case ibis::qExpr::OP_UNDEFINED:
        default: {
            iloc = findUpper<T>(fdes, nrows, ival);
            if (iloc < nrows) {
                hits.set(0, iloc);
                hits.adjustSize(nrows, nrows);
            }
            else {
                hits.set(0, nrows);
            }
            break;}
        } // switch (rng.rightOperator())
        break;}
    case ibis::qExpr::OP_LE: {
        switch (rng.rightOperator()) {
        case ibis::qExpr::OP_LT: {
            if (ival < jval) {
                iloc = findLower<T>(fdes, nrows, ival);
                jloc = findLower<T>(fdes, nrows, jval);
                if (iloc < jloc) {
                    hits.set(0, iloc);
                    hits.adjustSize(jloc, nrows);
                }
            }
            else {
                hits.set(0, nrows);
            }
            break;}
        case ibis::qExpr::OP_LE: {
            if (ival <= jval) {
                iloc = findLower<T>(fdes, nrows, ival);
                jloc = findUpper<T>(fdes, nrows, jval);
                if (iloc < jloc) {
                    hits.set(0, iloc);
                    hits.adjustSize(jloc, nrows);
                }
            }
            else {
                hits.set(0, nrows);
            }
            break;}
        case ibis::qExpr::OP_GT: {
            if (ival > jval) {
                iloc = findLower<T>(fdes, nrows, ival);
                if (iloc < nrows) {
                    hits.appendFill(0, iloc);
                    hits.adjustSize(nrows, nrows);
                }
                else {
                    hits.set(0, nrows);
                }
            }
            else {
                iloc = findUpper<T>(fdes, nrows, jval);
                if (iloc < nrows) {
                    hits.set(0, iloc);
                    hits.adjustSize(nrows, nrows);
                }
                else {
                    hits.set(0, nrows);
                }
            }
            break;}
        case ibis::qExpr::OP_GE: {
            if (ival >= jval) {
                iloc = findLower<T>(fdes, nrows, ival);
                if (iloc < nrows) {
                    hits.set(0, iloc);
                    hits.adjustSize(nrows, nrows);
                }
                else {
                    hits.set(0, nrows);
                }
            }
            else {
                iloc = findLower<T>(fdes, nrows, jval);
                if (iloc < nrows) {
                    hits.set(0, iloc);
                    hits.adjustSize(nrows, nrows);
                }
                else {
                    hits.set(0, nrows);
                }
            }
            break;}
        case ibis::qExpr::OP_EQ: {
            if (rng.rightBound() >= rng.leftBound()) {
                T tmp;
                iloc = findLower<T>(fdes, nrows, jval);
                ierr = UnixSeek(fdes, iloc*sz, SEEK_SET);
                ierr = UnixRead(fdes, &tmp, sz);
                if (iloc < nrows && ierr == (int)sz &&
                    tmp == rng.rightBound()) {
                    for (jloc = iloc+1; jloc < nrows; ++ jloc) {
                        ierr = UnixRead(fdes, &tmp, sz);
                        if (ierr < (int) sz || tmp != jval) break;
                    }
                    hits.set(0, iloc);
                    hits.adjustSize(jloc, nrows);
                    ibis::fileManager::instance().recordPages(iloc*sz,
                                                              jloc*sz+sz);
                }
                else {
                    hits.set(0, nrows);
                    ibis::fileManager::instance().recordPages(iloc*sz,
                                                              iloc*sz+sz);
                }
            }
            else {
                hits.set(0, nrows);
            }
            break;}
        case ibis::qExpr::OP_UNDEFINED:
        default: {
            iloc = findLower<T>(fdes, nrows, ival);
            if (iloc < nrows) {
                hits.set(0, iloc);
                hits.adjustSize(nrows, nrows);
            }
            else {
                hits.set(0, nrows);
            }
            break;}
        } // switch (rng.rightOperator())
        break;}
    case ibis::qExpr::OP_GT: {
        switch (rng.rightOperator()) {
        case ibis::qExpr::OP_LT: {
            if (ival <= jval) {
                iloc = findLower<T>(fdes, nrows, ival);
                if (iloc > 0) {
                    hits.adjustSize(iloc, nrows);
                }
                else {
                    hits.set(0, nrows);
                }
            }
            else {
                iloc = findLower<T>(fdes, nrows, jval);
                if (iloc > 0) {
                    hits.adjustSize(iloc, nrows);
                }
                else {
                    hits.set(0, nrows);
                }
            }
            break;}
        case ibis::qExpr::OP_LE: {
            if (ival < jval) {
                iloc = findLower<T>(fdes, nrows, ival);
                if (iloc > 0) {
                    hits.adjustSize(iloc, nrows);
                }
                else {
                    hits.set(0, nrows);
                }
            }
            else {
                iloc = findUpper<T>(fdes, nrows, jval);
                if (iloc > 0) {
                    hits.adjustSize(iloc, nrows);
                }
                else {
                    hits.set(0, nrows);
                }
            }
            break;}
        case ibis::qExpr::OP_GT: {
            if (jval < ival) {
                iloc = findUpper<T>(fdes, nrows, jval);
                jloc = findLower<T>(fdes, nrows, ival);
                if (jloc > iloc) {
                    hits.set(0, iloc);
                    hits.adjustSize(jloc, nrows);
                }
                else {
                    hits.set(0, nrows);
                }
            }
            else {
                hits.set(0, nrows);
            }
            break;}
        case ibis::qExpr::OP_GE: {
            if (jval < ival) {
                iloc = findLower<T>(fdes, nrows, jval);
                jloc = findLower<T>(fdes, nrows, ival);
                if (jloc > iloc) {
                    hits.set(0, iloc);
                    hits.adjustSize(jloc, nrows);
                }
                else {
                    hits.set(0, nrows);
                }
            }
            else {
                hits.set(0, nrows);
            }
            break;}
        case ibis::qExpr::OP_EQ: {
            if (rng.rightBound() > rng.leftBound()) {
                T tmp;
                iloc = findLower<T>(fdes, nrows, jval);
                ierr = UnixSeek(fdes, iloc*sz, SEEK_SET);
                ierr = UnixRead(fdes, &tmp, sz);
                if (iloc < nrows && ierr == (int)sz &&
                    tmp == rng.rightBound()) {
                    for (jloc = iloc+1; jloc < nrows; ++ jloc) {
                        ierr = UnixRead(fdes, &tmp, sz);
                        if (ierr < (int)sz || tmp != jval) break;
                    }
                    hits.set(0, iloc);
                    hits.adjustSize(jloc, nrows);
                    ibis::fileManager::instance().recordPages(iloc*sz,
                                                              jloc*sz+sz);
                }
                else {
                    hits.set(0, nrows);
                    ibis::fileManager::instance().recordPages(iloc*sz,
                                                              iloc*sz+sz);
                }
            }
            else {
                hits.set(0, nrows);
            }
            break;}
        case ibis::qExpr::OP_UNDEFINED:
        default: {
            iloc = findLower<T>(fdes, nrows, ival);
            hits.adjustSize(iloc, nrows);
            break;}
        } // switch (rng.rightOperator())
        break;}
    case ibis::qExpr::OP_GE: {
        switch (rng.rightOperator()) {
        case ibis::qExpr::OP_LT: {
            if (ival < jval) {
                iloc = findUpper<T>(fdes, nrows, ival);
                if (iloc > 0) {
                    hits.adjustSize(iloc, nrows);
                }
                else {
                    hits.set(0, nrows);
                }
            }
            else {
                iloc = findLower<T>(fdes, nrows, jval);
                if (iloc > 0) {
                    hits.adjustSize(iloc, nrows);
                }
                else {
                    hits.set(0, nrows);
                }
            }
            break;}
        case ibis::qExpr::OP_LE: {
            if (ival <= jval) {
                iloc = findUpper<T>(fdes, nrows, ival);
                if (iloc > 0) {
                    hits.adjustSize(iloc, nrows);
                }
                else {
                    hits.set(0, nrows);
                }
            }
            else {
                iloc = findUpper<T>(fdes, nrows, jval);
                if (iloc > 0) {
                    hits.adjustSize(iloc, nrows);
                }
                else {
                    hits.set(0, nrows);
                }
            }
            break;}
        case ibis::qExpr::OP_GT: {
            if (jval < ival) {
                iloc = findUpper<T>(fdes, nrows, jval);
                jloc = findUpper<T>(fdes, nrows, ival);
                if (jloc > iloc) {
                    hits.set(0, iloc);
                    hits.adjustSize(jloc, nrows);
                }
                else {
                    hits.set(0, nrows);
                }
            }
            else {
                hits.set(0, nrows);
            }
            break;}
        case ibis::qExpr::OP_GE: {
            if (jval <= ival) {
                iloc = findLower<T>(fdes, nrows, jval);
                jloc = findUpper<T>(fdes, nrows, ival);
                if (jloc > iloc) {
                    hits.set(0, iloc);
                    hits.adjustSize(jloc, nrows);
                }
                else {
                    hits.set(0, nrows);
                }
            }
            else {
                hits.set(0, nrows);
            }
            break;}
        case ibis::qExpr::OP_EQ: {
            if (rng.rightBound() >= rng.leftBound()) {
                T tmp;
                iloc = findLower<T>(fdes, nrows, jval);
                ierr = UnixSeek(fdes, iloc*sz, SEEK_SET);
                ierr = UnixRead(fdes, &tmp, sz);
                if (iloc < nrows && ierr == (int)sz &&
                    tmp == rng.rightBound()) {
                    for (jloc = iloc+1; jloc < nrows; ++ jloc) {
                        ierr = UnixRead(fdes, &tmp, sz);
                        if (ierr < (int)sz || tmp != jval) break;
                    }
                    hits.set(0, iloc);
                    hits.adjustSize(jloc, nrows);
                    ibis::fileManager::instance().recordPages(iloc*sz,
                                                              jloc*sz+sz);
                }
                else {
                    hits.set(0, nrows);
                    ibis::fileManager::instance().recordPages(iloc*sz,
                                                              iloc*sz+sz);
                }
            }
            else {
                hits.set(0, nrows);
            }
            break;}
        case ibis::qExpr::OP_UNDEFINED:
        default: {
            iloc = findUpper<T>(fdes, nrows, ival);
            hits.adjustSize(iloc, nrows);
            break;}
        } // switch (rng.rightOperator())
        break;}
    case ibis::qExpr::OP_EQ: {
        switch (rng.rightOperator()) {
        case ibis::qExpr::OP_LT: {
            if (rng.leftBound() < rng.rightBound()) {
                T tmp;
                iloc = findLower<T>(fdes, nrows, ival);
                ierr = UnixSeek(fdes, iloc*sz, SEEK_SET);
                ierr = UnixRead(fdes, &tmp, sz);
                if (iloc < nrows && ierr == (int)sz &&
                    tmp == rng.leftBound()) {
                    for (jloc = iloc+1; jloc < nrows; ++ jloc) {
                        ierr = UnixRead(fdes, &tmp, sz);
                        if (ierr < (int)sz || tmp != ival) break;
                    }
                    hits.set(0, iloc);
                    hits.adjustSize(jloc, nrows);
                    ibis::fileManager::instance().recordPages(iloc*sz,
                                                              jloc*sz+sz);
                }
                else {
                    hits.set(0, nrows);
                    ibis::fileManager::instance().recordPages(iloc*sz,
                                                              iloc*sz+sz);
                }
            }
            else {
                hits.set(0, nrows);
            }
            break;}
        case ibis::qExpr::OP_LE: {
            if (rng.leftBound() <= rng.rightBound()) {
                T tmp;
                iloc = findLower<T>(fdes, nrows, ival);
                ierr = UnixSeek(fdes, iloc*sz, SEEK_SET);
                ierr = UnixRead(fdes, &tmp, sz);
                if (iloc < nrows && ierr == (int)sz &&
                    tmp == rng.leftBound()) {
                    for (jloc = iloc+1; jloc < nrows; ++ jloc) {
                        ierr = UnixRead(fdes, &tmp, sz);
                        if (ierr < (int)sz || tmp != ival) break;
                    }
                    hits.set(0, iloc);
                    hits.adjustSize(jloc, nrows);
                    ibis::fileManager::instance().recordPages(iloc*sz,
                                                              jloc*sz+sz);
                }
                else {
                    hits.set(0, nrows);
                    ibis::fileManager::instance().recordPages(iloc*sz,
                                                              iloc*sz+sz);
                }
            }
            else {
                hits.set(0, nrows);
            }
            break;}
        case ibis::qExpr::OP_GT: {
            if (rng.leftBound() > rng.rightBound()) {
                T tmp;
                iloc = findLower<T>(fdes, nrows, ival);
                ierr = UnixSeek(fdes, iloc*sz, SEEK_SET);
                ierr = UnixRead(fdes, &tmp, sz);
                if (iloc < nrows && ierr == (int) sz &&
                    tmp == rng.leftBound()) {
                    for (jloc = iloc+1; jloc < nrows; ++ jloc) {
                        ierr = UnixRead(fdes, &tmp, sz);
                        if (ierr < (int)sz || tmp != ival) break;
                    }
                    hits.set(0, iloc);
                    hits.adjustSize(jloc, nrows);
                    ibis::fileManager::instance().recordPages(iloc*sz,
                                                              jloc*sz+sz);
                }
                else {
                    hits.set(0, nrows);
                    ibis::fileManager::instance().recordPages(iloc*sz,
                                                              iloc*sz+sz);
                }
            }
            else {
                hits.set(0, nrows);
            }
            break;}
        case ibis::qExpr::OP_GE: {
            if (rng.leftBound() >= rng.rightBound()) {
                T tmp;
                iloc = findLower<T>(fdes, nrows, ival);
                ierr = UnixSeek(fdes, iloc*sz, SEEK_SET);
                ierr = UnixRead(fdes, &tmp, sz);
                if (iloc < nrows && ierr == (int)sz &&
                    tmp == rng.leftBound()) {
                    for (jloc = iloc+1; jloc < nrows; ++ jloc) {
                        ierr = UnixRead(fdes, &tmp, sz);
                        if (ierr < (int)sz || tmp != ival) break;
                    }
                    hits.set(0, iloc);
                    hits.adjustSize(jloc, nrows);
                    ibis::fileManager::instance().recordPages(iloc*sz,
                                                              jloc*sz+sz);
                }
                else {
                    hits.set(0, nrows);
                    ibis::fileManager::instance().recordPages(iloc*sz,
                                                              iloc*sz+sz);
                }
            }
            else {
                hits.set(0, nrows);
            }
            break;}
        case ibis::qExpr::OP_EQ: {
            if (rng.leftBound() == rng.rightBound()) {
                T tmp;
                iloc = findLower<T>(fdes, nrows, ival);
                ierr = UnixSeek(fdes, iloc*sz, SEEK_SET);
                ierr = UnixRead(fdes, &tmp, sz);
                if (iloc < nrows && ierr == (int)sz &&
                    tmp == rng.leftBound()) {
                    for (jloc = iloc+1; jloc < nrows; ++ jloc) {
                        ierr = UnixRead(fdes, &tmp, sz);
                        if (ierr < (int)sz || tmp != ival) break;
                    }
                    hits.set(0, iloc);
                    hits.adjustSize(jloc, nrows);
                    ibis::fileManager::instance().recordPages(iloc*sz,
                                                              jloc*sz+sz);
                }
                else {
                    hits.set(0, nrows);
                    ibis::fileManager::instance().recordPages(iloc*sz,
                                                              iloc*sz+sz);
                }
            }
            else {
                hits.set(0, nrows);
            }
            break;}
        case ibis::qExpr::OP_UNDEFINED:
        default: {
            T tmp;
            iloc = findLower<T>(fdes, nrows, ival);
            ierr = UnixSeek(fdes, iloc*sz, SEEK_SET);
            ierr = UnixRead(fdes, &tmp, sz);
            if (iloc < nrows && ierr == (int)sz &&
                tmp == rng.leftBound()) {
                for (jloc = iloc+1; jloc < nrows; ++ jloc) {
                    ierr = UnixRead(fdes, &tmp, sz);
                    if (ierr < (int)sz || tmp != ival) break;
                }
                hits.set(0, iloc);
                hits.adjustSize(jloc, nrows);
                ibis::fileManager::instance().recordPages(iloc*sz, jloc*sz+sz);
            }
            else {
                hits.set(0, nrows);
                ibis::fileManager::instance().recordPages(iloc*sz, iloc*sz+sz);
            }
            break;}
        } // switch (rng.rightOperator())
        break;}
    case ibis::qExpr::OP_UNDEFINED:
    default: {
        switch (rng.rightOperator()) {
        case ibis::qExpr::OP_LT: {
            jloc = findLower<T>(fdes, nrows, jval);
            hits.adjustSize(jloc, nrows);
            break;}
        case ibis::qExpr::OP_LE: {
            jloc = findUpper<T>(fdes, nrows, jval);
            hits.adjustSize(jloc, nrows);
            break;}
        case ibis::qExpr::OP_GT: {
            jloc = findUpper<T>(fdes, nrows, jval);
            if (jloc < nrows) {
                hits.set(0, jloc);
                hits.adjustSize(nrows, nrows);
            }
            else {
                hits.set(0, nrows);
            }
            break;}
        case ibis::qExpr::OP_GE: {
            jloc = findLower<T>(fdes, nrows, jval);
            if (jloc < nrows) {
                hits.set(0, jloc);
                hits.adjustSize(nrows, nrows);
            }
            else {
                hits.set(0, nrows);
            }
            break;}
        case ibis::qExpr::OP_EQ: {
            T tmp;
            iloc = findLower<T>(fdes, nrows, jval);
            ierr = UnixSeek(fdes, iloc*sz, SEEK_SET);
            ierr = UnixRead(fdes, &tmp, sz);
            if (iloc < nrows && ierr == (int)sz &&
                tmp == rng.rightBound()) {
                for (jloc = iloc+1; jloc < nrows; ++ jloc) {
                    ierr = UnixRead(fdes, &tmp, sz);
                    if (ierr < (int)sz || tmp != jval) break;
                }
                hits.set(0, iloc);
                hits.adjustSize(jloc, nrows);
                ibis::fileManager::instance().recordPages(iloc*sz, jloc*sz+sz);
            }
            else {
                hits.set(0, nrows);
                ibis::fileManager::instance().recordPages(iloc*sz, iloc*sz+sz);
            }
            break;}
        case ibis::qExpr::OP_UNDEFINED:
        default: {
            getNullMask(hits);
            break;}
        } // switch (rng.rightOperator())
        break;}
    } // switch (rng.leftOperator())

    return 0;
} // ibis::column::searchSortedOOCC

// explicit instantiation
template int ibis::column::searchSortedOOCC<signed char>
(const char*, const ibis::qContinuousRange&, ibis::bitvector& hits) const;
template int ibis::column::searchSortedOOCC<unsigned char>
(const char*, const ibis::qContinuousRange&, ibis::bitvector& hits) const;
template int ibis::column::searchSortedOOCC<int16_t>
(const char*, const ibis::qContinuousRange&, ibis::bitvector& hits) const;
template int ibis::column::searchSortedOOCC<uint16_t>
(const char*, const ibis::qContinuousRange&, ibis::bitvector& hits) const;
template int ibis::column::searchSortedOOCC<int32_t>
(const char*, const ibis::qContinuousRange&, ibis::bitvector& hits) const;
template int ibis::column::searchSortedOOCC<uint32_t>
(const char*, const ibis::qContinuousRange&, ibis::bitvector& hits) const;
template int ibis::column::searchSortedOOCC<int64_t>
(const char*, const ibis::qContinuousRange&, ibis::bitvector& hits) const;
template int ibis::column::searchSortedOOCC<uint64_t>
(const char*, const ibis::qContinuousRange&, ibis::bitvector& hits) const;
template int ibis::column::searchSortedOOCC<float>
(const char*, const ibis::qContinuousRange&, ibis::bitvector& hits) const;
template int ibis::column::searchSortedOOCC<double>
(const char*, const ibis::qContinuousRange&, ibis::bitvector& hits) const;

/// An equivalent of array_t<T>::find.  It reads the open file one word at
/// a time and therefore is likely to be very slow.
template<typename T> uint32_t
ibis::column::findLower(int fdes, const uint32_t nr, const T tgt) const {
    int ierr;
    const uint32_t sz = sizeof(T);
    uint32_t left = 0, right = nr;
    uint32_t mid = ((left + right) >> 1);
    while (mid > left) {
        off_t pos = mid * sz;
        ierr = UnixSeek(fdes, pos, SEEK_SET);
        if (ierr != pos) {
            LOGGER(ibis::gVerbose >= 0)
                << "Warning -- column[" << fullname() << "]::findLower("
                << fdes << ", " << tgt << ") failed to seek to " << pos
                << ", ierr = " << ierr;
            return nr;
        }

        T tmp;
        ierr = UnixRead(fdes, &tmp, sz);
        ibis::fileManager::instance().recordPages(pos, pos+sz);
        if (ierr != (int) sz) {
            LOGGER(ibis::gVerbose >= 0)
                << "Warning -- column[" << fullname() << "]::findLower("
                << fdes << ", " << tgt << ") failed to read a word of type "
                << typeid(T).name() << " at " << pos << ", ierr = " << ierr;
            return nr;
        }

        if (tmp < tgt)
            left = mid;
        else
            right = mid;
        mid = ((left + right) >> 1);
    }

    if (mid < nr) { // read the value at mid
        off_t pos = mid * sz;
        ierr = UnixSeek(fdes, pos, SEEK_SET);
        if (ierr != pos) {
            LOGGER(ibis::gVerbose >= 0)
                << "Warning -- column[" << fullname() << "]::findLower("
                << fdes << ", " << tgt << ") failed to seek to " << pos
                << ", ierr = " << ierr;
            return nr;
        }

        T tmp;
        ierr = UnixRead(fdes, &tmp, sz);
        ibis::fileManager::instance().recordPages(pos, pos+sz);
        if (ierr != (int) sz) {
            LOGGER(ibis::gVerbose >= 0)
                << "Warning -- column[" << fullname() << "]::findLower("
                << fdes << ", " << tgt << ") failed to read a word of type "
                << typeid(T).name() << " at " << pos << ", ierr = " << ierr;
            return nr;
        }
        if (tmp < tgt)
            ++ mid;
    }
    return mid;
} // ibis::column::findLower

/// An equivalent of array_t<T>::find_upper.  It reads the open file one
/// word at a time and therefore is likely to be very slow.
template<typename T> uint32_t
ibis::column::findUpper(int fdes, const uint32_t nr, const T tgt) const {
    int ierr;
    const uint32_t sz = sizeof(T);
    uint32_t left = 0, right = nr;
    uint32_t mid = ((left + right) >> 1);
    while (mid > left) {
        off_t pos = mid * sz;
        ierr = UnixSeek(fdes, pos, SEEK_SET);
        if (ierr != pos) {
            LOGGER(ibis::gVerbose >= 0)
                << "Warning -- column[" << fullname() << "]::findUpper("
                << fdes << ", " << tgt << ") failed to seek to " << pos
                << ", ierr = " << ierr;
            return nr;
        }

        T tmp;
        ierr = UnixRead(fdes, &tmp, sz);
        ibis::fileManager::instance().recordPages(pos, pos+sz);
        if (ierr != (int) sz) {
            LOGGER(ibis::gVerbose >= 0)
                << "Warning -- column[" << fullname() << "]::findUpper("
                << fdes << ", " << tgt << ") failed to read a word of type "
                << typeid(T).name() << " at " << pos << ", ierr = " << ierr;
            return nr;
        }

        if (tgt < tmp)
            right = mid;
        else
            left = mid;
        mid = ((left + right) >> 1);
    }

    if (mid < nr) { // read the value at mid
        off_t pos = mid * sz;
        ierr = UnixSeek(fdes, pos, SEEK_SET);
        if (ierr != pos) {
            LOGGER(ibis::gVerbose >= 0)
                << "Warning -- column[" << fullname() << "]::findLower("
                << fdes << ", " << tgt << ") failed to seek to " << pos
                << ", ierr = " << ierr;
            return nr;
        }

        T tmp;
        ierr = UnixRead(fdes, &tmp, sz);
        ibis::fileManager::instance().recordPages(pos, pos+sz);
        if (ierr != (int) sz) {
            LOGGER(ibis::gVerbose >= 0)
                << "Warning -- column[" << fullname() << "]::findLower("
                << fdes << ", " << tgt << ") failed to read a word of type "
                << typeid(T).name() << " at " << pos << ", ierr = " << ierr;
            return nr;
        }
        if (! (tgt < tmp))
            ++ mid;
    }
    return mid;
} // ibis::column::findUpper

template<typename T>
int ibis::column::searchSortedICD(const array_t<T>& vals,
                                  const ibis::qDiscreteRange& rng,
                                  ibis::bitvector& hits) const {
    const ibis::array_t<double>& u = rng.getValues();
    std::string evt = "column::searchSortedICD";
    if (ibis::gVerbose > 4) {
        std::ostringstream oss;
        oss << "column[" << fullname() << "]::searchSortedICD<"
            << typeid(T).name() << ">(" << rng.colName() << " IN "
            << u.size() << "-element list)";
        evt = oss.str();
    }
    ibis::util::timer mytimer(evt.c_str(), 5);
    hits.clear();
    hits.reserve(vals.size(), u.size()); // reserve space


    uint32_t ju = 0;
    uint32_t jv = 0;
    while (ju < u.size() && jv < vals.size()) {
        if (u[ju] < vals[jv])
            ju = ibis::util::find(u, (double)vals[jv], ju);
        if (ju < u.size()) {
            if (u[ju] > vals[jv])
                jv = ibis::util::find(vals, (T)u[ju], jv);
            while (jv < vals.size() && u[ju] == vals[jv]) {
                hits.setBit(jv, 1);
                ++ jv;
            }
        }
    }
    hits.adjustSize(0, vals.size());
    return 0;
} // ibis::column::searchSortedICD

// explicit instantiation
template int ibis::column::searchSortedICD
(const array_t<signed char>&, const ibis::qDiscreteRange&,
 ibis::bitvector&) const;
template int ibis::column::searchSortedICD
(const array_t<unsigned char>&, const ibis::qDiscreteRange&,
 ibis::bitvector&) const;
template int ibis::column::searchSortedICD
(const array_t<int16_t>&, const ibis::qDiscreteRange&,
 ibis::bitvector&) const;
template int ibis::column::searchSortedICD
(const array_t<uint16_t>&, const ibis::qDiscreteRange&,
 ibis::bitvector&) const;
template int ibis::column::searchSortedICD
(const array_t<int32_t>&, const ibis::qDiscreteRange&,
 ibis::bitvector&) const;
template int ibis::column::searchSortedICD
(const array_t<uint32_t>&, const ibis::qDiscreteRange&,
 ibis::bitvector&) const;
template int ibis::column::searchSortedICD
(const array_t<int64_t>&, const ibis::qDiscreteRange&,
 ibis::bitvector&) const;
template int ibis::column::searchSortedICD
(const array_t<uint64_t>&, const ibis::qDiscreteRange&,
 ibis::bitvector&) const;
template int ibis::column::searchSortedICD
(const array_t<float>&, const ibis::qDiscreteRange&,
 ibis::bitvector&) const;
template int ibis::column::searchSortedICD
(const array_t<double>&, const ibis::qDiscreteRange&,
 ibis::bitvector&) const;

/// This version of search function reads the content of data file through
/// explicit read operations.  It sequentially reads the content of the
/// data file.  Note the content of the data file is assumed to be sorted
/// in ascending order as elementary data type T.
template<typename T>
int ibis::column::searchSortedOOCD(const char* fname,
                                   const ibis::qDiscreteRange& rng,
                                   ibis::bitvector& hits) const {
    const ibis::array_t<double>& u = rng.getValues();
    std::string evt = "column::searchSortedOOCD";
    if (ibis::gVerbose > 4) {
        std::ostringstream oss;
        oss << "column[" << fullname() << "]::searchSortedOOCD<"
            << typeid(T).name() << ">(" << fname << ", " << rng.colName()
            << " IN " << u.size() << "-element list)";
        evt = oss.str();
    }
    ibis::util::timer mytimer(evt.c_str(), 5);
    int fdes = UnixOpen(fname, OPEN_READONLY);
    if (fdes < 0) {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- " << evt << " failed to "
            << "open the named data file, errno = " << errno
            << strerror(errno);
        return -1;
    }
#if defined(_WIN32) && defined(_MSC_VER)
    (void)_setmode(fdes, _O_BINARY);
#endif
    IBIS_BLOCK_GUARD(UnixClose, fdes);

    const uint32_t sz = sizeof(T);
    int ierr = UnixSeek(fdes, 0, SEEK_END);
    if (ierr < 0) {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- " << evt << " failed to seek to the end of file";
        return -2;
    }
    ibis::fileManager::instance().recordPages(0, ierr);
    const uint32_t nrows = ierr / sz;
    ibis::fileManager::buffer<T> buf;
    hits.clear();
    hits.reserve(nrows, u.size()); // reserve space
    ierr = UnixSeek(fdes, 0, SEEK_SET); // point to the beginning of file
    if (buf.size() > 0) { // has a buffer to use
        uint32_t ju = 0;
        uint32_t jv = 0;
        while (ju < u.size() && 0 <
               (ierr = ibis::util::read(fdes, buf.address(), buf.size()*sz))) {
            for (uint32_t j = 0; ju < u.size() && j < buf.size(); ++ j) {
                while (ju < u.size() && u[ju] < buf[j]) ++ ju;
                if (buf[j] == u[ju]) {
                    hits.setBit(jv+j, 1);
                }
            }
            jv += ierr / sz;
        }
    }
    else { // read one value at a time
        T tmp;
        uint32_t ju = 0;
        uint32_t jv = 0;
        while (ju < u.size() &&
               (ierr = UnixRead(fdes, &tmp, sizeof(tmp))) > 0) {
            while (ju < u.size() && u[ju] < tmp) ++ ju;
            if (u[ju] == tmp)
                hits.setBit(jv, 1);
            ++ jv;
        }
    }

    hits.adjustSize(0, nrows);
    return (ierr > 0 ? 0 : -3);
} // ibis::column::searchSortedOOCD

template<typename T>
int ibis::column::searchSortedICD(const array_t<T>& vals,
                                  const ibis::qIntHod& rng,
                                  ibis::bitvector& hits) const {
    const ibis::array_t<int64_t>& u = rng.getValues();
    std::string evt = "column::searchSortedICD";
    if (ibis::gVerbose > 4) {
        std::ostringstream oss;
        oss << "column[" << fullname() << "]::searchSortedICD<"
            << typeid(T).name() << ">(" << rng.colName() << " IN "
            << u.size() << "-element list)";
        evt = oss.str();
    }
    ibis::util::timer mytimer(evt.c_str(), 5);
    hits.clear();
    hits.reserve(vals.size(), u.size()); // reserve space
    uint32_t ju = 0;
    uint32_t jv = 0;
    while (ju < u.size() && jv < vals.size()) {
        if (u[ju] < (int64_t)vals[jv])
            ju = ibis::util::find(u, (int64_t)vals[jv], ju);

        if (ju < u.size()) {
            if (u[ju] > (int64_t)vals[jv])
                jv = ibis::util::find(vals, (T)u[ju], jv);
            while (jv < vals.size() && u[ju] == vals[jv]) {
                hits.setBit(jv, 1);
                ++ jv;
            }
        }
    }
    hits.adjustSize(0, vals.size());
    return 0;
} // ibis::column::searchSortedICD

/// This version of search function reads the content of data file through
/// explicit read operations.  It sequentially reads the content of the
/// data file.  Note the content of the data file is assumed to be sorted
/// in ascending order as elementary data type T.
template<typename T>
int ibis::column::searchSortedOOCD(const char* fname,
                                   const ibis::qIntHod& rng,
                                   ibis::bitvector& hits) const {
    const ibis::array_t<int64_t>& u = rng.getValues();
    std::string evt = "column::searchSortedOOCD";
    if (ibis::gVerbose > 4) {
        std::ostringstream oss;
        oss << "column[" << fullname() << "]::searchSortedOOCD<"
            << typeid(T).name() << ">(" << fname << ", " << rng.colName()
            << " IN " << u.size() << "-element list)";
        evt = oss.str();
    }
    ibis::util::timer mytimer(evt.c_str(), 5);
    int fdes = UnixOpen(fname, OPEN_READONLY);
    if (fdes < 0) {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- " << evt << " failed to "
            << "open the named data file, errno = " << errno
            << strerror(errno);
        return -1;
    }
#if defined(_WIN32) && defined(_MSC_VER)
    (void)_setmode(fdes, _O_BINARY);
#endif
    IBIS_BLOCK_GUARD(UnixClose, fdes);

    const uint32_t sz = sizeof(T);
    int ierr = UnixSeek(fdes, 0, SEEK_END);
    if (ierr < 0) {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- " << evt << " failed to seek to the end of file";
        return -2;
    }
    ibis::fileManager::instance().recordPages(0, ierr);
    const uint32_t nrows = ierr / sz;
    ibis::fileManager::buffer<T> buf;
    hits.clear();
    hits.reserve(nrows, u.size()); // reserve space
    ierr = UnixSeek(fdes, 0, SEEK_SET); // point to the beginning of file
    if (buf.size() > 0) { // has a buffer to use
        uint32_t ju = 0;
        uint32_t jv = 0;
        while (ju < u.size() && 0 <
               (ierr = ibis::util::read(fdes, buf.address(), buf.size()*sz))) {
            for (uint32_t j = 0; ju < u.size() && j < buf.size(); ++ j) {
                while (ju < u.size() && u[ju] < (int64_t)buf[j]) ++ ju;
                if ((int64_t)buf[j] == u[ju]) {
                    hits.setBit(jv+j, 1);
                }
            }
            jv += ierr / sz;
        }
    }
    else { // read one value at a time
        T tmp;
        int64_t itmp;
        uint32_t ju = 0;
        uint32_t jv = 0;
        while (ju < u.size() &&
               (ierr = UnixRead(fdes, &tmp, sizeof(tmp))) > 0) {
            itmp = (int64_t) tmp;
            if ((T)itmp == tmp) {
                while (ju < u.size() && u[ju] < itmp) ++ ju;
                if (u[ju] == itmp)
                    hits.setBit(jv, 1);
            }
            ++ jv;
        }
    }

    hits.adjustSize(0, nrows);
    return (ierr > 0 ? 0 : -3);
} // ibis::column::searchSortedOOCD

template<typename T>
int ibis::column::searchSortedICD(const array_t<T>& vals,
                                  const ibis::qUIntHod& rng,
                                  ibis::bitvector& hits) const {
    const ibis::array_t<uint64_t>& u = rng.getValues();
    std::string evt = "column::searchSortedICD";
    if (ibis::gVerbose > 4) {
        std::ostringstream oss;
        oss << "column[" << fullname() << "]::searchSortedICD<"
            << typeid(T).name() << ">(" << rng.colName() << " IN "
            << u.size() << "-element list)";
        evt = oss.str();
    }
    ibis::util::timer mytimer(evt.c_str(), 5);
    hits.clear();
    hits.reserve(vals.size(), u.size()); // reserve space
    size_t ju = 0;
    size_t jv = 0;
    while (ju < u.size() && jv < vals.size()) {
        if (u[ju] < (uint64_t)vals[jv])
            ju = ibis::util::find(u, (uint64_t)vals[jv], ju);
        if (ju < u.size()) {
            if (u[ju] > (uint64_t)vals[jv])
                jv = ibis::util::find(vals, (T)u[ju], jv);
            while (jv < vals.size() && u[ju] == (uint64_t)vals[jv]) {
                hits.setBit(jv, 1);
                ++ jv;
            }
        }
    }
    hits.adjustSize(0, vals.size());
    return 0;
} // ibis::column::searchSortedICD

/// This version of search function reads the content of data file through
/// explicit read operations.  It sequentially reads the content of the
/// data file.  Note the content of the data file is assumed to be sorted
/// in ascending order as elementary data type T.
template<typename T>
int ibis::column::searchSortedOOCD(const char* fname,
                                   const ibis::qUIntHod& rng,
                                   ibis::bitvector& hits) const {
    const ibis::array_t<uint64_t>& u = rng.getValues();
    std::string evt = "column::searchSortedOOCD";
    if (ibis::gVerbose > 4) {
        std::ostringstream oss;
        oss << "column[" << fullname() << "]::searchSortedOOCD<"
            << typeid(T).name() << ">(" << fname << ", " << rng.colName()
            << " IN " << u.size() << "-element list)";
        evt = oss.str();
    }
    ibis::util::timer mytimer(evt.c_str(), 5);
    int fdes = UnixOpen(fname, OPEN_READONLY);
    if (fdes < 0) {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- " << evt << " failed to "
            << "open the named data file, errno = " << errno
            << strerror(errno);
        return -1;
    }
#if defined(_WIN32) && defined(_MSC_VER)
    (void)_setmode(fdes, _O_BINARY);
#endif
    IBIS_BLOCK_GUARD(UnixClose, fdes);

    const uint32_t sz = sizeof(T);
    int ierr = UnixSeek(fdes, 0, SEEK_END);
    if (ierr < 0) {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- " << evt << " failed to seek to the end of file";
        return -2;
    }
    ibis::fileManager::instance().recordPages(0, ierr);
    const uint32_t nrows = ierr / sz;
    ibis::fileManager::buffer<T> buf;
    hits.clear();
    hits.reserve(nrows, u.size()); // reserve space
    ierr = UnixSeek(fdes, 0, SEEK_SET); // point to the beginning of file
    if (buf.size() > 0) { // has a buffer to use
        uint32_t ju = 0;
        uint32_t jv = 0;
        while (ju < u.size() && 0 < 
               (ierr = ibis::util::read(fdes, buf.address(), buf.size()*sz))) {
            for (uint32_t j = 0; ju < u.size() && j < buf.size(); ++ j) {
                while (ju < u.size() && u[ju] < (uint64_t)buf[j]) ++ ju;
                if ((uint64_t)buf[j] == u[ju]) {
                    hits.setBit(jv+j, 1);
                }
            }
            jv += ierr / sz;
        }
    }
    else { // read one value at a time
        T tmp;
        uint64_t itmp;
        uint32_t ju = 0;
        uint32_t jv = 0;
        while (ju < u.size() &&
               (ierr = UnixRead(fdes, &tmp, sizeof(tmp))) > 0) {
            itmp = (uint64_t) tmp;
            if ((T)itmp == tmp) {
                while (ju < u.size() && u[ju] < itmp) ++ ju;
                if (u[ju] == itmp)
                    hits.setBit(jv, 1);
            }
            ++ jv;
        }
    }

    hits.adjustSize(0, nrows);
    return (ierr > 0 ? 0 : -3);
} // ibis::column::searchSortedOOCD

/// Constructor of index lock.  Must have a valid column object as argument
/// 1.  This class could do nothing without a valid column object.
ibis::column::indexLock::indexLock(const ibis::column* col, const char* m)
    : theColumn(col), mesg(m) {
    bool toload = false;
    if (col != 0) {
        ibis::column::readLock lk(col, m);
        // only attempt to build the index if idxcnt is zero and idx is zero
        toload = (theColumn->idxcnt() == 0 &&
                  (theColumn->idx == 0 || theColumn->idx->empty()));
    }
    else {
        return;
    }

    if (toload)
        theColumn->loadIndex();
    if (theColumn->idx != 0) {
        int ierr = pthread_rwlock_rdlock(&(col->rwlock));
        std::string evt = "column[";
        evt += theColumn->fullname();
        evt += "]::indexLock";
        if (0 != ierr) {
            LOGGER(ibis::gVerbose >= 0)
                << "Warning -- " << evt << " -- pthread_rwlock_rdlock("
                << static_cast<const void*>(&(theColumn->rwlock)) << ") for "
                << mesg << " returned " << ierr << " (" << strerror(ierr)
                << ')';
        }
        else {
            LOGGER(ibis::gVerbose > 9)
                << evt << " -- pthread_rwlock_rdlock("
                << static_cast<const void*>(&(theColumn->rwlock)) << ") for "
                << mesg;
        }

        ++ theColumn->idxcnt; // increment the counter
    }
}

/// Destructor of index lock.
ibis::column::indexLock::~indexLock() {
    if (theColumn->idx != 0) {
        -- (theColumn->idxcnt); // decrement counter

        int ierr = pthread_rwlock_unlock(&(theColumn->rwlock));
        std::string evt = "column[";
        evt += theColumn->fullname();
        evt += "]::~indexLock";
        if (0 != ierr) {
            LOGGER(ibis::gVerbose >= 0)
                << "Warning -- " << evt << " -- pthread_rwlock_unlock("
                << static_cast<const void*>(&(theColumn->rwlock)) << ") for "
                << mesg << " returned " << ierr << " (" << strerror(ierr)
                << ')';
        }
        else {
            LOGGER(ibis::gVerbose > 9)
                << evt << " -- pthread_rwlock_unlock("
                << static_cast<const void*>(&(theColumn->rwlock)) << ") for "
                << mesg;
        }
    }
}

/// Constructor.  No error checking, both incoming arguments must be valid.
ibis::column::readLock::readLock(const ibis::column* col, const char* m)
    : theColumn(col), mesg(m) {
    int ierr = pthread_rwlock_rdlock(&(col->rwlock));
    if (0 != ierr) {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- column[" << theColumn->fullname()
            << "]::readLock -- pthread_rwlock_rdlock("
            << static_cast<const void*>(&(theColumn->rwlock)) << ") for "
            << mesg << " returned " << ierr << " (" << strerror(ierr) << ')';
    }
    else {
        LOGGER(ibis::gVerbose > 9)
            << "column[" << theColumn->fullname()
            << "]::readLock -- pthread_rwlock_rdlock("
            << static_cast<const void*>(&(theColumn->rwlock)) << ") for "
            << mesg;
    }
}

/// Destructor.
ibis::column::readLock::~readLock() {
    int ierr = pthread_rwlock_unlock(&(theColumn->rwlock));
    if (0 != ierr) {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- column[" << theColumn->fullname()
            << "]::readLock -- pthread_rwlock_unlock("
            << static_cast<const void*>(&(theColumn->rwlock)) << ") for "
            << mesg << " returned " << ierr << " (" << strerror(ierr) << ')';
    }
    else {
        LOGGER(ibis::gVerbose > 9)
            << "column[" << theColumn->fullname()
            << "]::readLock -- pthread_rwlock_unlock("
            << static_cast<const void*>(&(theColumn->rwlock)) << ") for "
            << mesg;
    }
}

/// Constructor.  No error checking, both incoming arguments must be valid.
ibis::column::writeLock::writeLock(const ibis::column* col, const char* m)
    : theColumn(col), mesg(m) {
    int ierr = pthread_rwlock_wrlock(&(col->rwlock));
    if (0 != ierr) {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- column[" << theColumn->fullname()
            << "]::writeLock -- pthread_rwlock_wrlock("
            << static_cast<const void*>(&(theColumn->rwlock)) << ") for "
            << mesg << " returned " << ierr << " (" << strerror(ierr) << ')';
    }
    else {
        LOGGER(ibis::gVerbose > 9)
            << "column[" << theColumn->fullname()
            << "]::writeLock -- pthread_rwlock_wrlock("
            << static_cast<const void*>(&(theColumn->rwlock)) << ") for "
            << mesg;
    }
}

/// Destructor.
ibis::column::writeLock::~writeLock() {
    int ierr = pthread_rwlock_unlock(&(theColumn->rwlock));
    if (0 != ierr) {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- column[" << theColumn->fullname() << '.'
            << "]::writeLock -- pthread_rwlock_unlock("
            << static_cast<const void*>(&(theColumn->rwlock)) << ") for "
            << mesg << " returned " << ierr << " (" << strerror(ierr) << ')';
    }
    else {
        LOGGER(ibis::gVerbose > 9)
            << "column[" << theColumn->fullname()
            << "]::writeLock -- pthread_rwlock_unlock("
            << static_cast<const void*>(&(theColumn->rwlock)) << ") for "
            << mesg;
    }
}

/// Constructor.  No argument checking, both incoming arguments must be valid.
ibis::column::softWriteLock::softWriteLock(const ibis::column* col,
                                           const char* m)
    : theColumn(col), mesg(m),
      locked(pthread_rwlock_trywrlock(&(col->rwlock))) {
    if (0 != locked) {
        LOGGER(ibis::gVerbose > 2)
            << "Warning -- column[" << theColumn->fullname()
            << "]::softWriteLock -- pthread_rwlock_trywrlock("
            << static_cast<const void*>(&(theColumn->rwlock)) << ") for "
            << mesg << " returned " << locked << " (" << strerror(locked)
            << ')';
    }
    else {
        LOGGER(ibis::gVerbose > 9)
            << "column[" << theColumn->fullname()
            << "]::softWriteLock -- pthread_rwlock_trywrlock("
            << static_cast<const void*>(&(theColumn->rwlock)) << ") for "
            << mesg;
    }
}

/// Destructor.
ibis::column::softWriteLock::~softWriteLock() {
    if (locked == 0) {
        int ierr = pthread_rwlock_unlock(&(theColumn->rwlock));
        if (0 != ierr) {
            LOGGER(ibis::gVerbose >= 0)
                << "Warning -- column[" << theColumn->fullname()
                << "]::softWriteLock -- pthread_rwlock_unlock("
                << static_cast<const void*>(&(theColumn->rwlock)) << ") for "
                << mesg << " returned " << ierr << " (" << strerror(ierr)
                << ')';
        }
        else {
            LOGGER(ibis::gVerbose > 9)
                << "column[" << theColumn->fullname()
                << "]::softWriteLock -- pthread_rwlock_unlock("
                << static_cast<const void*>(&(theColumn->rwlock)) << ") for "
                << mesg;
        }
    }
}

/// Constructor.
ibis::column::info::info(const ibis::column& col)
    : name(col.name()), description(col.description()),
      expectedMin(col.lowerBound()),
      expectedMax(col.upperBound()), type(col.type()) {
    if (expectedMin > expectedMax) {
        const_cast<ibis::column&>(col).computeMinMax();
        const_cast<double&>(expectedMin) = col.lowerBound();
        const_cast<double&>(expectedMax) = col.upperBound();
    }
}

// explicit template instantiation
template long ibis::column::selectValuesT
(const char*, const bitvector&, array_t<unsigned char>&,
 array_t<uint32_t>&) const;
template long ibis::column::selectValuesT
(const char*, const bitvector&, array_t<signed char>&,
 array_t<uint32_t>&) const;
template long ibis::column::selectValuesT
(const char*, const bitvector&, array_t<char>&, array_t<uint32_t>&) const;
template long ibis::column::selectValuesT
(const char*, const bitvector&, array_t<uint16_t>&, array_t<uint32_t>&) const;
template long ibis::column::selectValuesT
(const char*, const bitvector&, array_t<int16_t>&, array_t<uint32_t>&) const;
template long ibis::column::selectValuesT
(const char*, const bitvector&, array_t<uint32_t>&, array_t<uint32_t>&) const;
template long ibis::column::selectValuesT
(const char*, const bitvector&, array_t<int32_t>&, array_t<uint32_t>&) const;
template long ibis::column::selectValuesT
(const char*, const bitvector&, array_t<uint64_t>&, array_t<uint32_t>&) const;
template long ibis::column::selectValuesT
(const char*, const bitvector&, array_t<int64_t>&, array_t<uint32_t>&) const;
template long ibis::column::selectValuesT
(const char*, const bitvector&, array_t<float>&, array_t<uint32_t>&) const;
template long ibis::column::selectValuesT
(const char*, const bitvector&, array_t<double>&, array_t<uint32_t>&) const;

template long ibis::column::castAndWrite
(const array_t<double>& vals, ibis::bitvector& mask, const char special);
template long ibis::column::castAndWrite
(const array_t<double>& vals, ibis::bitvector& mask,
 const unsigned char special);
template long ibis::column::castAndWrite
(const array_t<double>& vals, ibis::bitvector& mask, const int16_t special);
template long ibis::column::castAndWrite
(const array_t<double>& vals, ibis::bitvector& mask, const uint16_t special);
template long ibis::column::castAndWrite
(const array_t<double>& vals, ibis::bitvector& mask, const int32_t special);
template long ibis::column::castAndWrite
(const array_t<double>& vals, ibis::bitvector& mask, const uint32_t special);
template long ibis::column::castAndWrite
(const array_t<double>& vals, ibis::bitvector& mask, const int64_t special);
template long ibis::column::castAndWrite
(const array_t<double>& vals, ibis::bitvector& mask, const uint64_t special);
template long ibis::column::castAndWrite
(const array_t<double>& vals, ibis::bitvector& mask, const float special);
template long ibis::column::castAndWrite
(const array_t<double>& vals, ibis::bitvector& mask, const double special);

/// Format the integer value @c ut assuming it is representing a unix time
/// stamp.
///
/// @note The unix time is measured as seconds since the beginning of 1970.
/// The original definition uses a 32-bit integer.  This funciton uses a
/// 64-bit integer to provide a large range.
void ibis::column::unixTimeScribe::operator()
    (std::ostream &out, int64_t ut) const {
    char buf[80];
    const time_t tt = static_cast<time_t>(ut);
#if (defined(__MINGW__) || defined(__MINGW32__) || defined(__MINGW64__) || defined(_MSC_VER)) && defined(_WIN32)
    struct tm *mytm;
    if (timezone_ != 0 && (*timezone_ == 'g' || *timezone_ == 'G' ||
                           *timezone_ == 'u' || *timezone_ == 'U'))
        mytm = gmtime(&tt);
    else
        mytm = localtime(&tt);
    (void) strftime(buf, 80, format_, mytm);
#else
    struct tm mytm;
    if (timezone_ != 0 && (*timezone_ == 'g' || *timezone_ == 'G' ||
                           *timezone_ == 'u' || *timezone_ == 'U'))
        (void) gmtime_r(&tt, &mytm);
    else
        (void) localtime_r(&tt, &mytm);
    (void) strftime(buf, 80, format_, &mytm);
#endif
    out << buf;
} // ibis::column::unixTimeScribe::operator()

void ibis::column::unixTimeScribe::operator()
    (std::ostream &out, double ut) const {
    char buf[80];
    const time_t tt = static_cast<time_t>(ut);
#if (defined(__MINGW__) || defined(__MINGW32__) || defined(__MINGW64__) || defined(_MSC_VER)) && defined(_WIN32)
    struct tm *mytm;
    if (timezone_ != 0 && (*timezone_ == 'g' || *timezone_ == 'G' ||
                           *timezone_ == 'u' || *timezone_ == 'U'))
        mytm = gmtime(&tt);
    else
        mytm = localtime(&tt);
    (void) strftime(buf, 80, format_, mytm);
#else
    struct tm mytm;
    if (timezone_ != 0 && (*timezone_ == 'g' || *timezone_ == 'G' ||
                           *timezone_ == 'u' || *timezone_ == 'U'))
        (void) gmtime_r(&tt, &mytm);
    else
        (void) localtime_r(&tt, &mytm);
    (void) strftime(buf, 80, format_, &mytm);
#endif
    out << buf;
} // ibis::column::unixTimeScribe::operator()

