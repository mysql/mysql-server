/*
   Copyright (c) 2007, 2018, Oracle and/or its affiliates. All rights reserved.

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
#include <util/ndb_opts.h>
#include <map>
#include <string>
#include <util/BaseString.hpp>
#include <util/File.hpp>
#include <util/NdbOut.hpp>
#include "atrt.hpp"

extern int g_mt;
extern int g_mt_rr;

static atrt_host* find(const char* hostname, Vector<atrt_host*>&);
static bool load_process(atrt_config&, atrt_cluster&, BaseString,
                         atrt_process::Type, unsigned idx,
                         const char* hostname);
static bool load_options(int argc, char** argv, int type, atrt_options&);
bool load_custom_processes(atrt_config& config, atrt_cluster& cluster);
bool load_deployment_options_for_process(atrt_cluster& cluster,
                                         atrt_process& proc);
bool matches_custom_process_option(char* arg, BaseString& proc_name,
                                   BaseString& hosts);
BaseString getProcGroupName(atrt_process::Type type);

enum {
  PO_NDB = atrt_options::AO_NDBCLUSTER,
  PO_REP_SLAVE = 256,
  PO_REP_MASTER = 512,
  PO_REP = (atrt_options::AO_REPLICATION | PO_REP_SLAVE | PO_REP_MASTER)
};

struct proc_option {
  const char* name;
  int type;
  int options;
};

static struct proc_option f_options[] = {
    {"--FileSystemPath=", atrt_process::AP_NDBD, 0},
    {"--PortNumber=", atrt_process::AP_NDB_MGMD, 0},
    {"--datadir=", atrt_process::AP_MYSQLD, 0},
    {"--socket=", atrt_process::AP_MYSQLD | atrt_process::AP_CLIENT, 0},
    {"--port=",
     atrt_process::AP_MYSQLD | atrt_process::AP_CLIENT |
         atrt_process::AP_CUSTOM,
     0},
    {"--host=", atrt_process::AP_CLIENT, 0},
    {"--server-id=", atrt_process::AP_MYSQLD, PO_REP},
    {"--log-bin", atrt_process::AP_MYSQLD, PO_REP_MASTER},
    {"--ndb-connectstring=", atrt_process::AP_MYSQLD | atrt_process::AP_CLUSTER,
     PO_NDB},
    {"--ndbcluster", atrt_process::AP_MYSQLD, PO_NDB},
    {0, 0, 0}};
const char* ndbcs = "--ndb-connectstring=";

bool setup_config(atrt_config& config, const char* atrt_mysqld) {
  config.m_site = g_site;

  BaseString tmp(g_clusters);

  if (atrt_mysqld) {
    tmp.appfmt(",.atrt");
  }
  Vector<BaseString> clusters;
  tmp.split(clusters, ",");

  bool fqpn = clusters.size() > 1 || g_fqpn;

  size_t j;
  for (unsigned i = 0; i < clusters.size(); i++) {
    struct atrt_cluster* cluster = new atrt_cluster;
    config.m_clusters.push_back(cluster);

    cluster->m_name = clusters[i];
    cluster->m_options.m_features = 0;
    if (fqpn) {
      cluster->m_dir.assfmt("cluster%s/", cluster->m_name.c_str());
    } else {
      cluster->m_dir = "";
    }
    cluster->m_next_nodeid = 1;

    int argc = 1;
    const char* argv[] = {"atrt", 0, 0};

    BaseString buf;
    buf.assfmt("--defaults-group-suffix=%s", clusters[i].c_str());
    argv[argc++] = buf.c_str();
    char** tmp = (char**)argv;
    const char* groups[] = {"cluster_config", 0};
    int ret = load_defaults(g_my_cnf, groups, &argc, &tmp);
    if (ret) {
      g_logger.error("Unable to load defaults for cluster: %s",
                     clusters[i].c_str());
      return false;
    }

    struct {
      atrt_process::Type type;
      const char* name;
      const char* value;
    } proc_args[] = {{atrt_process::AP_NDB_MGMD, "--ndb_mgmd=", 0},
                     {atrt_process::AP_NDBD, "--ndbd=", 0},
                     {atrt_process::AP_NDB_API, "--ndbapi=", 0},
                     {atrt_process::AP_NDB_API, "--api=", 0},
                     {atrt_process::AP_MYSQLD, "--mysqld=", 0},
                     {atrt_process::AP_ALL, 0, 0}};

    /**
     * Find all processes...
     */
    for (j = 0; j < (size_t)argc; j++) {
      if (my_getopt_is_args_separator(tmp[j])) /* skip arguments separator */
        continue;
      for (unsigned k = 0; proc_args[k].name; k++) {
        if (!strncmp(tmp[j], proc_args[k].name, strlen(proc_args[k].name))) {
          proc_args[k].value = tmp[j] + strlen(proc_args[k].name);
          break;
        }
      }
    }

    if (strcmp(clusters[i].c_str(), ".atrt") == 0) {
      /**
       * Only use a mysqld...
       */
      proc_args[0].value = 0;
      proc_args[1].value = 0;
      proc_args[2].value = 0;
      proc_args[3].value = 0;
      proc_args[4].value = atrt_mysqld;
    }

    /**
     * Load each process
     */
    BaseString name;
    for (j = 0; proc_args[j].name; j++) {
      if (proc_args[j].value) {
        BaseString tmp(proc_args[j].value);
        Vector<BaseString> list;
        tmp.split(list, ",");
        for (unsigned k = 0; k < list.size(); k++)
          if (!load_process(config, *cluster, name, proc_args[j].type, k + 1,
                            list[k].c_str()))
            return false;
      }
    }

    /**
     * Load custom processes
     */
    if (!load_custom_processes(config, *cluster)) return false;

    {
      /**
       * Load cluster options
       */
      int argc = 1;
      const char* argv[] = {"atrt", 0, 0};
      argv[argc++] = buf.c_str();
      const char* groups[] = {"mysql_cluster", 0};
      char** tmp = (char**)argv;
      ret = load_defaults(g_my_cnf, groups, &argc, &tmp);

      if (ret) {
        g_logger.error("Unable to load defaults for cluster: %s",
                       clusters[i].c_str());
        return false;
      }

      load_options(argc, tmp, atrt_process::AP_CLUSTER, cluster->m_options);
    }
  }
  return true;
}

