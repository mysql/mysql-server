// $Id$
// Author: John Wu <John.Wu at ACM.org>
//         Lawrence Berkeley National Laboratory
// Copyright 2001-2010 the Regents of the University of California
//
/** @file trydll.cpp

   Modified from ibis.cpp to try the DLL library.

   A sample code that exercises the main features of the FastBit bitmap
   indexing search capabilities in this directory.  It provides the basic
   functionality of creating a database, accepts a limited version of SQL
   for query processing.  The queries may be entered either as command line
   arguments or from standard input.

   The queries are specified in a much simplified SQL format.  A query is a
   basically a SQL select statement of the form
   [SELECT ...] [FROM ...] WHERE ... [ORDER BY ... [ASC | DESC]] [LIMIT ...]

   The SELECT clause contains a list of column names and any of the four
   one-argument functions, AVG, MAX, MIN, and SUM, e.g., "SELECT a, b,
   AVG(c), MIN(d)."  If specified, the named columns of qualified records
   will be displayed as the result of the query.  The unqualified variables
   will be used to group the selected records; for each group the values of
   the functions are evaluated.  This is equivalent to use all unqualified
   variables in the "GROUP BY" clause.  Note the print out always orders
   the unqualified variables first followed by the values of the functions.
   It always has an implicit "count(*)" as the end of each line of print
   out.

   The FROM clause contains a list of data partition names.  If specified,
   the search will be performed only on the named partitions.  Otherwise,
   the search is performed on all known tables.

   The column names and partition names can be delimited by either ',', or ';'.
   The leading space and trailing space of each name will be removed but
   space in the middle of the names will be preserved.

   The WHERE clause specifies the condition of the query.  It is specified
   as range queries of the form

   RANGE LOGICAL_OP RANGE
   LOGICAL_OP can be one of "and", "or", "xor", "minus", "&&", "&",
   "||", "|", "^", "-"

   A range is specifed on one column of the form "ColumnA CMP Const"
   CMP can be one of =, ==, !=, >, >=, <, <=
   The ranges and expressions can also be negated with either '!' or '~'.

   The expressions in the ORDER BY clause must be a proper subset of the
   SELECT clause.  The modifiers ASC and DESC are optional.  By default ASC
   (ascending) order is used.  One may use DESC to change to use the
   descending order.

   The LIMIT clause limits the maximum number of output rows.  Only number
   may follow the LIMIT keyword.  This clause has effects only if the
   preceeding WHERE clause selected less than or equal to the specified
   number of rows (after applying the implicit group by clause).


   Command line options:
   ibis [-a[ppend] data_dir [to partition_name]]
	[-c[onf] conf_file] [-d[atadir] data_dir]
        [-q[uery] [SELECT ...] [FROM ...] WHERE ... [ORDER BY ...] [LIMIT ...]]
        [-ou[tput-file] filename] [-l logfilename] [-i[nteractive]]
        [-b[uild-indexes]] [-k[eep-tempory-files]]
 	[-n[o-estimation]] [-e[stimation-only]] [-s[quential-scan]]
        [-v[=n]] [-t[est]] [-h[elp]]

   NOTE: options -one-step-evaluation and -estimation-only are mutually
   exclusive, the one that appears later will overwrite the one that
   appears early on the same command line.

   NOTE: option -t is interpreted as sele-testing if specified alone, if
   any query is also specified, it is interpreted as indicating the number
   of threads to use.
*/
#if defined(_WIN32) && defined(_MSC_VER)
#pragma warning(disable:4786)	// some identifier longer than 256 characters
#endif
#include "ibis.h"
#include <sstream> // std::ostringstream

// local data types
typedef std::vector<const char*> stringArray;

struct thArg {
    const char* uid;
    const stringArray& qlist;
    ibis::partList tlist;
    ibis::util::counter& task;

    thArg(const char* id, const stringArray& ql, ibis::partList& tl,
	  ibis::util::counter& tc) : uid(id), qlist(ql), tlist(tl), task(tc) {}
};

// global varialbes defined in this file
static unsigned testing = 0;
static unsigned threading = 0;
static unsigned build_index = 0;
static bool estimate_only = false;
static bool skip_estimation = false;
static bool sequential_scan = false;
static bool zapping = false;
static const char *appendto = 0;
static const char *outputfile = 0;
static const char *indexingOption = 0;
static const char *junkstring = 0;
static const char *keepstring = 0;
typedef std::pair<const char*, const char*> namepair;

namespace ibis {
#if defined(TEST_SCAN_OPTIONS)
    // a temporary option for testing various options of performing scan for
    // candidate check
    int _scan_option = 0; // default to the old way
#endif
#if defined(TEST_SUMBINS_OPTIONS)
    // a temporary option for controlling the various options of performing
    // the sumBins operation in index.cpp
    int _sumBins_option = 0; // default to the old way
#endif
}

// printout the usage string
static void usage(const char* name) {
    std::cout << "usage:\n" << name << " [-c[onf] conf_file] "
	"[-d[atadir] data_dir] [-i[nteractive]]\n"
	"[-q[uery] [SELECT ...] [FROM ...] WHERE ...]\n"
	"[-ou[tput-file] filename] [-l logfilename] "
	"[-s[quential-scan]]\n"
	"[-n[o-estimation]] [-e[stimation-only]] [-k[eep-temporary-files]]"
	"[-a[ppend] data_dir [partition_name]]\n"
	"[-b[uild-indexes] [numThreads|indexSpec] -z[ap-existing-indexes]]\n"
	"[-v[=n]] [-t[=n]] [-h[elp]] [-j[unk] filename|conditions]\n\n"
	"NOTE: multiple -c -d -q and -v options may be specified.  All "
	"queries are applied to all data partitions by default.  "
	"Verboseness levels are cumulated.\n"
	"NOTE: options -n and -e are mutually exclusive, "
	"the one that appears "
	"later will overwrite the one that appears earlier on "
	"the same command line.\n"
	"NOTE: option -t is interpreted as testing if specified alone, "
	"however if any query is also specified, it is interpreted as "
	"number of threads\n"
	"NOTE: option -j must be followed by either a file name or a list "
	"of conditions.  The named file may contain arbitrary number of "
	"non-negative integers that are treated as row numbers (starting "
	"from 0).  The rows whose numbers are specified in the file will "
	"be marked inactive and will not participate in any further queries.  "
	"If a set of conditions are specified, all rows satisfying the "
	"conditions will be marked inactive.  Additionally, if the -z option "
	"is also specified, all inactive rows will be purged permanently "
	"from the data files.\n"
	"NOTE: option -j is applied to all data partitions known to this "
        "program.  Use with care.\n"
	"NOTE: the output file stores the results selected by queries, the "
	"log file is for the rest of the messages such error messages or "
	"debug information\n"
	      << std::endl;
} // usage

// printout the help message
static void help(const char* name) {
    std::cout << name << " accepts the following commands:\n"
	"help, exit, quit, append\nand query of the form\n\n"
	"[SELECT column_names] [FROM dataset_names] WHERE ranges\n\n"
	"The WHERE clause of a query must be specified.  "
	"It is used to determine\nwhat records qualify the query.\n"
	"If SELECT clause is present in a query, the qualified "
	"records named\ncolumns will be printed, otherwise only "
	"information about number of\nhits will be printed.\n"
	"If FROM clause is present, the WHERE clause will be "
	"only apply on the\nnamed datasets, otherwise, all "
	"available datasets will be used.\n\n"
	"append dir -- add the data in dir to database.\n"
	"print [Parts|Columns|Distributions|column-name [: conditions]]\n"
	"           -- print information about partition names, column names "
	"or an individual column.\n"
	"           -- For an individual column, a set of range conditions "
	"may also be added following a colon (:, denoting such that)\n"
	"exit, quit -- terminate this program.\n"
	"help -- print this message.\n"
	      << std::endl;
} // help

// show column names
static void printNames(const ibis::partList& tlist) {
    ibis::part::info* tinfo;
    ibis::util::logger lg(0);
    for (ibis::partList::const_iterator it = tlist.begin();
	 it != tlist.end(); ++it) {
	tinfo = new ibis::part::info(**it);
	lg() << "Partition " << tinfo->name << ":\n";
	std::vector<ibis::column::info*>::const_iterator vit;
	for (vit = tinfo->cols.begin(); vit != tinfo->cols.end(); ++vit)
	    lg() << (*vit)->name << ' ';
	lg() << "\n";
	delete tinfo;
    }
} // printNames

// print all partitions and columns
static void printAll(const ibis::partList& tlist) {
    ibis::util::logger lg(0);
    ibis::partList::const_iterator it;
    for (it = tlist.begin(); it != tlist.end(); ++it)
	(*it)->print(lg());
} // printAll

// Print the detailed information about a specific column.  It will use a
// more detailed distribution than that printed by function
// printDistribution.
static void printColumn(const ibis::part& tbl, const char* cname,
			const char* cond) {
    ibis::column* col = tbl.getColumn(cname);
    if (col) {
	std::vector<double> bounds;
	std::vector<uint32_t> counts;
	double amin = col->getActualMin();
	double amax = col->getActualMax();
	long nb = tbl.getCumulativeDistribution(cond, cname, bounds, counts);

	ibis::util::logger lg(0);
	lg() << "Column " << cname << " in Partition "
	     << tbl.name() << ":\n";
	if (nb > 0) {
	    col->print(lg());
	    lg() << ", actual range <" << amin << ", " << amax
		 << ">\ncumulative distribution [" << nb
		 << "]";
	    if (cond != 0 && *cond != 0)
		lg() << " under the condition of \"" << cond
		     << "\"";
	    lg() << "\n(bound,\t# records < bound)\n";
	    for (int j = 0; j < nb; ++ j) {
		if (j > 0 && ! (fabs(bounds[j] - bounds[j-1]) >
				1e-15*(fabs(bounds[j])+fabs(bounds[j-1]))))
		    lg() << "*** Error *** bounds[" << j
			 << "] is too close to bounds[" << j-1
			 << "]\n";
		lg() << bounds[j] << ",\t" << counts[j] << "\n";
	    }
	}
	else {
	    col->print(lg());
	    lg() << " -- getCumulativeDistribution(" << cname
		 << ") failed with error code " << nb;
	}
    }
} // printColumn

