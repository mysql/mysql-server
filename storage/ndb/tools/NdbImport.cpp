/*
   Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

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

#include <ndb_global.h>
#include <NdbOut.hpp>
#include "NdbImportImpl.hpp"

typedef NdbImportImpl::JobState JobState;
typedef NdbImportImpl::TeamState TeamState;

NdbImport::NdbImport() :
  m_impl(*new NdbImportImpl(*this))
{
}

NdbImport::NdbImport(NdbImportImpl& impl) :
  m_impl(impl)
{
}

NdbImport::~NdbImport()
{
  NdbImportImpl* impl = &m_impl;
  if (this != impl)
    delete impl;
}

// csv spec

NdbImport::OptCsv::OptCsv()
{
  m_fields_terminated_by = "\\t";
  m_fields_enclosed_by = 0;
  m_fields_optionally_enclosed_by = 0;
  m_fields_escaped_by = "\\\\";
#ifndef _WIN32
  m_lines_terminated_by = "\\n";
#else
  m_lines_terminated_by = "\\r\\n";
#endif
}

// opt

NdbImport::Opt::Opt()
{
  m_connections = 1;
  m_database = 0;
  m_state_dir = ".";
  m_keep_state = false;
  m_stats = false;
  m_table = 0;
  m_input_type = "csv";
  m_input_file = 0;
  m_input_workers = 4;
  m_output_type = "ndb";
  m_output_workers = 2;
  m_db_workers = 4;
  m_ignore_lines = 0;
  m_max_rows = 0;
  m_result_file = 0;
  m_reject_file = 0;
  m_rowmap_file = 0;
  m_stopt_file = 0;
  m_stats_file = 0;
  m_continue = false;
  m_resume = false;
  m_monitor = 2;
  m_ai_prefetch_sz = 1024;
  m_ai_increment = 1;
  m_ai_offset = 1;
  m_no_asynch = false;
  m_no_hint = false;
  m_pagesize = 4096;
  m_pagecnt = 0;
  m_pagebuffer = 500000;
  m_rowbatch = 0;
  m_rowbytes = 500000;
  m_opbatch = 500;
  m_opbytes = 0;
  m_polltimeout = 1000;
  m_temperrors = 0;
  m_tempdelay = 10;
  m_rowswait = 10;
  m_idlespin = 0;
  m_idlesleep = 1;
  m_checkloop = 100;
  m_alloc_chunk = 20;
  m_rejects = 0;
  // character set
  m_charset_name = "binary";
  m_charset = 0;
  // csv options
  m_csvopt = 0;
  // debug options
  m_log_level = 0;
  m_abort_on_error = false;
  m_errins_type = 0;
  m_errins_delay = 1000;
}

int
NdbImport::set_opt(Opt& opt)
{
  NdbImportUtil& util = m_impl.m_util;
  NdbImportCsv& csv = m_impl.m_csv;
  // XXX clean this up (map strings to enums)
  if (opt.m_input_type != 0)
  {
    const char* valid[] = { "csv", "random", 0 };
    const char** p = valid;
    while (*p != 0 && strcmp(*p, opt.m_input_type) != 0)
      p++;
    if (*p == 0)
    {
      util.set_error_usage(util.c_error, __LINE__,
                           "invalid input-type %s", opt.m_input_type);
      return -1;
    }
    if (opt.m_input_workers < 1)
    {
      util.set_error_usage(util.c_error, __LINE__,
                           "number of input workers must be >= 1");
      return -1;
    }
    if (strcmp(opt.m_input_type, "csv") == 0 &&
        opt.m_input_workers < 2)
    {
      util.set_error_usage(util.c_error, __LINE__,
                           "number of csv input workers must be >= 2");
      return -1;
    }
    if (strcmp(opt.m_input_type, "random") == 0 &&
        opt.m_rowbatch == 0)
    {
      util.set_error_usage(util.c_error, __LINE__,
                           "input type random requires nonzero --rowbatch");
      return -1;
    }
  }
  if (opt.m_output_type != 0)
  {
    const char* valid[] = { "ndb", "null", 0 };
    const char** p = valid;
    while (*p != 0 && strcmp(*p, opt.m_output_type) != 0)
      p++;
    if (*p == 0)
    {
      util.set_error_usage(util.c_error, __LINE__,
                           "invalid output-type %s", opt.m_output_type);
      return -1;
    }
    if (opt.m_output_workers < 1)
    {
      util.set_error_usage(util.c_error, __LINE__,
                           "number of output workers must be >= 1");
      return -1;
    }
    if (strcmp(opt.m_output_type, "ndb") == 0 &&
        opt.m_db_workers < 1)
    {
      util.set_error_usage(util.c_error, __LINE__,
                           "number of db workers must be >= 1");
      return -1;
    }
  }
  if (opt.m_pagesize == 0)
  {
    util.set_error_usage(util.c_error, __LINE__,
                         "option --pagesize must be non-zero");
    return -1;
  }
  if (opt.m_pagebuffer != 0)
  {
    opt.m_pagecnt = (opt.m_pagebuffer + opt.m_pagesize - 1) / opt.m_pagesize;
  }
  if (opt.m_opbatch == 0)
  {
    util.set_error_usage(util.c_error, __LINE__,
                         "option --opbatch must be non-zero");
    return -1;
  }
  if (opt.m_ai_prefetch_sz == 0 ||
      opt.m_ai_increment == 0 ||
      opt.m_ai_offset == 0)
  {
    util.set_error_usage(util.c_error, __LINE__,
                         "invalid autoincrement options");
    return -1;
  }
  if (opt.m_alloc_chunk == 0)
  {
    util.set_error_usage(util.c_error, __LINE__,
                         "option --alloc-chunk must be non-zero");
    return -1;
  }
  // character set
  require(opt.m_charset_name != 0);
  opt.m_charset = get_charset_by_name(opt.m_charset_name, MYF(0));
  if (opt.m_charset == 0)
  {
    util.set_error_usage(util.c_error, __LINE__,
                         "unknown character set: %s", opt.m_charset_name);
    return -1;
  }
  // csv options
  NdbImportCsv::Spec csvspec;
  if (csv.set_spec(csvspec, opt.m_optcsv, OptCsv::ModeInput) == -1)
  {
    require(util.has_error());
    return -1;
  }
  util.c_opt = opt;
  return 0;
}

// connect

int
NdbImport::do_connect()
{
  if (m_impl.do_connect() == -1)
    return -1;
  if (m_impl.get_nodes(m_impl.c_nodes) == -1)
    return -1;
  return 0;
}

void
NdbImport::do_disconnect()
{
  m_impl.do_disconnect();
}

// table

int
NdbImport::add_table(const char* database, const char* table, uint& tabid)
{
  return m_impl.add_table(database, table, tabid, m_impl.m_error);
}

// job

NdbImport::JobStats::JobStats()
{
  m_rows = 0;
  m_reject = 0;
  m_temperrors = 0;
  m_runtime = 0;
  m_rowssec = 0;
  m_utime = 0;
  m_stime = 0;
}

NdbImport::Job::Job(NdbImport& imp) :
  m_imp(imp)
{
  m_jobno = Inval_uint;
  m_status = JobStatus::Status_null;
  m_str_status = g_str_status(m_status);
  m_teamcnt = 0;
  m_teams = 0;
}

NdbImport::Job::~Job()
{
  if (m_teams != 0)
  {
    for (uint i = 0; i < m_teamcnt; i++)
      delete m_teams[i];
    delete [] m_teams;
  }
}

int
NdbImport::Job::do_create()
{
  require(m_status == JobStatus::Status_null);
  NdbImportImpl& impl = m_imp.m_impl;
  NdbImportImpl::Job* jobImpl = impl.create_job();
  m_jobno = jobImpl->m_jobno;
  m_teamcnt = jobImpl->m_teamcnt;
  m_teams = new Team* [m_teamcnt];
  for (uint i = 0; i < m_teamcnt; i++)
    m_teams[i] = new Team(*this, i);
  m_status = JobStatus::Status_created;
  return 0;
}

int
NdbImport::Job::do_start()
{
  NdbImportImpl& impl = m_imp.m_impl;
  NdbImportImpl::Job* jobImpl = impl.find_job(m_jobno);
  impl.start_job(jobImpl);
  return 0;
}

int
NdbImport::Job::do_stop()
{
  NdbImportImpl& impl = m_imp.m_impl;
  NdbImportImpl::Job* jobImpl = impl.find_job(m_jobno);
  impl.stop_job(jobImpl);
  return 0;
}

int
NdbImport::Job::do_wait()
{
  NdbImportImpl& impl = m_imp.m_impl;
  NdbImportImpl::Job* jobImpl = impl.find_job(m_jobno);
  impl.wait_job(jobImpl);
  return 0;
}

void
NdbImport::Job::do_destroy()
{
  NdbImportImpl& impl = m_imp.m_impl;
  NdbImportImpl::Job* jobImpl = impl.find_job(m_jobno);
  impl.destroy_job(jobImpl);
  m_jobno = Inval_uint;
}
 
int
NdbImport::Job::add_table(const char* database,
                          const char* table,
                          uint& tabid)
{
  NdbImportImpl& impl = m_imp.m_impl;
  NdbImportImpl::Job* jobImpl = impl.find_job(m_jobno);
  return jobImpl->add_table(database, table, tabid);
}

void
NdbImport::Job::set_table(uint tabid)
{
  NdbImportImpl& impl = m_imp.m_impl;
  NdbImportImpl::Job* jobImpl = impl.find_job(m_jobno);
  jobImpl->set_table(tabid);
}

bool
NdbImport::Job::has_error() const
{
  NdbImportImpl& impl = m_imp.m_impl;
  NdbImportUtil& util = impl.m_util;
  NdbImportImpl::Job* jobImpl = impl.find_job(m_jobno);
  return util.has_error(jobImpl->m_error);
}

const Error&
NdbImport::Job::get_error() const
{
  NdbImportImpl& impl = m_imp.m_impl;
  NdbImportImpl::Job* jobImpl = impl.find_job(m_jobno);
  return jobImpl->m_error;
}

/*
 * Set job and teams status and various statistics.  Job status
 * reflects the implementation job state but is not identical to it.
 * Job state controls job flow and there is no error state because
 * the flow must be completed normally in any case.  Whereas job
 * status includes error statuses (resumable or not).
 */
