/*	$NetBSD: term.c,v 1.35 2002/03/18 16:00:59 christos Exp $	*/

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
 */

#include "config.h"
#if !defined(lint) && !defined(SCCSID)
#if 0
static char sccsid[] = "@(#)term.c	8.2 (Berkeley) 4/30/95";
#else
__RCSID("$NetBSD: term.c,v 1.35 2002/03/18 16:00:59 christos Exp $");
#endif
#endif /* not lint && not SCCSID */

/*
 * term.c: Editor/termcap-curses interface
 *	   We have to declare a static variable here, since the
 *	   termcap putchar routine does not take an argument!
 */
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#ifdef HAVE_TERMCAP_H
#include <termcap.h>
#endif
#ifdef HAVE_CURSES_H
#include <curses.h>
#endif
#ifdef HAVE_NCURSES_H
#include <ncurses.h>
#endif

#include "el.h"

#if !defined(HAVE_TERMCAP_H) && defined(HAVE_TERM_H)
#include <term.h>
#endif
#include <sys/types.h>
#include <sys/ioctl.h>

/*
 * IMPORTANT NOTE: these routines are allowed to look at the current screen
 * and the current possition assuming that it is correct.  If this is not
 * true, then the update will be WRONG!  This is (should be) a valid
 * assumption...
 */

#define	TC_BUFSIZE	2048

#define	GoodStr(a)	(el->el_term.t_str[a] != NULL && \
			    el->el_term.t_str[a][0] != '\0')
#define	Str(a)		el->el_term.t_str[a]
#define	Val(a)		el->el_term.t_val[a]

#ifdef notdef
private const struct {
	const char *b_name;
	int b_rate;
} baud_rate[] = {
#ifdef B0
	{ "0", B0 },
#endif
#ifdef B50
	{ "50", B50 },
#endif
#ifdef B75
	{ "75", B75 },
#endif
#ifdef B110
	{ "110", B110 },
#endif
#ifdef B134
	{ "134", B134 },
#endif
#ifdef B150
	{ "150", B150 },
#endif
#ifdef B200
	{ "200", B200 },
#endif
#ifdef B300
	{ "300", B300 },
#endif
#ifdef B600
	{ "600", B600 },
#endif
#ifdef B900
	{ "900", B900 },
#endif
#ifdef B1200
	{ "1200", B1200 },
#endif
#ifdef B1800
	{ "1800", B1800 },
#endif
#ifdef B2400
	{ "2400", B2400 },
#endif
#ifdef B3600
	{ "3600", B3600 },
#endif
#ifdef B4800
	{ "4800", B4800 },
#endif
#ifdef B7200
	{ "7200", B7200 },
#endif
#ifdef B9600
	{ "9600", B9600 },
#endif
#ifdef EXTA
	{ "19200", EXTA },
#endif
#ifdef B19200
	{ "19200", B19200 },
#endif
#ifdef EXTB
	{ "38400", EXTB },
#endif
#ifdef B38400
	{ "38400", B38400 },
#endif
	{ NULL, 0 }
};
#endif

private const struct termcapstr {
	const char *name;
	const char *long_name;
} tstr[] = {
#define	T_al	0
	{ "al", "add new blank line" },
#define	T_bl	1
	{ "bl", "audible bell" },
#define	T_cd	2
	{ "cd", "clear to bottom" },
#define	T_ce	3
	{ "ce", "clear to end of line" },
#define	T_ch	4
	{ "ch", "cursor to horiz pos" },
#define	T_cl	5
	{ "cl", "clear screen" },
#define	T_dc	6
	{ "dc", "delete a character" },
#define	T_dl	7
	{ "dl", "delete a line" },
#define	T_dm	8
	{ "dm", "start delete mode" },
#define	T_ed	9
	{ "ed", "end delete mode" },
#define	T_ei	10
	{ "ei", "end insert mode" },
#define	T_fs	11
	{ "fs", "cursor from status line" },
#define	T_ho	12
	{ "ho", "home cursor" },
#define	T_ic	13
	{ "ic", "insert character" },
#define	T_im	14
	{ "im", "start insert mode" },
#define	T_ip	15
	{ "ip", "insert padding" },
#define	T_kd	16
	{ "kd", "sends cursor down" },
#define	T_kl	17
	{ "kl", "sends cursor left" },
#define	T_kr	18
	{ "kr", "sends cursor right" },
#define	T_ku	19
	{ "ku", "sends cursor up" },
#define	T_md	20
	{ "md", "begin bold" },
#define	T_me	21
	{ "me", "end attributes" },
#define	T_nd	22
	{ "nd", "non destructive space" },
#define	T_se	23
	{ "se", "end standout" },
#define	T_so	24
	{ "so", "begin standout" },
#define	T_ts	25
	{ "ts", "cursor to status line" },
#define	T_up	26
	{ "up", "cursor up one" },
#define	T_us	27
	{ "us", "begin underline" },
#define	T_ue	28
	{ "ue", "end underline" },
#define	T_vb	29
	{ "vb", "visible bell" },
#define	T_DC	30
	{ "DC", "delete multiple chars" },
#define	T_DO	31
	{ "DO", "cursor down multiple" },
#define	T_IC	32
	{ "IC", "insert multiple chars" },
#define	T_LE	33
	{ "LE", "cursor left multiple" },
#define	T_RI	34
	{ "RI", "cursor right multiple" },
#define	T_UP	35
	{ "UP", "cursor up multiple" },
#define	T_kh	36
	{ "kh", "send cursor home" },
#define	T_at7	37
	{ "@7", "send cursor end" },
#define	T_str	38
	{ NULL, NULL }
};

private const struct termcapval {
	const char *name;
	const char *long_name;
} tval[] = {
#define	T_am	0
	{ "am", "has automatic margins" },
#define	T_pt	1
	{ "pt", "has physical tabs" },
#define	T_li	2
	{ "li", "Number of lines" },
#define	T_co	3
	{ "co", "Number of columns" },
#define	T_km	4
	{ "km", "Has meta key" },
#define	T_xt	5
	{ "xt", "Tab chars destructive" },
#define	T_xn	6
	{ "xn", "newline ignored at right margin" },
#define	T_MT	7
	{ "MT", "Has meta key" },			/* XXX? */
#define	T_val	8
	{ NULL, NULL, }
};
/* do two or more of the attributes use me */

