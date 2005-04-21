/*	$NetBSD: readline.c,v 1.49 2005/03/10 19:34:46 christos Exp $	*/

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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
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

#include <config.h>

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
#include <vis.h>

#include "el.h"
#include "fcns.h"		/* for EL_NUM_FCNS */
#include "histedit.h"
#include "readline/readline.h"

/* for rl_complete() */
#define TAB		'\r'

/* see comment at the #ifdef for sense of this */
/* #define GDB_411_HACK */

/* readline compatibility stuff - look at readline sources/documentation */
/* to see what these variables mean */
const char *rl_library_version = "EditLine wrapper";
static char empty[] = { '\0' };
static char expand_chars[] = { ' ', '\t', '\n', '=', '(', '\0' };
static char break_chars[] = { ' ', '\t', '\n', '"', '\\', '\'', '`', '@', '$',
    '>', '<', '=', ';', '|', '&', '{', '(', '\0' };
char *rl_readline_name = empty;
FILE *rl_instream = NULL;
FILE *rl_outstream = NULL;
int rl_point = 0;
int rl_end = 0;
char *rl_line_buffer = NULL;
VFunction *rl_linefunc = NULL;
int rl_done = 0;
VFunction *rl_event_hook = NULL;

int history_base = 1;		/* probably never subject to change */
int history_length = 0;
int max_input_history = 0;
char history_expansion_char = '!';
char history_subst_char = '^';
char *history_no_expand_chars = expand_chars;
Function *history_inhibit_expansion_function = NULL;
char *history_arg_extract(int start, int end, const char *str);

int rl_inhibit_completion = 0;
int rl_attempted_completion_over = 0;
char *rl_basic_word_break_characters = break_chars;
char *rl_completer_word_break_characters = NULL;
char *rl_completer_quote_characters = NULL;
Function *rl_completion_entry_function = NULL;
CPPFunction *rl_attempted_completion_function = NULL;
Function *rl_pre_input_hook = NULL;
Function *rl_startup1_hook = NULL;
Function *rl_getc_function = NULL;
char *rl_terminal_name = NULL;
int rl_already_prompted = 0;
int rl_filename_completion_desired = 0;
int rl_ignore_completion_duplicates = 0;
int rl_catch_signals = 1;
VFunction *rl_redisplay_function = NULL;
Function *rl_startup_hook = NULL;
VFunction *rl_completion_display_matches_hook = NULL;
VFunction *rl_prep_term_function = NULL;
VFunction *rl_deprep_term_function = NULL;

/*
 * The current prompt string.
 */
char *rl_prompt = NULL;
/*
 * This is set to character indicating type of completion being done by
 * rl_complete_internal(); this is available for application completion
 * functions.
 */
int rl_completion_type = 0;

/*
 * If more than this number of items results from query for possible
 * completions, we ask user if they are sure to really display the list.
 */
int rl_completion_query_items = 100;

/*
 * List of characters which are word break characters, but should be left
 * in the parsed text when it is passed to the completion function.
 * Shell uses this to help determine what kind of completing to do.
 */
char *rl_special_prefixes = (char *)NULL;

/*
 * This is the character appended to the completed words if at the end of
 * the line. Default is ' ' (a space).
 */
int rl_completion_append_character = ' ';

/* stuff below is used internally by libedit for readline emulation */

/* if not zero, non-unique completions always show list of possible matches */
static int _rl_complete_show_all = 0;

static History *h = NULL;
static EditLine *e = NULL;
static Function *map[256];
static int el_rl_complete_cmdnum = 0;

/* internal functions */
static unsigned char	 _el_rl_complete(EditLine *, int);
static unsigned char	 _el_rl_tstp(EditLine *, int);
static char		*_get_prompt(EditLine *);
static HIST_ENTRY	*_move_history(int);
static int		 _history_expand_command(const char *, size_t, size_t,
    char **);
static char		*_rl_compat_sub(const char *, const char *,
    const char *, int);
static int		 _rl_complete_internal(int);
static int		 _rl_qsort_string_compare(const void *, const void *);
static int		 _rl_event_read_char(EditLine *, char *);
static void		 _rl_update_pos(void);


/* ARGSUSED */
static char *
_get_prompt(EditLine *el __attribute__((__unused__)))
{
	rl_already_prompted = 1;
	return (rl_prompt);
}


/*
 * generic function for moving around history
 */
static HIST_ENTRY *
_move_history(int op)
{
	HistEvent ev;
	static HIST_ENTRY rl_he;

	if (history(h, &ev, op) != 0)
		return (HIST_ENTRY *) NULL;

	rl_he.line = ev.str;
	rl_he.data = (histdata_t) &(ev.num);

	return (&rl_he);
}


/*
 * READLINE compatibility stuff
 */

/*
 * initialize rl compat stuff
 */
