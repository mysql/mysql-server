/* bucomm.h -- binutils common include file.
   Copyright (C) 1992, 93, 94, 95, 96, 1997 Free Software Foundation, Inc.

This file is part of GNU Binutils.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#ifndef _BUCOMM_H
#define _BUCOMM_H

#include "ansidecl.h"
#include <stdio.h>
#include <sys/types.h>

#include <errno.h>
#include <unistd.h>

#include <string.h>

#include <stdlib.h>

#include <fcntl.h>

#ifdef __GNUC__
# undef alloca
# define alloca __builtin_alloca
#else
# if HAVE_ALLOCA_H
#  include <alloca.h>
# else
#  ifndef alloca /* predefined by HP cc +Olibcalls */
#   if !defined (__STDC__) && !defined (__hpux)
char *alloca ();
#   else
void *alloca ();
#   endif /* __STDC__, __hpux */
#  endif /* alloca */
# endif /* HAVE_ALLOCA_H */
#endif

/* bucomm.c */
void bfd_nonfatal PARAMS ((CONST char *));

void bfd_fatal PARAMS ((CONST char *));

void fatal PARAMS ((CONST char *, ...));

void set_default_bfd_target PARAMS ((void));

void list_matching_formats PARAMS ((char **p));

void list_supported_targets PARAMS ((const char *, FILE *));

void print_arelt_descr PARAMS ((FILE *file, bfd *abfd, boolean verbose));

char *make_tempname PARAMS ((char *));

bfd_vma parse_vma PARAMS ((const char *, const char *));

extern char *program_name;

/* filemode.c */
void mode_string PARAMS ((unsigned long mode, char *buf));

/* version.c */
extern void print_version PARAMS ((const char *));

/* libiberty */
PTR xmalloc PARAMS ((size_t));

PTR xrealloc PARAMS ((PTR, size_t));

#endif /* _BUCOMM_H */