void
NdbImport::Job::get_status()
{
  if (m_status == JobStatus::Status_null)
  {
    // job not yet created
    return;
  }
  NdbImportImpl& impl = m_imp.m_impl;
  NdbImportImpl::Job* jobImpl = impl.find_job(m_jobno);
  switch (jobImpl->m_state) {
  case JobState::State_null:
    require(false);
    break;
  case JobState::State_created:
    m_status = JobStatus::Status_created;
    break;
  case JobState::State_starting:
    m_status = JobStatus::Status_starting;
    break;
  case JobState::State_running:
  case JobState::State_stop:
  case JobState::State_stopped:
    m_status = JobStatus::Status_running;
    break;
  case JobState::State_done:
    m_status = JobStatus::Status_success;
    break;
  }
  m_stats.m_rows = jobImpl->m_stat_rows->m_max;
  m_stats.m_reject = jobImpl->m_stat_reject->m_max;
  m_stats.m_temperrors = jobImpl->m_errormap.get_sum();
  m_stats.m_errormap = jobImpl->m_errormap.m_map;
  m_stats.m_runtime = jobImpl->m_stat_runtime->m_max;
  m_stats.m_rowssec = jobImpl->m_stat_rowssec->m_max;
  m_stats.m_utime = jobImpl->m_stat_utime->m_max;
  m_stats.m_stime = jobImpl->m_stat_stime->m_max;
  // check for errors
  if (jobImpl->has_error())
  {
    m_status = JobStatus::Status_error;
    if (jobImpl->m_fatal)
      m_status = JobStatus::Status_fatal;
  }
  m_str_status = g_str_status(m_status);
}

