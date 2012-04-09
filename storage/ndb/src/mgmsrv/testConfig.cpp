/*
  Copyright (c) 2008, 2010, Oracle and/or its affiliates. All rights reserved.


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
#include "InitConfigFileParser.hpp"
#include "ConfigInfo.hpp"
#include "Config.hpp"
#include <portlib/NdbDir.hpp>

#define CHECK(x) \
  if (!(x)) {\
    fprintf(stderr, "testConfig: '"#x"' failed on line %d\n", __LINE__); \
    exit(1); \
  }


static const ConfigInfo g_info;

/*
  Create a small config.ini with the given parameter and run
  it through InitConfigFileParser
 */
bool
check_param(const ConfigInfo::ParamInfo & param)
{
  FILE* config_file= tmpfile();
  CHECK(config_file);

  const char* section= g_info.nameToAlias(param._section);
  if (section == NULL)
    section= param._section;

  if (param._type == ConfigInfo::CI_SECTION)
  {
    fclose(config_file);
    return true;
  }

  if(param._default == MANDATORY)
  {
    // Mandatory parameter
    fclose(config_file);
    return true;
  }
  else
  {
    fprintf(config_file, "[%s]\n", section);
    fprintf(config_file, "%s=%s\n", param._fname,
            param._default ? param._default : "some value");
  }

  // Fill in lines needed for a minimal config
  if (strcmp(section, "NDBD") != 0)
    fprintf(config_file, "[ndbd]\n");
  if (strcmp(param._fname, "NoOfReplicas") != 0)
    fprintf(config_file, "NoOfReplicas=1\n");

  if (strcmp(section, "NDB_MGMD") != 0)
    fprintf(config_file, "[ndb_mgmd]\n");
  if (strcmp(param._fname, "Hostname") != 0)
    fprintf(config_file, "HostName=localhost\n");

  if (strcmp(section, "MYSQLD") != 0)
    fprintf(config_file, "[mysqld]\n");

  rewind(config_file);

  // Run the config file through InitConfigFileParser
  InitConfigFileParser parser;
  Config* conf = parser.parseConfig(config_file);
  fclose(config_file);

  if (conf == NULL)
    return false;
  delete conf;
  return true;
}


bool
check_params(void)
{
  bool ok= true;
  for (int j=0; j<g_info.m_NoOfParams; j++) {
    const ConfigInfo::ParamInfo & param= g_info.m_ParamInfo[j];
    printf("Checking %s...\n", param._fname);
    if (!check_param(param))
    {
      ok= false;
    }
  }

  return true; // Ignore "ok" for now, just checking it doesn't crash
}



Config*
create_config(const char* first, ...)
{
  va_list args;

  FILE* config_file= tmpfile();
  CHECK(config_file);

  va_start(args, first);
  const char* str= first;
  do
    fprintf(config_file, "%s\n", str);
  while((str= va_arg(args, const char*)) != NULL);
  va_end(args);

#if 0
  rewind(config_file);

  char buf[100];
  while(fgets(buf, sizeof(buf), config_file))
    ndbout_c(buf);
#endif

  rewind(config_file);

  InitConfigFileParser parser;
  Config* conf = parser.parseConfig(config_file);
  fclose(config_file);

  return conf;
}

// Global variable for my_getopt
extern "C" const char* my_defaults_file;

static
unsigned
ndb_procid()
{
#ifdef _WIN32
  return (unsigned)GetCurrentProcessId();
#else
  return (unsigned)getpid();
#endif
}

Config*
create_mycnf(const char* first, ...)
{
  va_list args;

  NdbDir::Temp tempdir;
  BaseString mycnf_file;
  mycnf_file.assfmt("%s%stest_my.%u.cnf",
                    tempdir.path(), DIR_SEPARATOR, ndb_procid());

  FILE* config_file= fopen(mycnf_file.c_str(), "w+");
  CHECK(config_file);

  va_start(args, first);
  const char* str= first;
  do
    fprintf(config_file, "%s\n", str);
  while((str= va_arg(args, const char*)) != NULL);
  va_end(args);

#if 0
  rewind(config_file);

  char buf[100];
  while(fgets(buf, sizeof(buf), config_file))
    printf("%s", buf);
#endif

  fflush(config_file);
  rewind(config_file);

  // Trick handle_options to read from the temp file
  const char* save_defaults_file = my_defaults_file;
  my_defaults_file = mycnf_file.c_str();

  InitConfigFileParser parser;
  Config* conf = parser.parse_mycnf();

  // Restore the global variable
  my_defaults_file = save_defaults_file;

  fclose(config_file);

  // Remove file
  unlink(mycnf_file.c_str());

  return conf;
}




