//File: $Id$
// Author: John Wu <John.Wu at ACM.org>
// Copyright (c) 2000-2016 the Regents of the University of California
//
// Purpose: implementation of the two version of text fields ibis::category
// and ibis::text
//
#if defined(_WIN32) && defined(_MSC_VER)
#pragma warning(disable:4786)   // some identifier longer than 256 characters
#endif

#include "part.h"
#include "category.h"
#include "irelic.h"     // ibis::relic
#include "ikeywords.h"

#include <algorithm>    // std::copy
#include <memory>       // std::unique_ptr

#define FASTBIT_SYNC_WRITE 1 

////////////////////////////////////////////////////////////////////////
// functions for ibis::category
ibis::category::category(const part* tbl, FILE* file)
    : text(tbl, file), dic() {
#ifdef FASTBIT_EAGER_INIT
    prepareMembers();
    lower = 1;
    upper = dic.size();
#endif
} // ibis::category::category

/// Construct a category object from a name.
ibis::category::category(const part* tbl, const char* name)
    : text(tbl, name, ibis::CATEGORY), dic() {
#ifdef FASTBIT_EAGER_INIT
    prepareMembers();
    lower = 1;
    upper = dic.size();
#endif
} // ibis::category::category

/// Copy constructor.  Copy from a collumn object of the type CATEGORY.
ibis::category::category(const ibis::column& col) : ibis::text(col), dic() {
    if (m_type != ibis::CATEGORY) {
        throw ibis::bad_alloc("Must be type CATEGORY -- category::ctor"
                              IBIS_FILE_LINE);
    }
#ifdef FASTBIT_EAGER_INIT
    prepareMembers();
    lower = 1;
    upper = dic.size();
#endif
} // ibis::category::category

/// Construct a categorical column that has only one possible value.  Also
/// builds the corresponding index.
ibis::category::category(const part* tbl, const char* name,
                         const char* value, const char* dir,
                         uint32_t nevt)
    : text(tbl, name, ibis::CATEGORY), dic() {
    dic.insert(value);
    lower = 1;
    upper = 1;
    std::string df = (dir ? dir : tbl->currentDataDir());
    df += FASTBIT_DIRSEP;
    df += name;
    df += ".dic";
    dic.write(df.c_str());
    if (nevt == 0) nevt = tbl->nRows();
    if (dir == 0)  dir  = tbl->currentDataDir();
    if (nevt > 0 && dir != 0) { // generate a trivial index
        ibis::direkte rlc(this, 1, nevt);
        rlc.write(dir);
    }
} // ibis::category::category

/// Destructor.  It also writes the dictionary to a file if the dictionary
/// file is currently empty but the in-memory dictionary is not.
ibis::category::~category() {
    unloadIndex();
    if (dic.size() > 0) {
        std::string dname;
        dataFileName(dname);
        if (! dname.empty()) {
            dname += ".dic";
            if (ibis::util::getFileSize(dname.c_str()) <= 0)
                dic.write(dname.c_str());
        }
    }
} // ibis::category::~category

ibis::array_t<uint32_t>*
ibis::category::selectUInts(const ibis::bitvector& mask) const {
    if (idx == 0)
        prepareMembers();

    std::string fname;
    bool tryintfile = (0 != dataFileName(fname));
    if (tryintfile) {
        fname += ".int";
        tryintfile = (thePart->nRows() ==
                      (ibis::util::getFileSize(fname.c_str()) >> 2));
    }
    if (tryintfile) {
        std::unique_ptr< ibis::array_t<uint32_t> >
            tmp(new ibis::array_t<uint32_t>);
        if (selectValuesT(fname.c_str(), mask, *tmp) >= 0)
            return tmp.release();
    }

    indexLock lock(this, "category::selectUInts");
    if (idx != 0) {
        const ibis::direkte *dir = dynamic_cast<const ibis::direkte*>(idx);
        if (dir != 0)
            return dir->keys(mask);
        const ibis::relic *rlc = dynamic_cast<const ibis::relic*>(idx);
        if (rlc != 0)
            return rlc->keys(mask);
    }

    LOGGER(ibis::gVerbose >= 0)
        << "Warning -- category[" << (thePart ? thePart->name() : "?") << '.'
        << m_name << "]::selectUInts failed the .int option and .idx option";
    return 0;
} // ibis::category::selectUInts

/// Retrieve the string values from the rows marked 1 in mask.
///
/// @note FastBit does not track the memory usage of neither std::vector
/// nor std::string.
std::vector<std::string>*
ibis::category::selectStrings(const ibis::bitvector& mask) const {
    if (mask.cnt() == 0)
        return new std::vector<std::string>();
    if (dic.size() == 0 || idx == 0)
        prepareMembers();
    if (dic.size() == 0) // return empty strings
        return new std::vector<std::string>(mask.cnt(), "");

    if (idx != 0 && (idx->getBitvector(0) == 0 ||
                     idx->getBitvector(0)->cnt() == 0)) {
        // an index exist and there is no null value, try to see if all
        // values are the same
        unsigned j = 1;
        while (j < idx->numBitvectors()) {
            if (idx->getBitvector(j) == 0) {
                ++ j;
            }
            else {
                unsigned nb = idx->getBitvector(j)->cnt();
                if (nb == 0)
                    ++ j;
                else if (nb == mask.size())
                    break;
                else
                    j = UINT_MAX;
            }
        }
        if (j <= dic.size())
            return new std::vector<std::string>(mask.cnt(), dic[j]);
    }

    unsigned int opt = 0; // 0: raw data, 1: int file, 2: idx
    std::string fname;
    bool hasbase = (0 != dataFileName(fname));
    float rawdata = (hasbase ? ibis::util::getFileSize(fname.c_str()) : -1.0);
    float intfile = 0.0, idxfile = 0.0;
    if (hasbase) { // check the size of the .int file
        fname += ".int";
        if (thePart->nRows() ==
            (ibis::util::getFileSize(fname.c_str()) >> 2))
            intfile = 4.0 * thePart->nRows();
    }
    if (idx != 0) {
        idxfile = idx->sizeInBytes();
        idxfile *= ibis::util::log2(idx->numBitvectors());
    }

    if (rawdata <= 0.0 && intfile <= 0.0 && idxfile <= 0.0)
        return 0;
    if (rawdata > 0.0) {
        if (intfile > 0.0 && intfile < rawdata) {
            if (idxfile > 0.0 && idxfile < intfile) {
                opt = 2;
            }
            else {
                opt = 1;
            }
        }
        else if (idxfile > 0.0 && idxfile < rawdata) {
            opt = 2;
        }
        else {
            opt = 0;
        }
    }
    else if (intfile > 0.0) {
        if (idxfile > 0.0 && idxfile < intfile) {
            opt = 2;
        }
        else {
            opt = 1;
        }
    }
    else if (idxfile > 0.0) {
        opt = 2;
    }

    if (opt > 0) {
        std::unique_ptr< ibis::array_t<uint32_t> >
            keys(new ibis::array_t<uint32_t>);
        if (opt == 1) {
            (void) selectValuesT(fname.c_str(), mask, *keys);
        }
        else {
            const ibis::direkte *dir = dynamic_cast<const ibis::direkte*>(idx);
            if (dir != 0) {
                keys.reset(dir->keys(mask));
            }
            else {
                const ibis::relic *rlc = dynamic_cast<const ibis::relic*>(idx);
                if (rlc != 0)
                    keys.reset(rlc->keys(mask));
            }
        }
        if (keys->size() == mask.cnt()) {
            std::unique_ptr< std::vector<std::string> >
                strings(new std::vector<std::string>());
            strings->reserve(keys->size());
            for (unsigned i = 0; i < keys->size(); ++i) {
                const char *ptr = dic[(*keys)[i]];
                strings->push_back(ptr!=0 ? ptr : "");
            }
            return strings.release();
        }
    }

    // the option to read the strings from the raw data file
    return ibis::text::selectStrings(mask);
} // ibis::category::selectStrings

/// A function to read the dictionary and load the index.
///
/// This is a const function because it only manipulates mutable data
/// members.  It is callable from const member functions.
void ibis::category::prepareMembers() const {
    mutexLock lock(this, "category::prepareMembers");
    if (dic.size() == 0) {
        readDictionary();
    }
    if (dic.size() > 0 && idx != 0) return;
    if (thePart == 0) return;

    writeLock wlock(this, "category::prepareMembers");
    if (idx == 0 && thePart->currentDataDir() != 0) {
        // attempt to read the index file
        std::string idxf = thePart->currentDataDir();
        idxf += FASTBIT_DIRSEP;
        idxf += m_name;
        idxf += ".idx";
        // construct a dummy index so that we can invoke the function read
        idx = new ibis::direkte
            (this, static_cast<ibis::fileManager::storage*>(0));
        if (static_cast<ibis::direkte*>(idx)->read(idxf.c_str()) < 0 ||
            idx->getNRows() != thePart->nRows()) {
            delete idx;
            idx = 0;
            ibis::fileManager::instance().flushFile(idxf.c_str());
        }
    }

    if (idx == 0 || idx->getNRows() != thePart->nRows()) {
        delete idx;
        idx = 0;
        (void) fillIndex();
    }

    if ((idx == 0 || dic.size() == 0) &&
        thePart->getMetaTag(m_name.c_str()) != 0) {
        ibis::category tmp(thePart, m_name.c_str(),
                           thePart->getMetaTag(m_name.c_str()),
                           static_cast<const char*>(0),
                           thePart->nRows());
        readDictionary(); // in case the index was accidentially removed
        idx = tmp.idx;
        tmp.idx = 0;
    }
} // ibis::category::prepareMembers

/// Read the dictionary from the specified directory.  If the incoming
/// argument is nil, the current directory of the data partition is used.
void ibis::category::readDictionary(const char *dir) const {
    std::string fnm;
    if (dir != 0 && *dir != 0) {
        fnm = dir;
    }
    else if (thePart != 0) { // default to the current dictionary
        if (thePart->currentDataDir() != 0)
            fnm = thePart->currentDataDir();
        else
            return;
    }
    else {
        return;
    }
    fnm += FASTBIT_DIRSEP;
    fnm += m_name;
    fnm += ".dic"; // suffix of the dictionary
    int ierr = dic.read(fnm.c_str());
    LOGGER(ierr < 0 && ibis::gVerbose > 2)
        << "Warning -- category[" << fullname()
        << "] failed to read dictionary file " << fnm << ", ierr = "
        << ierr;
    if (ierr >= 0 && ibis::gVerbose > 5) {
        ibis::util::logger lg;
        lg() << "Dictionary from " << fnm << " for " << fullname() << "\n";
        dic.toASCII(lg());
    }
} // ibis::category::readDictionary

/// Build an ibis::direkte index using the existing primary data.
/// If the dictionary exists and the size is one, it builds a dummy index.
/// Otherwise, it reads the primary data file to update the dictionary and
/// complete a new ibis::direkte index.
///
/// @note It also writes the index to the same directory.
ibis::direkte* ibis::category::fillIndex(const char *dir) const {
    std::string dirstr;
    if (dir != 0 && *dir != 0) {
        // the name may be a filename instead of directory name
        unsigned ldir = std::strlen(dir);
        std::string idx = m_name;
        idx += ".idx";
        if (ldir > idx.size()) {
            unsigned dlen = ldir - idx.size();
            if (0 == std::strcmp(dir+dlen, idx.c_str())) {
                if (dir[dlen-1] == '/'
#if defined(_WIN32) && defined(_MSC_VER)
                    || dir[dlen-1] == '\\'
#endif
                    ) {
                    -- dlen;
                    for (unsigned i = 0; i < dlen; ++ i)
                        dirstr += dir[i];
                    dir = dirstr.c_str();
                }
            }
        }
        else if (ldir > m_name.size()) {
            unsigned dlen = ldir - m_name.size();
            if (0 == std::strcmp(dir+dlen, m_name.c_str()) &&
                dir[dlen-1] == '/'
#if defined(_WIN32) && defined(_MSC_VER)
                || dir[dlen-1] == '\\'
#endif
                ) {
                -- dlen;
                for (unsigned i = 0; i < dlen; ++ i)
                    dirstr += dir[i];
                dir = dirstr.c_str();
            }
        }
    }
    else if (thePart != 0) {
        dir = thePart->currentDataDir();
    }
    if (dir == 0) return 0;
    if (dic.size() == 0)
        readDictionary(dir);

    std::string evt = "category";
    if (ibis::gVerbose > 1) {
        evt += '[';
        evt += (thePart ? thePart->name() : "?");
        evt += '.';
        evt += m_name;
        evt += ']';
    }
    evt += "::fillIndex";
    if (ibis::gVerbose > 2) {
        evt += '(';
        evt += dir;
        evt += ')';
    }

    ibis::direkte *rlc = 0;
    if (dic.size() == 1) { // assume every entry has the given value
        rlc = new ibis::direkte(this, 1, thePart->nRows());
    }
    else { // actually read the raw data to build an index
        const bool iscurrent =
            (std::strcmp(dir, thePart->currentDataDir()) == 0 &&
             thePart->getStateNoLocking() != ibis::part::PRETRANSITION_STATE);
        array_t<uint32_t> ints;
        std::string raw = (dir ? dir : thePart->currentDataDir());
        raw += FASTBIT_DIRSEP;
        raw += m_name; // primary data file name
        std::string intfile = raw;
        intfile += ".int";
        if (dic.size() > 0) // read .int file only if a dictionary is present
            ints.read(intfile.c_str());
        if (ints.size() == 0 ||
            (iscurrent && ints.size() < thePart->nRows())) {
            int fraw = UnixOpen(raw.c_str(), OPEN_READONLY);
            if (fraw < 0) {
                LOGGER(ibis::gVerbose > 1)
                    << "Warning -- " << evt << " failed to open data file "
                    << raw;
                return 0;
            }
            IBIS_BLOCK_GUARD(UnixClose, fraw);
#if defined(_WIN32) && defined(_MSC_VER)
            (void)_setmode(fraw, _O_BINARY);
#endif

            int ret;
            ibis::fileManager::buffer<char> mybuf;
            char *buf = mybuf.address();
            uint32_t nbuf = mybuf.size();
            do {
                array_t<uint32_t> tmp;
                ret = string2int(fraw, dic, nbuf, buf, tmp);
                if (ret > 0) {
                    if (! ints.empty())
                        ints.insert(ints.end(), tmp.begin(), tmp.end());
                    else
                        ints.swap(tmp);
                }
            } while (ret > 0 &&
                     (! iscurrent || ints.size() < thePart->nRows()));
        }
        if (iscurrent) {
            if (ints.size() > thePart->nRows()) {
                unsigned cnt = 0;
                const unsigned nints = ints.size();
                for (unsigned i = 0; i < nints; ++ i)
                    cnt += (ints[i] == 0);
                if (cnt + thePart->nRows() == nints) {
                    LOGGER(ibis::gVerbose > 1)
                        << "Warning -- " << evt << " found " << nints
                        << " strings while expecting " << thePart->nRows()
                        << "; but the extra " << cnt
                        << " strings are nulls, will remove the nulls";

                    cnt = 0;
                    for (unsigned i = 0; i < nints; ++ i) {
                        if (ints[i] != 0) {
                            ints[cnt] = ints[i];
                            ++ cnt;
                        }
                    }
                }
                else {
                    LOGGER(ibis::gVerbose > 1)
                        << "Warning -- " << evt << " found " << nints
                        << " strings while expecting " << thePart->nRows()
                        << ", truncating the list of values";
                }
            }
            else if (ints.size() < thePart->nRows()) {
                LOGGER(ibis::gVerbose > 0)
                    << "Warning -- " << evt << " found only " << ints.size()
                    << " string value" << (ints.size() > 1 ? "s" : "")
                    << ", expected " << thePart->nRows()
                    << ", assume the remaining entries are nulls";
                ints.insert(ints.end(), thePart->nRows() - ints.size(), 0);
            }
            if (ints.size() != thePart->nRows())
                ints.resize(thePart->nRows());

            try {
                if ((ints.size() >> ibis::gVerbose) == 0) {
                    // reorder dictionary and ints if the dictioinary is small
                    ibis::array_t<uint32_t> o2n;
                    dic.sort(o2n);
                    if (! o2n.isSorted()) {
                        const uint32_t nints = ints.size();
                        for (uint32_t j = 0; j < nints; ++ j)
                            ints[j] = o2n[ints[j]];
                    }
                }
            }
            catch (...) {
                LOGGER(ibis::gVerbose > 5)
                    << evt << " did not find enough space to reorder the "
                    "dictionary entries, continue with the existing order";
            }
            ints.write(intfile.c_str());
        }
        if (rlc != 0) {
            (void) rlc->append(ints);
        }
        else {
            rlc = new ibis::direkte(this, 1+dic.size(), ints);
        }
    }

    if (rlc) {
        rlc->write(dir);
        if (dir == thePart->currentDataDir() ||
            (thePart->currentDataDir() !=0 &&
             std::strcmp(dir, thePart->currentDataDir()) == 0)) {
            idx = rlc;
        }
        else {
            delete rlc;
            rlc = 0;
        }
    }

    if (ibis::gVerbose > 6) {
        ibis::util::logger lg;
        lg() << evt << " constructed the following dictionary\n";
        dic.toASCII(lg());
    }
    std::string dicfile = (dir ? dir : thePart->currentDataDir());
    dicfile += FASTBIT_DIRSEP;
    dicfile += m_name;
    dicfile += ".dic";
    dic.write(dicfile.c_str());
    return rlc;
} // ibis::category::fillIndex

/// Return a pointer to the dictionary used for the categorical values.
const ibis::dictionary* ibis::category::getDictionary() const {
    if (dic.size() == 0) {
        prepareMembers();
    }
    return &dic;
}

