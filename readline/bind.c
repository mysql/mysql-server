/* bind.c -- key binding and startup file support for the readline library. */

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

#include <stdio.h>
#include <sys/types.h>
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

#include <signal.h>
#include <errno.h>

#if !defined (errno)
extern int errno;
#endif /* !errno */

#include "posixstat.h"

/* System-specific feature definitions and include files. */
#include "rldefs.h"

/* Some standard library routines. */
#include "readline.h"
#include "history.h"

#if !defined (strchr) && !defined (__STDC__)
extern char *strchr (), *strrchr ();
#endif /* !strchr && !__STDC__ */

extern int _rl_horizontal_scroll_mode;
extern int _rl_mark_modified_lines;
extern int _rl_bell_preference;
extern int _rl_meta_flag;
extern int _rl_convert_meta_chars_to_ascii;
extern int _rl_output_meta_chars;
extern int _rl_complete_show_all;
extern int _rl_complete_mark_directories;
extern int _rl_print_completions_horizontally;
extern int _rl_completion_case_fold;
extern int _rl_enable_keypad;
#if defined (PAREN_MATCHING)
extern int rl_blink_matching_paren;
#endif /* PAREN_MATCHING */
#if defined (VISIBLE_STATS)
extern int rl_visible_stats;
#endif /* VISIBLE_STATS */
extern int rl_complete_with_tilde_expansion;
extern int rl_completion_query_items;
extern int rl_inhibit_completion;
extern char *_rl_comment_begin;
extern unsigned char *_rl_isearch_terminators;

extern int rl_explicit_arg;
extern int rl_editing_mode;
extern unsigned char _rl_parsing_conditionalized_out;
extern Keymap _rl_keymap;

extern char *possible_control_prefixes[], *possible_meta_prefixes[];

/* Functions imported from funmap.c */
extern char **rl_funmap_names ();
extern int rl_add_funmap_entry ();

/* Functions imported from util.c */
extern char *_rl_strindex ();

/* Functions imported from shell.c */
extern char *get_env_value ();

/* Variables exported by this file. */
Keymap rl_binding_keymap;

/* Forward declarations */
void rl_set_keymap_from_edit_mode ();

static int _rl_read_init_file ();
static int glean_key_from_name ();
static int substring_member_of_array ();

extern char *xmalloc (), *xrealloc ();

/* **************************************************************** */
/*								    */
/*			Binding keys				    */
/*								    */
/* **************************************************************** */

/* rl_add_defun (char *name, Function *function, int key)
   Add NAME to the list of named functions.  Make FUNCTION be the function
   that gets called.  If KEY is not -1, then bind it. */
int
rl_add_defun (name, function, key)
     char *name;
     Function *function;
     int key;
{
  if (key != -1)
    rl_bind_key (key, function);
  rl_add_funmap_entry (name, function);
  return 0;
}

/* Bind KEY to FUNCTION.  Returns non-zero if KEY is out of range. */
int
rl_bind_key (key, function)
     int key;
     Function *function;
{
  if (key < 0)
    return (key);

  if (META_CHAR (key) && _rl_convert_meta_chars_to_ascii)
    {
      if (_rl_keymap[ESC].type == ISKMAP)
	{
	  Keymap escmap;

	  escmap = FUNCTION_TO_KEYMAP (_rl_keymap, ESC);
	  key = UNMETA (key);
	  escmap[key].type = ISFUNC;
	  escmap[key].function = function;
	  return (0);
	}
      return (key);
    }

  _rl_keymap[key].type = ISFUNC;
  _rl_keymap[key].function = function;
  rl_binding_keymap = _rl_keymap;
  return (0);
}

/* Bind KEY to FUNCTION in MAP.  Returns non-zero in case of invalid
   KEY. */
int
rl_bind_key_in_map (key, function, map)
     int key;
     Function *function;
     Keymap map;
{
  int result;
  Keymap oldmap;

  oldmap = _rl_keymap;
  _rl_keymap = map;
  result = rl_bind_key (key, function);
  _rl_keymap = oldmap;
  return (result);
}

/* Make KEY do nothing in the currently selected keymap.
   Returns non-zero in case of error. */
int
rl_unbind_key (key)
     int key;
{
  return (rl_bind_key (key, (Function *)NULL));
}

/* Make KEY do nothing in MAP.
   Returns non-zero in case of error. */
int
rl_unbind_key_in_map (key, map)
     int key;
     Keymap map;
{
  return (rl_bind_key_in_map (key, (Function *)NULL, map));
}

/* Unbind all keys bound to FUNCTION in MAP. */
int
rl_unbind_function_in_map (func, map)
     Function *func;
     Keymap map;
{
  register int i, rval;

  for (i = rval = 0; i < KEYMAP_SIZE; i++)
    {
      if (map[i].type == ISFUNC && map[i].function == func)
	{
	  map[i].function = (Function *)NULL;
	  rval = 1;
	}
    }
  return rval;
}

int
rl_unbind_command_in_map (command, map)
     char *command;
     Keymap map;
{
  Function *func;

  func = rl_named_function (command);
  if (func == 0)
    return 0;
  return (rl_unbind_function_in_map (func, map));
}

/* Bind the key sequence represented by the string KEYSEQ to
   FUNCTION.  This makes new keymaps as necessary.  The initial
   place to do bindings is in MAP. */
int
rl_set_key (keyseq, function, map)
     char *keyseq;
     Function *function;
     Keymap map;
{
  return (rl_generic_bind (ISFUNC, keyseq, (char *)function, map));
}

/* Bind the key sequence represented by the string KEYSEQ to
   the string of characters MACRO.  This makes new keymaps as
   necessary.  The initial place to do bindings is in MAP. */
int
rl_macro_bind (keyseq, macro, map)
     char *keyseq, *macro;
     Keymap map;
{
  char *macro_keys;
  int macro_keys_len;

  macro_keys = (char *)xmalloc ((2 * strlen (macro)) + 1);

  if (rl_translate_keyseq (macro, macro_keys, &macro_keys_len))
    {
      free (macro_keys);
      return -1;
    }
  rl_generic_bind (ISMACR, keyseq, macro_keys, map);
  return 0;
}

/* Bind the key sequence represented by the string KEYSEQ to
   the arbitrary pointer DATA.  TYPE says what kind of data is
   pointed to by DATA, right now this can be a function (ISFUNC),
   a macro (ISMACR), or a keymap (ISKMAP).  This makes new keymaps
   as necessary.  The initial place to do bindings is in MAP. */
