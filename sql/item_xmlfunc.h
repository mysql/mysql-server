#ifndef ITEM_XMLFUNC_INCLUDED
#define ITEM_XMLFUNC_INCLUDED

/* Copyright (c) 2000-2007 MySQL AB, 2009 Sun Microsystems, Inc.
   Use is subject to license terms.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */


/* This file defines all XML functions */


#ifdef __GNUC__
#pragma interface			/* gcc class implementation */
#endif


class Item_xml_str_func: public Item_str_func
{
protected:
  String tmp_value, pxml;
  Item *nodeset_func;
public:
  Item_xml_str_func(Item *a, Item *b): 
    Item_str_func(a,b) 
  {
    maybe_null= TRUE;
  }
  Item_xml_str_func(Item *a, Item *b, Item *c): 
    Item_str_func(a,b,c) 
  {
    maybe_null= TRUE;
  }
  void fix_length_and_dec();
  String *parse_xml(String *raw_xml, String *parsed_xml_buf);
  bool check_vcol_func_processor(uchar *int_arg) 
  {
    return trace_unsupported_by_check_vcol_func_processor(func_name());
  }
};


class Item_func_xml_extractvalue: public Item_xml_str_func
{
public:
  Item_func_xml_extractvalue(Item *a,Item *b) :Item_xml_str_func(a,b) {}
  const char *func_name() const { return "extractvalue"; }
  String *val_str(String *);
};


class Item_func_xml_update: public Item_xml_str_func
{
  String tmp_value2, tmp_value3;
public:
  Item_func_xml_update(Item *a,Item *b,Item *c) :Item_xml_str_func(a,b,c) {}
  const char *func_name() const { return "updatexml"; }
  String *val_str(String *);
};

#endif /* ITEM_XMLFUNC_INCLUDED */