bool load_custom_processes(atrt_config& config, atrt_cluster& cluster) {
  int argc = 1;
  const char* argv[] = {"atrt", 0, 0};

  BaseString buf;
  buf.assfmt("--defaults-group-suffix=%s", cluster.m_name.c_str());
  argv[argc++] = buf.c_str();
  char** tmp = (char**)argv;
  const char* groups[] = {"cluster_deployment", 0};

  int ret = load_defaults(g_my_cnf, groups, &argc, &tmp);
  if (ret != 0) {
    g_logger.error("Failure to '%s' group for cluster %s", groups[0],
                   cluster.m_name.c_str());
    return false;
  }

  for (int i = 1; i < argc; i++) {
    BaseString proc_name;
    BaseString hosts;
    if (!matches_custom_process_option(tmp[i], proc_name, hosts)) continue;

    Vector<BaseString> host_list;
    hosts.split(host_list, ",");
    for (unsigned int j = 0; j < host_list.size(); j++) {
      bool ok =
          load_process(config, cluster, proc_name, atrt_process::AP_CUSTOM,
                       j + 1, host_list[j].c_str());
      if (!ok) return false;
    }
  }

  return true;
}

bool matches_custom_process_option(char* arg, BaseString& proc_name,
                                   BaseString& hosts) {
  const char* opt_prefix = "--proc:";

  if (strncmp(arg, opt_prefix, strlen(opt_prefix)) != 0) return false;

  arg += strlen(opt_prefix);  // advance prefix

  char* list = strstr(arg, "=");
  if (list == NULL) return false;

  proc_name.assign(arg, list - arg);

  list++;  // advance "="
  hosts.assign(list);

  return true;
}

static atrt_host* find(const char* hostname, Vector<atrt_host*>& hosts) {
  for (unsigned i = 0; i < hosts.size(); i++) {
    if (hosts[i]->m_hostname == hostname) {
      return hosts[i];
    }
  }

  atrt_host* host = new atrt_host;
  host->m_index = hosts.size();
  host->m_cpcd = new SimpleCpcClient(hostname, 1234);
  host->m_basedir = g_basedir;
  host->m_user = g_user;
  host->m_hostname = hostname;
  hosts.push_back(host);
  return host;
}

static char* dirname(const char* path) {
  char* s = strdup(path);
  size_t len = strlen(s);
  for (size_t i = 1; i < len; i++) {
    if (s[len - i] == '/') {
      s[len - i] = 0;
      return s;
    }
  }
  free(s);
  return 0;
}

bool load_deployment_options_for_process(atrt_cluster& cluster,
                                         atrt_process& proc) {
  if (proc.m_name.empty()) {
    g_logger.debug("Skipping deployment_options loading for process type %d",
                   proc.m_type);
    return true;
  }

  BaseString suffix;
  suffix.assfmt("--defaults-group-suffix=%s", cluster.m_name.c_str());

  const char* argv[] = {"atrt", suffix.c_str(), 0};
  int argc = 2;
  char** tmp = (char**)argv;

  BaseString buf[2];
  buf[0].assfmt("cluster_deployment.%s", proc.m_name.c_str());
  buf[1].assfmt("cluster_deployment.%s.%u", proc.m_name.c_str(), proc.m_index);
  const char* groups[] = {buf[0].c_str(), buf[1].c_str(), 0};

  int ret = load_defaults(g_my_cnf, groups, &argc, &tmp);
  if (ret != 0) {
    g_logger.error("Failed to load defaults for cluster %s's process %s",
                   cluster.m_name.c_str(), proc.m_name.c_str());
    return false;
  }

  char* cmd = NULL;
  char* args = NULL;
  my_bool generate_port = 0;
  char* cpuset = NULL;

  struct my_option options[] = {
      {"cmd", 0, "Executable name", &cmd, 0, 0, GET_STR, OPT_ARG, 0, 0, 0, 0, 0,
       0},
      {"args", 0, "Arguments passed to process", &args, 0, 0, GET_STR, OPT_ARG,
       0, 0, 0, 0, 0, 0},
      {"port-generate", 0, "Flag to generate --port=N", &generate_port, 0, 0,
       GET_BOOL, OPT_ARG, 0, 0, 0, 0, 0, 0},
      {"cpuset", 0, "Process's CPU affinity", &cpuset, 0, 0, GET_STR, OPT_ARG,
       0, 0, 0, 0, 0, 0},
      {0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}};

  int status = handle_options(&argc, &tmp, options, NULL);
  if (status != 0) {
    g_logger.error("Failed to handle options for cluster %s' process %d",
                   cluster.m_name.c_str(), proc.m_type);

    return false;
  }

  proc.m_proc.m_cpuset = BaseString(cpuset);

  if (proc.m_type != atrt_process::AP_CUSTOM) {
    // Prevent overwriting settings of non custom processes
    return true;
  }

  if (!cmd) {
    g_logger.error("Cluster's %s process %s must define 'cmd'",
                   cluster.m_name.c_str(), proc.m_name.c_str());
    return false;
  }

  char* bin_path = find_bin_path(cmd);
  if (!bin_path) {
    g_logger.error("Cluster's %s custom process %s binary could not be found",
                   cluster.m_name.c_str(), proc.m_name.c_str());
    return false;
  }

  proc.m_proc.m_path.assign(bin_path);

  if (args) {
    proc.m_proc.m_args.appfmt(" %s", args);
  }

  const char* port_arg = "--port=";
  const char* val;
  if (generate_port && !proc.m_options.m_loaded.get(port_arg, &val)) {
    BaseString portno;
    portno.assfmt("%d", g_baseport + proc.m_procno);

    proc.m_proc.m_args.appfmt(" %s%s", port_arg, portno.c_str());
    proc.m_options.m_generated.put(port_arg, portno.c_str());
    proc.m_options.m_loaded.put(port_arg, portno.c_str());
  }

  return true;
}

