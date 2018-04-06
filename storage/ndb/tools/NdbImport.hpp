/*
   Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef NDB_IMPORT_HPP
#define NDB_IMPORT_HPP

// STL
#include <map>

struct CHARSET_INFO;
class NdbOut;
class Ndb_cluster_connection;

class NdbImport {
public:
  NdbImport();
  ~NdbImport();

  // csv spec

  struct OptCsv {
    OptCsv();
    enum Mode {
      ModeInput = 1,
      ModeOutput = 2
    };
    const char* m_fields_terminated_by;
    const char* m_fields_enclosed_by;
    const char* m_fields_optionally_enclosed_by;
    const char* m_fields_escaped_by;
    const char* m_lines_terminated_by;
  };

  // opt

  struct Opt {
    Opt();
    uint m_connections;
    const char* m_database;
    const char* m_state_dir;
    bool m_keep_state;
    bool m_stats;
    const char* m_table;
    const char* m_input_type;
    const char* m_input_file;
    uint m_input_workers;
    const char* m_output_type;
    uint m_output_workers;
    uint m_db_workers;
    uint m_ignore_lines;
    uint m_max_rows;
    const char* m_result_file;
    const char* m_reject_file;
    const char* m_rowmap_file;
    const char* m_stopt_file;
    const char* m_stats_file;
    bool m_continue;
    bool m_resume;
    uint m_monitor;
    uint m_ai_prefetch_sz;
    uint m_ai_increment;
    uint m_ai_offset;
    bool m_no_asynch;
    bool m_no_hint;
    uint m_pagesize;
    uint m_pagecnt;
    uint m_pagebuffer;
    uint m_rowbatch;
    uint m_rowbytes;
    uint m_opbatch;
    uint m_opbytes;
    uint m_polltimeout;
    uint m_temperrors;
    uint m_tempdelay;
    uint m_rowswait;
    uint m_idlespin;
    uint m_idlesleep;
    uint m_checkloop;
    uint m_alloc_chunk;
    uint m_rejects;
    // character set of input file (currently fixed as binary)
    const char* m_charset_name;
    const CHARSET_INFO* m_charset;
    // csv options
    OptCsv m_optcsv;
    const char* m_csvopt;
    // debug options
    uint m_log_level;
    bool m_abort_on_error;
    const char* m_errins_type;
    uint m_errins_delay;
  };
  // set options for next job
  int set_opt(Opt& opt);

  // connect

  int do_connect();
  void do_disconnect();

  // table

  // tables are shared and can also be added outside job context
  int add_table(const char* database, const char* table, uint& tabid);

  // job

  struct Job;
  struct Team;
  struct Error;

  struct JobStatus {
    enum Status {
      Status_null = 0,
      Status_created,
      Status_starting,
      Status_running,
      Status_success,
      Status_error,
      Status_fatal
    };
  };

  // a selection of stats (full details are in stats file t1.stt)
  struct JobStats {
    JobStats();
    // from all resumed runs
    uint64 m_rows;
    uint64 m_reject;
    uint64 m_runtime;
    uint64 m_rowssec;
    // from latest run
    uint64 m_new_rows;
    uint64 m_new_reject;
    uint m_temperrors;  // sum of values from m_errormap
    std::map<uint, uint> m_errormap;
  };

  struct Job {
    Job(NdbImport& imp);
    ~Job();
    int do_create();
    int do_start();
    int do_stop();      // ask to stop before ready
    int do_wait();
    void do_destroy();
    int add_table(const char* database, const char* table, uint& tabid);
    void set_table(uint tabid);
    bool has_error() const;
    const Error& get_error() const;
    NdbImport& m_imp;
    uint m_jobno;
    uint m_runno;       // run number i.e. resume count
    JobStatus::Status m_status;
    const char* m_str_status;
    JobStats m_stats;
    uint m_teamcnt;
    Team** m_teams;
    // update status of job and all teams
    void get_status();
  };

  struct TeamStatus {
    enum Status {
      Status_null = 0
    };
  };

  struct Team {
    Team(const Job& job, uint teamno);
    const char* get_name();
    bool has_error() const;
    const Error& get_error() const;
    const Job& m_job;
    const uint m_teamno;
    // snapshot or final status
    enum Status {
      Status_null = 0
    };
    TeamStatus::Status m_status;
    const char* m_str_status;
  };

  static const char* g_str_status(JobStatus::Status status);
  static const char* g_str_status(TeamStatus::Status status);

  // error

  struct Error {
    enum Type {
      Type_noerror = 0,
      Type_gen = 1,
      Type_usage = 2,
      Type_alloc = 3,
      Type_mgm = 4,
      Type_con = 5,
      Type_ndb = 6,
      Type_os = 7,
      Type_data = 8
    };
    Error();
    const char* gettypetext() const;
    Type type;
    int code;
    int line;
    char text[1024];
  };

  bool has_error() const;
  const Error& get_error() const;
  friend class NdbOut& operator<<(NdbOut&, const Error&);

  // stop all jobs (crude way to handle signals)
  static void set_stop_all();

private:
  friend class NdbImportImpl;
  NdbImport(class NdbImportImpl& impl);
  class NdbImportImpl& m_impl;
};

#endif
