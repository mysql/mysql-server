/*
   Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef NDB_IMPORT_UTIL_HPP
#define NDB_IMPORT_UTIL_HPP

#include <ndb_global.h>
#include <stdint.h>
#include <ndb_limits.h>
#include <mgmapi.h>
#include <NdbMutex.h>
#include <NdbCondition.h>
#include <NdbHost.h>
#include <NdbThread.h>
#include <NdbSleep.h>
#include <NdbGetRUsage.h>
#include <OutputStream.hpp>
#include <NdbOut.hpp>
#include <NdbApi.hpp>
#include <NdbImport.hpp>
#ifdef _WIN32
#include <io.h>
#endif
// STL
#include <string>
#include <vector>
#include <map>
#include <algorithm>

typedef NdbImport::Error Error;

#undef TEST_NDBIMPORT
#ifdef TEST_NDBIMPORTUTIL
#define TEST_NDBIMPORT
#endif
#ifdef TEST_NDBIMPORTCSV
#define TEST_NDBIMPORT
#endif

#define logN(x, n) \
  do { \
    if (unlikely(m_util.c_opt.m_verbose >= n)) \
    { \
      NdbMutex_Lock(m_util.c_logmutex); \
      m_util.c_logtimer.stop(); \
      *(m_util.c_log) << *this \
                      << " " <<__LINE__ \
                      << " " << m_util.c_logtimer \
                      << ": " << x << endl; \
      NdbMutex_Unlock(m_util.c_logmutex); \
    } \
  } while (0)

#define log1(x) logN(x, 1)
#define log2(x) logN(x, 2)

#if defined(VM_TRACE) || defined(TEST_NDBIMPORT)
#define log3(x) logN(x, 3)
#define log4(x) logN(x, 4)
#else
#define log3(x)
#define log4(x)
#endif

#define Inval_uint (~(uint)0)
#define Inval_uint32 (~(uint32)0)
#define Inval_uint64 (~(uint64)0)

/*
 * Utilities class.  There is one Util instance attached to the
 * single Impl plus Csv instance.
 */

class NdbImportUtil {
public:
  NdbImportUtil();
  ~NdbImportUtil();
  NdbImportUtil& m_util;        // self

  // opt

  NdbImport::Opt c_opt;

  // allow changing options within a scope
  struct OptGuard {
    OptGuard(NdbImportUtil& util) :
      m_util(util) {
      m_opt_save = m_util.c_opt;
    }
    ~OptGuard() {
      m_util.c_opt = m_opt_save;
    }
    NdbImportUtil& m_util;
    NdbImport::Opt m_opt_save;
  };

  // name

  /*
   * Construct hierachical names where parts are separated by hyphens.
   * Used to name teams, workers, stats.
   */

  struct Name {
    Name(const char* s);
    Name(const char* s, const char* t);
    Name(const char* s, uint t);
    operator const char*() const {
      return m_str.c_str();
    };
    const char* str() const {
      return m_str.c_str();
    }
    std::string m_str;
  };

  // lockable

  struct Lockable {
    Lockable();
    ~Lockable();
    void lock();
    void unlock();
    void wait(uint timeout);
    void signal();
    NdbMutex* m_mutex;
    NdbCondition* m_condition;
  };

  // thread

  struct Thread : Lockable {
    Thread();
    ~Thread();
    void join();
    NdbThread* m_thread;
  };

  // list

  /*
   * Doubly-linked "intrusive" list, subclassed for various purposes.
   * For example List/ListEnt are subclassed as RowList/Row where
   * RowList stores only Row entries.
   *
   * For type-safety, subclasses of List use private inheritance and
   * define explicitly the methods needed.  This prevents putting an
   * entry on a wrong type of list.
   *
   * A list has optional associated stats under a given name.  These
   * can be extended by subclasses.
   */

  struct Stat;
  struct Stats;

  struct ListEnt {
    ListEnt();
    virtual ~ListEnt();
    ListEnt* m_next;
    ListEnt* m_prev;
  };

