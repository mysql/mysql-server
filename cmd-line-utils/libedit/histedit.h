/*	$NetBSD: histedit.h,v 1.21 2003/01/21 18:40:24 christos Exp $	*/

/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Christos Zoulas of Cornell University.
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
 *	@(#)histedit.h	8.2 (Berkeley) 1/3/94
 */

/*
 * histedit.h: Line editor and history interface.
 */
#ifndef _HISTEDIT_H_
#define	_HISTEDIT_H_

#define	LIBEDIT_MAJOR 2
#define	LIBEDIT_MINOR 6

#include <sys/types.h>
#include <stdio.h>

/*
 * ==== Editing ====
 */
typedef struct editline EditLine;

/*
 * For user-defined function interface
 */
typedef struct lineinfo {
	const char	*buffer;
	const char	*cursor;
	const char	*lastchar;
} LineInfo;


/*
 * EditLine editor function return codes.
 * For user-defined function interface
 */
#define	CC_NORM		0
#define	CC_NEWLINE	1
#define	CC_EOF		2
#define	CC_ARGHACK	3
#define	CC_REFRESH	4
#define	CC_CURSOR	5
#define	CC_ERROR	6
#define	CC_FATAL	7
#define	CC_REDISPLAY	8
#define	CC_REFRESH_BEEP	9

/*
 * Initialization, cleanup, and resetting
 */
EditLine	*el_init(const char *, FILE *, FILE *, FILE *);
void		 el_reset(EditLine *);
void		 el_end(EditLine *);


/*
 * Get a line, a character or push a string back in the input queue
 */
const char	*el_gets(EditLine *, int *);
int		 el_getc(EditLine *, char *);
void		 el_push(EditLine *, char *);

/*
 * Beep!
 */
void		 el_beep(EditLine *);

/*
 * High level function internals control
 * Parses argc, argv array and executes builtin editline commands
 */
int		 el_parse(EditLine *, int, const char **);

/*
 * Low level editline access functions
 */
int		 el_set(EditLine *, int, ...);
int		 el_get(EditLine *, int, void *);

/*
 * el_set/el_get parameters
 */
#define	EL_PROMPT	0	/* , el_pfunc_t);		*/
#define	EL_TERMINAL	1	/* , const char *);		*/
#define	EL_EDITOR	2	/* , const char *);		*/
#define	EL_SIGNAL	3	/* , int);			*/
#define	EL_BIND		4	/* , const char *, ..., NULL);	*/
#define	EL_TELLTC	5	/* , const char *, ..., NULL);	*/
#define	EL_SETTC	6	/* , const char *, ..., NULL);	*/
#define	EL_ECHOTC	7	/* , const char *, ..., NULL);	*/
#define	EL_SETTY	8	/* , const char *, ..., NULL);	*/
#define	EL_ADDFN	9	/* , const char *, const char *	*/
				/* , el_func_t);		*/
#define	EL_HIST		10	/* , hist_fun_t, const char *);	*/
#define	EL_EDITMODE	11	/* , int);			*/
#define	EL_RPROMPT	12	/* , el_pfunc_t);		*/
#define	EL_GETCFN	13	/* , el_rfunc_t);		*/
#define	EL_CLIENTDATA	14	/* , void *);			*/

#define EL_BUILTIN_GETCFN	(NULL)

/*
 * Source named file or $PWD/.editrc or $HOME/.editrc
 */
int		el_source(EditLine *, const char *);

/*
 * Must be called when the terminal changes size; If EL_SIGNAL
 * is set this is done automatically otherwise it is the responsibility
 * of the application
 */
void		 el_resize(EditLine *);


/*
 * User-defined function interface.
 */
const LineInfo	*el_line(EditLine *);
int		 el_insertstr(EditLine *, const char *);
void		 el_deletestr(EditLine *, int);

/*
 * ==== History ====
 */

typedef struct history History;

typedef struct HistEvent {
	int		 num;
	const char	*str;
} HistEvent;

/*
 * History access functions.
 */
History *	history_init(void);
void		history_end(History *);

int		history(History *, HistEvent *, int, ...);

#define	H_FUNC		 0	/* , UTSL		*/
#define	H_SETSIZE	 1	/* , const int);	*/
#define	H_GETSIZE	 2	/* , void);		*/
#define	H_FIRST		 3	/* , void);		*/
#define	H_LAST		 4	/* , void);		*/
#define	H_PREV		 5	/* , void);		*/
#define	H_NEXT		 6	/* , void);		*/
#define	H_CURR		 8	/* , const int);	*/
#define	H_SET		 7	/* , int);		*/
#define	H_ADD		 9	/* , const char *);	*/
#define	H_ENTER		10	/* , const char *);	*/
#define	H_APPEND	11	/* , const char *);	*/
#define	H_END		12	/* , void);		*/
#define	H_NEXT_STR	13	/* , const char *);	*/
#define	H_PREV_STR	14	/* , const char *);	*/
#define	H_NEXT_EVENT	15	/* , const int);	*/
#define	H_PREV_EVENT	16	/* , const int);	*/
#define	H_LOAD		17	/* , const char *);	*/
#define	H_SAVE		18	/* , const char *);	*/
#define	H_CLEAR		19	/* , void);		*/
#define	H_SETUNIQUE	20	/* , int);		*/
#define	H_GETUNIQUE	21	/* , void);		*/

#endif /* _HISTEDIT_H_ */
