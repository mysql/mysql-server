/*	$NetBSD: history.c,v 1.22 2003/01/21 18:40:24 christos Exp $	*/

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
static char sccsid[] = "@(#)history.c	8.1 (Berkeley) 6/4/93";
#else
__RCSID("$NetBSD: history.c,v 1.22 2003/01/21 18:40:24 christos Exp $");
#endif
#endif /* not lint && not SCCSID */

/*
 * hist.c: History access functions
 */
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#ifdef HAVE_VIS_H
#include <vis.h>
#else
#include "np/vis.h"
#endif
#include <sys/stat.h>

static const char hist_cookie[] = "_HiStOrY_V2_\n";

#include "histedit.h"

typedef int (*history_gfun_t)(ptr_t, HistEvent *);
typedef int (*history_efun_t)(ptr_t, HistEvent *, const char *);
typedef void (*history_vfun_t)(ptr_t, HistEvent *);
typedef int (*history_sfun_t)(ptr_t, HistEvent *, const int);

struct history {
	ptr_t h_ref;		/* Argument for history fcns	 */
	int h_ent;		/* Last entry point for history	 */
	history_gfun_t h_first;	/* Get the first element	 */
	history_gfun_t h_next;	/* Get the next element		 */
	history_gfun_t h_last;	/* Get the last element		 */
	history_gfun_t h_prev;	/* Get the previous element	 */
	history_gfun_t h_curr;	/* Get the current element	 */
	history_sfun_t h_set;	/* Set the current element	 */
	history_vfun_t h_clear;	/* Clear the history list	 */
	history_efun_t h_enter;	/* Add an element		 */
	history_efun_t h_add;	/* Append to an element		 */
};

#define	HNEXT(h, ev)		(*(h)->h_next)((h)->h_ref, ev)
#define	HFIRST(h, ev)		(*(h)->h_first)((h)->h_ref, ev)
#define	HPREV(h, ev)		(*(h)->h_prev)((h)->h_ref, ev)
#define	HLAST(h, ev)		(*(h)->h_last)((h)->h_ref, ev)
#define	HCURR(h, ev)		(*(h)->h_curr)((h)->h_ref, ev)
#define	HSET(h, ev, n)		(*(h)->h_set)((h)->h_ref, ev, n)
#define	HCLEAR(h, ev)		(*(h)->h_clear)((h)->h_ref, ev)
#define	HENTER(h, ev, str)	(*(h)->h_enter)((h)->h_ref, ev, str)
#define	HADD(h, ev, str)	(*(h)->h_add)((h)->h_ref, ev, str)

#define	h_malloc(a)	malloc(a)
#define	h_realloc(a, b)	realloc((a), (b))
#define	h_free(a)	free(a)

typedef struct {
    int		num;
    char	*str;
} HistEventPrivate;



private int history_setsize(History *, HistEvent *, int);
private int history_getsize(History *, HistEvent *);
private int history_setunique(History *, HistEvent *, int);
private int history_getunique(History *, HistEvent *);
private int history_set_fun(History *, History *);
private int history_load(History *, const char *);
private int history_save(History *, const char *);
private int history_prev_event(History *, HistEvent *, int);
private int history_next_event(History *, HistEvent *, int);
private int history_next_string(History *, HistEvent *, const char *);
private int history_prev_string(History *, HistEvent *, const char *);


/***********************************************************************/

/*
 * Builtin- history implementation
 */
typedef struct hentry_t {
	HistEvent ev;		/* What we return		 */
	struct hentry_t *next;	/* Next entry			 */
	struct hentry_t *prev;	/* Previous entry		 */
} hentry_t;

typedef struct history_t {
	hentry_t list;		/* Fake list header element	*/
	hentry_t *cursor;	/* Current element in the list	*/
	int max;		/* Maximum number of events	*/
	int cur;		/* Current number of events	*/
	int eventid;		/* For generation of unique event id	 */
	int flags;		/* History flags		*/
#define H_UNIQUE	1	/* Store only unique elements	*/
} history_t;