private void	term_setflags(EditLine *);
private int	term_rebuffer_display(EditLine *);
private void	term_free_display(EditLine *);
private int	term_alloc_display(EditLine *);
private void	term_alloc(EditLine *, const struct termcapstr *, const char *);
private void	term_init_arrow(EditLine *);
private void	term_reset_arrow(EditLine *);


private FILE *term_outfile = NULL;	/* XXX: How do we fix that? */


/* term_setflags():
 *	Set the terminal capability flags
 */
private void
term_setflags(EditLine *el)
{
	EL_FLAGS = 0;
	if (el->el_tty.t_tabs)
		EL_FLAGS |= (Val(T_pt) && !Val(T_xt)) ? TERM_CAN_TAB : 0;

	EL_FLAGS |= (Val(T_km) || Val(T_MT)) ? TERM_HAS_META : 0;
	EL_FLAGS |= GoodStr(T_ce) ? TERM_CAN_CEOL : 0;
	EL_FLAGS |= (GoodStr(T_dc) || GoodStr(T_DC)) ? TERM_CAN_DELETE : 0;
	EL_FLAGS |= (GoodStr(T_im) || GoodStr(T_ic) || GoodStr(T_IC)) ?
	    TERM_CAN_INSERT : 0;
	EL_FLAGS |= (GoodStr(T_up) || GoodStr(T_UP)) ? TERM_CAN_UP : 0;
	EL_FLAGS |= Val(T_am) ? TERM_HAS_AUTO_MARGINS : 0;
	EL_FLAGS |= Val(T_xn) ? TERM_HAS_MAGIC_MARGINS : 0;

	if (GoodStr(T_me) && GoodStr(T_ue))
		EL_FLAGS |= (strcmp(Str(T_me), Str(T_ue)) == 0) ?
		    TERM_CAN_ME : 0;
	else
		EL_FLAGS &= ~TERM_CAN_ME;
	if (GoodStr(T_me) && GoodStr(T_se))
		EL_FLAGS |= (strcmp(Str(T_me), Str(T_se)) == 0) ?
		    TERM_CAN_ME : 0;


#ifdef DEBUG_SCREEN
	if (!EL_CAN_UP) {
		(void) fprintf(el->el_errfile,
		    "WARNING: Your terminal cannot move up.\n");
		(void) fprintf(el->el_errfile,
		    "Editing may be odd for long lines.\n");
	}
	if (!EL_CAN_CEOL)
		(void) fprintf(el->el_errfile, "no clear EOL capability.\n");
	if (!EL_CAN_DELETE)
		(void) fprintf(el->el_errfile, "no delete char capability.\n");
	if (!EL_CAN_INSERT)
		(void) fprintf(el->el_errfile, "no insert char capability.\n");
#endif /* DEBUG_SCREEN */
}


/* term_init():
 *	Initialize the terminal stuff
 */
protected int
term_init(EditLine *el)
{

	el->el_term.t_buf = (char *) el_malloc(TC_BUFSIZE);
	if (el->el_term.t_buf == NULL)
		return (-1);
	el->el_term.t_cap = (char *) el_malloc(TC_BUFSIZE);
	if (el->el_term.t_cap == NULL)
		return (-1);
	el->el_term.t_fkey = (fkey_t *) el_malloc(A_K_NKEYS * sizeof(fkey_t));
	if (el->el_term.t_fkey == NULL)
		return (-1);
	el->el_term.t_loc = 0;
	el->el_term.t_str = (char **) el_malloc(T_str * sizeof(char *));
	if (el->el_term.t_str == NULL)
		return (-1);
	(void) memset(el->el_term.t_str, 0, T_str * sizeof(char *));
	el->el_term.t_val = (int *) el_malloc(T_val * sizeof(int));
	if (el->el_term.t_val == NULL)
		return (-1);
	(void) memset(el->el_term.t_val, 0, T_val * sizeof(int));
	term_outfile = el->el_outfile;
	(void) term_set(el, NULL);
	term_init_arrow(el);
	return (0);
}
/* term_end():
 *	Clean up the terminal stuff
 */
protected void
term_end(EditLine *el)
{

	el_free((ptr_t) el->el_term.t_buf);
	el->el_term.t_buf = NULL;
	el_free((ptr_t) el->el_term.t_cap);
	el->el_term.t_cap = NULL;
	el->el_term.t_loc = 0;
	el_free((ptr_t) el->el_term.t_str);
	el->el_term.t_str = NULL;
	el_free((ptr_t) el->el_term.t_val);
	el->el_term.t_val = NULL;
	term_free_display(el);
}


/* term_alloc():
 *	Maintain a string pool for termcap strings
 */
private void
term_alloc(EditLine *el, const struct termcapstr *t, const char *cap)
{
	char termbuf[TC_BUFSIZE];
	int tlen, clen;
	char **tlist = el->el_term.t_str;
	char **tmp, **str = &tlist[t - tstr];

	if (cap == NULL || *cap == '\0') {
		*str = NULL;
		return;
	} else
		clen = strlen(cap);

	tlen = *str == NULL ? 0 : strlen(*str);

	/*
         * New string is shorter; no need to allocate space
         */
	if (clen <= tlen) {
		(void) strcpy(*str, cap);	/* XXX strcpy is safe */
		return;
	}
	/*
         * New string is longer; see if we have enough space to append
         */
	if (el->el_term.t_loc + 3 < TC_BUFSIZE) {
						/* XXX strcpy is safe */
		(void) strcpy(*str = &el->el_term.t_buf[el->el_term.t_loc],
		    cap);
		el->el_term.t_loc += clen + 1;	/* one for \0 */
		return;
	}
	/*
         * Compact our buffer; no need to check compaction, cause we know it
         * fits...
         */
	tlen = 0;
	for (tmp = tlist; tmp < &tlist[T_str]; tmp++)
		if (*tmp != NULL && *tmp != '\0' && *tmp != *str) {
			char *ptr;

			for (ptr = *tmp; *ptr != '\0'; termbuf[tlen++] = *ptr++)
				continue;
			termbuf[tlen++] = '\0';
		}
	memcpy(el->el_term.t_buf, termbuf, TC_BUFSIZE);
	el->el_term.t_loc = tlen;
	if (el->el_term.t_loc + 3 >= TC_BUFSIZE) {
		(void) fprintf(el->el_errfile,
		    "Out of termcap string space.\n");
		return;
	}
					/* XXX strcpy is safe */
	(void) strcpy(*str = &el->el_term.t_buf[el->el_term.t_loc], cap);
	el->el_term.t_loc += clen + 1;	/* one for \0 */
	return;
}