// Print the distribution of each column in the specified partition.  It
// uses two fixed size arrays for storing distributions.  This causes
// coarser distributions to printed.
static void printDistribution(const ibis::part& tbl) {
    double bounds[100];
    uint32_t counts[100];
    ibis::part::info tinfo(tbl);
    {
	ibis::util::logger lg(0);
	lg() << "Partition " << tinfo.name << " (" << tinfo.description
	     << ") -- nRows=" << tinfo.nrows << ", nCols="
	     << tinfo.cols.size() << "\nColumn names: ";
	for (uint32_t i = 0; i < tinfo.cols.size(); ++ i) {
	    lg() << tinfo.cols[i]->name << " ";
	}
    }
    for (uint32_t i = 0; i < tinfo.cols.size(); ++ i) {
	double amin = tbl.getActualMin(tinfo.cols[i]->name);
	double amax = tbl.getActualMax(tinfo.cols[i]->name);
	long ierr = tbl.getDistribution(tinfo.cols[i]->name,
					100, bounds, counts);

	ibis::util::logger lg(0); // use an IO lock
	lg() << "  Column " << tinfo.cols[i]->name << " ("
	     << tinfo.cols[i]->description << ") "
	     << ibis::TYPESTRING[tinfo.cols[i]->type]
	     << " expected range [" << tinfo.cols[i]->expectedMin
	     << ", " << tinfo.cols[i]->expectedMax << "]";
	if (ierr > 1) {
	    lg() <<", actual range <" << amin << ", " << amax
		 << ">\n # bins " << ierr << "\n";
	    lg() << "(..., " << bounds[0] << ")\t" << counts[0] << "\n";
	    for (int j = 1; j < ierr-1; ++ j) {
		if (! (fabs(bounds[j] - bounds[j-1]) >
		       1e-15*(fabs(bounds[j])+fabs(bounds[j-1]))))
		    lg() << "*** Error *** bounds[" << j
			 << "] is too close to bounds[" << j-1
			 << "]\n";
		lg() << "[" << bounds[j-1] << ", " << bounds[j] << ")\t"
		     << counts[j] << "\n";
	    }
	    lg() << "[" << bounds[ierr-2] << ", ...)\t"
		 << counts[ierr-1] << "\n";
	}
	else {
	    lg() << "\ngetCumulativeDistribution returned ierr="
		 << ierr << ", skip ...";
	}
    }
} // printDistribution

static void printDistribution(const ibis::partList& tlist) {
    ibis::partList::const_iterator it;
    for (it = tlist.begin(); it != tlist.end(); ++it) {
	printDistribution(**it);
    }
} // printDistribution

static void printJointDistribution(const ibis::part& tbl, const char *col1,
				   const char *col2, const char *cond) {
    std::vector<double> bds1, bds2;
    std::vector<uint32_t> cnts;
    ibis::util::logger lg(0);
    long ierr = tbl.getJointDistribution(cond, col1, col2, bds1, bds2, cnts);
    if (ierr > 0 && static_cast<uint32_t>(ierr) == cnts.size()) {
	const uint32_t nb2p1 = bds2.size() + 1;
	lg() << "\nJoint distribution of " << col1 << " and " << col2;
	if (cond && *cond)
	    lg() << " subject to the condition " << cond;
	lg() << ", # bins " << cnts.size() << " on " << bds1.size()+1
	     << " x " << bds2.size()+1 << " cells\n";

	uint32_t cnt = 0, tot=0;
	for (uint32_t i = 0; i < cnts.size(); ++ i) {
	    if (cnts[i] > 0) {
		uint32_t i1 = i / nb2p1;
		uint32_t i2 = i % nb2p1;
		if (i1 == 0)
		    lg() << "(..., " << bds1[0] << ")";
		else if (i1 < bds1.size())
		    lg() << "[" << bds1[i1-1] << ", " << bds1[i1]
			 << ")";
		else
		    lg() << "[" << bds1.back() << ", ...)";
		if (i2 == 0)
		    lg() << "(..., " << bds2[0] << ")";
		else if (i2 < bds2.size())
		    lg() << "[" << bds2[i2-1] << ", " << bds2[i2]
			 << ")";
		else
		    lg() << "[" << bds2.back() << ", ...)";
		lg() << "\t" << cnts[i] << "\n";
		tot += cnts[i];
		++ cnt;
	    }
	}
	lg() << "\tnumber of occupied cells: " << cnt
	     << ", total count = " << tot << "\n";
    }
} // printJointDistribution

// print some helpful information
static void print(const char* cmd, const ibis::partList& tlist) {
    if (cmd == 0 || *cmd == 0) return;

    const char* names = cmd;
    if (strnicmp(cmd, "print ", 6) == 0)
	names += 6;
    while (*names && isspace(*names))
	++ names;
    const char *cond = strchr(names, ':');
    if (cond > names) {
	*const_cast<char*>(cond) = 0; // add a null terminator
	// skip to the next non-space character
	for (++ cond; *cond != 0 && isspace(*cond); ++ cond);
    }
    if (strnicmp(names, "joint ", 6) == 0) {
	names += 6;
	bool warn = true;
	while (*names) {
	    std::string name1, name2;
	    int ierr = ibis::util::readString(name1, names);
	    if (ierr < 0 || name1.empty()) {
		if (warn)
		    LOGGER(0) << "the command print joint needs two "
			"column names as arguments";
		return;
	    }
	    ierr = ibis::util::readString(name2, names);
	    if (ierr < 0 || name2.empty()) {
		if (warn)
		    LOGGER(0) << "the command print joint needs two "
			"column names as arguments";
		return;
	    }
	    warn = false;
	    for (ibis::partList::const_iterator tit = tlist.begin();
		 tit != tlist.end(); ++ tit)
		printJointDistribution(**tit, name1.c_str(),
				       name2.c_str(), cond);
	}
    }
    else if (names) { // there are arguments after the print command
	ibis::nameList nlist(names); // split using the space as delimiter
	for (ibis::nameList::const_iterator it = nlist.begin();
	     it != nlist.end(); ++it) { // go through each name
	    ibis::partList::const_iterator tit = tlist.begin();
	    for (; tit != tlist.end() &&
		     stricmp(*it, (*tit)->name()) != 0 &&
		     ibis::util::strMatch((*tit)->name(), *it) == false;
		 ++ tit);
	    if (tit != tlist.end()) { // it's a data partition
		ibis::util::logger lg(0);
		lg() << "Partition " << (*tit)->name() << ":\n";
		(*tit)->print(lg());
	    }
	    else if ((*it)[0] == '*') {
		printAll(tlist);
	    }
	    else if (stricmp(*it, "parts") == 0) {
		ibis::util::logger lg(0);
		lg() << "Name(s) of all data partitioins\n";
		for (tit = tlist.begin(); tit != tlist.end(); ++tit)
		    lg() << (*tit)->name() << ' ';
	    }
	    else if (stricmp(*it, "names") == 0 ||
		     stricmp(*it, "columns") == 0) {
		printNames(tlist);
	    }
	    else if (stricmp(*it, "distributions") == 0) {
		printDistribution(tlist);
	    }
	    else { // assume it to be a column name
		for (tit = tlist.begin(); tit != tlist.end(); ++tit) {
		    printColumn(**tit, *it, cond);
		}
	    }
	}
    }
    else {
	ibis::util::logger lg(0);
	lg() << "Name(s) of all partitions\n";
	for (ibis::partList::const_iterator tit = tlist.begin();
	     tit != tlist.end(); ++tit)
	    lg() << (*tit)->name() << ' ';
    }
} // print

// Read SQL query statements terminated with semicolon (;).
static void readQueryFile(const char *fname, std::vector<std::string> &queff) {
    std::ifstream qfile(fname);
    if (! qfile) {
	ibis::util::logMessage("readQueryFile", "unable to open file \"%s\"",
			       fname);
	return;
    }

    char buf[MAX_LINE];
    std::string qtemp;
    while (qfile.getline(buf, MAX_LINE)) {
	if (*buf != 0 || *buf != '#') { // line started with # is a comment
	    char *ch = buf;
	    while (*ch != 0 && isspace(*ch)) ++ ch; // skip leading space
	    if (ch != buf)
		qtemp += ' '; // add a space

	    while (*ch != 0) {
		if (*ch == ';') { // terminating a SQL statement
		    if (! qtemp.empty()) {
			bool onlyspace = true;
			for (unsigned i = 0; onlyspace && i < qtemp.size();
			     ++ i)
			    onlyspace = (isspace(qtemp[i]) != 0);
			if (! onlyspace) {
			    queff.push_back(qtemp);
			}
		    }
		    qtemp.clear();
		    ++ ch;
		}
		else if (*ch == '-' && ch[1] == '-') {
		    *ch = 0; // ignore the rest of the line
		}
		else {
		    qtemp += *ch;
		    ++ ch;
		}
	    }
	}
    }
    if (! qtemp.empty()) {
	bool onlyspace = true;
	for (unsigned i = 0; onlyspace && i < qtemp.size(); ++ i)
	    onlyspace = (isspace(qtemp[i]) != 0);
	if (! onlyspace) {
	    queff.push_back(qtemp);
	}
    }
} // readQueryFile