int
rl_generic_bind (type, keyseq, data, map)
     int type;
     char *keyseq, *data;
     Keymap map;
{
  char *keys;
  int keys_len;
  register int i;

  /* If no keys to bind to, exit right away. */
  if (!keyseq || !*keyseq)
    {
      if (type == ISMACR)
	free (data);
      return -1;
    }

  keys = xmalloc (1 + (2 * strlen (keyseq)));

  /* Translate the ASCII representation of KEYSEQ into an array of
     characters.  Stuff the characters into KEYS, and the length of
     KEYS into KEYS_LEN. */
  if (rl_translate_keyseq (keyseq, keys, &keys_len))
    {
      free (keys);
      return -1;
    }

  /* Bind keys, making new keymaps as necessary. */
  for (i = 0; i < keys_len; i++)
    {
      int ic = (int) ((unsigned char)keys[i]);

      if (_rl_convert_meta_chars_to_ascii && META_CHAR (ic))
	{
	  ic = UNMETA (ic);
	  if (map[ESC].type == ISKMAP)
	    map = FUNCTION_TO_KEYMAP (map, ESC);
	}

      if ((i + 1) < keys_len)
	{
	  if (map[ic].type != ISKMAP)
	    {
	      if (map[ic].type == ISMACR)
		free ((char *)map[ic].function);

	      map[ic].type = ISKMAP;
	      map[ic].function = KEYMAP_TO_FUNCTION (rl_make_bare_keymap());
	    }
	  map = FUNCTION_TO_KEYMAP (map, ic);
	}
      else
	{
	  if (map[ic].type == ISMACR)
	    free ((char *)map[ic].function);

	  map[ic].function = KEYMAP_TO_FUNCTION (data);
	  map[ic].type = type;
	}

      rl_binding_keymap = map;
    }
  free (keys);
  return 0;
}

/* Translate the ASCII representation of SEQ, stuffing the values into ARRAY,
   an array of characters.  LEN gets the final length of ARRAY.  Return
   non-zero if there was an error parsing SEQ. */
int
rl_translate_keyseq (seq, array, len)
     char *seq, *array;
     int *len;
{
  register int i, c, l, temp;

  for (i = l = 0; c = seq[i]; i++)
    {
      if (c == '\\')
	{
	  c = seq[++i];

	  if (c == 0)
	    break;

	  /* Handle \C- and \M- prefixes. */
	  if ((c == 'C' || c == 'M') && seq[i + 1] == '-')
	    {
	      /* Handle special case of backwards define. */
	      if (strncmp (&seq[i], "C-\\M-", 5) == 0)
		{
		  array[l++] = ESC;
		  i += 5;
		  array[l++] = CTRL (_rl_to_upper (seq[i]));
		  if (seq[i] == '\0')
		    i--;
		}
	      else if (c == 'M')
		{
		  i++;
		  array[l++] = ESC;	/* XXX */
		}
	      else if (c == 'C')
		{
		  i += 2;
		  /* Special hack for C-?... */
		  array[l++] = (seq[i] == '?') ? RUBOUT : CTRL (_rl_to_upper (seq[i]));
		}
	      continue;
	    }	      

	  /* Translate other backslash-escaped characters.  These are the
	     same escape sequences that bash's `echo' and `printf' builtins
	     handle, with the addition of \d -> RUBOUT.  A backslash
	     preceding a character that is not special is stripped. */
	  switch (c)
	    {
	    case 'a':
	      array[l++] = '\007';
	      break;
	    case 'b':
	      array[l++] = '\b';
	      break;
	    case 'd':
	      array[l++] = RUBOUT;	/* readline-specific */
	      break;
	    case 'e':
	      array[l++] = ESC;
	      break;
	    case 'f':
	      array[l++] = '\f';
	      break;
	    case 'n':
	      array[l++] = NEWLINE;
	      break;
	    case 'r':
	      array[l++] = RETURN;
	      break;
	    case 't':
	      array[l++] = TAB;
	      break;
	    case 'v':
	      array[l++] = 0x0B;
	      break;
	    case '\\':
	      array[l++] = '\\';
	      break;
	    case '0': case '1': case '2': case '3':
	    case '4': case '5': case '6': case '7':
	      i++;
	      for (temp = 2, c -= '0'; ISOCTAL (seq[i]) && temp--; i++)
	        c = (c * 8) + OCTVALUE (seq[i]);
	      i--;	/* auto-increment in for loop */
	      array[l++] = c % (largest_char + 1);
	      break;
	    case 'x':
	      i++;
	      for (temp = 3, c = 0; isxdigit (seq[i]) && temp--; i++)
	        c = (c * 16) + HEXVALUE (seq[i]);
	      if (temp == 3)
	        c = 'x';
	      i--;	/* auto-increment in for loop */
	      array[l++] = c % (largest_char + 1);
	      break;
	    default:	/* backslashes before non-special chars just add the char */
	      array[l++] = c;
	      break;	/* the backslash is stripped */
	    }
	  continue;
	}

      array[l++] = c;
    }

  *len = l;
  array[l] = '\0';
  return (0);
}

char *
rl_untranslate_keyseq (seq)
     int seq;
{
  static char kseq[16];
  int i, c;

  i = 0;
  c = seq;
  if (META_CHAR (c))
    {
      kseq[i++] = '\\';
      kseq[i++] = 'M';
      kseq[i++] = '-';
      c = UNMETA (c);
    }
  else if (CTRL_CHAR (c))
    {
      kseq[i++] = '\\';
      kseq[i++] = 'C';
      kseq[i++] = '-';
      c = _rl_to_lower (UNCTRL (c));
    }
  else if (c == RUBOUT)
    {
      kseq[i++] = '\\';
      kseq[i++] = 'C';
      kseq[i++] = '-';
      c = '?';
    }

  if (c == ESC)
    {
      kseq[i++] = '\\';
      c = 'e';
    }
  else if (c == '\\' || c == '"')
    {
      kseq[i++] = '\\';
    }

  kseq[i++] = (unsigned char) c;
  kseq[i] = '\0';
  return kseq;
}

static char *
_rl_untranslate_macro_value (seq)
     char *seq;
{
  char *ret, *r, *s;
  int c;

  r = ret = xmalloc (7 * strlen (seq) + 1);
  for (s = seq; *s; s++)
    {
      c = *s;
      if (META_CHAR (c))
	{
	  *r++ = '\\';
	  *r++ = 'M';
	  *r++ = '-';
	  c = UNMETA (c);
	}
      else if (CTRL_CHAR (c) && c != ESC)
	{
	  *r++ = '\\';
	  *r++ = 'C';
	  *r++ = '-';
	  c = _rl_to_lower (UNCTRL (c));
	}
      else if (c == RUBOUT)
 	{
 	  *r++ = '\\';
 	  *r++ = 'C';
 	  *r++ = '-';
 	  c = '?';
 	}

      if (c == ESC)
	{
	  *r++ = '\\';
	  c = 'e';
	}
      else if (c == '\\' || c == '"')
	*r++ = '\\';

      *r++ = (unsigned char)c;
    }
  *r = '\0';
  return ret;
}

