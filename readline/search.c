/* search.c - code for non-incremental searching in emacs and vi modes. */

/* Copyright (C) 1992 Free Software Foundation, Inc.

   This file is part of the Readline Library (the Library), a set of
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

#ifdef abs
#  undef abs
#endif
#define abs(x)		(((x) >= 0) ? (x) : -(x))

extern char *xmalloc (), *xrealloc ();

/* Variables imported from readline.c */
extern int rl_point, rl_end, rl_line_buffer_len;
extern int rl_editing_mode;
extern char *rl_prompt;
extern char *rl_line_buffer;
extern HIST_ENTRY *saved_line_for_history;
extern Function *rl_last_func;

/* Functions imported from the rest of the library. */
extern int _rl_free_history_entry ();
extern char *_rl_make_prompt_for_search ();
extern void rl_extend_line_buffer ();

static char *noninc_search_string = (char *) NULL;
static int noninc_history_pos;
static char *prev_line_found = (char *) NULL;

/* Search the history list for STRING starting at absolute history position
   POS.  If STRING begins with `^', the search must match STRING at the
   beginning of a history line, otherwise a full substring match is performed
   for STRING.  DIR < 0 means to search backwards through the history list,
   DIR >= 0 means to search forward. */
static int
noninc_search_from_pos (string, pos, dir)
     char *string;
     int pos, dir;
{
  int ret, old;

  old = where_history ();
  history_set_pos (pos);

  if (*string == '^')
    ret = history_search_prefix (string + 1, dir);
  else
    ret = history_search (string, dir);

  if (ret != -1)
    ret = where_history ();

  history_set_pos (old);
  return (ret);
}

/* Search for a line in the history containing STRING.  If DIR is < 0, the
   search is backwards through previous entries, else through subsequent
   entries. */
static void
noninc_dosearch (string, dir)
     char *string;
     int dir;
{
  int oldpos, pos, line_len;
  HIST_ENTRY *entry;

  if (string == 0 || *string == '\0' || noninc_history_pos < 0)
    {
      ding ();
      return;
    }

  pos = noninc_search_from_pos (string, noninc_history_pos + dir, dir);
  if (pos == -1)
    {
      /* Search failed, current history position unchanged. */
      maybe_unsave_line ();
      rl_clear_message ();
      rl_point = 0;
      ding ();
      return;
    }

  noninc_history_pos = pos;

  oldpos = where_history ();
  history_set_pos (noninc_history_pos);
  entry = current_history ();
#if defined (VI_MODE)
  if (rl_editing_mode != vi_mode)
#endif
  history_set_pos (oldpos);

  line_len = strlen (entry->line);
  if (line_len >= rl_line_buffer_len)
    rl_extend_line_buffer (line_len);
  strcpy (rl_line_buffer, entry->line);

  rl_undo_list = (UNDO_LIST *)entry->data;
  rl_end = strlen (rl_line_buffer);
  rl_point = 0;
  rl_clear_message ();

  if (saved_line_for_history)
    _rl_free_history_entry (saved_line_for_history);
  saved_line_for_history = (HIST_ENTRY *)NULL;
}

/* Search non-interactively through the history list.  DIR < 0 means to
   search backwards through the history of previous commands; otherwise
   the search is for commands subsequent to the current position in the
   history list.  PCHAR is the character to use for prompting when reading
   the search string; if not specified (0), it defaults to `:'. */