/// Replace the dictionary with the incoming one.  The incoming dictionary
/// is expected to contain more words than the existing one.  If a larger
/// dictionary is provided, this function will replace the internally kept
/// dictionary and update the index associated with the column.
int ibis::category::setDictionary(const ibis::dictionary &sup) {
    if (dic.size() == 0 || idx == 0)
        prepareMembers();
    if (sup.size() == dic.size()) {
        return (sup.equal_to(dic) ? 0 : -10);
    }
    else if (sup.size() < dic.size()) {
        return -11;
    }

    ibis::array_t<uint32_t> o2n;
    int ierr = sup.morph(dic, o2n);
    if (ierr <= 0) return ierr;

    std::string evt = "category";
    if (ibis::gVerbose > 0) {
        evt += '[';
        evt += (thePart ? thePart->name() : "??");
        evt += '.';
        evt += m_name;
        evt += ']';
    }
    evt += "::setDictionary";
    softWriteLock lock(this, evt.c_str());
    if (! lock.isLocked()) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " failed to obtain a write lock on "
            << m_name;
        return -12;
    }

    ibis::util::timer mytimer(evt.c_str(), 4);
    if (idx != 0 && idxcnt() > 0) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " can not proceed because the existing "
            "index is in use";
        return -13;
    }

    std::string fnm;
    if (0 == dataFileName(fnm)) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " failed to determine the data file name";
        return -14;
    }

    bool neednewindex = true;
    if (idx != 0) {
        ibis::direkte *drk = dynamic_cast<ibis::direkte*>(idx);
        if (drk != 0) {
            ierr = drk->remapKeys(o2n);
            if (ierr >= 0) {
                neednewindex = false;
            }
            else {
                LOGGER(ibis::gVerbose > 3)
                    << "Warning " << evt << " failed to remap keys of the index"
                    ", need to recreate the index";
            }
        }
    }

    dic.copy(sup);
    fnm += ".dic";
    dic.write(fnm.c_str());
    fnm.erase(fnm.size()-3);
    fnm += "int";
    ibis::array_t<uint32_t> ints;
    ints.reserve(thePart->nRows());
    (void) ints.read(fnm.c_str(), 0, (thePart->nRows()<<2));
    if (thePart->nRows() == ints.size()) { // existing int file is good
        for (unsigned j = 0; j < thePart->nRows(); ++ j) {
            ints[j] = o2n[ints[j]];
        }
        ierr = ints.write(fnm.c_str());
        LOGGER(ierr < 0 && ibis::gVerbose >= 0)
            << "Warning -- " << evt << " failed to write integers to " << fnm;
    }
    else { // read the raw strings and generate .int and .idx
        std::string data = thePart->currentDataDir();
        data += FASTBIT_DIRSEP;
        data += m_name; // primary data file name
        int fdata = UnixOpen(data.c_str(), OPEN_READONLY);
        if (fdata >= 0) {
#if defined(_WIN32) && defined(_MSC_VER)
            (void)_setmode(fdata, _O_BINARY);
#endif
            IBIS_BLOCK_GUARD(UnixClose, fdata);
            int ret;
            ibis::fileManager::buffer<char> mybuf;
            char *buf = mybuf.address();
            uint32_t nbuf = mybuf.size();
            ints.clear();
            do {
                array_t<uint32_t> tmp;
                ret = string2int(fdata,
                                 const_cast<ibis::dictionary&>(sup),
                                 nbuf, buf, tmp);
                if (ret > 0) {
                    if (! ints.empty())
                        ints.insert(ints.end(), tmp.begin(), tmp.end());
                    else
                        ints.swap(tmp);
                }
            } while (ret > 0 && ints.size() < thePart->nRows());

            if (ints.size() < thePart->nRows()) {
                ints.insert(ints.end(), thePart->nRows() - ints.size(), 0);
            }
            ierr = ints.write(fnm.c_str());
            LOGGER(ierr < 0 && ibis::gVerbose >= 0)
                << "Warning -- " << evt << " failed to write integers to "
                << fnm;
        }
        else if (neednewindex) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- " << evt << " failed to open data file "
                << data << " to create an index";
            return -15;
        }
    }

    if (neednewindex) {
        if (ints.size() == thePart->nRows()) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << evt << " expects ints.size() to be "
                << thePart->nRows() << ", but it is actually " << ints.size();
            return -16;
        }

        idx = new ibis::direkte(this, 1+sup.size(), ints);
        if (idx == 0) {
            LOGGER(ibis::gVerbose >= 0)
                << "Warning -- " << evt << " failed to generate index from "
                << ints.size() << " integers";
            return -17;
        }

        ierr = idx->write(thePart->currentDataDir());
        LOGGER(ierr < 0 && ibis::gVerbose >= 0)
            << "Warning -- " << evt << " failed to write index to "
            << thePart->currentDataDir();
    }
    if (ierr >= 0)
        ierr = ints.size();
    return ierr;
} // ibis::category::setDictionary

/// Find rows with the exact string as the argument.
long ibis::category::stringSearch(const char* str,
                                  ibis::bitvector& hits) const {
    std::string evt;
    if (ibis::gVerbose > 1) {
        evt = "category[";
        evt += thePart->name();
        evt += '.';
        evt += m_name;
        evt += "]::stringSearch(";
        if (str != 0)
            evt += str;
        else
            evt += "<NULL>";
        evt += ')';
    }
    else {
        evt = "category::stringSearch";
    }
    ibis::util::timer mytimer(evt.c_str(), 4);
    prepareMembers();
    uint32_t ind = dic[str];
    if (ind < dic.size()) { // found it in the dictionary
        indexLock lock(this, evt.c_str());
        if (idx != 0) {
            ibis::qContinuousRange expr(m_name.c_str(),
                                        ibis::qExpr::OP_EQ, ind);
            long ierr = idx->evaluate(expr, hits);
            if (ierr < 0) {
                LOGGER(ibis::gVerbose > 1)
                    << "Warning -- " << evt
                    << " failed because idx->evaluate(" << expr
                    << ") returned " << ierr << ", attempt to work directly"
                    " with raw string values";
                return ibis::text::stringSearch(str, hits);
            }
        }
        else {
            LOGGER(ibis::gVerbose > 2)
                << "Warning -- " << evt << ") failed to reconstruct the "
                "index, try to use the raw string values";
            return ibis::text::stringSearch(str, hits);
        }
    }
    else { // not in the dictionary
        hits.set(0, thePart->nRows());
    }
    LOGGER(ibis::gVerbose > 8)
        << evt << " return hit vector\n" << hits;
    return hits.sloppyCount();
} // ibis::category::stringSearch

/// Estimate an upper bound on the number of apparence of the given string.
long ibis::category::stringSearch(const char* str) const {
    long ret;
    prepareMembers();
    uint32_t ind = dic[str];
    if (ind < dic.size()) { // found it
        indexLock lock(this, "category::stringSearch");
        if (idx != 0) {
            ibis::qContinuousRange expr(m_name.c_str(),
                                        ibis::qExpr::OP_EQ, ind);
            ret = idx->estimate(expr);
        }
        else { // no index, use the number of rows
            ret = thePart->nRows();
        }
    }
    else { // not in the dictionary
        ret = 0;
    }
    return ret;
} // ibis::category::stringSearch

double ibis::category::estimateCost(const ibis::qString& qstr) const {
    double ret;
    prepareMembers();
    const char* str = (stricmp(qstr.leftString(), m_name.c_str()) == 0 ?
                       qstr.rightString() : qstr.leftString());
    uint32_t ind = dic[str];
    if (ind < dic.size()) {
        indexLock lock(this, "category::estimateCost");
        if (idx != 0) {
            ibis::qContinuousRange expr(m_name.c_str(),
                                        ibis::qExpr::OP_EQ, ind);
            ret = idx->estimateCost(expr);
        }
        else { // no index, use the number of rows
            ret = static_cast<double>(thePart->nRows()) * sizeof(uint32_t);
        }
    }
    else {
        ret = 0;
    }
    return ret;
} // ibis::category::estimateCost

double ibis::category::estimateCost(const ibis::qAnyString& qstr) const {
    double ret = 0;
    prepareMembers();
    indexLock lock(this, "category::estimateCost");
    if (idx != 0) {
        const std::vector<std::string>& strs = qstr.valueList();
        std::vector<uint32_t> inds;
        inds.reserve(strs.size());
        for (unsigned j = 0; j < strs.size(); ++ j) {
            uint32_t jnd = dic[strs[j].c_str()];
            if (jnd < dic.size())
                inds.push_back(jnd);
        }
        ibis::qDiscreteRange expr(m_name.c_str(), inds);
        ret = idx->estimateCost(expr);
    }
    else { // no index, use the number of rows
        ret = static_cast<double>(thePart->nRows()) * sizeof(uint32_t);
    }
    return ret;
} // ibis::category::estimateCost

/// Estimate the cost of evaluating a Like expression.
double ibis::category::estimateCost(const ibis::qLike &cmp) const {
    return patternSearch(cmp.pattern());
} // ibis::category::estimateCost

/// Locate the position of the rows with values matching of the string
/// values.
long ibis::category::stringSearch(const std::vector<std::string>& strs,
                                  ibis::bitvector& hits) const {
    if (strs.empty()) {
        hits.set(0, thePart->nRows());
        return 0;
    }

    if (strs.size() == 1) // the list contains only one value
        return stringSearch(strs.back().c_str(), hits);

    prepareMembers();
    // there are more than one value in the list
    std::vector<uint32_t> inds;
    inds.reserve(strs.size());
    for (std::vector<std::string>::const_iterator it = strs.begin();
         it != strs.end(); ++ it) {
        uint32_t ind = dic[(*it).c_str()];
        if (ind < dic.size())
            inds.push_back(ind);
    }

    if (inds.empty()) { // nothing match
        hits.set(0, thePart->nRows());
    }
    else { // found some values in the dictionary
        indexLock lock(this, "category::stringSearch");
        if (idx != 0) {
            ibis::qDiscreteRange expr(m_name.c_str(), inds);
            long ierr = idx->evaluate(expr, hits);
            if (ierr < 0) {
                LOGGER(ibis::gVerbose >= 0)
                    << "Warning -- category["
                    << (thePart != 0 ? thePart->name() : "")
                    << "." << name() << "]::stringSearch on " << strs.size()
                    << " strings failed because idx->evaluate(" << expr
                    << ") failed with error code " << ierr;
                return ierr;
            }
        }
        else { // index must exist! can not proceed
            hits.set(0, thePart->nRows());
            LOGGER(ibis::gVerbose >= 0)
                << "Warning -- category["
                << (thePart != 0 ? thePart->name() : "")
                << "." << name() << "]::stringSearch can not obtain a lock "
                "on the index or there is no index, can not produce any answer";
        }
    }
    return hits.sloppyCount();
} // ibis::category::stringSearch

/// Estimate an upper bound of the number of rows matching any of the given
/// strings.
long ibis::category::stringSearch(const std::vector<std::string>& strs) const {
    long ret = thePart->nRows();
    if (strs.empty()) {
        ret = 0;
    }
    else if (strs.size() == 1) {// the list contains only one value
        ret = stringSearch(strs.back().c_str());
    }
    else {
        // there are more than one value in the list
        prepareMembers();
        std::vector<uint32_t> inds;
        inds.reserve(strs.size());
        for (std::vector<std::string>::const_iterator it = strs.begin();
             it != strs.end(); ++ it) {
            uint32_t ind = dic[(*it).c_str()];
            if (ind < dic.size())
                inds.push_back(ind);
        }

        if (inds.empty()) { // null value
            ibis::bitvector hits;
            getNullMask(hits); // mask = 0 if null
            ret = hits.size() - hits.cnt();
        }
        else { // found some values in the dictionary
            indexLock lock(this, "category::stringSearch");
            if (idx != 0) {
                ibis::qDiscreteRange expr(m_name.c_str(), inds);
                ret = idx->estimate(expr);
            }
            else { // index must exist
                ret = 0;
                LOGGER(ibis::gVerbose >= 0)
                    << "Warning -- category["
                    << (thePart != 0 ? thePart->name() : "")
                    << "." << name() << "]::stringSearch can not obtain a "
                    "lock on the index or there is no index";
            }
        }
    }
    return ret;
} // ibis::category::stringSearch

/// Estimate the number of hits for a string pattern.
long ibis::category::patternSearch(const char *pat) const {
    if (pat == 0 || *pat == 0) return -1;
    prepareMembers();

    if (idx == 0) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- category[" << (thePart != 0 ? thePart->name() : "??")
            << '.' << m_name << "]::patternSearch can not proceed without "
            "an index ";
        return -2;
    }

    const ibis::direkte *rlc = dynamic_cast<const ibis::direkte*>(idx);
    if (rlc == 0) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- category[" << (thePart != 0 ? thePart->name() : "??")
            << '.' << m_name << "]::patternSearch can not proceed without "
            "an index ";
        return -3;
    }

    LOGGER(ibis::gVerbose > 5)
        << "category[" << (thePart != 0 ? thePart->name() : "??") << '.'
        << m_name << "]::patternSearch starting to match pattern " << pat;
    long est = 0;
    ibis::array_t<uint32_t> tmp;
    dic.patternSearch(pat, tmp);
    for (uint32_t j = 0; j < tmp.size(); ++ j) {
        const ibis::bitvector *bv = rlc->getBitvector(tmp[j]);
        if (bv != 0)
            est += bv->cnt();
    }
    return est;
} // ibis::category::patternSearch

/// Find the records with string values that match the given pattern.
long ibis::category::patternSearch(const char *pat,
                                   ibis::bitvector &hits) const {
    hits.clear();
    if (pat == 0 || *pat == 0) return -1;
    if (idx == 0) prepareMembers();

    if (idx == 0) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- category[" << (thePart != 0 ? thePart->name() : "??")
            << '.' << m_name << "]::patternSearch can not proceed without "
            "an index ";
        return -2;
    }

    const ibis::direkte *rlc = dynamic_cast<const ibis::direkte*>(idx);
    if (rlc == 0) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- category[" << (thePart != 0 ? thePart->name() : "??")
            << '.' << m_name << "]::patternSearch can not proceed without "
            "the latex version of index";
        return -3;
    }

    LOGGER(ibis::gVerbose > 5)
        << "category[" << (thePart != 0 ? thePart->name() : "??") << '.'
        << m_name << "]::patternSearch starting to match pattern " << pat;

    ibis::array_t<uint32_t> tmp;
    dic.patternSearch(pat, tmp);
    if (tmp.empty()) {
        hits.set(0, thePart->nRows());
        return 0;
    }
    else {
        rlc->sumBins(tmp, hits);
        return hits.sloppyCount();
    }
} // ibis::category::patternSearch

/// Return the string at the <code>i</code>th row.  If the .int file is
/// present, it will be used, otherwise this function uses the raw data
/// file.
int ibis::category::getString(uint32_t i, std::string &str) const {
    str.clear();
    if (i == 0) return 0; // nothing else to do

    if (i >= dic.size())
        prepareMembers();

    int ierr;
    std::string fnm;
    if (dataFileName(fnm) != 0) { // try .int file
        fnm += ".int";
        ibis::array_t<uint32_t> ints;
        ierr = ibis::fileManager::instance().getFile(fnm.c_str(), ints);
        if (ierr >= 0 && ints.size() == thePart->nRows()) {
            if (i < ints.size()) {
                str = dic[ints[i]];
            }
            return 0;
        }
    }
    return ibis::text::readString(i, str);
} // ibis::category::getString

/// This function makes sure the index is ready.  It can also be called to
/// initialize all the internal data members because the lazy
/// initialization in the constructor of this class.
void ibis::category::loadIndex(const char*, int) const throw () {
    prepareMembers();
} // ibis::category::loadIndex

