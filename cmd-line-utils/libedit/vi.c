/*	$NetBSD: vi.c,v 1.8 2000/09/04 22:06:33 lukem Exp $	*/

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

/*
 * vi.c: Vi mode commands.
 */
#include "sys.h"
#include "el.h"

private el_action_t	cv_action(EditLine *, int);
private el_action_t	cv_paste(EditLine *, int);

/* cv_action():
 *	Handle vi actions.
 */
private el_action_t
cv_action(EditLine *el, int c)
{
	char *cp, *kp;

	if (el->el_chared.c_vcmd.action & DELETE) {
		el->el_chared.c_vcmd.action = NOP;
		el->el_chared.c_vcmd.pos = 0;

		el->el_chared.c_undo.isize = 0;
		el->el_chared.c_undo.dsize = 0;
		kp = el->el_chared.c_undo.buf;
		for (cp = el->el_line.buffer; cp < el->el_line.lastchar; cp++) {
			*kp++ = *cp;
			el->el_chared.c_undo.dsize++;
		}

		el->el_chared.c_undo.action = INSERT;
		el->el_chared.c_undo.ptr = el->el_line.buffer;
		el->el_line.lastchar = el->el_line.buffer;
		el->el_line.cursor = el->el_line.buffer;
		if (c & INSERT)
			el->el_map.current = el->el_map.key;

		return (CC_REFRESH);
	}
	el->el_chared.c_vcmd.pos = el->el_line.cursor;
	el->el_chared.c_vcmd.action = c;
	return (CC_ARGHACK);

#ifdef notdef
	/*
         * I don't think that this is needed. But we keep it for now
         */
	else
	if (el_chared.c_vcmd.action == NOP) {
		el->el_chared.c_vcmd.pos = el->el_line.cursor;
		el->el_chared.c_vcmd.action = c;
		return (CC_ARGHACK);
	} else {
		el->el_chared.c_vcmd.action = 0;
		el->el_chared.c_vcmd.pos = 0;
		return (CC_ERROR);
	}
#endif
}


/* cv_paste():
 *	Paste previous deletion before or after the cursor
 */
private el_action_t
cv_paste(EditLine *el, int c)
{
	char *ptr;
	c_undo_t *un = &el->el_chared.c_undo;

#ifdef DEBUG_PASTE
	(void) fprintf(el->el_errfile, "Paste: %x \"%s\" +%d -%d\n",
	    un->action, un->buf, un->isize, un->dsize);
#endif
	if (un->isize == 0)
		return (CC_ERROR);

	if (!c && el->el_line.cursor < el->el_line.lastchar)
		el->el_line.cursor++;
	ptr = el->el_line.cursor;

	c_insert(el, (int) un->isize);
	if (el->el_line.cursor + un->isize > el->el_line.lastchar)
		return (CC_ERROR);
	(void) memcpy(ptr, un->buf, un->isize);
	return (CC_REFRESH);
}


/* vi_paste_next():
 *	Vi paste previous deletion to the right of the cursor
 *	[p]
 */
protected el_action_t
/*ARGSUSED*/
vi_paste_next(EditLine *el, int c __attribute__((unused)))
{

	return (cv_paste(el, 0));
}


/* vi_paste_prev():
 *	Vi paste previous deletion to the left of the cursor
 *	[P]
 */
protected el_action_t
/*ARGSUSED*/
vi_paste_prev(EditLine *el, int c __attribute__((unused)))
{

	return (cv_paste(el, 1));
}


/* vi_prev_space_word():
 *	Vi move to the previous space delimited word
 *	[B]
 */
protected el_action_t
/*ARGSUSED*/
vi_prev_space_word(EditLine *el, int c __attribute__((unused)))
{

	if (el->el_line.cursor == el->el_line.buffer)
		return (CC_ERROR);

	el->el_line.cursor = cv_prev_word(el, el->el_line.cursor,
	    el->el_line.buffer,
	    el->el_state.argument,
	    cv__isword);

	if (el->el_chared.c_vcmd.action & DELETE) {
		cv_delfini(el);
		return (CC_REFRESH);
	}
	return (CC_CURSOR);
}


