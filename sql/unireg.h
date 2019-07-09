#ifndef UNIREG_INCLUDED
#define UNIREG_INCLUDED

/* Copyright (c) 2000, 2014, Oracle and/or its affiliates. All rights reserved.

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
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */


#include "my_global.h"                          /* ulonglong */

/*  Extra functions used by unireg library */

typedef struct st_ha_create_information HA_CREATE_INFO;
typedef struct st_key KEY;
class THD;
class Create_field;
class handler;
template <class T> class List;

/* Include prototypes for unireg */

bool mysql_create_frm(THD *thd, const char *file_name,
                      const char *db, const char *table,
		      HA_CREATE_INFO *create_info,
		      List<Create_field> &create_field,
		      uint key_count,KEY *key_info,handler *db_type);
int rea_create_table(THD *thd, const char *path,
                     const char *db, const char *table_name,
                     HA_CREATE_INFO *create_info,
  		     List<Create_field> &create_field,
                     uint key_count,KEY *key_info,
                     handler *file,
                     bool no_ha_table);
#endif
