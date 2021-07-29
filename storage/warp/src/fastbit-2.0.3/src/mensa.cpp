// File $Id$
// author: John Wu <John.Wu at ACM.org> Lawrence Berkeley National Laboratory
// Copyright (c) 2007-2016 the Regents of the University of California
//
#if defined(_WIN32) && defined(_MSC_VER)
#pragma warning(disable:4786)   // some identifier longer than 256 characters
#endif

#include "tab.h"        // ibis::tabula and ibis::tabele
#include "bord.h"       // ibis::bord
#include "mensa.h"      // ibis::mensa
#include "countQuery.h" // ibis::countQuery
#include "selectClause.h"       // ibis::selectClause
#include "index.h"      // ibis::index

#include "blob.h"       // ibis::blob
#include "category.h"   // ibis::text

#include <algorithm>    // std::sort, std::copy
#include <sstream>      // std::ostringstream
#include <limits>       // std::numeric_limits
#include <cmath>        // std::floor
#include <iomanip>      // std::setprecision

/// This function expects a valid data directory to find data partitions.
/// If the incoming directory is not a valid string, it will use
/// ibis::gParameter() to find data partitions.
ibis::mensa::mensa(const char* dir) : nrows(0) {
    if (dir != 0 && *dir != 0)
        ibis::util::gatherParts(parts, dir, true);
    if (parts.empty())
        ibis::util::gatherParts(parts, ibis::gParameters(), true);
    for (ibis::partList::const_iterator it = parts.begin();
         it != parts.end(); ++ it) {
        (*it)->combineNames(naty);
        nrows += (*it)->nRows();
    }
    if (name_.empty() && ! parts.empty()) {
        // take on the name of the first partition
        ibis::partList::const_iterator it = parts.begin();
        name_ = "T-";
        name_ += (*it)->name();
        if (desc_.empty()) {
            if (dir != 0 && *dir != 0)
                desc_ = dir;
            else
                desc_ = "data specified in RC file";
        }
    }
    if (ibis::gVerbose > 0 && ! name_.empty()) {
        ibis::util::logger lg;
        lg() << "mensa -- constructed table "
             << name_ << " (" << desc_ << ") from ";
        if (dir != 0 && *dir != 0)
            lg() << "directory " << dir;
        else
            lg() << "RC file entries";
        lg() << ".  It consists of " << parts.size() << " partition"
             << (parts.size()>1 ? "s" : "") << " with "
             << naty.size() << " column"
             << (naty.size()>1 ? "s" : "") << " and "
             << nrows << " row" << (nrows>1 ? "s" : "");
    }
} // constructor with one directory as argument

/// This function expects a pair of data directories to define data
/// partitions.  If either dir1 and dir2 is not valid, it will attempt to
/// find data partitions using global parameters ibis::gParameters().
ibis::mensa::mensa(const char* dir1, const char* dir2) : nrows(0) {
    if (*dir1 == 0 && *dir2 == 0) return;
    if (dir1 != 0 && *dir1 != 0) {
        ibis::util::gatherParts(parts, dir1, dir2, true);
    }
    if (parts.empty())
        ibis::util::gatherParts(parts, ibis::gParameters(), true);
    for (ibis::partList::const_iterator it = parts.begin();
         it != parts.end(); ++ it) {
        (*it)->combineNames(naty);
        nrows += (*it)->nRows();
    }
    if (name_.empty() && ! parts.empty()) {
        // take on the name of the first partition
        ibis::partList::const_iterator it = parts.begin();
        name_ = "T-";
        name_ += (*it)->name();
        if (desc_.empty()) {
            if (dir1 != 0 && *dir1 != 0) {
                desc_ = dir1;
                if (dir2 != 0 && *dir2 != 0) {
                    desc_ += " + ";
                    desc_ += dir2;
                }
            }
            else {
                desc_ = "data specified in RC file";
            }
        }
    }
    if (ibis::gVerbose > 0 && ! name_.empty()) {
        ibis::util::logger lg;
        lg() << "mensa -- constructed table "
             << name_ << " (" << desc_ << ") from ";
        if (dir1 != 0 && *dir1 != 0) {
            if (dir2 != 0 && *dir2 != 0)
                lg() << "directories " << dir1 << " + " << dir2;
            else
                lg() << "directory " << dir1;
        }
        else {
            lg() << "RC file entries";
        }
        lg() << ".  It consists of " << parts.size() << " partition"
             << (parts.size()>1 ? "s" : "") << " with "
             << naty.size() << " column"
             << (naty.size()>1 ? "s" : "") << " and "
             << nrows << "row" << (nrows>1 ? "s" : "");
    }
} // constructor with two directories as arguments

/// Add data partitions defined in the named directory.  It uses opendir
/// and friends to traverse the subdirectories, which means it will only
/// able to descend to subdirectories on unix and compatible systems.
int ibis::mensa::addPartition(const char* dir) {
    const uint32_t npold = parts.size();
    const uint32_t ncold = naty.size();
    const uint64_t nrold = nrows;
    unsigned int newparts = 0;
    if (dir != 0 && *dir != 0)
        newparts = ibis::util::gatherParts(parts, dir, true);
    else
        newparts = ibis::util::gatherParts(parts, ibis::gParameters(), true);
    if (newparts == 0) {
        LOGGER(ibis::gVerbose > 1)
            << "mensa::addPartition(" << dir
            << ") did not find any valid data partition";
        return -2;
    }
    LOGGER(ibis::gVerbose > 1)
        << "mensa::addPartition(" << dir << ") found " << newparts
        << " new data partition" << (newparts > 1 ? "s" : "");

    nrows = 0;
    for (ibis::partList::const_iterator it = parts.begin();
         it != parts.end(); ++ it) {
        (*it)->combineNames(naty);
        nrows += (*it)->nRows();
    }

    if (name_.empty() && ! parts.empty()) {
        // take on the name of the first partition
        ibis::partList::const_iterator it = parts.begin();
        name_ = "T-";
        name_ += (*it)->name();
        if (desc_.empty()) {
            if (dir != 0 && *dir != 0)
                desc_ = dir;
            else
                desc_ = "data specified in RC file";
        }
    }
    LOGGER(ibis::gVerbose > 0)
        << "mensa::addPartition(" << dir
        << ") increases the number partitions from " << npold << " to "
        << parts.size() << ", the number of rows from " << nrold << " to "
        << nrows << ", and the number of columns from " << ncold << " to "
        << naty.size();
    return newparts;
} // ibis::mensa::addPartition

int ibis::mensa::dropPartition(const char *nm) {
    if (nm == 0) return -1;
    ibis::util::mutexLock lock(&ibis::util::envLock, nm);

    int cnt = 0;
    if (*nm == 0) { // drop every partition
        cnt = parts.size();
        parts.clear();
        naty.clear();
        nrows = 0;
        return cnt;
    }

    // loop to check the names, all partition names are assumed to be unique
    for (size_t j = 0; j < parts.size(); ++ j) {
        if (stricmp(nm, parts[j]->name()) == 0) {
            nrows -= parts[j]->nRows();
            while (j+1 < parts.size()) {
                parts[j] = parts[j+1];
                ++ j;
            }
            parts.resize(parts.size()-1);
            return 1;
        }
    }

    // did not match any partition names, try directory names
    size_t nmlen = std::strlen(nm);
    size_t j, k;
    j = 0;
    k = parts.size();
    while (j < k) {
        const char *dir = parts[j]->currentDataDir();
        if (strncmp(nm, dir, nmlen) == 0 &&
            (dir[nmlen] == 0 || dir[nmlen] == FASTBIT_DIRSEP)) {
            -- k;
            nrows -= parts[j]->nRows();
            if (k > j) {
                ibis::part *tmp = parts[j];
                parts[j] = parts[k];
                parts[k] = tmp;
            }
        }
        else {
            ++ j;
        }
    }
    cnt = parts.size() - k;
    parts.resize(k);
    return cnt;
} // ibis::mensa::dropPartition

int ibis::mensa::getPartitions(ibis::constPartList &lst) const {
    if (! lst.empty()) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- mensa::getPartitions is to clear the "
            "partitions in the incoming argument";
    }

    lst.resize(parts.size());
    for (uint32_t i = 0; i < parts.size(); ++ i)
        lst[i] = parts[i];
    return parts.size();
} // ibis::mensa::getPartitions

void ibis::mensa::clear() {
    const uint32_t np = parts.size();
    LOGGER(ibis::gVerbose > 2 && np > 0)
        << "mensa::clear -- clearing the existing content of "
        << np << " partition" << (np>1 ? "s" : "") << " with "
        << naty.size() << " column" << (naty.size()>1 ? "s" : "")
        << " and " << nrows << " row" << (nrows>1 ? "s" : "");

    nrows = 0;
    naty.clear();
    name_.erase();
    desc_.erase();
    for (uint32_t j = 0; j < np; ++ j)
        delete parts[j];
} // ibis::mensa::clear

/// Number of columns.  It actually returns the number of columns of
/// the first data partition.  This is consistent with other functions
/// such as columnTypes and columnNames.
uint32_t ibis::mensa::nColumns() const {
    return (parts.empty()?0:parts.front()->nColumns());
} // ibis::mensa::nColumns

/// Return the column names in a list.
///
/// @note this implementation only look at the first data partition in the
/// list of data partitions.
///
/// @note the list of column names contains raw pointers to column names.
/// If the underlying data partition is removed, these pointers will become
/// invalid.
ibis::table::stringArray ibis::mensa::columnNames() const {
    if (parts.empty()) {
        ibis::table::stringArray res;
        return res;
    }
    else {
        return parts.front()->columnNames();
    }
} // ibis::mensa::columnNames

/// Return the column types in a list.
ibis::table::typeArray ibis::mensa::columnTypes() const {
    if (parts.empty()) {
        ibis::table::typeArray res;
        return res;
    }
    else {
        return parts.front()->columnTypes();
    }
} // ibis::mensa::columnTypes

void ibis::mensa::describe(std::ostream& out) const {
    out << "Table (on disk) " << name_ << " (" << desc_ << ") consists of "
        << parts.size() << " partition" << (parts.size()>1 ? "s" : "")
        << " with "<< naty.size() << " column" << (naty.size()>1 ? "s" : "")
        << " and " << nrows << " row" << (nrows>1 ? "s" : "");
    for (ibis::table::namesTypes::const_iterator it = naty.begin();
         it != naty.end(); ++ it)
        out << "\n" << (*it).first << "\t" << ibis::TYPESTRING[(*it).second];
    out << std::endl;
} // ibis::mensa::describe

void ibis::mensa::dumpNames(std::ostream& out, const char* del) const {
    if (naty.empty()) return;

    ibis::table::namesTypes::const_iterator it = naty.begin();
    out << (*it).first;
    for (++ it; it != naty.end(); ++ it)
        out << del << (*it).first;
    out << std::endl;
} // ibis::mensa::dumpNames

const char* ibis::mensa::indexSpec(const char* colname) const {
    if (parts.empty()) {
        return 0;
    }
    else if (colname == 0 || *colname == 0) {
        return parts[0]->indexSpec();
    }
    else {
        for (ibis::partList::const_iterator it = parts.begin();
             it != parts.end(); ++ it) {
            const ibis::column* col = (*it)->getColumn(colname);
            if (col != 0)
                return col->indexSpec();
        }
        return parts[0]->indexSpec();
    }
} // ibis::mensa::indexSpec

void ibis::mensa::indexSpec(const char* opt, const char* colname) {
    if (opt == 0 || *opt == 0) return;

    for (ibis::partList::iterator it = parts.begin();
         it != parts.end(); ++ it) {
        if (colname == 0 || *colname == 0) {
            (*it)->indexSpec(opt);
            (*it)->updateMetaData();
        }
        else {
            ibis::column* col = (*it)->getColumn(colname);
            if (col != 0) {
                col->indexSpec(opt);
                (*it)->updateMetaData();
            }
        }
    }
} // ibis::mensa::indexSpec

int ibis::mensa::buildIndex(const char* colname, const char* option) {
    if (colname == 0 || *colname == 0) return -1;

    int ierr = 0;
    for (ibis::partList::const_iterator it = parts.begin();
         it != parts.end(); ++ it) {
        ibis::column* col = (*it)->getColumn(colname);
        if (col != 0) {
            col->loadIndex(option);
            col->unloadIndex();
        }
    }

    if (ierr == 0)
        ierr = -2;
    else if ((unsigned)ierr < parts.size())
        ierr = 1;
    else
        ierr = 0;
    return ierr;
} // ibis::mensa::buildIndex

int ibis::mensa::buildIndexes(const char* opt) {
    for (ibis::partList::const_iterator it = parts.begin();
         it != parts.end(); ++ it) {
        (*it)->buildIndexes(opt, 1);
    }
    return 0;
} // ibis::mensa::buildIndexes

int ibis::mensa::buildIndexes(const ibis::table::stringArray &opt) {
    for (ibis::partList::const_iterator it = parts.begin();
         it != parts.end(); ++ it) {
        (*it)->buildIndexes(opt);
    }
    return 0;
} // ibis::mensa::buildIndexes

int ibis::mensa::mergeCategories(const ibis::table::stringArray &nms) {
    if (parts.size() <= 1 && nms.size() == 0)
        return 0;

    std::string evt="mensa";
    if (ibis::gVerbose > 0) {
        evt += '[';
        evt += name_;
        evt += ']';
    }
    evt += "::mergeCategories";
    if (ibis::gVerbose > 1) {
        std::ostringstream oss;
        oss << '(';
        if (nms.empty()) {
            oss << "<NULL>";
        }
        else if (nms.size() == 1) {
            oss << (nms[0] ? nms[0] : "");
        }
        else {
            oss << nms.size() << " names";
        }
        oss << ')';
        evt += oss.str();
    }
    ibis::util::timer mytimer(evt.c_str(), 2);
    int ierr = 0, cnt = 0;

    if (nms.empty()) { // merge categorical columns with the same name
        ibis::table::stringArray names;
        std::vector<ibis::dictionary*> words;
        for (ibis::table::namesTypes::const_iterator it = naty.begin();
             it != naty.end();
             ++ it) { // gather all names
            if ((*it).second == ibis::CATEGORY) {
                names.push_back((*it).first);
                words.push_back(new ibis::dictionary());
            }
        }
        if (names.empty() || words.empty()) return 0;

        IBIS_BLOCK_GUARD(ibis::util::clearVec<ibis::dictionary>,
                         ibis::util::ref(words));
        // loop to consolidate the dictionaries
        for (ibis::partList::const_iterator it = parts.begin();
             it != parts.end();
             ++ it) {
            for (unsigned k = 0; k < names.size(); ++ k) {
                const ibis::column *c0 = (*it)->getColumn(names[k]);
                const ibis::category *c1 =
                    dynamic_cast<const ibis::category*>(c0);
                if (c1 != 0) {
                    c1->loadIndex(0, 0); // force initalization of all members
                    ierr = words[k]->merge(*(c1->getDictionary()));
                    LOGGER(ibis::gVerbose > 0 && ierr < 0)
                        << "Warning -- " << evt
                        << " failed to merge dictionary for "
                        << (*it)->name() << '.' << c1->name()
                        << ", ierr = " << ierr;
                }
            }
        }

        // sort the new combined dictionaries
        for (unsigned k = 0; k < names.size(); ++ k) {
            ibis::array_t<uint32_t> tmp;
            words[k]->sort(tmp);
        }

        // loop to update the indexes
        for (ibis::partList::iterator it = parts.begin();
             it != parts.end();
             ++ it) {
            for (unsigned k = 0; k < names.size(); ++ k) {
                ibis::column *c0 = (*it)->getColumn(names[k]);
                ibis::category *c1 = dynamic_cast<ibis::category*>(c0);
                if (c1 != 0) {
                    ierr = c1->setDictionary(*(words[k]));
                    LOGGER(ibis::gVerbose > 0 && ierr < 0)
                        << "Warning -- " << evt
                        << " failed to change dictionary for "
                        << (*it)->name() << '.' << c1->name()
                        << ", ierr = " << ierr;
                    cnt += (ierr >= 0);
                }
            }
        }
    } // merge categorical columns with the same name
    else { // merge columns with the specified names
        ibis::dictionary words;
        // loop to gather all the words
        for (ibis::partList::const_iterator pit = parts.begin();
             pit != parts.end();
             ++ pit) {
            for (ibis::table::stringArray::const_iterator nit = nms.begin();
                 nit != nms.end();
                 ++ nit) {
                const ibis::column *c0 = (*pit)->getColumn(*nit);
                const ibis::category *c1 =
                    dynamic_cast<const ibis::category*>(c0);
                if (c1 != 0) {
                    c1->loadIndex(0, 0); // force initalization of all members
                    ierr = words.merge(*c1->getDictionary());
                    LOGGER(ierr < 0 && ibis::gVerbose > 0)
                        << "Warning -- " << evt
                        << " failed to merge words from "
                        << (*pit)->name() << '.' << c1->name()
                        << ", ierr = " << ierr;
                }
            }
        }

        if (words.size() == 0) return 0;
        { // sort the new combined dictionary
            ibis::array_t<uint32_t> tmp;
            words.sort(tmp);
        }

        // loop to update the indexes
        for (ibis::partList::iterator pit = parts.begin();
             pit != parts.end();
             ++ pit) {
            for (ibis::table::stringArray::const_iterator nit = nms.begin();
                 nit != nms.end();
                 ++ nit) {
                ibis::column *c0 = (*pit)->getColumn(*nit);
                ibis::category *c1 =
                    dynamic_cast<ibis::category*>(c0);
                if (c1 != 0) {
                    ierr = c1->setDictionary(words);
                    LOGGER(ierr < 0 && ibis::gVerbose > 0)
                        << "Warning -- " << evt
                        << " failed to update index for "
                        << (*pit)->name() << '.' << c1->name()
                        << ", ierr = " << ierr;
                    cnt += (ierr >= 0);
                }
            }
        }
    } // merge columns with the specified names

    return cnt;
} // ibis::mensa::mergeCateogires

void ibis::mensa::estimate(const char* cond,
                           uint64_t& nmin, uint64_t& nmax) const {
    nmin = 0;
    nmax = nRows();
    ibis::countQuery qq;
    int ierr = qq.setWhereClause(cond);
    if (ierr < 0) {
        return;
    }

    for (ibis::partList::const_iterator it = parts.begin();
         it != parts.end(); ++ it) {
        ierr = qq.setPartition(*it);
        if (ierr >= 0) {
            ierr = qq.estimate();
            if (ierr >= 0) {
                nmin += qq.getMinNumHits();
                nmax += qq.getMaxNumHits();
            }
            else {
                nmax += (*it)->nRows();
            }
        }
        else {
            nmax += (*it)->nRows();
        }
    }
} // ibis::mensa::estimate

void ibis::mensa::estimate(const ibis::qExpr* cond,
                           uint64_t& nmin, uint64_t& nmax) const {
    nmin = 0;
    nmax = nRows();
    ibis::countQuery qq;
    int ierr = qq.setWhereClause(cond);
    if (ierr < 0) {
        return;
    }

    for (ibis::partList::const_iterator it = parts.begin();
         it != parts.end(); ++ it) {
        ierr = qq.setPartition(*it);
        if (ierr >= 0) {
            ierr = qq.estimate();
            if (ierr >= 0) {
                nmin += qq.getMinNumHits();
                nmax += qq.getMaxNumHits();
            }
            else {
                nmax += (*it)->nRows();
            }
        }
        else {
            nmax += (*it)->nRows();
        }
    }
} // ibis::mensa::estimate

ibis::table* ibis::mensa::select(const char* sel, const char* cond) const {
    if (cond == 0 || *cond == 0 || nRows() == 0 || nColumns() == 0) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- mensa::select requires a non-empty table "
            "and a valid where clause";
        return 0;
    }

    if (sel != 0) // skip leading space
        while (isspace(*sel)) ++ sel;
    if (sel == 0 || *sel == 0) {
        int64_t nhits = computeHits(cond);
        if (nhits < 0) {
            return 0;
        }
        else {
            std::string des = name_;
            if (! desc_.empty()) {
                des += " -- ";
                des += desc_;
            }
            return new ibis::tabula(cond, des.c_str(), nhits);
        }
    }
    else if (stricmp(sel, "count(*)") == 0) { // count(*)
        int64_t nhits = computeHits(cond);
        if (nhits < 0) {
            return 0;
        }
        else {
            std::string des = name_;
            if (! desc_.empty()) {
                des += " -- ";
                des += desc_;
            }
            return new ibis::tabele(cond, des.c_str(), nhits, sel);
        }
    }
    else {
        // handle the non-trivial case in a separate function
        return ibis::table::select
            (reinterpret_cast<const ibis::constPartList&>(parts),
             sel, cond);
    }
} // ibis::mensa::select