/* vi_prev_word():
 *	Vi move to the previous word
 *	[B]
 */
protected el_action_t
/*ARGSUSED*/
vi_prev_word(EditLine *el, int c __attribute__((unused)))
{

	if (el->el_line.cursor == el->el_line.buffer)
		return (CC_ERROR);

	el->el_line.cursor = cv_prev_word(el, el->el_line.cursor,
	    el->el_line.buffer,
	    el->el_state.argument,
	    ce__isword);

	if (el->el_chared.c_vcmd.action & DELETE) {
		cv_delfini(el);
		return (CC_REFRESH);
	}
	return (CC_CURSOR);
}


/* vi_next_space_word():
 *	Vi move to the next space delimited word
 *	[W]
 */
protected el_action_t
/*ARGSUSED*/
vi_next_space_word(EditLine *el, int c __attribute__((unused)))
{

	if (el->el_line.cursor == el->el_line.lastchar)
		return (CC_ERROR);

	el->el_line.cursor = cv_next_word(el, el->el_line.cursor,
	    el->el_line.lastchar,
	    el->el_state.argument,
	    cv__isword);

	if (el->el_map.type == MAP_VI)
		if (el->el_chared.c_vcmd.action & DELETE) {
			cv_delfini(el);
			return (CC_REFRESH);
		}
	return (CC_CURSOR);
}


/* vi_next_word():
 *	Vi move to the next word
 *	[w]
 */
protected el_action_t
/*ARGSUSED*/
vi_next_word(EditLine *el, int c __attribute__((unused)))
{

	if (el->el_line.cursor == el->el_line.lastchar)
		return (CC_ERROR);

	el->el_line.cursor = cv_next_word(el, el->el_line.cursor,
	    el->el_line.lastchar,
	    el->el_state.argument,
	    ce__isword);

	if (el->el_map.type == MAP_VI)
		if (el->el_chared.c_vcmd.action & DELETE) {
			cv_delfini(el);
			return (CC_REFRESH);
		}
	return (CC_CURSOR);
}


/* vi_change_case():
 *	Vi change case of character under the cursor and advance one character
 *	[~]
 */
protected el_action_t
vi_change_case(EditLine *el, int c)
{

	if (el->el_line.cursor < el->el_line.lastchar) {
		c = *el->el_line.cursor;
		if (isupper(c))
			*el->el_line.cursor++ = tolower(c);
		else if (islower(c))
			*el->el_line.cursor++ = toupper(c);
		else
			el->el_line.cursor++;
		re_fastaddc(el);
		return (CC_NORM);
	}
	return (CC_ERROR);
}


/* vi_change_meta():
 *	Vi change prefix command
 *	[c]
 */
protected el_action_t
/*ARGSUSED*/
vi_change_meta(EditLine *el, int c __attribute__((unused)))
{

	/*
         * Delete with insert == change: first we delete and then we leave in
         * insert mode.
         */
	return (cv_action(el, DELETE | INSERT));
}


/* vi_insert_at_bol():
 *	Vi enter insert mode at the beginning of line
 *	[I]
 */
protected el_action_t
/*ARGSUSED*/
vi_insert_at_bol(EditLine *el, int c __attribute__((unused)))
{

	el->el_line.cursor = el->el_line.buffer;
	el->el_chared.c_vcmd.ins = el->el_line.cursor;

	el->el_chared.c_undo.ptr = el->el_line.cursor;
	el->el_chared.c_undo.action = DELETE;

	el->el_map.current = el->el_map.key;
	return (CC_CURSOR);
}


/* vi_replace_char():
 *	Vi replace character under the cursor with the next character typed
 *	[r]
 */
protected el_action_t
/*ARGSUSED*/
vi_replace_char(EditLine *el, int c __attribute__((unused)))
{

	el->el_map.current = el->el_map.key;
	el->el_state.inputmode = MODE_REPLACE_1;
	el->el_chared.c_undo.action = CHANGE;
	el->el_chared.c_undo.ptr = el->el_line.cursor;
	el->el_chared.c_undo.isize = 0;
	el->el_chared.c_undo.dsize = 0;
	return (CC_NORM);
}


