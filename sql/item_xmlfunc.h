#ifndef ITEM_XMLFUNC_INCLUDED
#define ITEM_XMLFUNC_INCLUDED

/* Copyright (c) 2000, 2015, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#include "item_strfunc.h"     // Item_str_func

/* This file defines all XML functions */

class Item_xml_str_func: public Item_str_func
{
protected:
  String tmp_value, pxml;
  Item *nodeset_func;
public:
  Item_xml_str_func(const POS &pos, Item *a, Item *b): 
    Item_str_func(pos, a,b) 
  {
    maybe_null= TRUE;
  }
  Item_xml_str_func(const POS &pos, Item *a, Item *b, Item *c): 
    Item_str_func(pos, a,b,c) 
  {
    maybe_null= TRUE;
  }
  void fix_length_and_dec();
  String *parse_xml(String *raw_xml, String *parsed_xml_buf);
  bool check_gcol_func_processor(uchar *int_arg) { return false; }

protected:
  /** 
    Parse the specified XPATH expression and initialize @c nodeset_func.

    @note This is normally called in resolve phase since we only support
          constant XPATH expressions, but it may be called upon execution when
          const value is not yet known at resolve time.

    @param xpath_expr XPATH expression to be parsed
   */
  void parse_xpath(Item* xpath_expr);
};


class Item_func_xml_extractvalue: public Item_xml_str_func
{
public:
  Item_func_xml_extractvalue(const POS &pos, Item *a, Item *b)
    :Item_xml_str_func(pos, a, b)
  {}
  const char *func_name() const { return "extractvalue"; }
  String *val_str(String *);
};


class Item_func_xml_update: public Item_xml_str_func
{
  String tmp_value2, tmp_value3;
public:
  Item_func_xml_update(const POS &pos, Item *a, Item *b, Item *c)
    :Item_xml_str_func(pos, a, b, c)
  {}
  const char *func_name() const { return "updatexml"; }
  String *val_str(String *);
  bool check_gcol_func_processor(uchar *int_arg)
  { return true; }
};

#endif /* ITEM_XMLFUNC_INCLUDED */
