// File: $Id$
// Author: John Wu <John.Wu at ACM.org>
//         Lawrence Berkeley National Laboratory
// Copyright (c) 2000-2016 the Regents of the University of California
//
// This file contains the implementation details of the classes defined in
// bundle.h
//
#if defined(_WIN32) && defined(_MSC_VER)
#pragma warning(disable:4786)   // some identifier longer than 256 characters
#endif
#include "bundle.h"
#include "utilidor.h"   // ibis::util::reorder
#include <sstream>      // std::ostringstream

#define FASTBIT_SYNC_WRITE 1

//////////////////////////////////////////////////////////////////////
// functions of class bundle

/// Create new bundle from a query object.  Write info to q.dir().
/// @param dir:
/// - > 0 sort RIDs,
/// - < 0 do not sort RIDs, leave them in whatever order after sorting the
///       order-by keys,
/// - = 0 if FASTBIT_ORDER_OUTPUT_RIDS is defined, sort RIDs, otherwise
///       don't sort.
ibis::bundle* ibis::bundle::create(const ibis::query& q, int dir) {
    ibis::bundle* bdl = 0;
    try {
        ibis::horometer timer;
        if (ibis::gVerbose > 2)
            timer.start();

        if (q.components().empty())
            bdl = new ibis::bundle0(q);
        else if (q.components().aggSize() == 1)
            bdl = new ibis::bundle1(q, dir);
        else
            bdl = new ibis::bundles(q, dir);

        if (ibis::gVerbose > 2) {
            timer.stop();
            q.logMessage("createBundle", "time to generate the bundle: "
                         "%g sec(CPU), %g sec(elapsed)", timer.CPUTime(),
                         timer.realTime());
        }
    }
    catch (...) {
        bdl = 0;
    }
    return bdl;
} // ibis::bundle::create

/// Create a new bundle from previously stored information.
ibis::bundle* ibis::bundle::create(const ibis::query& q,
                                   const ibis::bitvector& hits,
                                   int dir) {
    if (hits.size() == 0 || hits.cnt() == 0)
        return 0;
    ibis::bundle* bdl = 0;
    try {
        ibis::horometer timer;
        if (ibis::gVerbose > 2)
            timer.start();

        if (q.components().empty())
            bdl = new ibis::bundle0(q, hits);
        else if (q.components().aggSize() == 1)
            bdl = new ibis::bundle1(q, hits, dir);
        else
            bdl = new ibis::bundles(q, hits, dir);

        if (ibis::gVerbose > 2) {
            timer.stop();
            q.logMessage("createBundle", "time to generate the bundle: "
                         "%g sec(CPU), %g sec(elapsed)", timer.CPUTime(),
                         timer.realTime());
        }
    }
    catch (...) {
        bdl = 0;
    }
    return bdl;
} // ibis::bundle::create

/// Create a bundle using the all values of the partition.
ibis::bundle* ibis::bundle::create(const ibis::part& tbl,
                                   const ibis::selectClause& sel,
                                   int dir) {
    ibis::bundle* res = 0;
    try {
        const uint32_t nc = sel.aggSize();
        bool cs = (nc == 1 && sel.getAggregator(0) == ibis::selectClause::CNT);
        // if (cs) {
        //      const ibis::math::term *tm = sel.aggExpr(0);
        //      if (tm->termType() == ibis::math::VARIABLE)
        //          cs = ('*' == *(static_cast<const ibis::math::variable*>(tm)->
        //                         variableName()));
        //      else
        //          cs = false;
        // }
        if (nc == 0 || cs) {
            res = new ibis::bundle0(tbl, sel);
        }
        else if (nc == 1) {
            res = new ibis::bundle1(tbl, sel, dir);
        }
        else {
            res = new ibis::bundles(tbl, sel, dir);
        }
    }
    catch (...) {
        res = 0;
    }
    return res;
} // ibis::bundle::create

/// Sort RIDs in the range of [i, j)
void ibis::bundle::sortRIDs(uint32_t i, uint32_t j) {
    std::less<ibis::rid_t> cmp;
    if (i+32 >= j) { // use buble sort
        for (uint32_t i1=j-1; i1>i; --i1) {
            for (uint32_t i2=i; i2<i1; ++i2) {
                if (cmp((*rids)[i2+1], (*rids)[i2]))
                    swapRIDs(i2, i2+1);
            }
        }
    }
    else { // use quick sort
        ibis::rid_t tmp = (*rids)[(i+j)/2];
        uint32_t i1 = i;
        uint32_t i2 = j-1;
        bool left = cmp((*rids)[i1], tmp);
        bool right = !cmp((*rids)[i2], tmp);
        while (i1 < i2) {
            if (left && right) {
                // both i1 and i2 are on the correct size
                ++ i1; --i2;
                left = cmp((*rids)[i1], tmp);
                right = !cmp((*rids)[i2], tmp);
            }
            else if (right) {
                // i2 is on the correct size
                -- i2;
                right = !cmp((*rids)[i2], tmp);
            }
            else if (left) {
                // i1 is on the correct side
                ++ i1;
                left = cmp((*rids)[i1], tmp);
            }
            else { // both on the wrong side, swap them
                swapRIDs(i2, i1);
                ++ i1; -- i2;
                left = cmp((*rids)[i1], tmp);
                right = !cmp((*rids)[i2], tmp);
            }
        }
        i1 += left; // if left is true, rids[i1] should be on the left
        // everything below i1 is less than tmp
        if (i1 > i) {
            sortRIDs(i, i1);
            sortRIDs(i1, j);
        }
        else { // nothing has been swapped, i.e., tmp is the smallest
            while (i1 < j &&
                   0 == memcmp(&tmp, &((*rids)[i1]), sizeof(ibis::rid_t)))
                ++i1;
            if (i1+i1 < i+j) {
                swapRIDs(i1, (i+j)/2);
                ++i1;
            }
            sortRIDs(i1, j);
        }
    }
} // ibis::bundle::sortRIDs

/// Read the RIDs related to the ith bundle.
const ibis::RIDSet* ibis::bundle::readRIDs(const char* dir,
                                           const uint32_t i) {
    if (dir == 0) return 0;

    char fn[PATH_MAX];
    uint32_t len = std::strlen(dir);
    if (len+8 >= PATH_MAX) {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- bundle::readRIDs -- argument dir (" << dir
            << ") too long" IBIS_FILE_LINE;
        throw "bundle::readRIDs -- argument dir too long" IBIS_FILE_LINE;
    }

    if (dir[len-1] == FASTBIT_DIRSEP) {
        strcpy(fn, dir);
        strcat(fn, "bundles");
    }
    else {
        ++len;
        sprintf(fn, "%s%cbundles", dir, FASTBIT_DIRSEP);
    }
    ibis::fileManager::storage* bdlstore=0;
    int ierr = ibis::fileManager::instance().getFile(fn, &bdlstore);
    if (ierr != 0) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- bundle::readRIDs failed to retrieve the bundle file "
            << fn;
        return 0;
    }
    uint32_t ncol, nbdl, offset;
    bdlstore->beginUse(); // obtain a read lock on dblstore
    { // get the first two numbers out of bdlstore
        ibis::array_t<uint32_t> tmp(bdlstore, 0, 2);
        nbdl = tmp[0];
        ncol = tmp[1];
    }
    { // verify the file contains the right number of bytes
        ibis::array_t<uint32_t> sizes(bdlstore, 2*sizeof(uint32_t), ncol);
        uint32_t expected = sizeof(uint32_t)*(ncol+3+nbdl);
        for (uint32_t i0 = 0; i0 < ncol; ++i0)
            expected += sizes[i0] * nbdl;
        if (expected != bdlstore->bytes()) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- bundle::readRIDs -- according to the header, "
                << expected << " bytes are expected, but the file " << fn
                << "contains " << bdlstore->bytes();
            return 0;
        }
        offset = expected - sizeof(uint32_t)*(nbdl+1);
    }

    ibis::array_t<uint32_t> starts(bdlstore, offset, nbdl+1);
    bdlstore->endUse(); // this function no longer needs the read lock
    if (i < nbdl) { // open the rid file and read the selected segment
        ibis::RIDSet* res = new ibis::RIDSet;
        strcpy(fn+len, "-rids");
        int fdes = UnixOpen(fn, OPEN_READONLY);
        if (fdes < 0) {
            LOGGER(errno != ENOENT || ibis::gVerbose > 10)
                << "Warning -- bundle::readRIDs -- failed to open file \""
                << fn << "\" ... "
                << (errno ? strerror(errno) : "no free stdio stream");
            delete res;
            return 0;
        }
#if defined(_WIN32) && defined(_MSC_VER)
        (void)_setmode(fdes, _O_BINARY);
#endif
        offset = sizeof(ibis::rid_t) * starts[i];
        if (offset != static_cast<uint32_t>(UnixSeek(fdes, offset, SEEK_SET))) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- bundle::readRIDs -- failed to fseek to "
                << offset << " in file " << fn;
            delete res;
            return 0;
        }

        len = starts[i+1] - starts[i];
        res->resize(len);
        offset = UnixRead(fdes, res->begin(), sizeof(ibis::rid_t)*len);
        ibis::fileManager::instance().recordPages
            (sizeof(ibis::rid_t)*starts[i], sizeof(ibis::rid_t)*starts[i+1]);
        UnixClose(fdes);
        if (offset != sizeof(ibis::rid_t)*len) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- bundle::readRIDs -- expected to read "
                << len << " RIDs but got " << nbdl;
            delete res;
            return 0;
        }
        else {
            return res;
        }
    }
    else {
        return 0;
    }
} // ibis::bundle::readRIDs

/// Return the maximal int value.
int32_t ibis::bundle::getInt(uint32_t, uint32_t) const {
    return 0x7FFFFFFF;
} // ibis::bundle::getInt

/// Return the maximal unsigned int value.
uint32_t ibis::bundle::getUInt(uint32_t, uint32_t) const {
    return 0xFFFFFFFFU;
} // ibis::bundle::getUInt

/// Return the maximal int value.
int64_t ibis::bundle::getLong(uint32_t, uint32_t) const {
    return 0x7FFFFFFFFFFFFFFFLL;
} // ibis::bundle::getLong

/// Return the maximal unsigned int value.
uint64_t ibis::bundle::getULong(uint32_t, uint32_t) const {
    return 0xFFFFFFFFFFFFFFFFULL;
} // ibis::bundle::getULong

/// Return the maximal float value.
float ibis::bundle::getFloat(uint32_t, uint32_t) const {
    return FLT_MAX;
} // ibis::bundle::getFloat

/// Return the maximum double value.
double ibis::bundle::getDouble(uint32_t, uint32_t) const {
    return DBL_MAX;
} // ibis::bundle::getDouble

/// Return an empty string.  Could have thrown an exception, but that
/// seemed to be a little too heavy handed.
std::string ibis::bundle::getString(uint32_t, uint32_t) const {
    std::string ret;
    return ret;
} // ibis::bundle::getString

uint32_t ibis::bundle::rowCounts(array_t<uint32_t>& cnt) const {
    cnt.clear();
    if (starts == 0) return 0;

    const uint32_t ng = starts->size()-1;
    cnt.resize(ng);
    for (unsigned i = 0; i < ng; ++ i)
        cnt[i] = (*starts)[i+1] - (*starts)[i];
    return ng;
} // ibis::bundle::rowCounts

ibis::bundle::~bundle() {
    delete rids;
    delete starts;
}

//////////////////////////////////////////////////////////////////////
// functions for ibis::bundle0

/// Constructor.
ibis::bundle0::bundle0(const ibis::query& q) : bundle(q) {
    q.writeRIDs(rids);
}

/// Constructor.
ibis::bundle0::bundle0(const ibis::query& q, const ibis::bitvector& hits)
    : bundle(q, hits) {
    if (rids != 0 && static_cast<long>(rids->size()) != q.getNumHits()) {
        delete rids;
        rids = 0;
    }
}

/// Constructor.
ibis::bundle0::bundle0(const ibis::part &t, const ibis::selectClause &s)
    : bundle(t, s) {
}

/// Print the bundle values to the specified output stream.
void ibis::bundle0::print(std::ostream& out) const {
    out << "bundle " << id << " is empty" << std::endl;
}