/* vi_replace_mode():
 *	Vi enter replace mode
 *	[R]
 */
protected el_action_t
/*ARGSUSED*/
vi_replace_mode(EditLine *el, int c __attribute__((unused)))
{

	el->el_map.current = el->el_map.key;
	el->el_state.inputmode = MODE_REPLACE;
	el->el_chared.c_undo.action = CHANGE;
	el->el_chared.c_undo.ptr = el->el_line.cursor;
	el->el_chared.c_undo.isize = 0;
	el->el_chared.c_undo.dsize = 0;
	return (CC_NORM);
}


/* vi_substitute_char():
 *	Vi replace character under the cursor and enter insert mode
 *	[r]
 */
protected el_action_t
/*ARGSUSED*/
vi_substitute_char(EditLine *el, int c __attribute__((unused)))
{

	c_delafter(el, el->el_state.argument);
	el->el_map.current = el->el_map.key;
	return (CC_REFRESH);
}


/* vi_substitute_line():
 *	Vi substitute entire line
 *	[S]
 */
protected el_action_t
/*ARGSUSED*/
vi_substitute_line(EditLine *el, int c __attribute__((unused)))
{

	(void) em_kill_line(el, 0);
	el->el_map.current = el->el_map.key;
	return (CC_REFRESH);
}


/* vi_change_to_eol():
 *	Vi change to end of line
 *	[C]
 */
protected el_action_t
/*ARGSUSED*/
vi_change_to_eol(EditLine *el, int c __attribute__((unused)))
{

	(void) ed_kill_line(el, 0);
	el->el_map.current = el->el_map.key;
	return (CC_REFRESH);
}


/* vi_insert():
 *	Vi enter insert mode
 *	[i]
 */
protected el_action_t
/*ARGSUSED*/
vi_insert(EditLine *el, int c __attribute__((unused)))
{

	el->el_map.current = el->el_map.key;

	el->el_chared.c_vcmd.ins = el->el_line.cursor;
	el->el_chared.c_undo.ptr = el->el_line.cursor;
	el->el_chared.c_undo.action = DELETE;

	return (CC_NORM);
}


/* vi_add():
 *	Vi enter insert mode after the cursor
 *	[a]
 */
protected el_action_t
/*ARGSUSED*/
vi_add(EditLine *el, int c __attribute__((unused)))
{
	int ret;

	el->el_map.current = el->el_map.key;
	if (el->el_line.cursor < el->el_line.lastchar) {
		el->el_line.cursor++;
		if (el->el_line.cursor > el->el_line.lastchar)
			el->el_line.cursor = el->el_line.lastchar;
		ret = CC_CURSOR;
	} else
		ret = CC_NORM;

	el->el_chared.c_vcmd.ins = el->el_line.cursor;
	el->el_chared.c_undo.ptr = el->el_line.cursor;
	el->el_chared.c_undo.action = DELETE;

	return (ret);
}


/* vi_add_at_eol():
 *	Vi enter insert mode at end of line
 *	[A]
 */
protected el_action_t
/*ARGSUSED*/
vi_add_at_eol(EditLine *el, int c __attribute__((unused)))
{

	el->el_map.current = el->el_map.key;
	el->el_line.cursor = el->el_line.lastchar;

	/* Mark where insertion begins */
	el->el_chared.c_vcmd.ins = el->el_line.lastchar;
	el->el_chared.c_undo.ptr = el->el_line.lastchar;
	el->el_chared.c_undo.action = DELETE;
	return (CC_CURSOR);
}


/* vi_delete_meta():
 *	Vi delete prefix command
 *	[d]
 */
protected el_action_t
/*ARGSUSED*/
vi_delete_meta(EditLine *el, int c __attribute__((unused)))
{

	return (cv_action(el, DELETE));
}


/* vi_end_word():
 *	Vi move to the end of the current space delimited word
 *	[E]
 */
