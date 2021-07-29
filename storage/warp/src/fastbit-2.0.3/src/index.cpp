// $Id$
// Author: John Wu <John.Wu at ACM.org>
// Copyright (c) 2000-2016 the Regents of the University of California
//
// This file contains the implementation of the classes defined in index.h
// The primary function from the database point of view is a function
// called estimate.  It evaluates a given range condition and produces
// two bit vectors representing the range where the actual solution lies.
// The bulk of the code is devoted to maintain and update the indexes.
//
#if defined(_WIN32) && defined(_MSC_VER)
#pragma warning(disable:4786)   // some identifier longer than 256 characters
#endif
#include "index.h"
#include "ibin.h"
#include "irelic.h"
#include "ikeywords.h"
#include "part.h"
#include "category.h"
#include "resource.h"
#include "bitvector64.h"

#include <memory>       // std::unique_ptr
#include <queue>        // priority queue
#include <algorithm>    // std::sort
#include <sstream>      // std::ostringstream
#include <typeinfo>     // typeid

namespace ibis {
#if defined(TEST_SUMBINS_OPTIONS)
    //a temporary variable for testing the various options in sumBits
    extern int _sumBits_option;
#endif
}

namespace std { // specialize the std::less struct
    template <> struct less<std::pair<ibis::bitvector*, bool> > {
        bool operator()
        (const std::pair<ibis::bitvector*, bool> &x,
         const std::pair<ibis::bitvector*, bool> &y) const {
            return (x.first->bytes() > y.first->bytes());
        }
    };

    template <> struct less<ibis::bitvector* > {
        bool operator()(const ibis::bitvector *x,
                        const ibis::bitvector *y) const {
            return (x->bytes() < y->bytes());
        }
    };
}

////////////////////////////////////////////////////////////////////////
// functions from ibis::index

/// Index factory.  It creates a specific concrete index object.  It
/// attempts to read the existing index if a location is specified.  If it
/// fails to read an index or no expilicit location is given, it attempts
/// to create a new index based on the current data file and index
/// specification.  Any newly created index will be written to a file.
///
/// @param c a pointer to a ibis::column object.  This argument must be
/// present.
///
/// @param dfname data file name, may also be the name of the index file,
/// or the directory containing the data file.  If the name ends with
/// '.idx' is treated as an index file, and the content of the file is
/// read.  If the name does not end with '.idx', it is assumed to be the
/// data file name, unless it is determined to be a directory name.  If it
/// is a directory name, the data file is assumed to be in the specified
/// directory with the same name as the column.  Once a data file is found,
/// the content of the data file is read to construct a new index according
/// to the return value of function indexSpec.  The argument dfname can be
/// nil, in which case, the data file name is constructed by concatenate
/// the return from partition()->currentDataDir() and the column name.
///
/// @param spec the index specification.  This string contains the
/// parameters for how to create an index.  The most general form is
///\verbatim
/// <binning .../> <encoding .../> <compression .../>.
///\endverbatim
/// Here is one example (it is the default for some integer columns)
///\verbatim
/// <binning none /> <encoding equality />
///\endverbatim
/// FastBit always compresses every bitmap it ever generates.  The
/// compression option is to instruct it to uncompress some bitmaps or
/// not compress indexes at all.  The compress option is usually not
/// used.
///
/// If the argument @c spec is not specified, this function checks the
/// specification in the following order.
///
/// -- use the index specification for the column being indexed;
/// -- use the index specification for the table containing the
///    column being indexed;
/// -- use the most specific index specification relates to the
///    column be indexed in the global resources (gParameters).
///
/// It stops looking as soon as it finds the first non-empty string, which
/// follows the principle of a more specific index specification override a
/// general specification.
///
/// @param readopt Depending on whether this value is positive, zero or
/// negative, the index is read from the index file in three different
/// ways.
///
/// (1) If this value is positive, the content of the index file is read
/// into memory and there is no need for further I/O operation to use the
/// index.
///
/// (2) If this value is zero, the content of a large index file is loaded
/// into memory through memory map and the content of a small index file
/// will be read into memory.  This is the default option.
///
/// (3) If this value is negative, then only the metadata is read into
/// memory.  This option requires the least amount of memory, but requires
/// more I/O operations later when bitmaps are needed to answer queries.
/// To use option (1) and (2), there must be enough memory to hold the
/// index file in memory.  Furthermore, to use the memory map option,
/// FastBit must be able to hold the index file open indefinitely and the
/// operating system must support memory map function mmap.
///
/// These three options have different start up cost and different query
/// processing cost.  Typically, reading the whole file in this function
/// will take the longest time, but since it requires no further I/O
/// operations, its query processing cost is likely the lowest.  The memory
/// map option only need to load the page table into memory and read part
/// of the metadata, it is likely to be relatively inexpensive to
/// reconstruct the index object this way.  Since the memory map option can
/// read the necessary portion of the index file into memory pretty
/// efficiently, the query processing cost should have reasonable
/// performance.  The third of reading metadata only in this function
/// requires the least amount of memory, but it might actually read more
/// bytes in this function than the memory map option because this option
/// actually needs to read all the bytes representing the metadata while
/// the memory map option only need to create a memory map for the index
/// file.  Later when the index is used, the needed bitmaps are read into
/// memory, which is likely take more time than accessing the memory mapped
/// bitmaps.  Additionally, the third option also causes the bitmaps to be
/// placed in unpredictable memory locations, while the first two options
/// place all bitmaps of an index consecutively in memory.  This difference
/// in memory layout could cause the memory accesses to take different
/// amounts of time; typically, accessing consecutive memory locations is
/// more efficient.
///
/// The default value of @c readopt is 0, which prefers the memory map
/// option.
///
/// @return This function returns a pointer to the index created.  The
/// caller is responsible for freeing the pointer.  In case of error, it
/// returns a nil pointer.  It captures and absorbs exceptions in most
/// cases.
///
/// @note An index can NOT be built correctly if it does not fit in memory!
/// This is the most likely reason for the function to fail.  If this does
/// happen, try to build indexes one at a time, use a machine with more
/// memory, or break up a large partition into a number of smaller ones.
/// Normally, we recommand one to not put more than 100 million rows in a
/// data partition.
///
/// @note Set @c dfname to null to build a brand new index and discard
/// the existing index.
///
/// @note The index specification passed to this function will be attached
/// to the column object if a new index is to be built.  This is the only
/// possible change to the column object.
ibis::index* ibis::index::create(const ibis::column* c, const char* dfname,
                                 const char* spec, int readopt) {
    ibis::index* ind = 0;
    int ierr;
    bool isRead = false;
    std::string evt = "index::create";
    if (ibis::gVerbose > 0) {
        evt += '(';
        if (c != 0)
            evt += c->fullname();
        else
            evt += '?';
        evt += ')';
    }

    if (dfname != 0 && *dfname != 0) { // first attempt to read the index
        ibis::fileManager::storage* st=0;
        std::string file;
        const char* header = 0;
        char buf[12];
        const size_t dfnlen = std::strlen(dfname);
        if (dfnlen > 4 && dfname[dfnlen-4] == '.' &&
            dfname[dfnlen-3] == 'i' && dfname[dfnlen-2] == 'd' &&
            dfname[dfnlen-1] == 'x') {
            file = dfname;
        }
        else if (c != 0) {
            c->dataFileName(file, dfname);
            if (! file.empty())
                file += ".idx";
        }
        else {
            file = dfname;
            LOGGER(ibis::gVerbose > 1)
                << evt << " is to attempt to read " << dfname
                << ") as an index file";
        }
        if (! file.empty()) {
            bool useGetFile = (readopt >= 0);
            ibis::fileManager::ACCESS_PREFERENCE prf =
                (readopt > 0 ? ibis::fileManager::PREFER_READ :
                 ibis::fileManager::MMAP_LARGE_FILES);
            if (readopt == 0) { // default option, check parameters
                std::string key;
                if (c != 0) {
                    if (c->partition() != 0) {
                        key = c->partition()->name();
                        key += ".";
                    }
                    key += c->name();
                    key += ".preferMMapIndex";
                }
                else {
                    key = "preferMMapIndex";
                }
                if (ibis::gParameters().isTrue(key.c_str())) {
                    useGetFile = true;
                    prf = ibis::fileManager::PREFER_MMAP;
                }
                else {
                    key.erase(key.size()-9);
                    key += "ReadIndex";
                    if (ibis::gParameters().isTrue(key.c_str())) {
                        useGetFile = true;
                        prf = ibis::fileManager::PREFER_READ;
                    }
                }
            }
            if (useGetFile) {
                // manage the index file as a whole
                ierr = ibis::fileManager::instance().tryGetFile
                    (file.c_str(), &st, prf);
                if (ierr != 0) {
                    LOGGER(ibis::gVerbose > 6)
                        << evt << " tryGetFile(" << file
                        << ") failed with return code " << ierr;
                    st = 0;
                }
                if (st)
                    header = st->begin();
            }
            if (header == 0) {
                // attempt to read the file using read(2)
                int fdes = UnixOpen(file.c_str(), OPEN_READONLY);
                if (fdes >= 0) {
#if defined(_WIN32) && defined(_MSC_VER)
                    (void)_setmode(fdes, _O_BINARY);
#endif
                    if (8 == UnixRead(fdes, static_cast<void*>(buf), 8)) {
                        header = buf;
                    }
                    UnixClose(fdes);
                }
            }
            if (header) { // verify header
                const bool check = (header[0] == '#' && header[1] == 'I' &&
                                    header[2] == 'B' && header[3] == 'I' &&
                                    header[4] == 'S' &&
                                    (header[6] == 8 || header[6] == 4) &&
                                    header[7] == static_cast<char>(0));
                if (!check) {
                    if (ibis::gVerbose > 0) {
                        ibis::util::logger lg;
                        lg()  << "Warning -- index file \"" << file
                              << "\" contains an incorrect header (";
                        printHeader(lg(), header);
                        lg() << ')';
                    }
                    header = 0;
                }
            }

            if (header) { // reconstruct index from st
                isRead = true;
                ibis::horometer tm4;
                if (ibis::gVerbose > 2)
                    tm4.start();
                ind = readOld(c, file.c_str(), st,
                              static_cast<INDEX_TYPE>(header[5]));
                if (ind == 0) {
                    LOGGER(ibis::gVerbose > 0)
                        << "Warning -- " << evt
                        << " did not read an index from " << file;
                    ibis::fileManager::instance().flushFile(file.c_str());
                    if (c != 0) {
                        LOGGER(ibis::gVerbose > 0)
                            << evt << " will remove the index file " << file
                            << " and try to build a new index from data";
                        (void) remove(file.c_str());
                    }
                }
                else if (ibis::gVerbose > 2) {
                    tm4.stop();
                    LOGGER(1) << evt << " reading the existing index took "
                              << tm4.realTime() << " sec";
                }
            }
        }
    } // if (dfname != 0 && *dfname != 0)
    if (ind != 0) // successfully read an index
        return ind;

    // could not read an index, try to create a new one
    if (c == 0) // can not proceed
        return ind;
    if (c->partition() != 0 && c->partition()->nRows() == 0)
        return ind;
    if (c->type() == ibis::UNKNOWN_TYPE || c->type() == ibis::BLOB ||
        c->type() == ibis::BIT)
        return ind;

    if (spec == 0 || *spec == static_cast<char>(0))
        spec = c->indexSpec(); // index spec of the column
    if ((spec == 0 || *spec == static_cast<char>(0)) && c->partition() != 0)
        spec = c->partition()->indexSpec(); // index spec of the table
    if (spec == 0 || *spec == static_cast<char>(0)) {
        // attempt to retrieve the value of tableName.columnName.index for
        // the index specification in the global resource
        std::string idxnm;
        if (c->partition() != 0) {
            idxnm = c->partition()->name();
            idxnm += '.';
        }
        idxnm += c->name();
        idxnm += ".index";
        spec = ibis::gParameters()[idxnm.c_str()];
    }
    if (spec) {
        // skip leading spaces
        while (spec && isspace(*spec)) ++ spec;
        // no index is to be used if the index specification start
        // with "noindex", "null" or "none".
        if (strncmp(spec, "noindex", 7) == 0 ||
            strncmp(spec, "null", 4) == 0 ||
            strncmp(spec, "none", 4) == 0) {
            return ind;
        }
    }
    ibis::horometer timer;
    if (ibis::gVerbose > 1)
        timer.start();

    try {
        if (dfname == 0) {
            // user has passed in an explicit nil pointer, purge index files
            c->purgeIndexFile();
        }

        if (ind == 0) {
            isRead = false;
            ibis::horometer tm3;
            if (ibis::gVerbose > 2)
                tm3.start();
            ind = buildNew(c, dfname, spec);
            if (ind != 0 && ibis::gVerbose > 2) {
                tm3.stop();
                LOGGER(1) << evt << " building a new index took "
                          << tm3.realTime() << " sec";
            }
        }
        if (ind == 0) {
            LOGGER(ibis::gVerbose > 0)
                << evt << " failed to create an index for " << c->name();
        }
        else if (ind->getNRows() == 0) {
            delete ind;
            ind = 0;
            LOGGER(ibis::gVerbose > 0)
                << evt << " create an empty index for " << c->name();
        }
        else if (c->partition() == 0 ||
                 ind->getNRows() == c->partition()->nRows()) {
            // having built a valid index, write out its content
            try {
                if (! isRead) {
                    ibis::horometer tm2;
                    if (ibis::gVerbose > 2)
                        tm2.start();
                    ierr = ind->write(dfname);
                    if (ierr >= 0 && ibis::gVerbose > 2) {
                        tm2.stop();
                        LOGGER(1) << evt << " writing the index took "
                                  << tm2.realTime() << " sec";
                    }
                }
                else {
                    ierr = 0;
                }
                if (ierr < 0) {
                    std::string idxname;
                    ind->indexFileName(idxname, dfname);
                    remove(idxname.c_str());
                    LOGGER(ibis::gVerbose > 0)
                        << "Warning -- " << evt
                        << " failed to write the index ("
                        << ind->name() << ") to " << idxname << ", ierr = "
                        << ierr;
                }
            }
            catch (...) {
                std::string idxname;
                ind->indexFileName(idxname, dfname);
                remove(idxname.c_str());
                LOGGER(ibis::gVerbose > 0)
                    << "Warning -- " << evt
                    << " failed to write the index (" << ind->name()
                    << ") to " << idxname << ", received an exception";
            }
        }
        else {
            LOGGER(ibis::gVerbose > 0)
                << evt << " created an index with "
                << ind->getNRows() << " row"
                << (ind->getNRows() > 1 ? "s" : "")
                << ", but the data partition has "
                << c->partition()->nRows() << " row"
                << (c->partition()->nRows() > 1 ? "s" : "");
        }
    }
    catch (const char* s) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " received a string exception -- " << s;
        delete ind;
        ind = 0;
    }
    catch (const ibis::bad_alloc& e) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " failed to allocate memory -- "
            << e.what();
        delete ind;
        ind = 0;
    }
    catch (const std::exception& e) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " received an std::exception -- "
            << e.what();
        delete ind;
        ind = 0;
    }
    catch (...) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " received a unexpected exception";
        delete ind;
        ind = 0;
    }

    if (ind == 0) {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- " << evt << " failed to create an index of type "
            << (spec != 0 && *spec != 0 ? spec : "default");
    }
    else if (ibis::gVerbose > 1) {
        timer.stop();
        ibis::util::logger lg;
        lg() << evt << " -- the " << ind->name() << " index for column "
             << c->fullname();
        if (isRead) {
            lg() << " was read from " << dfname;
        }
        else if (c->partition() != 0 && c->partition()->currentDataDir() != 0) {
            lg() << " was created from data in "
                 << c->partition()->currentDataDir();
        }
        else {
            lg() << " was created from in-memory data";
        }
        lg() << " in " << timer.CPUTime() <<" sec(CPU), "<< timer.realTime()
             << " sec(elapsed)";
        if (ibis::gVerbose > 3) {
            lg() << "\n";
            ind->print(lg());
        }
    }
    return ind;
} // ibis::index::create

/// Read an index of the specified type from the incoming data file.  The
/// index type t has been determined by the caller.  Furthermore, the
/// caller might have read the index file into storage object st.
ibis::index* ibis::index::readOld(const ibis::column *c,
                                  const char *f,
                                  ibis::fileManager::storage *st,
                                  ibis::index::INDEX_TYPE t) {
    ibis::index *ind = 0;
    if (f == 0 && *f == 0 && st == 0 && c->partition() == 0) return ind;
    LOGGER(ibis::gVerbose > 3)
        << "index::create -- attempt to read index type #"
        << (int)t << " from "
        << (f ? f : c->partition()->currentDataDir())
        << " for column " << (c ? c->fullname() : "?");
    switch (t) {
    case ibis::index::BINNING: // ibis::bin
        if (st) {
            ind = new ibis::bin(c, st);
        }
        else {
            ind = new ibis::bin(0);
            ind->col = c;
            int ierr = ind->read(f);
            if (ierr < 0) {
                delete ind;
                ind = 0;
            }
        }
        break;
    case ibis::index::RANGE: // new range index
        if (st) {
            ind = new ibis::range(c, st);
        }
        else {
            ind = new ibis::range(0);
            ind->col = c;
            int ierr = ind->read(f);
            if (ierr < 0) {
                delete ind;
                ind = 0;
            }
        }
        break;
    case ibis::index::AMBIT: // multilevel range index
        if (st) {
            ind = new ibis::ambit(c, st);
        }
        else {
            ind = new ibis::ambit(0);
            ind->col = c;
            int ierr = ind->read(f);
            if (ierr < 0) {
                delete ind;
                ind = 0;
            }
        }
        break;
    case ibis::index::PALE: // two-level bin/range index
        if (st) {
            ind = new ibis::pale(c, st);
        }
        else {
            ind = new ibis::pale(0);
            ind->col = c;
            int ierr = ind->read(f);
            if (ierr < 0) {
                delete ind;
                ind = 0;
            }
        }
        break;
    case ibis::index::PACK: // two-level range/bin index
        if (st) {
            ind = new ibis::pack(c, st);
        }
        else {
            ind = new ibis::pack(0);
            ind->col = c;
            int ierr = ind->read(f);
            if (ierr < 0) {
                delete ind;
                ind = 0;
            }
        }
        break;
    case ibis::index::ZONE: // two-level bin/bin index
        if (st) {
            ind = new ibis::zone(c, st);
        }
        else {
            ind = new ibis::zone(0);
            ind->col = c;
            int ierr = ind->read(f);
            if (ierr < 0) {
                delete ind;
                ind = 0;
            }
        }
        break;
    case ibis::index::MESA: // one-level (interval) encoded index
        if (st) {
            ind = new ibis::mesa(c, st);
        }
        else {
            ind = new ibis::mesa(0);
            ind->col = c;
            int ierr = ind->read(f);
            if (ierr < 0) {
                delete ind;
                ind = 0;
            }
        }
        break;
    case ibis::index::RELIC: // the basic bitmap index
        if (st) {
            ind = new ibis::relic(c, st);
        }
        else {
            ind = new ibis::relic(c, f);
        }
        break;
    case ibis::index::SKIVE: // binary encoded index
        if (st) {
            ind = new ibis::skive(c, st);
        }
        else {
            ind = new ibis::skive(0);
            ind->col = c;
            int ierr = ind->read(f);
            if (ierr < 0) {
                delete ind;
                ind = 0;
            }
        }
        break;
    case ibis::index::SLICE: // bit-slice
        if (st) {
            ind = new ibis::slice(c, st);
        }
        else {
            ind = new ibis::slice(0);
            ind->col = c;
            int ierr = ind->read(f);
            if (ierr < 0) {
                delete ind;
                ind = 0;
            }
        }
        break;
    case ibis::index::FADE: // multicomponent range-encoded
        if (st) {
            ind = new ibis::fade(c, st);
        }
        else {
            ind = new ibis::fade(0);
            ind->col = c;
            int ierr = ind->read(f);
            if (ierr < 0) {
                delete ind;
                ind = 0;
            }
        }
        break;
    case ibis::index::SAPID: // multicomponent equality-encoded
        if (st) {
            ind = new ibis::sapid(c, st);
        }
        else {
            ind = new ibis::sapid(0);
            ind->col = c;
            int ierr = ind->read(f);
            if (ierr < 0) {
                delete ind;
                ind = 0;
            }
        }
        break;
    case ibis::index::SBIAD: // multicomponent interval-encoded
        if (st) {
            ind = new ibis::sbiad(c, st);
        }
        else {
            ind = new ibis::sbiad(0);
            ind->col = c;
            int ierr = ind->read(f);
            if (ierr < 0) {
                delete ind;
                ind = 0;
            }
        }
        break;
    case ibis::index::EGALE:
        // multicomponent equality code on bins
        if (st) {
            ind = new ibis::egale(c, st);
        }
        else {
            ind = new ibis::egale(0);
            ind->col = c;
            int ierr = ind->read(f);
            if (ierr < 0) {
                delete ind;
                ind = 0;
            }
        }
        break;
    case ibis::index::MOINS:
        // multicomponent equality code on bins
        if (st) {
            ind = new ibis::moins(c, st);
        }
        else {
            ind = new ibis::moins(0);
            ind->col = c;
            int ierr = ind->read(f);
            if (ierr < 0) {
                delete ind;
                ind = 0;
            }
        }
        break;
    case ibis::index::ENTRE:
        // multicomponent equality code on bins
        if (st) {
            ind = new ibis::entre(c, st);
        }
        else {
            ind = new ibis::entre(0);
            ind->col = c;
            int ierr = ind->read(f);
            if (ierr < 0) {
                delete ind;
                ind = 0;
            }
        }
        break;
    case ibis::index::BAK:
        // equality code on reduced precision values
        if (st) {
            ind = new ibis::bak(c, st);
        }
        else {
            ind = new ibis::bak(0);
            ind->col = c;
            int ierr = ind->read(f);
            if (ierr < 0) {
                delete ind;
                ind = 0;
            }
        }
        break;
    case ibis::index::BAK2:
        // equality code on reduced precision values, split bins
        if (st) {
            ind = new ibis::bak2(c, st);
        }
        else {
            ind = new ibis::bak2(0);
            ind->col = c;
            int ierr = ind->read(f);
            if (ierr < 0) {
                delete ind;
                ind = 0;
            }
        }
        break;
    case ibis::index::KEYWORDS:
        // boolean term-document matrix
        if (st) {
            ind = new ibis::keywords(c, st);
        }
        else {
            ind = new ibis::keywords(c, f);
        }
        break;
    case ibis::index::DIREKTE:
        if (st) {
            ind = new ibis::direkte(c, st);
        }
        else {
            ind = new ibis::direkte(0);
            ind->col = c;
            int ierr = ind->read(f);
            if (ierr < 0) {
                delete ind;
                ind = 0;
            }
        }
        break;
    case ibis::index::BYLT:
        if (st) {
            ind = new ibis::bylt(c, st);
        }
        else {
            ind = new ibis::bylt(c, f);
        }
        break;
    case ibis::index::ZONA:
        if (st) {
            ind = new ibis::zona(c, st);
        }
        else {
            ind = new ibis::zona(c, f);
        }
        break;
    case ibis::index::FUZZ:
        if (st) {
            ind = new ibis::fuzz(c, st);
        }
        else {
            ind = new ibis::fuzz(c, f);
        }
        break;
    case ibis::index::FUGE:
        if (st) {
            ind = new ibis::fuge(c, st);
        }
        else {
            ind = new ibis::fuge(c, f);
        }
        break;
    default:
        LOGGER(ibis::gVerbose > 1)
            << "Warning -- index::create can not process index type "
            << (int)t << " from " << f;
    }
    return ind;
} // ibis::index::readOld

/// Build a new index from attribute values.
ibis::index* ibis::index::buildNew
(const ibis::column *c, const char* dfname, const char* spec) {
    if (c->type() == ibis::CATEGORY) {
        // special handling
        return reinterpret_cast<const ibis::category*>(c)->
            fillIndex(dfname);
        //return ind;
    }
    else if (c->type() == ibis::TEXT) {
        if (spec != 0 && *spec != 0)
            const_cast<column*>(c)->indexSpec(spec);
        return new ibis::keywords(c, dfname);
    }
    if (spec == 0 || *spec == 0) {
        switch (c->type()) {
        case ibis::USHORT:
        case ibis::SHORT:
        case ibis::UBYTE:
        case ibis::BYTE:
        case ibis::UINT:
        case ibis::INT:
        case ibis::OID:
        case ibis::ULONG:
        case ibis::LONG:
        case ibis::FLOAT:
        case ibis::DOUBLE: {
            spec = "default";
            break;}
        case ibis::CATEGORY: {
            spec = "direkte";
            break;}
        case ibis::TEXT: {
            spec = "keywords delimiters=','";
            break;}
        default: {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- index::create can not work with column type "
                << ibis::TYPESTRING[(int)c->type()];
            return 0;}
        }
    }
    else if (c->indexSpec() == 0 || *(c->indexSpec()) == 0 ||
             std::strcmp(c->indexSpec(), spec) != 0) {
        const_cast<column*>(c)->indexSpec(spec);
    }
    LOGGER(ibis::gVerbose > 3)
        << "index::create -- attempt to build a new index with spec `"
        << spec << "' on data from directory "
        << (dfname ? dfname :
            (c->partition() ? (c->partition()->currentDataDir() ?
                               c->partition()->currentDataDir() : "?") : "?"))
        << " for column " << c->fullname();

    ibis::index *ind = 0;
    bool usebin = (strstr(spec, "bin") != 0 && strstr(spec, "none") == 0);
    if (usebin && c->partition() != 0) {
        unsigned nb = ibis::bin::parseNbins(*c);
        const unsigned nr = c->partition()->nRows();
        if (nb >= nr) {
            usebin = false;
        }
        else if (nb >= (nr >> 1) && c->isInteger()) {
            usebin = false;
        }
    }
    uint32_t ncomp = 0;
    const char* ptr = strstr(spec, "ncomp=");
    if (ptr != 0) {
        ptr += 6;
        while (isspace(*ptr)) { // skip till the first digit
            ++ ptr;
        }
        if (*ptr) {
            if (isdigit(*ptr)) { // string --> number
                ncomp = strtol(ptr, 0, 0);
                if (ncomp == 0) {
                    LOGGER(errno == EINVAL && ibis::gVerbose > 0)
                        << "Warning -- index::create failed to extract "
                        "the number of components from  " << ptr
                        << ", use the default value 2";
                    ncomp = 2; // default to 2
                }
            }
            else {
                ncomp = 1;
            }
        }
    }

    bool dflt = false;
    if (c->type() == ibis::CATEGORY) {
        dflt = true;
    }
    else if (spec == 0) {
        dflt = true;
    }
    else if (*spec == 0) {
        dflt = true;
    }
    else {
        while (*spec != 0 && isspace(*spec)) ++ spec;
        if (strstr(spec, "automatic") != 0 ||
            strstr(spec, "default") != 0) {
            dflt = true;
        }
        else {
            dflt = (*spec == 0);
        }
    }

    if (dflt) {
        switch (c->type()) {
        case ibis::ULONG:
        case ibis::LONG:
        case ibis::UINT:
        case ibis::INT: {
            double amin = c->lowerBound();
            double amax = c->upperBound();
            if (!(amin <= amax)) {
                const_cast<ibis::column*>(c)->computeMinMax();
                amin = c->lowerBound();
                amax = c->upperBound();
            }
            if (amax - amin < 1e4 || amax - amin < c->nRows()*0.1) {
                if (amin >= 0.0 && amin <= ceil(amax*0.01))
                    ind = new ibis::direkte(c, dfname);
                else if (amax >= amin+1e2)
                    ind = new ibis::fuzz(c, dfname);
                else
                    ind = new ibis::relic(c, dfname);
            }
            else {
                ind = new ibis::bin(c, dfname);
            }
            break;}
        case ibis::FLOAT:
        case ibis::DOUBLE: {
            ind = new ibis::bin(c, dfname);
            break;}
        case ibis::USHORT:
        case ibis::SHORT:
        case ibis::UBYTE:
        case ibis::BYTE: {
            ind = new ibis::relic(c, dfname);
            break;}
        case ibis::CATEGORY: {
            ind = reinterpret_cast<const ibis::category*>(c)->
                fillIndex(dfname);
            break;}
        case ibis::TEXT: {
            ind = new ibis::keywords(c, dfname);
            break;}
        default: {
            c->logWarning("createIndex", "not able to "
                          "generate for this column type");
            break;}
        }
    }
    else if (ncomp > 1 || strstr(spec, "mcbin") != 0 ||
             strstr(spec, "multicomponent") != 0) {
        INDEX_TYPE t = SAPID; // default to equality encoding
        if (strstr(spec, "equal")) {
            t = SAPID;
        }
        else if (strstr(spec, "range")) {
            t = FADE;
        }
        else if (strstr(spec, "interval")) {
            t = SBIAD;
        }
        switch (t) {
        default:
        case SBIAD:
            if (usebin)
                ind = new ibis::entre(c, dfname, ncomp);
            else
                ind = new ibis::sbiad(c, dfname, ncomp);
            break;
        case SAPID:
            if (usebin)
                ind = new ibis::egale(c, dfname, ncomp);
            else
                ind = new ibis::sapid(c, dfname, ncomp);
            break;
        case FADE:
            if (usebin)
                ind = new ibis::moins(c, dfname, ncomp);
            else
                ind = new ibis::fade(c, dfname, ncomp);
            break;
        }
    }
    else if (!usebin) { // <binning none> is specified explicitly
        INDEX_TYPE t = RELIC;
        const char* str = strstr(spec, "<encoding ");
        double lo = c->lowerBound(), hi = c->upperBound();
        if (str) {
            str += 10; // skip "<encoding "
            if (strstr(str, "range/equality") ||
                strstr(str, "range-equality")) {
                if (c->lowerBound() < c->upperBound()) {
                    t = BYLT;
                }
                else {
                    bool asc;
                    c->computeMinMax(dfname, lo, hi, asc);
                    if (lo < hi)
                        t = BYLT;
                }
            }
            else if (strstr(str, "equality/equality") ||
                     strstr(str, "equality-equality")) {
                t = ZONA;
            }
            else if (strstr(str, "interval/equality") ||
                     strstr(str, "interval-equality")) {
                t = FUZZ;
            }
            else if (strstr(str, "equal")) {
                t = SAPID;
            }
            else if (strstr(str, "interval")) {
                t = SBIAD;
            }
            else if (strstr(str, "range")) {
                if (lo < hi) {
                    t = FADE;
                }
                else {
                    bool asc;
                    c->computeMinMax(dfname, lo, hi, asc);
                    if (lo < hi)
                        t = FADE;
                }
            }
            else if (strstr(str, "binary")) {
                t = SKIVE;
            }
        }
        else if (stricmp(spec, "index=simple") == 0 ||
                 stricmp(spec, "index=basic") == 0 ||
                 strstr(spec, "relic") != 0) {
            t = RELIC;
        }
        else if (strstr(spec, "skive") != 0 ||
                 strstr(spec, "binary") != 0) {
            t = SKIVE;
        }
        else if (strstr(spec, "slice") != 0) { // bit-slice, bitslice
            if (c->isInteger()) {
                if (! (lo < hi)) {
                    bool asc;
                    c->computeMinMax(dfname, lo, hi, asc);
                }
                if (lo >= 0)
                    t = SLICE;
                else
                    t = SKIVE;
            }
            else {
                t = SKIVE;
            }
        }
        else {
            t = SAPID;
        }
        switch (t) {
        default:
        case SAPID:
            if (ncomp > 1)
                ind = new ibis::sapid(c, dfname, ncomp);
            else if ((c->type() != ibis::FLOAT &&
                      c->type() != ibis::DOUBLE &&
                      c->type() != ibis::TEXT) &&
                     c->lowerBound() >= 0.0 &&
                     c->lowerBound() <=
                     ceil(c->upperBound()*0.01) &&
                     (c->partition() == 0 ||
                      c->upperBound() <= c->partition()->nRows()))
                ind = new ibis::direkte(c, dfname);
            else
                ind = new ibis::relic(c, dfname);
            break;
        case RELIC:
            ind = new ibis::relic(c, dfname);
            break;
        case FADE:
            ind = new ibis::fade(c, dfname, ncomp);
            break;
        case SBIAD:
            ind = new ibis::sbiad(c, dfname, ncomp);
            break;
        case SKIVE:
            ind = new ibis::skive(c, dfname);
            break;
        case SLICE:
            ind = new ibis::slice(c, dfname);
            break;
        case BYLT:
            ind = new ibis::bylt(c, dfname);
            break;
        case ZONA:
            ind = new ibis::zona(c, dfname);
            break;
        case FUZZ:
            ind = new ibis::fuzz(c, dfname);
            break;
        }
    }
    else if (strstr(spec, "skive") != 0 ||
             strstr(spec, "binary") != 0) { // ibis::skive
        ind = new ibis::skive(c, dfname);
    }
    else if (strstr(spec, "slice") != 0) { // ibis::slice
        ind = new ibis::slice(c, dfname);
    }
    else if (stricmp(spec, "index=simple") == 0 ||
             stricmp(spec, "index=basic") == 0 ||
             strstr(spec, "relic") != 0) {
        if ((c->type() != ibis::FLOAT &&
             c->type() != ibis::DOUBLE &&
             c->type() != ibis::TEXT) &&
            c->lowerBound() >= 0.0 &&
            c->lowerBound() <= ceil(c->upperBound()*0.01) &&
            (c->partition() == 0 ||
             c->upperBound() <= c->partition()->nRows()))
            ind = new ibis::direkte(c, dfname);
        else
            ind = new ibis::relic(c, dfname);
    }
    else if (strstr(spec, "fade") != 0 ||
             strstr(spec, "multi-range") != 0) {
        ind = new ibis::fade(c, dfname);
    }
    else if (strstr(spec, "sapid") != 0 ||
             strstr(spec, "multi-equal") != 0) {
        ind = new ibis::sapid(c, dfname);
    }
    else if (strstr(spec, "sbiad") != 0 ||
             strstr(spec, "multi-interval") != 0) {
        ind = new ibis::sbiad(c, dfname);
    }
    else if (strstr(spec, "egale") != 0) {
        ind = new ibis::egale(c, dfname);
    }
    else if (strstr(spec, "moins") != 0) {
        ind = new ibis::moins(c, dfname);
    }
    else if (strstr(spec, "entre") != 0) {
        ind = new ibis::entre(c, dfname);
    }
    else if (strstr(spec, "ambit") != 0 ||
             strstr(spec, "range/range") != 0 ||
             strstr(spec, "range-range") != 0) {
        std::unique_ptr<ibis::bin> tmp(new  ibis::bin(c, dfname));
        if (tmp->numBins() > 2) {
            ind = new ibis::ambit(*tmp);
        }
        else {
            ind = tmp.release();
        }
    }
    else if (strstr(spec, "pale") != 0 ||
             strstr(spec, "bin/range") != 0 ||
             strstr(spec, "equality-range") != 0) {
        std::unique_ptr<ibis::bin> tmp(new ibis::bin(c, dfname));
        if (tmp->numBins() > 2) {
            ind = new ibis::pale(*tmp);
        }
        else {
            ind = tmp.release();
        }
    }
    else if (strstr(spec, "pack") != 0 ||
             strstr(spec, "range/bin") != 0 ||
             strstr(spec, "range/equality") != 0 ||
             strstr(spec, "range-equality") != 0) {
        std::unique_ptr<ibis::bin> tmp(new ibis::bin(c, dfname));
        if (tmp->numBins() > 2) {
            ind = new ibis::pack(*tmp);
        }
        else {
            ind = tmp.release();
        }
    }
    else if (strstr(spec, "zone") != 0 ||
             strstr(spec, "bin/bin") != 0 ||
             strstr(spec, "equality/equality") != 0 ||
             strstr(spec, "equality-equality") != 0) {
        std::unique_ptr<ibis::bin> tmp(new ibis::bin(c, dfname));
        if (tmp->numBins() > 2) {
            ind = new ibis::zone(*tmp);
        }
        else {
            ind = tmp.release();
        }
    }
    else if (strstr(spec, "interval/equality") != 0 ||
             strstr(spec, "interval-equality") != 0) {
        ind = new ibis::fuge(c, dfname);
    }
    else if (strstr(spec, "bak2") != 0) {
        ind = new ibis::bak2(c, dfname);
    }
    else if (strstr(spec, "bak") != 0) {
        ind = new ibis::bak(c, dfname);
    }
    else if (strstr(spec, "mesa") != 0 ||
             strstr(spec, "interval") != 0 ||
             strstr(spec, "2sided") != 0) {
        std::unique_ptr<ibis::bin> tmp(new ibis::bin(c, dfname));
        if (tmp->numBins() > 2) {
            ind = new ibis::mesa(*tmp);
        }
        else {
            ind = tmp.release();
        }
    }
    else if (strstr(spec, "range") != 0 ||
             strstr(spec, "cumulative") != 0) {
        std::unique_ptr<ibis::bin> tmp(new ibis::bin(c, dfname));
        if (tmp->numBins() > 2) {
            ind = new ibis::range(*tmp);
        }
        else {
            ind = tmp.release();
        }
    }
    else {
        LOGGER(ibis::gVerbose > 1 && strstr(spec, "bin") == 0)
            << "Warning -- index::create can not understand index spec \""
            << spec << "\", use simple bins instead";
        ind = new ibis::bin(c, dfname);
    }
    if (ind != 0 && c->lowerBound() >= c->upperBound()) {
        const_cast<ibis::column*>(c)->lowerBound(ind->getMin());
        const_cast<ibis::column*>(c)->upperBound(ind->getMax());
        LOGGER(ibis::gVerbose > 1)
            << "index::create updated column min and max of column "
            << c->fullname() << " to be "
            << c->lowerBound() << " and " << c->upperBound();
    }
    return ind;
} // ibis::index::buildNew

