/* input.c -- character input functions for readline. */

/* Copyright (C) 1994 Free Software Foundation, Inc.

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
   675 Mass Ave, Cambridge, MA 02139, USA. */
#define READLINE_LIBRARY

#if defined (HAVE_CONFIG_H)
#  include <config.h>
#endif

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

#if defined (HAVE_SELECT)
#  if !defined (HAVE_SYS_SELECT_H) || !defined (M_UNIX)
#    include <sys/time.h>
#  endif
#endif /* HAVE_SELECT */
#if defined (HAVE_SYS_SELECT_H)
#  include <sys/select.h>
#endif

#if defined (FIONREAD_IN_SYS_IOCTL)
#  include <sys/ioctl.h>
#endif

#include <stdio.h>
#include <errno.h>

#if !defined (errno)
extern int errno;
#endif /* !errno */

/* System-specific feature definitions and include files. */
#include "rldefs.h"

/* Some standard library routines. */
#include "readline.h"

/* What kind of non-blocking I/O do we have? */
#if !defined (O_NDELAY) && defined (O_NONBLOCK)
#  define O_NDELAY O_NONBLOCK	/* Posix style */
#endif

/* Functions imported from other files in the library. */
extern char *xmalloc (), *xrealloc ();

/* Variables and functions from macro.c. */
extern void _rl_add_macro_char ();
extern void _rl_with_macro_input ();
extern int _rl_next_macro_key ();
extern int _rl_defining_kbd_macro;

#if defined (VI_MODE)
extern void _rl_vi_set_last ();
extern int _rl_vi_textmod_command ();
#endif /* VI_MODE */

extern FILE *rl_instream, *rl_outstream;
extern Function *rl_last_func;
extern int rl_key_sequence_length;
extern int rl_pending_input;
extern int rl_editing_mode;

extern Keymap _rl_keymap;

extern int _rl_convert_meta_chars_to_ascii;

#if defined (__GO32__)
#  include <pc.h>
#endif /* __GO32__ */

/* Non-null means it is a pointer to a function to run while waiting for
   character input. */
Function *rl_event_hook = (Function *)NULL;

Function *rl_getc_function = rl_getc;

/* **************************************************************** */
/*								    */
/*			Character Input Buffering       	    */
/*								    */
/* **************************************************************** */

static int pop_index, push_index;
static unsigned char ibuffer[512];
static int ibuffer_len = sizeof (ibuffer) - 1;

#define any_typein (push_index != pop_index)

int
_rl_any_typein ()
{
  return any_typein;
}

/* Return the amount of space available in the buffer for stuffing
   characters. */
static int
ibuffer_space ()
{
  if (pop_index > push_index)
    return (pop_index - push_index - 1);
  else
    return (ibuffer_len - (push_index - pop_index));
}

/* Get a key from the buffer of characters to be read.
   Return the key in KEY.
   Result is KEY if there was a key, or 0 if there wasn't. */
static int
rl_get_char (key)
     int *key;
{
  if (push_index == pop_index)
    return (0);

  *key = ibuffer[pop_index++];

  if (pop_index >= ibuffer_len)
    pop_index = 0;

  return (1);
}

/* Stuff KEY into the *front* of the input buffer.
   Returns non-zero if successful, zero if there is
   no space left in the buffer. */
static int
rl_unget_char (key)
     int key;
{
  if (ibuffer_space ())
    {
      pop_index--;
      if (pop_index < 0)
	pop_index = ibuffer_len - 1;
      ibuffer[pop_index] = key;
      return (1);
    }
  return (0);
}

#if defined(__EMX__)
int waiting_char = -1;
#endif

/* If a character is available to be read, then read it
   and stuff it into IBUFFER.  Otherwise, just return. */
