// File: $Id$
// Author: John Wu <John.Wu at acm.org>
//      Lawrence Berkeley National Laboratory
// Copyright (c) 2006-2016 the Regents of the University of California
//
#include "capi.h"
#include "part.h"       // ibis::part, ibis::column, ibis::tablex
#include "query.h"      // ibis::query
#include "bundle.h"     // ibis::query::result
#include "tafel.h"      // a concrete instance of ibis::tablex

#include <time.h>       // clock, clock_gettime
#if defined(__sun) || defined(__linux__) || defined(__HOS_AIX__) || \
    defined(__CYGWIN__) || defined(__APPLE__) || defined(__FreeBSD__)
#   include <limits.h> // CLK_TCK
#   include <sys/time.h> // gettimeofday, timeval
#   include <sys/times.h> // times, struct tms
#   include <sys/resource.h> // getrusage
#   ifndef RUSAGE_SELF
#       define RUSAGE_SELF 0
#   endif
#   ifndef RUSAGE_CHILDREN
#       define RUSAGE_CHILDRED -1
#   endif
#elif defined(CRAY)
#   include <sys/times.h> // times
#elif defined(sgi)
#   include <limits.h> // CLK_TCK
#   define RUSAGE_SELF      0         /* calling process */
#   define RUSAGE_CHILDREN  -1        /* terminated child processes */
#   include <sys/times.h> // times
//#   include <sys/types.h> // struct tms
#   include <sys/time.h> // gettimeofday, getrusage
#   include <sys/resource.h> // getrusage
#elif defined(__MINGW32__)
#   include <limits.h> // CLK_TCK
#   include <sys/time.h> // gettimeofday, timeval
#elif defined(_WIN32)
#   include <windows.h>
#elif defined(VMS)
#   include <unistd.h>
#endif

// FIXME: we should not have to do this but C99 limit macros are not
// defined in C++ unless __STDC_LIMIT_MACROS is defined
#ifndef INT64_MAX
#  define INT64_MAX (9223372036854775807LL)
#endif

extern "C" {
    /// The object underlying the FastBit query handle.
    struct FastBitQuery {
        const ibis::part *t; ///!< The ibis::part this query refers to.
        ibis::query q; ///!< The ibis::query object
        typedef std::map< int, void* > typeValues;
        typedef std::map< const char*, typeValues*, ibis::lessi > valList;
        /// List of values that has been selected and sent to user.
        valList vlist;

        /// For storing null-terminated strings.
        struct NullTerminatedStrings {
            const char **pointers; ///!< The pointer passed to the caller.
            std::vector<std::string> *values; ///!< Actual string values.
        }; // NullTerminatedStrings
    };

    /// A @c FastBitResultSet holds the results of a query in memory and
    /// provides a row-oriented access mechanism for the results.
    ///@note An important limitation is that the current implementation
    /// requires all selected values to be in memory.
    struct FastBitResultSet {
        /// The ibis::query::result object to hold the results in memory.
        ibis::query::result *results;
        /// A place-holder for all the string objects.
        std::vector<std::string> strbuf;
    };
}

/// A list of data partitions known to the C API.  This class is only
/// used in this file for implementing the C API.
class fastbit_part_list {
public:
    fastbit_part_list() {}
    ~fastbit_part_list() {
        (void) clear();
        if (!parts.empty()) {
            LOGGER(ibis::gVerbose >= 0)
                << "Warning -- attempting to delete list of data "
                "partitions while some are still in use";
            throw "can not delete list of data tables while they are in use";
        }
    }

    ibis::part* find(const char* dir);
    void        remove(const char* dir);
    int         clear();

private:
    ibis::partAssoc parts;
}; // class fastbit_part_list

// Can not rely on the automatic deallocation of static variable because it
// require the resources hold by another static variable
// (ibis::fileManager::instance()).  The two static variables may be
// deallocated in unpredictable order.  Change to use a local pointer and
// use init and cleanup function to manage its content.
static fastbit_part_list *_capi_tlist=0;
// The pointer to the in-memory buffer used to store new data records.
static ibis::tablex *_capi_tablex=0;
// The mutex lock for controlling the accesses the two above global variables.
static pthread_mutex_t _capi_mutex = PTHREAD_MUTEX_INITIALIZER;;

/// Clear all data partitions that are not currently use.
///
/// Return the number of data partitions left in the list, i.e., the number
/// of data partitions that are currently in use.
///
/// @note the caller must hold the lock to the shared object.
int fastbit_part_list::clear() {
    int cnt = 0;
    for (ibis::partAssoc::iterator it = parts.begin();
         it != parts.end();) {
        ibis::partAssoc::iterator todel = it;
        ++ it;
        char* name = const_cast<char*>((*todel).first);
        ibis::part* tbl = (*todel).second;
        if (tbl->clear() == 0) {
            delete [] name;
            delete tbl;
            parts.erase(todel);
        }
        else {
            ++ cnt;
        }
    }
    return cnt;
} // fastbit_part_list::clear

/// Locate the named directory in the list of data partitions.  If the
/// named directory is not already in the list, it is added to the list.
///
/// @note the caller must hold the lock to the shared object.
ibis::part* fastbit_part_list::find(const char* dir) {
    LOGGER(ibis::gVerbose > 12)
        << "fastbit_part_list::find(" << dir << ") start with " << parts.size()
        << " known partitions";
    ibis::partAssoc::const_iterator it = parts.find(dir);
    if (it != parts.end()) {
        if (it->second != 0) {
            (void) it->second->updateData();
            LOGGER(ibis::gVerbose > 11)
                << "fastbit_part_list::find(" << dir << ") found the partition "
                "from the named directory, partition name = "
                << it->second->name() << " with nRows = " << it->second->nRows()
                << " and nColumns = " << it->second->nColumns();
            if (it->second->gainReadAccess() == 0) {
                return it->second;
            }
            else {
                LOGGER(ibis::gVerbose > 0)
                    << "Warning -- fastbit_part_list::find(" << dir
                    << ") located a data partition from the given directory, "
                    "but it is not readable at this time";
                return 0;
            }
        }
    }

    ibis::part *tmp;
    try {
        tmp = new ibis::part(dir, static_cast<const char*>(0));
    }
    catch (...) {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- failed to construct a table from directory \""
            << dir << "\"";
        tmp = 0;
    }
    if (tmp != 0 && (tmp->name() == 0 || tmp->nRows() == 0 ||
                     tmp->nColumns() == 0)) {
        LOGGER(ibis::gVerbose > 1)
            << "Warning -- directory " << dir
            << " contains an empty data partition";
        delete tmp;
        tmp = 0;
    }
    if (tmp != 0) {
        if (tmp->gainReadAccess() == 0) {
            parts[ibis::util::strnewdup(dir)] = tmp;
        }
        else {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- failed to aquire read a lock on data from "
                << dir << ", can not use the data";
            delete tmp;
            tmp = 0;
        }
    }
    return tmp;
} // fastbit_part_list::find

/// Delete the named directory from the list.
///
/// @note the caller must hold the lock to the shared object.
void fastbit_part_list::remove(const char* dir) {
    ibis::partAssoc::iterator it = parts.find(dir);
    if (it != parts.end()) {
        delete [] (*it).first;
        delete (*it).second;
        parts.erase(it);
    }
} // fastbit_part_list::remove

static ibis::part* _capi_get_part(const char *dir) {
    ibis::util::mutexLock lock(&_capi_mutex, "_capi_get_part");
    if (_capi_tlist == 0) {
        _capi_tlist = new fastbit_part_list;
        if (_capi_tlist == 0) {
            return 0;
        }
    }

    return _capi_tlist->find(dir);
} // _capi_get_part

extern "C" int fastbit_get_version_number() {
    return ibis::util::getVersionNumber();
} // fastbit_get_version_number

extern "C" const char* fastbit_get_version_string() {
    return ibis::util::getVersionString();
} // fastbit_get_version_string

extern "C" int fastbit_build_indexes(const char *dir, const char *opt) {
    int ierr = -1;
    if (dir == 0 || *dir == 0)
        return ierr;

    try {
        ibis::part *t = _capi_get_part(dir);
        if (t != 0 && t->nRows() > 0 && t->nColumns() > 0) {
            ierr = t->buildIndexes(opt, 1);
        }
        else {
            LOGGER(ibis::gVerbose > 0)
                << "fastbit_build_indexes -- data directory \"" << dir
                << "\" contains no data";
            ierr = 1;
        }
    }
    catch (const std::exception& e) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_build_indexes failed to build indexes in \""
            << dir << "\" due to exception: " << e.what();
        ierr = -2;
    }
    catch (const char* s) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_build_indexes failed to build indexes in \""
            << dir << "\" due to a string exception: " << s;
        ierr = -3;
    }
    catch (...) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_build_indexes failed to build indexes in \""
            << dir << "\" due to a unknown exception";
        ierr = -4;
    }

    return ierr;
} // fastbit_build_indexes

extern "C" int fastbit_purge_indexes(const char *dir) {
    int ierr = -1;
    if (dir == 0 || *dir == 0)
        return ierr;

    try {
        ibis::part *t = _capi_get_part(dir);
        if (t == 0) return ierr;

        t->purgeIndexFiles();
        t->releaseAccess();
        ierr = 0;
    }
    catch (const std::exception& e) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_purge_indexes failed to purge indexes in \""
            << dir << "\" due to exception: " << e.what();
        ierr = -2;
    }
    catch (const char* s) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_purge_indexes failed to purge indexes in \""
            << dir << "\" due to a string exception: " << s;
        ierr = -3;
    }
    catch (...) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_purge_indexes failed to purge indexes in \""
            << dir << "\" due to a unknown exception";
        ierr = -4;
    }

    return ierr;
} // fastbit_purge_indexes

extern "C" int fastbit_build_index(const char *dir, const char *att,
                                   const char *opt) {
    int ierr = -1;
    if (dir == 0 || att == 0 || *dir == 0 || *att == 0)
        return ierr;

    try {
        ibis::part *t = _capi_get_part(dir);
        if (t == 0 || t->nRows() == 0 || t->nColumns() == 0) {
            LOGGER(ibis::gVerbose > 0)
                << "fastbit_build_index -- data directory \"" << dir
                << "\" contains no data";
            ierr = 1;
            return ierr;
        }

        ibis::column *c = t->getColumn(att);
        if (c == 0) {
            LOGGER(ibis::gVerbose > 0)
                << "fastbit_build_index -- can not find column \"" << att
                << "\"in data directory \"" << dir << "\"";
            ierr = -2;
            return ierr;
        }

        c->loadIndex(opt);
        c->unloadIndex();
        if (opt != 0 && *opt != 0) {
            c->indexSpec(opt);
            t->updateMetaData();
        }
        ierr = 0;
    }
    catch (const std::exception& e) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_build_index failed to build index for "
            << att << " in \"" << dir << "\" due to exception: " << e.what();
        ierr = -2;
    }
    catch (const char* s) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_build_index failed to build index for "
            << att << " in \"" << dir << "\" due to a string exception: " << s;
        ierr = -3;
    }
    catch (...) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_build_index failed to build index for "
            << att << " in \"" << dir << "\" due to a unknown exception";
        ierr = -4;
    }

    return ierr;
} // fastbit_build_index

