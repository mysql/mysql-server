/*
   Copyright (c) 2009, 2022, Oracle and/or its affiliates.


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

#include "util/require.h"
#include <NDBT.hpp>
#include <NDBT_Test.hpp>
#include <portlib/NdbDir.hpp>
#include "ConfigFactory.hpp"
#include <NdbMgmd.hpp>
#include <NdbProcess.hpp>
#include <NDBT_Find.hpp>
#include <NDBT_Workingdir.hpp>
#include <NdbEnv.h>

static const char * exe_valgrind = 0;
static const char * arg_valgrind = 0;
static int no_node_config = 0;
static bool with_nodeid = true;

static bool file_exists(const char* path, Uint32 timeout = 1)
{
  g_info << "File '" << path << "' ";
  /**
   * ndb_mgmd does currently not fsync the directory
   *   after committing config-bin,
   *   which means that it can be on disk, wo/ being visible
   *   remedy this by retrying some
   */
  for (Uint32 i = 0; i < 10 * timeout; i++)
  {
    if (access(path, F_OK) == 0)
    {
      g_info << "exists" << endl;
      return true;
    }
    if (i == 0)
    {
      g_info << "does not exist, retrying...";
    }
    NdbSleep_MilliSleep(100);
  }
  g_info << "does not exist" << endl;
  return false;
}

// Util function that concatenate strings to form a path

static BaseString path(const char* first, ...)
{
  BaseString path;
  path.assign(first);

  const char* str;
  va_list args;
  va_start(args, first);
  while ((str = va_arg(args, const char*)) != NULL)
  {
    path.appfmt("%s%s", DIR_SEPARATOR, str);
  }
  va_end(args);
  return path;
}

class Mgmd
{
protected:
  NdbProcess* m_proc;
  int m_nodeid;
  BaseString m_name;
  BaseString m_exe;
  NdbMgmd m_mgmd_client;

  Mgmd(const Mgmd& other) = delete;
public:

  Mgmd(int nodeid) :
  m_proc(NULL),
  m_nodeid(nodeid)
  {
    m_name.assfmt("ndb_mgmd_%d", nodeid);

    NDBT_find_ndb_mgmd(m_exe);
  }

  Mgmd() :
  m_proc(NULL)
  {
    no_node_config = no_node_config + 1;
    m_name.assfmt("ndb_mgmd_autonode_%d", no_node_config);
    NDBT_find_ndb_mgmd(m_exe);
  }

  ~Mgmd()
  {
    if (m_proc)
    {
      //stop the proces
      stop();
    }

  }

  const char* name(void) const
  {
    return m_name.c_str();
  }

  const char* exe(void) const
  {
    return m_exe.c_str();
  }

  bool start(const char* working_dir, NdbProcess::Args& args)
  {
    g_info << "Starting " << name() << " ";
    for (unsigned i = 0; i < args.args().size(); i++)
      g_info << args.args()[i].c_str() << " ";
    g_info << endl;

    if (exe_valgrind == 0)
    {
      m_proc = NdbProcess::create(name(),
                                  exe(),
                                  working_dir,
                                  args);
    }
    else
    {
      NdbProcess::Args copy;
      if (arg_valgrind)
      {
        copy.add(arg_valgrind);
      }
      copy.add(exe());
      copy.add(args);
      m_proc = NdbProcess::create(name(),
                                  BaseString(exe_valgrind),
                                  working_dir,
                                  copy);
    }
    return (m_proc != NULL);
  }

  bool start_from_config_ini(const char* working_dir,
                             const char* first_extra_arg = NULL, ...)
  {
    NdbProcess::Args args;
    args.add("--no-defaults");
    args.add("--configdir=", working_dir);
    args.add("-f config.ini");
    if (with_nodeid)
    {
      args.add("--ndb-nodeid=", m_nodeid);
    }
    args.add("--nodaemon");
    args.add("--log-name=", name());
    args.add("--verbose");

    if (first_extra_arg)
    {
      // Append any extra args
      va_list extra_args;
      const char* str = first_extra_arg;
      va_start(extra_args, first_extra_arg);
      do
      {
        args.add(str);
      } while ((str = va_arg(extra_args, const char*)) != NULL);
      va_end(extra_args);
    }

    return start(working_dir, args);
  }

  bool start(const char* working_dir,
             const char* first_extra_arg = NULL, ...)
  {
    NdbProcess::Args args;
    args.add("--no-defaults");
    args.add("--configdir=", working_dir);
    args.add("--ndb-nodeid=", m_nodeid);
    args.add("--nodaemon");
    args.add("--log-name=", name());
    args.add("--verbose");

    if (first_extra_arg)
    {
      // Append any extra args
      va_list extra_args;
      const char* str = first_extra_arg;
      va_start(extra_args, first_extra_arg);
      do
      {
        args.add(str);
      } while ((str = va_arg(extra_args, const char*)) != NULL);
      va_end(extra_args);
    }

    return start(working_dir, args);
  }

  bool stop(void)
  {
    g_info << "Stopping " << name() << endl;

    // Diconnect and close our "builtin" client
    m_mgmd_client.close();

    if (m_proc == 0 || !m_proc->stop())
    {
      fprintf(stderr, "Failed to stop process %s\n", name());
      return false; // Can't kill with -9 -> fatal error
    }
    int ret;
    if (!m_proc->wait(ret, 300))
    {
      fprintf(stderr, "Failed to wait for process %s\n", name());
      return false; // Can't wait after kill with -9 -> fatal error
    }

    if (ret != 9)
    {
      // The normal case after killing the process with -9 is that wait
      // returns 9, but other return codes may also be returned for example
      // when the process has already terminated itself.
      // The important thing is that the process has terminated, just log return
      // code and continue releasing resources.
      fprintf(stderr, "Process %s stopped with ret: %u\n", name(), ret);
    }

    delete m_proc;
    m_proc = 0;

    return true;

  }

  bool wait(int& ret, int timeout = 300)
  {
    g_info << "Waiting for " << name() << endl;

    if (m_proc == 0 || !m_proc->wait(ret, timeout))
    {
      fprintf(stderr, "Failed to wait for process %s\n", name());
      return false;
    }
    delete m_proc;
    m_proc = 0;

    return true;

  }

  const BaseString connectstring(const Properties& config)
  {
    const char* hostname;
    require(get_section_string(config, m_name.c_str(),
                               "HostName", &hostname));

    Uint32 port;
    require(get_section_uint32(config, m_name.c_str(),
                               "PortNumber", &port));

    BaseString constr;
    constr.assfmt("%s:%d", hostname, port);
    return constr;
  }

  bool connect(const Properties& config,
               int num_retries = 60, int retry_delay_in_seconds = 1)
  {
    BaseString constr = connectstring(config);
    g_info << "Connecting to " << name() << " @ " << constr.c_str() << endl;

    return m_mgmd_client.connect(constr.c_str(),
                                 num_retries,
                                 retry_delay_in_seconds);
  }

  bool wait_confirmed_config(int timeout = 30)
  {
    if (!m_mgmd_client.is_connected())
    {
      g_err << "wait_confirmed_config: not connected!" << endl;
      return false;
    }

    int retries = 0;
    Config conf;
    while (!m_mgmd_client.get_config(conf))
    {
      retries++;

      if (retries == timeout * 10)
      {
        g_err << "wait_confirmed_config: Failed to get config within "
                << timeout << " seconds" << endl;
        return false;
      }

      g_err << "Failed to get config, sleeping" << endl;
      NdbSleep_MilliSleep(100);

    }
    g_info << "wait_confirmed_config: ok" << endl;
    return true;

  }

