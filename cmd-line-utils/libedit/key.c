/*	$NetBSD: key.c,v 1.13 2002/03/18 16:00:55 christos Exp $	*/

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
static char sccsid[] = "@(#)key.c	8.1 (Berkeley) 6/4/93";
#else
__RCSID("$NetBSD: key.c,v 1.13 2002/03/18 16:00:55 christos Exp $");
#endif
#endif /* not lint && not SCCSID */

/*
 * key.c: This module contains the procedures for maintaining
 *	  the extended-key map.
 *
 *      An extended-key (key) is a sequence of keystrokes introduced
 *	with an sequence introducer and consisting of an arbitrary
 *	number of characters.  This module maintains a map (the el->el_key.map)
 *	to convert these extended-key sequences into input strs
 *	(XK_STR), editor functions (XK_CMD), or unix commands (XK_EXE).
 *
 *      Warning:
 *	  If key is a substr of some other keys, then the longer
 *	  keys are lost!!  That is, if the keys "abcd" and "abcef"
 *	  are in el->el_key.map, adding the key "abc" will cause the first two
 *	  definitions to be lost.
 *
 *      Restrictions:
 *      -------------
 *      1) It is not possible to have one key that is a
 *	   substr of another.
 */
#include <string.h>
#include <stdlib.h>

#include "el.h"

/*
 * The Nodes of the el->el_key.map.  The el->el_key.map is a linked list
 * of these node elements
 */
struct key_node_t {
	char		ch;		/* single character of key 	 */
	int		type;		/* node type			 */
	key_value_t	val;		/* command code or pointer to str,  */
					/* if this is a leaf 		 */
	struct key_node_t *next;	/* ptr to next char of this key  */
	struct key_node_t *sibling;	/* ptr to another key with same prefix*/
};

private int		 node_trav(EditLine *, key_node_t *, char *,
    key_value_t *);
private int		 node__try(EditLine *, key_node_t *, const char *,
    key_value_t *, int);
private key_node_t	*node__get(int);
private void		 node__put(EditLine *, key_node_t *);
private int		 node__delete(EditLine *, key_node_t **, const char *);
private int		 node_lookup(EditLine *, const char *, key_node_t *,
    int);
private int		 node_enum(EditLine *, key_node_t *, int);
private int		 key__decode_char(char *, int, int);

#define	KEY_BUFSIZ	EL_BUFSIZ


/* key_init():
 *	Initialize the key maps
 */
protected int
key_init(EditLine *el)
{

	el->el_key.buf = (char *) el_malloc(KEY_BUFSIZ);
	if (el->el_key.buf == NULL)
		return (-1);
	el->el_key.map = NULL;
	key_reset(el);
	return (0);
}


/* key_end():
 *	Free the key maps
 */
protected void
key_end(EditLine *el)
{

	el_free((ptr_t) el->el_key.buf);
	el->el_key.buf = NULL;
	/* XXX: provide a function to clear the keys */
	el->el_key.map = NULL;
}


/* key_map_cmd():
 *	Associate cmd with a key value
 */
protected key_value_t *
key_map_cmd(EditLine *el, int cmd)
{

	el->el_key.val.cmd = (el_action_t) cmd;
	return (&el->el_key.val);
}


/* key_map_str():
 *	Associate str with a key value
 */
protected key_value_t *
key_map_str(EditLine *el, char *str)
{

	el->el_key.val.str = str;
	return (&el->el_key.val);
}


/* key_reset():
 *	Takes all nodes on el->el_key.map and puts them on free list.  Then
 *	initializes el->el_key.map with arrow keys
 *	[Always bind the ansi arrow keys?]
 */
protected void
key_reset(EditLine *el)
{

	node__put(el, el->el_key.map);
	el->el_key.map = NULL;
	return;
}


/* key_get():
 *	Calls the recursive function with entry point el->el_key.map
 *      Looks up *ch in map and then reads characters until a
 *      complete match is found or a mismatch occurs. Returns the
 *      type of the match found (XK_STR, XK_CMD, or XK_EXE).
 *      Returns NULL in val.str and XK_STR for no match.
 *      The last character read is returned in *ch.
 */
protected int
key_get(EditLine *el, char *ch, key_value_t *val)
{

	return (node_trav(el, el->el_key.map, ch, val));
}


/* key_add():
 *      Adds key to the el->el_key.map and associates the value in val with it.
 *      If key is already is in el->el_key.map, the new code is applied to the
 *      existing key. Ntype specifies if code is a command, an
 *      out str or a unix command.
 */