extern "C" int
fastbit_purge_index(const char *dir, const char *att) {
    int ierr = -1;
    if (dir == 0 || att == 0 || *dir == 0 || *att == 0)
        return ierr;

    try {
        ibis::part *t = _capi_get_part(dir);
        if (t == 0 || t->nRows() == 0 || t->nColumns() == 0) {
            LOGGER(ibis::gVerbose > 0)
                << "fastbit_purge_index -- data directory \"" << dir
                << "\" contains no data";
            t->releaseAccess();
            return 1;
        }

        ibis::column *c = t->getColumn(att);
        if (c == 0) {
            ierr = -2;
            return ierr;
        }

        c->purgeIndexFile();
        t->releaseAccess();
        ierr = 0;
    }
    catch (const std::exception& e) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_purge_index failed to purge index for "
            << att << " in \"" << dir << "\" due to exception: " << e.what();
        ierr = -2;
    }
    catch (const char* s) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_purge_index failed to purge index for "
            << att << " in \"" << dir << "\" due to a string exception: " << s;
        ierr = -3;
    }
    catch (...) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_purge_index failed to purge index for "
            << att << " in \"" << dir << "\" due to a unknown exception";
        ierr = -4;
    }

    return ierr;
} // fastbit_purge_index

/// Reorder all the columns in the partition.  Reordering the rows can lead
/// to better index compression and query performance.
///
/// @return It returns zero (0) on success and a negative on failure.
///
/// @warning When this function fails for whatever reason, the data is left
/// in an undetermined state.  Make sure you have a copy of the original
/// data before attempting to reorder the rows.
extern "C" int fastbit_reorder_partition(const char *dir) {
    if (dir == 0 || *dir == 0)
        return -1;

    try {
        ibis::part *t = _capi_get_part(dir);

        if (t != 0) {
            t->releaseAccess(); // release read lock, before reorder
            long ierr = t->reorder();
            if (ierr < 0)
                return ierr;
            else
                return 0;
        }
        else {
            return -2;
        }
    }
    catch (const std::exception& e) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_reorder_partition failed for \""
            << dir << "\" due to exception: " << e.what();
        return -2;
    }
    catch (const char* s) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_reorder_partition failed for \""
            << dir << "\" due to a string exception: " << s;
        return -3;
    }
    catch (...) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_reorder_partition failed for \""
            << dir << "\" due to a unknown exception";
        return -4;
    }
} // fastbit_reorder_partition

/// This is logically equivalent to the SQL statement "SELECT selectClause
/// FROM dataDir WHERE queryConditions."  A blank selectClause is
/// equivalent to "count(*)".  The dataLocation is the directory containing
/// the data and indexes.  This is a required field.  If the where clause
/// is missing, the query is assumed to match all rows following the
/// convention used by SQL.
///
/// @note Must call fastbit_destroy_query on the handle returned to free
/// the resources.
extern "C" FastBitQueryHandle
fastbit_build_query(const char *select, const char *datadir,
                    const char *where) {
    if (datadir == 0 || *datadir == 0)
        return 0;

    try {
        FastBitQueryHandle h = new FastBitQuery;
        h->t = _capi_get_part(datadir);
        if (h->t == 0) {
            LOGGER(ibis::gVerbose >= 0)
                << "Warning -- fastbit_build_query failed to generate table "
                "object from data directory \"" << datadir << "\"";
            delete h;
            h = 0;
            return h;
        }

        int ierr = h->q.setPartition(h->t);
        if (ierr < 0) {
            LOGGER(ibis::gVerbose >= 0)
                << "Warning -- fastbit_build_query failed to assign an "
                << "part (" << h->t->name() << ") object to a query";
            fastbit_destroy_query(h);
            h = 0;
            return h;
        }

        ierr = h->q.setWhereClause(where);
        if (ierr < 0) {
            LOGGER(ibis::gVerbose >= 0)
                << "Warning -- fastbit_build_query failed to assign "
                << "conditions (" << where << ") to a query";
            fastbit_destroy_query(h);
            h = 0;
            return h;
        }

        if (select != 0 && *select != 0) {
            ierr = h->q.setSelectClause(select);
            if (ierr < 0) {
                LOGGER(ibis::gVerbose > 0)
                    << "fastbit_build_query -- failed to assign a select "
                    << "clause (" << select << ") to a query";
            }
        }

        // evaluate the query here
        ierr = h->q.evaluate();
        if (ierr < 0) {
            fastbit_destroy_query(h);
            h = 0;
        }
        return h;
    }
    catch (const std::exception& e) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_build_query failed for \"SELECT "
            << (select && *select ? select : "count(*)")
            << " FROM " << datadir << " WHERE " << where
            << "\" due to exception: " << e.what();
    }
    catch (const char* s) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_build_query failed for \"SELECT "
            << (select && *select ? select : "count(*)")
            << " FROM " << datadir << " WHERE " << where
            << "\" due to a string exception: " << s;
    }
    catch (...) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_build_query failed for \"SELECT "
            << (select && *select ? select : "count(*)")
            << " FROM " << datadir << " WHERE " << where
            << "\" due to a unknown exception";
    }
    return 0;
} // fastbit_build_query

extern "C" int
fastbit_destroy_query(FastBitQueryHandle qhandle) {
    if (qhandle == 0)
        return 0;

    try {
        // first delete the list of values
        for (FastBitQuery::valList::iterator it = qhandle->vlist.begin();
             it != qhandle->vlist.end(); ++ it) {
            FastBitQuery::typeValues& tv = *((*it).second);
            for (FastBitQuery::typeValues::iterator tvit = tv.begin();
                 tvit != tv.end(); ++ tvit) {
                switch ((ibis::TYPE_T) (*tvit).first) {
                case ibis::TEXT: {
                    FastBitQuery::NullTerminatedStrings *nts
                        = static_cast<FastBitQuery::NullTerminatedStrings*>
                        ((*tvit).second);
                    delete [] nts->pointers;
                    delete nts->values;
                    delete nts;
                    break;}
                case ibis::DOUBLE: {
                    ibis::array_t<double> *tmp =
                        static_cast<ibis::array_t<double>*>((*tvit).second);
                    delete tmp;
                    break;}
                case ibis::FLOAT: {
                    ibis::array_t<float> *tmp = 
                        static_cast<ibis::array_t<float>*>((*tvit).second);
                    delete tmp;
                    break;}
                case ibis::OID: {
                    ibis::array_t<ibis::rid_t> *tmp =
                        static_cast<ibis::array_t<ibis::rid_t>*>
                        ((*tvit).second);
                    delete tmp;
                    break;}
                case ibis::BYTE: {
                    ibis::array_t<signed char> *btmp =
                        static_cast<ibis::array_t<signed char>*>
                        ((*tvit).second);
                    delete btmp;
                    break;}
                case ibis::SHORT: {
                    ibis::array_t<int16_t> *stmp =
                        static_cast<ibis::array_t<int16_t>*>((*tvit).second);
                    delete stmp;
                    break;}
                case ibis::INT: {
                    ibis::array_t<int32_t> *tmp =
                        static_cast<ibis::array_t<int32_t>*>((*tvit).second);
                    delete tmp;
                    break;}
                case ibis::LONG: {
                    ibis::array_t<int64_t> *ltmp =
                        static_cast<ibis::array_t<int64_t>*>((*tvit).second);
                    delete ltmp;
                    break;}
                case ibis::UBYTE:  {
                    ibis::array_t<unsigned char> *btmp =
                        static_cast<ibis::array_t<unsigned char>*>
                        ((*tvit).second);
                    delete btmp;
                    break;}
                case ibis::USHORT: {
                    ibis::array_t<uint16_t> *stmp =
                        static_cast<ibis::array_t<uint16_t>*>((*tvit).second);
                    delete stmp;
                    break;}
                case ibis::UINT: {
                    ibis::array_t<uint32_t> *tmp =
                        static_cast<ibis::array_t<uint32_t>*>((*tvit).second);
                    delete tmp;
                    break;}
                case ibis::ULONG: {
                    ibis::array_t<uint64_t> *ltmp =
                        static_cast<ibis::array_t<uint64_t>*>((*tvit).second);
                    delete ltmp;
                    break;}
                default: {
                    LOGGER(ibis::gVerbose >= 0)
                        << "Warning -- column type " << (*tvit).first
                        << " not supported";
                    break;}
                }
            } // tvit
            delete (*it).second;
        } // it

        // release the read lock on the data partition object
        qhandle->t->releaseAccess();
        // finally remove the FastBitQuery object itself
        delete qhandle;
        return 0;
    }
    catch (const std::exception& e) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_destroy_query failed for query "
            << static_cast<const void*>(qhandle) << " due to exception: "
            << e.what();
        return -2;
    }
    catch (const char* s) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_destroy_query failed for query "
            << static_cast<const void*>(qhandle)
            << " due to a string exception: " << s;
        return -3;
    }
    catch (...) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_destroy_query failed for query "
            << static_cast<const void*>(qhandle)
            << " due to a unknown exception";
        return -4;
    }
} // fastbit_destroy_query

/// Return the number of ids placed in ids.  The row ids are limited to be
/// uint32_t so that no more than 4 billion rows could be stored in a
/// single data partition.
///
/// The caller must have allocated enough space.
extern "C" int
fastbit_get_result_row_ids(FastBitQueryHandle qhandle, uint32_t *ids) {
    int ret = -1;
    if (qhandle == 0 || ids == 0) return ret;
    if (qhandle->t == 0)
        return ret;
    try {
        if (qhandle->q.getState() != ibis::query::FULL_EVALUATE)
            qhandle->q.evaluate();

        const ibis::bitvector *bv = qhandle->q.getHitVector();
        ret = 0;
        for (ibis::bitvector::indexSet is = bv->firstIndexSet();
             is.nIndices() > 0;
             ++ is) {
            const ibis::bitvector::word_t *ii = is.indices();
            if (is.isRange()) {
                for (ibis::bitvector::word_t j = *ii;
                     j < ii[1];
                     ++ j) {
                    ids[ret] = j;
                    ++ ret;
                }
            }
            else {
                for (unsigned j = 0; j < is.nIndices(); ++ j) {
                    ids[ret] = ii[j];
                    ++ ret;
                }
            }
        }
    }
    catch (const std::exception& e) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_get_result_row_ids failed for query "
            << static_cast<const void*>(qhandle) << " due to exception: "
            << e.what();
        ret = -2;
    }
    catch (const char* s) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_get_result_row_ids failed for query "
            << static_cast<const void*>(qhandle)
            << " due to a string exception: " << s;
        ret = -3;
    }
    catch (...) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_get_result_row_ids failed for query "
            << static_cast<const void*>(qhandle)
            << " due to a unknown exception";
        ret = -4;
    }
    return ret;
} // fastbit_get_result_row_ids

