/*
  Copyright (c) 2002, 2003 Novell, Inc. All Rights Reserved. 

  This program is free software; you can redistribute it and/or modify 
  it under the terms of the GNU General Public License as published by 
  the Free Software Foundation; either version 2 of the License, or 
  (at your option) any later version. 

  This program is distributed in the hope that it will be useful, 
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the 
  GNU General Public License for more details. 

  You should have received a copy of the GNU General Public License 
  along with this program; if not, write to the Free Software 
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/ 

#include <my_global.h>
#include <m_string.h>
#include <dirent.h>
#include <screen.h>
#include <nks/vm.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/mode.h>
#include "my_manage.h"
#ifdef __NETWARE__
#define strindex(a,b) ((char*)strindex(a,b))
#define strstr(a,b)   ((char*)strstr(a,b))
#endif

/******************************************************************************

  macros
  
******************************************************************************/

#define HEADER  "TEST                                           ELAPSED      RESULT      \n"
#define DASH    "------------------------------------------------------------------------\n"

#define NW_TEST_SUFFIX    ".nw-test"
#define NW_RESULT_SUFFIX  ".nw-result"
#define TEST_SUFFIX		    ".test"
#define RESULT_SUFFIX	    ".result"
#define REJECT_SUFFIX	    ".reject"
#define OUT_SUFFIX		    ".out"
#define ERR_SUFFIX		    ".err"

#define TEST_PASS		"[ pass ]"
#define TEST_SKIP		"[ skip ]"
#define TEST_FAIL		"[ fail ]"
#define TEST_BAD		"[ bad  ]"
#define TEST_IGNORE		"[ignore]"

/******************************************************************************

  global variables
  
******************************************************************************/

char base_dir[PATH_MAX]   = "sys:/mysql";
char db[PATH_MAX]         = "test";
char user[PATH_MAX]       = "root";
char password[PATH_MAX]   = "";

int master_port           = 9306;
int slave_port            = 9307;

// comma delimited list of tests to skip or empty string
char skip_test[PATH_MAX]  = " lowercase_table3 , system_mysql_db_fix ";
char ignore_test[PATH_MAX]  = "";

char bin_dir[PATH_MAX];
char mysql_test_dir[PATH_MAX];
char test_dir[PATH_MAX];
char mysql_tmp_dir[PATH_MAX];
char result_dir[PATH_MAX];
char master_dir[PATH_MAX];
char slave_dir[PATH_MAX];
char lang_dir[PATH_MAX];
char char_dir[PATH_MAX];

char mysqladmin_file[PATH_MAX];
char mysqld_file[PATH_MAX];
char mysqltest_file[PATH_MAX];
char master_pid[PATH_MAX];
char slave_pid[PATH_MAX];

char master_opt[PATH_MAX] = "";
char slave_opt[PATH_MAX]  = "";

char slave_master_info[PATH_MAX]  = "";

char master_init_script[PATH_MAX]  = "";
char slave_init_script[PATH_MAX]  = "";

// OpenSSL
char ca_cert[PATH_MAX];
char server_cert[PATH_MAX];
char server_key[PATH_MAX];
char client_cert[PATH_MAX];
char client_key[PATH_MAX];

int total_skip    = 0;
int total_pass    = 0;
int total_fail    = 0;
int total_test    = 0;

int total_ignore  = 0;
double total_time = 0;

int use_openssl     = FALSE;
int master_running  = FALSE;
int slave_running   = FALSE;
int skip_slave      = TRUE;
int single_test     = TRUE;

int restarts  = 0;

FILE *log_fd  = NULL;

/******************************************************************************

  functions
  
******************************************************************************/

/******************************************************************************

  prototypes
  
******************************************************************************/

void report_stats();
void install_db(char *);
void mysql_install_db();
void start_master();
void start_slave();
void mysql_start();
void stop_slave();
void stop_master();
void mysql_stop();
void mysql_restart();
int read_option(char *, char *);
void run_test(char *);
void setup(char *);
void vlog(char *, va_list);
void log_msg(char *, ...);
void log_info(char *, ...);
void log_error(char *, ...);
void log_errno(char *, ...);
void die(char *);
char *str_tok(char *string, const char *delim);

/******************************************************************************

  report_stats()
  
  Report the gathered statistics.

******************************************************************************/
void report_stats()
{
  if (total_fail == 0)
  {
    log_msg("\nAll %d test(s) were successful.\n", total_test);
  }
  else
  {
    double percent = ((double)total_pass / total_test) * 100;
    
    log_msg("\nFailed %u/%u test(s), %.02f%% successful.\n",
      total_fail, total_test, percent);
		log_msg("\nThe .out and .err files in %s may give you some\n", result_dir);
		log_msg("hint of what went wrong.\n");
		log_msg("\nIf you want to report this error, please first read the documentation\n");
		log_msg("at: http://www.mysql.com/doc/en/MySQL_test_suite.html\n");
  }

  log_msg("\n%.02f total minutes elapsed in the test cases\n\n", total_time / 60);
}