private int history_def_first(ptr_t, HistEvent *);
private int history_def_last(ptr_t, HistEvent *);
private int history_def_next(ptr_t, HistEvent *);
private int history_def_prev(ptr_t, HistEvent *);
private int history_def_curr(ptr_t, HistEvent *);
private int history_def_set(ptr_t, HistEvent *, const int n);
private int history_def_enter(ptr_t, HistEvent *, const char *);
private int history_def_add(ptr_t, HistEvent *, const char *);
private int history_def_init(ptr_t *, HistEvent *, int);
private void history_def_clear(ptr_t, HistEvent *);
private int history_def_insert(history_t *, HistEvent *, const char *);
private void history_def_delete(history_t *, HistEvent *, hentry_t *);

#define	history_def_setsize(p, num)(void) (((history_t *)p)->max = (num))
#define	history_def_getsize(p)  (((history_t *)p)->cur)
#define	history_def_getunique(p) (((((history_t *)p)->flags) & H_UNIQUE) != 0)
#define	history_def_setunique(p, uni) \
    if (uni) \
	(((history_t *)p)->flags) |= H_UNIQUE; \
    else \
	(((history_t *)p)->flags) &= ~H_UNIQUE

#define	he_strerror(code)	he_errlist[code]
#define	he_seterrev(evp, code)	{\
				    evp->num = code;\
				    evp->str = he_strerror(code);\
				}

/* error messages */
static const char *const he_errlist[] = {
	"OK",
	"unknown error",
	"malloc() failed",
	"first event not found",
	"last event not found",
	"empty list",
	"no next event",
	"no previous event",
	"current event is invalid",
	"event not found",
	"can't read history from file",
	"can't write history",
	"required parameter(s) not supplied",
	"history size negative",
	"function not allowed with other history-functions-set the default",
	"bad parameters"
};
/* error codes */
#define	_HE_OK                   0
#define	_HE_UNKNOWN		 1
#define	_HE_MALLOC_FAILED        2
#define	_HE_FIRST_NOTFOUND       3
#define	_HE_LAST_NOTFOUND        4
#define	_HE_EMPTY_LIST           5
#define	_HE_END_REACHED          6
#define	_HE_START_REACHED	 7
#define	_HE_CURR_INVALID	 8
#define	_HE_NOT_FOUND		 9
#define	_HE_HIST_READ		10
#define	_HE_HIST_WRITE		11
#define	_HE_PARAM_MISSING	12
#define	_HE_SIZE_NEGATIVE	13
#define	_HE_NOT_ALLOWED		14
#define	_HE_BAD_PARAM		15

/* history_def_first():
 *	Default function to return the first event in the history.
 */
private int
history_def_first(ptr_t p, HistEvent *ev)
{
	history_t *h = (history_t *) p;

	h->cursor = h->list.next;
	if (h->cursor != &h->list)
		*ev = h->cursor->ev;
	else {
		he_seterrev(ev, _HE_FIRST_NOTFOUND);
		return (-1);
	}

	return (0);
}


/* history_def_last():
 *	Default function to return the last event in the history.
 */
private int
history_def_last(ptr_t p, HistEvent *ev)
{
	history_t *h = (history_t *) p;

	h->cursor = h->list.prev;
	if (h->cursor != &h->list)
		*ev = h->cursor->ev;
	else {
		he_seterrev(ev, _HE_LAST_NOTFOUND);
		return (-1);
	}

	return (0);
}


/* history_def_next():
 *	Default function to return the next event in the history.
 */
private int
history_def_next(ptr_t p, HistEvent *ev)
{
	history_t *h = (history_t *) p;

	if (h->cursor != &h->list)
		h->cursor = h->cursor->next;
	else {
		he_seterrev(ev, _HE_EMPTY_LIST);
		return (-1);
	}

	if (h->cursor != &h->list)
		*ev = h->cursor->ev;
	else {
		he_seterrev(ev, _HE_END_REACHED);
		return (-1);
	}

	return (0);
}


/* history_def_prev():
 *	Default function to return the previous event in the history.
 */
private int
history_def_prev(ptr_t p, HistEvent *ev)
{
	history_t *h = (history_t *) p;

	if (h->cursor != &h->list)
		h->cursor = h->cursor->prev;
	else {
		he_seterrev(ev,
		    (h->cur > 0) ? _HE_END_REACHED : _HE_EMPTY_LIST);
		return (-1);
	}

	if (h->cursor != &h->list)
		*ev = h->cursor->ev;
	else {
		he_seterrev(ev, _HE_START_REACHED);
		return (-1);
	}

	return (0);
}