/// A variation of the function select defined in ibis::table.  It accepts
/// an extra argument for caller to specify a list of names of data
/// partitions that will participate in the select operation.  The argument
/// pts may contain wild characters accepted by SQL function 'LIKE', more
/// specifically, '_' and '%'.  If the argument pts is a nil pointer or an
/// empty string
ibis::table* ibis::mensa::select2(const char* sel, const char* cond,
                                  const char* pts) const {
    if (cond == 0 || *cond == 0 || pts == 0 || *pts == 0) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- mensa::select2 requires a non-empty table "
            "and a valid where clause";
        return 0;
    }

    while (isspace(*pts)) ++ pts;
    while (isspace(*cond)) ++ cond;
    if (sel != 0) // skip leading space
        while (isspace(*sel)) ++ sel;
    if (*pts == 0) return 0;
    if (*pts == '%' || *pts == '*') return select(sel, cond);

    ibis::nameList patterns(pts);
    if (patterns.empty()) {
        LOGGER(ibis::gVerbose > 0)
            << "mensa::select2 can not find any valid data partition "
            "names to use";
        return 0;
    }

    ibis::constPartList mylist;
    for (unsigned k = 0; k < parts.size(); ++ k) {
        for (unsigned j = 0; j < patterns.size(); ++ j) {
            const char* pat = patterns[j];
            if (stricmp(pat, parts[k]->name()) == 0) {
                mylist.push_back(parts[k]);
                break;
            }
            else if (ibis::util::strMatch(parts[k]->name(), pat)) {
                mylist.push_back(parts[k]);
                break;
            }
        }
    }
    if (mylist.empty()) {
        LOGGER(ibis::gVerbose > 0)
            << "mensa::select2 cannot find any data partitions "
            "matching \"" << pts << "\"";
        return 0;
    }

    if (sel == 0 || *sel == 0) {
        int64_t nhits = ibis::table::computeHits(mylist, cond);
        if (nhits < 0) {
            return 0;
        }
        else {
            std::string des = name_;
            if (! desc_.empty()) {
                des += " -- ";
                des += desc_;
            }
            return new ibis::tabula(cond, des.c_str(), nhits);
        }
    }
    else if (stricmp(sel, "count(*)") == 0) { // count(*)
        int64_t nhits = ibis::table::computeHits(mylist, cond);
        if (nhits < 0) {
            return 0;
        }
        else {
            std::string des = name_;
            if (! desc_.empty()) {
                des += " -- ";
                des += desc_;
            }
            return new ibis::tabele(cond, des.c_str(), nhits, sel);
        }
    }
    else {
        return ibis::table::select(mylist, sel, cond);
    }
    return 0;
} // ibis::mensa::select2

/// Reordering the rows using the specified columns.  Each data partition
/// is reordered separately.
void ibis::mensa::orderby(const ibis::table::stringArray& names) {
    for (ibis::partList::iterator it = parts.begin();
         it != parts.end(); ++ it) {
        long ierr = (*it)->reorder(names);
        if (ierr < 0) {
            LOGGER(ibis::gVerbose >= 0)
                << "mensa::orderby -- reordering partition "
                << (*it)->name() << " encountered error " << ierr;
        }
    }
} // ibis::mensa::orderby

/// Reordering the rows using the specified columns.  Each data partition
/// is reordered separately.
void ibis::mensa::orderby(const ibis::table::stringArray& names,
                          const std::vector<bool>& asc) {
    for (ibis::partList::iterator it = parts.begin();
         it != parts.end(); ++ it) {
        long ierr = (*it)->reorder(names, asc);
        if (ierr < 0) {
            LOGGER(ibis::gVerbose >= 0)
                << "mensa::orderby -- reordering partition "
                << (*it)->name() << " encountered error " << ierr;
        }
    }
} // ibis::mensa::orderby

int64_t ibis::mensa::getColumnAsBytes(const char* cn, char* vals,
                                      uint64_t begin, uint64_t end) const {
    if (end == 0 || end > nrows) end = nrows;
    if (begin >= end) return 0;
    {
        ibis::table::namesTypes::const_iterator nit = naty.find(cn);
        if (nit == naty.end())
            return -1;
        else if ((*nit).second != ibis::BYTE &&
                 (*nit).second != ibis::UBYTE)
            return -2;
    }

    array_t<signed char> tmp;
    uint64_t ival = 0;
    uint64_t irow = 0;
    for (ibis::partList::const_iterator it = parts.begin();
         it != parts.end() && irow < end; ++ it) {
        const ibis::part& dp = **it;
        if (irow + dp.nRows() > begin) {
            const ibis::column* col = dp.getColumn(cn);
            if (col == 0)
                return -3;
            if (col->getValuesArray(&tmp) < 0)
                return -4;

            const size_t i0 = (begin > irow ? begin - irow : 0);
            const size_t i1 = (end>=irow+dp.nRows() ? dp.nRows() :
                               end-irow) - i0;
            memcpy(vals+ival, tmp.begin()+i0, i1);
            ival += i1;
        }
        irow += dp.nRows();
    }
    return ival;
} // ibis::mensa::getColumnAsBytes

int64_t ibis::mensa::getColumnAsUBytes(const char* cn, unsigned char* vals,
                                       uint64_t begin, uint64_t end) const {
    if (end == 0 || end > nrows) end = nrows;
    if (begin >= end) return 0;
    {
        ibis::table::namesTypes::const_iterator nit = naty.find(cn);
        if (nit == naty.end())
            return -1;
        else if ((*nit).second != ibis::BYTE &&
                 (*nit).second != ibis::UBYTE)
            return -2;
    }

    array_t<unsigned char> tmp;
    uint64_t ival = 0;
    uint64_t irow = 0;
    for (ibis::partList::const_iterator it = parts.begin();
         it != parts.end() && irow < end; ++ it) {
        const ibis::part& dp = **it;
        if (irow + dp.nRows() > begin) {
            const ibis::column* col = dp.getColumn(cn);
            if (col == 0)
                return -3;
            if (col->getValuesArray(&tmp) < 0)
                return -4;

            const size_t i0 = (begin > irow ? begin - irow : 0);
            const size_t i1 = (end>=irow+dp.nRows() ? dp.nRows() :
                               end-irow) - i0;
            memcpy(vals+ival, tmp.begin()+i0, i1);
            ival += i1;
        }
        irow += dp.nRows();
    }
    return ival;
} // ibis::mensa::getColumnAsUBytes

int64_t ibis::mensa::getColumnAsShorts(const char* cn, int16_t* vals,
                                       uint64_t begin, uint64_t end) const {
    if (end == 0 || end > nrows) end = nrows;
    if (begin >= end) return 0;
    ibis::table::namesTypes::const_iterator nit = naty.find(cn);
    if (nit == naty.end())
        return -1;

    uint64_t ival = 0;
    switch ((*nit).second) {
    case ibis::BYTE:
    case ibis::UBYTE: {
        array_t<signed char> tmp;
        uint64_t irow = 0;
        for (ibis::partList::const_iterator it = parts.begin();
             it != parts.end() && irow < end; ++ it) {
            const ibis::part& dp = **it;
            if (irow + dp.nRows() > begin) {
                const ibis::column* col = dp.getColumn(cn);
                if (col == 0)
                    return -3;
                if (col->getValuesArray(&tmp) < 0)
                    return -4;

                const size_t i0 = (begin > irow ? begin - irow : 0);
                const size_t i1 = (end>=irow+dp.nRows() ? dp.nRows() : end-irow);
                std::copy(tmp.begin()+i0, tmp.begin()+i1, vals+ival);
                ival += (i1-i0);
            }
            irow += dp.nRows();
        }
        break;}
    case ibis::SHORT:
    case ibis::USHORT: {
        array_t<int16_t> tmp;
        uint64_t irow = 0;
        for (ibis::partList::const_iterator it = parts.begin();
             it != parts.end() && irow < end; ++ it) {
            const ibis::part& dp = **it;
            if (irow + dp.nRows() > begin) {
                const ibis::column* col = dp.getColumn(cn);
                if (col == 0)
                    return -3;
                if (col->getValuesArray(&tmp) < 0)
                    return -4;

                const size_t i0 = (begin > irow ? begin - irow : 0);
                const size_t i1 = (end>=irow+dp.nRows() ? dp.nRows() :
                                   end-irow) - i0;
                memcpy(vals+ival, tmp.begin()+i0, i1 * 2U);
                ival += i1;
            }
            irow += dp.nRows();
        }
        break;}
    default:
        return -2;
    }
    return ival;
} // ibis::mensa::getColumnAsShorts

int64_t ibis::mensa::getColumnAsUShorts(const char* cn, uint16_t* vals,
                                        uint64_t begin, uint64_t end) const {
    if (end == 0 || end > nrows) end = nrows;
    if (begin >= end) return 0;
    ibis::table::namesTypes::const_iterator nit = naty.find(cn);
    if (nit == naty.end())
        return -1;

    uint64_t ival = 0;
    switch ((*nit).second) {
    case ibis::BYTE:
    case ibis::UBYTE: {
        array_t<unsigned char> tmp;
        uint64_t irow = 0;
        for (ibis::partList::const_iterator it = parts.begin();
             it != parts.end() && irow < end; ++ it) {
            const ibis::part& dp = **it;
            if (irow + dp.nRows() > begin) {
                const ibis::column* col = dp.getColumn(cn);
                if (col == 0)
                    return -3;
                if (col->getValuesArray(&tmp) < 0)
                    return -4;

                const size_t i0 = (begin > irow ? begin - irow : 0);
                const size_t i1 = (end>=irow+dp.nRows() ? dp.nRows() : end-irow);
                std::copy(tmp.begin()+i0, tmp.begin()+i1, vals+ival);
                ival += (i1-i0);
            }
            irow += dp.nRows();
        }
        break;}
    case ibis::SHORT:
    case ibis::USHORT: {
        array_t<uint16_t> tmp;
        uint64_t irow = 0;
        for (ibis::partList::const_iterator it = parts.begin();
             it != parts.end() && irow < end; ++ it) {
            const ibis::part& dp = **it;
            if (irow + dp.nRows() > begin) {
                const ibis::column* col = dp.getColumn(cn);
                if (col == 0)
                    return -3;
                if (col->getValuesArray(&tmp) < 0)
                    return -4;

                const size_t i0 = (begin > irow ? begin - irow : 0);
                const size_t i1 = (end>=irow+dp.nRows() ? dp.nRows() :
                                   end-irow) - i0;
                memcpy(vals+ival, tmp.begin()+i0, i1 * 2U);
                ival += i1;
            }
            irow += dp.nRows();
        }
        break;}
    default:
        return -2;
    }
    return ival;
} // ibis::mensa::getColumnAsUShorts

int64_t ibis::mensa::getColumnAsInts(const char* cn, int32_t* vals,
                                     uint64_t begin, uint64_t end) const {
    if (end == 0 || end > nrows) end = nrows;
    if (begin >= end) return 0;
    ibis::table::namesTypes::const_iterator nit = naty.find(cn);
    if (nit == naty.end())
        return -1;

    uint64_t ival = 0;
    switch ((*nit).second) {
    case ibis::BYTE: {
        array_t<signed char> tmp;
        uint64_t irow = 0;
        for (ibis::partList::const_iterator it = parts.begin();
             it != parts.end() && irow < end; ++ it) {
            const ibis::part& dp = **it;
            if (irow + dp.nRows() > begin) {
                const ibis::column* col = dp.getColumn(cn);
                if (col == 0)
                    return -3;
                if (col->getValuesArray(&tmp) < 0)
                    return -4;

                const size_t i0 = (begin > irow ? begin - irow : 0);
                const size_t i1 = (end>=irow+dp.nRows() ? dp.nRows() : end-irow);
                std::copy(tmp.begin()+i0, tmp.begin()+i1, vals+ival);
                ival += i1 - i0;
            }
            irow += dp.nRows();
        }
        break;}
    case ibis::UBYTE: {
        array_t<unsigned char> tmp;
        uint64_t irow = 0;
        for (ibis::partList::const_iterator it = parts.begin();
             it != parts.end() && irow < end; ++ it) {
            const ibis::part& dp = **it;
            if (irow + dp.nRows() > begin) {
                const ibis::column* col = dp.getColumn(cn);
                if (col == 0)
                    return -3;
                if (col->getValuesArray(&tmp) < 0)
                    return -4;

                const size_t i0 = (begin > irow ? begin - irow : 0);
                const size_t i1 = (end>=irow+dp.nRows() ? dp.nRows() : end-irow);
                std::copy(tmp.begin()+i0, tmp.begin()+i1, vals+ival);
                ival += i1 - i0;
            }
            irow += dp.nRows();
        }
        break;}
    case ibis::SHORT: {
        array_t<int16_t> tmp;
        uint64_t irow = 0;
        for (ibis::partList::const_iterator it = parts.begin();
             it != parts.end() && irow < end; ++ it) {
            const ibis::part& dp = **it;
            if (irow + dp.nRows() > begin) {
                const ibis::column* col = dp.getColumn(cn);
                if (col == 0)
                    return -3;
                if (col->getValuesArray(&tmp) < 0)
                    return -4;

                const size_t i0 = (begin > irow ? begin - irow : 0);
                const size_t i1 = (end>=irow+dp.nRows() ? dp.nRows() : end-irow);
                std::copy(tmp.begin()+i0, tmp.begin()+i1, vals+ival);
                ival += i1 - i0;
            }
            irow += dp.nRows();
        }
        break;}
    case ibis::USHORT: {
        array_t<uint16_t> tmp;
        uint64_t irow = 0;
        for (ibis::partList::const_iterator it = parts.begin();
             it != parts.end() && irow < end; ++ it) {
            const ibis::part& dp = **it;
            if (irow + dp.nRows() > begin) {
                const ibis::column* col = dp.getColumn(cn);
                if (col == 0)
                    return -3;
                if (col->getValuesArray(&tmp) < 0)
                    return -4;

                const size_t i0 = (begin > irow ? begin - irow : 0);
                const size_t i1 = (end>=irow+dp.nRows() ? dp.nRows() : end-irow);
                std::copy(tmp.begin()+i0, tmp.begin()+i1, vals+ival);
                ival += i1 - i0;
            }
            irow += dp.nRows();
        }
        break;}
    case ibis::INT:
    case ibis::UINT: {
        array_t<int32_t> tmp;
        uint64_t irow = 0;
        for (ibis::partList::const_iterator it = parts.begin();
             it != parts.end() && irow < end; ++ it) {
            const ibis::part& dp = **it;
            if (irow + dp.nRows() > begin) {
                const ibis::column* col = dp.getColumn(cn);
                if (col == 0)
                    return -3;
                if (col->getValuesArray(&tmp) < 0)
                    return -4;

                const size_t i0 = (begin > irow ? begin - irow : 0);
                const size_t i1 = (end>=irow+dp.nRows() ? dp.nRows() :
                                   end-irow) - i0;
                memcpy(vals+ival, tmp.begin()+i0, i1*4U);
                ival += i1;
            }
            irow += dp.nRows();
        }
        break;}
    default:
        return -2;
    }
    return ival;
} // ibis::mensa::getColumnAsInts

int64_t ibis::mensa::getColumnAsUInts(const char* cn, uint32_t* vals,
                                      uint64_t begin, uint64_t end) const {
    if (end == 0 || end > nrows) end = nrows;
    if (begin >= end) return 0;
    ibis::table::namesTypes::const_iterator nit = naty.find(cn);
    if (nit == naty.end())
        return -1;

    uint64_t ival = 0;
    switch ((*nit).second) {
    case ibis::BYTE:
    case ibis::UBYTE: {
        array_t<unsigned char> tmp;
        uint64_t irow = 0;
        for (ibis::partList::const_iterator it = parts.begin();
             it != parts.end() && irow < end; ++ it) {
            const ibis::part& dp = **it;
            if (irow + dp.nRows() > begin) {
                const ibis::column* col = dp.getColumn(cn);
                if (col == 0)
                    return -3;
                if (col->getValuesArray(&tmp) < 0)
                    return -4;

                const size_t i0 = (begin > irow ? begin - irow : 0);
                const size_t i1 = (end>=irow+dp.nRows() ? dp.nRows() : end-irow);
                std::copy(tmp.begin()+i0, tmp.begin()+i1, vals+ival);
                ival += i1 - i0;
            }
            irow += dp.nRows();
        }
        break;}
    case ibis::SHORT:
    case ibis::USHORT: {
        array_t<uint16_t> tmp;
        uint64_t irow = 0;
        for (ibis::partList::const_iterator it = parts.begin();
             it != parts.end() && irow < end; ++ it) {
            const ibis::part& dp = **it;
            if (irow + dp.nRows() > begin) {
                const ibis::column* col = dp.getColumn(cn);
                if (col == 0)
                    return -3;
                if (col->getValuesArray(&tmp) < 0)
                    return -4;

                const size_t i0 = (begin > irow ? begin - irow : 0);
                const size_t i1 = (end>=irow+dp.nRows() ? dp.nRows() : end-irow);
                std::copy(tmp.begin()+i0, tmp.begin()+i1, vals+ival);
                ival += i1 - i0;
            }
            irow += dp.nRows();
        }
        break;}
    case ibis::INT:
    case ibis::UINT: {
        array_t<uint32_t> tmp;
        uint64_t irow = 0;
        for (ibis::partList::const_iterator it = parts.begin();
             it != parts.end() && irow < end; ++ it) {
            const ibis::part& dp = **it;
            if (irow + dp.nRows() > begin) {
                const ibis::column* col = dp.getColumn(cn);
                if (col == 0)
                    return -3;
                if (col->getValuesArray(&tmp) < 0)
                    return -4;

                const size_t i0 = (begin > irow ? begin - irow : 0);
                const size_t i1 = (end>=irow+dp.nRows() ? dp.nRows() :
                                   end-irow) - i0;
                memcpy(vals+ival, tmp.begin()+i0, i1 * 4U);
                ival += i1;
            }
            irow += dp.nRows();
        }
        break;}
    default:
        return -2;
    }
    return ival;
} // ibis::mensa::getColumnAsUInts

/// @note All integers 4-byte or shorter in length can be safely converted
/// into int64_t.  Values in uint64_t are treated as signed integers, which
/// may create incorrect values.
int64_t ibis::mensa::getColumnAsLongs(const char* cn, int64_t* vals,
                                      uint64_t begin, uint64_t end) const {
    if (end == 0 || end > nrows) end = nrows;
    if (begin >= end) return 0;
    ibis::table::namesTypes::const_iterator nit = naty.find(cn);
    if (nit == naty.end())
        return -1;

    uint64_t ival = 0;
    switch ((*nit).second) {
    case ibis::BYTE: {
        array_t<signed char> tmp;
        uint64_t irow = 0;
        for (ibis::partList::const_iterator it = parts.begin();
             it != parts.end() && irow < end; ++ it) {
            const ibis::part& dp = **it;
            if (irow + dp.nRows() > begin) {
                const ibis::column* col = dp.getColumn(cn);
                if (col == 0)
                    return -3;
                if (col->getValuesArray(&tmp) < 0)
                    return -4;

                const size_t i0 = (begin > irow ? begin - irow : 0);
                const size_t i1 = (end>=irow+dp.nRows() ? dp.nRows() : end-irow);
                std::copy(tmp.begin()+i0, tmp.begin()+i1, vals+ival);
                ival += i1 - i0;
            }
            irow += dp.nRows();
        }
        break;}
    case ibis::UBYTE: {
        array_t<unsigned char> tmp;
        uint64_t irow = 0;
        for (ibis::partList::const_iterator it = parts.begin();
             it != parts.end() && irow < end; ++ it) {
            const ibis::part& dp = **it;
            if (irow + dp.nRows() > begin) {
                const ibis::column* col = dp.getColumn(cn);
                if (col == 0)
                    return -3;
                if (col->getValuesArray(&tmp) < 0)
                    return -4;

                const size_t i0 = (begin > irow ? begin - irow : 0);
                const size_t i1 = (end>=irow+dp.nRows() ? dp.nRows() : end-irow);
                std::copy(tmp.begin()+i0, tmp.begin()+i1, vals+ival);
                ival += i1 - i0;
            }
            irow += dp.nRows();
        }
        break;}
    case ibis::SHORT: {
        array_t<int16_t> tmp;
        uint64_t irow = 0;
        for (ibis::partList::const_iterator it = parts.begin();
             it != parts.end() && irow < end; ++ it) {
            const ibis::part& dp = **it;
            if (irow + dp.nRows() > begin) {
                const ibis::column* col = dp.getColumn(cn);
                if (col == 0)
                    return -3;
                if (col->getValuesArray(&tmp) < 0)
                    return -4;

                const size_t i0 = (begin > irow ? begin - irow : 0);
                const size_t i1 = (end>=irow+dp.nRows() ? dp.nRows() : end-irow);
                std::copy(tmp.begin()+i0, tmp.begin()+i1, vals+ival);
                ival += i1 - i0;
            }
            irow += dp.nRows();
        }
        break;}
    case ibis::USHORT: {
        array_t<uint16_t> tmp;
        uint64_t irow = 0;
        for (ibis::partList::const_iterator it = parts.begin();
             it != parts.end() && irow < end; ++ it) {
            const ibis::part& dp = **it;
            if (irow + dp.nRows() > begin) {
                const ibis::column* col = dp.getColumn(cn);
                if (col == 0)
                    return -3;
                if (col->getValuesArray(&tmp) < 0)
                    return -4;

                const size_t i0 = (begin > irow ? begin - irow : 0);
                const size_t i1 = (end>=irow+dp.nRows() ? dp.nRows() : end-irow);
                std::copy(tmp.begin()+i0, tmp.begin()+i1, vals+ival);
                ival += i1 - i0;
            }
            irow += dp.nRows();
        }
        break;}
    case ibis::INT: {
        array_t<int32_t> tmp;
        uint64_t irow = 0;
        for (ibis::partList::const_iterator it = parts.begin();
             it != parts.end() && irow < end; ++ it) {
            const ibis::part& dp = **it;
            if (irow + dp.nRows() > begin) {
                const ibis::column* col = dp.getColumn(cn);
                if (col == 0)
                    return -3;
                if (col->getValuesArray(&tmp) < 0)
                    return -4;

                const size_t i0 = (begin > irow ? begin - irow : 0);
                const size_t i1 = (end>=irow+dp.nRows() ? dp.nRows() : end-irow);
                std::copy(tmp.begin()+i0, tmp.begin()+i1, vals+ival);
                ival += i1 - i0;
            }
            irow += dp.nRows();
        }
        break;}
    case ibis::UINT: {
        array_t<uint32_t> tmp;
        uint64_t irow = 0;
        for (ibis::partList::const_iterator it = parts.begin();
             it != parts.end() && irow < end; ++ it) {
            const ibis::part& dp = **it;
            if (irow + dp.nRows() > begin) {
                const ibis::column* col = dp.getColumn(cn);
                if (col == 0)
                    return -3;
                if (col->getValuesArray(&tmp) < 0)
                    return -4;

                const size_t i0 = (begin > irow ? begin - irow : 0);
                const size_t i1 = (end>=irow+dp.nRows() ? dp.nRows() : end-irow);
                std::copy(tmp.begin()+i0, tmp.begin()+i1, vals+ival);
                ival += i1 - i0;
            }
            irow += dp.nRows();
        }
        break;}
    case ibis::OID:
    case ibis::LONG:
    case ibis::ULONG: {
        array_t<int64_t> tmp;
        uint64_t irow = 0;
        for (ibis::partList::const_iterator it = parts.begin();
             it != parts.end() && irow < end; ++ it) {
            const ibis::part& dp = **it;
            if (irow + dp.nRows() > begin) {
                const ibis::column* col = dp.getColumn(cn);
                if (col == 0)
                    return -3;
                if (col->getValuesArray(&tmp) < 0)
                    return -4;

                const size_t i0 = (begin > irow ? begin - irow : 0);
                const size_t i1 = (end>=irow+dp.nRows() ? dp.nRows() :
                                   end-irow) - i0;
                memcpy(vals+ival, tmp.begin()+i0, i1*8U);
                ival += i1;
            }
            irow += dp.nRows();
        }
        break;}
    default:
        return -2;
    }
    return ival;
} // ibis::mensa::getColumnAsLongs