static void
rl_gather_tyi ()
{
#if defined (__EMX__)
  if (isatty (0) && (waiting_char = _read_kbd(0, 0, 0)) != -1 && ibuffer_space ())
    {
      int i;
      i = (*rl_getc_function) (rl_instream);
      rl_stuff_char (i);
    }
#elif defined (__GO32__)
  char input;

  if (isatty (0) && kbhit () && ibuffer_space ())
    {
      int i;
      i = (*rl_getc_function) (rl_instream);
      rl_stuff_char (i);
    }
#else /* !__GO32__ */

  int tty;
  register int tem, result;
  int chars_avail;
  char input;
#if defined(HAVE_SELECT)
  fd_set readfds, exceptfds;
  struct timeval timeout;
#endif

  tty = fileno (rl_instream);

#if defined (HAVE_SELECT)
  FD_ZERO (&readfds);
  FD_ZERO (&exceptfds);
  FD_SET (tty, &readfds);
  FD_SET (tty, &exceptfds);
  timeout.tv_sec = 0;
  timeout.tv_usec = 100000;	/* 0.1 seconds */
  if (select (tty + 1, &readfds, (fd_set *)NULL, &exceptfds, &timeout) <= 0)
    return;	/* Nothing to read. */
#endif

  result = -1;
#if defined (FIONREAD)
  result = ioctl (tty, FIONREAD, &chars_avail);
#endif

#if defined (O_NDELAY)
  if (result == -1)
    {
      tem = fcntl (tty, F_GETFL, 0);

      fcntl (tty, F_SETFL, (tem | O_NDELAY));
      chars_avail = read (tty, &input, 1);

      fcntl (tty, F_SETFL, tem);
      if (chars_avail == -1 && errno == EAGAIN)
	return;
    }
#endif /* O_NDELAY */

  /* If there's nothing available, don't waste time trying to read
     something. */
  if (chars_avail <= 0)
    return;

  tem = ibuffer_space ();

  if (chars_avail > tem)
    chars_avail = tem;

  /* One cannot read all of the available input.  I can only read a single
     character at a time, or else programs which require input can be
     thwarted.  If the buffer is larger than one character, I lose.
     Damn! */
  if (tem < ibuffer_len)
    chars_avail = 0;

  if (result != -1)
    {
      while (chars_avail--)
	rl_stuff_char ((*rl_getc_function) (rl_instream));
    }
  else
    {
      if (chars_avail)
	rl_stuff_char (input);
    }
#endif /* !__GO32__ */
}

/* Is there input available to be read on the readline input file
   descriptor?  Only works if the system has select(2) or FIONREAD. */
int
_rl_input_available ()
{
#if defined (__EMX__)
  if (isatty (0) && (waiting_char = _read_kbd(0, 0, 0)) != -1)
    return 1;
#else /*  __EMX__ */
#if defined(HAVE_SELECT)
  fd_set readfds, exceptfds;
  struct timeval timeout;
#endif
#if defined(FIONREAD)
  int chars_avail;
#endif
  int tty;

  tty = fileno (rl_instream);

#if defined (HAVE_SELECT)
  FD_ZERO (&readfds);
  FD_ZERO (&exceptfds);
  FD_SET (tty, &readfds);
  FD_SET (tty, &exceptfds);
  timeout.tv_sec = 0;
  timeout.tv_usec = 100000;	/* 0.1 seconds */
  return (select (tty + 1, &readfds, (fd_set *)NULL, &exceptfds, &timeout) > 0);
#endif

#if defined (FIONREAD)
  if (ioctl (tty, FIONREAD, &chars_avail) == 0)
    return (chars_avail);
#endif
#endif /* !__EMX__ */

  return 0;
}

void
_rl_insert_typein (c)
     int c;     
{    	
  int key, t, i;
  char *string;

  i = key = 0;
  string = xmalloc (ibuffer_len + 1);
  string[i++] = (char) c;

  while ((t = rl_get_char (&key)) &&
	 _rl_keymap[key].type == ISFUNC &&
	 _rl_keymap[key].function == rl_insert)
    string[i++] = key;

  if (t)
    rl_unget_char (key);

  string[i] = '\0';
  rl_insert_text (string);
  free (string);
}

/* Add KEY to the buffer of characters to be read.  Returns 1 if the
   character was stuffed correctly; 0 otherwise. */
int
rl_stuff_char (key)
     int key;
{
  if (ibuffer_space () == 0)
    return 0;

  if (key == EOF)
    {
      key = NEWLINE;
      rl_pending_input = EOF;
    }
  ibuffer[push_index++] = key;
  if (push_index >= ibuffer_len)
    push_index = 0;

  return 1;
}

/* Make C be the next command to be executed. */
int
rl_execute_next (c)
     int c;
{
  rl_pending_input = c;
  return 0;
}

/* **************************************************************** */
/*								    */
/*			     Character Input			    */
/*								    */
/* **************************************************************** */