int
rl_initialize(void)
{
	HistEvent ev;
	const LineInfo *li;
	int i;
	int editmode = 1;
	struct termios t;

	if (e != NULL)
		el_end(e);
	if (h != NULL)
		history_end(h);

	if (!rl_instream)
		rl_instream = stdin;
	if (!rl_outstream)
		rl_outstream = stdout;

	/*
	 * See if we don't really want to run the editor
	 */
	if (tcgetattr(fileno(rl_instream), &t) != -1 && (t.c_lflag & ECHO) == 0)
		editmode = 0;

	e = el_init(rl_readline_name, rl_instream, rl_outstream, stderr);

	if (!editmode)
		el_set(e, EL_EDITMODE, 0);

	h = history_init();
	if (!e || !h)
		return (-1);

	history(h, &ev, H_SETSIZE, INT_MAX);	/* unlimited */
	history_length = 0;
	max_input_history = INT_MAX;
	el_set(e, EL_HIST, history, h);

	/* for proper prompt printing in readline() */
	rl_prompt = strdup("");
	if (rl_prompt == NULL) {
		history_end(h);
		el_end(e);
		return -1;
	}
	el_set(e, EL_PROMPT, _get_prompt);
	el_set(e, EL_SIGNAL, rl_catch_signals);

	/* set default mode to "emacs"-style and read setting afterwards */
	/* so this can be overriden */
	el_set(e, EL_EDITOR, "emacs");
	if (rl_terminal_name != NULL)
		el_set(e, EL_TERMINAL, rl_terminal_name);
	else
		el_get(e, EL_TERMINAL, &rl_terminal_name);

	/*
	 * Word completion - this has to go AFTER rebinding keys
	 * to emacs-style.
	 */
	el_set(e, EL_ADDFN, "rl_complete",
	    "ReadLine compatible completion function",
	    _el_rl_complete);
	el_set(e, EL_BIND, "^I", "rl_complete", NULL);

	/*
	 * Send TSTP when ^Z is pressed.
	 */
	el_set(e, EL_ADDFN, "rl_tstp",
	    "ReadLine compatible suspend function",
	    _el_rl_tstp);
	el_set(e, EL_BIND, "^Z", "rl_tstp", NULL);

	/*
	 * Find out where the rl_complete function was added; this is
	 * used later to detect that lastcmd was also rl_complete.
	 */
	for(i=EL_NUM_FCNS; i < e->el_map.nfunc; i++) {
		if (e->el_map.func[i] == _el_rl_complete) {
			el_rl_complete_cmdnum = i;
			break;
		}
	}
		
	/* read settings from configuration file */
	el_source(e, NULL);

	/*
	 * Unfortunately, some applications really do use rl_point
	 * and rl_line_buffer directly.
	 */
	li = el_line(e);
	/* a cheesy way to get rid of const cast. */
	rl_line_buffer = memchr(li->buffer, *li->buffer, 1);
	_rl_update_pos();

	if (rl_startup_hook)
		(*rl_startup_hook)(NULL, 0);

	return (0);
}


/*
 * read one line from input stream and return it, chomping
 * trailing newline (if there is any)
 */
char *
readline(const char *prompt)
{
	HistEvent ev;
	int count;
	const char *ret;
	char *buf;
	static int used_event_hook;

	if (e == NULL || h == NULL)
		rl_initialize();

	rl_done = 0;

	/* update prompt accordingly to what has been passed */
	if (!prompt)
		prompt = "";
	if (strcmp(rl_prompt, prompt) != 0) {
		free(rl_prompt);
		rl_prompt = strdup(prompt);
		if (rl_prompt == NULL)
			return NULL;
	}

	if (rl_pre_input_hook)
		(*rl_pre_input_hook)(NULL, 0);

	if (rl_event_hook && !(e->el_flags&NO_TTY)) {
		el_set(e, EL_GETCFN, _rl_event_read_char);
		used_event_hook = 1;
	}

	if (!rl_event_hook && used_event_hook) {
		el_set(e, EL_GETCFN, EL_BUILTIN_GETCFN);
		used_event_hook = 0;
	}

	rl_already_prompted = 0;

	/* get one line from input stream */
	ret = el_gets(e, &count);

	if (ret && count > 0) {
		int lastidx;

		buf = strdup(ret);
		if (buf == NULL)
			return NULL;
		lastidx = count - 1;
		if (buf[lastidx] == '\n')
			buf[lastidx] = '\0';
	} else
		buf = NULL;

	history(h, &ev, H_GETSIZE);
	history_length = ev.num;

	return buf;
}

/*
 * history functions
 */

/*
 * is normally called before application starts to use
 * history expansion functions
 */
void
using_history(void)
{
	if (h == NULL || e == NULL)
		rl_initialize();
}


/*
 * substitute ``what'' with ``with'', returning resulting string; if
 * globally == 1, substitutes all occurrences of what, otherwise only the
 * first one
 */
static char *
_rl_compat_sub(const char *str, const char *what, const char *with,
    int globally)
{
	const	char	*s;
	char	*r, *result;
	size_t	len, with_len, what_len;

	len = strlen(str);
	with_len = strlen(with);
	what_len = strlen(what);

	/* calculate length we need for result */
	s = str;
	while (*s) {
		if (*s == *what && !strncmp(s, what, what_len)) {
			len += with_len - what_len;
			if (!globally)
				break;
			s += what_len;
		} else
			s++;
	}
	r = result = malloc(len + 1);
	if (result == NULL)
		return NULL;
	s = str;
	while (*s) {
		if (*s == *what && !strncmp(s, what, what_len)) {
			(void)strncpy(r, with, with_len);
			r += with_len;
			s += what_len;
			if (!globally) {
				(void)strcpy(r, s);
				return(result);
			}
		} else
			*r++ = *s++;
	}
	*r = 0;
	return(result);
}

static	char	*last_search_pat;	/* last !?pat[?] search pattern */
static	char	*last_search_match;	/* last !?pat[?] that matched */