/******************************************************************************

  install_db()
  
  Install the a database.

******************************************************************************/
void install_db(char *datadir)
{
  arg_list_t al;
  int err, i;
  char input[PATH_MAX];
  char output[PATH_MAX];
  char error[PATH_MAX];

  // input file
  snprintf(input, PATH_MAX, "%s/bin/test_db.sql", base_dir);
  snprintf(output, PATH_MAX, "%s/install.out", datadir);
  snprintf(error, PATH_MAX, "%s/install.err", datadir);
  
  // args
  init_args(&al);
  add_arg(&al, mysqld_file);
  add_arg(&al, "--no-defaults");
  add_arg(&al, "--bootstrap");
  add_arg(&al, "--skip-grant-tables");
  add_arg(&al, "--basedir=%s", base_dir);
  add_arg(&al, "--datadir=%s", datadir);
  add_arg(&al, "--skip-innodb");
  add_arg(&al, "--skip-bdb");

  // spawn
  if ((err = spawn(mysqld_file, &al, TRUE, input, output, error)) != 0)
  {
    die("Unable to create database.");
  }
  
  // free args
  free_args(&al);
}

/******************************************************************************

  mysql_install_db()
  
  Install the test databases.

******************************************************************************/
void mysql_install_db()
{
  char temp[PATH_MAX];
  
  // var directory
  snprintf(temp, PATH_MAX, "%s/var", mysql_test_dir);
  
  // clean up old direcotry
  del_tree(temp);
  
  // create var directory
  mkdir(temp, S_IRWXU);
  
  // create subdirectories
  log_msg("Creating test-suite folders...\n");
  snprintf(temp, PATH_MAX, "%s/var/run", mysql_test_dir);
  mkdir(temp, S_IRWXU);
  snprintf(temp, PATH_MAX, "%s/var/tmp", mysql_test_dir);
  mkdir(temp, S_IRWXU);
  snprintf(temp, PATH_MAX, "%s/var/master-data", mysql_test_dir);
  mkdir(temp, S_IRWXU);
  snprintf(temp, PATH_MAX, "%s/var/master-data/mysql", mysql_test_dir);
  mkdir(temp, S_IRWXU);
  snprintf(temp, PATH_MAX, "%s/var/master-data/test", mysql_test_dir);
  mkdir(temp, S_IRWXU);
  snprintf(temp, PATH_MAX, "%s/var/slave-data", mysql_test_dir);
  mkdir(temp, S_IRWXU);
  snprintf(temp, PATH_MAX, "%s/var/slave-data/mysql", mysql_test_dir);
  mkdir(temp, S_IRWXU);
  snprintf(temp, PATH_MAX, "%s/var/slave-data/test", mysql_test_dir);
  mkdir(temp, S_IRWXU);

  // install databases
  log_msg("Creating test databases for master... \n");
  install_db(master_dir);
  log_msg("Creating test databases for slave... \n");
  install_db(slave_dir);
}

