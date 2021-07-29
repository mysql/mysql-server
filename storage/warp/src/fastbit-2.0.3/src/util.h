// File: $Id$
// Author: John Wu <John.Wu at acm.org> Lawrence Berkeley National Laboratory
// Copyright (c) 2000-2016 the Regents of the University of California
#ifndef IBIS_UTIL_H
#define IBIS_UTIL_H
///@file
/// Defines minor utility functions and common classes used by
/// FastBit.
///
#include "const.h"

#include <map>          // std::map
#include <string>       // std::string
#include <limits>       // std::numeric_limits
#include <sstream>      // std::ostringstream used by ibis::util::logger
#include <cctype>       // std::isspace
#include <cstring>      // std::strcpy

#include <float.h>
#include <math.h>       // fabs, floor, ceil, log10, nextafter...
#include <stdlib.h>
#include <stdio.h>      // sprintf, remove
#include <sys/stat.h>   // stat, mkdir, chmod
#include <fcntl.h>      // open, close
#if !defined(unix) && defined(_WIN32)
#  include <windows.h>
#  include <io.h>
#  include <fcntl.h>      // _O_..
#  include <direct.h>     // _rmdir
#  include <sys/stat.h>   // _stat, mkdir, chmod
#  define rmdir _rmdir
   int truncate(const char*, uint32_t);
//#elif HAVE_UNISTD_H
#else
#  include <unistd.h>     // read, lseek, truncate, rmdir
#endif
#if defined(HAVE_FLOCK)
#  include <sys/file.h>   // flock
#endif
#if defined(HAVE_ATOMIC_TEMPLATE)
#  include <atomic>
#endif

// minimum size for invoking mmap operation, default to 1 MB
#ifndef FASTBIT_MIN_MAP_SIZE
#define FASTBIT_MIN_MAP_SIZE 1048576
#endif

#if ! (defined(HAVE_MMAP) || defined(_MSC_VER))
#  if defined(unix)||defined(__linux__)||defined(__APPLE__)||defined(__CYGWIN__)
#    define HAVE_MMAP 1
#  elif defined(__MINGW32__)
#    undef HAVE_MMAP 
#  elif defined(_XOPEN_SOURCE)
#    define HAVE_MMAP _XOPEN_SOURCE - 0 >= 500
#  elif defined(_POSIX_C_SOURCE)
#    define HAVE_MMAP _POSIX_C_SOURCE - 0 >= 0
#  else
#    undef HAVE_MMAP
#  endif
#endif

#if (HAVE_MMAP+0>0) || (defined(_WIN32) && defined(_MSC_VER))
#define HAVE_FILE_MAP 1
#endif

#ifndef DBL_EPSILON
#define DBL_EPSILON 2.2204460492503131e-16
#else
// some wrongly define it to be 1.1e-16
#undef  DBL_EPSILON
#define DBL_EPSILON 2.2204460492503131e-16
#endif

#ifndef FASTBIT_FLOAT_NULL
#define FASTBIT_FLOAT_NULL std::numeric_limits<float>::quiet_NaN()
#endif
#ifndef FASTBIT_DOUBLE_NULL
#define FASTBIT_DOUBLE_NULL std::numeric_limits<double>::quiet_NaN()
#endif

/// Guess about GCC atomic operations
#if !defined(HAVE_GCC_ATOMIC32) && defined(WITHOUT_FASTBIT_CONFIG_H)
#if __GNUC__+0 >= 4 && defined(__linux__)
#define HAVE_GCC_ATOMIC32 2
#endif
#endif
#if !defined(HAVE_GCC_ATOMIC64) && defined(WITHOUT_FASTBIT_CONFIG_H)
#if defined(__IA64__) || defined(__x86_64__) || defined(__ppc64__)
#if __GNUC__+0 >= 4 && defined(__linux__)
#define HAVE_GCC_ATOMIC64 2
#endif
#endif
#endif

/// Guess about MS Windows automic operation support
#if defined(_MSC_VER) && defined(_WIN32)
#ifndef HAVE_WIN_ATOMIC32
#if defined(NTDDI_VERSION) && defined(NTDDI_WIN2K)
#if NTDDI_VERSION >= NTDDI_WIN2K
#define HAVE_WIN_ATOMIC32
#endif
#elif defined(WINVER)
#if WINVER >= 0x0500
#define HAVE_WIN_ATOMIC32
#endif
#endif
#endif
#ifndef HAVE_WIN_ATOMIC64
#if defined(NTDDI_VERSION) && defined(NTDDI_WINVISTA)
#if NTDDI_VERSION >= NTDDI_WINVISTA
#define HAVE_WIN_ATOMIC64
#endif
#elif defined(WINVER)
#if WINVER >= 0x0600
#define HAVE_WIN_ATOMIC64
#endif
#endif
#endif
#endif

// mapping the names of the low level IO functions defined in <unistd.h>
#if defined(_MSC_VER) && defined(_WIN32)
#define UnixOpen  ::_open
#define UnixClose ::_close
#define UnixRead  ::_read
#define UnixWrite ::_write
#define UnixSeek  ::_lseek
#define UnixFlush  ::_commit
#define UnixSnprintf ::_snprintf
#define UnixStat  ::_stat
#define UnixFStat ::_fstat
#define Stat_T    struct _stat
#else //_BSD_SOURCE || _ISOC99_SOURCE || _XOPEN_SOURCE >= 500 || _POSIX_C_SOURCE >= 200112L
#define UnixOpen  ::open
#define UnixClose ::close
#define UnixRead  ::read
#define UnixWrite ::write
#define UnixSeek  ::lseek
#define UnixFlush ::fsync
#define UnixSnprintf ::snprintf
#define UnixStat  ::stat
#define UnixFStat ::fstat
#define Stat_T    struct stat
#endif

