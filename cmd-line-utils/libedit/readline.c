/*	$NetBSD: readline.c,v 1.28 2003/03/10 01:14:54 christos Exp $	*/

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

#include "config.h"
#if !defined(lint) && !defined(SCCSID)
__RCSID("$NetBSD: readline.c,v 1.28 2003/03/10 01:14:54 christos Exp $");
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
#ifdef HAVE_ALLOCA_H
#include <alloca.h>
#endif
#include "histedit.h"
#include "readline/readline.h"
#include "el.h"
#include "fcns.h"		/* for EL_NUM_FCNS */

/* for rl_complete() */
#define	TAB		'\r'

/* see comment at the #ifdef for sense of this */
#define	GDB_411_HACK

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

int history_base = 1;		/* probably never subject to change */
int history_length = 0;
int max_input_history = 0;
char history_expansion_char = '!';
char history_subst_char = '^';
char *history_no_expand_chars = expand_chars;
Function *history_inhibit_expansion_function = NULL;

int rl_inhibit_completion = 0;
int rl_attempted_completion_over = 0;
char *rl_basic_word_break_characters = break_chars;
char *rl_completer_word_break_characters = NULL;
char *rl_completer_quote_characters = NULL;
CPFunction *rl_completion_entry_function = NULL;
CPPFunction *rl_attempted_completion_function = NULL;

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
static int el_rl_complete_cmdnum = 0;

/* internal functions */
static unsigned char	 _el_rl_complete(EditLine *, int);
static char		*_get_prompt(EditLine *);
static HIST_ENTRY	*_move_history(int);
static int		 _history_search_gen(const char *, int, int);
static int		 _history_expand_command(const char *, size_t, char **);
static char		*_rl_compat_sub(const char *, const char *,
			    const char *, int);
static int		 rl_complete_internal(int);
static int		 _rl_qsort_string_compare(const void *, const void *);

/*
 * needed for prompt switching in readline()
 */
static char *el_rl_prompt = NULL;


