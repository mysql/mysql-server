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

/* Test of all stringfunktions that is coded in assembler */

#include <global.h>
#include <stdarg.h>
#include "m_string.h"

#define F_LEN	8
#define F_CHAR	'A'
#define F_FILL	'B'
#define T_LEN	15
#define T_CHAR	'D'
#define T_FILL	'E'
#define F_PREFILL '0'
#define T_PREFILL '1'

static char from_buff[100],to_buff[100];
static my_string from,to;
static int errors,tests;
static int test_strarg(const char *name,...);
static void init_strings (void);	/* Init from and to */
void test_arg (const char *message,long func_value,long value);
int compare_buff(const char *message,my_string b1,my_string b2,int length,
		  pchar fill, pchar prefill);

static int my_test(int a)
{
  return a ? 1 : 0;
}

int main(void)
{
  static char v1[]="Monty",v2[]="on",v3[]="Montys",v4[]="ty",v5[]="gr",
              v6[]="hohohoo",v7[]="hohoo",v8[]="hohooo",v9[]="t",
	      cont[]="qwet";
  errors=tests=0;
  init_strings();

  test_arg("bcmp(from,to,5)",(long) my_test(bcmp(from,to,5)),1L);
  test_arg("bcmp(from,from,5)",(long) bcmp(from,from,5),0L);

  test_arg("bcmp(from,to,0)",(long) bcmp(from,to,0),0L);
  test_arg("strend(from)",(long) strend(from),(long) from+F_LEN);
  test_arg("strchr(v1,'M')",(long) strchr(v1,'M'),(long) v1);
  test_arg("strchr(v1,'y')",(long) strchr(v1,'y'),(long) v1+4);
  test_arg("strchr(v1,'x')",(long) strchr(v1,'x'),0L);
  test_arg("strcont(v1,cont)",(long) strcont(v1,cont),(long) v1+3);
  test_arg("strcont(v1,v2)",(long) strcont(v1,v2),(long) v1+1);
  test_arg("strcont(v1,v5)",(long) strcont(v1,v5),0L);
  test_arg("is_prefix(v3,v1)",(long) is_prefix(v3,v1),1L);
  test_arg("is_prefix(v1,v3)",(long) is_prefix(v1,v3),0L);
  test_arg("is_prefix(v3,v4)",(long) is_prefix(v3,v4),0L);
  test_arg("strstr(v1,v1)",(long) strstr(v1,v1),(long) v1);
  test_arg("strstr(v1,v2)",(long) strstr(v1,v2),(long) v1+1);
  test_arg("strstr(v1,v4)",(long) strstr(v1,v4),(long) v1+3);
  test_arg("strstr(v6,v7)",(long) strstr(v6,v7),(long) v6+2);
  test_arg("strstr(v1,v9)",(long) strstr(v1,v9),(long) v1+3);
  test_arg("strstr(v1,v3)",(long) strstr(v1,v3),0L);
  test_arg("strstr(v1,v5)",(long) strstr(v1,v5),0L);
  test_arg("strstr(v6,v8)",(long) strstr(v6,v8),0L);

  test_arg("strinstr(v1,v4)",(long) strinstr(v1,v4),4L);
  test_arg("strinstr(v1,v5)",(long) strinstr(v1,v5),0L);
  test_arg("strlen(from)",(long) strlen(from),(long) F_LEN);
  test_arg("strlen(\"\")",(long) strlen(""),0L);
#ifdef HAVE_STRNLEN
  test_arg("strnlen(from,3)",(long) strnlen(from,3),3L);
  test_arg("strnlen(from,0)",(long) strnlen(from,0),0L);
  test_arg("strnlen(from,1000)",(long) strnlen(from,1000),(long) F_LEN);
#endif

  test_strarg("bfill(to,4,' ')",(bfill(to,4,' '),0L),INT_MAX32,4,' ',0,0);
  test_strarg("bfill(from,0,' ')",(bfill(from,0,' '),0L),INT_MAX32,0,0);
  test_strarg("bzero(to,3)",(bzero(to,3),0L),INT_MAX32,3,0,0,0);
  test_strarg("bzero(to,0)",(bzero(to,0),0L),INT_MAX32,0,0);
  test_strarg("bmove(to,from,4)",(bmove(to,from,4),0L),INT_MAX32,4,F_CHAR,
	      0,0);
  test_strarg("bmove(to,from,0)",(bmove(to,from,0),0L),INT_MAX32,0,0);
  test_strarg("bmove_upp(to+6,from+6,3)",(bmove_upp(to+6,from+6,3),0L),INT_MAX32,
	       3,T_CHAR,3,F_CHAR,0,0);
  test_strarg("bmove_upp(to,from,0)",(bmove_upp(to,from,0),0L),INT_MAX32,0,0);
  test_strarg("strappend(to,3,' ')",(strappend(to,3,' '),0L),INT_MAX32,
	      3,T_CHAR,1,0,T_LEN-4,T_CHAR,1,0,0,0);
  test_strarg("strappend(to,T_LEN+5,' ')",(strappend(to,T_LEN+5,' '),0L),INT_MAX32,
	       T_LEN,T_CHAR,5,' ',1,0,0,0);
  test_strarg("strcat(to,from)",strcat(to,from),to,T_LEN,T_CHAR,
	      F_LEN,F_CHAR,1,0,0,0);
  test_strarg("strcat(to,\"\")",strcat(to,""),INT_MAX32,0,0);
  test_strarg("strfill(to,4,' ')",strfill(to,4,' '),to+4,4,' ',1,0,0,0);
  test_strarg("strfill(from,0,' ')",strfill(from,0,' '),from,0,1,0,0);
  test_strarg("strmake(to,from,4)",strmake(to,from,4),to+4,4,F_CHAR,
	      1,0,0,0);
  test_strarg("strmake(to,from,0)",strmake(to,from,0),to+0,1,0,0,0);
  test_strarg("strmov(to,from)",strmov(to,from),to+F_LEN,F_LEN,F_CHAR,0,0);
  test_strarg("strmov(to,\"\")",strmov(to,""),to,1,0,0,0);
  test_strarg("strnmov(to,from,2)",strnmov(to,from,2),to+2,2,F_CHAR,0,0);
  test_strarg("strnmov(to,from,F_LEN+5)",strnmov(to,from,F_LEN+5),to+F_LEN,
	       F_LEN,F_CHAR,1,0,0,0);
  test_strarg("strnmov(to,\"\",2)",strnmov(to,"",2),to,1,0,0,0);
  test_strarg("strxmov(to,from,\"!!\",NullS)",strxmov(to,from,"!!",NullS),to+F_LEN+2,F_LEN,F_CHAR,2,'!',0,0,0);
  test_strarg("strxmov(to,NullS)",strxmov(to,NullS),to,1,0,0,0);
  test_strarg("strxmov(to,from,from,from,from,from,'!!',from,NullS)",strxmov(to,from,from,from,from,from,"!!",from,NullS),to+F_LEN*6+2,F_LEN,F_CHAR,F_LEN,F_CHAR,F_LEN,F_CHAR,F_LEN,F_CHAR,F_LEN,F_CHAR,2,'!',F_LEN,F_CHAR,1,0,0,0);

  test_strarg("strxnmov(to,100,from,\"!!\",NullS)",strxnmov(to,100,from,"!!",NullS),to+F_LEN+2,F_LEN,F_CHAR,2,'!',0,0,0);
  test_strarg("strxnmov(to,2,NullS)",strxnmov(to,2,NullS),to,1,0,0,0);
  test_strarg("strxnmov(to,100,from,from,from,from,from,'!!',from,NullS)",strxnmov(to,100,from,from,from,from,from,"!!",from,NullS),to+F_LEN*6+2,F_LEN,F_CHAR,F_LEN,F_CHAR,F_LEN,F_CHAR,F_LEN,F_CHAR,F_LEN,F_CHAR,2,'!',F_LEN,F_CHAR,1,0,0,0);
  test_strarg("strxnmov(to,2,\"!!!\",NullS)",strxnmov(to,2,"!!!",NullS),to+2,2,'!',0,0,0);
  test_strarg("strxnmov(to,2,\"!!\",NullS)",strxnmov(to,2,"!!","xx",NullS),to+2,2,'!',0,0,0);
  test_strarg("strxnmov(to,2,\"!\",\"x\",\"y\",NullS)",strxnmov(to,2,"!","x","y",NullS),to+2,1,'!',1,'x',0,0,0);

  test_strarg("bchange(to,2,from,4,6)",(bchange(to,2,from,4,6),0L),INT_MAX32,
	      4,F_CHAR,2,T_CHAR,0,0);

  printf("tests: %d  errors: %d\n",tests,errors);
  if (errors)
    fputs("--- Some functions doesn't work!! Fix them\n",stderr);
  return(errors > 0);

  fputs("Fatal error\n",stderr);
  return(2);
} /* main */


	/* Init strings */