// define the option for opening a file in read only mode
#if defined(O_RDONLY)
#if defined(O_LARGEFILE)
#if defined(O_BINARY)
#define OPEN_READONLY O_RDONLY | O_BINARY | O_LARGEFILE
#else
#define OPEN_READONLY O_RDONLY | O_LARGEFILE
#endif
#elif defined(O_BINARY)
#define OPEN_READONLY O_RDONLY | O_BINARY
#else
#define OPEN_READONLY O_RDONLY
#endif
#elif defined(_O_RDONLY)
#if defined(_O_LARGEFILE)
#define OPEN_READONLY _O_RDONLY | _O_LARGEFILE | _O_BINARY
#else
#define OPEN_READONLY _O_RDONLY | _O_BINARY
#endif
#endif
// define the option for opening a new file for writing
#if defined(O_WRONLY)
#if defined(O_LARGEFILE)
#if defined(O_BINARY)
#define OPEN_WRITENEW O_WRONLY | O_BINARY | O_CREAT | O_TRUNC | O_LARGEFILE
#else
#define OPEN_WRITENEW O_WRONLY | O_CREAT | O_TRUNC | O_LARGEFILE
#endif
#elif defined(O_BINARY)
#define OPEN_WRITENEW O_WRONLY | O_BINARY | O_CREAT | O_TRUNC
#else
#define OPEN_WRITENEW O_WRONLY | O_CREAT | O_TRUNC
#endif
#elif defined(_O_WRONLY)
#if defined(_O_LARGEFILE)
#define OPEN_WRITENEW _O_WRONLY|_O_CREAT|_O_TRUNC|_O_LARGEFILE|_O_BINARY
#else
#define OPEN_WRITENEW _O_WRONLY|_O_CREAT|_O_TRUNC|_O_BINARY
#endif
#endif
// define the option for opening an existing file for writing
#if defined(O_WRONLY)
#if defined(O_LARGEFILE)
#if defined(O_BINARY)
#define OPEN_WRITEADD O_WRONLY | O_BINARY | O_CREAT | O_LARGEFILE
#else
#define OPEN_WRITEADD O_WRONLY | O_CREAT| O_LARGEFILE
#endif
#elif defined(O_BINARY)
#define OPEN_WRITEADD O_WRONLY | O_BINARY | O_CREAT
#else
#define OPEN_WRITEADD O_WRONLY | O_CREAT
#endif
#elif defined(_O_WRONLY)
#if defined(_O_LARGEFILE)
#define OPEN_WRITEADD _O_WRONLY | _O_CREAT | _O_LARGEFILE | _O_BINARY
#else
#define OPEN_WRITEADD _O_WRONLY | _O_CREAT | _O_BINARY
#endif
#endif
// define the option for opening a file for reading and writing
#if defined(O_RDWR)
#if defined(O_LARGEFILE)
#if defined(O_BINARY)
#define OPEN_READWRITE O_RDWR | O_BINARY | O_CREAT | O_LARGEFILE
#else
#define OPEN_READWRITE O_RDWR | O_CREAT | O_LARGEFILE
#endif
#elif defined(O_BINARY)
#define OPEN_READWRITE O_RDWR | O_BINARY | O_CREAT
#else
#define OPEN_READWRITE O_RDWR | O_CREAT
#endif
#elif defined(_O_RDWR)
#if defined(_O_LARGEFILE)
#define OPEN_READWRITE _O_RDWR | _O_CREAT | _O_LARGEFILE | _O_BINARY
#else
#define OPEN_READWRITE _O_RDWR | _O_CREAT | _O_BINARY
#endif
#endif
// define the option for opening an existing file for appending only
#if defined(O_WRONLY)
#if defined(O_LARGEFILE)
#if defined(O_BINARY)
#define OPEN_APPENDONLY O_WRONLY | O_BINARY | O_CREAT | O_APPEND | O_LARGEFILE
#else
#define OPEN_APPENDONLY O_WRONLY | O_CREAT | O_APPEND | O_LARGEFILE
#endif
#elif defined(O_BINARY)
#define OPEN_APPENDONLY O_WRONLY | O_BINARY | O_CREAT | O_APPEND
#else
#define OPEN_APPENDONLY O_WRONLY | O_CREAT | O_APPEND
#endif
#elif defined(_O_WRONLY)
#if defined(_O_LARGEFILE)
#define OPEN_APPENDONLY _O_WRONLY | _O_CREAT | _O_APPEND | _O_LARGEFILE | _O_BINARY
#else
#define OPEN_APPENDONLY _O_WRONLY | _O_CREAT | _O_APPEND | _O_BINARY
#endif
#endif
// the default file mode (associate with _O_CREAT)
#if defined(_MSC_VER) && defined(_WIN32)
#define OPEN_FILEMODE _S_IREAD | _S_IWRITE
#elif defined(S_IRGRP) && defined(S_IWGRP) && defined(S_IROTH)
#define OPEN_FILEMODE S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH
#else
#define OPEN_FILEMODE S_IRUSR | S_IWUSR
#endif

#if defined(_MSC_VER)

#define FORCE_INLINE    __forceinline

#include <stdlib.h>

#define BIG_CONSTANT(x) (x)

// Other compilers

#else   // defined(_MSC_VER)

#define FORCE_INLINE inline __attribute__((always_inline))

inline uint32_t _rotl32( uint32_t x, int8_t r ) {
    return (x << r) | (x >> (32 - r));
}

inline uint64_t _rotl64( uint64_t x, int8_t r ) {
    return (x << r) | (x >> (64 - r));
}

#define BIG_CONSTANT(x) (x##LLU)
#endif // !defined(_MSC_VER)
#define FASTBIT_ROTL32(x,y)     _rotl32(x,y)
#define FASTBIT_ROTL64(x,y)     _rotl64(x,y)

#if defined(_WIN32) && defined(_MSC_VER)
// needed for numeric_limits<>::max, min function calls
#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif
#endif

#ifndef FASTBIT_CASE_SENSITIVE_COMPARE
// By default use case sensitive comparisons for string values when
// evaluating SQL LIKE statements.  Explicitly set this to 0 before
// compiling to allow case insensitive evaluations.
#define FASTBIT_CASE_SENSITIVE_COMPARE 1
#endif

// The meta characters used in ibis::util::strMatch.
#define STRMATCH_META_CSH_ANY '*'
#define STRMATCH_META_CSH_ONE '?'
#define STRMATCH_META_SQL_ANY '%'
#define STRMATCH_META_SQL_ONE '_'
#define STRMATCH_META_ESCAPE '\\'

// // The function isfinite is a macro defined in math.h according to
// // opengroup.org.  As of 2011, only MS visual studio does not have a
// // definition for isfinite, but it has _finite in float,h.
// #if defined(_MSC_VER) && defined(_WIN32)
// inline int isfinite(double x) {return _finite(x);}
// #elif !defined(isfinite)
// #define isfinite finite
// #endif
#define IBIS_2STR(x) #x
#define IBIS_INT_STR(x) IBIS_2STR(x)
#define IBIS_FILE_LINE " -- " __FILE__ ":" IBIS_INT_STR(__LINE__)

#define LOGGER(v)                                       \
    if (false == (v)) ; else ibis::util::logger(0)() 
// need these silly intermediate macro functions to force the arguments to
// be evaluated before ## is applied
#define IBIS_JOIN_MACRO2(X, Y) X##Y
#define IBIS_JOIN_MACRO(X, Y) IBIS_JOIN_MACRO2(X, Y)
#ifdef __GNUC__
#define IBIS_GUARD_NAME IBIS_JOIN_MACRO(_guard, __LINE__) __attribute__( ( unused ) )
#else
#define IBIS_GUARD_NAME IBIS_JOIN_MACRO(_guard, __LINE__)
#endif
#define IBIS_BLOCK_GUARD                                        \
    ibis::util::guard IBIS_GUARD_NAME = ibis::util::makeGuard

namespace std { // extend namespace std slightly
    // specialization of less<> to work with char*
    template <> struct less< char* > {
        bool operator()(const char*x, const char*y) const {
            return std::strcmp(x, y) < 0;
        }
    };

    // specialization of less<> on const char* (case sensitive comparison)
    template <> struct less< const char* > {
        bool operator()(const char* x, const char* y) const {
            return std::strcmp(x, y) < 0;
        }
    };

    // specialization of equal_to<> on const char* (case sensitive comparison)
    template <> struct equal_to< const char* > {
        bool operator()(const char* x, const char* y) const {
            return std::strcmp(x, y) == 0;
        }
    };

    template <> struct less< ibis::rid_t > {
        bool operator()(const ibis::rid_t& x, const ibis::rid_t& y) const {
            return (x < y);
        }
    };

    template <> struct less< const ibis::rid_t* > {
        bool operator()(const ibis::rid_t* x, const ibis::rid_t* y) const {
            return (*x < *y);
        }
    };
} // namespace std

namespace ibis {
    /// @defgroup FastBitIBIS FastBit IBIS implementation core objects.
    /// @{
    class resource;     ///!< To store configuration parameters.
    class bitvector;    ///!< To store one bit sequence/bitmap.
    class column;       ///!< One column/attribute of a table.
    class fileManager;  ///!< A simple file manager.
    class horometer;    ///!< A timer class.
    class index;        ///!< The base class of indices.
    class roster;       ///!< A projection of a column in ascending order.
    class bitvector64;  ///!< The 64-bit version of bitvector class.

