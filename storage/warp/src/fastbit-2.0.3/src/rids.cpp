// $Id$
// Copyright (c) 2003-2016 the Regents of the University of California
// Author: John Wu <John.Wu at acm.org>
//      Lawrence Berkeley National Laboratory
//
/// Implements the functions defined in ibis::ridHandler.

#include "rids.h"
#include "array_t.h"    // array_t<ibis::rid_t> i.e., ibis::RIDSet
#include <fstream>
const char *const ibis::ridHandler::version = "0.3";

ibis::ridHandler::ridHandler(const char *dbName, const char *pref) {
    if (dbName != 0 && *dbName != 0) {
        _dbName = ibis::util::strnewdup(dbName);
    }
    else {
        _dbName = ibis::util::strnewdup("sample");
    }
    if (pref != 0 && *pref != 0) {
        _prefix = ibis::util::strnewdup(pref);
    }
    else {
        _prefix = ibis::util::strnewdup("ibis");
    }
    if (0 != pthread_mutex_init(&mutex, 0))
        throw "ridHandler::ctor failed to initialize a mutex lock"
            IBIS_FILE_LINE;
}

ibis::ridHandler::~ridHandler() {
    pthread_mutex_destroy(&mutex);
    delete [] _prefix;
    delete [] _dbName;
}

/// Write the rid set.  Return the number of rids written.  If the first
/// argument is specified, the internally stored dbName would be modified.
int ibis::ridHandler::write(const ibis::RIDSet& rids,
                            const char* fname, const char* dbName) {
    if (fname == 0 || *fname == 0) return -1;
    ibis::util::mutexLock lock(&mutex, "ridHandler::write");
    std::ofstream to(fname);
    if (!to) {
        LOGGER(ibis::gVerbose >= 0)
            << "ridHandler cannot open output file " << fname;
        return false;
    }

    if (dbName != 0 && *dbName != 0) { // record the new dbName
        if (stricmp(_dbName, dbName) != 0) {
            delete [] _dbName;
            _dbName = ibis::util::strnewdup(dbName);
        }
    }

    to << _prefix << "*RidSet " << version << "\n";
    to << _prefix << "*RidSetName " << _dbName << "\n";

    const unsigned int nr = rids.size();
    to << _prefix << "*RidCount " << nr << "\n";

    for (unsigned int i=0; i < nr; ++i)
        to << rids[i] << "\n";
    to.close();
    return nr;
} // ibis::ridHandler::write

/// Append the rid set to the name file.  Return the number of rids
/// written.  This function can be called after ibis::ridHandler::write has
/// been called to write a file.  It can be called many times.  The
/// function ibis::ridHandler::read will concatenate all rid sets into one.
int ibis::ridHandler::append(const ibis::RIDSet& rids,
                             const char* fname) const {
    if (fname == 0 || *fname == 0) return -1;
    ibis::util::mutexLock lock(&mutex, "ridHandler::append");
    std::fstream to(fname, std::ios::in | std::ios::out);
    if (! to) {
        LOGGER(ibis::gVerbose >= 0)
            << "ridHandler cannot open input/output file " << fname;
        return -2;
    }
    if (0 != readVersion(to)) {
        LOGGER(ibis::gVerbose >= 0)
            << fname << " is not a recognized RidFile";
        return -3;
    }
    if (0 != matchDBName(to)) {
        LOGGER(ibis::gVerbose >= 0)
            << "The name in file " << fname << " must be \""
            << _dbName << "\" in order to append a new rid set";
        return -4;
    }

    // ready to write at the end of file
    to.seekg(std::ios::end);
    const unsigned int nr = rids.size();
    to << _prefix << "*RidCount " << nr << "\n";
    for (unsigned int i = 0; i < nr; ++ i)
        to << rids[i] << "\n";
    to.close();
    return nr;
} // ibis::ridHandler::append

