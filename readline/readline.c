/* readline.c -- a general facility for reading lines of input
   with emacs style editing and completion. */

/* Copyright (C) 1987, 1989, 1992 Free Software Foundation, Inc.

   This file is part of the GNU Readline Library, a library for
   reading lines of text with interactive input and history editing.

   The GNU Readline Library is free software; you can redistribute it
   and/or modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 1, or
   (at your option) any later version.

   The GNU Readline Library is distributed in the hope that it will be
   useful, but WITHOUT ANY WARRANTY; without even the implied warranty
   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   The GNU General Public License is often shipped with GNU software, and
   is generally kept in a file called COPYING or LICENSE.  If you do not
   have a copy of the license, write to the Free Software Foundation,
   675 Mass Ave, Cambridge, MA 02139, USA. */
#define READLINE_LIBRARY

#if defined (HAVE_CONFIG_H)
#  include <config.h>
#endif

#include <sys/types.h>
#include "posixstat.h"
#include <fcntl.h>
#if defined (HAVE_SYS_FILE_H)
#  include <sys/file.h>
#endif /* HAVE_SYS_FILE_H */

#if defined (HAVE_UNISTD_H)
#  include <unistd.h>
#endif /* HAVE_UNISTD_H */

#if defined (HAVE_STDLIB_H)
#  include <stdlib.h>
#else
#  include "ansi_stdlib.h"
#endif /* HAVE_STDLIB_H */

#if defined (HAVE_LOCALE_H)
#  include <locale.h>
#endif

#include <signal.h>
#include <stdio.h>
#include "posixjmp.h"

/* System-specific feature definitions and include files. */
#include "rldefs.h"

#if defined (__EMX__)
#  define INCL_DOSPROCESS
#  include <os2.h>
#endif /* __EMX__ */

/* Some standard library routines. */
#include "readline.h"
#include "history.h"

#ifndef RL_LIBRARY_VERSION
#  define RL_LIBRARY_VERSION "4.0"
#endif

/* Evaluates its arguments multiple times. */
#define SWAP(s, e)  do { int t; t = s; s = e; e = t; } while (0)

/* NOTE: Functions and variables prefixed with `_rl_' are
   pseudo-global: they are global so they can be shared
   between files in the readline library, but are not intended
   to be visible to readline callers. */

/* Variables and functions imported from terminal.c */
extern int _rl_init_terminal_io ();
extern void _rl_enable_meta_key ();
#ifdef _MINIX
extern void _rl_output_character_function ();
#else
extern int _rl_output_character_function ();
#endif

extern int _rl_enable_meta;
extern int _rl_term_autowrap;
extern int screenwidth, screenheight, screenchars;

/* Variables and functions imported from rltty.c. */
extern void rl_prep_terminal (), rl_deprep_terminal ();
extern void rltty_set_default_bindings ();

/* Functions imported from util.c. */
extern void _rl_abort_internal ();
extern void rl_extend_line_buffer ();
extern int alphabetic ();

/* Functions imported from bind.c. */
extern void _rl_bind_if_unbound ();

/* Functions imported from input.c. */
extern int _rl_any_typein ();
extern void _rl_insert_typein ();
extern int rl_read_key ();

/* Functions imported from nls.c */
extern int _rl_init_eightbit ();

/* Functions imported from shell.c */
extern char *get_env_value ();

/* External redisplay functions and variables from display.c */
extern void _rl_move_vert ();
extern void _rl_update_final ();
extern void _rl_clear_to_eol ();
extern void _rl_clear_screen ();
extern void _rl_erase_entire_line ();

extern void _rl_erase_at_end_of_line ();
extern void _rl_move_cursor_relative ();

extern int _rl_vis_botlin;
extern int _rl_last_c_pos;
extern int _rl_horizontal_scroll_mode;
extern int rl_display_fixed;
extern int _rl_suppress_redisplay;
extern char *rl_display_prompt;

/* Variables imported from complete.c. */
extern char *rl_completer_word_break_characters;
extern char *rl_basic_word_break_characters;
extern int rl_completion_query_items;
extern int rl_complete_with_tilde_expansion;

/* Variables and functions from macro.c. */
extern void _rl_add_macro_char ();
extern void _rl_with_macro_input ();
extern int _rl_next_macro_key ();
extern int _rl_defining_kbd_macro;

#if defined (VI_MODE)
/* Functions imported from vi_mode.c. */
extern void _rl_vi_set_last ();
extern void _rl_vi_reset_last ();
extern void _rl_vi_done_inserting ();
extern int _rl_vi_textmod_command ();
extern void _rl_vi_initialize_line ();
#endif /* VI_MODE */

extern UNDO_LIST *rl_undo_list;
extern int _rl_doing_an_undo;

/* Forward declarations used in this file. */
void _rl_free_history_entry ();

int _rl_dispatch ();
int _rl_init_argument ();

static char *readline_internal ();
static void readline_initialize_everything ();
static void start_using_history ();
static void bind_arrow_keys ();

#if !defined (__GO32__)
static void readline_default_bindings ();
#endif /* !__GO32__ */

#if defined (__GO32__)
#  include <go32.h>
#  include <pc.h>
#  undef HANDLE_SIGNALS
#endif /* __GO32__ */

extern char *xmalloc (), *xrealloc ();

/* **************************************************************** */
/*								    */
/*			Line editing input utility		    */
/*								    */
/* **************************************************************** */

char *rl_library_version = RL_LIBRARY_VERSION;

/* A pointer to the keymap that is currently in use.
   By default, it is the standard emacs keymap. */
Keymap _rl_keymap = emacs_standard_keymap;

/* The current style of editing. */
int rl_editing_mode = emacs_mode;

/* Non-zero if we called this function from _rl_dispatch().  It's present
   so functions can find out whether they were called from a key binding
   or directly from an application. */
int rl_dispatching;

/* Non-zero if the previous command was a kill command. */
int _rl_last_command_was_kill = 0;

/* The current value of the numeric argument specified by the user. */
int rl_numeric_arg = 1;

/* Non-zero if an argument was typed. */
int rl_explicit_arg = 0;

/* Temporary value used while generating the argument. */
int rl_arg_sign = 1;

/* Non-zero means we have been called at least once before. */
static int rl_initialized;

/* If non-zero, this program is running in an EMACS buffer. */
static int running_in_emacs;

/* The current offset in the current input line. */
int rl_point;

/* Mark in the current input line. */
int rl_mark;

/* Length of the current input line. */
int rl_end;

/* Make this non-zero to return the current input_line. */
int rl_done;

/* The last function executed by readline. */
Function *rl_last_func = (Function *)NULL;

/* Top level environment for readline_internal (). */
procenv_t readline_top_level;

/* The streams we interact with. */
FILE *_rl_in_stream, *_rl_out_stream;

