/*	$NetBSD: chared.c,v 1.14 2001/05/17 01:02:17 christos Exp $	*/

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

#include "compat.h"
#if !defined(lint) && !defined(SCCSID)
#if 0
static char sccsid[] = "@(#)chared.c	8.1 (Berkeley) 6/4/93";
#else
__RCSID("$NetBSD: chared.c,v 1.14 2001/05/17 01:02:17 christos Exp $");
#endif
#endif /* not lint && not SCCSID */

/*
 * chared.c: Character editor utilities
 */
#include "sys.h"

#include <stdlib.h>
#include "el.h"

/* value to leave unused in line buffer */
#define	EL_LEAVE	2

/* cv_undo():
 *	Handle state for the vi undo command
 */
protected void
cv_undo(EditLine *el,int action, size_t size, char *ptr)
{
	c_undo_t *vu = &el->el_chared.c_undo;
	vu->action = action;
	vu->ptr    = ptr;
	vu->isize  = size;
	(void) memcpy(vu->buf, vu->ptr, size);
#ifdef DEBUG_UNDO
	(void) fprintf(el->el_errfile, "Undo buffer \"%s\" size = +%d -%d\n",
	       vu->ptr, vu->isize, vu->dsize);
#endif
}


/* c_insert():
 *	Insert num characters
 */
protected void
c_insert(EditLine *el, int num)
{
	char *cp;

	if (el->el_line.lastchar + num >= el->el_line.limit)
		return;			/* can't go past end of buffer */

	if (el->el_line.cursor < el->el_line.lastchar) {
		/* if I must move chars */
		for (cp = el->el_line.lastchar; cp >= el->el_line.cursor; cp--)
			cp[num] = *cp;
	}
	el->el_line.lastchar += num;
}


/* c_delafter():
 *	Delete num characters after the cursor
 */
protected void
c_delafter(EditLine *el, int num)
{

	if (el->el_line.cursor + num > el->el_line.lastchar)
		num = el->el_line.lastchar - el->el_line.cursor;

	if (num > 0) {
		char *cp;

		if (el->el_map.current != el->el_map.emacs)
			cv_undo(el, INSERT, (size_t)num, el->el_line.cursor);

		for (cp = el->el_line.cursor; cp <= el->el_line.lastchar; cp++)
			*cp = cp[num];

		el->el_line.lastchar -= num;
	}
}


/* c_delbefore():
 *	Delete num characters before the cursor
 */
protected void
c_delbefore(EditLine *el, int num)
{

	if (el->el_line.cursor - num < el->el_line.buffer)
		num = el->el_line.cursor - el->el_line.buffer;

	if (num > 0) {
		char *cp;

		if (el->el_map.current != el->el_map.emacs)
			cv_undo(el, INSERT, (size_t)num,
			    el->el_line.cursor - num);

		for (cp = el->el_line.cursor - num;
		    cp <= el->el_line.lastchar;
		    cp++)
			*cp = cp[num];

		el->el_line.lastchar -= num;
	}
}


/* ce__isword():
 *	Return if p is part of a word according to emacs
 */
protected int
ce__isword(int p)
{
	return (isalpha(p) || isdigit(p) || strchr("*?_-.[]~=", p) != NULL);
}


/* cv__isword():
 *	Return if p is part of a word according to vi
 */
protected int
cv__isword(int p)
{
	return (!isspace(p));
}


/* c__prev_word():
 *	Find the previous word
 */
protected char *
c__prev_word(char *p, char *low, int n, int (*wtest)(int))
{
	p--;

	while (n--) {
		while ((p >= low) && !(*wtest)((unsigned char) *p))
			p--;
		while ((p >= low) && (*wtest)((unsigned char) *p))
			p--;
	}

	/* cp now points to one character before the word */
	p++;
	if (p < low)
		p = low;
	/* cp now points where we want it */
	return (p);
}


/* c__next_word():
 *	Find the next word
 */
protected char *
c__next_word(char *p, char *high, int n, int (*wtest)(int))
{
	while (n--) {
		while ((p < high) && !(*wtest)((unsigned char) *p))
			p++;
		while ((p < high) && (*wtest)((unsigned char) *p))
			p++;
	}
	if (p > high)
		p = high;
	/* p now points where we want it */
	return (p);
}

/* cv_next_word():
 *	Find the next word vi style
 */
