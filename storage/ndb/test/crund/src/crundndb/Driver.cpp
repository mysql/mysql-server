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

// global type aliases
typedef const NdbDictionary::Table* NdbTable;

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
    int aStart, aEnd, aScale;
    int bStart, bEnd, bScale;
    int maxVarbinaryBytes;
    int maxVarcharChars;
    int maxBlobBytes;
    int maxTextChars;
    int warmupRuns;
    int hotRuns;
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
     * Opens the benchmark's data log file.
     */
    void openLogFile();

    /**
     * Closes the benchmark's data log file.
     */
    void closeLogFile();

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
    template< bool > struct ADelAllOp;

    template< bool > struct B0DelAllOp;

    template< bool, bool > struct AInsOp;

    template< bool, bool > struct B0InsOp;

    template< const char**,
              void (crund_ndb::Operations::*)(NdbTable,int,int,bool),
              bool >
    struct AByPKOp;

    template< const char**,
              void (crund_ndb::Operations::*)(NdbTable,int,int,bool),
              bool >
    struct B0ByPKOp;

    template< const char**,
              void (crund_ndb::Operations::*)(NdbTable,int,int,bool,int),
              bool >
    struct LengthOp;

    template< const char**,
              void (crund_ndb::Operations::*)(NdbTable,int,int,bool,int),
              bool >
    struct ZeroLengthOp;

    template< const char**,
              void (crund_ndb::Operations::*)(int,int,bool),
              bool >
    struct RelOp;
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
using utils::toInt;
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
    for (int i = 0; i < warmupRuns; i++) {
        runTests();
    }

    // truncate log file, reset log buffers
    cout << endl
         << "++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++" << endl
         << "start logging results ..." << endl
         << "++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++" << endl
         << endl;
    header.rdbuf()->str("");
    rtimes.rdbuf()->str("");
    closeLogFile();
    openLogFile();

    // hot runs
    for (int i = 0; i < hotRuns; i++) {
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
    openLogFile();

    // clear log buffers
    logHeader = true;
    header.rdbuf()->str("");
    rtimes.rdbuf()->str("");

    // initialize the benchmark's resources
    ops = new crund_ndb::Operations();
    assert (!mgmdConnect.empty());
    ops->init(mgmdConnect.c_str());
}

