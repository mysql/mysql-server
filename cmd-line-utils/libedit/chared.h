/*	$NetBSD: chared.h,v 1.11 2002/11/20 16:50:08 christos Exp $	*/

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
 *	@(#)chared.h	8.1 (Berkeley) 6/4/93
 */

/*
 * el.chared.h: Character editor interface
 */
#ifndef _h_el_chared
#define	_h_el_chared

#include <ctype.h>
#include <string.h>

#include "histedit.h"

#define	EL_MAXMACRO	10

/*
 * This is a issue of basic "vi" look-and-feel. Defining VI_MOVE works
 * like real vi: i.e. the transition from command<->insert modes moves
 * the cursor.
 *
 * On the other hand we really don't want to move the cursor, because
 * all the editing commands don't include the character under the cursor.
 * Probably the best fix is to make all the editing commands aware of
 * this fact.
 */
#define	VI_MOVE


typedef struct c_macro_t {
	int	  level;
	char	**macro;
	char	 *nline;
} c_macro_t;

/*
 * Undo information for vi - no undo in emacs (yet)
 */
typedef struct c_undo_t {
	int	 len;			/* length of saved line */
	int	 cursor;		/* position of saved cursor */
	char	*buf;			/* full saved text */
} c_undo_t;

/* redo for vi */
typedef struct c_redo_t {
	char	*buf;			/* redo insert key sequence */
	char	*pos;
	char	*lim;
	el_action_t	cmd;		/* command to redo */
	char	ch;			/* char that invoked it */
	int	count;
	int	action;			/* from cv_action() */
} c_redo_t;

/*
 * Current action information for vi
 */
typedef struct c_vcmd_t {
	int	 action;
	char	*pos;
} c_vcmd_t;

/*
 * Kill buffer for emacs
 */
typedef struct c_kill_t {
	char	*buf;
	char	*last;
	char	*mark;
} c_kill_t;

/*
 * Note that we use both data structures because the user can bind
 * commands from both editors!
 */
typedef struct el_chared_t {
	c_undo_t	c_undo;
	c_kill_t	c_kill;
	c_redo_t	c_redo;
	c_vcmd_t	c_vcmd;
	c_macro_t	c_macro;
} el_chared_t;


#define	STReof		"^D\b\b"
#define	STRQQ		"\"\""

#define	isglob(a)	(strchr("*[]?", (a)) != NULL)
#define	isword(a)	(isprint(a))

#define	NOP		0x00
#define	DELETE		0x01
#define	INSERT		0x02
#define	YANK		0x04

#define	CHAR_FWD	(+1)
#define	CHAR_BACK	(-1)

#define	MODE_INSERT	0
#define	MODE_REPLACE	1
#define	MODE_REPLACE_1	2

#include "common.h"
#include "vi.h"
#include "emacs.h"
#include "search.h"
#include "fcns.h"


protected int	 cv__isword(int);
protected int	 cv__isWord(int);
protected void	 cv_delfini(EditLine *);
protected char	*cv__endword(char *, char *, int, int (*)(int));
protected int	 ce__isword(int);
protected void	 cv_undo(EditLine *);
protected void	 cv_yank(EditLine *, const char *, int);
protected char	*cv_next_word(EditLine*, char *, char *, int, int (*)(int));
protected char	*cv_prev_word(char *, char *, int, int (*)(int));
protected char	*c__next_word(char *, char *, int, int (*)(int));
protected char	*c__prev_word(char *, char *, int, int (*)(int));
protected void	 c_insert(EditLine *, int);
protected void	 c_delbefore(EditLine *, int);
protected void	 c_delafter(EditLine *, int);
protected int	 c_gets(EditLine *, char *, const char *);
protected int	 c_hpos(EditLine *);

protected int	 ch_init(EditLine *);
protected void	 ch_reset(EditLine *);
protected int	 ch_enlargebufs(EditLine *, size_t);
protected void	 ch_end(EditLine *);

#endif /* _h_el_chared */