void
diff_config(void)
{
  Config* c1=
    create_config("[ndbd]", "NoOfReplicas=1",
                  "[ndb_mgmd]", "HostName=localhost",
                  "[mysqld]", NULL);
  CHECK(c1);
  Config* c2=
    create_config("[ndbd]", "NoOfReplicas=1",
                  "[ndb_mgmd]", "HostName=localhost",
                  "[mysqld]", "[mysqld]", NULL);
  CHECK(c2);

  CHECK(c1->equal(c1));

  CHECK(!c1->equal(c2));
  CHECK(!c2->equal(c1));
  CHECK(!c2->illegal_change(c1));
  CHECK(!c1->illegal_change(c2));

  ndbout_c("==================");
  ndbout_c("c1->print_diff(c2)");
  c1->print_diff(c2);
  ndbout_c("==================");
  ndbout_c("c2->print_diff(c1)");
  c2->print_diff(c1);
  ndbout_c("==================");

  {


    // BUG#47036 Reload of config shows only diff of last changed parameter
    // - check that diff of c1 and c3 shows 2 diffs
    Config* c1_bug47306=
      create_config("[ndbd]", "NoOfReplicas=1",
                    "DataMemory=100M", "IndexMemory=100M",
                    "[ndb_mgmd]", "HostName=localhost",
                    "[mysqld]", NULL);
    CHECK(c1_bug47306);

    ndbout_c("c1->print_diff(c1_bug47306)");
    c1->print_diff(c1_bug47306);

    Properties diff_list;
    unsigned exclude[]= {CFG_SECTION_SYSTEM, 0};
    c1->diff(c1_bug47306, diff_list, exclude);

    // open section for ndbd with NodeId=1
    const Properties* section;
    CHECK(diff_list.get("NodeId=1", &section));

    // Count the number of diffs for ndbd 1
    const char* name;
    int count= 0, found = 0;
    Properties::Iterator prop_it(section);
    while ((name = prop_it.next())){
      if (strcmp(name, "IndexMemory") == 0)
        found++;
      if (strcmp(name, "DataMemory") == 0)
        found++;
      count++;
    }
    CHECK(found == 2 &&
          count == found + 2); // Overhead == 2
    ndbout_c("==================");

    delete c1_bug47306;
  }

  delete c1;
  delete c2;
}


void
print_restart_info(void)
{
  Vector<const char*> initial_node;
  Vector<const char*> system;
  Vector<const char*> initial_system;

  for (int i = 0; i < g_info.m_NoOfParams; i++) {
    const ConfigInfo::ParamInfo & param = g_info.m_ParamInfo[i];
    if ((param._flags & ConfigInfo::CI_RESTART_INITIAL) &&
        (param._flags & ConfigInfo::CI_RESTART_SYSTEM))
      initial_system.push_back(param._fname);
    else if (param._flags & (ConfigInfo::CI_RESTART_SYSTEM))
      system.push_back(param._fname);
    else if (param._flags & (ConfigInfo::CI_RESTART_INITIAL))
      initial_node.push_back(param._fname);
  }

  fprintf(stderr, "*** initial node restart ***\n");
  for (size_t i = 0; i < initial_node.size(); i++) {
    fprintf(stderr, "%s\n", initial_node[i]);
  }
  fprintf(stderr, "\n");

  fprintf(stderr, "*** system restart ***\n");
  for (size_t i = 0; i < system.size(); i++) {
    fprintf(stderr, "%s\n", system[i]);
  }
  fprintf(stderr, "\n");

  fprintf(stderr, "*** initial system restart ***\n");
  for (size_t i = 0; i < initial_system.size(); i++) {
    fprintf(stderr, "%s\n", initial_system[i]);
  }
  fprintf(stderr, "\n");
}


