/*	$NetBSD: chared.c,v 1.18 2002/11/20 16:50:08 christos Exp $	*/

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
static char sccsid[] = "@(#)chared.c	8.1 (Berkeley) 6/4/93";
#else
__RCSID("$NetBSD: chared.c,v 1.18 2002/11/20 16:50:08 christos Exp $");
#endif
#endif /* not lint && not SCCSID */

/*
 * chared.c: Character editor utilities
 */
#include <stdlib.h>
#include "el.h"

/* value to leave unused in line buffer */
#define	EL_LEAVE	2

/* cv_undo():
 *	Handle state for the vi undo command
 */
protected void
cv_undo(EditLine *el)
{
	c_undo_t *vu = &el->el_chared.c_undo;
	c_redo_t *r = &el->el_chared.c_redo;
	int size;

	/* Save entire line for undo */
	size = el->el_line.lastchar - el->el_line.buffer;
	vu->len = size;
	vu->cursor = el->el_line.cursor - el->el_line.buffer;
	memcpy(vu->buf, el->el_line.buffer, (size_t)size);

	/* save command info for redo */
	r->count = el->el_state.doingarg ? el->el_state.argument : 0;
	r->action = el->el_chared.c_vcmd.action;
	r->pos = r->buf;
	r->cmd = el->el_state.thiscmd;
	r->ch = el->el_state.thisch;
}

/* cv_yank():
 *	Save yank/delete data for paste
 */
protected void
cv_yank(EditLine *el, const char *ptr, int size)
{
	c_kill_t *k = &el->el_chared.c_kill;

	memcpy(k->buf, ptr, size +0u);
	k->last = k->buf + size;
}


/* c_insert():
 *	Insert num characters
 */
