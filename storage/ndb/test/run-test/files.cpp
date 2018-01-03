/*
   Copyright (c) 2007, 2017, Oracle and/or its affiliates. All rights reserved.

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

#include "atrt.hpp"
#include <portlib/NdbDir.hpp>
#include <portlib/NdbSleep.h>

static bool create_directory(const char * path);

bool
setup_directories(atrt_config& config, int setup)
{
  /**
   * 0 = validate
   * 1 = setup
   * 2 = setup+clean
   */
  for (unsigned i = 0; i < config.m_clusters.size(); i++)
  {
    atrt_cluster& cluster = *config.m_clusters[i];
    for (unsigned j = 0; j<cluster.m_processes.size(); j++)
    {
      atrt_process& proc = *cluster.m_processes[j];
      const char * dir = proc.m_proc.m_cwd.c_str();
      struct stat sbuf;
      int exists = 0;
      if (lstat(dir, &sbuf) == 0)
      {
	if (S_ISDIR(sbuf.st_mode))
	  exists = 1;
	else
	  exists = -1;
      }
      
      switch(setup){
      case 0:
	switch(exists){
	case 0:
	  g_logger.error("Could not find directory: %s", dir);
	  return false;
	case -1:
	  g_logger.error("%s is not a directory!", dir);
	  return false;
	}
	break;
      case 1:
	if (exists == -1)
	{
	  g_logger.error("%s is not a directory!", dir);
	  return false;
	}
	break;
      case 2:
	if (exists == 1)
	{
	  if (!remove_dir(dir))
	  {
	    g_logger.error("Failed to remove %s!", dir);
	    return false;
	  }
	  exists = 0;
	  break;
	}
	else if (exists == -1)
	{
	  if (!unlink(dir))
	  {
	    g_logger.error("Failed to remove %s!", dir);
	    return false;
	  }
	  exists = 0;
	}
      }
      if (exists != 1)
      {
	if (!create_directory(dir))
	{
	  return false;
	}
      }
    }
  }
  return true;
}

static
void
printfile(FILE* out, Properties& props, const char * section, ...)
  ATTRIBUTE_FORMAT(printf, 3, 4);

static
void
printfile(FILE* out, Properties& props, const char * section, ...)
{
  Properties::Iterator it (&props);
  const char * name = it.first();
  if (name)
  {
    va_list ap;
    va_start(ap, section);
    /* const int ret = */ vfprintf(out, section, ap);
    va_end(ap);
    fprintf(out, "\n");
    
    for (; name;  name = it.next())
    {
      const char* val;
      props.get(name, &val);
      fprintf(out, "%s %s\n", name + 2, val);
    }
    fprintf(out, "\n");
  }
  fflush(out);
}

static
char *
dirname(const char * path)
{
  char * s = strdup(path);
  size_t len = strlen(s);
  for (size_t i = 1; i<len; i++)
  {
    if (s[len - i] == '/')
    {
      s[len - i] = 0;
      return s;
    }
  }
  free(s);
  return 0;
}

#ifdef _WIN32
#define popen _popen
#define pclose _pclose
#endif

