/* terminal.c -- controlling the terminal with termcap. */

/* Copyright (C) 1996-2006 Free Software Foundation, Inc.

   This file is part of the GNU Readline Library, a library for
   reading lines of text with interactive input and history editing.

   The GNU Readline Library is free software; you can redistribute it
   and/or modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2, or
   (at your option) any later version.

   The GNU Readline Library is distributed in the hope that it will be
   useful, but WITHOUT ANY WARRANTY; without even the implied warranty
   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   The GNU General Public License is often shipped with GNU software, and
   is generally kept in a file called COPYING or LICENSE.  If you do not
   have a copy of the license, write to the Free Software Foundation,
   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA. */
#define READLINE_LIBRARY

#if defined (HAVE_CONFIG_H)
#  include "config_readline.h"
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

#include <stdio.h>

/* System-specific feature definitions and include files. */
#include "rldefs.h"

#if defined (GWINSZ_IN_SYS_IOCTL) && !defined (TIOCGWINSZ)
#  include <sys/ioctl.h>
#endif /* GWINSZ_IN_SYS_IOCTL && !TIOCGWINSZ */

#include "rltty.h"
#include "tcap.h"

/* Some standard library routines. */
#include "readline.h"
#include "history.h"

#include "rlprivate.h"
#include "rlshell.h"
#include "xmalloc.h"

#if defined (__MINGW32__)
#  include <windows.h>
#  include <wincon.h>

static void _win_get_screensize PARAMS((int *, int *));
#endif

#if defined (__EMX__)
static void _emx_get_screensize PARAMS((int *, int *));
#endif

#define CUSTOM_REDISPLAY_FUNC() (rl_redisplay_function != rl_redisplay)
#define CUSTOM_INPUT_FUNC() (rl_getc_function != rl_getc)

/*  If the calling application sets this to a non-zero value, readline will
    use the $LINES and $COLUMNS environment variables to set its idea of the
    window size before interrogating the kernel. */
int rl_prefer_env_winsize = 0;

/* **************************************************************** */
/*								    */
/*			Terminal and Termcap			    */
/*								    */
/* **************************************************************** */

static char *term_buffer = (char *)NULL;
static char *term_string_buffer = (char *)NULL;

static int tcap_initialized;

#if !defined (__linux__)
#  if defined (__EMX__) || defined (NEED_EXTERN_PC)
extern 
#  endif /* __EMX__ || NEED_EXTERN_PC */
char PC, *BC, *UP;
#endif /* __linux__ */

/* Some strings to control terminal actions.  These are output by tputs (). */
const char *_rl_term_clreol;
const char *_rl_term_clrpag;
const char *_rl_term_cr;
const char *_rl_term_backspace;
char _rl_term_backspace_default[2] = { '\b', 0 };
const char *_rl_term_goto;
const char *_rl_term_pc;

/* Non-zero if we determine that the terminal can do character insertion. */
int _rl_terminal_can_insert = 0;

/* How to insert characters. */
const char *_rl_term_im;
const char *_rl_term_ei;
const char *_rl_term_ic;
const char *_rl_term_ip;
const char *_rl_term_IC;

/* How to delete characters. */
const char *_rl_term_dc;
const char *_rl_term_DC;

const char *_rl_term_forward_char;

/* How to go up a line. */
const char *_rl_term_up;
char _rl_term_up_default[2] = { 0, 0 };

/* A visible bell; char if the terminal can be made to flash the screen. */
static const char *_rl_visible_bell;

/* Non-zero means the terminal can auto-wrap lines. */
int _rl_term_autowrap = -1;

/* Non-zero means that this terminal has a meta key. */
static int term_has_meta;

/* The sequences to write to turn on and off the meta key, if this
   terminal has one. */
static const char *_rl_term_mm;
static const char *_rl_term_mo;

/* The key sequences output by the arrow keys, if this terminal has any. */
static const char *_rl_term_ku;
static const char *_rl_term_kd;
static const char *_rl_term_kr;
static const char *_rl_term_kl;

/* How to initialize and reset the arrow keys, if this terminal has any. */
static const char *_rl_term_ks;
static const char *_rl_term_ke;

/* The key sequences sent by the Home and End keys, if any. */
static const char *_rl_term_kh;
static const char *_rl_term_kH;
static const char *_rl_term_at7;	/* @7 */

/* Delete key */
static const char *_rl_term_kD;

/* Insert key */
static const char *_rl_term_kI;

