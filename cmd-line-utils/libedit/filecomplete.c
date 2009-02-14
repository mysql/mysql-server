/*	$NetBSD: filecomplete.c,v 1.13 2009/01/26 17:32:41 apb Exp $	*/

/*-
 * Copyright (c) 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jaromir Dolecek.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/* AIX requires this to be the first thing in the file.  */
#if defined (_AIX) && !defined (__GNUC__)
 #pragma alloca
#endif

#include "config.h"

/* XXXMYSQL */
#ifdef __GNUC__
# undef alloca
# define alloca(n) __builtin_alloca (n)
#else
# ifdef HAVE_ALLOCA_H
#  include <alloca.h>
# else
#  ifndef _AIX
extern char *alloca ();
#  endif
# endif
#endif

#if !defined(lint) && !defined(SCCSID)
#endif /* not lint && not SCCSID */

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <pwd.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include <fcntl.h>
#ifdef HAVE_VIS_H
#include <vis.h>
#else
#include "np/vis.h"
#endif
#ifdef HAVE_ALLOCA_H
#include <alloca.h>
#endif
#include "el.h"
#include "fcns.h"		/* for EL_NUM_FCNS */
#include "histedit.h"
#include "filecomplete.h"

static char break_chars[] = { ' ', '\t', '\n', '"', '\\', '\'', '`', '@', '$',
    '>', '<', '=', ';', '|', '&', '{', '(', '\0' };


/********************************/
/* completion functions */

/*
 * does tilde expansion of strings of type ``~user/foo''
 * if ``user'' isn't valid user name or ``txt'' doesn't start
 * w/ '~', returns pointer to strdup()ed copy of ``txt''
 *
 * it's callers's responsibility to free() returned string
 */
char *
fn_tilde_expand(const char *txt)
{
	struct passwd pwres, *pass;
	char *temp;
	size_t len = 0;
	char pwbuf[1024];

	if (txt[0] != '~')
		return (strdup(txt));

	temp = strchr(txt + 1, '/');
	if (temp == NULL) {
		temp = strdup(txt + 1);
		if (temp == NULL)
			return NULL;
	} else {
		len = temp - txt + 1;	/* text until string after slash */
		temp = malloc(len);
		if (temp == NULL)
			return NULL;
		(void)strncpy(temp, txt + 1, len - 2);
		temp[len - 2] = '\0';
	}
	/* XXXMYSQL: use non-_r functions for now */
	if (temp[0] == 0) {
		pass = getpwuid(getuid());
	} else {
		pass = getpwnam(temp);
	}
	free(temp);		/* value no more needed */
	if (pass == NULL)
		return (strdup(txt));

	/* update pointer txt to point at string immedially following */
	/* first slash */
	txt += len;

	temp = malloc(strlen(pass->pw_dir) + 1 + strlen(txt) + 1);
	if (temp == NULL)
		return NULL;
	(void)sprintf(temp, "%s/%s", pass->pw_dir, txt);

	return (temp);
}


/*
 * return first found file name starting by the ``text'' or NULL if no
 * such file can be found
 * value of ``state'' is ignored
 *
 * it's caller's responsibility to free returned string
 */
char *
fn_filename_completion_function(const char *text, int state)
{
	static DIR *dir = NULL;
	static char *filename = NULL, *dirname = NULL, *dirpath = NULL;
	static size_t filename_len = 0;
	struct dirent *entry;
	char *temp;
	size_t len;

	if (state == 0 || dir == NULL) {
		temp = strrchr(text, '/');
		if (temp) {
			char *nptr;
			temp++;
			nptr = realloc(filename, strlen(temp) + 1);
			if (nptr == NULL) {
				free(filename);
				return NULL;
			}
			filename = nptr;
			(void)strcpy(filename, temp);
			len = temp - text;	/* including last slash */
			nptr = realloc(dirname, len + 1);
			if (nptr == NULL) {
				free(filename);
				return NULL;
			}
			dirname = nptr;
			(void)strncpy(dirname, text, len);
			dirname[len] = '\0';
		} else {
			if (*text == 0)
				filename = NULL;
			else {
				filename = strdup(text);
				if (filename == NULL)
					return NULL;
			}
			dirname = NULL;
		}

		if (dir != NULL) {
			(void)closedir(dir);
			dir = NULL;
		}

		/* support for ``~user'' syntax */
		free(dirpath);

		if (dirname == NULL && (dirname = strdup("./")) == NULL)
			return NULL;

		if (*dirname == '~')
			dirpath = fn_tilde_expand(dirname);
		else
			dirpath = strdup(dirname);

		if (dirpath == NULL)
			return NULL;

		dir = opendir(dirpath);
		if (!dir)
			return (NULL);	/* cannot open the directory */

		/* will be used in cycle */
		filename_len = filename ? strlen(filename) : 0;
	}

	/* find the match */
	while ((entry = readdir(dir)) != NULL) {
		/* skip . and .. */
		if (entry->d_name[0] == '.' && (!entry->d_name[1]
		    || (entry->d_name[1] == '.' && !entry->d_name[2])))
			continue;
		if (filename_len == 0)
			break;
		/* otherwise, get first entry where first */
		/* filename_len characters are equal	  */
		if (entry->d_name[0] == filename[0]
#if HAVE_STRUCT_DIRENT_D_NAMLEN
		    && entry->d_namlen >= filename_len
#else
		    && strlen(entry->d_name) >= filename_len
#endif
		    && strncmp(entry->d_name, filename,
			filename_len) == 0)
			break;
	}

	if (entry) {		/* match found */

#if HAVE_STRUCT_DIRENT_D_NAMLEN
		len = entry->d_namlen;
#else
		len = strlen(entry->d_name);
#endif

		temp = malloc(strlen(dirname) + len + 1);
		if (temp == NULL)
			return NULL;
		(void)sprintf(temp, "%s%s", dirname, entry->d_name);
	} else {
		(void)closedir(dir);
		dir = NULL;
		temp = NULL;
	}

	return (temp);
}