bool
setup_files(atrt_config& config, int setup, int sshx)
{
  /**
   * 0 = validate
   * 1 = setup
   * 2 = setup+clean
   */
  BaseString mycnf;
  mycnf.assfmt("%s/my.cnf", g_basedir);

  if (!create_directory(g_basedir))
  {
    return false;
  }

  if (mycnf != g_my_cnf)
  {
    struct stat sbuf;
    int ret = lstat(to_native(mycnf).c_str(), &sbuf);
    
    if (ret == 0)
    {
      if (unlink(to_native(mycnf).c_str()) != 0)
      {
	g_logger.error("Failed to remove %s", mycnf.c_str());
	return false;
      }
    }
    
    BaseString cp;
    cp.assfmt("cp %s %s", g_my_cnf, mycnf.c_str());
    to_fwd_slashes(cp);
    if (sh(cp.c_str()) != 0)
    {
      g_logger.error("Failed to '%s'", cp.c_str());
      return false;
    }
  }
  
  if (setup == 2 || config.m_generated)
  {
    bool use_mysqld = (g_mysql_install_db_bin_path == NULL);
    if (!use_mysqld)
    {
      // Even if mysql_install_db exists, prefer use of mysqld if possible
      BaseString tmp;
      tmp.assfmt("%s --help --verbose", g_mysqld_bin_path);
      FILE *f = popen(tmp.c_str(), "re");
      char buf[1000];
      while (NULL != fgets(buf, sizeof(buf), f))
      {
        if (strncmp(buf, "initialize-insecure ", 20) == 0)
        {
          use_mysqld = true;
        }
      }
      pclose(f);
    }
    /**
     * Do mysql_install_db
     */
    for (unsigned i = 0; i < config.m_clusters.size(); i++)
    {
      atrt_cluster& cluster = *config.m_clusters[i];
      for (unsigned j = 0; j<cluster.m_processes.size(); j++)
      {
	atrt_process& proc = *cluster.m_processes[j];
	if (proc.m_type == atrt_process::AP_MYSQLD)
#ifndef _WIN32
	{
	  const char * val;
	  require(proc.m_options.m_loaded.get("--datadir=", &val));
	  BaseString tmp;
          if (use_mysqld)
          {
            tmp.assfmt("%s --defaults-file=%s/my.cnf --basedir=%s "
                         "--datadir=%s --initialize-insecure "
                         "> %s/mysqld-initialize.log 2>&1",
                       g_mysqld_bin_path,
                       g_basedir,
                       g_prefix,
                       val,
                       proc.m_proc.m_cwd.c_str());
          }
          else
          {
            assert(g_mysql_install_db_bin_path != NULL);
            tmp.assfmt("%s --defaults-file=%s/my.cnf --basedir=%s "
                         "--datadir=%s > %s/mysql_install_db.log 2>&1",
                       g_mysql_install_db_bin_path,
                       g_basedir,
                       g_prefix0,
                       val,
                       proc.m_proc.m_cwd.c_str());
          }
          to_fwd_slashes(tmp);
          if (sh(tmp.c_str()) != 0)
          {
            if (use_mysqld)
            {
              g_logger.error("Failed to mysqld --initialize-insecure for "
                               "%s, cmd: '%s'",
                             proc.m_proc.m_cwd.c_str(),
                             tmp.c_str());
            }
            else
            {
              g_logger.error("Failed to mysql_install_db for %s, cmd: '%s'",
                             proc.m_proc.m_cwd.c_str(),
                             tmp.c_str());
            }
          }
          else
          {
            if (use_mysqld)
            {
              g_logger.info("mysqld --initialize-insecure for %s",
                            proc.m_proc.m_cwd.c_str());
            }
            else
            {
              g_logger.info("mysql_install_db for %s",
                            proc.m_proc.m_cwd.c_str());
            }
          }
        }
#else
        {
          g_logger.info("not running mysqld --initialize-insecure nor "
                          "mysql_install_db for %s",
                        proc.m_proc.m_cwd.c_str());
        }
#endif
      }
    }
  }
  
  FILE * out = NULL;
  bool retval = true;
  if (config.m_generated == false)
  {
    g_logger.info("Nothing configured...");
  }
  else
  {
    out = fopen(mycnf.c_str(), "a+");
    if (out == 0)
    {
      g_logger.error("Failed to open %s for append", mycnf.c_str());
      return false;
    }
    time_t now = time(0);
    fprintf(out, "#\n# Generated by atrt\n");
    fprintf(out, "# %s\n", ctime(&now));
  }
  
  for (unsigned i = 0; i < config.m_clusters.size(); i++)
  {
    atrt_cluster& cluster = *config.m_clusters[i];
    if (out)
    {
      Properties::Iterator it(&cluster.m_options.m_generated);
      printfile(out, cluster.m_options.m_generated,
		"[mysql_cluster%s]", cluster.m_name.c_str());
    }
      
    for (unsigned j = 0; j<cluster.m_processes.size(); j++)
    {
      atrt_process& proc = *cluster.m_processes[j];
      
      if (out)
      {
	switch(proc.m_type){
	case atrt_process::AP_NDB_MGMD:
	  printfile(out, proc.m_options.m_generated,
		    "[cluster_config.ndb_mgmd.%d%s]", 
		    proc.m_index, proc.m_cluster->m_name.c_str());
	  break;
	case atrt_process::AP_NDBD: 
	  printfile(out, proc.m_options.m_generated,
		    "[cluster_config.ndbd.%d%s]",
		    proc.m_index, proc.m_cluster->m_name.c_str());
	  break;
	case atrt_process::AP_MYSQLD:
	  printfile(out, proc.m_options.m_generated,
		    "[mysqld.%d%s]",
		    proc.m_index, proc.m_cluster->m_name.c_str());
	  break;
	case atrt_process::AP_NDB_API:
	  break;
	case atrt_process::AP_CLIENT:
	  printfile(out, proc.m_options.m_generated,
		    "[client.%d%s]",
		    proc.m_index, proc.m_cluster->m_name.c_str());
	  break;
	case atrt_process::AP_ALL:
	case atrt_process::AP_CLUSTER:
	  abort();
	}
      }
      
      /**
       * Create env.sh
       */
      BaseString tmp;
      tmp.assfmt("%s/env.sh", proc.m_proc.m_cwd.c_str());
      to_native(tmp);
      char **env = BaseString::argify(0, proc.m_proc.m_env.c_str());
      if (env[0] || proc.m_proc.m_path.length())
      {
	Vector<BaseString> keys;
	FILE *fenv = fopen(tmp.c_str(), "w+");
	if (fenv == 0)
	{
	  g_logger.error("Failed to open %s for writing", tmp.c_str());
	  retval = false;
          goto end;
	}
	for (size_t k = 0; env[k]; k++)
	{
	  tmp = env[k];
	  ssize_t pos = tmp.indexOf('=');
	  require(pos > 0);
	  env[k][pos] = 0;
	  fprintf(fenv, "%s=\"%s\"\n", env[k], env[k]+pos+1);
	  keys.push_back(env[k]);
	  free(env[k]);
	}
	if (proc.m_proc.m_path.length())
	{
	  fprintf(fenv, "CMD=\"%s", proc.m_proc.m_path.c_str());
	  if (proc.m_proc.m_args.length())
	  {
	    fprintf(fenv, " %s", proc.m_proc.m_args.c_str());
	  }
	  fprintf(fenv, "\"\nexport CMD\n");
	}

        fprintf(fenv, "PATH=");
        for (int i = 0; g_search_path[i] != 0; i++)
        {
          fprintf(fenv, "%s/%s:", g_prefix0, g_search_path[i]);
        }
        fprintf(fenv, "$PATH\n");
	keys.push_back("PATH");

        {
          /**
           * In 5.5...binaries aren't compiled with rpath
           * So we need an explicit LD_LIBRARY_PATH
           *
           * Use path from libmysqlclient.so
           */
          char * dir = dirname(g_libmysqlclient_so_path);
#if defined(__MACH__)
          fprintf(fenv, "DYLD_LIBRARY_PATH=%s:$DYLD_LIBRARY_PATH\n", dir);
          keys.push_back("DYLD_LIBRARY_PATH");
#else
          fprintf(fenv, "LD_LIBRARY_PATH=%s:$LD_LIBRARY_PATH\n", dir);
          keys.push_back("LD_LIBRARY_PATH");
#endif
          free(dir);
        }

        for (unsigned k = 0; k<keys.size(); k++)
	  fprintf(fenv, "export %s\n", keys[k].c_str());

	fflush(fenv);
	fclose(fenv);
      }
      free(env);

      {
        tmp.assfmt("%s/ssh-login.sh", proc.m_proc.m_cwd.c_str());
        FILE* fenv = fopen(tmp.c_str(), "w+");
        if (fenv == 0)
        {
          g_logger.error("Failed to open %s for writing", tmp.c_str());
          retval = false;
          goto end;
        }
        fprintf(fenv, "#!/bin/sh\n");
        fprintf(fenv, "cd %s\n", proc.m_proc.m_cwd.c_str());
        fprintf(fenv, "[ -f /etc/profile ] && . /etc/profile\n");
        fprintf(fenv, ". ./env.sh\n");
        fprintf(fenv, "ulimit -Sc unlimited\n");
        fprintf(fenv, "bash -i");
        fflush(fenv);
        fclose(fenv);
      }
    }
  }

end:
  if (out)
  {
    fclose(out);
  }

  return retval;
}


