/* Copyright (C) 2006,2007,2008 MySQL AB

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

struct st_dirty_page /* used only in the REDO phase */
{
  uint64 file_and_page_id;
  LSN rec_lsn;
};
extern HASH all_dirty_pages;
/*
  LSN after which dirty pages list does not apply. Can be slightly before
  when ma_checkpoint_execute() started.
*/
extern LSN checkpoint_start;
extern my_bool procent_printed;
extern FILE *tracef;


my_bool _ma_redo_not_needed_for_page(uint16 shortid, LSN lsn,
                                     pgcache_page_no_t page,
                                     my_bool index);
void tprint(FILE *trace_file, const char *format, ...)
  ATTRIBUTE_FORMAT(printf, 2, 3);
void eprint(FILE *trace_file, const char *format, ...)
  ATTRIBUTE_FORMAT(printf, 2, 3);
