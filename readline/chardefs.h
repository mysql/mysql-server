/* chardefs.h -- Character definitions for readline. */

/* Copyright (C) 1994 Free Software Foundation, Inc.

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

#ifndef _CHARDEFS_H_
#define _CHARDEFS_H_

#ifndef _m_ctype_h
#include <ctype.h>
#endif

#if defined (HAVE_CONFIG_H)
#  if defined (HAVE_STRING_H)
#    include <string.h>
#  else
#    include <strings.h>
#  endif /* HAVE_STRING_H */
#else
#  include <string.h>
#endif /* !HAVE_CONFIG_H */

#ifndef whitespace
#define whitespace(c) (((c) == ' ') || ((c) == '\t'))
#endif

#ifdef CTRL
#undef CTRL
#endif

/* Some character stuff. */
#define control_character_threshold 0x020   /* Smaller than this is control. */
#define control_character_mask 0x1f	    /* 0x20 - 1 */
#define meta_character_threshold 0x07f	    /* Larger than this is Meta. */
#define control_character_bit 0x40	    /* 0x000000, must be off. */
#define meta_character_bit 0x080	    /* x0000000, must be on. */
#define largest_char 255		    /* Largest character value. */

#define CTRL_CHAR(c) ((c) < control_character_threshold && (c) >= 0)
#define META_CHAR(c) ((c) > meta_character_threshold && (c) <= largest_char)

#define CTRL(c) ((c) & control_character_mask)
#define META(c) ((c) | meta_character_bit)

#define UNMETA(c) ((c) & (~meta_character_bit))
#define UNCTRL(c) _rl_to_upper(((c)|control_character_bit))

/* Old versions
#define _rl_lowercase_p(c) (((c) > ('a' - 1) && (c) < ('z' + 1)))
#define _rl_uppercase_p(c) (((c) > ('A' - 1) && (c) < ('Z' + 1)))
#define _rl_digit_p(c)  ((c) >= '0' && (c) <= '9')
*/

#define _rl_lowercase_p(c) (islower(c))
#define _rl_uppercase_p(c) (isupper(c))
#define _rl_digit_p(x)  (isdigit (x))

#define _rl_pure_alphabetic(c) (_rl_lowercase_p(c) || _rl_uppercase_p(c))
#define ALPHABETIC(c)	(_rl_lowercase_p(c) || _rl_uppercase_p(c) || _rl_digit_p(c))

/* Old versions
#  define _rl_to_upper(c) (_rl_lowercase_p(c) ? ((c) - 32) : (c))
#  define _rl_to_lower(c) (_rl_uppercase_p(c) ? ((c) + 32) : (c))
*/

#ifndef _rl_to_upper
#  define _rl_to_upper(c) (islower(c) ? toupper(c) : (c))
#  define _rl_to_lower(c) (isupper(c) ? tolower(c) : (c))
#endif

#ifndef _rl_digit_value
#define _rl_digit_value(x) ((x) - '0')
#endif

#ifndef NEWLINE
#define NEWLINE '\n'
#endif

#ifndef RETURN
#define RETURN CTRL('M')
#endif

#ifndef RUBOUT
#define RUBOUT 0x7f
#endif

#ifndef TAB
#define TAB '\t'
#endif

#ifdef ABORT_CHAR
#undef ABORT_CHAR
#endif
#define ABORT_CHAR CTRL('G')

#ifdef PAGE
#undef PAGE
#endif
#define PAGE CTRL('L')

#ifdef SPACE
#undef SPACE
#endif
#define SPACE ' '	/* XXX - was 0x20 */

#ifdef ESC
#undef ESC
#endif
#define ESC CTRL('[')

#ifndef ISOCTAL
#define ISOCTAL(c)      ((c) >= '0' && (c) <= '7')
#endif
#define OCTVALUE(c)     ((c) - '0')

#ifndef isxdigit
#  define isxdigit(c)   (isdigit((c)) || ((c) >= 'a' && (c) <= 'f') || ((c) >= 'A' && (c) <= 'F'))
#endif

#define HEXVALUE(c) \
  (((c) >= 'a' && (c) <= 'f') \
  	? (c)-'a'+10 \
  	: (c) >= 'A' && (c) <= 'F' ? (c)-'A'+10 : (c)-'0')

#endif  /* _CHARDEFS_H_ */
