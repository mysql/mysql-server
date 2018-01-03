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

#include "NdbImportImpl.hpp"

NdbImportImpl::NdbImportImpl(NdbImport& facade) :
  NdbImport(*this),
  m_facade(&facade),
  m_csv(m_util),
  m_error(m_util.c_error)
{
  c_connectionindex = 0;
  log1("ctor");
}

NdbImportImpl::~NdbImportImpl()
{
  log1("dtor");
}

NdbOut&
operator<<(NdbOut& out, const NdbImportImpl& impl)
{
  out << "impl";
  return out;
}

// opt

// mgm

NdbImportImpl::Mgm::Mgm(NdbImportImpl& impl) :
  m_impl(impl),
  m_util(m_impl.m_util),
  m_error(m_util.c_error)
{
  m_handle = 0;
  m_connected = false;
  m_status = 0;
}

NdbImportImpl::Mgm::~Mgm()
{
  do_disconnect();
}

NdbOut&
operator<<(NdbOut& out, const NdbImportImpl::Mgm& mgm)
{
  out << "mgm";
  return out;
}

int
NdbImportImpl::Mgm::do_connect()
{
  log1("do_connect");
  require(m_handle == 0);
  m_handle = ndb_mgm_create_handle();
  require(m_handle != 0);
  ndb_mgm_set_connectstring(m_handle, opt_ndb_connectstring);
  int retries = opt_connect_retries;
  int delay = opt_connect_retry_delay;
  if (ndb_mgm_connect(m_handle, retries, delay, 0) == -1)
  {
    m_util.set_error_mgm(m_error, __LINE__, m_handle);
    return -1;
  }
  m_connected = true;
  log1("do_connect: success");
  return 0;
}

void
NdbImportImpl::Mgm::do_disconnect()
{
  if (m_handle != 0)
  {
    if (m_status != 0)
    {
      free(m_status);
      m_status = 0;
    }
    if (m_connected)
    {
      (void)ndb_mgm_disconnect(m_handle);
      m_connected = false;
    }
    ndb_mgm_destroy_handle(&m_handle);
    m_handle = 0;
    log1("do_disconnect: done");
  }
}

int
NdbImportImpl::Mgm::get_status()
{
  log1("get_status");
  require(m_connected);
  require(m_status == 0);
  int retries = 0;
  while (retries < 10)
  {
    if ((m_status = ndb_mgm_get_status(m_handle)) != 0)
    {
      log1("get_status: success");
      return 0;
    }
    NdbSleep_SecSleep(1);
    retries++;
    log1("get_status: retries " << retries);
  }
  m_util.set_error_mgm(m_error, __LINE__, m_handle);
  return -1;
}

// node

NdbImportImpl::Node::Node()
{
  m_nodeid = Inval_uint;
}

NdbImportImpl::Nodes::Nodes()
{
  m_nodecnt = 0;
  for (uint i = 0; i < g_max_nodes; i++)
    m_index[i] = Inval_uint;
}

int
NdbImportImpl::get_nodes(Nodes& c)
{
  log1("get_nodes");
  c.m_nodecnt = 0;
  do
  {
    Mgm mgm(*this);
    if (mgm.do_connect() == -1)
      break;
    if (mgm.get_status() == -1)
      break;
    const ndb_mgm_cluster_state* status = mgm.m_status;
    for (uint i = 0; i < (uint)status->no_of_nodes; i++)
    {
      const ndb_mgm_node_state* anynode = &status->node_states[i];
      if (anynode->node_type == NDB_MGM_NODE_TYPE_NDB)
      {
        Node& node = c.m_nodes[c.m_nodecnt];
        require(node.m_nodeid == Inval_uint);
        node.m_nodeid = anynode->node_id;
        require(node.m_nodeid < g_max_nodes);
        require(c.m_index[node.m_nodeid] == Inval_uint);
        c.m_index[node.m_nodeid] = c.m_nodecnt;
        log1("node " << c.m_nodecnt << ": " << node.m_nodeid);
        c.m_nodecnt++;
      }
    }
    mgm.do_disconnect();
    return 0;
  } while (0);
  return -1;
}

// connect

NdbImportImpl::Connect::Connect()
{
  m_connectioncnt = 0;
  m_connections = 0;
  m_mainconnection = 0;
  m_connected = false;
  m_mainndb = 0;
}

NdbImportImpl::Connect::~Connect()
{
}

int
NdbImportImpl::do_connect()
{
  log1("do_connect");
  const Opt& opt = m_util.c_opt;
  Connect& c = c_connect;
  if (c.m_connected)
  {
    m_util.set_error_usage(m_error, __LINE__);
    return -1;
  }
  {
    require(c.m_connections == 0 && c.m_mainconnection == 0);
    c.m_connectioncnt = opt.m_connections;
    c.m_connections = new Ndb_cluster_connection* [c.m_connectioncnt];
    for (uint i = 0; i < c.m_connectioncnt; i++)
    {
      c.m_connections[i] =
        new Ndb_cluster_connection(opt_ndb_connectstring,
                                   c.m_mainconnection);
      if (i == 0)
        c.m_mainconnection = c.m_connections[i];
    }
    for (uint i = 0; i < c.m_connectioncnt; i++)
    {
      log1("connection " << i << " of " << c.m_connectioncnt);
      Ndb_cluster_connection* con = c.m_connections[i];
      int retries = opt_connect_retries;
      int delay = opt_connect_retry_delay;
      if (con->connect(retries, delay, 1) != 0)
      {
        m_util.set_error_con(m_error, __LINE__, con);
        return -1;
      }
      log1("connection " << i << " api nodeid " << con->node_id());
    }
  }
  for (uint i = 0; i < c.m_connectioncnt; i++)
  {
    Ndb_cluster_connection* con = c.m_connections[i];
    if (con->wait_until_ready(30, 0) < 0)
    {
      m_util.set_error_con(m_error, __LINE__, con);
      return -1;
    }
    log1("connection " << i << " wait_until_ready done");
  }
  require(c.m_mainndb == 0);
  c.m_mainndb = new Ndb(c.m_mainconnection);
  if (c.m_mainndb->init() != 0)
  {
    m_util.set_error_ndb(m_error, __LINE__, c.m_mainndb->getNdbError());
    return -1;
  }
  if (c.m_mainndb->waitUntilReady() != 0)
  {
    m_util.set_error_ndb(m_error, __LINE__, c.m_mainndb->getNdbError());
    return -1;
  }
  c.m_connected = true;
  log1("do_connect: success");
  return 0;
}

void
NdbImportImpl::do_disconnect()
{
  log1("do_disconnect");
  Connect& c = c_connect;
  // delete any ndb before delete connection
  delete c.m_mainndb;
  c.m_mainndb = 0;
  if (c.m_connections != 0)
  {
    for (uint i = 0; i < c.m_connectioncnt; i++)
    {
      log1("delete connection " << i << " of " << c.m_connectioncnt);
      delete c.m_connections[i];
      c.m_connections[i] = 0;
    }
  }
  delete [] c.m_connections;
  c.m_connections = 0;
  c.m_connected = false;
  log1("do_disconnect: done");
}

// tables

int
NdbImportImpl::add_table(const char* database,
                         const char* table,
                         uint& tabid,
                         Error& error)
{
  Connect& c = c_connect;
  if (!c.m_connected)
  {
    m_util.set_error_usage(error, __LINE__);
    return -1;
  }
  if (database == 0 || table == 0)
  {
    m_util.set_error_usage(error, __LINE__);
    return -1;
  }
  log1("add table " << database << "." << table);
  Ndb* ndb = c.m_mainndb;
  if (strcmp(ndb->getDatabaseName(), database) != 0)
  {
    if (ndb->setDatabaseName(database) != 0)
    {
      m_util.set_error_ndb(error, __LINE__, ndb->getNdbError());
      return -1;
    }
  }
  NdbDictionary::Dictionary* dic = ndb->getDictionary();
  const NdbDictionary::Table* tab = dic->getTable(table);
  if (tab == 0)
  {
    m_util.set_error_ndb(error, __LINE__, dic->getNdbError());
    return -1;
  }
  if (m_util.add_table(dic, tab, tabid, error) != 0)
    return -1;
  return 0;
}

// files

NdbImportImpl::WorkerFile::WorkerFile(NdbImportUtil& util, Error& error) :
  File(util, error)
{
  m_workerno = Inval_uint;
}

// job

NdbImportImpl::Job::Job(NdbImportImpl& impl, uint jobno) :
  m_impl(impl),
  m_util(m_impl.m_util),
  m_jobno(jobno),
  m_name("job", m_jobno),
  m_stats(m_util),
  m_rowmap_in(m_util),
  m_rowmap_out(m_util)
{
  m_runno = 0;
  m_state = JobState::State_null;
  m_tabid = Inval_uint;
  m_dostop = false;
  m_fatal = false;
  m_teamcnt = 0;
  for (uint i = 0; i < g_max_teams; i++)
    m_teams[i] = 0;
  for (int k = 0; k < g_teamstatecnt; k++)
    m_teamstates[k] = 0;
  m_rows_relay = 0;
  for (uint i = 0; i < g_max_ndb_nodes; i++)
    m_rows_exec[i] = 0;
  m_rows_reject = 0;
  // stats
  Stats& stats = m_stats;
  {
    const Name name("job", "rows");
    Stat* stat = stats.create(name, 0, 0);
    m_stat_rows = stat;
  }
  {
    const Name name("job", "reject");
    Stat* stat = stats.create(name, 0, 0);
    m_stat_reject = stat;
  }
  {
    const Name name("job", "runtime");
    Stat* stat = stats.create(name, 0, 0);
    m_stat_runtime = stat;
  }
  {
    const Name name("job", "rowssec");
    Stat* stat = stats.create(name, 0, 0);
    m_stat_rowssec = stat;
  }
  {
    const Name name("job", "utime");
    Stat* stat = stats.create(name, 0, 0);
    m_stat_utime = stat;
  }
  {
    const Name name("job", "stime");
    Stat* stat = stats.create(name, 0, 0);
    m_stat_stime = stat;
  }
  {
    const Name name("job", "rowmap");
    Stat* stat = stats.create(name, 0, 0);
    m_stat_rowmap = stat;
  }
  log1("ctor");
}

NdbImportImpl::Job::~Job()
{
  log1("dtor");
  for (uint i = 0; i < m_teamcnt; i++)
    delete m_teams[i];
  delete m_rows_relay;
  for (uint i = 0; i < g_max_ndb_nodes; i++)
    delete m_rows_exec[i];
  delete m_rows_reject;
}

void
NdbImportImpl::Job::do_create()
{
  const Opt& opt = m_util.c_opt;
  uint nodecnt = m_impl.c_nodes.m_nodecnt;
  require(nodecnt != 0);
  Job& job = *this;
  require(job.m_state == JobState::State_null);
  // diag team is team number 0
  {
    uint workercnt = 1;
    Team* team = new DiagTeam(job, workercnt);
    add_team(team);
  }
  // worker teams start at number 1
  if (opt.m_input_type != 0)
  {
    if (strcmp(opt.m_input_type, "random") == 0)
    {
      uint workercnt = opt.m_input_workers;
      RandomInputTeam* team = new RandomInputTeam(job, workercnt);
      add_team(team);
    }
    if (strcmp(opt.m_input_type, "csv") == 0)
    {
      uint workercnt = opt.m_input_workers;
      CsvInputTeam* team = new CsvInputTeam(job, workercnt);
      add_team(team);
    }
  }
  if (opt.m_output_type != 0)
  {
    if (strcmp(opt.m_output_type, "null") == 0)
    {
      uint workercnt = opt.m_output_workers;
      NullOutputTeam* team = new NullOutputTeam(job, workercnt);
      add_team(team);
    }
    if (strcmp(opt.m_output_type, "ndb") == 0)
    {
      uint workercnt = opt.m_output_workers;
      RelayOpTeam* team = new RelayOpTeam(job, workercnt);
      add_team(team);
    }
    if (strcmp(opt.m_output_type, "ndb") == 0)
    {
      require(opt.m_db_workers != 0);
      uint workercnt = opt.m_db_workers * nodecnt;
      ExecOpTeam* team = new ExecOpTeam(job, workercnt);
      add_team(team);
    }
  }
  // row queues
  Stats& stats = m_stats;
  {
    m_rows_relay = new RowList;
    m_rows_relay->set_stats(stats, "rows-relay");
    uint rowbatch = opt.m_rowbatch;
    if (rowbatch != 0)
      m_rows_relay->m_rowbatch = rowbatch;
    uint rowbytes = opt.m_rowbytes;
    if (rowbytes != 0)
      m_rows_relay->m_rowbytes = rowbytes;
  }
  {
    for (uint i = 0; i < nodecnt; i++)
    {
      Name name("rows-exec", i);
      m_rows_exec[i] = new RowList;
      m_rows_exec[i]->set_stats(stats, name);
      uint rowbatch = opt.m_rowbatch;
      if (rowbatch != 0)
        m_rows_exec[i]->m_rowbatch = rowbatch;
      uint rowbytes = opt.m_rowbytes;
      if (rowbytes != 0)
        m_rows_exec[i]->m_rowbytes = rowbytes;
    }
  }
  {
    m_rows_reject = new RowList;
    m_rows_reject->set_stats(stats, "rows-reject");
  }
  job.m_state = JobState::State_created;
}

void
NdbImportImpl::Job::add_team(Team* team)
{
  require(team != 0);
  require(m_teamcnt < g_max_teams);
  m_teams[m_teamcnt] = team;
  m_teamcnt++;
}