  NdbMgmHandle handle() { return m_mgmd_client.handle(); }

private:

  bool get_section_string(const Properties& config,
                          const char* section_name,
                          const char* key,
                          const char** value) const
  {
    const Properties* section;
    if (!config.get(section_name, &section))
      return false;

    if (!section->get(key, value))
      return false;
    return true;
  }

  bool get_section_uint32(const Properties& config,
                          const char* section_name,
                          const char* key,
                          Uint32* value) const
  {
    const Properties* section;
    if (!config.get(section_name, &section))
      return false;

    if (!section->get(key, value))
      return false;
    return true;
  }

};

class MgmdProcessList : public Vector<Mgmd*>
{
public:

  ~MgmdProcessList()
  {
    // Delete and thus stop the mgmd(s)
    for (unsigned i = 0; i < size(); i++)
    {
      Mgmd* mgmd = this->operator[](i);
      delete mgmd;
    }
    //  delete this->[i];
    clear();
  }
};

class Ndbd : public Mgmd
{
public:
  Ndbd(int nodeid) : Mgmd(nodeid)
  {
    m_name.assfmt("ndbd_%d", nodeid);
    NDBT_find_ndbd(m_exe);
  }

  bool start(const char *working_dir, BaseString connect_string)
  {
    NdbProcess::Args args;
    args.add("-c");
    args.add(connect_string.c_str());
    args.add("--ndb-nodeid=", m_nodeid);
    args.add("--nodaemon");
    return Mgmd::start(working_dir, args);
  }
};


#include <fstream>
#include <iostream>
#include <string>

bool Print_find_in_file(const char* path, Vector<BaseString> search_string)
{
  std::ifstream indata;
  indata.open(path);
  Vector<bool> found;
  for (unsigned int i = 0; i < search_string.size(); i++)
  {
    found.push_back(false);
  }

  if (indata.is_open())
  {
    std::string read_line;
    while (std::getline(indata, read_line))
    {
      for (unsigned int i = 0; i < search_string.size(); i++)
      {
        if (found[i] == false)
        {
          if (read_line.find(search_string[i].c_str(), 0) != std::string::npos) {
            {
              found[i] = true;
              break;
            }
          }
        }
      }
      printf("%s\n", read_line.c_str());
    }
  }
  else
  {
    return false;
  }
  indata.close();
  bool ret = true;
  for (unsigned int i = 0; i < search_string.size(); i++)
  {
    ret = ret && found[i];
  }
  return ret;
}

#define CHECK(x)                                            \
  if (!(x)) {                                               \
    fprintf(stderr, "CHECK(" #x ") failed at line: %d\n", \
            __LINE__);                                      \
     return NDBT_FAILED;                                    \
  }

int runTestBasic2Mgm(NDBT_Context* ctx, NDBT_Step* step)
{
  NDBT_Workingdir wd("test_mgmd"); // temporary working directory

  // Create config.ini
  Properties config = ConfigFactory::create(2);
  CHECK(ConfigFactory::write_config_ini(config,
                                        path(wd.path(),
                                             "config.ini",
                                             NULL).c_str()));
  // Start ndb_mgmd(s)
  MgmdProcessList mgmds;
  for (int i = 1; i <= 2; i++)
  {
    Mgmd* mgmd = new Mgmd(i);
    mgmds.push_back(mgmd);
    CHECK(mgmd->start_from_config_ini(wd.path()));
  }

  // Connect the ndb_mgmd(s)
  for (unsigned i = 0; i < mgmds.size(); i++)
    CHECK(mgmds[i]->connect(config));

  // wait for confirmed config
  for (unsigned i = 0; i < mgmds.size(); i++)
    CHECK(mgmds[i]->wait_confirmed_config());

  // Check binary config files created
  CHECK(file_exists(path(wd.path(),
                         "ndb_1_config.bin.1",
                         NULL).c_str()));
  CHECK(file_exists(path(wd.path(),
                         "ndb_2_config.bin.1",
                         NULL).c_str()));

  // Stop the ndb_mgmd(s)
  for (unsigned i = 0; i < mgmds.size(); i++)
    CHECK(mgmds[i]->stop());

  // Start up the mgmd(s) again from config.bin
  for (unsigned i = 0; i < mgmds.size(); i++)
    CHECK(mgmds[i]->start_from_config_ini(wd.path()));

  // Connect the ndb_mgmd(s)
  for (unsigned i = 0; i < mgmds.size(); i++)
    CHECK(mgmds[i]->connect(config));

  // check ndb_X_config.bin.1 still exists but not ndb_X_config.bin.2
  CHECK(file_exists(path(wd.path(),
                         "ndb_1_config.bin.1",
                         NULL).c_str()));
  CHECK(file_exists(path(wd.path(),
                         "ndb_2_config.bin.1",
                         NULL).c_str()));

  CHECK(!file_exists(path(wd.path(),
                          "ndb_1_config.bin.2",
                          NULL).c_str()));
  CHECK(!file_exists(path(wd.path(),
                          "ndb_2_config.bin.2",
                          NULL).c_str()));

  return NDBT_OK;

}

