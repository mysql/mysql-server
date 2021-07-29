/// $Id$
///
/// A program to generate sample data based on the Risk taxonomy used by
/// Justo Ruiz Ferrer <justo.ruizferrer@endelec.com>, 2010/12/02.  It
/// generates a dataset with six columns named: rowid, k1, k2, k3, jc, and
/// js, where the field jc and js are a concatenations of k1, k2 and k3
/// connected either with coma (jc) or space (js).  It also generates a
/// term-document matrix for jc based on the three risk fields, k1, k2 and
/// k3.
///
/// The random number generator is initialized with time, therefore will
/// generate different data, however, selections based on known keywords
/// should always return the same answer no matter whether is on k1, k2, k3
/// or jc or js.
///
/// Usage:
/// jrf <output-dir> [#rows] [#rows-per-dir] [conf-file]
///

#include <ibis.h>	// ibis name space, FastBit IBIS functions
#include <twister.h>	// ibis::discreteZipf1, ibis::MersenneTwister
#include <iostream>
#include <iomanip>
#include <memory>

/// The number of categories.
#define NCATEGORIES 20
/// Joining the three keys with space.
const char* JS[] = {
    "A     Strong                        Good       ",
    "A-    Strong                        Good       ",
    "A+    Strong                        Good       ",
    "AA    'Very Strong'                 Good       ",
    "AA-   'Very Strong'                 Good       ",
    "AA+   'Very Strong'                 Good       ",
    "AAA   'Extremely Strong'            Good       ",
    "B     'More Vulnerable'             'Not so good'",
    "B-    'More Vulnerable'             'Not so good'",
    "B+    'More Vulnerable'             'Not so good'",
    "BB    'Less Vulnerable'             'Not so good'",
    "BB-   'Less Vulnerable'             'Not so good'",
    "BB+   'Less Vulnerable'             'Not so good'",
    "BBB   Adequate                      FiftyFifty ",
    "BBB-  Adequate                      FiftyFifty ",
    "BBB+  Adequate                    	 FiftyFifty ",
    "C     'Currently Highly Vulnerable' 'Run Away'   ",
    "CC    'Currently Highly Vulnerable' 'Run Away'   ",
    "CCC   'Currently Vulnerable'        'Run Away'   ",
    "D     Failed                        'Run Away'   ",};
/// Joining the three keys with coma and space.
const char* JC[] = {
    "A  ,  Strong                     ,  Good       ",
    "A- ,  Strong                     ,  Good       ",
    "A+ ,  Strong                     ,  Good       ",
    "AA ,  Very Strong                ,  Good       ",
    "AA-,  Very Strong                ,  Good       ",
    "AA+,  Very Strong                ,  Good       ",
    "AAA,  Extremely Strong           ,  Good       ",
    "B  ,  More Vulnerable            ,  Not so good",
    "B- ,  More Vulnerable            ,  Not so good",
    "B+ ,  More Vulnerable            ,  Not so good",
    "BB ,  Less Vulnerable            ,  Not so good",
    "BB-,  Less Vulnerable            ,  Not so good",
    "BB+,  Less Vulnerable            ,  Not so good",
    "BBB,  Adequate                   ,  FiftyFifty ",
    "BBB-, Adequate                   ,  FiftyFifty ",
    "BBB+, Adequate                   ,	 FiftyFifty ",
    "C   , Currently Highly Vulnerable,  Run Away   ",
    "CC  , Currently Highly Vulnerable,  Run Away   ",
    "CCC , Currently Vulnerable       ,  Run Away   ",
    "D   , Failed                     ,  Run Away   ",};
/// The risk score.  Component 3 of the risk category.
const char* K3[] = {
    "A",
    "A-",
    "A+",
    "AA",
    "AA-",
    "AA+",
    "AAA",
    "B",
    "B-",
    "B+",
    "BB",
    "BB-",
    "BB+",
    "BBB",
    "BBB-",
    "BBB+",
    "C",
    "CC",
    "CCC",
    "D",};
/// The risk description.  Component 2 of the risk category.
const char* K2[] = {
    "Strong",
    "Strong",
    "Strong",
    "Very Strong",
    "Very Strong",
    "Very Strong",
    "Extremely Strong",
    "More Vulnerable",
    "More Vulnerable",
    "More Vulnerable",
    "Less Vulnerable",
    "Less Vulnerable",
    "Less Vulnerable",
    "Adequate",
    "Adequate",
    "Adequate",
    "Currently Highly Vulnerable",
    "Currently Highly Vulnerable",
    "Currently Vulnerable",
    "Failed",};
/// The risk level.  Component 1 of the risk category.
const char* K1[] = {
    "Good",
    "Good",
    "Good",
    "Good",
    "Good",
    "Good",
    "Good",
    "Not so good",
    "Not so good",
    "Not so good",
    "Not so good",
    "Not so good",
    "Not so good",
    "FiftyFifty",
    "FiftyFifty",
    "FiftyFifty",
    "Run Away",
    "Run Away",
    "Run Away",
    "Run Away",};

typedef std::map<const char*, std::vector<uint64_t>, ibis::lessi> TDList;

static void initColumns(ibis::tablex& tab, ibis::table::row& val) {
    tab.addColumn("rowid", ibis::UINT);
    tab.addColumn("k1", ibis::CATEGORY);
    tab.addColumn("k2", ibis::CATEGORY);
    tab.addColumn("k3", ibis::CATEGORY);
    tab.addColumn("jc", ibis::TEXT, "concatenated risk keys",
		  "keywords, delimiters=','");
    tab.addColumn("js", ibis::TEXT, "concatenated risk keys",
		  "keywords, delimiters=' ', docidname=rowid");

    val.clear();
    val.uintsnames.push_back("rowid");
    val.uintsvalues.resize(1);
    val.catsnames.push_back("k1");
    val.catsnames.push_back("k2");
    val.catsnames.push_back("k3");
    val.catsvalues.resize(3);
    val.textsnames.push_back("jc");
    val.textsnames.push_back("js");
    val.textsvalues.resize(2);
} // initColumns