int
NdbImportImpl::Job::add_table(const char* database,
                              const char* table,
                              uint& tabid)
{
  return m_impl.add_table(database, table, tabid, m_error);
}

void
NdbImportImpl::Job::set_table(uint tabid)
{
  (void)m_util.get_table(tabid);
  m_tabid = tabid;
}

void
NdbImportImpl::Job::do_start()
{
  const Opt& opt = m_util.c_opt;
  log1("start");
  m_timer.start();
  do
  {
    m_state = JobState::State_starting;
    start_diag_team();
    if (has_error())
    {
      m_state = JobState::State_stop;
      break;
    }
    if (opt.m_resume)
      start_resume();
    start_teams();
    if (has_error())
    {
      m_state = JobState::State_stop;
      break;
    }
    m_state = JobState::State_running;
    while (m_state != JobState::State_stop)
    {
      log2("running");
      check_teams(false);
      check_userstop();
      NdbSleep_MilliSleep(opt.m_checkloop);
    }
  } while (0);
  log1("stop");
  while (m_state != JobState::State_stopped)
  {
    log2("stopping");
    check_teams(true);
    if (m_state == JobState::State_stop)
      m_state = JobState::State_stopped;
    NdbSleep_MilliSleep(opt.m_checkloop);
  }
  log1("stopped");
  collect_teams();
  collect_stats();
  stop_diag_team();
  log1("rowmap out: " << m_rowmap_out);
  m_state = JobState::State_done;
  log1("done");
}

void
NdbImportImpl::Job::start_diag_team()
{
  Team* team = m_teams[0];
  team->do_create();
  team->do_start();
  if (team->has_error())
  {
    m_util.set_error_gen(m_error, __LINE__,
                         "failed to start team %u-%s (state file manager)",
                         team->m_teamno, team->m_name.str());
    return;
  }
  team->do_run();
  log1("diag team started");
}

void
NdbImportImpl::Job::start_resume()
{
  // copy entire old rowmap
  require(m_rowmap_out.empty());
  m_rowmap_out.add(m_rowmap_in);
  // input worker handles seek in do_init()
  log1("range_in: " << m_range_in);
}

void
NdbImportImpl::Job::start_teams()
{
  for (uint i = 1; i < m_teamcnt; i++)
  {
    Team* team = m_teams[i];
    team->do_create();
  }
  for (uint i = 1; i < m_teamcnt; i++)
  {
    Team* team = m_teams[i];
    team->do_start();
    if (team->has_error())
    {
      m_util.set_error_gen(m_error, __LINE__,
                           "failed to start team %u-%s",
                           team->m_teamno, team->m_name.str());
      return;
    }
  }
  for (uint i = 1; i < m_teamcnt; i++)
  {
    Team* team = m_teams[i];
    team->do_run();
  }
  log1("teams started");
}

void
NdbImportImpl::Job::check_teams(bool dostop)
{
  for (uint i = 1; i < m_teamcnt; i++)
  {
    Team* team = m_teams[i];
    if (team->m_state == TeamState::State_null)
    {
      // never started
      team->m_state = TeamState::State_stopped;
      continue;
    }
    team->check_workers();
    if (team->m_state == TeamState::State_stop)
    {
      team->do_stop();
    }
    if (dostop && team->m_state != TeamState::State_stopped)
    {
      team->do_stop();
    }
    if (!team->m_rowmap_out.empty())
    {
      // lock since diag team also writes to job rowmap
      m_rowmap_out.lock();
      m_rowmap_out.add(team->m_rowmap_out);
      log1("rowmap " << m_rowmap_out.size() << " <- "
                     << team->m_rowmap_out.size());
      m_stat_rowmap->add(m_rowmap_out.size());
      m_rowmap_out.unlock();
      team->m_rowmap_out.clear();
    }
  }
  for (int k = 0; k < g_teamstatecnt; k++)
    m_teamstates[k] = 0;
  for (uint i = 1; i < m_teamcnt; i++)
  {
    Team* team = m_teams[i];
    int k = (int)team->m_state;
    require(k >= 0 && k < g_teamstatecnt);
    m_teamstates[k]++;
  }
  if (m_teamstates[TeamState::State_stopped] == m_teamcnt - 1)
    m_state = JobState::State_stop;
}

void
NdbImportImpl::Job::check_userstop()
{
  if (m_dostop && m_state != JobState::State_stop)
  {
    log1("stop by user request");
    m_util.set_error_gen(m_error, __LINE__,
                         "stop by user request");
    m_state = JobState::State_stop;
  }
  if (NdbImportUtil::g_stop_all && m_state != JobState::State_stop)
  {
    log1("stop by user interrupt");
    m_util.set_error_gen(m_error, __LINE__,
                         "stop by user interrupt");
    m_state = JobState::State_stop;
  }
}

void
NdbImportImpl::Job::collect_teams()
{
  char error_team[100] = "";
  {
    Team* team = m_teams[0];
    if (team->has_error())
      sprintf(error_team + strlen(error_team),
              " %u-%s", team->m_teamno, team->m_name.str());
  }
  for (uint i = 1; i < m_teamcnt; i++)
  {
    Team* team = m_teams[i];
    require(team->m_state == TeamState::State_stopped);
    if (team->has_error())
      sprintf(error_team + strlen(error_team),
              " %u-%s", team->m_teamno, team->m_name.str());
  }
  if (strlen(error_team) != 0)
  {
    m_util.set_error_gen(m_error, __LINE__,
                         "error in teams:%s", error_team);
  }
}

void
NdbImportImpl::Job::collect_stats()
{
  m_timer.stop();
  uint64 msec = m_timer.elapsed_msec();
  if (msec == 0)
    msec = 1;
  const RowMap& rowmap = m_rowmap_out;
  uint64 rows;
  uint64 reject;
  rowmap.get_total(rows, reject);
  uint64 rowssec = (rows * 1000) / msec;
  m_stat_rows->add(rows);
  m_stat_reject->add(reject);
  m_stat_runtime->add(msec);
  m_stat_rowssec->add(rowssec);
}

void
NdbImportImpl::Job::stop_diag_team()
{
  const Opt& opt = m_util.c_opt;
  Team* team = m_teams[0];
  team->do_stop();
  while (team->m_state != TeamState::State_stopped)
  {
    NdbSleep_MilliSleep(opt.m_checkloop);
  }
  log1("diag team stopped");
}

void
NdbImportImpl::Job::do_stop()
{
  m_dostop = true;
}

// team

NdbImportImpl::Team::Team(Job& job,
                          const char* name,
                          uint workercnt) :
  m_job(job),
  m_impl(job.m_impl),
  m_util(m_impl.m_util),
  m_teamno(job.m_teamcnt),
  m_name(name),
  m_workercnt(workercnt),
  m_rowmap_out(m_util)
{
  m_state = TeamState::State_null;
  m_workers = 0;
  for (int k = 0; k < g_workerstatecnt; k++)
    m_workerstates[k] = 0;
  m_tabid = Inval_uint;
  m_is_diag = false;
  // stats
  Stats& stats = m_job.m_stats;
  {
    const Name name(m_name, "runtime");
    Stat* stat = stats.create(name, 0, 0);
    m_stat_runtime = stat;
  }
  {
    const Name name(m_name, "slice");
    Stat* stat = stats.create(name, 0, 0);
    m_stat_slice = stat;
  }
  {
    const Name name(m_name, "idleslice");
    Stat* stat = stats.create(name, 0, 0);
    m_stat_idleslice = stat;
  }
  {
    const Name name(m_name, "idlerun");
    Stat* stat = stats.create(name, 0, 0);
    m_stat_idlerun = stat;
  }
  {
    const Name name(m_name, "utime");
    uint parent = m_job.m_stat_utime->m_id;
    Stat* stat = stats.create(name, parent, 0);
    m_stat_utime = stat;
  }
  {
    const Name name(m_name, "stime");
    uint parent = m_job.m_stat_stime->m_id;
    Stat* stat = stats.create(name, parent, 0);
    m_stat_stime = stat;
  }
  {
    const Name name(m_name, "rowmap");
    Stat* stat = stats.create(name, 0, 0);
    m_stat_rowmap = stat;
  }
  log1("ctor");
}

NdbImportImpl::Team::~Team()
{
  log1("dtor");
  if (m_workers != 0)
  {
    for (uint n = 0; n < m_workercnt; n++)
    {
      Worker* w = m_workers[n];
      delete w;
    }
    delete [] m_workers;
  }
}

void
NdbImportImpl::Team::do_create()
{
  log1("do_create");
  require(m_state == TeamState::State_null);
  require(m_workers == 0);
  require(m_workercnt > 0);
  m_workers = new Worker* [m_workercnt];
  for (uint n = 0; n < m_workercnt; n++)
  {
    m_workers[n] = 0;
  }
  for (uint n = 0; n < m_workercnt; n++)
  {
    Worker* w = create_worker(n);
    require(w != 0);
    m_workers[n] = w;
  }
  m_state = TeamState::State_created;
}

void
NdbImportImpl::Team::do_start()
{
  log1("start");
  m_timer.start();
  require(m_state == TeamState::State_created);
  require(m_workers != 0);
  do_init();
  if (has_error())
  {
    m_state = TeamState::State_stop;
    return;
  }
  for (uint n = 0; n < m_workercnt; n++)
  {
    Worker* w = get_worker(n);
    start_worker(w);
  }
  wait_workers(WorkerState::State_wait);
  m_state = TeamState::State_started;
}

extern "C" { static void* start_worker_c(void* data); }

static void*
start_worker_c(void* data)
{
  NdbImportImpl::Worker* w = (NdbImportImpl::Worker*)data;
  require(w != 0);
  w->do_start();
  return 0;
}

NdbImportImpl::Worker*
NdbImportImpl::Team::get_worker(uint n)
{
  require(n < m_workercnt);
  Worker* w = m_workers[n];
  require(w != 0);
  return w;
}

void
NdbImportImpl::Team::start_worker(Worker* w)
{
  NDB_THREAD_PRIO prio = NDB_THREAD_PRIO_MEAN;
  uint stack_size = 64*1024;
  w->m_thread = NdbThread_Create(
    start_worker_c, (void**)w, stack_size, w->m_name, prio);
  require(w->m_thread != 0);
}

void
NdbImportImpl::Team::wait_workers(WorkerState::State state)
{
  log1("wait_workers");
  for (uint n = 0; n < m_workercnt; n++)
  {
    Worker* w = get_worker(n);
    wait_worker(w, state);
  }
}

void
NdbImportImpl::Team::wait_worker(Worker* w, WorkerState::State state)
{
  log1("wait_worker: " << *w << " for " << g_str_state(state));
  const Opt& opt = m_util.c_opt;
  uint timeout = opt.m_idlesleep;
  w->lock();
  while (1)
  {
    log1(*w << ": wait for " << g_str_state(state));
    if (w->m_state == state || w->m_state == WorkerState::State_stopped)
      break;
    w->wait(timeout);
  }
  w->unlock();
}

void
NdbImportImpl::Team::do_run()
{
  log1("do_run");
  if (has_error())
  {
    if (m_state != TeamState::State_stopped)
      m_state = TeamState::State_stop;
    return;
  }
  for (uint n = 0; n < m_workercnt; n++)
  {
    Worker* w = get_worker(n);
    run_worker(w);
  }
  wait_workers(WorkerState::State_running);
  m_state = TeamState::State_running;
}

void
NdbImportImpl::Team::run_worker(Worker* w)
{
  log1("run_worker: " << *w);
  w->lock();
  w->m_state = WorkerState::State_run;
  w->signal();
  w->unlock();
}

void
NdbImportImpl::Team::check_workers()
{
  log2("check_workers");
  for (int k = 0; k < g_workerstatecnt; k++)
    m_workerstates[k] = 0;
  for (uint n = 0; n < m_workercnt; n++)
  {
    Worker* w = get_worker(n);
    w->lock();
    log2("check_worker " << *w);
    int k = (int)w->m_state;
    require(k >= 0 && k < g_workerstatecnt);
    m_workerstates[k]++;
    // transfer rowmap while worker is locked
    if (!w->m_rowmap_out.empty())
    {
      m_rowmap_out.add(w->m_rowmap_out);
      log1("rowmap " << m_rowmap_out.size() << " <- "
                     << w->m_rowmap_out.size());
      m_stat_rowmap->add(m_rowmap_out.size());
      w->m_rowmap_out.clear();
    }
    log2("rowmap out: " << m_rowmap_out);
    w->unlock();
  }
  if (m_workerstates[WorkerState::State_stopped] == m_workercnt)
  {
    if (m_state != TeamState::State_stopped)
      m_state = TeamState::State_stop;
  }
  if (has_error())
  {
    if (m_state != TeamState::State_stopped)
      m_state = TeamState::State_stop;
  }
  log2("check_workers done");
}

void
NdbImportImpl::Team::do_stop()
{
  log1("do_stop");
  for (uint n = 0; n < m_workercnt; n++)
  {
    Worker* w = get_worker(n);
    stop_worker(w);
  }
  wait_workers(WorkerState::State_stopped);
  // transfer final rowmap entries
  for (uint n = 0; n < m_workercnt; n++)
  {
    Worker* w = get_worker(n);
    if (!w->m_rowmap_out.empty())
    {
      m_rowmap_out.add(w->m_rowmap_out);
      m_stat_rowmap->add(m_rowmap_out.size());
      w->m_rowmap_out.clear();
    }
  }
  do_end();
  for (uint n = 0; n < m_workercnt; n++)
  {
    Worker* w = get_worker(n);
    if (w->m_thread != 0)
      w->join();
    w->m_thread = 0;
  }
  m_state = TeamState::State_stopped;
  m_timer.stop();
  // stats
  {
    uint64 msec = m_timer.elapsed_msec();
    if (msec == 0)
      msec = 1;
    m_stat_runtime->add(msec);
  }
}

