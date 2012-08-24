/* Copyright (c) 2000, 2011, Oracle and/or its affiliates.
   Copyright (c) 2009-2011, Monty Program Ab

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "strings_def.h"
#include <m_ctype.h>
#include <stdarg.h>


/**
  Returns escaped string

  @param cs         string charset
  @param to         buffer where escaped string will be placed
  @param end        end of buffer
  @param par        string to escape
  @param par_len    string length
  @param quote_char character for quoting

  @retval
    position in buffer which points on the end of escaped string
*/

static char *backtick_string(char *to, char *end, char *par,
                             size_t par_len, char quote_char)
{
  char *start= to;
  char *par_end= par + par_len;
  size_t buff_length= (size_t) (end - to);

  if (buff_length <= par_len)
    goto err;
  *start++= quote_char;

  for ( ; par < par_end; ++par)
  {
    char c= *par;
    if (c == quote_char)
    {
      if (start + 1 >= end)
        goto err;
      *start++= quote_char;
    }
    if (start + 1 >= end)
      goto err;
    *start++= c;
  }
    
  if (start + 1 >= end)
    goto err;
  *start++= quote_char;
  return start;

err:
    *to='\0';
  return to;
}


/*
  Limited snprintf() implementations

  SYNOPSIS
    my_vsnprintf()
    to		Store result here
    n		Store up to n-1 characters, followed by an end 0
    fmt		printf format
    ap		Arguments

  IMPLEMENTION:
    Supports following formats:
    %#[l]d
    %#[l]u
    %#[l]x
    %#.#b 	Local format; note first # is ignored and second is REQUIRED
    %#.#s	Note first # is ignored
    
  RETURN
    length of result string
*/

size_t my_vsnprintf(char *to, size_t n, const char* fmt, va_list ap)
{
  char *start=to, *end=to+n-1;
  size_t length, width;
  uint pre_zero, have_long, escaped_arg;

  for (; *fmt ; fmt++)
  {
    if (*fmt != '%')
    {
      if (to == end)			/* End of buffer */
	break;
      *to++= *fmt;			/* Copy ordinary char */
      continue;
    }
    fmt++;					/* skip '%' */
    /* Read max fill size (only used with %d and %u) */
    if (*fmt == '-')
      fmt++;
    length= width= 0;
    pre_zero= have_long= escaped_arg= 0;
    if (*fmt == '*')
    {
      fmt++;
      length= va_arg(ap, int);
    }
    else
      for (; my_isdigit(&my_charset_latin1, *fmt); fmt++)
      {
        length= length * 10 + (uint)(*fmt - '0');
        if (!length)
          pre_zero= 1;			/* first digit was 0 */
      }
    if (*fmt == '.')
    {
      fmt++;
      if (*fmt == '*')
      {
        fmt++;
        width= va_arg(ap, int);
      }
      else
        for (; my_isdigit(&my_charset_latin1, *fmt); fmt++)
          width= width * 10 + (uint)(*fmt - '0');
    }
    else
      width= ~0;
    if (*fmt == 'l')
    {
      fmt++;
      have_long= 1;
    }
    if (*fmt == '`')
    {
      fmt++;
      escaped_arg= 1;
    }
    if (*fmt == 's')				/* String parameter */
    {
      reg2 char	*par = va_arg(ap, char *);
      size_t plen,left_len = (size_t) (end - to) + 1;
      if (!par) par = (char*)"(null)";
      plen= (uint) strnlen(par, width);
      if (left_len <= plen)
	plen = left_len - 1;
      if (escaped_arg)
        to= backtick_string(to, end, par, plen, '`');
      else
        to= strnmov(to,par,plen);
      continue;
    }
    else if (*fmt == 'b')				/* Buffer parameter */
    {
      char *par = va_arg(ap, char *);
      DBUG_ASSERT(to <= end);
      if (to + abs(width) + 1 > end)
        width= (uint) (end - to - 1);  /* sign doesn't matter */
      memmove(to, par, abs(width));
      to+= width;
      continue;
    }
    else if (*fmt == 'd' || *fmt == 'u'|| *fmt== 'x')	/* Integer parameter */
    {
      register long larg;
      size_t res_length, to_length;
      char *store_start= to, *store_end;
      char buff[32];

      if ((to_length= (size_t) (end-to)) < 16 || length)
	store_start= buff;
      if (have_long)
        larg = va_arg(ap, long);
      else
        if (*fmt == 'd')
          larg = va_arg(ap, int);
        else
          larg= (long) (uint) va_arg(ap, int);
      if (*fmt == 'd')
	store_end= int10_to_str(larg, store_start, -10);
      else
        if (*fmt== 'u')
          store_end= int10_to_str(larg, store_start, 10);
        else
          store_end= int2str(larg, store_start, 16, 0);
      if ((res_length= (size_t) (store_end - store_start)) > to_length)
	break;					/* num doesn't fit in output */
      /* If %#d syntax was used, we have to pre-zero/pre-space the string */
      if (store_start == buff)
      {
	length= min(length, to_length);
	if (res_length < length)
	{
	  size_t diff= (length- res_length);
	  bfill(to, diff, pre_zero ? '0' : ' ');
	  to+= diff;
	}
	bmove(to, store_start, res_length);
      }
      to+= res_length;
      continue;
    }
    else if (*fmt == 'c')                       /* Character parameter */
    {
      register int larg;
      if (to == end)
        break;
      larg = va_arg(ap, int);
      *to++= (char) larg;
      continue;
    }

    /* We come here on '%%', unknown code or too long parameter */
    if (to == end)
      break;
    *to++='%';				/* % used as % or unknown code */
  }
  DBUG_ASSERT(to <= end);
  *to='\0';				/* End of errmessage */
  return (size_t) (to - start);
}