void
Driver::close() {
    // clear log buffers
    header.rdbuf()->str("");
    rtimes.rdbuf()->str("");

    // release the benchmark's resources
    assert (!mgmdConnect.empty());
    ops->close();
    delete ops;
    ops = NULL;

    closeLogFile();
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
    cout << "initializing properties ... " << flush;

    ostringstream msg;

    logRealTime = toBool(props[L"logRealTime"]);
    logCpuTime = toBool(props[L"logCpuTime"]);
    logSumOfOps = toBool(props[L"logSumOfOps"]);
    renewOperations = toBool(props[L"renewOperations"]);
    renewConnection = toBool(props[L"renewConnection"]);

    aStart = toInt(props[L"aStart"], 256, 0);
    if (aStart < 1) {
        msg << "[ignored] aStart:            '"
            << toString(props[L"aStart"]) << "'" << endl;
        aStart = 256;
    }
    aEnd = toInt(props[L"aEnd"], aStart, 0);
    if (aEnd < aStart) {
        msg << "[ignored] aEnd:              '"
            << toString(props[L"aEnd"]) << "'" << endl;
        aEnd = aStart;
    }
    aScale = toInt(props[L"aScale"], 2, 0);
    if (aScale < 2) {
        msg << "[ignored] aScale:            '"
            << toString(props[L"aScale"]) << "'" << endl;
        aScale = 2;
    }

    bStart = toInt(props[L"bStart"], aStart, 0);
    if (bStart < 1) {
        msg << "[ignored] bStart:            '"
            << toString(props[L"bStart"]) << "'" << endl;
        bStart = aStart;
    }
    bEnd = toInt(props[L"bEnd"], bStart, 0);
    if (bEnd < bStart) {
        msg << "[ignored] bEnd:              '"
            << toString(props[L"bEnd"]) << "'" << endl;
        bEnd = bStart;
    }
    bScale = toInt(props[L"bScale"], 2, 0);
    if (bScale < 2) {
        msg << "[ignored] bScale:            '"
            << toString(props[L"bScale"]) << "'" << endl;
        bScale = 2;
    }

    maxVarbinaryBytes = toInt(props[L"maxVarbinaryBytes"], 100, 0);
    if (maxVarbinaryBytes < 1) {
        msg << "[ignored] maxVarbinaryBytes: '"
            << toString(props[L"maxVarbinaryBytes"]) << "'" << endl;
        maxVarbinaryBytes = 100;
    }

    maxVarcharChars = toInt(props[L"maxVarcharChars"], 100, 0);
    if (maxVarcharChars < 1) {
        msg << "[ignored] maxVarcharChars:   '"
            << toString(props[L"maxVarcharChars"]) << "'" << endl;
        maxVarcharChars = 100;
    }

    maxBlobBytes = toInt(props[L"maxBlobBytes"], 1000, 0);
    if (maxBlobBytes < 1) {
        msg << "[ignored] maxBlobBytes:      '"
            << toString(props[L"maxBlobBytes"]) << "'" << endl;
        maxBlobBytes = 1000;
    }

    maxTextChars = toInt(props[L"maxTextChars"], 1000, 0);
    if (maxTextChars < 1) {
        msg << "[ignored] maxTextChars:      '"
            << toString(props[L"maxTextChars"]) << "'" << endl;
        maxTextChars = 1000;
    }

    warmupRuns = toInt(props[L"warmupRuns"], 0, -1);
    if (warmupRuns < 0) {
        msg << "[ignored] warmupRuns:        '"
            << toString(props[L"warmupRuns"]) << "'" << endl;
        warmupRuns = 0;
    }

    hotRuns = toInt(props[L"hotRuns"], 1, -1);
    if (hotRuns < 0) {
        msg << "[ignored] hotRuns:           '"
            << toString(props[L"hotRuns"]) << "'" << endl;
        hotRuns = 1;
    }

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

    if (msg.tellp() == 0) {
        cout << "[ok]" << endl;
    } else {
        cout << endl << msg.str() << endl;
    }

    cout << "data set:                   "
         << "[A=" << aStart << ".." << aEnd
         << ", B=" << bStart << ".." << bEnd << "]" << endl;
}

void
Driver::printProperties() {
    const ios_base::fmtflags f = cout.flags();
    // no effect calling manipulator function, not sure why
    //cout << ios_base::boolalpha;
    cout.flags(ios_base::boolalpha);

    cout << endl << "main settings:" << endl;
    cout << "logRealTime:                " << logRealTime << endl;
    cout << "logCpuTime:                 " << logCpuTime << endl;
    cout << "logSumOfOps:                " << logSumOfOps << endl;
    cout << "renewOperations:            " << renewOperations << endl;
    cout << "renewConnection:            " << renewConnection << endl;
    cout << "aStart:                     " << aStart << endl;
    cout << "bStart:                     " << bStart << endl;
    cout << "aEnd:                       " << aEnd << endl;
    cout << "bEnd:                       " << bEnd << endl;
    cout << "aScale:                     " << aScale << endl;
    cout << "bScale:                     " << bScale << endl;
    cout << "maxVarbinaryBytes:          " << maxVarbinaryBytes << endl;
    cout << "maxVarcharChars:            " << maxVarcharChars << endl;
    cout << "maxBlobBytes:               " << maxBlobBytes << endl;
    cout << "maxTextChars:               " << maxTextChars << endl;
    cout << "warmupRuns:                 " << warmupRuns << endl;
    cout << "hotRuns:                    " << hotRuns << endl;
    cout << "exclude:                    " << toString(exclude) << endl;
    cout << "ndb.mgmdConnect             \"" << mgmdConnect << "\"" << endl;
    cout << "ndb.catalog                 \"" << catalog << "\"" << endl;
    cout << "ndb.schema                  \"" << schema << "\"" << endl;

    cout.flags(f);
}

void
Driver::openLogFile() {
    cout << endl
         << "writing results to file:    " << logFileName << endl;
    //log.open(logFileName.c_str());
    log.open(logFileName.c_str(), ios_base::out | ios_base::trunc);
    assert (log.good());
}

void
Driver::closeLogFile() {
    cout << "closing files ...    " << flush;
    log.close();
    cout << "       [ok]" << endl;
}

// ----------------------------------------------------------------------