int runTestBug45495(NDBT_Context* ctx, NDBT_Step* step)
{
  NDBT_Workingdir wd("test_mgmd"); // temporary working directory

  g_err << "** Create config.ini" << endl;
  Properties config = ConfigFactory::create(2);
  CHECK(ConfigFactory::write_config_ini(config,
                                        path(wd.path(),
                                             "config.ini",
                                             NULL).c_str()));
  // Start ndb_mgmd(s)
  MgmdProcessList mgmds;
  for (int i = 1; i <= 2; i++)
  {
    Mgmd* mgmd = new Mgmd(i);
    mgmds.push_back(mgmd);
    CHECK(mgmd->start_from_config_ini(wd.path()));
  }

  // Connect the ndb_mgmd(s)
  for (unsigned i = 0; i < mgmds.size(); i++)
    CHECK(mgmds[i]->connect(config));

  // wait for confirmed config
  for (unsigned i = 0; i < mgmds.size(); i++)
    CHECK(mgmds[i]->wait_confirmed_config());

  // Check binary config files created
  CHECK(file_exists(path(wd.path(),
                         "ndb_1_config.bin.1",
                         NULL).c_str()));
  CHECK(file_exists(path(wd.path(),
                         "ndb_2_config.bin.1",
                         NULL).c_str()));

  g_err << "** Restart one ndb_mgmd at a time --reload + --initial" << endl;
  for (unsigned i = 0; i < mgmds.size(); i++)
  {
    CHECK(mgmds[i]->stop());
    CHECK(mgmds[i]->start_from_config_ini(wd.path(),
                                          "--reload", "--initial", NULL));
    CHECK(mgmds[i]->connect(config));
    CHECK(mgmds[i]->wait_confirmed_config());

    // check ndb_X_config.bin.1 still exists but not ndb_X_config.bin.2
    CHECK(file_exists(path(wd.path(),
                           "ndb_1_config.bin.1",
                           NULL).c_str()));
    CHECK(file_exists(path(wd.path(),
                           "ndb_2_config.bin.1",
                           NULL).c_str()));

    CHECK(!file_exists(path(wd.path(),
                            "ndb_1_config.bin.2",
                            NULL).c_str()));
    CHECK(!file_exists(path(wd.path(),
                            "ndb_2_config.bin.2",
                            NULL).c_str()));
  }

  g_err << "** Restart one ndb_mgmd at a time --initial" << endl;
  for (unsigned i = 0; i < mgmds.size(); i++)
  {
    CHECK(mgmds[i]->stop());
    CHECK(mgmds[i]->start_from_config_ini(wd.path(),
                                          "--initial", NULL));
    CHECK(mgmds[i]->connect(config));
    CHECK(mgmds[i]->wait_confirmed_config());

    // check ndb_X_config.bin.1 still exists but not ndb_X_config.bin.2
    CHECK(file_exists(path(wd.path(),
                           "ndb_1_config.bin.1",
                           NULL).c_str()));
    CHECK(file_exists(path(wd.path(),
                           "ndb_2_config.bin.1",
                           NULL).c_str()));

    CHECK(!file_exists(path(wd.path(),
                            "ndb_1_config.bin.2",
                            NULL).c_str()));
    CHECK(!file_exists(path(wd.path(),
                            "ndb_2_config.bin.2",
                            NULL).c_str()));
  }

  g_err << "** Create config2.ini" << endl;
  CHECK(ConfigFactory::put(config, "ndb_mgmd", 1, "ArbitrationDelay", 100));
  CHECK(ConfigFactory::write_config_ini(config,
                                        path(wd.path(),
                                             "config2.ini",
                                             NULL).c_str()));

  g_err << "** Restart one ndb_mgmd at a time --initial should not work" << endl;
  for (unsigned i = 0; i < mgmds.size(); i++)
  {
    CHECK(mgmds[i]->stop());
    // Start from config2.ini
    CHECK(mgmds[i]->start_from_config_ini(wd.path(),
                                          "-f config2.ini",
                                          "--initial", NULL));

    // Wait for mgmd to exit and check return status
    int ret;
    CHECK(mgmds[i]->wait(ret));
    CHECK(ret == 1);

    // check config files exist only for the still running mgmd(s)
    for (unsigned j = 0; j < mgmds.size(); j++)
    {
      BaseString tmp;
      tmp.assfmt("ndb_%d_config.bin.1", j+1);
      CHECK(file_exists(path(wd.path(),
                             tmp.c_str(),
                             NULL).c_str()) == (j != i));
    }

    // Start from config.ini again
    CHECK(mgmds[i]->start_from_config_ini(wd.path(),
                                          "--initial",
                                          "--reload",
                                          NULL));
    CHECK(mgmds[i]->connect(config));
    CHECK(mgmds[i]->wait_confirmed_config());
  }

  g_err << "** Reload from config2.ini" << endl;
  for (unsigned i = 0; i < mgmds.size(); i++)
  {
    CHECK(mgmds[i]->stop());
    // Start from config2.ini
    CHECK(mgmds[i]->start_from_config_ini(wd.path(),
                                          "-f config2.ini",
                                          "--reload", NULL));
    CHECK(mgmds[i]->connect(config));
    CHECK(mgmds[i]->wait_confirmed_config());
  }

  CHECK(file_exists(path(wd.path(),
                         "ndb_1_config.bin.1",
                         NULL).c_str()));
  CHECK(file_exists(path(wd.path(),
                         "ndb_2_config.bin.1",
                         NULL).c_str()));

  Uint32 timeout = 30;
  CHECK(file_exists(path(wd.path(),
                         "ndb_1_config.bin.2",
                         NULL).c_str(), timeout));
  CHECK(file_exists(path(wd.path(),
                         "ndb_2_config.bin.2",
                         NULL).c_str(), timeout));

  g_err << "** Reload mgmd initial(from generation=2)" << endl;
  for (unsigned i = 0; i < mgmds.size(); i++)
  {
    CHECK(mgmds[i]->stop());
    CHECK(mgmds[i]->start_from_config_ini(wd.path(),
                                          "-f config2.ini",
                                          "--reload", "--initial", NULL));

    CHECK(mgmds[i]->connect(config));
    CHECK(mgmds[i]->wait_confirmed_config());

     // check config files exist
    for (unsigned j = 0; j < mgmds.size(); j++)
    {
      BaseString tmp;
      tmp.assfmt("ndb_%d_config.bin.1", j+1);
      CHECK(file_exists(path(wd.path(),
                             tmp.c_str(),
                             NULL).c_str()) == (i < j));

      tmp.assfmt("ndb_%d_config.bin.2", j+1);
      CHECK(file_exists(path(wd.path(),
                             tmp.c_str(),
                             NULL).c_str(),
                        timeout));
    }
  }

  return NDBT_OK;
}



int runTestBug42015(NDBT_Context* ctx, NDBT_Step* step)
{
  NDBT_Workingdir wd("test_mgmd"); // temporary working directory

  g_err << "** Create config.ini" << endl;
  Properties config = ConfigFactory::create(2);
  CHECK(ConfigFactory::write_config_ini(config,
                                        path(wd.path(),
                                             "config.ini",
                                             NULL).c_str()));

  MgmdProcessList mgmds;
  // Start ndb_mgmd 1 from config.ini
  Mgmd* mgmd = new Mgmd(1);
  mgmds.push_back(mgmd);
  CHECK(mgmd->start_from_config_ini(wd.path()));

  // Start ndb_mgmd 2 by fetching from first
  Mgmd* mgmd2 = new Mgmd(2);
  mgmds.push_back(mgmd2);
  CHECK(mgmd2->start(wd.path(),
                     "--ndb-connectstring",
                     mgmd->connectstring(config).c_str(),
                     NULL));

  // Connect the ndb_mgmd(s)
  for (unsigned i = 0; i < mgmds.size(); i++)
    CHECK(mgmds[i]->connect(config));

  // wait for confirmed config
  for (unsigned i = 0; i < mgmds.size(); i++)
    CHECK(mgmds[i]->wait_confirmed_config());

  // Check binary config files created
  CHECK(file_exists(path(wd.path(),
                         "ndb_1_config.bin.1",
                         NULL).c_str()));
  CHECK(file_exists(path(wd.path(),
                         "ndb_2_config.bin.1",
                         NULL).c_str()));

  return NDBT_OK;

}

/* Test for bug 53008:  --skip-config-cache */
int runTestNoConfigCache(NDBT_Context* ctx, NDBT_Step* step)
{
  NDBT_Workingdir wd("test_mgmd"); // temporary working directory
  
  g_err << "** Create config.ini" << endl;
  Properties config = ConfigFactory::create();
  CHECK(ConfigFactory::write_config_ini(config,
                                        path(wd.path(),
                                             "config.ini",
                                             NULL).c_str()));
  
  MgmdProcessList mgmds;

  // Start ndb_mgmd  from config.ini
  Mgmd* mgmd = new Mgmd(1);
  mgmds.push_back(mgmd);

  CHECK(mgmd->start_from_config_ini(wd.path(), "--skip-config-cache", NULL));
     
  // Connect the ndb_mgmd(s)
  CHECK(mgmd->connect(config));
  
  // wait for confirmed config
  CHECK(mgmd->wait_confirmed_config());
  
  // Check binary config files *not* created
  bool bin_conf_file = file_exists(path(wd.path(),
                                        "ndb_1_config.bin.1", 
                                        NULL).c_str());
  CHECK(bin_conf_file == false);

  mgmd->stop();
  return NDBT_OK;
}