// read the string values (terminated with NULL) from the directory "dt" and
// extend the set of bitvectors representing the strings
long ibis::category::append(const char* dt, const char* df,
                            const uint32_t nold, const uint32_t nnew,
                            uint32_t nbuf, char* buf) {
    long ret = 0; // the return value
    long ierr = 0;
    uint32_t cnt = 0;
    if (nnew == 0)
        return ret;
    if (dt == 0 || df == 0)
        return ret;
    if (*dt == 0 || *df == 0)
        return ret;
    if (std::strcmp(dt, df) == 0)
        return ret;
    std::string evt = "category";
    if (ibis::gVerbose > 1) {
        evt += '[';
        evt += (thePart ? thePart->name() : "?");
        evt += '.';
        evt += m_name;
        evt += ']';
    }
    evt += "::append";
    if (ibis::gVerbose > 2) {
        evt += '(';
        evt += dt;
        evt += ", ";
        evt += df;
        evt += ')';
    }

    prepareMembers();
    // STEP 1: convert the strings to ibis::direkte
    std::string dest = dt;
    std::string src = df;
    src += FASTBIT_DIRSEP;
    src += name();
    src += ".idx";
    dest += FASTBIT_DIRSEP;
    dest += name();
    //dest += ".idx";
    ibis::direkte *binp = 0;
    ibis::fileManager::storage *st = 0;
    ierr = ibis::fileManager::instance().getFile(src.c_str(), &st);
    readDictionary(df); // read the dictionary in df
    src.erase(src.size()-4); // remove .idx extension
    //dest.erase(dest.size()-4); // remove .idx
    if (ierr == 0 && st != 0 && st->size() > 0) {
        // read the previously built index
        binp = new ibis::direkte(this, st);
        cnt = nnew;

        // copy the raw bytes to dt
        int fptr = UnixOpen(src.c_str(), OPEN_READONLY);
        if (fptr >= 0) {
            IBIS_BLOCK_GUARD(UnixClose, fptr);
#if defined(_WIN32) && defined(_MSC_VER)
            (void)_setmode(fptr, _O_BINARY);
#endif
            int fdest = UnixOpen(dest.c_str(), OPEN_APPENDONLY, OPEN_FILEMODE);
            if (fdest >= 0) { // copy raw bytes without any sanity check
                IBIS_BLOCK_GUARD(UnixClose, fdest);
#if defined(_WIN32) && defined(_MSC_VER)
                (void)_setmode(fdest, _O_BINARY);
#endif
                while ((ierr = UnixRead(fptr, buf, nbuf))) {
                    ret = UnixWrite(fdest, buf, ierr);
                    LOGGER(ret != ierr && ibis::gVerbose > 2)
                        << "Warning -- " << evt << " expected to write "
                        << ierr << " byte " << (ierr>1?"s":"")
                        << " to \"" << dest << "\" by only wrote "
                        << ret;
                }
#if defined(FASTBIT_SYNC_WRITE)
#if _POSIX_FSYNC+0 > 0
                    (void) UnixFlush(fdest); // write to disk
#elif defined(_WIN32) && defined(_MSC_VER)
                    (void) _commit(fdest);
#endif
#endif
            }
            else {
                LOGGER(ibis::gVerbose > 0)
                    << "Warning " << evt << " failed to open \"" << dest
                    << "\"";
            }
        }
        else {
            LOGGER(ibis::gVerbose > 5)
                << "Warning -- " << evt << " failed to open file \"" << src
                << "\" for reading ... "
                << (errno ? strerror(errno) : "no free stdio stream")
                << ", assume the attribute to have only one value";
        }
    }
    else {
        // first time accessing these strings, need to parse them
        int fptr = UnixOpen(src.c_str(), OPEN_READONLY);
        if (fptr >= 0) {
            IBIS_BLOCK_GUARD(UnixClose, fptr);
#if defined(_WIN32) && defined(_MSC_VER)
            (void)_setmode(fptr, _O_BINARY);
#endif
            ret = 0;
            array_t<uint32_t> ints;
            do { // loop through the content of the file
                array_t<uint32_t> tmp;
                ret = string2int(fptr, dic, nbuf, buf, tmp);
                if (ret < 0) {
                    LOGGER(ibis::gVerbose >= 0)
                        << "Warning -- category["
                        << (thePart != 0 ? thePart->name() : "")
                        << "." << name() << "]::append string2int returned "
                        << ret << " after processed " << cnt
                        << " strings from \"" << src << "\"";
                    return ret;
                }
                if (ret > 0) {
                    if (! ints.empty()) {
                        ints.insert(ints.end(), tmp.begin(), tmp.end());
                    }
                    else {
                        ints.swap(tmp);
                    }
                }
            } while (ret > 0);
            if (ints.size() > nnew) {
                // step 1: look through the values to find how many nil
                // strings
                cnt = 0;
                const unsigned long nints = ints.size();
                for (unsigned i = 0; i < nints; ++ i)
                    cnt += (ints[i] == 0);
                if (ints.size() == cnt + nnew) {
                    LOGGER(ibis::gVerbose > 1)
                        << "Warning -- " << evt << " found " << nints
                        << " element(s), but expected only " << ret
                        << ", extra ones are likely nill strings, "
                        "removing nill strings";
                    cnt = 0;
                    for (unsigned i = 0; i < nints; ++ i) {
                        if (ints[i] != 0) {
                            ints[cnt] = ints[i];
                            ++ cnt;
                        }
                    }
                }
                else {
                    LOGGER(ibis::gVerbose > 1)
                        << "Warning -- " << evt << " found "<< nints
                        << " element(s), but expected only " << ret
                        << ", truncate the extra elements";
                }
                ints.resize(nnew);
            }
            else if (ints.size() < nnew) {
                LOGGER(ibis::gVerbose > 1)
                    << "Warning -- " << evt << "found " << ints.size()
                    << " element(s), but expecting only " << ret
                    << ", adding nill strings to make up the difference";

                ints.insert(ints.end(), nnew-ints.size(), (uint32_t)0);
            }
            cnt = ints.size();

            if (binp != 0) {
                ierr = binp->append(ints);
            }
            else {
                binp = new ibis::direkte(this, 1+dic.size(), ints);
                ierr = ints.size() * (binp != 0);
            }
            LOGGER(static_cast<uint32_t>(ierr) != ints.size() &&
                   ibis::gVerbose >= 0)
                << "Warning -- category["
                << (thePart != 0 ? thePart->name() : "")
                << "." << name() << "]::append string2int processed "
                << ints.size() << " strings from \"" << src
                << "\" but was only able append " << ierr << " to the index";

            int fdest = UnixOpen(dest.c_str(), OPEN_APPENDONLY, OPEN_FILEMODE);
            if (fdest >= 0) { // copy raw bytes without any sanity check
                IBIS_BLOCK_GUARD(UnixClose, fdest);
#if defined(_WIN32) && defined(_MSC_VER)
                (void)_setmode(fdest, _O_BINARY);
#endif
                ierr = UnixSeek(fptr, 0, SEEK_SET);
                if (ierr < 0) return -2;
                while ((ierr = UnixRead(fptr, buf, nbuf)) > 0) {
                    ret = UnixWrite(fdest, buf, ierr);
                    LOGGER(ret != ierr && ibis::gVerbose > 2)
                        << "Warning -- " << evt << " expected to write "
                        << ierr << " bytes to \"" << dest
                        << "\" by only wrote " << ret;
                }
#if defined(FASTBIT_SYNC_WRITE)
#if  _POSIX_FSYNC+0 > 0
                (void) UnixFlush(fdest); // write to disk
#elif defined(_WIN32) && defined(_MSC_VER)
                (void) _commit(fdest);
#endif
#endif
            }
            else {
                LOGGER(ibis::gVerbose > 0)
                    << "Warning -- " << evt << " failed to open \"" << dest
                    << "\"";
            }
            if (ierr < 0) return -3;
        }
        else {
            LOGGER(ibis::gVerbose > 5)
                << "Warning -- " << evt << " failed to open file \"" << src
                << "\" for reading ... "
                << (errno ? strerror(errno) : "no free stdio stream")
                << ", assume the attribute to have only one value";

            binp = new ibis::direkte(this, 1, nnew);
            cnt = nnew;
        }
        if (binp != 0)
            binp->write(df); // record the bitmap
        src += ".dic";
        dic.write(src.c_str()); // write the dictionary to source directory
        src.erase(src.size()-4);
    }

    // write dictionary to the destination directory
    lower = 1;
    upper = dic.size();
    dest += ".dic";
    dic.write(dest.c_str());
    LOGGER(ibis::gVerbose > 4)
        << evt << "appended " << cnt << " row" << (cnt>1?"s":"")
        << ", new dictionary size is " << dic.size();

    ////////////////////////////////////////
    // STEP 2: extend the null mask
    src += ".msk";
    ibis::bitvector mapp(src.c_str());
    if (mapp.size() != nnew)
        mapp.adjustSize(cnt, nnew);
    LOGGER(ibis::gVerbose > 7)
        << evt << "-- mask file \"" << src << "\" contains " << mapp.cnt()
        << " set bits out of " << mapp.size() << " total bits";

    dest.erase(dest.size()-3);
    dest += "msk";
    ibis::bitvector mtot(dest.c_str());
    if (mtot.size() == 0)
        mtot.set(1, nold);
    else if (mtot.size() != nold)
        mtot.adjustSize(0, nold);
    LOGGER(ibis::gVerbose > 7)
        << evt << " -- mask file \"" << dest << "\" contains "
        << mtot.cnt() << " set bits out of " << mtot.size() << " total bits";

    mtot += mapp; // append the new ones to the end of the old ones
    if (mtot.size() != nold+nnew) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- category[" << (thePart != 0 ? thePart->name() : "")
            << "." << name() << "]::append expects the combined mask to have "
            << nold+nnew << " bits, but it has " << mtot.size();
        mtot.adjustSize(nold+nnew, nold+nnew);
    }
    if (mtot.cnt() != mtot.size()) {
        mtot.write(dest.c_str());
        LOGGER(ibis::gVerbose > 6)
            << evt << " -- mask file \"" << dest << "\" indicates "
            << mtot.cnt() << " valid records out of " << mtot.size();
#if DEBUG+0 > 0 || _DEBUG+0 > 0
        LOGGER(ibis::gVerbose > 6) << mtot;
#endif
    }
    else {
        remove(dest.c_str()); // no need to have the file
        ibis::fileManager::instance().flushFile(dest.c_str());
        LOGGER(ibis::gVerbose > 6)
            << evt << " -- mask file \"" << dest << "\" removed, all "
            << mtot.size() << " records are valid";
    }

    ////////////////////////////////////////
    // extend the index
    try { // attempt to load the index from directory dt
        if (binp) {
            ibis::direkte ind(this, dt);

            if (ind.getNRows() == nold && nold > 0) { // append the index
                ierr = ind.append(*binp);
                if (ierr == 0) {
                    ind.write(dt);
                    LOGGER(ibis::gVerbose > 6)
                        << evt << " successfully extended the index in " << dt;
                    if (ibis::gVerbose > 8) {
                        ibis::util::logger lg;
                        ind.print(lg());
                    }
                }
                else {
                    LOGGER(ibis::gVerbose > 0)
                        << "Warning -- category["
                        << (thePart != 0 ? thePart->name() : "") << "."
                        << name()
                        << "]::append failed to extend the index, ierr = "
                        << ierr;
                    if (ind.getNRows() > 0)
                        purgeIndexFile(dt);
                    (void) fillIndex(dt);
                    if (idx != 0)
                        (void) idx->write(dt);
                }
            }
            else if (nold == 0) { // only need to copy the pointer
                binp->write(dt);
                ierr = 0;
            }
            else {
                LOGGER(ibis::gVerbose > 2)
                    << "Warning -- " << evt << "encountered an unexpected "
                    "index for existing values in " << dt << " (nold="
                    << nold << ", ind.nrows=" << ind.getNRows() << ")";
                if (ind.getNRows() > 0)
                    purgeIndexFile(dt);
                (void) fillIndex(dt);
                if (idx != 0)
                    (void) idx->write(dt);
            }
            delete binp;
        }
        else {
            LOGGER(ibis::gVerbose > 2)
                << "Warning -- " << evt << " failed to generate the index for "
                "data in " << df << ", start scanning all records in " << dt;
            (void) fillIndex(dt);
            if (idx != 0)
                (void) idx->write(dt);
        }
    }
    catch (...) {
        LOGGER(ibis::gVerbose > 2)
            << "Warning -- " << evt << " absorbed an exception while extending "
            "the index, start scanning all records in " << dt;
        (void) fillIndex(dt);
        if (idx != 0)
            (void) idx->write(dt);
    }
    ret = cnt;

    return ret;
} // ibis::category::append

/// Write the current content to the metadata file for the data partition.
void ibis::category::write(FILE* file) const {
    std::string evt = "category";
    evt += '[';
    if (ibis::gVerbose > 0 && thePart != 0) {
        evt += thePart->name();
        evt += '.';
    }
    evt += m_name;
    evt += "]::write";

    off_t ierr = 0;
    fputs("\nBegin Column\n", file);
    fprintf(file, "name = \"%s\"\n", (const char*)m_name.c_str());
    if ((m_desc.empty() || m_desc == m_name) && dic.size() > 0) {
        fprintf(file, "description = %s ", m_name.c_str());
        unsigned lim = (dic.size()+1);
        unsigned nchar = 0;
        unsigned i = 1;
        fprintf(file, "= ");
        if (lim > 10)
            lim = 10;
        for (i = 1; i < lim && nchar < 100; ++i) {
            ierr = fprintf(file, "%s, ", dic[i]);
            if (ierr <= 0) {
                LOGGER(ibis::gVerbose >= 0)
                    << "Warning -- " << evt
                    << " failed to write a description from dictionary";
                break;
            }
            nchar += ierr;
        }
        if (i < dic.size()) {
            fprintf(file, "...");
            if (nchar+std::strlen(dic[dic.size()-1]) < 200) {
                fprintf(file, ", %s", dic[dic.size()-1]);
            }
        }
        fprintf(file, "\n");
    }
    else if (! m_desc.empty()) {
        if (m_desc.size() > MAX_LINE-60)
            const_cast<std::string&>(m_desc).erase(MAX_LINE-60);
        fprintf(file, "description =\"%s\"\n", m_desc.c_str());
    }
    fprintf(file, "data_type = \"%s\"\n", TYPESTRING[m_type]);
    fprintf(file, "minimum = 1\nmaximum = %lu\n",
            static_cast<long unsigned>(dic.size()));
    if (! m_bins.empty())
        fprintf(file, "index=%s\n", m_bins.c_str());
    fputs("End Column\n", file);
} // ibis::category::write

/// Print header info.
void ibis::category::print(std::ostream& out) const {
    out << m_name << ": " << m_desc << " (KEY) [";
    if (dic.size() > 20) {
        for (int i = 0; i < 9; ++ i)
            out << dic[i] << ", ";
        out << "...(" << dic.size()-10 << " skipped), " << dic[dic.size()-1];
    }
    else if (dic.size() > 1) {
        out << dic[0U];
        for (unsigned int i = 1; i < dic.size(); ++ i)
            out << ", " << dic[i];
    }
    out << "]";
} // ibis::category::print

/// Return the number of key values.
uint32_t ibis::category::getNumKeys() const {
    if (dic.size() == 0)
        prepareMembers();
    return dic.size();
}

/// Return the ith value in the dictionary.
const char* ibis::category::getKey(uint32_t i) const {
    if (i == 0)
        return 0;
    if (dic.size() == 0)
        prepareMembers();
    return dic[i];
}

/// Is the given string one of the keys in the dictionary?  Return a
/// null pointer if not.
const char* ibis::category::isKey(const char* str) const {
    if (dic.size() == 0)
        prepareMembers();
    return dic.find(str);
}

////////////////////////////////////////////////////////////////////////
// functions for ibis::text
ibis::text::text(const part* tbl, FILE* file) : ibis::column(tbl, file) {
#ifdef FASTBIT_EAGER_INIT
    if (thePart != 0)
        startPositions(thePart->currentDataDir(), 0, 0);
#endif
}

/// Construct a text object for a data partition with the given name.
ibis::text::text(const part* tbl, const char* name, ibis::TYPE_T t)
    : ibis::column(tbl, t, name) {
#ifdef FASTBIT_EAGER_INIT
    if (thePart != 0 && thePart->currentDataDir() != 0)
        startPositions(thePart->currentDataDir(), 0, 0);
#endif
}

/// Copy constructor.  Copy from a column of the type TEXT.
ibis::text::text(const ibis::column& col) : ibis::column(col) {
    if (m_type != ibis::TEXT && m_type != ibis::CATEGORY) {
        throw ibis::bad_alloc("Must be either TEXT or CATEGORY -- text::ctor"
                              IBIS_FILE_LINE);
    }
#ifdef FASTBIT_EAGER_INIT
    if (thePart != 0 && thePart->urrentDataDir() != 0)
        startPositions(thePart->currentDataDir(), 0, 0);
#endif
} // copy constructor

