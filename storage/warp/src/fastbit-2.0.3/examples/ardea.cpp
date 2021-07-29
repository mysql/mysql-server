// $Id$
// Author: John Wu <John.Wu at ACM.org> Lawrence Berkeley National Laboratory
// Copyright (c) 2007-2016 the Regents of the University of California

/// @defgroup FastBitExamples FastBit IBIS example programs.
/** @file ardea.cpp

This is a simple test program for functions defined in ibis::tablex.

The user may specify a set of records to be read by using a combination of
-m option (for meta data, i.e., column names and types) and -t or -r
options or specify a SQL dump file.  Option -t is used to specify the name
of a text/CSV file and option -r is used to specify a row of text/CSV data
on the command line.  Specify a SQL dump file with '-sqldump filename'.

The caller may further specify a number of queries to be run on the data
after they are written to disk.

If the directory specified in -d option (default to "tmp") contains data,
the new records will be appended.  When the names match, the records are
assumed to the same type (not checked).  When the names do not match, the
rows with missing values are padded with NULL values.  See @c
ibis::tablex::appendRow for more information about NULL values.

If the user does not specify any new data, it will write a built-in set of
data (91 rows and 8 columns) and then run 10 built-in queries with known
numbers of hits.

Here is a list of arguments.

- <code>-b break/delimiters-in-text-data</code> the delimiters to be
  expected in the input data, the default value is ", ".

- <code>-c conf-file</code> a configuration file for FastBit
- <code>-d data-dir</code> the output directory to write the data.

- <code>-h</code> print a brief message about usage.  Any unknown options
  will trigger this print function, which also terminates this program.

- <code>-M metadata-filename</code> name the metadata file that contains
  the name and type information.  The names and types can be either
  specified in the form of 'name:type' pairs or in the form of "-part.txt"
  files.  The 'name:type' string is parsed by the function
  ibis::tablex::parseNamesAndTypes.

- <code>-m name:type[name:type,..]</code> metadata, i.e., the names and
  types of the columns.  All specification of 'name:type' pairs are
  concatenated according to the order they appear on the command line.
  This order is used to match with the order of the columns in the in the
  text file to be processed.

- <code>-m max-rows-per-file</code> an upper bound on the number of rows
  in an input file, used to allocate internal read buffer.  This is an
  optional advisory parameter.

- <code>-k column-name dictionary-filename</code> supply an ASCII
  dictionary for the column of categorical values.  The ASCII dictionary
  contains a pair of "integer-code: string value" on each line.  Must
  provide two separate arguments to <code>-k</code>.

- <code>-n name-of-dataset</code> the name to be associated with the
  dataset.

- <code>-tag metatags</code> the name=value pairs associated with the data
  set.

- <code>-r a-row-in-ascii</code> give one row of input data.

- <code>-sqldump sqldump-filename</code> name of the SQL dump file to be
  read.
- <code>-select clause</code> a select clause to be used for test queries.
  There can only be one select clause.

- <code>-t text-filename</code> name of the text file to be read.

- <code>-where clause</code> a where clause to be used for test queries.
  All where clauses specified on the command line will be tried in turn.
  A query will be composed of the select clause and one of the where
  clauses.

@note This program uses standard unix functions to perform the read and
      write operations.  If your input data file is not using unix-style
      end-of-line character, then it is possible that this program will not
      process the end-of-line correctly.  If you see this program putting
      an entire line of text into one field, it is likely that you are
      experiencing the problem with the end-of-line characters.  Please
      convert the end-of-line to unix-type.

@note This file is named after Cattle Egret, whose Latin name is <A
    HREF="http://tinyurl.com/ded8yj">Ardea ibis</A>.

@ingroup FastBitExamples
*/
#if defined(_WIN32) && defined(_MSC_VER)
#pragma warning(disable:4786)   // some identifier longer than 256 characters
#endif
#include "ibis.h"       // FastBit IBIS public header file
#include <set>          // std::set
#include <memory>       // std::unique_ptr
#include <iomanip>      // std::setprecision

// local data types
typedef std::set< const char*, ibis::lessi > qList;
static std::vector<const char*> inputrows;
static std::vector<const char*> csvfiles;
static std::vector<const char*> sqlfiles;
static const char* metadatafile = 0;
static const char* indexing = 0;
static std::string namestypes;
static std::string metatags;
static std::vector<const char*> userdicts;
static int build_indexes = 0;
static unsigned xrepeats = 0;

