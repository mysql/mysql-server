/*
 * Copyright (c) 1989 The Regents of the University of California.
 * All rights reserved.
 *
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: @(#)time.h	5.12 (Berkeley) 3/9/91
 *	$Id$
 */

#ifndef _SYS_TIME_H_
#define	_SYS_TIME_H_

#include <time.h>
#include <sys/cdefs.h>

struct timeval {
	long				tv_sec;			/* seconds */
	long				tv_usec;		/* microseconds */
};

struct timezone {
	int 				tz_minuteswest;	/* minutes west of Greenwich */
	int 				tz_dsttime;		/* dst correction */
};						
#define DST_NONE    	0  			 	/* not on dst */
#define DST_USA     	1  			 	/* USA style dst */
#define DST_AUST    	2  			 	/* Australian style dst */
#define DST_WET     	3  			 	/* Western European dst */
#define DST_MET     	4  			 	/* Middle European dst */
#define DST_EET     	5  			 	/* Eastern European dst */


struct  itimerval {
    struct timeval		it_interval;	/* timer interval */
    struct timeval		it_value;   	/* current value */
};
#define ITIMER_REAL 	0
#define ITIMER_VIRTUAL  1
#define ITIMER_PROF 	2

/*
 * Functions
 */
__BEGIN_DECLS

int 	gettimeofday		__P_((struct timeval *, struct timezone *));

__END_DECLS

#endif