/// Locate the starting position of each string.
/// Using the data file located in the named directory @c dir.  If @c dir
/// is a nil pointer, the directory defaults to the current working
/// directory of the data partition.
///
/// It writes the starting positions as int64_t integers to a file with .sp
/// as extension.
///
/// Argument @c buf (with @c nbuf bytes) is used as temporary work space.
/// If @c nbuf = 0, this function allocates its own working space.
void ibis::text::startPositions(const char *dir, char *buf,
                                uint32_t nbuf) const {
    if (thePart == 0) return;
    if (dir == 0) // default to the current data directory
        dir = thePart->currentDataDir();
    if (dir == 0 || *dir == 0) return;

    int64_t pos = 0;
    uint32_t nold=0;
    std::string evt = "text[";
    evt += thePart->name();
    evt += '.';
    evt += m_name;
    evt += "]::startPositions";
    std::string dfile = dir;
    dfile += FASTBIT_DIRSEP;
    dfile += m_name;
    std::string spfile = dfile;
    spfile += ".sp";
    mutexLock lock(this, "text::startPositions");
    FILE *fdata = fopen(dfile.c_str(), "r+b"); // mostly for reading
    FILE *fsp = fopen(spfile.c_str(), "r+b"); // mostly for writing
    if (fsp == 0) // probably because the file does not exist, try again
        fsp = fopen(spfile.c_str(), "wb");
    if (fsp == 0) { // again failed to open .sp file
        if (fdata == 0) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << evt << " failed to open file "
                << dfile;
        }
        else {
            fclose(fdata);
        }
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " failed to open file "
            << spfile;
        return;
    }
    const bool isActiveData =
        (thePart->getStateNoLocking() == ibis::part::STABLE_STATE &&
         (dir == thePart->currentDataDir() ||
          std::strcmp(dir, thePart->currentDataDir()) == 0));
    if (fdata == 0) { // failed to open data file, assume it is empty
#ifdef FASTBIT_WEIRED_SETUP
        // comment out to avoid problem when user removes the data file
        if ((isActiveData || thePart->currentDataDir()== 0)
            && thePart->nRows() > 0) {
            rewind(fsp);
            nold = thePart->nRows();
            for (unsigned j = 0; j <= nold; ++ j)
                (void) fwrite(&pos, sizeof(int64_t), 1, fsp);
        }
#endif
        fclose(fsp);
        return;
    }

    long ierr = fseek(fdata, 0, SEEK_END);
    int64_t dfbytes = ftell(fdata);
    ierr = fseek(fsp, 0, SEEK_END);
    ierr = ftell(fsp);
    if (isActiveData && ierr > (long)(8 * thePart->nRows())) {
        fclose(fdata);
        fclose(fsp);
        return;
    }

    ibis::util::timer mytimer(evt.c_str(), 3);
    ibis::fileManager::buffer<char> mybuf(nbuf != 0);
    if (nbuf == 0) {
        nbuf = mybuf.size();
        buf = mybuf.address();
    }

    if (ierr > (long)sizeof(uint64_t)) // .sp contains at least two integers
        ierr = fseek(fsp, -static_cast<long>(sizeof(int64_t)), SEEK_END);
    else
        ierr = -1;
    if (ierr == 0) { // try to read the last word in .sp file
        if (fread(&pos, sizeof(int64_t), 1, fsp) != 1) {
            LOGGER(ibis::gVerbose >= 0)
                << "Warning -- " << evt << " failed to read the last "
                "integer in file \"" << spfile << "\"";
            fclose(fsp);
            fclose(fdata);
            return;
        }
        if (pos >= 0 && pos <= dfbytes) {// within the valid range
            nold = ftell(fsp) / sizeof(int64_t) - 1;
            if (nold > thePart->nRows()) { // start from beginning
                pos = 0;
                nold = 0;
                fclose(fsp);
                /*remove(spfile.c_str());*/
                fsp = fopen(spfile.c_str(), "wb");
            }
        }
        else { // start from scratch
            pos = 0;
        }
    }

    if (nold > 0) { // ready to overwrite the last integer
        ierr = fseek(fsp, nold*sizeof(int64_t), SEEK_SET);
    }
    else {
        rewind(fsp);
        pos = 0;
    }
    if (dfbytes <= 0) { // empty data file
        if (isActiveData) {
            for (unsigned j = nold; j <= thePart->nRows(); ++ j)
                (void) fwrite(&pos, sizeof(int64_t), 1, fsp);
        }
        fclose(fsp);
        fclose(fdata);
        return;
    }

    ibis::fileManager::buffer<int64_t> sps;
    int64_t last = pos;
    int64_t offset = 0;
    uint32_t nnew = 0;
    ierr = fflush(fsp); // get ready for writing
    ierr = fseek(fdata, pos, SEEK_SET);
    if (sps.size() <= 1) { // write one sp value at a time
        while (0 < (ierr = fread(buf+offset, 1, nbuf-offset, fdata))) {
            const char* const end = buf + offset + ierr;
            for (const char *s = buf+offset; s < end; ++ s, ++ pos) {
                if (*s == 0) { // find a terminator
                    if (1 > fwrite(&last, sizeof(last), 1, fsp)) {
                        LOGGER(ibis::gVerbose >= 0)
                            << "Warning -- "
                            << evt << " failed to write integer value "
                            << last << " to file \"" << spfile << "\"";
                    }
                    last = pos+1;
                    ++ nnew;
                    LOGGER(ibis::gVerbose > 4 && nnew % 1000000 == 0)
                        << evt << " -- processed "
                        << nnew << " strings from " << dfile;
                }
            }
            offset = pos - last;
            if (static_cast<uint64_t>(offset) < nbuf) {
                // copy the string without a terminator
                const int tmp = ierr - offset;
                for (int i = 0; i < offset; ++ i)
                    buf[i] = buf[i+tmp];
            }
            else {
                offset = 0;
            }
        }
    }
    else { // temporarily store sp values in sps
        const uint32_t nsps = sps.size();
        uint32_t jsps = 0;
        while (0 < (ierr = fread(buf+offset, 1, nbuf-offset, fdata))) {
            const char* const end = buf + offset + ierr;
            for (const char *s = buf+offset; s < end; ++ s, ++ pos) {
                if (*s == 0) { // find a terminator
                    sps[jsps] = last;
                    ++ jsps;
                    if (jsps >= nsps) {
                        if (jsps >
                            fwrite(sps.address(), sizeof(last), jsps, fsp)) {
                            LOGGER(ibis::gVerbose >= 0)
                                << "Warning -- "
                                << evt << " failed to write " << jsps
                                << " integers to file \"" << spfile << "\"";
                        }
                        jsps = 0;
                    }
                    last = pos+1;
                    ++ nnew;
                    LOGGER(ibis::gVerbose > 4 && nnew % 1000000 == 0)
                        << evt << " -- processed "
                        << nnew << " strings from " << dfile;
                }
            }
            offset = pos - last;
            if (static_cast<uint64_t>(offset) < nbuf) {
                // copy the string without a terminator
                const int tmp = ierr - offset;
                for (int i = 0; i < offset; ++ i)
                    buf[i] = buf[i+tmp];
            }
            else {
                offset = 0;
            }
        }
        if (jsps > 0) {
            if (jsps >
                fwrite(sps.address(), sizeof(last), jsps, fsp)) {
                LOGGER(ibis::gVerbose >= 0)
                    << "Warning -- " << evt << " failed to write " << jsps
                    << " integers to file \"" << spfile << "\"";
            }
        }
    }

    if (nold + nnew < thePart->nRows() && thePart->currentDataDir() != 0 &&
        std::strcmp(dir, thePart->currentDataDir()) == 0) {
        // make up missing values with a null string
        char zero = 0;
        // commit all read operations in preparation for write
        pos = ftell(fdata);
        ierr = fflush(fdata);
        ierr = fwrite(&zero, 1, 1, fdata);
        int64_t *tmp = (int64_t*) buf;
        uint32_t ntmp = nbuf / sizeof(int64_t);
        for (uint32_t i = 0; i < ntmp; ++ i)
            tmp[i] = pos;
        const long missed = thePart->nRows() - nold - nnew + pos;
        for (long i = 0; i < missed; i += ntmp) {
            ierr = fwrite(tmp, sizeof(int64_t),
                          (i+(long)ntmp<=missed?(long)ntmp:missed-i), fsp);
        }
    }
    if (nnew > 0) {
        pos = ftell(fdata);// current size of the data file
        (void) fwrite(&pos, sizeof(pos), 1, fsp);
    }
    (void) fclose(fdata);
    (void) fclose(fsp);

    LOGGER(ibis::gVerbose > 3)
        << evt << " located the starting positions of " << nnew
        << " new string" << (nnew > 1 ? "s" : "") << ", file " << spfile
        << " now has " << (nnew+nold+1) << " 64-bit integers (total "
        << sizeof(int64_t)*(nnew+nold+1) << " bytes)";

    if (isActiveData && nold + nnew > thePart->nRows()) {
        // too many strings in the base data file, truncate the file
        fsp = fopen(spfile.c_str(), "rb");
        ierr = fseek(fsp, thePart->nRows()*sizeof(int64_t), SEEK_SET);
        ierr = fread(&pos, sizeof(int64_t), 1, fsp);
        ierr = fclose(fsp);
        ierr = truncate(spfile.c_str(), (1+thePart->nRows())*sizeof(int64_t));
        ierr = truncate(dfile.c_str(), pos);
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- " << evt << " truncated files " << dfile << " and "
            << spfile << " to contain only " << thePart->nRows() << " record"
            << (thePart->nRows() > 1 ? "s" : "");
    }

    ibis::bitvector msk;
    if (isActiveData)
        msk.copy(mask_);
    msk.adjustSize(nold+nnew, thePart->nRows());
    if (msk.cnt() < msk.size()) {
        spfile.erase(spfile.size()-2);
        spfile += "msk";
        (void) msk.write(spfile.c_str());
    }
    if (isActiveData) {
        // we have discovered something unexpected, need to modify the mask
        const_cast<ibis::bitvector&>(mask_).swap(msk);
    }
} // ibis::text::startPositions

void ibis::text::loadIndex(const char* iopt, int ropt) const throw () {
    if (thePart != 0 && thePart->currentDataDir() != 0) {
        startPositions(thePart->currentDataDir(), 0, 0);
        ibis::column::loadIndex(iopt, ropt);
    }
} // ibis::text::loadIndex

/// Append the data file stored in directory @c df to the corresponding
/// data file in directory @c dt.  Use the buffer @c buf to copy data in
/// large chuncks.
///@note  No error checking is performed.
///@note  Does not check for missing entries.  May cause records to be
/// misaligned.
long ibis::text::append(const char* dt, const char* df,
                        const uint32_t nold, const uint32_t nnew,
                        uint32_t nbuf, char* buf) {
    long ret = 0; // the return value
    long ierr = 0;

    if (nnew == 0)
        return ret;
    if (dt == 0 || df == 0)
        return ret;
    if (*dt == 0 || *df == 0)
        return ret;
    if (std::strcmp(dt, df) == 0)
        return ret;

    // step 1: make sure the starting positions are updated
    if (nold > 0)
        startPositions(dt, buf, nbuf);

    // step 2: append the content of file in df to that in dt.
    std::string evt = "text[";
    evt += fullname();
    evt += "]::append";
    std::string dest = dt;
    std::string src = df;
    src += FASTBIT_DIRSEP;
    src += name();
    dest += FASTBIT_DIRSEP;
    dest += name();

    int fsrc = UnixOpen(src.c_str(), OPEN_READONLY);
    if (fsrc < 0) {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- " << evt << " failed to open file \"" << src
            <<"\" for reading";
        return -1;
    }
#if defined(_WIN32) && defined(_MSC_VER)
    (void)_setmode(fsrc, _O_BINARY);
#endif
    int fdest = UnixOpen(dest.c_str(), OPEN_APPENDONLY, OPEN_FILEMODE);
    if (fdest < 0) {
        UnixClose(fsrc);
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- " << evt << " failed to open file \"" << dest
            <<"\" for appending";
        return -2;
    }
#if defined(_WIN32) && defined(_MSC_VER)
    (void)_setmode(fdest, _O_BINARY);
#endif

    while (0 < (ierr = UnixRead(fsrc, buf, nbuf))) {
        ret = UnixWrite(fdest, buf, ierr);
        if (ret < ierr) {
            LOGGER(ibis::gVerbose >= 0)
                << "Warning -- " << evt << " failed to write " << ierr
                << " bytes to file \"" << dest << "\", only wrote " << ret;
            ret = -3;
            break;
        }
    }
#if defined(FASTBIT_SYNC_WRITE)
#if  _POSIX_FSYNC+0 > 0
    (void) UnixFlush(fdest); // write to disk
#elif defined(_WIN32) && defined(_MSC_VER)
    (void) _commit(fdest);
#endif
#endif
    UnixClose(fdest);
    UnixClose(fsrc);
    if (ret < 0) return ret;
    if (! (lower < upper)) {
        lower = 0;
        upper = nnew + nold - 1;
    }
    else if (upper < nnew+nold-1) {
        upper = nnew + nold-1;
    }

    // step 3: update the starting positions after copying the values
    startPositions(dt, buf, nbuf);
    ret = nnew;

    // step 4: deals with the masks
    std::string filename;
    filename = fsrc;
    filename += ".msk";
    ibis::bitvector mapp;
    try {mapp.read(filename.c_str());} catch (...) {/* ok to continue */}
    mapp.adjustSize(nnew, nnew);
    LOGGER(ibis::gVerbose > 7)
        << evt << " mask file \"" << filename << "\" contains "
        << mapp.cnt() << " set bits out of " << mapp.size()
        << " total bits";

    filename = dest;
    filename += ".msk";
    ibis::bitvector mtot;
    try {mtot.read(filename.c_str());} catch (...) {/* ok to continue */}
    mtot.adjustSize(nold, nold);
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
        mask_.swap(mtot);
    }
    return ret;
} // ibis::text::append

long ibis::text::stringSearch(const char*) const {
    return (thePart ? thePart->nRows() : INT_MAX);
} // ibis::text::stringSearch

long ibis::text::stringSearch(const std::vector<std::string>&) const {
    return (thePart ? thePart->nRows() : INT_MAX);
} // ibis::text::stringSearch

