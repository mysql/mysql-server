/*	$NetBSD: search.c,v 1.14 2002/11/20 16:50:08 christos Exp $	*/

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
static char sccsid[] = "@(#)search.c	8.1 (Berkeley) 6/4/93";
#else
__RCSID("$NetBSD: search.c,v 1.14 2002/11/20 16:50:08 christos Exp $");
#endif
#endif /* not lint && not SCCSID */

/*
 * search.c: History and character search functions
 */
#include <stdlib.h>
#if defined(REGEX)
#include <sys/types.h>
#include <regex.h>
#elif defined(REGEXP)
#include <regexp.h>
#endif
#include "el.h"

/*
 * Adjust cursor in vi mode to include the character under it
 */
#define	EL_CURSOR(el) \
    ((el)->el_line.cursor + (((el)->el_map.type == MAP_VI) && \
			    ((el)->el_map.current == (el)->el_map.alt)))

/* search_init():
 *	Initialize the search stuff
 */
protected int
search_init(EditLine *el)
{

	el->el_search.patbuf = (char *) el_malloc(EL_BUFSIZ);
	if (el->el_search.patbuf == NULL)
		return (-1);
	el->el_search.patlen = 0;
	el->el_search.patdir = -1;
	el->el_search.chacha = '\0';
	el->el_search.chadir = CHAR_FWD;
	el->el_search.chatflg = 0;
	return (0);
}


/* search_end():
 *	Initialize the search stuff
 */
protected void
search_end(EditLine *el)
{

	el_free((ptr_t) el->el_search.patbuf);
	el->el_search.patbuf = NULL;
}


#ifdef REGEXP
/* regerror():
 *	Handle regular expression errors
 */
public void
/*ARGSUSED*/
regerror(const char *msg)
{
}
#endif


/* el_match():
 *	Return if string matches pattern
 */
protected int
el_match(const char *str, const char *pat)
{
#if defined (REGEX)
	regex_t re;
	int rv;
#elif defined (REGEXP)
	regexp *rp;
	int rv;
#else
	extern char	*re_comp(const char *);
	extern int	 re_exec(const char *);
#endif

	if (strstr(str, pat) != NULL)
		return (1);

#if defined(REGEX)
	if (regcomp(&re, pat, 0) == 0) {
		rv = regexec(&re, str, 0, NULL, 0) == 0;
		regfree(&re);
	} else {
		rv = 0;
	}
	return (rv);
#elif defined(REGEXP)
	if ((re = regcomp(pat)) != NULL) {
		rv = regexec(re, str);
		free((ptr_t) re);
	} else {
		rv = 0;
	}
	return (rv);
#else
	if (re_comp(pat) != NULL)
		return (0);
	else
		return (re_exec(str) == 1);
#endif
}


/* c_hmatch():
 *	 return True if the pattern matches the prefix
 */
protected int
c_hmatch(EditLine *el, const char *str)
{
#ifdef SDEBUG
	(void) fprintf(el->el_errfile, "match `%s' with `%s'\n",
	    el->el_search.patbuf, str);
#endif /* SDEBUG */

	return (el_match(str, el->el_search.patbuf));
}


/* c_setpat():
 *	Set the history seatch pattern
 */
protected void
c_setpat(EditLine *el)
{
	if (el->el_state.lastcmd != ED_SEARCH_PREV_HISTORY &&
	    el->el_state.lastcmd != ED_SEARCH_NEXT_HISTORY) {
		el->el_search.patlen = EL_CURSOR(el) - el->el_line.buffer;
		if (el->el_search.patlen >= EL_BUFSIZ)
			el->el_search.patlen = EL_BUFSIZ - 1;
		if (el->el_search.patlen != 0) {
			(void) strncpy(el->el_search.patbuf, el->el_line.buffer,
			    el->el_search.patlen);
			el->el_search.patbuf[el->el_search.patlen] = '\0';
		} else
			el->el_search.patlen = strlen(el->el_search.patbuf);
	}
#ifdef SDEBUG
	(void) fprintf(el->el_errfile, "\neventno = %d\n",
	    el->el_history.eventno);
	(void) fprintf(el->el_errfile, "patlen = %d\n", el->el_search.patlen);
	(void) fprintf(el->el_errfile, "patbuf = \"%s\"\n",
	    el->el_search.patbuf);
	(void) fprintf(el->el_errfile, "cursor %d lastchar %d\n",
	    EL_CURSOR(el) - el->el_line.buffer,
	    el->el_line.lastchar - el->el_line.buffer);
#endif
}