// printout the usage string
static void usage(const char* name) {
    std::cout << "usage:\n" << name << " [-c conf-file] "
        "[-d directory-to-write-data] [-n name-of-dataset] "
        "[-r a-row-in-ASCII] [-t text-file-to-read] "
        "[-sqldump file-to-read] [-b break/delimiters-in-text-data]"
        "[-M metadata-file] [-m name:type[,name:type,...]] "
        "[-k column-name dictionary-filename] "
        "[-m max-rows-per-file] [-tag name-value-pair] [-p max-per-partition]"
        "[-select clause] [-where clause] [-v[=| ]verbose_level]\n\n"
        "Note:\n\tColumn name must start with an alphabet and can only "
        "contain alphanumeric values, and max-rows-per-file must start "
        "with a decimal digit\n"
        "\tThe option -k must be followed by a column name and a filename\n"
        "\tThis program only recognize the following column types:\n"
        "\tbyte, short, int, long, float, double, key, and text\n"
        "\tIt only checks the first character of the types.\n"
        "\tFor example, one can load the data in tests/test0.csv either "
        "one of the following command lines:\n"
        "\tardea -d somwhere1 -m a:i,b:i,c:i -t tests/test0.csv\n"
        "\tardea -d somwhere2 -m a:i -m b:f -m c:d -t tests/test0.csv\n"
              << std::endl;
} // usage

// // Adds a table defined in the named directory.
// static void addTables(ibis::tableList& tlist, const char* dir) {
//     ibis::table *tbl = ibis::table::create(dir);
//     if (tbl == 0) return;
//     if (tbl->nRows() != 0 && tbl->nColumns() != 0)
//      tlist.add(tbl);
//     delete tbl;
// } // addTables