BaseString getProcGroupName(atrt_process::Type type) {
  BaseString name;
  switch (type) {
    case atrt_process::AP_CLIENT:
      name.assign("client");
      break;
    case atrt_process::AP_MYSQLD:
      name.assign("mysqld");
      break;
    case atrt_process::AP_NDB_API:
      name.assign("ndb_api");
      break;
    case atrt_process::AP_NDB_MGMD:
      name.assign("ndb_mgmd");
      break;
    case atrt_process::AP_NDBD:
      name.assign("ndbd");
      break;
    case atrt_process::AP_CUSTOM:
    case atrt_process::AP_ALL:
    case atrt_process::AP_CLUSTER:
      // No group name in my.cnf skipping
      break;
  }

  return name;
}

static bool load_process(atrt_config& config, atrt_cluster& cluster,
                         BaseString name, atrt_process::Type type, unsigned idx,
                         const char* hostname) {
  atrt_host* host_ptr = find(hostname, config.m_hosts);
  atrt_process* proc_ptr = new atrt_process;

  const unsigned proc_no = (unsigned)config.m_processes.size();
  config.m_processes.push_back(proc_ptr);
  host_ptr->m_processes.push_back(proc_ptr);
  cluster.m_processes.push_back(proc_ptr);

  atrt_process& proc = *proc_ptr;

  proc.m_index = idx;
  proc.m_type = type;

  proc.m_name = name.empty() ? getProcGroupName(type) : name;
  proc.m_procno = proc_no;
  proc.m_host = host_ptr;
  proc.m_save.m_saved = false;
  proc.m_nodeid = -1;
  proc.m_cluster = &cluster;
  proc.m_options.m_features = 0;
  proc.m_rep_src = 0;
  proc.m_proc.m_id = -1;
  proc.m_proc.m_type = "temporary";
  proc.m_proc.m_owner = "atrt";
  if (config.m_site.length() == 0) {
    proc.m_proc.m_group.assfmt("%s", cluster.m_name.c_str());
  } else {
    proc.m_proc.m_group.assfmt("%s-%s", config.m_site.c_str(),
                               cluster.m_name.c_str());
  }
  proc.m_proc.m_stdout = "log.out";
  proc.m_proc.m_stderr = "2>&1";
  proc.m_proc.m_runas = proc.m_host->m_user;
  proc.m_proc.m_ulimit = "c:unlimited";
  proc.m_proc.m_env.assfmt("MYSQL_BASE_DIR=%s", g_prefix0);
  proc.m_proc.m_env.appfmt(" MYSQL_HOME=%s", g_basedir);
  proc.m_proc.m_env.appfmt(" ATRT_PID=%u", (unsigned)proc_no);
  proc.m_proc.m_shutdown_options = "";

  {
    /**
     * In 5.5...binaries aren't compiled with rpath
     * So we need an explicit LD_LIBRARY_PATH
     *
     * Use path from libmysqlclient.so
     */
    char* dir = dirname(g_libmysqlclient_so_path);
#if defined(__MACH__)
    proc.m_proc.m_env.appfmt(" DYLD_LIBRARY_PATH=%s", dir);
#else
    proc.m_proc.m_env.appfmt(" LD_LIBRARY_PATH=%s", dir);
#endif
    free(dir);
  }

  int argc = 1;
  const char* argv[] = {"atrt", 0, 0};

  BaseString buf[10];
  char** tmp = (char**)argv;
  const char* groups[] = {0, 0, 0, 0};
  switch (type) {
    case atrt_process::AP_NDB_MGMD:
      proc.m_nodeid = cluster.m_next_nodeid++;  // always specify node-id

      groups[0] = "cluster_config";
      buf[1].assfmt("cluster_config.ndb_mgmd.%u", idx);
      groups[1] = buf[1].c_str();
      buf[0].assfmt("--defaults-group-suffix=%s", cluster.m_name.c_str());
      argv[argc++] = buf[0].c_str();
      break;
    case atrt_process::AP_NDBD:
      if (g_fix_nodeid) proc.m_nodeid = cluster.m_next_nodeid++;

      groups[0] = "cluster_config";
      buf[1].assfmt("cluster_config.ndbd.%u", idx);
      groups[1] = buf[1].c_str();
      buf[0].assfmt("--defaults-group-suffix=%s", cluster.m_name.c_str());
      argv[argc++] = buf[0].c_str();
      break;
    case atrt_process::AP_MYSQLD:
      if (g_fix_nodeid) proc.m_nodeid = cluster.m_next_nodeid++;

      groups[0] = "mysqld";
      groups[1] = "mysql_cluster";
      buf[0].assfmt("--defaults-group-suffix=.%u%s", idx,
                    cluster.m_name.c_str());
      argv[argc++] = buf[0].c_str();
      break;
    case atrt_process::AP_CLIENT:
      buf[0].assfmt("client.%u%s", idx, cluster.m_name.c_str());
      groups[0] = buf[0].c_str();
      break;
    case atrt_process::AP_NDB_API:
      if (g_fix_nodeid) proc.m_nodeid = cluster.m_next_nodeid++;
      break;
    case atrt_process::AP_CUSTOM:
      buf[0].assfmt("%s%s", proc.m_name.c_str(), cluster.m_name.c_str());
      groups[0] = buf[0].c_str();

      buf[1].assfmt("%s.%u%s", proc.m_name.c_str(), idx,
                    cluster.m_name.c_str());
      groups[1] = buf[1].c_str();
      break;
    default:
      g_logger.critical("Unhandled process type: %d", type);
      return false;
  }

  int ret = load_defaults(g_my_cnf, groups, &argc, &tmp);
  if (ret) {
    g_logger.error("Unable to load defaults for cluster: %s",
                   cluster.m_name.c_str());
    return false;
  }

  load_options(argc, tmp, type, proc.m_options);

  BaseString dir;
  dir.assfmt("%s/%s", proc.m_host->m_basedir.c_str(), cluster.m_dir.c_str());

  switch (type) {
    case atrt_process::AP_NDB_MGMD: {
      proc.m_proc.m_name.assfmt("%u-%s", proc_no, "ndb_mgmd");
      proc.m_proc.m_cwd.assfmt("%sndb_mgmd.%u", dir.c_str(), proc.m_index);
      proc.m_proc.m_path.assign(g_ndb_mgmd_bin_path);
      proc.m_proc.m_env.appfmt(" MYSQL_GROUP_SUFFIX=%s",
                               cluster.m_name.c_str());
      proc.m_proc.m_args.assfmt("--defaults-file=%s/my.cnf",
                                proc.m_host->m_basedir.c_str());
      proc.m_proc.m_args.appfmt(" --defaults-group-suffix=%s",
                                cluster.m_name.c_str());

      switch (config.m_config_type) {
        case atrt_config::CNF: {
          proc.m_proc.m_args.append(" --mycnf");
          break;
        }
        case atrt_config::INI: {
          proc.m_proc.m_args.assfmt("--config-file=%s/config%s.ini",
                                    proc.m_host->m_basedir.c_str(),
                                    cluster.m_name.c_str());
          break;
        }
      }
      proc.m_proc.m_args.append(" --nodaemon");
      proc.m_proc.m_args.appfmt(" --ndb-nodeid=%u", proc.m_nodeid);
      proc.m_proc.m_args.appfmt(" --configdir=%s", proc.m_proc.m_cwd.c_str());
      break;
    }
    case atrt_process::AP_NDBD: {
      proc.m_proc.m_name.assfmt("%u-%s", proc_no, "ndbd");
      proc.m_proc.m_cwd.assfmt("%sndbd.%u", dir.c_str(), proc.m_index);

      if (g_mt == 0 || (g_mt == 1 && ((g_mt_rr++) & 1) == 0) ||
          g_ndbmtd_bin_path == 0) {
        proc.m_proc.m_path.assign(g_ndbd_bin_path);
      } else {
        proc.m_proc.m_path.assign(g_ndbmtd_bin_path);
      }

      proc.m_proc.m_env.appfmt(" MYSQL_GROUP_SUFFIX=%s",
                               cluster.m_name.c_str());

      proc.m_proc.m_args.assfmt("--defaults-file=%s/my.cnf",
                                proc.m_host->m_basedir.c_str());
      proc.m_proc.m_args.appfmt(" --defaults-group-suffix=%s",
                                cluster.m_name.c_str());
      proc.m_proc.m_args.append(" --nodaemon -n");

      if (!g_restart) proc.m_proc.m_args.append(" --initial");
      if (g_fix_nodeid)
        proc.m_proc.m_args.appfmt(" --ndb-nodeid=%u", proc.m_nodeid);
      break;
    }
    case atrt_process::AP_MYSQLD: {
      proc.m_proc.m_name.assfmt("%u-%s", proc_no, "mysqld");
      proc.m_proc.m_path.assign(g_mysqld_bin_path);
      proc.m_proc.m_args.assfmt("--defaults-file=%s/my.cnf",
                                proc.m_host->m_basedir.c_str());
      proc.m_proc.m_args.appfmt(" --defaults-group-suffix=.%d%s", proc.m_index,
                                cluster.m_name.c_str());
      proc.m_proc.m_args.append(" --core-file");
      if (g_fix_nodeid)
        proc.m_proc.m_args.appfmt(" --ndb-nodeid=%d", proc.m_nodeid);

      // Add ndb connect string
      const char* val;
      if (cluster.m_options.m_loaded.get(ndbcs, &val)) {
        proc.m_proc.m_args.appfmt(" %s=%s", ndbcs, val);
      }

      proc.m_proc.m_cwd.appfmt("%smysqld.%u", dir.c_str(), proc.m_index);
      proc.m_proc.m_shutdown_options = "SIGKILL";  // not nice
      proc.m_proc.m_env.appfmt(" MYSQL_GROUP_SUFFIX=.%u%s", proc.m_index,
                               cluster.m_name.c_str());
      break;
    }
    case atrt_process::AP_NDB_API: {
      proc.m_proc.m_name.assfmt("%u-%s", proc_no, "ndb_api");
      proc.m_proc.m_path = "";
      proc.m_proc.m_args = "";
      proc.m_proc.m_cwd.appfmt("%sndb_api.%u", dir.c_str(), proc.m_index);
      proc.m_proc.m_env.appfmt(" MYSQL_GROUP_SUFFIX=%s",
                               cluster.m_name.c_str());
      break;
    }
    case atrt_process::AP_CLIENT: {
      proc.m_proc.m_name.assfmt("%u-%s", proc_no, "mysql");
      proc.m_proc.m_path = "";
      proc.m_proc.m_args = "";
      proc.m_proc.m_cwd.appfmt("%s/client.%u", dir.c_str(), proc.m_index);
      proc.m_proc.m_env.appfmt(" MYSQL_GROUP_SUFFIX=.%d%s", proc.m_index,
                               cluster.m_name.c_str());
      break;
    }
    case atrt_process::AP_CUSTOM: {
      proc.m_proc.m_name.assfmt("%u-%s", proc_no, proc.m_name.c_str());
      proc.m_proc.m_cwd.assfmt("%s%s.%u", dir.c_str(), proc.m_name.c_str(),
                               proc.m_index);

      const char* port_arg = "--port=";
      const char* val;
      if (proc.m_options.m_loaded.get(port_arg, &val)) {
        proc.m_proc.m_args.assfmt("%s%s", port_arg, val);
      }
      break;
    }
    case atrt_process::AP_ALL:
    case atrt_process::AP_CLUSTER:
      g_logger.critical("Unhandled process type: %d", proc.m_type);
      return false;
  }

  if (type == atrt_process::AP_MYSQLD) {
    /**
     * Add a client for each mysqld
     */
    BaseString name;
    if (!load_process(config, cluster, name, atrt_process::AP_CLIENT, idx,
                      hostname)) {
      return false;
    }
  }

  if (type == atrt_process::AP_CLIENT) {
    proc.m_mysqld = cluster.m_processes[cluster.m_processes.size() - 2];
  }

  bool status = load_deployment_options_for_process(cluster, proc);
  return status;
}

