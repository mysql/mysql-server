/* Copyright (C) 2000 MySQL AB

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
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include <my_global.h>
#include <m_string.h>
#include <stdarg.h>
#include <m_ctype.h>

/*
  Limited snprintf() implementations

  IMPLEMENTION:
    Supports following formats:
    %#[l]d
    %#[l]u
    %#[l]x
    %#.#s	Note first # is ignored

  RETURN
    length of result string
*/

int my_vsnprintf(char *to, size_t n, const char* fmt, va_list ap)
{
  char *start=to, *end=to+n-1;
  uint length, width, pre_zero, have_long;

  for (; *fmt ; fmt++)
  {
    if (fmt[0] != '%')
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
    length= width= pre_zero= have_long= 0;
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
    if (*fmt == 's')				/* String parameter */
    {
      reg2 char	*par = va_arg(ap, char *);
      uint plen,left_len = (uint)(end-to)+1;
      if (!par) par = (char*)"(null)";
      plen = (uint) strlen(par);
      set_if_smaller(plen,width);
      if (left_len <= plen)
	plen = left_len - 1;
      to=strnmov(to,par,plen);
      continue;
    }
    else if (*fmt == 'd' || *fmt == 'u'|| *fmt== 'x')	/* Integer parameter */
    {
      register long larg;
      uint res_length, to_length;
      char *store_start= to, *store_end;
      char buff[32];

      if ((to_length= (uint) (end-to)) < 16 || length)
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
      if ((res_length= (uint) (store_end - store_start)) > to_length)
	break;					/* num doesn't fit in output */
      /* If %#d syntax was used, we have to pre-zero/pre-space the string */
      if (store_start == buff)
      {
	length= min(length, to_length);
	if (res_length < length)
	{
	  uint diff= (length- res_length);
	  bfill(to, diff, pre_zero ? '0' : ' ');
	  to+= diff;
	}
	bmove(to, store_start, res_length);
      }
      to+= res_length;
      continue;
    }
    /* We come here on '%%', unknown code or too long parameter */
    if (to == end)
      break;
    *to++='%';				/* % used as % or unknown code */
  }
  DBUG_ASSERT(to <= end);
  *to='\0';				/* End of errmessage */
  return (uint) (to - start);
}

int my_snprintf(char* to, size_t n, const char* fmt, ...)
{
  int result;
  va_list args;
  va_start(args,fmt);
  result= my_vsnprintf(to, n, fmt, args);
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