/// Constructor with a storage object.  Both the column object and the
/// storage object are expected to be valid.  However, this function only
/// make uses of the storage object.
ibis::index::index(const ibis::column* c, ibis::fileManager::storage* s) :
    col(c), str(s), fname(0), breader(0), nrows(0) {
    if (s != 0) {
        nrows = *reinterpret_cast<const uint32_t*>(s->begin()+8);
    }
    // else {
    //  LOGGER(ibis::gVerbose > 0)
    //      << "index::ctor needs a column object and a storage object";
    //  throw "index::ctor needs a column object and a storage object";
    // }
    LOGGER(ibis::gVerbose > 3)
        << "index::ctor reconstituted an index for "
        << (col!=0 ? col->fullname() : "?.?") << " from storage object @ "
        << static_cast<const void*>(s);
} // ibis::index::index

/// Copy constructor.
ibis::index::index(const ibis::index &rhs)
    : col(rhs.col), str(rhs.str), fname(ibis::util::strnewdup(rhs.fname)),
      breader(rhs.breader!=0 ? new bitmapReader(*rhs.breader) : 0),
      offset32(rhs.offset32), offset64(rhs.offset64), bits(rhs.bits.size()),
      nrows(rhs.nrows) {
    for (size_t j = 0; j < rhs.bits.size(); ++ j) {
        if (rhs.bits[j] != 0) {
            bits[j] = new ibis::bitvector(*rhs.bits[j]);
        }
        else {
            bits[j] = 0;
        }
    }
    LOGGER(ibis::gVerbose > 3)
        << "index::ctor copied an index for "
        << (col!=0 ? col->fullname() : "?.?") << " from the existing index @ "
        << static_cast<const void*>(&rhs);
} // ibis::index::index

/// Assignment operator.
ibis::index& ibis::index::operator=(const ibis::index &rhs) {
    clear(); // clear the existing content
    col = rhs.col;
    str = rhs.str;
    fname = ibis::util::strnewdup(rhs.fname);
    breader = (rhs.breader!=0 ? new ibis::index::bitmapReader(*rhs.breader) : 0);
    offset32.copy(rhs.offset32);
    offset64.copy(rhs.offset64);
    bits.copy(rhs.bits);
    nrows = rhs.nrows;
    return *this;
} // ibis::index::operator=

/// Free the bitmap objectes common to all index objects.
void ibis::index::clear() {
    if (bits.size() > 0) {
        LOGGER(ibis::gVerbose > 6 && col != 0)
            << "clearing " << bits.size() << " bit vector"
            << (bits.size()>1?"s":"") << " associated with column "
            << col->name();
        for (uint32_t i = 0; i < bits.size(); ++ i) {
            delete bits[i];
            bits[i] = 0;
        }
    }
    bits.clear();
    offset32.clear();
    offset64.clear();
    nrows = 0;

    // reassign the internal storage tracking variables to null
    delete breader;
    breader = 0;
    delete [] fname;
    fname = 0;
    // the pointer str can only be from a file and must be managed by the
    // fileManager and can not be deleted here
    str = 0;
} // ibis::index::clear

/// Compute the size of the serialized version of the index.  This the
/// fallback implementation which always returns 0.
size_t ibis::index::getSerialSize() const throw () {
    LOGGER(ibis::gVerbose > 1)
        << "Warning -- invoking an abstract implementation of "
        "index::getSerialSize that always returns 0";
    return 0U;
} // ibis::index::getSerialSize

/// Estiamte the size of this index object measured in bytes.  Do not
/// intend to be precise, but should be good enough for operations such as
/// comparing index size against base data size to determine which
/// operation to use for answering a query.
float ibis::index::sizeInBytes() const {
    if (offset64.size() > bits.size()) {
        return (float)offset64[bits.size()];
    }
    else if (offset32.size() > bits.size()) {
        return (float)offset32[bits.size()];
    }
    else if (str != 0) {
        return (float)str->size();
    }
    else if (fname != 0 && *fname != 0) {
        return (float)ibis::util::getFileSize(fname);
    }
    else if (! bits.empty()) {
        offset32.clear();
        offset64.clear();
        offset64.resize(bits.size()+1);
        offset64[0] = 0;
        for (size_t j = 0; j < bits.size(); ++ j) {
            offset64[j+1] = offset64[j] +
                (bits[j] != 0 ? bits[j]->getSerialSize() : 0U);
        }
        return (float)offset64[bits.size()];
    }
    else {
        return FLT_MAX;
    }
} // ibis::index::sizeInBytes

void ibis::index::printHeader(std::ostream &out, const char *header) {
    if (isprint(header[0]) != 0)
        out << header[0];
    else
        out << "0x" << std::hex << (uint16_t) header[0] << std::dec;
    out << ' ';
    if (isprint(header[1]) != 0)
        out << header[1];
    else
        out << "0x" << std::hex << (uint16_t) header[1] << std::dec;
    out << ' ';
    if (isprint(header[2]) != 0)
        out << header[2];
    else
        out << "0x" << std::hex << (uint16_t) header[2] << std::dec;
    out << ' ';
    if (isprint(header[3]) != 0)
        out << header[3];
    else
        out << "0x" << std::hex << (uint16_t) header[3] << std::dec;
    out << ' ';
    if (isprint(header[4]) != 0)
        out << header[4];
    else
        out << "0x" << std::hex << (uint16_t) header[4] << std::dec;
    out << ' ';
    if (isprint(header[5]) != 0)
        out << header[5];
    else
        out << "0x" << std::hex << (uint16_t) header[5] << std::dec;
    out << ' ';
    if (isprint(header[6]) != 0)
        out << header[6];
    else
        out << "0x" << std::hex << (uint16_t) header[6] << std::dec;
    out << ' ';
    if (isprint(header[7]) != 0)
        out << header[7];
    else
        out << "0x" << std::hex << (uint16_t) header[7] << std::dec;
}

/// Is the named file an index file?  Read the header of the named
/// file to determine if it contains an index of the specified type.
/// Returns true if the correct header is found, else return false.
bool ibis::index::isIndex(const char* f, ibis::index::INDEX_TYPE t) {
    char buf[12];
    char* header = 0;
    // attempt to read the file using read(2)
    int fdes = UnixOpen(f, OPEN_READONLY);
    if (fdes >= 0) {
#if defined(_WIN32) && defined(_MSC_VER)
        (void)_setmode(fdes, _O_BINARY);
#endif
        if (8 == UnixRead(fdes, static_cast<void*>(buf), 8)) {
            header = buf;
        }
        UnixClose(fdes);
    }

    if (header) { // verify header
        const bool check =
            (header[0] == '#' && header[1] == 'I' &&
             header[2] == 'B' && header[3] == 'I' &&
             header[4] == 'S' && t ==
             static_cast<ibis::index::INDEX_TYPE>(header[5]) &&
             (header[6] == 8 || header[6] == 4) &&
             header[7] == static_cast<char>(0));
        if (!check) {
            ibis::util::logMessage("readIndex", "index file \"%s\" contains "
                                   "an incorrect header "
                                   "(%c%c%c%c%c:%i.%i.%i)",
                                   f, header[0], header[1], header[2],
                                   header[3], header[4], (int)header[5],
                                   (int)header[6], (int)header[7]);
        }
        return check;
    }
    return false;
} // ibis::index::isIndex

/// Generate data file name from "f".  Invokes ibis::column::dataFileName
/// to do the actual work.
void ibis::index::dataFileName(std::string& iname, const char* f) const {
    iname.clear();
    if (col != 0) {
        (void) col->dataFileName(iname, f);
    }
} // dataFileName

/// Generates index file name from "f".  Invokes ibis::column::dataFileName
/// to do most of the work.
void ibis::index::indexFileName(std::string& iname, const char* f) const {
    iname.clear();
    if (col != 0) {
        (void) col->dataFileName(iname, f);
        if (! iname.empty()) {
            iname += ".idx";
        }
    }
    else if (f != 0 && *f != 0) {
        unsigned len = std::strlen(f);
        if (len > 4 && f[len-4] == '.' && f[len-3] == 'i' &&
            f[len-2] == 'd' && f[len-1] == 'x') {
            // the incoming name ends with ".idx", use it
            iname = f;
        }
        else {
            Stat_T st0;
            if (UnixStat(f, &st0)) { // stat fails, use the name
                iname = f;
                iname += ".idx";
            }
            else if ((st0.st_mode & S_IFDIR) == S_IFDIR) {
                // named directory exists, use file name _.idx
                iname = f;
                iname += FASTBIT_DIRSEP;
                iname += "_.idx";
            }    
            else {
                // the incoming argument names an existing file, add ".idx"
                // to create the new index file name
                iname = f;
                iname += ".idx";
            }
        }
    }

    LOGGER(ibis::gVerbose > 6)
        << "index::indexFileName will use \"" << iname
        << "\" as the index file name for " << (col ? col->fullname() : "?.?");
} // indexFileName

/// Generate the index file name for the composite index fromed on two
/// columns.  May use argument "dir" if it is not null.
void ibis::index::indexFileName(std::string& iname,
                                const ibis::column *col1,
                                const ibis::column *col2,
                                const char* dir) {
    if (dir == 0 || *dir == 0) {
        iname = col1->partition()->currentDataDir();
        iname += FASTBIT_DIRSEP;
        iname += col1->name();
        iname += '-';
        iname += col2->name();
        iname += ".idx";
    }
    else {
        Stat_T st0;
        if (UnixStat(dir, &st0)) { // stat fails, use the name
            iname = dir;
            uint32_t j = iname.rfind(FASTBIT_DIRSEP);
            if (j < iname.size()) {
                ++ j;
                iname.erase(j);
            }
            else if (iname.size() > 0) {
                iname += FASTBIT_DIRSEP;
            }
        }
        else if ((st0.st_mode & S_IFDIR) == S_IFDIR) {
            // named directory exist
            iname = dir;
            if (iname[iname.size()-1] != FASTBIT_DIRSEP)
                iname += FASTBIT_DIRSEP;
        }
        else {
            iname = dir;
            uint32_t j = iname.rfind(FASTBIT_DIRSEP);
            if (j < iname.size()) {
                ++ j;
                iname.erase(j);
            }
            else if (iname.size() > 0) {
                iname += FASTBIT_DIRSEP;
            }
        }
        iname += col1->name();
        iname += '-';
        iname += col2->name();
        iname += ".idx";
    }
} // indexFileName

// actually go through values and determine the min/max values
void ibis::index::computeMinMax(const char* f, double& min,
                                double& max) const {
    if (col == 0) return; // nothing can be done

    std::string fnm;
    dataFileName(fnm, f); // generate the correct data file name
    if (fnm.empty()) return;

    switch (col->type()) {
    case ibis::UINT: {
        array_t<uint32_t> val;
        int ierr = ibis::fileManager::instance().getFile(fnm.c_str(), val);
        if (ierr != 0) {
            col->logWarning("computeMinMax", "failed to retrieve file %s",
                            fnm.c_str());
            return;
        }

        uint32_t imin = val[0];
        uint32_t imax = val[0];
        uint32_t nelm = val.size();
        for (uint32_t i = 1; i < nelm; ++i) {
            if (imin > val[i])
                imin = val[i];
            else if (imax < val[i])
                imax = val[i];
        }
        min = imin;
        max = imax;
        break;}
    case ibis::INT: {
        array_t<int32_t> val;
        int ierr = ibis::fileManager::instance().getFile(fnm.c_str(), val);
        if (ierr != 0) {
            col->logWarning("computeMinMax", "failed to retrieve file %s",
                            fnm.c_str());
            return;
        }

        int32_t imin = val[0];
        int32_t imax = val[0];
        uint32_t nelm = val.size();
        for (uint32_t i = 1; i < nelm; ++i) {
            if (imin > val[i])
                imin = val[i];
            else if (imax < val[i])
                imax = val[i];
        }
        min = imin;
        max = imax;
        break;}
    case ibis::USHORT: {
        array_t<uint16_t> val;
        int ierr = ibis::fileManager::instance().getFile(fnm.c_str(), val);
        if (ierr != 0) {
            col->logWarning("computeMinMax", "failed to retrieve file %s",
                            fnm.c_str());
            return;
        }

        uint16_t imin = val[0];
        uint16_t imax = val[0];
        uint32_t nelm = val.size();
        for (uint32_t i = 1; i < nelm; ++i) {
            if (imin > val[i])
                imin = val[i];
            else if (imax < val[i])
                imax = val[i];
        }
        min = imin;
        max = imax;
        break;}
    case ibis::SHORT: {
        array_t<int16_t> val;
        int ierr = ibis::fileManager::instance().getFile(fnm.c_str(), val);
        if (ierr != 0) {
            col->logWarning("computeMinMax", "failed to retrieve file %s",
                            fnm.c_str());
            return;
        }

        int16_t imin = val[0];
        int16_t imax = val[0];
        uint32_t nelm = val.size();
        for (uint32_t i = 1; i < nelm; ++i) {
            if (imin > val[i])
                imin = val[i];
            else if (imax < val[i])
                imax = val[i];
        }
        min = imin;
        max = imax;
        break;}
    case ibis::UBYTE: {
        array_t<unsigned char> val;
        int ierr = ibis::fileManager::instance().getFile(fnm.c_str(), val);
        if (ierr != 0) {
            col->logWarning("computeMinMax", "failed to retrieve file %s",
                            fnm.c_str());
            return;
        }

        unsigned char imin = val[0];
        unsigned char imax = val[0];
        uint32_t nelm = val.size();
        for (uint32_t i = 1; i < nelm; ++i) {
            if (imin > val[i])
                imin = val[i];
            else if (imax < val[i])
                imax = val[i];
        }
        min = imin;
        max = imax;
        break;}
    case ibis::BYTE: {
        array_t<signed char> val;
        int ierr = ibis::fileManager::instance().getFile(fnm.c_str(), val);
        if (ierr != 0) {
            col->logWarning("computeMinMax", "failed to retrieve file %s",
                            fnm.c_str());
            return;
        }

        signed char imin = val[0];
        signed char imax = val[0];
        uint32_t nelm = val.size();
        for (uint32_t i = 1; i < nelm; ++i) {
            if (imin > val[i])
                imin = val[i];
            else if (imax < val[i])
                imax = val[i];
        }
        min = imin;
        max = imax;
        break;}
    case ibis::FLOAT: {
        array_t<float> val;
        int ierr = ibis::fileManager::instance().getFile(fnm.c_str(), val);
        if (ierr != 0) {
            col->logWarning("computeMinMax", "failed to retrieve file %s",
                            fnm.c_str());
            return;
        }

        min = val[0];
        max = val[0];
        uint32_t nelm = val.size();
        for (uint32_t i = 1; i < nelm; ++i) {
            if (min > val[i])
                min = val[i];
            else if (max < val[i])
                max = val[i];
        }
        break;}
    case ibis::DOUBLE: {
        array_t<double> val;
        int ierr = ibis::fileManager::instance().getFile(fnm.c_str(), val);
        if (ierr != 0) {
            col->logWarning("computeMinMax", "failed to retrieve file %s",
                            fnm.c_str());
            return;
        }

        min = val[0];
        max = val[0];
        uint32_t nelm = val.size();
        for (uint32_t i = 1; i < nelm; ++i) {
            if (min > val[i])
                min = val[i];
            else if (max < val[i])
                max = val[i];
        }
        break;}
    default:
        col->logMessage("computeMinMax", "not able to compute min/max or "
                        "no need for min/max");
        min = 0;
        max = 0;
    } // switch(m_type)
} // ibis::index::computeMinMax

/// Map the locations of the values of one column.  Given a file containing
/// the values of a column, this function maps the position of each
/// individual values and stores the result in a set of bitmaps.
///@note
/// IMPROTANT ASSUMPTION.
/// A value of any supported type is supposed to be able to fit in a
/// double with no rounding, no approximation and no overflow.
void ibis::index::mapValues(const char* f, VMap& bmap) const {
    if (col == 0) return;

    horometer timer;
    if (ibis::gVerbose > 4)
        timer.start();
    uint32_t i, j, k, nev;
    std::string fnm; // name of the data file

    bmap.clear();
    dataFileName(fnm, f);
    std::string evt = "index";
    if (ibis::gVerbose > 0) {
        evt += '[';
        evt += col->fullname();
        evt += ']';
    }
    evt += "::mapValues";
    if (ibis::gVerbose > 2 && ! fnm.empty()) {
        evt += '(';
        evt += fnm;
        evt += ')';
    }
    LOGGER(fnm.empty() && f != 0 && ibis::gVerbose > 2)
        << "Warning -- " << evt << " failed to determine the data file "
        "name from \"" << (f ? f : "") << "\" for column " << col->name()
        << ", will attempt to use in-memory data";

    if (! fnm.empty()) {
        k = ibis::util::getFileSize(fnm.c_str());
        if (k > 0) {
            LOGGER(ibis::gVerbose > 1)
                << evt << " attempt to map the positions of every value in \""
                << fnm << '"';
        }
        else {
            if (col->partition() != 0 && col->partition()->nRows() > 0) {
                if (col->type() == ibis::CATEGORY) {
                    if (col->partition()->getState() ==
                        ibis::part::PRETRANSITION_STATE) {
                        ibis::bitvector *tmp = new ibis::bitvector;
                        tmp->set(1, col->partition()->nRows());
                        bmap[1] = tmp;
                    }
                }
                else {
                    LOGGER(ibis::gVerbose > 4)
                        << "Warning -- " << evt
                        << " failed to determine the size of data file \""
                        << fnm << "\"";
                }
            }
            return;
        }
    }

    int ierr = 0;
    VMap::iterator it;
    ibis::bitvector mask;
    col->getNullMask(mask);
#if defined(MAPVALUES_EXCLUDE_INACTIVE)
    if (col->partition() != 0) {
        mask &= col->partition()->getMaskRef();
        mask.adjustSize(0, col->partition()->nRows());
    }
#endif
    // need to do different things for different columns
    switch (col->type()) {
    case ibis::TEXT:
    case ibis::UINT:
    case ibis::CATEGORY: {// if data file exists, must be unsigned int
        array_t<uint32_t> val;
        if (! fnm.empty())
            ierr = ibis::fileManager::instance().getFile(fnm.c_str(), val);
        else if (col->type() == ibis::UINT)
            ierr = col->getValuesArray(&val);
        else
            ierr = -1;
        nev = val.size();
        if (ierr < 0 || val.size() == 0)
            break;

        if (nev > mask.size())
            mask.adjustSize(nev, nev);
        ibis::bitvector::indexSet iset = mask.firstIndexSet();
        uint32_t nind = iset.nIndices();
        const ibis::bitvector::word_t *iix = iset.indices();
        while (nind) { // build the list of bitmaps
            if (iset.isRange()) { // a range
                k = (iix[1] < nev ? iix[1] : nev);
                for (i = *iix; i < k; ++i) {
                    it = bmap.find(val[i]);
                    if (it != bmap.end()) {
                        (*it).second->setBit(i, 1);
                    }
                    else {
                        ibis::bitvector* tmp = new ibis::bitvector();
                        tmp->setBit(i, 1);
                        bmap[val[i]] = tmp;
                    }
                }
            }
            else if (*iix+ibis::bitvector::bitsPerLiteral() < nev) {
                // a list of indices that are definitely within nev
                for (i = 0; i < nind; ++i) {
                    k = iix[i];
                    it = bmap.find(val[k]);
                    if (it != bmap.end()) {
                        (*it).second->setBit(k, 1);
                    }
                    else {
                        ibis::bitvector* tmp = new ibis::bitvector();
                        tmp->setBit(k, 1);
                        bmap[val[k]] = tmp;
                    }
                }
            }
            else { // a list of indices that may be larger than nev
                for (i = 0; i < nind; ++i) {
                    k = iix[i];
                    if (k < nev) {
                        it = bmap.find(val[k]);
                        if (it != bmap.end()) {
                            (*it).second->setBit(k, 1);
                        }
                        else {
                            ibis::bitvector* tmp = new ibis::bitvector();
                            tmp->setBit(k, 1);
                            bmap[val[k]] = tmp;
                        }
                    }
                }
            }
            ++iset;
            nind = iset.nIndices();
            if (*iix >= nev) nind = 0;
        } // while (nind)
        break;}

    case ibis::INT: {// signed int
        array_t<int32_t> val;
        if (! fnm.empty())
            ierr = ibis::fileManager::instance().getFile(fnm.c_str(), val);
        else
            ierr = col->getValuesArray(&val);
        nev = val.size();
        if (ierr < 0 || val.size() == 0)
            break;

        if (nev > mask.size())
            mask.adjustSize(nev, nev);
        ibis::bitvector::indexSet iset = mask.firstIndexSet();
        unsigned nind = iset.nIndices();
        const ibis::bitvector::word_t *iix = iset.indices();
        while (nind) { // build the list of bitmaps
            if (iset.isRange()) { // a range
                k = (iix[1] < nev ? iix[1] : nev);
                for (i = *iix; i < k; ++i) {
                    it = bmap.find(val[i]);
                    if (it != bmap.end()) {
                        (*it).second->setBit(i, 1);
                    }
                    else {
                        ibis::bitvector* tmp = new ibis::bitvector();
                        tmp->setBit(i, 1);
                        bmap[val[i]] = tmp;
                    }
                }
            }
            else if (*iix+ibis::bitvector::bitsPerLiteral() < nev) {
                // a list of indices
                for (i = 0; i < nind; ++i) {
                    k = iix[i];
                    it = bmap.find(val[k]);
                    if (it != bmap.end()) {
                        (*it).second->setBit(k, 1);
                    }
                    else {
                        ibis::bitvector* tmp = new ibis::bitvector();
                        tmp->setBit(k, 1);
                        bmap[val[k]] = tmp;
                    }
                }
            }
            else {
                for (i = 0; i < nind; ++i) {
                    k = iix[i];
                    if (k < nev) {
                        it = bmap.find(val[k]);
                        if (it != bmap.end()) {
                            (*it).second->setBit(k, 1);
                        }
                        else {
                            ibis::bitvector* tmp = new ibis::bitvector();
                            tmp->setBit(k, 1);
                            bmap[val[k]] = tmp;
                        }
                    }
                }
            }
            ++iset;
            nind = iset.nIndices();
            if (*iix >= nev) nind = 0;
        } // while (nind)
        break;}

    case ibis::FLOAT: {// (4-byte) floating-point values
        array_t<float> val;
        if (! fnm.empty())
            ierr = ibis::fileManager::instance().getFile(fnm.c_str(), val);
        else
            ierr = col->getValuesArray(&val);
        nev = val.size();
        if (ierr < 0 || val.size() == 0)
            break;

        if (nev > mask.size())
            mask.adjustSize(nev, nev);
        ibis::bitvector::indexSet iset = mask.firstIndexSet();
        unsigned nind = iset.nIndices();
        const ibis::bitvector::word_t *iix = iset.indices();
        while (nind) { // build the list of bitmaps
            if (iset.isRange()) { // a range
                k = (iix[1] < nev ? iix[1] : nev);
                for (i = *iix; i < k; ++i) {
                    it = bmap.find(val[i]);
                    if (it != bmap.end()) {
                        (*it).second->setBit(i, 1);
                    }
                    else {
                        ibis::bitvector* tmp = new ibis::bitvector();
                        tmp->setBit(i, 1);
                        bmap[val[i]] = tmp;
                    }
                }
            }
            else if (*iix+ibis::bitvector::bitsPerLiteral() < nev) {
                // a list of indices
                for (i = 0; i < nind; ++i) {
                    k = iix[i];
                    it = bmap.find(val[k]);
                    if (it != bmap.end()) {
                        (*it).second->setBit(k, 1);
                    }
                    else {
                        ibis::bitvector* tmp = new ibis::bitvector();
                        tmp->setBit(k, 1);
                        bmap[val[k]] = tmp;
                    }
                }
            }
            else {
                for (i = 0; i < nind; ++i) {
                    k = iix[i];
                    if (k < nev) {
                        it = bmap.find(val[k]);
                        if (it != bmap.end()) {
                            (*it).second->setBit(k, 1);
                        }
                        else {
                            ibis::bitvector* tmp = new ibis::bitvector();
                            tmp->setBit(k, 1);
                            bmap[val[k]] = tmp;
                        }
                    }
                }
            }
            ++iset;
            nind = iset.nIndices();
            if (*iix >= nev) nind = 0;
        } // while (nind)
        break;}

    case ibis::DOUBLE: {// (8-byte) floating-point values
        array_t<double> val;
        if (! fnm.empty())
            ierr = ibis::fileManager::instance().getFile(fnm.c_str(), val);
        else
            ierr = col->getValuesArray(&val);
        nev = val.size();
        if (ierr < 0 || val.size() == 0)
            break;

        if (nev > mask.size())
            mask.adjustSize(nev, nev);
        ibis::bitvector::indexSet iset = mask.firstIndexSet();
        unsigned nind = iset.nIndices();
        const ibis::bitvector::word_t *iix = iset.indices();
        while (nind) { // build the list of bitmaps
            if (iset.isRange()) { // a range
                k = (iix[1] < nev ? iix[1] : nev);
                for (i = *iix; i < k; ++i) {
                    it = bmap.find(val[i]);
                    if (it != bmap.end()) {
                        (*it).second->setBit(i, 1);
                    }
                    else {
                        ibis::bitvector* tmp = new ibis::bitvector();
                        tmp->setBit(i, 1);
                        bmap[val[i]] = tmp;
                    }
                }
            }
            else if (*iix+ibis::bitvector::bitsPerLiteral() < nev) {
                // a list of indices
                for (i = 0; i < nind; ++i) {
                    k = iix[i];
                    it = bmap.find(val[k]);
                    if (it != bmap.end()) {
                        (*it).second->setBit(k, 1);
                    }
                    else {
                        ibis::bitvector* tmp = new ibis::bitvector();
                        tmp->setBit(k, 1);
                        bmap[val[k]] = tmp;
                    }
                }
            }
            else {
                for (i = 0; i < nind; ++i) {
                    k = iix[i];
                    if (k < nev) {
                        it = bmap.find(val[k]);
                        if (it != bmap.end()) {
                            (*it).second->setBit(k, 1);
                        }
                        else {
                            ibis::bitvector* tmp = new ibis::bitvector();
                            tmp->setBit(k, 1);
                            bmap[val[k]] = tmp;
                        }
                    }
                }
            }
            ++iset;
            nind = iset.nIndices();
            if (*iix >= nev) nind = 0;
        } // while (nind)
        break;}

    case ibis::BYTE: {// (1-byte) integer values
        array_t<signed char> val;
        if (! fnm.empty())
            ierr = ibis::fileManager::instance().getFile(fnm.c_str(), val);
        else
            ierr = col->getValuesArray(&val);
        nev = val.size();
        if (ierr < 0 || val.size() == 0)
            break;

        if (nev > mask.size())
            mask.adjustSize(nev, nev);
        ibis::bitvector::indexSet iset = mask.firstIndexSet();
        unsigned nind = iset.nIndices();
        const ibis::bitvector::word_t *iix = iset.indices();
        while (nind) { // build the list of bitmaps
            if (iset.isRange()) { // a range
                k = (iix[1] < nev ? iix[1] : nev);
                for (i = *iix; i < k; ++i) {
                    it = bmap.find(val[i]);
                    if (it != bmap.end()) {
                        (*it).second->setBit(i, 1);
                    }
                    else {
                        ibis::bitvector* tmp = new ibis::bitvector();
                        tmp->setBit(i, 1);
                        bmap[val[i]] = tmp;
                    }
                }
            }
            else if (*iix+ibis::bitvector::bitsPerLiteral() < nev) {
                // a list of indices
                for (i = 0; i < nind; ++i) {
                    k = iix[i];
                    it = bmap.find(val[k]);
                    if (it != bmap.end()) {
                        (*it).second->setBit(k, 1);
                    }
                    else {
                        ibis::bitvector* tmp = new ibis::bitvector();
                        tmp->setBit(k, 1);
                        bmap[val[k]] = tmp;
                    }
                }
            }
            else {
                for (i = 0; i < nind; ++i) {
                    k = iix[i];
                    if (k < nev) {
                        it = bmap.find(val[k]);
                        if (it != bmap.end()) {
                            (*it).second->setBit(k, 1);
                        }
                        else {
                            ibis::bitvector* tmp = new ibis::bitvector();
                            tmp->setBit(k, 1);
                            bmap[val[k]] = tmp;
                        }
                    }
                }
            }
            ++iset;
            nind = iset.nIndices();
            if (*iix >= nev) nind = 0;
        } // while (nind)
        break;}

    case ibis::UBYTE: {// (1-byte) integer values
        array_t<unsigned char> val;
        if (! fnm.empty())
            ierr = ibis::fileManager::instance().getFile(fnm.c_str(), val);
        else
            ierr = col->getValuesArray(&val);
        nev = val.size();
        if (ierr < 0 || val.size() == 0)
            break;

        if (nev > mask.size())
            mask.adjustSize(nev, nev);
        ibis::bitvector::indexSet iset = mask.firstIndexSet();
        unsigned nind = iset.nIndices();
        const ibis::bitvector::word_t *iix = iset.indices();
        while (nind) { // build the list of bitmaps
            if (iset.isRange()) { // a range
                k = (iix[1] < nev ? iix[1] : nev);
                for (i = *iix; i < k; ++i) {
                    it = bmap.find(val[i]);
                    if (it != bmap.end()) {
                        (*it).second->setBit(i, 1);
                    }
                    else {
                        ibis::bitvector* tmp = new ibis::bitvector();
                        tmp->setBit(i, 1);
                        bmap[val[i]] = tmp;
                    }
                }
            }
            else if (*iix+ibis::bitvector::bitsPerLiteral() < nev) {
                // a list of indices
                for (i = 0; i < nind; ++i) {
                    k = iix[i];
                    it = bmap.find(val[k]);
                    if (it != bmap.end()) {
                        (*it).second->setBit(k, 1);
                    }
                    else {
                        ibis::bitvector* tmp = new ibis::bitvector();
                        tmp->setBit(k, 1);
                        bmap[val[k]] = tmp;
                    }
                }
            }
            else {
                for (i = 0; i < nind; ++i) {
                    k = iix[i];
                    if (k < nev) {
                        it = bmap.find(val[k]);
                        if (it != bmap.end()) {
                            (*it).second->setBit(k, 1);
                        }
                        else {
                            ibis::bitvector* tmp = new ibis::bitvector();
                            tmp->setBit(k, 1);
                            bmap[val[k]] = tmp;
                        }
                    }
                }
            }
            ++iset;
            nind = iset.nIndices();
            if (*iix >= nev) nind = 0;
        } // while (nind)
        break;}

    case ibis::SHORT: {// (2-byte) integer values
        array_t<int16_t> val;
        if (! fnm.empty())
            ierr = ibis::fileManager::instance().getFile(fnm.c_str(), val);
        else
            ierr = col->getValuesArray(&val);
        nev = val.size();
        if (ierr < 0 || val.size() == 0)
            break;

        if (nev > mask.size())
            mask.adjustSize(nev, nev);
        ibis::bitvector::indexSet iset = mask.firstIndexSet();
        unsigned nind = iset.nIndices();
        const ibis::bitvector::word_t *iix = iset.indices();
        while (nind) { // build the list of bitmaps
            if (iset.isRange()) { // a range
                k = (iix[1] < nev ? iix[1] : nev);
                for (i = *iix; i < k; ++i) {
                    it = bmap.find(val[i]);
                    if (it != bmap.end()) {
                        (*it).second->setBit(i, 1);
                    }
                    else {
                        ibis::bitvector* tmp = new ibis::bitvector();
                        tmp->setBit(i, 1);
                        bmap[val[i]] = tmp;
                    }
                }
            }
            else if (*iix+ibis::bitvector::bitsPerLiteral() < nev) {
                // a list of indices
                for (i = 0; i < nind; ++i) {
                    k = iix[i];
                    it = bmap.find(val[k]);
                    if (it != bmap.end()) {
                        (*it).second->setBit(k, 1);
                    }
                    else {
                        ibis::bitvector* tmp = new ibis::bitvector();
                        tmp->setBit(k, 1);
                        bmap[val[k]] = tmp;
                    }
                }
            }
            else {
                for (i = 0; i < nind; ++i) {
                    k = iix[i];
                    if (k < nev) {
                        it = bmap.find(val[k]);
                        if (it != bmap.end()) {
                            (*it).second->setBit(k, 1);
                        }
                        else {
                            ibis::bitvector* tmp = new ibis::bitvector();
                            tmp->setBit(k, 1);
                            bmap[val[k]] = tmp;
                        }
                    }
                }
            }
            ++iset;
            nind = iset.nIndices();
            if (*iix >= nev) nind = 0;
        } // while (nind)
        break;}

    case ibis::USHORT: {// (2-byte) integer values
        array_t<uint16_t> val;
        if (! fnm.empty())
            ierr = ibis::fileManager::instance().getFile(fnm.c_str(), val);
        else
            ierr = col->getValuesArray(&val);
        nev = val.size();
        if (ierr < 0 || val.size() == 0)
            break;

        if (nev > mask.size())
            mask.adjustSize(nev, nev);
        ibis::bitvector::indexSet iset = mask.firstIndexSet();
        unsigned nind = iset.nIndices();
        const ibis::bitvector::word_t *iix = iset.indices();
        while (nind) { // build the list of bitmaps
            if (iset.isRange()) { // a range
                k = (iix[1] < nev ? iix[1] : nev);
                for (i = *iix; i < k; ++i) {
                    it = bmap.find(val[i]);
                    if (it != bmap.end()) {
                        (*it).second->setBit(i, 1);
                    }
                    else {
                        ibis::bitvector* tmp = new ibis::bitvector();
                        tmp->setBit(i, 1);
                        bmap[val[i]] = tmp;
                    }
                }
            }
            else if (*iix+ibis::bitvector::bitsPerLiteral() < nev) {
                // a list of indices
                for (i = 0; i < nind; ++i) {
                    k = iix[i];
                    it = bmap.find(val[k]);
                    if (it != bmap.end()) {
                        (*it).second->setBit(k, 1);
                    }
                    else {
                        ibis::bitvector* tmp = new ibis::bitvector();
                        tmp->setBit(k, 1);
                        bmap[val[k]] = tmp;
                    }
                }
            }
            else {
                for (i = 0; i < nind; ++i) {
                    k = iix[i];
                    if (k < nev) {
                        it = bmap.find(val[k]);
                        if (it != bmap.end()) {
                            (*it).second->setBit(k, 1);
                        }
                        else {
                            ibis::bitvector* tmp = new ibis::bitvector();
                            tmp->setBit(k, 1);
                            bmap[val[k]] = tmp;
                        }
                    }
                }
            }
            ++iset;
            nind = iset.nIndices();
            if (*iix >= nev) nind = 0;
        } // while (nind)
        break;}

    case ibis::ULONG: {// if data file exists, must be unsigned int64_t
        array_t<uint64_t> val;
        if (! fnm.empty())
            ierr = ibis::fileManager::instance().getFile(fnm.c_str(), val);
        else
            ierr = col->getValuesArray(&val);
        nev = val.size();
        if (ierr < 0 || val.size() == 0)
            break;

        if (nev > mask.size())
            mask.adjustSize(nev, nev);
        ibis::bitvector::indexSet iset = mask.firstIndexSet();
        uint32_t nind = iset.nIndices();
        const ibis::bitvector::word_t *iix = iset.indices();
        while (nind) { // build the list of bitmaps
            if (iset.isRange()) { // a range
                k = (iix[1] < nev ? iix[1] : nev);
                for (i = *iix; i < k; ++i) {
                    it = bmap.find(val[i]);
                    if (it != bmap.end()) {
                        (*it).second->setBit(i, 1);
                    }
                    else {
                        ibis::bitvector* tmp = new ibis::bitvector();
                        tmp->setBit(i, 1);
                        bmap[val[i]] = tmp;
                    }
                }
            }
            else if (*iix+ibis::bitvector::bitsPerLiteral() < nev) {
                // a list of indices that are definitely within nev
                for (i = 0; i < nind; ++i) {
                    k = iix[i];
                    it = bmap.find(val[k]);
                    if (it != bmap.end()) {
                        (*it).second->setBit(k, 1);
                    }
                    else {
                        ibis::bitvector* tmp = new ibis::bitvector();
                        tmp->setBit(k, 1);
                        bmap[val[k]] = tmp;
                    }
                }
            }
            else { // a list of indices that may be larger than nev
                for (i = 0; i < nind; ++i) {
                    k = iix[i];
                    if (k < nev) {
                        it = bmap.find(val[k]);
                        if (it != bmap.end()) {
                            (*it).second->setBit(k, 1);
                        }
                        else {
                            ibis::bitvector* tmp = new ibis::bitvector();
                            tmp->setBit(k, 1);
                            bmap[val[k]] = tmp;
                        }
                    }
                }
            }
            ++iset;
            nind = iset.nIndices();
            if (*iix >= nev) nind = 0;
        } // while (nind)
        break;}

    case ibis::LONG: {// signed int64_t
        array_t<int64_t> val;
        if (! fnm.empty())
            ierr = ibis::fileManager::instance().getFile(fnm.c_str(), val);
        else
            ierr = col->getValuesArray(&val);
        nev = val.size();
        if (ierr < 0 || val.size() == 0)
            break;

        if (nev > mask.size())
            mask.adjustSize(nev, nev);
        ibis::bitvector::indexSet iset = mask.firstIndexSet();
        unsigned nind = iset.nIndices();
        const ibis::bitvector::word_t *iix = iset.indices();
        while (nind) { // build the list of bitmaps
            if (iset.isRange()) { // a range
                k = (iix[1] < nev ? iix[1] : nev);
                for (i = *iix; i < k; ++i) {
                    it = bmap.find(val[i]);
                    if (it != bmap.end()) {
                        (*it).second->setBit(i, 1);
                    }
                    else {
                        ibis::bitvector* tmp = new ibis::bitvector();
                        tmp->setBit(i, 1);
                        bmap[val[i]] = tmp;
                    }
                }
            }
            else if (*iix+ibis::bitvector::bitsPerLiteral() < nev) {
                // a list of indices
                for (i = 0; i < nind; ++i) {
                    k = iix[i];
                    it = bmap.find(val[k]);
                    if (it != bmap.end()) {
                        (*it).second->setBit(k, 1);
                    }
                    else {
                        ibis::bitvector* tmp = new ibis::bitvector();
                        tmp->setBit(k, 1);
                        bmap[val[k]] = tmp;
                    }
                }
            }
            else {
                for (i = 0; i < nind; ++i) {
                    k = iix[i];
                    if (k < nev) {
                        it = bmap.find(val[k]);
                        if (it != bmap.end()) {
                            (*it).second->setBit(k, 1);
                        }
                        else {
                            ibis::bitvector* tmp = new ibis::bitvector();
                            tmp->setBit(k, 1);
                            bmap[val[k]] = tmp;
                        }
                    }
                }
            }
            ++iset;
            nind = iset.nIndices();
            if (*iix >= nev) nind = 0;
        } // while (nind)
        break;}

    default:
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " can not process column type "
            << ibis::TYPESTRING[(int)col->type()];
        return;
    }

    if (ierr < 0) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " failed to read data, ierr="
            << ierr;
        return;
    }
    else if (nev == 0) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " read on data entry";
        return;
    }

    // make sure all bit vectors are the same size
    if (mask.size() > nev)
        nev = mask.size();
    j = nev - 1;
    for (it = bmap.begin(); it != bmap.end(); ++it) {
        if ((*it).second->size() < nev) {
            (*it).second->setBit(j, 0);
        }
        else if ((*it).second->size() > nev) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << evt << ": bitvector for value "
                << (*it).first << "contains " << (*it).second->size()
                << " bits while " << nev << " are expected -- "
                "removing the extra bits";
            (*it).second->adjustSize(nev, nev);
        }
    }
    if (ibis::gVerbose > 4) {
        timer.stop();
        ibis::util::logger lg;
        lg() << evt << " mapped " << nev << " values to " << bmap.size()
             << " bitvectors of " << nev << "-bit each in " << timer.realTime()
             << " sec(elapsed)";
        if (ibis::gVerbose > 30 || ((1U<<ibis::gVerbose)>bmap.size())) {
            lg() << "value, count (extracted from the bitvector)\n";
            for (it = bmap.begin(); it != bmap.end(); ++it)
                lg() << (*it).first << ",\t" << (*it).second->cnt() << "\n";
        }
    }
    else {
        LOGGER(ibis::gVerbose > 2) 
            << evt << " mapped " << nev << " values to " << bmap.size()
            << " bitvectors of " << nev << "-bit each";
    }
} // ibis::index::mapValues