/* term_rebuffer_display():
 *	Rebuffer the display after the screen changed size
 */
private int
term_rebuffer_display(EditLine *el)
{
	coord_t *c = &el->el_term.t_size;

	term_free_display(el);

	c->h = Val(T_co);
	c->v = Val(T_li);

	if (term_alloc_display(el) == -1)
		return (-1);
	return (0);
}


/* term_alloc_display():
 *	Allocate a new display.
 */
private int
term_alloc_display(EditLine *el)
{
	int i;
	char **b;
	coord_t *c = &el->el_term.t_size;

	b = (char **) el_malloc((size_t) (sizeof(char *) * (c->v + 1)));
	if (b == NULL)
		return (-1);
	for (i = 0; i < c->v; i++) {
		b[i] = (char *) el_malloc((size_t) (sizeof(char) * (c->h + 1)));
		if (b[i] == NULL)
			return (-1);
	}
	b[c->v] = NULL;
	el->el_display = b;

	b = (char **) el_malloc((size_t) (sizeof(char *) * (c->v + 1)));
	if (b == NULL)
		return (-1);
	for (i = 0; i < c->v; i++) {
		b[i] = (char *) el_malloc((size_t) (sizeof(char) * (c->h + 1)));
		if (b[i] == NULL)
			return (-1);
	}
	b[c->v] = NULL;
	el->el_vdisplay = b;
	return (0);
}


/* term_free_display():
 *	Free the display buffers
 */
private void
term_free_display(EditLine *el)
{
	char **b;
	char **bufp;

	b = el->el_display;
	el->el_display = NULL;
	if (b != NULL) {
		for (bufp = b; *bufp != NULL; bufp++)
			el_free((ptr_t) * bufp);
		el_free((ptr_t) b);
	}
	b = el->el_vdisplay;
	el->el_vdisplay = NULL;
	if (b != NULL) {
		for (bufp = b; *bufp != NULL; bufp++)
			el_free((ptr_t) * bufp);
		el_free((ptr_t) b);
	}
}


/* term_move_to_line():
 *	move to line <where> (first line == 0)
 * 	as efficiently as possible
 */
protected void
term_move_to_line(EditLine *el, int where)
{
	int del;

	if (where == el->el_cursor.v)
		return;

	if (where > el->el_term.t_size.v) {
#ifdef DEBUG_SCREEN
		(void) fprintf(el->el_errfile,
		    "term_move_to_line: where is ridiculous: %d\r\n", where);
#endif /* DEBUG_SCREEN */
		return;
	}
	if ((del = where - el->el_cursor.v) > 0) {
		while (del > 0) {
			if (EL_HAS_AUTO_MARGINS &&
			    el->el_display[el->el_cursor.v][0] != '\0') {
				/* move without newline */
				term_move_to_char(el, el->el_term.t_size.h - 1);
				term_overwrite(el,
				    &el->el_display[el->el_cursor.v][el->el_cursor.h],
				    1);
				/* updates Cursor */
				del--;
			} else {
				if ((del > 1) && GoodStr(T_DO)) {
					(void) tputs(tgoto(Str(T_DO), del, del),
					    del, term__putc);
					del = 0;
				} else {
					for (; del > 0; del--)
						term__putc('\n');
					/* because the \n will become \r\n */
					el->el_cursor.h = 0;
				}
			}
		}
	} else {		/* del < 0 */
		if (GoodStr(T_UP) && (-del > 1 || !GoodStr(T_up)))
			(void) tputs(tgoto(Str(T_UP), -del, -del), -del,
			    term__putc);
		else {
			if (GoodStr(T_up))
				for (; del < 0; del++)
					(void) tputs(Str(T_up), 1, term__putc);
		}
	}
	el->el_cursor.v = where;/* now where is here */
}


/* term_move_to_char():
 *	Move to the character position specified
 */
protected void
term_move_to_char(EditLine *el, int where)
{
	int del, i;

mc_again:
	if (where == el->el_cursor.h)
		return;

	if (where > el->el_term.t_size.h) {
#ifdef DEBUG_SCREEN
		(void) fprintf(el->el_errfile,
		    "term_move_to_char: where is riduculous: %d\r\n", where);
#endif /* DEBUG_SCREEN */
		return;
	}
	if (!where) {		/* if where is first column */
		term__putc('\r');	/* do a CR */
		el->el_cursor.h = 0;
		return;
	}
	del = where - el->el_cursor.h;

	if ((del < -4 || del > 4) && GoodStr(T_ch))
		/* go there directly */
		(void) tputs(tgoto(Str(T_ch), where, where), where, term__putc);
	else {
		if (del > 0) {	/* moving forward */
			if ((del > 4) && GoodStr(T_RI))
				(void) tputs(tgoto(Str(T_RI), del, del),
				    del, term__putc);
			else {
					/* if I can do tabs, use them */
				if (EL_CAN_TAB) {
					if ((el->el_cursor.h & 0370) !=
					    (where & 0370)) {
						/* if not within tab stop */
						for (i =
						    (el->el_cursor.h & 0370);
						    i < (where & 0370);
						    i += 8)
							term__putc('\t');	
							/* then tab over */
						el->el_cursor.h = where & 0370;
					}
				}
				/*
				 * it's usually cheaper to just write the
				 * chars, so we do.
				 */
				/*
				 * NOTE THAT term_overwrite() WILL CHANGE
				 * el->el_cursor.h!!!
				 */
				term_overwrite(el,
				    &el->el_display[el->el_cursor.v][el->el_cursor.h],
				    where - el->el_cursor.h);

			}
		} else {	/* del < 0 := moving backward */
			if ((-del > 4) && GoodStr(T_LE))
				(void) tputs(tgoto(Str(T_LE), -del, -del),
				    -del, term__putc);
			else {	/* can't go directly there */
				/*
				 * if the "cost" is greater than the "cost"
				 * from col 0
				 */
				if (EL_CAN_TAB ?
				    (((unsigned int)-del) > (((unsigned int) where >> 3) +
				     (where & 07)))
				    : (-del > where)) {
					term__putc('\r');	/* do a CR */
					el->el_cursor.h = 0;
					goto mc_again;	/* and try again */
				}
				for (i = 0; i < -del; i++)
					term__putc('\b');
			}
		}
	}
	el->el_cursor.h = where;		/* now where is here */
}


