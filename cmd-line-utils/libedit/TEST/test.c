/*	$NetBSD: test.c,v 1.9 2000/09/04 23:36:41 lukem Exp $	*/

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

#include <sys/cdefs.h>
#include "compat.h"
#ifndef lint
__COPYRIGHT("@(#) Copyright (c) 1992, 1993\n\
	The Regents of the University of California.  All rights reserved.\n");
#endif /* not lint */

#if !defined(lint) && !defined(SCCSID)
#if 0
static char sccsid[] = "@(#)test.c	8.1 (Berkeley) 6/4/93";
#else
__RCSID("$NetBSD: test.c,v 1.9 2000/09/04 23:36:41 lukem Exp $");
#endif
#endif /* not lint && not SCCSID */

/*
 * test.c: A little test program
 */
#include "sys.h"
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>

#include "histedit.h"
#include "tokenizer.h"

static int continuation = 0;
static EditLine *el = NULL;

static	u_char	complete(EditLine *, int);
	int	main(int, char **);
static	char   *prompt(EditLine *);
static	void	sig(int);

static char *
prompt(EditLine *el)
{
	static char a[] = "Edit$";
	static char b[] = "Edit>";

	return (continuation ? b : a);
}

static void
sig(int i)
{

	(void) fprintf(stderr, "Got signal %d.\n", i);
	el_reset(el);
}

static unsigned char
complete(EditLine *el, int ch)
{
	DIR *dd = opendir(".");
	struct dirent *dp;
	const char* ptr;
	const LineInfo *lf = el_line(el);
	int len;

	/*
	 * Find the last word
	 */
	for (ptr = lf->cursor - 1; !isspace(*ptr) && ptr > lf->buffer; ptr--)
		continue;
	len = lf->cursor - ++ptr;

	for (dp = readdir(dd); dp != NULL; dp = readdir(dd)) {
		if (len > strlen(dp->d_name))
			continue;
		if (strncmp(dp->d_name, ptr, len) == 0) {
			closedir(dd);
			if (el_insertstr(el, &dp->d_name[len]) == -1)
				return (CC_ERROR);
			else
				return (CC_REFRESH);
		}
	}

	closedir(dd);
	return (CC_ERROR);
}

int
main(int argc, char *argv[])
{
	int num;
	const char *buf;
	Tokenizer *tok;
	int lastevent = 0, ncontinuation;
	History *hist;
	HistEvent ev;

	(void) signal(SIGINT, sig);
	(void) signal(SIGQUIT, sig);
	(void) signal(SIGHUP, sig);
	(void) signal(SIGTERM, sig);

	hist = history_init();		/* Init the builtin history	*/
					/* Remember 100 events		*/
	history(hist, &ev, H_SETSIZE, 100);

	tok  = tok_init(NULL);		/* Initialize the tokenizer	*/

					/* Initialize editline		*/
	el = el_init(*argv, stdin, stdout, stderr);

	el_set(el, EL_EDITOR, "vi");	/* Default editor is vi		*/
	el_set(el, EL_SIGNAL, 1);	/* Handle signals gracefully	*/
	el_set(el, EL_PROMPT, prompt);	/* Set the prompt function	*/

			/* Tell editline to use this history interface	*/
	el_set(el, EL_HIST, history, hist);

					/* Add a user-defined function	*/
	el_set(el, EL_ADDFN, "ed-complete", "Complete argument", complete);

					/* Bind tab to it 		*/
	el_set(el, EL_BIND, "^I", "ed-complete", NULL);

	/*
	 * Bind j, k in vi command mode to previous and next line, instead
	 * of previous and next history.
	 */
	el_set(el, EL_BIND, "-a", "k", "ed-prev-line", NULL);
	el_set(el, EL_BIND, "-a", "j", "ed-next-line", NULL);

	/*
	 * Source the user's defaults file.
	 */
	el_source(el, NULL);

	while ((buf = el_gets(el, &num)) != NULL && num != 0)  {
		int ac;
		char **av;
#ifdef DEBUG
		(void) fprintf(stderr, "got %d %s", num, buf);
#endif
		if (!continuation && num == 1)
			continue;

		if (tok_line(tok, buf, &ac, &av) > 0)
			ncontinuation = 1;

#if 0
		if (continuation) {
			/*
			 * Append to the right event in case the user
			 * moved around in history.
			 */
			if (history(hist, &ev, H_SET, lastevent) == -1)
				err(1, "%d: %s\n", lastevent, ev.str);
			history(hist, &ev, H_ADD , buf);
		} else {
			history(hist, &ev, H_ENTER, buf);
			lastevent = ev.num;
		}
#else
				/* Simpler */
		history(hist, &ev, continuation ? H_APPEND : H_ENTER, buf);
#endif

		continuation = ncontinuation;
		ncontinuation = 0;

		if (strcmp(av[0], "history") == 0) {
			int rv;

			switch (ac) {
			case 1:
				for (rv = history(hist, &ev, H_LAST); rv != -1;
				    rv = history(hist, &ev, H_PREV))
					(void) fprintf(stdout, "%4d %s",
					    ev.num, ev.str);
				break;

			case 2:
				if (strcmp(av[1], "clear") == 0)
					 history(hist, &ev, H_CLEAR);
				else
					 goto badhist;
				break;

			case 3:
				if (strcmp(av[1], "load") == 0)
					 history(hist, &ev, H_LOAD, av[2]);
				else if (strcmp(av[1], "save") == 0)
					 history(hist, &ev, H_SAVE, av[2]);
				break;

			badhist:
			default:
				(void) fprintf(stderr,
				    "Bad history arguments\n");
				break;
			}
		} else if (el_parse(el, ac, av) == -1) {
			switch (fork()) {
			case 0:
				execvp(av[0], av);
				perror(av[0]);
				_exit(1);
				/*NOTREACHED*/
				break;

			case -1:
				perror("fork");
				break;

			default:
				if (wait(&num) == -1)
					perror("wait");
				(void) fprintf(stderr, "Exit %x\n", num);
				break;
			}
		}

		tok_reset(tok);
	}

	el_end(el);
	tok_end(tok);
	history_end(hist);

	return (0);
}