static const char *
append_char_function(const char *name)
{
	struct stat stbuf;
	char *expname = *name == '~' ? fn_tilde_expand(name) : NULL;
	const char *rs = " ";

	if (stat(expname ? expname : name, &stbuf) == -1)
		goto out;
	if (S_ISDIR(stbuf.st_mode))
		rs = "/";
out:
	if (expname)
		free(expname);
	return rs;
}
/*
 * returns list of completions for text given
 * non-static for readline.
 */
char ** completion_matches(const char *, char *(*)(const char *, int));
char **
completion_matches(const char *text, char *(*genfunc)(const char *, int))
{
	char **match_list = NULL, *retstr, *prevstr;
	size_t match_list_len, max_equal, which, i;
	size_t matches;

	matches = 0;
	match_list_len = 1;
	while ((retstr = (*genfunc) (text, (int)matches)) != NULL) {
		/* allow for list terminator here */
		if (matches + 3 >= match_list_len) {
			char **nmatch_list;
			while (matches + 3 >= match_list_len)
				match_list_len <<= 1;
			nmatch_list = realloc(match_list,
			    match_list_len * sizeof(char *));
			if (nmatch_list == NULL) {
				free(match_list);
				return NULL;
			}
			match_list = nmatch_list;

		}
		match_list[++matches] = retstr;
	}

	if (!match_list)
		return NULL;	/* nothing found */

	/* find least denominator and insert it to match_list[0] */
	which = 2;
	prevstr = match_list[1];
	max_equal = strlen(prevstr);
	for (; which <= matches; which++) {
		for (i = 0; i < max_equal &&
		    prevstr[i] == match_list[which][i]; i++)
			continue;
		max_equal = i;
	}

	retstr = malloc(max_equal + 1);
	if (retstr == NULL) {
		free(match_list);
		return NULL;
	}
	(void)strncpy(retstr, match_list[1], max_equal);
	retstr[max_equal] = '\0';
	match_list[0] = retstr;

	/* add NULL as last pointer to the array */
	match_list[matches + 1] = (char *) NULL;

	return (match_list);
}

/*
 * Sort function for qsort(). Just wrapper around strcasecmp().
 */
static int
_fn_qsort_string_compare(const void *i1, const void *i2)
{
	const char *s1 = ((const char * const *)i1)[0];
	const char *s2 = ((const char * const *)i2)[0];

	return strcasecmp(s1, s2);
}

/*
 * Display list of strings in columnar format on readline's output stream.
 * 'matches' is list of strings, 'len' is number of strings in 'matches',
 * 'max' is maximum length of string in 'matches'.
 */
void
fn_display_match_list (EditLine *el, char **matches, int len, int max)
{
	int i, idx, limit, count;
	int screenwidth = el->el_term.t_size.h;

	/*
	 * Find out how many entries can be put on one line, count
	 * with two spaces between strings.
	 */
	limit = screenwidth / (max + 2);
	if (limit == 0)
		limit = 1;

	/* how many lines of output */
	count = len / limit;
	if (count * limit < len)
		count++;

	/* Sort the items if they are not already sorted. */
	qsort(&matches[1], (size_t)(len - 1), sizeof(char *),
	    _fn_qsort_string_compare);

	idx = 1;
	for(; count > 0; count--) {
		for(i = 0; i < limit && matches[idx]; i++, idx++)
			(void)fprintf(el->el_outfile, "%-*s  ", max,
			    matches[idx]);
		(void)fprintf(el->el_outfile, "\n");
	}
}

