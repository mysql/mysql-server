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


/* classes to use when handling where clause */

#ifndef _opt_ft_h
#define _opt_ft_h

#ifdef __GNUC__
#pragma interface			/* gcc class implementation */
#endif

class FT_SELECT: public QUICK_SELECT {
public:
  TABLE_REF *ref;

  FT_SELECT(TABLE *table, TABLE_REF *tref) :
      QUICK_SELECT (table,tref->key,1), ref(tref) {}

  int init()
  {
#if 0
    if (cp_buffer_from_ref(ref)) // as ft-key doesn't use store_key's
      return -1;
#endif
    return error=file->ft_init(ref->key,
                                  ref->key_buff,
                                  ref->key_length);
  }
  int get_next() { return error=file->ft_read(record); }
};

QUICK_SELECT *get_ft_or_quick_select_for_ref(TABLE *table, JOIN_TAB *tab);

#endif
