/* Copyright (c) 2000, 2013, Oracle and/or its affiliates. All rights reserved.

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

/* This file is included by all myisam-merge files */

#include "../myisam/myisamdef.h"
#include "myisammrg.h"

extern LIST *myrg_open_list;

extern mysql_mutex_t THR_LOCK_open;

int _myrg_init_queue(MYRG_INFO *info,int inx,enum ha_rkey_function search_flag);
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

