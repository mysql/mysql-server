/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/* Common defines for all clients */

#include <my_global.h>
#include <my_sys.h>
#include <m_string.h>
#include <mysql.h>
#include <mysql_embed.h>
#include <errmsg.h>
#include <my_getopt.h>

/* We have to define 'enum options' identical in all files to keep OS2 happy */

enum options_client
{
  OPT_CHARSETS_DIR=256, OPT_DEFAULT_CHARSET,
  OPT_PAGER, OPT_NOPAGER, OPT_TEE, OPT_NOTEE,
  OPT_LOW_PRIORITY, OPT_AUTO_REPAIR, OPT_COMPRESS,
  OPT_DROP, OPT_LOCKS, OPT_KEYWORDS, OPT_DELAYED, OPT_OPTIMIZE,
  OPT_FTB, OPT_LTB, OPT_ENC, OPT_O_ENC, OPT_ESC, OPT_TABLES,
  OPT_MASTER_DATA, OPT_AUTOCOMMIT, OPT_AUTO_REHASH,
  OPT_LINE_NUMBERS, OPT_COLUMN_NAMES, OPT_CONNECT_TIMEOUT,
  OPT_MAX_ALLOWED_PACKET, OPT_NET_BUFFER_LENGTH,
  OPT_SELECT_LIMIT, OPT_MAX_JOIN_SIZE, OPT_SSL_SSL,
  OPT_SSL_KEY, OPT_SSL_CERT, OPT_SSL_CA, OPT_SSL_CAPATH,
  OPT_SSL_CIPHER, OPT_SHUTDOWN_TIMEOUT, OPT_LOCAL_INFILE,
  OPT_DELETE_MASTER_LOGS, OPT_COMPACT,
  OPT_PROMPT, OPT_IGN_LINES,OPT_TRANSACTION,OPT_MYSQL_PROTOCOL,
  OPT_SHARED_MEMORY_BASE_NAME, OPT_FRM, OPT_SKIP_OPTIMIZATION,
  OPT_COMPATIBLE, OPT_RECONNECT, OPT_DELIMITER, OPT_SECURE_AUTH,
  OPT_OPEN_FILES_LIMIT, OPT_SET_CHARSET, OPT_CREATE_OPTIONS,
  OPT_START_POSITION, OPT_STOP_POSITION, OPT_START_DATETIME, OPT_STOP_DATETIME,
  OPT_SIGINT_IGNORE, OPT_HEXBLOB, OPT_ORDER_BY_PRIMARY
#ifdef HAVE_NDBCLUSTER_DB
  ,OPT_NDBCLUSTER,OPT_NDB_CONNECTSTRING
#endif
};
