/* Copyright (c) 2017, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef XCOM_CONFIG_H
#define XCOM_CONFIG_H

/* Use global node set to decide if proposal should be sent */
/* #define PROPOSE_IF_LEADER */

/* Always use the full three phase Paxos protocol */
/* #define ALWAYS_THREEPHASE */

/* Make node 0 the default arbitrator */
/* #define NODE_0_IS_ARBITRATOR */

/* On first propose, wait for all nodes, not just majority */
/* #define WAIT_FOR_ALL_FIRST */

/* Do not delay termination of xcom */
/* #define NO_DELAYED_TERMINATION */
#define TERMINATE_DELAY 3.0

/* Set max number of acceptors */
/* #define MAXACCEPT 5  */

/* Impose upper limit on delivery time? */
/* #define DELIVERY_TIMEOUT */

/* Time until we suspect failed delivery */
#define BUILD_TIMEOUT 0.5

/* #define IGNORE_LOSERS */

/* #define ACCEPT_SITE_TEST */

/* Define this to enable the binary event logger */
/* #define TASK_EVENT_TRACE */

/* Size of binary event logger cache */
#define MAX_TASK_EVENT 20000

/* Make sweeper task run more often */
#define AGGRESSIVE_SWEEP

/* Turn automatic batching on or off */
#define AUTOBATCH 1

enum {
  EVENT_HORIZON_MIN = 10,
  EVENT_HORIZON_MAX = 200,
  MAX_BATCH_SIZE = 0x3fffffff, /* Limit batch size to sensible ? amount */
  MAX_BATCH_APP_DATA = 5000,   /* Limit nr. of batched elements */
  MAX_DEAD = 10,
  PROPOSERS = 10,              /* The number of proposers on one node */
  MIN_CACHE_SIZE = 250000,     /* Minimum cache size */
  DEFAULT_CACHE_LIMIT = 1000000000UL /* Reasonable initial cache limit */
};

/* How long to wait for snapshots when trying to find the best node to recover
 * from */
#define SNAPSHOT_WAIT_TIME 3.0

/* Disable test to see if we should handle need_boot */
#define ALWAYS_HANDLE_NEED_BOOT 0

/* Disable test to see if we should handle prepare and accept */
#define ALWAYS_HANDLE_CONSENSUS 0

/* Force IPv4 */
/* #define FORCE_IPV4 */

/* Define this if xcom should suppress delivery of duplicate global views */
#define SUPPRESS_DUPLICATE_VIEWS

/* Define this to use the default event horizon if we have no config */
/* #define PERMISSIVE_EH_ACTIVE_CONFIG */

/* Define this to 0 if you always have address:port, 1 if not */
#define NO_PORT_IN_ADDRESS 0

/* Let the executor_task be more aggressive in proposing `no_op` for
   missing messages */
/* #define EXECUTOR_TASK_AGGRESSIVE_NO_OP */

#endif