/* Cursor control */
static const char *_rl_term_vs;	/* very visible */
static const char *_rl_term_ve;	/* normal */

static void bind_termcap_arrow_keys PARAMS((Keymap));

/* Variables that hold the screen dimensions, used by the display code. */
int _rl_screenwidth, _rl_screenheight, _rl_screenchars;

/* Non-zero means the user wants to enable the keypad. */
int _rl_enable_keypad;

/* Non-zero means the user wants to enable a meta key. */
int _rl_enable_meta = 1;

#if defined (__EMX__)
static void
_emx_get_screensize (swp, shp)
     int *swp, *shp;
{
  int sz[2];

  _scrsize (sz);

  if (swp)
    *swp = sz[0];
  if (shp)
    *shp = sz[1];
}
#endif

#if defined (__MINGW32__)
static void
_win_get_screensize (swp, shp)
     int *swp, *shp;
{
  HANDLE hConOut;
  CONSOLE_SCREEN_BUFFER_INFO scr;

  hConOut = GetStdHandle (STD_OUTPUT_HANDLE);
  if (hConOut != INVALID_HANDLE_VALUE)
    {
      if (GetConsoleScreenBufferInfo (hConOut, &scr))
	{
	  *swp = scr.dwSize.X;
	  *shp = scr.srWindow.Bottom - scr.srWindow.Top + 1;
	}
    }
}
#endif

/* Get readline's idea of the screen size.  TTY is a file descriptor open
   to the terminal.  If IGNORE_ENV is true, we do not pay attention to the
   values of $LINES and $COLUMNS.  The tests for TERM_STRING_BUFFER being
   non-null serve to check whether or not we have initialized termcap. */
void
_rl_get_screen_size (tty, ignore_env)
     int tty, ignore_env;
{
  char *ss;
#if defined (TIOCGWINSZ)
  struct winsize window_size;
#endif /* TIOCGWINSZ */
  int wr, wc;

  wr = wc = -1;
#if defined (TIOCGWINSZ)
  if (ioctl (tty, TIOCGWINSZ, &window_size) == 0)
    {
      wc = (int) window_size.ws_col;
      wr = (int) window_size.ws_row;
    }
#endif /* TIOCGWINSZ */

#if defined (__EMX__)
  _emx_get_screensize (&wc, &wr);
#elif defined (__MINGW32__)
  _win_get_screensize (&wc, &wr);
#endif

  if (ignore_env || rl_prefer_env_winsize == 0)
    {
      _rl_screenwidth = wc;
      _rl_screenheight = wr;
    }
  else
    _rl_screenwidth = _rl_screenheight = -1;

  /* Environment variable COLUMNS overrides setting of "co" if IGNORE_ENV
     is unset.  If we prefer the environment, check it first before
     assigning the value returned by the kernel. */
  if (_rl_screenwidth <= 0)
    {
      if (ignore_env == 0 && (ss = sh_get_env_value ("COLUMNS")))
	_rl_screenwidth = atoi (ss);

      if (_rl_screenwidth <= 0)
        _rl_screenwidth = wc;

#if !defined (__DJGPP__)
      if (_rl_screenwidth <= 0 && term_string_buffer)
	_rl_screenwidth = tgetnum ((char *)"co");
#endif
    }

  /* Environment variable LINES overrides setting of "li" if IGNORE_ENV
     is unset. */
  if (_rl_screenheight <= 0)
    {
      if (ignore_env == 0 && (ss = sh_get_env_value ("LINES")))
	_rl_screenheight = atoi (ss);

      if (_rl_screenheight <= 0)
        _rl_screenheight = wr;

#if !defined (__DJGPP__)
      if (_rl_screenheight <= 0 && term_string_buffer)
	_rl_screenheight = tgetnum ((char *)"li");
#endif
    }

  /* If all else fails, default to 80x24 terminal. */
  if (_rl_screenwidth <= 1)
    _rl_screenwidth = 80;

  if (_rl_screenheight <= 0)
    _rl_screenheight = 24;

  /* If we're being compiled as part of bash, set the environment
     variables $LINES and $COLUMNS to new values.  Otherwise, just
     do a pair of putenv () or setenv () calls. */
  sh_set_lines_and_columns (_rl_screenheight, _rl_screenwidth);

  if (_rl_term_autowrap == 0)
    _rl_screenwidth--;

  _rl_screenchars = _rl_screenwidth * _rl_screenheight;
}