/* Return a pointer to the function that STRING represents.
   If STRING doesn't have a matching function, then a NULL pointer
   is returned. */
Function *
rl_named_function (string)
     char *string;
{
  register int i;

  rl_initialize_funmap ();

  for (i = 0; funmap[i]; i++)
    if (_rl_stricmp (funmap[i]->name, string) == 0)
      return (funmap[i]->function);
  return ((Function *)NULL);
}

/* Return the function (or macro) definition which would be invoked via
   KEYSEQ if executed in MAP.  If MAP is NULL, then the current keymap is
   used.  TYPE, if non-NULL, is a pointer to an int which will receive the
   type of the object pointed to.  One of ISFUNC (function), ISKMAP (keymap),
   or ISMACR (macro). */
Function *
rl_function_of_keyseq (keyseq, map, type)
     char *keyseq;
     Keymap map;
     int *type;
{
  register int i;

  if (!map)
    map = _rl_keymap;

  for (i = 0; keyseq && keyseq[i]; i++)
    {
      int ic = keyseq[i];

      if (META_CHAR (ic) && _rl_convert_meta_chars_to_ascii)
	{
	  if (map[ESC].type != ISKMAP)
	    {
	      if (type)
		*type = map[ESC].type;

	      return (map[ESC].function);
	    }
	  else
	    {
	      map = FUNCTION_TO_KEYMAP (map, ESC);
	      ic = UNMETA (ic);
	    }
	}

      if (map[ic].type == ISKMAP)
	{
	  /* If this is the last key in the key sequence, return the
	     map. */
	  if (!keyseq[i + 1])
	    {
	      if (type)
		*type = ISKMAP;

	      return (map[ic].function);
	    }
	  else
	    map = FUNCTION_TO_KEYMAP (map, ic);
	}
      else
	{
	  if (type)
	    *type = map[ic].type;

	  return (map[ic].function);
	}
    }
  return ((Function *) NULL);
}

/* The last key bindings file read. */
static char *last_readline_init_file = (char *)NULL;

/* The file we're currently reading key bindings from. */
static char *current_readline_init_file;
static int current_readline_init_include_level;
static int current_readline_init_lineno;

/* Read FILENAME into a locally-allocated buffer and return the buffer.
   The size of the buffer is returned in *SIZEP.  Returns NULL if any
   errors were encountered. */
static char *
_rl_read_file (filename, sizep)
     char *filename;
     size_t *sizep;
{
  struct stat finfo;
  size_t file_size;
  char *buffer;
  int i, file;

  if ((stat (filename, &finfo) < 0) || (file = open (filename, O_RDONLY, 0666)) < 0)
    return ((char *)NULL);

  file_size = (size_t)finfo.st_size;

  /* check for overflow on very large files */
  if (file_size != finfo.st_size || file_size + 1 < file_size)
    {
      if (file >= 0)
	close (file);
#if defined (EFBIG)
      errno = EFBIG;
#endif
      return ((char *)NULL);
    }

  /* Read the file into BUFFER. */
  buffer = (char *)xmalloc (file_size + 1);
  i = read (file, buffer, file_size);
  close (file);

#if 0
  if (i < file_size)
#else
  if (i < 0)
#endif
    {
      free (buffer);
      return ((char *)NULL);
    }

  buffer[file_size] = '\0';
  if (sizep)
    *sizep = file_size;
  return (buffer);
}

/* Re-read the current keybindings file. */
int
rl_re_read_init_file (count, ignore)
     int count, ignore;
{
  int r;
  r = rl_read_init_file ((char *)NULL);
  rl_set_keymap_from_edit_mode ();
  return r;
}

/* Do key bindings from a file.  If FILENAME is NULL it defaults
   to the first non-null filename from this list:
     1. the filename used for the previous call
     2. the value of the shell variable `INPUTRC'
     3. ~/.inputrc
   If the file existed and could be opened and read, 0 is returned,
   otherwise errno is returned. */
int
rl_read_init_file (filename)
     char *filename;
{
  /* Default the filename. */
  if (filename == 0)
    {
      filename = last_readline_init_file;
      if (filename == 0)
        filename = get_env_value ("INPUTRC");
      if (filename == 0)
	filename = DEFAULT_INPUTRC;
    }

  if (*filename == 0)
    filename = DEFAULT_INPUTRC;

  return (_rl_read_init_file (filename, 0));
}

static int
_rl_read_init_file (filename, include_level)
     char *filename;
     int include_level;
{
  register int i;
  char *buffer, *openname, *line, *end;
  size_t file_size;

  current_readline_init_file = filename;
  current_readline_init_include_level = include_level;

  openname = tilde_expand (filename);
  buffer = _rl_read_file (openname, &file_size);
  free (openname);

  if (buffer == 0)
    return (errno);
  
  if (include_level == 0 && filename != last_readline_init_file)
    {
      FREE (last_readline_init_file);
      last_readline_init_file = savestring (filename);
    }

  /* Loop over the lines in the file.  Lines that start with `#' are
     comments; all other lines are commands for readline initialization. */
  current_readline_init_lineno = 1;
  line = buffer;
  end = buffer + file_size;
  while (line < end)
    {
      /* Find the end of this line. */
      for (i = 0; line + i != end && line[i] != '\n'; i++);

      /* Mark end of line. */
      line[i] = '\0';

      /* Skip leading whitespace. */
      while (*line && whitespace (*line))
        {
	  line++;
	  i--;
        }

      /* If the line is not a comment, then parse it. */
      if (*line && *line != '#')
	rl_parse_and_bind (line);

      /* Move to the next line. */
      line += i + 1;
      current_readline_init_lineno++;
    }

  free (buffer);
  return (0);
}

static void
_rl_init_file_error (msg)
     char *msg;
{
  fprintf (stderr, "readline: %s: line %d: %s\n", current_readline_init_file,
 		   current_readline_init_lineno,
 		   msg);
}

/* **************************************************************** */
/*								    */
/*			Parser Directives       		    */
/*								    */
/* **************************************************************** */

/* Conditionals. */

/* Calling programs set this to have their argv[0]. */
char *rl_readline_name = "other";

/* Stack of previous values of parsing_conditionalized_out. */
static unsigned char *if_stack = (unsigned char *)NULL;
static int if_stack_depth;
static int if_stack_size;

/* Push _rl_parsing_conditionalized_out, and set parser state based
   on ARGS. */