void
NdbImportImpl::Team::stop_worker(Worker* w)
{
  log1("stop_worker: " << *w);
  w->lock();
  switch (w->m_state) {
  case WorkerState::State_null:
    w->m_state = WorkerState::State_stopped;
    w->signal();
    break;
  case WorkerState::State_wait:
    w->m_state = WorkerState::State_stopped;
    w->signal();
    break;
  case WorkerState::State_running:
    /*
     * Here we do not interfere with worker state but ask it
     * to stop when convenient.  It is enough and simpler for
     * only producers to act on this flag.
     */
    w->m_dostop = true;
    w->signal();
    break;
  case WorkerState::State_stop:
    /*
     * Worker is about to stop, allow it to do so.  It is either
     * ready or it is reacting to m_dostop.
     */
    break;
  case WorkerState::State_stopped:
    break;
  default:
    require(false);
    break;
  }
  w->unlock();
}

void
NdbImportImpl::Team::set_table(uint tabid)
{
  (void)m_util.get_table(tabid);
  m_tabid = tabid;
}

// worker

NdbImportImpl::Worker::Worker(NdbImportImpl::Team& team, uint n) :
  m_team(team),
  m_impl(m_team.m_impl),
  m_util(m_impl.m_util),
  m_workerno(n),
  m_name(team.m_name, m_workerno),
  m_rowmap_out(m_util),
  m_error(m_team.m_error)
{
  m_state = WorkerState::State_null;
  m_dostop = false;
  m_slice = 0;
  m_idleslice = 0;
  m_idle = false;
  m_idlerun = 0;
  m_seed = 0;
  // stats
  Stats& stats = m_team.m_job.m_stats;
  {
    const Name name(m_name, "slice");
    uint parent = m_team.m_stat_slice->m_id;
    Stat* stat = stats.create(name, parent, 0);
    m_stat_slice = stat;
  }
  {
    const Name name(m_name, "idleslice");
    uint parent = m_team.m_stat_idleslice->m_id;
    Stat* stat = stats.create(name, parent, 0);
    m_stat_idleslice = stat;
  }
  {
    const Name name(m_name, "idlerun");
    uint parent = m_team.m_stat_idlerun->m_id;
    Stat* stat = stats.create(name, parent, 0);
    m_stat_idlerun = stat;
  }
  {
    const Name name(m_name, "utime");
    uint parent = m_team.m_stat_utime->m_id;
    Stat* stat = stats.create(name, parent, 0);
    m_stat_utime = stat;
  }
  {
    const Name name(m_name, "stime");
    uint parent = m_team.m_stat_stime->m_id;
    Stat* stat = stats.create(name, parent, 0);
    m_stat_stime = stat;
  }
  {
    const Name name(m_name, "rowmap");
    Stat* stat = stats.create(name, 0, 0);
    m_stat_rowmap = stat;
  }
  log1("ctor");
}

NdbImportImpl::Worker::~Worker()
{
  log1("dtor");
}

void
NdbImportImpl::Worker::do_start()
{
  log1("start");
  const Opt& opt = m_util.c_opt;
  uint timeout = opt.m_idlesleep;
  do_init();
  if (has_error())
  {
    m_state = WorkerState::State_stop;
  }
  m_seed = (unsigned)(NdbHost_GetProcessId() ^ m_workerno);
  while (m_state != WorkerState::State_stopped)
  {
    log2("slice: " << m_slice);
    lock();
    m_idle = false;
    switch (m_state) {
    case WorkerState::State_null:
      m_state = WorkerState::State_wait;
      break;
    case WorkerState::State_wait:
      wait(timeout);
      break;
    case WorkerState::State_run:
      m_state = WorkerState::State_running;
      break;
    case WorkerState::State_running:
      do_run();
      m_slice++;
      if (m_idle)
      {
        m_idleslice++;
        m_idlerun++;
        m_stat_idlerun->add(m_idlerun);
      }
      else
      {
        m_idlerun = 0;
      }
      break;
    case WorkerState::State_stop:
      do_end();
      m_slice++;
      m_stat_slice->add(m_slice);
      m_stat_idleslice->add(m_idleslice);
      m_timer.stop();
      m_stat_utime->add(m_timer.m_utime_msec);
      m_stat_stime->add(m_timer.m_stime_msec);
      m_state = WorkerState::State_stopped;
      break;
    case WorkerState::State_stopped:
      require(false);
      break;
    default:
      require(false);
      break;
    }
    if (has_error())
    {
      if (m_state != WorkerState::State_stopped)
        m_state = WorkerState::State_stop;
    }
    signal();
    unlock();
    if (!m_team.m_is_diag)
    {
      if (m_idlerun > opt.m_idlespin && opt.m_idlesleep != 0)
      {
        NdbSleep_MilliSleep(opt.m_idlesleep);
      }
    }
    else
    {
      NdbSleep_MilliSleep(opt.m_checkloop);
    }
  }
  log1("stopped");
}

NdbImportImpl::Worker*
NdbImportImpl::Worker::next_worker()
{
  Team& team = m_team;
  require(team.m_workercnt > 0);
  uint n = (m_workerno + 1) % team.m_workercnt;
  Worker* w = team.get_worker(n);
  return w;
}

// print

const char*
NdbImportImpl::g_str_state(JobState::State state)
{
  const char* str = 0;
  switch (state) {
  case JobState::State_null:
    str = "null";
    break;
  case JobState::State_created:
    str = "created";
    break;
  case JobState::State_starting:
    str = "starting";
    break;
  case JobState::State_running:
    str = "running";
    break;
  case JobState::State_stop:
    str = "stop";
    break;
  case JobState::State_stopped:
    str = "stopped";
    break;
  case JobState::State_done:
    str = "done";
    break;
  }
  require(str != 0);
  return str;
}

const char*
NdbImportImpl::g_str_state(TeamState::State state)
{
  const char* str = 0;
  switch (state) {
  case TeamState::State_null:
    str = "null";
    break;
  case TeamState::State_created:
    str = "created";
    break;
  case TeamState::State_started:
    str = "started";
    break;
  case TeamState::State_running:
    str = "running";
    break;
  case TeamState::State_stop:
    str = "stop";
    break;
  case TeamState::State_stopped:
    str = "stopped";
    break;
  }
  require(str != 0);
  return str;
}

const char*
NdbImportImpl::g_str_state(WorkerState::State state)
{
  const char* str = 0;
  switch (state) {
  case WorkerState::State_null:
    str = "null";
    break;
  case WorkerState::State_wait:
    str = "wait";
    break;
  case WorkerState::State_run:
    str = "run";
    break;
  case WorkerState::State_running:
    str = "running";
    break;
  case WorkerState::State_stop:
    str = "stop";
    break;
  case WorkerState::State_stopped:
    str = "stopped";
    break;
  }
  require(str != 0);
  return str;
}

void
NdbImportImpl::Job::str_state(char* str) const
{
  strcpy(str, g_str_state(m_state));
}

void
NdbImportImpl::Team::str_state(char* str) const
{
  strcpy(str, g_str_state(m_state));
}

void
NdbImportImpl::Worker::str_state(char* str) const
{
  strcpy(str, g_str_state(m_state));
}

NdbOut&
operator<<(NdbOut& out, const NdbImportImpl::Job& job)
{
  char str[100];
  job.str_state(str);
  out << "J-" << job.m_jobno << " [" << str << "]";
  if (job.has_error())
  {
    const Error& error = job.m_error;
    const char* typetext = error.gettypetext();
    out << " error[" << typetext << "-" << error.code << "]";
  }
  return out;
}

NdbOut&
operator<<(NdbOut& out, const NdbImportImpl::Team& team)
{
  char str[100];
  team.str_state(str);
  out << "T-" << team.m_teamno << " " << team.m_name << " [" << str << "]";
  if (team.has_error())
  {
    const Error& error = team.m_error;
    const char* typetext = error.gettypetext();
    out << " error[" << typetext << "-" << error.code << "]";
  }
  return out;
}

NdbOut&
operator<<(NdbOut& out, const NdbImportImpl::Worker& w)
{
  char str[100];
  w.str_state(str);
  out << "W " << w.m_name << " [" << str << "]";
  return out;
}

// random input team

NdbImportImpl::RandomInputTeam::RandomInputTeam(Job& job,
                                                uint workercnt) :
  Team(job, "random-input", workercnt)
{
}

NdbImportImpl::RandomInputTeam::~RandomInputTeam()
{
}

NdbImportImpl::Worker*
NdbImportImpl::RandomInputTeam::create_worker(uint n)
{
  RandomInputWorker* w = new RandomInputWorker(*this, n);
  return w;
}

void
NdbImportImpl::RandomInputTeam::do_init()
{
  log1("do_init");
  set_table(m_job.m_tabid);
}

void
NdbImportImpl::RandomInputTeam::do_end()
{
  log1("do_end");
  RowList& rows_out = *m_job.m_rows_relay;
  rows_out.lock();
  require(!rows_out.m_eof);
  rows_out.m_eof = true;
  rows_out.unlock();
}

NdbImportImpl::RandomInputWorker::RandomInputWorker(Team& team, uint n) :
  Worker(team, n)
{
  m_seed = 0;
}

NdbImportImpl::RandomInputWorker::~RandomInputWorker()
{
}

void
NdbImportImpl::RandomInputWorker::do_init()
{
  log1("do_init");
}

void
NdbImportImpl::RandomInputWorker::do_run()
{
  log2("do_run");
  const Opt& opt = m_util.c_opt;
  const uint tabid = m_team.m_tabid;
  const Table& table = m_util.get_table(tabid);
  RowList& rows_out = *m_team.m_job.m_rows_relay;
  uint64 max_rows = opt.m_max_rows != 0 ? opt.m_max_rows : UINT64_MAX;
  rows_out.lock();
  for (uint i = 0; i < opt.m_rowbatch; i++)
  {
    if (rows_out.totcnt() >= max_rows)
    {
      log1("stop at max rows " << max_rows);
      m_state = WorkerState::State_stop;
      break;
    }
    if (m_dostop)
    {
      log1("stop by request");
      m_state = WorkerState::State_stop;
      break;
    }
    uint64 rowid = rows_out.totcnt();
    Row* row = create_row(rowid, table);
    if (row == 0)
    {
      require(has_error());
      break;
    }
    require(row->m_tabid == table.m_tabid);
    if (!rows_out.push_back(row))
    {
      m_idle = true;
      break;
    }
  }
  rows_out.unlock();
}

void
NdbImportImpl::RandomInputWorker::do_end()
{
  log1("do_end");
}

NdbImportImpl::Row*
NdbImportImpl::RandomInputWorker::create_row(uint64 rowid, const Table& table)
{
  Row* row = m_util.alloc_row(table);
  row->m_rowid = rowid;
  const Attrs& attrs = table.m_attrs;
  const uint attrcnt = attrs.size();
  char keychr[100];
  uint keylen;
  sprintf(keychr, "%llu:", rowid);
  keylen = strlen(keychr);
  for (uint i = 0; i < attrcnt; i++)
  {
    const Attr& attr = attrs[i];
    switch (attr.m_type) {
    case NdbDictionary::Column::Unsigned:
      {
        uint32 val = (uint32)rowid;
        attr.set_value(row, &val, sizeof(val));
      }
      break;
    case NdbDictionary::Column::Bigunsigned:
      {
        uint64 val = rowid;
        attr.set_value(row, &val, sizeof(val));
      }
      break;
    case NdbDictionary::Column::Varchar:
      {
        const uint maxsize = 255;
        uchar val[maxsize];
        uint maxlen = attr.m_length;
        uint len = attr.m_pk ? maxlen : get_rand() % (maxlen + 1);
        for (uint i = 0; i < len; i++)
          val[i] = keychr[i % keylen];
        attr.set_value(row, val, len);
      }
      break;
    case NdbDictionary::Column::Longvarchar:
      {
        const uint maxsize = 65535;
        uchar val[maxsize];
        uint maxlen = attr.m_length;
        uint len = attr.m_pk ? maxlen : get_rand() % (maxlen + 1);
        for (uint i = 0; i < len; i++)
          val[i] = keychr[i % keylen];
        attr.set_value(row, val, len);
      }
      break;
    default:
      {
        m_util.set_error_usage(m_error, __LINE__,
                              "column type %d not supported for random input",
                              (int)attr.m_type);
        return 0;
      }
      break;
    }
  }
  return row;
}

// csv input team

NdbImportImpl::CsvInputTeam::CsvInputTeam(Job& job,
                                          uint workercnt) :
  Team(job, "csv-input", workercnt),
  m_file(m_util, m_error)
{
  // stats
  Stats& stats = m_job.m_stats;
  {
    const Name name(m_name, "waittail");
    Stat* stat = stats.create(name, 0, 0);
    m_stat_waittail = stat;
  }
  {
    const Name name(m_name, "waitmove");
    Stat* stat = stats.create(name, 0, 0);
    m_stat_waitmove = stat;
  }
  {
    const Name name(m_name, "movetail");
    Stat* stat = stats.create(name, 0, 0);
    m_stat_movetail = stat;
  }
}

NdbImportImpl::CsvInputTeam::~CsvInputTeam()
{
}

