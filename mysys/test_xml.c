/* Copyright (c) 2000, 2002 MySQL AB
   Use is subject to license terms

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "my_xml.h"

static void mstr(char *str,const char *src,uint l1,uint l2)
{
  l1 = l1<l2 ? l1 : l2;
  memcpy(str,src,l1);
  str[l1]='\0';
}

static int dstr(MY_XML_PARSER *st,const char *attr, uint len)
{
  char str[1024];
  
  mstr(str,attr,len,sizeof(str)-1);
  printf("VALUE '%s'\n",str);
  return MY_XML_OK;
}

static int bstr(MY_XML_PARSER *st,const char *attr, uint len)
{
  char str[1024];
  
  mstr(str,attr,len,sizeof(str)-1);
  printf("ENTER %s\n",str);
  return MY_XML_OK;
}


static int estr(MY_XML_PARSER *st,const char *attr, uint len)
{
  char str[1024];
  
  mstr(str,attr,len,sizeof(str)-1);
  printf("LEAVE %s\n",str);
  return MY_XML_OK;
}

static void usage(const char *prog)
{
  printf("Usage:\n");
  printf("%s xmlfile\n",prog);
}

int main(int ac, char **av)
{
  char str[1024*64]="";
  const char *fn;
  int  f;
  uint len;
  MY_XML_PARSER p;
  
  if (ac<2)
  {
    usage(av[0]);
    return 0;
  }
  
  fn=av[1]?av[1]:"test.xml";
  if ((f=open(fn,O_RDONLY))<0)
  {
    fprintf(stderr,"Err '%s'\n",fn);
    return 1;
  }
  
  len=read(f,str,sizeof(str)-1);
  str[len]='\0';
  
  my_xml_parser_create(&p);
  
  my_xml_set_enter_handler(&p,bstr);
  my_xml_set_value_handler(&p,dstr);
  my_xml_set_leave_handler(&p,estr);
  
  if (MY_XML_OK!=(f=my_xml_parse(&p,str,len)))
  {
    printf("ERROR at line %d pos %d '%s'\n",
      my_xml_error_lineno(&p)+1,
      my_xml_error_pos(&p),
      my_xml_error_string(&p));
  }
  
  my_xml_parser_free(&p);
  
  return 0;
}