const char *
get_history_event(const char *cmd, int *cindex, int qchar)
{
	int idx, sign, sub, num, begin, ret;
	size_t len;
	char	*pat;
	const char *rptr;
	HistEvent ev;

	idx = *cindex;
	if (cmd[idx++] != history_expansion_char)
		return(NULL);

	/* find out which event to take */
	if (cmd[idx] == history_expansion_char || cmd[idx] == 0) {
		if (history(h, &ev, H_FIRST) != 0)
			return(NULL);
		*cindex = cmd[idx]? (idx + 1):idx;
		return(ev.str);
	}
	sign = 0;
	if (cmd[idx] == '-') {
		sign = 1;
		idx++;
	}

	if ('0' <= cmd[idx] && cmd[idx] <= '9') {
		HIST_ENTRY *rl_he;

		num = 0;
		while (cmd[idx] && '0' <= cmd[idx] && cmd[idx] <= '9') {
			num = num * 10 + cmd[idx] - '0';
			idx++;
		}
		if (sign)
			num = history_length - num + 1;

		if (!(rl_he = history_get(num)))
			return(NULL);

		*cindex = idx;
		return(rl_he->line);
	}
	sub = 0;
	if (cmd[idx] == '?') {
		sub = 1;
		idx++;
	}
	begin = idx;
	while (cmd[idx]) {
		if (cmd[idx] == '\n')
			break;
		if (sub && cmd[idx] == '?')
			break;
		if (!sub && (cmd[idx] == ':' || cmd[idx] == ' '
				    || cmd[idx] == '\t' || cmd[idx] == qchar))
			break;
		idx++;
	}
	len = idx - begin;
	if (sub && cmd[idx] == '?')
		idx++;
	if (sub && len == 0 && last_search_pat && *last_search_pat)
		pat = last_search_pat;
	else if (len == 0)
		return(NULL);
	else {
		if ((pat = malloc(len + 1)) == NULL)
			return NULL;
		(void)strncpy(pat, cmd + begin, len);
		pat[len] = '\0';
	}

	if (history(h, &ev, H_CURR) != 0) {
		if (pat != last_search_pat)
			free(pat);
		return (NULL);
	}
	num = ev.num;

	if (sub) {
		if (pat != last_search_pat) {
			if (last_search_pat)
				free(last_search_pat);
			last_search_pat = pat;
		}
		ret = history_search(pat, -1);
	} else
		ret = history_search_prefix(pat, -1);

	if (ret == -1) {
		/* restore to end of list on failed search */
		history(h, &ev, H_FIRST);
		(void)fprintf(rl_outstream, "%s: Event not found\n", pat);
		if (pat != last_search_pat)
			free(pat);
		return(NULL);
	}

	if (sub && len) {
		if (last_search_match && last_search_match != pat)
			free(last_search_match);
		last_search_match = pat;
	}

	if (pat != last_search_pat)
		free(pat);

	if (history(h, &ev, H_CURR) != 0)
		return(NULL);
	*cindex = idx;
	rptr = ev.str;

	/* roll back to original position */
	(void)history(h, &ev, H_SET, num);

	return rptr;
}

/*
 * the real function doing history expansion - takes as argument command
 * to do and data upon which the command should be executed
 * does expansion the way I've understood readline documentation
 *
 * returns 0 if data was not modified, 1 if it was and 2 if the string
 * should be only printed and not executed; in case of error,
 * returns -1 and *result points to NULL
 * it's callers responsibility to free() string returned in *result
 */
static int
_history_expand_command(const char *command, size_t offs, size_t cmdlen,
    char **result)
{
	char *tmp, *search = NULL, *aptr;
	const char *ptr, *cmd;
	static char *from = NULL, *to = NULL;
	int start, end, idx, has_mods = 0;
	int p_on = 0, g_on = 0;

	*result = NULL;
	aptr = NULL;
	ptr = NULL;

	/* First get event specifier */
	idx = 0;

	if (strchr(":^*$", command[offs + 1])) {
		char str[4];
		/*
		* "!:" is shorthand for "!!:".
		* "!^", "!*" and "!$" are shorthand for
		* "!!:^", "!!:*" and "!!:$" respectively.
		*/
		str[0] = str[1] = '!';
		str[2] = '0';
		ptr = get_history_event(str, &idx, 0);
		idx = (command[offs + 1] == ':')? 1:0;
		has_mods = 1;
	} else {
		if (command[offs + 1] == '#') {
			/* use command so far */
			if ((aptr = malloc(offs + 1)) == NULL)
				return -1;
			(void)strncpy(aptr, command, offs);
			aptr[offs] = '\0';
			idx = 1;
		} else {
			int	qchar;

			qchar = (offs > 0 && command[offs - 1] == '"')? '"':0;
			ptr = get_history_event(command + offs, &idx, qchar);
		}
		has_mods = command[offs + idx] == ':';
	}

	if (ptr == NULL && aptr == NULL)
		return(-1);

	if (!has_mods) {
		*result = strdup(aptr? aptr : ptr);
		if (aptr)
			free(aptr);
		return(1);
	}

	cmd = command + offs + idx + 1;

	/* Now parse any word designators */

	if (*cmd == '%')	/* last word matched by ?pat? */
		tmp = strdup(last_search_match? last_search_match:"");
	else if (strchr("^*$-0123456789", *cmd)) {
		start = end = -1;
		if (*cmd == '^')
			start = end = 1, cmd++;
		else if (*cmd == '$')
			start = -1, cmd++;
		else if (*cmd == '*')
			start = 1, cmd++;
	       else if (*cmd == '-' || isdigit((unsigned char) *cmd)) {
			start = 0;
			while (*cmd && '0' <= *cmd && *cmd <= '9')
				start = start * 10 + *cmd++ - '0';

			if (*cmd == '-') {
				if (isdigit((unsigned char) cmd[1])) {
					cmd++;
					end = 0;
					while (*cmd && '0' <= *cmd && *cmd <= '9')
						end = end * 10 + *cmd++ - '0';
				} else if (cmd[1] == '$') {
					cmd += 2;
					end = -1;
				} else {
					cmd++;
					end = -2;
				}
			} else if (*cmd == '*')
				end = -1, cmd++;
			else
				end = start;
		}
		tmp = history_arg_extract(start, end, aptr? aptr:ptr);
		if (tmp == NULL) {
			(void)fprintf(rl_outstream, "%s: Bad word specifier",
			    command + offs + idx);
			if (aptr)
				free(aptr);
			return(-1);
		}
	} else
		tmp = strdup(aptr? aptr:ptr);

	if (aptr)
		free(aptr);

	if (*cmd == 0 || (cmd - (command + offs) >= cmdlen)) {
		*result = tmp;
		return(1);
	}

	for (; *cmd; cmd++) {
		if (*cmd == ':')
			continue;
		else if (*cmd == 'h') {		/* remove trailing path */
			if ((aptr = strrchr(tmp, '/')) != NULL)
				*aptr = 0;
		} else if (*cmd == 't') {	/* remove leading path */
			if ((aptr = strrchr(tmp, '/')) != NULL) {
				aptr = strdup(aptr + 1);
				free(tmp);
				tmp = aptr;
			}
		} else if (*cmd == 'r') {	/* remove trailing suffix */
			if ((aptr = strrchr(tmp, '.')) != NULL)
				*aptr = 0;
		} else if (*cmd == 'e') {	/* remove all but suffix */
			if ((aptr = strrchr(tmp, '.')) != NULL) {
				aptr = strdup(aptr);
				free(tmp);
				tmp = aptr;
			}
		} else if (*cmd == 'p')		/* print only */
			p_on = 1;
		else if (*cmd == 'g')
			g_on = 2;
		else if (*cmd == 's' || *cmd == '&') {
			char *what, *with, delim;
			size_t len, from_len;
			size_t size;

			if (*cmd == '&' && (from == NULL || to == NULL))
				continue;
			else if (*cmd == 's') {
				delim = *(++cmd), cmd++;
				size = 16;
				what = realloc(from, size);
				if (what == NULL) {
					free(from);
					return 0;
				}
				len = 0;
				for (; *cmd && *cmd != delim; cmd++) {
					if (*cmd == '\\' && cmd[1] == delim)
						cmd++;
					if (len >= size) {
						char *nwhat;
						nwhat = realloc(what,
								(size <<= 1));
						if (nwhat == NULL) {
							free(what);
							return 0;
						}
						what = nwhat;
					}
					what[len++] = *cmd;
				}
				what[len] = '\0';
				from = what;
				if (*what == '\0') {
					free(what);
					if (search) {
						from = strdup(search);
						if (from == NULL)
							return 0;
					} else {
						from = NULL;
						return (-1);
					}
				}
				cmd++;	/* shift after delim */
				if (!*cmd)
					continue;

				size = 16;
				with = realloc(to, size);
				if (with == NULL) {
					free(to);
					return -1;
				}
				len = 0;
				from_len = strlen(from);
				for (; *cmd && *cmd != delim; cmd++) {
					if (len + from_len + 1 >= size) {
						char *nwith;
						size += from_len + 1;
						nwith = realloc(with, size);
						if (nwith == NULL) {
							free(with);
							return -1;
						}
						with = nwith;
					}
					if (*cmd == '&') {
						/* safe */
						(void)strcpy(&with[len], from);
						len += from_len;
						continue;
					}
					if (*cmd == '\\'
					    && (*(cmd + 1) == delim
						|| *(cmd + 1) == '&'))
						cmd++;
					with[len++] = *cmd;
				}
				with[len] = '\0';
				to = with;
			}

			aptr = _rl_compat_sub(tmp, from, to, g_on);
			if (aptr) {
				free(tmp);
				tmp = aptr;
			}
			g_on = 0;
		}
	}
	*result = tmp;
	return (p_on? 2:1);
}


