/* Copyright (C) 2007 MySQL AB

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

#include <my_check_opt.h>

/* almost every standalone maria program will need it */
void _mi_report_crashed(void *file __attribute__((unused)),
                        const char *message __attribute__((unused)),
                        const char *sfile __attribute__((unused)),
                        uint sline __attribute__((unused)))
{
}

/* only those that included myisamchk.h may need and can use the below */
#ifdef _myisamchk_h
/*
  All standalone programs which need to use functions from ma_check.c
  (like maria_repair()) must define their version of _ma_killed_ptr()
  and _ma_check_print_info|warning|error(). Indeed, linking with ma_check.o
  brings in the dependencies of ma_check.o which are definitions of the above
  functions; if the program does not define them then the ones of
  ha_maria.o are used i.e. ha_maria.o is linked into the program, and this
  brings dependencies of ha_maria.o on mysqld.o into the program's linking
  which thus fails, as the program is not linked with mysqld.o.
  This file contains the versions of these functions used by maria_chk and
  maria_read_log.
*/

/*
  Check if check/repair operation was killed by a signal
*/

int _ma_killed_ptr(HA_CHECK *param __attribute__((unused)))
{
  return 0;
}


void _ma_report_progress(HA_CHECK *param __attribute__((unused)),
                         ulonglong progress __attribute__((unused)),
                         ulonglong max_progress __attribute__((unused)))
{
}

	/* print warnings and errors */
	/* VARARGS */

void _ma_check_print_info(HA_CHECK *param __attribute__((unused)),
			 const char *fmt,...)
{
  va_list args;
  DBUG_ENTER("_ma_check_print_info");
  DBUG_PRINT("enter", ("format: %s", fmt));

  va_start(args,fmt);
  vfprintf(stdout, fmt, args);
  fputc('\n',stdout);
  va_end(args);
  DBUG_VOID_RETURN;
}

/* VARARGS */

void _ma_check_print_warning(HA_CHECK *param, const char *fmt,...)
{
  va_list args;
  DBUG_ENTER("_ma_check_print_warning");
  DBUG_PRINT("enter", ("format: %s", fmt));

  fflush(stdout);
  if (!param->warning_printed && !param->error_printed)
  {
    if (param->testflag & T_SILENT)
      fprintf(stderr,"%s: Aria file %s\n",my_progname_short,
	      param->isam_file_name);
    param->out_flag|= O_DATA_LOST;
  }
  param->warning_printed=1;
  va_start(args,fmt);
  fprintf(stderr,"%s: warning: ",my_progname_short);
  vfprintf(stderr, fmt, args);
  fputc('\n',stderr);
  fflush(stderr);
  va_end(args);
  DBUG_VOID_RETURN;
}

/* VARARGS */

void _ma_check_print_error(HA_CHECK *param, const char *fmt,...)
{
  va_list args;
  DBUG_ENTER("_ma_check_print_error");
  DBUG_PRINT("enter", ("format: %s", fmt));

  fflush(stdout);
  if (!param->warning_printed && !param->error_printed)
  {
    if (param->testflag & T_SILENT)
      fprintf(stderr,"%s: Aria file %s\n",my_progname_short,param->isam_file_name);
    param->out_flag|= O_DATA_LOST;
  }
  param->error_printed|=1;
  va_start(args,fmt);
  fprintf(stderr,"%s: error: ",my_progname_short);
  vfprintf(stderr, fmt, args);
  fputc('\n',stderr);
  fflush(stderr);
  va_end(args);
  DBUG_VOID_RETURN;
}
#endif