void
_rl_set_screen_size (rows, cols)
     int rows, cols;
{
  if (_rl_term_autowrap == -1)
    _rl_init_terminal_io (rl_terminal_name);

  if (rows > 0)
    _rl_screenheight = rows;
  if (cols > 0)
    {
      _rl_screenwidth = cols;
      if (_rl_term_autowrap == 0)
	_rl_screenwidth--;
    }

  if (rows > 0 || cols > 0)
    _rl_screenchars = _rl_screenwidth * _rl_screenheight;
}

void
rl_set_screen_size (rows, cols)
     int rows, cols;
{
  _rl_set_screen_size (rows, cols);
}

void
rl_get_screen_size (rows, cols)
     int *rows, *cols;
{
  if (rows)
    *rows = _rl_screenheight;
  if (cols)
    *cols = _rl_screenwidth;
}

void
rl_reset_screen_size ()
{
  _rl_get_screen_size (fileno (rl_instream), 0);
}
     
void
rl_resize_terminal ()
{
  if (readline_echoing_p)
    {
      _rl_get_screen_size (fileno (rl_instream), 1);
      if (CUSTOM_REDISPLAY_FUNC ())
	rl_forced_update_display ();
      else
	_rl_redisplay_after_sigwinch ();
    }
}

struct _tc_string {
     const char *tc_var;
     const char **tc_value;
};

/* This should be kept sorted, just in case we decide to change the
   search algorithm to something smarter. */
static struct _tc_string tc_strings[] =
{
  { "@7", &_rl_term_at7 },
  { "DC", &_rl_term_DC },
  { "IC", &_rl_term_IC },
  { "ce", &_rl_term_clreol },
  { "cl", &_rl_term_clrpag },
  { "cr", &_rl_term_cr },
  { "dc", &_rl_term_dc },
  { "ei", &_rl_term_ei },
  { "ic", &_rl_term_ic },
  { "im", &_rl_term_im },
  { "kD", &_rl_term_kD },	/* delete */
  { "kH", &_rl_term_kH },	/* home down ?? */
  { "kI", &_rl_term_kI },	/* insert */
  { "kd", &_rl_term_kd },
  { "ke", &_rl_term_ke },	/* end keypad mode */
  { "kh", &_rl_term_kh },	/* home */
  { "kl", &_rl_term_kl },
  { "kr", &_rl_term_kr },
  { "ks", &_rl_term_ks },	/* start keypad mode */
  { "ku", &_rl_term_ku },
  { "le", &_rl_term_backspace },
  { "mm", &_rl_term_mm },
  { "mo", &_rl_term_mo },
  { "nd", &_rl_term_forward_char },
  { "pc", &_rl_term_pc },
  { "up", &_rl_term_up },
  { "vb", &_rl_visible_bell },
  { "vs", &_rl_term_vs },
  { "ve", &_rl_term_ve },
};

#define NUM_TC_STRINGS (sizeof (tc_strings) / sizeof (struct _tc_string))

/* Read the desired terminal capability strings into BP.  The capabilities
   are described in the TC_STRINGS table. */
static void
get_term_capabilities (bp)
     char **bp;
{
#if !defined (__DJGPP__)	/* XXX - doesn't DJGPP have a termcap library? */
  register unsigned int i;

  for (i = 0; i < NUM_TC_STRINGS; i++)
    *(tc_strings[i].tc_value) = tgetstr ((char *)tc_strings[i].tc_var, bp);
#endif
  tcap_initialized = 1;
}

