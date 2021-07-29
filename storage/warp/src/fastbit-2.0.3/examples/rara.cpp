// $Id$
// Author: John Wu <John.Wu at ACM.org> Lawrence Berkeley National Laboratory
// Copyright (c) 2008-2016 the Regents of the University of California
/** @file rara.cpp

    This is meant to be the simplest test program for the querying
    functions of ibis::query and ibis::part.  It accepts the following
    fixed list of arguments to simplify the program:

    data-dir query-conditions [column-to-print [column-to-print ...]]

    The data-dir must exist and contain valid data files and query
    conditions much be specified as well.  If no column-to-print, this
    program effective is answering the following SQL query

    SELECT count(*) FROM data-dir WHERE query-conditions

    If any column-to-print is specified, all of them are concatenated
    together and the SQL query answered is of the form

    SELECT column-to-print, column-to-print, ... FROM data-dir WHERE query-conditions

    About the name: Bostrychia rara, Spot-breasted Ibis, the smallest ibis
    <http://www.sandiegozoo.org/animalbytes/t-ibis.html>.  As an example
    program for using FastBit IBIS, this might be also the smallest.

    @ingroup FastBitExamples
*/
#include "ibis.h"       // FastBit IBIS primary include file

// printout the usage string
static void usage(const char* name) {
    std::cout << "usage:\n" << name << " data-dir query-conditions"
              << " [column-to-print [column-to-print ...]]\n"
              << std::endl;
} // usage

int main(int argc, char** argv) {
    if (argc < 3) {
        usage(*argv);
        return -1;
    }

    // construct a data partition from the given data directory.
    ibis::part apart(argv[1], static_cast<const char*>(0));
    // create a query object with the current user name.
    ibis::query aquery(ibis::util::userName(), &apart);
    // assign the query conditions as the where clause.
    int ierr = aquery.setWhereClause(argv[2]);
    if (ierr < 0) {
        std::clog << *argv << " setWhereClause(" << argv[2]
                  << ") failed with error code " << ierr << std::endl;
        return -2;
    }
    // collect all column-to-print together
    std::string sel;
    for (int j = 3; j < argc; ++ j) {
        if (j > 3)
            sel += ", ";
        sel += argv[j];
    }
    if (sel.empty()) { // select count(*)...
        ierr = aquery.evaluate(); // evaluate the query
        std::cout << "SELECT count(*) FROM " << argv[1] << " WHERE "
                  << argv[2] << "\n--> ";
        if (ierr >= 0) {
            std::cout << aquery.getNumHits();
        }
        else {
            std::cout << "error " << ierr;
        }
    }
    else { // select column-to-print...
        ierr = aquery.setSelectClause(sel.c_str());
        if (ierr < 0) {
            std::clog << *argv << " setSelectClause(" << sel
                      << ") failed with error code " << ierr << std::endl;
            return -3;
        }
        ierr = aquery.evaluate(); // evaluate the query
        std::cout << "SELECT " << sel << " FROM " << argv[1] << " WHERE "
                  << argv[2] << "\n--> ";
        if (ierr >= 0) { // print out the select values
            aquery.printSelected(std::cout);
        }
        else {
            std::cout << "error " << ierr;
        }
    }
    std::cout << std::endl;
    return ierr;
} // main