/* ARGSUSED */
static char *
_get_prompt(EditLine *el __attribute__((unused)))
{
	return (el_rl_prompt);
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
	rl_he.data = "";

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
	el_rl_prompt = strdup("");
	if (el_rl_prompt == NULL) {
		history_end(h);
		el_end(e);
		return -1;
	}
	el_set(e, EL_PROMPT, _get_prompt);
	el_set(e, EL_SIGNAL, 1);

	/* set default mode to "emacs"-style and read setting afterwards */
	/* so this can be overriden */
	el_set(e, EL_EDITOR, "emacs");

	/*
	 * Word completition - this has to go AFTER rebinding keys
	 * to emacs-style.
	 */
	el_set(e, EL_ADDFN, "rl_complete",
	    "ReadLine compatible completition function",
	    _el_rl_complete);
	el_set(e, EL_BIND, "^I", "rl_complete", NULL);

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
	rl_point = rl_end = 0;

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

	if (e == NULL || h == NULL)
		rl_initialize();

	/* update prompt accordingly to what has been passed */
	if (!prompt)
		prompt = "";
	if (strcmp(el_rl_prompt, prompt) != 0) {
		free(el_rl_prompt);
		el_rl_prompt = strdup(prompt);
		if (el_rl_prompt == NULL)
			return NULL;
	}
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
 * globally == 1, substitutes all occurences of what, otherwise only the
 * first one
 */
static char *
_rl_compat_sub(const char *str, const char *what, const char *with,
    int globally)
{
	char *result;
	const char *temp, *new;
	unsigned int len, with_len, what_len, add;
	size_t size, i;

	result = malloc((size = 16));
	if (result == NULL)
		return NULL;
	temp = str;
	with_len = strlen(with);
	what_len = strlen(what);
	len = 0;
	do {
		new = strstr(temp, what);
		if (new) {
			i = new - temp;
			add = i + with_len;
			if (i + add + 1 >= size) {
				char *nresult;
				size += add + 1;
				nresult = realloc(result, size);
				if (nresult == NULL) {
					free(result);
					return NULL;
				}
				result = nresult;
			}
			(void) strncpy(&result[len], temp, i);
			len += i;
			(void) strcpy(&result[len], with);	/* safe */
			len += with_len;
			temp = new + what_len;
		} else {
			add = strlen(temp);
			if (len + add + 1 >= size) {
				char *nresult;
				size += add + 1;
				nresult = realloc(result, size);
				if (nresult == NULL) {
					free(result);
					return NULL;
				}
				result = nresult;
			}
			(void) strcpy(&result[len], temp);	/* safe */
			len += add;
			temp = NULL;
		}
	} while (temp && globally);
	result[len] = '\0';

	return (result);
}


/*
 * the real function doing history expansion - takes as argument command
 * to do and data upon which the command should be executed
 * does expansion the way I've understood readline documentation
 * word designator ``%'' isn't supported (yet ?)
 *
 * returns 0 if data was not modified, 1 if it was and 2 if the string
 * should be only printed and not executed; in case of error,
 * returns -1 and *result points to NULL
 * it's callers responsibility to free() string returned in *result
 */
static int
_history_expand_command(const char *command, size_t cmdlen, char **result)
{
	char **arr, *tempcmd, *line, *search = NULL, *cmd;
	const char *event_data = NULL;
	static char *from = NULL, *to = NULL;
	int start = -1, end = -1, max, i, idx;
	int h_on = 0, t_on = 0, r_on = 0, e_on = 0, p_on = 0, g_on = 0;
	int event_num = 0, retval;
	size_t cmdsize;

	*result = NULL;

	cmd = alloca(cmdlen + 1);
	(void) strncpy(cmd, command, cmdlen);
	cmd[cmdlen] = 0;

	idx = 1;
	/* find out which event to take */
	if (cmd[idx] == history_expansion_char) {
		event_num = history_length;
		idx++;
	} else {
		int off, num;
		size_t len;
		off = idx;
		while (cmd[off] && !strchr(":^$*-%", cmd[off]))
			off++;
		num = atoi(&cmd[idx]);
		if (num != 0) {
			event_num = num;
			if (num < 0)
				event_num += history_length + 1;
		} else {
			int prefix = 1, curr_num;
			HistEvent ev;

			len = off - idx;
			if (cmd[idx] == '?') {
				idx++, len--;
				if (cmd[off - 1] == '?')
					len--;
				else if (cmd[off] != '\n' && cmd[off] != '\0')
					return (-1);
				prefix = 0;
			}
			search = alloca(len + 1);
			(void) strncpy(search, &cmd[idx], len);
			search[len] = '\0';

			if (history(h, &ev, H_CURR) != 0)
				return (-1);
			curr_num = ev.num;

			if (prefix)
				retval = history_search_prefix(search, -1);
			else
				retval = history_search(search, -1);

			if (retval == -1) {
				fprintf(rl_outstream, "%s: Event not found\n",
				    search);
				return (-1);
			}
			if (history(h, &ev, H_CURR) != 0)
				return (-1);
			event_data = ev.str;

			/* roll back to original position */
			history(h, &ev, H_NEXT_EVENT, curr_num);
		}
		idx = off;
	}

	if (!event_data && event_num >= 0) {
		HIST_ENTRY *rl_he;
		rl_he = history_get(event_num);
		if (!rl_he)
			return (0);
		event_data = rl_he->line;
	} else
		return (-1);

	if (cmd[idx] != ':')
		return (-1);
	cmd += idx + 1;

	/* recognize cmd */
	if (*cmd == '^')
		start = end = 1, cmd++;
	else if (*cmd == '$')
		start = end = -1, cmd++;
	else if (*cmd == '*')
		start = 1, end = -1, cmd++;
	else if (isdigit((unsigned char) *cmd)) {
		const char *temp;
		int shifted = 0;

		start = atoi(cmd);
		temp = cmd;
		for (; isdigit((unsigned char) *cmd); cmd++);
		if (temp != cmd)
			shifted = 1;
		if (shifted && *cmd == '-') {
			if (!isdigit((unsigned char) *(cmd + 1)))
				end = -2;
			else {
				end = atoi(cmd + 1);
				for (; isdigit((unsigned char) *cmd); cmd++);
			}
		} else if (shifted && *cmd == '*')
			end = -1, cmd++;
		else if (shifted)
			end = start;
	}
	if (*cmd == ':')
		cmd++;

	line = strdup(event_data);
	if (line == NULL)
		return 0;
	for (; *cmd; cmd++) {
		if (*cmd == ':')
			continue;
		else if (*cmd == 'h')
			h_on = 1 | g_on, g_on = 0;
		else if (*cmd == 't')
			t_on = 1 | g_on, g_on = 0;
		else if (*cmd == 'r')
			r_on = 1 | g_on, g_on = 0;
		else if (*cmd == 'e')
			e_on = 1 | g_on, g_on = 0;
		else if (*cmd == 'p')
			p_on = 1 | g_on, g_on = 0;
		else if (*cmd == 'g')
			g_on = 2;
		else if (*cmd == 's' || *cmd == '&') {
			char *what, *with, delim;
			unsigned int len, from_len;
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
					if (*cmd == '\\'
					    && *(cmd + 1) == delim)
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
						(void) strcpy(&with[len], from);
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

				tempcmd = _rl_compat_sub(line, from, to,
				    (g_on) ? 1 : 0);
				if (tempcmd) {
					free(line);
					line = tempcmd;
				}
				g_on = 0;
			}
		}
	}

	arr = history_tokenize(line);
	free(line);		/* no more needed */
	if (arr && *arr == NULL)
		free(arr), arr = NULL;
	if (!arr)
		return (-1);

	/* find out max valid idx to array of array */
	max = 0;
	for (i = 0; arr[i]; i++)
		max++;
	max--;

	/* set boundaries to something relevant */
	if (start < 0)
		start = 1;
	if (end < 0)
		end = max - ((end < -1) ? 1 : 0);

	/* check boundaries ... */
	if (start > max || end > max || start > end)
		return (-1);

	for (i = 0; i <= max; i++) {
		char *temp;
		if (h_on && (i == 1 || h_on > 1) &&
		    (temp = strrchr(arr[i], '/')))
			*(temp + 1) = '\0';
		if (t_on && (i == 1 || t_on > 1) &&
		    (temp = strrchr(arr[i], '/')))
			(void) strcpy(arr[i], temp + 1);
		if (r_on && (i == 1 || r_on > 1) &&
		    (temp = strrchr(arr[i], '.')))
			*temp = '\0';
		if (e_on && (i == 1 || e_on > 1) &&
		    (temp = strrchr(arr[i], '.')))
			(void) strcpy(arr[i], temp);
	}

	cmdsize = 1, cmdlen = 0;
	if ((tempcmd = malloc(cmdsize)) == NULL)
		return 0;
	for (i = start; start <= i && i <= end; i++) {
		int arr_len;

		arr_len = strlen(arr[i]);
		if (cmdlen + arr_len + 1 >= cmdsize) {
			char *ntempcmd;
			cmdsize += arr_len + 1;
			ntempcmd = realloc(tempcmd, cmdsize);
			if (ntempcmd == NULL) {
				free(tempcmd);
				return 0;
			}
			tempcmd = ntempcmd;
		}
		(void) strcpy(&tempcmd[cmdlen], arr[i]);	/* safe */
		cmdlen += arr_len;
		tempcmd[cmdlen++] = ' ';	/* add a space */
	}
	while (cmdlen > 0 && isspace((unsigned char) tempcmd[cmdlen - 1]))
		cmdlen--;
	tempcmd[cmdlen] = '\0';

	*result = tempcmd;

	for (i = 0; i <= max; i++)
		free(arr[i]);
	free(arr), arr = (char **) NULL;
	return (p_on) ? 2 : 1;
}