extern "C" int
fastbit_get_result_rows(FastBitQueryHandle qhandle) {
    int ret = -1;
    if (qhandle == 0) return ret;
    if (qhandle->t == 0)
        return ret;
    try {
        if (qhandle->q.getState() != ibis::query::FULL_EVALUATE)
            qhandle->q.evaluate();

        ret = qhandle->q.getNumHits();
    }
    catch (const std::exception& e) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_get_result_rows failed for query "
            << static_cast<const void*>(qhandle) << " due to exception: "
            << e.what();
        ret = -2;
    }
    catch (const char* s) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_get_result_rows failed for query "
            << static_cast<const void*>(qhandle)
            << " due to a string exception: " << s;
        ret = -3;
    }
    catch (...) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_get_result_rows failed for query "
            << static_cast<const void*>(qhandle)
            << " due to a unknown exception";
        ret = -4;
    }
    return ret;
} // fastbit_get_result_rows

extern "C" int
fastbit_get_result_columns(FastBitQueryHandle qhandle) {
    int ret = -1;
    if (qhandle == 0)
        return ret;
    ret = qhandle->q.components().numTerms();
    return ret;
} // fastbit_get_result_columns

extern "C" const char*
fastbit_get_select_clause(FastBitQueryHandle qhandle) {
    const char* tmp = 0;
    if (qhandle == 0)
        return tmp;
    tmp = qhandle->q.getSelectClause();
    return tmp;
} // fastbit_get_select_clause

extern "C" const char*
fastbit_get_from_clause(FastBitQueryHandle qhandle) {
    const char* tmp = 0;
    if (qhandle == 0)
        return tmp;
    tmp = qhandle->q.partition()->name();
    return tmp;
} // fastbit_get_from_clause

extern "C" const char*
fastbit_get_where_clause(FastBitQueryHandle qhandle) {
    const char* tmp = 0;
    if (qhandle == 0)
        return tmp;
    tmp = qhandle->q.getWhereClause();
    return tmp;
} // fastbit_get_where_clause

extern "C" const signed char *
fastbit_get_qualified_bytes(FastBitQueryHandle qhandle, const char *att) {
    const signed char *ret = 0;
    if (qhandle == 0 || att == 0 || *att == 0) return ret;
    if (qhandle->t == 0 ||
        qhandle->q.getState() != ibis::query::FULL_EVALUATE) {
        LOGGER(ibis::gVerbose > 0)
            << "fastbit_get_qualified_bytes -- invalid query handle ("
            << qhandle << ")";
        return ret;
    }

    try {
        const ibis::column *c = qhandle->t->getColumn(att);
        if (c == 0) {
            LOGGER(ibis::gVerbose > 0)
                << "fastbit_get_qualified_bytes -- can not find a column "
                << "named \"" << att << "\"";
            return ret;
        }
        if (c->type() != ibis::BYTE) {
            LOGGER(ibis::gVerbose > 0)
                << "fastbit_get_qualified_bytes -- column \"" << att
                << "\" has type " << ibis::TYPESTRING[(int)c->type()]
                << ", expect type BYTE";
            return ret;
        }

        FastBitQuery::valList::const_iterator it = qhandle->vlist.find(att);
        if (it != qhandle->vlist.end()) {
            const FastBitQuery::typeValues *tv = (*it).second;
            FastBitQuery::typeValues::const_iterator tvit =
                tv->find((int) ibis::BYTE);
            if (tvit == tv->end())
                tvit = tv->find((int) ibis::UBYTE);
            if (tvit != tv->end()) {
                LOGGER(ibis::gVerbose > 3)
                    << "fastbit_get_qualified_bytes -- found column \""
                    << att << "\" in the existing list";
                ret = static_cast<ibis::array_t<signed char>*>
                    ((*tvit).second)->begin();
            }
        }
        if (ret == 0) {
            ibis::array_t<signed char> *tmp =
                c->selectBytes(*(qhandle->q.getHitVector()));
            if (tmp == 0 || tmp->empty()) {
                delete tmp;
            }
            else {
                ibis::query::writeLock
                    lock(&(qhandle->q), "fastbit_get_qualified_bytes");
                it = qhandle->vlist.find(att);
                if (it == qhandle->vlist.end()) {
                    FastBitQuery::typeValues *tv = new FastBitQuery::typeValues;
                    (*tv)[(int) ibis::BYTE] = static_cast<void*>(tmp);
                    qhandle->vlist[c->name()] = tv;
                    ret = tmp->begin();
                }
                else {
                    FastBitQuery::typeValues *tv = (*it).second;
                    (*tv)[(int) ibis::BYTE] = static_cast<void*>(tmp);
                    ret = tmp->begin();
                }
            }
        }
    }
    catch (const std::exception& e) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_get_qualified_bytes failed to retrieve "
            "values of " << att << " satisfying query "
            << static_cast<const void*>(qhandle) << " due to exception: "
            << e.what();
    ret = 0;
}
catch (const char* s) {
    LOGGER(ibis::gVerbose > 0)
        << "Warning -- fastbit_get_qualified_bytes failed to retrieve "
        "values of " << att << " satisfying query "
        << static_cast<const void*>(qhandle)
        << " due to a string exception: " << s;
    ret = 0;
 }
 catch (...) {
     LOGGER(ibis::gVerbose > 0)
         << "Warning -- fastbit_get_qualified_bytes failed to retrieve "
         "values of " << att << " satisfying query "
         << static_cast<const void*>(qhandle)
         << " due to a unknown exception";
     ret = 0;
 }
return ret;
} // fastbit_get_qualified_bytes

extern "C" const int16_t *
fastbit_get_qualified_shorts(FastBitQueryHandle qhandle, const char *att) {
    const int16_t *ret = 0;
    if (qhandle == 0 || att == 0 || *att == 0) return ret;
    if (qhandle->t == 0 ||
        qhandle->q.getState() != ibis::query::FULL_EVALUATE) {
        LOGGER(ibis::gVerbose > 0)
            << "fastbit_get_qualified_shorts -- invalid query handle ("
            << qhandle << ")";
        return ret;
    }

    try {
        const ibis::column *c = qhandle->t->getColumn(att);
        if (c == 0) {
            LOGGER(ibis::gVerbose > 0)
                << "fastbit_get_qualified_shorts -- can not find a column "
                << "named \"" << att << "\"";
            return ret;
        }
        if (c->type() != ibis::BYTE &&
            c->type() != ibis::UBYTE &&
            c->type() != ibis::SHORT) {
            LOGGER(ibis::gVerbose > 0)
                << "fastbit_get_qualified_shorts -- column \"" << att
                << "\" has type " << ibis::TYPESTRING[(int)c->type()]
                << ", expect type SHORT or BYTE";
            return ret;
        }

        FastBitQuery::valList::const_iterator it = qhandle->vlist.find(att);
        if (it != qhandle->vlist.end()) {
            const FastBitQuery::typeValues *tv = (*it).second;
            FastBitQuery::typeValues::const_iterator tvit =
                tv->find((int) ibis::SHORT);
            if (tvit == tv->end())
                tvit = tv->find((int) ibis::USHORT);
            if (tvit != tv->end()) {
                LOGGER(ibis::gVerbose > 3)
                    << "fastbit_get_qualified_shorts -- found column \""
                    << att << "\" in the existing list";
                ret = static_cast<ibis::array_t<int16_t>*>
                    ((*tvit).second)->begin();
            }
        }
        if (ret == 0) {
            ibis::array_t<int16_t> *tmp =
                c->selectShorts(*(qhandle->q.getHitVector()));
            if (tmp == 0 || tmp->empty()) {
                delete tmp;
            }
            else {
                ibis::query::writeLock
                    lock(&(qhandle->q), "fastbit_get_qualified_shorts");
                it = qhandle->vlist.find(att);
                if (it == qhandle->vlist.end()) {
                    FastBitQuery::typeValues *tv = new FastBitQuery::typeValues;
                    (*tv)[(int) ibis::SHORT] = static_cast<void*>(tmp);
                    qhandle->vlist[c->name()] = tv;
                }
                else {
                    FastBitQuery::typeValues *tv = (*it).second;
                    (*tv)[(int) ibis::SHORT] = static_cast<void*>(tmp);
                }
                ret = tmp->begin();
            }
        }
    }
    catch (const std::exception& e) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_get_qualified_shorts failed to retrieve "
            "values of " << att << " satisfying query "
            << static_cast<const void*>(qhandle) << " due to exception: "
            << e.what();
        ret = 0;
    }
    catch (const char* s) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_get_qualified_shorts failed to retrieve "
            "values of " << att << " satisfying query "
            << static_cast<const void*>(qhandle)
            << " due to a string exception: " << s;
        ret = 0;
    }
    catch (...) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_get_qualified_shorts failed to retrieve "
            "values of " << att << " satisfying query "
            << static_cast<const void*>(qhandle)
            << " due to a unknown exception";
        ret = 0;
    }
    return ret;
} // fastbit_get_qualified_shorts