/// Given a string literal, return a bitvector that marks the strings that
/// matche it.  This is a relatively slow process since this function
/// actually reads the string values from disk.
long ibis::text::stringSearch(const char* str, ibis::bitvector& hits) const {
    hits.clear(); // clear the existing content of hits
    if (thePart == 0) return -1L;

    std::string evt = "text[";
    if (thePart != 0 && thePart->name() != 0) {
        evt += thePart->name();
        evt += '.';
    }
    evt += m_name;
    evt += "]::stringSearch";
    ibis::util::timer mytimer(evt.c_str(), 4);
    std::string data = thePart->currentDataDir();
    data += FASTBIT_DIRSEP;
    data += m_name;
    FILE *fdata = fopen(data.c_str(), "rb");
    if (fdata == 0) {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- " << evt << " can not open data file \"" << data
            << "\" for reading";
        return -2L;
    }

#if defined(DEBUG) || defined(_DEBUG) // DEBUG+0 > 0 || _DEBUG+0 > 0
    ibis::fileManager::buffer<char> mybuf(5000);
#else
    ibis::fileManager::buffer<char> mybuf;
#endif
    char *buf = mybuf.address();
    uint32_t nbuf = mybuf.size();
    if (buf == 0 || nbuf == 0) return -3L;

    std::string sp = data;
    sp += ".sp";
    FILE *fsp = fopen(sp.c_str(), "rb");
    if (fsp == 0) { // try again
        startPositions(thePart->currentDataDir(), buf, nbuf);
        fsp = fopen(sp.c_str(), "rb");
        if (fsp == 0) { // really won't work out
            LOGGER(ibis::gVerbose >= 0)
                << "Warning -- " << evt << " can not create or open file \""
                << sp << "\"";
            fclose(fdata);
            return -4L;
        }
    }

#if defined(DEBUG) || defined(_DEBUG) // DEBUG+0 > 0 || _DEBUG+0 > 0
    ibis::fileManager::buffer<int64_t> spbuf(1000);
#else
    ibis::fileManager::buffer<int64_t> spbuf;
#endif
    uint32_t irow = 0; // row index
    long jbuf = 0; // number of bytes in buffer
    int64_t begin = 0; // beginning position (file offset) of the bytes in buf
    int64_t next = 0;
    int64_t curr, ierr;
    if (1 > fread(&curr, sizeof(curr), 1, fsp)) {
        // odd to be sure, but try again anyway
        fclose(fsp);
        startPositions(thePart->currentDataDir(), buf, nbuf);
        fsp = fopen(sp.c_str(), "rb");
        if (fsp == 0) { // really won't work out
            LOGGER(ibis::gVerbose >= 0)
                << "Warning -- " << evt <<  " can not open or read file \""
                << sp << "\"";
            fclose(fdata);
            return -5L;
        }
    }
    if (spbuf.size() > 1 && (str == 0 || *str == 0)) {
        // match empty strings, with a buffer for starting positions
        uint32_t jsp, nsp;
        ierr = fread(spbuf.address(), sizeof(int64_t), spbuf.size(), fsp);
        if (ierr <= 0) {
            LOGGER(ibis::gVerbose >= 0)
                << "Warning -- " << evt << " failed to read file " << sp;
            fclose(fsp);
            fclose(fdata);
            return -6L;
        }
        next = spbuf[0];
        nsp = ierr;
        jsp = 1;
        while ((jbuf = fread(buf, 1, nbuf, fdata)) > 0) {
            bool moresp = true;
            if (next > begin+jbuf) {
                LOGGER(ibis::gVerbose >= 0)
                    << "Warning -- " << evt
                    << " expects string # " << irow << " in file \""
                    << data << "\" to be " << (next-begin) << "-byte long, but "
                    << (jbuf<(long)nbuf ? "can only read " :
                        "the internal buffer is only ")
                    << jbuf << ", skipping " << jbuf
                    << (jbuf > 1 ? " bytes" : " byte");
                curr += jbuf;
            }
            while (begin + jbuf >= next) {
                if (buf[curr-begin] == 0)
                    hits.setBit(irow, 1);
                ++ irow;
                curr = next;
                LOGGER(ibis::gVerbose > 2 && irow % 1000000 == 0)
                    << evt << " processed " << irow
                    << " strings from file " << data;

                if (moresp) {
                    if (jsp >= nsp) {
                        ierr = fread(spbuf.address(), sizeof(int64_t),
                                     spbuf.size(), fsp);
                        if (ierr <= 0) {
                            LOGGER(ierr < 0 && ibis::gVerbose >= 0)
                                << "Warning -- " << evt << " failed to read "
                                << sp;
                            moresp = false;
                            nsp = 0;
                            break;
                        }
                        else {
                            nsp = ierr;
                        }
                        jsp = 0;
                    }
                    moresp = (jsp < nsp);
                    next = spbuf[jsp];
                    ++ jsp;
                }
            }
            if (moresp) {// move back file pointer for fdata
                fseek(fdata, curr, SEEK_SET);
                begin = curr;
            }
            else
                break;
        }
    }
    else if (spbuf.size() > 1)  { // normal strings, use the second buffer
        std::string pat = str;
#if FASTBIT_CASE_SENSITIVE_COMPARE+0 == 0
        // convert to lower case
        for (uint32_t i = 0; i < pat.length(); ++ i)
            pat[i] = tolower(pat[i]);
#endif
        const uint32_t slen = pat.length() + 1;
        uint32_t jsp, nsp;
        ierr = fread(spbuf.address(), sizeof(int64_t), spbuf.size(), fsp);
        if (ierr <= 0) {
            LOGGER(ibis::gVerbose >= 0)
                << "Warning -- " << evt << " failed to read file " << sp;
            fclose(fsp);
            fclose(fdata);
            return -7L;
        }
        jsp = 1;
        nsp = ierr;
        next = spbuf[0];
        while ((jbuf = fread(buf, 1, nbuf, fdata)) > 0) {
#if FASTBIT_CASE_SENSITIVE_COMPARE+0 == 0
            for (long j = 0; j < jbuf; ++ j) // convert to lower case
                buf[j] = tolower(buf[j]);
#endif
            bool moresp = true;
            if (next > begin+jbuf) {
                LOGGER(ibis::gVerbose >= 0)
                    << "Warning -- " << evt
                    << " expects string # " << irow << " in file \""
                    << data << "\" to be " << (next-begin) << "-byte long, but "
                    << (jbuf<(long)nbuf ? "can only read " :
                        "the internal buffer is only ")
                    << jbuf << ", skipping " << jbuf
                    << (jbuf > 1 ? " bytes" : " byte");
                curr += jbuf;
            }
            while (begin+jbuf >= next) {
                bool match = (curr+(int64_t)slen == next); // same length?
                long j = curr;
                while (j+4 < next && match) {
                    match = (buf[j-begin] == pat[j-curr]) &&
                        (buf[j-begin+1] == pat[j-curr+1]) &&
                        (buf[j-begin+2] == pat[j-curr+2]) &&
                        (buf[j-begin+3] == pat[j-curr+3]);
                    j += 4;
                }
                if (match) {
                    if (j+4 == next) {
                        match = (buf[j-begin] == pat[j-curr]) &&
                            (buf[j-begin+1] == pat[j-curr+1]) &&
                            (buf[j-begin+2] == pat[j-curr+2]);
                    }
                    else if (j+3 == next) {
                        match = (buf[j-begin] == pat[j-curr]) &&
                            (buf[j-begin+1] == pat[j-curr+1]);
                    }
                    else if (j+2 == next) {
                        match = (buf[j-begin] == pat[j-curr]);
                    }
                }
                if (match)
                    hits.setBit(irow, 1);
#if _DEBUG+0 > 1 || DEBUG+0 > 1
                if (ibis::gVerbose > 5) {
                    ibis::util::logger lg(4);
                    lg() << "DEBUG -- " << evt << " processing string "
                         << irow << " \'";
                    for (long i = curr; i < next-1; ++ i)
                        lg() << buf[i-begin];
                    lg() << "\'";
                    if (match)
                        lg() << " == ";
                    else
                        lg() << " != ";
                    lg() << pat;
                }
#endif
                ++ irow;
                LOGGER(ibis::gVerbose > 2 && irow % 1000000 == 0)
                    << evt << " -- processed " << irow
                    << " strings from file " << data;

                curr = next;
                if (moresp) {
                    if (jsp >= nsp) {
                        if (feof(fsp) == 0) {
                            ierr = fread(spbuf.address(), sizeof(int64_t),
                                         spbuf.size(), fsp);
                            if (ierr <= 0) {
                                LOGGER(ierr < 0 && ibis::gVerbose >= 0)
                                    << "Warning -- " << evt
                                    << " failed to read file " << sp;
                                moresp = false;
                                break;
                            }
                            else {
                                nsp = ierr;
                            }
                        }
                        else { // end of sp file
                            moresp = false;
                            break;
                        }
                        jsp = 0;
                    }
                    moresp = (jsp < nsp);
                    next = spbuf[jsp];
                    ++ jsp;
                }
            }
            if (moresp) {// move back file pointer in fdata
                fseek(fdata, curr, SEEK_SET);
                begin = curr;
            }
            else
                break; // avoid reading the data file
        } // while (jbuf > 0) -- as long as there are bytes to examine
    }
    else if (str == 0 || *str == 0) { // only match empty strings
        ierr = fread(&next, sizeof(next), 1, fsp);
        while ((jbuf = fread(buf, 1, nbuf, fdata)) > 0) {
            bool moresp = true;
            if (next > begin+jbuf) {
                LOGGER(ibis::gVerbose >= 0)
                    << "Warning -- " << evt
                    << " expects string # " << irow << " in file \""
                    << data << "\" to be " << (next-begin) << "-byte long, but "
                    << (jbuf<(long)nbuf ? "can only read " :
                        "the internal buffer is only ")
                    << jbuf << ", skipping " << jbuf
                    << (jbuf > 1 ? " bytes" : " byte");
                curr += jbuf;
            }
            while (begin + jbuf >= next) {
                if (buf[curr-begin] == 0)
                    hits.setBit(irow, 1);
                ++ irow;
                curr = next;
                LOGGER(ibis::gVerbose > 2 && irow % 1000000 == 0)
                    << evt << " -- processed " << irow
                    << " strings from file " << data;

                moresp = (feof(fsp) == 0);
                if (moresp)
                    moresp = (1 == fread(&next, sizeof(next), 1, fsp));
                if (! moresp)
                    break;
            }
            if (moresp) {// move back file pointer for fdata
                //fseek(fsp, -static_cast<long>(sizeof(next)), SEEK_CUR);
                fseek(fdata, curr, SEEK_SET);
                begin = curr;
            }
            else
                break;
        }
    }
    else { // normal null-terminated strings
        std::string pat = str;
#if FASTBIT_CASE_SENSITIVE_COMPARE+0 == 0
        // convert the string to be search to lower case
        for (uint32_t i = 0; i < pat.length(); ++ i)
            pat[i] = tolower(pat[i]);
#endif
        const uint32_t slen = pat.length() + 1;
        ierr = fread(&next, sizeof(next), 1, fsp);
        while ((jbuf = fread(buf, 1, nbuf, fdata)) > 0) {
#if FASTBIT_CASE_SENSITIVE_COMPARE+0 == 0
            for (long j = 0; j < jbuf; ++ j) // convert to lower case
                buf[j] = tolower(buf[j]);
#endif
            bool moresp = true;
            if (next > begin+jbuf) {
                LOGGER(ibis::gVerbose >= 0)
                    << "Warning -- " << evt
                    << " expects string # " << irow << " in file \""
                    << data << "\" to be " << (next-begin) << "-byte long, but "
                    << (jbuf<(long)nbuf ? "can only read " :
                        "the internal buffer is only ")
                    << jbuf << ", skipping " << jbuf
                    << (jbuf > 1 ? " bytes" : " byte");
                curr += jbuf;
            }
            while (begin+jbuf >= next) { // has a whole string
                bool match = (curr+(int64_t)slen == next); // same length?
                long j = curr;
                while (j+4 < next && match) {
                    match = (buf[j-begin] == pat[j-curr]) &&
                        (buf[j-begin+1] == pat[j-curr+1]) &&
                        (buf[j-begin+2] == pat[j-curr+2]) &&
                        (buf[j-begin+3] == pat[j-curr+3]);
                    j += 4;
                }
                if (match) {
                    if (j+4 == next) {
                        match = (buf[j-begin] == pat[j-curr]) &&
                            (buf[j-begin+1] == pat[j-curr+1]) &&
                            (buf[j-begin+2] == pat[j-curr+2]);
                    }
                    else if (j+3 == next) {
                        match = (buf[j-begin] == pat[j-curr]) &&
                            (buf[j-begin+1] == pat[j-curr+1]);
                    }
                    else if (j+2 == next) {
                        match = (buf[j-begin] == pat[j-curr]);
                    }
                }
                if (match)
                    hits.setBit(irow, 1);
                ++ irow;
                LOGGER(ibis::gVerbose > 2 && irow % 1000000 == 0)
                    << evt << " -- processed " << irow
                    << " strings from file " << data;

                curr = next;
                moresp = (feof(fsp) == 0);
                if (moresp)
                    moresp = (1 == fread(&next, sizeof(next), 1, fsp));
                if (! moresp)
                    break;
            }
            if (moresp) {// move back file pointer for fdata
                // fseek(fsp, -static_cast<long>(sizeof(next)), SEEK_CUR);
                fseek(fdata, curr, SEEK_SET);
                begin = curr;
            }
            else
                break; // avoid reading the data file
        } // while (jbuf > 0) -- as long as there are bytes to examine
    }

    fclose(fsp);
    fclose(fdata);
    ibis::fileManager::instance().recordPages(0, next);
    ibis::fileManager::instance().recordPages
        (0, sizeof(uint64_t)*thePart->nRows());
    if (hits.size() != thePart->nRows()) {
        LOGGER(irow != thePart->nRows() && ibis::gVerbose >= 0)
            << "Warning -- " << evt << " expects " << thePart->nRows()
            << " entr" << (irow>1?"ies":"y") << " in file \"" << data
            << "\", but finds " << irow;
        if (irow < thePart->nRows())
            startPositions(thePart->currentDataDir(), buf, nbuf);
        hits.adjustSize(0, thePart->nRows());
    }

    LOGGER(ibis::gVerbose > 4)
        << evt << " found " << hits.cnt() << " string" << (hits.cnt()>1?"s":"")
        << " in \"" << data << "\" matching " << str;
    return hits.cnt();
} // ibis::text::stringSearch

/// Locate the rows match any of the given strings.
///
/// Return the number of hits upon successful completion of this function,
/// otherwise return a negative number to indicate error.
long ibis::text::stringSearch(const std::vector<std::string>& strs,
                              ibis::bitvector& hits) const {
    if (strs.empty()) {
        hits.set(0, thePart->nRows());
        return 0;
    }

    if (strs.size() == 1) // the list contains only one value
        return stringSearch(strs[0].c_str(), hits);

    hits.clear();
    if (thePart == 0) return -1L;

    std::string evt = "text[";
    if (thePart != 0 && thePart->name() != 0) {
        evt += thePart->name();
        evt += '.';
    }
    evt += m_name;
    evt += "]::stringSearch";
    std::string data = thePart->currentDataDir();
    data += FASTBIT_DIRSEP;
    data += m_name;
    FILE *fdata = fopen(data.c_str(), "rb");
    if (fdata == 0) {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- " << evt << " can not open data file \"" << data
            << "\" for reading";
        return -2L;
    }

#if defined(DEBUG) || defined(_DEBUG) // DEBUG+0 > 0 || _DEBUG+0 > 0
    ibis::fileManager::buffer<char> mybuf(5000);
#else
    ibis::fileManager::buffer<char> mybuf;
#endif
    char *buf = mybuf.address();
    uint32_t nbuf = mybuf.size();
    if (buf == 0 || nbuf == 0) return -3L;

    std::string sp = data;
    sp += ".sp";
    FILE *fsp = fopen(sp.c_str(), "rb");
    if (fsp == 0) { // try again
        startPositions(thePart->currentDataDir(), buf, nbuf);
        fsp = fopen(sp.c_str(), "rb");
        if (fsp == 0) { // really won't work out
            LOGGER(ibis::gVerbose >= 0)
                << "Warning -- " << evt << " can not create or open file \""
                << sp << "\"";
            fclose(fdata);
            return -4L;
        }
    }

    unsigned irow = 0; // row index
    long jbuf = 0; // number of bytes in buffer
    long ierr;
    int64_t begin = 0; // beginning position (file offset) of the bytes in buf
    int64_t curr = 0;
    int64_t next = 0;
    if (1 > fread(&curr, sizeof(curr), 1, fsp)) {
        // odd to be sure, but try again anyway
        fclose(fsp);
        startPositions(thePart->currentDataDir(), buf, nbuf);
        fsp = fopen(sp.c_str(), "rb");
        if (fsp == 0) { // really won't work out
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << evt << " can not open or read file \""
                << sp << "\"";
            fclose(fdata);
            return -5L;
        }
    }

#if defined(DEBUG) || defined(_DEBUG) // DEBUG+0 > 0 || _DEBUG+0 > 0
    ibis::fileManager::buffer<int64_t> spbuf(1000);
#else
    ibis::fileManager::buffer<int64_t> spbuf;
#endif
    if (spbuf.size() > 1) { // try to use the spbuf for starting positions
        uint32_t jsp, nsp;
        ierr = fread(spbuf.address(), sizeof(int64_t), spbuf.size(), fsp);
        if (ierr <= 0) {
            LOGGER(ibis::gVerbose >= 0)
                << "Warning -- " << evt << " failed to read " << sp;
            fclose(fsp);
            fclose(fdata);
            return -5L;
        }
        next = spbuf[0];
        nsp = ierr;
        jsp = 1;
        while ((jbuf = fread(buf, 1, nbuf, fdata)) > 0) {
            bool moresp = true;
            if (next > begin+jbuf) {
                LOGGER(ibis::gVerbose>0)
                    << "Warning -- " << evt << " string " << irow
                    << " in file \"" << data << "\" is longer "
                    "than internal buffer (size " << jbuf
                    << "), skipping " << jbuf << " bytes";
                curr += jbuf;
            }
            while (begin+jbuf >= next) {
                const char *str = buf + (curr - begin);
                bool match = false;
                for (uint32_t i = 0; i < strs.size() && match == false; ++ i) {
#if FASTBIT_CASE_SENSITIVE_COMPARE+0 == 0
                    match = (stricmp(strs[i].c_str(), str) == 0);
#else
                    match = (std::strcmp(strs[i].c_str(), str) == 0);
#endif
                }
                if (match)
                    hits.setBit(irow, 1);
                ++ irow;
                curr = next;
                if (moresp) {
                    if (jsp >= nsp) {
                        if (feof(fsp) == 0) {
                            ierr = fread(spbuf.address(), sizeof(int64_t),
                                         spbuf.size(), fsp);
                            if (ierr <= 0) {
                                LOGGER(ierr < 0 && ibis::gVerbose >= 0)
                                    << "Warning -- " << evt
                                    << " failed to read file " << sp;
                                moresp = false;
                                break;
                            }
                            else {
                                nsp = ierr;
                            }
                        }
                        else { // end of sp file
                            moresp = false;
                            break;
                        }
                        jsp = 0;
                    }
                    moresp = (jsp < nsp);
                    next = spbuf[jsp];
                    ++ jsp;
                }
            }
            if (moresp) {// move back file pointer to reread unused bytes
                fseek(fdata, curr, SEEK_SET);
                begin = curr;
            }
            else
                break;
        } // while (jbuf > 0) -- as long as there are bytes to examine
    }
    else {
        ierr = fread(&next, sizeof(next), 1, fsp);
        while ((jbuf = fread(buf, 1, nbuf, fdata)) > 0) {
            bool moresp = true;
            if (next > begin+jbuf) {
                LOGGER(ibis::gVerbose > 0)
                    << "Warning -- " << evt << " string " << irow
                    << " in file \"" << data << "\" is longer "
                    "than internal buffer (size " << jbuf << "), skipping "
                    << jbuf << " bytes";
                curr += jbuf;
            }
            while (begin+jbuf >= next) {
                const char *str = buf + (curr - begin);
                bool match = false;
                for (uint32_t i = 0; i < strs.size() && match == false; ++ i) {
#if FASTBIT_CASE_SENSITIVE_COMPARE+0 == 0
                    match = (stricmp(strs[i].c_str(), str) == 0);
#else
                    match = (std::strcmp(strs[i].c_str(), str) == 0);
#endif
                }
                if (match)
                    hits.setBit(irow, 1);
                ++ irow;
                curr = next;
                moresp = (feof(fsp) == 0);
                if (moresp)
                    moresp = (1 == fread(&next, sizeof(next), 1, fsp));
                if (! moresp)
                    break;
            }
            if (moresp) {// move back file pointer to reread some bytes
                fseek(fdata, curr, SEEK_SET);
                begin = curr;
            }
            else
                break;
        } // while (jbuf > 0) -- as long as there are bytes to examine
    }

    fclose(fsp);
    fclose(fdata);
    ibis::fileManager::instance().recordPages(0, next);
    ibis::fileManager::instance().recordPages
        (0, sizeof(uint64_t)*thePart->nRows());
    if (hits.size() != thePart->nRows()) {
        LOGGER(irow != thePart->nRows() && ibis::gVerbose >= 0)
            << "Warning -- " << evt << " expects " << thePart->nRows()
            << " entr" << (irow>1?"ies":"y") << " in file \"" << data
            << "\", but finds " << irow;
        if (hits.size() < thePart->nRows())
            startPositions(thePart->currentDataDir(), buf, nbuf);
        hits.adjustSize(0, thePart->nRows());
    }

    LOGGER(ibis::gVerbose > 4)
        << evt << " found " << hits.cnt() << " string" << (hits.cnt()>1?"s":"")
        << " in \"" << data << "\" matching " << strs.size() << " strings";
    return hits.cnt();
} // ibis::text::stringSearch

long ibis::text::patternSearch(const char*) const {
    return (thePart ? thePart->nRows() : INT_MAX);
} // ibis::text::patternSearch