/*
 * csh-style history expansion
 */
int
history_expand(char *str, char **output)
{
	int i, retval = 0, idx;
	size_t size;
	char *temp, *result;

	if (h == NULL || e == NULL)
		rl_initialize();

	*output = strdup(str);	/* do it early */
	if (*output == NULL)
		return 0;

	if (str[0] == history_subst_char) {
		/* ^foo^foo2^ is equivalent to !!:s^foo^foo2^ */
		temp = alloca(4 + strlen(str) + 1);
		temp[0] = temp[1] = history_expansion_char;
		temp[2] = ':';
		temp[3] = 's';
		(void) strcpy(temp + 4, str);
		str = temp;
	}
#define	ADD_STRING(what, len) 						\
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
		int start, j, loop_again;
		size_t len;

		loop_again = 1;
		start = j = i;
loop:
		for (; str[j]; j++) {
			if (str[j] == '\\' &&
			    str[j + 1] == history_expansion_char) {
				(void) strcpy(&str[j], &str[j + 1]);
				continue;
			}
			if (!loop_again) {
				if (str[j] == '?') {
					while (str[j] && str[++j] != '?');
					if (str[j] == '?')
						j++;
				} else if (isspace((unsigned char) str[j]))
					break;
			}
			if (str[j] == history_expansion_char
			    && !strchr(history_no_expand_chars, str[j + 1])
			    && (!history_inhibit_expansion_function ||
			    (*history_inhibit_expansion_function)(str, j) == 0))
				break;
		}

		if (str[j] && str[j + 1] != '#' && loop_again) {
			i = j;
			j++;
			if (str[j] == history_expansion_char)
				j++;
			loop_again = 0;
			goto loop;
		}
		len = i - start;
		temp = &str[start];
		ADD_STRING(temp, len);

		if (str[i] == '\0' || str[i] != history_expansion_char
		    || str[i + 1] == '#') {
			len = j - i;
			temp = &str[i];
			ADD_STRING(temp, len);
			if (start == 0)
				retval = 0;
			else
				retval = 1;
			break;
		}
		retval = _history_expand_command(&str[i], (size_t) (j - i),
		    &temp);
		if (retval != -1) {
			len = strlen(temp);
			ADD_STRING(temp, len);
		}
		i = j;
	}			/* for(i ...) */

	if (retval == 2) {
		add_history(temp);
#ifdef GDB_411_HACK
		/* gdb 4.11 has been shipped with readline, where */
		/* history_expand() returned -1 when the line	  */
		/* should not be executed; in readline 2.1+	  */
		/* it should return 2 in such a case		  */
		retval = -1;
#endif
	}
	free(*output);
	*output = result;

	return (retval);
}