/* The names of the streams that we do input and output to. */
FILE *rl_instream = (FILE *)NULL;
FILE *rl_outstream = (FILE *)NULL;

/* Non-zero means echo characters as they are read. */
int readline_echoing_p = 1;

/* Current prompt. */
char *rl_prompt;
int rl_visible_prompt_length = 0;

/* The number of characters read in order to type this complete command. */
int rl_key_sequence_length = 0;

/* If non-zero, then this is the address of a function to call just
   before readline_internal_setup () prints the first prompt. */
Function *rl_startup_hook = (Function *)NULL;

/* If non-zero, this is the address of a function to call just before
   readline_internal_setup () returns and readline_internal starts
   reading input characters. */
Function *rl_pre_input_hook = (Function *)NULL;

/* What we use internally.  You should always refer to RL_LINE_BUFFER. */
static char *the_line;

/* The character that can generate an EOF.  Really read from
   the terminal driver... just defaulted here. */
int _rl_eof_char = CTRL ('D');

/* Non-zero makes this the next keystroke to read. */
int rl_pending_input = 0;

/* Pointer to a useful terminal name. */
char *rl_terminal_name = (char *)NULL;

/* Non-zero means to always use horizontal scrolling in line display. */
int _rl_horizontal_scroll_mode = 0;

/* Non-zero means to display an asterisk at the starts of history lines
   which have been modified. */
int _rl_mark_modified_lines = 0;  

/* The style of `bell' notification preferred.  This can be set to NO_BELL,
   AUDIBLE_BELL, or VISIBLE_BELL. */
int _rl_bell_preference = AUDIBLE_BELL;
     
/* String inserted into the line by rl_insert_comment (). */
char *_rl_comment_begin;

/* Keymap holding the function currently being executed. */
Keymap rl_executing_keymap;

/* Non-zero means to erase entire line, including prompt, on empty input lines. */
int rl_erase_empty_line = 0;

/* Line buffer and maintenence. */
char *rl_line_buffer = (char *)NULL;
int rl_line_buffer_len = 0;

/* Forward declarations used by the display and termcap code. */

/* **************************************************************** */
/*								    */
/*			`Forward' declarations  		    */
/*								    */
/* **************************************************************** */

/* Non-zero means do not parse any lines other than comments and
   parser directives. */
unsigned char _rl_parsing_conditionalized_out = 0;

/* Non-zero means to convert characters with the meta bit set to
   escape-prefixed characters so we can indirect through
   emacs_meta_keymap or vi_escape_keymap. */
int _rl_convert_meta_chars_to_ascii = 1;

/* Non-zero means to output characters with the meta bit set directly
   rather than as a meta-prefixed escape sequence. */
int _rl_output_meta_chars = 0;

/* **************************************************************** */
/*								    */
/*			Top Level Functions			    */
/*								    */
/* **************************************************************** */

/* Non-zero means treat 0200 bit in terminal input as Meta bit. */
int _rl_meta_flag = 0;	/* Forward declaration */

/* Read a line of input.  Prompt with PROMPT.  An empty PROMPT means
   none.  A return value of NULL means that EOF was encountered. */
char *
readline (prompt)
     char *prompt;
{
  char *value;

  rl_prompt = prompt;

  /* If we are at EOF return a NULL string. */
  if (rl_pending_input == EOF)
    {
      rl_pending_input = 0;
      return ((char *)NULL);
    }

  rl_visible_prompt_length = rl_expand_prompt (rl_prompt);

  rl_initialize ();
  (*rl_prep_term_function) (_rl_meta_flag);

#if defined (HANDLE_SIGNALS)
  rl_set_signals ();
#endif

  value = readline_internal ();
  (*rl_deprep_term_function) ();

#if defined (HANDLE_SIGNALS)
  rl_clear_signals ();
#endif

  return (value);
}

#if defined (READLINE_CALLBACKS)
#  define STATIC_CALLBACK
#else
#  define STATIC_CALLBACK static
#endif

STATIC_CALLBACK void
readline_internal_setup ()
{
  _rl_in_stream = rl_instream;
  _rl_out_stream = rl_outstream;

  if (rl_startup_hook)
    (*rl_startup_hook) ();

  if (readline_echoing_p == 0)
    {
      if (rl_prompt)
	{
	  fprintf (_rl_out_stream, "%s", rl_prompt);
	  fflush (_rl_out_stream);
	}
    }
  else
    {
      rl_on_new_line ();
      (*rl_redisplay_function) ();
#if defined (VI_MODE)
      if (rl_editing_mode == vi_mode)
	rl_vi_insertion_mode (1, 0);
#endif /* VI_MODE */
    }

  if (rl_pre_input_hook)
    (*rl_pre_input_hook) ();
}

STATIC_CALLBACK char *
readline_internal_teardown (eof)
     int eof;
{
  char *temp;
  HIST_ENTRY *entry;

  /* Restore the original of this history line, iff the line that we
     are editing was originally in the history, AND the line has changed. */
  entry = current_history ();

  if (entry && rl_undo_list)
    {
      temp = savestring (the_line);
      rl_revert_line (1, 0);
      entry = replace_history_entry (where_history (), the_line, (histdata_t)NULL);
      _rl_free_history_entry (entry);

      strcpy (the_line, temp);
      free (temp);
    }

  /* At any rate, it is highly likely that this line has an undo list.  Get
     rid of it now. */
  if (rl_undo_list)
    free_undo_list ();

  return (eof ? (char *)NULL : savestring (the_line));
}

STATIC_CALLBACK int
#if defined (READLINE_CALLBACKS)
readline_internal_char ()
#else
readline_internal_charloop ()
#endif
{
  static int lastc, eof_found;
  int c, code, lk;

  lastc = -1;
  eof_found = 0;

#if !defined (READLINE_CALLBACKS)
  while (rl_done == 0)
    {
#endif
      lk = _rl_last_command_was_kill;

      code = setjmp (readline_top_level);

      if (code)
	(*rl_redisplay_function) ();

      if (rl_pending_input == 0)
	{
	  /* Then initialize the argument and number of keys read. */
	  _rl_init_argument ();
	  rl_key_sequence_length = 0;
	}

      c = rl_read_key ();

      /* EOF typed to a non-blank line is a <NL>. */
      if (c == EOF && rl_end)
	c = NEWLINE;

      /* The character _rl_eof_char typed to blank line, and not as the
	 previous character is interpreted as EOF. */
      if (((c == _rl_eof_char && lastc != c) || c == EOF) && !rl_end)
	{
#if defined (READLINE_CALLBACKS)
	  return (rl_done = 1);
#else
	  eof_found = 1;
	  break;
#endif
	}

      lastc = c;
      _rl_dispatch (c, _rl_keymap);

      /* If there was no change in _rl_last_command_was_kill, then no kill
	 has taken place.  Note that if input is pending we are reading
	 a prefix command, so nothing has changed yet. */
      if (rl_pending_input == 0 && lk == _rl_last_command_was_kill)
	_rl_last_command_was_kill = 0;

#if defined (VI_MODE)
      /* In vi mode, when you exit insert mode, the cursor moves back
	 over the previous character.  We explicitly check for that here. */
      if (rl_editing_mode == vi_mode && _rl_keymap == vi_movement_keymap)
	rl_vi_check ();
#endif /* VI_MODE */

      if (rl_done == 0)
	(*rl_redisplay_function) ();

      /* If the application writer has told us to erase the entire line if
	  the only character typed was something bound to rl_newline, do so. */
      if (rl_erase_empty_line && rl_done && rl_last_func == rl_newline &&
	  rl_point == 0 && rl_end == 0)
	_rl_erase_entire_line ();

#if defined (READLINE_CALLBACKS)
      return 0;
#else
    }

  return (eof_found);
#endif
}