/// Print the bundle values along with the RIDs.
void ibis::bundle0::printAll(std::ostream& out) const {
    //ibis::util::ioLock lock;
    if (rids) {
        // print all RIDs one on a line
        ibis::RIDSet::const_iterator it;
        if (ibis::gVerbose > 2)
            out << "IDs of all qualified rows for bundle " << id
                << " (one per line)" << std::endl;
        for (it = rids->begin(); it != rids->end(); ++it) {
            out << *it << std::endl;;
        }
        out << std::endl;
    }
    else if (ibis::gVerbose > 1) {
        out << "No RIDS for bundle " << id << std::endl;
    }
} // ibis::bundle0::printAll

//////////////////////////////////////////////////////////////////////
// functions for ibis::bundle1
//
/// Constructor.  It attempt to read to read a bundle from files first.  If
/// that fails, it attempts to create a bundle based on the current hits.
ibis::bundle1::bundle1(const ibis::query& q, int dir)
    : bundle(q), col(0), aggr(comps.getAggregator(0)) {
    if (q.getNumHits() == 0)
        return;

    char bdlfile[PATH_MAX];
    const ibis::part* tbl = q.partition();
    if (q.dir()) {
        strcpy(bdlfile, q.dir());
        strcat(bdlfile, "bundles");
    }
    else {
        bdlfile[0] = 0;
    }
    if (comps.empty()) {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- bundle1 can not continue with an empty "
            "select clause" IBIS_FILE_LINE;
        throw "bundle1 can not work with empty select clauses" IBIS_FILE_LINE;
    }
    LOGGER(comps.aggSize() != 1 && ibis::gVerbose > 0)
        << "Warning -- bundle1 will only use the 1st term out of "
        << comps.aggSize() << " (" << *comps << ')';
    ibis::column* c = tbl->getColumn(comps.aggName(0));
    if (c == 0) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- bundle1::ctor name " << comps.aggName(0)
            << " is not a column in table ";
        return;
    }

    try {
        if (ibis::util::getFileSize(bdlfile) > 0) {
            // file bundles exists, read it
            if (rids == 0) {
                rids = q.readRIDs(); // read RIDs
                if (rids != 0 && static_cast<long>(rids->size()) !=
                    q.getNumHits()) {
                    delete rids;
                    rids = 0;
                }
            }
            ibis::fileManager::storage* bdlstore = 0;
            int ierr =
                ibis::fileManager::instance().getFile(bdlfile, &bdlstore);
            if (ierr != 0) {
                LOGGER(ibis::gVerbose >= 0)
                    << "Warning -- bundle1::ctor failed to retrieve bundle "
                    "file " << bdlfile << IBIS_FILE_LINE;
                throw ibis::bad_alloc("failed to retrieve bundle file"
                                      IBIS_FILE_LINE);
            }
            // no need to explicitly call beginUse() because array_t sizes will
            // hold a read lock long enough until starts holds another one for
            // the life time of this object
            ibis::array_t<uint32_t> sizes(bdlstore, 0, 3);
            uint32_t expected = sizeof(uint32_t)*(sizes[0]+4) +
                sizes[0]*sizes[2];
            if (bdlstore->bytes() == expected) {
                if (aggr == ibis::selectClause::NIL_AGGR) {
                    LOGGER(ibis::gVerbose > 4)
                        << "bundle1::ctor constructing a colValues for \""
                        << comps.aggName(0) << '"';
                    col = ibis::colValues::create
                        (c, bdlstore, 3*sizeof(uint32_t), sizes[0]);
                }
                else {
                    switch (aggr) {
                    case ibis::selectClause::AVG:
                    case ibis::selectClause::SUM:
                    case ibis::selectClause::VARPOP:
                    case ibis::selectClause::VARSAMP:
                    case ibis::selectClause::STDPOP:
                    case ibis::selectClause::STDSAMP:
                        LOGGER(ibis::gVerbose > 4)
                            << "bundle1::ctor constructing a colDoubles for \""
                            << *(comps.aggExpr(0)) << '"';
                        col = new ibis::colDoubles
                            (c, bdlstore, 3*sizeof(uint32_t),
                             3*sizeof(uint32_t)+8*sizes[0]);
                        break;
                    default:
                        LOGGER(ibis::gVerbose > 4)
                            << "bundle1::ctor constructing a colValues for \""
                            << *(comps.aggExpr(0)) << '"';
                        col = ibis::colValues::create
                            (c, bdlstore, 3*sizeof(uint32_t),
                             3*sizeof(uint32_t)+sizes[0]*c->elementSize());
                        break;
                    }
                }
                starts = new ibis::array_t<uint32_t>
                    (bdlstore, 3*sizeof(uint32_t)+sizes[0]*sizes[2],
                     sizes[0]+1);
                infile = true;
            }
            else {
                LOGGER(ibis::gVerbose > 0)
                    << "Warning -- bundle1::ctor -- according to the header, "
                    << expected << " bytes are expected, but the file "
                    << bdlfile << "contains " << bdlstore->bytes();
            }
        }

        if (starts == 0) { // use the current hit vector
            const ibis::bitvector* hits = q.getHitVector();
            if (hits != 0 && hits->sloppyCount() > 0) {
                if (rids == 0) {
                    rids = tbl->getRIDs(*hits);
                    if (rids != 0 && rids->size() != hits->cnt()) {
                        delete rids;
                        rids = 0;
                    }
                }
                if (aggr == ibis::selectClause::NIL_AGGR) {
                    LOGGER(ibis::gVerbose > 4)
                        << "bundle1::ctor initializing a colValues for \""
                        << *(comps.aggExpr(0)) << '"';
                    col = ibis::colValues::create(c, *hits);
                }
                else {
                    switch (aggr) {
                    case ibis::selectClause::AVG:
                    case ibis::selectClause::SUM:
                    case ibis::selectClause::VARPOP:
                    case ibis::selectClause::VARSAMP:
                    case ibis::selectClause::STDPOP:
                    case ibis::selectClause::STDSAMP:
                        LOGGER(ibis::gVerbose > 4)
                            << "bundle1::ctor initializing a colDoubles for \""
                            << *(comps.aggExpr(0)) << '"';
                        col = new ibis::colDoubles(c, *hits);
                        break;
                    default:
                        LOGGER(ibis::gVerbose > 4)
                            << "bundle1::ctor initializing a colValues for \""
                            << *(comps.aggExpr(0)) << '"';
                        col = ibis::colValues::create(c, *hits);
                        break;
                    }
                }
                if (col->size() != hits->cnt()) {
                    LOGGER(ibis::gVerbose >= 0)
                        << "Warning -- bundle1::ctor got " << col->size()
                        << " value" << (col->size()>1?"s":"")
                        << " but expected " << hits->cnt() << IBIS_FILE_LINE;
                    delete col;
                    col = 0;
                    throw ibis::bad_alloc("incorrect number of bundles"
                                          IBIS_FILE_LINE);
                }
            }
            sort(dir);
        }

        if (ibis::gVerbose > 5) {
            ibis::util::logger lg;
            lg() << "query[" << q.id()
                 << "]::bundle1 -- generated the bundle\n";
            if (rids == 0) {
                print(lg());
            }
            else if (ibis::gVerbose > 8) {
                printAll(lg());
            }
            else {
                print(lg());
            }
        }
    }
    catch (...) {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- bundle1::ctor received an exception, freeing col"
            IBIS_FILE_LINE;
        delete col;
        throw; // rethrow the exception
    }
} // ibis::bundle1::bundle1

/// Constructor.  It creates a bundle using the rows selected by hits.
ibis::bundle1::bundle1(const ibis::query& q, const ibis::bitvector& hits,
                       int dir)
    : bundle(q, hits), col(0), aggr(comps.getAggregator(0)) {
    if (hits.cnt() == 0)
        return;

    const ibis::part* tbl = q.partition();
    if (rids == 0) {
        rids = tbl->getRIDs(hits);
        if (rids != 0 && rids->size() != hits.cnt()) {
            delete rids;
            rids = 0;
        }
    }
    ibis::column* c = tbl->getColumn(comps.aggName(0));
    try {
        if (c != 0) {
            if (aggr == ibis::selectClause::NIL_AGGR) {
                // use column type
                LOGGER(ibis::gVerbose > 4)
                    << "bundle1::ctor initializing a colValues for \""
                    << *(comps.aggExpr(0)) << '"';
                col = ibis::colValues::create(c, hits);
            }
            else { // a function, treat AVG and SUM as double
                switch (aggr) {
                case ibis::selectClause::AVG:
                case ibis::selectClause::SUM:
                case ibis::selectClause::VARPOP:
                case ibis::selectClause::VARSAMP:
                case ibis::selectClause::STDPOP:
                case ibis::selectClause::STDSAMP:
                    LOGGER(ibis::gVerbose > 4)
                        << "bundle1::ctor initializing a colDoubles for \""
                        << *(comps.aggExpr(0)) << '"';
                    col = new ibis::colDoubles(c, hits);
                    break;
                case ibis::selectClause::CONCAT:
                    LOGGER(ibis::gVerbose > 4)
                        << "bundle1::ctor initializing a colStrings for \""
                        << *(comps.aggExpr(0)) << '"';
                    col = new ibis::colStrings(c, hits);
                    break;
                default:
                    LOGGER(ibis::gVerbose > 4)
                        << "bundle1::ctor initializing a colValues for \""
                        << *(comps.aggExpr(0)) << '"';
                    col = ibis::colValues::create(c, hits);
                    break;
                }
            }
            if (col->size() != hits.cnt()) {
                LOGGER(ibis::gVerbose >= 0)
                    << "Warning -- bundle1::ctor got "
                    << col->size() << " value"
                    << (col->size()>1?"s":"") << ", but expected "
                    << hits.cnt() << IBIS_FILE_LINE;
                delete col;
                col = 0;
                throw ibis::bad_alloc("incorrect number of bundles"
                                      IBIS_FILE_LINE);
            }
        }
        else {
            LOGGER(ibis::gVerbose >= 0)
                << "Warning -- bundle1::ctor name \"" << comps.aggName(0)
                << "\" is not a column in table " << tbl->name()
                << IBIS_FILE_LINE;
            throw ibis::bad_alloc("not a valid column name" IBIS_FILE_LINE);
        }
        sort(dir);

        if (ibis::gVerbose > 5) {
            ibis::util::logger lg;
            lg() << "query[" << q.id()
                 << "]::bundle1 -- generated the bundle\n";
            if (rids == 0) {
                print(lg());
            }
            else if (ibis::gVerbose > 8) {
                printAll(lg());
            }
            else {
                print(lg());
            }
        }
    }
    catch (...) {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- bundle1::ctor received an exception, freeing col"
            IBIS_FILE_LINE;
        delete col;
        throw; // rethrow the exception
    }
} // ibis::bundle1::bundle1

