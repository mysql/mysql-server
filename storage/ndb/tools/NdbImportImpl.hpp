/*
   Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef NDB_IMPORT_IMPL_HPP
#define NDB_IMPORT_IMPL_HPP

#include <ndb_global.h>
#include <stdint.h>
#include <ndb_opts.h>
#include <ndb_limits.h>
#include <mgmapi.h>
#include <NdbApi.hpp>
#include <NdbError.hpp>
#include <NdbSleep.h>
#include <ndb_rand.h>
#include <NdbImport.hpp>
#include "NdbImportUtil.hpp"
#include "NdbImportCsv.hpp"
// STL
#include <map>
#include <algorithm>

class NdbImportImpl : public NdbImport {
public:
  friend class NdbImport;

  typedef NdbImportUtil::OptGuard OptGuard;
  typedef NdbImportUtil::Name Name;
  typedef NdbImportUtil::Lockable Lockable;
  typedef NdbImportUtil::Thread Thread;
  typedef NdbImportUtil::ListEnt ListEnt;
  typedef NdbImportUtil::List List;
  typedef NdbImportUtil::Attr Attr;
  typedef NdbImportUtil::Attrs Attrs;
  typedef NdbImportUtil::Table Table;
  typedef NdbImportUtil::Tables Tables;
  typedef NdbImportUtil::RowCtl RowCtl;
  typedef NdbImportUtil::Row Row;
  typedef NdbImportUtil::RowList RowList;
  typedef NdbImportUtil::Blob Blob;
  typedef NdbImportUtil::Range Range;
  typedef NdbImportUtil::RangeList RangeList;
  typedef NdbImportUtil::RowMap RowMap;
  typedef NdbImportUtil::ErrorMap ErrorMap;
  typedef NdbImportUtil::Buf Buf;
  typedef NdbImportUtil::File File;
  typedef NdbImportUtil::Stat Stat;
  typedef NdbImportUtil::Stats Stats;
  typedef NdbImportUtil::Timer Timer;
  typedef NdbImportCsv::Spec CsvSpec;
  typedef NdbImportCsv::Input CsvInput;
  typedef NdbImportCsv::Output CsvOutput;

  NdbImportImpl(NdbImport& facade);
  ~NdbImportImpl();
  NdbImport* const m_facade;
  NdbImportUtil m_util;
  NdbImportCsv m_csv;
  Error& m_error;

  // mgm

  struct Mgm {
    Mgm(NdbImportImpl& impl);
    ~Mgm();
    int do_connect();
    void do_disconnect();
    int get_status();
    NdbImportImpl& m_impl;
    NdbImportUtil& m_util;
    Error& m_error;
    NdbMgmHandle m_handle;
    bool m_connected;
    ndb_mgm_cluster_state* m_status;
  };

  // node

  static const uint g_max_ndb_nodes = MAX_NDB_NODES;
  static const uint g_max_nodes = MAX_NODES;

  struct Node {
    Node();
    uint m_nodeid;
  };

  struct Nodes {
    Nodes();
    uint m_nodecnt;
    Node m_nodes[g_max_ndb_nodes];
    uint m_index[g_max_nodes];
  };

  Nodes c_nodes;

  int get_nodes(Nodes& c);

  // connect

  struct Connect {
    Connect();
    ~Connect();
    uint m_connectioncnt;
    Ndb_cluster_connection** m_connections;
    Ndb_cluster_connection* m_mainconnection;
    bool m_connected;
    Ndb* m_mainndb;
  };

  Connect c_connect;
  uint c_connectionindex;

  int do_connect();
  void do_disconnect();

  // tables

  int add_table(const char* database,
                const char* table,
                uint& tabid,
                Error& error);

  // files

  struct WorkerFile : File, Lockable {
    WorkerFile(NdbImportUtil& util, Error& error);
    uint m_workerno;
  };

  // job

  /*
   * Execution: Job - Team - Worker.
   *
   * A job does one task, for example loading a file of CSV data
   * into a table.  Multiple and parallel jobs are allowed.  They
   * share cluster connections, table definitions, and some data
   * pools, but not threads.
   *
   * A job is defined as a set of teams.  Basically there is
   * a producer team and a consumer team.  The CSV->NDB job also has
   * a relay team between producer and consumer.  Teams run in the
   * same thread as the job owning them.  A team only controls its
   * workers and a job only controls its teams.
   *
   * A team has a set of workers cooperating on the same task, for
   * example reading and parsing input in turns, or loading rows
   * into NDB.  Each worker runs in its own thread.
   *
   * Table data: Row, RowList.
   *
   * A row is data for one table row in NdbRecord format (blob data
   * is included).  A row has a unique row id starting at 0 (e.g.
   * CSV line number).  Teams are connected by row queues (lists).
   * Rows are produced and consumed by workers.  Usually a mutex on
   * the row queue is required so batching should be done.
   *
   * A row queue has a preferred size limit to balance consumer and
   * producer activities.  When a producing worker sees the limit
   * exceeded, it takes a short break.  Similarly when a consuming
   * worker finds no rows to consume, it takes a short break.
   *
   * Termination:  A producing worker stops when done (e.g. end of
   * CSV file).  When the team sees that all workers have stopped it
   * sets an "end-of-file" flag on the row queue.  The queue need
   * not be empty yet.  A consuming worker stops when it finds no
   * rows and also sees the end-of-file flag.  When all workers have
   * stopped, the team stops.
   *
   * A consuming team sets a "reverse" end-of-file flag on the row
   * queue when it stops (which can be on fatal team error).  This
   * causes the previous producing team(s) to stop too.
   *
   * When all teams have stopped the job itself stops, waiting to be
   * collected by the client application such as ndb_import.
   *
   * A job can also be asked to stop gracefully.  This asks each
   * team and worker to stop when they are in a consistent state.
   * This is used to handle SIGHUP etc or other user interaction.
   *
   * Diagnostics: There is a diagnostics team which starts before
   * and stops after other teams.  Its task is to record job and
   * team termination status, rejected rows, a map of processed
   * rows, and statistics.  The records exist in "pseudo-tables" and
   * are written to files in CSV format.
   *
   * The diagnostics team is also responsible for reading required
   * old result files if a job is resumed.
   *
   * Errors can be global (e.g. connection failure) or specific to
   * a job.  Any error causes job termination.  Workers record their
   * errors on team level and team level error causes job error.
   * Rejected rows record their error in the rejected row but do not
   * cause job error (up to a given limit).
   */

  struct Job;
  struct Team;
  struct Worker;

  struct JobState {
    enum State {
      State_null = 0,
      State_created,
      State_starting,
      State_running,
      State_stop,
      State_stopped,
      State_done
    };
  };

  struct TeamState {
    enum State {
      State_null = 0,
      State_created,
      State_started,
      State_running,
      State_stop,
      State_stopped
    };
  };

  struct WorkerState {
    enum State {
      State_null = 0,
      State_wait,
      State_run,
      State_running,
      State_stop,
      State_stopped
    };
  };

  static const int g_teamstatecnt = TeamState::State_stopped + 1;
  static const int g_workerstatecnt = WorkerState::State_stopped + 1;

  static const char* g_str_state(JobState::State state);
  static const char* g_str_state(TeamState::State state);
  static const char* g_str_state(WorkerState::State state);

  static const uint g_max_teams = 10;

  struct Job : Thread {
    Job(NdbImportImpl& impl, uint jobno);
    ~Job();
    // define teams and row queues
    void do_create();
    void add_team(Team* team);
    // add and set table
    int add_table(const char* database, const char* table, uint& tabid);
    void set_table(uint tabid);
    // start teams and run the job until done
    void do_start();
    void start_diag_team();
    void start_resume();
    void start_teams();
    void check_teams(bool dostop);
    void check_userstop();
    void collect_teams();
    void collect_stats();
    void stop_diag_team();
    // client request to stop the job
    void do_stop();
    void str_state(char* str) const;
    NdbImportImpl& m_impl;
    NdbImportUtil& m_util;
    uint m_runno;       // run number i.e. resume count
    const uint m_jobno;
    const Name m_name;
    // per-job stats
    Stats m_stats;
    JobState::State m_state;
    uint m_tabid;
    bool m_dostop;      // request graceful stop
    bool m_fatal;       // error is unlikely to be resumable
    ErrorMap m_errormap;// temporary errors from exec-op
    uint m_teamcnt;
    Team* m_teams[g_max_teams];
    uint m_teamstates[g_teamstatecnt];
    RowList* m_rows_relay;
    RowList* m_rows_exec[g_max_ndb_nodes];
    RowList* m_rows_reject;
    RowMap m_rowmap_in;         // old rowmap on resume
    Range m_range_in;           // first range on resume
    RowMap m_rowmap_out;
    mutable Timer m_timer;
    Error m_error;
    bool has_error() const {
      return m_util.has_error(m_error);
    }
    // stats
    Stat* m_stat_rows;          // rows inserted
    Stat* m_stat_reject;        // rows rejected at some stage
    Stat* m_stat_runtime;       // total runtime in milliseconds
    Stat* m_stat_rowssec;       // rows inserted per second
    Stat* m_stat_utime;
    Stat* m_stat_stime;
    Stat* m_stat_rowmap;
  };

  struct Team {
    Team(Job& job, const char* name, uint workercnt);
    virtual ~Team() = 0;
    // create workers
    void do_create();
    virtual Worker* create_worker(uint n) = 0;
    // create worker threads
    void do_start();
    virtual void do_init() = 0;
    Worker* get_worker(uint n);
    void start_worker(Worker* w);
    void wait_workers(WorkerState::State state);
    void wait_worker(Worker* w, WorkerState::State state);
    // start worker threads running
    void do_run();
    void run_worker(Worker* w);
    void check_workers();
    // stop and collect workers and stop this team
    void do_stop();
    void stop_worker(Worker* w);
    virtual void do_end() = 0;
    void set_table(uint tabid);
    virtual void str_state(char* str) const;
    Job& m_job;
    NdbImportImpl& m_impl;
    NdbImportUtil& m_util;
    const uint m_teamno;
    const Name m_name;
    TeamState::State m_state;
    const uint m_workercnt;
    Worker** m_workers;
    uint m_workerstates[g_workerstatecnt];
    uint m_tabid;
    RowMap m_rowmap_out;
    bool m_is_diag;
    mutable Timer m_timer;
    Error m_error;              // team level
    bool has_error() const {
      return m_util.has_error(m_error);
    }
    // stats
    Stat* m_stat_runtime;
    Stat* m_stat_slice;
    Stat* m_stat_idleslice;
    Stat* m_stat_idlerun;
    Stat* m_stat_utime;
    Stat* m_stat_stime;
    Stat* m_stat_rowmap;
  };

  struct Worker : Thread {
    Worker(Team& team, uint n);
    virtual ~Worker();
    // run the worker thread until done
    void do_start();
    virtual void do_init() = 0;
    /*
     * Run one "slice" of the task, e.g. parse a page of CSV or do
     * a batch of database ops.  Between slices the worker "comes up
     * for air" to check status and to sleep for a while if the
     * slice did no work.
     */
    virtual void do_run() = 0;
    virtual void do_end() = 0;
    Worker* next_worker();
    virtual void str_state(char* str) const;
    Team& m_team;
    NdbImportImpl& m_impl;
    NdbImportUtil& m_util;
    const uint m_workerno;
    const Name m_name;
    WorkerState::State m_state;
    bool m_dostop;      // request graceful stop
    uint m_slice;
    uint m_idleslice;
    bool m_idle;        // last slice did no work
    uint m_idlerun;     // consecutive idle slices
    RowMap m_rowmap_out;
    mutable Timer m_timer;
    Error& m_error;     // team level
    bool has_error() const {
      return m_team.has_error();
    }
    // random for tests
    uint get_rand() {
      return (uint)ndb_rand_r(&m_seed);
    }
    unsigned m_seed;
    // stats
    Stat* m_stat_slice;         // slices
    Stat* m_stat_idleslice;     // slices which did no work
    Stat* m_stat_idlerun;
    Stat* m_stat_utime;
    Stat* m_stat_stime;
    Stat* m_stat_rowmap;
  };

  // random input team

  struct RandomInputTeam : Team {
    RandomInputTeam(Job& job, uint workercnt);
    virtual ~RandomInputTeam();
    virtual Worker* create_worker(uint n);
    virtual void do_init();
    virtual void do_end();
  };

  struct RandomInputWorker : Worker {
    RandomInputWorker(Team& team, uint n);
    virtual ~RandomInputWorker();
    virtual void do_init();
    virtual void do_run();
    virtual void do_end();
    Row* create_row(uint64 rowid, const Table& t);
  };

  // csv input team

  // see NdbImportCsv::Input for details
  struct InputState {
    enum State {
      State_null = 0,
      // try to lock the input file to this worker
      State_lock,
      // read a block from the locked file and release the lock
      State_read,
      // waiting for previous worker to transfer partial last line
      State_waittail,
      // parse the complete lines via Csv
      State_parse,
      // transfer partial last line to next worker
      State_movetail,
      // evaluate parsed lines and fields into rows via Csv
      State_eval,
      // send evaluated rows to relay row queue via Csv
      State_send,
      // end of CSV input
      State_eof
    };
  };

  static const char* g_str_state(InputState::State state);

  struct CsvInputTeam : Team {
    CsvInputTeam(Job& job, uint workercnt);
    virtual ~CsvInputTeam();
    virtual Worker* create_worker(uint n);
    virtual void do_init();
    virtual void do_end();
    CsvSpec m_csvspec;
    WorkerFile m_file;
    // stats
    Stat* m_stat_waittail;
    Stat* m_stat_waitmove;
    Stat* m_stat_movetail;
  };

  struct CsvInputWorker : Worker {
    CsvInputWorker(Team& team, uint n);
    virtual ~CsvInputWorker();
    virtual void do_init();
    virtual void do_run();
    virtual void do_end();
    void state_lock();
    void state_read();
    void state_waittail();
    void state_parse();
    void state_movetail();
    void state_eval();
    void state_send();
    virtual void str_state(char* str) const;
    InputState::State m_inputstate;
    Buf m_buf;
    CsvInput* m_csvinput;
    bool m_firstread;
    bool m_eof;
  };

  // null output team

  struct NullOutputTeam : Team {
    NullOutputTeam(Job& job, uint workercnt);
    virtual ~NullOutputTeam();
    virtual Worker* create_worker(uint n);
    virtual void do_init();
    virtual void do_end();
  };

  struct NullOutputWorker : Worker {
    NullOutputWorker(Team& team, uint n);
    virtual ~NullOutputWorker();
    virtual void do_init();
    virtual void do_run();
    virtual void do_end();
  };

  // op

  struct Op : ListEnt {
    Op();
    Op* next() {
      return static_cast<Op*>(m_next);
    }
    Row* m_row;
    const NdbOperation* m_rowop;
    uint m_opcnt;       // main and blob NDB ops
    uint m_opsize;
  };

  struct OpList : private List {
    OpList();
    ~OpList();
    void set_stats(Stats& stats, const char* name) {
      List::set_stats(stats, name);
    }
    Op* front() {
      return static_cast<Op*>(m_front);
    }
    Op* pop_front() {
      return static_cast<Op*>(List::pop_front());
    }
    void push_back(Op* op) {
      List::push_back(op);
    }
    void push_front(Op* op) {
      List::push_front(op);
    }
    uint cnt() const {
      return m_cnt;
    }
  };

  // tx

  struct DbWorker;

  struct Tx : ListEnt {
    Tx(DbWorker* worker);
    virtual ~Tx();
    DbWorker* const m_worker;
    NdbTransaction* m_trans;
    OpList m_ops;
    Tx* next() {
      return static_cast<Tx*>(m_next);
    }
  };

  struct TxList : private List {
    TxList();
    ~TxList();
    void set_stats(Stats& stats, const char* name) {
      List::set_stats(stats, name);
    }
    Tx* front() {
      return static_cast<Tx*>(m_front);
    }
    void push_back(Tx* tx) {
      List::push_back(tx);
    }
    Tx* pop_front() {
      return static_cast<Tx*>(List::pop_front());
    }
    void remove(Tx* tx) {
      List::remove(tx);
    }
    uint cnt() const {
      return m_cnt;
    }
  };

  // db team

  struct DbTeam : Team {
    DbTeam(Job& job, const char* name, uint workercnt);
    virtual ~DbTeam() = 0;
  };

  struct DbWorker : Worker {
    DbWorker(Team& team, uint n);
    virtual ~DbWorker() = 0;
    int create_ndb(uint transcnt);
    Op* alloc_op();
    void free_op(Op* op);
    Tx* start_trans();
    Tx* start_trans(const NdbRecord* keyrec,
                    const char* keydata,
                    uchar* xfrmbuf, uint xfrmbuflen);
    Tx* start_trans(uint nodeid, uint instanceid);
    void close_trans(Tx* tx);
    Ndb* m_ndb;
    OpList m_op_free;
    TxList m_tx_free;
    TxList m_tx_open;
    // rows to free at batch end under single mutex
    RowList m_rows_free;
  };

  // relay op team

  /*
   * A relay op worker consumes relay rows.  It calls the hash
   * calculation on distribution keys to determine optimal node to
   * send the row to.  It then pipes the row to exec op worker(s)
   * dedicated to that node.
   */

  struct RelayState {
    enum State {
      State_null = 0,
      // receive rows from e.g. CSV input
      State_receive,
      // select optimal node
      State_define,
      // send rows to each exec op worker
      State_send,
      // no more rows
      State_eof
    };
  };

  static const char* g_str_state(RelayState::State state);

  struct RelayOpTeam : DbTeam {
    RelayOpTeam(Job& job, uint workercnt);
    virtual ~RelayOpTeam();
    virtual Worker* create_worker(uint n);
    virtual void do_init();
    virtual void do_end();
  };

  struct RelayOpWorker : DbWorker {
    RelayOpWorker(Team& team, uint n);
    virtual ~RelayOpWorker();
    virtual void do_init();
    virtual void do_run();
    virtual void do_end();
    void state_receive();
    void state_define();
    void state_send();
    virtual void str_state(char* str) const;
    RelayState::State m_relaystate;
    uchar* m_xfrmalloc;
    uchar* m_xfrmbuf;
    uint m_xfrmbuflen;
    RowList m_rows;     // rows received
    RowList* m_rows_exec[g_max_ndb_nodes];      // sorted to per-node
  };

  // exec op team

  /*
   * An exec op worker is dedicated to a specific node (DBTC).  This
   * allows better use of the transporter.  The worker receives rows
   * from relay op workers.  A row gives rise to a main operation
   * and any blob part operations.
   *
   * The code has synch and asynch variants.  The synch variant is
   * mainly for performance comparison.  It uses one transaction for
   * all rows in the batch and does not check errors on individual
   * operations.  The asynch variant uses one transaction for each
   * row and can detect rows to reject.
   */

  struct ExecState {
    enum State {
      State_null = 0,
      // receive rows until a batch is full
      State_receive,
      // define transactions and operations
      State_define,
      // prepare the transactions (asynch)
      State_prepare,
      // execute (synch) or send (asynch) the transactions
      State_send,
      // poll for the transactions (asynch)
      State_poll,
      // no more incoming rows
      State_eof
    };
  };

  static const char* g_str_state(ExecState::State state);

  struct ExecOpTeam : DbTeam {
    ExecOpTeam(Job& job, uint workercnt);
    virtual ~ExecOpTeam();
    virtual Worker* create_worker(uint n);
    virtual void do_init();
    virtual void do_end();
  };

  struct ExecOpWorker : DbWorker {
    ExecOpWorker(Team& team, uint n);
    virtual ~ExecOpWorker() = 0;
    virtual void do_init();
    virtual void do_run();
    virtual void do_end() = 0;
    void state_receive();       // common to synch/asynch
    virtual void state_define() = 0;
    virtual void state_prepare() = 0;
    virtual void state_send() = 0;
    virtual void state_poll() = 0;
    virtual void str_state(char* str) const;
    void reject_row(Row* row, const Error& error);
    ExecState::State m_execstate;
    uint m_nodeindex;   // index into ndb nodes array
    uint m_nodeid;
    RowList m_rows;     // received rows
    OpList m_ops;       // received rows converted to ops
    bool m_eof;
    ErrorMap m_errormap;// temporary errors in current batch
    uint m_opcnt;       // current batch
    uint m_opsize;
  };

  struct ExecOpWorkerSynch : ExecOpWorker {
    ExecOpWorkerSynch(Team& team, uint n);
    virtual ~ExecOpWorkerSynch();
    virtual void do_end();
    virtual void state_define();
    virtual void state_prepare();
    virtual void state_send();
    virtual void state_poll();
  };

  struct ExecOpWorkerAsynch : ExecOpWorker {
    ExecOpWorkerAsynch(Team& team, uint n);
    virtual ~ExecOpWorkerAsynch();
    virtual void do_end();
    virtual void state_define();
    virtual void state_prepare();
    virtual void state_send();
    virtual void state_poll();
    void asynch_callback(Tx* tx);
  };

  // diag team

  struct DiagTeam : Team {
    DiagTeam(Job& job, uint workercnt);
    virtual ~DiagTeam();
    virtual Worker* create_worker(uint n);
    virtual void do_init();
    void read_old_diags(const char* name,
                        const char* path,
                        const Table& table,
                        RowList& rows_out);
    void read_old_diags();
    void open_new_diags();
    virtual void do_end();
    CsvSpec m_csvspec;
    WorkerFile m_result_file;
    WorkerFile m_reject_file;
    WorkerFile m_rowmap_file;
    WorkerFile m_stopt_file;
    WorkerFile m_stats_file;
  };

  struct DiagWorker : Worker {
    DiagWorker(Team& team, uint n);
    virtual ~DiagWorker();
    virtual void do_init();
    virtual void do_run();
    virtual void do_end();
    void write_result();
    void write_reject();
    void write_rowmap();
    void write_stopt();
    void write_stats();
    Buf m_result_buf;
    Buf m_reject_buf;
    Buf m_rowmap_buf;
    Buf m_stopt_buf;
    Buf m_stats_buf;
    CsvOutput* m_result_csv;
    CsvOutput* m_reject_csv;
    CsvOutput* m_rowmap_csv;
    CsvOutput* m_stopt_csv;
    CsvOutput* m_stats_csv;
  };

  // global

  struct Jobs {
    Jobs();
    ~Jobs();
    std::map<uint, Job*> m_jobs;
    uint m_jobno;       // next job number (forever increasing)
  };
  Jobs c_jobs;

  Job* create_job();
  Job* find_job(uint jobno);
  void start_job(Job* job);
  void stop_job(Job* job);
  void wait_job(Job* job);
  void destroy_job(Job* job);
};

NdbOut& operator<<(NdbOut& out, const NdbImportImpl& impl);
NdbOut& operator<<(NdbOut& out, const NdbImportImpl::Mgm& mgm);
NdbOut& operator<<(NdbOut& out, const NdbImportImpl::Job& job);
NdbOut& operator<<(NdbOut& out, const NdbImportImpl::Team& team);
NdbOut& operator<<(NdbOut& out, const NdbImportImpl::Worker& w);

#endif
