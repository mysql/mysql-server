
/*
   Copyright (c) 2003, 2010, Oracle and/or its affiliates. All rights reserved.

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


#ifdef _WIN32
#define DEFAULT_PREFIX "c:/atrt"
#endif

#include "atrt.hpp"
#include <my_sys.h>
#include <my_getopt.h>

#include <NdbOut.hpp>
#include <NdbAutoPtr.hpp>

#include <SysLogHandler.hpp>
#include <FileLogHandler.hpp>

#include <NdbSleep.h>

#define PATH_SEPARATOR DIR_SEPARATOR

/** Global variables */
static const char progname[] = "ndb_atrt";
static const char * g_gather_progname = "atrt-gather-result.sh";
static const char * g_analyze_progname = "atrt-analyze-result.sh";
static const char * g_setup_progname = "atrt-setup.sh";

static const char * g_log_filename = 0;
static const char * g_test_case_filename = 0;
static const char * g_report_filename = 0;

static int g_do_setup = 0;
static int g_do_deploy = 0;
static int g_do_sshx = 0;
static int g_do_start = 0;
static int g_do_quit = 0;

static int g_help = 0;
static int g_verbosity = 1;
static FILE * g_report_file = 0;
static FILE * g_test_case_file = stdin;
static int g_mode = 0;

Logger g_logger;
atrt_config g_config;
const char * g_user = 0;
int          g_baseport = 10000;
int          g_fqpn = 0;
int          g_fix_nodeid= 0;
int          g_default_ports = 0;
int          g_mt = 0;
int          g_mt_rr = 0;

const char * g_cwd = 0;
const char * g_basedir = 0;
const char * g_my_cnf = 0;
const char * g_prefix = 0;
const char * g_prefix1 = 0;
const char * g_clusters = 0;
BaseString g_replicate;
const char *save_file = 0;
const char *save_group_suffix = 0;
const char * g_dummy;
char * g_env_path = 0;
const char* g_mysqld_host = 0;

