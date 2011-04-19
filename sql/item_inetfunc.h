#ifndef ITEM_INETFUNC_INCLUDED
#define ITEM_INETFUNC_INCLUDED

/* Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.

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


#include "item.h"

/*************************************************************************
  Item_func_inet_aton implements INET_ATON() SQL-function.
*************************************************************************/

class Item_func_inet_aton : public Item_int_func
{
public:
  inline Item_func_inet_aton(Item *arg)
    : Item_int_func(arg)
  {}

public:
  virtual longlong val_int();

  virtual const char *func_name() const
  { return "inet_aton"; }

  virtual void fix_length_and_dec()
  {
    decimals= 0;
    max_length= 21;
    maybe_null= 1;
    unsigned_flag= 1;
  }
};


/*************************************************************************
  Item_func_inet_ntoa implements INET_NTOA() SQL-function.
*************************************************************************/

class Item_func_inet_ntoa : public Item_str_func
{
public:
  inline Item_func_inet_ntoa(Item *arg)
    : Item_str_func(arg)
  { }

public:
  virtual String* val_str(String* str);

  virtual const char *func_name() const
  { return "inet_ntoa"; }

  virtual void fix_length_and_dec()
  {
    decimals= 0;
    fix_length_and_charset(3 * 8 + 7, default_charset());
    maybe_null= 1;
  }
};

#endif // ITEM_INETFUNC_INCLUDED