protected el_action_t
/*ARGSUSED*/
vi_end_word(EditLine *el, int c __attribute__((unused)))
{

	if (el->el_line.cursor == el->el_line.lastchar)
		return (CC_ERROR);

	el->el_line.cursor = cv__endword(el->el_line.cursor,
	    el->el_line.lastchar, el->el_state.argument);

	if (el->el_chared.c_vcmd.action & DELETE) {
		el->el_line.cursor++;
		cv_delfini(el);
		return (CC_REFRESH);
	}
	return (CC_CURSOR);
}


/* vi_to_end_word():
 *	Vi move to the end of the current word
 *	[e]
 */
protected el_action_t
/*ARGSUSED*/
vi_to_end_word(EditLine *el, int c __attribute__((unused)))
{

	if (el->el_line.cursor == el->el_line.lastchar)
		return (CC_ERROR);

	el->el_line.cursor = cv__endword(el->el_line.cursor,
	    el->el_line.lastchar, el->el_state.argument);

	if (el->el_chared.c_vcmd.action & DELETE) {
		el->el_line.cursor++;
		cv_delfini(el);
		return (CC_REFRESH);
	}
	return (CC_CURSOR);
}


/* vi_undo():
 *	Vi undo last change
 *	[u]
 */
protected el_action_t
/*ARGSUSED*/
vi_undo(EditLine *el, int c __attribute__((unused)))
{
	char *cp, *kp;
	char temp;
	int i, size;
	c_undo_t *un = &el->el_chared.c_undo;

#ifdef DEBUG_UNDO
	(void) fprintf(el->el_errfile, "Undo: %x \"%s\" +%d -%d\n",
	    un->action, un->buf, un->isize, un->dsize);
#endif
	switch (un->action) {
	case DELETE:
		if (un->dsize == 0)
			return (CC_NORM);

		(void) memcpy(un->buf, un->ptr, un->dsize);
		for (cp = un->ptr; cp <= el->el_line.lastchar; cp++)
			*cp = cp[un->dsize];

		el->el_line.lastchar -= un->dsize;
		el->el_line.cursor = un->ptr;

		un->action = INSERT;
		un->isize = un->dsize;
		un->dsize = 0;
		break;

	case DELETE | INSERT:
		size = un->isize - un->dsize;
		if (size > 0)
			i = un->dsize;
		else
			i = un->isize;
		cp = un->ptr;
		kp = un->buf;
		while (i-- > 0) {
			temp = *kp;
			*kp++ = *cp;
			*cp++ = temp;
		}
		if (size > 0) {
			el->el_line.cursor = cp;
			c_insert(el, size);
			while (size-- > 0 && cp < el->el_line.lastchar) {
				temp = *kp;
				*kp++ = *cp;
				*cp++ = temp;
			}
		} else if (size < 0) {
			size = -size;
			for (; cp <= el->el_line.lastchar; cp++) {
				*kp++ = *cp;
				*cp = cp[size];
			}
			el->el_line.lastchar -= size;
		}
		el->el_line.cursor = un->ptr;
		i = un->dsize;
		un->dsize = un->isize;
		un->isize = i;
		break;

	case INSERT:
		if (un->isize == 0)
			return (CC_NORM);

		el->el_line.cursor = un->ptr;
		c_insert(el, (int) un->isize);
		(void) memcpy(un->ptr, un->buf, un->isize);
		un->action = DELETE;
		un->dsize = un->isize;
		un->isize = 0;
		break;

	case CHANGE:
		if (un->isize == 0)
			return (CC_NORM);

		el->el_line.cursor = un->ptr;
		size = (int) (el->el_line.cursor - el->el_line.lastchar);
		if (size < (int)un->isize)
			size = un->isize;
		cp = un->ptr;
		kp = un->buf;
		for (i = 0; i < size; i++) {
			temp = *kp;
			*kp++ = *cp;
			*cp++ = temp;
		}
		un->dsize = 0;
		break;

	default:
		return (CC_ERROR);
	}

	return (CC_REFRESH);
}


/* vi_command_mode():
 *	Vi enter command mode (use alternative key bindings)
 *	[<ESC>]
 */