#if defined (READLINE_CALLBACKS)
static int
readline_internal_charloop ()
{
  int eof = 1;

  while (rl_done == 0)
    eof = readline_internal_char ();
  return (eof);
}
#endif /* READLINE_CALLBACKS */

/* Read a line of input from the global rl_instream, doing output on
   the global rl_outstream.
   If rl_prompt is non-null, then that is our prompt. */
static char *
readline_internal ()
{
  int eof;

  readline_internal_setup ();
  eof = readline_internal_charloop ();
  return (readline_internal_teardown (eof));
}

void
_rl_init_line_state ()
{
  rl_point = rl_end = 0;
  the_line = rl_line_buffer;
  the_line[0] = 0;
}

void
_rl_set_the_line ()
{
  the_line = rl_line_buffer;
}

/* Do the command associated with KEY in MAP.
   If the associated command is really a keymap, then read
   another key, and dispatch into that map. */
int
_rl_dispatch (key, map)
     register int key;
     Keymap map;
{
  int r, newkey;
  char *macro;
  Function *func;

  if (META_CHAR (key) && _rl_convert_meta_chars_to_ascii)
    {
      if (map[ESC].type == ISKMAP)
	{
	  if (_rl_defining_kbd_macro)
	    _rl_add_macro_char (ESC);
	  map = FUNCTION_TO_KEYMAP (map, ESC);
	  key = UNMETA (key);
	  rl_key_sequence_length += 2;
	  return (_rl_dispatch (key, map));
	}
      else
	ding ();
      return 0;
    }

  if (_rl_defining_kbd_macro)
    _rl_add_macro_char (key);

  r = 0;
  switch (map[key].type)
    {
    case ISFUNC:
      func = map[key].function;
      if (func != (Function *)NULL)
	{
	  /* Special case rl_do_lowercase_version (). */
	  if (func == rl_do_lowercase_version)
	    return (_rl_dispatch (_rl_to_lower (key), map));

	  rl_executing_keymap = map;

#if 0
	  _rl_suppress_redisplay = (map[key].function == rl_insert) && _rl_input_available ();
#endif

	  rl_dispatching = 1;
	  r = (*map[key].function)(rl_numeric_arg * rl_arg_sign, key);
	  rl_dispatching = 0;

	  /* If we have input pending, then the last command was a prefix
	     command.  Don't change the state of rl_last_func.  Otherwise,
	     remember the last command executed in this variable. */
	  if (!rl_pending_input && map[key].function != rl_digit_argument)
	    rl_last_func = map[key].function;
	}
      else
	{
	  _rl_abort_internal ();
	  return -1;
	}
      break;

    case ISKMAP:
      if (map[key].function != (Function *)NULL)
	{
	  rl_key_sequence_length++;
	  newkey = rl_read_key ();
	  r = _rl_dispatch (newkey, FUNCTION_TO_KEYMAP (map, key));
	}
      else
	{
	  _rl_abort_internal ();
	  return -1;
	}
      break;

    case ISMACR:
      if (map[key].function != (Function *)NULL)
	{
	  macro = savestring ((char *)map[key].function);
	  _rl_with_macro_input (macro);
	  return 0;
	}
      break;
    }
#if defined (VI_MODE)
  if (rl_editing_mode == vi_mode && _rl_keymap == vi_movement_keymap &&
      _rl_vi_textmod_command (key))
    _rl_vi_set_last (key, rl_numeric_arg, rl_arg_sign);
#endif
  return (r);
}

/* **************************************************************** */
/*								    */
/*			Initializations 			    */
/*								    */
/* **************************************************************** */

/* Initialize readline (and terminal if not already). */
int
rl_initialize ()
{
  /* If we have never been called before, initialize the
     terminal and data structures. */
  if (!rl_initialized)
    {
      readline_initialize_everything ();
      rl_initialized++;
    }

  /* Initalize the current line information. */
  _rl_init_line_state ();

  /* We aren't done yet.  We haven't even gotten started yet! */
  rl_done = 0;

  /* Tell the history routines what is going on. */
  start_using_history ();

  /* Make the display buffer match the state of the line. */
  rl_reset_line_state ();

  /* No such function typed yet. */
  rl_last_func = (Function *)NULL;

  /* Parsing of key-bindings begins in an enabled state. */
  _rl_parsing_conditionalized_out = 0;

#if defined (VI_MODE)
  if (rl_editing_mode == vi_mode)
    _rl_vi_initialize_line ();
#endif

  return 0;
}

#if defined (__EMX__)
static void
_emx_build_environ ()
{
  TIB *tibp;
  PIB *pibp;
  char *t, **tp;
  int c;

  DosGetInfoBlocks (&tibp, &pibp);
  t = pibp->pib_pchenv;
  for (c = 1; *t; c++)
    t += strlen (t) + 1;
  tp = environ = (char **)xmalloc ((c + 1) * sizeof (char *));
  t = pibp->pib_pchenv;
  while (*t)
    {
      *tp++ = t;
      t += strlen (t) + 1;
    }
  *tp = 0;
}
#endif /* __EMX__ */