static bool load_options(int argc, char** argv, int type, atrt_options& opts) {
  for (size_t i = 0; i < (size_t)argc; i++) {
    if (ndb_is_load_default_arg_separator(argv[i])) continue;
    for (size_t j = 0; f_options[j].name; j++) {
      const char* name = f_options[j].name;
      const size_t len = strlen(name);

      if ((f_options[j].type & type) && strncmp(argv[i], name, len) == 0) {
        opts.m_loaded.put(name, argv[i] + len, true);
        break;
      }
    }
  }
  return true;
}

struct proc_rule_ctx {
  int m_setup;
  atrt_config* m_config;
  atrt_host* m_host;
  atrt_cluster* m_cluster;
  atrt_process* m_process;
};

struct proc_rule {
  int type;
  bool (*func)(Properties& prop, proc_rule_ctx&, int extra);
  int extra;
};

static bool pr_check_replication(Properties&, proc_rule_ctx&, int);
static bool pr_check_features(Properties&, proc_rule_ctx&, int);
static bool pr_fix_client(Properties&, proc_rule_ctx&, int);
static bool pr_proc_options(Properties&, proc_rule_ctx&, int);
static bool pr_fix_ndb_connectstring(Properties&, proc_rule_ctx&, int);
static bool pr_set_ndb_connectstring(Properties&, proc_rule_ctx&, int);
static bool pr_check_proc(Properties&, proc_rule_ctx&, int);
static bool pr_set_customprocs_connectstring(Properties&, proc_rule_ctx&, int);