/******************************************************************************

  start_master()
  
  Start the master server.

******************************************************************************/
void start_master()
{
  arg_list_t al;
  int err, i;
  char master_out[PATH_MAX];
  char master_err[PATH_MAX];
  char temp[PATH_MAX], temp2[PATH_MAX];

  // remove old berkeley db log files that can confuse the server
  removef("%s/log.*", master_dir);

  // remove stale binary logs
  removef("%s/var/log/*-bin.*", mysql_test_dir);

  // remove stale binary logs
  removef("%s/var/log/*.index", mysql_test_dir);

  // remove master.info file
  removef("%s/master.info", master_dir);

  // remove relay files
  removef("%s/var/log/*relay*", mysql_test_dir);

  // remove relay-log.info file
  removef("%s/relay-log.info", master_dir);

  // init script
  if (master_init_script[0] != NULL)
  {
    // run_init_script(master_init_script);

    // TODO: use the scripts
    if (strindex(master_init_script, "repair_part2-master.sh") != NULL)
    {
      FILE *fp;

      // create an empty index file
      snprintf(temp, PATH_MAX, "%s/test/t1.MYI", master_dir);
      fp = fopen(temp, "wb+");

      fputs("1", fp);

      fclose(fp);
    }

  }

  // redirection files
  snprintf(master_out, PATH_MAX, "%s/var/run/master%u.out",
           mysql_test_dir, restarts);
  snprintf(master_err, PATH_MAX, "%s/var/run/master%u.err",
           mysql_test_dir, restarts);

  snprintf(temp2,PATH_MAX,"%s/var",mysql_test_dir);
  mkdir(temp2,0);
  snprintf(temp2,PATH_MAX,"%s/var/log",mysql_test_dir);
  mkdir(temp2,0);

  // args
  init_args(&al);
  add_arg(&al, "%s", mysqld_file);
  add_arg(&al, "--no-defaults");
  add_arg(&al, "--log-bin=%s/var/log/master-bin",mysql_test_dir);
  add_arg(&al, "--server-id=1");
  add_arg(&al, "--basedir=%s", base_dir);
  add_arg(&al, "--port=%u", master_port);
  add_arg(&al, "--local-infile");
  add_arg(&al, "--core");
  add_arg(&al, "--datadir=%s", master_dir);
  add_arg(&al, "--pid-file=%s", master_pid);
  add_arg(&al, "--character-sets-dir=%s", char_dir);
  add_arg(&al, "--tmpdir=%s", mysql_tmp_dir);
  add_arg(&al, "--language=%s", lang_dir);
  add_arg(&al, "--log-bin-trust-routine-creators");
#ifdef DEBUG	//only for debug builds
  add_arg(&al, "--debug");
#endif

  if (use_openssl)
  {
    add_arg(&al, "--ssl-ca=%s", ca_cert);
    add_arg(&al, "--ssl-cert=%s", server_cert);
    add_arg(&al, "--ssl-key=%s", server_key);
  }

  // $MASTER_40_ARGS
  add_arg(&al, "--rpl-recovery-rank=1");
  add_arg(&al, "--init-rpl-role=master");

  // $SMALL_SERVER
  add_arg(&al, "-O");
  add_arg(&al, "key_buffer_size=1M");
  add_arg(&al, "-O");
  add_arg(&al, "sort_buffer=256K");
  add_arg(&al, "-O");
  add_arg(&al, "max_heap_table_size=1M");

  // $EXTRA_MASTER_OPT
  if (master_opt[0] != NULL)
  {
    char *p;

    p = (char *)str_tok(master_opt, " \t");
    if (!strstr(master_opt, "timezone"))
    {
      while (p)
      {
        add_arg(&al, "%s", p);
        p = (char *)str_tok(NULL, " \t");
      }
    }
  }

  // remove the pid file if it exists
  remove(master_pid);

  // spawn
  if ((err= spawn(mysqld_file, &al, FALSE, NULL, master_out, master_err)) == 0)
  {
    sleep_until_file_exists(master_pid);

	if ((err = wait_for_server_start(bin_dir, user, password, master_port,
                                         mysql_tmp_dir)) == 0)
    {
      master_running = TRUE;
    }
    else
    {
	  log_error("The master server went down early.");
    }
  }
  else
  {
    log_error("Unable to start master server.");
  }

  // free_args
  free_args(&al);
}

