/* Copyright (C) 2004-2006 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef INCLUDES_MYSQL_INSTANCE_MANAGER_PRIV_H
#define INCLUDES_MYSQL_INSTANCE_MANAGER_PRIV_H

#include <my_global.h>
#include <m_string.h>
#include <my_pthread.h>

#include <sys/types.h>

#ifndef __WIN__
#include <unistd.h>
#endif

#include "portability.h"

/* IM-wide platform-independent defines */
#define SERVER_DEFAULT_PORT MYSQL_PORT
#define DEFAULT_MONITORING_INTERVAL 20
#define DEFAULT_PORT 2273
/* three-week timeout should be enough */
#define LONG_TIMEOUT ((ulong) 3600L*24L*21L)

const int MEM_ROOT_BLOCK_SIZE= 512;

/* The maximal length of option name and option value. */
const int MAX_OPTION_LEN= 1024;

/*
  The maximal length of whole option string:
    --<option name>=<option value>
*/
const int MAX_OPTION_STR_LEN= 2 + MAX_OPTION_LEN + 1 + MAX_OPTION_LEN + 1;

const int MAX_VERSION_LENGTH= 160;

const int MAX_INSTANCE_NAME_SIZE= FN_REFLEN;

extern const LEX_STRING mysqlmanager_version;

/* MySQL client-server protocol version: substituted from configure */
extern const unsigned char protocol_version;

/*
  These variables are used in MySQL subsystem to work with mysql clients
  To be moved to a config file/options one day.
*/


/* Buffer length for TCP/IP and socket communication */
extern unsigned long net_buffer_length;


/* Maximum allowed incoming/ougoung packet length */
extern unsigned long max_allowed_packet;


/*
  Number of seconds to wait for more data from a connection before aborting
  the read
*/
extern unsigned long net_read_timeout;


/*
  Number of seconds to wait for a block to be written to a connection
  before aborting the write.
*/
extern unsigned long net_write_timeout;


/*
  If a read on a communication port is interrupted, retry this many times
  before giving up.
*/
extern unsigned long net_retry_count;

extern unsigned int test_flags;
extern unsigned long bytes_sent, bytes_received;
extern unsigned long mysqld_net_retry_count;
extern unsigned long open_files_limit;

bool create_pid_file(const char *pid_file_name, int pid);

#endif // INCLUDES_MYSQL_INSTANCE_MANAGER_PRIV_H