template <typename E>
void ibis::index::mapValues(const array_t<E>& val, VMap& bmap) {
    bmap.clear();
    if (val.size() == 0) {
        LOGGER(ibis::gVerbose > 2)
            << "index::mapValues can not proceed with an empty input array";
        return;
    }

    uint32_t nev = val.size();
    VMap::iterator it;
    ibis::horometer timer;
    timer.start();
    for (uint32_t i = 0; i < nev; ++i) {
        it = bmap.find(val[i]);
        if (it != bmap.end()) {
            (*it).second->setBit(i, 1);
        }
        else {
            ibis::bitvector* tmp = new ibis::bitvector();
            tmp->setBit(i, 1);
            bmap[val[i]] = tmp;
        }
    }
    const uint32_t j = nev - 1;
    for (it = bmap.begin(); it != bmap.end(); ++it) {
        if ((*it).second->size() < nev) {
            (*it).second->setBit(j, 0);
        }
        else if ((*it).second->size() > nev) {
            ibis::util::logMessage
                ("index::mapValues", "bitvector for value %.9g "
                 "contains %lu bits while %lu are expected -- "
                 "removing the extra bits",
                 (*it).first, static_cast<long unsigned>((*it).second->size()),
                 static_cast<long unsigned>(nev));
            (*it).second->adjustSize(nev, nev);
        }
    }
    if (ibis::gVerbose > 4) {
        timer.stop();
        ibis::util::logMessage
            ("index::mapValues", "mapping an array[%lu] generated "
             "%lu bitvectors of %lu-bit each in %g sec(elapsed)",
             static_cast<long unsigned>(nev),
             static_cast<long unsigned>(bmap.size()),
             static_cast<long unsigned>(nev), timer.realTime());
        if (ibis::gVerbose > 30 || ((1U<<ibis::gVerbose)>bmap.size())) {
            ibis::util::logger lg;
            lg() << "value, count (extracted from the bitvector)\n";
            for (it = bmap.begin(); it != bmap.end(); ++it)
                lg() << (*it).first << ",\t" << (*it).second->cnt() << "\n";
        }
    }
    else if (ibis::gVerbose > 2) {
        ibis::util::logMessage
            ("index::mapValues", "mapping an array[%lu] found "
             "%lu unique values", static_cast<long unsigned>(nev),
             static_cast<long unsigned>(bmap.size()));
    }
} // ibis::index::mapValues

/// A brute-force approach to get an accurate distribution.
long ibis::index::getDistribution
(std::vector<double>& bds, std::vector<uint32_t>& cts) const {
    bds.clear();
    cts.clear();

    histogram hist;
    mapValues(0, hist);
    bds.reserve(hist.size());
    cts.reserve(hist.size());
    histogram::const_iterator it = hist.begin();
    cts.push_back((*it).second);
    for (++ it; it != hist.end(); ++ it) {
        bds.push_back((*it).first);
        cts.push_back((*it).second);
    }
    return cts.size();
} // ibis::index::getDistribution

/// A brute-force approach to get an accurate cumulative distribution.
long ibis::index::getCumulativeDistribution
(std::vector<double>& bds, std::vector<uint32_t>& cts) const {
    bds.clear();
    cts.clear();

    histogram hist;
    mapValues(0, hist);
    bds.reserve(hist.size());
    cts.reserve(hist.size());
    histogram::const_iterator it = hist.begin();
    cts.push_back((*it).second);
    uint32_t sum = (*it).second;
    for (++ it; it != hist.end(); ++ it) {
        sum += (*it).second;
        bds.push_back((*it).first);
        cts.push_back(sum);
    }
    if (bds.size() > 0) {
        double tmp = bds.back();
        bds.push_back(ibis::util::compactValue(tmp, (tmp>0?tmp+tmp:0.0)));
    }

    return cts.size();
} // ibis::index::getCumulativeDistribution

/// Compute a histogram of a column.  Given a property file containing the
/// values of a column, this function counts the occurances of each
/// distinct values.  Argument @c count is the number of samples to be
/// used for building the histogram.  If it is zero or greater than half of
/// the number of values in the data files, all values are used, otherwise,
/// approximately @c count values will be sampled with nearly uniform
/// distances from each other.
///
/// @note IMPROTANT ASSUMPTION.
/// A value of any supported type is supposed to be able to fit in a
/// double with no rounding, no approximation and no overflow.
void ibis::index::mapValues(const char* f, histogram& hist,
                            uint32_t count) const {
    if (col == 0 || col->partition() == 0) return;
    if (col->partition()->nRows() == 0) return;
    uint32_t i, k, nev;
    // TODO: implement a different algorithm, like sort the values first, to
    // make memory usage more predictable.  The numerous dynamically allocated
    // elements used by histogram really could slow down this function!

    horometer timer;
    std::string fnm; // name of the data file
    dataFileName(fnm, f);
    std::string evt = "index";
    if (ibis::gVerbose > 0) {
        evt += '[';
        if (col->partition() != 0) {
            evt += col->partition()->name();
            evt += '.';
        }
        evt += col->name();
        evt += ']';
    }
    evt += "::mapValues";
    if (ibis::gVerbose > 2 && ! fnm.empty()) {
        evt += '(';
        evt += fnm;
        evt += ')';
    }
    if (fnm.empty()) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " failed to determine the data file "
            "name from \"" << (f ? f : "") << '"';
        return;
    }
    if (ibis::gVerbose > 4) {
        timer.start();
        LOGGER(ibis::gVerbose > 4)
            << evt << " -- attempting to generate a histogram";
    }

    int ierr;
    histogram::iterator it;
    ibis::bitvector mask;
    col->getNullMask(mask);
    if (count > 0 && (mask.size() > 10000000 || count <
                      (mask.size() >> (col->elementSize() <= 4 ? 11 : 10)))) {
        ibis::bitvector pgm; // page mask
        const unsigned ntot = mask.size();
        const unsigned multip = ((ntot >> 12)>count ? (ntot >> 10)/count : 4);
        const unsigned stride = 1024 * multip;
        if (ibis::gVerbose > 2)
            col->logMessage("mapValues", "will sample 1024 values out of "
                            "every %u (total %lu)",
                            static_cast<unsigned>(stride),
                            static_cast<long unsigned>(ntot));
        for (i = 0; i < ntot; i += stride) {
            unsigned skip = static_cast<unsigned>(ibis::util::rand() * multip);
            if (skip > 0)
                pgm.appendFill(0, 1024*skip);
            pgm.appendFill(1, 1024);
            if (skip+2 < multip)
                pgm.appendFill(0, stride-1024*(skip+1));
        }
        pgm.adjustSize(0, mask.size());
        mask &= pgm;
    }

    // need to do different things for different columns
    switch (col->type()) {
    case ibis::TEXT:
    case ibis::UINT: {// unsigned int
        array_t<uint32_t> val;
        if (! fnm.empty())
            ierr = ibis::fileManager::instance().getFile(fnm.c_str(), val);
        else if (col->type() == ibis::UINT)
            ierr = col->getValuesArray(&val);
        else
            ierr = -1;
        if (ierr < 0 || val.empty()) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << evt << " failed to retrieve values";
            return;
        }

        nev = val.size();
        if (nev > mask.size())
            mask.adjustSize(nev, nev);
        ibis::bitvector::indexSet iset = mask.firstIndexSet();
        uint32_t nind = iset.nIndices();
        const ibis::bitvector::word_t *iix = iset.indices();
        while (nind) { // count the occurances
            if (iset.isRange()) { // a range
                k = (iix[1] < nev ? iix[1] : nev);
                for (i = *iix; i < k; ++i) {
                    it = hist.find(static_cast<double>(val[i]));
                    if (it != hist.end()) {
                        ++ ((*it).second);
                    }
                    else {
                        hist[val[i]] = 1;
                    }
                }
            }
            else if (*iix+ibis::bitvector::bitsPerLiteral() < nev) {
                // a list of indices within the valid range
                for (i = 0; i < nind; ++i) {
                    k = iix[i];
                    it = hist.find(static_cast<double>(val[k]));
                    if (it != hist.end()) {
                        ++ ((*it).second);
                    }
                    else {
                        hist[val[k]] = 1;
                    }
                }
            }
            else { // some indices may be out of the valid range
                for (i = 0; i < nind; ++i) {
                    k = iix[i];
                    if (k < nev) {
                        it = hist.find(static_cast<double>(val[k]));
                        if (it != hist.end()) {
                            ++ ((*it).second);
                        }
                        else {
                            hist[val[k]] = 1;
                        }
                    }
                }
            }
            ++iset;
            nind = iset.nIndices();
            if (*iix >= nev) nind = 0;
        } // while (nind)
        break;}
    case ibis::INT: {// signed int
        array_t<int32_t> val;
        if (! fnm.empty())
            ierr = ibis::fileManager::instance().getFile(fnm.c_str(), val);
        else
            ierr = col->getValuesArray(&val);
        if (ierr < 0 || val.empty()) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << evt << " failed to retrieve values";
            return;
        }

        nev = val.size();
        if (nev > mask.size())
            mask.adjustSize(nev, nev);
        ibis::bitvector::indexSet iset = mask.firstIndexSet();
        uint32_t nind = iset.nIndices();
        const ibis::bitvector::word_t *iix = iset.indices();
        while (nind) { // count the occurances
            if (iset.isRange()) { // a range
                k = (iix[1] < nev ? iix[1] : nev);
                for (i = *iix; i < k; ++i) {
                    it = hist.find(static_cast<double>(val[i]));
                    if (it != hist.end()) {
                        ++ ((*it).second);
                    }
                    else {
                        hist[val[i]] = 1;
                    }
                }
            }
            else if (*iix+ibis::bitvector::bitsPerLiteral() < nev) {
                // a list of indices within the valid range
                for (i = 0; i < nind; ++i) {
                    k = iix[i];
                    it = hist.find(static_cast<double>(val[k]));
                    if (it != hist.end()) {
                        ++ ((*it).second);
                    }
                    else {
                        hist[val[k]] = 1;
                    }
                }
            }
            else { // some indices may be out of the valid range
                for (i = 0; i < nind; ++i) {
                    k = iix[i];
                    if (k < nev) {
                        it = hist.find(static_cast<double>(val[k]));
                        if (it != hist.end()) {
                            ++ ((*it).second);
                        }
                        else {
                            hist[val[k]] = 1;
                        }
                    }
                }
            }
            ++iset;
            nind = iset.nIndices();
            if (*iix >= nev) nind = 0;
        } // while (nind)
        break;}
    case ibis::FLOAT: {// (4-byte) floating-point values
        array_t<float> val;
        if (! fnm.empty())
            ierr = ibis::fileManager::instance().getFile(fnm.c_str(), val);
        else
            ierr = col->getValuesArray(&val);
        if (ierr < 0 || val.empty()) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << evt << " failed to retrieve values";
            return;
        }

        nev = val.size();
        if (nev > mask.size())
            mask.adjustSize(nev, nev);
        ibis::bitvector::indexSet iset = mask.firstIndexSet();
        uint32_t nind = iset.nIndices();
        const ibis::bitvector::word_t *iix = iset.indices();
        while (nind) { // count the occurances
            if (iset.isRange()) { // a range
                k = (iix[1] < nev ? iix[1] : nev);
                for (i = *iix; i < k; ++i) {
                    it = hist.find(static_cast<double>(val[i]));
                    if (it != hist.end()) {
                        ++ ((*it).second);
                    }
                    else {
                        hist[val[i]] = 1;
                    }
                }
            }
            else if (*iix+ibis::bitvector::bitsPerLiteral() < nev) {
                // a list of indices within the valid range
                for (i = 0; i < nind; ++i) {
                    k = iix[i];
                    it = hist.find(static_cast<double>(val[k]));
                    if (it != hist.end()) {
                        ++ ((*it).second);
                    }
                    else {
                        hist[val[k]] = 1;
                    }
                }
            }
            else { // some indices may be out of the valid range
                for (i = 0; i < nind; ++i) {
                    k = iix[i];
                    if (k < nev) {
                        it = hist.find(static_cast<double>(val[k]));
                        if (it != hist.end()) {
                            ++ ((*it).second);
                        }
                        else {
                            hist[val[k]] = 1;
                        }
                    }
                }
            }
            ++iset;
            nind = iset.nIndices();
            if (*iix >= nev) nind = 0;
        } // while (nind)
        break;}
    case ibis::DOUBLE: {// (8-byte) floating-point values
        array_t<double> val;
        if (! fnm.empty())
            ierr = ibis::fileManager::instance().getFile(fnm.c_str(), val);
        else
            ierr = col->getValuesArray(&val);
        if (ierr < 0 || val.empty()) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << evt << " failed to retrieve values";
            return;
        }

        nev = val.size();
        if (nev > mask.size())
            mask.adjustSize(nev, nev);
        ibis::bitvector::indexSet iset = mask.firstIndexSet();
        uint32_t nind = iset.nIndices();
        const ibis::bitvector::word_t *iix = iset.indices();
        while (nind) { // count the occurances
            if (iset.isRange()) { // a range
                k = (iix[1] < nev ? iix[1] : nev);
                for (i = *iix; i < k; ++i) {
                    it = hist.find(val[i]);
                    if (it != hist.end()) {
                        ++ ((*it).second);
                    }
                    else {
                        hist[val[i]] = 1;
                    }
                }
            }
            else if (*iix+ibis::bitvector::bitsPerLiteral() < nev) {
                // a list of indices within the valid range
                for (i = 0; i < nind; ++i) {
                    k = iix[i];
                    it = hist.find(static_cast<double>(val[k]));
                    if (it != hist.end()) {
                        ++ ((*it).second);
                    }
                    else {
                        hist[val[k]] = 1;
                    }
                }
            }
            else { // some indices may be out of the valid range
                for (i = 0; i < nind; ++i) {
                    k = iix[i];
                    if (k < nev) {
                        it = hist.find(val[k]);
                        if (it != hist.end()) {
                            ++ ((*it).second);
                        }
                        else {
                            hist[val[k]] = 1;
                        }
                    }
                }
            }
            ++iset;
            nind = iset.nIndices();
            if (*iix >= nev) nind = 0;
        } // while (nind)
        break;}
    case ibis::BYTE: {// (1-byte) integer values
        array_t<signed char> val;
        if (! fnm.empty())
            ierr = ibis::fileManager::instance().getFile(fnm.c_str(), val);
        else
            ierr = col->getValuesArray(&val);
        if (ierr < 0 || val.empty()) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << evt << " failed to retrieve values";
            return;
        }

        nev = val.size();
        if (nev > mask.size())
            mask.adjustSize(nev, nev);
        ibis::bitvector::indexSet iset = mask.firstIndexSet();
        uint32_t nind = iset.nIndices();
        const ibis::bitvector::word_t *iix = iset.indices();
        while (nind) { // count the occurances
            if (iset.isRange()) { // a range
                k = (iix[1] < nev ? iix[1] : nev);
                for (i = *iix; i < k; ++i) {
                    it = hist.find(val[i]);
                    if (it != hist.end()) {
                        ++ ((*it).second);
                    }
                    else {
                        hist[val[i]] = 1;
                    }
                }
            }
            else if (*iix+ibis::bitvector::bitsPerLiteral() < nev) {
                // a list of indices within the valid range
                for (i = 0; i < nind; ++i) {
                    k = iix[i];
                    it = hist.find(static_cast<double>(val[k]));
                    if (it != hist.end()) {
                        ++ ((*it).second);
                    }
                    else {
                        hist[val[k]] = 1;
                    }
                }
            }
            else { // some indices may be out of the valid range
                for (i = 0; i < nind; ++i) {
                    k = iix[i];
                    if (k < nev) {
                        it = hist.find(val[k]);
                        if (it != hist.end()) {
                            ++ ((*it).second);
                        }
                        else {
                            hist[val[k]] = 1;
                        }
                    }
                }
            }
            ++iset;
            nind = iset.nIndices();
            if (*iix >= nev) nind = 0;
        } // while (nind)
        break;}
    case ibis::UBYTE: {// (1-byte) integer values
        array_t<unsigned char> val;
        if (! fnm.empty())
            ierr = ibis::fileManager::instance().getFile(fnm.c_str(), val);
        else
            ierr = col->getValuesArray(&val);
        if (ierr < 0 || val.empty()) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << evt << " failed to retrieve values";
            return;
        }

        nev = val.size();
        if (nev > mask.size())
            mask.adjustSize(nev, nev);
        ibis::bitvector::indexSet iset = mask.firstIndexSet();
        uint32_t nind = iset.nIndices();
        const ibis::bitvector::word_t *iix = iset.indices();
        while (nind) { // count the occurances
            if (iset.isRange()) { // a range
                k = (iix[1] < nev ? iix[1] : nev);
                for (i = *iix; i < k; ++i) {
                    it = hist.find(val[i]);
                    if (it != hist.end()) {
                        ++ ((*it).second);
                    }
                    else {
                        hist[val[i]] = 1;
                    }
                }
            }
            else if (*iix+ibis::bitvector::bitsPerLiteral() < nev) {
                // a list of indices within the valid range
                for (i = 0; i < nind; ++i) {
                    k = iix[i];
                    it = hist.find(static_cast<double>(val[k]));
                    if (it != hist.end()) {
                        ++ ((*it).second);
                    }
                    else {
                        hist[val[k]] = 1;
                    }
                }
            }
            else { // some indices may be out of the valid range
                for (i = 0; i < nind; ++i) {
                    k = iix[i];
                    if (k < nev) {
                        it = hist.find(val[k]);
                        if (it != hist.end()) {
                            ++ ((*it).second);
                        }
                        else {
                            hist[val[k]] = 1;
                        }
                    }
                }
            }
            ++iset;
            nind = iset.nIndices();
            if (*iix >= nev) nind = 0;
        } // while (nind)
        break;}
    case ibis::SHORT: {// (2-byte) integer values
        array_t<int16_t> val;
        if (! fnm.empty())
            ierr = ibis::fileManager::instance().getFile(fnm.c_str(), val);
        else
            ierr = col->getValuesArray(&val);
        if (ierr < 0 || val.empty()) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << evt << " failed to retrieve values";
            return;
        }

        nev = val.size();
        if (nev > mask.size())
            mask.adjustSize(nev, nev);
        ibis::bitvector::indexSet iset = mask.firstIndexSet();
        uint32_t nind = iset.nIndices();
        const ibis::bitvector::word_t *iix = iset.indices();
        while (nind) { // count the occurances
            if (iset.isRange()) { // a range
                k = (iix[1] < nev ? iix[1] : nev);
                for (i = *iix; i < k; ++i) {
                    it = hist.find(val[i]);
                    if (it != hist.end()) {
                        ++ ((*it).second);
                    }
                    else {
                        hist[val[i]] = 1;
                    }
                }
            }
            else if (*iix+ibis::bitvector::bitsPerLiteral() < nev) {
                // a list of indices within the valid range
                for (i = 0; i < nind; ++i) {
                    k = iix[i];
                    it = hist.find(static_cast<double>(val[k]));
                    if (it != hist.end()) {
                        ++ ((*it).second);
                    }
                    else {
                        hist[val[k]] = 1;
                    }
                }
            }
            else { // some indices may be out of the valid range
                for (i = 0; i < nind; ++i) {
                    k = iix[i];
                    if (k < nev) {
                        it = hist.find(val[k]);
                        if (it != hist.end()) {
                            ++ ((*it).second);
                        }
                        else {
                            hist[val[k]] = 1;
                        }
                    }
                }
            }
            ++iset;
            nind = iset.nIndices();
            if (*iix >= nev) nind = 0;
        } // while (nind)
        break;}
    case ibis::USHORT: {// (2-byte) integer values
        array_t<uint16_t> val;
        if (! fnm.empty())
            ierr = ibis::fileManager::instance().getFile(fnm.c_str(), val);
        else
            ierr = col->getValuesArray(&val);
        if (ierr < 0 || val.empty()) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << evt << " failed to retrieve values";
            return;
        }

        nev = val.size();
        if (nev > mask.size())
            mask.adjustSize(nev, nev);
        ibis::bitvector::indexSet iset = mask.firstIndexSet();
        uint32_t nind = iset.nIndices();
        const ibis::bitvector::word_t *iix = iset.indices();
        while (nind) { // count the occurances
            if (iset.isRange()) { // a range
                k = (iix[1] < nev ? iix[1] : nev);
                for (i = *iix; i < k; ++i) {
                    it = hist.find(val[i]);
                    if (it != hist.end()) {
                        ++ ((*it).second);
                    }
                    else {
                        hist[val[i]] = 1;
                    }
                }
            }
            else if (*iix+ibis::bitvector::bitsPerLiteral() < nev) {
                // a list of indices within the valid range
                for (i = 0; i < nind; ++i) {
                    k = iix[i];
                    it = hist.find(static_cast<double>(val[k]));
                    if (it != hist.end()) {
                        ++ ((*it).second);
                    }
                    else {
                        hist[val[k]] = 1;
                    }
                }
            }
            else { // some indices may be out of the valid range
                for (i = 0; i < nind; ++i) {
                    k = iix[i];
                    if (k < nev) {
                        it = hist.find(val[k]);
                        if (it != hist.end()) {
                            ++ ((*it).second);
                        }
                        else {
                            hist[val[k]] = 1;
                        }
                    }
                }
            }
            ++iset;
            nind = iset.nIndices();
            if (*iix >= nev) nind = 0;
        } // while (nind)
        break;}
    case ibis::LONG: {// (8-byte) integer values
        array_t<int64_t> val;
        if (! fnm.empty())
            ierr = ibis::fileManager::instance().getFile(fnm.c_str(), val);
        else
            ierr = col->getValuesArray(&val);
        if (ierr < 0 || val.empty()) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << evt << " failed to retrieve values";
            return;
        }

        nev = val.size();
        if (nev > mask.size())
            mask.adjustSize(nev, nev);
        ibis::bitvector::indexSet iset = mask.firstIndexSet();
        uint32_t nind = iset.nIndices();
        const ibis::bitvector::word_t *iix = iset.indices();
        while (nind) { // count the occurances
            if (iset.isRange()) { // a range
                k = (iix[1] < nev ? iix[1] : nev);
                for (i = *iix; i < k; ++i) {
                    it = hist.find(val[i]);
                    if (it != hist.end()) {
                        ++ ((*it).second);
                    }
                    else {
#if DEBUG+0 > 1 || _DEBUG+0 > 1
                        LOGGER(ibis::gVerbose > 5)
                            << "DEBUG -- index::mapValues adding value "
                            << val[i] << " to the histogram of size "
                            << hist.size();
#endif
                        hist[val[i]] = 1;
                    }
                }
            }
            else if (*iix+ibis::bitvector::bitsPerLiteral() < nev) {
                // a list of indices within the valid range
                for (i = 0; i < nind; ++i) {
                    k = iix[i];
                    it = hist.find(static_cast<double>(val[k]));
                    if (it != hist.end()) {
                        ++ ((*it).second);
                    }
                    else {
                        hist[val[k]] = 1;
                    }
                }
            }
            else { // some indices may be out of the valid range
                for (i = 0; i < nind; ++i) {
                    k = iix[i];
                    if (k < nev) {
                        it = hist.find(val[k]);
                        if (it != hist.end()) {
                            ++ ((*it).second);
                        }
                        else {
                            hist[val[k]] = 1;
                        }
                    }
                }
            }
            ++iset;
            nind = iset.nIndices();
            if (*iix >= nev) nind = 0;
        } // while (nind)
        break;}
    case ibis::ULONG: {// unsigned (8-byte) integer values
        array_t<uint64_t> val;
        if (! fnm.empty())
            ierr = ibis::fileManager::instance().getFile(fnm.c_str(), val);
        else
            ierr = col->getValuesArray(&val);
        if (ierr < 0 || val.empty()) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << evt << " failed to retrieve values";
            return;
        }

        nev = val.size();
        if (nev > mask.size())
            mask.adjustSize(nev, nev);
        ibis::bitvector::indexSet iset = mask.firstIndexSet();
        uint32_t nind = iset.nIndices();
        const ibis::bitvector::word_t *iix = iset.indices();
        while (nind) { // count the occurances
            if (iset.isRange()) { // a range
                k = (iix[1] < nev ? iix[1] : nev);
                for (i = *iix; i < k; ++i) {
                    it = hist.find(val[i]);
                    if (it != hist.end()) {
                        ++ ((*it).second);
                    }
                    else {
                        hist[val[i]] = 1;
                    }
                }
            }
            else if (*iix+ibis::bitvector::bitsPerLiteral() < nev) {
                // a list of indices within the valid range
                for (i = 0; i < nind; ++i) {
                    k = iix[i];
                    it = hist.find(static_cast<double>(val[k]));
                    if (it != hist.end()) {
                        ++ ((*it).second);
                    }
                    else {
                        hist[val[k]] = 1;
                    }
                }
            }
            else { // some indices may be out of the valid range
                for (i = 0; i < nind; ++i) {
                    k = iix[i];
                    if (k < nev) {
                        it = hist.find(val[k]);
                        if (it != hist.end()) {
                            ++ ((*it).second);
                        }
                        else {
                            hist[val[k]] = 1;
                        }
                    }
                }
            }
            ++iset;
            nind = iset.nIndices();
            if (*iix >= nev) nind = 0;
        } // while (nind)
        break;}
    case ibis::CATEGORY: // no need for a separate index
        col->logWarning("index::mapValues", "no value to compute a "
                        "histogram -- use the basic bitmap index for the "
                        "same information");
        hist.clear();
        break;
    default:
        col->logWarning("index::mapValues", "failed to create a histogram "
                        "for this type of column");
        break;
    }

    if (ibis::gVerbose > 4) {
        timer.stop();
        col->logMessage("index::mapValues", "generated histogram (%lu "
                        "distinct value%s) in %g sec(elapsed)",
                        static_cast<long unsigned>(hist.size()),
                        (hist.size()>1?"s":""), timer.realTime());
        if (ibis::gVerbose > 30 || ((1U<<ibis::gVerbose)>hist.size())) {
            ibis::util::logger lg;
            lg() << "value, count\n";
            for (it = hist.begin(); it != hist.end(); ++it)
                lg() << (*it).first << ",\t" << (*it).second << "\n";
        }
    }
    else if (ibis::gVerbose > 2) {
        col->logMessage("index::mapValues", "generated histogram (%lu "
                        "distinct value%s)",
                        static_cast<long unsigned>(hist.size()),
                        (hist.size()>1?"s":""));
    }
} // ibis::index::mapValues

template <typename E>
void ibis::index::mapValues(const array_t<E>& val, histogram& hist,
                            uint32_t count) {
    if (val.empty()) return;
    horometer timer;
    const uint32_t nev = val.size();
    uint32_t stride = 1;
    histogram::iterator it;

    if (count > 0 && count+count <= nev)
        stride = static_cast<uint32_t>(0.5 + static_cast<double>(nev)
                                       / static_cast<double>(count));
    if (ibis::gVerbose > 4) {
        timer.start();
        ibis::util::logMessage("index::mapValues", "starting to count the "
                               "frequencies of %s[%lu] with stride %lu",
                               typeid(E).name(),
                               static_cast<long unsigned>(nev),
                               static_cast<long unsigned>(stride));
    }
    if (stride <= 2) {
        for (uint32_t i = 0; i < nev; ++i) {
            it = hist.find(static_cast<double>(val[i]));
            if (it != hist.end()) {
                ++ ((*it).second);
            }
            else {
                hist[val[i]] = 1;
            }
        }
    }
    else { // initial stride > 2
        uint32_t cnt = 0;
        for (uint32_t i = 0; i < nev; i += stride) {
            it = hist.find(static_cast<double>(val[i]));
            if (it != hist.end()) {
                ++ ((*it).second);
            }
            else {
                hist[val[i]] = 1;
            }
            ++ cnt;
            if (cnt < count) // adjust stride as needed
                stride = (nev-i > count-cnt ? (nev-i)/(count-cnt) : 1);
            else
                break;
        }
    }

    if (ibis::gVerbose > 4) {
        timer.stop();
        ibis::util::logMessage
            ("index::mapValues", "generated histogram (%lu "
             "distinct value%s) in %g sec(elapsed)",
             static_cast<long unsigned>(hist.size()), (hist.size()>1?"s":""),
             timer.realTime());
        if (ibis::gVerbose > 30 || ((1U<<ibis::gVerbose)>hist.size())) {
            ibis::util::logger lg;
            lg() << "value, count\n";
            for (it = hist.begin(); it != hist.end(); ++it)
                lg() << (*it).first << ",\t" << (*it).second << "\n";
        }
    }
    else if (ibis::gVerbose > 2) {
        ibis::util::logMessage("index::mapValues", "generated histogram (%lu "
                               "distinct value%s)",
                               static_cast<long unsigned>(hist.size()),
                               (hist.size()>1?"s":""));
    }
} // ibis::index::mapValues

template <typename E>
void ibis::index::mapValues(const array_t<E>& val, array_t<E>& bounds,
                            std::vector<uint32_t>& cnts) {
    if (val.size() == 0)
        return;
    bool existing = ! bounds.empty();
    for (uint32_t i = 1; i < bounds.size() && existing; ++ i)
        existing = (bounds[i] > bounds[i-1]);
    if (! existing) { // need to generate boundaries
        E amin = val.front();
        E amax = val.front();
        for (uint32_t i = 1; i < val.size(); ++ i) {
            if (amin > val[i])
                amin = val[i];
            else if (amax < val[i])
                amax = val[i];
        }
        E diff = (amax - amin) / 1024;
        if (diff > 0) {
            uint32_t cnt = static_cast<uint32_t>((amax - amin) / diff);
            bounds.reserve(cnt);
            for (uint32_t i = 1; i <= cnt; ++ i)
                bounds.push_back(amin + i * diff);
        }
        else {
            uint32_t cnt = static_cast<uint32_t>(amax - amin);
            bounds.reserve(cnt);
            for (uint32_t i = 1; i <= cnt; ++ i)
                bounds.push_back(amin + i);
        }
    }

    const uint32_t nbounds = bounds.size();
    if (cnts.size() != nbounds+1) {
        cnts.resize(nbounds+1);
        for (uint32_t i = 0; i <= nbounds; ++ i)
            cnts[i] = 0;
    }

    for (uint32_t i = 0; i < val.size(); ++ i) {
        uint32_t j1 = bounds.find(val[i]);
        if (j1 < nbounds)
            j1 += (val[i] == bounds[j1]);
        else
            j1 = nbounds;
        ++ cnts[j1];
    }
} // ibis::index::mapValues

/// Compute a two-dimensional histogram.  Given two arrays of the same
/// size, count the number of appearance of each combinations defined by @c
/// bnd1 and @c bnd2.  If the arrays @c bnd1 or @c bnd2 contain values in
/// ascending order, their values are directly used in this function.  The
/// empty one will be replaced by a linear division of actual range into
/// 256 bins.  The array @c counts stores the 2-D bins in raster-scan order
/// with the second variable, @c val2, as the faster varying variables.
/// More specifically, the bins for variable 1 are defined as:
/// \verbatim
/// (..., bnd1[0]) [bnd1[1], bin1[2]) [bnd1[2], bin1[3) ... [bnd1.back(), ...)
/// \endverbatim
/// Note that '[' denote the left boundary is inclusive and ')' denote the
/// right boundary is exclusive.
/// Similarly, the bins for variable 2 are
/// \verbatim
/// (..., bnd2[0]) [bnd2[1], bin2[2]) [bnd2[2], bin2[3) ... [bnd2.back(), ...)
/// \endverbatim
/// The @c counts are for the following bins
/// \verbatim
/// (..., bin1[0]) (.... bin2[0])
/// (..., bin1[0]) [bin2[0], bin2[1])
/// (..., bin1[0]) [bin2[1], bin2[2])
/// ...
/// (..., bin1[0]) [bin2.back(), ...)
/// [bin1[0], bin1[1]) (..., bin2[0])
/// [bin1[0], bin1[1]) [bin2[0], bin2[1])
/// [bin1[0], bin1[1]) [bin2[1], bin2[2])
/// ...
/// [bin1[0], bin1[1]) [bin2.back(), ...)
/// ...
/// \endverbatim
template <typename E1, typename E2>
void ibis::index::mapValues(const array_t<E1>& val1, const array_t<E2>& val2,
                            array_t<E1>& bnd1, array_t<E2>& bnd2,
                            std::vector<uint32_t>& cnts) {
    if (val1.size() == 0 || val2.size() == 0 || val1.size() != val2.size())
        return;
    bool sorted = ! bnd1.empty();
    for (uint32_t i = 1; i < bnd1.size() && sorted; ++ i)
        sorted = (bnd1[i] > bnd1[i-1]);
    if (bnd1.size() == 0 || ! sorted) { // need to generate boundaries
        E1 amin = val1.front();
        E1 amax = val1.front();
        for (uint32_t i = 1; i < val1.size(); ++ i) {
            if (amin > val1[i])
                amin = val1[i];
            else if (amax < val1[i])
                amax = val1[i];
        }
        E1 diff = (amax - amin) / 255;
        if (diff > 0) {
            uint32_t cnt = static_cast<uint32_t>((amax - amin) / diff);
            bnd1.reserve(cnt);
            for (uint32_t i = 1; i <= cnt; ++ i)
                bnd1.push_back(amin + i * diff);
        }
        else {
            uint32_t cnt = static_cast<uint32_t>(amax - amin);
            bnd1.reserve(cnt);
            for (uint32_t i = 1; i <= cnt; ++ i)
                bnd1.push_back(amin + i);
        }
    }
    sorted = ! bnd2.empty();
    for (uint32_t i = 1; i < bnd2.size() && sorted; ++ i)
        sorted = (bnd2[i] > bnd2[i-1]);
    if (bnd2.size() == 0 || ! sorted) { // need to generate boundaries
        E2 amin = val2.front();
        E2 amax = val2.front();
        for (uint32_t i = 1; i < val2.size(); ++ i) {
            if (amin > val2[i])
                amin = val2[i];
            else if (amax < val2[i])
                amax = val2[i];
        }
        E2 diff = (amax - amin) / 255;
        if (diff > 0) {
            uint32_t cnt = static_cast<uint32_t>((amax - amin) / diff);
            bnd2.reserve(cnt);
            for (uint32_t i = 1; i <= cnt; ++ i)
                bnd2.push_back(amin + i * diff);
        }
        else {
            uint32_t cnt = static_cast<uint32_t>(amax - amin);
            bnd2.reserve(cnt);
            for (uint32_t i = 1; i <= cnt; ++ i)
                bnd2.push_back(amin + i);
        }
    }

    const uint32_t nbnd1 = bnd1.size();
    const uint32_t nbnd2 = bnd2.size();
    const uint32_t nb2p1 = bnd2.size() + 1;
    if (cnts.size() != (nbnd1+1) * nb2p1) {
        cnts.resize(nb2p1 * (nbnd1+1));
        for (uint32_t i = 0; i < nb2p1 * (nbnd1+1); ++ i)
            cnts[i] = 0;
    }

    for (uint32_t i = 0; i < val1.size(); ++ i) {
        uint32_t j1 = bnd1.find(val1[i]);
        uint32_t j2 = bnd2.find(val2[i]);
        if (j1 < nbnd1)
            j1 += (val1[i] == bnd1[j1]);
        else
            j1 = nbnd1;
        if (j2 < nbnd2)
            j2 += (val2[i] == bnd2[j2]);
        else
            j2 = nbnd2;
        ++ cnts[j1*nb2p1 + j2];
    }
} // ibis::index::mapValues

