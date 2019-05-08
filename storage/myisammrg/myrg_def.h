/* Copyright (c) 2000, 2018, Oracle and/or its affiliates. All rights reserved.

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
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/* This file is included by all myisam-merge files */

/**
  @file storage/myisammrg/myrg_def.h
*/

#include "myisammrg.h"

extern LIST *myrg_open_list;

extern mysql_mutex_t THR_LOCK_open;

int _myrg_init_queue(MYRG_INFO *info, int inx,
                     enum ha_rkey_function search_flag);
int _myrg_mi_read_record(MI_INFO *info, uchar *buf);

C_MODE_START
void myrg_print_wrong_table(const char *table_name);

/* Always defined */
extern PSI_memory_key rg_key_memory_MYRG_INFO;
extern PSI_memory_key rg_key_memory_children;

#ifdef HAVE_PSI_INTERFACE
extern PSI_mutex_key rg_key_mutex_MYRG_INFO_mutex;

extern PSI_file_key rg_key_file_MRG;
void init_myisammrg_psi_keys();
#endif /* HAVE_PSI_INTERFACE */

C_MODE_END
