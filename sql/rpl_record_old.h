/* Copyright 2007 MySQL AB. All rights reserved.

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

#ifndef RPL_RECORD_OLD_H
#define RPL_RECORD_OLD_H

#ifndef MYSQL_CLIENT
size_t pack_row_old(TABLE *table, MY_BITMAP const* cols,
                    uchar *row_data, const uchar *record);

#ifdef HAVE_REPLICATION
int unpack_row_old(Relay_log_info *rli,
                   TABLE *table, uint const colcnt, uchar *record,
                   uchar const *row, MY_BITMAP const *cols,
                   uchar const **row_end, ulong *master_reclength,
                   MY_BITMAP* const rw_set,
                   Log_event_type const event_type);
#endif
#endif
#endif
