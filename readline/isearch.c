/* **************************************************************** */
/*								    */
/*			I-Search and Searching			    */
/*								    */
/* **************************************************************** */

/* Copyright (C) 1987,1989 Free Software Foundation, Inc.

   This file contains the Readline Library (the Library), a set of
   routines for providing Emacs style line input to programs that ask
   for it.

   The Library is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 1, or (at your option)
   any later version.

   The Library is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   The GNU General Public License is often shipped with GNU software, and
   is generally kept in a file called COPYING or LICENSE.  If you do not
   have a copy of the license, write to the Free Software Foundation,
   675 Mass Ave, Cambridge, MA 02139, USA. */
#define READLINE_LIBRARY

#if defined (HAVE_CONFIG_H)
#  include <config.h>
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
#include "readline.h"
#include "history.h"

/* Variables exported to other files in the readline library. */
unsigned char *_rl_isearch_terminators = (unsigned char *)NULL;

/* Variables imported from other files in the readline library. */
extern Keymap _rl_keymap;
extern HIST_ENTRY *saved_line_for_history;
extern int rl_line_buffer_len;
extern int rl_point, rl_end;
extern char *rl_line_buffer;

extern int rl_execute_next ();
extern void rl_extend_line_buffer ();

extern int _rl_input_available ();

extern char *xmalloc (), *xrealloc ();

static int rl_search_history ();

/* Last line found by the current incremental search, so we don't `find'
   identical lines many times in a row. */
static char *prev_line_found;

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
   DIRECTION is zero for forward, or 1 for reverse,
   WHERE is the history list number of the current line.  If it is
   -1, then this line is the starting one. */
static void
rl_display_search (search_string, reverse_p, where)
     char *search_string;
     int reverse_p, where;
{
  char *message;
  int msglen, searchlen;

  searchlen = (search_string && *search_string) ? strlen (search_string) : 0;

  message = xmalloc (searchlen + 33);
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

  rl_message ("%s", message, 0);
  free (message);
  (*rl_redisplay_function) ();
}

/* Search through the history looking for an interactively typed string.
   This is analogous to i-search.  We start the search in the current line.
   DIRECTION is which direction to search; >= 0 means forward, < 0 means
   backwards. */