/* term_overwrite():
 *	Overstrike num characters
 */
protected void
term_overwrite(EditLine *el, const char *cp, int n)
{
	if (n <= 0)
		return;		/* catch bugs */

	if (n > el->el_term.t_size.h) {
#ifdef DEBUG_SCREEN
		(void) fprintf(el->el_errfile,
		    "term_overwrite: n is riduculous: %d\r\n", n);
#endif /* DEBUG_SCREEN */
		return;
	}
	do {
		term__putc(*cp++);
		el->el_cursor.h++;
	} while (--n);

	if (el->el_cursor.h >= el->el_term.t_size.h) {	/* wrap? */
		if (EL_HAS_AUTO_MARGINS) {	/* yes */
			el->el_cursor.h = 0;
			el->el_cursor.v++;
			if (EL_HAS_MAGIC_MARGINS) {
				/* force the wrap to avoid the "magic"
				 * situation */
				char c;
				if ((c = el->el_display[el->el_cursor.v][el->el_cursor.h])
				    != '\0')
					term_overwrite(el, &c, 1);
				else
					term__putc(' ');
				el->el_cursor.h = 1;
			}
		} else		/* no wrap, but cursor stays on screen */
			el->el_cursor.h = el->el_term.t_size.h;
	}
}


/* term_deletechars():
 *	Delete num characters
 */
protected void
term_deletechars(EditLine *el, int num)
{
	if (num <= 0)
		return;

	if (!EL_CAN_DELETE) {
#ifdef DEBUG_EDIT
		(void) fprintf(el->el_errfile, "   ERROR: cannot delete   \n");
#endif /* DEBUG_EDIT */
		return;
	}
	if (num > el->el_term.t_size.h) {
#ifdef DEBUG_SCREEN
		(void) fprintf(el->el_errfile,
		    "term_deletechars: num is riduculous: %d\r\n", num);
#endif /* DEBUG_SCREEN */
		return;
	}
	if (GoodStr(T_DC))	/* if I have multiple delete */
		if ((num > 1) || !GoodStr(T_dc)) {	/* if dc would be more
							 * expen. */
			(void) tputs(tgoto(Str(T_DC), num, num),
			    num, term__putc);
			return;
		}
	if (GoodStr(T_dm))	/* if I have delete mode */
		(void) tputs(Str(T_dm), 1, term__putc);

	if (GoodStr(T_dc))	/* else do one at a time */
		while (num--)
			(void) tputs(Str(T_dc), 1, term__putc);

	if (GoodStr(T_ed))	/* if I have delete mode */
		(void) tputs(Str(T_ed), 1, term__putc);
}


/* term_insertwrite():
 *	Puts terminal in insert character mode or inserts num
 *	characters in the line
 */
protected void
term_insertwrite(EditLine *el, char *cp, int num)
{
	if (num <= 0)
		return;
	if (!EL_CAN_INSERT) {
#ifdef DEBUG_EDIT
		(void) fprintf(el->el_errfile, "   ERROR: cannot insert   \n");
#endif /* DEBUG_EDIT */
		return;
	}
	if (num > el->el_term.t_size.h) {
#ifdef DEBUG_SCREEN
		(void) fprintf(el->el_errfile,
		    "StartInsert: num is riduculous: %d\r\n", num);
#endif /* DEBUG_SCREEN */
		return;
	}
	if (GoodStr(T_IC))	/* if I have multiple insert */
		if ((num > 1) || !GoodStr(T_ic)) {
				/* if ic would be more expensive */
			(void) tputs(tgoto(Str(T_IC), num, num),
			    num, term__putc);
			term_overwrite(el, cp, num);
				/* this updates el_cursor.h */
			return;
		}
	if (GoodStr(T_im) && GoodStr(T_ei)) {	/* if I have insert mode */
		(void) tputs(Str(T_im), 1, term__putc);

		el->el_cursor.h += num;
		do
			term__putc(*cp++);
		while (--num);

		if (GoodStr(T_ip))	/* have to make num chars insert */
			(void) tputs(Str(T_ip), 1, term__putc);

		(void) tputs(Str(T_ei), 1, term__putc);
		return;
	}
	do {
		if (GoodStr(T_ic))	/* have to make num chars insert */
			(void) tputs(Str(T_ic), 1, term__putc);
					/* insert a char */

		term__putc(*cp++);

		el->el_cursor.h++;

		if (GoodStr(T_ip))	/* have to make num chars insert */
			(void) tputs(Str(T_ip), 1, term__putc);
					/* pad the inserted char */

	} while (--num);
}


/* term_clear_EOL():
 *	clear to end of line.  There are num characters to clear
 */
protected void
term_clear_EOL(EditLine *el, int num)
{
	int i;

	if (EL_CAN_CEOL && GoodStr(T_ce))
		(void) tputs(Str(T_ce), 1, term__putc);
	else {
		for (i = 0; i < num; i++)
			term__putc(' ');
		el->el_cursor.h += num;	/* have written num spaces */
	}
}


/* term_clear_screen():
 *	Clear the screen
 */
protected void
term_clear_screen(EditLine *el)
{				/* clear the whole screen and home */

	if (GoodStr(T_cl))
		/* send the clear screen code */
		(void) tputs(Str(T_cl), Val(T_li), term__putc);
	else if (GoodStr(T_ho) && GoodStr(T_cd)) {
		(void) tputs(Str(T_ho), Val(T_li), term__putc);	/* home */
		/* clear to bottom of screen */
		(void) tputs(Str(T_cd), Val(T_li), term__putc);
	} else {
		term__putc('\r');
		term__putc('\n');
	}
}


