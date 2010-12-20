/* **************************************************************** */
/*								    */
/*			I-Search and Searching			    */
/*								    */
/* **************************************************************** */

/* Copyright (C) 1987-2005 Free Software Foundation, Inc.

   This file contains the Readline Library (the Library), a set of
   routines for providing Emacs style line input to programs that ask
   for it.

   The Library is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   The Library is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   The GNU General Public License is often shipped with GNU software, and
   is generally kept in a file called COPYING or LICENSE.  If you do not
   have a copy of the license, write to the Free Software Foundation,
   59 Temple Place, Suite 330, Boston, MA 02111 USA. */
#define READLINE_LIBRARY

#if defined (HAVE_CONFIG_H)
#  include "config_readline.h"
#endif

#include <sys/types.h>

#include <stdio.h>

#if defined (HAVE_UNISTD_H)
#  include <unistd.h>
#endif

#if defined (HAVE_STDLIB_H)
#  include <stdlib.h>
#else
#  include "ansi_stdlib.h"
#endif

#include "rldefs.h"
#include "rlmbutil.h"

#include "readline.h"
#include "history.h"

#include "rlprivate.h"
#include "xmalloc.h"

/* Variables exported to other files in the readline library. */
char *_rl_isearch_terminators = (char *)NULL;

_rl_search_cxt *_rl_iscxt = 0;

/* Variables imported from other files in the readline library. */
extern HIST_ENTRY *_rl_saved_line_for_history;

static int rl_search_history PARAMS((int, int));

static _rl_search_cxt *_rl_isearch_init PARAMS((int));
static void _rl_isearch_fini PARAMS((_rl_search_cxt *));
static int _rl_isearch_cleanup PARAMS((_rl_search_cxt *, int));

/* Last line found by the current incremental search, so we don't `find'
   identical lines many times in a row.  Now part of isearch context. */
/* static char *prev_line_found; */

/* Last search string and its length. */
static char *last_isearch_string;
static int last_isearch_string_len;

static const char *default_isearch_terminators = "\033\012";

_rl_search_cxt *
_rl_scxt_alloc (type, flags)
     int type, flags;
{
  _rl_search_cxt *cxt;

  cxt = (_rl_search_cxt *)xmalloc (sizeof (_rl_search_cxt));

  cxt->type = type;
  cxt->sflags = flags;

  cxt->search_string = 0;
  cxt->search_string_size = cxt->search_string_index = 0;

  cxt->lines = 0;
  cxt->allocated_line = 0;
  cxt->hlen = cxt->hindex = 0;

  cxt->save_point = rl_point;
  cxt->save_mark = rl_mark;
  cxt->save_line = where_history ();
  cxt->last_found_line = cxt->save_line;
  cxt->prev_line_found = 0;

  cxt->save_undo_list = 0;

  cxt->history_pos = 0;
  cxt->direction = 0;

  cxt->lastc = 0;

  cxt->sline = 0;
  cxt->sline_len = cxt->sline_index = 0;

  cxt->search_terminators = 0;

  return cxt;
}

void
_rl_scxt_dispose (cxt, flags)
     _rl_search_cxt *cxt;
     int flags __attribute__((unused));
{
  FREE (cxt->search_string);
  FREE (cxt->allocated_line);
  FREE (cxt->lines);

  free (cxt);
}

/* Search backwards through the history looking for a string which is typed
   interactively.  Start with the current line. */
int
rl_reverse_search_history (sign, key)
     int sign, key;
{
  return (rl_search_history (-sign, key));
}

/* Search forwards through the history looking for a string which is typed
   interactively.  Start with the current line. */
int
rl_forward_search_history (sign, key)
     int sign, key;
{
  return (rl_search_history (sign, key));
}

/* Display the current state of the search in the echo-area.
   SEARCH_STRING contains the string that is being searched for,
   DIRECTION is zero for forward, or non-zero for reverse,
   WHERE is the history list number of the current line.  If it is
   -1, then this line is the starting one. */