/* Test for BUG#13428853 */
int runTestNoConfigCache_DontCreateConfigDir(NDBT_Context* ctx, NDBT_Step* step)
{
  NDBT_Workingdir wd("test_mgmd"); // temporary working directory

  g_err << "** Create config.ini" << endl;
  Properties config = ConfigFactory::create();
  CHECK(ConfigFactory::write_config_ini(config,
                                        path(wd.path(),
                                             "config.ini",
                                             NULL).c_str()));

  MgmdProcessList mgmds;

  g_err << "Test no configdir is created with --skip-config-cache" << endl;
  Mgmd* mgmd = new Mgmd(1);
  mgmds.push_back(mgmd);

  CHECK(mgmd->start_from_config_ini(wd.path(),
                                    "--skip-config-cache",
                                    "--config-dir=dir37",
                                    NULL));

  // Connect the ndb_mgmd(s)
  CHECK(mgmd->connect(config));

  // wait for confirmed config
  CHECK(mgmd->wait_confirmed_config());

  // Check configdir not created
  CHECK(!file_exists(path(wd.path(), "dir37", NULL).c_str()));

  mgmd->stop();

  g_err << "Also test --initial --skip-config-cache" << endl;
  // Also test starting ndb_mgmd --initial --skip-config-cache
  CHECK(mgmd->start_from_config_ini(wd.path(),
                                    "--skip-config-cache",
                                    "--initial",
                                    "--config-dir=dir37",
                                    NULL));
  // Connect the ndb_mgmd(s)
  CHECK(mgmd->connect(config));

  // wait for confirmed config
  CHECK(mgmd->wait_confirmed_config());

  // Check configdir not created
  CHECK(!file_exists(path(wd.path(), "dir37", NULL).c_str()));

  mgmd->stop();
  return NDBT_OK;
}


int runTestNoConfigCache_Fetch(NDBT_Context* ctx, NDBT_Step* step)
{
  NDBT_Workingdir wd("test_mgmd"); // temporary working directory

  Properties config = ConfigFactory::create(2);
  CHECK(ConfigFactory::write_config_ini(config,
                                        path(wd.path(),
                                             "config.ini",
                                             NULL).c_str()));

  MgmdProcessList mgmds;
  // Start ndb_mgmd 1 from config.ini without config cache
  Mgmd* mgmd = new Mgmd(1);
  mgmds.push_back(mgmd);
  CHECK(mgmd->start_from_config_ini(wd.path(),
                                    "--skip-config-cache",
                                    NULL));

  // Start ndb_mgmd 2 without config cache and by fetching from first
  Mgmd* mgmd2 = new Mgmd(2);
  mgmds.push_back(mgmd2);
  CHECK(mgmd2->start(wd.path(),
                     "--ndb-connectstring",
                     mgmd->connectstring(config).c_str(),
                     "--skip-config-cache",
                     NULL));

  // Connect the ndb_mgmd(s)
  for (unsigned i = 0; i < mgmds.size(); i++)
    CHECK(mgmds[i]->connect(config));

  // wait for confirmed config
  for (unsigned i = 0; i < mgmds.size(); i++)
    CHECK(mgmds[i]->wait_confirmed_config());

  return NDBT_OK;

}


int runTestNowaitNodes(NDBT_Context* ctx, NDBT_Step* step)
{
  MgmdProcessList mgmds;
  NDBT_Workingdir wd("test_mgmd"); // temporary working directory

  // Create config.ini
  unsigned nodeids[] = { 1, 2 };
  Properties config = ConfigFactory::create(2, 1, 1, nodeids);
  CHECK(ConfigFactory::write_config_ini(config,
                                        path(wd.path(),
                                             "config.ini",
                                             NULL).c_str()));


  BaseString binfile[2];
  binfile[0].assfmt("ndb_%u_config.bin.1", nodeids[0]);
  binfile[1].assfmt("ndb_%u_config.bin.1", nodeids[1]);

  // Start first ndb_mgmd
  Mgmd* mgmd1 = new Mgmd(nodeids[0]);
  {
    mgmds.push_back(mgmd1);
    BaseString arg;
    arg.assfmt("--nowait-nodes=%u", nodeids[1]);
    CHECK(mgmd1->start_from_config_ini(wd.path(),
                                       "--initial",
                                       arg.c_str(),
                                       NULL));

    // Connect the ndb_mgmd
    CHECK(mgmd1->connect(config));

    // wait for confirmed config
    CHECK(mgmd1->wait_confirmed_config());

    // Check binary config file created
    CHECK(file_exists(path(wd.path(),
                           binfile[0].c_str(),
                           NULL).c_str()));
  }

  // Start second ndb_mgmd
  {
    Mgmd* mgmd2 = new Mgmd(nodeids[1]);
    mgmds.push_back(mgmd2);
    CHECK(mgmd2->start_from_config_ini(wd.path(),
                                       "--initial",
                                       NULL));

    // Connect the ndb_mgmd
    CHECK(mgmd2->connect(config));

    // wait for confirmed config
    CHECK(mgmd2->wait_confirmed_config());

    // Check binary config file created
    CHECK(file_exists(path(wd.path(),
                           binfile[1].c_str(),
                           NULL).c_str()));

  }

  // Create new config.ini
  g_err << "** Create config2.ini" << endl;
  CHECK(ConfigFactory::put(config, "ndb_mgmd", nodeids[0], "ArbitrationDelay", 100));
  CHECK(ConfigFactory::write_config_ini(config,
                                        path(wd.path(),
                                             "config2.ini",
                                             NULL).c_str()));

  g_err << "** Reload second mgmd from config2.ini" << endl;
  {
    Mgmd* mgmd2 = mgmds[1];
    CHECK(mgmd2->stop());
    // Start from config2.ini
    CHECK(mgmd2->start_from_config_ini(wd.path(),
                                       "-f config2.ini",
                                       "--reload", NULL));
    CHECK(mgmd2->connect(config));
    CHECK(mgmd1->wait_confirmed_config());
    CHECK(mgmd2->wait_confirmed_config());

    CHECK(file_exists(path(wd.path(),
                           binfile[0].c_str(),
                           NULL).c_str()));
    CHECK(file_exists(path(wd.path(),
                           binfile[1].c_str(),
                           NULL).c_str()));

    // Both ndb_mgmd(s) should have reloaded and new binary config exist
    binfile[0].assfmt("ndb_%u_config.bin.2", nodeids[0]);
    binfile[1].assfmt("ndb_%u_config.bin.2", nodeids[1]);
    CHECK(file_exists(path(wd.path(),
                           binfile[0].c_str(),
                           NULL).c_str()));
    CHECK(file_exists(path(wd.path(),
                           binfile[1].c_str(),
                           NULL).c_str()));
  }

  // Stop the ndb_mgmd(s)
  for (unsigned i = 0; i < mgmds.size(); i++)
    CHECK(mgmds[i]->stop());

  return NDBT_OK;
}


