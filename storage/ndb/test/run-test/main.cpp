/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */


#include "atrt.hpp"
#include <my_sys.h>
#include <my_getopt.h>

#include <NdbOut.hpp>
#include <NdbAutoPtr.hpp>

#include <SysLogHandler.hpp>
#include <FileLogHandler.hpp>

#include <NdbSleep.h>

#define PATH_SEPARATOR "/"

/** Global variables */
static const char progname[] = "ndb_atrt";
static const char * g_gather_progname = "atrt-gather-result.sh";
static const char * g_analyze_progname = "atrt-analyze-result.sh";
static const char * g_clear_progname = "atrt-clear-result.sh";
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
int          g_default_ports = 0;

const char * g_cwd = 0;
const char * g_basedir = 0;
const char * g_my_cnf = 0;
const char * g_prefix = 0;
const char * g_clusters = 0;
BaseString g_replicate;
const char *save_file = 0;
char *save_extra_file = 0;
const char *save_group_suffix = 0;
const char * g_dummy;
char * g_env_path = 0;

/** Dummy, extern declared in ndb_opts.h */
int g_print_full_config = 0, opt_ndb_shm;
my_bool opt_core;

static struct my_option g_options[] =
{
  { "help", '?', "Display this help and exit.", 
    &g_help, &g_help,
    0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 }, 
  { "version", 'V', "Output version information and exit.", 0, 0, 0, 
    GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0 }, 
  { "clusters", 256, "Cluster",
    &g_clusters, &g_clusters,
    0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  { "replicate", 1024, "replicate",
    &g_dummy, &g_dummy,
    0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  { "log-file", 256, "log-file",
    &g_log_filename, &g_log_filename,
    0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  { "testcase-file", 'f', "testcase-file",
    &g_test_case_filename, &g_test_case_filename,
    0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  { "report-file", 'r', "report-file",
    &g_report_filename, &g_report_filename,
    0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  { "basedir", 256, "Base path",
    &g_basedir, &g_basedir,
    0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  { "baseport", 256, "Base port",
    &g_baseport, &g_baseport,
    0, GET_INT, REQUIRED_ARG, g_baseport, 0, 0, 0, 0, 0},
  { "prefix", 256, "mysql install dir",
    &g_prefix, &g_prefix,
    0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  { "verbose", 'v', "Verbosity",
    &g_verbosity, &g_verbosity,
    0, GET_INT, REQUIRED_ARG, g_verbosity, 0, 0, 0, 0, 0},
  { "configure", 256, "configure",
    &g_do_setup, &g_do_setup,
    0, GET_INT, REQUIRED_ARG, g_do_setup, 0, 0, 0, 0, 0 },
  { "deploy", 256, "deploy",
    &g_do_deploy, &g_do_deploy,
    0, GET_INT, REQUIRED_ARG, g_do_deploy, 0, 0, 0, 0, 0 },
  { "sshx", 256, "sshx",
    &g_do_sshx, &g_do_sshx,
    0, GET_INT, REQUIRED_ARG, g_do_sshx, 0, 0, 0, 0, 0 },
  { "start", 256, "start",
    &g_do_start, &g_do_start,
    0, GET_INT, REQUIRED_ARG, g_do_start, 0, 0, 0, 0, 0 },
  { "fqpn", 256, "Fully qualified path-names ",
    &g_fqpn, &g_fqpn,
    0, GET_INT, REQUIRED_ARG, g_fqpn, 0, 0, 0, 0, 0 },
  { "default-ports", 256, "Use default ports when possible",
    &g_default_ports, &g_default_ports,
    0, GET_INT, REQUIRED_ARG, g_default_ports, 0, 0, 0, 0, 0 },
  { "mode", 256, "Mode 0=interactive 1=regression 2=bench",
    &g_mode, &g_mode,
    0, GET_INT, REQUIRED_ARG, g_mode, 0, 0, 0, 0, 0 },
  { "quit", 256, "Quit before starting tests",
    &g_mode, &g_do_quit,
    0, GET_BOOL, NO_ARG, g_do_quit, 0, 0, 0, 0, 0 },
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
    goto end;
  
  g_logger.info("Starting...");
  g_config.m_generated = false;
  g_config.m_replication = g_replicate;
  if (!setup_config(g_config))
    goto end;

  if (!configure(g_config, g_do_setup))
    goto end;
  
  g_logger.info("Setting up directories");
  if (!setup_directories(g_config, g_do_setup))
    goto end;

  if (g_do_setup)
  {
    g_logger.info("Setting up files");
    if (!setup_files(g_config, g_do_setup, g_do_sshx))
      goto end;
  }
  
  if (g_do_deploy)
  {
    if (!deploy(g_config))
      goto end;
  }

  if (g_do_quit)
  {
    return_code = 0;
    goto end;
  }

  if(!setup_hosts(g_config))
    goto end;

  if (g_do_sshx)
  {
    g_logger.info("Starting xterm-ssh");
    if (!sshx(g_config, g_do_sshx))
      goto end;

    g_logger.info("Done...sleeping");
    while(true)
    {
      NdbSleep_SecSleep(1);
    }
    return_code = 0;
    goto end;
  }
 
  g_logger.info("Connecting to hosts");
  if(!connect_hosts(g_config))
    goto end;

  if (g_do_start && !g_test_case_filename)
  {
    g_logger.info("Starting server processes: %x", g_do_start);    
    if (!start(g_config, g_do_start))
      goto end;
    
    g_logger.info("Done...sleeping");
    while(true)
    {
      NdbSleep_SecSleep(1);
    }
    return_code = 0;
    goto end;
  }

  return_code = 0;
  
  /**
   * Main loop
   */
  while(!feof(g_test_case_file)){
    /**
     * Do we need to restart ndb
     */
    if(restart){
      g_logger.info("(Re)starting server processes processes");
      if(!stop_processes(g_config, ~0))
	goto end;

      if (!setup_directories(g_config, 2))
	goto end;
      
      if (!setup_files(g_config, 2, 1))
	goto end;
      
      if(!setup_hosts(g_config))
        goto end;
      
      if (!start(g_config, p_ndb | p_servers))
	goto end;
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
      goto end;
    
    if(!start_processes(g_config, p_clients))
      goto end;

    int result = 0;
    
    const time_t start = time(0);
    time_t now = start;
    do {
      if(!update_status(g_config, atrt_process::AP_ALL))
	goto end;

      int count = 0;

      if((count = is_running(g_config, p_ndb)) != 2){
	result = ERR_NDB_FAILED;
	break;
      }

      if((count = is_running(g_config, p_servers)) != 2){
	result = ERR_SERVERS_FAILED;
	break;
      }

      if((count = is_running(g_config, p_clients)) == 0){
	break;
      }
      
      now = time(0);
      if(now  > (start + test_case.m_max_time)){
	result = ERR_MAX_TIME_ELAPSED;
	break;
      }
      NdbSleep_SecSleep(1);
    } while(true);
    
    const time_t elapsed = time(0) - start;
   
    if(!stop_processes(g_config, p_clients))
      goto end;
    
    int tmp, *rp = result ? &tmp : &result;
    if(!gather_result(g_config, rp))
      goto end;
    
    g_logger.info("#%d %s(%d)", 
		  test_no, 
		  (result == 0 ? "OK" : "FAILED"), result);

    if(g_report_file != 0){
      fprintf(g_report_file, "%s ; %d ; %d ; %ld\n",
	      test_case.m_name.c_str(), test_no, result, elapsed);
      fflush(g_report_file);
    }    

    if(g_mode == 0 && result){
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
    
    if(result != 0){
      restart = true;
    } else {
      restart = false;
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

  stop_processes(g_config, atrt_process::AP_ALL);
  return return_code;
}

static 
my_bool 
get_one_option(int arg, const struct my_option * opt, char * value)
{
  if (arg == 1024)
  {
    if (g_replicate.length())
      g_replicate.append(";");
    g_replicate.append(value);
    return 1;
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
  if (argc > 1 && lstat(argv[argc-1], &sbuf) == 0)
  {
    mycnf.append(g_cwd);
    mycnf.append(PATH_SEPARATOR);
    mycnf.append(argv[argc-1]);
  }
  else
  {
    mycnf.append(g_cwd);
    mycnf.append(PATH_SEPARATOR);
    mycnf.append("my.cnf");
    if (lstat(mycnf.c_str(), &sbuf) != 0)
    {
      g_logger.error("Unable to stat %s", mycnf.c_str());
      return false;
    }
  }

  g_logger.info("Bootstrapping using %s", mycnf.c_str());
  
  const char *groups[] = { "atrt", 0 };
  int ret = load_defaults(mycnf.c_str(), groups, &argc, &argv);
  
  save_file = my_defaults_file;
  save_extra_file = my_defaults_extra_file;
  save_group_suffix = my_defaults_group_suffix;

  if (save_extra_file)
  {
    g_logger.error("--defaults-extra-file(%s) is not supported...",
		   save_extra_file);
    return false;
  }
  
  if (ret || handle_options(&argc, &argv, g_options, get_one_option))
  {
    g_logger.error("Failed to load defaults/handle_options");
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
	g_do_deploy = 1;
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
  
  if (g_do_setup == 0)
  {
    BaseString tmp;
    tmp.append(g_basedir);
    tmp.append(PATH_SEPARATOR);
    tmp.append("my.cnf");
    if (lstat(tmp.c_str(), &sbuf) != 0)
    {
      g_logger.error("Unable to stat %s", tmp.c_str());
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
  
  g_user = strdup(getenv("LOGNAME"));
  
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
      g_logger.critical("Unable to find mgm handle");
      return false;
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
    g_logger.critical("starting already started process: %d", proc.m_index);
    return false;
  }
  
  BaseString tmp = g_setup_progname;
  tmp.appfmt(" %s %s/ %s",
	     proc.m_host->m_hostname.c_str(),
	     proc.m_proc.m_cwd.c_str(),
	     proc.m_proc.m_cwd.c_str());
  
  g_logger.debug("system(%s)", tmp.c_str());
  const int r1 = system(tmp.c_str());
  if(r1 != 0){
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
    if((types & proc.m_type) != 0 && proc.m_proc.m_path != ""){
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
  g_logger.debug("system(%s)", g_clear_progname);
  const int r1 = system(g_clear_progname);
  if(r1 != 0){
    g_logger.critical("Failed to clear result");
    return false;
  }
  
  size_t i = 0;
  for(; i<config.m_processes.size(); i++)
  {
    atrt_process & proc = *config.m_processes[i]; 
    if(proc.m_type == atrt_process::AP_NDB_API || proc.m_type == atrt_process::AP_CLIENT){
      proc.m_proc.m_path = "";
      if (tc.m_command.c_str()[0] != '/')
      {
	proc.m_proc.m_path.appfmt("%s/bin/", g_prefix);
      }
      proc.m_proc.m_path.append(tc.m_command.c_str());
      proc.m_proc.m_args.assign(tc.m_args);
      if(!tc.m_run_all)
        break;
    }
  }
  for(i++; i<config.m_processes.size(); i++){
    atrt_process & proc = *config.m_processes[i]; 
    if(proc.m_type == atrt_process::AP_NDB_API || proc.m_type == atrt_process::AP_CLIENT){
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
  const int r1 = system(tmp.c_str());
  if(r1 != 0)
  {
    g_logger.critical("Failed to gather result!");
    return false;
  }
  
  g_logger.debug("system(%s)", g_analyze_progname);
  const int r2 = system(g_analyze_progname);
  
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
  g_logger.debug("system(%s)", g_clear_progname);
  const int r1 = system(g_clear_progname);
  if(r1 != 0){
    g_logger.critical("Failed to clear result");
    return false;
  }

  for(size_t i = 0; i<config.m_hosts.size(); i++){
    BaseString tmp = g_setup_progname;
    tmp.appfmt(" %s %s/ %s/", 
	       config.m_hosts[i]->m_hostname.c_str(),
	       g_basedir,
	       config.m_hosts[i]->m_basedir.c_str());
    
    g_logger.debug("system(%s)", tmp.c_str());
    const int r1 = system(tmp.c_str());
    if(r1 != 0){
      g_logger.critical("Failed to setup %s",
			config.m_hosts[i]->m_hostname.c_str());
      return false;
    }
  }
  return true;
}

bool
deploy(atrt_config & config)
{
  for (size_t i = 0; i<config.m_hosts.size(); i++)
  {
    BaseString tmp = g_setup_progname;
    tmp.appfmt(" %s %s/ %s",
	       config.m_hosts[i]->m_hostname.c_str(),
	       g_prefix,
	       g_prefix);
  
    g_logger.info("rsyncing %s to %s", g_prefix,
		  config.m_hosts[i]->m_hostname.c_str());
    g_logger.debug("system(%s)", tmp.c_str());
    const int r1 = system(tmp.c_str());
    if(r1 != 0)
    {
      g_logger.critical("Failed to rsync %s to %s", 
			g_prefix,
			config.m_hosts[i]->m_hostname.c_str());
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
    
    tmp.appfmt("xterm -fg black -title \"%s(%s) on %s\""
	       " -e 'ssh -t -X %s sh %s/ssh-login.sh' &",
	       type,
	       proc.m_cluster->m_name.c_str(),
	       proc.m_host->m_hostname.c_str(),
	       proc.m_host->m_hostname.c_str(),
	       proc.m_proc.m_cwd.c_str());
    
    g_logger.debug("system(%s)", tmp.c_str());
    const int r1 = system(tmp.c_str());
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

void
require(bool x)
{
  if (!x)
    abort();
}

template class Vector<Vector<SimpleCpcClient::Process> >;
template class Vector<atrt_host*>;
template class Vector<atrt_cluster*>;
template class Vector<atrt_process*>;