protected char *
cv_next_word(EditLine *el, char *p, char *high, int n, int (*wtest)(int))
{
	int test;

	while (n--) {
		test = (*wtest)((unsigned char) *p);
		while ((p < high) && (*wtest)((unsigned char) *p) == test)
			p++;
		/*
		 * vi historically deletes with cw only the word preserving the
		 * trailing whitespace! This is not what 'w' does..
		 */
		if (el->el_chared.c_vcmd.action != (DELETE|INSERT))
			while ((p < high) && isspace((unsigned char) *p))
				p++;
	}

	/* p now points where we want it */
	if (p > high)
		return (high);
	else
		return (p);
}


/* cv_prev_word():
 *	Find the previous word vi style
 */
protected char *
cv_prev_word(EditLine *el, char *p, char *low, int n, int (*wtest)(int))
{
	int test;

	while (n--) {
		p--;
		/*
		 * vi historically deletes with cb only the word preserving the
		 * leading whitespace! This is not what 'b' does..
		 */
		if (el->el_chared.c_vcmd.action != (DELETE|INSERT))
			while ((p > low) && isspace((unsigned char) *p))
				p--;
		test = (*wtest)((unsigned char) *p);
		while ((p >= low) && (*wtest)((unsigned char) *p) == test)
			p--;
		p++;
		while (isspace((unsigned char) *p))
			p++;
	}

	/* p now points where we want it */
	if (p < low)
		return (low);
	else
		return (p);
}


#ifdef notdef
/* c__number():
 *	Ignore character p points to, return number appearing after that.
 * 	A '$' by itself means a big number; "$-" is for negative; '^' means 1.
 * 	Return p pointing to last char used.
 */
protected char *
c__number(
    char *p,	/* character position */
    int *num,	/* Return value	*/
    int dval)	/* dval is the number to subtract from like $-3 */
{
	int i;
	int sign = 1;

	if (*++p == '^') {
		*num = 1;
		return (p);
	}
	if (*p == '$') {
		if (*++p != '-') {
			*num = 0x7fffffff;	/* Handle $ */
			return (--p);
		}
		sign = -1;			/* Handle $- */
		++p;
	}
	for (i = 0; isdigit((unsigned char) *p); i = 10 * i + *p++ - '0')
		continue;
	*num = (sign < 0 ? dval - i : i);
	return (--p);
}
#endif

/* cv_delfini():
 *	Finish vi delete action
 */
protected void
cv_delfini(EditLine *el)
{
	int size;
	int oaction;

	if (el->el_chared.c_vcmd.action & INSERT)
		el->el_map.current = el->el_map.key;

	oaction = el->el_chared.c_vcmd.action;
	el->el_chared.c_vcmd.action = NOP;

	if (el->el_chared.c_vcmd.pos == 0)
		return;


	if (el->el_line.cursor > el->el_chared.c_vcmd.pos) {
		size = (int) (el->el_line.cursor - el->el_chared.c_vcmd.pos);
		c_delbefore(el, size);
		el->el_line.cursor = el->el_chared.c_vcmd.pos;
		re_refresh_cursor(el);
	} else if (el->el_line.cursor < el->el_chared.c_vcmd.pos) {
		size = (int)(el->el_chared.c_vcmd.pos - el->el_line.cursor);
		c_delafter(el, size);
	} else {
		size = 1;
		c_delafter(el, size);
	}
	switch (oaction) {
	case DELETE|INSERT:
		el->el_chared.c_undo.action = DELETE|INSERT;
		break;
	case DELETE:
		el->el_chared.c_undo.action = INSERT;
		break;
	case NOP:
	case INSERT:
	default:
		EL_ABORT((el->el_errfile, "Bad oaction %d\n", oaction));
		break;
	}


	el->el_chared.c_undo.ptr = el->el_line.cursor;
	el->el_chared.c_undo.dsize = size;
}


#ifdef notdef
/* ce__endword():
 *	Go to the end of this word according to emacs
 */
protected char *
ce__endword(char *p, char *high, int n)
{
	p++;

	while (n--) {
		while ((p < high) && isspace((unsigned char) *p))
			p++;
		while ((p < high) && !isspace((unsigned char) *p))
			p++;
	}

	p--;
	return (p);
}
#endif


/* cv__endword():
 *	Go to the end of this word according to vi
 */
protected char *
cv__endword(char *p, char *high, int n)
{
	p++;

	while (n--) {
		while ((p < high) && isspace((unsigned char) *p))
			p++;

		if (isalnum((unsigned char) *p))
			while ((p < high) && isalnum((unsigned char) *p))
				p++;
		else
			while ((p < high) && !(isspace((unsigned char) *p) ||
			    isalnum((unsigned char) *p)))
				p++;
	}
	p--;
	return (p);
}

/* ch_init():
 *	Initialize the character editor
 */