int runTestNowaitNodes2(NDBT_Context* ctx, NDBT_Step* step)
{
  int ret;
  NDBT_Workingdir wd("test_mgmd"); // temporary working directory

  // Create config.ini
  Properties config = ConfigFactory::create(2);
  CHECK(ConfigFactory::write_config_ini(config,
                                        path(wd.path(),
                                             "config.ini",
                                             NULL).c_str()));

  g_err << "** Start mgmd1 from config.ini" << endl;
  MgmdProcessList mgmds;
  Mgmd* mgmd1 = new Mgmd(1);
  mgmds.push_back(mgmd1);
  CHECK(mgmd1->start_from_config_ini(wd.path(),
                                     "--initial",
                                     "--nowait-nodes=1-255",
                                     NULL));
  CHECK(mgmd1->connect(config));
  CHECK(mgmd1->wait_confirmed_config());

  // check config files exist
  CHECK(file_exists(path(wd.path(),
                         "ndb_1_config.bin.1",
                         NULL).c_str()));

  g_err << "** Create config2.ini" << endl;
  CHECK(ConfigFactory::put(config, "ndb_mgmd", 1, "ArbitrationDelay", 100));
  CHECK(ConfigFactory::write_config_ini(config,
                                        path(wd.path(),
                                             "config2.ini",
                                             NULL).c_str()));

  g_err << "** Start mgmd2 from config2.ini" << endl;
  Mgmd* mgmd2 = new Mgmd(2);
  mgmds.push_back(mgmd2);
  CHECK(mgmd2->start_from_config_ini(wd.path(),
                                     "-f config2.ini",
                                     "--initial",
                                     "--nowait-nodes=1-255",
                                     NULL));
  CHECK(mgmd2->wait(ret));
  CHECK(ret == 1);

  CHECK(mgmd1->stop());

  g_err << "** Start mgmd2 again from config2.ini" << endl;
  CHECK(mgmd2->start_from_config_ini(wd.path(),
                                     "-f config2.ini",
                                     "--initial",
                                     "--nowait-nodes=1-255",
                                     NULL));


  CHECK(mgmd2->connect(config));
  CHECK(mgmd2->wait_confirmed_config());

  // check config files exist
  CHECK(file_exists(path(wd.path(),
                         "ndb_2_config.bin.1",
                         NULL).c_str()));

  g_err << "** Start mgmd1 from config.ini, mgmd2 should shutdown" << endl;
  CHECK(mgmd1->start_from_config_ini(wd.path(),
                                     "--initial",
                                     "--nowait-nodes=1-255",
                                     NULL));
  CHECK(mgmd2->wait(ret));
  CHECK(ret == 1);

  CHECK(mgmd1->stop());

  return NDBT_OK;
}

int
runBug56844(NDBT_Context* ctx, NDBT_Step* step)
{
  NDBT_Workingdir wd("test_mgmd"); // temporary working directory

  g_err << "** Create config.ini" << endl;
  Properties config = ConfigFactory::create(2);
  CHECK(ConfigFactory::write_config_ini(config,
                                        path(wd.path(),
                                             "config.ini",
                                             NULL).c_str()));
  // Start ndb_mgmd(s)
  MgmdProcessList mgmds;
  for (int i = 1; i <= 2; i++)
  {
    Mgmd* mgmd = new Mgmd(i);
    mgmds.push_back(mgmd);
    CHECK(mgmd->start_from_config_ini(wd.path()));
  }

  // Connect the ndb_mgmd(s)
  for (unsigned i = 0; i < mgmds.size(); i++)
  {
    CHECK(mgmds[i]->connect(config));
  }

  // wait for confirmed config
  for (unsigned i = 0; i < mgmds.size(); i++)
  {
    CHECK(mgmds[i]->wait_confirmed_config());
  }

  // stop them
  for (unsigned i = 0; i < mgmds.size(); i++)
  {
    CHECK(mgmds[i]->stop());
  }

  // Check binary config files created
  CHECK(file_exists(path(wd.path(),
                         "ndb_1_config.bin.1",
                         NULL).c_str()));
  CHECK(file_exists(path(wd.path(),
                         "ndb_2_config.bin.1",
                         NULL).c_str()));

  CHECK(ConfigFactory::put(config, "ndb_mgmd", 1, "ArbitrationDelay", 100));
  CHECK(ConfigFactory::write_config_ini(config,
                                        path(wd.path(),
                                             "config2.ini",
                                             NULL).c_str()));
  Uint32 no = 2;
  int loops = ctx->getNumLoops();
  for (int l = 0; l < loops; l++, no++)
  {
    g_err << l << ": *** Reload from config.ini" << endl;
    for (unsigned i = 0; i < mgmds.size(); i++)
    {
      // Start from config2.ini
      CHECK(mgmds[i]->start_from_config_ini(wd.path(),
                                            (l & 1) == 1 ?
                                            "-f config.ini" :
                                            "-f config2.ini",
                                            "--reload", NULL));
    }
    for (unsigned i = 0; i < mgmds.size(); i++)
    {
      CHECK(mgmds[i]->connect(config));
      CHECK(mgmds[i]->wait_confirmed_config());
    }

    /**
     * Since it will first be confirmed...
     *   and then once connected to other ndb_nmgmd start a config
     *   change, it can take a bit until new config exists...
     *   allow 30s
     */
    Uint32 timeout = 30;
    for (unsigned i = 0; i < mgmds.size(); i++)
    {
      BaseString p = path(wd.path(), "", NULL);
      p.appfmt("ndb_%u_config.bin.%u", i+1, no);
      g_err << "CHECK(" << p.c_str() << ")" << endl;
      CHECK(file_exists(p.c_str(), timeout));
    }

    for (unsigned i = 0; i < mgmds.size(); i++)
    {
      CHECK(mgmds[i]->stop());
    }
  }
  return NDBT_OK;
}

static bool
get_status(const char* connectstring,
           Properties& status)
{
  NdbMgmd ndbmgmd;
  if (!ndbmgmd.connect(connectstring))
    return false;

  Properties args;
  if (!ndbmgmd.call("get status", args,
                    "node status", status, NULL, true))
  {
    g_err << "fetch_mgmd_status: mgmd.call failed" << endl;
    return false;
  }
  return true;
}

static bool
value_equal(Properties& status,
            int nodeid, const char* name,
            const char* expected_value)
{
  const char* value;
  BaseString key;
  key.assfmt("node.%d.%s", nodeid, name);
  if (!status.get(key.c_str(), &value))
  {
    g_err << "value_equal: no value found for '" << name
          << "." << nodeid << "'" << endl;
    return false;
  }

  if (strcmp(value, expected_value))
  {
    g_err << "value_equal: found unexpected value: '" << value
          << "', expected: '" << expected_value << "'" <<endl;
    return false;
  }
  g_info << "'" << value << "'=='" << expected_value << "'" << endl;
  return true;
}

#include <ndb_version.h>

