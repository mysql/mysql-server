/*
   Copyright (c) 2000, 2010, Oracle and/or its affiliates

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

/* Written by Sergei A. Golubchik, who has a shared copyright to this code */

/* some definitions for full-text indices */

#include "myisamdef.h"
#include "ft_global.h"

int  _mi_ft_cmp(MI_INFO *, uint, const uchar *, const uchar *);
int  _mi_ft_add(MI_INFO *, uint, uchar *, const uchar *, my_off_t);
int  _mi_ft_del(MI_INFO *, uint, uchar *, const uchar *, my_off_t);

uint _mi_ft_convert_to_ft2(MI_INFO *, uint, uchar *);