/// @note All integers can be converted to uint64_t, however, negative
/// integers will be treated as unsigned integers.
int64_t ibis::mensa::getColumnAsULongs(const char* cn, uint64_t* vals,
                                       uint64_t begin, uint64_t end) const {
    if (end == 0 || end > nrows) end = nrows;
    if (begin >= end) return 0;
    ibis::table::namesTypes::const_iterator nit = naty.find(cn);
    if (nit == naty.end())
        return -1;

    uint64_t ival = 0;
    switch ((*nit).second) {
    case ibis::BYTE:
    case ibis::UBYTE: {
        array_t<unsigned char> tmp;
        uint64_t irow = 0;
        for (ibis::partList::const_iterator it = parts.begin();
             it != parts.end() && irow < end; ++ it) {
            const ibis::part& dp = **it;
            if (irow + dp.nRows() > begin) {
                const ibis::column* col = dp.getColumn(cn);
                if (col == 0)
                    return -3;
                if (col->getValuesArray(&tmp) < 0)
                    return -4;

                const size_t i0 = (begin > irow ? begin - irow : 0);
                const size_t i1 = (end>=irow+dp.nRows() ? dp.nRows() : end-irow);
                std::copy(tmp.begin()+i0, tmp.begin()+i1, vals+ival);
                ival += i1 - i0;
            }
            irow += dp.nRows();
        }
        break;}
    case ibis::SHORT:
    case ibis::USHORT: {
        array_t<uint16_t> tmp;
        uint64_t irow = 0;
        for (ibis::partList::const_iterator it = parts.begin();
             it != parts.end() && irow < end; ++ it) {
            const ibis::part& dp = **it;
            if (irow + dp.nRows() > begin) {
                const ibis::column* col = dp.getColumn(cn);
                if (col == 0)
                    return -3;
                if (col->getValuesArray(&tmp) < 0)
                    return -4;

                const size_t i0 = (begin > irow ? begin - irow : 0);
                const size_t i1 = (end>=irow+dp.nRows() ? dp.nRows() : end-irow);
                std::copy(tmp.begin()+i0, tmp.begin()+i1, vals+ival);
                ival += i1 - i0;
            }
            irow += dp.nRows();
        }
        break;}
    case ibis::INT:
    case ibis::UINT: {
        array_t<uint32_t> tmp;
        uint64_t irow = 0;
        for (ibis::partList::const_iterator it = parts.begin();
             it != parts.end() && irow < end; ++ it) {
            const ibis::part& dp = **it;
            if (irow + dp.nRows() > begin) {
                const ibis::column* col = dp.getColumn(cn);
                if (col == 0)
                    return -3;
                if (col->getValuesArray(&tmp) < 0)
                    return -4;

                const size_t i0 = (begin > irow ? begin - irow : 0);
                const size_t i1 = (end>=irow+dp.nRows() ? dp.nRows() : end-irow);
                std::copy(tmp.begin()+i0, tmp.begin()+i1, vals+ival);
                ival += i1 - i0;
            }
            irow += dp.nRows();
        }
        break;}
    case ibis::OID:
    case ibis::LONG:
    case ibis::ULONG: {
        array_t<uint64_t> tmp;
        uint64_t irow = 0;
        for (ibis::partList::const_iterator it = parts.begin();
             it != parts.end() && irow < end; ++ it) {
            const ibis::part& dp = **it;
            if (irow + dp.nRows() > begin) {
                const ibis::column* col = dp.getColumn(cn);
                if (col == 0)
                    return -3;
                if (col->getValuesArray(&tmp) < 0)
                    return -4;

                const size_t i0 = (begin > irow ? begin - irow : 0);
                const size_t i1 = (end>=irow+dp.nRows() ? dp.nRows() :
                                   end-irow) - i0;
                memcpy(vals+ival, tmp.begin()+i0, i1*8U);
                ival += i1;
            }
            irow += dp.nRows();
        }
        break;}
    default:
        return -2;
    }
    return ival;
} // ibis::mensa::getColumnAsULongs

/// @note Integers two-byte or less in length can be converted safely to
/// floats.
int64_t ibis::mensa::getColumnAsFloats(const char* cn, float* vals,
                                       uint64_t begin, uint64_t end) const {
    if (end == 0 || end > nrows) end = nrows;
    if (begin >= end) return 0;
    ibis::table::namesTypes::const_iterator nit = naty.find(cn);
    if (nit == naty.end())
        return -1;

    uint64_t ival = 0;
    switch ((*nit).second) {
    case ibis::BYTE: {
        array_t<signed char> tmp;
        uint64_t irow = 0;
        for (ibis::partList::const_iterator it = parts.begin();
             it != parts.end() && irow < end; ++ it) {
            const ibis::part& dp = **it;
            if (irow + dp.nRows() > begin) {
                const ibis::column* col = dp.getColumn(cn);
                if (col == 0)
                    return -3;
                if (col->getValuesArray(&tmp) < 0)
                    return -4;

                const size_t i0 = (begin > irow ? begin - irow : 0);
                const size_t i1 = (end>=irow+dp.nRows() ? dp.nRows() : end-irow);
                std::copy(tmp.begin()+i0, tmp.begin()+i1, vals+ival);
                ival += i1 - i0;
            }
            irow += dp.nRows();
        }
        break;}
    case ibis::UBYTE: {
        array_t<unsigned char> tmp;
        uint64_t irow = 0;
        for (ibis::partList::const_iterator it = parts.begin();
             it != parts.end() && irow < end; ++ it) {
            const ibis::part& dp = **it;
            if (irow + dp.nRows() > begin) {
                const ibis::column* col = dp.getColumn(cn);
                if (col == 0)
                    return -3;
                if (col->getValuesArray(&tmp) < 0)
                    return -4;

                const size_t i0 = (begin > irow ? begin - irow : 0);
                const size_t i1 = (end>=irow+dp.nRows() ? dp.nRows() : end-irow);
                std::copy(tmp.begin()+i0, tmp.begin()+i1, vals+ival);
                ival += i1 - i0;
            }
            irow += dp.nRows();
        }
        break;}
    case ibis::SHORT: {
        array_t<int16_t> tmp;
        uint64_t irow = 0;
        for (ibis::partList::const_iterator it = parts.begin();
             it != parts.end() && irow < end; ++ it) {
            const ibis::part& dp = **it;
            if (irow + dp.nRows() > begin) {
                const ibis::column* col = dp.getColumn(cn);
                if (col == 0)
                    return -3;
                if (col->getValuesArray(&tmp) < 0)
                    return -4;

                const size_t i0 = (begin > irow ? begin - irow : 0);
                const size_t i1 = (end>=irow+dp.nRows() ? dp.nRows() : end-irow);
                std::copy(tmp.begin()+i0, tmp.begin()+i1, vals+ival);
                ival += i1 - i0;
            }
            irow += dp.nRows();
        }
        break;}
    case ibis::USHORT: {
        array_t<uint16_t> tmp;
        uint64_t irow = 0;
        for (ibis::partList::const_iterator it = parts.begin();
             it != parts.end() && irow < end; ++ it) {
            const ibis::part& dp = **it;
            if (irow + dp.nRows() > begin) {
                const ibis::column* col = dp.getColumn(cn);
                if (col == 0)
                    return -3;
                if (col->getValuesArray(&tmp) < 0)
                    return -4;

                const size_t i0 = (begin > irow ? begin - irow : 0);
                const size_t i1 = (end>=irow+dp.nRows() ? dp.nRows() : end-irow);
                std::copy(tmp.begin()+i0, tmp.begin()+i1, vals+ival);
                ival += i1 - i0;
            }
            irow += dp.nRows();
        }
        break;}
    case ibis::FLOAT: {
        array_t<float> tmp;
        uint64_t irow = 0;
        for (ibis::partList::const_iterator it = parts.begin();
             it != parts.end() && irow < end; ++ it) {
            const ibis::part& dp = **it;
            if (irow + dp.nRows() > begin) {
                const ibis::column* col = dp.getColumn(cn);
                if (col == 0)
                    return -3;
                if (col->getValuesArray(&tmp) < 0)
                    return -4;

                const size_t i0 = (begin > irow ? begin - irow : 0);
                const size_t i1 = (end>=irow+dp.nRows() ? dp.nRows() :
                                   end-irow) - i0;
                memcpy(vals+ival, tmp.begin()+i0, i1*4U);
                ival += i1;
            }
            irow += dp.nRows();
        }
        break;}
    default:
        return -2;
    }
    return ival;
} // ibis::mensa::getColumnAsFloats

/// @note Integers four-byte or less in length can be converted to double
/// safely.  Float values may also be converted into doubles.
int64_t ibis::mensa::getColumnAsDoubles(const char* cn, double* vals,
                                        uint64_t begin, uint64_t end) const {
    if (end == 0 || end > nrows) end = nrows;
    if (begin >= end) return 0;
    ibis::table::namesTypes::const_iterator nit = naty.find(cn);
    if (nit == naty.end())
        return -1;

    uint64_t ival = 0;
    switch ((*nit).second) {
    case ibis::BYTE: {
        array_t<signed char> tmp;
        uint64_t irow = 0;
        for (ibis::partList::const_iterator it = parts.begin();
             it != parts.end() && irow < end; ++ it) {
            const ibis::part& dp = **it;
            if (irow + dp.nRows() > begin) {
                const ibis::column* col = dp.getColumn(cn);
                if (col == 0)
                    return -3;
                if (col->getValuesArray(&tmp) < 0)
                    return -4;

                const size_t i0 = (begin > irow ? begin - irow : 0);
                const size_t i1 = (end>=irow+dp.nRows() ? dp.nRows() : end-irow);
                std::copy(tmp.begin()+i0, tmp.begin()+i1, vals+ival);
                ival += i1 - i0;
            }
            irow += dp.nRows();
        }
        break;}
    case ibis::UBYTE: {
        array_t<unsigned char> tmp;
        uint64_t irow = 0;
        for (ibis::partList::const_iterator it = parts.begin();
             it != parts.end() && irow < end; ++ it) {
            const ibis::part& dp = **it;
            if (irow + dp.nRows() > begin) {
                const ibis::column* col = dp.getColumn(cn);
                if (col == 0)
                    return -3;
                if (col->getValuesArray(&tmp) < 0)
                    return -4;

                const size_t i0 = (begin > irow ? begin - irow : 0);
                const size_t i1 = (end>=irow+dp.nRows() ? dp.nRows() : end-irow);
                std::copy(tmp.begin()+i0, tmp.begin()+i1, vals+ival);
                ival += i1 - i0;
            }
            irow += dp.nRows();
        }
        break;}
    case ibis::SHORT: {
        array_t<int16_t> tmp;
        uint64_t irow = 0;
        for (ibis::partList::const_iterator it = parts.begin();
             it != parts.end() && irow < end; ++ it) {
            const ibis::part& dp = **it;
            if (irow + dp.nRows() > begin) {
                const ibis::column* col = dp.getColumn(cn);
                if (col == 0)
                    return -3;
                if (col->getValuesArray(&tmp) < 0)
                    return -4;

                const size_t i0 = (begin > irow ? begin - irow : 0);
                const size_t i1 = (end>=irow+dp.nRows() ? dp.nRows() : end-irow);
                std::copy(tmp.begin()+i0, tmp.begin()+i1, vals+ival);
                ival += i1 - i0;
            }
            irow += dp.nRows();
        }
        break;}
    case ibis::USHORT: {
        array_t<uint16_t> tmp;
        uint64_t irow = 0;
        for (ibis::partList::const_iterator it = parts.begin();
             it != parts.end() && irow < end; ++ it) {
            const ibis::part& dp = **it;
            if (irow + dp.nRows() > begin) {
                const ibis::column* col = dp.getColumn(cn);
                if (col == 0)
                    return -3;
                if (col->getValuesArray(&tmp) < 0)
                    return -4;

                const size_t i0 = (begin > irow ? begin - irow : 0);
                const size_t i1 = (end>=irow+dp.nRows() ? dp.nRows() : end-irow);
                std::copy(tmp.begin()+i0, tmp.begin()+i1, vals+ival);
                ival += i1 - i0;
            }
            irow += dp.nRows();
        }
        break;}
    case ibis::INT: {
        array_t<int32_t> tmp;
        uint64_t irow = 0;
        for (ibis::partList::const_iterator it = parts.begin();
             it != parts.end() && irow < end; ++ it) {
            const ibis::part& dp = **it;
            if (irow + dp.nRows() > begin) {
                const ibis::column* col = dp.getColumn(cn);
                if (col == 0)
                    return -3;
                if (col->getValuesArray(&tmp) < 0)
                    return -4;

                const size_t i0 = (begin > irow ? begin - irow : 0);
                const size_t i1 = (end>=irow+dp.nRows() ? dp.nRows() : end-irow);
                std::copy(tmp.begin()+i0, tmp.begin()+i1, vals+ival);
                ival += i1 - i0;
            }
            irow += dp.nRows();
        }
        break;}
    case ibis::UINT: {
        array_t<uint32_t> tmp;
        uint64_t irow = 0;
        for (ibis::partList::const_iterator it = parts.begin();
             it != parts.end() && irow < end; ++ it) {
            const ibis::part& dp = **it;
            if (irow + dp.nRows() > begin) {
                const ibis::column* col = dp.getColumn(cn);
                if (col == 0)
                    return -3;
                if (col->getValuesArray(&tmp) < 0)
                    return -4;

                const size_t i0 = (begin > irow ? begin - irow : 0);
                const size_t i1 = (end>=irow+dp.nRows() ? dp.nRows() : end-irow);
                std::copy(tmp.begin()+i0, tmp.begin()+i1, vals+ival);
                ival += i1 - i0;
            }
            irow += dp.nRows();
        }
        break;}
    case ibis::FLOAT: {
        array_t<float> tmp;
        uint64_t irow = 0;
        for (ibis::partList::const_iterator it = parts.begin();
             it != parts.end() && irow < end; ++ it) {
            const ibis::part& dp = **it;
            if (irow + dp.nRows() > begin) {
                const ibis::column* col = dp.getColumn(cn);
                if (col == 0)
                    return -3;
                if (col->getValuesArray(&tmp) < 0)
                    return -4;

                const size_t i0 = (begin > irow ? begin - irow : 0);
                const size_t i1 = (end>=irow+dp.nRows() ? dp.nRows() : end-irow);
                std::copy(tmp.begin()+i0, tmp.begin()+i1, vals+ival);
                ival += i1 - i0;
            }
            irow += dp.nRows();
        }
        break;}
    case ibis::DOUBLE: {
        array_t<double> tmp;
        uint64_t irow = 0;
        for (ibis::partList::const_iterator it = parts.begin();
             it != parts.end() && irow < end; ++ it) {
            const ibis::part& dp = **it;
            if (irow + dp.nRows() > begin) {
                const ibis::column* col = dp.getColumn(cn);
                if (col == 0)
                    return -3;
                if (col->getValuesArray(&tmp) < 0)
                    return -4;

                const size_t i0 = (begin > irow ? begin - irow : 0);
                const size_t i1 = (end>=irow+dp.nRows() ? dp.nRows() :
                                   end-irow) - i0;
                memcpy(vals+ival, tmp.begin()+i0, i1*8);
                ival += i1;
            }
            irow += dp.nRows();
        }
        break;}
    default:
        return -2;
    }
    return ival;
} // ibis::mensa::getColumnAsDoubles