/*
 * Parse the string into individual tokens, similarily to how shell would do it.
 */
char **
history_tokenize(const char *str)
{
	int size = 1, result_idx = 0, i, start;
	size_t len;
	char **result = NULL, *temp, delim = '\0';

	for (i = 0; str[i]; i++) {
		while (isspace((unsigned char) str[i]))
			i++;
		start = i;
		for (; str[i]; i++) {
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
		}

		if (result_idx + 2 >= size) {
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
			free(result);
			return NULL;
		}
		(void) strncpy(temp, &str[start], len);
		temp[len] = '\0';
		result[result_idx++] = temp;
		result[result_idx] = NULL;
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
	int i = 1, curr_num;

	if (h == NULL || e == NULL)
		rl_initialize();

	/* rewind to beginning */
	if (history(h, &ev, H_CURR) != 0)
		return (NULL);
	curr_num = ev.num;
	if (history(h, &ev, H_LAST) != 0)
		return (NULL);	/* error */
	while (i < num && history(h, &ev, H_PREV) == 0)
		i++;
	if (i != num)
		return (NULL);	/* not so many entries */

	she.line = ev.str;
	she.data = NULL;

	/* rewind history to the same event it was before */
	(void) history(h, &ev, H_FIRST);
	(void) history(h, &ev, H_NEXT_EVENT, curr_num);

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

	(void) history(h, &ev, H_ENTER, line);
	if (history(h, &ev, H_GETSIZE) == 0)
		history_length = ev.num;

	return (!(history_length > 0));	/* return 0 if all is okay */
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
	int off, curr_num;

	if (pos > history_length || pos < 0)
		return (-1);

	history(h, &ev, H_CURR);
	curr_num = ev.num;
	history(h, &ev, H_FIRST);
	off = 0;
	while (off < pos && history(h, &ev, H_NEXT) == 0)
		off++;

	if (off != pos) {	/* do a rollback in case of error */
		history(h, &ev, H_FIRST);
		history(h, &ev, H_NEXT_EVENT, curr_num);
		return (-1);
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
 * generic history search function
 */
static int
_history_search_gen(const char *str, int direction, int pos)
{
	HistEvent ev;
	const char *strp;
	int curr_num;

	if (history(h, &ev, H_CURR) != 0)
		return (-1);
	curr_num = ev.num;

	for (;;) {
		strp = strstr(ev.str, str);
		if (strp && (pos < 0 || &ev.str[pos] == strp))
			return (int) (strp - ev.str);
		if (history(h, &ev, direction < 0 ? H_PREV : H_NEXT) != 0)
			break;
	}

	history(h, &ev, direction < 0 ? H_NEXT_EVENT : H_PREV_EVENT, curr_num);

	return (-1);
}


/*
 * searches for first history event containing the str
 */
int
history_search(const char *str, int direction)
{

	return (_history_search_gen(str, direction, -1));
}


/*
 * searches for first history event beginning with str
 */
int
history_search_prefix(const char *str, int direction)
{

	return (_history_search_gen(str, direction, 0));
}


/*
 * search for event in history containing str, starting at offset
 * abs(pos); continue backward, if pos<0, forward otherwise
 */
/* ARGSUSED */
int
history_search_pos(const char *str, 
		   int direction __attribute__((unused)), int pos)
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
/* completition functions	*/

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
		(void) strncpy(temp, txt + 1, len - 2);
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
	(void) sprintf(temp, "%s/%s", pass->pw_dir, txt);

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
			(void) strcpy(filename, temp);
			len = temp - text;	/* including last slash */
			nptr = realloc(dirname, len + 1);
			if (nptr == NULL) {
				free(filename);
				return NULL;
			}
			dirname = nptr;
			(void) strncpy(dirname, text, len);
			dirname[len] = '\0';
		} else {
			filename = strdup(text);
			if (filename == NULL)
				return NULL;
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
			(void) strcpy(dirname, temp);	/* safe */
			free(temp);	/* no longer needed */
		}
		/* will be used in cycle */
		filename_len = strlen(filename);
		if (filename_len == 0)
			return (NULL);	/* no expansion possible */

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
		/* otherwise, get first entry where first */
		/* filename_len characters are equal	  */
		if (entry->d_name[0] == filename[0]
#ifndef STRUCT_DIRENT_HAS_D_NAMLEN
		    && strlen(entry->d_name) >= filename_len
#else
		    && entry->d_namlen >= filename_len
#endif
		    && strncmp(entry->d_name, filename,
			filename_len) == 0)
			break;
	}

	if (entry) {		/* match found */

		struct stat stbuf;
#ifndef STRUCT_DIRENT_HAS_D_NAMLEN
		len = strlen(entry->d_name) +
#else
		len = entry->d_namlen +
#endif
		    ((dirname) ? strlen(dirname) : 0) + 1 + 1;
		temp = malloc(len);
		if (temp == NULL)
			return NULL;
		(void) sprintf(temp, "%s%s",
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
_el_rl_complete(EditLine *el __attribute__((unused)), int ch)
{
	return (unsigned char) rl_complete(0, ch);
}


/*
 * returns list of completitions for text given
 */
char **
completion_matches(const char *text, CPFunction *genfunc)
{
	char **match_list = NULL, *retstr, *prevstr;
	size_t match_list_len, max_equal, which, i;
	unsigned int matches;

	if (h == NULL || e == NULL)
		rl_initialize();

	matches = 0;
	match_list_len = 1;
	while ((retstr = (*genfunc) (text, matches)) != NULL) {
		/* allow for list terminator here */
		if (matches + 2 >= match_list_len) {
			char **nmatch_list;
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
	(void) strncpy(retstr, match_list[1], max_equal);
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
		for(i=0; i < limit && matches[idx]; i++, idx++)
			fprintf(e->el_outfile, "%-*s  ", max, matches[idx]);
		fprintf(e->el_outfile, "\n");
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
rl_complete_internal(int what_to_do)
{
	CPFunction *complet_func;
	const LineInfo *li;
	char *temp, **matches;
	const char *ctemp;
	size_t len;

	rl_completion_type = what_to_do;

	if (h == NULL || e == NULL)
		rl_initialize();

	complet_func = rl_completion_entry_function;
	if (!complet_func)
		complet_func = filename_completion_function;

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
	(void) strncpy(temp, ctemp, len);
	temp[len] = '\0';

	/* these can be used by function called in completion_matches() */
	/* or (*rl_attempted_completion_function)() */
	rl_point = li->cursor - li->buffer;
	rl_end = li->lastchar - li->buffer;

	if (!rl_attempted_completion_function)
		matches = completion_matches(temp, complet_func);
	else {
		int end = li->cursor - li->buffer;
		matches = (*rl_attempted_completion_function) (temp, (int)
		    (end - len), end);
	}

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
			 * it, unless we do filename completition and the
			 * object is a directory.
			 */
			size_t alen = strlen(matches[0]);
			if ((complet_func != filename_completion_function
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
			fprintf(e->el_outfile, "\n");

			/*
			 * If there are too many items, ask user for display
			 * confirmation.
			 */
			if (matches_num > rl_completion_query_items) {
				fprintf(e->el_outfile,
				"Display all %d possibilities? (y or n) ",
					matches_num);
				fflush(e->el_outfile);
				if (getc(stdin) != 'y')
					match_display = 0;
				fprintf(e->el_outfile, "\n");
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
rl_complete(int ignore, int invoking_key)
{
	if (h == NULL || e == NULL)
		rl_initialize();

	if (rl_inhibit_completion) {
		rl_insert(ignore, invoking_key);
		return (CC_REFRESH);
	} else if (e->el_state.lastcmd == el_rl_complete_cmdnum)
		return rl_complete_internal('?');
	else if (_rl_complete_show_all)
		return rl_complete_internal('!');
	else
		return (rl_complete_internal(TAB));
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
rl_reset_terminal(const char *p __attribute__((unused)))
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