/*
 * csh-style history expansion
 */
int
history_expand(char *str, char **output)
{
	int ret = 0;
	size_t idx, i, size;
	char *tmp, *result;

	if (h == NULL || e == NULL)
		rl_initialize();

	if (history_expansion_char == 0) {
		*output = strdup(str);
		return(0);
	}

	*output = NULL;
	if (str[0] == history_subst_char) {
		/* ^foo^foo2^ is equivalent to !!:s^foo^foo2^ */
		*output = malloc(strlen(str) + 4 + 1);
		if (*output == NULL)
			return 0;
		(*output)[0] = (*output)[1] = history_expansion_char;
		(*output)[2] = ':';
		(*output)[3] = 's';
		(void)strcpy((*output) + 4, str);
		str = *output;
	} else {
		*output = strdup(str);
		if (*output == NULL)
			return 0;
	}

#define ADD_STRING(what, len)						\
	{								\
		if (idx + len + 1 > size) {				\
			char *nresult = realloc(result, (size += len + 1));\
			if (nresult == NULL) {				\
				free(*output);				\
				return 0;				\
			}						\
			result = nresult;				\
		}							\
		(void)strncpy(&result[idx], what, len);			\
		idx += len;						\
		result[idx] = '\0';					\
	}

	result = NULL;
	size = idx = 0;
	for (i = 0; str[i];) {
		int qchar, loop_again;
		size_t len, start, j;

		qchar = 0;
		loop_again = 1;
		start = j = i;
loop:
		for (; str[j]; j++) {
			if (str[j] == '\\' &&
			    str[j + 1] == history_expansion_char) {
				(void)strcpy(&str[j], &str[j + 1]);
				continue;
			}
			if (!loop_again) {
				if (isspace((unsigned char) str[j])
				    || str[j] == qchar)
					break;
			}
			if (str[j] == history_expansion_char
			    && !strchr(history_no_expand_chars, str[j + 1])
			    && (!history_inhibit_expansion_function ||
			    (*history_inhibit_expansion_function)(str,
			    (int)j) == 0))
				break;
		}

		if (str[j] && loop_again) {
			i = j;
			qchar = (j > 0 && str[j - 1] == '"' )? '"':0;
			j++;
			if (str[j] == history_expansion_char)
				j++;
			loop_again = 0;
			goto loop;
		}
		len = i - start;
		tmp = &str[start];
		ADD_STRING(tmp, len);

		if (str[i] == '\0' || str[i] != history_expansion_char) {
			len = j - i;
			tmp = &str[i];
			ADD_STRING(tmp, len);
			if (start == 0)
				ret = 0;
			else
				ret = 1;
			break;
		}
		ret = _history_expand_command (str, i, (j - i), &tmp);
		if (ret > 0 && tmp) {
			len = strlen(tmp);
			ADD_STRING(tmp, len);
			free(tmp);
		}
		i = j;
	}

	/* ret is 2 for "print only" option */
	if (ret == 2) {
		add_history(result);
#ifdef GDB_411_HACK
		/* gdb 4.11 has been shipped with readline, where */
		/* history_expand() returned -1 when the line	  */
		/* should not be executed; in readline 2.1+	  */
		/* it should return 2 in such a case		  */
		ret = -1;
#endif
	}
	free(*output);
	*output = result;

	return (ret);
}