/// @note The array @c bdry stores the dividers.  The first group is [0,
/// bdry[0]), the second group is [bdry[0], bdry[1]), and so on.   Ths
/// function uses the size of array @c bdry to determine the number of
/// groups to use.
void ibis::index::divideCounts(array_t<uint32_t>& bdry,
                               const array_t<uint32_t>& cnt) {
    if (bdry.empty())
        return;

    const uint32_t nb = bdry.size();
    const uint32_t ncnt = cnt.size();
    uint32_t i, j, avg=0, top=0;
    if (nb*3/2 >= ncnt) {
        bdry.resize(ncnt);
        for (i=0; i<ncnt; ++i)
            bdry[i] = i + 1;
        return;
    }

    array_t<uint32_t> weight(nb); // a temperory array
    for (i=0; i<ncnt; ++i) {
        avg += cnt[i];
        if (top < cnt[i])
            top = cnt[i];
    }
    avg = (avg + (nb>>1)) / nb; // round to the nearest integer
    if (top < avg) { // no isolated values with high counts
        top = cnt[0];
        i = 1;
        j = 0;
        while (i < ncnt && j < nb) {
            if (top + cnt[i] < avg) {
                top += cnt[i];
            }
            else if (top + cnt[i] == avg) {
                weight[j] = avg;
                bdry[j] = i + 1;
                ++ j;
                ++ i;
                if (i < ncnt)
                    top = cnt[i];
                else
                    top = 0;
            }
            else if (j > 0 && weight[j-1] > avg) {
                // previous bin is somewhat heavy, prefer a lighter bin
                if (top > 0.9*avg) {
                    weight[j] = top;
                    bdry[j] = i;
                    ++ j;
                    top = cnt[i];
                }
                else if (top + cnt[i] < 1.2*avg) {
                    weight[j] = top + cnt[i];
                    bdry[j] = i + 1;
                    ++ j;
                    ++ i;
                    if (i < ncnt)
                        top = cnt[i];
                    else
                        top = 0;
                }
                else if (top > 0.7*avg) {
                    weight[j] = top;
                    bdry[j] = i;
                    ++ j;
                    top = cnt[i];
                }
                else if (top + cnt[i] < 1.4*avg) {
                    weight[j] = top + cnt[i];
                    bdry[j] = i + 1;
                    ++ j;
                    ++ i;
                    if (i < ncnt)
                        top = cnt[i];
                    else
                        top = 0;
                }
                else {
                    weight[j] = top;
                    bdry[j] = i;
                    ++ j;
                    top = cnt[i];
                }
            }
            // the next part attempts to put the current group into a bin
            // that is slightly slighter
            else if (top + cnt[i] < 1.1*avg) {
                weight[j] = top + cnt[i];
                bdry[j] = i + 1;
                ++ j;
                ++ i;
                if (i < ncnt)
                    top = cnt[i];
                else
                    top = 0;
            }
            else if (top > 0.8*avg) {
                weight[j] = top;
                bdry[j] = i;
                ++ j;
                top = cnt[i];
            }
            else if (top + cnt[i] < 1.3*avg) {
                weight[j] = top + cnt[i];
                bdry[j] = i + 1;
                ++ j;
                ++ i;
                if (i < ncnt)
                    top = cnt[i];
                else
                    top = 0;
            }
            else if (top > 0.6*avg) {
                weight[j] = top;
                bdry[j] = i;
                ++ j;
                top = cnt[i];
            }
            else {
                weight[j] = top + cnt[i];
                bdry[j] = i + 1;
                ++ j;
                ++ i;
                if (i < ncnt)
                    top = cnt[i];
                else
                    top = 0;
            }
            ++i;
        } // while (i < ncnt && j < nb)
        if (top > 0) { // deal with events that have not been put in to a bin
            if (j < nb) { // the last bin
                weight[j] = top;
                bdry[j] = ncnt;
                ++ j;
            }
            else { // count the remaining events
                while (i < ncnt) {
                    top += cnt[i];
                    ++i;
                }
                if (weight[j-1] + top < (avg<<1)) { // merge with the last bin
                    weight[j-1] += top;
                    bdry[j-1] = ncnt;
                }
                else { // make a new bin
                    weight.push_back(top);
                    bdry.push_back(ncnt);
                    j = bdry.size();
                }
            }
        }
        if (j < nb) { // have put too many events into first j bins
            bool dosplit = false;
            do {
                // attempt to find the last heaviest bin
                for (i = 1, top = 0; i < j; ++i) {
                    if (weight[i] >= weight[top])
                        top = i;
                }
                // attempt to split bin i and later
                dosplit = false;
                for (i = top; i < j; ++i) {
                    if (i > 0)
                        dosplit = (bdry[i]>bdry[i-1]+1);
                    else
                        dosplit = (bdry[0]>1);
                    if (dosplit) { // move the last value to the next bin
                        -- bdry[i];
                        weight[i] -= cnt[bdry[i]];
                        if (i+1 < j) {
                            weight[i+1] += cnt[bdry[i]];
                        }
                        else { // make a new bin
                            weight[i+1] = cnt[bdry[i]];
                            bdry[i+1] = ncnt;
                        }
                    }
                }
                j += (dosplit == true);
            } while (j < nb && dosplit);
            if (j < nb) {
                bdry.resize(j);
                weight.resize(j);
            }
        }

        // attempt to move the bin boundaries around to get more uniform bins
        bool doadjust = (bdry.size()>2);
        while (doadjust) {
            if (ibis::gVerbose > 12) {
                array_t<uint32_t>::const_iterator it;
                ibis::util::logger lg;
                lg() << "divideCounts(): smoothing --\n bounds("
                     << bdry.size() << ") = [";
                for (it = bdry.begin(); it != bdry.end(); ++it)
                    lg() << " " << *it;
                lg() << "]\nweights(" << bdry.size() << ") = [";
                for (it = weight.begin(); it != weight.end(); ++it)
                    lg() << " " << *it;
                lg() << "]\n";
            }

            // locate the largest difference between two borders
            int diff = weight[1] - weight[0];
            j = 1;
            for (i = 2; i < bdry.size(); ++i) {
                int tmp = weight[i] - weight[i-1];
                if ((diff>=0?diff:-diff) < (tmp>=0?tmp:-tmp)) {
                    diff = tmp;
                    j = i;
                }
            }
            doadjust = false;
            // can the border be moved to reduce the difference
            if (diff > 0) { // weight[j] > weight[j-1]
                if (weight[j-1]+cnt[bdry[j-1]] < weight[j]) {
                    diff >>= 1;
                    doadjust = true;
                    if (cnt[bdry[j-1]] > static_cast<uint32_t>(diff)) {
                        // move only one value
                        weight[j-1] += cnt[bdry[j-1]];
                        weight[j] -= cnt[bdry[j-1]];
                        ++ bdry[j-1];
                    }
                    else { // may move more than one value
                        for (i=bdry[j-1]+1, top=cnt[bdry[j-1]];
                             top <= static_cast<uint32_t>(diff); ++i)
                            top += cnt[i];
                        --i;
                        top -= cnt[i];
                        weight[j-1] += top;
                        weight[j] -= top;
                        bdry[j-1] = i;
                    }
                }
                else if (j>1 && weight[j-1]+cnt[bdry[j-1]]-cnt[bdry[j-2]] <
                         weight[j]) {
                    doadjust = true;
                    for (i = j-1; doadjust && i>1; --i) {
                        if (weight[i-1]+cnt[bdry[i-1]] < weight[j]) {
                            break;
                        }
                        else {
                            doadjust = ((weight[i-1]+cnt[bdry[i-1]]-
                                         cnt[bdry[i-2]]) < weight[j]);
                        }
                    }
                    if (i == 1 && doadjust)
                        doadjust = (weight[0]+cnt[bdry[0]] < weight[j]);
                    if (doadjust) { // actually change the borders
                        while (i <= j) {
                            weight[i-1] += cnt[bdry[i-1]];
                            weight[i] -= cnt[bdry[i-1]];
                            ++ bdry[i-1];
                            ++ i;
                        }
                    }
                }
            }
            else if (diff < 0) { // weight[j] < weight[j-1]
                if (weight[j-1] > weight[j]+cnt[bdry[j-1]-1]) {
                    doadjust = true;
                    diff = -diff / 2;
                    if (cnt[bdry[j-1]-1] > static_cast<uint32_t>(diff)) {
                        // move only one value
                        -- bdry[j-1];
                        weight[j] += cnt[bdry[j-1]];
                        weight[j-1] -= cnt[bdry[j-1]];
                    }
                    else { // may move more than one value
                        for (i=bdry[j-1]-2, top=cnt[bdry[j-1]-1];
                             top+cnt[i] <= static_cast<uint32_t>(diff); --i)
                            top += cnt[i];
                        ++i;
                        bdry[j-1] = i;
                        weight[j] += top;
                        weight[j-1] -= top;
                    }
                }
                else if (weight[j-1]-cnt[bdry[j-1]-1] >
                         weight[j]-cnt[bdry[j]-1]) {
                    doadjust = (j+1 < weight.size());
                    for (i = j+1; doadjust && i < weight.size(); ++i) {
                        if (weight[j-1] > weight[i]+cnt[bdry[i-1]-1]) {
                            break;
                        }
                        else {
                            doadjust = ((i+1 < weight.size()) &&
                                        (weight[i-1]-cnt[bdry[i-1]-1] >
                                         weight[i]-cnt[bdry[i]-1]));
                        }
                    }
                    if (doadjust) { // actually change the borders
                        while (i >= j) {
                            -- bdry[i-1];
                            weight[i] += cnt[bdry[i-1]];
                            weight[i-1] -= cnt[bdry[i-1]];
                            -- i;
                        }
                    }
                }
            }
        } // while (doadjust)
    }
    else { // got some values with very large counts
        // first locate the values with heavy counts
        for (i = 0, j = 0; i < ncnt && j < nb; ++i) {
            if (cnt[i] >= avg) {
                weight[j] = i; // mark the position of the heavy counts
                ++ j;
                LOGGER(ibis::gVerbose > 4)
                    << "index::divideCounts -- treating bin " << i
                    << " as heavy (weight = " << cnt[i] << ")";
            }
        }
        if (i < ncnt || j >= nb) {
            // special case -- all values have equal counts
            avg = ncnt / nb;
            j = ncnt % nb;
            for (i = 0, top = 0; i < j; ++i) {
                top += avg+1;
                bdry[i] = top;
            }
            for (; i < nb; ++i) {
                top += avg;
                bdry[i] = top;
            }
            return;
        }

        // count the number of events in-between the heavy ones and decide
        // how many bins they will get
        // cnt2 stores the number of events
        // nb2  stores the number of bins to use
        weight.resize(j);
        array_t<uint32_t> cnt2(j+1);
        cnt2[0] = 0;
        avg = 0;
        for (i = 0; i < weight[0]; ++ i)
            cnt2[0] += cnt[i];
        avg += cnt2[0];
        for (i = 1; i < j; ++ i) {
            cnt2[i] = 0;
            for (uint32_t ki = weight[i-1]+1; ki < weight[i]; ++ ki)
                cnt2[i] += cnt[ki];
            avg += cnt2[i];
        }
        cnt2[j] = 0;
        for (i=weight.back()+1; i<ncnt; ++i)
            cnt2[j] += cnt[i];
        avg += cnt2[j];
        avg = ((avg > nb-j) ? (avg + ((nb-j)>>1)) / (nb-j) : 1);
        top = (avg >> 1);

        // initial assignment of the number of bins to use
        array_t<uint32_t> nb2(j+1);
        for (i = 0; i <= j; ++ i) {
            nb2[i] = (top + cnt2[i]) / avg; // round to nearest integer
            if (nb2[i] == 0 && cnt2[i] > 0) {
                nb2[i] = 1;
            }
            else if (i == j) {
                if (nb2[i] > ncnt - weight.back() - 1)
                    nb2[i] = ncnt - weight.back() - 1;
            }
            else if (i > 0) {
                if (nb2[i] > weight[i] - weight[i-1] - 1)
                    nb2[i] = weight[i] - weight[i-1] - 1;
            }
            else if (i == 0 && nb2[0] > weight[0]) {
                nb2[0] = weight[0];
            }
        }

        // attempt to make sure the total number of bins is exactly nb
        // avg stores the total number of bins
        for (i=0, avg=j; i<j+1; ++i)
            avg += nb2[i];
        while (avg > nb) { // need to reduce the number of bins
            top = 0;
            double frac = DBL_MAX;
            // find the interval containing the most bins
            if (nb2[0] > 1)
                frac = static_cast<double>(cnt2[0]) / nb2[0];
            for (i = 1; i <= j; ++i) {
                if (nb2[i] > 1) {
                    if (frac < DBL_MAX) {
                        if (frac*nb2[i] < cnt2[i]) {
                            top = i;
                            frac = static_cast<double>(cnt2[i]) / nb2[i];
                        }
                        else if (frac*nb2[i]==cnt2[i] &&
                                 cnt2[i]>cnt2[top]) {
                            top = i;
                            frac = static_cast<double>(cnt2[i]) / nb2[i];
                        }
                    }
                    else {
                        top = i;
                        frac = static_cast<double>(cnt2[i]) / nb2[i];
                    }
                }
            }
            if (frac == DBL_MAX) { // can not use less bins
                break;
            }
            // reduce the number of bins in the range top
            -- nb2[top];
            -- avg;
        } // while (avg > nb)
        while (avg < nb) { // need to use more bins
            top = 0;
            double frac = (nb2[0] < weight[0] ?
                           (nb2[0]>0 ? cnt2[0]/nb2[0] : cnt2[0]) : 0.0);
            // find the crowdest bin
            for (i = 1; i <= j; ++ i) {
                if (nb2[i] > 0 && nb2[i] <
                    (i<j ? nb2[i] > weight[i] - weight[i-1] - 1 :
                     ncnt - weight.back() - 1)) {
                    if (frac*nb2[i] > cnt2[i]) {
                        top = i;
                        frac = static_cast<double>(cnt2[i]) / nb2[i];
                    }
                    else if (frac*nb2[i]==cnt2[i] && cnt2[i]>cnt2[top]) {
                        top = i;
                        frac = static_cast<double>(cnt2[i]) / nb2[i];
                    }
                    else if (frac <= 0.0) {
                        top = i;
                        frac = static_cast<double>(cnt2[i]) / nb2[i];
                    }
                }
            }
            if (frac == 0.0) { // can not use more bins
                break;
            }
            // increase the number of bins in the range top
            ++ nb2[top];
            ++ avg;
        } // while (avg < nb)

        // actually establish the boundaries
        if (nb2[0] > 1) {
            bdry.resize(nb2[0]);
            const array_t<uint32_t> tmp(cnt, 0, weight[0]);
            LOGGER(ibis::gVerbose > 6)
                << "index::divideCounts -- attempting to divide [0, "
                << weight[0] << ") into " << nb2[0] << " bins";
            divideCounts(bdry, tmp);
        }
        else if (nb2[0] == 1) {
            bdry[0] = weight[0];
            bdry.resize(1);
        }
        else {
            bdry.clear();
        }
        for (i=0; i<j; ++i) {
            uint32_t off = weight[i] + 1;
            bdry.push_back(off);
            if (nb2[i+1] > 1) {
                const array_t<uint32_t> tmp(cnt, off,
                                            (i+1<j?weight[i+1]:ncnt));
                array_t<uint32_t> bnd(nb2[i+1]);
                LOGGER(ibis::gVerbose > 6)
                    << "index::divideCounts -- attempting to divide [" << off
                    <<", " << (i+1<j?weight[i+1]:ncnt) << ") into "
                    << nb2[i+1] << " bins";
                divideCounts(bnd, tmp);
                for (array_t<uint32_t>::const_iterator it = bnd.begin();
                     it != bnd.end(); ++it)
                    bdry.push_back(off + *it);
            }
            else if (nb2[i+1] == 1) {
                bdry.push_back(i+1<j?weight[i+1]:ncnt);
            }
        }
    }

    if (ibis::gVerbose > 8) {
        ibis::util::logger lg;
        lg() << "index::divideCounts results (i, cnt[i], sum cnt[i])\n";
        for (i = 0, top=0; i < bdry[0]; ++i) {
            top += cnt[i];
            lg() << i << "\t" << cnt[i] << "\t" << top << "\n";
        }
        if (bdry[0] > 0) {
            lg() << "-^- bin 0 -^-\n";
        }
        else {
            lg() << "index::divideCounts -- bin 0 is empty\n";
        }
        for (j = 1; j < bdry.size(); ++j) {
            for (i = bdry[j-1], top = 0; i < bdry[j]; ++i) {
                top += cnt[i];
                if (i < (bdry[j-1]+(1<<ibis::gVerbose))) {
                    lg() << i << "\t" << cnt[i] << "\t" << top << "\n";
                }
                else if (i+1 == bdry[j]) {
                    if (i > (bdry[j-1]+(1<<ibis::gVerbose)))
                        lg() << "...\n";
                    lg() << i << "\t" << cnt[i] << "\t" << top << "\n";
                }
            }
            if (bdry[j] > bdry[j-1]) {
                lg() << "-^- bin " << j << "\n";
            }
            else {
                lg() << "index::divideCounts -- bin: " << j << " ["
                     << bdry[j-1] << ", " << bdry[j] << ") is empty\n";
            }
        }
    }
    else {
        weight.resize(bdry.size()); // actually compute the weights
        for (i = 0; i < bdry.size(); ++i) {
            weight[i] = 0;
            for (j = (i==0 ? 0 : bdry[i-1]); j < bdry[i]; ++j)
                weight[i] += cnt[j];
            LOGGER(ibis::gVerbose > 2 && weight[i] == 0)
                << "index::divideCounts -- bin:" << i << " ["
                << (i==0 ? 0 : bdry[i-1]) << ", " << bdry[i] << ") is empty";
        }

        if (ibis::gVerbose > 6) {
            array_t<uint32_t>::const_iterator it;
            ibis::util::logger lg;
            lg() << "index::divideCounts\n    cnt(" << ncnt << ") = [";
            if (ncnt < 256) {
                for (it = cnt.begin(); it != cnt.end(); ++it)
                    lg() << " " << *it;
            }
            else {
                for (i = 0; i < 128; ++i)
                    lg() << " " << cnt[i];
                lg() << " ... " << cnt.back();
            }
            lg() << "];\nbounds(" << bdry.size() << ") = [";
            if (bdry.size() < 256) {
                for (it = bdry.begin(); it != bdry.end(); ++it)
                    lg() << " " << *it;
            }
            else {
                for (i = 0; i < 128; ++i)
                    lg() << " " << bdry[i];
                lg() << " ... " << bdry.back();
            }
            lg() << "]\nweights(" << bdry.size() << ") = [";
            if (weight.size() < 256) {
                for (it = weight.begin(); it != weight.end(); ++it)
                    lg() << " " << *it;
            }
            else {
                for (i = 0; i < 128; ++ i)
                    lg() << " " << weight[i];
                lg() << " ... " << weight.back();
            }
            lg() << "]\n";
        }
    }
} // ibis::index::divideCounts

/// Initialize the offsets from the given data array.
/// 
/// @note The incoming argument buf is from the write function that stores
/// offsets in the number 4-byte words and need to translated into offsets
/// in bytes.
int ibis::index::initOffsets(int64_t *buf, size_t noffsets) {
    if (noffsets <= 1) return -1;

    ibis::array_t<int64_t> tmp(buf, noffsets);
#if defined(DEBUG) || defined(_DEBUG)
    if (ibis::gVerbose > 5) {
        ibis::util::logger lg;
        lg() << "DEBUG -- index::initOffsets recent the following values\n";
        tmp.print(lg());
    }
#endif
    offset64.deepCopy(tmp);
    for (size_t j = 0; j < offset64.size(); ++ j)
        offset64[j] *= 4;
    offset32.clear();
    return 0;
} // ibis::index::initOffsets

/// Read in the offset array.  The offset size has been read by the caller
/// and so has the number of bitmaps to expect.
int ibis::index::initOffsets(int fdes, const char offsize, size_t start,
                             uint32_t nobs) {
    if (offsize != 4 && offsize != 8)
        return -11;
    if (start != static_cast<size_t>(UnixSeek(fdes, start, SEEK_SET)))
        return -12;
    size_t offbytes = nobs*offsize+offsize;
    try {
        if (offsize == 8) {
            offset32.clear();
            array_t<int64_t> tmp(fname, fdes, start, start+offbytes);
            offset64.swap(tmp);
        }
        else {
            offset64.clear();
            array_t<int32_t> tmp(fname, fdes, start, start+offbytes);
            offset32.swap(tmp);
        }
    }
    catch (...) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- index::initOffsets(" << fdes << ", " << offsize
            << ", " << start << ", " << nobs << ") received an exception";
        offset32.clear();
        offset64.clear();
        return -13;
    }
    return 0;
} // ibis::index::initOffsets

/// Regenerate the offsets array from the given storage object.  It
/// determines the size of offsets based on the 7th bytes in the storage
/// object, and the offset size is expected to be either 4-byte or 8-byte.
/// Unexpected offset size will cause an exception to be raised.  It is to
/// be used by the constructors of a concrete index classes.
int ibis::index::initOffsets(ibis::fileManager::storage* st, size_t start,
                             uint32_t nobs) {
    // check offset size -- can only be 4 or 8
    if (st->begin()[6] == 8) {
        array_t<int64_t> offs(st, start, start+8*nobs+8);
        offset64.swap(offs);
    }
    else if (st->begin()[6] == 4) {
        array_t<int32_t> offs(st, start, start+4*nobs+4);
        offset32.swap(offs); // save the offsets
    }
    else {
        LOGGER(ibis::gVerbose > 0 && col != 0)
            << "Warning -- index["
            << (col ? col->fullname() : "?.?") << "]::initOffsets("
            << static_cast<const void*>(st) << ", " << start << ", "
            << nobs << ") the current offset size "
            << static_cast<int>(st->begin()[6]) << " is neither 4 or 8";
        return -13;
    }
    return 0;
} // ibis::index::initOffsets

/// Prepare the bitmaps using the given file descriptor.  It clears the
/// existing content of the array bits and resize the array to have nobs
/// elements.  It reconstructs all the bitmaps if the file name (fname) is
/// not a valid pointer.  It reads the first bitmap if the compiler macro
/// FASTBIT_READ_BITVECTOR0 is defined.  It is to be used by the
/// constructors of a concrete index classes after initOffsets has been
/// called.
void ibis::index::initBitmaps(int fdes) {
    const uint32_t nobs = (offset64.size() > 1 ? offset64.size()-1 :
                           offset32.size() > 1 ? offset32.size()-1 : 0);
    for (uint32_t i = 0; i < bits.size(); ++ i)
        delete bits[i]; // free existing bitmaps
    if (nobs == 0) {
        LOGGER(ibis::gVerbose > 3 && col != 0)
            << "Warning -- index[" << (col ? col->fullname() : "??")
            << "]::initBitmaps(" << fdes
            << ") can not continue without a valid offset64 or offset32";
        return;
    }

    if (nrows == 0 && col != 0)
        nrows = col->nRows();
    str = 0;
    bits.resize(nobs);
    for (uint32_t i = 0; i < nobs; ++ i)
        bits[i] = 0;
    if (offset64.size() > nobs) {
        if (fname == 0) {    // read all bitvectors
            for (uint32_t i = 0; i < nobs; ++i) {
                if (offset64[i+1] > offset64[i]) {
                    array_t<ibis::bitvector::word_t>
                        a0(fdes, offset64[i], offset64[i+1]);
                    ibis::bitvector* tmp = new ibis::bitvector(a0);
                    tmp->sloppySize(nrows);
                    bits[i] = tmp;
                    if (nrows == 0) {
                        const_cast<index*>(this)->nrows = bits[i]->size();
                    }
#if defined(WAH_CHECK_SIZE)
                    if (nrows != bits[i]->size()) {
                        LOGGER(ibis::gVerbose > 0)
                            << "Warning -- " << evt << " encountered bitvector "
                            << i << " with a different size (" << bits[i]->size()
                            << ") from the overall nrows (" << nrows << ')';
                    }
#else
                    bits[i]->sloppySize(nrows);
#endif
                }
                else if (i == 0) {
                    bits[0] = new ibis::bitvector;
                    bits[0]->set(0, nrows);
                }
                else {
                    bits[i] = 0;
                }
            }
        }
#if defined(FASTBIT_READ_BITVECTOR0)
        else if (offset64[1] > offset64[0]) {
            array_t<ibis::bitvector::word_t> a0(fdes, offset64[0], offset64[1]);
            ibis::bitvector* tmp = new ibis::bitvector(a0);
            bits[0] = tmp;
            if (nrows == 0) {
                const_cast<index*>(this)->nrows = bits[0]->size();
            }
#if defined(WAH_CHECK_SIZE)
            LOGGER(tmp->size() != nrows && ibis::gVerbose > 0)
                << "Warning -- readIndex enters bits[" << i
                << "] with unexpected size (" << tmp->size()
                << "), expected it to be " << nrows;
#else
            tmp->sloppySize(nrows);
#endif
        }
        else {
            bits[0] = new ibis::bitvector;
            bits[0]->set(0, nrows);
        }
#endif
    }
    else if (offset32.size() > nobs) {
        if (fname == 0) {    // read all bitvectors
            for (uint32_t i = 0; i < nobs; ++i) {
                if (offset32[i+1] > offset32[i]) {
                    array_t<ibis::bitvector::word_t>
                        a0(fdes, offset32[i], offset32[i+1]);
                    ibis::bitvector* tmp = new ibis::bitvector(a0);
                    tmp->sloppySize(nrows);
                    bits[i] = tmp;
                    if (nrows == 0) {
                        const_cast<index*>(this)->nrows = bits[i]->size();
                    }
#if defined(WAH_CHECK_SIZE)
                    if (nrows != bits[i]->size()) {
                        LOGGER(ibis::gVerbose > 0)
                            << "Warning -- " << evt << " encountered bitvector "
                            << i << " with a different size (" << bits[i]->size()
                            << ") from the overall nrows (" << nrows << ')';
                    }
#else
                    bits[i]->sloppySize(nrows);
#endif
                }
                else if (i == 0) {
                    bits[0] = new ibis::bitvector;
                    bits[0]->set(0, nrows);
                }
                else {
                    bits[i] = 0;
                }
            }
        }
#if defined(FASTBIT_READ_BITVECTOR0)
        else if (offset32[1] > offset32[0]) {
            array_t<ibis::bitvector::word_t> a0(fdes, offset32[0], offset32[1]);
            ibis::bitvector* tmp = new ibis::bitvector(a0);
            bits[0] = tmp;
            if (nrows == 0) {
                const_cast<index*>(this)->nrows = bits[0]->size();
            }
#if defined(WAH_CHECK_SIZE)
            LOGGER(tmp->size() != nrows && ibis::gVerbose > 0)
                << "Warning -- readIndex enters bits[" << i
                << "] with unexpected size (" << tmp->size()
                << "), expected it to be " << nrows;
#else
            tmp->sloppySize(nrows);
#endif
        }
        else {
            bits[0] = new ibis::bitvector;
            bits[0]->set(0, nrows);
        }
#endif
    }
    else {
        LOGGER(ibis::gVerbose > 1 && col != 0)
            << "Warning -- index[" << (col ? col->fullname() : "?.?")
            << "]::initBitmaps can not proceed because both offset32["
            << offset32.size() << "] and offset64[" << offset64.size()
            << "] have less than " << nobs+1 << " elements";
    }
} // ibis::index::initBitmaps

/// Prepare bitmaps from the given storage object.  Used by constructors
/// to initialize the array bits based on the content of offset32 and
/// offset64.  The member variable nrows is expected to be set to the
/// correct value as well.
void ibis::index::initBitmaps(ibis::fileManager::storage* st) {
    // initialize bits to zero pointers
    for (uint32_t i = 0; i < bits.size(); ++ i)
        delete bits[i];

    const uint32_t nobs = (offset64.size() > 1 ? offset64.size()-1 :
                           offset32.size() > 1 ? offset32.size()-1 : 0);
    if (nobs == 0U) {
        LOGGER(ibis::gVerbose > 3 && col != 0)
            << "Warning -- index[" << col->fullname() << "]::initBitmaps("
            << static_cast<const void*>(st) << ") can not continue without "
            "a valid offset64 or offset32";
        return;
    }

    bits.resize(nobs);
    for (uint32_t i = 0; i < nobs; ++i)
        bits[i] = 0;

    if (nrows == 0 && col != 0)
        nrows = col->nRows();
    str = st;
    if (offset64.size() > 1) {
        if (st->isFileMap()) {// only map the first bitvector
#if defined(FASTBIT_READ_BITVECTOR0)
            if (offset64[1] > offset64[0]) {
                array_t<ibis::bitvector::word_t>
                    a0(st, offset64[0], offset64[1]);
                bits[0] = new ibis::bitvector(a0);
                bits[0]->sloppySize(nrows);
            }
            else {
                bits[0] = new ibis::bitvector;
                bits[0]->set(0, nrows);
            }
#endif
        }
        else {// map all the bitvectors
            for (uint32_t i = 0; i < nobs; ++i) {
                if (offset64[i+1] > offset64[i]) {
                    array_t<ibis::bitvector::word_t>
                        a(st, offset64[i], offset64[i+1]);
                    ibis::bitvector* btmp = new ibis::bitvector(a);
                    bits[i] = btmp;
#if defined(WAH_CHECK_SIZE)
                    LOGGER(btmp->size() != nrows)
                        << "Warning -- index::initBitmaps for column "
                        << (col!=0?col->fullname():"?") << " found the length ("
                        << btmp->size() << ") of bitvector " << i
                        << " differs from the expect value " << nrows;
#else
                    btmp->sloppySize(nrows);
#endif
                }
            }
        }
    }
    else if (st->isFileMap()) {// only map the first bitvector
#if defined(FASTBIT_READ_BITVECTOR0)
        if (offset32[1] > offset32[0]) {
            array_t<ibis::bitvector::word_t>
                a0(st, offset32[0], offset32[1]);
            bits[0] = new ibis::bitvector(a0);
            bits[0]->sloppySize(nrows);
        }
        else {
            bits[0] = new ibis::bitvector;
            bits[0]->set(0, nrows);
        }
#endif
    }
    else {// map all the bitvectors
        for (uint32_t i = 0; i < nobs; ++i) {
            if (offset32[i+1] > offset32[i]) {
                array_t<ibis::bitvector::word_t>
                    a(st, offset32[i], offset32[i+1]);
                ibis::bitvector* btmp = new ibis::bitvector(a);
                bits[i] = btmp;
#if defined(WAH_CHECK_SIZE)
                LOGGER(btmp->size() != nrows && ibis::gVerbose > 0)
                    << "Warning -- index[" << (col ? col->fullname() : "?.?")
                    << "]::readIndex -- the length (" << btmp->size()
                    << ") of the " << i << "-th bitvector differs from "
                    "that of the 1st one(" << nrows << ")";
#else
                btmp->sloppySize(nrows);
#endif
            }
        }
    }
} // ibis::index::initBitmaps

/// Prepare bitmaps from the given raw pointer.  Used by constructors to
/// initialize the array bits after the content of offset32 and offset64
/// have been initialized correctly.  It expects all bitmaps are serialized
/// and packed into this single array.
///
/// The member variable nrows is expected to be set to the correct value as
/// well.
void ibis::index::initBitmaps(uint32_t* st) {
    // initialize bits to zero pointers
    for (uint32_t i = 0; i < bits.size(); ++ i)
        delete bits[i];

    const uint32_t nobs = (offset64.size() > 1 ? offset64.size()-1 :
                           offset32.size() > 1 ? offset32.size()-1 : 0);
    if (nobs == 0U) {
        LOGGER(ibis::gVerbose > 3 && col != 0)
            << "Warning -- index[" << col->fullname() << "]::initBitmaps("
            << static_cast<const void*>(st) << ") can not continue without "
            "a valid offset64 or offset32";
        return;
    }

    str = 0;
    bits.resize(nobs);
    if (nrows == 0 && col != 0)
        nrows = col->nRows();
    if (offset64.size() > 1) {
        for (uint32_t i = 0; i < nobs; ++i) {
            if (offset64[i+1] > offset64[i]) {
                ibis::bitvector* btmp =
                    new ibis::bitvector(st+offset64[i]/4,
                                        (offset64[i+1]-offset64[i])/4);
                bits[i] = btmp;
                if (nrows == 0) {
                    nrows = btmp->size();
                }
                else {
#if defined(WAH_CHECK_SIZE)
                    LOGGER(btmp->size() != nrows)
                        << "Warning -- index::initBitmaps for column "
                        << (col!=0?col->fullname():"?") << " found the length ("
                        << btmp->size() << ") of bitvector " << i
                        << " differs from the expect value " << nrows;
#else
                    btmp->sloppySize(nrows);
#endif
                }
            }
        }
    }
    else {
        for (uint32_t i = 0; i < nobs; ++i) {
            if (offset32[i+1] > offset32[i]) {
                ibis::bitvector* btmp =
                    new ibis::bitvector(st+offset32[i]/4,
                                        (offset32[i+1]-offset32[i])/4);
                bits[i] = btmp;
                if (nrows == 0) {
                    nrows = btmp->size();
                }
                else {
#if defined(WAH_CHECK_SIZE)
                    LOGGER(btmp->size() != nrows)
                        << "Warning -- index::initBitmaps for column "
                        << (col!=0?col->fullname():"?") << " found the length ("
                        << btmp->size() << ") of bitvector " << i
                        << " differs from the expect value " << nrows;
#else
                    btmp->sloppySize(nrows);
#endif
                }
            }
        }
    }
} // ibis::index::initBitmaps

/// Prepare bitmaps from the user provided function pointer and context.
/// This is intended for reading serialized bitmaps placed in a more
/// complex setting, however, we still view the content as if it is written
/// as 1-D array.
void ibis::index::initBitmaps(void *ctx, FastBitReadBitmaps rd) {
    // initialize bits to zero pointers
    for (uint32_t i = 0; i < bits.size(); ++ i)
        delete bits[i];

    const uint32_t nobs = (offset64.size() > 1 ? offset64.size()-1 :
                           offset32.size() > 1 ? offset32.size()-1 : 0);
    if (nobs == 0U) {
        LOGGER(ibis::gVerbose > 3 && col != 0)
            << "Warning -- index[" << col->fullname() << "]::initBitmaps("
            << ctx << ", "<< reinterpret_cast<const void*>(rd)
            << ") can not continue without a valid offset64 or offset32";
        return;
    }

    bits.resize(nobs);
    for (uint32_t i = 0; i < nobs; ++i)
        bits[i] = 0;

    if (nrows == 0 && col != 0)
        nrows = col->nRows();
    if (breader != 0) delete breader;
    breader = new bitmapReader(ctx, rd);
} // ibis::index::initBitmaps