    class dictionary;   ///!< Map strings to integers and back.
    class bundle;       ///!< To organize in-memory data for group-by.
    class colValues;    ///!< To store a column of in-memory data.

    class fromClause;   ///!< From clause.
    class whereClause;  ///!< Where clause.
    class selectClause; ///!< Select clause.
    /// @}

    /// A global list of data partitions.
    extern FASTBIT_CXX_DLLSPEC partList datasets;

    typedef std::vector<colValues*> colList; /// List of in-memory data.

    /// The reference to the global configuration parameters.
    FASTBIT_CXX_DLLSPEC ibis::resource& gParameters();

    /// A data structure to store a small set of names.  The names are
    /// sorted in case-insensitive order.
    class FASTBIT_CXX_DLLSPEC nameList {
    public:
        nameList() : cstr(0), buff(0) {};
        nameList(const char* str) : cstr(0), buff(0) {select(str);}
        ~nameList() {if (cstr) clear();}

        bool empty() const {return cstr == 0;}
        const char* operator*() const {return cstr;};
        uint32_t size() const {return cvec.size();};

        /// Replace existing content with these names.  Remove existing names.
        void select(const char* str);
        /// Add more names to the list.  Keep existing content.
        void add(const char* str);
        /// Find the order of the @c key in the list.  If the @c key is in
        /// the list it returns the position of the @c key, otherwise it
        /// returns the size of the name list.
        uint32_t find(const char* key) const;

        const char* operator[](uint32_t i) const {return cvec[i];}
        typedef std::vector< const char* >::const_iterator const_iterator;
        const_iterator begin() const {return cvec.begin();}
        const_iterator end() const {return cvec.end();}

        void clear()
        {cvec.clear(); delete [] cstr; delete [] buff; buff=0; cstr=0;}

    private:
        typedef std::vector< const char * > compStore;
        char* cstr;     // copy of the names as a single string
        char* buff;     // same as cstr, but delimiter is \0
        compStore cvec; // contains pointers to buff, for easier access

        nameList(const nameList&);
        nameList& operator=(const nameList&);
    }; // class nameList

    /// An associative array for data partitions.  Only used internally for
    /// sorting data partitions.
    typedef std::map< const char*, part*, lessi > partAssoc;

    /// A specialization of std::bad_alloc.
    class bad_alloc : public std::bad_alloc {
    public:
        /// Constructor.  The argument is expected to be a static string so
        /// that there is no need to allocate any memory for the error
        /// message.
        bad_alloc(const char *m="unknown") throw() : mesg_(m) {};
        virtual ~bad_alloc() throw() {}
        virtual const char* what() const throw() {return mesg_;}

    private:
        /// Only contains a static string so as not to invoke any dynamic
        /// memory operations.
        const char *mesg_;
    }; // bad_alloc

    /// Organize the miscellaneous functions under the name util.
    namespace util {
        /// charTable lists the 64 printable characters to be used for names
        extern const char* charTable;
        /// charIndex maps the characters (ASCII) back to integer [0-64]
        extern const short unsigned charIndex[];
        /// Delimiters used to separate a string of names. @sa ibis::nameList
        extern const char* delimiters;
        /// log base 2 of an integer, the lookup table
        extern const int log2table[256];
        /// A mutex for serialize operations FastBit wide.  Currently it is
        /// used by the functions that generating user name, asking for
        /// password, backing up active tables, cleaning up the list of
        /// tables.  It is also used extensively in the implementation of C
        /// API functions to ensure the cache maintained for C users are
        /// manipulated by one user at a time.
        extern FASTBIT_CXX_DLLSPEC pthread_mutex_t envLock;

        /// Remove leading and trailing blank space.
        inline char* trim(char* str);
        /// Duplicate string content with C++ default new operator.
        char* strnewdup(const char* s);
        /// Duplicate no more than n characters.
        char* strnewdup(const char* s, const uint32_t n);
        /// Remove trailing character 'tail' from str.
        inline void removeTail(char* str, char tail);
        char* getString(const char* buf);
        const char* getToken(char*& str, const char* tok_chrs);
        int readInt(int64_t& val, const char *&str,
                    const char* del=ibis::util::delimiters);
        int readUInt(uint64_t& val, const char *&str,
                     const char* del=ibis::util::delimiters);
        int readDouble(double& val, const char *&str,
                       const char* del=ibis::util::delimiters);
        FASTBIT_CXX_DLLSPEC int
        readString(std::string& str, const char*& buf,
                   const char *delim=0);
        FASTBIT_CXX_DLLSPEC const char*
        readString(char*& buf, const char *delim=0);

        int64_t read(int, void*, int64_t);
        int64_t write(int, const void*, int64_t);

        void removeDir(const char* name, bool leaveDir=false);
        int makeDir(const char* dir);
        FASTBIT_CXX_DLLSPEC off_t getFileSize(const char* name);
        int copy(const char* to, const char* from);

        /// Set the verboseness level.  Unless the code is compiled with
        /// DEBUG macro set, the default verboseness level is 0, which will
        /// only print out information about major errors.
        inline void setVerboseLevel(int v) {ibis::gVerbose=v;}
        /// Return the user name.
        FASTBIT_CXX_DLLSPEC const char* userName();
        uint32_t serialNumber();
        void uniformFraction(const long unsigned idx,
                             long unsigned &denominator,
                             long unsigned &numerator);
        inline double rand();

        ///@{
        FASTBIT_CXX_DLLSPEC uint32_t checksum(const char* str, uint32_t sz);
        inline uint32_t checksum(uint32_t a, uint32_t b);
        std::string shortName(const std::string& longname);
        std::string randName(const std::string& longname);
        ///@}

        ///@{
        void int2string(std::string &str, unsigned val);
        void int2string(std::string &str, unsigned v1, unsigned v2);
        void int2string(std::string &str, unsigned v1,
                        unsigned v2, unsigned v3);
        void int2string(std::string &str, const std::vector<unsigned>& val);
        void encode64(uint64_t, std::string&);
        int  decode64(uint64_t&, const std::string&);
        int  decode16(uint64_t&, const char*);
        std::string groupby1000(uint64_t);
        ///@}

        /// Functions to handle manipulation of floating-point numbers.
        ///@{
        double incrDouble(const double&);
        double decrDouble(const double&);
        void   eq2range(const double&, double&, double&);
        /// Reduce the decimal precision of the incoming floating-point
        /// value to specified precision. 
        inline double coarsen(const double in, unsigned prec=2);
        /// Compute a compact 64-bit floating-point value with a short
        /// decimal representation.
        double compactValue(double left, double right,
                            double start=0.0);

        /// Compute a compact 64-bit floating-point value with a short
        /// binary representation.
        double compactValue2(double left, double right,
                             double start=0.0);

        /// Set a double to NaN.
        void setNaN(double& val);
        void setNaN(float& val);

        /// Round the incoming value to the largest output value that is no
        /// more than the input.  Both Tin and Tout must be elementary data
        /// types, and Tout must be an elementary integral type.
        template <typename Tin, typename Tout>
        void round_down(const Tin& inval, Tout& outval) {
            outval = (std::numeric_limits<Tout>::min() > inval ?
                      std::numeric_limits<Tout>::min() :
                      (double)std::numeric_limits<Tout>::max() <= inval ?
                      std::numeric_limits<Tout>::max() :
                      static_cast<Tout>(inval));
        }
        /// Round the incoming value to the smallest output value that is
        /// no less than the input.  Both Tin and Tout must be elementary
        /// data types, and Tout must be an elementary integral type.
        template <typename Tin, typename Tout>
        void round_up(const Tin& inval, Tout& outval) {
            outval = (std::numeric_limits<Tout>::min() >= inval ?
                      std::numeric_limits<Tout>::min() :
                      (double) std::numeric_limits<Tout>::max() < inval ?
                      std::numeric_limits<Tout>::max() :
                      static_cast<Tout>(inval) +
                      ((inval-static_cast<Tin>(static_cast<Tout>(inval))) > 0));
        }
        /// A specialization of round_up for the output type float.
        template <typename Tin>
        void round_up(const Tin& inval, float&);
        /// A specialization of round_up for the output in double.
        template <typename Tin>
        void round_up(const Tin& inval, double& outval) {
            outval = static_cast<double>(inval);
        }