NdbImportImpl::Worker*
NdbImportImpl::CsvInputTeam::create_worker(uint n)
{
  CsvInputWorker* w = new CsvInputWorker(*this, n);
  return w;
}

void
NdbImportImpl::CsvInputTeam::do_init()
{
  log1("do_init");
  const Opt& opt = m_util.c_opt;
  const OptCsv& optcsv = opt.m_optcsv;
  if (m_impl.m_csv.set_spec(m_csvspec, optcsv, OptCsv::ModeInput) == -1)
  {
    require(m_util.has_error());
    return;
  }
  set_table(m_job.m_tabid);
  WorkerFile& file = m_file;
  file.set_path(opt.m_input_file);
  if (file.do_open(File::Read_flags) == -1)
  {
    require(has_error());
    m_job.m_fatal = true;
    return;
  }
  log1("file: opened: " << file.get_path());
  const uint workerno = 0;
  file.m_workerno = workerno;
  CsvInputWorker* w = static_cast<CsvInputWorker*>(get_worker(workerno));
  w->m_firstread = true;
}

void
NdbImportImpl::CsvInputTeam::do_end()
{
  log1("do_end");
  WorkerFile& file = m_file;
  if (file.do_close() == -1)
  {
    require(has_error());
    // continue
  }
  RowList& rows_out = *m_job.m_rows_relay;
  rows_out.lock();
  require(!rows_out.m_eof);
  rows_out.m_eof = true;
  rows_out.unlock();
}

NdbImportImpl::CsvInputWorker::CsvInputWorker(Team& team, uint n) :
  Worker(team, n),
  m_buf(true)
{
  m_inputstate = InputState::State_null;
  m_csvinput = 0;
  m_firstread = false;
  m_eof = false;
}

NdbImportImpl::CsvInputWorker::~CsvInputWorker()
{
  delete m_csvinput;
}

void
NdbImportImpl::CsvInputWorker::do_init()
{
  log1("do_init");
  const Opt& opt = m_util.c_opt;
  const CsvSpec& csvspec = static_cast<CsvInputTeam&>(m_team).m_csvspec;
  const uint tabid = m_team.m_tabid;
  const Table& table = m_util.get_table(tabid);
  uint pagesize = opt.m_pagesize;
  uint pagecnt = opt.m_pagecnt;
  m_buf.alloc(pagesize, 2 * pagecnt);
  RowList& rows_out = *m_team.m_job.m_rows_relay;
  RowList& rows_reject = *m_team.m_job.m_rows_reject;
  RowMap& rowmap_in = m_team.m_job.m_rowmap_in;
  m_csvinput = new CsvInput(m_impl.m_csv,
                            Name("csvinput", m_workerno),
                            csvspec,
                            table,
                            m_buf,
                            rows_out,
                            rows_reject,
                            rowmap_in,
                            m_team.m_job.m_stats);
  m_csvinput->do_init();
  if (m_firstread)
  {
    // this worker does first read
    if (opt.m_resume)
    {
      CsvInputTeam& team = static_cast<CsvInputTeam&>(m_team);
      WorkerFile& file = team.m_file;
      RangeList& ranges_in = rowmap_in.m_ranges;
      require(!ranges_in.empty());
      Range range_in = *ranges_in.front();
      /*
       * First range is likely to be the big one.  If the range
       * starts with rowid 0 seek to the end and erase it.
       * In rare cases rowid 0 may not yet have been processed
       * due to an early error and rejected out of order rows.
       */
      if (range_in.m_start == 0)
      {
        uint64 seekpos = range_in.m_endpos;
        if (file.do_seek(seekpos) == -1)
        {
          require(has_error());
          return;
        }
        log1("file " << file.get_path() << ": "
             "seek to pos " << seekpos << " done");
        m_csvinput->do_resume(range_in);
        (void)ranges_in.pop_front();
      }
      else
      {
        log1("file " << file.get_path() << ": "
             "cannot seek first rowid=" << range_in.m_start);
      }
    }
  }
}

void
NdbImportImpl::CsvInputWorker::do_run()
{
  log2("do_run");
  switch (m_inputstate) {
  case InputState::State_null:
    m_inputstate = InputState::State_lock;
    break;
  case InputState::State_lock:
    state_lock();
    break;
  case InputState::State_read:
    state_read();
    break;
  case InputState::State_waittail:
    state_waittail();
    break;
  case InputState::State_parse:
    state_parse();
    break;
  case InputState::State_movetail:
    state_movetail();
    break;
  case InputState::State_eval:
    state_eval();
    break;
  case InputState::State_send:
    state_send();
    break;
  case InputState::State_eof:
    m_state = WorkerState::State_stop;
    break;
  default:
    require(false);
    break;
  }
}

void
NdbImportImpl::CsvInputWorker::do_end()
{
  log1("do_end");
}

void
NdbImportImpl::CsvInputWorker::state_lock()
{
  log2("state_lock");
  if (m_dostop)
  {
    log1("stop by request");
    m_state = WorkerState::State_stop;
    return;
  }
  WorkerFile& file = static_cast<CsvInputTeam&>(m_team).m_file;
  file.lock();
  if (file.m_workerno == m_workerno)
    m_inputstate = InputState::State_read;
  else
    m_idle = true;
  file.unlock();
}

void
NdbImportImpl::CsvInputWorker::state_read()
{
  log2("state_read");
  if (m_dostop)
  {
    log1("stop by request");
    m_state = WorkerState::State_stop;
    return;
  }
  WorkerFile& file = static_cast<CsvInputTeam&>(m_team).m_file;
  Buf& buf = m_buf;
  buf.reset();
  if (file.do_read(buf) == -1)
  {
    require(has_error());
    return;
  }
  log2("file: read: " << buf.m_len);
  if (buf.m_eof)
  {
    log1("eof");
    m_eof = true;
  }
  file.lock();
  CsvInputWorker* w2 = static_cast<CsvInputWorker*>(next_worker());
  file.m_workerno = w2->m_workerno;
  file.unlock();
  if (m_firstread)
  {
    m_inputstate = InputState::State_parse;
    m_firstread = false;
  }
  else
  {
    m_inputstate = InputState::State_waittail;
  }
}

void
NdbImportImpl::CsvInputWorker::state_waittail()
{
  log2("state_waittail");
  if (m_dostop)
  {
    log1("stop by request");
    m_state = WorkerState::State_stop;
    return;
  }
  CsvInputTeam& team = static_cast<CsvInputTeam&>(m_team);
  team.m_stat_waittail->add(1);
  m_idle = true;
}

void
NdbImportImpl::CsvInputWorker::state_parse()
{
  log2("state_parse");
  m_csvinput->do_parse();
  log2("lines parsed:" << m_csvinput->m_line_list.cnt());
  m_inputstate = InputState::State_movetail;
}

void
NdbImportImpl::CsvInputWorker::state_movetail()
{
  log2("state_movetail");
  if (m_dostop)
  {
    log1("stop by request");
    m_state = WorkerState::State_stop;
    return;
  }
  CsvInputTeam& team = static_cast<CsvInputTeam&>(m_team);
  CsvInputWorker* w2 = static_cast<CsvInputWorker*>(next_worker());
  w2->lock();
  log2("next worker: " << *w2);
  if (w2->m_inputstate == InputState::State_waittail)
  {
    m_csvinput->do_movetail(*w2->m_csvinput);
    team.m_stat_movetail->add(1);
    m_inputstate = InputState::State_eval;
    w2->m_inputstate = InputState::State_parse;
  }
  else if (w2->m_inputstate == InputState::State_eof)
  {
    m_inputstate = InputState::State_eval;
  }
  else
  {
    // cannot move tail yet
    team.m_stat_waitmove->add(1);
    m_idle = true;
  }
  w2->unlock();
}

void
NdbImportImpl::CsvInputWorker::state_eval()
{
  log2("state_eval");
  m_csvinput->do_eval();
  m_inputstate = InputState::State_send;
}

void
NdbImportImpl::CsvInputWorker::state_send()
{
  log2("state_send");
  const Opt& opt = m_util.c_opt;
  do
  {
    // max-rows is a test option, it need not be exact
    if (opt.m_max_rows != 0)
    {
      RowList& rows_out = *m_team.m_job.m_rows_relay;
      if (rows_out.totcnt() >= opt.m_max_rows)
      {
        log1("stop on max-rows option");
        m_inputstate = InputState::State_eof;
        break;
      }
    }
    uint curr = 0;
    uint left = 0;
    m_csvinput->do_send(curr, left);
    log2("send: rows curr=" << curr << " left=" << left);
    if (m_csvinput->has_error())
    {
      m_util.copy_error(m_error, m_csvinput->m_error);
      break;
    }
    if (left != 0)
    {
      log2("send not ready");
      m_idle = true;
      break;
    }
    if (!m_eof)
    {
      log2("send ready and not eof");
      // stop if csv error
      if (m_csvinput->has_error())
      {
        m_util.copy_error(m_error, m_csvinput->m_error);
        break;
      }
      if (m_dostop)
      {
        log1("stop by request");
        m_state = WorkerState::State_stop;
        break;
      }
      m_inputstate = InputState::State_lock;
      break;
    }
    log2("send ready and eof");
    m_inputstate = InputState::State_eof;
  } while (0);
}

// print

const char*
NdbImportImpl::g_str_state(InputState::State state)
{
  const char* str = 0;
  switch (state) {
  case InputState::State_null:
    str = "null";
    break;
  case InputState::State_lock:
    str = "lock";
    break;
  case InputState::State_read:
    str = "read";
    break;
  case InputState::State_waittail:
    str = "waittail";
    break;
  case InputState::State_parse:
    str = "parse";
    break;
  case InputState::State_movetail:
    str = "movetail";
    break;
  case InputState::State_eval:
    str = "eval";
    break;
  case InputState::State_send:
    str = "send";
    break;
  case InputState::State_eof:
    str = "eof";
    break;
  }
  require(str != 0);
  return str;
}

void
NdbImportImpl::CsvInputWorker::str_state(char* str) const
{
  sprintf(str, "%s/%s", g_str_state(m_state), g_str_state(m_inputstate));
}

// null output team

NdbImportImpl::NullOutputTeam::NullOutputTeam(Job& job,
                                              uint workercnt) :
  Team(job, "null-output", workercnt)
{
}

NdbImportImpl::NullOutputTeam::~NullOutputTeam()
{
}

NdbImportImpl::Worker*
NdbImportImpl::NullOutputTeam::create_worker(uint n)
{
  NullOutputWorker* w = new NullOutputWorker(*this, n);
  return w;
}

void
NdbImportImpl::NullOutputTeam::do_init()
{
  log1("do_init");
}

void
NdbImportImpl::NullOutputTeam::do_end()
{
  log1("do_end");
}

NdbImportImpl::NullOutputWorker::NullOutputWorker(Team& team, uint n) :
  Worker(team, n)
{
}

NdbImportImpl::NullOutputWorker::~NullOutputWorker()
{
}

void
NdbImportImpl::NullOutputWorker::do_init()
{
  log1("do_init");
}

void
NdbImportImpl::NullOutputWorker::do_run()
{
  log2("do_run");
  RowList& rows_in = *m_team.m_job.m_rows_relay;
  rows_in.lock();
  Row* row = rows_in.pop_front();
  bool eof = (row == 0 && rows_in.m_eof);
  rows_in.unlock();
  if (eof)
  {
    m_state = WorkerState::State_stop;
    return;
  }
  if (row == 0)
  {
    m_idle = true;
    return;
  }
  const uint tabid = row->m_tabid;
  (void)m_util.get_table(tabid);
  m_impl.m_util.free_row(row);
}

void
NdbImportImpl::NullOutputWorker::do_end()
{
  log1("do_end");
}

// op

NdbImportImpl::Op::Op()
{
  m_row = 0;
  m_rowop = 0;
  m_opcnt = 0;
  m_opsize = 0;
}

NdbImportImpl::OpList::OpList()
{
}

NdbImportImpl::OpList::~OpList()
{
}

// tx

NdbImportImpl::Tx::Tx(DbWorker* w) :
  m_worker(w)
{
  Stats& stats = w->m_team.m_job.m_stats;
  m_trans = 0;
  m_ops.set_stats(stats, "op-used");
}

NdbImportImpl::Tx::~Tx()
{
  require(m_trans == 0);
}

NdbImportImpl::TxList::TxList()
{
}

NdbImportImpl::TxList::~TxList()
{
}

// db team

NdbImportImpl::DbTeam::DbTeam(Job& job,
                              const char* name,
                              uint workercnt) :
  Team(job, name, workercnt)
{
}

NdbImportImpl::DbTeam::~DbTeam()
{
}

NdbImportImpl::DbWorker::DbWorker(Team& team, uint n) :
  Worker(team, n)
{
  m_ndb = 0;
  Stats& stats = team.m_job.m_stats;
  m_op_free.set_stats(stats, "op-free");
  m_tx_free.set_stats(stats, "tx-free");
  m_tx_open.set_stats(stats, "tx-open");
}

NdbImportImpl::DbWorker::~DbWorker()
{
  require(m_tx_open.cnt() == 0);
  delete m_ndb;
}

int
NdbImportImpl::DbWorker::create_ndb(uint transcnt)
{
  Connect& c = m_impl.c_connect;
  require(m_ndb == 0);
  Ndb* ndb = 0;
  do
  {
    uint index = m_impl.c_connectionindex;
    require(index < c.m_connectioncnt);
    ndb = new Ndb(c.m_connections[index]);
    m_impl.c_connectionindex = (index + 1) % c.m_connectioncnt;
    require(ndb != 0);
    if (ndb->init(transcnt) != 0)
    {
      m_util.set_error_ndb(m_error, __LINE__, ndb->getNdbError());
      break;
    }
    m_ndb = ndb;
    return 0;
  } while (0);
  delete ndb;
  return -1;
}