/******************************************************************************

  start_slave()
  
  Start the slave server.

******************************************************************************/
void start_slave()
{
  arg_list_t al;
  int err, i;
  char slave_out[PATH_MAX];
  char slave_err[PATH_MAX];
  char temp[PATH_MAX];
  
  // skip?
  if (skip_slave) return;

  // remove stale binary logs
  removef("%s/*-bin.*", slave_dir);

  // remove stale binary logs
  removef("%s/*.index", slave_dir);

  // remove master.info file
  removef("%s/master.info", slave_dir);

  // remove relay files
  removef("%s/var/log/*relay*", mysql_test_dir);

  // remove relay-log.info file
  removef("%s/relay-log.info", slave_dir);

  // init script
  if (slave_init_script[0] != NULL)
  {
    // run_init_script(slave_init_script);
    
    // TODO: use the scripts
    if (strindex(slave_init_script, "rpl000016-slave.sh") != NULL)
    {
      // create empty master.info file
      snprintf(temp, PATH_MAX, "%s/master.info", slave_dir);
      close(open(temp, O_WRONLY | O_CREAT,S_IRWXU|S_IRWXG|S_IRWXO));
    }
    else if (strindex(slave_init_script, "rpl000017-slave.sh") != NULL)
    {
      FILE *fp;
      
      // create a master.info file
      snprintf(temp, PATH_MAX, "%s/master.info", slave_dir);
      fp = fopen(temp, "wb+");
      
      fputs("master-bin.000001\n", fp);
      fputs("4\n", fp);
      fputs("127.0.0.1\n", fp);
      fputs("replicate\n", fp);
      fputs("aaaaaaaaaaaaaaab\n", fp);
      fputs("9306\n", fp);
      fputs("1\n", fp);
      fputs("0\n", fp);

      fclose(fp);
    }
    else if (strindex(slave_init_script, "rpl_rotate_logs-slave.sh") != NULL)
    {
      // create empty master.info file
      snprintf(temp, PATH_MAX, "%s/master.info", slave_dir);
      close(open(temp, O_WRONLY | O_CREAT,S_IRWXU|S_IRWXG|S_IRWXO));
    }
  }

  // redirection files
  snprintf(slave_out, PATH_MAX, "%s/var/run/slave%u.out",
           mysql_test_dir, restarts);
  snprintf(slave_err, PATH_MAX, "%s/var/run/slave%u.err",
           mysql_test_dir, restarts);
  
  // args
  init_args(&al);
  add_arg(&al, "%s", mysqld_file);
  add_arg(&al, "--no-defaults");
  add_arg(&al, "--log-bin=slave-bin");
  add_arg(&al, "--relay_log=slave-relay-bin");
  add_arg(&al, "--basedir=%s", base_dir);
  add_arg(&al, "--port=%u", slave_port);
  add_arg(&al, "--datadir=%s", slave_dir);
  add_arg(&al, "--pid-file=%s", slave_pid);
  add_arg(&al, "--character-sets-dir=%s", char_dir);
  add_arg(&al, "--core");
  add_arg(&al, "--tmpdir=%s", mysql_tmp_dir);
  add_arg(&al, "--language=%s", lang_dir);

  add_arg(&al, "--exit-info=256");
  add_arg(&al, "--log-slave-updates");
  add_arg(&al, "--init-rpl-role=slave");
  add_arg(&al, "--skip-innodb");
  add_arg(&al, "--skip-slave-start");
  add_arg(&al, "--slave-load-tmpdir=../../var/tmp");
  
  add_arg(&al, "--report-user=%s", user);
	add_arg(&al, "--report-host=127.0.0.1");
  add_arg(&al, "--report-port=%u", slave_port);

  add_arg(&al, "--master-retry-count=10");
  add_arg(&al, "-O");
  add_arg(&al, "slave_net_timeout=10");
  add_arg(&al, "--log-bin-trust-routine-creators");
#ifdef DEBUG	//only for debug builds
  add_arg(&al, "--debug");
#endif

  if (use_openssl)
  {
    add_arg(&al, "--ssl-ca=%s", ca_cert);
    add_arg(&al, "--ssl-cert=%s", server_cert);
    add_arg(&al, "--ssl-key=%s", server_key);
  }

  // slave master info
  if (slave_master_info[0] != NULL)
  {
    char *p;

    p = (char *)str_tok(slave_master_info, " \t");

    while(p)
    {
      add_arg(&al, "%s", p);
      
      p = (char *)str_tok(NULL, " \t");
    }
  }
  else
  {
    add_arg(&al, "--master-user=%s", user);
    add_arg(&al, "--master-password=%s", password);
    add_arg(&al, "--master-host=127.0.0.1");
    add_arg(&al, "--master-port=%u", master_port);
    add_arg(&al, "--master-connect-retry=1");
    add_arg(&al, "--server-id=2");
    add_arg(&al, "--rpl-recovery-rank=2");
  }
  
  // small server
  add_arg(&al, "-O");
  add_arg(&al, "key_buffer_size=1M");
  add_arg(&al, "-O");
  add_arg(&al, "sort_buffer=256K");
  add_arg(&al, "-O");
  add_arg(&al, "max_heap_table_size=1M");

  // opt args
  if (slave_opt[0] != NULL)
  {
    char *p;

    p = (char *)str_tok(slave_opt, " \t");

    while(p)
    {
      add_arg(&al, "%s", p);
      
      p = (char *)str_tok(NULL, " \t");
    }
  }
  
  // remove the pid file if it exists
  remove(slave_pid);

  // spawn
  if ((err = spawn(mysqld_file, &al, FALSE, NULL, slave_out, slave_err)) == 0)
  {
    sleep_until_file_exists(slave_pid);
    
    if ((err = wait_for_server_start(bin_dir, user, password, slave_port,
                                     mysql_tmp_dir)) == 0)
    {
      slave_running = TRUE;

    }
    else
    {
      log_error("The slave server went down early.");

    }
  }
  else
  {
    log_error("Unable to start slave server.");

  }
  
  // free args
  free_args(&al);
}

/******************************************************************************

  mysql_start()
  
  Start the mysql servers.

******************************************************************************/
void mysql_start()
{
  log_info("Starting the MySQL server(s): %u", ++restarts);
  start_master();

  start_slave();

  // activate the test screen
  ActivateScreen(getscreenhandle());
}

/******************************************************************************

  stop_slave()

  Stop the slave server.

******************************************************************************/
void stop_slave()
{
  int err;

  // running?
  if (!slave_running) return;

  // stop
  if ((err = stop_server(bin_dir, user, password, slave_port, slave_pid,
                         mysql_tmp_dir)) == 0)
  {
    slave_running = FALSE;
  }
  else
  {
    log_error("Unable to stop slave server.");
  }
}

/******************************************************************************

  stop_master()

  Stop the master server.

******************************************************************************/
void stop_master()
{
  int err;

  // running?
  if (!master_running) return;

  if ((err = stop_server(bin_dir, user, password, master_port, master_pid,
                         mysql_tmp_dir)) == 0)
  {
    master_running = FALSE;
  }
  else
  {
    log_error("Unable to stop master server.");
  }
}

/******************************************************************************

  mysql_stop()

  Stop the mysql servers.

******************************************************************************/
void mysql_stop()
{
  log_info("Stopping the MySQL server(s)...");
  stop_master();

  stop_slave();

  // activate the test screen
  ActivateScreen(getscreenhandle());
}

