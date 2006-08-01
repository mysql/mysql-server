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
#include <m_ctype.h>
#include <my_xml.h>
#ifndef SCO
#include <m_string.h>
#endif


/*

  This files implements routines which parse XML based
  character set and collation description files.
  
  Unicode collations are encoded according to
  
    Unicode Technical Standard #35
    Locale Data Markup Language (LDML)
    http://www.unicode.org/reports/tr35/
  
  and converted into ICU string according to
  
    Collation Customization
    http://oss.software.ibm.com/icu/userguide/Collate_Customization.html
  
*/

static char *mstr(char *str,const char *src,uint l1,uint l2)
{
  l1= l1<l2 ? l1 : l2;
  memcpy(str,src,l1);
  str[l1]='\0';
  return str;
}

struct my_cs_file_section_st
{
  int        state;
  const char *str;
};

#define _CS_MISC	1
#define _CS_ID		2
#define _CS_CSNAME	3
#define _CS_FAMILY	4
#define _CS_ORDER	5
#define _CS_COLNAME	6
#define _CS_FLAG	7
#define _CS_CHARSET	8
#define _CS_COLLATION	9
#define _CS_UPPERMAP	10
#define _CS_LOWERMAP	11
#define _CS_UNIMAP	12
#define _CS_COLLMAP	13
#define _CS_CTYPEMAP	14
#define _CS_PRIMARY_ID	15
#define _CS_BINARY_ID	16
#define _CS_CSDESCRIPT	17
#define _CS_RESET	18
#define	_CS_DIFF1	19
#define	_CS_DIFF2	20
#define	_CS_DIFF3	21


static struct my_cs_file_section_st sec[] =
{
  {_CS_MISC,		"xml"},
  {_CS_MISC,		"xml/version"},
  {_CS_MISC,		"xml/encoding"},
  {_CS_MISC,		"charsets"},
  {_CS_MISC,		"charsets/max-id"},
  {_CS_CHARSET,		"charsets/charset"},
  {_CS_PRIMARY_ID,	"charsets/charset/primary-id"},
  {_CS_BINARY_ID,	"charsets/charset/binary-id"},
  {_CS_CSNAME,		"charsets/charset/name"},
  {_CS_FAMILY,		"charsets/charset/family"},
  {_CS_CSDESCRIPT,	"charsets/charset/description"},
  {_CS_MISC,		"charsets/charset/alias"},
  {_CS_MISC,		"charsets/charset/ctype"},
  {_CS_CTYPEMAP,	"charsets/charset/ctype/map"},
  {_CS_MISC,		"charsets/charset/upper"},
  {_CS_UPPERMAP,	"charsets/charset/upper/map"},
  {_CS_MISC,		"charsets/charset/lower"},
  {_CS_LOWERMAP,	"charsets/charset/lower/map"},
  {_CS_MISC,		"charsets/charset/unicode"},
  {_CS_UNIMAP,		"charsets/charset/unicode/map"},
  {_CS_COLLATION,	"charsets/charset/collation"},
  {_CS_COLNAME,		"charsets/charset/collation/name"},
  {_CS_ID,		"charsets/charset/collation/id"},
  {_CS_ORDER,		"charsets/charset/collation/order"},
  {_CS_FLAG,		"charsets/charset/collation/flag"},
  {_CS_COLLMAP,		"charsets/charset/collation/map"},
  {_CS_RESET,		"charsets/charset/collation/rules/reset"},
  {_CS_DIFF1,		"charsets/charset/collation/rules/p"},
  {_CS_DIFF2,		"charsets/charset/collation/rules/s"},
  {_CS_DIFF3,		"charsets/charset/collation/rules/t"},
  {0,	NULL}
};

static struct my_cs_file_section_st * cs_file_sec(const char *attr, uint len)
{
  struct my_cs_file_section_st *s;
  for (s=sec; s->str; s++)
  {
    if (!strncmp(attr,s->str,len))
      return s;
  }
  return NULL;
}

#define MY_CS_CSDESCR_SIZE	64
#define MY_CS_TAILORING_SIZE	128

typedef struct my_cs_file_info
{
  char   csname[MY_CS_NAME_SIZE];
  char   name[MY_CS_NAME_SIZE];
  uchar  ctype[MY_CS_CTYPE_TABLE_SIZE];
  uchar  to_lower[MY_CS_TO_LOWER_TABLE_SIZE];
  uchar  to_upper[MY_CS_TO_UPPER_TABLE_SIZE];
  uchar  sort_order[MY_CS_SORT_ORDER_TABLE_SIZE];
  uint16 tab_to_uni[MY_CS_TO_UNI_TABLE_SIZE];
  char   comment[MY_CS_CSDESCR_SIZE];
  char   tailoring[MY_CS_TAILORING_SIZE];
  size_t tailoring_length;
  CHARSET_INFO cs;
  int (*add_collation)(CHARSET_INFO *cs);
} MY_CHARSET_LOADER;



static int fill_uchar(uchar *a,uint size,const char *str, uint len)
{
  uint i= 0;
  const char *s, *b, *e=str+len;
  
  for (s=str ; s < e ; i++)
  { 
    for ( ; (s < e) && strchr(" \t\r\n",s[0]); s++) ;
    b=s;
    for ( ; (s < e) && !strchr(" \t\r\n",s[0]); s++) ;
    if (s == b || i > size)
      break;
    a[i]= (uchar) strtoul(b,NULL,16);
  }
  return 0;
}