NdbImportImpl::Op*
NdbImportImpl::DbWorker::alloc_op()
{
  Op* op = m_op_free.pop_front();
  if (op == 0)
  {
    op = new Op;
  }
  return op;
}

void
NdbImportImpl::DbWorker::free_op(Op* op)
{
  m_op_free.push_back(op);
}

NdbImportImpl::Tx*
NdbImportImpl::DbWorker::start_trans()
{
  log2("start_trans");
  TxList& tx_free = m_tx_free;
  TxList& tx_open = m_tx_open;
  require(m_ndb != 0);
  NdbTransaction* trans = m_ndb->startTransaction();
  if (trans == 0)
  {
    return 0;
  }
  Tx* tx = tx_free.pop_front();
  if (tx == 0)
  {
    tx = new Tx(this);
  }
  require(tx != 0);
  require(tx->m_trans == 0);
  require(tx->m_ops.cnt() == 0);
  tx->m_trans = trans;
  tx_open.push_back(tx);
  return tx;
}

NdbImportImpl::Tx*
NdbImportImpl::DbWorker::start_trans(const NdbRecord* keyrec,
                                     const char* keydata,
                                     uchar* xfrmbuf, uint xfrmbuflen)
{
  log2("start_trans");
  TxList& tx_free = m_tx_free;
  TxList& tx_open = m_tx_open;
  require(m_ndb != 0);
  NdbTransaction* trans = m_ndb->startTransaction(keyrec, keydata,
                                                  xfrmbuf, xfrmbuflen);
  if (trans == 0)
  {
    return 0;
  }
  Tx* tx = tx_free.pop_front();
  if (tx == 0)
  {
    tx = new Tx(this);
  }
  require(tx != 0);
  require(tx->m_trans == 0);
  require(tx->m_ops.cnt() == 0);
  tx->m_trans = trans;
  tx_open.push_back(tx);
  return tx;
}

NdbImportImpl::Tx*
NdbImportImpl::DbWorker::start_trans(uint nodeid, uint instanceid)
{
  log2("start_trans");
  TxList& tx_free = m_tx_free;
  TxList& tx_open = m_tx_open;
  require(m_ndb != 0);
  NdbTransaction* trans = m_ndb->startTransaction(nodeid, instanceid);
  if (trans == 0)
  {
    return 0;
  }
  Tx* tx = tx_free.pop_front();
  if (tx == 0)
  {
    tx = new Tx(this);
  }
  require(tx != 0);
  require(tx->m_trans == 0);
  require(tx->m_ops.cnt() == 0);
  tx->m_trans = trans;
  tx_open.push_back(tx);
  return tx;
}

void
NdbImportImpl::DbWorker::close_trans(Tx* tx)
{
  log2("close_trans");
  TxList& tx_free = m_tx_free;
  TxList& tx_open = m_tx_open;
  require(tx->m_trans != 0);
  m_ndb->closeTransaction(tx->m_trans);
  tx->m_trans = 0;
  while (tx->m_ops.cnt() != 0)
  {
    Op* op = tx->m_ops.pop_front();
    require(op != 0);
    require(op->m_row != 0);
    m_rows_free.push_back(op->m_row);
    op->m_row = 0;
    op->m_rowop = 0;
    op->m_opcnt = 0;
    op->m_opsize = 0;
    free_op(op);
  }
  tx_open.remove(tx);
  tx_free.push_back(tx);
}

// relay op team

NdbImportImpl::RelayOpTeam::RelayOpTeam(Job& job,
                                        uint workercnt) :
  DbTeam(job, "relay-op", workercnt)
{
}

NdbImportImpl::RelayOpTeam::~RelayOpTeam()
{
}

NdbImportImpl::Worker*
NdbImportImpl::RelayOpTeam::create_worker(uint n)
{
  RelayOpWorker* w = new RelayOpWorker(*this, n);
  return w;
}

void
NdbImportImpl::RelayOpTeam::do_init()
{
  log1("do_init");
}

void
NdbImportImpl::RelayOpTeam::do_end()
{
  log1("do_end");
  RowList& rows_in = *m_job.m_rows_relay;
  rows_in.lock();
  require(!rows_in.m_foe);
  rows_in.m_foe = true;
  rows_in.unlock();
  for (uint i = 0; i < m_impl.c_nodes.m_nodecnt; i++)
  {
    RowList& rows_out = *m_job.m_rows_exec[i];
    rows_out.lock();
    require(!rows_out.m_eof);
    rows_out.m_eof = true;
    rows_out.unlock();
  }
}

NdbImportImpl::RelayOpWorker::RelayOpWorker(Team& team, uint n) :
  DbWorker(team, n)
{
  m_relaystate = RelayState::State_null;
  m_xfrmalloc = 0;
  m_xfrmbuf = 0;
  m_xfrmbuflen = 0;
  for (uint i = 0; i < g_max_ndb_nodes; i++)
    m_rows_exec[i] = 0;
}

NdbImportImpl::RelayOpWorker::~RelayOpWorker()
{
  delete [] m_xfrmalloc;
  for (uint i = 0; i < g_max_ndb_nodes; i++)
    delete m_rows_exec[i];
}

void
NdbImportImpl::RelayOpWorker::do_init()
{
  log1("do_init");
  create_ndb(1);
  uint len = (MAX_KEY_SIZE_IN_WORDS << 2) + sizeof(uint64);
  m_xfrmalloc = new uchar [len];
  // copied from Ndb::computeHash()
  UintPtr org = UintPtr(m_xfrmalloc);
  UintPtr use = (org + 7) & ~(UintPtr)7;
  m_xfrmbuf = (uchar*)use;
  m_xfrmbuflen = len - uint(use - org);
  uint nodecnt = m_impl.c_nodes.m_nodecnt;
  require(nodecnt != 0);
  for (uint i = 0; i < nodecnt; i++)
  {
    m_rows_exec[i] = new RowList;
  }
}

void
NdbImportImpl::RelayOpWorker::do_run()
{
  log2("do_run");
  switch (m_relaystate) {
  case RelayState::State_null:
    m_relaystate = RelayState::State_receive;
    break;
  case RelayState::State_receive:
    state_receive();
    break;
  case RelayState::State_define:
    state_define();
    break;
  case RelayState::State_send:
    state_send();
    break;
  case RelayState::State_eof:
    m_state = WorkerState::State_stop;
    break;
  }
}

void
NdbImportImpl::RelayOpWorker::state_receive()
{
  log2("state_receive");
  const Opt& opt = m_util.c_opt;
  RowList& rows_in = *m_team.m_job.m_rows_relay;
  rows_in.lock();
  RowCtl ctl(opt.m_rowswait);
  m_rows.push_back_from(rows_in, ctl);
  bool eof = rows_in.m_eof;
  rows_in.unlock();
  if (m_rows.empty())
  {
    if (!eof)
    {
      m_idle = true;
      return;
    }
    m_relaystate = RelayState::State_eof;
    return;
  }
  m_relaystate = RelayState::State_define;
}

void
NdbImportImpl::RelayOpWorker::state_define()
{
  log2("state_define");
  const Opt& opt = m_util.c_opt;
  Row* row;
  while ((row = m_rows.pop_front()) != 0)
  {
    const Nodes& c = m_impl.c_nodes;
    const Table& table = m_util.get_table(row->m_tabid);
    const bool no_hint = opt.m_no_hint;
    uint nodeid = 0;
    if (no_hint)
    {
      uint i = get_rand() % c.m_nodecnt;
      nodeid = c.m_nodes[i].m_nodeid;
    }
    else
    {
      Uint32 hash;
      m_ndb->computeHash(&hash, table.m_keyrec, (const char*)row->m_data,
                         m_xfrmbuf, m_xfrmbuflen);
      uint fragid = (uint)table.m_tab->getPartitionId(hash);
      nodeid = table.get_nodeid(fragid);
    }
    require(nodeid < g_max_nodes);
    uint nodeindex = c.m_index[nodeid];
    require(nodeindex < c.m_nodecnt);
    // move locally to per-node rows
    RowList& rows_exec = *m_rows_exec[nodeindex];
    rows_exec.push_back(row);
  }
  m_relaystate = RelayState::State_send;
}

void
NdbImportImpl::RelayOpWorker::state_send()
{
  log2("state_send");
  const Opt& opt = m_util.c_opt;
  uint nodecnt = m_impl.c_nodes.m_nodecnt;
  uint left = 0;
  for (uint i = 0; i < nodecnt; i++)
  {
    RowList& rows_exec = *m_rows_exec[i];
    RowList& rows_out = *m_team.m_job.m_rows_exec[i];
    if (rows_exec.cnt() != 0)
    {
      rows_out.lock();
      RowCtl ctl(opt.m_rowswait);
      rows_exec.pop_front_to(rows_out, ctl);
      rows_out.unlock();
      left += rows_exec.cnt();
    }
  }
  if (!left)
  {
    m_relaystate = RelayState::State_receive;
    return;
  }
  m_idle = true;
}

void
NdbImportImpl::RelayOpWorker::do_end()
{
  log1("do_end");
  if (!has_error())
  {
    require(m_tx_open.cnt() == 0);
  }
  else if (m_tx_open.cnt() != 0)
  {
    require(m_tx_open.cnt() == 1);
    Tx* tx = m_tx_open.front();
    close_trans(tx);
  }
}

// print

const char*
NdbImportImpl::g_str_state(RelayState::State state)
{
  const char* str = 0;
  switch (state) {
  case RelayState::State_null:
    str = "null";
    break;
  case RelayState::State_receive:
    str = "receive";
    break;
  case RelayState::State_define:
    str = "define";
    break;
  case RelayState::State_send:
    str = "send";
    break;
  case RelayState::State_eof:
    str = "eof";
    break;
  }
  require(str != 0);
  return str;
}

void
NdbImportImpl::RelayOpWorker::str_state(char* str) const
{
  sprintf(str, "%s/%s",
          g_str_state(m_state), g_str_state(m_relaystate));
}

// exec op team

NdbImportImpl::ExecOpTeam::ExecOpTeam(Job& job,
                                      uint workercnt) :
  DbTeam(job, "exec-op", workercnt)
{
  uint nodecnt = m_impl.c_nodes.m_nodecnt;
  require(nodecnt != 0);
  require(workercnt % nodecnt == 0);
}

NdbImportImpl::ExecOpTeam::~ExecOpTeam()
{
}

NdbImportImpl::Worker*
NdbImportImpl::ExecOpTeam::create_worker(uint n)
{
  ExecOpWorker* w = 0;
  const Opt& opt = m_util.c_opt;
  if (opt.m_no_asynch)
    w = new ExecOpWorkerSynch(*this, n);
  else
    w = new ExecOpWorkerAsynch(*this, n);
  return w;
}

void
NdbImportImpl::ExecOpTeam::do_init()
{
  log1("do_init");
}

void
NdbImportImpl::ExecOpTeam::do_end()
{
  log1("do_end");
  for (uint i = 0; i < m_impl.c_nodes.m_nodecnt; i++)
  {
    RowList& rows_in = *m_job.m_rows_exec[i];
    rows_in.lock();
    require(!rows_in.m_foe);
    rows_in.m_foe = true;
    rows_in.unlock();
  }
}

NdbImportImpl::ExecOpWorker::ExecOpWorker(Team& team, uint n) :
  DbWorker(team, n)
{
  m_execstate = ExecState::State_null;
  m_nodeindex = Inval_uint;
  m_nodeid = Inval_uint;
  m_eof = false;
  m_opcnt = 0;
  m_opsize = 0;
}

NdbImportImpl::ExecOpWorker::~ExecOpWorker()
{
}

void
NdbImportImpl::ExecOpWorker::do_init()
{
  log1("do_init");
  const Nodes& c = m_impl.c_nodes;
  require(c.m_nodecnt > 0);
  m_nodeindex = m_workerno % c.m_nodecnt;
  m_nodeid = c.m_nodes[m_nodeindex].m_nodeid;
  /*
   * Option opbatch limits number of received rows and
   * therefore number of async transactions.  Each row
   * creates one transaction (this is unlikely to change).
   */
  const Opt& opt = m_util.c_opt;
  require(opt.m_opbatch != 0);
  m_rows.m_rowbatch = opt.m_opbatch;
  m_rows.m_rowbytes = opt.m_opbytes != 0 ? opt.m_opbytes : UINT_MAX;
  create_ndb(opt.m_opbatch);
}

void
NdbImportImpl::ExecOpWorker::do_run()
{
  log2("do_run");
  switch (m_execstate) {
  case ExecState::State_null:
    m_execstate = ExecState::State_receive;
    break;
  case ExecState::State_receive:
    state_receive();
    break;
  case ExecState::State_define:
    state_define();
    break;
  case ExecState::State_prepare:
    state_prepare();
    break;
  case ExecState::State_send:
    state_send();
    break;
  case ExecState::State_poll:
    state_poll();
    break;
  case ExecState::State_eof:
    m_state = WorkerState::State_stop;
    break;
  default:
    require(false);
    break;
  }
}

/*
 * Receive rows until a batch is full or eof is seen.  At the end
 * convert the rows into ops.  The ops are assigned to transactions
 * in state_define().
 */