static void
rl_display_search (search_string, reverse_p, where)
     char *search_string;
     int reverse_p, where __attribute__((unused));
{
  char *message;
  int msglen, searchlen;

  searchlen = (search_string && *search_string) ? strlen (search_string) : 0;

  message = (char *)xmalloc (searchlen + 33);
  msglen = 0;

#if defined (NOTDEF)
  if (where != -1)
    {
      sprintf (message, "[%d]", where + history_base);
      msglen = strlen (message);
    }
#endif /* NOTDEF */

  message[msglen++] = '(';

  if (reverse_p)
    {
      strcpy (message + msglen, "reverse-");
      msglen += 8;
    }

  strcpy (message + msglen, "i-search)`");
  msglen += 10;

  if (search_string)
    {
      strcpy (message + msglen, search_string);
      msglen += searchlen;
    }

  strcpy (message + msglen, "': ");

  rl_message ("%s", message);
  free (message);
  (*rl_redisplay_function) ();
}

static _rl_search_cxt *
_rl_isearch_init (direction)
     int direction;
{
  _rl_search_cxt *cxt;
  register int i;
  HIST_ENTRY **hlist;

  cxt = _rl_scxt_alloc (RL_SEARCH_ISEARCH, 0);
  if (direction < 0)
    cxt->sflags |= SF_REVERSE;

  cxt->search_terminators = _rl_isearch_terminators ? _rl_isearch_terminators
						: default_isearch_terminators;

  /* Create an arrary of pointers to the lines that we want to search. */
  hlist = history_list ();
  rl_maybe_replace_line ();
  i = 0;
  if (hlist)
    for (i = 0; hlist[i]; i++);

  /* Allocate space for this many lines, +1 for the current input line,
     and remember those lines. */
  cxt->lines = (char **)xmalloc ((1 + (cxt->hlen = i)) * sizeof (char *));
  for (i = 0; i < cxt->hlen; i++)
    cxt->lines[i] = hlist[i]->line;

  if (_rl_saved_line_for_history)
    cxt->lines[i] = _rl_saved_line_for_history->line;
  else
    {
      /* Keep track of this so we can free it. */
      cxt->allocated_line = (char *)xmalloc (1 + strlen (rl_line_buffer));
      strcpy (cxt->allocated_line, &rl_line_buffer[0]);
      cxt->lines[i] = cxt->allocated_line;
    }

  cxt->hlen++;

  /* The line where we start the search. */
  cxt->history_pos = cxt->save_line;

  rl_save_prompt ();

  /* Initialize search parameters. */
  cxt->search_string = (char *)xmalloc (cxt->search_string_size = 128);
  cxt->search_string[cxt->search_string_index = 0] = '\0';

  /* Normalize DIRECTION into 1 or -1. */
  cxt->direction = (direction >= 0) ? 1 : -1;

  cxt->sline = rl_line_buffer;
  cxt->sline_len = strlen (cxt->sline);
  cxt->sline_index = rl_point;

  _rl_iscxt = cxt;		/* save globally */

  return cxt;
}

static void
_rl_isearch_fini (cxt)
     _rl_search_cxt *cxt;
{
  /* First put back the original state. */
  strcpy (rl_line_buffer, cxt->lines[cxt->save_line]);

  rl_restore_prompt ();

  /* Save the search string for possible later use. */
  FREE (last_isearch_string);
  last_isearch_string = cxt->search_string;
  last_isearch_string_len = cxt->search_string_index;
  cxt->search_string = 0;

  if (cxt->last_found_line < cxt->save_line)
    rl_get_previous_history (cxt->save_line - cxt->last_found_line, 0);
  else
    rl_get_next_history (cxt->last_found_line - cxt->save_line, 0);

  /* If the string was not found, put point at the end of the last matching
     line.  If last_found_line == orig_line, we didn't find any matching
     history lines at all, so put point back in its original position. */
  if (cxt->sline_index < 0)
    {
      if (cxt->last_found_line == cxt->save_line)
	cxt->sline_index = cxt->save_point;
      else
	cxt->sline_index = strlen (rl_line_buffer);
      rl_mark = cxt->save_mark;
    }

  rl_point = cxt->sline_index;
  /* Don't worry about where to put the mark here; rl_get_previous_history
     and rl_get_next_history take care of it. */

  rl_clear_message ();
}

int
_rl_search_getchar (cxt)
     _rl_search_cxt *cxt;
{
  int c;

  /* Read a key and decide how to proceed. */
  RL_SETSTATE(RL_STATE_MOREINPUT);
  c = cxt->lastc = rl_read_key ();
  RL_UNSETSTATE(RL_STATE_MOREINPUT);

#if defined (HANDLE_MULTIBYTE)
  if (MB_CUR_MAX > 1 && rl_byte_oriented == 0)
    c = cxt->lastc = _rl_read_mbstring (cxt->lastc, cxt->mb, MB_LEN_MAX);
#endif

  return c;
}

