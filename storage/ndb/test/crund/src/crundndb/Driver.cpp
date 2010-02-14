/*
 * Driver.cpp
 *
 */

#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <vector>
#include <cassert>
#include <ctime>
#include <set>

#include "helpers.hpp"
#include "string_helpers.hpp"
#include "Properties.hpp"
#include "hrt_utils.h"
#include "Operations.hpp"

namespace crund_ndb {

//using namespace std;
using std::string;
using std::wstring;
using std::vector;
using std::ofstream;
using std::ostringstream;
using std::set;

using utils::Properties;

class Driver {
    
public:
    /**
     * Creates a Driver instance.
     */
    Driver();
    
    /**
     * Parses the benchmark's command-line arguments.
     */
    void parseArguments(int argc, const char* argv[]);
    
    /**
     * Runs the entire benchmark.
     */
    void run();

protected:
    // command-line arguments
    vector< string > propFileNames;
    string logFileName;

    // the data output stream
    ofstream log;

    // benchmark settings
    Properties props;
    string descr;
    bool logRealTime;
    bool logCpuTime;
    bool logSumOfOps;
    bool renewConnection;
    bool renewOperations;
    unsigned int aStart, aEnd, aIncr;
    unsigned int bStart, bEnd, bIncr;
    unsigned int maxStringLength;
    unsigned int warmupRuns;
    unsigned int hotRuns;
    set< string > exclude;

    // the NDB database connection
    string mgmdConnect;
    string catalog;
    string schema;

    // the benchmark's basic database operations.
    static crund_ndb::Operations* ops;

    /**
     * A database operation to be benchmarked.
     */
    struct Op {
        const string name;
        
        virtual void run(int countA, int countB) const = 0;
        //void (*run)(int countA, int countB);

        Op(const string& name)
            : name(name) {
        }

        virtual ~Op() {
        }
    };

    /**
     * The list of database operations to be benchmarked.
     * Managed by methods initOperations() and closeOperations().
     */
    typedef vector< const Op* > Operations;
    Operations operations;

private:
    // buffers collecting the header and data lines written to log
    bool logHeader;
    ostringstream header;
    ostringstream rtimes;
    ostringstream ctimes;

    // benchmark data fields
    int s0, s1;
    hrt_tstamp t0, t1;
    long rta, cta;

protected:

    /**
     * Loads the benchmark's properties from properties files.
     */
    void loadProperties();

    /**
     * Reads and initializes the benchmark's properties from a file.
     */
    void initProperties();
    
    /**
     * Prints the benchmark's properties.
     */
    void printProperties();

    /**
     * Initializes the benchmark's resources.
     */
    void init();

    /**
     * Releases the benchmark's resources.
     */
    void close();

    /**
     * Prints a command-line usage message and exits.
     */
    void exitUsage();

    /**
     * Runs a series of benchmark operations on scaled-up data.
     */
    void runTests();

    /**
     * Runs a series of benchmark operations.
     */
    void runOperations(int countA, int countB);
    
    /**
     * Runs a benchmark operation.
     */
    void runOp(const Op& op, int countA, int countB);

    /**
     * Begins a benchmarked transaction.
     */
    void begin(const string& name);
    
    /**
     * Closes a benchmarked transaction.
     */
    void commit(const string& name);

    // ----------------------------------------------------------------------

    void initConnection();

    void closeConnection();

    void initOperations();
    
    template< bool feat > void initOperationsFeat();

    void closeOperations();

    void clearData();

    // operation invocation templates
    template< bool, bool > struct DelAllOp;

    template< bool, bool, bool > struct InsOp;

    template< const char** opName,
              bool isTableA,
              void (crund_ndb::Operations::*OP)(const NdbDictionary::Table*,
                                                int,int,bool),
              bool batch >
    struct byPKOp;

    template< const char** opName,
              void (crund_ndb::Operations::*OP)(const NdbDictionary::Table*,
                                                int,int,bool,int),
              bool batch >
    struct lengthOp;

    template< const char** opName,
              void (crund_ndb::Operations::*OP)(int,int,bool),
              bool forceSend >
    struct relOp;
};

} // crund_ndb