static void
noninc_search (dir, pchar)
     int dir;
     int pchar;
{
  int saved_point, c;
  char *p;

  maybe_save_line ();
  saved_point = rl_point;

  /* Use the line buffer to read the search string. */
  rl_line_buffer[0] = 0;
  rl_end = rl_point = 0;

  p = _rl_make_prompt_for_search (pchar ? pchar : ':');
  rl_message (p, 0, 0);
  free (p);

#define SEARCH_RETURN rl_restore_prompt (); return

  /* Read the search string. */
  while ((c = rl_read_key ()))
    {
      switch (c)
	{
	case CTRL('H'):
	case RUBOUT:
	  if (rl_point == 0)
	    {
	      maybe_unsave_line ();
	      rl_clear_message ();
	      rl_point = saved_point;
	      SEARCH_RETURN;
	    }
	  rl_rubout (1, c);
	  break;

	case CTRL('W'):
	  rl_unix_word_rubout (1, c);
	  break;

	case CTRL('U'):
	  rl_unix_line_discard (1, c);
	  break;

	case RETURN:
	case NEWLINE:
	  goto dosearch;
	  /* NOTREACHED */
	  break;

	case CTRL('C'):
	case CTRL('G'):
	  maybe_unsave_line ();
	  rl_clear_message ();
	  rl_point = saved_point;
	  ding ();
	  SEARCH_RETURN;

	default:
	  rl_insert (1, c);
	  break;
	}
      (*rl_redisplay_function) ();
    }

 dosearch:
  /* If rl_point == 0, we want to re-use the previous search string and
     start from the saved history position.  If there's no previous search
     string, punt. */
  if (rl_point == 0)
    {
      if (!noninc_search_string)
	{
	  ding ();
	  SEARCH_RETURN;
	}
    }
  else
    {
      /* We want to start the search from the current history position. */
      noninc_history_pos = where_history ();
      if (noninc_search_string)
	free (noninc_search_string);
      noninc_search_string = savestring (rl_line_buffer);
    }

  rl_restore_prompt ();
  noninc_dosearch (noninc_search_string, dir);
}

/* Search forward through the history list for a string.  If the vi-mode
   code calls this, KEY will be `?'. */
int
rl_noninc_forward_search (count, key)
     int count, key;
{
  noninc_search (1, (key == '?') ? '?' : 0);
  return 0;
}

/* Reverse search the history list for a string.  If the vi-mode code
   calls this, KEY will be `/'. */
int
rl_noninc_reverse_search (count, key)
     int count, key;
{
  noninc_search (-1, (key == '/') ? '/' : 0);
  return 0;
}

/* Search forward through the history list for the last string searched
   for.  If there is no saved search string, abort. */
int
rl_noninc_forward_search_again (count, key)
     int count, key;
{
  if (!noninc_search_string)
    {
      ding ();
      return (-1);
    }
  noninc_dosearch (noninc_search_string, 1);
  return 0;
}

/* Reverse search in the history list for the last string searched
   for.  If there is no saved search string, abort. */
int
rl_noninc_reverse_search_again (count, key)
     int count, key;
{
  if (!noninc_search_string)
    {
      ding ();
      return (-1);
    }
  noninc_dosearch (noninc_search_string, -1);
  return 0;
}

static int
rl_history_search_internal (count, direction)
     int count, direction;
{
  HIST_ENTRY *temp, *old_temp;
  int line_len;

  maybe_save_line ();

  temp = old_temp = (HIST_ENTRY *)NULL;
  while (count)
    {
      temp = (direction < 0) ? previous_history () : next_history ();
      if (temp == 0)
        break;
      /* On an empty prefix, make this the same as previous-history. */
      if (rl_point == 0)
	{
	  count--;
	  continue;
	}
      if (STREQN (rl_line_buffer, temp->line, rl_point))
	{
	  /* Don't find multiple instances of the same line. */
	  if (prev_line_found && STREQ (prev_line_found, temp->line))
	    continue;
          if (direction < 0)
            old_temp = temp;
          prev_line_found = temp->line;
          count--;
	}
    }

  if (temp == 0)
    {
      if (direction < 0 && old_temp)
	temp = old_temp;
      else
	{
	  maybe_unsave_line ();
	  ding ();
	  return 1;
	}
    }

  line_len = strlen (temp->line);
  if (line_len >= rl_line_buffer_len)
    rl_extend_line_buffer (line_len);
  strcpy (rl_line_buffer, temp->line);
  rl_undo_list = (UNDO_LIST *)temp->data;
  rl_end = line_len;
  return 0;
}

/* Search forward in the history for the string of characters
   from the start of the line to rl_point.  This is a non-incremental
   search. */
int
rl_history_search_forward (count, ignore)
     int count, ignore;
{
  if (count == 0)
    return (0);
  if (rl_last_func != rl_history_search_forward)
    prev_line_found = (char *)NULL;
  return (rl_history_search_internal (abs (count), (count > 0) ? 1 : -1));
}

/* Search backward through the history for the string of characters
   from the start of the line to rl_point.  This is a non-incremental
   search. */
int
rl_history_search_backward (count, ignore)
     int count, ignore;
{
  if (count == 0)
    return (0);
  if (rl_last_func != rl_history_search_backward)
    prev_line_found = (char *)NULL;
  return (rl_history_search_internal (abs (count), (count > 0) ? -1 : 1));
}
