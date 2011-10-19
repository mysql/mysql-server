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

/*
  Q: Why isn't ma_recovery_util.c simply moved to ma_recovery.c ?

  A: ma_recovery.c, because it invokes objects from ma_check.c (like
  maria_chk_init()) causes the following problem:
  if a source file a.c of a program invokes a function defined in
  ma_recovery.c, then a.o depends on ma_recovery.o which depends on
  ma_check.o: linker thus brings in ma_check.o. That brings in the
  dependencies of ma_check.o which are definitions of _ma_check_print_info()
  etc; if a.o does not define them then the ones of ha_maria.o are used
  i.e. ha_maria.o is linked into the program, and this brings in dependencies
  of ha_maria.o on mysqld.o into the program's linking which thus fails, as
  the program is not linked with mysqld.o.
  Thus, while several functions defined in ma_recovery.c could be useful to
  other files, they cannot be used by them.
  So we are going to gradually move a great share of ma_recovery.c's exported
  functions into the present file, to isolate the problematic components and
  avoid the problem.
*/

#include "maria_def.h"

HASH all_dirty_pages;
struct st_dirty_page /* used only in the REDO phase */
{
  uint64 file_and_page_id;
  LSN rec_lsn;
};
/*
  LSN after which dirty pages list does not apply. Can be slightly before
  when ma_checkpoint_execute() started.
*/
LSN checkpoint_start= LSN_IMPOSSIBLE;

/** @todo looks like duplicate of recovery_message_printed */
my_bool procent_printed;
FILE *tracef; /**< trace file for debugging */


/** @brief Prints to a trace file if it is not NULL */
void tprint(FILE *trace_file __attribute__ ((unused)),
            const char *format __attribute__ ((unused)), ...)
{
  va_list args;
#ifndef DBUG_OFF
  {
    char buff[1024], *end;
    va_start(args, format);
    vsnprintf(buff, sizeof(buff)-1, format, args);
    if (*(end= strend(buff)) == '\n')
      *end= 0;                                  /* Don't print end \n */
    DBUG_PRINT("info", ("%s", buff));
    va_end(args);
  }
#endif
  va_start(args, format);
  if (trace_file != NULL)
  {
    if (procent_printed)
    {
      procent_printed= 0;
      fputc('\n', trace_file);
    }
    vfprintf(trace_file, format, args);
  }
  va_end(args);
}


void eprint(FILE *trace_file __attribute__ ((unused)),
            const char *format __attribute__ ((unused)), ...)
{
  va_list args;
  va_start(args, format);
  DBUG_PRINT("error", ("%s", format));
  if (!trace_file)
    trace_file= stderr;

  if (procent_printed)
  {
    /* In silent mode, print on another line than the 0% 10% 20% line */
    procent_printed= 0;
    fputc('\n', trace_file);
  }
  vfprintf(trace_file , format, args);
  fputc('\n', trace_file);
  if (trace_file != stderr)
  {
    va_start(args, format);
    my_printv_error(HA_ERR_INITIALIZATION, format, MYF(0), args);
  }
  va_end(args);
  fflush(trace_file);
}


/**
   Tells if the dirty pages list found in checkpoint record allows to ignore a
   REDO for a certain page.

   @param  shortid         short id of the table
   @param  lsn             REDO record's LSN
   @param  page            page number
   @param  index           TRUE if index page, FALSE if data page
*/

my_bool _ma_redo_not_needed_for_page(uint16 shortid, LSN lsn,
                                     pgcache_page_no_t page,
                                     my_bool index)
{
  if (cmp_translog_addr(lsn, checkpoint_start) < 0)
  {
    /*
      64-bit key is formed like this:
      Most significant byte: 0 if data page, 1 if index page
      Next 2 bytes: table's short id
      Next 5 bytes: page number
    */
    char llbuf[22];
    uint64 file_and_page_id=
      (((uint64)((index << 16) | shortid)) << 40) | page;
    struct st_dirty_page *dirty_page= (struct st_dirty_page *)
      my_hash_search(&all_dirty_pages,
                  (uchar *)&file_and_page_id, sizeof(file_and_page_id));
    DBUG_PRINT("info", ("page %lld in dirty pages list: %d",
                        (ulonglong) page,
                        dirty_page != NULL));
    if ((dirty_page == NULL) ||
        cmp_translog_addr(lsn, dirty_page->rec_lsn) < 0)
    {
      tprint(tracef, ", ignoring page %s because of dirty_pages list\n",
             llstr((ulonglong) page, llbuf));
      return TRUE;
    }
  }
  return FALSE;
}