/// Constructor.  It creates the bundle using all rows of tbl.
ibis::bundle1::bundle1(const ibis::part& tbl, const ibis::selectClause& cmps,
                       int dir)
    : bundle(cmps), col(0), aggr(comps.getAggregator(0)) {
    if (comps.empty())
        return;

    id = tbl.name();
    uint32_t icol = 0;
    const ibis::math::term* tm = 0;
    while (tm == 0 && icol < comps.aggSize()) {
        tm = comps.aggExpr(icol);
        if (tm->termType() == ibis::math::VARIABLE) {
            if (*(static_cast<const ibis::math::variable*>(tm)->variableName())
                == '*') {
                tm = 0;
                ++ icol;
            }
        }
    }
    if (tm == 0 || icol >= comps.aggSize()) {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- bundle1::ctor failed to locate a valid column "
            "name in " << comps << IBIS_FILE_LINE;
        throw "bundle1::ctor can not find a column name" IBIS_FILE_LINE;
    }

    ibis::column* c = 0;
    if (tm->termType() == ibis::math::VARIABLE)
        c = tbl.getColumn(static_cast<const ibis::math::variable*>(tm)->
                          variableName());
    if (c == 0)
        c = tbl.getColumn(comps.aggName(icol));
    if (c == 0) {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- bundle1 constructor failed to find column "
            << comps.aggName(icol) << " in " << tbl.name() << IBIS_FILE_LINE;
        throw "bundle1::ctor can find the named column" IBIS_FILE_LINE;
    }

    try {
        aggr = comps.getAggregator(icol);
        if (aggr == ibis::selectClause::NIL_AGGR) {
            // use column type
            LOGGER(ibis::gVerbose > 4)
                << "bundle1::ctor initializing a colValues for \""
                << *(comps.aggExpr(icol)) << '"';
            col = ibis::colValues::create(c);
        }
        else { // a function, treat AVG and SUM as double
            switch (aggr) {
            case ibis::selectClause::AVG:
            case ibis::selectClause::SUM:
            case ibis::selectClause::VARPOP:
            case ibis::selectClause::VARSAMP:
            case ibis::selectClause::STDPOP:
            case ibis::selectClause::STDSAMP:
                LOGGER(ibis::gVerbose > 4)
                    << "bundle1::ctor initializing a colDoubles for \""
                    << *(comps.aggExpr(icol)) << '"';
                col = new ibis::colDoubles(c);
                break;
            case ibis::selectClause::CONCAT:
                LOGGER(ibis::gVerbose > 4)
                    << "bundle1::ctor initializing a colStrings for \""
                    << *(comps.aggExpr(icol)) << '"';
                col = new ibis::colStrings(c);
                break;
            default:
                LOGGER(ibis::gVerbose > 4)
                    << "bundle1::ctor initializing a colValues for \""
                    << *(comps.aggExpr(icol)) << '"';
                col = ibis::colValues::create(c);
                break;
            }
        }
        sort(dir);

        if (col == 0) {
            LOGGER(ibis::gVerbose >= 0)
                << "Warning -- bundle1::ctor failed to create an in-memory "
                "representation for " << *comps << IBIS_FILE_LINE;
            throw "bundle1::ctor failed to create a bundle" IBIS_FILE_LINE;
        }
        else if (ibis::gVerbose > 5) {
            ibis::util::logger lg;
            lg() << "bundle1 -- generated the bundle for \"" << *comps
                 << "\"\n";
            print(lg());
        }
    }
    catch (...) {
        LOGGER(ibis::gVerbose >= 0)
            << "Error -- bundle1::ctor received an exception, freeing col";
        delete col;
        throw; // rethrow the exception
    }
} // ibis::bundle1::bundle1

/// Destructor.
ibis::bundle1::~bundle1() {
    delete col;
    LOGGER(ibis::gVerbose > 5)
        << "bundle1[" << (id && *id ? id : "") << "] freed colValues @ "
        << static_cast<void*>(col);
}

/// Print the bundle values to the specified output stream.
/// print out the bundles without RIDs.
void ibis::bundle1::print(std::ostream& out) const {
    if (col == 0)
        return;

    uint32_t nbdl = col->size();
    if (ibis::gVerbose > 4)
        out << "Bundle1 " << id << " has " << nbdl
            << (col->canSort() ? " distinct" : "")
            << (nbdl > 1 ? " values" : " value")
            << std::endl;
    if (starts != 0 && ibis::gVerbose > 4) {
        if (ibis::gVerbose > 4)
            out << (*col)->name() << " (with counts)\n";
        for (uint32_t i=0; i < nbdl; ++i) {
            col->write(out, i);
            out << ",\t" << (*starts)[i+1] - (*starts)[i] << "\n";
        }
    }
    else {
        if (ibis::gVerbose > 4)
            out << *comps << "\n";
        for (uint32_t i=0; i < nbdl; ++i) {
            col->write(out, i);
            out << "\n";
        }
    }
} // ibis::bundle1::print

/// Print the bundle values along with the RIDs.
void ibis::bundle1::printAll(std::ostream& out) const {
    if (col == 0)
        return;

    if (rids != 0 && starts != 0) {
        //ibis::util::ioLock lock;
        uint32_t nbdl = col->size();
        if (ibis::gVerbose > 4)
            out << "Bundle " << id << " has " << nbdl
                << (col->canSort() ? " distinct" : "")
                << (nbdl > 1 ? " values" : " value")
                << " from " << rids->size()
                << (rids->size()>1 ? " rows" : " row") << std::endl;
        out << *comps << " : followed by RIDs\n";
        for (uint32_t i=0; i < nbdl; ++i) {
            col->write(out, i);
            out << ",\t";
            for (uint32_t j=(*starts)[i]; j < (*starts)[i+1]; ++j)
                out << (*rids)[j] << (j+1<(*starts)[i+1] ? ", " : "\n");
        }
    }
    else {
        print(out);
        return;
    }
} // ibis::bundle1::printAll

void ibis::bundle1::printColumnNames(std::ostream& out) const {
    if (col != 0)
        out << col->name();
} // ibis::bundle1::printColumnNames

/// Sort the rows.  Remove the duplicate elements and generate the
/// starts.
/// @param dir:
/// - > 0 sort RIDs,
/// - < 0 do not sort RIDs, leave them in whatever order after sorting the
///       order-by keys,
/// - = 0 if FASTBIT_ORDER_OUTPUT_RIDS is defined, sort RIDs, otherwise
///       don't sort.
void ibis::bundle1::sort(int dir) {
    if (col == 0) return;

    const uint32_t nrow = col->size();
    col->nosharing();
    if (ibis::gVerbose > 5) {
        ibis::util::logger lg;
        lg() << "bundle1[" << id << "]::sort starting with "
             << nrow << " row" << (nrow > 1 ? "s" : "");
#if _DEBUG+0 > 2 || DEBUG+0 > 1
        for (uint32_t j = 0; j < nrow; ++ j) {
            lg() << "\n";
            col->write(lg(), j);
        }
#endif
    }

    if (nrow < 2) { // no need to sort the rows, but may do aggregation
        starts = new array_t<uint32_t>((uint32_t)2);
        (*starts)[1] = nrow;
        (*starts)[0] = 0;

        if (aggr != ibis::selectClause::NIL_AGGR) {
            col->reduce(*starts, aggr);
        }
    }
    else if (comps.getAggregator(0) == ibis::selectClause::NIL_AGGR) {
        // sort according to the values
        col->sort(0, nrow, this);
        // determine the starting positions of the identical segments
        starts = col->segment();
        if (starts == 0) {
            LOGGER(ibis::gVerbose >= 0)
                << "Warning -- bundle1::sort failed to sort and segment "
                "the values of " << col->name() << " ("
                << ibis::TYPESTRING[static_cast<int>(col->getType())] << ")";
            return;
        }

        uint32_t nGroups = starts->size() - 1;
        if (nGroups < nrow) {    // erase the dupliate elements
            col->reduce(*starts);
            if (dir == 0) {
#ifdef FASTBIT_ORDER_OUTPUT_RIDS
                dir = 1;
#else
                dir = -1;
#endif
            }
            if (dir > 0 && rids != 0 && rids->size() == nrow) {
                for (uint32_t i=nGroups; i>0; --i)
                    sortRIDs((*starts)[i-1], (*starts)[i]);
            }
        }
    }
    else { // a function is involved
        starts = new array_t<uint32_t>((uint32_t)2);
        (*starts)[1] = nrow;
        (*starts)[0] = 0;
        col->reduce(*starts, aggr);
    }
#if _DEBUG+0 > 2 || DEBUG+0 > 1
    if (ibis::gVerbose > 5) {
        const size_t nGroups = starts->size()-1;
        ibis::util::logger lg;
        lg() << "DEBUG -- bundle1[" << id << "]::sort ending "
             << nGroups << " row" << (nGroups > 1 ? "s" : "");
        for (uint32_t j = 0; j < nGroups; ++ j) {
            lg() << "\n";
            col->write(lg(), j);
        }
    }
#endif
} // ibis::bundle1::sort

/// Change from ascending order to descending order.  Most lines of the
/// code deals with the re-ordering of the RIDs.
void ibis::bundle1::reverse() {
    if (col == 0 || starts == 0) return;
    if (starts->size() <= 2) return;
    const uint32_t ngroups = starts->size() - 1;

    col->nosharing();
    if (rids != 0) { // has a rid list
        array_t<uint32_t> cnts(ngroups);
        for (uint32_t i = 0; i < ngroups; ++ i)
            cnts[i] = (*starts)[i+1] - (*starts)[i];
        for (uint32_t i = 0; i+i < ngroups; ++ i) {
            const uint32_t j = ngroups - i - 1;
            {
                uint32_t tmp;
                tmp = (*starts)[i];
                (*starts)[i] = (*starts)[j];
                (*starts)[j] = tmp;
            }
            {
                uint32_t tmp;
                tmp = cnts[i];
                cnts[i] = cnts[j];
                cnts[j] = tmp;
            }
            col->swap(i, j);
        }

        ibis::RIDSet tmpids;
        tmpids.reserve(rids->size());
        for (uint32_t i = 0; i < ngroups; ++ i) {
            for (uint32_t j = 0; j < cnts[i]; ++ j)
                tmpids.push_back((*rids)[(*starts)[i]+j]);
        }
        rids->swap(tmpids);
        (*starts)[0] = 0;
        for (uint32_t i = 0; i <= ngroups; ++ i)
            (*starts)[i+1] = (*starts)[i] + cnts[i];
    }
    else {
        // turn starts into counts
        for (uint32_t i = 0; i < ngroups; ++ i)
            (*starts)[i] = (*starts)[i+1] - (*starts)[i];
        for (uint32_t i = 0; i < ngroups/2; ++ i) {
            const uint32_t j = ngroups-1-i;
            uint32_t tmp = (*starts)[i];
            (*starts)[i] = (*starts)[j];
            (*starts)[j] = tmp;
            col->swap(i, j);
        }
        // turn counts back into starts
        uint32_t sum = 0;
        for (uint32_t i = 0; i < ngroups; ++ i) {
            uint32_t tmp = (*starts)[i];
            (*starts)[i] = sum;
            sum += tmp;
        }
    }
} // ibis::bundle1::reverse

/// This single-argument version keep the first few records.
long ibis::bundle1::truncate(uint32_t keep) {
    if (col == 0 || starts == 0) return -2L;
    if (starts->size() <= 2) return -3L;
    const uint32_t ngroups = starts->size()-1;
    if (keep >= ngroups) return ngroups;
    if (keep == 0) {
        starts->clear();
        col->truncate(0);
        return 0;
    }

    if (rids != 0) {
        rids->resize((*starts)[keep]);
    }
    infile = false;
    starts->resize(keep+1);
    return col->truncate(keep);
} // ibis::bundle1::truncate

/// This two-argument version keeps a few records starting at a
/// user-specified row number.  Note that the row number starts with 0,
/// i.e., the first row has the row number 0.
long ibis::bundle1::truncate(uint32_t keep, uint32_t start) {
    if (col == 0 || starts == 0) return -2L;
    if (starts->size() <= 2) return -3L;
    const uint32_t ngroups = starts->size()-1;
    if (start >= ngroups || keep == 0) {
        starts->clear();
        col->truncate(0);
        return 0;
    }
    else if (keep >= ngroups && start == 0) {
        return ngroups;
    }

    const uint32_t end = (keep+start < ngroups ? keep+start : ngroups);
    keep = end - start;
    if (rids != 0) {
        rids->truncate((*starts)[end]-(*starts)[start], (*starts)[start]);
    }
    infile = false;
    starts->truncate(keep+1, start);
    if (start != 0) {
        const uint32_t offset = starts->front();
        for (array_t<uint32_t>::iterator it = starts->begin();
             it != starts->end(); ++ it)
            *it -= offset;
    }
    return col->truncate(keep, start);
} // ibis::bundle1::truncate

long ibis::bundle1::truncate(const char *, uint32_t keep) {
    // if (direction < 0) {
    //  reverse();
    //  infile = false;
    // }
    return truncate(keep);
}

void ibis::bundle1::write(const ibis::query& theQ) const {
    if (theQ.dir() == 0) return;
    if (col == 0) return;
    if (infile) return;
    uint32_t tmp = col->size();
    if (starts->size() != tmp+1) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- bundle1::write detected invalid bundle "
            "(starts->size(" << starts->size()
            << ") != col->size(" << tmp << ")+1)";
        return;
    }

    if (rids != 0)
        theQ.writeRIDs(rids); // write the RIDs

    uint32_t len = std::strlen(theQ.dir());
    char* fn = new char[len+16];
    strcpy(fn, theQ.dir());
    strcat(fn, "bundles");
    FILE* fptr = fopen(fn, "wb");
    if (fptr == 0) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- bundle1::write failed to open file \""
            << fn << "\" ... "
            << (errno ? strerror(errno) : "no free stdio stream");
        return;
    }

    IBIS_BLOCK_GUARD(fclose, fptr);
    int ierr = fwrite(&tmp, sizeof(uint32_t), 1, fptr);
    if (ierr != 1) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- bundle1::write failed to the number of rows to "
            << fn;
        return;
    }

    tmp = 1;
    ierr = fwrite(&tmp, sizeof(uint32_t), 1, fptr);
    tmp = col->elementSize();
    ierr = fwrite(&tmp, sizeof(uint32_t), 1, fptr);
    ierr = col->write(fptr);
    ierr = fwrite(starts->begin(), sizeof(uint32_t), starts->size(), fptr);