        /// Log_2 of a 32-bit integer.
        inline int log2(uint32_t x) {
            uint32_t xx, xxx;
            return (xx = x >> 16)
                ? (xxx = xx >> 8) ? 24 + log2table[xxx] : 16 + log2table[xx]
                : (xxx = x >> 8) ? 8 + log2table[xxx] : log2table[x];
        }
        /// Log_2 of a 64-bit integer.
        inline int log2(uint64_t x) {
            uint32_t xx;
            return (xx = x >> 32)
                ? 32 + log2(xx)
                : log2(static_cast<uint32_t>(x));
        }
        ///@}

        FASTBIT_CXX_DLLSPEC void
        logMessage(const char* event, const char* fmt, ...);
        FASTBIT_CXX_DLLSPEC FILE* getLogFile();
        int writeLogFileHeader(FILE *fptr, const char* fname);
        FASTBIT_CXX_DLLSPEC void closeLogFile();
        FASTBIT_CXX_DLLSPEC int setLogFileName(const char* filename);
        FASTBIT_CXX_DLLSPEC const char* getLogFileName();

        FASTBIT_CXX_DLLSPEC bool strMatch(const char* str, const char* pat);
        FASTBIT_CXX_DLLSPEC bool nameMatch(const char* str, const char* pat);

        /// Compute the outer product of @c a and @c b, add the result to @c c.
        const ibis::bitvector64& outerProduct(const ibis::bitvector& a,
                                              const ibis::bitvector& b,
                                              ibis::bitvector64& c);
        /// Add the strict upper triangular portion of the outer production
        /// between @c a and @c b to @c c.
        const ibis::bitvector64& outerProductUpper(const ibis::bitvector& a,
                                                   const ibis::bitvector& b,
                                                   ibis::bitvector64& c);

        /// Intersect two sets of bit vectors.
        long intersect(const std::vector<ibis::bitvector> &bits1,
                       const std::vector<ibis::bitvector> &bits2,
                       std::vector<ibis::bitvector> &res);
        /// Intersect three sets of bit vectors.
        long intersect(const std::vector<ibis::bitvector> &bits1,
                       const std::vector<ibis::bitvector> &bits2,
                       const std::vector<ibis::bitvector> &bits3,
                       std::vector<ibis::bitvector> &res);
        void clear(ibis::array_t<ibis::bitvector*> &bv) throw();
        void clear(ibis::partList &pl) throw();
        void updateDatasets(void);
        void emptyCache(void);

        /// Return a pointer to the string designating the version of this
        /// software.
        inline const char* getVersionString() {
            return FASTBIT_STRING;
        }
        /// Return an integer designating the version of this software.
        /// The version number is composed of four segments each with two
        /// decimal digits.  For example, version 1.3.0.2 will be
        /// represented as 1030002.  The stable releases typically have
        /// the last segment as zero, which is generally referred to
        /// without the last ".0".
        inline int getVersionNumber() {
#ifdef FASTBIT_IBIS_INT_VERSION
            return FASTBIT_IBIS_INT_VERSION;
#else
            return 1030000;
#endif
        }

        /// Return the current time in string format as @c asctime_r.
        void getLocalTime(char *str);
        /// Return the current GMT time in string format.
        void getGMTime(char *str);
        void secondsToString(const time_t, char *str);


#if defined(WIN32) && ! defined(__CYGWIN__)
        char* getpass_r(const char *prompt, char *buffer, uint32_t buflen);
        char* getpass(const char* prompt);
#else
        inline char *itoa(int value, char *str, int /* radix */) {
            sprintf(str,"%d",value);
            return str;
        }
#endif

        /// A template to clean up a vector of pointers.
        template <typename T> inline void
        clearVec(std::vector<T*> &v) {
            const size_t nv = v.size();
            for (size_t j = 0; j < nv; ++j)
                delete v[j];
            v.clear();
        } // clearVec

        /// A class for logging messages.  The caller writes message to a
        /// std::ostream returned by the function buffer as if to
        /// std::cout.  Note that messages are stored in this buffer and
        /// written out in the destructor of this class.  There is a macro
        /// LOGGER that can simplify some of the routine stuff.  Use
        /// function ibis::util::setLogFile to explicit name of the log
        /// file or use RC file entry logfile to specify a file name.  By
        /// default all messages are dump to stdout.
        class FASTBIT_CXX_DLLSPEC logger {
        public:
            /// Constructor.
            logger(int blanks=0);
            /// Destructor.
            ~logger();
            /// Return an output stream for caller to build a message.
            std::ostream& operator()(void) {return mybuffer;}
            std::string str() const;
            const char* c_str() const;

        protected:
            /// The message is stored in this buffer.
            std::ostringstream mybuffer;

        private:
            logger(const logger&);
            logger& operator=(const logger&);
        }; // logger

        /// A global I/O lock.  All ioLock objects share the same
        /// underlying pthread_mutex lock.
        class ioLock {
        public:
            ioLock() {
#if defined(PTW32_STATIC_LIB)
                if (mutex == PTHREAD_MUTEX_INITIALIZER) {
                    int ierr = pthread_mutex_init(&mutex, 0);
                    if (ierr != 0)
                        throw "ioLock failed to initialize the necessary mutex";
                }
#endif
                if (0 != pthread_mutex_lock(&mutex))
                    throw "ioLock failed to obtain a lock";
            }
            ~ioLock() {
                (void) pthread_mutex_unlock(&mutex);
            }
        private:
            // every instantiation of this class locks on the same mutex
            static pthread_mutex_t mutex;

            ioLock(const ioLock&) {}; // can not copy
            ioLock& operator=(const ioLock&);
        };

        /// An wrapper class for perform pthread_mutex_lock/unlock.
        class mutexLock {
        public:
            mutexLock(pthread_mutex_t* lk, const char* m)
                : mesg(m), lock(lk) {
                LOGGER(ibis::gVerbose > 10)
                    << "util::mutexLock -- acquiring lock (" << lock
                    << ") for " << mesg;
                if (0 != pthread_mutex_lock(lock)) {
                    throw "mutexLock failed to obtain a lock";
                }
            }
            ~mutexLock() {
                LOGGER(ibis::gVerbose > 10)
                    << "util::mutexLock -- releasing lock (" << lock
                    << ") for " << mesg;
                (void) pthread_mutex_unlock(lock);
            }

        private:
            const char *mesg;
            pthread_mutex_t *lock;

            mutexLock() : mesg(0), lock(0) {}; // no default constructor
            mutexLock(const mutexLock&); // can not copy
            mutexLock& operator=(const mutexLock&);
        }; // mutexLock

        /// An wrapper class for perform pthread_mutex_lock/unlock.  Avoid
        /// invoking ibis::util::logMessage so it can be used inside
        /// ibis::util::logMessage.
        class quietLock {
        public:
            /// Constructor.
            quietLock(pthread_mutex_t *lk) : lock(lk) {
                if (0 != pthread_mutex_lock(lock))
                    throw "quietLock failed to obtain a mutex lock";
            }
            /// Destructor.
            ~quietLock() {
                (void) pthread_mutex_unlock(lock);
            }

