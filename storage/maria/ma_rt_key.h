/* Copyright (C) 2006 MySQL AB & Ramil Kalimullin & MySQL Finland AB
   & TCX DataKonsult AB

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

/* Written by Ramil Kalimullin, who has a shared copyright to this code */

#ifndef _rt_key_h
#define _rt_key_h

#ifdef HAVE_RTREE_KEYS

int maria_rtree_add_key(MARIA_HA *info, MARIA_KEYDEF *keyinfo, uchar *key,
                 uint key_length, uchar *page_buf, my_off_t *new_page);
int maria_rtree_delete_key(MARIA_HA *info, uchar *page, uchar *key,
                    uint key_length, uint nod_flag);
int maria_rtree_set_key_mbr(MARIA_HA *info, MARIA_KEYDEF *keyinfo, uchar *key,
                    uint key_length, my_off_t child_page);

#endif /*HAVE_RTREE_KEYS*/
#endif /* _rt_key_h */