  struct List {
    List();
    virtual ~List();
    void set_stats(Stats& stats, const char* name);
    void push_back(ListEnt* ent);
    void push_front(ListEnt* ent);
    ListEnt* pop_front();
    void remove(ListEnt* ent);
    void push_back(List& list2);
#if defined(VM_TRACE) || defined(TEST_NDBIMPORTUTIL)
    void validate() const;
#else
    void validate() const {}
#endif
    ListEnt* m_front;
    ListEnt* m_back;
    uint m_cnt;
    uint m_maxcnt;
    uint64 m_totcnt;
    Stat* m_stat_occup;
    Stat* m_stat_total;
  };

  // attrs

  struct Row;

  struct Attr {
    Attr();
    void set_value(Row* row, const void* data, uint len) const;
    void set_blob(Row* row, const void* data, uint len) const;
    void set_null(Row* row, bool null) const;
    // used only for pseudo-tables, attrs are non-nullable
    const void* get_value(const Row* row) const;
    // avoid alignment crash, add required methods here
    void get_value(const Row* row, uint32& value) const {
      memcpy(&value, get_value(row), sizeof(value));
    }
    void get_value(const Row* row, uint64& value) const {
      memcpy(&value, get_value(row), sizeof(value));
    }
    bool get_null(const Row* row) const;
    uint get_blob_parts(uint len) const;
    void set_sqltype();
    std::string m_attrname;
    uint m_attrno;
    uint m_attrid;
    NdbDictionary::Column::Type m_type;
    CHARSET_INFO* m_charset;
    char m_sqltype[100];
    bool m_pk;
    bool m_nullable;
    uint m_precision;
    uint m_scale;
    uint m_length;
    uint m_charlength;
    NdbDictionary::Column::ArrayType  m_arraytype;
    uint m_inlinesize;
    uint m_partsize;
    const NdbDictionary::Table* m_blobtable;
    uint m_size;
    bool m_pad;
    uchar m_padchar;
    bool m_quotable;
    bool m_isblob;
    uint m_blobno;
    uint m_offset;
    uint m_null_byte;
    uint m_null_bit;
  };

  typedef std::vector<Attr> Attrs;

  // tables

  struct Table {
    Table();
    void add_pseudo_attr(const char* name,
                         NdbDictionary::Column::Type type,
                         uint length = 1);
    const Attr& get_attr(const char* attrname) const;
    uint get_nodeid(uint fragid) const;
    uint m_tabid;
    const NdbDictionary::Table* m_tab;
    const NdbRecord* m_rec;
    const NdbRecord* m_keyrec;
    uint m_rowsize;
    bool m_has_hidden_pk;
    Attrs m_attrs;
    std::vector<uint> m_blobids;
    // map fragid to nodeid
    std::vector<uint16> m_fragments;
  };

  // tables mapped by table id
  struct Tables {
    std::map<uint, Table> m_tables;
  };

  Tables c_tables;

  int add_table(NdbDictionary::Dictionary* dic,
                const NdbDictionary::Table* tab,
                uint& tabid,
                Error& error);
  const Table& get_table(uint tabid);

  // rows

  struct Blob;

  struct Row : ListEnt {
    Row();
    virtual ~Row();
    void init(const Table& table);
    uint m_tabid;
    uint m_rowsize;
    uint m_allocsize;
    uint64 m_rowid;
    uint64 m_linenr;    // file line number starting at 1
    uint64 m_startpos;
    uint64 m_endpos;
    uchar* m_data;
    std::vector<Blob*> m_blobs;
  };

  struct RowList : private List, Lockable {
    RowList();
    virtual ~RowList();
    void set_stats(Stats& stats, const char* name);
    bool push_back(Row* row);
    void push_back_force(Row* row);
    bool push_front(Row* row);
    Row* pop_front();
    void remove(Row* row);
    uint cnt() const {
      return m_cnt;
    }
    uint64 totcnt() const {
      return m_totcnt;
    }
    uint m_rowsize;     // sum from row entries
    uint m_rowbatch;    // limit m_cnt
    uint m_rowbytes;    // limit m_rowsize
    bool m_eof;         // producer team has stopped
    bool m_foe;         // consumer team has stopped
    uint64 m_overflow;
    uint64 m_underflow;
    Stat* m_stat_overflow;      // failed to push due to size limit
    Stat* m_stat_underflow;     // failed to pop due to empty
  };

