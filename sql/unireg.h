#ifndef UNIREG_INCLUDED
#define UNIREG_INCLUDED

/* Copyright (c) 2000, 2014, Oracle and/or its affiliates. All rights reserved.

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