int
_rl_init_terminal_io (terminal_name)
     const char *terminal_name;
{
  const char *term;
  char *buffer;
  int tty, tgetent_ret;

  term = terminal_name ? terminal_name : sh_get_env_value ("TERM");
  _rl_term_clrpag = _rl_term_cr = _rl_term_clreol = (char *)NULL;
  tty = rl_instream ? fileno (rl_instream) : 0;

  if (term == 0)
    term = "dumb";

  /* I've separated this out for later work on not calling tgetent at all
     if the calling application has supplied a custom redisplay function,
     (and possibly if the application has supplied a custom input function). */
  if (CUSTOM_REDISPLAY_FUNC())
    {
      tgetent_ret = -1;
    }
  else
    {
      if (term_string_buffer == 0)
	term_string_buffer = (char *)xmalloc(2032);

      if (term_buffer == 0)
	term_buffer = (char *)xmalloc(4080);

      buffer = term_string_buffer;

      tgetent_ret = tgetent (term_buffer, term);
    }

  if (tgetent_ret <= 0)
    {
      FREE (term_string_buffer);
      FREE (term_buffer);
      buffer = term_buffer = term_string_buffer = (char *)NULL;

      _rl_term_autowrap = 0;	/* used by _rl_get_screen_size */

      /* Allow calling application to set default height and width, using
	 rl_set_screen_size */
      if (_rl_screenwidth <= 0 || _rl_screenheight <= 0)
	{
#if defined (__EMX__)
	  _emx_get_screensize (&_rl_screenwidth, &_rl_screenheight);
	  _rl_screenwidth--;
#else /* !__EMX__ */
	  _rl_get_screen_size (tty, 0);
#endif /* !__EMX__ */
	}

      /* Defaults. */
      if (_rl_screenwidth <= 0 || _rl_screenheight <= 0)
        {
	  _rl_screenwidth = 79;
	  _rl_screenheight = 24;
        }

      /* Everything below here is used by the redisplay code (tputs). */
      _rl_screenchars = _rl_screenwidth * _rl_screenheight;
      _rl_term_cr = "\r";
      _rl_term_im = _rl_term_ei = _rl_term_ic = _rl_term_IC = (char *)NULL;
      _rl_term_up = _rl_term_dc = _rl_term_DC = _rl_visible_bell = (char *)NULL;
      _rl_term_ku = _rl_term_kd = _rl_term_kl = _rl_term_kr = (char *)NULL;
      _rl_term_kh = _rl_term_kH = _rl_term_kI = _rl_term_kD = (char *)NULL;
      _rl_term_ks = _rl_term_ke = _rl_term_at7 = (char *)NULL;
      _rl_term_mm = _rl_term_mo = (char *)NULL;
      _rl_term_ve = _rl_term_vs = (char *)NULL;
      _rl_term_forward_char = (char *)NULL;
      _rl_terminal_can_insert = term_has_meta = 0;

      /* Reasonable defaults for tgoto().  Readline currently only uses
         tgoto if _rl_term_IC or _rl_term_DC is defined, but just in case we
         change that later... */
      PC = '\0';
      _rl_term_backspace = _rl_term_backspace_default;
      BC = (char*)_rl_term_backspace;
      UP = (char*)_rl_term_up;

      return 0;
    }

  get_term_capabilities (&buffer);

  /* Set up the variables that the termcap library expects the application
     to provide. */
  PC = _rl_term_pc ? *_rl_term_pc : 0;
  BC = (char*)_rl_term_backspace;
  UP = (char*)_rl_term_up;

  if (!_rl_term_cr)
    _rl_term_cr = "\r";

  _rl_term_autowrap = tgetflag ((char *)"am") && tgetflag ((char *)"xn");

  /* Allow calling application to set default height and width, using
     rl_set_screen_size */
  if (_rl_screenwidth <= 0 || _rl_screenheight <= 0)
    _rl_get_screen_size (tty, 0);

  /* "An application program can assume that the terminal can do
      character insertion if *any one of* the capabilities `IC',
      `im', `ic' or `ip' is provided."  But we can't do anything if
      only `ip' is provided, so... */
  _rl_terminal_can_insert = (_rl_term_IC || _rl_term_im || _rl_term_ic);

  /* Check to see if this terminal has a meta key and clear the capability
     variables if there is none. */
  term_has_meta = (tgetflag ((char *)"km") || tgetflag ((char *)"MT"));
  if (!term_has_meta)
    _rl_term_mm = _rl_term_mo = (char *)NULL;

  /* Attempt to find and bind the arrow keys.  Do not override already
     bound keys in an overzealous attempt, however. */

  bind_termcap_arrow_keys (emacs_standard_keymap);

#if defined (VI_MODE)
  bind_termcap_arrow_keys (vi_movement_keymap);
  bind_termcap_arrow_keys (vi_insertion_keymap);
#endif /* VI_MODE */

  return 0;
}

/* Bind the arrow key sequences from the termcap description in MAP. */
static void
bind_termcap_arrow_keys (map)
     Keymap map;
{
  Keymap xkeymap;

  xkeymap = _rl_keymap;
  _rl_keymap = map;

  rl_bind_keyseq_if_unbound (_rl_term_ku, rl_get_previous_history);
  rl_bind_keyseq_if_unbound (_rl_term_kd, rl_get_next_history);
  rl_bind_keyseq_if_unbound (_rl_term_kr, rl_forward_char);
  rl_bind_keyseq_if_unbound (_rl_term_kl, rl_backward_char);

  rl_bind_keyseq_if_unbound (_rl_term_kh, rl_beg_of_line);	/* Home */
  rl_bind_keyseq_if_unbound (_rl_term_at7, rl_end_of_line);	/* End */

  rl_bind_keyseq_if_unbound (_rl_term_kD, rl_delete);

  _rl_keymap = xkeymap;
}

