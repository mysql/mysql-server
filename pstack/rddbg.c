/* rddbg.c -- Read debugging information into a generic form.
   Copyright (C) 1995, 96, 1997 Free Software Foundation, Inc.
   Written by Ian Lance Taylor <ian@cygnus.com>.

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
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.  */

/* This file reads debugging information into a generic form.  This
   file knows how to dig the debugging information out of an object
   file.  */

#include <bfd.h>
#include "bucomm.h"
#include <libiberty.h>
#include "debug.h"
#include "budbg.h"

static boolean read_section_stabs_debugging_info
  PARAMS ((bfd *, asymbol **, long, PTR, boolean *));
static boolean read_symbol_stabs_debugging_info
  PARAMS ((bfd *, asymbol **, long, PTR, boolean *));
static boolean read_ieee_debugging_info PARAMS ((bfd *, PTR, boolean *));
static void save_stab PARAMS ((int, int, bfd_vma, const char *));
static void stab_context PARAMS ((void));
static void free_saved_stabs PARAMS ((void));

/* Read debugging information from a BFD.  Returns a generic debugging
   pointer.  */

PTR
read_debugging_info (abfd, syms, symcount)
     bfd *abfd;
     asymbol **syms;
     long symcount;
{
  PTR dhandle;
  boolean found;

  dhandle = debug_init ();
  if (dhandle == NULL)
    return NULL;

  if (! read_section_stabs_debugging_info (abfd, syms, symcount, dhandle,
					   &found))
    return NULL;

  if (bfd_get_flavour (abfd) == bfd_target_aout_flavour)
    {
      if (! read_symbol_stabs_debugging_info (abfd, syms, symcount, dhandle,
					      &found))
	return NULL;
    }

  if (bfd_get_flavour (abfd) == bfd_target_ieee_flavour)
    {
      if (! read_ieee_debugging_info (abfd, dhandle, &found))
	return NULL;
    }

  /* Try reading the COFF symbols if we didn't find any stabs in COFF
     sections.  */
  if (! found
      && bfd_get_flavour (abfd) == bfd_target_coff_flavour
      && symcount > 0)
    {
#if 0
/*
 * JZ: Do we need coff?
 */
      if (! parse_coff (abfd, syms, symcount, dhandle))
#else
      fprintf (stderr, "%s: COFF support temporarily disabled\n",
	       bfd_get_filename (abfd));
      return NULL;
#endif
	return NULL;
      found = true;
    }

  if (! found)
    {
      fprintf (stderr, "%s: no recognized debugging information\n",
	       bfd_get_filename (abfd));
      return NULL;
    }

  return dhandle;
}

/* Read stabs in sections debugging information from a BFD.  */