/*
* Return a string consisting of arguments of "str" from "start" to "end".
*/
char *
history_arg_extract(int start, int end, const char *str)
{
	size_t  i, len, max;
	char	**arr, *result;

	arr = history_tokenize(str);
	if (!arr)
		return(NULL);
	if (arr && *arr == NULL) {
		free(arr);
		return(NULL);
	}

	for (max = 0; arr[max]; max++)
		continue;
	max--;

	if (start == '$')
		start = max;
	if (end == '$')
		end = max;
	if (end < 0)
		end = max + end + 1;
	if (start < 0)
		start = end;

	if (start < 0 || end < 0 || start > max || end > max || start > end)
		return(NULL);

	for (i = start, len = 0; i <= end; i++)
		len += strlen(arr[i]) + 1;
	len++;
	result = malloc(len);
	if (result == NULL)
		return NULL;

	for (i = start, len = 0; i <= end; i++) {
		(void)strcpy(result + len, arr[i]);
		len += strlen(arr[i]);
		if (i < end)
			result[len++] = ' ';
	}
	result[len] = 0;

	for (i = 0; arr[i]; i++)
		free(arr[i]);
	free(arr);

	return(result);
}

/*
 * Parse the string into individual tokens,
 * similar to how shell would do it.
 */
char **
history_tokenize(const char *str)
{
	int size = 1, idx = 0, i, start;
	size_t len;
	char **result = NULL, *temp, delim = '\0';

	for (i = 0; str[i];) {
		while (isspace((unsigned char) str[i]))
			i++;
		start = i;
		for (; str[i];) {
			if (str[i] == '\\') {
				if (str[i+1] != '\0')
					i++;
			} else if (str[i] == delim)
				delim = '\0';
			else if (!delim &&
				    (isspace((unsigned char) str[i]) ||
				strchr("()<>;&|$", str[i])))
				break;
			else if (!delim && strchr("'`\"", str[i]))
				delim = str[i];
			if (str[i])
				i++;
		}

		if (idx + 2 >= size) {
			char **nresult;
			size <<= 1;
			nresult = realloc(result, size * sizeof(char *));
			if (nresult == NULL) {
				free(result);
				return NULL;
			}
			result = nresult;
		}
		len = i - start;
		temp = malloc(len + 1);
		if (temp == NULL) {
			for (i = 0; i < idx; i++)
				free(result[i]);
			free(result);
			return NULL;
		}
		(void)strncpy(temp, &str[start], len);
		temp[len] = '\0';
		result[idx++] = temp;
		result[idx] = NULL;
		if (str[i])
			i++;
	}
	return (result);
}


/*
 * limit size of history record to ``max'' events
 */
void
stifle_history(int max)
{
	HistEvent ev;

	if (h == NULL || e == NULL)
		rl_initialize();

	if (history(h, &ev, H_SETSIZE, max) == 0)
		max_input_history = max;
}


/*
 * "unlimit" size of history - set the limit to maximum allowed int value
 */
int
unstifle_history(void)
{
	HistEvent ev;
	int omax;

	history(h, &ev, H_SETSIZE, INT_MAX);
	omax = max_input_history;
	max_input_history = INT_MAX;
	return (omax);		/* some value _must_ be returned */
}


int
history_is_stifled(void)
{

	/* cannot return true answer */
	return (max_input_history != INT_MAX);
}


/*
 * read history from a file given
 */
int
read_history(const char *filename)
{
	HistEvent ev;

	if (h == NULL || e == NULL)
		rl_initialize();
	return (history(h, &ev, H_LOAD, filename));
}


/*
 * write history to a file given
 */
int
write_history(const char *filename)
{
	HistEvent ev;

	if (h == NULL || e == NULL)
		rl_initialize();
	return (history(h, &ev, H_SAVE, filename));
}


/*
 * returns history ``num''th event
 *
 * returned pointer points to static variable
 */
HIST_ENTRY *
history_get(int num)
{
	static HIST_ENTRY she;
	HistEvent ev;
	int curr_num;

	if (h == NULL || e == NULL)
		rl_initialize();

	/* save current position */
	if (history(h, &ev, H_CURR) != 0)
		return (NULL);
	curr_num = ev.num;

	/* start from most recent */
	if (history(h, &ev, H_FIRST) != 0)
		return (NULL);	/* error */

	/* look backwards for event matching specified offset */
	if (history(h, &ev, H_NEXT_EVENT, num))
		return (NULL);

	she.line = ev.str;
	she.data = NULL;

	/* restore pointer to where it was */
	(void)history(h, &ev, H_SET, curr_num);

	return (&she);
}


/*
 * add the line to history table
 */
int
add_history(const char *line)
{
	HistEvent ev;

	if (h == NULL || e == NULL)
		rl_initialize();

	(void)history(h, &ev, H_ENTER, line);
	if (history(h, &ev, H_GETSIZE) == 0)
		history_length = ev.num;

	return (!(history_length > 0)); /* return 0 if all is okay */
}


/*
 * clear the history list - delete all entries
 */
void
clear_history(void)
{
	HistEvent ev;

	history(h, &ev, H_CLEAR);
}


/*
 * returns offset of the current history event
 */
int
where_history(void)
{
	HistEvent ev;
	int curr_num, off;

	if (history(h, &ev, H_CURR) != 0)
		return (0);
	curr_num = ev.num;

	history(h, &ev, H_FIRST);
	off = 1;
	while (ev.num != curr_num && history(h, &ev, H_NEXT) == 0)
		off++;

	return (off);
}


/*
 * returns current history event or NULL if there is no such event
 */
HIST_ENTRY *
current_history(void)
{

	return (_move_history(H_CURR));
}


/*
 * returns total number of bytes history events' data are using
 */