  Row* alloc_row(const Table& Table);
  void free_row(Row* row);

  RowList* c_rows_free;

  // blobs

  struct Blob : ListEnt {
    Blob();
    virtual ~Blob();
    void resize(uint size);
    uint m_blobsize;
    uint m_allocsize;
    uchar* m_data;
  };

  struct BlobList : private List, Lockable {
    BlobList();
    virtual ~BlobList();
    void push_back(Blob* blob) {
      List::push_back(blob);
    }
    Blob* pop_front() {
      return static_cast<Blob*>(List::pop_front());
    }
  };

  Blob* alloc_blob();
  void free_blob(Blob* blob);

  BlobList* c_blobs_free;
 
  // rowmap

  /*
   * A processed row is a row that has been inserted or rejected
   * with permanent error.  Processed rows tend to form ranges which
   * merge together as processing continues.  A row map represents
   * such set of rows.  Shared access requires mutex so workers
   * should have private row maps merged periodically to a global
   * row map.  Contents of the row map are written to t1.map etc and
   * are used to implement a --resume function.
   */
  struct RowMap : Lockable {
    struct Range {
      bool operator<(const Range& r2) const {
        return m_start < r2.m_start;
      }
      uint64 m_start;           // starting rowid
      uint64 m_end;             // next rowid after
      uint64 m_startpos;        // byte offset of start in input
      uint64 m_endpos;          // byte offset of end in input
      uint64 m_reject;          // rejected rows (info only)
    };
    typedef std::vector<Range> Ranges;
    typedef Ranges::iterator Iterator;
    typedef Ranges::const_iterator ConstIterator;
    bool empty() {
      return m_ranges.empty();
    }
    void clear() {
      m_ranges.clear();
    }
    void add(const Row* row, bool reject) {
      Range r;
      r.m_start = row->m_rowid;
      r.m_end = row->m_rowid + 1;
      r.m_startpos = row->m_startpos;
      r.m_endpos = row->m_endpos;
      r.m_reject = (uint)reject;
      add(r);
    }
    void add(const Range r);
    void add(const RowMap& m);
    bool find(uint64 rowid, Iterator& itout);
    bool remove(uint64 rowid);
    bool merge_down(Range& rprev, Range r) {
      if (rprev.m_end == r.m_start)
      {
        rprev.m_end = r.m_end;
        rprev.m_endpos = r.m_endpos;
        rprev.m_reject += r.m_reject;
        return true;
      }
      require(rprev.m_end < r.m_start);
      return false;
    }
    bool merge_up(Range& rnext, Range r) {
      if (rnext.m_start == r.m_end)
      {
        rnext.m_start = r.m_start;
        rnext.m_startpos = r.m_startpos;
        rnext.m_reject += r.m_reject;
        return true;
      }
      require(rnext.m_start > r.m_end);
      return false;
    }
    void get_total(uint64& rows, uint64& reject) const;
    Ranges m_ranges;
  };

  // errormap

  /*
   * Count temporary errors per error code.  Any number of temporary
   * errors per db execution batch is counted as 1 on job level.
   * This is because usually individual transactions are not
   * responsible and all tend to fail with same error.
   */
  struct ErrorMap : Lockable {
    void clear() {
      m_map.clear();
    }
    uint size() const {
      return m_map.size();
    }
    uint get_sum() const {
      uint sum = 0;
      std::map<uint, uint>::const_iterator it;
      for (it = m_map.begin(); it != m_map.end(); it++)
        sum += it->second;
      return sum;
    }
    void add_one(uint key) {
      std::map<uint, uint>::iterator it;
      it = m_map.find(key);
      if (it != m_map.end())
        it->second += 1;
      else
        m_map.insert(std::pair<uint, uint>(key, 1));
    }
    void add_one(const ErrorMap& errormap) {
      std::map<uint, uint>::const_iterator it;
      for (it = errormap.m_map.begin(); it != errormap.m_map.end(); it++)
        add_one(it->first);
    }
    std::map<uint, uint> m_map;
  };

  // pseudo-tables

