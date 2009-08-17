/* Copyright (c) 2005 PrimeBase Technologies GmbH
 *
 * PrimeBase XT
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * 2005-02-07	Paul McCullagh
 *
 * H&G2JCtL
 */
#ifndef __xt_trace_h__
#define __xt_trace_h__

#include "xt_defs.h"

xtBool	xt_init_trace(void);
void	xt_exit_trace(void);
void	xt_dump_trace(void);
void	xt_print_trace(void);

void	xt_trace(const char *fmt, ...);
void	xt_ttraceq(struct XTThread *self, char *query);
void	xt_ttracef(struct XTThread *self, char *fmt, ...);
xtWord8	xt_trace_clock(void);
char	*xt_trace_clock_str(char *ptr);
char	*xt_trace_clock_diff(char *ptr);
char	*xt_trace_clock_diff(char *ptr, xtWord8 start_time);
void	xt_set_fflush(xtBool on);
void	xt_ftracef(char *fmt, ...);

#define XT_DEBUG_TRACE(x)
#define XT_DISABLED_TRACE(x)
#ifdef DEBUG
//#define PBXT_HANDLER_TRACE
#endif

/*
 * -----------------------------------------------------------------------
 * CONNECTION TRACKING
 */

#define XT_TRACK_CONNECTIONS

#ifdef XT_TRACK_CONNECTIONS
#define XT_TRACK_MAX_CONNS		500

typedef struct XTConnInfo {
	xtThreadID			cu_t_id;
	xtXactID			ci_curr_xact_id;
	xtWord8				ci_xact_start;

	xtXactID			ci_prev_xact_id;
	xtWord8				ci_prev_xact_time;
} XTConnInfoRec, *XTConnInfoPtr;

extern XTConnInfoRec xt_track_conn_info[XT_TRACK_MAX_CONNS];

void	xt_dump_conn_tracking(void);

#endif

#endif
