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
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA  */


#ifndef _my_xml_h
#define _my_xml_h

#ifdef	__cplusplus
extern "C" {
#endif


#define MY_XML_OK	0
#define MY_XML_ERROR	1

typedef struct xml_stack_st
{
  char errstr[128];
  char attr[128];
  char *attrend;
  const char *beg;
  const char *cur;
  const char *end;
  void *user_data;
  int  (*enter)(struct xml_stack_st *st,const char *val, uint len);
  int  (*value)(struct xml_stack_st *st,const char *val, uint len);
  int  (*leave_xml)(struct xml_stack_st *st,const char *val, uint len);
} MY_XML_PARSER;

void my_xml_parser_create(MY_XML_PARSER *st);
void my_xml_parser_free(MY_XML_PARSER *st);
int  my_xml_parse(MY_XML_PARSER *st,const char *str, uint len);

void my_xml_set_value_handler(MY_XML_PARSER *st, int (*)(MY_XML_PARSER *,
							 const char *,
							 uint len));
void my_xml_set_enter_handler(MY_XML_PARSER *st, int (*)(MY_XML_PARSER *,
							 const char *,
							 uint len));
void my_xml_set_leave_handler(MY_XML_PARSER *st, int (*)(MY_XML_PARSER *,
							 const char *,
							 uint len));
void my_xml_set_user_data(MY_XML_PARSER *st, void *);

uint my_xml_error_pos(MY_XML_PARSER *st);
uint my_xml_error_lineno(MY_XML_PARSER *st);

const char *my_xml_error_string(MY_XML_PARSER *st);

#ifdef	__cplusplus
}
#endif

#endif /* _my_xml_h */