/* ce_inc_search():
 *	Emacs incremental search
 */
protected el_action_t
ce_inc_search(EditLine *el, int dir)
{
	static const char STRfwd[] = {'f', 'w', 'd', '\0'},
	     STRbck[] = {'b', 'c', 'k', '\0'};
	static char pchar = ':';/* ':' = normal, '?' = failed */
	static char endcmd[2] = {'\0', '\0'};
	char ch, *ocursor = el->el_line.cursor, oldpchar = pchar;
	const char *cp;

	el_action_t ret = CC_NORM;

	int ohisteventno = el->el_history.eventno;
	int oldpatlen = el->el_search.patlen;
	int newdir = dir;
	int done, redo;

	if (el->el_line.lastchar + sizeof(STRfwd) / sizeof(char) + 2 +
	    el->el_search.patlen >= el->el_line.limit)
		return (CC_ERROR);

	for (;;) {

		if (el->el_search.patlen == 0) {	/* first round */
			pchar = ':';
#ifdef ANCHOR
			el->el_search.patbuf[el->el_search.patlen++] = '.';
			el->el_search.patbuf[el->el_search.patlen++] = '*';
#endif
		}
		done = redo = 0;
		*el->el_line.lastchar++ = '\n';
		for (cp = (newdir == ED_SEARCH_PREV_HISTORY) ? STRbck : STRfwd;
		    *cp; *el->el_line.lastchar++ = *cp++)
			continue;
		*el->el_line.lastchar++ = pchar;
		for (cp = &el->el_search.patbuf[1];
		    cp < &el->el_search.patbuf[el->el_search.patlen];
		    *el->el_line.lastchar++ = *cp++)
			continue;
		*el->el_line.lastchar = '\0';
		re_refresh(el);

		if (el_getc(el, &ch) != 1)
			return (ed_end_of_file(el, 0));

		switch (el->el_map.current[(unsigned char) ch]) {
		case ED_INSERT:
		case ED_DIGIT:
			if (el->el_search.patlen > EL_BUFSIZ - 3)
				term_beep(el);
			else {
				el->el_search.patbuf[el->el_search.patlen++] =
				    ch;
				*el->el_line.lastchar++ = ch;
				*el->el_line.lastchar = '\0';
				re_refresh(el);
			}
			break;

		case EM_INC_SEARCH_NEXT:
			newdir = ED_SEARCH_NEXT_HISTORY;
			redo++;
			break;

		case EM_INC_SEARCH_PREV:
			newdir = ED_SEARCH_PREV_HISTORY;
			redo++;
			break;

		case ED_DELETE_PREV_CHAR:
			if (el->el_search.patlen > 1)
				done++;
			else
				term_beep(el);
			break;

		default:
			switch (ch) {
			case 0007:	/* ^G: Abort */
				ret = CC_ERROR;
				done++;
				break;

			case 0027:	/* ^W: Append word */
			/* No can do if globbing characters in pattern */
				for (cp = &el->el_search.patbuf[1];; cp++)
				    if (cp >= &el->el_search.patbuf[el->el_search.patlen]) {
					el->el_line.cursor +=
					    el->el_search.patlen - 1;
					cp = c__next_word(el->el_line.cursor,
					    el->el_line.lastchar, 1,
					    ce__isword);
					while (el->el_line.cursor < cp &&
					    *el->el_line.cursor != '\n') {
						if (el->el_search.patlen >
						    EL_BUFSIZ - 3) {
							term_beep(el);
							break;
						}
						el->el_search.patbuf[el->el_search.patlen++] =
						    *el->el_line.cursor;
						*el->el_line.lastchar++ =
						    *el->el_line.cursor++;
					}
					el->el_line.cursor = ocursor;
					*el->el_line.lastchar = '\0';
					re_refresh(el);
					break;
				    } else if (isglob(*cp)) {
					    term_beep(el);
					    break;
				    }
				break;

			default:	/* Terminate and execute cmd */
				endcmd[0] = ch;
				el_push(el, endcmd);
				/* FALLTHROUGH */

			case 0033:	/* ESC: Terminate */
				ret = CC_REFRESH;
				done++;
				break;
			}
			break;
		}

		while (el->el_line.lastchar > el->el_line.buffer &&
		    *el->el_line.lastchar != '\n')
			*el->el_line.lastchar-- = '\0';
		*el->el_line.lastchar = '\0';

		if (!done) {

			/* Can't search if unmatched '[' */
			for (cp = &el->el_search.patbuf[el->el_search.patlen-1],
			    ch = ']';
			    cp > el->el_search.patbuf;
			    cp--)
				if (*cp == '[' || *cp == ']') {
					ch = *cp;
					break;
				}
			if (el->el_search.patlen > 1 && ch != '[') {
				if (redo && newdir == dir) {
					if (pchar == '?') { /* wrap around */
						el->el_history.eventno =
						    newdir == ED_SEARCH_PREV_HISTORY ? 0 : 0x7fffffff;
						if (hist_get(el) == CC_ERROR)
							/* el->el_history.event
							 * no was fixed by
							 * first call */
							(void) hist_get(el);
						el->el_line.cursor = newdir ==
						    ED_SEARCH_PREV_HISTORY ?
						    el->el_line.lastchar :
						    el->el_line.buffer;
					} else
						el->el_line.cursor +=
						    newdir ==
						    ED_SEARCH_PREV_HISTORY ?
						    -1 : 1;
				}
#ifdef ANCHOR
				el->el_search.patbuf[el->el_search.patlen++] =
				    '.';
				el->el_search.patbuf[el->el_search.patlen++] =
				    '*';
#endif
				el->el_search.patbuf[el->el_search.patlen] =
				    '\0';
				if (el->el_line.cursor < el->el_line.buffer ||
				    el->el_line.cursor > el->el_line.lastchar ||
				    (ret = ce_search_line(el,
				    &el->el_search.patbuf[1],
				    newdir)) == CC_ERROR) {
					/* avoid c_setpat */
					el->el_state.lastcmd =
					    (el_action_t) newdir;
					ret = newdir == ED_SEARCH_PREV_HISTORY ?
					    ed_search_prev_history(el, 0) :
					    ed_search_next_history(el, 0);
					if (ret != CC_ERROR) {
						el->el_line.cursor = newdir ==
						    ED_SEARCH_PREV_HISTORY ?
						    el->el_line.lastchar :
						    el->el_line.buffer;
						(void) ce_search_line(el,
						    &el->el_search.patbuf[1],
						    newdir);
					}
				}
				el->el_search.patbuf[--el->el_search.patlen] =
				    '\0';
				if (ret == CC_ERROR) {
					term_beep(el);
					if (el->el_history.eventno !=
					    ohisteventno) {
						el->el_history.eventno =
						    ohisteventno;
						if (hist_get(el) == CC_ERROR)
							return (CC_ERROR);
					}
					el->el_line.cursor = ocursor;
					pchar = '?';
				} else {
					pchar = ':';
				}
			}
			ret = ce_inc_search(el, newdir);

			if (ret == CC_ERROR && pchar == '?' && oldpchar == ':')
				/*
				 * break abort of failed search at last
				 * non-failed
				 */
				ret = CC_NORM;

		}
		if (ret == CC_NORM || (ret == CC_ERROR && oldpatlen == 0)) {
			/* restore on normal return or error exit */
			pchar = oldpchar;
			el->el_search.patlen = oldpatlen;
			if (el->el_history.eventno != ohisteventno) {
				el->el_history.eventno = ohisteventno;
				if (hist_get(el) == CC_ERROR)
					return (CC_ERROR);
			}
			el->el_line.cursor = ocursor;
			if (ret == CC_ERROR)
				re_refresh(el);
		}
		if (done || ret != CC_NORM)
			return (ret);
	}
}