// ----------------------------------------------------------------------

//using namespace std;
using std::wistringstream;
using std::ios_base;
using std::cout;
using std::flush;
using std::endl;
using std::string;
using std::wstring;

using utils::toBool;
using utils::toString;
using crund_ndb::Driver;

Driver::Driver() {
}

#include <algorithm>
#include <cctype>

// ----------------------------------------------------------------------

crund_ndb::Operations* Driver::ops = NULL;

void
Driver::run() {
    init();

    // warmup runs
    for (unsigned int i = 0; i < warmupRuns; i++)
        runTests();

    // truncate log file, reset log buffers
    cout << endl
         << "++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++" << endl
         << "start logging results ..." << endl
         << "++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++" << endl
         << endl;
    log.close();
    log.open(logFileName.c_str(), ios_base::out | ios_base::trunc);
    assert (log.good());
    header.rdbuf()->str("");
    rtimes.rdbuf()->str("");

    // hot runs
    for (unsigned int i = 0; i < hotRuns; i++) {
        runTests();
    }    

    // write log buffers
    if (logRealTime) {
        // doesn't work: ostream << ostringstream->rdbuf()
        log << descr << ", rtime[ms]"
            << header.rdbuf()->str() << endl
            << rtimes.rdbuf()->str() << endl << endl << endl;
    }
    if (logCpuTime) {
        // doesn't work: ostream << ostringstream->rdbuf()
        log << descr << ", ctime[ms]"
            << header.rdbuf()->str() << endl
            << ctimes.rdbuf()->str() << endl << endl << endl;
    }

    close();
}

void
Driver::init() {
    loadProperties();
    initProperties();
    printProperties();

    // initialize the benchmark's resources
    ops = new crund_ndb::Operations();
    assert (!mgmdConnect.empty());
    ops->init(mgmdConnect.c_str());

    // open the benchmark's data log file
    cout << endl;
    cout << "writing results to file:    " << logFileName << endl;
    log.open(logFileName.c_str());
    assert (log.good());

    // clear log buffers
    logHeader = true;
    header.rdbuf()->str("");
    rtimes.rdbuf()->str("");
}

void
Driver::close() {
    // clear log buffers
    header.rdbuf()->str("");
    rtimes.rdbuf()->str("");

    // close the benchmark's data log file
    cout << "closing files ...    " << flush;
    log.close();
    cout << "       [ok]" << endl;

    // release the benchmark's resources
    assert (!mgmdConnect.empty());
    ops->close();
    delete ops;
    ops = NULL;
}

void
Driver::loadProperties() {
    if (propFileNames.size() == 0) {
        propFileNames.push_back("crund.properties");
    }

    cout << endl;
    for (vector<string>::const_iterator i = propFileNames.begin();
         i != propFileNames.end(); ++i) {
        cout << "reading properties file:    " << *i << endl;
        props.load(i->c_str());
        props.load(i->c_str());
        //wcout << props << endl;
    }
}

