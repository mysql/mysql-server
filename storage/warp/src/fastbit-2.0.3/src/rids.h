// $Id$
// Copyright (c) 2003-2016 the Regents of the University of California
// Author: John Wu <John.Wu at acm.org>
//      Lawrence Berkeley National Laboratory
#ifndef IBIS_RIDS_H
#define IBIS_RIDS_H
#include "utilidor.h"	// ibis::RIDSet
///@file
/// Define simple IO functions for ibis::rid_t.
/// Based on on OidIOHandler by David Malon <malon@anl.gov>.

namespace ibis {
    class ridHandler;
}

/// A class for handling file IO for ibis::rid_t.
class ibis::ridHandler {
 public:
    ridHandler(const char* dbName, const char* pref="ibis");
    ~ridHandler();

    // Read a set of rids from a file.
    int read(RIDSet& rids, const char* source);
    // Write a set of rids to a file.
    int write(const RIDSet& rids, const char* destination,
	      const char* dbName=0);
    // Append a set of rids to an existing rid file.
    int append(const RIDSet& rids, const char* destination) const;

 protected:
    // member variables
    char* _dbName;  // name of the rid set
    char* _prefix;  // prefix in the names of the parameters
    mutable pthread_mutex_t mutex; // a mutex lock

    // class variables used as internal parameters
    static const char *const version; // Internal version number.

    int readDBName(std::istream& _from);
    int matchDBName(std::istream& _from) const;
    int readVersion(std::istream& _from) const;
    int readRidCount(std::istream& _from, int& ic) const;

private:
    ridHandler(const ridHandler&);
    ridHandler& operator=(const ridHandler&);
};
#endif