extern "C" const int32_t *
fastbit_get_qualified_ints(FastBitQueryHandle qhandle, const char *att) {
    const int32_t *ret = 0;
    if (qhandle == 0 || att == 0 || *att == 0) return ret;
    if (qhandle->t == 0 ||
        qhandle->q.getState() != ibis::query::FULL_EVALUATE) {
        LOGGER(ibis::gVerbose > 0)
            << "fastbit_get_qualified_ints -- invalid query handle ("
            << qhandle << ")";
        return ret;
    }

    try {
        const ibis::column *c = qhandle->t->getColumn(att);
        if (c == 0) {
            LOGGER(ibis::gVerbose > 0)
                << "fastbit_get_qualified_ints -- can not find a column "
                << "named \"" << att << "\"";
            return ret;
        }
        if (c->type() != ibis::INT &&
            c->type() != ibis::BYTE &&
            c->type() != ibis::UBYTE &&
            c->type() != ibis::SHORT &&
            c->type() != ibis::USHORT) {
            LOGGER(ibis::gVerbose > 0)
                << "fastbit_get_qualified_ints -- column \"" << att
                << "\" has type " << ibis::TYPESTRING[(int)c->type()]
                << ", expect type INT or shorter integer types";
            return ret;
        }

        FastBitQuery::valList::const_iterator it = qhandle->vlist.find(att);
        if (it != qhandle->vlist.end()) {
            const FastBitQuery::typeValues *tv = (*it).second;
            FastBitQuery::typeValues::const_iterator tvit =
                tv->find((int) ibis::INT);
            if (tvit == tv->end())
                tvit = tv->find((int) ibis::UINT);
            if (tvit != tv->end()) {
                LOGGER(ibis::gVerbose > 3)
                    << "fastbit_get_qualified_ints -- found column \"" << att
                    << "\" in the existing list";
                ret = static_cast<ibis::array_t<int32_t>*>
                    ((*tvit).second)->begin();
            }
        }
        if (ret == 0) {
            ibis::array_t<int32_t> *tmp =
                c->selectInts(*(qhandle->q.getHitVector()));
            if (tmp == 0 || tmp->empty()) {
                delete tmp;
            }
            else {
                LOGGER(ibis::gVerbose > 3)
                    << "fastbit_get_qualified_ints -- retrieved "
                    << tmp->size() << " value" << (tmp->size()>1 ? "s" : "")
                    << " of " << att << " from "
                    << c->partition()->currentDataDir();
                ibis::query::writeLock
                    lock(&(qhandle->q), "fastbit_get_qualified_ints");
                it = qhandle->vlist.find(att);
                if (it == qhandle->vlist.end()) {
                    FastBitQuery::typeValues *tv = new FastBitQuery::typeValues;
                    (*tv)[(int) ibis::INT] = static_cast<void*>(tmp);
                    qhandle->vlist[c->name()] = tv;
                }
                else {
                    FastBitQuery::typeValues *tv = (*it).second;
                    (*tv)[(int) ibis::INT] = static_cast<void*>(tmp);
                }
                ret = tmp->begin();
            }
        }
    }
    catch (const std::exception& e) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_get_qualified_ints failed to retrieve "
            "values of " << att << " satisfying query "
            << static_cast<const void*>(qhandle) << " due to exception: "
            << e.what();
        ret = 0;
    }
    catch (const char* s) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_get_qualified_ints failed to retrieve "
            "values of " << att << " satisfying query "
            << static_cast<const void*>(qhandle)
            << " due to a string exception: " << s;
        ret = 0;
    }
    catch (...) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_get_qualified_ints failed to retrieve "
            "values of " << att << " satisfying query "
            << static_cast<const void*>(qhandle)
            << " due to a unknown exception";
        ret = 0;
    }
    return ret;
} // fastbit_get_qualified_ints

extern "C" const int64_t *
fastbit_get_qualified_longs(FastBitQueryHandle qhandle, const char *att) {
    const int64_t *ret = 0;
    if (qhandle == 0 || att == 0 || *att == 0) return ret;
    if (qhandle->t == 0 ||
        qhandle->q.getState() != ibis::query::FULL_EVALUATE) {
        LOGGER(ibis::gVerbose > 0)
            << "fastbit_get_qualified_longs -- invalid query handle ("
            << qhandle << ")";
        return ret;
    }

    try {
        const ibis::column *c = qhandle->t->getColumn(att);
        if (c == 0) {
            LOGGER(ibis::gVerbose > 0)
                << "fastbit_get_qualified_longs -- can not find a column "
                << "named \"" << att << "\"";
            return ret;
        }
        if (c->type() != ibis::LONG &&
            c->type() != ibis::INT &&
            c->type() != ibis::UINT &&
            c->type() != ibis::BYTE &&
            c->type() != ibis::UBYTE &&
            c->type() != ibis::SHORT &&
            c->type() != ibis::USHORT &&
            c->type() != ibis::TEXT &&
            c->type() != ibis::CATEGORY) {
            LOGGER(ibis::gVerbose > 0)
                << "fastbit_get_qualified_longs -- column \"" << att
                << "\" has type " << ibis::TYPESTRING[(int)c->type()]
                << ", expect type LONG or a compatible type";
            return ret;
        }

        FastBitQuery::valList::const_iterator it = qhandle->vlist.find(att);
        if (it != qhandle->vlist.end()) {
            const FastBitQuery::typeValues *tv = (*it).second;
            FastBitQuery::typeValues::const_iterator tvit =
                tv->find((int) ibis::LONG);
            if (tvit == tv->end())
                tvit = tv->find((int) ibis::ULONG);
            if (tvit != tv->end()) {
                LOGGER(ibis::gVerbose > 3)
                    << "fastbit_get_qualified_longs -- found column \""
                    << att << "\" in the existing list";
                ret = static_cast<ibis::array_t<int64_t>*>
                    ((*tvit).second)->begin();
            }
        }
        if (ret == 0) {
            ibis::array_t<int64_t> *tmp =
                c->selectLongs(*(qhandle->q.getHitVector()));
            if (tmp == 0 || tmp->empty()) {
                delete tmp;
            }
            else {
                ibis::query::writeLock
                    lock(&(qhandle->q), "fastbit_get_qualified_longs");
                it = qhandle->vlist.find(att);
                if (it == qhandle->vlist.end()) {
                    FastBitQuery::typeValues *tv = new FastBitQuery::typeValues;
                    (*tv)[(int) ibis::LONG] = static_cast<void*>(tmp);
                    qhandle->vlist[c->name()] = tv;
                }
                else {
                    FastBitQuery::typeValues *tv = (*it).second;
                    (*tv)[(int) ibis::LONG] = static_cast<void*>(tmp);
                }
                ret = tmp->begin();
            }
        }
    }
    catch (const std::exception& e) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_get_qualified_longs failed to retrieve "
            "values of " << att << " satisfying query "
            << static_cast<const void*>(qhandle) << " due to exception: "
            << e.what();
        ret = 0;
    }
    catch (const char* s) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_get_qualified_longs failed to retrieve "
            "values of " << att << " satisfying query "
            << static_cast<const void*>(qhandle)
            << " due to a string exception: " << s;
        ret = 0;
    }
    catch (...) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_get_qualified_longs failed to retrieve "
            "values of " << att << " satisfying query "
            << static_cast<const void*>(qhandle)
            << " due to a unknown exception";
        ret = 0;
    }
    return ret;
} // fastbit_get_qualified_longs

extern "C" const unsigned char *
fastbit_get_qualified_ubytes(FastBitQueryHandle qhandle, const char *att) {
    const unsigned char *ret = 0;
    if (qhandle == 0 || att == 0 || *att == 0) return ret;
    if (qhandle->t == 0 ||
        qhandle->q.getState() != ibis::query::FULL_EVALUATE) {
        LOGGER(ibis::gVerbose > 0)
            << "fastbit_get_qualified_ubytes -- invalid query handle ("
            << qhandle << ")";
        return ret;
    }

    try {
        const ibis::column *c = qhandle->t->getColumn(att);
        if (c == 0) {
            LOGGER(ibis::gVerbose > 0)
                << "fastbit_get_qualified_ubytes -- can not find a column "
                << "named \"" << att << "\"";
            return ret;
        }
        if (c->type() != ibis::UBYTE) {
            LOGGER(ibis::gVerbose > 0)
                << "fastbit_get_qualified_ubytes -- column \"" << att
                << "\" has type " << ibis::TYPESTRING[(int)c->type()]
                << ", expect type UBYTE";
            return ret;
        }

        FastBitQuery::valList::const_iterator it = qhandle->vlist.find(att);
        if (it != qhandle->vlist.end()) {
            const FastBitQuery::typeValues *tv = (*it).second;
            FastBitQuery::typeValues::const_iterator tvit =
                tv->find((int) ibis::UBYTE);
            if (tvit == tv->end())
                tvit = tv->find((int) ibis::BYTE);
            if (tvit != tv->end()) {
                LOGGER(ibis::gVerbose > 3)
                    << "fastbit_get_qualified_ubytes -- found column \""
                    << att << "\" in the existing list";
                ret = static_cast<ibis::array_t<unsigned char>*>
                    ((*tvit).second)->begin();
            }
        }
        if (ret == 0) {
            ibis::array_t<unsigned char> *tmp =
                c->selectUBytes(*(qhandle->q.getHitVector()));
            if (tmp == 0 || tmp->empty()) {
                delete tmp;
            }
            else {
                ibis::query::writeLock
                    lock(&(qhandle->q), "fastbit_get_qualified_ubytes");
                it = qhandle->vlist.find(att);
                if (it == qhandle->vlist.end()) {
                    FastBitQuery::typeValues *tv = new FastBitQuery::typeValues;
                    (*tv)[(int) ibis::UBYTE] = static_cast<void*>(tmp);
                    qhandle->vlist[c->name()] = tv;
                }
                else {
                    FastBitQuery::typeValues *tv = (*it).second;
                    (*tv)[(int) ibis::UBYTE] = static_cast<void*>(tmp);
                }
                ret = tmp->begin();
            }
        }
    }
    catch (const std::exception& e) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_get_qualified_ubytes failed to retrieve "
            "values of " << att << " satisfying query "
            << static_cast<const void*>(qhandle) << " due to exception: "
            << e.what();
        ret = 0;
    }
    catch (const char* s) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_get_qualified_ubytes failed to retrieve "
            "values of " << att << " satisfying query "
            << static_cast<const void*>(qhandle)
            << " due to a string exception: " << s;
        ret = 0;
    }
    catch (...) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_get_qualified_ubytes failed to retrieve "
            "values of " << att << " satisfying query "
            << static_cast<const void*>(qhandle)
            << " due to a unknown exception";
        ret = 0;
    }
    return ret;
} // fastbit_get_qualified_ubytes

extern "C" const uint16_t *
fastbit_get_qualified_ushorts(FastBitQueryHandle qhandle, const char *att) {
    const uint16_t *ret = 0;
    if (qhandle == 0 || att == 0 || *att == 0) return ret;
    if (qhandle->t == 0 ||
        qhandle->q.getState() != ibis::query::FULL_EVALUATE) {
        LOGGER(ibis::gVerbose > 0)
            << "fastbit_get_qualified_ushorts -- invalid query handle ("
            << qhandle << ")";
        return ret;
    }

    try {
        const ibis::column *c = qhandle->t->getColumn(att);
        if (c == 0) {
            LOGGER(ibis::gVerbose > 0)
                << "fastbit_get_qualified_ushorts -- can not find a column "
                << "named \"" << att << "\"";
            return ret;
        }
        if (c->type() != ibis::USHORT &&
            c->type() != ibis::BYTE &&
            c->type() != ibis::UBYTE) {
            LOGGER(ibis::gVerbose > 0)
                << "fastbit_get_qualified_ushorts -- column \"" << att
                << "\" has type " << ibis::TYPESTRING[(int)c->type()]
                << ", expect type USHORT or BYTE";
            return ret;
        }

        FastBitQuery::valList::const_iterator it = qhandle->vlist.find(att);
        if (it != qhandle->vlist.end()) {
            const FastBitQuery::typeValues *tv = (*it).second;
            FastBitQuery::typeValues::const_iterator tvit =
                tv->find((int) ibis::USHORT);
            if (tvit == tv->end())
                tvit = tv->find((int) ibis::SHORT);
            if (tvit != tv->end()) {
                LOGGER(ibis::gVerbose > 3)
                    << "fastbit_get_qualified_ushorts -- found column \""
                    << att << "\" in the existing list";
                ret = static_cast<ibis::array_t<uint16_t>*>
                    ((*tvit).second)->begin();
            }
        }
        if (ret == 0) {
            ibis::array_t<uint16_t> *tmp =
                c->selectUShorts(*(qhandle->q.getHitVector()));
            if (tmp == 0 || tmp->empty()) {
                delete tmp;
            }
            else {
                ibis::query::writeLock
                    lock(&(qhandle->q), "fastbit_get_qualified_ushorts");
                it = qhandle->vlist.find(att);
                if (it == qhandle->vlist.end()) {
                    FastBitQuery::typeValues *tv = new FastBitQuery::typeValues;
                    (*tv)[(int) ibis::USHORT] = static_cast<void*>(tmp);
                    qhandle->vlist[c->name()] = tv;
                }
                else {
                    FastBitQuery::typeValues *tv = (*it).second;
                    (*tv)[(int) ibis::USHORT] = static_cast<void*>(tmp);
                }
                ret = tmp->begin();
            }
        }
    }
    catch (const std::exception& e) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_get_qualified_ushorts failed to retrieve "
            "values of " << att << " satisfying query "
            << static_cast<const void*>(qhandle) << " due to exception: "
            << e.what();
        ret = 0;
    }
    catch (const char* s) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_get_qualified_ushorts failed to retrieve "
            "values of " << att << " satisfying query "
            << static_cast<const void*>(qhandle)
            << " due to a string exception: " << s;
        ret = 0;
    }
    catch (...) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_get_qualified_ushorts failed to retrieve "
            "values of " << att << " satisfying query "
            << static_cast<const void*>(qhandle)
            << " due to a unknown exception";
        ret = 0;
    }
    return ret;
} // fastbit_get_qualified_ushorts