const char *
rl_get_termcap (cap)
     const char *cap;
{
  register unsigned int i;

  if (tcap_initialized == 0)
    return ((char *)NULL);
  for (i = 0; i < NUM_TC_STRINGS; i++)
    {
      if (tc_strings[i].tc_var[0] == cap[0] && strcmp (tc_strings[i].tc_var, cap) == 0)
        return *(tc_strings[i].tc_value);
    }
  return ((char *)NULL);
}

/* Re-initialize the terminal considering that the TERM/TERMCAP variable
   has changed. */
int
rl_reset_terminal (terminal_name)
     const char *terminal_name;
{
  _rl_screenwidth = _rl_screenheight = 0;
  _rl_init_terminal_io (terminal_name);
  return 0;
}

/* A function for the use of tputs () */
#ifdef _MINIX
void
_rl_output_character_function (c)
     int c;
{
  putc (c, _rl_out_stream);
}
#else /* !_MINIX */
int
_rl_output_character_function (c)
     int c;
{
  return putc (c, _rl_out_stream);
}
#endif /* !_MINIX */

/* Write COUNT characters from STRING to the output stream. */
void
_rl_output_some_chars (string, count)
     const char *string;
     int count;
{
  if (fwrite (string, 1, count, _rl_out_stream) != (size_t)count)
    fprintf(stderr, "Write failed\n");
}

/* Move the cursor back. */
int
_rl_backspace (count)
     int count;
{
  register int i;

  if (_rl_term_backspace)
    for (i = 0; i < count; i++)
      tputs (_rl_term_backspace, 1, _rl_output_character_function);
  else
    for (i = 0; i < count; i++)
      putc ('\b', _rl_out_stream);
  return 0;
}

/* Move to the start of the next line. */
int
rl_crlf ()
{
#if defined (NEW_TTY_DRIVER)
  if (_rl_term_cr)
    tputs (_rl_term_cr, 1, _rl_output_character_function);
#endif /* NEW_TTY_DRIVER */
  putc ('\n', _rl_out_stream);
  return 0;
}

/* Ring the terminal bell. */
int
rl_ding ()
{
  if (readline_echoing_p)
    {
      switch (_rl_bell_preference)
        {
	case NO_BELL:
	default:
	  break;
	case VISIBLE_BELL:
	  if (_rl_visible_bell)
	    {
	      tputs (_rl_visible_bell, 1, _rl_output_character_function);
	      break;
	    }
	  /* FALLTHROUGH */
	case AUDIBLE_BELL:
	  fprintf (stderr, "\007");
	  fflush (stderr);
	  break;
        }
      return (0);
    }
  return (-1);
}

/* **************************************************************** */
/*								    */
/*	 	Controlling the Meta Key and Keypad		    */
/*								    */
/* **************************************************************** */

void
_rl_enable_meta_key ()
{
#if !defined (__DJGPP__)
  if (term_has_meta && _rl_term_mm)
    tputs (_rl_term_mm, 1, _rl_output_character_function);
#endif
}

void
_rl_control_keypad (on)
     int on;
{
#if !defined (__DJGPP__)
  if (on && _rl_term_ks)
    tputs (_rl_term_ks, 1, _rl_output_character_function);
  else if (!on && _rl_term_ke)
    tputs (_rl_term_ke, 1, _rl_output_character_function);
#endif
}

/* **************************************************************** */
/*								    */
/*	 		Controlling the Cursor			    */
/*								    */
/* **************************************************************** */

/* Set the cursor appropriately depending on IM, which is one of the
   insert modes (insert or overwrite).  Insert mode gets the normal
   cursor.  Overwrite mode gets a very visible cursor.  Only does
   anything if we have both capabilities. */
void
_rl_set_cursor (im, force)
     int im, force;
{
  if (_rl_term_ve && _rl_term_vs)
    {
      if (force || im != rl_insert_mode)
	{
	  if (im == RL_IM_OVERWRITE)
	    tputs (_rl_term_vs, 1, _rl_output_character_function);
	  else
	    tputs (_rl_term_ve, 1, _rl_output_character_function);
	}
    }
}
