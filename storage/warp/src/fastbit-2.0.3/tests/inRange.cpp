/**
   inRange.cpp: A tester for discrete range queries on in-memory data.
   Modified from smatch.cpp, which was originally by Bernd Jaenichen <bernd
   dot jaenichen at globalpark dot com>, 2010/4/28.

   usage:
   inRange <datadir> [where_1 [where_2 ...]]

   The datadir argument must be provided.  If the user provides addition
   arguments, they will be treated as where clauses to be evaluated one at
   a time.  This program was originally provided by Bernd to reveal a bug
   in the string search function ibis::text::stringSearch.
 */
#include "ibis.h"
#include <memory>	// std::auto_ptr

/// Encapsulate the testing functions.
class tester {
public:
    tester();
    ~tester();
    void load(const char *datadir);
    void query(const char *datadir, const char *where);

private:
    void builtindata(const char* datadir);
};

tester::tester() {
#if defined(_DEBUG)
    ibis::util::setVerboseLevel(5);
#elif defined(DEBUG)
    ibis::util::setVerboseLevel(7);
#endif
}

tester::~tester() {
}

/// Generate some records.  Use ibis::tablex interface and
/// ibis::table::row.  The integer records go from 1 to 16, with the value
/// J repeated J times.
void tester::builtindata(const char *datadir) {
    ibis::table::row irow;
    std::auto_ptr<ibis::tablex> ta(ibis::tablex::create());

    ta->addColumn("l", ibis::LONG);

    irow.longsnames.push_back("l");
    irow.longsvalues.resize(1);
    for (int j = 1; j < 16; ++ j) {
	irow.longsvalues.back() = j;
	for (int i = 0; i < j; ++ i)
	    ta->appendRow(irow);
    }

    ta->write(datadir);
    LOGGER(ibis::gVerbose > 0)
	<< "generated " << ta->mRows() << " rows in directory " << datadir;
}

/// Load the data in the specified directory.  If the directory contains
/// nothing it will generate some records using the function builtindata.
/// If some data records are found, it will run a minimal test that counts
/// the number of records.
void tester::load(const char *datadir) {
    if (datadir == 0 || *datadir == 0) return;

    try { // use existing data records
	std::auto_ptr<ibis::table> table(ibis::table::create(datadir));
	if (table.get() == 0) {
	    LOGGER(ibis::gVerbose >= 0)
		<< "failed to load table from " << datadir;
	    return;
	}
	if (table->nRows() == 0 || table->nColumns() == 0) {
	    // empty or non-existent directory, create new data
	    builtindata(datadir);
	    table->addPartition(datadir);
	}

	std::auto_ptr<ibis::table> select(table->select("", "1=1"));
	if (select.get() == 0) {
	    LOGGER(ibis::gVerbose >= 0)
		<< "failed to select all rows from table " << table->name();
	    return;
	}

	LOGGER(select->nRows() != table->nRows() && ibis::gVerbose >= 0)
	    << "expected to select " << table->nRows() << " row"
	    << (table->nRows()>1?"s":"") << ", but got " << select->nRows();
    }
    catch (...) { // generate hard-coded data records
	builtindata(datadir);
    }
} // tester::load

/// Evaluate the query condition where on the data records in directory
/// datadir.  Make use of the ibis::table interface.  It retrieves the
/// values of the first column as strings after evaluating the query
/// conditions.
void tester::query(const char *datadir, const char *where) {
    if (datadir == 0 || where == 0 || *datadir == 0 || *where == 0) return;

    std::auto_ptr<ibis::table> table(ibis::table::create(datadir));
    if (table.get() == 0) {
	LOGGER(ibis::gVerbose >= 0)
	    << "failed to load table from " << datadir;
	return;
    }
    if (table->name() == 0 || *(table->name()) == 0) {
	LOGGER(ibis::gVerbose >= 0)
	    << "failed to find any data records in directory " << datadir;
	return;
    }
    if (table->nColumns() == 0) {
	LOGGER(ibis::gVerbose >= 0)
	    << "Table " << table->name() << " in " << datadir << " is empty";
	return;
    }

    ibis::table::stringArray cnames = table->columnNames();
    if (cnames.empty()) {
	LOGGER(ibis::gVerbose >= 0)
	    << "failed to retrieve column names from table " << table->name()
	    << " in " << datadir;
	return;
    }

    std::string selall = cnames.front();
    for (unsigned j = 1; j < cnames.size(); ++ j) {
	selall += ", ";
	selall += cnames[j];
    }
    // create in-memory table to process where
    std::auto_ptr<ibis::table> inmemory(table->select(selall.c_str(), "1=1"));
    if (inmemory.get() == 0) {
	LOGGER(ibis::gVerbose >= 0)
	    << "failed to select all rows from table " << table->name();
	return;
    }
    if (ibis::gVerbose > 0) { // an extra test with group-by
	std::auto_ptr<ibis::table> groupby(inmemory->groupby(cnames.front()));
	if (groupby.get() == 0) {
	    LOGGER(ibis::gVerbose > 0)
		<< "failed to evaluate groupby(" << cnames.front()
		<< " on table " << inmemory->name();
	}
	else {
	    std::cout << "\nThe result of groupby(" << cnames.front() << ")\n";
	    groupby->dump(std::cout);
	    std::cout << std::endl;
	}
    }

    std::string sel1 = cnames.front();
    sel1 += ", count(*)";
    std::auto_ptr<ibis::table> select(inmemory->select(sel1.c_str(), where));
    if (select.get() == 0) {
	LOGGER(ibis::gVerbose >= 0)
	    << "failed to select \"" << where << "\" on table"
	    << inmemory->name();
	return;
    }

    std::cout << "Number of rows produced by \"SELECT " << sel1
	      << " WHERE " << where << "\": "
	      << select->nRows() << std::endl;
    select->dump(std::cout);
} // tester::query

int main(int argc, char** argv) {
    if (argc < 2) {
	printf("\nUsage:\n%s <datadir> [where_clause1] [where_clause2...]\n\n",
	       *argv);
	return 0;
    }

    char *datadir = argv[1];
    tester test0r;
    test0r.load(datadir);
    //ibis::gVerbose = 6;
    if (argc == 2) { // try some built-in tests
	test0r.query(datadir, "l IN (1,2)");
	test0r.query(datadir, "l IN (1)");
	test0r.query(datadir, "l IN (3)");
	test0r.query(datadir, "l IN (1,3)");
    }
    for (int iarg = 2; iarg < argc; ++ iarg)
	test0r.query(datadir, argv[iarg]);

    return 0;
} // main