void init_strings(void)
{
  reg1 int i;
  reg2 char *pos;

  from=from_buff+3; to=to_buff+3;

  pos=from_buff; *pos++= F_FILL; *pos++=F_FILL; *pos++=F_PREFILL;
  for (i=0 ; i < F_LEN ; i++)
    *pos++=F_CHAR;
  *pos++=0;
  for (i=0; i<50 ; i++)
    *pos++= F_FILL;

  pos=to_buff; *pos++= T_FILL; *pos++=T_FILL; *pos++=T_PREFILL;
  for (i=0 ; i < T_LEN ; i++)
    *pos++=T_CHAR;
  *pos++=0;
  for (i=0; i<50 ; i++)
    *pos++= T_FILL;
} /* init_strings */


	/* Test that function return rigth value */

void test_arg(const char *message, long int func_value, long int value)
{
  tests++;
  printf("testing '%s'\n",message);
  if (func_value != value)
  {
    printf("func: '%s' = %ld   Should be: %ld\n",message,func_value,value);
    errors++;
  }
} /* test_arg */

	/* Test function return value and from and to arrays */

static int test_strarg(const char *message,...)
{
  long func_value,value;
  int error,length;
  char chr,cmp_buff[100],*pos,*pos2;
  va_list pvar;

  tests++;
  va_start(pvar,message);
  func_value=va_arg(pvar,long);
  value=va_arg(pvar,long);

  printf("testing '%s'\n",message);
  if (func_value != value && value != INT_MAX32)
  {
    printf("func: '%s' = %ld   Should be: %ld\n",message,func_value,value);
    errors++;
  }
  pos= cmp_buff;
  while ((length = va_arg(pvar, int)) != 0)
  {
    chr= (char) (va_arg(pvar, int));
    while (length--)
      *pos++=chr;
  }
  pos2=to+ (int)(pos-cmp_buff);
  while (pos <= cmp_buff+T_LEN)
    *pos++= *pos2++;
  if (compare_buff(message,to,cmp_buff,(int) (pos-cmp_buff),T_FILL,T_PREFILL))
  {
    init_strings();
    va_end(pvar);
    return 1;
  }

  pos= cmp_buff;
  while ((length = va_arg(pvar, int)) != 0)
  {
    chr= (char) (va_arg(pvar, int));
    while (length--)
      *pos++=chr;
  }
  pos2=from+ (int)(pos-cmp_buff);
  while (pos <= cmp_buff+F_LEN)
    *pos++= *pos2++;
  error=compare_buff(message,from,cmp_buff,(int) (pos-cmp_buff),F_FILL,F_PREFILL);
  init_strings();
  va_end(pvar);
  return (error != 0);
} /* test_strarg */


	/* test if function made right value */