static proc_rule f_rules[] = {
    {atrt_process::AP_CLUSTER, pr_check_features, 0},
    {atrt_process::AP_MYSQLD, pr_check_replication, 0},
    {(atrt_process::AP_ALL & ~atrt_process::AP_CLIENT &
      ~atrt_process::AP_CUSTOM),
     pr_proc_options, ~(PO_REP | PO_NDB)},
    {(atrt_process::AP_ALL & ~atrt_process::AP_CLIENT &
      ~atrt_process::AP_CUSTOM),
     pr_proc_options, PO_REP},
    {atrt_process::AP_CLIENT, pr_fix_client, 0},
    {atrt_process::AP_CLUSTER, pr_fix_ndb_connectstring, 0},
    {atrt_process::AP_MYSQLD, pr_set_ndb_connectstring, 0},
    {atrt_process::AP_ALL & ~atrt_process::AP_CUSTOM, pr_check_proc, 0},
    {atrt_process::AP_CLUSTER, pr_set_customprocs_connectstring, 0},
    {0, 0, 0}};

bool configure(atrt_config& config, int setup) {
  Properties props;

  for (size_t i = 0; f_rules[i].func; i++) {
    bool ok = true;
    proc_rule_ctx ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.m_setup = setup;
    ctx.m_config = &config;

    for (unsigned j = 0; j < config.m_clusters.size(); j++) {
      ctx.m_cluster = config.m_clusters[j];

      if (f_rules[i].type & atrt_process::AP_CLUSTER) {
        g_logger.debug("applying rule %u to cluster %s", (unsigned)i,
                       ctx.m_cluster->m_name.c_str());
        if (!(*f_rules[i].func)(props, ctx, f_rules[i].extra)) ok = false;
      } else {
        atrt_cluster& cluster = *config.m_clusters[j];
        for (unsigned k = 0; k < cluster.m_processes.size(); k++) {
          atrt_process& proc = *cluster.m_processes[k];
          ctx.m_process = cluster.m_processes[k];
          if (proc.m_type & f_rules[i].type) {
            g_logger.debug("applying rule %u to %s", (unsigned)i,
                           proc.m_proc.m_cwd.c_str());
            if (!(*f_rules[i].func)(props, ctx, f_rules[i].extra)) ok = false;
          }
        }
      }
    }

    if (!ok) {
      return false;
    }
  }

  return true;
}

