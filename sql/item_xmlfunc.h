/* Copyright (C) 2000-2005 MySQL AB

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
  Item_xml_str_func(Item *a, Item *b): Item_str_func(a,b) {}
  Item_xml_str_func(Item *a, Item *b, Item *c): Item_str_func(a,b,c) {}
  void fix_length_and_dec();
  String *parse_xml(String *raw_xml, String *parsed_xml_buf);
};


class Item_func_xml_extractvalue: public Item_xml_str_func
{
public:
  Item_func_xml_extractvalue(Item *a,Item *b) :Item_xml_str_func(a,b) {}
  const char *func_name() const { return "extractvalue"; }
  String *val_str(String *);
  bool check_partition_func_processor(byte *int_arg) { return FALSE; }
};


class Item_func_xml_update: public Item_xml_str_func
{
  String tmp_value2, tmp_value3;
public:
  Item_func_xml_update(Item *a,Item *b,Item *c) :Item_xml_str_func(a,b,c) {}
  const char *func_name() const { return "updatexml"; }
  String *val_str(String *);
};