long ibis::text::patternSearch(const char* pat, ibis::bitvector& hits) const {
    hits.clear(); // clear the existing content of hits
    if (thePart == 0 || pat == 0 || *pat == 0) return -1L;

    std::string evt = "text[";
    if (thePart != 0 && thePart->name() != 0) {
        evt += thePart->name();
        evt += '.';
    }
    evt += m_name;
    evt += "]::patternSearch";
    ibis::util::timer mytimer(evt.c_str(), 4);
    std::string data = thePart->currentDataDir();
    data += FASTBIT_DIRSEP;
    data += m_name;
    FILE *fdata = fopen(data.c_str(), "rb");
    if (fdata == 0) {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- " << evt << " can not open data file \"" << data
            << "\" for reading";
        return -2L;
    }

    IBIS_BLOCK_GUARD(fclose, fdata);
#if defined(DEBUG) || defined(_DEBUG) // DEBUG+0 > 0 || _DEBUG+0 > 0
    ibis::fileManager::buffer<char> mybuf(5000);
#else
    ibis::fileManager::buffer<char> mybuf;
#endif
    char *buf = mybuf.address();
    uint32_t nbuf = mybuf.size();
    if (buf == 0 || nbuf == 0) return -3L;

    std::string sp = data;
    sp += ".sp";
    FILE *fsp = fopen(sp.c_str(), "rb");
    if (fsp == 0) { // try again
        startPositions(thePart->currentDataDir(), buf, nbuf);
        fsp = fopen(sp.c_str(), "rb");
        if (fsp == 0) { // really won't work out
            LOGGER(ibis::gVerbose >= 0)
                << "Warning -- " << evt << " can not create or open file \""
                << sp << "\"";
            return -4L;
        }
    }

#if defined(DEBUG) || defined(_DEBUG) // DEBUG+0 > 0 || _DEBUG+0 > 0
    ibis::fileManager::buffer<int64_t> spbuf(100);
#else
    ibis::fileManager::buffer<int64_t> spbuf;
#endif
    uint32_t irow = 0; // row index
    long jbuf = 0; // number of bytes in buffer
    int64_t begin = 0; // beginning position (file offset) of the bytes in buf
    int64_t next = 0;
    int64_t curr, ierr;
    if (1 > fread(&curr, sizeof(curr), 1, fsp)) {
        // odd to be sure, but try again anyway
        fclose(fsp);
        startPositions(thePart->currentDataDir(), buf, nbuf);
        fsp = fopen(sp.c_str(), "rb");
        if (fsp == 0) { // really won't work out
            LOGGER(ibis::gVerbose >= 0)
                << "Warning -- " << evt <<  " can not open or read file \""
                << sp << "\"";
            return -5L;
        }
    }
    IBIS_BLOCK_GUARD(fclose, fsp);
    if (spbuf.size() > 1)  { // use the second buffer, spbuf
        uint32_t jsp, nsp;
        ierr = fread(spbuf.address(), sizeof(int64_t), spbuf.size(), fsp);
        if (ierr <= 0) {
            LOGGER(ibis::gVerbose >= 0)
                << "Warning -- " << evt << " failed to read file " << sp;
            return -7L;
        }
        jsp = 1;
        nsp = ierr;
        next = spbuf[0];
        while ((jbuf = fread(buf, 1, nbuf, fdata)) > 0) {
            bool moresp = true;
            if (next > begin+jbuf) {
                LOGGER(ibis::gVerbose >= 0)
                    << "Warning -- " << evt
                    << " expects string # " << irow << " in file \""
                    << data << "\" to be " << (next-begin) << "-byte long, but "
                    << (jbuf<(long)nbuf ? "can only read " :
                        "the internal buffer is only ")
                    << jbuf << ", skipping " << jbuf
                    << (jbuf > 1 ? " bytes" : " byte");
                curr += jbuf;
            }
            while (begin+jbuf >= next) { // whole string
                const bool match = ibis::util::strMatch(buf+(curr-begin), pat);
                if (match)
                    hits.setBit(irow, 1);
#if _DEBUG+0 > 1 || DEBUG+0 > 1
                if (ibis::gVerbose > 5) {
                    ibis::util::logger lg(4);
                    lg()
                        << "DEBUG -- " << evt << " processing string "
                        << irow << " \'";
                    for (long i = curr; i < next-1; ++ i)
                        lg() << buf[i-begin];
                    lg() << "\'";
                    if (match)
                        lg() << " matches ";
                    else
                        lg() << " does not match ";
                    lg() << pat;
                }
#endif
                ++ irow;
                LOGGER(ibis::gVerbose > 2 && irow % 1000000 == 0)
                    << evt << " -- processed " << irow
                    << " strings from file " << data;

                curr = next;
                if (moresp) {
                    if (jsp >= nsp) {
                        if (feof(fsp) == 0) {
                            ierr = fread(spbuf.address(), sizeof(int64_t),
                                         spbuf.size(), fsp);
                            if (ierr <= 0) {
                                LOGGER(ierr < 0 && ibis::gVerbose >= 0)
                                    << "Warning -- " << evt
                                    << " failed to read " << sp;
                                moresp = false;
                                break;
                            }
                            else {
                                nsp = ierr;
                            }
                        }
                        else { // end of sp file
                            moresp = false;
                            break;
                        }
                        jsp = 0;
                    }
                    moresp = (jsp < nsp);
                    next = spbuf[jsp];
                    ++ jsp;
                }
            }
            if (moresp) {// move back file pointer in fdata
                fseek(fdata, curr, SEEK_SET);
                begin = curr;
            }
            else
                break; // avoid reading the data file
        } // while (jbuf > 0) -- as long as there are bytes to examine
    }
    else { // normal null-terminated strings, no spbuf
        ierr = fread(&next, sizeof(next), 1, fsp);
        while ((jbuf = fread(buf, 1, nbuf, fdata)) > 0) {
            bool moresp = true;
            if (next > begin+jbuf) {
                LOGGER(ibis::gVerbose >= 0)
                    << "Warning -- " << evt
                    << " expects string # " << irow << " in file \""
                    << data << "\" to be " << (next-begin) << "-byte long, but "
                    << (jbuf<(long)nbuf ? "can only read " :
                        "the internal buffer is only ")
                    << jbuf << ", skipping " << jbuf
                    << (jbuf > 1 ? " bytes" : " byte");
                curr += jbuf;
            }
            while (begin+jbuf >= next) { // has a whole string
                const bool match = ibis::util::strMatch(buf+(curr-begin), pat);
                if (match)
                    hits.setBit(irow, 1);
                ++ irow;
                LOGGER(ibis::gVerbose > 2 && irow % 1000000 == 0)
                    << evt << " -- processed " << irow
                    << " strings from file " << data;

                curr = next;
                moresp = (feof(fsp) == 0);
                if (moresp)
                    moresp = (1 == fread(&next, sizeof(next), 1, fsp));
                if (! moresp)
                    break;
            }
            if (moresp) {// move back file pointer for fdata
                // fseek(fsp, -static_cast<long>(sizeof(next)), SEEK_CUR);
                fseek(fdata, curr, SEEK_SET);
                begin = curr;
            }
            else
                break; // avoid reading the data file
        } // while (jbuf > 0) -- as long as there are bytes to examine
    }

    ibis::fileManager::instance().recordPages(0, next);
    ibis::fileManager::instance().recordPages
        (0, sizeof(uint64_t)*thePart->nRows());
    if (hits.size() != thePart->nRows()) {
        LOGGER(irow != thePart->nRows() && ibis::gVerbose >= 0)
            << "Warning -- " << evt << "data file \"" << data
            << "\" contains " << irow << " string" << (irow>1?"s":"")
            << ", but expected " << thePart->nRows();
        if (irow < thePart->nRows())
            startPositions(thePart->currentDataDir(), buf, nbuf);
        hits.adjustSize(0, thePart->nRows());
    }

    LOGGER(ibis::gVerbose > 4)
        << evt << " found " << hits.cnt() << " string" << (hits.cnt()>1?"s":"")
        << " in \"" << data << "\" matching " << pat;
    return hits.cnt();
} // ibis::text::patternSearch

/// Write the current metadata to -part.txt of the data partition.
void ibis::text::write(FILE* file) const {
    fputs("\nBegin Column\n", file);
    fprintf(file, "name = \"%s\"\n", (const char*)m_name.c_str());
    if (m_desc.empty() || m_desc == m_name) {
        fprintf(file, "description = %s ", m_name.c_str());
        fprintf(file, "\n");
    }
    else {
        if (m_desc.size() > MAX_LINE-60)
            const_cast<std::string&>(m_desc).erase(MAX_LINE-60);
        fprintf(file, "description =\"%s\"\n",
                (const char*)m_desc.c_str());
    }
    fprintf(file, "data_type = \"%s\"\n", TYPESTRING[m_type]);
//     fprintf(file, "minimum = %lu\n", static_cast<long unsigned>(lower));
//     fprintf(file, "maximum = %lu\n", static_cast<long unsigned>(upper));
    if (! m_bins.empty())
        fprintf(file, "index=%s\n", m_bins.c_str());
    fputs("End Column\n", file);
} // ibis::text::write

void ibis::text::print(std::ostream& out) const {
    out << m_name << ": " << m_desc << " (STRING)";
}

/// This indicates to ibis::bundle that every string value is distinct.  It
/// also forces the sorting procedure to produce an order following the
/// order of the entries in the table.  This makes the print out of an
/// ibis::text field quite less useful than others!
ibis::array_t<uint32_t>*
ibis::text::selectUInts(const ibis::bitvector& mask) const {
    array_t<uint32_t>* ret = new array_t<uint32_t>;
    for (ibis::bitvector::indexSet ix = mask.firstIndexSet();
         ix.nIndices() > 0; ++ ix) {
        const ibis::bitvector::word_t *ind = ix.indices();
        if (ix.isRange()) {
            for (unsigned i = *ind; i < ind[1]; ++ i)
                ret->push_back(i);
        }
        else {
            for (uint32_t i = 0; i < ix.nIndices(); ++ i)
                ret->push_back(ind[i]);
        }
    }
    return ret;
} // ibis::text::selectUInts

/// The starting positions of the selected string values are stored in the
/// returned array.
ibis::array_t<int64_t>*
ibis::text::selectLongs(const ibis::bitvector& mask) const {
    std::string fnm = thePart->currentDataDir();
    fnm += FASTBIT_DIRSEP;
    fnm += m_name;
    fnm += ".sp"; // starting position file
    off_t spsize = ibis::util::getFileSize(fnm.c_str());
    if (spsize < 0 || (uint32_t)spsize != (mask.size()+1)*sizeof(int64_t))
        startPositions(thePart->currentDataDir(), (char*)0, 0U);
    array_t<int64_t> sp;
    int ierr = ibis::fileManager::instance().getFile(fnm.c_str(), sp);
    if (ierr != 0) return 0; // can not provide starting positions

    array_t<int64_t>* ret = new array_t<int64_t>;
    for (ibis::bitvector::indexSet ix = mask.firstIndexSet();
         ix.nIndices() > 0; ++ ix) {
        const ibis::bitvector::word_t *ind = ix.indices();
        if (*ind >= sp.size()) {
            break;
        }
        else if (ix.isRange()) {
            const ibis::bitvector::word_t end =
                (ind[1] <= sp.size() ? ind[1] : sp.size());
            for (unsigned i = *ind; i < end; ++ i)
                ret->push_back(sp[i]);
        }
        else {
            for (uint32_t i = 0; i < ix.nIndices(); ++ i)
                if (ind[i] < sp.size())
                    ret->push_back(sp[ind[i]]);
        }
    }
    return ret;
} // ibis::text::selectLongs

/// Retrieve the string values from the rows marked 1 in mask.
///
/// @note FastBit does not track the memory usage of neither std::vector
/// nor std::string.
std::vector<std::string>*
ibis::text::selectStrings(const ibis::bitvector& mask) const {
    std::unique_ptr< std::vector<std::string> >
        res(new std::vector<std::string>());
    if (mask.cnt() == 0) return res.release();

    int ierr;
    std::string evt = "text";
    if (ibis::gVerbose > 1) {
        evt += "[";
        evt += fullname();
        evt += "]";
    }
    evt += "::selectStrings";

    std::string fname = thePart->currentDataDir();
    fname += FASTBIT_DIRSEP;
    fname += m_name;
    fname += ".sp";
    off_t spsize = ibis::util::getFileSize(fname.c_str());
    if (spsize < 0 || (uint32_t)spsize != (mask.size()+1)*sizeof(int64_t)) {
        startPositions(thePart->currentDataDir(), (char*)0, 0U);
        spsize = ibis::util::getFileSize(fname.c_str());
        if (spsize < 0 || (uint32_t)spsize != (mask.size()+1)*sizeof(int64_t)) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << evt
                << " failed to create .sp file after retrying";
            return 0;
        }
    }

    try {
        // compute the threshold for deciding when to invoke readStrings1
        // or readString2
        size_t thr = ibis::util::log2(mask.size());
        if (thr > 6U && mask.cnt() > thr) {
            // when the number of rows > 2^6 (64) and number of elements to
            // be read is more than log_2(N), then try readStrings2
            ierr = readStrings2(mask, *res);
        }
        else {
            // the total number of strings is small or the number of
            // strings to be read is small
            ierr = readStrings1(mask, *res);
        }
    }
    catch (...) {
        // attempt to reading the strings one at a time
        ierr = readStrings1(mask, *res);
    }

    if (ierr >= 0) {
        LOGGER(ibis::gVerbose > 4)
            << evt << " read " << res->size() << " string"
            << (res->size() > 1 ? "s" : "") << ", " << mask.cnt()
            << " expected";
        return res.release();
    }
    else {
        LOGGER(ibis::gVerbose > 1)
            << "Warning -- " << evt << " failed with error " << ierr
            << " from readStrings1 or readStrings2";
        return 0;
    }
} // ibis::text::selectStrings

/// Read one string from an open file.
/// The string starts at position @c be and ends at @c en.  The content may
/// be in the array @c buf.
///
/// Returns 0 if successful, otherwise return a negative number to indicate
/// error.
int ibis::text::readString(std::string& res, int fdes, long be, long en,
                           char* buf, uint32_t nbuf, uint32_t& inbuf,
                           off_t& boffset) const {
    res.clear();
    if (boffset + (off_t)inbuf >= en) { // in buffer
        res = buf + (be - boffset);
    }
    else if (boffset + (off_t)inbuf > be) { // partially in buffer
        for (uint32_t j = be - boffset; j < inbuf; ++ j)
            res += buf[j];

        off_t ierr = UnixSeek(fdes, boffset+inbuf, SEEK_SET);
        if (ierr != boffset+(long)inbuf) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- text[" << fullname()
                << "]::readString failed to move file pointer to "
                << (boffset+inbuf);
            return -1;
        }
        ierr = UnixRead(fdes, buf, nbuf);
        if (ierr < 0) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- text[" << fullname()
                << "]::readString failed to read from data file position "
                << (boffset+inbuf);
            inbuf = 0;
            return -2;
        }

        ibis::fileManager::instance().recordPages
            (boffset+inbuf, boffset+inbuf+nbuf);
        boffset += static_cast<long>(inbuf);
        inbuf = static_cast<uint32_t>(ierr);
        be = boffset;
        while ((long)(boffset + inbuf) < en) {
            for (uint32_t j = 0; j < inbuf; ++ j)
                res += buf[j];
            ierr = UnixRead(fdes, buf, nbuf);
            if (ierr < 0) {
                LOGGER(ibis::gVerbose > 1)
                    << "Warning -- text[" << fullname()
                    << "]::readString failed to read from data file position "
                    << (boffset+inbuf);
                inbuf = 0;
                return -3;
            }
            boffset += inbuf;
            inbuf = static_cast<uint32_t>(ierr);
        }
        res += buf;
    }
    else { // start reading from @c be
        off_t ierr = UnixSeek(fdes, be, SEEK_SET);
        if (ierr != static_cast<off_t>(be)) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- text[" << fullname()
                << "]::readString failed to move file pointer to " << be;
            return -4;
        }
        ierr = UnixRead(fdes, buf, nbuf);
        if (ierr < 0) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- text[" << fullname()
                << "]::readString failed to read from data file position "
                << be;
            inbuf = 0;
            return -5;
        }

        ibis::fileManager::instance().recordPages(be, be+nbuf);
        boffset = be;
        inbuf = static_cast<uint32_t>(ierr);
        while (en > boffset+(off_t)inbuf) {
            for (uint32_t j = 0; j < inbuf; ++ j)
                res += buf[j];
            ierr = UnixRead(fdes, buf, nbuf);
            if (ierr < 0) {
                LOGGER(ibis::gVerbose > 1)
                    << "Warning-- text[" << fullname()
                    << "]::readString failed to read from data file position "
                    << be;
                inbuf = 0;
                return -6;
            }
            boffset += inbuf;
            inbuf = static_cast<uint32_t>(ierr);
        }
        res += buf;
    }
#if _DEBUG+0 > 2 || DEBUG+0 > 1
    LOGGER(ibis::gVerbose > 5)
        << "DEBUG -- text[" << fullname()
        << "]::readString got string value \"" << res
        << "\" from file descriptor " << fdes << " offsets " << be << " -- "
        << en;
#endif
    return 0;
} // ibis::text::readString

/// Read the string value of <code>i</code>th row.
/// It goes through a two-stage process by reading from two files, first
/// from the .sp file to read the position of the string in the second file
/// and the second file contains the actual string values (with nil
/// terminators).  This can be quite slow!
int ibis::text::readString(uint32_t i, std::string &ret) const {
    ret.clear();
    if (thePart == 0 || i >= thePart->nRows() ||
        thePart->currentDataDir() == 0 ||
        *(thePart->currentDataDir()) == 0) return -1;
    std::string fnm = thePart->currentDataDir();
    fnm += FASTBIT_DIRSEP;
    fnm += m_name;
    fnm += ".sp"; // starting position file

    long ierr = 0;
    int64_t positions[2];
    // open the file explicitly to read two starting positions
    int des = UnixOpen(fnm.c_str(), OPEN_READONLY);
    if (des < 0) {
        startPositions(thePart->currentDataDir(), 0, 0);
        des = UnixOpen(fnm.c_str(), OPEN_READONLY);
        if (des < 0) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- text::readString failed to open file \""
                << fnm << "\"";
            return -2;
        }
    }
#if defined(_WIN32) && defined(_MSC_VER)
    (void)_setmode(des, _O_BINARY);
#endif
    ierr = UnixSeek(des, i*sizeof(int64_t), SEEK_SET);
    if (ierr != static_cast<long>(i*sizeof(int64_t))) {
        (void) UnixClose(des);
        startPositions(thePart->currentDataDir(), 0, 0);
        des = UnixOpen(fnm.c_str(), OPEN_READONLY);
        if (des < 0) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- text::readString failed to open file \""
                << fnm << "\"";
            return -3;
        }

        ierr = UnixSeek(des, i*sizeof(int64_t), SEEK_SET);
        if (ierr != (long) (i*sizeof(int64_t))) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- text::readString(" << i << ") failed to seek to "
                << i*sizeof(int64_t) << " in " << fnm;
            (void) UnixClose(des);
            return -4;
        }
    }
    ierr = UnixRead(des, &positions, sizeof(positions));
    if (ierr != static_cast<long>(sizeof(positions))) {
        (void) UnixClose(des);
        startPositions(thePart->currentDataDir(), 0, 0);
        des = UnixOpen(fnm.c_str(), OPEN_READONLY);
        if (des < 0) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- text::readString failed to open file \""
                << fnm << "\"";
            return -5;
        }

        ierr = UnixSeek(des, i*sizeof(int64_t), SEEK_SET);
        if (ierr != static_cast<long>(i*sizeof(int64_t))) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- text::readString(" << i << ") failed to seek to "
                << i*sizeof(int64_t) << " in " << fnm;
            (void) UnixClose(des);
            return -6;
        }

        ierr = UnixRead(des, &positions, sizeof(positions));
        if (ierr != static_cast<long>(sizeof(positions))) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- text::readString(" << i << ") failed to read "
                << sizeof(positions) << " bytes from " << fnm;
            (void) UnixClose(des);
            return -7;
        }
    }
    (void) UnixClose(des);
    ibis::fileManager::instance().recordPages
        (i*sizeof(int64_t), i*sizeof(int64_t)+sizeof(positions));

    fnm.erase(fnm.size()-3); // remove ".sp"
    int datafile = UnixOpen(fnm.c_str(), OPEN_READONLY);
    if (datafile < 0) {
        LOGGER(ibis::gVerbose > 1)
            << "Warning -- text::readString failed to open file \""
            << fnm << "\"";
        return -8;
    }