void
Driver::initProperties() {
    descr = "";
    logRealTime = false;
    logCpuTime = false;
    logSumOfOps = false;
    renewConnection = false;
    renewOperations = false;
    aStart = (1 << 8), aEnd = (1 << 12), aIncr = (1 << 2);
    bStart = (1 << 8), bEnd = (1 << 12), bIncr = (1 << 2);
    maxStringLength = 100;
    warmupRuns = 0;
    hotRuns = 1;
    mgmdConnect = "localhost";
    catalog = "";
    schema = "";

    // initialize boolean properties
    logRealTime = toBool(props[L"logRealTime"]);
    logCpuTime = toBool(props[L"logCpuTime"]);
    logSumOfOps = toBool(props[L"logSumOfOps"]);
    renewOperations = toBool(props[L"renewOperations"]);
    renewConnection = toBool(props[L"renewConnection"]);
    
    // initialize numeric properties
    wistringstream(props[L"aStart"]) >> aStart;
    wistringstream(props[L"aEnd"]) >> aEnd;
    wistringstream(props[L"aIncr"]) >> aIncr;
    wistringstream(props[L"bStart"]) >> bStart;
    wistringstream(props[L"bEnd"]) >> bEnd;
    wistringstream(props[L"bIncr"]) >> bIncr;
    wistringstream(props[L"maxStringLength"]) >> maxStringLength;
    wistringstream(props[L"warmupRuns"]) >> warmupRuns;
    wistringstream(props[L"hotRuns"]) >> hotRuns;

    // initialize exclude set
    const wstring& estr = props[L"exclude"];
    //cout << "estr='" << toString(estr) << "'" << endl;
    const size_t len = estr.length();
    size_t beg = 0, next;
    while (beg < len
           && ((next = estr.find_first_of(L",", beg)) != wstring::npos)) {
        // add substring if not empty
        if (beg < next) {
            const wstring& s = estr.substr(beg, next - beg);
            exclude.insert(toString(s));
        }
        beg = next + 1;
    }
    // add last substring if any
    if (beg < len) {
        const wstring& s = estr.substr(beg, len - beg);
        exclude.insert(toString(s));
    }
    
    // string properties
    mgmdConnect = toString(props[L"ndb.mgmdConnect"]);
    catalog = toString(props[L"ndb.catalog"]);
    schema = toString(props[L"ndb.schema"]);

    descr = "C++->NDBAPI(" + mgmdConnect + ")";
}

/**
 * Prints the benchmark's properties.
 */
void
Driver::printProperties() {
    const ios_base::fmtflags f = cout.flags();    
    // no effect calling manipulator function, not sure why
    //cout << ios_base::boolalpha;
    cout.flags(ios_base::boolalpha);
    
    cout << "main settings:" << endl;
    cout << "logRealTime:                " << logRealTime << endl;
    cout << "logCpuTime:                 " << logCpuTime << endl;
    cout << "logSumOfOps:                " << logSumOfOps << endl;
    cout << "renewOperations:            " << renewOperations << endl;
    cout << "renewConnection:            " << renewConnection << endl;
    cout << "aStart:                     " << aStart << endl;
    cout << "aEnd:                       " << aEnd << endl;
    cout << "aIncr:                      " << aIncr << endl;
    cout << "bStart:                     " << bStart << endl;
    cout << "bEnd:                       " << bEnd << endl;
    cout << "bIncr:                      " << bIncr << endl;
    cout << "maxStringLength:            " << maxStringLength << endl;
    cout << "warmupRuns:                 " << warmupRuns << endl;
    cout << "hotRuns:                    " << hotRuns << endl;
    cout << "exclude:                    " << toString(exclude) << endl;
    cout << "ndb.mgmdConnect             \"" << mgmdConnect << "\"" << endl;
    cout << "ndb.catalog                 \"" << catalog << "\"" << endl;
    cout << "ndb.schema                  \"" << schema << "\"" << endl;

    cout.flags(f);
}

// ----------------------------------------------------------------------

void
Driver::runTests() {
    initConnection();
    initOperations();

    for (unsigned int i = aStart; i <= aEnd; i *= aIncr) {
        //for (int j = bBeg; j <= bEnd; j *= bIncr)
        for (unsigned int j = (i > bStart ? i : bStart); j <= bEnd; j *= bIncr) {
            runOperations(i, j);
        }
    }

    cout << endl
         << "------------------------------------------------------------" << endl
         << endl;

    clearData();
    closeOperations();
    closeConnection();
}