protected el_action_t
/*ARGSUSED*/
vi_command_mode(EditLine *el, int c __attribute__((unused)))
{
	int size;

	/* [Esc] cancels pending action */
	el->el_chared.c_vcmd.ins = 0;
	el->el_chared.c_vcmd.action = NOP;
	el->el_chared.c_vcmd.pos = 0;

	el->el_state.doingarg = 0;
	size = el->el_chared.c_undo.ptr - el->el_line.cursor;
	if (size < 0)
		size = -size;
	if (el->el_chared.c_undo.action == (INSERT | DELETE) ||
	    el->el_chared.c_undo.action == DELETE)
		el->el_chared.c_undo.dsize = size;
	else
		el->el_chared.c_undo.isize = size;

	el->el_state.inputmode = MODE_INSERT;
	el->el_map.current = el->el_map.alt;
#ifdef VI_MOVE
	if (el->el_line.cursor > el->el_line.buffer)
		el->el_line.cursor--;
#endif
	return (CC_CURSOR);
}


/* vi_zero():
 *	Vi move to the beginning of line
 *	[0]
 */
protected el_action_t
vi_zero(EditLine *el, int c)
{

	if (el->el_state.doingarg) {
		if (el->el_state.argument > 1000000)
			return (CC_ERROR);
		el->el_state.argument =
		    (el->el_state.argument * 10) + (c - '0');
		return (CC_ARGHACK);
	} else {
		el->el_line.cursor = el->el_line.buffer;
		if (el->el_chared.c_vcmd.action & DELETE) {
			cv_delfini(el);
			return (CC_REFRESH);
		}
		return (CC_CURSOR);
	}
}


/* vi_delete_prev_char():
 * 	Vi move to previous character (backspace)
 *	[^H]
 */
protected el_action_t
/*ARGSUSED*/
vi_delete_prev_char(EditLine *el, int c __attribute__((unused)))
{

	if (el->el_chared.c_vcmd.ins == 0)
		return (CC_ERROR);

	if (el->el_chared.c_vcmd.ins >
	    el->el_line.cursor - el->el_state.argument)
		return (CC_ERROR);

	c_delbefore(el, el->el_state.argument);
	el->el_line.cursor -= el->el_state.argument;

	return (CC_REFRESH);
}


/* vi_list_or_eof():
 *	Vi list choices for completion or indicate end of file if empty line
 *	[^D]
 */
protected el_action_t
/*ARGSUSED*/
vi_list_or_eof(EditLine *el, int c __attribute__((unused)))
{

#ifdef notyet
	if (el->el_line.cursor == el->el_line.lastchar &&
	    el->el_line.cursor == el->el_line.buffer) {
#endif
		term_overwrite(el, STReof, 4);	/* then do a EOF */
		term__flush();
		return (CC_EOF);
#ifdef notyet
	} else {
		re_goto_bottom(el);
		*el->el_line.lastchar = '\0';	/* just in case */
		return (CC_LIST_CHOICES);
	}
#endif
}


/* vi_kill_line_prev():
 *	Vi cut from beginning of line to cursor
 *	[^U]
 */
protected el_action_t
/*ARGSUSED*/
vi_kill_line_prev(EditLine *el, int c __attribute__((unused)))
{
	char *kp, *cp;

	cp = el->el_line.buffer;
	kp = el->el_chared.c_kill.buf;
	while (cp < el->el_line.cursor)
		*kp++ = *cp++;	/* copy it */
	el->el_chared.c_kill.last = kp;
	c_delbefore(el, el->el_line.cursor - el->el_line.buffer);
	el->el_line.cursor = el->el_line.buffer;	/* zap! */
	return (CC_REFRESH);
}


/* vi_search_prev():
 *	Vi search history previous
 *	[?]
 */
protected el_action_t
/*ARGSUSED*/
vi_search_prev(EditLine *el, int c __attribute__((unused)))
{

	return (cv_search(el, ED_SEARCH_PREV_HISTORY));
}


/* vi_search_next():
 *	Vi search history next
 *	[/]
 */
