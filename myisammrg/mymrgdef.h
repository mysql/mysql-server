/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB
   
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

/* This file is included by all myisam-merge files */

#ifndef N_MAXKEY
#include "../myisam/myisamdef.h"
#endif

#include "myisammrg.h"

extern LIST *myrg_open_list;

#ifdef THREAD
extern pthread_mutex_t THR_LOCK_open;
#endif

int _myrg_init_queue(MYRG_INFO *info,int inx,enum ha_rkey_function search_flag);
int _myrg_finish_scan(MYRG_INFO *info, int inx, enum ha_rkey_function type);