static boolean
read_section_stabs_debugging_info (abfd, syms, symcount, dhandle, pfound)
     bfd *abfd;
     asymbol **syms;
     long symcount;
     PTR dhandle;
     boolean *pfound;
{
  static struct
    {
      const char *secname;
      const char *strsecname;
    } names[] = { { ".stab", ".stabstr" } };
  unsigned int i;
  PTR shandle;

  *pfound = false;
  shandle = NULL;

  for (i = 0; i < sizeof names / sizeof names[0]; i++)
    {
      asection *sec, *strsec;

      sec = bfd_get_section_by_name (abfd, names[i].secname);
      strsec = bfd_get_section_by_name (abfd, names[i].strsecname);
      if (sec != NULL && strsec != NULL)
	{
	  bfd_size_type stabsize, strsize;
	  bfd_byte *stabs, *strings;
	  bfd_byte *stab;
	  bfd_size_type stroff, next_stroff;

	  stabsize = bfd_section_size (abfd, sec);
	  stabs = (bfd_byte *) xmalloc (stabsize);
	  if (! bfd_get_section_contents (abfd, sec, stabs, 0, stabsize))
	    {
	      fprintf (stderr, "%s: %s: %s\n",
		       bfd_get_filename (abfd), names[i].secname,
		       bfd_errmsg (bfd_get_error ()));
	      return false;
	    }

	  strsize = bfd_section_size (abfd, strsec);
	  strings = (bfd_byte *) xmalloc (strsize);
	  if (! bfd_get_section_contents (abfd, strsec, strings, 0, strsize))
	    {
	      fprintf (stderr, "%s: %s: %s\n",
		       bfd_get_filename (abfd), names[i].strsecname,
		       bfd_errmsg (bfd_get_error ()));
	      return false;
	    }

	  if (shandle == NULL)
	    {
	      shandle = start_stab (dhandle, abfd, true, syms, symcount);
	      if (shandle == NULL)
		return false;
	    }

	  *pfound = true;

	  stroff = 0;
	  next_stroff = 0;
	  for (stab = stabs; stab < stabs + stabsize; stab += 12)
	    {
	      bfd_size_type strx;
	      int type;
	      int other;
	      int desc;
	      bfd_vma value;

	      /* This code presumes 32 bit values.  */

	      strx = bfd_get_32 (abfd, stab);
	      type = bfd_get_8 (abfd, stab + 4);
	      other = bfd_get_8 (abfd, stab + 5);
	      desc = bfd_get_16 (abfd, stab + 6);
	      value = bfd_get_32 (abfd, stab + 8);

	      if (type == 0)
		{
		  /* Special type 0 stabs indicate the offset to the
                     next string table.  */
		  stroff = next_stroff;
		  next_stroff += value;
		}
	      else
		{
		  char *f, *s;

		  f = NULL;
		  s = (char *) strings + stroff + strx;
		  while (s[strlen (s) - 1] == '\\'
			 && stab + 12 < stabs + stabsize)
		    {
		      char *p;

		      stab += 12;
		      p = s + strlen (s) - 1;
		      *p = '\0';
		      s = concat (s,
				  ((char *) strings
				   + stroff
				   + bfd_get_32 (abfd, stab)),
				  (const char *) NULL);

		      /* We have to restore the backslash, because, if
                         the linker is hashing stabs strings, we may
                         see the same string more than once.  */
		      *p = '\\';

		      if (f != NULL)
			free (f);
		      f = s;
		    }

		  save_stab (type, desc, value, s);

		  if (! parse_stab (dhandle, shandle, type, desc, value, s))
		    {
#if 0
/*
 * JZ: skip the junk.
 */
		      stab_context ();
		      free_saved_stabs ();
		      return false;
#endif
		    }

		  /* Don't free f, since I think the stabs code
                     expects strings to hang around.  This should be
                     straightened out.  FIXME.  */
		}
	    }

	  free_saved_stabs ();
	  free (stabs);

	  /* Don't free strings, since I think the stabs code expects
             the strings to hang around.  This should be straightened
             out.  FIXME.  */
	}
    }

  if (shandle != NULL)
    {
      if (! finish_stab (dhandle, shandle))
	return false;
    }

  return true;
}

/* Read stabs in the symbol table.  */

static boolean
read_symbol_stabs_debugging_info (abfd, syms, symcount, dhandle, pfound)
     bfd *abfd;
     asymbol **syms;
     long symcount;
     PTR dhandle;
     boolean *pfound;
{
  PTR shandle;
  asymbol **ps, **symend;

  shandle = NULL;
  symend = syms + symcount;
  for (ps = syms; ps < symend; ps++)
    {
      symbol_info i;

      bfd_get_symbol_info (abfd, *ps, &i);

      if (i.type == '-')
	{
	  const char *s;
	  char *f;

	  if (shandle == NULL)
	    {
	      shandle = start_stab (dhandle, abfd, false, syms, symcount);
	      if (shandle == NULL)
		return false;
	    }

	  *pfound = true;

	  s = i.name;
	  f = NULL;
	  while (s[strlen (s) - 1] == '\\'
		 && ps + 1 < symend)
	    {
	      char *sc, *n;

	      ++ps;
	      sc = xstrdup (s);
	      sc[strlen (sc) - 1] = '\0';
	      n = concat (sc, bfd_asymbol_name (*ps), (const char *) NULL);
	      free (sc);
	      if (f != NULL)
		free (f);
	      f = n;
	      s = n;
	    }

	  save_stab (i.stab_type, i.stab_desc, i.value, s);

	  if (! parse_stab (dhandle, shandle, i.stab_type, i.stab_desc,
			    i.value, s))
	    {
	      stab_context ();
	      free_saved_stabs ();
	      return false;
	    }

	  /* Don't free f, since I think the stabs code expects
	     strings to hang around.  This should be straightened out.
	     FIXME.  */
	}
    }

  free_saved_stabs ();

  if (shandle != NULL)
    {
      if (! finish_stab (dhandle, shandle))
	return false;
    }

  return true;
}

