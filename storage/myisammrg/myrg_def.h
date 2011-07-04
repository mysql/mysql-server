/* Copyright (c) 2000, 2011, Oracle and/or its affiliates. All rights reserved.

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

#ifndef N_MAXKEY
#include "../myisam/myisamdef.h"
#endif

#include "myisammrg.h"

extern LIST *myrg_open_list;

extern mysql_mutex_t THR_LOCK_open;

int _myrg_init_queue(MYRG_INFO *info,int inx,enum ha_rkey_function search_flag);
int _myrg_mi_read_record(MI_INFO *info, uchar *buf);
#ifdef __cplusplus
extern "C" 
#endif
void myrg_print_wrong_table(const char *table_name);

#ifdef HAVE_PSI_INTERFACE
extern PSI_mutex_key rg_key_mutex_MYRG_INFO_mutex;

extern PSI_file_key rg_key_file_MRG;

C_MODE_START
void init_myisammrg_psi_keys();
C_MODE_END
#endif /* HAVE_PSI_INTERFACE */