protected void
key_add(EditLine *el, const char *key, key_value_t *val, int ntype)
{

	if (key[0] == '\0') {
		(void) fprintf(el->el_errfile,
		    "key_add: Null extended-key not allowed.\n");
		return;
	}
	if (ntype == XK_CMD && val->cmd == ED_SEQUENCE_LEAD_IN) {
		(void) fprintf(el->el_errfile,
		    "key_add: sequence-lead-in command not allowed\n");
		return;
	}
	if (el->el_key.map == NULL)
		/* tree is initially empty.  Set up new node to match key[0] */
		el->el_key.map = node__get(key[0]);
			/* it is properly initialized */

	/* Now recurse through el->el_key.map */
	(void) node__try(el, el->el_key.map, key, val, ntype);
	return;
}


/* key_clear():
 *
 */
protected void
key_clear(EditLine *el, el_action_t *map, const char *in)
{

	if ((map[(unsigned char)*in] == ED_SEQUENCE_LEAD_IN) &&
	    ((map == el->el_map.key &&
	    el->el_map.alt[(unsigned char)*in] != ED_SEQUENCE_LEAD_IN) ||
	    (map == el->el_map.alt &&
	    el->el_map.key[(unsigned char)*in] != ED_SEQUENCE_LEAD_IN)))
		(void) key_delete(el, in);
}


/* key_delete():
 *      Delete the key and all longer keys staring with key, if
 *      they exists.
 */
protected int
key_delete(EditLine *el, const char *key)
{

	if (key[0] == '\0') {
		(void) fprintf(el->el_errfile,
		    "key_delete: Null extended-key not allowed.\n");
		return (-1);
	}
	if (el->el_key.map == NULL)
		return (0);

	(void) node__delete(el, &el->el_key.map, key);
	return (0);
}


/* key_print():
 *	Print the binding associated with key key.
 *	Print entire el->el_key.map if null
 */
protected void
key_print(EditLine *el, const char *key)
{

	/* do nothing if el->el_key.map is empty and null key specified */
	if (el->el_key.map == NULL && *key == 0)
		return;

	el->el_key.buf[0] = '"';
	if (node_lookup(el, key, el->el_key.map, 1) <= -1)
		/* key is not bound */
		(void) fprintf(el->el_errfile, "Unbound extended key \"%s\"\n",
		    key);
	return;
}


/* node_trav():
 *	recursively traverses node in tree until match or mismatch is
 * 	found.  May read in more characters.
 */
private int
node_trav(EditLine *el, key_node_t *ptr, char *ch, key_value_t *val)
{

	if (ptr->ch == *ch) {
		/* match found */
		if (ptr->next) {
			/* key not complete so get next char */
			if (el_getc(el, ch) != 1) {	/* if EOF or error */
				val->cmd = ED_END_OF_FILE;
				return (XK_CMD);
				/* PWP: Pretend we just read an end-of-file */
			}
			return (node_trav(el, ptr->next, ch, val));
		} else {
			*val = ptr->val;
			if (ptr->type != XK_CMD)
				*ch = '\0';
			return (ptr->type);
		}
	} else {
		/* no match found here */
		if (ptr->sibling) {
			/* try next sibling */
			return (node_trav(el, ptr->sibling, ch, val));
		} else {
			/* no next sibling -- mismatch */
			val->str = NULL;
			return (XK_STR);
		}
	}
}


/* node__try():
 * 	Find a node that matches *str or allocate a new one
 */
private int
node__try(EditLine *el, key_node_t *ptr, const char *str, key_value_t *val, int ntype)
{

	if (ptr->ch != *str) {
		key_node_t *xm;

		for (xm = ptr; xm->sibling != NULL; xm = xm->sibling)
			if (xm->sibling->ch == *str)
				break;
		if (xm->sibling == NULL)
			xm->sibling = node__get(*str);	/* setup new node */
		ptr = xm->sibling;
	}
	if (*++str == '\0') {
		/* we're there */
		if (ptr->next != NULL) {
			node__put(el, ptr->next);
				/* lose longer keys with this prefix */
			ptr->next = NULL;
		}
		switch (ptr->type) {
		case XK_CMD:
		case XK_NOD:
			break;
		case XK_STR:
		case XK_EXE:
			if (ptr->val.str)
				el_free((ptr_t) ptr->val.str);
			break;
		default:
			EL_ABORT((el->el_errfile, "Bad XK_ type %d\n",
			    ptr->type));
			break;
		}

		switch (ptr->type = ntype) {
		case XK_CMD:
			ptr->val = *val;
			break;
		case XK_STR:
		case XK_EXE:
			ptr->val.str = strdup(val->str);
			break;
		default:
			EL_ABORT((el->el_errfile, "Bad XK_ type %d\n", ntype));
			break;
		}
	} else {
		/* still more chars to go */
		if (ptr->next == NULL)
			ptr->next = node__get(*str);	/* setup new node */
		(void) node__try(el, ptr->next, str, val, ntype);
	}
	return (0);
}


