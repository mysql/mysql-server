/*
Copyright (c) 2003, 2011, 2013, Oracle and/or its affiliates. All rights
reserved.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of
the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
02110-1301  USA
*/

#ifndef REPEVENT_INCLUDED
#define	REPEVENT_INCLUDED

#include "binlog_event.h"
#include "field_iterator.h"
#include "load_data_events.h"
#include "rowset.h"
#include "rows_event.h"
#include "transitional_methods.h"
#include <iosfwd>
#include <list>
#include <cassert>
#include <algorithm>

#define BAPI_STRERROR_SIZE (256)
namespace binary_log
{

/**
 * Error codes.
 */
enum Error_code {
  ERR_OK = 0,                                   /* All OK */
  ERR_EOF,                                      /* End of file */
  ERR_FAIL,                                     /* Unspecified failure */
  ERR_CHECKSUM_ENABLED,
  ERR_CHECKSUM_QUERY_FAIL,
  ERR_CONNECT,
  ERR_BINLOG_VERSION,
  ERR_PACKET_LENGTH,
  ERR_MYSQL_QUERY_FAIL,
  ERROR_CODE_COUNT
};

/**
 *Errors you can get from the API
 */
extern const char *bapi_error_messages[];

extern const char *str_error(int error_no);

}

#endif	/* REPEVENT_INCLUDED */
