/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */


#include <ndb_global.h>
#include <getarg.h>
#include <BaseString.hpp>
#include <Parser.hpp>
#include <NdbOut.hpp>
#include <Properties.hpp>
#include <NdbAutoPtr.hpp>

#include "run-test.hpp"
#include <SysLogHandler.hpp>
#include <FileLogHandler.hpp>

#include <mgmapi.h>
#include "CpcClient.hpp"

/** Global variables */
static const char progname[] = "ndb_atrt";
static const char * g_gather_progname = "atrt-gather-result.sh";
static const char * g_analyze_progname = "atrt-analyze-result.sh";
static const char * g_clear_progname = "atrt-clear-result.sh";
static const char * g_setup_progname = "atrt-setup.sh";

static const char * g_setup_path = 0;
static const char * g_process_config_filename = "d.txt";
static const char * g_log_filename = 0;
static const char * g_test_case_filename = 0;
static const char * g_report_filename = 0;

static const char * g_default_user = 0;
static const char * g_default_base_dir = 0;
static int          g_default_base_port = 0;
static int          g_mysqld_use_base = 1;

static int g_report = 0;
static int g_verbosity = 0;
static FILE * g_report_file = 0;
static FILE * g_test_case_file = stdin;

Logger g_logger;
atrt_config g_config;

static int g_mode_bench = 0;
static int g_mode_regression = 0;
static int g_mode_interactive = 0;
static int g_mode = 0;

static
struct getargs args[] = {
  { "process-config", 0, arg_string, &g_process_config_filename, 0, 0 },
  { "setup-path", 0, arg_string, &g_setup_path, 0, 0 },
  { 0, 'v', arg_counter, &g_verbosity, 0, 0 },
  { "log-file", 0, arg_string, &g_log_filename, 0, 0 },
  { "testcase-file", 'f', arg_string, &g_test_case_filename, 0, 0 },
  { 0, 'R', arg_flag, &g_report, 0, 0 },
  { "report-file", 0, arg_string, &g_report_filename, 0, 0 },
  { "interactive", 'i', arg_flag, &g_mode_interactive, 0, 0 },
  { "regression", 'r', arg_flag, &g_mode_regression, 0, 0 },
  { "bench", 'b', arg_flag, &g_mode_bench, 0, 0 },
};

const int arg_count = 10;