// function to parse the command line arguments
static void parse_args(int argc, char** argv,
		       int& mode, ibis::partList& tlist,
		       stringArray& qlist, stringArray& alist,
		       std::vector<std::string> &queff) {
    mode = -1;
    tlist.clear();
    qlist.clear();
    alist.clear();

    std::vector<const char*> confs; // name of the configuration files
    std::vector<const char*> dirs;  // directories specified on command line
    std::vector<const char*> rdirs; // directories to be reordered
    std::string printcmd; // collect all print options into one string
    const char* mesgfile = 0;
    for (int i=1; i<argc; ++i) {
	if (*argv[i] == '-') { // normal arguments starting with -
	    switch (argv[i][1]) {
	    case 'a': // append a directory of data (must have a directory
	    case 'A': // name, optionally specify data partition name with "to
		      // name")
		if (i+1 < argc) {
		    alist.push_back(argv[i+1]);
		    if (i+3 < argc && stricmp(argv[i+2], "to")==0 &&
			argv[i+3][0] != '-') {
			appendto = argv[i+3];
			i += 3;
		    }
		    else if (i+2 < argc && argv[i+2][0] != '-') {
			appendto = argv[i+2];
			i += 2;
		    }
		    else {
			++ i;
		    }
		}
	    break;
	    case 'b':
	    case 'B': { // build indexes,
		// it also accepts an optional argument to indicate the
		// number of threads to use
		char *ptr = strchr(argv[i], '=');
		if (ptr == 0) {
		    if (i+1 < argc) {
			if (isdigit(*argv[i+1])) {
			    build_index += atoi(argv[i+1]);
			    i = i + 1;
			}
			else {
			    ++ build_index;
			    if (*argv[i+1] != '-') {
				// assume to be an index specification
				indexingOption = argv[i+1];
				i = i + 1;
			    }
			}
		    }
		    else {
			++ build_index;
		    }
		}
		else {
		    build_index += atoi(++ptr);
		    if (i+1 < argc && *argv[i+1] != '-') {
			// assume to be an index specification
			indexingOption = argv[i+1];
			i = i + 1;
		    }
		}
		break;}
	    case 'c':
	    case 'C': // configuration file, multiple files allowed
		if (i+1 < argc) {
		    confs.push_back(argv[i+1]);
		    ++ i;
		}
	    break;
	    case 'd':
	    case 'D': // data directory, multiple directory allowed
		if (i+1 < argc && argv[i+1][0] != '-') {
		    dirs.push_back(argv[i+1]);
		    i = i + 1;
		}
		else {
		    std::clog << "Warning: argument -d must be followed by "
			      << "a directory name" << std::endl;
		}
	    break;
	    case 'e':
	    case 'E': // estiamtion only
		estimate_only = true;
	    if (skip_estimation)
		skip_estimation = false;
	    break;
	    case 'f':
	    case 'F': // query file, multiple files allowed
		if (i+1 < argc) {
		    readQueryFile(argv[i+1], queff);
		    ++ i;
		}
	    break;
	    default:
	    case 'h':
	    case 'H': // print usage
		usage(*argv);
	    if (argc <= 2)
		exit(0);
	    break;
	    case 'i':
	    case 'I': // interactive mode
		mode = 1;
	    break;
	    case 'j':
	    case 'J': // junk some rows of every data partition available
		// must have an argument after the flag to indicate a file
		// containing row numbers or a string indicate conditions
		// on rows to mark as inactive/junk
		if (i+1 < argc && *argv[i+1] != '-') {
		    junkstring = argv[i+1];
		    i = i + 1;
		}
	    break;
	    case 'k':
	    case 'K': // keep temporary query files or reverse -j
		if (i+1 < argc && *argv[i+1] != '-') { // reverse -j
		    keepstring = argv[i+1];
		    i = i + 1;
		}
		else { // keep temporary files
		    ibis::query::keepQueryRecords();
		}
	    break;
	    case 'l':
	    case 'L': // logfile or load index in one-shot
		if (i+1 < argc && argv[i+1][0] != '-') {
		    mesgfile = argv[i+1];
		    ++ i;
		}
		else if ((argv[i][2] == 'o' || argv[i][2] == 'O') &&
			 (argv[i][3] == 'g' || argv[i][3] == 'G')) {
		    mesgfile = 0; // reset the log file to stdout
		}
	    break;
#if defined(TEST_SUMBINS_OPTIONS)
	    case 'm':
	    case 'M': {// _sumBins_option
		char* ptr = strchr(argv[i], '=');
		if (ptr != 0) {
		    ++ ptr; // skip '='
		    ibis::_sumBins_option = atoi(ptr);
		}
		else if (i+1 < argc) {
		    if (isdigit(*argv[i+1])) {
			ibis::_sumBins_option = atoi(argv[i+1]);
			i = i + 1;
		    }
		}
		break;}
#endif
	    case 'n':
	    case 'N': {
		// skip estimation, directly call function evaluate
		skip_estimation = true;
		if (estimate_only)
		    estimate_only = false;
		break;}
	    case 'o':
	    case 'O':
		if (argv[i][2] == 'n' || argv[i][2] == 'N') {
		    // skip estimation, directly call function evaluate
		    skip_estimation = true;
		    if (estimate_only)
			estimate_only = false;
		}
		else if (i+1 < argc && argv[i+1][0] != '-') {
		    // output file specified
		    outputfile = argv[i+1];
		    i = i + 1;
		}
	    break;
	    case 'p':
	    case 'P': // collect the print options
		if (i+1 < argc) {
		    if (argv[i+1][0] != '-') {
			if (! printcmd.empty()) {
			    printcmd += ", ";
			    printcmd += argv[i+1];
			}
			else {
			    printcmd = argv[i+1];
			}
			++ i;
		    }
		    else if (printcmd.empty()) {
			printcmd = "parts";
		    }
		}
		else  if (printcmd.empty()) { // at least print partition names
		    printcmd = "parts";
		}
	    break;
	    case 'q':
	    case 'Q': // specify a query "[select ...] [from ...] where ..."
		if (i+1 < argc) {
		    qlist.push_back(argv[i+1]);
		    ++ i;
		}
	    break;
	    case 's':
	    case 'S': // sequential scan, or scan option
#if defined(TEST_SCAN_OPTIONS)
		if (i+1 < argc) {
		    if (isdigit(*argv[i+1])) {
			ibis::_scan_option = atoi(argv[i+1]);
			i = i + 1;
		    }
		    else {
			sequential_scan = true;
		    }
		}
		else {
		    sequential_scan = true;
		}
#else
	    sequential_scan = true;
#endif
	    break;
	    case 't':
	    case 'T': { // self-testing mode or number of threads
		char *ptr = strchr(argv[i], '=');
		if (ptr == 0) {
		    if (i+1 < argc) {
			if (isdigit(*argv[i+1])) {
			    testing += atoi(argv[i+1]);
			    i = i + 1;
			}
			else {
			    ++ testing;
			}
		    }
		    else {
			++ testing;
		    }
		}
		else {
		    testing += atoi(++ptr);
		}
		break;}
	    case 'v':
	    case 'V': { // verboseness
		char *ptr = strchr(argv[i], '=');
		if (ptr == 0) {
		    if (i+1 < argc) {
			if (isdigit(*argv[i+1])) {
			    ibis::gVerbose += atoi(argv[i+1]);
			    i = i + 1;
			}
			else {
			    ++ ibis::gVerbose;
			}
		    }
		    else {
			++ ibis::gVerbose;
		    }
		}
		else {
		    ibis::gVerbose += atoi(++ptr);
		}
		break;}
	    case 'z':
	    case 'Z': {
		zapping = true;
		break;}
	    } // switch (argv[i][1])
	} // normal arguments
	else { // argument not started with '-' and not following
	       // apropriate '-' operations are assumed to be names of the
	       // data directories and are read two at a time
	    dirs.push_back(argv[i]);
	}
    } // for (inti=1; ...)
#if defined(DEBUG) || defined(_DEBUG)
#if DEBUG + 0 > 10 || _DEBUG + 0 > 10
    ibis::gVerbose = INT_MAX;
#elif DEBUG + 0 > 0
    ibis::gVerbose += 7 * DEBUG;
#elif _DEBUG + 0 > 0
    ibis::gVerbose += 5 * _DEBUG;
#else
    ibis::gVerbose += 3;
#endif
#endif

    for (unsigned i = 0; i < queff.size(); ++ i) {
	qlist.push_back(queff[i].c_str());
    }
    if (mode < 0) {
	mode = (qlist.empty() && testing <= 0 && build_index <= 0 &&
		alist.empty() && printcmd.empty() &&
		rdirs.empty() && junkstring == 0 && keepstring == 0);
    }
    if (qlist.size() > 1U) {
	if (testing > 0) {
	    threading = testing;
	    testing = 0;
	}
	else {
#if defined(_SC_NPROCESSORS_ONLN)
	    threading = sysconf(_SC_NPROCESSORS_ONLN);
#elif defined(_WIN32) && defined(_MSC_VER)
	    SYSTEM_INFO myinfo;
	    GetSystemInfo(&myinfo);
	    threading = myinfo.dwNumberOfProcessors;
#endif
	    if (threading > 2) // more than two processor, leave one for OS
		-- threading;
	}
	if (threading > qlist.size())
	    threading = static_cast<unsigned>
		(ceil(sqrt(static_cast<double>(qlist.size()))));
	if (threading <= 1) // make sure it exactly 0
	    threading = 0;
    }
    if (mesgfile != 0 && *mesgfile != 0) {
	int ierr = ibis::util::setLogFileName(mesgfile);
	if (ierr < 0)
	    std::clog << *argv << " failed to open file " << mesgfile
		      << " for logging error messages" << std::endl;
	else if (ibis::gVerbose > 2)
	    std::clog << *argv << " will write messages to " << mesgfile
		      << std::endl;
    }
    if (ibis::gVerbose > 0) {
	ibis::util::logger lg(1);
	lg() << "\n" << argv[0] << ": "
	     << (mode ? "interactive mode" : "batch mode")
	     << ", log level " << ibis::gVerbose;
	if (build_index > 0) {
	    lg() << ", building indexes";
	    if (zapping)
		lg() << " (remove any existing indexes)";
	}
	if (testing > 0)
	    lg() << ", performing self test";
	if (threading > 0)
	    lg() << ", threading " << threading;
	if (skip_estimation)
	    lg() << ", skipping estimation";
	else if (estimate_only)
	    lg() << ", computing only bounds";
	if (! alist.empty()) {
	    lg() << "\nappending data in the following director"
		 << (alist.size()>1 ? "ies" : "y");
	    if (appendto)
		lg() << " to partition " << appendto;
	    for (uint32_t i = 0; i < alist.size(); ++ i)
		lg() << "\n" << alist[i];
	}
	lg() << "\n";
    }
    if (! confs.empty()) {
	// read all configuration files
	for (uint32_t i = 0; i < confs.size(); ++ i)
	    ibis::gParameters().read(confs[i]);
    }
    else if (ibis::gParameters().empty()) {
	// read default parameter files
	ibis::gParameters().read();
    }

    // reorder the data directories first
    for (unsigned i = 0; i < rdirs.size(); ++ i) {
	ibis::part tbl(rdirs[i], static_cast<const char*>(0));
	tbl.reorder();
    }

    // construct the paritions using both the command line arguments and
    // the resource files
    ibis::util::gatherParts(tlist, ibis::gParameters());
    for (std::vector<const char*>::const_iterator it = dirs.begin();
	 it != dirs.end(); ++ it) {
	ibis::util::gatherParts(tlist, *it);
    }

    if (ibis::gVerbose > 1) {
	ibis::util::logger lg(2);
	if (tlist.size()) {
	    lg() << "Partition" << (tlist.size()>1 ? "s" : "")
		 << "[" << tlist.size() << "]:\n";
	    for (ibis::partList::const_iterator it = tlist.begin();
		 it != tlist.end(); ++it)
		lg() << (*it)->name() << "\n";
	}
	if (qlist.size()) {
	    lg() << "Quer" << (qlist.size()>1 ? "ies" : "y")
		 << "[" << qlist.size() << "]:\n";
	    for (stringArray::const_iterator it = qlist.begin();
		 it != qlist.end(); ++it)
		lg() << *it << "\n";
	}
    }

    if (ibis::gVerbose > 1 &&
	(testing > 1 || build_index > 0 || ! printcmd.empty())) {
	for (ibis::partList::const_iterator it = tlist.begin();
	     it != tlist.end(); ++it) {
	    bool recompute = (testing>5 && ibis::gVerbose>7);
	    // check to see if the nominal min and max are different
	    ibis::part::info *info = (*it)->getInfo();
	    for (uint32_t i = 0; i < info->cols.size() && ! recompute; ++i)
		recompute = (info->cols[i]->type != ibis::CATEGORY &&
			     info->cols[i]->type != ibis::TEXT &&
			     info->cols[i]->expectedMin >
			     info->cols[i]->expectedMax);
	    delete info;   // no use for it any more
	    if (recompute) {// acutally compute the min and max of attributes
		LOGGER(2) << *argv
			  << ": recomputing the min/max for partition "
			  << (*it)->name();
		(*it)->computeMinMax();
	    }
	}
    }
    if (! printcmd.empty()) {
	LOGGER(4) << "printcmd ='" << printcmd << "' --";
	print(printcmd.c_str(), tlist);
    }
} // parse_args