int64_t ibis::mensa::getColumnAsDoubles(const char* cn,
                                        std::vector<double>& vals,
                                        uint64_t begin, uint64_t end) const {
    if (end == 0 || end > nrows) end = nrows;
    if (begin >= end) return 0;
    ibis::table::namesTypes::const_iterator nit = naty.find(cn);
    if (nit == naty.end())
        return -1;
    try {
        vals.resize(end-begin);
    }
    catch (...) {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- mensa::getColumnAsDoubles failed to "
            "allocate space for the output std::vector<double>("
            << end-begin << ')';
        vals.clear();
        return -5;
    }

    uint64_t ival = 0;
    switch ((*nit).second) {
    case ibis::BYTE: {
        array_t<signed char> tmp;
        uint64_t irow = 0;
        for (ibis::partList::const_iterator it = parts.begin();
             it != parts.end() && irow < end; ++ it) {
            const ibis::part& dp = **it;
            if (irow + dp.nRows() > begin) {
                const ibis::column* col = dp.getColumn(cn);
                if (col == 0)
                    return -3;
                if (col->getValuesArray(&tmp) < 0)
                    return -4;

                size_t i0 = (begin > irow ? begin - irow : 0);
                const size_t i1 = (end>=irow+dp.nRows() ? dp.nRows() :
                                   end-irow);
                while (i0 < i1) {
                    vals[ival] = tmp[i0];
                    ++ ival;
                    ++ i0;
                }
            }
            irow += dp.nRows();
        }
        break;}
    case ibis::UBYTE: {
        array_t<unsigned char> tmp;
        uint64_t irow = 0;
        for (ibis::partList::const_iterator it = parts.begin();
             it != parts.end() && irow < end; ++ it) {
            const ibis::part& dp = **it;
            if (irow + dp.nRows() > begin) {
                const ibis::column* col = dp.getColumn(cn);
                if (col == 0)
                    return -3;
                if (col->getValuesArray(&tmp) < 0)
                    return -4;

                size_t i0 = (begin > irow ? begin - irow : 0);
                const size_t i1 = (end>=irow+dp.nRows() ? dp.nRows() :
                                   end-irow);
                while (i0 < i1) {
                    vals[ival] = tmp[i0];
                    ++ ival;
                    ++ i0;
                }
            }
            irow += dp.nRows();
        }
        break;}
    case ibis::SHORT: {
        array_t<int16_t> tmp;
        uint64_t irow = 0;
        for (ibis::partList::const_iterator it = parts.begin();
             it != parts.end() && irow < end; ++ it) {
            const ibis::part& dp = **it;
            if (irow + dp.nRows() > begin) {
                const ibis::column* col = dp.getColumn(cn);
                if (col == 0)
                    return -3;
                if (col->getValuesArray(&tmp) < 0)
                    return -4;

                size_t i0 = (begin > irow ? begin - irow : 0);
                const size_t i1 = (end>=irow+dp.nRows() ? dp.nRows() :
                                   end-irow);
                while (i0 < i1) {
                    vals[ival] = tmp[i0];
                    ++ ival;
                    ++ i0;
                }
            }
            irow += dp.nRows();
        }
        break;}
    case ibis::USHORT: {
        array_t<uint16_t> tmp;
        uint64_t irow = 0;
        for (ibis::partList::const_iterator it = parts.begin();
             it != parts.end() && irow < end; ++ it) {
            const ibis::part& dp = **it;
            if (irow + dp.nRows() > begin) {
                const ibis::column* col = dp.getColumn(cn);
                if (col == 0)
                    return -3;
                if (col->getValuesArray(&tmp) < 0)
                    return -4;

                size_t i0 = (begin > irow ? begin - irow : 0);
                const size_t i1 = (end>=irow+dp.nRows() ? dp.nRows() :
                                   end-irow);
                while (i0 < i1) {
                    vals[ival] = tmp[i0];
                    ++ ival;
                    ++ i0;
                }
            }
            irow += dp.nRows();
        }
        break;}
    case ibis::INT: {
        array_t<int32_t> tmp;
        uint64_t irow = 0;
        for (ibis::partList::const_iterator it = parts.begin();
             it != parts.end() && irow < end; ++ it) {
            const ibis::part& dp = **it;
            if (irow + dp.nRows() > begin) {
                const ibis::column* col = dp.getColumn(cn);
                if (col == 0)
                    return -3;
                if (col->getValuesArray(&tmp) < 0)
                    return -4;

                size_t i0 = (begin > irow ? begin - irow : 0);
                const size_t i1 = (end>=irow+dp.nRows() ? dp.nRows() :
                                   end-irow);
                while (i0 < i1) {
                    vals[ival] = tmp[i0];
                    ++ ival;
                    ++ i0;
                }
            }
            irow += dp.nRows();
        }
        break;}
    case ibis::UINT: {
        array_t<uint32_t> tmp;
        uint64_t irow = 0;
        for (ibis::partList::const_iterator it = parts.begin();
             it != parts.end() && irow < end; ++ it) {
            const ibis::part& dp = **it;
            if (irow + dp.nRows() > begin) {
                const ibis::column* col = dp.getColumn(cn);
                if (col == 0)
                    return -3;
                if (col->getValuesArray(&tmp) < 0)
                    return -4;

                size_t i0 = (begin > irow ? begin - irow : 0);
                const size_t i1 = (end>=irow+dp.nRows() ? dp.nRows() :
                                   end-irow);
                while (i0 < i1) {
                    vals[ival] = tmp[i0];
                    ++ ival;
                    ++ i0;
                }
            }
            irow += dp.nRows();
        }
        break;}
    case ibis::FLOAT: {
        array_t<float> tmp;
        uint64_t irow = 0;
        for (ibis::partList::const_iterator it = parts.begin();
             it != parts.end() && irow < end; ++ it) {
            const ibis::part& dp = **it;
            if (irow + dp.nRows() > begin) {
                const ibis::column* col = dp.getColumn(cn);
                if (col == 0)
                    return -3;
                if (col->getValuesArray(&tmp) < 0)
                    return -4;

                size_t i0 = (begin > irow ? begin - irow : 0);
                const size_t i1 = (end>=irow+dp.nRows() ? dp.nRows() :
                                   end-irow);
                while (i0 < i1) {
                    vals[ival] = tmp[i0];
                    ++ ival;
                    ++ i0;
                }
            }
            irow += dp.nRows();
        }
        break;}
    case ibis::DOUBLE: {
        array_t<double> tmp;
        uint64_t irow = 0;
        for (ibis::partList::const_iterator it = parts.begin();
             it != parts.end() && irow < end; ++ it) {
            const ibis::part& dp = **it;
            if (irow + dp.nRows() > begin) {
                const ibis::column* col = dp.getColumn(cn);
                if (col == 0)
                    return -3;
                if (col->getValuesArray(&tmp) < 0)
                    return -4;

                size_t i0 = (begin > irow ? begin - irow : 0);
                const size_t i1 = (end>=irow+dp.nRows() ? dp.nRows() :
                                   end-irow);
                while (i0 < i1) {
                    vals[ival] = tmp[i0];
                    ++ ival;
                    ++ i0;
                }
            }
            irow += dp.nRows();
        }
        break;}
    default:
        return -2;
    }
    return ival;
} // ibis::mensa::getColumnAsDoubles

/// Convert values to their string form.  Many data types can be converted
/// to strings, however, the conversion may take a significant amount of
/// time.
int64_t ibis::mensa::getColumnAsStrings(const char* cn,
                                        std::vector<std::string>& vals,
                                        uint64_t begin, uint64_t end) const {
    if (end == 0 || end > nrows) end = nrows;
    if (begin >= end) return 0;
    ibis::table::namesTypes::const_iterator nit = naty.find(cn);
    if (nit == naty.end())
        return -1;
    try {
        vals.resize(end-begin);
    }
    catch (...) {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- mensa::getColumnAsStrings failed to "
            "allocate space for the output std::vector<std::string>("
            << end-begin << ')';
        vals.clear();
        return -5;
    }

    uint64_t ival = 0;
    switch ((*nit).second) {
    case ibis::BYTE: {
        array_t<signed char> tmp;
        uint64_t irow = 0;
        for (ibis::partList::const_iterator it = parts.begin();
             it != parts.end() && irow < end; ++ it) {
            const ibis::part& dp = **it;
            if (irow + dp.nRows() > begin) {
                const ibis::column* col = dp.getColumn(cn);
                if (col == 0)
                    return -3;
                if (col->getValuesArray(&tmp) < 0)
                    return -4;

                size_t i0 = (begin > irow ? begin - irow : 0);
                const size_t i1 = (end>=irow+dp.nRows() ? dp.nRows() :
                                   end-irow);
                while (i0 < i1) {
                    std::ostringstream oss;
                    oss << static_cast<int>(tmp[i0]);
                    vals[ival] = oss.str();
                    ++ ival;
                    ++ i0;
                }
            }
            irow += dp.nRows();
        }
        break;}
    case ibis::UBYTE: {
        array_t<unsigned char> tmp;
        uint64_t irow = 0;
        for (ibis::partList::const_iterator it = parts.begin();
             it != parts.end() && irow < end; ++ it) {
            const ibis::part& dp = **it;
            if (irow + dp.nRows() > begin) {
                const ibis::column* col = dp.getColumn(cn);
                if (col == 0)
                    return -3;
                if (col->getValuesArray(&tmp) < 0)
                    return -4;

                size_t i0 = (begin > irow ? begin - irow : 0);
                const size_t i1 = (end>=irow+dp.nRows() ? dp.nRows() :
                                   end-irow);
                while (i0 < i1) {
                    std::ostringstream oss;
                    oss << static_cast<int>(tmp[i0]);
                    vals[ival] = oss.str();
                    ++ ival;
                    ++ i0;
                }
            }
            irow += dp.nRows();
        }
        break;}
    case ibis::SHORT: {
        array_t<int16_t> tmp;
        uint64_t irow = 0;
        for (ibis::partList::const_iterator it = parts.begin();
             it != parts.end() && irow < end; ++ it) {
            const ibis::part& dp = **it;
            if (irow + dp.nRows() > begin) {
                const ibis::column* col = dp.getColumn(cn);
                if (col == 0)
                    return -3;
                if (col->getValuesArray(&tmp) < 0)
                    return -4;

                size_t i0 = (begin > irow ? begin - irow : 0);
                const size_t i1 = (end>=irow+dp.nRows() ? dp.nRows() :
                                   end-irow);
                while (i0 < i1) {
                    std::ostringstream oss;
                    oss << tmp[i0];
                    vals[ival] = oss.str();
                    ++ ival;
                    ++ i0;
                }
            }
            irow += dp.nRows();
        }
        break;}
    case ibis::USHORT: {
        array_t<uint16_t> tmp;
        uint64_t irow = 0;
        for (ibis::partList::const_iterator it = parts.begin();
             it != parts.end() && irow < end; ++ it) {
            const ibis::part& dp = **it;
            if (irow + dp.nRows() > begin) {
                const ibis::column* col = dp.getColumn(cn);
                if (col == 0)
                    return -3;
                if (col->getValuesArray(&tmp) < 0)
                    return -4;

                size_t i0 = (begin > irow ? begin - irow : 0);
                const size_t i1 = (end>=irow+dp.nRows() ? dp.nRows() :
                                   end-irow);
                while (i0 < i1) {
                    std::ostringstream oss;
                    oss << tmp[i0];
                    vals[ival] = oss.str();
                    ++ ival;
                    ++ i0;
                }
            }
            irow += dp.nRows();
        }
        break;}
    case ibis::INT: {
        array_t<int32_t> tmp;
        uint64_t irow = 0;
        for (ibis::partList::const_iterator it = parts.begin();
             it != parts.end() && irow < end; ++ it) {
            const ibis::part& dp = **it;
            if (irow + dp.nRows() > begin) {
                const ibis::column* col = dp.getColumn(cn);
                if (col == 0)
                    return -3;
                if (col->getValuesArray(&tmp) < 0)
                    return -4;

                size_t i0 = (begin > irow ? begin - irow : 0);
                const size_t i1 = (end>=irow+dp.nRows() ? dp.nRows() :
                                   end-irow);
                while (i0 < i1) {
                    std::ostringstream oss;
                    oss << tmp[i0];
                    vals[ival] = oss.str();
                    ++ ival;
                    ++ i0;
                }
            }
            irow += dp.nRows();
        }
        break;}
    case ibis::UINT: {
        array_t<uint32_t> tmp;
        uint64_t irow = 0;
        for (ibis::partList::const_iterator it = parts.begin();
             it != parts.end() && irow < end; ++ it) {
            const ibis::part& dp = **it;
            if (irow + dp.nRows() > begin) {
                const ibis::column* col = dp.getColumn(cn);
                if (col == 0)
                    return -3;
                if (col->getValuesArray(&tmp) < 0)
                    return -4;

                size_t i0 = (begin > irow ? begin - irow : 0);
                const size_t i1 = (end>=irow+dp.nRows() ? dp.nRows() :
                                   end-irow);
                while (i0 < i1) {
                    std::ostringstream oss;
                    oss << tmp[i0];
                    vals[ival] = oss.str();
                    ++ ival;
                    ++ i0;
                }
            }
            irow += dp.nRows();
        }
        break;}
    case ibis::LONG: {
        array_t<int64_t> tmp;
        uint64_t irow = 0;
        for (ibis::partList::const_iterator it = parts.begin();
             it != parts.end() && irow < end; ++ it) {
            const ibis::part& dp = **it;
            if (irow + dp.nRows() > begin) {
                const ibis::column* col = dp.getColumn(cn);
                if (col == 0)
                    return -3;
                if (col->getValuesArray(&tmp) < 0)
                    return -4;

                size_t i0 = (begin > irow ? begin - irow : 0);
                const size_t i1 = (end>=irow+dp.nRows() ? dp.nRows() :
                                   end-irow);
                while (i0 < i1) {
                    std::ostringstream oss;
                    oss << tmp[i0];
                    vals[ival] = oss.str();
                    ++ ival;
                    ++ i0;
                }
            }
            irow += dp.nRows();
        }
        break;}
    case ibis::OID:
    case ibis::ULONG: {
        array_t<uint64_t> tmp;
        uint64_t irow = 0;
        for (ibis::partList::const_iterator it = parts.begin();
             it != parts.end() && irow < end; ++ it) {
            const ibis::part& dp = **it;
            if (irow + dp.nRows() > begin) {
                const ibis::column* col = dp.getColumn(cn);
                if (col == 0)
                    return -3;
                if (col->getValuesArray(&tmp) < 0)
                    return -4;

                size_t i0 = (begin > irow ? begin - irow : 0);
                const size_t i1 = (end>=irow+dp.nRows() ? dp.nRows() :
                                   end-irow);
                while (i0 < i1) {
                    std::ostringstream oss;
                    oss << tmp[i0];
                    vals[ival] = oss.str();
                    ++ ival;
                    ++ i0;
                }
            }
            irow += dp.nRows();
        }
        break;}
    case ibis::FLOAT: {
        array_t<float> tmp;
        uint64_t irow = 0;
        for (ibis::partList::const_iterator it = parts.begin();
             it != parts.end() && irow < end; ++ it) {
            const ibis::part& dp = **it;
            if (irow + dp.nRows() > begin) {
                const ibis::column* col = dp.getColumn(cn);
                if (col == 0)
                    return -3;
                if (col->getValuesArray(&tmp) < 0)
                    return -4;

                size_t i0 = (begin > irow ? begin - irow : 0);
                const size_t i1 = (end>=irow+dp.nRows() ? dp.nRows() :
                                   end-irow);
                while (i0 < i1) {
                    std::ostringstream oss;
                    oss << tmp[i0];
                    vals[ival] = oss.str();
                    ++ ival;
                    ++ i0;
                }
            }
            irow += dp.nRows();
        }
        break;}
    case ibis::DOUBLE: {
        array_t<double> tmp;
        uint64_t irow = 0;
        for (ibis::partList::const_iterator it = parts.begin();
             it != parts.end() && irow < end; ++ it) {
            const ibis::part& dp = **it;
            if (irow + dp.nRows() > begin) {
                const ibis::column* col = dp.getColumn(cn);
                if (col == 0)
                    return -3;
                if (col->getValuesArray(&tmp) < 0)
                    return -4;

                size_t i0 = (begin > irow ? begin - irow : 0);
                const size_t i1 = (end>=irow+dp.nRows() ? dp.nRows() :
                                   end-irow);
                while (i0 < i1) {
                    std::ostringstream oss;
                    oss << tmp[i0];
                    vals[ival] = oss.str();
                    ++ ival;
                    ++ i0;
                }
            }
            irow += dp.nRows();
        }
        break;}
    case ibis::CATEGORY:
    case ibis::TEXT: {
        std::string tmp;
        uint64_t irow = 0;
        for (ibis::partList::const_iterator it = parts.begin();
             it != parts.end() && irow < end; ++ it) {
            const ibis::part& dp = **it;
            if (irow + dp.nRows() > begin) {
                const ibis::column* col = dp.getColumn(cn);
                if (col == 0)
                    return -3;

                size_t i0 = (begin > irow ? begin - irow : 0);
                const size_t i1 = (end>=irow+dp.nRows() ? dp.nRows() :
                                   end-irow);
                while (i0 < i1) {
                    static_cast<const ibis::text*>(col)->getString(i0, tmp);
                    vals[ival] = tmp;
                    ++ ival;
                    ++ i0;
                }
            }
            irow += dp.nRows();
        }
        break;}
    default:
        return -2;
    }
    return ival;
} // ibis::mensa::getColumnAsStrings

int64_t ibis::mensa::getColumnAsOpaques(const char* cn,
                                        std::vector<ibis::opaque>& vals,
                                        uint64_t begin, uint64_t end) const {
    if (end == 0 || end > nrows) end = nrows;
    if (begin >= end) return 0;
    ibis::table::namesTypes::const_iterator nit = naty.find(cn);
    if (nit == naty.end())
        return -1;
    try {
        vals.resize(end-begin);
    }
    catch (...) {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- mensa::getColumnAsOpaques failed to "
            "allocate space for the output std::vector<ibis::opaque>("
            << end-begin << ')';
        vals.clear();
        return -5;
    }

    uint64_t ival = 0;
    switch ((*nit).second) {
    case ibis::BYTE: {
        array_t<signed char> tmp;
        uint64_t irow = 0;
        for (ibis::partList::const_iterator it = parts.begin();
             it != parts.end() && irow < end; ++ it) {
            const ibis::part& dp = **it;
            if (irow + dp.nRows() > begin) {
                const ibis::column* col = dp.getColumn(cn);
                if (col == 0)
                    return -3;
                if (col->getValuesArray(&tmp) < 0)
                    return -4;

                size_t i0 = (begin > irow ? begin - irow : 0);
                const size_t i1 = (end>=irow+dp.nRows() ? dp.nRows() :
                                   end-irow);
                while (i0 < i1) {
                    vals[ival].copy((const char*)(&(tmp[i0])), sizeof(char));
                    ++ ival;
                    ++ i0;
                }
            }
            irow += dp.nRows();
        }
        break;}
    case ibis::UBYTE: {
        array_t<unsigned char> tmp;
        uint64_t irow = 0;
        for (ibis::partList::const_iterator it = parts.begin();
             it != parts.end() && irow < end; ++ it) {
            const ibis::part& dp = **it;
            if (irow + dp.nRows() > begin) {
                const ibis::column* col = dp.getColumn(cn);
                if (col == 0)
                    return -3;
                if (col->getValuesArray(&tmp) < 0)
                    return -4;

                size_t i0 = (begin > irow ? begin - irow : 0);
                const size_t i1 = (end>=irow+dp.nRows() ? dp.nRows() :
                                   end-irow);
                while (i0 < i1) {
                    vals[ival].copy((const char*)(&(tmp[i0])), sizeof(char));
                    ++ ival;
                    ++ i0;
                }
            }
            irow += dp.nRows();
        }
        break;}
    case ibis::SHORT: {
        array_t<int16_t> tmp;
        uint64_t irow = 0;
        for (ibis::partList::const_iterator it = parts.begin();
             it != parts.end() && irow < end; ++ it) {
            const ibis::part& dp = **it;
            if (irow + dp.nRows() > begin) {
                const ibis::column* col = dp.getColumn(cn);
                if (col == 0)
                    return -3;
                if (col->getValuesArray(&tmp) < 0)
                    return -4;

                size_t i0 = (begin > irow ? begin - irow : 0);
                const size_t i1 = (end>=irow+dp.nRows() ? dp.nRows() :
                                   end-irow);
                while (i0 < i1) {
                    vals[ival].copy((const char*)(&(tmp[i0])), sizeof(int16_t));
                    ++ ival;
                    ++ i0;
                }
            }
            irow += dp.nRows();
        }
        break;}
    case ibis::USHORT: {
        array_t<uint16_t> tmp;
        uint64_t irow = 0;
        for (ibis::partList::const_iterator it = parts.begin();
             it != parts.end() && irow < end; ++ it) {
            const ibis::part& dp = **it;
            if (irow + dp.nRows() > begin) {
                const ibis::column* col = dp.getColumn(cn);
                if (col == 0)
                    return -3;
                if (col->getValuesArray(&tmp) < 0)
                    return -4;

                size_t i0 = (begin > irow ? begin - irow : 0);
                const size_t i1 = (end>=irow+dp.nRows() ? dp.nRows() :
                                   end-irow);
                while (i0 < i1) {
                    vals[ival].copy((const char*)(&(tmp[i0])), sizeof(int16_t));
                    ++ ival;
                    ++ i0;
                }
            }
            irow += dp.nRows();
        }
        break;}
    case ibis::INT: {
        array_t<int32_t> tmp;
        uint64_t irow = 0;
        for (ibis::partList::const_iterator it = parts.begin();
             it != parts.end() && irow < end; ++ it) {
            const ibis::part& dp = **it;
            if (irow + dp.nRows() > begin) {
                const ibis::column* col = dp.getColumn(cn);
                if (col == 0)
                    return -3;
                if (col->getValuesArray(&tmp) < 0)
                    return -4;

                size_t i0 = (begin > irow ? begin - irow : 0);
                const size_t i1 = (end>=irow+dp.nRows() ? dp.nRows() :
                                   end-irow);
                while (i0 < i1) {
                    vals[ival].copy((const char*)(&(tmp[i0])), sizeof(int32_t));
                    ++ ival;
                    ++ i0;
                }
            }
            irow += dp.nRows();
        }
        break;}
    case ibis::UINT: {
        array_t<uint32_t> tmp;
        uint64_t irow = 0;
        for (ibis::partList::const_iterator it = parts.begin();
             it != parts.end() && irow < end; ++ it) {
            const ibis::part& dp = **it;
            if (irow + dp.nRows() > begin) {
                const ibis::column* col = dp.getColumn(cn);
                if (col == 0)
                    return -3;
                if (col->getValuesArray(&tmp) < 0)
                    return -4;

                size_t i0 = (begin > irow ? begin - irow : 0);
                const size_t i1 = (end>=irow+dp.nRows() ? dp.nRows() :
                                   end-irow);
                while (i0 < i1) {
                    vals[ival].copy((const char*)(&(tmp[i0])), sizeof(int32_t));
                    ++ ival;
                    ++ i0;
                }
            }
            irow += dp.nRows();
        }
        break;}
    case ibis::LONG: {
        array_t<int64_t> tmp;
        uint64_t irow = 0;
        for (ibis::partList::const_iterator it = parts.begin();
             it != parts.end() && irow < end; ++ it) {
            const ibis::part& dp = **it;
            if (irow + dp.nRows() > begin) {
                const ibis::column* col = dp.getColumn(cn);
                if (col == 0)
                    return -3;
                if (col->getValuesArray(&tmp) < 0)
                    return -4;

                size_t i0 = (begin > irow ? begin - irow : 0);
                const size_t i1 = (end>=irow+dp.nRows() ? dp.nRows() :
                                   end-irow);
                while (i0 < i1) {
                    vals[ival].copy((const char*)(&(tmp[i0])), sizeof(int64_t));
                    ++ ival;
                    ++ i0;
                }
            }
            irow += dp.nRows();
        }
        break;}
    case ibis::OID:
    case ibis::ULONG: {
        array_t<uint64_t> tmp;
        uint64_t irow = 0;
        for (ibis::partList::const_iterator it = parts.begin();
             it != parts.end() && irow < end; ++ it) {
            const ibis::part& dp = **it;
            if (irow + dp.nRows() > begin) {
                const ibis::column* col = dp.getColumn(cn);
                if (col == 0)
                    return -3;
                if (col->getValuesArray(&tmp) < 0)
                    return -4;

                size_t i0 = (begin > irow ? begin - irow : 0);
                const size_t i1 = (end>=irow+dp.nRows() ? dp.nRows() :
                                   end-irow);
                while (i0 < i1) {
                    vals[ival].copy((const char*)(&(tmp[i0])), sizeof(uint64_t));
                    ++ ival;
                    ++ i0;
                }
            }
            irow += dp.nRows();
        }
        break;}
    case ibis::FLOAT: {
        array_t<float> tmp;
        uint64_t irow = 0;
        for (ibis::partList::const_iterator it = parts.begin();
             it != parts.end() && irow < end; ++ it) {
            const ibis::part& dp = **it;
            if (irow + dp.nRows() > begin) {
                const ibis::column* col = dp.getColumn(cn);
                if (col == 0)
                    return -3;
                if (col->getValuesArray(&tmp) < 0)
                    return -4;

                size_t i0 = (begin > irow ? begin - irow : 0);
                const size_t i1 = (end>=irow+dp.nRows() ? dp.nRows() :
                                   end-irow);
                while (i0 < i1) {
                    vals[ival].copy((const char*)(&(tmp[i0])), sizeof(float));
                    ++ ival;
                    ++ i0;
                }
            }
            irow += dp.nRows();
        }
        break;}
    case ibis::DOUBLE: {
        array_t<double> tmp;
        uint64_t irow = 0;
        for (ibis::partList::const_iterator it = parts.begin();
             it != parts.end() && irow < end; ++ it) {
            const ibis::part& dp = **it;
            if (irow + dp.nRows() > begin) {
                const ibis::column* col = dp.getColumn(cn);
                if (col == 0)
                    return -3;
                if (col->getValuesArray(&tmp) < 0)
                    return -4;

                size_t i0 = (begin > irow ? begin - irow : 0);
                const size_t i1 = (end>=irow+dp.nRows() ? dp.nRows() :
                                   end-irow);
                while (i0 < i1) {
                    vals[ival].copy((const char*)(&(tmp[i0])), sizeof(double));
                    ++ ival;
                    ++ i0;
                }
            }
            irow += dp.nRows();
        }
        break;}
    case ibis::CATEGORY:
    case ibis::TEXT: {
        std::string tmp;
        uint64_t irow = 0;
        for (ibis::partList::const_iterator it = parts.begin();
             it != parts.end() && irow < end; ++ it) {
            const ibis::part& dp = **it;
            if (irow + dp.nRows() > begin) {
                const ibis::column* col = dp.getColumn(cn);
                if (col == 0)
                    return -3;

                size_t i0 = (begin > irow ? begin - irow : 0);
                const size_t i1 = (end>=irow+dp.nRows() ? dp.nRows() :
                                   end-irow);
                while (i0 < i1) {
                    if (col->getString(i0, tmp) >= 0)
                        vals[ival].copy(tmp.data(), tmp.size());
                    ++ ival;
                    ++ i0;
                }
            }
            irow += dp.nRows();
        }
        break;}
    case ibis::BLOB: {
        ibis::opaque tmp;
        uint64_t irow = 0;
        for (ibis::partList::const_iterator it = parts.begin();
             it != parts.end() && irow < end; ++ it) {
            const ibis::part& dp = **it;
            if (irow + dp.nRows() > begin) {
                const ibis::column* col = dp.getColumn(cn);
                if (col == 0)
                    return -3;

                size_t i0 = (begin > irow ? begin - irow : 0);
                const size_t i1 = (end>=irow+dp.nRows() ? dp.nRows() :
                                   end-irow);
                while (i0 < i1) {
                    if (col->getOpaque(i0, tmp) >= 0)
                        vals[ival].assign(tmp);
                    ++ ival;
                    ++ i0;
                }
            }
            irow += dp.nRows();
        }
        break;}
    default:
        return -2;
    }
    return ival;
} // ibis::mensa::getColumnAsOpaques