        private:
            /// The pointer to the mutex object.
            pthread_mutex_t *lock;

            quietLock(); // no default constructor
            quietLock(const quietLock&); // can not copy
            quietLock& operator=(const quietLock&);
        }; // quietLock

        /// An wrapper class for perform pthread_mutex_trylock/unlock.  It
        /// does not use ibis::util::logMessage.
        class softLock {
        public:
            /// Constructor.
            softLock(pthread_mutex_t *lk)
                : lock_(lk), locked_(pthread_mutex_trylock(lock_)) {}
            /// Has a mutex lock being acquired?  Returns true if yes,
            /// otherwise false.
            bool isLocked() const {return (locked_==0);}
            /// Destructor.
            ~softLock() {
                (void) pthread_mutex_unlock(lock_);
            }

        private:
            /// Pointer to the mutex lock object.
            pthread_mutex_t *lock_;
            /// The return value from pthread_mutex_trylock.
            const int locked_;

            softLock(); // no default constructor
            softLock(const softLock&); // can not copy
            softLock& operator=(const softLock&);
        }; // softLock

        /// An wrapper class for perform pthread_rwlock_rdlock/unlock.
        class readLock {
        public:
            readLock(pthread_rwlock_t* lk, const char* m)
                : mesg(m), lock(lk) {
                if (0 != pthread_rwlock_rdlock(lock)) {
                    throw "readLock failed to obtain a lock";
                }
            }
            ~readLock() {
                (void) pthread_rwlock_unlock(lock);
            }

        private:
            const char *mesg;
            pthread_rwlock_t *lock;

            readLock() : mesg(0), lock(0) {}; // no default constructor
            readLock(const readLock&); // can not copy
            readLock& operator=(const readLock&);
        }; // readLock

        /// An wrapper class for perform pthread_rwlock_wrlock/unlock.
        class writeLock {
        public:
            /// Constructor.
            writeLock(pthread_rwlock_t* lk, const char* m)
                : mesg(m), lock(lk) {
                if (0 != pthread_rwlock_wrlock(lock)) {
                    throw "writeLock failed to obtain a lock";
                }
            }
            /// Destructor.
            ~writeLock() {
                (void) pthread_rwlock_unlock(lock);
            }

        private:
            const char *mesg;
            pthread_rwlock_t *lock;

            writeLock() : mesg(0), lock(0) {}; // no default constructor
            writeLock(const writeLock&); // can not copy
            writeLock& operator=(const writeLock&);
        }; // writeLock

        /// A simple shared counter.  Each time the operator() is called,
        /// it is incremented by 1.  Calls from different threads are
        /// serialized through a mutual exclusion lock or an atomic
        /// operation.  Currently, it only knows about atomic operations
        /// provided by GCC and visual studio on WIN32.  The GCC automic
        /// functions are determined in the configure script.
        class FASTBIT_CXX_DLLSPEC counter {
        public:
            ~counter() {
#if defined(HAVE_GCC_ATOMIC32)
#elif defined(HAVE_WIN_ATOMIC32)
#else
                (void)pthread_mutex_destroy(&lock_);
#endif
            }
            counter() : count_(0) {
#if defined(HAVE_GCC_ATOMIC32)
#elif defined(HAVE_WIN_ATOMIC32)
#else
                if (0 != pthread_mutex_init(&lock_, 0))
                    throw ibis::bad_alloc
                        ("util::counter failed to initialize mutex lock");
#endif
            }

            /// Return the current count and increment the count.
            uint32_t operator()() {
#if defined(HAVE_GCC_ATOMIC32)
                return __sync_fetch_and_add(&count_, 1);
#elif defined(HAVE_WIN_ATOMIC32)
                return InterlockedIncrement((volatile long *)&count_)-1;
#else
                ibis::util::quietLock lck(&lock_);
                uint32_t ret = count_;
                ++ count_;
                return ret;
#endif
            }
            /// Reset count to zero.
            void reset() {
#if defined(HAVE_GCC_ATOMIC32)
                (void) __sync_fetch_and_sub(&count_, count_);
#elif defined(HAVE_WIN_ATOMIC32)
                (void) InterlockedExchange((volatile long *)&count_, 0);
#else
                ibis::util::quietLock lck(&lock_);
                count_ = 0;
#endif
            }
            /// Return the current count value.
            uint32_t value() const {
                return count_;
            }

        private:
#if defined(HAVE_GCC_ATOMIC32)
#elif defined(HAVE_WIN_ATOMIC32)
#else
            mutable pthread_mutex_t lock_; ///!< The mutex lock.
#endif
            volatile uint32_t count_; ///!< The counter value.

            /// Copy constructor.  Decleared but not implemented.
            counter(const counter&);
            /// Assignment operator.  Decleared but not implemented.
            counter& operator=(const counter&);
        }; // counter

        /// A shared unsigned 32-bit integer class.  Multiply threads may
        /// safely perform different operations on this integer at the same
        /// time.  It serializes the operations by using the atomic
        /// operations provided by GCC extension.  The availability of
        /// automic operations is indicated by whether or not the compiler
        /// macro HAVE_GCC_ATOMIC32 is defined.  If the atomic extension is
        /// not available, it falls back on the mutual exclusion lock
        /// provided by pthread library.
        ///
        /// @note The overhead of using mutual exclusion lock is large.  In
        /// one test that acquires and release three locks a million time
        /// each, using the locks took about 10 seconds, while using the
        /// atomic extension to perform the same arithmetic operations took
        /// about 0.1 seconds.
        class FASTBIT_CXX_DLLSPEC sharedInt32 {
        public:
            sharedInt32() : val_(0) {
#if defined(HAVE_ATOMIC_TEMPLATE)
#elif defined(HAVE_GCC_ATOMIC32)
#elif defined(HAVE_WIN_ATOMIC32)
#else
                if (pthread_mutex_init(&mytex, 0) != 0)
                    throw "pthread_mutex_init failed for sharedInt";
#endif
            }

            ~sharedInt32() {
#if defined(HAVE_ATOMIC_TEMPLATE)
#elif defined(HAVE_GCC_ATOMIC32)
#elif defined(HAVE_WIN_ATOMIC32)
#else
                (void)pthread_mutex_destroy(&mytex);
#endif
            }

            /// Read the current value.
            uint32_t operator()() const {
#if defined(HAVE_ATOMIC_TEMPLATE)
                return val_.load();
#elif defined(HAVE_GCC_ATOMIC32)
                return __sync_add_and_fetch(const_cast<uint32_t*>(&val_), 0);
#elif defined(HAVE_WIN_ATOMIC32)
                return val_;
#else
                ibis::util::quietLock lck(const_cast<pthread_mutex_t*>(&mytex));
                return val_;
#endif
            }

            /// Increment operator.
            uint32_t operator++() {
#if defined(HAVE_ATOMIC_TEMPLATE)
                ++ val_;
                return val_.load();
#elif defined(HAVE_GCC_ATOMIC32)
                return __sync_add_and_fetch(&val_, 1);
#elif defined(HAVE_WIN_ATOMIC32)
                return InterlockedIncrement((volatile long *)&val_);
#else
                ibis::util::quietLock lock(&mytex);
                ++ val_;
                return val_;
#endif
            }

            /// Decrement operator.
            uint32_t operator--() {
#if defined(HAVE_ATOMIC_TEMPLATE)
                -- val_;
                return val_.load();
#elif defined(HAVE_GCC_ATOMIC32)
                return __sync_sub_and_fetch(&val_, 1);
#elif defined(HAVE_WIN_ATOMIC32)
                return InterlockedDecrement((volatile long *)&val_);
#else
                ibis::util::quietLock lock(&mytex);
                -- val_;
                return val_;
#endif
            }