/* Initialize the entire state of the world. */
static void
readline_initialize_everything ()
{
#if defined (__EMX__)
  if (environ == 0)
    _emx_build_environ ();
#endif

  /* Find out if we are running in Emacs. */
  running_in_emacs = get_env_value ("EMACS") != (char *)0;

  /* Set up input and output if they are not already set up. */
  if (!rl_instream)
    rl_instream = stdin;

  if (!rl_outstream)
    rl_outstream = stdout;

  /* Bind _rl_in_stream and _rl_out_stream immediately.  These values
     may change, but they may also be used before readline_internal ()
     is called. */
  _rl_in_stream = rl_instream;
  _rl_out_stream = rl_outstream;

  /* Allocate data structures. */
  if (rl_line_buffer == 0)
    rl_line_buffer = xmalloc (rl_line_buffer_len = DEFAULT_BUFFER_SIZE);

  /* Initialize the terminal interface. */
  _rl_init_terminal_io ((char *)NULL);

#if !defined (__GO32__)
  /* Bind tty characters to readline functions. */
  readline_default_bindings ();
#endif /* !__GO32__ */

  /* Initialize the function names. */
  rl_initialize_funmap ();

  /* Decide whether we should automatically go into eight-bit mode. */
  _rl_init_eightbit ();
      
  /* Read in the init file. */
  rl_read_init_file ((char *)NULL);

  /* XXX */
  if (_rl_horizontal_scroll_mode && _rl_term_autowrap)
    {
      screenwidth--;
      screenchars -= screenheight;
    }

  /* Override the effect of any `set keymap' assignments in the
     inputrc file. */
  rl_set_keymap_from_edit_mode ();

  /* Try to bind a common arrow key prefix, if not already bound. */
  bind_arrow_keys ();

  /* Enable the meta key, if this terminal has one. */
  if (_rl_enable_meta)
    _rl_enable_meta_key ();

  /* If the completion parser's default word break characters haven't
     been set yet, then do so now. */
  if (rl_completer_word_break_characters == (char *)NULL)
    rl_completer_word_break_characters = rl_basic_word_break_characters;
}

/* If this system allows us to look at the values of the regular
   input editing characters, then bind them to their readline
   equivalents, iff the characters are not bound to keymaps. */
static void
readline_default_bindings ()
{
  rltty_set_default_bindings (_rl_keymap);
}

static void
bind_arrow_keys_internal ()
{
  Function *f;

  f = rl_function_of_keyseq ("\033[A", _rl_keymap, (int *)NULL);
  if (!f || f == rl_do_lowercase_version)
    {
      _rl_bind_if_unbound ("\033[A", rl_get_previous_history);
      _rl_bind_if_unbound ("\033[B", rl_get_next_history);
      _rl_bind_if_unbound ("\033[C", rl_forward);
      _rl_bind_if_unbound ("\033[D", rl_backward);
    }

  f = rl_function_of_keyseq ("\033OA", _rl_keymap, (int *)NULL);
  if (!f || f == rl_do_lowercase_version)
    {
      _rl_bind_if_unbound ("\033OA", rl_get_previous_history);
      _rl_bind_if_unbound ("\033OB", rl_get_next_history);
      _rl_bind_if_unbound ("\033OC", rl_forward);
      _rl_bind_if_unbound ("\033OD", rl_backward);
    }
}

/* Try and bind the common arrow key prefix after giving termcap and
   the inputrc file a chance to bind them and create `real' keymaps
   for the arrow key prefix. */
static void
bind_arrow_keys ()
{
  Keymap xkeymap;

  xkeymap = _rl_keymap;

  _rl_keymap = emacs_standard_keymap;
  bind_arrow_keys_internal ();

#if defined (VI_MODE)
  _rl_keymap = vi_movement_keymap;
  bind_arrow_keys_internal ();
#endif

  _rl_keymap = xkeymap;
}


/* **************************************************************** */
/*								    */
/*			Numeric Arguments			    */
/*								    */
/* **************************************************************** */

/* Handle C-u style numeric args, as well as M--, and M-digits. */
static int
rl_digit_loop ()
{
  int key, c, sawminus, sawdigits;

  rl_save_prompt ();

  sawminus = sawdigits = 0;
  while (1)
    {
      if (rl_numeric_arg > 1000000)
	{
	  sawdigits = rl_explicit_arg = rl_numeric_arg = 0;
	  ding ();
	  rl_restore_prompt ();
	  rl_clear_message ();
	  return 1;
	}
      rl_message ("(arg: %d) ", rl_arg_sign * rl_numeric_arg);
      key = c = rl_read_key ();

      /* If we see a key bound to `universal-argument' after seeing digits,
	 it ends the argument but is otherwise ignored. */
      if (_rl_keymap[c].type == ISFUNC &&
	  _rl_keymap[c].function == rl_universal_argument)
	{
	  if (sawdigits == 0)
	    {
	      rl_numeric_arg *= 4;
	      continue;
	    }
	  else
	    {
	      key = rl_read_key ();
	      rl_restore_prompt ();
	      rl_clear_message ();
	      return (_rl_dispatch (key, _rl_keymap));
	    }
	}

      c = UNMETA (c);

      if (_rl_digit_p (c))
	{
	  rl_numeric_arg = rl_explicit_arg ? (rl_numeric_arg * 10) + c - '0' : c - '0';
	  sawdigits = rl_explicit_arg = 1;
	}
      else if (c == '-' && rl_explicit_arg == 0)
	{
	  rl_numeric_arg = sawminus = 1;
	  rl_arg_sign = -1;
	}
      else
	{
	  /* Make M-- command equivalent to M--1 command. */
	  if (sawminus && rl_numeric_arg == 1 && rl_explicit_arg == 0)
	    rl_explicit_arg = 1;
	  rl_restore_prompt ();
	  rl_clear_message ();
	  return (_rl_dispatch (key, _rl_keymap));
	}
    }

  return 0;
}

/* Add the current digit to the argument in progress. */
int
rl_digit_argument (ignore, key)
     int ignore __attribute__((unused)), key;
{
  rl_pending_input = key;
  return (rl_digit_loop ());
}

/* What to do when you abort reading an argument. */
int
rl_discard_argument ()
{
  ding ();
  rl_clear_message ();
  _rl_init_argument ();
  return 0;
}

/* Create a default argument. */
int
_rl_init_argument ()
{
  rl_numeric_arg = rl_arg_sign = 1;
  rl_explicit_arg = 0;
  return 0;
}

/* C-u, universal argument.  Multiply the current argument by 4.
   Read a key.  If the key has nothing to do with arguments, then
   dispatch on it.  If the key is the abort character then abort. */
int
rl_universal_argument (count, key)
     int count __attribute__((unused)), key __attribute__((unused));
{
  rl_numeric_arg *= 4;
  return (rl_digit_loop ());
}

/* **************************************************************** */
/*								    */
/*			Insert and Delete			    */
/*								    */
/* **************************************************************** */

/* Insert a string of text into the line at point.  This is the only
   way that you should do insertion.  rl_insert () calls this
   function. */
