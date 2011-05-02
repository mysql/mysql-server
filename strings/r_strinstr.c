/* Copyright (c) 2000 David Axmark
   Copyright (c) 2000 TXT DataKonsult Ab & Monty Program Ab
   Copyright (c) 2009-2011, Monty Program Ab

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:

   1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.

   2. Redistributions in binary form must the following disclaimer in
     the documentation and/or other materials provided with the
     distribution.

   THIS SOFTWARE IS PROVIDED BY <COPYRIGHT HOLDER> ``AS IS'' AND ANY
   EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
   PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
   USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
   ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
   OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
   OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
   SUCH DAMAGE.
*/

/*
  Author : David
  strintstr(src, from, pat) looks for an instance of pat in src
  backwards from pos from.  pat is not a regex(3) pattern, it is a literal
  string which must be matched exactly.
  The result 0 if the pattern was not found else it is the start char of
  the pattern counted from the begining of the string.
*/

#include "strings_def.h"

size_t r_strinstr(reg1 const char * str, size_t from, reg4 const char * search)
{
  reg2 const char *i, *j;
  size_t	len = strlen(search);
  /* pointer to the last char of buff */
  const char *	start = str + from - 1;
  /* pointer to the last char of search */
  const char *	search_end = search + len - 1;

 skip:
  while (start >= str)		/* Cant be != because the first char */
  {
    if (*start-- == *search_end)
    {
      i = start; j = search_end - 1;
      while (j >= search && start > str)
	if (*i-- != *j--)
	  goto skip;
      return (size_t) ((start - len) - str + 3);
    }
  }
  return (0);
}