/* cv_search():
 *	Vi search.
 */
protected el_action_t
cv_search(EditLine *el, int dir)
{
	char ch;
	char tmpbuf[EL_BUFSIZ];
	int tmplen;

#ifdef ANCHOR
	tmpbuf[0] = '.';
	tmpbuf[1] = '*';
#define	LEN	2
#else
#define	LEN	0
#endif
	tmplen = LEN;

	el->el_search.patdir = dir;

	tmplen = c_gets(el, &tmpbuf[LEN],
		dir == ED_SEARCH_PREV_HISTORY ? "\n/" : "\n?" );
	if (tmplen == -1)
		return CC_REFRESH;

	tmplen += LEN;
	ch = tmpbuf[tmplen];
	tmpbuf[tmplen] = '\0';

	if (tmplen == LEN) {
		/*
		 * Use the old pattern, but wild-card it.
		 */
		if (el->el_search.patlen == 0) {
			re_refresh(el);
			return (CC_ERROR);
		}
#ifdef ANCHOR
		if (el->el_search.patbuf[0] != '.' &&
		    el->el_search.patbuf[0] != '*') {
			(void) strncpy(tmpbuf, el->el_search.patbuf,
			    sizeof(tmpbuf) - 1);
			el->el_search.patbuf[0] = '.';
			el->el_search.patbuf[1] = '*';
			(void) strncpy(&el->el_search.patbuf[2], tmpbuf,
			    EL_BUFSIZ - 3);
			el->el_search.patlen++;
			el->el_search.patbuf[el->el_search.patlen++] = '.';
			el->el_search.patbuf[el->el_search.patlen++] = '*';
			el->el_search.patbuf[el->el_search.patlen] = '\0';
		}
#endif
	} else {
#ifdef ANCHOR
		tmpbuf[tmplen++] = '.';
		tmpbuf[tmplen++] = '*';
#endif
		tmpbuf[tmplen] = '\0';
		(void) strncpy(el->el_search.patbuf, tmpbuf, EL_BUFSIZ - 1);
		el->el_search.patlen = tmplen;
	}
	el->el_state.lastcmd = (el_action_t) dir;	/* avoid c_setpat */
	el->el_line.cursor = el->el_line.lastchar = el->el_line.buffer;
	if ((dir == ED_SEARCH_PREV_HISTORY ? ed_search_prev_history(el, 0) :
	    ed_search_next_history(el, 0)) == CC_ERROR) {
		re_refresh(el);
		return (CC_ERROR);
	}
	if (ch == 0033) {
		re_refresh(el);
		return ed_newline(el, 0);
	}
	return (CC_REFRESH);
}