extern "C" const uint32_t *
fastbit_get_qualified_uints(FastBitQueryHandle qhandle, const char *att) {
    const uint32_t *ret = 0;
    if (qhandle == 0 || att == 0 || *att == 0) return ret;
    if (qhandle->t == 0 ||
        qhandle->q.getState() != ibis::query::FULL_EVALUATE) {
        LOGGER(ibis::gVerbose > 0)
            << "fastbit_get_qualified_uints -- invalid query handle ("
            << qhandle << ")";
        return ret;
    }

    try {
        const ibis::column *c = qhandle->t->getColumn(att);
        if (c == 0) {
            LOGGER(ibis::gVerbose > 0)
                << "fastbit_get_qualified_uints -- can not find a column "
                << "named \"" << att << "\"";
            return ret;
        }
        if (c->type() != ibis::UINT &&
            c->type() != ibis::CATEGORY &&
            c->type() != ibis::USHORT &&
            c->type() != ibis::UBYTE &&
            c->type() != ibis::SHORT &&
            c->type() != ibis::BYTE) {
            LOGGER(ibis::gVerbose > 0)
                << "fastbit_get_qualified_uints -- column \"" << att
                << "\" has type " << ibis::TYPESTRING[(int)c->type()]
                << ", expect type UINT or shoter integer types";
            return ret;
        }

        FastBitQuery::valList::const_iterator it = qhandle->vlist.find(att);
        if (it != qhandle->vlist.end()) {
            const FastBitQuery::typeValues *tv = (*it).second;
            FastBitQuery::typeValues::const_iterator tvit =
                tv->find((int) ibis::UINT);
            if (tvit == tv->end())
                tvit = tv->find((int) ibis::INT);
            if (tvit != tv->end()) {
                LOGGER(ibis::gVerbose > 3)
                    << "fastbit_get_qualified_uints -- found column \""
                    << att << "\" in the existing list";
                ret = static_cast<ibis::array_t<uint32_t>*>
                    ((*tvit).second)->begin();
            }
        }
        if (ret == 0) {
            ibis::array_t<uint32_t> *tmp =
                c->selectUInts(*(qhandle->q.getHitVector()));
            if (tmp == 0 || tmp->empty()) {
                delete tmp;
            }
            else {
                ibis::query::writeLock
                    lock(&(qhandle->q), "fastbit_get_qualified_uints");
                it = qhandle->vlist.find(att);
                if (it == qhandle->vlist.end()) {
                    FastBitQuery::typeValues *tv = new FastBitQuery::typeValues;
                    (*tv)[(int) ibis::UINT] = static_cast<void*>(tmp);
                    qhandle->vlist[c->name()] = tv;
                }
                else {
                    FastBitQuery::typeValues *tv = (*it).second;
                    (*tv)[(int) ibis::UINT] = static_cast<void*>(tmp);
                }
                ret = tmp->begin();
            }
        }
    }
    catch (const std::exception& e) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_get_qualified_uints failed to retrieve "
            "values of " << att << " satisfying query "
            << static_cast<const void*>(qhandle) << " due to exception: "
            << e.what();
        ret = 0;
    }
    catch (const char* s) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_get_qualified_uints failed to retrieve "
            "values of " << att << " satisfying query "
            << static_cast<const void*>(qhandle)
            << " due to a string exception: " << s;
        ret = 0;
    }
    catch (...) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_get_qualified_uints failed to retrieve "
            "values of " << att << " satisfying query "
            << static_cast<const void*>(qhandle)
            << " due to a unknown exception";
        ret = 0;
    }
    return ret;
} // fastbit_get_qualified_uints

extern "C" const uint64_t *
fastbit_get_qualified_ulongs(FastBitQueryHandle qhandle, const char *att) {
    const uint64_t *ret = 0;
    if (qhandle == 0 || att == 0 || *att == 0) return ret;
    if (qhandle->t == 0 ||
        qhandle->q.getState() != ibis::query::FULL_EVALUATE) {
        LOGGER(ibis::gVerbose > 0)
            << "fastbit_get_qualified_ulongs -- invalid query handle ("
            << qhandle << ")";
        return ret;
    }

    try {
        const ibis::column *c = qhandle->t->getColumn(att);
        if (c == 0) {
            LOGGER(ibis::gVerbose > 0)
                << "fastbit_get_qualified_ulongs -- can not find a column "
                << "named \"" << att << "\"";
            return ret;
        }
        if (c->type() != ibis::ULONG &&
            c->type() != ibis::UINT &&
            c->type() != ibis::USHORT &&
            c->type() != ibis::UBYTE &&
            c->type() != ibis::INT &&
            c->type() != ibis::SHORT &&
            c->type() != ibis::BYTE) {
            LOGGER(ibis::gVerbose > 0)
                << "fastbit_get_qualified_ulongs -- column \"" << att
                << "\" has type " << ibis::TYPESTRING[(int)c->type()]
                << ", expect type ULONG or shorter integer types";
            return ret;
        }

        FastBitQuery::valList::const_iterator it = qhandle->vlist.find(att);
        if (it != qhandle->vlist.end()) {
            const FastBitQuery::typeValues *tv = (*it).second;
            FastBitQuery::typeValues::const_iterator tvit =
                tv->find((int) ibis::ULONG);
            if (tvit == tv->end())
                tvit = tv->find((int) ibis::LONG);
            if (tvit != tv->end()) {
                LOGGER(ibis::gVerbose > 3)
                    << "fastbit_get_qualified_ulongs -- found column \""
                    << att << "\" in the existing list";
                ret = static_cast<ibis::array_t<uint64_t>*>
                    ((*tvit).second)->begin();
            }
        }
        if (ret == 0) {
            ibis::array_t<uint64_t> *tmp =
                c->selectULongs(*(qhandle->q.getHitVector()));
            if (tmp == 0 || tmp->empty()) {
                delete tmp;
            }
            else {
                ibis::query::writeLock
                    lock(&(qhandle->q), "fastbit_get_qualified_ulongs");
                it = qhandle->vlist.find(att);
                if (it == qhandle->vlist.end()) {
                    FastBitQuery::typeValues *tv = new FastBitQuery::typeValues;
                    (*tv)[(int) ibis::ULONG] = static_cast<void*>(tmp);
                    qhandle->vlist[c->name()] = tv;
                }
                else {
                    FastBitQuery::typeValues *tv = (*it).second;
                    (*tv)[(int) ibis::ULONG] = static_cast<void*>(tmp);
                }
                ret = tmp->begin();
            }
        }
    }
    catch (const std::exception& e) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_get_qualified_ulongs failed to retrieve "
            "values of " << att << " satisfying query "
            << static_cast<const void*>(qhandle) << " due to exception: "
            << e.what();
        ret = 0;
    }
    catch (const char* s) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_get_qualified_ulongs failed to retrieve "
            "values of " << att << " satisfying query "
            << static_cast<const void*>(qhandle)
            << " due to a string exception: " << s;
        ret = 0;
    }
    catch (...) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_get_qualified_ulongs failed to retrieve "
            "values of " << att << " satisfying query "
            << static_cast<const void*>(qhandle)
            << " due to a unknown exception";
        ret = 0;
    }
    return ret;
} // fastbit_get_qualified_ulongs

extern "C" const float *
fastbit_get_qualified_floats(FastBitQueryHandle qhandle, const char *att) {
    const float *ret = 0;
    if (qhandle == 0 || att == 0 || *att == 0) return ret;
    if (qhandle->t == 0 ||
        qhandle->q.getState() != ibis::query::FULL_EVALUATE) {
        LOGGER(ibis::gVerbose > 0)
            << "fastbit_get_qualified_floats -- invalid query handle ("
            << qhandle << ")";
        return ret;
    }

    try {
        const ibis::column *c = qhandle->t->getColumn(att);
        if (c == 0) {
            LOGGER(ibis::gVerbose > 0)
                << "fastbit_get_qualified_floats -- can not find a column "
                << "named \"" << att << "\"";
            return ret;
        }
        if (c->type() != ibis::FLOAT) {
            LOGGER(ibis::gVerbose > 0)
                << "fastbit_get_qualified_floats -- column \"" << att
                << "\" has type " << ibis::TYPESTRING[(int)c->type()]
                << ", expect type FLOAT or short integer types";
            return ret;
        }

        FastBitQuery::valList::const_iterator it = qhandle->vlist.find(att);
        if (it != qhandle->vlist.end()) {
            const FastBitQuery::typeValues *tv = (*it).second;
            FastBitQuery::typeValues::const_iterator tvit =
                tv->find((int) ibis::FLOAT);
            if (tvit != tv->end()) {
                LOGGER(ibis::gVerbose > 3)
                    << "fastbit_get_qualified_floats -- found column \""
                    << att << "\" in the existing list";
                ret = static_cast<ibis::array_t<float>*>
                    ((*tvit).second)->begin();
            }
        }
        if (ret == 0) {// need to read the files
            ibis::array_t<float> *tmp =
                c->selectFloats(*(qhandle->q.getHitVector()));
            if (tmp == 0 || tmp->empty()) {
                delete tmp;
            }
            else {
                ibis::query::writeLock
                    lock(&(qhandle->q), "fastbit_get_qualified_floats");
                it = qhandle->vlist.find(att);
                if (it == qhandle->vlist.end()) {
                    FastBitQuery::typeValues *tv = new FastBitQuery::typeValues;
                    (*tv)[(int) ibis::FLOAT] = static_cast<void*>(tmp);
                    qhandle->vlist[c->name()] = tv;
                }
                else {
                    FastBitQuery::typeValues *tv = (*it).second;
                    (*tv)[(int) ibis::FLOAT] = static_cast<void*>(tmp);
                }
                ret = tmp->begin();
            }
        }
    }
    catch (const std::exception& e) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_get_qualified_floats failed to retrieve "
            "values of " << att << " satisfying query "
            << static_cast<const void*>(qhandle) << " due to exception: "
            << e.what();
        ret = 0;
    }
    catch (const char* s) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_get_qualified_floats failed to retrieve "
            "values of " << att << " satisfying query "
            << static_cast<const void*>(qhandle)
            << " due to a string exception: " << s;
        ret = 0;
    }
    catch (...) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_get_qualified_floats failed to retrieve "
            "values of " << att << " satisfying query "
            << static_cast<const void*>(qhandle)
            << " due to a unknown exception";
        ret = 0;
    }
    return ret;
} // fastbit_get_qualified_floats

