/*	$NetBSD: parse.c,v 1.16 2003/01/21 18:40:24 christos Exp $	*/

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
static char sccsid[] = "@(#)parse.c	8.1 (Berkeley) 6/4/93";
#else
__RCSID("$NetBSD: parse.c,v 1.16 2003/01/21 18:40:24 christos Exp $");
#endif
#endif /* not lint && not SCCSID */

/*
 * parse.c: parse an editline extended command
 *
 * commands are:
 *
 *	bind
 *	echotc
 *	edit
 *	gettc
 *	history
 *	settc
 *	setty
 */
#include "el.h"
#include "tokenizer.h"
#include <stdlib.h>

private const struct {
	const char *name;
	int (*func)(EditLine *, int, const char **);
} cmds[] = {
	{ "bind",	map_bind	},
	{ "echotc",	term_echotc	},
	{ "edit",	el_editmode	},
	{ "history",	hist_command	},
	{ "telltc",	term_telltc	},
	{ "settc",	term_settc	},
	{ "setty",	tty_stty	},
	{ NULL,		NULL		}
};


/* parse_line():
 *	Parse a line and dispatch it
 */
protected int
parse_line(EditLine *el, const char *line)
{
	const char **argv;
	int argc;
	Tokenizer *tok;

	tok = tok_init(NULL);
	tok_line(tok, line, &argc, &argv);
	argc = el_parse(el, argc, argv);
	tok_end(tok);
	return (argc);
}


/* el_parse():
 *	Command dispatcher
 */
public int
el_parse(EditLine *el, int argc, const char *argv[])
{
	const char *ptr;
	int i;

	if (argc < 1)
		return (-1);
	ptr = strchr(argv[0], ':');
	if (ptr != NULL) {
		char *tprog;
		size_t l;

		if (ptr == argv[0])
			return (0);
		l = ptr - argv[0] - 1;
		tprog = (char *) el_malloc(l + 1);
		if (tprog == NULL)
			return (0);
		(void) strncpy(tprog, argv[0], l);
		tprog[l] = '\0';
		ptr++;
		l = el_match(el->el_prog, tprog);
		el_free(tprog);
		if (!l)
			return (0);
	} else
		ptr = argv[0];

	for (i = 0; cmds[i].name != NULL; i++)
		if (strcmp(cmds[i].name, ptr) == 0) {
			i = (*cmds[i].func) (el, argc, argv);
			return (-i);
		}
	return (-1);
}


/* parse__escape():
 *	Parse a string of the form ^<char> \<odigit> \<char> and return
 *	the appropriate character or -1 if the escape is not valid
 */
protected int
parse__escape(const char **const ptr)
{
	const char *p;
	int c;

	p = *ptr;

	if (p[1] == 0)
		return (-1);

	if (*p == '\\') {
		p++;
		switch (*p) {
		case 'a':
			c = '\007';	/* Bell */
			break;
		case 'b':
			c = '\010';	/* Backspace */
			break;
		case 't':
			c = '\011';	/* Horizontal Tab */
			break;
		case 'n':
			c = '\012';	/* New Line */
			break;
		case 'v':
			c = '\013';	/* Vertical Tab */
			break;
		case 'f':
			c = '\014';	/* Form Feed */
			break;
		case 'r':
			c = '\015';	/* Carriage Return */
			break;
		case 'e':
			c = '\033';	/* Escape */
			break;
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		{
			int cnt, ch;

			for (cnt = 0, c = 0; cnt < 3; cnt++) {
				ch = *p++;
				if (ch < '0' || ch > '7') {
					p--;
					break;
				}
				c = (c << 3) | (ch - '0');
			}
			if ((c & 0xffffff00) != 0)
				return (-1);
			--p;
			break;
		}
		default:
			c = *p;
			break;
		}
	} else if (*p == '^' && isalpha((unsigned char) p[1])) {
		p++;
		c = (*p == '?') ? '\177' : (*p & 0237);
	} else
		c = *p;
	*ptr = ++p;
	return (c);
}
/* parse__string():
 *	Parse the escapes from in and put the raw string out
 */
protected char *
parse__string(char *out, const char *in)
{
	char *rv = out;
	int n;

	for (;;)
		switch (*in) {
		case '\0':
			*out = '\0';
			return (rv);

		case '\\':
		case '^':
			if ((n = parse__escape(&in)) == -1)
				return (NULL);
			*out++ = n;
			break;

		default:
			*out++ = *in++;
			break;
		}
}


/* parse_cmd():
 *	Return the command number for the command string given
 *	or -1 if one is not found
 */
protected int
parse_cmd(EditLine *el, const char *cmd)
{
	el_bindings_t *b;

	for (b = el->el_map.help; b->name != NULL; b++)
		if (strcmp(b->name, cmd) == 0)
			return (b->func);
	return (-1);
}