/******************************************************************************

  mysql_restart()

  Restart the mysql servers.

******************************************************************************/
void mysql_restart()
{
  log_info("Restarting the MySQL server(s): %u", ++restarts);

  mysql_stop();

  mysql_start();
}

/******************************************************************************

  read_option()

  Read the option file.

******************************************************************************/
int read_option(char *opt_file, char *opt)
{
  int fd, err;
  int result;
  char *p;
  char buf[PATH_MAX];

  // copy current option
  strncpy(buf, opt, PATH_MAX);

  // open options file
  fd = open(opt_file, O_RDONLY);
  
  err = read(fd, opt, PATH_MAX);
  
  close(fd);
  
  if (err > 0)
  {
    // terminate string
    if ((p = strchr(opt, '\n')) != NULL)
    {
      *p = NULL;
      
      // check for a '\r'
      if ((p = strchr(opt, '\r')) != NULL)
      {
        *p = NULL;
      }
    }
    else
    {
      opt[err] = NULL;
    }

    // check for $MYSQL_TEST_DIR
    if ((p = strstr(opt, "$MYSQL_TEST_DIR")) != NULL)
    {
      char temp[PATH_MAX];
      
      *p = NULL;
      
      strcpy(temp, p + strlen("$MYSQL_TEST_DIR"));
      
      strcat(opt, mysql_test_dir);
      
      strcat(opt, temp);
    }
    // Check for double backslash and replace it with single bakslash
    if ((p = strstr(opt, "\\\\")) != NULL)
    {
      /* bmove is guranteed to work byte by byte */
      bmove(p, p+1, strlen(p+1));
    }
  }
  else
  {
    // clear option
    *opt = NULL;
  }
  
  // compare current option with previous
  return strcmp(opt, buf);
}

/******************************************************************************

  run_test()
  
  Run the given test case.

******************************************************************************/
void run_test(char *test)
{
  char temp[PATH_MAX];
  char *rstr;
  double elapsed = 0;
  int skip = FALSE, ignore=FALSE;
  int restart = FALSE;
  int flag = FALSE;
  struct stat info;
  
  // skip tests in the skip list
  snprintf(temp, PATH_MAX, " %s ", test);
  skip = (strindex(skip_test, temp) != NULL);
  if (skip == FALSE)
    ignore = (strindex(ignore_test, temp) != NULL);
    
  if (ignore)
  {
    // show test
    log_msg("%-46s ", test);
         
    // ignore
    rstr = TEST_IGNORE;
    ++total_ignore;
  }  
  else if (!skip)     // skip test?
  {
    char test_file[PATH_MAX];
    char master_opt_file[PATH_MAX];
    char slave_opt_file[PATH_MAX];
    char slave_master_info_file[PATH_MAX];
    char result_file[PATH_MAX];
    char reject_file[PATH_MAX];
    char out_file[PATH_MAX];
    char err_file[PATH_MAX];
    int err;
    arg_list_t al;
    NXTime_t start, stop;
    
    // skip slave?
    flag = skip_slave;
    skip_slave = (strncmp(test, "rpl", 3) != 0);
    if (flag != skip_slave) restart = TRUE;
    
    // create files
    snprintf(master_opt_file, PATH_MAX, "%s/%s-master.opt", test_dir, test);
    snprintf(slave_opt_file, PATH_MAX, "%s/%s-slave.opt", test_dir, test);
    snprintf(slave_master_info_file, PATH_MAX, "%s/%s.slave-mi", test_dir, test);
    snprintf(reject_file, PATH_MAX, "%s/%s%s", result_dir, test, REJECT_SUFFIX);
    snprintf(out_file, PATH_MAX, "%s/%s%s", result_dir, test, OUT_SUFFIX);
    snprintf(err_file, PATH_MAX, "%s/%s%s", result_dir, test, ERR_SUFFIX);
    
    // netware specific files
    snprintf(test_file, PATH_MAX, "%s/%s%s", test_dir, test, NW_TEST_SUFFIX);
    if (stat(test_file, &info))
    {
      snprintf(test_file, PATH_MAX, "%s/%s%s", test_dir, test, TEST_SUFFIX);
      if (access(test_file,0))
      {
        printf("Invalid test name %s, %s file not found\n",test,test_file);
        return;
      }
    }

    snprintf(result_file, PATH_MAX, "%s/%s%s", result_dir, test, NW_RESULT_SUFFIX);
    if (stat(result_file, &info))
    {
      snprintf(result_file, PATH_MAX, "%s/%s%s", result_dir, test, RESULT_SUFFIX);
    }

    // init scripts
    snprintf(master_init_script, PATH_MAX, "%s/%s-master.sh", test_dir, test);
    if (stat(master_init_script, &info))
      master_init_script[0] = NULL;
    else
      restart = TRUE;
    
    snprintf(slave_init_script, PATH_MAX, "%s/%s-slave.sh", test_dir, test);
    if (stat(slave_init_script, &info))
      slave_init_script[0] = NULL;
    else
      restart = TRUE;

    // read options
    if (read_option(master_opt_file, master_opt)) restart = TRUE;
    if (read_option(slave_opt_file, slave_opt)) restart = TRUE;
    if (read_option(slave_master_info_file, slave_master_info)) restart = TRUE;
    
    // cleanup previous run
    remove(reject_file);
    remove(out_file);
    remove(err_file);
    
    // start or restart?
    if (!master_running) mysql_start();
      else if (restart) mysql_restart();
    
    // let the system stabalize
    sleep(1);

    // show test
    log_msg("%-46s ", test);
    
    // args
    init_args(&al);
    add_arg(&al, "%s", mysqltest_file);
    add_arg(&al, "--no-defaults");
    add_arg(&al, "--port=%u", master_port);
    add_arg(&al, "--database=%s", db);
    add_arg(&al, "--user=%s", user);
    add_arg(&al, "--password=%s", password);
    add_arg(&al, "--silent");
    add_arg(&al, "--basedir=%s/", mysql_test_dir);
    add_arg(&al, "--host=127.0.0.1");
    add_arg(&al, "-v");
    add_arg(&al, "-R");
    add_arg(&al, "%s", result_file);

    if (use_openssl)
    {
      add_arg(&al, "--ssl-ca=%s", ca_cert);
      add_arg(&al, "--ssl-cert=%s", client_cert);
      add_arg(&al, "--ssl-key=%s", client_key);
    }

    // start timer
    NXGetTime(NX_SINCE_BOOT, NX_USECONDS, &start);
    
    // spawn
    err = spawn(mysqltest_file, &al, TRUE, test_file, out_file, err_file);
    
    // stop timer
    NXGetTime(NX_SINCE_BOOT, NX_USECONDS, &stop);
    
    // calculate
    elapsed = ((double)(stop - start)) / NX_USECONDS;
    total_time += elapsed;
    
    // free args
    free_args(&al);
    
    if (err == 0)
    {
      // pass
      rstr = TEST_PASS;
      ++total_pass;
      
      // increment total
      ++total_test;
    }
    else if (err == 62)
    {
      // skip
      rstr = TEST_SKIP;
      ++total_skip;
    }
    else if (err == 1)
    {
      // fail
      rstr = TEST_FAIL;
      ++total_fail;
      
      // increment total
      ++total_test;
    }
    else
    {
      rstr = TEST_BAD;
    }
  }
  else // early skips
  {
    // show test
    log_msg("%-46s ", test);
    
    // skip
    rstr = TEST_SKIP;
    ++total_skip;
  }
  
  // result
  log_msg("%10.06f   %-14s\n", elapsed, rstr);
}