int runTestBug12352191(NDBT_Context* ctx, NDBT_Step* step)
{
  BaseString version;
  version.assfmt("%u", NDB_VERSION_D);
  BaseString mysql_version;
  mysql_version.assfmt("%u", NDB_MYSQL_VERSION_D);
  BaseString address_ipv4("127.0.0.1");
  BaseString address_ipv6("::1");

  NDBT_Workingdir wd("test_mgmd"); // temporary working directory

  g_err << "** Create config.ini" << endl;
  Properties config = ConfigFactory::create(2);
  CHECK(ConfigFactory::write_config_ini(config,
                                        path(wd.path(),
                                             "config.ini",
                                             NULL).c_str()));

  MgmdProcessList mgmds;
  const int nodeid1 = 1;
  Mgmd* mgmd1 = new Mgmd(nodeid1);
  mgmds.push_back(mgmd1);

  const int nodeid2 = 2;
  Mgmd* mgmd2 = new Mgmd(nodeid2);
  mgmds.push_back(mgmd2);

  // Start first mgmd
  CHECK(mgmd1->start_from_config_ini(wd.path()));
  CHECK(mgmd1->connect(config));

  Properties status1;
  CHECK(get_status(mgmd1->connectstring(config).c_str(), status1));
  //status1.print();
  // Check status for own mgm node, always CONNECTED
  CHECK(value_equal(status1, nodeid1, "type", "MGM"));
  CHECK(value_equal(status1, nodeid1, "status", "CONNECTED"));
  CHECK(value_equal(status1, nodeid1, "version", version.c_str()));
  CHECK(value_equal(status1, nodeid1, "mysql_version", mysql_version.c_str()));
  CHECK(value_equal(status1, nodeid1, "address", address_ipv4.c_str()) ||
        value_equal(status1, nodeid1, "address", address_ipv6.c_str()));
  CHECK(value_equal(status1, nodeid1, "startphase", "0"));
  CHECK(value_equal(status1, nodeid1, "dynamic_id", "0"));
  CHECK(value_equal(status1, nodeid1, "node_group", "0"));
  CHECK(value_equal(status1, nodeid1, "connect_count", "0"));

  // Check status for other mgm node
  // not started yet -> NO_CONTACT, no address, no versions
  CHECK(value_equal(status1, nodeid2, "type", "MGM"));
  CHECK(value_equal(status1, nodeid2, "status", "NO_CONTACT"));
  CHECK(value_equal(status1, nodeid2, "version", "0"));
  CHECK(value_equal(status1, nodeid2, "mysql_version", "0"));
  CHECK(value_equal(status1, nodeid2, "address", ""));
  CHECK(value_equal(status1, nodeid2, "startphase", "0"));
  CHECK(value_equal(status1, nodeid2, "dynamic_id", "0"));
  CHECK(value_equal(status1, nodeid2, "node_group", "0"));
  CHECK(value_equal(status1, nodeid2, "connect_count", "0"));

  // Start second mgmd
  CHECK(mgmd2->start_from_config_ini(wd.path()));
  CHECK(mgmd2->connect(config));

  // wait for confirmed config
  for (unsigned i = 0; i < mgmds.size(); i++)
    CHECK(mgmds[i]->wait_confirmed_config());

  Properties status2;
  CHECK(get_status(mgmd2->connectstring(config).c_str(), status2));
  //status2.print();
  // Check status for own mgm node, always CONNECTED
  CHECK(value_equal(status2, nodeid2, "type", "MGM"));
  CHECK(value_equal(status2, nodeid2, "status", "CONNECTED"));
  CHECK(value_equal(status2, nodeid2, "version", version.c_str()));
  CHECK(value_equal(status2, nodeid2, "mysql_version", mysql_version.c_str()));
  CHECK(value_equal(status2, nodeid2, "address", address_ipv4.c_str()) ||
        value_equal(status2, nodeid2, "address", address_ipv6.c_str()));
  CHECK(value_equal(status2, nodeid2, "startphase", "0"));
  CHECK(value_equal(status2, nodeid2, "dynamic_id", "0"));
  CHECK(value_equal(status2, nodeid2, "node_group", "0"));
  CHECK(value_equal(status2, nodeid2, "connect_count", "0"));

  // Check status for other mgm node
  // both started now -> CONNECTED, address and versions filled in
  CHECK(value_equal(status2, nodeid1, "type", "MGM"));
  CHECK(value_equal(status2, nodeid1, "status", "CONNECTED"));
  CHECK(value_equal(status2, nodeid1, "version", version.c_str()));
  CHECK(value_equal(status2, nodeid1, "mysql_version", mysql_version.c_str()));
  CHECK(value_equal(status2, nodeid1, "address", address_ipv4.c_str()) ||
        value_equal(status2, nodeid1, "address", address_ipv6.c_str()));
  CHECK(value_equal(status2, nodeid1, "startphase", "0"));
  CHECK(value_equal(status2, nodeid1, "dynamic_id", "0"));
  CHECK(value_equal(status2, nodeid1, "node_group", "0"));
  CHECK(value_equal(status2, nodeid1, "connect_count", "0"));

  Properties status3;
  CHECK(get_status(mgmd1->connectstring(config).c_str(), status3));
  //status3.print();
  // Check status for own mgm node, always CONNECTED
  CHECK(value_equal(status3, nodeid1, "type", "MGM"));
  CHECK(value_equal(status3, nodeid1, "status", "CONNECTED"));
  CHECK(value_equal(status3, nodeid1, "version", version.c_str()));
  CHECK(value_equal(status3, nodeid1, "mysql_version", mysql_version.c_str()));
  CHECK(value_equal(status3, nodeid1, "address", address_ipv4.c_str()) ||
        value_equal(status3, nodeid1, "address", address_ipv6.c_str()));
  CHECK(value_equal(status3, nodeid1, "startphase", "0"));
  CHECK(value_equal(status3, nodeid1, "dynamic_id", "0"));
  CHECK(value_equal(status3, nodeid1, "node_group", "0"));
  CHECK(value_equal(status3, nodeid1, "connect_count", "0"));

  // Check status for other mgm node
  // both started now -> CONNECTED, address and versions filled in
  CHECK(value_equal(status3, nodeid2, "type", "MGM"));
  CHECK(value_equal(status3, nodeid2, "status", "CONNECTED"));
  CHECK(value_equal(status3, nodeid2, "version", version.c_str()));
  CHECK(value_equal(status3, nodeid2, "mysql_version", mysql_version.c_str()));
  CHECK(value_equal(status3, nodeid2, "address", address_ipv4.c_str()) ||
        value_equal(status3, nodeid2, "address", address_ipv6.c_str()));
  CHECK(value_equal(status3, nodeid2, "startphase", "0"));
  CHECK(value_equal(status3, nodeid2, "dynamic_id", "0"));
  CHECK(value_equal(status3, nodeid2, "node_group", "0"));
  CHECK(value_equal(status3, nodeid2, "connect_count", "0"));

  return NDBT_OK;

}

int
runBug61607(NDBT_Context* ctx, NDBT_Step* step)
{
  NDBT_Workingdir wd("test_mgmd"); // temporary working directory

  // Create config.ini
  const int cnt_mgmd = 1;
  Properties config = ConfigFactory::create(cnt_mgmd);
  CHECK(ConfigFactory::write_config_ini(config,
                                        path(wd.path(),
                                             "config.ini",
                                             NULL).c_str()));
  // Start ndb_mgmd(s)
  MgmdProcessList mgmds;
  for (int i = 1; i <= cnt_mgmd; i++)
  {
    Mgmd* mgmd = new Mgmd(i);
    mgmds.push_back(mgmd);
    CHECK(mgmd->start_from_config_ini(wd.path()));
  }

  // Connect the ndb_mgmd(s)
  for (unsigned i = 0; i < mgmds.size(); i++)
    CHECK(mgmds[i]->connect(config));

  // wait for confirmed config
  for (unsigned i = 0; i < mgmds.size(); i++)
    CHECK(mgmds[i]->wait_confirmed_config());

  // Check binary config files created
  CHECK(file_exists(path(wd.path(),
                         "ndb_1_config.bin.1",
                         NULL).c_str()));

  int no_of_nodes = 0;
  int * node_ids = 0;
  int initialstart = 0;
  int nostart = 0;
  int abort = 0;
  int force = 0;
  int need_disconnect = 0;
  int res = ndb_mgm_restart4(mgmds[0]->handle(), no_of_nodes, node_ids,
                             initialstart, nostart, abort, force,
                             &need_disconnect);


  return res == 0 ? NDBT_OK : NDBT_FAILED;
}