/* node__delete():
 *	Delete node that matches str
 */
private int
node__delete(EditLine *el, key_node_t **inptr, const char *str)
{
	key_node_t *ptr;
	key_node_t *prev_ptr = NULL;

	ptr = *inptr;

	if (ptr->ch != *str) {
		key_node_t *xm;

		for (xm = ptr; xm->sibling != NULL; xm = xm->sibling)
			if (xm->sibling->ch == *str)
				break;
		if (xm->sibling == NULL)
			return (0);
		prev_ptr = xm;
		ptr = xm->sibling;
	}
	if (*++str == '\0') {
		/* we're there */
		if (prev_ptr == NULL)
			*inptr = ptr->sibling;
		else
			prev_ptr->sibling = ptr->sibling;
		ptr->sibling = NULL;
		node__put(el, ptr);
		return (1);
	} else if (ptr->next != NULL &&
	    node__delete(el, &ptr->next, str) == 1) {
		if (ptr->next != NULL)
			return (0);
		if (prev_ptr == NULL)
			*inptr = ptr->sibling;
		else
			prev_ptr->sibling = ptr->sibling;
		ptr->sibling = NULL;
		node__put(el, ptr);
		return (1);
	} else {
		return (0);
	}
}


/* node__put():
 *	Puts a tree of nodes onto free list using free(3).
 */
private void
node__put(EditLine *el, key_node_t *ptr)
{
	if (ptr == NULL)
		return;

	if (ptr->next != NULL) {
		node__put(el, ptr->next);
		ptr->next = NULL;
	}
	node__put(el, ptr->sibling);

	switch (ptr->type) {
	case XK_CMD:
	case XK_NOD:
		break;
	case XK_EXE:
	case XK_STR:
		if (ptr->val.str != NULL)
			el_free((ptr_t) ptr->val.str);
		break;
	default:
		EL_ABORT((el->el_errfile, "Bad XK_ type %d\n", ptr->type));
		break;
	}
	el_free((ptr_t) ptr);
}


/* node__get():
 *	Returns pointer to an key_node_t for ch.
 */
private key_node_t *
node__get(int ch)
{
	key_node_t *ptr;

	ptr = (key_node_t *) el_malloc((size_t) sizeof(key_node_t));
	if (ptr == NULL)
		return NULL;
	ptr->ch = ch;
	ptr->type = XK_NOD;
	ptr->val.str = NULL;
	ptr->next = NULL;
	ptr->sibling = NULL;
	return (ptr);
}



/* node_lookup():
 *	look for the str starting at node ptr.
 *	Print if last node
 */
private int
node_lookup(EditLine *el, const char *str, key_node_t *ptr, int cnt)
{
	int ncnt;

	if (ptr == NULL)
		return (-1);	/* cannot have null ptr */

	if (*str == 0) {
		/* no more chars in str.  node_enum from here. */
		(void) node_enum(el, ptr, cnt);
		return (0);
	} else {
		/* If match put this char into el->el_key.buf.  Recurse */
		if (ptr->ch == *str) {
			/* match found */
			ncnt = key__decode_char(el->el_key.buf, cnt,
			    (unsigned char) ptr->ch);
			if (ptr->next != NULL)
				/* not yet at leaf */
				return (node_lookup(el, str + 1, ptr->next,
				    ncnt + 1));
			else {
			    /* next node is null so key should be complete */
				if (str[1] == 0) {
					el->el_key.buf[ncnt + 1] = '"';
					el->el_key.buf[ncnt + 2] = '\0';
					key_kprint(el, el->el_key.buf,
					    &ptr->val, ptr->type);
					return (0);
				} else
					return (-1);
					/* mismatch -- str still has chars */
			}
		} else {
			/* no match found try sibling */
			if (ptr->sibling)
				return (node_lookup(el, str, ptr->sibling,
				    cnt));
			else
				return (-1);
		}
	}
}


/* node_enum():
 *	Traverse the node printing the characters it is bound in buffer
 */