protected el_action_t
/*ARGSUSED*/
vi_search_next(EditLine *el, int c __attribute__((unused)))
{

	return (cv_search(el, ED_SEARCH_NEXT_HISTORY));
}


/* vi_repeat_search_next():
 *	Vi repeat current search in the same search direction
 *	[n]
 */
protected el_action_t
/*ARGSUSED*/
vi_repeat_search_next(EditLine *el, int c __attribute__((unused)))
{

	if (el->el_search.patlen == 0)
		return (CC_ERROR);
	else
		return (cv_repeat_srch(el, el->el_search.patdir));
}


/* vi_repeat_search_prev():
 *	Vi repeat current search in the opposite search direction
 *	[N]
 */
/*ARGSUSED*/
protected el_action_t
vi_repeat_search_prev(EditLine *el, int c __attribute__((unused)))
{

	if (el->el_search.patlen == 0)
		return (CC_ERROR);
	else
		return (cv_repeat_srch(el,
		    el->el_search.patdir == ED_SEARCH_PREV_HISTORY ?
		    ED_SEARCH_NEXT_HISTORY : ED_SEARCH_PREV_HISTORY));
}


/* vi_next_char():
 *	Vi move to the character specified next
 *	[f]
 */
protected el_action_t
/*ARGSUSED*/
vi_next_char(EditLine *el, int c __attribute__((unused)))
{
	char ch;

	if (el_getc(el, &ch) != 1)
		return (ed_end_of_file(el, 0));

	el->el_search.chadir = CHAR_FWD;
	el->el_search.chacha = ch;

	return (cv_csearch_fwd(el, ch, el->el_state.argument, 0));

}


/* vi_prev_char():
 *	Vi move to the character specified previous
 *	[F]
 */
protected el_action_t
/*ARGSUSED*/
vi_prev_char(EditLine *el, int c __attribute__((unused)))
{
	char ch;

	if (el_getc(el, &ch) != 1)
		return (ed_end_of_file(el, 0));

	el->el_search.chadir = CHAR_BACK;
	el->el_search.chacha = ch;

	return (cv_csearch_back(el, ch, el->el_state.argument, 0));
}


/* vi_to_next_char():
 *	Vi move up to the character specified next
 *	[t]
 */
protected el_action_t
/*ARGSUSED*/
vi_to_next_char(EditLine *el, int c __attribute__((unused)))
{
	char ch;

	if (el_getc(el, &ch) != 1)
		return (ed_end_of_file(el, 0));

	return (cv_csearch_fwd(el, ch, el->el_state.argument, 1));

}


/* vi_to_prev_char():
 *	Vi move up to the character specified previous
 *	[T]
 */
protected el_action_t
/*ARGSUSED*/
vi_to_prev_char(EditLine *el, int c __attribute__((unused)))
{
	char ch;

	if (el_getc(el, &ch) != 1)
		return (ed_end_of_file(el, 0));

	return (cv_csearch_back(el, ch, el->el_state.argument, 1));
}


/* vi_repeat_next_char():
 *	Vi repeat current character search in the same search direction
 *	[;]
 */
protected el_action_t
/*ARGSUSED*/
vi_repeat_next_char(EditLine *el, int c __attribute__((unused)))
{

	if (el->el_search.chacha == 0)
		return (CC_ERROR);

	return (el->el_search.chadir == CHAR_FWD
	    ? cv_csearch_fwd(el, el->el_search.chacha,
		el->el_state.argument, 0)
	    : cv_csearch_back(el, el->el_search.chacha,
		el->el_state.argument, 0));
}


/* vi_repeat_prev_char():
 *	Vi repeat current character search in the opposite search direction
 *	[,]
 */
protected el_action_t
/*ARGSUSED*/
vi_repeat_prev_char(EditLine *el, int c __attribute__((unused)))
{

	if (el->el_search.chacha == 0)
		return (CC_ERROR);

	return el->el_search.chadir == CHAR_BACK ?
	    cv_csearch_fwd(el, el->el_search.chacha, el->el_state.argument, 0) :
	    cv_csearch_back(el, el->el_search.chacha, el->el_state.argument, 0);
}