#if defined(FASTBIT_SYNC_WRITE)
    (void)fflush(fptr);
#endif
    //ierr = fclose(fptr);
    delete [] fn;
    infile = true;
} // ibis::bundle1::write

/// Retrieve the value of i-th row j-th column as a 32-bit integer.
/// Return the maximal value defined in the class numeric_limits if either
/// i or j is out of bounds.
int32_t ibis::bundle1::getInt(uint32_t i, uint32_t j) const {
    int32_t ret = 0x7FFFFFFF;
    if (col != 0 && i < col->size() && j == 0) { // indices i and j are valid
        ret = col->getInt(i);
    }
    return ret;
} // ibis::bundle1::getInt

/// Retrieve the value of i-th row j-th column as a 32-bit unsigned
/// integer.  Return the maximal value defined in the class numeric_limits
/// if either i or j is out of bounds.
uint32_t ibis::bundle1::getUInt(uint32_t i, uint32_t j) const {
    uint32_t ret = 0xFFFFFFFFU;
    if (col != 0 && i < col->size() && j == 0) { // indices i and j are valid
        ret = col->getUInt(i);
    }
    return ret;
} // ibis::bundle1::getUInt

/// Retrieve the value of i-th row j-th column as a 64-bit integer.
/// Return the maximal value defined in the class numeric_limits if either
/// i or j is out of bounds.
int64_t ibis::bundle1::getLong(uint32_t i, uint32_t j) const {
    int64_t ret = 0x7FFFFFFFFFFFFFFFLL;
    if (col != 0 && i < col->size() && j == 0) { // indices i and j are valid
        ret = col->getLong(i);
    }
    return ret;
} // ibis::bundle1::getLong

/// Retrieve the value of i-th row j-th column as a 64-bit unsigned
/// integer.  Return the maximal value defined in the class numeric_limits
/// if either i or j is out of bounds.
uint64_t ibis::bundle1::getULong(uint32_t i, uint32_t j) const {
    uint64_t ret = 0xFFFFFFFFFFFFFFFFULL;
    if (col != 0 && i < col->size() && j == 0) { // indices i and j are valid
        ret = col->getULong(i);
    }
    return ret;
} // ibis::bundle1::getULong

/// Retrieve the value of i-th row j-th column as a 32-bit floating-point
/// value.  Return the maximal value defined in the class numeric_limits if
/// wither i or j is out of bounds.
float ibis::bundle1::getFloat(uint32_t i, uint32_t j) const {
    float ret = FLT_MAX;
    if (col != 0 && i < col->size() && j == 0) { // indices i and j are valid
        ret = col->getFloat(i);
    }
    return ret;
} // ibis::bundle1::getFloat

/// Retrieve the value of i-th row j-th column as a 64-bit floating-point
/// value.  Return the maximal value defined in the class numeric_limits if
/// either i or j is out of bounds.
double ibis::bundle1::getDouble(uint32_t i, uint32_t j) const {
    double ret = DBL_MAX;
    if (col != 0 && i < col->size() && j == 0) { // indices i and j are valid
        ret = col->getDouble(i);
    }
    return ret;
} // ibis::bundle1::getDouble

/// Retrieve the value of i-th row j-th column as a string.  Returns an
/// empty string if either i or j is out of bounds, which can not be
/// distinguished from an actual empty string.  This function converts a
/// value to its string representation through @c std::ostringstream.
std::string ibis::bundle1::getString(uint32_t i, uint32_t j) const {
    std::ostringstream oss;
    if (col != 0 && i < col->size() && j == 0) { // indices i and j are valid
        col->write(oss, i);
    }
    return oss.str();
} // ibis::bundle1::getString

//////////////////////////////////////////////////////////////////////
// functions of ibis::bundles

/// Constructor.  It will use the hit vector of the query to generate a new
/// bundle, if it is not able to read the existing bundles.
ibis::bundles::bundles(const ibis::query& q, int dir) : bundle(q) {
    if (q.getNumHits() == 0)
        return;

    // this some how requires a copy constructor for ibis::bundles, which
    // can not be implemented without a copy constructor for all colValues
    // classes as well!  Use try-catch block!
    // ibis::util::guard myguard =
    //  ibis::util::objectGuard(*this, &ibis::bundles::clear);
    try {
        char bdlfile[PATH_MAX];
        const ibis::part* tbl = q.partition();
        if (q.dir() != 0) {
            strcpy(bdlfile, q.dir());
            strcat(bdlfile, "bundles");
        }
        else {
            bdlfile[0] = 0;
        }

        const uint32_t ncol = comps.aggSize();
        if (q.dir() != 0 && ibis::util::getFileSize(bdlfile) > 0) {
            // file bundles exists, attempt to read in its content
            if (rids == 0) {
                rids = q.readRIDs();
                if (rids != 0 && static_cast<long>(rids->size()) !=
                    q.getNumHits()) {
                    delete rids;
                    rids = 0;
                }
            }
            ibis::fileManager::storage* bdlstore=0;
            int ierr =
                ibis::fileManager::instance().getFile(bdlfile, &bdlstore);
            if (ierr != 0) {
                LOGGER(ibis::gVerbose >= 0)
                    << "Warning -- bundles::ctor failed to retrieve bundle "
                    "file " << bdlfile << IBIS_FILE_LINE;
                throw ibis::bad_alloc("failed to retrieve bundle file"
                                      IBIS_FILE_LINE);
            }
            // no need to explicitly call beginUse() because array_t sizes
            // will hold a read lock long enough until starts holds another
            // one for the life time of this object
            array_t<uint32_t> sizes(bdlstore, 0, ncol+2);
            uint32_t expected = sizeof(uint32_t)*(3+sizes[0]+sizes[1]);
            for (uint32_t i = 2; i < 2+ncol; ++i)
                expected += sizes[i] * sizes[0];
            if (ncol == sizes[1] && expected == bdlstore->bytes()) {
                // go through every selected column to construct the colValues
                uint32_t start = sizeof(uint32_t)*(ncol+2);
                for (uint32_t i=0; i < ncol; ++i) {
                    if (comps.getAggregator(i) == ibis::selectClause::CNT)
                        continue;

                    const ibis::column* cptr = tbl->getColumn(comps.aggName(i));
                    if (cptr != 0) {
                        LOGGER(ibis::gVerbose > 4)
                            << "bundles::ctor to recreate a colValues for \""
                            << *(comps.aggExpr(i)) << "\" as cols["
                            << cols.size() << ']';
                        ibis::colValues* tmp;
                        switch (comps.getAggregator(i)) {
                        case ibis::selectClause::AVG:
                        case ibis::selectClause::SUM:
                        case ibis::selectClause::VARPOP:
                        case ibis::selectClause::VARSAMP:
                        case ibis::selectClause::STDPOP:
                        case ibis::selectClause::STDSAMP:
                            tmp = new ibis::colDoubles
                                (cptr, bdlstore, start,
                                 start+8*sizes[0]);
                            break;
                        // case ibis::selectClause::CONCAT:
                        //     tmp = new ibis::colStrings
                        //      (cptr, bdlstore, start,
                        //       start+8*sizes[0]);
                        //     break;
                        default:
                            tmp = ibis::colValues::create
                                (cptr, bdlstore, start,
                                 start+sizes[0]*cptr->elementSize());
                            break;
                        }
                        cols.push_back(tmp);
                        start += sizes[2+i] * sizes[0];
                        aggr.push_back(comps.getAggregator(i));
                    }
                    else {
                        LOGGER(ibis::gVerbose >= 0)
                            << "Warning -- bundles::ctor \"" << comps.aggName(i)
                            << "\" is not the name of a column in table "
                            << tbl->name() << IBIS_FILE_LINE;
                        throw ibis::bad_alloc("unknown column name"
                                              IBIS_FILE_LINE);
                    }
                }
                starts = new
                    ibis::array_t<uint32_t>(bdlstore, start, sizes[0]+1);
                infile = true;
            }
            else {
                LOGGER(ibis::gVerbose > 0)
                    << "Warning -- bundles::ctor -- according to the header, "
                    << expected << " bytes are expected, but the file "
                    << bdlfile << " contains " << bdlstore->bytes();
            }
        }

        if (starts == 0) {
            // use the current hit vector of the query to generate the bundle
            const ibis::bitvector* hits = q.getHitVector();
            if (hits != 0) {
                if (rids == 0) {
                    rids = tbl->getRIDs(*hits);
                    if (rids != 0 && rids->size() != hits->cnt()) {
                        delete rids;
                        rids = 0;
                    }
                }
            }
            else {
                LOGGER(ibis::gVerbose >= 0)
                    << "Warning -- bundles::ctor -- query " << q.id()
                    << " contains an invalid hit vector, call the function "
                    "evaluate to generate a valid hit vector" IBIS_FILE_LINE;
                throw ibis::bad_alloc("bundles::ctor -- no hit vector");
            }
            for (uint32_t i=0; i < ncol; ++i) {
                if (comps.getAggregator(i) == ibis::selectClause::CNT) {
                    continue;
                }

                const ibis::column* cptr = tbl->getColumn(comps.aggName(i));
                if (cptr != 0) {
                    ibis::colValues* tmp;
                    LOGGER(ibis::gVerbose > 4)
                        << "bundles::ctor to create a colValues for \""
                        << *(comps.aggExpr(i)) << "\" as cols[" << cols.size()
                        << ']';
                    switch (comps.getAggregator(i)) {
                    case ibis::selectClause::AVG:
                    case ibis::selectClause::SUM:
                    case ibis::selectClause::VARPOP:
                    case ibis::selectClause::VARSAMP:
                    case ibis::selectClause::STDPOP:
                    case ibis::selectClause::STDSAMP:
                        tmp = new ibis::colDoubles(cptr, *hits);
                        break;
                    case ibis::selectClause::CONCAT:
                        tmp = new ibis::colStrings(cptr, *hits);
                        break;
                    default:
                        tmp = ibis::colValues::create(cptr, *hits);
                        break;
                    }
                    cols.push_back(tmp);
                    aggr.push_back(comps.getAggregator(i));
                }
                else {
                    LOGGER(ibis::gVerbose >= 0)
                        << "Warning -- bundles::ctor \"" << comps.aggName(i)
                        << "\" is not the name of a column in table "
                        << tbl->name() << IBIS_FILE_LINE;
                    throw ibis::bad_alloc("unknown column name" IBIS_FILE_LINE);
                }
            }

            if (cols.size() > 0)
                sort(dir);
        }

        if (ibis::gVerbose > 5) {
            ibis::util::logger lg;
            lg() << "query[" << q.id()
                 << "]::bundles -- generated the bundle\n";
            if (rids == 0) {
                print(lg());
            }
            else if (ibis::gVerbose > 8) {
                printAll(lg());
            }
            else {
                print(lg());
            }
        }
    }
    catch (...) {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- bundles::ctor received an exception, "
            "start cleaning up" IBIS_FILE_LINE;
        clear();
        throw; // rethrow the exception
    }
} // ibis::bundles::bundles

