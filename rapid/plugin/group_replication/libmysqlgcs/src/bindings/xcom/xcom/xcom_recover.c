/* Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <assert.h>
#include <stdlib.h>

#include "xcom_common.h"
#include "simset.h"
#include "xcom_vp.h"
#include "task.h"
#include "task_debug.h"
#include "node_no.h"
#include "server_struct.h"
#include "xcom_detector.h"
#include "site_struct.h"
#include "xcom_transport.h"
#include "xcom_base.h"
#include "synode_no.h"

#include "xcom_recover.h"
#include "app_data.h"
#include "site_def.h"

extern task_env *boot;
extern task_env *net_boot;
extern task_env *net_recover;
extern task_env *killer;

extern synode_no executed_msg;    /* The message we are waiting to execute */

start_t start_type;
int	client_boot_done = 0;
int	netboot_ok = 0;
int	booting = 0;

define_xdr_funcs(synode_no)
define_xdr_funcs(app_data_ptr)

/* purecov: begin deadcode */
void init_recover_vars()
{
	start_type = IDLE;
	client_boot_done = 0;
	netboot_ok = 0;
	booting = 0;
}
/* purecov: end */

static synode_no log_start;       /* Redo log from this synode */
static synode_no log_end;         /* Redo log until this synode */

void	xcom_recover_init()
{
	log_start = null_synode;
	log_end = null_synode;
}

void	set_log_group_id(uint32_t group_id)
{
	log_start.group_id = group_id;
	log_end.group_id = group_id;
}

/* purecov: begin deadcode */
int	log_prefetch_task(task_arg arg MY_ATTRIBUTE((unused)))
{
	DECL_ENV
	    int	self;
	int n;
	END_ENV;

	TASK_BEGIN

	    ep->self = 0;
	ep->n = 0;

	MAY_DBG(FN; NDBG(ep->self, d); NDBG(task_now(), f));

	assert(log_start.msgno != 0);

	while (net_recover && (!synode_gt(executed_msg, log_end))) {
		request_values(log_start, log_end);
		ep->n ++;
		if(ep->n > 1){
			G_WARNING("log_prefetch_task retry %d",ep->n);
		}
		TASK_DELAY(1.0);
	}
	FINALLY
	    MAY_DBG(FN; STRLIT(" exit "); NDBG(ep->self, d); NDBG(task_now(), f));
	TASK_END;
}

void	setup_recover(pax_msg *m)
{
	DBGOUT(FN; NDBG(client_boot_done, d));
	if (!client_boot_done) {
		start_type = RECOVER;
		client_boot_done = 1; /* Detected incoming recovery from the net */
		set_group(m->group_id);
		SET_EXECUTED_MSG(m->synode);
		check_tasks();
	}
}


void	setup_boot(pax_msg *m)
{
	DBGOUT(FN; NDBG(client_boot_done, d));
	if (!client_boot_done) {
		start_type = BOOT;
		client_boot_done = 1; /* Detected incoming boot from the net */
		SET_EXECUTED_MSG(m->synode);
		check_tasks();
	}
}


int	xcom_booted()
{
	return get_maxnodes(get_site_def()) > 0 && netboot_ok;
}
/* purecov: end */