int
rl_insert_text (string)
     char *string;
{
  register int i, l = strlen (string);

  if (rl_end + l >= rl_line_buffer_len)
    rl_extend_line_buffer (rl_end + l);

  for (i = rl_end; i >= rl_point; i--)
    the_line[i + l] = the_line[i];
  strncpy (the_line + rl_point, string, l);

  /* Remember how to undo this if we aren't undoing something. */
  if (!_rl_doing_an_undo)
    {
      /* If possible and desirable, concatenate the undos. */
      if ((l == 1) &&
	  rl_undo_list &&
	  (rl_undo_list->what == UNDO_INSERT) &&
	  (rl_undo_list->end == rl_point) &&
	  (rl_undo_list->end - rl_undo_list->start < 20))
	rl_undo_list->end++;
      else
	rl_add_undo (UNDO_INSERT, rl_point, rl_point + l, (char *)NULL);
    }
  rl_point += l;
  rl_end += l;
  the_line[rl_end] = '\0';
  return l;
}

/* Delete the string between FROM and TO.  FROM is
   inclusive, TO is not. */
int
rl_delete_text (from, to)
     int from, to;
{
  register char *text;
  register int diff, i;

  /* Fix it if the caller is confused. */
  if (from > to)
    SWAP (from, to);

  /* fix boundaries */
  if (to > rl_end)
    {
      to = rl_end;
      if (from > to)
        from = to;
    }

  text = rl_copy_text (from, to);

  /* Some versions of strncpy() can't handle overlapping arguments. */
  diff = to - from;
  for (i = from; i < rl_end - diff; i++)
    the_line[i] = the_line[i + diff];

  /* Remember how to undo this delete. */
  if (_rl_doing_an_undo == 0)
    rl_add_undo (UNDO_DELETE, from, to, text);
  else
    free (text);

  rl_end -= diff;
  the_line[rl_end] = '\0';
  return (diff);
}

/* Fix up point so that it is within the line boundaries after killing
   text.  If FIX_MARK_TOO is non-zero, the mark is forced within line
   boundaries also. */

#define _RL_FIX_POINT(x) \
	do { \
	if (x > rl_end) \
	  x = rl_end; \
	else if (x < 0) \
	  x = 0; \
	} while (0)

void
_rl_fix_point (fix_mark_too)
     int fix_mark_too;
{
  _RL_FIX_POINT (rl_point);
  if (fix_mark_too)
    _RL_FIX_POINT (rl_mark);
}
#undef _RL_FIX_POINT

void
_rl_replace_text (text, start, end)
     char *text;
     int start, end;
{
  rl_begin_undo_group ();
  rl_delete_text (start, end + 1);
  rl_point = start;
  rl_insert_text (text);
  rl_end_undo_group ();
}

/* **************************************************************** */
/*								    */
/*			Readline character functions		    */
/*								    */
/* **************************************************************** */

/* This is not a gap editor, just a stupid line input routine.  No hair
   is involved in writing any of the functions, and none should be. */

/* Note that:

   rl_end is the place in the string that we would place '\0';
   i.e., it is always safe to place '\0' there.

   rl_point is the place in the string where the cursor is.  Sometimes
   this is the same as rl_end.

   Any command that is called interactively receives two arguments.
   The first is a count: the numeric arg pased to this command.
   The second is the key which invoked this command.
*/

/* **************************************************************** */
/*								    */
/*			Movement Commands			    */
/*								    */
/* **************************************************************** */

/* Note that if you `optimize' the display for these functions, you cannot
   use said functions in other functions which do not do optimizing display.
   I.e., you will have to update the data base for rl_redisplay, and you
   might as well let rl_redisplay do that job. */

/* Move forward COUNT characters. */
int
rl_forward (count, key)
     int count, key;
{
  if (count < 0)
    rl_backward (-count, key);
  else if (count > 0)
    {
      int end = rl_point + count;
#if defined (VI_MODE)
      int lend = rl_end - (rl_editing_mode == vi_mode);
#else
      int lend = rl_end;
#endif

      if (end > lend)
	{
	  rl_point = lend;
	  ding ();
	}
      else
	rl_point = end;
    }
  return 0;
}

/* Move backward COUNT characters. */
int
rl_backward (count, key)
     int count, key;
{
  if (count < 0)
    rl_forward (-count, key);
  else if (count > 0)
    {
      if (rl_point < count)
	{
	  rl_point = 0;
	  ding ();
	}
      else
        rl_point -= count;
    }
  return 0;
}

/* Move to the beginning of the line. */
int
rl_beg_of_line (count, key)
     int count __attribute__((unused)), key __attribute__((unused));
{
  rl_point = 0;
  return 0;
}

/* Move to the end of the line. */
int
rl_end_of_line (count, key)
     int count __attribute__((unused)), key __attribute__((unused));
{
  rl_point = rl_end;
  return 0;
}

/* Move forward a word.  We do what Emacs does. */
int
rl_forward_word (count, key)
     int count, key;
{
  int c;

  if (count < 0)
    {
      rl_backward_word (-count, key);
      return 0;
    }

  while (count)
    {
      if (rl_point == rl_end)
	return 0;

      /* If we are not in a word, move forward until we are in one.
	 Then, move forward until we hit a non-alphabetic character. */
      c = the_line[rl_point];
      if (alphabetic (c) == 0)
	{
	  while (++rl_point < rl_end)
	    {
	      c = the_line[rl_point];
	      if (alphabetic (c))
		break;
	    }
	}
      if (rl_point == rl_end)
	return 0;
      while (++rl_point < rl_end)
	{
	  c = the_line[rl_point];
	  if (alphabetic (c) == 0)
	    break;
	}
      --count;
    }
  return 0;
}

/* Move backward a word.  We do what Emacs does. */
int
rl_backward_word (count, key)
     int count, key;
{
  int c;

  if (count < 0)
    {
      rl_forward_word (-count, key);
      return 0;
    }

  while (count)
    {
      if (!rl_point)
	return 0;

      /* Like rl_forward_word (), except that we look at the characters
	 just before point. */

      c = the_line[rl_point - 1];
      if (alphabetic (c) == 0)
	{
	  while (--rl_point)
	    {
	      c = the_line[rl_point - 1];
	      if (alphabetic (c))
		break;
	    }
	}

      while (rl_point)
	{
	  c = the_line[rl_point - 1];
	  if (alphabetic (c) == 0)
	    break;
	  else
	    --rl_point;
	}
      --count;
    }
  return 0;
}

/* Clear the current line.  Numeric argument to C-l does this. */
int
rl_refresh_line (ignore1, ignore2)
     int ignore1 __attribute__((unused)), ignore2 __attribute__((unused));
{
  int curr_line, nleft;

  /* Find out whether or not there might be invisible characters in the
     editing buffer. */
  if (rl_display_prompt == rl_prompt)
    nleft = _rl_last_c_pos - screenwidth - rl_visible_prompt_length;
  else
    nleft = _rl_last_c_pos - screenwidth;

  if (nleft > 0)
    curr_line = 1 + nleft / screenwidth;
  else
    curr_line = 0;

  _rl_move_vert (curr_line);
  _rl_move_cursor_relative (0, the_line);   /* XXX is this right */

#if defined (__GO32__)
  {
    int row, col, width, row_start;

    ScreenGetCursor (&row, &col);
    width = ScreenCols ();
    row_start = ScreenPrimary + (row * width);
    memset (row_start + col, 0, (width - col) * 2);
  }
#else /* !__GO32__ */
  _rl_clear_to_eol (0);		/* arg of 0 means to not use spaces */
#endif /* !__GO32__ */

  rl_forced_update_display ();
  rl_display_fixed = 1;

  return 0;
}