extern "C" const double *
fastbit_get_qualified_doubles(FastBitQueryHandle qhandle, const char *att) {
    const double *ret = 0;
    if (qhandle == 0 || att == 0 || *att == 0) return ret;
    if (qhandle->t == 0 ||
        qhandle->q.getState() != ibis::query::FULL_EVALUATE) {
        LOGGER(ibis::gVerbose > 0)
            << "fastbit_get_qualified_doubles -- invalid query handle ("
            << qhandle << ")";
        return ret;
    }

    try {
        const ibis::column *c = qhandle->t->getColumn(att);
        if (c == 0) {
            LOGGER(ibis::gVerbose > 0)
                << "fastbit_get_qualified_doubles -- can not find a column "
                << "named \"" << att << "\"";
            return ret;
        }
        if (c->type() == ibis::CATEGORY || c->type() == ibis::TEXT) {
            LOGGER(ibis::gVerbose > 0)
                << "fastbit_get_qualified_doubles -- column \"" << att
                << "\" has type " << ibis::TYPESTRING[(int)c->type()]
                << ", expect type DOUBLE or shorter numerical values";
            return ret;
        }

        FastBitQuery::valList::const_iterator it = qhandle->vlist.find(att);
        if (it != qhandle->vlist.end()) {
            const FastBitQuery::typeValues *tv = (*it).second;
            FastBitQuery::typeValues::const_iterator tvit =
                tv->find((int) ibis::DOUBLE);
            if (tvit != tv->end()) {
                LOGGER(ibis::gVerbose > 3)
                    << "fastbit_get_qualified_doubles -- found column \""
                    << att << "\" in the existing list";
                ret = static_cast<ibis::array_t<double>*>
                    ((*tvit).second)->begin();
            }
        }
        if (ret == 0) { // need to read the data file
            ibis::array_t<double> *tmp =
                c->selectDoubles(*(qhandle->q.getHitVector()));
            if (tmp == 0 || tmp->empty()) {
                delete tmp;
            }
            else {
                ibis::query::writeLock
                    lock(&(qhandle->q), "fastbit_get_qualified_doubles");
                it = qhandle->vlist.find(att);
                if (it == qhandle->vlist.end()) {
                    FastBitQuery::typeValues *tv = new FastBitQuery::typeValues;
                    (*tv)[(int) ibis::DOUBLE] = static_cast<void*>(tmp);
                    qhandle->vlist[c->name()] = tv;
                }
                else {
                    FastBitQuery::typeValues *tv = (*it).second;
                    (*tv)[(int) ibis::DOUBLE] = static_cast<void*>(tmp);
                }
                ret = tmp->begin();
            }
        }
    }
    catch (const std::exception& e) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_get_qualified_doubles failed to retrieve "
            "values of " << att << " satisfying query "
            << static_cast<const void*>(qhandle) << " due to exception: "
            << e.what();
        ret = 0;
    }
    catch (const char* s) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_get_qualified_doubles failed to retrieve "
            "values of " << att << " satisfying query "
            << static_cast<const void*>(qhandle)
            << " due to a string exception: " << s;
        ret = 0;
    }
    catch (...) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_get_qualified_doubles failed to retrieve "
            "values of " << att << " satisfying query "
            << static_cast<const void*>(qhandle)
            << " due to a unknown exception";
        ret = 0;
    }
    return ret;
} // fastbit_get_qualified_doubles

extern "C" const char **
fastbit_get_qualified_strings(FastBitQueryHandle qhandle, const char *att) {
    const char **ret = 0;
    if (qhandle == 0 || att == 0 || *att == 0) return ret;
    if (qhandle->t == 0 ||
        qhandle->q.getState() != ibis::query::FULL_EVALUATE) {
        LOGGER(ibis::gVerbose > 0)
            << "fastbit_get_qualified_strings -- invalid query handle ("
            << qhandle << ")";
        return ret;
    }

    try {
        const ibis::column *c = qhandle->t->getColumn(att);
        if (c == 0) {
            LOGGER(ibis::gVerbose > 0)
                << "fastbit_get_qualified_strings -- can not find a column "
                << "named \"" << att << "\"";
            return ret;
        }

        FastBitQuery::valList::const_iterator it = qhandle->vlist.find(att);
        if (it != qhandle->vlist.end()) {
            const FastBitQuery::typeValues *tv = (*it).second;
            FastBitQuery::typeValues::const_iterator tvit =
                tv->find((int) ibis::TEXT);
            if (tvit != tv->end()) {
                LOGGER(ibis::gVerbose > 3)
                    << "fastbit_get_qualified_strings -- found column \""
                    << att << "\" in the existing list";
                ret = static_cast<const FastBitQuery::NullTerminatedStrings*>
                    ((*tvit).second)->pointers;
            }
        }
        if (ret == 0) { // read data file to extract the values
            std::vector<std::string> *tmp =
                c->selectStrings(*(qhandle->q.getHitVector()));
            if (tmp == 0 || tmp->empty()) {
                delete tmp;
            }
            else {
                FastBitQuery::NullTerminatedStrings *nts
                    = new FastBitQuery::NullTerminatedStrings;
                nts->values = tmp;
                nts->pointers = new const char*[tmp->size()];
                for (size_t ii = 0; ii < tmp->size(); ++ ii)
                    nts->pointers[ii] = (*tmp)[ii].c_str();

                ibis::query::writeLock
                    lock(&(qhandle->q), "fastbit_get_qualified_strings");
                it = qhandle->vlist.find(att);
                if (it == qhandle->vlist.end()) {
                    FastBitQuery::typeValues *tv = new FastBitQuery::typeValues;
                    (*tv)[(int) ibis::TEXT] = static_cast<void*>(nts);
                    qhandle->vlist[c->name()] = tv;
                }
                else {
                    FastBitQuery::typeValues *tv = (*it).second;
                    (*tv)[(int) ibis::TEXT] = static_cast<void*>(nts);
                }
                ret = nts->pointers;
            }
        }
    }
    catch (const std::exception& e) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_get_qualified_strings failed to retrieve "
            "values of " << att << " satisfying query "
            << static_cast<const void*>(qhandle) << " due to exception: "
            << e.what();
        ret = 0;
    }
    catch (const char* s) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_get_qualified_strings failed to retrieve "
            "values of " << att << " satisfying query "
            << static_cast<const void*>(qhandle)
            << " due to a string exception: " << s;
        ret = 0;
    }
    catch (...) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_get_qualified_strings failed to retrieve "
            "values of " << att << " satisfying query "
            << static_cast<const void*>(qhandle)
            << " due to a unknown exception";
        ret = 0;
    }
    return ret;
} // fastbit_get_qualified_strings

/// This initialization function may optionally read a configuration file.
/// May pass in a nil pointer as rcfile if one is expected to use use the
/// default configuartion files listed in the documentation of
/// ibis::resources::read.  One may call this function multiple times to
/// read multiple configuration files to modify the parameters.
extern "C" void fastbit_init(const char *rcfile) {
#if defined(DEBUG) || defined(_DEBUG)
    if (ibis::gVerbose == 0) {
#if DEBUG + 0 > 10 || _DEBUG + 0 > 10
        ibis::gVerbose = INT_MAX;
#elif DEBUG + 0 > 0
        ibis::gVerbose += 7 * DEBUG;
#elif _DEBUG + 0 > 0
        ibis::gVerbose += 5 * _DEBUG;
#else
        ibis::gVerbose += 3;
#endif
    }
#endif
    if (rcfile && *rcfile)
        ibis::gParameters().read(rcfile);
    ibis::util::mutexLock lock(&_capi_mutex, "fastbit_init");
    if (_capi_tlist == 0)
        _capi_tlist = new fastbit_part_list;
} // fastbit_init

/// This function releases the list of data partitions.  It is expected to
/// be te last function to be called by the user.  Since there is no
/// centralized list of query objects, the user is responsible for freeing
/// the resources held by each query object.
extern "C" void fastbit_cleanup(void) {
    ibis::util::mutexLock lock(&_capi_mutex, "fastbit_cleanup");
    if (_capi_tlist != 0) {
        int ierr = _capi_tlist->clear();
        if (ierr < 1) {
            delete _capi_tlist;
            // assign it to 0 so that we can call fastbit_init again
            _capi_tlist = 0;
            ibis::fileManager::instance().clear();
            ibis::util::closeLogFile();
        }
        else {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- fastbit_cleanup found " << ierr
                << " data director" << (ierr>1?"ies are":"y is")
                << " still in use, will leave "<< (ierr>1?"them":"it")
                << " in meory";
        }
    }
    if (_capi_tablex != 0) {
        LOGGER(ibis::gVerbose > 0)
            << "fastbit_cleanup is removing a non-empty data buffer "
            "for new records";
        delete _capi_tablex;
        _capi_tablex = 0;
    }
} // fastbit_cleanup

/// @return Returns the old verboseness level.
///
/// @note This function is not thread-safe.  It is possible for multiple
/// threads to assign different values at the same time, however, we
/// anticipate that getting the log message wrong by a few notches will not
/// be of great harm.
extern "C" int fastbit_set_verbose_level(int v) {
    int ret = ibis::gVerbose;
    ibis::gVerbose = v;
    return ret;
} // fastbit_set_verbose_level

extern "C" int fastbit_get_verbose_level(void) {
    return ibis::gVerbose;
} // fastbit_get_verbose_level

/// @return Return 0 to indicate success, a negative value to indicate
/// error.
extern "C" int fastbit_set_logfile(const char* filename) {
    try {
        return ibis::util::setLogFileName(filename);
    }
    catch (const std::exception& e) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_set_logfile failed to redirect logs to \""
            << filename << "\" due to exception: " << e.what();
        return -2;
    }
    catch (const char* s) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_set_logfile failed to redirect logs to \""
            << filename << "\" due to a string exception: " << s;
        return -3;
    }
    catch (...) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_set_logfile failed to redirect logs to \""
            << filename << "\" due to a unknown exception";
        return -4;
    }
} // fastbit_set_logfile