int
main(int argc, const char ** argv){
  ndb_init();
  
  bool restart = true;
  int lineno = 1;
  int test_no = 1; 

  const int p_ndb     = atrt_process::NDB_MGM | atrt_process::NDB_DB;
  const int p_servers = atrt_process::MYSQL_SERVER | atrt_process::NDB_REP;
  const int p_clients = atrt_process::MYSQL_CLIENT | atrt_process::NDB_API;
  
  g_logger.setCategory(progname);
  g_logger.enable(Logger::LL_ALL);
  g_logger.createConsoleHandler();
  
  if(!parse_args(argc, argv))
    goto end;

  g_logger.info("Starting...");
  if(!setup_config(g_config))
    goto end;
  
  g_logger.info("Connecting to hosts");
  if(!connect_hosts(g_config))
    goto end;

  if(!setup_hosts(g_config))
    goto end;

  /**
   * Main loop
   */
  while(!feof(g_test_case_file)){
    /**
     * Do we need to restart ndb
     */
    if(restart){
      g_logger.info("(Re)starting ndb processes");
      if(!stop_processes(g_config, atrt_process::NDB_MGM))
	goto end;

      if(!stop_processes(g_config, atrt_process::NDB_DB))
	goto end;

      if(!start_processes(g_config, atrt_process::NDB_MGM))
	goto end;
      
      if(!connect_ndb_mgm(g_config)){
	goto end;
      }
      
      if(!start_processes(g_config, atrt_process::NDB_DB))
	goto end;
      
      if(!wait_ndb(g_config, NDB_MGM_NODE_STATUS_NOT_STARTED))
        goto end;
      
      for(Uint32 i = 0; i<3; i++)      
        if(wait_ndb(g_config, NDB_MGM_NODE_STATUS_STARTED))
	  goto started;
      
      goto end;
      
    started:
      g_logger.info("Ndb start completed");
    }
    
    const int start_line = lineno;
    atrt_testcase test_case;
    if(!read_test_case(g_test_case_file, test_case, lineno))
      goto end;
    
    g_logger.info("#%d - %s %s", 
		  test_no,
		  test_case.m_command.c_str(), test_case.m_args.c_str());
    
    // Assign processes to programs
    if(!setup_test_case(g_config, test_case))
      goto end;
    
    if(!start_processes(g_config, p_servers))
      goto end;
    
    if(!start_processes(g_config, p_clients))
      goto end;

    int result = 0;
    
    const time_t start = time(0);
    time_t now = start;
    do {
      if(!update_status(g_config, atrt_process::ALL))
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
      sleep(1);
    } while(true);
    
    const time_t elapsed = time(0) - start;
   
    if(!stop_processes(g_config, p_clients))
      goto end;
    
    if(!stop_processes(g_config, p_servers))
      goto end;

    if(!gather_result(g_config, &result))
      goto end;
    
    g_logger.info("#%d %s(%d)", 
		  test_no, 
		  (result == 0 ? "OK" : "FAILED"), result);

    if(g_report_file != 0){
      fprintf(g_report_file, "%s %s ; %d ; %d ; %ld\n",
	      test_case.m_command.c_str(),
	      test_case.m_args.c_str(),
	      test_no, result, elapsed);
      fflush(g_report_file);
    }    

    if(g_mode_bench || (g_mode_regression && result)){
      BaseString tmp;
      tmp.assfmt("result.%d", test_no);
      if(rename("result", tmp.c_str()) != 0){
	g_logger.critical("Failed to rename %s as %s",
			  "result", tmp.c_str());
	goto end;
      }
    }

    if(g_mode_interactive && result){
      g_logger.info
	("Encountered failed test in interactive mode - terminating");
      break;
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

  stop_processes(g_config, atrt_process::ALL);
  return 0;
}

bool
parse_args(int argc, const char** argv){
  int optind = 0;
  if(getarg(args, arg_count, argc, argv, &optind)) {
    arg_printusage(args, arg_count, progname, "");
    return false;
  }

  if(g_log_filename != 0){
    g_logger.removeConsoleHandler();
    g_logger.addHandler(new FileLogHandler(g_log_filename));
  }

  {
    int tmp = Logger::LL_WARNING - g_verbosity;
    tmp = (tmp < Logger::LL_DEBUG ? Logger::LL_DEBUG : tmp);
    g_logger.disable(Logger::LL_ALL);
    g_logger.enable((Logger::LoggerLevel)tmp, Logger::LL_ALERT);
  }



  if(!g_process_config_filename){
    g_logger.critical("Process config not specified!");
    return false;
  }
  
  if(!g_setup_path){
    char buf[1024];
    if(getcwd(buf, sizeof(buf))){
      g_setup_path = strdup(buf);
      g_logger.info("Setup path not specified, using %s", buf);
    } else {
      g_logger.critical("Setup path not specified!\n");
      return false;
    }
  }
  
  if(g_report & !g_report_filename){
    g_report_filename = "report.txt";
  }

  if(g_report_filename){
    g_report_file = fopen(g_report_filename, "w");
    if(g_report_file == 0){
      g_logger.critical("Unable to create report file: %s", g_report_filename);
      return false;
    }
  }

  if(g_test_case_filename){
    g_test_case_file = fopen(g_test_case_filename, "r");
    if(g_test_case_file == 0){
      g_logger.critical("Unable to open file: %s", g_test_case_filename);
      return false;
    }
  }
  
  int sum = g_mode_interactive + g_mode_regression + g_mode_bench;
  if(sum == 0){
    g_mode_interactive = 1;
  }

  if(sum > 1){
    g_logger.critical
      ("Only one of bench/regression/interactive can be specified");
    return false;
  }
  
  g_default_user = strdup(getenv("LOGNAME"));

  return true;
}


static
atrt_host *
find(const BaseString& host, Vector<atrt_host> & hosts){
  for(size_t i = 0; i<hosts.size(); i++){
    if(hosts[i].m_hostname == host){
      return &hosts[i];
    }
  }
  return 0;
}

bool
setup_config(atrt_config& config){

  FILE * f = fopen(g_process_config_filename, "r");
  if(!f){
    g_logger.critical("Failed to open process config file: %s",
		      g_process_config_filename);
    return false;
  }
  bool result = true;

  int lineno = 0;
  char buf[2048];
  BaseString connect_string;
  int mysql_port_offset = 0;
  while(fgets(buf, 2048, f)){
    lineno++;

    BaseString tmp(buf);
    tmp.trim(" \t\n\r");

    if(tmp.length() == 0 || tmp == "" || tmp.c_str()[0] == '#')
      continue;

    Vector<BaseString> split1;
    if(tmp.split(split1, ":", 2) != 2){
      g_logger.warning("Invalid line %d in %s - ignoring",
		       lineno, g_process_config_filename);
      continue;
    }

    if(split1[0].trim() == "basedir"){
      g_default_base_dir = strdup(split1[1].trim().c_str());
      continue;
    }

    if(split1[0].trim() == "baseport"){
      g_default_base_port = atoi(split1[1].trim().c_str());
      continue;
    }

    if(split1[0].trim() == "user"){
      g_default_user = strdup(split1[1].trim().c_str());
      continue;
    }

    if(split1[0].trim() == "mysqld-use-base" && split1[1].trim() == "no"){
      g_mysqld_use_base = 0;
      continue;
    }

    Vector<BaseString> hosts;
    if(split1[1].trim().split(hosts) <= 0){
      g_logger.warning("Invalid line %d in %s - ignoring", 
		       lineno, g_process_config_filename);
    }

    // 1 - Check hosts
    for(size_t i = 0; i<hosts.size(); i++){
      Vector<BaseString> tmp;
      hosts[i].split(tmp, ":");
      BaseString hostname = tmp[0].trim();
      BaseString base_dir;
      if(tmp.size() >= 2)
        base_dir = tmp[1];
      else if(g_default_base_dir == 0){
	g_logger.critical("Basedir not specified...");
        return false;
      }

      atrt_host * host_ptr;
      if((host_ptr = find(hostname, config.m_hosts)) == 0){
	atrt_host host;
	host.m_index = config.m_hosts.size();
	host.m_cpcd = new SimpleCpcClient(hostname.c_str(), 1234);
	host.m_base_dir = (base_dir.empty() ? g_default_base_dir : base_dir);
	host.m_user = g_default_user;
	host.m_hostname = hostname.c_str();
	config.m_hosts.push_back(host);
      } else {
        if(!base_dir.empty() && (base_dir == host_ptr->m_base_dir)){
          g_logger.critical("Inconsistent base dir definition for host %s"
                            ", \"%s\" != \"%s\"", hostname.c_str(), 
                            base_dir.c_str(), host_ptr->m_base_dir.c_str());
          return false;
        }
      }
    }
    
    for(size_t i = 0; i<hosts.size(); i++){
      BaseString & tmp = hosts[i];
      atrt_host * host = find(tmp, config.m_hosts);
      BaseString & dir = host->m_base_dir;

      const int index = config.m_processes.size() + 1;

      atrt_process proc;
      proc.m_index = index;
      proc.m_host = host;
      proc.m_proc.m_id = -1;
      proc.m_proc.m_type = "temporary";
      proc.m_proc.m_owner = "atrt";  
      proc.m_proc.m_group = "group";    
      proc.m_proc.m_cwd.assign(dir).append("/run/");
      proc.m_proc.m_stdout = "log.out";
      proc.m_proc.m_stderr = "2>&1";
      proc.m_proc.m_runas = proc.m_host->m_user;
      proc.m_proc.m_ulimit = "c:unlimited";
      proc.m_proc.m_env.assfmt("MYSQL_BASE_DIR=%s", dir.c_str());
      proc.m_hostname = proc.m_host->m_hostname;
      proc.m_ndb_mgm_port = g_default_base_port;
      if(split1[0] == "mgm"){
	proc.m_type = atrt_process::NDB_MGM;
	proc.m_proc.m_name.assfmt("%d-%s", index, "ndb_mgmd");
	proc.m_proc.m_path.assign(dir).append("/libexec/ndb_mgmd");
	proc.m_proc.m_args = "--nodaemon -f config.ini";
	proc.m_proc.m_cwd.appfmt("%d.ndb_mgmd", index);
	connect_string.appfmt("host=%s:%d;", 
			      proc.m_hostname.c_str(), proc.m_ndb_mgm_port);
      } else if(split1[0] == "ndb"){
	proc.m_type = atrt_process::NDB_DB;
	proc.m_proc.m_name.assfmt("%d-%s", index, "ndbd");
	proc.m_proc.m_path.assign(dir).append("/libexec/ndbd");
	proc.m_proc.m_args = "--initial --nodaemon -n";
	proc.m_proc.m_cwd.appfmt("%d.ndbd", index);
      } else if(split1[0] == "mysqld"){
	proc.m_type = atrt_process::MYSQL_SERVER;
	proc.m_proc.m_name.assfmt("%d-%s", index, "mysqld");
	proc.m_proc.m_path.assign(dir).append("/libexec/mysqld");
	proc.m_proc.m_args = "--core-file --ndbcluster";
	proc.m_proc.m_cwd.appfmt("%d.mysqld", index);
	if(mysql_port_offset > 0 || g_mysqld_use_base){
	  // setup mysql specific stuff
	  const char * basedir = proc.m_proc.m_cwd.c_str();
	  proc.m_proc.m_args.appfmt("--datadir=%s", basedir);
	  proc.m_proc.m_args.appfmt("--pid-file=%s/mysql.pid", basedir);
	  proc.m_proc.m_args.appfmt("--socket=%s/mysql.sock", basedir);
	  proc.m_proc.m_args.appfmt("--port=%d", 
				    g_default_base_port-(++mysql_port_offset));
	}
      } else if(split1[0] == "api"){
	proc.m_type = atrt_process::NDB_API;
	proc.m_proc.m_name.assfmt("%d-%s", index, "ndb_api");
	proc.m_proc.m_path = "";
	proc.m_proc.m_args = "";
	proc.m_proc.m_cwd.appfmt("%d.ndb_api", index);
      } else {
	g_logger.critical("%s:%d: Unhandled process type: %s",
			  g_process_config_filename, lineno,
			  split1[0].c_str());
	result = false;
	goto end;
      }
      config.m_processes.push_back(proc);
    }
  }

  // Setup connect string
  for(size_t i = 0; i<config.m_processes.size(); i++){
    config.m_processes[i].m_proc.m_env.appfmt(" NDB_CONNECTSTRING=%s", 
					      connect_string.c_str());
  }
  
 end:
  fclose(f);
  return result;
}

bool
connect_hosts(atrt_config& config){
  for(size_t i = 0; i<config.m_hosts.size(); i++){
    if(config.m_hosts[i].m_cpcd->connect() != 0){
      g_logger.error("Unable to connect to cpc %s:%d",
		     config.m_hosts[i].m_cpcd->getHost(),
		     config.m_hosts[i].m_cpcd->getPort());
      return false;
    }
    g_logger.debug("Connected to %s:%d",
		   config.m_hosts[i].m_cpcd->getHost(),
		   config.m_hosts[i].m_cpcd->getPort());
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
  BaseString tmp = proc.m_hostname;
  tmp.appfmt(":%d", proc.m_ndb_mgm_port);
  time_t start = time(0);
  const time_t max_connect_time = 30;
  do {
    if(ndb_mgm_connect(handle, tmp.c_str()) != -1){
      proc.m_ndb_mgm_handle = handle;
      return true;
    }
    sleep(1);
  } while(time(0) < (start + max_connect_time));
  g_logger.critical("Unable to connect to ndb mgm %s", tmp.c_str());
  return false;
}

bool
connect_ndb_mgm(atrt_config& config){
  for(size_t i = 0; i<config.m_processes.size(); i++){
    atrt_process & proc = config.m_processes[i];
    if((proc.m_type & atrt_process::NDB_MGM) != 0){
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


  /**
   * Get mgm handle for cluster
   */
  NdbMgmHandle handle = 0;
  for(size_t i = 0; i<config.m_processes.size(); i++){
    atrt_process & proc = config.m_processes[i];
    if((proc.m_type & atrt_process::NDB_MGM) != 0){
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
    for(int i = 0; i<state->no_of_nodes; i++){
      if(state->node_states[i].node_type == NDB_MGM_NODE_TYPE_NDB){
	const int s = remap(state->node_states[i].node_status);
	min2 = (min2 < s ? min2 : s );
	
	if(s < remap(NDB_MGM_NODE_STATUS_NO_CONTACT) || 
	   s > NDB_MGM_NODE_STATUS_STARTED){
	  g_logger.critical("Strange DB status during start: %d %d", i, min2);
	  return false;
	}

	if(min2 < min){
	  g_logger.critical("wait ndb failed node: %d %d %d %d", 
			    state->node_states[i].node_id, min, min2, goal);
	}
      }
    }
    
    if(min2 < min){
      g_logger.critical("wait ndb failed %d %d %d", min, min2, goal);
      return false;
    }
    
    if(min2 == goal){
      return true;
      break;
    }
    
    min = min2;
    now = time(0);
  }
  
  g_logger.critical("wait ndb timed out %d %d %d", min, min2, goal);
  
  return false;
}

bool
start_process(atrt_process & proc){
  if(proc.m_proc.m_id != -1){
    g_logger.critical("starting already started process: %d", proc.m_index);
    return false;
  }
  
  BaseString path = proc.m_proc.m_cwd.substr(proc.m_host->m_base_dir.length()+BaseString("/run").length());
  
  BaseString tmp = g_setup_progname;
  tmp.appfmt(" %s %s/%s/ %s",
	     proc.m_host->m_hostname.c_str(),
	     g_setup_path,
	     path.c_str(),
	     proc.m_proc.m_cwd.c_str());

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
    atrt_process & proc = config.m_processes[i];
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
    atrt_process & proc = config.m_processes[i];
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
    config.m_hosts[i].m_cpcd->list_processes(m_procs[i], p);
  }

  for(size_t i = 0; i<config.m_processes.size(); i++){
    atrt_process & proc = config.m_processes[i];
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
		       proc.m_hostname.c_str(),
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
    atrt_process & proc = config.m_processes[i]; 
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
  
  return true;
}

bool
setup_test_case(atrt_config& config, const atrt_testcase& tc){
  const int r1 = system(g_clear_progname);
  if(r1 != 0){
    g_logger.critical("Failed to clear result");
    return false;
  }
  
  size_t i = 0;
  for(; i<config.m_processes.size(); i++){
    atrt_process & proc = config.m_processes[i]; 
    if(proc.m_type == atrt_process::NDB_API){
      proc.m_proc.m_path.assfmt("%s/bin/%s", proc.m_host->m_base_dir.c_str(),
				tc.m_command.c_str());
      proc.m_proc.m_args.assign(tc.m_args);
      break;
    }
  }
  for(i++; i<config.m_processes.size(); i++){
    atrt_process & proc = config.m_processes[i]; 
    if(proc.m_type == atrt_process::NDB_API){
      proc.m_proc.m_path.assign("");
      proc.m_proc.m_args.assign("");
    }
  }
  return true;
}

bool
gather_result(atrt_config& config, int * result){
  BaseString tmp = g_gather_progname;
  for(size_t i = 0; i<config.m_processes.size(); i++){
    atrt_process & proc = config.m_processes[i]; 
    if(proc.m_proc.m_path != ""){
      tmp.appfmt(" %s:%s", 
		 proc.m_hostname.c_str(),
		 proc.m_proc.m_cwd.c_str());
    }
  }
  
  const int r1 = system(tmp.c_str());
  if(r1 != 0){
    g_logger.critical("Failed to gather result");
    return false;
  }

  const int r2 = system(g_analyze_progname);

  if(r2 == -1 || r2 == (127 << 8)){
    g_logger.critical("Failed to analyze results");
    return false;
  }
  
  * result = r2 ;
  return true;
}

bool
setup_hosts(atrt_config& config){
  const int r1 = system(g_clear_progname);
  if(r1 != 0){
    g_logger.critical("Failed to clear result");
    return false;
  }

  for(size_t i = 0; i<config.m_hosts.size(); i++){
    BaseString tmp = g_setup_progname;
    tmp.appfmt(" %s %s/ %s/run", 
	       config.m_hosts[i].m_hostname.c_str(),
	       g_setup_path,
	       config.m_hosts[i].m_base_dir.c_str());
    
    const int r1 = system(tmp.c_str());
    if(r1 != 0){
      g_logger.critical("Failed to setup %s",
			config.m_hosts[i].m_hostname.c_str());
      return false;
    }
  }
  return true;
}

template class Vector<Vector<SimpleCpcClient::Process> >;
template class Vector<atrt_host>;
template class Vector<atrt_process>;