static int
parser_if (args)
     char *args;
{
  register int i;

  /* Push parser state. */
  if (if_stack_depth + 1 >= if_stack_size)
    {
      if (!if_stack)
	if_stack = (unsigned char *)xmalloc (if_stack_size = 20);
      else
	if_stack = (unsigned char *)xrealloc (if_stack, if_stack_size += 20);
    }
  if_stack[if_stack_depth++] = _rl_parsing_conditionalized_out;

  /* If parsing is turned off, then nothing can turn it back on except
     for finding the matching endif.  In that case, return right now. */
  if (_rl_parsing_conditionalized_out)
    return 0;

  /* Isolate first argument. */
  for (i = 0; args[i] && !whitespace (args[i]); i++);

  if (args[i])
    args[i++] = '\0';

  /* Handle "$if term=foo" and "$if mode=emacs" constructs.  If this
     isn't term=foo, or mode=emacs, then check to see if the first
     word in ARGS is the same as the value stored in rl_readline_name. */
  if (rl_terminal_name && _rl_strnicmp (args, "term=", 5) == 0)
    {
      char *tem, *tname;

      /* Terminals like "aaa-60" are equivalent to "aaa". */
      tname = savestring (rl_terminal_name);
      tem = strchr (tname, '-');
      if (tem)
	*tem = '\0';

      /* Test the `long' and `short' forms of the terminal name so that
	 if someone has a `sun-cmd' and does not want to have bindings
	 that will be executed if the terminal is a `sun', they can put
	 `$if term=sun-cmd' into their .inputrc. */
      _rl_parsing_conditionalized_out = _rl_stricmp (args + 5, tname) &&
					_rl_stricmp (args + 5, rl_terminal_name);
      free (tname);
    }
#if defined (VI_MODE)
  else if (_rl_strnicmp (args, "mode=", 5) == 0)
    {
      int mode;

      if (_rl_stricmp (args + 5, "emacs") == 0)
	mode = emacs_mode;
      else if (_rl_stricmp (args + 5, "vi") == 0)
	mode = vi_mode;
      else
	mode = no_mode;

      _rl_parsing_conditionalized_out = mode != rl_editing_mode;
    }
#endif /* VI_MODE */
  /* Check to see if the first word in ARGS is the same as the
     value stored in rl_readline_name. */
  else if (_rl_stricmp (args, rl_readline_name) == 0)
    _rl_parsing_conditionalized_out = 0;
  else
    _rl_parsing_conditionalized_out = 1;
  return 0;
}

/* Invert the current parser state if there is anything on the stack. */
static int
parser_else (args)
     char *args;
{
  register int i;

  if (if_stack_depth == 0)
    {
      _rl_init_file_error ("$else found without matching $if");
      return 0;
    }

  /* Check the previous (n - 1) levels of the stack to make sure that
     we haven't previously turned off parsing. */
  for (i = 0; i < if_stack_depth - 1; i++)
    if (if_stack[i] == 1)
      return 0;

  /* Invert the state of parsing if at top level. */
  _rl_parsing_conditionalized_out = !_rl_parsing_conditionalized_out;
  return 0;
}

/* Terminate a conditional, popping the value of
   _rl_parsing_conditionalized_out from the stack. */
static int
parser_endif (args)
     char *args;
{
  if (if_stack_depth)
    _rl_parsing_conditionalized_out = if_stack[--if_stack_depth];
  else
    _rl_init_file_error ("$endif without matching $if");
  return 0;
}

static int
parser_include (args)
     char *args;
{
  char *old_init_file, *e;
  int old_line_number, old_include_level, r;

  if (_rl_parsing_conditionalized_out)
    return (0);

  old_init_file = current_readline_init_file;
  old_line_number = current_readline_init_lineno;
  old_include_level = current_readline_init_include_level;

  e = strchr (args, '\n');
  if (e)
    *e = '\0';
  r = _rl_read_init_file (args, old_include_level + 1);

  current_readline_init_file = old_init_file;
  current_readline_init_lineno = old_line_number;
  current_readline_init_include_level = old_include_level;

  return r;
}
  
/* Associate textual names with actual functions. */
static struct {
  char *name;
  Function *function;
} parser_directives [] = {
  { "if", parser_if },
  { "endif", parser_endif },
  { "else", parser_else },
  { "include", parser_include },
  { (char *)0x0, (Function *)0x0 }
};

/* Handle a parser directive.  STATEMENT is the line of the directive
   without any leading `$'. */
static int
handle_parser_directive (statement)
     char *statement;
{
  register int i;
  char *directive, *args;

  /* Isolate the actual directive. */

  /* Skip whitespace. */
  for (i = 0; whitespace (statement[i]); i++);

  directive = &statement[i];

  for (; statement[i] && !whitespace (statement[i]); i++);

  if (statement[i])
    statement[i++] = '\0';

  for (; statement[i] && whitespace (statement[i]); i++);

  args = &statement[i];

  /* Lookup the command, and act on it. */
  for (i = 0; parser_directives[i].name; i++)
    if (_rl_stricmp (directive, parser_directives[i].name) == 0)
      {
	(*parser_directives[i].function) (args);
	return (0);
      }

  /* display an error message about the unknown parser directive */
  _rl_init_file_error ("unknown parser directive");
  return (1);
}

/* Read the binding command from STRING and perform it.
   A key binding command looks like: Keyname: function-name\0,
   a variable binding command looks like: set variable value.
   A new-style keybinding looks like "\C-x\C-x": exchange-point-and-mark. */