/******************************************************************************

  vlog()
  
  Log the message.

******************************************************************************/
void vlog(char *format, va_list ap)
{
  vfprintf(stdout, format, ap);
  fflush(stdout);
  
  if (log_fd)
  {
    vfprintf(log_fd, format, ap);
    fflush(log_fd);
  }
}

/******************************************************************************

  log()
  
  Log the message.

******************************************************************************/
void log_msg(char *format, ...)
{
  va_list ap;

  va_start(ap, format);

  vlog(format, ap);
  
  va_end(ap);
}

/******************************************************************************

  log_info()
  
  Log the given information.

******************************************************************************/
void log_info(char *format, ...)
{
  va_list ap;
  
  va_start(ap, format);

  log_msg("-- INFO : ");
  vlog(format, ap);
  log_msg("\n");

  va_end(ap);
}

/******************************************************************************

  log_error()
  
  Log the given error.

******************************************************************************/
void log_error(char *format, ...)
{
  va_list ap;
  
  va_start(ap, format);

  log_msg("-- ERROR: ");
  vlog(format, ap);
  log_msg("\n");

  va_end(ap);
}

/******************************************************************************

  log_errno()
  
  Log the given error and errno.

******************************************************************************/
void log_errno(char *format, ...)
{
  va_list ap;
  
  va_start(ap, format);

  log_msg("-- ERROR: (%003u) ", errno);
  vlog(format, ap);
  log_msg("\n");

  va_end(ap);
}

/******************************************************************************

  die()
  
  Exit the application.

******************************************************************************/
void die(char *msg)
{
  log_error(msg);

  pressanykey();

  exit(-1);
}