/// Activate all bitvectors.
void ibis::index::activate() const {
    std::string evt = "index";
    if (ibis::gVerbose > 0 && col != 0) {
        evt += '[';
        evt += col->fullname();
        evt += ']';
    }
    evt += "::activate";
    ibis::column::mutexLock lock(col, evt.c_str());
    ibis::util::timer mytimer(evt.c_str(), 4);

    const size_t nobs = bits.size();
    bool missing = false; // any bits[i] missing (is 0)?
    for (size_t i = 0; i < nobs && ! missing; ++ i)
        missing = (bits[i] == 0);
    if (missing == false) return;

    if (str == 0 && fname == 0 && breader == 0) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " cannot proceed without str or fname";
        return;
    }
    if (offset32.size() <= nobs && offset64.size() <= nobs) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " needs offsets to proceed";
        return;
    }
    if (offset64.size() > nobs) { // prefer to use offset64 if present
        if (str) { // using a ibis::fileManager::storage as back store
            LOGGER(ibis::gVerbose > 5)
                << evt << " using ibis::fileManager::storage(0x" << str << ")";

            for (size_t i = 0; i < nobs; ++i) {
                if (bits[i] == 0 && offset64[i+1] > offset64[i]) {
#if DEBUG+0 > 1 || _DEBUG+0 > 1
                    LOGGER(ibis::gVerbose > 5)
                        << "DEBUG -- " << evt << " activating bitvector "
                        << i << " from a raw storage ("
                        << static_cast<const void*>(str->begin())
                        << "), offsets[" << i << "]= " << offset64[i]
                        << ", offsets[" << i+1 << "]= " << offset64[i+1];
#endif
                    array_t<ibis::bitvector::word_t>
                        a(str, offset64[i], offset64[i+1]);
                    bits[i] = new ibis::bitvector(a);
                    if (nrows == 0) {
                        const_cast<index*>(this)->nrows = bits[i]->size();
                    }
#if defined(WAH_CHECK_SIZE)
                    if (nrows != bits[i]->size()) {
                        LOGGER(ibis::gVerbose > 0)
                            << "Warning -- " << evt << " encountered bitvector "
                            << i << " with a different size (" << bits[i]->size()
                            << ") from the overall nrows (" << nrows << ')';
                    }
#else
                    bits[i]->sloppySize(nrows);
#endif
                }
                else if (bits[i] == 0) {
                    bits[i] = new ibis::bitvector();
                    bits[i]->set(0, nrows);
                }
            }
        }
        else if (breader != 0) {
            ibis::array_t<uint32_t> buf;
            int ierr = breader->read
                (offset64[0]/4, (offset64[nobs]-offset64[0])/4, buf);
            if (ierr < 0) {
                LOGGER(ibis::gVerbose > 0)
                    << "Warning -- " << evt << " failed to read "
                    "bitvector # " << 0 << " - # " << nobs
                    << ", which occupies " << buf.size() << " words";
                throw "FastBitReadBitmaps failed to read bitvectors"
                    IBIS_FILE_LINE;
            }
            str = buf.getStorage();
            for (size_t j = 0; j < nobs; ++ j) {
                if (offset64[j+1] > offset64[j]) {
                    bits[j] = new ibis::bitvector
                        (buf, offset64[j]/4, offset64[j+1]/4);
                    if (nrows == 0) {
                        const_cast<index*>(this)->nrows = bits[j]->size();
                    }
#if defined(WAH_CHECK_SIZE)
                    if (nrows != bits[j]->size()) {
                        LOGGER(ibis::gVerbose > 0)
                            << "Warning -- " << evt
                            << " encountered bitvector "
                            << j << " with a different size ("
                            << bits[j]->size()
                            << ") from the overall nrows (" << nrows << ')';
                    }
#else
                    bits[j]->sloppySize(nrows);
#endif
                }
                else {
                    bits[j] = 0;
                }
            }
        }
        else { // using the named file directly
            int fdes = UnixOpen(fname, OPEN_READONLY);
            if (fdes < 0) {
                LOGGER(ibis::gVerbose > 0)
                    << "Warning -- " << evt << " failed to open file \""
                    << fname << "\" ... " << (errno ? strerror(errno) : "??");
                errno = 0;
                return;
            }
            LOGGER(ibis::gVerbose > 5)
                << evt << " using file \"" << fname << "\"";

#if defined(_WIN32) && defined(_MSC_VER)
            (void)_setmode(fdes, _O_BINARY);
#endif
            size_t i = 0;
            while (i < nobs) {
                // skip to next empty bit vector
                while (i < nobs && bits[i] != 0)
                    ++ i;
                // the last bitvector to activate. can not be larger
                // than j
                size_t aj = (i<nobs ? i + 1 : nobs);
                while (aj < nobs && bits[aj] == 0)
                    ++ aj;
                if (offset64[aj] > offset64[i]) {
                    const uint64_t start = offset64[i];
                    ibis::fileManager::storage *a0 = new
                        ibis::fileManager::storage(fdes, start,
                                                   offset64[aj]);
                    while (i < aj) {
#if DEBUG+0 > 1 || _DEBUG+0 > 1
                        LOGGER(ibis::gVerbose > 5)
                            << "DEBUG -- " << evt << " activating bitvector "
                            << i << " by reading file " << fname
                            << "offsets[" << i << "]= " << offset64[i]
                            << ", offsets[" << i+1 << "]= " << offset64[i+1];
#endif
                        if (bits[i] == 0 && offset64[i+1] > offset64[i]) {
                            array_t<ibis::bitvector::word_t>
                                a1(a0, offset64[i]-start, offset64[i+1]-start);
                            bits[i] = new ibis::bitvector(a1);
                            if (nrows == 0) {
                                const_cast<index*>(this)->nrows = bits[i]->size();
                            }
#if defined(WAH_CHECK_SIZE)
                            if (nrows != bits[i]->size()) {
                                LOGGER(ibis::gVerbose > 0)
                                    << "Warning -- " << evt
                                    << " encountered bitvector "
                                    << i << " with a different size ("
                                    << bits[i]->size()
                                    << ") from the overall nrows ("
                                    << nrows << ')';
                            }
#else
                            bits[i]->sloppySize(nrows);
#endif
                        }
                        else if (bits[i] == 0) {
                            bits[i] = new ibis::bitvector();
                            bits[i]->set(0, nrows);
                        }
                        ++ i;
                    }
                }
                i = aj; // always advance i
            }
            UnixClose(fdes);
        }
    }
    else if (str) { // using a ibis::fileManager::storage as back store
        LOGGER(ibis::gVerbose > 5)
            << evt << " using ibis::fileManager::storage(0x" << str << ")";

        for (size_t i = 0; i < nobs; ++i) {
            if (bits[i] == 0 && offset32[i+1] > offset32[i]) {
#if DEBUG+0 > 1 || _DEBUG+0 > 1
                LOGGER(ibis::gVerbose > 5)
                    << "DEBUG -- " << evt << " activating bitvector "
                    << i << " from a raw storage ("
                    << static_cast<const void*>(str->begin())
                    << "), offsets[" << i << "]= " << offset32[i]
                    << ", offsets[" << i+1 << "]= " << offset32[i+1];
#endif
                array_t<ibis::bitvector::word_t>
                    a(str, offset32[i], offset32[i+1]);
                bits[i] = new ibis::bitvector(a);
                if (nrows == 0) {
                    const_cast<index*>(this)->nrows = bits[i]->size();
                }
#if defined(WAH_CHECK_SIZE)
                if (nrows != bits[i]->size()) {
                    LOGGER(ibis::gVerbose > 0)
                        << "Warning -- " << evt << " encountered bitvector "
                        << i << " with a different size (" << bits[i]->size()
                        << ") from the overall nrows (" << nrows << ')';
                }
#else
                bits[i]->sloppySize(nrows);
#endif
            }
            else if (bits[i] == 0) {
                bits[i] = new ibis::bitvector();
                bits[i]->set(0, nrows);
            }
        }
    }
    else if (breader != 0) {
        ibis::array_t<uint32_t> buf;
        int ierr = breader->read
            (offset32[0]/4, (offset32[nobs]-offset32[0])/4, buf);
        if (ierr < 0) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << evt << " failed to read "
                "bitvector # " << 0 << " - # " << nobs
                << ", which occupies " << buf.size() << " words";
            throw "FastBitReadBitmaps failed to read bitvectors"
                IBIS_FILE_LINE;
        }
        str = buf.getStorage();
        for (size_t j = 0; j < nobs; ++ j) {
            if (offset32[j+1] > offset32[j]) {
                bits[j] = new ibis::bitvector
                    (buf, offset32[j]/4, offset32[j+1]/4);
                if (nrows == 0) {
                    const_cast<index*>(this)->nrows = bits[j]->size();
                }
#if defined(WAH_CHECK_SIZE)
                if (nrows != bits[j]->size()) {
                    LOGGER(ibis::gVerbose > 0)
                        << "Warning -- " << evt
                        << " encountered bitvector "
                        << j << " with a different size ("
                        << bits[j]->size()
                        << ") from the overall nrows (" << nrows << ')';
                }
#else
                bits[j]->sloppySize(nrows);
#endif
            }
            else {
                bits[j] = 0;
            }
        }
    }
    else { // using the named file directly
        int fdes = UnixOpen(fname, OPEN_READONLY);
        if (fdes < 0) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << evt << " failed to open file \"" << fname
                << "\" ... " << (errno ? strerror(errno) : "??");
            errno = 0;
            return;
        }
        LOGGER(ibis::gVerbose > 5)
            << evt << " using file \"" << fname << "\"";

#if defined(_WIN32) && defined(_MSC_VER)
        (void)_setmode(fdes, _O_BINARY);
#endif
        size_t i = 0;
        while (i < nobs) {
            // skip to next empty bit vector
            while (i < nobs && bits[i] != 0)
                ++ i;
            // the last bitvector to activate. can not be larger
            // than j
            size_t aj = (i<nobs ? i + 1 : nobs);
            while (aj < nobs && bits[aj] == 0)
                ++ aj;
            if (offset32[aj] > offset32[i]) {
                const uint32_t start = offset32[i];
                ibis::fileManager::storage *a0 = new
                    ibis::fileManager::storage(fdes, start, offset32[aj]);
                while (i < aj) {
#if DEBUG+0 > 1 || _DEBUG+0 > 1
                    LOGGER(ibis::gVerbose > 5)
                        << "DEBUG -- " << evt << " activating bitvector " << i
                        << " by reading from file " << fname
                        << " between offsets[" << i << "]= " << offset32[i]
                        << ", offsets[" << i+1 << "]= " << offset32[i+1];
#endif
                    if (bits[i] == 0 && offset32[i+1] > offset32[i]) {
                        array_t<ibis::bitvector::word_t>
                            a1(a0, offset32[i]-start, offset32[i+1]-start);
                        bits[i] = new ibis::bitvector(a1);
                        if (nrows == 0) {
                            const_cast<index*>(this)->nrows = bits[i]->size();
                        }
#if defined(WAH_CHECK_SIZE)
                        if (nrows != bits[i]->size()) {
                            LOGGER(ibis::gVerbose > 0)
                                << "Warning -- " << evt
                                << " encountered bitvector "
                                << i << " with a different size ("
                                << bits[i]->size()
                                << ") from the overall nrows (" << nrows << ')';
                        }
#else
                        bits[i]->sloppySize(nrows);
#endif
                    }
                    else if (bits[i] == 0) {
                        bits[i] = new ibis::bitvector();
                        bits[i]->set(0, nrows);
                    }
                    ++ i;
                }
            }
            i = aj; // always advance i
        }
        UnixClose(fdes);
    }
} // ibis::index::activate

// activate the ith bitvector
void ibis::index::activate(uint32_t i) const {
    if (i >= bits.size()) return;   // index out of range
    std::string evt = "index";
    if (col != 0 && ibis::gVerbose > 0) {
        evt += '[';
        evt += col->fullname();
        evt += ']';
    }
    evt += "::activate";
    ibis::column::mutexLock lock(col, evt.c_str());
    ibis::util::timer mytimer(evt.c_str(), 4);

    if (bits[i] != 0) return;   // already active
    if (offset32.size() <= bits.size() && offset64.size() <= bits.size()) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " needs offset to regenerate bitvector "
            << i;
        return;
    }
    else if (str == 0 && breader == 0 && fname == 0) {
        LOGGER(ibis::gVerbose > 1)
            << "Warning -- " << evt << " can not regenerate bitvector " << i
            << " without either str or fname";
        return;
    }

    if (offset64.size() > bits.size()) {
        if (offset64[i+1] <= offset64[i]) {
            bits[i] = new ibis::bitvector();
            bits[i]->set(0, nrows);
        }
        else if (str != 0) { // using a ibis::fileManager::storage as back store
            LOGGER(ibis::gVerbose > 5)
                << evt << "(" << i << ") using storage @ " << str;
#if DEBUG+0 > 1 || _DEBUG+0 > 1
            LOGGER(ibis::gVerbose > 5)
                << "DEBUG -- " << evt << " constructing bitvector " << i
                << " from range [" << offset64[i] << ", " << offset64[i+1]
                << ") of a storage at "
                << static_cast<const void*>(str->begin());
#endif

            array_t<ibis::bitvector::word_t>
                a(str, offset64[i], offset64[i+1]);
            bits[i] = new ibis::bitvector(a);
            if (nrows == 0) {
                const_cast<index*>(this)->nrows = bits[i]->size();
            }
#if defined(WAH_CHECK_SIZE)
            if (nrows != bits[i]->size()) {
                LOGGER(ibis::gVerbose > 0)
                    << "Warning -- " << evt << " encountered bitvector "
                    << i << " with a different size (" << bits[i]->size()
                    << ") from the overall nrows (" << nrows << ')';
            }
#else
            bits[i]->sloppySize(nrows);
#endif
        }
        else if (breader != 0) {
            ibis::array_t<uint32_t> buf;
            if (breader->read(offset64[i]/4, (offset64[i+1]-offset64[i])/4, buf)
                >= 0) {
                bits[i] = new ibis::bitvector
                    (buf, offset64[i]/4, offset64[i+1]/4);
                if (nrows == 0) {
                    const_cast<index*>(this)->nrows = bits[i]->size();
                }
#if defined(WAH_CHECK_SIZE)
                if (nrows != bits[i]->size()) {
                    LOGGER(ibis::gVerbose > 0)
                        << "Warning -- " << evt << " encountered bitvector "
                        << i << " with a different size (" << bits[i]->size()
                        << ") from the overall nrows (" << nrows << ')';
                }
#else
                bits[i]->sloppySize(nrows);
#endif
            }
            else {
                LOGGER(ibis::gVerbose > 0)
                    << "Warning -- " << evt << " failed to read bitvector "
                    << i << " through the callback function";
            }
        }
        else if (fname != 0) { // using the named file directly
            int fdes = UnixOpen(fname, OPEN_READONLY);
            if (fdes >= 0) {
                LOGGER(ibis::gVerbose > 5)
                    << evt << "(" << i << ") using file \"" << fname << "\"";

#if defined(_WIN32) && defined(_MSC_VER)
                (void)_setmode(fdes, _O_BINARY);
#endif
#if DEBUG+0 > 1 || _DEBUG+0 > 1
                LOGGER(ibis::gVerbose > 5)
                    << "DEBUG -- " << evt << "(" << i
                    << ") constructing the bitvector from range ["
                    << offset64[i] << ", " << offset64[i+1] << ") of file "
                    << fname;
#endif
                array_t<ibis::bitvector::word_t>
                    a0(fdes, offset64[i], offset64[i+1]);
                bits[i] = new ibis::bitvector(a0);
                UnixClose(fdes);
                if (nrows == 0) {
                    const_cast<index*>(this)->nrows = bits[i]->size();
                }
#if defined(WAH_CHECK_SIZE)
                if (nrows != bits[i]->size()) {
                    LOGGER(ibis::gVerbose > 0)
                        << "Warning -- " << evt << " encountered bitvector "
                        << i << " with a different size (" << bits[i]->size()
                        << ") from the overall nrows (" << nrows << ')';
                }
#else
                bits[i]->sloppySize(nrows);
#endif
            }
            else {
                LOGGER(ibis::gVerbose > 0)
                    << "Warning -- " << evt << " failed to open file \""
                    << fname << "\" ... " << (errno ? strerror(errno) : "??");
            }
        }
    }
    else if (offset32[i+1] <= offset32[i]) {
        bits[i] = new ibis::bitvector();
        bits[i]->set(0, nrows);
    }
    else if (str) { // using a ibis::fileManager::storage as back store
        LOGGER(ibis::gVerbose > 5)
            << evt << "(" << i << ") using storage @ " << str;
#if DEBUG+0 > 1 || _DEBUG+0 > 1
        LOGGER(ibis::gVerbose > 5)
                << "DEBUG -- " << evt << " constructing bitvector " << i
                << " from range [" << offset32[i] << ", " << offset32[i+1]
                << ") of a storage at "
                << static_cast<const void*>(str->begin());
#endif

        array_t<ibis::bitvector::word_t>
            a(str, offset32[i], offset32[i+1]);
        bits[i] = new ibis::bitvector(a);
        if (nrows == 0) {
            const_cast<index*>(this)->nrows = bits[i]->size();
        }
#if defined(WAH_CHECK_SIZE)
        if (nrows != bits[i]->size()) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << evt << " encountered bitvector "
                << i << " with a different size (" << bits[i]->size()
                << ") from the overall nrows (" << nrows << ')';
        }
#else
        bits[i]->sloppySize(nrows);
#endif
    }
    else if (breader != 0) {
        ibis::array_t<uint32_t> buf;
        if (breader->read(offset32[i]/4, (offset32[i+1]-offset32[i])/4, buf)
            >= 0) {
            bits[i] = new ibis::bitvector
                (buf, offset32[i]/4, offset32[i+1]/4);
            if (nrows == 0) {
                const_cast<index*>(this)->nrows = bits[i]->size();
            }
#if defined(WAH_CHECK_SIZE)
            if (nrows != bits[i]->size()) {
                LOGGER(ibis::gVerbose > 0)
                    << "Warning -- " << evt << " encountered bitvector "
                    << i << " with a different size (" << bits[i]->size()
                    << ") from the overall nrows (" << nrows << ')';
            }
#else
            bits[i]->sloppySize(nrows);
#endif
        }
        else {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << evt << " failed to read bitvector "
                << i << " through the callback function";
        }
    }
    else if (fname) { // using the named file directly
        int fdes = UnixOpen(fname, OPEN_READONLY);
        if (fdes >= 0) {
            LOGGER(ibis::gVerbose > 5)
                << evt << "(" << i << ") using file \"" << fname << "\"";
#if defined(_WIN32) && defined(_MSC_VER)
            (void)_setmode(fdes, _O_BINARY);
#endif
#if DEBUG+0 > 1 || _DEBUG+0 > 1
            LOGGER(ibis::gVerbose > 5)
                    << "DEBUG -- " << evt << " construct bitvector "
                    << i << " from range [" << offset32[i] << ", "
                    << offset32[i+1] << ") of file " << fname;
#endif
            array_t<ibis::bitvector::word_t>
                a0(fdes, offset32[i], offset32[i+1]);
            bits[i] = new ibis::bitvector(a0);
            UnixClose(fdes);
            if (nrows == 0) {
                const_cast<index*>(this)->nrows = bits[i]->size();
            }
#if defined(WAH_CHECK_SIZE)
            if (nrows != bits[i]->size()) {
                LOGGER(ibis::gVerbose > 0)
                    << "Warning -- " << evt << " encountered bitvector "
                    << i << " with a different size (" << bits[i]->size()
                    << ") from the overall nrows (" << nrows << ')';
            }
#else
            bits[i]->sloppySize(nrows);
#endif
        }
        else {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << evt << " failed to open file \""
                << fname << "\" ... " << (errno ? strerror(errno) : "??");
            errno = 0;
        }
    }
    else {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " needs str, breader or fname to "
            "regenerate bitvector " << i;
    }
} // ibis::index::activate

// activate bitvectors [i, j).
void ibis::index::activate(uint32_t i, uint32_t j) const {
    if (j > bits.size())
        j = bits.size();
    if (i >= j || i >= bits.size()) // empty range
        return;
    std::string evt = "index";
    if (col != 0 && ibis::gVerbose > 0) {
        evt += '[';
        evt += col->fullname();
        evt += ']';
    }
    evt += "::activate";
    ibis::column::mutexLock lock(col, evt.c_str());
    ibis::util::timer mytimer(evt.c_str(), 4);

    while (i < j && bits[i] != 0) ++ i;
    while (i < j && bits[j-1] != 0) j = j - 1;
    if (i >= j) // all bitvectors active
        return;
    if (str == 0 && breader == 0 && fname == 0) {
        LOGGER(ibis::gVerbose > 1)
            << "Warning -- " << evt << "(" << i << ", " << j
            << ") can not proceed without either str or fname";
        return;
    }
    if (offset32.size() <= bits.size() && offset64.size() <= bits.size()) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " no records of offsets, can not "
            "regenerate bitvectors " << i << ":" << j;
        return;
    }

    if (offset64.size() > bits.size()) {
        if (str) { // using an ibis::fileManager::storage as back store
            LOGGER(ibis::gVerbose > 5)
                << evt << "(" << i << ", " << j
                << ") using ibis::fileManager::storage(0x" << str << ")";

            while (i < j) {
#if DEBUG+0 > 1 || _DEBUG+0 > 1
                LOGGER(ibis::gVerbose > 5)
                    << "DEBUG -- " << evt << " to construct bitvector "
                    << i << " from range [" << offset64[i] << ", "
                    << offset64[i+1] << ") of a storage at "
                    << static_cast<const void*>(str->begin());
#endif
                if (bits[i] == 0 && offset64[i+1] > offset64[i]) {
                    array_t<ibis::bitvector::word_t>
                        a(str, offset64[i], offset64[i+1]);
                    bits[i] = new ibis::bitvector(a);
                    if (nrows == 0) {
                        const_cast<index*>(this)->nrows = bits[i]->size();
                    }
#if defined(WAH_CHECK_SIZE)
                    if (nrows != bits[i]->size()) {
                        LOGGER(ibis::gVerbose > 0)
                            << "Warning -- " << evt << " encountered bitvector "
                            << i << " with a different size (" << bits[i]->size()
                            << ") from the overall nrows (" << nrows << ')';
                    }
#else
                    bits[i]->sloppySize(nrows);
#endif
                }
                else if (bits[i] == 0) {
                    bits[i] = new ibis::bitvector();
                    bits[i]->set(0, nrows);
                }
                ++ i;
            }
        }
        else if (breader != 0) {
            ibis::array_t<uint32_t> buf;
            int ierr = breader->read
                (offset64[i]/4, (offset64[j]-offset64[i])/4, buf);
            if (ierr < 0) {
                LOGGER(ibis::gVerbose > 0)
                    << "Warning -- " << evt << " failed to read bitvectors "
                    << i << " - " << j << ", which occupies "
                    << (offset64[j]-offset64[i])/4 << " words";
                throw "FastBitReadBitmaps failed to read bitvectors"
                    IBIS_FILE_LINE;
            }
            for (size_t j0 = i; j0 < j; ++ j0) {
                size_t j1 = j0+1;
                if (offset64[j1] > offset64[j0]) {
                    bits[j0] = new ibis::bitvector
                        (buf, (offset64[j0]-offset64[i])/4,
                         (offset64[j1]-offset64[i])/4);
                    if (nrows == 0) {
                        const_cast<index*>(this)->nrows = bits[j0]->size();
                    }
#if defined(WAH_CHECK_SIZE)
                    if (nrows != bits[j0]->size()) {
                        LOGGER(ibis::gVerbose > 0)
                            << "Warning -- " << evt
                            << " encountered bitvector "
                            << j0 << " with a different size ("
                            << bits[j0]->size()
                            << ") from the overall nrows (" << nrows << ')';
                    }
#else
                    bits[j0]->sloppySize(nrows);
#endif
                }
                else {
                    bits[j0] = 0;
                }
            }
        }
        else if (fname) { // using the named file directly
            if (offset64[j] > offset64[i]) {
                int fdes = UnixOpen(fname, OPEN_READONLY);
                if (fdes < 0) {
                    LOGGER(ibis::gVerbose > 0)
                        << "Warning -- " << evt << "failed to open file \""
                        << fname << "\" ... " << (errno?strerror(errno):"??");
                    errno = 0;
                    return;
                }

                LOGGER(ibis::gVerbose > 5)
                    << evt << "(" << i << ", " << j
                    << ") using file \"" << fname << "\"";                  
#if defined(_WIN32) && defined(_MSC_VER)
                (void)_setmode(fdes, _O_BINARY);
#endif
                while (i < j) {
                    // skip to next empty bit vector
                    while (i < j && bits[i] != 0)
                        ++ i;
                    // the last bitvector to activate. can not be larger
                    // than j
                    uint32_t aj = (i<j ? i + 1 : j);
                    while (aj < j && bits[aj] == 0)
                        ++ aj;
                    if (offset64[aj] > offset64[i]) {
                        const uint64_t start = offset64[i];
                        ibis::fileManager::storage *a0 = new
                            ibis::fileManager::storage(fdes, start,
                                                       offset64[aj]);
                        while (i < aj) {
#if DEBUG+0 > 1 || _DEBUG+0 > 1
                            LOGGER(ibis::gVerbose > 5)
                                << "DEBUG -- " << evt
                                << " constructing bitvector " << i
                                << " from range [" << offset64[i] << ", "
                                << offset64[i+1] << ") of file " << fname;
#endif
                            if (bits[i] == 0 &&
                                offset64[i+1] > offset64[i]) {
                                array_t<ibis::bitvector::word_t>
                                    a1(a0, offset64[i]-start,
                                       offset64[i+1]-start);
                                bits[i] = new ibis::bitvector(a1);
                                if (nrows == 0) {
                                    const_cast<index*>(this)->nrows =
                                        bits[i]->size();
                                }
#if defined(WAH_CHECK_SIZE)
                                if (nrows != bits[i]->size()) {
                                    LOGGER(ibis::gVerbose > 0)
                                        << "Warning -- " << evt
                                        << " encountered bitvector "
                                        << i << " with a different size ("
                                        << bits[i]->size()
                                        << ") from the overall nrows ("
                                        << nrows << ')';
                                }
#else
                                bits[i]->sloppySize(nrows);
#endif
                            }
                            else if (bits[i] == 0) {
                                bits[i] = new ibis::bitvector();
                                bits[i]->set(0, nrows);
                            }
                            ++ i;
                        }
                    }
                    i = aj; // always advance i
                }
                UnixClose(fdes);
            }
        }
    }
    else if (str) { // using an ibis::fileManager::storage as back store
        LOGGER(ibis::gVerbose > 5)
            << evt << "(" << i << ", " << j
            << ") using ibis::fileManager::storage(0x" << str << ")";

        while (i < j) {
#if DEBUG+0 > 1 || _DEBUG+0 > 1
            LOGGER(ibis::gVerbose > 5)
                << "DEBUG -- " << evt << " constructing bitvector " << i
                << " from range [" << offset32[i] << ", " << offset32[i+1]
                << ") of a storage at "
                << static_cast<const void*>(str->begin());
#endif
            if (bits[i] == 0 && offset32[i+1] > offset32[i]) {
                array_t<ibis::bitvector::word_t>
                    a(str, offset32[i], offset32[i+1]);
                bits[i] = new ibis::bitvector(a);
                if (nrows == 0) {
                    const_cast<index*>(this)->nrows = bits[i]->size();
                }
#if defined(WAH_CHECK_SIZE)
                if (nrows != bits[i]->size()) {
                    LOGGER(ibis::gVerbose > 0)
                        << "Warning -- " << evt
                        << " encountered bitvector "
                        << i << " with a different size ("
                        << bits[i]->size()
                        << ") from the overall nrows ("
                        << nrows << ')';
                }
#else
                bits[i]->sloppySize(nrows);
#endif
            }
            else if (bits[i] == 0) {
                bits[i] = new ibis::bitvector();
                bits[i]->set(0, nrows);
            }
            ++ i;
        }
    }
    else if (breader != 0) {
        ibis::array_t<uint32_t> buf;
        int ierr = breader->read
            (offset32[i]/4, (offset32[j]-offset32[i])/4, buf);
        if (ierr < 0) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << evt << " failed to read bitvectors "
                << i << " - " << j << ", which occupies "
                << (offset32[j]-offset32[i])/4 << " words";
            throw "FastBitReadBitmaps failed to read bitvectors"
                IBIS_FILE_LINE;
        }
        for (size_t j0 = i; j0 < j; ++ j0) {
            size_t j1 = j0+1;
            if (offset32[j1] > offset32[j0]) {
                bits[j0] = new ibis::bitvector
                    (buf, (offset32[j0]-offset32[i])/4,
                     (offset32[j1]-offset32[i])/4);
                if (nrows == 0) {
                    const_cast<index*>(this)->nrows = bits[j0]->size();
                }
#if defined(WAH_CHECK_SIZE)
                if (nrows != bits[j0]->size()) {
                    LOGGER(ibis::gVerbose > 0)
                        << "Warning -- " << evt
                        << " encountered bitvector "
                        << j0 << " with a different size ("
                        << bits[j0]->size()
                        << ") from the overall nrows (" << nrows << ')';
                }
#else
                bits[j0]->sloppySize(nrows);
#endif
            }
            else {
                bits[j0] = 0;
            }
        }
    }
    else if (fname) { // using the named file directly
        if (offset32[j] > offset32[i]) {
            int fdes = UnixOpen(fname, OPEN_READONLY);
            if (fdes < 0) {
                LOGGER(ibis::gVerbose > 0)
                    << "Warning -- " << evt << " failed to open file \""
                    << fname << "\" ... " << (errno ? strerror(errno) : "??");
                return;
            }
            LOGGER(ibis::gVerbose > 5)
                << evt << "(" << i << ", " << j
                << ") using file \"" << fname << "\"";

#if defined(_WIN32) && defined(_MSC_VER)
            (void)_setmode(fdes, _O_BINARY);
#endif
            while (i < j) {
                // skip to next empty bit vector
                while (i < j && bits[i] != 0)
                    ++ i;
                // the last bitvector to activate. can not be larger
                // than j
                uint32_t aj = (i<j ? i + 1 : j);
                while (aj < j && bits[aj] == 0)
                    ++ aj;
                if (offset32[aj] > offset32[i]) {
                    const uint32_t start = offset32[i];
                    ibis::fileManager::storage *a0 = new
                        ibis::fileManager::storage(fdes, start,
                                                   offset32[aj]);
                    while (i < aj) {
#if DEBUG+0 > 1 || _DEBUG+0 > 1
                        LOGGER(ibis::gVerbose > 5)
                            << "DEBUG -- " << evt << " constructing bitvector "
                            << i << " from range [" << offset32[i] << ", "
                            << offset32[i+1] << ") of file " << fname;
#endif
                        if (bits[i] == 0 && offset32[i+1] > offset32[i]) {
                            array_t<ibis::bitvector::word_t>
                                a1(a0, offset32[i]-start, offset32[i+1]-start);
                            bits[i] = new ibis::bitvector(a1);
                            if (nrows == 0) {
                                const_cast<index*>(this)->nrows = bits[i]->size();
                            }
#if defined(WAH_CHECK_SIZE)
                            if (nrows != bits[i]->size()) {
                                LOGGER(ibis::gVerbose > 0)
                                    << "Warning -- " << evt
                                    << " encountered bitvector "
                                    << i << " with a different size ("
                                    << bits[i]->size()
                                    << ") from the overall nrows ("
                                    << nrows << ')';
                            }
#else
                            bits[i]->sloppySize(nrows);
#endif
                        }
                        else if (bits[i] == 0) {
                            bits[i] = new ibis::bitvector();
                            bits[i]->set(0, nrows);
                        }
                        ++ i;
                    }
                }
                i = aj; // always advance i
            }
            UnixClose(fdes);
        }
    }
    else {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " can not regenerate bitvectors "
            << i << ":" << j <<  " without str or fname";
    }
} // ibis::index::activate

/// Add the sum of @c bits[ib] through @c bits[ie-1] to @c res.  Always
/// explicitly use @c bits[ib] through @c bits[ie-1].
/// The most important difference between this function and @c sumBins is
/// that this function always use @c bits[ib] through @c bits[ie-1].  This
/// is similar to the function @c addBits.
void ibis::index::addBins(uint32_t ib, uint32_t ie,
                          ibis::bitvector& res) const {
    LOGGER(ibis::gVerbose > 6)
        << "index[" << (col ? col->fullname() : "?.?") << "]::addBins(" << ib
        << ", " << ie << ", res(" << res.cnt() << ", " << res.size()
        << ")) ...";
    const uint32_t nobs = bits.size();
    if (res.cnt() >= nrows) return; // useless to add more bits
    if (res.size() != nrows)
        res.adjustSize(0, nrows);
    if (ie > nobs)
        ie = nobs;
    if (ib >= ie)
        return;
    if (ib == 0 && ie == nobs) {
        res.set(1, nrows);
        return;
    }

    activate(ib, ie);
    for (; ib < ie && ib < nobs && bits[ib] == 0; ++ ib);
    const uint32_t na = ie - ib;
    if (na <= 2) { // some special cases
        if (na == 1) {
            if (bits[ib])
                res |= *(bits[ib]);
        }
        else if (na > 1) { // two consecutive bitmaps in the range
            if (bits[ib])
                res |= (*(bits[ib]));
            if (bits[ib+1])
                res |= *(bits[ib+1]);
        }
        else {
            res.set(0, nrows);
        }
        return;
    }

    horometer timer;
    uint32_t bytes = 0;
    // compute the total size of all bitmaps
    for (uint32_t i = ib; i < ie; ++i) {
        if (bits[i])
            bytes += bits[i]->bytes();
    }
    // based on extensive testing, we have settled on the following
    // combination
    // (1) if the total size of the first two are greater than the
    // uncompressed size of one bitmap, use option 1.  Because the
    // operation of the first two will produce an uncompressed result,
    // it will sum together all other bits with to the uncompressed
    // result generated already.
    // (2) if total size (bytes) times log 2 of number of bitmaps
    // is less than or equal to the size of an uncompressed
    // bitmap, use option 3, else use option 4.
    if (ibis::gVerbose > 4) {
        LOGGER(ibis::gVerbose > 4)
            << "index::addBins(" << ib << ", " << ie << ") will operate on "
            << na << " out of " << nobs << " bitmaps using the combined option";
        timer.start();
    }
    const uint32_t uncomp = (ibis::bitvector::bitsPerLiteral() == 8 ?
                             nrows * 2 / 15 :
                             nrows * 4 / 31);
    const uint32_t sum2 = (bits[ib] ? bits[ib]->bytes() : 0U) +
        (bits[ib+1] ? bits[ib+1]->bytes() : 0U);
    if (sum2 >= uncomp) {
        LOGGER(ibis::gVerbose > 5)
            << "index::addBins(" << ib << ", " << ie
            << ") takes a simple loop to OR the bitmaps";
        for (uint32_t i = ib; i < ie; ++i) {
            if (bits[i])
                res |= *(bits[i]);
        }
    }
    else if (bytes*static_cast<double>(na)*na <= log(2.0)*uncomp) {
        LOGGER(ibis::gVerbose > 5)
            << "index::addBins(" << ib << ", " << ie
            << ") uses a priority queue to OR the bitmaps";
        typedef std::pair<ibis::bitvector*, bool> _elem;
        // put all bitmaps in a priority queue
        std::priority_queue<_elem> que;
        _elem op1, op2, tmp;
        tmp.first = 0;

        // populate the priority queue with the original input
        for (uint32_t i = ib; i < ie; ++i) {
            if (bits[i]) {
                op1.first = bits[i];
                op1.second = false;
                que.push(op1);
            }
        }

        try {
            while (! que.empty()) {
                op1 = que.top();
                que.pop();
                if (que.empty()) {
                    res.copy(*(op1.first));
                    if (op1.second) delete op1.first;
                    break;
                }

                op2 = que.top();
                que.pop();
                tmp.second = true;
                tmp.first = *(op1.first) | *(op2.first);
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                LOGGER(ibis::gVerbose >= 0)
                    << "DEBUG -- addBins-using priority queue: "
                    << op1.first->bytes()
                    << (op1.second ? "(transient), " : ", ")
                    << op2.first->bytes()
                    << (op2.second ? "(transient) >> " : " >> ")
                    << tmp.first->bytes();
#endif
                if (op1.second)
                    delete op1.first;
                if (op2.second)
                    delete op2.first;
                if (! que.empty()) {
                    que.push(tmp);
                    tmp.first = 0;
                }
            }
            if (tmp.first != 0) {
                if (tmp.second) {
                    res |= *(tmp.first);
                    delete tmp.first;
                    tmp.first = 0;
                }
                else {
                    res |= *(tmp.first);
                }
            }
        }
        catch (...) { // need to free the pointers
            delete tmp.first;
            while (! que.empty()) {
                tmp = que.top();
                if (tmp.second)
                    delete tmp.first;
                que.pop();
            }
            throw;
        }
    }
    else if (sum2 <= (uncomp >> 2)) {
        LOGGER(ibis::gVerbose > 5)
            << "index::addBins(" << ib << ", " << ie
            << ") decompresses the result bitmap before ORing the bitmaps";
        // use uncompressed res
        while (ib < ie && bits[ib] == 0)
            ++ ib;
        if (ib < ie) {
            if (bits[ib])
                res |= *(bits[ib]);
            ++ ib;
        }
        res.decompress();
        for (uint32_t i = ib; i < ie; ++ i) {
            if (bits[i])
                res |= *(bits[i]);
        }
    }
    else {
        LOGGER(ibis::gVerbose > 5)
            << "index::addBins(" << ib << ", " << ie
            << ") takes a simple loop to OR the bitmaps";
        for (uint32_t i = ib; i < ie; ++ i)
            if (bits[i])
                res |= *(bits[i]);
    }

    if (ibis::gVerbose > 4) {
        timer.stop();
        LOGGER(ibis::gVerbose > 4)
            << "index::addBins operated on " << na << " bitmap"
            << (na>1?"s":"") << " (" << bytes << " in " << res.bytes()
            << " out) took " << timer.CPUTime() << " sec(CPU), "
            << timer.realTime() << "%g sec(elapsed)";
    }
#if DEBUG+0 > 0 || _DEBUG+0 > 0
    if (ibis::gVerbose > 30 || (1U << ibis::gVerbose) >= res.bytes()) {
        LOGGER(ibis::gVerbose >= 0)
            << "DEBUG -- addBins(" << ib << ", " << ie << "):" << res;
    }
#endif
} // ibis::index::addBins