int
history_total_bytes(void)
{
	HistEvent ev;
	int curr_num, size;

	if (history(h, &ev, H_CURR) != 0)
		return (-1);
	curr_num = ev.num;

	history(h, &ev, H_FIRST);
	size = 0;
	do
		size += strlen(ev.str);
	while (history(h, &ev, H_NEXT) == 0);

	/* get to the same position as before */
	history(h, &ev, H_PREV_EVENT, curr_num);

	return (size);
}


/*
 * sets the position in the history list to ``pos''
 */
int
history_set_pos(int pos)
{
	HistEvent ev;
	int curr_num;

	if (pos > history_length || pos < 0)
		return (-1);

	history(h, &ev, H_CURR);
	curr_num = ev.num;

	if (history(h, &ev, H_SET, pos)) {
		history(h, &ev, H_SET, curr_num);
		return(-1);
	}
	return (0);
}


/*
 * returns previous event in history and shifts pointer accordingly
 */
HIST_ENTRY *
previous_history(void)
{

	return (_move_history(H_PREV));
}


/*
 * returns next event in history and shifts pointer accordingly
 */
HIST_ENTRY *
next_history(void)
{

	return (_move_history(H_NEXT));
}


/*
 * searches for first history event containing the str
 */
int
history_search(const char *str, int direction)
{
	HistEvent ev;
	const char *strp;
	int curr_num;

	if (history(h, &ev, H_CURR) != 0)
		return (-1);
	curr_num = ev.num;

	for (;;) {
		if ((strp = strstr(ev.str, str)) != NULL)
			return (int) (strp - ev.str);
		if (history(h, &ev, direction < 0 ? H_NEXT:H_PREV) != 0)
			break;
	}
	history(h, &ev, H_SET, curr_num);
	return (-1);
}


/*
 * searches for first history event beginning with str
 */
int
history_search_prefix(const char *str, int direction)
{
	HistEvent ev;

	return (history(h, &ev, direction < 0? H_PREV_STR:H_NEXT_STR, str));
}


/*
 * search for event in history containing str, starting at offset
 * abs(pos); continue backward, if pos<0, forward otherwise
 */
/* ARGSUSED */
int
history_search_pos(const char *str,
		   int direction __attribute__((__unused__)), int pos)
{
	HistEvent ev;
	int curr_num, off;

	off = (pos > 0) ? pos : -pos;
	pos = (pos > 0) ? 1 : -1;

	if (history(h, &ev, H_CURR) != 0)
		return (-1);
	curr_num = ev.num;

	if (history_set_pos(off) != 0 || history(h, &ev, H_CURR) != 0)
		return (-1);


	for (;;) {
		if (strstr(ev.str, str))
			return (off);
		if (history(h, &ev, (pos < 0) ? H_PREV : H_NEXT) != 0)
			break;
	}

	/* set "current" pointer back to previous state */
	history(h, &ev, (pos < 0) ? H_NEXT_EVENT : H_PREV_EVENT, curr_num);

	return (-1);
}


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
tilde_expand(char *txt)
{
	struct passwd *pass;
	char *temp;
	size_t len = 0;

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
	pass = getpwnam(temp);
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
filename_completion_function(const char *text, int state)
{
	static DIR *dir = NULL;
	static char *filename = NULL, *dirname = NULL;
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

		/* support for ``~user'' syntax */
		if (dirname && *dirname == '~') {
			char *nptr;
			temp = tilde_expand(dirname);
			if (temp == NULL)
				return NULL;
			nptr = realloc(dirname, strlen(temp) + 1);
			if (nptr == NULL) {
				free(dirname);
				return NULL;
			}
			dirname = nptr;
			(void)strcpy(dirname, temp);	/* safe */
			free(temp);	/* no longer needed */
		}
		/* will be used in cycle */
		filename_len = filename ? strlen(filename) : 0;

		if (dir != NULL) {
			(void)closedir(dir);
			dir = NULL;
		}
		dir = opendir(dirname ? dirname : ".");
		if (!dir)
			return (NULL);	/* cannot open the directory */
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
          /* Some dirents have d_namlen, but it is not portable. */
		    && strlen(entry->d_name) >= filename_len
		    && strncmp(entry->d_name, filename,
			filename_len) == 0)
			break;
	}

	if (entry) {		/* match found */

		struct stat stbuf;
      /* Some dirents have d_namlen, but it is not portable. */
		len = strlen(entry->d_name) +
		    ((dirname) ? strlen(dirname) : 0) + 1 + 1;
		temp = malloc(len);
		if (temp == NULL)
			return NULL;
		(void)sprintf(temp, "%s%s",
		    dirname ? dirname : "", entry->d_name);	/* safe */

		/* test, if it's directory */
		if (stat(temp, &stbuf) == 0 && S_ISDIR(stbuf.st_mode))
			strcat(temp, "/");	/* safe */
	} else {
		(void)closedir(dir);
		dir = NULL;
		temp = NULL;
	}

	return (temp);
}


/*
 * a completion generator for usernames; returns _first_ username
 * which starts with supplied text
 * text contains a partial username preceded by random character
 * (usually '~'); state is ignored
 * it's callers responsibility to free returned value
 */
char *
username_completion_function(const char *text, int state)
{
	struct passwd *pwd;

	if (text[0] == '\0')
		return (NULL);

	if (*text == '~')
		text++;

	if (state == 0)
		setpwent();

	while ((pwd = getpwent()) && text[0] == pwd->pw_name[0]
	    && strcmp(text, pwd->pw_name) == 0);

	if (pwd == NULL) {
		endpwent();
		return (NULL);
	}
	return (strdup(pwd->pw_name));
}


/*
 * el-compatible wrapper around rl_complete; needed for key binding
 */
/* ARGSUSED */
static unsigned char
_el_rl_complete(EditLine *el __attribute__((__unused__)), int ch)
{
	return (unsigned char) rl_complete(0, ch);
}

/*
 * el-compatible wrapper to send TSTP on ^Z
 */
