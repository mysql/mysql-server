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


/* MySQL library headers */
#include <my_global.h>
#include <my_sys.h>
#include <my_dir.h>
#include <m_string.h>

/* These 'should' be POSIX or ANSI */
#include <stdio.h>
#include <stdarg.h>

#ifdef __NETWARE__
#include <screen.h>
#endif

#include "my_config.h"
#include "my_manage.h"

#ifdef __WIN__
#include <Shlwapi.h>
#endif


/******************************************************************************

  macros
  
******************************************************************************/

#define HEADER  "TEST                                           ELAPSED      RESULT      \n"
#define DASH    "------------------------------------------------------------------------\n"

#define NW_TEST_SUFFIX    ".nw-test"
#define NW_RESULT_SUFFIX  ".nw-result"
#define TEST_SUFFIX        ".test"
#define RESULT_SUFFIX      ".result"
#define REJECT_SUFFIX      ".reject"
#define OUT_SUFFIX        ".out"
#define ERR_SUFFIX        ".err"

#define TEST_PASS    "[ pass ]"
#define TEST_SKIP    "[ skip ]"
#define TEST_FAIL    "[ fail ]"
#define TEST_BAD    "[ bad  ]"

/******************************************************************************

  global variables
  
******************************************************************************/

char base_dir[PATH_MAX]   = "/mysql";
char db[PATH_MAX]         = "test";
char user[PATH_MAX]       = "root";
char password[PATH_MAX]   = "";

int master_port           = 9306;
int slave_port            = 9307;

/* comma delimited list of tests to skip or empty string */
char skip_test[PATH_MAX]  = "";

char bin_dir[PATH_MAX];
char mysql_test_dir[PATH_MAX];
char test_dir[PATH_MAX];
char mysql_tmp_dir[PATH_MAX];
char result_dir[PATH_MAX];
char master_dir[PATH_MAX];
char slave_dir[PATH_MAX];
char lang_dir[PATH_MAX];
char char_dir[PATH_MAX];

char mysqld_file[PATH_MAX];
char mysqltest_file[PATH_MAX];
char master_pid[PATH_MAX];
char slave_pid[PATH_MAX];

char master_opt[PATH_MAX] = "";
char slave_opt[PATH_MAX]  = "";

char slave_master_info[PATH_MAX]  = "";

char master_init_script[PATH_MAX]  = "";
char slave_init_script[PATH_MAX]  = "";

int total_skip    = 0;
int total_pass    = 0;
int total_fail    = 0;
int total_test    = 0;

double total_time = 0;

int master_running  = FALSE;
int slave_running   = FALSE;
int skip_slave      = TRUE;
int single_test     = TRUE;

int restarts  = 0;

FILE *log_fd  = NULL;
// WAX
#include <my_getopt.h>

const char* mysqld = "mysqld",  *opt_exedir="client_debug";