int
rl_parse_and_bind (string)
     char *string;
{
  char *funname, *kname;
  register int c, i;
  int key, equivalency;

  while (string && whitespace (*string))
    string++;

  if (!string || !*string || *string == '#')
    return 0;

  /* If this is a parser directive, act on it. */
  if (*string == '$')
    {
      handle_parser_directive (&string[1]);
      return 0;
    }

  /* If we aren't supposed to be parsing right now, then we're done. */
  if (_rl_parsing_conditionalized_out)
    return 0;

  i = 0;
  /* If this keyname is a complex key expression surrounded by quotes,
     advance to after the matching close quote.  This code allows the
     backslash to quote characters in the key expression. */
  if (*string == '"')
    {
      int passc = 0;

      for (i = 1; c = string[i]; i++)
	{
	  if (passc)
	    {
	      passc = 0;
	      continue;
	    }

	  if (c == '\\')
	    {
	      passc++;
	      continue;
	    }

	  if (c == '"')
	    break;
	}
      /* If we didn't find a closing quote, abort the line. */
      if (string[i] == '\0')
        {
          _rl_init_file_error ("no closing `\"' in key binding");
          return 1;
        }
    }

  /* Advance to the colon (:) or whitespace which separates the two objects. */
  for (; (c = string[i]) && c != ':' && c != ' ' && c != '\t'; i++ );

  equivalency = (c == ':' && string[i + 1] == '=');

  /* Mark the end of the command (or keyname). */
  if (string[i])
    string[i++] = '\0';

  /* If doing assignment, skip the '=' sign as well. */
  if (equivalency)
    string[i++] = '\0';

  /* If this is a command to set a variable, then do that. */
  if (_rl_stricmp (string, "set") == 0)
    {
      char *var = string + i;
      char *value;

      /* Make VAR point to start of variable name. */
      while (*var && whitespace (*var)) var++;

      /* Make value point to start of value string. */
      value = var;
      while (*value && !whitespace (*value)) value++;
      if (*value)
	*value++ = '\0';
      while (*value && whitespace (*value)) value++;

      rl_variable_bind (var, value);
      return 0;
    }

  /* Skip any whitespace between keyname and funname. */
  for (; string[i] && whitespace (string[i]); i++);
  funname = &string[i];

  /* Now isolate funname.
     For straight function names just look for whitespace, since
     that will signify the end of the string.  But this could be a
     macro definition.  In that case, the string is quoted, so skip
     to the matching delimiter.  We allow the backslash to quote the
     delimiter characters in the macro body. */
  /* This code exists to allow whitespace in macro expansions, which
     would otherwise be gobbled up by the next `for' loop.*/
  /* XXX - it may be desirable to allow backslash quoting only if " is
     the quoted string delimiter, like the shell. */
  if (*funname == '\'' || *funname == '"')
    {
      int delimiter = string[i++], passc;

      for (passc = 0; c = string[i]; i++)
	{
	  if (passc)
	    {
	      passc = 0;
	      continue;
	    }

	  if (c == '\\')
	    {
	      passc = 1;
	      continue;
	    }

	  if (c == delimiter)
	    break;
	}
      if (c)
	i++;
    }

  /* Advance to the end of the string.  */
  for (; string[i] && !whitespace (string[i]); i++);

  /* No extra whitespace at the end of the string. */
  string[i] = '\0';

  /* Handle equivalency bindings here.  Make the left-hand side be exactly
     whatever the right-hand evaluates to, including keymaps. */
  if (equivalency)
    {
      return 0;
    }

  /* If this is a new-style key-binding, then do the binding with
     rl_set_key ().  Otherwise, let the older code deal with it. */
  if (*string == '"')
    {
      char *seq;
      register int j, k, passc;

      seq = xmalloc (1 + strlen (string));
      for (j = 1, k = passc = 0; string[j]; j++)
	{
	  /* Allow backslash to quote characters, but leave them in place.
	     This allows a string to end with a backslash quoting another
	     backslash, or with a backslash quoting a double quote.  The
	     backslashes are left in place for rl_translate_keyseq (). */
	  if (passc || (string[j] == '\\'))
	    {
	      seq[k++] = string[j];
	      passc = !passc;
	      continue;
	    }

	  if (string[j] == '"')
	    break;

	  seq[k++] = string[j];
	}
      seq[k] = '\0';

      /* Binding macro? */
      if (*funname == '\'' || *funname == '"')
	{
	  j = strlen (funname);

	  /* Remove the delimiting quotes from each end of FUNNAME. */
	  if (j && funname[j - 1] == *funname)
	    funname[j - 1] = '\0';

	  rl_macro_bind (seq, &funname[1], _rl_keymap);
	}
      else
	rl_set_key (seq, rl_named_function (funname), _rl_keymap);

      free (seq);
      return 0;
    }

  /* Get the actual character we want to deal with. */
  kname = strrchr (string, '-');
  if (!kname)
    kname = string;
  else
    kname++;

  key = glean_key_from_name (kname);

  /* Add in control and meta bits. */
  if (substring_member_of_array (string, possible_control_prefixes))
    key = CTRL (_rl_to_upper (key));

  if (substring_member_of_array (string, possible_meta_prefixes))
    key = META (key);

  /* Temporary.  Handle old-style keyname with macro-binding. */
  if (*funname == '\'' || *funname == '"')
    {
      unsigned char useq[2];
      int fl = strlen (funname);

      useq[0] = key; useq[1] = '\0';
      if (fl && funname[fl - 1] == *funname)
	funname[fl - 1] = '\0';

      rl_macro_bind (useq, &funname[1], _rl_keymap);
    }
#if defined (PREFIX_META_HACK)
  /* Ugly, but working hack to keep prefix-meta around. */
  else if (_rl_stricmp (funname, "prefix-meta") == 0)
    {
      char seq[2];

      seq[0] = key;
      seq[1] = '\0';
      rl_generic_bind (ISKMAP, seq, (char *)emacs_meta_keymap, _rl_keymap);
    }
#endif /* PREFIX_META_HACK */
  else
    rl_bind_key (key, rl_named_function (funname));
  return 0;
}

/* Simple structure for boolean readline variables (i.e., those that can
   have one of two values; either "On" or 1 for truth, or "Off" or 0 for
   false. */

static struct {
  char *name;
  int *value;
} boolean_varlist [] = {
#if defined (PAREN_MATCHING)
  { "blink-matching-paren",	&rl_blink_matching_paren },
#endif
  { "completion-ignore-case",	&_rl_completion_case_fold },
  { "convert-meta",		&_rl_convert_meta_chars_to_ascii },
  { "disable-completion",	&rl_inhibit_completion },
  { "enable-keypad",		&_rl_enable_keypad },
  { "expand-tilde",		&rl_complete_with_tilde_expansion },
  { "horizontal-scroll-mode",	&_rl_horizontal_scroll_mode },
  { "input-meta",		&_rl_meta_flag },
  { "mark-directories",		&_rl_complete_mark_directories },
  { "mark-modified-lines",	&_rl_mark_modified_lines },
  { "meta-flag",		&_rl_meta_flag },
  { "output-meta",		&_rl_output_meta_chars },
  { "print-completions-horizontally", &_rl_print_completions_horizontally },
  { "show-all-if-ambiguous",	&_rl_complete_show_all },
#if defined (VISIBLE_STATS)
  { "visible-stats",		&rl_visible_stats },
#endif /* VISIBLE_STATS */
  { (char *)NULL, (int *)NULL }
};