/// Compute the sum of bit vectors [@c ib, @c ie).  If computing a
/// complement is faster, assume all bit vectors add up to @c tot.
/// This is basically a copy of the function @c sumBins (without the 4th
/// argument).  There are two changes: (1) if @c res has the same number
/// of bits as @c tot, the new sum is added to the existing bitvector, and
/// (2) when it computes the sum through complements, it performs a
/// subtraction from @c tot.
void ibis::index::addBins(uint32_t ib, uint32_t ie, ibis::bitvector& res,
                          const ibis::bitvector& tot) const {
    LOGGER(ibis::gVerbose > 6)
        << "index[" << (col ? col->fullname() : "?.?") << "]::addBins(" << ib
        << ", " << ie << ", res(" << res.cnt() << ", " << res.size()
        << "), tot(" << tot.cnt() << ", " << tot.size() << ")) ...";
    if (res.size() != tot.size())
        res.adjustSize(0U, tot.size());
    if (ib >= ie) { // no bitmap in the range
        return;
    }

    typedef std::pair<ibis::bitvector*, bool> _elem;
    const size_t nobs = bits.size();
    if (ie > nobs) ie = nobs;
    bool straight = true;
    if (offset32.size() <= nobs && offset64.size() <= nobs) {
        // all bitvectors must be in memory
        offset32.clear();
        try {
            offset64.resize(nobs+1);
            offset64[0] = 0;
            for (uint32_t i = 0; i < nobs; ++ i)
                offset64[i+1] = offset64[i] +
                    (bits[i] ? bits[i]->bytes() : 0U);
        }
        catch (...) {
            offset64.clear();
        }
    }
    if (offset64.size() > nobs) {
        const uint64_t all = offset64[nobs] - offset64[0];
        const uint64_t mid = offset64[ie] - offset64[ib];
        straight = (mid <= (all >> 1));
    }
    else if (offset32.size() > nobs) {
        const uint32_t all = offset32[nobs] - offset32[0];
        const uint32_t mid = offset32[ie] - offset32[ib];
        straight = (mid <= (all >> 1));
    }
    else {
        straight = (ie-ib <= (nobs >> 1));
    }

    if (breader || str || fname) { // try to activate the needed bitmaps
        if (straight) {
            activate(ib, ie);
        }
        else {
            activate(0, ib);
            activate(ie, nobs);
        }
    }

    const uint32_t na = (straight ? ie-ib : nobs + ib - ie);
    if (na <= 2) { // some special cases
        if (ib >= ie) { // no bitmap in the range
            res.set(0, nrows);
        }
        else if (ib == 0 && ie == nobs) { // every bitmap in the range
            res |= tot;
        }
        else if (na == 1) {
            if (straight) { // only one bitmap in the range
                if (bits[ib])
                    res |= (*(bits[ib]));
            }
            else if (ib == 0) { // last one is outside
                if (bits[ie]) {
                    ibis::bitvector tmp(tot);
                    tmp -= *(bits[ie]);
                    res |= tmp;
                }
                else {
                    res |= tot;
                }
            }
            else { // the first one is outside
                ibis::bitvector tmp(tot);
                if (bits[0])
                    tmp -= *(bits[0]);
                res |= tmp;
            }
        }
        else if (straight) { // two consecutive bitmaps in the range
            if (bits[ib])
                res |= *(bits[ib]);
            if (bits[ib+1])
                res |= *(bits[ib+1]);
        }
        else if (ib == 0) { // two bitmaps at the end are outside
            ibis::bitvector tmp(tot);
            if (bits[ie])
                tmp -= (*(bits[ie]));
            if (bits[nobs-1])
                tmp -= *(bits[nobs-1]);
            res |= tmp;
        }
        else if (ib == 1) { // two outside bitmaps are split
            ibis::bitvector tmp(tot);
            if (bits[0])
                tmp -= (*(bits[0]));
            if (bits[ie])
                tmp -= *(bits[ie]);
            res |= tmp;
        }
        else if (ib == 2) { // two outside bitmaps are at the beginning
            ibis::bitvector tmp(tot);
            if (bits[0])
                tmp -= (*(bits[0]));
            if (bits[1])
                tmp -= *(bits[1]);
            res |= tmp;
        }
        return;
    }

    horometer timer;
    uint32_t bytes = 0;
    // based on extensive testing, we have settled on the following
    // combination
    // (1) if the two size of the first two are greater than the
    // uncompressed size of one bitmap, use option 1.  Because the
    // operation of the first two will produce an uncompressed result,
    // it will sum together all other bits with to the uncompressed
    // result generated already.
    // (2) if total size (bytes) times log 2 of number of bitmaps
    // is less than or equal to the size of an uncompressed
    // bitmap, use option 3, else use option 4.
    if (ibis::gVerbose > 4) {
        LOGGER(ibis::gVerbose > 4)
            << "index::addBins(" << ib << ", " << ie << ") will operate on "
            << na << " out of " << nobs << " bitmaps using the combined option";
        timer.start();
        if (straight) {
            for (uint32_t i = ib; i < ie; ++i) {
                if (bits[i])
                    bytes += bits[i]->bytes();
            }
        }
        else {
            for (uint32_t i = 0; i < ib; ++i) {
                if (bits[i])
                    bytes += bits[i]->bytes();
            }
            for (uint32_t i = ie; i < nobs; ++i) {
                if (bits[i])
                    bytes += bits[i]->bytes();
            }
        }
    }
    const uint32_t uncomp = (ibis::bitvector::bitsPerLiteral() == 8 ?
                             nrows * 2 / 15 : nrows * 4 / 31);
    if (straight) {
        uint32_t sum2 = (bits[ib] ? bits[ib]->bytes() : 0U) +
            (bits[ib+1] ? bits[ib+1]->bytes() : 0U);
        if (sum2 >= uncomp) { // let the automatic decompression work
            LOGGER(ibis::gVerbose > 5)
                << "index::addBins(" << ib << ", " << ie
                << ") takes a simple loop to OR the bitmaps";
            for (uint32_t i = ib; i < ie; ++i) {
                if (bits[i])
                    res |= *(bits[i]);
            }
        }
        else {
            // need to compute the total size of all bitmaps
            if (bytes == 0) {
                for (uint32_t i = ib; i < ie; ++i) {
                    if (bits[i])
                        bytes += bits[i]->bytes();
                }
            }
            if (bytes*static_cast<double>(na)*na <= log(2.0)*uncomp) {
                LOGGER(ibis::gVerbose > 5)
                    << "index::addBins(" << ib << ", " << ie
                    << ") uses a priority queue to OR the bitmaps";
                // put all bitmaps in a priority queue
                std::priority_queue<_elem> que;
                _elem op1, op2, tmp;
                tmp.first = 0;

                // populate the priority queue with the original input
                for (uint32_t i = ib; i < ie; ++i) {
                    if (bits[i]) {
                        op1.first = bits[i];
                        op1.second = false;
                        que.push(op1);
                    }
                }

                try {
                    while (! que.empty()) {
                        op1 = que.top();
                        que.pop();
                        if (que.empty()) {
                            res.copy(*(op1.first));
                            if (op1.second) delete op1.first;
                            break;
                        }

                        op2 = que.top();
                        que.pop();
                        tmp.second = true;
                        tmp.first = *(op1.first) | *(op2.first);
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                        LOGGER(ibis::gVerbose >= 0)
                            << "DEBUG -- addBins-using priority queue: "
                            << op1.first->bytes()
                            << (op1.second ? "(transient), " : ", ")
                            << op2.first->bytes()
                            << (op2.second ? "(transient) >> " : " >> ")
                            << tmp.first->bytes();
#endif
                        if (op1.second) delete op1.first;
                        if (op2.second) delete op2.first;
                        if (! que.empty()) {
                            que.push(tmp);
                            tmp.first = 0;
                        }
                    }
                    if (tmp.first != 0) {
                        if (tmp.second) {
                            res |= *(tmp.first);
                            delete tmp.first;
                            tmp.first = 0;
                        }
                        else {
                            res |= *(tmp.first);
                        }
                    }
                }
                catch (...) { // need to free the pointers
                    delete tmp.first;
                    while (! que.empty()) {
                        tmp = que.top();
                        if (tmp.second)
                            delete tmp.first;
                        que.pop();
                    }
                    throw;
                }
            }
            else {
                LOGGER(ibis::gVerbose > 5)
                    << "index::addBins(" << ib << ", " << ie
                    << ") decompresses the result before ORing the bitmaps";
                res.decompress(); // explicit decompression needed
                for (uint32_t i = ib; i < ie; ++i) {
                    if (bits[i])
                        res |= *(bits[i]);
                }
            }
        }
    } // if (straight)
    else { // use complements
        uint32_t sum2;
        ibis::bitvector sum;
        if (ib > 1) {
            activate(0, 2);
            if (bits[0] && bits[1])
                sum2 = bits[0]->bytes() + (bits[1] ? bits[1]->bytes() : 0U);
            else if (bits[0])
                sum2 = bits[0]->bytes();
            else if (bits[1])
                sum2 = bits[1]->bytes();
            else
                sum2 = 0;
        }
        else if (ib == 1) {
            if (bits[0]) {
                if (bits[ie])
                    sum2 = bits[0]->bytes() + bits[ie]->bytes();
                else
                    sum2 = bits[0]->bytes();
            }
            else if (bits[ie]) {
                sum2 = bits[ie]->bytes();
            }
            else {
                sum2 = 0;
            }
        }
        else {
            sum2 = (bits[ie] ? bits[ie]->bytes() : 0U) +
                (bits[ie+1] ? bits[ie+1]->bytes() : 0U);
        }
        if (sum2 >= uncomp) { // take advantage of built-in decopression
            LOGGER(ibis::gVerbose > 5)
                << "index::addBins(" << ib << ", " << ie
                << ") takes a simple loop to OR the bitmaps (complement)";
            if (ib > 1) {
                if (bits[0])
                    sum.copy(*(bits[0]));
                else
                    sum.set(0, nrows);
                for (uint32_t i = 1; i < ib; ++i)
                    if (bits[i])
                        sum |= *(bits[i]);
            }
            else if (ib == 1) {
                if (bits[0])
                    sum.copy(*(bits[0]));
                else
                    sum.set(0, nrows);
            }
            else {
                for (; bits[ie] == 0 && ie < nobs; ++ ie);
                if (ie < nobs)
                    sum.copy(*(bits[ie]));
                else
                    sum.set(0, nrows);
            }
            for (uint32_t i = ie; i < nobs; ++i) {
                if (bits[i])
                    sum |= *(bits[i]);
            }
        }
        else { // need to look at the total size
            if (bytes == 0) {
                for (uint32_t i = 0; i < ib; ++i) {
                    if (bits[i])
                        bytes += bits[i]->bytes();
                }
                for (uint32_t i = ie; i < nobs; ++i) {
                    if (bits[i])
                        bytes += bits[i]->bytes();
                }
            }
            if (bytes*static_cast<double>(na)*na <= log(2.0)*uncomp) {
                LOGGER(ibis::gVerbose > 5)
                    << "index::addBins(" << ib << ", " << ie
                    << ") uses a priority queue to OR the bitmaps (complement)";
                // use priority queue for all bitmaps
                std::priority_queue<_elem> que;
                _elem op1, op2, tmp;
                tmp.first = 0;

                // populate the priority queue with the original input
                for (uint32_t i = 0; i < ib; ++i) {
                    if (bits[i]) {
                        op1.first = bits[i];
                        op1.second = false;
                        que.push(op1);
                    }
                }
                for (uint32_t i = ie; i < nobs; ++i) {
                    if (bits[i]) {
                        op1.first = bits[i];
                        op1.second = false;
                        que.push(op1);
                    }
                }

                try {
                    while (! que.empty()) {
                        op1 = que.top();
                        que.pop();
                        if (que.empty()) {
                            res.copy(*(op1.first));
                            if (op1.second) delete op1.first;
                            break;
                        }

                        op2 = que.top();
                        que.pop();
                        tmp.second = true;
                        tmp.first = *(op1.first) | *(op2.first);
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                        LOGGER(ibis::gVerbose >= 0)
                            << "DEBUG -- addBins-using priority queue: "
                            << op1.first->bytes()
                            << (op1.second ? "(transient), " : ", ")
                            << op2.first->bytes()
                            << (op2.second ? "(transient) >> " : " >> ")
                            << tmp.first->bytes();
#endif
                        if (op1.second) delete op1.first;
                        if (op2.second) delete op2.first;
                        if (! que.empty()) {
                            que.push(tmp);
                            tmp.first = 0;
                        }
                    }
                    if (tmp.first != 0) {
                        if (tmp.second) {
                            sum.swap(*(tmp.first));
                            delete tmp.first;
                            tmp.first = 0;
                        }
                        else {
                            sum.copy(*(tmp.first));
                        }
                    }
                }
                catch (...) { // need to free the pointers
                    delete tmp.first;
                    while (! que.empty()) {
                        tmp = que.top();
                        if (tmp.second)
                            delete tmp.first;
                        que.pop();
                    }
                    throw;
                }
            }
            else if (sum2 <= (uncomp >> 2)){
                LOGGER(ibis::gVerbose > 5)
                    << "index::addBins(" << ib << ", " << ie
                    << ") decompresses the result before ORing the "
                    "bitmaps (complement)";
                // uncompress the first bitmap generated
                if (ib > 1) {
                    if (bits[0])
                        sum.copy(*(bits[0]));
                    else
                        sum.set(0, nrows);
                    if (bits[1]) {
                        sum |= *(bits[1]);
                    }
                    sum.decompress();
                    for (uint32_t i = 2; i < ib; ++i)
                        if (bits[i])
                            sum |= *(bits[i]);
                }
                else if (ib == 1) {
                    if (bits[0])
                        sum.copy(*(bits[0]));
                    else
                        sum.set(0, nrows);
                    sum.decompress();
                }
                else {
                    for (; bits[ie] == 0 && ie < nobs; ++ ie);
                    if (ie < nobs) {
                        sum.copy(*(bits[ie]));
                        ++ ie;
                        if (ie < nobs)
                            sum.decompress();
                    }
                    else {
                        sum.set(0, nrows);
                    }
                }
                for (uint32_t i = ie; i < nobs; ++i)
                    if (bits[i])
                        sum |= *(bits[i]);
            }
            else if (ib > 0) {
                LOGGER(ibis::gVerbose > 5)
                    << "index::addBins(" << ib << ", " << ie
                    << ") decompresses the result before ORing the "
                    "bitmaps (complement)";
                if (bits[0])
                    sum.copy(*(bits[0]));
                else
                    sum.set(0, nrows);
                sum.decompress();
                for (uint32_t i = 1; i < ib; ++i)
                    if (bits[i])
                        sum |= *(bits[i]);
                for (uint32_t i = ie; i < nobs; ++i)
                    if (bits[i])
                        sum |= *(bits[i]);
            }
            else {
                LOGGER(ibis::gVerbose > 5)
                    << "index::addBins(" << ib << ", " << ie
                    << ") decompresses the result before ORing the "
                    "bitmaps (complement)";
                for (; bits[ie] == 0 && ie < nobs; ++ ie);
                if (ie < nobs) {
                    sum.copy(*(bits[ie]));
                    ++ ie;
                    if (ie < nobs)
                        sum.decompress();
                }
                else {
                    sum.set(0, nrows);
                }
                for (uint32_t i = ie; i < nobs; ++i)
                    if (bits[i])
                        sum |= *(bits[i]);
            }
        }
        // need to flip because we have been using complement
        ibis::bitvector tmp(tot);
        tmp -= sum;
        res |= tmp;
    }
    if (ibis::gVerbose > 4) {
        timer.stop();
        LOGGER(ibis::gVerbose > 4)
            << "index::addBins operated on " << na << " bitmap"
            << (na>1?"s":"") << " (" << bytes << " in " << res.bytes()
            << " out) took " << timer.CPUTime() << " sec(CPU), "
            << timer.realTime() << " sec(elapsed)";
    }
#if DEBUG+0 > 0 || _DEBUG+0 > 0
    if (ibis::gVerbose > 30 || (1U << ibis::gVerbose) >= res.bytes()) {
        LOGGER(ibis::gVerbose >= 0)
            << "DEBUG -- addBins(" << ib << ", " << ie << "):" << res;
    }
#endif
} // ibis::index::addBins

/// Sum up <code>bits[ib:ie-1]</code> and place the result in res.  The
/// bitmaps (bits) are stored in the argument buf and have to be
/// regenerated based on the information in offset64.
/// The basic rule is as follows.
/// - If there are two or less bit vectors, use |= operator directly.
/// - Compute the total size of the bitmaps involved.
/// - If the total size times log(number of bitvectors involved) is less
///   than the size of an uncompressed bitvector, use a priority queue to
///   store all input bitvectors and intermediate solutions,
/// - or else, decompress the first bitvector and use inplace bitwise OR
///   operator to complete the operations.
void ibis::index::sumBins(uint32_t ib, uint32_t ie, ibis::bitvector& res,
                          uint32_t *buf) const {
    std::string evt = "index";
    if (ibis::gVerbose > 2 && col != 0) {
        evt += '[';
        evt += col->fullname();
        evt += ']';
    }
    evt += "::sumBins";
    LOGGER(ibis::gVerbose > 6)
        << evt << ": ib=" << ib << ", ie=" << ie << ", res(" << res.cnt()
        << ", " << res.size() << ")";
    const size_t nobs = offset64.size() - 1;
    res.clear();
    if (ie > nobs || ib >= nobs || ib >= ie) {
        LOGGER(ibis::gVerbose > 3)
            << "Waring -- " << evt << " encounters an empty range (ib=" << ib
            << ", ie=" << ie << ", offset64.size()=" << offset64.size();
        return;
    }
    while (ib < ie && offset64[ib+1] == offset64[ib]) ++ ib;
    while (ib < ie && offset64[ie] == offset64[ie-1]) -- ie;
    if (ie > nobs || ib >= nobs || ib >= ie) {
        LOGGER(ibis::gVerbose > 3)
            << "Waring -- " << evt << " encounters an empty range (ib=" << ib
            << ", ie=" << ie << ", offset64.size()=" << offset64.size();
        return;
    }

    const uint32_t na = ie-ib;
    if (na == 1) { // some special cases
        ibis::bitvector tmp(buf, offset64[ie]-offset64[ib]);
        res.swap(tmp);
        return;
    }
    else if (na == 2) {
        ibis::bitvector tmp1(buf, offset64[ib+1]-offset64[ib]);
        ibis::bitvector tmp2(buf+offset64[ib+1]-offset64[ib],
                             offset64[ib+2]-offset64[ib+1]);
        tmp1.swap(res);
        res |= tmp2;
        return;
    }
    else if (na == 0) {
        return;
    }

    horometer timer;
    uint32_t bytes = 4*(offset64[ie]-offset64[ib]);
    // based on extensive testing, we have settled on the following
    // combination
    // (1) if the two size of the first two are greater than the
    // uncompressed size of one bitmap, use option 1.  Because the
    // operation of the first two will produce an uncompressed result,
    // it will sum together all other bits with to the uncompressed
    // result generated already.
    // (2) if total size (bytes) times log 2 of number of bitmaps
    // is less than or equal to the size of an uncompressed
    // bitmap, use option 3, else use option 4.
    if (ibis::gVerbose > 4) {
        LOGGER(ibis::gVerbose > 4)
            << evt << " is to use the combined option on " << na << " bitmaps";
        timer.start();
    }
    if (nrows == 0) {
        ibis::bitvector tmp(buf, offset64[ib+1]-offset64[ib]);
        const_cast<index*>(this)->nrows = tmp.size();
    }
    const uint32_t uncomp = (ibis::bitvector::bitsPerLiteral() == 8 ?
                             nrows * 2 / 15 : nrows * 4 / 31);
    uint32_t sum2 = (offset64[ib+2]-offset64[ib]);
    if (sum2 >= uncomp) {
        LOGGER(ibis::gVerbose > 5)
            << evt << "(" << ib << ", " << ie
            << ") performs bitwise OR with a simple loop";
        {
            ibis::bitvector tmp1(buf, offset64[ib+1]-offset64[ib]);
            ibis::bitvector tmp2(buf+offset64[ib+1]-offset64[ib],
                                 offset64[ib+2]-offset64[ib+1]);
            res.swap(tmp1);
            res |= tmp2;
        }
        for (uint32_t i = ib+2; i < ie; ++i) {
            ibis::bitvector tmp(buf+offset64[i]-offset64[ib],
                                offset64[i+1]-offset64[i]);
            res |= tmp;
        }
    }
    else {
        if (bytes*static_cast<double>(na)*na <= log(2.0)*uncomp) {
            LOGGER(ibis::gVerbose > 5)
                << evt << "(" << ib << ", " << ie
                << ") performs bitwise OR with a priority queue";
            // put all bitmaps in a priority queue
            std::priority_queue<ibis::bitvector*> que;
            ibis::bitvector *op1, *op2, *tmp;
            tmp = 0;

            // populate the priority queue with the original input
            for (uint32_t i = ib; i < ie; ++i) {
                tmp = new ibis::bitvector(buf+offset64[i]-offset64[ib],
                                          offset64[i+1]-offset64[i]);
                que.push(tmp);
            }

            try {
                while (! que.empty()) {
                    op1 = que.top();
                    que.pop();
                    if (que.empty()) {
                        res.swap(*op1);
                        delete op1;
                        break;
                    }

                    op2 = que.top();
                    que.pop();
                    tmp = (*op1) | (*op2);
                    delete op1;
                    delete op2;
                    if (! que.empty()) {
                        que.push(tmp);
                        tmp = 0;
                    }
                }
                if (tmp != 0) {
                    res.swap(*tmp);
                    delete tmp;
                    tmp = 0;
                }
            }
            catch (...) { // need to free the pointers
                delete tmp;
                while (! que.empty()) {
                    tmp = que.top();
                    delete tmp;
                    que.pop();
                }
                throw;
            }
        }
        else if (sum2 <= (uncomp >> 2)) {
            // use uncompressed res
            LOGGER(ibis::gVerbose > 5)
                << evt << "(" << ib << ", " << ie
                << ") performs bitwise OR with a decompressed result";
            {
                ibis::bitvector tmp(buf, offset64[ib+1]-offset64[ib]);
                res.copy(tmp);
            }
            res.decompress();
            for (uint32_t i = ib+1; i < ie; ++i) {
                ibis::bitvector tmp(buf+offset64[i]-offset64[ib],
                                    offset64[i+1]-offset64[i]);
                res |= tmp;
            }
        }
        else {
            LOGGER(ibis::gVerbose > 5)
                << "index::sumBins(" << ib << ", " << ie
                << ") performs bitwise OR with a simple loop";
            {
                ibis::bitvector tmp(buf, offset64[ib+1]-offset64[ib]);
                res.copy(tmp);
            }
            for (uint32_t i = ib+1; i < ie; ++i) {
                ibis::bitvector tmp(buf+offset64[i]-offset64[ib],
                                    offset64[i+1]-offset64[i]);
                res |= tmp;
            }
        }
    }
    if (ibis::gVerbose > 4) {
        timer.stop();
        LOGGER(ibis::gVerbose > 4)
            << evt << " operated on " << na << " bitmaps, took " << timer.CPUTime()
            << " sec(CPU) and " << timer.realTime() << " sec(elased)";
    }
#if DEBUG+0 > 0 || _DEBUG+0 > 0
    if (ibis::gVerbose > 30 || (1U << ibis::gVerbose) >= res.bytes()) {
        LOGGER(ibis::gVerbose >= 0)
            << "DEBUG -- sumBins(" << ib << ", " << ie << "):" << res;
    }
#endif
} // ibis::index::sumBins

/// Sum up <code>bits[ib:ie-1]</code> and place the result in res.  The
/// bitmaps (bits) are held by this index object and may be regenerated as
/// needed.  It uses the combined strategy that was determined in an series
/// of earlier tests.  The basic rule is as follows.
/// - If there are two or less bit vectors, use |= operator directly.
/// - Compute the total size of the bitmaps involved.
/// - If the total size times log(number of bitvectors involved) is less
///   than the size of an uncompressed bitvector, use a priority queue to
///   store all input bitvectors and intermediate solutions,
/// - or else, decompress the first bitvector and use inplace bitwise OR
///   operator to complete the operations.
void ibis::index::sumBins(uint32_t ib, uint32_t ie,
                          ibis::bitvector& res) const {
    LOGGER(ibis::gVerbose > 6)
        << "index[" << (col != 0 ? col->name() : "?.?") << "]::sumBins(" << ib
        << ", " << ie << ", res(" << res.cnt() << ", " << res.size()
        << ")) ...";
    const size_t nobs = bits.size();
    if (ie > nobs) ie = nobs;
    if (ib >= ie) {
        res.set(0, nrows); // no bitmap in the range
        return;
    }

    typedef std::pair<ibis::bitvector*, bool> _elem;
    bool straight = true;
    if (offset32.size() <= nobs && offset64.size() <= nobs) {
        // all bitvectors must be in memory
        offset32.clear();
        try {
            offset64.resize(nobs+1);
            offset64[0] = 0;
            for (size_t i = 0; i < nobs; ++ i)
                offset64[i+1] = offset64[i] +
                    (bits[i] ? bits[i]->bytes() : 0U);
        }
        catch (...) {
            offset64.clear();
        }
    }
    if (offset64.size() > nobs) {
        const uint64_t all = offset64[nobs] - offset64[0];
        const uint64_t mid = offset64[ie] - offset64[ib];
        if (mid == 0U) {
            res.set(0, nrows);
            return;
        }
        else if (all == mid) {
            res.set(1, nrows);
            return;
        }
        straight = (mid <= (all >> 1));
    }
    else if (offset32.size() > nobs) {
        const uint32_t all = offset32[nobs] - offset32[0];
        const uint32_t mid = offset32[ie] - offset32[ib];
        if (mid == 0U) {
            res.set(0, nrows);
            return;
        }
        else if (all == mid) {
            res.set(1, nrows);
            return;
        }
        straight = (mid <= (all >> 1));
    }
    else {
        straight = (ie-ib <= (nobs >> 1));
    }

    if (breader || str || fname) { // try to activate the needed bitmaps
        if (straight) {
            activate(ib, ie);
        }
        else {
            activate(0, ib);
            activate(ie, nobs);
        }
    }
    const uint32_t na = (straight ? ie-ib : nobs + ib - ie);
    if (na <= 2) { // some special cases
        if (ib >= ie) { // no bitmap in the range
            res.set(0, nrows);
        }
        else if (ib == 0 && ie == nobs) { // every bitmap in the range
            res.set(1, nrows);
        }
        else if (na == 1) {
            if (straight) { // only one bitmap in the range
                if (bits[ib]) {
                    res.copy(*(bits[ib]));
                }
                else
                    res.set(0, nrows);
            }
            else if (ib == 0) { // last one is outside
                if (bits[ie]) {
                    res.copy(*(bits[ie]));
                    res.flip();
                }
                else {
                    res.set(1, nrows);
                }
            }
            else { // the first one is outside
                if (bits[0]) {
                    res.copy(*(bits[0]));
                    res.flip();
                }
                else {
                    res.set(1, nrows);
                }
            }
        }
        else if (straight) { // two consecutive bitmaps in the range
            if (bits[ib]) {
                ibis::bitvector tmp(*(bits[ib]));
                res.swap(tmp);
                if (bits[ib+1])
                    res |= *(bits[ib+1]);
            }
            else if (bits[ib+1]) {
                res.copy(*(bits[ib+1]));
            }
            else {
                res.set(0, nrows);
            }
        }
        else if (ib == 0) { // two bitmaps at the end are outside
            if (bits[ie]) {
                res.copy(*(bits[ie]));
                if (bits[nobs-1])
                    res |= *(bits[nobs-1]);
                res.flip();
            }
            else if (bits[nobs-1]) {
                res.copy(*(bits[nobs-1]));
                res.flip();
            }
            else {
                res.set(1, nrows);
            }
        }
        else if (ib == 1) { // two outside bitmaps are split
            if (bits[0])
                res.copy(*(bits[0]));
            else
                res.set(0, nrows);
            if (bits[ie])
                res |= *(bits[ie]);
            res.flip();
        }
        else if (ib == 2) { // two outside bitmaps are at the beginning
            if (bits[0])
                res.copy(*(bits[0]));
            else
                res.set(0, nrows);
            if (bits[1])
                res |= *(bits[1]);
            res.flip();
        }
        return;
    }

    horometer timer;
    uint32_t bytes = 0;
    // based on extensive testing, we have settled on the following
    // combination
    // (1) if the two size of the first two are greater than the
    // uncompressed size of one bitmap, use option 1.  Because the
    // operation of the first two will produce an uncompressed result,
    // it will sum together all other bits with to the uncompressed
    // result generated already.
    // (2) if total size (bytes) times log 2 of number of bitmaps
    // is less than or equal to the size of an uncompressed
    // bitmap, use option 3, else use option 4.
    if (ibis::gVerbose > 4) {
        ibis::util::logMessage("index", "sumBins(%lu, %lu) will operate on "
                               "%lu out of %lu bitmaps using the combined "
                               "option", static_cast<long unsigned>(ib),
                               static_cast<long unsigned>(ie),
                               static_cast<long unsigned>(na),
                               static_cast<long unsigned>(nobs));
        timer.start();
        if (straight) {
            for (uint32_t i = ib; i < ie; ++i) {
                if (bits[i])
                    bytes += bits[i]->bytes();
            }
        }
        else {
            for (uint32_t i = 0; i < ib; ++i) {
                if (bits[i])
                    bytes += bits[i]->bytes();
            }
            for (uint32_t i = ie; i < nobs; ++i) {
                if (bits[i])
                    bytes += bits[i]->bytes();
            }
        }
    }
    const uint32_t uncomp = (ibis::bitvector::bitsPerLiteral() == 8 ?
                             nrows * 2 / 15 : nrows * 4 / 31);
    if (straight) {
        uint32_t sum2 = (bits[ib] ? bits[ib]->bytes() : 0U) +
            (bits[ib+1] ? bits[ib+1]->bytes() : 0U);
        if (sum2 >= uncomp) {
            LOGGER(ibis::gVerbose > 5)
                << "index::sumBins(" << ib << ", " << ie
                << ") performs bitwise OR with a simple loop";
            if (bits[ib]) {
                res.copy(*(bits[ib]));
                if (bits[ib+1])
                    res |= *(bits[ib+1]);
            }
            else if (bits[ib+1]) {
                res.copy(*(bits[ib+1]));
            }
            else {
                res.set(0, nrows);
            }
            for (uint32_t i = ib+2; i < ie; ++i) {
                if (bits[i])
                    res |= *(bits[i]);
            }
        }
        else {
            // need to compute the total size of all bitmaps
            if (bytes == 0) {
                for (uint32_t i = ib; i < ie; ++i) {
                    if (bits[i])
                        bytes += bits[i]->bytes();
                }
            }
            if (bytes*static_cast<double>(na)*na <= log(2.0)*uncomp) {
                LOGGER(ibis::gVerbose > 5)
                    << "index::sumBins(" << ib << ", " << ie
                    << ") performs bitwise OR with a priority queue";
                // put all bitmaps in a priority queue
                std::priority_queue<_elem> que;
                _elem op1, op2, tmp;
                tmp.first = 0;

                // populate the priority queue with the original input
                for (uint32_t i = ib; i < ie; ++i) {
                    if (bits[i]) {
                        op1.first = bits[i];
                        op1.second = false;
                        que.push(op1);
                    }
                }

                try {
                    while (! que.empty()) {
                        op1 = que.top();
                        que.pop();
                        if (que.empty()) {
                            res.copy(*(op1.first));
                            if (op1.second) delete op1.first;
                            break;
                        }

                        op2 = que.top();
                        que.pop();
                        tmp.second = true;
                        tmp.first = *(op1.first) | *(op2.first);
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                        LOGGER(ibis::gVerbose >= 0)
                            << "DEBUG -- sumBins using priority queue: "
                            << op1.first->bytes()
                            << (op1.second ? "(transient), " : ", ")
                            << op2.first->bytes()
                            << (op2.second ? "(transient) >> " : " >> ")
                            << tmp.first->bytes();
#endif
                        if (op1.second) delete op1.first;
                        if (op2.second) delete op2.first;
                        if (! que.empty()) {
                            que.push(tmp);
                            tmp.first = 0;
                        }
                    }
                    if (tmp.first != 0) {
                        if (tmp.second) {
                            res.swap(*(tmp.first));
                            delete tmp.first;
                            tmp.first = 0;
                        }
                        else {
                            res.copy(*(tmp.first));
                        }
                    }
                }
                catch (...) { // need to free the pointers
                    delete tmp.first;
                    while (! que.empty()) {
                        tmp = que.top();
                        if (tmp.second)
                            delete tmp.first;
                        que.pop();
                    }
                    throw;
                }
            }
            else if (sum2 <= (uncomp >> 2)) {
                // use uncompressed res
                LOGGER(ibis::gVerbose > 5)
                    << "index::sumBins(" << ib << ", " << ie
                    << ") performs bitwise OR with a decompressed result";
                if (bits[ib]) {
                    res.copy(*(bits[ib]));
                    if (bits[ib+1])
                        res |= *(bits[ib+1]);
                }
                else if (bits[ib+1]) {
                    res.copy(*(bits[ib+1]));
                }
                else {
                    res.set(0, nrows);
                }
                res.decompress();
                for (uint32_t i = ib+2; i < ie; ++i) {
                    if (bits[i])
                        res |= *(bits[i]);
                }
            }
            else {
                LOGGER(ibis::gVerbose > 5)
                    << "index::sumBins(" << ib << ", " << ie
                    << ") performs bitwise OR with a simple loop";
                uint32_t i = ib;
                for (; bits[i] == 0 && i < ie; ++ i); // skip leading nulls
                if (i < ie) {
                    res.copy(*(bits[i]));
                    res.decompress();
                    for (++i; i < ie; ++i) {
                        if (bits[i])
                            res |= *(bits[i]);
                    }
                }
                else {
                    res.set(0, nrows);
                }
            }
        }
    } // if (straight)
    else { // use complements
        uint32_t sum2;
        if (ib > 1) {
            sum2 = (bits[0] ? bits[0]->bytes() : 0U) +
                (bits[1] ? bits[1]->bytes() : 0U);
        }
        else if (ib == 1) {
            sum2 = (bits[0] ? bits[0]->bytes() : 0U) +
                (bits[ie] ? bits[ie]->bytes() : 0U);
        }
        else {
            sum2 = (bits[ie] ? bits[ie]->bytes() : 0U) +
                (bits[ie+1] ? bits[ie+1]->bytes() : 0U);
        }
        if (sum2 >= uncomp) { // take advantage of automated decopression
            LOGGER(ibis::gVerbose > 5)
                << "index::sumBins(" << ib << ", " << ie
                << ") performs bitwise OR with a simple loop (complement)";
            if (ib > 1) {
                if (bits[0])
                    res.copy(*(bits[0]));
                else
                    res.set(0, nrows);
                for (uint32_t i = 1; i < ib; ++i)
                    if (bits[i])
                        res |= *(bits[i]);
            }
            else if (ib == 1) {
                if (bits[0])
                    res.copy(*(bits[0]));
                else
                    res.set(0, nrows);
            }
            else {
                for (; bits[ie] == 0 && ie < nobs; ++ ie);
                if (ie < nobs)
                    res.copy(*(bits[ie]));
                else
                    res.set(0, nrows);
            }
            for (uint32_t i = ie; i < nobs; ++i) {
                if (bits[i])
                    res |= *(bits[i]);
            }
        }
        else { // need to look at the total size
            if (bytes == 0) {
                for (uint32_t i = 0; i < ib; ++i) {
                    if (bits[i])
                        bytes += bits[i]->bytes();
                }
                for (uint32_t i = ie; i < nobs; ++i) {
                    if (bits[i])
                        bytes += bits[i]->bytes();
                }
            }
            if (bytes*static_cast<double>(na)*na <= log(2.0)*uncomp) {
                LOGGER(ibis::gVerbose > 5)
                    << "index::sumBins(" << ib << ", " << ie
                    << ") performs bitwise OR with a priority queue "
                    "(complement)";
                // use priority queue for all bitmaps
                std::priority_queue<_elem> que;
                _elem op1, op2, tmp;
                tmp.first = 0;

                // populate the priority queue with the original input
                for (uint32_t i = 0; i < ib; ++i) {
                    if (bits[i]) {
                        op1.first = bits[i];
                        op1.second = false;
                        que.push(op1);
                    }
                }
                for (uint32_t i = ie; i < nobs; ++i) {
                    if (bits[i]) {
                        op1.first = bits[i];
                        op1.second = false;
                        que.push(op1);
                    }
                }

                try {
                    while (! que.empty()) {
                        op1 = que.top();
                        que.pop();
                        if (que.empty()) {
                            res.copy(*(op1.first));
                            if (op1.second) delete op1.first;
                            break;
                        }

                        op2 = que.top();
                        que.pop();
                        tmp.second = true;
                        tmp.first = *(op1.first) | *(op2.first);
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                        LOGGER(ibis::gVerbose >= 0)
                            << "DEBUG -- sumBins using priority queue: "
                            << op1.first->bytes()
                            << (op1.second ? "(transient), " : ", ")
                            << op2.first->bytes()
                            << (op2.second ? "(transient) >> " : " >> ")
                            << tmp.first->bytes();
#endif
                        if (op1.second) delete op1.first;
                        if (op2.second) delete op2.first;
                        if (! que.empty()) {
                            que.push(tmp);
                            tmp.first = 0;
                        }
                    }
                    if (tmp.first != 0) {
                        if (tmp.second) {
                            res.swap(*(tmp.first));
                            delete tmp.first;
                            tmp.first = 0;
                        }
                        else {
                            res.copy(*(tmp.first));
                        }
                    }
                }
                catch (...) { // need to free the pointers
                    delete tmp.first;
                    while (! que.empty()) {
                        tmp = que.top();
                        if (tmp.second)
                            delete tmp.first;
                        que.pop();
                    }
                    throw;
                }
            }
            else if (sum2 <= (uncomp >> 2)){
                LOGGER(ibis::gVerbose > 5)
                    << "index::sumBins(" << ib << ", " << ie
                    << ") performs bitwise OR with a decompressed result "
                    "(complement)";
                // uncompress the first bitmap generated
                if (ib > 1) {
                    if (bits[0]) {
                        res.copy(*(bits[0]));
                        if (bits[1])
                            res |= *(bits[1]);
                    }
                    else if (bits[1]) {
                        res.copy(*(bits[1]));
                    }
                    if (res.size() != nrows)
                        res.set(0, nrows);
                    res.decompress();
                    for (uint32_t i = 2; i < ib; ++i)
                        if (bits[i])
                            res |= *(bits[i]);
                }
                else if (ib == 1) {
                    if (bits[0])
                        res.copy(*(bits[0]));
                    else
                        res.set(0, nrows);
                    res.decompress();
                }
                else {
                    for (; bits[ie] == 0 && ie < nobs; ++ ie);
                    if (ie < nobs) {
                        res.copy(*(bits[ie]));
                        ++ ie;
                        if (ie < nobs)
                            res.decompress();
                    }
                    else {
                        res.set(0, nrows);
                    }
                }
                for (uint32_t i = ie; i < nobs; ++i)
                    if (bits[i])
                        res |= *(bits[i]);
            }
            else if (ib > 0) {
                LOGGER(ibis::gVerbose > 5)
                    << "index::sumBins(" << ib << ", " << ie
                    << ") performs bitwise OR with a decompressed result "
                    "(complement)";
                if (bits[0])
                    res.copy(*(bits[0]));
                else
                    res.set(0, nrows);
                res.decompress();
                for (uint32_t i = 1; i < ib; ++i)
                    if (bits[i])
                        res |= *(bits[i]);
                for (uint32_t i = ie; i < nobs; ++i)
                    if (bits[i])
                        res |= *(bits[i]);
            }
            else {
                LOGGER(ibis::gVerbose > 5)
                    << "index::sumBins(" << ib << ", " << ie
                    << ") performs bitwise OR with a decompressed result "
                    "(complement)";
                for (; bits[ie] == 0 && ie < nobs; ++ ie);
                if (ie < nobs) {
                    res.copy(*(bits[ie]));
                    ++ ie;
                    if (ie < nobs)
                        res.decompress();
                }
                else {
                    res.set(0, nrows);
                }
                for (uint32_t i = ie; i < nobs; ++i)
                    if (bits[i])
                        res |= *(bits[i]);
            }
        }
        res.flip(); // need to flip because we have been using complement
    }
    if (ibis::gVerbose > 4) {
        timer.stop();
        ibis::util::logMessage("index", "sumBins operated on %u bitmap%s "
                               "(%lu in %lu out) took %g sec(CPU), "
                               "%g sec(elapsed).",
                               static_cast<unsigned>(na), (na>1?"s":""),
                               static_cast<long unsigned>(bytes),
                               static_cast<long unsigned>(res.bytes()),
                               timer.CPUTime(), timer.realTime());
    }
#if DEBUG+0 > 0 || _DEBUG+0 > 0
    if (ibis::gVerbose > 30 || (1U << ibis::gVerbose) >= res.bytes()) {
        LOGGER(ibis::gVerbose >= 0)
            << "DEBUG -- sumBins(" << ib << ", " << ie << "):" << res;
    }
#endif
} // ibis::index::sumBins