protected void
c_insert(EditLine *el, int num)
{
	char *cp;

	if (el->el_line.lastchar + num >= el->el_line.limit) {
		if (!ch_enlargebufs(el, num +0u))
			return;		/* can't go past end of buffer */
	}

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

	if (el->el_map.current != el->el_map.emacs) {
		cv_undo(el);
		cv_yank(el, el->el_line.cursor, num);
	}

	if (num > 0) {
		char *cp;

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

	if (el->el_map.current != el->el_map.emacs) {
		cv_undo(el);
		cv_yank(el, el->el_line.cursor - num, num);
	}

	if (num > 0) {
		char *cp;

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
	return (isalnum(p) || strchr("*?_-.[]~=", p) != NULL);
}


/* cv__isword():
 *	Return if p is part of a word according to vi
 */
protected int
cv__isword(int p)
{
	if (isalnum(p) || p == '_')
		return 1;
	if (isgraph(p))
		return 2;
	return 0;
}


/* cv__isWord():
 *	Return if p is part of a big word according to vi
 */
protected int
cv__isWord(int p)
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
		if (n || el->el_chared.c_vcmd.action != (DELETE|INSERT))
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
cv_prev_word(char *p, char *low, int n, int (*wtest)(int))
{
	int test;

	p--;
	while (n--) {
		while ((p > low) && isspace((unsigned char) *p))
			p--;
		test = (*wtest)((unsigned char) *p);
		while ((p >= low) && (*wtest)((unsigned char) *p) == test)
			p--;
	}
	p++;

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
	int action = el->el_chared.c_vcmd.action;

	if (action & INSERT)
		el->el_map.current = el->el_map.key;

	if (el->el_chared.c_vcmd.pos == 0)
		/* sanity */
		return;

	size = el->el_line.cursor - el->el_chared.c_vcmd.pos;
	if (size == 0)
		size = 1;
	el->el_line.cursor = el->el_chared.c_vcmd.pos;
	if (action & YANK) {
		if (size > 0)
			cv_yank(el, el->el_line.cursor, size);
		else
			cv_yank(el, el->el_line.cursor + size, -size);
	} else {
		if (size > 0) {
			c_delafter(el, size);
			re_refresh_cursor(el);
		} else  {
			c_delbefore(el, -size);
			el->el_line.cursor += size;
		}
	}
	el->el_chared.c_vcmd.action = NOP;
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
cv__endword(char *p, char *high, int n, int (*wtest)(int))
{
	int test;

	p++;

	while (n--) {
		while ((p < high) && isspace((unsigned char) *p))
			p++;

		test = (*wtest)((unsigned char) *p);
		while ((p < high) && (*wtest)((unsigned char) *p) == test)
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
	el->el_line.limit		= &el->el_line.buffer[EL_BUFSIZ - EL_LEAVE];

	el->el_chared.c_undo.buf	= (char *) el_malloc(EL_BUFSIZ);
	if (el->el_chared.c_undo.buf == NULL)
		return (-1);
	(void) memset(el->el_chared.c_undo.buf, 0, EL_BUFSIZ);
	el->el_chared.c_undo.len	= -1;
	el->el_chared.c_undo.cursor	= 0;
	el->el_chared.c_redo.buf	= (char *) el_malloc(EL_BUFSIZ);
	if (el->el_chared.c_redo.buf == NULL)
		return (-1);
	el->el_chared.c_redo.pos	= el->el_chared.c_redo.buf;
	el->el_chared.c_redo.lim	= el->el_chared.c_redo.buf + EL_BUFSIZ;
	el->el_chared.c_redo.cmd	= ED_UNASSIGNED;

	el->el_chared.c_vcmd.action	= NOP;
	el->el_chared.c_vcmd.pos	= el->el_line.buffer;

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

	el->el_chared.c_undo.len	= -1;
	el->el_chared.c_undo.cursor	= 0;

	el->el_chared.c_vcmd.action	= NOP;
	el->el_chared.c_vcmd.pos	= el->el_line.buffer;

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
	/* don't set new size until all buffers are enlarged */
	el->el_line.limit  = &newbuffer[sz - EL_LEAVE];

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
	el->el_chared.c_undo.buf = newbuffer;

	newbuffer = el_realloc(el->el_chared.c_redo.buf, newsz);
	if (!newbuffer)
		return 0;
	el->el_chared.c_redo.pos = newbuffer +
			(el->el_chared.c_redo.pos - el->el_chared.c_redo.buf);
	el->el_chared.c_redo.lim = newbuffer +
			(el->el_chared.c_redo.lim - el->el_chared.c_redo.buf);
	el->el_chared.c_redo.buf = newbuffer;
	
	if (!hist_enlargebuf(el, sz, newsz))
		return 0;

	/* Safe to set enlarged buffer size */
	el->el_line.limit  = &newbuffer[newsz - EL_LEAVE];
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
	el_free((ptr_t) el->el_chared.c_redo.buf);
	el->el_chared.c_redo.buf = NULL;
	el->el_chared.c_redo.pos = NULL;
	el->el_chared.c_redo.lim = NULL;
	el->el_chared.c_redo.cmd = ED_UNASSIGNED;
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
c_gets(EditLine *el, char *buf, const char *prompt)
{
	char ch;
	int len;
	char *cp = el->el_line.buffer;

	if (prompt) {
		len = strlen(prompt);
		memcpy(cp, prompt, len + 0u);
		cp += len;
	}
	len = 0;

	for (;;) {
		el->el_line.cursor = cp;
		*cp = ' ';
		el->el_line.lastchar = cp + 1;
		re_refresh(el);

		if (el_getc(el, &ch) != 1) {
			ed_end_of_file(el, 0);
			len = -1;
			break;
		}

		switch (ch) {

		case 0010:	/* Delete and backspace */
		case 0177:
			if (len <= 0) {
				len = -1;
				break;
			}
			cp--;
			continue;

		case 0033:	/* ESC */
		case '\r':	/* Newline */
		case '\n':
			buf[len] = ch;
			break;

		default:
			if (len >= EL_BUFSIZ - 16)
				term_beep(el);
			else {
				buf[len++] = ch;
				*cp++ = ch;
			}
			continue;
		}
		break;
	}

	el->el_line.buffer[0] = '\0';
	el->el_line.lastchar = el->el_line.buffer;
	el->el_line.cursor = el->el_line.buffer;
	return len;
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