// evaluate a single query -- directly retrieve values of selected columns
static void xdoQuery(const char* uid, ibis::part* tbl, const char* wstr,
		     const char* sstr) {
    LOGGER(1) << "xdoQuery -- processing query " << wstr
	      << " on partition " << tbl->name();

    ibis::query aQuery(uid, tbl); // in case of exception, content of query
				  // will be automatically freed
    long num1, num2;
    aQuery.setWhereClause(wstr);
    if (aQuery.getWhereClause() == 0)
	return;
    if (zapping) {
	std::string old = aQuery.getWhereClause();
	std::string comp = aQuery.removeComplexConditions();
	if (ibis::gVerbose > 1) {
	    ibis::util::logger lg(1);
	    if (! comp.empty())
		lg() << "xdoQuery -- the WHERE clause \"" << old.c_str()
		     << "\" is split into \"" << comp.c_str()
		     << "\" AND \"" << aQuery.getWhereClause() << "\"";
	    else
		lg() << "xdoQuery -- the WHERE clause \""
		     << aQuery.getWhereClause()
		     << "\" is considered simple";
	}
    }
    const char* asstr = 0;
    if (sstr != 0) {
	aQuery.setSelectClause(sstr);
	asstr = aQuery.getSelectClause();
    }

    if (! skip_estimation) {
	num2 = aQuery.estimate();
	if (num2 < 0) {
	    LOGGER(0) << "xdoQuery -- failed to estimate \"" << wstr
		      << "\", error code = " << num2;
	    return;
	}
	num1 = aQuery.getMinNumHits();
	num2 = aQuery.getMaxNumHits();
	if (ibis::gVerbose > 0) {
	    ibis::util::logger lg(0);
	    lg() << "xdoQuery -- the number of hits is ";
	    if (num2 > num1) 
		lg() << "between " << num1 << " and ";
	    lg() << num2;
	}
	if (estimate_only)
	    return;
    }

    num2 = aQuery.evaluate();
    if (num2 < 0) {
	LOGGER(0) << "xdoQuery -- failed to evaluate \"" << wstr
		  << "\", error code = " << num2;
	return;
    }
    num1 = aQuery.getNumHits();
    LOGGER(1) << "xdoQuery -- the number of hits = " << num1;

    if (asstr != 0 && *asstr != 0 && num1 > 0) {
	ibis::nameList names(asstr);
	for (ibis::nameList::const_iterator it = names.begin();
	     it != names.end(); ++it) {
	    ibis::column* col = tbl->getColumn(*it);
	    if (col) {
		LOGGER(1) << "xdoQuery -- retrieving qualified "
		    "values of " << *it;

		switch (col->type()) {
		case ibis::UBYTE:
		case ibis::BYTE:
		case ibis::USHORT:
		case ibis::SHORT:
		case ibis::UINT:
		case ibis::INT: {
		    ibis::array_t<int32_t>* intarray;
		    intarray = aQuery.getQualifiedInts(*it);
		    ibis::util::logger lg(0);
		    if (intarray->size() != static_cast<uint32_t>(num1))
			lg()
			    << "expected to retrieve " << num1
			    << " entries, but got " << intarray->size();
		    if (num1 < (2 << ibis::gVerbose) ||
			ibis::gVerbose > 30) {
			lg() << "selected entries of column " << *it
			     << "\n";
			for (ibis::array_t<int32_t>::const_iterator ait =
				 intarray->begin();
			     ait != intarray->end(); ++ait)
			    lg() << *ait << "\n";
		    }
		    else {
			lg() << "xdoQuery -- retrieved "
			     << intarray->size()
			     << " ints (expecting " << num1 << ")\n";
		    }
		    delete intarray;
		    break;}

		case ibis::FLOAT: {
		    ibis::array_t<float>* floatarray;
		    floatarray = aQuery.getQualifiedFloats(*it);

		    ibis::util::logger lg(0);
		    if (floatarray->size() !=
			static_cast<uint32_t>(num1))
			lg() << "expected to retrieve " << num1
			     << " entries, but got "
			     << floatarray->size();
		    if (num1 < (2 << ibis::gVerbose) ||
			ibis::gVerbose > 30) {
			lg() << "selected entries of column " << *it;
			for (ibis::array_t<float>::const_iterator ait =
				 floatarray->begin();
			     ait != floatarray->end(); ++ait)
			    lg() << "\n" << *ait;
		    }
		    else {
			lg() << "xdoQuery -- retrieved "
			     << floatarray->size()
			     << " floats (expecting " << num1 << ")";
		    }
		    delete floatarray;
		    break;}

		case ibis::DOUBLE: {
		    ibis::array_t<double>* doublearray;
		    doublearray = aQuery.getQualifiedDoubles(*it);

		    ibis::util::logger lg(0);
		    if (doublearray->size() !=
			static_cast<uint32_t>(num1))
			lg() << "expected to retrieve " << num1
			     << " entries, but got "
			     << doublearray->size();
		    if (num1<(2<<ibis::gVerbose) || ibis::gVerbose>30) {
			lg() << "selected entries of column " << *it;
			for (ibis::array_t<double>::const_iterator ait =
				 doublearray->begin();
			     ait != doublearray->end(); ++ait)
			    lg() << "\n" << *ait;
		    }
		    else {
			lg() << "xdoQuery -- retrieved "
			     << doublearray->size()
			     << " doubles (expecting " << num1 << ")";
		    }
		    delete doublearray;
		    break;}
		default:
		    LOGGER(0) << "column " << *it << " has an unsupported "
			      << "type(" << static_cast<int>(col->type())
			      << ")";
		}
	    } // if (col)...
	} // for ...
    } // if (asstr != 0 && num1 > 0)
} // xdoQuery

// This print function takes the most general option in getting the values
// out of a query.  If the values in the select clause are of known type,
// those types should be used instead of @c getString.
static void printQueryResults(std::ostream &out, ibis::query &q) {
    ibis::query::result cursor(q);
    out << "printing results of query " << q.id() << "(numHits="
	<< q.getNumHits() << ")\n"
	<< q.getSelectClause() << std::endl;
    const ibis::selectClause& sel = q.components();
    if (sel.size() == 0) return;

    while (cursor.next()) {
	out << cursor.getString(static_cast<uint32_t>(0U));
	for (uint32_t i = 1; i < sel.size(); ++ i)
	    out << ", " << cursor.getString(i);
	out << "\n";
    }
} // printQueryResults