void
NdbImportImpl::ExecOpWorker::state_receive()
{
  log2("state_receive");
  const Opt& opt = m_util.c_opt;
  RowList& rows_in = *m_team.m_job.m_rows_exec[m_nodeindex];
  rows_in.lock();
  RowCtl ctl(opt.m_rowswait);
  m_rows.push_back_from(rows_in, ctl);
  bool eof = rows_in.m_eof;
  rows_in.unlock();
  do
  {
    if (m_rows.full())
    {
      log2("got full batch");
      break;
    }
    if (eof)
    {
      if (m_rows.cnt() != 0)
      {
        log2("got partial last batch");
        break;
      }
      log2("no more rows");
      m_execstate = ExecState::State_eof;
      return;
    }
    log2("wait for more rows");
    m_idle = true;
    return;
  } while (0);
  // assign op to each row and move the row under the op
  require(m_ops.cnt() == 0);
  Row* row;
  while ((row = m_rows.pop_front()) != 0)
  {
    Op* op = alloc_op();
    op->m_row = row;
    m_ops.push_back(op);
  }
  m_execstate = ExecState::State_define;
}

void
NdbImportImpl::ExecOpWorker::reject_row(Row* row, const Error& error)
{
  const Opt& opt = m_util.c_opt;
  RowList& rows_reject = *m_team.m_job.m_rows_reject;
  rows_reject.lock();
  // write reject row first
  const Table& reject_table = m_util.c_reject_table;
  Row* rejectrow = m_util.alloc_row(reject_table);
  rejectrow->m_rowid = row->m_rowid;
  rejectrow->m_linenr = row->m_linenr;
  rejectrow->m_startpos = row->m_startpos;
  rejectrow->m_endpos = row->m_endpos;
  const char* reject = "<row data not yet available>";
  uint32 rejectlen = strlen(reject);
  m_util.set_reject_row(rejectrow, m_team.m_job.m_runno, error, reject, rejectlen);
  require(rows_reject.push_back(rejectrow));
  // error if rejects exceeded
  if (rows_reject.totcnt() > opt.m_rejects)
  {
    // set team level error
    m_util.set_error_data(m_error, __LINE__, 0,
                          "reject limit %u exceeded", opt.m_rejects);
  }
  rows_reject.unlock();
}

// synch

NdbImportImpl::ExecOpWorkerSynch::ExecOpWorkerSynch(Team& team, uint n) :
  ExecOpWorker(team, n)
{
}

NdbImportImpl::ExecOpWorkerSynch::~ExecOpWorkerSynch()
{
}

void
NdbImportImpl::ExecOpWorkerSynch::do_end()
{
  log1("do_end/synch");
  if (!has_error())
  {
    require(m_tx_open.cnt() == 0);
  }
  else if (m_tx_open.cnt() != 0)
  {
    require(m_tx_open.cnt() == 1);
    Tx* tx = m_tx_open.front();
    close_trans(tx);
  }
}

void
NdbImportImpl::ExecOpWorkerSynch::state_define()
{
  log2("state_define/synch");
  TxList& tx_open = m_tx_open;
  // single trans
  require(tx_open.cnt() == 0);
  Tx* tx = start_trans();
  if (tx == 0)
  {
    const NdbError& ndberror = m_ndb->getNdbError();
    require(ndberror.code != 0);
    // synch does not handle temporary errors yet
    m_util.set_error_ndb(m_error, __LINE__, ndberror);
    return;
  }
  NdbTransaction* trans = tx->m_trans;
  require(trans != 0);
  while (m_ops.cnt() != 0)
  {
    Op* op = m_ops.pop_front();
    Row* row = op->m_row;
    require(row != 0);
    const Table& table = m_util.get_table(row->m_tabid);
    const NdbOperation* rowop = 0;
    const char* rowdata = (const char*)row->m_data;
    if ((rowop = trans->insertTuple(table.m_rec, rowdata)) == 0)
    {
      m_util.set_error_ndb(m_error, __LINE__, trans->getNdbError());
      break;
    }
    for (uint j = 0; j < table.m_blobids.size(); j++)
    {
      uint i = table.m_blobids[j];
      require(i < table.m_attrs.size());
      const Attr& attr = table.m_attrs[i];
      require(attr.m_isblob);
      NdbBlob* bh = 0;
      if ((bh = rowop->getBlobHandle(i)) == 0)
      {
        m_util.set_error_ndb(m_error, __LINE__, rowop->getNdbError());
        break;
      }
      Blob* blob = row->m_blobs[attr.m_blobno];
      if (!attr.get_null(row))
      {
        if (bh->setValue(blob->m_data, blob->m_blobsize) == -1)
        {
          m_util.set_error_ndb(m_error, __LINE__, bh->getNdbError());
          break;
        }
      }
      else
      {
        if (bh->setValue((void*)0, 0) == -1)
        {
          m_util.set_error_ndb(m_error, __LINE__, bh->getNdbError());
          break;
        }
      }
    }
    op->m_rowop = rowop;
    tx->m_ops.push_back(op);
  }
  m_execstate = ExecState::State_prepare;
}

void
NdbImportImpl::ExecOpWorkerSynch::state_prepare()
{
  log2("state_prepare/synch");
  // nothing to do
  m_execstate = ExecState::State_send;
}

void
NdbImportImpl::ExecOpWorkerSynch::state_send()
{
  log2("state_send/synch");
  TxList& tx_open = m_tx_open;
  require(tx_open.cnt() == 1);
  Tx* tx = tx_open.front();
  require(tx != 0);
  NdbTransaction* trans = tx->m_trans;
  require(trans != 0);
  const NdbTransaction::ExecType et = NdbTransaction::Commit;
  if (trans->execute(et) == -1)
  {
    m_util.set_error_ndb(m_error, __LINE__, trans->getNdbError());
  }
  close_trans(tx);
  m_execstate = ExecState::State_poll;
}

void
NdbImportImpl::ExecOpWorkerSynch::state_poll()
{
  log2("state_poll/synch");
  // nothing to poll
  m_opcnt = 0;
  m_opsize = 0;
  m_util.free_rows(m_rows_free);
  m_execstate = ExecState::State_receive;
}

// asynch

NdbImportImpl::ExecOpWorkerAsynch::ExecOpWorkerAsynch(Team& team, uint n) :
  ExecOpWorker(team, n)
{
}

NdbImportImpl::ExecOpWorkerAsynch::~ExecOpWorkerAsynch()
{
}

void
NdbImportImpl::ExecOpWorkerAsynch::do_end()
{
  log1("do_end/asynch");
  // currently only way for "graceful" stop is fatal error
  if (!has_error())
  {
    require(m_execstate == ExecState::State_eof);
    require(m_tx_open.cnt() == 0);
  }
  else if (m_execstate == ExecState::State_prepare)
  {
    // error in State_define, simply close the txs
    while (m_tx_open.cnt() != 0)
    {
      Tx* tx = m_tx_open.front();
      close_trans(tx);
    }
  }
  else
  {
    // currently trans cannot be closed after executeAsynchPrepare
    if (m_execstate == ExecState::State_send)
    {
      log1("send remaining transes");
      state_send();
    }
    while (m_execstate == ExecState::State_poll)
    {
      log1("poll remaining transes");
      state_poll();
    }
  }
}

static void
asynch_callback(int result, NdbTransaction* trans, void* tx_void)
{
  NdbImportImpl::Tx* tx = (NdbImportImpl::Tx*)tx_void;
  require(trans == tx->m_trans);
  NdbImportImpl::ExecOpWorkerAsynch* w =
    (NdbImportImpl::ExecOpWorkerAsynch*)(tx->m_worker);
  w->asynch_callback(tx);
}

void
NdbImportImpl::ExecOpWorkerAsynch::asynch_callback(Tx* tx)
{
  NdbTransaction* trans = tx->m_trans;
  const NdbError& ndberror = trans->getNdbError();
  if (ndberror.status == NdbError::Success)
  {
    Op* op = tx->m_ops.front();
    while (op != 0)
    {
      Row* row = op->m_row;
      m_rowmap_out.add(row, false);
      op = op->next();
    }
  }
  else if (ndberror.status == NdbError::TemporaryError)
  {
    m_errormap.add_one(ndberror.code);
    /*
     * Move rows back to input for processing by new txs.
     * Check for too many temp errors later in state_poll().
     */
    RowList& rows_in = *m_team.m_job.m_rows_exec[m_nodeindex];
    rows_in.lock();
    while (tx->m_ops.cnt() != 0)
    {
      Op* op = tx->m_ops.pop_front();
      Row* row = op->m_row;
      require(row != 0);
      log1("push back to input: rowid " << row->m_rowid);
      rows_in.push_back_force(row);
    }
    rows_in.unlock();
  }
  else if (ndberror.status == NdbError::PermanentError &&
           ndberror.classification == NdbError::ConstraintViolation)
  {
    Error error;        // local error
    m_util.set_error_ndb(error, __LINE__, ndberror,
                         "permanent error");
    while (tx->m_ops.cnt() != 0)
    {
      Op* op = tx->m_ops.pop_front();
      require(op != 0);
      require(op->m_row != 0);
      reject_row(op->m_row, error);
    }
  }
  else
  {
    m_util.set_error_ndb(m_error, __LINE__, ndberror);
  }
  close_trans(tx);
}

void
NdbImportImpl::ExecOpWorkerAsynch::state_define()
{
  log2("state_define/asynch");
  const Opt& opt = m_util.c_opt;
  TxList& tx_open = m_tx_open;
  // no transes yet
  require(tx_open.cnt() == 0);
  m_errormap.clear();
  /*
   * Temporary errors can occur at auto-incr and start trans.  We
   * don't want to get stuck here on "permanent" temporary errors.
   * So we limit them by opt.m_tmperrors (counted per op).
   */
  while (m_ops.cnt() != 0)
  {
    Op* op = m_ops.pop_front();
    Row* row = op->m_row;
    require(row != 0);
    const Table& table = m_util.get_table(row->m_tabid);
    if (table.m_has_hidden_pk)
    {
      const Attrs& attrs = table.m_attrs;
      const uint attrcnt = attrs.size();
      const Attr& attr = attrs[attrcnt - 1];
      require(attr.m_type == NdbDictionary::Column::Bigunsigned);
      uint64 val;
      if (m_ndb->getAutoIncrementValue(table.m_tab, val,
                                       opt.m_ai_prefetch_sz,
                                       opt.m_ai_increment,
                                       opt.m_ai_offset) == -1)
      {
        const NdbError& ndberror = m_ndb->getNdbError();
        require(ndberror.code != 0);
        if (ndberror.status == NdbError::TemporaryError)
        {
          m_errormap.add_one(ndberror.code);
          uint temperrors = m_errormap.get_sum();
          if (temperrors <= opt.m_temperrors)
          {
            log1("get autoincr try " << temperrors << ": " << ndberror);
            m_ops.push_front(op);
            NdbSleep_MilliSleep(opt.m_tempdelay);
            continue;
          }
          m_util.set_error_gen(m_error, __LINE__,
                               "number of transaction tries with"
                               " temporary errors is %u (limit %u)",
                               temperrors, opt.m_temperrors);
          break;
        }
        else
        {
          m_util.set_error_ndb(m_error, __LINE__, ndberror,
                               "table %s: get autoincrement failed",
                               table.m_tab->getName());
          break;
        }
      }
      attr.set_value(row, &val, 8);
    }
    const bool no_hint = opt.m_no_hint;
    Tx* tx = 0;
    if (no_hint)
      tx = start_trans();
    else
      tx = start_trans(m_nodeid, 0);
    if (tx == 0)
    {
      const NdbError& ndberror = m_ndb->getNdbError();
      require(ndberror.code != 0);
      if (ndberror.status == NdbError::TemporaryError)
      {
        m_errormap.add_one(ndberror.code);
        uint temperrors = m_errormap.get_sum();
        if (temperrors <= opt.m_temperrors)
        {
          log1("start trans try " << temperrors << ": " << ndberror);
          m_ops.push_front(op);
          NdbSleep_MilliSleep(opt.m_tempdelay);
          continue;
        }
        m_util.set_error_gen(m_error, __LINE__,
                             "number of transaction tries with"
                             " temporary errors is %u (limit %u)",
                             temperrors, opt.m_temperrors);
        break;
      }
      else
      {
        m_util.set_error_ndb(m_error, __LINE__, ndberror,
                             "table %s: start transaction failed",
                             table.m_tab->getName());
        break;
      }
    }
    NdbTransaction* trans = tx->m_trans;
    require(trans != 0);
    const NdbOperation* rowop = 0;
    const char* rowdata = (const char*)row->m_data;
    if ((rowop = trans->insertTuple(table.m_rec, rowdata)) == 0)
    {
      m_util.set_error_ndb(m_error, __LINE__, trans->getNdbError());
      break;
    }
    for (uint j = 0; j < table.m_blobids.size(); j++)
    {
      uint i = table.m_blobids[j];
      require(i < table.m_attrs.size());
      const Attr& attr = table.m_attrs[i];
      require(attr.m_isblob);
      NdbBlob* bh = 0;
      if ((bh = rowop->getBlobHandle(i)) == 0)
      {
        m_util.set_error_ndb(m_error, __LINE__, rowop->getNdbError());
        break;
      }
      Blob* blob = row->m_blobs[attr.m_blobno];
      if (!attr.get_null(row))
      {
        if (bh->setValue(blob->m_data, blob->m_blobsize) == -1)
        {
          m_util.set_error_ndb(m_error, __LINE__, bh->getNdbError());
          break;
        }
      }
      else
      {
        if (bh->setValue((void*)0, 0) == -1)
        {
          m_util.set_error_ndb(m_error, __LINE__, bh->getNdbError());
          break;
        }
      }
      bool batch = false;
      if (bh->preExecute(NdbTransaction::Commit, batch) == -1)
      {
        m_util.set_error_ndb(m_error, __LINE__, bh->getNdbError());
        break;
      }
    }
    op->m_rowop = rowop;
    tx->m_ops.push_back(op);
  }
  m_execstate = ExecState::State_prepare;
}