// function to parse the command line arguments
// output arguments are:
// qcnd   -- list of query conditions, i.e., where clauses
// sel    -- select clause (only one of this, all query conditions share the
//           same select clause)
// outdir -- The output directory name, where to output the data read into
//           memory
// dsname -- the name of the data set just in case the directory does not
//           contain an existing dataset, if not specified, the directory
//           name will be used
// del    -- the delimiters used to parse the ASCII data, if not specified,
//           coma is assumed.  Blank spaces are always skipped except insie
//           quotes.
// nrpf   -- the number of rows to be stored in memory while performing the
//           reading operations.  When more than this many rows are read
//           into memory, both readCSV and readSQLDump will attempt to
//           write out the in-memory data records.  If the user does not
//           specify a value for this variable, the value will be set by
//           readCSV or readSQLDump internally based on available memory.
static void parse_args(int argc, char** argv, qList& qcnd, const char*& sel,
                       const char*& outdir, const char*& dsname,
                       const char*& del, int& nrpf, int& pmax) {
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
    sel = 0;
    nrpf = 0;
    pmax = 0;
    for (int i=1; i<argc; ++i) {
        if (*argv[i] == '-') { // normal arguments starting with -
            switch (argv[i][1]) {
            default:
            case 'h':
            case 'H':
                usage(*argv);
                exit(0);
            case 'b':
            case 'B': // break/delimiters or build indexes
                if (i+1 < argc && argv[i+1][0] != '-') {
                    ++ i;
                    del = argv[i];
                }
                else {
                    build_indexes = 1;
                }
                break;
            case 'c':
            case 'C': // conf or csv? default conf
                if (i+1 < argc) {
                    if (argv[i][2] == 's' || argv[i][2] == 'S')
                        csvfiles.push_back(argv[i+1]);
                    else
                        ibis::gParameters().read(argv[i+1]);
                    ++ i;
                }
                break;
            case 'd':
            case 'D':
            case 'o':
            case 'O':
                if (i+1 < argc) {
                    ++ i;
                    outdir = argv[i];
                }
                break;
            case 'i':
            case 'I':
                if (i+1 < argc && argv[i+1][0] != '-') {
                    ++ i;
                    indexing = argv[i];
                }
                else {
                    build_indexes = 1;
                }
                break;
            case 'k':
            case 'K': { // key (aka category) and dictionary
                if (i+2 < argc) {
                    userdicts.push_back(argv[i+1]);
                    userdicts.push_back(argv[i+2]);
                    i += 2;
                }
                else {
                    std::clog << *argv << " skipping option -k because it is "
                        "not followed by two-argument <columname, "
                        "dictfilename> pair";
                }
                break;}
            case 'm':
                if (i+1 < argc) {
                    ++ i;
                    if (isdigit(*argv[i])) {
                        int nn = (int)strtod(argv[i], 0);
                        if (nn > 1)
                            nrpf = nn;
                    }
                    else {
                        if (! namestypes.empty())
                            namestypes += ", ";
                        namestypes += argv[i];
                    }
                }
                break;
            case 'M':
                if (i+1 < argc) {
                    ++ i;
                    metadatafile = argv[i];
                }
                break;
            case 'n':
            case 'N':
                if (i+1 < argc) {
                    ++ i;
                    if (isdigit(*argv[i])) {
                        int nn = strtol(argv[i], 0, 0);
                        if (nn > nrpf)
                            nrpf = nn;
                    }
                    else {
                        dsname = argv[i];
                    }
                }
                break;
            case 'p':
            case 'P':
                if (i+1 < argc) {
                    ++ i;
                    int nn = (int)strtod(argv[i], 0);
                    if (nn > 1)
                        pmax = nn;
                }
                break;
            case 'r':
            case 'R':
                if (i+1 < argc) {
                    ++ i;
                    inputrows.push_back(argv[i]);
                }
                break;
            case 't':
            case 'T': // tag or text, default text
                if (i+1 < argc) {
                    if (argv[i][2] == 'a' || argv[i][2] == 'A') {
                        if (metatags.empty()) {
                            metatags = argv[i+1];
                        }
                        else {
                            metatags += ", ";
                            metatags += argv[i+1];
                        }
                    }
                    else {
                        csvfiles.push_back(argv[i+1]);
                    }
                    ++ i;
                }
                break;
            case 'q':
            case 'Q':
            case 'w':
            case 'W':
                if (i+1 < argc) {
                    ++ i;
                    qcnd.insert(argv[i]);
                }
                break;
            case 's':
            case 'S': // sql dump file or select clause
                if (i+1 < argc) {
                    ++ i;
                    if (argv[i][2] == 'e' || argv[i][2] == 'E') {
                        sel = argv[i];
                    }
                    else { // assume to be sql dump file
                        sqlfiles.push_back(argv[i]);
                    }
                }
                break;
            case 'v':
            case 'V': {
                char *ptr = strchr(argv[i], '=');
                if (ptr == 0) {
                    if (i+1 < argc) {
                        if (isdigit(*argv[i+1])) {
                            ibis::gVerbose += strtol(argv[i+1], 0, 0);
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
                    ibis::gVerbose += strtol(++ptr, 0, 0);
                }
                break;}
            case 'x':
            case 'X': { // repeat the user supplied data xrepeats times
                char *ptr = strchr(argv[i], '=');
                if (ptr == 0) {
                    if (i+1 < argc) {
                        if (isdigit(*argv[i+1])) {
                            xrepeats += strtol(argv[i+1], 0, 0);
                            i = i + 1;
                        }
                        else {
                            ++ xrepeats;
                        }
                    }
                    else {
                        ++ xrepeats;
                    }
                }
                else {
                    xrepeats += strtol(++ptr, 0, 0);
                }
                break;}
            } // switch (argv[i][1])
        } // normal arguments
        else { // assume to be a set of query conditioins
            qcnd.insert(argv[i]);
        }
    } // for (int i=1; ...)

    std::cout << argv[0] << " -v " << ibis::gVerbose;
    if (outdir != 0 && *outdir != 0)
        std::cout << " -d \"" << outdir << '"';
    else
        std::cout << "\n  Will not write data to disk";

    if (sqlfiles.size() > 0) {
        std::cout << "\n  Will attempt to parse sql dump file"
                  << (sqlfiles.size()>1?"s":"") << ":";
        for (size_t i = 0; i < sqlfiles.size(); ++ i)
            std::cout << "\n\t" << sqlfiles[i];
        std::cout << std::endl;
    }

    if (inputrows.size() > 0 || csvfiles.size() > 0) {
        std::cout << "\n  Will attempt to parse ";
        if (inputrows.size() > 0)
            std::cout << inputrows.size() << " row"
                      << (inputrows.size() > 1 ? "s" : "");
        if (csvfiles.size() > 0) {
            if (inputrows.size() > 0)
                std::cout <<  " and ";
            std::cout << csvfiles.size() << " CSV file"
                      << (csvfiles.size() > 1 ? "s" : "");
            for (size_t i = 0; i < csvfiles.size(); ++ i)
                std::cout << "\n\t" << csvfiles[i];
        }
        std::cout << "\n";
        if (!namestypes.empty()) {
            std::cout << " with the following column names and types\n\t"
                      << namestypes << "\n";
            if (metadatafile != 0)
                std::cout << "as well as those names and types from "
                          << metadatafile << "\n";
        }
        else if (metadatafile != 0) {
            std::cout << " with names and types from " << metadatafile << "\n";
        }
        else {
            std::clog
                << "\n" << *argv << " can not parse the specified data "
                "without metadata, use -m name:type[,name:type] or "
                "-M metadatafilename to specify the column names and types\n";
        }
        std::cout << std::endl;
    }

    if (qcnd.size() > 0) {
        std::cout << "  Will exercise the following queries: ";
        for (qList::const_iterator it = qcnd.begin(); it != qcnd.end(); ++it)
            std::cout << "\t" << *it << "\n";
    }
    std::cout << std::endl;
} // parse_args

static void clearBuffers(const ibis::table::typeArray& tps,
                         std::vector<void*>& buffers) {
    const size_t nc = (tps.size() <= buffers.size() ?
                       tps.size() : buffers.size());
    for (size_t j = 0; j < nc; ++ j) {
        switch (tps[j]) {
        case ibis::BYTE: {
            signed char* tmp = static_cast<signed char*>(buffers[j]);
            delete [] tmp;
            break;}
        case ibis::UBYTE: {
            unsigned char* tmp = static_cast<unsigned char*>(buffers[j]);
            delete [] tmp;
            break;}
        case ibis::SHORT: {
            int16_t* tmp = static_cast<int16_t*>(buffers[j]);
            delete [] tmp;
            break;}
        case ibis::USHORT: {
            uint16_t* tmp = static_cast<uint16_t*>(buffers[j]);
            delete [] tmp;
            break;}
        case ibis::INT: {
            int32_t* tmp = static_cast<int32_t*>(buffers[j]);
            delete [] tmp;
            break;}
        case ibis::UINT: {
            uint32_t* tmp = static_cast<uint32_t*>(buffers[j]);
            delete [] tmp;
            break;}
        case ibis::LONG: {
            int64_t* tmp = static_cast<int64_t*>(buffers[j]);
            delete [] tmp;
            break;}
        case ibis::ULONG: {
            uint64_t* tmp = static_cast<uint64_t*>(buffers[j]);
            delete [] tmp;
            break;}
        case ibis::FLOAT: {
            float* tmp = static_cast<float*>(buffers[j]);
            delete [] tmp;
            break;}
        case ibis::DOUBLE: {
            double* tmp = static_cast<double*>(buffers[j]);
            delete [] tmp;
            break;}
        case ibis::TEXT:
        case ibis::CATEGORY: {
                std::vector<std::string>* tmp =
                    static_cast<std::vector<std::string>*>(buffers[j]);
                delete tmp;
                break;}
        default: {
            LOGGER(ibis::gVerbose > 0)
                << "clearBuffers unable to type " << tps[j];
            break;}
        }
    }
} // clearBuffers

static void dumpIth(size_t i, ibis::TYPE_T t, void* buf) {
    switch (t) {
    case ibis::BYTE: {
        const signed char* tmp = static_cast<const signed char*>(buf);
        std::cout << (int)tmp[i];
        break;}
    case ibis::UBYTE: {
        const unsigned char* tmp = static_cast<const unsigned char*>(buf);
        std::cout << (unsigned)tmp[i];
        break;}
    case ibis::SHORT: {
        const int16_t* tmp = static_cast<const int16_t*>(buf);
        std::cout << tmp[i];
        break;}
    case ibis::USHORT: {
        const uint16_t* tmp = static_cast<const uint16_t*>(buf);
        std::cout << tmp[i];
        break;}
    case ibis::INT: {
        const int32_t* tmp = static_cast<const int32_t*>(buf);
        std::cout << tmp[i];
        break;}
    case ibis::UINT: {
        const uint32_t* tmp = static_cast<const uint32_t*>(buf);
        std::cout << tmp[i];
        break;}
    case ibis::LONG: {
        const int64_t* tmp = static_cast<const int64_t*>(buf);
        std::cout << tmp[i];
        break;}
    case ibis::ULONG: {
        const uint64_t* tmp = static_cast<const uint64_t*>(buf);
        std::cout << tmp[i];
        break;}
    case ibis::FLOAT: {
        const float* tmp = static_cast<const float*>(buf);
        std::cout << std::setprecision(8) << tmp[i];
        break;}
    case ibis::DOUBLE: {
        const double* tmp = static_cast<const double*>(buf);
        std::cout << std::setprecision(18) << tmp[i];
        break;}
    case ibis::TEXT:
    case ibis::CATEGORY: {
        const std::vector<std::string>* tmp =
            static_cast<const std::vector<std::string>*>(buf);
        std::cout << '"' << (*tmp)[i] << '"';
        break;}
    default: {
        LOGGER(ibis::gVerbose > 0)
            << "dumpIth -- unable to process type " << t;
        break;}
    }
} // dumpIth

// Print the first few rows of a table.  This is meant as an example that
// attempts to read all records into memory.  It is likely faster than
// funtion printValues, but it may be more likely to run out of memory.
static int printValues1(const ibis::table& tbl) {
    if (ibis::gVerbose < 0) return 0;

    const size_t nr = static_cast<size_t>(tbl.nRows());
    if (nr != tbl.nRows()) {
        std::cout << "printValues is unlikely to be able to do it job "
            "because the number of rows (" << tbl.nRows()
                  << ") is too large for it read all records into memory"
                  << std::endl;
        return -1;
    }

    ibis::table::stringArray nms = tbl.columnNames();
    ibis::table::typeArray tps = tbl.columnTypes();
    std::vector<void*> buffers(nms.size(), 0);
    for (size_t i = 0; i < nms.size(); ++ i) {
        switch (tps[i]) {
        case ibis::BYTE: {
            char* buf = new char[nr];
            if (buf == 0) { // run out of memory
                clearBuffers(tps, buffers);
                return -1;
            }
            int64_t ierr = tbl.getColumnAsBytes(nms[i], buf);
            if (ierr < 0 || ((size_t) ierr) < nr) {
                clearBuffers(tps, buffers);
                return -2;
            }
            buffers[i] = buf;
            break;}
        case ibis::UBYTE: {
            unsigned char* buf = new unsigned char[nr];
            if (buf == 0) { // run out of memory
                clearBuffers(tps, buffers);
                return -1;
            }
            int64_t ierr = tbl.getColumnAsUBytes(nms[i], buf);
            if (ierr < 0 || ((size_t) ierr) < nr) {
                clearBuffers(tps, buffers);
                return -2;
            }
            buffers[i] = buf;
            break;}
        case ibis::SHORT: {
            int16_t* buf = new int16_t[nr];
            if (buf == 0) { // run out of memory
                clearBuffers(tps, buffers);
                return -1;
            }
            int64_t ierr = tbl.getColumnAsShorts(nms[i], buf);
            if (ierr < 0 || ((size_t) ierr) < nr) {
                clearBuffers(tps, buffers);
                return -2;
            }
            buffers[i] = buf;
            break;}
        case ibis::USHORT: {
            uint16_t* buf = new uint16_t[nr];
            if (buf == 0) { // run out of memory
                clearBuffers(tps, buffers);
                return -1;
            }
            int64_t ierr = tbl.getColumnAsUShorts(nms[i], buf);
            if (ierr < 0 || ((size_t) ierr) < nr) {
                clearBuffers(tps, buffers);
                return -2;
            }
            buffers[i] = buf;
            break;}
        case ibis::INT: {
            int32_t* buf = new int32_t[nr];
            if (buf == 0) { // run out of memory
                clearBuffers(tps, buffers);
                return -1;
            }
            int64_t ierr = tbl.getColumnAsInts(nms[i], buf);
            if (ierr < 0 || ((size_t) ierr) < nr) {
                clearBuffers(tps, buffers);
                return -2;
            }
            buffers[i] = buf;
            break;}
        case ibis::UINT: {
            uint32_t* buf = new uint32_t[nr];
            if (buf == 0) { // run out of memory
                clearBuffers(tps, buffers);
                return -1;
            }
            int64_t ierr = tbl.getColumnAsUInts(nms[i], buf);
            if (ierr < 0 || ((size_t) ierr) < nr) {
                clearBuffers(tps, buffers);
                return -2;
            }
            buffers[i] = buf;
            break;}
        case ibis::LONG: {
            int64_t* buf = new int64_t[nr];
            if (buf == 0) { // run out of memory
                clearBuffers(tps, buffers);
                return -1;
            }
            int64_t ierr = tbl.getColumnAsLongs(nms[i], buf);
            if (ierr < 0 || ((size_t) ierr) < nr) {
                clearBuffers(tps, buffers);
                return -2;
            }
            buffers[i] = buf;
            break;}
        case ibis::ULONG: {
            uint64_t* buf = new uint64_t[nr];
            if (buf == 0) { // run out of memory
                clearBuffers(tps, buffers);
                return -1;
            }
            int64_t ierr = tbl.getColumnAsULongs(nms[i], buf);
            if (ierr < 0 || ((size_t) ierr) < nr) {
                clearBuffers(tps, buffers);
                return -2;
            }
            buffers[i] = buf;
            break;}
        case ibis::FLOAT: {
            float* buf = new float[nr];
            if (buf == 0) { // run out of memory
                clearBuffers(tps, buffers);
                return -1;
            }
            int64_t ierr = tbl.getColumnAsFloats(nms[i], buf);
            if (ierr < 0 || ((size_t) ierr) < nr) {
                clearBuffers(tps, buffers);
                return -2;
            }
            buffers[i] = buf;
            break;}
        case ibis::DOUBLE: {
            double* buf = new double[nr];
            if (buf == 0) { // run out of memory
                clearBuffers(tps, buffers);
                return -1;
            }
            int64_t ierr = tbl.getColumnAsDoubles(nms[i], buf);
            if (ierr < 0 || ((size_t) ierr) < nr) {
                clearBuffers(tps, buffers);
                return -2;
            }
            buffers[i] = buf;
            break;}
        case ibis::TEXT:
        case ibis::CATEGORY: {
            std::vector<std::string>* buf = new std::vector<std::string>();
            if (buf == 0) { // run out of memory
                clearBuffers(tps, buffers);
                return -1;
            }
            int64_t ierr = tbl.getColumnAsStrings(nms[i], *buf);
            if (ierr < 0 || ((size_t) ierr) < nr) {
                clearBuffers(tps, buffers);
                return -2;
            }
            buffers[i] = buf;
            break;}
        default:
            LOGGER(ibis::gVerbose > 0)
                << "printValues1(" << tbl.name() << ") -- unable to handle "
                << "column " << nms[i] << " of type " << tps[i];
            break;
        }
    }
    if (nms.size() != tbl.nColumns() || nms.size() == 0) return -3;

    size_t nprt = 10;
    if (ibis::gVerbose > 30) {
        nprt = nr;
    }
    else if ((1U << ibis::gVerbose) > nprt) {
        nprt = (1U << ibis::gVerbose);
    }
    if (nprt > nr)
        nprt = nr;
    for (size_t i = 0; i < nprt; ++ i) {
        dumpIth(i, tps[0], buffers[0]);
        for (size_t j = 1; j < nms.size(); ++ j) {
            std::cout << ", ";
            dumpIth(i, tps[j], buffers[j]);
        }
        std::cout << "\n";
    }
    clearBuffers(tps, buffers);

    if (nprt < nr)
        std::cout << "-- " << (nr - nprt) << " skipped...\n";
    //std::cout << std::endl;
    return 0;
} // printValues1

// This version uses ibis::cursor to print the first few rows.  It is
// likely to be slower than printValues1, but is likely to use less memory
// and less prone to failure.
static int printValues2(const ibis::table& tbl) {
    ibis::table::cursor *cur = tbl.createCursor();
    if (cur == 0) return -1;
    uint64_t nr = tbl.nRows();
    size_t nprt = 10;
    if (ibis::gVerbose > 30) {
        nprt = static_cast<size_t>(nr);
    }
    else if ((1U << ibis::gVerbose) > nprt) {
        nprt = (1U << ibis::gVerbose);
    }
    if (nprt > nr)
        nprt = static_cast<size_t>(nr);
    int ierr = 0;
    for (size_t i = 0; i < nprt; ++ i) {
        ierr = cur->fetch(); // make the next row ready
        if (ierr == 0) {
            cur->dump(std::cout, ", ");
        }
        else {
            std::cout << "printValues2 failed to fetch row " << i << std::endl;
            ierr = -2;
            nprt = i;
            break;
        }
    }
    delete cur; // clean up the cursor

    if (nprt < nr)
        std::cout << "-- " << (nr - nprt) << " skipped...\n";
    //std::cout << std::endl;
    return ierr;
} // printValues2

static void printValues(const ibis::table& tbl) {
    if (tbl.nColumns() == 0 || tbl.nRows() == 0) return;
    int ierr = printValues1(tbl); // this version may be faster
    if (ierr < 0) { // try to the slower version
        ierr = printValues2(tbl);
        if (ierr < 0)
            std::cout << "printValues failed with error code " << ierr
                      << std::endl;
    }
} // printValues

// evaluate a single query, print out the number of hits
static void doQuery(const ibis::table& tbl, const char* wstr,
                    const char* sstr) {
    if (wstr == 0 || *wstr == 0) return;

    uint64_t n0, n1;
    if (ibis::gVerbose > 0) {
        tbl.estimate(wstr, n0, n1);
        std::cout << "doQuery(" << wstr
                  << ") -- the estimated number of hits on "
                  << tbl.name() << " is ";
        if (n1 > n0)
            std::cout << "between " << n0 << " and " << n1 << "\n";
        else
            std::cout << n1 << "\n";
        if (n1 == 0U) return;
    }

    // function select returns a table containing the selected values
    ibis::table *selected = tbl.select(sstr, wstr);
    if (selected == 0) {
        std::cout << "doQuery(" << wstr << ") failed to produce any result"
                  << std::endl;
        return;
    }

    n0 = selected->nRows();
    n1 = tbl.nRows();
    std::cout << "doQuery(" << wstr << ") evaluated on " << tbl.name()
              << " produced " << n0 << " hit" << (n0>1 ? "s" : "")
              << " out of " << n1 << " record" << (n1>1 ? "s" : "")
              << "\n";
    if (ibis::gVerbose > 0) {
        std::cout << "-- begin printing the table of results --\n";
        selected->describe(std::cout); // ask the table to describe itself

        if (ibis::gVerbose > 0 && n0 > 0 && selected->nColumns() > 0)
            printValues(*selected);
        std::cout << "-- end  printing the table of results --\n";
    }
    std::cout << std::endl;
    delete selected;
} // doQuery

int main(int argc, char** argv) {
    ibis::util::timer mytimer(*argv, 0);
    const char* outdir = ""; // default to keep data in memory
    const char* sel;
    const char* dsn = 0;
    const char* del = ", "; // delimiters
    int ierr, nrpf, pmax;
    qList qcnd;
    std::ostringstream oss;
    for (int i = 0; i < argc; ++ i) {
        if (i > 0) oss << " ";
        oss << argv[i];
    }

    ibis::init();
    parse_args(argc, argv, qcnd, sel, outdir, dsn, del, nrpf, pmax);
    const bool usersupplied = (! sqlfiles.empty()) ||
        ((! namestypes.empty() || metadatafile != 0) &&
         (! csvfiles.empty() || ! inputrows.empty()));
    // create a new table that does not support querying
    std::unique_ptr<ibis::tablex> ta(ibis::tablex::create());
    ta->setPartitionMax(pmax);
    if (usersupplied) { // use user-supplied data
        // process the SQL dump files first just in case the CSV files
        // require the metadata from them
        for (size_t i = 0; i < sqlfiles.size(); ++ i) {
            if (ibis::gVerbose >= 0)
                std::cout << *argv << " is to read SQL dump file "
                          << sqlfiles[i] << " ..." << std::endl;
            std::string tname;
            ierr = ta->readSQLDump(sqlfiles[i], tname, nrpf, outdir);
            if (ierr < 0) {
                std::clog << *argv << " failed to process file \""
                          << sqlfiles[i] << "\", readSQLDump returned "
                          << ierr << std::endl;
            }
            else if (outdir != 0 && *outdir != 0) {
                if (ibis::gVerbose >= 0)
                    std::cout << *argv << " read " << ierr << " row"
                              << (ierr>1?"s":"") << " from " << csvfiles[i]
                              << std::endl;

                ierr = ta->write(outdir, (tname.empty()?dsn:tname.c_str()),
                                 oss.str().c_str(), indexing, metatags.c_str());
                if (ierr < 0) {
                    std::clog
                        << *argv << " failed to write content of SQL "
                        "dump file " << sqlfiles[i] << " to \"" << outdir
                        << "\", error code = " << ierr << std::endl;
                    return(ierr);
                }
                ta->clearData();
                if (build_indexes > 0) { // build indexes
                    std::unique_ptr<ibis::table>
                        tbl(ibis::table::create(outdir));
                    if (tbl.get() != 0)
                        tbl->buildIndexes(0);
                }
            }
        }

        // process the metadata explicitly entered
        if (! namestypes.empty())
            ta->parseNamesAndTypes(namestypes.c_str());
        if (metadatafile != 0)
            ta->readNamesAndTypes(metadatafile);
        for (unsigned j = 0; j+1 < userdicts.size(); j += 2) {
            ta->setASCIIDictionary(userdicts[j], userdicts[j+1]);
        }

        // process the CSV files
        for (size_t i = 0; i < csvfiles.size(); ++ i) {
            if (ibis::gVerbose >= 0)
                std::cout << *argv << " is to read CSV file " << csvfiles[i]
                          << " ..." << std::endl;
            ierr = ta->readCSV(csvfiles[i], nrpf, outdir, del);
            if (ierr < 0)
                std::clog << *argv << " failed to parse file \""
                          << csvfiles[i] << "\", readCSV returned "
                          << ierr << std::endl;
            else if (outdir != 0 && *outdir != 0) {
                if (ibis::gVerbose >= 0)
                    std::cout << *argv << " read " << ierr << " row"
                              << (ierr>1?"s":"") << " from " << csvfiles[i]
                              << std::endl;

                ierr = ta->write(outdir, dsn, oss.str().c_str(), indexing,
                                 metatags.c_str());
                if (ierr < 0) {
                    std::clog << *argv << " failed to write data in CSV file "
                              << csvfiles[i] << " to \"" << outdir
                              << "\", error code = " << ierr << std::endl;
                    return ierr;
                }
                else if (xrepeats > 0) { // repeat xrepeats times
                    for (unsigned j = 1; j < xrepeats; ++ j)
                        (void) ta->write(outdir, dsn, oss.str().c_str(),
                                         indexing, metatags.c_str());
                }
                ta->clearData();
                if (build_indexes > 0) { // build indexes
                    std::unique_ptr<ibis::table>
                        tbl(ibis::table::create(outdir));
                    if (tbl.get() != 0)
                        tbl->buildIndexes(0);
                }
            }
        }
        for (size_t i = 0; i < inputrows.size(); ++ i) {
            ierr = ta->appendRow(inputrows[i], del);
            if (ierr < 0)
                std::clog << *argv
                          << " failed to parse text (appendRow returned "
                          << ierr << ")\n" << inputrows[i] << std::endl;
        }
        if (outdir != 0 && *outdir != 0 && ta->mColumns() > 0) {
            if (ta->mRows() > 0)
                ierr = ta->write(outdir, dsn, oss.str().c_str(), indexing,
                                 metatags.c_str());
            else
                ierr = ta->writeMetaData(outdir, dsn, oss.str().c_str(),
                                         indexing, metatags.c_str());
            if (ierr < 0) {
                std::clog << *argv << " failed to write user-supplied data to "
                          << outdir << ", error code = " << ierr << std::endl;
                return(ierr);
            }
            else if (ta->mRows() > 0 && xrepeats > 0) { // repeat xrepeats times
                for (unsigned j = 1; j < xrepeats; ++ j)
                    (void) ta->write(outdir, dsn, oss.str().c_str(),
                                     indexing, metatags.c_str());
            }
        }
    }
    else { // use hard-coded data and queries
        int64_t buf[] = {10, -21, 32, -43, 54, -65, 76, -87, 98, -127};
        if (ibis::gVerbose >= 0)
            std::cout << *argv << " to use hard-coded test data ..."
                      << std::endl;

        ta->addColumn("s1", ibis::SHORT);
        ta->addColumn("i2", ibis::INT);
        ta->addColumn("b3", ibis::BYTE);
        ta->addColumn("l4", ibis::LONG);
        ta->addColumn("f5", ibis::FLOAT);
        ta->addColumn("d6", ibis::DOUBLE);
        ta->addColumn("k7", ibis::CATEGORY);
        ta->addColumn("t8", ibis::TEXT);
        ta->appendRow("1,2,3,4,5,6,7,8");
        ta->appendRow("2 3 4 5 6 7 8 9");
        ta->append("l4", 2, 5, buf);
        ta->append("s1", 3, 10, buf+2);
        ta->append("i2", 4, 10, buf+3);
        ta->append("b3", 10, 90, buf);
        ta->appendRow("10,11,12,13,14,15,16");

        if (ta->mRows() > 0 && outdir != 0 && *outdir != 0) {
            ierr = ta->write(outdir, dsn,
                             "hard-coded test data written by ardea.cpp");
            if (ierr < 0) {
                std::clog << "Warning -- " << *argv
                          << " failed to write data to " << outdir
                          << ", error code = " << ierr << std::endl;
                return(ierr);
            }
        }
    }

    std::unique_ptr<ibis::table> tb(outdir != 0 && *outdir != 0 ?
                                    ibis::table::create(outdir) :
                                    ta->toTable());
    delete ta.release(); // no long need the tablex object
    if (tb.get() == 0) {
        std::cerr << "Warning -- " << *argv
                  << " failed to constructure a table from";
        if (outdir != 0 && *outdir != 0)
            std::cerr << " data files in " << outdir << std::endl;
        else
            std::cerr << " data in memory" << std::endl;
        return -10;
    }
    else if (! usersupplied && (tb->nRows() == 0 || tb->nColumns() != 8 ||
                                tb->nRows() % 91 != 0)) {
        std::cerr << "Warning -- " << *argv << " data in "
                  << (outdir!=0&&*outdir!=0 ? outdir : "memory")
                  << " is expected to have 8 columns and a multiple of 91 "
            "rows, but it does not"
                  << std::endl;
    }
    if (ibis::gVerbose > 0) {
        // use a logger object to hold the print out in memory to avoid it
        // be interrupted by other log messages
        ibis::util::logger lg;
        lg() << "-- begin printing table in "
             << (outdir!=0&&*outdir!=0 ? outdir : "memory") << " --\n";
        tb->describe(lg());
        if (tb->nRows() > 0 && tb->nColumns() > 0) {
            uint64_t nprint;
            if (ibis::gVerbose > 30) {
                nprint = tb->nRows();
            }
            else {
                nprint = (1 << ibis::gVerbose);
                if (nprint < 10)
                    nprint = 10;
            }
            tb->dump(lg(), nprint);
        }
        lg() << "--  end  printing table in "
             << (outdir!=0&&*outdir!=0 ? outdir : "memory") << " --\n";
    }
    if (usersupplied == false && qcnd.empty()) {
        // check the number of hits of built-in queries
        const char* arq[] = {"s1=1", "i2<=3", "l4<4",
                             "b3 between 10 and 100", "b3 > 0 && i2 < 0",
                             "\"8\" == k7 or \"8\" == t8", "1+f5 == d6",
                             "s1 between 0 and 10 and i2 between 0 and 10",
                             "t8=a && l4 > 8", "sqrt(d6)+log(f5)<5 && b3 <0"};
        const size_t arc[] = {1, 7, 1, 6, 0, 2, 3, 2, 0, 0};
        const size_t multi = static_cast<size_t>(tb->nRows() / 91);
        ierr = 0;
        for (size_t i = 0; i < 10; ++ i) {
            ibis::table* res = tb->select(static_cast<const char*>(0), arq[i]);
            if (res == 0) {
                std::clog << "Warning -- " << "Query \"" << arq[i] << "\" on "
                          << tb->name() << " produced a null table"
                          << std::endl;
                ++ ierr;
            }
            else if (res->nRows() != multi*arc[i]) {
                std::clog << "Warning -- " << "Query \"" << arq[i]
                          << "\" is expected to produce "
                          << multi*arc[i] << " hit" << (multi*arc[i]>1?"s":"")
                          << ", but actually found " << res->nRows()
                          << std::endl;
                ++ ierr;
            }
            else if (ibis::gVerbose > 0) {
                std::cout << "Query \"" << arq[i]
                          << "\" produced the expected number of hits ("
                          << res->nRows() << ")" << std::endl;
            }
            delete res;
        }
        if (ierr > 0)
            std::cout << "Warning -- ";
        std::cout << *argv << " processed 10 hard-coded queries on " << multi
                  << " cop" << (multi > 1 ? "ies" : "y")
                  << " of hard-coded test data, found " << ierr
                  << " unexpected result" << (ierr > 1 ? "s" : "")
                  << std::endl;
    }
    // user-supplied queries
    for (qList::const_iterator qit = qcnd.begin();
         qit != qcnd.end(); ++ qit) {
        doQuery(*tb, *qit, sel);
    }
    return 0;
} // main
