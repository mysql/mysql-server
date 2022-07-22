// File $Id$
// Author: John Wu <John.Wu at ACM.org> Lawrence Berkeley National Laboratory
// Copyright (c) 2000-2016 the Regents of the University of California
//
// Implementation of the ibis::part functions except those that modify the
// content of a partition (which are in parti.cpp), or perform self joins
// (in party.cpp), or compute histograms (in parth*.cpp).
//
#if defined(_WIN32) && defined(_MSC_VER)
#pragma warning(disable:4786)   // some identifier longer than 256 characters
#endif
#if defined(__unix__) || defined(__HOS_AIX__) || defined(__APPLE__) || defined(_XOPEN_SOURCE) || defined(_POSIX_C_SOURCE) || defined(__MINGW__) || defined(__MINGW32__) || defined(__MINGW64__)
#include <unistd.h>     // popen, pclose
#include <sys/stat.h>   // stat
#include <dirent.h>     // DIR, opendir, readdir
#endif

#include "blob.h"       // ibis::blob
#include "category.h"   // ibis::text, ibis::category, ibis::column
#include "query.h"
#include "countQuery.h" // ibis::countQuery
#include "iroster.h"
#include "twister.h"    // ibis::MersenneTwister

#include <fstream>
#include <sstream>      // std::ostringstream
#include <algorithm>    // std::find, std::less, ...
#include <typeinfo>     // typeid
#include <stdexcept>    // std::invalid_argument
#include <memory>       // std::unique_ptr
#include <cctype>       // std::tolower

#include <stdio.h>      // popen, pclose
#include <stdlib.h>     // rand
#include <stdarg.h>     // vsprintf, ...
#include <signal.h>     // SIGINT

#if defined(HAVE_DIRENT_H) || defined(__unix__) || defined(__HOS_AIX__) \
    || defined(__APPLE__) || defined(__CYGWIN__) || defined(__MINGW32__) \
    || defined(_XOPEN_SOURCE) || defined(_POSIX_C_SOURCE)
#include <dirent.h>
#elif defined(_WIN32) && defined(_MSC_VER)
#define popen _popen
#define pclose _pclose
#endif

#define FASTBIT_SYNC_WRITE 1

// A higher quality random number generator within the file scope.  Use a
// function to (possibly) delay the invocation of the constructor.
static ibis::MersenneTwister& _ibis_part_urand() {
    static ibis::MersenneTwister mt;
    return mt;
} // _ibis_part_urand

extern "C" {
    /// A thread function to run the function queryTest or quickTest.
    static void* ibis_part_threadedTestFun1(void* arg) {
        if (arg == 0)
            return reinterpret_cast<void*>(-1L);
        ibis::part::thrArg* myArg = (ibis::part::thrArg*)arg;
        if (myArg->et == 0)
            return reinterpret_cast<void*>(-2L);
        const ibis::part *et0 = myArg->et;

        try {
            std::string longtest;
            ibis::part::readLock lock(et0, "threadedTestFun1");
            if (myArg->pref) {
                longtest = myArg->pref;
                longtest += ".longTests";
            }
            else {
                longtest = et0->name();
                longtest += ".longTests";
            }

            if (et0->nRows() < 1048576 ||
                ibis::gParameters().isTrue(longtest.c_str()))
                et0->queryTest(myArg->pref, myArg->nerrors);
            else
                et0->quickTest(myArg->pref, myArg->nerrors);
            return(reinterpret_cast<void*>(0L));
        }
        catch (const std::exception &e) {
            et0->logMessage("threadedTestFun1",
                            "received exception \"%s\"", e.what());
            return(reinterpret_cast<void*>(-11L));
        }
        catch (const char* s) {
            et0->logMessage("threadedTestFun1",
                            "received exception \"%s\"", s);
            return(reinterpret_cast<void*>(-12L));
        }
        catch (...) {
            et0->logMessage("threadedTestFun1", "received an "
                            "unexpected exception");
            return(reinterpret_cast<void*>(-10L));
        }
    } // ibis_part_threadedTestFun1

    /// A thread function to work on a shared list of range conditions.
    static void* ibis_part_threadedTestFun2(void* arg) {
        if (arg == 0)
            return reinterpret_cast<void*>(-1L);

        ibis::part::thrArg *myList = (ibis::part::thrArg*)arg;
        if (myList->et == 0)
            return reinterpret_cast<void*>(-2L);
        const ibis::part *et0 = myList->et;
        const time_t myid = ibis::fileManager::iBeat();
        LOGGER(ibis::gVerbose > 2)
            << "INFO: thread (local id " << myid
            << ") start evaluating queries on partition " << et0->name();

        try {
            unsigned myerr = 0;
            unsigned mycnt = 0;
            ibis::countQuery qq(et0);
            for (uint32_t j = myList->cnt(); j < myList->conds.size();
                 j = myList->cnt()) {
                ++ mycnt;
                qq.setWhereClause(myList->conds[j].c_str());
                const int ierr = qq.evaluate();
                if (ierr == 0) {
                    myList->hits[j] = qq.getNumHits();
                }
                else {
                    LOGGER(ibis::gVerbose >= 0)
                        << "Warning -- thread " << myid
                        << " received error code " << ierr
                        << " while evaluating \"" << myList->conds[j]
                        << "\" on data partition " << et0->name();
                    ++ myerr;
                }
            }
            (*myList->nerrors) += myerr;
            LOGGER(ibis::gVerbose > 2)
                << "INFO: thread " << myid << " completed " << mycnt
                << " set" << (mycnt > 1 ? "s" : "")
                << " of range conditions and encountered " << myerr << " error"
                << (myerr > 1 ? "s" : "") << " during query evaluations";
            return(reinterpret_cast<void*>(0L));
        }
        catch (const std::exception &e) {
            et0->logMessage("threadedTestFun2",
                            "received exception \"%s\"", e.what());
            return(reinterpret_cast<void*>(-11L));
        }
        catch (const char* s) {
            et0->logMessage("threadedTestFun2",
                            "received exception \"%s\"", s);
            return(reinterpret_cast<void*>(-12L));
        }
        catch (...) {
            et0->logMessage("threadedTestFun2", "received an "
                            "unexpected exception");
            return(reinterpret_cast<void*>(-10L));
        }
    } // ibis_part_threadedTestFun2

    /// This routine wraps around doBackup to allow doBackup being run in a
    /// separated thread.
    static void* ibis_part_startBackup(void* arg) {
        if (arg == 0) return reinterpret_cast<void*>(-1L);
        ibis::part* et = (ibis::part*)arg; // arg is actually a part*
        try {
            ibis::part::readLock lock(et, "startBackup");
            et->doBackup();
            return(reinterpret_cast<void*>(0L));
        }
        catch (const std::exception &e) {
            et->logMessage("startBackup", "doBackup received "
                           "exception \"%s\"", e.what());
            return(reinterpret_cast<void*>(-21L));
        }
        catch (const char* s) {
            et->logMessage("startBackup", "doBackup received "
                           "exception \"%s\"", s);
            return(reinterpret_cast<void*>(-22L));
        }
        catch (...) {
            et->logMessage("startBackup", "doBackup received "
                           "an unexpected exception");
            return(reinterpret_cast<void*>(-20L));
        }
    } // ibis_part_startBackup

    /// The thread function to building indexes.
    static void* ibis_part_build_indexes(void* arg) {
        if (arg == 0) return reinterpret_cast<void*>(-1L);
        ibis::part::indexBuilderPool &pool =
            *(reinterpret_cast<ibis::part::indexBuilderPool*>(arg));
        const ibis::table::stringArray &opt = pool.opt;
        const char *iopt;
        try {
            for (uint32_t i = pool.cnt(); i < pool.tbl.nColumns();
                 i = pool.cnt()) {
                ibis::column *col = pool.tbl.getColumn(i);
                if (col == 0) break;
                iopt = 0;
                if (opt.size() > 1) {
                    size_t j = 0;
                    for (j = 0; j+1 < opt.size(); j += 2) {
                        if (ibis::util::nameMatch(col->name(), opt[j])) {
                            ++ j;
                            break;
                        }
                    }
                    if (j < opt.size()) {
                        iopt = opt[j];
                    }
                }
                else if (opt.size() > 0) {
                    iopt = opt.back();
                }

                if (! (col->upperBound() >= col->lowerBound()))
                    col->computeMinMax();
                col->loadIndex(iopt);
                if (col->indexedRows() != pool.tbl.nRows() && col->indexedRows() > 0) {
                    std::cerr << "FBHERE\n";
                    // rebuild the index if the existing one does not
                    // have the same number of rows as the current data
                    // partition
                    col->unloadIndex();
                    col->purgeIndexFile();
                    std::unique_ptr<ibis::index>
                        tmp(ibis::index::create(col, 0, iopt));
                }
                else {
                    std::cerr << "FBHERE2\n";
                    col->unloadIndex();
                }
                // std::string snm;
                // const char *fnm = col->dataFileName(snm);
                // ibis::fileManager::instance().flushFile(fnm);
            }
            return 0;
        }
        catch (const std::exception &e) {
            pool.tbl.logMessage("buildIndexes", "loadIndex "
                                "received std::exception \"%s\"", e.what());
            return(reinterpret_cast<void*>(-31L));
        }
        catch (const char* s) {
            pool.tbl.logMessage("buildIndexes", "loadIndex "
                                "received exception \"%s\"", s);
            return(reinterpret_cast<void*>(-32L));
        }
        catch (...) {
            pool.tbl.logMessage("buildIndexes", "loadIndex received an "
                                "unexpected exception");
            return(reinterpret_cast<void*>(-30L));
        }
    } // ibis_part_build_indexes
} // extern "C"

/// The incoming argument can be a directory name or a data partition name.
/// If it contains any forward or backward slash, it is assumed to be a
/// directory name.  If it names an existing directory, it is used as the
/// primary directory for storing the data.  Otherwise, it is assumed to be
/// the name of a data partition.  In which case, this function looks for
/// data directory names in the global parameter list under the parameters
/// 'name.activeDir' and 'name.backupDir' or 'name.dataDir1' and
/// 'name.dataDir2'.  If the name is a directory name, then no attempt
/// shall be made to produce a second directory name.
///
/// The default value for name is a nil pointer.  In this case, it will
/// find 'dataDir1' and 'dataDir2' from the global parameter list.
///
/// The default argument for ro is false, which allows new directory to be
/// created and new data records to be appended.  If the argument ro is
/// true, then the specified data directory must already exist, otherwise,
/// an exception is thrown.  A data partition constructed with ro set to
/// true will be called a read-only data partition and its content shall
/// not be changed in the relational algebra view.
ibis::part::part(const char* name, bool ro) :
    m_name(0), m_desc(), rids(0), nEvents(0), activeDir(0),
    backupDir(0), switchTime(0), state(UNKNOWN_STATE), idxstr(0),
    myCleaner(0), readonly(ro) {
    // initialize the locks
    if (0 != pthread_mutex_init
        (&mutex, static_cast<const pthread_mutexattr_t*>(0))) {
        throw "part::ctor failed to initialize the mutex lock" IBIS_FILE_LINE;
    }

    if (0 != pthread_rwlock_init(&rwlock, 0)) {
        throw "part::ctor failed to initialize the rwlock" IBIS_FILE_LINE;
    }

    (void) ibis::fileManager::instance(); // initialize the file manager
    // for the special "in-core" data partition, there is no need to call
    // the function init, note that a valid data partition name can not
    // contain a dash
    if (name == 0 || stricmp(name, "in-core") != 0)
        init(name);
} // the default constructor of ibis::part

/// The meta tags are specified as a list of name-value strings, where each
/// string in one name-value pair.
ibis::part::part(const std::vector<const char*> &mtags, bool ro) :
    m_name(0), m_desc(), rids(0), nEvents(0), activeDir(0),
    backupDir(0), switchTime(0), state(UNKNOWN_STATE), idxstr(0),
    myCleaner(0), readonly(ro) {
    // initialize the locks
    if (0 != pthread_mutex_init
        (&mutex, static_cast<const pthread_mutexattr_t*>(0))) {
        throw "part::ctor failed to initialize the mutex lock" IBIS_FILE_LINE;
    }

    if (0 != pthread_rwlock_init(&rwlock, 0)) {
        throw "part::ctor failed to initialize the rwlock" IBIS_FILE_LINE;
    }

    (void) ibis::fileManager::instance(); // initialize the file manager
    std::string pref;
    genName(mtags, pref);
    init(pref.c_str());
    if (mtags.size() > 2 || 0 == stricmp(mtags[0], "name"))
        setMetaTags(mtags);
} // constructor from a vector of strings

/// The name-value pairs are specified in a structured form.
ibis::part::part(const ibis::resource::vList &mtags, bool ro) :
    m_name(0), m_desc(), rids(0), nEvents(0), activeDir(0),
    backupDir(0), switchTime(0), state(UNKNOWN_STATE), idxstr(0),
    myCleaner(0), readonly(ro) {
    // initialize the locks
    if (0 != pthread_mutex_init
        (&mutex, static_cast<const pthread_mutexattr_t*>(0))) {
        throw "part::ctor failed to initialize the mutex lock" IBIS_FILE_LINE;
    }

    if (0 != pthread_rwlock_init(&rwlock, 0)) {
        throw "part::ctor failed to initialize the rwlock" IBIS_FILE_LINE;
    }

    (void) ibis::fileManager::instance(); // initialize the file manager
    std::string pref; // new name
    genName(mtags, pref);
    init(pref.c_str());
    if (mtags.size() > 1 || mtags.find("name") != mtags.begin())
        setMetaTags(mtags);
} // constructor from vList

/// Construct a partition from the named directories.  Originally, FastBit
/// was designed to work with a pair of directories, @c adir and @c bdir.
/// Therefore, the constructor takes a pair of directory names.  In many
/// cases, data is stored only in one directory, in which simply give the
/// data directory as @c adir and leave @c bdir as null.  Prefer to have
/// full and complete path.
///
/// The default argument for ro is false, which allows new directory to be
/// created and new data records to be appended.  If the argument ro is
/// true, then the specified data directory must already exist, otherwise,
/// an exception is thrown.  A data partition constructed with ro set to
/// true will be called a read-only data partition and its content shall
/// not be changed in the relational algebra view.
ibis::part::part(const char* adir, const char* bdir, bool ro) :
    m_name(0), m_desc(), rids(0), nEvents(0), activeDir(0),
    backupDir(0), switchTime(0), state(UNKNOWN_STATE), idxstr(0),
    myCleaner(0), readonly(ro) {
    (void) ibis::fileManager::instance(); // initialize the file manager
    // initialize the locks
    if (pthread_mutex_init(&mutex, 0)) {
        throw "part::ctor failed to initialize the mutex lock" IBIS_FILE_LINE;
    }

    if (pthread_rwlock_init(&rwlock, 0)) {
        throw "part::ctor failed to initialize the rwlock" IBIS_FILE_LINE;
    }

    if (adir == 0) return;
    //if (*adir != FASTBIT_DIRSEP) return;
    (void) ibis::fileManager::instance(); // initialize the file manager
    int maxLength = 0;
    activeDir = ibis::util::strnewdup(adir);
    ibis::util::removeTail(activeDir, FASTBIT_DIRSEP);
    { // make sure activeDir exists, use an extra block to limit the scope of tmp
        Stat_T tmp;
        if (UnixStat(activeDir, &tmp) == 0) {
            if ((tmp.st_mode&S_IFDIR) == S_IFDIR) {
                // read the metadata file in activeDir
                maxLength = readMetaData(nEvents, columns, activeDir);
            }
            else {
                LOGGER(ibis::gVerbose >= 0)
                    << "Error -- part::part(" << adir << ", "
                    << (const void*)bdir << "): stat.st_mode="
                    << static_cast<int>(tmp.st_mode) << " is not a directory";
                throw std::invalid_argument("the argument to part::ctor was not "
                                            "a directory name" IBIS_FILE_LINE);
            }
        }
        else {
            if (errno == ENOENT) {
                // no such directory
                if (readonly) {
                    return;
                }
                else {
                    // make one
                    int ierr = ibis::util::makeDir(adir);
                    if (ierr < 0)
                        throw "part::ctor can NOT generate the "
                            "specified directory" IBIS_FILE_LINE;
                }
            }
            else if (errno != 0) {
                LOGGER(ibis::gVerbose > 5 || errno != ENOENT)
                    << "Warning -- part::part(" << (void*)adir << ", "
                    << (void*)bdir << ") stat(" << adir << ") failed ... "
                    << strerror(errno);
                throw std::invalid_argument("the argument to part::part was not "
                                            "a directory name" IBIS_FILE_LINE);
            }
        }
    }

    if (maxLength > 0) {
        // read in the RIDs
        readRIDs();
        if (rids->size() > 0 && rids->size() != nEvents)
            nEvents = rids->size();
        if (nEvents > 0 && switchTime == 0)
            switchTime = time(0);

        if (rids->size() == 0) {
            std::string fillrids(m_name);
            fillrids += ".fillRIDs";
            if (readonly == false &&
                ibis::gParameters().isTrue(fillrids.c_str())) {
                std::string fname(activeDir);
                fname += FASTBIT_DIRSEP;
                fname += "-rids";
                fillRIDs(fname.c_str());
            }
        }
    }
    else if (readonly) { // missing directory or metadata
        LOGGER(ibis::gVerbose > 2)
            << "part::ctor can not construct a part objet from "
            << activeDir << " because the directory does not exist or it does "
            "not have the metadata file -part.txt";
        return;
    }

    if (m_name == 0) {
        // copy the directory name as the name of the data part
        char* tmp = strrchr(activeDir, FASTBIT_DIRSEP);
        if (tmp != 0)
            m_name = ibis::util::strnewdup(tmp+1);
        else
            m_name = ibis::util::strnewdup(activeDir);
    }
    // generate the backupDir name
    if (bdir != 0 && *bdir != 0) {
        char *tmp = backupDir;
        backupDir = const_cast<char*>(bdir); // will not modify bdir
        if (0 == verifyBackupDir()) { // no obvious error
            backupDir = ibis::util::strnewdup(bdir);
            delete [] tmp;
        }
        else { // restore the backupDir read from metadata file
            backupDir = tmp;
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- user provided directory \"" << bdir
                << "\" doesn't match the active data directory \""
                << activeDir << "\"; use the alternative directory \""
                << tmp << "\" stored in the metadata file";
        }
    }
    if (backupDir == 0) {
        std::string nm = "ibis.table";
        if (m_name != 0) {
            nm += '.';
            nm += m_name;
        }
        nm += ".useBackupDir";
        const char *str = ibis::gParameters()[nm.c_str()];
        if (str == 0) {
            nm.erase(nm.size()-9);
            nm += "ShadowDir";
            str = ibis::gParameters()[nm.c_str()];
        }
        if (ibis::resource::isStringTrue(str)) {
            if (bdir)
                backupDir = ibis::util::strnewdup(bdir);
            else
                deriveBackupDirName();
        }
    }

    if (nEvents > 0) {
        std::string mskfile(activeDir);
        if (! mskfile.empty())
            mskfile += FASTBIT_DIRSEP;
        mskfile += "-part.msk";
        try {
            // read mask of the partition
            amask.read(mskfile.c_str());
            if (amask.size() != nEvents) {
                LOGGER(ibis::gVerbose > 1 && amask.size() > 0)
                    << "Warning -- part::ctor read a unexpected "
                    "-part.msk, mask file \"" << mskfile
                    << "\" contains only " << amask.size()
                    << " bit" << (amask.size()>1?"s":"")
                    << ", but " << nEvents << (nEvents>1?" were":" was")
                    << " expected";
                amask.adjustSize(nEvents, nEvents);
                if (amask.cnt() < nEvents) {
                    amask.write(mskfile.c_str());
                }
                else {
                    remove(mskfile.c_str());
                }
                ibis::fileManager::instance().flushFile(mskfile.c_str());
            }
            LOGGER(ibis::gVerbose > 5)
                << "part::ctor -- mask for partition " << name() << " has "
                << amask.cnt() << " set bit" << (amask.cnt()>1?"s":"")
                << " out of " << amask.size();
        }
        catch (...) {
            LOGGER(ibis::gVerbose >= 0)
                << "Warning -- part::ctor cannot read mask file \""
                << mskfile << "\", assume all rows (" << nEvents
                << ") are active";
            amask.set(1, nEvents);
            remove(mskfile.c_str());
        }
    }

    // superficial checks
    int j = 0;
    if (maxLength <= 0) maxLength = 16;
    if (std::strlen(activeDir)+16+maxLength > PATH_MAX) {
        ibis::util::logMessage("Warning", "directory name \"%s\" too long",
                               activeDir);
        ++j;
    }
    if (backupDir != 0 && std::strlen(backupDir)+16+maxLength > PATH_MAX) {
        ibis::util::logMessage("Warning", "directory name \"%s\" too long",
                               backupDir);
        ++j;
    }
    if (j) throw "part::ctor -- direcotry names too long" IBIS_FILE_LINE;

    if (backupDir != 0) {
        ibis::util::removeTail(backupDir, FASTBIT_DIRSEP);
        // check its header file for consistency
        if (nEvents > 0) {
            if (verifyBackupDir() == 0) {
                state = STABLE_STATE;
            }
            else {
                makeBackupCopy();
            }
        }
        else {
            ibis::util::mutexLock lock(&ibis::util::envLock, backupDir);
            ibis::util::removeDir(backupDir, true);
            state = STABLE_STATE;
        }
    }
    else { // assumed to be in stable state
        state = STABLE_STATE;
    }

    myCleaner = new ibis::part::cleaner(this);
    ibis::fileManager::instance().addCleaner(myCleaner);

    if ((ibis::gVerbose > 1 || (ibis::gVerbose > 0 && nEvents > 0)) &&
        m_name != 0) {
        ibis::util::logger lg;
        lg() << "Constructed ";
        if (nEvents == 0)
            lg() << "(empty) ";
        lg() << "part "
             << (m_name?m_name:"??");
        if (! m_desc.empty())
            lg() << " -- " << m_desc;
        if (ibis::gVerbose > 1) {
            lg() << "\nactiveDir = \"" << activeDir << "\"";
            if (backupDir != 0)
                lg() << "\nbackupDir = \"" << backupDir << "\"";
        }
        if (ibis::gVerbose > 1 && nEvents > 0 && ! columns.empty()) {
            lg() << "\n";
            if (ibis::gVerbose > 3) {
                print(lg());
            }
            else {
                lg() << "  " << nEvents << " row" << (nEvents>1?"s":"")
                     << " and " << columns.size() << " column"
                     << (columns.size()>1?"s":"");
            }
        }
    }
} // construct part from the named direcotries

// the destructor
ibis::part::~part() {
    LOGGER(ibis::gVerbose > 3 && m_name != 0)
        << "clearing data partition " << name();
    {   // make sure all read accesses have finished
        writeLock lock(this, "~part");

        // Because the key in the std::map that defined the columns are
        // part of the objects to be deleted, need to copy the columns into
        // a linear container to avoid any potential problems.
        std::vector<ibis::column*> tmp;
        tmp.reserve(columns.size());
        for (columnList::const_iterator it = columns.begin();
             it != columns.end(); ++ it)
            tmp.push_back((*it).second);
        columns.clear();

        for (uint32_t i = 0; i < tmp.size(); ++ i)
            delete tmp[i];
        tmp.clear();
    }

    ibis::fileManager::instance().removeCleaner(myCleaner);
    ibis::resource::clear(metaList);
    // clear the rid list and the rest
    delete rids;
    delete myCleaner;
    if (backupDir != 0 && *backupDir != 0)
        ibis::fileManager::instance().flushDir(backupDir);
    //ibis::fileManager::instance().flushDir(activeDir);
    delete [] activeDir;
    delete [] backupDir;
    delete [] idxstr;
    delete [] m_name;

    pthread_mutex_destroy(&mutex);
    pthread_rwlock_destroy(&rwlock);
} // the destructor

void ibis::part::genName(const std::vector<const char*> &mtags,
                         std::string &name) {
    for (uint32_t i = 1; i < mtags.size(); i += 2) {
        if (i > 1)
            name += '_';
        name += mtags[i];
    }
    if (name.empty())
        name = ibis::util::userName();
} // ibis::part::genName

void ibis::part::genName(const ibis::resource::vList &mtags,
                         std::string &name) {
    bool isStar = (mtags.size() == 3);
    if (isStar) {
        isStar = ((mtags.find("trgSetupName") != mtags.end()) &&
                  (mtags.find("production") != mtags.end()) &&
                  (mtags.find("magScale") != mtags.end()));
    }
    if (isStar) {
        ibis::resource::vList::const_iterator it = mtags.find("production");
        name = (*it).second;
        name += '_';
        it = mtags.find("trgSetupName");
        name += (*it).second;
        name += '_';
        it = mtags.find("magScale");
        name += (*it).second;
    }
    else {
        for (ibis::resource::vList::const_iterator it = mtags.begin();
             it != mtags.end(); ++it) {
            if (it != mtags.begin())
                name += '_';
            name += (*it).second;
        }
    }
    if (name.empty())
        name = ibis::util::userName();
} // ibis::part::genName

/// Rename the partition to avoid conflicts with an existing list of
/// data partitions.
/// If the incoming name is empty, this function assigns the data partition
/// a random name.  If it already has a name, it will append a random
/// number at the end.  It will try as many random numbers as necessary to
/// produce a name that is not already in the list of known data
/// partitions.
void ibis::part::rename(const ibis::partAssoc& known) {
    std::string tmp1, tmp2;
    std::vector<unsigned> rands;
    ibis::util::mutexLock ml(&mutex, "part::rename");
    if (switchTime == 0) // presume to be not yet set
        switchTime = time(0);
    // attempt 0: use the description
    if (m_name == 0 || *m_name == 0) {
        if (activeDir != 0 && *activeDir != 0) {
            tmp1 = activeDir;
        }
        else if (!m_desc.empty()) {
            tmp1 = ibis::util::shortName(m_desc);
        }
        else {
            ibis::util::int2string(tmp2, ibis::fileManager::iBeat());
            tmp1 = "_";
            tmp1 += tmp2;
        }
        if (known.find(tmp1.c_str()) == known.end()) { // got a unique name
            delete [] m_name;
            m_name = ibis::util::strnewdup(tmp1.c_str());
            return;
        }
    }
    // attempt 1: use the time stamp
    rands.push_back(switchTime);
    ibis::util::int2string(tmp2, rands[0]);
    if (m_name != 0 && *m_name != 0)
        tmp1 = m_name;
    tmp1 += '_';
    const size_t stem = tmp1.size();
    tmp1 += tmp2;
    if (known.find(tmp1.c_str()) == known.end()) { // got a unique name
        delete [] m_name;
        m_name = ibis::util::strnewdup(tmp1.c_str());
        return;
    }
    // attempt 2: add ibeat
    rands.push_back(ibis::fileManager::iBeat());
    ibis::util::int2string(tmp2, rands[0], rands[1]);
    tmp1.erase(stem);
    tmp1 += tmp2;
    if (known.find(tmp1.c_str()) == known.end()) { // got a unique name
        delete [] m_name;
        m_name = ibis::util::strnewdup(tmp1.c_str());
        return;
    }
    // attempt 3: add random numbers
    while (true) {
        // add a new random number
        rands.push_back(_ibis_part_urand().nextInt());
        ibis::util::int2string(tmp2, rands);
        tmp1.erase(stem);
        tmp1 += tmp2;
        if (known.find(tmp1.c_str()) == known.end()) { // got a unique name
            delete [] m_name;
            m_name = ibis::util::strnewdup(tmp1.c_str());
            return;
        }

        for (long j = ibis::fileManager::iBeat(); j > 0; -- j) {
            ++ rands.back(); // next number
            ibis::util::int2string(tmp2, rands);
            tmp1.erase(stem);
            tmp1 += tmp2;
            if (known.find(tmp1.c_str()) == known.end()) {
                delete [] m_name;
                m_name = ibis::util::strnewdup(tmp1.c_str());
                return;
            }
        }
    } // while (true)

    // LOGGER(ibis::gVerbose > 0)
    //  << "Warning -- part[" << (m_name ? m_name : "?")
    //  << "]::rename cannot produce a name that is different from "
    //  "those in a list of " << known.size() << " known partition"
    //  << (known.size()>1 ? "s" : "");
} // ibis::part::rename

/// Change the name of the data partition to the given name.  Nothing is
/// done if the incoming argument is a nil pointer or points to a null
/// string.
///
/// @note This function does not check that incoming name conforms to the
/// SQL standard.
void ibis::part::rename(const char *newname) {
    if (newname == 0 || *newname == 0 || newname == m_name) return;

    delete [] m_name;
    m_name = ibis::util::strnewdup(newname);
} // ibis::part::rename

/// Determines where to store the data.
void ibis::part::init(const char* iname) {
    // make sure the file manager is initialized
    (void) ibis::fileManager::instance();

    int j = 0;
    const char* str;
    delete [] activeDir;
    delete [] backupDir;
    activeDir = 0;
    backupDir = 0;

    if (iname != 0 && *iname != 0) {
        if (strchr(iname, '/') != 0) {
            activeDir = ibis::util::strnewdup(iname);
        }
        else if (strchr(iname, '\\') != 0) {
            activeDir = ibis::util::strnewdup(iname);
        }
        else {
            Stat_T tmp;
            if (UnixStat(iname, &tmp) == 0) {
                if ((tmp.st_mode & S_IFDIR) == S_IFDIR)
                    activeDir = ibis::util::strnewdup(iname);
            }
        }

        if (activeDir == 0)
            j = std::strlen(iname);
    }

    std::string pname("ibis.");
    if (j > 0) {
        pname += iname;
        pname += '.';
    }
    j += 6;

    if (activeDir == 0) {// get the active directory name
        pname += "activeDir";
        str = ibis::gParameters()[pname.c_str()];
        if (str != 0 && *str != 0) {
            activeDir = ibis::util::strnewdup(str);
            pname.erase(j, pname.size());
            pname += "backupDir";
            str = ibis::gParameters()[pname.c_str()];
            if (str != 0 && *str != 0)
                backupDir = ibis::util::strnewdup(str);
        }
    }
    if (activeDir == 0) {
        pname.erase(j, pname.size());
        pname += "DataDir1";
        str = ibis::gParameters()[pname.c_str()];
        if (str != 0 && *str != 0) {
            activeDir = ibis::util::strnewdup(str);
            pname.erase(j, pname.size());
            pname += "DataDir2";
            str = ibis::gParameters()[pname.c_str()];
            if (str != 0 && *str != 0)
                backupDir = ibis::util::strnewdup(str);
        }
    }
    if (activeDir == 0) {
        pname.erase(j, pname.size());
        pname += "activeDirectory";
        str = ibis::gParameters()[pname.c_str()];
        if (str != 0 && *str != 0) {
            activeDir = ibis::util::strnewdup(str);
            pname.erase(j, pname.size());
            pname += "backupDirectory";
            str = ibis::gParameters()[pname.c_str()];
            if (str != 0 && *str != 0)
                backupDir = ibis::util::strnewdup(str);
        }
    }
    if (activeDir == 0) {
        pname.erase(j, pname.size());
        pname += "DataDir";
        str = ibis::gParameters()[pname.c_str()];
        if (str != 0 && *str != 0) {
            activeDir = ibis::util::strnewdup(str);
            pname.erase(j, pname.size());
            pname += "backupDir";
            str = ibis::gParameters()[pname.c_str()];
            if (str != 0 && *str != 0)
                backupDir = ibis::util::strnewdup(str);
        }
    }
    if (activeDir == 0) {
        pname.erase(j, pname.size());
        pname += "DataDirectory";
        str = ibis::gParameters()[pname.c_str()];
        if (str != 0 && *str != 0) {
            activeDir = ibis::util::strnewdup(str);
            pname.erase(j, pname.size());
            pname += "backupDirectory";
            str = ibis::gParameters()[pname.c_str()];
            if (str != 0 && *str != 0)
                backupDir = ibis::util::strnewdup(str);
        }
        else {
            pname.erase(j, pname.size());
            pname += "IndexDirectory";
            str = ibis::gParameters()[pname.c_str()];
            if (str != 0 && *str != 0) {
                activeDir = ibis::util::strnewdup(str);
            }
            else {
                pname.erase(j, pname.size());
                pname += "DataDir2";
                str = ibis::gParameters()[pname.c_str()];
                if (0 != str && *str != 0) {
                    backupDir = ibis::util::strnewdup(str);
                }
            }
        }
    }
    if (activeDir == 0) {
        if (readonly) {
            throw std::invalid_argument
                ("part::init failed to determine a data directory"
                 IBIS_FILE_LINE);
        }
        else if (FASTBIT_DIRSEP == '/') {
            activeDir = ibis::util::strnewdup(".ibis/dir1");
        }
        else {
            activeDir = ibis::util::strnewdup(".ibis\\dir1");
        }
    }

    ibis::util::removeTail(activeDir, FASTBIT_DIRSEP);
    try {
        if (! readonly) {
            int ierr = ibis::util::makeDir(activeDir); // make sure it exists
            if (ierr < 0) {
                LOGGER(ibis::gVerbose >= 0)
                    << "Error -- part::init(" << (iname!=0 ? iname : "")
                    << ") failed to create directory " << activeDir;
                throw "part::init can NOT generate the necessary data directory"
                    IBIS_FILE_LINE;
            }
        }
    }
    catch (const std::exception &e) {
        LOGGER(ibis::gVerbose >= 0)
            << "Error -- part::init failed because of " << e.what();
        throw "part::init received a std::exception while calling makeDir";
    }
    catch (const char* s) {
        LOGGER(ibis::gVerbose >= 0)
            << "Error -- part::init failed because of " << s;
        throw "part::init received a string exception while calling makeDir";
    }
    catch (...) {
        throw "part::init received a unknown exception while calling makeDir";
    }

    // read metadata file in activeDir
    int maxLength = readMetaData(nEvents, columns, activeDir);
    if (maxLength <= 0) {
        LOGGER(ibis::gVerbose > 2)
            << "part::init cannot initialize the ibis::part "
            "object because " << activeDir
            << " does not exist or does not have the metadata file -part.txt";
        if (readonly) return;
    }
    const char *tmp = strrchr(activeDir, FASTBIT_DIRSEP);
    // use the current activeDir if it contains a valid data partition, the
    // name was derived from global resource parameters, or activeDir
    // matches exactly the input name.
    const bool useDir = ((m_name != 0 && nEvents > 0) ||
                         iname == 0 || *iname == 0 ||
                         (iname[std::strlen(iname)-1] != FASTBIT_DIRSEP ?
                          std::strcmp(activeDir, iname) == 0 :
                          strncmp(activeDir, iname, std::strlen(iname)-1) == 0) ||
                         (tmp != 0 && 0 == std::strcmp(tmp+1, iname)));
    if (! useDir) { // need a new subdirectory
        std::string subdir = activeDir;
        subdir += FASTBIT_DIRSEP;
        subdir += iname;
        ibis::util::makeDir(subdir.c_str());
        delete [] activeDir;
        activeDir = ibis::util::strnewdup(subdir.c_str());
        if (backupDir != 0) {
            subdir = backupDir;
            delete [] backupDir;
            backupDir = 0;
        }
        else {
            subdir.erase();
        }
        maxLength = readMetaData(nEvents, columns, activeDir);
        if (maxLength <= 0) {
            LOGGER(ibis::gVerbose > 2)
                << "Warning -- part::init can not initialize the object "
                "because " << activeDir << " does not exist or does not "
                "have the metadata file -part.txt";
            if (readonly) return;
        }
        if (backupDir != 0) {
            if (verifyBackupDir() != 0) {
                if (! subdir.empty()) {
                    delete [] backupDir;
                    backupDir = 0;
                }
            }
        }

        if (backupDir == 0) {
            pname.erase(j, pname.size());
            pname += "useBackupDir";
            if (ibis::gParameters().isTrue(pname.c_str())) {
                // use backup dir
                if (! subdir.empty()) {
                    subdir += FASTBIT_DIRSEP;
                    subdir += iname;
                    int ierr = ibis::util::makeDir(subdir.c_str());
                    if (ierr >= 0)
                        backupDir = ibis::util::strnewdup(subdir.c_str());
                }
                if (backupDir == 0)
                    deriveBackupDirName();
            }
        }
    }

    if (maxLength > 0 && nEvents > 0) { // metadata file exists
        // read in the RIDs
        readRIDs();
        if (rids->size() > 0 && rids->size() != nEvents)
            nEvents = rids->size();
        if (nEvents > 0 && switchTime == 0)
            switchTime = time(0);
        if (rids->size() == 0) {
            std::string fillrids(m_name);
            fillrids += ".fillRIDs";
            if (readonly == false &&
                ibis::gParameters().isTrue(fillrids.c_str())) {
                std::string fname(activeDir);
                fname += FASTBIT_DIRSEP;
                fname += "-rids";
                fillRIDs(fname.c_str());
            }
        }
    }

    if (m_name == 0) { // should assign a name
        if (iname != 0) {  // use argument to this function
            m_name = ibis::util::strnewdup(iname);
        }
        else if (nEvents > 0) {
            // use the directory name
            if (tmp != 0)
                m_name = ibis::util::strnewdup(tmp+1);
            else
                m_name = ibis::util::strnewdup(activeDir);
        }
    }

    if (backupDir != 0 &&
        0 == strncmp(backupDir, activeDir, std::strlen(backupDir)))
        deriveBackupDirName();

    if (backupDir != 0) {
        ibis::util::removeTail(backupDir, FASTBIT_DIRSEP);
        if (nEvents > 0) {
            // check its header file for consistency
            if (verifyBackupDir() == 0) {
                state = STABLE_STATE;
            }
            else {
                makeBackupCopy();
            }
        }
        else {
            ibis::util::mutexLock lock(&ibis::util::envLock, backupDir);
            ibis::util::removeDir(backupDir, true);
            state = STABLE_STATE;
        }
    }
    else { // assumed to be in stable state
        state = STABLE_STATE;
    }

    if (nEvents > 0) { // read mask of the partition
        std::string mskfile(activeDir);
        if (! mskfile.empty())
            mskfile += FASTBIT_DIRSEP;
        mskfile += "-part.msk";
        try {
            amask.read(mskfile.c_str());
            if (amask.size() != nEvents) {
                LOGGER(ibis::gVerbose > 1 && amask.size() > 0)
                    << "Warning -- part::init read a unexpected "
                    "-part.msk, mask file \"" << mskfile
                    << "\" contains only " << amask.size()
                    << " bit" << (amask.size()>1?"s":"")
                    << ", but " << nEvents << (nEvents>1?" were":" was")
                    << " expected";
                amask.adjustSize(nEvents, nEvents);
                if (amask.cnt() < nEvents)
                    amask.write(mskfile.c_str());
                else
                    remove(mskfile.c_str());
                ibis::fileManager::instance().flushFile(mskfile.c_str());
            }
            LOGGER(ibis::gVerbose > 5)
                << "part::init -- mask for partition " << name() << " has "
                << amask.cnt() << " set bit" << (amask.cnt()>1?"s":"")
                << " out of " << amask.size();
        }
        catch (...) {
            LOGGER(ibis::gVerbose >= 0)
                << "Warning -- part::init cannot read mask file \""
                << mskfile << "\", assume all rows (" << nEvents
                << ") are active";
            amask.set(1, nEvents);
            remove(mskfile.c_str());
        }
    }

    // superficial checks
    j = 0;
    if (maxLength <= 0) maxLength = 16;
    if (std::strlen(activeDir)+16+maxLength > PATH_MAX) {
        LOGGER(ibis::gVerbose > 1)
            << "Warning -- directory name \"" << activeDir << "\" is too long";
        ++j;
    }
    if (backupDir != 0 && std::strlen(backupDir)+16+maxLength > PATH_MAX) {
        LOGGER(ibis::gVerbose > 1)
            << "Warning -- directory name \"" << backupDir << "\" is too long";
        ++j;
    }
    if (j) throw "part::init failed because direcotry names are too long"
               IBIS_FILE_LINE;

    myCleaner = new ibis::part::cleaner(this);
    ibis::fileManager::instance().addCleaner(myCleaner);

    if (ibis::gVerbose > 0 && m_name != 0) {
        ibis::util::logger lg;
        lg() << "Constructed ";
        if (nEvents == 0)
            lg() << "(empty) ";
        lg() << "part "
             << (m_name?m_name:"??");
        if (ibis::gVerbose > 1) {
            lg() << "\nactiveDir = \"" << activeDir << "\"";
            if (backupDir != 0 && *backupDir != 0)
                lg() << "\nbackupDir = \"" << backupDir << "\"";
        }
        if (ibis::gVerbose > 1 && columns.size() > 0 && nEvents > 0) {
            lg() << "\n";
            if (ibis::gVerbose > 3)
                print(lg());
            else
                lg() << "  " << nEvents << " row" << (nEvents>1 ? "s":"")
                     << " and " << columns.size() << " column"
                     << (columns.size()>1 ? "s" : "");
        }
    }
} // ibis::part::init

/// Read the meta tag entry in the header section of the metadata file in
/// directory dir.  The meta tags are name-value pairs associated with a
/// data partition.  They record information about about a data partition
/// that one might want to search through matchNameValuePair or
/// matchMetaTags or simply part of the regular query expressions.
///
/// @note This function only returns the first line of the meta tags in the
/// metadata file.
char* ibis::part::readMetaTags(const char* const dir) {
    char *s1;
    char* m_tags = 0;
    if (dir == 0)
        return m_tags;
    if (*dir == 0)
        return m_tags;

    char buf[MAX_LINE];
#if defined(HAVE_SNPRINTF)
    long ierr = UnixSnprintf(buf, MAX_LINE, "%s%c-part.txt", dir,
                             FASTBIT_DIRSEP);
#else
    long ierr = sprintf(buf, "%s%c-part.txt", dir, FASTBIT_DIRSEP);
#endif
    if (ierr < 2 || ierr > MAX_LINE) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- part::readMetaTags failed "
            "to generate the metadata file name";
        return m_tags;
    }

    FILE* file = fopen(buf, "r");
    if (file == 0) {
        strcpy(buf+(ierr-9), "table.tdc"); // old metadata file name
        file = fopen(buf, "r");
    }
    if (file == 0) {
        LOGGER(ibis::gVerbose > 2)
            << "part::readMetaTags could not find neither -part.txt "
            "nor table.tdc in \"" << dir << "\" ... "
            << (errno ? strerror(errno) : "no free stdio stream");
        return m_tags;
    }
    LOGGER(ibis::gVerbose > 4)
        << "part::readMetaTags -- opened " << buf;

    // skip till begin header
    while ((s1 = fgets(buf, MAX_LINE, file))) {
        if (strnicmp(buf, "BEGIN HEADER", 12) == 0) break;
    }

    // parse header -- read till end header
    while ((s1 = fgets(buf, MAX_LINE, file))) {
        LOGGER(std::strlen(buf) + 1 >= MAX_LINE && ibis::gVerbose > 1)
            << "Warning -- part::readMetaTags may have encountered a line "
            "that has more than " << MAX_LINE << " characters";
        LOGGER(ibis::gVerbose > 14) << buf;

        if (strnicmp(buf, "END HEADER", 10) == 0) {
            break;
        }
        else if (strnicmp(buf, "metaTags", 8) == 0 ||
                 strnicmp(buf, "part.metaTags", 13) == 0 ||
                 strnicmp(buf, "table.metaTags", 14) == 0 ||
                 strnicmp(buf, "DataSet.metaTags", 16) == 0 ||
                 strnicmp(buf, "partition.metaTags", 18) == 0) {
            s1 = strchr(buf, '=');
            if (s1!=0) {
                if (s1[1] != 0) {
                    ++ s1;
                    m_tags = ibis::util::getString(s1);
                }
            }
            break;
        }
    } // the loop to parse header

    fclose(file); // close the file

    return m_tags;
} // ibis::part::readMetaTags

// read the meta tag entry in the header section of the metadata file in
// directory dir
void ibis::part::readMeshShape(const char* const dir) {
    char *s1;
    if (dir == 0)
        return;
    if (*dir == 0)
        return;

    char buf[MAX_LINE];
#if defined(HAVE_SNPRINTF)
    long ierr = UnixSnprintf(buf, MAX_LINE, "%s%c-part.txt", dir,
                             FASTBIT_DIRSEP);
#else
    long ierr = sprintf(buf, "%s%c-part.txt", dir, FASTBIT_DIRSEP);
#endif
    if (ierr < 10 || ierr > MAX_LINE) {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- part::readMeshShape could not generate the name "
            "of the metadata file, likely because the file name is longer "
            "than " << MAX_LINE << " bytes";
        return;
    }

    FILE* file = fopen(buf, "r");
    if (file == 0) {
        strcpy(buf+(ierr-9), "table.tdc"); // old metadata file name
        file = fopen(buf, "r");
    }
    if (file == 0) {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- part::readMeshShape could not open file \"" << buf
            << "\" ... " << (errno ? strerror(errno) : "no free stdio stream");
        return;
    }
    LOGGER(ibis::gVerbose > 4)
        << "part::readMeshShape() opened " << buf;

    // skip till begin header
    while ((s1 = fgets(buf, MAX_LINE, file))) {
        if (strnicmp(buf, "BEGIN HEADER", 12) == 0) break;
    }

    // parse header -- read till end header
    while ((s1 = fgets(buf, MAX_LINE, file))) {
        LOGGER(std::strlen(buf) + 1 >= MAX_LINE && ibis::gVerbose > 0)
            << "Warning -- part::readMeshShape may have encountered a line "
            "with more than " << MAX_LINE << " characters";
        LOGGER(ibis::gVerbose > 14) << buf;

        if (strnicmp(buf, "END HEADER", 10) == 0) {
            break;
        }
        else if (strnicmp(buf, "columnShape", 11) == 0 ||
                 strnicmp(buf, "Part.columnShape", 16) == 0 ||
                 strnicmp(buf, "Table.columnShape", 17) == 0 ||
                 strnicmp(buf, "DataSet.columnShape", 19) == 0 ||
                 strnicmp(buf, "Partition.columnShape", 21) == 0 ||
                 strnicmp(buf, "meshShape", 9) == 0 ||
                 strnicmp(buf, "Part.meshShape", 14) == 0 ||
                 strnicmp(buf, "Partition.meshShape", 19) == 0) {
            s1 = strchr(buf, '(');
            if (s1 != 0) {
                if (s1[1] != 0) {
                    ++ s1;
                    digestMeshShape(s1);
                }
            }
            break;
        }
    } // the loop to parse header

    fclose(file); // close the file
} // ibis::part::readMeshShape

/// Read the metadata file from the named dir.  If dir is the activeDir,
/// it will also update the content of *this, otherwise it will only modify
/// arguments @c nrows and @c plist.  If this function completes
/// successfully, it returns the maximum length of the column names.
/// Otherwise, it returns a value of zero or less to indicate errors.
/// @remarks The metadata file is named "-part.txt".  It is a plain text
/// file with a fixed structure.  The prefix '-' is to ensure that none of
/// the data files could possibly have the same name (since '-' cann't
/// appear in any column names).  This file was previously named
/// "table.tdc" and this function still recognize this old name.
int ibis::part::readMetaData(uint32_t &nrows, columnList &plist,
                             const char* dir) {
    if (dir == 0 || *dir == 0) return -90;
#if DEBUG+0>0 || _DEBUG+0>1
    LOGGER(ibis::gVerbose > 0)
        << "DEBUG -- readMetaData called with dir=\"" << dir << "\"";
#endif
    // clear the content of plist
    for (columnList::iterator it = plist.begin(); it != plist.end(); ++it)
        delete (*it).second;
    plist.clear();
    nrows = 0;

    std::string tdcname(dir);
    tdcname += FASTBIT_DIRSEP;
    tdcname += "-part.txt";
    FILE* fptr = fopen(tdcname.c_str(), "r");
    if (fptr == 0) {
        tdcname.erase(tdcname.size()-9);
        tdcname += "table.tdc"; // the old name
        fptr = fopen(tdcname.c_str(), "r");
    }
    if (fptr == 0) {
        LOGGER(ibis::gVerbose > 2)
            << "part::readMetaData -- could not find neither -part.txt "
            "nor table.tdc in \"" << dir << "\" ... "
            << (errno ? strerror(errno) : "no free stdio stream");
        return -91;
    }
    LOGGER(ibis::gVerbose > 4)
        << "part::readMetaData -- opened " << tdcname << " for reading";

    char *s1;
    long ret;
    int  maxLength = 0;
    int  tot_columns = INT_MAX;
    int  num_columns = INT_MAX;
    const bool isActive =
        (activeDir ? std::strcmp(activeDir, dir) == 0 : false);
    std::set<int> selected; // list of selected columns
    char buf[MAX_LINE];

    // skip till begin header
    while ((s1 = fgets(buf, MAX_LINE, fptr))) {
        if (strnicmp(buf, "BEGIN HEADER", 12) == 0) break;
    }

    // parse header -- read till end header
    while ((s1 = fgets(buf, MAX_LINE, fptr))) {
        LOGGER(std::strlen(buf) + 1 >= MAX_LINE && ibis::gVerbose > 0)
            << "Warning -- part::readMetaData(" << tdcname
            << ") may have encountered a line that has more than "
            << MAX_LINE << " characters";
        LOGGER(ibis::gVerbose > 6) << buf;

        // s1 points to the value come after = sign
        s1 = strchr(buf, '=');
        if (s1!=0) {
            if (s1[1]!=0) {
                ++ s1;
            }
            else {
                s1 = 0;
            }
        }
        else {
            s1 = 0;
        }

        if (strnicmp(buf, "END HEADER", 10) == 0) {
            break;
        }
        else if (strnicmp(buf, "Number_of_rows", 14) == 0 ||
                 strnicmp(buf, "Number_of_events", 16) == 0 ||
                 strnicmp(buf, "Number_of_records", 17) == 0) {
            ret = strtol(s1, 0, 0);
            if (ret <= 0x7FFFFFFF) {
                nrows = ret;
                if (isActive)
                    nEvents = nrows;
            }
            else {
                LOGGER(ibis::gVerbose > 0)
                    << "Warning -- part::readMetaData got number_of_rows of "
                    << ret << ", which is more than 2 billion limit";
                nEvents = -1;
                return -92;
            }
        }
        else if (strnicmp(buf, "Number_of_columns", 17) == 0 ||
                 strnicmp(buf, "Number_of_properties", 20) == 0) {
            ret = strtol(s1, 0, 0);
            if (ret <= 0x7FFFFFFF) {
                num_columns = ret;
            }
            else {
                LOGGER(ibis::gVerbose > 0)
                    << "Warning -- part::readMetaData got number_of_columns of "
                    << ret << ", which is more than 2 billion limit";
                return -93;
            }
        }
        else if (strnicmp(buf, "Tot_num_of", 10) == 0) {
            ret = strtol(s1, 0, 0);
            if (ret <= 0x7FFFFFFF) {
                tot_columns = ret;
            }
            else {
                LOGGER(ibis::gVerbose > 0)
                    << "Warning -- part::readMetaData got "
                    "Tot_num_of_columns of "
                    << ret << ", which is more than 2 billion limit";
                return -94;
            }
        }
        else if (strnicmp(buf, "index", 5) == 0) {
            delete [] idxstr; // discard the old value
            idxstr = ibis::util::getString(s1);
#if defined(INDEX_SPEC_TO_LOWER)
            s1 = idxstr + std::strlen(idxstr) - 1;
            while (s1 >= idxstr) {
                *s1 = std::tolower(*s1);
                -- s1;
            }
#endif
            LOGGER(ibis::gVerbose > 1 && ibis::gVerbose <= 6) << buf;
        }
        else if (strnicmp(buf, "Bins:", 5) == 0) {
            delete [] idxstr; // discard teh old value
            s1 = buf + 5;
            idxstr = ibis::util::getString(s1);
#if defined(INDEX_SPEC_TO_LOWER)
            s1 = idxstr + std::strlen(idxstr) - 1;
            while (s1 >= idxstr) {
                *s1 = std::tolower(*s1);
                -- s1;
            }
#endif
            LOGGER(ibis::gVerbose > 1 && ibis::gVerbose <= 6) << buf;
        }
        else if (strnicmp(buf, "Columns_Selected", 16) == 0 ||
                 strnicmp(buf, "Properties_Selected", 19) == 0) {
            // the list can contain a list of ranges or numbers separated by
            // ',', ';', or space
            while (*s1 == 0) {
                char* s2;
                int i = strtol(s1, 0, 0);
                if (i > 0) {
                    selected.insert(i);
                }
                s2 = strchr(s1, '-');
                if (s2 != 0) {
                    s1 = s2 + 1;
                    int j = strtol(s1, 0, 0);
                    LOGGER(j < i && ibis::gVerbose > 0)
                        << "Warning -- readMetaData encounters "
                        "an illformed range: " << i << s2;
                    while (i<j) {
                        ++i;
                        selected.insert(i);
                    }
                }
                s2 = strpbrk(s1, ",; \t");
                if (s2 != 0) {
                    s1 = s2 + 1;
                }
                else {
                    s1 = 0;
                }
            }
            if (num_columns == INT_MAX) {
                num_columns = selected.size();
            }
        }
        else if (isActive) {
            if ((strnicmp(buf, "Name", 4) == 0 &&
                 (isspace(buf[4]) || buf[4]=='=')) ||
                strnicmp(buf, "Table.Name", 10) == 0 ||
                strnicmp(buf, "DataSet.Name", 12) == 0 ||
                strnicmp(buf, "Partition.Name", 14) == 0 ||
                strnicmp(buf, "Part.Name", 9) == 0) {
                delete [] m_name; // discard the existing name
                m_name = ibis::util::getString(s1);
            }
            else if (strnicmp(buf, "Description", 11) == 0 ||
                     strnicmp(buf, "Table.Description", 17) == 0 ||
                     strnicmp(buf, "DataSet.Description", 19) == 0 ||
                     strnicmp(buf, "Partition.Description", 21) == 0 ||
                     strnicmp(buf, "Part.Description", 16) == 0) {
                char *s2 = ibis::util::getString(s1);
                m_desc = s2;
                delete [] s2;
            }
            else if (strnicmp(buf, "Timestamp", 9) == 0) {
                if (sizeof(time_t) == sizeof(int))
                    switchTime = strtol(s1, 0, 0);
                else
                    switchTime = strtol(s1, 0, 0);
            }
            else if (strnicmp(buf, "Alternative_Directory", 21) == 0) {
                if (activeDir == 0 || *activeDir == 0 ||
                    backupDir == 0 || *backupDir == 0 ||
                    (std::strcmp(s1, activeDir) != 0 &&
                     std::strcmp(s1, backupDir) != 0)) {
                    delete [] backupDir;
                    backupDir = ibis::util::getString(s1);
                }
            }
            else if (strnicmp(buf, "State", 5) == 0 ||
                     strnicmp(buf, "Part.State", 10) == 0 ||
                     strnicmp(buf, "Table.State", 11) == 0 ||
                     strnicmp(buf, "DataSet.State", 13) == 0 ||
                     strnicmp(buf, "Partition.State", 15) == 0) {
                state = (ibis::part::TABLE_STATE)strtol(s1, 0, 0);
            }
            else if (strnicmp(buf, "metaTags", 8) == 0 ||
                     strnicmp(buf, "Part.metaTags", 13) == 0 ||
                     strnicmp(buf, "Table.metaTags", 14) == 0 ||
                     strnicmp(buf, "DataSet.metaTags", 16) == 0 ||
                     strnicmp(buf, "Partition.metaTags", 18) == 0) {
                ibis::resource::parseNameValuePairs(s1, metaList);
                ibis::resource::vList::const_iterator it =
                    metaList.find("columnShape");
                if (it == metaList.end())
                    it = metaList.find("meshShape");
                if (it != metaList.end())
                    digestMeshShape(it->second);
            }
            else if (strnicmp(buf, "columnShape", 11) == 0 ||
                     strnicmp(buf, "Part.columnShape", 16) == 0 ||
                     strnicmp(buf, "Table.columnShape", 17) == 0 ||
                     strnicmp(buf, "DataSet.columnShape", 19) == 0 ||
                     strnicmp(buf, "Partition.columnShape", 21) == 0 ||
                     strnicmp(buf, "meshShape", 9) == 0 ||
                     strnicmp(buf, "Part.meshShape", 14) == 0 ||
                     strnicmp(buf, "Partition.meshShape", 19) == 0) {
                digestMeshShape(s1);
                if (! shapeSize.empty())
                    metaList[ibis::util::strnewdup("meshShape")] =
                        ibis::util::strnewdup(s1);
            }
        }
    } // the loop to parse header

    // some minimal integrity check
    if ((uint32_t)num_columns != selected.size() && selected.size() > 0U) {
        ibis::util::logMessage
            ("Warning", "Properties_Positions_Selected field "
             "contains %lu elements,\nbut Number_of_columns "
             "field is %lu", static_cast<long unsigned>(selected.size()),
             static_cast<long unsigned>(num_columns));
        num_columns = selected.size();
    }
    if (tot_columns != INT_MAX &&
        tot_columns < num_columns) {
        ibis::util::logMessage
            ("Warning", "Tot_num_of_prop (%lu) is less than "
             "Number_of_columns(%lu", static_cast<long unsigned>(tot_columns),
             static_cast<long unsigned>(num_columns));
        tot_columns = INT_MAX;
    }

    // start to parse columns
    int len, cnt=0;
    while ((s1 = fgets(buf, MAX_LINE, fptr))) {
        // get to the next "Begin Column" line
        LOGGER(std::strlen(buf) + 1 >= MAX_LINE)
            << "Warning -- part::readMetaData(" << tdcname
            << ") may have encountered a line with more than " << MAX_LINE
            << " characters";

        if (strnicmp(buf, "Begin Column", 12) == 0 ||
            strnicmp(buf, "Begin Property", 14) == 0) {
            ++ cnt;
            column* prop = new column(this, fptr);
            LOGGER(ibis::gVerbose > 5)
                << "part::readMetaData -- got column " << prop->name()
                <<  " from " << tdcname;

            if (prop->type() == ibis::CATEGORY) {
                column* tmp = new ibis::category(*prop);
                delete prop;
                prop = tmp;
            }
            else if (prop->type() == ibis::TEXT) {
                column* tmp = new ibis::text(*prop);
                delete prop;
                prop = tmp;
            }
            else if (prop->type() == ibis::BLOB) {
                column* tmp = new ibis::blob(*prop);
                delete prop;
                prop = tmp;
            }

            if (selected.empty()) {
                // if Properties_Selected is not explicitly
                // specified, assume every column is to be included
                plist[prop->name()] = prop;
                len = std::strlen(prop->name());
                if (len > maxLength) maxLength = len;
            }
            else if (selected.find(cnt) != selected.end()) {
                // Properties_Positions_Selected is explicitly specified
                plist[prop->name()] = prop;
                len = std::strlen(prop->name());
                if (len > maxLength) maxLength = len;
            }
            else {
                // column is not selected
                delete prop;
            }
        }
    } // parse columns
    (void) fclose(fptr); // close the tdc file

    if ((uint32_t)num_columns != plist.size() && num_columns < INT_MAX) {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- part::readMetaData found " << plist.size()
            << " columns, but " << num_columns << " were expected";
    }
    if (cnt != tot_columns && tot_columns != INT_MAX) {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- part::readMetaData expects " << tot_columns
            << " column" << (tot_columns > 1 ? "s" : "")
            << " in the metadata file, but only " << cnt
            << " entr" << (cnt > 1 ? "ies" : "y") << " were found";
    }

    if (isActive && nrows > 0) {
        uint32_t mt = 0;
        // deal with the meta tags
        for (ibis::resource::vList::const_iterator mit = metaList.begin();
             mit != metaList.end();
             ++ mit) {
            columnList::iterator cit = plist.find((*mit).first);
            if (cit == plist.end()) { // need to add a new column
                ibis::category* prop =
                    new ibis::category(this, (*mit).first, (*mit).second,
                                       dir, nrows);
                plist[prop->name()] = prop;
                ++ mt;
            }
            else if (cit->second->type() != ibis::CATEGORY) {
                LOGGER(ibis::gVerbose > 1)
                    << "Warning -- part::readMetaData expects column "
                    << cit->first << " to be a CATEGORY, but it is "
                    << ibis::TYPESTRING[(int)cit->second->type()]
                    << ", regenerate the column for meta tag";
                plist.erase(cit);
                ibis::category* prop =
                    new ibis::category(this, (*mit).first, (*mit).second,
                                       dir, nrows);
                plist[prop->name()] = prop;
                ++ mt;
            }
            // else if (static_cast<ibis::category*>(cit->second)->getNumKeys()
            //       != 1) {
            //  LOGGER(ibis::gVerbose > 1)
            //      << "Warning -- part::readMetaData expectd meta tag "
            //      << cit->first << " to have only 1 value, but found "
            //      << static_cast<ibis::category*>(cit->second)->getNumKeys()
            //      << ", regenerating the column";
            //  plist.erase(cit);
            //  ibis::category* prop =
            //      new ibis::category(this, (*mit).first, (*mit).second,
            //                         dir, nrows);
            //  plist[prop->name()] = prop;
            // }
        }
        // try to assign the directory name as the part name
        if (m_name == 0 || *m_name == 0) {
            const char* cur = dir;
            const char* lst = dir;
            while (*cur != 0) {
                if (*cur == FASTBIT_DIRSEP)
                    lst = cur;
                ++ cur;
            }
            ++ lst;
            if (lst < cur) { // found a directory name to copy
                delete [] m_name;
                m_name = ibis::util::strnewdup(lst);
                if (*cur != 0) {
                    // the incoming dir ended with FASTBIT_DIRSEP, need to
                    // change it to null
                    len = cur - lst;
                    m_name[len-1] = 0;
                }
                ++ mt;
            }
        }

        switchTime = time(0); // record the current time
        if (mt > 0 && activeDir != 0 && *activeDir != 0) {
            // write an updated metadata file
            LOGGER(ibis::gVerbose > 1)
                << "part::readMetaData found " << mt << " meta tags not "
                "recorded as columns, writing new metadata file to " << dir;
            writeMetaData(nEvents, plist, activeDir);
            if (backupDir)
                writeMetaData(nEvents, plist, activeDir);
        }
    }
    return maxLength;
} // ibis::part::readMetaData

/// Write the metadata about the data partition into the ASCII file named
/// "-part.txt".  The caller is expected to hold a write lock on the data
/// partition to prevent simultaneous writes.
void ibis::part::writeMetaData(const uint32_t nrows, const columnList &plist,
                               const char* dir) const {
    if (dir == 0 || *dir == 0)
        return;
    const int nfn = std::strlen(dir)+16;
    char* filename = new char[nfn];
#if defined(HAVE_SNPRINTF)
    int ierr = UnixSnprintf(filename, nfn, "%s%c-part.txt", dir,
                            FASTBIT_DIRSEP);
#else
    int ierr = sprintf(filename, "%s%c-part.txt", dir, FASTBIT_DIRSEP);
#endif
    if (ierr < 10 || ierr > nfn) {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- part::writeMetaData could not generate the name "
            "of the metadata file, very unexpected";
        delete [] filename;
        return;
    }
    FILE *fptr = fopen(filename, "w");
    if (fptr == 0) {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- part::writeMetaData could not open file \""
            << filename << "\" for writing ... "
            << (errno ? strerror(errno) : "no free stdio stream");
        delete [] filename;
        return;
    }

    bool isActive = (activeDir != 0 ? (std::strcmp(activeDir, dir) == 0)
                     : false);
    bool isBackup = (backupDir != 0 ? (std::strcmp(backupDir, dir) == 0)
                     : false);
    char stamp[28];
    ibis::util::getLocalTime(stamp);
    fprintf(fptr, "# metadata file written by ibis::part::writeMetaData\n"
            "# on %s\n\n", stamp);
    if (m_name != 0) {
        fprintf(fptr, "BEGIN HEADER\nName = \"%s\"\n", m_name);
    }
    else { // make up a name based on the time stamp
        std::string nm;
        uint32_t tmp = ibis::util::checksum(stamp, std::strlen(stamp));
        ibis::util::int2string(nm, tmp);
        if (! isalpha(nm[0]))
            nm[0] = 'A' + (nm[0] % 26);
        fprintf(fptr, "BEGIN HEADER\nName = \"%s\"\n", nm.c_str());
    }
    if (!m_desc.empty() && (isActive || isBackup)) {
        fprintf(fptr, "Description = \"%s\"\n", m_desc.c_str());
    }
    else {
        fprintf(fptr, "Description = \"This table was created "
                "on %s with %lu rows and %lu columns.\"\n",
                stamp, static_cast<long unsigned>(nrows),
                static_cast<long unsigned>(plist.size()));
    }
    if (! metaList.empty()) {
        std::string mt = metaTags();
        fprintf(fptr, "metaTags = %s\n", mt.c_str());
    }

    fprintf(fptr, "Number_of_columns = %lu\n",
            static_cast<long unsigned>(plist.size()));
    fprintf(fptr, "Number_of_rows = %lu\n",
            static_cast<long unsigned>(nrows));
    if (shapeSize.size() > 0) {
        fprintf(fptr, "columnShape = (");
        for (uint32_t i = 0; i < shapeSize.size(); ++i) {
            if (i > 0)
                fprintf(fptr, ", ");
            if (shapeName.size() > i && ! shapeName[i].empty())
                fprintf(fptr, "%s=%lu", shapeName[i].c_str(),
                        static_cast<long unsigned>(shapeSize[i]));
            else
                fprintf(fptr, "%lu",
                        static_cast<long unsigned>(shapeSize[i]));
        }
        fprintf(fptr, ")\n");
    }
    if (isActive) {
        if (backupDir != 0 && *backupDir != 0 && backupDir != activeDir &&
            std::strcmp(activeDir, backupDir) != 0)
            fprintf(fptr, "Alternative_Directory = \"%s\"\n", backupDir);
    }
    else if (isBackup) {
        if (activeDir != 0 && *activeDir != 0 && backupDir != activeDir &&
            std::strcmp(activeDir, backupDir) != 0)
            fprintf(fptr, "Alternative_Directory = \"%s\"\n", activeDir);
    }
    if (isActive || isBackup) {
        fprintf(fptr, "Timestamp = %lu\n",
                static_cast<long unsigned>(switchTime));
        fprintf(fptr, "State = %d\n", (int)state);
    }
    if (idxstr != 0) {
        fprintf(fptr, "index = %s\n", idxstr);
    }
    fputs("END HEADER\n", fptr);

    for (columnList::const_iterator it = plist.begin();
         it != plist.end(); ++ it)
        (*it).second->write(fptr);
    fclose(fptr);
    LOGGER(ibis::gVerbose > 4)
        << "part[" << name() << "]::writeMetaData -- wrote metadata for "
        << nrows << " rows and " << plist.size() << " columns to \""
        << filename << "\"";
    delete [] filename;
} // ibis::part::writeMetaData

/// Write the metadata file to record the changes to the partition.
///
/// @note This function uses a soft write lock.  When a write lock can not
/// be acquired, it will not write the metadata to file -part.txt.
void ibis::part::updateMetaData() const {
    if (activeDir != 0 && *activeDir != 0) {
        softWriteLock lock(this, "updateMetaData");
        if (lock.isLocked()) {
            writeMetaData(nEvents, columns, activeDir);
        }
        else {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- part[" << name() << "]::updateMetaData failed "
                "to acquire a write lock, metadata file is not changed";
        }
    }
}

/// Digest the mesh shape stored in the string.  The shape can be just
/// numbers, e.g., "(10, 12, 14)", or 'name=value' pairs, e.g., "(nz=10,
/// ny=12, nx=14)".
///
/// @note This function uses a soft write lock.  When a write lock can not
/// be acquired, it will not write the metadata to file -part.txt.
void ibis::part::setMeshShape(const char *shape) {
    digestMeshShape(shape);

    if (activeDir != 0 && *activeDir != 0) {
        softWriteLock lock(this, "setMeshShape");
        if (lock.isLocked()) {
            writeMetaData(nEvents, columns, activeDir);
        }
        else {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- part[" << name() << "]::setMeshShape failed "
                "to acquire a write lock, metadata file is not changed";
        }
    }
}

/// copy the incoming as the mesh shape of the data partition.
void ibis::part::setMeshShape(const ibis::array_t<uint64_t>& ms) {
    shapeSize.resize(ms.size());
    for (unsigned j = 0; j < ms.size(); ++ j)
        shapeSize[j] = ms[j];
} // ibis::part::setMeshShape

/// Make a deep copy of the incoming name-value pairs.
void ibis::part::setMetaTags(const ibis::resource::vList &mts) {
    for (ibis::resource::vList::const_iterator it = mts.begin();
         it != mts.end();
         ++ it)
        metaList[ibis::util::strnewdup((*it).first)] =
            ibis::util::strnewdup((*it).second);
} // ibis::part::setMetaTags

/// Make a deep copy of the incoming name-value pairs.  The even element is
/// assumed to be the name and the odd element is assumed to be the value.
/// If the last name is not followed by a value it is assumed to have the
/// value of '*', which indicates do-not-care.
void ibis::part::setMetaTags(const std::vector<const char*> &mts) {
    ibis::resource::clear(metaList); // clear the current content
    const uint32_t len = mts.size();
    for (uint32_t i = 0; i+1 < len; i += 2) {
        metaList[ibis::util::strnewdup(mts[i])] =
            ibis::util::strnewdup(mts[i+1]);
    }
    if (len % 2 == 1) { // add name with a do-not-care value
        metaList[ibis::util::strnewdup(mts.back())] =
            ibis::util::strnewdup("*");
    }
} // ibis::part::setMetaTags

/// Output meta tags as a string.
std::string ibis::part::metaTags() const {
    std::string st;
    st.erase();
    for (ibis::resource::vList::const_iterator it = metaList.begin();
         it != metaList.end();
         ++ it) {
        if (! st.empty())
            st += ", ";
        st += (*it).first;
        st += " = ";
        st += (*it).second;
    }
    return st;
} // ibis::part::metaTags

/// Return true if the list of meta tags contains a name-value pair that
/// matches the input arguments.
bool ibis::part::matchNameValuePair(const char* name, const char* value)
    const {
    bool ret = false;
    if (name == 0) return ret;
    if (*name == 0) return ret;
    ibis::resource::vList::const_iterator it = metaList.find(name);
    if (it != metaList.end()) {
        if (value == 0) {
            ret = true;
        }
        else if (*value == 0) {
            ret = true;
        }
        else if ((value[0] == '*' && value[1] == 0) ||
                 ((*it).second[0] == '*' || (*it).second[0] == 0)) {
            ret = true;
        }
        else {
            ret = ibis::util::nameMatch((*it).second, value);
        }
    }
    return ret;
} // ibis::part::matchNameValuePair

bool
ibis::part::matchMetaTags(const std::vector<const char*>& mtags) const {
    bool ret = true;
    const uint32_t len = mtags.size();
    for (uint32_t i = 0; ret && (i+1 < len); i += 2) {
        ret = matchNameValuePair(mtags[i], mtags[i+1]);
    }
    return ret;
} // ibis::part::matchMetaTags

/// Return true if and only if the two vLists match exactly.
bool ibis::part::matchMetaTags(const ibis::resource::vList &mtags) const {
    const uint32_t len = mtags.size();
    bool ret = (metaList.size() == mtags.size());
    ibis::resource::vList::const_iterator it1 = mtags.begin();
    ibis::resource::vList::const_iterator it2 = metaList.begin();
    for (uint32_t i = 0; ret && (i < len); ++i, ++it1, ++it2) {
        ret = ((stricmp((*it1).first, (*it2).first) == 0) &&
               ((std::strcmp((*it1).second, "*")==0) ||
                (std::strcmp((*it2).second, "*")==0) ||
                (stricmp((*it1).second, (*it2).second)==0)));
        LOGGER(ibis::gVerbose > 5)
            << "util::matchMetaTags -- meta tags (" << it1->first << " = "
            << it1->second << ") and (" << it2->first << " = " << it2->second
            << ") " << (ret ? "match" : "donot match");;
    }
    return ret;
} // ibis::part::matchMetaTags

/// Digest the column shape string read from metadata file.
void ibis::part::digestMeshShape(const char *shape) {
    if (shape == 0) return;
    while (*shape && isspace(*shape))
        ++ shape;
    if (*shape == 0) return;

    // clear the current shape
    shapeSize.clear();
    shapeName.clear();

    const char* str = shape + strspn(shape, " \t(");
    while (str) {
        const char* tmp;
        std::string dname;
        if (isalpha(*str)) { // collect the name
            tmp = strchr(str, '=');
            while (str < tmp) { // copy every byte except space
                if (! isspace(*str))
                    dname += *str;
                ++ str;
            }
            str = tmp + strspn(tmp, " \t=");
        }
        while (*str && !std::isdigit(*str)) // skip everything not a digit
            ++ str;

        uint32_t dim = 0;
        if (*str)
            dim = strtol(str, 0, 0);
        if (dim > 0) { // record the name and value pair
            shapeSize.push_back(dim);
            shapeName.push_back(dname);
        }
        if (*str) {
            tmp = strpbrk(str, " \t,;");
            if (tmp)
                str = tmp + strspn(tmp, " \t,;");
            else
                str = 0;
        }
        else {
            str = 0;
        }
    }

    if (ibis::gVerbose > 6) {
        std::ostringstream ostr;
        for (uint32_t i = 0; i < shapeSize.size(); ++i) {
            if (i > 0)
                ostr << ", ";
            if (shapeName.size() > i && ! shapeName[i].empty())
                ostr << shapeName[i] << "=";
            ostr << shapeSize[i];
        }
        logMessage("digestMeshShape", "converted string \"%s\" to "
                   "shape (%s)", shape, ostr.str().c_str());
    }
} // ibis::part::digestMeshShape

void ibis::part::combineNames(ibis::table::namesTypes &metalist) const {
    for (ibis::part::columnList::const_iterator cit = columns.begin();
         cit != columns.end(); ++ cit) {
        const ibis::column* col = (*cit).second;
        ibis::table::namesTypes::const_iterator nit =
            metalist.find((*cit).first);
        if (nit == metalist.end()) { // a new column
            metalist[col->name()] = col->type();
        }
        else if ((*nit).second != col->type()) { // type must match
            logWarning("combineNames", "column %s is of type \"%s\", "
                       "but it is type \"%s\" in the combined list",
                       col->name(), ibis::TYPESTRING[(int)col->type()],
                       ibis::TYPESTRING[(int)(*nit).second]);
        }
    }
} // ibis::part::combineNames

/// Return column names in a list.  The list contains raw pointers.  These
/// pointers are valid as long as the part objects are present in memory.
/// The names are in the same order as specified during the construction of
/// the data partition.  If no order was specified at that time, the
/// columns are in alphabetical order.
ibis::table::stringArray ibis::part::columnNames() const {
    ibis::table::stringArray res(columns.size());
    if (columns.empty()) return res;

    if (colorder.empty()) {
        res.clear();
        for (ibis::part::columnList::const_iterator it = columns.begin();
             it != columns.end(); ++ it) {
            res.push_back((*it).first);
        }
    }
    else if (colorder.size() == columns.size()) {
        for (uint32_t i = 0; i < columns.size(); ++ i)
            res[i] = colorder[i]->name();
    }
    else {
        std::set<const char*, ibis::lessi> names;
        for (ibis::part::columnList::const_iterator it = columns.begin();
             it != columns.end(); ++ it)
            names.insert((*it).first);
        res.resize(colorder.size());
        for (uint32_t i = 0; i < colorder.size(); ++ i) {
            res[i] = colorder[i]->name();
            names.erase(colorder[i]->name());
        }
        for (std::set<const char*, ibis::lessi>::const_iterator it =
                 names.begin(); it != names.end(); ++ it) {
            res.push_back(*it);
        }
    }
    return res;
} // ibis::part::columnNames

/// Return column types in a list.
ibis::table::typeArray ibis::part::columnTypes() const {
    ibis::table::typeArray res(columns.size());
    if (columns.empty()) return res;

    if (colorder.empty()) {
        res.clear();
        for (ibis::part::columnList::const_iterator it = columns.begin();
             it != columns.end(); ++ it) {
            res.push_back((*it).second->type());
        }
    }
    else if (colorder.size() == columns.size()) {
        for (uint32_t i = 0; i < columns.size(); ++ i)
            res[i] = colorder[i]->type();
    }
    else {
        std::set<const char*, ibis::lessi> names;
        for (ibis::part::columnList::const_iterator it = columns.begin();
             it != columns.end(); ++ it)
            names.insert((*it).first);
        res.resize(colorder.size());
        for (uint32_t i = 0; i < colorder.size(); ++ i) {
            res[i] = colorder[i]->type();
            names.erase(colorder[i]->name());
        }
        for (std::set<const char*, ibis::lessi>::const_iterator it =
                 names.begin(); it != names.end(); ++ it) {
            ibis::part::columnList::const_iterator cit = columns.find(*it);
            res.push_back(cit->second->type());
        }
    }
    return res;
} // ibis::part::columnTypes

/// Print the basic information to the specified output stream.
void ibis::part::print(std::ostream &out) const {
    //    readLock lock(this, "print");
    if (m_name == 0) return;
    out << "part: " << name();
    if (! m_desc.empty())
        out << " (" << m_desc << ")";
    if (rids != 0 && rids->size() > 0)
        out << " with " << rids->size() << " row" << (rids->size()>1?"s":"")
            << ", " << columns.size() << " column" << (columns.size()>1?"s":"");
    else
        out << " with " << nEvents << " row" << (nEvents>1?"s":"") << ", "
            << columns.size() << " column" << (columns.size()>1?"s":"");
    if (columns.size() > 0) {
        out << "\nColumn list:";
        if (colorder.empty()) {
            for (columnList::const_iterator it = columns.begin();
                 it != columns.end(); ++it) {
                out << "\n" << *((*it).second);
            }
        }
        else if (colorder.size() == columns.size()) {
            for (uint32_t i = 0; i < columns.size(); ++ i)
                out << "\n" << colorder[i]->name();
        }
        else {
            std::set<const char*, ibis::lessi> names;
            for (ibis::part::columnList::const_iterator it = columns.begin();
                 it != columns.end(); ++ it)
                names.insert((*it).first);
            for (uint32_t i = 0; i < colorder.size(); ++ i) {
                out << "\n" << colorder[i]->name();
                names.erase(colorder[i]->name());
            }
            for (std::set<const char*, ibis::lessi>::const_iterator it =
                     names.begin(); it != names.end(); ++ it)
                out << "\n" << *it;
        }
    }
    out << std::endl;
} // ibis::part::print

/// A function to retrieve RIDs stored in file.
void ibis::part::readRIDs() const {
    if (activeDir == 0) return;

    readLock lock(this, "readRIDs");
    if (rids != 0 && rids->size() == nEvents)
        return;

    ibis::util::mutexLock mtx(&mutex, "part::readRIDs");
    delete rids;
    rids = new array_t<ibis::rid_t>;

    std::string fn(activeDir);
    fn += FASTBIT_DIRSEP;
    fn += "-rids";
    if (ibis::fileManager::instance().getFile(fn.c_str(), *rids)) {
        LOGGER(ibis::gVerbose > 4)
            << "part[" << name() << "]::readRIDs -- the file manager "
            "could not read file \"" << fn << "\".  There is no RIDs.";
        rids->clear();
    }
    LOGGER(nEvents != rids->size() && rids->size() > 0 && ibis::gVerbose > 2)
        << "Warning -- part[" << name() << "]::readRIDs -- nEvents ("
        << nEvents << ") is different from the number of RIDs ("
        << rids->size() << ")";
} // ibis::part::readRIDs

/// Attempt to free the RID column.
/// Complete the task if it can acquire a write lock on the
/// ibis::part object.  Otherwise, the rids will be left unchanged.
void ibis::part::freeRIDs() const {
    if (rids != 0) {
        softWriteLock lock(this, "freeRIDs");
        if (lock.isLocked()) {
            // only perform deletion if it actually acquired a write lock
            delete rids;
            rids = 0;
        }
        else {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- part[" << name() << "]::freeRIDs failed "
                "to acquire a write lock, metadata file is not changed";
        }
    }
} // ibis::part::freeRIDs

// generate arbitrary RIDs to that we can function correctly
void ibis::part::fillRIDs(const char* fn) const {
    if (nEvents == 0) return; // can not do anything

    FILE* rf = fopen(fn, "wb");
    std::string sfile(fn);
    sfile += ".srt";
    FILE* sf = fopen(sfile.c_str(), "wb");

    uint32_t ir = ibis::fileManager::iBeat();
    rid_t tmp;
    tmp.num.run = ir;
    tmp.num.event = 0;
    rids->resize(nEvents);
    if (rf && sf) {
        for (uint32_t i = 0; i < nEvents; ++i) {
            ++ tmp.value;
            (*rids)[i].value = tmp.value;
            fwrite(&((*rids)[i].value), sizeof(rid_t), 1, rf);
            fwrite(&((*rids)[i].value), sizeof(rid_t), 1, sf);
            fwrite(&i, sizeof(uint32_t), 1, sf);
        }
        fclose(rf);
        fclose(sf);
    }
    else {
        if (rf) fclose(rf);
        if (sf) fclose(sf);
        for (uint32_t i = 0; i < nEvents; ++i) {
            ++ tmp.value;
            (*rids)[i].value = tmp.value;
        }
    }
} // ibis::part::fillRIDs

/// Generate a sorted version of the RIDs and stored the result in -rids.srt.
void ibis::part::sortRIDs() const {
    if (activeDir == 0 && rids == 0) return;

    char name[PATH_MAX];
    ibis::util::mutexLock lck(&mutex, "part::sortRIDs");
    //ibis::util::mutexLock lck(&ibis::util::envLock, "sortRIDs");
    sprintf(name, "%s%c-rids.srt", activeDir, FASTBIT_DIRSEP);
    uint32_t sz = ibis::util::getFileSize(name);
    if (sz == nEvents*(sizeof(rid_t)+sizeof(uint32_t)))
        return;
    if (sz > 0) {
        // remove it from the file cache
        ibis::fileManager::instance().flushFile(name);
        remove(name); // remove the existing (wrong-sized) file
    }

    typedef std::map< const ibis::rid_t*, uint32_t,
        std::less<const ibis::rid_t*> > RIDmap;
    RIDmap rmap;
    ibis::horometer timer;
    timer.start();
    // insert all rids into the RIDmap
    for (uint32_t i=0; i<nEvents; ++i)
        rmap[&(*rids)[i]] = i;
    if (rids->size() != rmap.size()) {
        logWarning("sortRIDs",
                   "There are %lu unique RIDs out of %lu total RIDs",
                   static_cast<long unsigned>(rmap.size()),
                   static_cast<long unsigned>(rids->size()));
    }

    // write the sorted rids into -rids.srt
    uint32_t buf[2];
    int fdes = UnixOpen(name, OPEN_WRITENEW, OPEN_FILEMODE);
    if (fdes < 0) {
        logWarning("sortRIDs", "could not open file %s for writing ... %s",
                   name, (errno ? strerror(errno) : "no free stdio stream"));
        return;
    }
    IBIS_BLOCK_GUARD(UnixClose, fdes);
#if defined(_WIN32) && defined(_MSC_VER)
    (void)_setmode(fdes, _O_BINARY);
#endif

    const uint32_t nbuf = sizeof(buf);
    for (RIDmap::const_iterator it = rmap.begin(); it != rmap.end(); ++it) {
        buf[0] = (*it).first->num.run;
        buf[1] = (*it).first->num.event;
        off_t ierr = UnixWrite(fdes, buf, nbuf);
        ierr += UnixWrite(fdes, &((*it).second), sizeof(uint32_t));
        if (ierr <= 0 ||
            static_cast<uint32_t>(ierr) != nbuf+sizeof(uint32_t)) {
            logWarning("sortRIDs", "could not write run (%lu, %lu, %lu) to "
                       "file %s", static_cast<long unsigned>(buf[0]),
                       static_cast<long unsigned>(buf[1]),
                       static_cast<long unsigned>((*it).second), name);
            remove(name);
            return;
        }
    }
    if (ibis::gVerbose > 4) {
        timer.stop();
        logMessage("sortRIDs", "sorting %lu RIDs took  %g sec(CPU), %g "
                   "sec(elapsed); result written to %s",
                   static_cast<long unsigned>(rmap.size()),
                   timer.CPUTime(), timer.realTime(), name);
    }
} // ibis::part::sortRIDs

/// It tries the sorted RID list first.  If that fails, it uses the brute
/// force searching algorithm.
uint32_t ibis::part::getRowNumber(const ibis::rid_t &rid) const {
    uint32_t ind = searchSortedRIDs(rid);
    if (ind >= nEvents)
        ind = searchRIDs(rid);
    return ind;
} // ibis::part::getRowNumber

/// Use file -rids.srt to search for the rid.
uint32_t ibis::part::searchSortedRIDs(const ibis::rid_t &rid) const {
    uint32_t ind = nEvents;
    if (activeDir == 0) return ind;

    char name[PATH_MAX];
    // NOTE
    // really should not use the following lock, however, it appears that
    // g++ 2.95.2 is not generating the correct code to deallocate ridx at
    // the end of the function call.  This mutex lock helps to resolve the
    // problem.
    //mutexLock lock(this, "searchSortedRIDs");
    sprintf(name, "%s%c-rids.srt", activeDir, FASTBIT_DIRSEP);
    array_t<uint32_t> ridx;
    int ierr = ibis::fileManager::instance().getFile(name, ridx);
    if (ierr != 0) {
        sortRIDs(); // generate -rids.srt file from rids
        ierr = ibis::fileManager::instance().getFile(name, ridx);
        if (ierr != 0) {
            logWarning("searchSortedRIDs",
                       "cound not generate -rids.srt (%s)",
                       name);
            return ind;
        }
    }
    if (ridx.size() < 3)
        return ind;

    // binary search
    uint32_t lower = 0, upper = ridx.size() / 3;
    while (lower < upper) {
        ind = (lower + upper) / 2;
        uint32_t ind3 = ind*3;
        if (rid.num.run < ridx[ind3]) {
            upper = ind;
        }
        else if (rid.num.run > ridx[ind3]) {
            if (ind == lower) break;
            lower = ind;
        }
        else if (rid.num.event < ridx[ind3+1]) {
            upper = ind;
        }
        else if (rid.num.event > ridx[ind3+1]) {
            if (ind == lower) break;
            lower = ind;
        }
        else {
            ind = ridx[ind3+2];
#if DEBUG+0 > 1 || _DEBUG+0 > 1
            logMessage("searchSortedRIDs", "RID(%lu, %lu) ==> %lu",
                       static_cast<long unsigned>(rid.num.run),
                       static_cast<long unsigned>(rid.num.event),
                       static_cast<long unsigned>(ind));
#endif
            return ind;
        }
    }

#if DEBUG+0 > 1 || _DEBUG+0 > 1
    logMessage("searchSortedRIDs", "can not find RID(%lu, %lu)",
               static_cast<long unsigned>(rid.num.run),
               static_cast<long unsigned>(rid.num.event));
#endif
    ind = nEvents;
    return ind;
} // ibis::part::searchSortedRIDs

/// A brute-force search for the rid.
uint32_t ibis::part::searchRIDs(const ibis::rid_t &rid) const {
    uint32_t i = nEvents;
    for (i=0; i<nEvents; ++i) {
        if ((*rids)[i].value == rid.value) {
#if DEBUG+0 > 1 || _DEBUG+0 > 1
            logMessage("searchRIDs", "RID(%lu, %lu) ==> %lu",
                       static_cast<long unsigned>(rid.num.run),
                       static_cast<long unsigned>(rid.num.event),
                       static_cast<long unsigned>(i));
#endif
            return i;
        }
    }
#if DEBUG+0 > 1 || _DEBUG+0 > 1
    logMessage("searchRIDs", "can not find RID(%lu, %lu)",
               static_cast<long unsigned>(rid.num.run),
               static_cast<long unsigned>(rid.num.event));
#endif
    return i;
} // ibis::part::searchRIDs

/// Use file -rids.srt to search for the rid.
/// Assume the incoming RIDs are sorted.
void ibis::part::searchSortedRIDs(const ibis::RIDSet &in,
                                  ibis::bitvector &res) const {
    if (activeDir == 0) return;
    char name[PATH_MAX];
    sprintf(name, "%s%c-rids.srt", activeDir, FASTBIT_DIRSEP);
    array_t<uint32_t> ridx;
    int ierr = ibis::fileManager::instance().getFile(name, ridx);
    if (ierr != 0) {
        sortRIDs(); // generate -rids.srt file from rids
        ierr = ibis::fileManager::instance().getFile(name, ridx);
        if (ierr != 0) {
            logWarning("searchSortedRIDs",
                       "could not generate -rids.srt (%s)",
                       name);
            searchRIDs(in, res);
            return;
        }
    }
    if (ridx.size() != 3*nEvents) {
        // Even though we have read the -rids.srt file correctly, but the
        // file is not the right size.  Need a way to delete the references
        // to the file -rids.srt.
        array_t<uint32_t> *tmp = new array_t<uint32_t>;
        tmp->swap(ridx);
        delete tmp;

        searchRIDs(in, res);
        return;
    }
    if (in.size() > 100) {
        res.set(0, nEvents);
        res.decompress();
    }
    else {
        res.clear();
    }

    // matching two sorted lists
    uint32_t i0 = 0, i1 = 0;
    while (i0 < 3*nEvents && i1 < in.size()) {
        if (in[i1].num.run > ridx[i0]) {
            i0 += 3;
        }
        else if (in[i1].num.run < ridx[i0]) {
            ++ i1;
        }
        else if (in[i1].num.event > ridx[i0+1]) {
            i0 += 3;
        }
        else if (in[i1].num.event < ridx[i0+1]) {
            ++ i1;
        }
        else { // two RIDs are the same
            res.setBit(ridx[i0+2], 1);
#if DEBUG+0 > 1 || _DEBUG+0 > 1
            logMessage("searchSortedRIDs", "RID(%lu, %lu) ==> %lu",
                       static_cast<long unsigned>(ridx[i0]),
                       static_cast<long unsigned>(ridx[i0+1]),
                       static_cast<long unsigned>(ridx[i0+2]));
#endif
            i0 += 3;
            ++ i1;
        }
    }

    res.compress();
    res.adjustSize(0, nEvents);
} // ibis::part::searchSortedRIDs

/// A brute-force search for the rids.
/// Assume the incoming RIDs are sorted.
void ibis::part::searchRIDs(const ibis::RIDSet &in,
                            ibis::bitvector &res) const {
    uint32_t i = nEvents, cnt = 0;
    if (in.size() > 100) {
        res.set(0, nEvents);
        res.decompress();
    }
    else
        res.clear();
    for (i=0; i<nEvents && cnt<in.size(); ++i) {
        ibis::RIDSet::const_iterator it = std::find(in.begin(), in.end(),
                                                    (*rids)[i]);
        if (it != in.end()) { // found one
#if DEBUG+0 > 1 || _DEBUG+0 > 1
            logMessage("searchRIDs", "RID(%lu, %lu) ==> %lu",
                       static_cast<long unsigned>((*it).num.run),
                       static_cast<long unsigned>((*it).num.event),
                       static_cast<long unsigned>(i));
#endif
            res.setBit(i, 1);
        }
    }

    res.compress();
    res.adjustSize(0, nEvents);
} // ibis::part::searchRIDs

/// Retrieve the RIDs corresponding to mask[i] == 1.  If no external row
/// identifers are provided, this function will use the implicit RIDs which
/// are simply the positions of the rows numbered from 0 to nRows()-1.
ibis::array_t<ibis::rid_t>*
ibis::part::getRIDs(const ibis::bitvector &mask) const {
    const uint32_t cnt = mask.cnt();
    ibis::array_t<ibis::rid_t>* ret = new array_t<ibis::rid_t>;
    if (cnt == 0)
        return ret;
    if (rids == 0)
        readRIDs(); // attempt to read the file rids
    // else if (rids->size() == 0)
    //  readRIDs();

    ret->reserve(cnt);
    ibis::bitvector::indexSet ind = mask.firstIndexSet();
    if (rids != 0 && rids->size() > 0) { // has rids specified
        readLock lock(this, "getRIDs");
        const uint32_t nmask = mask.size();
        const uint32_t nrids = rids->size();
        LOGGER(nrids != nmask && ibis::gVerbose > 1)
            << "Warning -- part[" << name()
            << "]::getRIDs found the number of RIDs (" << nrids
            << ") to be different from the size of the mask ("
            << nmask << ')';
        if (nrids >= nmask) { // more RIDS than mask bits
            while (ind.nIndices()) {    // copy selected elements
                const uint32_t nind = ind.nIndices();
                const ibis::bitvector::word_t *idx = ind.indices();
                if (ind.isRange()) {
                    for (uint32_t j = *idx; j < idx[1]; ++j)
                        ret->push_back((*rids)[j]);
                }
                else {
                    for (uint32_t j = 0; j < nind; ++j)
                        ret->push_back((*rids)[idx[j]]);
                }
                ++ ind;
            }
        }
        else { // less RIDS than mask bits, need to check loop indices
            while (ind.nIndices()) {    // copy selected elements
                const uint32_t nind = ind.nIndices();
                const ibis::bitvector::word_t *idx = ind.indices();
                if (*idx >= nrids) {
                    return ret;
                }
                if (ind.isRange()) {
                    for (uint32_t j = *idx;
                         j < (idx[1]<=nrids ? idx[1] : nrids);
                         ++j)
                        ret->push_back((*rids)[j]);
                }
                else {
                    for (uint32_t j = 0; j < nind; ++j) {
                        if (idx[j] < nrids)
                            ret->push_back((*rids)[idx[j]]);
                        else
                            break;
                    }
                }
                ++ ind;
            }
        }
    }
    else { // use the row numbers as RIDs
        while (ind.nIndices()) {
            const uint32_t nind = ind.nIndices();
            const ibis::bitvector::word_t *idx = ind.indices();
            rid_t tmp;
            if (ind.isRange()) {
                for (uint32_t j = *idx; j < idx[1]; ++j) {
                    tmp.value = j;
                    ret->push_back(tmp);
                }
            }
            else {
                for (uint32_t j = 0; j < nind; ++j) {
                    tmp.value = idx[j];
                    ret->push_back(tmp);
                }
            }
            ++ ind;
        }
    }

    LOGGER(ret->size() != cnt && ibis::gVerbose > 0)
        << "Warning -- part[" << name() << "]::getRIDs expected to get "
        << cnt << " RIDs, but actually got " << ret->size();
    return ret;
} // ibis::part::getRIDs

/// Assuming the pages are packed with values of wordsize bytes, this
/// function examines the given bitvector to determine the number of pages
/// would be accessed in order to read all the positions marked 1.  The
/// page size is determine by the function ibis::fileManager::pageSize.
uint32_t ibis::part::countPages(const ibis::bitvector &mask,
                                unsigned wordsize) {
    uint32_t res = 0;
    if (mask.cnt() == 0)
        return res;
    if (wordsize == 0)
        return res;

    // words per page
    const uint32_t wpp = ibis::fileManager::pageSize() / wordsize;
    uint32_t last;  // the position of the last entry encountered
    ibis::bitvector::indexSet ix = mask.firstIndexSet();
    last = *(ix.indices());
    if (ibis::gVerbose < 8) {
        while (ix.nIndices() > 0) {
            const ibis::bitvector::word_t *ind = ix.indices();
            const uint32_t p0 = *ind / wpp;
            res += (last < p0*wpp); // last not on the current page
            if (ix.isRange()) {
                res += (ind[1] / wpp - p0);
                last = ind[1];
            }
            else {
                last = ind[ix.nIndices()-1];
                res += (last / wpp > p0);
            }
            ++ ix;
        }
    }
    else {
        ibis::util::logger lg;
        lg() << "part::countPages(" << wordsize
             << ") page numbers: ";
        while (ix.nIndices() > 0) {
            const ibis::bitvector::word_t *ind = ix.indices();
            const uint32_t p0 = *ind / wpp;
            if (last < p0*wpp) { // last not on the current page
                lg() << last/wpp << " ";
                ++ res;
            }
            if (ix.isRange()) {
                const unsigned mp = ind[1]/wpp - p0;
                if (mp > 1)
                    lg() << p0 << "*" << mp << " ";
                else if (mp > 0)
                    lg() << p0 << " ";
                last = ind[1];
                res += mp;
            }
            else {
                last = ind[ix.nIndices()-1];
                if (last / wpp > p0) {
                    lg() << p0 << " ";
                    ++ res;
                }
            }
            ++ ix;
        }
    }
    if (res == 0)
        res = 1;
    return res;
} // ibis::part::countPages

ibis::fileManager::ACCESS_PREFERENCE
ibis::part::accessHint(const ibis::bitvector &mask, unsigned elem) const {
    ibis::fileManager::ACCESS_PREFERENCE hint =
        ibis::fileManager::MMAP_LARGE_FILES;
    uint32_t cnt = mask.cnt();
    if (elem == 0 || mask.size() == 0 || cnt >= (nEvents >> 3))
        return hint;

    const uint32_t npages = static_cast<uint32_t>
        (ceil(static_cast<double>(nEvents) * elem
              / ibis::fileManager::pageSize()));
    // selecting very few values or very many values, use default
    if (cnt < (npages >> 4) || (cnt >> 5) > npages)
        return hint;

    // count the number of pages, get first and last page number
    const uint32_t wpp = ibis::fileManager::pageSize() / elem;
    uint32_t first; // first page number
    uint32_t last;  // the position of the last entry encountered
    cnt = 0; // the number of pages to be accessed
    ibis::bitvector::indexSet ix = mask.firstIndexSet();
    last = *(ix.indices());
    first = *(ix.indices()) / wpp;
    while (ix.nIndices() > 0) {
        const ibis::bitvector::word_t *ind = ix.indices();
        const uint32_t p0 = *ind / wpp;
        cnt += (last < p0*wpp); // last not on the current page
        if (ix.isRange()) {
            cnt += (ind[1] / wpp - p0);
            last = ind[1];
        }
        else {
            last = ind[ix.nIndices()-1];
            cnt += (last / wpp > p0);
        }
        ++ ix;
    }
    last /= wpp; // the last page number
    if (cnt > 24 && (cnt+cnt >= last-first || last-first <= (npages >> 3))) {
        // pages to be accessed are concentrated
        hint = ibis::fileManager::PREFER_MMAP;
    }
    else if (cnt > (npages >> 4)) {
        // more than 1/16th of the pages will be accessed
        hint = ibis::fileManager::PREFER_READ;
    }
    if (ibis::gVerbose > 4)
        logMessage("accessHint", "nRows=%lu, selected=%lu, #pages=%lu, "
                   "first page=%lu, last page=%lu, hint=%s",
                   static_cast<long unsigned>(nRows()),
                   static_cast<long unsigned>(mask.cnt()),
                   static_cast<long unsigned>(cnt),
                   static_cast<long unsigned>(first),
                   static_cast<long unsigned>(last),
                   (hint == ibis::fileManager::MMAP_LARGE_FILES ?
                    "MMAP_LARGE_FILES" :
                    hint == ibis::fileManager::PREFER_READ ?
                    "PREFER_READ" : "PREFER_MMAP"));
    return hint;
} // ibis::part::accessHint

/// The selected values are packed into the resulting array.  Only those
/// rows marked 1 are retrieved.  The caller is responsible for deleting
/// the returned value.
ibis::array_t<signed char>* ibis::part::selectBytes
(const char* pname, const ibis::bitvector &mask) const {
    ibis::array_t<signed char>* res = 0;
    try {
        const ibis::column* col = getColumn(pname);
        if (col != 0) { // got it
            res =  col->selectBytes(mask);
        }
    }
    catch (const std::exception &e) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- part[" << (m_name ? m_name : "")
            << "]::selectBytes(" << (pname ? pname : "") << ") with mask("
            << mask.cnt() << " out of " << mask.size()
            << ") received the following std::exception -- " << e.what();
        ibis::util::emptyCache();
        delete res;
        res = 0;
    }
    catch (const char* s) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- part[" << (m_name ? m_name : "")
            << "]::selectBytes(" << (pname ? pname : "") << ") with mask("
            << mask.cnt() << " out of " << mask.size()
            << ") received the following string exception -- " << s;
        ibis::util::emptyCache();
        delete res;
        res = 0;
    }
    catch (...) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- part[" << (m_name ? m_name : "")
            << "]::selectBytes(" << (pname ? pname : "") << ") with mask("
            << mask.cnt() << " out of " << mask.size()
            << ") received a unexpected exception";
        ibis::util::emptyCache();
        delete res;
        res = 0;
    }

    return res;
} // ibis::part::selectBytes

/// The selected values are packed into the resulting array.  Only those
/// rows marked 1 are retrieved.  The caller is responsible for deleting
/// the returned value.
ibis::array_t<unsigned char>* ibis::part::selectUBytes
(const char* pname, const ibis::bitvector &mask) const {
    ibis::array_t<unsigned char>* res = 0;
    try {
        const ibis::column *col = getColumn(pname);
        if (col != 0) { // got it
            res =  col->selectUBytes(mask);
        }
    }
    catch (const std::exception &e) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- part[" << (m_name ? m_name : "")
            << "]::selectUBytes(" << (pname ? pname : "") << ") with mask("
            << mask.cnt() << " out of " << mask.size()
            << ") received the following std::exception -- " << e.what();
        ibis::util::emptyCache();
        delete res;
        res = 0;
    }
    catch (const char* s) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- part[" << (m_name ? m_name : "")
            << "]::selectUBytes(" << (pname ? pname : "") << ") with mask("
            << mask.cnt() << " out of " << mask.size()
            << ") received the following string exception -- " << s;
        ibis::util::emptyCache();
        delete res;
        res = 0;
    }
    catch (...) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- part[" << (m_name ? m_name : "")
            << "]::selectUBytes(" << (pname ? pname : "") << ") with mask("
            << mask.cnt() << " out of " << mask.size()
            << ") received a unexpected exception";
        ibis::util::emptyCache();
        delete res;
        res = 0;
    }

    return res;
} // ibis::part::selectUBytes

/// The selected values are packed into the resulting array.  Only those
/// rows marked 1 are retrieved.  The caller is responsible for deleting
/// the returned value.
ibis::array_t<int16_t>* ibis::part::selectShorts
(const char* pname, const ibis::bitvector &mask) const {
    ibis::array_t<int16_t>* res = 0;
    try {
        const ibis::column* col = getColumn(pname);
        if (col != 0) { // got it
            res =  col->selectShorts(mask);
        }
    }
    catch (const std::exception &e) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- part[" << (m_name ? m_name : "")
            << "]::selectShorts(" << (pname ? pname : "") << ") with mask("
            << mask.cnt() << " out of " << mask.size()
            << ") received the following std::exception -- " << e.what();
        ibis::util::emptyCache();
        delete res;
        res = 0;
    }
    catch (const char* s) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- part[" << (m_name ? m_name : "")
            << "]::selectShorts(" << (pname ? pname : "") << ") with mask("
            << mask.cnt() << " out of " << mask.size()
            << ") received the following string exception -- " << s;
        ibis::util::emptyCache();
        delete res;
        res = 0;
    }
    catch (...) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- part[" << (m_name ? m_name : "")
            << "]::selectShorts(" << (pname ? pname : "") << ") with mask("
            << mask.cnt() << " out of " << mask.size()
            << ") received a unexpected exception";
        ibis::util::emptyCache();
        delete res;
        res = 0;
    }

    return res;
} // ibis::part::selectShorts

/// The selected values are packed into the resulting array.  Only those
/// rows marked 1 are retrieved.  The caller is responsible for deleting
/// the returned value.
ibis::array_t<uint16_t>* ibis::part::selectUShorts
(const char* pname, const ibis::bitvector &mask) const {
    ibis::array_t<uint16_t>* res = 0;
    try {
        const ibis::column* col = getColumn(pname);
        if (col != 0) { // got it
            res =  col->selectUShorts(mask);
        }
    }
    catch (const std::exception &e) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- part[" << (m_name ? m_name : "")
            << "]::selectUShorts(" << (pname ? pname : "") << ") with mask("
            << mask.cnt() << " out of " << mask.size()
            << ") received the following std::exception -- " << e.what();
        ibis::util::emptyCache();
        delete res;
        res = 0;
    }
    catch (const char* s) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- part[" << (m_name ? m_name : "")
            << "]::selectUShorts(" << (pname ? pname : "") << ") with mask("
            << mask.cnt() << " out of " << mask.size()
            << ") received the following string exception -- " << s;
        ibis::util::emptyCache();
        delete res;
        res = 0;
    }
    catch (...) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- part[" << (m_name ? m_name : "")
            << "]::selectUShorts(" << (pname ? pname : "") << ") with mask("
            << mask.cnt() << " out of " << mask.size()
            << ") received a unexpected exception";
        ibis::util::emptyCache();
        delete res;
        res = 0;
    }

    return res;
} // ibis::part::selectUShorts

/// The selected values are packed into the resulting array.  Only those
/// rows marked 1 are retrieved.  The caller is responsible for deleting
/// the returned value.
ibis::array_t<int32_t>* ibis::part::selectInts
(const char* pname, const ibis::bitvector &mask) const {
    ibis::array_t<int32_t>* res = 0;
    try {
        const ibis::column *col = getColumn(pname);
        if (col != 0) { // got it
            res = col->selectInts(mask);
        }
    }
    catch (const std::exception &e) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- part[" << (m_name ? m_name : "")
            << "]::selectInts(" << (pname ? pname : "") << ") with mask("
            << mask.cnt() << " out of " << mask.size()
            << ") received the following std::exception -- " << e.what();
        ibis::util::emptyCache();
        delete res;
        res = 0;
    }
    catch (const char* s) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- part[" << (m_name ? m_name : "")
            << "]::selectInts(" << (pname ? pname : "") << ") with mask("
            << mask.cnt() << " out of " << mask.size()
            << ") received the following string exception -- " << s;
        ibis::util::emptyCache();
        delete res;
        res = 0;
    }
    catch (...) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- part[" << (m_name ? m_name : "")
            << "]::selectInts(" << (pname ? pname : "") << ") with mask("
            << mask.cnt() << " out of " << mask.size()
            << ") received a unexpected exception";
        ibis::util::emptyCache();
        delete res;
        res = 0;
    }

    return res;
} // ibis::part::selectInts

/// The selected values are packed into the resulting array.  Only those
/// rows marked 1 are retrieved.  The caller is responsible for deleting
/// the returned value.
ibis::array_t<uint32_t>* ibis::part::selectUInts
(const char* pname, const ibis::bitvector &mask) const {
    ibis::array_t<uint32_t>* res = 0;
    try {
        const ibis::column *col = getColumn(pname);
        if (col != 0) { // got it
            res = col->selectUInts(mask);
        }
    }
    catch (const std::exception &e) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- part[" << (m_name ? m_name : "")
            << "]::selectUInts(" << (pname ? pname : "") << ") with mask("
            << mask.cnt() << " out of " << mask.size()
            << ") received the following std::exception -- " << e.what();
        ibis::util::emptyCache();
        delete res;
        res = 0;
    }
    catch (const char* s) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- part[" << (m_name ? m_name : "")
            << "]::selectUInts(" << (pname ? pname : "") << ") with mask("
            << mask.cnt() << " out of " << mask.size()
            << ") received the following string exception -- " << s;
        ibis::util::emptyCache();
        delete res;
        res = 0;
    }
    catch (...) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- part[" << (m_name ? m_name : "")
            << "]::selectUInts(" << (pname ? pname : "") << ") with mask("
            << mask.cnt() << " out of " << mask.size()
            << ") received a unexpected exception";
        ibis::util::emptyCache();
        delete res;
        res = 0;
    }

    return res;
} // ibis::part::selectUInts

/// The selected values are packed into the resulting array.  Only those
/// rows marked 1 are retrieved.  The caller is responsible for deleting
/// the returned value.
ibis::array_t<int64_t>* ibis::part::selectLongs
(const char* pname, const ibis::bitvector &mask) const {
    ibis::array_t<int64_t>* res = 0;
    try {
        const ibis::column* col = getColumn(pname);
        if (col != 0) { // got it
            res =  col->selectLongs(mask);
        }
    }
    catch (const std::exception &e) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- part[" << (m_name ? m_name : "")
            << "]::selectLongs(" << (pname ? pname : "") << ") with mask("
            << mask.cnt() << " out of " << mask.size()
            << ") received the following std::exception -- " << e.what();
        ibis::util::emptyCache();
        delete res;
        res = 0;
    }
    catch (const char* s) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- part[" << (m_name ? m_name : "")
            << "]::selectLongs(" << (pname ? pname : "") << ") with mask("
            << mask.cnt() << " out of " << mask.size()
            << ") received the following string exception -- " << s;
        ibis::util::emptyCache();
        delete res;
        res = 0;
    }
    catch (...) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- part[" << (m_name ? m_name : "")
            << "]::selectLongs(" << (pname ? pname : "") << ") with mask("
            << mask.cnt() << " out of " << mask.size()
            << ") received a unexpected exception";
        ibis::util::emptyCache();
        delete res;
        res = 0;
    }

    return res;
} // ibis::part::selectLongs

/// The selected values are packed into the resulting array.  Only those
/// rows marked 1 are retrieved.  The caller is responsible for deleting
/// the returned value.
ibis::array_t<uint64_t>* ibis::part::selectULongs
(const char* pname, const ibis::bitvector &mask) const {
    ibis::array_t<uint64_t>* res = 0;
    try {
        const ibis::column* col = getColumn(pname);
        if (col != 0) { // got it
            res =  col->selectULongs(mask);
        }
    }
    catch (const std::exception &e) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- part[" << (m_name ? m_name : "")
            << "]::selectULongs(" << (pname ? pname : "") << ") with mask("
            << mask.cnt() << " out of " << mask.size()
            << ") received the following std::exception -- " << e.what();
        ibis::util::emptyCache();
        delete res;
        res = 0;
    }
    catch (const char* s) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- part[" << (m_name ? m_name : "")
            << "]::selectULongs(" << (pname ? pname : "") << ") with mask("
            << mask.cnt() << " out of " << mask.size()
            << ") received the following string exception -- " << s;
        ibis::util::emptyCache();
        delete res;
        res = 0;
    }
    catch (...) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- part[" << (m_name ? m_name : "")
            << "]::selectULongs(" << (pname ? pname : "") << ") with mask("
            << mask.cnt() << " out of " << mask.size()
            << ") received a unexpected exception";
        ibis::util::emptyCache();
        delete res;
        res = 0;
    }

    return res;
} // ibis::part::selectULongs

/// The selected values are packed into the resulting array.  Only those
/// rows marked 1 are retrieved.  The caller is responsible for deleting
/// the returned value.
ibis::array_t<float>* ibis::part::selectFloats
(const char* pname, const ibis::bitvector &mask) const {
    ibis::array_t<float>* res = 0;
    try {
        const ibis::column* col = getColumn(pname);
        if (col != 0) { // got it
            res = col->selectFloats(mask);
        }
    }
    catch (const std::exception &e) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- part[" << (m_name ? m_name : "")
            << "]::selectFloats(" << (pname ? pname : "")
            << ") with mask(" << mask.cnt() << " out of " << mask.size()
            << ") received the following std::exception -- " << e.what();
        ibis::util::emptyCache();
        delete res;
        res = 0;
    }
    catch (const char* s) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- part[" << (m_name ? m_name : "")
            << "]::selectFloats(" << (pname ? pname : "")
            << ") with mask(" << mask.cnt() << " out of " << mask.size()
            << ") received the following string exception -- " << s;
        ibis::util::emptyCache();
        delete res;
        res = 0;
    }
    catch (...) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- part[" << (m_name ? m_name : "")
            << "]::selectFloats(" << (pname ? pname : "")
            << ") with mask(" << mask.cnt() << " out of " << mask.size()
            << ") received a unexpected exception";
        ibis::util::emptyCache();
        delete res;
        res = 0;
    }

    return res;
} // ibis::part::selectFloats

/// The selected values are packed into the resulting array.  Only those
/// rows marked 1 are retrieved.  The caller is responsible for deleting
/// the returned value.
ibis::array_t<double>* ibis::part::selectDoubles
(const char* pname, const ibis::bitvector &mask) const {
    ibis::array_t<double>* res = 0;
    try {
        const ibis::column* col = getColumn(pname);
        if (col != 0) { // got it
            res = col->selectDoubles(mask);
        }
    }
    catch (const std::exception &e) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- part[" << (m_name ? m_name : "")
            << "]::selectDoubles(" << (pname ? pname : "")
            << ") with mask(" << mask.cnt() << " out of " << mask.size()
            << ") received the following std::exception -- " << e.what();
        ibis::util::emptyCache();
        delete res;
        res = 0;
    }
    catch (const char* s) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- part[" << (m_name ? m_name : "")
            << "]::selectDoubles(" << (pname ? pname : "")
            << ") with mask(" << mask.cnt() << " out of " << mask.size()
            << ") received the following string exception -- " << s;
        ibis::util::emptyCache();
        delete res;
        res = 0;
    }
    catch (...) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- part[" << (m_name ? m_name : "")
            << "]::selectDoubles(" << (pname ? pname : "")
            << ") with mask(" << mask.cnt() << " out of " << mask.size()
            << ") received a unexpected exception";
        ibis::util::emptyCache();
        delete res;
        res = 0;
    }

    return res;
} // ibis::part::selectDoubles

/// The selected values are packed into the resulting array.  Only those
/// rows marked 1 are retrieved.  The caller is responsible for deleting
/// the returned value.
std::vector<std::string>* ibis::part::selectStrings
(const char* pname, const ibis::bitvector &mask) const {
    std::vector<std::string>* res = 0;
    try {
        const ibis::column* col = getColumn(pname);
        if (col != 0) { // got it
            res = col->selectStrings(mask);
        }
    }
    catch (const std::exception &e) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- part[" << (m_name ? m_name : "")
            << "]::selectStrings(" << (pname ? pname : "")
            << ") with mask(" << mask.cnt() << " out of " << mask.size()
            << ") received the following std::exception -- " << e.what();
        ibis::util::emptyCache();
        delete res;
        res = 0;
    }
    catch (const char* s) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- part[" << (m_name ? m_name : "")
            << "]::selectStrings(" << (pname ? pname : "")
            << ") with mask(" << mask.cnt() << " out of " << mask.size()
            << ") received the following string exception -- " << s;
        ibis::util::emptyCache();
        delete res;
        res = 0;
    }
    catch (...) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- part[" << (m_name ? m_name : "")
            << "]::selectStrings(" << (pname ? pname : "")
            << ") with mask(" << mask.cnt() << " out of " << mask.size()
            << ") received a unexpected exception";
        ibis::util::emptyCache();
        delete res;
        res = 0;
    }

    return res;
} // ibis::part::selectStrings

/// Select values of a column based on the given mask.
long ibis::part::selectValues(const char* cname, const ibis::bitvector &mask,
                              void* vals) const {
    const ibis::column* col = getColumn(cname);
    if (col == 0) {
        LOGGER(ibis::gVerbose > 2)
            << "Warning -- part[" << (m_name ? m_name : "")
            << "]::selectValues could not find a column named \""
            << (cname ? cname : "") << '"';
        return -1;
    }

    return col->selectValues(mask, vals);
} // ibis::part::selectValues

/// Select values of the column based on the range condition.
/// The column to be selected is also the column subject to the
/// range condition.
long ibis::part::selectValues(const ibis::qContinuousRange& cond,
                              void* vals) const {
    const char* cname = cond.colName();
    const ibis::column* col = getColumn(cname);
    if (col == 0) {
        LOGGER(ibis::gVerbose > 2)
            << "Warning -- part[" << (m_name ? m_name : "")
            << "]::selectValues could not find a column named \""
            << (cname ? cname : "") << '"';
        return -1;
    }

    return col->selectValues(cond, vals);
} // ibis::part::selectValues

/// Convert a list of RIDs into a bitvector.  If an list of external RIDs
/// is available, sort those RIDS and search through them, otherwise,
/// assume the incoming numbers are row numbers and mark the corresponding
/// positions of hits.
///
/// Return a negative value to indicate error, 0 to indicate no hit, and
/// positive value to indicate there are zero or more hits.
long ibis::part::evaluateRIDSet(const ibis::RIDSet &in,
                                ibis::bitvector &hits) const {
    if (in.empty() || nEvents == 0)
        return 0;
    if (rids != 0 && rids->size() > 0) {
        try {
            sortRIDs(); // make sure the file -rids.srt is up-to-date
            searchSortedRIDs(in, hits);
        }
        catch (...) { // don't care about the exception
            searchRIDs(in, hits); // use bruteforce search
        }
    }
    else {
        // take the values in ridset to be row indices
        for (uint32_t i = 0; i < in.size(); ++i)
            hits.setBit(static_cast<unsigned>(in[i].value), 1);
        hits.adjustSize(0, nEvents); // size the bitvector hits
    }
    LOGGER(ibis::gVerbose > 4)
        << "part[" << name() << "]::evaluateRIDSet found " << hits.cnt()
        << " out of " << in.size() << " rid" << (in.size()>1 ? "s" : "");
    return hits.sloppyCount();
} // ibis::part::evaluateRIDSet

/// Find all records that has the exact string value.
/// The object qString contains only two string values without any
/// indication as what they represent.  It first tries to match the left
/// string against known column names of this partition.  If the name
/// matches one that is of type STRING or KEY, the search is performed on
/// this column.  Otherwise the right string is compared against the column
/// names, if a match is found, the search is performed on that column.  If
/// both failed, the search returns no hit.
///
/// Return a negative value to indicate error, 0 to indicate no hit, and
/// positive value to indicate there are zero or more hits.
long ibis::part::stringSearch(const ibis::qString &cmp,
                              ibis::bitvector &low) const {
    if (columns.empty() || nEvents == 0) return 0;
    if (cmp.leftString() == 0) {
        low.set(0, nEvents);
        return 0;
    }

    long ierr = -1;
    // try leftString()
    const ibis::column* col = getColumn(cmp.leftString());
    if (col != 0) {
        ierr = col->stringSearch(cmp.rightString(), low);
    }
    else {
        // try rightString
        col = getColumn(cmp.rightString());
        if (col != 0) {
            ierr = col->stringSearch(cmp.leftString(), low);
        }
        else if (std::strcmp(cmp.leftString(), cmp.rightString()) == 0) {
            getNullMask(low);
        }
        else {
            // no match -- no hit
            low.set(0, nEvents);
        }
    }
    return ierr;
} // ibis::part::stringSearch

/// Return an upper bound of the number of records that have the exact
/// string value.
long ibis::part::stringSearch(const ibis::qString &cmp) const {
    long ret = 0;
    if (columns.empty() || nEvents == 0)
        return ret;
    if (cmp.leftString() == 0)
        return ret;

    // try leftString()
    const ibis::column* col = getColumn(cmp.leftString());
    if (col != 0) {
        ret = col->stringSearch(cmp.rightString());
    }
    else {    // try rightString
        col = getColumn(cmp.rightString());
        if (col != 0) {
            ret = col->stringSearch(cmp.leftString());
        }
        else if (std::strcmp(cmp.leftString(), cmp.rightString()) == 0) {
            ret = amask.cnt();
        }
    }
    return ret;
} // ibis::part::stringSearch

/// Determine the records that have the exact string values.  Actual work
/// done in the function search of the string-valued column.  It
/// produces no hit if the name is not a string-valued column.
///
/// Return a negative value to indicate error, 0 to indicate no hit, and
/// positive value to indicate there are zero or more hits.  To determine
/// the exact number of hits, call low.count().
long ibis::part::stringSearch(const ibis::qAnyString &cmp,
                              ibis::bitvector &low) const {
    if (columns.empty() || nEvents == 0) return 0;

    int ierr = -1;
    const ibis::column* col = getColumn(cmp.colName());
    if (col != 0) {
        if (col->type() == ibis::TEXT ||
            col->type() == ibis::CATEGORY) {
            ierr = col->stringSearch(cmp.valueList(), low);
            if (ierr > 0) {
                ibis::bitvector mskc;
                col->getNullMask(mskc);
                low &= mskc;
            }
            return ierr;
        }
    }

    // no match -- no hit
    low.set(0, nEvents);
    return ierr;
} // ibis::part::stringSearch

long ibis::part::stringSearch(const ibis::qAnyString &cmp) const {
    if (columns.empty() || nEvents == 0)
        return 0;

    long ret = -1;
    const ibis::column* col = getColumn(cmp.colName());
    if (col != 0) {
        if (col->type() == ibis::TEXT ||
            col->type() == ibis::CATEGORY) {
            ret = col->stringSearch(cmp.valueList());
            return ret;
        }
    }

    // no match -- no hit
    return ret;
} // ibis::part::stringSearch

/// Look for string like the given pattern.
long ibis::part::patternSearch(const ibis::qLike &cmp) const {
    if (columns.empty() || nEvents == 0) return 0;

    const ibis::column* col = getColumn(cmp.colName());
    if (col != 0) {
        return col->patternSearch(cmp.pattern());
    }
    else {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- part[" << name() << "]::patternSearch(" << cmp
            << ") failed because " << cmp.colName()
            << " is not a known column name";
    }
    return -1L;
} // ibis::part::patternSearch

/// Look for string like the given pattern.
long ibis::part::patternSearch(const ibis::qLike &cmp,
                               ibis::bitvector &hits) const {
    if (columns.empty() || nEvents == 0) return 0;

    const ibis::column* col = getColumn(cmp.colName());
    if (col != 0) {
        long ierr = col->patternSearch(cmp.pattern(), hits);
        if (ierr > 0) {
            ibis::bitvector mskc;
            col->getNullMask(mskc);
            hits &= mskc;
        }
        return hits.sloppyCount();
    }
    else {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- part[" << name() << "]::patternSearch(" << cmp
            << ") failed because " << cmp.colName()
            << " is not a known column name";
    }

    hits.set(0, nEvents);
    return -1;
} // ibis::part::patternSearch

/// Identify all rows containing the specified keyword.  The keyword search
/// is only applicable to a text column with full-text index (keyword
/// index).
long ibis::part::keywordSearch(const ibis::qKeyword &cmp,
                               ibis::bitvector &low) const {
    if (columns.empty() || nEvents == 0)
        return 0;
    if (cmp.colName() == 0) {
        low.set(0, nEvents);
        return 0;
    }

    long ierr = -1;
    const ibis::column* col = getColumn(cmp.colName());
    if (col != 0) {
        if (col->type() == ibis::TEXT) {
            ierr = col->keywordSearch(cmp.keyword(), low);
        }
        else if (std::strcmp(cmp.colName(), cmp.keyword()) == 0) {
            getNullMask(low);
        }
    }
    else if (std::strcmp(cmp.colName(), cmp.keyword()) == 0) {
        getNullMask(low);
    }
    else {
        // no match -- no hit
        low.set(0, nEvents);
    }
    return ierr;
} // ibis::part::keywordSearch

/// Return an upper bound of the number of records that have the keyword.
long ibis::part::keywordSearch(const ibis::qKeyword &cmp) const {
    if (columns.empty() || nEvents == 0)
        return 0;
    if (cmp.colName() == 0)
        return 0;

    long ret = -1;
    const ibis::column* col = getColumn(cmp.colName());
    if (col != 0) {
        if (col->type() == ibis::TEXT) {
            ret = col->keywordSearch(cmp.keyword());
        }
        else if (std::strcmp(cmp.colName(), cmp.keyword()) == 0) {
            ret = amask.cnt();
        }
    }
    else if (std::strcmp(cmp.colName(), cmp.keyword()) == 0) {
        ret = amask.cnt();
    }
    return ret;
} // ibis::part::keywordSearch

/// Determine the records that have all specified keywords.
///
/// Return a negative value to indicate error, 0 to indicate no hit, and
/// positive value to indicate there are zero or more hits.  To determine
/// the exact number of hits, call low.count().
long ibis::part::keywordSearch(const ibis::qAllWords &cmp,
                              ibis::bitvector &low) const {
    if (columns.empty() || nEvents == 0) return 0;

    int ierr = -1;
    const ibis::column* col = getColumn(cmp.colName());
    if (col != 0) {
        if (col->type() == ibis::TEXT) {
            ierr = col->keywordSearch(cmp.valueList(), low);
            return ierr;
        }
    }

    // no match -- no hit
    low.set(0, nEvents);
    return ierr;
} // ibis::part::keywordSearch

/// Compute an upper bound on the number of rows with all the specified
/// keywords.  Returns 0 if the column is not a text column.
long ibis::part::keywordSearch(const ibis::qAllWords &cmp) const {
    if (columns.empty() || nEvents == 0)
        return 0;

    long ret = -1;
    const ibis::column* col = getColumn(cmp.colName());
    if (col != 0) {
        if (col->type() == ibis::TEXT) {
            ret = col->keywordSearch(cmp.valueList());
        }
    }

    return ret;
} // ibis::part::keywordSearch

// simply pass the job to the named column
long ibis::part::evaluateRange(const ibis::qContinuousRange &cmp,
                               const ibis::bitvector &mask,
                               ibis::bitvector &hits) const {
    if (columns.empty() || nEvents == 0) return 0;
    if (cmp.colName() == 0 || *(cmp.colName()) == 0) { // no hit
        hits.set(0, nEvents);
        return 0;
    }

    long ierr = -1;
    std::string evt = "part[";
    evt += m_name;
    evt += "]::evaluateRange";
    const ibis::column* col = getColumn(cmp.colName());
    if (col != 0) {
        if (ibis::gVerbose > 2) {
            std::ostringstream oss;
            oss << '(' << cmp << ')';
            evt += oss.str();
        }

        ierr = col->evaluateRange(cmp, mask, hits);
        // if (ierr < 0) {
        //     ibis::util::mutexLock lock(&mutex, evt.c_str());
        //     unloadIndexes();
        //     ierr = col->evaluateRange(cmp, mask, hits);
        // }
    }
    else {
        LOGGER(ibis::gVerbose > 2)
            << "Warning -- " << evt << " could not find a column named "
            << cmp.colName();
        hits.set(0, nEvents);
    }

    LOGGER(ibis::gVerbose > 7 || (ibis::gVerbose > 1 && ierr < 0))
        << (ierr<0?"Waring -- ":"") << evt << " completed with ierr = "
        << ierr;
    return ierr;
} // ibis::part::evaluateRange

/// Return sure hits in bitvector low, and sure hits plus candidates in
/// bitvector high.  An alternative view is that low and high represent an
/// lower bound and an upper bound of the actual hits.
long ibis::part::estimateRange(const ibis::qContinuousRange &cmp,
                               ibis::bitvector &low,
                               ibis::bitvector &high) const {
    if (columns.empty() || nEvents == 0) return 0;

    if (cmp.colName() == 0 || *(cmp.colName()) == 0) { // no hit
        low.set(0, nEvents);
        high.set(0, nEvents);
        return 0;
    }

    long ierr = -1;
    std::string evt = "part[";
    evt += m_name;
    evt += "]::estimateRange";
    const ibis::column* col = getColumn(cmp.colName());
    if (col != 0) {
        if (ibis::gVerbose > 2) {
            std::ostringstream oss;
            oss << '(' << cmp << ')';
            evt += oss.str();
        }

        ierr = col->estimateRange(cmp, low, high);
        if (amask.size() == low.size()) {
            low &= amask;
            if (amask.size() == high.size())
                high &= amask;
            else
                high.clear();
        }
    }
    else {
        LOGGER(ibis::gVerbose > 2)
            << "Warning -- " << evt << " could not find a column named "
            << cmp.colName();
        high.set(0, nEvents);
        low.set(0, nEvents);
    }

    if (high.size() == low.size() && high.cnt() > low.cnt()) {
        LOGGER(ibis::gVerbose > 7)
            << evt << " --> [" << low.cnt() << ", " << high.cnt() << "]";
    }
    else {
        LOGGER(ibis::gVerbose > 7)
            << evt << " = " << low.cnt();
    }
    return ierr;
} // ibis::part::estimateRange

// simply pass the job to the named column
long ibis::part::estimateRange(const ibis::qContinuousRange &cmp) const {
    if (columns.empty() || nEvents == 0)
        return 0;
    if (cmp.colName() == 0)
        return 0;

    long ret = -1;
    const ibis::column* col = getColumn(cmp.colName());
    if (col != 0) {
        ret = col->estimateRange(cmp);
        if (ret < 0) {
            ibis::util::mutexLock lock(&mutex, "part::estimateRange");
            unloadIndexes();
            ret = col->estimateRange(cmp);
        }
    }
    else {
        LOGGER(ibis::gVerbose > 2)
            << "Warning -- part[" << name()
            << "]::estimateRange could not find a column named "
            << cmp.colName();
    }

    LOGGER(ibis::gVerbose > 7)
        << "part[" << name() << "]::estimateRange("
        << cmp << ") <= " << ret;
    return ret;
} // ibis::part::estimateRange

/// Estimate the cost of evaluate the query expression.
double ibis::part::estimateCost(const ibis::qContinuousRange &cmp) const {
    if (columns.empty() || nEvents == 0)
        return 0;
    if (cmp.colName() == 0 || *cmp.colName() == 0 ||
        (cmp.leftOperator() == ibis::qExpr::OP_UNDEFINED &&
         cmp.rightOperator() == ibis::qExpr::OP_UNDEFINED))
        return 0;

    double ret = -1;
    const ibis::column* col = getColumn(cmp.colName());
    if (col != 0) {
        ret = col->estimateCost(cmp);
    }
    else {
        LOGGER(ibis::gVerbose > 2)
            << "Warning -- part[" << name()
            << "]::estimateCost could not find a column named "
            << cmp.colName();
        ret = nEvents;
    }
    return ret;
} // ibis::part::estimateCost

long ibis::part::evaluateRange(const ibis::qDiscreteRange &cmp,
                               const ibis::bitvector &mask,
                               ibis::bitvector &hits) const {
    if (columns.empty() || nEvents == 0) return 0;

    long ierr = -1;
    if (cmp.colName() == 0) { // no hit
        hits.set(0, nEvents);
        ierr = -7;
    }
    else {
        const ibis::column* col = getColumn(cmp.colName());
        if (col != 0) {
            // ibis::bitvector mymask;
            // col->getNullMask(mymask);
            // mymask &= mask;
            ierr = col->evaluateRange(cmp, mask, hits);
            if (ierr < 0) {
                ibis::util::mutexLock lock(&mutex, "part::evaluateRange");
                unloadIndexes();
                ierr = col->evaluateRange(cmp, mask, hits);
            }
        }
        else {
            LOGGER(ibis::gVerbose > 2)
                << "Warning -- part[" << name()
                << "]::evaluateRange could not find a column named "
                << cmp.colName();
            hits.copy(mask);
        }
    }

    LOGGER(ibis::gVerbose > 7)
        << "part[" << name() << "]::evaluateRange("
        << cmp.colName() << " IN ...), ierr = " << ierr;
    return ierr;
} // ibis::part::evaluateRange

long ibis::part::estimateRange(const ibis::qDiscreteRange &cmp,
                               ibis::bitvector &low,
                               ibis::bitvector &high) const {
    if (columns.empty() || nEvents == 0) return 0;

    long ierr = -1;
    if (cmp.colName() == 0) { // no hit
        low.set(0, nEvents);
        high.set(0, nEvents);
        ierr = -7;
    }
    else {
        const ibis::column* col = getColumn(cmp.colName());
        if (col != 0) {
            ierr = col->estimateRange(cmp, low, high);
            if (amask.size() == low.size()) {
                low &= amask;
                if (amask.size() == high.size())
                    high &= amask;
            }
        }
        else {
            LOGGER(ibis::gVerbose > 2)
                << "Warning -- part["<< name()
                << "]::estimateRange could not find a column named "
                << cmp.colName();
            high.set(0, nEvents);
            low.set(0, nEvents);
        }
    }

    if (high.size() == low.size() && high.cnt() > low.cnt()) {
        LOGGER(ibis::gVerbose > 7)
            << "part[" << name() << "]::estimateRange("
            << cmp.colName() << " IN ...) --> [" << low.cnt() << ", "
            << high.cnt() << "]";
    }
    else {
        LOGGER(ibis::gVerbose > 7)
            << "part[" << name() << "]::estimateRange("
            << cmp.colName() << " IN ...) = " << low.cnt();
    }
    return ierr;
} // ibis::part::estimateRange

long ibis::part::estimateRange(const ibis::qDiscreteRange &cmp) const {
    if (columns.empty() || nEvents == 0)
        return 0;
    if (cmp.colName() == 0)
        return 0;

    long ret = -1;
    const ibis::column* col = getColumn(cmp.colName());
    if (col != 0) {
        ret = col->estimateRange(cmp);
        if (ret < 0) {
            ibis::util::mutexLock lock(&mutex, "part::estimateRange");
            unloadIndexes();
            ret = col->estimateRange(cmp);
        }
    }
    else {
        LOGGER(ibis::gVerbose > 2)
            << "Warning -- part[" << name()
            << "]::estimateRange could not find a column named "
            << cmp.colName();
    }

    LOGGER(ibis::gVerbose > 7)
        << "part[" << name() << "]::estimateRange(" << cmp.colName()
        << " IN ...) <= " << ret;
    return ret;
} // ibis::part::estimateRange

/// Estimate the cost of evaluate the query expression.
double ibis::part::estimateCost(const ibis::qDiscreteRange &cmp) const {
    if (columns.empty() || nEvents == 0)
        return 0;
    if (cmp.colName() == 0)
        return 0;

    double ret = -1;
    const ibis::column* col = getColumn(cmp.colName());
    if (col != 0) {
        ret = col->estimateCost(cmp);
    }
    else {
        LOGGER(ibis::gVerbose > 2)
            << "Warning -- part["<< name()
            << "]::estimateCost could not find a column named "
            << cmp.colName();
        ret = nEvents;
    }
    return ret;
} // ibis::part::estimateCost

long ibis::part::evaluateRange(const ibis::qIntHod &cmp,
                               const ibis::bitvector &mask,
                               ibis::bitvector &hits) const {
    if (columns.empty() || nEvents == 0) return 0;

    long ierr = -1;
    if (cmp.colName() == 0) { // no hit
        hits.set(0, nEvents);
        ierr = -7;
    }
    else {
        const ibis::column* col = getColumn(cmp.colName());
        if (col != 0) {
            // ibis::bitvector mymask;
            // col->getNullMask(mymask);
            // mymask &= mask;
            ierr = col->evaluateRange(cmp, mask, hits);
            if (ierr < 0) {
                ibis::util::mutexLock lock(&mutex, "part::evaluateRange");
                unloadIndexes();
                ierr = col->evaluateRange(cmp, mask, hits);
            }
        }
        else {
            LOGGER(ibis::gVerbose > 2)
                << "Warning -- part[" << name()
                << "]::evaluateRange could not find a column named "
                << cmp.colName();
            hits.copy(mask);
        }
    }

    LOGGER(ibis::gVerbose > 7)
        << "part[" << name() << "]::evaluateRange("
        << cmp.colName() << " IN ...), ierr = " << ierr;
    return ierr;
} // ibis::part::evaluateRange

long ibis::part::estimateRange(const ibis::qIntHod &cmp,
                               ibis::bitvector &low,
                               ibis::bitvector &high) const {
    if (columns.empty() || nEvents == 0) return 0;

    long ierr = -1;
    if (cmp.colName() == 0) { // no hit
        low.set(0, nEvents);
        high.set(0, nEvents);
        ierr = -7;
    }
    else {
        const ibis::column* col = getColumn(cmp.colName());
        if (col != 0) {
            ierr = col->estimateRange(cmp, low, high);
            if (amask.size() == low.size()) {
                low &= amask;
                if (amask.size() == high.size())
                    high &= amask;
            }
        }
        else {
            LOGGER(ibis::gVerbose > 2)
                << "Warning -- part[" << name()
                << "]::estimateRange could not find a column named "
                << cmp.colName();
            high.set(0, nEvents);
            low.set(0, nEvents);
        }
    }

    if (high.size() == low.size() && high.cnt() > low.cnt()) {
        LOGGER(ibis::gVerbose > 7)
            << "part[" << name() << "]::estimateRange("
            << cmp.colName() << " IN ...) --> [" << low.cnt() << ", "
            << high.cnt() << "]";
    }
    else {
        LOGGER(ibis::gVerbose > 7)
            << "part[" << name() << "]::estimateRange("
            << cmp.colName() << " IN ...) = " << low.cnt();
    }
    return ierr;
} // ibis::part::estimateRange

long ibis::part::estimateRange(const ibis::qIntHod &cmp) const {
    if (columns.empty() || nEvents == 0)
        return 0;
    if (cmp.colName() == 0)
        return 0;

    long ret = -1;
    const ibis::column* col = getColumn(cmp.colName());
    if (col != 0) {
        ret = col->estimateRange(cmp);
        if (ret < 0) {
            ibis::util::mutexLock lock(&mutex, "part::estimateRange");
            unloadIndexes();
            ret = col->estimateRange(cmp);
        }
    }
    else {
        LOGGER(ibis::gVerbose > 2)
            << "Warning -- part[" << name()
            << "]::estimateRange could not find a column named "
            << cmp.colName();
    }

    LOGGER(ibis::gVerbose > 7)
        << "part[" << name() << "]::estimateRange(" << cmp.colName()
        << " IN ...) <= " << ret;
    return ret;
} // ibis::part::estimateRange

/// Estimate the cost of evaluate the query expression.
double ibis::part::estimateCost(const ibis::qIntHod &cmp) const {
    if (columns.empty() || nEvents == 0)
        return 0;
    if (cmp.colName() == 0)
        return 0;

    double ret = -1;
    const ibis::column* col = getColumn(cmp.colName());
    if (col != 0) {
        ret = col->estimateCost(cmp);
    }
    else {
        LOGGER(ibis::gVerbose > 2)
            << "Warning -- part[" << name()
            << "]::estimateCost could not find a column named "
            << cmp.colName();
        ret = nEvents;
    }
    return ret;
} // ibis::part::estimateCost

long ibis::part::evaluateRange(const ibis::qUIntHod &cmp,
                               const ibis::bitvector &mask,
                               ibis::bitvector &hits) const {
    if (columns.empty() || nEvents == 0) return 0;

    long ierr = -1;
    if (cmp.colName() == 0) { // no hit
        hits.set(0, nEvents);
        ierr = -7;
    }
    else {
        const ibis::column* col = getColumn(cmp.colName());
        if (col != 0) {
            // ibis::bitvector mymask;
            // col->getNullMask(mymask);
            // mymask &= mask;
            ierr = col->evaluateRange(cmp, mask, hits);
            if (ierr < 0) {
                ibis::util::mutexLock lock(&mutex, "part::evaluateRange");
                unloadIndexes();
                ierr = col->evaluateRange(cmp, mask, hits);
            }
        }
        else {
            LOGGER(ibis::gVerbose > 2)
                << "Warning -- part[" << name()
                << "]::evaluateRange could not find a column named "
                << cmp.colName();
            hits.copy(mask);
        }
    }

    LOGGER(ibis::gVerbose > 7)
        << "part[" << name() << "]::evaluateRange("
        << cmp.colName() << " IN ...), ierr = " << ierr;
    return ierr;
} // ibis::part::evaluateRange

long ibis::part::estimateRange(const ibis::qUIntHod &cmp,
                               ibis::bitvector &low,
                               ibis::bitvector &high) const {
    if (columns.empty() || nEvents == 0) return 0;

    long ierr = -1;
    if (cmp.colName() == 0) { // no hit
        low.set(0, nEvents);
        high.set(0, nEvents);
        ierr = -7;
    }
    else {
        const ibis::column* col = getColumn(cmp.colName());
        if (col != 0) {
            ierr = col->estimateRange(cmp, low, high);
            if (amask.size() == low.size()) {
                low &= amask;
                if (amask.size() == high.size())
                    high &= amask;
            }
        }
        else {
            LOGGER(ibis::gVerbose > 2)
                << "Warning -- part[" << name()
                << "]::estimateRange to find a column named "
                << cmp.colName();
            high.set(0, nEvents);
            low.set(0, nEvents);
        }
    }

    if (high.size() == low.size() && high.cnt() > low.cnt()) {
        LOGGER(ibis::gVerbose > 7)
            << "part[" << name() << "]::estimateRange("
            << cmp.colName() << " IN ...) --> [" << low.cnt() << ", "
            << high.cnt() << "]";
    }
    else {
        LOGGER(ibis::gVerbose > 7)
            << "part[" << name() << "]::estimateRange("
            << cmp.colName() << " IN ...) = " << low.cnt();
    }
    return ierr;
} // ibis::part::estimateRange

long ibis::part::estimateRange(const ibis::qUIntHod &cmp) const {
    if (columns.empty() || nEvents == 0)
        return 0;
    if (cmp.colName() == 0)
        return 0;

    long ret = -1;
    const ibis::column* col = getColumn(cmp.colName());
    if (col != 0) {
        ret = col->estimateRange(cmp);
        if (ret < 0) {
            ibis::util::mutexLock lock(&mutex, "part::estimateRange");
            unloadIndexes();
            ret = col->estimateRange(cmp);
        }
    }
    else {
        LOGGER(ibis::gVerbose > 2)
            << "Warning -- part[" << name()
            << "]::estimateRange could not find a column named "
            << cmp.colName();
    }

    LOGGER(ibis::gVerbose > 7)
        << "part[" << name() << "]::estimateRange(" << cmp.colName()
        << " IN ...) <= " << ret;
    return ret;
} // ibis::part::estimateRange

/// Estimate the cost of evaluate the query expression.
double ibis::part::estimateCost(const ibis::qUIntHod &cmp) const {
    if (columns.empty() || nEvents == 0)
        return 0;
    if (cmp.colName() == 0)
        return 0;

    double ret = -1;
    const ibis::column* col = getColumn(cmp.colName());
    if (col != 0) {
        ret = col->estimateCost(cmp);
    }
    else {
        LOGGER(ibis::gVerbose > 2)
            << "Warning -- part[" << name()
            << "]::estimateCost could not find a column named "
            << cmp.colName();
        ret = nEvents;
    }
    return ret;
} // ibis::part::estimateCost

/// Estimate the cost of evaluate the query expression.
double ibis::part::estimateCost(const ibis::qString &cmp) const {
    if (columns.empty() || nEvents == 0)
        return 0;
    if (cmp.leftString() == 0 || cmp.rightString() == 0)
        return 0;

    double ret = -1;
    const ibis::column* col = getColumn(cmp.leftString());
    if (col == 0)
        col = getColumn(cmp.rightString());
    if (col != 0) {
        ret = col->estimateCost(cmp);
    }
    else {
        LOGGER(ibis::gVerbose > 2)
            << "Warning -- part[" << name()
            << "]::estimateCost could not find a column named "
            <<  cmp.leftString() << " or " << cmp.rightString();
        ret = nEvents;
    }
    return ret;
} // ibis::part::estimateCost

/// Estimate the cost of evaluate the query expression.
double ibis::part::estimateCost(const ibis::qAnyString &cmp) const {
    if (columns.empty() || nEvents == 0)
        return 0;
    if (cmp.colName() == 0)
        return 0;

    double ret = -1;
    const ibis::column* col = getColumn(cmp.colName());
    if (col != 0) {
        ret = col->estimateCost(cmp);
    }
    else {
        LOGGER(ibis::gVerbose > 2)
            << "Warning -- part[" << name()
            << "]::estimateCost could not find a column named "
            << cmp.colName();
        ret = nEvents;
    }
    return ret;
} // ibis::part::estimateCost

/// Estimate a lower bound and an upper bound on the records that are
/// hits.  The bitvector @c low contains records that are hits (for
/// sure) and the bitvector @c high contains records that are possible
/// hits.
long ibis::part::estimateMatchAny(const ibis::qAnyAny &cmp,
                                  ibis::bitvector &low,
                                  ibis::bitvector &high) const {
    if (cmp.getPrefix() == 0 || cmp.getValues().empty()) return -1;
    if (nEvents == 0) return 0;

    low.set(0, nEvents);
    high.set(0, nEvents);
    const char *pref = cmp.getPrefix();
    const int   len = std::strlen(pref);
    const ibis::array_t<double> &vals = cmp.getValues();
    columnList::const_iterator it = columns.lower_bound(pref);
    if (vals.size() > 1) { // multiple values, use discrete range query
        // TODO: to implement the proper functions to handle discrete
        // ranges in all index classes.
        // Temporary solution: change discrete range into a series of
        // equality conditions.
        while (it != columns.end() &&
               0 == strnicmp((*it).first, pref, len)) {
            // the column name has the specified prefix
            ibis::bitvector msk, ltmp, htmp;
            (*it).second->getNullMask(msk);
            //      ibis::qDiscreteRange ex((*it).first, vals);
            //      (*it).second->estimateRange(ex, ltmp, htmp);
            ibis::qContinuousRange ex((*it).first, ibis::qExpr::OP_EQ,
                                      vals[0]);
            (*it).second->estimateRange(ex, ltmp, htmp);
            for (uint32_t i = 1; i < vals.size(); ++ i) {
                ibis::bitvector ltmp2, htmp2;
                ibis::qContinuousRange ex2((*it).first, ibis::qExpr::OP_EQ,
                                           vals[i]);
                (*it).second->estimateRange(ex2, ltmp2, htmp2);
                ltmp |= ltmp2;
                if (htmp2.size() == htmp.size())
                    htmp |= htmp2;
                else
                    htmp |= ltmp2;
            }
            ltmp &= msk;
            low |= ltmp;
            if (ltmp.size() == htmp.size()) {
                htmp &= msk;
                high |= htmp;
            }
            else {
                high |= ltmp;
            }
            ++ it; // check the next column
        }
    }
    else { // a single value, translate to simple equality query
        while (it != columns.end() &&
               0 == strnicmp((*it).first, pref, len)) {
            // the column name has the specified prefix
            ibis::bitvector msk, ltmp, htmp;
            (*it).second->getNullMask(msk);
            ibis::qContinuousRange ex((*it).first, ibis::qExpr::OP_EQ,
                                      vals.back());
            (*it).second->estimateRange(ex, ltmp, htmp);
            low |= ltmp;
            if (ltmp.size() == htmp.size())
                high |= htmp;
            else
                high |= ltmp;
            ++ it; // check the next column
        }
    }
    return 0;
} // ibis::part::estimateMatchAny

/// Convert a set of numbers to an ibis::bitvector.
void ibis::part::numbersToBitvector(const std::vector<uint32_t> &rows,
                                    ibis::bitvector &msk) const {
    if (rows.size() > 1) {
        ibis::array_t<uint32_t> r(rows.size());
        std::copy(rows.begin(), rows.end(), r.begin());
        std::sort(r.begin(), r.end());
        for (uint32_t i = 0; i < rows.size() && r[i] < nEvents; ++ i)
            msk.setBit(r[i], 1);
    }
    else {
        msk.appendFill(0, rows[0]-1);
        msk += 1;
    }
    msk.adjustSize(0, nEvents);
} // ibis::part::numbersToBitvector

/// Convert a set of range conditions to an ibis::bitvector.
/// @note The bitvector may include active as well as inactive rows.
void ibis::part::stringToBitvector(const char* conds,
                                   ibis::bitvector &msk) const {
    if (nEvents > 0) {
        ibis::query q(ibis::util::userName(), this);
        q.setWhereClause(conds);
        q.getExpandedHits(msk);
    }
    else {
        msk.clear();
    }
} // ibis::part::stringToBitvector

/// Evaluate the range condition.  Scan the base data to resolve the
/// range condition.
/// Without a user specified mask, all non-NULL values are examined.
long ibis::part::doScan(const ibis::qRange &cmp,
                        ibis::bitvector &hits) const {
    if (columns.empty() || nEvents == 0)
        return 0;
    if (cmp.colName() == 0)
        return 0;

    const ibis::column* col = getColumn(cmp.colName());
    if (col == 0) {
        LOGGER(ibis::gVerbose > 2)
            << "Warning -- part[" << name()
            << "]::doScan could not find column "
            << cmp.colName();
        hits.clear();
        return 0;
    }

    ibis::bitvector mask;
    col->getNullMask(mask);
    if (amask.size() == mask.size())
        mask &= amask;
    return doScan(cmp, mask, hits);
} // ibis::part::doScan

/// Evalute the range condition on the records that are marked 1 in the
/// mask.  The i'th element of the column is examined if mask[i] is set
/// (mask[i] == 1).
long ibis::part::doScan(const ibis::qRange &cmp,
                        const ibis::bitvector &mask,
                        ibis::bitvector &hits) const {
    if (columns.empty() || nEvents == 0 || cmp.colName() == 0 ||
        mask.size() == 0 || mask.cnt() == 0)
        return 0;

    std::string evt = "part[";
    evt += m_name;
    evt += "]::doScan";
    const ibis::column* col = getColumn(cmp.colName());
    if (col == 0) {
        LOGGER(ibis::gVerbose > 2)
            << "Warning -- " << evt << " could not find named column "
            << cmp.colName();
        return -1;
    }
    if (ibis::gVerbose > 2) {
        std::ostringstream oss;
        oss << '(' << cmp << ')';
        evt += oss.str();
    }

    std::string sname;
    (void) col->dataFileName(sname);
    long ierr = 0;
    switch (col->type()) {
    default:
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " can not process data type "
            << col->type() << " (" << ibis::TYPESTRING[(int)col->type()] << ")";
        hits.set(0, nEvents);
        ierr = -2;
        break;

    case ibis::CATEGORY: {
        // NOTE that there should not be any qRange query on columns of
        // type KEY or STRING.  However, they are implemented here to make
        // the rest of the code a little simpler!
        ibis::bitvector tmp;
        if (cmp.getType() == ibis::qExpr::RANGE)
            col->estimateRange
                (reinterpret_cast<const ibis::qContinuousRange&>(cmp),
                 hits, tmp);
        else
            col->estimateRange
                (reinterpret_cast<const ibis::qDiscreteRange&>(cmp),
                 hits, tmp);
        hits &= mask;
        break;}

    case ibis::TEXT: { // treat the values as row numbers
        if (cmp.getType() == ibis::qExpr::RANGE) {
            double tmp = reinterpret_cast<const ibis::qContinuousRange&>
                (cmp).leftBound();
            const uint32_t left = (tmp<=0.0 ? 0 : static_cast<uint32_t>(tmp));
            tmp = reinterpret_cast<const ibis::qContinuousRange&>
                (cmp).rightBound();
            uint32_t right = (tmp <= left ? left :
                              static_cast<uint32_t>(tmp));
            if (right > nEvents)
                right = nEvents;
            for (uint32_t i = left; i < right; ++ i)
                hits.setBit(i, 1);
        }
        else {
            const ibis::array_t<double> &vals =
                reinterpret_cast<const ibis::qDiscreteRange&>
                (cmp).getValues();
            for (uint32_t i = 0; i < vals.size(); ++ i) {
                if (vals[i] >= 0 && vals[i] < nEvents)
                    hits.setBit(static_cast<unsigned>(vals[i]), 1);
            }
        }
        hits.adjustSize(0, nEvents);
        hits &= mask;
        break;}

    case ibis::LONG: {
        ibis::array_t<int64_t> intarray;
        ierr = col->getValuesArray(&intarray);
        if (ierr >= 0) {
            switch (cmp.getType()) {
            default: {
                ierr = doCompare(intarray, cmp, mask, hits);
                break;}
            case ibis::qExpr::RANGE: {
                const ibis::qContinuousRange& rng =
                    static_cast<const ibis::qContinuousRange&>(cmp);
                ierr = doScan(intarray, rng, mask, hits);
                break;}
            case ibis::qExpr::INTHOD: {
                const ibis::qIntHod& qih =
                    static_cast<const ibis::qIntHod&>(cmp);
                ierr = doCompare(intarray, qih, mask, hits);
                break;}
            case ibis::qExpr::UINTHOD: {
                const ibis::qUIntHod& qih =
                    static_cast<const ibis::qUIntHod&>(cmp);
                ierr = doCompare(intarray, qih, mask, hits);
                break;}
            }
        }
        else if (! sname.empty()) {
            switch (cmp.getType()) {
            default:
                ierr = doCompare<int64_t>(sname.c_str(), cmp, mask, hits);
                break;
            case ibis::qExpr::INTHOD: {
                const ibis::qIntHod& qih =
                    static_cast<const ibis::qIntHod&>(cmp);
                ierr = doCompare<int64_t>(sname.c_str(), qih, mask, hits);
                break;}
            case ibis::qExpr::UINTHOD: {
                const ibis::qIntHod& qih =
                    static_cast<const ibis::qIntHod&>(cmp);
                ierr = doCompare<int64_t>(sname.c_str(), qih, mask, hits);
                break;}
            }
        }
        else {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << evt << " could not locate any data";
            hits.set(0, nEvents);
            ierr = -3;
        }
        break;}

    case ibis::ULONG: {
        ibis::array_t<uint64_t> intarray;
        ierr = col->getValuesArray(&intarray);
        if (ierr >= 0) {
            switch (cmp.getType()) {
            default: {
                ierr = doCompare(intarray, cmp, mask, hits);
                break;}
            case ibis::qExpr::RANGE: {
                const ibis::qContinuousRange& rng =
                    static_cast<const ibis::qContinuousRange&>(cmp);
                ierr = doScan(intarray, rng, mask, hits);
                break;}
            case ibis::qExpr::INTHOD: {
                const ibis::qIntHod& qih =
                    static_cast<const ibis::qIntHod&>(cmp);
                ierr = doCompare(intarray, qih, mask, hits);
                break;}
            case ibis::qExpr::UINTHOD: {
                const ibis::qUIntHod& qih =
                    static_cast<const ibis::qUIntHod&>(cmp);
                ierr = doCompare(intarray, qih, mask, hits);
                break;}
            }
        }
        else if (! sname.empty()) {
            switch (cmp.getType()) {
            default:
                ierr = doCompare<uint64_t>(sname.c_str(), cmp, mask, hits);
                break;
            case ibis::qExpr::INTHOD: {
                const ibis::qIntHod& qih =
                    static_cast<const ibis::qIntHod&>(cmp);
                ierr = doCompare<uint64_t>(sname.c_str(), qih, mask, hits);
                break;}
            case ibis::qExpr::UINTHOD: {
                const ibis::qIntHod& qih =
                    static_cast<const ibis::qIntHod&>(cmp);
                ierr = doCompare<uint64_t>(sname.c_str(), qih, mask, hits);
                break;}
            }
        }
        else {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << evt << " could not locate any data";
            hits.set(0, nEvents);
            ierr = -3;
        }
        break;}

    case ibis::INT: {
        ibis::array_t<int32_t> intarray;
        ierr = col->getValuesArray(&intarray);
        if (ierr >= 0) {
            if (cmp.getType() == ibis::qExpr::RANGE) {
                const ibis::qContinuousRange &rng =
                    static_cast<const ibis::qContinuousRange&>(cmp);
                ierr = doScan(intarray, rng, mask, hits);
            }
            else {
                ierr = doCompare(intarray, cmp, mask, hits);
            }
        }
        else if (! sname.empty()) {
            ierr = doCompare<int32_t>(sname.c_str(), cmp, mask, hits);
        }
        else {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << evt << " could not locate any data";
            hits.set(0, nEvents);
            ierr = -3;
        }
        break;}

    case ibis::UINT: {
        ibis::array_t<uint32_t> intarray;
        ierr = col->getValuesArray(&intarray);
        if (ierr >= 0) {
            if (cmp.getType() == ibis::qExpr::RANGE) {
                const ibis::qContinuousRange &rng =
                    static_cast<const ibis::qContinuousRange&>(cmp);
                ierr = doScan(intarray, rng, mask, hits);
            }
            else {
                ierr = doCompare(intarray, cmp, mask, hits);
            }
        }
        else if (! sname.empty()) {
            ierr = doCompare<uint32_t>(sname.c_str(), cmp, mask, hits);
        }
        else {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << evt << " could not locate any data";
            hits.set(0, nEvents);
            ierr = -3;
        }
        break;}

    case ibis::SHORT: {
        ibis::array_t<int16_t> intarray;
        ierr = col->getValuesArray(&intarray);
        if (ierr >= 0) {
            if (cmp.getType() == ibis::qExpr::RANGE) {
                const ibis::qContinuousRange &rng =
                    static_cast<const ibis::qContinuousRange&>(cmp);
                ierr = doScan(intarray, rng, mask, hits);
            }
            else {
                ierr = doCompare(intarray, cmp, mask, hits);
            }
        }
        else if (! sname.empty()) {
            ierr = doCompare<int16_t>(sname.c_str(), cmp, mask, hits);
        }
        else {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << evt << " could not locate any data";
            hits.set(0, nEvents);
            ierr = -3;
        }
        break;}

    case ibis::USHORT: {
        ibis::array_t<uint16_t> intarray;
        ierr = col->getValuesArray(&intarray);
        if (ierr >= 0) {
            if (cmp.getType() == ibis::qExpr::RANGE) {
                const ibis::qContinuousRange &rng =
                    static_cast<const ibis::qContinuousRange&>(cmp);
                ierr = doScan(intarray, rng, mask, hits);
            }
            else {
                ierr = doCompare(intarray, cmp, mask, hits);
            }
        }
        else if (! sname.empty()) {
            ierr = doCompare<uint16_t>(sname.c_str(), cmp, mask, hits);
        }
        else {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << evt << " could not locate any data";
            hits.set(0, nEvents);
            ierr = -3;
        }
        break;}

    case ibis::BYTE: {
        ibis::array_t<signed char> intarray;
        ierr = col->getValuesArray(&intarray);
        if (ierr >= 0) {
            if (cmp.getType() == ibis::qExpr::RANGE) {
                const ibis::qContinuousRange &rng =
                    static_cast<const ibis::qContinuousRange&>(cmp);
                ierr = doScan(intarray, rng, mask, hits);
            }
            else {
                ierr = doCompare(intarray, cmp, mask, hits);
            }
        }
        else if (! sname.empty()) {
            ierr = doCompare<signed char>(sname.c_str(), cmp, mask, hits);
        }
        else {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << evt << " could not locate any data";
            hits.set(0, nEvents);
            ierr = -3;
        }
        break;}

    case ibis::UBYTE: {
        ibis::array_t<unsigned char> intarray;
        ierr = col->getValuesArray(&intarray);
        if (ierr >= 0) {
            if (cmp.getType() == ibis::qExpr::RANGE) {
                const ibis::qContinuousRange &rng =
                    static_cast<const ibis::qContinuousRange&>(cmp);
                ierr = doScan(intarray, rng, mask, hits);
            }
            else {
                ierr = doCompare(intarray, cmp, mask, hits);
            }
        }
        else if (! sname.empty()) {
            ierr = doCompare<unsigned char>(sname.c_str(), cmp, mask, hits);
        }
        else {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << evt << " could not locate any data";
            hits.set(0, nEvents);
            ierr = -3;
        }
        break;}

    case ibis::FLOAT: {
        ibis::array_t<float> floatarray;
        ierr = col->getValuesArray(&floatarray);
        if (ierr >= 0) {
            if (cmp.getType() == ibis::qExpr::RANGE)
                ierr = doScan
                    (floatarray,
                     static_cast<const ibis::qContinuousRange&>(cmp),
                     mask, hits);
            else
                ierr = doCompare(floatarray, cmp, mask, hits);
        }
        else if (! sname.empty()) {
            ierr = doCompare<float>(sname.c_str(), cmp, mask, hits);
        }
        else {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << evt << " could not locate any data";
            hits.set(0, nEvents);
            ierr = -3;
        }
        break;}

    case ibis::DOUBLE: {
        ibis::array_t<double> doublearray;
        ierr = col->getValuesArray(&doublearray);
        if (ierr >= 0) {
            if (cmp.getType() == ibis::qExpr::RANGE)
                ierr = doScan
                    (doublearray,
                     static_cast<const ibis::qContinuousRange&>(cmp),
                     mask, hits);
            else
                ierr = doCompare(doublearray, cmp, mask, hits);
        }
        else if (! sname.empty()) {
            ierr = doCompare<double>(sname.c_str(), cmp, mask, hits);
        }
        else {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << evt << " could not locate any data";
            hits.set(0, nEvents);
            ierr = -3;
        }
        break;}
    }

    if (hits.size() != nEvents) {
        LOGGER(ibis::gVerbose > 3) // could happen when no hits
            << evt << " need to reset the result bit vector from "
            << hits.size() << " to " << nEvents;
        hits.adjustSize(0, nEvents); // append 0 bits or remove extra bits
    }
    LOGGER(ibis::gVerbose > 7)
        << evt << " examined " << mask.cnt() << " candidates and found "
        << hits.cnt() << " hits";
    return ierr;
} // ibis::part::doScan

/// Evalute the range condition and record the values satisfying the
/// condition in res.  The tests are only performed on the records that are
/// marked 1 in the mask (mask[i] == 1).  This function only works for
/// integers and floating-point numbers.
long ibis::part::doScan(const ibis::qRange &cmp,
                        const ibis::bitvector &mask,
                        void *res) const {
    if (columns.empty() || nEvents == 0 || cmp.colName() == 0 ||
        mask.size() == 0 || mask.cnt() == 0)
        return 0;

    std::string evt = "part[";
    evt += m_name;
    evt += "]::doScan";
    const ibis::column* col = getColumn(cmp.colName());
    if (col == 0) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " could not find named column "
            << cmp.colName();
        return -1;
    }
    if (ibis::gVerbose > 2) {
        std::ostringstream oss;
        oss << '(' << cmp << ')';
        evt += oss.str();
    }

    std::string sname;
    (void) col->dataFileName(sname);

    long ierr = 0;
    switch (col->type()) {
    default:
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " can not process data type "
            << col->type() << " (" << ibis::TYPESTRING[(int)col->type()] << ")";
        ierr = -2;
        break;

    case ibis::LONG: {
        ibis::array_t<int64_t> intarray;
        ierr = col->getValuesArray(&intarray);
        if (ierr >= 0) {
            switch (cmp.getType()) {
            default: {
                ierr = doCompare(intarray, cmp, mask,
                                 *static_cast<array_t<int64_t>*>(res));
                break;}
            case ibis::qExpr::RANGE: {
                const ibis::qContinuousRange& rng =
                    static_cast<const ibis::qContinuousRange&>(cmp);
                ierr = doScan(intarray, rng, mask,
                              *static_cast<array_t<int64_t>*>(res));
                break;}
            }
        }
        else if (! sname.empty()) {
            ierr = doCompare<int64_t>
                (sname.c_str(), cmp, mask,
                 *static_cast<array_t<int64_t>*>(res));
        }
        else {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << evt << " could not locate any data";
            ierr = -3;
        }
        break;}

    case ibis::ULONG: {
        ibis::array_t<uint64_t> intarray;
        ierr = col->getValuesArray(&intarray);
        if (ierr >= 0) {
            switch (cmp.getType()) {
            default: {
                ierr = doCompare
                    (intarray, cmp, mask,
                     *static_cast<array_t<uint64_t>*>(res));
                break;}
            case ibis::qExpr::RANGE: {
                const ibis::qContinuousRange& rng =
                    static_cast<const ibis::qContinuousRange&>(cmp);
                ierr = doScan(intarray, rng, mask,
                              *static_cast<array_t<uint64_t>*>(res));
                break;}
            }
        }
        else if (! sname.empty()) {
            ierr = doCompare<uint64_t>
                (sname.c_str(), cmp, mask,
                 *static_cast<array_t<uint64_t>*>(res));
        }
        else {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << evt << " could not locate any data";
            ierr = -3;
        }
        break;}

    case ibis::INT: {
        ibis::array_t<int32_t> intarray;
        ierr = col->getValuesArray(&intarray);
        if (ierr >= 0) {
            if (cmp.getType() == ibis::qExpr::RANGE) {
                const ibis::qContinuousRange &rng =
                    static_cast<const ibis::qContinuousRange&>(cmp);
                ierr = doScan(intarray, rng, mask,
                              *static_cast<array_t<int32_t>*>(res));
            }
            else {
                ierr = doCompare(intarray, cmp, mask,
                                 *static_cast<array_t<int32_t>*>(res));
            }
        }
        else if (! sname.empty()) {
            ierr = doCompare<int32_t>
                (sname.c_str(), cmp, mask,
                 *static_cast<array_t<int32_t>*>(res));
        }
        else {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << evt << " could not locate any data";
            ierr = -3;
        }
        break;}

    case ibis::UINT: {
        ibis::array_t<uint32_t> intarray;
        ierr = col->getValuesArray(&intarray);
        if (ierr >= 0) {
            if (cmp.getType() == ibis::qExpr::RANGE) {
                const ibis::qContinuousRange &rng =
                    static_cast<const ibis::qContinuousRange&>(cmp);
                ierr = doScan(intarray, rng, mask,
                              *static_cast<array_t<uint32_t>*>(res));
            }
            else {
                ierr = doCompare(intarray, cmp, mask,
                                 *static_cast<array_t<uint32_t>*>(res));
            }
        }
        else if (! sname.empty()) {
            ierr = doCompare<uint32_t>
                (sname.c_str(), cmp, mask,
                 *static_cast<array_t<uint32_t>*>(res));
        }
        else {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << evt << " could not locate any data";
            ierr = -3;
        }
        break;}

    case ibis::SHORT: {
        ibis::array_t<int16_t> intarray;
        ierr = col->getValuesArray(&intarray);
        if (ierr >= 0) {
            if (cmp.getType() == ibis::qExpr::RANGE) {
                const ibis::qContinuousRange &rng =
                    static_cast<const ibis::qContinuousRange&>(cmp);
                ierr = doScan(intarray, rng, mask,
                              *static_cast<array_t<int16_t>*>(res));
            }
            else {
                ierr = doCompare(intarray, cmp, mask,
                                 *static_cast<array_t<int16_t>*>(res));
            }
        }
        else if (! sname.empty()) {
            ierr = doCompare<int16_t>
                (sname.c_str(), cmp, mask,
                 *static_cast<array_t<int16_t>*>(res));
        }
        else {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << evt << " could not locate any data";
            ierr = -3;
        }
        break;}

    case ibis::USHORT: {
        ibis::array_t<uint16_t> intarray;
        ierr = col->getValuesArray(&intarray);
        if (ierr >= 0) {
            if (cmp.getType() == ibis::qExpr::RANGE) {
                const ibis::qContinuousRange &rng =
                    static_cast<const ibis::qContinuousRange&>(cmp);
                ierr = doScan(intarray, rng, mask,
                              *static_cast<array_t<uint16_t>*>(res));
            }
            else {
                ierr = doCompare(intarray, cmp, mask,
                                 *static_cast<array_t<uint16_t>*>(res));
            }
        }
        else if (! sname.empty()) {
            ierr = doCompare<uint16_t>
                (sname.c_str(), cmp, mask,
                 *static_cast<array_t<uint16_t>*>(res));
        }
        else {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << evt << " could not locate any data";
            ierr = -3;
        }
        break;}

    case ibis::BYTE: {
        ibis::array_t<signed char> intarray;
        ierr = col->getValuesArray(&intarray);
        if (ierr >= 0) {
            if (cmp.getType() == ibis::qExpr::RANGE) {
                const ibis::qContinuousRange &rng =
                    static_cast<const ibis::qContinuousRange&>(cmp);
                ierr = doScan(intarray, rng, mask,
                              *static_cast<array_t<signed char>*>(res));
            }
            else {
                ierr = doCompare(intarray, cmp, mask,
                                 *static_cast<array_t<signed char>*>(res));
            }
        }
        else if (! sname.empty()) {
            ierr = doCompare<signed char>
                (sname.c_str(), cmp, mask,
                 *static_cast<array_t<signed char>*>(res));
        }
        else {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << evt << " could not locate any data";
            ierr = -3;
        }
        break;}

    case ibis::UBYTE: {
        ibis::array_t<unsigned char> intarray;
        ierr = col->getValuesArray(&intarray);
        if (ierr >= 0) {
            if (cmp.getType() == ibis::qExpr::RANGE) {
                const ibis::qContinuousRange &rng =
                    static_cast<const ibis::qContinuousRange&>(cmp);
                ierr = doScan(intarray, rng, mask,
                              *static_cast<array_t<unsigned char>*>(res));
            }
            else {
                ierr = doCompare
                    (intarray, cmp, mask,
                     *static_cast<array_t<unsigned char>*>(res));
            }
        }
        else if (! sname.empty()) {
            ierr = doCompare<unsigned char>
                (sname.c_str(), cmp, mask,
                 *static_cast<array_t<unsigned char>*>(res));
        }
        else {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << evt << " could not locate any data";
            ierr = -3;
        }
        break;}

    case ibis::FLOAT: {
        ibis::array_t<float> floatarray;
        ierr = col->getValuesArray(&floatarray);
        if (ierr >= 0) {
            if (cmp.getType() == ibis::qExpr::RANGE)
                ierr = doScan
                    (floatarray,
                     static_cast<const ibis::qContinuousRange&>(cmp),
                     mask,
                     *static_cast<array_t<float>*>(res));
            else
                ierr = doCompare(floatarray, cmp, mask,
                                 *static_cast<array_t<float>*>(res));
        }
        else if (! sname.empty()) {
            ierr = doCompare<float>
                (sname.c_str(), cmp, mask,
                 *static_cast<array_t<float>*>(res));
        }
        else {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << evt << " could not locate any data";
            ierr = -3;
        }
        break;}

    case ibis::DOUBLE: {
        ibis::array_t<double> doublearray;
        ierr = col->getValuesArray(&doublearray);
        if (ierr >= 0) {
            if (cmp.getType() == ibis::qExpr::RANGE)
                ierr = doScan
                    (doublearray,
                     static_cast<const ibis::qContinuousRange&>(cmp),
                     mask, *static_cast<array_t<double>*>(res));
            else
                ierr = doCompare(doublearray, cmp, mask,
                                 *static_cast<array_t<double>*>(res));
        }
        else if (! sname.empty()) {
            ierr = doCompare<double>
                (sname.c_str(), cmp, mask,
                 *static_cast<array_t<double>*>(res));
        }
        else {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << evt << " could not locate any data";
            ierr = -3;
        }
        break;}
    }

    LOGGER(ibis::gVerbose > 7)
        << evt << " evaluated to have " << mask.cnt() << " candidates and found "
        << ierr << " hits";
    return ierr;
} // ibis::part::doScan

/// Evalute the range condition and record the values satisfying the
/// condition in res.  The tests are only performed on the records that are
/// marked 1 in the mask (mask[i] == 1).  This function only works for
/// integers and floating-point numbers.
long ibis::part::doScan(const ibis::qRange &cmp,
                        const ibis::bitvector &mask,
                        void *res, ibis::bitvector &hits) const {
    if (columns.empty() || nEvents == 0 || cmp.colName() == 0 ||
        mask.size() == 0 || mask.cnt() == 0)
        return 0;

    std::string evt = "part[";
    evt += m_name;
    evt += "]::doScan";
    const ibis::column* col = getColumn(cmp.colName());
    if (col == 0) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " could not find named column "
            << cmp.colName();
        return -1;
    }
    if (ibis::gVerbose > 2) {
        std::ostringstream oss;
        oss << '(' << cmp << ')';
        evt += oss.str();
    }

    std::string sname;
    (void) col->dataFileName(sname);
    long ierr = 0;
    switch (col->type()) {
    default:
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " can not process data type "
            << col->type() << " (" << ibis::TYPESTRING[(int)col->type()] << ")";
        ierr = -2;
        break;

    case ibis::LONG: {
        try {
            ibis::array_t<int64_t> intarray;
            if (col->getValuesArray(&intarray) >= 0) {
                switch (cmp.getType()) {
                default: {
                    ierr = doCompare(intarray, cmp, mask,
                                     *static_cast<array_t<int64_t>*>(res),
                                     hits);
                    break;}
                case ibis::qExpr::RANGE: {
                    const ibis::qContinuousRange& rng =
                        static_cast<const ibis::qContinuousRange&>(cmp);
                    ierr = doScan(intarray, rng, mask,
                                  *static_cast<array_t<int64_t>*>(res), hits);
                    break;}
                }
            }
            else if (! sname.empty()) {
                ierr = doCompare<int64_t>
                    (sname.c_str(), cmp, mask,
                     *static_cast<array_t<int64_t>*>(res), hits);
            }
            else {
                LOGGER(ibis::gVerbose > 0)
                    << "Warning -- " << evt << " could not locate any data";
                hits.set(0, nEvents);
                ierr = -3;
            }
        }
        catch (const std::bad_alloc&) {
            ibis::util::emptyCache();
            if (! sname.empty()) {
                ierr = doCompare<int64_t>
                    (sname.c_str(), cmp, mask,
                     *static_cast<array_t<int64_t>*>(res), hits);
            }
            else {
                LOGGER(ibis::gVerbose > 0)
                    << "Warning -- " << evt << " could not locate any data";
                hits.set(0, nEvents);
                ierr = -3;
            }
        }
        catch (...) {
            throw;
        }
        break;}

    case ibis::ULONG: {
        try {
            ibis::array_t<uint64_t> intarray;
            if (col->getValuesArray(&intarray) == 0) {
                switch (cmp.getType()) {
                default: {
                    ierr = doCompare
                        (intarray, cmp, mask,
                         *static_cast<array_t<uint64_t>*>(res), hits);
                    break;}
                case ibis::qExpr::RANGE: {
                    const ibis::qContinuousRange& rng =
                        static_cast<const ibis::qContinuousRange&>(cmp);
                    ierr = doScan(intarray, rng, mask,
                                  *static_cast<array_t<uint64_t>*>(res),
                                  hits);
                    break;}
                }
            }
            else if (! sname.empty()) {
                ierr = doCompare<uint64_t>
                    (sname.c_str(), cmp, mask,
                     *static_cast<array_t<uint64_t>*>(res), hits);
            }
            else {
                LOGGER(ibis::gVerbose > 0)
                    << "Warning -- " << evt << " could not locate any data";
                hits.set(0, nEvents);
                ierr = -3;
            }
        }
        catch (const std::bad_alloc&) {
            ibis::util::emptyCache();
            if (! sname.empty()) {
                ierr = doCompare<uint64_t>
                    (sname.c_str(), cmp, mask,
                     *static_cast<array_t<uint64_t>*>(res), hits);
            }
            else {
                LOGGER(ibis::gVerbose > 0)
                    << "Warning -- " << evt << " could not locate any data";
                hits.set(0, nEvents);
                ierr = -3;
            }
        }
        catch (...) {
            throw;
        }
        break;}

    case ibis::INT: {
        try {
            ibis::array_t<int32_t> intarray;
            if (col->getValuesArray(&intarray) >= 0) {
                if (cmp.getType() == ibis::qExpr::RANGE) {
                    const ibis::qContinuousRange &rng =
                        static_cast<const ibis::qContinuousRange&>(cmp);
                    ierr = doScan(intarray, rng, mask,
                                  *static_cast<array_t<int32_t>*>(res),
                                  hits);
                }
                else {
                    ierr = doCompare(intarray, cmp, mask,
                                     *static_cast<array_t<int32_t>*>(res),
                                     hits);
                }
            }
            else if (! sname.empty()) {
                ierr = doCompare<int32_t>
                    (sname.c_str(), cmp, mask,
                     *static_cast<array_t<int32_t>*>(res), hits);
            }
            else {
                LOGGER(ibis::gVerbose > 0)
                    << "Warning -- " << evt << " could not locate any data";
                hits.set(0, nEvents);
                ierr = -3;
            }
        }
        catch (const std::bad_alloc&) {
            ibis::util::emptyCache();
            if (! sname.empty()) {
                ierr = doCompare<int32_t>
                    (sname.c_str(), cmp, mask,
                     *static_cast<array_t<int32_t>*>(res), hits);
            }
            else {
                LOGGER(ibis::gVerbose > 0)
                    << "Warning -- " << evt << " could not locate any data";
                hits.set(0, nEvents);
                ierr = -3;
            }
        }
        catch (...) {
            throw;
        }
        break;}

    case ibis::UINT: {
        try {
            ibis::array_t<uint32_t> intarray;
            if (col->getValuesArray(&intarray) == 0) {
                if (cmp.getType() == ibis::qExpr::RANGE) {
                    const ibis::qContinuousRange &rng =
                        static_cast<const ibis::qContinuousRange&>(cmp);
                    ierr = doScan
                        (intarray, rng, mask,
                         *static_cast<array_t<uint32_t>*>(res), hits);
                }
                else {
                    ierr = doCompare
                        (intarray, cmp, mask,
                         *static_cast<array_t<uint32_t>*>(res), hits);
                }
            }
            else if (! sname.empty()) {
                ierr = doCompare<uint32_t>
                    (sname.c_str(), cmp, mask,
                     *static_cast<array_t<uint32_t>*>(res), hits);
            }
            else {
                LOGGER(ibis::gVerbose > 0)
                    << "Warning -- " << evt << " could not locate any data";
                hits.set(0, nEvents);
                ierr = -3;
            }
        }
        catch (const std::bad_alloc&) {
            ibis::util::emptyCache();
            if (! sname.empty()) {
                ierr = doCompare<uint32_t>
                    (sname.c_str(), cmp, mask,
                     *static_cast<array_t<uint32_t>*>(res), hits);
            }
            else {
                LOGGER(ibis::gVerbose > 0)
                    << "Warning -- " << evt << " could not locate any data";
                hits.set(0, nEvents);
                ierr = -3;
            }
        }
        catch (...) {
            throw;
        }
        break;}

    case ibis::SHORT: {
        try {
            ibis::array_t<int16_t> intarray;
            if (col->getValuesArray(&intarray) >= 0) {
                if (cmp.getType() == ibis::qExpr::RANGE) {
                    const ibis::qContinuousRange &rng =
                        static_cast<const ibis::qContinuousRange&>(cmp);
                    ierr = doScan(intarray, rng, mask,
                                  *static_cast<array_t<int16_t>*>(res),
                                  hits);
                }
                else {
                    ierr = doCompare(intarray, cmp, mask,
                                     *static_cast<array_t<int16_t>*>(res),
                                     hits);
                }
            }
            else if (! sname.empty()) {
                ierr = doCompare<int16_t>
                    (sname.c_str(), cmp, mask,
                     *static_cast<array_t<int16_t>*>(res), hits);
            }
            else {
                LOGGER(ibis::gVerbose > 0)
                    << "Warning -- " << evt << " could not locate any data";
                hits.set(0, nEvents);
                ierr = -3;
            }
        }
        catch (const std::bad_alloc&) {
            ibis::util::emptyCache();
            if (! sname.empty()) {
                ierr = doCompare<int16_t>
                    (sname.c_str(), cmp, mask,
                     *static_cast<array_t<int16_t>*>(res), hits);
            }
            else {
                LOGGER(ibis::gVerbose > 0)
                    << "Warning -- " << evt << " could not locate any data";
                hits.set(0, nEvents);
                ierr = -3;
            }
        }
        catch (...) {
            throw;
        }
        break;}

    case ibis::USHORT: {
        try {
            ibis::array_t<uint16_t> intarray;
            if (col->getValuesArray(&intarray) >= 0) {
                if (cmp.getType() == ibis::qExpr::RANGE) {
                    const ibis::qContinuousRange &rng =
                        static_cast<const ibis::qContinuousRange&>(cmp);
                    ierr = doScan(intarray, rng, mask,
                                  *static_cast<array_t<uint16_t>*>(res),
                                  hits);
                }
                else {
                    ierr = doCompare
                        (intarray, cmp, mask,
                         *static_cast<array_t<uint16_t>*>(res), hits);
                }
            }
            else if (! sname.empty()) {
                ierr = doCompare<uint16_t>
                    (sname.c_str(), cmp, mask,
                     *static_cast<array_t<uint16_t>*>(res), hits);
            }
            else {
                LOGGER(ibis::gVerbose > 0)
                    << "Warning -- " << evt << " could not locate any data";
                hits.set(0, nEvents);
                ierr = -3;
            }
        }
        catch (const std::bad_alloc&) {
            ibis::util::emptyCache();
            if (! sname.empty()) {
                ierr = doCompare<uint16_t>
                    (sname.c_str(), cmp, mask,
                     *static_cast<array_t<uint16_t>*>(res), hits);
            }
            else {
                LOGGER(ibis::gVerbose > 0)
                    << "Warning -- " << evt << " could not locate any data";
                hits.set(0, nEvents);
                ierr = -3;
            }
        }
        catch (...) {
            throw;
        }
        break;}

    case ibis::BYTE: {
        try {
            ibis::array_t<signed char> intarray;
            if (col->getValuesArray(&intarray) >= 0) {
                if (cmp.getType() == ibis::qExpr::RANGE) {
                    const ibis::qContinuousRange &rng =
                        static_cast<const ibis::qContinuousRange&>(cmp);
                    ierr = doScan(intarray, rng, mask,
                                  *static_cast<array_t<signed char>*>(res),
                                  hits);
                }
                else {
                    ierr = doCompare(intarray, cmp, mask,
                                     *static_cast<array_t<signed char>*>(res),
                                     hits);
                }
            }
            else if (! sname.empty()) {
                ierr = doCompare<signed char>
                    (sname.c_str(), cmp, mask,
                     *static_cast<array_t<signed char>*>(res), hits);
            }
            else {
                LOGGER(ibis::gVerbose > 0)
                    << "Warning -- " << evt << " could not locate any data";
                hits.set(0, nEvents);
                ierr = -3;
            }
        }
        catch (const std::bad_alloc&) {
            ibis::util::emptyCache();
            if (! sname.empty()) {
                ierr = doCompare<signed char>
                    (sname.c_str(), cmp, mask,
                     *static_cast<array_t<signed char>*>(res), hits);
            }
            else {
                LOGGER(ibis::gVerbose > 0)
                    << "Warning -- " << evt << " could not locate any data";
                hits.set(0, nEvents);
                ierr = -3;
            }
        }
        catch (...) {
            throw;
        }
        break;}

    case ibis::UBYTE: {
        try {
            ibis::array_t<unsigned char> intarray;
            if (col->getValuesArray(&intarray) >= 0) {
                if (cmp.getType() == ibis::qExpr::RANGE) {
                    const ibis::qContinuousRange &rng =
                        static_cast<const ibis::qContinuousRange&>(cmp);
                    ierr = doScan(intarray, rng, mask,
                                  *static_cast<array_t<unsigned char>*>(res),
                                  hits);
                }
                else {
                    ierr = doCompare
                        (intarray, cmp, mask,
                         *static_cast<array_t<unsigned char>*>(res),
                         hits);
                }
            }
            else if (! sname.empty()) {
                ierr = doCompare<unsigned char>
                    (sname.c_str(), cmp, mask,
                     *static_cast<array_t<unsigned char>*>(res),
                     hits);
            }
            else {
                LOGGER(ibis::gVerbose > 0)
                    << "Warning -- " << evt << " could not locate any data";
                hits.set(0, nEvents);
                ierr = -3;
            }
        }
        catch (const std::bad_alloc&) {
            ibis::util::emptyCache();
            if (! sname.empty()) {
                ierr = doCompare<unsigned char>
                    (sname.c_str(), cmp, mask,
                     *static_cast<array_t<unsigned char>*>(res), hits);
            }
            else {
                LOGGER(ibis::gVerbose > 0)
                    << "Warning -- " << evt << " could not locate any data";
                hits.set(0, nEvents);
                ierr = -3;
            }
        }
        catch (...) {
            throw;
        }
        break;}

    case ibis::FLOAT: {
        try {
            ibis::array_t<float> floatarray;
            if (col->getValuesArray(&floatarray) >= 0) {
                if (cmp.getType() == ibis::qExpr::RANGE)
                    ierr = doScan
                        (floatarray,
                         static_cast<const ibis::qContinuousRange&>(cmp),
                         mask, *static_cast<array_t<float>*>(res), hits);
                else
                    ierr = doCompare(floatarray, cmp, mask,
                                     *static_cast<array_t<float>*>(res), hits);
            }
            else if (! sname.empty()) {
                ierr = doCompare<float>
                    (sname.c_str(), cmp, mask,
                     *static_cast<array_t<float>*>(res), hits);
            }
            else {
                LOGGER(ibis::gVerbose > 0)
                    << "Warning -- " << evt << " could not locate any data";
                hits.set(0, nEvents);
                ierr = -3;
            }
        }
        catch (const std::bad_alloc&) {
            ibis::util::emptyCache();
            if (! sname.empty()) {
                ierr = doCompare<float>
                    (sname.c_str(), cmp, mask,
                     *static_cast<array_t<float>*>(res), hits);
            }
            else {
                LOGGER(ibis::gVerbose > 0)
                    << "Warning -- " << evt << " could not locate any data";
                hits.set(0, nEvents);
                ierr = -3;
            }
        }
        catch (...) {
            throw;
        }
        break;}

    case ibis::DOUBLE: {
        try {
            ibis::array_t<double> doublearray;
            if (col->getValuesArray(&doublearray) >= 0) {
                if (cmp.getType() == ibis::qExpr::RANGE)
                    ierr = doScan
                        (doublearray,
                         static_cast<const ibis::qContinuousRange&>(cmp),
                         mask, *static_cast<array_t<double>*>(res), hits);
                else
                    ierr = doCompare(doublearray, cmp, mask,
                                     *static_cast<array_t<double>*>(res),
                                     hits);
            }
            else if (! sname.empty()) {
                ierr = doCompare<double>
                    (sname.c_str(), cmp, mask,
                     *static_cast<array_t<double>*>(res), hits);
            }
            else {
                LOGGER(ibis::gVerbose > 0)
                    << "Warning -- " << evt << " could not locate any data";
                hits.set(0, nEvents);
                ierr = -3;
            }
        }
        catch (const std::bad_alloc&) {
            ibis::util::emptyCache();
            if (! sname.empty()) {
                ierr = doCompare<double>
                    (sname.c_str(), cmp, mask,
                     *static_cast<array_t<double>*>(res), hits);
            }
            else {
                LOGGER(ibis::gVerbose > 0)
                    << "Warning -- " << evt << " could not locate any data";
                hits.set(0, nEvents);
                ierr = -3;
            }
        }
        catch (...) {
            throw;
        }
        break;}
    }

    LOGGER(ibis::gVerbose > 7)
        << evt << " examined " << mask.cnt() << " candidates"
        << " and found " << ierr << " hits";
    return ierr;
} // ibis::part::doScan

/// Locate the records that satisfy the range condition.
/// A generic scan function that rely on the virtual function
/// ibis::range::inRange.
/// This static member function works on an array provided by the
/// caller.  Since the values are provided, this function does not check
/// the name of the variable involved in the range condition.
/// @note The incoming values in varr is an ibis::array_t type.  This
/// limits the type of data supported by this function.
template <typename E>
long ibis::part::doScan(const array_t<E> &varr,
                        const ibis::qRange &cmp,
                        const ibis::bitvector &mask,
                        ibis::bitvector &hits) {
    long ierr = 0;
    ibis::horometer timer;
    if (ibis::gVerbose > 3)
        timer.start();
    hits.set(0, mask.size());
    hits.decompress();
    for (ibis::bitvector::indexSet is = mask.firstIndexSet();
         is.nIndices() > 0; ++ is) {
        const ibis::bitvector::word_t *iix = is.indices();
        if (is.isRange()) {
            const uint32_t last = (varr.size()>=iix[1] ? iix[1] : varr.size());
            for (uint32_t i = iix[0]; i < last; ++ i) {
                if (cmp.inRange(varr[i])) {
                    ++ ierr;
                    hits.setBit(i, 1);
#if DEBUG+0 > 1 || _DEBUG+0 > 1
                    LOGGER(ibis::gVerbose >= 0) << varr[i] << " is in " << cmp;
#endif
                }
            }
        }
        else {
            for (uint32_t i = 0; i < is.nIndices(); ++ i) {
                if (iix[i] < varr.size() && cmp.inRange(varr[iix[i]])) {
                    ++ ierr;
                    hits.setBit(iix[i], 1);
#if DEBUG+0 > 1 || _DEBUG+0 > 1
                    LOGGER(ibis::gVerbose >= 0)
                        << varr[iix[i]] << " is in " << cmp;
#endif
                }
            }
        }
    } // main loop

    hits.compress();
    if (ibis::gVerbose > 3 && ierr >= 0) {
        timer.stop();
        ibis::util::logger lg;
        lg() << "part::doScan<" << typeid(E).name() << "> -- evaluating "
             << cmp << " on " << mask.cnt()
             << (mask.cnt() > 1 ? " values" : " value")
             << " (total: " << mask.size() << ") took "
             << timer.realTime() << " sec elapsed time and produced "
             << ierr << (ierr > 1 ? " hits" : " hit");
#if DEBUG+0 > 1 || _DEBUG+0 > 1
        lg() << "\nmask\n" << mask;
        lg() << "\nhit vector\n" << hits << "\n";
#endif
    }
    return ierr;
} // ibis::part::doScan

/// Compute the records (marked 1 in the mask) that does not satisfy the
/// range condition.  Only the entries with mask[i] == 1 are examined,
/// those with mask[i] == 0 will never be hits.
long ibis::part::negativeScan(const ibis::qRange &cmp,
                              const ibis::bitvector &mask,
                              ibis::bitvector &hits) const {
    if (columns.empty() || nEvents == 0 || cmp.colName() == 0 ||
        mask.size() == 0 || mask.cnt() == 0)
        return 0;

    std::string evt = "part[";
    evt += m_name;
    evt += "]::negativeScan";
    const ibis::column* col = getColumn(cmp.colName());
    if (col == 0) {
        LOGGER(ibis::gVerbose > 2)
            << "Warning -- " << evt << " could not find named column "
            << cmp.colName();
        return -1;
    }

    long ierr = 0;
    std::string sname;
    (void) col->dataFileName(sname);
    switch (col->type()) {
    default:
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " can not process data type "
            << col->type() << " (" << ibis::TYPESTRING[(int)col->type()] << ")";
        hits.set(0, nEvents);
        ierr = -2;
        break;

    case CATEGORY: {
        ibis::bitvector tmp;
        if (cmp.getType() == ibis::qExpr::RANGE)
            col->estimateRange
                (reinterpret_cast<const ibis::qContinuousRange&>(cmp),
                 hits, tmp);
        else
            col->estimateRange
                (reinterpret_cast<const ibis::qDiscreteRange&>(cmp),
                 hits, tmp);
        hits &= mask;
        ierr = hits.sloppyCount();
        break;}

    case ibis::LONG: {
        array_t<int64_t> intarray;
        ierr = col->getValuesArray(&intarray);
        if (ierr >= 0) {
            switch (cmp.getType()) {
            default:
                ierr = negativeCompare(intarray, cmp, mask, hits);
                break;
            case ibis::qExpr::INTHOD: {
                const ibis::qIntHod& qih =
                    static_cast<const ibis::qIntHod&>(cmp);
                ierr = negativeCompare(intarray, qih, mask, hits);
                break;}
            case ibis::qExpr::UINTHOD: {
                const ibis::qIntHod& qih =
                    static_cast<const ibis::qIntHod&>(cmp);
                ierr = negativeCompare(intarray, qih, mask, hits);
                break;}
            }
        }
        else if (! sname.empty()) {
            switch (cmp.getType()) {
            default:
                ierr = negativeCompare<int64_t>(sname.c_str(), cmp, mask, hits);
                break;
            case ibis::qExpr::INTHOD: {
                const ibis::qIntHod& qih =
                    static_cast<const ibis::qIntHod&>(cmp);
                ierr = negativeCompare<int64_t>(sname.c_str(), qih, mask, hits);
                break;}
            case ibis::qExpr::UINTHOD: {
                const ibis::qIntHod& qih =
                    static_cast<const ibis::qIntHod&>(cmp);
                ierr = negativeCompare<int64_t>(sname.c_str(), qih, mask, hits);
                break;}
            }
        }
        else {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << evt << " could not locate any data";
            hits.set(0, nEvents);
            ierr = -3;
        }
        break;}

    case ibis::ULONG: {
        array_t<uint64_t> intarray;
        ierr = col->getValuesArray(&intarray);
        if (ierr >= 0) {
            switch (cmp.getType()) {
            default:
                ierr = negativeCompare(intarray, cmp, mask, hits);
                break;
            case ibis::qExpr::INTHOD: {
                const ibis::qIntHod& qih =
                    static_cast<const ibis::qIntHod&>(cmp);
                ierr = negativeCompare(intarray, qih, mask, hits);
                break;}
            case ibis::qExpr::UINTHOD: {
                const ibis::qIntHod& qih =
                    static_cast<const ibis::qIntHod&>(cmp);
                ierr = negativeCompare(intarray, qih, mask, hits);
                break;}
            }
        }
        else if (! sname.empty()) {
            switch (cmp.getType()) {
            default:
                ierr = negativeCompare<uint64_t>
                    (sname.c_str(), cmp, mask, hits);
                break;
            case ibis::qExpr::INTHOD: {
                const ibis::qIntHod& qih =
                    static_cast<const ibis::qIntHod&>(cmp);
                ierr = negativeCompare<uint64_t>
                    (sname.c_str(), qih, mask, hits);
                break;}
            case ibis::qExpr::UINTHOD: {
                const ibis::qIntHod& qih =
                    static_cast<const ibis::qIntHod&>(cmp);
                ierr = negativeCompare<uint64_t>
                    (sname.c_str(), qih, mask, hits);
                break;}
            }
        }
        else {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << evt << " could not locate any data";
            hits.set(0, nEvents);
            ierr = -3;
        }
        break;}

    case ibis::INT: {
        array_t<int32_t> intarray;
        ierr = col->getValuesArray(&intarray);
        if (ierr >= 0) {
            ierr = negativeCompare(intarray, cmp, mask, hits);
        }
        else if (! sname.empty()) {
            ierr = negativeCompare<int32_t>(sname.c_str(), cmp, mask, hits);
        }
        else {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << evt << " could not locate any data";
            hits.set(0, nEvents);
            ierr = -3;
        }
        break;}

    case TEXT:
    case ibis::UINT: {
        array_t<uint32_t> intarray;
        ierr = col->getValuesArray(&intarray);
        if (ierr >= 0) {
            ierr = negativeCompare(intarray, cmp, mask, hits);
        }
        else if (! sname.empty()) {
            ierr = negativeCompare<uint32_t>(sname.c_str(), cmp, mask, hits);
        }
        else {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << evt << " could not locate any data";
            hits.set(0, nEvents);
            ierr = -3;
        }
        break;}

    case ibis::SHORT: {
        array_t<int16_t> intarray;
        ierr = col->getValuesArray(&intarray);
        if (ierr >= 0) {
            ierr = negativeCompare(intarray, cmp, mask, hits);
        }
        else if (! sname.empty()) {
            ierr = negativeCompare<int16_t>(sname.c_str(), cmp, mask, hits);
        }
        else {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << evt << " could not locate any data";
            hits.set(0, nEvents);
            ierr = -3;
        }
        break;}

    case ibis::USHORT: {
        array_t<uint16_t> intarray;
        ierr = col->getValuesArray(&intarray);
        if (ierr >= 0) {
            ierr = negativeCompare(intarray, cmp, mask, hits);
        }
        else if (! sname.empty()) {
            ierr = negativeCompare<uint16_t>(sname.c_str(), cmp, mask, hits);
        }
        else {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << evt << " could not locate any data";
            hits.set(0, nEvents);
            ierr = -3;
        }
        break;}

    case ibis::BYTE: {
        array_t<signed char> intarray;
        ierr = col->getValuesArray(&intarray);
        if (ierr >= 0) {
            ierr = negativeCompare(intarray, cmp, mask, hits);
        }
        else if (! sname.empty()) {
            ierr = negativeCompare<signed char>(sname.c_str(), cmp, mask, hits);
        }
        else {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << evt << " could not locate any data";
            hits.set(0, nEvents);
            ierr = -3;
        }
        break;}

    case ibis::UBYTE: {
        array_t<unsigned char> intarray;
        ierr = col->getValuesArray(&intarray);
        if (ierr >= 0) {
            ierr = negativeCompare(intarray, cmp, mask, hits);
        }
        else if (! sname.empty()) {
            ierr = negativeCompare<unsigned char>
                (sname.c_str(), cmp, mask, hits);
        }
        else {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << evt << " could not locate any data";
            hits.set(0, nEvents);
            ierr = -3;
        }
        break;}

    case ibis::FLOAT: {
        array_t<float> floatarray;
        ierr = col->getValuesArray(&floatarray);
        if (ierr >= 0) {
            ierr = negativeCompare(floatarray, cmp, mask, hits);
        }
        else if (! sname.empty()) {
            ierr = negativeCompare<float>(sname.c_str(), cmp, mask, hits);
        }
        else {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << evt << " could not locate any data";
            hits.set(0, nEvents);
            ierr = -3;
        }
        break;}

    case ibis::DOUBLE: {
        array_t<double> doublearray;
        ierr = col->getValuesArray(&doublearray);
        if (ierr >= 0) {
            ierr = negativeCompare(doublearray, cmp, mask, hits);
        }
        else if (! sname.empty()) {
            ierr = negativeCompare<double>(sname.c_str(), cmp, mask, hits);
        }
        else {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << evt << " could not locate any data";
            hits.set(0, nEvents);
            ierr = -3;
        }
        break;}
    }

    if (hits.size() != nEvents) {
        hits.adjustSize(0, nEvents); // append 0 bits or remove extra bits
    }
    LOGGER(ibis::gVerbose > 7)
        << evt << " examined " << mask.cnt() << " candidates and found "
        << hits.cnt() << " hits";
    return ierr;
} // ibis::part::negativeScan

/// Logically, iffy = high - low, were high and low are computed from
/// estimateRange.  The return value is the estimated fraction of records
/// that might satisfy the range condition.
float ibis::part::getUndecidable(const ibis::qContinuousRange &cmp,
                                 ibis::bitvector &iffy) const {
    float ret = 0;
    if (columns.empty() || nEvents == 0)
        return ret;
    if (cmp.colName() == 0 || *cmp.colName() == 0 ||
        (cmp.leftOperator() == ibis::qExpr::OP_UNDEFINED &&
         cmp.rightOperator() == ibis::qExpr::OP_UNDEFINED))
        return ret;

    const ibis::column* col = getColumn(cmp.colName());
    if (col != 0) {
        ret = col->getUndecidable(cmp, iffy);
    }
    else {
        LOGGER(ibis::gVerbose > 2)
            << "Warning -- part[" << name()
            << "]::getUndecidable could not find a column named "
            << cmp.colName();
    }

    LOGGER(ibis::gVerbose > 7)
        << "part[" << name() << "]::getUndecidable("
        << cmp << ") get a bitvector with " << iffy.cnt()
        << " nonzeros, " << ret*100
        << " per cent of them might be in the range";
    return ret;
} // ibis::part::getUndecidable

float ibis::part::getUndecidable(const ibis::qDiscreteRange &cmp,
                                 ibis::bitvector &iffy) const {
    float ret = 0;
    if (columns.empty() || nEvents == 0 || cmp.colName() == 0)
        return ret;

    const ibis::column* col = getColumn(cmp.colName());
    if (col != 0) {
        ret = col->getUndecidable(cmp, iffy);
    }
    else {
        LOGGER(ibis::gVerbose > 2)
            << "Warning -- part[" << name()
            << "]::getUndecidable could not find a column named "
            << cmp.colName();
    }

    LOGGER(ibis::gVerbose > 7)
        << "part[" << name() << "]::getUndecidable("
        << cmp.colName() << " IN ...) get a bitvector with " << iffy.cnt()
        << " nonzeros, " << ret*100
        << " per cent of them might be in the range";
    return ret;
} // ibis::part::getUndecidable

float ibis::part::getUndecidable(const ibis::qIntHod &cmp,
                                 ibis::bitvector &iffy) const {
    float ret = 0;
    if (columns.empty() || nEvents == 0 || cmp.colName() == 0)
        return ret;

    const ibis::column* col = getColumn(cmp.colName());
    if (col != 0) {
        ret = col->getUndecidable(cmp, iffy);
    }
    else {
        LOGGER(ibis::gVerbose > 2)
            << "Warning -- part[" << name()
            << "]::getUndecidable could not find a column named "
            << cmp.colName();
    }

    LOGGER(ibis::gVerbose > 7)
        << "part[" << name() << "]::getUndecidable("
        << cmp.colName() << " IN ...) get a bitvector with " << iffy.cnt()
        << " nonzeros, " << ret*100
        << " per cent of them might be in the range";
    return ret;
} // ibis::part::getUndecidable

float ibis::part::getUndecidable(const ibis::qUIntHod &cmp,
                                 ibis::bitvector &iffy) const {
    float ret = 0;
    if (columns.empty() || nEvents == 0 || cmp.colName() == 0)
        return ret;

    const ibis::column* col = getColumn(cmp.colName());
    if (col != 0) {
        ret = col->getUndecidable(cmp, iffy);
    }
    else {
        LOGGER(ibis::gVerbose > 2)
            << "Warning -- part[" << name()
            << "]::getUndecidable could not find a column named "
            << cmp.colName();
    }

    LOGGER(ibis::gVerbose > 7)
        << "part[" << name() << "]::getUndecidable("
        << cmp.colName() << " IN ...) get a bitvector with " << iffy.cnt()
        << " nonzeros, " << ret*100
        << " per cent of them might be in the range";
    return ret;
} // ibis::part::getUndecidable

/// Sequential scan without a mask.  It assumes that every valid row is to
/// be examined.
long ibis::part::doScan(const ibis::compRange &cmp,
                        ibis::bitvector &hits) const {
    if (columns.empty() || nEvents == 0)
        return 0;

    ibis::bitvector mask;
    mask.set(1, nEvents); // initialize mask to have all set bit
    return doScan(cmp, mask, hits);
} // ibis::part::doScan

/// Locate the records that have mark value 1 and satisfy the complex
/// range conditions.
/// This implementation uses ibis::part::barrel for handling actual values
/// needed.
long ibis::part::doScan(const ibis::compRange &cmp,
                        const ibis::bitvector &mask,
                        ibis::bitvector &hits) const {
    if (columns.empty() || nEvents == 0)
        return 0;

    ibis::horometer timer;
    if (ibis::gVerbose > 3) {
        LOGGER(ibis::gVerbose > 4)
            << "part[" << name()
            << "]::doScan - starting scanning data for \"" << cmp
            << "\" with mask (" << mask.cnt() << " out of "
            << mask.size() << ")";
        timer.start();
    }

    ibis::part::barrel vlist(this);
    long ierr = 0;
    if (cmp.getLeft())
        vlist.recordVariable(static_cast<const ibis::math::term*>
                             (cmp.getLeft()));
    if (cmp.getRight())
        vlist.recordVariable(static_cast<const ibis::math::term*>
                             (cmp.getRight()));
    if (cmp.getTerm3())
        vlist.recordVariable(static_cast<const ibis::math::term*>
                             (cmp.getTerm3()));

    // ibis::bitvector mymask;
    // vlist.getNullMask(mymask);
    // mymask &= mask;
    if (vlist.size() == 0) { // a constant expression
        if (cmp.inRange())
            hits.copy(mask);
        else
            hits.set(mask.size(), 0);
        ierr = hits.sloppyCount();
        return ierr;
    }

    ierr = vlist.open();
    if (ierr < 0) {
        LOGGER(ibis::gVerbose > 2)
            << "Warning -- part[" << (m_name ? m_name : "?")
            << "]::doScan -- failed to prepare data for " << cmp
            << IBIS_FILE_LINE;
        throw "part::doScan -- failed to prepare data" IBIS_FILE_LINE;
    }

    const bool uncomp = ((mask.size() >> 8) < mask.cnt());
    if (uncomp) { // use uncompressed hits internally
        hits.set(0, mask.size());
        hits.decompress();
    }
    else {
        hits.clear();
        hits.reserve(mask.size(), mask.cnt());
    }

    // attempt to feed the values into vlist and evaluate the arithmetic
    // expression through ibis::compRange::inRange
    ibis::bitvector::indexSet idx = mask.firstIndexSet();
    const ibis::bitvector::word_t *iix = idx.indices();
    while (idx.nIndices() > 0) {
        if (idx.isRange()) {
            // move the file pointers of open files
            vlist.seek(*iix);
            for (uint32_t j = 0; j < idx.nIndices(); ++j) {
                vlist.read(); // read the value into memory
                if (cmp.inRange()) // actual comparison
                    hits.setBit(j + *iix, 1);
            } // for (uint32_t j = 0; j < idx.nIndices(); ++j)
        }
        else {
            for (uint32_t j = 0; j < idx.nIndices(); ++j) {
                // move the file pointers of open files
                vlist.seek(iix[j]);
                vlist.read(); // read values
                if (cmp.inRange()) // actual comparison
                    hits.setBit(iix[j], 1);
            } // for (uint32_t j = 0; j < idx.nIndices(); ++j)
        }

        ++ idx;
    } // while (idx.nIndices() > 0)

    if (uncomp)
        hits.compress();
    else if (hits.size() < nEvents)
        hits.setBit(nEvents-1, 0);

    if (ierr >= 0) {
        if (ibis::gVerbose > 3) {
            timer.stop();
            ierr = hits.cnt();
            ibis::util::logger lg;
            lg() << "part[" << (m_name ? m_name : "?")
                 << "]::doScan -- evaluating "
                 << cmp << " on " << mask.cnt() << " records (total: "
                 << nEvents << ") took " << timer.realTime()
                 << " sec elapsed time and produced "
                 << ierr << " hit" << (ierr>1?"s":"");
#if DEBUG+0 > 1 || _DEBUG+0 > 1
            lg() << "\nmask\n" << mask << "\nhit vector\n" << hits;
#endif
        }
        else {
            ierr = hits.sloppyCount();
        }
    }
    return ierr;
} // ibis::part::doScan

/// Calculate the values of an arithmetic expression as doubles.
/// The arithmetic expression is applied to each row that are marked 1 in
/// the mask, msk, with names in the arithmetic expression interpretted as
/// column names.  The resulting values are packed into the array res as
/// doubles.  Upon the successful completion of this function, the return
/// value should be the number of records examined, which should be same as
/// msk.cnt() and res.size().
long ibis::part::calculate(const ibis::math::term &trm,
                           const ibis::bitvector &msk,
                           array_t<double> &res) const {
    if (columns.empty() || nEvents == 0 || msk.size() == 0 || msk.cnt() == 0)
        return 0;

    long ierr = 0;
    ibis::horometer timer;
    if (ibis::gVerbose > 3) {
        LOGGER(ibis::gVerbose > 4)
            << "part[" << name()
            << "]::calculate - starting to evaluate \"" << trm
            << "\" with mask (" << msk.cnt() << " out of "
            << msk.size() << ")";
        timer.start();
    }

    ibis::part::barrel vlist(this);
    vlist.recordVariable(&trm);
    res.reserve(msk.cnt());
    res.clear(); // clear the existing content
    if (vlist.size() == 0) { // a constant expression
        res.resize(msk.cnt());
        const double val = trm.eval();
        for (unsigned i = 0; i < msk.cnt(); ++ i)
            res[i] = val;
        return msk.cnt();
    }
    if (trm.termType() == ibis::math::VARIABLE) { // a single variable
        const ibis::math::variable &var =
            static_cast<const ibis::math::variable&>(trm);
        array_t<double> *tmp = selectDoubles(var.variableName(), msk);
        if (tmp != 0) {
            res.swap(*tmp);
            delete tmp;
            return res.size();
        }
        else {
            return -1;
        }
    }

    // open all necessary files
    ierr = vlist.open();
    if (ierr < 0) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- part[" << (m_name ? m_name : "?")
            << "]::calculate -- failed to prepare data for " << trm
            << IBIS_FILE_LINE;
        throw "part::calculate -- failed to prepare data" IBIS_FILE_LINE;
    }

    // feed the values into vlist and evaluate the arithmetic expression
    ibis::bitvector::indexSet idx = msk.firstIndexSet();
    const ibis::bitvector::word_t *iix = idx.indices();
    while (idx.nIndices() > 0) {
        if (idx.isRange()) {
            // move the file pointers of open files
            vlist.seek(*iix);
            for (uint32_t j = 0; j < idx.nIndices(); ++j) {
                vlist.read();
                res.push_back(trm.eval());
            } // for (uint32_t j = 0; j < idx.nIndices(); ++j)
        }
        else {
            for (uint32_t j = 0; j < idx.nIndices(); ++j) {
                vlist.seek(iix[j]);
                vlist.read();
                res.push_back(trm.eval());
            } // for (uint32_t j = 0; j < idx.nIndices(); ++j)
        }

        ++ idx;
    } // while (idx.nIndices() > 0)

    if (ibis::gVerbose > 3) {
        timer.stop();
        ibis::util::logger lg;
        lg() << "part[" << (m_name ? m_name : "?")
             << "]::calculate -- evaluating " << trm << " on "
             << msk.cnt() << " records (total: " << nEvents
             << ") took " << timer.realTime()
             << " sec elapsed time and produced " << res.size()
             << " value" << (res.size() > 1 ? "s" : "");
    }
    if (ierr >= 0)
        ierr = res.size();
    return ierr;
} // ibis::part::calculate

/// Calculate the values of a math expression as strings.
/// The expression is applied to each row that are marked 1 in
/// the mask, msk, with names in the arithmetic expression interpretted as
/// column names.  The resulting values are packed into the array res as
/// strings.  Upon the successful completion of this function, the return
/// value should be the number of records examined, which should be same as
/// msk.cnt() and res.size().
long ibis::part::calculate(const ibis::math::stringFunction1 &trm,
                           const ibis::bitvector &msk,
                           std::vector<std::string> &res) const {
    if (columns.empty() || nEvents == 0 || msk.size() == 0 || msk.cnt() == 0)
        return 0;

    long ierr = 0;
    ibis::horometer timer;
    if (ibis::gVerbose > 3) {
        LOGGER(ibis::gVerbose > 4)
            << "part[" << name()
            << "]::calculate - starting to evaluate \"" << trm
            << "\" with mask (" << msk.cnt() << " out of "
            << msk.size() << ")";
        timer.start();
    }

    ibis::part::barrel vlist(this);
    vlist.recordVariable(&trm);
    res.reserve(msk.cnt());
    res.clear(); // clear the existing content
    if (vlist.size() == 0) { // a constant expression
        res.resize(msk.cnt());
        const std::string val = trm.sval();
        for (unsigned i = 0; i < msk.cnt(); ++ i)
            res[i] = val;
        return msk.cnt();
    }

    // open all necessary files
    ierr = vlist.open();
    if (ierr < 0) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- part[" << (m_name ? m_name : "?")
            << "]::calculate -- failed to prepare data for " << trm
            << IBIS_FILE_LINE;
        throw "part::calculate -- failed to prepare data" IBIS_FILE_LINE;
    }

    // feed the values into vlist and evaluate the arithmetic expression
    ibis::bitvector::indexSet idx = msk.firstIndexSet();
    const ibis::bitvector::word_t *iix = idx.indices();
    while (idx.nIndices() > 0) {
        if (idx.isRange()) {
            // move the file pointers of open files
            vlist.seek(*iix);
            for (uint32_t j = 0; j < idx.nIndices(); ++j) {
                vlist.read();
                res.push_back(trm.sval());
            } // for (uint32_t j = 0; j < idx.nIndices(); ++j)
        }
        else {
            for (uint32_t j = 0; j < idx.nIndices(); ++j) {
                vlist.seek(iix[j]);
                vlist.read();
                res.push_back(trm.sval());
            } // for (uint32_t j = 0; j < idx.nIndices(); ++j)
        }

        ++ idx;
    } // while (idx.nIndices() > 0)

    if (ibis::gVerbose > 3) {
        timer.stop();
        ibis::util::logger lg;
        lg() << "part[" << (m_name ? m_name : "?")
             << "]::calculate -- evaluating " << trm << " on "
             << msk.cnt() << " records (total: " << nEvents
             << ") took " << timer.realTime()
             << " sec elapsed time and produced " << res.size()
             << " value" << (res.size() > 1 ? "s" : "");
    }
    if (ierr >= 0)
        ierr = res.size();
    return ierr;
} // ibis::part::calculate

/// Treat the arithmetic expression as true or false.  The arithmetic
/// expression is evaluated, nonzero values are treated as true and others
/// are treated as false.  This function only uses the test 'eval() != 0',
/// which will treat all NaN as false.
long ibis::part::doScan(const ibis::math::term &trm,
                        const ibis::bitvector &msk,
                        ibis::bitvector &res) const {
    res.clear();
    if (columns.empty() || nEvents == 0 || msk.size() == 0)
        return 0;
    if (msk.cnt() == 0) {
        res.copy(msk);
        return 0;
    }

    ibis::horometer timer;
    if (ibis::gVerbose > 3) {
        LOGGER(ibis::gVerbose > 4)
            << "part[" << name()
            << "]::doScan - starting to evaluate \"" << trm
            << "\" with mask (" << msk.cnt() << " out of "
            << msk.size() << ")";
        timer.start();
    }

    long ierr = 0;
    ibis::part::barrel vlist(this);
    vlist.recordVariable(&trm);
    if (vlist.size() == 0) { // a constant expression
        const double val = trm.eval();
        if (val != 0) {
            res.copy(msk);
            if (msk.size() < nEvents)
                res.adjustSize(msk.size(), nEvents);
            ierr = msk.cnt();
        }
        else {
            res.set(0, nEvents);
            ierr = 0;
        }
        return ierr;
    }

    // open all necessary files
    vlist.open();
    // feed the values into vlist and evaluate the arithmetic expression
    ibis::bitvector::indexSet idx = msk.firstIndexSet();
    const ibis::bitvector::word_t *iix = idx.indices();
    while (idx.nIndices() > 0) {
        if (idx.isRange()) {
            // move the file pointers of open files
            vlist.seek(*iix);
            for (uint32_t j = 0; j < idx.nIndices(); ++j) {
                vlist.read();
                if (trm.eval() != 0)
                    res.setBit(*iix + j, 1);
            } // for (uint32_t j = 0; j < idx.nIndices(); ++j)
        }
        else {
            for (uint32_t j = 0; j < idx.nIndices(); ++j) {
                vlist.seek(iix[j]);
                vlist.read();
                if (trm.eval() != 0)
                    res.setBit(iix[j], 1);
            } // for (uint32_t j = 0; j < idx.nIndices(); ++j)
        }

        ++ idx;
    } // while (idx.nIndices() > 0)

    if (ierr >= 0) {
        if (ibis::gVerbose > 3) {
            timer.stop();
            ierr = res.cnt();
            ibis::util::logger lg;
            lg() << "part[" << (m_name ? m_name : "?")
                 << "]::doScan -- evaluating " << trm << " on "
                 << msk.cnt() << " records (total: " << nEvents
                 << ") took " << timer.realTime()
                 << " sec elapsed time and produced " << ierr
                 << " hit" << (ierr > 1 ? "s" : "");
        }
        else {
            ierr = res.sloppyCount();
        }
    }
    return ierr;
} // ibis::part::doScan

long ibis::part::matchAny(const ibis::qAnyAny &cmp,
                          ibis::bitvector &hits) const {
    if (cmp.getPrefix() == 0 || cmp.getValues().empty()) return -1;
    if (nEvents == 0) return 0;
    ibis::bitvector mask;
    mask.set(1, nEvents);
    return matchAny(cmp, hits, mask);
} // ibis::part::matchAny

/// Perform exact match operation for an AnyAny query.  The bulk of the
/// work is performed as range queries.
long ibis::part::matchAny(const ibis::qAnyAny &cmp,
                          const ibis::bitvector &mask,
                          ibis::bitvector &hits) const {
    if (cmp.getPrefix() == 0 || cmp.getValues().empty()) return -1;
    if (nEvents == 0) return 0;

    long ierr = 0;
    hits.set(0, mask.size());
    const char* pref = cmp.getPrefix();
    const int len = std::strlen(pref);
    ibis::array_t<double> vals(cmp.getValues());
    columnList::const_iterator it = columns.lower_bound(pref);
    if (vals.size() > 1) { // more than one value
        while (it != columns.end() &&
               0 == strnicmp((*it).first, pref, len)) {
            // the column name has the specified prefix
            ibis::bitvector msk, res; // the mask for this particular column
            (*it).second->getNullMask(msk);
            msk &= mask;
            ibis::qDiscreteRange ex((*it).first, vals);
            if (hits.cnt() > hits.bytes()) {
                msk -= hits;
                msk.compress();
            }
            ierr = doScan(ex, msk, res);
            if (res.size() == hits.size())
                hits |= res;
            ++ it;
        }
    }
    else { // a single value to match
        while (it != columns.end() &&
               0 == strnicmp((*it).first, pref, len)) {
            // the column name has the specified prefix
            ibis::bitvector msk, res; // the mask for this particular column
            (*it).second->getNullMask(msk);
            msk &= mask;
            ibis::qContinuousRange ex((*it).first, ibis::qExpr::OP_EQ,
                                      vals.back());
            // hits.bytes() -- represent work of operator-=
            // hits.cnt() -- additional elements scanned without this
            // operation
            // should be worth it in most case!
            //if (hits.cnt() > hits.bytes())
            msk -= hits;
            ierr = doScan(ex, msk, res);
            if (res.size() == hits.size())
                hits |= res;
            ++ it;
        }
    }
    if (ierr >= 0)
        ierr = hits.sloppyCount();
    return ierr;
} // ibis::part::matchAny

/// Compute the min and max for each column.  Actually compute the min and
/// max of each attribute and write out a new metadata file for the data
/// partition.
void ibis::part::computeMinMax() {
    for (columnList::iterator it=columns.begin(); it!=columns.end(); ++it) {
        (*it).second->computeMinMax();
    }

    if (activeDir == 0) return;
    // limit the scope of lock to write operation only
    writeLock lock(this, "computeMinMax");
    writeMetaData(nEvents, columns, activeDir);

    Stat_T tmp;
    if (backupDir != 0 && *backupDir != 0) {
        if (UnixStat(backupDir, &tmp) == 0) {
            if ((tmp.st_mode&S_IFDIR) == S_IFDIR)
                writeMetaData(nEvents, columns, backupDir);
        }
    }
} // ibis::part::computeMinMax

/// Build a sorted version of the specified column.  Will sort the
/// base data of the named column if needed.
void ibis::part::buildSorted(const char* cname) const {
    readLock lock(this, "buildSorted");
    if (cname == 0 || *cname == 0) return;

    std::string evt = "part[";
    evt += m_name;
    evt += "]::buildSorted(";
    evt += cname;
    evt += ')';
    const ibis::column *col = getColumn(cname);
    if (col == 0) {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- " << evt
            << " could not find the named column in the data partition";
        return;
    }

    ibis::util::timer mytime(evt.c_str(), 3);
    ibis::roster(col, activeDir);  // don't need a variable name
} // ibis::part::buildSorted

/// Make sure indexes for all columns are available.
/// May use @c nthr threads to build indexes.  The argument iopt is used to
/// build new indexes if the corresponding columns do not already have
/// indexes.
/// @sa ibis::part::loadIndexes
int ibis::part::buildIndexes(const char* iopt, int nthr) {
    std::string evt = "part[";
    evt += m_name;
    evt += "]::buildIndexes";
    readLock lock(this, evt.c_str());
    ibis::horometer timer;
    timer.start();
    LOGGER(ibis::gVerbose > 5)
        << evt << " -- starting ...";
    if (nthr > 1) {
        -- nthr; // spawn one less thread than specified
        indexBuilderPool pool(*this, iopt);
        std::vector<pthread_t> tid(nthr);
        pthread_attr_t tattr;
        int ierr = pthread_attr_init(&tattr);
        if (ierr == 0) {
#if defined(PTHREAD_SCOPE_SYSTEM)
            ierr = pthread_attr_setscope(&tattr, PTHREAD_SCOPE_SYSTEM);
            if (ierr != 0
#if defined(ENOTSUP)
                && ierr != ENOTSUP
#endif
                ) {
                LOGGER(ibis::gVerbose > 1)
                    << "Warning -- " << evt << " pthread_attr_setscope failed "
                    "to set system scope (ierr = " << ierr << ')';
            }
#endif
            for (long i = 0; i < nthr; ++i) {
                ierr = pthread_create
                    (&(tid[i]), &tattr, ibis_part_build_indexes, (void*)&pool);
                if (0 != ierr) {
                    LOGGER(ibis::gVerbose > 0)
                        << "Warning -- " << evt
                        << " could not start thread # " << i
                        << " to run ibis_part_build_index ("
                        << strerror(ierr) << ')';
                }
                else {
                    LOGGER(ibis::gVerbose > 1)
                        << evt << " successfully started thread # " << i
                        << " to run ibis_part_build_index";
                }
            }
        }
        else {
            LOGGER(ibis::gVerbose > 2)
                << evt << " -- pthread_attr_init completed with " << ierr
                << ", using default attributes";
            for (long i = 0; i < nthr; ++i) {
                ierr = pthread_create
                    (&(tid[i]), 0, ibis_part_build_indexes, (void*)&pool);
                if (0 != ierr) {
                    LOGGER(ibis::gVerbose > 0)
                        << "Warning -- " << evt
                        << " could not start thread # " << i
                        << " to run ibis_part_build_index ("
                        << strerror(ierr) << ')';
                }
                else {
                    LOGGER(ibis::gVerbose > 1)
                        << evt << " successfully started thread # " << i
                        << " to run ibis_part_build_index";
                }
            }
        }
        (void) ibis_part_build_indexes((void*)&pool);
        for (int i = 0; i < nthr; ++ i) {
            void *j;
            pthread_join(tid[i], &j);
            LOGGER(j != 0 && ibis::gVerbose > 0)
                << "Warning -- part[" << name()
                << "]::buildIndexes -- thread # "
                << i << " returned a nonzero code " << j;
        }
        ++ nthr; // restore the original value
    }
    else { // do not spawn any new threads
        indexBuilderPool pool(*this, iopt);
        (void) ibis_part_build_indexes((void*)&pool);
        nthr = 1; // used only this thread
    }
    if (ibis::gVerbose > 0) {
        timer.stop();
        ibis::util::logger lg;
        lg() << evt << " processed "
             << nColumns() << " column" << (nColumns()>1 ? "s" : "")
             << " using " << nthr << " thread" << (nthr > 1 ? "s" : "")
             << " took " << timer.CPUTime() << " CPU seconds and "
             << timer.realTime() << " elapsed seconds";
    }

    if (activeDir == 0 || *activeDir == 0) return 0;
    writeMetaData(nEvents, columns, activeDir);
    if (backupDir != 0 && *backupDir != 0) {
        Stat_T tmp;
        if (UnixStat(backupDir, &tmp) == 0) {
            if ((tmp.st_mode&S_IFDIR) == S_IFDIR)
                writeMetaData(nEvents, columns, backupDir);
        }
    }
    return 0;
} // ibis::part::buildIndexes

/// Make sure indexes for all columns are available.
///
/// The sequence of strings are used as follows.
///
/// - The strings are taken two at a time where the first of the pair is
///   taken as the pattern to be used to match the name of the columns.  If
///   a column name matches the pattern, the second of the pair is used as
///   the indexing option for the column.
/// - The odd entry at the end of the sequence is used as the indexing
///   option for any column that does not match any of the preceeding
///   pairs.
/// - If the sequence has an even number of entries, the column whose name
///   does not match any name patterns will be indexed with the default
///   indexing option.
///
/// Here is an example sequence ("a%", "<binning none/>", "_b*" "<binning
/// precision=2>", "bit-slice").  A column name that matches "a%", i.e.,
/// starting columns starting with 'a', will be indexed with option
/// "<binning none/>".  All column names do not match the pattern "a%" will
/// then be compared with "_b*".  If they match, then the column will be
/// indexed with option "<binning precision=2>".  All other columns will be
/// indexed with option "bit-slice".
///
/// May use @c nthr threads to build indexes.  The argument iopt is used to
/// build new indexes if the corresponding columns do not already have
/// indexes.
///
/// @sa ibis::part::loadIndexes
int ibis::part::buildIndexes(const ibis::table::stringArray &iopt, int nthr) {
    std::string evt = "part[";
    evt += m_name;
    evt += "]::buildIndexes";
    readLock lock(this, evt.c_str());
    ibis::horometer timer;
    timer.start();
    LOGGER(ibis::gVerbose > 5)
        << evt << " -- starting ...";
    if (nthr > 1) {
        -- nthr; // spawn one less thread than specified
        indexBuilderPool pool(*this, iopt);
        std::vector<pthread_t> tid(nthr);
        pthread_attr_t tattr;
        int ierr = pthread_attr_init(&tattr);
        if (ierr == 0) {
#if defined(PTHREAD_SCOPE_SYSTEM)
            ierr = pthread_attr_setscope(&tattr, PTHREAD_SCOPE_SYSTEM);
            if (ierr != 0
#if defined(ENOTSUP)
                && ierr != ENOTSUP
#endif
                ) {
                LOGGER(ibis::gVerbose > 1)
                    << "Warning -- " << evt << " pthread_attr_setscope failed "
                    "to set system scope (ierr = " << ierr << ')';
            }
#endif
            for (long i = 0; i < nthr; ++i) {
                ierr = pthread_create
                    (&(tid[i]), &tattr, ibis_part_build_indexes, (void*)&pool);
                if (0 != ierr) {
                    LOGGER(ibis::gVerbose > 0)
                        << "Warning -- " << evt << " could not start thread # "
                        << i << " to run ibis_part_build_index ("
                        << strerror(ierr) << ')';
                }
                else {
                    LOGGER(ibis::gVerbose > 1)
                        << evt << " successfully started thread # " << i
                        << " to run ibis_part_build_index";
                }
            }
        }
        else {
            LOGGER(ibis::gVerbose > 2)
                << evt << " -- pthread_attr_init failed with " << ierr
                << ", using default attributes";
            for (long i = 0; i < nthr; ++i) {
                ierr = pthread_create
                    (&(tid[i]), 0, ibis_part_build_indexes, (void*)&pool);
                if (0 != ierr) {
                    LOGGER(ibis::gVerbose > 0)
                        << "Warning -- " << evt << " could not start thread # "
                        << i << " to run ibis_part_build_index ("
                        << strerror(ierr) << ')';
                }
                else {
                    LOGGER(ibis::gVerbose > 1)
                        << evt << " successfully started thread # " << i
                        << " to run ibis_part_build_index";
                }
            }
        }
        (void) ibis_part_build_indexes((void*)&pool);
        for (int i = 0; i < nthr; ++ i) {
            void *j;
            pthread_join(tid[i], &j);
            LOGGER(j != 0 && ibis::gVerbose > 0)
                << "Warning -- " << evt << " -- thread # "
                << i << " returned a nonzero code " << j;
        }
        ++ nthr; // restore the original value
    }
    else { // do not spawn any new threads
        indexBuilderPool pool(*this, iopt);
        (void) ibis_part_build_indexes((void*)&pool);
        nthr = 1; // used only this thread
    }
    if (ibis::gVerbose > 0) {
        timer.stop();
        ibis::util::logger lg;
        lg() << evt << " processed "
             << nColumns() << " column" << (nColumns()>1 ? "s" : "")
             << " using " << nthr << " thread" << (nthr > 1 ? "s" : "")
             << " took " << timer.CPUTime() << " CPU seconds and "
             << timer.realTime() << " elapsed seconds";
    }

    if (activeDir == 0 || *activeDir == 0) return 0;
    writeMetaData(nEvents, columns, activeDir);
    if (backupDir != 0 && *backupDir != 0) {
        Stat_T tmp;
        if (UnixStat(backupDir, &tmp) == 0) {
            if ((tmp.st_mode&S_IFDIR) == S_IFDIR)
                writeMetaData(nEvents, columns, backupDir);
        }
    }
    return 0;
} // ibis::part::buildIndexes

/// Load indexes of all columns.  This function iterates through all
/// columns and load the index associated with each one of them by
/// call ibis::column::loadIndex.  If an index for a column does not
/// exist, the index is built in memory and written to disk.  The
/// argument iopt is used as the index specification if a new index is
/// to be built.  If iopt is nil, the index specifications for the
/// individual columns or the data partition are used.  The
/// argument ropt is passed to ibis::index::create to regenerate an index
/// object from the index file.  The default value of ropt is 0.
/// @sa function ibis::index::create
void ibis::part::loadIndexes(const char* iopt, int ropt) const {
    if (activeDir == 0) return;

    for (columnList::const_iterator it=columns.begin(); it!=columns.end();
         ++it) {
        (*it).second->loadIndex(iopt, ropt);
    }
    std::string evt = "part[";
    evt += m_name;
    evt += "]::loadIndexes";
    LOGGER(ibis::gVerbose > 6)
        << evt << " loaded all indexes of this data partition";

    const char* expf = ibis::gParameters()["exportBitmapAsCsr"];
    if (expf != 0 && *expf != 0) {
        // write out the bitmap index in CSR format
        std::vector< ibis::index* > idx;
        columnList::const_iterator it;
        array_t<uint32_t> cnt;
        uint32_t i, j, tot = 0;
        cnt.reserve(nColumns()*12);
        idx.reserve(nColumns());
        for (i = 0, it = columns.begin(); i < nColumns(); ++i, ++it) {
            idx.push_back(ibis::index::create((*it).second, activeDir));
            for (j = 0; j < idx[i]->numBitvectors(); ++j) {
                const ibis::bitvector* tmp = idx[i]->getBitvector(j);
                if (tmp) {
                    const uint32_t ct = tmp->cnt();
                    if (ct > 0) {
                        cnt.push_back(ct);
                        tot += ct;
                    }
                }
            }
        }

        LOGGER(ibis::gVerbose > 1)
            << evt << " attempt to write " << cnt.size() << " bitmap(s) ("
            << tot << ") to " << expf;
        FILE* fptr = fopen(expf, "w"); // write in ASCII text
        if (fptr) { // ready to write out the data
            fprintf(fptr, "%lu %lu %lu\n0\n",
                    static_cast<long unsigned>(nRows()),
                    static_cast<long unsigned>(cnt.size()),
                    static_cast<long unsigned>(tot));
            tot = 0;
            for (i = 0; i < cnt.size(); ++i) {
                tot += cnt[i];
                fprintf(fptr, "%lu\n", static_cast<long unsigned>(tot));
            }
            ibis::bitvector::indexSet is;
            uint32_t nis=0;
            const ibis::bitvector::word_t *iis=is.indices();
            for (i = 0; i < idx.size(); ++i) {
                for (j = 0; j < idx[i]->numBitvectors(); ++j) {
                    const ibis::bitvector* tmp = idx[i]->getBitvector(j);
                    if (tmp == 0) continue;
                    is = tmp->firstIndexSet();
                    nis = is.nIndices();
                    while (nis) {
                        if (is.isRange()) {
                            for (uint32_t k = iis[0]; k < iis[1]; ++k)
                                fprintf(fptr, "%lu\n",
                                        static_cast<long unsigned>(k));
                        }
                        else {
                            for (uint32_t k = 0; k < nis; ++k)
                                fprintf(fptr, "%lu\n",
                                        static_cast<long unsigned>(iis[k]));
                        }
                        ++is;
                        nis = is.nIndices();
                    } // while (nis)
                } // for (j = 0;
            } // for (i = 0;
            fclose(fptr);
        }
        else {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << evt << "could not open file \"" << expf
                << "\" to write the bitmaps ... "
                << (errno ? strerror(errno) : "no free stdio stream");
        }
        for (i = 0; i < idx.size(); ++i)
            delete idx[i];
    }
} // ibis::part::loadIndexes

/// Unload indexes of all columns.
void ibis::part::unloadIndexes() const {
    for (columnList::const_iterator it=columns.begin(); it!=columns.end();
         ++it) {
        (*it).second->unloadIndex();
    }
    LOGGER(ibis::gVerbose > 6)
        << "part[" << name() << "]::unloadIndexes completed successfully";
} // ibis::part::unloadIndexes

/// Remove existing index files!
/// The indexes will be rebuilt next time they are needed.  This
/// function is useful after changing the index specification before
/// rebuilding a set of new indices.
void ibis::part::purgeIndexFiles() const {
    readLock lock(this, "purgeIndexFiles");
    for (columnList::const_iterator it = columns.begin();
         it != columns.end();
         ++ it) {
        (*it).second->unloadIndex();
        (*it).second->purgeIndexFile();
    }
} // ibis::part::purgeIndexFiles

void ibis::part::indexSpec(const char *spec) {
    writeLock lock(this, "indexSpec");
    delete [] idxstr;
    idxstr = ibis::util::strnewdup(spec);
    if (activeDir != 0)
        writeMetaData(nEvents, columns, activeDir);
    if (backupDir != 0)
        writeMetaData(nEvents, columns, backupDir);
} // ibis::part::indexSpec

/// Retrieve the current state of data partition.  It holds a read lock to
/// ensure the sanity of the state.   The function getStateNoLocking may
/// return a transient value unless the caller hold a lock.
ibis::part::TABLE_STATE ibis::part::getState() const {
    readLock lock(this, "getState");
    return state;
} // ibis::part::getState

/// Given a name, return the associated column.  Return nil pointer if
/// the name is not found.  If the name contains a period, it skips the
/// characters up to the first period.
ibis::column* ibis::part::getColumn(const char* prop) const {
    ibis::column *ret = 0;
    if (prop == 0 || *prop == 0 || *prop == '*')
        return ret;

    const char *str = strchr(prop, '.');
    columnList::const_iterator it = columns.end();
    if (str != 0) {
        ++ str; // skip '.'
        it = columns.find(str);

        if (it == columns.end()) {// try the whole name
            it = columns.find(prop);
        }
    }
    else {
        it = columns.find(prop);
    }

    if (it == columns.end()) {
        const size_t nch = std::strlen(m_name);
        if (std::strlen(prop) > nch+1 && prop[nch] == '_' &&
            strnicmp(prop, m_name, nch) == 0) {
            str = prop + nch + 1;
            it = columns.find(str);
        }
    }

    if (it == columns.end()) {
        std::string nm = prop;
        // normalize the name and try again
        if (isalpha(nm[0]) == 0 && nm[0] != '_')
            nm[0] = 'A' + (nm[0] % 26);
        for (unsigned j = 1; j < nm.size(); ++ j)
            if (isalnum(nm[j]) == 0)
                nm[j] = '_';
        it = columns.find(nm.c_str());
    }

    if (it != columns.end()) {
        ret = (*it).second;
    }
    else if (*prop == '_') {
        for (++ prop; *prop == '_'; ++ prop); // skip leading '_'
        if (std::isxdigit(*prop) != 0) { // hexdecimal digits
            unsigned ind = 0;
            for (; std::isxdigit(*prop); ++ prop) {
                if (std::isdigit(*prop)) {
                    ind = ind * 16 + (*prop - '0');
                }
                else if (*prop >= 'A' && *prop <= 'F') {
                    ind = ind * 16 + (10 + *prop - 'A');
                }
                else {
                    ind = ind * 16 + (10 + *prop - 'a');
                }
            }
            if (ind < colorder.size()) {
                return const_cast<ibis::column*>(colorder[ind]);
            }
            else if (ind < columns.size()) {
                for(it = columns.begin(); ind > 0; ++ it, -- ind);
                return const_cast<ibis::column*>(it->second);
            }
        }
    }
    return ret;
} // ibis::part::getColumn

/// Skip pass all the dots in the given string.  The pointer returned from
/// this function points to the first character after the last dot (.).  If
/// the incoming string ends with a dot, the return value would be null
/// terminator.  If there is no dot at all, the return value is the same
/// as the input value.
const char* ibis::part::skipPrefix(const char* name) {
    const char* ptr = name;
    while (*ptr != 0) {
        while (*ptr != 0 && *ptr != '.') ++ ptr;
        if (*ptr == '.') {
            ++ ptr;
            name = ptr;
        }
    }
    return name;
} // ibis::part::skipPrefix

/// Perform predefined set of tests and return the number of failures.
long ibis::part::selfTest(int nth, const char* pref) const {
    long nerr = 0;
    if (activeDir == 0) return nerr;

    ibis::horometer timer;
    try {
        readLock lock(this, "selfTest"); // need a read lock on the part
        if (ibis::gVerbose > 1) {
            logMessage("selfTest", "start testing data in %s with option "
                       "%d ... ", activeDir, nth);
            timer.start();
        }
        columnList::const_iterator it;

        if (columns.size() == 0 || nEvents == 0) {
            logMessage("selfTest", "empty ibis::part in %s", activeDir);
            return nerr;
        }

        // test the file sizes based on column::elementSize
        uint32_t sz = 0;
        for (it = columns.begin(); it != columns.end(); ++it) {
            int elm = (*it).second->elementSize();
            if (elm > 0) { // the column has fixed element size
                std::string sname;
                const char* fname = (*it).second->dataFileName(sname);
                uint32_t fsize = ibis::util::getFileSize(fname);
                sz = fsize / elm;
                if (sz != nEvents) {
                    ++ nerr;
                    logWarning("selfTest", "column %s has %lu records, "
                               "%lu expected", (*it).first,
                               static_cast<long unsigned>(sz),
                               static_cast<long unsigned>(nEvents));
                }
                else if (ibis::gVerbose > 4) {
                    logMessage("selfTest", "column %s has %lu records as "
                               "expected.", (*it).first,
                               static_cast<long unsigned>(nEvents));
                }

                // pick one random column to try all possible range
                // operators
                if (ibis::util::rand()*columns.size() < 1.0)
                    testRangeOperators((*it).second, &nerr);
            }
            else if (elm < 0) { // a bad element size
                ++ nerr;
                logWarning("selfTest", "column %s [tyoe %d] has an "
                           "unsupported type (element size = %d)",
                           (*it).first,
                           static_cast<int>((*it).second->type()),
                           elm);
            }

            std::string tmp;
            if (pref) {
                tmp = pref;
                tmp += ".testIndexSpeed";
            }
            else {
                tmp = m_name;
                tmp = ".testIndexSpeed";
            }
            if (ibis::gParameters().isTrue(tmp.c_str())) {
                (*it).second->indexSpeedTest();
            }
        }
        if (nth <= 0 || nerr > 0)
            return nerr;

        bool longtest = false;
        {
            std::string ltest;
            if (pref) {
                ltest = pref;
                ltest += ".longTests";
            }
            else {
                ltest = name();
                ltest += ".longTests";
            }
            longtest = ibis::gParameters().isTrue(ltest.c_str());
        }
        if (nth > 1) {
            // select some attributes for further testing -- do part of the
            // work in new threads
            -- nth;
            if (nth > 100)
                nth = 100;
            std::vector<pthread_t> tid(nth);
            if (tid.empty()) {
                logWarning("selfTest", "could not allocate array "
                           "pthread_t[%d], will run only one set of tests",
                           nth);
                ++ nerr;
                quickTest(pref, &nerr);
                return nerr;
            }

            thrArg arg;
            arg.et = this;
            arg.pref = pref;
            arg.nerrors = &nerr;
            pthread_attr_t tattr;
            int ierr = pthread_attr_init(&tattr);
            const bool myattr = (ierr == 0);

            if (ibis::gVerbose > 1)
                logMessage("selfTest", "parallel tests with %d thread%s",
                           (int)(nth+1), (nth>0?"s":""));
            // spawn threads to invoke ibis_part_threadedTestFun1
            if (myattr) {
#if defined(PTHREAD_SCOPE_SYSTEM)
                ierr = pthread_attr_setscope(&tattr, PTHREAD_SCOPE_SYSTEM);
                if (ierr != 0
#if defined(ENOTSUP)
                    && ierr != ENOTSUP
#endif
                    ) {
                    logMessage("selfTest", "pthread_attr_setscope is unable "
                               "to set system scope (ierr = %d)", ierr);
                }
#endif
                for (int i = 0; i < nth; ++i) {
                    ierr = pthread_create
                        (&(tid[i]), &tattr, ibis_part_threadedTestFun1,
                         (void*)&arg);
                    if (0 != ierr) {
                        logWarning("selfTest", "could not start thread # "
                                   "%d to run ibis_part_threadedTestFun1 (%s)",
                                   i, strerror(ierr));
                    }
                    else if (ibis::gVerbose > 2) {
                        logMessage("selfTest", "started thread # %d to run "
                                   "ibis_part_threadedTestFun1", i);
                    }
                }
            }
            else {
                logWarning("selfTest", "pthread_attr_init failed with %d, "
                           "using default attributes", ierr);
                for (int i = 0; i < nth; ++i) {
                    ierr = pthread_create
                        (&(tid[i]), 0, ibis_part_threadedTestFun1, (void*)&arg);
                    if (0 != ierr) {
                        logWarning("selfTest", "could not start the thread # "
                                   "%d to run ibis_part_threadedTestFun1 (%s)",
                                   i, strerror(ierr));
                    }
                    else if (ibis::gVerbose > 2) {
                        logMessage("selfTest", "started thread # %d to run "
                                   "ibis_part_threadedTestFun1", i);
                    }
                }
            }

            // handle part of work using this thread
            if (nEvents < 1048576 || longtest)
                queryTest(pref, &nerr);
            else
                quickTest(pref, &nerr);

            for (int i = 0; i < nth; ++i) { // wait for the other threads
                void* j;
                pthread_join(tid[i], &j);
                LOGGER(j != 0 && ibis::gVerbose > 0)
                    << "Warning -- part[" << name() << "]::selfTest thread # "
                    << i << " returned a nonzero code " << j;
            }

            if (nerr == 0 && columns.size() > 1) {
                // spawn threads to invoke ibis_part_threadedTestFun2
                const unsigned nc =
                    (columns.size() > 2 ?
                     columns.size() - (columns.size() >> 1) : columns.size());
                unsigned nq = (63 & ibis::util::serialNumber()) +
                    7 * ibis::gVerbose;
                nq *= (nth + 1);
                if (nEvents >= 104857600)
                    nq >>= 1; // reduce number of queries for large partition
                else if (nEvents <= 1048576)
                    nq <<= 1; // increase number of queries for small partition
                buildQueryList(arg, nc, nq);
                for (int i = 0; i < nth; ++ i) {
                    ierr = pthread_create(&(tid[i]), &tattr,
                                          ibis_part_threadedTestFun2,
                                          (void*)&arg);
                    if (0 != ierr) {
                        logWarning("selfTest", "could not start the thread # "
                                   "%d to run ibis_part_threadedTestFun2 (%s)",
                                   i, strerror(ierr));
                    }
                    else if (ibis::gVerbose > 2) {
                        logMessage("selfTest", "started thread # %d to run "
                                   "ibis_part_threadedTestFun2", i);
                    }
                }

                void *j;
                j = ibis_part_threadedTestFun2((void*)&arg);
                if (j != 0) {
                    ++ nerr;
                    LOGGER(ibis::gVerbose > 0)
                        << "Warning -- part[" << name()
                        << "]::selfTest ibis_part_threadedTestFun2 returned "
                        << j << " instead of 0";
                }
                for (int i = 0; i < nth; ++ i) {
                    pthread_join(tid[i], &j);
                    LOGGER(j != 0 && ibis::gVerbose > 0)
                        << "Warning -- part[" << name()
                        << "]::selfTest thread # " << i
                        << " returned a nonzero code " << j;
                }
                checkQueryList(arg);
            }

            if (myattr)
                ierr = pthread_attr_destroy(&tattr);
        }
        else { // try some queries using this thread
            if (nEvents < 1048576 || longtest)
                queryTest(pref, &nerr);
            else
                quickTest(pref, &nerr);
        }
    }
    catch (const std::exception &e) {
        ibis::util::emptyCache();
        ibis::util::logMessage("Warning", "part::selfTest() received "
                               "the following std::exception\n%s",
                               e.what());
        ++ nerr;
    }
    catch (const char* s) {
        ibis::util::emptyCache();
        ibis::util::logMessage("Warning", "part::selfTest() received "
                               "the following string exception\n%s", s);
        ++ nerr;
    }
    catch (...) {
        ibis::util::emptyCache();
        ibis::util::logMessage("Warning", "part::selfTest() received "
                               "an unexpected exception");
        ++ nerr;
    }
    if (nerr) {
        logWarning("selfTest", "encountered %d error%s", nerr,
                   (nerr>1?"s":""));
    }
    else if (ibis::gVerbose > 1) {
        timer.stop();
        logMessage("selfTest", "completed successfully using %g sec(CPU), "
                   "%g sec(elapsed)", timer.CPUTime(),
                   timer.realTime());
    }

    return nerr;
} // ibis::part::selfTest

/// Randomly select a column and perform a set of tests recursively.
void ibis::part::queryTest(const char* pref, long* nerrors) const {
    if (columns.empty() || nEvents == 0) return;

    // select a colum to perform the test on
    int i = (static_cast<int>(ibis::util::rand() * columns.size()) +
             ibis::util::serialNumber()) % columns.size();
    columnList::const_iterator it = columns.begin();
    while (i > 0) {++it; --i;}
    for (i = 0; static_cast<unsigned>(i) < columns.size() &&
             ((*it).second->type() == ibis::TEXT ||
              (*it).second->type() == ibis::CATEGORY); ++ i) {
        // skip over string attributes
        ++ it;
        if (it == columns.end()) // wrap around
            it = columns.begin();
    }
    if (static_cast<unsigned>(i) >= columns.size()) {
        LOGGER(ibis::gVerbose > 2)
            << "Warning -- part[" << name()
            << "]::queryTest needs a non-string attribute to proceed";
        return;
    }

    // select the range to test
    double lower = (*it).second->lowerBound();
    double upper = (*it).second->upperBound();
    if (! (lower < upper)) {
        // actually compute the minimum and maximum values
        (*it).second->computeMinMax();
        lower = (*it).second->lowerBound();
        upper = (*it).second->upperBound();
    }
    if (! (lower < upper)) {
        if (lower < DBL_MAX && lower > -DBL_MAX)
            upper = ibis::util::compactValue(lower, DBL_MAX);
        if (upper < DBL_MAX && upper > -DBL_MAX)
            lower = ibis::util::compactValue(-DBL_MAX, upper);

        if (! (lower < upper)) { // force min/max to be 0/1
            lower = 0.0;
            upper = 1.0;
        }
    }

    std::string random;
    if (pref) {
        random = pref;
        random += ".randomTests";
    }
    else {
        random = "randomTests";
    }
    if (ibis::gParameters().isTrue(random.c_str())) {
        unsigned tmp1 = time(0);
        unsigned tmp2 = rand();
        double range = (*it).second->upperBound()
            - (*it).second->lowerBound();
        lower = range * ((tmp1 % 1024) / 1024.0);
        upper = range *((tmp2 % 1024) / 1024.0);
        if (fabs(lower - upper)*256 < range) {
            lower = (*it).second->lowerBound();
            upper = (*it).second->upperBound();
        }
        else if (lower < upper) {
            lower += (*it).second->lowerBound();
            upper += (*it).second->lowerBound();
        }
        else {
            range = lower;
            lower = upper + (*it).second->lowerBound();
            upper = range + (*it).second->lowerBound();
        }
        ibis::TYPE_T type = (*it).second->type();
        if (type != ibis::FLOAT && type != ibis::DOUBLE) {
            // remove the fractional part for consistent printing
            lower = floor(lower);
            upper = ceil(upper);
        }
    }

    ibis::horometer timer;
    if (ibis::gVerbose > 2)
        timer.start();
    // do the test
    recursiveQuery(pref, (*it).second, lower, upper, nerrors);
    if (ibis::gVerbose > 2) {
        timer.stop();
        logMessage("queryTest", "tests on %s took %g sec(CPU), %g "
                   "sec(elapsed)", (*it).first, timer.CPUTime(),
                   timer.realTime());
    }
} // ibis::part::queryTest

/// Randomly select a column from the current list and perform a dozen
/// tests on the column.
void ibis::part::quickTest(const char* pref, long* nerrors) const {
    if (columns.empty() || nEvents == 0) return;

    char clause[MAX_LINE];
    // select a colum to perform the test on
    columnList::const_iterator it = columns.begin();
    {
        int i = (static_cast<int>(ibis::util::rand() * columns.size()) +
                 ibis::util::serialNumber()) % columns.size();
        while (i) {++it; --i;};
        for (i = 0; static_cast<unsigned>(i) < columns.size() &&
                 ((*it).second->type() == ibis::TEXT ||
                  (*it).second->type() == ibis::CATEGORY); ++ i) {
            // skip over string attributes
            ++ it;
            if (it == columns.end()) // wrap around
                it = columns.begin();
        }
        if (static_cast<unsigned>(i) >= columns.size()) {
            LOGGER(ibis::gVerbose > 2)
                << "Warning -- part[" << name()
                << "]::quickTest needs a non-string attribute to proceed";
            return;
        }
    }
    ibis::column* att = (*it).second;
    if (it != columns.begin()) {
        -- it;
        sprintf(clause, "%s, %s", (*it).first, att->name());
    }
    else if (columns.size() > 1) {
        it = columns.end();
        -- it;
        sprintf(clause, "%s, %s", (*it).first, att->name());
    }
    else {
        strcpy(clause, att->name());
    }

    // select the range to test
    double lower = att->lowerBound();
    double upper = att->upperBound();
    if (! (lower < upper)) {
        // actually compute the minimum and maximum values
        att->computeMinMax();
        lower = att->lowerBound();
        upper = att->upperBound();
    }
    if (! (lower < upper)) {
        if (lower < DBL_MAX && lower > -DBL_MAX)
            upper = ibis::util::compactValue(lower, DBL_MAX);
        if (upper < DBL_MAX && upper > -DBL_MAX)
            lower = ibis::util::compactValue(-DBL_MAX, upper);

        if (! (lower < upper)) { // force min/max to 0/1
            lower = 0.0;
            upper = 1.0;
        }
    }

    std::string random;
    if (pref) {
        random = pref;
        random += ".randomTests";
    }
    else {
        random = "randomTests";
    }
    if (ibis::gParameters().isTrue(random.c_str())) {
        unsigned tmp1 = time(0);
        unsigned tmp2 = rand();
        double range = upper - lower;
        lower += range * ((tmp1 % 1024) / 1024.0);
        upper -= range *((tmp2 % 1024) / 1024.0);
        if (fabs(lower - upper)*512 < range) {
            // very small difference, use the whole range
            lower -= range * ((tmp1 % 1024) / 1024.0);
            upper += range *((tmp2 % 1024) / 1024.0);
        }
        else if (lower > upper) { // lower bound is smaller
            range = lower;
            lower = upper;
            upper = range;
        }
        ibis::TYPE_T type = att->type();
        if (type != ibis::FLOAT && type == ibis::DOUBLE) {
            // remove the fractional part for consistent printing
            lower = floor(lower);
            upper = ceil(upper);
        }
    }

    ibis::horometer timer;
    if (ibis::gVerbose > 2)
        timer.start();
    // do the test
    const char* str=0;
    uint32_t total=0;
    query qtmp("[:]", this, pref);
    qtmp.setSelectClause(clause);

    sprintf(clause, "%s < %g", att->name(), lower);
    qtmp.setWhereClause(clause);
    long ierr = qtmp.evaluate();
    if (ierr >= 0)
        total = qtmp.getNumHits();
    else
        ++ (*nerrors);
    str = qtmp.getLastError();
    if (str != 0 && *str != static_cast<char>(0)) {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- part::quickTest last error on query "
            << "\"" << clause << "\" is \n" << str;
        qtmp.clearErrorMessage();
        ++(*nerrors);
    }

    sprintf(clause, "%s >= %g", att->name(), upper);
    qtmp.setWhereClause(clause);
    ierr = qtmp.evaluate();
    if (ierr < 0) { // unload all indexes, try once more
        ibis::util::mutexLock lock(&mutex, "part::quickTest");
        unloadIndexes();
        ierr = qtmp.evaluate();
    }
    if (ierr >= 0) {
        total += qtmp.getNumHits();
        if (ibis::gVerbose > 2) { // use sequential scan to verify counts
            ibis::bitvector tmp;
            ierr = qtmp.sequentialScan(tmp);
            if (ierr >= 0) {
                tmp ^= *(qtmp.getHitVector());
                if (tmp.cnt() > 0) {
                    ++ (*nerrors);
                    LOGGER(ibis::gVerbose >= 0)
                        << "Warning -- the sequential scan for "
                        << clause << " produced "
                        << tmp.cnt() << " different result"
                        << (tmp.cnt() > 1 ? "s" : "");
                }
            }
            else {
                ++ (*nerrors);
            }
        }
    }
    else {
        ++ (*nerrors);
    }
    str = qtmp.getLastError();
    if (str != 0 && *str != static_cast<char>(0)) {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- part::quickTest last error on query "
            << "\"" << clause << "\" is \n" << str;
        qtmp.clearErrorMessage();
        ++(*nerrors);
    }

    double tgt = lower + 0.01 * (upper - lower);
    double b1 = 0.5*(lower + upper);
    double b2 = upper;
    while (b1 > tgt) {
        sprintf(clause, "%g <= %s < %g", b1, att->name(), b2);
        qtmp.setWhereClause(clause);
        ierr = qtmp.evaluate();
        if (ierr < 0) { // unload all indexes, try once more
            ibis::util::mutexLock lock(&mutex, "part::quickTest");
            unloadIndexes();
            ierr = qtmp.evaluate();
        }
        if (ierr >= 0) {
            total += qtmp.getNumHits();
            if (ibis::gVerbose > 2) { // use sequential scan to verify counts
                ibis::bitvector tmp;
                ierr = qtmp.sequentialScan(tmp);
                if (ierr >= 0) {
                    tmp ^= *(qtmp.getHitVector());
                    if (tmp.cnt() > 0) {
                        ++ (*nerrors);
                        LOGGER(ibis::gVerbose >= 0)
                            << "Warning -- the sequential scan for "
                            << clause << " produced "
                            << tmp.cnt() << " different result"
                            << (tmp.cnt() > 1 ? "s" : "");
                    }
                }
                else {
                    ++ (*nerrors);
                }
            }
        }
        else {
            ++ (*nerrors);
        }
        str = qtmp.getLastError();
        if (str != 0 && *str != static_cast<char>(0)) {
            logWarning("quickTest",  "last error on query \"%s\" "
                       "is \n%s", clause, str);
            qtmp.clearErrorMessage();
            ++(*nerrors);
        }
        b2 = b1;
        b1 = ibis::util::compactValue(0.875*lower+0.125*b1, 0.5*(lower + b1));
    }

    sprintf(clause, "%g <= %s < %g", lower, att->name(), b2);
    qtmp.setWhereClause(clause);
    ierr = qtmp.evaluate();
    if (ierr < 0) { // unload all indexes, try once more
        ibis::util::mutexLock lock(&mutex, "part::quickTest");
        unloadIndexes();
        ierr = qtmp.evaluate();
    }
    if (ierr >= 0)
        total += qtmp.getNumHits();
    else
        ++ (*nerrors);
    str = qtmp.getLastError();
    if (str != 0 && *str != static_cast<char>(0)) {
        logWarning("quickTest",  "last error on query \"%s\" "
                   "is \n%s", clause, str);
        qtmp.clearErrorMessage();
        ++(*nerrors);
    }

    { // a block to limit the scope of mask
        ibis::bitvector mask;
        att->getNullMask(mask);
        if (total != mask.cnt()) {
            ++(*nerrors);
            logWarning("quickTest", "the total number of values for %s is "
                       "expected to be %lu but is actually %lu", att->name(),
                       static_cast<long unsigned>(mask.cnt()),
                       static_cast<long unsigned>(total));
        }
    }

    // a small identity expression to test the evaluation of arithmetic
    // expressions
    sprintf(clause, "%g <= tan(atan(0.5*(%s+%s))) < %g",
            lower, att->name(), att->name(), b2);
    qtmp.setWhereClause(clause);
    ierr = qtmp.evaluate();
    if (ierr < 0) { // unload all indexes, try once more
        ibis::util::mutexLock lock(&mutex, "part::quickTest");
        unloadIndexes();
        ierr = qtmp.evaluate();
    }
    if (ierr < 0)
        ++ (*nerrors);
    str = qtmp.getLastError();
    if (str != 0 && *str != static_cast<char>(0)) {
        logWarning("quickTest",  "last error on query \"%s\" "
                   "is \n%s", clause, str);
        qtmp.clearErrorMessage();
        ++(*nerrors);
    }

    { // use sequential scan to verify the hit list
        ibis::bitvector seqhits;
        ierr = qtmp.sequentialScan(seqhits);
        if (ierr < 0) {
            ++(*nerrors);
            logWarning("quickTest", "sequential scan on query \"%s\" failed",
                       clause);
        }
        else {
            seqhits ^= *(qtmp.getHitVector());
            if (seqhits.cnt() != 0) {
                ++(*nerrors);
                logWarning("quickTest", "sequential scan on query \"%s\" "
                           "produced %lu different hits", clause,
                           static_cast<long unsigned>(seqhits.cnt()));
                if (ibis::gVerbose > 2) {
                    uint32_t maxcnt = (ibis::gVerbose > 30 ? nRows()
                                       : (1U << ibis::gVerbose));
                    if (maxcnt > seqhits.cnt())
                        maxcnt = seqhits.cnt();
                    uint32_t cnt = 0;
                    ibis::bitvector::indexSet is = seqhits.firstIndexSet();
                    ibis::util::logger lg;
                    lg() << "the locations of the difference\n";
                    while (is.nIndices() && cnt < maxcnt) {
                        const ibis::bitvector::word_t *ii = is.indices();
                        if (is.isRange()) {
                            lg() << *ii << " -- " << ii[1];
                        }
                        else {
                            for (uint32_t i0=0; i0 < is.nIndices(); ++i0)
                                lg() << ii[i0] << " ";
                        }
                        cnt += is.nIndices();
                        lg() << "\n";
                        ++ is;
                    }
                    if (cnt < seqhits.cnt())
                        lg() << "... (" << seqhits.cnt() - cnt
                             << " rows skipped\n";
                }
            }
            else if (ibis::gVerbose > 3) {
                logMessage("quickTest",
                           "sequential scan produced the same hits");
            }
        }
    }
    // test RID query -- limit to no more than 2048 RIDs to save time
    ibis::RIDSet* rid1 = qtmp.getRIDs();
    if (rid1 == 0 || rid1->empty()) {
        delete rid1;
        if (ibis::gVerbose > 1) {
            timer.stop();
            logMessage("quickTest", "tests on %s took %g sec(CPU), %g "
                       "sec(elapsed)", att->name(), timer.CPUTime(),
                       timer.realTime());
        }
        return;
    }
#if DEBUG+0 > 0 || _DEBUG+0 > 0
    {
        ibis::util::logger lg(4);
        const char* str = qtmp.getWhereClause();
        lg() << "DEBUG -- query[" << qtmp.id() << ", "
             << (str?str:"??") << "]::getRIDs returned "
             << rid1->size() << "\n";
#if DEBUG > 2
        for (ibis::RIDSet::const_iterator it1 = rid1->begin();
             it1 != rid1->end(); ++it1)
            lg() << (*it1) << "\n";
#endif
    }
#endif
    if (rid1->size() > 2048)
        rid1->resize(1024+(1023 & rid1->size()));
    std::sort(rid1->begin(), rid1->end());
#if DEBUG+0 > 0 || _DEBUG+0 > 0
    {
        ibis::util::logger lg(4);
        lg() << "rid1 after query[" << qtmp.id()
             << "]::evaluate contains " << rid1->size() << "\n";
#if DEBUG > 2
        for (ibis::RIDSet::const_iterator it1 = rid1->begin();
             it1 != rid1->end(); ++it1)
            lg() << (*it1) << "\n";
#endif
    }
#endif
    ibis::RIDSet* rid2 = new ibis::RIDSet();
    rid2->deepCopy(*rid1); // setRIDs removes the underlying file for rid1
    delete rid1;
    rid1 = rid2;
    qtmp.setRIDs(*rid1);
    ierr = qtmp.evaluate();
    if (ierr < 0) { // unload all indexes, try once more
        ibis::util::mutexLock lock(&mutex, "part::quickTest");
        unloadIndexes();
        ierr = qtmp.evaluate();
    }
    if (ierr >= 0 && qtmp.getNumHits() > 0) {
        rid2 = qtmp.getRIDs();
        std::sort(rid2->begin(), rid2->end());
        if (rid1->size() == rid2->size()) {
            ibis::util::logger lg;
            uint32_t i, cnt=0;
            for (i=0; i<rid1->size(); ++i) {
                if ((*rid1)[i].value != (*rid2)[i].value) {
                    ++cnt;
                    lg() << i << "th RID " << (*rid1)[i]
                         << " != " << (*rid2)[i] << "\n";
                }
            }
            if (cnt > 0) {
                lg() << "Warning -- query[" << qtmp.id() << "] " << cnt
                     << " mismatches out of a total of "
                     << rid1->size() << "\n";
                ++(*nerrors);
            }
            else if (ibis::gVerbose > 4) {
                lg() << "RID query returned the expected RIDs"
                     << "\n";
            }
        }
        else {
            ibis::util::logger lg;
            lg() << "Warning -- query[" << qtmp.id() << "] sent "
                 << rid1->size() << " RIDs, got back "
                 << rid2->size() << "\n";
            uint32_t i=0, cnt;
            cnt = (rid1->size() < rid2->size()) ? rid1->size() :
                rid2->size();
            while (i < cnt) {
                lg() << (*rid1)[i] << " >>> " << (*rid2)[i] << "\n";
                ++i;
            }
            if (rid1->size() < rid2->size()) {
                while (i < rid2->size()) {
                    lg() << "??? >>> " << (*rid2)[i] << "\n";
                    ++i;
                }
            }
            else {
                while (i < rid1->size()) {
                    lg() << (*rid1)[i] << " >>> ???\n";
                    ++i;
                }
            }
            lg() << "\n";
            ++(*nerrors);
        }
        delete rid2;
    }
    else {
        ++ (*nerrors);
    }
    delete rid1;

    if (ibis::gVerbose > 2) {
        timer.stop();
        logMessage("quickTest", "tests on %s took %g sec(CPU), %g "
                   "sec(elapsed)", att->name(), timer.CPUTime(),
                   timer.realTime());
    }
} // ibis::part::quickTest

/// Loop through all operators for a continuous range expression,
/// check to see if the number of hits computed from evaluating a count
/// query matches that returned from ibis::part::countHits.
void ibis::part::testRangeOperators(const ibis::column* col,
                                    long* nerrors) const {
    if (col == 0 || nEvents <= 1) return;

    ibis::qExpr::COMPARE ops[] =
        {ibis::qExpr::OP_UNDEFINED, ibis::qExpr::OP_LT, ibis::qExpr::OP_LE,
         ibis::qExpr::OP_GT, ibis::qExpr::OP_GE, ibis::qExpr::OP_EQ};
    double b1 = col->lowerBound();
    double b2 = col->upperBound();
    if (b2 <= b1) {
        bool asc;
        col->computeMinMax(currentDataDir(), b1, b2, asc);
    }
    if (b2 <= b1) {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- part[" << (m_name ? m_name : "?")
            << "]::testRangeOperators(" << col->name()
            << ") could not determine the min/max values";
        ++ (*nerrors);
        return;
    }
    b2 -= b1;
    for (uint32_t i1 = 0; i1 < 6; ++ i1) {
        for (uint32_t i2 = 0; i2 < 6; ++ i2) {
            double r1 = ibis::util::rand();
            LOGGER(ibis::gVerbose > 3)
                << "part[" << (m_name ? m_name : "?")
                << "]::testRangeOperators test case " << i1 << ":" << i2
                << " -- " << b1 << ' ' << ops[i1] << ' ' << col->name()
                << ' ' << ops[i2] << ' ' << b1 + b2 * r1;
            ibis::qContinuousRange rng(b1, ops[i1], col->name(), ops[i2],
                                       b1 + b2 * r1);
            //ibis::query cq(ibis::util::userName(), this, pref);
            ibis::countQuery cq(this);
            long ierr = cq.setWhereClause(&rng);
            if (ierr < 0) {
                LOGGER(ibis::gVerbose >= 0)
                    << "Warning -- part[" << (m_name ? m_name : "?")
                    << "]::testRangeOperators could not assign " << rng
                    << " as a where clause to a count query";
                ++ (*nerrors);
                continue;
            }
            ierr = cq.evaluate();
            if (ierr >= 0) {
                ierr = countHits(rng);
                if (ierr < 0) {
                    LOGGER(ibis::gVerbose >= 0)
                        << "Warning -- part[" << (m_name ? m_name : "?")
                        << "]::testRangeOperators could not count hits for "
                        << rng << ", ierr = " << ierr;
                    ++ (*nerrors);
                }
                else if (ierr != cq.getNumHits()) {
                    LOGGER(ibis::gVerbose >= 0)
                        << "Warning -- part[" << (m_name ? m_name : "?")
                        << "]::testRangeOperators mismatching number of hits, "
                        "countHits(" << rng << ") returns " << ierr
                        << ", but countQuery::getNumHits returns "
                        << cq.getNumHits();
                    ++ (*nerrors);
                }
            }
            else if (ops[i1] != ibis::qExpr::OP_UNDEFINED ||
                     ops[i2] != ibis::qExpr::OP_UNDEFINED) {
                LOGGER(ibis::gVerbose >= 0)
                    << "Warning -- part[" << (m_name ? m_name : "?")
                    << "]::testRangeOperators could not evaluate expression "
                    << rng << ", ierr = " << ierr;
                ++ (*nerrors);
            }
        }
    }
} // ibis::part::testRangeOperators

/// Issues a query and then subdivided the range into three to check the
/// total hits of the three sub queries matches the hits of the single
/// query.  It allows a maximum of 6 levels of recursion, no more than 80
/// queries.
uint32_t ibis::part::recursiveQuery(const char* pref, const column* att,
                                    double low, double high,
                                    long *nerrors) const {
    uint32_t cnt0, cnt1, cnt2, cnt3;
    double mid1, mid2;
    {// use a block to limit the scope of query object
        char predicate[MAX_LINE];
        sprintf(predicate, "%g <= %s < %g", low, att->name(), high);
        query qtmp("[:]", this, pref);
        qtmp.setSelectClause(att->name());
        qtmp.setWhereClause(predicate);
        const char *str = qtmp.getLastError();
        if (str != 0 && *str != static_cast<char>(0)) {
            ibis::util::logger lg;
            lg() << "Warning -- part::queryTest last error on "
                 << "query \"" << predicate << "\" is \n" << str;
            qtmp.clearErrorMessage();
            ++(*nerrors);
        }

        if (ibis::gVerbose > 1)
            qtmp.logMessage("queryTest", "selectivity = %g", (high-low) /
                            (att->upperBound() - att->lowerBound()));

        // compute the number of hits
        qtmp.estimate(); // to provide some timing data
        int ierr = qtmp.evaluate();
        if (ierr < 0) { // unload all indexes, try once more
            ibis::util::mutexLock lock(&mutex, "part::queryTest");
            unloadIndexes();
            ierr = qtmp.evaluate();
        }
        if (ierr >= 0) {
            cnt0 = qtmp.getNumHits();
        }
        else {
            cnt0 = 0;
            ++ (*nerrors);
        }
        str = qtmp.getLastError();
        if (str != 0 && *str != static_cast<char>(0)) {
            ibis::util::logger lg;
            lg() << "Warning -- part::queryTest last error on "
                 << "query \"" << predicate << "\" is \n" << str;
            qtmp.clearErrorMessage();
            ++(*nerrors);
        }

        LOGGER(ibis::gVerbose > 4)
            << "part::queryTest(" << att->name() << ") found "
            << cnt0 << " hit" << (cnt0<2?"":"s") << " in ["
            << low << ", " << high << ")";

        { // use sequential scan to verify the hit list
            ibis::bitvector seqhits;
            ierr = qtmp.sequentialScan(seqhits);
            if (ierr < 0) {
                ++(*nerrors);
                logWarning("queryTest", "sequential scan failed");
            }
            else if (seqhits.cnt() != cnt0) {
                ++(*nerrors);
                logWarning("queryTest", "a sequential scan on \"%s\" produced "
                           "%lu, but the function evaluate produced %lu",
                           predicate,
                           static_cast<long unsigned>(seqhits.cnt()),
                           static_cast<long unsigned>(cnt0));
            }
            else {
                seqhits ^= *(qtmp.getHitVector());
                if (seqhits.cnt() > 0) {
                    ++(*nerrors);
                    logWarning("queryTest", "sequential scan on \"%s\" "
                               "produced %lu different result%s", predicate,
                               static_cast<long unsigned>(seqhits.cnt()),
                               (seqhits.cnt() > 1 ? "s" : ""));
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                    seqhits.compress();
                    LOGGER(ibis::gVerbose >= 0)
                        << "DEBUG -- queryTest -- " << seqhits;
#endif
                }
                else if (ibis::gVerbose > 3) {
                    logMessage("queryTest",
                               "sequential scan produced the same hits");
                }
            }
        }

        if (low == att->lowerBound() && high == att->upperBound()) {
            sprintf(predicate, "%s < %g", att->name(), low);
            qtmp.setWhereClause(predicate);
            ierr = qtmp.evaluate();
            if (ierr < 0) { // unload all indexes, try once more
                ibis::util::mutexLock lock(&mutex, "part::queryTest");
                unloadIndexes();
                ierr = qtmp.evaluate();
            }
            if (ierr >= 0) {
                cnt1 = qtmp.getNumHits();
            }
            else {
                cnt1 = 0;
                ++ (*nerrors);
            }
            sprintf(predicate, "%s >= %g", att->name(), high);
            qtmp.setWhereClause(predicate);
            ierr = qtmp.evaluate();
            if (ierr < 0) { // unload all indexes, try once more
                ibis::util::mutexLock lock(&mutex, "part::queryTest");
                unloadIndexes();
                ierr = qtmp.evaluate();
            }
            if (ierr >= 0) {
                cnt2 = qtmp.getNumHits();
            }
            else {
                cnt2 = 0;
                ++ (*nerrors);
            }

            uint32_t nev = nEvents;
            {
                ibis::bitvector tmp;
                att->getNullMask(tmp);
                nev = tmp.cnt();
            }
            if (cnt0+cnt1+cnt2 != nev) {
                logWarning("queryTest", "The total of %lu %s entries "
                           "(%lu |%g| %lu |%g| %lu) is different from the "
                           "expected %lu",
                           static_cast<long unsigned>(cnt0+cnt1+cnt2),
                           att->name(),
                           static_cast<long unsigned>(cnt1), low,
                           static_cast<long unsigned>(cnt0), high,
                           static_cast<long unsigned>(cnt2),
                           static_cast<long unsigned>(nEvents));
                ++(*nerrors);
            }
            else if (ibis::gVerbose > 3) {
                logMessage("queryTest", "The total of %lu %s entries "
                           "(%lu |%g| %lu |%g| %lu) is the same as the "
                           "expected %lu",
                           static_cast<long unsigned>(cnt0+cnt1+cnt2),
                           att->name(),
                           static_cast<long unsigned>(cnt1), low,
                           static_cast<long unsigned>(cnt0), high,
                           static_cast<long unsigned>(cnt2),
                           static_cast<long unsigned>(nEvents));
            }
        }
    }

    mid1 = high - low;
    double range = att->upperBound() - att->lowerBound();
    if ((mid1*64 > range) && (cnt0*256 > nEvents)) {
        mid1 = ibis::util::compactValue(0.125*(low*7+high), 0.5*(low+high));
        mid2 = ibis::util::compactValue(mid1, 0.125*(low+high*7));
        if (att->type() != ibis::FLOAT &&
            att->type() != ibis::DOUBLE) {
            // get rid of the fractional parts for consistent printing --
            // query processing automatically truncates floating-point
            // values to integers
            mid1 = ceil(mid1);
            mid2 = floor(mid2);
        }
        if (mid1 < mid2) {
            cnt1 = recursiveQuery(pref, att, low, mid1, nerrors);
            cnt2 = recursiveQuery(pref, att, mid1, mid2, nerrors);
            cnt3 = recursiveQuery(pref, att, mid2, high, nerrors);
            if (cnt0 != (cnt1 + cnt2 + cnt3)) {
                logWarning("queryTest", "The total of %lu %s rows "
                           "[%g| %lu |%g| %lu |%g| %lu |%g) is different "
                           "from the expected value %lu",
                           static_cast<long unsigned>(cnt1+cnt2+cnt3),
                           att->name(), low, static_cast<long unsigned>(cnt1),
                           mid1, static_cast<long unsigned>(cnt2), mid2,
                           static_cast<long unsigned>(cnt3), high,
                           static_cast<long unsigned>(cnt0));
                ++(*nerrors);
            }
            else if (ibis::gVerbose > 3) {
                logMessage("queryTest", "The total of %lu %s rows "
                           "[%g| %lu |%g| %lu |%g| %lu |%g) is the same as "
                           "the expected value %lu",
                           static_cast<long unsigned>(cnt1+cnt2+cnt3),
                           att->name(), low, static_cast<long unsigned>(cnt1),
                           mid1, static_cast<long unsigned>(cnt2), mid2,
                           static_cast<long unsigned>(cnt3), high,
                           static_cast<long unsigned>(cnt0));
            }
        }
    }
    return cnt0;
} // ibis::part::recursiveQuery

void ibis::part::composeQueryString
(std::string &str, const ibis::column* col1, const ibis::column* col2,
 const double &lower1, const double &upper1,
 const double &lower2, const double &upper2) const {
    std::ostringstream oss;
    oss << lower1 << " <= " << col1->name() << " < " << upper1 << " AND "
        << lower2 << " <= " << col2->name() << " < " << upper2;
    str = oss.str();
} // ibis::part::composeQueryString

/// Generate a list of random query conditions.  It selects nc columns from
/// the list of all columns and fills the array lst.conds and lst.super.
/// The array lst.hits is resized to the correct size, but left to be
/// filled with other functions.  It generates at least nc-1 queries.  When
/// nq > nc, it may generate one nq+1 queries because it always adds two
/// subranges as queries together.  This is to ensure that two sub-ranges
/// of any given range is present together for checkQueryList.
void ibis::part::buildQueryList(ibis::part::thrArg &lst,
                                unsigned nc, unsigned nq) const {
    lst.conds.clear();
    lst.super.clear();
    lst.hits.clear();
    if (columns.size() < nc || nc == 0 || nq == 0 || nEvents == 0) return;

    std::vector<const ibis::column*> cols(nc);
    std::vector<double> lower(nc);
    std::vector<double> upper(nc);
    lst.conds.reserve(nq+1);
    lst.super.reserve(nq+1);

    // select the columns
    columnList::const_iterator cit = columns.begin();
    for (int i = static_cast<int>(ibis::util::rand() * (columns.size() - nc));
         i > 0; ++ cit, -- i);
    for (unsigned i = 0; i < nc; ++ i) {
        cols[i] = (*cit).second;
        lower[i] = (*cit).second->lowerBound();
        upper[i] = (*cit).second->upperBound();
        if (! (lower[i] < upper[i])) {
            (*cit).second->computeMinMax();
            lower[i] = (*cit).second->lowerBound();
            upper[i] = (*cit).second->upperBound();
        }
        if (! (lower[i] <= upper[i])) {
            lower[i] = 0.0;
            upper[i] = 1.0;
            ++(*lst.nerrors);
        }
        ++ cit;
    }
    // shuffle the selected columns
    for (unsigned i = 0; i < nc; ++ i) {
        unsigned j = static_cast<unsigned>(ibis::util::rand() * nc);
        if (i != j) {
            const ibis::column *ctmp = cols[i];
            cols[i] = cols[j];
            cols[j] = ctmp;
            double dtmp = lower[i];
            lower[i] = lower[j];
            lower[j] = dtmp;
            dtmp = upper[i];
            upper[i] = upper[j];
            upper[j] = dtmp;
        }
    }

    ibis::MersenneTwister &mt = _ibis_part_urand();
    struct group {
        const ibis::column *col1;
        const ibis::column *col2;
        std::vector<unsigned> pos;
        std::vector<double> lower1, lower2, upper1, upper2;
    };
    group *grp = new group[nc-1];
    for (unsigned i = 0; i < nc-1; ++ i) {
        grp[i].col1 = cols[i];
        grp[i].col2 = cols[i+1];
        const double mid1 = lower[i] + (upper[i] - lower[i]) * mt();
        const double mid2 = lower[i+1] + (upper[i+1] - lower[i+1]) * mt();
        grp[i].pos.resize(2);
        grp[i].lower1.resize(2);
        grp[i].lower2.resize(2);
        grp[i].upper1.resize(2);
        grp[i].upper2.resize(2);
        grp[i].lower1[0] = lower[i];
        grp[i].upper1[0] = mid1;
        grp[i].lower2[0] = lower[i+1];
        grp[i].upper2[0] = mid2;
        grp[i].lower1[1] = mid1;
        grp[i].upper1[1] = upper[i];
        grp[i].lower2[1] = mid2;
        grp[i].upper2[1] = upper[i+1];

        std::string cnd1, cnd2;
        composeQueryString(cnd1, grp[i].col1, grp[i].col2,
                           grp[i].lower1[0], grp[i].upper1[0],
                           grp[i].lower2[0], grp[i].upper2[0]);
        composeQueryString(cnd2, grp[i].col1, grp[i].col2,
                           grp[i].lower1[1], grp[i].upper1[1],
                           grp[i].lower2[1], grp[i].upper2[1]);

        grp[i].pos[0] = i + i;
        grp[i].pos[1] = i + i + 1;
        lst.conds.push_back(cnd1);
        lst.conds.push_back(cnd2);
        lst.super.push_back(grp[i].pos[0]);
        lst.super.push_back(grp[i].pos[1]);
    }

    bool more = (lst.conds.size() < nq);
    bool expand1 = true;
    while (more) {
        for (unsigned ig = 0; ig < nc-1 && more; ++ ig) {
            std::vector<unsigned> pos;
            std::vector<double> lower1, lower2, upper1, upper2;
            pos.resize(2*grp[ig].pos.size());
            lower1.resize(2*grp[ig].pos.size());
            lower2.resize(2*grp[ig].pos.size());
            upper1.resize(2*grp[ig].pos.size());
            upper2.resize(2*grp[ig].pos.size());
            for (unsigned i = 0; i < grp[ig].lower1.size() && more; ++ i) {
                if (expand1) { // subdivide the range of col1
                    double mid1 = grp[ig].lower1[i] +
                        (grp[ig].upper1[i]-grp[ig].lower1[i]) * mt();
                    std::string front, back;
                    lower1[i+i] = grp[ig].lower1[i];
                    upper1[i+i] = mid1;
                    lower1[i+i+1] = mid1;
                    upper1[i+i+1] = grp[ig].upper1[i];
                    lower2[i+i] = grp[ig].lower2[i];
                    upper2[i+i] = grp[ig].upper2[i];
                    lower2[i+i+1] = grp[ig].lower2[i];
                    upper2[i+i+1] = grp[ig].upper2[i];
                    // front half of the range
                    composeQueryString(front, grp[ig].col1, grp[ig].col2,
                                       lower1[i+i], upper1[i+i],
                                       lower2[i+i], upper2[i+i]);
                    pos[i+i] = lst.conds.size();
                    lst.conds.push_back(front);
                    lst.super.push_back(grp[ig].pos[i]);
                    // back half of the range
                    composeQueryString(back, grp[ig].col1, grp[ig].col2,
                                       lower1[i+i+1], upper1[i+i+1],
                                       lower2[i+i+1], upper2[i+i+1]);
                    pos[i+i+1] = lst.conds.size();
                    lst.conds.push_back(back);
                    lst.super.push_back(grp[ig].pos[i]);
                    more = (lst.conds.size() < nq);
                    LOGGER(ibis::gVerbose > 4)
                        << "buildQueryList split (" << grp[ig].col1->name()
                        << "): " << lst.conds[grp[ig].pos[i]]
                        << " ==> " << front << " -|- " << back
#if defined(_DEBUG) || defined(DEBUG)
                        << "\n\tlst.super[" << lst.super.size()-2 << "]="
                        << lst.super[lst.super.size()-2] << ", lst.super["
                        << lst.super.size()-1 << "]="
                        << lst.super[lst.super.size()-1]
#endif
                        ;
                }
                else { // subdivide the range of col2
                    double mid2 = grp[ig].lower2[i] +
                        (grp[ig].upper2[i]-grp[ig].lower2[i]) * mt();
                    std::string front, back;
                    lower1[i+i] = grp[ig].lower1[i];
                    upper1[i+i] = grp[ig].upper1[i];
                    lower1[i+i+1] = grp[ig].lower1[i];
                    upper1[i+i+1] = grp[ig].upper1[i];
                    lower2[i+i] = grp[ig].lower2[i];
                    upper2[i+i] = mid2;
                    lower2[i+i+1] = mid2;
                    upper2[i+i+1] = grp[ig].upper2[i];
                    // front half of the range
                    composeQueryString(front, grp[ig].col1, grp[ig].col2,
                                       lower1[i+i], upper1[i+i],
                                       lower2[i+i], upper2[i+i]);
                    pos[i+i] = lst.conds.size();
                    lst.conds.push_back(front);
                    lst.super.push_back(grp[ig].pos[i]);
                    // back half of the range
                    composeQueryString(back, grp[ig].col1, grp[ig].col2,
                                       lower1[i+i+1], upper1[i+i+1],
                                       lower2[i+i+1], upper2[i+i+1]);
                    pos[i+i+1] = lst.conds.size();
                    lst.conds.push_back(back);
                    lst.super.push_back(grp[ig].pos[i]);
                    more = (lst.conds.size() < nq);
                    LOGGER(ibis::gVerbose > 4)
                        << "buildQueryList split (" << grp[ig].col2->name()
                        << "): " << lst.conds[grp[ig].pos[i]]
                        << " ==> " << front << " -|- " << back
#if defined(_DEBUG) || defined(DEBUG)
                        << "\n\tlst.super[" << lst.super.size()-2 << "]="
                        << lst.super[lst.super.size()-2] << ", lst.super["
                        << lst.super.size()-1 << "]="
                        << lst.super[lst.super.size()-1]
#endif
                        ;
                }
            } // for (unsigned i = 0; ...
            if (more) { // update the group with new records
                grp[ig].pos.swap(pos);
                grp[ig].lower1.swap(lower1);
                grp[ig].lower2.swap(lower2);
                grp[ig].upper1.swap(upper1);
                grp[ig].upper2.swap(upper2);
            }
        } // for (unsigned ig = 0; ...
        expand1 = !expand1; // swap the dimension to expand
    }
    delete [] grp;
    lst.hits.resize(lst.conds.size());
    LOGGER(ibis::gVerbose > 3)
        << "part[" << name() << "]::buildQueryList constructed "
        << lst.conds.size() << " sets of 2D range conditions";
} // ibis::part::buildQueryList

/// Sum up the hits from sub-divisions to verify the hits computing from
/// the whole range.  Based on the construction of buildQueryList, each
/// query condition knows which conditions contains it.
void ibis::part::checkQueryList(const ibis::part::thrArg &lst) const {
    unsigned nerr0 = 0;
    std::vector<unsigned> fromChildren(lst.conds.size(), 0U);
    for(unsigned i = lst.conds.size(); i > 0;) {
        -- i;
        if (lst.super[i] < i)
            fromChildren[lst.super[i]] += lst.hits[i];
        if (fromChildren[i] > 0) {
            if (fromChildren[i] != lst.hits[i]) {
                ++ nerr0;
                ++ (*lst.nerrors);
                LOGGER(ibis::gVerbose > 0)
                    << "Warning -- part::checkQueryList found the "
                    "number of hits (" << lst.hits[i] << ") for \""
                    << lst.conds[i] << "\" not matching the sum ("
                    << fromChildren[i] << ") from its two sub-divisions";
            }
        }
    }
    LOGGER(ibis::gVerbose > 3)
        << (nerr0 > 0 ? "Warning -- " : "")
        << "part[" << name() << "]::checkQueryList found "
        << nerr0 << " mismatch" << (nerr0>1 ? "es" : "");
} // ibis::part::checkQueryList

// three error logging functions
void ibis::part::logError(const char* event, const char* fmt, ...) const {
#if (defined(HAVE_VPRINTF) || defined(_WIN32)) && ! defined(DISABLE_VPRINTF)
    char* s = new char[std::strlen(fmt)+MAX_LINE];
    if (s != 0) {
        va_list args;
        va_start(args, fmt);
        vsprintf(s, fmt, args);
        va_end(args);

        {
            ibis::util::logger lg;
            lg() << " Error *** part[" << (m_name?m_name:"") << "]::"
                 << event << " -- " << s;
            if (errno != 0)
                lg() << " ... " << strerror(errno);
        }
        throw s;
    }
    else {
#endif
        {
            ibis::util::logger lg;
            lg() << " Error *** part[" << (m_name?m_name:"") << "]::"
                 << event << " == " << fmt << " ...";
            if (errno != 0)
                lg() << " ... " << strerror(errno);
        }
        throw fmt;
#if (defined(HAVE_VPRINTF) || defined(_WIN32)) && ! defined(DISABLE_VPRINTF)
    }
#endif
} // ibis::part::logError

void ibis::part::logWarning(const char* event,
                            const char* fmt, ...) const {
    if (ibis::gVerbose < 0)
        return;
    char tstr[28];
    ibis::util::getLocalTime(tstr);
    FILE* fptr = ibis::util::getLogFile();

    ibis::util::ioLock lock;
    fprintf(fptr, "%s\nWarning -- part[%s]::%s -- ", tstr,
            (m_name?m_name:""), event);

#if (defined(HAVE_VPRINTF) || defined(_WIN32)) && ! defined(DISABLE_VPRINTF)
    va_list args;
    va_start(args, fmt);
    vfprintf(fptr, fmt, args);
    va_end(args);
#else
    fprintf(fptr, "%s ...", fmt);
#endif
    if (errno != 0) {
        if (errno != ENOENT)
            fprintf(fptr, " ... %s", strerror(errno));
        errno = 0;
    }
    fprintf(fptr, "\n");
    fflush(fptr);
} // ibis::part::logWarning

void ibis::part::logMessage(const char* event,
                            const char* fmt, ...) const {
    FILE* fptr = ibis::util::getLogFile();
    ibis::util::ioLock lock;
#if defined(FASTBIT_TIMED_LOG)
    char tstr[28];
    ibis::util::getLocalTime(tstr);
    fprintf(fptr, "%s   ", tstr);
#endif
    fprintf(fptr, "part[%s]::%s -- ", (m_name?m_name:"?"), event);
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
} // ibis::part::logMessage

/// Perform comparisons with data in the named file.  It attempts to
/// allocate a buffer so the reading can be done in relatively large-size
/// chunks.  If it is unable to allocate a useful buffer, it will read one
/// value at a time.
template <typename T>
long ibis::part::doCompare(const char* file,
                           const ibis::qRange &cmp,
                           const ibis::bitvector &mask,
                           ibis::bitvector &hits) {
    ibis::horometer timer;
    if (ibis::gVerbose > 3)
        timer.start(); // start the timer

    int fdes = UnixOpen(file, OPEN_READONLY);
    if (fdes < 0) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- part::doCompare could not open file \"" << file
            << '"';
        hits.set(0, mask.size());
        return -1;
    }
    IBIS_BLOCK_GUARD(UnixClose, fdes);
#if defined(_WIN32) && defined(_MSC_VER)
    (void)_setmode(fdes, _O_BINARY);
#endif

    const unsigned elem = sizeof(T);
    // attempt to allocate a decent sized buffer for operations
    ibis::fileManager::buffer<T> mybuf;
    uint32_t nbuf = mybuf.size();
    T *buf = mybuf.address();

    uint32_t i=0, j=0;
    long diff, ierr=0;
    const ibis::bitvector::word_t *ii;
    if ((mask.size() >> 8) < mask.cnt()) { // use uncompressed hits internally
        hits.set(0, mask.size());
        hits.decompress();
    }
    else {
        hits.clear();
        hits.reserve(mask.size(), mask.cnt());
    }

    ibis::bitvector::indexSet idx = mask.firstIndexSet();

    if (buf) { // has a good size buffer to read data into
        while (idx.nIndices() > 0) { // the outer loop
            ii = idx.indices();
            if (idx.isRange()) {
                diff = *ii * elem;
                ierr = UnixSeek(fdes, diff, SEEK_SET);
                if (ierr != diff) {
                    LOGGER(ibis::gVerbose > 0)
                        << "part::doCompare(" << file
                        << ") could not seek to " << diff;
                    hits.clear();
                    return -2;
                }
                ibis::fileManager::instance().recordPages
                    (diff, elem*ii[1]);
                j = ii[1];
                for (i = *ii; i < ii[1]; i += diff) {
                    diff = nbuf;
                    if (i+diff > ii[1])
                        diff = ii[1] - i;
                    ierr = UnixRead(fdes, buf, elem*diff) / elem;
                    if (diff == ierr && ierr >= 0) {
                        j = ierr;
                    }
                    else {
                        j = 0;
                        LOGGER(ibis::gVerbose > 0)
                            << "Warning -- part::doCompare expected to read "
                            << diff << "values from \"" << file
                            << "\" but got only " << ierr;
                    }
                    for (uint32_t k = 0; k < j; ++k) {
                        if (cmp.inRange(buf[k])) {
                            hits.setBit(i+k, 1);
                        }
                    }
                }
            }
            else if (idx.nIndices() > 1) {
                diff = ii[idx.nIndices()-1] - *ii + 1;
                if (static_cast<uint32_t>(diff) < nbuf) {
                    ierr = UnixSeek(fdes, *ii * elem, SEEK_SET);
                    if (ierr != static_cast<long>(*ii * elem)) {
                        LOGGER(ibis::gVerbose > 0)
                            << "Warning -- part::doCompare(" << file
                            << ") could not seek to " << *ii *elem;
                        hits.clear();
                        return -3;
                    }
                    ierr = UnixRead(fdes, buf, elem*diff) / elem;
                    if (diff == ierr && ierr >= 0) {
                        j = idx.nIndices();
                    }
                    else {
                        j = 0;
                        LOGGER(ibis::gVerbose > 0)
                            << "Warning -- part::doCompare expected to read "
                            << diff << "values from \"" << file
                            << "\" but got only " << ierr;
                    }
                    for (i = 0; i < j; ++i) {
                        uint32_t k0 = ii[i] - *ii;
                        if (cmp.inRange(buf[k0])) {
                            hits.setBit(ii[i], 1);
                        }
                    }
                }
                else if (diff > 0) { // read one element at a time
                    for (i = 0; i < idx.nIndices(); ++i) {
                        j = ii[i];
                        diff = j * elem;
                        ierr = UnixSeek(fdes, diff, SEEK_SET);
                        if (ierr != diff) {
                            LOGGER(ibis::gVerbose > 0)
                                << "Warning -- part::doCompare(" << file
                                << ") could not seek to " << diff;
                            hits.clear();
                            return -4;
                        }
                        ierr = UnixRead(fdes, buf, elem);
                        if (ierr > 0) {
                            if (cmp.inRange(*buf)) {
                                hits.setBit(j, 1);
                            }
                        }
                        else {
                            LOGGER(ibis::gVerbose > 0)
                                << "Warning -- part::doCompare(" << file
                                << ") could not read a value at " << diff;
                        }
                    }
                }
                ibis::fileManager::instance().recordPages
                    (elem*ii[0], elem*ii[idx.nIndices()-1]);
            }
            else { // read a single value
                j = *ii;
                diff = j * elem;
                ierr = UnixSeek(fdes, diff, SEEK_SET);
                if (ierr != diff) {
                    LOGGER(ibis::gVerbose > 0)
                        << "Warning -- part::doCompare(" << file
                        << ") could not seek to " << diff;
                    hits.clear();
                    return -4;
                }
                ierr = UnixRead(fdes, buf, elem);
                if (ierr > 0) {
                    ibis::fileManager::instance().recordPages(diff, diff+elem);
                    if (cmp.inRange(*buf)) {
                        hits.setBit(j, 1);
                    }
                }
                else {
                    LOGGER(ibis::gVerbose > 0)
                        << "Warning -- part::doCompare(" << file
                        << ") could not read a value at " << diff;
                }
            }

            ++idx; // next set of selected entries
        } // while (idx.nIndices() > 0)
    }
    else { // no user buffer to use, read a single value at a time
        T tmp;
        while (idx.nIndices() > 0) { // the outer loop
            ii = idx.indices();
            if (idx.isRange()) {
                diff = *ii * elem;
                ierr = UnixSeek(fdes, diff, SEEK_SET);
                if (ierr != diff) {
                    LOGGER(ibis::gVerbose > 0)
                        << "Warning -- part::doCompare(" << file
                        << ") could not seek to " << diff;
                    hits.clear();
                    return -5;
                }

                ibis::fileManager::instance().recordPages(diff, elem*ii[1]);
                j = ii[1];
                for (i = *ii; i < j; ++i) {
                    ierr = UnixRead(fdes, &tmp, elem);
                    if (ierr > 0 && cmp.inRange(tmp)) {
                        hits.setBit(i, 1);
                    }
                }
            }
            else {
                for (i = 0; i < idx.nIndices(); ++i) {
                    j = ii[1];
                    diff = j * elem;
                    ierr = UnixSeek(fdes, diff, SEEK_SET);
                    if (ierr != diff) {
                        LOGGER(ibis::gVerbose > 0)
                            << "Warning -- part::doCompare(" << file
                            << ") could not seek to " << diff;
                        hits.clear();
                        return -6;
                    }

                    ierr = UnixRead(fdes, &tmp, elem);
                    if (ierr > 0 && cmp.inRange(tmp)) {
                        hits.setBit(j, 1);
                    }
                }
                ibis::fileManager::instance().recordPages
                    (elem*ii[0], elem*ii[idx.nIndices()-1]);
            }

            ++idx; // next set of selected entries
        } // while (idx.nIndices() > 0)
    }

    hits.compress();
    if (hits.size() != mask.size())
        hits.adjustSize(0, mask.size());

    if (ibis::gVerbose > 3) {
        timer.stop();
        ibis::util::logger lg;
        lg() << "part::doCompare -- comparison with column "
             << cmp.colName() << " on " << mask.cnt() << " element"
             << (mask.cnt() > 1 ? "s" : "") << " of " << typeid(T).name()
             << " from file \"" << file << "\" took "
             << timer.realTime() << " sec elapsed time and produced "
             << hits.cnt() << " hits" << "\n";
#if DEBUG+0 > 1 || _DEBUG+0 > 1
        lg() << "mask\n" << mask << "\nhit vector\n" << hits << "\n";
#endif
        ierr = hits.cnt();
    }
    else {
        ierr = hits.sloppyCount();
    }
    return ierr;
} // ibis::part::doCompare

/// Perform comparisons with data in the named file.  Place the values
/// satisfying the specified condition into res.  This function attempts to
/// allocate a buffer so the reading can be done in relatively large-size
/// chunks.  If it is unable to allocate a useful buffer, it will read one
/// value at a time.
template <typename T>
long ibis::part::doCompare(const char* file,
                           const ibis::qRange &cmp,
                           const ibis::bitvector &mask,
                           ibis::array_t<T> &res) {
    ibis::horometer timer;
    if (ibis::gVerbose > 3)
        timer.start(); // start the timer

    res.clear();
    int fdes = UnixOpen(file, OPEN_READONLY);
    if (fdes < 0) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- part::doCompare could not open file \"" << file
            << '"';
        return -1;
    }
    IBIS_BLOCK_GUARD(UnixClose, fdes);
#if defined(_WIN32) && defined(_MSC_VER)
    (void)_setmode(fdes, _O_BINARY);
#endif

    res.nosharing();
    if (res.capacity() < mask.cnt())
        res.reserve(mask.cnt() >> 1);

    const unsigned elem = sizeof(T);
    // attempt to allocate a decent sized buffer for operations
    ibis::fileManager::buffer<T> mybuf;
    uint32_t nbuf = mybuf.size();
    T *buf = mybuf.address();

    uint32_t i=0, j=0;
    long diff, ierr=0;
    const ibis::bitvector::word_t *ii;
    ibis::bitvector::indexSet idx = mask.firstIndexSet();

    if (buf) { // has a good size buffer to read data into
        while (idx.nIndices() > 0) { // the outer loop
            ii = idx.indices();
            if (idx.isRange()) {
                diff = *ii * elem;
                ierr = UnixSeek(fdes, diff, SEEK_SET);
                if (ierr != diff) {
                    LOGGER(ibis::gVerbose > 0)
                        << "Warning -- part::doCompare(" << file
                        << ") could not seek to " << diff;
                    res.clear();
                    return -2;
                }
                ibis::fileManager::instance().recordPages
                    (diff, elem*ii[1]);
                j = ii[1];
                for (i = *ii; i < ii[1]; i += diff) {
                    diff = nbuf;
                    if (i+diff > ii[1])
                        diff = ii[1] - i;
                    ierr = UnixRead(fdes, buf, elem*diff) / elem;
                    if (diff == ierr && ierr >= 0) {
                        j = ierr;
                    }
                    else {
                        j = 0;
                        LOGGER(ibis::gVerbose > 0)
                            << "Warning -- part::doCompare expected to read "
                            << diff << "values from \"" << file
                            << "\" but got only " << ierr;
                    }
                    for (uint32_t k = 0; k < j; ++k) {
                        if (cmp.inRange(buf[k])) {
                            res.push_back(buf[k]);
                        }
                    }
                }
            }
            else if (idx.nIndices() > 1) {
                diff = ii[idx.nIndices()-1] - *ii + 1;
                if (static_cast<uint32_t>(diff) < nbuf) {
                    ierr = UnixSeek(fdes, *ii * elem, SEEK_SET);
                    if (ierr != static_cast<long>(*ii * elem)) {
                        LOGGER(ibis::gVerbose > 0)
                            << "Warning -- part::doCompare(" << file
                            << ") could not seek to " << *ii *elem;
                        res.clear();
                        return -3;
                    }
                    ierr = UnixRead(fdes, buf, elem*diff) / elem;
                    if (diff == ierr && ierr >= 0) {
                        j = idx.nIndices();
                    }
                    else {
                        j = 0;
                        LOGGER(ibis::gVerbose > 0)
                            << "Warning -- part::doCompare expected to read "
                            << diff << "values from \"" << file
                            << "\" but got only " << ierr;
                    }
                    for (i = 0; i < j; ++i) {
                        uint32_t k0 = ii[i] - *ii;
                        if (cmp.inRange(buf[k0])) {
                            res.push_back(buf[k0]);
                        }
                    }
                }
                else if (diff > 0) { // read one element at a time
                    for (i = 0; i < idx.nIndices(); ++i) {
                        j = ii[i];
                        diff = j * elem;
                        ierr = UnixSeek(fdes, diff, SEEK_SET);
                        if (ierr != diff) {
                            LOGGER(ibis::gVerbose > 0)
                                << "Warning -- part::doCompare(" << file
                                << ") could not seek to " << diff;
                            res.clear();
                            return -4;
                        }
                        ierr = UnixRead(fdes, buf, elem);
                        if (ierr > 0) {
                            if (cmp.inRange(*buf)) {
                                res.push_back(*buf);
                            }
                        }
                        else {
                            LOGGER(ibis::gVerbose > 0)
                                << "Warning -- part::doCompare(" << file
                                << ") could not read a value at " << diff;
                        }
                    }
                }
                ibis::fileManager::instance().recordPages
                    (elem*ii[0], elem*ii[idx.nIndices()-1]);
            }
            else { // read a single value
                j = *ii;
                diff = j * elem;
                ierr = UnixSeek(fdes, diff, SEEK_SET);
                if (ierr != diff) {
                    LOGGER(ibis::gVerbose > 0)
                        << "Warning -- part::doCompare(" << file
                        << ") could not seek to " << diff;
                    res.clear();
                    return -4;
                }
                ierr = UnixRead(fdes, buf, elem);
                if (ierr > 0) {
                    ibis::fileManager::instance().recordPages(diff, diff+elem);
                    if (cmp.inRange(*buf)) {
                        res.push_back(*buf);
                    }
                }
                else {
                    LOGGER(ibis::gVerbose > 0)
                        << "Warning -- part::doCompare(" << file
                        << ") could not read a value at " << diff;
                }
            }

            ++idx; // next set of selected entries
        } // while (idx.nIndices() > 0)
    }
    else { // no user buffer to use, read a single value at a time
        T tmp;
        while (idx.nIndices() > 0) { // the outer loop
            ii = idx.indices();
            if (idx.isRange()) {
                diff = *ii * elem;
                ierr = UnixSeek(fdes, diff, SEEK_SET);
                if (ierr != diff) {
                    LOGGER(ibis::gVerbose > 0)
                        << "Warning -- part::doCompare(" << file
                        << ") could not seek to " << diff;
                    res.clear();
                    return -5;
                }

                ibis::fileManager::instance().recordPages(diff, elem*ii[1]);
                j = ii[1];
                for (i = *ii; i < j; ++i) {
                    ierr = UnixRead(fdes, &tmp, elem);
                    if (ierr > 0 && cmp.inRange(tmp)) {
                        res.push_back(tmp);
                    }
                }
            }
            else {
                for (i = 0; i < idx.nIndices(); ++i) {
                    j = ii[1];
                    diff = j * elem;
                    ierr = UnixSeek(fdes, diff, SEEK_SET);
                    if (ierr != diff) {
                        LOGGER(ibis::gVerbose > 0)
                            << "Warning -- part::doCompare(" << file
                            << ") could not seek to " << diff;
                        res.clear();
                        return -6;
                    }

                    ierr = UnixRead(fdes, &tmp, elem);
                    if (ierr > 0 && cmp.inRange(tmp)) {
                        res.push_back(tmp);
                    }
                }
                ibis::fileManager::instance().recordPages
                    (elem*ii[0], elem*ii[idx.nIndices()-1]);
            }

            ++idx; // next set of selected entries
        } // while (idx.nIndices() > 0)
    }

    ierr = res.size();
    if (ibis::gVerbose > 3) {
        timer.stop();
        ibis::util::logger lg;
        lg() << "part::doCompare -- comparison with column "
             << cmp.colName() << " on " << mask.cnt() << " element"
             << (mask.cnt() > 1 ? "s" : "") << " of " << typeid(T).name()
             << " from file \"" << file << "\" took "
             << timer.realTime() << " sec elapsed time and produced "
             << ierr << " hit" << (ierr > 1 ? "s" : "") << "\n";
    }

    return ierr;
} // ibis::part::doCompare

/// Perform comparisons with data in the named file.  Place the values
/// satisfying the specified condition into res.  This function attempts to
/// allocate a buffer so the reading can be done in relatively large-size
/// chunks.  If it is unable to allocate a useful buffer, it will read one
/// value at a time.
template <typename T>
long ibis::part::doCompare(const char* file,
                           const ibis::qRange &cmp,
                           const ibis::bitvector &mask,
                           ibis::array_t<T> &res,
                           ibis::bitvector &hits) {
    ibis::horometer timer;
    if (ibis::gVerbose > 3)
        timer.start(); // start the timer

    res.clear();
    hits.clear();
    int fdes = UnixOpen(file, OPEN_READONLY);
    if (fdes < 0) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- part::doCompare failed to open file \"" << file
            << '"';
        return -1;
    }
    IBIS_BLOCK_GUARD(UnixClose, fdes);
#if defined(_WIN32) && defined(_MSC_VER)
    (void)_setmode(fdes, _O_BINARY);
#endif

    res.nosharing();
    if (res.capacity() < mask.cnt())
        res.reserve(mask.cnt() >> 1);

    const unsigned elem = sizeof(T);
    // attempt to allocate a decent sized buffer for operations
    ibis::fileManager::buffer<T> mybuf;
    uint32_t nbuf = mybuf.size();
    T *buf = mybuf.address();

    if ((mask.size() >> 8) < mask.cnt()) {
        hits.set(0, mask.size());
        hits.decompress();
    }

    uint32_t i=0, j=0;
    long diff, ierr=0;
    const ibis::bitvector::word_t *ii;
    ibis::bitvector::indexSet idx = mask.firstIndexSet();

    if (buf) { // has a good size buffer to read data into
        while (idx.nIndices() > 0) { // the outer loop
            ii = idx.indices();
            if (idx.isRange()) {
                diff = *ii * elem;
                ierr = UnixSeek(fdes, diff, SEEK_SET);
                if (ierr != diff) {
                    LOGGER(ibis::gVerbose > 0)
                        << "Warning -- part::doCompare(" << file
                        << ") could not seek to " << diff;
                    res.clear();
                    return -2;
                }
                ibis::fileManager::instance().recordPages
                    (diff, elem*ii[1]);
                j = ii[1];
                for (i = *ii; i < ii[1]; i += diff) {
                    diff = nbuf;
                    if (i+diff > ii[1])
                        diff = ii[1] - i;
                    ierr = UnixRead(fdes, buf, elem*diff) / elem;
                    if (diff == ierr && ierr >= 0) {
                        j = ierr;
                    }
                    else {
                        j = 0;
                        LOGGER(ibis::gVerbose > 0)
                            << "Warning -- part::doCompare expected to read "
                            << diff << "values from \"" << file
                            << "\" but got only " << ierr;
                    }
                    for (uint32_t k = 0; k < j; ++k) {
                        if (cmp.inRange(buf[k])) {
                            res.push_back(buf[k]);
                            hits.setBit(i+k, 1);
                        }
                    }
                }
            }
            else if (idx.nIndices() > 1) {
                diff = ii[idx.nIndices()-1] - *ii + 1;
                if (static_cast<uint32_t>(diff) < nbuf) {
                    ierr = UnixSeek(fdes, *ii * elem, SEEK_SET);
                    if (ierr != static_cast<long>(*ii * elem)) {
                        LOGGER(ibis::gVerbose > 0)
                            << "Warning -- part::doCompare(" << file
                            << ") could not seek to " << *ii *elem;
                        res.clear();
                        return -3;
                    }
                    ierr = UnixRead(fdes, buf, elem*diff) / elem;
                    if (diff == ierr && ierr >= 0) {
                        j = idx.nIndices();
                    }
                    else {
                        j = 0;
                        LOGGER(ibis::gVerbose > 0)
                            << "Warning -- part::doCompare expected to read "
                            << diff << "values from \"" << file
                            << "\" but got only " << ierr;
                    }
                    for (i = 0; i < j; ++i) {
                        uint32_t k0 = ii[i] - *ii;
                        if (cmp.inRange(buf[k0])) {
                            res.push_back(buf[k0]);
                            hits.setBit(ii[i], 1);
                        }
                    }
                }
                else if (diff > 0) { // read one element at a time
                    for (i = 0; i < idx.nIndices(); ++i) {
                        j = ii[i];
                        diff = j * elem;
                        ierr = UnixSeek(fdes, diff, SEEK_SET);
                        if (ierr != diff) {
                            LOGGER(ibis::gVerbose > 0)
                                << "Warning -- part::doCompare(" << file
                                << ") could not seek to " << diff;
                            res.clear();
                            return -4;
                        }
                        ierr = UnixRead(fdes, buf, elem);
                        if (ierr > 0) {
                            if (cmp.inRange(*buf)) {
                                res.push_back(*buf);
                                hits.setBit(j, 1);
                            }
                        }
                        else {
                            LOGGER(ibis::gVerbose > 0)
                                << "Warning -- part::doCompare(" << file
                                << ") could not read a value at " << diff;
                        }
                    }
                }
                ibis::fileManager::instance().recordPages
                    (elem*ii[0], elem*ii[idx.nIndices()-1]);
            }
            else { // read a single value
                j = *ii;
                diff = j * elem;
                ierr = UnixSeek(fdes, diff, SEEK_SET);
                if (ierr != diff) {
                    LOGGER(ibis::gVerbose > 0)
                        << "Warning -- part::doCompare(" << file
                        << ") could not seek to " << diff;
                    res.clear();
                    return -4;
                }
                ierr = UnixRead(fdes, buf, elem);
                if (ierr > 0) {
                    ibis::fileManager::instance().recordPages(diff, diff+elem);
                    if (cmp.inRange(*buf)) {
                        res.push_back(*buf);
                        hits.setBit(j, 1);
                    }
                }
                else {
                    LOGGER(ibis::gVerbose > 0)
                        << "Warning -- part::doCompare(" << file
                        << ") could not read a value at " << diff;
                }
            }

            ++idx; // next set of selected entries
        } // while (idx.nIndices() > 0)
    }
    else { // no user buffer to use, read a single value at a time
        T tmp;
        while (idx.nIndices() > 0) { // the outer loop
            ii = idx.indices();
            if (idx.isRange()) {
                diff = *ii * elem;
                ierr = UnixSeek(fdes, diff, SEEK_SET);
                if (ierr != diff) {
                    LOGGER(ibis::gVerbose > 0)
                        << "Warning -- part::doCompare(" << file
                        << ") could not seek to " << diff;
                    res.clear();
                    return -5;
                }

                ibis::fileManager::instance().recordPages(diff, elem*ii[1]);
                j = ii[1];
                for (i = *ii; i < j; ++i) {
                    ierr = UnixRead(fdes, &tmp, elem);
                    if (ierr > 0 && cmp.inRange(tmp)) {
                        res.push_back(tmp);
                        hits.setBit(i, 1);
                    }
                }
            }
            else {
                for (i = 0; i < idx.nIndices(); ++i) {
                    j = ii[1];
                    diff = j * elem;
                    ierr = UnixSeek(fdes, diff, SEEK_SET);
                    if (ierr != diff) {
                        LOGGER(ibis::gVerbose > 0)
                            << "Warning -- part::doCompare(" << file
                            << ") could not seek to " << diff;
                        res.clear();
                        return -6;
                    }

                    ierr = UnixRead(fdes, &tmp, elem);
                    if (ierr > 0 && cmp.inRange(tmp)) {
                        res.push_back(tmp);
                        hits.setBit(j, 1);
                    }
                }
                ibis::fileManager::instance().recordPages
                    (elem*ii[0], elem*ii[idx.nIndices()-1]);
            }

            ++idx; // next set of selected entries
        } // while (idx.nIndices() > 0)
    }

    hits.compress();
    if (hits.size() != mask.size())
        hits.adjustSize(0, mask.size());
    ierr = res.size();
    if (ibis::gVerbose > 3) {
        timer.stop();
        ibis::util::logger lg;
        lg() << "part::doCompare -- comparison with column "
             << cmp.colName() << " on " << mask.cnt() << " element"
             << (mask.cnt() > 1 ? "s" : "") << " of " << typeid(T).name()
             << " from file \"" << file << "\" took "
             << timer.realTime() << " sec elapsed time and produced "
             << ierr << " hit" << (ierr > 1 ? "s" : "") << "\n";
    }
    return ierr;
} // ibis::part::doCompare

/// The function that performs the actual comparison for range queries.
/// The size of array may either match the number of bits in @c mask or the
/// number of set bits in @c mask.  This allows one to either use the whole
/// array or the only the elements need for this operation.  In either
/// case, only mask.cnt() elements of array are checked but position of the
/// bits that need to be set in the output bitvector @c hits have to be
/// handled differently.
template <typename T>
long ibis::part::doCompare(const array_t<T> &array,
                           const ibis::qRange &cmp,
                           const ibis::bitvector &mask,
                           ibis::bitvector &hits) {
    ibis::horometer timer;
    if (ibis::gVerbose > 3) timer.start(); // start the timer

    long ierr = 0;
    uint32_t i=0, j=0;
    const bool uncomp = ((mask.size() >> 8) < mask.cnt());
    if (uncomp) { // use uncompressed hits internally
        hits.set(0, mask.size());
        hits.decompress();
    }
    else {
        hits.clear();
        hits.reserve(mask.size(), mask.cnt());
    }

    ibis::bitvector::indexSet idx = mask.firstIndexSet();
    if (array.size() == mask.size()) { // full array available
        while (idx.nIndices() > 0) { // the outer loop
            const ibis::bitvector::word_t *ii = idx.indices();
            if (idx.isRange()) {
                for (j = *ii; j < ii[1]; ++j) {
                    if (cmp.inRange(array[j])) {
                        hits.setBit(j, 1);
                        ++ ierr;
#if DEBUG+0 > 1 || _DEBUG+0 > 1
                        LOGGER(ibis::gVerbose >= 0)
                            << "DEBUG -- doCompare" << array[j] << " is in "
                            << cmp;
#endif
                    }
                }
            }
            else {
                for (i = 0; i < idx.nIndices(); ++i) {
                    j = ii[i];
                    if (cmp.inRange(array[j])) {
                        hits.setBit(j, 1);
                        ++ ierr;
#if DEBUG+0 > 1 || _DEBUG+0 > 1
                        LOGGER(ibis::gVerbose >= 0)
                            << "DEBUG -- doCompare " << array[j] << " is in "
                            << cmp;
#endif
                    }
                }
            }

            ++idx; // next set of selected entries
        }
    }
    else if (array.size() == mask.cnt()) { // packed array available
        while (idx.nIndices() > 0) {
            const ibis::bitvector::word_t *ii = idx.indices();
            if (idx.isRange()) {
                for (unsigned k = *ii; k < ii[1]; ++ k) {
                    if (cmp.inRange(array[j++])) {
                        hits.setBit(k, 1);
                        ++ ierr;
#if DEBUG+0 > 1 || _DEBUG+0 > 1
                        LOGGER(ibis::gVerbose >= 0)
                            << "DEBUG -- doCompare" << array[j] << " is in "
                            << cmp << ", setting position " << k
                            << " of hit vector to 1";
#endif
                    }
                }
            }
            else {
                for (uint32_t k = 0; k < idx.nIndices(); ++ k) {
                    if (cmp.inRange(array[j++])) {
                        hits.setBit(ii[k], 1);
                        ++ ierr;
#if DEBUG+0 > 1 || _DEBUG+0 > 1
                        LOGGER(ibis::gVerbose >= 0)
                            << "DEBUG -- doCompare " << array[j] << " is in "
                            << cmp << ", setting position " << ii[k]
                            << " of hit vector to 1";
#endif
                    }
                }
            }
            ++ idx;
        }
    }
    else {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- part::doCompare requires the input data array size ("
            << array.size() << ") to be either " << mask.size() << " or "
            << mask.cnt();
        ierr = -6;
    }

    if (uncomp)
        hits.compress();
    else if (hits.size() != mask.size())
        hits.adjustSize(0, mask.size());

    if (ibis::gVerbose > 3 && ierr >= 0) {
        timer.stop();
        ibis::util::logger lg;
        lg() << "part::doCompare -- comparison with column "
             << cmp.colName() << " on " << mask.cnt() << " element"
             << (mask.cnt() > 1 ? "s" : "") << " of a "
             << typeid(T).name() << "-array[" << array.size()
             << "] took " << timer.realTime()
             << " sec elapsed time and produced "
             << ierr << " hit" << (ierr>1?"s":"");
#if DEBUG+0 > 1 || _DEBUG+0 > 1
        lg() << "\nmask\n" << mask << "\nhit vector\n" << hits;
#endif
    }
    return ierr;
} // ibis::part::doCompare

/// The function that performs the actual comparison for range queries.
/// The size of array may either match the number of bits in @c mask or the
/// number of set bits in @c mask.  This allows one to either use the whole
/// array or the only the elements need for this operation.  The values
/// satisfying the query condition is stored in res.
///
/// The return value is the number of elements in res or a negative number
/// to indicate error.
template <typename T>
long ibis::part::doCompare(const array_t<T> &array,
                           const ibis::qRange &cmp,
                           const ibis::bitvector &mask,
                           ibis::array_t<T> &res) {
    ibis::horometer timer;
    if (ibis::gVerbose > 3) timer.start(); // start the timer

    res.clear();
    res.nosharing();
    long ierr = 0;
    uint32_t i=0, j=0;
    if (res.capacity() < mask.cnt())
        res.reserve(mask.cnt() >> 1);

    ibis::bitvector::indexSet idx = mask.firstIndexSet();
    if (array.size() == mask.size()) { // full array available
        while (idx.nIndices() > 0) { // the outer loop
            const ibis::bitvector::word_t *ii = idx.indices();
            if (idx.isRange()) {
                for (j = *ii; j < ii[1]; ++j) {
                    if (cmp.inRange(array[j])) {
                        res.push_back(array[j]);
#if DEBUG+0 > 1 || _DEBUG+0 > 1
                        LOGGER(ibis::gVerbose >= 0)
                            << "DEBUG -- doCompare" << array[j] << " is in "
                            << cmp;
#endif
                    }
                }
            }
            else {
                for (i = 0; i < idx.nIndices(); ++i) {
                    j = ii[i];
                    if (cmp.inRange(array[j])) {
                        res.push_back(array[j]);
#if DEBUG+0 > 1 || _DEBUG+0 > 1
                        LOGGER(ibis::gVerbose >= 0)
                            << "DEBUG -- doCompare " << array[j] << " is in "
                            << cmp;
#endif
                    }
                }
            }

            ++ idx; // next set of selected entries
        }
        ierr = res.size();
    }
    else if (array.size() == mask.cnt()) { // packed array available
        while (idx.nIndices() > 0) {
            const ibis::bitvector::word_t *ii = idx.indices();
            if (idx.isRange()) {
                for (unsigned k = *ii; k < ii[1]; ++ k) {
                    if (cmp.inRange(array[j])) {
                        res.push_back(array[j]);
#if DEBUG+0 > 1 || _DEBUG+0 > 1
                        LOGGER(ibis::gVerbose >= 0)
                            << "DEBUG -- doCompare" << array[j] << " is in "
                            << cmp << ", setting position " << k
                            << " of hit vector to 1";
#endif
                    }
                    ++ j;
                }
            }
            else {
                for (uint32_t k = 0; k < idx.nIndices(); ++ k) {
                    if (cmp.inRange(array[j])) {
                        res.push_back(array[j]);
#if DEBUG+0 > 1 || _DEBUG+0 > 1
                        LOGGER(ibis::gVerbose >= 0)
                            << "DEBUG -- doCompare " << array[j] << " is in "
                            << cmp << ", setting position " << ii[k]
                            << " of hit vector to 1";
#endif
                    }
                    ++ j;
                }
            }
            ++ idx;
        }
        ierr = res.size();
    }
    else {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- part::doCompare requires the input data array size ("
            << array.size() << ") to be either " << mask.size() << " or "
            << mask.cnt();
        ierr = -6;
    }

    if (ibis::gVerbose > 3 && ierr >= 0) {
        timer.stop();
        ibis::util::logger lg;
        lg() << "part::doCompare -- comparison with column "
             << cmp.colName() << " on " << mask.cnt() << " element"
             << (mask.cnt() > 1 ? "s" : "") << " of a "
             << typeid(T).name() << "-array[" << array.size()
             << "] took " << timer.realTime()
             << " sec elapsed time and produced " << ierr
             << " hit" << (ierr>1?"s":"") << "\n";
    }
    return ierr;
} // ibis::part::doCompare

/// The function that performs the actual comparison for range queries.
/// The size of array may either match the number of bits in @c mask or the
/// number of set bits in @c mask.  This allows one to either use the whole
/// array or the only the elements need for this operation.  The values
/// satisfying the query condition is stored in res.
///
/// The return value is the number of elements in res or a negative number
/// to indicate error.
template <typename T>
long ibis::part::doCompare(const array_t<T> &array,
                           const ibis::qRange &cmp,
                           const ibis::bitvector &mask,
                           ibis::array_t<T> &res,
                           ibis::bitvector &hits) {
    ibis::horometer timer;
    if (ibis::gVerbose > 3) timer.start(); // start the timer

    res.clear();
    res.nosharing();
    long ierr = 0;
    uint32_t i=0, j=0;
    if (res.capacity() < mask.cnt())
        res.reserve(mask.cnt() >> 1);

    ibis::bitvector::indexSet idx = mask.firstIndexSet();
    if (array.size() == mask.size()) { // full array available
        while (idx.nIndices() > 0) { // the outer loop
            const ibis::bitvector::word_t *ii = idx.indices();
            if (idx.isRange()) {
                for (j = *ii; j < ii[1]; ++j) {
                    if (cmp.inRange(array[j])) {
                        res.push_back(array[j]);
#if DEBUG+0 > 1 || _DEBUG+0 > 1
                        LOGGER(ibis::gVerbose >= 0)
                            << "DEBUG -- doCompare" << array[j] << " is in "
                            << cmp;
#endif
                    }
                }
            }
            else {
                for (i = 0; i < idx.nIndices(); ++i) {
                    j = ii[i];
                    if (cmp.inRange(array[j])) {
                        res.push_back(array[j]);
#if DEBUG+0 > 1 || _DEBUG+0 > 1
                        LOGGER(ibis::gVerbose >= 0)
                            << "DEBUG -- doCompare " << array[j] << " is in "
                            << cmp;
#endif
                    }
                }
            }

            ++ idx; // next set of selected entries
        }
        ierr = res.size();
    }
    else if (array.size() == mask.cnt()) { // packed array available
        while (idx.nIndices() > 0) {
            const ibis::bitvector::word_t *ii = idx.indices();
            if (idx.isRange()) {
                for (unsigned k = *ii; k < ii[1]; ++ k) {
                    if (cmp.inRange(array[j])) {
                        res.push_back(array[j]);
#if DEBUG+0 > 1 || _DEBUG+0 > 1
                        LOGGER(ibis::gVerbose >= 0)
                            << "DEBUG -- doCompare" << array[j] << " is in "
                            << cmp << ", setting position " << k
                            << " of hit vector to 1";
#endif
                    }
                    ++ j;
                }
            }
            else {
                for (uint32_t k = 0; k < idx.nIndices(); ++ k) {
                    if (cmp.inRange(array[j])) {
                        res.push_back(array[j]);
#if DEBUG+0 > 1 || _DEBUG+0 > 1
                        LOGGER(ibis::gVerbose >= 0)
                            << "DEBUG -- doCompare " << array[j] << " is in "
                            << cmp << ", setting position " << ii[k]
                            << " of hit vector to 1";
#endif
                    }
                    ++ j;
                }
            }
            ++ idx;
        }
        ierr = res.size();
    }
    else {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- part::doCompare requires the input data array size ("
            << array.size() << ") to be either " << mask.size() << " or "
            << mask.cnt();
        ierr = -6;
    }

    if (ibis::gVerbose > 3 && ierr >= 0) {
        timer.stop();
        ibis::util::logger lg;
        lg() << "part::doCompare -- comparison with column "
             << cmp.colName() << " on " << mask.cnt() << " element"
             << (mask.cnt() > 1 ? "s" : "") << " of a "
             << typeid(T).name() << "-array[" << array.size()
             << "] took " << timer.realTime()
             << " sec elapsed time and produced " << ierr
             << " hit" << (ierr>1?"s":"") << "\n";
    }
    return ierr;
} // ibis::part::doCompare

// hits are those do not satisfy the speficied range condition
template <typename T>
long ibis::part::negativeCompare(const array_t<T> &array,
                                 const ibis::qRange &cmp,
                                 const ibis::bitvector &mask,
                                 ibis::bitvector &hits) {
    ibis::horometer timer;
    if (ibis::gVerbose > 3) timer.start(); // start the timer

    uint32_t i=0, j=0;
    long ierr=0;
    const uint32_t nelm = (array.size() <= mask.size() ?
                           array.size() : mask.size());
    const bool uncomp = ((mask.size() >> 8) < mask.cnt());
    if (uncomp) { // use uncompressed hits internally
        hits.set(0, mask.size());
        hits.decompress();
    }
    else {
        hits.clear();
        hits.reserve(mask.size(), mask.cnt());
    }

    ibis::bitvector::indexSet idx = mask.firstIndexSet();
    while (idx.nIndices() > 0) { // the outer loop
        const ibis::bitvector::word_t *ii = idx.indices();
        if (idx.isRange()) {
            uint32_t diff = (ii[1] <= nelm ? ii[1] : nelm);
            for (j = *ii; j < diff; ++j) {
                if (! cmp.inRange(array[j])) {
                    hits.setBit(j, 1);
                    ++ ierr;
#if DEBUG+0 > 1 || _DEBUG+0 > 1
                    LOGGER(ibis::gVerbose >= 0)
                        << "DEBUG -- negativeCompare " << array[j]
                        << " is not in " << cmp;
#endif
                }
            }
        }
        else {
            for (i = 0; i < idx.nIndices(); ++i) {
                j = ii[i];
                if (j < nelm && ! cmp.inRange(array[j])) {
                    hits.setBit(j, 1);
                    ++ ierr;
#if DEBUG+0 > 1 || _DEBUG+0 > 1
                    LOGGER(ibis::gVerbose >= 0)
                        << "DEBUG -- negativeCompare " << array[j]
                        << " is no in " << cmp;
#endif
                }
            }
        }

        ++idx;
    }

    if (uncomp)
        hits.compress();
    else if (hits.size() < mask.size())
        hits.adjustSize(0, mask.size());

    if (ibis::gVerbose > 3 && ierr >= 0) {
        timer.stop();
        ibis::util::logger lg;
        lg() << "part::negativeCompare -- comparison with column "
             << cmp.colName() << " on " << mask.cnt()<< " element"
             << (mask.cnt() > 1 ? "s" : "") << " of a "
             << typeid(T).name() << "-array[" << array.size()
             << "] took " << timer.realTime()
             << " sec elapsed time and produced "
             << ierr << " hit" << (ierr>1?"s":"");
#if DEBUG+0 > 1 || _DEBUG+0 > 1
        lg() << "\nmask\n" << mask << "\nhit vector\n" << hits << "\n";
#endif
    }
    return ierr;
} // ibis::part::negativeCompare

template <typename T>
long ibis::part::negativeCompare(const char* file,
                                 const ibis::qRange &cmp,
                                 const ibis::bitvector &mask,
                                 ibis::bitvector &hits) {
    ibis::horometer timer;
    if (ibis::gVerbose > 3)
        timer.start(); // start the timer

    hits.clear(); // clear the existing content
    int fdes = UnixOpen(file, OPEN_READONLY);
    if (fdes < 0) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- part::negativeCompare could not open file \""
            << file << '"';
        hits.set(0, mask.size());
        return -1;
    }
    IBIS_BLOCK_GUARD(UnixClose, fdes);
#if defined(_WIN32) && defined(_MSC_VER)
    (void)_setmode(fdes, _O_BINARY);
#endif

    const unsigned elem = sizeof(T);
    // attempt to allocate a decent size buffer for operations
    ibis::fileManager::buffer<T> mybuf;
    uint32_t nbuf = mybuf.size();
    T *buf = mybuf.address();

    uint32_t i=0, j=0;
    long diff, ierr=0;
    const ibis::bitvector::word_t *ii;
    const bool uncomp = ((mask.size() >> 8) < mask.cnt());
    if (uncomp) { // use uncompressed hits internally
        hits.set(0, mask.size());
        hits.decompress();
    }
    else {
        hits.clear();
        hits.reserve(mask.size(), mask.cnt());
    }

    ibis::bitvector::indexSet idx = mask.firstIndexSet();

    if (buf) { // has a good size buffer to read data into
        while (idx.nIndices() > 0) { // the outer loop
            ii = idx.indices();
            if (idx.isRange()) {
                diff = *ii * elem;
                ierr = UnixSeek(fdes, diff, SEEK_SET);
                if (ierr != diff) {
                    LOGGER(ibis::gVerbose > 0)
                        << "Warning -- part::negativeCompare(" << file
                        << ") could not seek to " << diff;
                    hits.clear();
                    return -3;
                }

                ibis::fileManager::instance().recordPages
                    (diff, elem*ii[1]);
                j = ii[1];
                for (i = *ii; i < ii[1]; i += diff) {
                    diff = nbuf;
                    if (i+diff > ii[1])
                        diff = ii[1] - i;
                    ierr = UnixRead(fdes, buf, elem*diff) / elem;
                    if (ierr > 0 && static_cast<int32_t>(diff) == diff) {
                        j = diff;
                    }
                    else {
                        j = 0;
                        LOGGER(ibis::gVerbose > 0)
                            << "Warning -- part::negativeCompare expected "
                            "to read " << diff << " values from \""
                            << file << "\" but got only " << ierr;
                    }
                    for (uint32_t k = 0; k < j; ++k) {
                        if (! cmp.inRange(buf[k])) {
                            hits.setBit(i+k, 1);
                        }
                    }
                }
            }
            else if (idx.nIndices() > 1) {
                j = idx.nIndices() - 1;
                diff = ii[j] - *ii + 1;
                if (static_cast<uint32_t>(diff) < nbuf) {
                    ierr = UnixSeek(fdes, *ii * elem, SEEK_SET);
                    if (ierr != static_cast<long>(*ii * elem)) {
                        LOGGER(ibis::gVerbose > 0)
                            << "Warning -- part::negativeCompare(" << file
                            << ") could not seek to " << *ii *elem;
                        hits.clear();
                        return -4;
                    }

                    ierr = UnixRead(fdes, buf, elem*diff) / elem;
                    if (ierr > 0 && ierr == diff) {
                        j = diff;
                    }
                    else {
                        j = 0;
                        LOGGER(ibis::gVerbose > 0)
                            << "Warning -- part::negativeCompare(" << file
                            << ") expected to read " << diff
                            << " elements of " << elem << "-byte each, "
                            << "but got " << ierr;
                    }
                    for (i = 0; i < j; ++i) {
                        uint32_t k0 = ii[i] - *ii;
                        if (! cmp.inRange(buf[k0])) {
                            hits.setBit(ii[i], 1);
                        }
                    }
                }
                else {
                    for (i = 0; i < idx.nIndices(); ++i) {
                        j = ii[i];
                        diff = j * elem;
                        ierr = UnixSeek(fdes, diff, SEEK_SET);
                        if (ierr != diff) {
                            LOGGER(ibis::gVerbose > 0)
                                << "Warning -- part::negativeCompare(" << file
                                << ") could not seek to " << diff;
                            hits.clear();
                            return -5;
                        }
                        ierr = UnixRead(fdes, buf, elem);
                        if (ierr > 0) {
                            if (! cmp.inRange(*buf)) {
                                hits.setBit(j, 1);
                            }
                        }
                    }
                }
                ibis::fileManager::instance().recordPages
                    (elem*ii[0], elem*ii[idx.nIndices()-1]);
            }
            else {
                j = *ii;
                diff = j * elem;
                ierr = UnixSeek(fdes, diff, SEEK_SET);
                if (ierr != diff) {
                    LOGGER(ibis::gVerbose > 0)
                        << "Warning -- part::negativeCompare(" << file
                        << ") could not seek to " << diff;
                    hits.clear();
                    return -6;
                }
                ierr = UnixRead(fdes, buf, elem);
                if (ierr > 0) {
                    ibis::fileManager::instance().recordPages(diff, diff+elem);
                    if (! cmp.inRange(*buf)) {
                        hits.setBit(j, 1);
                    }
                }
            }

            ++idx; // next set of selected entries
        } // while (idx.nIndices() > 0)
    }
    else { // no user buffer to use, read one element at a time
        T tmp;
        while (idx.nIndices() > 0) { // the outer loop
            ii = idx.indices();
            if (idx.isRange()) {
                diff = *ii * elem;
                ierr = UnixSeek(fdes, diff, SEEK_SET);
                if (ierr != diff) {
                    LOGGER(ibis::gVerbose > 0)
                        << "Warning -- part::negativeCompare(" << file
                        << ") could not seek to " << diff;
                    hits.clear();
                    return -7;
                }
                ibis::fileManager::instance().recordPages(diff, elem*ii[1]);
                j = ii[1];
                for (i = *ii; i < j; ++i) {
                    ierr = UnixRead(fdes, &tmp, elem);
                    if (ierr > 0) {
                        if (! cmp.inRange(tmp)) {
                            hits.setBit(i, 1);
                        }
                    }
                }
            }
            else {
                for (i = 0; i < idx.nIndices(); ++i) {
                    j = ii[1];
                    diff = j * elem;
                    ierr = UnixSeek(fdes, diff, SEEK_SET);
                    if (ierr != diff) {
                        LOGGER(ibis::gVerbose > 0)
                            << "Warning -- part::negativeCompare(" << file
                            << ") could not seek to " << diff;
                        hits.clear();
                        return -8;
                    }
                    ierr = UnixRead(fdes, &tmp, elem);
                    if (ierr > 0) {
                        if (! cmp.inRange(tmp)) {
                            hits.setBit(j, 1);
                        }
                    }
                }
                ibis::fileManager::instance().recordPages
                    (elem*ii[0], elem*ii[idx.nIndices()-1]);
            }

            ++idx; // next set of selected entries
        } // while (idx.nIndices() > 0)
    }

    if (uncomp)
        hits.compress();
    else if (hits.size() < mask.size())
        hits.adjustSize(0, mask.size());

    if (ierr >= 0) {
        if (ibis::gVerbose > 3) {
            timer.stop();
            ierr = hits.cnt();
            ibis::util::logger lg;
            lg() << "part::negativeCompare -- comparison with column "
                 << cmp.colName() << " on " << mask.cnt() << ' '
                 << typeid(T).name() << "s from file \"" << file << "\" took "
                 << timer.realTime() << " sec elapsed time and produced "
                 << ierr << " hit" << (ierr>1?"s":"");
#if DEBUG+0 > 1 || _DEBUG+0 > 1
            lg() << "\nmask\n" << mask << "\nhit vector\n" << hits << "\n";
#endif
        }
        else {
            ierr = hits.sloppyCount();
        }
    }
    return ierr;
} // ibis::part::negativeCompare

/// The function that performs the actual comparison for range queries.
/// The size of array may either match the number of bits in @c mask or the
/// number of set bits in @c mask.  This allows one to either use the whole
/// array or the only the elements need for this operation.  In either
/// case, only mask.cnt() elements of array are checked but position of the
/// bits that need to be set in the output bitvector @c hits have to be
/// handled differently.
template <typename T>
long ibis::part::doCompare(const array_t<T> &array,
                           const ibis::qIntHod &cmp,
                           const ibis::bitvector &mask,
                           ibis::bitvector &hits) {
    ibis::horometer timer;
    if (ibis::gVerbose > 3) timer.start(); // start the timer

    long ierr = 0;
    uint32_t i=0, j=0;
    const bool uncomp = ((mask.size() >> 8) < mask.cnt());
    if (uncomp) { // use uncompressed hits internally
        hits.set(0, mask.size());
        hits.decompress();
    }
    else {
        hits.clear();
        hits.reserve(mask.size(), mask.cnt());
    }

    ibis::bitvector::indexSet idx = mask.firstIndexSet();
    if (array.size() == mask.size()) { // full array available
        while (idx.nIndices() > 0) { // the outer loop
            const ibis::bitvector::word_t *ii = idx.indices();
            if (idx.isRange()) {
                for (j = *ii; j < ii[1]; ++j) {
                    if (cmp.inRange((int64_t)array[j])) {
                        hits.setBit(j, 1);
                        ++ ierr;
#if DEBUG+0 > 1 || _DEBUG+0 > 1
                        LOGGER(ibis::gVerbose >= 0)
                            << "DEBUG -- doCompare" << array[j] << " is in "
                            << cmp;
#endif
                    }
                }
            }
            else {
                for (i = 0; i < idx.nIndices(); ++i) {
                    j = ii[i];
                    if (cmp.inRange((int64_t)array[j])) {
                        hits.setBit(j, 1);
                        ++ ierr;
#if DEBUG+0 > 1 || _DEBUG+0 > 1
                        LOGGER(ibis::gVerbose >= 0)
                            << "DEBUG -- doCompare " << array[j] << " is in "
                            << cmp;
#endif
                    }
                }
            }

            ++idx; // next set of selected entries
        }
    }
    else if (array.size() == mask.cnt()) { // packed array available
        while (idx.nIndices() > 0) {
            const ibis::bitvector::word_t *ii = idx.indices();
            if (idx.isRange()) {
                for (unsigned k = *ii; k < ii[1]; ++ k) {
                    if (cmp.inRange((int64_t)array[j++])) {
                        hits.setBit(k, 1);
                        ++ ierr;
#if DEBUG+0 > 1 || _DEBUG+0 > 1
                        LOGGER(ibis::gVerbose >= 0)
                            << "DEBUG -- doCompare" << array[j] << " is in "
                            << cmp << ", setting position " << k
                            << " of hit vector to 1";
#endif
                    }
                }
            }
            else {
                for (uint32_t k = 0; k < idx.nIndices(); ++ k) {
                    if (cmp.inRange((int64_t)array[j++])) {
                        hits.setBit(ii[k], 1);
                        ++ ierr;
#if DEBUG+0 > 1 || _DEBUG+0 > 1
                        LOGGER(ibis::gVerbose >= 0)
                            << "DEBUG -- doCompare " << array[j] << " is in "
                            << cmp << ", setting position " << ii[k]
                            << " of hit vector to 1";
#endif
                    }
                }
            }
            ++ idx;
        }
    }
    else {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- part::doCompare requires the input data array size ("
            << array.size() << ") to be either " << mask.size() << " or "
            << mask.cnt();
        ierr = -6;
    }

    if (uncomp)
        hits.compress();
    else if (hits.size() < mask.size())
        hits.adjustSize(0, mask.size());

    if (ierr >= 0 && ibis::gVerbose > 3) {
        timer.stop();
        ibis::util::logger lg;
        lg() << "part::doCompare -- comparison with column "
             << cmp.colName() << " on " << mask.cnt() << " element"
             << (mask.cnt() > 1 ? "s" : "") << " of a "
             << typeid(T).name() << "-array[" << array.size()
             << "] took " << timer.realTime()
             << " sec elapsed time and produced "
             << ierr << " hit" << (ierr>1?"s":"");
#if DEBUG+0 > 1 || _DEBUG+0 > 1
        lg() << "\nmask\n" << mask << "\nhit vector\n" << hits;
#endif
    }
    return ierr;
} // ibis::part::doCompare

/// Evaluate the range condition.  The actual comparison function is
/// only applied on rows with mask == 1.
template <typename T>
long ibis::part::doCompare(const char* file,
                           const ibis::qIntHod &cmp,
                           const ibis::bitvector &mask,
                           ibis::bitvector &hits) {
    ibis::horometer timer;
    if (ibis::gVerbose > 3)
        timer.start(); // start the timer

    int fdes = UnixOpen(file, OPEN_READONLY);
    if (fdes < 0) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- part::doCompare could not open file \""
            << file << '"';
        hits.set(0, mask.size());
        return -1;
    }
    IBIS_BLOCK_GUARD(UnixClose, fdes);
#if defined(_WIN32) && defined(_MSC_VER)
    (void)_setmode(fdes, _O_BINARY);
#endif

    const unsigned elem = sizeof(T);
    // attempt to allocate a decent sized buffer for operations
    ibis::fileManager::buffer<T> mybuf;
    uint32_t nbuf = mybuf.size();
    T *buf = mybuf.address();

    uint32_t i=0, j=0;
    long diff, ierr=0;
    const ibis::bitvector::word_t *ii;
    const bool uncomp = ((mask.size() >> 8) < mask.cnt());
    if (uncomp) { // use uncompressed hits internally
        hits.set(0, mask.size());
        hits.decompress();
    }
    else {
        hits.clear();
        hits.reserve(mask.size(), mask.cnt());
    }

    ibis::bitvector::indexSet idx = mask.firstIndexSet();

    if (buf) { // has a good size buffer to read data into
        while (idx.nIndices() > 0) { // the outer loop
            ii = idx.indices();
            if (idx.isRange()) {
                diff = *ii * elem;
                ierr = UnixSeek(fdes, diff, SEEK_SET);
                if (ierr != diff) {
                    LOGGER(ibis::gVerbose > 0)
                        << "part::doCompare(" << file
                        << ") could not seek to " << diff;
                    hits.clear();
                    return -2;
                }
                ibis::fileManager::instance().recordPages
                    (diff, elem*ii[1]);
                j = ii[1];
                for (i = *ii; i < ii[1]; i += diff) {
                    diff = nbuf;
                    if (i+diff > ii[1])
                        diff = ii[1] - i;
                    ierr = UnixRead(fdes, buf, elem*diff) / elem;
                    if (diff == ierr && ierr >= 0) {
                        j = ierr;
                    }
                    else {
                        j = 0;
                        LOGGER(ibis::gVerbose > 0)
                            << "Warning -- part::doCompare expected to read "
                            << diff << " values from \"" << file
                            << "\" but got only " << ierr;
                    }
                    for (uint32_t k = 0; k < j; ++k) {
                        if (cmp.inRange((int64_t)buf[k])) {
                            hits.setBit(i+k, 1);
                        }
                    }
                }
            }
            else if (idx.nIndices() > 1) {
                diff = ii[idx.nIndices()-1] - *ii + 1;
                if (static_cast<uint32_t>(diff) < nbuf) {
                    ierr = UnixSeek(fdes, *ii * elem, SEEK_SET);
                    if (ierr != static_cast<long>(*ii * elem)) {
                        LOGGER(ibis::gVerbose > 0)
                            << "Warning -- part::doCompare(" << file
                            << ") could not seek to " << *ii *elem;
                        hits.clear();
                        return -3;
                    }
                    ierr = UnixRead(fdes, buf, elem*diff) / elem;
                    if (diff == ierr && ierr >= 0) {
                        j = idx.nIndices();
                    }
                    else {
                        j = 0;
                        LOGGER(ibis::gVerbose > 0)
                            << "Warning -- part::doCompare expected to read "
                            << diff << " values from \"" << file
                            << "\" but got only " << ierr;
                    }
                    for (i = 0; i < j; ++i) {
                        uint32_t k0 = ii[i] - *ii;
                        if (cmp.inRange((int64_t)buf[k0])) {
                            hits.setBit(ii[i], 1);
                        }
                    }
                }
                else if (diff > 0) { // read one element at a time
                    for (i = 0; i < idx.nIndices(); ++i) {
                        j = ii[i];
                        diff = j * elem;
                        ierr = UnixSeek(fdes, diff, SEEK_SET);
                        if (ierr != diff) {
                            LOGGER(ibis::gVerbose > 0)
                                << "Warning -- part::doCompare(" << file
                                << ") could not seek to " << diff;
                            hits.clear();
                            return -4;
                        }
                        ierr = UnixRead(fdes, buf, elem);
                        if (ierr > 0) {
                            if (cmp.inRange((int64_t)*buf)) {
                                hits.setBit(j, 1);
                            }
                        }
                        else {
                            LOGGER(ibis::gVerbose > 0)
                                << "Warning -- part::doCompare(" << file
                                << ") could not read a value at " << diff;
                        }
                    }
                }
                ibis::fileManager::instance().recordPages
                    (elem*ii[0], elem*ii[idx.nIndices()-1]);
            }
            else { // read a single value
                j = *ii;
                diff = j * elem;
                ierr = UnixSeek(fdes, diff, SEEK_SET);
                if (ierr != diff) {
                    LOGGER(ibis::gVerbose > 0)
                        << "Warning -- part::doCompare(" << file
                        << ") could not seek to " << diff;
                    hits.clear();
                    return -4;
                }
                ierr = UnixRead(fdes, buf, elem);
                if (ierr > 0) {
                    ibis::fileManager::instance().recordPages(diff, diff+elem);
                    if (cmp.inRange((int64_t)*buf)) {
                        hits.setBit(j, 1);
                    }
                }
                else {
                    LOGGER(ibis::gVerbose > 0)
                        << "Warning -- part::doCompare(" << file
                        << ") could not read a value at " << diff;
                }
            }

            ++idx; // next set of selected entries
        } // while (idx.nIndices() > 0)
    }
    else { // no user buffer to use, read a single value at a time
        T tmp;
        while (idx.nIndices() > 0) { // the outer loop
            ii = idx.indices();
            if (idx.isRange()) {
                diff = *ii * elem;
                ierr = UnixSeek(fdes, diff, SEEK_SET);
                if (ierr != diff) {
                    LOGGER(ibis::gVerbose > 0)
                        << "Warning -- part::doCompare(" << file
                        << ") could not seek to " << diff;
                    hits.clear();
                    return -5;
                }

                ibis::fileManager::instance().recordPages(diff, elem*ii[1]);
                j = ii[1];
                for (i = *ii; i < j; ++i) {
                    ierr = UnixRead(fdes, &tmp, elem);
                    if (ierr > 0 && cmp.inRange((int64_t)tmp)) {
                        hits.setBit(i, 1);
                    }
                }
            }
            else {
                for (i = 0; i < idx.nIndices(); ++i) {
                    j = ii[1];
                    diff = j * elem;
                    ierr = UnixSeek(fdes, diff, SEEK_SET);
                    if (ierr != diff) {
                        LOGGER(ibis::gVerbose > 0)
                            << "Warning -- part::doCompare(" << file
                            << ") could not seek to " << diff;
                        hits.clear();
                        return -6;
                    }

                    ierr = UnixRead(fdes, &tmp, elem);
                    if (ierr > 0 && cmp.inRange((int64_t)tmp)) {
                        hits.setBit(j, 1);
                    }
                }
                ibis::fileManager::instance().recordPages
                    (elem*ii[0], elem*ii[idx.nIndices()-1]);
            }

            ++idx; // next set of selected entries
        } // while (idx.nIndices() > 0)
    }

    if (uncomp)
        hits.compress();
    else if (hits.size() < mask.size())
        hits.adjustSize(0, mask.size());

    if (ibis::gVerbose > 3 && ierr >= 0) {
        timer.stop();
        ierr = hits.cnt();
        ibis::util::logger lg;
        lg() << "part::doCompare -- comparison with column "
             << cmp.colName() << " on " << mask.cnt() << " element"
             << (mask.cnt() > 1 ? "s" : "") << " of " << typeid(T).name()
             << " from file \"" << file << "\" took "
             << timer.realTime() << " sec elapsed time and produced "
             << ierr << " hit" << (ierr>1?"s":"");
#if DEBUG+0 > 1 || _DEBUG+0 > 1
        lg() << "\nmask\n" << mask << "\nhit vector\n" << hits << "\n";
#endif
    }
    else if (ierr >= 0) {
        ierr = hits.sloppyCount();
    }
    return ierr;
} // ibis::part::doCompare

/// Perform the negative comparison.  Hits are those don't satisfy the
/// range conditions, however, the comparisons are only performed on
/// those rows with mask == 1.
template <typename T>
long ibis::part::negativeCompare(const array_t<T> &array,
                                 const ibis::qIntHod &cmp,
                                 const ibis::bitvector &mask,
                                 ibis::bitvector &hits) {
    ibis::horometer timer;
    if (ibis::gVerbose > 3) timer.start(); // start the timer

    uint32_t i=0, j=0;
    long ierr=0;
    const uint32_t nelm =
        (array.size() <= mask.size() ? array.size() : mask.size());
    const bool uncomp = ((mask.size() >> 8) < mask.cnt());
    if (uncomp) { // use uncompressed hits internally
        hits.set(0, mask.size());
        hits.decompress();
    }
    else {
        hits.clear();
        hits.reserve(mask.size(), mask.cnt());
    }

    ibis::bitvector::indexSet idx = mask.firstIndexSet();
    while (idx.nIndices() > 0) { // the outer loop
        const ibis::bitvector::word_t *ii = idx.indices();
        if (idx.isRange()) {
            uint32_t diff = (ii[1] <= nelm ? ii[1] : nelm);
            for (j = *ii; j < diff; ++j) {
                if (! cmp.inRange((int64_t)array[j])) {
                    hits.setBit(j, 1);
                    ++ ierr;
#if DEBUG+0 > 1 || _DEBUG+0 > 1
                    LOGGER(ibis::gVerbose >= 0)
                        << "DEBUG -- negativeCompare " << array[j]
                        << " is not in " << cmp;
#endif
                }
            }
        }
        else {
            for (i = 0; i < idx.nIndices(); ++i) {
                j = ii[i];
                if (j < nelm && ! cmp.inRange((int64_t)array[j])) {
                    hits.setBit(j, 1);
                    ++ ierr;
#if DEBUG+0 > 1 || _DEBUG+0 > 1
                    LOGGER(ibis::gVerbose >= 0)
                        << "DEBUG -- negativeCompare " << array[j]
                        << " is no in " << cmp;
#endif
                }
            }
        }

        ++idx;
    }

    if (uncomp)
        hits.compress();
    else if (hits.size() < mask.size())
        hits.adjustSize(0, mask.size());

    if (ierr >= 0 && ibis::gVerbose > 3) {
        timer.stop();
        ibis::util::logger lg;
        lg() << "part::negativeCompare -- comparison with column "
             << cmp.colName() << " on " << mask.cnt()<< " element"
             << (mask.cnt() > 1 ? "s" : "") << " of a "
             << typeid(T).name() << "-array[" << array.size()
             << "] took " << timer.realTime()
             << " sec elapsed time and produced "
             << ierr << " hit" << (ierr>1?"s":"");
#if DEBUG+0 > 1 || _DEBUG+0 > 1
        lg() << "\nmask\n" << mask << "\nhit vector\n" << hits << "\n";
#endif
    }
    return ierr;
} // ibis::part::negativeCompare

/// Perform the negative comparison.  Hits are those don't satisfy the
/// range conditions, however, the comparisons are only performed on
/// those rows with mask == 1.
template <typename T>
long ibis::part::negativeCompare(const char* file,
                                 const ibis::qIntHod &cmp,
                                 const ibis::bitvector &mask,
                                 ibis::bitvector &hits) {
    ibis::horometer timer;
    if (ibis::gVerbose > 3)
        timer.start(); // start the timer

    hits.clear(); // clear the existing content
    int fdes = UnixOpen(file, OPEN_READONLY);
    if (fdes < 0) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- part::negativeCompare could not open file \""
            << file << '"';
        hits.set(0, mask.size());
        return -1;
    }
    IBIS_BLOCK_GUARD(UnixClose, fdes);
#if defined(_WIN32) && defined(_MSC_VER)
    (void)_setmode(fdes, _O_BINARY);
#endif

    const unsigned elem = sizeof(T);
    // attempt to allocate a decent size buffer for operations
    ibis::fileManager::buffer<T> mybuf;
    uint32_t nbuf = mybuf.size();
    T *buf = mybuf.address();

    uint32_t i=0, j=0;
    long diff, ierr=0;
    const ibis::bitvector::word_t *ii;
    const bool uncomp = ((mask.size() >> 8) < mask.cnt());
    if (uncomp) { // use uncompressed hits internally
        hits.set(0, mask.size());
        hits.decompress();
    }
    else {
        hits.clear();
        hits.reserve(mask.size(), mask.cnt());
    }

    ibis::bitvector::indexSet idx = mask.firstIndexSet();

    if (buf) { // has a good size buffer to read data into
        while (idx.nIndices() > 0) { // the outer loop
            ii = idx.indices();
            if (idx.isRange()) {
                diff = *ii * elem;
                ierr = UnixSeek(fdes, diff, SEEK_SET);
                if (ierr != diff) {
                    LOGGER(ibis::gVerbose > 0)
                        << "Warning -- part::negativeCompare(" << file
                        << ") could not seek to " << diff;
                    hits.clear();
                    return -3;
                }

                ibis::fileManager::instance().recordPages
                    (diff, elem*ii[1]);
                j = ii[1];
                for (i = *ii; i < ii[1]; i += diff) {
                    diff = nbuf;
                    if (i+diff > ii[1])
                        diff = ii[1] - i;
                    ierr = UnixRead(fdes, buf, elem*diff) / elem;
                    if (ierr > 0 && static_cast<int32_t>(diff) == diff) {
                        j = diff;
                    }
                    else {
                        j = 0;
                        LOGGER(ibis::gVerbose > 0)
                            << "Warning -- part::negativeCompare expected "
                            "to read " << diff << " values from \""
                            << file << "\", but got only " << ierr;
                    }
                    for (uint32_t k = 0; k < j; ++k) {
                        if (! cmp.inRange((int64_t)buf[k])) {
                            hits.setBit(i+k, 1);
                        }
                    }
                }
            }
            else if (idx.nIndices() > 1) {
                j = idx.nIndices() - 1;
                diff = ii[j] - *ii + 1;
                if (static_cast<uint32_t>(diff) < nbuf) {
                    ierr = UnixSeek(fdes, *ii * elem, SEEK_SET);
                    if (ierr != static_cast<long>(*ii * elem)) {
                        LOGGER(ibis::gVerbose > 0)
                            << "Warning -- part::negativeCompare(" << file
                            << ") could not seek to " << *ii *elem;
                        hits.clear();
                        return -4;
                    }

                    ierr = UnixRead(fdes, buf, elem*diff) / elem;
                    if (ierr > 0 && ierr == diff) {
                        j = diff;
                    }
                    else {
                        j = 0;
                        LOGGER(ibis::gVerbose > 0)
                            << "Warning -- part::negativeCompare(" << file
                            << ") expected to read " << diff
                            << " elements of " << elem << "-byte each, "
                            << "but got " << ierr;
                    }
                    for (i = 0; i < j; ++i) {
                        uint32_t k0 = ii[i] - *ii;
                        if (! cmp.inRange((int64_t)buf[k0])) {
                            hits.setBit(ii[i], 1);
                        }
                    }
                }
                else {
                    for (i = 0; i < idx.nIndices(); ++i) {
                        j = ii[i];
                        diff = j * elem;
                        ierr = UnixSeek(fdes, diff, SEEK_SET);
                        if (ierr != diff) {
                            LOGGER(ibis::gVerbose > 0)
                                << "Warning -- part::negativeCompare(" << file
                                << ") could not seek to " << diff;
                            hits.clear();
                            return -5;
                        }
                        ierr = UnixRead(fdes, buf, elem);
                        if (ierr > 0) {
                            if (! cmp.inRange((int64_t)*buf)) {
                                hits.setBit(j, 1);
                            }
                        }
                    }
                }
                ibis::fileManager::instance().recordPages
                    (elem*ii[0], elem*ii[idx.nIndices()-1]);
            }
            else {
                j = *ii;
                diff = j * elem;
                ierr = UnixSeek(fdes, diff, SEEK_SET);
                if (ierr != diff) {
                    LOGGER(ibis::gVerbose > 0)
                        << "Warning -- part::negativeCompare(" << file
                        << ") could not seek to " << diff;
                    hits.clear();
                    return -6;
                }
                ierr = UnixRead(fdes, buf, elem);
                if (ierr > 0) {
                    ibis::fileManager::instance().recordPages(diff, diff+elem);
                    if (! cmp.inRange((int64_t)*buf)) {
                        hits.setBit(j, 1);
                    }
                }
            }

            ++idx; // next set of selected entries
        } // while (idx.nIndices() > 0)
    }
    else { // no user buffer to use, read one element at a time
        T tmp;
        while (idx.nIndices() > 0) { // the outer loop
            ii = idx.indices();
            if (idx.isRange()) {
                diff = *ii * elem;
                ierr = UnixSeek(fdes, diff, SEEK_SET);
                if (ierr != diff) {
                    LOGGER(ibis::gVerbose > 0)
                        << "Warning -- part::negativeCompare(" << file
                        << ") could not seek to " << diff;
                    hits.clear();
                    return -7;
                }
                ibis::fileManager::instance().recordPages(diff, elem*ii[1]);
                j = ii[1];
                for (i = *ii; i < j; ++i) {
                    ierr = UnixRead(fdes, &tmp, elem);
                    if (ierr > 0) {
                        if (! cmp.inRange((int64_t)tmp)) {
                            hits.setBit(i, 1);
                        }
                    }
                }
            }
            else {
                for (i = 0; i < idx.nIndices(); ++i) {
                    j = ii[1];
                    diff = j * elem;
                    ierr = UnixSeek(fdes, diff, SEEK_SET);
                    if (ierr != diff) {
                        LOGGER(ibis::gVerbose > 0)
                            << "Warning -- part::negativeCompare(" << file
                            << ") could not seek to " << diff;
                        hits.clear();
                        return -8;
                    }
                    ierr = UnixRead(fdes, &tmp, elem);
                    if (ierr > 0) {
                        if (! cmp.inRange((int64_t)tmp)) {
                            hits.setBit(j, 1);
                        }
                    }
                }
                ibis::fileManager::instance().recordPages
                    (elem*ii[0], elem*ii[idx.nIndices()-1]);
            }

            ++idx; // next set of selected entries
        } // while (idx.nIndices() > 0)
    }

    if (uncomp)
        hits.compress();
    else if (hits.size() < mask.size())
        hits.adjustSize(0, mask.size());

    if (ierr >= 0) {
        if (ibis::gVerbose > 3) {
            timer.stop();
            ierr = hits.cnt();
            ibis::util::logger lg;
            lg() << "part::negativeCompare -- comparison with column "
                << cmp.colName() << " on " << mask.cnt() << ' '
                 << typeid(T).name() << "s from file \"" << file << "\" took "
                 << timer.realTime() << " sec elapsed time and produced "
                 << ierr << " hit" << (ierr>1?"s":"");
#if DEBUG+0 > 1 || _DEBUG+0 > 1
            lg() << "\nmask\n" << mask << "\nhit vector\n" << hits << "\n";
#endif
        }
        else {
            ierr = hits.sloppyCount();
        }
    }
    return ierr;
} // ibis::part::negativeCompare

/// The function that performs the actual comparison for range queries.
/// The size of array may either match the number of bits in @c mask or the
/// number of set bits in @c mask.  This allows one to either use the whole
/// array or the only the elements need for this operation.  In either
/// case, only mask.cnt() elements of array are checked but position of the
/// bits that need to be set in the output bitvector @c hits have to be
/// handled differently.
template <typename T>
long ibis::part::doCompare(const array_t<T> &array,
                           const ibis::qUIntHod &cmp,
                           const ibis::bitvector &mask,
                           ibis::bitvector &hits) {
    ibis::horometer timer;
    if (ibis::gVerbose > 3) timer.start(); // start the timer

    long ierr = 0;
    uint32_t i=0, j=0;
    const bool uncomp = ((mask.size() >> 8) < mask.cnt());
    if (uncomp) { // use uncompressed hits internally
        hits.set(0, mask.size());
        hits.decompress();
    }
    else {
        hits.clear();
        hits.reserve(mask.size(), mask.cnt());
    }

    ibis::bitvector::indexSet idx = mask.firstIndexSet();
    if (array.size() == mask.size()) { // full array available
        while (idx.nIndices() > 0) { // the outer loop
            const ibis::bitvector::word_t *ii = idx.indices();
            if (idx.isRange()) {
                for (j = *ii; j < ii[1]; ++j) {
                    if (cmp.inRange((uint64_t)array[j])) {
                        hits.setBit(j, 1);
                        ++ ierr;
#if DEBUG+0 > 1 || _DEBUG+0 > 1
                        LOGGER(ibis::gVerbose >= 0)
                            << "DEBUG -- doCompare" << array[j] << " is in "
                            << cmp;
#endif
                    }
                }
            }
            else {
                for (i = 0; i < idx.nIndices(); ++i) {
                    j = ii[i];
                    if (cmp.inRange((uint64_t)array[j])) {
                        hits.setBit(j, 1);
                        ++ ierr;
#if DEBUG+0 > 1 || _DEBUG+0 > 1
                        LOGGER(ibis::gVerbose >= 0)
                            << "DEBUG -- doCompare " << array[j] << " is in "
                            << cmp;
#endif
                    }
                }
            }

            ++idx; // next set of selected entries
        }
    }
    else if (array.size() == mask.cnt()) { // packed array available
        while (idx.nIndices() > 0) {
            const ibis::bitvector::word_t *ii = idx.indices();
            if (idx.isRange()) {
                for (unsigned k = *ii; k < ii[1]; ++ k) {
                    if (cmp.inRange((uint64_t)array[j++])) {
                        hits.setBit(k, 1);
                        ++ ierr;
#if DEBUG+0 > 1 || _DEBUG+0 > 1
                        LOGGER(ibis::gVerbose >= 0)
                            << "DEBUG -- doCompare" << array[j] << " is in "
                            << cmp << ", setting position " << k
                            << " of hit vector to 1";
#endif
                    }
                }
            }
            else {
                for (uint32_t k = 0; k < idx.nIndices(); ++ k) {
                    if (cmp.inRange((uint64_t)array[j++])) {
                        hits.setBit(ii[k], 1);
                        ++ ierr;
#if DEBUG+0 > 1 || _DEBUG+0 > 1
                        LOGGER(ibis::gVerbose >= 0)
                            << "DEBUG -- doCompare " << array[j] << " is in "
                            << cmp << ", setting position " << ii[k]
                            << " of hit vector to 1";
#endif
                    }
                }
            }
            ++ idx;
        }
    }
    else {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- part::doCompare requires the input data array size ("
            << array.size() << ") to be either " << mask.size() << " or "
            << mask.cnt();
        ierr = -6;
    }

    if (uncomp)
        hits.compress();
    else if (hits.size() < mask.size())
        hits.adjustSize(0, mask.size());

    if (ierr >= 0 && ibis::gVerbose > 3) {
        timer.stop();
        ibis::util::logger lg;
        lg() << "part::doCompare -- comparison with column "
             << cmp.colName() << " on " << mask.cnt() << " element"
             << (mask.cnt() > 1 ? "s" : "") << " of a "
             << typeid(T).name() << "-array[" << array.size()
             << "] took " << timer.realTime()
             << " sec elapsed time and produced "
             << ierr << " hit" << (ierr>1?"s":"");
#if DEBUG+0 > 1 || _DEBUG+0 > 1
        lg() << "\nmask\n" << mask << "\nhit vector\n" << hits;
#endif
    }
    return ierr;
} // ibis::part::doCompare

template <typename T>
long ibis::part::doCompare(const char* file,
                           const ibis::qUIntHod &cmp,
                           const ibis::bitvector &mask,
                           ibis::bitvector &hits) {
    ibis::horometer timer;
    if (ibis::gVerbose > 3)
        timer.start(); // start the timer

    int fdes = UnixOpen(file, OPEN_READONLY);
    if (fdes < 0) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- part::doCompare could not open file \""
            << file << '"';
        hits.set(0, mask.size());
        return -1;
    }
    IBIS_BLOCK_GUARD(UnixClose, fdes);
#if defined(_WIN32) && defined(_MSC_VER)
    (void)_setmode(fdes, _O_BINARY);
#endif

    const unsigned elem = sizeof(T);
    // attempt to allocate a decent sized buffer for operations
    ibis::fileManager::buffer<T> mybuf;
    uint32_t nbuf = mybuf.size();
    T *buf = mybuf.address();

    uint32_t i=0, j=0;
    long diff, ierr=0;
    const ibis::bitvector::word_t *ii;
    const bool uncomp = ((mask.size() >> 8) < mask.cnt());
    if (uncomp) { // use uncompressed hits internally
        hits.set(0, mask.size());
        hits.decompress();
    }
    else {
        hits.clear();
        hits.reserve(mask.size(), mask.cnt());
    }

    ibis::bitvector::indexSet idx = mask.firstIndexSet();

    if (buf) { // has a good size buffer to read data into
        while (idx.nIndices() > 0) { // the outer loop
            ii = idx.indices();
            if (idx.isRange()) {
                diff = *ii * elem;
                ierr = UnixSeek(fdes, diff, SEEK_SET);
                if (ierr != diff) {
                    LOGGER(ibis::gVerbose > 0)
                        << "part::doCompare(" << file
                        << ") could not seek to " << diff;
                    hits.clear();
                    return -2;
                }
                ibis::fileManager::instance().recordPages
                    (diff, elem*ii[1]);
                j = ii[1];
                for (i = *ii; i < ii[1]; i += diff) {
                    diff = nbuf;
                    if (i+diff > ii[1])
                        diff = ii[1] - i;
                    ierr = UnixRead(fdes, buf, elem*diff) / elem;
                    if (diff == ierr && ierr >= 0) {
                        j = ierr;
                    }
                    else {
                        j = 0;
                        LOGGER(ibis::gVerbose > 0)
                            << "Warning -- part::doCompare expected to read "
                            << diff <<  " values from \"" << file
                            << "\" but got only " << ierr;
                    }
                    for (uint32_t k = 0; k < j; ++k) {
                        if (cmp.inRange((uint64_t)buf[k])) {
                            hits.setBit(i+k, 1);
                        }
                    }
                }
            }
            else if (idx.nIndices() > 1) {
                diff = ii[idx.nIndices()-1] - *ii + 1;
                if (static_cast<uint32_t>(diff) < nbuf) {
                    ierr = UnixSeek(fdes, *ii * elem, SEEK_SET);
                    if (ierr != static_cast<long>(*ii * elem)) {
                        LOGGER(ibis::gVerbose > 0)
                            << "Warning -- part::doCompare(" << file
                            << ") could not seek to " << *ii *elem;
                        hits.clear();
                        return -3;
                    }
                    ierr = UnixRead(fdes, buf, elem*diff) / elem;
                    if (diff == ierr && ierr >= 0) {
                        j = idx.nIndices();
                    }
                    else {
                        j = 0;
                        LOGGER(ibis::gVerbose > 0)
                            << "Warning -- part::doCompare expected to read "
                            << diff << " values from \"" << file
                            << "\" but got only " << ierr;
                    }
                    for (i = 0; i < j; ++i) {
                        uint32_t k0 = ii[i] - *ii;
                        if (cmp.inRange((uint64_t)buf[k0])) {
                            hits.setBit(ii[i], 1);
                        }
                    }
                }
                else if (diff > 0) { // read one element at a time
                    for (i = 0; i < idx.nIndices(); ++i) {
                        j = ii[i];
                        diff = j * elem;
                        ierr = UnixSeek(fdes, diff, SEEK_SET);
                        if (ierr != diff) {
                            LOGGER(ibis::gVerbose > 0)
                                << "Warning -- part::doCompare(" << file
                                << ") could not seek to " << diff;
                            hits.clear();
                            return -4;
                        }
                        ierr = UnixRead(fdes, buf, elem);
                        if (ierr > 0) {
                            if (cmp.inRange((uint64_t)(*buf))) {
                                hits.setBit(j, 1);
                            }
                        }
                        else {
                            LOGGER(ibis::gVerbose > 0)
                                << "Warning -- part::doCompare(" << file
                                << ") could not read a value at " << diff;
                        }
                    }
                }
                ibis::fileManager::instance().recordPages
                    (elem*ii[0], elem*ii[idx.nIndices()-1]);
            }
            else { // read a single value
                j = *ii;
                diff = j * elem;
                ierr = UnixSeek(fdes, diff, SEEK_SET);
                if (ierr != diff) {
                    LOGGER(ibis::gVerbose > 0)
                        << "Warning -- part::doCompare(" << file
                        << ") could not seek to " << diff;
                    hits.clear();
                    return -4;
                }
                ierr = UnixRead(fdes, buf, elem);
                if (ierr > 0) {
                    ibis::fileManager::instance().recordPages(diff, diff+elem);
                    if (cmp.inRange((uint64_t)(*buf))) {
                        hits.setBit(j, 1);
                    }
                }
                else {
                    LOGGER(ibis::gVerbose > 0)
                        << "Warning -- part::doCompare(" << file
                        << ") could not read a value at " << diff;
                }
            }

            ++idx; // next set of selected entries
        } // while (idx.nIndices() > 0)
    }
    else { // no user buffer to use, read a single value at a time
        T tmp;
        while (idx.nIndices() > 0) { // the outer loop
            ii = idx.indices();
            if (idx.isRange()) {
                diff = *ii * elem;
                ierr = UnixSeek(fdes, diff, SEEK_SET);
                if (ierr != diff) {
                    LOGGER(ibis::gVerbose > 0)
                        << "Warning -- part::doCompare(" << file
                        << ") could not seek to " << diff;
                    hits.clear();
                    return -5;
                }

                ibis::fileManager::instance().recordPages(diff, elem*ii[1]);
                j = ii[1];
                for (i = *ii; i < j; ++i) {
                    ierr = UnixRead(fdes, &tmp, elem);
                    if (ierr > 0 && cmp.inRange((uint64_t)tmp)) {
                        hits.setBit(i, 1);
                    }
                }
            }
            else {
                for (i = 0; i < idx.nIndices(); ++i) {
                    j = ii[1];
                    diff = j * elem;
                    ierr = UnixSeek(fdes, diff, SEEK_SET);
                    if (ierr != diff) {
                        LOGGER(ibis::gVerbose > 0)
                            << "Warning -- part::doCompare(" << file
                            << ") could not seek to " << diff;
                        hits.clear();
                        return -6;
                    }

                    ierr = UnixRead(fdes, &tmp, elem);
                    if (ierr > 0 && cmp.inRange((uint64_t)tmp)) {
                        hits.setBit(j, 1);
                    }
                }
                ibis::fileManager::instance().recordPages
                    (elem*ii[0], elem*ii[idx.nIndices()-1]);
            }

            ++idx; // next set of selected entries
        } // while (idx.nIndices() > 0)
    }

    if (uncomp)
        hits.compress();
    else if (hits.size() < mask.size())
        hits.adjustSize(0, mask.size());

    if (ierr >= 0) {
        if (ibis::gVerbose > 3) {
            timer.stop();
            ierr = hits.cnt();
            ibis::util::logger lg;
            lg() << "part::doCompare -- comparison with column "
                 << cmp.colName() << " on " << mask.cnt() << " element"
                 << (mask.cnt() > 1 ? "s" : "") << " of " << typeid(T).name()
                 << " from file \"" << file << "\" took "
                 << timer.realTime() << " sec elapsed time and produced "
                 << ierr << " hit" << (ierr>1?"s":"");
#if DEBUG+0 > 1 || _DEBUG+0 > 1
            lg() << "\nmask\n" << mask << "\nhit vector\n" << hits << "\n";
#endif
        }
        else {
            ierr = hits.sloppyCount();
        }
    }
    return ierr;
} // ibis::part::doCompare

// hits are those do not satisfy the speficied range condition
template <typename T>
long ibis::part::negativeCompare(const array_t<T> &array,
                                 const ibis::qUIntHod &cmp,
                                 const ibis::bitvector &mask,
                                 ibis::bitvector &hits) {
    ibis::horometer timer;
    if (ibis::gVerbose > 3) timer.start(); // start the timer

    uint32_t i=0, j=0;
    long ierr=0;
    const uint32_t nelm = (array.size() <= mask.size() ?
                           array.size() : mask.size());
    const bool uncomp = ((mask.size() >> 8) < mask.cnt());
    if (uncomp) { // use uncompressed hits internally
        hits.set(0, mask.size());
        hits.decompress();
    }
    else {
        hits.clear();
        hits.reserve(mask.size(), mask.cnt());
    }

    ibis::bitvector::indexSet idx = mask.firstIndexSet();
    while (idx.nIndices() > 0) { // the outer loop
        const ibis::bitvector::word_t *ii = idx.indices();
        if (idx.isRange()) {
            uint32_t diff = (ii[1] <= nelm ? ii[1] : nelm);
            for (j = *ii; j < diff; ++j) {
                if (! cmp.inRange((uint64_t)array[j])) {
                    hits.setBit(j, 1);
                    ++ ierr;
#if DEBUG+0 > 1 || _DEBUG+0 > 1
                    LOGGER(ibis::gVerbose >= 0)
                        << "DEBUG -- negativeCompare " << array[j]
                        << " is not in " << cmp;
#endif
                }
            }
        }
        else {
            for (i = 0; i < idx.nIndices(); ++i) {
                j = ii[i];
                if (j < nelm && ! cmp.inRange((uint64_t)array[j])) {
                    hits.setBit(j, 1);
                    ++ ierr;
#if DEBUG+0 > 1 || _DEBUG+0 > 1
                    LOGGER(ibis::gVerbose >= 0)
                        << "DEBUG -- negativeCompare " << array[j]
                        << " is no in " << cmp;
#endif
                }
            }
        }

        ++idx;
    }

    if (uncomp)
        hits.compress();
    else if (hits.size() < mask.size())
        hits.adjustSize(0, mask.size());

    if (ibis::gVerbose > 3 && ierr >= 0) {
        timer.stop();
        ibis::util::logger lg;
        lg() << "part::negativeCompare -- comparison with column "
             << cmp.colName() << " on " << mask.cnt()<< " element"
             << (mask.cnt() > 1 ? "s" : "") << " of a "
             << typeid(T).name() << "-array[" << array.size()
             << "] took " << timer.realTime()
             << " sec elapsed time and produced "
             << ierr << " hit" << (ierr>1?"s":"");
#if DEBUG+0 > 1 || _DEBUG+0 > 1
        lg() << "\nmask\n" << mask << "\nhit vector\n" << hits << "\n";
#endif
    }
    return ierr;
} // ibis::part::negativeCompare

template <typename T>
long ibis::part::negativeCompare(const char* file,
                                 const ibis::qUIntHod &cmp,
                                 const ibis::bitvector &mask,
                                 ibis::bitvector &hits) {
    ibis::horometer timer;
    if (ibis::gVerbose > 3)
        timer.start(); // start the timer

    hits.clear(); // clear the existing content
    int fdes = UnixOpen(file, OPEN_READONLY);
    if (fdes < 0) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- part::negativeCompare could not open file \""
            << file << '"';
        hits.set(0, mask.size());
        return -1;
    }
    IBIS_BLOCK_GUARD(UnixClose, fdes);
#if defined(_WIN32) && defined(_MSC_VER)
    (void)_setmode(fdes, _O_BINARY);
#endif

    const unsigned elem = sizeof(T);
    // attempt to allocate a decent size buffer for operations
    ibis::fileManager::buffer<T> mybuf;
    uint32_t nbuf = mybuf.size();
    T *buf = mybuf.address();

    uint32_t i=0, j=0;
    long diff, ierr=0;
    const ibis::bitvector::word_t *ii;
    const bool uncomp = ((mask.size() >> 8) < mask.cnt());
    if (uncomp) { // use uncompressed hits internally
        hits.set(0, mask.size());
        hits.decompress();
    }
    else {
        hits.clear();
        hits.reserve(mask.size(), mask.cnt());
    }

    ibis::bitvector::indexSet idx = mask.firstIndexSet();

    if (buf) { // has a good size buffer to read data into
        while (idx.nIndices() > 0) { // the outer loop
            ii = idx.indices();
            if (idx.isRange()) {
                diff = *ii * elem;
                ierr = UnixSeek(fdes, diff, SEEK_SET);
                if (ierr != diff) {
                    LOGGER(ibis::gVerbose > 0)
                        << "Warning -- part::negativeCompare(" << file
                        << ") could not seek to " << diff;
                    hits.clear();
                    return -3;
                }

                ibis::fileManager::instance().recordPages
                    (diff, elem*ii[1]);
                j = ii[1];
                for (i = *ii; i < ii[1]; i += diff) {
                    diff = nbuf;
                    if (i+diff > ii[1])
                        diff = ii[1] - i;
                    ierr = UnixRead(fdes, buf, elem*diff) / elem;
                    if (ierr > 0 && static_cast<int32_t>(diff) == diff) {
                        j = diff;
                    }
                    else {
                        j = 0;
                        LOGGER(ibis::gVerbose > 0)
                            << "Warning -- part::negativeCompare expected "
                            "to read " << diff << " values from \""
                            << file << "\" but got only " << ierr;
                    }
                    for (uint32_t k = 0; k < j; ++k) {
                        if (! cmp.inRange((uint64_t)buf[k])) {
                            hits.setBit(i+k, 1);
                        }
                    }
                }
            }
            else if (idx.nIndices() > 1) {
                j = idx.nIndices() - 1;
                diff = ii[j] - *ii + 1;
                if (static_cast<uint32_t>(diff) < nbuf) {
                    ierr = UnixSeek(fdes, *ii * elem, SEEK_SET);
                    if (ierr != static_cast<long>(*ii * elem)) {
                        LOGGER(ibis::gVerbose > 0)
                            << "Warning -- part::negativeCompare(" << file
                            << ") could not seek to " << *ii *elem;
                        hits.clear();
                        return -4;
                    }

                    ierr = UnixRead(fdes, buf, elem*diff) / elem;
                    if (ierr > 0 && ierr == diff) {
                        j = diff;
                    }
                    else {
                        j = 0;
                        LOGGER(ibis::gVerbose > 0)
                            << "Warning -- part::negativeCompare(" << file
                            << ") expected to read " << diff
                            << " elements of " << elem << "-byte each, "
                            << "but got " << ierr;
                    }
                    for (i = 0; i < j; ++i) {
                        uint32_t k0 = ii[i] - *ii;
                        if (! cmp.inRange((uint64_t)buf[k0])) {
                            hits.setBit(ii[i], 1);
                        }
                    }
                }
                else {
                    for (i = 0; i < idx.nIndices(); ++i) {
                        j = ii[i];
                        diff = j * elem;
                        ierr = UnixSeek(fdes, diff, SEEK_SET);
                        if (ierr != diff) {
                            LOGGER(ibis::gVerbose > 0)
                                << "Warning -- part::negativeCompare(" << file
                                << ") could not seek to " << diff;
                            hits.clear();
                            return -5;
                        }
                        ierr = UnixRead(fdes, buf, elem);
                        if (ierr > 0) {
                            if (! cmp.inRange((uint64_t)(*buf))) {
                                hits.setBit(j, 1);
                            }
                        }
                    }
                }
                ibis::fileManager::instance().recordPages
                    (elem*ii[0], elem*ii[idx.nIndices()-1]);
            }
            else {
                j = *ii;
                diff = j * elem;
                ierr = UnixSeek(fdes, diff, SEEK_SET);
                if (ierr != diff) {
                    LOGGER(ibis::gVerbose > 0)
                        << "Warning -- part::negativeCompare(" << file
                        << ") could not seek to " << diff;
                    hits.clear();
                    return -6;
                }
                ierr = UnixRead(fdes, buf, elem);
                if (ierr > 0) {
                    ibis::fileManager::instance().recordPages(diff, diff+elem);
                    if (! cmp.inRange((uint64_t)(*buf))) {
                        hits.setBit(j, 1);
                    }
                }
            }

            ++idx; // next set of selected entries
        } // while (idx.nIndices() > 0)
    }
    else { // no user buffer to use, read one element at a time
        T tmp;
        while (idx.nIndices() > 0) { // the outer loop
            ii = idx.indices();
            if (idx.isRange()) {
                diff = *ii * elem;
                ierr = UnixSeek(fdes, diff, SEEK_SET);
                if (ierr != diff) {
                    LOGGER(ibis::gVerbose > 0)
                        << "Warning -- part::negativeCompare(" << file
                        << ") could not seek to " << diff;
                    hits.clear();
                    return -7;
                }
                ibis::fileManager::instance().recordPages(diff, elem*ii[1]);
                j = ii[1];
                for (i = *ii; i < j; ++i) {
                    ierr = UnixRead(fdes, &tmp, elem);
                    if (ierr > 0) {
                        if (! cmp.inRange((uint64_t)tmp)) {
                            hits.setBit(i, 1);
                        }
                    }
                }
            }
            else {
                for (i = 0; i < idx.nIndices(); ++i) {
                    j = ii[1];
                    diff = j * elem;
                    ierr = UnixSeek(fdes, diff, SEEK_SET);
                    if (ierr != diff) {
                        LOGGER(ibis::gVerbose > 0)
                            << "Warning -- part::negativeCompare(" << file
                            << ") could not seek to " << diff;
                        hits.clear();
                        return -8;
                    }
                    ierr = UnixRead(fdes, &tmp, elem);
                    if (ierr > 0) {
                        if (! cmp.inRange((uint64_t)tmp)) {
                            hits.setBit(j, 1);
                        }
                    }
                }
                ibis::fileManager::instance().recordPages
                    (elem*ii[0], elem*ii[idx.nIndices()-1]);
            }

            ++idx; // next set of selected entries
        } // while (idx.nIndices() > 0)
    }

    if (uncomp)
        hits.compress();
    else if (hits.size() < mask.size())
        hits.adjustSize(0, mask.size());

    if (ierr >= 0) {
        if (ibis::gVerbose > 3) {
            timer.stop();
            ierr = hits.cnt();
            ibis::util::logger lg;
            lg() << "part::negativeCompare -- comparison with column "
                 << cmp.colName() << " on " << mask.cnt() << ' '
                 << typeid(T).name() << "s from file \"" << file << "\" took "
                 << timer.realTime() << " sec elapsed time and produced "
                 << ierr << " hit" << (ierr>1?"s":"");
#if DEBUG+0 > 1 || _DEBUG+0 > 1
            lg() << "\nmask\n" << mask << "\nhit vector\n" << hits << "\n";
#endif
        }
        else {
            ierr = hits.sloppyCount();
        }
    }
    return ierr;
} // ibis::part::negativeCompare

/// Evalue the range condition on the in memory values.
/// This static member function works on an array that is provided by the
/// caller.  Since the values are provided, this function does not check
/// the name of the variable involved in the range condition.
template <typename T>
long ibis::part::doScan(const array_t<T> &vals,
                        const ibis::qContinuousRange &rng,
                        const ibis::bitvector &mask,
                        ibis::bitvector &hits) {
    long ierr = -2;
    ibis::horometer timer;
    if (ibis::gVerbose > 3)
        timer.start();
    T leftBound, rightBound;
    ibis::qExpr::COMPARE lop = rng.leftOperator();
    ibis::qExpr::COMPARE rop = rng.rightOperator();
    if (lop == ibis::qExpr::OP_UNDEFINED &&
        rop == ibis::qExpr::OP_UNDEFINED) {
        hits.copy(mask);
        return hits.sloppyCount();
    }

    switch (rng.leftOperator()) {
    case ibis::qExpr::OP_UNDEFINED:
        leftBound = 0;
        break;
    case ibis::qExpr::OP_LT:
        if (rng.leftBound() < std::numeric_limits<T>::min()) {
            leftBound = std::numeric_limits<T>::min();
            lop = ibis::qExpr::OP_LE;
        }
        else if (rng.leftBound() > std::numeric_limits<T>::max()) {
            leftBound = std::numeric_limits<T>::max();
        }
        else {
            leftBound = (T) rng.leftBound();
        }
        break;
    case ibis::qExpr::OP_LE:
        if (rng.leftBound() < std::numeric_limits<T>::min()) {
            leftBound = std::numeric_limits<T>::min();
            lop = ibis::qExpr::OP_LE;
        }
        else if (rng.leftBound() > std::numeric_limits<T>::max()) {
            leftBound = std::numeric_limits<T>::max();
            lop = ibis::qExpr::OP_LT;
        }
        else {
            leftBound = (T) rng.leftBound();
            if (leftBound != rng.leftBound())
                lop = ibis::qExpr::OP_LT;
        }
        break;
    case ibis::qExpr::OP_GE:
        if (rng.leftBound() < std::numeric_limits<T>::min()) {
            leftBound = std::numeric_limits<T>::min();
            lop = ibis::qExpr::OP_GT;
        }
        else if (rng.leftBound() > std::numeric_limits<T>::max()) {
            leftBound = std::numeric_limits<T>::max();
        }
        else {
            leftBound = (T) rng.leftBound();
        }
        break;
    case ibis::qExpr::OP_GT:
        if (rng.leftBound() < std::numeric_limits<T>::min()) {
            leftBound = std::numeric_limits<T>::min();
        }
        else if (rng.leftBound() > std::numeric_limits<T>::max()) {
            leftBound = std::numeric_limits<T>::max();
            lop = ibis::qExpr::OP_GE;
        }
        else {
            leftBound = (T) rng.leftBound();
            if (leftBound != rng.leftBound())
                lop = ibis::qExpr::OP_GE;
        }
        break;
    default:
        ibis::util::round_down(rng.leftBound(), leftBound);
        break;
    }
    switch (rng.rightOperator()) {
    case ibis::qExpr::OP_UNDEFINED:
        rightBound = 0;
        break;
    case ibis::qExpr::OP_LE:
        if (rng.rightBound() < std::numeric_limits<T>::min()) {
            rightBound = std::numeric_limits<T>::min();
            rop = ibis::qExpr::OP_LT;
        }
        else if (rng.rightBound() > std::numeric_limits<T>::max()) {
            rightBound = std::numeric_limits<T>::max();
        }
        else {
            rightBound = (T) rng.rightBound();
        }
        break;
    case ibis::qExpr::OP_LT:
        if (rng.rightBound() < std::numeric_limits<T>::min()) {
            rightBound = std::numeric_limits<T>::min();
        }
        else if (rng.rightBound() > std::numeric_limits<T>::max()) {
            rightBound = std::numeric_limits<T>::max();
            rop = ibis::qExpr::OP_LE;
        }
        else {
            rightBound = (T) rng.rightBound();
            if (rightBound < rng.rightBound())
                rop = ibis::qExpr::OP_LE;
        }
        break;
    case ibis::qExpr::OP_GE:
        if (rng.rightBound() < std::numeric_limits<T>::min()) {
            rightBound = std::numeric_limits<T>::min();
        }
        else if (rng.rightBound() > std::numeric_limits<T>::max()) {
            rightBound = std::numeric_limits<T>::max();
            rop = ibis::qExpr::OP_GT;
        }
        else {
            rightBound = (T) rng.rightBound();
            if (rightBound < rng.rightBound())
                rop = ibis::qExpr::OP_GT;
        }
        break;
    case ibis::qExpr::OP_GT:
        if (rng.rightBound() < std::numeric_limits<T>::min()) {
            rightBound = std::numeric_limits<T>::min();
            rop = ibis::qExpr::OP_GE;
        }
        else if (rng.rightBound() > std::numeric_limits<T>::max()) {
            rightBound = std::numeric_limits<T>::max();
        }
        else {
            rightBound = (T) rng.rightBound();
        }
        break;
    default:
        ibis::util::round_down(rng.rightBound(), rightBound);
        break;
    }

    const bool uncomp = ((mask.size() >> 8) < mask.cnt());
    switch (lop) {
    case ibis::qExpr::OP_LT: {
        switch (rop) {
        case ibis::qExpr::OP_LT: {
            if (leftBound < rightBound) {
                if (uncomp)
                    ierr = doComp0
                        (vals,
                         std::bind
                         (std::less<T>(), leftBound, std::placeholders::_1),
                         // std::bind1st<std::less<T> >
                         // (std::less<T>(), leftBound),
                         std::bind
                         (std::less<T>(), std::placeholders::_1, rightBound),
                         // std::bind2nd<std::less<T> >
                         // (std::less<T>(), rightBound),
                         mask, hits);
                else
                    ierr = doComp
                        (vals,
                         std::bind1st<std::less<T> >
                         (std::less<T>(), leftBound),
                         std::bind2nd<std::less<T> >
                         (std::less<T>(), rightBound),
                         mask, hits);
            }
            else {
                hits.set(0, mask.size());
                ierr = 0;
            }
            break;}
        case ibis::qExpr::OP_LE: {
            if (leftBound < rightBound) {
                if (uncomp)
                    ierr = doComp0
                        (vals,
                         std::bind1st<std::less<T> >
                         (std::less<T>(), leftBound),
                         std::bind2nd<std::less_equal<T> >
                         (std::less_equal<T>(), rightBound),
                         mask, hits);
                else
                    ierr = doComp
                        (vals,
                         std::bind1st<std::less<T> >
                         (std::less<T>(), leftBound),
                         std::bind2nd<std::less_equal<T> >
                         (std::less_equal<T>(), rightBound),
                         mask, hits);
            }
            else {
                hits.set(0, mask.size());
                ierr = 0;
            }
            break;}
        case ibis::qExpr::OP_GT: {
            if (leftBound >= rightBound) {
                if (uncomp)
                    ierr = doComp0
                        (vals,
                         std::bind1st<std::less<T> >
                         (std::less<T>(), leftBound),
                         mask, hits);
                else
                    ierr = doComp
                        (vals,
                         std::bind1st<std::less<T> >
                         (std::less<T>(), leftBound),
                         mask, hits);
            }
            else {
                if (uncomp)
                    ierr = doComp0
                        (vals,
                         std::bind2nd<std::greater<T> >
                         (std::greater<T>(), rightBound),
                         mask, hits);
                else
                    ierr = doComp
                        (vals,
                         std::bind2nd<std::greater<T> >
                         (std::greater<T>(), rightBound),
                         mask, hits);
            }
            break;}
        case ibis::qExpr::OP_GE: {
            if (leftBound >= rightBound) {
                if (uncomp)
                    ierr = doComp0
                        (vals,
                         std::bind1st<std::less<T> >
                         (std::less<T>(), leftBound),
                         mask, hits);
                else
                    ierr = doComp
                        (vals,
                         std::bind1st<std::less<T> >
                         (std::less<T>(), leftBound),
                         mask, hits);
            }
            else {
                if (uncomp)
                    ierr = doComp0
                        (vals,
                         std::bind2nd<std::greater_equal<T> >
                         (std::greater_equal<T>(), rightBound),
                         mask, hits);
                else
                    ierr = doComp
                        (vals,
                         std::bind2nd<std::greater_equal<T> >
                         (std::greater_equal<T>(), rightBound),
                         mask, hits);
            }
            break;}
        case ibis::qExpr::OP_EQ: {
            if (rightBound == rng.rightBound() && leftBound < rightBound) {
                if (uncomp)
                    ierr = doComp0
                        (vals,
                         std::bind2nd<std::equal_to<T> >
                         (std::equal_to<T>(), rightBound),
                         mask, hits);
                else
                    ierr = doComp
                        (vals,
                         std::bind2nd<std::equal_to<T> >
                         (std::equal_to<T>(), rightBound),
                         mask, hits);
            }
            else {
                hits.set(0, mask.size());
                ierr = 0;
            }
            break;}
        default: {
            if (uncomp)
                ierr = doComp0
                    (vals,
                     std::bind1st<std::less<T> >
                     (std::less<T>(), leftBound),
                     mask, hits);
            else
                ierr = doComp
                    (vals,
                     std::bind1st<std::less<T> >
                     (std::less<T>(), leftBound),
                     mask, hits);
            break;}
        }
        break;}
    case ibis::qExpr::OP_LE: {
        switch (rop) {
        case ibis::qExpr::OP_LT: {
            if (leftBound < rightBound) {
                if (uncomp)
                    ierr = doComp0
                        (vals,
                         std::bind1st<std::less_equal<T> >
                         (std::less_equal<T>(), leftBound),
                         std::bind2nd<std::less<T> >
                         (std::less<T>(), rightBound),
                         mask, hits);
                else
                    ierr = doComp
                        (vals,
                         std::bind1st<std::less_equal<T> >
                         (std::less_equal<T>(), leftBound),
                         std::bind2nd<std::less<T> >
                         (std::less<T>(), rightBound),
                         mask, hits);
            }
            else {
                hits.set(0, mask.size());
                ierr = 0;
            }
            break;}
        case ibis::qExpr::OP_LE: {
            if (leftBound <= rightBound) {
                if (uncomp)
                    ierr = doComp0
                        (vals,
                         std::bind1st<std::less_equal<T> >
                         (std::less_equal<T>(), leftBound),
                         std::bind2nd<std::less_equal<T> >
                         (std::less_equal<T>(), rightBound),
                         mask, hits);
                else
                    ierr = doComp
                        (vals,
                         std::bind1st<std::less_equal<T> >
                         (std::less_equal<T>(), leftBound),
                         std::bind2nd<std::less_equal<T> >
                         (std::less_equal<T>(), rightBound),
                         mask, hits);
            }
            else {
                hits.set(0, mask.size());
                ierr = 0;
            }
            break;}
        case ibis::qExpr::OP_GT: {
            if (leftBound > rightBound) {
                if (uncomp)
                    ierr = doComp0
                        (vals,
                         std::bind1st<std::less_equal<T> >
                         (std::less_equal<T>(), leftBound),
                         mask, hits);
                else
                    ierr = doComp
                        (vals,
                         std::bind1st<std::less_equal<T> >
                         (std::less_equal<T>(), leftBound),
                         mask, hits);
            }
            else {
                if (uncomp)
                    ierr = doComp0
                        (vals,
                         std::bind2nd<std::greater<T> >
                         (std::greater<T>(), rightBound),
                         mask, hits);
                else
                    ierr = doComp
                        (vals,
                         std::bind2nd<std::greater<T> >
                         (std::greater<T>(), rightBound),
                         mask, hits);
            }
            break;}
        case ibis::qExpr::OP_GE: {
            if (leftBound >= rightBound) {
                if (uncomp)
                    ierr = doComp0
                        (vals,
                         std::bind1st<std::less_equal<T> >
                         (std::less_equal<T>(), leftBound),
                         mask, hits);
                else
                    ierr = doComp
                        (vals,
                         std::bind1st<std::less_equal<T> >
                         (std::less_equal<T>(), leftBound),
                         mask, hits);
            }
            else {
                if (uncomp)
                    ierr = doComp0
                        (vals,
                         std::bind2nd<std::greater_equal<T> >
                         (std::greater_equal<T>(), rightBound),
                         mask, hits);
                else
                    ierr = doComp
                        (vals,
                         std::bind2nd<std::greater_equal<T> >
                         (std::greater_equal<T>(), rightBound),
                         mask, hits);
            }
            break;}
        case ibis::qExpr::OP_EQ: {
            if (rightBound == rng.rightBound() && leftBound <= rightBound) {
                if (uncomp)
                    ierr = doComp0
                        (vals,
                         std::bind2nd<std::equal_to<T> >
                         (std::equal_to<T>(), rightBound),
                         mask, hits);
                else
                    ierr = doComp
                        (vals,
                         std::bind2nd<std::equal_to<T> >
                         (std::equal_to<T>(), rightBound),
                         mask, hits);
            }
            else {
                hits.set(0, mask.size());
                ierr = 0;
            }
            break;}
        default: {
            if (uncomp)
                ierr = doComp0
                    (vals,
                     std::bind1st<std::less_equal<T> >
                     (std::less_equal<T>(), leftBound),
                     mask, hits);
            else
                ierr = doComp
                    (vals,
                     std::bind1st<std::less_equal<T> >
                     (std::less_equal<T>(), leftBound),
                     mask, hits);
            break;}
        }
        break;}
    case ibis::qExpr::OP_GT: {
        switch (rop) {
        case ibis::qExpr::OP_LT: {
            if (leftBound <= rightBound) {
                if (uncomp)
                    ierr = doComp0
                        (vals,
                         std::bind1st< std::greater<T> >
                         (std::greater<T>(), leftBound),
                         mask, hits);
                else
                    ierr = doComp
                        (vals,
                         std::bind1st< std::greater<T> >
                         (std::greater<T>(), leftBound),
                         mask, hits);
            }
            else {
                if (uncomp)
                    ierr = doComp0
                        (vals,
                         std::bind2nd<std::less<T> >
                         (std::less<T>(), rightBound),
                         mask, hits);
                else
                    ierr = doComp
                        (vals,
                         std::bind2nd<std::less<T> >
                         (std::less<T>(), rightBound),
                         mask, hits);
            }
            break;}
        case ibis::qExpr::OP_LE: {
            if (leftBound < rightBound) {
                if (uncomp)
                    ierr = doComp0
                        (vals,
                         std::bind1st< std::greater<T> >
                         (std::greater<T>(), leftBound),
                         mask, hits);
                else
                    ierr = doComp
                        (vals,
                         std::bind1st< std::greater<T> >
                         (std::greater<T>(), leftBound),
                         mask, hits);
            }
            else {
                if (uncomp)
                    ierr = doComp0
                        (vals,
                         std::bind2nd<std::less_equal<T> >
                         (std::less_equal<T>(), rightBound),
                         mask, hits);
                else
                    ierr = doComp
                        (vals,
                         std::bind2nd<std::less_equal<T> >
                         (std::less_equal<T>(), rightBound),
                         mask, hits);
            }
            break;}
        case ibis::qExpr::OP_GT: {
            if (leftBound > rightBound) {
                if (uncomp)
                    ierr = doComp0
                        (vals,
                         std::bind1st< std::greater<T> >
                         (std::greater<T>(), leftBound),
                         std::bind2nd<std::greater<T> >
                         (std::greater<T>(), rightBound),
                         mask, hits);
                else
                    ierr = doComp
                        (vals,
                         std::bind1st< std::greater<T> >
                         (std::greater<T>(), leftBound),
                         std::bind2nd<std::greater<T> >
                         (std::greater<T>(), rightBound),
                         mask, hits);
            }
            else {
                hits.set(0, mask.size());
                ierr = 0;
            }
            break;}
        case ibis::qExpr::OP_GE: {
            if (leftBound > rightBound) {
                if (uncomp)
                    ierr = doComp0
                        (vals,
                         std::bind1st< std::greater<T> >
                         (std::greater<T>(), leftBound),
                         std::bind2nd<std::greater_equal<T> >
                         (std::greater_equal<T>(), rightBound),
                         mask, hits);
                else
                    ierr = doComp
                        (vals,
                         std::bind1st< std::greater<T> >
                         (std::greater<T>(), leftBound),
                         std::bind2nd<std::greater_equal<T> >
                         (std::greater_equal<T>(), rightBound),
                         mask, hits);
            }
            else {
                hits.set(0, mask.size());
                ierr = 0;
            }
            break;}
        case ibis::qExpr::OP_EQ: {
            if (rightBound == rng.rightBound() && rightBound < leftBound) {
                if (uncomp)
                    ierr = doComp0
                        (vals,
                         std::bind2nd<std::equal_to<T> >
                         (std::equal_to<T>(), rightBound),
                         mask, hits);
                else
                    ierr = doComp
                        (vals,
                         std::bind2nd<std::equal_to<T> >
                         (std::equal_to<T>(), rightBound),
                         mask, hits);
            }
            else {
                hits.set(0, mask.size());
                ierr = 0;
            }
            break;}
        default: {
            if (uncomp)
                ierr = doComp0
                    (vals,
                     std::bind1st< std::greater<T> >
                     (std::greater<T>(), leftBound),
                     mask, hits);
            else
                ierr = doComp
                    (vals,
                     std::bind1st< std::greater<T> >
                     (std::greater<T>(), leftBound),
                     mask, hits);
            break;}
        }
        break;}
    case ibis::qExpr::OP_GE: {
        switch (rop) {
        case ibis::qExpr::OP_LT: {
            if (leftBound < rightBound) {
                if (uncomp)
                    ierr = doComp0
                        (vals,
                         std::bind1st< std::greater_equal<T> >
                         (std::greater_equal<T>(), leftBound),
                         mask, hits);
                else
                    ierr = doComp
                        (vals,
                         std::bind1st< std::greater_equal<T> >
                         (std::greater_equal<T>(), leftBound),
                         mask, hits);
            }
            else {
                if (uncomp)
                    ierr = doComp0
                        (vals,
                         std::bind2nd<std::less<T> >
                         (std::less<T>(), rightBound),
                         mask, hits);
                else
                    ierr = doComp
                        (vals,
                         std::bind2nd<std::less<T> >
                         (std::less<T>(), rightBound),
                         mask, hits);
            }
            break;}
        case ibis::qExpr::OP_LE: {
            if (leftBound <= rightBound) {
                if (uncomp)
                    ierr = doComp0
                        (vals,
                         std::bind1st< std::greater_equal<T> >
                         (std::greater_equal<T>(), leftBound),
                         mask, hits);
                else
                    ierr = doComp
                        (vals,
                         std::bind1st< std::greater_equal<T> >
                         (std::greater_equal<T>(), leftBound),
                         mask, hits);
            }
            else {
                if (uncomp)
                    ierr = doComp0
                        (vals,
                         std::bind2nd<std::less_equal<T> >
                         (std::less_equal<T>(), rightBound),
                         mask, hits);
                else
                    ierr = doComp
                        (vals,
                         std::bind2nd<std::less_equal<T> >
                         (std::less_equal<T>(), rightBound),
                         mask, hits);
            }
            break;}
        case ibis::qExpr::OP_GT: {
            if (leftBound > rightBound) {
                if (uncomp)
                    ierr = doComp0
                        (vals,
                         std::bind1st< std::greater_equal<T> >
                         (std::greater_equal<T>(), leftBound),
                         std::bind2nd<std::greater<T> >
                         (std::greater<T>(), rightBound),
                         mask, hits);
                else
                    ierr = doComp
                        (vals,
                         std::bind1st< std::greater_equal<T> >
                         (std::greater_equal<T>(), leftBound),
                         std::bind2nd<std::greater<T> >
                         (std::greater<T>(), rightBound),
                         mask, hits);
            }
            else {
                hits.set(0, mask.size());
                ierr = 0;
            }
            break;}
        case ibis::qExpr::OP_GE: {
            if (leftBound >= rightBound) {
                if (uncomp)
                    ierr = doComp0
                        (vals,
                         std::bind1st< std::greater_equal<T> >
                         (std::greater_equal<T>(), leftBound),
                         std::bind2nd<std::greater_equal<T> >
                         (std::greater_equal<T>(), rightBound),
                         mask, hits);
                else
                    ierr = doComp
                        (vals,
                         std::bind1st< std::greater_equal<T> >
                         (std::greater_equal<T>(), leftBound),
                         std::bind2nd<std::greater_equal<T> >
                         (std::greater_equal<T>(), rightBound),
                         mask, hits);
            }
            else {
                hits.set(0, mask.size());
                ierr = 0;
            }
            break;}
        case ibis::qExpr::OP_EQ: {
            if (rightBound == rng.rightBound() && rightBound <= leftBound) {
                if (uncomp)
                    ierr = doComp0
                        (vals,
                         std::bind1st< std::greater_equal<T> >
                         (std::greater_equal<T>(), leftBound),
                         std::bind2nd<std::equal_to<T> >
                         (std::equal_to<T>(), rightBound),
                         mask, hits);
                else
                    ierr = doComp
                        (vals,
                         std::bind1st< std::greater_equal<T> >
                         (std::greater_equal<T>(), leftBound),
                         std::bind2nd<std::equal_to<T> >
                         (std::equal_to<T>(), rightBound),
                         mask, hits);
            }
            else {
                hits.set(0, mask.size());
                ierr = 0;
            }
            break;}
        default: {
            if (uncomp)
                ierr = doComp0
                    (vals,
                     std::bind1st< std::greater_equal<T> >
                     (std::greater_equal<T>(), leftBound),
                     mask, hits);
            else
                ierr = doComp
                    (vals,
                     std::bind1st< std::greater_equal<T> >
                     (std::greater_equal<T>(), leftBound),
                     mask, hits);
            break;}
        }
        break;}
    case ibis::qExpr::OP_EQ: {
        if (leftBound == rng.leftBound()) {
            switch (rop) {
            case ibis::qExpr::OP_LT: {
                if (leftBound < rightBound) {
                    if (uncomp)
                        ierr = doComp0
                            (vals,
                             std::bind1st< std::equal_to<T> >
                             (std::equal_to<T>(), leftBound),
                             mask, hits);
                    else
                        ierr = doComp
                            (vals,
                             std::bind1st< std::equal_to<T> >
                             (std::equal_to<T>(), leftBound),
                             mask, hits);
                }
                else {
                    hits.set(0, mask.size());
                    ierr = 0;
                }
                break;}
            case ibis::qExpr::OP_LE: {
                if (leftBound <= rightBound) {
                    if (uncomp)
                        ierr = doComp0
                            (vals,
                             std::bind1st< std::equal_to<T> >
                             (std::equal_to<T>(), leftBound),
                             mask, hits);
                    else
                        ierr = doComp
                            (vals,
                             std::bind1st< std::equal_to<T> >
                             (std::equal_to<T>(), leftBound),
                             mask, hits);
                }
                else {
                    hits.set(0, mask.size());
                    ierr = 0;
                }
                break;}
            case ibis::qExpr::OP_GT: {
                if (leftBound > rightBound) {
                    if (uncomp)
                        ierr = doComp0
                            (vals,
                             std::bind1st< std::equal_to<T> >
                             (std::equal_to<T>(), leftBound),
                             mask, hits);
                    else
                        ierr = doComp
                            (vals,
                             std::bind1st< std::equal_to<T> >
                             (std::equal_to<T>(), leftBound),
                             mask, hits);
                }
                else {
                    hits.set(0, mask.size());
                    ierr = 0;
                }
                break;}
            case ibis::qExpr::OP_GE: {
                if (leftBound >= rightBound) {
                    if (uncomp)
                        ierr = doComp0
                            (vals,
                             std::bind1st< std::equal_to<T> >
                             (std::equal_to<T>(), leftBound),
                             mask, hits);
                    else
                        ierr = doComp
                            (vals,
                             std::bind1st< std::equal_to<T> >
                             (std::equal_to<T>(), leftBound),
                             mask, hits);
                }
                else {
                    hits.set(0, mask.size());
                    ierr = 0;
                }
                break;}
            case ibis::qExpr::OP_EQ: {
                if (leftBound == rightBound && rightBound == rng.rightBound()) {
                    if (uncomp)
                        ierr = doComp0
                            (vals,
                             std::bind1st< std::equal_to<T> >
                             (std::equal_to<T>(), leftBound),
                             mask, hits);
                    else
                        ierr = doComp
                            (vals,
                             std::bind1st< std::equal_to<T> >
                             (std::equal_to<T>(), leftBound),
                             mask, hits);
                }
                else {
                    hits.set(0, mask.size());
                    ierr = 0;
                }
                break;}
            default: {
                if (uncomp)
                    ierr = doComp0
                        (vals,
                         std::bind1st< std::equal_to<T> >
                         (std::equal_to<T>(), leftBound),
                         mask, hits);
                else
                    ierr = doComp
                        (vals,
                         std::bind1st< std::equal_to<T> >
                         (std::equal_to<T>(), leftBound),
                         mask, hits);
                break;}
            }
        }
        else {
            hits.set(0, mask.size());
            ierr = 0;
        }
        break;}
    default: {
        switch (rop) {
        case ibis::qExpr::OP_LT: {
            if (uncomp)
                ierr = doComp0
                    (vals,
                     std::bind2nd<std::less<T> >
                     (std::less<T>(), rightBound),
                     mask, hits);
            else
                ierr = doComp
                    (vals,
                     std::bind2nd<std::less<T> >
                     (std::less<T>(), rightBound),
                     mask, hits);
            break;}
        case ibis::qExpr::OP_LE: {
            if (uncomp)
                ierr = doComp0
                    (vals,
                     std::bind2nd<std::less_equal<T> >
                     (std::less_equal<T>(), rightBound),
                     mask, hits);
            else
                ierr = doComp
                    (vals,
                     std::bind2nd<std::less_equal<T> >
                     (std::less_equal<T>(), rightBound),
                     mask, hits);
            break;}
        case ibis::qExpr::OP_GT: {
            if (uncomp)
                ierr = doComp0
                    (vals,
                     std::bind2nd<std::greater<T> >
                     (std::greater<T>(), rightBound),
                     mask, hits);
            else
                ierr = doComp
                    (vals,
                     std::bind2nd<std::greater<T> >
                     (std::greater<T>(), rightBound),
                     mask, hits);
            break;}
        case ibis::qExpr::OP_GE: {
            if (uncomp)
                ierr = doComp0
                    (vals,
                     std::bind2nd<std::greater_equal<T> >
                     (std::greater_equal<T>(), rightBound),
                     mask, hits);
            else
                ierr = doComp
                    (vals,
                     std::bind2nd<std::greater_equal<T> >
                     (std::greater_equal<T>(), rightBound),
                     mask, hits);
            break;}
        case ibis::qExpr::OP_EQ: {
            if (rightBound == rng.rightBound()) {
                if (uncomp)
                    ierr = doComp0
                        (vals,
                         std::bind2nd<std::equal_to<T> >
                         (std::equal_to<T>(), rightBound),
                         mask, hits);
                else
                    ierr = doComp
                        (vals,
                         std::bind2nd<std::equal_to<T> >
                         (std::equal_to<T>(), rightBound),
                         mask, hits);
            }
            else {
                hits.set(0, mask.size());
                ierr = 0;
            }
            break;}
        default: {
            hits.set(0, mask.size());
            ierr = 0;
            break;}
        }
        break;}
    }

    if (ibis::gVerbose > 3) {
        timer.stop();
        ibis::util::logger lg;
        lg() << "part::doScan -- evaluating " << rng
             << " on " << mask.cnt() << " " << typeid(T).name()
             << (mask.cnt() > 1 ? " values" : " value") << " (total: "
             << mask.size() << ") took " << timer.realTime()
             << " sec elapsed time and produced " << hits.cnt()
             << (hits.cnt() > 1 ? " hits" : " hit");
#if DEBUG+0 > 1 || _DEBUG+0 > 1
        lg() << "\nmask\n" << mask;
        lg() << "\nhit vector\n" << hits << "\n";
#endif
    }
    return ierr;
} // ibis::part::doScan

/// Examine the range condition with in memory values.
/// A specialization of the template for float arrays.  All comparisons are
/// performed as doubles.
template <>
long ibis::part::doScan(const array_t<float> &vals,
                        const ibis::qContinuousRange &rng,
                        const ibis::bitvector &mask,
                        ibis::bitvector &hits) {
    long ierr = -2;
    ibis::horometer timer;
    if (ibis::gVerbose > 3)
        timer.start();
    const double leftBound = rng.leftBound();
    const double rightBound = rng.rightBound();
    const ibis::qExpr::COMPARE lop = rng.leftOperator();
    const ibis::qExpr::COMPARE rop = rng.rightOperator();
    if (lop == ibis::qExpr::OP_UNDEFINED &&
        rop == ibis::qExpr::OP_UNDEFINED) {
        hits.copy(mask);
        return hits.sloppyCount();
    }

    const bool uncomp = ((mask.size() >> 8) < mask.cnt());
    switch (lop) {
    case ibis::qExpr::OP_LT: {
        switch (rop) {
        case ibis::qExpr::OP_LT: {
            if (leftBound < rightBound) {
                if (uncomp)
                    ierr = doComp0
                        (vals,
                         std::bind1st<std::less<double> >
                         (std::less<double>(), leftBound),
                         std::bind2nd<std::less<double> >
                         (std::less<double>(), rightBound),
                         mask, hits);
                else
                    ierr = doComp
                        (vals,
                         std::bind1st<std::less<double> >
                         (std::less<double>(), leftBound),
                         std::bind2nd<std::less<double> >
                         (std::less<double>(), rightBound),
                         mask, hits);
            }
            else {
                hits.set(0, mask.size());
                ierr = 0;
            }
            break;}
        case ibis::qExpr::OP_LE: {
            if (leftBound < rightBound) {
                if (uncomp)
                    ierr = doComp0
                        (vals,
                         std::bind1st<std::less<double> >
                         (std::less<double>(), leftBound),
                         std::bind2nd<std::less_equal<double> >
                         (std::less_equal<double>(), rightBound),
                         mask, hits);
                else
                    ierr = doComp
                        (vals,
                         std::bind1st<std::less<double> >
                         (std::less<double>(), leftBound),
                         std::bind2nd<std::less_equal<double> >
                         (std::less_equal<double>(), rightBound),
                         mask, hits);
            }
            else {
                hits.set(0, mask.size());
                ierr = 0;
            }
            break;}
        case ibis::qExpr::OP_GT: {
            if (leftBound >= rightBound) {
                if (uncomp)
                    ierr = doComp0
                        (vals,
                         std::bind1st<std::less<double> >
                         (std::less<double>(), leftBound),
                         mask, hits);
                else
                    ierr = doComp(vals,
                                  std::bind1st<std::less<double> >
                                  (std::less<double>(), leftBound),
                                  mask, hits);
            }
            else {
                if (uncomp)
                    ierr = doComp0
                        (vals,
                         std::bind2nd<std::greater<double> >
                         (std::greater<double>(), rightBound),
                         mask, hits);
                else
                    ierr = doComp
                        (vals,
                         std::bind2nd<std::greater<double> >
                         (std::greater<double>(), rightBound),
                         mask, hits);
            }
            break;}
        case ibis::qExpr::OP_GE: {
            if (leftBound >= rightBound) {
                if (uncomp)
                    ierr = doComp0
                        (vals,
                         std::bind1st<std::less<double> >
                         (std::less<double>(), leftBound),
                         mask, hits);
                else
                    ierr = doComp
                        (vals,
                         std::bind1st<std::less<double> >
                         (std::less<double>(), leftBound),
                         mask, hits);
            }
            else {
                if (uncomp)
                    ierr = doComp0
                        (vals, std::bind2nd<std::greater_equal<double> >
                         (std::greater_equal<double>(), rightBound),
                         mask, hits);
                else
                    ierr = doComp
                        (vals, std::bind2nd<std::greater_equal<double> >
                         (std::greater_equal<double>(), rightBound),
                         mask, hits);
            }
            break;}
        case ibis::qExpr::OP_EQ: {
            if (leftBound < rightBound) {
                if (uncomp)
                    ierr = doComp0
                        (vals, std::bind2nd<std::equal_to<double> >
                         (std::equal_to<double>(), rightBound),
                         mask, hits);
                else
                    ierr = doComp
                        (vals, std::bind2nd<std::equal_to<double> >
                         (std::equal_to<double>(), rightBound),
                         mask, hits);
            }
            else {
                hits.set(0, mask.size());
                ierr = 0;
            }
            break;}
        default: {
            if (uncomp)
                ierr = doComp0
                    (vals, std::bind1st<std::less<double> >
                     (std::less<double>(), leftBound),
                     mask, hits);
            else
                ierr = doComp
                    (vals, std::bind1st<std::less<double> >
                     (std::less<double>(), leftBound),
                     mask, hits);
            break;}
        }
        break;}
    case ibis::qExpr::OP_LE: {
        switch (rop) {
        case ibis::qExpr::OP_LT: {
            if (leftBound < rightBound) {
                if (uncomp)
                    ierr = doComp0
                        (vals, std::bind1st<std::less_equal<double> >
                         (std::less_equal<double>(), leftBound),
                         std::bind2nd<std::less<double> >
                         (std::less<double>(), rightBound),
                         mask, hits);
                else
                    ierr = doComp
                        (vals, std::bind1st<std::less_equal<double> >
                         (std::less_equal<double>(), leftBound),
                         std::bind2nd<std::less<double> >
                         (std::less<double>(), rightBound),
                         mask, hits);
            }
            else {
                hits.set(0, mask.size());
                ierr = 0;
            }
            break;}
        case ibis::qExpr::OP_LE: {
            if (leftBound <= rightBound) {
                if (uncomp)
                    ierr = doComp0
                        (vals, std::bind1st<std::less_equal<double> >
                         (std::less_equal<double>(), leftBound),
                         std::bind2nd<std::less_equal<double> >
                         (std::less_equal<double>(), rightBound),
                         mask, hits);
                else
                    ierr = doComp
                        (vals, std::bind1st<std::less_equal<double> >
                         (std::less_equal<double>(), leftBound),
                         std::bind2nd<std::less_equal<double> >
                         (std::less_equal<double>(), rightBound),
                         mask, hits);
            }
            else {
                hits.set(0, mask.size());
                ierr = 0;
            }
            break;}
        case ibis::qExpr::OP_GT: {
            if (leftBound > rightBound) {
                if (uncomp)
                    ierr = doComp0
                        (vals, std::bind1st<std::less_equal<double> >
                         (std::less_equal<double>(), leftBound),
                         mask, hits);
                else
                    ierr = doComp
                        (vals, std::bind1st<std::less_equal<double> >
                         (std::less_equal<double>(), leftBound),
                         mask, hits);
            }
            else {
                if (uncomp)
                    ierr = doComp0
                        (vals, std::bind2nd<std::greater<double> >
                         (std::greater<double>(), rightBound),
                         mask, hits);
                else
                    ierr = doComp
                        (vals, std::bind2nd<std::greater<double> >
                         (std::greater<double>(), rightBound),
                         mask, hits);
            }
            break;}
        case ibis::qExpr::OP_GE: {
            if (leftBound >= rightBound) {
                if (uncomp)
                    ierr = doComp0
                        (vals, std::bind1st<std::less_equal<double> >
                         (std::less_equal<double>(), leftBound),
                         mask, hits);
                else
                    ierr = doComp
                        (vals, std::bind1st<std::less_equal<double> >
                         (std::less_equal<double>(), leftBound),
                         mask, hits);
            }
            else {
                if (uncomp)
                    ierr = doComp0
                        (vals, std::bind2nd<std::greater_equal<double> >
                         (std::greater_equal<double>(), rightBound),
                         mask, hits);
                else
                    ierr = doComp
                        (vals, std::bind2nd<std::greater_equal<double> >
                         (std::greater_equal<double>(), rightBound),
                         mask, hits);
            }
            break;}
        case ibis::qExpr::OP_EQ: {
            if (leftBound <= rightBound) {
                if (uncomp)
                    ierr = doComp0
                        (vals, std::bind2nd<std::equal_to<double> >
                         (std::equal_to<double>(), rightBound),
                         mask, hits);
                else
                    ierr = doComp
                        (vals, std::bind2nd<std::equal_to<double> >
                         (std::equal_to<double>(), rightBound),
                         mask, hits);
            }
            else {
                hits.set(0, mask.size());
                ierr = 0;
            }
            break;}
        default: {
            if (uncomp)
                ierr = doComp0
                    (vals, std::bind1st<std::less_equal<double> >
                     (std::less_equal<double>(), leftBound),
                     mask, hits);
            else
                ierr = doComp
                    (vals, std::bind1st<std::less_equal<double> >
                     (std::less_equal<double>(), leftBound),
                     mask, hits);
            break;}
        }
        break;}
    case ibis::qExpr::OP_GT: {
        switch (rop) {
        case ibis::qExpr::OP_LT: {
            if (leftBound <= rightBound) {
                if (uncomp)
                    ierr = doComp0
                        (vals, std::bind1st< std::greater<double> >
                         (std::greater<double>(), leftBound),
                         mask, hits);
                else
                    ierr = doComp
                        (vals, std::bind1st< std::greater<double> >
                         (std::greater<double>(), leftBound),
                         mask, hits);
            }
            else {
                if (uncomp)
                    ierr = doComp0
                        (vals, std::bind2nd<std::less<double> >
                         (std::less<double>(), rightBound),
                         mask, hits);
                else
                    ierr = doComp
                        (vals, std::bind2nd<std::less<double> >
                         (std::less<double>(), rightBound),
                         mask, hits);
            }
            break;}
        case ibis::qExpr::OP_LE: {
            if (leftBound < rightBound) {
                if (uncomp)
                    ierr = doComp0
                        (vals, std::bind1st< std::greater<double> >
                         (std::greater<double>(), leftBound),
                         mask, hits);
                else
                    ierr = doComp
                        (vals, std::bind1st< std::greater<double> >
                         (std::greater<double>(), leftBound),
                         mask, hits);
            }
            else {
                if (uncomp)
                    ierr = doComp0
                        (vals, std::bind2nd<std::less_equal<double> >
                         (std::less_equal<double>(), rightBound),
                         mask, hits);
                else
                    ierr = doComp
                        (vals, std::bind2nd<std::less_equal<double> >
                         (std::less_equal<double>(), rightBound),
                         mask, hits);
            }
            break;}
        case ibis::qExpr::OP_GT: {
            if (leftBound > rightBound) {
                if (uncomp)
                    ierr = doComp0
                        (vals, std::bind1st< std::greater<double> >
                         (std::greater<double>(), leftBound),
                         std::bind2nd<std::greater<double> >
                         (std::greater<double>(), rightBound),
                         mask, hits);
                else
                    ierr = doComp
                        (vals, std::bind1st< std::greater<double> >
                         (std::greater<double>(), leftBound),
                         std::bind2nd<std::greater<double> >
                         (std::greater<double>(), rightBound),
                         mask, hits);
            }
            else {
                hits.set(0, mask.size());
                ierr = 0;
            }
            break;}
        case ibis::qExpr::OP_GE: {
            if (leftBound > rightBound) {
                if (uncomp)
                    ierr = doComp0
                        (vals, std::bind1st< std::greater<double> >
                         (std::greater<double>(), leftBound),
                         std::bind2nd<std::greater_equal<double> >
                         (std::greater_equal<double>(), rightBound),
                         mask, hits);
                else
                    ierr = doComp
                        (vals, std::bind1st< std::greater<double> >
                         (std::greater<double>(), leftBound),
                         std::bind2nd<std::greater_equal<double> >
                         (std::greater_equal<double>(), rightBound),
                         mask, hits);
            }
            else {
                hits.set(0, mask.size());
                ierr = 0;
            }
            break;}
        case ibis::qExpr::OP_EQ: {
            if (rightBound < leftBound) {
                if (uncomp)
                    ierr = doComp0
                        (vals, std::bind2nd<std::equal_to<double> >
                         (std::equal_to<double>(), rightBound),
                         mask, hits);
                else
                    ierr = doComp
                        (vals, std::bind2nd<std::equal_to<double> >
                         (std::equal_to<double>(), rightBound),
                         mask, hits);
            }
            else {
                hits.set(0, mask.size());
                ierr = 0;
            }
            break;}
        default: {
            if (uncomp)
                ierr = doComp0
                    (vals, std::bind1st< std::greater<double> >
                     (std::greater<double>(), leftBound),
                     mask, hits);
            else
                ierr = doComp
                    (vals, std::bind1st< std::greater<double> >
                     (std::greater<double>(), leftBound),
                     mask, hits);
            break;}
        }
        break;}
    case ibis::qExpr::OP_GE: {
        switch (rop) {
        case ibis::qExpr::OP_LT: {
            if (leftBound < rightBound) {
                if (uncomp)
                    ierr = doComp0
                        (vals, std::bind1st< std::greater_equal<double> >
                         (std::greater_equal<double>(), leftBound),
                         mask, hits);
                else
                    ierr = doComp
                        (vals, std::bind1st< std::greater_equal<double> >
                         (std::greater_equal<double>(), leftBound),
                         mask, hits);
            }
            else {
                if (uncomp)
                    ierr = doComp0
                        (vals, std::bind2nd<std::less<double> >
                         (std::less<double>(), rightBound),
                         mask, hits);
                else
                    ierr = doComp
                        (vals, std::bind2nd<std::less<double> >
                         (std::less<double>(), rightBound),
                         mask, hits);
            }
            break;}
        case ibis::qExpr::OP_LE: {
            if (leftBound <= rightBound) {
                if (uncomp)
                    ierr = doComp0
                        (vals, std::bind1st< std::greater_equal<double> >
                         (std::greater_equal<double>(), leftBound),
                         mask, hits);
                else
                    ierr = doComp
                        (vals, std::bind1st< std::greater_equal<double> >
                         (std::greater_equal<double>(), leftBound),
                         mask, hits);
            }
            else {
                if (uncomp)
                    ierr = doComp0
                        (vals, std::bind2nd<std::less_equal<double> >
                         (std::less_equal<double>(), rightBound),
                         mask, hits);
                else
                    ierr = doComp
                        (vals, std::bind2nd<std::less_equal<double> >
                         (std::less_equal<double>(), rightBound),
                         mask, hits);
            }
            break;}
        case ibis::qExpr::OP_GT: {
            if (leftBound > rightBound) {
                if (uncomp)
                    ierr = doComp0
                        (vals, std::bind1st< std::greater_equal<double> >
                         (std::greater_equal<double>(), leftBound),
                         std::bind2nd<std::greater<double> >
                         (std::greater<double>(), rightBound),
                         mask, hits);
                else
                    ierr = doComp
                        (vals, std::bind1st< std::greater_equal<double> >
                         (std::greater_equal<double>(), leftBound),
                         std::bind2nd<std::greater<double> >
                         (std::greater<double>(), rightBound),
                         mask, hits);
            }
            else {
                hits.set(0, mask.size());
                ierr = 0;
            }
            break;}
        case ibis::qExpr::OP_GE: {
            if (leftBound >= rightBound) {
                if (uncomp)
                    ierr = doComp0
                        (vals, std::bind1st< std::greater_equal<double> >
                         (std::greater_equal<double>(), leftBound),
                         std::bind2nd<std::greater_equal<double> >
                         (std::greater_equal<double>(), rightBound),
                         mask, hits);
                else
                    ierr = doComp
                        (vals, std::bind1st< std::greater_equal<double> >
                         (std::greater_equal<double>(), leftBound),
                         std::bind2nd<std::greater_equal<double> >
                         (std::greater_equal<double>(), rightBound),
                         mask, hits);
            }
            else {
                hits.set(0, mask.size());
                ierr = 0;
            }
            break;}
        case ibis::qExpr::OP_EQ: {
            if (rightBound <= leftBound) {
                if (uncomp)
                    ierr = doComp0
                        (vals, std::bind1st< std::greater_equal<double> >
                         (std::greater_equal<double>(), leftBound),
                         std::bind2nd<std::equal_to<double> >
                         (std::equal_to<double>(), rightBound),
                         mask, hits);
                else
                    ierr = doComp
                        (vals, std::bind1st< std::greater_equal<double> >
                         (std::greater_equal<double>(), leftBound),
                         std::bind2nd<std::equal_to<double> >
                         (std::equal_to<double>(), rightBound),
                         mask, hits);
            }
            else {
                hits.set(0, mask.size());
                ierr = 0;
            }
            break;}
        default: {
            if (uncomp)
                ierr = doComp0
                    (vals, std::bind1st< std::greater_equal<double> >
                     (std::greater_equal<double>(), leftBound),
                     mask, hits);
            else
                ierr = doComp
                    (vals, std::bind1st< std::greater_equal<double> >
                     (std::greater_equal<double>(), leftBound),
                     mask, hits);
            break;}
        }
        break;}
    case ibis::qExpr::OP_EQ: {
        switch (rop) {
        case ibis::qExpr::OP_LT: {
            if (leftBound < rightBound) {
                if (uncomp)
                    ierr = doComp0
                        (vals, std::bind1st< std::equal_to<double> >
                         (std::equal_to<double>(), leftBound),
                         mask, hits);
                else
                    ierr = doComp
                        (vals, std::bind1st< std::equal_to<double> >
                         (std::equal_to<double>(), leftBound),
                         mask, hits);
            }
            else {
                hits.set(0, mask.size());
                ierr = 0;
            }
            break;}
        case ibis::qExpr::OP_LE: {
            if (leftBound <= rightBound) {
                if (uncomp)
                    ierr = doComp0
                        (vals, std::bind1st< std::equal_to<double> >
                         (std::equal_to<double>(), leftBound),
                         mask, hits);
                else
                    ierr = doComp
                        (vals, std::bind1st< std::equal_to<double> >
                         (std::equal_to<double>(), leftBound),
                         mask, hits);
            }
            else {
                hits.set(0, mask.size());
                ierr = 0;
            }
            break;}
        case ibis::qExpr::OP_GT: {
            if (leftBound > rightBound) {
                if (uncomp)
                    ierr = doComp0
                        (vals, std::bind1st< std::equal_to<double> >
                         (std::equal_to<double>(), leftBound),
                         mask, hits);
                else
                    ierr = doComp
                        (vals, std::bind1st< std::equal_to<double> >
                         (std::equal_to<double>(), leftBound),
                         mask, hits);
            }
            else {
                hits.set(0, mask.size());
                ierr = 0;
            }
            break;}
        case ibis::qExpr::OP_GE: {
            if (leftBound >= rightBound) {
                if (uncomp)
                    ierr = doComp0
                        (vals, std::bind1st< std::equal_to<double> >
                         (std::equal_to<double>(), leftBound),
                         mask, hits);
                else
                    ierr = doComp
                        (vals, std::bind1st< std::equal_to<double> >
                         (std::equal_to<double>(), leftBound),
                         mask, hits);
            }
            else {
                hits.set(0, mask.size());
                ierr = 0;
            }
            break;}
        case ibis::qExpr::OP_EQ: {
            if (leftBound == rightBound && rightBound == rng.rightBound()) {
                if (uncomp)
                    ierr = doComp0
                        (vals, std::bind1st< std::equal_to<double> >
                         (std::equal_to<double>(), leftBound),
                         mask, hits);
                else
                    ierr = doComp
                        (vals, std::bind1st< std::equal_to<double> >
                         (std::equal_to<double>(), leftBound),
                         mask, hits);
            }
            else {
                hits.set(0, mask.size());
                ierr = 0;
            }
            break;}
        default: {
            if (uncomp)
                ierr = doComp0
                    (vals, std::bind1st< std::equal_to<double> >
                     (std::equal_to<double>(), leftBound),
                     mask, hits);
            else
                ierr = doComp
                    (vals, std::bind1st< std::equal_to<double> >
                     (std::equal_to<double>(), leftBound),
                     mask, hits);
            break;}
        }
        break;}
    default: {
        switch (rop) {
        case ibis::qExpr::OP_LT: {
            if (uncomp)
                ierr = doComp0
                    (vals, std::bind2nd<std::less<double> >
                     (std::less<double>(), rightBound),
                     mask, hits);
            else
                ierr = doComp
                    (vals, std::bind2nd<std::less<double> >
                     (std::less<double>(), rightBound),
                     mask, hits);
            break;}
        case ibis::qExpr::OP_LE: {
            if (uncomp)
                ierr = doComp0
                    (vals, std::bind2nd<std::less_equal<double> >
                     (std::less_equal<double>(), rightBound),
                     mask, hits);
            else
                ierr = doComp
                    (vals, std::bind2nd<std::less_equal<double> >
                     (std::less_equal<double>(), rightBound),
                     mask, hits);
            break;}
        case ibis::qExpr::OP_GT: {
            if (uncomp)
                ierr = doComp0
                    (vals, std::bind2nd<std::greater<double> >
                     (std::greater<double>(), rightBound),
                     mask, hits);
            else
                ierr = doComp
                    (vals, std::bind2nd<std::greater<double> >
                     (std::greater<double>(), rightBound),
                     mask, hits);
            break;}
        case ibis::qExpr::OP_GE: {
            if (uncomp)
                ierr = doComp0
                    (vals, std::bind2nd<std::greater_equal<double> >
                     (std::greater_equal<double>(), rightBound),
                     mask, hits);
            else
                ierr = doComp
                    (vals, std::bind2nd<std::greater_equal<double> >
                     (std::greater_equal<double>(), rightBound),
                     mask, hits);
            break;}
        case ibis::qExpr::OP_EQ: {
            if (rightBound == rng.rightBound()) {
                if (uncomp)
                    ierr = doComp0
                        (vals, std::bind2nd<std::equal_to<double> >
                         (std::equal_to<double>(), rightBound),
                         mask, hits);
                else
                    ierr = doComp
                        (vals, std::bind2nd<std::equal_to<double> >
                         (std::equal_to<double>(), rightBound),
                         mask, hits);
            }
            else {
                hits.set(0, mask.size());
                ierr = 0;
            }
            break;}
        default: {
            hits.set(0, mask.size());
            ierr = 0;
            break;}
        }
        break;}
    }

    if (ibis::gVerbose > 3) {
        timer.stop();
        ibis::util::logger lg;
        lg() << "part::doScan -- evaluating " << rng
             << " on " << mask.cnt() << " float "
             << (mask.cnt() > 1 ? " values" : " value") << " (total: "
             << mask.size() << ") took " << timer.realTime()
             << " sec elapsed time and produced " << hits.cnt()
             << (hits.cnt() > 1 ? " hits" : " hit");
#if DEBUG+0 > 1 || _DEBUG+0 > 1
        lg() << "\nmask\n" << mask;
        lg() << "\nhit vector\n" << hits << "\n";
#endif
    }
    return ierr;
} // ibis::part::doScan

/// Examine the range condition with in memory values.
/// A specialization of the template for double values.
template <>
long ibis::part::doScan(const array_t<double> &vals,
                        const ibis::qContinuousRange &rng,
                        const ibis::bitvector &mask,
                        ibis::bitvector &hits) {
    long ierr = -2;
    ibis::horometer timer;
    if (ibis::gVerbose > 3)
        timer.start();
    const double leftBound = rng.leftBound();
    const double rightBound = rng.rightBound();
    const ibis::qExpr::COMPARE lop = rng.leftOperator();
    const ibis::qExpr::COMPARE rop = rng.rightOperator();
    if (lop == ibis::qExpr::OP_UNDEFINED &&
        rop == ibis::qExpr::OP_UNDEFINED) {
        hits.copy(mask);
        return hits.sloppyCount();
    }

    const bool uncomp = ((mask.size() >> 8) < mask.cnt());
    switch (lop) {
    case ibis::qExpr::OP_LT: {
        switch (rop) {
        case ibis::qExpr::OP_LT: {
            if (leftBound < rightBound) {
                if (uncomp)
                    ierr = doComp0
                        (vals, std::bind1st<std::less<double> >
                         (std::less<double>(), leftBound),
                         std::bind2nd<std::less<double> >
                         (std::less<double>(), rightBound),
                         mask, hits);
                else
                    ierr = doComp
                        (vals, std::bind1st<std::less<double> >
                         (std::less<double>(), leftBound),
                         std::bind2nd<std::less<double> >
                         (std::less<double>(), rightBound),
                         mask, hits);
            }
            else {
                hits.set(0, mask.size());
                ierr = 0;
            }
            break;}
        case ibis::qExpr::OP_LE: {
            if (leftBound < rightBound) {
                if (uncomp)
                    ierr = doComp0
                        (vals, std::bind1st<std::less<double> >
                         (std::less<double>(), leftBound),
                         std::bind2nd<std::less_equal<double> >
                         (std::less_equal<double>(), rightBound),
                         mask, hits);
                else
                    ierr = doComp
                        (vals, std::bind1st<std::less<double> >
                         (std::less<double>(), leftBound),
                         std::bind2nd<std::less_equal<double> >
                         (std::less_equal<double>(), rightBound),
                         mask, hits);
            }
            else {
                hits.set(0, mask.size());
                ierr = 0;
            }
            break;}
        case ibis::qExpr::OP_GT: {
            if (leftBound >= rightBound) {
                if (uncomp)
                    ierr = doComp0
                        (vals, std::bind1st<std::less<double> >
                         (std::less<double>(), leftBound),
                         mask, hits);
                else
                    ierr = doComp
                        (vals, std::bind1st<std::less<double> >
                         (std::less<double>(), leftBound),
                         mask, hits);
            }
            else {
                if (uncomp)
                    ierr = doComp0
                        (vals, std::bind2nd<std::greater<double> >
                         (std::greater<double>(), rightBound),
                         mask, hits);
                else
                    ierr = doComp
                        (vals, std::bind2nd<std::greater<double> >
                         (std::greater<double>(), rightBound),
                         mask, hits);
            }
            break;}
        case ibis::qExpr::OP_GE: {
            if (leftBound >= rightBound) {
                if (uncomp)
                    ierr = doComp0
                        (vals, std::bind1st<std::less<double> >
                         (std::less<double>(), leftBound),
                         mask, hits);
                else
                    ierr = doComp
                        (vals, std::bind1st<std::less<double> >
                         (std::less<double>(), leftBound),
                         mask, hits);
            }
            else {
                if (uncomp)
                    ierr = doComp0
                        (vals, std::bind2nd<std::greater_equal<double> >
                         (std::greater_equal<double>(), rightBound),
                         mask, hits);
                else
                    ierr = doComp
                        (vals, std::bind2nd<std::greater_equal<double> >
                         (std::greater_equal<double>(), rightBound),
                         mask, hits);
            }
            break;}
        case ibis::qExpr::OP_EQ: {
            if (leftBound < rightBound) {
                if (uncomp)
                    ierr = doComp0
                        (vals, std::bind2nd<std::equal_to<double> >
                         (std::equal_to<double>(), rightBound),
                         mask, hits);
                else
                    ierr = doComp
                        (vals, std::bind2nd<std::equal_to<double> >
                         (std::equal_to<double>(), rightBound),
                         mask, hits);
            }
            else {
                hits.set(0, mask.size());
                ierr = 0;
            }
            break;}
        default: {
            if (uncomp)
                ierr = doComp0
                    (vals, std::bind1st<std::less<double> >
                     (std::less<double>(), leftBound),
                     mask, hits);
            else
                ierr = doComp
                    (vals, std::bind1st<std::less<double> >
                     (std::less<double>(), leftBound),
                     mask, hits);
            break;}
        }
        break;}
    case ibis::qExpr::OP_LE: {
        switch (rop) {
        case ibis::qExpr::OP_LT: {
            if (leftBound < rightBound) {
                if (uncomp)
                    ierr = doComp0
                        (vals, std::bind1st<std::less_equal<double> >
                         (std::less_equal<double>(), leftBound),
                         std::bind2nd<std::less<double> >
                         (std::less<double>(), rightBound),
                         mask, hits);
                else
                    ierr = doComp
                        (vals, std::bind1st<std::less_equal<double> >
                         (std::less_equal<double>(), leftBound),
                         std::bind2nd<std::less<double> >
                         (std::less<double>(), rightBound),
                         mask, hits);
            }
            else {
                hits.set(0, mask.size());
                ierr = 0;
            }
            break;}
        case ibis::qExpr::OP_LE: {
            if (leftBound <= rightBound) {
                if (uncomp)
                    ierr = doComp0
                        (vals, std::bind1st<std::less_equal<double> >
                         (std::less_equal<double>(), leftBound),
                         std::bind2nd<std::less_equal<double> >
                         (std::less_equal<double>(), rightBound),
                         mask, hits);
                else
                    ierr = doComp
                        (vals, std::bind1st<std::less_equal<double> >
                         (std::less_equal<double>(), leftBound),
                         std::bind2nd<std::less_equal<double> >
                         (std::less_equal<double>(), rightBound),
                         mask, hits);
            }
            else {
                hits.set(0, mask.size());
                ierr = 0;
            }
            break;}
        case ibis::qExpr::OP_GT: {
            if (leftBound > rightBound) {
                if (uncomp)
                    ierr = doComp0
                        (vals, std::bind1st<std::less_equal<double> >
                         (std::less_equal<double>(), leftBound),
                         mask, hits);
                else
                    ierr = doComp
                        (vals, std::bind1st<std::less_equal<double> >
                         (std::less_equal<double>(), leftBound),
                         mask, hits);
            }
            else {
                if (uncomp)
                    ierr = doComp0
                        (vals, std::bind2nd<std::greater<double> >
                         (std::greater<double>(), rightBound),
                         mask, hits);
                else
                    ierr = doComp
                        (vals, std::bind2nd<std::greater<double> >
                         (std::greater<double>(), rightBound),
                         mask, hits);
            }
            break;}
        case ibis::qExpr::OP_GE: {
            if (leftBound >= rightBound) {
                if (uncomp)
                    ierr = doComp0
                        (vals, std::bind1st<std::less_equal<double> >
                         (std::less_equal<double>(), leftBound),
                         mask, hits);
                else
                    ierr = doComp
                        (vals, std::bind1st<std::less_equal<double> >
                         (std::less_equal<double>(), leftBound),
                         mask, hits);
            }
            else {
                if (uncomp)
                    ierr = doComp0
                        (vals, std::bind2nd<std::greater_equal<double> >
                         (std::greater_equal<double>(), rightBound),
                         mask, hits);
                else
                    ierr = doComp
                        (vals, std::bind2nd<std::greater_equal<double> >
                         (std::greater_equal<double>(), rightBound),
                         mask, hits);
            }
            break;}
        case ibis::qExpr::OP_EQ: {
            if (leftBound <= rightBound) {
                if (uncomp)
                    ierr = doComp0
                        (vals, std::bind2nd<std::equal_to<double> >
                         (std::equal_to<double>(), rightBound),
                         mask, hits);
                else
                    ierr = doComp
                        (vals, std::bind2nd<std::equal_to<double> >
                         (std::equal_to<double>(), rightBound),
                         mask, hits);
            }
            else {
                hits.set(0, mask.size());
                ierr = 0;
            }
            break;}
        default: {
            if (uncomp)
                ierr = doComp0
                    (vals, std::bind1st<std::less_equal<double> >
                     (std::less_equal<double>(), leftBound),
                     mask, hits);
            else
                ierr = doComp
                    (vals, std::bind1st<std::less_equal<double> >
                     (std::less_equal<double>(), leftBound),
                     mask, hits);
            break;}
        }
        break;}
    case ibis::qExpr::OP_GT: {
        switch (rop) {
        case ibis::qExpr::OP_LT: {
            if (leftBound <= rightBound) {
                if (uncomp)
                    ierr = doComp0
                        (vals, std::bind1st< std::greater<double> >
                         (std::greater<double>(), leftBound),
                         mask, hits);
                else
                    ierr = doComp
                        (vals, std::bind1st< std::greater<double> >
                         (std::greater<double>(), leftBound),
                         mask, hits);
            }
            else {
                if (uncomp)
                    ierr = doComp0
                        (vals, std::bind2nd<std::less<double> >
                         (std::less<double>(), rightBound),
                         mask, hits);
                else
                    ierr = doComp
                        (vals, std::bind2nd<std::less<double> >
                         (std::less<double>(), rightBound),
                         mask, hits);
            }
            break;}
        case ibis::qExpr::OP_LE: {
            if (leftBound < rightBound) {
                if (uncomp)
                    ierr = doComp0
                        (vals, std::bind1st< std::greater<double> >
                         (std::greater<double>(), leftBound),
                         mask, hits);
                else
                    ierr = doComp
                        (vals,
                         std::bind1st< std::greater<double> >
                         (std::greater<double>(), leftBound),
                         mask, hits);
            }
            else {
                if (uncomp)
                    ierr = doComp0
                        (vals, std::bind2nd<std::less_equal<double> >
                         (std::less_equal<double>(), rightBound),
                         mask, hits);
                else
                    ierr = doComp
                        (vals, std::bind2nd<std::less_equal<double> >
                         (std::less_equal<double>(), rightBound),
                         mask, hits);
            }
            break;}
        case ibis::qExpr::OP_GT: {
            if (leftBound > rightBound) {
                if (uncomp)
                    ierr = doComp0
                        (vals, std::bind1st< std::greater<double> >
                         (std::greater<double>(), leftBound),
                         std::bind2nd<std::greater<double> >
                         (std::greater<double>(), rightBound),
                         mask, hits);
                else
                    ierr = doComp
                        (vals, std::bind1st< std::greater<double> >
                         (std::greater<double>(), leftBound),
                         std::bind2nd<std::greater<double> >
                         (std::greater<double>(), rightBound),
                         mask, hits);
            }
            else {
                hits.set(0, mask.size());
                ierr = 0;
            }
            break;}
        case ibis::qExpr::OP_GE: {
            if (leftBound > rightBound) {
                if (uncomp)
                    ierr = doComp0
                        (vals, std::bind1st< std::greater<double> >
                         (std::greater<double>(), leftBound),
                         std::bind2nd<std::greater_equal<double> >
                         (std::greater_equal<double>(), rightBound),
                         mask, hits);
                else
                    ierr = doComp
                        (vals, std::bind1st< std::greater<double> >
                         (std::greater<double>(), leftBound),
                         std::bind2nd<std::greater_equal<double> >
                         (std::greater_equal<double>(), rightBound),
                         mask, hits);
            }
            else {
                hits.set(0, mask.size());
                ierr = 0;
            }
            break;}
        case ibis::qExpr::OP_EQ: {
            if (rightBound < leftBound) {
                if (uncomp)
                    ierr = doComp0
                        (vals, std::bind2nd<std::equal_to<double> >
                         (std::equal_to<double>(), rightBound),
                         mask, hits);
                else
                    ierr = doComp
                        (vals, std::bind2nd<std::equal_to<double> >
                         (std::equal_to<double>(), rightBound),
                         mask, hits);
            }
            else {
                hits.set(0, mask.size());
                ierr = 0;
            }
            break;}
        default: {
            if (uncomp)
                ierr = doComp0
                    (vals, std::bind1st< std::greater<double> >
                     (std::greater<double>(), leftBound),
                     mask, hits);
            else
                ierr = doComp
                    (vals, std::bind1st< std::greater<double> >
                     (std::greater<double>(), leftBound),
                     mask, hits);
            break;}
        }
        break;}
    case ibis::qExpr::OP_GE: {
        switch (rop) {
        case ibis::qExpr::OP_LT: {
            if (leftBound < rightBound) {
                if (uncomp)
                    ierr = doComp0
                        (vals, std::bind1st< std::greater_equal<double> >
                         (std::greater_equal<double>(), leftBound),
                         mask, hits);
                else
                    ierr = doComp
                        (vals, std::bind1st< std::greater_equal<double> >
                         (std::greater_equal<double>(), leftBound),
                         mask, hits);
            }
            else {
                if (uncomp)
                    ierr = doComp0
                        (vals, std::bind2nd<std::less<double> >
                         (std::less<double>(), rightBound),
                         mask, hits);
                else
                    ierr = doComp
                        (vals, std::bind2nd<std::less<double> >
                         (std::less<double>(), rightBound),
                         mask, hits);
            }
            break;}
        case ibis::qExpr::OP_LE: {
            if (leftBound <= rightBound) {
                if (uncomp)
                    ierr = doComp0
                        (vals, std::bind1st< std::greater_equal<double> >
                         (std::greater_equal<double>(), leftBound),
                         mask, hits);
                else
                    ierr = doComp
                        (vals, std::bind1st< std::greater_equal<double> >
                         (std::greater_equal<double>(), leftBound),
                         mask, hits);
            }
            else {
                if (uncomp)
                    ierr = doComp0
                        (vals, std::bind2nd<std::less_equal<double> >
                         (std::less_equal<double>(), rightBound),
                         mask, hits);
                else
                    ierr = doComp
                        (vals, std::bind2nd<std::less_equal<double> >
                         (std::less_equal<double>(), rightBound),
                         mask, hits);
            }
            break;}
        case ibis::qExpr::OP_GT: {
            if (leftBound > rightBound) {
                if (uncomp)
                    ierr = doComp0
                        (vals, std::bind1st< std::greater_equal<double> >
                         (std::greater_equal<double>(), leftBound),
                         std::bind2nd<std::greater<double> >
                         (std::greater<double>(), rightBound),
                         mask, hits);
                else
                    ierr = doComp
                        (vals, std::bind1st< std::greater_equal<double> >
                         (std::greater_equal<double>(), leftBound),
                         std::bind2nd<std::greater<double> >
                         (std::greater<double>(), rightBound),
                         mask, hits);
            }
            else {
                hits.set(0, mask.size());
                ierr = 0;
            }
            break;}
        case ibis::qExpr::OP_GE: {
            if (leftBound >= rightBound) {
                if (uncomp)
                    ierr = doComp0
                        (vals, std::bind1st< std::greater_equal<double> >
                         (std::greater_equal<double>(), leftBound),
                         std::bind2nd<std::greater_equal<double> >
                         (std::greater_equal<double>(), rightBound),
                         mask, hits);
                else
                    ierr = doComp
                        (vals, std::bind1st< std::greater_equal<double> >
                         (std::greater_equal<double>(), leftBound),
                         std::bind2nd<std::greater_equal<double> >
                         (std::greater_equal<double>(), rightBound),
                         mask, hits);
            }
            else {
                hits.set(0, mask.size());
                ierr = 0;
            }
            break;}
        case ibis::qExpr::OP_EQ: {
            if (rightBound <= leftBound) {
                if (uncomp)
                    ierr = doComp0
                        (vals, std::bind1st< std::greater_equal<double> >
                         (std::greater_equal<double>(), leftBound),
                         std::bind2nd<std::equal_to<double> >
                         (std::equal_to<double>(), rightBound),
                         mask, hits);
                else
                    ierr = doComp
                        (vals, std::bind1st< std::greater_equal<double> >
                         (std::greater_equal<double>(), leftBound),
                         std::bind2nd<std::equal_to<double> >
                         (std::equal_to<double>(), rightBound),
                         mask, hits);
            }
            else {
                hits.set(0, mask.size());
                ierr = 0;
            }
            break;}
        default: {
            if (uncomp)
                ierr = doComp0
                    (vals, std::bind1st< std::greater_equal<double> >
                     (std::greater_equal<double>(), leftBound),
                     mask, hits);
            else
                ierr = doComp
                    (vals, std::bind1st< std::greater_equal<double> >
                     (std::greater_equal<double>(), leftBound),
                     mask, hits);
            break;}
        }
        break;}
    case ibis::qExpr::OP_EQ: {
        switch (rop) {
        case ibis::qExpr::OP_LT: {
            if (leftBound < rightBound) {
                if (uncomp)
                    ierr = doComp0
                        (vals, std::bind1st< std::equal_to<double> >
                         (std::equal_to<double>(), leftBound),
                         mask, hits);
                else
                    ierr = doComp
                        (vals, std::bind1st< std::equal_to<double> >
                         (std::equal_to<double>(), leftBound),
                         mask, hits);
            }
            else {
                hits.set(0, mask.size());
                ierr = 0;
            }
            break;}
        case ibis::qExpr::OP_LE: {
            if (leftBound <= rightBound) {
                if (uncomp)
                    ierr = doComp0
                        (vals, std::bind1st< std::equal_to<double> >
                         (std::equal_to<double>(), leftBound),
                         mask, hits);
                else
                    ierr = doComp
                        (vals, std::bind1st< std::equal_to<double> >
                         (std::equal_to<double>(), leftBound),
                         mask, hits);
            }
            else {
                hits.set(0, mask.size());
                ierr = 0;
            }
            break;}
        case ibis::qExpr::OP_GT: {
            if (leftBound > rightBound) {
                if (uncomp)
                    ierr = doComp0
                        (vals, std::bind1st< std::equal_to<double> >
                         (std::equal_to<double>(), leftBound),
                         mask, hits);
                else
                    ierr = doComp
                        (vals, std::bind1st< std::equal_to<double> >
                         (std::equal_to<double>(), leftBound),
                         mask, hits);
            }
            else {
                hits.set(0, mask.size());
                ierr = 0;
            }
            break;}
        case ibis::qExpr::OP_GE: {
            if (leftBound >= rightBound) {
                if (uncomp)
                    ierr = doComp0
                        (vals, std::bind1st< std::equal_to<double> >
                         (std::equal_to<double>(), leftBound),
                         mask, hits);
                else
                    ierr = doComp
                        (vals, std::bind1st< std::equal_to<double> >
                         (std::equal_to<double>(), leftBound),
                         mask, hits);
            }
            else {
                hits.set(0, mask.size());
                ierr = 0;
            }
            break;}
        case ibis::qExpr::OP_EQ: {
            if (leftBound == rightBound && rightBound == rng.rightBound()) {
                if (uncomp)
                    ierr = doComp0
                        (vals, std::bind1st< std::equal_to<double> >
                         (std::equal_to<double>(), leftBound),
                         mask, hits);
                else
                    ierr = doComp
                        (vals, std::bind1st< std::equal_to<double> >
                         (std::equal_to<double>(), leftBound),
                         mask, hits);
            }
            else {
                hits.set(0, mask.size());
                ierr = 0;
            }
            break;}
        default: {
            if (uncomp)
                ierr = doComp0
                    (vals, std::bind1st< std::equal_to<double> >
                     (std::equal_to<double>(), leftBound),
                     mask, hits);
            else
                ierr = doComp
                    (vals, std::bind1st< std::equal_to<double> >
                     (std::equal_to<double>(), leftBound),
                     mask, hits);
            break;}
        }
        break;}
    default: {
        switch (rop) {
        case ibis::qExpr::OP_LT: {
            if (uncomp)
                ierr = doComp0
                    (vals, std::bind2nd<std::less<double> >
                     (std::less<double>(), rightBound),
                     mask, hits);
            else
                ierr = doComp
                    (vals, std::bind2nd<std::less<double> >
                     (std::less<double>(), rightBound),
                     mask, hits);
            break;}
        case ibis::qExpr::OP_LE: {
            if (uncomp)
                ierr = doComp0
                    (vals, std::bind2nd<std::less_equal<double> >
                     (std::less_equal<double>(), rightBound),
                     mask, hits);
            else
                ierr = doComp
                    (vals, std::bind2nd<std::less_equal<double> >
                     (std::less_equal<double>(), rightBound),
                     mask, hits);
            break;}
        case ibis::qExpr::OP_GT: {
            if (uncomp)
                ierr = doComp0
                    (vals, std::bind2nd<std::greater<double> >
                     (std::greater<double>(), rightBound),
                     mask, hits);
            else
                ierr = doComp
                    (vals, std::bind2nd<std::greater<double> >
                     (std::greater<double>(), rightBound),
                     mask, hits);
            break;}
        case ibis::qExpr::OP_GE: {
            if (uncomp)
                ierr = doComp0
                    (vals, std::bind2nd<std::greater_equal<double> >
                     (std::greater_equal<double>(), rightBound),
                     mask, hits);
            else
                ierr = doComp
                    (vals, std::bind2nd<std::greater_equal<double> >
                     (std::greater_equal<double>(), rightBound),
                     mask, hits);
            break;}
        case ibis::qExpr::OP_EQ: {
            if (rightBound == rng.rightBound()) {
                if (uncomp)
                    ierr = doComp0
                        (vals, std::bind2nd<std::equal_to<double> >
                         (std::equal_to<double>(), rightBound),
                         mask, hits);
                else
                    ierr = doComp
                        (vals, std::bind2nd<std::equal_to<double> >
                         (std::equal_to<double>(), rightBound),
                         mask, hits);
            }
            else {
                hits.set(0, mask.size());
                ierr = 0;
            }
            break;}
        default: {
            hits.set(0, mask.size());
            ierr = 0;
            break;}
        }
        break;}
    }

    if (ibis::gVerbose > 3) {
        timer.stop();
        ibis::util::logger lg;
        lg() << "part::doScan -- evaluating " << rng
             << " on " << mask.cnt() << " double "
             << (mask.cnt() > 1 ? " values" : " value") << " (total: "
             << mask.size() << ") took " << timer.realTime()
             << " sec elapsed time and produced " << hits.cnt()
             << (hits.cnt() > 1 ? " hits" : " hit");
#if DEBUG+0 > 1 || _DEBUG+0 > 1
        lg() << "\nmask\n" << mask;
        lg() << "\nhit vector\n" << hits << "\n";
#endif
    }
    return ierr;
} // ibis::part::doScan

/// Evalue the range condition on the in memory values.  This static member
/// function works on integer data provided by the caller.  Since the
/// values are provided, this function does not check the name of the
/// variable involved in the range condition.
template <typename T>
long ibis::part::doScan(const array_t<T> &vals,
                        const ibis::qContinuousRange &rng,
                        const ibis::bitvector &mask,
                        array_t<T>& res) {
    long ierr = -2;
    ibis::horometer timer;
    if (ibis::gVerbose > 3)
        timer.start();
    T leftBound, rightBound;
    ibis::qExpr::COMPARE lop = rng.leftOperator();
    ibis::qExpr::COMPARE rop = rng.rightOperator();
    if (lop == ibis::qExpr::OP_UNDEFINED &&
        rop == ibis::qExpr::OP_UNDEFINED) {
        res.reserve(mask.cnt());
        for (ibis::bitvector::indexSet is = mask.firstIndexSet();
             is.nIndices() > 0;
             ++ is) {
            const ibis::bitvector::word_t *ind = is.indices();
            if (is.isRange()) {
                for (ibis::bitvector::word_t j = ind[0]; j < ind[1]; ++ j)
                    res.push_back(vals[j]);
            }
            else {
                for (unsigned j = 0; j < is.nIndices(); ++ j)
                    res.push_back(vals[ind[j]]);
            }
        }
    }

    switch (rng.leftOperator()) {
    case ibis::qExpr::OP_UNDEFINED:
        leftBound = 0;
        break;
    case ibis::qExpr::OP_LT:
        if (rng.leftBound() < std::numeric_limits<T>::min()) {
            leftBound = std::numeric_limits<T>::min();
            lop = ibis::qExpr::OP_LE;
        }
        else if (rng.leftBound() > std::numeric_limits<T>::max()) {
            leftBound = std::numeric_limits<T>::max();
        }
        else {
            leftBound = (T) rng.leftBound();
        }
        break;
    case ibis::qExpr::OP_LE:
        if (rng.leftBound() < std::numeric_limits<T>::min()) {
            leftBound = std::numeric_limits<T>::min();
            lop = ibis::qExpr::OP_LE;
        }
        else if (rng.leftBound() > std::numeric_limits<T>::max()) {
            leftBound = std::numeric_limits<T>::max();
            lop = ibis::qExpr::OP_LT;
        }
        else {
            leftBound = (T) rng.leftBound();
            if (leftBound != rng.leftBound())
                lop = ibis::qExpr::OP_LT;
        }
        break;
    case ibis::qExpr::OP_GE:
        if (rng.leftBound() < std::numeric_limits<T>::min()) {
            leftBound = std::numeric_limits<T>::min();
            lop = ibis::qExpr::OP_GT;
        }
        else if (rng.leftBound() > std::numeric_limits<T>::max()) {
            leftBound = std::numeric_limits<T>::max();
        }
        else {
            leftBound = (T) rng.leftBound();
        }
        break;
    case ibis::qExpr::OP_GT:
        if (rng.leftBound() < std::numeric_limits<T>::min()) {
            leftBound = std::numeric_limits<T>::min();
        }
        else if (rng.leftBound() > std::numeric_limits<T>::max()) {
            leftBound = std::numeric_limits<T>::max();
            lop = ibis::qExpr::OP_GE;
        }
        else {
            leftBound = (T) rng.leftBound();
            if (leftBound != rng.leftBound())
                lop = ibis::qExpr::OP_GE;
        }
        break;
    default:
        ibis::util::round_down(rng.leftBound(), leftBound);
        break;
    }
    switch (rng.rightOperator()) {
    case ibis::qExpr::OP_UNDEFINED:
        rightBound = 0;
        break;
    case ibis::qExpr::OP_LE:
        if (rng.rightBound() < std::numeric_limits<T>::min()) {
            rightBound = std::numeric_limits<T>::min();
            rop = ibis::qExpr::OP_LT;
        }
        else if (rng.rightBound() > std::numeric_limits<T>::max()) {
            rightBound = std::numeric_limits<T>::max();
        }
        else {
            rightBound = (T) rng.rightBound();
        }
        break;
    case ibis::qExpr::OP_LT:
        if (rng.rightBound() < std::numeric_limits<T>::min()) {
            rightBound = std::numeric_limits<T>::min();
        }
        else if (rng.rightBound() > std::numeric_limits<T>::max()) {
            rightBound = std::numeric_limits<T>::max();
            rop = ibis::qExpr::OP_LE;
        }
        else {
            rightBound = (T) rng.rightBound();
            if (rightBound < rng.rightBound())
                rop = ibis::qExpr::OP_LE;
        }
        break;
    case ibis::qExpr::OP_GE:
        if (rng.rightBound() < std::numeric_limits<T>::min()) {
            rightBound = std::numeric_limits<T>::min();
        }
        else if (rng.rightBound() > std::numeric_limits<T>::max()) {
            rightBound = std::numeric_limits<T>::max();
            rop = ibis::qExpr::OP_GT;
        }
        else {
            rightBound = (T) rng.rightBound();
            if (rightBound < rng.rightBound())
                rop = ibis::qExpr::OP_GT;
        }
        break;
    case ibis::qExpr::OP_GT:
        if (rng.rightBound() < std::numeric_limits<T>::min()) {
            rightBound = std::numeric_limits<T>::min();
            rop = ibis::qExpr::OP_GE;
        }
        else if (rng.rightBound() > std::numeric_limits<T>::max()) {
            rightBound = std::numeric_limits<T>::max();
        }
        else {
            rightBound = (T) rng.rightBound();
        }
        break;
    default:
        ibis::util::round_down(rng.rightBound(), rightBound);
        break;
    }

    switch (lop) {
    case ibis::qExpr::OP_LT: {
        switch (rop) {
        case ibis::qExpr::OP_LT: {
            if (leftBound < rightBound) {
                ierr = doComp
                    (vals, std::bind1st<std::less<T> >
                     (std::less<T>(), leftBound),
                     std::bind2nd<std::less<T> >
                     (std::less<T>(), rightBound),
                     mask, res);
            }
            else {
                res.clear();
                ierr = 0;
            }
            break;}
        case ibis::qExpr::OP_LE: {
            if (leftBound < rightBound) {
                ierr = doComp
                    (vals, std::bind1st<std::less<T> >
                     (std::less<T>(), leftBound),
                     std::bind2nd<std::less_equal<T> >
                     (std::less_equal<T>(), rightBound),
                     mask, res);
            }
            else {
                res.clear();
                ierr = 0;
            }
            break;}
        case ibis::qExpr::OP_GT: {
            if (leftBound >= rightBound) {
                ierr = doComp
                    (vals, std::bind1st<std::less<T> >
                     (std::less<T>(), leftBound),
                     mask, res);
            }
            else {
                ierr = doComp
                    (vals, std::bind2nd<std::greater<T> >
                     (std::greater<T>(), rightBound),
                     mask, res);
            }
            break;}
        case ibis::qExpr::OP_GE: {
            if (leftBound >= rightBound) {
                ierr = doComp
                    (vals, std::bind1st<std::less<T> >
                     (std::less<T>(), leftBound),
                     mask, res);
            }
            else {
                ierr = doComp
                    (vals, std::bind2nd<std::greater_equal<T> >
                     (std::greater_equal<T>(), rightBound),
                     mask, res);
            }
            break;}
        case ibis::qExpr::OP_EQ: {
            if (rightBound == rng.rightBound() && leftBound < rightBound) {
                ierr = doComp
                    (vals, std::bind2nd<std::equal_to<T> >
                     (std::equal_to<T>(), rightBound),
                     mask, res);
            }
            else {
                res.clear();
                ierr = 0;
            }
            break;}
        default: {
            ierr = doComp
                (vals, std::bind1st<std::less<T> >
                 (std::less<T>(), leftBound),
                 mask, res);
            break;}
        }
        break;}
    case ibis::qExpr::OP_LE: {
        switch (rop) {
        case ibis::qExpr::OP_LT: {
            if (leftBound < rightBound) {
                ierr = doComp
                    (vals, std::bind1st<std::less_equal<T> >
                     (std::less_equal<T>(), leftBound),
                     std::bind2nd<std::less<T> >
                     (std::less<T>(), rightBound),
                     mask, res);
            }
            else {
                res.clear();
                ierr = 0;
            }
            break;}
        case ibis::qExpr::OP_LE: {
            if (leftBound <= rightBound) {
                ierr = doComp
                    (vals, std::bind1st<std::less_equal<T> >
                     (std::less_equal<T>(), leftBound),
                     std::bind2nd<std::less_equal<T> >
                     (std::less_equal<T>(), rightBound),
                     mask, res);
            }
            else {
                res.clear();
                ierr = 0;
            }
            break;}
        case ibis::qExpr::OP_GT: {
            if (leftBound > rightBound) {
                ierr = doComp
                    (vals, std::bind1st<std::less_equal<T> >
                     (std::less_equal<T>(), leftBound),
                     mask, res);
            }
            else {
                ierr = doComp
                    (vals, std::bind2nd<std::greater<T> >
                     (std::greater<T>(), rightBound),
                     mask, res);
            }
            break;}
        case ibis::qExpr::OP_GE: {
            if (leftBound >= rightBound) {
                ierr = doComp
                    (vals, std::bind1st<std::less_equal<T> >
                     (std::less_equal<T>(), leftBound),
                     mask, res);
            }
            else {
                ierr = doComp
                    (vals, std::bind2nd<std::greater_equal<T> >
                     (std::greater_equal<T>(), rightBound),
                     mask, res);
            }
            break;}
        case ibis::qExpr::OP_EQ: {
            if (rightBound == rng.rightBound() && leftBound <= rightBound) {
                ierr = doComp
                    (vals, std::bind2nd<std::equal_to<T> >
                     (std::equal_to<T>(), rightBound),
                     mask, res);
            }
            else {
                res.clear();
                ierr = 0;
            }
            break;}
        default: {
            ierr = doComp
                (vals, std::bind1st<std::less_equal<T> >
                 (std::less_equal<T>(), leftBound),
                 mask, res);
            break;}
        }
        break;}
    case ibis::qExpr::OP_GT: {
        switch (rop) {
        case ibis::qExpr::OP_LT: {
            if (leftBound <= rightBound) {
                ierr = doComp
                    (vals, std::bind1st< std::greater<T> >
                     (std::greater<T>(), leftBound),
                     mask, res);
            }
            else {
                ierr = doComp
                    (vals, std::bind2nd<std::less<T> >
                     (std::less<T>(), rightBound),
                     mask, res);
            }
            break;}
        case ibis::qExpr::OP_LE: {
            if (leftBound < rightBound) {
                ierr = doComp
                    (vals, std::bind1st< std::greater<T> >
                     (std::greater<T>(), leftBound),
                     mask, res);
            }
            else {
                ierr = doComp
                    (vals, std::bind2nd<std::less_equal<T> >
                     (std::less_equal<T>(), rightBound),
                     mask, res);
            }
            break;}
        case ibis::qExpr::OP_GT: {
            if (leftBound > rightBound) {
                ierr = doComp
                    (vals, std::bind1st< std::greater<T> >
                     (std::greater<T>(), leftBound),
                     std::bind2nd<std::greater<T> >
                     (std::greater<T>(), rightBound),
                     mask, res);
            }
            else {
                res.clear();
                ierr = 0;
            }
            break;}
        case ibis::qExpr::OP_GE: {
            if (leftBound > rightBound) {
                ierr = doComp
                    (vals, std::bind1st< std::greater<T> >
                     (std::greater<T>(), leftBound),
                     std::bind2nd<std::greater_equal<T> >
                     (std::greater_equal<T>(), rightBound),
                     mask, res);
            }
            else {
                res.clear();
                ierr = 0;
            }
            break;}
        case ibis::qExpr::OP_EQ: {
            if (rightBound == rng.rightBound() && rightBound < leftBound) {
                ierr = doComp
                    (vals, std::bind2nd<std::equal_to<T> >
                     (std::equal_to<T>(), rightBound),
                     mask, res);
            }
            else {
                res.clear();
                ierr = 0;
            }
            break;}
        default: {
            ierr = doComp
                (vals, std::bind1st< std::greater<T> >
                 (std::greater<T>(), leftBound),
                 mask, res);
            break;}
        }
        break;}
    case ibis::qExpr::OP_GE: {
        switch (rop) {
        case ibis::qExpr::OP_LT: {
            if (leftBound < rightBound) {
                ierr = doComp
                    (vals, std::bind1st< std::greater_equal<T> >
                     (std::greater_equal<T>(), leftBound),
                     mask, res);
            }
            else {
                ierr = doComp
                    (vals, std::bind2nd<std::less<T> >
                     (std::less<T>(), rightBound),
                     mask, res);
            }
            break;}
        case ibis::qExpr::OP_LE: {
            if (leftBound <= rightBound) {
                ierr = doComp
                    (vals, std::bind1st< std::greater_equal<T> >
                     (std::greater_equal<T>(), leftBound),
                     mask, res);
            }
            else {
                ierr = doComp
                    (vals, std::bind2nd<std::less_equal<T> >
                     (std::less_equal<T>(), rightBound),
                     mask, res);
            }
            break;}
        case ibis::qExpr::OP_GT: {
            if (leftBound > rightBound) {
                ierr = doComp
                    (vals, std::bind1st< std::greater_equal<T> >
                     (std::greater_equal<T>(), leftBound),
                     std::bind2nd<std::greater<T> >
                     (std::greater<T>(), rightBound),
                     mask, res);
            }
            else {
                res.clear();
                ierr = 0;
            }
            break;}
        case ibis::qExpr::OP_GE: {
            if (leftBound >= rightBound) {
                ierr = doComp
                    (vals, std::bind1st< std::greater_equal<T> >
                     (std::greater_equal<T>(), leftBound),
                     std::bind2nd<std::greater_equal<T> >
                     (std::greater_equal<T>(), rightBound),
                     mask, res);
            }
            else {
                res.clear();
                ierr = 0;
            }
            break;}
        case ibis::qExpr::OP_EQ: {
            if (rightBound == rng.rightBound() && rightBound <= leftBound) {
                ierr = doComp
                    (vals, std::bind1st< std::greater_equal<T> >
                     (std::greater_equal<T>(), leftBound),
                     std::bind2nd<std::equal_to<T> >
                     (std::equal_to<T>(), rightBound),
                     mask, res);
            }
            else {
                res.clear();
                ierr = 0;
            }
            break;}
        default: {
            ierr = doComp
                (vals, std::bind1st< std::greater_equal<T> >
                 (std::greater_equal<T>(), leftBound),
                 mask, res);
            break;}
        }
        break;}
    case ibis::qExpr::OP_EQ: {
        if (leftBound == rng.leftBound()) {
            switch (rop) {
            case ibis::qExpr::OP_LT: {
                if (leftBound < rightBound) {
                    ierr = doComp
                        (vals, std::bind1st< std::equal_to<T> >
                         (std::equal_to<T>(), leftBound),
                         mask, res);
                }
                else {
                    res.clear();
                    ierr = 0;
                }
                break;}
            case ibis::qExpr::OP_LE: {
                if (leftBound <= rightBound) {
                    ierr = doComp
                        (vals, std::bind1st< std::equal_to<T> >
                         (std::equal_to<T>(), leftBound),
                         mask, res);
                }
                else {
                    res.clear();
                    ierr = 0;
                }
                break;}
            case ibis::qExpr::OP_GT: {
                if (leftBound > rightBound) {
                    ierr = doComp
                        (vals, std::bind1st< std::equal_to<T> >
                         (std::equal_to<T>(), leftBound),
                         mask, res);
                }
                else {
                    res.clear();
                    ierr = 0;
                }
                break;}
            case ibis::qExpr::OP_GE: {
                if (leftBound >= rightBound) {
                    ierr = doComp
                        (vals, std::bind1st< std::equal_to<T> >
                         (std::equal_to<T>(), leftBound),
                         mask, res);
                }
                else {
                    res.clear();
                    ierr = 0;
                }
                break;}
            case ibis::qExpr::OP_EQ: {
                if (leftBound == rightBound && rightBound == rng.rightBound()) {
                    ierr = doComp
                        (vals, std::bind1st< std::equal_to<T> >
                         (std::equal_to<T>(), leftBound),
                         mask, res);
                }
                else {
                    res.clear();
                    ierr = 0;
                }
                break;}
            default: {
                ierr = doComp
                    (vals, std::bind1st< std::equal_to<T> >
                     (std::equal_to<T>(), leftBound),
                     mask, res);
                break;}
            }
        }
        else {
            res.clear();
            ierr = 0;
        }
        break;}
    default: {
        switch (rop) {
        case ibis::qExpr::OP_LT: {
            ierr = doComp
                (vals, std::bind2nd<std::less<T> >
                 (std::less<T>(), rightBound),
                 mask, res);
            break;}
        case ibis::qExpr::OP_LE: {
            ierr = doComp
                (vals, std::bind2nd<std::less_equal<T> >
                 (std::less_equal<T>(), rightBound),
                 mask, res);
            break;}
        case ibis::qExpr::OP_GT: {
            ierr = doComp
                (vals, std::bind2nd<std::greater<T> >
                 (std::greater<T>(), rightBound),
                 mask, res);
            break;}
        case ibis::qExpr::OP_GE: {
            ierr = doComp
                (vals, std::bind2nd<std::greater_equal<T> >
                 (std::greater_equal<T>(), rightBound),
                 mask, res);
            break;}
        case ibis::qExpr::OP_EQ: {
            if (rightBound == rng.rightBound()) {
                ierr = doComp
                    (vals, std::bind2nd<std::equal_to<T> >
                     (std::equal_to<T>(), rightBound),
                     mask, res);
            }
            else {
                res.clear();
                ierr = 0;
            }
            break;}
        default: {
            res.clear();
            ierr = 0;
            break;}
        }
        break;}
    }

    if (ibis::gVerbose > 3) {
        timer.stop();
        ibis::util::logger lg;
        lg() << "part::doScan -- evaluating " << rng
             << " on " << mask.cnt() << " " << typeid(T).name()
             << (mask.cnt() > 1 ? " values" : " value") << " (total: "
             << mask.size() << ") took " << timer.realTime()
             << " sec elapsed time and produced " << res.size()
             << (res.size() > 1 ? " hits" : " hit");
    }
    return ierr;
} // ibis::part::doScan

/// Evalue the range condition on the in memory values.  This static member
/// function works on integer data provided by the caller.  Since the
/// values are provided, this function does not check the name of the
/// variable involved in the range condition.
template <typename T>
long ibis::part::doScan(const array_t<T> &vals,
                        const ibis::qContinuousRange &rng,
                        const ibis::bitvector &mask,
                        array_t<T> &res, ibis::bitvector &hits) {
    long ierr = -2;
    ibis::horometer timer;
    if (ibis::gVerbose > 3)
        timer.start();
    T leftBound, rightBound;
    ibis::qExpr::COMPARE lop = rng.leftOperator();
    ibis::qExpr::COMPARE rop = rng.rightOperator();
    if (lop == ibis::qExpr::OP_UNDEFINED &&
        rop == ibis::qExpr::OP_UNDEFINED) {
        res.reserve(mask.cnt());
        for (ibis::bitvector::indexSet is = mask.firstIndexSet();
             is.nIndices() > 0;
             ++ is) {
            const ibis::bitvector::word_t *ind = is.indices();
            if (is.isRange()) {
                for (ibis::bitvector::word_t j = ind[0]; j < ind[1]; ++ j)
                    res.push_back(vals[j]);
            }
            else {
                for (unsigned j = 0; j < is.nIndices(); ++ j)
                    res.push_back(vals[ind[j]]);
            }
        }
    }

    switch (rng.leftOperator()) {
    case ibis::qExpr::OP_UNDEFINED:
        leftBound = 0;
        break;
    case ibis::qExpr::OP_LT:
        if (rng.leftBound() < std::numeric_limits<T>::min()) {
            leftBound = std::numeric_limits<T>::min();
            lop = ibis::qExpr::OP_LE;
        }
        else if (rng.leftBound() > std::numeric_limits<T>::max()) {
            leftBound = std::numeric_limits<T>::max();
        }
        else {
            leftBound = (T) rng.leftBound();
        }
        break;
    case ibis::qExpr::OP_LE:
        if (rng.leftBound() < std::numeric_limits<T>::min()) {
            leftBound = std::numeric_limits<T>::min();
            lop = ibis::qExpr::OP_LE;
        }
        else if (rng.leftBound() > std::numeric_limits<T>::max()) {
            leftBound = std::numeric_limits<T>::max();
            lop = ibis::qExpr::OP_LT;
        }
        else {
            leftBound = (T) rng.leftBound();
            if (leftBound != rng.leftBound())
                lop = ibis::qExpr::OP_LT;
        }
        break;
    case ibis::qExpr::OP_GE:
        if (rng.leftBound() < std::numeric_limits<T>::min()) {
            leftBound = std::numeric_limits<T>::min();
            lop = ibis::qExpr::OP_GT;
        }
        else if (rng.leftBound() > std::numeric_limits<T>::max()) {
            leftBound = std::numeric_limits<T>::max();
        }
        else {
            leftBound = (T) rng.leftBound();
        }
        break;
    case ibis::qExpr::OP_GT:
        if (rng.leftBound() < std::numeric_limits<T>::min()) {
            leftBound = std::numeric_limits<T>::min();
        }
        else if (rng.leftBound() > std::numeric_limits<T>::max()) {
            leftBound = std::numeric_limits<T>::max();
            lop = ibis::qExpr::OP_GE;
        }
        else {
            leftBound = (T) rng.leftBound();
            if (leftBound != rng.leftBound())
                lop = ibis::qExpr::OP_GE;
        }
        break;
    default:
        ibis::util::round_down(rng.leftBound(), leftBound);
        break;
    }
    switch (rng.rightOperator()) {
    case ibis::qExpr::OP_UNDEFINED:
        rightBound = 0;
        break;
    case ibis::qExpr::OP_LE:
        if (rng.rightBound() < std::numeric_limits<T>::min()) {
            rightBound = std::numeric_limits<T>::min();
            rop = ibis::qExpr::OP_LT;
        }
        else if (rng.rightBound() > std::numeric_limits<T>::max()) {
            rightBound = std::numeric_limits<T>::max();
        }
        else {
            rightBound = (T) rng.rightBound();
        }
        break;
    case ibis::qExpr::OP_LT:
        if (rng.rightBound() < std::numeric_limits<T>::min()) {
            rightBound = std::numeric_limits<T>::min();
        }
        else if (rng.rightBound() > std::numeric_limits<T>::max()) {
            rightBound = std::numeric_limits<T>::max();
            rop = ibis::qExpr::OP_LE;
        }
        else {
            rightBound = (T) rng.rightBound();
            if (rightBound < rng.rightBound())
                rop = ibis::qExpr::OP_LE;
        }
        break;
    case ibis::qExpr::OP_GE:
        if (rng.rightBound() < std::numeric_limits<T>::min()) {
            rightBound = std::numeric_limits<T>::min();
        }
        else if (rng.rightBound() > std::numeric_limits<T>::max()) {
            rightBound = std::numeric_limits<T>::max();
            rop = ibis::qExpr::OP_GT;
        }
        else {
            rightBound = (T) rng.rightBound();
            if (rightBound < rng.rightBound())
                rop = ibis::qExpr::OP_GT;
        }
        break;
    case ibis::qExpr::OP_GT:
        if (rng.rightBound() < std::numeric_limits<T>::min()) {
            rightBound = std::numeric_limits<T>::min();
            rop = ibis::qExpr::OP_GE;
        }
        else if (rng.rightBound() > std::numeric_limits<T>::max()) {
            rightBound = std::numeric_limits<T>::max();
        }
        else {
            rightBound = (T) rng.rightBound();
        }
        break;
    default:
        ibis::util::round_down(rng.rightBound(), rightBound);
        break;
    }

    switch (lop) {
    case ibis::qExpr::OP_LT: {
        switch (rop) {
        case ibis::qExpr::OP_LT: {
            if (leftBound < rightBound) {
                ierr = doComp
                    (vals, std::bind1st<std::less<T> >
                     (std::less<T>(), leftBound),
                     std::bind2nd<std::less<T> >
                     (std::less<T>(), rightBound),
                     mask, res, hits);
            }
            else {
                res.clear();
                ierr = 0;
            }
            break;}
        case ibis::qExpr::OP_LE: {
            if (leftBound < rightBound) {
                ierr = doComp
                    (vals, std::bind1st<std::less<T> >
                     (std::less<T>(), leftBound),
                     std::bind2nd<std::less_equal<T> >
                     (std::less_equal<T>(), rightBound),
                     mask, res, hits);
            }
            else {
                res.clear();
                ierr = 0;
            }
            break;}
        case ibis::qExpr::OP_GT: {
            if (leftBound >= rightBound) {
                ierr = doComp
                    (vals, std::bind1st<std::less<T> >
                     (std::less<T>(), leftBound),
                     mask, res, hits);
            }
            else {
                ierr = doComp
                    (vals, std::bind2nd<std::greater<T> >
                     (std::greater<T>(), rightBound),
                     mask, res, hits);
            }
            break;}
        case ibis::qExpr::OP_GE: {
            if (leftBound >= rightBound) {
                ierr = doComp
                    (vals, std::bind1st<std::less<T> >
                     (std::less<T>(), leftBound),
                     mask, res, hits);
            }
            else {
                ierr = doComp
                    (vals, std::bind2nd<std::greater_equal<T> >
                     (std::greater_equal<T>(), rightBound),
                     mask, res, hits);
            }
            break;}
        case ibis::qExpr::OP_EQ: {
            if (rightBound == rng.rightBound() && leftBound < rightBound) {
                ierr = doComp
                    (vals, std::bind2nd<std::equal_to<T> >
                     (std::equal_to<T>(), rightBound),
                     mask, res, hits);
            }
            else {
                res.clear();
                ierr = 0;
            }
            break;}
        default: {
            ierr = doComp
                (vals, std::bind1st<std::less<T> >
                 (std::less<T>(), leftBound),
                 mask, res, hits);
            break;}
        }
        break;}
    case ibis::qExpr::OP_LE: {
        switch (rop) {
        case ibis::qExpr::OP_LT: {
            if (leftBound < rightBound) {
                ierr = doComp
                    (vals, std::bind1st<std::less_equal<T> >
                     (std::less_equal<T>(), leftBound),
                     std::bind2nd<std::less<T> >
                     (std::less<T>(), rightBound),
                     mask, res, hits);
            }
            else {
                res.clear();
                ierr = 0;
            }
            break;}
        case ibis::qExpr::OP_LE: {
            if (leftBound <= rightBound) {
                ierr = doComp
                    (vals, std::bind1st<std::less_equal<T> >
                     (std::less_equal<T>(), leftBound),
                     std::bind2nd<std::less_equal<T> >
                     (std::less_equal<T>(), rightBound),
                     mask, res, hits);
            }
            else {
                res.clear();
                ierr = 0;
            }
            break;}
        case ibis::qExpr::OP_GT: {
            if (leftBound > rightBound) {
                ierr = doComp
                    (vals, std::bind1st<std::less_equal<T> >
                     (std::less_equal<T>(), leftBound),
                     mask, res, hits);
            }
            else {
                ierr = doComp
                    (vals, std::bind2nd<std::greater<T> >
                     (std::greater<T>(), rightBound),
                     mask, res, hits);
            }
            break;}
        case ibis::qExpr::OP_GE: {
            if (leftBound >= rightBound) {
                ierr = doComp
                    (vals, std::bind1st<std::less_equal<T> >
                     (std::less_equal<T>(), leftBound),
                     mask, res, hits);
            }
            else {
                ierr = doComp
                    (vals, std::bind2nd<std::greater_equal<T> >
                     (std::greater_equal<T>(), rightBound),
                     mask, res, hits);
            }
            break;}
        case ibis::qExpr::OP_EQ: {
            if (rightBound == rng.rightBound() && leftBound <= rightBound) {
                ierr = doComp
                    (vals, std::bind2nd<std::equal_to<T> >
                     (std::equal_to<T>(), rightBound),
                     mask, res, hits);
            }
            else {
                res.clear();
                ierr = 0;
            }
            break;}
        default: {
            ierr = doComp
                (vals, std::bind1st<std::less_equal<T> >
                 (std::less_equal<T>(), leftBound),
                 mask, res, hits);
            break;}
        }
        break;}
    case ibis::qExpr::OP_GT: {
        switch (rop) {
        case ibis::qExpr::OP_LT: {
            if (leftBound <= rightBound) {
                ierr = doComp
                    (vals, std::bind1st< std::greater<T> >
                     (std::greater<T>(), leftBound),
                     mask, res, hits);
            }
            else {
                ierr = doComp
                    (vals, std::bind2nd<std::less<T> >
                     (std::less<T>(), rightBound),
                     mask, res, hits);
            }
            break;}
        case ibis::qExpr::OP_LE: {
            if (leftBound < rightBound) {
                ierr = doComp
                    (vals, std::bind1st< std::greater<T> >
                     (std::greater<T>(), leftBound),
                     mask, res, hits);
            }
            else {
                ierr = doComp
                    (vals, std::bind2nd<std::less_equal<T> >
                     (std::less_equal<T>(), rightBound),
                     mask, res, hits);
            }
            break;}
        case ibis::qExpr::OP_GT: {
            if (leftBound > rightBound) {
                ierr = doComp
                    (vals, std::bind1st< std::greater<T> >
                     (std::greater<T>(), leftBound),
                     std::bind2nd<std::greater<T> >
                     (std::greater<T>(), rightBound),
                     mask, res, hits);
            }
            else {
                res.clear();
                ierr = 0;
            }
            break;}
        case ibis::qExpr::OP_GE: {
            if (leftBound > rightBound) {
                ierr = doComp
                    (vals, std::bind1st< std::greater<T> >
                     (std::greater<T>(), leftBound),
                     std::bind2nd<std::greater_equal<T> >
                     (std::greater_equal<T>(), rightBound),
                     mask, res, hits);
            }
            else {
                res.clear();
                ierr = 0;
            }
            break;}
        case ibis::qExpr::OP_EQ: {
            if (rightBound == rng.rightBound() && rightBound < leftBound) {
                ierr = doComp
                    (vals, std::bind2nd<std::equal_to<T> >
                     (std::equal_to<T>(), rightBound),
                     mask, res, hits);
            }
            else {
                res.clear();
                ierr = 0;
            }
            break;}
        default: {
            ierr = doComp
                (vals, std::bind1st< std::greater<T> >
                 (std::greater<T>(), leftBound),
                 mask, res, hits);
            break;}
        }
        break;}
    case ibis::qExpr::OP_GE: {
        switch (rop) {
        case ibis::qExpr::OP_LT: {
            if (leftBound < rightBound) {
                ierr = doComp
                    (vals, std::bind1st< std::greater_equal<T> >
                     (std::greater_equal<T>(), leftBound),
                     mask, res, hits);
            }
            else {
                ierr = doComp
                    (vals, std::bind2nd<std::less<T> >
                     (std::less<T>(), rightBound),
                     mask, res, hits);
            }
            break;}
        case ibis::qExpr::OP_LE: {
            if (leftBound <= rightBound) {
                ierr = doComp
                    (vals, std::bind1st< std::greater_equal<T> >
                     (std::greater_equal<T>(), leftBound),
                     mask, res, hits);
            }
            else {
                ierr = doComp
                    (vals, std::bind2nd<std::less_equal<T> >
                     (std::less_equal<T>(), rightBound),
                     mask, res, hits);
            }
            break;}
        case ibis::qExpr::OP_GT: {
            if (leftBound > rightBound) {
                ierr = doComp
                    (vals, std::bind1st< std::greater_equal<T> >
                     (std::greater_equal<T>(), leftBound),
                     std::bind2nd<std::greater<T> >
                     (std::greater<T>(), rightBound),
                     mask, res, hits);
            }
            else {
                res.clear();
                ierr = 0;
            }
            break;}
        case ibis::qExpr::OP_GE: {
            if (leftBound >= rightBound) {
                ierr = doComp
                    (vals, std::bind1st< std::greater_equal<T> >
                     (std::greater_equal<T>(), leftBound),
                     std::bind2nd<std::greater_equal<T> >
                     (std::greater_equal<T>(), rightBound),
                     mask, res, hits);
            }
            else {
                res.clear();
                ierr = 0;
            }
            break;}
        case ibis::qExpr::OP_EQ: {
            if (rightBound == rng.rightBound() && rightBound <= leftBound) {
                ierr = doComp
                    (vals, std::bind1st< std::greater_equal<T> >
                     (std::greater_equal<T>(), leftBound),
                     std::bind2nd<std::equal_to<T> >
                     (std::equal_to<T>(), rightBound),
                     mask, res, hits);
            }
            else {
                res.clear();
                ierr = 0;
            }
            break;}
        default: {
            ierr = doComp
                (vals, std::bind1st< std::greater_equal<T> >
                 (std::greater_equal<T>(), leftBound),
                 mask, res, hits);
            break;}
        }
        break;}
    case ibis::qExpr::OP_EQ: {
        if (leftBound == rng.leftBound()) {
            switch (rop) {
            case ibis::qExpr::OP_LT: {
                if (leftBound < rightBound) {
                    ierr = doComp
                        (vals, std::bind1st< std::equal_to<T> >
                         (std::equal_to<T>(), leftBound),
                         mask, res, hits);
                }
                else {
                    res.clear();
                    ierr = 0;
                }
                break;}
            case ibis::qExpr::OP_LE: {
                if (leftBound <= rightBound) {
                    ierr = doComp
                        (vals, std::bind1st< std::equal_to<T> >
                         (std::equal_to<T>(), leftBound),
                         mask, res, hits);
                }
                else {
                    res.clear();
                    ierr = 0;
                }
                break;}
            case ibis::qExpr::OP_GT: {
                if (leftBound > rightBound) {
                    ierr = doComp
                        (vals, std::bind1st< std::equal_to<T> >
                         (std::equal_to<T>(), leftBound),
                         mask, res, hits);
                }
                else {
                    res.clear();
                    ierr = 0;
                }
                break;}
            case ibis::qExpr::OP_GE: {
                if (leftBound >= rightBound) {
                    ierr = doComp
                        (vals, std::bind1st< std::equal_to<T> >
                         (std::equal_to<T>(), leftBound),
                         mask, res, hits);
                }
                else {
                    res.clear();
                    ierr = 0;
                }
                break;}
            case ibis::qExpr::OP_EQ: {
                if (leftBound == rightBound && rightBound == rng.rightBound()) {
                    ierr = doComp
                        (vals, std::bind1st< std::equal_to<T> >
                         (std::equal_to<T>(), leftBound),
                         mask, res, hits);
                }
                else {
                    res.clear();
                    ierr = 0;
                }
                break;}
            default: {
                ierr = doComp
                    (vals, std::bind1st< std::equal_to<T> >
                     (std::equal_to<T>(), leftBound),
                     mask, res, hits);
                break;}
            }
        }
        else {
            res.clear();
            ierr = 0;
        }
        break;}
    default: {
        switch (rop) {
        case ibis::qExpr::OP_LT: {
            ierr = doComp
                (vals, std::bind2nd<std::less<T> >
                 (std::less<T>(), rightBound),
                 mask, res, hits);
            break;}
        case ibis::qExpr::OP_LE: {
            ierr = doComp
                (vals, std::bind2nd<std::less_equal<T> >
                 (std::less_equal<T>(), rightBound),
                 mask, res, hits);
            break;}
        case ibis::qExpr::OP_GT: {
            ierr = doComp
                (vals, std::bind2nd<std::greater<T> >
                 (std::greater<T>(), rightBound),
                 mask, res, hits);
            break;}
        case ibis::qExpr::OP_GE: {
            ierr = doComp
                (vals, std::bind2nd<std::greater_equal<T> >
                 (std::greater_equal<T>(), rightBound),
                 mask, res, hits);
            break;}
        case ibis::qExpr::OP_EQ: {
            if (rightBound == rng.rightBound()) {
                ierr = doComp
                    (vals, std::bind2nd<std::equal_to<T> >
                     (std::equal_to<T>(), rightBound),
                     mask, res, hits);
            }
            else {
                res.clear();
                ierr = 0;
            }
            break;}
        default: {
            res.clear();
            ierr = 0;
            break;}
        }
        break;}
    }

    hits.adjustSize(0, mask.size());
    if (ibis::gVerbose > 3) {
        timer.stop();
        ibis::util::logger lg;
        lg() << "part::doScan -- evaluating " << rng
             << " on " << mask.cnt() << " " << typeid(T).name()
             << (mask.cnt() > 1 ? " values" : " value") << " (total: "
             << mask.size() << ") took " << timer.realTime()
             << " sec elapsed time and produced " << res.size()
             << (res.size() > 1 ? " hits" : " hit");
    }
    return ierr;
} // ibis::part::doScan

/// Examine the range condition with in memory values.
/// A specialization of the template for float arrays.  All comparisons are
/// performed as doubles.
template <>
long ibis::part::doScan(const array_t<float> &vals,
                        const ibis::qContinuousRange &rng,
                        const ibis::bitvector &mask,
                        array_t<float> &res) {
    long ierr = -2;
    ibis::horometer timer;
    if (ibis::gVerbose > 3)
        timer.start();
    const double leftBound = rng.leftBound();
    const double rightBound = rng.rightBound();
    const ibis::qExpr::COMPARE lop = rng.leftOperator();
    const ibis::qExpr::COMPARE rop = rng.rightOperator();
    if (lop == ibis::qExpr::OP_UNDEFINED &&
        rop == ibis::qExpr::OP_UNDEFINED) {
        res.reserve(mask.cnt());
        for (ibis::bitvector::indexSet is = mask.firstIndexSet();
             is.nIndices() > 0;
             ++ is) {
            const ibis::bitvector::word_t *ind = is.indices();
            if (is.isRange()) {
                for (ibis::bitvector::word_t j = ind[0]; j < ind[1]; ++ j)
                    res.push_back(vals[j]);
            }
            else {
                for (unsigned j = 0; j < is.nIndices(); ++ j)
                    res.push_back(vals[ind[j]]);
            }
        }
    }

    switch (lop) {
    case ibis::qExpr::OP_LT: {
        switch (rop) {
        case ibis::qExpr::OP_LT: {
            if (leftBound < rightBound) {
                ierr = doComp
                    (vals, std::bind1st<std::less<double> >
                     (std::less<double>(), leftBound),
                     std::bind2nd<std::less<double> >
                     (std::less<double>(), rightBound),
                     mask, res);
            }
            else {
                res.clear();
                ierr = 0;
            }
            break;}
        case ibis::qExpr::OP_LE: {
            if (leftBound < rightBound) {
                ierr = doComp
                    (vals, std::bind1st<std::less<double> >
                     (std::less<double>(), leftBound),
                     std::bind2nd<std::less_equal<double> >
                     (std::less_equal<double>(), rightBound),
                     mask, res);
            }
            else {
                res.clear();
                ierr = 0;
            }
            break;}
        case ibis::qExpr::OP_GT: {
            if (leftBound >= rightBound) {
                ierr = doComp
                    (vals, std::bind1st<std::less<double> >
                     (std::less<double>(), leftBound),
                     mask, res);
            }
            else {
                ierr = doComp
                    (vals, std::bind2nd<std::greater<double> >
                     (std::greater<double>(), rightBound),
                     mask, res);
            }
            break;}
        case ibis::qExpr::OP_GE: {
            if (leftBound >= rightBound) {
                ierr = doComp
                    (vals, std::bind1st<std::less<double> >
                     (std::less<double>(), leftBound),
                     mask, res);
            }
            else {
                ierr = doComp
                    (vals, std::bind2nd<std::greater_equal<double> >
                     (std::greater_equal<double>(), rightBound),
                     mask, res);
            }
            break;}
        case ibis::qExpr::OP_EQ: {
            if (leftBound < rightBound) {
                ierr = doComp
                    (vals, std::bind2nd<std::equal_to<double> >
                     (std::equal_to<double>(), rightBound),
                     mask, res);
            }
            else {
                res.clear();
                ierr = 0;
            }
            break;}
        default: {
            ierr = doComp
                (vals, std::bind1st<std::less<double> >
                 (std::less<double>(), leftBound),
                 mask, res);
            break;}
        }
        break;}
    case ibis::qExpr::OP_LE: {
        switch (rop) {
        case ibis::qExpr::OP_LT: {
            if (leftBound < rightBound) {
                ierr = doComp
                    (vals, std::bind1st<std::less_equal<double> >
                     (std::less_equal<double>(), leftBound),
                     std::bind2nd<std::less<double> >
                     (std::less<double>(), rightBound),
                     mask, res);
            }
            else {
                res.clear();
                ierr = 0;
            }
            break;}
        case ibis::qExpr::OP_LE: {
            if (leftBound <= rightBound) {
                ierr = doComp
                    (vals, std::bind1st<std::less_equal<double> >
                     (std::less_equal<double>(), leftBound),
                     std::bind2nd<std::less_equal<double> >
                     (std::less_equal<double>(), rightBound),
                     mask, res);
            }
            else {
                res.clear();
                ierr = 0;
            }
            break;}
        case ibis::qExpr::OP_GT: {
            if (leftBound > rightBound) {
                ierr = doComp
                    (vals, std::bind1st<std::less_equal<double> >
                     (std::less_equal<double>(), leftBound),
                     mask, res);
            }
            else {
                ierr = doComp
                    (vals, std::bind2nd<std::greater<double> >
                     (std::greater<double>(), rightBound),
                     mask, res);
            }
            break;}
        case ibis::qExpr::OP_GE: {
            if (leftBound >= rightBound) {
                ierr = doComp
                    (vals, std::bind1st<std::less_equal<double> >
                     (std::less_equal<double>(), leftBound),
                     mask, res);
            }
            else {
                ierr = doComp
                    (vals, std::bind2nd<std::greater_equal<double> >
                     (std::greater_equal<double>(), rightBound),
                     mask, res);
            }
            break;}
        case ibis::qExpr::OP_EQ: {
            if (leftBound <= rightBound) {
                ierr = doComp
                    (vals, std::bind2nd<std::equal_to<double> >
                     (std::equal_to<double>(), rightBound),
                     mask, res);
            }
            else {
                res.clear();
                ierr = 0;
            }
            break;}
        default: {
            ierr = doComp
                (vals, std::bind1st<std::less_equal<double> >
                 (std::less_equal<double>(), leftBound),
                 mask, res);
            break;}
        }
        break;}
    case ibis::qExpr::OP_GT: {
        switch (rop) {
        case ibis::qExpr::OP_LT: {
            if (leftBound <= rightBound) {
                ierr = doComp
                    (vals, std::bind1st< std::greater<double> >
                     (std::greater<double>(), leftBound),
                     mask, res);
            }
            else {
                ierr = doComp
                    (vals, std::bind2nd<std::less<double> >
                     (std::less<double>(), rightBound),
                     mask, res);
            }
            break;}
        case ibis::qExpr::OP_LE: {
            if (leftBound < rightBound) {
                ierr = doComp
                    (vals, std::bind1st< std::greater<double> >
                     (std::greater<double>(), leftBound),
                     mask, res);
            }
            else {
                ierr = doComp
                    (vals, std::bind2nd<std::less_equal<double> >
                     (std::less_equal<double>(), rightBound),
                     mask, res);
            }
            break;}
        case ibis::qExpr::OP_GT: {
            if (leftBound > rightBound) {
                ierr = doComp
                    (vals, std::bind1st< std::greater<double> >
                     (std::greater<double>(), leftBound),
                     std::bind2nd<std::greater<double> >
                     (std::greater<double>(), rightBound),
                     mask, res);
            }
            else {
                res.clear();
                ierr = 0;
            }
            break;}
        case ibis::qExpr::OP_GE: {
            if (leftBound > rightBound) {
                ierr = doComp
                    (vals, std::bind1st< std::greater<double> >
                     (std::greater<double>(), leftBound),
                     std::bind2nd<std::greater_equal<double> >
                     (std::greater_equal<double>(), rightBound),
                     mask, res);
            }
            else {
                res.clear();
                ierr = 0;
            }
            break;}
        case ibis::qExpr::OP_EQ: {
            if (rightBound < leftBound) {
                ierr = doComp
                    (vals, std::bind2nd<std::equal_to<double> >
                     (std::equal_to<double>(), rightBound),
                     mask, res);
            }
            else {
                res.clear();
                ierr = 0;
            }
            break;}
        default: {
            ierr = doComp
                (vals, std::bind1st< std::greater<double> >
                 (std::greater<double>(), leftBound),
                 mask, res);
            break;}
        }
        break;}
    case ibis::qExpr::OP_GE: {
        switch (rop) {
        case ibis::qExpr::OP_LT: {
            if (leftBound < rightBound) {
                ierr = doComp
                    (vals, std::bind1st< std::greater_equal<double> >
                     (std::greater_equal<double>(), leftBound),
                     mask, res);
            }
            else {
                ierr = doComp
                    (vals, std::bind2nd<std::less<double> >
                     (std::less<double>(), rightBound),
                     mask, res);
            }
            break;}
        case ibis::qExpr::OP_LE: {
            if (leftBound <= rightBound) {
                ierr = doComp
                    (vals, std::bind1st< std::greater_equal<double> >
                     (std::greater_equal<double>(), leftBound),
                     mask, res);
            }
            else {
                ierr = doComp
                    (vals, std::bind2nd<std::less_equal<double> >
                     (std::less_equal<double>(), rightBound),
                     mask, res);
            }
            break;}
        case ibis::qExpr::OP_GT: {
            if (leftBound > rightBound) {
                ierr = doComp
                    (vals, std::bind1st< std::greater_equal<double> >
                     (std::greater_equal<double>(), leftBound),
                     std::bind2nd<std::greater<double> >
                     (std::greater<double>(), rightBound),
                     mask, res);
            }
            else {
                res.clear();
                ierr = 0;
            }
            break;}
        case ibis::qExpr::OP_GE: {
            if (leftBound >= rightBound) {
                ierr = doComp
                    (vals, std::bind1st< std::greater_equal<double> >
                     (std::greater_equal<double>(), leftBound),
                     std::bind2nd<std::greater_equal<double> >
                     (std::greater_equal<double>(), rightBound),
                     mask, res);
            }
            else {
                res.clear();
                ierr = 0;
            }
            break;}
        case ibis::qExpr::OP_EQ: {
            if (rightBound <= leftBound) {
                ierr = doComp
                    (vals, std::bind1st< std::greater_equal<double> >
                     (std::greater_equal<double>(), leftBound),
                     std::bind2nd<std::equal_to<double> >
                     (std::equal_to<double>(), rightBound),
                     mask, res);
            }
            else {
                res.clear();
                ierr = 0;
            }
            break;}
        default: {
            ierr = doComp
                (vals, std::bind1st< std::greater_equal<double> >
                 (std::greater_equal<double>(), leftBound),
                 mask, res);
            break;}
        }
        break;}
    case ibis::qExpr::OP_EQ: {
        switch (rop) {
        case ibis::qExpr::OP_LT: {
            if (leftBound < rightBound) {
                ierr = doComp
                    (vals, std::bind1st< std::equal_to<double> >
                     (std::equal_to<double>(), leftBound),
                     mask, res);
            }
            else {
                res.clear();
                ierr = 0;
            }
            break;}
        case ibis::qExpr::OP_LE: {
            if (leftBound <= rightBound) {
                ierr = doComp
                    (vals, std::bind1st< std::equal_to<double> >
                     (std::equal_to<double>(), leftBound),
                     mask, res);
            }
            else {
                res.clear();
                ierr = 0;
            }
            break;}
        case ibis::qExpr::OP_GT: {
            if (leftBound > rightBound) {
                ierr = doComp
                    (vals, std::bind1st< std::equal_to<double> >
                     (std::equal_to<double>(), leftBound),
                     mask, res);
            }
            else {
                res.clear();
                ierr = 0;
            }
            break;}
        case ibis::qExpr::OP_GE: {
            if (leftBound >= rightBound) {
                ierr = doComp
                    (vals, std::bind1st< std::equal_to<double> >
                     (std::equal_to<double>(), leftBound),
                     mask, res);
            }
            else {
                res.clear();
                ierr = 0;
            }
            break;}
        case ibis::qExpr::OP_EQ: {
            if (leftBound == rightBound && rightBound == rng.rightBound()) {
                ierr = doComp
                    (vals, std::bind1st< std::equal_to<double> >
                     (std::equal_to<double>(), leftBound),
                     mask, res);
            }
            else {
                res.clear();
                ierr = 0;
            }
            break;}
        default: {
            ierr = doComp
                (vals, std::bind1st< std::equal_to<double> >
                 (std::equal_to<double>(), leftBound),
                 mask, res);
            break;}
        }
        break;}
    default: {
        switch (rop) {
        case ibis::qExpr::OP_LT: {
            ierr = doComp
                (vals, std::bind2nd<std::less<double> >
                 (std::less<double>(), rightBound),
                 mask, res);
            break;}
        case ibis::qExpr::OP_LE: {
            ierr = doComp
                (vals, std::bind2nd<std::less_equal<double> >
                 (std::less_equal<double>(), rightBound),
                 mask, res);
            break;}
        case ibis::qExpr::OP_GT: {
            ierr = doComp
                (vals, std::bind2nd<std::greater<double> >
                 (std::greater<double>(), rightBound),
                 mask, res);
            break;}
        case ibis::qExpr::OP_GE: {
            ierr = doComp
                (vals, std::bind2nd<std::greater_equal<double> >
                 (std::greater_equal<double>(), rightBound),
                 mask, res);
            break;}
        case ibis::qExpr::OP_EQ: {
            if (rightBound == rng.rightBound()) {
                ierr = doComp
                    (vals, std::bind2nd<std::equal_to<double> >
                     (std::equal_to<double>(), rightBound),
                     mask, res);
            }
            else {
                res.clear();
                ierr = 0;
            }
            break;}
        default: {
            res.clear();
            ierr = 0;
            break;}
        }
        break;}
    }

    if (ibis::gVerbose > 3) {
        timer.stop();
        ibis::util::logger lg;
        lg() << "part::doScan -- evaluating " << rng
             << " on " << mask.cnt() << " float "
             << (mask.cnt() > 1 ? " values" : " value") << " (total: "
             << mask.size() << ") took " << timer.realTime()
             << " sec elapsed time and produced " << res.size()
             << (res.size() > 1 ? " hits" : " hit");
#if DEBUG+0 > 1 || _DEBUG+0 > 1
        lg() << "\nmask\n" << mask;
        lg() << "\nhit vector\n" << hits << "\n";
#endif
    }
    return ierr;
} // ibis::part::doScan

/// Examine the range condition with in memory values.
/// A specialization of the template for double values.
template <>
long ibis::part::doScan(const array_t<double> &vals,
                        const ibis::qContinuousRange &rng,
                        const ibis::bitvector &mask,
                        array_t<double> &res) {
    long ierr = -2;
    ibis::horometer timer;
    if (ibis::gVerbose > 3)
        timer.start();
    const double leftBound = rng.leftBound();
    const double rightBound = rng.rightBound();
    const ibis::qExpr::COMPARE lop = rng.leftOperator();
    const ibis::qExpr::COMPARE rop = rng.rightOperator();
    if (lop == ibis::qExpr::OP_UNDEFINED &&
        rop == ibis::qExpr::OP_UNDEFINED) {
        res.reserve(mask.cnt());
        for (ibis::bitvector::indexSet is = mask.firstIndexSet();
             is.nIndices() > 0;
             ++ is) {
            const ibis::bitvector::word_t *ind = is.indices();
            if (is.isRange()) {
                for (ibis::bitvector::word_t j = ind[0]; j < ind[1]; ++ j)
                    res.push_back(vals[j]);
            }
            else {
                for (unsigned j = 0; j < is.nIndices(); ++ j)
                    res.push_back(vals[ind[j]]);
            }
        }
    }

    switch (lop) {
    case ibis::qExpr::OP_LT: {
        switch (rop) {
        case ibis::qExpr::OP_LT: {
            if (leftBound < rightBound) {
                ierr = doComp
                    (vals, std::bind1st<std::less<double> >
                     (std::less<double>(), leftBound),
                     std::bind2nd<std::less<double> >
                     (std::less<double>(), rightBound),
                     mask, res);
            }
            else {
                res.clear();
                ierr = 0;
            }
            break;}
        case ibis::qExpr::OP_LE: {
            if (leftBound < rightBound) {
                ierr = doComp
                    (vals, std::bind1st<std::less<double> >
                     (std::less<double>(), leftBound),
                     std::bind2nd<std::less_equal<double> >
                     (std::less_equal<double>(), rightBound),
                     mask, res);
            }
            else {
                res.clear();
                ierr = 0;
            }
            break;}
        case ibis::qExpr::OP_GT: {
            if (leftBound >= rightBound) {
                ierr = doComp
                    (vals, std::bind1st<std::less<double> >
                     (std::less<double>(), leftBound),
                     mask, res);
            }
            else {
                ierr = doComp
                    (vals, std::bind2nd<std::greater<double> >
                     (std::greater<double>(), rightBound),
                     mask, res);
            }
            break;}
        case ibis::qExpr::OP_GE: {
            if (leftBound >= rightBound) {
                ierr = doComp
                    (vals, std::bind1st<std::less<double> >
                     (std::less<double>(), leftBound),
                     mask, res);
            }
            else {
                ierr = doComp
                    (vals, std::bind2nd<std::greater_equal<double> >
                     (std::greater_equal<double>(), rightBound),
                     mask, res);
            }
            break;}
        case ibis::qExpr::OP_EQ: {
            if (leftBound < rightBound) {
                ierr = doComp
                    (vals, std::bind2nd<std::equal_to<double> >
                     (std::equal_to<double>(), rightBound),
                     mask, res);
            }
            else {
                res.clear();
                ierr = 0;
            }
            break;}
        default: {
            ierr = doComp
                (vals, std::bind1st<std::less<double> >
                 (std::less<double>(), leftBound),
                 mask, res);
            break;}
        }
        break;}
    case ibis::qExpr::OP_LE: {
        switch (rop) {
        case ibis::qExpr::OP_LT: {
            if (leftBound < rightBound) {
                ierr = doComp
                    (vals, std::bind1st<std::less_equal<double> >
                     (std::less_equal<double>(), leftBound),
                     std::bind2nd<std::less<double> >
                     (std::less<double>(), rightBound),
                     mask, res);
            }
            else {
                res.clear();
                ierr = 0;
            }
            break;}
        case ibis::qExpr::OP_LE: {
            if (leftBound <= rightBound) {
                ierr = doComp
                    (vals, std::bind1st<std::less_equal<double> >
                     (std::less_equal<double>(), leftBound),
                     std::bind2nd<std::less_equal<double> >
                     (std::less_equal<double>(), rightBound),
                     mask, res);
            }
            else {
                res.clear();
                ierr = 0;
            }
            break;}
        case ibis::qExpr::OP_GT: {
            if (leftBound > rightBound) {
                ierr = doComp
                    (vals, std::bind1st<std::less_equal<double> >
                     (std::less_equal<double>(), leftBound),
                     mask, res);
            }
            else {
                ierr = doComp
                    (vals, std::bind2nd<std::greater<double> >
                     (std::greater<double>(), rightBound),
                     mask, res);
            }
            break;}
        case ibis::qExpr::OP_GE: {
            if (leftBound >= rightBound) {
                ierr = doComp
                    (vals, std::bind1st<std::less_equal<double> >
                     (std::less_equal<double>(), leftBound),
                     mask, res);
            }
            else {
                ierr = doComp
                    (vals, std::bind2nd<std::greater_equal<double> >
                     (std::greater_equal<double>(), rightBound),
                     mask, res);
            }
            break;}
        case ibis::qExpr::OP_EQ: {
            if (leftBound <= rightBound) {
                ierr = doComp
                    (vals, std::bind2nd<std::equal_to<double> >
                     (std::equal_to<double>(), rightBound),
                     mask, res);
            }
            else {
                res.clear();
                ierr = 0;
            }
            break;}
        default: {
            ierr = doComp
                (vals, std::bind1st<std::less_equal<double> >
                 (std::less_equal<double>(), leftBound),
                 mask, res);
            break;}
        }
        break;}
    case ibis::qExpr::OP_GT: {
        switch (rop) {
        case ibis::qExpr::OP_LT: {
            if (leftBound <= rightBound) {
                ierr = doComp
                    (vals, std::bind1st< std::greater<double> >
                     (std::greater<double>(), leftBound),
                     mask, res);
            }
            else {
                ierr = doComp
                    (vals, std::bind2nd<std::less<double> >
                     (std::less<double>(), rightBound),
                     mask, res);
            }
            break;}
        case ibis::qExpr::OP_LE: {
            if (leftBound < rightBound) {
                ierr = doComp
                    (vals,
                     std::bind1st< std::greater<double> >
                     (std::greater<double>(), leftBound),
                     mask, res);
            }
            else {
                ierr = doComp
                    (vals,
                     std::bind2nd<std::less_equal<double> >
                     (std::less_equal<double>(), rightBound),
                     mask, res);
            }
            break;}
        case ibis::qExpr::OP_GT: {
            if (leftBound > rightBound) {
                ierr = doComp
                    (vals,
                     std::bind1st< std::greater<double> >
                     (std::greater<double>(), leftBound),
                     std::bind2nd<std::greater<double> >
                     (std::greater<double>(), rightBound),
                     mask, res);
            }
            else {
                res.clear();
                ierr = 0;
            }
            break;}
        case ibis::qExpr::OP_GE: {
            if (leftBound > rightBound) {
                ierr = doComp
                    (vals,
                     std::bind1st< std::greater<double> >
                     (std::greater<double>(), leftBound),
                     std::bind2nd<std::greater_equal<double> >
                     (std::greater_equal<double>(), rightBound),
                     mask, res);
            }
            else {
                res.clear();
                ierr = 0;
            }
            break;}
        case ibis::qExpr::OP_EQ: {
            if (rightBound < leftBound) {
                ierr = doComp
                    (vals,
                     std::bind2nd<std::equal_to<double> >
                     (std::equal_to<double>(), rightBound),
                     mask, res);
            }
            else {
                res.clear();
                ierr = 0;
            }
            break;}
        default: {
            ierr = doComp
                (vals,
                 std::bind1st< std::greater<double> >
                 (std::greater<double>(), leftBound),
                 mask, res);
            break;}
        }
        break;}
    case ibis::qExpr::OP_GE: {
        switch (rop) {
        case ibis::qExpr::OP_LT: {
            if (leftBound < rightBound) {
                ierr = doComp
                    (vals,
                     std::bind1st< std::greater_equal<double> >
                     (std::greater_equal<double>(), leftBound),
                     mask, res);
            }
            else {
                ierr = doComp
                    (vals,
                     std::bind2nd<std::less<double> >
                     (std::less<double>(), rightBound),
                     mask, res);
            }
            break;}
        case ibis::qExpr::OP_LE: {
            if (leftBound <= rightBound) {
                ierr = doComp
                    (vals, std::bind1st< std::greater_equal<double> >
                     (std::greater_equal<double>(), leftBound),
                     mask, res);
            }
            else {
                ierr = doComp
                    (vals,
                     std::bind2nd<std::less_equal<double> >
                     (std::less_equal<double>(), rightBound),
                     mask, res);
            }
            break;}
        case ibis::qExpr::OP_GT: {
            if (leftBound > rightBound) {
                ierr = doComp
                    (vals, std::bind1st< std::greater_equal<double> >
                     (std::greater_equal<double>(), leftBound),
                     std::bind2nd<std::greater<double> >
                     (std::greater<double>(), rightBound),
                     mask, res);
            }
            else {
                res.clear();
                ierr = 0;
            }
            break;}
        case ibis::qExpr::OP_GE: {
            if (leftBound >= rightBound) {
                ierr = doComp
                    (vals, std::bind1st< std::greater_equal<double> >
                     (std::greater_equal<double>(), leftBound),
                     std::bind2nd<std::greater_equal<double> >
                     (std::greater_equal<double>(), rightBound),
                     mask, res);
            }
            else {
                res.clear();
                ierr = 0;
            }
            break;}
        case ibis::qExpr::OP_EQ: {
            if (rightBound <= leftBound) {
                ierr = doComp
                    (vals, std::bind1st< std::greater_equal<double> >
                     (std::greater_equal<double>(), leftBound),
                     std::bind2nd<std::equal_to<double> >
                     (std::equal_to<double>(), rightBound),
                     mask, res);
            }
            else {
                res.clear();
                ierr = 0;
            }
            break;}
        default: {
            ierr = doComp
                (vals,
                 std::bind1st< std::greater_equal<double> >
                 (std::greater_equal<double>(), leftBound),
                 mask, res);
            break;}
        }
        break;}
    case ibis::qExpr::OP_EQ: {
        switch (rop) {
        case ibis::qExpr::OP_LT: {
            if (leftBound < rightBound) {
                ierr = doComp
                    (vals,
                     std::bind1st< std::equal_to<double> >
                     (std::equal_to<double>(), leftBound),
                     mask, res);
            }
            else {
                res.clear();
                ierr = 0;
            }
            break;}
        case ibis::qExpr::OP_LE: {
            if (leftBound <= rightBound) {
                ierr = doComp
                    (vals,
                     std::bind1st< std::equal_to<double> >
                     (std::equal_to<double>(), leftBound),
                     mask, res);
            }
            else {
                res.clear();
                ierr = 0;
            }
            break;}
        case ibis::qExpr::OP_GT: {
            if (leftBound > rightBound) {
                ierr = doComp
                    (vals,
                     std::bind1st< std::equal_to<double> >
                     (std::equal_to<double>(), leftBound),
                     mask, res);
            }
            else {
                res.clear();
                ierr = 0;
            }
            break;}
        case ibis::qExpr::OP_GE: {
            if (leftBound >= rightBound) {
                ierr = doComp
                    (vals,
                     std::bind1st< std::equal_to<double> >
                     (std::equal_to<double>(), leftBound),
                     mask, res);
            }
            else {
                res.clear();
                ierr = 0;
            }
            break;}
        case ibis::qExpr::OP_EQ: {
            if (leftBound == rightBound && rightBound == rng.rightBound()) {
                ierr = doComp
                    (vals,
                     std::bind1st< std::equal_to<double> >
                     (std::equal_to<double>(), leftBound),
                     mask, res);
            }
            else {
                res.clear();
                ierr = 0;
            }
            break;}
        default: {
            ierr = doComp
                (vals,
                 std::bind1st< std::equal_to<double> >
                 (std::equal_to<double>(), leftBound),
                 mask, res);
            break;}
        }
        break;}
    default: {
        switch (rop) {
        case ibis::qExpr::OP_LT: {
            ierr = doComp
                (vals,
                 std::bind2nd<std::less<double> >
                 (std::less<double>(), rightBound),
                 mask, res);
            break;}
        case ibis::qExpr::OP_LE: {
            ierr = doComp
                (vals,
                 std::bind2nd<std::less_equal<double> >
                 (std::less_equal<double>(), rightBound),
                 mask, res);
            break;}
        case ibis::qExpr::OP_GT: {
            ierr = doComp
                (vals,
                 std::bind2nd<std::greater<double> >
                 (std::greater<double>(), rightBound),
                 mask, res);
            break;}
        case ibis::qExpr::OP_GE: {
            ierr = doComp
                (vals,
                 std::bind2nd<std::greater_equal<double> >
                 (std::greater_equal<double>(), rightBound),
                 mask, res);
            break;}
        case ibis::qExpr::OP_EQ: {
            if (rightBound == rng.rightBound()) {
                ierr = doComp
                    (vals,
                     std::bind2nd<std::equal_to<double> >
                     (std::equal_to<double>(), rightBound),
                     mask, res);
            }
            else {
                res.clear();
                ierr = 0;
            }
            break;}
        default: {
            res.clear();
            ierr = 0;
            break;}
        }
        break;}
    }

    if (ibis::gVerbose > 3) {
        timer.stop();
        ibis::util::logger lg;
        lg() << "part::doScan -- evaluating " << rng
             << " on " << mask.cnt() << " double "
             << (mask.cnt() > 1 ? " values" : " value") << " (total: "
             << mask.size() << ") took " << timer.realTime()
             << " sec elapsed time and produced " << res.size()
             << (res.size() > 1 ? " hits" : " hit");
#if DEBUG+0 > 1 || _DEBUG+0 > 1
        lg() << "\nmask\n" << mask;
        lg() << "\nhit vector\n" << hits << "\n";
#endif
    }
    return ierr;
} // ibis::part::doScan

/// Examine the range condition with in memory values.
/// A specialization of the template for float arrays.  All comparisons are
/// performed as doubles.
template <>
long ibis::part::doScan(const array_t<float> &vals,
                        const ibis::qContinuousRange &rng,
                        const ibis::bitvector &mask,
                        array_t<float> &res, ibis::bitvector &hits) {
    long ierr = -2;
    ibis::horometer timer;
    if (ibis::gVerbose > 3)
        timer.start();
    const double leftBound = rng.leftBound();
    const double rightBound = rng.rightBound();
    const ibis::qExpr::COMPARE lop = rng.leftOperator();
    const ibis::qExpr::COMPARE rop = rng.rightOperator();
    if (lop == ibis::qExpr::OP_UNDEFINED &&
        rop == ibis::qExpr::OP_UNDEFINED) {
        hits.copy(mask);
        return hits.sloppyCount();
    }

    switch (lop) {
    case ibis::qExpr::OP_LT: {
        switch (rop) {
        case ibis::qExpr::OP_LT: {
            if (leftBound < rightBound) {
                ierr = doComp
                    (vals,
                     std::bind1st<std::less<double> >
                     (std::less<double>(), leftBound),
                     std::bind2nd<std::less<double> >
                     (std::less<double>(), rightBound),
                     mask, res, hits);
            }
            else {
                res.clear();
                ierr = 0;
            }
            break;}
        case ibis::qExpr::OP_LE: {
            if (leftBound < rightBound) {
                ierr = doComp
                    (vals,
                     std::bind1st<std::less<double> >
                     (std::less<double>(), leftBound),
                     std::bind2nd<std::less_equal<double> >
                     (std::less_equal<double>(), rightBound),
                     mask, res, hits);
            }
            else {
                res.clear();
                ierr = 0;
            }
            break;}
        case ibis::qExpr::OP_GT: {
            if (leftBound >= rightBound) {
                ierr = doComp
                    (vals,
                     std::bind1st<std::less<double> >
                     (std::less<double>(), leftBound),
                     mask, res, hits);
            }
            else {
                ierr = doComp
                    (vals,
                     std::bind2nd<std::greater<double> >
                     (std::greater<double>(), rightBound),
                     mask, res, hits);
            }
            break;}
        case ibis::qExpr::OP_GE: {
            if (leftBound >= rightBound) {
                ierr = doComp
                    (vals,
                     std::bind1st<std::less<double> >
                     (std::less<double>(), leftBound),
                     mask, res, hits);
            }
            else {
                ierr = doComp
                    (vals,
                     std::bind2nd<std::greater_equal<double> >
                     (std::greater_equal<double>(), rightBound),
                     mask, res, hits);
            }
            break;}
        case ibis::qExpr::OP_EQ: {
            if (leftBound < rightBound) {
                ierr = doComp
                    (vals,
                     std::bind2nd<std::equal_to<double> >
                     (std::equal_to<double>(), rightBound),
                     mask, res, hits);
            }
            else {
                res.clear();
                ierr = 0;
            }
            break;}
        default: {
            ierr = doComp
                (vals,
                 std::bind1st<std::less<double> >
                 (std::less<double>(), leftBound),
                 mask, res, hits);
            break;}
        }
        break;}
    case ibis::qExpr::OP_LE: {
        switch (rop) {
        case ibis::qExpr::OP_LT: {
            if (leftBound < rightBound) {
                ierr = doComp
                    (vals,
                     std::bind1st<std::less_equal<double> >
                     (std::less_equal<double>(), leftBound),
                     std::bind2nd<std::less<double> >
                     (std::less<double>(), rightBound),
                     mask, res, hits);
            }
            else {
                res.clear();
                ierr = 0;
            }
            break;}
        case ibis::qExpr::OP_LE: {
            if (leftBound <= rightBound) {
                ierr = doComp
                    (vals,
                     std::bind1st<std::less_equal<double> >
                     (std::less_equal<double>(), leftBound),
                     std::bind2nd<std::less_equal<double> >
                     (std::less_equal<double>(), rightBound),
                     mask, res, hits);
            }
            else {
                res.clear();
                ierr = 0;
            }
            break;}
        case ibis::qExpr::OP_GT: {
            if (leftBound > rightBound) {
                ierr = doComp
                    (vals,
                     std::bind1st<std::less_equal<double> >
                     (std::less_equal<double>(), leftBound),
                     mask, res, hits);
            }
            else {
                ierr = doComp
                    (vals,
                     std::bind2nd<std::greater<double> >
                     (std::greater<double>(), rightBound),
                     mask, res, hits);
            }
            break;}
        case ibis::qExpr::OP_GE: {
            if (leftBound >= rightBound) {
                ierr = doComp
                    (vals,
                     std::bind1st<std::less_equal<double> >
                     (std::less_equal<double>(), leftBound),
                     mask, res, hits);
            }
            else {
                ierr = doComp
                    (vals,
                     std::bind2nd<std::greater_equal<double> >
                     (std::greater_equal<double>(), rightBound),
                     mask, res, hits);
            }
            break;}
        case ibis::qExpr::OP_EQ: {
            if (leftBound <= rightBound) {
                ierr = doComp
                    (vals,
                     std::bind2nd<std::equal_to<double> >
                     (std::equal_to<double>(), rightBound),
                     mask, res, hits);
            }
            else {
                res.clear();
                ierr = 0;
            }
            break;}
        default: {
            ierr = doComp
                (vals,
                 std::bind1st<std::less_equal<double> >
                 (std::less_equal<double>(), leftBound),
                 mask, res, hits);
            break;}
        }
        break;}
    case ibis::qExpr::OP_GT: {
        switch (rop) {
        case ibis::qExpr::OP_LT: {
            if (leftBound <= rightBound) {
                ierr = doComp
                    (vals,
                     std::bind1st< std::greater<double> >
                     (std::greater<double>(), leftBound),
                     mask, res, hits);
            }
            else {
                ierr = doComp
                    (vals,
                     std::bind2nd<std::less<double> >
                     (std::less<double>(), rightBound),
                     mask, res, hits);
            }
            break;}
        case ibis::qExpr::OP_LE: {
            if (leftBound < rightBound) {
                ierr = doComp
                    (vals,
                     std::bind1st< std::greater<double> >
                     (std::greater<double>(), leftBound),
                     mask, res, hits);
            }
            else {
                ierr = doComp
                    (vals,
                     std::bind2nd<std::less_equal<double> >
                     (std::less_equal<double>(), rightBound),
                     mask, res, hits);
            }
            break;}
        case ibis::qExpr::OP_GT: {
            if (leftBound > rightBound) {
                ierr = doComp
                    (vals,
                     std::bind1st< std::greater<double> >
                     (std::greater<double>(), leftBound),
                     std::bind2nd<std::greater<double> >
                     (std::greater<double>(), rightBound),
                     mask, res, hits);
            }
            else {
                res.clear();
                ierr = 0;
            }
            break;}
        case ibis::qExpr::OP_GE: {
            if (leftBound > rightBound) {
                ierr = doComp
                    (vals,
                     std::bind1st< std::greater<double> >
                     (std::greater<double>(), leftBound),
                     std::bind2nd<std::greater_equal<double> >
                     (std::greater_equal<double>(), rightBound),
                     mask, res, hits);
            }
            else {
                res.clear();
                ierr = 0;
            }
            break;}
        case ibis::qExpr::OP_EQ: {
            if (rightBound < leftBound) {
                ierr = doComp
                    (vals,
                     std::bind2nd<std::equal_to<double> >
                     (std::equal_to<double>(), rightBound),
                     mask, res, hits);
            }
            else {
                res.clear();
                ierr = 0;
            }
            break;}
        default: {
            ierr = doComp
                (vals,
                 std::bind1st< std::greater<double> >
                 (std::greater<double>(), leftBound),
                 mask, res, hits);
            break;}
        }
        break;}
    case ibis::qExpr::OP_GE: {
        switch (rop) {
        case ibis::qExpr::OP_LT: {
            if (leftBound < rightBound) {
                ierr = doComp
                    (vals, std::bind1st< std::greater_equal<double> >
                     (std::greater_equal<double>(), leftBound),
                     mask, res, hits);
            }
            else {
                ierr = doComp
                    (vals,
                     std::bind2nd<std::less<double> >
                     (std::less<double>(), rightBound),
                     mask, res, hits);
            }
            break;}
        case ibis::qExpr::OP_LE: {
            if (leftBound <= rightBound) {
                ierr = doComp
                    (vals, std::bind1st< std::greater_equal<double> >
                     (std::greater_equal<double>(), leftBound),
                     mask, res, hits);
            }
            else {
                ierr = doComp
                    (vals,
                     std::bind2nd<std::less_equal<double> >
                     (std::less_equal<double>(), rightBound),
                     mask, res, hits);
            }
            break;}
        case ibis::qExpr::OP_GT: {
            if (leftBound > rightBound) {
                ierr = doComp
                    (vals, std::bind1st< std::greater_equal<double> >
                     (std::greater_equal<double>(), leftBound),
                     std::bind2nd<std::greater<double> >
                     (std::greater<double>(), rightBound),
                     mask, res, hits);
            }
            else {
                res.clear();
                ierr = 0;
            }
            break;}
        case ibis::qExpr::OP_GE: {
            if (leftBound >= rightBound) {
                ierr = doComp
                    (vals, std::bind1st< std::greater_equal<double> >
                     (std::greater_equal<double>(), leftBound),
                     std::bind2nd<std::greater_equal<double> >
                     (std::greater_equal<double>(), rightBound),
                     mask, res, hits);
            }
            else {
                res.clear();
                ierr = 0;
            }
            break;}
        case ibis::qExpr::OP_EQ: {
            if (rightBound <= leftBound) {
                ierr = doComp
                    (vals, std::bind1st< std::greater_equal<double> >
                     (std::greater_equal<double>(), leftBound),
                     std::bind2nd<std::equal_to<double> >
                     (std::equal_to<double>(), rightBound),
                     mask, res, hits);
            }
            else {
                res.clear();
                ierr = 0;
            }
            break;}
        default: {
            ierr = doComp
                (vals,
                 std::bind1st< std::greater_equal<double> >
                 (std::greater_equal<double>(), leftBound),
                 mask, res, hits);
            break;}
        }
        break;}
    case ibis::qExpr::OP_EQ: {
        switch (rop) {
        case ibis::qExpr::OP_LT: {
            if (leftBound < rightBound) {
                ierr = doComp
                    (vals,
                     std::bind1st< std::equal_to<double> >
                     (std::equal_to<double>(), leftBound),
                     mask, res, hits);
            }
            else {
                res.clear();
                ierr = 0;
            }
            break;}
        case ibis::qExpr::OP_LE: {
            if (leftBound <= rightBound) {
                ierr = doComp
                    (vals,
                     std::bind1st< std::equal_to<double> >
                     (std::equal_to<double>(), leftBound),
                     mask, res, hits);
            }
            else {
                res.clear();
                ierr = 0;
            }
            break;}
        case ibis::qExpr::OP_GT: {
            if (leftBound > rightBound) {
                ierr = doComp
                    (vals,
                     std::bind1st< std::equal_to<double> >
                     (std::equal_to<double>(), leftBound),
                     mask, res, hits);
            }
            else {
                res.clear();
                ierr = 0;
            }
            break;}
        case ibis::qExpr::OP_GE: {
            if (leftBound >= rightBound) {
                ierr = doComp
                    (vals,
                     std::bind1st< std::equal_to<double> >
                     (std::equal_to<double>(), leftBound),
                     mask, res, hits);
            }
            else {
                res.clear();
                ierr = 0;
            }
            break;}
        case ibis::qExpr::OP_EQ: {
            if (leftBound == rightBound && rightBound == rng.rightBound()) {
                ierr = doComp
                    (vals,
                     std::bind1st< std::equal_to<double> >
                     (std::equal_to<double>(), leftBound),
                     mask, res, hits);
            }
            else {
                res.clear();
                ierr = 0;
            }
            break;}
        default: {
            ierr = doComp
                (vals,
                 std::bind1st< std::equal_to<double> >
                 (std::equal_to<double>(), leftBound),
                 mask, res, hits);
            break;}
        }
        break;}
    default: {
        switch (rop) {
        case ibis::qExpr::OP_LT: {
            ierr = doComp
                (vals,
                 std::bind2nd<std::less<double> >
                 (std::less<double>(), rightBound),
                 mask, res, hits);
            break;}
        case ibis::qExpr::OP_LE: {
            ierr = doComp
                (vals,
                 std::bind2nd<std::less_equal<double> >
                 (std::less_equal<double>(), rightBound),
                 mask, res, hits);
            break;}
        case ibis::qExpr::OP_GT: {
            ierr = doComp
                (vals,
                 std::bind2nd<std::greater<double> >
                 (std::greater<double>(), rightBound),
                 mask, res, hits);
            break;}
        case ibis::qExpr::OP_GE: {
            ierr = doComp
                (vals,
                 std::bind2nd<std::greater_equal<double> >
                 (std::greater_equal<double>(), rightBound),
                 mask, res, hits);
            break;}
        case ibis::qExpr::OP_EQ: {
            if (rightBound == rng.rightBound()) {
                ierr = doComp
                    (vals,
                     std::bind2nd<std::equal_to<double> >
                     (std::equal_to<double>(), rightBound),
                     mask, res, hits);
            }
            else {
                res.clear();
                ierr = 0;
            }
            break;}
        default: {
            res.clear();
            ierr = 0;
            break;}
        }
        break;}
    }

    hits.adjustSize(0, mask.size());
    if (ibis::gVerbose > 3) {
        timer.stop();
        ibis::util::logger lg;
        lg() << "part::doScan -- evaluating " << rng
             << " on " << mask.cnt() << " float "
             << (mask.cnt() > 1 ? " values" : " value") << " (total: "
             << mask.size() << ") took " << timer.realTime()
             << " sec elapsed time and produced " << res.size()
             << (res.size() > 1 ? " hits" : " hit");
#if DEBUG+0 > 1 || _DEBUG+0 > 1
        lg() << "\nmask\n" << mask;
        lg() << "\nhit vector\n" << hits << "\n";
#endif
    }
    return ierr;
} // ibis::part::doScan

/// Examine the range condition with in memory values.
/// A specialization of the template for double values.
template <>
long ibis::part::doScan(const array_t<double> &vals,
                        const ibis::qContinuousRange &rng,
                        const ibis::bitvector &mask,
                        array_t<double> &res, ibis::bitvector &hits) {
    long ierr = -2;
    ibis::horometer timer;
    if (ibis::gVerbose > 3)
        timer.start();
    const double leftBound = rng.leftBound();
    const double rightBound = rng.rightBound();
    const ibis::qExpr::COMPARE lop = rng.leftOperator();
    const ibis::qExpr::COMPARE rop = rng.rightOperator();
    if (lop == ibis::qExpr::OP_UNDEFINED &&
        rop == ibis::qExpr::OP_UNDEFINED) {
        hits.copy(mask);
        return hits.sloppyCount();
    }

    switch (lop) {
    case ibis::qExpr::OP_LT: {
        switch (rop) {
        case ibis::qExpr::OP_LT: {
            if (leftBound < rightBound) {
                ierr = doComp
                    (vals,
                     std::bind1st<std::less<double> >
                     (std::less<double>(), leftBound),
                     std::bind2nd<std::less<double> >
                     (std::less<double>(), rightBound),
                     mask, res, hits);
            }
            else {
                res.clear();
                ierr = 0;
            }
            break;}
        case ibis::qExpr::OP_LE: {
            if (leftBound < rightBound) {
                ierr = doComp
                    (vals,
                     std::bind1st<std::less<double> >
                     (std::less<double>(), leftBound),
                     std::bind2nd<std::less_equal<double> >
                     (std::less_equal<double>(), rightBound),
                     mask, res, hits);
            }
            else {
                res.clear();
                ierr = 0;
            }
            break;}
        case ibis::qExpr::OP_GT: {
            if (leftBound >= rightBound) {
                ierr = doComp
                    (vals,
                     std::bind1st<std::less<double> >
                     (std::less<double>(), leftBound),
                     mask, res, hits);
            }
            else {
                ierr = doComp
                    (vals,
                     std::bind2nd<std::greater<double> >
                     (std::greater<double>(), rightBound),
                     mask, res, hits);
            }
            break;}
        case ibis::qExpr::OP_GE: {
            if (leftBound >= rightBound) {
                ierr = doComp
                    (vals,
                     std::bind1st<std::less<double> >
                     (std::less<double>(), leftBound),
                     mask, res, hits);
            }
            else {
                ierr = doComp
                    (vals,
                     std::bind2nd<std::greater_equal<double> >
                     (std::greater_equal<double>(), rightBound),
                     mask, res, hits);
            }
            break;}
        case ibis::qExpr::OP_EQ: {
            if (leftBound < rightBound) {
                ierr = doComp
                    (vals,
                     std::bind2nd<std::equal_to<double> >
                     (std::equal_to<double>(), rightBound),
                     mask, res, hits);
            }
            else {
                res.clear();
                ierr = 0;
            }
            break;}
        default: {
            ierr = doComp
                (vals,
                 std::bind1st<std::less<double> >
                 (std::less<double>(), leftBound),
                 mask, res, hits);
            break;}
        }
        break;}
    case ibis::qExpr::OP_LE: {
        switch (rop) {
        case ibis::qExpr::OP_LT: {
            if (leftBound < rightBound) {
                ierr = doComp
                    (vals,
                     std::bind1st<std::less_equal<double> >
                     (std::less_equal<double>(), leftBound),
                     std::bind2nd<std::less<double> >
                     (std::less<double>(), rightBound),
                     mask, res, hits);
            }
            else {
                res.clear();
                ierr = 0;
            }
            break;}
        case ibis::qExpr::OP_LE: {
            if (leftBound <= rightBound) {
                ierr = doComp
                    (vals,
                     std::bind1st<std::less_equal<double> >
                     (std::less_equal<double>(), leftBound),
                     std::bind2nd<std::less_equal<double> >
                     (std::less_equal<double>(), rightBound),
                     mask, res, hits);
            }
            else {
                res.clear();
                ierr = 0;
            }
            break;}
        case ibis::qExpr::OP_GT: {
            if (leftBound > rightBound) {
                ierr = doComp
                    (vals,
                     std::bind1st<std::less_equal<double> >
                     (std::less_equal<double>(), leftBound),
                     mask, res, hits);
            }
            else {
                ierr = doComp
                    (vals,
                     std::bind2nd<std::greater<double> >
                     (std::greater<double>(), rightBound),
                     mask, res, hits);
            }
            break;}
        case ibis::qExpr::OP_GE: {
            if (leftBound >= rightBound) {
                ierr = doComp
                    (vals,
                     std::bind1st<std::less_equal<double> >
                     (std::less_equal<double>(), leftBound),
                     mask, res, hits);
            }
            else {
                ierr = doComp
                    (vals,
                     std::bind2nd<std::greater_equal<double> >
                     (std::greater_equal<double>(), rightBound),
                     mask, res, hits);
            }
            break;}
        case ibis::qExpr::OP_EQ: {
            if (leftBound <= rightBound) {
                ierr = doComp
                    (vals,
                     std::bind2nd<std::equal_to<double> >
                     (std::equal_to<double>(), rightBound),
                     mask, res, hits);
            }
            else {
                res.clear();
                ierr = 0;
            }
            break;}
        default: {
            ierr = doComp
                (vals,
                 std::bind1st<std::less_equal<double> >
                 (std::less_equal<double>(), leftBound),
                 mask, res, hits);
            break;}
        }
        break;}
    case ibis::qExpr::OP_GT: {
        switch (rop) {
        case ibis::qExpr::OP_LT: {
            if (leftBound <= rightBound) {
                ierr = doComp
                    (vals,
                     std::bind1st< std::greater<double> >
                     (std::greater<double>(), leftBound),
                     mask, res, hits);
            }
            else {
                ierr = doComp
                    (vals,
                     std::bind2nd<std::less<double> >
                     (std::less<double>(), rightBound),
                     mask, res, hits);
            }
            break;}
        case ibis::qExpr::OP_LE: {
            if (leftBound < rightBound) {
                ierr = doComp
                    (vals,
                     std::bind1st< std::greater<double> >
                     (std::greater<double>(), leftBound),
                     mask, res, hits);
            }
            else {
                ierr = doComp
                    (vals,
                     std::bind2nd<std::less_equal<double> >
                     (std::less_equal<double>(), rightBound),
                     mask, res, hits);
            }
            break;}
        case ibis::qExpr::OP_GT: {
            if (leftBound > rightBound) {
                ierr = doComp
                    (vals,
                     std::bind1st< std::greater<double> >
                     (std::greater<double>(), leftBound),
                     std::bind2nd<std::greater<double> >
                     (std::greater<double>(), rightBound),
                     mask, res, hits);
            }
            else {
                res.clear();
                ierr = 0;
            }
            break;}
        case ibis::qExpr::OP_GE: {
            if (leftBound > rightBound) {
                ierr = doComp
                    (vals,
                     std::bind1st< std::greater<double> >
                     (std::greater<double>(), leftBound),
                     std::bind2nd<std::greater_equal<double> >
                     (std::greater_equal<double>(), rightBound),
                     mask, res, hits);
            }
            else {
                res.clear();
                ierr = 0;
            }
            break;}
        case ibis::qExpr::OP_EQ: {
            if (rightBound < leftBound) {
                ierr = doComp
                    (vals,
                     std::bind2nd<std::equal_to<double> >
                     (std::equal_to<double>(), rightBound),
                     mask, res, hits);
            }
            else {
                res.clear();
                ierr = 0;
            }
            break;}
        default: {
            ierr = doComp
                (vals,
                 std::bind1st< std::greater<double> >
                 (std::greater<double>(), leftBound),
                 mask, res, hits);
            break;}
        }
        break;}
    case ibis::qExpr::OP_GE: {
        switch (rop) {
        case ibis::qExpr::OP_LT: {
            if (leftBound < rightBound) {
                ierr = doComp
                    (vals, std::bind1st< std::greater_equal<double> >
                     (std::greater_equal<double>(), leftBound),
                     mask, res, hits);
            }
            else {
                ierr = doComp
                    (vals,
                     std::bind2nd<std::less<double> >
                     (std::less<double>(), rightBound),
                     mask, res, hits);
            }
            break;}
        case ibis::qExpr::OP_LE: {
            if (leftBound <= rightBound) {
                ierr = doComp
                    (vals, std::bind1st< std::greater_equal<double> >
                     (std::greater_equal<double>(), leftBound),
                     mask, res, hits);
            }
            else {
                ierr = doComp
                    (vals,
                     std::bind2nd<std::less_equal<double> >
                     (std::less_equal<double>(), rightBound),
                     mask, res, hits);
            }
            break;}
        case ibis::qExpr::OP_GT: {
            if (leftBound > rightBound) {
                ierr = doComp
                    (vals, std::bind1st< std::greater_equal<double> >
                     (std::greater_equal<double>(), leftBound),
                     std::bind2nd<std::greater<double> >
                     (std::greater<double>(), rightBound),
                     mask, res, hits);
            }
            else {
                res.clear();
                ierr = 0;
            }
            break;}
        case ibis::qExpr::OP_GE: {
            if (leftBound >= rightBound) {
                ierr = doComp
                    (vals, std::bind1st< std::greater_equal<double> >
                     (std::greater_equal<double>(), leftBound),
                     std::bind2nd<std::greater_equal<double> >
                     (std::greater_equal<double>(), rightBound),
                     mask, res, hits);
            }
            else {
                res.clear();
                ierr = 0;
            }
            break;}
        case ibis::qExpr::OP_EQ: {
            if (rightBound <= leftBound) {
                ierr = doComp
                    (vals, std::bind1st< std::greater_equal<double> >
                     (std::greater_equal<double>(), leftBound),
                     std::bind2nd<std::equal_to<double> >
                     (std::equal_to<double>(), rightBound),
                     mask, res, hits);
            }
            else {
                res.clear();
                ierr = 0;
            }
            break;}
        default: {
            ierr = doComp
                (vals,
                 std::bind1st< std::greater_equal<double> >
                 (std::greater_equal<double>(), leftBound),
                 mask, res, hits);
            break;}
        }
        break;}
    case ibis::qExpr::OP_EQ: {
        switch (rop) {
        case ibis::qExpr::OP_LT: {
            if (leftBound < rightBound) {
                ierr = doComp
                    (vals,
                     std::bind1st< std::equal_to<double> >
                     (std::equal_to<double>(), leftBound),
                     mask, res, hits);
            }
            else {
                res.clear();
                ierr = 0;
            }
            break;}
        case ibis::qExpr::OP_LE: {
            if (leftBound <= rightBound) {
                ierr = doComp
                    (vals,
                     std::bind1st< std::equal_to<double> >
                     (std::equal_to<double>(), leftBound),
                     mask, res, hits);
            }
            else {
                res.clear();
                ierr = 0;
            }
            break;}
        case ibis::qExpr::OP_GT: {
            if (leftBound > rightBound) {
                ierr = doComp
                    (vals,
                     std::bind1st< std::equal_to<double> >
                     (std::equal_to<double>(), leftBound),
                     mask, res, hits);
            }
            else {
                res.clear();
                ierr = 0;
            }
            break;}
        case ibis::qExpr::OP_GE: {
            if (leftBound >= rightBound) {
                ierr = doComp
                    (vals,
                     std::bind1st< std::equal_to<double> >
                     (std::equal_to<double>(), leftBound),
                     mask, res, hits);
            }
            else {
                res.clear();
                ierr = 0;
            }
            break;}
        case ibis::qExpr::OP_EQ: {
            if (leftBound == rightBound && rightBound == rng.rightBound()) {
                ierr = doComp
                    (vals,
                     std::bind1st< std::equal_to<double> >
                     (std::equal_to<double>(), leftBound),
                     mask, res, hits);
            }
            else {
                res.clear();
                ierr = 0;
            }
            break;}
        default: {
            ierr = doComp
                (vals,
                 std::bind1st< std::equal_to<double> >
                 (std::equal_to<double>(), leftBound),
                 mask, res, hits);
            break;}
        }
        break;}
    default: {
        switch (rop) {
        case ibis::qExpr::OP_LT: {
            ierr = doComp
                (vals,
                 std::bind2nd<std::less<double> >
                 (std::less<double>(), rightBound),
                 mask, res, hits);
            break;}
        case ibis::qExpr::OP_LE: {
            ierr = doComp
                (vals,
                 std::bind2nd<std::less_equal<double> >
                 (std::less_equal<double>(), rightBound),
                 mask, res, hits);
            break;}
        case ibis::qExpr::OP_GT: {
            ierr = doComp
                (vals,
                 std::bind2nd<std::greater<double> >
                 (std::greater<double>(), rightBound),
                 mask, res, hits);
            break;}
        case ibis::qExpr::OP_GE: {
            ierr = doComp
                (vals,
                 std::bind2nd<std::greater_equal<double> >
                 (std::greater_equal<double>(), rightBound),
                 mask, res, hits);
            break;}
        case ibis::qExpr::OP_EQ: {
            if (rightBound == rng.rightBound()) {
                ierr = doComp
                    (vals,
                     std::bind2nd<std::equal_to<double> >
                     (std::equal_to<double>(), rightBound),
                     mask, res, hits);
            }
            else {
                res.clear();
                ierr = 0;
            }
            break;}
        default: {
            res.clear();
            ierr = 0;
            break;}
        }
        break;}
    }

    hits.adjustSize(0, mask.size());
    if (ibis::gVerbose > 3) {
        timer.stop();
        ibis::util::logger lg;
        lg() << "part::doScan -- evaluating " << rng
             << " on " << mask.cnt() << " double "
             << (mask.cnt() > 1 ? " values" : " value") << " (total: "
             << mask.size() << ") took " << timer.realTime()
             << " sec elapsed time and produced " << res.size()
             << (res.size() > 1 ? " hits" : " hit");
#if DEBUG+0 > 1 || _DEBUG+0 > 1
        lg() << "\nmask\n" << mask;
        lg() << "\nhit vector\n" << hits << "\n";
#endif
    }
    return ierr;
} // ibis::part::doScan

/// Evaluate the range condition.  Accepts an externally passed comparison
/// operator.  It chooses whether the bitvector @c hits will be compressed
/// internally based on the number of set bits in the @c mask.
template <typename T, typename F>
long ibis::part::doComp(const array_t<T> &vals, F cmp,
                        const ibis::bitvector &mask,
                        ibis::bitvector &hits) {
    long ierr = 0;
    if (mask.size() == 0 || mask.cnt() == 0)
        return ierr;

    if (vals.size() != mask.size() && vals.size() != mask.cnt()) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- part::doComp<" << typeid(T).name() << ", "
            << typeid(F).name() << ">(vals[" << vals.size()
            << "]) -- vals.size() must be either mask.size(" << mask.size()
            << ") or mask.cnt(" << mask.cnt() << ")";
        ierr = -1;
        return ierr;
    }

    const bool uncomp = ((mask.size() >> 8) < mask.cnt());
    if (uncomp) { // use uncompressed hits internally
        hits.set(0, mask.size());
        hits.decompress();
    }
    else {
        hits.clear();
        hits.reserve(mask.size(), mask.cnt());
    }
    if (vals.size() == mask.size()) { // full list of values
        for (ibis::bitvector::indexSet ix = mask.firstIndexSet();
             ix.nIndices() > 0; ++ ix) {
            const ibis::bitvector::word_t *iix = ix.indices();
            if (ix.isRange()) {
                for (unsigned j = *iix; j < iix[1]; ++ j) {
                    if (cmp(vals[j]))
                        hits.setBit(j, 1);
                }
            }
            else {
                for (unsigned j = 0; j < ix.nIndices(); ++ j) {
                    if (cmp(vals[iix[j]]))
                        hits.setBit(iix[j], 1);
                }
            }
        }
    }
    else { // compacted values
        unsigned ival = 0;
        for (ibis::bitvector::indexSet ix = mask.firstIndexSet();
             ix.nIndices() > 0; ++ ix) {
            const ibis::bitvector::word_t *iix = ix.indices();
            if (ix.isRange()) {
                for (unsigned j = *iix; j < iix[1]; ++ j) {
                    if (cmp(vals[ival]))
                        hits.setBit(j, 1);
                    ++ ival;
                }
            }
            else {
                for (unsigned j = 0; j < ix.nIndices(); ++ j) {
                    if (cmp(vals[ival]))
                        hits.setBit(iix[j], 1);
                    ++ ival;
                }
            }
        }
    }

    if (uncomp)
        hits.compress();
    else
        hits.adjustSize(0, mask.size());

    ierr = hits.sloppyCount();
    return ierr;
} // ibis::part::doComp

/// Evaluate the range condition.  The actual comparison function is
/// only applied on rows with mask == 1.
/// This version uses a uncompressed bitvector to store the scan results
/// internally.
template <typename T, typename F>
long ibis::part::doComp0(const array_t<T> &vals, F cmp,
                         const ibis::bitvector &mask,
                         ibis::bitvector &hits) {
    long ierr = 0;
    if (mask.size() == 0 || mask.cnt() == 0)
        return ierr;

    if (vals.size() != mask.size() && vals.size() != mask.cnt()) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- part::doComp0<" << typeid(T).name() << ", "
            << typeid(F).name() << ">(vals[" << vals.size()
            << "]) -- vals.size() must be either mask.size(" << mask.size()
            << ") or mask.cnt(" << mask.cnt() << ")";
        ierr = -1;
        return ierr;
    }

    hits.set(0, mask.size());
    hits.decompress();
    if (vals.size() == mask.size()) { // full list of values
        for (ibis::bitvector::indexSet ix = mask.firstIndexSet();
             ix.nIndices() > 0; ++ ix) {
            const ibis::bitvector::word_t *iix = ix.indices();
            if (ix.isRange()) {
                for (unsigned j = *iix; j < iix[1]; ++ j) {
                    if (cmp(vals[j]))
                        hits.turnOnRawBit(j);
                }
            }
            else {
                for (unsigned j = 0; j < ix.nIndices(); ++ j) {
                    if (cmp(vals[iix[j]]))
                        hits.turnOnRawBit(iix[j]);
                }
            }
        }
    }
    else { // compacted values
        unsigned ival = 0;
        for (ibis::bitvector::indexSet ix = mask.firstIndexSet();
             ix.nIndices() > 0; ++ ix) {
            const ibis::bitvector::word_t *iix = ix.indices();
            if (ix.isRange()) {
                for (unsigned j = *iix; j < iix[1]; ++ j) {
                    if (cmp(vals[ival]))
                        hits.turnOnRawBit(j);
                    ++ ival;
                }
            }
            else {
                for (unsigned j = 0; j < ix.nIndices(); ++ j) {
                    if (cmp(vals[ival]))
                        hits.turnOnRawBit(iix[j]);
                    ++ ival;
                }
            }
        }
    }

    hits.compress();
    ierr = hits.sloppyCount();
    return ierr;
} // ibis::part::doComp0

/// Evaluate the range condition.  The actual comparison functions are
/// only applied on rows with mask == 1.
/// The actual scan function.  This one chooses whether the internal
/// bitvector for storing the scan results will be compressed or not.  It
/// always returns a compressed bitvector.
template <typename T, typename F1, typename F2>
long ibis::part::doComp(const array_t<T> &vals, F1 cmp1, F2 cmp2,
                        const ibis::bitvector &mask,
                        ibis::bitvector &hits) {
    long ierr = 0;
    if (mask.size() == 0 || mask.cnt() == 0)
        return ierr;

    if (vals.size() != mask.size() && vals.size() != mask.cnt()) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- part::doComp<" << typeid(T).name() << ", "
            << typeid(F1).name() << ", " << typeid(F2).name() << ">(vals["
            << vals.size() << "]) -- vals.size() must be either mask.size("
            << mask.size() << ") or mask.cnt(" << mask.cnt() << ")";
        ierr = -1;
        return ierr;
    }

    const bool uncomp = ((mask.size() >> 8) < mask.cnt());
    if (uncomp) { // use uncompressed hits internally
        hits.set(0, mask.size());
        hits.decompress();
    }
    else {
        hits.clear();
        hits.reserve(mask.size(), mask.cnt());
    }
    if (vals.size() == mask.size()) { // full list of values
        for (ibis::bitvector::indexSet ix = mask.firstIndexSet();
             ix.nIndices() > 0; ++ ix) {
            const ibis::bitvector::word_t *iix = ix.indices();
            if (ix.isRange()) {
                for (unsigned j = *iix; j < iix[1]; ++ j) {
                    if (cmp1(vals[j]) && cmp2(vals[j]))
                        hits.setBit(j, 1);
                }
            }
            else {
                for (unsigned j = 0; j < ix.nIndices(); ++ j) {
                    if (cmp1(vals[iix[j]]) && cmp2(vals[iix[j]]))
                        hits.setBit(iix[j], 1);
                }
            }
        }
    }
    else { // compacted values
        unsigned ival = 0;
        for (ibis::bitvector::indexSet ix = mask.firstIndexSet();
             ix.nIndices() > 0; ++ ix) {
            const ibis::bitvector::word_t *iix = ix.indices();
            if (ix.isRange()) {
                for (unsigned j = *iix; j < iix[1]; ++ j) {
                    if (cmp1(vals[ival]) && cmp2(vals[ival]))
                        hits.setBit(j, 1);
                    ++ ival;
                }
            }
            else {
                for (unsigned j = 0; j < ix.nIndices(); ++ j) {
                    if (cmp1(vals[ival]) && cmp2(vals[ival]))
                        hits.setBit(iix[j], 1);
                    ++ ival;
                }
            }
        }
    }

    if (uncomp)
        hits.compress();
    else
        hits.adjustSize(0, mask.size());
    ierr = hits.sloppyCount();
    return ierr;
} // ibis::part::doComp

/// This version uses uncompressed bitvector to store the scan results
/// internally.
template <typename T, typename F1, typename F2>
long ibis::part::doComp0(const array_t<T> &vals, F1 cmp1, F2 cmp2,
                         const ibis::bitvector &mask,
                         ibis::bitvector &hits) {
    long ierr = 0;
    if (mask.size() == 0 || mask.cnt() == 0)
        return ierr;

    if (vals.size() != mask.size() && vals.size() != mask.cnt()) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- part::doComp0<" << typeid(T).name() << ", "
            << typeid(F1).name() << ", " << typeid(F2).name() << ">(vals["
            << vals.size() << "]) -- vals.size() must be either mask.size("
            << mask.size() << ") or mask.cnt(" << mask.cnt() << ")";
        ierr = -1;
        return ierr;
    }

    hits.set(0, mask.size());
    hits.decompress();
    if (vals.size() == mask.size()) { // full list of values
        for (ibis::bitvector::indexSet ix = mask.firstIndexSet();
             ix.nIndices() > 0; ++ ix) {
            const ibis::bitvector::word_t *iix = ix.indices();
            if (ix.isRange()) {
                for (unsigned j = *iix; j < iix[1]; ++ j) {
                    if (cmp1(vals[j]) && cmp2(vals[j]))
                        hits.turnOnRawBit(j);
                }
            }
            else {
                for (unsigned j = 0; j < ix.nIndices(); ++ j) {
                    if (cmp1(vals[iix[j]]) && cmp2(vals[iix[j]]))
                        hits.turnOnRawBit(iix[j]);
                }
            }
        }
    }
    else { // compacted values
        unsigned ival = 0;
        for (ibis::bitvector::indexSet ix = mask.firstIndexSet();
             ix.nIndices() > 0; ++ ix) {
            const ibis::bitvector::word_t *iix = ix.indices();
            if (ix.isRange()) {
                for (unsigned j = *iix; j < iix[1]; ++ j) {
                    if (cmp1(vals[ival]) && cmp2(vals[ival]))
                        hits.turnOnRawBit(j);
                    ++ ival;
                }
            }
            else {
                for (unsigned j = 0; j < ix.nIndices(); ++ j) {
                    if (cmp1(vals[ival]) && cmp2(vals[ival]))
                        hits.turnOnRawBit(iix[j]);
                    ++ ival;
                }
            }
        }
    }

    hits.compress();
    ierr = hits.sloppyCount();
    return ierr;
} // ibis::part::doComp0

/// Evaluate the range condition.  Accepts an externally passed comparison
/// operator.  It chooses whether the bitvector @c hits will be compressed
/// internally based on the number of set bits in the @c mask.
template <typename T, typename F>
long ibis::part::doComp(const array_t<T> &vals, F cmp,
                        const ibis::bitvector &mask,
                        array_t<T> &res) {
    res.clear();
    long ierr = 0;
    if (mask.size() == 0 || mask.cnt() == 0)
        return ierr;

    if (vals.size() != mask.size() && vals.size() != mask.cnt()) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- part::doComp<" << typeid(T).name() << ", "
            << typeid(F).name() << ">(vals[" << vals.size()
            << "]) -- vals.size() must be either mask.size(" << mask.size()
            << ") or mask.cnt(" << mask.cnt() << ")";
        ierr = -1;
        return ierr;
    }

    res.nosharing();
    if (res.capacity() < mask.cnt())
        res.reserve(mask.cnt() >> 1); // reserve space
    if (vals.size() == mask.size()) { // full list of values
        for (ibis::bitvector::indexSet ix = mask.firstIndexSet();
             ix.nIndices() > 0; ++ ix) {
            const ibis::bitvector::word_t *iix = ix.indices();
            if (ix.isRange()) {
                for (unsigned j = *iix; j < iix[1]; ++ j) {
                    if (cmp(vals[j]))
                        res.push_back(vals[j]);
                }
            }
            else {
                for (unsigned j = 0; j < ix.nIndices(); ++ j) {
                    if (cmp(vals[iix[j]]))
                        res.push_back(vals[iix[j]]);
                }
            }
        }
    }
    else { // compacted values
        unsigned ival = 0; // vals[ival]
        for (ibis::bitvector::indexSet ix = mask.firstIndexSet();
             ix.nIndices() > 0; ++ ix) {
            const ibis::bitvector::word_t *iix = ix.indices();
            if (ix.isRange()) {
                for (unsigned j = *iix; j < iix[1]; ++ j) {
                    if (cmp(vals[ival]))
                        res.push_back(vals[ival]);
                    ++ ival;
                }
            }
            else {
                for (unsigned j = 0; j < ix.nIndices(); ++ j) {
                    if (cmp(vals[ival]))
                        res.push_back(vals[ival]);
                    ++ ival;
                }
            }
        }
    }

    ierr = res.size();
    return ierr;
} // ibis::part::doComp

/// Evaluate the range condition.  Accepts an externally passed comparison
/// operator.  It chooses whether the bitvector @c hits will be compressed
/// internally based on the number of set bits in the @c mask.
template <typename T, typename F>
long ibis::part::doComp(const array_t<T> &vals, F cmp,
                        const ibis::bitvector &mask,
                        array_t<T> &res, ibis::bitvector& hits) {
    res.clear();
    hits.clear();
    long ierr = 0;
    if (mask.size() == 0 || mask.cnt() == 0) {
        hits.copy(mask);
        return ierr;
    }

    if (vals.size() != mask.size() && vals.size() != mask.cnt()) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- part::doComp<" << typeid(T).name() << ", "
            << typeid(F).name() << ">(vals[" << vals.size()
            << "]) -- vals.size() must be either mask.size(" << mask.size()
            << ") or mask.cnt(" << mask.cnt() << ")";
        ierr = -1;
        return ierr;
    }

    res.nosharing();
    if (res.capacity() < mask.cnt())
        res.reserve(mask.cnt() >> 1); // reserve space
    if (vals.size() == mask.size()) { // full list of values
        for (ibis::bitvector::indexSet ix = mask.firstIndexSet();
             ix.nIndices() > 0; ++ ix) {
            const ibis::bitvector::word_t *iix = ix.indices();
            if (ix.isRange()) {
                for (unsigned j = *iix; j < iix[1]; ++ j) {
                    if (cmp(vals[j])) {
                        res.push_back(vals[j]);
                        hits.setBit(j, 1);
                    }
                }
            }
            else {
                for (unsigned j = 0; j < ix.nIndices(); ++ j) {
                    if (cmp(vals[iix[j]])) {
                        res.push_back(vals[iix[j]]);
                        hits.setBit(iix[j], 1);
                    }
                }
            }
        }
    }
    else { // compacted values
        unsigned ival = 0; // vals[ival]
        for (ibis::bitvector::indexSet ix = mask.firstIndexSet();
             ix.nIndices() > 0; ++ ix) {
            const ibis::bitvector::word_t *iix = ix.indices();
            if (ix.isRange()) {
                for (unsigned j = *iix; j < iix[1]; ++ j) {
                    if (cmp(vals[ival])) {
                        res.push_back(vals[ival]);
                        hits.setBit(j, 1);
                    }
                    ++ ival;
                }
            }
            else {
                for (unsigned j = 0; j < ix.nIndices(); ++ j) {
                    if (cmp(vals[ival])) {
                        res.push_back(vals[ival]);
                        hits.setBit(iix[j], 1);
                    }
                    ++ ival;
                }
            }
        }
    }

    ierr = res.size();
    return ierr;
} // ibis::part::doComp

/// Evaluate the range condition.  The actual comparison functions are only
/// applied on rows with mask == 1.  The values satisfying the comparison
/// operators are stored in res.  This function reserves enough space in
/// res for about half of the set bits in mask to avoid repeat reallocation
/// of space for res.  This space reservation will likely increase memory
/// usage.
template <typename T, typename F1, typename F2>
long ibis::part::doComp(const array_t<T> &vals, F1 cmp1, F2 cmp2,
                        const ibis::bitvector &mask,
                        array_t<T> &res) {
    res.clear();
    long ierr = 0;
    if (mask.size() == 0 || mask.cnt() == 0)
        return ierr;

    if (vals.size() != mask.size() && vals.size() != mask.cnt()) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- part::doComp<" << typeid(T).name() << ", "
            << typeid(F1).name() << ", " << typeid(F2).name() << ">(vals["
            << vals.size() << "]) -- vals.size() must be either mask.size("
            << mask.size() << ") or mask.cnt(" << mask.cnt() << ")";
        ierr = -1;
        return ierr;
    }

    res.nosharing();
    if (res.capacity() < mask.cnt())
        res.reserve(mask.cnt() >> 1); // reserve space
    if (vals.size() == mask.size()) { // full list of values
        for (ibis::bitvector::indexSet ix = mask.firstIndexSet();
             ix.nIndices() > 0; ++ ix) {
            const ibis::bitvector::word_t *iix = ix.indices();
            if (ix.isRange()) {
                for (unsigned j = *iix; j < iix[1]; ++ j) {
                    if (cmp1(vals[j]) && cmp2(vals[j]))
                        res.push_back(vals[j]);
                }
            }
            else {
                for (unsigned j = 0; j < ix.nIndices(); ++ j) {
                    if (cmp1(vals[iix[j]]) && cmp2(vals[iix[j]]))
                        res.push_back(vals[iix[j]]);
                }
            }
        }
    }
    else { // compacted values
        unsigned ival = 0; // position in the compacted vals
        for (ibis::bitvector::indexSet ix = mask.firstIndexSet();
             ix.nIndices() > 0; ++ ix) {
            const ibis::bitvector::word_t *iix = ix.indices();
            if (ix.isRange()) {
                for (unsigned j = *iix; j < iix[1]; ++ j) {
                    if (cmp1(vals[ival]) && cmp2(vals[ival]))
                        res.push_back(vals[ival]);
                    ++ ival;
                }
            }
            else {
                for (unsigned j = 0; j < ix.nIndices(); ++ j) {
                    if (cmp1(vals[ival]) && cmp2(vals[ival]))
                        res.push_back(vals[ival]);
                    ++ ival;
                }
            }
        }
    }

    ierr = res.size();
    return ierr;
} // ibis::part::doComp

/// Evaluate the range condition.  The actual comparison functions are only
/// applied on rows with mask == 1.  The values satisfying the comparison
/// operators are stored in res.  This function reserves enough space in
/// res for about half of the set bits in mask to avoid repeat reallocation
/// of space for res.  This space reservation will likely increase memory
/// usage.
template <typename T, typename F1, typename F2>
long ibis::part::doComp(const array_t<T> &vals, F1 cmp1, F2 cmp2,
                        const ibis::bitvector &mask,
                        array_t<T> &res, ibis::bitvector &hits) {
    res.clear();
    hits.clear();
    long ierr = 0;
    if (mask.size() == 0 || mask.cnt() == 0) {
        hits.copy(mask);
        return ierr;
    }

    if (vals.size() != mask.size() && vals.size() != mask.cnt()) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- part::doComp<" << typeid(T).name() << ", "
            << typeid(F1).name() << ", " << typeid(F2).name() << ">(vals["
            << vals.size() << "]) -- vals.size() must be either mask.size("
            << mask.size() << ") or mask.cnt(" << mask.cnt() << ")";
        ierr = -1;
        return ierr;
    }

    res.nosharing();
    if (res.capacity() < mask.cnt())
        res.reserve(mask.cnt() >> 1); // reserve space
    if (vals.size() == mask.size()) { // full list of values
        for (ibis::bitvector::indexSet ix = mask.firstIndexSet();
             ix.nIndices() > 0; ++ ix) {
            const ibis::bitvector::word_t *iix = ix.indices();
            if (ix.isRange()) {
                for (unsigned j = *iix; j < iix[1]; ++ j) {
                    if (cmp1(vals[j]) && cmp2(vals[j])) {
                        res.push_back(vals[j]);
                        hits.setBit(j, 1);
                    }
                }
            }
            else {
                for (unsigned j = 0; j < ix.nIndices(); ++ j) {
                    if (cmp1(vals[iix[j]]) && cmp2(vals[iix[j]])) {
                        res.push_back(vals[iix[j]]);
                        hits.setBit(iix[j], 1);
                    }
                }
            }
        }
    }
    else { // compacted values
        unsigned ival = 0; // position in the compacted vals
        for (ibis::bitvector::indexSet ix = mask.firstIndexSet();
             ix.nIndices() > 0; ++ ix) {
            const ibis::bitvector::word_t *iix = ix.indices();
            if (ix.isRange()) {
                for (unsigned j = *iix; j < iix[1]; ++ j) {
                    if (cmp1(vals[ival]) && cmp2(vals[ival])) {
                        res.push_back(vals[ival]);
                        hits.setBit(j, 1);
                    }
                    ++ ival;
                }
            }
            else {
                for (unsigned j = 0; j < ix.nIndices(); ++ j) {
                    if (cmp1(vals[ival]) && cmp2(vals[ival])) {
                        res.push_back(vals[ival]);
                        hits.setBit(iix[j], 1);
                    }
                    ++ ival;
                }
            }
        }
    }

    hits.adjustSize(0, mask.size());
    ierr = res.size();
    return ierr;
} // ibis::part::doComp

/// Count the number of hits for a single range condition.
long ibis::part::countHits(const ibis::qRange &cmp) const {
    const ibis::column* col = getColumn(cmp.colName());
    if (col == 0) {
        logWarning("countHits", "unknown column %s in the range expression",
                   cmp.colName());
        return -1;
    }

    long ierr = 0;
    ibis::horometer timer;
    if (ibis::gVerbose > 3)
        timer.start();
    switch (col->type()) {
    case ibis::UBYTE:
        ierr = doCount<unsigned char>(cmp);
        break;
    case ibis::BYTE:
        ierr = doCount<signed char>(cmp);
        break;
    case ibis::USHORT:
        ierr = doCount<uint16_t>(cmp);
        break;
    case ibis::SHORT:
        ierr = doCount<int16_t>(cmp);
        break;
    case ibis::UINT:
        ierr = doCount<uint32_t>(cmp);
        break;
    case ibis::INT:
        ierr = doCount<int32_t>(cmp);
        break;
    case ibis::ULONG:
        ierr = doCount<uint64_t>(cmp);
        break;
    case ibis::LONG:
        ierr = doCount<int64_t>(cmp);
        break;
    case ibis::FLOAT:
        ierr = doCount<float>(cmp);
        break;
    case ibis::DOUBLE:
        ierr = doCount<double>(cmp);
        break;
    default:
        if (ibis::gVerbose >= 0)
            logWarning("countHits", "does not support type %d (%s)",
                       static_cast<int>(col->type()),
                       cmp.colName());
        ierr = -4;
        break;
    }

    if (ibis::gVerbose > 3) {
        timer.stop();
        ibis::util::logger lg;
        lg() << "part[" << (m_name ? m_name : "?")
             << "]::countHits -- evaluating a condition involving "
             << cmp.colName() << " on " << nEvents << " records took "
             << timer.realTime()
             << " sec elapsed time and produced "
             << ierr << (ierr > 1 ? " hits" : " hit") << "\n";
    }
    return ierr;
} // ibis::part::countHits

/// Count the number rows satisfying the range expression.
template <typename T>
long ibis::part::doCount(const ibis::qRange &cmp) const {
    const ibis::column* col = getColumn(cmp.colName());
    if (col == 0) {
        return -1;
    }

    array_t<T> vals;
    long ierr = col->getValuesArray(&vals);
    if (ierr < 0) {
        return -3;
    }
    ibis::bitvector mask;
    col->getNullMask(mask);
    mask.adjustSize(0, vals.size());
    if (cmp.getType() == ibis::qExpr::INTHOD) {
        return doCount(vals, static_cast<const ibis::qIntHod&>(cmp), mask);
    }
    else if (cmp.getType() == ibis::qExpr::UINTHOD) {
        return doCount(vals, static_cast<const ibis::qUIntHod&>(cmp), mask);
    }
    else if (cmp.getType() != ibis::qExpr::RANGE) { // not a simple range
        return doCount(vals, cmp, mask);
    }

    // a simple continuous range expression
    const ibis::qContinuousRange &rng =
        static_cast<const ibis::qContinuousRange&>(cmp);
    T leftBound, rightBound;
    ibis::qExpr::COMPARE lop = rng.leftOperator();
    ibis::qExpr::COMPARE rop = rng.rightOperator();
    if (lop == ibis::qExpr::OP_UNDEFINED &&
        rop == ibis::qExpr::OP_UNDEFINED) {
        return mask.cnt();
    }

    switch (rng.leftOperator()) {
    case ibis::qExpr::OP_UNDEFINED:
        leftBound = 0;
        break;
    case ibis::qExpr::OP_LT:
        if (rng.leftBound() < std::numeric_limits<T>::min()) {
            leftBound = std::numeric_limits<T>::min();
            lop = ibis::qExpr::OP_LE;
        }
        else if (rng.leftBound() > std::numeric_limits<T>::max()) {
            leftBound = std::numeric_limits<T>::max();
        }
        else {
            leftBound = (T) rng.leftBound();
        }
        break;
    case ibis::qExpr::OP_LE:
        if (rng.leftBound() < std::numeric_limits<T>::min()) {
            leftBound = std::numeric_limits<T>::min();
            lop = ibis::qExpr::OP_LE;
        }
        else if (rng.leftBound() > std::numeric_limits<T>::max()) {
            leftBound = std::numeric_limits<T>::max();
            lop = ibis::qExpr::OP_LT;
        }
        else {
            leftBound = (T) rng.leftBound();
            if (leftBound != rng.leftBound())
                lop = ibis::qExpr::OP_LT;
        }
        break;
    case ibis::qExpr::OP_GE:
        if (rng.leftBound() < std::numeric_limits<T>::min()) {
            leftBound = std::numeric_limits<T>::min();
            lop = ibis::qExpr::OP_GT;
        }
        else if (rng.leftBound() > std::numeric_limits<T>::max()) {
            leftBound = std::numeric_limits<T>::max();
        }
        else {
            leftBound = (T) rng.leftBound();
        }
        break;
    case ibis::qExpr::OP_GT:
        if (rng.leftBound() < std::numeric_limits<T>::min()) {
            leftBound = std::numeric_limits<T>::min();
        }
        else if (rng.leftBound() > std::numeric_limits<T>::max()) {
            leftBound = std::numeric_limits<T>::max();
            lop = ibis::qExpr::OP_GE;
        }
        else {
            leftBound = (T) rng.leftBound();
            if (leftBound != rng.leftBound())
                lop = ibis::qExpr::OP_GE;
        }
        break;
    default:
        ibis::util::round_down(rng.leftBound(), leftBound);
        break;
    }
    switch (rng.rightOperator()) {
    case ibis::qExpr::OP_UNDEFINED:
        rightBound = 0;
        break;
    case ibis::qExpr::OP_LE:
        if (rng.rightBound() < std::numeric_limits<T>::min()) {
            rightBound = std::numeric_limits<T>::min();
            rop = ibis::qExpr::OP_LT;
        }
        else if (rng.rightBound() > std::numeric_limits<T>::max()) {
            rightBound = std::numeric_limits<T>::max();
        }
        else {
            rightBound = (T) rng.rightBound();
        }
        break;
    case ibis::qExpr::OP_LT:
        if (rng.rightBound() < std::numeric_limits<T>::min()) {
            rightBound = std::numeric_limits<T>::min();
        }
        else if (rng.rightBound() > std::numeric_limits<T>::max()) {
            rightBound = std::numeric_limits<T>::max();
            rop = ibis::qExpr::OP_LE;
        }
        else {
            rightBound = (T) rng.rightBound();
            if (rightBound < rng.rightBound())
                rop = ibis::qExpr::OP_LE;
        }
        break;
    case ibis::qExpr::OP_GE:
        if (rng.rightBound() < std::numeric_limits<T>::min()) {
            rightBound = std::numeric_limits<T>::min();
        }
        else if (rng.rightBound() > std::numeric_limits<T>::max()) {
            rightBound = std::numeric_limits<T>::max();
            rop = ibis::qExpr::OP_GT;
        }
        else {
            rightBound = (T) rng.rightBound();
            if (rightBound < rng.rightBound())
                rop = ibis::qExpr::OP_GT;
        }
        break;
    case ibis::qExpr::OP_GT:
        if (rng.rightBound() < std::numeric_limits<T>::min()) {
            rightBound = std::numeric_limits<T>::min();
            rop = ibis::qExpr::OP_GE;
        }
        else if (rng.rightBound() > std::numeric_limits<T>::max()) {
            rightBound = std::numeric_limits<T>::max();
        }
        else {
            rightBound = (T) rng.rightBound();
        }
        break;
    default:
        ibis::util::round_down(rng.rightBound(), rightBound);
        break;
    }


    switch (lop) {
    case ibis::qExpr::OP_LT: {
        switch (rop) {
        case ibis::qExpr::OP_LT: {
            if (leftBound < rightBound)
                ierr = doCount(vals, mask,
                               std::bind1st<std::less<T> >
                               (std::less<T>(), leftBound),
                               std::bind2nd<std::less<T> >
                               (std::less<T>(), rightBound));
            else
                ierr = 0;
            break;}
        case ibis::qExpr::OP_LE: {
            if (leftBound < rightBound)
                ierr = doCount(vals, mask,
                               std::bind1st<std::less<T> >
                               (std::less<T>(), leftBound),
                               std::bind2nd<std::less_equal<T> >
                               (std::less_equal<T>(), rightBound));
            else
                ierr = 0;
            break;}
        case ibis::qExpr::OP_GT: {
            if (leftBound >= rightBound)
                ierr = doCount(vals, mask,
                               std::bind1st<std::less<T> >
                               (std::less<T>(), leftBound));
            else
                ierr = doCount(vals, mask,
                               std::bind2nd<std::greater<T> >
                               (std::greater<T>(), rightBound));
            break;}
        case ibis::qExpr::OP_GE: {
            if (leftBound > rightBound)
                ierr = doCount(vals, mask,
                               std::bind1st<std::less<T> >
                               (std::less<T>(), leftBound));
            else
                ierr = doCount(vals, mask,
                               std::bind2nd<std::greater_equal<T> >
                               (std::greater_equal<T>(), rightBound));
            break;}
        case ibis::qExpr::OP_EQ: {
            if (rightBound == rng.rightBound() && leftBound < rightBound) {
                ierr = doCount(vals, mask,
                               std::bind2nd<std::equal_to<T> >
                               (std::equal_to<T>(), rightBound));
            }
            else {
                ierr = 0;
            }
            break;}
        default: {
            ierr = doCount(vals, mask,
                           std::bind1st<std::less<T> >
                           (std::less<T>(), leftBound));
            break;}
        }
        break;}
    case ibis::qExpr::OP_LE: {
        switch (rop) {
        case ibis::qExpr::OP_LT: {
            if (leftBound < rightBound)
                ierr = doCount(vals, mask,
                               std::bind1st<std::less_equal<T> >
                               (std::less_equal<T>(), leftBound),
                               std::bind2nd<std::less<T> >
                               (std::less<T>(), rightBound));
            else
                ierr = 0;
            break;}
        case ibis::qExpr::OP_LE: {
            if (leftBound <= rightBound)
                ierr = doCount(vals, mask,
                               std::bind1st<std::less_equal<T> >
                               (std::less_equal<T>(), leftBound),
                               std::bind2nd<std::less_equal<T> >
                               (std::less_equal<T>(), rightBound));
            else
                ierr = 0;
            break;}
        case ibis::qExpr::OP_GT: {
            if (leftBound > rightBound)
                ierr = doCount(vals, mask,
                               std::bind1st<std::less_equal<T> >
                               (std::less_equal<T>(), leftBound));
            else
                ierr = doCount(vals, mask,
                               std::bind2nd<std::greater<T> >
                               (std::greater<T>(), rightBound));
            break;}
        case ibis::qExpr::OP_GE: {
            if (leftBound >= rightBound)
                ierr = doCount(vals, mask,
                               std::bind1st<std::less_equal<T> >
                               (std::less_equal<T>(), leftBound));
            else
                ierr = doCount(vals, mask,
                               std::bind2nd<std::greater_equal<T> >
                               (std::greater_equal<T>(), rightBound));
            break;}
        case ibis::qExpr::OP_EQ: {
            if (rightBound == rng.rightBound() && leftBound <= rightBound) {
                ierr = doCount(vals, mask,
                               std::bind2nd<std::equal_to<T> >
                               (std::equal_to<T>(), rightBound));
            }
            else {
                ierr = 0;
            }
            break;}
        default: {
            ierr = doCount(vals, mask,
                           std::bind1st<std::less_equal<T> >
                           (std::less_equal<T>(), leftBound));
            break;}
        }
        break;}
    case ibis::qExpr::OP_GT: {
        switch (rop) {
        case ibis::qExpr::OP_LT: {
            if (leftBound <= rightBound)
                ierr = doCount(vals, mask,
                               std::bind1st< std::greater<T> >
                               (std::greater<T>(), leftBound));
            else
                ierr = doCount(vals, mask,
                               std::bind2nd<std::less<T> >
                               (std::less<T>(), rightBound));
            break;}
        case ibis::qExpr::OP_LE: {
            if (leftBound <= rightBound)
                ierr = doCount(vals, mask,
                               std::bind1st< std::greater<T> >
                               (std::greater<T>(), leftBound));
            else
                ierr = doCount(vals, mask,
                               std::bind2nd<std::less_equal<T> >
                               (std::less_equal<T>(), rightBound));
            break;}
        case ibis::qExpr::OP_GT: {
            if (leftBound > rightBound)
                ierr = doCount(vals, mask,
                               std::bind1st< std::greater<T> >
                               (std::greater<T>(), leftBound),
                               std::bind2nd<std::greater<T> >
                               (std::greater<T>(), rightBound));
            else
                ierr = 0;
            break;}
        case ibis::qExpr::OP_GE: {
            if (leftBound > rightBound)
                ierr = doCount(vals, mask,
                               std::bind1st< std::greater<T> >
                               (std::greater<T>(), leftBound),
                               std::bind2nd<std::greater_equal<T> >
                               (std::greater_equal<T>(), rightBound));
            else
                ierr = 0;
            break;}
        case ibis::qExpr::OP_EQ: {
            if (rightBound == rng.rightBound() && rightBound < leftBound) {
                ierr = doCount(vals, mask,
                               std::bind2nd<std::equal_to<T> >
                               (std::equal_to<T>(), rightBound));
            }
            else {
                ierr = 0;
            }
            break;}
        default: {
            ierr = doCount(vals, mask,
                           std::bind1st< std::greater<T> >
                           (std::greater<T>(), leftBound));
            break;}
        }
        break;}
    case ibis::qExpr::OP_GE: {
        switch (rop) {
        case ibis::qExpr::OP_LT: {
            if (leftBound < rightBound)
                ierr = doCount(vals, mask,
                               std::bind1st< std::greater_equal<T> >
                               (std::greater_equal<T>(), leftBound));
            else
                ierr = doCount(vals, mask,
                               std::bind2nd<std::less<T> >
                               (std::less<T>(), rightBound));
            break;}
        case ibis::qExpr::OP_LE: {
            if (leftBound <= rightBound)
                ierr = doCount(vals, mask,
                               std::bind1st< std::greater_equal<T> >
                               (std::greater_equal<T>(), leftBound));
            else
                ierr = doCount(vals, mask,
                               std::bind2nd<std::less_equal<T> >
                               (std::less_equal<T>(), rightBound));
            break;}
        case ibis::qExpr::OP_GT: {
            if (leftBound > rightBound)
                ierr = doCount(vals, mask,
                               std::bind1st< std::greater_equal<T> >
                               (std::greater_equal<T>(), leftBound),
                               std::bind2nd<std::greater<T> >
                               (std::greater<T>(), rightBound));
            else
                ierr = 0;
            break;}
        case ibis::qExpr::OP_GE: {
            if (leftBound >= rightBound)
                ierr = doCount(vals, mask,
                               std::bind1st< std::greater_equal<T> >
                               (std::greater_equal<T>(), leftBound),
                               std::bind2nd<std::greater_equal<T> >
                               (std::greater_equal<T>(), rightBound));
            else
                ierr = 0;
            break;}
        case ibis::qExpr::OP_EQ: {
            if (rightBound == rng.rightBound() && leftBound > rightBound) {
                ierr = doCount(vals, mask,
                               std::bind1st< std::greater_equal<T> >
                               (std::greater_equal<T>(), leftBound),
                               std::bind2nd<std::equal_to<T> >
                               (std::equal_to<T>(), rightBound));
            }
            else {
                ierr = 0;
            }
            break;}
        default: {
            ierr = doCount(vals, mask,
                           std::bind1st< std::greater_equal<T> >
                           (std::greater_equal<T>(), leftBound));
            break;}
        }
        break;}
    case ibis::qExpr::OP_EQ: {
        if (leftBound == rng.leftBound()) {
            switch (rop) {
            case ibis::qExpr::OP_LT: {
                if (leftBound < rightBound) {
                    ierr = doCount(vals, mask,
                                   std::bind1st< std::equal_to<T> >
                                   (std::equal_to<T>(), leftBound));
                }
                else {
                    ierr = 0;
                }
                break;}
            case ibis::qExpr::OP_LE: {
                if (leftBound <= rightBound) {
                    ierr = doCount(vals, mask,
                                   std::bind1st< std::equal_to<T> >
                                   (std::equal_to<T>(), leftBound));
                }
                else {
                    ierr = 0;
                }
                break;}
            case ibis::qExpr::OP_GT: {
                if (leftBound > rightBound) {
                    ierr = doCount(vals, mask,
                                   std::bind1st< std::equal_to<T> >
                                   (std::equal_to<T>(), leftBound));
                }
                else {
                    ierr = 0;
                }
                break;}
            case ibis::qExpr::OP_GE: {
                if (leftBound >= rightBound) {
                    ierr = doCount(vals, mask,
                                   std::bind1st< std::equal_to<T> >
                                   (std::equal_to<T>(), leftBound));
                }
                else {
                    ierr = 0;
                }
                break;}
            case ibis::qExpr::OP_EQ: {
                if (leftBound == rightBound && rightBound == rng.rightBound()) {
                    ierr = doCount(vals, mask,
                                   std::bind1st< std::equal_to<T> >
                                   (std::equal_to<T>(), leftBound));
                }
                else {
                    ierr = 0;
                }
                break;}
            default: {
                ierr = doCount(vals, mask,
                               std::bind1st< std::equal_to<T> >
                               (std::equal_to<T>(), leftBound));
                break;}
            }
        }
        else {
            ierr = 0;
        }
        break;}
    default: {
        switch (rop) {
        case ibis::qExpr::OP_LT: {
            ierr = doCount(vals, mask,
                           std::bind2nd<std::less<T> >
                           (std::less<T>(), rightBound));
            break;}
        case ibis::qExpr::OP_LE: {
            ierr = doCount(vals, mask,
                           std::bind2nd<std::less_equal<T> >
                           (std::less_equal<T>(), rightBound));
            break;}
        case ibis::qExpr::OP_GT: {
            ierr = doCount(vals, mask,
                           std::bind2nd<std::greater<T> >
                           (std::greater<T>(), rightBound));
            break;}
        case ibis::qExpr::OP_GE: {
            ierr = doCount(vals, mask,
                           std::bind2nd<std::greater_equal<T> >
                           (std::greater_equal<T>(), rightBound));
            break;}
        case ibis::qExpr::OP_EQ: {
            if (rightBound == rng.rightBound()) {
                ierr = doCount(vals, mask,
                               std::bind2nd<std::equal_to<T> >
                               (std::equal_to<T>(), rightBound));
            }
            else {
                ierr = 0;
            }
            break;}
        default: {
            ierr = 0;
            break;}
        }
        break;}
    }
    return ierr;
} // ibis::part::doCount

/// A specialization of template part::doCount for float values.  Note that
/// the comparison are performed as doubles.
template <>
long ibis::part::doCount<float>(const ibis::qRange &cmp) const {
    const ibis::column* col = getColumn(cmp.colName());
    if (col == 0) {
        return -1;
    }

    array_t<float> vals;
    long ierr = col->getValuesArray(&vals);
    if (ierr < 0) {
        return -3;
    }
    ibis::bitvector mask;
    col->getNullMask(mask);
    mask.adjustSize(0, vals.size());
    if (cmp.getType() != ibis::qExpr::RANGE) { // not a simple range
        return doCount(vals, cmp, mask);
    }

    // a simple continuous range expression
    const ibis::qContinuousRange &rng =
        static_cast<const ibis::qContinuousRange&>(cmp);
    const double leftBound = rng.leftBound();
    const double rightBound = rng.rightBound();
    const ibis::qExpr::COMPARE lop = rng.leftOperator();
    const ibis::qExpr::COMPARE rop = rng.rightOperator();
    if (lop == ibis::qExpr::OP_UNDEFINED &&
        rop == ibis::qExpr::OP_UNDEFINED) {
        return mask.cnt();
    }

    switch (lop) {
    case ibis::qExpr::OP_LT: {
        switch (rop) {
        case ibis::qExpr::OP_LT: {
            if (leftBound < rightBound)
                ierr = doCount(vals, mask,
                               std::bind1st<std::less<double> >
                               (std::less<double>(), leftBound),
                               std::bind2nd<std::less<double> >
                               (std::less<double>(), rightBound));
            else
                ierr = 0;
            break;}
        case ibis::qExpr::OP_LE: {
            if (leftBound < rightBound)
                ierr = doCount(vals, mask,
                               std::bind1st<std::less<double> >
                               (std::less<double>(), leftBound),
                               std::bind2nd<std::less_equal<double> >
                               (std::less_equal<double>(), rightBound));
            else
                ierr = 0;
            break;}
        case ibis::qExpr::OP_GT: {
            if (leftBound >= rightBound)
                ierr = doCount(vals, mask,
                               std::bind1st<std::less<double> >
                               (std::less<double>(), leftBound));
            else
                ierr = doCount(vals, mask,
                               std::bind2nd<std::greater<double> >
                               (std::greater<double>(), rightBound));
            break;}
        case ibis::qExpr::OP_GE: {
            if (leftBound > rightBound)
                ierr = doCount(vals, mask,
                               std::bind1st<std::less<double> >
                               (std::less<double>(), leftBound));
            else
                ierr = doCount(vals, mask,
                               std::bind2nd<std::greater_equal<double> >
                               (std::greater_equal<double>(), rightBound));
            break;}
        case ibis::qExpr::OP_EQ: {
            if (leftBound < rightBound) {
                ierr = doCount(vals, mask,
                               std::bind2nd<std::equal_to<double> >
                               (std::equal_to<double>(), rightBound));
            }
            else {
                ierr = 0;
            }
            break;}
        default: {
            ierr = doCount(vals, mask,
                           std::bind1st<std::less<double> >
                           (std::less<double>(), leftBound));
            break;}
        }
        break;}
    case ibis::qExpr::OP_LE: {
        switch (rop) {
        case ibis::qExpr::OP_LT: {
            if (leftBound < rightBound)
                ierr = doCount(vals, mask,
                               std::bind1st<std::less_equal<double> >
                               (std::less_equal<double>(), leftBound),
                               std::bind2nd<std::less<double> >
                               (std::less<double>(), rightBound));
            else
                ierr = 0;
            break;}
        case ibis::qExpr::OP_LE: {
            if (leftBound <= rightBound)
                ierr = doCount(vals, mask,
                               std::bind1st<std::less_equal<double> >
                               (std::less_equal<double>(), leftBound),
                               std::bind2nd<std::less_equal<double> >
                               (std::less_equal<double>(), rightBound));
            else
                ierr = 0;
            break;}
        case ibis::qExpr::OP_GT: {
            if (leftBound > rightBound)
                ierr = doCount(vals, mask,
                               std::bind1st<std::less_equal<double> >
                               (std::less_equal<double>(), leftBound));
            else
                ierr = doCount(vals, mask,
                               std::bind2nd<std::greater<double> >
                               (std::greater<double>(), rightBound));
            break;}
        case ibis::qExpr::OP_GE: {
            if (leftBound >= rightBound)
                ierr = doCount(vals, mask,
                               std::bind1st<std::less_equal<double> >
                               (std::less_equal<double>(), leftBound));
            else
                ierr = doCount(vals, mask,
                               std::bind2nd<std::greater_equal<double> >
                               (std::greater_equal<double>(), rightBound));
            break;}
        case ibis::qExpr::OP_EQ: {
            if (leftBound <= rightBound) {
                ierr = doCount(vals, mask,
                               std::bind2nd<std::equal_to<double> >
                               (std::equal_to<double>(), rightBound));
            }
            else {
                ierr = 0;
            }
            break;}
        default: {
            ierr = doCount(vals, mask,
                           std::bind1st<std::less_equal<double> >
                           (std::less_equal<double>(), leftBound));
            break;}
        }
        break;}
    case ibis::qExpr::OP_GT: {
        switch (rop) {
        case ibis::qExpr::OP_LT: {
            if (leftBound <= rightBound)
                ierr = doCount(vals, mask,
                               std::bind1st< std::greater<double> >
                               (std::greater<double>(), leftBound));
            else
                ierr = doCount(vals, mask,
                               std::bind2nd<std::less<double> >
                               (std::less<double>(), rightBound));
            break;}
        case ibis::qExpr::OP_LE: {
            if (leftBound <= rightBound)
                ierr = doCount(vals, mask,
                               std::bind1st< std::greater<double> >
                               (std::greater<double>(), leftBound));
            else
                ierr = doCount(vals, mask,
                               std::bind2nd<std::less_equal<double> >
                               (std::less_equal<double>(), rightBound));
            break;}
        case ibis::qExpr::OP_GT: {
            if (leftBound > rightBound)
                ierr = doCount(vals, mask,
                               std::bind1st< std::greater<double> >
                               (std::greater<double>(), leftBound),
                               std::bind2nd<std::greater<double> >
                               (std::greater<double>(), rightBound));
            else
                ierr = 0;
            break;}
        case ibis::qExpr::OP_GE: {
            if (leftBound > rightBound)
                ierr = doCount(vals, mask,
                               std::bind1st< std::greater<double> >
                               (std::greater<double>(), leftBound),
                               std::bind2nd<std::greater_equal<double> >
                               (std::greater_equal<double>(), rightBound));
            else
                ierr = 0;
            break;}
        case ibis::qExpr::OP_EQ: {
            if (rightBound < leftBound) {
                ierr = doCount(vals, mask,
                               std::bind2nd<std::equal_to<double> >
                               (std::equal_to<double>(), rightBound));
            }
            else {
                ierr = 0;
            }
            break;}
        default: {
            ierr = doCount(vals, mask,
                           std::bind1st< std::greater<double> >
                           (std::greater<double>(), leftBound));
            break;}
        }
        break;}
    case ibis::qExpr::OP_GE: {
        switch (rop) {
        case ibis::qExpr::OP_LT: {
            if (leftBound < rightBound)
                ierr = doCount(vals, mask,
                               std::bind1st< std::greater_equal<double> >
                               (std::greater_equal<double>(), leftBound));
            else
                ierr = doCount(vals, mask,
                               std::bind2nd<std::less<double> >
                               (std::less<double>(), rightBound));
            break;}
        case ibis::qExpr::OP_LE: {
            if (leftBound <= rightBound)
                ierr = doCount(vals, mask,
                               std::bind1st< std::greater_equal<double> >
                               (std::greater_equal<double>(), leftBound));
            else
                ierr = doCount(vals, mask,
                               std::bind2nd<std::less_equal<double> >
                               (std::less_equal<double>(), rightBound));
            break;}
        case ibis::qExpr::OP_GT: {
            if (leftBound > rightBound)
                ierr = doCount(vals, mask,
                               std::bind1st< std::greater_equal<double> >
                               (std::greater_equal<double>(), leftBound),
                               std::bind2nd<std::greater<double> >
                               (std::greater<double>(), rightBound));
            else
                ierr = 0;
            break;}
        case ibis::qExpr::OP_GE: {
            if (leftBound >= rightBound)
                ierr = doCount(vals, mask,
                               std::bind1st< std::greater_equal<double> >
                               (std::greater_equal<double>(), leftBound),
                               std::bind2nd<std::greater_equal<double> >
                               (std::greater_equal<double>(), rightBound));
            else
                ierr = 0;
            break;}
        case ibis::qExpr::OP_EQ: {
            if (leftBound > rightBound) {
                ierr = doCount(vals, mask,
                               std::bind1st< std::greater_equal<double> >
                               (std::greater_equal<double>(), leftBound),
                               std::bind2nd<std::equal_to<double> >
                               (std::equal_to<double>(), rightBound));
            }
            else {
                ierr = 0;
            }
            break;}
        default: {
            ierr = doCount(vals, mask,
                           std::bind1st< std::greater_equal<double> >
                           (std::greater_equal<double>(), leftBound));
            break;}
        }
        break;}
    case ibis::qExpr::OP_EQ: {
        switch (rop) {
        case ibis::qExpr::OP_LT: {
            if (leftBound < rightBound) {
                ierr = doCount(vals, mask,
                               std::bind1st< std::equal_to<double> >
                               (std::equal_to<double>(), leftBound));
            }
            else {
                ierr = 0;
            }
            break;}
        case ibis::qExpr::OP_LE: {
            if (leftBound <= rightBound) {
                ierr = doCount(vals, mask,
                               std::bind1st< std::equal_to<double> >
                               (std::equal_to<double>(), leftBound));
            }
            else {
                ierr = 0;
            }
            break;}
        case ibis::qExpr::OP_GT: {
            if (leftBound > rightBound) {
                ierr = doCount(vals, mask,
                               std::bind1st< std::equal_to<double> >
                               (std::equal_to<double>(), leftBound));
            }
            else {
                ierr = 0;
            }
            break;}
        case ibis::qExpr::OP_GE: {
            if (leftBound >= rightBound) {
                ierr = doCount(vals, mask,
                               std::bind1st< std::equal_to<double> >
                               (std::equal_to<double>(), leftBound));
            }
            else {
                ierr = 0;
            }
            break;}
        case ibis::qExpr::OP_EQ: {
            if (leftBound == rightBound && rightBound == rng.rightBound()) {
                ierr = doCount(vals, mask,
                               std::bind1st< std::equal_to<double> >
                               (std::equal_to<double>(), leftBound));
            }
            else {
                ierr = 0;
            }
            break;}
        default: {
            ierr = doCount(vals, mask,
                           std::bind1st< std::equal_to<double> >
                           (std::equal_to<double>(), leftBound));
            break;}
        }
        break;}
    default: {
        switch (rop) {
        case ibis::qExpr::OP_LT: {
            ierr = doCount(vals, mask,
                           std::bind2nd<std::less<double> >
                           (std::less<double>(), rightBound));
            break;}
        case ibis::qExpr::OP_LE: {
            ierr = doCount(vals, mask,
                           std::bind2nd<std::less_equal<double> >
                           (std::less_equal<double>(), rightBound));
            break;}
        case ibis::qExpr::OP_GT: {
            ierr = doCount(vals, mask,
                           std::bind2nd<std::greater<double> >
                           (std::greater<double>(), rightBound));
            break;}
        case ibis::qExpr::OP_GE: {
            ierr = doCount(vals, mask,
                           std::bind2nd<std::greater_equal<double> >
                           (std::greater_equal<double>(), rightBound));
            break;}
        case ibis::qExpr::OP_EQ: {
            if (rightBound == rng.rightBound()) {
                ierr = doCount(vals, mask,
                               std::bind2nd<std::equal_to<double> >
                               (std::equal_to<double>(), rightBound));
            }
            else {
                ierr = 0;
            }
            break;}
        default: {
            ierr = 0;
            break;}
        }
        break;}
    }
    return ierr;
} // ibis::part::doCount

/// A specialization of template part::doCount for double values.
template <>
long ibis::part::doCount<double>(const ibis::qRange &cmp) const {
    const ibis::column* col = getColumn(cmp.colName());
    if (col == 0) {
        return -1;
    }

    array_t<double> vals;
    long ierr = col->getValuesArray(&vals);
    if (ierr < 0) {
        return -3;
    }
    ibis::bitvector mask;
    col->getNullMask(mask);
    mask.adjustSize(0, vals.size());
    if (cmp.getType() != ibis::qExpr::RANGE) { // not a simple range
        return doCount(vals, cmp, mask);
    }

    // a simple continuous range expression
    const ibis::qContinuousRange &rng =
        static_cast<const ibis::qContinuousRange&>(cmp);
    const double leftBound = rng.leftBound();
    const double rightBound = rng.rightBound();
    const ibis::qExpr::COMPARE lop = rng.leftOperator();
    const ibis::qExpr::COMPARE rop = rng.rightOperator();
    if (lop == ibis::qExpr::OP_UNDEFINED &&
        rop == ibis::qExpr::OP_UNDEFINED) {
        return mask.cnt();
    }

    switch (lop) {
    case ibis::qExpr::OP_LT: {
        switch (rop) {
        case ibis::qExpr::OP_LT: {
            if (leftBound < rightBound)
                ierr = doCount(vals, mask,
                               std::bind1st<std::less<double> >
                               (std::less<double>(), leftBound),
                               std::bind2nd<std::less<double> >
                               (std::less<double>(), rightBound));
            else
                ierr = 0;
            break;}
        case ibis::qExpr::OP_LE: {
            if (leftBound < rightBound)
                ierr = doCount(vals, mask,
                               std::bind1st<std::less<double> >
                               (std::less<double>(), leftBound),
                               std::bind2nd<std::less_equal<double> >
                               (std::less_equal<double>(), rightBound));
            else
                ierr = 0;
            break;}
        case ibis::qExpr::OP_GT: {
            if (leftBound >= rightBound)
                ierr = doCount(vals, mask,
                               std::bind1st<std::less<double> >
                               (std::less<double>(), leftBound));
            else
                ierr = doCount(vals, mask,
                               std::bind2nd<std::greater<double> >
                               (std::greater<double>(), rightBound));
            break;}
        case ibis::qExpr::OP_GE: {
            if (leftBound > rightBound)
                ierr = doCount(vals, mask,
                               std::bind1st<std::less<double> >
                               (std::less<double>(), leftBound));
            else
                ierr = doCount(vals, mask,
                               std::bind2nd<std::greater_equal<double> >
                               (std::greater_equal<double>(), rightBound));
            break;}
        case ibis::qExpr::OP_EQ: {
            if (leftBound < rightBound) {
                ierr = doCount(vals, mask,
                               std::bind2nd<std::equal_to<double> >
                               (std::equal_to<double>(), rightBound));
            }
            else {
                ierr = 0;
            }
            break;}
        default: {
            ierr = doCount(vals, mask,
                           std::bind1st<std::less<double> >
                           (std::less<double>(), leftBound));
            break;}
        }
        break;}
    case ibis::qExpr::OP_LE: {
        switch (rop) {
        case ibis::qExpr::OP_LT: {
            if (leftBound < rightBound)
                ierr = doCount(vals, mask,
                               std::bind1st<std::less_equal<double> >
                               (std::less_equal<double>(), leftBound),
                               std::bind2nd<std::less<double> >
                               (std::less<double>(), rightBound));
            else
                ierr = 0;
            break;}
        case ibis::qExpr::OP_LE: {
            if (leftBound <= rightBound)
                ierr = doCount(vals, mask,
                               std::bind1st<std::less_equal<double> >
                               (std::less_equal<double>(), leftBound),
                               std::bind2nd<std::less_equal<double> >
                               (std::less_equal<double>(), rightBound));
            else
                ierr = 0;
            break;}
        case ibis::qExpr::OP_GT: {
            if (leftBound > rightBound)
                ierr = doCount(vals, mask,
                               std::bind1st<std::less_equal<double> >
                               (std::less_equal<double>(), leftBound));
            else
                ierr = doCount(vals, mask,
                               std::bind2nd<std::greater<double> >
                               (std::greater<double>(), rightBound));
            break;}
        case ibis::qExpr::OP_GE: {
            if (leftBound >= rightBound)
                ierr = doCount(vals, mask,
                               std::bind1st<std::less_equal<double> >
                               (std::less_equal<double>(), leftBound));
            else
                ierr = doCount(vals, mask,
                               std::bind2nd<std::greater_equal<double> >
                               (std::greater_equal<double>(), rightBound));
            break;}
        case ibis::qExpr::OP_EQ: {
            if (leftBound <= rightBound) {
                ierr = doCount(vals, mask,
                               std::bind2nd<std::equal_to<double> >
                               (std::equal_to<double>(), rightBound));
            }
            else {
                ierr = 0;
            }
            break;}
        default: {
            ierr = doCount(vals, mask,
                           std::bind1st<std::less_equal<double> >
                           (std::less_equal<double>(), leftBound));
            break;}
        }
        break;}
    case ibis::qExpr::OP_GT: {
        switch (rop) {
        case ibis::qExpr::OP_LT: {
            if (leftBound <= rightBound)
                ierr = doCount(vals, mask,
                               std::bind1st< std::greater<double> >
                               (std::greater<double>(), leftBound));
            else
                ierr = doCount(vals, mask,
                               std::bind2nd<std::less<double> >
                               (std::less<double>(), rightBound));
            break;}
        case ibis::qExpr::OP_LE: {
            if (leftBound <= rightBound)
                ierr = doCount(vals, mask,
                               std::bind1st< std::greater<double> >
                               (std::greater<double>(), leftBound));
            else
                ierr = doCount(vals, mask,
                               std::bind2nd<std::less_equal<double> >
                               (std::less_equal<double>(), rightBound));
            break;}
        case ibis::qExpr::OP_GT: {
            if (leftBound > rightBound)
                ierr = doCount(vals, mask,
                               std::bind1st< std::greater<double> >
                               (std::greater<double>(), leftBound),
                               std::bind2nd<std::greater<double> >
                               (std::greater<double>(), rightBound));
            else
                ierr = 0;
            break;}
        case ibis::qExpr::OP_GE: {
            if (leftBound > rightBound)
                ierr = doCount(vals, mask,
                               std::bind1st< std::greater<double> >
                               (std::greater<double>(), leftBound),
                               std::bind2nd<std::greater_equal<double> >
                               (std::greater_equal<double>(), rightBound));
            else
                ierr = 0;
            break;}
        case ibis::qExpr::OP_EQ: {
            if (rightBound < leftBound) {
                ierr = doCount(vals, mask,
                               std::bind2nd<std::equal_to<double> >
                               (std::equal_to<double>(), rightBound));
            }
            else {
                ierr = 0;
            }
            break;}
        default: {
            ierr = doCount(vals, mask,
                           std::bind1st< std::greater<double> >
                           (std::greater<double>(), leftBound));
            break;}
        }
        break;}
    case ibis::qExpr::OP_GE: {
        switch (rop) {
        case ibis::qExpr::OP_LT: {
            if (leftBound < rightBound)
                ierr = doCount(vals, mask,
                               std::bind1st< std::greater_equal<double> >
                               (std::greater_equal<double>(), leftBound));
            else
                ierr = doCount(vals, mask,
                               std::bind2nd<std::less<double> >
                               (std::less<double>(), rightBound));
            break;}
        case ibis::qExpr::OP_LE: {
            if (leftBound <= rightBound)
                ierr = doCount(vals, mask,
                               std::bind1st< std::greater_equal<double> >
                               (std::greater_equal<double>(), leftBound));
            else
                ierr = doCount(vals, mask,
                               std::bind2nd<std::less_equal<double> >
                               (std::less_equal<double>(), rightBound));
            break;}
        case ibis::qExpr::OP_GT: {
            if (leftBound > rightBound)
                ierr = doCount(vals, mask,
                               std::bind1st< std::greater_equal<double> >
                               (std::greater_equal<double>(), leftBound),
                               std::bind2nd<std::greater<double> >
                               (std::greater<double>(), rightBound));
            else
                ierr = 0;
            break;}
        case ibis::qExpr::OP_GE: {
            if (leftBound >= rightBound)
                ierr = doCount(vals, mask,
                               std::bind1st< std::greater_equal<double> >
                               (std::greater_equal<double>(), leftBound),
                               std::bind2nd<std::greater_equal<double> >
                               (std::greater_equal<double>(), rightBound));
            else
                ierr = 0;
            break;}
        case ibis::qExpr::OP_EQ: {
            if (leftBound > rightBound) {
                ierr = doCount(vals, mask,
                               std::bind1st< std::greater_equal<double> >
                               (std::greater_equal<double>(), leftBound),
                               std::bind2nd<std::equal_to<double> >
                               (std::equal_to<double>(), rightBound));
            }
            else {
                ierr = 0;
            }
            break;}
        default: {
            ierr = doCount(vals, mask,
                           std::bind1st< std::greater_equal<double> >
                           (std::greater_equal<double>(), leftBound));
            break;}
        }
        break;}
    case ibis::qExpr::OP_EQ: {
        switch (rop) {
        case ibis::qExpr::OP_LT: {
            if (leftBound < rightBound) {
                ierr = doCount(vals, mask,
                               std::bind1st< std::equal_to<double> >
                               (std::equal_to<double>(), leftBound));
            }
            else {
                ierr = 0;
            }
            break;}
        case ibis::qExpr::OP_LE: {
            if (leftBound <= rightBound) {
                ierr = doCount(vals, mask,
                               std::bind1st< std::equal_to<double> >
                               (std::equal_to<double>(), leftBound));
            }
            else {
                ierr = 0;
            }
            break;}
        case ibis::qExpr::OP_GT: {
            if (leftBound > rightBound) {
                ierr = doCount(vals, mask,
                               std::bind1st< std::equal_to<double> >
                               (std::equal_to<double>(), leftBound));
            }
            else {
                ierr = 0;
            }
            break;}
        case ibis::qExpr::OP_GE: {
            if (leftBound >= rightBound) {
                ierr = doCount(vals, mask,
                               std::bind1st< std::equal_to<double> >
                               (std::equal_to<double>(), leftBound));
            }
            else {
                ierr = 0;
            }
            break;}
        case ibis::qExpr::OP_EQ: {
            if (leftBound == rightBound && rightBound == rng.rightBound()) {
                ierr = doCount(vals, mask,
                               std::bind1st< std::equal_to<double> >
                               (std::equal_to<double>(), leftBound));
            }
            else {
                ierr = 0;
            }
            break;}
        default: {
            ierr = doCount(vals, mask,
                           std::bind1st< std::equal_to<double> >
                           (std::equal_to<double>(), leftBound));
            break;}
        }
        break;}
    default: {
        switch (rop) {
        case ibis::qExpr::OP_LT: {
            ierr = doCount(vals, mask,
                           std::bind2nd<std::less<double> >
                           (std::less<double>(), rightBound));
            break;}
        case ibis::qExpr::OP_LE: {
            ierr = doCount(vals, mask,
                           std::bind2nd<std::less_equal<double> >
                           (std::less_equal<double>(), rightBound));
            break;}
        case ibis::qExpr::OP_GT: {
            ierr = doCount(vals, mask,
                           std::bind2nd<std::greater<double> >
                           (std::greater<double>(), rightBound));
            break;}
        case ibis::qExpr::OP_GE: {
            ierr = doCount(vals, mask,
                           std::bind2nd<std::greater_equal<double> >
                           (std::greater_equal<double>(), rightBound));
            break;}
        case ibis::qExpr::OP_EQ: {
            if (rightBound == rng.rightBound()) {
                ierr = doCount(vals, mask,
                               std::bind2nd<std::equal_to<double> >
                               (std::equal_to<double>(), rightBound));
            }
            else {
                ierr = 0;
            }
            break;}
        default: {
            ierr = 0;
            break;}
        }
        break;}
    }
    return ierr;
} // ibis::part::doCount

template <typename T>
long ibis::part::doCount(const array_t<T> &vals, const ibis::qIntHod &cmp,
                         const ibis::bitvector &mask) const {
    long ierr = 0;
    for (ibis::bitvector::indexSet ix = mask.firstIndexSet();
         ix.nIndices() > 0; ++ ix) {
        const ibis::bitvector::word_t *iix = ix.indices();
        if (ix.isRange()) {
            for (unsigned ii = *iix; ii < iix[1]; ++ ii) {
                const int64_t tmp = (int64_t) vals[ii];
                if (static_cast<T>(tmp) == vals[ii])
                    ierr += static_cast<int>(cmp.inRange(tmp));
            }
        }
        else {
            for (unsigned ii = 0; ii < ix.nIndices(); ++ ii) {
                const int64_t tmp = (int64_t)vals[iix[ii]];
                if (static_cast<T>(tmp) == vals[iix[ii]])
                    ierr += static_cast<int>(cmp.inRange(tmp));
            }
        }
    }
    return ierr;
} // ibis::part::doCount

template <typename T>
long ibis::part::doCount(const array_t<T> &vals, const ibis::qUIntHod &cmp,
                         const ibis::bitvector &mask) const {
    long ierr = 0;
    for (ibis::bitvector::indexSet ix = mask.firstIndexSet();
         ix.nIndices() > 0; ++ ix) {
        const ibis::bitvector::word_t *iix = ix.indices();
        if (ix.isRange()) {
            for (unsigned ii = *iix; ii < iix[1]; ++ ii) {
                const uint64_t tmp = (uint64_t)vals[ii];
                if (static_cast<T>(tmp) == vals[ii])
                    ierr += static_cast<int>(cmp.inRange(tmp));
            }
        }
        else {
            for (unsigned ii = 0; ii < ix.nIndices(); ++ ii) {
                const uint64_t tmp = (uint64_t)vals[iix[ii]];
                if (static_cast<T>(tmp) == vals[iix[ii]])
                    ierr += static_cast<int>(cmp.inRange(tmp));
            }
        }
    }
    return ierr;
} // ibis::part::doCount

template <typename T>
long ibis::part::doCount(const array_t<T> &vals, const ibis::qRange &cmp,
                         const ibis::bitvector &mask) const {
    long ierr = 0;
    for (ibis::bitvector::indexSet ix = mask.firstIndexSet();
         ix.nIndices() > 0; ++ ix) {
        const ibis::bitvector::word_t *iix = ix.indices();
        if (ix.isRange()) {
            for (unsigned ii = *iix; ii < iix[1]; ++ ii)
                ierr += static_cast<int>(cmp.inRange(vals[ii]));
        }
        else {
            for (unsigned ii = 0; ii < ix.nIndices(); ++ ii)
                ierr += static_cast<int>(cmp.inRange(vals[iix[ii]]));
        }
    }
    return ierr;
} // ibis::part::doCount

template <typename T, typename F>
long ibis::part::doCount(const array_t<T> &vals,
                         const ibis::bitvector &mask, F cmp) const {
    long ierr = 0;
    for (ibis::bitvector::indexSet ix = mask.firstIndexSet();
         ix.nIndices() > 0; ++ ix) {
        const ibis::bitvector::word_t *iix = ix.indices();
        if (ix.isRange()) {
            for (unsigned ii = *iix; ii < iix[1]; ++ ii)
                ierr += static_cast<int>(cmp(vals[ii]));
        }
        else {
            for (unsigned ii = 0; ii < ix.nIndices(); ++ ii)
                ierr += static_cast<int>(cmp(vals[iix[ii]]));
        }
    }
    return ierr;
} // ibis::part::doCount

template <typename T, typename F1, typename F2>
long ibis::part::doCount(const array_t<T> &vals,
                         const ibis::bitvector &mask,
                         F1 cmp1, F2 cmp2) const {
    long ierr = 0;
    for (ibis::bitvector::indexSet ix = mask.firstIndexSet();
         ix.nIndices() > 0; ++ ix) {
        const ibis::bitvector::word_t *iix = ix.indices();
        if (ix.isRange()) {
            for (unsigned ii = *iix; ii < iix[1]; ++ ii)
                ierr += static_cast<int>(cmp1(vals[ii]) && cmp2(vals[ii]));
        }
        else {
            for (unsigned ii = 0; ii < ix.nIndices(); ++ ii)
                ierr += static_cast<int>(cmp1(vals[iix[ii]]) &&
                                         cmp2(vals[iix[ii]]));
        }
    }
    return ierr;
} // ibis::part::doCount

// derive backupDir from activeDir
void ibis::part::deriveBackupDirName() {
    if (activeDir == 0) {
#if FASTBIT_DIRSEP == '/'
        activeDir = ibis::util::strnewdup(".ibis/dir1");
        backupDir = ibis::util::strnewdup(".ibis/dir2");
#else
        activeDir = ibis::util::strnewdup(".ibis\\dir1");
        backupDir = ibis::util::strnewdup(".ibis\\dir2");
#endif
    }
    if (backupDir) {
        if (std::strcmp(activeDir, backupDir)) {
            // activeDir differs from backupDir, no need to do anything
            return;
        }
        delete [] backupDir;
        backupDir = 0;
    }

    uint32_t j = std::strlen(activeDir);
    backupDir = new char[j+12];
    (void) strcpy(backupDir, activeDir);
    char* ptr = backupDir + j - 1;
    while (ptr>=backupDir && std::isdigit(*ptr)) {
        -- ptr;
    }
    ++ ptr;
    if (ptr >= backupDir + j) {
        ptr = backupDir + j;
        j = 0;
    }
    else {
        j = strtol(ptr, 0, 0);
    }

    long ierr;
    do {
        ++j;
        Stat_T buf;
        (void) sprintf(ptr, "%lu", static_cast<long unsigned>(j));
        ierr = UnixStat(backupDir, &buf);
    } while ((ierr == 0 || errno != ENOENT) && j);
    if (j == 0) {
        logError("deriveBackupDirName", "all names of the form %snnn are "
                 "in use", activeDir);
    }
} // ibis::part::deriveBackupDirName

// make sure the Alternative_Directory specified in the metadata files from
// the activeDir and the backupDir are consistent, and the number of events
// are the same
long ibis::part::verifyBackupDir() {
    long ierr = 0;
    if (activeDir == 0 || backupDir == 0 || *backupDir == 0 ||
        backupDir == activeDir || std::strcmp(activeDir, backupDir) == 0)
        return ierr;

    try {
        ierr = ibis::util::makeDir(backupDir);
        if (ierr < 0) {
            delete [] backupDir;
            backupDir = 0;
            return ierr;
        }
    }
    catch (const std::exception &e) {
        logWarning("part::verifyBackupDir",
                   "could not create backupDir \"%s\" -- %s",
                   backupDir, e.what());
        delete [] backupDir;
        backupDir = 0;
        return -12;
    }
    catch (const char* s) {
        logWarning("part::verifyBackupDir",
                   "could not create backupDir \"%s\" -- %s",
                   backupDir, s);
        delete [] backupDir;
        backupDir = 0;
        return -13;
    }
    catch (...) {
        logWarning("part::verifyBackupDir",
                   "could not create backupDir \"%s\" -- unknow error",
                   backupDir);
        delete [] backupDir;
        backupDir = 0;
        return -14;
    }

    Stat_T st;
    uint32_t np = 0;
    std::string fn = backupDir;
    fn += FASTBIT_DIRSEP;
    fn += "-part.txt";
    ierr = UnixStat(fn.c_str(), &st);
    if (ierr != 0) { // try the old file name
        fn.erase(fn.size()-9);
        fn += "table.tdc";
        ierr = UnixStat(fn.c_str(), &st);
    }
    if (ierr == 0) {
        // read the file to retrieve Alternative_Directory and
        // Number_of_events
        FILE* file = fopen(fn.c_str(), "r");
        if (file == 0) {
            logWarning("verifyBackupDir", "could not open file \"%s\" ... %s",
                       fn.c_str(), (errno ? strerror(errno)
                                    : "no free stdio stream"));
            if (nEvents == 0) ierr = 0;
            return ierr;
        }

        char buf[MAX_LINE];
        char *rs;
        while (0 != (rs = fgets(buf, MAX_LINE, file))) {
            rs = strchr(buf, '=');
            if (strnicmp(buf, "END HEADER", 10) == 0) {
                break;
            }
            else if (rs != 0) {
                ++ rs; // pass = character
                if (strnicmp(buf, "Number_of_rows", 14) == 0 ||
                    strnicmp(buf, "Number_of_events", 16) == 0 ||
                    strnicmp(buf, "Number_of_records", 17) == 0) {
                    uint32_t ne = strtol(rs, 0, 0);
                    if (ne != nEvents) {
                        -- ierr;
                        logWarning("verifyBackupDir", "backup directory"
                                   " contains %lu rows, but the active"
                                   " directory has %lu.",
                                   static_cast<long unsigned>(ne),
                                   static_cast<long unsigned>(nEvents));
                    }
                }
                else if (strnicmp(buf, "Number_of_columns", 17) == 0 ||
                         strnicmp(buf, "Number_of_properties", 20) == 0) {
                    np = strtol(rs, 0, 0);
                }
                else if (strnicmp(buf, "Alternative_Directory", 21) == 0) {
                    rs += strspn(rs," \t\"\'");
                    char* tmp = strpbrk(rs, " \t\"\'");
                    if (tmp != 0) *tmp = static_cast<char>(0);
                    if ((backupDir == 0 || std::strcmp(rs, backupDir)) &&
                        (activeDir == 0 || std::strcmp(rs, activeDir))) {
                        -- ierr;
                        logWarning("verifyBackupDir",
                                   "Alternative_Directory "
                                   "entry inconsistent: active=\"%s\" "
                                   "backup=\"%s\"", backupDir, rs);
                    }
                }
            }
        } // while ...
        fclose(file);
    } // if (ierr == 0)
    else if (nEvents > 0) {
        logWarning("verifyBackupDir", "no metadata file in \"%s\".  The "
                   "backup directory is likely empty.\nstat returns %d, "
                   "errno = %d (%s).", backupDir, ierr, errno,
                   strerror(errno));
        ierr = -10;
    }
    else {
        ierr = 0;
    }
    if (ierr < 0)
        return ierr;
    ierr = 0;
    if (np != columns.size()) {
        ierr = -11;
        logWarning("verifyBackupDir", "backup directory "
                   "contains %lu columns, but the active "
                   "directory has %lu.", static_cast<long unsigned>(np),
                   static_cast<long unsigned>(columns.size()));
        return ierr;
    }

    if (ierr == 0) {
        if (ibis::gVerbose > 1)
            logMessage("verifyBackupDir", "backupDir verified to be ok");
    }
    else {
        if (ibis::gVerbose > 0)
            logWarning("verifyBackupDir", "backupDir verified to be NOT ok. "
                       "ierr = %d", ierr);
        ierr -= 100;
    }
    return ierr;
} // ibis::part::verifyBackupDir

/// The routine to perform the actual copying for making a backup copy.
void ibis::part::doBackup() {
    if (backupDir == 0 || *backupDir == 0 || activeDir == 0)
        return;

    if ((state == UNKNOWN_STATE || state == PRETRANSITION_STATE ||
         state == POSTTRANSITION_STATE) && nEvents > 0) {
        // only make copy nonempty tables in some states
#if (defined(__unix__) && !defined(__CYGWIN__) && !(defined(__sun) && defined(__GNUC__) && __GNUC__ <= 2)) || defined(__HOS_AIX__)
        // block SIGHUP and SIGINT to this thread
        sigset_t sigs;
        sigemptyset(&sigs);
        sigaddset(&sigs, SIGHUP);
        sigaddset(&sigs, SIGINT);
        pthread_sigmask(SIG_BLOCK, &sigs, 0);
#endif
        {
            ibis::util::mutexLock lck(&ibis::util::envLock, backupDir);
            ibis::util::removeDir(backupDir);
        }
        if (ibis::gVerbose > 2)
            logMessage("doBackup", "copy files from \"%s\" to \"%s\"",
                       activeDir, backupDir);

        char* cmd = new char[std::strlen(activeDir)+std::strlen(backupDir)+32];
#if defined(__unix__) || defined(__linux__) || defined(__HOS_AIX__) || defined(__APPLE__) || defined(__FreeBSD__)
        sprintf(cmd, "/bin/cp -fr \"%s\" \"%s\"", activeDir, backupDir);
#elif defined(_WIN32)
        sprintf(cmd, "xcopy /i /s /e /h /r /q \"%s\" \"%s\"", activeDir,
                backupDir);
#else
#error DO NOT KNOW HOW TO COPY FILES!
#endif
        if (ibis::gVerbose > 4)
            logMessage("doBackup", "issuing sh command \"%s\"..", cmd);

        FILE* fptr = popen(cmd, "r");
        if (fptr) {
            char* buf = new char[MAX_LINE];
            LOGGER(ibis::gVerbose > 4) << "output from command " << cmd;

            while (fgets(buf, MAX_LINE, fptr))
                LOGGER(ibis::gVerbose > 4) << buf;

            delete [] buf;

            int ierr = pclose(fptr);
            if (ierr == 0) {
                state = STABLE_STATE;   // end in STABLE_STATE
                if (ibis::gVerbose > 4)
                    logMessage("doBackup", "successfully copied files");
            }
            else {
                logWarning("doBackup", "pclose failed ... %s",
                           strerror(errno));
            }
        }
        else {
            logError("doBackup", "popen(%s) failed with "
                     "errno = %d", cmd, errno);
        }
        delete [] cmd;
#if (defined(__unix__) && !defined(__CYGWIN__) && !(defined(__sun) && defined(__GNUC__) && __GNUC__ <= 2)) || defined(__HOS_AIX__)
        pthread_sigmask(SIG_UNBLOCK, &sigs, 0);
#endif
    }
} // ibis::part::doBackup

/// Spawn another thread to copy the content of @c activeDir to @c backupDir.
void ibis::part::makeBackupCopy() {
    if (backupDir == 0 || *backupDir == 0 || activeDir == 0)
        return; // nothing to do

    pthread_attr_t tattr;
    int ierr = pthread_attr_init(&tattr);
    if (ierr) {
        logError("makeBackupCopy", "pthread_attr_init failed with %d", ierr);
    }
#if defined(PTHREAD_SCOPE_SYSTEM)
    ierr = pthread_attr_setscope(&tattr, PTHREAD_SCOPE_SYSTEM);
    if (ierr
#if defined(ENOTSUP)
        && ierr != ENOTSUP
#endif
        ) {
        logMessage("makeBackupCopy", "pthread_attr_setscope could not "
                   "set system scope (ierr = %d ... %s)", ierr,
                   strerror(ierr));
    }
#endif
#if defined(PTHREAD_CREATE_DETACHED)
    ierr = pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_DETACHED);
    if (ierr
#if defined(ENOTSUP)
        && ierr != ENOTSUP
#endif
        ) {
        logMessage("makeBackupCopy", "pthread_attr_setdetachstate in unable "
                   "to set detached stat (ierr = %d ... %s)", ierr,
                   strerror(ierr));
    }
#elif defined(__unix__) || defined(__HOS_AIX__)
    ierr = pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_DETACHED);
    if (ierr
#if defined(ENOTSUP)
        && ierr != ENOTSUP
#endif
        ) {
        logError("makeBackupCopy", "pthread_attr_setdetachstate could not"
                 " set DETACHED state (ierr = %d)", ierr);
    }
#endif

    pthread_t tid;
    ierr = pthread_create(&tid, &tattr, ibis_part_startBackup,
                          (void*)this);
    if (ierr) {
        logError("makeBackupCopy", "pthread_create could not create a"
                 " detached thread to perform the actual file copying."
                 " returned value is %d", ierr);
    }
    else if (ibis::gVerbose > 1) {
        logMessage("makeBackupCopy", "created a new thread to perform "
                   "the actual copying");
    }
    ierr = pthread_attr_destroy(&tattr);
} // ibis::part::makeBackupCopy

double ibis::part::getActualMin(const char *name) const {
    const ibis::column* col = getColumn(name);
    if (col != 0)
        return col->getActualMin();
    else
        return DBL_MAX;
} // ibis::part::getActualMin

double ibis::part::getActualMax(const char *name) const {
    const ibis::column* col = getColumn(name);
    if (col != 0)
        return col->getActualMax();
    else
        return -DBL_MAX;
} // ibis::part::getActualMax

double ibis::part::getColumnSum(const char *name) const {
    double ret;
    const ibis::column* col = getColumn(name);
    if (col != 0)
        ret = col->getSum();
    else
        ibis::util::setNaN(ret);
    return ret;
} // ibis::part::getColumnSum

/// Write the content of vals to an open file.  This template function
/// works with fixed size elements stored in array_t.
///
/// Return the number of elements written or an error code.  The error code
/// is always less than 0.
template <typename T>
int ibis::part::writeColumn(int fdes,
                            ibis::bitvector::word_t nold,
                            ibis::bitvector::word_t nnew,
                            ibis::bitvector::word_t voffset,
                            const array_t<T>& vals,
                            const T& fill,
                            ibis::bitvector& totmask,
                            const ibis::bitvector& newmask) {
    const uint32_t elem = sizeof(T);
    off_t pos = UnixSeek(fdes, 0, SEEK_END);
    if (pos < 0) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- part::writeColumn<" << typeid(T).name() << ">("
            << fdes << ", " << nold << ", " << nnew << " ...) could not seek "
            "to the end of the file";
        return -3; // could not find the EOF position
    }
    if ((uint32_t) pos < nold*elem) {
        const uint32_t n1 = (uint32_t)pos / elem;
        totmask.adjustSize(n1, nold);
        for (uint32_t j = n1; j < nold; ++ j)
            LOGGER(elem > UnixWrite(fdes, &fill, elem) &&
                   ibis::gVerbose > 1)
                << "Warning -- part::writeColumn<" << typeid(T).name() << ">("
                << fdes << ", " << nold << ", " << nnew << " ...) could not "
                "write fill value as " << j << "th value";
    }
    else if ((uint32_t) pos > nold*elem) {
        pos = UnixSeek(fdes, nold*elem, SEEK_SET);
        totmask.adjustSize(nold, nold);
    }
    else {
        totmask.adjustSize(nold, nold);
    }

    if (vals.size() >= nnew+voffset) {
        pos = UnixWrite(fdes, vals.begin()+voffset, nnew*elem);
        totmask += newmask;
    }
    else {
        pos = UnixWrite(fdes, vals.begin()+voffset, (vals.size()-voffset)*elem);
        for (uint32_t j = vals.size(); j < nnew; ++ j)
            pos += UnixWrite(fdes, &fill, elem);
        totmask += newmask;
    }
    totmask.adjustSize(totmask.size(), nnew+nold);
    if (ibis::gVerbose > 4) {
        ibis::util::logger lg;
        lg() << "part::writeColumn wrote " << pos << " bytes of "
             << typeid(T).name() << " for " << nnew << " element"
             << (nnew>1?"s":"") << " starting from " << voffset;
        if (ibis::gVerbose > 6) {
            if (ibis::gVerbose > 7)
                lg() << "\nmask for new records: " << newmask;
            lg() << "\nOverall bit mask: "<< totmask;
        }
    }
    return (pos / elem);
} // ibis::part::writeColumn

/// Write strings to an open file.  The strings are stored in a
/// std::vector<std::string>.  The strings are null-terminated and
/// therefore can not contain null characters in them.
///
/// Return the number of strings written to the open file or an error code.
int ibis::part::writeStrings(const char *fnm,
                             ibis::bitvector::word_t nold,
                             ibis::bitvector::word_t nnew,
                             ibis::bitvector::word_t voffset,
                             const std::vector<std::string>& vals,
                             ibis::bitvector& totmask,
                             const ibis::bitvector& newmask) {
    std::string evt = "part::writeStrings";
    if (ibis::gVerbose > 0) {
        evt += "(";
        evt += fnm;
        evt += ", ";
        if (ibis::gVerbose > 2) {
            std::ostringstream oss;
            oss << nold << ", " << nnew;
            evt += oss.str();
        }
        evt += "...)";
    }
    FILE *fptr = fopen(fnm, "ab");
    if (fptr == 0) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " could not open the named file: "
            << (errno ? strerror(errno) : "no free stdio stream");
        return -1;
    }
    off_t pos = ftell(fptr); // pos == end of file
    if (pos < 0) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " could not seek to the end of the file";
        return -3; // could not find the EOF position
    }

    size_t cnt = 0;
    ibis::bitvector::word_t nnew0;
    totmask.adjustSize(nold, nold);
    if (vals.size() >= nnew+voffset) {
        for (uint32_t j = voffset; j < voffset+nnew; ++ j)
            cnt += (0 < fwrite(vals[j].c_str(), vals[j].size()+1, 1, fptr));
        nnew0 = cnt;
    }
    else {
        for (uint32_t j = voffset; j < vals.size(); ++ j)
            cnt += (0 < fwrite(vals[j].c_str(), vals[j].size()+1, 1, fptr));
        nnew0 = cnt;
        char buf[MAX_LINE];
        memset(buf, 0, MAX_LINE);
        for (uint32_t j = (vals.size()>voffset?vals.size()-voffset:0);
             j < nnew; j += MAX_LINE)
            cnt += fwrite(buf, 1, (j+MAX_LINE<=nnew?MAX_LINE:nnew-j), fptr);
    }
#if defined(FASTBIT_SYNC_WRITE)
    (void) fflush(fptr);
#endif
    (void) fclose(fptr);

    totmask += newmask;
    totmask.adjustSize(nold+nnew0, nnew+nold);
    if (ibis::gVerbose > 4) {
        ibis::util::logger lg;
        lg() << evt << " wrote " << cnt << " strings (" << nnew << " expected)";
#if DEBUG+0 > 1 || _DEBUG+0 > 1
        lg() << "\nvals[" << vals.size() << "]:";
        for (uint32_t j = 0; j < (nnew <= vals.size() ? nnew : vals.size());
             ++ j)
            lg() << "\n  " << j << "\t" << vals[j];
#endif
        if (ibis::gVerbose > 6) {
            if (ibis::gVerbose > 7)
                lg() << "\nmask for new records: " << newmask;
            lg() << "\nOverall bit mask: " << totmask;
        }
    }
    return nnew;
} // ibis::part::writeStrings

/// Write raw bytes to an open file.  It also requires a second file to
/// store starting positions of the raw binary objects.
///
/// Return the number of raw objects written to the open file or an error
/// code.  Note that the error code is always less than 0.
int ibis::part::writeRaw(int bdes, int sdes,
                         ibis::bitvector::word_t nold,
                         ibis::bitvector::word_t nnew,
                         ibis::bitvector::word_t voffset,
                         const ibis::array_t<unsigned char>& bytes,
                         const ibis::array_t<int64_t>& starts,
                         ibis::bitvector& totmask,
                         const ibis::bitvector& newmask) {
    off_t ierr;
    const uint32_t selem = sizeof(int64_t);
    int64_t bpos = UnixSeek(bdes, 0, SEEK_END);
    if (bpos < 0) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- part::writeRaw(" << bdes << ", " << sdes << ", "
            << nold << ", " << nnew << " ...) could not seek to end of file "
            << bdes << ", seek returned " << bpos;
        return -3; // could not find the EOF position
    }
    off_t spos = UnixSeek(sdes, 0, SEEK_END);
    if (spos < 0) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- part::writeRaw(" << bdes << ", " << sdes << ", "
            << nold << ", " << nnew << "...) could not seek to the end of file "
            << sdes << ", seek returned " << spos;
        return -4;
    }
    if (spos % selem != 0) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- part::writeRaw expects the file for starting "
            "posistion to have a multiple of " << selem << " bytes, but it is "
            << spos;
        return -5;
    }
    if (spos == (int64_t)selem) {
        spos = 0;
        ierr = UnixSeek(sdes, 0, SEEK_SET);
        if (ierr != 0) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- part::writeRaw could not seek to the beginning "
                "of file " << sdes << " for starting positions, seek returned "
                << ierr;
            return -6;
        }
    }

    int64_t stmp;
    if (spos > 0) {
        ierr = UnixSeek(sdes, spos-selem, SEEK_SET);
        if (ierr != static_cast<off_t>(spos-selem)) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- part::writeRaw could not seek to "
                << spos-selem << " in file " << sdes << " for starting "
                "positions, seek returned" << ierr;
            return -7;
        }
        ierr = UnixRead(sdes, &stmp, selem);
        if (ierr < (off_t)selem) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- part::writeRaw could not read the last " << selem
                << " bytes from file " << sdes << " for starting positions, "
                "read returned " << ierr;
            return -8;
        }
        if (stmp != bpos) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- part::writeRaw expects the last value in file "
                << sdes << "(which is " << stmp << ") to match the size of "
                << bdes << " (which is " << bpos << "), but they do NOT";
            return -9;
        }
    }

    const ibis::bitvector::word_t nold1 =
        (spos > static_cast<off_t>(selem) ? (spos / selem - 1) : 0);
    if (nold1 == 0) { // need to write the 1st number which is always 0
        bpos = 0;
        ierr = UnixWrite(sdes, &bpos, selem);
        if (ierr < (off_t)selem) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- part::writeRaw could not write " << bpos
                << " to file " << sdes << ", write returned " << ierr;
            return -10;
        }
    }
    if (nold1 < nold) {
        // existing data file does not have enough elements, add empty ones
        // to fill them
        for (size_t j = spos/selem; j <= nold; ++ j) {
            ierr = UnixWrite(sdes, &bpos, selem);
            if (ierr < (off_t)selem) {
                LOGGER(ibis::gVerbose > 0)
                    << "Warning -- part::writeRaw could not write " << bpos
                    << " to the end of file " << sdes << ", write returned "
                    << ierr;
                return -11;
            }
        }
    }
    else if (nold1 > nold) {
        // existing files have too many elements
        spos = nold*selem;
        ierr = UnixSeek(sdes, spos, SEEK_SET);
        if (ierr != spos) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- part::writeRaw could not seek to " << spos
                << " in file " << sdes
                << " for starting positions, seek returned " << ierr;
            return -12;
        }
        ierr = UnixRead(sdes, &bpos, selem);
        if (ierr < (off_t)selem) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- part::writeRaw could not read " << selem
                << " bytes from " << spos << " of file " << sdes
                << " for starting positions, read returned " << ierr;
            return -13;
        }
        ierr = UnixSeek(bdes, bpos, SEEK_SET);
        if (ierr != bpos) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- part::writeRaw could not seek to " << bpos
                << " in file "<< bpos << " for binary objects, seek returned "
                << ierr;
            return -14;
        }
    }

    ibis::bitvector::word_t nnew1 = (starts.size() > voffset+nnew+1 ? nnew :
                                     (starts.size()>voffset+1 ?
                                      starts.size()-voffset-1 : 0));
    for (bitvector::word_t j = voffset; j < voffset+nnew1; ++ j) {
        bpos += starts[j+1] - starts[j];
        ierr = UnixWrite(sdes, &bpos, selem);
        if (ierr < (int64_t)selem) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- part::writeRaw could not write " << bpos
                << " to file "<< sdes << " for starting positions, "
                "write returned " << ierr;
            return -15;
        }
    }
    stmp = starts[voffset+nnew1] - starts[voffset];
    ierr = UnixWrite(bdes, bytes.begin()+starts[voffset], stmp);
    if (ierr != stmp) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- part::writeRaw expects to write " << stmp << " byte"
            << (stmp>1 ? "s" : "") << ", but wrote " << ierr << " instead";
        return -16;
    }

    totmask.adjustSize(nold1, nold);
    totmask += newmask;
    totmask.adjustSize(totmask.size(), nnew1+nold);
    if (ibis::gVerbose > 4) {
        ibis::util::logger lg;
        lg() << "part::writeRaw wrote " << nnew1 << " binary object"
             << (nnew1>1?"s":"") << " starting from " << voffset
             << " (" << nnew << " expected)";
        if (ibis::gVerbose > 6) {
            if (ibis::gVerbose > 7)
                lg() << "\nmask for new records: " << newmask;
            lg() << "\nOverall bit mask: " << totmask;
        }
    }
    return (nnew1);
} // ibis::part::writeRaw

/// Write raw bytes to an open file.  It also requires a second file to
/// store starting positions of the raw binary objects.
///
/// Return the number of raw objects written to the open file or an error
/// code.  Note that the error code is always less than 0.
int ibis::part::writeOpaques(int bdes, int sdes,
                             ibis::bitvector::word_t nold,
                             ibis::bitvector::word_t nnew,
                             ibis::bitvector::word_t voffset,
                             const std::vector<ibis::opaque>& opq,
                             ibis::bitvector& totmask,
                             const ibis::bitvector& newmask) {
    off_t ierr;
    const uint32_t selem = sizeof(int64_t);
    int64_t bpos = UnixSeek(bdes, 0, SEEK_END);
    if (bpos < 0) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- part::writeOpaques(" << bdes << ", " << sdes
            << ", " << nold << ", " << opq.size()
            << " ...) could not seek to the end of file "
            << bdes << ", seek returned " << bpos;
        return -3; // could not find the EOF position
    }
    off_t spos = UnixSeek(sdes, 0, SEEK_END);
    if (spos < 0) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- part::writeOpaques(" << bdes << ", " << sdes
            << ", " << nold << ", " << opq.size()
            << "...) could not seek to the end of file " << sdes
            << ", seek returned " << spos;
        return -4;
    }
    if (spos % selem != 0) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- part::writeOpaques expects the file for starting "
            "posistion to have a multiple of " << selem << " bytes, but it is "
            << spos;
        return -5;
    }
    if (spos == (int64_t)selem) {
        spos = 0;
        ierr = UnixSeek(sdes, 0, SEEK_SET);
        if (ierr != 0) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- part::writeOpaques could not seek to the "
                "beginning of file " << sdes
                << " for starting positions, seek returned " << ierr;
            return -6;
        }
    }

    int64_t stmp;
    if (spos > 0) { // go back to read the last word
        ierr = UnixSeek(sdes, spos-selem, SEEK_SET);
        if (ierr != static_cast<off_t>(spos-selem)) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- part::writeOpaques could not seek to "
                << spos-selem << " in file " << sdes << " for starting "
                "positions, seek returned" << ierr;
            return -7;
        }
        ierr = UnixRead(sdes, &stmp, selem);
        if (ierr < (off_t)selem) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- part::writeOpaques could not read the last "
                << selem << " bytes from file " << sdes
                << " for starting positions, read returned " << ierr;
            return -8;
        }
        if (stmp != bpos) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- part::writeOpaques expects the last value in "
                "file " << sdes << "(which is " << stmp
                << ") to match the size of file "
                << bdes << " (which is " << bpos << "), but they do NOT";
            return -9;
        }
    }

    const ibis::bitvector::word_t nold1 =
        (spos > static_cast<off_t>(selem) ? (spos / selem - 1) : 0);
    if (nold1 == 0) { // need to write the 1st number which is always 0
        bpos = 0;
        ierr = UnixWrite(sdes, &bpos, selem);
        if (ierr < (off_t)selem) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- part::writeOpaques could not write " << bpos
                << " to file " << sdes << ", write returned " << ierr;
            return -10;
        }
    }
    if (nold1 < nold) {
        // existing data file does not have enough elements, add empty ones
        // to fill them
        for (size_t j = nold1; j < nold; ++ j) {
            ierr = UnixWrite(sdes, &bpos, selem);
            if (ierr < (off_t)selem) {
                LOGGER(ibis::gVerbose > 0)
                    << "Warning -- part::writeOpaques could not write " << bpos
                    << " to the end of file " << sdes << ", write returned "
                    << ierr;
                return -11;
            }
        }
    }
    else if (nold1 > nold) {
        // existing files have too many elements
        spos = nold*selem;
        ierr = UnixSeek(sdes, spos, SEEK_SET);
        if (ierr != spos) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- part::writeOpaques could not seek to " << spos
                << " in file " << sdes << " for starting positions, "
                "seek returned " << ierr;
            return -12;
        }
        ierr = UnixRead(sdes, &bpos, selem);
        if (ierr < (off_t)selem) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- part::writeOpaques could not read " << selem
                << " bytes from " << spos << " of file " << sdes
                << " for starting positions, ead returned " << ierr;
            return -13;
        }
        ierr = UnixSeek(bdes, bpos, SEEK_SET);
        if (ierr != bpos) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- part::writeOpaques could not seek to " << bpos
                << " in file " << bpos << " for binary objects, seek returned "
                << ierr;
            return -14;
        }
    }

    ibis::bitvector::word_t nnew1 = (opq.size()>voffset?opq.size()-voffset:0U);
    if (nnew1 > nnew) nnew1 = nnew;
    ibis::array_t<int64_t> starts(nnew1);
    for (bitvector::word_t j = voffset; j < voffset+nnew1; ++ j) {
        ierr = UnixWrite(bdes, opq[j].address(), opq[j].size());
        if (ierr < (off_t)opq[j].size()) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- part::writeOpaques could not write "
                << opq[j].size() << " byte" << (opq[j].size()>1?"s":"")
                << " to file " << bdes << ", write returned " << ierr;
            return -15;
        }
        bpos += ierr;
        starts[j] = bpos;
    }
    stmp = nnew1 * selem;
    ierr = UnixWrite(sdes, starts.begin(), stmp);
    if (ierr < stmp) {
        LOGGER(ibis::gVerbose > 0)
            << "part::writeOpaques expects to write " << stmp << " byte"
            << (stmp>1 ? "s" : "") << " to file " << sdes << ", but wrote "
            << ierr << " instead";
        return -16;
    }

    totmask.adjustSize(nold1, nold);
    totmask += newmask;
    if (ibis::gVerbose > 4) {
        ibis::util::logger lg;
        lg() << "part::writeOpaques wrote " << nnew1 << " binary object"
             << (nnew1>1?"s":"") << " starting from " << voffset;
        if (ibis::gVerbose > 6) {
            if (ibis::gVerbose > 7)
                lg() << "\nmask for new records: " << newmask;
            lg() << "\nOverall bit mask: " << totmask;
        }
    }
    return nnew1;
} // ibis::part::writeOpaques

/// Unload the indexes to free up some resources.
void ibis::part::cleaner::operator()() const {
    const uint32_t sz = ibis::fileManager::bytesInUse();
    thePart->unloadIndexes();
    if (sz == ibis::fileManager::bytesInUse() &&
        thePart->getStateNoLocking() == ibis::part::STABLE_STATE) {
        thePart->freeRIDs();
        LOGGER(sz == ibis::fileManager::bytesInUse() &&
               ibis::gVerbose > 3)
            << "part[" << thePart->name() << "]::cleaner did not "
            "remove anything from memory";
    }
} // ibis::part::cleaner::operator

// Construct an info object from a list of columns
ibis::part::info::info(const char* na, const char* de, const uint64_t &nr,
                       const ibis::part::columnList &co)
    : name(na), description(de), metaTags(0), nrows(nr) {
    columnList::const_iterator it;
    for (it = co.begin(); it != co.end(); ++ it)
        cols.push_back(new ibis::column::info(*((*it).second)));
}

// Construct an info object from a partition of a table
ibis::part::info::info(const part &tbl)
    : name(tbl.name()), description(tbl.description()),
      metaTags(tbl.metaTags().c_str()), nrows(tbl.nRows()) {
    columnList::const_iterator it;
    for (it = tbl.columns.begin(); it != tbl.columns.end(); ++ it)
        cols.push_back(new ibis::column::info(*((*it).second)));
}

// the destrubtor for the info class
ibis::part::info::~info() {
    for (std::vector< ibis::column::info* >::iterator it = cols.begin();
         it != cols.end(); ++it)
        delete *it;
    cols.clear();
}

/// Collect the null masks together.
void ibis::part::barrel::getNullMask(ibis::bitvector &mask) const {
    if (_tbl == 0) return; // can not do anything

    _tbl->getNullMask(mask);
    for (uint32_t i = 0; i < namelist.size(); ++ i) {
        ibis::bitvector tmp;
        if (i < cols.size() && cols[i] != 0) {
            cols[i]->getNullMask(tmp);
        }
        else {
            const char *nm = name(i);
            const ibis::column* col = _tbl->getColumn(nm);
            if (col) {
                col->getNullMask(tmp);
            }
            else if (nm != 0 && *nm != 0 && *nm != '*') {
                LOGGER(ibis::gVerbose > 2)
                    << "Warning -- barrel::getNullMask could not find a "
                    "column named \"" << nm << "\" in partition "
                    << _tbl->name();
            }
        }
        if (tmp.size() == _tbl->nRows()) {
            if (mask.size() == _tbl->nRows())
                mask &= tmp;
            else
                mask.copy(tmp);
        }
    }
} // ibis::part::barrel::getNullMask

/// If the data files are on disk, then attempt to read the content through
/// ibis::fileManager::getFile first.  If that fails, open the data file
/// and so that each value can be read one at a time.  If the data files
/// are not on disk, it invokes the function getRawData to get a pointer to
/// the raw data in memory.  Upon successful completion, it returns 0,
/// otherwise it returns a negative value to indicate error conditions.
///
/// The argument t may be left as nil if the barrel object was initialized
/// with a valid pointer to the ibis::part object.
long ibis::part::barrel::open(const ibis::part *t) {
    long ierr = 0;
    position = 0;
    if (t == 0 && _tbl == 0) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- part::barrel::open needs an ibis::part object";
        ierr = -1;
        return ierr;
    }
    if (_tbl == 0) _tbl = t;
    if (t == 0) t = _tbl;

    if (size() == 0) return ierr; // nothing to do

    stores.resize(size());
    fdes.resize(size());
    cols.resize(size());
    for (uint32_t i = 0; i < size(); ++ i) {
        stores[i] = 0;
        fdes[i] = -1;
        cols[i] = 0;
    }

    if (t->currentDataDir() != 0 && *(t->currentDataDir()) != 0) {
        std::string dfn = t->currentDataDir();
        uint32_t dirlen = dfn.size();
        if (dfn[dirlen-1] != FASTBIT_DIRSEP) {
            dfn += FASTBIT_DIRSEP;
            ++ dirlen;
        }

        for (uint32_t i = 0; i < size(); ++ i) {
            ibis::column *col = t->getColumn(name(i));
            if (col == 0) {
                fdes.resize(i);
                close();
                ierr = -2;
                LOGGER(ibis::gVerbose > 0)
                    << "Warning -- barrel::open could not find a column "
                    "named \"" << name(i) << "\" in data partition "
                    << t->name();
                return ierr;
            }
            else if (col->type() == ibis::BLOB || col->type() == ibis::TEXT) {
                fdes.resize(i);
                close();
                ierr = -3;
                LOGGER(ibis::gVerbose > 0)
                    << "Warning -- vault::open does not support type \""
                    << ibis::TYPESTRING[(int)col->type()] << "\" of column \""
                    << name(i) << "\"";
                return ierr;
            }
            // use the name from col to ensure the case is correct
            dfn += col->name();
            if (col->type() == ibis::CATEGORY)
                dfn += ".int";
            // use getFile first
            ierr = ibis::fileManager::instance().
                getFile(dfn.c_str(), &(stores[i]));
            if (ierr == 0) {
                stores[i]->beginUse();
            }
            else { // getFile failed, open the name file
                fdes[i] = UnixOpen(dfn.c_str(), OPEN_READONLY);
                if (fdes[i] < 0) {
                    LOGGER(ibis::gVerbose > 0)
                        << "Warning -- barrel::open could not open file \""
                        << dfn.c_str() << "\"";
                    fdes.resize(i);
                    close();
                    ierr = -4;
                    return ierr;
                }
#if defined(_WIN32) && defined(_MSC_VER)
                (void)_setmode(fdes[i], _O_BINARY);
#endif
            }
            if (size() > 1)
                dfn.erase(dirlen);
            cols[i] = col;
        }
    }
    else { // in-memory data
        for (uint32_t i = 0; ierr == 0 && i < size(); ++ i) {
            ibis::column *col = t->getColumn(name(i));
            if (col == 0) {
                ierr = -2;
            }
            else {
                stores[i] = col->getRawData();
                if (stores[i] != 0) {
                    stores[i]->beginUse();
                    cols[i] = col;
                }
                else {
                    ierr = -5;
                }
            }
        }
        if (ierr < 0) {
            close();
            return ierr;
        }
    }
    if (ibis::gVerbose > 5) {
        ibis::util::logger lg;
        lg() << "part[" << t->name() << "]::barrel::open -- ";
        if (size() > 1) {
            if (t->currentDataDir() != 0)
                lg() << "opened " << size() << " files from "
                     << t->currentDataDir();
            else
                lg() << "prepared " << size() << " arrays in memory";
            if (ibis::gVerbose > 7) {
                if (stores[0])
                    lg() << "\n\t0: " << name(0) << ", "
                         << static_cast<void*>(stores[0]->begin());
                else
                    lg() << "\n\t0: " << name(0) << ", " << fdes[0];
                for (uint32_t i = 1 ; i < size(); ++ i) {
                    lg() << "; " << i << ": " << name(i) << ", ";
                    if (stores[i])
                        lg() << static_cast<void*>(stores[i]->begin());
                    else
                        lg() << "file " << fdes[i];
                }
            }
        }
        else if (fdes[0] >= 0) {
            lg() << "successfully opened file " << name(0)
                 << " with descriptor " << fdes[0];
        }
        else if (cols[0]) {
            lg() << "successfully read " << name(0) << " into memory at "
                 << static_cast<void*>(stores[0]->begin());
        }
        else {
            lg() << "could not locate a column named " << name(0);
            ierr = -5;
        }
    }
    return ierr;
} // ibis::part::barrel::open

long ibis::part::barrel::close() {
    for (uint32_t i = 0; i < stores.size(); ++ i) {
        if (stores[i])
            stores[i]->endUse();
    }
    for (uint32_t i = 0; i < fdes.size(); ++ i)
        if (fdes[i] >= 0)
            UnixClose(fdes[i]);
    stores.clear();
    fdes.clear();
    cols.clear();
    return 0;
} // ibis::part::barrel::close

/// Read the variable values from the current record.  All values are
/// internally stored as doubles.
long ibis::part::barrel::read() {
    long ierr = 0;
    for (uint32_t i = 0; i < size(); ++ i) {
        switch (cols[i]->type()) {
        case ibis::UBYTE: { // unsigned char
            unsigned char utmp;
            if (stores[i]) {
                utmp = *(reinterpret_cast<unsigned char*>
                         (stores[i]->begin() +
                          sizeof(utmp) * position));
            }
            else {
                ierr = (sizeof(utmp) !=
                        UnixRead(fdes[i], &utmp, sizeof(utmp)));
            }
            value(i) = utmp;
            break;}
        case ibis::BYTE: { // signed char
            char itmp;
            if (stores[i]) {
                itmp = *(reinterpret_cast<char*>
                         (stores[i]->begin() +
                          sizeof(itmp) * position));
            }
            else {
                ierr = (sizeof(itmp) !=
                        UnixRead(fdes[i], &itmp, sizeof(itmp)));
            }
            value(i) = itmp;
            break;}
        case ibis::USHORT: { // unsigned short
            uint16_t utmp;
            if (stores[i]) {
                utmp = *(reinterpret_cast<uint16_t*>
                         (stores[i]->begin() +
                          sizeof(utmp) * position));
            }
            else {
                ierr = (sizeof(utmp) !=
                        UnixRead(fdes[i], &utmp, sizeof(utmp)));
            }
            value(i) = utmp;
            break;}
        case ibis::SHORT: { // signed short integer
            int16_t itmp;
            if (stores[i]) {
                itmp = *(reinterpret_cast<int16_t*>
                         (stores[i]->begin() +
                          sizeof(itmp) * position));
            }
            else {
                ierr = (sizeof(itmp) !=
                        UnixRead(fdes[i], &itmp, sizeof(itmp)));
            }
            value(i) = itmp;
            break;}
        case ibis::CATEGORY:
        case ibis::UINT:
        case ibis::TEXT: { // unsigned integer
            uint32_t utmp;
            if (stores[i]) {
                utmp = *(reinterpret_cast<uint32_t*>
                         (stores[i]->begin() +
                          sizeof(utmp) * position));
            }
            else {
                ierr = (sizeof(utmp) !=
                        UnixRead(fdes[i], &utmp, sizeof(utmp)));
            }
            value(i) = utmp;
            break;}
        case ibis::INT: { // signed integer
            int32_t itmp;
            if (stores[i]) {
                itmp = *(reinterpret_cast<int32_t*>
                         (stores[i]->begin() +
                          sizeof(itmp) * position));
            }
            else {
                ierr = (sizeof(itmp) !=
                        UnixRead(fdes[i], &itmp, sizeof(itmp)));
            }
            value(i) = itmp;
            break;}
        case ibis::ULONG: { // unsigned long integer
            uint64_t utmp;
            if (stores[i]) {
                utmp = *(reinterpret_cast<uint64_t*>
                         (stores[i]->begin() +
                          sizeof(utmp) * position));
            }
            else {
                ierr = (sizeof(utmp) !=
                        UnixRead(fdes[i], &utmp, sizeof(utmp)));
            }
            value(i) = utmp;
            break;}
        case ibis::LONG: { // signed long integer
            int64_t itmp;
            if (stores[i]) {
                itmp = *(reinterpret_cast<int64_t*>
                         (stores[i]->begin() +
                          sizeof(itmp) * position));
            }
            else {
                ierr = (sizeof(itmp) !=
                        UnixRead(fdes[i], &itmp, sizeof(itmp)));
            }
            value(i) = itmp;
            break;}
        case ibis::FLOAT: {
            // 4-byte IEEE floating-point values
            float ftmp;
            if (stores[i]) {
                ftmp = *(reinterpret_cast<float*>
                         (stores[i]->begin() +
                          sizeof(ftmp) * position));
            }
            else {
                ierr = (sizeof(ftmp) !=
                        UnixRead(fdes[i], &ftmp, sizeof(ftmp)));
            }
            value(i) = ftmp;
            break;}
        case ibis::DOUBLE: {
            // 8-byte IEEE floating-point values
            double dtmp;
            if (stores[i]) {
                dtmp = *(reinterpret_cast<double*>
                         (stores[i]->begin() +
                          sizeof(dtmp) * position));
            }
            else {
                ierr = (sizeof(dtmp) !=
                        UnixRead(fdes[i], &dtmp, sizeof(dtmp)));
            }
            value(i) = dtmp;
            break;}
        case ibis::OID:
        default: {
            ++ ierr;
            LOGGER(ibis::gVerbose > 1)
                << "Waring -- barrel::read can not work with column type "
                << ibis::TYPESTRING[(int)cols[i]->type()]
                << " (name: " << cols[i]->name() << ')';
            break;}
        }
    }
    ++ position;
    return ierr;
} // ibis::part::barrel::read

/// Seek to the position of the specified record for all variables.
long ibis::part::barrel::seek(uint32_t pos) {
    if (pos == position) return 0;
    if (pos >= cols[0]->partition()->nRows()) return -1;

    long ierr = 0;
    uint32_t i = 0;
    while (ierr == 0 && i < size()) {
        if (fdes[i] >= 0) {
            ierr = UnixSeek(fdes[i], cols[i]->elementSize()*pos, SEEK_SET);
            if (ierr != (int32_t)-1)
                ierr = 0;
        }
        ++ i;
    }
    if (ierr < 0) { // rollback the file pointers
        while (i > 0) {
            -- i;
            if (fdes[i] >= 0)
                ierr = UnixSeek(fdes[i], cols[i]->elementSize()*position,
                                SEEK_SET);
        }
    }
    else {
        position = pos;
    }
    return ierr;
} // ibis::part::barrel::seek

ibis::part::vault::vault(const ibis::roster &r)
    : barrel(r.getColumn()->partition()), _roster(r) {
    (void) recordVariable(r.getColumn()->name());
}

/// The function @c valut::open different from barrel::open in that it
/// opens the .srt file for the first variable.
long ibis::part::vault::open(const ibis::part *t) {
    long ierr = 0;
    position = 0;
    if (t == 0 && _tbl == 0) {
        ierr = -1;
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- part::vault::open needs an ibis::part object";
        return ierr;
    }
    if (_tbl == 0) _tbl = t;
    if (t == 0) t = _tbl;

    if (size() == 0) return ierr; // nothing to do

    stores.resize(size());
    fdes.resize(size());
    cols.resize(size());
    for (uint32_t i = 0; i < size(); ++ i) {
        stores[i] = 0;
        fdes[i] = -1;
        cols[i] = 0;
    }

    std::string dfn = t->currentDataDir();
    uint32_t dirlen = dfn.size();
    if (dfn[dirlen-1] != FASTBIT_DIRSEP) {
        dfn += FASTBIT_DIRSEP;
        ++ dirlen;
    }

    { // for variable 0, read the .srt file
        ibis::column *col = t->getColumn(name(0));
        if (col == 0) {
            fdes.resize(0);
            close();
            ierr = -2;
            t->logWarning("vault::open",
                          "could not find a column named \"%s\"",
                          name(0));
            return ierr;
        }

        dfn += col->name();
        dfn += ".srt"; // .srt file name
        // use getFile first
        ierr = ibis::fileManager::instance().
            getFile(dfn.c_str(), &(stores[0]));
        if (ierr == 0) {
            stores[0]->beginUse();
        }
        else { // getFile failed, open the name file
            fdes[0] = UnixOpen(dfn.c_str(), OPEN_READONLY);
            if (fdes[0] < 0) {
                t->logWarning("vault::open",
                              "could not open file \"%s\"", dfn.c_str());
                fdes.resize(0);
                close();
                ierr = -3;
                return ierr;
            }
#if defined(_WIN32) && defined(_MSC_VER)
            (void)_setmode(fdes[0], _O_BINARY);
#endif
        }
        if (ibis::gVerbose > 5) {
            if (stores[0])
                t->logMessage("vault::open", "successfully read %s "
                              "(0x%.8x) for variable %s",
                              dfn.c_str(),
                              static_cast<void*>(stores[0]->begin()),
                              col->name());
            else
                t->logMessage("vault::open",
                              "successfully opened %s with descriptor %d "
                              "for variable %s",
                              dfn.c_str(), fdes[0], col->name());
        }
        dfn.erase(dirlen);
        cols[0] = col;
    }

    // the remaining variables are opened the same way as in barrel
    for (uint32_t i = 1; i < size(); ++ i) {
        ibis::column *col = t->getColumn(name(i));
        if (col == 0) {
            fdes.resize(i);
            close();
            ierr = -2;
            t->logWarning("vault::open",
                          "could not find a column named \"%s\"",
                          name(i));
            return ierr;
        }
        else if (col->type() == ibis::BLOB || col->type() == ibis::TEXT) {
            fdes.resize(i);
            close();
            ierr = -3;
            t->logWarning("vault::open",
                          "does not support type \"%s\" of column \"%s\"",
                          ibis::TYPESTRING[(int)col->type()], name(i));
            return ierr;
        }

        dfn += col->name(); // the data file name
        if (col->type() == ibis::CATEGORY)
            dfn += ".int";
        // use getFile first
        ierr = ibis::fileManager::instance().
            getFile(dfn.c_str(), &(stores[i]));
        if (ierr == 0) {
            stores[i]->beginUse();
        }
        else { // getFile failed, open the name file
            fdes[i] = UnixOpen(dfn.c_str(), OPEN_READONLY);
            if (fdes[i] < 0) {
                t->logWarning("vault::open",
                              "could not open file \"%s\"", dfn.c_str());
                fdes.resize(i);
                close();
                ierr = -4;
                return ierr;
            }
#if defined(_WIN32) && defined(_MSC_VER)
            (void)_setmode(fdes[i], _O_BINARY);
#endif
        }
        dfn.erase(dirlen);
        cols[i] = col;
    }
    if (ibis::gVerbose > 5 && size() > 1) {
        t->logMessage("vault::open", "successfully opened %lu files from %s",
                      static_cast<long unsigned>(size()), dfn.c_str());
        if (ibis::gVerbose > 7) {
            ibis::util::logger lg;
            if (stores[0])
                lg() << "0x" << static_cast<void*>(stores[0]->begin());
            else
                lg() << "file " << fdes[0];
            for (uint32_t i = 1 ; i < size(); ++ i) {
                lg() << ", ";
                if (stores[i])
                    lg() << "0x"
                         << static_cast<void*>(stores[i]->begin());
                else
                    lg() << "file " << fdes[i];
            }
        }
    }
    return ierr;
} // ibis::part::vault::open

/// Read the records indicated by @c position.  Treat @c position as the
/// logical position, the physical position is @c _roster[position].
long ibis::part::vault::read() {
    if (position >= _roster.size()) return -1; // out of bounds

    long ierr = 0;

    // the first variable is being read sequentially
    switch (cols[0]->type()) {
    case ibis::CATEGORY:
    case ibis::UINT:
    case ibis::TEXT: { // unsigned integer
        unsigned utmp;
        if (stores[0]) {
            utmp = *(reinterpret_cast<unsigned*>
                     (stores[0]->begin() +
                      sizeof(utmp) * position));
        }
        else {
            ierr = UnixSeek(fdes[0], sizeof(utmp)*position, SEEK_SET);
            ierr = (sizeof(utmp) !=
                    UnixRead(fdes[0], &utmp, sizeof(utmp)));
        }
        value(0) = utmp;
        break;}
    case ibis::INT: { // signed integer
        int itmp;
        if (stores[0]) {
            itmp = *(reinterpret_cast<int*>
                     (stores[0]->begin() +
                      sizeof(itmp) * position));
        }
        else {
            ierr = UnixSeek(fdes[0], sizeof(itmp)*position, SEEK_SET);
            ierr = (sizeof(itmp) !=
                    UnixRead(fdes[0], &itmp, sizeof(itmp)));
        }
        value(0) = itmp;
        break;}
    case ibis::FLOAT: {
        // 4-byte IEEE floating-point values
        float ftmp;
        if (stores[0]) {
            ftmp = *(reinterpret_cast<float*>
                     (stores[0]->begin() +
                      sizeof(ftmp) * position));
        }
        else {
            ierr = UnixSeek(fdes[0], sizeof(ftmp)*position, SEEK_SET);
            ierr = (sizeof(ftmp) !=
                    UnixRead(fdes[0], &ftmp, sizeof(ftmp)));
        }
        value(0) = ftmp;
        break;}
    case ibis::DOUBLE: {
        // 8-byte IEEE floating-point values
        double dtmp;
        if (stores[0]) {
            dtmp = *(reinterpret_cast<double*>
                     (stores[0]->begin() +
                      sizeof(dtmp) * position));
        }
        else {
            ierr = UnixSeek(fdes[0], sizeof(dtmp)*position, SEEK_SET);
            ierr = (sizeof(dtmp) !=
                    UnixRead(fdes[0], &dtmp, sizeof(dtmp)));
        }
        value(0) = dtmp;
        break;}
    case ibis::OID:
    default: {
        ++ ierr;
        _tbl->logWarning("vault::read", "could not evaluate "
                         "attribute of type %s (name: %s)",
                         ibis::TYPESTRING[(int)cols[0]->type()],
                         cols[0]->name());
        break;}
    }

    uint32_t pos = _roster[position]; // the actual position in files
    for (uint32_t i = 1; i < size(); ++ i) {
        switch (cols[i]->type()) {
        case ibis::CATEGORY:
        case ibis::UINT:
        case ibis::TEXT: { // unsigned integer
            unsigned utmp;
            if (stores[i]) {
                utmp = *(reinterpret_cast<unsigned*>
                         (stores[i]->begin() +
                          sizeof(utmp) * pos));
            }
            else {
                ierr = UnixSeek(fdes[i], sizeof(utmp)*pos, SEEK_SET);
                ierr = (sizeof(utmp) !=
                        UnixRead(fdes[i], &utmp, sizeof(utmp)));
            }
            value(i) = utmp;
            break;}
        case ibis::INT: { // signed integer
            int itmp;
            if (stores[i]) {
                itmp = *(reinterpret_cast<int*>
                         (stores[i]->begin() +
                          sizeof(itmp) * pos));
            }
            else {
                ierr = UnixSeek(fdes[i], sizeof(itmp)*pos, SEEK_SET);
                ierr = (sizeof(itmp) !=
                        UnixRead(fdes[i], &itmp, sizeof(itmp)));
            }
            value(i) = itmp;
            break;}
        case ibis::FLOAT: {
            // 4-byte IEEE floating-point values
            float ftmp;
            if (stores[i]) {
                ftmp = *(reinterpret_cast<float*>
                         (stores[i]->begin() +
                          sizeof(ftmp) * pos));
            }
            else {
                ierr = UnixSeek(fdes[i], sizeof(ftmp)*pos, SEEK_SET);
                ierr = (sizeof(ftmp) !=
                        UnixRead(fdes[i], &ftmp, sizeof(ftmp)));
            }
            value(i) = ftmp;
            break;}
        case ibis::DOUBLE: {
            // 8-byte IEEE floating-point values
            double dtmp;
            if (stores[i]) {
                dtmp = *(reinterpret_cast<double*>
                         (stores[i]->begin() +
                          sizeof(dtmp) * pos));
            }
            else {
                ierr = UnixSeek(fdes[i], sizeof(dtmp)*pos, SEEK_SET);
                ierr = (sizeof(dtmp) !=
                        UnixRead(fdes[i], &dtmp, sizeof(dtmp)));
            }
            value(i) = dtmp;
            break;}
        case ibis::OID:
        default: {
            ++ ierr;
            _tbl->logWarning("vault::read", "could not evaluate "
                             "attribute of type %s (name: %s)",
                             ibis::TYPESTRING[(int)cols[i]->type()],
                             cols[i]->name());
            break;}
        }
    }
    ++ position;
    return ierr;
} // ibis::part::vault::read

/// Change the logical position of the files.
long ibis::part::vault::seek(uint32_t pos) {
    long ierr = 0;
    if (pos != position) {
        if (pos < _roster.size()) { // good
            if (fdes[0] >= 0)
                ierr = UnixSeek(fdes[0], cols[0]->elementSize()*pos,
                                SEEK_SET);
            if (ierr == 0)
                position = pos;
        }
        else {
            ierr = -1;
        }
    }
    return ierr;
} // ibis::part::vault::seek

long ibis::part::vault::seek(double val) {
    long ierr = 0;

    if (stores[0] != 0) { // data in-memory
        switch (cols[0]->type()) {
        case ibis::CATEGORY:
        case ibis::UINT:
        case ibis::TEXT: { // unsigned integer
            array_t<uint32_t> array(stores[0]);
            unsigned tgt = (val <= 0.0 ? 0 :
                            static_cast<uint32_t>(ceil(val)));
            position = array.find(tgt);
            break;}
        case ibis::INT: { // signed integer
            array_t<int32_t> array(stores[0]);
            position = array.find(static_cast<int32_t>(ceil(val)));
            break;}
        case ibis::FLOAT: {
            // 4-byte IEEE floating-point values
            array_t<float> array(stores[0]);
            position = array.find(static_cast<float>(val));
            break;}
        case ibis::DOUBLE: {
            // 8-byte IEEE floating-point values
            array_t<double> array(stores[0]);
            position = array.find(val);
            break;}
        case ibis::OID:
        default: {
            ierr = -2;
            _tbl->logWarning("vault::seek", "could not evaluate "
                             "attribute of type %s (name: %s)",
                             ibis::TYPESTRING[(int)cols[0]->type()],
                             cols[0]->name());
            break;}
        }
    }
    else { // has to go through a file one value at a time
        switch (cols[0]->type()) {
        case ibis::CATEGORY:
        case ibis::UINT:
        case ibis::TEXT: { // unsigned integer
            uint32_t tgt = (val <= 0.0 ? 0 :
                            static_cast<uint32_t>(ceil(val)));
            position = seekValue<uint32_t>(fdes[0], tgt);
            break;}
        case ibis::INT: { // signed integer
            position = seekValue<int32_t>(fdes[0],
                                          static_cast<int32_t>(ceil(val)));
            break;}
        case ibis::FLOAT: {
            // 4-byte IEEE floating-point values
            position = seekValue<float>(fdes[0], static_cast<float>(val));
            break;}
        case ibis::DOUBLE: {
            // 8-byte IEEE floating-point values
            position = seekValue<double>(fdes[0], val);
            break;}
        case ibis::OID:
        default: {
            ierr = -2;
            _tbl->logWarning("vault::seek", "could not evaluate "
                             "attribute of type %s (name: %s)",
                             ibis::TYPESTRING[(int)cols[0]->type()],
                             cols[0]->name());
            break;}
        }
    }
    return ierr;
} // ibis::part::vault::seek

/// Can not fit the value into memory, has to read one value from the file
/// one at a time to do the comparison
template <class T>
uint32_t ibis::part::vault::seekValue(int fd, const T &val) const {
    long ierr;
    uint32_t i = 0;
    uint32_t j = _roster.size();
    uint32_t m = (i + j) / 2;
    while (i < m) {
        T tmp;
        uint32_t pos = sizeof(T) * _roster[m];
        ierr = UnixSeek(fd, pos, SEEK_SET);
        if (ierr < 0)
            return _roster.size();
        ierr = UnixRead(fd, &tmp, sizeof(T));
        if (ierr < 0)
            return _roster.size();
        if (tmp < val)
            i = m;
        else
            j = m;
        m = (i + j) / 2;
    }
    if (i == 0) { // it is possible that 0th value has not been checked
        T tmp;
        uint32_t pos = sizeof(T) * _roster[0];
        ierr = UnixSeek(fd, pos, SEEK_SET);
        if (ierr < 0)
            return _roster.size();
        ierr = UnixRead(fd, &tmp, sizeof(T));
        if (ierr < 0)
            return _roster.size();
        if (tmp >= val)
            j = 0;
    }
    return j;
} // ibis::part::vault::seekValue

template <class T>
uint32_t ibis::part::vault::seekValue(const array_t<T>&arr,
                                      const T &val) const {
    uint32_t i = 0;
    uint32_t j = _roster.size();
    uint32_t m = (i + j) / 2;
    while (i < m) {
        uint32_t pos = _roster[m];
        if (arr[pos] < val)
            i = m;
        else
            j = m;
        m = (i + j) / 2;
    }
    if (i == 0) { // it is possible that 0th value has not been checked
        uint32_t pos = _roster[m];
        if (arr[pos] >= val)
            j = 0;
    }
    return j;
} // ibis::part::vault::seekValue

// This is not inlined because inlining it would make it necessary for
// part.h to include iroster.h.  Users of this function may directly call
// _roster[position] to avoid the function call overhead.
uint32_t ibis::part::vault::tellReal() const {
    return _roster[position];
} // ibis::part::vault::tellReal

ibis::part::indexBuilderPool::indexBuilderPool
(const ibis::part &t, const ibis::table::stringArray &p)
    : cnt(), opt(p.size()), tbl(t) {
    for (size_t j = 0; j < p.size(); ++ j)
        opt[j] = p[j];
} // ibis::part::indexBuilderPool

/// Examining the given directory to look for the metadata files and to
/// construct ibis::part.  Can only descend into subdirectories through
/// opendir family of functions.
///
/// Returns the number of data partitions found.
unsigned ibis::util::gatherParts(ibis::partList &tlist, const char *dir1,
                                 bool ro) {
    if (dir1 == 0) return 0;
    unsigned int cnt = 0;
    LOGGER(ibis::gVerbose > 1)
        << "util::gatherParts -- examining " << dir1;

    try {
        ibis::part* tmp = new ibis::part(dir1, ro);
        if (tmp != 0) {
            if (tmp->name() != 0 && tmp->nColumns() > 0) {
                ++ cnt;
                ibis::util::mutexLock
                    lock(&ibis::util::envLock, "gatherParts");
                ibis::partAssoc sorted;
                for (ibis::partList::iterator it = tlist.begin();
                     it != tlist.end(); ++ it) {
                    sorted[(*it)->name()] = *it;
                }
                ibis::partAssoc::const_iterator it = sorted.find(tmp->name());
                if (it != sorted.end()) { // deallocate the old table
                    if (it->second->timestamp() == tmp->timestamp() &&
                        it->second->nColumns()  == tmp->nColumns() &&
                        it->second->nRows()     == tmp->nRows()) {
                        LOGGER(ibis::gVerbose > 0)
                            << "Warning -- util::gatherParts finds the data "
                            "partition in " << dir1 << " to have exactly the "
                            "same name, number of rows, number of columns, "
                            "and time stamp as the one in "
                            << it->second->currentDataDir()
                            << " already in memory, discards the new one and "
                            "keeps the old one";
                        delete tmp;
                        tmp = 0;
                    }
                    else {
                        tmp->rename(sorted);
                        it = sorted.find(tmp->name());
                        if (it != sorted.end()) {
                            LOGGER(ibis::gVerbose > 0)
                                << "Warning -- util::gatherParts could not "
                                "rename the data partition from " << dir1
                                << " to a unique name, have to drop it";
                            delete tmp;
                            tmp = 0;
                        }
                    }
                }

                if (tmp != 0) {
                    sorted[tmp->name()] = tmp;

                    tlist.clear();
                    for (it = sorted.begin(); it != sorted.end(); ++ it)
                        tlist.push_back(it->second);
                }
            }
            else {
                LOGGER(ibis::gVerbose > 4)
                    << "util::gatherParts -- directory " << dir1
                    << "does not contain a valid \"-part.txt\" file "
                    "or contains an empty partition";
                delete tmp;
            }
        }
    }
    catch (const std::exception &e) {
        logMessage("gatherParts", "received a std::exception -- %s",
                   e.what());
    }
    catch (const char* s) {
        logMessage("gatherParts", "received a string exception -- %s",
                   s);
    }
    catch (...) {
        logMessage("gatherParts", "received an unexpected exception");
    }
#if defined(HAVE_DIRENT_H) || defined(__unix__) || defined(__HOS_AIX__) \
    || defined(__APPLE__) || defined(__CYGWIN__) || defined(__MINGW32__) \
    || defined(_XOPEN_SOURCE) || defined(_POSIX_C_SOURCE)
    // on unix machines, we know how to traverse the subdirectories
    // traverse the subdirectories to generate more tables
    char nm1[PATH_MAX];
    long len = std::strlen(dir1);

    DIR* dirp = opendir(dir1);
    if (dirp == 0) return cnt;
    struct dirent* ent = 0;
    while ((ent = readdir(dirp)) != 0) {
        if ((ent->d_name[1] == 0 || ent->d_name[1] == '.') &&
            ent->d_name[0] == '.') { // skip '.' and '..'
            continue;
        }
        if (len + std::strlen(ent->d_name)+2 >= PATH_MAX) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- util::gatherParts skipping " << dir1
                << FASTBIT_DIRSEP << ent->d_name
                << " because the name has more than " << PATH_MAX << " bytes";
            continue;
        }

        sprintf(nm1, "%s%c%s", dir1, FASTBIT_DIRSEP, ent->d_name);
        Stat_T st1;
        if (UnixStat(nm1, &st1)==0) {
            if ((st1.st_mode & S_IFDIR) == S_IFDIR) {
                cnt += gatherParts(tlist, nm1, ro);
            }
        }
    }
    closedir(dirp);
#endif
    return cnt;
} // ibis::util::gatherParts

/// Read the two directories, if there are matching subdirs, construct an
/// ibis::part from them.  Will descend into the subdirectories when run on
/// unix systems to look for matching subdirectories.
///
/// Returns the number of data partitions found.
unsigned ibis::util::gatherParts(ibis::partList &tlist,
                                 const char* adir, const char* bdir, bool ro) {
    if (adir == 0 || *adir == 0) return 0;
    unsigned int cnt = 0;
    LOGGER(ibis::gVerbose > 1)
        << "util::gatherParts -- examining directories " << adir << " and "
        << (bdir ? bdir : "?");

    try {
        part* tbl = new ibis::part(adir, bdir, ro);
        if (tbl != 0 && tbl->name() != 0 && tbl->nRows() > 0 &&
            tbl->nColumns()>0) {
            ++ cnt;
            ibis::util::mutexLock
                lock(&ibis::util::envLock, "gatherParts");
            ibis::partAssoc sorted;
            for (ibis::partList::iterator it = tlist.begin();
                 it != tlist.end(); ++ it) {
                sorted[(*it)->name()] = *it;
            }
            ibis::partAssoc::const_iterator it = sorted.find(tbl->name());
            if (it == sorted.end()) { // a new name
                sorted[tbl->name()] = tbl;
                LOGGER(ibis::gVerbose > 1)
                    << "util::gatherParts -- add new partition \""
                    << tbl->name() << "\"";
            }
            else if (it->second->timestamp() == tbl->timestamp() &&
                     it->second->nColumns()  == tbl->nColumns() &&
                     it->second->nRows()     == tbl->nRows()) {
                LOGGER(ibis::gVerbose > 0)
                    << "Warning -- util::gatherParts finds the data "
                    "partition in " << adir << " (and " << bdir
                    << ") to have exactly the same name, number of rows, "
                    "number of columns, and time stamp as the one in "
                    << it->second->currentDataDir()
                    << " already in memory, discards the new one and "
                    "keeps the old one";
                delete tbl;
                tbl = 0;
            }
            else {
                tbl->rename(sorted);
                it = sorted.find(tbl->name());
                if (it != sorted.end()) {
                    LOGGER(ibis::gVerbose > 0)
                        << "Warning -- util::gatherParts could not rename "
                        "the data partition from " << adir  << " (and " << bdir
                        << ") to a unique name, have to drop it";
                    delete tbl;
                    tbl = 0;
                }
                else {
                    sorted[tbl->name()] = tbl;
                }
            }

            tlist.clear();
            for (ibis::partAssoc::iterator it = sorted.begin();
                 it != sorted.end(); ++ it)
                tlist.push_back(it->second);
        }
        else {
            if (ibis::gVerbose > 4) {
                if (bdir == 0 || *bdir == 0)
                    logMessage("gatherParts", "directory "
                               "%s contains an invalid -part.txt or "
                               "an empty partition", adir);
                else
                    logMessage("gatherParts", "directories %s and %s "
                               "contain mismatching information or both of "
                               "them are empty", adir, bdir);
            }
            delete tbl;
            tbl = 0;
        }
        //if (tbl) return cnt; // if a new part.has been created, return now
    }
    catch (const char* s) {
        logMessage("gatherParts", "received a string "
                   "exception -- %s", s);
    }
    catch (const std::exception &e) {
        logMessage("gatherParts", "received a library "
                   "exception -- %s", e.what());
    }
    catch (...) {
        logMessage("gatherParts", "received an unexpected "
                   "exception");
    }
#if defined(HAVE_DIRENT_H) || defined(__unix__) || defined(__HOS_AIX__) || defined(__APPLE__) || defined(_XOPEN_SOURCE) || defined(_POSIX_C_SOURCE)
    if (bdir == 0) return cnt; // must have both adir and bdir
    // on unix machines, the directories adir and bdir may contain
    // subdirectories -- this section of code reads the subdirectories to
    // generate more tables
    char nm1[PATH_MAX], nm2[PATH_MAX];
    uint32_t j = std::strlen(adir);
    uint32_t len = std::strlen(bdir);
    len = (len<j) ? j : len;

    DIR* dirp = opendir(adir);
    if (dirp == 0) return cnt;
    struct dirent* ent = 0;
    while ((ent = readdir(dirp)) != 0) {
        if ((ent->d_name[1] == 0 || ent->d_name[1] == '.') &&
            ent->d_name[0] == '.') { // skip '.' and '..'
            continue;
        }
        if (len + std::strlen(ent->d_name)+2 >= PATH_MAX) {
            LOGGER(ibis::gVerbose >= 0)
                << "util::gatherParts name (" << adir << FASTBIT_DIRSEP
                << ent->d_name << " | " << bdir << FASTBIT_DIRSEP
                << ent->d_name << ") too long";
            continue;
        }

        sprintf(nm1, "%s%c%s", adir, FASTBIT_DIRSEP, ent->d_name);
        sprintf(nm2, "%s%c%s", bdir, FASTBIT_DIRSEP, ent->d_name);
        struct stat st1, st2;
        if (stat(nm1, &st1)==0 && stat(nm2, &st2)==0) {
            if (((st1.st_mode  &S_IFDIR) == S_IFDIR) &&
                ((st2.st_mode  &S_IFDIR) == S_IFDIR)) {
                cnt += gatherParts(tlist, nm1, nm2, ro);
            }
        }
    }
    closedir(dirp);
#endif
    return cnt;
} // ibis::util::gatherParts

/// Read the parameters dataDir1 and dataDir2 to build data partitions.
///
/// Returns the number of data partitions found.
unsigned ibis::util::gatherParts(ibis::partList &tables,
                                 const ibis::resource &res, bool ro) {
    unsigned int cnt = 0;
    const char* dir1 = res.getValue("activeDir");
    if (dir1 == 0) {
        dir1 = res.getValue("dataDir1");
        if (dir1 == 0) {
            dir1 = res.getValue("activeDirectory");
            if (dir1 == 0) {
                dir1 = res.getValue("dataDir");
                if (dir1 == 0) {
                    dir1 = res.getValue("dataDirectory");
                    if (dir1 == 0) {
                        dir1 = res.getValue("indexDir");
                        if (dir1 == 0) {
                            dir1 = res.getValue("indexDirectory");
                        }
                    }
                }
            }
        }
    }
    if (dir1) {
        const char* dir2 = res.getValue("backupDir");
        if (dir2 == 0) {
            dir2 = res.getValue("DataDir2");
            if (dir2 == 0) {
                dir2 = res.getValue("backupDirectory");
            }
        }
        if (dir2 != 0 && *dir2 != 0)
            cnt = gatherParts(tables, dir1, dir2, ro);
        else
            cnt = gatherParts(tables, dir1, ro);
    }

    for (ibis::resource::gList::const_iterator it = res.gBegin();
         it != res.gEnd(); ++it)
        cnt += gatherParts(tables, *((*it).second), ro);
    return cnt;
} // ibis::util::gatherParts

/// Deallocate the list of data partitions.
void ibis::util::clear(ibis::partList &pl) throw() {
    const uint32_t npl = pl.size();
    for (uint32_t j = 0; j < npl; ++ j)
        delete pl[j];
    pl.clear();
} // ibis::util::clear

/// Update the metadata about the data partitions.
/// Loop through all known data partitions and check for any update in the
/// metadata files.
void ibis::util::updateDatasets() {
    const uint32_t npt = ibis::datasets.size();
    for (uint32_t j = 0; j < npt; ++ j)
        ibis::datasets[j]->updateData();
} // ibis::util::updateDatasets

/// Attempt to remove all currently unused data from memory cache.
///
/// @note this function was previously called cleanDatasets.  The new name
/// should be more precisely describing it actual function.  It also
/// matches the function is ibis::part::emptyCache.
void ibis::util::emptyCache() {
    const uint32_t npt = ibis::datasets.size();
    for (uint32_t j = 0; j < npt; ++ j) {
        ibis::datasets[j]->emptyCache();
        // if (ibis::datasets[j]->tryWriteAccess() == 0) {
        //     ibis::datasets[j]->emptyCache();
        //     ibis::datasets[j]->releaseAccess();
        // }
    }
} // ibis::util::emptyCache

// explicit instantiations of the templated functions
template long
ibis::part::doScan(const array_t<char>&,
                   const ibis::qRange&, const ibis::bitvector&,
                   ibis::bitvector&);
template long
ibis::part::doScan(const array_t<signed char>&,
                   const ibis::qRange&, const ibis::bitvector&,
                   ibis::bitvector&);
template long
ibis::part::doScan(const array_t<unsigned char>&,
                   const ibis::qRange&, const ibis::bitvector&,
                   ibis::bitvector&);
template long
ibis::part::doScan(const array_t<int16_t>&,
                   const ibis::qRange&, const ibis::bitvector&,
                   ibis::bitvector&);
template long
ibis::part::doScan(const array_t<uint16_t>&,
                   const ibis::qRange&, const ibis::bitvector&,
                   ibis::bitvector&);
template long
ibis::part::doScan(const array_t<int32_t>&,
                   const ibis::qRange&, const ibis::bitvector&,
                   ibis::bitvector&);
template long
ibis::part::doScan(const array_t<uint32_t>&,
                   const ibis::qRange&, const ibis::bitvector&,
                   ibis::bitvector&);
template long
ibis::part::doScan(const array_t<int64_t>&,
                   const ibis::qRange&, const ibis::bitvector&,
                   ibis::bitvector&);
template long
ibis::part::doScan(const array_t<uint64_t>&,
                   const ibis::qRange&, const ibis::bitvector&,
                   ibis::bitvector&);
template long
ibis::part::doScan(const array_t<float>&,
                   const ibis::qRange&, const ibis::bitvector&,
                   ibis::bitvector&);
template long
ibis::part::doScan(const array_t<double>&,
                   const ibis::qRange&, const ibis::bitvector&,
                   ibis::bitvector&);
template long
ibis::part::doCompare<signed char>
(const ibis::array_t<signed char>&, const ibis::qRange&,
 const ibis::bitvector&, ibis::bitvector&);
template long
ibis::part::doCompare(const array_t<unsigned char>&,
                      const ibis::qRange&, const ibis::bitvector&,
                      ibis::bitvector&);
template long
ibis::part::doCompare(const array_t<int16_t>&,
                      const ibis::qRange&, const ibis::bitvector&,
                      ibis::bitvector&);
template long
ibis::part::doCompare(const array_t<uint16_t>&,
                      const ibis::qRange&, const ibis::bitvector&,
                      ibis::bitvector&);
template long
ibis::part::doCompare(const array_t<int32_t>&,
                      const ibis::qRange&, const ibis::bitvector&,
                      ibis::bitvector&);
template long
ibis::part::doCompare(const array_t<uint32_t>&,
                      const ibis::qRange&, const ibis::bitvector&,
                      ibis::bitvector&);
template long
ibis::part::doCompare(const array_t<int64_t>&,
                      const ibis::qRange&, const ibis::bitvector&,
                      ibis::bitvector&);
template long
ibis::part::doCompare(const array_t<uint64_t>&,
                      const ibis::qRange&, const ibis::bitvector&,
                      ibis::bitvector&);
template long
ibis::part::doCompare(const array_t<float>&,
                      const ibis::qRange&, const ibis::bitvector&,
                      ibis::bitvector&);
template long
ibis::part::doCompare(const array_t<double>&,
                      const ibis::qRange&, const ibis::bitvector&,
                      ibis::bitvector&);
template long
ibis::part::doCompare<signed char>
(const char*, const ibis::qRange&,
 const ibis::bitvector&, ibis::bitvector&);

template int ibis::part::writeColumn<ibis::rid_t>
(int, ibis::bitvector::word_t, ibis::bitvector::word_t, ibis::bitvector::word_t,
 const array_t<ibis::rid_t>&, const ibis::rid_t&, ibis::bitvector&,
 const ibis::bitvector&);
template int ibis::part::writeColumn<signed char>
(int, ibis::bitvector::word_t, ibis::bitvector::word_t, ibis::bitvector::word_t,
 const array_t<signed char>&, const signed char&,
 ibis::bitvector&, const ibis::bitvector&);
template int ibis::part::writeColumn<unsigned char>
(int, ibis::bitvector::word_t, ibis::bitvector::word_t, ibis::bitvector::word_t,
 const array_t<unsigned char>&, const unsigned char&,
 ibis::bitvector&, const ibis::bitvector&);
template int ibis::part::writeColumn<int16_t>
(int, ibis::bitvector::word_t, ibis::bitvector::word_t, ibis::bitvector::word_t,
 const array_t<int16_t>&, const int16_t&,
 ibis::bitvector&, const ibis::bitvector&);
template int ibis::part::writeColumn<uint16_t>
(int, ibis::bitvector::word_t, ibis::bitvector::word_t, ibis::bitvector::word_t,
 const array_t<uint16_t>&, const uint16_t&,
 ibis::bitvector&, const ibis::bitvector&);
template int ibis::part::writeColumn<int32_t>
(int, ibis::bitvector::word_t, ibis::bitvector::word_t, ibis::bitvector::word_t,
 const array_t<int32_t>&, const int32_t&,
 ibis::bitvector&, const ibis::bitvector&);
template int ibis::part::writeColumn<uint32_t>
(int, ibis::bitvector::word_t, ibis::bitvector::word_t, ibis::bitvector::word_t,
 const array_t<uint32_t>&, const uint32_t&,
 ibis::bitvector&, const ibis::bitvector&);
template int ibis::part::writeColumn<int64_t>
(int, ibis::bitvector::word_t, ibis::bitvector::word_t, ibis::bitvector::word_t,
 const array_t<int64_t>&, const int64_t&,
 ibis::bitvector&, const ibis::bitvector&);
template int ibis::part::writeColumn<uint64_t>
(int, ibis::bitvector::word_t, ibis::bitvector::word_t, ibis::bitvector::word_t,
 const array_t<uint64_t>&, const uint64_t&,
 ibis::bitvector&, const ibis::bitvector&);
template int ibis::part::writeColumn<float>
(int, ibis::bitvector::word_t, ibis::bitvector::word_t, ibis::bitvector::word_t,
 const array_t<float>&, const float&,
 ibis::bitvector&, const ibis::bitvector&);
template int ibis::part::writeColumn<double>
(int, ibis::bitvector::word_t, ibis::bitvector::word_t, ibis::bitvector::word_t,
 const array_t<double>&, const double&,
 ibis::bitvector&, const ibis::bitvector&);