size_t my_snprintf(char* to, size_t n, const char* fmt, ...)
{
  size_t result;
  va_list args;
  va_start(args,fmt);
  result= my_vsnprintf(to, n, fmt, args);
  va_end(args);
  return result;
}

/**
  Writes output to the stream according to a format string.

  @param stream     file to write to
  @param format     string format
  @param args       list of parameters

  @retval
    number of the characters written.
*/

int my_vfprintf(FILE *stream, const char* format, va_list args)
{
  char cvtbuf[1024];
  int alloc= 0;
  char *p= cvtbuf;
  size_t cur_len= sizeof(cvtbuf);
  int ret;

  /*
    We do not know how much buffer we need.
    So start with a reasonably-sized stack-allocated buffer, and increase
    it exponentially until it is big enough.
  */
  for (;;)
  {
    size_t new_len;
    size_t actual= my_vsnprintf(p, cur_len, format, args);
    if (actual < cur_len - 1)
      break;
    /*
      Not enough space (or just enough with nothing to spare - but we cannot
      distinguish this case from the return value). Allocate a bigger buffer
      and try again.
    */
    if (alloc)
      (*my_str_free)(p);
    else
      alloc= 1;
    new_len= cur_len*2;
    if (new_len < cur_len)
      return 0;                                 /* Overflow */
    cur_len= new_len;
    p= (*my_str_malloc)(cur_len);
    if (!p)
      return 0;
  }
  ret= fprintf(stream, "%s", p);
  if (alloc)
    (*my_str_free)(p);
  return ret;
}

int my_fprintf(FILE *stream, const char* format, ...)
{
  int result;
  va_list args;
  va_start(args, format);
  result= my_vfprintf(stream, format, args);
  va_end(args);
  return result;
}


#ifdef MAIN
#define OVERRUN_SENTRY  250
static void my_printf(const char * fmt, ...)
{
  char buf[33];
  int n;
  va_list ar;
  va_start(ar, fmt);
  buf[sizeof(buf)-1]=OVERRUN_SENTRY;
  n = my_vsnprintf(buf, sizeof(buf)-1,fmt, ar);
  printf(buf);
  printf("n=%d, strlen=%d\n", n, strlen(buf));
  if ((uchar) buf[sizeof(buf)-1] != OVERRUN_SENTRY)
  {
    fprintf(stderr, "Buffer overrun\n");
    abort();
  }
  va_end(ar);
}


int main()
{

  my_printf("Hello\n");
  my_printf("Hello int, %d\n", 1);
  my_printf("Hello string '%s'\n", "I am a string");
  my_printf("Hello hack hack hack hack hack hack hack %d\n", 1);
  my_printf("Hello %d hack  %d\n", 1, 4);
  my_printf("Hello %d hack hack hack hack hack %d\n", 1, 4);
  my_printf("Hello '%s' hhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhh\n", "hack");
  my_printf("Hello hhhhhhhhhhhhhh %d sssssssssssssss\n", 1);
  my_printf("Hello  %u\n", 1);
  my_printf("Hex:   %lx  '%6lx'\n", 32, 65);
  my_printf("conn %ld to: '%-.64s' user: '%-.32s' host:\
 `%-.64s' (%-.64s)", 1, 0,0,0,0);
  return 0;
}
#endif
