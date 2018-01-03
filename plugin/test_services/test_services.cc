/* Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <fcntl.h>
#include <mysql/plugin.h>
#include <mysql_version.h>

#include "m_string.h"                           // strlen
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_io.h"
#include "my_sys.h"                             // my_write, my_malloc
//#include "sql_plugin.h"                         // st_plugin_int

#define STRING_BUFFER 100
  
File outfile;

/* Shows status of test (Busy/READY) */
enum t_test_status { BUSY= 0, READY= 1 };
static volatile t_test_status test_status;

/* declaration of status variable for plugin */
static struct st_mysql_show_var test_services_status[]=
{
  { "test_services_status",
    (char *) &test_status,
     SHOW_INT, SHOW_SCOPE_GLOBAL },
  {0, 0, SHOW_UNDEF, SHOW_SCOPE_GLOBAL}
};

/* SQL (system) variables to control test execution                     */
/* SQL (system) variables to switch on/off test of services, default=on */
/* Only be effective at start od mysqld by setting it as option --loose-...  */
static int     with_snprintf_val= 0;
static MYSQL_SYSVAR_INT  (with_snprintf, with_snprintf_val , PLUGIN_VAR_RQCMDARG, 
		"Switch on/off test of snprintf service", NULL, NULL, 1, 0, 1, 0);

static int     with_log_message_val= 0;
static MYSQL_SYSVAR_INT  (with_log_message, with_log_message_val, PLUGIN_VAR_RQCMDARG, 
		"Switch on/off test of log message service", NULL, NULL, 1, 0, 1, 0);

static int non_default_variable_value= 0;
static MYSQL_SYSVAR_INT(non_default_variable, non_default_variable_value,
                        PLUGIN_VAR_NOCMDOPT | PLUGIN_VAR_NODEFAULT,
                        "A variable that won't accept SET DEFAULT", NULL, NULL,
                        1, 0, 100, 0);

static struct st_mysql_sys_var *test_services_sysvars[]= {
  MYSQL_SYSVAR(with_snprintf),
  MYSQL_SYSVAR(with_log_message),
  MYSQL_SYSVAR(non_default_variable),
  NULL
};

/* The test cases for snprintf service.  */
static int test_snprintf()
{
  DBUG_ENTER("mysql_outfile");
  char filename[FN_REFLEN];
  char buffer[STRING_BUFFER];


  fn_format(filename, "test_services", "", ".log",
            MY_REPLACE_EXT | MY_UNPACK_FILENAME);
  unlink(filename);
  outfile= my_open(filename, O_CREAT|O_RDWR, MYF(0));

  my_snprintf(buffer, sizeof(buffer),
              "Starting test of my_snprintf in test_services.\n");
  my_write(outfile, (uchar*) buffer, strlen(buffer), MYF(0));

  my_snprintf(buffer, sizeof(buffer),
              "This is a text output formatted with my_snprintf.\n");
  my_write(outfile, (uchar*) buffer, strlen(buffer), MYF(0));
  
  my_snprintf(buffer, sizeof(buffer),
              "Shutting down test of my_snprintf in test_services.\n");
  my_write(outfile, (uchar*) buffer, strlen(buffer), MYF(0));

  my_close(outfile, MYF(0));
  
  DBUG_RETURN(0);
}

/* The test cases for the log_message service. */
static int test_my_plugin_log_message(void *p)
{
  DBUG_ENTER("my_plugin_log_message");
/* Writes to mysqld.1.err: Plugin test_services reports an info text */
  int ret = my_plugin_log_message(&p, MY_INFORMATION_LEVEL, "This is the test plugin for services testing info report output");

/* Writes to mysqld.1.err: Plugin test_services reports a warning. */
  ret = my_plugin_log_message(&p, MY_WARNING_LEVEL, "This is a warning from test plugin for services testing warning report output");

/* Writes to mysqld.1.err: Plugin test_services reports an error. */
  ret = my_plugin_log_message(&p, MY_ERROR_LEVEL, "This is an error from test plugin for services testing error report output");

  DBUG_RETURN(ret);
}

/* the tests of snprintf ans log_message run when INSTALL PLUGIN is called. */
static int test_services_plugin_init(void *p)
{
  DBUG_ENTER("test_services_plugin_init");

  int ret=0; 
  test_status= BUSY;
/* Test of service: snprintf */
  /* Log the value of the switch in mysqld.err. */
  ret = my_plugin_log_message(&p, MY_INFORMATION_LEVEL, "Test_services with_snprintf_val: %d", 
		               with_snprintf_val);
  if (with_snprintf_val==1){
     ret= test_snprintf();
  }
  else {
     ret = my_plugin_log_message(&p, MY_INFORMATION_LEVEL, "Test of snprintf switched off");
  }
/* Test of service: my_plugin_log_message */
  /* Log the value of the switch in mysqld.err. */
  ret = my_plugin_log_message(&p, MY_INFORMATION_LEVEL, "Test_services with_log_message_val: %d", 
		               with_log_message_val);
  if (with_log_message_val==1){
     ret=  test_my_plugin_log_message(p);
  }
  else {
     ret = my_plugin_log_message(&p, MY_INFORMATION_LEVEL, "Test of log_message switched off");
  }

  test_status= READY;
  	
  DBUG_RETURN(ret);
}

/* There is nothing to clean up when UNINSTALL PLUGIN. */
static int test_services_plugin_deinit(void*)
{
  DBUG_ENTER("test_services_plugin_deinit");
  DBUG_RETURN(0);
}

/* Mandatory structure describing the properties of the plugin. */
struct st_mysql_daemon test_services_plugin=
{ MYSQL_DAEMON_INTERFACE_VERSION  };

mysql_declare_plugin(test_daemon)
{
  MYSQL_DAEMON_PLUGIN,
  &test_services_plugin,
  "test_services",
  "Horst Hunger",
  "Test services",
  PLUGIN_LICENSE_GPL,
  test_services_plugin_init, /* Plugin Init */
  NULL, /* Plugin Check uninstall */
  test_services_plugin_deinit, /* Plugin Deinit */
  0x0100 /* 1.0 */,
  test_services_status,       /* status variables                */
  test_services_sysvars,      /* system variables                */
  NULL,                       /* config options                  */
  0,                          /* flags                           */
}
mysql_declare_plugin_end;