/// Constructor.  It creates a bundle using the hits provided by the caller
/// (instead of from the query object q).
ibis::bundles::bundles(const ibis::query& q, const ibis::bitvector& hits,
                       int dir) : bundle(q, hits) {
    if (hits.cnt() == 0)
        return;

    try {
        // need to retrieve the named columns
        const ibis::part* tbl = q.partition();
        const uint32_t ncol = comps.aggSize();
        for (uint32_t i=0; i < ncol; ++i) {
            if (comps.getAggregator(i) != ibis::selectClause::CNT) {
                continue;
            }

            const ibis::column* cptr = tbl->getColumn(comps.aggName(i));
            if (cptr != 0) {
                LOGGER(ibis::gVerbose > 4)
                    << "bundles::ctor to create a colValues for \""
                    << *(comps.aggExpr(i)) << "\" as cols[" << cols.size()
                    << ']';
                ibis::colValues* tmp;
                switch (comps.getAggregator(i)) {
                case ibis::selectClause::AVG:
                case ibis::selectClause::SUM:
                case ibis::selectClause::VARPOP:
                case ibis::selectClause::VARSAMP:
                case ibis::selectClause::STDPOP:
                case ibis::selectClause::STDSAMP:
                    tmp = new ibis::colDoubles(cptr, hits);
                    break;
                case ibis::selectClause::CONCAT:
                    tmp = new ibis::colStrings(cptr, hits);
                    break;
                default:
                    tmp = ibis::colValues::create(cptr, hits);
                    break;
                }
                cols.push_back(tmp);
                aggr.push_back(comps.getAggregator(i));
            }
            else {
                LOGGER(ibis::gVerbose >= 0)
                    << "Warning -- bundles::ctr \"" << comps.aggExpr(i)
                    << "\" is not the name of a column in table "
                    << tbl->name() << IBIS_FILE_LINE;
                throw ibis::bad_alloc("unknown column name" IBIS_FILE_LINE);
            }
        }
        if (rids == 0) {
            rids = tbl->getRIDs(hits);
            if (rids != 0 && rids->size() != hits.cnt()) {
                delete rids;
                rids = 0;
            }
        }
        if (cols.size() > 0)
            sort(dir);

        if (ibis::gVerbose > 5) {
            ibis::util::logger lg;
            lg() << "query[" << q.id()
                 << "]::bundle1 -- generated the bundle\n";
            if (rids == 0) {
                    print(lg());
            }
            else if (ibis::gVerbose > 8) {
                printAll(lg());
            }
            else {
                print(lg());
            }
        }
    }
    catch (...) {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- bundles::ctor received an exception, "
            "start cleaning up" IBIS_FILE_LINE;
        clear();
        throw; // rethrow the exception
    }
} // ibis::bundles::bundles

/// Constructor.  It creates a bundle from all rows of tbl.
ibis::bundles::bundles(const ibis::part& tbl, const ibis::selectClause& cmps,
                       int dir) : bundle(cmps) {
    id = tbl.name();
    try {
        ibis::bitvector msk;
        tbl.getNullMask(msk);
        for (unsigned ic = 0; ic < comps.aggSize(); ++ ic) {
            const ibis::math::term& expr = *comps.aggExpr(ic);
            const char* cn = comps.aggName(ic);
            if (comps.getAggregator(ic) == ibis::selectClause::CNT) {
                continue;
            }

            ibis::column* c = tbl.getColumn(cn);
            if (expr.termType() == ibis::math::VARIABLE &&
                (c == 0 || 0 != stricmp(cn, c->name()))) {
                c = tbl.getColumn(static_cast<const ibis::math::variable&>
                                  (expr).variableName());
            }
            if (c == 0) {
                clear();
                LOGGER(ibis::gVerbose >= 0)
                    << "Warning -- bundles(" << tbl.name() << ", "
                    << comps << ") can not find a column named "
                    << (cn ? cn : "");
                throw "bundle1::ctor can not find a column name"
                    IBIS_FILE_LINE;
            }

            LOGGER(ibis::gVerbose > 6)
                << "bundles::ctor is to start a colValues for \""
                << *(comps.aggExpr(ic)) << "\" as cols[" << cols.size()
                << "] with data from " << c->fullname();
            ibis::colValues* cv = 0;
            switch (comps.getAggregator(ic)) {
            case ibis::selectClause::AVG:
            case ibis::selectClause::SUM:
            case ibis::selectClause::VARPOP:
            case ibis::selectClause::VARSAMP:
            case ibis::selectClause::STDPOP:
            case ibis::selectClause::STDSAMP:
                cv = new ibis::colDoubles(c, msk);
                break;
            case ibis::selectClause::CONCAT:
                cv = new ibis::colStrings(c, msk);
                break;
            default:
                cv = ibis::colValues::create(c, msk);
                break;
            }
            if (cv != 0) {
                LOGGER(ibis::gVerbose > 2)
                    << "bundles::ctor created a colValues for \""
                    << *(comps.aggExpr(ic)) << "\" as cols[" << cols.size()
                    << "] with size " << cv->size();
                cols.push_back(cv);
                aggr.push_back(comps.getAggregator(ic));
            }
            else {
                LOGGER(ibis::gVerbose > 0)
                    << "Warning -- bundles(" << tbl.name() << ", " << comps
                    << ") failed to create an in-memory column for \""
                    << *(comps.aggExpr(ic)) << '"';
            }
        }

        if (cols.size() > 0)
            sort(dir);

        if (ibis::gVerbose > 5) {
            ibis::util::logger lg;
            lg() << "bundles -- generated the bundle for \"" << *comps
                 << "\"\n";
            print(lg());
        }
    }
    catch (...) {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- bundles::ctor received an exception, "
            "start cleaning up" IBIS_FILE_LINE;
        clear();
        throw; // rethrow the exception
    }
} // ibis::bundles::bundles

/// Print out the bundles without RIDs.
void ibis::bundles::print(std::ostream& out) const {
    // caller must hold an ioLock ibis::util::ioLock lock;
    const uint32_t ncol = cols.size();
    if (ncol == 0) return; // nothing to print

    const uint32_t total = (cols[0] != 0 ? cols[0]->size() : 0);
    const uint32_t nprt = ((total>>ibis::gVerbose) > 1 ?
                           (1U << ibis::gVerbose) : total);
    bool distinct = true;
    for (uint32_t i = 0; i < ncol && distinct; ++ i) {
        if (cols[i] == 0) {
            LOGGER(ibis::gVerbose >= 0)
                << "Warning -- bundles::print can not proceed because cols["
                << i << "] is nil";
            return;
        }
        distinct = cols[i]->canSort();
    }
    if (ibis::gVerbose > 4)
        out << "Bundle " << id << " contains " << total
            << (distinct ? " distinct " : " ") << ncol << "-tuple"
            << (total > 1 ? "s" : "") << std::endl;
    if (starts != 0 && ibis::gVerbose > 4) {
        if (ibis::gVerbose > 4) {
            for (uint32_t i = 0; i < ncol; ++ i) {
                if (i > 0) out << ", ";
                out << (*(cols[i]))->name();
            }
            out << " (with counts)\n";
        }
        for (uint32_t i=0; i<nprt; ++i) {
            for (uint32_t ii=0; ii<ncol; ++ii) {
                cols[ii]->write(out, i);
                out << ", ";
            }
            out << "\t" << (*starts)[i+1]-(*starts)[i] << "\n";
        }
    }
    else {
        if (ibis::gVerbose > 4)
            out << *comps << "\n";
        for (uint32_t i=0; i<nprt; ++i) {
            for (uint32_t ii=0; ii<ncol; ++ii) {
                cols[ii]->write(out, i);
                out << (ii+1<ncol ? ", " : "\n");
            }
        }
    }
    if (nprt < total)
        out << "\t...\t" << total-nprt << " skipped\n";
} // ibis::bundles::print

/// Print out the bundles with RIDs.
void ibis::bundles::printAll(std::ostream& out) const {
    const uint32_t ncol = cols.size();
    if (ncol == 0) return;

    if (rids == 0 || starts == 0) {
        print(out);
        return;
    }

    bool distinct = true;
    for (uint32_t i = 0; i < ncol && distinct; ++ i) {
        if (cols[i] == 0) {
            LOGGER(ibis::gVerbose >= 0)
                << "Warning -- bundles::printAll can not proceed because cols["
                << i << "] is nil";
            return;
        }
        distinct = cols[i]->canSort();
    }
    const uint32_t size = cols[0]->size();
    //ibis::util::ioLock lock;
    if (ibis::gVerbose > 2)
        out << "Bundle " << id << " contains " << size
            << (distinct ? " distinct " : " ") << ncol << "-tuple"
            << (size > 1 ? "s" : "") << " from "
            << rids->size() << (rids->size()>1 ? " rows" : " row")
            << std::endl;
    out << *comps << "\n";
    for (uint32_t i=0; i<size; ++i) {
        for (uint32_t ii=0; ii<ncol; ++ii) {
            cols[ii]->write(out, i);
            out << ", ";
        }
        out << ",\t";
        for (uint32_t j=(*starts)[i]; j < (*starts)[i+1]; ++j) {
            out << (*rids)[j] << (j+1<(*starts)[i+1] ? ", " : "\n");
        }
    }
} // ibis::bundles::printAll

void ibis::bundles::printColumnNames(std::ostream& out) const {
    if (cols.size() > 0) {
        out << cols[0]->name();
        for (unsigned j = 1; j < cols.size(); ++ j)
            out << ", " << cols[j]->name();
    }
} // ibis::bundles::printColumnNames

