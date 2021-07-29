/**
   SMatch: A tester for the string matching function.  Originally by Bernd
   Jaenichen <bernd dot jaenichen at globalpark dot com>, 2010/4/28.

   usage:
   smatch <datadir> [where_1 [where_2 ...]]

   The datadir argument must be provided.  If the user provides addition
   arguments, they will be treated as where clauses to be evaluated one at
   a time.  This program was originally provided by Bernd to reveal a bug
   in the string search function ibis::text::stringSearch.

   2010/04/29: renamed to smatch.cpp, add documentation.  John Wu.

   @note
   Smatch, v.i. To smack. [Obs.] --Banister (1578).
   -- Webster's Revised Unabridged Dictionary, 1998
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
/// ibis::table::row.
void tester::builtindata(const char *datadir) {
    ibis::table::row irow;
    std::auto_ptr<ibis::tablex> ta(ibis::tablex::create());

    ta->addColumn("s", ibis::TEXT);

    irow.clear();
    irow.textsnames.push_back("s");
    irow.textsvalues.push_back("tr10000");
    ta->appendRow(irow);

    irow.clear();
    irow.textsnames.push_back("s");
    irow.textsvalues.push_back("tr10000");
    ta->appendRow(irow);

    irow.clear();
    irow.textsnames.push_back("s");
    irow.textsvalues.push_back("tr10001");
    ta->appendRow(irow);

    irow.clear();
    irow.textsnames.push_back("s");
    irow.textsvalues.push_back("tr10002");
    ta->appendRow(irow);

    irow.clear();
    irow.textsnames.push_back("s");
    irow.textsvalues.push_back("tr100");
    ta->appendRow(irow);

    irow.clear();
    irow.textsnames.push_back("s");
    irow.textsvalues.push_back("tr100");
    ta->appendRow(irow);

    irow.clear();
    irow.textsnames.push_back("s");
    irow.textsvalues.push_back("tr101");
    ta->appendRow(irow);

    irow.clear();
    irow.textsnames.push_back("s");
    irow.textsvalues.push_back("tr102");
    ta->appendRow(irow);

    irow.clear();
    irow.textsnames.push_back("s");
    irow.textsvalues.push_back("tr1000");
    ta->appendRow(irow);

    irow.clear();
    irow.textsnames.push_back("s");
    irow.textsvalues.push_back("tr1000");
    ta->appendRow(irow);

    irow.clear();
    irow.textsnames.push_back("s");
    irow.textsvalues.push_back("tr1001");
    ta->appendRow(irow);

    irow.clear();
    irow.textsnames.push_back("s");
    irow.textsvalues.push_back("tr1002");
    ta->appendRow(irow);

    irow.clear();
    irow.textsnames.push_back("s");
    irow.textsvalues.push_back("tr111110000");
    ta->appendRow(irow);

    irow.clear();
    irow.textsnames.push_back("s");
    irow.textsvalues.push_back("tr111110000");
    ta->appendRow(irow);

    irow.clear();
    irow.textsnames.push_back("s");
    irow.textsvalues.push_back("tr111110001");
    ta->appendRow(irow);

    irow.clear();
    irow.textsnames.push_back("s");
    irow.textsvalues.push_back("tr111110002");
    ta->appendRow(irow);

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
	    << "Warning -- expected to select " << table->nRows() << " row"
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
    std::auto_ptr<ibis::table> select(table->select(cnames.front(), where));
    if (select.get() == 0) {
	LOGGER(ibis::gVerbose >= 0)
	    << "failed to select \"" << where << "\" on table "
	    << table->name();
	return;
    }

    std::cout << "Number of rows satisfying \"" << where << "\": "
	      << select->nRows() << std::endl;
    std::auto_ptr<ibis::table::cursor> cur(select->createCursor());
    if (cur.get() == 0) {
	LOGGER(ibis::gVerbose >= 0)
	    << "failed to create a cursor from the result table named "
	    << select->name();
	return;
    }
    unsigned irow = 0;
    while(0 == cur->fetch()) {
	std::string ivalue;
	cur->getColumnAsString(cnames.front(), ivalue);
	std::cout << cnames.front() << "[" << irow << "] = " << ivalue
		  << std::endl;
	++ irow;
    }
} // tester::query

int main(int argc, char** argv) {
    if (argc < 2) {
	printf("\nUsage:\n\t%s <datadir> [where_1 [where_2...]]\n\n", *argv);
	return 0;
    }
    //ibis::gVerbose = 5;
    char *datadir = argv[1];
    tester test0r;
    test0r.load(datadir);
    if (argc == 2) { // try some built-in tests
	test0r.query(datadir, "s='tr100'");
	test0r.query(datadir, "s='tr1000'");
	test0r.query(datadir, "s='tr10000'");
	test0r.query(datadir, "s='tr111110000'");
    }
    for (int iarg = 2; iarg < argc; ++ iarg)
	test0r.query(datadir, argv[iarg]);

    return 0;
} // main