            /// In-place addition operator.
            void operator+=(const uint32_t rhs) {
#if defined(HAVE_ATOMIC_TEMPLATE)
                (void) val_.fetch_add(rhs);
#elif defined(HAVE_GCC_ATOMIC32)
                (void) __sync_add_and_fetch(&val_, rhs);
#elif defined(HAVE_WIN_ATOMIC32)
                (void) InterlockedExchangeAdd((volatile long *)&val_, rhs);
#else
                ibis::util::quietLock lock(&mytex);
                val_ += rhs;
#endif
            }

            /// In-place subtraction operator.
            void operator-=(const uint32_t rhs) {
#if defined(HAVE_ATOMIC_TEMPLATE)
                (void) val_.fetch_sub(rhs);
#elif defined(HAVE_GCC_ATOMIC32)
                (void) __sync_sub_and_fetch(&val_, rhs);
#elif defined(HAVE_WIN_ATOMIC32)
                (void) InterlockedExchangeAdd((volatile long *)&val_,
                                              -(long)rhs);
#else
                ibis::util::quietLock lock(&mytex);
                val_ -= rhs;
#endif
            }

//             /// Swap the contents of two integer variables.
//             void swap(sharedInt32 &rhs) {
// #if defined(HAVE_GCC_ATOMIC32)
//                 uint32_t tmp = rhs.val_;
//                 rhs.val_ = val_;
//                 val_ = tmp;
// #elif defined(HAVE_WIN_ATOMIC32)
//                 uint32_t tmp = rhs.val_;
//                 rhs.val_ = val_;
//                 val_ = tmp;
// #else
//                 ibis::util::quietLock lock(&mytex);
//                 uint32_t tmp = rhs.val_;
//                 rhs.val_ = val_;
//                 val_ = tmp;
// #endif
//             }

        private:
#if defined(HAVE_ATOMIC_TEMPLATE)
            /// The actual integer value is encapsulated by std::atomic
            std::atomic<uint32_t> val_;
#elif defined(HAVE_GCC_ATOMIC32)
            /// The actual integer value.
            uint32_t volatile val_;
#elif defined(HAVE_WIN_ATOMIC32)
            /// The actual integer value.
            uint32_t volatile val_;
#else
            /// The actual integer value.
            uint32_t volatile val_;
            /// The mutex for this object.
            pthread_mutex_t mytex;
#endif

            sharedInt32(const sharedInt32&); // no copy constructor
            sharedInt32& operator=(const sharedInt32&); // no assignment
        }; // sharedInt32

        /// A unsigned 64-bit shared integer class.  It allows multiple
        /// threads to safely operate on the integer through the use of the
        /// atomic operations provided by GCC extension.  If the atomic
        /// extension is not available, it falls back on the mutual
        /// exclusion lock provided by pthread library.  @sa
        /// ibis::util::sharedInt32
        class sharedInt64 {
        public:
            sharedInt64() : val_(0) {
#if defined(HAVE_ATOMIC_TEMPLATE)
#elif defined(HAVE_GCC_ATOMIC64)
#elif defined(HAVE_WIN_ATOMIC64)
#else
                if (pthread_mutex_init(&mytex, 0) != 0)
                    throw "pthread_mutex_init failed for sharedInt";
#endif
            }

            ~sharedInt64() {
#if defined(HAVE_ATOMIC_TEMPLATE)
#elif defined(HAVE_GCC_ATOMIC64)
#elif defined(HAVE_WIN_ATOMIC64)
#else
                (void)pthread_mutex_destroy(&mytex);
#endif
            }

            /// Read the current value.
            uint64_t operator()() const {
#if defined(HAVE_ATOMIC_TEMPLATE)
                return val_.load();
#elif defined(HAVE_GCC_ATOMIC64)
                return __sync_add_and_fetch(const_cast<uint64_t*>(&val_), 0);
#elif defined(HAVE_WIN_ATOMIC64)
                return val_;
#else
                ibis::util::quietLock lck(const_cast<pthread_mutex_t*>(&mytex));
                return val_;
#endif
            }

            /// Increment operator.
            uint64_t operator++() {
#if defined(HAVE_ATOMIC_TEMPLATE)
                ++ val_;
                return val_.load();
#elif defined(HAVE_GCC_ATOMIC64)
                return __sync_add_and_fetch(&val_, 1);
#elif defined(HAVE_WIN_ATOMIC64)
                return InterlockedIncrement64((volatile LONGLONG *)&val_);
#else
                ibis::util::quietLock lock(&mytex);
                ++ val_;
                return val_;
#endif
            }

            /// Decrement operator.
            uint64_t operator--() {
#if defined(HAVE_ATOMIC_TEMPLATE)
                -- val_;
                return val_.load();
#elif defined(HAVE_GCC_ATOMIC64)
                return __sync_sub_and_fetch(&val_, 1);
#elif defined(HAVE_WIN_ATOMIC64)
                return InterlockedDecrement64((volatile LONGLONG *)&val_);
#else
                ibis::util::quietLock lock(&mytex);
                -- val_;
                return val_;
#endif
            }

            /// In-place addition operator.
            void operator+=(const uint64_t rhs) {
#if defined(HAVE_ATOMIC_TEMPLATE)
                (void) val_.fetch_add(rhs);
#elif defined(HAVE_GCC_ATOMIC64)
                (void) __sync_add_and_fetch(&val_, rhs);
#elif defined(HAVE_WIN_ATOMIC64)
                (void) InterlockedExchangeAdd64((volatile LONGLONG *)&val_,
                                                rhs);
#else
                ibis::util::quietLock lock(&mytex);
                val_ += rhs;
#endif
            }

            /// In-place subtraction operator.
            void operator-=(const uint64_t rhs) {
#if defined(HAVE_ATOMIC_TEMPLATE)
                (void) val_.fetch_sub(rhs);
#elif defined(HAVE_GCC_ATOMIC64)
                (void) __sync_sub_and_fetch(&val_, rhs);
#elif defined(HAVE_WIN_ATOMIC64)
                (void) InterlockedExchangeAdd64((volatile LONGLONG *)&val_,
                                                -(long)rhs);
#else
                ibis::util::quietLock lock(&mytex);
                val_ -= rhs;
#endif
            }

//             /// Swap the contents of two integer variables.
//             void swap(sharedInt64 &rhs) {
// #if defined(HAVE_GCC_ATOMIC64)
//                 uint64_t tmp = rhs.val_;
//                 rhs.val_ = val_;
//                 val_ = tmp;
// #elif defined(HAVE_WIN_ATOMIC64)
//                 uint64_t tmp = rhs.val_;
//                 rhs.val_ = val_;
//                 val_ = tmp;
// #else
//                 ibis::util::quietLock lock(&mytex);
//                 uint64_t tmp = rhs.val_;
//                 rhs.val_ = val_;
//                 val_ = tmp;
// #endif
//             }

        private:
#if defined(HAVE_ATOMIC_TEMPLATE)
            /// The actual value is encapsulated in std::atomic
            std::atomic<uint64_t> val_;
#elif defined(HAVE_GCC_ATOMIC64)
            /// The actual integer value.
            uint64_t volatile val_;
#elif defined(HAVE_WIN_ATOMIC64)
            /// The actual integer value.
            uint64_t volatile val_;
#else
            /// The actual integer value.
            uint64_t volatile val_;
            /// The mutex for this object.
            pthread_mutex_t mytex;
#endif