/* ce_search_line():
 *	Look for a pattern inside a line
 */
protected el_action_t
ce_search_line(EditLine *el, char *pattern, int dir)
{
	char *cp;

	if (dir == ED_SEARCH_PREV_HISTORY) {
		for (cp = el->el_line.cursor; cp >= el->el_line.buffer; cp--)
			if (el_match(cp, pattern)) {
				el->el_line.cursor = cp;
				return (CC_NORM);
			}
		return (CC_ERROR);
	} else {
		for (cp = el->el_line.cursor; *cp != '\0' &&
		    cp < el->el_line.limit; cp++)
			if (el_match(cp, pattern)) {
				el->el_line.cursor = cp;
				return (CC_NORM);
			}
		return (CC_ERROR);
	}
}


/* cv_repeat_srch():
 *	Vi repeat search
 */
protected el_action_t
cv_repeat_srch(EditLine *el, int c)
{

#ifdef SDEBUG
	(void) fprintf(el->el_errfile, "dir %d patlen %d patbuf %s\n",
	    c, el->el_search.patlen, el->el_search.patbuf);
#endif

	el->el_state.lastcmd = (el_action_t) c;	/* Hack to stop c_setpat */
	el->el_line.lastchar = el->el_line.buffer;

	switch (c) {
	case ED_SEARCH_NEXT_HISTORY:
		return (ed_search_next_history(el, 0));
	case ED_SEARCH_PREV_HISTORY:
		return (ed_search_prev_history(el, 0));
	default:
		return (CC_ERROR);
	}
}


/* cv_csearch():
 *	Vi character search
 */
protected el_action_t
cv_csearch(EditLine *el, int direction, int ch, int count, int tflag)
{
	char *cp;

	if (ch == 0)
		return CC_ERROR;

	if (ch == -1) {
		char c;
		if (el_getc(el, &c) != 1)
			return ed_end_of_file(el, 0);
		ch = c;
	}

	/* Save for ';' and ',' commands */
	el->el_search.chacha = ch;
	el->el_search.chadir = direction;
	el->el_search.chatflg = tflag;

	cp = el->el_line.cursor;
	while (count--) {
		if (*cp == ch)
			cp += direction;
		for (;;cp += direction) {
			if (cp >= el->el_line.lastchar)
				return CC_ERROR;
			if (cp < el->el_line.buffer)
				return CC_ERROR;
			if (*cp == ch)
				break;
		}
	}

	if (tflag)
		cp -= direction;

	el->el_line.cursor = cp;

	if (el->el_chared.c_vcmd.action != NOP) {
		if (direction > 0)
			el->el_line.cursor++;
		cv_delfini(el);
		return CC_REFRESH;
	}
	return CC_CURSOR;
}
