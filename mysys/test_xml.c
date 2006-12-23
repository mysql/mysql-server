/* Copyright (C) 2000 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

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
