/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.
   
   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA */

/*
  A quicker atof. About 2-10 times faster than standard atof on sparc.
  This don't handle iee-options (NaN...) and the presission :s is a little
  less for some high exponential numbers (+-1 at 14th place).
  Returns 0.0 if overflow or wrong number.
  Must be inited with init_my_atof to handle possibly overflows.
*/

#include <my_global.h>
#ifdef USE_MY_ATOF				/* Skipp if we don't want it */
#include <m_ctype.h>
#include <floatingpoint.h>
#include <signal.h>

/* Read a double. If float is wrong return 0.
   float ::= [space]* [sign] {digit}+  decimal-point {digit}+ [exponent] |
   [sign] {digit}+  [decimal-point {digit}*] exponent |
   [sign] {digit}+  decimal-point [{digit}*] exponent |
   [sign] decimal-point {digit}* exponent |
   exponent :: = exponent-marker [sign] {digit}+
   exponent-marker ::= E e
   */


#define is_exponent_marker(ch) (ch == 'E' || ch == 'e')

static void my_atof_overflow  _A((int sig,int code, struct sigcontext *scp,
			      char *addr));
static int parse_sign _A((char **str));
static void parse_float_number_part _A((char **str,double *number, int *length));
static void parse_decimal_number_part _A((char **str,double *number));
static int parse_int_number_part _A((char **str,uint *number));

static int volatile overflow,in_my_atof;
static sigfpe_handler_type old_overflow_handler;

void init_my_atof()
{
  old_overflow_handler = (sigfpe_handler_type)
    ieee_handler("get", "overflow", old_overflow_handler);
  VOID(ieee_handler("set", "overflow", my_atof_overflow));
  return;
}

static void my_atof_overflow(sig, code, scp, addr)
int sig;
int code;
struct sigcontext *scp;
char *addr;
{
  if (!in_my_atof)
    old_overflow_handler(sig,code,scp,addr);
  else
    overflow=1;
  return;
}

double my_atof(src)
const char *src;
{
  int		sign, exp_sign; /* is number negative (+1) or positive (-1) */
  int		length_before_point;
  double	after_point;	/* Number after decimal point and before exp */
  uint		exponent;	/* Exponent value */
  double	exp_log,exp_val;
  char		*tmp_src;
  double	result_number;

  tmp_src = (char*) src;
  while (isspace(tmp_src[0]))
    tmp_src++;					/* Skipp pre-space */
  sign = parse_sign(&tmp_src);
  overflow=0;
  in_my_atof=1;
  parse_float_number_part(&tmp_src, &result_number, &length_before_point);
  if (*tmp_src == '.')
  {
    tmp_src++;
    parse_decimal_number_part(&tmp_src, &after_point);
    result_number += after_point;
  }
  else if (length_before_point == 0)
  {
    in_my_atof=0;
    return 0.0;
  }
  if (is_exponent_marker(*tmp_src))
  {
    tmp_src++;
    exp_sign = parse_sign(&tmp_src);
    overflow|=parse_int_number_part(&tmp_src, &exponent);

    exp_log=10.0; exp_val=1.0;
    for (;;)
    {
      if (exponent & 1)
      {
	exp_val*= exp_log;
	exponent--;
      }
      if (!exponent)
	break;
      exp_log*=exp_log;
      exponent>>=1;
    }
    if (exp_sign < 0)
      result_number*=exp_val;
    else
      result_number/=exp_val;
  }
  if (sign > 0)
    result_number= -result_number;

  in_my_atof=0;
  if (overflow)
    return 0.0;
  return result_number;
}


static int parse_sign(str)
char **str;
{
  if (**str == '-')
  {
    (*str)++;
    return 1;
  }
  if (**str == '+')
    (*str)++;
  return -1;
}

	/* Get number with may be separated with ',' */

static void parse_float_number_part(str, number, length)
char **str;
double *number;
int *length;
{
  *number = 0;
  *length = 0;

  for (;;)
  {
    while (isdigit(**str))
    {
      (*length)++;
      *number = (*number * 10) + (**str - '0');
      (*str)++;
    }
    if (**str != ',')
      return;					/* Skipp possibly ',' */
    (*str)++;
  }
}

static void parse_decimal_number_part(str, number)
char **str;
double *number;
{
  double exp_log;

  *number = 0;
  exp_log=1/10.0;
  while (isdigit(**str))
  {
    *number+= (**str - '0')*exp_log;
    exp_log/=10;
    (*str)++;
  }
}

	/* Parses int suitably for exponent */

static int parse_int_number_part(str, number)
char **str;
uint *number;
{
  *number = 0;
  while (isdigit(**str))
  {
    if (*number >= ((uint) ~0)/10)
      return 1;						/* Don't overflow */
    *number = (*number * 10) + **str - '0';
    (*str)++;
  }
  return 0;
}

#endif