double ibis::mensa::getColumnMin(const char* cn) const {
    double ret = DBL_MAX;
    if (cn == 0 || *cn == 0)
        return ret;
    if (naty.find(cn) == naty.end())
        return ret;

    for (ibis::partList::const_iterator it = parts.end();
         it != parts.end(); ++ it) {
        const ibis::column *col = (*it)->getColumn(cn);
        if (col != 0) {
            double tmp = col->getActualMin();
            if (tmp < ret)
                ret = tmp;
        }
    }
    return ret;
} // ibis::mensa::getColumnMin

double ibis::mensa::getColumnMax(const char* cn) const {
    double ret = -DBL_MAX;
    if (cn == 0 || *cn == 0)
        return ret;
    if (naty.find(cn) == naty.end())
        return ret;

    for (ibis::partList::const_iterator it = parts.end();
         it != parts.end(); ++ it) {
        const ibis::column *col = (*it)->getColumn(cn);
        if (col != 0) {
            double tmp = col->getActualMax();
            if (tmp > ret)
                ret = tmp;
        }
    }
    return ret;
} // ibis::mensa::getColumnMax

long ibis::mensa::getHistogram(const char* constraints,
                               const char* cname,
                               double begin, double end, double stride,
                               std::vector<uint32_t>& counts) const {
    long ierr = -1;
    if (cname == 0 || *cname == 0 || (begin >= end && !(stride < 0.0)) ||
        (begin <= end && !(stride > 0.0))) return ierr;
    if (sizeof(uint32_t) == sizeof(uint32_t)) {
        counts.clear();
        for (ibis::partList::const_iterator it = parts.begin();
             it != parts.end(); ++ it) {
            ierr = (*it)->get1DDistribution
                (constraints, cname, begin, end, stride,
                 reinterpret_cast<std::vector<uint32_t>&>(counts));
            if (ierr < 0) return ierr;
        }
    }
    else {
        const uint32_t nbins = 1 +
            static_cast<uint32_t>(std::floor((end - begin) / stride));
        counts.resize(nbins);
        for (uint32_t i = 0; i < nbins; ++ i)
            counts[i] = 0;

        std::vector<uint32_t> tmp;
        for (ibis::partList::const_iterator it = parts.begin();
             it != parts.end(); ++ it) {
            tmp.clear();
            ierr = (*it)->get1DDistribution(constraints, cname, begin,
                                            end, stride, tmp);
            if (ierr < 0) return ierr;
            for (uint32_t i = 0; i < nbins; ++ i)
                counts[i] += tmp[i];
        }
    }
    return ierr;
} // ibis::mensa::getHistogram

long ibis::mensa::getHistogram2D(const char* constraints,
                                 const char* cname1,
                                 double begin1, double end1, double stride1,
                                 const char* cname2,
                                 double begin2, double end2, double stride2,
                                 std::vector<uint32_t>& counts) const {
    long ierr = -1;
    if (cname1 == 0 || cname2 == 0 || (begin1 >= end1 && !(stride1 < 0.0)) ||
        (begin1 <= end1 && !(stride1 > 0.0)) ||
        *cname1 == 0 || *cname2 == 0 ||  (begin2 >= end2 && !(stride2 < 0.0)) ||
        (begin2 <= end2 && !(stride2 > 0.0))) return ierr;

    if (sizeof(uint32_t) == sizeof(uint32_t)) {
        counts.clear();

        for (ibis::partList::const_iterator it = parts.begin();
             it != parts.end(); ++ it) {
            ierr = (*it)->get2DDistribution
                (constraints, cname1, begin1, end1, stride1, cname2, begin2,
                 end2, stride2,
                 reinterpret_cast<std::vector<uint32_t>&>(counts));
            if (ierr < 0) return ierr;
        }
    }
    else {
        const uint32_t nbins =
            (1 + static_cast<uint32_t>(std::floor((end1 - begin1) / stride1))) *
            (1 + static_cast<uint32_t>(std::floor((end2 - begin2) / stride2)));
        counts.resize(nbins);
        for (uint32_t i = 0; i < nbins; ++ i)
            counts[i] = 0;

        std::vector<uint32_t> tmp;
        for (ibis::partList::const_iterator it = parts.begin();
             it != parts.end(); ++ it) {
            ierr = (*it)->get2DDistribution
                (constraints, cname1, begin1, end1, stride1, cname2, begin2,
                 end2, stride2, tmp);
            if (ierr < 0) return ierr;

            for (uint32_t i = 0; i < nbins; ++ i)
                counts[i] += tmp[i];
        }
    }
    return ierr;
} // ibis::mensa::getHistogram2D

long ibis::mensa::getHistogram3D(const char* constraints,
                                 const char* cname1,
                                 double begin1, double end1, double stride1,
                                 const char* cname2,
                                 double begin2, double end2, double stride2,
                                 const char* cname3,
                                 double begin3, double end3, double stride3,
                                 std::vector<uint32_t>& counts) const {
    long ierr = -1;
    if (cname1 == 0 || cname2 == 0 || cname3 == 0 ||
        *cname1 == 0 || *cname2 == 0 || *cname3 == 0 ||
        (begin1 >= end1 && !(stride1 < 0.0)) ||
        (begin1 <= end1 && !(stride1 > 0.0)) ||
        (begin2 >= end2 && !(stride2 < 0.0)) ||
        (begin2 <= end2 && !(stride2 > 0.0)) ||
        (begin3 >= end3 && !(stride3 < 0.0)) ||
        (begin3 <= end3 && !(stride3 > 0.0))) return -1;

    if (sizeof(uint32_t) == sizeof(uint32_t)) {
        counts.clear();

        for (ibis::partList::const_iterator it = parts.begin();
             it != parts.end(); ++ it) {
            ierr = (*it)->get3DDistribution
                (constraints, cname1, begin1, end1, stride1, cname2, begin2,
                 end2, stride2, cname3, begin3, end3, stride3,
                 reinterpret_cast<std::vector<uint32_t>&>(counts));
            if (ierr < 0) return ierr;
        }
    }
    else {
        const uint32_t nbins =
            (1 + static_cast<uint32_t>(std::floor((end1 - begin1) / stride1))) *
            (1 + static_cast<uint32_t>(std::floor((end2 - begin2) / stride2))) *
            (1 + static_cast<uint32_t>(std::floor((end3 - begin3) / stride3)));
        counts.resize(nbins);
        for (uint32_t i = 0; i < nbins; ++ i)
            counts[i] = 0;

        std::vector<uint32_t> tmp;
        for (ibis::partList::const_iterator it = parts.begin();
             it != parts.end(); ++ it) {
            ierr = (*it)->get3DDistribution
                (constraints, cname1, begin1, end1, stride1, cname2, begin2,
                 end2, stride2, cname3, begin3, end3, stride3, tmp);
            if (ierr < 0) return ierr;

            for (uint32_t i = 0; i < nbins; ++ i)
                counts[i] += tmp[i];
        }
    }

    return ierr;
} // ibis::mensa::getHistogram3D

int ibis::mensa::dump(std::ostream& out, const char* del) const {
    ibis::mensa::cursor cur(*this);
    while (0 == cur.fetch()) {
        int ierr = cur.dumpBlock(out, del);
        if (ierr < 0)  {
            out << " ... ierr = " << ierr << std::endl;
            return ierr;
        }
    }
    return 0;
} // ibis::mensa::dump

int ibis::mensa::backup(const char* dir, const char* tname,
                        const char* tdesc) const {
    LOGGER(ibis::gVerbose > 0)
        << "Warning -- function mensa::backup has NOT been implemented yet";
    return -1;
} // ibis::mensa::backup

ibis::table::cursor* ibis::mensa::createCursor() const {
    return new ibis::mensa::cursor(*this);
} // ibis::mensa::createCursor

/// Constructure a cursor object for row-wise data access to a @c
/// ibis::mensa object.
ibis::mensa::cursor::cursor(const ibis::mensa& t)
    : buffer(t.nColumns()), tab(t), curPart(0), pBegin(0), bBegin(0),
      bEnd(0), curRow(-1) {
    if (curPart >= t.parts.size()) return; // no data partition
    if (buffer.empty()) return;

    // use the 1st data partition for the names and types
    uint32_t j = 0;
    long unsigned row_width = 0;
    const ibis::part& pt1 = *(t.parts.front());
    for (j = 0; j < pt1.nColumns(); ++ j) {
        const ibis::column &col = *(pt1.getColumn(j));
        buffer[j].cval = 0;
        buffer[j].cname = col.name();
        buffer[j].ctype = col.type();
        bufmap[col.name()] = j;
        switch (col.type()) {
        case ibis::BYTE:
        case ibis::UBYTE:
            ++ row_width; break;
        case ibis::SHORT:
        case ibis::USHORT:
            row_width += 2; break;
        case ibis::INT:
        case ibis::UINT:
        case ibis::FLOAT:
            row_width += 4; break;
        case ibis::OID:
        case ibis::LONG:
        case ibis::ULONG:
        case ibis::DOUBLE:
            row_width += 8; break;
        default:
            row_width += 16; break;
        }
    }
    if (row_width == 0) row_width = 1024 * t.naty.size();
    row_width = ibis::fileManager::bytesFree() / row_width;
    j = 0;
    while (row_width > 0) {
        row_width >>= 1;
        ++ j;
    }
    -- j;
    if (j > 30) // maximum block size 1 billion
        preferred_block_size = 0x40000000;
    else if (j > 10) // read enough rows to take up half of the free memory
        preferred_block_size = (1 << j);
    else // minimum block size 1 thousand
        preferred_block_size = 1024;

    LOGGER(ibis::gVerbose > 2)
        << "mensa::cursor constructed for table " << t.name()
        << " with preferred block size " << preferred_block_size;
} // ibis::mensa::cursor::cursor

/// Fill the buffer for variable number @c i.  On success, return 0,
/// otherwise return a negative value.
/// @note The member variable @c cval in the buffer for a string valued
/// column is not the usual ibis::fileManager::storage object, instead
/// it is simply the pointer to the ibis::column object.  The string
/// variable is retrieved through the column object one at a time using the
/// function @c getString.
int ibis::mensa::cursor::fillBuffer(uint32_t i) const {
    if (curPart >= tab.parts.size() || tab.parts[curPart] == 0) return -1;
    const ibis::part& apart = *(tab.parts[curPart]);
    // has to do a look up based on the column name, becaus the ith column
    // of the data partition may not be the correct one (some columns may
    // be missing)
    const ibis::column* col = apart.getColumn(buffer[i].cname);
    if (col == 0)
        return -2;
    if (buffer[i].ctype == ibis::CATEGORY || buffer[i].ctype == ibis::TEXT
        || buffer[i].ctype == ibis::BLOB) {
        buffer[i].cval = const_cast<void*>(static_cast<const void*>(col));
        return 0;
    }

    int ierr = 0;
    ibis::bitvector mask;
    if (bBegin > pBegin)
        mask.appendFill(0, static_cast<ibis::bitvector::word_t>(bBegin-pBegin));
    mask.adjustSize(static_cast<ibis::bitvector::word_t>(bEnd-pBegin),
                    static_cast<ibis::bitvector::word_t>(apart.nRows()));
    switch (buffer[i].ctype) {
    case ibis::BYTE: {
        if (buffer[i].cval == 0)
            buffer[i].cval = new array_t<signed char>();
        ierr = col->selectValues
            (mask, static_cast<array_t< signed char > *>(buffer[i].cval));
        break;}
    case ibis::UBYTE: {
        if (buffer[i].cval == 0)
            buffer[i].cval = new array_t<unsigned char>();
        ierr = col->selectValues
            (mask, static_cast<array_t< unsigned char > *>(buffer[i].cval));
        break;}
    case ibis::SHORT: {
        if (buffer[i].cval == 0)
            buffer[i].cval = new array_t<int16_t>();
        ierr = col->selectValues
            (mask, static_cast<array_t< int16_t > *>(buffer[i].cval));
        break;}
    case ibis::USHORT: {
        if (buffer[i].cval == 0)
            buffer[i].cval = new array_t<uint16_t>();
        ierr = col->selectValues
            (mask, static_cast<array_t< uint16_t >* >(buffer[i].cval));
        break;}
    case ibis::INT: {
        if (buffer[i].cval == 0)
            buffer[i].cval = new array_t<int32_t>;
        ierr = col->selectValues
            (mask, static_cast< array_t< int32_t >* >(buffer[i].cval));
        break;}
    case ibis::UINT: {
        if (buffer[i].cval == 0)
            buffer[i].cval = new array_t<uint32_t>;
        ierr = col->selectValues
            (mask, static_cast< array_t< uint32_t >* >(buffer[i].cval));
        break;}
    case ibis::LONG: {
        if (buffer[i].cval == 0)
            buffer[i].cval = new array_t<int64_t>;
        ierr = col->selectValues
            (mask, static_cast< array_t<int64_t>* >(buffer[i].cval));
        break;}
    case ibis::OID:
    case ibis::ULONG: {
     if (buffer[i].cval == 0)
         buffer[i].cval = new array_t<uint64_t>;
     ierr = col->selectValues
         (mask, static_cast< array_t< uint64_t >* >(buffer[i].cval));
     break;}
    case ibis::FLOAT: {
        if (buffer[i].cval == 0)
            buffer[i].cval = new array_t<float>;
        ierr = col->selectValues
            (mask, static_cast< array_t< float >* >(buffer[i].cval));
        break;}
    case ibis::DOUBLE: {
        if (buffer[i].cval == 0)
            buffer[i].cval = new array_t<double>;
        ierr = col->selectValues
            (mask, static_cast< array_t< double >* >(buffer[i].cval));
        break;}
    default:
        LOGGER(ibis::gVerbose > 0)
            << "mensa::cursor::fillBuffer(" << i
            << ") can not handle column " << col->name() << " type "
            << ibis::TYPESTRING[buffer[i].ctype];
        ierr = -2;
        break;
    } // switch

    return ierr;
} // ibis::mensa::cursor::fillBuffer

/// Fill the buffers for every column.  If all column buffers are not
/// empty, then they are assumed to contain the expected content already.
/// Otherwise, it calls fillBuffer on each column.
int ibis::mensa::cursor::fillBuffers() const {
    uint32_t cnt = 0;
    for (uint32_t i = 0; i < buffer.size(); ++ i) {
        if (buffer[i].cval != 0) {
            switch (buffer[i].ctype) {
            case ibis::BYTE: {
                cnt += (static_cast<const array_t< signed char > *>
                        (buffer[i].cval)->empty() == false);
                break;}
            case ibis::UBYTE: {
                cnt += (static_cast<const array_t<unsigned char> *>
                        (buffer[i].cval)->empty() == false);
                break;}
            case ibis::SHORT: {
                cnt += (static_cast<const array_t< int16_t > *>
                        (buffer[i].cval)->empty() == false);
                break;}
            case ibis::USHORT: {
                cnt += (static_cast<const array_t< uint16_t >* >
                        (buffer[i].cval)->empty() == false);
                break;}
            case ibis::INT: {
                cnt += (static_cast<const array_t< int32_t >* >
                        (buffer[i].cval)->empty() == false);
                break;}
            case ibis::UINT: {
                cnt += (static_cast<const array_t< uint32_t >* >
                        (buffer[i].cval)->empty() == false);
                break;}
            case ibis::LONG: {
                cnt += (static_cast<const array_t<int64_t>* >
                        (buffer[i].cval)->empty() == false);
                break;}
            case ibis::OID:
            case ibis::ULONG: {
                cnt += (static_cast<const array_t< uint64_t >* >
                        (buffer[i].cval)->empty() == false);
                break;}
            case ibis::FLOAT: {
                cnt += (static_cast<const array_t< float >* >
                        (buffer[i].cval)->empty() == false);
                break;}
            case ibis::DOUBLE: {
                cnt += (static_cast<const array_t< double >* >
                        (buffer[i].cval)->empty() == false);
                break;}
            default:
                ++ cnt;
                break;
            } // switch
        }
    }
    if (cnt >= buffer.size()) return 1;

    std::string evt = "mensa[";
    evt += tab.name();
    evt += "]::cursor::fillBuffers";
    int ierr;
    ibis::util::timer mytimer(evt.c_str(), 4);
    for (uint32_t i = 0; i < buffer.size(); ++ i) {
        ierr = fillBuffer(i);
        if (ierr < 0) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << evt << " failed to fill buffer for column "
                << i << "(" << buffer[i].cname << ", "
                << ibis::TYPESTRING[buffer[i].ctype] << ") of partition "
                << tab.parts[curPart]->name() << " with pBegin " << pBegin
                << ", bBegin " << bBegin << ", and bEnd " << bEnd
                << ", ierr = " << ierr;
            return ierr;
        }
    }
    return 0;
} // ibis::mensa::cursor::fillBuffers

