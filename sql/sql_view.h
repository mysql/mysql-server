/* -*- C++ -*- */
/* Copyright (C) 2004 MySQL AB

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
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

int mysql_create_view(THD *thd,
		      enum_view_create_mode mode);

my_bool mysql_make_view(File_parser *parser, TABLE_LIST *table);

int mysql_drop_view(THD *thd, TABLE_LIST *view, enum_drop_mode drop_mode);

bool check_key_in_view(THD *thd, TABLE_LIST * view);

int insert_view_fields(List<Item> *list, TABLE_LIST *view);

frm_type_enum mysql_frm_type(char *path);

extern TYPELIB sql_updatable_view_key_typelib;

#define VIEW_ANY_ACL (SELECT_ACL | UPDATE_ACL | INSERT_ACL | DELETE_ACL)