/* term_beep():
 *	Beep the way the terminal wants us
 */
protected void
term_beep(EditLine *el)
{
	if (GoodStr(T_bl))
		/* what termcap says we should use */
		(void) tputs(Str(T_bl), 1, term__putc);
	else
		term__putc('\007');	/* an ASCII bell; ^G */
}


#ifdef notdef
/* term_clear_to_bottom():
 *	Clear to the bottom of the screen
 */
protected void
term_clear_to_bottom(EditLine *el)
{
	if (GoodStr(T_cd))
		(void) tputs(Str(T_cd), Val(T_li), term__putc);
	else if (GoodStr(T_ce))
		(void) tputs(Str(T_ce), Val(T_li), term__putc);
}
#endif


/* term_set():
 *	Read in the terminal capabilities from the requested terminal
 */
protected int
term_set(EditLine *el, const char *term)
{
	int i;
	char buf[TC_BUFSIZE];
	char *area;
	const struct termcapstr *t;
	sigset_t oset, nset;
	int lins, cols;

	(void) sigemptyset(&nset);
	(void) sigaddset(&nset, SIGWINCH);
	(void) sigprocmask(SIG_BLOCK, &nset, &oset);

	area = buf;


	if (term == NULL)
		term = getenv("TERM");

	if (!term || !term[0])
		term = "dumb";

	if (strcmp(term, "emacs") == 0)
		el->el_flags |= EDIT_DISABLED;

	memset(el->el_term.t_cap, 0, TC_BUFSIZE);

	i = tgetent(el->el_term.t_cap, term);

	if (i <= 0) {
		if (i == -1)
			(void) fprintf(el->el_errfile,
			    "Cannot read termcap database;\n");
		else if (i == 0)
			(void) fprintf(el->el_errfile,
			    "No entry for terminal type \"%s\";\n", term);
		(void) fprintf(el->el_errfile,
		    "using dumb terminal settings.\n");
		Val(T_co) = 80;	/* do a dumb terminal */
		Val(T_pt) = Val(T_km) = Val(T_li) = 0;
		Val(T_xt) = Val(T_MT);
		for (t = tstr; t->name != NULL; t++)
			term_alloc(el, t, NULL);
	} else {
		/* auto/magic margins */
		Val(T_am) = tgetflag("am");
		Val(T_xn) = tgetflag("xn");
		/* Can we tab */
		Val(T_pt) = tgetflag("pt");
		Val(T_xt) = tgetflag("xt");
		/* do we have a meta? */
		Val(T_km) = tgetflag("km");
		Val(T_MT) = tgetflag("MT");
		/* Get the size */
		Val(T_co) = tgetnum("co");
		Val(T_li) = tgetnum("li");
		for (t = tstr; t->name != NULL; t++)
			term_alloc(el, t, tgetstr(t->name, &area));
	}

	if (Val(T_co) < 2)
		Val(T_co) = 80;	/* just in case */
	if (Val(T_li) < 1)
		Val(T_li) = 24;

	el->el_term.t_size.v = Val(T_co);
	el->el_term.t_size.h = Val(T_li);

	term_setflags(el);

				/* get the correct window size */
	(void) term_get_size(el, &lins, &cols);
	if (term_change_size(el, lins, cols) == -1)
		return (-1);
	(void) sigprocmask(SIG_SETMASK, &oset, NULL);
	term_bind_arrow(el);
	return (i <= 0 ? -1 : 0);
}


/* term_get_size():
 *	Return the new window size in lines and cols, and
 *	true if the size was changed.
 */
protected int
term_get_size(EditLine *el, int *lins, int *cols)
{

	*cols = Val(T_co);
	*lins = Val(T_li);

#ifdef TIOCGWINSZ
	{
		struct winsize ws;
		if (ioctl(el->el_infd, TIOCGWINSZ, (ioctl_t) & ws) != -1) {
			if (ws.ws_col)
				*cols = ws.ws_col;
			if (ws.ws_row)
				*lins = ws.ws_row;
		}
	}
#endif
#ifdef TIOCGSIZE
	{
		struct ttysize ts;
		if (ioctl(el->el_infd, TIOCGSIZE, (ioctl_t) & ts) != -1) {
			if (ts.ts_cols)
				*cols = ts.ts_cols;
			if (ts.ts_lines)
				*lins = ts.ts_lines;
		}
	}
#endif
	return (Val(T_co) != *cols || Val(T_li) != *lins);
}


/* term_change_size():
 *	Change the size of the terminal
 */
protected int
term_change_size(EditLine *el, int lins, int cols)
{
	/*
         * Just in case
         */
	Val(T_co) = (cols < 2) ? 80 : cols;
	Val(T_li) = (lins < 1) ? 24 : lins;

	/* re-make display buffers */
	if (term_rebuffer_display(el) == -1)
		return (-1);
	re_clear_display(el);
	return (0);
}


/* term_init_arrow():
 *	Initialize the arrow key bindings from termcap
 */
private void
term_init_arrow(EditLine *el)
{
	fkey_t *arrow = el->el_term.t_fkey;

	arrow[A_K_DN].name = "down";
	arrow[A_K_DN].key = T_kd;
	arrow[A_K_DN].fun.cmd = ED_NEXT_HISTORY;
	arrow[A_K_DN].type = XK_CMD;

	arrow[A_K_UP].name = "up";
	arrow[A_K_UP].key = T_ku;
	arrow[A_K_UP].fun.cmd = ED_PREV_HISTORY;
	arrow[A_K_UP].type = XK_CMD;

	arrow[A_K_LT].name = "left";
	arrow[A_K_LT].key = T_kl;
	arrow[A_K_LT].fun.cmd = ED_PREV_CHAR;
	arrow[A_K_LT].type = XK_CMD;

	arrow[A_K_RT].name = "right";
	arrow[A_K_RT].key = T_kr;
	arrow[A_K_RT].fun.cmd = ED_NEXT_CHAR;
	arrow[A_K_RT].type = XK_CMD;

	arrow[A_K_HO].name = "home";
	arrow[A_K_HO].key = T_kh;
	arrow[A_K_HO].fun.cmd = ED_MOVE_TO_BEG;
	arrow[A_K_HO].type = XK_CMD;

	arrow[A_K_EN].name = "end";
	arrow[A_K_EN].key = T_at7;
	arrow[A_K_EN].fun.cmd = ED_MOVE_TO_END;
	arrow[A_K_EN].type = XK_CMD;
}