static int
rl_search_history (direction, invoking_key)
     int direction, invoking_key;
{
  /* The string that the user types in to search for. */
  char *search_string;

  /* The current length of SEARCH_STRING. */
  int search_string_index;

  /* The amount of space that SEARCH_STRING has allocated to it. */
  int search_string_size;

  /* The list of lines to search through. */
  char **lines, *allocated_line;

  /* The length of LINES. */
  int hlen;

  /* Where we get LINES from. */
  HIST_ENTRY **hlist;

  register int i;
  int orig_point, orig_line, last_found_line;
  int c, found, failed, sline_len;

  /* The line currently being searched. */
  char *sline;

  /* Offset in that line. */
  int line_index;

  /* Non-zero if we are doing a reverse search. */
  int reverse;

  /* The list of characters which terminate the search, but are not
     subsequently executed.  If the variable isearch-terminators has
     been set, we use that value, otherwise we use ESC and C-J. */
  unsigned char *isearch_terminators;

  orig_point = rl_point;
  last_found_line = orig_line = where_history ();
  reverse = direction < 0;
  hlist = history_list ();
  allocated_line = (char *)NULL;

  isearch_terminators = _rl_isearch_terminators ? _rl_isearch_terminators
						: (unsigned char *)"\033\012";

  /* Create an arrary of pointers to the lines that we want to search. */
  maybe_replace_line ();
  i = 0;
  if (hlist)
    for (i = 0; hlist[i]; i++);

  /* Allocate space for this many lines, +1 for the current input line,
     and remember those lines. */
  lines = (char **)xmalloc ((1 + (hlen = i)) * sizeof (char *));
  for (i = 0; i < hlen; i++)
    lines[i] = hlist[i]->line;

  if (saved_line_for_history)
    lines[i] = saved_line_for_history->line;
  else
    {
      /* Keep track of this so we can free it. */
      allocated_line = xmalloc (1 + strlen (rl_line_buffer));
      strcpy (allocated_line, &rl_line_buffer[0]);
      lines[i] = allocated_line;
    }

  hlen++;

  /* The line where we start the search. */
  i = orig_line;

  rl_save_prompt ();

  /* Initialize search parameters. */
  search_string = xmalloc (search_string_size = 128);
  *search_string = '\0';
  search_string_index = 0;
  prev_line_found = (char *)0;		/* XXX */

  /* Normalize DIRECTION into 1 or -1. */
  direction = (direction >= 0) ? 1 : -1;

  rl_display_search (search_string, reverse, -1);

  sline = rl_line_buffer;
  sline_len = strlen (sline);
  line_index = rl_point;

  found = failed = 0;
  for (;;)
    {
      Function *f = (Function *)NULL;

      /* Read a key and decide how to proceed. */
      c = rl_read_key ();

      if (_rl_keymap[c].type == ISFUNC)
	{
	  f = _rl_keymap[c].function;

	  if (f == rl_reverse_search_history)
	    c = reverse ? -1 : -2;
	  else if (f == rl_forward_search_history)
	    c =  !reverse ? -1 : -2;
	}

#if 0
      /* Let NEWLINE (^J) terminate the search for people who don't like
	 using ESC.  ^M can still be used to terminate the search and
	 immediately execute the command. */
      if (c == ESC || c == NEWLINE)
#else
      /* The characters in isearch_terminators (set from the user-settable
	 variable isearch-terminators) are used to terminate the search but
	 not subsequently execute the character as a command.  The default
	 value is "\033\012" (ESC and C-J). */
      if (strchr((char*) isearch_terminators, c))
#endif
	{
	  /* ESC still terminates the search, but if there is pending
	     input or if input arrives within 0.1 seconds (on systems
	     with select(2)) it is used as a prefix character
	     with rl_execute_next.  WATCH OUT FOR THIS!  This is intended
	     to allow the arrow keys to be used like ^F and ^B are used
	     to terminate the search and execute the movement command. */
	  if (c == ESC && _rl_input_available ())	/* XXX */
	    rl_execute_next (ESC);
	  break;
	}

      if (c >= 0 && (CTRL_CHAR (c) || META_CHAR (c) || c == RUBOUT) && c != CTRL ('G'))
	{
	  rl_execute_next (c);
	  break;
	}

      switch (c)
	{
	case -1:
	  if (search_string_index == 0)
	    continue;
	  else if (reverse)
	    --line_index;
	  else if (line_index != sline_len)
	    ++line_index;
	  else
	    ding ();
	  break;

	  /* switch directions */
	case -2:
	  direction = -direction;
	  reverse = direction < 0;
	  break;

	case CTRL ('G'):
	  strcpy (rl_line_buffer, lines[orig_line]);
	  rl_point = orig_point;
	  rl_end = strlen (rl_line_buffer);
	  rl_restore_prompt();
	  rl_clear_message ();
	  if (allocated_line)
	    free (allocated_line);
	  free (lines);
	  return 0;

#if 0
	/* delete character from search string. */
	case -3:
	  if (search_string_index == 0)
	    ding ();
	  else
	    {
	      search_string[--search_string_index] = '\0';
	      /* This is tricky.  To do this right, we need to keep a
		 stack of search positions for the current search, with
		 sentinels marking the beginning and end. */
	    }
	  break;
#endif

	default:
	  /* Add character to search string and continue search. */
	  if (search_string_index + 2 >= search_string_size)
	    {
	      search_string_size += 128;
	      search_string = xrealloc (search_string, search_string_size);
	    }
	  search_string[search_string_index++] = c;
	  search_string[search_string_index] = '\0';
	  break;
	}

      for (found = failed = 0;;)
	{
	  int limit = sline_len - search_string_index + 1;

	  /* Search the current line. */
	  while (reverse ? (line_index >= 0) : (line_index < limit))
	    {
	      if (STREQN (search_string, sline + line_index, search_string_index))
		{
		  found++;
		  break;
		}
	      else
		line_index += direction;
	    }
	  if (found)
	    break;

	  /* Move to the next line, but skip new copies of the line
	     we just found and lines shorter than the string we're
	     searching for. */
	  do
	    {
	      /* Move to the next line. */
	      i += direction;

	      /* At limit for direction? */
	      if (reverse ? (i < 0) : (i == hlen))
		{
		  failed++;
		  break;
		}

	      /* We will need these later. */
	      sline = lines[i];
	      sline_len = strlen (sline);
	    }
	  while ((prev_line_found && STREQ (prev_line_found, lines[i])) ||
		 (search_string_index > sline_len));

	  if (failed)
	    break;

	  /* Now set up the line for searching... */
	  line_index = reverse ? sline_len - search_string_index : 0;
	}

      if (failed)
	{
	  /* We cannot find the search string.  Ding the bell. */
	  ding ();
	  i = last_found_line;
	  continue; 		/* XXX - was break */
	}

      /* We have found the search string.  Just display it.  But don't
	 actually move there in the history list until the user accepts
	 the location. */
      if (found)
	{
	  int line_len;

	  prev_line_found = lines[i];
	  line_len = strlen (lines[i]);

	  if (line_len >= rl_line_buffer_len)
	    rl_extend_line_buffer (line_len);

	  strcpy (rl_line_buffer, lines[i]);
	  rl_point = line_index;
	  rl_end = line_len;
	  last_found_line = i;
	  rl_display_search (search_string, reverse, (i == orig_line) ? -1 : i);
	}
    }

  /* The searching is over.  The user may have found the string that she
     was looking for, or else she may have exited a failing search.  If
     LINE_INDEX is -1, then that shows that the string searched for was
     not found.  We use this to determine where to place rl_point. */

  /* First put back the original state. */
  strcpy (rl_line_buffer, lines[orig_line]);

  rl_restore_prompt ();

  /* Free the search string. */
  free (search_string);

  if (last_found_line < orig_line)
    rl_get_previous_history (orig_line - last_found_line, 0);
  else
    rl_get_next_history (last_found_line - orig_line, 0);

  /* If the string was not found, put point at the end of the line. */
  if (line_index < 0)
    line_index = strlen (rl_line_buffer);
  rl_point = line_index;
  rl_clear_message ();

  if (allocated_line)
    free (allocated_line);
  free (lines);

  return 0;
}