/// Compute a new sum for bit vectors [ib, ie) by taking advantage of the
/// old sum for bitvectors [ib0, ie0).
/// This function attempts to take advantage of existing results of a
/// previously computed sum.
/// - On input, res = sum_{i=ib0}^{ie0-1} bits[i].
/// - On exit, res = sum_{i=ib}^{ie-1} bits[i].
void ibis::index::sumBins(uint32_t ib, uint32_t ie, ibis::bitvector& res,
                          uint32_t ib0, uint32_t ie0) const {
    LOGGER(ibis::gVerbose > 6)
        << "index[" << (col != 0 ? col->name() : "?.?") << "]::sumBins(" << ib
        << ", " << ie << ", res(" << res.cnt() << ", " << res.size()
        << "), " << ib0 << ", " << ie0 << ") ...";
    if (offset32.size() <= bits.size() && offset64.size() <= bits.size()) {
        // all bitvectors must be in memory
        offset32.clear();
        try {
            offset64.resize(bits.size()+1);
            offset64[0] = 0;
            for (size_t i = 0; i < bits.size(); ++ i)
                offset64[i+1] = offset64[i] +
                    (bits[i] ? bits[i]->bytes() : 0U);
        }
        catch (...) {
            offset64.clear();
        }
    }

    if (ie > bits.size())
        ie = bits.size();
    if (ib0 > ie || ie0 < ib || ib0 >= ie0 ||
        res.size() != nrows) {  // no overlap
        sumBins(ib, ie, res);
    }
    else { // [ib, ie] overlaps [ib0, ie0]
        const uint32_t ib1 = (ib0 >= ib ? ib0 : ib);
        const uint32_t ie1 = (ie0 <= ie ? ie0 : ie);
        bool local; // do the operations here (true) or call sumBins
        if (offset64.size() > bits.size()) {
            uint32_t change = (offset64[ib1] - offset64[ib0>=ib ? ib : ib0])
                + (offset64[ie0 <= ie ? ie : ie0] - offset64[ie1]);
            uint32_t direct = offset64[ie] - offset64[ib];
            local = (change <= direct);
        }
        else if (offset32.size() > bits.size()) {
            uint32_t change = (offset32[ib1] - offset32[ib0>=ib ? ib : ib0])
                + (offset32[ie0 <= ie ? ie : ie0] - offset32[ie1]);
            uint32_t direct = offset32[ie] - offset32[ib];
            local = (change <= direct);
        }
        else {
            local = ((ib0 >= ib ? ib0 - ib : ib - ib0) +
                     (ie0 <= ie ? ie - ie0 : ie0 - ie) < ie - ib);
        }

        if (local) { // evaluate new sum here
            if (ib0 < ib) { // take away bits[ib0:ib]
                activate(ib0, ib);
                for (uint32_t i = ib0; i < ib; ++ i)
                    if (bits[i])
                        res -= *(bits[i]);
            }
            else if (ib0 > ib) { // add bits[ib:ib0]
                activate(ib, ib0);
                for (uint32_t i = ib; i < ib0; ++ i) {
                    if (bits[i])
                        res |= *(bits[i]);
                }
            }
            if (ie0 > ie) { // subtract bits[ie:ie0]
                activate(ie, ie0);
                for (uint32_t i = ie; i < ie0; ++ i)
                    if (bits[i])
                        res -= *(bits[i]);
            }
            else if (ie0 < ie) { // add bits[ie0:ie]
                activate(ie0, ie);
                for (uint32_t i = ie0; i < ie; ++ i) {
                    if (bits[i])
                        res |= *(bits[i]);
                }
            }
        }
        else { // evalute the new sum directly
            sumBins(ib, ie, res);
        }
    }
#if DEBUG+0 > 0 || _DEBUG+0 > 0
    if (ibis::gVerbose > 30 || (1U << ibis::gVerbose) >= res.bytes()) {
        LOGGER(ibis::gVerbose >= 0)
            << "DEBUG -- sumBins(" << ib << ", " << ie << "):" << res;
    }
#endif
} // ibis::index::sumBins

/// Sum up the bits in in the specified bins.
void ibis::index::sumBins(const ibis::array_t<uint32_t> &bns,
                          ibis::bitvector &res) const {
    if (bns.empty()) {
        res.set(0, nrows);
        return;
    }
    if (bns.size() == 1) {
        if (bns[0] < bits.size()) {
            activate(bns[0]);
            if (bits[bns[0]] != 0) {
                ibis::bitvector tmp(*(bits[bns[0]]));
                res.swap(tmp);
            }
            else {
                res.set(0, nrows);
            }
        }
        else {
            LOGGER(ibis::gVerbose > 3)
                << "Warning -- index::sumBins encountered a bin number ("
                << bns[0] << ") that is too large, expect to be less than "
                << bits.size();
            res.set(0, nrows);
        }
        return;
    }

    ibis::array_t<ibis::bitvector*> pile;
    pile.reserve(bns.size());
    if (bns.size() >= (bits.size() >> 2)) {
        activate(); // make all bitmaps ready to use
        for (size_t j = 0; j < bns.size(); ++ j) {
            if (bns[j] < bits.size()) {
                if (bits[bns[j]] != 0) {
                    pile.push_back(bits[bns[j]]);
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                    LOGGER(ibis::gVerbose > 0)
                        << "DEBUG -- sumBins adds bits[" << bns[j]
                        << "] with " << bits[bns[j]]->cnt()
                        << " set bit(s) to pile";
#endif
                }
            }
        }
    }
    else {
        for (size_t j = 0; j < bns.size(); ++ j) {
            if (bns[j] < bits.size()) {
                activate(bns[j]);
                if (bits[bns[j]] != 0)
                    pile.push_back(bits[bns[j]]);
            }
        }
    }
    if (pile.empty()) {
        res.set(0, nrows);
    }
    else {
        addBits(pile, 0, pile.size(), res);
    }
} // ibis::index::sumBins

/// Add the @c pile[ib:ie-1] to @c res.  This function always use bitvectors
/// @c pile[ib] through @c pile[ie-1] and expects the caller to have filled
/// these bitvectors already.
///
/// @note The caller need to activate the required bit vectors!  In other
/// words, pile[ib:ie-1] must be in memory.
/// @note If pile[i] is a null pointer, it is skipped, which is equivalent
/// to it being a bitvector of all 0s.
void ibis::index::addBits(const array_t<bitvector*>& pile,
                          uint32_t ib, uint32_t ie, ibis::bitvector& res) {
    LOGGER(ibis::gVerbose > 6)
        << "index::addBits(" << pile.size()
        << "-bitvector set, " << ib << ", " << ie << ", res("
        << res.cnt() << ", " << res.size() << ")) ...";
    const uint32_t nobs = pile.size();
    while (ib < nobs && pile[ib] == 0) // skip the leading nulls
        ++ ib;
    if (ie > nobs)
        ie = nobs;
    if (ib >= ie || ib >= nobs) {
        return;
    }
    if (res.size() != pile[ib]->size()) {
        res.set(0, pile[ib]->size());
    }
    else if (res.cnt() >= res.size()) {
        return; // useless to add more bit
    }

    horometer timer;
    bool decmp = false;
    if (ibis::gVerbose > 4)
        timer.start();
    if (res.size() != pile[ib]->size()) {
        res.copy(*(pile[ib]));
        ++ ib;
    }

    // first determine whether to decompres the result
    if (ie-ib>64) {
        decmp = true;
    }
    else if (ie - ib > 3) {
        uint32_t tot = 0;
        for (uint32_t i = ib; i < ie; ++i)
            if (pile[i])
                tot += pile[i]->bytes();
        if (tot > (res.size() >> 2))
            decmp = true;
        else if (tot > (res.size() >> 3) && ie-ib > 4)
            decmp = true;
    }
    if (decmp) { // use decompressed res
        if (ibis::gVerbose > 5)
            ibis::util::logMessage("index", "addBits(%lu, %lu) using "
                                   "uncompressed bitvector",
                                   static_cast<long unsigned>(ib),
                                   static_cast<long unsigned>(ie));
        res.decompress(); // decompress res
        for (uint32_t i = ib; i < ie; ++i)
            res |= *(pile[i]);
        res.compress();
    }
    else if (ie > ib + 2) { // use compressed res
        typedef std::pair<ibis::bitvector*, bool> _elem;
        std::priority_queue<_elem> que;
        _elem op1, op2, tmp;
        tmp.first = 0;
        if (ibis::gVerbose > 5) 
            ibis::util::logMessage("index", "addBits(%lu, %lu) using "
                                   "compressed bitvector (with a priority "
                                   "queue)", static_cast<long unsigned>(ib),
                                   static_cast<long unsigned>(ie));

        // populate the priority queue with the original input
        for (uint32_t i = ib; i < ie; ++i) {
            if (pile[i]) {
                op1.first = pile[i];
                op1.second = false;
                que.push(op1);
            }
        }

        try {
            while (! que.empty()) {
                op1 = que.top();
                que.pop();
                if (que.empty()) {
                    res.copy(*(op1.first));
                    if (op1.second) delete op1.first;
                    break;
                }

                op2 = que.top();
                que.pop();
                tmp.second = true;
                tmp.first = *(op1.first) | *(op2.first);
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                LOGGER(ibis::gVerbose >= 0)
                    << "DEBUG -- addBits-using priority queue: "
                    << op1.first->bytes()
                    << (op1.second ? "(transient), " : ", ")
                    << op2.first->bytes()
                    << (op2.second ? "(transient) >> " : " >> ")
                    << tmp.first->bytes();
#endif
                if (op1.second)
                    delete op1.first;
                if (op2.second)
                    delete op2.first;
                if (! que.empty()) {
                    que.push(tmp);
                    tmp.first = 0;
                }
            }
            if (tmp.first != 0) {
                if (tmp.second) {
                    res |= *(tmp.first);
                    delete tmp.first;
                    tmp.first = 0;
                }
                else {
                    res |= *(tmp.first);
                }
            }
        }
        catch (...) { // need to free the pointers
            delete tmp.first;
            while (! que.empty()) {
                tmp = que.top();
                if (tmp.second)
                    delete tmp.first;
                que.pop();
            }
            throw;
        }
    }
    else if (ie > ib + 1) {
        if (pile[ib])
            res |= *(pile[ib]);
        if (pile[ib+1])
            res |= *(pile[ib+1]);
    }
    else if (pile[ib]) {
        res |= *(pile[ib]);
    }
    if (ibis::gVerbose > 4) {
        timer.stop();
        ibis::util::logMessage("index", "addBits(%lu, %lu) took %g sec(CPU), "
                               "%g sec(elapsed).",
                               static_cast<long unsigned>(ib),
                               static_cast<long unsigned>(ie),
                               timer.CPUTime(), timer.realTime());
    }
#if DEBUG+0 > 0 || _DEBUG+0 > 0
    if (ibis::gVerbose > 30 || (1U << ibis::gVerbose) >= res.bytes()) {
        LOGGER(ibis::gVerbose >= 0)
            << "DEBUG -- addBits(" << ib << ", " << ie << "):" << res;
    }
#endif
} // ibis::index::addBits

/// Sum up @c pile[ib:ie-1] and place the result in @c res.
///
/// @note This function either uses pile[ib:ie-1] or pile[0:ib-1] and
/// pile[ie:nobs-1] depending which set has more bit vectors!  This requires
/// the caller to determine with set of them are to be used and then load
/// them to memory before calling this function.
///
/// @note This function always uses the operator |=.
/// Tests show that using the function @c setBit is always slower.
void ibis::index::sumBits(const array_t<bitvector*>& pile,
                          uint32_t ib, uint32_t ie, ibis::bitvector& res) {
    LOGGER(ibis::gVerbose > 6)
        << "index::sumBits(" << pile.size()
        << "-bitvector set, " << ib << ", " << ie << ", res("
        << res.cnt() << ", " << res.size() << ")) ...";
    typedef std::pair<ibis::bitvector*, bool> _elem;
    const uint32_t nobs = pile.size();
    if (ie > nobs) ie = nobs;
    const bool straight = (2*(ie-ib) <= nobs);
    const uint32_t na = (straight ? ie-ib : nobs + ib - ie);
    // need to figure out the size of bit vectors
    uint32_t sz = 0;
    for (unsigned i = 0; i < nobs && sz == 0; ++i)
        if (pile[i] != 0)
            sz = pile[i]->size();

    if (ib >= ie) {
        res.set(0, sz); // no bitmap in the range
        return;
    }
    else if (na <= 2) { // some special cases
        if (ib == 0 && ie == nobs) { // every bitmap in the range
            res.set(1, pile[0]->size());
        }
        else if (na == 1) {
            if (straight) { // only one bitmap in the range
                if (pile[ib]) {
                    res.copy(*(pile[ib]));
                }
                else {
                    res.set(0, sz);
                }
            }
            else if (ib == 0) { // last one is outside
                if (pile[ie]) {
                    res.copy(*(pile[ie]));
                    res.flip();
                }
                else {
                    res.set(1, sz);
                }
            }
            else { // the first one is outside
                if (pile[0] != 0) {
                    res.copy(*(pile[0]));
                    res.flip();
                }
                else {
                    res.set(1, sz);
                }
            }
        }
        else if (straight) { // two consecutive bitmaps in the range
            if (pile[ib]) {
                res.copy(*(pile[ib]));
                if (pile[ib+1])
                    res |= *(pile[ib+1]);
            }
            else if (pile[ib+1]) {
                res.copy(*(pile[ib+1]));
            }
            else {
                res.set(0, sz);
            }
        }
        else if (ib == 0) { // two bitmaps at the end are outside
            if (pile[ie]) {
                res.copy(*(pile[ie]));
                if (pile[nobs-1])
                    res |= *(pile[nobs-1]);
                res.flip();
            }
            else if (pile[nobs-1]) {
                res.copy(*(pile[nobs-1]));
                res.flip();
            }
            else {
                res.set(1, sz);
            }
        }
        else if (ib == 1) { // two outside bitmaps are split
            res.copy(*(pile[0]));
            if (pile[ie])
                res |= *(pile[ie]);
            res.flip();
        }
        else if (ib == 2) { // two outside bitmaps are at the beginning
            res.copy(*(pile[0]));
            if (pile[1])
                res |= *(pile[1]);
            res.flip();
        }
        return;
    }

    horometer timer;
    uint32_t bytes = 0;

#if defined(TEST_SUMBINS_OPTIONS)
    if (ibis::gVerbose > 4 || ibis::_sumBits_option != 0) {
        ibis::util::logMessage("index", "sumBits(%lu, %lu) will operate on "
                               "%lu out of %lu bitmaps using option %d",
                               static_cast<long unsigned>(ib),
                               static_cast<long unsigned>(ie),
                               static_cast<long unsigned>(na),
                               static_cast<long unsigned>(nobs),
                               ibis::_sumBits_option);
        if (straight) {
            for (uint32_t i = ib; i < ie; ++i)
                bytes += pile[i]->bytes();
        }
        else {
            for (uint32_t i = 0; i < ib; ++i)
                bytes += pile[i]->bytes();
            for (uint32_t i = ie; i < nobs; ++i)
                bytes += pile[i]->bytes();
        }
        timer.start();
    }

    switch (ibis::_sumBits_option) {
    case 1: // compressed or in natural order
        if (2*(ie-ib) <= nobs) {
            res.copy(*(pile[ib]));
            for (uint32_t i = ib+1; i < ie; ++i)
                res |= *(pile[i]);
        }
        else { // use complement
            if (ib > 0) {
                res.copy(*(pile[0]));
                for (uint32_t i = 1; i < ib; ++i)
                    res |= *(pile[i]);
            }
            else {
                res.copy(*(pile[ie]));
                ++ ie;
            }
            for (uint32_t i = ie; i < nobs; ++i)
                res |= *(pile[i]);
            res.flip();
        }
        break;
    case 2: {// compressed or, sort input bitmap according to size
        array_t<bitvector*> ind;
        ind.reserve(na);
        if (straight) {
            for (uint32_t i = ib; i < ie; ++i)
                ind.push_back(pile[i]);
        }
        else { // use complement
            for (uint32_t i = 0; i < ib; ++i)
                ind.push_back(pile[i]);
            for (uint32_t i = ie; i < nobs; ++i)
                ind.push_back(pile[i]);
        }
        // sort ind according the size of the bitvectors
        // make use the specialized version of std::less
        std::less<ibis::bitvector*> cmp;
        std::sort(ind.begin(), ind.end(), cmp);
        // evaluate according the order ind
        res.copy(*(ind[0]));
        for (uint32_t i = 1; i < na; ++i) {
            res |= *(ind[i]);
#if DEBUG+0 > 0 || _DEBUG+0 > 0
            LOGGER(ibis::gVerbose >= 0)
                << "DEBUG -- sumBits-option 2: " << i << ", "
                << ind[i]->bytes();
#endif
        }
        if (! straight)
            res.flip();
        break;
    }
    case 3: {// compressed or, put all bitmaps on a priority queue
        std::priority_queue<_elem> que;
        _elem op1, op2, tmp;
        tmp.first = 0;

        // populate the priority queue with the original input
        if (straight) {
            for (uint32_t i = ib; i < ie; ++i) {
                op1.first = pile[i];
                op1.second = false;
                que.push(op1);
            }
        }
        else { // use complement
            for (uint32_t i = 0; i < ib; ++i) {
                op1.first = pile[i];
                op1.second = false;
                que.push(op1);
            }
            for (uint32_t i = ie; i < nobs; ++i) {
                op1.first = pile[i];
                op1.second = false;
                que.push(op1);
            }
        }

        try {
            while (! que.empty()) {
                op1 = que.top();
                que.pop();
                if (que.empty()) {
                    res.copy(*(op1.first));
                    if (op1.second) delete op1.first;
                    break;
                }

                op2 = que.top();
                que.pop();
                tmp.second = true;
                tmp.first = *(op1.first) | *(op2.first);
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                LOGGER(ibis::gVerbose >= 0)
                    << "DEBUG -- sumBits-option 3: " << op1.first->bytes()
                    << (op1.second ? "(transient), " : ", ")
                    << op2.first->bytes()
                    << (op2.second ? "(transient) >> " : " >> ")
                    << tmp.first->bytes();
#endif
                if (op1.second) delete op1.first;
                if (op2.second) delete op2.first;
                if (! que.empty()) {
                    que.push(tmp);
                    tmp.first = 0;
                }
            }
            if (tmp.first != 0) {
                if (tmp.second) {
                    res.swap(*(tmp.first));
                    delete tmp.first;
                    tmp.first = 0;
                }
                else {
                    res.copy(*(tmp.first));
                }
            }
        }
        catch (...) { // need to free the pointers
            delete tmp.first;
            while (! que.empty()) {
                tmp = que.top();
                if (tmp.second)
                    delete tmp.first;
                que.pop();
            }
            throw;
        }

        if (! straight)
            res.flip();
        break;
    }
    case 4: {// uncompressed res, start with either from or end
        if (straight) {
            if (pile[ib]->bytes() >= pile[ie-1]->bytes()) {
                res.copy(*(pile[ib]));
                ++ ib;
            }
            else {
                -- ie;
                res.copy(*(pile[ie]));
            }
            res.decompress();
            for (uint32_t i = ib; i < ie; ++i)
                res |= *(pile[i]);
            res.compress();
        }
        else if (ib > 0) {
            if (pile[0]->bytes() >= pile[ib-1]->bytes()) {
                res.copy(*(pile[0]));
                res.decompress();
                for (uint32_t i = 1; i < ib; ++i)
                    res |= *(pile[i]);
            }
            else {
                -- ib;
                res.copy(*(pile[ib]));
                res.decompress();
                for (uint32_t i = 0; i < ib; ++i)
                    res |= *(pile[i]);
            }
            for (uint32_t i = ie; i < nobs; ++i)
                res |= *(pile[i]);
            res.compress();
            res.flip();
        }
        else if (pile[ie]->bytes() >= pile[nobs-1]->bytes()) {
            res.copy(*(pile[ie]));
            res.decompress();
            for (uint32_t i = ie+1; i < nobs; ++i)
                res |= *(pile[i]);
            res.compress();
            res.flip();
        }
        else {
            res.copy(*(pile[nobs-1]));
            res.decompress();
            for (uint32_t i = ie; i < nobs-1; ++i)
                res |= *(pile[i]);
            res.compress();
            res.flip();
        }
        break;
    }
    case 5: {// uncompressed res, start with the heaviest bitmap
        std::vector<uint32_t> ind;
        ind.reserve(na);
        if (straight) {
            for (uint32_t i = ib; i < ie; ++i)
                ind.push_back(i);
        }
        else { // use complement
            for (uint32_t i = 0; i < ib; ++i)
                ind.push_back(i);
            for (uint32_t i = ie; i < nobs; ++i)
                ind.push_back(i);
        }
        uint32_t j = 0;
        for (uint32_t i = 1; i < na; ++i) {
            if (pile[ind[i]]->bytes() > pile[ind[j]]->bytes())
                j = i;
        }
        res.copy(*(pile[ind[j]]));
        res.decompress();
        ind[j] = ind[0];
        for (uint32_t i = 1; i < na; ++i)
            res |= *(pile[ind[i]]);
        res.compress();
        if (! straight)
            res.flip();
        break;
    }
    case 6: {
        // based on the timing results of 1 - 5, here is the rule for
        // choosing which scheme to use
        // (1) if the two size of the first two are greater than the
        // uncompressed size of one bitmap, use option 1.  Because the
        // operation of the first two will produce an uncompressed result,
        // it will sum together all other bitmaps with to the uncompressed
        // result generated already.
        // (2) if total size (bytes) times square root of number of bitmaps
        // is less than or equal to twice the size of an uncompressed
        // bitmap, use option 3, else use option 4.
        const uint32_t uncomp = (ibis::bitvector::pilePerLiteral() == 8 ?
                                 sz * 2 / 15 :
                                 sz * 4 / 31);
        if (straight) {
            uint32_t sum2 = pile[ib]->bytes() + pile[ib+1]->bytes();
            if (sum2 >= uncomp) {
                ibis::bitvector *tmp;
                tmp = *(pile[ib]) | *(pile[ib+1]);
                res.swap(*tmp);
                delete tmp;
                for (uint32_t i = ib+2; i < ie; ++i) {
                    res |= *(pile[i]);
                }
            }
            else {
                // need to compute the total size of all bitmaps
                if (bytes == 0) {
                    for (uint32_t i = ib; i < ie; ++i)
                        bytes += pile[i]->bytes();
                }
                if (bytes*static_cast<double>(na)*na <= log(2.0)*uncomp) {
                    // put all bitmaps in a priority queue
                    std::priority_queue<_elem> que;
                    _elem op1, op2, tmp;
                    tmp.first = 0;

                    // populate the priority queue with the original input
                    for (uint32_t i = ib; i < ie; ++i) {
                        op1.first = pile[i];
                        op1.second = false;
                        que.push(op1);
                    }

                    try {
                        while (! que.empty()) {
                            op1 = que.top();
                            que.pop();
                            if (que.empty()) {
                                res.copy(*(op1.first));
                                if (op1.second) delete op1.first;
                                break;
                            }

                            op2 = que.top();
                            que.pop();
                            tmp.second = true;
                            tmp.first = *(op1.first) | *(op2.first);
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                            LOGGER(ibis::gVerbose >= 0)
                                << "DEBUG -- sumBits-using priority queue: "
                                << op1.first->bytes()
                                << (op1.second ? "(transient), " : ", ")
                                << op2.first->bytes()
                                << (op2.second ? "(transient) >> " : " >> ")
                                << tmp.first->bytes();
#endif
                            if (op1.second) delete op1.first;
                            if (op2.second) delete op2.first;
                            if (! que.empty()) {
                                que.push(tmp);
                                tmp.first = 0;
                            }
                        }
                        if (tmp.first != 0) {
                            if (tmp.second) {
                                res.swap(*(tmp.first));
                                delete tmp.first;
                            }
                            else {
                                res.copy(*(tmp.first));
                            }
                        }
                    }
                    catch (...) { // need to free the pointers
                        delete tmp.first;
                        while (! que.empty()) {
                            tmp = que.top();
                            if (tmp.second)
                                delete tmp.first;
                            que.pop();
                        }
                        throw;
                    }
                }
                else if (sum2 <= (uncomp >> 2)) {
                    // use uncompressed res
                    ibis::bitvector *tmp;
                    tmp = *(pile[ib]) | *(pile[ib+1]);
                    res.swap(*tmp);
                    delete tmp;
                    res.decompress();
                    for (uint32_t i = ib+2; i < ie; ++i)
                        res |= *(pile[i]);
                }
                else {
                    res.copy(*(pile[ib]));
                    res.decompress();
                    for (uint32_t i = ib + 1; i < ie; ++i)
                        res |= *(pile[i]);
                }
            }
        } // if (straight)
        else { // use complements
            uint32_t sum2;
            if (ib > 1) {
                sum2 = pile[0]->bytes() + pile[1]->bytes();
            }
            else if (ib == 1) {
                sum2 = pile[0]->bytes() + pile[ie]->bytes();
            }
            else {
                sum2 = pile[ie]->bytes() + pile[ie+1]->bytes();
            }
            if (sum2 >= uncomp) { // take advantage of automate decopression
                if (ib > 1) {
                    ibis::bitvector *tmp;
                    tmp = *(pile[0]) | *(pile[1]);
                    res.swap(*tmp);
                    delete tmp;
                    for (uint32_t i = 2; i < ib; ++i)
                        res |= *(pile[i]);
                }
                else if (ib == 1) {
                    ibis::bitvector *tmp;
                    tmp = *(pile[0]) | *(pile[ie]);
                    res.swap(*tmp);
                    delete tmp;
                    ++ ie;
                }
                else {
                    ibis::bitvector *tmp;
                    tmp = *(pile[ie]) | *(pile[ie+1]);
                    res.swap(*tmp);
                    delete tmp;
                    ie += 2;
                }
                for (uint32_t i = ie; i < nobs; ++i)
                    res |= *(pile[i]);
            }
            else { // need to look at the total size
                if (bytes == 0) {
                    for (uint32_t i = 0; i < ib; ++i)
                        bytes += pile[i]->bytes();
                    for (uint32_t i = ie; i < nobs; ++i)
                        bytes += pile[i]->bytes();
                }
                if (bytes*static_cast<double>(na)*na <= log(2.0)*uncomp) {
                    // use priority queue for all bitmaps
                    std::priority_queue<_elem> que;
                    _elem op1, op2, tmp;
                    tmp.first = 0;

                    // populate the priority queue with the original input
                    for (uint32_t i = 0; i < ib; ++i) {
                        op1.first = pile[i];
                        op1.second = false;
                        que.push(op1);
                    }
                    for (uint32_t i = ie; i < nobs; ++i) {
                        op1.first = pile[i];
                        op1.second = false;
                        que.push(op1);
                    }

                    try {
                        while (! que.empty()) {
                            op1 = que.top();
                            que.pop();
                            if (que.empty()) {
                                res.copy(*(op1.first));
                                if (op1.second) delete op1.first;
                                break;
                            }

                            op2 = que.top();
                            que.pop();
                            tmp.second = true;
                            tmp.first = *(op1.first) | *(op2.first);
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                            LOGGER(ibis::gVerbose >= 0)
                                << "DEBUG -- sumBits-using priority queue: "
                                << op1.first->bytes()
                                << (op1.second ? "(transient), " : ", ")
                                << op2.first->bytes()
                                << (op2.second ? "(transient) >> " : " >> ")
                                << tmp.first->bytes();
#endif
                            if (op1.second) delete op1.first;
                            if (op2.second) delete op2.first;
                            if (! que.empty()) {
                                que.push(tmp);
                                tmp.first = 0;
                            }
                        }
                        if (tmp.first != 0) {
                            if (tmp.second) {
                                res.swap(*(tmp.first));
                                delete tmp.first;
                                tmp.first = 0;
                            }
                            else {
                                res.copy(*(tmp.first));
                            }
                        }
                    }
                    catch (...) { // need to free the pointers
                        delete tmp.first;
                        while (! que.empty()) {
                            tmp = que.top();
                            if (tmp.second)
                                delete tmp.first;
                            que.pop();
                        }
                        throw;
                    }
                }
                else if (sum2 <= (uncomp >> 2)){
                    // uncompress the first bitmap generated
                    if (ib > 1) {
                        ibis::bitvector *tmp;
                        tmp = *(pile[0]) | *(pile[1]);
                        res.swap(*tmp);
                        delete tmp;
                        res.decompress();
                        for (uint32_t i = 2; i < ib; ++i)
                            res |= *(pile[i]);
                    }
                    else if (ib == 1) {
                        ibis::bitvector *tmp;
                        tmp = *(pile[0]) | *(pile[ie]);
                        res.swap(*tmp);
                        delete tmp;
                        res.decompress();
                        ++ ie;
                    }
                    else {
                        ibis::bitvector *tmp;
                        tmp = *(pile[ie]) | *(pile[ie+1]);
                        res.swap(*tmp);
                        delete tmp;
                        res.decompress();
                        ie += 2;
                    }
                    for (uint32_t i = ie; i < nobs; ++i)
                        res |= *(pile[i]);
                }
                else if (ib > 0) {
                    if (pile[0]->bytes() >= pile[ib-1]->bytes()) {
                        res.copy(*(pile[0]));
                        res.decompress();
                        for (uint32_t i = 1; i < ib; ++i)
                            res |= *(pile[i]);
                    }
                    else {
                        -- ib;
                        res.copy(*(pile[ib]));
                        res.decompress();
                        for (uint32_t i = 0; i < ib; ++i)
                            res |= *(pile[i]);
                    }
                    for (uint32_t i = ie; i < nobs; ++i)
                        res |= *(pile[i]);
                }
                else if (pile[ie]->bytes() >= pile[nobs-1]->bytes()) {
                    res.copy(*(pile[ie]));
                    res.decompress();
                    for (uint32_t i = ie+1; i < nobs; ++i)
                        res |= *(pile[i]);
                }
                else {
                    res.copy(*(pile[nobs-1]));
                    res.decompress();
                    for (uint32_t i = ie; i < nobs-1; ++i)
                        res |= *(pile[i]);
                }
            }
            res.flip(); // need to flip because we have been using complement
        }
        break;
    }
    default:
        if (straight) { // sum less than half of the bitvectors
            bool decmp = false;
            // first determine whether to decompres the result
            if (ie-ib>64) {
                decmp = true;
            }
            else if (ie - ib > 3) {
                uint32_t tot = 0;
                for (uint32_t i = ib; i < ie; ++i)
                    tot += pile[i]->bytes();
                if (tot > (sz >> 2))
                    decmp = true;
                else if (tot > (sz >> 3) && ie-ib > 4)
                    decmp = true;
            }
            if (decmp) { // use decompressed res
                if (ibis::gVerbose > 5) {
                    double sb = 0;
                    for (uint32_t i = ib; i < ie; ++i)
                        sb += pile[i]->bytes();
                    ibis::util::logMessage("index", "sumBits(%lu, %lu) using "
                                           "uncompressed bitvector, total "
                                           "input bitmap size is %lG bytes",
                                           static_cast<long unsigned>(ib),
                                           static_cast<long unsigned>(ie), sb);
                }
                res.copy(*(pile[ib]));
                res.decompress();
                for (uint32_t i = ib+1; i < ie; ++i)
                    res |= *(pile[i]);
            }
            else if (ie > ib + 2) { // use compressed res
                if (ibis::gVerbose > 5) {
                    double sb = 0;
                    for (uint32_t i = ib; i < ie; ++i)
                        sb += pile[i]->bytes();
                    ibis::util::logMessage("index", "sumBits(%lu, %lu) using "
                                           "compressed bitvector, total "
                                           "input bitmap size is %lG bytes",
                                           static_cast<long unsigned>(ib),
                                           static_cast<long unsigned>(ie), sb);
                }
                // first determine an good evaluation order (ind)
                std::vector<uint32_t> ind;
                uint32_t i, j, k;
                ind.reserve(ie-ib);
                for (i = ib; i < ie; ++i)
                    ind.push_back(i);
                // sort ind according the size of bitvectors (insertion sort)
                for (i = 0; i < ie-ib-1; ++i) {
                    k = i + 1;
                    for (j = k+1; j < ie-ib; ++j)
                        if (pile[ind[j]]->bytes() < pile[ind[k]]->bytes())
                            k = j;
                    if (pile[ind[i]]->bytes() > pile[ind[k]]->bytes()) {
                        j = ind[i];
                        ind[i] = ind[k];
                        ind[k] = j;
                    }
                    else {
                        ++ i;
                        if (pile[ind[i]]->bytes() > pile[ind[k]]->bytes()) {
                            j = ind[i];
                            ind[i] = ind[k];
                            ind[k] = j;
                        }
                    }
                }
                // evaluate according the order ind
                res.copy(*(pile[ind[0]]));
                for (i = 1; i < ie-ib; ++i)
                    res |= *(pile[ind[i]]);
            }
            else if (ie > ib + 1) {
                if (ibis::gVerbose > 5) {
                    double sb = pile[ib]->bytes();
                    sb += pile[ib+1]->bytes();
                    ibis::util::logMessage("index", "sumBits(%lu, %lu) using "
                                           "compressed bitvector, total "
                                           "input bitmap size is %lG bytes",
                                           static_cast<long unsigned>(ib),
                                           static_cast<long unsigned>(ie), sb);
                }
                res.copy(*(pile[ib]));
                res |= *(pile[ib+1]);
            }
            else {
                res.copy(*(pile[ib]));
            }
        }
        else if (nobs - ie + ib > 64) { // use uncompressed res
            if (ibis::gVerbose > 5) {
                double sb = 0;
                for (uint32_t i = 0; i < ib; ++i)
                    sb += pile[i]->bytes();
                for (uint32_t i = ie; i < nobs; ++i)
                    sb += pile[i]->bytes();
                ibis::util::logMessage("index", "sumBits(%lu, %lu) using "
                                       "uncompressed bitvecector, total "
                                       "input bitmap size is %lG bytes",
                                       static_cast<long unsigned>(ib),
                                       static_cast<long unsigned>(ie), sb);
            }
            if (ib > 0) {
                -- ib;
                res.copy(*(pile[ib]));
            }
            else {
                res.copy(*(pile[ie]));
                ++ ie;
            }
            res.decompress();
            for (uint32_t i = 0; i < ib; ++i)
                res |= *(pile[i]);
            for (uint32_t i = ie; i < nobs; ++i)
                res |= *(pile[i]);
            res.compress();
            res.flip();
        }
        else { // need to check the sizes of bitvectors to be added
            std::vector<uint32_t> ind;
            bool decmp = false;
            for (uint32_t i=0; i < ib; ++i)
                ind.push_back(i);
            for (uint32_t i=ie; i < nobs; ++i)
                ind.push_back(i);
            if (ind.size() > 64) {
                decmp = true;
            }
            else if (ind.size() > 3) {
                uint32_t tot = 0;
                for (uint32_t i = 0; i < ind.size(); ++i)
                    tot += pile[ind[i]]->bytes();
                if (tot > (sz >> 2))
                    decmp = true;
                else if (tot > (sz >> 3) && ind.size() > 8)
                    decmp = true;
            }
            if (decmp) {
                if (ibis::gVerbose > 5) {
                    double sb = 0;
                    uint32_t j = 0;
                    uint32_t large=0, tmp;
                    for (uint32_t i = 0; i < ind.size(); ++i) {
                        tmp = pile[ind[i]]->bytes();
                        if (tmp > large) {
                            large = tmp;
                            j = i;
                        }
                        sb += tmp;
                    }
                    if (j != 0) {
                        uint32_t k = ind[0];
                        ind[0] = ind[j];
                        ind[j] = k;
                    }
                    ibis::util::logMessage("index", "sumBits(%lu, %lu) using "
                                           "uncompressed bitvecector, total "
                                           "input bitmap size is %lG bytes",
                                           static_cast<long unsigned>(ib),
                                           static_cast<long unsigned>(ie), sb);
                }
                res.copy(*(pile[ind[0]]));
                res.decompress();
                for (uint32_t i = 1; i < ind.size(); ++i)
                    res |= *(pile[ind[i]]);
                res.compress();
            }
            else {
                const uint32_t nb = ind.size();
                if (ibis::gVerbose > 5) {
                    double sb = 0;
                    for (uint32_t i = 0; i < nb; ++i)
                        sb += pile[ind[i]]->bytes();
                    ibis::util::logMessage("index", "sumBits(%lu, %lu) using "
                                           "compressed bitvector, total "
                                           "input bitmap size is %lG bytes",
                                           static_cast<long unsigned>(ib),
                                           static_cast<long unsigned>(ie), sb);
                }
                uint32_t i, j, k;
                // sort the ind array (insertion sort)
                for (i = 0; i < nb-1; ++i) {
                    k = i + 1;
                    for (j = k + 1; j < nb; ++j)
                        if (pile[ind[j]]->bytes() < pile[ind[k]]->bytes())
                            k = j;
                    if (pile[ind[i]]->bytes() > pile[ind[k]]->bytes()) {
                        j = ind[i];
                        ind[i] = ind[k];
                        ind[k] = j;
                    }
                    else {
                        ++ i;
                        if (pile[ind[i]]->bytes() > pile[ind[k]]->bytes()) {
                            j = ind[i];
                            ind[i] = ind[k];
                            ind[k] = j;
                        }
                    }
                }
                // evaluate in the order specified by ind
                res.copy(*(pile[ind[0]]));
                for (i = 1; i < nb; ++i)
                    res |= *(pile[ind[i]]);
            }
            res.flip();
        }
        break;
    } // switch (ibis::_sumBits_option)
    if (ibis::gVerbose > 4 || ibis::_sumBits_option != 0) {
        timer.stop();
        ibis::util::logMessage("index", "sumBits operated on %lu bitmap%s "
                               "using option %d (%lu in %lu out) took "
                               "%g sec(CPU), %g sec(elapsed).",
                               static_cast<long unsigned>(na),
                               (na>1?"s":""), ibis::_sumBits_option,
                               bytes, res.bytes(), timer.CPUTime(),
                               timer.realTime());
    }
#else
    // based on extensive testing, we have settled on the following
    // combination
    // (1) if the two size of the first two are greater than the
    // uncompressed size of one bitmap, use option 1.  Because the
    // operation of the first two will produce an uncompressed result,
    // it will sum together all other bitmaps with to the uncompressed
    // result generated already.
    // (2) if total size (bytes) times log 2 of number of bitmaps
    // is less than or equal to the size of an uncompressed
    // bitmap, use option 3, else use option 4.
    if (ibis::gVerbose > 4) {
        ibis::util::logMessage("index", "sumBits(%lu, %lu) will operate on "
                               "%lu out of %lu bitmaps using the combined "
                               "option", static_cast<long unsigned>(ib),
                               static_cast<long unsigned>(ie),
                               static_cast<long unsigned>(na),
                               static_cast<long unsigned>(nobs));
        timer.start();
        if (straight) {
            for (uint32_t i = ib; i < ie; ++i) {
                if (pile[i])
                    bytes += pile[i]->bytes();
            }
        }
        else {
            for (uint32_t i = 0; i < ib; ++i) {
                if (pile[i])
                    bytes += pile[i]->bytes();
            }
            for (uint32_t i = ie; i < nobs; ++i) {
                if (pile[i])
                    bytes += pile[i]->bytes();
            }
        }
    }
    const uint32_t uncomp = (ibis::bitvector::bitsPerLiteral() == 8 ?
                             sz * 2 / 15 :
                             sz * 4 / 31);
    if (straight) {
        uint32_t sum2 = (pile[ib] ? pile[ib]->bytes() : 0U) +
            (pile[ib+1] ? pile[ib+1]->bytes() : 0U);
        if (sum2 >= uncomp) {
            uint32_t i;
            for (i = ib; i < ie && pile[i] == 0; ++ i);
            if (i < ie)
                res.copy(*(pile[i]));
            else
                res.set(0, sz);
            for (++ i; i < ie; ++ i) {
                if (pile[i])
                    res |= *(pile[i]);
            }
        }
        else {
            // need to compute the total size of all bitmaps
            if (bytes == 0) {
                for (uint32_t i = ib; i < ie; ++i)
                    if (pile[i])
                        bytes += pile[i]->bytes();
            }
            if (bytes*static_cast<double>(na)*na <= log(2.0)*uncomp) {
                // put all bitmaps in a priority queue
                std::priority_queue<_elem> que;
                _elem op1, op2, tmp;
                tmp.first = 0;

                // populate the priority queue with the original input
                for (uint32_t i = ib; i < ie; ++i) {
                    if (pile[i]) {
                        op1.first = pile[i];
                        op1.second = false;
                        que.push(op1);
                    }
                }

                try {
                    while (! que.empty()) {
                        op1 = que.top();
                        que.pop();
                        if (que.empty()) {
                            res.copy(*(op1.first));
                            if (op1.second) delete op1.first;
                            break;
                        }

                        op2 = que.top();
                        que.pop();
                        tmp.second = true;
                        tmp.first = *(op1.first) | *(op2.first);
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                        LOGGER(ibis::gVerbose >= 0)
                            << "DEBUG -- sumBits-using priority queue: "
                            << op1.first->bytes()
                            << (op1.second ? "(transient), " : ", ")
                            << op2.first->bytes()
                            << (op2.second ? "(transient) >> " : " >> ")
                            << tmp.first->bytes() << std::endl;
#endif
                        if (op1.second)
                            delete op1.first;
                        if (op2.second)
                            delete op2.first;
                        if (! que.empty()) {
                            que.push(tmp);
                            tmp.first = 0;
                        }
                    }
                    if (tmp.first != 0) {
                        if (tmp.second) {
                            res.swap(*(tmp.first));
                            delete tmp.first;
                        }
                        else {
                            res.copy(*(tmp.first));
                        }
                    }
                }
                catch (...) { // need to free the pointers
                    delete tmp.first;
                    while (! que.empty()) {
                        tmp = que.top();
                        if (tmp.second)
                            delete tmp.first;
                        que.pop();
                    }
                    throw;
                }
            }
            else {
                uint32_t i;
                for (i = ib; i < ie && pile[i] == 0; ++ i);
                if (i < ie) {
                    res.copy(*(pile[i]));
                    res.decompress();
                    for (++ i; i < ie; ++ i)
                        if (pile[i])
                            res |= *(pile[i]);
                }
                else {
                    res.set(0, sz);
                }
            }
        }
    } // if (straight)
    else { // use complements
        uint32_t sum2;
        if (ib > 1) {
            sum2 = pile[0]->bytes() + (pile[1] ? pile[1]->bytes() : 0U);
        }
        else if (ib == 1) {
            sum2 = pile[0]->bytes() + (pile[ie] ? pile[ie]->bytes() : 0U);
        }
        else {
            sum2 = (pile[ie] ? pile[ie]->bytes() : 0U) +
                (pile[ie+1] ? pile[ie+1]->bytes() : 0U);
        }
        if (sum2 >= uncomp) { // take advantage of automate decopression
            if (ib > 1) {
                res.copy(*(pile[0]));
                for (uint32_t i = 1; i < ib; ++i)
                    if (pile[i])
                        res |= *(pile[i]);
            }
            else if (ib == 1) {
                res.copy(*(pile[0]));
            }
            else {
                while (ie < nobs && pile[ie] == 0)
                    ++ ie;
                if (ie < nobs) {
                    res.copy(*(pile[ie]));
                    ++ ie;
                }
            }
            for (uint32_t i = ie; i < nobs; ++i)
                if (pile[i])
                    res |= *(pile[i]);
        }
        else { // need to look at the total size
            if (bytes == 0) {
                for (uint32_t i = 0; i < ib; ++i)
                    if (pile[i])
                        bytes += pile[i]->bytes();
                for (uint32_t i = ie; i < nobs; ++i)
                    if (pile[i])
                        bytes += pile[i]->bytes();
            }
            if (bytes*static_cast<double>(na)*na <= log(2.0)*uncomp) {
                // use priority queue for all bitmaps
                std::priority_queue<_elem> que;
                _elem op1, op2, tmp;
                tmp.first = 0;

                // populate the priority queue with the original input
                for (uint32_t i = 0; i < ib; ++i) {
                    if (pile[i]) {
                        op1.first = pile[i];
                        op1.second = false;
                        que.push(op1);
                    }
                }
                for (uint32_t i = ie; i < nobs; ++i) {
                    if (pile[i]) {
                        op1.first = pile[i];
                        op1.second = false;
                        que.push(op1);
                    }
                }

                try {
                    while (! que.empty()) {
                        op1 = que.top();
                        que.pop();
                        if (que.empty()) {
                            res.copy(*(op1.first));
                            if (op1.second) delete op1.first;
                            break;
                        }

                        op2 = que.top();
                        que.pop();
                        tmp.second = true;
                        tmp.first = *(op1.first) | *(op2.first);
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                        LOGGER(ibis::gVerbose >= 0)
                            << "DEBUG -- sumBits-using priority queue: "
                            << op1.first->bytes()
                            << (op1.second ? "(transient), " : ", ")
                            << op2.first->bytes()
                            << (op2.second ? "(transient) >> " : " >> ")
                            << tmp.first->bytes();
#endif
                        if (op1.second)
                            delete op1.first;
                        if (op2.second)
                            delete op2.first;
                        if (! que.empty()) {
                            que.push(tmp);
                            tmp.first = 0;
                        }
                    }
                    if (tmp.first != 0) {
                        if (tmp.second) {
                            res.swap(*(tmp.first));
                            delete tmp.first;
                        }
                        else {
                            res.copy(*(tmp.first));
                        }
                    }
                }
                catch (...) { // need to free the pointers
                    delete tmp.first;
                    while (! que.empty()) {
                        tmp = que.top();
                        if (tmp.second)
                            delete tmp.first;
                        que.pop();
                    }
                    throw;
                }
            }
            else if (sum2 <= (uncomp >> 2)){
                // uncompress the first bitmap generated
                if (ib > 1) {
                    res.copy(*(pile[0]));
                    res.decompress();
                    for (uint32_t i = 1; i < ib; ++i)
                        if (pile[i])
                            res |= *(pile[i]);
                }
                else if (ib == 1) {
                    res.copy(*(pile[0]));
                    res.decompress();
                }
                else {
                    while (ie < nobs && pile[ie] == 0)
                        ++ ie;
                    if (ie < nobs) {
                        res.copy(*(pile[ie]));
                        res.decompress();
                        ++ ie;
                    }
                    else {
                        res.set(0, sz);
                    }
                }
                for (uint32_t i = ie; i < nobs; ++i)
                    if (pile[i])
                        res |= *(pile[i]);
            }
            else if (ib > 0) {
                res.copy(*(pile[0]));
                res.decompress();
                for (uint32_t i = 1; i < ib; ++i)
                    if (pile[i])
                        res |= *(pile[i]);
                for (uint32_t i = ie; i < nobs; ++i)
                    if (pile[i])
                        res |= *(pile[i]);
            }
            else {
                while (ie < nobs && pile[ie] == 0)
                    ++ ie;
                if (ie < nobs) {
                    res.copy(*(pile[ie]));
                    res.decompress();
                    for (uint32_t i = ie+1; i < nobs; ++i)
                        if (pile[i])
                            res |= *(pile[i]);
                }
                else {
                    res.set(0, sz);
                }
            }
        }
        res.flip(); // need to flip because we have been using complement
    }
    if (ibis::gVerbose > 4) {
        timer.stop();
        LOGGER(ibis::gVerbose > 4)
            << "index::sumBits operated on " << na << " bitmap"
            << (na>1?"s":"") << "(" << bytes << " B in " << res.bytes()
            << " B out) took " << timer.CPUTime() << " sec(CPU), "
            << timer.realTime() << " sec(elapsed)";
    }
#endif
#if DEBUG+0 > 0 || _DEBUG+0 > 0
    if (ibis::gVerbose > 30 || (1U << ibis::gVerbose) >= res.bytes()) {
        LOGGER(ibis::gVerbose >= 0)
            << "DEBUG -- sumBits(" << ib << ", " << ie << "):" << res;
    }
#endif
} // ibis::index::sumBits