static atrt_process* find(atrt_config& config, int type, const char* name) {
  BaseString tmp(name);
  Vector<BaseString> src;
  Vector<BaseString> dst;
  tmp.split(src, ".");

  if (src.size() != 2) {
    return 0;
  }
  atrt_cluster* cluster = 0;
  BaseString cl;
  cl.appfmt(".%s", src[1].c_str());
  for (unsigned i = 0; i < config.m_clusters.size(); i++) {
    if (config.m_clusters[i]->m_name == cl) {
      cluster = config.m_clusters[i];
      break;
    }
  }

  if (cluster == 0) {
    return 0;
  }

  int idx = atoi(src[0].c_str()) - 1;
  for (unsigned i = 0; i < cluster->m_processes.size(); i++) {
    if (cluster->m_processes[i]->m_type & type) {
      if (idx == 0)
        return cluster->m_processes[i];
      else
        idx--;
    }
  }

  return 0;
}

static bool pr_check_replication(Properties& props, proc_rule_ctx& ctx, int) {
  if (!(ctx.m_config->m_replication == "")) {
    Vector<BaseString> list;
    ctx.m_config->m_replication.split(list, ";");
    atrt_config& config = *ctx.m_config;

    ctx.m_config->m_replication = "";

    const char* msg = "Invalid replication specification";
    for (unsigned i = 0; i < list.size(); i++) {
      Vector<BaseString> rep;
      list[i].split(rep, ":");
      if (rep.size() != 2) {
        g_logger.error("%s: %s (split: %d)", msg, list[i].c_str(), rep.size());
        return false;
      }

      atrt_process* src = find(config, atrt_process::AP_MYSQLD, rep[0].c_str());
      atrt_process* dst = find(config, atrt_process::AP_MYSQLD, rep[1].c_str());

      if (src == 0 || dst == 0) {
        g_logger.error("%s: %s (%d %d)", msg, list[i].c_str(), src != 0,
                       dst != 0);
        return false;
      }

      if (dst->m_rep_src != 0) {
        g_logger.error("%s: %s : %s already has replication src (%s)", msg,
                       list[i].c_str(), dst->m_proc.m_cwd.c_str(),
                       dst->m_rep_src->m_proc.m_cwd.c_str());
        return false;
      }

      dst->m_rep_src = src;
      src->m_rep_dst.push_back(dst);

      src->m_options.m_features |= PO_REP_MASTER;
      dst->m_options.m_features |= PO_REP_SLAVE;
    }
  }
  return true;
}

static bool pr_check_features(Properties& props, proc_rule_ctx& ctx, int) {
  atrt_cluster& cluster = *ctx.m_cluster;
  if (cluster.m_name == ".atrt") {
    // skip cluster and replication features for atrt
    return true;
  }

  int features = 0;
  for (unsigned i = 0; i < cluster.m_processes.size(); i++) {
    if (cluster.m_processes[i]->m_type == atrt_process::AP_NDB_MGMD ||
        cluster.m_processes[i]->m_type == atrt_process::AP_NDB_API ||
        cluster.m_processes[i]->m_type == atrt_process::AP_NDBD) {
      features |= atrt_options::AO_NDBCLUSTER;
      break;
    }
  }

  if (features) {
    cluster.m_options.m_features |= features;
    for (unsigned i = 0; i < cluster.m_processes.size(); i++) {
      cluster.m_processes[i]->m_options.m_features |= features;
    }
  }
  return true;
}

static bool pr_fix_client(Properties& props, proc_rule_ctx& ctx, int) {
  atrt_process& proc = *ctx.m_process;
  const char *val, *name = "--host=";
  if (!proc.m_options.m_loaded.get(name, &val)) {
    val = proc.m_mysqld->m_host->m_hostname.c_str();
    proc.m_options.m_loaded.put(name, val);
    proc.m_options.m_generated.put(name, val);
  }

  for (size_t i = 0; f_options[i].name; i++) {
    proc_option& opt = f_options[i];
    const char* name = opt.name;
    if (opt.type & atrt_process::AP_CLIENT) {
      const char* val;
      if (!proc.m_options.m_loaded.get(name, &val)) {
        require(proc.m_mysqld->m_options.m_loaded.get(name, &val));
        proc.m_options.m_loaded.put(name, val);
        proc.m_options.m_generated.put(name, val);
      }
    }
  }

  return true;
}

static Uint32 try_default_port(atrt_process& proc, const char* name) {
  Uint32 port = strcmp(name, "--port=") == 0
                    ? 3306
                    : strcmp(name, "--PortNumber=") == 0 ? 1186 : 0;

  atrt_host* host = proc.m_host;
  for (unsigned i = 0; i < host->m_processes.size(); i++) {
    const char* val;
    if (host->m_processes[i]->m_options.m_loaded.get(name, &val)) {
      if ((Uint32)atoi(val) == port) return 0;
    }
  }
  return port;
}