/// This function is capable of reading a file written with one write
/// command and multiple append commands.  All rids are placed in @c rids
/// in the order they appear in the file.  The member varialbe @c _dbName
/// will be set to the name stored in the file.
int ibis::ridHandler::read(ibis::RIDSet& rids, const char* fname) {
    if (fname == 0 || *fname == 0) return -1;
    ibis::util::mutexLock lock(&mutex, "ridHandler::read");
    std::ifstream from(fname);
    if (!from) {
        LOGGER(ibis::gVerbose >= 0)
            << "ridHandler cannot open input file " << fname;
        return -2;
    }

    if (0 != readVersion(from)) {
        LOGGER(ibis::gVerbose >= 0)
            << fname << " is not a recognized RidFile";
        return -3;
    }

    if (0 != readDBName(from)) {
        LOGGER(ibis::gVerbose >= 0)
            << "ridHandler cannot determine the name of the RID set in "
            << fname;
        return -4;
    }

    int nr = 0;
    while (0 == readRidCount(from, nr)) {
        if (nr <= 0)
            break;
        LOGGER(ibis::gVerbose > 1)
            << "ridHandler to read " << nr << (nr>1 ? " rids" : " rid")
            << " from " << fname;
        rids.reserve(rids.size()+nr);
        for (int i = 0; i < nr && !from.fail(); ++i) {
            ibis::rid_t tmp;
            from >> tmp;
            rids.push_back(tmp);
            LOGGER(ibis::gVerbose > 2)
                << rids.size()-1 << ":\t" << rids.back();
        }
    }
    from.close();

    nr = rids.size();
    LOGGER(ibis::gVerbose > 0)
        << "ridHandler read " << nr << (nr > 1 ? " rids" : " rid")
        << " from " << _dbName << " in file " << fname;
    return nr;
} // ibis::ridHandler::read

// If the first line of the file is as expected, it returns zero, otherwise
// it returns a nonzero value.
int ibis::ridHandler::readVersion(std::istream& from) const {
    char text[256];

    // read the first string in the file.  Do not compare the portion
    // before the first '*', expect what follows '*' to be "OIDSET" or
    // "RIDSET".
    from >> text;
    const char* str = strchr(text, '*');
    if (str != 0)
        ++ str;
    else
        str = text;
    if (*str != 'R' && *str != 'O' && 0 != stricmp(str+1, "idset"))
        return -1;

    from >> text; // second string is the version number
    if (text[0] != '0' || text[1] != '.')
        return -2;

    return 0;
} // ibis::ridHandler::readVersion

// Set the @c _dbName according the next line of the file.
int ibis::ridHandler::readDBName(std::istream& from) {
    char text[256];
    const char* str;

    from >> text;
    str = strchr(text, '*');
    if (str != 0)
        ++ str;
    else
        str = text;
    if (*str != 'R' && *str != 'O' && 0 != stricmp(str+1, "idsetname"))
        return -1;

    from >> text;
    if (std::strcmp(text, _dbName)) { 
        delete [] _dbName;
        _dbName = ibis::util::strnewdup(text);
    }

    return 0;
} // ibis::ridHandler::readDBName

// If the dataset name in the file matches the name in this object, return
// 0, otherwise return 1.
int ibis::ridHandler::matchDBName(std::istream& from) const {
    char text[256];
    const char* str;

    from >> text;
    str = strchr(text, '*');
    if (str != 0 && 0 == strnicmp(_prefix, text, str-text)) {
        ++ str;
    }
    else {
        text[str-text] = 0;
        LOGGER(ibis::gVerbose >= 0)
            << "ridHandler::matchDBName prefix expected to be "
            << _prefix << ", but is actually " << text;
        return -1;
    }
    if (*str != 'R' && *str != 'O' && 0 != stricmp(str+1, "idsetname")) {
        LOGGER(ibis::gVerbose >= 0)
            << "ridHandler::matchDBName: unknown identifier " << text;
        return -2;
    }

    from >> text;
    if (0 == stricmp(text, _dbName)) {
        return 0;
    }
    else {
        return 1;
    }
} // ibis::ridHandler::matchDBName

// Read the next line of the file to find out how many rids to expect.
int ibis::ridHandler::readRidCount(std::istream& from, int& nr) const {
    char text[256];
    from >> text;
    nr = 0;

    const char *str = strchr(text, '*');
    if (str != 0)
        ++ str;
    else
        str = text;
    if (*str != 'R' && *str != 'O' && 0 != stricmp(str+1, "idcount"))
        return -1;

    from >> nr;
    return 0;
} // ibis::ridHandler::readRidCount