void
Driver::runOperations(int countA, int countB) {
    cout << endl
         << "------------------------------------------------------------" << endl
         << "countA = " << countA << ", countB = " << countB << endl
         << endl;

    // log buffers
    if (logRealTime) {
        rtimes << "A=" << countA << ", B=" << countB;
        rta = 0L;
    }
    if (logCpuTime) {
        ctimes << "A=" << countA << ", B=" << countB;
        cta = 0L;
    }
    
    // pre-run cleanup
    if (renewConnection) {
        closeOperations();
        closeConnection();
        initConnection();
        initOperations();
    } else if (renewOperations) {
        closeOperations();
        initOperations();
    }
    clearData();

    // run operations
    for (Operations::const_iterator i = operations.begin();
         i != operations.end(); ++i) {
        // no need for pre-tx cleanup with NDBAPI-based loads
        //if (!allowExtendedPC) {
        //    // effectively prevent caching beyond Tx scope by clearing
        //    // any data/result caches before the next transaction
        //    clearPersistenceContext();
        //}
        runOp(**i, countA, countB);
    }
    if (logHeader) {
        if (logSumOfOps)
            header << "\ttotal";
    }

    // log buffers
    logHeader = false;
    if (logRealTime) {
        if (logSumOfOps) {
            rtimes << "\t" << rta;
            cout << endl
                 << "total" << endl
                 << "tx real time     = " << rta
                 << "\tms [begin..commit]" << endl;
        }
        rtimes << endl;
    }
    if (logCpuTime) {
        if (logSumOfOps) {
            ctimes << "\t" << cta;
            cout << endl
                 << "total" << endl
                 << "tx cpu time      = " << cta
                 << "\tms [begin..commit]" << endl;
        }
        ctimes << endl;
    }
}

void
Driver::runOp(const Op& op, int countA, int countB) {
    const string& name = op.name;
    if (exclude.find(name) == exclude.end()) {
        begin(name);
        op.run(countA, countB);
        commit(name);
    }
}
    
void
Driver::begin(const string& name) {
    cout << endl;
    cout << name << endl;

    if (logRealTime && logCpuTime) {
        s0 = hrt_tnow(&t0);
    } else if (logRealTime) {
        s0 = hrt_rtnow(&t0.rtstamp);
    } else if (logCpuTime) {
        s0 = hrt_ctnow(&t0.ctstamp);
    }

    // NDB Operations manages transactions boundaries itself
    //beginTransaction();
}

void
Driver::commit(const string& name) {
    // NDB Operations manages transactions boundaries itself
    //commitTransaction();

    if (logRealTime && logCpuTime) {
        s1 = hrt_tnow(&t1);
    } else if (logRealTime) {
        s1 = hrt_rtnow(&t1.rtstamp);
    } else if (logCpuTime) {
        s1 = hrt_ctnow(&t1.ctstamp);
    }

    if (logRealTime) {
        if (s0 | s1) {
            cout << "ERROR: failed to get the system's real time.";
            rtimes << "\tERROR";
        } else {
            long t = long(hrt_rtmicros(&t1.rtstamp, &t0.rtstamp)/1000);
            cout << "tx real time     = " << t
                 << "\tms [begin..commit]" << endl;
            rtimes << "\t" << t;
            rta += t;
        }
    }

    if (logCpuTime) {
        if (s0 | s1) {
            cout << "ERROR: failed to get this process's cpu time.";
            ctimes << "\tERROR";
        } else {
            long t = long(hrt_ctmicros(&t1.ctstamp, &t0.ctstamp)/1000);
            cout << "tx cpu time      = " << t
                 << "\tms [begin..commit]" << endl;
            ctimes << "\t" << t;
            cta += t;
        }
    }

    if (logHeader)
        header << "\t" << name;
}

//---------------------------------------------------------------------------

void
Driver::initConnection() {
    ops->initConnection(catalog.c_str(), schema.c_str());
}

void
Driver::closeConnection() {
    ops->closeConnection();
}

void
Driver::initOperations() {
    cout << "initializing operations ..." << flush;

    const bool feat = true;
    initOperationsFeat< !feat >();
    initOperationsFeat< feat >();    

    cout << " [Op: " << operations.size() << "]" << endl;
}

// the operation invocation templates look a bit complex, but they help
// a lot to factorize code over the operations' parameter signatures

template< bool tableA, bool batch >
struct Driver::DelAllOp : Op {
    DelAllOp() : Op(string(string(tableA ? "delAllA" : "delAllB0")
                            + (batch ? "_batch" : ""))) {
    }
            
    virtual void run(int countA, int countB) const {
        int count;
        ops->delByScan((tableA ? ops->meta->table_A :  ops->meta->table_B0),
                       count, batch);
        assert (count == (tableA ? countA : countB));
    }
};