static int fill_uint16(uint16 *a,uint size,const char *str, uint len)
{
  uint i= 0;
  
  const char *s, *b, *e=str+len;
  for (s=str ; s < e ; i++)
  { 
    for ( ; (s < e) && strchr(" \t\r\n",s[0]); s++) ;
    b=s;
    for ( ; (s < e) && !strchr(" \t\r\n",s[0]); s++) ;
    if (s == b || i > size)
      break;
    a[i]= (uint16) strtol(b,NULL,16);
  }
  return 0;
}


static int cs_enter(MY_XML_PARSER *st,const char *attr, uint len)
{
  struct my_cs_file_info *i= (struct my_cs_file_info *)st->user_data;
  struct my_cs_file_section_st *s= cs_file_sec(attr,len);
  
  if ( s && (s->state == _CS_CHARSET))
    bzero(&i->cs,sizeof(i->cs));
  
  if (s && (s->state == _CS_COLLATION))
    i->tailoring_length= 0;

  return MY_XML_OK;
}


static int cs_leave(MY_XML_PARSER *st,const char *attr, uint len)
{
  struct my_cs_file_info *i= (struct my_cs_file_info *)st->user_data;
  struct my_cs_file_section_st *s= cs_file_sec(attr,len);
  int    state= s ? s->state : 0;
  int    rc;
  
  switch(state){
  case _CS_COLLATION:
    rc= i->add_collation ? i->add_collation(&i->cs) : MY_XML_OK;
    break;
  default:
    rc=MY_XML_OK;
  }
  return rc;
}


static int cs_value(MY_XML_PARSER *st,const char *attr, uint len)
{
  struct my_cs_file_info *i= (struct my_cs_file_info *)st->user_data;
  struct my_cs_file_section_st *s;
  int    state= (int)((s=cs_file_sec(st->attr, (int) strlen(st->attr))) ? s->state : 0);
  
  switch (state) {
  case _CS_ID:
    i->cs.number= strtol(attr,(char**)NULL,10);
    break;
  case _CS_BINARY_ID:
    i->cs.binary_number= strtol(attr,(char**)NULL,10);
    break;
  case _CS_PRIMARY_ID:
    i->cs.primary_number= strtol(attr,(char**)NULL,10);
    break;
  case _CS_COLNAME:
    i->cs.name=mstr(i->name,attr,len,MY_CS_NAME_SIZE-1);
    break;
  case _CS_CSNAME:
    i->cs.csname=mstr(i->csname,attr,len,MY_CS_NAME_SIZE-1);
    break;
  case _CS_CSDESCRIPT:
    i->cs.comment=mstr(i->comment,attr,len,MY_CS_CSDESCR_SIZE-1);
    break;
  case _CS_FLAG:
    if (!strncmp("primary",attr,len))
      i->cs.state|= MY_CS_PRIMARY;
    else if (!strncmp("binary",attr,len))
      i->cs.state|= MY_CS_BINSORT;
    else if (!strncmp("compiled",attr,len))
      i->cs.state|= MY_CS_COMPILED;
    break;
  case _CS_UPPERMAP:
    fill_uchar(i->to_upper,MY_CS_TO_UPPER_TABLE_SIZE,attr,len);
    i->cs.to_upper=i->to_upper;
    break;
  case _CS_LOWERMAP:
    fill_uchar(i->to_lower,MY_CS_TO_LOWER_TABLE_SIZE,attr,len);
    i->cs.to_lower=i->to_lower;
    break;
  case _CS_UNIMAP:
    fill_uint16(i->tab_to_uni,MY_CS_TO_UNI_TABLE_SIZE,attr,len);
    i->cs.tab_to_uni=i->tab_to_uni;
    break;
  case _CS_COLLMAP:
    fill_uchar(i->sort_order,MY_CS_SORT_ORDER_TABLE_SIZE,attr,len);
    i->cs.sort_order=i->sort_order;
    break;
  case _CS_CTYPEMAP:
    fill_uchar(i->ctype,MY_CS_CTYPE_TABLE_SIZE,attr,len);
    i->cs.ctype=i->ctype;
    break;
  case _CS_RESET:
  case _CS_DIFF1:
  case _CS_DIFF2:
  case _CS_DIFF3:
    {
      /*
        Convert collation description from
        Locale Data Markup Language (LDML)
        into ICU Collation Customization expression.
      */
      char arg[16];
      const char *cmd[]= {"&","<","<<","<<<"};
      i->cs.tailoring= i->tailoring;
      mstr(arg,attr,len,sizeof(arg)-1);
      if (i->tailoring_length + 20 < sizeof(i->tailoring))
      {
        char *dst= i->tailoring_length + i->tailoring;
        i->tailoring_length+= sprintf(dst," %s %s",cmd[state-_CS_RESET],arg);
      }
    }
  }
  return MY_XML_OK;
}


my_bool my_parse_charset_xml(const char *buf, uint len, 
				    int (*add_collation)(CHARSET_INFO *cs))
{
  MY_XML_PARSER p;
  struct my_cs_file_info i;
  my_bool rc;
  
  my_xml_parser_create(&p);
  my_xml_set_enter_handler(&p,cs_enter);
  my_xml_set_value_handler(&p,cs_value);
  my_xml_set_leave_handler(&p,cs_leave);
  i.add_collation= add_collation;
  my_xml_set_user_data(&p,(void*)&i);
  rc= (my_xml_parse(&p,buf,len) == MY_XML_OK) ? FALSE : TRUE;
  my_xml_parser_free(&p);
  return rc;
}