/// @return Return the current log file name.  A blank string or a null
/// pointer indicates the standard output.
extern "C" const char* fastbit_get_logfile() {
    return ibis::util::getLogFileName();
} // fastbit_get_logfile

extern "C" FILE* fastbit_get_logfilepointer() {
    return ibis::util::getLogFile();
} // fastbit_get_logfilepointer

/// Read the system's wallclock timer.  It tries to use clock_gettime if it
/// is available, otherwise it falls back to gettimeofday and clock.
extern "C" double fastbit_read_clock() {
#if defined(CLOCK_MONOTONIC) && !defined(__CYGWIN__)
    struct timespec tb;
    if (0 == clock_gettime(CLOCK_MONOTONIC, &tb)) {
        return static_cast<double>(tb.tv_sec) + (1e-9 * tb.tv_nsec);
    }
    else {
        struct timeval cpt;
        gettimeofday(&cpt, 0);
        return static_cast<double>(cpt.tv_sec) + (1e-6 * cpt.tv_usec);
    }
#elif defined(HAVE_GETTIMEOFDAY) || defined(__unix__) || defined(CRAY) || \
    defined(__linux__) || defined(__HOS_AIX__) || defined(__APPLE__) || \
    defined(__FreeBSD__)
    struct timeval cpt;
    gettimeofday(&cpt, 0);
    return static_cast<double>(cpt.tv_sec) + (1e-6 * cpt.tv_usec);
#elif defined(_WIN32) && defined(_MSC_VER)
    double ret = 0.0;
    if (countPeriod != 0) {
        LARGE_INTEGER cnt;
        if (QueryPerformanceCounter(&cnt)) {
            ret = countPeriod * cnt.QuadPart;
        }
    }
    if (ret == 0.0) { // fallback option -- use GetSystemTime
        union {
            FILETIME ftFileTime;
            __int64  ftInt64;
        } ftRealTime;
        GetSystemTimeAsFileTime(&ftRealTime.ftFileTime);
        ret = (double) ftRealTime.ftInt64 * 1e-7;
    }
    return ret;
#elif defined(VMS)
    return (double) clock() * 0.001;
#else
    return (double) clock() / CLOCKS_PER_SEC;
#endif
} //  fastbit_read_clock

/// Create a @c FastBitResultSetHandle from a query object.
extern "C" FastBitResultSetHandle
fastbit_build_result_set(FastBitQueryHandle qhandle) {
    FastBitResultSetHandle ret = 0; // a new null handle
    if (qhandle == 0)
        return ret;
    if (qhandle->q.getSelectClause() == 0)
        return ret;
    if (qhandle->q.components().empty())
        return ret;
    if (qhandle->t == 0 ||
        qhandle->q.getState() != ibis::query::FULL_EVALUATE) {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- fastbit_build_result_set -- invalid query "
            << "handle (" << qhandle << ")";
        return ret;
    }

    try {
        ret = new FastBitResultSet;
        ret->results = new ibis::query::result(qhandle->q);
        ret->strbuf.resize(qhandle->q.components().aggSize());
    }
    catch (const std::exception& e) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_build_result_set failed to retrieve "
            "values for query " << static_cast<const void*>(qhandle)
            << " due to exception: " << e.what();
    ret = 0;
}
catch (const char* s) {
    LOGGER(ibis::gVerbose > 0)
        << "Warning -- fastbit_build_result_set failed to retrieve values "
        "for query " << static_cast<const void*>(qhandle)
        << " due to a string exception: " << s;
    ret = 0;
 }
 catch (...) {
     LOGGER(ibis::gVerbose > 0)
         << "Warning -- fastbit_build_result_set failed to retrieve values "
         "for query " << static_cast<const void*>(qhandle)
         << " due to a unknown exception";
     ret = 0;
 }
return ret;
} // fastbit_build_result_set

extern "C" int
fastbit_destroy_result_set(FastBitResultSetHandle rset) {
    delete rset->results;
    delete rset;
    return 0;
} // fastbit_destroy_result_set

extern "C" int
fastbit_result_set_next(FastBitResultSetHandle rset) {
    int ierr = -1;
    try {
        if (rset == 0)
            ierr = -2;
        else if (rset->results->next())
            ierr = 0;
    }
    catch (const std::exception& e) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_result_set_next failed to prepare the "
            "next row due to exception: " << e.what();
        ierr = -3;
    }
    catch (const char* s) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_result_set_next failed to prepare the "
            "next row due to a string exception: " << s;
        ierr = -4;
    }
    catch (...) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_result_set_next failed to prepare the "
            "next row due to a unknown exception";
        ierr = -5;
    }

    return ierr;
} // fastbit_result_set_next

extern "C" int
fastbit_result_set_next_bundle(FastBitResultSetHandle rset) {
    int ierr = -1;
    try {
        if (rset == 0)
            ierr = -2;
        else if (rset->results->nextBundle())
            ierr = 0;
    }
    catch (const std::exception& e) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_result_set_next_bundle failed to prepare "
            "the next row due to exception: " << e.what();
        ierr = -3;
    }
    catch (const char* s) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_result_set_next_bundle failed to prepare "
            "the next row due to a string exception: " << s;
        ierr = -4;
    }
    catch (...) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_result_set_next_bundle failed to prepare "
            "the next row due to a unknown exception";
        ierr = -5;
    }

    return ierr;
} // fastbit_result_set_next_bundle

extern "C" int
fastbit_result_set_get_int(FastBitResultSetHandle rset,
                           const char *cname) {
    int ret = INT_MAX;
    try {
        if (rset != 0 && cname != 0 && *cname != 0)
            ret = rset->results->getInt(cname);
    }
    catch (const std::exception& e) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_result_set_get_int failed to retrieve "
            << "value of " << cname << " due to exception: " << e.what();
        ret = INT_MAX;
    }
    catch (const char* s) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_result_set_get_int failed to retrieve "
            "value of " << cname << " due to a string exception: " << s;
        ret = INT_MAX;
    }
    catch (...) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_result_set_get_int failed toretrieve "
            "value of " << cname << " due to a unknown exception";
        ret = INT_MAX;
    }
    return ret;
} // fastbit_result_set_get_int

extern "C" unsigned
fastbit_result_set_get_unsigned(FastBitResultSetHandle rset,
                                const char *cname) {
    unsigned ret = UINT_MAX;
    try {
        if (rset != 0 && cname != 0 && *cname != 0)
            ret = rset->results->getUInt(cname);
    }
    catch (const std::exception& e) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_result_set_get_unsigned failed to retrieve "
            "value of " << cname << " due to exception: " << e.what();
        ret = UINT_MAX;
    }
    catch (const char* s) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_result_set_get_unsigned failed to retrieve "
            "value of " << cname << " due to a string exception: " << s;
        ret = UINT_MAX;
    }
    catch (...) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_result_set_get_unsigned failed to retrieve "
            "value of " << cname << " due to a unknown exception";
        ret = UINT_MAX;
    }
    return ret;
} // fastbit_result_set_get_unsigned

extern "C" int64_t
fastbit_result_set_get_long(FastBitResultSetHandle rset,
                            const char *cname) {
    int64_t ret = INT64_MAX;
    try {
        if (rset != 0 && cname != 0 && *cname != 0)
            ret = rset->results->getLong(cname);
    }
    catch (const std::exception& e) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_result_set_get_long failed to retrieve "
            "value of " << cname << " due to exception: " << e.what();
        ret = INT64_MAX;
    }
    catch (const char* s) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_result_set_get_long failed to retrieve "
            "value of " << cname << " due to a string exception: " << s;
        ret = INT64_MAX;
    }
    catch (...) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_result_set_get_long failed to retrieve "
            "value of " << cname << " due to a unknown exception";
        ret = INT64_MAX;
    }
    return ret;
} // fastbit_result_set_get_long

extern "C" float
fastbit_result_set_get_float(FastBitResultSetHandle rset,
                             const char *cname) {
    float ret = FLT_MAX;
    try {
        if (rset != 0 && cname != 0 && *cname != 0)
            ret = rset->results->getFloat(cname);
    }
    catch (const std::exception& e) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_result_set_get_float failed to retrieve "
            "value of " << cname << " due to exception: " << e.what();
        ret = FLT_MAX;
    }
    catch (const char* s) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_result_set_get_float failed to retrieve "
            "value of " << cname << " due to a string exception: " << s;
        ret = FLT_MAX;
    }
    catch (...) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_result_set_get_float failed to retrieve "
            "value of " << cname << " due to a unknown exception";
        ret = FLT_MAX;
    }
    return ret;
} // fastbit_result_set_get_float

extern "C" double
fastbit_result_set_get_double(FastBitResultSetHandle rset,
                              const char *cname) {
    double ret = DBL_MAX;
    try {
        if (rset != 0 && cname != 0 && *cname != 0)
            ret = rset->results->getDouble(cname);
    }
    catch (const std::exception& e) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_result_set_get_double failed to retrieve "
            "value of " << cname << " due to exception: " << e.what();
        ret = DBL_MAX;
    }
    catch (const char* s) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_result_set_get_double failed to retrieve "
            "value of " << cname << " due to a string exception: " << s;
        ret = DBL_MAX;
    }
    catch (...) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_result_set_get_double failed to retrieve "
            "value of " << cname << " due to a unknown exception";
        ret = DBL_MAX;
    }
    return ret;
} // fastbit_result_set_get_double

extern "C" const char*
fastbit_result_set_get_string(FastBitResultSetHandle rset,
                              const char *cname) {
    if (rset == 0 || cname != 0 || *cname != 0)
        return static_cast<const char*>(0);

    try {
        unsigned pos = rset->results->colPosition(cname);
        if (pos >= rset->strbuf.size())
            return static_cast<const char*>(0);

        std::string tmp = rset->results->getString(pos);
        rset->strbuf[pos].swap(tmp);
        return rset->strbuf[pos].c_str();
    }
    catch (const std::exception& e) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_result_set_get_string failed to retrieve "
            "value of " << cname << " due to exception: " << e.what();
        return 0;
    }
    catch (const char* s) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_result_set_get_string failed to retrieve "
            "value of " << cname << " due to a string exception: " << s;
        return 0;
    }
    catch (...) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_result_set_get_string failed to retrieve "
            "value of " << cname << " due to a unknown exception";
        return 0;
    }
} // fastbit_result_set_get_string

extern "C" int32_t
fastbit_result_set_getInt(FastBitResultSetHandle rset,
                          unsigned pos) {
    int32_t ret = INT_MAX;
    if (rset == 0)
        return ret;
    try {
        ret = rset->results->getInt(pos);
    }
    catch (const std::exception& e) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_result_set_getInt failed to retrieve "
            "value of column " << pos << " due to exception: " << e.what();
        ret = INT_MAX;
    }
    catch (const char* s) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_result_set_getInt failed to retrieve "
            "value of column " << pos << " due to a string exception: " << s;
        ret = INT_MAX;
    }
    catch (...) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_result_set_getInt failed toretrieve "
            "value of column " << pos << " due to a unknown exception";
        ret = INT_MAX;
    }
    return ret;
} // fastbit_result_set_getInt