int
rl_variable_bind (name, value)
     char *name, *value;
{
  register int i;

  /* Check for simple variables first. */
  for (i = 0; boolean_varlist[i].name; i++)
    {
      if (_rl_stricmp (name, boolean_varlist[i].name) == 0)
	{
	  /* A variable is TRUE if the "value" is "on", "1" or "". */
	  *boolean_varlist[i].value = *value == 0 ||
	  			      _rl_stricmp (value, "on") == 0 ||
				      (value[0] == '1' && value[1] == '\0');
	  return 0;
	}
    }

  /* Not a boolean variable, so check for specials. */

  /* Editing mode change? */
  if (_rl_stricmp (name, "editing-mode") == 0)
    {
      if (_rl_strnicmp (value, "vi", 2) == 0)
	{
#if defined (VI_MODE)
	  _rl_keymap = vi_insertion_keymap;
	  rl_editing_mode = vi_mode;
#endif /* VI_MODE */
	}
      else if (_rl_strnicmp (value, "emacs", 5) == 0)
	{
	  _rl_keymap = emacs_standard_keymap;
	  rl_editing_mode = emacs_mode;
	}
    }

  /* Comment string change? */
  else if (_rl_stricmp (name, "comment-begin") == 0)
    {
      if (*value)
	{
	  if (_rl_comment_begin)
	    free (_rl_comment_begin);

	  _rl_comment_begin = savestring (value);
	}
    }
  else if (_rl_stricmp (name, "completion-query-items") == 0)
    {
      int nval = 100;
      if (*value)
	{
	  nval = atoi (value);
	  if (nval < 0)
	    nval = 0;
	}
      rl_completion_query_items = nval;
    }
  else if (_rl_stricmp (name, "keymap") == 0)
    {
      Keymap kmap;
      kmap = rl_get_keymap_by_name (value);
      if (kmap)
        rl_set_keymap (kmap);
    }
  else if (_rl_stricmp (name, "bell-style") == 0)
    {
      if (!*value)
        _rl_bell_preference = AUDIBLE_BELL;
      else
        {
          if (_rl_stricmp (value, "none") == 0 || _rl_stricmp (value, "off") == 0)
            _rl_bell_preference = NO_BELL;
          else if (_rl_stricmp (value, "audible") == 0 || _rl_stricmp (value, "on") == 0)
            _rl_bell_preference = AUDIBLE_BELL;
          else if (_rl_stricmp (value, "visible") == 0)
            _rl_bell_preference = VISIBLE_BELL;
        }
    }
  else if (_rl_stricmp (name, "prefer-visible-bell") == 0)
    {
      /* Backwards compatibility. */
      if (*value && (_rl_stricmp (value, "on") == 0 ||
		     (*value == '1' && !value[1])))
        _rl_bell_preference = VISIBLE_BELL;
      else
        _rl_bell_preference = AUDIBLE_BELL;
    }
  else if (_rl_stricmp (name, "isearch-terminators") == 0)
    {
      /* Isolate the value and translate it into a character string. */
      int beg, end;
      char *v;

      v = savestring (value);
      FREE (_rl_isearch_terminators);
      if (v[0] == '"' || v[0] == '\'')
	{
	  int delim = v[0];
	  for (beg = end = 1; v[end] && v[end] != delim; end++)
	    ;
	}
      else
	{
	  for (beg = end = 0; whitespace (v[end]) == 0; end++)
	    ;
	}

      v[end] = '\0';
      /* The value starts at v + beg.  Translate it into a character string. */
      _rl_isearch_terminators = (unsigned char *)xmalloc (2 * strlen (v) + 1);
      rl_translate_keyseq (v + beg, (char*) _rl_isearch_terminators, &end);
      _rl_isearch_terminators[end] = '\0';
      free (v);
    }
      
  /* For the time being, unknown variable names are simply ignored. */
  return 0;
}

/* Return the character which matches NAME.
   For example, `Space' returns ' '. */

typedef struct {
  char *name;
  int value;
} assoc_list;

static assoc_list name_key_alist[] = {
  { "DEL", 0x7f },
  { "ESC", '\033' },
  { "Escape", '\033' },
  { "LFD", '\n' },
  { "Newline", '\n' },
  { "RET", '\r' },
  { "Return", '\r' },
  { "Rubout", 0x7f },
  { "SPC", ' ' },
  { "Space", ' ' },
  { "Tab", 0x09 },
  { (char *)0x0, 0 }
};

static int
glean_key_from_name (name)
     char *name;
{
  register int i;

  for (i = 0; name_key_alist[i].name; i++)
    if (_rl_stricmp (name, name_key_alist[i].name) == 0)
      return (name_key_alist[i].value);

  return (*(unsigned char *)name);	/* XXX was return (*name) */
}

/* Auxiliary functions to manage keymaps. */
static struct {
  char *name;
  Keymap map;
} keymap_names[] = {
  { "emacs", emacs_standard_keymap },
  { "emacs-standard", emacs_standard_keymap },
  { "emacs-meta", emacs_meta_keymap },
  { "emacs-ctlx", emacs_ctlx_keymap },
#if defined (VI_MODE)
  { "vi", vi_movement_keymap },
  { "vi-move", vi_movement_keymap },
  { "vi-command", vi_movement_keymap },
  { "vi-insert", vi_insertion_keymap },
#endif /* VI_MODE */
  { (char *)0x0, (Keymap)0x0 }
};

Keymap
rl_get_keymap_by_name (name)
     char *name;
{
  register int i;

  for (i = 0; keymap_names[i].name; i++)
    if (strcmp (name, keymap_names[i].name) == 0)
      return (keymap_names[i].map);
  return ((Keymap) NULL);
}

char *
rl_get_keymap_name (map)
     Keymap map;
{
  register int i;
  for (i = 0; keymap_names[i].name; i++)
    if (map == keymap_names[i].map)
      return (keymap_names[i].name);
  return ((char *)NULL);
}
  
void
rl_set_keymap (map)
     Keymap map;
{
  if (map)
    _rl_keymap = map;
}

Keymap
rl_get_keymap ()
{
  return (_rl_keymap);
}

void
rl_set_keymap_from_edit_mode ()
{
  if (rl_editing_mode == emacs_mode)
    _rl_keymap = emacs_standard_keymap;
#if defined (VI_MODE)
  else if (rl_editing_mode == vi_mode)
    _rl_keymap = vi_insertion_keymap;
#endif /* VI_MODE */
}

char *
rl_get_keymap_name_from_edit_mode ()
{
  if (rl_editing_mode == emacs_mode)
    return "emacs";
#if defined (VI_MODE)
  else if (rl_editing_mode == vi_mode)
    return "vi";
#endif /* VI_MODE */
  else
    return "none";
}

/* **************************************************************** */
/*								    */
/*		  Key Binding and Function Information		    */
/*								    */
/* **************************************************************** */

/* Each of the following functions produces information about the
   state of keybindings and functions known to Readline.  The info
   is always printed to rl_outstream, and in such a way that it can
   be read back in (i.e., passed to rl_parse_and_bind (). */