/* ARGSUSED */
static unsigned char
_el_rl_tstp(EditLine *el __attribute__((__unused__)), int ch __attribute__((__unused__)))
{
	(void)kill(0, SIGTSTP);
	return CC_NORM;
}

/*
 * returns list of completions for text given
 */
char **
completion_matches(const char *text, CPFunction *genfunc)
{
	char **match_list = NULL, *retstr, *prevstr;
	size_t match_list_len, max_equal, which, i;
	size_t matches;

	if (h == NULL || e == NULL)
		rl_initialize();

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
_rl_qsort_string_compare(i1, i2)
	const void *i1, *i2;
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
rl_display_match_list (matches, len, max)
     char **matches;
     int len, max;
{
	int i, idx, limit, count;
	int screenwidth = e->el_term.t_size.h;

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
	    _rl_qsort_string_compare);

	idx = 1;
	for(; count > 0; count--) {
		for(i = 0; i < limit && matches[idx]; i++, idx++)
			(void)fprintf(e->el_outfile, "%-*s  ", max,
			    matches[idx]);
		(void)fprintf(e->el_outfile, "\n");
	}
}

/*
 * Complete the word at or before point, called by rl_complete()
 * 'what_to_do' says what to do with the completion.
 * `?' means list the possible completions.
 * TAB means do standard completion.
 * `*' means insert all of the possible completions.
 * `!' means to do standard completion, and list all possible completions if
 * there is more than one.
 *
 * Note: '*' support is not implemented
 */
static int
_rl_complete_internal(int what_to_do)
{
	Function *complet_func;
	const LineInfo *li;
	char *temp, **matches;
	const char *ctemp;
	size_t len;

	rl_completion_type = what_to_do;

	if (h == NULL || e == NULL)
		rl_initialize();

	complet_func = rl_completion_entry_function;
	if (!complet_func)
		complet_func = (Function *)(void *)filename_completion_function;

	/* We now look backwards for the start of a filename/variable word */
	li = el_line(e);
	ctemp = (const char *) li->cursor;
	while (ctemp > li->buffer
	    && !strchr(rl_basic_word_break_characters, ctemp[-1])
	    && (!rl_special_prefixes
		|| !strchr(rl_special_prefixes, ctemp[-1]) ) )
		ctemp--;

	len = li->cursor - ctemp;
	temp = alloca(len + 1);
	(void)strncpy(temp, ctemp, len);
	temp[len] = '\0';

	/* these can be used by function called in completion_matches() */
	/* or (*rl_attempted_completion_function)() */
	_rl_update_pos();

	if (rl_attempted_completion_function) {
		int end = li->cursor - li->buffer;
		matches = (*rl_attempted_completion_function) (temp, (int)
		    (end - len), end);
	} else
		matches = 0;
	if (!rl_attempted_completion_function || !matches)
		matches = completion_matches(temp, (CPFunction *)complet_func);

	if (matches) {
		int i, retval = CC_REFRESH;
		int matches_num, maxlen, match_len, match_display=1;

		/*
		 * Only replace the completed string with common part of
		 * possible matches if there is possible completion.
		 */
		if (matches[0][0] != '\0') {
			el_deletestr(e, (int) len);
			el_insertstr(e, matches[0]);
		}

		if (what_to_do == '?')
			goto display_matches;

		if (matches[2] == NULL && strcmp(matches[0], matches[1]) == 0) {
			/*
			 * We found exact match. Add a space after
			 * it, unless we do filename completion and the
			 * object is a directory.
			 */
			size_t alen = strlen(matches[0]);
			if ((complet_func !=
			    (Function *)filename_completion_function
			      || (alen > 0 && (matches[0])[alen - 1] != '/'))
			    && rl_completion_append_character) {
				char buf[2];
				buf[0] = rl_completion_append_character;
				buf[1] = '\0';
				el_insertstr(e, buf);
			}
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
			(void)fprintf(e->el_outfile, "\n");

			/*
			 * If there are too many items, ask user for display
			 * confirmation.
			 */
			if (matches_num > rl_completion_query_items) {
				(void)fprintf(e->el_outfile,
				    "Display all %d possibilities? (y or n) ",
				    matches_num);
				(void)fflush(e->el_outfile);
				if (getc(stdin) != 'y')
					match_display = 0;
				(void)fprintf(e->el_outfile, "\n");
			}

			if (match_display)
				rl_display_match_list(matches, matches_num,
					maxlen);
			retval = CC_REDISPLAY;
		} else if (matches[0][0]) {
			/*
			 * There was some common match, but the name was
			 * not complete enough. Next tab will print possible
			 * completions.
			 */
			el_beep(e);
		} else {
			/* lcd is not a valid object - further specification */
			/* is needed */
			el_beep(e);
			retval = CC_NORM;
		}

		/* free elements of array and the array itself */
		for (i = 0; matches[i]; i++)
			free(matches[i]);
		free(matches), matches = NULL;

		return (retval);
	}
	return (CC_NORM);
}


/*
 * complete word at current point
 */
int
/*ARGSUSED*/
rl_complete(int ignore, int invoking_key)
{
	if (h == NULL || e == NULL)
		rl_initialize();

	if (rl_inhibit_completion) {
		char arr[2];
		arr[0] = (char)invoking_key;
		arr[1] = '\0';
		el_insertstr(e, arr);
		return (CC_REFRESH);
	} else if (e->el_state.lastcmd == el_rl_complete_cmdnum)
		return _rl_complete_internal('?');
	else if (_rl_complete_show_all)
		return _rl_complete_internal('!');
	else
		return _rl_complete_internal(TAB);
}


/*
 * misc other functions
 */

/*
 * bind key c to readline-type function func
 */
int
rl_bind_key(int c, int func(int, int))
{
	int retval = -1;

	if (h == NULL || e == NULL)
		rl_initialize();

	if (func == rl_insert) {
		/* XXX notice there is no range checking of ``c'' */
		e->el_map.key[c] = ED_INSERT;
		retval = 0;
	}
	return (retval);
}