int compare_buff(const char *message, my_string b1, my_string b2, int length,
		 pchar fill, pchar prefill)
{
  int i,error=0;

  if (bcmp(b1,b2,length))
  {
    errors++;
    printf("func: '%s'   Buffers differ\nIs:        ",message);
    for (i=0 ; i<length ; i++)
      printf("%3d ",b1[i]);
    printf("\nShould be: ");
    for (i=0 ; i<length ; i++)
      printf("%3d ",b2[i]);
    puts("");
  }
  else if (b1[-1] != prefill || b1[-2] != fill || b1[-3] != fill)
  {
    printf("func: '%s'   Chars before buffer is changed\n",message);
    errors++;
    error=1;
  }
  else if (b1[length] != fill || b1[length+1] != fill)
  {
    printf("func: '%s'   Chars after buffer is changed\n",message);
    errors++;
    error=1;
  }
  return error;
} /* compare_buff */

	/* These are here to be loaded and examined */

extern void dummy_functions(void);

void dummy_functions(void)
{
  VOID(memchr(from,'a',5));
  VOID(memcmp(from,to,5));
  VOID(memcpy(from,to,5));
  VOID(memset(from,' ',5));
  VOID(strcmp(from,to));
  VOID(strcpy(from,to));
  VOID(strstr(from,to));
  VOID(strrchr(from,'a'));
  return;
}