static bool generate(atrt_process& proc, const char* name, Properties& props) {
  atrt_options& opts = proc.m_options;
  if (strcmp(name, "--port=") == 0 || strcmp(name, "--PortNumber=") == 0) {
    Uint32 val;
    if (g_default_ports == 0 || (val = try_default_port(proc, name)) == 0) {
      val = g_baseport;
      props.get("--PortNumber=", &val);
      props.put("--PortNumber=", (val + 1), true);
    }

    char buf[255];
    BaseString::snprintf(buf, sizeof(buf), "%u", val);
    opts.m_loaded.put(name, buf);
    opts.m_generated.put(name, buf);
    return true;
  } else if (strcmp(name, "--datadir=") == 0) {
    BaseString datadir(proc.m_proc.m_cwd);
    datadir.append("/data");
    opts.m_loaded.put(name, datadir.c_str());
    opts.m_generated.put(name, datadir.c_str());
    return true;
  } else if (strcmp(name, "--FileSystemPath=") == 0) {
    opts.m_loaded.put(name, proc.m_proc.m_cwd.c_str());
    opts.m_generated.put(name, proc.m_proc.m_cwd.c_str());
    return true;
  } else if (strcmp(name, "--socket=") == 0) {
    const char* sock = 0;
    if (g_default_ports) {
      sock = "/tmp/mysql.sock";
      atrt_host* host = proc.m_host;
      for (unsigned i = 0; i < host->m_processes.size(); i++) {
        const char* val;
        if (host->m_processes[i]->m_options.m_loaded.get(name, &val)) {
          if (strcmp(sock, val) == 0) {
            sock = 0;
            break;
          }
        }
      }
    }

    BaseString tmp;
    if (sock == 0) {
      tmp.assfmt("%s/mysql.sock", proc.m_proc.m_cwd.c_str());
      sock = tmp.c_str();
    }

    opts.m_loaded.put(name, sock);
    opts.m_generated.put(name, sock);
    return true;
  } else if (strcmp(name, "--server-id=") == 0) {
    Uint32 val = 1;
    props.get(name, &val);
    char buf[255];
    BaseString::snprintf(buf, sizeof(buf), "%u", val);
    opts.m_loaded.put(name, buf);
    opts.m_generated.put(name, buf);
    props.put(name, (val + 1), true);
    return true;
  } else if (strcmp(name, "--log-bin") == 0) {
    opts.m_loaded.put(name, "");
    opts.m_generated.put(name, "");
    return true;
  }

  g_logger.warning("Unknown parameter: %s", name);
  return true;
}

static bool pr_proc_options(Properties& props, proc_rule_ctx& ctx, int extra) {
  for (size_t i = 0; f_options[i].name; i++) {
    proc_option& opt = f_options[i];
    atrt_process& proc = *ctx.m_process;
    const char* name = opt.name;
    if (opt.type & proc.m_type) {
      if (opt.options == 0 ||
          (opt.options & extra & proc.m_options.m_features)) {
        const char* val;
        if (!proc.m_options.m_loaded.get(name, &val)) {
          generate(proc, name, props);
        }
      }
    }
  }
  return true;
}

static bool pr_fix_ndb_connectstring(Properties& props, proc_rule_ctx& ctx,
                                     int) {
  const char* val;
  atrt_cluster& cluster = *ctx.m_cluster;

  if (cluster.m_options.m_features & atrt_options::AO_NDBCLUSTER) {
    if (!cluster.m_options.m_loaded.get(ndbcs, &val)) {
      /**
       * Construct connect string for this cluster
       */
      BaseString str;
      for (unsigned i = 0; i < cluster.m_processes.size(); i++) {
        atrt_process* tmp = cluster.m_processes[i];
        if (tmp->m_type == atrt_process::AP_NDB_MGMD) {
          if (str.length()) {
            str.append(";");
          }
          const char* port;
          require(tmp->m_options.m_loaded.get("--PortNumber=", &port));
          str.appfmt("%s:%s", tmp->m_host->m_hostname.c_str(), port);
        }
      }
      cluster.m_options.m_loaded.put(ndbcs, str.c_str());
      cluster.m_options.m_generated.put(ndbcs, str.c_str());
      cluster.m_options.m_loaded.get(ndbcs, &val);
    }

    for (unsigned i = 0; i < cluster.m_processes.size(); i++) {
      cluster.m_processes[i]->m_proc.m_env.appfmt(" NDB_CONNECTSTRING=%s", val);
    }
  }
  return true;
}

static bool pr_set_ndb_connectstring(Properties& props, proc_rule_ctx& ctx,
                                     int) {
  const char* val;

  atrt_process& proc = *ctx.m_process;
  if (proc.m_options.m_features & atrt_options::AO_NDBCLUSTER) {
    if (!proc.m_options.m_loaded.get(ndbcs, &val)) {
      require(proc.m_cluster->m_options.m_loaded.get(ndbcs, &val));
      proc.m_options.m_loaded.put(ndbcs, val);
      proc.m_options.m_generated.put(ndbcs, val);
    }

    if (!proc.m_options.m_loaded.get("--ndbcluster", &val)) {
      proc.m_options.m_loaded.put("--ndbcluster", "");
      proc.m_options.m_generated.put("--ndbcluster", "");
    }
  }
  return true;
}

