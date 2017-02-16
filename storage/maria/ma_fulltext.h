/* Copyright (C) 2006 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/* Written by Sergei A. Golubchik, who has a shared copyright to this code */

/* some definitions for full-text indices */

#include "maria_def.h"
#include "ft_global.h"

int  _ma_ft_cmp(MARIA_HA *, uint, const uchar *, const uchar *);
int  _ma_ft_add(MARIA_HA *, uint, uchar *, const uchar *, my_off_t);
int  _ma_ft_del(MARIA_HA *, uint, uchar *, const uchar *, my_off_t);

my_bool _ma_ft_convert_to_ft2(MARIA_HA *, MARIA_KEY *);