/* term_reset_arrow():
 *	Reset arrow key bindings
 */
private void
term_reset_arrow(EditLine *el)
{
	fkey_t *arrow = el->el_term.t_fkey;
	static const char strA[] = {033, '[', 'A', '\0'};
	static const char strB[] = {033, '[', 'B', '\0'};
	static const char strC[] = {033, '[', 'C', '\0'};
	static const char strD[] = {033, '[', 'D', '\0'};
	static const char strH[] = {033, '[', 'H', '\0'};
	static const char strF[] = {033, '[', 'F', '\0'};
	static const char stOA[] = {033, 'O', 'A', '\0'};
	static const char stOB[] = {033, 'O', 'B', '\0'};
	static const char stOC[] = {033, 'O', 'C', '\0'};
	static const char stOD[] = {033, 'O', 'D', '\0'};
	static const char stOH[] = {033, 'O', 'H', '\0'};
	static const char stOF[] = {033, 'O', 'F', '\0'};

	el_key_add(el, strA, &arrow[A_K_UP].fun, arrow[A_K_UP].type);
	el_key_add(el, strB, &arrow[A_K_DN].fun, arrow[A_K_DN].type);
	el_key_add(el, strC, &arrow[A_K_RT].fun, arrow[A_K_RT].type);
	el_key_add(el, strD, &arrow[A_K_LT].fun, arrow[A_K_LT].type);
	el_key_add(el, strH, &arrow[A_K_HO].fun, arrow[A_K_HO].type);
	el_key_add(el, strF, &arrow[A_K_EN].fun, arrow[A_K_EN].type);
	el_key_add(el, stOA, &arrow[A_K_UP].fun, arrow[A_K_UP].type);
	el_key_add(el, stOB, &arrow[A_K_DN].fun, arrow[A_K_DN].type);
	el_key_add(el, stOC, &arrow[A_K_RT].fun, arrow[A_K_RT].type);
	el_key_add(el, stOD, &arrow[A_K_LT].fun, arrow[A_K_LT].type);
	el_key_add(el, stOH, &arrow[A_K_HO].fun, arrow[A_K_HO].type);
	el_key_add(el, stOF, &arrow[A_K_EN].fun, arrow[A_K_EN].type);

	if (el->el_map.type == MAP_VI) {
		el_key_add(el, &strA[1], &arrow[A_K_UP].fun, arrow[A_K_UP].type);
		el_key_add(el, &strB[1], &arrow[A_K_DN].fun, arrow[A_K_DN].type);
		el_key_add(el, &strC[1], &arrow[A_K_RT].fun, arrow[A_K_RT].type);
		el_key_add(el, &strD[1], &arrow[A_K_LT].fun, arrow[A_K_LT].type);
		el_key_add(el, &strH[1], &arrow[A_K_HO].fun, arrow[A_K_HO].type);
		el_key_add(el, &strF[1], &arrow[A_K_EN].fun, arrow[A_K_EN].type);
		el_key_add(el, &stOA[1], &arrow[A_K_UP].fun, arrow[A_K_UP].type);
		el_key_add(el, &stOB[1], &arrow[A_K_DN].fun, arrow[A_K_DN].type);
		el_key_add(el, &stOC[1], &arrow[A_K_RT].fun, arrow[A_K_RT].type);
		el_key_add(el, &stOD[1], &arrow[A_K_LT].fun, arrow[A_K_LT].type);
		el_key_add(el, &stOH[1], &arrow[A_K_HO].fun, arrow[A_K_HO].type);
		el_key_add(el, &stOF[1], &arrow[A_K_EN].fun, arrow[A_K_EN].type);
	}
}


/* term_set_arrow():
 *	Set an arrow key binding
 */
protected int
term_set_arrow(EditLine *el, const char *name, key_value_t *fun, int type)
{
	fkey_t *arrow = el->el_term.t_fkey;
	int i;

	for (i = 0; i < A_K_NKEYS; i++)
		if (strcmp(name, arrow[i].name) == 0) {
			arrow[i].fun = *fun;
			arrow[i].type = type;
			return (0);
		}
	return (-1);
}


/* term_clear_arrow():
 *	Clear an arrow key binding
 */
protected int
term_clear_arrow(EditLine *el, const char *name)
{
	fkey_t *arrow = el->el_term.t_fkey;
	int i;

	for (i = 0; i < A_K_NKEYS; i++)
		if (strcmp(name, arrow[i].name) == 0) {
			arrow[i].type = XK_NOD;
			return (0);
		}
	return (-1);
}


/* term_print_arrow():
 *	Print the arrow key bindings
 */
protected void
term_print_arrow(EditLine *el, const char *name)
{
	int i;
	fkey_t *arrow = el->el_term.t_fkey;

	for (i = 0; i < A_K_NKEYS; i++)
		if (*name == '\0' || strcmp(name, arrow[i].name) == 0)
			if (arrow[i].type != XK_NOD)
				el_key_kprint(el, arrow[i].name, &arrow[i].fun,
				    arrow[i].type);
}


/* term_bind_arrow():
 *	Bind the arrow keys
 */