/* Print the names of functions known to Readline. */
void
rl_list_funmap_names ()
{
  register int i;
  char **funmap_names;

  funmap_names = rl_funmap_names ();

  if (!funmap_names)
    return;

  for (i = 0; funmap_names[i]; i++)
    fprintf (rl_outstream, "%s\n", funmap_names[i]);

  free (funmap_names);
}

static char *
_rl_get_keyname (key)
     int key;
{
  char *keyname;
  int i, c;

  keyname = (char *)xmalloc (8);

  c = key;
  /* Since this is going to be used to write out keysequence-function
     pairs for possible inclusion in an inputrc file, we don't want to
     do any special meta processing on KEY. */

#if 0
  /* We might want to do this, but the old version of the code did not. */

  /* If this is an escape character, we don't want to do any more processing.
     Just add the special ESC key sequence and return. */
  if (c == ESC)
    {
      keyseq[0] = '\\';
      keyseq[1] = 'e';
      keyseq[2] = '\0';
      return keyseq;
    }
#endif

  /* RUBOUT is translated directly into \C-? */
  if (key == RUBOUT)
    {
      keyname[0] = '\\';
      keyname[1] = 'C';
      keyname[2] = '-';
      keyname[3] = '?';
      keyname[4] = '\0';
      return keyname;
    }

  i = 0;
  /* Now add special prefixes needed for control characters.  This can
     potentially change C. */
  if (CTRL_CHAR (c))
    {
      keyname[i++] = '\\';
      keyname[i++] = 'C';
      keyname[i++] = '-';
      c = _rl_to_lower (UNCTRL (c));
    }

  /* XXX experimental code.  Turn the characters that are not ASCII or
     ISO Latin 1 (128 - 159) into octal escape sequences (\200 - \237).
     This changes C. */
  if (c >= 128 && c <= 159)
    {
      keyname[i++] = '\\';
      keyname[i++] = '2';
      c -= 128;
      keyname[i++] = (c / 8) + '0';
      c = (c % 8) + '0';
    }

  /* Now, if the character needs to be quoted with a backslash, do that. */
  if (c == '\\' || c == '"')
    keyname[i++] = '\\';

  /* Now add the key, terminate the string, and return it. */
  keyname[i++] = (char) c;
  keyname[i] = '\0';

  return keyname;
}

/* Return a NULL terminated array of strings which represent the key
   sequences that are used to invoke FUNCTION in MAP. */
char **
rl_invoking_keyseqs_in_map (function, map)
     Function *function;
     Keymap map;
{
  register int key;
  char **result;
  int result_index, result_size;

  result = (char **)NULL;
  result_index = result_size = 0;

  for (key = 0; key < KEYMAP_SIZE; key++)
    {
      switch (map[key].type)
	{
	case ISMACR:
	  /* Macros match, if, and only if, the pointers are identical.
	     Thus, they are treated exactly like functions in here. */
	case ISFUNC:
	  /* If the function in the keymap is the one we are looking for,
	     then add the current KEY to the list of invoking keys. */
	  if (map[key].function == function)
	    {
	      char *keyname;

	      keyname = _rl_get_keyname (key);

	      if (result_index + 2 > result_size)
	        {
	          result_size += 10;
		  result = (char **) xrealloc (result, result_size * sizeof (char *));
	        }

	      result[result_index++] = keyname;
	      result[result_index] = (char *)NULL;
	    }
	  break;

	case ISKMAP:
	  {
	    char **seqs;
	    register int i;

	    /* Find the list of keyseqs in this map which have FUNCTION as
	       their target.  Add the key sequences found to RESULT. */
	    if (map[key].function)
	      seqs =
	        rl_invoking_keyseqs_in_map (function, FUNCTION_TO_KEYMAP (map, key));
	    else
	      break;

	    if (seqs == 0)
	      break;

	    for (i = 0; seqs[i]; i++)
	      {
		char *keyname = (char *)xmalloc (6 + strlen (seqs[i]));

		if (key == ESC)
		  sprintf (keyname, "\\e");
		else if (CTRL_CHAR (key))
		  sprintf (keyname, "\\C-%c", _rl_to_lower (UNCTRL (key)));
		else if (key == RUBOUT)
		  sprintf (keyname, "\\C-?");
		else if (key == '\\' || key == '"')
		  {
		    keyname[0] = '\\';
		    keyname[1] = (char) key;
		    keyname[2] = '\0';
		  }
		else
		  {
		    keyname[0] = (char) key;
		    keyname[1] = '\0';
		  }
		
		strcat (keyname, seqs[i]);
		free (seqs[i]);

		if (result_index + 2 > result_size)
		  {
		    result_size += 10;
		    result = (char **) xrealloc (result, result_size * sizeof (char *));
		  }

		result[result_index++] = keyname;
		result[result_index] = (char *)NULL;
	      }

	    free (seqs);
	  }
	  break;
	}
    }
  return (result);
}

/* Return a NULL terminated array of strings which represent the key
   sequences that can be used to invoke FUNCTION using the current keymap. */
char **
rl_invoking_keyseqs (function)
     Function *function;
{
  return (rl_invoking_keyseqs_in_map (function, _rl_keymap));
}

/* Print all of the functions and their bindings to rl_outstream.  If
   PRINT_READABLY is non-zero, then print the output in such a way
   that it can be read back in. */
void
rl_function_dumper (print_readably)
     int print_readably;
{
  register int i;
  char **names;
  char *name;

  names = rl_funmap_names ();

  fprintf (rl_outstream, "\n");

  for (i = 0; name = names[i]; i++)
    {
      Function *function;
      char **invokers;

      function = rl_named_function (name);
      invokers = rl_invoking_keyseqs_in_map (function, _rl_keymap);

      if (print_readably)
	{
	  if (!invokers)
	    fprintf (rl_outstream, "# %s (not bound)\n", name);
	  else
	    {
	      register int j;

	      for (j = 0; invokers[j]; j++)
		{
		  fprintf (rl_outstream, "\"%s\": %s\n",
			   invokers[j], name);
		  free (invokers[j]);
		}

	      free (invokers);
	    }
	}
      else
	{
	  if (!invokers)
	    fprintf (rl_outstream, "%s is not bound to any keys\n",
		     name);
	  else
	    {
	      register int j;

	      fprintf (rl_outstream, "%s can be found on ", name);

	      for (j = 0; invokers[j] && j < 5; j++)
		{
		  fprintf (rl_outstream, "\"%s\"%s", invokers[j],
			   invokers[j + 1] ? ", " : ".\n");
		}

	      if (j == 5 && invokers[j])
		fprintf (rl_outstream, "...\n");

	      for (j = 0; invokers[j]; j++)
		free (invokers[j]);

	      free (invokers);
	    }
	}
    }
}

