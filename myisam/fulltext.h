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

/* Written by Sergei A. Golubchik, who has a shared copyright to this code */

/* some definitions for full-text indices */

#include "myisamdef.h"
#include "ft_global.h"

/* shoudn't be def'ed when linking with mysql */
#undef EVAL_RUN

#define HA_FT_WTYPE  HA_KEYTYPE_FLOAT
#define HA_FT_WLEN   4
#ifdef EVAL_RUN
#define FT_SEGS      3
#else /* EVAL_RUN */
#define FT_SEGS      2
#endif /* EVAL_RUN */

extern const MI_KEYSEG ft_keysegs[FT_SEGS];

int  _mi_ft_cmp(MI_INFO *, uint, const byte *, const byte *);
int  _mi_ft_add(MI_INFO *, uint, byte *, const byte *, my_off_t);
int  _mi_ft_del(MI_INFO *, uint, byte *, const byte *, my_off_t);