void
Driver::runTests() {
    initConnection();
    initOperations();

    assert(aStart <= aEnd && aScale > 1);
    assert(bStart <= bEnd && bScale > 1);
    for (int i = aStart; i <= aEnd; i *= aScale) {
        for (int j = bStart; j <= bEnd; j *= bScale) {
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
         << "------------------------------------------------------------" << endl;

    if (countA > countB) {
        cout << "skipping operations ...     "
             << "[A=" << countA << ", B=" << countB << "]" << endl;
        return;
    }
    cout << "running operations ...      "
         << "[A=" << countA << ", B=" << countB << "]" << endl;

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
                 << "tx real time\t\t= " << rta
                 << "\tms [begin..commit]" << endl;
        }
        rtimes << endl;
    }
    if (logCpuTime) {
        if (logSumOfOps) {
            ctimes << "\t" << cta;
            cout << endl
                 << "total" << endl
                 << "tx cpu time\t\t= " << cta
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
            cout << "tx real time\t\t= " << t
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
            cout << "tx cpu time\t\t= " << t
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

template< bool OB >
struct Driver::ADelAllOp : Op {
    ADelAllOp() : Op(string("delAllA")
                     + (OB ? "_batch" : "")) {
    }

    virtual void run(int countA, int countB) const {
        int count;
        ops->delByScan(ops->meta->table_A, count, OB);
        assert (count == countA);
    }
};

template< bool OB >
struct Driver::B0DelAllOp : Op {
    B0DelAllOp() : Op(string("delAllB0")
                      + (OB ? "_batch" : "")) {
    }

    virtual void run(int countA, int countB) const {
        int count;
        ops->delByScan(ops->meta->table_B0, count, OB);
        assert (count == countB);
    }
};

template< bool OSA, bool OB >
struct Driver::AInsOp : Op {
    AInsOp() : Op(string("insA")
                  + (OSA ? "_attr" : "")
                  + (OB ? "_batch" : "")) {
    }

    virtual void run(int countA, int countB) const {
        ops->ins(ops->meta->table_A, 1, countA, OSA, OB);
    }
};

template< bool OSA, bool OB >
struct Driver::B0InsOp : Op {
    B0InsOp() : Op(string("insB0")
                   + (OSA ? "_attr" : "")
                   + (OB ? "_batch" : "")) {
    }

    virtual void run(int countA, int countB) const {
        ops->ins(ops->meta->table_B0, 1, countB, OSA, OB);
    }
};

template< const char** ON,
          void (crund_ndb::Operations::*OF)(NdbTable,int,int,bool),
          bool OB >
struct Driver::AByPKOp : Op {
    AByPKOp() : Op(string(*ON)
                   + (OB ? "_batch" : "")) {
    }

    virtual void run(int countA, int countB) const {
        (ops->*OF)(ops->meta->table_A, 1, countA, OB);
    }
};

template< const char** ON,
          void (crund_ndb::Operations::*OF)(NdbTable,int,int,bool),
          bool OB >
struct Driver::B0ByPKOp : Op {
    B0ByPKOp() : Op(string(*ON)
                    + (OB ? "_batch" : "")) {
    }

    virtual void run(int countA, int countB) const {
        (ops->*OF)(ops->meta->table_B0, 1, countB, OB);
    }
};

template< const char** ON,
          void (crund_ndb::Operations::*OF)(NdbTable,int,int,bool,int),
          bool OB >
struct Driver::LengthOp : Op {
    const int length;

    LengthOp(int length) : Op(string(*ON)
                              + toString(length)
                              + (OB ? "_batch" : "")),
                           length(length) {
    }

    virtual void run(int countA, int countB) const {
        (ops->*OF)(ops->meta->table_B0, 1, countB, OB, length);
    }
};

template< const char** ON,
          void (crund_ndb::Operations::*OF)(NdbTable,int,int,bool,int),
          bool OB >
struct Driver::ZeroLengthOp : LengthOp< ON, OF, OB > {
    ZeroLengthOp(int length) : LengthOp< ON, OF, OB >(length) {
    }

    virtual void run(int countA, int countB) const {
        (ops->*OF)(ops->meta->table_B0, 1, countB, OB, 0);
    }
};

template< const char** ON,
          void (crund_ndb::Operations::*OF)(int,int,bool),
          bool OFS >
struct Driver::RelOp : Op {
    RelOp() : Op(string(*ON)
                 + (OFS ? "_forceSend" : "")) {
    }

    virtual void run(int countA, int countB) const {
        (ops->*OF)(countA, countB, OFS);
    }
};

// ISO C++ 98 does not allow for a string literal as a template argument
// for a non-type template parameter, because string literals are objects
// with internal linkage.  This restriction maybe lifted in C++0x.
//
// Until then, we have to allocate the operation names as variables
// (which are external at file scope by default).
const char* delAByPK_s = "delAByPK";
const char* delB0ByPK_s = "delB0ByPK";
const char* setAByPK_s = "setAByPK";
const char* setB0ByPK_s = "setB0ByPK";
const char* getAByPK_bb_s = "getAByPK_bb";
const char* getB0ByPK_bb_s = "getB0ByPK_bb";
const char* getAByPK_ar_s = "getAByPK_ar";
const char* getB0ByPK_ar_s = "getB0ByPK_ar";

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

    const bool setAttr = true;
    operations.push_back(
        new AInsOp< !setAttr, feat >());

    operations.push_back(
        new B0InsOp< !setAttr, feat >());

    operations.push_back(
        new AByPKOp< &setAByPK_s, &crund_ndb::Operations::setByPK, feat >());

    operations.push_back(
        new B0ByPKOp< &setB0ByPK_s, &crund_ndb::Operations::setByPK, feat >());

    operations.push_back(
        new AByPKOp< &getAByPK_bb_s, &crund_ndb::Operations::getByPK_bb, feat >());

    operations.push_back(
        new AByPKOp< &getAByPK_ar_s, &crund_ndb::Operations::getByPK_ar, feat >());

    operations.push_back(
        new B0ByPKOp< &getB0ByPK_bb_s, &crund_ndb::Operations::getByPK_bb, feat >());

    operations.push_back(
        new B0ByPKOp< &getB0ByPK_ar_s, &crund_ndb::Operations::getByPK_ar, feat >());

    for (int i = 1; i <= maxVarbinaryBytes; i *= 10) {
        const int length = i;

        operations.push_back(
            new LengthOp< &setVarbinary_s, &crund_ndb::Operations::setVarbinary, feat >(length));

        operations.push_back(
            new LengthOp< &getVarbinary_s, &crund_ndb::Operations::getVarbinary, feat >(length));

        operations.push_back(
            new ZeroLengthOp< &clearVarbinary_s, &crund_ndb::Operations::setVarbinary, feat >(length));
    }

    for (int i = 1; i <= maxVarcharChars; i *= 10) {
        const int length = i;

        operations.push_back(
            new LengthOp< &setVarchar_s, &crund_ndb::Operations::setVarchar, feat >(length));

        operations.push_back(
            new LengthOp< &getVarchar_s, &crund_ndb::Operations::getVarchar, feat >(length));

        operations.push_back(
            new ZeroLengthOp< &clearVarchar_s, &crund_ndb::Operations::setVarchar, feat >(length));
    }

    operations.push_back(
        new RelOp< &setB0ToA_s, &crund_ndb::Operations::setB0ToA, feat >());

    operations.push_back(
        new RelOp< &navB0ToA_s, &crund_ndb::Operations::navB0ToA, feat >());

    operations.push_back(
        new RelOp< &navB0ToAalt_s, &crund_ndb::Operations::navB0ToAalt, feat >());

    operations.push_back(
        new RelOp< &navAToB0_s, &crund_ndb::Operations::navAToB0, feat >());

    operations.push_back(
        new RelOp< &navAToB0alt_s, &crund_ndb::Operations::navAToB0alt, feat >());

    operations.push_back(
        new RelOp< &nullB0ToA_s, &crund_ndb::Operations::nullB0ToA, feat >());

    operations.push_back(
        new B0ByPKOp< &setAByPK_s, &crund_ndb::Operations::delByPK, feat >());

    operations.push_back(
        new AByPKOp< &setB0ByPK_s, &crund_ndb::Operations::delByPK, feat >());

    operations.push_back(
        new AInsOp< setAttr, feat >());

    operations.push_back(
        new B0InsOp< setAttr, feat >());

    operations.push_back(
        new ADelAllOp< feat >());

    operations.push_back(
        new B0DelAllOp< feat >());
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
    ops->clearData();
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
