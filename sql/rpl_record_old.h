/* Copyright (c) 2007, 2015, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#ifndef RPL_RECORD_OLD_H
#define RPL_RECORD_OLD_H

#include "my_global.h"

#ifndef MYSQL_CLIENT
struct TABLE;
typedef struct st_bitmap MY_BITMAP;

size_t pack_row_old(TABLE *table, MY_BITMAP const* cols,
                    uchar *row_data, const uchar *record);

#ifdef HAVE_REPLICATION
#include "binlog_event.h"   // Log_event_type

class Relay_log_info;

int unpack_row_old(Relay_log_info *rli,
                   TABLE *table, uint const colcnt, uchar *record,
                   uchar const *row, MY_BITMAP const *cols,
                   uchar const **row_end, ulong *master_reclength,
                   MY_BITMAP* const rw_set,
                   binary_log::Log_event_type const event_type);
#endif
#endif
#endif