// evaluate a single query -- print selected columns through ibis::bundle
static void doQuery(const char* uid, ibis::part* tbl, const char* wstr,
		    const char* sstr, const char* ordkeys, int direction,
		    const uint32_t limit) {
    std::string sqlstring; //
    {
	std::ostringstream ostr;
	if (sstr != 0 && *sstr != 0)
	    ostr << "SELECT " << sstr;
	ostr << " FROM " << tbl->name() << " WHERE "<< wstr;
	if (ordkeys && *ordkeys) {
	    ostr << " ORDER BY " << ordkeys;
	    if (direction >= 0)
		ostr << " ASC";
	    else
		ostr << " DESC";
	}
	if (limit > 0)
	    ostr << " LIMIT " << limit;
	sqlstring = ostr.str();
    }
    LOGGER(2) << "doQuery -- processing \"" << sqlstring << '\"';

    long num1, num2;
    ibis::horometer timer;
    timer.start();
    // the third argument is needed to make sure a private directory is
    // created for the query object to store the results produced by the
    // select clause.
    ibis::query aQuery(uid, tbl,
		       ((sstr != 0 && *sstr != 0 &&
			 ((ordkeys != 0 && *ordkeys != 0) || limit > 0 ||
			  testing > 0)) ?
			"ibis" : static_cast<const char*>(0)));
    aQuery.setWhereClause(wstr);
    if (aQuery.getWhereClause() == 0)
	return;
    if (zapping && aQuery.getWhereClause()) {
	std::string old = aQuery.getWhereClause();
	std::string comp = aQuery.removeComplexConditions();
	if (ibis::gVerbose > 1) {
	    ibis::util::logger lg(1);
	    if (! comp.empty())
		lg() << "doQuery -- the WHERE clause \""
		     <<  old.c_str() << "\" is split into \""
		     << comp.c_str()  << "\" AND \""
		     << aQuery.getWhereClause() << "\"";
	    else
		lg() << "doQuery -- the WHERE clause \""
		     << aQuery.getWhereClause()
		     << "\" is considered simple";
	}
    }

    const char* asstr = 0;
    if (sstr != 0 && *sstr != 0) {
	aQuery.setSelectClause(sstr);
	asstr = aQuery.getSelectClause();
    }

    if (sequential_scan) {
	num2 = aQuery.countHits();
	// 	if (num2 < 0) {
	// 	    ibis::bitvector btmp;
	// 	    num2 = aQuery.sequentialScan(btmp);
	// 	    if (num2 < 0) {
	// 		ibis::util::logger lg(0);
	// 		lg() << "doQuery:: sequentialScan("
	// 			    << aQuery.getWhereClause() << ") failed";
	// 		return;
	// 	    }

	// 	    num2 = btmp.cnt();
	// 	}
	if (ibis::gVerbose >= 0) {
	    timer.stop();
	    ibis::util::logger lg(0);
	    lg() << "doQuery:: sequentialScan("
		 << aQuery.getWhereClause() << ") produced "
		 << num2 << " hit" << (num2>1 ? "s" : "") << ", took "
		 << timer.CPUTime() << " CPU seconds and "
		 << timer.realTime() << " elapsed seconds";
	}
	return;
    }

    if (! skip_estimation) {
	num2 = aQuery.estimate();
	if (num2 < 0) {
	    LOGGER(0) << "doQuery -- failed to estimate \"" << wstr
		      << "\", error code = " << num2;
	    return;
	}
	num1 = aQuery.getMinNumHits();
	num2 = aQuery.getMaxNumHits();
	if (ibis::gVerbose > 1) {
	    ibis::util::logger lg(1);
	    lg() << "doQuery -- the number of hits is ";
	    if (num2 > num1)
		lg() << "between " << num1 << " and ";
	    lg() << num2;
	}
	if (estimate_only) {
	    if (ibis::gVerbose >= 0) {
		timer.stop();
		ibis::util::logger lg(0);
		lg() << "doQuery:: estimate("
		     << aQuery.getWhereClause() << ") took "
		     << timer.CPUTime() << " CPU seconds and "
		     << timer.realTime() << " elapsed seconds";
	    }
	    return; // stop here is only want to estimate
	}
    }

    num2 = aQuery.evaluate();
    if (num2 < 0) {
	LOGGER(0) << "doQuery -- failed to evaluate \"" << wstr
		  << "\", error code = " << num2;
	return;
    }
    if ((ordkeys && *ordkeys) || limit > 0) { // top-K query
	aQuery.limit(ordkeys, direction, limit, (testing > 0));
    }
    num1 = aQuery.getNumHits();

    if (asstr != 0 && *asstr != 0 && num1 > 0 && ibis::gVerbose >= 0) {
	static bool appendToOutput = false;
	if (0 != outputfile && 0 == strcmp(outputfile, "/dev/null")) {
	    // read the values into memory, but avoid sorting the values
	    const ibis::selectClause& cmps = aQuery.components();
	    const uint32_t ncol = cmps.size();
	    const ibis::bitvector* hits = aQuery.getHitVector();
	    for (uint32_t i=0; i < ncol; ++i) {
		const ibis::column* cptr = tbl->getColumn(cmps.argName(i));
		if (cptr != 0) {
		    ibis::colValues* tmp;
		    tmp = ibis::colValues::create(cptr, *hits);
		    delete tmp;
		}
	    }
	}
	else if (testing > 1) { // use the new cursor class for print
	    if (outputfile != 0 && *outputfile != 0) {
		std::ofstream output(outputfile,
				     std::ios::out |
				     (appendToOutput ? std::ios::app :
				      std::ios::trunc));
		if (output) {
		    LOGGER(0) << "doQuery -- query ("
			      <<  aQuery.getWhereClause()
			      << ") results writtent to file \""
			      <<  outputfile << "\"";
		    printQueryResults(output, aQuery);
		}
		else {
		    ibis::util::logger lg(0);
		    lg() << "Warning ** doQuery failed to open \""
			 << outputfile << "\" for writing query ("
			 << aQuery.getWhereClause() << ")";
		    printQueryResults(lg(), aQuery);
		}
	    }
	    else {
		ibis::util::logger lg(0);
		printQueryResults(lg(), aQuery);
	    }
	}
	else if (outputfile != 0 && *outputfile != 0) {
	    std::ofstream output(outputfile,
				 std::ios::out | 
				 (appendToOutput ? std::ios::app :
				  std::ios::trunc));
	    if (output) {
		LOGGER(0) << "doQuery -- query ("
			  <<  aQuery.getWhereClause()
			  << ") results writtent to file \""
			  <<  outputfile << "\"";
		if (ibis::gVerbose > 8)
		    aQuery.printSelectedWithRID(output);
		else
		    aQuery.printSelected(output);
	    }
	    else {
		ibis::util::logger lg(0);
		lg() << "Warning ** doQuery failed to open file \""
		     << outputfile << "\" for writing query ("
		     << aQuery.getWhereClause() << ")\n";
		if (ibis::gVerbose > 8)
		    aQuery.printSelectedWithRID(lg());
		else
		    aQuery.printSelected(lg());
	    }
	}
	else {
	    ibis::util::logger lg(0);
	    if (ibis::gVerbose > 8)
		aQuery.printSelectedWithRID(lg());
	    else
		aQuery.printSelected(lg());
	}
	appendToOutput = true; // all query output go to the same file
    }
    if (ibis::gVerbose >= 0) {
	timer.stop();
	ibis::util::logger lg(0);
	lg() << "doQuery:: evaluate(" << sqlstring
	     << ") produced " << num1 << (num1 > 1 ? " hits" : " hit")
	     << ", took " << timer.CPUTime() << " CPU seconds and "
	     << timer.realTime() << " elapsed seconds";
    }

    //     if (testing > 1) {
    // 	ibis::bitvector btmp;
    // 	num2 = aQuery.sequentialScan(btmp);
    // 	if (num2 < 0) {
    // 	    ibis::util::logger lg(0);
    // 	    lg() << "doQuery:: sequentialScan("
    // 			<< aQuery.getWhereClause() << ") failed";
    // 	}
    // 	else {
    // 	    num2 = btmp.cnt();
    // 	    if (num1 != num2 && ibis::gVerbose >= 0) {
    // 		ibis::util::logger lg(0);
    // 		lg() << "Warning ** query \"" << aQuery.getWhereClause()
    // 			    << "\" generated " << num1
    // 			    << " hit" << (num1 >1  ? "s" : "")
    // 			    << " with evaluate(), but generated "
    // 			    << num2 << " hit" << (num2 >1  ? "s" : "")
    // 			    << " with sequentialScan";
    // 	    }

    // 	    if (asstr != 0 && *asstr != 0) {
    // 		// create bundles, i.e., retrieve the selected values
    // 		timer.start();
    // 		ibis::bundle* bdl = ibis::bundle::create(aQuery, btmp);
    // 		delete bdl;
    // 		timer.stop();
    // 		ibis::util::logger lg(0);
    // 		lg() << "doQuery ibis::bundle::create generated "
    // 			    << num2 << " bundles in " << timer.CPUTime()
    // 			    << " CPU seconds and " << timer.realTime()
    // 			    << " elapsed seconds";
    // 	    }
    // 	}
    //     }
} // doQuery