/// Mark all existing buffer as empty.
void ibis::mensa::cursor::clearBuffers() {
    for (uint32_t i = 0; i < buffer.size(); ++ i) {
        if (buffer[i].cval == 0) continue;

        switch (buffer[i].ctype) {
        default:
            buffer[i].cval = 0;
            break;
        case ibis::BYTE:
            static_cast<array_t<signed char>*>(buffer[i].cval)->clear();
            break;
        case ibis::UBYTE:
            static_cast<array_t<unsigned char>*>(buffer[i].cval)->clear();
            break;
        case ibis::SHORT:
            static_cast<array_t<int16_t>*>(buffer[i].cval)->clear();
            break;
        case ibis::USHORT:
            static_cast<array_t<uint16_t>*>(buffer[i].cval)->clear();
            break;
        case ibis::INT:
            static_cast<array_t<int32_t>*>(buffer[i].cval)->clear();
            break;
        case ibis::UINT:
            static_cast<array_t<uint32_t>*>(buffer[i].cval)->clear();
            break;
        case ibis::LONG:
            static_cast<array_t<int64_t>*>(buffer[i].cval)->clear();
            break;
        case ibis::OID:
        case ibis::ULONG:
            static_cast<array_t<uint64_t>*>(buffer[i].cval)->clear();
            break;
        case ibis::FLOAT:
            static_cast<array_t<float>*>(buffer[i].cval)->clear();
            break;
        case ibis::DOUBLE:
            static_cast<array_t<double>*>(buffer[i].cval)->clear();
            break;
        }
    }
} // ibis::mensa::cursor::clearBuffers

/// Points the the next row.
int ibis::mensa::cursor::fetch() {
    if (curPart >= tab.parts.size()) return -1;

    ++ curRow;
    if (static_cast<uint64_t>(curRow) >= bEnd) { // reach end of the block
        clearBuffers();
        if (bEnd >= pBegin + tab.parts[curPart]->nRows()) {
            pBegin += tab.parts[curPart]->nRows();
            ++ curPart;
            if (curPart >= tab.parts.size()) return -1;

            bBegin = pBegin;
            bEnd = pBegin +
                (preferred_block_size <= tab.parts[curPart]->nRows() ?
                 preferred_block_size : tab.parts[curPart]->nRows());
        }
        else {
            bBegin = bEnd;
            bEnd += preferred_block_size;
            const uint64_t pEnd = pBegin + tab.parts[curPart]->nRows();
            if (bEnd > pEnd)
                bEnd = pEnd;
        }
        return fillBuffers();
    }
    return 0;
} // ibis::mensa::cursor::fetch

/// Pointers to the specified row.
int ibis::mensa::cursor::fetch(uint64_t irow) {
    if (curPart >= tab.parts.size()) return -1;
    if (bEnd <= irow)
        clearBuffers();

    while (curPart < tab.parts.size() &&
           pBegin + tab.parts[curPart]->nRows() <= irow) {
        pBegin += tab.parts[curPart]->nRows();
        ++ curPart;
    }
    if (curPart < tab.parts.size()) {
        curRow = irow;
        bBegin = irow;
        bEnd = irow + preferred_block_size;
        if (bEnd > pBegin + tab.parts[curPart]->nRows())
            bEnd = pBegin + tab.parts[curPart]->nRows();
        return fillBuffers();
    }
    else {
        curRow = pBegin;
        return -1;
    }
} // ibis::mensa::cursor::fetch

int ibis::mensa::cursor::fetch(ibis::table::row& res) {
    int ierr = fetch();
    if (ierr < 0) return ierr;

    fillRow(res);
    return 0;
} // ibis::mensa::cursor::fetch

int ibis::mensa::cursor::fetch(uint64_t irow, ibis::table::row& res) {
    int ierr = fetch(irow);
    if (ierr < 0) return ierr;

    fillRow(res);
    return 0;
} // ibis::mensa::cursor::fetch

void ibis::mensa::cursor::fillRow(ibis::table::row& res) const {
    res.clear();
    const uint32_t il = static_cast<uint32_t>(curRow - bBegin);
    for (uint32_t j = 0; j < buffer.size(); ++ j) {
        switch (buffer[j].ctype) {
        case ibis::BYTE: {
            res.bytesnames.push_back(buffer[j].cname);
            if (buffer[j].cval != 0) {
                res.bytesvalues.push_back
                    ((*static_cast<const array_t<signed char>*>
                      (buffer[j].cval))[il]);
            }
            else {
                res.bytesvalues.push_back(0x7F);
            }
            break;}
        case ibis::UBYTE: {
            res.ubytesnames.push_back(buffer[j].cname);
            if (buffer[j].cval != 0) {
                res.ubytesvalues.push_back
                    ((*static_cast<const array_t<unsigned char>*>
                      (buffer[j].cval))[il]);
            }
            else {
                res.ubytesvalues.push_back(0xFFU);
            }
            break;}
        case ibis::SHORT: {
            res.shortsnames.push_back(buffer[j].cname);
            if (buffer[j].cval != 0) {
                res.shortsvalues.push_back
                    ((*static_cast<const array_t<int16_t>*>
                      (buffer[j].cval))[il]);
            }
            else {
                res.shortsvalues.push_back(0x7FFF);
            }
            break;}
        case ibis::USHORT: {
            res.ushortsnames.push_back(buffer[j].cname);
            if (buffer[j].cval != 0) {
                res.ushortsvalues.push_back
                    ((*static_cast<const array_t<uint16_t>*>
                      (buffer[j].cval))[il]);
            }
            else {
                res.ushortsvalues.push_back(0xFFFFU);
            }
            break;}
        case ibis::INT: {
            res.intsnames.push_back(buffer[j].cname);
            if (buffer[j].cval != 0) {
                res.intsvalues.push_back
                    ((*static_cast<const array_t<int32_t>*>
                      (buffer[j].cval))[il]);
            }
            else {
                res.intsvalues.push_back(0x7FFFFFFF);
            }
            break;}
        case ibis::UINT: {
            res.uintsnames.push_back(buffer[j].cname);
            if (buffer[j].cval != 0) {
                res.uintsvalues.push_back
                    ((*static_cast<const array_t<uint32_t>*>
                      (buffer[j].cval))[il]);
            }
            else {
                res.uintsvalues.push_back(0xFFFFFFFFU);
            }
            break;}
        case ibis::LONG: {
            res.longsnames.push_back(buffer[j].cname);
            if (buffer[j].cval != 0) {
                res.longsvalues.push_back
                    ((*static_cast<const array_t<int64_t>*>
                      (buffer[j].cval))[il]);
            }
            else {
                res.longsvalues.push_back(0x7FFFFFFFFFFFFFFFLL);
            }
            break;}
        case ibis::OID:
        case ibis::ULONG: {
            res.ulongsnames.push_back(buffer[j].cname);
            if (buffer[j].cval != 0) {
                res.ulongsvalues.push_back
                    ((*static_cast<const array_t<uint64_t>*>
                      (buffer[j].cval))[il]);
            }
            else {
                res.ulongsvalues.push_back(0xFFFFFFFFFFFFFFFFULL);
            }
            break;}
        case ibis::FLOAT: {
            res.floatsnames.push_back(buffer[j].cname);
            if (buffer[j].cval != 0) {
                res.floatsvalues.push_back
                    ((*static_cast<const array_t<float>*>
                      (buffer[j].cval))[il]);
            }
            else {
                res.floatsvalues.push_back(FASTBIT_FLOAT_NULL);
            }
            break;}
        case ibis::DOUBLE: {
            res.doublesnames.push_back(buffer[j].cname);
            if (buffer[j].cval != 0) {
                res.doublesvalues.push_back
                    ((*static_cast<const array_t<double>*>
                      (buffer[j].cval))[il]);
            }
            else {
                res.doublesvalues.push_back(FASTBIT_DOUBLE_NULL);
            }
            break;}
        case ibis::BLOB: {
            ibis::opaque val;
            res.blobsnames.push_back(buffer[j].cname);
            if (buffer[j].cval != 0) {
                char *buf = 0;
                uint64_t sz = 0;
                int ierr = reinterpret_cast<const ibis::blob*>(buffer[j].cval)
                    ->getBlob(curRow-pBegin, buf, sz);
                if (ierr >= 0 && sz > 0 && buf != 0)
                    val.assign(buf, sz);
                delete [] buf;
            }
            res.blobsvalues.resize(res.blobsvalues.size()+1);
            res.blobsvalues.back().swap(val);
            break;}
        case ibis::TEXT: {
            res.textsnames.push_back(buffer[j].cname);
            if (buffer[j].cval != 0) {
                std::string tmp;
                reinterpret_cast<const ibis::text*>(buffer[j].cval)
                    ->getString(curRow-pBegin, tmp);
                res.textsvalues.push_back(tmp);
            }
            else {
                res.textsvalues.push_back("");
            }
            break;}
        case ibis::CATEGORY: {
            res.catsnames.push_back(buffer[j].cname);
            if (buffer[j].cval != 0) {
                std::string tmp;
                reinterpret_cast<const ibis::text*>(buffer[j].cval)
                    ->getString(curRow - pBegin, tmp);
                res.catsvalues.push_back(tmp);
            }
            else {
                res.catsvalues.push_back("");
            }
            break;}
        default: {
            if (ibis::gVerbose > 1)
                ibis::util::logMessage
                    ("Warning", "mensa::cursor::fillRow is not expected "
                     "to encounter data type %s (column name %s)",
                     ibis::TYPESTRING[(int)buffer[j].ctype], buffer[j].cname);
            break;}
        }
    }
} // ibis::mensa::cursor::fillRow

/**
   Print the current row.  Assumes the caller has performed the fetch
   operation.

   Return values:
   -   0  -- normal (successful) completion.
   -  -1  -- cursor objection not initialized, call fetch first.
   -  -2  -- unable to load data into memory.
   -  -4  -- error in the output stream.
 */
int ibis::mensa::cursor::dump(std::ostream& out, const char* del) const {
    if (tab.nColumns() == 0) return 0;
    if (curRow < 0 || curPart >= tab.parts.size())
        return -1;
    int ierr;
//     if (static_cast<uint64_t>(curRow) == bBegin) {
//      // first time accessing the data partition
//      ierr = fillBuffers();
//      if (ierr < 0) {
//          LOGGER(ibis::gVerbose > 1)
//              << "mensa[" << tab.name() << "]::cursor::dump "
//              "call to fillBuffers() failed with ierr = " << ierr
//              << " at partition " << tab.parts[curPart]->name()
//              << ", pBegin " << pBegin << ", bBegin " << bBegin
//              << ", bEnd " << bEnd;
//          return -2;
//      }
//     }

    const uint32_t i = static_cast<uint32_t>(curRow - bBegin);
    ierr = dumpIJ(out, i, 0U);
    if (ierr < 0) return ierr;
    if (del == 0) del = ", ";
    for (uint32_t j = 1; j < tab.nColumns(); ++ j) {
        out << del;
        ierr = dumpIJ(out, i, j);
        if (ierr < 0)
            return ierr;
    }
    out << "\n";
    if (! out)
        ierr = -4;
    return ierr;
} // ibis::mensa::cursor::dump

/// Print out the content of the current block.  Also move the pointer to
/// the last row of the block.
int ibis::mensa::cursor::dumpBlock(std::ostream& out, const char* del) {
    if (tab.nColumns() == 0) return 0;
    if (curPart >= tab.parts.size()) return 0;
    if (curRow < 0) // not initialized
        return -1;
    int ierr;
    if (static_cast<uint64_t>(curRow) == bBegin) {
        // first time accessing the data partition
        ierr = fillBuffers();
        if (ierr < 0) {
            LOGGER(ibis::gVerbose > 1)
                << "mensa[" << tab.name() << "]::cursor::dumpBlock "
                "call to fillBuffers() failed with ierr = " << ierr
                << " at partition " << tab.parts[curPart]->name()
                << ", pBegin " << pBegin << ", bBegin " << bBegin
                << ", bEnd " << bEnd;
            return -2;
        }
    }

    uint32_t i = static_cast<uint32_t>(curRow - bBegin);
    // print the first row with error checking
    ierr = dumpIJ(out, i, 0U);
    if (ierr < 0) return ierr;
    if (del == 0) del = ", ";
    for (uint32_t j = 1; j < tab.nColumns(); ++ j) {
        out << del;
        ierr = dumpIJ(out, i, j);
        if (ierr < 0)
            return -4;
    }
    out << "\n";
    if (! out)
        ierr = -4;
    // print the rest of the rows without error checking
    const uint32_t nelem = static_cast<uint32_t>(bEnd - bBegin);
    for (++ i; i < nelem; ++ i) {
        (void) dumpIJ(out, i, 0U);
        for (uint32_t j = 1; j < buffer.size(); ++ j) {
            out << del;
            (void) dumpIJ(out, i, j);
        }
        out << "\n";
    }

    // move the position of the cursor to the last row of the block
    curRow = bEnd-1;
    return (out ? 0 : -4);
} // ibis::mensa::cursor::dumpBlock

/// Print the next nr rows of the table to the specified output stream.
int ibis::mensa::cursor::dumpSome(std::ostream &out, uint64_t nr,
                                  const char *del) {
    int ierr = 0;
    if (curRow < 0) {
        ierr = fetch();
        if (ierr < 0) {
            LOGGER(ibis::gVerbose > 1)
                << "mensa[" << tab.name() << "]::cursor::dumpSome "
                "call to fetch (of the block) failed with ierr = " << ierr
                << " at partition " << tab.parts[curPart]->name()
                << ", pBegin " << pBegin << ", bBegin " << bBegin
                << ", bEnd " << bEnd;
            return -1;
        }
    }

    nr += pBegin + bBegin + curRow;
    if (nr > tab.nRows()) nr = tab.nRows();
    while (static_cast<uint64_t>(curRow) < nr) {
        if (bEnd <= nr) {
            ierr = dumpBlock(out, del);
            if (ierr < 0) {
                LOGGER(ibis::gVerbose > 1)
                    << "mensa[" << tab.name() << "]::cursor::dumpSome "
                    "call to fillBlock() failed with ierr = " << ierr
                    << " at partition " << tab.parts[curPart]->name()
                    << ", pBegin " << pBegin << ", bBegin " << bBegin
                    << ", bEnd " << bEnd;
                return -3;
            }
            (void) fetch();
        }
        else {
            while (static_cast<uint64_t>(curRow) < nr) {
                ierr = dump(out, del);
                if (ierr < 0) {
                    LOGGER(ibis::gVerbose > 1)
                        << "mensa[" << tab.name()
                        << "]::cursor::dumpSome call to dump() "
                        "failed with ierr = " << ierr
                        << " at partition " << tab.parts[curPart]->name()
                        << ", pBegin " << pBegin << ", bBegin " << bBegin
                        << ", bEnd " << bEnd;
                    return -4;
                }
                ierr = fetch();
                if (ierr < 0) {
                    LOGGER(ibis::gVerbose > 1)
                        << "mensa[" << tab.name()
                        << "]::cursor::dumpSome call to fetch(row " << curRow
                        << ") failed with ierr = " << ierr
                        << " at partition " << tab.parts[curPart]->name()
                        << ", pBegin " << pBegin << ", bBegin " << bBegin
                        << ", bEnd " << bEnd;
                    return -5;
                }
            }
        }
    }
    if (static_cast<uint64_t>(curRow) < tab.nRows())
        out << "\t... " << tab.nRows() - curRow << " remaining in table "
            << tab.name() << "\n";
    return ierr;
} // ibis::mensa::cursor::dumpSome

/// Print the ith element in the current block for column j.
/// @note This function does not perform array bounds check.
int ibis::mensa::cursor::dumpIJ(std::ostream& out, uint32_t i,
                                uint32_t j) const {
    int ierr = 0;
    switch (buffer[j].ctype) {
    default: {
        ierr = -2;
        break;}
    case ibis::BYTE: {
        if (buffer[j].cval == 0) return -1; // null value
        const array_t<signed char> *tmp
            = static_cast<const array_t<signed char>*>(buffer[j].cval);
        out << (int) ((*tmp)[i]);
        break;}
    case ibis::UBYTE: {
        if (buffer[j].cval == 0) return -1; // null value
        const array_t<unsigned char> *tmp
            = static_cast<const array_t<unsigned char>*>(buffer[j].cval);
        out << (unsigned int) ((*tmp)[i]);
        break;}
    case ibis::SHORT: {
        if (buffer[j].cval == 0) return -1; // null value
        const array_t<int16_t> *tmp
            = static_cast<const array_t<int16_t>*>(buffer[j].cval);
        out << (*tmp)[i];
        break;}
    case ibis::USHORT: {
        if (buffer[j].cval == 0) return -1; // null value
        const array_t<uint16_t> *tmp
            = static_cast<const array_t<uint16_t>*>(buffer[j].cval);
        out << (*tmp)[i];
        break;}
    case ibis::INT: {
        if (buffer[j].cval == 0) return -1; // null value
        const array_t<int32_t> *tmp
            = static_cast<const array_t<int32_t>*>(buffer[j].cval);
        out << (*tmp)[i];
        break;}
    case ibis::UINT: {
        if (buffer[j].cval == 0) return -1; // null value
        const array_t<uint32_t> *tmp
            = static_cast<const array_t<uint32_t>*>(buffer[j].cval);
        out << (*tmp)[i];
        break;}
    case ibis::LONG: {
        if (buffer[j].cval == 0) return -1; // null value
        const array_t<int64_t> *tmp
            = static_cast<const array_t<int64_t>*>(buffer[j].cval);
        out << (*tmp)[i];
        break;}
    case ibis::OID:
    case ibis::ULONG: {
        if (buffer[j].cval == 0) return -1; // null value
        const array_t<uint64_t> *tmp
            = static_cast<const array_t<uint64_t>*>(buffer[j].cval);
        out << (*tmp)[i];
        break;}
    case ibis::FLOAT: {
        if (buffer[j].cval == 0) return -1; // null value
        const array_t<float> *tmp
            = static_cast<const array_t<float>*>(buffer[j].cval);
        out << std::setprecision(8) << (*tmp)[i];
        break;}
    case ibis::DOUBLE: {
        if (buffer[j].cval == 0) return -1; // null value
        const array_t<double> *tmp
            = static_cast<const array_t<double>*>(buffer[j].cval);
        out << std::setprecision(18) << (*tmp)[i];
        break;}
    case ibis::TEXT:
    case ibis::CATEGORY: {
        if (curPart < tab.parts.size()) {
            const ibis::column* col = 0;
            if (buffer[j].cval != 0) {
                col = reinterpret_cast<const ibis::column*>(buffer[j].cval);
            }
            else {
                col = tab.parts[curPart]->getColumn(buffer[j].cname);
                buffer[j].cval =
                    const_cast<void*>(static_cast<const void*>(col));
            }
            if (col != 0) {
                std::string val;
                static_cast<const ibis::text*>(col)
                    ->ibis::text::getString
                    (static_cast<uint32_t>(i+bBegin-pBegin), val);
#if DEBUG+0 > 1 || _DEBUG+0 > 1
                LOGGER(ibis::gVerbose > 5)
                    << "DEBUG -- mensa::cursor::dump(" << i << ", " << j
                    << ") printing string " << val << " to position "
                    << static_cast<off_t>(out.tellp()) << " of output stream "
                    << static_cast<void*>(&out);
#endif
                out << '"' << val << '"';
            }
            else {
                ierr = -3;
            }
        }
        break;}
    case ibis::BLOB: {
        if (curPart < tab.parts.size()) {
            const ibis::blob *blo;
            if (buffer[j].cval != 0) {
                blo = reinterpret_cast<const ibis::blob*>(buffer[j].cval);
            }
            else {
                blo = dynamic_cast<const ibis::blob*>
                    (tab.parts[curPart]->getColumn(buffer[j].cname));
                buffer[j].cval =
                    const_cast<void*>(static_cast<const void*>(blo));
                LOGGER(blo == 0 && ibis::gVerbose > 0)
                    << "mensa::cursor::dumpIJ(" << i << ", " << j
                    << ") failed to find a column with name "
                    << buffer[j].cname << " with type blob";
            }

            if (blo != 0) {
                char *buf = 0;
                uint64_t sz = 0;
                ierr = blo->getBlob(static_cast<uint32_t>(i+bBegin-pBegin),
                                    buf, sz);
                if (ierr >= 0 && sz > 0 && buf != 0) {
                    out << "0x" << std::hex;
                    for (uint32_t k = 0; k < sz; ++ k)
                        out << std::setprecision(2) << (uint16_t)(buf[k]);
                    out << std::dec;
                }
            }
            else {
                ierr = -5;
            }
        }
        break;}
    }
    if (ierr >= 0 && ! out) {
        LOGGER(ibis::gVerbose > 1)
            << "Warning -- mensa::cursor::dumpIJ(" << i << ", " << j
            << ") failed to write to the output stream at offset "
            << static_cast<off_t>(out.tellp());
        ierr = -4;
    }
    return ierr;
} // ibis::mensa::cursor::dumpIJ