/* history_def_curr():
 *	Default function to return the current event in the history.
 */
private int
history_def_curr(ptr_t p, HistEvent *ev)
{
	history_t *h = (history_t *) p;

	if (h->cursor != &h->list)
		*ev = h->cursor->ev;
	else {
		he_seterrev(ev,
		    (h->cur > 0) ? _HE_CURR_INVALID : _HE_EMPTY_LIST);
		return (-1);
	}

	return (0);
}


/* history_def_set():
 *	Default function to set the current event in the history to the
 *	given one.
 */
private int
history_def_set(ptr_t p, HistEvent *ev, const int n)
{
	history_t *h = (history_t *) p;

	if (h->cur == 0) {
		he_seterrev(ev, _HE_EMPTY_LIST);
		return (-1);
	}
	if (h->cursor == &h->list || h->cursor->ev.num != n) {
		for (h->cursor = h->list.next; h->cursor != &h->list;
		    h->cursor = h->cursor->next)
			if (h->cursor->ev.num == n)
				break;
	}
	if (h->cursor == &h->list) {
		he_seterrev(ev, _HE_NOT_FOUND);
		return (-1);
	}
	return (0);
}


/* history_def_add():
 *	Append string to element
 */
private int
history_def_add(ptr_t p, HistEvent *ev, const char *str)
{
	history_t *h = (history_t *) p;
	size_t len;
	char *s;
	HistEventPrivate *evp = (void *)&h->cursor->ev;

	if (h->cursor == &h->list)
		return (history_def_enter(p, ev, str));
	len = strlen(evp->str) + strlen(str) + 1;
	s = (char *) h_malloc(len);
	if (s == NULL) {
		he_seterrev(ev, _HE_MALLOC_FAILED);
		return (-1);
	}
	(void) strlcpy(s, h->cursor->ev.str, len);
	(void) strlcat(s, str, len);
	h_free((ptr_t)evp->str);
	evp->str = s;
	*ev = h->cursor->ev;
	return (0);
}


/* history_def_delete():
 *	Delete element hp of the h list
 */
/* ARGSUSED */
private void
history_def_delete(history_t *h, HistEvent *ev __attribute__((unused)), hentry_t *hp)
{
	HistEventPrivate *evp = (void *)&hp->ev;
	if (hp == &h->list)
		abort();
	hp->prev->next = hp->next;
	hp->next->prev = hp->prev;
	h_free((ptr_t) evp->str);
	h_free(hp);
	h->cur--;
}


/* history_def_insert():
 *	Insert element with string str in the h list
 */
private int
history_def_insert(history_t *h, HistEvent *ev, const char *str)
{

	h->cursor = (hentry_t *) h_malloc(sizeof(hentry_t));
	if (h->cursor == NULL)
		goto oomem;
	if ((h->cursor->ev.str = strdup(str)) == NULL) {
		h_free((ptr_t)h->cursor);
		goto oomem;
	}
	h->cursor->ev.num = ++h->eventid;
	h->cursor->next = h->list.next;
	h->cursor->prev = &h->list;
	h->list.next->prev = h->cursor;
	h->list.next = h->cursor;
	h->cur++;

	*ev = h->cursor->ev;
	return (0);
oomem:
	he_seterrev(ev, _HE_MALLOC_FAILED);
	return (-1);
}


/* history_def_enter():
 *	Default function to enter an item in the history
 */
private int
history_def_enter(ptr_t p, HistEvent *ev, const char *str)
{
	history_t *h = (history_t *) p;

	if ((h->flags & H_UNIQUE) != 0 && h->list.next != &h->list &&
	    strcmp(h->list.next->ev.str, str) == 0)
	    return (0); 

	if (history_def_insert(h, ev, str) == -1)
		return (-1);	/* error, keep error message */

	/*
         * Always keep at least one entry.
         * This way we don't have to check for the empty list.
         */
	while (h->cur > h->max && h->cur > 0)
		history_def_delete(h, ev, h->list.prev);

	return (1);
}