/* C-l typed to a line without quoting clears the screen, and then reprints
   the prompt and the current input line.  Given a numeric arg, redraw only
   the current line. */
int
rl_clear_screen (count, key)
     int count, key;
{
  if (rl_explicit_arg)
    {
      rl_refresh_line (count, key);
      return 0;
    }

  _rl_clear_screen ();		/* calls termcap function to clear screen */
  rl_forced_update_display ();
  rl_display_fixed = 1;

  return 0;
}

int
rl_arrow_keys (count, c)
     int count, c __attribute__((unused));
{
  int ch;

  ch = rl_read_key ();

  switch (_rl_to_upper (ch))
    {
    case 'A':
      rl_get_previous_history (count, ch);
      break;

    case 'B':
      rl_get_next_history (count, ch);
      break;

    case 'C':
      rl_forward (count, ch);
      break;

    case 'D':
      rl_backward (count, ch);
      break;

    default:
      ding ();
    }
  return 0;
}


/* **************************************************************** */
/*								    */
/*			Text commands				    */
/*								    */
/* **************************************************************** */

/* Insert the character C at the current location, moving point forward. */
int
rl_insert (count, c)
     int count, c;
{
  register int i;
  char *string;

  if (count <= 0)
    return 0;

  /* If we can optimize, then do it.  But don't let people crash
     readline because of extra large arguments. */
  if (count > 1 && count <= 1024)
    {
      string = xmalloc (1 + count);

      for (i = 0; i < count; i++)
	string[i] = c;

      string[i] = '\0';
      rl_insert_text (string);
      free (string);

      return 0;
    }

  if (count > 1024)
    {
      int decreaser;
      char str[1024+1];

      for (i = 0; i < 1024; i++)
	str[i] = c;

      while (count)
	{
	  decreaser = (count > 1024 ? 1024 : count);
	  str[decreaser] = '\0';
	  rl_insert_text (str);
	  count -= decreaser;
	}

      return 0;
    }

  /* We are inserting a single character.
     If there is pending input, then make a string of all of the
     pending characters that are bound to rl_insert, and insert
     them all. */
  if (_rl_any_typein ())
    _rl_insert_typein (c);
  else
    {
      /* Inserting a single character. */
      char str[2];

      str[1] = '\0';
      str[0] = c;
      rl_insert_text (str);
    }
  return 0;
}

/* Insert the next typed character verbatim. */
int
rl_quoted_insert (count, key)
     int count, key __attribute__((unused));
{
  int c;

  c = rl_read_key ();
  return (rl_insert (count, c));  
}

/* Insert a tab character. */
int
rl_tab_insert (count, key)
     int count, key __attribute__((unused));
{
  return (rl_insert (count, '\t'));
}

/* What to do when a NEWLINE is pressed.  We accept the whole line.
   KEY is the key that invoked this command.  I guess it could have
   meaning in the future. */
int
rl_newline (count, key)
     int count __attribute__((unused)), key __attribute__((unused));
{
  rl_done = 1;

#if defined (VI_MODE)
  if (rl_editing_mode == vi_mode)
    {
      _rl_vi_done_inserting ();
      _rl_vi_reset_last ();
    }
#endif /* VI_MODE */

  /* If we've been asked to erase empty lines, suppress the final update,
     since _rl_update_final calls crlf(). */
  if (rl_erase_empty_line && rl_point == 0 && rl_end == 0)
    return 0;

  if (readline_echoing_p)
    _rl_update_final ();
  return 0;
}

/* What to do for some uppercase characters, like meta characters,
   and some characters appearing in emacs_ctlx_keymap.  This function
   is just a stub, you bind keys to it and the code in _rl_dispatch ()
   is special cased. */
int
rl_do_lowercase_version (ignore1, ignore2)
     int ignore1 __attribute__((unused)), ignore2 __attribute__((unused));
{
  return 0;
}

/* Rubout the character behind point. */
int
rl_rubout (count, key)
     int count, key;
{
  if (count < 0)
    {
      rl_delete (-count, key);
      return 0;
    }

  if (!rl_point)
    {
      ding ();
      return -1;
    }

  if (count > 1 || rl_explicit_arg)
    {
      int orig_point = rl_point;
      rl_backward (count, key);
      rl_kill_text (orig_point, rl_point);
    }
  else
    {
      int c = the_line[--rl_point];
      rl_delete_text (rl_point, rl_point + 1);

      if (rl_point == rl_end && isprint (c) && _rl_last_c_pos)
	{
	  int l;
	  l = rl_character_len (c, rl_point);
	  _rl_erase_at_end_of_line (l);
	}
    }
  return 0;
}

/* Delete the character under the cursor.  Given a numeric argument,
   kill that many characters instead. */
int
rl_delete (count, key)
     int count, key;
{
  if (count < 0)
    return (rl_rubout (-count, key));

  if (rl_point == rl_end)
    {
      ding ();
      return -1;
    }

  if (count > 1 || rl_explicit_arg)
    {
      int orig_point = rl_point;
      rl_forward (count, key);
      rl_kill_text (orig_point, rl_point);
      rl_point = orig_point;
      return 0;
    }
  else
    return (rl_delete_text (rl_point, rl_point + 1));
}

/* Delete the character under the cursor, unless the insertion
   point is at the end of the line, in which case the character
   behind the cursor is deleted.  COUNT is obeyed and may be used
   to delete forward or backward that many characters. */      
int
rl_rubout_or_delete (count, key)
     int count, key;
{
  if (rl_end != 0 && rl_point == rl_end)
    return (rl_rubout (count, key));
  else
    return (rl_delete (count, key));
}  

/* Delete all spaces and tabs around point. */
int
rl_delete_horizontal_space (count, ignore)
     int count __attribute__((unused)), ignore __attribute__((unused));
{
  int start = rl_point;

  while (rl_point && whitespace (the_line[rl_point - 1]))
    rl_point--;

  start = rl_point;

  while (rl_point < rl_end && whitespace (the_line[rl_point]))
    rl_point++;

  if (start != rl_point)
    {
      rl_delete_text (start, rl_point);
      rl_point = start;
    }
  return 0;
}

/* Like the tcsh editing function delete-char-or-list.  The eof character
   is caught before this is invoked, so this really does the same thing as
   delete-char-or-list-or-eof, as long as it's bound to the eof character. */