/* Read IEEE debugging information.  */

static boolean
read_ieee_debugging_info (abfd, dhandle, pfound)
     bfd *abfd;
     PTR dhandle;
     boolean *pfound;
{
  asection *dsec;
  bfd_size_type size;
  bfd_byte *contents;

  /* The BFD backend puts the debugging information into a section
     named .debug.  */

  dsec = bfd_get_section_by_name (abfd, ".debug");
  if (dsec == NULL)
    return true;

  size = bfd_section_size (abfd, dsec);
  contents = (bfd_byte *) xmalloc (size);
  if (! bfd_get_section_contents (abfd, dsec, contents, 0, size))
    return false;

  if (! parse_ieee (dhandle, abfd, contents, size))
    return false;

  free (contents);

  *pfound = true;

  return true;
}

/* Record stabs strings, so that we can give some context for errors.  */

#define SAVE_STABS_COUNT (16)

struct saved_stab
{
  int type;
  int desc;
  bfd_vma value;
  char *string;
};

static struct saved_stab saved_stabs[SAVE_STABS_COUNT];
static int saved_stabs_index;

/* Save a stabs string.  */

static void
save_stab (type, desc, value, string)
     int type;
     int desc;
     bfd_vma value;
     const char *string;
{
  if (saved_stabs[saved_stabs_index].string != NULL)
    free (saved_stabs[saved_stabs_index].string);
  saved_stabs[saved_stabs_index].type = type;
  saved_stabs[saved_stabs_index].desc = desc;
  saved_stabs[saved_stabs_index].value = value;
  saved_stabs[saved_stabs_index].string = xstrdup (string);
  saved_stabs_index = (saved_stabs_index + 1) % SAVE_STABS_COUNT;
}

/* Provide context for an error.  */

static void
stab_context ()
{
  int i;

  fprintf (stderr, "Last stabs entries before error:\n");
  fprintf (stderr, "n_type n_desc n_value  string\n");

  i = saved_stabs_index;
  do
    {
      struct saved_stab *stabp;

      stabp = saved_stabs + i;
      if (stabp->string != NULL)
	{
	  const char *s;

	  s = bfd_get_stab_name (stabp->type);
	  if (s != NULL)
	    fprintf (stderr, "%-6s", s);
	  else if (stabp->type == 0)
	    fprintf (stderr, "HdrSym");
	  else
	    fprintf (stderr, "%-6d", stabp->type);
	  fprintf (stderr, " %-6d ", stabp->desc);
	  fprintf_vma (stderr, stabp->value);
	  if (stabp->type != 0)
	    fprintf (stderr, " %s", stabp->string);
	  fprintf (stderr, "\n");
	}
      i = (i + 1) % SAVE_STABS_COUNT;
    }
  while (i != saved_stabs_index);
}

/* Free the saved stab strings.  */

static void
free_saved_stabs ()
{
  int i;

  for (i = 0; i < SAVE_STABS_COUNT; i++)
    {
      if (saved_stabs[i].string != NULL)
	{
	  free (saved_stabs[i].string);
	  saved_stabs[i].string = NULL;
	}
    }

  saved_stabs_index = 0;
}
