/* Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef XCOM_CONFIG_H
#define XCOM_CONFIG_H

/* Use global node set to decide if proposal should be sent */
#define PROPOSE_IF_LEADER

/* Always use the full three phase Paxos protocol */
/* #define ALWAYS_THREEPHASE */

/* Make node 0 the default arbitrator */
/* #define NODE_0_IS_ARBITRATOR */

/* On first propose, wait for all nodes, not just majority */
/* #define WAIT_FOR_ALL_FIRST */

/* Do not delay termination of xcom */
/* #define NO_DELAYED_TERMINATION */

/* Define this if you want integrity check of message contents */
/* #define USE_CHECKSUM */

/* React to garbage by sending more garbage back */
/* #define XCOM_ECM */

/* #define USE_EXIT_TYPE */

/* Run without sweeper task */
/* #define NO_SWEEPER_TASK */

/* Impose upper limit on delivery time? */
/* #define DELIVERY_TIMEOUT */

/* #define IGNORE_LOSERS */

#define BUILD_TIMEOUT 3.0

#define TERMINATE_DELAY 3.0

/* Error injection for testing */
#define INJECT_ERROR 0

/* #define USE_EXIT_TYPE */
/* #define NO_SWEEPER_TASK */

/* #define ACCEPT_SITE_TEST */

/* Define this to enable the binary event logger */
/* #define TASK_EVENT_TRACE */

/* Make sweeper task run more often */
#define AGGRESSIVE_SWEEP

/* Turn automatic batching on or off */
#define AUTOBATCH 1

enum{
	EVENT_HORIZON_MIN = 10,
	MAX_BATCH_SIZE = 0x3fffffff, /* Limit batch size to sensible ? amount */
	MAX_DEAD = 10,
	PROPOSERS = 10				/* The number of proposers on one node */
};

/* How long to wait for snapshots when trying to find the best node to recover from */
#define SNAPSHOT_WAIT_TIME 3.0

#endif