/* history_def_init():
 *	Default history initialization function
 */
/* ARGSUSED */
private int
history_def_init(ptr_t *p, HistEvent *ev __attribute__((unused)), int n)
{
	history_t *h = (history_t *) h_malloc(sizeof(history_t));
	if (h == NULL)
		return -1;

	if (n <= 0)
		n = 0;
	h->eventid = 0;
	h->cur = 0;
	h->max = n;
	h->list.next = h->list.prev = &h->list;
	h->list.ev.str = NULL;
	h->list.ev.num = 0;
	h->cursor = &h->list;
	h->flags = 0;
	*p = (ptr_t) h;
	return 0;
}


/* history_def_clear():
 *	Default history cleanup function
 */
private void
history_def_clear(ptr_t p, HistEvent *ev)
{
	history_t *h = (history_t *) p;

	while (h->list.prev != &h->list)
		history_def_delete(h, ev, h->list.prev);
	h->eventid = 0;
	h->cur = 0;
}




/************************************************************************/

/* history_init():
 *	Initialization function.
 */
public History *
history_init(void)
{
	HistEvent ev;
	History *h = (History *) h_malloc(sizeof(History));
	if (h == NULL)
		return NULL;

	if (history_def_init(&h->h_ref, &ev, 0) == -1) {
		h_free((ptr_t)h);
		return NULL;
	}
	h->h_ent = -1;
	h->h_next = history_def_next;
	h->h_first = history_def_first;
	h->h_last = history_def_last;
	h->h_prev = history_def_prev;
	h->h_curr = history_def_curr;
	h->h_set = history_def_set;
	h->h_clear = history_def_clear;
	h->h_enter = history_def_enter;
	h->h_add = history_def_add;

	return (h);
}


/* history_end():
 *	clean up history;
 */
public void
history_end(History *h)
{
	HistEvent ev;

	if (h->h_next == history_def_next)
		history_def_clear(h->h_ref, &ev);
}



/* history_setsize():
 *	Set history number of events
 */
private int
history_setsize(History *h, HistEvent *ev, int num)
{

	if (h->h_next != history_def_next) {
		he_seterrev(ev, _HE_NOT_ALLOWED);
		return (-1);
	}
	if (num < 0) {
		he_seterrev(ev, _HE_BAD_PARAM);
		return (-1);
	}
	history_def_setsize(h->h_ref, num);
	return (0);
}


/* history_getsize():
 *      Get number of events currently in history
 */
private int
history_getsize(History *h, HistEvent *ev)
{
	if (h->h_next != history_def_next) {
		he_seterrev(ev, _HE_NOT_ALLOWED);
		return (-1);
	}
	ev->num = history_def_getsize(h->h_ref);
	if (ev->num < -1) {
		he_seterrev(ev, _HE_SIZE_NEGATIVE);
		return (-1);
	}
	return (0);
}


/* history_setunique():
 *	Set if adjacent equal events should not be entered in history.
 */
private int
history_setunique(History *h, HistEvent *ev, int uni)
{

	if (h->h_next != history_def_next) {
		he_seterrev(ev, _HE_NOT_ALLOWED);
		return (-1);
	}
	history_def_setunique(h->h_ref, uni);
	return (0);
}


/* history_getunique():
 *	Get if adjacent equal events should not be entered in history.
 */
private int
history_getunique(History *h, HistEvent *ev)
{
	if (h->h_next != history_def_next) {
		he_seterrev(ev, _HE_NOT_ALLOWED);
		return (-1);
	}
	ev->num = history_def_getunique(h->h_ref);
	return (0);
}


/* history_set_fun():
 *	Set history functions
 */