static bool pr_check_proc(Properties& props, proc_rule_ctx& ctx, int) {
  bool ok = true;
  bool generated = false;
  const int setup = ctx.m_setup;
  atrt_process& proc = *ctx.m_process;
  for (size_t i = 0; f_options[i].name; i++) {
    proc_option& opt = f_options[i];
    const char* name = opt.name;
    if ((ctx.m_process->m_type & opt.type) &&
        (opt.options == 0 ||
         (ctx.m_process->m_options.m_features & opt.options))) {
      const char* val;
      if (!proc.m_options.m_loaded.get(name, &val)) {
        ok = false;
        g_logger.warning("Missing parameter: %s for %s", name,
                         proc.m_proc.m_cwd.c_str());
      } else if (proc.m_options.m_generated.get(name, &val)) {
        if (setup == 0) {
          ok = false;
          g_logger.warning("Missing parameter: %s for %s", name,
                           proc.m_proc.m_cwd.c_str());
        } else {
          generated = true;
        }
      }
    }
  }

  if (generated) {
    ctx.m_config->m_generated = true;
  }

  // ndbout << proc << endl;

  return ok;
}

static bool pr_set_customprocs_connectstring(Properties& props,
                                             proc_rule_ctx& ctx, int) {
  atrt_cluster& cluster = *ctx.m_cluster;

  std::map<BaseString, BaseString> connectstrings;
  std::map<BaseString, BaseString>::iterator it;
  for (unsigned i = 0; i < cluster.m_processes.size(); i++) {
    atrt_process* proc = cluster.m_processes[i];
    if (proc->m_type != atrt_process::AP_CUSTOM) continue;

    BaseString host_list;

    it = connectstrings.find(proc->m_name);
    if (it != connectstrings.end()) {
      host_list = it->second;
      host_list.appfmt(",%s", proc->m_host->m_hostname.c_str());
    } else {
      host_list.assign(proc->m_host->m_hostname);
    }

    const char* port_arg = "--port=";
    const char* portno;
    if (proc->m_options.m_loaded.get(port_arg, &portno) ||
        proc->m_options.m_generated.get(port_arg, &portno)) {
      host_list.appfmt(":%s", portno);
    }

    connectstrings[proc->m_name] = host_list;
  }

  for (unsigned i = 0; i < cluster.m_processes.size(); i++) {
    atrt_process* proc = cluster.m_processes[i];

    bool client_or_api =
        proc->m_type & (atrt_process::AP_CLIENT | atrt_process::AP_NDB_API);
    if (!client_or_api) continue;

    for (it = connectstrings.begin(); it != connectstrings.end(); ++it) {
      BaseString connstr(it->first);
      connstr.ndb_toupper();
      connstr.append("_CONNECTSTRING");

      if (!proc->m_proc.m_env.empty()) proc->m_proc.m_env.append(" ");
      proc->m_proc.m_env.appfmt("%s=%s", connstr.c_str(), it->second.c_str());
    }
  }

  return true;
}

NdbOut& operator<<(NdbOut& out, const atrt_process& proc) {
  out << "[ atrt_process: ";
  switch (proc.m_type) {
    case atrt_process::AP_NDB_MGMD:
    case atrt_process::AP_NDBD:
    case atrt_process::AP_MYSQLD:
    case atrt_process::AP_NDB_API:
    case atrt_process::AP_CLIENT:
      out << proc.m_name.c_str();
      break;
    case atrt_process::AP_CUSTOM:
      out << "custom:" << proc.m_name.c_str() << ": ";
      break;
    default:
      out << "<unknown: " << (int)proc.m_type << " >";
  }

  out << " cluster: " << proc.m_cluster->m_name.c_str()
      << " host: " << proc.m_host->m_hostname.c_str() << endl
      << " cwd: " << proc.m_proc.m_cwd.c_str() << endl
      << " path: " << proc.m_proc.m_path.c_str() << endl
      << " args: " << proc.m_proc.m_args.c_str() << endl
      << " env: " << proc.m_proc.m_env.c_str() << endl;

  proc.m_options.m_generated.print(stdout, "generated: ");

  out << " ]";

#if 0  
  proc.m_index = 0; //idx;
  proc.m_host = host_ptr;
  proc.m_cluster = cluster;
  proc.m_proc.m_id = -1;
  proc.m_proc.m_type = "temporary";
  proc.m_proc.m_owner = "atrt";  
  proc.m_proc.m_group = cluster->m_name.c_str();
  proc.m_proc.m_cwd.assign(dir).append("/atrt/").append(cluster->m_dir);
  proc.m_proc.m_stdout = "log.out";
  proc.m_proc.m_stderr = "2>&1";
  proc.m_proc.m_runas = proc.m_host->m_user;
  proc.m_proc.m_ulimit = "c:unlimited";
  proc.m_proc.m_env.assfmt("MYSQL_BASE_DIR=%s", dir);
  proc.m_proc.m_shutdown_options = "";
#endif

  return out;
}

char* find_bin_path(const char* exe) { return find_bin_path(g_prefix0, exe); }

char* find_bin_path(const char* prefix, const char* exe) {
  if (exe == 0) return 0;

  if (exe[0] == '/') {
    /**
     * Trust that path is correct...
     */
    return strdup(exe);
  }

  for (int i = 0; g_search_path[i] != 0; i++) {
    BaseString p;
    p.assfmt("%s/%s/%s", prefix, g_search_path[i], exe);
    if (File_class::exists(p.c_str())) {
      return strdup(p.c_str());
    }
  }
  return 0;
}