// evaluate a single query -- only work on partitions that have defined
// column shapes, i.e., they contain data computed on meshes.
static void doMeshQuery(const char* uid, ibis::part* tbl, const char* wstr,
			const char* sstr) {
    LOGGER(1) << "doMeshQuery -- processing query " << wstr
	      << " on partition " << tbl->name();

    long num1, num2;
    ibis::horometer timer;
    timer.start();
    ibis::meshQuery aQuery(uid, tbl);
    aQuery.setWhereClause(wstr);
    if (aQuery.getWhereClause() == 0)
	return;
    if (zapping && aQuery.getWhereClause()) {
	std::string old = aQuery.getWhereClause();
	std::string comp = aQuery.removeComplexConditions();
	if (ibis::gVerbose > 1) {
	    ibis::util::logger lg(0);
	    if (! comp.empty())
		lg() << "doMeshQuery -- the WHERE clause \""
		     << old.c_str() << "\" is split into \""
		     << comp.c_str() << "\" AND \""
		     << aQuery.getWhereClause() << "\"";
	    else
		lg() << "doMeshQuery -- the WHERE clause \""
		     << aQuery.getWhereClause()
		     << "\" is considered simple";
	}
    }

    const char* asstr = 0;
    if (sstr != 0 && *sstr != 0) {
	aQuery.setSelectClause(sstr);
	asstr = aQuery.getSelectClause();
    }
    if (! skip_estimation) {
	num2 = aQuery.estimate();
	if (num2 < 0) {
	    LOGGER(0) << "doMeshQuery -- failed to estimate \"" << wstr
		      << "\", error code = " << num2;
	    return;
	}
	num1 = aQuery.getMinNumHits();
	num2 = aQuery.getMaxNumHits();
	if (ibis::gVerbose > 0) {
	    ibis::util::logger lg(1);
	    lg() << "doMeshQuery -- the number of hits is ";
	    if (num1 < num2)
		lg() << "between " << num1 << " and ";
	    lg() << num2;
	}
	if (estimate_only) {
	    if (ibis::gVerbose >= 0) {
		timer.stop();
		ibis::util::logger lg(0);
		lg() << "doMeshQuery:: estimate("
		     << aQuery.getWhereClause() << ") took "
		     << timer.CPUTime() << " CPU seconds and "
		     << timer.realTime() << " elapsed seconds";
	    }
	    return; // stop here is only want to estimate
	}
    }

    num2 = aQuery.evaluate();
    if (num2 < 0) {
	LOGGER(0) << "doMeshQuery -- failed to evaluate \"" << wstr
		  << "\", error code = " << num2;
	return;
    }
    num1 = aQuery.getNumHits();
    if (ibis::gVerbose >= 0) {
	timer.stop();
	ibis::util::logger lg(0);
	lg() << "doMeshQuery:: evaluate("
	     << aQuery.getWhereClause() 
	     << ") produced " << num1 << (num1 > 1 ? " hits" : " hit")
	     << ", took " << timer.CPUTime() << " CPU seconds and "
	     << timer.realTime() << " elapsed seconds";
    }

    std::vector< std::vector<uint32_t> > ranges;
    num2 = aQuery.getHitsAsBlocks(ranges);
    if (num2 < 0) {
	LOGGER(1) << "aQuery.getHitsAsBlocks() returned " << num2;
    }
    else if (ranges.empty()) {
	LOGGER(2) << "aQuery.getHitsAsBlocks() returned empty ranges";
    }
    else {
	ibis::util::logger lg(0);
	lg() << "aQuery.getHitsAsBlocks() returned " << ranges.size()
	     << " range" << (ranges.size() > 1 ? "s" : "") << " in "
	     << ranges[0].size()/2 << "-D space\n";
	if (ibis::gVerbose > 3) { // print all the ranges
	    uint32_t tot = (ibis::gVerbose >= 30 ? ranges.size() :
			    (1U << ibis::gVerbose));
	    if (tot > ranges.size())
		tot = ranges.size();
	    for (uint32_t i = 0; i < tot; ++i) {
		lg() << i << "\t(";
		for (uint32_t j = 0; j < ranges[i].size(); ++j) {
		    if (j > 0)
			lg() << ", ";
		    lg() << ranges[i][j];
		}
		lg() << ")\n";
	    }
	    if (tot < ranges.size()) {
		tot = ranges.size() - 1;
		lg() << "...\n" << tot << "\t(";
		for (uint32_t j = 0; j < ranges[tot].size(); ++j) {
		    if (j > 0)
			lg() << ", ";
		    lg() << ranges[tot][j];
		}
		lg() << ")";
	    }
	}
    }

    num2 = aQuery.getPointsOnBoundary(ranges);
    if (num2 < 0) {
	LOGGER(0) << "Warning ** aQuery.getPointsOnBoundary() returned "
		  << num2;
    }
    else if (ranges.empty()) {
	LOGGER(2) << "Warning ** aQuery.getPointsOnBoundary() "
	    "returned empty ranges";
    }
    else {
	ibis::util::logger lg(0);
	lg() << "aQuery.getPointsOnBoundary() returned "
	     << ranges.size() << " point"
	     << (ranges.size() > 1 ? "s" : "") << " in "
	     << ranges[0].size() << "-D space\n";
	if (ibis::gVerbose > 3) { // print all the points
	    uint32_t tot = (ibis::gVerbose >= 30 ? ranges.size() :
			    (1U << ibis::gVerbose));
	    if (tot > ranges.size())
		tot = ranges.size();
	    if (tot < ranges.size()) {
		for (uint32_t i = 0; i < tot; ++i) {
		    lg() << i << "\t(" << ranges[i][0];
		    for (uint32_t j = 1; j < ranges[i].size(); ++j) {
			lg() << ", " << ranges[i][j];
		    }
		    lg() << ")\n";
		}
		tot = ranges.size() - 1;
		lg() << "...\n" << tot << "\t(" << ranges[tot][0];
		for (uint32_t j = 1; j < ranges[tot].size(); ++j) {
		    lg() << ", " << ranges[tot][j];
		}
		lg() << ")";
	    }
	    else {
		for (uint32_t i = 0; i < ranges.size(); ++ i) {
		    lg() << "(" << ranges[i][0];
		    for (uint32_t j = 1; j < ranges[i].size(); ++ j)
			lg() << ", " << ranges[i][j];
		    lg() << ")";
		}
	    }
	}
    }

    if (asstr != 0 && *asstr != 0 && num1 > 0 && ibis::gVerbose > 0) {
	if (outputfile != 0 && *outputfile != 0) {
	    std::ofstream output(outputfile,
				 std::ios::out | std::ios::app);
	    if (output) {
		LOGGER(1) << "doMeshQuery -- query ("
			  << aQuery.getWhereClause()
			  << ") results writtent to file \""
			  << outputfile << "\"";
		if (ibis::gVerbose > 8)
		    aQuery.printSelectedWithRID(output);
		else
		    aQuery.printSelected(output);
	    }
	    else {
		ibis::util::logger lg(0);
		lg() << "Warning ** doMeshQuery failed to "
		     << "open file \"" << outputfile
		     << "\" for writing query ("
		     << aQuery.getWhereClause() << ") output\n";
		if (ibis::gVerbose > 8)
		    aQuery.printSelectedWithRID(lg());
		else
		    aQuery.printSelected(lg());
	    }
	}
	else {
	    ibis::util::logger lg(0);
	    if (ibis::gVerbose > 8)
		aQuery.printSelectedWithRID(lg());
	    else
		aQuery.printSelected(lg());
	}
    } // if (asstr != 0 && num1>0 && ibis::gVerbose > 0)
} // doMeshQuery

// append the content of the named directory to the existing partitions
static void doAppend(const char* dir, ibis::partList& tlist) {
    long ierr = 0;
    ibis::part *tbl = 0;
    bool newtable = true;
    if (appendto != 0) { // try to use the specified partition name
	ibis::partList::iterator itt;
	for (itt = tlist.begin(); itt != tlist.end() &&
		 stricmp(appendto, (*itt)->name()) != 0; ++ itt);
	if (itt != tlist.end()) { // found an existing partition
	    tbl = *itt;
	    newtable = false;
	}
    }

    if (tbl == 0) { // need to allocate a new partition
	if (appendto != 0) { // use externally specified name
	    tbl = new ibis::part(appendto);
	}
	else { // generate an random name based on user name and dir
	    char tmp[128];
	    const char* name = ibis::util::userName();
	    sprintf(tmp, "%c%lX", (isalpha(*name) ? toupper(*name) : 'T'),
		    static_cast<long unsigned>
		    (ibis::util::checksum(dir, strlen(dir))));
	    tbl = new ibis::part(tmp);
	}
	newtable = true;
    }
    if (tbl == 0) {
	LOGGER(0) << "doAppend(" << dir << ") failed to allocate an "
	    "ibis::part object. Can NOT continue.\n";
	return;
    }

    ibis::horometer timer;
    timer.start();
    ierr = tbl->append(dir);
    timer.stop();
    if (ierr < 0) {
	LOGGER(0) << "doAppend(" << dir << "): appending to data partition \""
		  << tbl->name() << "\" failed (ierr = " << ierr << ")\n";
	if (newtable)
	    delete tbl;
	return;
    }
    else {
	LOGGER(0) << "doAppend(" << dir << "): adding " << ierr
		  << " rows took "  << timer.CPUTime() << " CPU seconds and "
		  << timer.realTime() << " elapsed seconds";
    }
    const long napp = ierr;
    if (tbl->getState() != ibis::part::STABLE_STATE) {
	if (ibis::gVerbose >= 0) {// self test after append
	    int nth = static_cast<int>(ibis::gVerbose < 20
				       ? ibis::gVerbose * 0.25
				       : 3+log((double)ibis::gVerbose));
	    ierr = tbl->selfTest(nth);
	}
	else { // very quiet, skip self testing
	    ierr = 0;
	}
	if (ierr != 0) {
	    LOGGER(0) << "doAppend(" << dir << "): selfTest encountered "
		      << ierr << " error" << (ierr > 1 ? "s." : ".")
		      << " Will attempt to roll back the changes.";
	    ierr = tbl->rollback();
	    if (ierr <= 0)
		LOGGER(0) << "doAppend(" << dir << "): rollback returned with "
			  << ierr << "\n";
	    if (newtable)
		delete tbl;
	    return;
	}

	timer.start();
	ierr = tbl->commit(dir);
	timer.stop();
	if (ierr != napp) {
	    LOGGER(0) << "doAppend(" << dir
		      << "): expected commit command to return " << napp
		      << ", but it actually retruned " << ierr
		      << ".  Unrecoverable error!\n";
	}
	else {
	    LOGGER(0) << "doAppend(" << dir << "): committing " << napp
		      << " rows to partition \"" << tbl->name() << "\" took "
		      << timer.CPUTime() << " CPU seconds and "
		      << timer.realTime() << " elapsed seconds.  "
		"Total number of rows is " << tbl->nRows() << ".";
	}

	if (ierr <= 0) {
	    if (newtable) // new partition, delete it
		delete tbl;
	    return;
	}

	// self test after commit,
	if (ibis::gVerbose > 0) {
	    ierr = tbl->selfTest(0);
	    LOGGER(1) << "doAppend(" << dir << "): selfTest on partition \""
		      << tbl->name() << "\" (after committing " << napp
		      << (napp > 1 ? " rows" : " row")
		      << ") encountered " << ierr
		      << (ierr > 1 ? " errors\n" : " error\n");
	}
    }
    else {
	if (ibis::gVerbose > 0) {
	    ierr = tbl->selfTest(0);
	    LOGGER(1) << "doAppend(" << dir << "): selfTest on partition \""
		      << tbl->name() << "\" (after appending " << napp
		      << (napp > 1 ? " rows" : " row")
		      << ") encountered " << ierr
		      << (ierr > 1 ? " errors\n" : " error\n");
	}
    }
    if (newtable) // new partition, add it to the list of partitions
	tlist.push_back(tbl);
} // doAppend

static void readInts(const char* fname, std::vector<uint32_t> &ints) {
    std::ifstream sfile(fname);
    if (! sfile) {
	LOGGER(0) << "readInts unable to open file \"" << fname
		  << "\" for reading";
	return;
    }

    uint32_t tmp;
    while (sfile >> tmp) {
	ints.push_back(tmp);
    }
} // readInts