NdbImport::Team::Team(const Job& job, uint teamno) :
  m_job(job),
  m_teamno(teamno)
{
  m_status = TeamStatus::Status_null;
  m_str_status = g_str_status(m_status);
}

const char*
NdbImport::Team::get_name()
{
  NdbImportImpl& impl = m_job.m_imp.m_impl;
  NdbImportImpl::Job* jobImpl = impl.find_job(m_job.m_jobno);
  NdbImportImpl::Team* teamImpl = jobImpl->m_teams[m_teamno];
  return teamImpl->m_name.str();
}

bool
NdbImport::Team::has_error() const
{
  NdbImportImpl& impl = m_job.m_imp.m_impl;
  NdbImportUtil& util = impl.m_util;
  NdbImportImpl::Job* jobImpl = impl.find_job(m_job.m_jobno);
  NdbImportImpl::Team* teamImpl = jobImpl->m_teams[m_teamno];
  return util.has_error(teamImpl->m_error);
}

const NdbImport::Error&
NdbImport::Team::get_error() const
{
  NdbImportImpl& impl = m_job.m_imp.m_impl;
  NdbImportImpl::Job* jobImpl = impl.find_job(m_job.m_jobno);
  NdbImportImpl::Team* teamImpl = jobImpl->m_teams[m_teamno];
  return teamImpl->m_error;
}