static struct my_option g_options[] =
{
  { "help", '?', "Display this help and exit.", 
    (uchar **) &g_help, (uchar **) &g_help,
    0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 }, 
  { "version", 'V', "Output version information and exit.", 0, 0, 0, 
    GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0 }, 
  { "clusters", 256, "Cluster",
    (uchar **) &g_clusters, (uchar **) &g_clusters,
    0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  { "mysqld", 256, "atrt mysqld",
    (uchar **) &g_mysqld_host, (uchar **) &g_mysqld_host,
    0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  { "replicate", 1024, "replicate",
    (uchar **) &g_dummy, (uchar **) &g_dummy,
    0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  { "log-file", 256, "log-file",
    (uchar **) &g_log_filename, (uchar **) &g_log_filename,
    0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  { "testcase-file", 'f', "testcase-file",
    (uchar **) &g_test_case_filename, (uchar **) &g_test_case_filename,
    0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  { "report-file", 'r', "report-file",
    (uchar **) &g_report_filename, (uchar **) &g_report_filename,
    0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  { "basedir", 256, "Base path",
    (uchar **) &g_basedir, (uchar **) &g_basedir,
    0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  { "baseport", 256, "Base port",
    (uchar **) &g_baseport, (uchar **) &g_baseport,
    0, GET_INT, REQUIRED_ARG, g_baseport, 0, 0, 0, 0, 0},
  { "prefix", 256, "mysql install dir",
    (uchar **) &g_prefix, (uchar **) &g_prefix,
    0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  { "prefix1", 256, "mysql install dir 1",
    (uchar **) &g_prefix1, (uchar **) &g_prefix1,
    0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  { "verbose", 'v', "Verbosity",
    (uchar **) &g_verbosity, (uchar **) &g_verbosity,
    0, GET_INT, REQUIRED_ARG, g_verbosity, 0, 0, 0, 0, 0},
  { "configure", 256, "configure",
    (uchar **) &g_do_setup, (uchar **) &g_do_setup,
    0, GET_INT, REQUIRED_ARG, g_do_setup, 0, 0, 0, 0, 0 },
  { "deploy", 256, "deploy",
    (uchar **) &g_do_deploy, (uchar **) &g_do_deploy,
    0, GET_INT, REQUIRED_ARG, g_do_deploy, 0, 0, 0, 0, 0 },
  { "sshx", 256, "sshx",
    (uchar **) &g_do_sshx, (uchar **) &g_do_sshx,
    0, GET_INT, REQUIRED_ARG, g_do_sshx, 0, 0, 0, 0, 0 },
  { "start", 256, "start",
    (uchar **) &g_do_start, (uchar **) &g_do_start,
    0, GET_INT, REQUIRED_ARG, g_do_start, 0, 0, 0, 0, 0 },
  { "fqpn", 256, "Fully qualified path-names ",
    (uchar **) &g_fqpn, (uchar **) &g_fqpn,
    0, GET_INT, REQUIRED_ARG, g_fqpn, 0, 0, 0, 0, 0 },
  { "fix-nodeid", 256, "Fix nodeid for each started process ",
    (uchar **) &g_fix_nodeid, (uchar **) &g_fix_nodeid,
    0, GET_INT, REQUIRED_ARG, g_fqpn, 0, 0, 0, 0, 0 },
  { "default-ports", 256, "Use default ports when possible",
    (uchar **) &g_default_ports, (uchar **) &g_default_ports,
    0, GET_INT, REQUIRED_ARG, g_default_ports, 0, 0, 0, 0, 0 },
  { "mode", 256, "Mode 0=interactive 1=regression 2=bench",
    (uchar **) &g_mode, (uchar **) &g_mode,
    0, GET_INT, REQUIRED_ARG, g_mode, 0, 0, 0, 0, 0 },
  { "quit", 256, "Quit before starting tests",
    (uchar **) &g_do_quit, (uchar **) &g_do_quit,
    0, GET_BOOL, NO_ARG, g_do_quit, 0, 0, 0, 0, 0 },
  { "mt", 256, "Use ndbmtd (0 = never, 1 = round-robin, 2 = only)",
    (uchar **) &g_mt, (uchar **) &g_mt,
    0, GET_INT, REQUIRED_ARG, g_mt, 0, 0, 0, 0, 0 },
  { 0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};

const int p_ndb     = atrt_process::AP_NDB_MGMD | atrt_process::AP_NDBD;
const int p_servers = atrt_process::AP_MYSQLD;
const int p_clients = atrt_process::AP_CLIENT | atrt_process::AP_NDB_API;

int
main(int argc, char ** argv)
{
  ndb_init();

  bool restart = true;
  int lineno = 1;
  int test_no = 1; 
  int return_code = 1;

  g_logger.setCategory(progname);
  g_logger.enable(Logger::LL_ALL);
  g_logger.createConsoleHandler();
  
  if(!parse_args(argc, argv))
  {
    g_logger.critical("Failed to parse arguments");
    goto end;
  }
  
  g_logger.info("Starting...");
  g_config.m_generated = false;
  g_config.m_replication = g_replicate;
  if (!setup_config(g_config, g_mysqld_host))
  {
    g_logger.critical("Failed to setup configuration");
    goto end;
  }

  if (!configure(g_config, g_do_setup))
  {
    g_logger.critical("Failed to configure");
    goto end;
  }
  
  g_logger.info("Setting up directories...");
  if (!setup_directories(g_config, g_do_setup))
  {
    g_logger.critical("Failed to set up directories");
    goto end;
  }

  if (g_do_setup)
  {
    g_logger.info("Setting up files...");
    if (!setup_files(g_config, g_do_setup, g_do_sshx))
    {
      g_logger.critical("Failed to set up files");
      goto end;
    }
  }
  
  if (g_do_deploy)
  {
    if (!deploy(g_do_deploy, g_config))
    {
      g_logger.critical("Failed to deploy");
      goto end;
    }
  }

  if (g_do_quit)
  {
    return_code = 0;
    goto end;
  }

  if(!setup_hosts(g_config))
  {
    g_logger.critical("Failed to setup hosts");
    goto end;
  }

  if (g_do_sshx)
  {
    g_logger.info("Starting xterm-ssh");
    if (!sshx(g_config, g_do_sshx))
    {
      g_logger.critical("Failed to start xterm-ssh");
      goto end;
    }

    g_logger.info("Done...sleeping");
    while(true)
    {
      if (!do_command(g_config))
      {
        g_logger.critical("Failed to do ssh command");
        goto end;
      }

      NdbSleep_SecSleep(1);
    }
    return_code = 0;
    goto end;
  }
 
  g_logger.info("Connecting to hosts...");
  if(!connect_hosts(g_config))
  {
    g_logger.critical("Failed to connect to CPCD on hosts");
    goto end;
  }

#ifndef _WIN32
  if (g_do_start && !g_test_case_filename)
  {
    g_logger.info("Starting server processes: %x", g_do_start);    
    if (!start(g_config, g_do_start))
    {
      g_logger.critical("Failed to start server processes");
      goto end;
    }
    
    if (!setup_db(g_config))
    {
      g_logger.critical("Failed to setup database");
      goto end;
    }

    g_logger.info("Done...sleeping");
    while(true)
    {
      if (!do_command(g_config))
      {
        g_logger.info("Exiting");
        goto end;
      }

      NdbSleep_SecSleep(1);
    }
    return_code = 0;
    goto end;
  }
#endif

  return_code = 0;
  
  /**
   * Main loop
   */
  g_logger.debug("Entering main loop");
  while(!feof(g_test_case_file))
  {
    /**
     * Do we need to restart ndb
     */
    if(restart)
    {
      restart = false;
      g_logger.info("(Re)starting server processes...");

      if(!stop_processes(g_config, ~0))
      {
        g_logger.critical("Failed to stop all processes");
        goto end;
      }
      
      if (!setup_directories(g_config, 2))
      {
        g_logger.critical("Failed to setup directories");
        goto end;
      }
      
      if (!setup_files(g_config, 2, 1))
      {
        g_logger.critical("Failed to setup files");
        goto end;
      }
      
      if(!setup_hosts(g_config))
      {
        g_logger.critical("Failed to setup hosts");
        goto end;
      }
      
      g_logger.debug("Setup complete, starting servers");
      if (!start(g_config, p_ndb | p_servers))
      {
        g_logger.critical("Failed to start server processes");
        g_logger.info("Gathering logs and saving them as test %u", test_no);
        
        int tmp;
        if(!gather_result(g_config, &tmp))
        {
          g_logger.critical("Failed to gather results");
          goto end;
        }
        
        if(g_report_file != 0)
        {
          fprintf(g_report_file, "%s ; %d ; %d ; %d\n",
                  "start servers", test_no, ERR_FAILED_TO_START, 0);
          fflush(g_report_file);
        }

        BaseString resdir;
        resdir.assfmt("result.%d", test_no);
        remove_dir(resdir.c_str(), true);
        
        if(rename("result", resdir.c_str()) != 0)
        {
          g_logger.critical("Failed to rename %s as %s",
                            "result", resdir.c_str());
          goto end;
        }
        goto end;
      }

      if (!setup_db(g_config))
      {
        g_logger.critical("Failed to setup database");
        goto end;
      }
      
      g_logger.info("All servers start completed");
    }
    
    // const int start_line = lineno;
    atrt_testcase test_case;
    if(!read_test_case(g_test_case_file, test_case, lineno))
      goto end;
    
    g_logger.info("#%d - %s %s", 
		  test_no,
		  test_case.m_command.c_str(), test_case.m_args.c_str());
    
    // Assign processes to programs
    if(!setup_test_case(g_config, test_case))
    {
      g_logger.critical("Failed to setup test case");
      goto end;
    }
    
    if(!start_processes(g_config, p_clients))
    {
      g_logger.critical("Failed to start client processes");
      goto end;
    }

    int result = 0;
    
    const time_t start = time(0);
    time_t now = start;
    do 
    {
      if(!update_status(g_config, atrt_process::AP_ALL))
      {
        g_logger.critical("Failed to get updated status for all processes");
        goto end;
      }
      
      if(is_running(g_config, p_ndb) != 2)
      {
	result = ERR_NDB_FAILED;
	break;
      }
      
      if(is_running(g_config, p_servers) != 2)
      {
	result = ERR_SERVERS_FAILED;
	break;
      }

      if(is_running(g_config, p_clients) == 0)
      {
	break;
      }

      if (!do_command(g_config))
      {
        result = ERR_COMMAND_FAILED;
	break;
      }

      now = time(0);
      if(now  > (start + test_case.m_max_time))
      {
        g_logger.debug("Timed out");
	result = ERR_MAX_TIME_ELAPSED;
        g_logger.info("Timeout '%s' after %ld seconds", test_case.m_name.c_str(), test_case.m_max_time);
	break;
      }
      NdbSleep_SecSleep(1);
    } while(true);
    
    const time_t elapsed = time(0) - start;
   
    if(!stop_processes(g_config, p_clients))
    {
      g_logger.critical("Failed to stop client processes");
      goto end;
    }
    
    int tmp, *rp = result ? &tmp : &result;
    if(!gather_result(g_config, rp))
    {
      g_logger.critical("Failed to gather result after test run");
      goto end;
    }
    
    g_logger.info("#%d %s(%d)", 
		  test_no, 
		  (result == 0 ? "OK" : "FAILED"), result);

    if(g_report_file != 0)
    {
      fprintf(g_report_file, "%s ; %d ; %d ; %ld\n",
	      test_case.m_name.c_str(), test_no, result, elapsed);
      fflush(g_report_file);
    }    

    if(g_mode == 0 && result)
    {
      g_logger.info
	("Encountered failed test in interactive mode - terminating");
      break;
    }
    
    BaseString resdir;
    resdir.assfmt("result.%d", test_no);
    remove_dir(resdir.c_str(), true);
    
    if(test_case.m_report || g_mode == 2 || (g_mode && result))
    {
      if(rename("result", resdir.c_str()) != 0)
      {
	g_logger.critical("Failed to rename %s as %s",
			  "result", resdir.c_str());
	goto end;
      }
    }
    else
    {
      remove_dir("result", true);
    }
   
    if (reset_config(g_config))
    {
      restart = true;
    }
    
    if(result != 0)
    {
      restart = true;
    }
    test_no++;
  }
  
 end:
  if(g_report_file != 0){
    fclose(g_report_file);
    g_report_file = 0;
  }

  if(g_test_case_file != 0 && g_test_case_file != stdin){
    fclose(g_test_case_file);
    g_test_case_file = 0;
  }

  g_logger.info("Stopping all processes, result: %d", return_code);
  stop_processes(g_config, atrt_process::AP_ALL);
  return return_code;
}

extern "C"
my_bool 
get_one_option(int arg, const struct my_option * opt, char * value)
{
  if (arg == 1024)
  {
    if (g_replicate.length())
      g_replicate.append(";");
    g_replicate.append(value);
    return 0;
  }
  return 0;
}

bool
parse_args(int argc, char** argv)
{
  char buf[2048];
  if (getcwd(buf, sizeof(buf)) == 0)
  {
    g_logger.error("Unable to get current working directory");
    return false;
  }
  g_cwd = strdup(buf);
  
  struct stat sbuf;
  BaseString mycnf;
  mycnf.append(g_cwd);
  mycnf.append(DIR_SEPARATOR);

  if (argc > 1 && lstat(argv[argc-1], &sbuf) == 0)
  {
    mycnf.append(argv[argc-1]);
  }
  else
  {
    mycnf.append("my.cnf");
    if (lstat(mycnf.c_str(), &sbuf) != 0)
    {
      g_logger.error("Could not find out which config file to use! "
                     "Pass it as last argument to atrt: 'atrt <config file>' "
                     "(default: '%s')", mycnf.c_str());
      return false;
    }
  }

  to_fwd_slashes((char*)g_cwd);

  g_logger.info("Bootstrapping using %s", mycnf.c_str());
  
  const char *groups[] = { "atrt", 0 };
  int ret = load_defaults(mycnf.c_str(), groups, &argc, &argv);

  if (ret)
  {
    g_logger.error("Failed to load defaults, returned (%d)",ret);
    return false;
  }
  
  save_file = my_defaults_file;
  save_group_suffix = my_defaults_group_suffix;

  if (my_defaults_extra_file)
  {
    g_logger.error("--defaults-extra-file(%s) is not supported...",
		   my_defaults_extra_file);
    return false;
  }

  ret =  handle_options(&argc, &argv, g_options, get_one_option);
  if (ret)
  {
    g_logger.error("handle_options failed, ret: %d, argc: %d, *argv: '%s'", 
                    ret, argc, *argv);
    return false;
  }

  if (argc >= 2)
  {
    const char * arg = argv[argc-2];
    while(* arg)
    {
      switch(* arg){
      case 'c':
	g_do_setup = (g_do_setup == 0) ? 1 : g_do_setup;
	break;
      case 'C':
	g_do_setup = 2;
	break;
      case 'd':
	g_do_deploy = 3;
	break;
      case 'D':
        g_do_deploy = 2; // only binaries
        break;
      case 'x':
	g_do_sshx = atrt_process::AP_CLIENT | atrt_process::AP_NDB_API;
	break;
      case 'X':
	g_do_sshx = atrt_process::AP_ALL;
	break;
      case 's':
	g_do_start = p_ndb;
	break;
      case 'S':
	g_do_start = p_ndb | p_servers;
	break;
      case 'f':
	g_fqpn = 1;
	break;
      case 'z':
	g_fix_nodeid = 1;
	break;
      case 'q':
	g_do_quit = 1;
	break;
      default:
	g_logger.error("Unknown switch '%c'", *arg);
	return false;
      }
      arg++;
    }
  }

  if(g_log_filename != 0)
  {
    g_logger.removeConsoleHandler();
    g_logger.addHandler(new FileLogHandler(g_log_filename));
  }
  
  {
    int tmp = Logger::LL_WARNING - g_verbosity;
    tmp = (tmp < Logger::LL_DEBUG ? Logger::LL_DEBUG : tmp);
    g_logger.disable(Logger::LL_ALL);
    g_logger.enable(Logger::LL_ON);
    g_logger.enable((Logger::LoggerLevel)tmp, Logger::LL_ALERT);
  }
  
  if(!g_basedir)
  {
    g_basedir = g_cwd;
    g_logger.info("basedir not specified, using %s", g_basedir);
  }
  else
  {
    g_logger.info("basedir, %s", g_basedir);
  }

  if (!g_prefix)
  {
    g_prefix = DEFAULT_PREFIX;
  }

  /**
   * Add path to atrt-*.sh
   */
  {
    BaseString tmp;
    const char* env = getenv("PATH");
    if (env && strlen(env))
    {
      tmp.assfmt("PATH=%s:%s/mysql-test/ndb",
		 env, g_prefix);
    }
    else
    {
      tmp.assfmt("PATH=%s/mysql-test/ndb", g_prefix);
    }
    to_native(tmp);
    g_env_path = strdup(tmp.c_str());
    putenv(g_env_path);
  }
  
  if (g_help)
  {
    my_print_help(g_options);
    my_print_variables(g_options);
    return 0;
  }

  if(g_test_case_filename)
  {
    g_test_case_file = fopen(g_test_case_filename, "r");
    if(g_test_case_file == 0)
    {
      g_logger.critical("Unable to open file: %s", g_test_case_filename);
      return false;
    }
    if (g_do_setup == 0)
      g_do_setup = 2;
    
    if (g_do_start == 0)
      g_do_start = p_ndb | p_servers;
    
    if (g_mode == 0)
      g_mode = 1;

    if (g_do_sshx)
    {
      g_logger.critical("ssx specified...not possible with testfile");
      return false;
    }
  }
  else {
    g_logger.info("No test case file given with -f <test file>, "
                  "running in interactive mode from stdin");
  }
  
  if (g_do_setup == 0)
  {
    BaseString tmp;
    tmp.append(g_basedir);
    tmp.append(PATH_SEPARATOR);
    tmp.append("my.cnf");
    if (lstat(tmp.c_str(), &sbuf) != 0)
    {
      g_logger.error("Could not find a my.cnf file in the basedir '%s', "
                     "you probably need to configure it with "
                     "'atrt --configure=1 <config_file>'", g_basedir);
      return false;
    }

    if (!S_ISREG(sbuf.st_mode))
    {
      g_logger.error("%s is not a regular file", tmp.c_str());
      return false;
    }

    g_my_cnf = strdup(tmp.c_str());
    g_logger.info("Using %s", tmp.c_str());
  }
  else
  {
    g_my_cnf = strdup(mycnf.c_str());
  }
  
  g_logger.info("Using --prefix=\"%s\"", g_prefix);

  if (g_prefix1)
  {
    g_logger.info("Using --prefix1=\"%s\"", g_prefix1);
  }


  
  if(g_report_filename)
  {
    g_report_file = fopen(g_report_filename, "w");
    if(g_report_file == 0)
    {
      g_logger.critical("Unable to create report file: %s", g_report_filename);
      return false;
    }
  }
  
  if (g_clusters == 0)
  {
    g_logger.critical("No clusters specified");
    return false;
  }
  
  /* Read username from environment, default to sakila */
  g_user = strdup(getenv("LOGNAME"));
  if (g_user == 0)
  {
    g_user = "sakila";
    g_logger.info("No default user specified, will use 'sakila'.");
    g_logger.info("Please set LOGNAME environment variable for other username");
  }
  return true;
}

bool
connect_hosts(atrt_config& config){
  for(size_t i = 0; i<config.m_hosts.size(); i++){
    if(config.m_hosts[i]->m_cpcd->connect() != 0){
      g_logger.error("Unable to connect to cpc %s:%d",
		     config.m_hosts[i]->m_cpcd->getHost(),
		     config.m_hosts[i]->m_cpcd->getPort());
      return false;
    }
    g_logger.debug("Connected to %s:%d",
		   config.m_hosts[i]->m_cpcd->getHost(),
		   config.m_hosts[i]->m_cpcd->getPort());
  }
  
  return true;
}

bool
connect_ndb_mgm(atrt_process & proc){
  NdbMgmHandle handle = ndb_mgm_create_handle();
  if(handle == 0){
    g_logger.critical("Unable to create mgm handle");
    return false;
  }
  BaseString tmp = proc.m_host->m_hostname;
  const char * val;
  proc.m_options.m_loaded.get("--PortNumber=", &val);
  tmp.appfmt(":%s", val);

  if (ndb_mgm_set_connectstring(handle,tmp.c_str()))
  {
    g_logger.critical("Unable to create parse connectstring");
    return false;
  }

  if(ndb_mgm_connect(handle, 30, 1, 0) != -1)
  {
    proc.m_ndb_mgm_handle = handle;
    return true;
  }

  g_logger.critical("Unable to connect to ndb mgm %s", tmp.c_str());
  return false;
}

bool
connect_ndb_mgm(atrt_config& config){
  for(size_t i = 0; i<config.m_processes.size(); i++){
    atrt_process & proc = *config.m_processes[i];
    if((proc.m_type & atrt_process::AP_NDB_MGMD) != 0){
      if(!connect_ndb_mgm(proc)){
	return false;
      }
    }
  }
  
  return true;
}

static int remap(int i){
  if(i == NDB_MGM_NODE_STATUS_NO_CONTACT) return NDB_MGM_NODE_STATUS_UNKNOWN;
  if(i == NDB_MGM_NODE_STATUS_UNKNOWN) return NDB_MGM_NODE_STATUS_NO_CONTACT;
  return i;
}

bool
wait_ndb(atrt_config& config, int goal){

  goal = remap(goal);

  size_t cnt = 0;
  for (size_t i = 0; i<config.m_clusters.size(); i++)
  {
    atrt_cluster* cluster = config.m_clusters[i];

    if (strcmp(cluster->m_name.c_str(), ".atrt") == 0)
    {
      /**
       * skip atrt mysql
       */
      cnt++;
      continue;
    }
    
    /**
     * Get mgm handle for cluster
     */
    NdbMgmHandle handle = 0;
    for(size_t j = 0; j<cluster->m_processes.size(); j++){
      atrt_process & proc = *cluster->m_processes[j];
      if((proc.m_type & atrt_process::AP_NDB_MGMD) != 0){
	handle = proc.m_ndb_mgm_handle;
	break;
      }
    }

    if(handle == 0){
      return true;
    }
    
    if(goal == NDB_MGM_NODE_STATUS_STARTED){
      /**
       * 1) wait NOT_STARTED
       * 2) send start
       * 3) wait STARTED
       */
      if(!wait_ndb(config, NDB_MGM_NODE_STATUS_NOT_STARTED))
	return false;
      
      ndb_mgm_start(handle, 0, 0);
    }

    struct ndb_mgm_cluster_state * state;
    
    time_t now = time(0);
    time_t end = now + 360;
    int min = remap(NDB_MGM_NODE_STATUS_NO_CONTACT);
    int min2 = goal;
    
    while(now < end){
      /**
       * 1) retreive current state
       */
      state = 0;
      do {
	state = ndb_mgm_get_status(handle);
	if(state == 0){
	  const int err = ndb_mgm_get_latest_error(handle);
	  g_logger.error("Unable to poll db state: %d %s %s",
			 ndb_mgm_get_latest_error(handle),
			 ndb_mgm_get_latest_error_msg(handle),
			 ndb_mgm_get_latest_error_desc(handle));
	  if(err == NDB_MGM_SERVER_NOT_CONNECTED && connect_ndb_mgm(config)){
	    g_logger.error("Reconnected...");
	    continue;
	  }
	  return false;
	}
      } while(state == 0);
      NdbAutoPtr<void> tmp(state);
      
      min2 = goal;
      for(int j = 0; j<state->no_of_nodes; j++){
	if(state->node_states[j].node_type == NDB_MGM_NODE_TYPE_NDB){
	  const int s = remap(state->node_states[j].node_status);
	  min2 = (min2 < s ? min2 : s );
	  
	  if(s < remap(NDB_MGM_NODE_STATUS_NO_CONTACT) || 
	     s > NDB_MGM_NODE_STATUS_STARTED){
	    g_logger.critical("Strange DB status during start: %d %d", 
			      j, min2);
	    return false;
	  }
	  
	  if(min2 < min){
	    g_logger.critical("wait ndb failed node: %d %d %d %d", 
			      state->node_states[j].node_id, min, min2, goal);
	  }
	}
      }
      
      if(min2 < min){
	g_logger.critical("wait ndb failed %d %d %d", min, min2, goal);
	return false;
      }
      
      if(min2 == goal){
	cnt++;
	goto next;
      }
      
      min = min2;
      now = time(0);
    }
    
    g_logger.critical("wait ndb timed out %d %d %d", min, min2, goal);
    break;

next:
    ;
  }

  return cnt == config.m_clusters.size();
}

bool
start_process(atrt_process & proc){
  if(proc.m_proc.m_id != -1){
    g_logger.critical("starting already started process: %u", 
                      (unsigned)proc.m_index);
    return false;
  }
  
  BaseString tmp = g_setup_progname;
  tmp.appfmt(" %s %s/ %s",
	     proc.m_host->m_hostname.c_str(),
	     proc.m_proc.m_cwd.c_str(),
	     proc.m_proc.m_cwd.c_str());
  
  g_logger.debug("system(%s)", tmp.c_str());
  const int r1 = sh(tmp.c_str());
  if(r1 != 0)
  {
    g_logger.critical("Failed to setup process");
    return false;
  }
  
  {
    Properties reply;
    if(proc.m_host->m_cpcd->define_process(proc.m_proc, reply) != 0){
      BaseString msg;
      reply.get("errormessage", msg);
      g_logger.error("Unable to define process: %s", msg.c_str());      
      return false;
    }
  }
  {
    Properties reply;
    if(proc.m_host->m_cpcd->start_process(proc.m_proc.m_id, reply) != 0){
      BaseString msg;
      reply.get("errormessage", msg);
      g_logger.error("Unable to start process: %s", msg.c_str());
      return false;
    }
  }
  return true;
}

bool
start_processes(atrt_config& config, int types){
  for(size_t i = 0; i<config.m_processes.size(); i++){
    atrt_process & proc = *config.m_processes[i];
    if(IF_WIN(!(proc.m_type & atrt_process::AP_MYSQLD), 1)
       && (types & proc.m_type) != 0 && proc.m_proc.m_path != ""){
      if(!start_process(proc)){
	return false;
      }
    }
  }
  return true;
}

bool
stop_process(atrt_process & proc){
  if(proc.m_proc.m_id == -1){
    return true;
  }

  {
    Properties reply;
    if(proc.m_host->m_cpcd->stop_process(proc.m_proc.m_id, reply) != 0){
      Uint32 status;
      reply.get("status", &status);
      if(status != 4){
	BaseString msg;
	reply.get("errormessage", msg);
	g_logger.error("Unable to stop process: %s(%d)", msg.c_str(), status);
	return false;
      }
    }
  }
  {
    Properties reply;
    if(proc.m_host->m_cpcd->undefine_process(proc.m_proc.m_id, reply) != 0){
      BaseString msg;
      reply.get("errormessage", msg);
      g_logger.error("Unable to undefine process: %s", msg.c_str());      
      return false;
    }
    proc.m_proc.m_id = -1;
  }
  return true;
}

bool
stop_processes(atrt_config& config, int types){
  for(size_t i = 0; i<config.m_processes.size(); i++){
    atrt_process & proc = *config.m_processes[i];
    if((types & proc.m_type) != 0){
      if(!stop_process(proc)){
	return false;
      }
    }
  }
  return true;
}

bool
update_status(atrt_config& config, int){
  
  Vector<Vector<SimpleCpcClient::Process> > m_procs;
  
  Vector<SimpleCpcClient::Process> dummy;
  m_procs.fill(config.m_hosts.size(), dummy);
  for(size_t i = 0; i<config.m_hosts.size(); i++){
    Properties p;
    config.m_hosts[i]->m_cpcd->list_processes(m_procs[i], p);
  }

  for(size_t i = 0; i<config.m_processes.size(); i++){
    atrt_process & proc = *config.m_processes[i];
    if(proc.m_proc.m_id != -1){
      Vector<SimpleCpcClient::Process> &h_procs= m_procs[proc.m_host->m_index];
      bool found = false;
      for(size_t j = 0; j<h_procs.size(); j++){
	if(proc.m_proc.m_id == h_procs[j].m_id){
	  found = true;
	  proc.m_proc.m_status = h_procs[j].m_status;
	  break;
	}
      }
      if(!found){
	g_logger.error("update_status: not found");
	g_logger.error("id: %d host: %s cmd: %s", 
		       proc.m_proc.m_id,
		       proc.m_host->m_hostname.c_str(),
		       proc.m_proc.m_path.c_str());
	for(size_t j = 0; j<h_procs.size(); j++){
	  g_logger.error("found: %d %s", h_procs[j].m_id, 
			 h_procs[j].m_path.c_str());
	}
	return false;
      }
    }
  }
  return true;
}

int
is_running(atrt_config& config, int types){
  int found = 0, running = 0;
  for(size_t i = 0; i<config.m_processes.size(); i++){
    atrt_process & proc = *config.m_processes[i]; 
    if((types & proc.m_type) != 0){
      found++;
      if(proc.m_proc.m_status == "running")
	running++;
      else {
        if(IF_WIN(proc.m_type & atrt_process::AP_MYSQLD, 0))  {
          running++;
        }
      }
    }
  }
  
  if(found == running)
    return 2;
  if(running == 0)
    return 0;
  return 1;
}


int
insert(const char * pair, Properties & p){
  BaseString tmp(pair);
  
  tmp.trim(" \t\n\r");

  Vector<BaseString> split;
  tmp.split(split, ":=", 2);

  if(split.size() != 2)
    return -1;

  p.put(split[0].trim().c_str(), split[1].trim().c_str()); 

  return 0;
}

bool
read_test_case(FILE * file, atrt_testcase& tc, int& line){

  Properties p;
  int elements = 0;
  char buf[1024];

  while(!feof(file)){
    if (file == stdin)
      printf("atrt> ");
    if(!fgets(buf, 1024, file))
      break;

    line++;
    BaseString tmp = buf;
    
    if(tmp.length() > 0 && tmp.c_str()[0] == '#')
      continue;
    
    if(insert(tmp.c_str(), p) != 0)
      break;
    
    elements++;
  }
  
  if(elements == 0){
    if(file == stdin){
      BaseString tmp(buf); 
      tmp.trim(" \t\n\r");
      Vector<BaseString> split;
      tmp.split(split, " ", 2);
      tc.m_command = split[0];
      if(split.size() == 2)
	tc.m_args = split[1];
      else
	tc.m_args = "";
      tc.m_max_time = 60000;
      return true;
    }
    return false;
  }

  if(!p.get("cmd", tc.m_command)){
    g_logger.critical("Invalid test file: cmd is missing near line: %d", line);
    return false;
  }
  
  if(!p.get("args", tc.m_args))
    tc.m_args = "";

  const char * mt = 0;
  if(!p.get("max-time", &mt))
    tc.m_max_time = 60000;
  else
    tc.m_max_time = atoi(mt);

  if(p.get("type", &mt) && strcmp(mt, "bench") == 0)
    tc.m_report= true;
  else
    tc.m_report= false;

  if(p.get("run-all", &mt) && strcmp(mt, "yes") == 0)
    tc.m_run_all= true;
  else
    tc.m_run_all= false;

  if (!p.get("name", &mt))
  {
    tc.m_name.assfmt("%s %s", 
		     tc.m_command.c_str(),
		     tc.m_args.c_str());
  }
  else
  {
    tc.m_name.assign(mt);
  }
  
  return true;
}

bool
setup_test_case(atrt_config& config, const atrt_testcase& tc){

  if (!remove_dir("result", true))
  {
    g_logger.critical("setup_test_case: Failed to clear result");
    return false;
  }

  size_t i = 0;
  for(; i<config.m_processes.size(); i++)
  {
    atrt_process & proc = *config.m_processes[i]; 
    if(proc.m_type == atrt_process::AP_NDB_API || 
       proc.m_type == atrt_process::AP_CLIENT)
    {
      BaseString cmd;
      if (tc.m_command.c_str()[0] != '/')
      {
        cmd.appfmt("%s/bin/", g_prefix);
      }
      cmd.append(tc.m_command.c_str());

      if (0) // valgrind
      {
        proc.m_proc.m_path = "/usr/bin/valgrind";
        proc.m_proc.m_args.appfmt("%s %s", cmd.c_str(), tc.m_args.c_str());
      }
      else
      {
        proc.m_proc.m_path = cmd;
        proc.m_proc.m_args.assign(tc.m_args);
      }
      if(!tc.m_run_all)
        break;
    }
  }
  for(i++; i<config.m_processes.size(); i++){
    atrt_process & proc = *config.m_processes[i]; 
    if(proc.m_type == atrt_process::AP_NDB_API || 
       proc.m_type == atrt_process::AP_CLIENT)
    {
      proc.m_proc.m_path.assign("");
      proc.m_proc.m_args.assign("");
    }
  }
  return true;
}

bool
gather_result(atrt_config& config, int * result){
  BaseString tmp = g_gather_progname;

  for(size_t i = 0; i<config.m_hosts.size(); i++)
  {
    tmp.appfmt(" %s:%s/*", 
	       config.m_hosts[i]->m_hostname.c_str(),
	       config.m_hosts[i]->m_basedir.c_str());
  }

  g_logger.debug("system(%s)", tmp.c_str());
  const int r1 = sh(tmp.c_str());
  if(r1 != 0)
  {
    g_logger.critical("Failed to gather result!");
    return false;
  }
  
  g_logger.debug("system(%s)", g_analyze_progname);
  const int r2 = sh(g_analyze_progname);
  
  if(r2 == -1 || r2 == (127 << 8))
  {
    g_logger.critical("Failed to analyze results");
    return false;
  }
  
  * result = r2 ;
  return true;
}

bool
setup_hosts(atrt_config& config){
  if (!remove_dir("result", true))
  {
    g_logger.critical("setup_hosts: Failed to clear result");
    return false;
  }

  for(size_t i = 0; i<config.m_hosts.size(); i++){
    BaseString tmp = g_setup_progname;
    tmp.appfmt(" %s %s/ %s/", 
	       config.m_hosts[i]->m_hostname.c_str(),
	       g_basedir,
	       config.m_hosts[i]->m_basedir.c_str());
    
    g_logger.debug("system(%s)", tmp.c_str());
    const int r1 = sh(tmp.c_str());
    if(r1 != 0){
      g_logger.critical("Failed to setup %s",
			config.m_hosts[i]->m_hostname.c_str());
      return false;
    }
  }
  return true;
}

static
bool
do_rsync(const char *dir, const char *dst)
{
  BaseString tmp = g_setup_progname;
  tmp.appfmt(" %s %s/ %s", dst, dir, dir);
  
  g_logger.info("rsyncing %s to %s", dir, dst);
  g_logger.debug("system(%s)", tmp.c_str());
  const int r1 = sh(tmp.c_str());
  if(r1 != 0)
  {
    g_logger.critical("Failed to rsync %s to %s", dir, dst);
    return false;
  }
  
  return true;
}

bool
deploy(int d, atrt_config & config)
{
  for (size_t i = 0; i<config.m_hosts.size(); i++)
  {
    if (d & 1)
    {
      if (!do_rsync(g_basedir, config.m_hosts[i]->m_hostname.c_str()))
        return false;
    }

    if (d & 2)
    {
      if (!do_rsync(g_prefix, config.m_hosts[i]->m_hostname.c_str()))
        return false;
    
      if (g_prefix1 && 
          !do_rsync(g_prefix1, config.m_hosts[i]->m_hostname.c_str()))
        return false;
    }
  }
  
  return true;
}

bool
sshx(atrt_config & config, unsigned mask)
{
  for (size_t i = 0; i<config.m_processes.size(); i++)
  {
    atrt_process & proc = *config.m_processes[i]; 
    
    BaseString tmp;
    const char * type = 0;
    switch(proc.m_type){
    case atrt_process::AP_NDB_MGMD:
      type = (mask & proc.m_type) ? "ndb_mgmd" : 0;
      break;
    case atrt_process::AP_NDBD: 
      type = (mask & proc.m_type) ? "ndbd" : 0;
      break;
    case atrt_process::AP_MYSQLD:
      type = (mask & proc.m_type) ? "mysqld" : 0;
      break;
    case atrt_process::AP_NDB_API:
      type = (mask & proc.m_type) ? "ndbapi" : 0;
      break;
    case atrt_process::AP_CLIENT:
      type = (mask & proc.m_type) ? "client" : 0;
      break;
    default:
      type = "<unknown>";
    }
    
    if (type == 0)
      continue;

#ifdef _WIN32
#define SYS_SSH "bash '-c echo\"%s(%s) on %s\";" \
	        "ssh -t %s sh %s/ssh-login.sh' &"
#else
#define SYS_SSH "xterm -fg black -title \"%s(%s) on %s\"" \
	        " -e 'ssh -t -X %s sh %s/ssh-login.sh' &"
#endif

    tmp.appfmt(SYS_SSH,
	       type,
	       proc.m_cluster->m_name.c_str(),
	       proc.m_host->m_hostname.c_str(),
	       proc.m_host->m_hostname.c_str(),
	       proc.m_proc.m_cwd.c_str());
    
    g_logger.debug("system(%s)", tmp.c_str());
    const int r1 = sh(tmp.c_str());
    if(r1 != 0)
    {
      g_logger.critical("Failed sshx (%s)", 
			tmp.c_str());
      return false;
    }
    NdbSleep_MilliSleep(300); // To prevent xlock problem
  }
  
  return true;
}

bool
start(atrt_config & config, unsigned proc_mask)
{
  if (proc_mask & atrt_process::AP_NDB_MGMD)
    if(!start_processes(g_config, atrt_process::AP_NDB_MGMD))
      return false;

  if (proc_mask & atrt_process::AP_NDBD)
  {
    if(!connect_ndb_mgm(g_config)){
      return false;
    }
    
    if(!start_processes(g_config, atrt_process::AP_NDBD))
      return false;
    
    if(!wait_ndb(g_config, NDB_MGM_NODE_STATUS_NOT_STARTED))
      return false;
    
    for(Uint32 i = 0; i<3; i++)      
      if(wait_ndb(g_config, NDB_MGM_NODE_STATUS_STARTED))
	goto started;
    return false;
  }
  
started:
  if(!start_processes(g_config, p_servers & proc_mask))
    return false;

  return true;
}

bool
reset_config(atrt_config & config)
{
  bool changed = false;
  for(size_t i = 0; i<config.m_processes.size(); i++)
  {
    atrt_process & proc = *config.m_processes[i]; 
    if (proc.m_save.m_saved)
    {
      if (!stop_process(proc))
        return false;
      
      changed = true;
      proc.m_save.m_saved = false;
      proc.m_proc = proc.m_save.m_proc;
      proc.m_proc.m_id = -1;
    }
  }
  return changed;
}

template class Vector<Vector<SimpleCpcClient::Process> >;
template class Vector<atrt_host*>;
template class Vector<atrt_cluster*>;
template class Vector<atrt_process*>;