/* Process just-read character C according to isearch context CXT.  Return
   -1 if the caller should just free the context and return, 0 if we should
   break out of the loop, and 1 if we should continue to read characters. */
int
_rl_isearch_dispatch (cxt, c)
     _rl_search_cxt *cxt;
     int c;
{
  int n, wstart, wlen, limit, cval;
  rl_command_func_t *f;

  f = (rl_command_func_t *)NULL;
 
 /* Translate the keys we do something with to opcodes. */
  if (c >= 0 && _rl_keymap[c].type == ISFUNC)
    {
      f = _rl_keymap[c].function;

      if (f == rl_reverse_search_history)
	cxt->lastc = (cxt->sflags & SF_REVERSE) ? -1 : -2;
      else if (f == rl_forward_search_history)
	cxt->lastc = (cxt->sflags & SF_REVERSE) ? -2 : -1;
      else if (f == rl_rubout)
	cxt->lastc = -3;
      else if (c == CTRL ('G'))
	cxt->lastc = -4;
      else if (c == CTRL ('W'))	/* XXX */
	cxt->lastc = -5;
      else if (c == CTRL ('Y'))	/* XXX */
	cxt->lastc = -6;
    }

  /* The characters in isearch_terminators (set from the user-settable
     variable isearch-terminators) are used to terminate the search but
     not subsequently execute the character as a command.  The default
     value is "\033\012" (ESC and C-J). */
  if (strchr (cxt->search_terminators, cxt->lastc))
    {
      /* ESC still terminates the search, but if there is pending
	 input or if input arrives within 0.1 seconds (on systems
	 with select(2)) it is used as a prefix character
	 with rl_execute_next.  WATCH OUT FOR THIS!  This is intended
	 to allow the arrow keys to be used like ^F and ^B are used
	 to terminate the search and execute the movement command.
	 XXX - since _rl_input_available depends on the application-
	 settable keyboard timeout value, this could alternatively
	 use _rl_input_queued(100000) */
      if (cxt->lastc == ESC && _rl_input_available ())
	rl_execute_next (ESC);
      return (0);
    }

#define ENDSRCH_CHAR(c) \
  ((CTRL_CHAR (c) || META_CHAR (c) || (c) == RUBOUT) && ((c) != CTRL ('G')))

#if defined (HANDLE_MULTIBYTE)
  if (MB_CUR_MAX > 1 && rl_byte_oriented == 0)
    {
      if (cxt->lastc >= 0 && (cxt->mb[0] && cxt->mb[1] == '\0') && ENDSRCH_CHAR (cxt->lastc))
	{
	  /* This sets rl_pending_input to c; it will be picked up the next
	     time rl_read_key is called. */
	  rl_execute_next (cxt->lastc);
	  return (0);
	}
    }
  else
#endif
    if (cxt->lastc >= 0 && ENDSRCH_CHAR (cxt->lastc))
      {
	/* This sets rl_pending_input to LASTC; it will be picked up the next
	   time rl_read_key is called. */
	rl_execute_next (cxt->lastc);
	return (0);
      }

  /* Now dispatch on the character.  `Opcodes' affect the search string or
     state.  Other characters are added to the string.  */
  switch (cxt->lastc)
    {
    /* search again */
    case -1:
      if (cxt->search_string_index == 0)
	{
	  if (last_isearch_string)
	    {
	      cxt->search_string_size = 64 + last_isearch_string_len;
	      cxt->search_string = (char *)xrealloc (cxt->search_string, cxt->search_string_size);
	      strcpy (cxt->search_string, last_isearch_string);
	      cxt->search_string_index = last_isearch_string_len;
	      rl_display_search (cxt->search_string, (cxt->sflags & SF_REVERSE), -1);
	      break;
	    }
	  return (1);
	}
      else if (cxt->sflags & SF_REVERSE)
	cxt->sline_index--;
      else if (cxt->sline_index != cxt->sline_len)
	cxt->sline_index++;
      else
	rl_ding ();
      break;

    /* switch directions */
    case -2:
      cxt->direction = -cxt->direction;
      if (cxt->direction < 0)
	cxt->sflags |= SF_REVERSE;
      else
	cxt->sflags &= ~SF_REVERSE;
      break;

    /* delete character from search string. */
    case -3:	/* C-H, DEL */
      /* This is tricky.  To do this right, we need to keep a
	 stack of search positions for the current search, with
	 sentinels marking the beginning and end.  But this will
	 do until we have a real isearch-undo. */
      if (cxt->search_string_index == 0)
	rl_ding ();
      else
	cxt->search_string[--cxt->search_string_index] = '\0';
      break;

    case -4:	/* C-G, abort */
      rl_replace_line (cxt->lines[cxt->save_line], 0);
      rl_point = cxt->save_point;
      rl_mark = cxt->save_mark;
      rl_restore_prompt();
      rl_clear_message ();

      return -1;

    case -5:	/* C-W */
      /* skip over portion of line we already matched and yank word */
      wstart = rl_point + cxt->search_string_index;
      if (wstart >= rl_end)
	{
	  rl_ding ();
	  break;
	}

      /* if not in a word, move to one. */
      cval = _rl_char_value (rl_line_buffer, wstart);
      if (_rl_walphabetic (cval) == 0)
	{
	  rl_ding ();
	  break;
	}
      n = MB_NEXTCHAR (rl_line_buffer, wstart, 1, MB_FIND_NONZERO);;
      while (n < rl_end)
	{
	  cval = _rl_char_value (rl_line_buffer, n);
	  if (_rl_walphabetic (cval) == 0)
	    break;
	  n = MB_NEXTCHAR (rl_line_buffer, n, 1, MB_FIND_NONZERO);;
	}
      wlen = n - wstart + 1;
      if (cxt->search_string_index + wlen + 1 >= cxt->search_string_size)
	{
	  cxt->search_string_size += wlen + 1;
	  cxt->search_string = (char *)xrealloc (cxt->search_string, cxt->search_string_size);
	}
      for (; wstart < n; wstart++)
	cxt->search_string[cxt->search_string_index++] = rl_line_buffer[wstart];
      cxt->search_string[cxt->search_string_index] = '\0';
      break;

    case -6:	/* C-Y */
      /* skip over portion of line we already matched and yank rest */
      wstart = rl_point + cxt->search_string_index;
      if (wstart >= rl_end)
	{
	  rl_ding ();
	  break;
	}
      n = rl_end - wstart + 1;
      if (cxt->search_string_index + n + 1 >= cxt->search_string_size)
	{
	  cxt->search_string_size += n + 1;
	  cxt->search_string = (char *)xrealloc (cxt->search_string, cxt->search_string_size);
	}
      for (n = wstart; n < rl_end; n++)
	cxt->search_string[cxt->search_string_index++] = rl_line_buffer[n];
      cxt->search_string[cxt->search_string_index] = '\0';
      break;

    /* Add character to search string and continue search. */
    default:
      if (cxt->search_string_index + 2 >= cxt->search_string_size)
	{
	  cxt->search_string_size += 128;
	  cxt->search_string = (char *)xrealloc (cxt->search_string, cxt->search_string_size);
	}
#if defined (HANDLE_MULTIBYTE)
      if (MB_CUR_MAX > 1 && rl_byte_oriented == 0)
	{
	  int j, l;
	  for (j = 0, l = strlen (cxt->mb); j < l; )
	    cxt->search_string[cxt->search_string_index++] = cxt->mb[j++];
	}
      else
#endif
	cxt->search_string[cxt->search_string_index++] = c;
      cxt->search_string[cxt->search_string_index] = '\0';
      break;
    }

  for (cxt->sflags &= ~(SF_FOUND|SF_FAILED);; )
    {
      limit = cxt->sline_len - cxt->search_string_index + 1;

      /* Search the current line. */
      while ((cxt->sflags & SF_REVERSE) ? (cxt->sline_index >= 0) : (cxt->sline_index < limit))
	{
	  if (STREQN (cxt->search_string, cxt->sline + cxt->sline_index, cxt->search_string_index))
	    {
	      cxt->sflags |= SF_FOUND;
	      break;
	    }
	  else
	    cxt->sline_index += cxt->direction;
	}
      if (cxt->sflags & SF_FOUND)
	break;

      /* Move to the next line, but skip new copies of the line
	 we just found and lines shorter than the string we're
	 searching for. */
      do
	{
	  /* Move to the next line. */
	  cxt->history_pos += cxt->direction;

	  /* At limit for direction? */
	  if ((cxt->sflags & SF_REVERSE) ? (cxt->history_pos < 0) : (cxt->history_pos == cxt->hlen))
	    {
	      cxt->sflags |= SF_FAILED;
	      break;
	    }

	  /* We will need these later. */
	  cxt->sline = cxt->lines[cxt->history_pos];
	  cxt->sline_len = strlen (cxt->sline);
	}
      while ((cxt->prev_line_found && STREQ (cxt->prev_line_found, cxt->lines[cxt->history_pos])) ||
	     (cxt->search_string_index > cxt->sline_len));

      if (cxt->sflags & SF_FAILED)
	break;

      /* Now set up the line for searching... */
      cxt->sline_index = (cxt->sflags & SF_REVERSE) ? cxt->sline_len - cxt->search_string_index : 0;
    }

  if (cxt->sflags & SF_FAILED)
    {
      /* We cannot find the search string.  Ding the bell. */
      rl_ding ();
      cxt->history_pos = cxt->last_found_line;
      return 1;
    }

  /* We have found the search string.  Just display it.  But don't
     actually move there in the history list until the user accepts
     the location. */
  if (cxt->sflags & SF_FOUND)
    {
      cxt->prev_line_found = cxt->lines[cxt->history_pos];
      rl_replace_line (cxt->lines[cxt->history_pos], 0);
      rl_point = cxt->sline_index;
      cxt->last_found_line = cxt->history_pos;
      rl_display_search (cxt->search_string, (cxt->sflags & SF_REVERSE), (cxt->history_pos == cxt->save_line) ? -1 : cxt->history_pos);
    }

  return 1;
}

