/* Copyright (C) 2006 MySQL AB & Ramil Kalimullin & MySQL Finland AB
   & TCX DataKonsult AB

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

#ifndef _rt_index_h
#define _rt_index_h

#ifdef HAVE_RTREE_KEYS

#define rt_PAGE_FIRST_KEY(share, page, nod_flag) (page + share->keypage_header + nod_flag)
#define rt_PAGE_NEXT_KEY(share, key, key_length, nod_flag) (key + key_length +\
              (nod_flag ? nod_flag : share->base.rec_reflength))
#define rt_PAGE_END(page) ((page)->buff + (page)->size)

#define rt_PAGE_MIN_SIZE(block_length) ((uint)(block_length - KEYPAGE_CHECKSUM_SIZE) / 3)

my_bool maria_rtree_insert(MARIA_HA *info, MARIA_KEY *key);
my_bool maria_rtree_delete(MARIA_HA *info, MARIA_KEY *key);
int maria_rtree_insert_level(MARIA_HA *info, MARIA_KEY *key,
                             int ins_level, my_off_t *root);
my_bool maria_rtree_real_delete(MARIA_HA *info, MARIA_KEY *key,
                                my_off_t *root);
int maria_rtree_find_first(MARIA_HA *info, MARIA_KEY *key, uint search_flag);
int maria_rtree_find_next(MARIA_HA *info, uint keynr, uint32 search_flag);

int maria_rtree_get_first(MARIA_HA *info, uint keynr, uint key_length);
int maria_rtree_get_next(MARIA_HA *info, uint keynr, uint key_length);

ha_rows maria_rtree_estimate(MARIA_HA *info, MARIA_KEY *key, uint32 flag);

int maria_rtree_split_page(const MARIA_KEY *key, MARIA_PAGE *page,
                           my_off_t *new_page_offs);
#endif /*HAVE_RTREE_KEYS*/
#endif /* _rt_index_h */
