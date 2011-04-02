/***********************************************************************

Copyright (c) 2010, Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

***********************************************************************/

#include "memcached_mysql.h"
#include <stdlib.h>
#include <ctype.h>
#include <mysql_version.h>
#include "sql_plugin.h"


struct mysql_memcached_context
{
	pthread_t		memcached_thread;
	memcached_context_t	memcached_conf;
};

/** Some system configuration parameter passed to memcached, including
the name of our Memcached InnoDB engine to be loaded by memcached.
More configuration to be added */
static char*	mci_engine_library = NULL;
static char*	mci_address = NULL; 
static char*	mci_tcp_port = NULL;
static char*	mci_max_conn = NULL;

static MYSQL_SYSVAR_STR(engine_library, mci_engine_library,
  PLUGIN_VAR_READONLY | PLUGIN_VAR_MEMALLOC,
  "memcached engine library name", NULL, NULL, NULL);

static MYSQL_SYSVAR_STR(address, mci_address,
  PLUGIN_VAR_READONLY | PLUGIN_VAR_MEMALLOC,
  "memcached address", NULL, NULL, NULL);

static MYSQL_SYSVAR_STR(tcp_port, mci_tcp_port,
  PLUGIN_VAR_READONLY | PLUGIN_VAR_MEMALLOC,
  "memcached port", NULL, NULL, NULL);

static MYSQL_SYSVAR_STR(max_conn, mci_max_conn,
  PLUGIN_VAR_READONLY | PLUGIN_VAR_MEMALLOC,
  "max number of connection", NULL, NULL, NULL);

static struct st_mysql_sys_var *daemon_memcached_sys_var[] = {
  MYSQL_SYSVAR(engine_library),
  MYSQL_SYSVAR(address),
  MYSQL_SYSVAR(tcp_port),
  MYSQL_SYSVAR(max_conn),
  0
};

static int daemon_memcached_plugin_deinit(void *p)
{
	struct st_plugin_int* plugin = (struct st_plugin_int *)p;
	struct mysql_memcached_context* con = NULL;

	shutdown_server();

	sleep(2);

	con = (struct mysql_memcached_context*) (plugin->data);

	pthread_cancel(con->memcached_thread);

	if (con->memcached_conf.m_engine_library) {
		my_free(con->memcached_conf.m_engine_library);
	}

	my_free(con);

	mci_engine_library = NULL;

	return(0);
}

static int daemon_memcached_plugin_init(void *p)
{
	struct mysql_memcached_context*	con;
	pthread_attr_t			attr;
	struct st_plugin_int*		plugin = (struct st_plugin_int *)p;

	con = (mysql_memcached_context*)malloc(sizeof(*con));

	/* This is for testing purpose for now. Remove before final version */
	mci_engine_library = (char*) "innodb_engine.so";

	if (mci_engine_library) {
		con->memcached_conf.m_engine_library = (char*) malloc(
			strlen(opt_plugin_dir)
			+ strlen(mci_engine_library) + 1);
		strxmov(con->memcached_conf.m_engine_library, opt_plugin_dir,
			FN_DIRSEP, mci_engine_library, NullS);
	} else {
		con->memcached_conf.m_engine_library = NULL;
	}

	con->memcached_conf.m_address = mci_address; 
	con->memcached_conf.m_tcp_port = mci_tcp_port;
	con->memcached_conf.m_max_conn = mci_max_conn;
	con->memcached_conf.m_innodb_api_cb = plugin->data;

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

	/* now create the thread */
	if (pthread_create(&con->memcached_thread, &attr,
			   daemon_memcached_main,
			   (void *)&con->memcached_conf) != 0)
	{
		fprintf(stderr,"Could not create memcached daemon thread!\n");
		exit(0);
	}

	plugin->data= (void *)con;

	return(0);
}

struct st_mysql_daemon daemon_memcached_plugin =
	{MYSQL_DAEMON_INTERFACE_VERSION};

mysql_declare_plugin(daemon_memcached)
{
	MYSQL_DAEMON_PLUGIN,
	&daemon_memcached_plugin,
	"daemon_memcached",
	"Jimmy Yang",
	"Memcached Daemon",
	PLUGIN_LICENSE_GPL,
	daemon_memcached_plugin_init,	/* Plugin Init */
	daemon_memcached_plugin_deinit,	/* Plugin Deinit */
	0x0100 /* 1.0 */,
	NULL,				/* status variables */
	daemon_memcached_sys_var,	/* system variables */
	NULL				/* config options */
}
mysql_declare_plugin_end;