            sharedInt64(const sharedInt64&); // no copy constructor
            sharedInt64& operator=(const sharedInt64&); // no assignment
        }; // sharedInt64

        /// Print simple timing information.  It starts the clock in the
        /// constructor, stops the clock in the destructor, and reports the
        /// CPU time and elapsed time in between.  Typically one would
        /// declare an object of this class in a block of code, and let the
        /// object be cleaned up by compiler generated code at the end of
        /// its scope.  Upon destruction of this object, it prints its
        /// lifespan.  To distiguish the different time durations, the user
        /// should provide a meaningful description to the constructor.
        class timer {
        public:
            explicit timer(const char* msg, int lvl=1);
            ~timer();

        private:
            ibis::horometer *chrono_; ///!< The actual timer object.
            std::string mesg_; ///!< Holds a private copy of the message.

            timer(); // no default constructor
            timer(const timer&); // no copying
            timer& operator=(const timer&); // no assignment
        }; // timer

        /// A template to hold a reference to an object.
        template <class T> class refHolder {
        public:
            refHolder(T& r) : ref_(r) {}
            operator T& () const {return ref_;}

        private:
            T& ref_;

            refHolder();
        }; // refHolder

        /// A function template to produce refHolder.
        template <class T>
        inline refHolder<T> ref(T& r) {return refHolder<T>(r);}

        /// A class hierarchy for cleaning up after durable resources.  It
        /// follows the example set by Loki::ScopeGuard, but simpler.
        class guardBase {
        public:
            /// Tell the guard that it does not need to invoke clean up
            /// function any more.
            void dismiss() const {done_ = true;}

        protected:
            mutable volatile bool done_;

            /// Destructor.  No need to be virtual.
            ~guardBase() {};
            guardBase() : done_(false) {}; ///!< Default constructor.
            /// Copy constructor.  Allows all derived classes to use the
            /// compiler generated copy constructors.
            guardBase(const guardBase& rhs) : done_(rhs.done_) {
                rhs.dismiss();
            }

            /// A template to invoke the function registered.  Also absorbs
            /// all exceptions.
            template <typename T>
            static void cleanup(T& task) throw () {
                try {
                    if (!task.done_)
                        task.execute();
                }
                catch (const std::exception& e) {
                    LOGGER(ibis::gVerbose > 1)
                        << " ... caught a std::exception (" << e.what()
                        << ") in util::gard";
                }
                catch (const char* s) {
                    LOGGER(ibis::gVerbose > 1)
                        << " ... caught a string exception (" << s
                        << ") in util::guard";
                }
                catch (...) {
                    LOGGER(ibis::gVerbose > 1)
                        << " ... caught a unknown exception in util::guard";
                }
                task.done_ = true;
            }
        }; // guardBase

        /// The type to be used by client code.  User code uses type
        /// ibis::util::guard along with the overloaded function
        /// ibis::util::makeGuard, as in
        /// @code
        /// ibis::util::guard myguard = ibis::util::makeGuard...;
        /// @endcode
        ///
        /// @note The parameters passed to the function that does the
        /// actual clean up jobs are taken as they are at the construction
        /// time.  If such parameters are modified, the caller needs to
        /// either create the guard variable after the parameters take on
        /// their final values, or dismiss the old gard and create another
        /// one.
        typedef const guardBase& guard;

        /// A concrete class for cleanup jobs that take a function without
        /// any argument.
        template <typename F>
        class guardImpl0 : public guardBase {
        public:
            static guardImpl0<F> makeGuard(F f) {
                return guardImpl0<F>(f);
            }

            /// Destructor calls the cleanup function of the base class.
            ~guardImpl0() {cleanup(*this);}

        protected:
            friend class guardBase; // to call function execute
            void execute() {fun_();}

            /// Construct a guard object from a function.
            explicit guardImpl0(F f) : fun_(f) {}

        private:
            /// Copy of the function pointer.
            F fun_;

            guardImpl0();
            guardImpl0& operator=(const guardImpl0&);
        }; // guardImpl0

        template <typename F>
        inline guardImpl0<F> makeGuard(F f) {
            return guardImpl0<F>::makeGuard(f);
        }

        /// A concrete class for cleanup jobs that take a function with one
        /// argument.
        template <typename F, typename A>
        class guardImpl1 : public guardBase {
        public:
            static guardImpl1<F, A> makeGuard(F f, A a) {
                return guardImpl1<F, A>(f, a);
            }

            /// Destructor calls the cleanup function of the base class.
            ~guardImpl1() {cleanup(*this);}

        protected:
            friend class guardBase; // to call function execute
            void execute() {fun_(arg_);}

            /// Construct a guard object from a function.
            explicit guardImpl1(F f, A a) : fun_(f), arg_(a) {}

        private:
            /// The function pinter.
            F fun_;
            /// The argument to the function.
            A arg_;

            guardImpl1();
            guardImpl1& operator=(const guardImpl1&);
        }; // guardImpl1

        template <typename F, typename A>
        inline guardImpl1<F, A> makeGuard(F f, A a) {
            return guardImpl1<F, A>::makeGuard(f, a);
        }

        /// A concrete class for cleanup jobs that take a function with two
        /// arguments.
        template <typename F, typename A1, typename A2>
        class guardImpl2 : public guardBase {
        public:
            static guardImpl2<F, A1, A2> makeGuard(F f, A1 a1, A2 a2) {
                return guardImpl2<F, A1, A2>(f, a1, a2);
            }

            /// Destructor calls the cleanup function of the base class.
            ~guardImpl2() {cleanup(*this);}

        protected:
            friend class guardBase; // to call function execute
            void execute() {fun_(arg1_, arg2_);}

            /// Construct a guard object from a function.
            explicit guardImpl2(F f, A1 a1, A2 a2)
                : fun_(f), arg1_(a1), arg2_(a2) {}

        private:
            /// The function pinter.
            F fun_;
            /// The argument 1 to the function.
            A1 arg1_;
            /// The argument 2 to the function.
            A2 arg2_;

            guardImpl2();
            //guardImpl2(const guardImpl2&);
            guardImpl2& operator=(const guardImpl2&);
        }; // guardImpl2

        template <typename F, typename A1, typename A2>
        inline guardImpl2<F, A1, A2> makeGuard(F f, A1 a1, A2 a2) {
            return guardImpl2<F, A1, A2>::makeGuard(f, a1, a2);
        }

        /// A class to work with class member functions with no arguments.
        template <class C, typename F>
        class guardObj0 : public guardBase {
        public:
            static guardObj0<C, F> makeGuard(C& o, F f) {
                return guardObj0<C, F>(o, f);
            }

            /// Desutructor.
            ~guardObj0() {cleanup(*this);}

        protected:
            friend class guardBase; // to call function execute
            void execute() {(obj_.*fun_)();}

            /// Constructor.
            guardObj0(C& o, F f) : obj_(o), fun_(f) {}

        private:
            C& obj_; ///!< A reference to the class object.
            F fun_;  ///!< A pointer to the member function.

            guardObj0();
            guardObj0& operator=(const guardObj0&);
        }; // guardObj0

        template <class C, typename F>
        inline guardObj0<C, F> objectGuard(C o, F f) {
            return guardObj0<C, F>::makeGuard(o, f);
        }

#if defined(HAVE_FLOCK)
        /// A simple wrapper on flock.
        class flock {
        public:
            /// Constructor.  Take a file descriptor of type int, created
            /// by function open.
            flock(int fd)
                : fd_(fd),
                  locked(0 == ::flock(fd, LOCK_EX|LOCK_NB)) {
            }
            /// Destructor.
            ~flock() {
                if (locked)
                    (void) ::flock(fd_, LOCK_UN);
            }
            /// Was a lock acquired successfully?  Returns true for yes,
            /// otherwise no.
            bool isLocked() const {return locked;}