int
runStopDuringStart(NDBT_Context* ctx, NDBT_Step* step)
{
  MgmdProcessList mgmds;
  NDBT_Workingdir wd("test_mgmd"); // temporary working directory

  // Create config.ini
  unsigned nodeids[] = { 251, 252 };
  Properties config = ConfigFactory::create(2, 1, 1, nodeids);
  CHECK(ConfigFactory::write_config_ini(config,
                                        path(wd.path(),
                                             "config.ini",
                                             NULL).c_str()));

  for (unsigned i = 0; i < NDB_ARRAY_SIZE(nodeids); i++)
  {
    Mgmd* mgmd = new Mgmd(nodeids[i]);
    mgmds.push_back(mgmd);
    CHECK(mgmd->start_from_config_ini(wd.path()));
  }

  // Connect the ndb_mgmd(s)
  for (unsigned i = 0; i < mgmds.size(); i++)
    CHECK(mgmds[i]->connect(config));

  // wait for confirmed config
  for (unsigned i = 0; i < mgmds.size(); i++)
    CHECK(mgmds[i]->wait_confirmed_config());

  // Check binary config files created
  for (unsigned i = 0; i < mgmds.size(); i++)
  {
    BaseString file;
    file.assfmt("ndb_%u_config.bin.1", nodeids[i]);
    CHECK(file_exists(path(wd.path(),
                           file.c_str(),
                           NULL).c_str()));
  }

  // stop them
  for (unsigned i = 0; i < mgmds.size(); i++)
  {
    mgmds[i]->stop();
    int exitCode;
    mgmds[i]->wait(exitCode);
  }

  // restart one with error-insert 100
  // => it shall exit during start...
  mgmds[0]->start(wd.path(), "--error-insert=100", NULL);

  // restart rest normally
  for (unsigned i = 1; i < mgmds.size(); i++)
  {
    mgmds[i]->start(wd.path());
  }

  // wait first one to terminate
  int exitCode;
  mgmds[0]->wait(exitCode);
  NdbSleep_MilliSleep(3000);

  // check other OK
  for (unsigned i = 1; i < mgmds.size(); i++)
  {
    CHECK(mgmds[i]->connect(config));
    CHECK(mgmds[i]->wait_confirmed_config());
  }

  // now restart without error insert
  mgmds[0]->start(wd.path());

  // connect
  CHECK(mgmds[0]->connect(config));

  // all should be ok
  for (unsigned i = 0; i < mgmds.size(); i++)
    CHECK(mgmds[i]->wait_confirmed_config());

  return NDBT_OK;
}

/* WL#13860: AllowUnresolvedHostnames=false (the default)
   Check that MGM will not start up with unresolved hostname in configuration.
*/
int
runTestUnresolvedHosts1(NDBT_Context* ctx, NDBT_Step* step)
{
  NDBT_Workingdir wd("test_mgmd");

  char hostname[200];
  CHECK(gethostname(hostname, sizeof(hostname)) == 0);

  Properties config, mgm, ndb, api;
  mgm.put("NodeId", 1);
  mgm.put("HostName", hostname);
  mgm.put("PortNumber", ConfigFactory::get_ndbt_base_port() + /* mysqld */ 1);
  ndb.put("NodeId", 2);
  ndb.put("HostName", "xx-no-such-host.no.oracle.com.");
  ndb.put("NoOfReplicas", 1);
  api.put("NodeId", 3);
  config.put("ndb_mgmd", 1, &mgm);
  config.put("ndbd", 2, &ndb);
  config.put("mysqld", 3, &api);

  CHECK(ConfigFactory::write_config_ini(config,
                                        path(wd.path(),
                                             "config.ini",
                                             NULL).c_str()));

  Mgmd mgmd(1);
  int exit_value;
  CHECK(mgmd.start_from_config_ini(wd.path()));
  CHECK(mgmd.wait(exit_value, 50));
  CHECK(exit_value == 1);
  return NDBT_OK;
}

/* WL#13860: AllowUnresolvedHostnames=true
   This test uses a configuration with 144 data nodes, of which 143 have
   unresolvable hostnames, and shows that the one data node with a usable
   hostname succesfully connects, while a second data node with a bad hostname
   times out (within 40 seconds) with failure to allocate node id.
*/
int
runTestUnresolvedHosts2(NDBT_Context* ctx, NDBT_Step* step)
{
  NDBT_Workingdir wd("test_mgmd");

  char hostname[200];
  CHECK(gethostname(hostname, sizeof(hostname)) == 0);

  Properties config;
  {
    // 144 ndbds, nodeid 1 -> 144
    for(int i = 1 ; i <= 144 ; i++) {
      Properties ndbd;
      ndbd.put("NodeId", i);
      ndbd.put("NoOfReplicas", 4);
      if(i == 1) {
        // Node 1 has a good hostname.
        ndbd.put("HostName", hostname);
      } else  {
        // The other have unresolvable hostnames.
        ndbd.put("HostName", "xx-no-such-host.no.oracle.com.");
      }
      config.put("ndbd", i, &ndbd);
    }
  }
  {
    // 1 ndb_mgmd, nodeid 145
    Properties mgmd;
    mgmd.put("NodeId", 145);
    mgmd.put("HostName", hostname);
    mgmd.put("PortNumber", ConfigFactory::get_ndbt_base_port() + /* mysqld */ 1);
    config.put("ndb_mgmd", 145, &mgmd);
  }
  {
    // 1 mysqld, nodeid 151
    Properties mysqld;
    mysqld.put("NodeId", 151);
    config.put("mysqld", 151, &mysqld);
  }
  {
    Properties tcp;
    tcp.put("AllowUnresolvedHostnames", "true");
    config.put("TCP DEFAULT", &tcp);
  }

  CHECK(ConfigFactory::write_config_ini(config,
                                        path(wd.path(),
                                             "config.ini",
                                             NULL).c_str()));

  /* Start the management node and data node 1 together, and expect this to
     succeed despite the unresolvable host names and large configuration.
  */
  Mgmd mgmd(145);
  Ndbd ndbd1(1);

  CHECK(ndbd1.start(wd.path(), mgmd.connectstring(config))); // Start data node 1
  CHECK(mgmd.start_from_config_ini(wd.path()));    // Start management node
  CHECK(mgmd.connect(config));                     // Connect to management node
  CHECK(mgmd.wait_confirmed_config());             // Wait for configuration

  /* Start data node 2.
     Expect it to run for at least 20 seconds, trying to allocate a node id.
     But in the second 20-second interval, it will time out and shut down.
  */
  int ndbd_exit_code;
  Ndbd ndbd2(2);
  CHECK(ndbd2.start(wd.path(), mgmd.connectstring(config)));
  CHECK(ndbd2.wait(ndbd_exit_code, 200) == 0);   // first 20-second wait
  CHECK(ndbd2.wait(ndbd_exit_code, 200) == 1);   // second 20-second wait

  return NDBT_OK;
}