/* Read a key, including pending input. */
int
rl_read_key ()
{
  int c;

  rl_key_sequence_length++;

  if (rl_pending_input)
    {
      c = rl_pending_input;
      rl_pending_input = 0;
    }
  else
    {
      /* If input is coming from a macro, then use that. */
      if ((c = _rl_next_macro_key ()))
	return (c);

      /* If the user has an event function, then call it periodically. */
      if (rl_event_hook)
	{
	  while (rl_event_hook && rl_get_char (&c) == 0)
	    {
	      (*rl_event_hook) ();
	      rl_gather_tyi ();
	    }
	}
      else
	{
	  if (rl_get_char (&c) == 0)
	    c = (*rl_getc_function) (rl_instream);
	}
    }

  return (c);
}

int
rl_getc (stream)
     FILE *stream;
{
  int result, flags;
  unsigned char c;

#if defined (__EMX__)
  if (isatty (0))
  {
    int key;

    if (waiting_char != -1)
    {
      key = waiting_char;
      waiting_char = -1;
    }
    else
    {
#ifdef __RSXNT__
      pc_flush();
#endif
      key = _read_kbd(0, 1, 0);
    }

    while (key == 0)
    {
      key |= (_read_kbd(0, 1, 0) << 8);
      /* printf("<%04X> ", key);
      fflush(stdout); */

      switch (key)
      {
      case 0x4B00: /* left arrow */
	key = 'B' - 64;
	break;
      case 0x4D00: /* right arrow */
	key = 'F' - 64;
	break;
      case 0x7300: /* ctrl left arrow */
	key = 27;
	waiting_char = 'B';
	break;
      case 0x7400: /* ctrl right arrow */
	key = 27;
	waiting_char = 'F';
	break;
      case 0x4800: /* up arrow */
	key = 'P' - 64;
	break;
      case 0x5000: /* down arrow */
	key = 'N' - 64;
	break;
      case 0x8D00: /* ctrl up arrow */
	key = 'R' - 64;
	break;
      case 0x9100: /* ctrl down arrow */
	key = 'S' - 64;
	break;
      case 0x4700: /* home key */
	key = 'A' - 64;
	break;
      case 0x4F00: /* end key */
	key = 'E' - 64;
	break;
      case 0x7700: /* ctrl home key */
	key = 27;
	waiting_char = '<';
	break;
      case 0x7500: /* ctrl end key */
	key = 27;
	waiting_char = '>';
	break;
      case 0x5300: /* delete key */
	key = 'D' - 64;
	break;
      case 0x5200: /* insert key */
	key = 'V' - 64;
	break;
      default: /* ignore all other special keys, read next */
	key = _read_kbd(0, 1, 0);
	break;
      }
    }

    return (key & 0xFF);
  }
#endif /* __EMX__ */

#if defined (__GO32__)
  if (isatty (0))
    return (getkey () & 0x7F);
#endif /* __GO32__ */

  while (1)
    {
      result = read (fileno (stream), &c, sizeof (unsigned char));

      if (result == sizeof (unsigned char))
	return (c);

      /* If zero characters are returned, then the file that we are
	 reading from is empty!  Return EOF in that case. */
      if (result == 0)
	return (EOF);

#if defined (__BEOS__)
      if (errno == EINTR)
	continue;
#endif

#if defined (EWOULDBLOCK)
      if (errno == EWOULDBLOCK)
	{
	  if ((flags = fcntl (fileno (stream), F_GETFL, 0)) < 0)
	    return (EOF);
	  if (flags & O_NDELAY)
	    {
	      flags &= ~O_NDELAY;
	      fcntl (fileno (stream), F_SETFL, flags);
	      continue;
	    }
	  continue;
	}
#endif /* EWOULDBLOCK */

#if defined (_POSIX_VERSION) && defined (EAGAIN) && defined (O_NONBLOCK)
      if (errno == EAGAIN)
	{
	  if ((flags = fcntl (fileno (stream), F_GETFL, 0)) < 0)
	    return (EOF);
	  if (flags & O_NONBLOCK)
	    {
	      flags &= ~O_NONBLOCK;
	      fcntl (fileno (stream), F_SETFL, flags);
	      continue;
	    }
	}
#endif /* _POSIX_VERSION && EAGAIN && O_NONBLOCK */

#if !defined (__GO32__)
      /* If the error that we received was SIGINT, then try again,
	 this is simply an interrupted system call to read ().
	 Otherwise, some error ocurred, also signifying EOF. */
      if (errno != EINTR)
	return (EOF);
#endif /* !__GO32__ */
    }
}