/******************************************************************************

  setup()
  
  Setup the mysql test enviornment.

******************************************************************************/
void setup(char *file)
{
  char temp[PATH_MAX];
  char file_path[PATH_MAX*2];
  char *p;

  // set the timezone for the timestamp test
  setenv("TZ", "GMT-3", TRUE);

  // find base dir
  strcpy(temp, strlwr(file));
  while((p = strchr(temp, '\\')) != NULL) *p = '/';
  
  if ((p = strindex(temp, "/mysql-test/")) != NULL)
  {
    *p = NULL;
    strcpy(base_dir, temp);
  }

  // setup paths
  snprintf(bin_dir, PATH_MAX, "%s/bin", base_dir);
  snprintf(mysql_test_dir, PATH_MAX, "%s/mysql-test", base_dir);
  snprintf(test_dir, PATH_MAX, "%s/t", mysql_test_dir);
  snprintf(mysql_tmp_dir, PATH_MAX, "%s/var/tmp", mysql_test_dir);
  snprintf(result_dir, PATH_MAX, "%s/r", mysql_test_dir);
  snprintf(master_dir, PATH_MAX, "%s/var/master-data", mysql_test_dir);
  snprintf(slave_dir, PATH_MAX, "%s/var/slave-data", mysql_test_dir);
  snprintf(lang_dir, PATH_MAX, "%s/share/english", base_dir);
  snprintf(char_dir, PATH_MAX, "%s/share/charsets", base_dir);

#ifdef HAVE_OPENSSL
  use_openssl = TRUE;
#endif // HAVE_OPENSSL

  // OpenSSL paths
  snprintf(ca_cert, PATH_MAX, "%s/SSL/cacert.pem", base_dir);
  snprintf(server_cert, PATH_MAX, "%s/SSL/server-cert.pem", base_dir);
  snprintf(server_key, PATH_MAX, "%s/SSL/server-key.pem", base_dir);
  snprintf(client_cert, PATH_MAX, "%s/SSL/client-cert.pem", base_dir);
  snprintf(client_key, PATH_MAX, "%s/SSL/client-key.pem", base_dir);

  // setup files
  snprintf(mysqld_file, PATH_MAX, "%s/mysqld", bin_dir);
  snprintf(mysqltest_file, PATH_MAX, "%s/mysqltest", bin_dir);
  snprintf(mysqladmin_file, PATH_MAX, "%s/mysqladmin", bin_dir);
  snprintf(master_pid, PATH_MAX, "%s/var/run/master.pid", mysql_test_dir);
  snprintf(slave_pid, PATH_MAX, "%s/var/run/slave.pid", mysql_test_dir);

  // create log file
  snprintf(temp, PATH_MAX, "%s/mysql-test-run.log", mysql_test_dir);
  if ((log_fd = fopen(temp, "w+")) == NULL)
  {
    log_errno("Unable to create log file.");
  }

  // prepare skip test list
  while((p = strchr(skip_test, ',')) != NULL) *p = ' ';
  strcpy(temp, strlwr(skip_test));
  snprintf(skip_test, PATH_MAX, " %s ", temp);

  // environment
  setenv("MYSQL_TEST_DIR", mysql_test_dir, 1);
  snprintf(file_path, PATH_MAX*2, "%s/mysqldump --no-defaults -u root --port=%u", bin_dir, master_port);
  setenv("MYSQL_DUMP", file_path, 1);
  snprintf(file_path, PATH_MAX*2, "%s/mysqlbinlog --no-defaults --local-load=%s", bin_dir, mysql_tmp_dir);
  setenv("MYSQL_BINLOG", file_path, 1);
  setenv("MASTER_MYPORT", "9306", 1);
  setenv("SLAVE_MYPORT", "9307", 1);
  setenv("MYSQL_TCP_PORT", "3306", 1);
  snprintf(file_path, PATH_MAX*2, "%s/mysql_client_test --no-defaults --testcase--user=root --port=%u ", bin_dir, master_port); 
  setenv("MYSQL_CLIENT_TEST",file_path,1);
  snprintf(file_path, PATH_MAX*2, "%s/mysql --no-defaults --user=root --port=%u ", bin_dir, master_port);
  setenv("MYSQL",file_path,1); 
  snprintf(file_path, PATH_MAX*2, "%s/mysqlshow --no-defaults --user=root --port=%u", bin_dir, master_port);
  setenv("MYSQL_SHOW",file_path,1);
}