static void doDeletion(ibis::partList& tlist) {
    if (junkstring == 0 || *junkstring == 0) return;

    if (ibis::util::getFileSize(junkstring) > 0) {
	// assume the file contain a list of numbers that are row numbers
	std::vector<uint32_t> rows;
	readInts(junkstring, rows);
	if (rows.empty()) {
	    LOGGER(0) << "doDeletion -- file \"" << junkstring
		      << "\" does not start with integers, integer expected";
	    return;
	}
	LOGGER(1) << "doDeletion will invoke deactive on " << tlist.size()
		  << " data partition" << (tlist.size() > 1 ? "s" : "")
		  << " with " << rows.size() << " row number"
		  << (rows.size() > 1 ? "s" : "");

	for (ibis::partList::iterator it = tlist.begin();
	     it != tlist.end(); ++ it) {
	    long ierr = (*it)->deactivate(rows);
	    LOGGER(0) << "doDeletion -- deactivate(" << (*it)->name()
		      << ") returned " << ierr;
	    if (zapping) {
		ierr = (*it)->purgeInactive();
		if (ierr < 0) {
		    LOGGER(1) << "doDeletion purgeInactive(" << (*it)->name()
			      << ") returned " << ierr;
		}
	    }
	}
    }
    else {
	LOGGER(1) << "doDeletion will invoke deactive on " << tlist.size()
		  << " data partition" << (tlist.size() > 1 ? "s" : "")
		  << " with \"" << junkstring << "\"";

	for (ibis::partList::iterator it = tlist.begin();
	     it != tlist.end(); ++ it) {
	    long ierr = (*it)->deactivate(junkstring);
	    LOGGER(0) << "doDeletion -- deactivate(" << (*it)->name()
		      << ", " << junkstring << ") returned " << ierr;

	    if (zapping) {
		ierr = (*it)->purgeInactive();
		if (ibis::gVerbose > 0 || ierr < 0) {
		    LOGGER(0) << "doDeletion purgeInactive(" << (*it)->name()
			      << ") returned " << ierr;
		}
	    }
	}
    }
} // doDeletion

static void reverseDeletion(ibis::partList& tlist) {
    if (keepstring == 0 || *keepstring == 0) return;

    if (ibis::util::getFileSize(keepstring) > 0) {
	// assume the file contain a list of numbers that are row numbers
	std::vector<uint32_t> rows;
	readInts(keepstring, rows);
	if (rows.empty()) {
	    LOGGER(0) << "reverseDeletion -- file \"" << keepstring
		      << "\" does not start with integers, integer expected";
	    return;
	}
	LOGGER(1) << "reverseDeletion will invoke deactive on " << tlist.size()
		  << " data partition" << (tlist.size() > 1 ? "s" : "")
		  << " with " << rows.size() << " row number"
		  << (rows.size() > 1 ? "s" : "");

	for (ibis::partList::iterator it = tlist.begin();
	     it != tlist.end(); ++ it) {
	    long ierr = (*it)->reactivate(rows);
	    LOGGER(0) << "reverseDeletion -- reactivate(" << (*it)->name()
		      << ") returned " << ierr;
	}
    }
    else {
	LOGGER(1) << "reverseDeletion will invoke deactive on " << tlist.size()
		  << " data partition" << (tlist.size() > 1 ? "s" : "")
		  << " with \"" << keepstring << "\"";

	for (ibis::partList::iterator it = tlist.begin();
	     it != tlist.end(); ++ it) {
	    long ierr = (*it)->reactivate(keepstring);
	    LOGGER(0) << "reverseDeletion -- reactivate(" << (*it)->name()
		      << ", " << keepstring << ") returned " << ierr;
	}
    }
} // reverseDeletion

// parse the query string and evaluate the specified query
static void parseString(ibis::partList& tlist, const char* uid,
			const char* qstr) {
    if (qstr == 0) return;
    if (*qstr == 0) return;

    // got a valid string
    const char* str = qstr;
    const char* end;
    std::string sstr; // select clause
    std::string wstr; // where clause
    std::string ordkeys; // order by clause (the order keys)
    int direction = 0; // direction of the order by clause
    uint32_t limit = 0; // the limit on the number of output rows
    ibis::nameList qtables;

    // skip leading space
    while (isspace(*str)) ++str;
    // look for key word SELECT
    if (0 == strnicmp(str, "select ", 7)) {
	str += 7;
	while (isspace(*str)) ++str;
	// look for the next key word (either FROM or WHERE)
	end = strstr(str, " from ");
	if (end == 0) {
	    end = strstr(str, " FROM ");
	    if (end == 0)
		end = strstr(str, " From ");
	}
	if (end) { // found FROM clause
	    while (str < end) {
		sstr += *str;
		++ str;
	    }
	}
	else { // no FROM clause, try to locate WHERE
	    end = strstr(str, " where ");
	    if (end == 0) {
		end = strstr(str, " WHERE ");
		if (end == 0)
		    end = strstr(str, " Where ");
	    }
	    if (end == 0) {
		LOGGER(0) << "Unable to locate key word WHERE in " << qstr;
		return;
	    }
	    while (str < end) {
		sstr += *str;
		++ str;
	    }
	}
	str = end + 1;
    }

    // look for key word FROM
    if (0 == strnicmp(str, "from ", 5)) {
	str += 5;
	while (isspace(*str)) ++str;
	end = strstr(str, " where "); // look for key word WHERE
	if (end == 0) {
	    end = strstr(str, " WHERE ");
	    if (end == 0)
		end = strstr(str, " Where ");
	}
	if (end == 0) {
	    LOGGER(0) << "parseString(" << qstr << ") is unable to locate "
		      << "key word WHERE following FROM clause";
	    throw "unable to locate key word WHERE in query string";
	}
	char* fstr = new char[sizeof(char) * (end - str + 1)];
	(void) strncpy(fstr, str, end-str);
	fstr[end-str] = 0;
	qtables.select(fstr);
	delete [] fstr;
	str = end + 1;
    }

    // the WHERE clause must be present
    if (str == 0 || *str == 0) {
	LOGGER(0) << "Unable to fund a where clause in the query string \""
		  << qstr << "\"";
	return;
    }
    else if (0 == strnicmp(str, "where ", 6)) {
	str += 6;
    }
    else if (ibis::gVerbose > 1) {
	ibis::util::logger lg(2);
	lg() << "parseString(" << qstr
	     << ") is unable to locate key word WHERE.  "
	     << "assume the string is the where clause.";
    }
    // the end of the where clause is marked by the key words "order by" or
    // "limit" or the end of the string
    end = strstr(str, "order by");
    if (end == 0) {
	end = strstr(str, "Order by");
	if (end == 0)
	    end = strstr(str, "Order By");
	if (end == 0)
	    end = strstr(str, "ORDER BY");
	if (end == 0)
	    end = strstr(str, "limit");
	if (end == 0)
	    end = strstr(str, "Limit");
	if (end == 0)
	    end = strstr(str, "LIMIT");
    }
    if (end != 0) {
	while (str < end) {
	    wstr += *str;
	    ++ str;
	}
    }
    else {
	while (*str != 0) {
	    wstr += *str;
	    ++ str;
	}
    }

    if (0 == strnicmp(str, "order by ", 9)) { // order by clause
	// the order by clause may be terminated by key words "ASC", "DESC"
	// or "LIMIT"
	str += 9;
	end = strstr(str, "desc");
	if (end == 0) {
	    end = strstr(str, "Desc");
	    if (end == 0)
		end = strstr(str, "DESC");
	    if (end == 0)
		end = strstr(str, "asc");
	    if (end == 0)
		end = strstr(str, "Asc");
	    if (end == 0)
		end = strstr(str, "ASC");
	    if (end == 0)
		end = strstr(str, "limit");
	    if (end == 0)
		end = strstr(str, "Limit");
	    if (end == 0)
		end = strstr(str, "LIMIT");
	}
	if (end != 0) {
	    while (str < end) {
		ordkeys += *str;
		++ str;
	    }

	    if (0 == strnicmp(str, "desc ", 5)) {
		direction = -1;
		str += 5;
	    }
	    else if (0 == strnicmp(str, "asc ", 4)) {
		direction = 1;
		str += 4;
	    }
	}
	else {
	    while (*str) {
		ordkeys += *str;
		++ str;
	    }
	}
    }
    while (*str && isspace(*str)) // skip blank spaces
	++ str;
    if (0 == strnicmp(str, "limit ", 6)) {
	str += 6;
	double tmp = atof(str);
	if (tmp > 0.0)
	    limit = (uint32_t)tmp;
    }
    else if (str != 0 && *str != 0 && ibis::gVerbose >= 0) {
	ibis::util::logger lg(0);
	lg() << "Warning parseString(" << qstr
	     << ") expects the key word LIMIT, but got " << str;
    }

    // remove count(*) from select clause
    

    if (qtables.size()) {
	// go through each partition the user has specified and process the
	// queries
	for (size_t k = 0; k < tlist.size(); ++ k) {
	    for (size_t j = 0; j < qtables.size(); ++j) {
		if (stricmp(qtables[j], tlist[k]->name()) == 0 ||
		    ibis::util::strMatch(tlist[k]->name(), qtables[j])) {
		    if (sequential_scan ||
			tlist[k]->getMeshShape().empty())
			doQuery(uid, tlist[k], wstr.c_str(), sstr.c_str(),
				ordkeys.c_str(), direction, limit);
		    else
			doMeshQuery(uid, tlist[k], wstr.c_str(), sstr.c_str());

		    if (ibis::gVerbose > 10 || testing > 0)
			xdoQuery(uid, tlist[k], wstr.c_str(), sstr.c_str());
		    break;
		}
	    }
	}
    }
    else { // go through every partition and process the user query
	for (ibis::partList::iterator tit = tlist.begin();
	     tit != tlist.end(); ++tit) {
	    if (sequential_scan ||
		(*tit)->getMeshShape().empty())
		doQuery(uid, *tit, wstr.c_str(), sstr.c_str(),
			ordkeys.c_str(), direction, limit);
	    else
		doMeshQuery(uid, *tit, wstr.c_str(), sstr.c_str());

	    if (ibis::gVerbose > 10 || testing > 0)
		xdoQuery(uid, *tit, wstr.c_str(), sstr.c_str());
	}
    }
} // parseString