const char*
NdbImport::g_str_status(JobStatus::Status status)
{
  const char* str = 0;
  switch (status) {
  case JobStatus::Status_null:
    str = "null";
    break;
  case JobStatus::Status_created:
    str = "created";
    break;
  case JobStatus::Status_starting:
    str = "starting";
    break;
  case JobStatus::Status_running:
    str = "running";
    break;
  case JobStatus::Status_success:
    str = "success";
    break;
  case JobStatus::Status_error:
    str = "error";
    break;
  case JobStatus::Status_fatal:
    str = "fatal";
    break;
  }
  require(str != 0);
  return str;
}

const char*
NdbImport::g_str_status(TeamStatus::Status status)
{
  const char* str = 0;
  switch (status) {
  case TeamStatus::Status_null:
    str = "null";
    break;
  }
  require(str != 0);
  return str;
}

// error

NdbImport::Error::Error()
{
  type = Type_noerror;
  code = 0;
  line = 0;
  text[0] = 0;
}

const char*
NdbImport::Error::gettypetext() const
{
  const char* typetext = "";
  switch (type) {
  case Type_noerror:
    typetext = "noerror";
    break;
  case Type_usage:
    typetext = "usage";
    break;
  case Type_gen:
    typetext = "gen";
    break;
  case Type_alloc:
    typetext = "alloc";
    break;
  case Type_mgm:
    typetext = "mgm";
    break;
  case Type_con:
    typetext = "con";
    break;
  case Type_ndb:
    typetext = "ndb";
    break;
  case Type_os:
    typetext = "os";
    break;
  case Type_data:
    typetext = "data";
    break;
  };
  return typetext;
}

bool
NdbImport::has_error() const
{
  NdbImportImpl& impl = m_impl;
  NdbImportUtil& util = impl.m_util;
  return util.has_error();
}

const NdbImport::Error&
NdbImport::get_error() const
{
  NdbImportImpl& impl = m_impl;
  NdbImportUtil& util = impl.m_util;
  return util.c_error;
}

NdbOut&
operator<<(NdbOut& out, const NdbImport::Error& error)
{
  const char* typetext = error.gettypetext();
  out << "error[" << typetext << "-" << error.code << "]";
  if (error.text[0] != 0)
    out << ": " << error.text;
  out << " (source:" << error.line << ")";
  return out;
}

void
NdbImport::set_stop_all()
{
  NdbImportUtil::g_stop_all = true;
}