int
rl_delete_or_show_completions (count, key)
     int count, key;
{
  if (rl_end != 0 && rl_point == rl_end)
    return (rl_possible_completions (count, key));
  else
    return (rl_delete (count, key));
}

#ifndef RL_COMMENT_BEGIN_DEFAULT
#define RL_COMMENT_BEGIN_DEFAULT "#"
#endif

/* Turn the current line into a comment in shell history.
   A K*rn shell style function. */
int
rl_insert_comment (count, key)
     int count __attribute__((unused)), key;
{
  rl_beg_of_line (1, key);
  rl_insert_text (_rl_comment_begin ? _rl_comment_begin
				    : RL_COMMENT_BEGIN_DEFAULT);
  (*rl_redisplay_function) ();
  rl_newline (1, '\n');
  return (0);
}

/* **************************************************************** */
/*								    */
/*			Changing Case				    */
/*								    */
/* **************************************************************** */

/* The three kinds of things that we know how to do. */
#define UpCase 1
#define DownCase 2
#define CapCase 3

static int rl_change_case ();

/* Uppercase the word at point. */
int
rl_upcase_word (count, key)
     int count, key __attribute__((unused));
{
  return (rl_change_case (count, UpCase));
}

/* Lowercase the word at point. */
int
rl_downcase_word (count, key)
     int count, key __attribute__((unused));
{
  return (rl_change_case (count, DownCase));
}

/* Upcase the first letter, downcase the rest. */
int
rl_capitalize_word (count, key)
     int count, key __attribute__((unused));
{
 return (rl_change_case (count, CapCase));
}

/* The meaty function.
   Change the case of COUNT words, performing OP on them.
   OP is one of UpCase, DownCase, or CapCase.
   If a negative argument is given, leave point where it started,
   otherwise, leave it where it moves to. */
static int
rl_change_case (count, op)
     int count, op;
{
  register int start, end;
  int inword, c;

  start = rl_point;
  rl_forward_word (count, 0);
  end = rl_point;

  if (count < 0)
    SWAP (start, end);

  /* We are going to modify some text, so let's prepare to undo it. */
  rl_modifying (start, end);

  for (inword = 0; start < end; start++)
    {
      c = the_line[start];
      switch (op)
	{
	case UpCase:
	  the_line[start] = _rl_to_upper (c);
	  break;

	case DownCase:
	  the_line[start] = _rl_to_lower (c);
	  break;

	case CapCase:
	  the_line[start] = (inword == 0) ? _rl_to_upper (c) : _rl_to_lower (c);
	  inword = alphabetic (the_line[start]);
	  break;

	default:
	  ding ();
	  return -1;
	}
    }
  rl_point = end;
  return 0;
}

/* **************************************************************** */
/*								    */
/*			Transposition				    */
/*								    */
/* **************************************************************** */

/* Transpose the words at point. */
int
rl_transpose_words (count, key)
     int count, key;
{
  char *word1, *word2;
  int w1_beg, w1_end, w2_beg, w2_end;
  int orig_point = rl_point;

  if (!count)
    return 0;

  /* Find the two words. */
  rl_forward_word (count, key);
  w2_end = rl_point;
  rl_backward_word (1, key);
  w2_beg = rl_point;
  rl_backward_word (count, key);
  w1_beg = rl_point;
  rl_forward_word (1, key);
  w1_end = rl_point;

  /* Do some check to make sure that there really are two words. */
  if ((w1_beg == w2_beg) || (w2_beg < w1_end))
    {
      ding ();
      rl_point = orig_point;
      return -1;
    }

  /* Get the text of the words. */
  word1 = rl_copy_text (w1_beg, w1_end);
  word2 = rl_copy_text (w2_beg, w2_end);

  /* We are about to do many insertions and deletions.  Remember them
     as one operation. */
  rl_begin_undo_group ();

  /* Do the stuff at word2 first, so that we don't have to worry
     about word1 moving. */
  rl_point = w2_beg;
  rl_delete_text (w2_beg, w2_end);
  rl_insert_text (word1);

  rl_point = w1_beg;
  rl_delete_text (w1_beg, w1_end);
  rl_insert_text (word2);

  /* This is exactly correct since the text before this point has not
     changed in length. */
  rl_point = w2_end;

  /* I think that does it. */
  rl_end_undo_group ();
  free (word1);
  free (word2);

  return 0;
}

/* Transpose the characters at point.  If point is at the end of the line,
   then transpose the characters before point. */
int
rl_transpose_chars (count, key)
     int count, key __attribute__((unused));
{
  char dummy[2];

  if (!count)
    return 0;

  if (!rl_point || rl_end < 2)
    {
      ding ();
      return -1;
    }

  rl_begin_undo_group ();

  if (rl_point == rl_end)
    {
      --rl_point;
      count = 1;
    }
  rl_point--;

  dummy[0] = the_line[rl_point];
  dummy[1] = '\0';

  rl_delete_text (rl_point, rl_point + 1);

  rl_point += count;
  _rl_fix_point (0);
  rl_insert_text (dummy);

  rl_end_undo_group ();
  return 0;
}

/* **************************************************************** */
/*								    */
/*			Character Searching			    */
/*								    */
/* **************************************************************** */

int
_rl_char_search_internal (count, dir, schar)
     int count, dir, schar;
{
  int pos, inc;

  pos = rl_point;
  inc = (dir < 0) ? -1 : 1;
  while (count)
    {
      if ((dir < 0 && pos <= 0) || (dir > 0 && pos >= rl_end))
	{
	  ding ();
	  return -1;
	}

      pos += inc;
      do
	{
	  if (rl_line_buffer[pos] == schar)
	    {
	      count--;
	      if (dir < 0)
	        rl_point = (dir == BTO) ? pos + 1 : pos;
	      else
		rl_point = (dir == FTO) ? pos - 1 : pos;
	      break;
	    }
	}
      while ((dir < 0) ? pos-- : ++pos < rl_end);
    }
  return (0);
}

/* Search COUNT times for a character read from the current input stream.
   FDIR is the direction to search if COUNT is non-negative; otherwise
   the search goes in BDIR. */
static int
_rl_char_search (count, fdir, bdir)
     int count, fdir, bdir;
{
  int c;

  c = rl_read_key ();
  if (count < 0)
    return (_rl_char_search_internal (-count, bdir, c));
  else
    return (_rl_char_search_internal (count, fdir, c));
}

int
rl_char_search (count, key)
     int count, key __attribute__((unused));
{
  return (_rl_char_search (count, FFIND, BFIND));
}

int
rl_backward_char_search (count, key)
     int count, key __attribute__((unused));
{
  return (_rl_char_search (count, BFIND, FFIND));
}