        private:
            const int  fd_;
            const bool locked;
        }; // FLock
#endif
    } // namespace util
} // namespace ibis

#if defined(WIN32) && ! defined(__CYGWIN__)
char* getpass(const char* prompt);
#endif

/// A very simple pseudo-random number generator.  It produces a floating
/// point number between 0 and 1.
///
/// The is a a Linear Congruential pseudo-random number generator.  It
/// produces a floating-point in the range of (0, 1).  It is very simple
/// and fast, however, it does not produce high-quality random numbers.  It
/// is not thread-safe.  However, since the actual computation only
/// involves two arithmetic operations, it is very unlikely to have
/// thread-safty issues.
inline double ibis::util::rand() {
    // The internal variable @c seed is always an odd number.  Don't use it
    // directly.
    static uint32_t seed = 1;
    static const uint32_t alpha = 69069;
    static const double scale = ::pow(0.5, 32);
    seed = static_cast<uint32_t>(seed * alpha);
    return(scale * seed);
} // ibis::util::rand

/// Fletcher's checksum on two integers.  Returns an integer.
inline uint32_t ibis::util::checksum(uint32_t a, uint32_t b) {
    uint32_t a0 = (a >> 16);
    uint32_t a1 = (a & 0xFFFF);
    uint32_t b0 = (b >> 16);
    uint32_t b1 = (b & 0xFFFF);
    return ((((a0<<2)+a1*3+(b0<<1)+b1) << 16) | ((a0+a1+b0+b1) & 0xFFFF));
} // ibis::util::checksum

/// Increment the input value to the next larger value.  If the math
/// library has nextafter, it will use nextafter, otherwise, it will use
/// the unit round-off error to compute the next larger value.  The success
/// of this computation is highly sensitive to the definition of
/// DBL_EPSILON, which should be the smallest value x such that (1+x) is
/// different from x.  For 64-bit IEEE floating-point number, it is
/// approximately 2.2E-16 (2^{-52}).
inline double ibis::util::incrDouble(const double& in) {
#if defined(HAVE_NEXTAFTER)
    return nextafter(in, DBL_MAX);
#elif defined(_MSC_VER) && defined(_WIN32)
    return _nextafter(in, DBL_MAX);
#else
    double tmp = fabs(in) * DBL_EPSILON;
    if (tmp > 0.0) tmp += in;
    else tmp = in + DBL_MIN;
    return tmp;
#endif
}

/// Decrease the input value to the next smaller value.
/// @sa ibis::util::incrDouble
inline double ibis::util::decrDouble(const double& in) {
#if defined(HAVE_NEXTAFTER)
    return nextafter(in, -DBL_MAX);
#elif defined(_MSC_VER) && defined(_WIN32)
    return _nextafter(in, -DBL_MAX);
#else
    double tmp = fabs(in) * DBL_EPSILON;
    if (tmp > 0.0) tmp = in - tmp;
    else tmp = in - DBL_MIN;
    return tmp;
#endif
}

/// Generate a range [left, right) that contains exactly the input value
/// in.  This is used to transform an expression expression "A = in" into
/// "left <= A < right".
/// @sa ibis::util::incrDouble
inline void ibis::util::eq2range(const double& in,
                                 double& left, double& right) {
#if defined(HAVE_NEXTAFTER)
    right = nextafter(in, DBL_MAX);
#elif defined(_MSC_VER) && defined(_WIN32)
    right = _nextafter(in, DBL_MAX);
#else
    double tmp = fabs(in) * DBL_EPSILON;
    if (tmp > 0.0) {right = in + tmp;}
    else {right = in + DBL_MIN;}
#endif
    left = in;
} // ibis::util::eq2range

/// This function uses nextafterf if the macro HAVE_NEXTAFTER is defined,
/// otherwise it uses FLT_EPSILON to compute outval as
/// (float)(inval)*(1+FLT_EPSILON).
template <typename Tin>
inline void ibis::util::round_up(const Tin& inval, float& outval) {
    // perform the initial rounding
    outval = static_cast<float>(inval);
    if (static_cast<Tin>(outval) < inval) {
        // if the rounded value is less than the input value, compute the
        // next value
#if defined(HAVE_NEXTAFTER)
        outval = nextafterf(static_cast<float>(inval), FLT_MAX);
#else
        float tmp = fabsf(outval) * FLT_EPSILON;
        if (tmp > 0.0) outval += tmp;
        else outval += FLT_MIN;
#endif
    }
} // ibis::util::round_up

// remove all the trailing char 'tail'
inline void ibis::util::removeTail(char* str, char tail) {
    if (str != 0 && *str != 0) {
        char *tmp = str;
        while (*tmp != 0) ++ tmp;
        -- tmp;
        while (tmp > str && *tmp == tail) {
            *tmp = static_cast<char>(0);
            -- tmp;
        }
    }
} // ibis::util::removeTail

// remove the leading and trailing space of the incoming string
inline char* ibis::util::trim(char* str) {
    char* head = 0;
    if (str == 0) return head;
    if (*str == 0) return head;

    head = str;
    while (*head) {
        if (std::isspace(*head))
            ++ head;
        else
            break;
    }
    if (*head == 0)
        return head;

    for (str = head; *str != 0; ++ str);
    -- str;
    while (str >= head && std::isspace(*str)) {
        *str = static_cast<char>(0);
        -- str;
    }
    return head;
} // ibis::util::trim

/// In an attempt to compute small values more consistently, small values
/// are computed through division of integer values.  Since these integer
/// values are computed through the function pow, the accuracy of the
/// results depend on the implementation of the math library.
///
/// The value zero is always rounded to zero.   Incoming value less than
/// 1E-300 or greater than 1E300 is rounded to zero.
inline double ibis::util::coarsen(const double in, unsigned prec) {
    double ret;
    if (prec > 15) {
        ret = in;
    }
    else if (in == 0.0) {
        ret = in;
    }
    else {
        ret = fabs(in);
        if (ret < DBL_MIN) { // denormalized number --> 0
            ret = 0.0;
        }
        else if (ret < DBL_MAX) { // normal numbers
            ret = log10(ret);
            if (prec > 0)
                -- prec;
            const int ixp = static_cast<int>(floor(ret)) -
                static_cast<int>(prec);
            ret = floor(0.5 + pow(1e1, ret-ixp));
            if (ixp > 0)
                ret *= pow(1e1, ixp);
            else if (ixp < 0)
                ret /= pow(1e1, -ixp);
            if (in < 0.0)
                ret = -ret;
        }
        else {
            ret = in;
        }
    }
    return ret;
} // ibis::util::coarsen

/// Print a rid_t to an output stream.
inline std::ostream& operator<<(std::ostream& out, const ibis::rid_t& rid) {
    out << '(' << rid.num.run << ", " << rid.num.event << ')';
    return out;
}

/// Read a rid_t from an input stream.
inline std::istream& operator>>(std::istream& is, ibis::rid_t& rid) {
    char c = 0;
    is >> c;
    if (c == '(') { // (runNumber, EventNumber)
        is >> rid.num.run >> c;
        if (c == ',')
            is >> rid.num.event >> c;
        else
            rid.num.event = 0;
        if (c != ')')
            is.clear(std::ios::badbit); // forget the erro
    }
    else { // runNumber, EventNumber
        is.putback(c);
        is >> rid.num.run >> c;
        if (c != ',') // assume space separator
            is.putback(c);
        is >> rid.num.event;
    }
    return is;
}
#endif // IBIS_UTIL_H