/* Print all of the current functions and their bindings to
   rl_outstream.  If an explicit argument is given, then print
   the output in such a way that it can be read back in. */
int
rl_dump_functions (count, key)
     int count, key;
{
  if (rl_dispatching)
    fprintf (rl_outstream, "\r\n");
  rl_function_dumper (rl_explicit_arg);
  rl_on_new_line ();
  return (0);
}

static void
_rl_macro_dumper_internal (print_readably, map, prefix)
     int print_readably;
     Keymap map;
     char *prefix;
{
  register int key;
  char *keyname, *out;
  int prefix_len;

  for (key = 0; key < KEYMAP_SIZE; key++)
    {
      switch (map[key].type)
	{
	case ISMACR:
	  keyname = _rl_get_keyname (key);
#if 0
	  out = (char *)map[key].function;
#else
	  out = _rl_untranslate_macro_value ((char *)map[key].function);
#endif
	  if (print_readably)
	    fprintf (rl_outstream, "\"%s%s\": \"%s\"\n", prefix ? prefix : "",
						         keyname,
						         out ? out : "");
	  else
	    fprintf (rl_outstream, "%s%s outputs %s\n", prefix ? prefix : "",
							keyname,
							out ? out : "");
	  free (keyname);
#if 1
	  free (out);
#endif
	  break;
	case ISFUNC:
	  break;
	case ISKMAP:
	  prefix_len = prefix ? strlen (prefix) : 0;
	  if (key == ESC)
	    {
	      keyname = xmalloc (3 + prefix_len);
	      if (prefix)
		strcpy (keyname, prefix);
	      keyname[prefix_len] = '\\';
	      keyname[prefix_len + 1] = 'e';
	      keyname[prefix_len + 2] = '\0';
	    }
	  else
	    {
	      keyname = _rl_get_keyname (key);
	      if (prefix)
		{
		  out = xmalloc (strlen (keyname) + prefix_len + 1);
		  strcpy (out, prefix);
		  strcpy (out + prefix_len, keyname);
		  free (keyname);
		  keyname = out;
		}
	    }

	  _rl_macro_dumper_internal (print_readably, FUNCTION_TO_KEYMAP (map, key), keyname);
	  free (keyname);
	  break;
	}
    }
}

void
rl_macro_dumper (print_readably)
     int print_readably;
{
  _rl_macro_dumper_internal (print_readably, _rl_keymap, (char *)NULL);
}

int
rl_dump_macros (count, key)
     int count, key;
{
  if (rl_dispatching)
    fprintf (rl_outstream, "\r\n");
  rl_macro_dumper (rl_explicit_arg);
  rl_on_new_line ();
  return (0);
}

void
rl_variable_dumper (print_readably)
     int print_readably;
{
  int i;
  char *kname;

  for (i = 0; boolean_varlist[i].name; i++)
    {
      if (print_readably)
        fprintf (rl_outstream, "set %s %s\n", boolean_varlist[i].name,
			       *boolean_varlist[i].value ? "on" : "off");
      else
        fprintf (rl_outstream, "%s is set to `%s'\n", boolean_varlist[i].name,
			       *boolean_varlist[i].value ? "on" : "off");
    }

  /* bell-style */
  switch (_rl_bell_preference)
    {
    case NO_BELL:
      kname = "none"; break;
    case VISIBLE_BELL:
      kname = "visible"; break;
    case AUDIBLE_BELL:
    default:
      kname = "audible"; break;
    }
  if (print_readably)
    fprintf (rl_outstream, "set bell-style %s\n", kname);
  else
    fprintf (rl_outstream, "bell-style is set to `%s'\n", kname);

  /* comment-begin */
  if (print_readably)
    fprintf (rl_outstream, "set comment-begin %s\n", _rl_comment_begin ? _rl_comment_begin : RL_COMMENT_BEGIN_DEFAULT);
  else
    fprintf (rl_outstream, "comment-begin is set to `%s'\n", _rl_comment_begin ? _rl_comment_begin : "");

  /* completion-query-items */
  if (print_readably)
    fprintf (rl_outstream, "set completion-query-items %d\n", rl_completion_query_items);
  else
    fprintf (rl_outstream, "completion-query-items is set to `%d'\n", rl_completion_query_items);

  /* editing-mode */
  if (print_readably)
    fprintf (rl_outstream, "set editing-mode %s\n", (rl_editing_mode == emacs_mode) ? "emacs" : "vi");
  else
    fprintf (rl_outstream, "editing-mode is set to `%s'\n", (rl_editing_mode == emacs_mode) ? "emacs" : "vi");

  /* keymap */
  kname = rl_get_keymap_name (_rl_keymap);
  if (kname == 0)
    kname = rl_get_keymap_name_from_edit_mode ();
  if (print_readably)
    fprintf (rl_outstream, "set keymap %s\n", kname ? kname : "none");
  else
    fprintf (rl_outstream, "keymap is set to `%s'\n", kname ? kname : "none");

  /* isearch-terminators */
  if (_rl_isearch_terminators)
    {
      char *disp;

      disp = _rl_untranslate_macro_value (_rl_isearch_terminators);

      if (print_readably)
	fprintf (rl_outstream, "set isearch-terminators \"%s\"\n", disp);
      else
	fprintf (rl_outstream, "isearch-terminators is set to \"%s\"\n", disp);

      free (disp);
    }
}

/* Print all of the current variables and their values to
   rl_outstream.  If an explicit argument is given, then print
   the output in such a way that it can be read back in. */
int
rl_dump_variables (count, key)
     int count, key;
{
  if (rl_dispatching)
    fprintf (rl_outstream, "\r\n");
  rl_variable_dumper (rl_explicit_arg);
  rl_on_new_line ();
  return (0);
}

/* Bind key sequence KEYSEQ to DEFAULT_FUNC if KEYSEQ is unbound. */
void
_rl_bind_if_unbound (keyseq, default_func)
     char *keyseq;
     Function *default_func;
{
  Function *func;

  if (keyseq)
    {
      func = rl_function_of_keyseq (keyseq, _rl_keymap, (int *)NULL);
      if (!func || func == rl_do_lowercase_version)
	rl_set_key (keyseq, default_func, _rl_keymap);
    }
}

/* Return non-zero if any members of ARRAY are a substring in STRING. */
static int
substring_member_of_array (string, array)
     char *string, **array;
{
  while (*array)
    {
      if (_rl_strindex (string, *array))
	return (1);
      array++;
    }
  return (0);
}