private int
history_set_fun(History *h, History *nh)
{
	HistEvent ev;

	if (nh->h_first == NULL || nh->h_next == NULL || nh->h_last == NULL ||
	    nh->h_prev == NULL || nh->h_curr == NULL || nh->h_set == NULL ||
	    nh->h_enter == NULL || nh->h_add == NULL || nh->h_clear == NULL ||
	    nh->h_ref == NULL) {
		if (h->h_next != history_def_next) {
			history_def_init(&h->h_ref, &ev, 0);
			h->h_first = history_def_first;
			h->h_next = history_def_next;
			h->h_last = history_def_last;
			h->h_prev = history_def_prev;
			h->h_curr = history_def_curr;
			h->h_set = history_def_set;
			h->h_clear = history_def_clear;
			h->h_enter = history_def_enter;
			h->h_add = history_def_add;
		}
		return (-1);
	}
	if (h->h_next == history_def_next)
		history_def_clear(h->h_ref, &ev);

	h->h_ent = -1;
	h->h_first = nh->h_first;
	h->h_next = nh->h_next;
	h->h_last = nh->h_last;
	h->h_prev = nh->h_prev;
	h->h_curr = nh->h_curr;
	h->h_set = nh->h_set;
	h->h_clear = nh->h_clear;
	h->h_enter = nh->h_enter;
	h->h_add = nh->h_add;

	return (0);
}


/* history_load():
 *	History load function
 */
private int
history_load(History *h, const char *fname)
{
	FILE *fp;
	char *line;
	size_t sz, max_size;
	char *ptr;
	int i = -1;
	HistEvent ev;

	if ((fp = fopen(fname, "r")) == NULL)
		return (i);

	ptr = h_malloc(max_size = 1024);
	if (ptr == NULL)
		goto done;
	for (i = 0; (line = fgetln(fp, &sz)) != NULL; i++) {
		char c = line[sz];

		if (sz != 0 && line[sz - 1] == '\n')
			line[--sz] = '\0';
		else
			line[sz] = '\0';

		if (max_size < sz) {
			char *nptr;
			max_size = (sz + 1023) & ~1023;
			nptr = h_realloc(ptr, max_size);
			if (nptr == NULL) {
				i = -1;
				goto oomem;
			}
			ptr = nptr;
		}
		(void) strunvis(ptr, line);
		line[sz] = c;
		if (HENTER(h, &ev, ptr) == -1) {
			h_free((ptr_t)ptr);
			return -1;
		}
	}
oomem:
	h_free((ptr_t)ptr);
done:
	(void) fclose(fp);
	return (i);
}


/* history_save():
 *	History save function
 */
private int
history_save(History *h, const char *fname)
{
	FILE *fp;
	HistEvent ev;
	int i = -1, retval;
	size_t len, max_size;
	char *ptr;

	if ((fp = fopen(fname, "w")) == NULL)
		return (-1);

	if (fchmod(fileno(fp), S_IRUSR|S_IWUSR) == -1)
		goto done;
	ptr = h_malloc(max_size = 1024);
	if (ptr == NULL)
		goto done;
	for (i = 0, retval = HLAST(h, &ev);
	    retval != -1;
	    retval = HPREV(h, &ev), i++) {
		len = strlen(ev.str) * 4 + 1;
		if (len >= max_size) {
			char *nptr;
			max_size = (len + 1023) & ~1023;
			nptr = h_realloc(ptr, max_size);
			if (nptr == NULL) {
				i = -1;
				goto oomem;
			}
			ptr = nptr;
		}
		(void) strvis(ptr, ev.str, VIS_WHITE);
		(void) fprintf(fp, "%s\n", ev.str);
	}
oomem:
	h_free((ptr_t)ptr);
done:
	(void) fclose(fp);
	return (i);
}


/* history_prev_event():
 *	Find the previous event, with number given
 */
private int
history_prev_event(History *h, HistEvent *ev, int num)
{
	int retval;

	for (retval = HCURR(h, ev); retval != -1; retval = HPREV(h, ev))
		if (ev->num == num)
			return (0);

	he_seterrev(ev, _HE_NOT_FOUND);
	return (-1);
}


/* history_next_event():
 *	Find the next event, with number given
 */
private int
history_next_event(History *h, HistEvent *ev, int num)
{
	int retval;

	for (retval = HCURR(h, ev); retval != -1; retval = HNEXT(h, ev))
		if (ev->num == num)
			return (0);

	he_seterrev(ev, _HE_NOT_FOUND);
	return (-1);
}


/* history_prev_string():
 *	Find the previous event beginning with string
 */