void
NdbImportImpl::ExecOpWorkerAsynch::state_prepare()
{
  Tx* tx = m_tx_open.front();
  while (tx != 0)
  {
    const NdbTransaction::ExecType et = NdbTransaction::Commit;
    NdbTransaction* trans = tx->m_trans;
    require(trans != 0);
    trans->executeAsynchPrepare(et, &::asynch_callback, (void*)tx);
    tx = tx->next();
  }
  m_execstate = ExecState::State_send;
}

void
NdbImportImpl::ExecOpWorkerAsynch::state_send()
{
  log2("state_send/asynch");
  require(m_tx_open.cnt() != 0);
  int forceSend = 0;
  m_ndb->sendPreparedTransactions(forceSend);
  m_execstate = ExecState::State_poll;
}

void
NdbImportImpl::ExecOpWorkerAsynch::state_poll()
{
  log2("state_poll/asynch");
  const Opt& opt = m_util.c_opt;
  int timeout = opt.m_polltimeout;
  require(m_tx_open.cnt() != 0);
  m_ndb->pollNdb(timeout, m_tx_open.cnt());
  if (m_tx_open.cnt() != 0)
  {
    log2("poll not ready");
    return;
  }
  log2("poll ready");
  m_opcnt = 0;
  m_opsize = 0;
  if (m_errormap.size() != 0)
  {
    Job& job = m_team.m_job;
    job.lock();
    job.m_errormap.add_one(m_errormap);
    uint temperrors = job.m_errormap.get_sum();
    job.unlock();
    if (temperrors <= opt.m_temperrors)
    {
      log1("temp errors: sleep " << opt.m_tempdelay << "ms");
      NdbSleep_MilliSleep(opt.m_tempdelay);
    }
    else
    {
      if (!m_util.has_error(m_error))
        m_util.set_error_gen(m_error, __LINE__,
                             "number of db execution batches with"
                             " temporary errors is %u (limit %u)",
                             temperrors, opt.m_temperrors);
    }
  }
  log1("rowmap " << m_rowmap_out.size());
  m_stat_rowmap->add(m_rowmap_out.size());
  m_util.free_rows(m_rows_free);
  m_execstate = ExecState::State_receive;
}

// print

const char*
NdbImportImpl::g_str_state(ExecState::State state)
{
  const char* str = 0;
  switch (state) {
  case ExecState::State_null:
    str = "null";
    break;
  case ExecState::State_receive:
    str = "receive";
    break;
  case ExecState::State_define:
    str = "define";
    break;
  case ExecState::State_prepare:
    str = "prepare";
    break;
  case ExecState::State_send:
    str = "send";
    break;
  case ExecState::State_poll:
    str = "wait";
    break;
  case ExecState::State_eof:
    str = "eof";
    break;
  }
  require(str != 0);
  return str;
}

void
NdbImportImpl::ExecOpWorker::str_state(char* str) const
{
  sprintf(str, "%s/%s tx:free=%u,open=%u",
               g_str_state(m_state), g_str_state(m_execstate),
               m_tx_free.cnt(), m_tx_open.cnt());
}

// diag team

NdbImportImpl::DiagTeam::DiagTeam(Job& job,
                                  uint workercnt) :
  Team(job, "diag", workercnt),
  // diag file errors are global
  m_result_file(m_util, m_error),
  m_reject_file(m_util, m_error),
  m_rowmap_file(m_util, m_error),
  m_stopt_file(m_util, m_error),
  m_stats_file(m_util, m_error)
{
  m_is_diag = true;
}

NdbImportImpl::DiagTeam::~DiagTeam()
{
}

NdbImportImpl::Worker*
NdbImportImpl::DiagTeam::create_worker(uint n)
{
  DiagWorker* w = new DiagWorker(*this, n);
  return w;
}

void
NdbImportImpl::DiagTeam::do_init()
{
  log1("do_init");
  const Opt& opt = m_util.c_opt;
  if (opt.m_resume)
  {
    read_old_diags();
    if (has_error())
      return;
  }
  open_new_diags();
}

void
NdbImportImpl::DiagTeam::read_old_diags(const char* name,
                                        const char* path,
                                        const Table& table,
                                        RowList& rows_out)
{
  log1("read_old_diags: " << name << " path=" << path);
  OptGuard optGuard(m_util);
  Opt& opt = m_util.c_opt;
  opt.m_ignore_lines = 1;
  // use default MySQL spec for diags (set by OptCsv ctor)
  OptCsv optcsv;
  CsvSpec csvspec;
  if (m_impl.m_csv.set_spec(csvspec, optcsv, OptCsv::ModeInput) == -1)
  {
    m_util.copy_error(m_error, m_util.c_error);
    require(has_error());
    return;
  }
  File file(m_util, m_error);
  file.set_path(path);
  if (file.do_open(File::Read_flags) == -1)
  {
    require(has_error());
    m_job.m_fatal = true;
    return;
  }
  // csv input requires at least 2 instances
  Buf* buf[2];
  CsvInput* csvinput[2];
  RowList rows_reject;
  for (uint i = 0; i < 2; i++)
  {
    uint pagesize = opt.m_pagesize;
    uint pagecnt = opt.m_pagecnt;
    buf[i] = new Buf(true);
    buf[i]->alloc(pagesize, 2 * pagecnt);
    RowMap rowmap_in(m_util);   // dummy
    csvinput[i] = new CsvInput(m_impl.m_csv,
                               Name(name, i),
                               csvspec,
                               table,
                               *buf[i],
                               rows_out,
                               rows_reject,
                               rowmap_in,
                               m_job.m_stats);
    csvinput[i]->do_init();
  }
  {
    uint i = 0; // current index
    uint n = 0; // number of buffer switches
    while (1)
    {
      uint j = 1 - i;
      CsvInput& csvinput1 = *csvinput[i];
      Buf& b1 = *buf[i];
      Buf& b2 = *buf[j];
      b1.reset();
      if (file.do_read(b1) == -1)
      {
        require(has_error());
        break;
      }
      // if not first read, move tail from previous
      if (n != 0)
      {
        require(b2.movetail(b1) == 0);
      }
      csvinput1.do_parse();
      if (csvinput1.has_error())
      {
        m_util.copy_error(m_error, csvinput1.m_error);
        require(has_error());
        break;
      }
      csvinput1.do_eval();
      if (csvinput1.has_error())
      {
        m_util.copy_error(m_error, csvinput1.m_error);
        require(has_error());
        break;
      }
      uint curr = 0;
      uint left = 0;
      csvinput1.do_send(curr, left);
      require(!csvinput1.has_error());
      require(left == 0);
      if (b1.m_eof)
        break;
      i = j;
      n++;
    }
    log1("read_old_diags: " << name << " count=" << rows_out.cnt());
  }
  // XXX diag errors not yet handled
  require(rows_reject.cnt() == 0);
}

void
NdbImportImpl::DiagTeam::read_old_diags()
{
  log1("read_old_diags");
  const Opt& opt = m_util.c_opt;
  Job& job = m_job;
  // result
  {
    const char* path = opt.m_result_file;
    const Table& table = m_util.c_result_table;
    RowList rows;
    read_old_diags("old-result", path, table, rows);
    if (has_error())
      return;
    uint32 runno = Inval_uint32;
    Row* row = 0;
    while ((row = rows.pop_front()) != 0)
    {
      // runno
      {
        const Attr& attr = table.get_attr("runno");
        uint32 x;
        attr.get_value(row, x);
        if (runno == Inval_uint32 || runno < x)
          runno = x;
      }
      m_util.free_row(row);
    }
    if (runno == Inval_uint32)
    {
      m_util.set_error_gen(m_error, __LINE__,
                           "%s: no valid records found", path);
      return;
    }
    job.m_runno = runno + 1;
  }
  // rowmap
  {
    const char* path = opt.m_rowmap_file;
    const Table& table = m_util.c_rowmap_table;
    RowList rows;
    read_old_diags("old-rowmap", path, table, rows);
    if (has_error())
      return;
    RowMap& rowmap_in = job.m_rowmap_in;
    require(rowmap_in.empty());
    Row* row = 0;
    while ((row = rows.pop_front()) != 0)
    {
      Range range;
      // runno
      {
        const Attr& attr = table.get_attr("runno");
        uint32 runno;
        attr.get_value(row, runno);
        if (runno != job.m_runno - 1)
        {
          m_util.free_row(row);
          continue;
        }
      }
      // start
      {
        const Attr& attr = table.get_attr("start");
        attr.get_value(row, range.m_start);
      }
      // end
      {
        const Attr& attr = table.get_attr("end");
        attr.get_value(row, range.m_end);
      }
      // startpos
      {
        const Attr& attr = table.get_attr("startpos");
        attr.get_value(row, range.m_startpos);
      }
      // endpos
      {
        const Attr& attr = table.get_attr("endpos");
        attr.get_value(row, range.m_endpos);
      }
      // reject
      {
        const Attr& attr = table.get_attr("reject");
        attr.get_value(row, range.m_reject);
      }
      m_util.free_row(row);
      // add to old rowmap
      rowmap_in.add(range);
    }
    if (rowmap_in.empty())
    {
      m_util.set_error_gen(m_error, __LINE__,
                           "%s: no records for run %u found",
                           path, job.m_runno - 1);
      return;
    }
    log1("old rowmap:" << rowmap_in);
  }
}

void
NdbImportImpl::DiagTeam::open_new_diags()
{
  log1("open_new_diags");
  const Opt& opt = m_util.c_opt;
  int openflags = 0;
  if (!opt.m_resume)
    openflags = File::Write_flags;
  else
    openflags = File::Append_flags;
  // use default MySQL spec for diags (set by OptCsv ctor)
  OptCsv optcsv;
  if (m_impl.m_csv.set_spec(m_csvspec, optcsv, OptCsv::ModeOutput) == -1)
  {
    m_util.copy_error(m_error, m_util.c_error);
    require(has_error());
    return;
  }
  // result
  m_result_file.set_path(opt.m_result_file);
  if (m_result_file.do_open(openflags) == -1)
  {
    require(has_error());
    m_job.m_fatal = true;
    return;
  }
  log1("file: opened: " << m_result_file.get_path());
  // reject
  m_reject_file.set_path(opt.m_reject_file);
  if (m_reject_file.do_open(openflags) == -1)
  {
    require(has_error());
    m_job.m_fatal = true;
    return;
  }
  log1("file: opened: " << m_reject_file.get_path());
  // rowmap
  m_rowmap_file.set_path(opt.m_rowmap_file);
  if (m_rowmap_file.do_open(openflags) == -1)
  {
    require(has_error());
    m_job.m_fatal = true;
    return;
  }
  log1("file: opened: " << m_rowmap_file.get_path());
  // stats opt
  if (opt.m_stats)
  {
    m_stopt_file.set_path(opt.m_stopt_file);
    if (m_stopt_file.do_open(openflags) == -1)
    {
      require(has_error());
      m_job.m_fatal = true;
      return;
    }
    log1("file: opened: " << m_stopt_file.get_path());
  }
  // stats
  if (opt.m_stats)
  {
    m_stats_file.set_path(opt.m_stats_file);
    if (m_stats_file.do_open(openflags) == -1)
    {
      require(has_error());
      m_job.m_fatal = true;
      return;
    }
    log1("file: opened: " << m_stats_file.get_path());
  }
}

void
NdbImportImpl::DiagTeam::do_end()
{
  log1("do_end");
  const Opt& opt = m_util.c_opt;
  if (m_result_file.do_close() == -1)
  {
    require(has_error());
    // continue
  }
  if (m_reject_file.do_close() == -1)
  {
    require(has_error());
    // continue
  }
  if (m_rowmap_file.do_close() == -1)
  {
    require(has_error());
    // continue
  }
  if (opt.m_stats)
  {
    if (m_stopt_file.do_close() == -1)
    {
      require(has_error());
      // continue
    }
  }
  if (opt.m_stats)
  {
    if (m_stats_file.do_close() == -1)
    {
      require(has_error());
      // continue
    }
  }
}

NdbImportImpl::DiagWorker::DiagWorker(Team& team, uint n) :
  Worker(team, n)
{
}

NdbImportImpl::DiagWorker::~DiagWorker()
{
}

