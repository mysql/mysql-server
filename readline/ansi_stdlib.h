/* ansi_stdlib.h -- An ANSI Standard stdlib.h. */
/* A minimal stdlib.h containing extern declarations for those functions
   that bash uses. */

/* Copyright (C) 1993 Free Software Foundation, Inc.

   This file is part of GNU Bash, the Bourne Again SHell.

   Bash is free software; you can redistribute it and/or modify it under
   the terms of the GNU General Public License as published by the Free
   Software Foundation; either version 2, or (at your option) any later
   version.

   Bash is distributed in the hope that it will be useful, but WITHOUT ANY
   WARRANTY; without even the implied warranty of MERCHANTABILITY or
   FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
   for more details.

   You should have received a copy of the GNU General Public License along
   with Bash; see the file COPYING.  If not, write to the Free Software
   Foundation, 675 Mass Ave, Cambridge, MA 02139, USA. */

#if !defined (_STDLIB_H_)
#define	_STDLIB_H_ 1

/* String conversion functions. */
extern int atoi ();
extern long int atol ();

/* Memory allocation functions. */
extern char *malloc ();
extern char *realloc ();
extern void free ();

/* Other miscellaneous functions. */
extern void abort ();
extern void exit ();
extern char *getenv ();
extern void qsort ();

#endif /* _STDLIB_H  */