protected void
term_bind_arrow(EditLine *el)
{
	el_action_t *map;
	const el_action_t *dmap;
	int i, j;
	char *p;
	fkey_t *arrow = el->el_term.t_fkey;

	/* Check if the components needed are initialized */
	if (el->el_term.t_buf == NULL || el->el_map.key == NULL)
		return;

	map = el->el_map.type == MAP_VI ? el->el_map.alt : el->el_map.key;
	dmap = el->el_map.type == MAP_VI ? el->el_map.vic : el->el_map.emacs;

	term_reset_arrow(el);

	for (i = 0; i < A_K_NKEYS; i++) {
		p = el->el_term.t_str[arrow[i].key];
		if (p && *p) {
			j = (unsigned char) *p;
			/*
		         * Assign the arrow keys only if:
		         *
		         * 1. They are multi-character arrow keys and the user
		         *    has not re-assigned the leading character, or
		         *    has re-assigned the leading character to be
		         *	  ED_SEQUENCE_LEAD_IN
		         * 2. They are single arrow keys pointing to an
			 *    unassigned key.
		         */
			if (arrow[i].type == XK_NOD)
				el_key_clear(el, map, p);
			else {
				if (p[1] && (dmap[j] == map[j] ||
					map[j] == ED_SEQUENCE_LEAD_IN)) {
					el_key_add(el, p, &arrow[i].fun,
					    arrow[i].type);
					map[j] = ED_SEQUENCE_LEAD_IN;
				} else if (map[j] == ED_UNASSIGNED) {
					el_key_clear(el, map, p);
					if (arrow[i].type == XK_CMD)
						map[j] = arrow[i].fun.cmd;
					else
						el_key_add(el, p, &arrow[i].fun,
						           arrow[i].type);
				}
			}
		}
	}
}


/* term__putc():
 *	Add a character
 */
protected int
term__putc(int c)
{

	return (fputc(c, term_outfile));
}


/* term__flush():
 *	Flush output
 */
protected void
term__flush(void)
{

	(void) fflush(term_outfile);
}


/* term_telltc():
 *	Print the current termcap characteristics
 */
char		*el_key__decode_str(const char *, char *, const char *);

protected int
/*ARGSUSED*/
term_telltc(EditLine *el, int argc __attribute__((unused)),
	    const char **argv __attribute__((unused)))
{
	const struct termcapstr *t;
	char **ts;
	char upbuf[EL_BUFSIZ];

	(void) fprintf(el->el_outfile, "\n\tYour terminal has the\n");
	(void) fprintf(el->el_outfile, "\tfollowing characteristics:\n\n");
	(void) fprintf(el->el_outfile, "\tIt has %d columns and %d lines\n",
	    Val(T_co), Val(T_li));
	(void) fprintf(el->el_outfile,
	    "\tIt has %s meta key\n", EL_HAS_META ? "a" : "no");
	(void) fprintf(el->el_outfile,
	    "\tIt can%suse tabs\n", EL_CAN_TAB ? " " : "not ");
	(void) fprintf(el->el_outfile, "\tIt %s automatic margins\n",
	    EL_HAS_AUTO_MARGINS ? "has" : "does not have");
	if (EL_HAS_AUTO_MARGINS)
		(void) fprintf(el->el_outfile, "\tIt %s magic margins\n",
		    EL_HAS_MAGIC_MARGINS ? "has" : "does not have");

	for (t = tstr, ts = el->el_term.t_str; t->name != NULL; t++, ts++)
		(void) fprintf(el->el_outfile, "\t%25s (%s) == %s\n",
		    t->long_name,
		    t->name, *ts && **ts ?
		    el_key__decode_str(*ts, upbuf, "") : "(empty)");
	(void) fputc('\n', el->el_outfile);
	return (0);
}


/* term_settc():
 *	Change the current terminal characteristics
 */
protected int
/*ARGSUSED*/
term_settc(EditLine *el, int argc __attribute__((unused)), 
	   const char **argv __attribute__((unused)))
{
	const struct termcapstr *ts;
	const struct termcapval *tv;
	const char *what, *how;

	if (argv == NULL || argv[1] == NULL || argv[2] == NULL)
		return (-1);

	what = argv[1];
	how = argv[2];

	/*
         * Do the strings first
         */
	for (ts = tstr; ts->name != NULL; ts++)
		if (strcmp(ts->name, what) == 0)
			break;

	if (ts->name != NULL) {
		term_alloc(el, ts, how);
		term_setflags(el);
		return (0);
	}
	/*
         * Do the numeric ones second
         */
	for (tv = tval; tv->name != NULL; tv++)
		if (strcmp(tv->name, what) == 0)
			break;

	if (tv->name != NULL) {
		if (tv == &tval[T_pt] || tv == &tval[T_km] ||
		    tv == &tval[T_am] || tv == &tval[T_xn]) {
			if (strcmp(how, "yes") == 0)
				el->el_term.t_val[tv - tval] = 1;
			else if (strcmp(how, "no") == 0)
				el->el_term.t_val[tv - tval] = 0;
			else {
				(void) fprintf(el->el_errfile,
				    "settc: Bad value `%s'.\n", how);
				return (-1);
			}
			term_setflags(el);
			if (term_change_size(el, Val(T_li), Val(T_co)) == -1)
				return (-1);
			return (0);
		} else {
			long i;
			char *ep;

			i = strtol(how, &ep, 10);
			if (*ep != '\0') {
				(void) fprintf(el->el_errfile,
				    "settc: Bad value `%s'.\n", how);
				return (-1);
			}
			el->el_term.t_val[tv - tval] = (int) i;
			el->el_term.t_size.v = Val(T_co);
			el->el_term.t_size.h = Val(T_li);
			if (tv == &tval[T_co] || tv == &tval[T_li])
				if (term_change_size(el, Val(T_li), Val(T_co))
				    == -1)
					return (-1);
			return (0);
		}
	}
	return (-1);
}


/* term_echotc():
 *	Print the termcap string out with variable substitution
 */