static
bool
create_directory(const char * path)
{
  BaseString native(path);
  to_native(native);
  BaseString tmp(path);
  Vector<BaseString> list;

  if (tmp.split(list, "/") == 0)
  {
    g_logger.error("Failed to create directory: %s", tmp.c_str());
    return false;
  }
  
  BaseString cwd = IF_WIN("","/");
  for (unsigned i = 0; i < list.size(); i++)
  {
    cwd.append(list[i].c_str());
    cwd.append("/");
    NdbDir::create(cwd.c_str(),
                   NdbDir::u_rwx() | NdbDir::g_r() | NdbDir::g_x(),
                   true);
  }

  struct stat sbuf;
  if (lstat(native.c_str(), &sbuf) != 0 ||
      !S_ISDIR(sbuf.st_mode))
  {
    g_logger.error("Failed to create directory: %s (%s)", 
		   native.c_str(),
		   cwd.c_str());
    return false;
  }
  
  return true;
}

bool
remove_dir(const char * path, bool inclusive)
{
  if (access(path, 0))
    return true;

  const int max_retries = 20;
  int attempt = 0;

  while(true)
  {
    if (NdbDir::remove_recursive(path, !inclusive))
      return true;

    attempt++;
    if (attempt > max_retries)
    {
      g_logger.error("Failed to remove directory '%s'!", path);
      return false;
    }

    g_logger.warning(" - attempt %d to remove directory '%s' failed "
                     ", retrying...", attempt, path);

    NdbSleep_MilliSleep(100);
  }

  abort(); // Never reached
  return false;
}