void
NdbImportImpl::DiagWorker::do_init()
{
  log1("do_init");
  const Opt& opt = m_util.c_opt;
  const CsvSpec& csvspec = static_cast<DiagTeam&>(m_team).m_csvspec;
  if (has_error())
    return;
  // result
  {
    File& file = static_cast<DiagTeam&>(m_team).m_result_file;
    Buf& buf = m_result_buf;
    const Table& table = m_util.c_result_table;
    uint pagesize = opt.m_pagesize;
    uint pagecnt = opt.m_pagecnt;
    buf.alloc(pagesize, pagecnt);
    m_result_csv = new CsvOutput(m_impl.m_csv,
                                 csvspec,
                                 table,
                                 m_result_buf);
    m_result_csv->do_init();
    if (!opt.m_resume)
    {
      m_result_csv->add_header();
      if (file.do_write(buf) == -1)
      {
        require(has_error());
        m_team.m_job.m_fatal = true;
        return;
      }
    }
  }
  // reject
  {
    File& file = static_cast<DiagTeam&>(m_team).m_reject_file;
    Buf& buf = m_reject_buf;
    const Table& table = m_util.c_reject_table;
    uint pagesize = opt.m_pagesize;
    uint pagecnt = opt.m_pagecnt;
    buf.alloc(pagesize, pagecnt);
    m_reject_csv = new CsvOutput(m_impl.m_csv,
                                 csvspec,
                                 table,
                                 m_reject_buf);
    m_reject_csv->do_init();
    if (!opt.m_resume)
    {
      m_reject_csv->add_header();
      if (file.do_write(buf) == -1)
      {
        require(has_error());
        m_team.m_job.m_fatal = true;
        return;
      }
    }
  }
  // rowmap
  {
    File& file = static_cast<DiagTeam&>(m_team).m_rowmap_file;
    Buf& buf = m_rowmap_buf;
    const Table& table = m_util.c_rowmap_table;
    uint pagesize = opt.m_pagesize;
    uint pagecnt = opt.m_pagecnt;
    buf.alloc(pagesize, pagecnt);
    m_rowmap_csv = new CsvOutput(m_impl.m_csv,
                                 csvspec,
                                 table,
                                 m_rowmap_buf);
    m_rowmap_csv->do_init();
    if (!opt.m_resume)
    {
      m_rowmap_csv->add_header();
      if (file.do_write(buf) == -1)
      {
        require(has_error());
        m_team.m_job.m_fatal = true;
        return;
      }
    }
  }
  // stats opt
  if (opt.m_stats)
  {
    File& file = static_cast<DiagTeam&>(m_team).m_stopt_file;
    Buf& buf = m_stopt_buf;
    const Table& table = m_util.c_stopt_table;
    uint pagesize = opt.m_pagesize;
    uint pagecnt = opt.m_pagecnt;
    m_stopt_buf.alloc(pagesize, pagecnt);
    m_stopt_csv = new CsvOutput(m_impl.m_csv,
                                csvspec,
                                table,
                                m_stopt_buf);
    m_stopt_csv->do_init();
    if (!opt.m_resume)
    {
      m_stopt_csv->add_header();
      if (file.do_write(buf) == -1)
      {
        require(has_error());
        m_team.m_job.m_fatal = true;
        return;
      }
    }
  }
  // stats
  if (opt.m_stats)
  {
    File& file = static_cast<DiagTeam&>(m_team).m_stats_file;
    Buf& buf = m_stats_buf;
    const Table& table = m_util.c_stats_table;
    uint pagesize = opt.m_pagesize;
    uint pagecnt = opt.m_pagecnt;
    m_stats_buf.alloc(pagesize, pagecnt);
    m_stats_csv = new CsvOutput(m_impl.m_csv,
                                csvspec,
                                table,
                                m_stats_buf);
    m_stats_csv->do_init();
    if (!opt.m_resume)
    {
      m_stats_csv->add_header();
      if (file.do_write(buf) == -1)
      {
        require(has_error());
        m_team.m_job.m_fatal = true;
        return;
      }
    }
  }
}

void
NdbImportImpl::DiagWorker::do_run()
{
  log2("do_run");
  // reject
  write_reject();
  // stop by request
  if (m_dostop)
  {
    log1("stop by request");
    m_state = WorkerState::State_stop;
  }
}

void
NdbImportImpl::DiagWorker::do_end()
{
  const Opt& opt = m_util.c_opt;
  log1("do_end");
  // result
  write_result();
  // rowmap
  write_rowmap();
  // stats opt
  if (opt.m_stats)
  {
    write_stopt();
  }
  // stats
  if (opt.m_stats)
  {
    write_stats();
  }
}

void
NdbImportImpl::DiagWorker::write_result()
{
  log1("write_result");
  DiagTeam& team = static_cast<DiagTeam&>(m_team);
  const Job& job = team.m_job;
  File& file = team.m_result_file;
  Buf& buf = m_result_buf;
  buf.reset();
  const Table& table = m_util.c_result_table;
  // fatal global error, should not happen in job scope
  if (m_util.has_error())
  {
    Row* row = m_util.alloc_row(table);
    const char* name = "IMP";
    const char* desc = "";
    uint64 rows = 0;
    uint64 reject = 0;
    uint64 temperrors = 0;
    uint64 runtime = 0;
    uint64 utime = 0;
    const Error& error = m_util.c_error;
    m_util.set_result_row(row,
                          job.m_runno,
                          name,
                          desc,
                          rows,
                          reject,
                          temperrors,
                          runtime,
                          utime,
                          error);
    m_result_csv->add_line(row);
  }
  // job
  {
    Row* row = m_util.alloc_row(table);
    const Name name("job", job.m_jobno);
    const char* desc = "job";
    uint64 rows = job.m_stat_rows->m_max;
    uint64 reject = job.m_stat_reject->m_max;
    uint64 temperrors = job.m_errormap.get_sum();
    uint64 runtime = job.m_timer.elapsed_msec();
    uint64 utime = job.m_stat_utime->m_sum;
    const Error& error = job.m_error;
    m_util.set_result_row(row,
                          job.m_runno,
                          name,
                          desc,
                          rows,
                          reject,
                          temperrors,
                          runtime,
                          utime,
                          error);
    m_result_csv->add_line(row);
  }
  // teams
  for (uint teamno = 0; teamno < job.m_teamcnt; teamno++)
  {
    Row* row = m_util.alloc_row(table);
    const Team& team = *job.m_teams[teamno];
    const Name name("team", team.m_teamno);
    const Name desc(team.m_name);
    uint64 rows = 0;
    uint64 reject = 0;
    uint64 temperrors = 0;
    if (team.m_state != TeamState::State_stopped)
    {
      // not worth crashing
      team.m_timer.stop();
    }
    uint64 runtime = team.m_timer.elapsed_msec();
    uint64 utime = team.m_stat_utime->m_sum;
    const Error& error = team.m_error;
    m_util.set_result_row(row,
                          job.m_runno,
                          name,
                          desc,
                          rows,
                          reject,
                          temperrors,
                          runtime,
                          utime,
                          error);
    m_result_csv->add_line(row);
  }
  if (file.do_write(buf) == -1)
  {
    require(has_error());
    m_team.m_job.m_fatal = true;
  }
}

void
NdbImportImpl::DiagWorker::write_reject()
{
  log2("write_reject");
  DiagTeam& team = static_cast<DiagTeam&>(m_team);
  Job& job = team.m_job;
  File& file = team.m_reject_file;
  Buf& buf = m_reject_buf;
  RowList& rows_reject = *job.m_rows_reject;
  rows_reject.lock();
  while (1)
  {
    Row* row = rows_reject.pop_front();
    require(!rows_reject.m_eof);
    if (row == 0)
    {
      m_idle = true;
      break;
    }
    // Csv does not know runno so fix it here
    {
      const Table& table = m_util.c_reject_table;
      const Attrs& attrs = table.m_attrs;
      const void* p = attrs[0].get_value(row);
      uint32 x = *(uint32*)p;
      if (x == Inval_uint32)
      {
        attrs[0].set_value(row, &job.m_runno, sizeof(uint32));
      }
      else
      {
        require(x == job.m_runno);
      }
    }
    buf.reset();
    m_reject_csv->add_line(row);
    if (file.do_write(buf) == -1)
    {
      require(has_error());
      m_team.m_job.m_fatal = true;
      return;
    }
    // add to job level rowmap
    job.m_rowmap_out.lock();
    job.m_rowmap_out.add(row, true);
    job.m_rowmap_out.unlock();
  }
  rows_reject.unlock();
}

void
NdbImportImpl::DiagWorker::write_rowmap()
{
  log1("write_rowmap");
  DiagTeam& team = static_cast<DiagTeam&>(m_team);
  const Job& job = team.m_job;
  File& file = team.m_rowmap_file;
  Buf& buf = m_rowmap_buf;
  const Table& table = m_util.c_rowmap_table;
  const RowMap& rowmap = job.m_rowmap_out;
  const RangeList& ranges = rowmap.m_ranges;
  const Range* r = ranges.front();
  while (r != 0)
  {
    Row* row = m_util.alloc_row(table);
    const Range& range = *r;
    m_util.set_rowmap_row(row, job.m_runno, range);
    buf.reset();
    m_rowmap_csv->add_line(row);
    if (file.do_write(buf) == -1)
    {
      require(has_error());
      m_team.m_job.m_fatal = true;
      return;
    }
    r = r->next();
  }
}

void
NdbImportImpl::DiagWorker::write_stopt()
{
  static const Opt& opt = m_util.c_opt;
  DiagTeam& team = static_cast<DiagTeam&>(m_team);
  const Job& job = team.m_job;
  File& file = team.m_stopt_file;
  Buf& buf = m_stopt_buf;
  const Table& table = m_util.c_stopt_table;
  // write performance related option values
  const struct ov_st {
    const char* m_option;
    uint m_value;
  } ov_list[] = {
    { "connections", opt.m_connections },
    { "input_workers", opt.m_input_workers },
    { "output_workers", opt.m_output_workers },
    { "db_workers", opt.m_db_workers },
    { "no_hint", (uint)opt.m_no_hint },
    { "pagesize", opt.m_pagesize },
    { "pagecnt", opt.m_pagecnt },
    { "pagebuffer", opt.m_pagebuffer },
    { "rowbatch", opt.m_rowbatch },
    { "rowbytes", opt.m_rowbytes },
    { "opbatch", opt.m_opbatch },
    { "opbytes", opt.m_opbytes },
    { "rowswait", opt.m_rowswait },
    { "idlespin", opt.m_idlespin },
    { "idlesleep", opt.m_idlesleep },
    { "checkloop", opt.m_checkloop },
    { "alloc_chunk", opt.m_alloc_chunk }
  };
  const uint ov_size = sizeof(ov_list) / sizeof(ov_list[0]);
  for (uint i = 0; i < ov_size; i++)
  {
    const struct ov_st& ov = ov_list[i];
    Row* row = m_util.alloc_row(table);
    m_util.set_stopt_row(row, job.m_runno, ov.m_option, ov.m_value);
    buf.reset();
    m_stopt_csv->add_line(row);
    if (file.do_write(buf) == -1)
    {
      require(has_error());
      m_team.m_job.m_fatal = true;
      return;
    }
  }
}

void
NdbImportImpl::DiagWorker::write_stats()
{
  DiagTeam& team = static_cast<DiagTeam&>(m_team);
  const Job& job = team.m_job;
  File& file = team.m_stats_file;
  Buf& buf = m_stats_buf;
  const Table& table = m_util.c_stats_table;
  // write job and global (accumulating) stats
  const Stats* stats_list[2] = { &job.m_stats, &m_util.c_stats };
  for (uint k = 0; k <= 1; k++)
  {
    const Stats& stats = *stats_list[k];
    const bool global = (k == 1);
    for (uint id = 0; id < stats.m_stats.size(); id++)
    {
      const Stat* stat = stats.get(id);
      buf.reset();
      Row* row = m_util.alloc_row(table);
      m_util.set_stats_row(row, job.m_runno, *stat, global);
      m_stats_csv->add_line(row);
      if (file.do_write(buf) == -1)
      {
        require(has_error());
        m_team.m_job.m_fatal = true;
        return;
      }
    }
  }
}

// global

NdbImportImpl::Jobs::Jobs()
{
  m_jobno = 0;
}

NdbImportImpl::Jobs::~Jobs()
{
  // XXX delete jobs
}

NdbImportImpl::Job*
NdbImportImpl::create_job()
{
  Jobs& jobs = c_jobs;
  // internal and external number from 1
  Job* job = new Job(*this, ++jobs.m_jobno);
  jobs.m_jobs.insert(std::pair<uint, Job*>(job->m_jobno, job));
  job->do_create();
  return job;
}

NdbImportImpl::Job*
NdbImportImpl::find_job(uint jobno)
{
  Job* job = 0;
  const Jobs& jobs = c_jobs;
  std::map<uint, Job*>::const_iterator it;
  it = jobs.m_jobs.find(jobno);
  if (it != jobs.m_jobs.end())
  {
    job = it->second;
    require(job->m_jobno == jobno);
  }
  return job;
}

extern "C" { static void* start_job_c(void* data); }

static void*
start_job_c(void* data)
{
  NdbImportImpl::Job* job = (NdbImportImpl::Job*)data;
  require(job != 0);
  job->do_start();
  return 0;
}

void
NdbImportImpl::start_job(Job* job)
{
  NDB_THREAD_PRIO prio = NDB_THREAD_PRIO_MEAN;
  uint stack_size = 64*1024;
  job->m_thread = NdbThread_Create(
    start_job_c, (void**)job, stack_size, "job", prio);
  require(job->m_thread != 0);
}

void
NdbImportImpl::stop_job(Job* job)
{
  job->do_stop();
  log1("done");
}

void
NdbImportImpl::wait_job(Job* job)
{
  const Opt& opt = m_util.c_opt;
  while (job->m_state != JobState::State_done)
  {
    log2("wait for " << g_str_state(JobState::State_done));
    NdbSleep_MilliSleep(opt.m_checkloop);
  }
  log1("done");
}

void
NdbImportImpl::destroy_job(Job* job)
{
  Jobs& jobs = c_jobs;
  require(job != 0);
  require(find_job(job->m_jobno) == job);
  require(jobs.m_jobs.erase(job->m_jobno) == 1);
  delete job;
}