/// Sort the columns.  Remove the duplicate elements and generate the
/// starts.  This function allows aggregation functions to appear in
/// arbitrary positions in the select clause.
/// @param dir:
/// - > 0 sort RIDs,
/// - < 0 do not sort RIDs, leave them in whatever order after sorting the
///       order-by keys,
/// - = 0 if FASTBIT_ORDER_OUTPUT_RIDS is defined, sort RIDs, otherwise
///       don't sort.
void ibis::bundles::sort(int dir) {
    const uint32_t ncol = cols.size();
    if (ncol == 0) return;

    uint32_t nGroups = 0xFFFFFFFFU; // temporarily store the number of rows
    for (uint32_t i = 0; i < ncol; ++ i) {
        if (cols[i] != 0) {
            if (cols[i]->size() < nGroups)
                nGroups = cols[i]->size();
        }
        else {
            nGroups = 0;
        }
    }
    const uint32_t nplain = comps.numGroupbyKeys();
    const uint32_t nHits = nGroups;
    for (uint32_t i = 0; i < ncol; ++ i) {
        if (cols[i] != 0) {
            const uint32_t sz = cols[i]->size();
            if (sz > nHits)
                cols[i]->erase(nHits, sz);
        }
    }
    if (ibis::gVerbose > 5) {
        ibis::util::logger lg;
        lg() << "bundles[" << id << "]::sort starting with "
             << ncol << " columns and " << nHits << " row"
             << (nHits > 1 ? "s" : "");
#if _DEBUG+0 > 2 || DEBUG+0 > 1
        for (uint32_t j = 0; j < nHits; ++ j) {
            lg() << "\n";
            for (uint32_t i = 0; i < ncol; ++ i) {
                if (i > 0) lg() << ", ";
                cols[i]->write(lg(), j);
            }
        }
#endif
    }
    if (nHits < 2) { // no need to sort, but may do (silly) aggregations
        starts = new ibis::array_t<uint32_t>(2);
        (*starts)[1] = nHits;
        (*starts)[0] = 0;
        if (nplain < ncol && nHits > 0) {
            for (uint32_t i = 0; i < ncol; ++ i) {
                if (aggr[i] != ibis::selectClause::NIL_AGGR) {
                    cols[i]->nosharing();
                    cols[i]->reduce(*starts, aggr[i]);
                }
            }
        }
    }
    else if (nplain == ncol) { // no functions
        for (uint32_t i = 0; i < ncol; ++ i)
            cols[i]->nosharing();
        // sort according to the values of the first column
        cols[0]->sort(0, nHits, this, cols.begin()+1, cols.end());
        starts = cols[0]->segment();
        if (starts == 0) {
            LOGGER(ibis::gVerbose >= 0)
                << "Warning -- bundles::sort failed to sort and segment "
                "the values of " << cols[0]->name() << " ("
                << ibis::TYPESTRING[static_cast<int>(cols[0]->getType())]
                << ")";
            return;
        }

        nGroups = starts->size() - 1;
        // go through the rest of the columns if necessary
        for (uint32_t i=1; i<ncol && nGroups<nHits; ++i) {
            uint32_t i1 = i + 1;
            for (uint32_t i2=0; i2<nGroups; ++i2) { // sort one group at a time
                cols[i]->sort((*starts)[i2], (*starts)[i2+1], this,
                              cols.begin()+i1, cols.end());
            }
            array_t<uint32_t>* tmp = cols[i]->segment(starts);
            if (tmp == 0) {
                LOGGER(ibis::gVerbose >= 0)
                    << "Warning -- bundles::sort failed to sort and segment "
                    "the values of " << cols[i]->name() << " ("
                    << ibis::TYPESTRING[static_cast<int>(cols[i]->getType())]
                    << ")";
                return;
            }
            delete starts;
            starts = tmp;
            nGroups = starts->size() - 1;
        }

        if (nGroups < nHits) {// erase the dupliate elements
            for (uint32_t i2=0; i2<ncol; ++i2)
                cols[i2]->reduce(*starts);
        }
    }
    else if (nplain == 0) { // no column to sort
        for (uint32_t i = 0; i < ncol; ++ i)
            cols[i]->nosharing();
        delete starts;
        starts = new array_t<uint32_t>(2);
        (*starts)[0] = 0;
        (*starts)[1] = nHits;
        nGroups = 1;
        for (uint32_t i = 0; i < ncol; ++ i) {
            cols[i]->reduce(*starts, aggr[i]);
        }
    }
    else { // one or more columns to sort
        for (uint32_t i = 0; i < ncol; ++ i)
            cols[i]->nosharing();

        ibis::colList cols2(ncol);
        std::vector<ibis::selectClause::AGREGADO> ops(ncol);
        // move aggregation functions to the end of the list
        for (uint32_t i1 = 0, iplain = 0, iaggr = nplain;
             i1 < ncol; ++ i1) {
            if (aggr[i1] == ibis::selectClause::NIL_AGGR) {
                cols2[iplain] = cols[i1];
                ops[iplain] = ibis::selectClause::NIL_AGGR;
                ++ iplain;
            }
            else {
                cols2[iaggr] = cols[i1];
                ops[iaggr] = aggr[i1];
                ++ iaggr;
            }
        }
        cols2.swap(cols);

        // sort according to the values of the first column
        cols[0]->sort(0, nHits, this, cols.begin()+1, cols.end());
        starts = cols[0]->segment();
        if (starts == 0) {
            LOGGER(ibis::gVerbose >= 0)
                << "Warning -- bundles::sort failed to sort and segment "
                "the values of " << cols[0]->name() << " ("
                << ibis::TYPESTRING[static_cast<int>(cols[0]->getType())]
                << ")";
            return;
        }
        nGroups = starts->size() - 1;

        // go through the rest of the columns if necessary
        for (uint32_t i=1; i<nplain && nGroups<nHits; ++i) {
            uint32_t i1 = i + 1;
            for (uint32_t i2=0; i2<nGroups; ++i2) { // sort one group at a time
                cols[i]->sort((*starts)[i2], (*starts)[i2+1], this,
                              cols.begin()+i1, cols.end());
            }
            array_t<uint32_t>* tmp = cols[i]->segment(starts);
            if (tmp == 0) {
                LOGGER(ibis::gVerbose >= 0)
                    << "Warning -- bundles::sort failed to sort and segment "
                    "the values of " << cols[i]->name() << " ("
                    << ibis::TYPESTRING[static_cast<int>(cols[i]->getType())]
                    << ")";
                return;
            }
            delete starts;
            starts = tmp;
            nGroups = starts->size() - 1;
        }

        if (nGroups < nHits) {// erase the dupliate elements
            for (uint32_t i2 = 0; i2 < nplain; ++ i2)
                cols[i2]->reduce(*starts);
        }
        for (uint32_t i2 = nplain; i2 < ncol; ++ i2)
            cols[i2]->reduce(*starts, ops[i2]);

        // restore the input order of the columns
        cols2.swap(cols);
    }

    if (dir == 0) {
#ifdef FASTBIT_ORDER_OUTPUT_RIDS
        dir = 1;
#else
        dir = -1;
#endif
    }
    // sort RIDs
    if (dir > 0 && nGroups < nHits && rids != 0 && rids->size() == nHits) {
        for (uint32_t i1=nGroups; i1>0; --i1)
            sortRIDs((*starts)[i1-1], (*starts)[i1]);
    }
    if (ibis::gVerbose > 0) { // perform a sanity check
        for (uint32_t i1 = 0; i1 < ncol; ++i1) {
            LOGGER(cols[i1]->size() != nGroups)
                << "Warning -- bundles::sort -- column # " << i1
                << " (" << (*(cols[i1]))->name()
                << ") is expected to have " << nGroups << " value"
                << (nGroups>1?"s":"") << ", but it actually has "
                << cols[i1]->size();
        }
    }
#if _DEBUG+0>2 || DEBUG+0>1
    if (ibis::gVerbose > 5) {
        ibis::util::logger lg;
        lg() << "DEBUG -- bundles[" << id << "]::sort ending "
             << ncol << " columns and " << nGroups << " row"
             << (nGroups > 1 ? "s" : "");
        for (uint32_t j = 0; j < nGroups; ++ j) {
            lg() << "\n";
            for (uint32_t i = 0; i < ncol; ++ i) {
                if (i > 0) lg() << ", ";
                cols[i]->write(lg(), j);
            }
        }
    }
#endif
} // ibis::bundles::sort

/// Reorder the bundles according to the keys (names) given.  If the
/// argument direction is a negative number, the rows are reversed after
/// sorting.  Even if no sorting is done, the reversal of rows is still
/// performed.
void ibis::bundles::reorder(const char *names) {
    if (names == 0 || *names == 0) return;
    if (starts == 0 || cols.size() == 0) return;
    if (starts->size() <= 2) return; // one group, no need to sort

    ibis::nameList sortkeys; // the new keys for sorting
    sortkeys.select(names); // preserve the order of the sort keys

    bool nosort = true;
    for (unsigned j = 0; nosort && j < sortkeys.size() && j < cols.size(); ++ j)
        nosort = (stricmp(sortkeys[j], comps.aggName(j)) == 0);
    if (nosort) { // no need to sort
        // if (direction < 0)
        //     reverse();
        return;
    }
    // make sure all columns are ready for modification
    for (uint32_t i = 0; i < cols.size(); ++ i)
        cols[i]->nosharing();

    // verify the variable names.  Note that for functions, it only looks
    // at the attribute names not the actual funtion to match with the
    // select clause, this is not a complete verification.
    const uint32_t ngroups = starts->size() - 1;
    if (rids != 0) {
        // turn a single list of RIDs into a number of smaller lists so
        // that the smaller lists can be re-ordered along with the other
        // values
        array_t< ibis::RIDSet* > rid2;
        rid2.reserve(ngroups);
        for (uint32_t i = 0; i < ngroups; ++ i)
            rid2.push_back(new array_t<ibis::rid_t>
                           (*rids, (*starts)[i], (*starts)[i+1]));

        if (sortkeys.size() > 1) { // multiple keys
            array_t<uint32_t> gb;
            gb.reserve(ngroups);
            gb.push_back(0);
            gb.push_back(ngroups);
            for (uint32_t i = 0;
                 i < sortkeys.size() && gb.size() <= ngroups;
                 ++ i) {
                const uint32_t j = comps.find(sortkeys[i]);
                if (j >= comps.aggSize()) continue;

                array_t<uint32_t> ind0; // indices over all ngroups
                ind0.reserve(ngroups);
                for (uint32_t g = 0; g < gb.size()-1; ++ g) {
                    if (gb[g+1] > gb[g]+1) { // more than one group
                        array_t<uint32_t> ind1; // indices for group g
                        cols[j]->sort(gb[g], gb[g+1], ind1);
                        ind0.insert(ind0.end(), ind1.begin(), ind1.end());
                    }
                    else { // a single group
                        ind0.push_back(gb[g]);
                    }
                }
                for (uint32_t k = 0; k < cols.size(); ++ k)
                    cols[k]->reorder(ind0);
                ibis::util::reorder(rid2, ind0);

                {
                    array_t<uint32_t> *tmp = cols[j]->segment(&gb);
                    if (tmp == 0) {
                        LOGGER(ibis::gVerbose >= 0)
                            << "Warning -- bundles::reorder failed to segment "
                            "the values of " << cols[j]->name() << " ("
                            << ibis::TYPESTRING[static_cast<int>(cols[j]->getType())]
                            << ")";
                        return;
                    }
                    gb.swap(*tmp);
                    delete tmp;
                }
            }

            // if (direction < 0) { // reverse the order
            //  for (uint32_t j = 0; j < cols.size(); ++ j)
            //      for (uint32_t i = 0; i < ngroups/2; ++ i)
            //          cols[j]->swap(i, ngroups-1-i);
            //  for (uint32_t i = 0; i < ngroups/2; ++ i) {
            //      const uint32_t j = ngroups - 1 - i;
            //      ibis::RIDSet *tmp = rid2[i];
            //      rid2[i] = rid2[j];
            //      rid2[j] = tmp;
            //  }
            // }
        }
        else { // a single key
            const uint32_t j = comps.find(sortkeys[0]);
            if (j < comps.aggSize()) {
                array_t<uint32_t> ind;
                cols[j]->sort(0, ngroups, ind);
                // if (direction < 0) { // reverse the order of ind
                //     for (uint32_t i = 0; i < ngroups/2; ++ i) {
                //      const uint32_t itmp = ind[i];
                //      ind[i] = ind[ngroups-1-i];
                //      ind[ngroups-1-i] = itmp;
                //     }
                // }
                for (uint32_t i = 0; i < cols.size(); ++ i)
                    cols[i]->reorder(ind);
                ibis::util::reorder(rid2, ind);
            }
        }

        // time to put the smaller lists together again, and update starts
        ibis::RIDSet rid1;
        rid1.reserve(rids->size());
        for (uint32_t i = 0; i < ngroups; ++ i) {
            rid1.insert(rid1.end(), rid2[i]->begin(), rid2[i]->end());
            (*starts)[i+1] = (*starts)[i] + rid2[i]->size();
            delete rid2[i];
        }
        rids->swap(rid1);
    }
    else { // no rids
        // turn starts into counts
        for (uint32_t i = 0; i < ngroups; ++ i)
            (*starts)[i] = (*starts)[i+1] - (*starts)[i];
        starts->resize(ngroups);
        if (sortkeys.size() > 1) {
            ibis::array_t<uint32_t> gb;
            gb.reserve(ngroups);
            gb.push_back(0);
            gb.push_back(ngroups);
            for (uint32_t i = 0;
                 i < sortkeys.size() && gb.size() <= ngroups;
                 ++ i) {
                const uint32_t j = comps.find(sortkeys[i]);
                if (j >= comps.aggSize()) continue;

                ibis::array_t<uint32_t> ind0; // indices over all ngroups
                ind0.reserve(ngroups);
                for (uint32_t g = 0; g < gb.size()-1; ++ g) {
                    if (gb[g+1] > gb[g]+1) { // more than one group
                        ibis::array_t<uint32_t> ind1; // indices for group g
                        cols[j]->sort(gb[g], gb[g+1], ind1);
                        ind0.insert(ind0.end(), ind1.begin(), ind1.end());
                    }
                    else { // a single group
                        ind0.push_back(gb[g]);
                    }
                }
                for (uint32_t k = 0; k < cols.size(); ++ k)
                    cols[k]->reorder(ind0);
                ibis::util::reorder(*(starts), ind0);

                {
                    ibis::array_t<uint32_t> *tmp = cols[j]->segment(&gb);
                    if (tmp == 0) {
                        LOGGER(ibis::gVerbose >= 0)
                            << "Warning -- bundles::sort failed to segment "
                            "the values of " << cols[j]->name() << " ("
                            << ibis::TYPESTRING[static_cast<int>(cols[j]->getType())]
                            << ")";
                        return;
                    }
                    gb.swap(*tmp);
                    delete tmp;
                }
            }

            // if (direction < 0) { // reverse the order
            //  for (uint32_t j = 0; j < cols.size(); ++ j)
            //      for (uint32_t i = 0; i < ngroups/2; ++ i)
            //          cols[j]->swap(i, ngroups-1-i);
            //  for (uint32_t i = 0; i < ngroups/2; ++ i) {
            //      const uint32_t j = ngroups - 1 - i;
            //      const uint32_t tmp = (*starts)[i];
            //      (*starts)[i] = (*starts)[j];
            //      (*starts)[j] = tmp;
            //  }
            // }
        }
        else {
            const uint32_t j = comps.find(sortkeys[0]);
            if (j < comps.aggSize()) {
                ibis::array_t<uint32_t> ind;
                cols[j]->sort(0, ngroups, ind);
                // if (direction < 0) { // reverse the order of ind
                //     for (uint32_t i = 0; i < ngroups/2; ++ i) {
                //      const uint32_t itmp = ind[i];
                //      ind[i] = ind[ngroups-1-i];
                //      ind[ngroups-1-i] = itmp;
                //     }
                // }
                for (uint32_t i = 0; i < cols.size(); ++ i)
                    cols[i]->reorder(ind);
                ibis::util::reorder(*(starts), ind);
            }
        }

        /// turn counts back into starting positions (starts)
        uint32_t cumu = 0;
        for (uint32_t i = 0; i < ngroups; ++ i) {
            uint32_t tmp = (*starts)[i];
            (*starts)[i] = cumu;
            cumu += tmp;
        }
        starts->push_back(cumu);
    }
    // new content, definitely not in file
    infile = false;
} // ibis::bundles::reorder