template< bool tableA, bool setAttrs, bool batch >
struct Driver::InsOp : Op {
    InsOp() : Op(string(string(tableA ? "insA" : "insB0")
                        + (setAttrs ? "_attr" : "")
                        + (batch ? "_batch" : ""))) {
    }
            
    virtual void run(int countA, int countB) const {
        ops->ins((tableA ? ops->meta->table_A :  ops->meta->table_B0),
                 1, (tableA ? countA : countB), setAttrs, batch);
    }
};

template< const char** opName,
          bool isTableA,
          void (crund_ndb::Operations::*OP)(const NdbDictionary::Table*,
                                            int,int,bool),
          bool batch >
struct Driver::byPKOp : Op {
    byPKOp() : Op(string(*opName)
                   + string(isTableA ? "AByPK" : "B0ByPK")
                   + (batch ? "_batch" : "")) {
    }
            
    virtual void run(int countA, int countB) const {
        (ops->*OP)((isTableA ? ops->meta->table_A :  ops->meta->table_B0),
                     1, (isTableA ? countA : countB), batch);
    }
};

template< const char** opName,
          void (crund_ndb::Operations::*OP)(const NdbDictionary::Table*,
                                            int,int,bool,int),
          bool batch >
struct Driver::lengthOp : Op {
    const int length;

    lengthOp(int length) : Op(string(*opName)
                              + toString(length)
                              + (batch ? "_batch" : "")),
                           length(length) {
    }
            
    virtual void run(int countA, int countB) const {
        (ops->*OP)(ops->meta->table_B0, 1, countB, batch, length);
    }
};

template< const char** opName,
          void (crund_ndb::Operations::*OP)(int x,int y,bool z),
          bool forceSend >
struct Driver::relOp : Op {
    relOp() : Op(string(*opName) + (forceSend ? "_forceSend" : "")) {
    }
            
    virtual void run(int countA, int countB) const {
        (ops->*OP)(countA, countB, forceSend);
    }
};

// ISO C++ 98 does not allow for a string literal as a template argument
// for a non-type template parameter, because string literals are objects
// with internal linkage.  This restriction maybe lifted in C++0x.
//
// Until then, we have to allocate the operation names as variables
// (which are external at file scope by default).
const char* del_s = "del";
const char* set_s = "set";
const char* get_s = "get";
const char* setVarbinary_s = "setVarbinary";
const char* getVarbinary_s = "getVarbinary";
const char* clearVarbinary_s = "clearVarbinary";
const char* setVarchar_s = "setVarchar";
const char* getVarchar_s = "getVarchar";
const char* clearVarchar_s = "clearVarchar";
const char* setB0ToA_s = "setB0->A";
const char* navB0ToA_s = "navB0->A";
const char* navB0ToAalt_s = "navB0->A_alt";
const char* navAToB0_s = "navA->B0";
const char* navAToB0alt_s = "navA->B0_alt";
const char* nullB0ToA_s = "nullB0->A";

