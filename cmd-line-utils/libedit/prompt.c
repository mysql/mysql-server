/*	$NetBSD: prompt.c,v 1.9 2002/03/18 16:00:56 christos Exp $	*/

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
static char sccsid[] = "@(#)prompt.c	8.1 (Berkeley) 6/4/93";
#else
__RCSID("$NetBSD: prompt.c,v 1.9 2002/03/18 16:00:56 christos Exp $");
#endif
#endif /* not lint && not SCCSID */

/*
 * prompt.c: Prompt printing functions
 */
#include <stdio.h>
#include "el.h"

private char	*prompt_default(EditLine *);
private char	*prompt_default_r(EditLine *);

/* prompt_default():
 *	Just a default prompt, in case the user did not provide one
 */
private char *
/*ARGSUSED*/
prompt_default(EditLine *el __attribute__((unused)))
{
	static char a[3] = {'?', ' ', '\0'};

	return (a);
}


/* prompt_default_r():
 *	Just a default rprompt, in case the user did not provide one
 */
private char *
/*ARGSUSED*/
prompt_default_r(EditLine *el __attribute__((unused)))
{
	static char a[1] = {'\0'};

	return (a);
}


/* prompt_print():
 *	Print the prompt and update the prompt position.
 *	We use an array of integers in case we want to pass
 * 	literal escape sequences in the prompt and we want a
 *	bit to flag them
 */
protected void
prompt_print(EditLine *el, int op)
{
	el_prompt_t *elp;
	char *p;

	if (op == EL_PROMPT)
		elp = &el->el_prompt;
	else
		elp = &el->el_rprompt;
	p = (elp->p_func) (el);
	while (*p)
		re_putc(el, *p++, 1);

	elp->p_pos.v = el->el_refresh.r_cursor.v;
	elp->p_pos.h = el->el_refresh.r_cursor.h;
}


/* prompt_init():
 *	Initialize the prompt stuff
 */
protected int
prompt_init(EditLine *el)
{

	el->el_prompt.p_func = prompt_default;
	el->el_prompt.p_pos.v = 0;
	el->el_prompt.p_pos.h = 0;
	el->el_rprompt.p_func = prompt_default_r;
	el->el_rprompt.p_pos.v = 0;
	el->el_rprompt.p_pos.h = 0;
	return (0);
}


/* prompt_end():
 *	Clean up the prompt stuff
 */
protected void
/*ARGSUSED*/
prompt_end(EditLine *el __attribute__((unused)))
{
}


/* prompt_set():
 *	Install a prompt printing function
 */
protected int
prompt_set(EditLine *el, el_pfunc_t prf, int op)
{
	el_prompt_t *p;

	if (op == EL_PROMPT)
		p = &el->el_prompt;
	else
		p = &el->el_rprompt;
	if (prf == NULL) {
		if (op == EL_PROMPT)
			p->p_func = prompt_default;
		else
			p->p_func = prompt_default_r;
	} else
		p->p_func = prf;
	p->p_pos.v = 0;
	p->p_pos.h = 0;
	return (0);
}


/* prompt_get():
 *	Retrieve the prompt printing function
 */
protected int
prompt_get(EditLine *el, el_pfunc_t *prf, int op)
{

	if (prf == NULL)
		return (-1);
	if (op == EL_PROMPT)
		*prf = el->el_prompt.p_func;
	else
		*prf = el->el_rprompt.p_func;
	return (0);
}