protected int
/*ARGSUSED*/
term_echotc(EditLine *el __attribute__((unused)), 
	    int argc __attribute__((unused)),
	    const char **argv __attribute__((unused)))
{
	char *cap, *scap, *ep;
	int arg_need, arg_cols, arg_rows;
	int verbose = 0, silent = 0;
	char *area;
	static const char fmts[] = "%s\n", fmtd[] = "%d\n";
	const struct termcapstr *t;
	char buf[TC_BUFSIZE];
	long i;

	area = buf;

	if (argv == NULL || argv[1] == NULL)
		return (-1);
	argv++;

	if (argv[0][0] == '-') {
		switch (argv[0][1]) {
		case 'v':
			verbose = 1;
			break;
		case 's':
			silent = 1;
			break;
		default:
			/* stderror(ERR_NAME | ERR_TCUSAGE); */
			break;
		}
		argv++;
	}
	if (!*argv || *argv[0] == '\0')
		return (0);
	if (strcmp(*argv, "tabs") == 0) {
		(void) fprintf(el->el_outfile, fmts, EL_CAN_TAB ? "yes" : "no");
		return (0);
	} else if (strcmp(*argv, "meta") == 0) {
		(void) fprintf(el->el_outfile, fmts, Val(T_km) ? "yes" : "no");
		return (0);
	} else if (strcmp(*argv, "xn") == 0) {
		(void) fprintf(el->el_outfile, fmts, EL_HAS_MAGIC_MARGINS ?
		    "yes" : "no");
		return (0);
	} else if (strcmp(*argv, "am") == 0) {
		(void) fprintf(el->el_outfile, fmts, EL_HAS_AUTO_MARGINS ?
		    "yes" : "no");
		return (0);
	} else if (strcmp(*argv, "baud") == 0) {
#ifdef notdef
		int i;

		for (i = 0; baud_rate[i].b_name != NULL; i++)
			if (el->el_tty.t_speed == baud_rate[i].b_rate) {
				(void) fprintf(el->el_outfile, fmts,
				    baud_rate[i].b_name);
				return (0);
			}
		(void) fprintf(el->el_outfile, fmtd, 0);
#else
		(void) fprintf(el->el_outfile, fmtd, el->el_tty.t_speed);
#endif
		return (0);
	} else if (strcmp(*argv, "rows") == 0 || strcmp(*argv, "lines") == 0) {
		(void) fprintf(el->el_outfile, fmtd, Val(T_li));
		return (0);
	} else if (strcmp(*argv, "cols") == 0) {
		(void) fprintf(el->el_outfile, fmtd, Val(T_co));
		return (0);
	}
	/*
         * Try to use our local definition first
         */
	scap = NULL;
	for (t = tstr; t->name != NULL; t++)
		if (strcmp(t->name, *argv) == 0) {
			scap = el->el_term.t_str[t - tstr];
			break;
		}
	if (t->name == NULL)
		scap = tgetstr(*argv, &area);
	if (!scap || scap[0] == '\0') {
		if (!silent)
			(void) fprintf(el->el_errfile,
			    "echotc: Termcap parameter `%s' not found.\n",
			    *argv);
		return (-1);
	}
	/*
         * Count home many values we need for this capability.
         */
	for (cap = scap, arg_need = 0; *cap; cap++)
		if (*cap == '%')
			switch (*++cap) {
			case 'd':
			case '2':
			case '3':
			case '.':
			case '+':
				arg_need++;
				break;
			case '%':
			case '>':
			case 'i':
			case 'r':
			case 'n':
			case 'B':
			case 'D':
				break;
			default:
				/*
				 * hpux has lot's of them...
				 */
				if (verbose)
					(void) fprintf(el->el_errfile,
				"echotc: Warning: unknown termcap %% `%c'.\n",
					    *cap);
				/* This is bad, but I won't complain */
				break;
			}

	switch (arg_need) {
	case 0:
		argv++;
		if (*argv && *argv[0]) {
			if (!silent)
				(void) fprintf(el->el_errfile,
				    "echotc: Warning: Extra argument `%s'.\n",
				    *argv);
			return (-1);
		}
		(void) tputs(scap, 1, term__putc);
		break;
	case 1:
		argv++;
		if (!*argv || *argv[0] == '\0') {
			if (!silent)
				(void) fprintf(el->el_errfile,
				    "echotc: Warning: Missing argument.\n");
			return (-1);
		}
		arg_cols = 0;
		i = strtol(*argv, &ep, 10);
		if (*ep != '\0' || i < 0) {
			if (!silent)
				(void) fprintf(el->el_errfile,
				    "echotc: Bad value `%s' for rows.\n",
				    *argv);
			return (-1);
		}
		arg_rows = (int) i;
		argv++;
		if (*argv && *argv[0]) {
			if (!silent)
				(void) fprintf(el->el_errfile,
				    "echotc: Warning: Extra argument `%s'.\n",
				    *argv);
			return (-1);
		}
		(void) tputs(tgoto(scap, arg_cols, arg_rows), 1, term__putc);
		break;
	default:
		/* This is wrong, but I will ignore it... */
		if (verbose)
			(void) fprintf(el->el_errfile,
			 "echotc: Warning: Too many required arguments (%d).\n",
			    arg_need);
		/* FALLTHROUGH */
	case 2:
		argv++;
		if (!*argv || *argv[0] == '\0') {
			if (!silent)
				(void) fprintf(el->el_errfile,
				    "echotc: Warning: Missing argument.\n");
			return (-1);
		}
		i = strtol(*argv, &ep, 10);
		if (*ep != '\0' || i < 0) {
			if (!silent)
				(void) fprintf(el->el_errfile,
				    "echotc: Bad value `%s' for cols.\n",
				    *argv);
			return (-1);
		}
		arg_cols = (int) i;
		argv++;
		if (!*argv || *argv[0] == '\0') {
			if (!silent)
				(void) fprintf(el->el_errfile,
				    "echotc: Warning: Missing argument.\n");
			return (-1);
		}
		i = strtol(*argv, &ep, 10);
		if (*ep != '\0' || i < 0) {
			if (!silent)
				(void) fprintf(el->el_errfile,
				    "echotc: Bad value `%s' for rows.\n",
				    *argv);
			return (-1);
		}
		arg_rows = (int) i;
		if (*ep != '\0') {
			if (!silent)
				(void) fprintf(el->el_errfile,
				    "echotc: Bad value `%s'.\n", *argv);
			return (-1);
		}
		argv++;
		if (*argv && *argv[0]) {
			if (!silent)
				(void) fprintf(el->el_errfile,
				    "echotc: Warning: Extra argument `%s'.\n",
				    *argv);
			return (-1);
		}
		(void) tputs(tgoto(scap, arg_cols, arg_rows), arg_rows,
		    term__putc);
		break;
	}
	return (0);
}