// Change from ascending order to descending order.  Most lines of the code
// deals with the re-ordering of the RIDs.
void ibis::bundles::reverse() {
    if (starts == 0 || cols.empty()) return;
    if (starts->size() <= 2) return;
    const uint32_t ngroups = starts->size() - 1;

    // make sure all columns are ready for modification
    for (uint32_t i = 0; i < cols.size(); ++ i)
        cols[i]->nosharing();
    if (rids != 0) { // has a rid list
        array_t<uint32_t> cnts(ngroups);
        for (uint32_t i = 0; i < ngroups; ++ i)
            cnts[i] = (*starts)[i+1] - (*starts)[i];
        for (uint32_t i = 0; i+i < ngroups; ++ i) {
            const uint32_t j = ngroups - i - 1;
            {
                uint32_t tmp;
                tmp = (*starts)[i];
                (*starts)[i] = (*starts)[j];
                (*starts)[j] = tmp;
            }
            {
                uint32_t tmp;
                tmp = cnts[i];
                cnts[i] = cnts[j];
                cnts[j] = tmp;
            }
            for (ibis::colList::iterator it = cols.begin();
                 it != cols.end(); ++ it)
                (*it)->swap(i, j);
        }

        ibis::RIDSet tmpids;
        tmpids.reserve(rids->size());
        for (uint32_t i = 0; i < ngroups; ++ i) {
            for (uint32_t j = 0; j < cnts[i]; ++ j)
                tmpids.push_back((*rids)[(*starts)[i]+j]);
        }
        rids->swap(tmpids);
        (*starts)[0] = 0;
        for (uint32_t i = 0; i < ngroups; ++ i)
            (*starts)[i+1] = (*starts)[i] + cnts[i];
    }
    else {
        for (ibis::colList::iterator it = cols.begin();
             it != cols.end(); ++ it)
            for (uint32_t i = 0; i < ngroups/2; ++ i)
                (*it)->swap(i, ngroups-1-i);
        // turn starts into counts
        for (uint32_t i = 0; i < ngroups; ++ i)
            (*starts)[i] = (*starts)[i+1] - (*starts)[i];
        // swap counts
        for (uint32_t i = 0; i < ngroups/2; ++ i) {
            const uint32_t j = ngroups - 1 - i;
            const uint32_t tmp = (*starts)[i];
            (*starts)[i] = (*starts)[j];
            (*starts)[j] = tmp;
        }
        // turn counts back into starts
        uint32_t cumu = 0;
        for (uint32_t i = 0; i < ngroups; ++ i) {
            const uint32_t tmp = (*starts)[i];
            (*starts)[i] = cumu;
            cumu += tmp;
        }
        LOGGER(cumu != (*starts)[ngroups] && ibis::gVerbose >= 0)
            << "Warning -- bundles::reverse internal error, cumu ("
            << cumu << ") and (*starts)[" << ngroups << "] ("
            << (*starts)[ngroups] << ") are expected to be equal but are not";
    }
} // ibis::bundles::reverse

/// This single-arugment version of the function truncate keeps the first
/// few rows.
long ibis::bundles::truncate(uint32_t keep) {
    if (starts == 0 || cols.empty()) return -2L;
    if (starts->size() <= 2) return -3L;
    const uint32_t ngroups = starts->size() - 1;
    if (ngroups <= keep)
        return ngroups;

    if (rids != 0)
        rids->resize((*starts)[keep]);
    starts->resize(keep+1);
    for (uint32_t i = 0; i < cols.size(); ++ i)
        cols[i]->truncate(keep);
    infile = false;
    return keep;
} // ibis::bundles::truncate

/// This two-argument version of the function keeps a few rows after a
/// specified starting point.
long ibis::bundles::truncate(uint32_t keep, uint32_t start) {
    if (cols.empty() || starts == 0) return -2L;
    if (starts->size() <= 2) return -3L;
    const uint32_t ngroups = starts->size()-1;
    if (start >= ngroups || keep == 0) {
        starts->clear();
        for (uint32_t i = 0; i < cols.size(); ++ i)
            cols[i]->truncate(0);
        return 0;
    }
    else if (keep >= ngroups && start == 0) {
        return ngroups;
    }

    const uint32_t end = (keep+start < ngroups ? keep+start : ngroups);
    keep = end - start;
    if (rids != 0) {
        rids->truncate((*starts)[end]-(*starts)[start], (*starts)[start]);
    }
    infile = false;
    starts->truncate(keep+1, start);
    if (start != 0) {
        const uint32_t offset = starts->front();
        for (array_t<uint32_t>::iterator it = starts->begin();
             it != starts->end(); ++ it)
            *it -= offset;
    }
    for (uint32_t i = 0; i < cols.size(); ++ i)
        cols[i]->truncate(keep, start);
    return keep;
} // ibis::bundles::truncate

/// Reorder the bundles according to the keys (names) given.  Keep only the
/// first @c keep elements.  If @c direction < 0, keep the largest ones,
/// otherwise keep the smallest ones.
long ibis::bundles::truncate(const char *names, uint32_t keep) {
    if (names == 0 || *names == 0) return -1L;
    if (starts == 0 || cols.empty()) return -2L;
    if (starts->size() <= 2) return -3L;
    if (keep == 0) return -4L;

    ibis::nameList sortkeys; // the new keys for sorting
    sortkeys.select(names); // preserve the order of the sort keys
    if (sortkeys.size() == 0) {
        // if (direction < 0)
        //     reverse();
        return size();
    }

    // make sure all columns are ready for modification
    for (uint32_t i = 0; i < cols.size(); ++ i)
        cols[i]->nosharing();
    // Note that for functions, it only looks at the attribute names not
    // the actual funtion to match with the select clause, this is not a
    // complete verification.
    uint32_t ngroups = starts->size() - 1;
    if (rids != 0) {
        // turn a single list of RIDs into a number of smaller lists so
        // that the smaller lists can be re-ordered along with the other
        // values
        array_t< ibis::RIDSet* > rid2;
        rid2.reserve(ngroups);
        for (uint32_t i = 0; i < ngroups; ++ i)
            rid2.push_back(new array_t<ibis::rid_t>
                           (*rids, (*starts)[i], (*starts)[i+1]));

        if (sortkeys.size() > 1) {
            array_t<uint32_t> gb;
            uint32_t i = 0;
            uint32_t j = comps.find(sortkeys[0]);
            while (j >= comps.aggSize() && i < sortkeys.size()) {
                ++ i;
                j = comps.find(sortkeys[i]);
            }
            if (i >= sortkeys.size())
                return truncate(keep);

            array_t<uint32_t> ind0; // indices over all ngroups
            ind0.reserve(keep);
            // deal with the first sort key
            // if (direction >= 0)
                cols[j]->bottomk(keep, ind0);
            // else
            //  cols[j]->topk(keep, ind0);
            for (uint32_t ii = 0; ii < cols.size(); ++ ii)
                cols[ii]->reorder(ind0);
            ibis::util::reorder(rid2, ind0);
            ngroups = ind0.size();
            { // segment cols[j]
                array_t<uint32_t> *tmp = cols[j]->segment(&gb);
                if (tmp == 0) {
                    LOGGER(ibis::gVerbose >= 0)
                        << "Warning -- bundles::truncate failed to segment "
                        "the values of " << cols[j]->name() << " ("
                        << ibis::TYPESTRING[static_cast<int>(cols[j]->getType())]
                        << ")";
                    return -1;
                }
                gb.swap(*tmp);
                delete tmp;
            }

            for (++ i; // starting with 
                 i < sortkeys.size() && gb.size() <= ngroups;
                 ++ i) {
                j = comps.find(sortkeys[i]);
                if (j >= comps.aggSize()) continue;

                for (uint32_t g = 0; g < gb.size()-1; ++ g) {
                    if (gb[g+1] > gb[g]+1) { // more than one group
                        array_t<uint32_t> ind1; // indices for group g
                        cols[j]->sort(gb[g], gb[g+1], ind1);
                        ind0.insert(ind0.end(), ind1.begin(), ind1.end());
                    }
                    else { // a single group
                        ind0.push_back(gb[i]);
                    }
                }
                for (uint32_t k = 0; k < cols.size(); ++ k)
                    cols[k]->reorder(ind0);
                ibis::util::reorder(rid2, ind0);

                {
                    array_t<uint32_t> *tmp = cols[j]->segment(&gb);
                    if (tmp == 0) {
                        LOGGER(ibis::gVerbose >= 0)
                            << "Warning -- bundles::truncate failed to segment "
                            "the values of " << cols[j]->name() << " ("
                            << ibis::TYPESTRING[static_cast<int>(cols[j]->getType())]
                            << ")";
                        return -2;
                    }
                    gb.swap(*tmp);
                    delete tmp;
                }
            }
        }
        else {
            const uint32_t j = comps.find(sortkeys[0]);
            if (j < comps.aggSize()) {
                array_t<uint32_t> ind;
                // if (direction >= 0)
                    cols[j]->bottomk(keep, ind);
                // else
                //     cols[j]->topk(keep, ind);
                for (uint32_t i = 0; i < cols.size(); ++ i)
                    cols[i]->reorder(ind);
                ibis::util::reorder(rid2, ind);
                ngroups = ind.size();
            }
        }

        // if (direction < 0) { // reverse the order
        //     for (uint32_t j = 0; j < cols.size(); ++ j)
        //      for (uint32_t i = 0; i < ngroups/2; ++ i)
        //          cols[j]->swap(i, ngroups-1-i);
        //     for (uint32_t i = 0; i < ngroups/2; ++ i) {
        //      const uint32_t j = ngroups - 1 - i;
        //      ibis::RIDSet *tmp = rid2[i];
        //      rid2[i] = rid2[j];
        //      rid2[j] = tmp;
        //     }
        // }

        // time to put the smaller lists together again, also updates starts
        ibis::RIDSet rid1;
        rid1.reserve(rids->size());
        for (uint32_t i = 0; i < ngroups; ++ i) {
            rid1.insert(rid1.end(), rid2[i]->begin(), rid2[i]->end());
            (*starts)[i+1] = (*starts)[i] + rid2[i]->size();
            delete rid2[i];
        }
        rids->swap(rid1);
    }
    else { // no rids
        // turn starts into counts
        for (uint32_t i = 0; i < ngroups; ++ i)
            (*starts)[i] = (*starts)[i+1] - (*starts)[i];
        starts->resize(ngroups);
        if (sortkeys.size() > 1) {
            array_t<uint32_t> gb;
            uint32_t i = 0;
            uint32_t j0 = comps.find(sortkeys[0]);
            while (j0 >= comps.aggSize() && i < sortkeys.size()) {
                ++ i;
                j0 = comps.find(sortkeys[i]);
            }
            if (i >= sortkeys.size())
                return truncate(keep);

            array_t<uint32_t> ind0; // indices over all ngroups
            ind0.reserve(keep);
            // deal with the first sort key
            // if (direction >= 0)
                cols[j0]->bottomk(keep, ind0);
            // else
            //  cols[j0]->topk(keep, ind0);
            for (uint32_t ii = 0; ii < cols.size(); ++ ii)
                cols[ii]->reorder(ind0);
            ibis::util::reorder(*starts, ind0);
            ngroups = ind0.size();
            { // segment cols[j0]
                array_t<uint32_t> *tmp = cols[j0]->segment(&gb);
                if (tmp == 0) {
                    LOGGER(ibis::gVerbose >= 0)
                        << "Warning -- bundles::truncate failed to segment "
                        "the values of " << cols[0]->name() << " ("
                        << ibis::TYPESTRING[static_cast<int>(cols[0]->getType())]
                        << ")";
                    return -3;
                }
                gb.swap(*tmp);
                delete tmp;
            }

            for (++ i;
                 i < sortkeys.size() && gb.size() <= ngroups;
                 ++ i) {
                const uint32_t j1 = comps.find(sortkeys[i]);
                if (j1 >= comps.aggSize()) continue;

                for (uint32_t g = 0; g < gb.size()-1; ++ g) {
                    if (gb[g+1] > gb[g]+1) { // more than one group
                        array_t<uint32_t> ind1; // indices for group g
                        cols[j1]->sort(gb[g], gb[g+1], ind1);
                        ind0.insert(ind0.end(), ind1.begin(), ind1.end());
                    }
                    else { // a single group
                        ind0.push_back(gb[g]);
                    }
                }
                for (uint32_t k = 0; k < cols.size(); ++ k)
                    cols[k]->reorder(ind0);
                ibis::util::reorder(*(starts), ind0);

                {
                    array_t<uint32_t> *tmp = cols[j1]->segment(&gb);
                    if (tmp == 0) {
                        LOGGER(ibis::gVerbose >= 0)
                            << "Warning -- bundles::truncate failed to segment "
                            "the values of " << cols[j1]->name() << " ("
                            << ibis::TYPESTRING[static_cast<int>(cols[j1]->getType())]
                            << ")";
                        return -4;
                    }
                    gb.swap(*tmp);
                    delete tmp;
                }
            }
        }
        else {
            const uint32_t j = comps.find(sortkeys[0]);
            if (j < comps.aggSize()) {
                array_t<uint32_t> ind;
                // if (direction >= 0)
                    cols[j]->bottomk(keep, ind);
                // else
                //     cols[j]->topk(keep, ind);
                for (uint32_t i = 0; i < cols.size(); ++ i)
                    cols[i]->reorder(ind);
                ibis::util::reorder(*(starts), ind);
                ngroups = ind.size();
            }
        }

        // if (direction < 0) { // reverse the order
        //     for (uint32_t j = 0; j < cols.size(); ++ j)
        //      for (uint32_t i = 0; i < ngroups/2; ++ i)
        //          cols[j]->swap(i, ngroups-1-i);
        //     for (uint32_t i = 0; i < ngroups/2; ++ i) {
        //      const uint32_t j = ngroups - 1 - i;
        //      const uint32_t tmp = (*starts)[i];
        //      (*starts)[i] = (*starts)[j];
        //      (*starts)[j] = tmp;
        //     }
        // }

        /// turn counts back into starting positions (starts)
        uint32_t cumu = 0;
        for (uint32_t i = 0; i < ngroups; ++ i) {
            uint32_t tmp = (*starts)[i];
            (*starts)[i] = cumu;
            cumu += tmp;
        }
        starts->push_back(cumu);
    }

    // truncate arrays
    if (ngroups > keep) {
        if (rids != 0)
            rids->resize((*starts)[keep]);
        starts->resize(keep+1);
        for (uint32_t i = 0; i < cols.size(); ++ i)
            cols[i]->truncate(keep);
    }
    // new content, definitely not in file yet
    infile = false;
    return size();
} // ibis::bundles::truncate