private int
node_enum(EditLine *el, key_node_t *ptr, int cnt)
{
	int ncnt;

	if (cnt >= KEY_BUFSIZ - 5) {	/* buffer too small */
		el->el_key.buf[++cnt] = '"';
		el->el_key.buf[++cnt] = '\0';
		(void) fprintf(el->el_errfile,
		    "Some extended keys too long for internal print buffer");
		(void) fprintf(el->el_errfile, " \"%s...\"\n", el->el_key.buf);
		return (0);
	}
	if (ptr == NULL) {
#ifdef DEBUG_EDIT
		(void) fprintf(el->el_errfile,
		    "node_enum: BUG!! Null ptr passed\n!");
#endif
		return (-1);
	}
	/* put this char at end of str */
	ncnt = key__decode_char(el->el_key.buf, cnt, (unsigned char) ptr->ch);
	if (ptr->next == NULL) {
		/* print this key and function */
		el->el_key.buf[ncnt + 1] = '"';
		el->el_key.buf[ncnt + 2] = '\0';
		key_kprint(el, el->el_key.buf, &ptr->val, ptr->type);
	} else
		(void) node_enum(el, ptr->next, ncnt + 1);

	/* go to sibling if there is one */
	if (ptr->sibling)
		(void) node_enum(el, ptr->sibling, cnt);
	return (0);
}


/* key_kprint():
 *	Print the specified key and its associated
 *	function specified by val
 */
protected void
key_kprint(EditLine *el, const char *key, key_value_t *val, int ntype)
{
	el_bindings_t *fp;
	char unparsbuf[EL_BUFSIZ];
	static const char fmt[] = "%-15s->  %s\n";

	if (val != NULL)
		switch (ntype) {
		case XK_STR:
		case XK_EXE:
			(void) fprintf(el->el_outfile, fmt, key,
			    key__decode_str(val->str, unparsbuf,
				ntype == XK_STR ? "\"\"" : "[]"));
			break;
		case XK_CMD:
			for (fp = el->el_map.help; fp->name; fp++)
				if (val->cmd == fp->func) {
					(void) fprintf(el->el_outfile, fmt,
					    key, fp->name);
					break;
				}
#ifdef DEBUG_KEY
			if (fp->name == NULL)
				(void) fprintf(el->el_outfile,
				    "BUG! Command not found.\n");
#endif

			break;
		default:
			EL_ABORT((el->el_errfile, "Bad XK_ type %d\n", ntype));
			break;
		}
	else
		(void) fprintf(el->el_outfile, fmt, key, "no input");
}


/* key__decode_char():
 *	Put a printable form of char in buf.
 */
private int
key__decode_char(char *buf, int cnt, int ch)
{
	if (ch == 0) {
		buf[cnt++] = '^';
		buf[cnt] = '@';
		return (cnt);
	}
	if (iscntrl(ch)) {
		buf[cnt++] = '^';
		if (ch == '\177')
			buf[cnt] = '?';
		else
			buf[cnt] = ch | 0100;
	} else if (ch == '^') {
		buf[cnt++] = '\\';
		buf[cnt] = '^';
	} else if (ch == '\\') {
		buf[cnt++] = '\\';
		buf[cnt] = '\\';
	} else if (ch == ' ' || (isprint(ch) && !isspace(ch))) {
		buf[cnt] = ch;
	} else {
		buf[cnt++] = '\\';
		buf[cnt++] = (((unsigned int) ch >> 6) & 7) + '0';
		buf[cnt++] = (((unsigned int) ch >> 3) & 7) + '0';
		buf[cnt] = (ch & 7) + '0';
	}
	return (cnt);
}


/* key__decode_str():
 *	Make a printable version of the ey
 */
protected char *
key__decode_str(const char *str, char *buf, const char *sep)
{
	char *b;
	const char *p;

	b = buf;
	if (sep[0] != '\0')
		*b++ = sep[0];
	if (*str == 0) {
		*b++ = '^';
		*b++ = '@';
		if (sep[0] != '\0' && sep[1] != '\0')
			*b++ = sep[1];
		*b++ = 0;
		return (buf);
	}
	for (p = str; *p != 0; p++) {
		if (iscntrl((unsigned char) *p)) {
			*b++ = '^';
			if (*p == '\177')
				*b++ = '?';
			else
				*b++ = *p | 0100;
		} else if (*p == '^' || *p == '\\') {
			*b++ = '\\';
			*b++ = *p;
		} else if (*p == ' ' || (isprint((unsigned char) *p) &&
			!isspace((unsigned char) *p))) {
			*b++ = *p;
		} else {
			*b++ = '\\';
			*b++ = (((unsigned int) *p >> 6) & 7) + '0';
			*b++ = (((unsigned int) *p >> 3) & 7) + '0';
			*b++ = (*p & 7) + '0';
		}
	}
	if (sep[0] != '\0' && sep[1] != '\0')
		*b++ = sep[1];
	*b++ = 0;
	return (buf);		/* should check for overflow */
}