/******************************************************************************

  main()
  
******************************************************************************/
int main(int argc, char **argv)
{
  int is_ignore_list = 0;
  // setup
  setup(argv[0]);
  
  /* The --ignore option is comma saperated list of test cases to skip and
     should be very first command line option to the test suite. 

     The usage is now:
     mysql_test_run --ignore=test1,test2 test3 test4
     where test1 and test2 are test cases to ignore
     and test3 and test4 are test cases to run.
  */
  if (argc >= 2 && !strnicmp(argv[1], "--ignore=", sizeof("--ignore=")-1))
  {
    char *temp, *token;
    temp= strdup(strchr(argv[1],'=') + 1);
    for (token=str_tok(temp, ","); token != NULL; token=str_tok(NULL, ","))
    {
      if (strlen(ignore_test) + strlen(token) + 2 <= PATH_MAX-1)
        sprintf(ignore_test+strlen(ignore_test), " %s ", token);
      else
      {
        free(temp);
        die("ignore list too long.");
      }
    }
    free(temp);
    is_ignore_list = 1;
  }
  // header
  log_msg("MySQL Server %s, for %s (%s)\n\n", VERSION, SYSTEM_TYPE, MACHINE_TYPE);
  
  log_msg("Initializing Tests...\n");
  
  // install test databases
  mysql_install_db();
  
  log_msg("Starting Tests...\n");
  
  log_msg("\n");
  log_msg(HEADER);
  log_msg(DASH);

  if ( argc > 1 + is_ignore_list )
  {
    int i;

    // single test
    single_test = TRUE;

    for (i = 1 + is_ignore_list; i < argc; i++)
    {
      // run given test
      run_test(argv[i]);
    }
  }
  else
  {
    // run all tests
    DIR *dir = opendir(test_dir);
    DIR *entry;
    char test[NAME_MAX];
    char *p;
    
    // single test
    single_test = FALSE;    

    if (dir == NULL)
    {
      die("Unable to open tests directory.");
    }
    
    while((entry = readdir(dir)) != NULL)
    {
      if (!S_ISDIR(entry->d_type))
      {
        strcpy(test, strlwr(entry->d_name));
        
        // find the test suffix
        if ((p = strindex(test, TEST_SUFFIX)) != NULL)
        {
          // null terminate at the suffix
          *p = '\0';

          // run test
          run_test(test);
        }
      }
    }
    
    closedir(dir);
  }

  // stop server
  mysql_stop();

  log_msg(DASH);
  log_msg("\n");

  log_msg("Ending Tests...\n");

  // report stats
  report_stats();

  // close log
  if (log_fd) fclose(log_fd);

  // keep results up
  pressanykey();

  return 0;
}

/*
 Synopsis:
  This function breaks the string into a sequence of tokens. The difference
  between this function and strtok is that it respects the quoted string i.e.
  it skips  any delimiter character within the quoted part of the string. 
  It return tokens by eliminating quote character. It modifies the input string
  passed. It will work with whitespace delimeter but may not work properly with
  other delimeter. If the delimeter will contain any quote character, then
  function will not tokenize and will return null string.
  e.g. if input string is 
     --init-slave="set global max_connections=500" --skip-external-locking
  then the output will two string i.e.
     --init-slave=set global max_connections=500
     --skip-external-locking

Arguments:
  string:  input string
  delim:   set of delimiter character
Output:
  return the null terminated token of NULL.
*/


char *str_tok(char *string, const char *delim)
{
  char *token;            /* current token received from strtok */
  char *qt_token;         /* token delimeted by the matching pair of quote */
  /*
    if there are any quote chars found in the token then this variable
    will hold the concatenated string to return to the caller
  */
  char *ptr_token=NULL;
  /* pointer to the quote character in the token from strtok */
  char *ptr_quote=NULL;
  
  /* See if the delimeter contains any quote character */
  if (strchr(delim,'\'') || strchr(delim,'\"'))
    return NULL;

  /* repeate till we are getting some token from strtok */
  while ((token = (char*)strtok(string, delim) ) != NULL)
  {
    /*
      make the input string NULL so that next time onward strtok can
      be called with NULL input string.
    */
    string = NULL;
    
    /* check if the current token contain double quote character*/
    if ((ptr_quote = (char*)strchr(token,'\"')) != NULL)
    {
      /*
        get the matching the matching double quote in the remaining
        input string
      */
      qt_token = (char*)strtok(NULL,"\"");
    }
    /* check if the current token contain single quote character*/
    else if ((ptr_quote = (char*)strchr(token,'\'')) != NULL)
    {
      /*
        get the matching the matching single quote in the remaining
        input string
      */
      qt_token = (char*)strtok(NULL,"\'");
    }

    /*
      if the current token does not contains any quote character then
      return to the caller.
    */
    if (ptr_quote == NULL)
    {
      /*
        if there is any earlier token i.e. ptr_token then append the
        current token in it and return it else return the current
        token directly
      */
      return ptr_token ? strcat(ptr_token,token) : token;
    }

    /*
      remove the quote character i.e. make NULL so that the token will
      be devided in two part and later both part can be concatenated
      and hence quote will be removed
    */
    *ptr_quote= 0;
    
    /* check if ptr_token has been initialized or not */
    if (ptr_token == NULL)
    {
      /* initialize the ptr_token with current token */
      ptr_token= token;
      /* copy entire string between matching pair of quote*/
      sprintf(ptr_token+strlen(ptr_token),"%s %s", ptr_quote+1, qt_token);
    }
    else
    {
      /*
        copy the current token and entire string between matching pair
        of quote
      */
      sprintf(ptr_token+strlen(ptr_token),"%s%s %s", token, ptr_quote+1,
              qt_token );
    }
  }
  
  /* return the concatenated token */
  return ptr_token;
}