  static const uint g_result_tabid = 0xffff0000;
  static const uint g_reject_tabid = 0xffff0001;
  static const uint g_rowmap_tabid = 0xffff0002;
  static const uint g_stats_tabid = 0xffff0003;
  Table c_result_table;
  Table c_reject_table;
  Table c_rowmap_table;
  Table c_stats_table;

  void add_pseudo_tables();
  void add_result_table();
  void add_reject_table();
  void add_rowmap_table();
  void add_stats_table();

  void add_error_attrs(Table& table);

  void set_result_row(Row* row,
                      uint32 runno,
                      const char* name,
                      const char* desc,
                      uint64 rows,
                      uint64 reject,
                      uint64 temperrors,
                      uint64 runtime,
                      uint64 utime,
                      const Error& error);

  void set_reject_row(Row* row,
                      uint32 runno,
                      const Error& error,
                      const char* reject,
                      uint rejectlen);

  void set_rowmap_row(Row* row,
                      uint32 runno,
                      const RowMap::Range& range);

  void set_stats_row(Row* row,
                     uint32 runno,
                     const Stat& stat);

  void set_error_attrs(Row* row,
                       const Table& table,
                       const Error& error,
                       uint& id);

  // buf

  /*
   * Buffer for I/O and parsing etc.  A "split" buffer is divided
   * into upper and lower halves.  Lower half is for I/O but more
   * data can be added above it in upper half.  This is done in CSV
   * parsing to avoid splitting lines and fields between buffers.
   * The byte after data (at m_len) is valid and is set to NUL.
   */
  struct Buf {
    Buf(bool split = false);
    ~Buf();
    void alloc(uint pagesize, uint pagecnt);
    void copy(const uchar* src, uint len);
    void reset();
    int movetail(Buf& dst);     // move m_tail..m_len to another buffer
    const bool m_split;
    uchar* m_allocptr;
    uint m_allocsize;
    uchar* m_data;      // full data
    uint m_size;        // full size
    uint m_top;         // I/O top, zero or lower half if split
    uint m_start;       // data start
    uint m_tail;        // end of used data (relative to start)
    uint m_len;         // data length (relative to start)
    bool m_eof;         // mark eof on read
    // following are optional and are maintained by caller
    uint m_pos;         // current position (relative to start)
    uint m_lineno;      // current line number (starting at 0)
  };

  // dst should hold 5 times len
  static void pretty_print(char* dst, const void* ptr, uint len);

  // files

  struct File {
#ifndef _WIN32
    const static int Read_flags = O_RDONLY;
    const static int Write_flags = O_WRONLY | O_CREAT | O_TRUNC;
    const static int Append_flags = O_WRONLY | O_APPEND;
    const static int Creat_mode = 0644;
#else
    const static int Read_flags = _O_RDONLY | _O_BINARY;
    const static int Write_flags = _O_WRONLY | _O_CREAT | _O_TRUNC | _O_BINARY;
    const static int Append_flags = _O_WRONLY | _O_APPEND | _O_BINARY;
    const static int Creat_mode = _S_IREAD | _S_IWRITE;
#endif
    File(NdbImportUtil& util, Error& error);
    ~File();
    void set_path(const char* path) {
      m_path = path;
    }
    const char* get_path() {
      return m_path.c_str();
    }
    int do_open(int flags);
    int do_read(uchar* dst, uint size, uint& len);
    int do_read(Buf& buf);
    int do_write(const uchar* src, uint size);
    int do_write(const Buf& buf);
    int do_close();
    int do_seek(uint64 offset);
    NdbImportUtil& m_util;
    Error& m_error;
    std::string m_path;
    int m_fd;
    int m_flags;
  };

  // stats

  static const uint StatNULL = Inval_uint;

  /*
   * A stat entry is identified by name and has an id for fast
   * update access.  Child entries propagate their values to
   * the parent.  Entries with no children usually have a unique
   * source of values but this is not required.
   *
   * Many stats are updated under some mutex.  We do not use
   * std::atomic here.
   */
  struct Stat {
    Stat(Stats& stats,
         uint id,
         const char* name,
         uint parent,
         uint level,
         uint flags);
    void add(uint64 val);
    void reset();
    // meta
    Stats& m_stats;
    const uint m_id;
    const Name m_name;
    const uint m_parent;
    const uint m_level;
    const uint m_flags; // future use e.g. display options
    uint m_childcnt;
    uint m_firstchild;
    uint m_lastchild;
    uint m_nextchild;
    // data
    uint64 m_obs;       // observations
    uint64 m_sum;
    uint64 m_min;
    uint64 m_max;
    double m_sum1;      // m_sum summed as float
    double m_sum2;
  };