/* **************************************************************** */
/*								    */
/*			History Utilities			    */
/*								    */
/* **************************************************************** */

/* We already have a history library, and that is what we use to control
   the history features of readline.  This is our local interface to
   the history mechanism. */

/* While we are editing the history, this is the saved
   version of the original line. */
HIST_ENTRY *saved_line_for_history = (HIST_ENTRY *)NULL;

/* Set the history pointer back to the last entry in the history. */
static void
start_using_history ()
{
  using_history ();
  if (saved_line_for_history)
    _rl_free_history_entry (saved_line_for_history);

  saved_line_for_history = (HIST_ENTRY *)NULL;
}

/* Free the contents (and containing structure) of a HIST_ENTRY. */
void
_rl_free_history_entry (entry)
     HIST_ENTRY *entry;
{
  if (entry == 0)
    return;
  if (entry->line)
    free (entry->line);
  free (entry);
}

/* Perhaps put back the current line if it has changed. */
int
maybe_replace_line ()
{
  HIST_ENTRY *temp;

  temp = current_history ();
  /* If the current line has changed, save the changes. */
  if (temp && ((UNDO_LIST *)(temp->data) != rl_undo_list))
    {
      temp = replace_history_entry (where_history (), the_line, (histdata_t)rl_undo_list);
      free (temp->line);
      free (temp);
    }
  return 0;
}

/* Put back the saved_line_for_history if there is one. */
int
maybe_unsave_line ()
{
  int line_len;

  if (saved_line_for_history)
    {
      line_len = strlen (saved_line_for_history->line);

      if (line_len >= rl_line_buffer_len)
	rl_extend_line_buffer (line_len);

      strcpy (the_line, saved_line_for_history->line);
      rl_undo_list = (UNDO_LIST *)saved_line_for_history->data;
      _rl_free_history_entry (saved_line_for_history);
      saved_line_for_history = (HIST_ENTRY *)NULL;
      rl_end = rl_point = strlen (the_line);
    }
  else
    ding ();
  return 0;
}

/* Save the current line in saved_line_for_history. */
int
maybe_save_line ()
{
  if (saved_line_for_history == 0)
    {
      saved_line_for_history = (HIST_ENTRY *)xmalloc (sizeof (HIST_ENTRY));
      saved_line_for_history->line = savestring (the_line);
      saved_line_for_history->data = (char *)rl_undo_list;
    }
  return 0;
}

/* **************************************************************** */
/*								    */
/*			History Commands			    */
/*								    */
/* **************************************************************** */

/* Meta-< goes to the start of the history. */
int
rl_beginning_of_history (count, key)
     int count __attribute__((unused)), key;
{
  return (rl_get_previous_history (1 + where_history (), key));
}

/* Meta-> goes to the end of the history.  (The current line). */
int
rl_end_of_history (count, key)
     int count __attribute__((unused)), key __attribute__((unused));
{
  maybe_replace_line ();
  using_history ();
  maybe_unsave_line ();
  return 0;
}

/* Move down to the next history line. */
int
rl_get_next_history (count, key)
     int count, key;
{
  HIST_ENTRY *temp;
  int line_len;

  if (count < 0)
    return (rl_get_previous_history (-count, key));

  if (count == 0)
    return 0;

  maybe_replace_line ();

  temp = (HIST_ENTRY *)NULL;
  while (count)
    {
      temp = next_history ();
      if (!temp)
	break;
      --count;
    }

  if (temp == 0)
    maybe_unsave_line ();
  else
    {
      line_len = strlen (temp->line);

      if (line_len >= rl_line_buffer_len)
	rl_extend_line_buffer (line_len);

      strcpy (the_line, temp->line);
      rl_undo_list = (UNDO_LIST *)temp->data;
      rl_end = rl_point = strlen (the_line);
#if defined (VI_MODE)
      if (rl_editing_mode == vi_mode)
	rl_point = 0;
#endif /* VI_MODE */
    }
  return 0;
}

/* Get the previous item out of our interactive history, making it the current
   line.  If there is no previous history, just ding. */
int
rl_get_previous_history (count, key)
     int count, key;
{
  HIST_ENTRY *old_temp, *temp;
  int line_len;

  if (count < 0)
    return (rl_get_next_history (-count, key));

  if (count == 0)
    return 0;

  /* If we don't have a line saved, then save this one. */
  maybe_save_line ();

  /* If the current line has changed, save the changes. */
  maybe_replace_line ();

  temp = old_temp = (HIST_ENTRY *)NULL;
  while (count)
    {
      temp = previous_history ();
      if (temp == 0)
	break;

      old_temp = temp;
      --count;
    }

  /* If there was a large argument, and we moved back to the start of the
     history, that is not an error.  So use the last value found. */
  if (!temp && old_temp)
    temp = old_temp;

  if (temp == 0)
    ding ();
  else
    {
      line_len = strlen (temp->line);

      if (line_len >= rl_line_buffer_len)
	rl_extend_line_buffer (line_len);

      strcpy (the_line, temp->line);
      rl_undo_list = (UNDO_LIST *)temp->data;
      rl_end = rl_point = line_len;

#if defined (VI_MODE)
      if (rl_editing_mode == vi_mode)
	rl_point = 0;
#endif /* VI_MODE */
    }
  return 0;
}

/* **************************************************************** */
/*								    */
/*		   The Mark and the Region.			    */
/*								    */
/* **************************************************************** */

/* Set the mark at POSITION. */
int
_rl_set_mark_at_pos (position)
     int position;
{
  if (position > rl_end)
    return -1;

  rl_mark = position;
  return 0;
}

/* A bindable command to set the mark. */
int
rl_set_mark (count, key)
     int count, key __attribute__((unused));
{
  return (_rl_set_mark_at_pos (rl_explicit_arg ? count : rl_point));
}

/* Exchange the position of mark and point. */
int
rl_exchange_point_and_mark (count, key)
     int count __attribute__((unused)), key __attribute__((unused));
{
  if (rl_mark > rl_end)
    rl_mark = -1;

  if (rl_mark == -1)
    {
      ding ();
      return -1;
    }
  else
    SWAP (rl_point, rl_mark);

  return 0;
}

/* **************************************************************** */
/*								    */
/*			    Editing Modes			    */
/*								    */
/* **************************************************************** */
/* How to toggle back and forth between editing modes. */
int
rl_vi_editing_mode (count, key)
     int count __attribute__((unused)), key;
{
#if defined (VI_MODE)
  rl_editing_mode = vi_mode;
  rl_vi_insertion_mode (1, key);
#endif /* VI_MODE */
  return 0;
}

int
rl_emacs_editing_mode (count, key)
     int count __attribute__((unused)), key __attribute__((unused));
{
  rl_editing_mode = emacs_mode;
  _rl_keymap = emacs_standard_keymap;
  return 0;
}