protected int
ch_init(EditLine *el)
{
	el->el_line.buffer		= (char *) el_malloc(EL_BUFSIZ);
	if (el->el_line.buffer == NULL)
		return (-1);

	(void) memset(el->el_line.buffer, 0, EL_BUFSIZ);
	el->el_line.cursor		= el->el_line.buffer;
	el->el_line.lastchar		= el->el_line.buffer;
	el->el_line.limit		= &el->el_line.buffer[EL_BUFSIZ - 2];

	el->el_chared.c_undo.buf	= (char *) el_malloc(EL_BUFSIZ);
	if (el->el_chared.c_undo.buf == NULL)
		return (-1);
	(void) memset(el->el_chared.c_undo.buf, 0, EL_BUFSIZ);
	el->el_chared.c_undo.action	= NOP;
	el->el_chared.c_undo.isize	= 0;
	el->el_chared.c_undo.dsize	= 0;
	el->el_chared.c_undo.ptr	= el->el_line.buffer;

	el->el_chared.c_vcmd.action	= NOP;
	el->el_chared.c_vcmd.pos	= el->el_line.buffer;
	el->el_chared.c_vcmd.ins	= el->el_line.buffer;

	el->el_chared.c_kill.buf	= (char *) el_malloc(EL_BUFSIZ);
	if (el->el_chared.c_kill.buf == NULL)
		return (-1);
	(void) memset(el->el_chared.c_kill.buf, 0, EL_BUFSIZ);
	el->el_chared.c_kill.mark	= el->el_line.buffer;
	el->el_chared.c_kill.last	= el->el_chared.c_kill.buf;

	el->el_map.current		= el->el_map.key;

	el->el_state.inputmode		= MODE_INSERT; /* XXX: save a default */
	el->el_state.doingarg		= 0;
	el->el_state.metanext		= 0;
	el->el_state.argument		= 1;
	el->el_state.lastcmd		= ED_UNASSIGNED;

	el->el_chared.c_macro.nline	= NULL;
	el->el_chared.c_macro.level	= -1;
	el->el_chared.c_macro.macro	= (char **) el_malloc(EL_MAXMACRO *
	    sizeof(char *));
	if (el->el_chared.c_macro.macro == NULL)
		return (-1);
	return (0);
}

/* ch_reset():
 *	Reset the character editor
 */
protected void
ch_reset(EditLine *el)
{
	el->el_line.cursor		= el->el_line.buffer;
	el->el_line.lastchar		= el->el_line.buffer;

	el->el_chared.c_undo.action	= NOP;
	el->el_chared.c_undo.isize	= 0;
	el->el_chared.c_undo.dsize	= 0;
	el->el_chared.c_undo.ptr	= el->el_line.buffer;

	el->el_chared.c_vcmd.action	= NOP;
	el->el_chared.c_vcmd.pos	= el->el_line.buffer;
	el->el_chared.c_vcmd.ins	= el->el_line.buffer;

	el->el_chared.c_kill.mark	= el->el_line.buffer;

	el->el_map.current		= el->el_map.key;

	el->el_state.inputmode		= MODE_INSERT; /* XXX: save a default */
	el->el_state.doingarg		= 0;
	el->el_state.metanext		= 0;
	el->el_state.argument		= 1;
	el->el_state.lastcmd		= ED_UNASSIGNED;

	el->el_chared.c_macro.level	= -1;

	el->el_history.eventno		= 0;
}

/* ch_enlargebufs():
 *	Enlarge line buffer to be able to hold twice as much characters.
 *	Returns 1 if successful, 0 if not.
 */