static int
_rl_isearch_cleanup (cxt, r)
     _rl_search_cxt *cxt;
     int r;
{
  if (r >= 0)
    _rl_isearch_fini (cxt);
  _rl_scxt_dispose (cxt, 0);
  _rl_iscxt = 0;

  RL_UNSETSTATE(RL_STATE_ISEARCH);

  return (r != 0);
}

/* Search through the history looking for an interactively typed string.
   This is analogous to i-search.  We start the search in the current line.
   DIRECTION is which direction to search; >= 0 means forward, < 0 means
   backwards. */
static int
rl_search_history (direction, invoking_key)
     int direction, invoking_key __attribute__((unused));
{
  _rl_search_cxt *cxt;		/* local for now, but saved globally */
  int r;

  RL_SETSTATE(RL_STATE_ISEARCH);
  cxt = _rl_isearch_init (direction);

  rl_display_search (cxt->search_string, (cxt->sflags & SF_REVERSE), -1);

  /* If we are using the callback interface, all we do is set up here and
      return.  The key is that we leave RL_STATE_ISEARCH set. */
  if (RL_ISSTATE (RL_STATE_CALLBACK))
    return (0);

  r = -1;
  for (;;)
    {
      _rl_search_getchar (cxt);
      /* We might want to handle EOF here (c == 0) */
      r = _rl_isearch_dispatch (cxt, cxt->lastc);
      if (r <= 0)
        break;
    }

  /* The searching is over.  The user may have found the string that she
     was looking for, or else she may have exited a failing search.  If
     LINE_INDEX is -1, then that shows that the string searched for was
     not found.  We use this to determine where to place rl_point. */
  return (_rl_isearch_cleanup (cxt, r));
}

#if defined (READLINE_CALLBACKS)
/* Called from the callback functions when we are ready to read a key.  The
   callback functions know to call this because RL_ISSTATE(RL_STATE_ISEARCH).
   If _rl_isearch_dispatch finishes searching, this function is responsible
   for turning off RL_STATE_ISEARCH, which it does using _rl_isearch_cleanup. */
int
_rl_isearch_callback (cxt)
     _rl_search_cxt *cxt;
{
  int r;

  _rl_search_getchar (cxt);
  /* We might want to handle EOF here */
  r = _rl_isearch_dispatch (cxt, cxt->lastc);

  return (r <= 0) ? _rl_isearch_cleanup (cxt, r) : 0;
}
#endif