#if defined(_WIN32) && defined(_MSC_VER)
    (void)_setmode(datafile, _O_BINARY);
#endif
    ierr = UnixSeek(datafile, *positions, SEEK_SET);
    if (ierr != *positions) {
        LOGGER(ibis::gVerbose > 1)
            << "Warning -- text::readString(" << i << ") failed to seek to "
            << *positions << " in file " << fnm;
        return -9;
    }
    char buf[1025];
    buf[1024] = 0;
    for (long j = positions[0]; j < positions[1]; j += 1024) {
        long len = positions[1] - j;
        if (len > 1024)
            len = 1024;
        ierr = UnixRead(datafile, buf, len);
        if (ierr > 0) {
            LOGGER(ibis::gVerbose > 2 && ierr < len)
                << "Warning -- text::readString(" << i << ") expected to read "
                << len << " bytes, but only read " << ierr;
            ret.insert(ret.end(), buf, buf + ierr - (buf[ierr-1] == 0));
        }
        else {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- text::readString(" << i << ") failed to read "
                << len << " bytes from " << fnm << ", read returned " << ierr;
        }
    }
    (void) UnixClose(datafile);
    ibis::fileManager::instance().recordPages(positions[0], positions[1]);
    return 0;
} // ibis::text::readString

/// Read the strings marked 1 in the mask.  It goes through a two-stage
/// process by reading from two files, first from the .sp file to read the
/// position of the string in the second file containing the actual string
/// values (with nil terminators), and then go to the second file to read
/// the string value.  This can be quite slow if you are attempting to read
/// a lot of strings, however it could be faster than readStrings2 if only
/// a few strings are expected.
///
/// @note This function assumes that the .sp file has been prepared
/// properly.
int ibis::text::readStrings1(const ibis::bitvector &msk,
                             std::vector<std::string> &ret) const {
    ret.clear();
    if (msk.empty()) return 0;
    if (thePart == 0 || thePart->currentDataDir() == 0 ||
        *(thePart->currentDataDir()) == 0) return -1;
    std::string evt = "text";
    if (ibis::gVerbose > 1) {
        evt += '[';
        evt += fullname();
        evt += ']';
    }
    evt += "::readStrings1";

    ibis::util::timer mytimer(evt.c_str(), 4);
    std::string fnm = thePart->currentDataDir();
    fnm += FASTBIT_DIRSEP;
    fnm += m_name;
    fnm += ".sp"; // starting position file
    try {
        ret.reserve(msk.cnt());
    }
    catch (...) {
        LOGGER(ibis::gVerbose > 1)
            << "Warning -- " << evt << " failed to allocate space for "
            << msk.cnt() << " strings to be read";
        ret.clear();
        return -2;
    }

    long ierr = 0;
    int64_t positions[2];
    // open the file with the starting positions
    int dsp = UnixOpen(fnm.c_str(), OPEN_READONLY);
    if (dsp < 0) {
        startPositions(thePart->currentDataDir(), 0, 0);
        dsp = UnixOpen(fnm.c_str(), OPEN_READONLY);
        if (dsp < 0) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- " << evt << " failed to open file \""
                << fnm << "\" for reading";
            return -3;
        }
    }
    IBIS_BLOCK_GUARD(UnixClose, dsp);
#if defined(_WIN32) && defined(_MSC_VER)
    (void)_setmode(dsp, _O_BINARY);
#endif

    // open the file with the raw string values
    fnm.erase(fnm.size()-3);
    int draw = UnixOpen(fnm.c_str(), OPEN_READONLY);
    if (draw < 0) {
        LOGGER(ibis::gVerbose > 1)
            << "Warning -- " << evt << " failed to open file \""
            << fnm << "\" for reading";
        return -4;
    }
    IBIS_BLOCK_GUARD(UnixClose, draw);
#if defined(_WIN32) && defined(_MSC_VER)
    (void)_setmode(draw, _O_BINARY);
#endif

    for (ibis::bitvector::indexSet ix = msk.firstIndexSet();
         ix.nIndices() > 0;
         ++ ix) {
        const ibis::bitvector::word_t *ind = ix.indices();
        positions[1] = *ind * sizeof(int64_t);
        ierr = UnixSeek(dsp, positions[1], SEEK_SET);
        if (ierr != positions[1]) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- " << evt << " failed to locate position "
                << positions[1] << " in the .sp file";
            return -5;
        }

        if (ix.isRange()) {
            ibis::fileManager::instance().recordPages
                (ierr, ierr+8*ix.nIndices()+8);
            ierr = UnixRead(dsp, positions, sizeof(int64_t));
            if (ierr != 8) {
                LOGGER(ibis::gVerbose > 1)
                    << "Warning -- " << evt << " failed to read the starting "
                    "position at " << positions[1] << std::endl;
                return -6;
            }
            for (unsigned j = ind[0]; j < ind[1]; ++ j) {
                ierr = UnixRead(dsp, positions+1, sizeof(int64_t));
                ierr = UnixSeek(draw, positions[0], SEEK_SET);
                ierr = positions[1] - positions[0];
                std::string tmp;
                if (ierr > 1) {
                    -- ierr;
                    tmp.resize(ierr);
                    ierr = UnixRead(draw, const_cast<char*>(tmp.data()), ierr);
                }
                positions[0] = positions[1];
                ret.push_back(tmp);
            }
        }
        else {
            for (unsigned j = 0; j < ix.nIndices(); ++ j) {
                positions[1] = ind[j] * sizeof(int64_t);
                ibis::fileManager::instance().recordPages
                    (positions[1], positions[1]+16);
                ierr = UnixSeek(dsp, positions[1], SEEK_SET);
                if (ierr != positions[1]) {
                    LOGGER(ibis::gVerbose > 1)
                        << "Warning -- " << evt << " failed to seek to "
                        "position at " << positions[1] << std::endl;
                    return -7;
                }
                ierr = UnixRead(dsp, positions, sizeof(int64_t)*2);
                ierr = UnixSeek(draw, positions[0], SEEK_SET);
                ierr = positions[1] - positions[0];
                std::string tmp;
                if (ierr > 1) {
                    -- ierr;
                    tmp.resize(ierr);
                    ierr = UnixRead(draw, const_cast<char*>(tmp.data()), ierr);
                }
                ret.push_back(tmp);
            }
        }
    }

    LOGGER(ibis::gVerbose > 2)
        << evt << " completed processing " << fnm << " to locate "
        << ret.size() << " string value" << (ret.size()>1?"s":"")
        << ", expected " << msk.cnt();
    return ret.size();
} // ibis::text::readStrings1

/// Read the strings marked 1 in the mask.  It creates a memory map of the
/// .sp file before reading the actual string values.  Because the process
/// of creating the memory map can take more time than reading a few values,
/// this function is much more appropriate for reading a relatively large
/// number of string values.
///
/// @note This function assumes that the .sp file has been prepared
/// properly.
int ibis::text::readStrings2(const ibis::bitvector& mask,
                             std::vector<std::string>& res) const {
    res.clear();
    if (mask.cnt() == 0) return 0;
    if (thePart == 0 || thePart->currentDataDir() == 0 ||
        *(thePart->currentDataDir()) == 0) return -1;

    int ierr;
    std::string evt = "text";
    if (ibis::gVerbose > 1) {
        evt += "[";
        evt += fullname();
        evt += "]";
    }
    evt += "::readStrings2";

    ibis::util::timer mytime(evt.c_str(), 4);
    std::string fnm = thePart->currentDataDir();
    fnm += FASTBIT_DIRSEP;
    fnm += m_name;
    fnm += ".sp";
    const array_t<int64_t>
        sp(fnm.c_str(), static_cast<off_t>(0),
           static_cast<off_t>((mask.size()+1)*sizeof(int64_t)));
    if (sp.size() != mask.size()+1U) {
        LOGGER(ibis::gVerbose > 1)
            << "Warning -- " << evt << " failed to find " << mask.size()+1U
            << " elements in .sp file " << fnm;
        return -2;
    }

    fnm.erase(fnm.size()-3); // remove .sp
    int fdata = UnixOpen(fnm.c_str(), OPEN_READONLY);
    if (fdata < 0) {
        LOGGER(ibis::gVerbose > 1)
            << "Warning -- " << evt << " failed to open data file "
            << fnm;
        return -3;
    }
    IBIS_BLOCK_GUARD(UnixClose, fdata);
#if defined(_WIN32) && defined(_MSC_VER)
    (void)_setmode(fdata, _O_BINARY);
#endif

    std::string tmp;
    off_t boffset = 0;
    uint32_t inbuf = 0;
    ibis::fileManager::buffer<char> mybuf;
    char* buf = mybuf.address();
    uint32_t nbuf = mybuf.size();
    if (buf == 0 || nbuf == 0) {
        LOGGER(ibis::gVerbose > 1)
            << "Warning -- " << evt
            << " failed to allocate buffer for reading";
        return -4;
    }

    for (ibis::bitvector::indexSet ix = mask.firstIndexSet();
         ix.nIndices() > 0; ++ ix) {
        const ibis::bitvector::word_t *ixval = ix.indices();
        if (ix.isRange()) {
            const ibis::bitvector::word_t top =
                (ixval[1] <= sp.size()-1 ? ixval[1] : sp.size()-1);
            for (ibis::bitvector::word_t i = *ixval; i < top; ++ i) {
                ierr = readString(tmp, fdata, sp[i], sp[i+1],
                                  buf, nbuf, inbuf, boffset);
                if (ierr >= 0) {
                    res.push_back(tmp);
                }
                else {
                    LOGGER(ibis::gVerbose >= 0)
                        << "Warning -- " << evt
                        << " failed to read from file \"" << fnm
                        << "\" (position " << sp[i] <<
                        "), readString returned ierr = " << ierr;
                    return ierr;
                }
            }
        }
        else {
            for (unsigned i = 0; i < ix.nIndices(); ++ i) {
                if (ixval[i] < sp.size()-1) {
                    ierr = readString(tmp, fdata, sp[ixval[i]],
                                      sp[ixval[i]+1],
                                      buf, nbuf, inbuf, boffset);
                    if (ierr >= 0) {
                        res.push_back(tmp);
                    }
                    else {
                        LOGGER(ibis::gVerbose >= 0)
                            << (thePart != 0 ? thePart->name() : "")
                            << " failed to read from file \"" << fnm
                            << "\" (position " << sp[ixval[i]]
                            << "), readString returned ierr = " << ierr;
                        return ierr;
                    }
                }
            }
        }
    }

    ibis::fileManager::instance().recordPages(0, 8*(mask.size()+1));
    LOGGER(ibis::gVerbose > 2)
        << evt << " completed processing " << fnm << " to locate "
        << res.size() << " string value" << (res.size()>1?"s":"")
        << ", expected " << mask.cnt();
    return res.size();
} // ibis::text::readStrings2