static void
checksum_config(void)
{
  Config* c1=
    create_config("[ndbd]", "NoOfReplicas=1",
                  "[ndb_mgmd]", "HostName=localhost",
                  "[mysqld]", NULL);
  CHECK(c1);
  Config* c2=
    create_config("[ndbd]", "NoOfReplicas=1",
                  "[ndb_mgmd]", "HostName=localhost",
                  "[mysqld]", "[mysqld]", NULL);
  CHECK(c2);

  ndbout_c("== checksum tests ==");
  Uint32 c1_check = c1->checksum();
  Uint32 c2_check = c2->checksum();
  ndbout_c("c1->checksum(): 0x%x", c1_check);
  ndbout_c("c2->checksum(): 0x%x", c2_check);
   // Different config should not have same checksum
  CHECK(c1_check != c2_check);

  // Same config should have same checksum
  CHECK(c1_check == c1->checksum());

  // Copied config should have same checksum
  Config c1_copy(c1);
  CHECK(c1_check == c1_copy.checksum());

  ndbout_c("==================");

  delete c1;
  delete c2;
}

static void
test_param_values(void)
{
  struct test {
    const char* param;
    bool result;
  } tests [] = {
    // CI_ENUM
    { "Arbitration=Disabled", true },
    { "Arbitration=Invalid", false },
    { "Arbitration=", false },
    // CI_BITMASK
    { "LockExecuteThreadToCPU=0", true },
    { "LockExecuteThreadToCPU=1", true },
    { "LockExecuteThreadToCPU=65535", true },
    { "LockExecuteThreadToCPU=0-65535", true },
    { "LockExecuteThreadToCPU=0-1,65534-65535", true },
    { "LockExecuteThreadToCPU=17-256", true },
    { "LockExecuteThreadToCPU=1-2,36-37,17-256,11-12,1-2", true },
    { "LockExecuteThreadToCPU=", false }, // Zero size value not allowed
    { "LockExecuteThreadToCPU=1-", false },
    { "LockExecuteThreadToCPU=1--", false },
    { "LockExecuteThreadToCPU=1-2,34-", false },
    { "LockExecuteThreadToCPU=x", false },
    { "LockExecuteThreadToCPU=x-1", false },
    { "LockExecuteThreadToCPU=x-x", false },
    { 0, false }
  };

  for (struct test* t = tests; t->param; t++)
  {
    ndbout_c("testing %s", t->param);
    {
      const Config* c =
        create_config("[ndbd]", "NoOfReplicas=1",
                      t->param,
                      "[ndb_mgmd]", "HostName=localhost",
                      "[mysqld]", NULL);
      if (t->result)
      {
        CHECK(c);
        delete c;
      }
      else
      {
        CHECK(c == NULL);
      }
    }
    {
      const Config* c =
        create_mycnf("[cluster_config]",
                     "ndb_mgmd=localhost",
                     "ndbd=localhost,localhost",
                     "ndbapi=localhost",
                     "NoOfReplicas=1",
                     t->param,
                     NULL);
      if (t->result)
      {
        CHECK(c);
        delete c;
      }
      else
      {
        CHECK(c == NULL);
      }
    }
  }

}

static void
test_hostname_mycnf(void)
{
  // Check the special rule for my.cnf that says
  // the two hostname specs must match
  {
    // Valid config, ndbd=localhost, matches HostName=localhost
    const Config* c =
      create_mycnf("[cluster_config]",
                   "ndb_mgmd=localhost",
                   "ndbd=localhost,localhost",
                   "ndbapi=localhost",
                   "NoOfReplicas=1",
                   "[cluster_config.ndbd.1]",
                   "HostName=localhost",
                   NULL);
    CHECK(c);
    delete c;
  }

  {
    // Invalid config, ndbd=localhost, does not match HostName=host1
    const Config* c =
      create_mycnf("[cluster_config]",
                   "ndb_mgmd=localhost",
                   "ndbd=localhost,localhost",
                   "ndbapi=localhost",
                   "NoOfReplicas=1",
                   "[cluster_config.ndbd.1]",
                   "HostName=host1",
                   NULL);
    CHECK(c == NULL);
  }
}

#include <NdbTap.hpp>

#include <EventLogger.hpp>
extern EventLogger* g_eventLogger;

TAPTEST(MgmConfig)
{
  ndb_init();
  g_eventLogger->createConsoleHandler();
  diff_config();
  CHECK(check_params());
  checksum_config();
  test_param_values();
  test_hostname_mycnf();
#if 0
  print_restart_info();
#endif
  ndb_end(0);
  return 1; // OK
}