/*
 * Complete the word at or before point,
 * 'what_to_do' says what to do with the completion.
 * \t   means do standard completion.
 * `?' means list the possible completions.
 * `*' means insert all of the possible completions.
 * `!' means to do standard completion, and list all possible completions if
 * there is more than one.
 *
 * Note: '*' support is not implemented
 *       '!' could never be invoked
 */
int
fn_complete(EditLine *el,
	char *(*complet_func)(const char *, int),
	char **(*attempted_completion_function)(const char *, int, int),
	const char *word_break, const char *special_prefixes,
	const char *(*app_func)(const char *), int query_items,
	int *completion_type, int *over, int *point, int *end)
{
	const LineInfo *li;
	char *temp, **matches;
	const char *ctemp;
	size_t len;
	int what_to_do = '\t';
	int retval = CC_NORM;

	if (el->el_state.lastcmd == el->el_state.thiscmd)
		what_to_do = '?';

	/* readline's rl_complete() has to be told what we did... */
	if (completion_type != NULL)
		*completion_type = what_to_do;

	if (!complet_func)
		complet_func = fn_filename_completion_function;
	if (!app_func)
		app_func = append_char_function;

	/* We now look backwards for the start of a filename/variable word */
	li = el_line(el);
	ctemp = (const char *) li->cursor;
	while (ctemp > li->buffer
	    && !strchr(word_break, ctemp[-1])
	    && (!special_prefixes || !strchr(special_prefixes, ctemp[-1]) ) )
		ctemp--;

	len = li->cursor - ctemp;
#if defined(__SSP__) || defined(__SSP_ALL__)
	temp = malloc(len + 1);
#else
	temp = alloca(len + 1);
#endif
	(void)strncpy(temp, ctemp, len);
	temp[len] = '\0';

	/* these can be used by function called in completion_matches() */
	/* or (*attempted_completion_function)() */
	if (point != 0)
		*point = li->cursor - li->buffer;
	if (end != NULL)
		*end = li->lastchar - li->buffer;

	if (attempted_completion_function) {
		int cur_off = li->cursor - li->buffer;
		matches = (*attempted_completion_function) (temp,
		    (int)(cur_off - len), cur_off);
	} else
		matches = 0;
	if (!attempted_completion_function || 
	    (over != NULL && !*over && !matches))
		matches = completion_matches(temp, complet_func);

	if (over != NULL)
		*over = 0;

	if (matches) {
		int i;
		int matches_num, maxlen, match_len, match_display=1;

		retval = CC_REFRESH;
		/*
		 * Only replace the completed string with common part of
		 * possible matches if there is possible completion.
		 */
		if (matches[0][0] != '\0') {
			el_deletestr(el, (int) len);
			el_insertstr(el, matches[0]);
		}

		if (what_to_do == '?')
			goto display_matches;

		if (matches[2] == NULL && strcmp(matches[0], matches[1]) == 0) {
			/*
			 * We found exact match. Add a space after
			 * it, unless we do filename completion and the
			 * object is a directory.
			 */
			el_insertstr(el, (*app_func)(matches[0])); 
		} else if (what_to_do == '!') {
    display_matches:
			/*
			 * More than one match and requested to list possible
			 * matches.
			 */

			for(i=1, maxlen=0; matches[i]; i++) {
				match_len = strlen(matches[i]);
				if (match_len > maxlen)
					maxlen = match_len;
			}
			matches_num = i - 1;
				
			/* newline to get on next line from command line */
			(void)fprintf(el->el_outfile, "\n");

			/*
			 * If there are too many items, ask user for display
			 * confirmation.
			 */
			if (matches_num > query_items) {
				(void)fprintf(el->el_outfile,
				    "Display all %d possibilities? (y or n) ",
				    matches_num);
				(void)fflush(el->el_outfile);
				if (getc(stdin) != 'y')
					match_display = 0;
				(void)fprintf(el->el_outfile, "\n");
			}

			if (match_display)
				fn_display_match_list(el, matches, matches_num,
					maxlen);
			retval = CC_REDISPLAY;
		} else if (matches[0][0]) {
			/*
			 * There was some common match, but the name was
			 * not complete enough. Next tab will print possible
			 * completions.
			 */
			el_beep(el);
		} else {
			/* lcd is not a valid object - further specification */
			/* is needed */
			el_beep(el);
			retval = CC_NORM;
		}

		/* free elements of array and the array itself */
		for (i = 0; matches[i]; i++)
			free(matches[i]);
		free(matches);
		matches = NULL;
	}
#if defined(__SSP__) || defined(__SSP_ALL__)
	free(temp);
#endif
	return retval;
}

/*
 * el-compatible wrapper around rl_complete; needed for key binding
 */
/* ARGSUSED */
unsigned char
_el_fn_complete(EditLine *el, int ch __attribute__((__unused__)))
{
	return (unsigned char)fn_complete(el, NULL, NULL,
	    break_chars, NULL, NULL, 100,
	    NULL, NULL, NULL, NULL);
}