/*
 * read one key from input - handles chars pushed back
 * to input stream also
 */
int
rl_read_key(void)
{
	char fooarr[2 * sizeof(int)];

	if (e == NULL || h == NULL)
		rl_initialize();

	return (el_getc(e, fooarr));
}


/*
 * reset the terminal
 */
/* ARGSUSED */
void
rl_reset_terminal(const char *p __attribute__((__unused__)))
{

	if (h == NULL || e == NULL)
		rl_initialize();
	el_reset(e);
}


/*
 * insert character ``c'' back into input stream, ``count'' times
 */
int
rl_insert(int count, int c)
{
	char arr[2];

	if (h == NULL || e == NULL)
		rl_initialize();

	/* XXX - int -> char conversion can lose on multichars */
	arr[0] = c;
	arr[1] = '\0';

	for (; count > 0; count--)
		el_push(e, arr);

	return (0);
}

/*ARGSUSED*/
int
rl_newline(int count, int c)
{
	/*
	 * Readline-4.0 appears to ignore the args.
	 */
	return rl_insert(1, '\n');
}

/*ARGSUSED*/
static unsigned char
rl_bind_wrapper(EditLine *el, unsigned char c)
{
	if (map[c] == NULL)
	    return CC_ERROR;

	_rl_update_pos();

	(*map[c])(NULL, c);

	/* If rl_done was set by the above call, deal with it here */
	if (rl_done)
		return CC_EOF;

	return CC_NORM;
}

int
rl_add_defun(const char *name, Function *fun, int c)
{
	char dest[8];
	if (c >= sizeof(map) / sizeof(map[0]) || c < 0)
		return -1;
	map[(unsigned char)c] = fun;
	el_set(e, EL_ADDFN, name, name, rl_bind_wrapper);
	vis(dest, c, VIS_WHITE|VIS_NOSLASH, 0);
	el_set(e, EL_BIND, dest, name);
	return 0;
}

void
rl_callback_read_char()
{
	int count = 0, done = 0;
	const char *buf = el_gets(e, &count);
	char *wbuf;

	if (buf == NULL || count-- <= 0)
		return;
#ifdef CTRL2 /* _AIX */
	if (count == 0 && buf[0] == CTRL2('d'))
#else
	if (count == 0 && buf[0] == CTRL('d'))
#endif
		done = 1;
	if (buf[count] == '\n' || buf[count] == '\r')
		done = 2;

	if (done && rl_linefunc != NULL) {
		el_set(e, EL_UNBUFFERED, 0);
		if (done == 2) {
		    if ((wbuf = strdup(buf)) != NULL)
			wbuf[count] = '\0';
		} else
			wbuf = NULL;
		(*(void (*)(const char *))rl_linefunc)(wbuf);
		el_set(e, EL_UNBUFFERED, 1);
	}
}

void 
rl_callback_handler_install (const char *prompt, VFunction *linefunc)
{
	if (e == NULL) {
		rl_initialize();
	}
	if (rl_prompt)
		free(rl_prompt);
	rl_prompt = prompt ? strdup(strchr(prompt, *prompt)) : NULL;
	rl_linefunc = linefunc;
	el_set(e, EL_UNBUFFERED, 1);
}   

void 
rl_callback_handler_remove(void)
{
	el_set(e, EL_UNBUFFERED, 0);
}

void
rl_redisplay(void)
{
	char a[2];
#ifdef CTRL2 /* _AIX */
	a[0] = CTRL2('r');
#else
	a[0] = CTRL('r');
#endif
	a[1] = '\0';
	el_push(e, a);
}

int
rl_get_previous_history(int count, int key)
{
	char a[2];
	a[0] = key;
	a[1] = '\0';
	while (count--)
		el_push(e, a);
	return 0;
}

void
/*ARGSUSED*/
rl_prep_terminal(int meta_flag)
{
	el_set(e, EL_PREP_TERM, 1);
}

void
rl_deprep_terminal()
{
	el_set(e, EL_PREP_TERM, 0);
}

int
rl_read_init_file(const char *s)
{
	return(el_source(e, s));
}

int
rl_parse_and_bind(const char *line)
{
	const char **argv;
	int argc;
	Tokenizer *tok;

	tok = tok_init(NULL);
	tok_str(tok, line, &argc, &argv);
	argc = el_parse(e, argc, argv);
	tok_end(tok);
	return (argc ? 1 : 0);
}

void
rl_stuff_char(int c)
{
	char buf[2];

	buf[0] = c;
	buf[1] = '\0';
	el_insertstr(e, buf);
}

static int
_rl_event_read_char(EditLine *el, char *cp)
{
	int	n, num_read = 0;

	*cp = 0;
	while (rl_event_hook) {

		(*rl_event_hook)();

#if defined(FIONREAD)
		if (ioctl(el->el_infd, FIONREAD, &n) < 0)
			return(-1);
		if (n)
			num_read = read(el->el_infd, cp, 1);
		else
			num_read = 0;
#elif defined(F_SETFL) && defined(O_NDELAY)
		if ((n = fcntl(el->el_infd, F_GETFL, 0)) < 0)
			return(-1);
		if (fcntl(el->el_infd, F_SETFL, n|O_NDELAY) < 0)
			return(-1);
		num_read = read(el->el_infd, cp, 1);
		if (fcntl(el->el_infd, F_SETFL, n))
			return(-1);
#else
		/* not non-blocking, but what you gonna do? */
		num_read = read(el->el_infd, cp, 1);
		return(-1);
#endif

		if (num_read < 0 && errno == EAGAIN)
			continue;
		if (num_read == 0)
			continue;
		break;
	}
	if (!rl_event_hook)
		el_set(el, EL_GETCFN, EL_BUILTIN_GETCFN);
	return(num_read);
}

static void
_rl_update_pos(void)
{
	const LineInfo *li = el_line(e);

	rl_point = li->cursor - li->buffer;
	rl_end = li->lastchar - li->buffer;
}