  struct Stats : Lockable {
    Stats(NdbImportUtil& util);
    ~Stats();
    Stat* create(const char* name, uint parent, uint flags);
    Stat* get(uint i);
    const Stat* get(uint i) const;
    Stat* find(const char* name) const;
    void add(uint id, uint64 val);
    const Stat* next(uint id) const;
    void reset();
#if defined(VM_TRACE) || defined(TEST_NDBIMPORTUTIL)
    struct Validate {
      Validate(uint parent, uint id, uint level, bool* seen) :
        m_parent(parent),
        m_id(id),
        m_level(level),
        m_seen(seen)
      {}
      uint m_parent;
      uint m_id;
      uint m_cnt;
      uint m_level;
      bool* m_seen;
    };
    void validate() const;
    const Stat* validate(Validate& v) const;
#else
    void validate() const {}
#endif
    NdbImportUtil& m_util;
    std::vector<Stat*> m_stats; // root at 0
  };

  Stats c_stats;

  // timer - like NdbTimer but we do not link to libndbNDBT

  struct Timer {
    Timer();
    void start();
    void stop();
    uint64 elapsed_sec() const;
    uint64 elapsed_msec() const;
    uint64 elapsed_usec() const;
    NDB_TICKS m_start;
    NDB_TICKS m_stop;
    uint64 m_utime_msec;
    uint64 m_stime_msec;
  };

  // log

  FileOutputStream* c_logfile;
  NdbOut* c_log;
  NdbMutex* c_logmutex;
  Timer c_logtimer;

  // error

  /*
   * Errors are written on several levels.
   * - global error (c_error here), fatal to entire import
   * - team level error, fatal for the job
   * - local non-fatal error added to rejected rows
   */

  Error c_error;
  Lockable c_error_lock;

  void set_error_gen(Error& error, int line,
                     const char* fmt = 0, ...)
    ATTRIBUTE_FORMAT(printf, 4, 5);
  void set_error_usage(Error& error, int line,
                       const char* fmt = 0, ...)
    ATTRIBUTE_FORMAT(printf, 4, 5);
  void set_error_alloc(Error& error, int line);
  void set_error_mgm(Error& error, int line,
                     NdbMgmHandle handle);
  void set_error_con(Error& error, int line,
                     const Ndb_cluster_connection* con);
  void set_error_ndb(Error& error, int line,
                     const NdbError& ndberror, const char* fmt = 0, ...);
  void set_error_os(Error& error, int line,
                    const char* fmt = 0, ...)
    ATTRIBUTE_FORMAT(printf, 4, 5);
  void set_error_data(Error& error, int line,
                      int code, const char* fmt = 0, ...)
    ATTRIBUTE_FORMAT(printf, 5, 6);
  void copy_error(Error& error, const Error& error2);
  bool has_error(const Error& error) {
    return error.type != Error::Type_noerror;
  }
  bool has_error() {
    return has_error(c_error);
  }

  // global flag to stop all jobs
  static bool g_stop_all;

  // convert milliseconds to hours,minutes,seconds string
  static void fmt_msec_to_hhmmss(char* str, uint64 msec);
};

NdbOut& operator<<(NdbOut& out, const NdbImportUtil& util);
NdbOut& operator<<(NdbOut& out, const NdbImportUtil::Name& name);
NdbOut& operator<<(NdbOut& out, const NdbImportUtil::RowMap& rowmap);
NdbOut& operator<<(NdbOut& out, const NdbImportUtil::RowMap::Range& range);
NdbOut& operator<<(NdbOut& out, const NdbImportUtil::Buf& buf);
NdbOut& operator<<(NdbOut& out, const NdbImportUtil::Stats& stats);
NdbOut& operator<<(NdbOut& out, const NdbImportUtil::Timer& timer);

#endif