/// If the input string is found in the data file, it is returned, else
/// this function returns 0.  It needs to keep both the data file and the
/// starting position file open at the same time.
const char* ibis::text::findString(const char *str) const {
    std::string data = thePart->currentDataDir();
    data += FASTBIT_DIRSEP;
    data += m_name;
    FILE *fdata = fopen(data.c_str(), "rb");
    if (fdata == 0) {
        LOGGER(ibis::gVerbose > 1)
            << "Warning -- text::findString can not open data file \""
            << data << "\" for reading";
        return 0;
    }

    ibis::fileManager::buffer<char> mybuf;
    char *buf = mybuf.address();
    uint32_t nbuf = mybuf.size();
    if (buf == 0 || nbuf == 0) {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- text["
            << (thePart != 0 ? thePart->name() : "") << "." << name()
            << "]::findString(" << str << ") failed to allocate "
            "enough work space";
        return 0;
    }

    std::string sp = data;
    sp += ".sp";
    FILE *fsp = fopen(sp.c_str(), "rb");
    if (fsp == 0) { // try again
        startPositions(thePart->currentDataDir(), buf, nbuf);
        fsp = fopen(sp.c_str(), "rb");
        if (fsp == 0) { // really won't work out
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- text::findString can not create or open file \""
                << sp << "\"";
            fclose(fdata);
            return 0;
        }
    }

    uint32_t irow = 0; // row index
    long begin = 0; // beginning position (file offset) of the bytes in buf
    long jbuf = 0; // number of bytes in buffer
    long curr;
    if (1 > fread(&curr, sizeof(curr), 1, fsp)) {
        // odd to be sure, but try again anyway
        fclose(fsp);
        startPositions(thePart->currentDataDir(), buf, nbuf);
        fsp = fopen(sp.c_str(), "rb");
        if (fsp == 0) { // really won't work out
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- text::findString can not create, open or read "
                "starting positions file \"" << sp << "\"";
            fclose(fdata);
            return 0;
        }
    }

    long ierr;
    long next = 0;
    bool found = false;
    if (str == 0 || *str == 0) { // only match empty strings
        while ((jbuf = fread(buf, 1, nbuf, fdata)) > 0 && ! found) {
            bool moresp = true;
            ierr = fread(&next, sizeof(next), 1, fsp);
            if (ierr < 1 || next > begin+jbuf) {
                LOGGER(ibis::gVerbose > 1)
                    << "Warning -- text::findString is to skip " << jbuf
                    << " bytes of string " << irow << " in file \""
                    << data << "\" because the string is too long "
                    "for the internal buffer of size " << jbuf;
                curr += jbuf;
            }
            while (begin + jbuf >= next) {
                if (buf[curr-begin] == 0) {
                    found = true;
                    break;
                }
                ++ irow;
                curr = next;
                moresp = (feof(fsp) == 0);
                if (moresp)
                    moresp = (1 == fread(&next, sizeof(next), 1, fsp));
                if (! moresp)
                    break;
            }
            if (moresp) {// move back a word
                fseek(fsp, -static_cast<long>(sizeof(next)), SEEK_CUR);
                fseek(fdata, curr, SEEK_SET);
                begin = curr;
            }
            else
                break;
        }
    }
    else { // normal null-terminated strings
        const long slen = std::strlen(str);
        while ((jbuf = fread(buf, 1, nbuf, fdata)) > 0 && ! found) {
            bool moresp = true;
            ierr = fread(&next, sizeof(next), 1, fsp);
            if (ierr < 1 || next > begin+jbuf) {
                LOGGER(ibis::gVerbose > 1)
                    << "Warning -- text::findString is to skip " << jbuf
                    << " bytes of string " << irow << " from " << data
                    << " because it is longer than internal buffer (size "
                    << jbuf << ")";
                curr += jbuf;
            }
            while (begin+jbuf >= next) {
                bool match = (curr+slen+1 == next); // the same length
#if FASTBIT_CASE_SENSITIVE_COMPARE+0 == 0
                // case-insensitive match
                match = strnicmp(buf+begin, str+curr, next-curr-1);
#else
                match = strncmp(buf+begin, str+curr, next-curr-1);
#endif
                // for (long i = curr; i < next-1 && match; ++ i)
                //     match = (buf[i-begin] == str[i-curr] ||
                //           (islower(buf[i-begin]) &&
                //            buf[i-begin] == tolower(str[i-curr])) ||
                //           (isupper(buf[i-begin]) &&
                //            buf[i-begin] == toupper(str[i-curr])));
                if  (match) {
                    found = true;
                    break;
                }
                ++ irow;
                curr = next;
                moresp = (feof(fsp) == 0);
                if (moresp)
                    moresp = (1 == fread(&next, sizeof(next), 1, fsp));
                if (! moresp)
                    break;
            }
            if (moresp) {// move back a word
                fseek(fsp, -static_cast<long>(sizeof(next)), SEEK_CUR);
                fseek(fdata, curr, SEEK_SET);
                begin = curr;
            }
            else
                break; // avoid reading the data file
        } // while (jbuf > 0) -- as long as there are bytes to examine
    }

    fclose(fsp);
    fclose(fdata);
    ibis::fileManager::instance().recordPages(0, next);
    ibis::fileManager::instance().recordPages
        (0, sizeof(uint64_t)*thePart->nRows());

    if (found)
        return str;
    else
        return 0;
} // ibis::text::findString

int ibis::text::getOpaque(uint32_t irow, ibis::opaque &val) const {
    std::string str;
    int ierr = getString(irow, str);
    if (ierr < 0) return ierr;
    val.copy(str.data(), str.size());
    return ierr;
} // ibis::text::getOpaque

/// Locate the ID column for processing term-document list provided by the
/// user.  This function checks indexSpec first for docIDName=xx for the
/// name of the ID column, then checks the global parameter
/// <table-name>.<column-name>.docIDName.
const ibis::column* ibis::text::IDColumnForKeywordIndex() const {
    const ibis::column* idcol = 0;
    const char* spec = indexSpec();
    if (spec && *spec != 0) {
        const char* str = strstr(spec, "docidname");
        if (str == 0) {
            str = strstr(spec, "docIDName");
            if (str == 0) {
                str = strstr(spec, "docIdName");
                if (str == 0)
                    str = strstr(spec, "DOCIDNAME");
            }
        }
        if (str != 0 && *str != 0) {
            str += 9;
            str += strspn(str, " \t=");
            char *tmp = ibis::util::getString(str);
            if (tmp != 0 && *tmp != 0)
                idcol = partition()->getColumn(tmp);
            delete [] tmp;
        }
        if (idcol == 0) {
            str = strstr(spec, "docid");
            if (str == 0) {
                str = strstr(spec, "docID");
                if (str == 0) {
                    str = strstr(spec, "docId");
                    if (str == 0)
                        str = strstr(spec, "DOCID");
                }
            }
            if (str != 0 && *str != 0) {
                str += 5;
                str += strspn(str, " \t=");
                char *tmp = ibis::util::getString(str);
                if (tmp != 0 && *tmp != 0)
                    idcol = partition()->getColumn(tmp);
                delete [] tmp;
            }
        }
    }
    if (idcol == 0) {
        std::string idcpar = partition()->name();
        idcpar += '.';
        idcpar += m_name;
        idcpar += ".docIDName";
        const char* idname = ibis::gParameters()[idcpar.c_str()];
        if (idname != 0)
            idcol = partition()->getColumn(idname);
    }
    return idcol;
} // ibis::text::IDColumnForKeywordIndex

void ibis::text::TDListForKeywordIndex(std::string& fname) const {
    fname.erase(); // erase existing content in fname
    if (thePart != 0 && thePart->currentDataDir() != 0)
        startPositions(thePart->currentDataDir(), 0, 0);

    const char* spec = indexSpec();
    if (spec != 0 && *spec != 0) {
        const char* str = strstr(spec, "tdlist");
        if (str == 0) {
            str = strstr(spec, "TDList");
            if (str == 0) {
                str = strstr(spec, "tdList");
                if (str == 0)
                    str = strstr(spec, "TDLIST");
            }
        }
        if (str != 0 && *str != 0) {
            str += 5;
            str += strspn(str, " \t=");
            (void) ibis::util::readString(fname, str);
        }
    }
    if (fname.empty()) {
        std::string idcpar = partition()->name();
        idcpar += '.';
        idcpar += m_name;
        idcpar += ".TDList";
        const char* idname = ibis::gParameters()[idcpar.c_str()];
        if (idname != 0)
            fname = idname;
    }
} // ibis::text::TDListForKeywordIndex

void ibis::text::delimitersForKeywordIndex(std::string& fname) const {
    fname.erase(); // erase existing content in fname
    const char* spec = indexSpec();
    if (spec != 0 && *spec != 0) {
        const char* str = strstr(spec, "delimiters");
        if (str == 0) {
            str = strstr(spec, "Delimiters");
            if (str == 0) {
                str = strstr(spec, "DELIMITERS");
            }
        }
        if (str != 0 && *str != 0) {
            str += 10;
            str += strspn(str, " \t=");
            (void) ibis::util::readString(fname, str);
        }
        else {
            str = strstr(spec, "delim");
            if (str == 0) {
                str = strstr(spec, "Delim");
                if (str == 0) {
                    str = strstr(spec, "DELIM");
                }
            }
            if (str != 0 && *str != 0) {
                str += 5;
                str += strspn(str, " \t=");
                (void) ibis::util::readString(fname, str);
            }
        }
    }
    if (fname.empty()) {
        std::string idcpar = partition()->name();
        idcpar += '.';
        idcpar += m_name;
        idcpar += ".delimiters";
        const char* idname = ibis::gParameters()[idcpar.c_str()];
        if (idname != 0)
            fname = idname;
    }
} // ibis::text::delimitersForKeywordIndex

long ibis::text::keywordSearch(const char* str, ibis::bitvector& hits) const {
    long ierr = -1;
    if (str == 0 || *str == 0) return ierr;

    try {
        std::string evt;
        if (ibis::gVerbose > 1) {
            evt = "text[";
            evt += thePart->name();
            evt += '.';
            evt += m_name;
            evt += "]::keywordSearch(";
            evt += str;
            evt += ')';
        }
        else {
            evt = "text::keywordSearch";
        }
        ibis::util::timer mytimer(evt.c_str(), 4);
        indexLock lock(this, evt.c_str());
        if (idx != 0 && idx->type() == ibis::index::KEYWORDS) {
            ierr = reinterpret_cast<ibis::keywords*>(idx)->search(str, hits);
        }
        else {
            ierr = -2;
        }
    }
    catch (...) {
        ierr = -1;
    }
    return ierr;
} // ibis::text::keywordSearch

long ibis::text::keywordSearch(const char* str) const {
    long ierr = 0;
    try {
        //startPositions(0, 0, 0);
        indexLock lock(this, "keywordSearch");
        if (idx != 0 && idx->type() == ibis::index::KEYWORDS) {
            ierr = reinterpret_cast<ibis::keywords*>(idx)->search(str);
        }
        else {
            ierr = -2;
        }
    }
    catch (...) {
        ierr = -1;
    }
    return ierr;
} // ibis::text::keywordSearch

long ibis::text::keywordSearch(const std::vector<std::string> &strs,
                               ibis::bitvector& hits) const {
    long ierr = 0;
    try {
        if (strs.empty()) {
            getNullMask(hits);
            return ierr;
        }

        indexLock lock(this, "keywordSearch");
        if (idx != 0 && idx->type() == ibis::index::KEYWORDS) {
            ierr = reinterpret_cast<ibis::keywords*>(idx)->search(strs, hits);
        }
        else {
            ierr = -2;
        }
    }
    catch (...) {
        ierr = -1;
    }
    return ierr;
} // ibis::text::keywordSearch

long ibis::text::keywordSearch(const std::vector<std::string> &strs) const {
    long ierr = 0;
    try {
        if (strs.empty())
            return (thePart != 0 ? thePart->nRows() : INT_MAX);

        indexLock lock(this, "keywordSearch");
        if (idx != 0 && idx->type() == ibis::index::KEYWORDS) {
            ierr = reinterpret_cast<ibis::keywords*>(idx)->search(strs);
        }
        else {
            ierr = -2;
        }
    }
    catch (...) {
        ierr = -1;
    }
    return ierr;
} // ibis::text::keywordSearch

double ibis::text::estimateCost(const ibis::qString& cmp) const {
    double ret = partition()->nRows() *
        static_cast<double>(sizeof(uint64_t));
    return ret;
} // ibis::text::estimateCost

double ibis::text::estimateCost(const ibis::qAnyString& cmp) const {
    double ret = partition()->nRows() *
        static_cast<double>(sizeof(uint64_t));
    return ret;
} // ibis::text::estimateCost

/// Write the selected values to the specified directory.  If the
/// destination directory is the current data directory, the file
/// containing existing string values will be renamed to be
/// column-name.old, otherwise, the file in the destination directory is
/// simply overwritten.  In case of error, a negative number is returned,
/// otherwise, the number of rows saved to the new file is returned.
long ibis::text::saveSelected(const ibis::bitvector& sel, const char *dest,
                              char *buf, uint32_t nbuf) {
    if (thePart == 0 || thePart->currentDataDir() == 0)
        return -1;

    startPositions(thePart->currentDataDir(), 0, 0);
    long ierr = 0;
    ibis::bitvector msk;
    getNullMask(msk);
    if (dest == 0 || dest == thePart->currentDataDir() ||
        std::strcmp(dest, thePart->currentDataDir()) == 0) {
        // use the active directory, need a write lock
        std::string fname = thePart->currentDataDir();
        fname += FASTBIT_DIRSEP;
        fname += m_name;
        std::string gname = fname;
        gname += ".old";
        std::string sname = fname;
        sname += ".sp";
        std::string tname = sname;
        tname += ".old";

        writeLock lock(this, "saveSelected");
        if (idx != 0) {
            const uint32_t idxc = idxcnt();
            if (0 == idxc) {
                delete idx;
                idx = 0;
                purgeIndexFile(thePart->currentDataDir());
            }
            else {
                LOGGER(ibis::gVerbose > 1)
                    << "Warning -- text::saveSelected cannot modify "
                    "index files";
                return -2;
            }
        }
        ibis::fileManager::instance().flushFile(fname.c_str());

        ierr = rename(fname.c_str(), gname.c_str());
        if (ierr != 0) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- text::saveSelected failed to rename "
                << fname << " to " << gname << " -- " << strerror(errno);
            return -3;
        }
        ierr = rename(sname.c_str(), tname.c_str());
        if (ierr != 0) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- text::saveSelected failed to rename "
                << sname << " to " << tname << " -- " << strerror(errno);
            return -4;
        }
        ierr = writeStrings(fname.c_str(), gname.c_str(),
                            sname.c_str(), tname.c_str(),
                            msk, sel, buf, nbuf);
    }
    else {
        // using two separate sets of files, need a read lock only
        std::string fname = dest;
        fname += FASTBIT_DIRSEP;
        fname += m_name;
        std::string gname = thePart->currentDataDir();
        gname += FASTBIT_DIRSEP;
        gname += m_name;
        std::string sname = fname;
        sname += ".sp";
        std::string tname = gname;
        tname += ".sp";

        purgeIndexFile(dest);
        readLock lock(this, "saveSelected");
        ierr = writeStrings(fname.c_str(), gname.c_str(),
                            sname.c_str(), tname.c_str(),
                            msk, sel, buf, nbuf);
    }
    return ierr;
} // ibis::text::saveSelected

/// Write the selected strings.  The caller manages the necessary locks for
/// accessing this function.
int ibis::text::writeStrings(const char *to, const char *from,
                             const char *spto, const char *spfrom,
                             ibis::bitvector &msk, const ibis::bitvector &sel,
                             char *buf, uint32_t nbuf) const {
    std::string evt = "text[";
    evt += thePart->name();
    evt += '.';
    evt += m_name;
    evt += "]::writeStrings";
    ibis::fileManager::buffer<char> mybuf(buf != 0);
    if (buf == 0) { // incoming buf is nil, use mybuf
        nbuf = mybuf.size();
        buf = mybuf.address();
    }
    if (buf == 0 || to == 0 || from == 0 || spfrom == 0 || spto == 0) {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- " << evt
            << " failed to allocate work space to read strings";
        return -10;
    }

    int rffile = UnixOpen(from, OPEN_READONLY);
    if (rffile < 0) {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- " << evt << " failed to open file " << from
            << " for reading";
        return -11;
    }
    IBIS_BLOCK_GUARD(UnixClose, rffile);
#if defined(_WIN32) && defined(_MSC_VER)
    (void)_setmode(rffile, _O_BINARY);
#endif

    int sffile = UnixOpen(spfrom, OPEN_READONLY);
    if (sffile < 0) {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- " << evt << " failed to open file " << spfrom
            << " for reading";
        return -12;
    }
    IBIS_BLOCK_GUARD(UnixClose, sffile);
#if defined(_WIN32) && defined(_MSC_VER)
    (void)_setmode(sffile, _O_BINARY);
#endif

    int rtfile = UnixOpen(to, OPEN_APPENDONLY, OPEN_FILEMODE);
    if (rtfile < 0) {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- " << evt << " failed to open file " << to
            << " for writing";
        return -13;
    }
    IBIS_BLOCK_GUARD(UnixClose, rtfile);
#if defined(_WIN32) && defined(_MSC_VER)
    (void)_setmode(rtfile, _O_BINARY);
#endif

    int stfile = UnixOpen(spto, OPEN_APPENDONLY, OPEN_FILEMODE);
    if (rtfile < 0) {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- " << evt << " failed to open file " << spto
            << " for writing";
        return -14;
    }
    IBIS_BLOCK_GUARD(UnixClose, stfile);
#if defined(_WIN32) && defined(_MSC_VER)
    (void)_setmode(stfile, _O_BINARY);
#endif

    int64_t ierr, pos;
    for (ibis::bitvector::indexSet ix = sel.firstIndexSet();
         ix.nIndices() > 0; ++ ix) {
        const ibis::bitvector::word_t *idx = ix.indices();
        if (ix.isRange()) { // write many values together
            ibis::bitvector::word_t irow = *idx;
            pos = *idx * 8;
            ierr = UnixSeek(sffile, pos, SEEK_SET);
            if (pos == ierr) {
                // copy one string at a time
                int64_t rfbegin, rfend;
                ierr = UnixRead(sffile, &rfbegin, 8);
                if (ierr == 8) {
                    ierr = UnixSeek(rffile, rfbegin, SEEK_SET);
                    if (ierr == rfbegin)
                        ierr = 8;
                    else
                        ierr = 0;
                }
                while (irow < idx[1] && ierr == 8) {
                    ierr = UnixRead(sffile, &rfend, 8);
                    if (ierr != 8) break;

                    pos = UnixSeek(rtfile, 0, SEEK_CUR);
                    ierr = UnixWrite(stfile, &pos, 8);
                    if (ierr != 8) { // unrecoverable trouble
                        LOGGER(ibis::gVerbose >= 0)
                            << "Warning -- " << evt
                            << " failed to write the value " << pos
                            << " to " << spto << ", "
                            << (errno ? strerror(errno) : "??");
                        return -15;
                    }

                    pos = rfend - rfbegin;
                    for (int jtmp = 0; jtmp < pos; jtmp += nbuf) {
                        int bytes = (jtmp+nbuf < pos ? nbuf : pos-jtmp);
                        ierr = UnixRead(rffile, buf, bytes);
                        if (ierr == bytes) {
                            ierr = UnixWrite(rtfile, buf, bytes);
                            if (ierr != bytes) {
                                LOGGER(ibis::gVerbose >= 0)
                                    << "Warning -- " << evt
                                    << " failed to write " << bytes
                                    << " byte" << (bytes>1?"s":"") << " to "
                                    << to << ", "
                                    << (errno ? strerror(errno) : "??");
                                return -16;
                            }
                        }
                        else {
                            LOGGER(ibis::gVerbose >= 0)
                                << "Warning -- " << evt << " failed to read "
                                << bytes << " byte" << (bytes>1?"s":"")
                                << " from " << from << ", "
                                << (errno ? strerror(errno) : "??");
                            return -17;
                        }
                    }

                    rfbegin = rfend;
                    ierr = 8;
                    ++ irow;
                } // while (irow
            }
            else {
                LOGGER(ibis::gVerbose >= 0)
                    << "Warning -- " << evt << " failed to seek to " << pos
                    << " in file " << spfrom << ", seek function returned "
                    << ierr;
            }

            if (irow < idx[1]) {
                (void) memset(buf, 0, nbuf);
                pos = UnixSeek(rtfile, 0, SEEK_CUR);
                for (int jtmp = irow; jtmp < static_cast<long>(idx[1]);
                     ++ jtmp) {
                    ierr = UnixWrite(stfile, &pos, 8);
                    if (ierr != 8) {
                        LOGGER(ibis::gVerbose >= 0)
                            << "Warning -- " << evt
                            << " failed to write the value " << pos
                            << " to " << spto << ", failed to continue";
                        return -18;
                    }
                }
                while (irow < idx[1]) {
                    int bytes = (idx[1]-irow > nbuf ? nbuf : idx[1]-irow);
                    ierr = UnixWrite(rffile, buf, bytes);
                    if (ierr != bytes) {
                        LOGGER(ibis::gVerbose >= 0)
                            << "Warning -- " << evt << " failed to write "
                            << bytes << " byte" << (bytes>1?"s":"") << " to "
                            << to << ", can not continue";
                        return -19;
                    }
                    irow += bytes;
                }
            }
        }
        else {
            for (unsigned jdx = 0; jdx < ix.nIndices(); ++ jdx) {
                pos = UnixSeek(rtfile, 0, SEEK_CUR);
                ierr = UnixWrite(stfile, &pos, 8);
                if (ierr != 8) {
                    LOGGER(ibis::gVerbose >= 0)
                        << "Warning -- " << evt
                        << " failed to write the value " << pos
                        << " to " << spto << ", can not continue";
                    return -20;
                }

                pos = idx[jdx] * 8;
                ierr = UnixSeek(sffile, pos, SEEK_SET);
                if (ierr == pos) {
                    int64_t rfbegin;
                    ierr = UnixRead(sffile, &rfbegin, 8);
                    if (ierr == 8) {
                        ierr = UnixSeek(rffile, rfbegin, SEEK_SET);
                        bool more = (ierr == rfbegin);
                        if (! more)
                            ierr = 0;
                        while (more) {
                            ierr = UnixRead(rffile, buf, nbuf);
                            for (pos = 0; pos < ierr && buf[pos] != 0; ++ pos);
                            if (pos < ierr) {
                                more = false;
                                ++ pos;
                            }
                            if (pos > 0) {
                                ierr = UnixWrite(rtfile, buf, pos);
                                if (ierr == pos) {
                                    ierr = 8;
                                }
                                else {
                                    LOGGER(ibis::gVerbose >= 0)
                                        << "Warning -- " << evt
                                        << " failed to write " << pos
                                        << " byte" << (pos>1?"s":"")
                                        << " to " << to
                                        << ", can not continue";
                                    return -21;
                                }
                            }
                        } // while (more)
                    }
                }
                else {
                    ierr = 0;
                }

                if (ierr != 8) { // write a null string
                    buf[0] = 0;
                    ierr = UnixWrite(rtfile, buf, 1);
                    if (ierr != 1) {
                        LOGGER(ibis::gVerbose >= 0)
                            << "Warning -- " << evt
                            << " failed to write 1 byte to " << to
                            << ", can not continue";
                        return -22;
                    }
                }
            } // for (unsigned jdx
        }
    } // for (ibis::bitvector::indexSet ix

    pos = UnixSeek(rtfile, 0, SEEK_CUR);
    ierr = UnixWrite(stfile, &pos, 8);
    LOGGER(ierr != 8 && ibis::gVerbose >= 0)
        << "Warning -- " << evt << " failed to write the last position "
        << pos << " to " << spto;

    ibis::bitvector bv;
    msk.subset(sel, bv);
    bv.adjustSize(0, sel.cnt());
    bv.swap(msk);

    const int nr = sel.cnt();
    LOGGER(ibis::gVerbose > 1)
        << evt << " copied " << nr << " string" << (nr > 1 ? "s" : "")
        << " from \"" << from << "\" to \"" << to << '"';
    return nr;
} // ibis::text::writeStrings