/// Sum up @c pile[ib:ie-1] and add the result to @c res.  It is assumed
/// that all pile add up to @c tot.  In the other version of sumBits without
/// this argument @c tot, it was assumed that all bitmaps add up to a bit
/// vector of all ones.  The decision of whether to use pile[ib:ie-1]
/// directly or use the subtractive version (with pile[0:ib-1] and
/// pile[ie:nobs-1]) are based on the number of bit vectors.  The caller is
/// responsible to ensuring the necessary bitmaps are already in memory
/// before calling this function.
void ibis::index::sumBits(const array_t<bitvector*>& pile,
                          const ibis::bitvector& tot, uint32_t ib,
                          uint32_t ie, ibis::bitvector& res) {
    LOGGER(ibis::gVerbose > 6)
        << "index::sumBits(" << pile.size()
        << "-bitvector set, tot(" << tot.cnt() << ", " << tot.size()
        << "), " << ib << ", " << ie << "res(" << res.cnt() << ", "
        << res.size() << ")) ...";
    const uint32_t uncomp = (ibis::bitvector::bitsPerLiteral() == 8 ?
                             tot.size() * 2 / 15 : tot.size() * 4 / 31);
    const uint32_t nobs = pile.size();
    if (ie > nobs)
        ie = nobs;
    if (ib >= ie || ib >= nobs) {
        return;
    }
    horometer timer;
    if (ibis::gVerbose > 4)
        timer.start();

    if (res.size() != tot.size())
        res.set(0, tot.size());
    if ((ie-ib)*2 <= nobs) { // direct evaluation: less than half of bitmaps
        const uint32_t nb = ie - ib;
        if (ie-ib > 24) {
            res.decompress();
        }
        else if (nb > 3) {
            uint32_t tb = 0;
            for (uint32_t i = ib; i < ie; ++ i)
                if (pile[i])
                    tb += pile[i]->bytes();
            if (nb * log(static_cast<double>(nb)) >
                uncomp / static_cast<double>(tb))
                res.decompress();
        }
        for (uint32_t i=ib; i<ie; ++i)
            if (pile[i])
                res |= *(pile[i]);
    }
    else if (ib == 0 && ie >= nobs) { // all bitmaps
        res |= tot;
    }
    else { // more than half (but not all)
        ibis::bitvector tmp;
        while (ib > 0 && pile[ib-1] == 0)
            -- ib;
        if (pile[ib]) {
            tmp.copy(*(pile[ib]));
            if (ib > 0)
                -- ib;
        }
        else {
            while (ie < nobs && pile[ie] == 0)
                ++ ie;
            if (ie < nobs) {
                tmp.copy(*(pile[ie]));
                ++ ie;
            }
            else {
                tmp.set(0, tot.size());
            }
        }
        const uint32_t nb = nobs - ie + ib;
        if (nb > 24) {
            tmp.decompress();
        }
        else if (nb > 3) {
            uint32_t tb = 0;
            for (uint32_t i = 0; i < ib; ++ i)
                if (pile[i])
                    tb += pile[i]->bytes();
            for (uint32_t i = ie; i < nobs; ++ i)
                if (pile[i])
                    tb += pile[i]->bytes();
            if (nb * log(static_cast<double>(nb))
                > uncomp / static_cast<double>(tb))
                tmp.decompress();
        }
        for (uint32_t i = 0; i < ib; ++ i)
            if (pile[i])
                tmp |= *(pile[i]);
        for (uint32_t i = ie; i < nobs; ++ i)
            if (pile[i])
                tmp |= *(pile[i]);
        ibis::bitvector diff(tot);
        diff -= tmp;
        res |= diff;
    }
    if (ibis::gVerbose > 4) {
        timer.stop();
        LOGGER(ibis::gVerbose > 4)
            << "index::sumBits(" << ib << ", " << ie << ") took "
            << timer.CPUTime() << " sec(CPU), " << timer.realTime()
            << " sec(elapsed)";
    }
#if DEBUG+0 > 0 || _DEBUG+0 > 0
    if (ibis::gVerbose > 30 || (1U << ibis::gVerbose) >= res.bytes()) {
        LOGGER(ibis::gVerbose >= 0)
            << "DEBUG -- sumBits(" << ib << ", " << ie << "):" << res;
    }
#endif
} // ibis::index::sumBits

/// Fill the array bases with the values that cover the range [0, card).
/// Assumes at least two components.  Since the base size of each component
/// can not be less two, the maximum number of components could be used is
/// to have each component uses base size two.  If the input argument ncomp
/// is larger than ceiling(log_2(card)), the return array bases shall have
/// ceiling(log_2(card)) elements.
void ibis::index::setBases(array_t<uint32_t>& bases, uint32_t card,
                           uint32_t ncomp) {
    if (card > 7 && ncomp > 2) { // more than two components
        uint32_t b = static_cast<uint32_t>(ceil(pow((double)card, 1.0/ncomp)));
        if (b > 2) {
            bases.resize(ncomp);
            uint32_t tot = 1;
            for (uint32_t i = 0; i < ncomp; ++i) {
                bases[i] = b;
                tot *= b;
            }
            for (uint32_t i = 0; i < ncomp; ++i) {
                if ((tot/b)*(b-1) >= card) {
                    bases[ncomp-i-1] = b - 1;
                    tot /= b;
                    tot *= b - 1;
                }
                else {
                    break; // do not examine the rest of the bases
                }
            }
            // remove the last few bases that are one
            while (ncomp > 0 && bases[ncomp-1] == 1)
                -- ncomp;
            bases.resize(ncomp);
        }
        else { // use base size 2
            bases.reserve(ncomp);
            uint32_t tot = 1;
            for (uint32_t i = 0; i < ncomp && tot < card; ++ i) {
                bases.push_back(2);
                tot <<= 1;
            }
            if (tot < card)
                bases[0] = (uint32_t)(ceil(2.0 * card / tot));
        }
    }
    else if (card > 3 && ncomp > 1) { // assume two components
        uint32_t b =
            static_cast<uint32_t>(ceil(sqrt(static_cast<double>(card))));
        bases.resize(2);
        bases[0] = static_cast<uint32_t>
            (ceil(static_cast<double>(card)/static_cast<double>(b)));
        bases[1] = b;
        double tmp = 0.5 * (bases[0] + bases[1]);
        tmp = tmp*tmp - card;
        tmp = sqrt(tmp);
        tmp = tmp -  0.5 * (bases[1] - bases[0]);
        if (tmp > 0) {
            bases[0] -= static_cast<int>(tmp);
            bases[1] += static_cast<int>(tmp);
        }
        if (bases[1] > bases[0]) {
            b = bases[0];
            bases[0] = bases[1];
            bases[1] = b;
        }
        if (bases[1] < 2) { // should be only one component
            bases.resize(1);
        }
    }
    else { // only one component
        bases.resize(1);
        bases[0] = card;
    }

    if (ibis::gVerbose > 3) {
        ibis::util::logger lg;
        lg() << "index::setBases divides " << card << " distinct values into "
             << bases.size() << " component" << (bases.size()>1?"s":"");
        if (ibis::gVerbose > 4) {
            lg() << " (" << bases[0];
            for (unsigned j = 1; j < bases.size(); ++ j)
                lg() << ", " << bases[j];
            lg() << ')';
        }
    }
} // ibis::index::setBases

/// Decide whether to uncompress the bitmaps.
void ibis::index::optionalUnpack(array_t<bitvector*>& pile,
                                 const char *opt) {
    const size_t nobs = pile.size();
    const char *ptr = 0;
    if (opt != 0)
        ptr = strstr(opt, "<compressing ");
    if (ptr != 0) {
        ptr += 13;
        while (isspace(*ptr))
            ++ ptr;
        if (strnicmp(ptr, "uncompress", 10) == 0) {
            switch (ptr[10]) {
            case 'a':
            case 'A': { // uncompressAll
                for (size_t i = 0; i < nobs; ++i) {
                    if (pile[i])
                        pile[i]->decompress();
                }
                break;
            }
            case 'd':
            case 'D': { // uncompressDense
                double dens = 0.125;
                ptr = strchr(ptr, '>');
                if (ptr != 0) {
                    ++ ptr;
                    dens = strtod(ptr, 0);
                    if (dens <= 0.0)
                        dens = 0.125;
                }
                for (size_t i = 0; i < nobs; ++i) {
                    if (pile[i]) {
#ifdef FASTBIT_RETRY_COMPRESSION
                        pile[i]->compress();
#endif
                        if (pile[i]->cnt() >
                            static_cast<size_t>(dens * pile[i]->size()))
                            pile[i]->decompress();
                    }
                }
                break;
            }
            case 'l':
            case 'L': { // uncompressLarge
                double cr = 0.75;
                ptr = strchr(ptr, '>');
                if (ptr != 0) {
                    ++ ptr;
                    cr = strtod(ptr, 0);
                    if (cr <= 0.0)
                        cr = 0.75;
                }
                for (size_t i = 0; i < nobs; ++i) {
                    if (pile[i]) {
#ifdef FASTBIT_RETRY_COMPRESSION
                        pile[i]->compress();
#endif
                        if (pile[i]->bytes() > static_cast<size_t>
                            (ceil(cr * (pile[i]->size()>>3))))
                            pile[i]->decompress();
                    }
                }
                break;
            }
            default: break; // do nothing
            }
        }
        else if (strnicmp(ptr, "recompress", 10) == 0) {
            for (size_t j = 0; j < nobs; ++ j) {
                if (pile[j] != 0)
                    pile[j]->compress();
            }
        }
    }
    else { // check ibis::gParameters
        const size_t barmin = sizeof(ibis::bitvector) + 12U;
        std::string uA;
        if (col != 0) {
            if (col->partition() != 0) {
                uA = col->partition()->name();
                uA += '.';
            }
            uA += col->name();
            uA += '.';
        }
        uA += "uncompress";
        std::string uL = uA;
        uL += "LargeBitvector";
        uA += "All";
        if (ibis::gParameters().isTrue(uA.c_str())) {
            // decompress the bitvectors as requested
            for (size_t i = 0; i < nobs; ++i) {
                if (pile[i])
                    pile[i]->decompress();
            }
        }
        else if (ibis::gParameters().isTrue(uL.c_str())) {
            // decompress the bitvectors as requested -- decompress those
            // with compression ratios larger than 1/3
            size_t bar0 = nrows / 24;
            if (bar0 < barmin) bar0 = barmin;
            for (size_t i = 0; i < nobs; ++i) {
                if (pile[i]) {
#ifdef FASTBIT_RETRY_COMPRESSION
                    pile[i]->compress();
#endif
                    if (pile[i]->bytes() > bar0)
                        pile[i]->decompress();
                }
            }
        }
        else {
            // decompress very heavy bitvectors, > 8/9
            size_t bar1 = nrows / 9;
            if (bar1 < barmin) bar1 = barmin;
            for (size_t i = 0; i < nobs; ++i) {
                if (pile[i]) {
#ifdef FASTBIT_RETRY_COMPRESSION
                    pile[i]->compress();
#endif
                    if (pile[i]->bytes() > bar1)
                        pile[i]->decompress();
                }
            }
        }
    }
} // ibis::index::optionalUnpack

/// A trivial implementation to indicate the index can not determine any row.
void ibis::index::estimate(const ibis::qDiscreteRange& expr,
                           ibis::bitvector& lower,
                           ibis::bitvector& upper) const {
    LOGGER(ibis::gVerbose > 1)
        << "Note -- using a dummy version of index::estimate to "
        "evaluate a qDiscreteRange on column " << expr.colName();
    if (col && col->partition()) {
        lower.set(0, col->partition()->nRows());
        upper.set(1, col->partition()->nRows());
    }
} // ibis::index::estimate

uint32_t ibis::index::estimate(const ibis::qDiscreteRange& expr) const {
    LOGGER(ibis::gVerbose > 1)
        << " Note -- using a dummy version of index::estimate to "
        "evaluate a qDiscreteRange on column " << expr.colName();
    return (col && col->partition() ? col->partition()->nRows() : 0U);
} // ibis::index::estimate

float ibis::index::undecidable(const ibis::qDiscreteRange& expr,
                               ibis::bitvector& iffy) const {
    LOGGER(ibis::gVerbose > 2)
        << "Note -- using a dummy version of index::undecidable to "
        "evaluate a qDiscreteRange on column " << expr.colName();
    if (col && col->partition())
        iffy.set(1, col->partition()->nRows());
    return 0.5;
} // ibis::index::undecidable

// Provided as dummy implementation so that the derived classes are not
// force to implement these functions.  It indicates that every row is
// undecidable by the index.
void ibis::index::estimate(const ibis::index& idx2,
                           const ibis::deprecatedJoin& expr,
                           ibis::bitvector64& lower,
                           ibis::bitvector64& upper) const {
    if (col == 0) return;
    if (col->partition() == 0) return;

    LOGGER(ibis::gVerbose > 2)
        << "Note -- index::estimate is using a dummy estimate "
        "function to process " << expr;

    ibis::bitvector64::word_t nb = static_cast<ibis::bitvector64::word_t>
        (col->partition()->nRows());
    nb *= nb;
    lower.set(0, nb);
    upper.set(1, nb);
} // ibis::index::estimate

void ibis::index::estimate(const ibis::index& idx2,
                           const ibis::deprecatedJoin& expr,
                           const ibis::bitvector& mask,
                           ibis::bitvector64& lower,
                           ibis::bitvector64& upper) const {
    if (col == 0) return;
    if (col->partition() == 0) return;

    LOGGER(ibis::gVerbose > 2)
        << "Note -- index::estimate is using a dummy estimate "
        "function to process " << expr;

    ibis::bitvector64::word_t nb = static_cast<ibis::bitvector64::word_t>
        (col->partition()->nRows());
    nb *= nb;
    lower.set(0, nb);
    upper.clear();
    ibis::util::outerProduct(mask, mask, upper);
} // ibis::index::estimate

void ibis::index::estimate(const ibis::index& idx2,
                           const ibis::deprecatedJoin& expr,
                           const ibis::bitvector& mask,
                           const ibis::qRange* const range1,
                           const ibis::qRange* const range2,
                           ibis::bitvector64& lower,
                           ibis::bitvector64& upper) const {
    if (col == 0) return;
    if (col->partition() == 0) return;
    LOGGER(ibis::gVerbose > 1)
        << "Note -- index::estimate is using a dummy estimate "
        "function to process " << expr;

    ibis::bitvector64::word_t nb = static_cast<ibis::bitvector64::word_t>
        (col->partition()->nRows());
    nb *= nb;
    lower.set(0, nb);
    upper.clear();
    ibis::util::outerProduct(mask, mask, upper);
} // ibis::index::estimate

void ibis::index::estimate(const ibis::deprecatedJoin& expr,
                           const ibis::bitvector& mask,
                           const ibis::qRange* const range1,
                           const ibis::qRange* const range2,
                           ibis::bitvector64& lower,
                           ibis::bitvector64& upper) const {
    if (col == 0) return;
    if (col->partition() == 0) return;
    LOGGER(ibis::gVerbose > 1)
        << "Note -- index::estimate is using a dummy estimate "
        "function to process %s" << expr;

    ibis::bitvector64::word_t nb = static_cast<ibis::bitvector64::word_t>
        (col->partition()->nRows());
    nb *= nb;
    lower.set(0, nb);
    upper.clear();
    ibis::util::outerProduct(mask, mask, upper);
} // ibis::index::estimate

int64_t ibis::index::estimate(const ibis::index& idx2,
                              const ibis::deprecatedJoin& expr) const {
    if (col == 0) return -1;
    if (col->partition() == 0) return -2;
    LOGGER(ibis::gVerbose > 1)
        << "Note -- index::estimate is using a dummy estimate "
        "function to process %s" << expr;

    int64_t nb = static_cast<ibis::bitvector64::word_t>
        (col->partition()->nRows());
    nb *= nb;
    return nb;
} // ibis::index::estimate

int64_t ibis::index::estimate(const ibis::index& idx2,
                              const ibis::deprecatedJoin& expr,
                              const ibis::bitvector& mask) const {
    if (col == 0) return -1;
    if (col->partition() == 0) return -2;
    LOGGER(ibis::gVerbose > 1)
        << "Note -- index::estimate is using a dummy estimate "
        "function to process %s" << expr;

    int64_t nb = static_cast<ibis::bitvector64::word_t>
        (col->partition()->nRows());
    if (nb > mask.cnt())
        nb = mask.cnt();
    nb *= nb;
    return nb;
} // ibis::index::estimate

int64_t ibis::index::estimate(const ibis::index& idx2,
                              const ibis::deprecatedJoin& expr,
                              const ibis::bitvector& mask,
                              const ibis::qRange* const range1,
                              const ibis::qRange* const range2) const {
    if (col == 0) return -1;
    if (col->partition() == 0) return -2;
    LOGGER(ibis::gVerbose > 1)
        << "Note -- index::estimate is using a dummy estimate "
        "function to process %s" << expr;

    int64_t nb = static_cast<ibis::bitvector64::word_t>
        (col->partition()->nRows());
    if (nb > mask.cnt())
        nb = mask.cnt();
    nb *= nb;
    return nb;
} // ibis::index::estimate

int64_t ibis::index::estimate(const ibis::deprecatedJoin& expr,
                              const ibis::bitvector& mask,
                              const ibis::qRange* const range1,
                              const ibis::qRange* const range2) const {
    if (col == 0) return -1;
    if (col->partition() == 0) return -2;
    LOGGER(ibis::gVerbose > 1)
        << "Note -- index::estimate is using a dummy estimate "
        "function to process %s" << expr;

    int64_t nb = static_cast<ibis::bitvector64::word_t>
        (col->partition()->nRows());
    if (nb > mask.cnt())
        nb = mask.cnt();
    nb *= nb;
    return nb;
} // ibis::index::estimate

// explicit instantiation of templated functions
template void ibis::index::mapValues(const array_t<signed char>&, VMap&);
template void ibis::index::mapValues(const array_t<unsigned char>&, VMap&);
template void ibis::index::mapValues(const array_t<int16_t>&, VMap&);
template void ibis::index::mapValues(const array_t<uint16_t>&, VMap&);
template void ibis::index::mapValues(const array_t<int32_t>&, VMap&);
template void ibis::index::mapValues(const array_t<uint32_t>&, VMap&);
template void ibis::index::mapValues(const array_t<int64_t>&, VMap&);
template void ibis::index::mapValues(const array_t<uint64_t>&, VMap&);
template void ibis::index::mapValues(const array_t<float>&, VMap&);
template void ibis::index::mapValues(const array_t<double>&, VMap&);

template void
ibis::index::mapValues(const array_t<signed char>&, histogram&, uint32_t);
template void
ibis::index::mapValues(const array_t<unsigned char>&, histogram&, uint32_t);
template void
ibis::index::mapValues(const array_t<int16_t>&, histogram&, uint32_t);
template void
ibis::index::mapValues(const array_t<uint16_t>&, histogram&, uint32_t);
template void
ibis::index::mapValues(const array_t<int32_t>&, histogram&, uint32_t);
template void
ibis::index::mapValues(const array_t<uint32_t>&, histogram&, uint32_t);
template void
ibis::index::mapValues(const array_t<int64_t>&, histogram&, uint32_t);
template void
ibis::index::mapValues(const array_t<uint64_t>&, histogram&, uint32_t);
template void
ibis::index::mapValues(const array_t<float>&, histogram&, uint32_t);
template void
ibis::index::mapValues(const array_t<double>&, histogram&, uint32_t);

template void
ibis::index::mapValues(const array_t<signed char>&, array_t<signed char>&,
                       std::vector<uint32_t>&);
template void
ibis::index::mapValues(const array_t<unsigned char>&, array_t<unsigned char>&,
                       std::vector<uint32_t>&);
template void
ibis::index::mapValues(const array_t<int16_t>&, array_t<int16_t>&,
                       std::vector<uint32_t>&);
template void
ibis::index::mapValues(const array_t<uint16_t>&, array_t<uint16_t>&,
                       std::vector<uint32_t>&);
template void
ibis::index::mapValues(const array_t<int32_t>&, array_t<int32_t>&,
                       std::vector<uint32_t>&);
template void
ibis::index::mapValues(const array_t<uint32_t>&, array_t<uint32_t>&,
                       std::vector<uint32_t>&);
template void
ibis::index::mapValues(const array_t<float>&, array_t<float>&,
                       std::vector<uint32_t>&);
template void
ibis::index::mapValues(const array_t<double>&, array_t<double>&,
                       std::vector<uint32_t>&);

template void
ibis::index::mapValues(const array_t<int32_t>&, const array_t<int32_t>&,
                       array_t<int32_t>&, array_t<int32_t>&,
                       std::vector<uint32_t>&);
template void
ibis::index::mapValues(const array_t<int32_t>&, const array_t<uint32_t>&,
                       array_t<int32_t>&, array_t<uint32_t>&,
                       std::vector<uint32_t>&);
template void
ibis::index::mapValues(const array_t<int32_t>&, const array_t<float>&,
                       array_t<int32_t>&, array_t<float>&,
                       std::vector<uint32_t>&);
template void
ibis::index::mapValues(const array_t<int32_t>&, const array_t<double>&,
                       array_t<int32_t>&, array_t<double>&,
                       std::vector<uint32_t>&);
template void
ibis::index::mapValues(const array_t<uint32_t>&, const array_t<int32_t>&,
                       array_t<uint32_t>&, array_t<int32_t>&,
                       std::vector<uint32_t>&);
template void
ibis::index::mapValues(const array_t<uint32_t>&, const array_t<uint32_t>&,
                       array_t<uint32_t>&, array_t<uint32_t>&,
                       std::vector<uint32_t>&);
template void
ibis::index::mapValues(const array_t<uint32_t>&, const array_t<float>&,
                       array_t<uint32_t>&, array_t<float>&,
                       std::vector<uint32_t>&);
template void
ibis::index::mapValues(const array_t<uint32_t>&, const array_t<double>&,
                       array_t<uint32_t>&, array_t<double>&,
                       std::vector<uint32_t>&);
template void
ibis::index::mapValues(const array_t<float>&, const array_t<int32_t>&,
                       array_t<float>&, array_t<int32_t>&,
                       std::vector<uint32_t>&);
template void
ibis::index::mapValues(const array_t<float>&, const array_t<uint32_t>&,
                       array_t<float>&, array_t<uint32_t>&,
                       std::vector<uint32_t>&);
template void
ibis::index::mapValues(const array_t<float>&, const array_t<float>&,
                       array_t<float>&, array_t<float>&,
                       std::vector<uint32_t>&);
template void
ibis::index::mapValues(const array_t<float>&, const array_t<double>&,
                       array_t<float>&, array_t<double>&,
                       std::vector<uint32_t>&);
template void
ibis::index::mapValues(const array_t<double>&, const array_t<int32_t>&,
                       array_t<double>&, array_t<int32_t>&,
                       std::vector<uint32_t>&);
template void
ibis::index::mapValues(const array_t<double>&, const array_t<uint32_t>&,
                       array_t<double>&, array_t<uint32_t>&,
                       std::vector<uint32_t>&);
template void
ibis::index::mapValues(const array_t<double>&, const array_t<float>&,
                       array_t<double>&, array_t<float>&,
                       std::vector<uint32_t>&);
template void
ibis::index::mapValues(const array_t<double>&, const array_t<double>&,
                       array_t<double>&, array_t<double>&,
                       std::vector<uint32_t>&);