extern "C" void* thFun(void* arg) {
    thArg* myArg = (thArg*)arg; // recast the argument to the right type
    for (unsigned j = myArg->task(); j < myArg->qlist.size();
	 j = myArg->task()) {
	parseString(myArg->tlist, myArg->uid, myArg->qlist[j]);
    }
    return 0;
}

// read a line inputed from the user
static void readInput(std::string& str) {
    str.erase(); // empty the current content
    int wait = 0;
    char buf[MAX_LINE];
    do {
	std::cout << (wait ? "more > " : "ibis > ");
	std::flush(std::cout);

	if (0 == fgets(buf, MAX_LINE, stdin)) *buf = 0;
	// remove trailing space
	char* tmp = buf + strlen(buf) - 1;
	while (tmp>=buf && isspace(*tmp)) {
	    *tmp = 0; -- tmp;
	}

	if (tmp < buf) {
	    wait = 1;
	}
	else {
	    wait = 0;
	    if (*tmp == '\\') {
		int cnt = 0;
		char* t2 = tmp;
		while (t2 > buf && *t2 == '\\') {
		    --t2; ++cnt;
		}
		wait = (cnt % 2);
		if (wait) *tmp = ' ';
	    }
	    str += buf + strspn(buf, " \t");
	}
    } while (wait);
} // readInput

static void clean_up(ibis::partList& tlist, bool sane=true) {
    { // use envLock to make sure only one thread is deleting the partitions
	ibis::util::quietLock lock(&ibis::util::envLock);
	if (tlist.empty())
	    return;

	for (size_t j = 0; j < tlist.size(); ++ j) {
#if defined(_DEBUG) || defined(DEBUG)
	    LOGGER(5) << "clean_up -- deleting partition " << j
		      << ", " << tlist[j]->name() << " ("
		      << static_cast<const void*>(tlist[j]) << ")";
#endif
	    delete tlist[j];
	    tlist[j] = 0;
	}
	tlist.clear();
    }

#if defined(RUSAGE_SELF) && defined(RUSAGE_CHILDREN)
    if (ibis::gVerbose >= 2) {
	// getrusage might not fill all the fields
	struct rusage ruse0, ruse1;
	int ierr = getrusage(RUSAGE_SELF, &ruse0);
	ierr |= getrusage(RUSAGE_CHILDREN, &ruse1);
	if (ierr == 0) {
	    ibis::util::logger lg(2);
	    lg()
		<< "Report from getrusage: maxrss = "
		<< ruse0.ru_maxrss + ruse1.ru_maxrss
		<< " pages (" << getpagesize() << " bytes/page)"
		<< ", majflt = " << ruse0.ru_majflt + ruse1.ru_majflt
		<< ", minflt = " << ruse0.ru_minflt + ruse1.ru_minflt
		<< ", inblock = " << ruse0.ru_inblock + ruse1.ru_inblock
		<< ", outblock = " << ruse0.ru_oublock + ruse1.ru_oublock;
	}
    }
#endif
#if defined(_MSC_VER) && defined(_WIN32) && (defined(_DEBUG) || defined(DEBUG))
    std::cout << "\n*** DEBUG: report from _CrtMemDumpAllObjectsSince\n";
    _CrtMemDumpAllObjectsSince(NULL);
    _CrtDumpMemoryLeaks();
#endif

    // last thing -- close the file logging the messages
    ibis::util::closeLogFile();
} // clean_up

int main(int argc, char** argv) {
    // #if defined(_WIN32) && defined(_MSC_VER) && defined(_DEBUG)
    //     _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
    //     _CrtSetReportMode(_CRT_ASSERT | _CRT_ERROR | _CRT_WARN,
    // 		      _CRTDBG_MODE_FILE);
    //     _CrtSetReportFile(_CRT_ASSERT | _CRT_ERROR | _CRT_WARN,
    // 		      _CRTDBG_FILE_STDERR );
    // #endif
    if (argc <= 1) {
	usage(*argv);
	return 0;
    }

    ibis::partList tlist;
    try {
	int interactive;
	stringArray qlist;
	stringArray alist;
	std::vector<std::string> queff; // queries read from files (-f)
	const char* uid = ibis::util::userName();
	ibis::horometer timer; // total elapsed time
	timer.start();

	// parse the command line arguments
	parse_args(argc, argv, interactive, tlist, qlist, alist, queff);

	// add new data if any
	for (stringArray::const_iterator it = alist.begin();
	     it != alist.end();
	     ++ it) { // add new data before doing anything else
	    doAppend(*it, tlist);
	}
	alist.clear(); // no more use for it

	if (junkstring != 0 && *junkstring != 0)
	    doDeletion(tlist);
	if (keepstring != 0 && *keepstring != 0)
	    reverseDeletion(tlist);

	// build new indexes
	if (build_index > 0 && ! tlist.empty()) {
	    LOGGER(1) << *argv << ": start building indexes...";
	    ibis::horometer timer1;
	    timer1.start();
	    for (ibis::partList::const_iterator it = tlist.begin();
		 it != tlist.end(); ++ it) {
		if (zapping)
		    (*it)->purgeIndexFiles();
		if (indexingOption != 0)
		    (*it)->indexSpec(indexingOption);
		(*it)->buildIndexes(indexingOption, build_index);
	    }
	    timer1.stop();
	    LOGGER(0) << *argv << ": building indexes for " << tlist.size()
		      << " data partition"
		      << (tlist.size()>1 ? "s" : "") << " took "
		      << timer1.CPUTime() << " CPU seconds and "
		      << timer1.realTime() << " elapsed seconds\n";
	}

	// performing self test
	if (testing > 0 && ! tlist.empty()) {
	    LOGGER(1) << *argv << ": start testing ...";
	    ibis::horometer timer3;
	    timer3.start();
	    for (ibis::partList::const_iterator it = tlist.begin();
		 it != tlist.end(); ++ it) {
		// tell the partition to perform self tests
		long nerr = (*it)->selfTest(testing);
		(*it)->unloadIndexes();

		if (ibis::gVerbose >= 0) {
		    ibis::util::logger lg(0);
		    lg() << "self tests on " << (*it)->name();
		    if (nerr == 0)
			lg() << " found no error";
		    else if (nerr == 1)
			lg() << " found 1 error";
		    else if (nerr > 1)
			lg() << " found " << nerr << " errors";
		    else
			lg() << " returned unexpected value " << nerr;
		}
	    }
	    timer3.stop();
	    LOGGER(0) << *argv << ": testing " << tlist.size()
		      << " data partition"
		      << (tlist.size()>1 ? "s" : "") << " took "
		      << timer3.CPUTime() << " CPU seconds and "
		      << timer3.realTime() << " elapsed seconds\n";
	}


	if (tlist.empty() && !qlist.empty()) {
	    LOGGER(0) << *argv << " must have at least one data partition "
		"to process any query.";
	}
	else if (qlist.size() > 1 && threading > 0) {
#if defined(_DEBUG) || defined(DEBUG)
	    for (stringArray::const_iterator it = qlist.begin();
		 it != qlist.end(); ++it) {
		parseString(tlist, uid, *it);
	    }
#else
	    // process queries in a thread pool
	    const int nth =
		(threading < qlist.size() ? threading : qlist.size()-1);
	    ibis::util::counter taskpool;
	    thArg args(uid, qlist, tlist, taskpool);
	    std::vector<pthread_t> tid(nth);
	    for (int i =0; i < nth; ++ i) { // 
		int ierr = pthread_create(&(tid[i]), 0, thFun, (void*)&args);
		if (ierr != 0) {
		    LOGGER(0) << "pthread_create failed to create " << i
			      << "th thread";
		    return(-5);
		}
	    }
	    thFun((void*)&args); // this thread do something too
	    for (int i = 0; i < nth; ++ i) {
		int status;
		int ierr = pthread_join(tid[i], (void**)&status);
		if (ierr != 0) {
		    LOGGER(0) << "pthread_join failed on the " << i
			      << "th thread";
		}
	    }
#endif
	    queff.clear();
	    qlist.clear();
	}
	else if (qlist.size() > 0) { // no new threads
	    for (stringArray::const_iterator it = qlist.begin();
		 it != qlist.end(); ++it) {
		parseString(tlist, uid, *it);
	    }
	    queff.clear();
	    qlist.clear();
	}

	if (interactive) {	// iteractive operations
	    std::string str;
	    if (ibis::gVerbose >= 0) {
		// entering the interactive mode, print the help message
		std::cout << "\nEntering interactive mode\n";
		help(*argv);
	    }

	    while (1) {
		readInput(str);
		switch (*(str.c_str())) {
		case 'h': // help
		case 'H':
		case '?':
		default:
		    help(*argv);
		    break;
		case 'e': // exit
		case 'E':
		case 'q':
		case 'Q':
		    clean_up(tlist);
		    return(0);
		case 'p': // print command
		case 'P':
		    print(str.c_str(), tlist); break;
		case 's': // query must start with of the key words
		case 'f':
		case 'w':
		case 'S':
		case 'F':
		case 'W':
		    //std::cout << str << std::endl;
		    parseString(tlist, uid, str.c_str()); break;
		case 'a':
		case 'A': {
		    const char* dir = str.c_str();
		    while(isalpha(*dir)) ++dir; // skip key word append
		    while(isspace(*dir)) ++dir; // skip space
		    doAppend(dir, tlist);
		    break;}
		}
	    }
	}

	timer.stop();
	if (timer.realTime() > 0.001)
	    LOGGER(2) << *argv << ":: total CPU time " << timer.CPUTime()
		      << " s, total elapsed time " << timer.realTime() << " s";
	clean_up(tlist);
	return 0;
    }
    catch (const std::exception& e) {
	LOGGER(0) << "Warning ** " << *argv
		  << " received a standard exception\n" << e.what();
	//clean_up(tlist, false);
	return -10;
    }
    catch (const char* s) {
	LOGGER(0) << "Warning ** " << *argv
		  << " received a string exception\n" << s;
	//clean_up(tlist, false);
	return -11;
    }
    catch (...) {
	LOGGER(0) << "Warning ** " << *argv
		  << " received an unexpected exception";
	//clean_up(tlist, false);
	return -12;
    }
} // main