extern "C" uint32_t
fastbit_result_set_getUnsigned(FastBitResultSetHandle rset,
                               unsigned pos) {
    uint32_t ret = UINT_MAX;
    if (rset == 0)
        return ret;
    try {
        ret = rset->results->getUInt(pos);
    }
    catch (const std::exception& e) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_result_set_getUnsigned failed to retrieve "
            "value of column " << pos << " due to exception: " << e.what();
        ret = UINT_MAX;
    }
    catch (const char* s) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_result_set_getUnsigned failed to retrieve "
            "value of column " << pos << " due to a string exception: " << s;
        ret = UINT_MAX;
    }
    catch (...) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_result_set_getUnsigned failed toretrieve "
            "value of column " << pos << " due to a unknown exception";
        ret = UINT_MAX;
    }
    return ret;
} // fastbit_result_set_getUnsigned

extern "C" int64_t
fastbit_result_set_getLong(FastBitResultSetHandle rset,
                               unsigned pos) {
    int64_t ret = INT64_MAX;
    if (rset == 0)
        return ret;
    try {
        ret = rset->results->getLong(pos);
    }
    catch (const std::exception& e) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_result_set_getLong failed to retrieve "
            "value of column " << pos << " due to exception: " << e.what();
        ret = INT64_MAX;
    }
    catch (const char* s) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_result_set_getLong failed to retrieve "
            "value of column " << pos << " due to a string exception: " << s;
        ret = INT64_MAX;
    }
    catch (...) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_result_set_getLong failed toretrieve "
            "value of column " << pos << " due to a unknown exception";
        ret = INT64_MAX;
    }
    return ret;
} // fastbit_result_set_getLong

extern "C" float
fastbit_result_set_getFloat(FastBitResultSetHandle rset,
                            unsigned pos) {
    float ret = FLT_MAX;
    if (rset == 0)
        return ret;
    try {
        ret = rset->results->getFloat(pos);
    }
    catch (const std::exception& e) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_result_set_getFloat failed to retrieve "
            "value of column " << pos << " due to exception: " << e.what();
        ret = FLT_MAX;
    }
    catch (const char* s) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_result_set_getFloat failed to retrieve "
            "value of column " << pos << " due to a string exception: " << s;
        ret = FLT_MAX;
    }
    catch (...) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_result_set_getFloat failed toretrieve "
            "value of column " << pos << " due to a unknown exception";
        ret = FLT_MAX;
    }
    return ret;
} // fastbit_result_set_getFloat

extern "C" double
fastbit_result_set_getDouble(FastBitResultSetHandle rset,
                             unsigned pos) {
    double ret = DBL_MAX;
    if (rset == 0)
        return ret;
    try {
        ret = rset->results->getDouble(pos);
    }
    catch (const std::exception& e) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_result_set_getDouble failed to retrieve "
            "value of column " << pos << " due to exception: " << e.what();
        ret = DBL_MAX;
    }
    catch (const char* s) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_result_set_getDouble failed to retrieve "
            "value of column " << pos << " due to a string exception: " << s;
        ret = DBL_MAX;
    }
    catch (...) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_result_set_getDouble failed toretrieve "
            "value of column " << pos << " due to a unknown exception";
        ret = DBL_MAX;
    }
    return ret;
} // fastbit_result_set_getDouble

extern "C" const char*
fastbit_result_set_getString(FastBitResultSetHandle rset,
                             unsigned pos) {
    if (rset == 0)
        return static_cast<const char*>(0);
    if (pos >= rset->strbuf.size())
        return static_cast<const char*>(0);
    try {
        std::string tmp = rset->results->getString(pos);
        rset->strbuf[pos].swap(tmp);
        return rset->strbuf[pos].c_str();
    }
    catch (const std::exception& e) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_result_set_getString failed to retrieve "
            "value of column " << pos << " due to exception: " << e.what();
        return 0;
    }
    catch (const char* s) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_result_set_getString failed to retrieve "
            "value of column " << pos << " due to a string exception: " << s;
        return 0;
    }
    catch (...) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_result_set_getString failed toretrieve "
            "value of column " << pos << " due to a unknown exception";
        return 0;
    }
} // fastbit_result_set_getString

/// The new data records are appended to the records already in the
/// directory if there is any.  In addition, if the new records contain
/// columns that are not already in the directory, then the new columns are
/// automatically added with existing records assumed to contain NULL
/// values.  This set of functions are intended for a user to append some
/// number of rows in one operation.  It is clear that writing one row as a
/// time is slow because of the overhead involved in writing the files.  On
/// the other hand, since the new rows are stored in memory, it can not
/// store too many rows.
extern "C" int
fastbit_flush_buffer(const char *dir) {
    int ierr = 0;
    if (dir == 0 || *dir == 0)
        return -1;

    try {
        ibis::util::mutexLock lock(&_capi_mutex, "fastbit_flush_buffer");
        
        if (_capi_tablex != 0) {
            ierr = _capi_tablex->write(dir, 0, 0);
            delete _capi_tablex;
            _capi_tablex = 0;

            // update the data partition in the directory dir
            if (ierr == 0 && _capi_tlist != 0) {
                ibis::part *t = _capi_tlist->find(dir);
                if (t != 0) {
                    t->releaseAccess(); // release read lock before update
                    ierr = t->updateData();
                    if (ierr < 0) {
                        // failed to update the data partition
                        LOGGER(ibis::gVerbose > 2)
                            << "fastbit_flush_buffer failed to update the data "
                            "partition based on directory " << dir
                            << ", will remove it from the list of known data "
                            "partitions";
                        _capi_tlist->remove(dir);
                    }
                }
            }
        }
    }
    catch (const std::exception& e) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_flush_buffer failed to write in-memory "
            "data to " << dir << " due to exception: "
            << e.what();
        ierr = -2;
    }
    catch (const char* s) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_flush_buffer failed to write in-memory "
            "data to " << dir << " due to a string exception: " << s;
        ierr = -3;
    }
    catch (...) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_flush_buffer failed to write in-memory "
            "data to " << dir << " due to a unknown exception";
        ierr = -4;
    }
    return ierr;
} // fastbit_flush_buffer

/// All invocations of this function are adding data to a single in-memory
/// buffer for a single data partition.
///
/// @arg colname Name of the column.  Must start with an alphabet and
/// followed by a combination of alphanumerical characters.  Following the
/// SQL standard, the column name is not case sensitive.  @arg coltype The
/// type of the values for the column.  The support types are: "category",
/// "text", "double", "float", "long", "int", "short", "byte", "ulong",
/// "uint", "ushort", and "ubyte".  Only the first non-space character is
/// checked for the first eight types, and only the first two characters
/// are checked for the remaining types.  This string is not case
/// sensitive.
///
/// @arg vals The array containing the values.  It is expected to contain
/// no less than @c nelem values, though only the first @c nelem values are
/// used by this function.
///
/// @arg nelem The number of elements of the array @c vals to be added to
/// the in-memory buffer.
///
/// @arg start The position (row number) of the first element of the array.
/// Normally, this argument is zero (0) if all values are valid.  One may
/// use this argument to skip some rows and indicate to FastBit that the
/// skipped rows contain NULL values.
extern "C" int
fastbit_add_values(const char *colname, const char *coltype,
                   void *vals, uint32_t nelem, uint32_t start) {
    int ierr = -1;
    if (colname == 0 || coltype == 0 || vals == 0) return ierr;
    while (*colname != 0 && isspace(*colname)) ++ colname;
    while (*coltype != 0 && isspace(*coltype)) ++ coltype;
    if (*colname == 0 || *coltype == 0) return ierr;
    if (nelem == 0) return 0;

    ibis::TYPE_T type = ibis::UNKNOWN_TYPE;
    ierr = 0;
    switch (*coltype) {
    default : ierr = -2; break;
    case 'D':
    case 'd': type = ibis::DOUBLE; break;
    case 'F':
    case 'f': type = ibis::FLOAT; break;
    case 'L':
    case 'l': type = ibis::LONG; break;
    case 'I':
    case 'i': type = ibis::INT; break;
    case 'S':
    case 's': type = ibis::SHORT; break;
    case 'B':
    case 'b': type = ibis::BYTE; break;
    case 'C':
    case 'c':
    case 'K':
    case 'k': type = ibis::CATEGORY; break;
    case 'T':
    case 't': type = ibis::TEXT; break;
    case 'U':
    case 'u': {
        switch (coltype[1]) {
        default : ierr = -2; break;
        case 'L':
        case 'l': type = ibis::ULONG; break;
        case 'I':
        case 'i': type = ibis::UINT; break;
        case 'S':
        case 's': type = ibis::USHORT; break;
        case 'B':
        case 'b': type = ibis::UBYTE; break;
        }
        break;}
    }
    if (ierr < 0) return ierr;

    try {
        { // a block to limit the scope of mutex lock
            ibis::util::mutexLock
                lock(&_capi_mutex, "fastbit_add_values");
            if (_capi_tablex == 0)
                _capi_tablex = new ibis::tafel();
        }
        if (_capi_tablex == 0) return -3;

        ierr = _capi_tablex->addColumn(colname, type);
        if (type == ibis::TEXT || type == ibis::CATEGORY) {
            // copying incoming strings to a std::vector<std::string>
            std::vector<std::string> tvals(nelem);
            char **tmp = (char **)vals;
            for(unsigned i = 0; i < nelem; ++ i) {
                std::cout.flush();
                tvals[i] = tmp[i];
            }
            ierr = _capi_tablex->append(colname, start, start+nelem,
                                        (void *)&tvals);
        }
        else {
            // pass the raw pointer
            ierr = _capi_tablex->append(colname, start, start+nelem, vals);
        }
    }
    catch (const std::exception& e) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_add_values failed to add values to "
            << colname << " to an in-memory data partition due to exception: "
            << e.what();
        ierr = -3;
    }
    catch (const char* s) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_add_values failed to add values to "
            << colname << " to an in-memory data partition due to a string "
            "exception: " << s;
        ierr = -4;
    }
    catch (...) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_add_values failed to add values to "
            << colname << " to an in-memory data partition due to a unknown "
            "exception";
        ierr = -5;
    }
    return ierr;
} // fastbit_add_values

extern "C" int fastbit_rows_in_partition(const char *dir) {
    int ierr = 0;
    ibis::part *t = _capi_get_part(dir);

    if (t != 0) {
        ierr = t->nRows();
        t->releaseAccess();
    }
    else {
        ierr = -2;
    }
    return ierr;
} // fastbit_rows_in_partition

extern "C" int fastbit_columns_in_partition(const char *dir) {
    int ierr = 0;
    ibis::part *t = _capi_get_part(dir);

    if (t != 0) {
        ierr = t->nColumns();
        t->releaseAccess();
    }
    else {
        ierr = -2;
    }
    return ierr;
} // fastbit_columns_in_partition