template< bool feat > void
Driver::initOperationsFeat() {

    const bool tableA = true;
    const bool setAttr = true;
    operations.push_back(
        new InsOp< tableA, !setAttr, feat >());

    operations.push_back(
        new InsOp< !tableA, !setAttr, feat >());

    operations.push_back(
        new byPKOp< &set_s, tableA, &crund_ndb::Operations::setByPK, feat >());

    operations.push_back(
        new byPKOp< &set_s, !tableA, &crund_ndb::Operations::setByPK, feat >());

    operations.push_back(
        new byPKOp< &get_s, tableA, &crund_ndb::Operations::getByPK, feat >());

    operations.push_back(
        new byPKOp< &get_s, !tableA, &crund_ndb::Operations::getByPK, feat >());

    for (unsigned int i = 1; i <= maxStringLength; i *= 10) {
        const int length = i;

        operations.push_back(
            new lengthOp< &setVarbinary_s, &crund_ndb::Operations::setVarbinary, feat >(length));

        operations.push_back(
            new lengthOp< &getVarbinary_s, &crund_ndb::Operations::getVarbinary, feat >(length));

        operations.push_back(
            new lengthOp< &clearVarbinary_s, &crund_ndb::Operations::setVarbinary, feat >(0));

        operations.push_back(
            new lengthOp< &setVarchar_s, &crund_ndb::Operations::setVarchar, feat >(length));

        operations.push_back(
            new lengthOp< &getVarchar_s, &crund_ndb::Operations::getVarchar, feat >(length));

        operations.push_back(
            new lengthOp< &clearVarchar_s, &crund_ndb::Operations::setVarchar, feat >(0));

    }

    operations.push_back(
        new relOp< &setB0ToA_s, &crund_ndb::Operations::setB0ToA, feat >());

    operations.push_back(
        new relOp< &navB0ToA_s, &crund_ndb::Operations::navB0ToA, feat >());

    operations.push_back(
        new relOp< &navB0ToAalt_s, &crund_ndb::Operations::navB0ToAalt, feat >());

    operations.push_back(
        new relOp< &navAToB0_s, &crund_ndb::Operations::navAToB0, feat >());

    operations.push_back(
        new relOp< &navAToB0alt_s, &crund_ndb::Operations::navAToB0alt, feat >());

    operations.push_back(
        new relOp< &nullB0ToA_s, &crund_ndb::Operations::nullB0ToA, feat >());

    operations.push_back(
        new byPKOp< &del_s, !tableA, &crund_ndb::Operations::delByPK, feat >());

    operations.push_back(
        new byPKOp< &del_s, tableA, &crund_ndb::Operations::delByPK, feat >());

    operations.push_back(
        new InsOp< tableA, setAttr, feat >());

    operations.push_back(
        new InsOp< !tableA, setAttr, feat >());

    operations.push_back(
        new DelAllOp< !tableA, feat >());

    operations.push_back(
        new DelAllOp< tableA, feat >());
}

void
Driver::closeOperations() {
    cout << "closing operations ..." << flush;
    for (Operations::const_iterator i = operations.begin();
         i != operations.end(); ++i) {
        delete *i;
    }
    operations.clear();
    cout << "      [ok]" << endl;
}

void
Driver::clearData()
{
    cout << "deleting all rows ..." << flush;
    int delB0 = -1;
    const bool batch = true;
    ops->delByScan(ops->meta->table_B0, delB0, batch);
    cout << "       [B0: " << delB0 << flush;
    int delA = -1;
    ops->delByScan(ops->meta->table_A, delA, batch);
    cout << ", A: " << delA << "]" << endl;
}

//---------------------------------------------------------------------------

void
Driver::exitUsage()
{
    cout << "usage: [options]" << endl
         << "    [-p <file name>]...    properties file name" << endl
         << "    [-l <file name>]       log file name for data output" << endl
         << "    [-h|--help]            print usage message and exit" << endl
         << endl;
    exit(1); // return an error code
}

void
Driver::parseArguments(int argc, const char* argv[])
{
    for (int i = 1; i < argc; i++) {
        const string arg = argv[i];
        if (arg.compare("-p") == 0) {
            if (i >= argc) {
                exitUsage();
            }
            propFileNames.push_back(argv[++i]);
        } else if (arg.compare("-l") == 0) {
            if (i >= argc) {
                exitUsage();
            }
            logFileName = argv[++i];
        } else if (arg.compare("-h") == 0 || arg.compare("--help") == 0) {
            exitUsage();
        } else {
            cout << "unknown option: " << arg << endl;
            exitUsage();
        }
    }
    
    if (logFileName.empty()) {
        logFileName = "log_";

        // format, destination strings (static size)
        const char format[] = "%Y%m%d_%H%M%S"; 
        const int size = sizeof("yyyymmdd_HHMMSS");
        char dest[size];
     
        // get time, convert to timeinfo (statically allocated) then to string
        const time_t now = time(0); 
        const int nchars = strftime(dest, size, format, localtime(&now));
        assert (nchars == size-1);
        (void)nchars;

        logFileName += dest;
        logFileName += ".txt";
        //cout << "logFileName='" << logFileName << "'" << endl;
    }
}

int
main(int argc, const char* argv[])
{
    TRACE("main()");
    
    Driver d;
    d.parseArguments(argc, argv);
    d.run();

    return 0;
}
