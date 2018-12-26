/* Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef TASK_DEBUG_H
#define TASK_DEBUG_H

#ifdef __cplusplus
extern "C" {
#endif

#include "xcom_common.h"
#include "x_platform.h"

#ifdef TASK_DBUG_ON
#error "TASK_DBUG_ON already defined"
#else
#define TASK_DBUG_ON 0
#endif


#define TX_FMT "{" SY_FMT_DEF " %lu}"
#define TX_MEM(x) SY_MEM((x).cfg), (x).pc

#include <stdio.h>
#include <stdlib.h>

double       task_now();

#ifdef DBGOUT
#error "DBGOUT defined"
#endif

/* Log levels definition for use without external logger */
typedef enum
{
  LOG_FATAL= 0,
  LOG_ERROR= 1,
  LOG_WARN= 2,
  LOG_INFO= 3,
  LOG_DEBUG= 4,
  LOG_TRACE= 5
} xcom_log_level_t;

static const char* const log_levels[]=
{
  "[XCOM_FATAL] ",
  "[XCOM_ERROR] ",
  "[XCOM_WARN] ",
  "[XCOM_INFO] ",
  "[XCOM_DEBUG] ",
  "[XCOM_TRACE] "
};

typedef void (*xcom_logger)(int level, const char *message);


/**
  Logger callback used in the logging macros.
*/

extern xcom_logger xcom_log;


/**
  Prints the logging message to the console. It is invoked when no logger
  callback was set.
*/

void xcom_simple_log(int l, const char *msg);


/**
  Concatenates two strings and returns pointer to last character of final
  string, allowing further concatenations without having to cycle through the
  entire string again.

  @param dest pointer to last character of destination string
  @param size pointer to the number of characters currently added to
    xcom_log_buffer
  @param src pointer to the string to append to dest
  @return pointer to the last character of destination string after appending
    dest, which corresponds to the position of the '\0' character
*/

char *mystrcat(char *dest, int *size, const char *src);


/**
  This function allocates a new string where the format string and optional
  arguments are rendered to.
  Finally, it invokes mystr_cat to concatenate the rendered string to the
  string received in the first parameter.
*/

char *mystrcat_sprintf(char *dest, int *size, const char *format, ...)
  MY_ATTRIBUTE((format(printf, 3, 4)));

#define STR_SIZE 2047

#define GET_GOUT char xcom_log_buffer[STR_SIZE+1]; char *xcom_temp_buf= xcom_log_buffer; int xcom_log_buffer_size= 0; xcom_log_buffer[0]= 0 
#define GET_NEW_GOUT char *xcom_log_buffer= (char *) malloc((STR_SIZE+1)*sizeof(char)); char *xcom_temp_buf= xcom_log_buffer; int xcom_log_buffer_size= 0; xcom_log_buffer[0]= 0
#define FREE_GOUT xcom_log_buffer[0]= 0
#define ADD_GOUT(s) xcom_temp_buf= mystrcat(xcom_temp_buf, &xcom_log_buffer_size, s)
#define COPY_AND_FREE_GOUT(s) {char *__funny = s; ADD_GOUT(__funny); free(__funny);}
#define ADD_F_GOUT(...) xcom_temp_buf= mystrcat_sprintf(xcom_temp_buf, &xcom_log_buffer_size, __VA_ARGS__)
#define PRINT_LEVEL_GOUT(level) xcom_log(level, xcom_log_buffer)
#define PRINT_GOUT PRINT_LEVEL_GOUT(LOG_DEBUG)
#define RET_GOUT return xcom_log_buffer

#define G_LOG(l,...) { GET_GOUT; ADD_F_GOUT(__VA_ARGS__); PRINT_LEVEL_GOUT(l); FREE_GOUT; }

#ifdef WITH_LOG_DEBUG
#define G_DEBUG(...) G_LOG(LOG_DEBUG,__VA_ARGS__)
#else
#define G_DEBUG(...)
#endif

#ifdef WITH_LOG_TRACE
#define G_TRACE(...) G_LOG(LOG_TRACE,__VA_ARGS__)
#else
#define G_TRACE(...)
#endif

#define G_WARNING(...) G_LOG(LOG_WARN,__VA_ARGS__)
#define g_critical(...) G_LOG(LOG_FATAL,__VA_ARGS__)
#define G_ERROR(...) G_LOG(LOG_ERROR,__VA_ARGS__)
#define G_MESSAGE(...) G_LOG(LOG_INFO, __VA_ARGS__)


#if TASK_DBUG_ON
#define DBGOHK(x) { GET_GOUT; ADD_F_GOUT("%f ",task_now()); NEXP(get_nodeno(get_site_def()),u); x; PRINT_GOUT; FREE_GOUT; }
#define DBGOUT(x) { GET_GOUT; ADD_F_GOUT("%f ",task_now()); x; PRINT_GOUT; FREE_GOUT; }
#define MAY_DBG(x) DBGOUT(x)
#else
#define DBGOHK(x)
#define DBGOUT(x)
#define MAY_DBG(x)
#endif

#define g_string_append_printf fprintf
#define XDBG #error
#define FN ADD_GOUT(__func__); ADD_F_GOUT(" " __FILE__ ":%d ", __LINE__);
#define PTREXP(x) ADD_F_GOUT(#x ": %p ",(void*)(x))
#define PPUT(x) ADD_F_GOUT("0x%p ",(void*)(x))
#define STREXP(x) ADD_F_GOUT(#x ": %s ",x)
#define STRLIT(x) ADD_GOUT(x)
#define NPUT(x,f) ADD_F_GOUT("%" #f " ",x)
#define NDBG(x,f) ADD_F_GOUT(#x " = "); NPUT(x,f);
#define NEXP(x,f) ADD_F_GOUT(#x ": %" #f " ",x)
#define NUMEXP(x) NEXP(x,d)
#define g_strerror strerror
#define LOUT(pri,x) ADD_F_GOUT(x);
#define SYCEXP(exp) ADD_F_GOUT(#exp "={%x %llu %u} ", (exp).group_id, (long long unsigned int)((exp).msgno), ((exp).node))
#define TIDCEXP(exp) ADD_F_GOUT(#exp "={%x %llu %u %u} ", (exp).cfg.group_id, (long long unsigned int)(exp).cfg.msgno, (exp).cfg.node, (exp).pc)
#define TIMECEXP(exp) ADD_F_GOUT(#exp "=%f sec ", (exp))
#define BALCEXP(exp) ADD_F_GOUT(#exp "={%d %d} ", (exp).cnt, (exp).node)

#include <string.h>
#include "result.h"

#ifdef XCOM_HAVE_OPENSSL
static inline void task_dump_err(int err)
{
	if (err) {
		if (is_ssl_err(err)) {
			MAY_DBG(FN; NDBG(from_ssl_err(err), d));
		} else {
			MAY_DBG(FN; NDBG(from_errno(err), d); STREXP(strerror(err)));
		}
	}
}
#else
static inline void task_dump_err(int err)
{
	if (err) {
		MAY_DBG(FN; NDBG(to_errno(err), d); STREXP(strerror(err)));
	}
}
#endif

#ifdef __cplusplus
}
#endif

#endif