int ibis::mensa::cursor::getColumnAsByte(uint32_t j, char& val) const {
    if (curRow < 0 || curPart >= tab.parts.size() || j > tab.nColumns())
        return -1;
    int ierr = 0;
    if (static_cast<uint64_t>(curRow) == bBegin)
        ierr = fillBuffer(j); // first time accessing the data partition
    if (ierr < 0 || buffer[j].cval == 0) // missing column in this partition
        return -2;

    switch (buffer[j].ctype) {
    case ibis::UBYTE:
    case ibis::BYTE: {
        val = (*(static_cast<array_t<signed char>*>(buffer[j].cval)))
            [curRow - bBegin];
        ierr = 0;
        break;}
    default: {
        ierr = -1;
        break;}
    }
    return ierr;
} // ibis::mensa::cursor::getColumnAsByte

int ibis::mensa::cursor::getColumnAsUByte(uint32_t j,
                                          unsigned char& val) const {
    if (curRow < 0 || curPart >= tab.parts.size() || j > tab.nColumns())
        return -1;
    int ierr = 0;
    if (static_cast<uint64_t>(curRow) == bBegin)
        ierr = fillBuffer(j); // first time accessing the data partition
    if (ierr < 0 || buffer[j].cval == 0) // missing column in this partition
        return -2;

    switch (buffer[j].ctype) {
    case ibis::BYTE:
    case ibis::UBYTE: {
        val = (*(static_cast<array_t<unsigned char>*>(buffer[j].cval)))
            [curRow - bBegin];
        ierr = 0;
        break;}
    default: {
        ierr = -1;
        break;}
    }
    return ierr;
} // ibis::mensa::cursor::getColumnAsUByte

int ibis::mensa::cursor::getColumnAsShort(uint32_t j, int16_t& val) const {
    if (curRow < 0 || curPart >= tab.parts.size() || j > tab.nColumns())
        return -1;
    int ierr = 0;
    if (static_cast<uint64_t>(curRow) == bBegin)
        ierr = fillBuffer(j);
    if (ierr < 0 || buffer[j].cval == 0)
        return -2;

    switch (buffer[j].ctype) {
    case ibis::BYTE: {
        val = (*(static_cast<array_t<signed char>*>(buffer[j].cval)))
            [curRow - bBegin];
        ierr = 0;
        break;}
    case ibis::UBYTE: {
        val = (*(static_cast<array_t<unsigned char>*>(buffer[j].cval)))
            [curRow - bBegin];
        ierr = 0;
        break;}
    case ibis::SHORT:
    case ibis::USHORT: {
        val = (*(static_cast<array_t<int16_t>*>(buffer[j].cval)))
            [curRow - bBegin];
        ierr = 0;
        break;}
    default: {
        ierr = -1;
        break;}
    }
    return ierr;
} // ibis::mensa::cursor::getColumnAsShort

int ibis::mensa::cursor::getColumnAsUShort(uint32_t j, uint16_t& val) const {
    if (curRow < 0 || curPart >= tab.parts.size() || j > tab.nColumns())
        return -1;
    int ierr = 0;
    if (static_cast<uint64_t>(curRow) == bBegin)
        ierr = fillBuffer(j);
    if (ierr < 0 || buffer[j].cval == 0)
        return -2;

    switch (buffer[j].ctype) {
    case ibis::BYTE:
    case ibis::UBYTE: {
        val = (*(static_cast<array_t<unsigned char>*>(buffer[j].cval)))
            [curRow - bBegin];
        ierr = 0;
        break;}
    case ibis::SHORT:
    case ibis::USHORT: {
        val = (*(static_cast<array_t<uint16_t>*>(buffer[j].cval)))
            [curRow - bBegin];
        ierr = 0;
        break;}
    default: {
        ierr = -1;
        break;}
    }
    return ierr;
} // ibis::mensa::cursor::getColumnAsUShort

int ibis::mensa::cursor::getColumnAsInt(uint32_t j, int32_t& val) const {
    if (curRow < 0 || curPart >= tab.parts.size() || j > tab.nColumns())
        return -1;
    int ierr = 0;
    if (static_cast<uint64_t>(curRow) == bBegin)
        ierr = fillBuffer(j);
    if (ierr < 0 || buffer[j].cval == 0)
        return -2;

    switch (buffer[j].ctype) {
    case ibis::BYTE: {
        val = (*(static_cast<array_t<signed char>*>(buffer[j].cval)))
            [curRow - bBegin];
        ierr = 0;
        break;}
    case ibis::UBYTE: {
        val = (*(static_cast<array_t<unsigned char>*>(buffer[j].cval)))
            [curRow - bBegin];
        ierr = 0;
        break;}
    case ibis::SHORT: {
        val = (*(static_cast<array_t<int16_t>*>(buffer[j].cval)))
            [curRow - bBegin];
        ierr = 0;
        break;}
    case ibis::USHORT: {
        val = (*(static_cast<array_t<uint16_t>*>(buffer[j].cval)))
            [curRow - bBegin];
        ierr = 0;
        break;}
    case ibis::INT:
    case ibis::UINT: {
        val = (*(static_cast<array_t<int32_t>*>(buffer[j].cval)))
            [curRow - bBegin];
        ierr = 0;
        break;}
    default: {
        ierr = -1;
        break;}
    }
    return ierr;
} // ibis::mensa::cursor::getColumnAsInt

int ibis::mensa::cursor::getColumnAsUInt(uint32_t j, uint32_t& val) const {
    if (curRow < 0 || curPart >= tab.parts.size() || j > tab.nColumns())
        return -1;
    int ierr = 0;
    if (static_cast<uint64_t>(curRow) == bBegin)
        ierr = fillBuffer(j);
    if (ierr < 0 || buffer[j].cval == 0)
        return -2;

    switch (buffer[j].ctype) {
    case ibis::BYTE:
    case ibis::UBYTE: {
        val = (*(static_cast<array_t<unsigned char>*>(buffer[j].cval)))
            [curRow - bBegin];
        ierr = 0;
        break;}
    case ibis::SHORT:
    case ibis::USHORT: {
        val = (*(static_cast<array_t<uint16_t>*>(buffer[j].cval)))
            [curRow - bBegin];
        ierr = 0;
        break;}
    case ibis::INT:
    case ibis::UINT: {
        val = (*(static_cast<array_t<uint32_t>*>(buffer[j].cval)))
            [curRow - bBegin];
        ierr = 0;
        break;}
    default: {
        ierr = -1;
        break;}
    }
    return ierr;
} // ibis::mensa::cursor::getColumnAsUInt

int ibis::mensa::cursor::getColumnAsLong(uint32_t j, int64_t& val) const {
    if (curRow < 0 || curPart >= tab.parts.size() || j > tab.nColumns())
        return -1;
    int ierr = 0;
    if (static_cast<uint64_t>(curRow) == bBegin)
        ierr = fillBuffer(j);
    if (ierr < 0 || buffer[j].cval == 0)
        return -2;

    switch (buffer[j].ctype) {
    case ibis::BYTE: {
        val = (*(static_cast<array_t<signed char>*>(buffer[j].cval)))
            [curRow - bBegin];
        ierr = 0;
        break;}
    case ibis::UBYTE: {
        val = (*(static_cast<array_t<unsigned char>*>(buffer[j].cval)))
            [curRow - bBegin];
        ierr = 0;
        break;}
    case ibis::SHORT: {
        val = (*(static_cast<array_t<int16_t>*>(buffer[j].cval)))
            [curRow - bBegin];
        ierr = 0;
        break;}
    case ibis::USHORT: {
        val = (*(static_cast<array_t<uint16_t>*>(buffer[j].cval)))
            [curRow - bBegin];
        ierr = 0;
        break;}
    case ibis::INT: {
        val = (*(static_cast<array_t<int32_t>*>(buffer[j].cval)))
            [curRow - bBegin];
        ierr = 0;
        break;}
    case ibis::UINT: {
        val = (*(static_cast<array_t<uint32_t>*>(buffer[j].cval)))
            [curRow - bBegin];
        ierr = 0;
        break;}
    case ibis::OID:
    case ibis::LONG:
    case ibis::ULONG: {
        val = (*(static_cast<array_t<int64_t>*>(buffer[j].cval)))
            [curRow - bBegin];
        ierr = 0;
        break;}
    default: {
        ierr = -1;
        break;}
    }
    return ierr;
} // ibis::mensa::cursor::getColumnAsLong

int ibis::mensa::cursor::getColumnAsULong(uint32_t j, uint64_t& val) const {
    if (curRow < 0 || curPart >= tab.parts.size() || j > tab.nColumns())
        return -1;
    int ierr = 0;
    if (static_cast<uint64_t>(curRow) == bBegin)
        ierr = fillBuffer(j);
    if (ierr < 0 || buffer[j].cval == 0)
        return -2;

    switch (buffer[j].ctype) {
    case ibis::BYTE:
    case ibis::UBYTE: {
        val = (*(static_cast<array_t<unsigned char>*>(buffer[j].cval)))
            [curRow - bBegin];
        ierr = 0;
        break;}
    case ibis::SHORT:
    case ibis::USHORT: {
        val = (*(static_cast<array_t<uint16_t>*>(buffer[j].cval)))
            [curRow - bBegin];
        ierr = 0;
        break;}
    case ibis::INT:
    case ibis::UINT: {
        val = (*(static_cast<array_t<uint32_t>*>(buffer[j].cval)))
            [curRow - bBegin];
        ierr = 0;
        break;}
    case ibis::OID:
    case ibis::LONG:
    case ibis::ULONG: {
        val = (*(static_cast<array_t<uint64_t>*>(buffer[j].cval)))
            [curRow - bBegin];
        ierr = 0;
        break;}
    default: {
        ierr = -1;
        break;}
    }
    return ierr;
} // ibis::mensa::cursor::getColumnAsULong

int ibis::mensa::cursor::getColumnAsFloat(uint32_t j, float& val) const {
    if (curRow < 0 || curPart >= tab.parts.size() || j > tab.nColumns())
        return -1;
    int ierr = 0;
    if (static_cast<uint64_t>(curRow) == bBegin)
        ierr = fillBuffer(j);
    if (ierr < 0 || buffer[j].cval == 0)
        return -2;

    switch (buffer[j].ctype) {
    case ibis::BYTE: {
        val = (*(static_cast<array_t<signed char>*>(buffer[j].cval)))
            [curRow - bBegin];
        ierr = 0;
        break;}
    case ibis::UBYTE: {
        val = (*(static_cast<array_t<unsigned char>*>(buffer[j].cval)))
            [curRow - bBegin];
        ierr = 0;
        break;}
    case ibis::SHORT: {
        val = (*(static_cast<array_t<int16_t>*>(buffer[j].cval)))
            [curRow - bBegin];
        ierr = 0;
        break;}
    case ibis::USHORT: {
        val = (*(static_cast<array_t<uint16_t>*>(buffer[j].cval)))
            [curRow - bBegin];
        ierr = 0;
        break;}
    case ibis::FLOAT: {
        val = (*(static_cast<array_t<float>*>(buffer[j].cval)))
            [curRow - bBegin];
        ierr = 0;
        break;}
    default: {
        ierr = -1;
        break;}
    }
    return ierr;
} // ibis::mensa::cursor::getColumnAsFloat

int ibis::mensa::cursor::getColumnAsDouble(uint32_t j, double& val) const {
    if (curRow < 0 || curPart >= tab.parts.size() || j > tab.nColumns())
        return -1;
    int ierr = 0;
    if (static_cast<uint64_t>(curRow) == bBegin)
        ierr = fillBuffer(j);
    if (ierr < 0 || buffer[j].cval == 0)
        return -2;

    switch (buffer[j].ctype) {
    case ibis::BYTE: {
        val = (*(static_cast<array_t<signed char>*>(buffer[j].cval)))
            [curRow - bBegin];
        ierr = 0;
        break;}
    case ibis::UBYTE: {
        val = (*(static_cast<array_t<unsigned char>*>(buffer[j].cval)))
            [curRow - bBegin];
        ierr = 0;
        break;}
    case ibis::SHORT: {
        val = (*(static_cast<array_t<int16_t>*>(buffer[j].cval)))
            [curRow - bBegin];
        ierr = 0;
        break;}
    case ibis::USHORT: {
        val = (*(static_cast<array_t<uint16_t>*>(buffer[j].cval)))
            [curRow - bBegin];
        ierr = 0;
        break;}
    case ibis::INT: {
        val = (*(static_cast<array_t<int32_t>*>(buffer[j].cval)))
            [curRow - bBegin];
        ierr = 0;
        break;}
    case ibis::UINT: {
        val = (*(static_cast<array_t<uint32_t>*>(buffer[j].cval)))
            [curRow - bBegin];
        ierr = 0;
        break;}
    case ibis::DOUBLE: {
        val = (*(static_cast<array_t<double>*>(buffer[j].cval)))
            [curRow - bBegin];
        ierr = 0;
        break;}
    default: {
        ierr = -1;
        break;}
    }
    return ierr;
} // ibis::mensa::cursor::getColumnAsDouble

int ibis::mensa::cursor::getColumnAsString(uint32_t j, std::string& val) const {
    if (curRow < 0 || curPart >= tab.parts.size() || j > tab.nColumns())
        return -1;
    int ierr = 0;
    if (static_cast<uint64_t>(curRow) == bBegin)
        ierr = fillBuffer(j);
    if (ierr < 0 || buffer[j].cval == 0)
        return -2;

    const uint32_t irow = static_cast<uint32_t>(curRow-bBegin);
    std::ostringstream oss;
    switch (buffer[j].ctype) {
    case ibis::BYTE: {
        oss << static_cast<int>
            ((*(static_cast<array_t<signed char>*>(buffer[j].cval)))[irow]);
        val = oss.str();
        ierr = 0;
        break;}
    case ibis::UBYTE: {
        oss << static_cast<unsigned>
            ((*(static_cast<array_t<unsigned char>*>(buffer[j].cval)))[irow]);
        val = oss.str();
        ierr = 0;
        break;}
    case ibis::SHORT: {
        oss << (*(static_cast<array_t<int16_t>*>(buffer[j].cval)))[irow];
        val = oss.str();
        ierr = 0;
        break;}
    case ibis::USHORT: {
        oss << (*(static_cast<array_t<uint16_t>*>(buffer[j].cval)))[irow];
        val = oss.str();
        ierr = 0;
        break;}
    case ibis::INT: {
        oss << (*(static_cast<array_t<int32_t>*>(buffer[j].cval)))[irow];
        val = oss.str();
        ierr = 0;
        break;}
    case ibis::UINT: {
        oss << (*(static_cast<array_t<uint32_t>*>(buffer[j].cval)))[irow];
        val = oss.str();
        ierr = 0;
        break;}
    case ibis::LONG: {
        oss << (*(static_cast<array_t<int64_t>*>(buffer[j].cval)))[irow];
        val = oss.str();
        ierr = 0;
        break;}
    case ibis::OID:
    case ibis::ULONG: {
        oss << (*(static_cast<array_t<uint64_t>*>(buffer[j].cval)))[irow];
        val = oss.str();
        ierr = 0;
        break;}
    case ibis::FLOAT: {
        oss << (*(static_cast<array_t<float>*>(buffer[j].cval)))[irow];
        val = oss.str();
        ierr = 0;
        break;}
    case ibis::DOUBLE: {
        oss << (*(static_cast<array_t<double>*>(buffer[j].cval)))[irow];
        ierr = 0;
        val = oss.str();
        break;}
    case ibis::CATEGORY:
    case ibis::TEXT: {
        const ibis::column* col =
            tab.parts[curPart]->getColumn(buffer[j].cname);
        if (col != 0) {
            static_cast<const ibis::text*>(col)->
                getString(static_cast<uint32_t>(curRow-pBegin), val);
            ierr = 0;
        }
        else {
            ierr = -1;
        }
        break;}
    case ibis::BLOB: {
        char *buf = 0;
        uint64_t sz = 0;
        const ibis::blob* blo = dynamic_cast<const ibis::blob*>
            (tab.parts[curPart]->getColumn(buffer[j].cname));
        if (blo != 0) {
            ierr = blo->getBlob(static_cast<uint32_t>(curRow-pBegin), buf, sz);
            if (ierr >= 0) {
                if (sz > 0) {
                    oss << "0x" << std::hex;
                    for (uint32_t i = 0; i < sz; ++ i)
                        oss << std::setprecision(2) << buf[i];
                    oss << std::dec;
                    val = oss.str();
                }
            }
        }
        else {
            ierr = -3;
        }
        break;}
    default: {
        ierr = -1;
        break;}
    }
    return ierr;
} // ibis::mensa::cursor::getColumnAsString

int
ibis::mensa::cursor::getColumnAsOpaque(uint32_t j, ibis::opaque& val) const {
    if (curRow < 0 || curPart >= tab.parts.size() || j > tab.nColumns())
        return -1;
    int ierr = 0;
    if (static_cast<uint64_t>(curRow) == bBegin)
        ierr = fillBuffer(j);
    if (ierr < 0 || buffer[j].cval == 0)
        return -2;

    const uint32_t irow = static_cast<uint32_t>(curRow-bBegin);
    std::ostringstream oss;
    switch (buffer[j].ctype) {
    case ibis::BYTE: {
        val.copy((const char*)
                 ((static_cast<array_t<signed char>*>
                   (buffer[j].cval))->begin()+irow), sizeof(char));
        ierr = 0;
        break;}
    case ibis::UBYTE: {
        val.copy((const char*)
                 ((static_cast<array_t<unsigned char>*>
                   (buffer[j].cval))->begin()+irow), sizeof(char));
        ierr = 0;
        break;}
    case ibis::SHORT: {
        val.copy((const char*)
                 ((static_cast<array_t<int16_t>*>(buffer[j].cval))
                  ->begin()+irow), sizeof(int16_t));
        ierr = 0;
        break;}
    case ibis::USHORT: {
        val.copy((const char*)
                 ((static_cast<array_t<uint16_t>*>(buffer[j].cval))
                  ->begin()+irow), sizeof(int16_t));
        ierr = 0;
        break;}
    case ibis::INT: {
        val.copy((const char*)
                 ((static_cast<array_t<int32_t>*>(buffer[j].cval))
                  ->begin()+irow), sizeof(int32_t));
        ierr = 0;
        break;}
    case ibis::UINT: {
        val.copy((const char*)
                 ((static_cast<array_t<uint32_t>*>(buffer[j].cval))
                  ->begin()+irow), sizeof(int32_t));
        ierr = 0;
        break;}
    case ibis::LONG: {
        val.copy((const char*)
                 ((static_cast<array_t<int64_t>*>(buffer[j].cval))
                  ->begin()+irow), sizeof(int64_t));
        ierr = 0;
        break;}
    case ibis::OID:
    case ibis::ULONG: {
        val.copy((const char*)
                 ((static_cast<array_t<uint64_t>*>(buffer[j].cval))
                  ->begin()+irow), sizeof(int64_t));
        break;}
    case ibis::FLOAT: {
        val.copy((const char*)
                 ((static_cast<array_t<float>*>(buffer[j].cval))
                  ->begin()+irow), sizeof(float));
        ierr = 0;
        break;}
    case ibis::DOUBLE: {
        val.copy((const char*)
                 ((static_cast<array_t<double>*>(buffer[j].cval))
                  ->begin()+irow), sizeof(double));
        ierr = 0;
        break;}
    case ibis::CATEGORY:
    case ibis::TEXT: {
        const ibis::column* col =
            tab.parts[curPart]->getColumn(buffer[j].cname);
        if (col != 0) {
            std::string tmp;
            ierr = col->getString(static_cast<uint32_t>(curRow-pBegin), tmp);
            if (ierr >= 0)
                val.copy(tmp.data(), tmp.size());
        }
        else {
            ierr = -1;
        }
        break;}
    case ibis::BLOB: {
        ibis::opaque tmp;
        const ibis::column* blo =
            tab.parts[curPart]->getColumn(buffer[j].cname);
        if (blo != 0) {
            ierr = blo->getOpaque(static_cast<uint32_t>(curRow-pBegin), tmp);
            if (ierr >= 0) {
                val.assign(tmp);
            }
        }
        else {
            ierr = -3;
        }
        break;}
    default: {
        ierr = -1;
        break;}
    }
    return ierr;
} // ibis::mensa::cursor::getColumnAsOpaque

