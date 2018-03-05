/* Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef SERVER_STRUCT_H
#define SERVER_STRUCT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "task.h"
#include "xcom_common.h"
#include "xcom_limits.h"

struct srv_buf {
	u_int start;
	u_int n;
	char	buf[0x10000];
};
typedef struct srv_buf srv_buf;

/* Server definition */
struct server {
	int garbage;
	int	refcnt;
	char	*srv;        /* Server name */
	xcom_port	port;         /* Port */
	connection_descriptor con;           /* Descriptor for open connection */
	double	active;     /* Last activity */
	double	detected;     /* Last incoming */
	channel outgoing; /* Outbound messages */
	task_env * sender;  /* The sender task */
	task_env * reply_handler;  /* The reply task */
	srv_buf out_buf;
        int invalid;
};

typedef struct server server;

#ifdef __cplusplus
}
#endif

#endif