static struct my_option my_long_options[] =
{
  {"mysqld", 'M', "Type of mysqld (without extention of file)- mysqld, mysql-nt, mysql-nt-max, mysqld-max.",
  (gptr*) &mysqld, (gptr*) &mysqld, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"exefiledir", 'e', "Directory of .exe files (client_debug or client_release).", (gptr*) &opt_exedir,
  (gptr*) &opt_exedir, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
};


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
void vlog(const char *, va_list);
void mtr_log(const char *, ...);
void mtr_log_info(const char *, ...);
void mtr_log_error(const char *, ...);
void mtr_log_errno(const char *, ...);
void die(const char *);

/******************************************************************************

  report_stats()

  Report the gathered statistics.

******************************************************************************/
void report_stats()
{
  if (total_fail == 0)
  {
    mtr_log("\nAll %d test(s) were successful.\n", total_test);
  }
  else
  {
    double percent = ((double)total_pass / total_test) * 100;

    mtr_log("\nFailed %u/%u test(s), %.02f%% successful.\n",
      total_fail, total_test, percent);
    mtr_log("\nThe .out and .err files in %s may give you some\n", result_dir);
    mtr_log("hint of what when wrong.\n");
    mtr_log("\nIf you want to report this error, please first read the documentation\n");
    mtr_log("at: http://www.mysql.com/doc/M/y/MySQL_test_suite.html\n");
  }

  mtr_log("\n%.02f total minutes elapsed in the test cases\n\n", total_time / 60);
}

/******************************************************************************

  install_db()

  Install the a database.

******************************************************************************/
void install_db(char *datadir)
{
  arg_list_t al;
  int err;
  char input[PATH_MAX];
  char output[PATH_MAX];
  char error[PATH_MAX];
  
  // input file
  my_snprintf(input, PATH_MAX, "%s/bin/init_db.sql", base_dir);
  my_snprintf(output, PATH_MAX, "%s/install.out", datadir);
  my_snprintf(error, PATH_MAX, "%s/install.err", datadir);
  
  // args
  init_args(&al);
  add_arg(&al, mysqld_file);
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
  
  /* var directory */
  my_snprintf(temp, PATH_MAX, "%s/var", mysql_test_dir);
  
  /* clean up old directory */
  del_tree(temp);
  
  /* create var directory */
  my_mkdir(temp, 0700, MYF(MY_WME));
  
  /* create subdirectories */
  my_snprintf(temp, PATH_MAX, "%s/var/run", mysql_test_dir);
  my_mkdir(temp, 0700, MYF(MY_WME));
  my_snprintf(temp, PATH_MAX, "%s/var/tmp", mysql_test_dir);
  my_mkdir(temp, 0700, MYF(MY_WME));
  my_snprintf(temp, PATH_MAX, "%s/var/master-data", mysql_test_dir);
  my_mkdir(temp, 0700, MYF(MY_WME));
  my_snprintf(temp, PATH_MAX, "%s/var/master-data/mysql", mysql_test_dir);
  my_mkdir(temp, 0700, MYF(MY_WME));
  my_snprintf(temp, PATH_MAX, "%s/var/master-data/test", mysql_test_dir);
  my_mkdir(temp, 0700, MYF(MY_WME));
  my_snprintf(temp, PATH_MAX, "%s/var/slave-data", mysql_test_dir);
  my_mkdir(temp, 0700, MYF(MY_WME));
  my_snprintf(temp, PATH_MAX, "%s/var/slave-data/mysql", mysql_test_dir);
  my_mkdir(temp, 0700, MYF(MY_WME));
  my_snprintf(temp, PATH_MAX, "%s/var/slave-data/test", mysql_test_dir);
  my_mkdir(temp, 0700, MYF(MY_WME));

  /* install databases */
  install_db(master_dir);
  install_db(slave_dir);
}

/******************************************************************************

  start_master()
  
  Start the master server.

******************************************************************************/
void start_master()
{
  arg_list_t al;
  int err;
  char master_out[PATH_MAX];
  char master_err[PATH_MAX];
  
  /* remove old berkeley db log files that can confuse the server */
  removef("%s/log.*", master_dir);

  /* remove stale binary logs */
  removef("%s/*-bin.*", master_dir);

  /* remove stale binary logs */
  removef("%s/*.index", master_dir);

  /* remove master.info file */
  removef("%s/master.info", master_dir);

  /* remove relay files */
  removef("%s/var/log/*relay*", mysql_test_dir);

  /* remove relay-log.info file */
  removef("%s/relay-log.info", master_dir);
  
  /* redirection files */
  my_snprintf(master_out, PATH_MAX, "%s/var/run/master%u.out",
           mysql_test_dir, restarts);
  my_snprintf(master_err, PATH_MAX, "%s/var/run/master%u.err",
           mysql_test_dir, restarts);
  
  /* args */
  init_args(&al);
  add_arg(&al, "%s", mysqld_file);
  add_arg(&al, "--no-defaults");
  add_arg(&al, "--log-bin=master-bin");
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
  
  /* $MASTER_40_ARGS */
  add_arg(&al, "--rpl-recovery-rank=1");
  add_arg(&al, "--init-rpl-role=master");
  
  /* $SMALL_SERVER */
  add_arg(&al, "-O");
  add_arg(&al, "key_buffer_size=1M");
  add_arg(&al, "-O");
  add_arg(&al, "sort_buffer=256K");
  add_arg(&al, "-O");
  add_arg(&al, "max_heap_table_size=1M");

  /* $EXTRA_MASTER_OPT */
  if (master_opt[0] != '\0')
  {
    char *p;

    p = (char *)strtok(master_opt, " \t");

    while(p)
    {
      add_arg(&al, "%s", p);

      p = (char *)strtok(NULL, " \t");
    }
  }
  
  /* remove the pid file if it exists */
#ifndef __WIN__ 
  my_delete(master_pid, MYF(MY_WME));
#else
  pid_mode= MASTER_PID;
  run_server= TRUE;
#endif

  /* spawn */
  if ((err = spawn(mysqld_file, &al, FALSE, NULL, master_out, master_err)) == 0)
  {
    sleep_until_file_exists(master_pid);
    
    if ((err = wait_for_server_start(bin_dir, user, password, master_port)) == 0)
    {
      master_running = TRUE;
    }
    else
    {
    mtr_log_error("The master server went down early.");
    }
  }
  else
  {
    mtr_log_error("Unable to start master server.");
  }
  
  /* free_args */
  free_args(&al);
}

/******************************************************************************

  start_slave()
  
  Start the slave server.

******************************************************************************/
void start_slave()
{
  arg_list_t al;
  int err;
  char slave_out[PATH_MAX];
  char slave_err[PATH_MAX];
  char temp[PATH_MAX];
  
  /* skip? */
  if (skip_slave) return;

  /* remove stale binary logs */
  removef("%s/*-bin.*", slave_dir);

  /* remove stale binary logs */
  removef("%s/*.index", slave_dir);

  /* remove master.info file */
  removef("%s/master.info", slave_dir);

  /* remove relay files */
  removef("%s/var/log/*relay*", mysql_test_dir);

  /* remove relay-log.info file */
  removef("%s/relay-log.info", slave_dir);

  /* init script */
  if (slave_init_script[0] != '\0')
  {
    /* run_init_script(slave_init_script); */
    
    /* TODO: use the scripts */
    if (strstr(slave_init_script, "rpl000016-slave.sh") != NULL)
    {
      /* create empty master.info file */
      my_snprintf(temp, PATH_MAX, "%s/master.info", slave_dir);
      my_close(my_open(temp, O_WRONLY | O_CREAT, MYF(0)), MYF(0));
    }
    else if (strstr(slave_init_script, "rpl000017-slave.sh") != NullS)
    {
      FILE *fp;
      
      /* create a master.info file */
      my_snprintf(temp, PATH_MAX, "%s/master.info", slave_dir);
      fp = my_fopen(temp, (int)(O_WRONLY | O_BINARY | O_CREAT), MYF(MY_WME));
      
      fputs("master-bin.001\n", fp);
      fputs("4\n", fp);
      fputs("127.0.0.1\n", fp);
      fputs("replicate\n", fp);
      fputs("aaaaaaaaaaaaaaabthispartofthepasswordisnotused\n", fp);
      fputs("9306\n", fp);
      fputs("1\n", fp);
      fputs("0\n", fp);

      my_fclose(fp, MYF(MY_WME));
    }
    else if (strstr(slave_init_script, "rpl_rotate_logs-slave.sh") != NullS)
    {
      /* create empty master.info file */
      my_snprintf(temp, PATH_MAX, "%s/master.info", slave_dir);
      my_close(my_open(temp, O_WRONLY | O_CREAT, MYF(0)), MYF(0));
    }
  }

  /* redirection files */
  my_snprintf(slave_out, PATH_MAX, "%s/var/run/slave%u.out",
           mysql_test_dir, restarts);
  my_snprintf(slave_err, PATH_MAX, "%s/var/run/slave%u.err",
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

  /* slave master info */
  if (slave_master_info[0] != '\0')
  {
    char *p;

    p = (char *)strtok(slave_master_info, " \t");

    while(p)
    {
      add_arg(&al, "%s", p);
      
      p = (char *)strtok(NULL, " \t");
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
  
  /* small server */
  add_arg(&al, "-O");
  add_arg(&al, "key_buffer_size=1M");
  add_arg(&al, "-O");
  add_arg(&al, "sort_buffer=256K");
  add_arg(&al, "-O");
  add_arg(&al, "max_heap_table_size=1M");

  /* opt args */
  if (slave_opt[0] != '\0')
  {
    char *p;

    p = (char *)strtok(slave_opt, " \t");

    while(p)
    {
      add_arg(&al, "%s", p);
      
      p = (char *)strtok(NULL, " \t");
    }
  }

  /* remove the pid file if it exists */
#ifndef __WIN__
  my_delete(slave_pid, MYF(MY_WME));
#else
  pid_mode= SLAVE_PID;
  run_server= TRUE;
#endif

  // spawn
  if ((err = spawn(mysqld_file, &al, FALSE, NULL, slave_out, slave_err)) == 0)
  {
    sleep_until_file_exists(slave_pid);
    
    if ((err = wait_for_server_start(bin_dir, user, password, slave_port)) == 0)
    {
      slave_running = TRUE;
    }
    else
    {
      mtr_log_error("The slave server went down early.");
    }
  }
  else
  {
    mtr_log_error("Unable to start slave server.");
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
  start_master();

  start_slave();
  
#ifdef __NETWARE__
  // activate the test screen
  ActivateScreen(getscreenhandle());
#endif
}

/******************************************************************************

  stop_slave()
  
  Stop the slave server.

******************************************************************************/
void stop_slave()
{
  int err;
  
  /* running? */
  if (!slave_running) return;
#ifdef __WIN__
  pid_mode= SLAVE_PID;
#endif
  
  /* stop */
  if ((err = stop_server(bin_dir, user, password, slave_port, slave_pid)) == 0)
  {
    slave_running = FALSE;
  }
  else
  {
    mtr_log_error("Unable to stop slave server.");
  }
}

/******************************************************************************

  stop_master()
  
  Stop the master server.

******************************************************************************/
void stop_master()
{
  int err;
  
  /* running? */
  if (!master_running) return;
#ifdef __WIN__
  pid_mode= MASTER_PID;
#endif
  if ((err = stop_server(bin_dir, user, password, master_port, master_pid)) == 0)
  {
    master_running = FALSE;
  }
  else
  {
    mtr_log_error("Unable to stop master server.");
  }
}

/******************************************************************************

  mysql_stop()
  
  Stop the mysql servers.

******************************************************************************/
void mysql_stop()
{
  stop_master();

  stop_slave();
  
#ifdef __NETWARE__
  // activate the test screen
  ActivateScreen(getscreenhandle());
#endif
}

/******************************************************************************

  mysql_restart()
  
  Restart the mysql servers.

******************************************************************************/
void mysql_restart()
{
  mtr_log_info("Restarting the MySQL server(s): %u", ++restarts);

  mysql_stop();

  mysql_start();
}

/******************************************************************************

  read_option()
  
  Read the option file.

******************************************************************************/
int read_option(char *opt_file, char *opt)
{
  File fd;
  int err;
  char *p;
  char buf[PATH_MAX];
  
  /* copy current option */
  strncpy(buf, opt, PATH_MAX);
  
#ifdef __WIN__
  if (PathFileExistsA(opt_file)) 
  {
#endif

  /* open options file */
  fd = my_open(opt_file, O_RDONLY, MYF(MY_WME));
  
  err = my_read(fd, opt, PATH_MAX, MYF(MY_WME));
  
  my_close(fd, MYF(MY_WME));
  
  if (err > 0)
  {
    /* terminate string */
    if ((p = strchr(opt, '\n')) != NULL)
    {
      *p = '\0';
      
      /* check for a '\r' */
      if ((p = strchr(opt, '\r')) != NULL)
      {
        *p = '\0';
      }
    }
    else
    {
      opt[err] = '\0';
    }

    /* check for $MYSQL_TEST_DIR */
    if ((p = strstr(opt, "$MYSQL_TEST_DIR")) != NullS)
    {
      char temp[PATH_MAX];
      
      *p = '\0';
      
      strcpy(temp, p + strlen("$MYSQL_TEST_DIR"));
      
      strcat(opt, mysql_test_dir);
      
      strcat(opt, temp);
    }
  }
  else
  {
    /* clear option */
    *opt = '\0';
  }
#ifdef __WIN__
  }
#endif
  
  /* compare current option with previous */
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
  int skip = FALSE;
  int restart = FALSE;
  int flag = FALSE;
  struct stat info;

  /* single test? */
  if (!single_test)
  {
    // skip tests in the skip list
    my_snprintf(temp, PATH_MAX, " %s ", test);
    skip = strinstr(skip_test, temp);
  }
    
  /* skip test? */
  if (!skip)
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
    
    // skip slave?
    flag = skip_slave;
    skip_slave = (strncmp(test, "rpl", 3) != 0);
    if (flag != skip_slave) restart = TRUE;
    
    // create files
    my_snprintf(master_opt_file, PATH_MAX, "%s/%s-master.opt", test_dir, test);
    my_snprintf(slave_opt_file, PATH_MAX, "%s/%s-slave.opt", test_dir, test);
    my_snprintf(slave_master_info_file, PATH_MAX, "%s/%s.slave-mi", test_dir, test);
    my_snprintf(reject_file, PATH_MAX, "%s/%s%s", result_dir, test, REJECT_SUFFIX);
    my_snprintf(out_file, PATH_MAX, "%s/%s%s", result_dir, test, OUT_SUFFIX);
    my_snprintf(err_file, PATH_MAX, "%s/%s%s", result_dir, test, ERR_SUFFIX);
    
    // netware specific files
    my_snprintf(test_file, PATH_MAX, "%s/%s%s", test_dir, test, NW_TEST_SUFFIX);
    if (stat(test_file, &info))
    {
      my_snprintf(test_file, PATH_MAX, "%s/%s%s", test_dir, test, TEST_SUFFIX);
    }
    
    my_snprintf(result_file, PATH_MAX, "%s/%s%s", result_dir, test, NW_RESULT_SUFFIX);
    if (stat(result_file, &info))
    {
      my_snprintf(result_file, PATH_MAX, "%s/%s%s", result_dir, test, RESULT_SUFFIX);
    }
    
    // init scripts
    my_snprintf(master_init_script, PATH_MAX, "%s/%s-master.sh", test_dir, test);
    if (stat(master_init_script, &info))
      master_init_script[0] = '\0';
    else
      restart = TRUE;
    
    my_snprintf(slave_init_script, PATH_MAX, "%s/%s-slave.sh", test_dir, test);
    if (stat(slave_init_script, &info))
      slave_init_script[0] = '\0';
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
    mtr_log("%-46s ", test);
    
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
    
    // spawn
    err = spawn(mysqltest_file, &al, TRUE, test_file, out_file, err_file);
      
    // free args
    free_args(&al);
    
    if (err == 0)
    {
      // pass
      rstr = (char *)TEST_PASS;
      ++total_pass;
      
      // increment total
      ++total_test;
    }
    else if (err == 2)
    {
      // skip
      rstr = (char *)TEST_SKIP;
      ++total_skip;
    }
    else if (err == 1)
    {
      // fail
      rstr = (char *)TEST_FAIL;
      ++total_fail;
      
      // increment total
      ++total_test;
    }
    else
    {
      rstr = (char *)TEST_BAD;
    }
  }
  else // early skips
  {
    // show test
    mtr_log("%-46s ", test);
    
    // skip
    rstr = (char *)TEST_SKIP;
    ++total_skip;
  }
  
  // result
  mtr_log("   %-14s\n", rstr);
}

/******************************************************************************

  vlog()
  
  Log the message.

******************************************************************************/
void vlog(const char *format, va_list ap)
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

  mtr_log()
  
  Log the message.

******************************************************************************/
void mtr_log(const char *format, ...)
{
  va_list ap;
  
  va_start(ap, format);

  vlog(format, ap);
  
  va_end(ap);
}

/******************************************************************************

  mtr_log_info()
  
  Log the given information.

******************************************************************************/
void mtr_log_info(const char *format, ...)
{
  va_list ap;
  
  va_start(ap, format);

  mtr_log("-- INFO : ");
  vlog(format, ap);
  mtr_log("\n");

  va_end(ap);
}

/******************************************************************************

  mtr_log_error()
  
  Log the given error.

******************************************************************************/
void mtr_log_error(const char *format, ...)
{
  va_list ap;
  
  va_start(ap, format);

  mtr_log("-- ERROR: ");
  vlog(format, ap);
  mtr_log("\n");

  va_end(ap);
}

/******************************************************************************

  mtr_log_errno()
  
  Log the given error and errno.

******************************************************************************/
void mtr_log_errno(const char *format, ...)
{
  va_list ap;
  
  va_start(ap, format);

  mtr_log("-- ERROR: (%003u) ", errno);
  vlog(format, ap);
  mtr_log("\n");

  va_end(ap);
}

/******************************************************************************

  die()
  
  Exit the application.

******************************************************************************/
void die(const char *msg)
{
  mtr_log_error(msg);

#ifdef __NETWARE__
  pressanykey();
#endif

  exit(-1);
}

/******************************************************************************

  setup()
  
  Setup the mysql test enviornment.

******************************************************************************/
void setup(char *file)
{
  char temp[PATH_MAX];
  char *p;
  
  // set the timezone for the timestamp test
#ifdef __WIN__
  _putenv( "TZ=GMT-3" );
#else
  setenv("TZ", "GMT-3", TRUE);
#endif

  // find base dir
  strcpy(temp, file);
#ifndef __WIN__
  casedn_str(temp); 
#endif
  while((p = strchr(temp, '\\')) != NULL) *p = '/';
  
  if ((p = strstr(temp, "/mysql-test/")))
  {
    *p = '\0';
    strcpy(base_dir, (const char *)temp);
  }
  
  // setup paths
#ifdef __WIN__
  my_snprintf(bin_dir, PATH_MAX, "%s/%s", base_dir,opt_exedir);
#else
  my_snprintf(bin_dir, PATH_MAX, "%s/bin", base_dir);
#endif
  my_snprintf(mysql_test_dir, PATH_MAX, "%s/mysql-test", base_dir);
  my_snprintf(test_dir, PATH_MAX, "%s/t", mysql_test_dir);
  my_snprintf(mysql_tmp_dir, PATH_MAX, "%s/var/tmp", mysql_test_dir);
  my_snprintf(result_dir, PATH_MAX, "%s/r", mysql_test_dir);
  my_snprintf(master_dir, PATH_MAX, "%s/var/master-data", mysql_test_dir);
  my_snprintf(slave_dir, PATH_MAX, "%s/var/slave-data", mysql_test_dir);
  my_snprintf(lang_dir, PATH_MAX, "%s/share/english", base_dir);
  my_snprintf(char_dir, PATH_MAX, "%s/share/charsets", base_dir);
  
  // setup files
#ifdef __WIN__  
  my_snprintf(mysqld_file, PATH_MAX, "%s/%s.exe", bin_dir, mysqld);
  my_snprintf(mysqltest_file, PATH_MAX, "%s/mysqltest.exe", bin_dir);
  my_snprintf(mysqladmin_file, PATH_MAX, "%s/mysqladmin.exe", bin_dir);
  pid_mode= NOT_NEED_PID;
#else
  my_snprintf(mysqld_file, PATH_MAX, "%s/mysqld", bin_dir);
  my_snprintf(mysqltest_file, PATH_MAX, "%s/mysqltest", bin_dir);
  my_snprintf(mysqladmin_file, PATH_MAX, "%s/mysqladmin", bin_dir);
#endif
  my_snprintf(master_pid, PATH_MAX, "%s/var/run/master.pid", mysql_test_dir);
  my_snprintf(slave_pid, PATH_MAX, "%s/var/run/slave.pid", mysql_test_dir);

  // create log file
  my_snprintf(temp, PATH_MAX, "%s/mysql-test-run.log", mysql_test_dir);
  if ((log_fd = fopen(temp, "w+")) == NULL)
  {
    mtr_log_errno("Unable to create log file.");
  }
  
  // prepare skip test list
  while((p = strchr(skip_test, ',')) != NULL) *p = ' ';
  strcpy(temp, skip_test);
#ifndef __WIN__
  casedn_str(temp);
#endif
  my_snprintf(skip_test, PATH_MAX, " %s ", temp);
  
  // enviornment
#ifdef __WIN__
  my_snprintf(temp, PATH_MAX, "MYSQL_TEST_DIR=%s", mysql_test_dir);
  _putenv(temp);
#else
  setenv("MYSQL_TEST_DIR", mysql_test_dir, 1); 
#endif
}

static my_bool
get_one_option(int optid, const struct my_option *opt __attribute__((unused)),
	       char *argument)
{
  return(0);
}

/******************************************************************************

  main()
  
******************************************************************************/
int main(int argc, char **argv)
{
  int i;
  uint ui;

 
#ifdef __WIN__
  int ho_error;
  DWORD len= PATH_MAX;
  char current_dir[PATH_MAX] = "."; 

  if ((ho_error=handle_options(&argc, &argv, my_long_options, get_one_option)))
    exit(1);
  if (len= GetCurrentDirectory(&len,current_dir))
  {
    current_dir[len]= '\\';
    current_dir[len+1]='\0';
  };
  MY_INIT(current_dir); 
#endif
  // setup
#ifdef __WIN__
  setup(current_dir);
#else
  setup(argv[0]);
#endif  
  // header
  mtr_log("MySQL Server %s, for %s (%s)\n\n", VERSION, SYSTEM_TYPE, MACHINE_TYPE);
  
  mtr_log("Initializing Tests...\n");
  
  // install test databases
  mysql_install_db();
  
  mtr_log("Starting Tests...\n");
  
  mtr_log("\n");
  mtr_log(HEADER);
  mtr_log(DASH);

#ifdef __WIN__
  if (argc > 0)
#else
  if (argc > 1)
#endif
  {

    // single test
    single_test = TRUE;    

#ifdef __WIN__
    for (i = 0; i < argc; i++)
#else
    for (i = 1; i < argc; i++)
#endif
    {
      // run given test
      run_test(argv[i]);
    }
  }
  else
  {
    // run all tests
    MY_DIR *dir = my_dir(test_dir, MYF(MY_WME | MY_WANT_STAT));
    char test[NAME_MAX];
    char *p;
    
    // single test
    single_test = FALSE;    

    if (dir == NULL)
    {
      die("Unable to open tests directory.");
    }
    
    for (ui = 0; ui < dir->number_off_files; ui++)
    {
      if (!MY_S_ISDIR(dir->dir_entry[ui].mystat.st_mode))
      {
        strcpy(test, dir->dir_entry[ui].name);
#ifndef __WIN__
        casedn_str(test);
#endif        
        // find the test suffix
        if ((p = strstr(test, TEST_SUFFIX)))
        {
          // null terminate at the suffix
          *p = '\0';

          // run test
          run_test(test);
        }
      }
    }

    my_dirend(dir);
  }
  
  mtr_log(DASH);
  mtr_log("\n");
  
  mtr_log("Ending Tests...\n");

  // stop server
  mysql_stop();

  // report stats
  report_stats();
    
  // close log
  if (log_fd) fclose(log_fd);
  
#ifdef __NETWARE__
  // keep results up
  pressanykey();
#endif

  return 0;
}