private int
history_prev_string(History *h, HistEvent *ev, const char *str)
{
	size_t len = strlen(str);
	int retval;

	for (retval = HCURR(h, ev); retval != -1; retval = HNEXT(h, ev))
		if (strncmp(str, ev->str, len) == 0)
			return (0);

	he_seterrev(ev, _HE_NOT_FOUND);
	return (-1);
}


/* history_next_string():
 *	Find the next event beginning with string
 */
private int
history_next_string(History *h, HistEvent *ev, const char *str)
{
	size_t len = strlen(str);
	int retval;

	for (retval = HCURR(h, ev); retval != -1; retval = HPREV(h, ev))
		if (strncmp(str, ev->str, len) == 0)
			return (0);

	he_seterrev(ev, _HE_NOT_FOUND);
	return (-1);
}


/* history():
 *	User interface to history functions.
 */
int
history(History *h, HistEvent *ev, int fun, ...)
{
	va_list va;
	const char *str;
	int retval;

	va_start(va, fun);

	he_seterrev(ev, _HE_OK);

	switch (fun) {
	case H_GETSIZE:
		retval = history_getsize(h, ev);
		break;

	case H_SETSIZE:
		retval = history_setsize(h, ev, va_arg(va, int));
		break;

	case H_GETUNIQUE:
		retval = history_getunique(h, ev);
		break;

	case H_SETUNIQUE:
		retval = history_setunique(h, ev, va_arg(va, int));
		break;

	case H_ADD:
		str = va_arg(va, const char *);
		retval = HADD(h, ev, str);
		break;

	case H_ENTER:
		str = va_arg(va, const char *);
		if ((retval = HENTER(h, ev, str)) != -1)
			h->h_ent = ev->num;
		break;

	case H_APPEND:
		str = va_arg(va, const char *);
		if ((retval = HSET(h, ev, h->h_ent)) != -1)
			retval = HADD(h, ev, str);
		break;

	case H_FIRST:
		retval = HFIRST(h, ev);
		break;

	case H_NEXT:
		retval = HNEXT(h, ev);
		break;

	case H_LAST:
		retval = HLAST(h, ev);
		break;

	case H_PREV:
		retval = HPREV(h, ev);
		break;

	case H_CURR:
		retval = HCURR(h, ev);
		break;

	case H_SET:
		retval = HSET(h, ev, va_arg(va, const int));
		break;

	case H_CLEAR:
		HCLEAR(h, ev);
		retval = 0;
		break;

	case H_LOAD:
		retval = history_load(h, va_arg(va, const char *));
		if (retval == -1)
			he_seterrev(ev, _HE_HIST_READ);
		break;

	case H_SAVE:
		retval = history_save(h, va_arg(va, const char *));
		if (retval == -1)
			he_seterrev(ev, _HE_HIST_WRITE);
		break;

	case H_PREV_EVENT:
		retval = history_prev_event(h, ev, va_arg(va, int));
		break;

	case H_NEXT_EVENT:
		retval = history_next_event(h, ev, va_arg(va, int));
		break;

	case H_PREV_STR:
		retval = history_prev_string(h, ev, va_arg(va, const char *));
		break;

	case H_NEXT_STR:
		retval = history_next_string(h, ev, va_arg(va, const char *));
		break;

	case H_FUNC:
	{
		History hf;

		hf.h_ref = va_arg(va, ptr_t);
		h->h_ent = -1;
		hf.h_first = va_arg(va, history_gfun_t);
		hf.h_next = va_arg(va, history_gfun_t);
		hf.h_last = va_arg(va, history_gfun_t);
		hf.h_prev = va_arg(va, history_gfun_t);
		hf.h_curr = va_arg(va, history_gfun_t);
		hf.h_set = va_arg(va, history_sfun_t);
		hf.h_clear = va_arg(va, history_vfun_t);
		hf.h_enter = va_arg(va, history_efun_t);
		hf.h_add = va_arg(va, history_efun_t);

		if ((retval = history_set_fun(h, &hf)) == -1)
			he_seterrev(ev, _HE_PARAM_MISSING);
		break;
	}

	case H_END:
		history_end(h);
		retval = 0;
		break;

	default:
		retval = -1;
		he_seterrev(ev, _HE_UNKNOWN);
		break;
	}
	va_end(va);
	return (retval);
}
