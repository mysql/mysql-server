/***********************************************************************

Copyright (c) 2011, 2017, Oracle and/or its affiliates. All rights reserved.

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

***********************************************************************/

/**************************************************//**
@file memcached_mysql.cc
InnoDB Memcached Plugin

Created 04/12/2011 Jimmy Yang
*******************************************************/

#include "memcached_mysql.h"
#include <stdlib.h>
#include <ctype.h>
#include <mysql_version.h>
#include "plugin.h"
#include "sql/sql_plugin.h"

/** Configuration info passed to memcached, including
the name of our Memcached InnoDB engine and memcached configure
string to be loaded by memcached. */
struct mysql_memcached_context
{
	pthread_t		memcached_thread;
	memcached_context_t	memcached_conf;
};

/** Variables for configure options */
static char*	mci_engine_library = NULL;
static char*	mci_eng_lib_path = NULL;
static char*	mci_memcached_option = NULL;
static unsigned int mci_r_batch_size = 1048576;
static unsigned int mci_w_batch_size = 32;
static bool	mci_enable_binlog = false;

static MYSQL_SYSVAR_STR(engine_lib_name, mci_engine_library,
			PLUGIN_VAR_READONLY | PLUGIN_VAR_MEMALLOC |
                        PLUGIN_VAR_NOPERSIST,
			"memcached engine library name", NULL, NULL,
			"innodb_engine.so");

static MYSQL_SYSVAR_STR(engine_lib_path, mci_eng_lib_path,
			PLUGIN_VAR_READONLY | PLUGIN_VAR_MEMALLOC |
                        PLUGIN_VAR_NOPERSIST,
			"memcached engine library path", NULL, NULL, NULL);

static MYSQL_SYSVAR_STR(option, mci_memcached_option,
			PLUGIN_VAR_READONLY | PLUGIN_VAR_MEMALLOC |
                        PLUGIN_VAR_NOPERSIST,
			"memcached option string", NULL, NULL, NULL);

static MYSQL_SYSVAR_UINT(r_batch_size, mci_r_batch_size,
			 PLUGIN_VAR_READONLY,
			 "read batch commit size", 0, 0, 1,
			 1, 1073741824, 0);

static MYSQL_SYSVAR_UINT(w_batch_size, mci_w_batch_size,
			 PLUGIN_VAR_READONLY,
			 "write batch commit size", 0, 0, 1,
			 1, 1048576, 0);

static MYSQL_SYSVAR_BOOL(enable_binlog, mci_enable_binlog,
			 PLUGIN_VAR_READONLY,
			 "whether to enable binlog",
			 NULL, NULL, FALSE);

static struct st_mysql_sys_var *daemon_memcached_sys_var[] = {
	MYSQL_SYSVAR(engine_lib_name),
	MYSQL_SYSVAR(engine_lib_path),
	MYSQL_SYSVAR(option),
	MYSQL_SYSVAR(r_batch_size),
	MYSQL_SYSVAR(w_batch_size),
	MYSQL_SYSVAR(enable_binlog),
	0
};

static int daemon_memcached_plugin_deinit(void *p)
{
	struct st_plugin_int*		plugin = (struct st_plugin_int *)p;
	struct mysql_memcached_context*	con = NULL;
	int				loop_count = 0;

        /* If memcached plugin is still initializing, wait for a
        while.*/
	while (!init_complete() && loop_count < 15 ) {
                sleep(1);
                loop_count++;
	}

        if (!init_complete()) {
		fprintf(stderr," InnoDB_Memcached: Memcached plugin is still"
			" initializing. Can't shut down it.\n");
                return(0);
        }

	loop_count = 0;
	if (!shutdown_complete()) {
		shutdown_server();
	}

        loop_count = 0;
	while (!shutdown_complete() && loop_count < 25) {
		sleep(2);
		loop_count++;
	}

	if(!shutdown_complete()) {
		fprintf(stderr," InnoDB_Memcached: Waited for 50 seconds"
			" for memcached thread to exit. Now force terminating"
			" the thread\n");
	}

	con = (struct mysql_memcached_context*) (plugin->data);

	pthread_cancel(con->memcached_thread);

	if (con->memcached_conf.m_engine_library) {
		my_free(con->memcached_conf.m_engine_library);
	}

	my_free(con);

	return(0);
}

static int daemon_memcached_plugin_init(void *p)
{
	struct mysql_memcached_context*	con;
	pthread_attr_t			attr;
	struct st_plugin_int*		plugin = (struct st_plugin_int *)p;

	con = (mysql_memcached_context*) my_malloc(PSI_INSTRUMENT_ME,
                                                   sizeof(*con), MYF(0));

	if (mci_engine_library) {
		char*	lib_path = (mci_eng_lib_path)
					? mci_eng_lib_path : opt_plugin_dir;
		int	lib_len = strlen(lib_path)
				  + strlen(mci_engine_library)
				  + strlen(FN_DIRSEP) + 1;

		con->memcached_conf.m_engine_library = (char*) my_malloc(
                        PSI_INSTRUMENT_ME,
			lib_len, MYF(0));

		strxmov(con->memcached_conf.m_engine_library, lib_path,
			FN_DIRSEP, mci_engine_library, NullS);
	} else {
		con->memcached_conf.m_engine_library = NULL;
	}

	con->memcached_conf.m_mem_option = mci_memcached_option;
	con->memcached_conf.m_innodb_api_cb = plugin->data;
	con->memcached_conf.m_r_batch_size = mci_r_batch_size;
	con->memcached_conf.m_w_batch_size = mci_w_batch_size;
	con->memcached_conf.m_enable_binlog = mci_enable_binlog;

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
	"Oracle Corporation",
	"Memcached Daemon",
	PLUGIN_LICENSE_GPL,
	daemon_memcached_plugin_init,	/* Plugin Init */
	NULL,	                        /* Plugin Check uninstall */
	daemon_memcached_plugin_deinit,	/* Plugin Deinit */
	0x0100 /* 1.0 */,
	NULL,				/* status variables */
	daemon_memcached_sys_var,	/* system variables */
	NULL,				/* config options */
	0				/* flags */
}
mysql_declare_plugin_end;