int
runTestMgmdwithoutnodeid(NDBT_Context* ctx, NDBT_Step* step)
{
  NDBT_Workingdir wd("test_mgmd");
  Vector<BaseString> search_list;

  Properties config, mgm, mgm2, mgm3, ndb, api;
  mgm.put("HostName", "190.10.10.4");
  ndb.put("HostName", "190.10.10.1");
  ndb.put("NoOfReplicas", 1);
  config.put("ndb_mgmd", 1, &mgm);
  config.put("ndbd", 2, &ndb);
  config.put("mysqld", 3, &api);

  CHECK(ConfigFactory::write_config_ini(config,
    path(wd.path(),
      "config.ini",
      NULL).c_str()));

  Mgmd mgmd;
  int exit_value;
  //write the stdout to temp file
  BaseString out_file = path(wd.path(), "out.txt", NULL);
  FILE* temp_file = fopen(out_file.c_str(), "w");
  int file_desc = open(out_file.c_str(), O_WRONLY | O_APPEND);
  int stdoutCopy = dup(1);
  if (dup2(file_desc, 1) < 0) return NDBT_FAILED;
  close(file_desc);

  //TEST 1: start mgmd without nodeid and unknown address
  with_nodeid = false;
  CHECK(mgmd.start_from_config_ini(wd.path()));
  CHECK(mgmd.wait(exit_value, 300));
  CHECK(exit_value == 1);
  with_nodeid = true;
  search_list.push_back("At least one hostname in the configuration does not match a local interface");

  //TEST 2:start mgmd without nodeid and config containing 2 mgmd
  //sections with same valid hostname
  char hostname[200];
  CHECK(gethostname(hostname, sizeof(hostname)) == 0);
  mgm2.put("HostName", hostname);
  mgm2.put("PortNumber", 1011);
  mgm3.put("HostName", hostname);
  config.put("ndb_mgmd", 4, &mgm2);
  config.put("ndb_mgmd", 5, &mgm3);
  CHECK(ConfigFactory::write_config_ini(config,
    path(wd.path(),
      "config2.ini",
      NULL).c_str()));
  with_nodeid = false;
  CHECK(mgmd.start_from_config_ini(wd.path(), "-f config2.ini",
    "--initial", NULL));
  CHECK(mgmd.wait(exit_value, 300));
  CHECK(exit_value == 1);
  with_nodeid = true;
  search_list.push_back("More than one hostname matches a local interface, including node ids");

  //TEST 3: Check if the error message truncate if the length of hostnames are too big
  Properties config3, ndb3, api3;
  ndb3.put("HostName", "190.10.10.1");
  ndb3.put("NoOfReplicas", 1);
  for (int i = 1; i < 80; i++) {
    Properties p1;
    std::string host_generated = "190.100.100." + std::to_string(i);
    p1.put("HostName", host_generated.c_str());
    config3.put("ndb_mgmd", i, &p1);
  }
  config3.put("ndbd", 80, &ndb3);
  config3.put("mysqld", 81, &api3);
  CHECK(ConfigFactory::write_config_ini(
      config3, path(wd.path(), "config3.ini", NULL).c_str()));
  with_nodeid = false;
  CHECK(mgmd.start_from_config_ini(wd.path(), "-f config3.ini", "--initial",
                                   NULL));
  CHECK(mgmd.wait(exit_value, 300));
  CHECK(exit_value == 1);
  with_nodeid = true;

  //Write the stdout back to the screen
  if (dup2(stdoutCopy, 1) < 0) return NDBT_FAILED;
  close(stdoutCopy);

  // Search output log for the matching error message
  CHECK(Print_find_in_file(out_file.c_str(), search_list) == true);
  fclose(temp_file);
  remove(out_file.c_str());
  return NDBT_OK;
}

NDBT_TESTSUITE(testMgmd);
DRIVER(DummyDriver); /* turn off use of NdbApi */

TESTCASE("Basic2Mgm",
         "Basic test with two mgmd")
{
  INITIALIZER(runTestBasic2Mgm);
}

TESTCASE("Bug42015",
         "Test that mgmd can fetch configuration from another mgmd")
{
  INITIALIZER(runTestBug42015);
}
TESTCASE("NowaitNodes",
         "Test that one mgmd(of 2) can start alone with usage "
         "of --nowait-nodes, then start the second mgmd and it should join")
{
  INITIALIZER(runTestNowaitNodes);
}
TESTCASE("NowaitNodes2",
         "Test that one mgmd(of 2) can start alone with usage "
         "of --nowait-nodes, then start the second mgmd from different "
         "configuration and the one with lowest nodeid should shutdown")
{
  INITIALIZER(runTestNowaitNodes2);
}

TESTCASE("NoCfgCache",
         "Test that when an mgmd is started with --skip-config-cache, "
         "no ndb_xx_config.xx.bin file is created, but you can "
         "connect to the mgm node and retrieve the config.")
{
  INITIALIZER(runTestNoConfigCache);
}
TESTCASE("NoCfgCacheOrConfigDir",
         "Test that when an mgmd is started with --skip-config-cache, "
         "no ndb_xx_config.xx.bin file is created, but you can "
         "connect to the mgm node and retrieve the config.")
{
  INITIALIZER(runTestNoConfigCache_DontCreateConfigDir);
}
TESTCASE("NoCfgCacheFetch",
         "Test that when an mgmd is started with --skip-config-cache, "
         "it can still fetch config from another ndb_mgmd.")
{
  INITIALIZER(runTestNoConfigCache_Fetch);
}
TESTCASE("Bug45495",
         "Test that mgmd can be restarted in any order")
{
  INITIALIZER(runTestBug45495);
}

TESTCASE("Bug56844",
         "Test that mgmd can be reloaded in parallel")
{
  INITIALIZER(runBug56844);
}
TESTCASE("Mgmdwithoutnodeid",
         "Test that mgmd reports proper error message "
         "when configuration contains unresolvable ip address "
         " and does not include node ids")
{
  INITIALIZER(runTestMgmdwithoutnodeid);
}
TESTCASE("Bug12352191",
         "Test mgmd status for other mgmd")
{
  INITIALIZER(runTestBug12352191);
}
TESTCASE("Bug61607",
         "ndb_mgmd incorrectly reports failure when there are no ndbds to stop")
{
  INITIALIZER(runBug61607);
}
TESTCASE("StopDuringStart", "")
{
  INITIALIZER(runStopDuringStart);
}
TESTCASE("UnresolvedHosts1","Test mgmd failure due to unresolvable hostname")
{
  INITIALIZER(runTestUnresolvedHosts1);
}
TESTCASE("UnresolvedHosts2","Test mgmd with AllowUnresolvedHostnames=true")
{
  INITIALIZER(runTestUnresolvedHosts2);
}

NDBT_TESTSUITE_END(testMgmd)

int main(int argc, const char** argv)
{
  ndb_init();
  NDBT_TESTSUITE_INSTANCE(testMgmd);
  testMgmd.setCreateTable(false);
  testMgmd.setRunAllTables(true);
  testMgmd.setConnectCluster(false);
  testMgmd.setEnsureIndexStatTables(false);

#ifdef NDB_USE_GET_ENV
  char buf1[255], buf2[255];
  if (NdbEnv_GetEnv("NDB_MGMD_VALGRIND_EXE", buf1, sizeof(buf1))) {
    exe_valgrind = buf1;
  }

  if (NdbEnv_GetEnv("NDB_MGMD_VALGRIND_ARG", buf2, sizeof(buf2))) {
    arg_valgrind = buf2;
  }
#endif

  return testMgmd.execute(argc, argv);
}

template class Vector<Mgmd*>;