/// Clear the existing content.
void ibis::bundles::clear() {
    LOGGER(ibis::gVerbose > 5)
        << "bundles[" << (id && *id ? id : "") << "] -- clearing "
        << cols.size() << " colValue object" << (cols.size()>1?"s":"");
    for (ibis::colList::iterator it = cols.begin(); it != cols.end(); ++it)
        delete *it;
    cols.clear();
}

void ibis::bundles::write(const ibis::query& theQ) const {
    if (theQ.dir() == 0) return;
    if (cols.size() == 0) return;
    if (infile) return;
    if (starts == 0) return;
    if (cols[0]->size() == 0) return;
    if (cols[0]->size()+1 != starts->size()) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- bundles::write found an invalid bundle (starts->size("
            << starts->size() << ") != cols[0]->size(" << cols[0]->size()
            << ")+1)";
        return;
    }

    if (rids != 0)
        theQ.writeRIDs(rids); // write the RIDs

    uint32_t len = std::strlen(theQ.dir());
    char* fn = new char[len+16];
    strcpy(fn, theQ.dir());
    strcat(fn, "bundles");
    FILE* fptr = fopen(fn, "wb");
    if (fptr == 0) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- bundles::write failed to open file \""
            << fn << "\" ... "
            << (errno ? strerror(errno) : "no free stdio stream");
        return;
    }

    uint32_t i1, ncol = cols.size(), tmp = cols[0]->size();
    int32_t ierr = fwrite(&tmp, sizeof(uint32_t), 1, fptr);
    ierr += fwrite(&ncol, sizeof(uint32_t), 1, fptr);
    if (ierr < 2) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- bundles::write failed to write number of rows "
            "and columns to " << fn;
        return;
    }
    for (i1 = 0; i1 < ncol; ++ i1) { // element sizes
        tmp = cols[i1]->elementSize(); // ibis::colValue::elementSize
        ierr = fwrite(&tmp, sizeof(uint32_t), 1, fptr);
        LOGGER(cols[i1]->size() != cols[0]->size() && ibis::gVerbose >= 0)
            << "Warning -- invalid ibis::bundle object (cols[i1]->size("
            << cols[i1]->size() << ") != cols[0]->size("
            << cols[0]->size() << "))";
    }

    for (i1 = 0; i1 < ncol; ++ i1) { // the actual values
        ierr = cols[i1]->write(fptr);
    }

    // the starting positions
    ierr = fwrite(starts->begin(), sizeof(uint32_t), starts->size(),
                  fptr);
#if defined(FASTBIT_SYNC_WRITE)
    (void) fflush(fptr);
#endif
    (void) fclose(fptr);
    delete [] fn;
    infile = true;
} // ibis::bundles::write

/// Retrieve the value of i-th row j-th column as a 32-bit integer.
/// Return the maximal value defined in the class numeric_limits if indices
/// i and j are out of bounds.
int32_t ibis::bundles::getInt(uint32_t i, uint32_t j) const {
    int32_t ret = 0x7FFFFFFF;
    if (j < cols.size() && i < cols[j]->size()) { // indices i and j are valid
        ret = cols[j]->getInt(i);
    }
    return ret;
} // ibis::bundles::getInt

/// Retrieve the value of i-th row j-th column as a 32-bit unsigned integer.
/// Return the maximal value defined in the class numeric_limits if indices
/// i and j are out of bounds.
uint32_t ibis::bundles::getUInt(uint32_t i, uint32_t j) const {
    uint32_t ret = 0xFFFFFFFFU;
    if (j < cols.size() && i < cols[j]->size()) { // indices i and j are valid
        ret = cols[j]->getUInt(i);
    }
    return ret;
} // ibis::bundles::getUInt

/// Retrieve the value of i-th row j-th column as a 64-bit integer.
/// Return the maximal value defined in the class numeric_limits if indices
/// i and j are out of bounds.
int64_t ibis::bundles::getLong(uint32_t i, uint32_t j) const {
    int64_t ret = 0x7FFFFFFFFFFFFFFFLL;
    if (j < cols.size() && i < cols[j]->size()) { // indices i and j are valid
        ret = cols[j]->getLong(i);
    }
    return ret;
} // ibis::bundles::getLong

/// Retrieve the value of i-th row j-th column as a 64-bit unsigned integer.
/// Return the maximal value defined in the class numeric_limits if indices
/// i and j are out of bounds.
uint64_t ibis::bundles::getULong(uint32_t i, uint32_t j) const {
    uint64_t ret = 0xFFFFFFFFFFFFFFFFULL;
    if (j < cols.size() && i < cols[j]->size()) { // indices i and j are valid
        ret = cols[j]->getULong(i);
    }
    return ret;
} // ibis::bundles::getULong

/// Retrieve the value of i-th row j-th column as a 32-bit floating-point
/// number.
/// Return the maximal value defined in the class numeric_limits if indices
/// i and j are out of bounds.
float ibis::bundles::getFloat(uint32_t i, uint32_t j) const {
    float ret = FLT_MAX;
    if (j < cols.size() && i < cols[j]->size()) { // indices i and j are valid
        ret = cols[j]->getFloat(i);
    }
    return ret;
} // ibis::bundles::getFloat

/// Retrieve the value of i-th row j-th column as a 64-bit floating-point
/// number. 
/// Return the maximal value defined in the class numeric_limits if indices
/// i and j are out of bounds.
double ibis::bundles::getDouble(uint32_t i, uint32_t j) const {
    double ret = DBL_MAX;
    if (j < cols.size() && i < cols[j]->size()) { // indices i and j are valid
        ret = cols[j]->getDouble(i);
    }
    return ret;
} // ibis::bundles::getDouble

/// Retrieve the value of i-th row j-th column as a string.
/// Convert the value to its string representation through @c
/// std::ostringstream.
std::string ibis::bundles::getString(uint32_t i, uint32_t j) const {
    std::ostringstream oss;
    if (j < cols.size() && i < cols[j]->size()) { // indices i and j are valid
        cols[j]->write(oss, i);
    }
    return oss.str();
} // ibis::bundles::getString

ibis::query::result::result(ibis::query& q)
    : que_(q), bdl_(0), sel(q.components()), bid_(0), lib_(0) {
    const ibis::query::QUERY_STATE st = q.getState();
    if (st == ibis::query::UNINITIALIZED ||
        st == ibis::query::SET_COMPONENTS) {
        throw ibis::bad_alloc("Can not construct query::result on "
                              "an incomplete query" IBIS_FILE_LINE);
    }
    if (sel.empty()) {
        throw ibis::bad_alloc("Can not construct query::result on "
                              "a query with an empty select clause"
                              IBIS_FILE_LINE);
    }
    if (st == ibis::query::SPECIFIED ||
        st == ibis::query::QUICK_ESTIMATE) {
        int ierr = q.evaluate();
        if (ierr < 0) {
            LOGGER(ibis::gVerbose >= 0)
                << "Error -- query::result constructor failed "
                "to evaluate query " <<  q.id();
            throw ibis::bad_alloc("Can not evaluate query" IBIS_FILE_LINE);
        }
    }
    bdl_ = ibis::bundle::create(q);
    if (bdl_ == 0) {
        LOGGER(ibis::gVerbose >= 0)
            << "Error -- query::result constructor failed "
            "to create a bundle object from query " << q.id();
        throw ibis::bad_alloc("failed to create a result set from query"
                              IBIS_FILE_LINE);
    }
} // ibis::query::result::result

ibis::query::result::~result() {
    delete bdl_;
    bdl_ = 0;
    bid_ = 0;
    lib_ = 0;
} // ibis::query::result::~result

bool ibis::query::result::next() {
    bool ret = false;
    if (bdl_ == 0)
        return ret;
    const uint32_t bsize = bdl_->size();
    if (bid_ < bsize) {
        ret = true;
        if (lib_ > 0) {
            -- lib_;
        }
        else { // need to move on to the next bundle
            lib_ = bdl_->numRowsInBundle(bid_) - 1;
            ++ bid_;
        }
    }
    else if (bid_ == bsize) {
        if (lib_ > 0) {
            -- lib_;
            ret = true;
        }
        else {
            ++ bid_;
        }
    }
    return ret;
} // ibis::query::result::next

bool ibis::query::result::nextBundle() {
    bool ret = false;
    if (bdl_ == 0)
        return ret;
    const uint32_t bsize = bdl_->size();
    if (bid_ < bsize) {
        ret = true;
        lib_ = bdl_->numRowsInBundle(bid_) - 1;
        ++ bid_;
    }
    else if (bid_ == bsize) {
        lib_ = 0;
        ++ bid_;
    }
    return ret;
} // ibis::query::result::nextBundle

void ibis::query::result::reset() {
    bid_ = 0;
    lib_ = 0;
} // ibis::query::result::reset

int32_t ibis::query::result::getInt(const char *cname) const {
    uint32_t ind = sel.find(cname);
    return getInt(ind);
} // ibis::query::result::getInt

uint32_t ibis::query::result::getUInt(const char *cname) const {
    uint32_t ind = sel.find(cname);
    return getUInt(ind);
} // ibis::query::result::getUInt

int64_t ibis::query::result::getLong(const char *cname) const {
    uint32_t ind = sel.find(cname);
    return getLong(ind);
} // ibis::query::result::getLong

uint64_t ibis::query::result::getULong(const char *cname) const {
    uint32_t ind = sel.find(cname);
    return getULong(ind);
} // ibis::query::result::getULong

float ibis::query::result::getFloat(const char *cname) const {
    uint32_t ind = sel.find(cname);
    return getFloat(ind);
} // ibis::query::result::getFloat

double ibis::query::result::getDouble(const char *cname) const {
    uint32_t ind = sel.find(cname);
    return getDouble(ind);
} // ibis::query::result::getDouble

std::string ibis::query::result::getString(const char *cname) const {
    uint32_t ind = sel.find(cname);
    return getString(ind);
} // ibis::query::result::getDouble