protected int
ch_enlargebufs(el, addlen)
	EditLine *el;
	size_t addlen;
{
	size_t sz, newsz;
	char *newbuffer, *oldbuf, *oldkbuf;

	sz = el->el_line.limit - el->el_line.buffer + EL_LEAVE;
	newsz = sz * 2;
	/*
	 * If newly required length is longer than current buffer, we need
	 * to make the buffer big enough to hold both old and new stuff.
	 */
	if (addlen > sz) {
		while(newsz - sz < addlen)
			newsz *= 2;
	}

	/*
	 * Reallocate line buffer.
	 */
	newbuffer = el_realloc(el->el_line.buffer, newsz);
	if (!newbuffer)
		return 0;

	/* zero the newly added memory, leave old data in */
	(void) memset(&newbuffer[sz], 0, newsz - sz);
	    
	oldbuf = el->el_line.buffer;

	el->el_line.buffer = newbuffer;
	el->el_line.cursor = newbuffer + (el->el_line.cursor - oldbuf);
	el->el_line.lastchar = newbuffer + (el->el_line.lastchar - oldbuf);
	el->el_line.limit  = &newbuffer[newsz - EL_LEAVE];

	/*
	 * Reallocate kill buffer.
	 */
	newbuffer = el_realloc(el->el_chared.c_kill.buf, newsz);
	if (!newbuffer)
		return 0;

	/* zero the newly added memory, leave old data in */
	(void) memset(&newbuffer[sz], 0, newsz - sz);

	oldkbuf = el->el_chared.c_kill.buf;

	el->el_chared.c_kill.buf = newbuffer;
	el->el_chared.c_kill.last = newbuffer +
					(el->el_chared.c_kill.last - oldkbuf);
	el->el_chared.c_kill.mark = el->el_line.buffer +
					(el->el_chared.c_kill.mark - oldbuf);

	/*
	 * Reallocate undo buffer.
	 */
	newbuffer = el_realloc(el->el_chared.c_undo.buf, newsz);
	if (!newbuffer)
		return 0;

	/* zero the newly added memory, leave old data in */
	(void) memset(&newbuffer[sz], 0, newsz - sz);

	el->el_chared.c_undo.ptr = el->el_line.buffer +
				    (el->el_chared.c_undo.ptr - oldbuf);
	el->el_chared.c_undo.buf = newbuffer;
	
	if (!hist_enlargebuf(el, sz, newsz))
		return 0;

	return 1;
}

/* ch_end():
 *	Free the data structures used by the editor
 */
protected void
ch_end(EditLine *el)
{
	el_free((ptr_t) el->el_line.buffer);
	el->el_line.buffer = NULL;
	el->el_line.limit = NULL;
	el_free((ptr_t) el->el_chared.c_undo.buf);
	el->el_chared.c_undo.buf = NULL;
	el_free((ptr_t) el->el_chared.c_kill.buf);
	el->el_chared.c_kill.buf = NULL;
	el_free((ptr_t) el->el_chared.c_macro.macro);
	el->el_chared.c_macro.macro = NULL;
	ch_reset(el);
}


/* el_insertstr():
 *	Insert string at cursorI
 */
public int
el_insertstr(EditLine *el, const char *s)
{
	size_t len;

	if ((len = strlen(s)) == 0)
		return (-1);
	if (el->el_line.lastchar + len >= el->el_line.limit) {
		if (!ch_enlargebufs(el, len))
			return (-1);
	}

	c_insert(el, (int)len);
	while (*s)
		*el->el_line.cursor++ = *s++;
	return (0);
}


/* el_deletestr():
 *	Delete num characters before the cursor
 */
public void
el_deletestr(EditLine *el, int n)
{
	if (n <= 0)
		return;

	if (el->el_line.cursor < &el->el_line.buffer[n])
		return;

	c_delbefore(el, n);		/* delete before dot */
	el->el_line.cursor -= n;
	if (el->el_line.cursor < el->el_line.buffer)
		el->el_line.cursor = el->el_line.buffer;
}

/* c_gets():
 *	Get a string
 */
protected int
c_gets(EditLine *el, char *buf)
{
	char ch;
	int len = 0;

	for (ch = 0; ch == 0;) {
		if (el_getc(el, &ch) != 1)
			return (ed_end_of_file(el, 0));
		switch (ch) {
		case 0010:	/* Delete and backspace */
		case 0177:
			if (len > 1) {
				*el->el_line.cursor-- = '\0';
				el->el_line.lastchar = el->el_line.cursor;
				buf[len--] = '\0';
			} else {
				el->el_line.buffer[0] = '\0';
				el->el_line.lastchar = el->el_line.buffer;
				el->el_line.cursor = el->el_line.buffer;
				return (CC_REFRESH);
			}
			re_refresh(el);
			ch = 0;
			break;

		case 0033:	/* ESC */
		case '\r':	/* Newline */
		case '\n':
			break;

		default:
			if (len >= EL_BUFSIZ)
				term_beep(el);
			else {
				buf[len++] = ch;
				*el->el_line.cursor++ = ch;
				el->el_line.lastchar = el->el_line.cursor;
			}
			re_refresh(el);
			ch = 0;
			break;
		}
	}
	buf[len] = ch;
	return (len);
}


/* c_hpos():
 *	Return the current horizontal position of the cursor
 */
protected int
c_hpos(EditLine *el)
{
	char *ptr;

	/*
	 * Find how many characters till the beginning of this line.
	 */
	if (el->el_line.cursor == el->el_line.buffer)
		return (0);
	else {
		for (ptr = el->el_line.cursor - 1;
		     ptr >= el->el_line.buffer && *ptr != '\n';
		     ptr--)
			continue;
		return (el->el_line.cursor - ptr - 1);
	}
}