static void fillRow(uint64_t seq, ibis::table::row& val, TDList& tdl) {
    static ibis::MersenneTwister mt;
    static ibis::discreteZipf1 zipf(mt, NCATEGORIES-1);
    const unsigned long ir = zipf();
    val.uintsvalues[0] = seq;
    val.textsvalues[0] = JC[ir];
    val.textsvalues[1] = JS[ir];
    val.catsvalues[0] = K1[ir];
    val.catsvalues[1] = K2[ir];
    val.catsvalues[2] = K3[ir];
    tdl[K1[ir]].push_back(seq);
    tdl[K2[ir]].push_back(seq);
    tdl[K3[ir]].push_back(seq);
} // fillRow

static void writeTDList(const TDList& tdl, const char* dir) {
    std::string fname(dir);
    fname += FASTBIT_DIRSEP;
    fname += "js.tdlist";
    std::ofstream tdf(fname.c_str(),
		      std::ios::out | std::ios::ate | std::ios::app);
    if (! tdf) {
	LOGGER(1)
	    << "Warning -- writeTDList failed to open " << fname
	    << " for appending the new Term-Document entries";
	return;
    }

    for (TDList::const_iterator it = tdl.begin(); it != tdl.end(); ++ it) {
	const std::vector<uint64_t>& ids = (*it).second;
	tdf << (*it).first << ": ";
	for (size_t j = 0; j < ids.size(); ++ j)
	    tdf << ' ' << ids[j];
	tdf << "\n";
    }
} // writeTDList

/// main.
int main(int argc, char** argv) {
    uint32_t maxrow=0, nrpd=0;
    int nparts, ndigits, ierr;

    // must have the output directory name
    if (argc < 2) {
	std::cerr << "\nUsage:\n" << *argv
		  << " <output-dir> [#rows [#rows-per-dir [conf-file]]]\n"
		  << "If the 4th argument is not provided, the number of "
	    "rows per directory will be determined by the memory cache size, "
	    "which is by default 1/2 of the physical memory size.\n"
		  << std::endl;
	return -1;
    }

    //ibis::gVerbose = 8;
    // initialize the file manage with the 5th argument
    ibis::init(argc>4 ? argv[4] : (const char*)0);
    ibis::util::timer mytimer(*argv, 0);
    if (argc > 2) // user specified maxrow
	maxrow = (uint32_t)atof(argv[2]);
    if (maxrow <= 0) {
	double tmp = ibis::fileManager::currentCacheSize();
	maxrow = (uint32_t)
	    ibis::util::compactValue(tmp / 120.0, tmp / 80.0);
	nrpd = maxrow;
    }
    if (maxrow < 10)
	maxrow = 10;
    if (argc > 3) // user specified nrpd
	nrpd = (uint32_t) atof(argv[3]);
    if (nrpd <= 0) {
	double tmp = ibis::fileManager::currentCacheSize();
	nrpd = (uint32_t)
	    ibis::util::compactValue(tmp / 120.0, tmp / 80.0);
    }
    if (nrpd > maxrow) nrpd = maxrow;

    ibis::table::row val;
    std::auto_ptr<ibis::tablex> tab(ibis::tablex::create());
    initColumns(*tab, val);
    ierr = tab->reserveBuffer(nrpd);
    if (ierr > 0 && (unsigned)ierr < nrpd)
	nrpd = ierr;
    LOGGER(1) << *argv << ' ' << argv[1] << ' ' << maxrow << ' ' << nrpd
	      << std::endl;
    nparts = maxrow / nrpd;
    nparts += (maxrow > nparts*nrpd);
    ierr = nparts;
    for (ndigits = 1, ierr >>= 4; ierr > 0; ierr >>= 4, ++ ndigits);
    for (uint32_t irow = 1; irow <= maxrow;) {
	const uint32_t end = irow - 1 + nrpd;
	TDList tdl;
	std::string dir = argv[1];
	if (nparts > 1) { // figure out the directory name
	    const char* str = strrchr(argv[1], FASTBIT_DIRSEP);
	    if (str != 0) {
		if (str[1] == 0) {
		    while (str-1 > argv[1]) {
			if (*(str-1) == FASTBIT_DIRSEP) break;
			else -- str;
		    }
		}
		else {
		    ++ str;
		}
	    }
	    std::ostringstream oss;
	    oss << FASTBIT_DIRSEP << (str ? str : "_") << std::hex
		<< std::setprecision(ndigits) << std::setw(ndigits)
		<< std::setfill('0') << irow / nrpd;
	    dir += oss.str();
	}

	for (; irow <= end; ++ irow) {
	    fillRow(irow, val, tdl);
	    ierr = tab->appendRow(val);
	    LOGGER(ierr != 6)
		<< "Warning -- " << *argv << " failed to append row " << irow
		<< " to the in-memory table, appendRow returned " << ierr;
	    LOGGER(irow % 100000 == 0) << " . " << irow;
	}
	LOGGER(1) << "\n";
	ierr = tab->write(dir.c_str());
	LOGGER(ierr < 0)
	    << "Warning -- " << *argv << " failed to write " << tab->mRows()
	    << " rows to " << dir << ", ibis::tablex::write returned " << ierr;
	writeTDList(tdl, dir.c_str());
	tab->clearData();
	tdl.clear();
    }
    return 0;
} // main