/// Destructor for bufferElement.
ibis::mensa::cursor::bufferElement::~bufferElement() {
    switch (ctype) {
    default:
        break;
    case ibis::BLOB:
        delete static_cast<array_t<unsigned char>*>(cval);
        break;
    case ibis::BYTE:
        delete static_cast<array_t<signed char>*>(cval);
        break;
    case ibis::UBYTE:
        delete static_cast<array_t<unsigned char>*>(cval);
        break;
    case ibis::SHORT:
        delete static_cast<array_t<int16_t>*>(cval);
        break;
    case ibis::USHORT:
        delete static_cast<array_t<uint16_t>*>(cval);
        break;
    case ibis::INT:
        delete static_cast<array_t<int32_t>*>(cval);
        break;
    case ibis::UINT:
        delete static_cast<array_t<uint32_t>*>(cval);
        break;
    case ibis::LONG:
        delete static_cast<array_t<int64_t>*>(cval);
        break;
    case ibis::OID:
    case ibis::ULONG:
        delete static_cast<array_t<uint64_t>*>(cval);
        break;
    case ibis::FLOAT:
        delete static_cast<array_t<float>*>(cval);
        break;
    case ibis::DOUBLE:
        delete static_cast<array_t<double>*>(cval);
        break;
    }
} // ibis::mensa::cursor::bufferElement::~bufferElement

/// Ibis::liga does not own the data partitions and does not free the
/// resources in those partitions.
ibis::liga::~liga () {
    parts.clear();
} // ibis::liga::~liga

/// Create an object from an externally managed data partition.
ibis::liga::liga(ibis::part& p) : ibis::mensa() {
    if (p.nRows() == 0 || p.nColumns() == 0) return;
    parts.resize(1);
    parts[0] = &p;
    p.combineNames(naty);
    nrows = p.nRows();

    name_ = "T-";
    desc_ = "a simple container of data partition ";
    std::ostringstream oss;
    oss << "with " << p.nRows() << " row" << (p.nRows()>1 ? "s" : "")
        << " and " << p.nColumns() << " column"
        << (p.nColumns()>1 ? "s" : "");
    if (p.name() != 0 && *(p.name()) != 0) {
        name_ += p.name();
        desc_ += p.name();
    }
    else if (p.description() != 0 && *(p.description()) != 0) {
        unsigned sum =
            ibis::util::checksum(p.description(), std::strlen(p.description()));
        std::string tmp;
        ibis::util::int2string(tmp, sum);
        name_ += tmp;
        desc_ += p.description();
    }
    else { // produce a random name from the size of the data partition
        const unsigned v2 = (p.nColumns() ^ ibis::fileManager::iBeat());
        std::string tmp;
        ibis::util::int2string(tmp, p.nRows(), v2);
        desc_ += oss.str();
    }
    LOGGER(ibis::gVerbose > 1)
        << "liga -- constructed table " << name_ << " (" << desc_
        << ") from a partition " << oss.str();
} // ibis::liga::liga

/// Create an object from an external list of data partitions.  Note that
/// this object does not own the partitions and is not reponsible for
/// freeing the partitions.  It merely provide a container for the
/// partitions so that one can use the ibis::table API.
ibis::liga::liga(const ibis::partList &l) : ibis::mensa() {
    if (l.empty()) return;

    parts.insert(parts.end(), l.begin(), l.end());
    for (ibis::partList::const_iterator it = parts.begin();
         it != parts.end(); ++ it) {
        (*it)->combineNames(naty);
        nrows += (*it)->nRows();
    }
    if (! parts.empty()) {
        // take on the name of the first partition
        ibis::partList::const_iterator it = parts.begin();
        name_ = "T-";
        name_ += (*it)->name();
        if (desc_.empty()) {
            uint32_t mp = ((l.size() >> ibis::gVerbose) <= 1 ?
                         l.size() :
                         (ibis::gVerbose > 2 ? (1 << ibis::gVerbose) : 5));
            if (mp > l.size()) mp = l.size();
            desc_ = "a simple list of partition";
            if (l.size() > 1) desc_ += "s";
            desc_ += ": ";
            desc_ += parts[0]->name();
            uint32_t jp = 1;
            while (jp < mp) {
                desc_ += (jp+1 < parts.size() ? ", " : " and ");
                desc_ += parts[jp]->name();
                ++ jp;
            }
            if (jp < parts.size()) {
                std::ostringstream oss;
                oss << ", ... (" << parts.size()-jp << " skipped)";
                desc_ += oss.str();
            }
        }
    }
    LOGGER(ibis::gVerbose > 1 && ! name_.empty())
        << "liga -- constructed table " << name_ << " (" << desc_
        << ") from a list of " << l.size() << " data partition"
        << (l.size()>1 ? "s" : "") << ", with " << naty.size() << " column"
        << (naty.size()>1 ? "s" : "") << " and " << nrows << " row"
        << (nrows>1 ? "s" : "");
} // ibis::liga::liga

ibis::table* ibis::table::create(ibis::part& p) {
    return new ibis::liga(p);
} // ibis::table::create

ibis::table* ibis::table::create(const ibis::partList& pl) {
    return new ibis::liga(pl);
} // ibis::table::create

/// If the incoming directory name is nil or an empty string, it attempts
/// to use the directories specified in the configuration files.
ibis::table* ibis::table::create(const char* dir) {
    return new ibis::mensa(dir);
} // ibis::table::create

ibis::table* ibis::table::create(const char* dir1, const char* dir2) {
    if (dir1 == 0 || *dir1 == 0)
        return 0;
    else if (dir2 == 0 || *dir2 == 0)
        return new ibis::mensa(dir1);
    else
        return new ibis::mensa(dir1, dir2);
} // ibis::table::create

/// Parse the incoming string as an order-by clause.  An order-by clause is
/// a list of column names where each name is optionally followed by a
/// keyword ASC or DESC.  The corresponding element of direc is set true
/// for ASC and false for DESC.  The unspecified elements are assumed to be
/// ASC per SQL convention.
///
/// @note Some bytes in the incoming string may be turned into nil (0) to
/// mark the end of names.
void ibis::table::parseOrderby(char* in, ibis::table::stringArray& out,
                               std::vector<bool>& direc) {
    char* ptr1 = in;
    char* ptr2;
    while (*ptr1 != 0 && isspace(*ptr1) != 0) ++ ptr1; // leading space
    // since SQL names can not contain space, quotes must be for the whole
    // list of names
    if (*ptr1 == '\'') {
        ++ ptr1; // skip opening quote
        ptr2 = strchr(ptr1, '\'');
        if (ptr2 > ptr1)
            *ptr2 = 0; // string terminates here
    }
    else if (*ptr1 == '"') {
        ++ ptr1;
        ptr2 = strchr(ptr1, '"');
        if (ptr2 > ptr1)
            *ptr2 = 0;
    }

    while (*ptr1 != 0) {
        for (ptr2 = ptr1; *ptr2 == '_' || isalnum(*ptr2) != 0; ++ ptr2);
        while (*ptr2 == '(') {
            int nesting = 1;
            for (++ ptr2; *ptr2 != 0 && nesting > 0; ++ ptr2) {
                nesting -= (*ptr2 == ')');
                nesting += (*ptr2 == '(');
            }
            while (*ptr2 != 0 && *ptr2 != ',' && *ptr2 != ';' && *ptr2 != '(')
                ++ ptr2;
        }
        if (*ptr2 == 0) {
            out.push_back(ptr1);
            direc.push_back(true);
        }
        else if (ispunct(*ptr2)) {
            *ptr2 = 0;
            out.push_back(ptr1);
            direc.push_back(true);
            ++ ptr2;
        }
        else if (isspace(*ptr2)) {
            // skip over spaces
            *ptr2 = 0;
            out.push_back(ptr1);
            for (++ ptr2; isspace(*ptr2); ++ ptr2);
            if ((ptr2[0] == 'a' || ptr2[0] == 'A') &&
                (ptr2[1] == 's' || ptr2[1] == 'S') &&
                (ptr2[2] == 'c' || ptr2[2] == 'c') &&
                (ptr2[3] == 0 || isspace(ptr2[3]) || ispunct(ptr2[3]))) {
                direc.push_back(true);
                ptr2 += 3;
            }
            else if ((ptr2[0] == 'd' || ptr2[0] == 'D') &&
                     (ptr2[1] == 'e' || ptr2[1] == 'E') &&
                     (ptr2[2] == 's' || ptr2[2] == 'S') &&
                     (ptr2[3] == 'c' || ptr2[3] == 'C') &&
                     (ptr2[4] == 0 || isspace(ptr2[4]) || ispunct(ptr2[4]))) {
                direc.push_back(false);
                ptr2 += 4;
            }
        }
        else {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- table::parseOrderby can not part string \""
                << ptr1 << "\" into a column name or a function, skip till "
                "first character after the next comma or space";

            while (*ptr2 != 0 && ispunct(*ptr2) == 0 && isspace(*ptr2) == 0)
                ++ ptr2;
            if (*ptr2 != 0) ++ ptr2;
            while (*ptr2 != 0 && isspace(*ptr2) != 0) ++ ptr2;
        }
        // skip spaces and punctuations
        for (ptr1 = ptr2; *ptr1 && (ispunct(*ptr1) || isspace(*ptr1)); ++ ptr1);
    }
} // ibis::table::parseOrderby

/// Parse the incoming string into a set of names.  Some bytes in the
/// incoming string may be turned into nil (0) to mark the end of names or
/// functions.  Newly discovered tokens will be appended to out.
void ibis::table::parseNames(char* in, ibis::table::stringVector& out) {
    char* ptr1 = in;
    char* ptr2;
    while (*ptr1 != 0 && isspace(*ptr1) != 0) ++ ptr1; // leading space
    // since SQL names can not contain space, quotes must be for the whole
    // list of names
    if (*ptr1 == '\'') {
        ++ ptr1; // skip opening quote
        ptr2 = strchr(ptr1, '\'');
        if (ptr2 > ptr1)
            *ptr2 = 0; // string terminates here
    }
    else if (*ptr1 == '"') {
        ++ ptr1;
        ptr2 = strchr(ptr1, '"');
        if (ptr2 > ptr1)
            *ptr2 = 0;
    }

    while (*ptr1 != 0) {
        for (ptr2 = ptr1; *ptr2 == '_' || isalnum(*ptr2) != 0; ++ ptr2);
        while (*ptr2 == '(') {
            int nesting = 1;
            for (++ ptr2; *ptr2 != 0 && nesting > 0; ++ ptr2) {
                nesting -= (*ptr2 == ')');
                nesting += (*ptr2 == '(');
            }
            while (*ptr2 != 0 && *ptr2 != ',' && *ptr2 != ';' && *ptr2 != '(')
                ++ ptr2;
        }
        if (*ptr2 == 0) {
            out.push_back(ptr1);
        }
        else if (ispunct(*ptr2) || isspace(*ptr2)) {
            *ptr2 = 0;
            out.push_back(ptr1);
            ++ ptr2;
        }
        else {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- table::parseNames can not part string \"" << ptr1
                << "\" into a column name or a function, skip till first "
                "character after the next comma or space";

            while (*ptr2 != 0 && ispunct(*ptr2) == 0 && isspace(*ptr2) == 0)
                ++ ptr2;
            if (*ptr2 != 0) ++ ptr2;
            while (*ptr2 != 0 && isspace(*ptr2) != 0) ++ ptr2;
        }
        // skip spaces and punctuations
        for (ptr1 = ptr2; *ptr1 && (ispunct(*ptr1) || isspace(*ptr1)); ++ ptr1);
    }
} // ibis::table::parseNames

/// Parse the incoming string into a set of names.  Some bytes in the
/// incoming string may be turned into nil (0) to mark the end of names or
/// functions.  Newly discovered tokens will be appended to out.
void ibis::table::parseNames(char* in, ibis::table::stringArray& out) {
    char* ptr1 = in;
    char* ptr2;
    while (*ptr1 != 0 && isspace(*ptr1) != 0) ++ ptr1; // leading space
    // since SQL names can not contain space, quotes must be for the whole
    // list of names
    if (*ptr1 == '\'') {
        ++ ptr1; // skip opening quote
        ptr2 = strchr(ptr1, '\'');
        if (ptr2 > ptr1)
            *ptr2 = 0; // string terminates here
    }
    else if (*ptr1 == '"') {
        ++ ptr1;
        ptr2 = strchr(ptr1, '"');
        if (ptr2 > ptr1)
            *ptr2 = 0;
    }

    while (*ptr1 != 0) {
        for (ptr2 = ptr1; *ptr2 == '_' || isalnum(*ptr2) != 0; ++ ptr2);
        while (*ptr2 == '(') {
            int nesting = 1;
            for (++ ptr2; *ptr2 != 0 && nesting > 0; ++ ptr2) {
                nesting -= (*ptr2 == ')');
                nesting += (*ptr2 == '(');
            }
            while (*ptr2 != 0 && *ptr2 != ',' && *ptr2 != ';' && *ptr2 != '(')
                ++ ptr2;
        }
        if (*ptr2 == 0) {
            out.push_back(ptr1);
        }
        else if (ispunct(*ptr2) || isspace(*ptr2)) {
            *ptr2 = 0;
            out.push_back(ptr1);
            ++ ptr2;
        }
        else {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- table::parseNames can not part string \"" << ptr1
                << "\" into a column name or a function, skip till first "
                "character after the next comma or space";

            while (*ptr2 != 0 && ispunct(*ptr2) == 0 && isspace(*ptr2) == 0)
                ++ ptr2;
            if (*ptr2 != 0) ++ ptr2;
            while (*ptr2 != 0 && isspace(*ptr2) != 0) ++ ptr2;
        }
        // skip spaces and punctuations
        for (ptr1 = ptr2; *ptr1 && (ispunct(*ptr1) || isspace(*ptr1)); ++ ptr1);
    }
} // ibis::table::parseNames

/// Is the given string a valid FastBit name for a data column?
bool ibis::table::isValidName(const char *nm) {
    if (nm == 0 || *nm == 0) return false;
    if (! (*nm == '_' || (*nm >= 'a' && *nm <= 'z') ||
           (*nm >= 'A' && *nm <= 'Z'))) return false;
    for (++ nm; *nm != 0; ++ nm) {
        if (*nm == '_'  || (*nm >= 'a' && *nm <= 'z') ||
            (*nm >= 'A' && *nm <= 'Z') || (*nm >= '0' && *nm <= '9') ||
            *nm == '[' || *nm == ']' || *nm == '.') {
            ;
        }
        else if (*nm == '-' && nm[1] == '>') {
            ++ nm;
        }
        else {
            return false;
        }
    }
    return true;
} // ibis::table::isValidName

/// Remove unallowed characters from the given string to produce a
/// valid column name.  This function will not allocate new memory,
/// therefore, if the incoming string is nil, nothing is done.
void ibis::table::consecrateName(char *nm) {
    if (nm == 0 || *nm == 0) return;
    // 1st character must be either _ or one of the 26 English alphabets
    if (! (*nm == '_' || (*nm >= 'a' && *nm <= 'z') ||
           (*nm >= 'A' && *nm <= 'Z'))) {
        short j = (*nm % 27);
        if (j < 26)
            *nm = 'A' + j;
        else
            *nm = '_';
    }
    ++ nm;
    char *ptr = nm;
    while (*ptr != 0) {
        if (*ptr == '_'  || (*ptr >= 'a' && *ptr <= 'z') ||
            (*ptr >= 'A' && *ptr <= 'Z') || (*ptr >= '0' && *ptr <= '9') ||
            *ptr == '[' || *ptr == ']' || *ptr == '.') {
            *nm = *ptr;
            ++ ptr;
            ++ nm;
        }
        else if (*ptr == '-' && ptr[1] == '>') {
            *nm = *ptr;
            ++ ptr;
            ++ nm;
            *nm = *ptr;
            ++ ptr;
            ++ nm;
        }
        else {
            ++ ptr;
        }
    }
    while (nm < ptr) {
        *nm = 0;
        ++ nm;
    }
} // ibis::table::consecrateName

ibis::table* ibis::table::groupby(const char* str) const {
    stringArray lst;
    char* buf = 0;
    if (str != 0 && *str != 0) {
        buf = new char[std::strlen(str)+1];
        strcpy(buf, str);
        parseNames(buf, lst);
    }
    ibis::table* res = groupby(lst);
    delete [] buf;
    return res;
} // ibis::table::groupby

void ibis::table::orderby(const char* str) {
    stringArray lst;
    std::vector<bool> direc;
    char* buf = 0;
    if (str != 0 && *str != 0) {
        buf = new char[std::strlen(str)+1];
        strcpy(buf, str);
        parseOrderby(buf, lst, direc);
    }
    orderby(lst, direc);
    delete [] buf;
} // ibis::table::orderby

/// This implementation of the member function uses the class function
/// ibis::table::select that takes the similar arguments along with the
/// full list of data partitions to work with.  This function returns a nil
/// table when the select clause is empty or nil.
ibis::table*
ibis::table::select(const char* sel, const ibis::qExpr* cond) const {
    if (sel == 0 || *sel == 0 || cond == 0 || nRows() == 0 || nColumns() == 0) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- table::select requires a non-empty table, "
            "a valid select clause, and a valid where clause";
        return 0;
    }

    ibis::constPartList parts;
    int ierr = getPartitions(parts);
    if (ierr <= 0) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- table::select failed to getPartitions, ierr="
            << ierr;
        return 0;
    }

    try {
        return ibis::table::select(parts, sel, cond);
    }
    catch (const ibis::bad_alloc &e) {
        if (ibis::gVerbose >= 0) {
            ibis::util::logger lg;
            lg() << "Warning -- table::select absorbed a bad_alloc exception ("
                 << e.what() << "), will return a nil pointer";
            if (ibis::gVerbose > 0)
                ibis::fileManager::instance().printStatus(lg());
        }
        ibis::util::emptyCache();
    }
    catch (const std::exception &e) {
        if (ibis::gVerbose >= 0) {
            ibis::util::logger lg;
            lg() << "Warning -- table::select absorbed a std::exception ("
                 << e.what() << "), will return a nil pointer";
            if (ibis::gVerbose > 0)
                ibis::fileManager::instance().printStatus(lg());
        }
        ibis::util::emptyCache();
    }
    catch (const char *s) {
        if (ibis::gVerbose >= 0) {
            ibis::util::logger lg;
            lg() << "Warning -- table::select absorbed a string exception ("
                 << s << "), will return a nil pointer";
            if (ibis::gVerbose > 0)
                ibis::fileManager::instance().printStatus(lg());
        }
        ibis::util::emptyCache();
    }
    catch (...) {
        if (ibis::gVerbose >= 0) {
            ibis::util::logger lg;
            lg() << "Warning -- table::select absorbed an unknown exception, "
                "will return a nil pointer";
            if (ibis::gVerbose > 0)
                ibis::fileManager::instance().printStatus(lg());
        }
        ibis::util::emptyCache();
    }
    return 0;
} // ibis::table::select

/// It iterates through all data partitions to compute the number of hits.
int64_t ibis::table::computeHits(const ibis::constPartList& pts,
                                 const char* cond) {
    if (cond == 0 || *cond == 0) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- table::computeHits requires a query expression";
        return -1;
    }

    int ierr;
    uint64_t nhits = 0;
    ibis::countQuery qq;
    ierr = qq.setWhereClause(cond);
    if (ierr < 0)
        return ierr;

    for (ibis::constPartList::const_iterator it = pts.begin();
         it != pts.end(); ++ it) {
        ierr = qq.setPartition(*it);
        if (ierr < 0) continue;
        ierr = qq.evaluate();
        if (ierr == 0) {
            nhits += qq.getNumHits();
        }
        else if (ibis::gVerbose > 1) {
            ibis::util::logger lg;
            lg() << "Warning -- table::computeHits failed to evaluate \""
                 << cond << "\" on data partition " << (*it)->name()
                 << ", query::evaluate returned " << ierr;
        }
    }
    return nhits;
} // ibis::table::computeHits

/// It iterates through all data partitions to compute the number of hits.
int64_t ibis::table::computeHits(const ibis::constPartList& pts,
                                 const ibis::qExpr* cond) {
    if (cond == 0) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- table::computeHits requires a query expression";
        return -1;
    }

    int ierr;
    uint64_t nhits = 0;
    ibis::countQuery qq;
    ierr = qq.setWhereClause(cond);
    if (ierr < 0)
        return ierr;

    for (ibis::constPartList::const_iterator it = pts.begin();
         it != pts.end(); ++ it) {
        ierr = qq.setPartition(*it);
        if (ierr < 0) continue;
        ierr = qq.evaluate();
        if (ierr == 0) {
            nhits += qq.getNumHits();
        }
        else if (ibis::gVerbose > 1) {
            ibis::util::logger lg;
            lg() << "Warning -- table::computeHits failed to evaluate \""
                 << *cond << "\" on data partition " << (*it)->name()
                 << ", query::evaluate returned " << ierr;
        }
    }
    return nhits;
} // ibis::table::computeHits
