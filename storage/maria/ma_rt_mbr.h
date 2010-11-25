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

#ifndef _rt_mbr_h
#define _rt_mbr_h

#ifdef HAVE_RTREE_KEYS

int maria_rtree_key_cmp(HA_KEYSEG *keyseg, const uchar *a, const uchar *b,
                        uint key_length, uint32 nextflag);
int maria_rtree_combine_rect(const HA_KEYSEG *keyseg,
                             const uchar *, const uchar *, uchar*,
                             uint key_length);
double maria_rtree_rect_volume(HA_KEYSEG *keyseg, uchar*, uint key_length);
int maria_rtree_d_mbr(const HA_KEYSEG *keyseg, const uchar *a,
                      uint key_length, double *res);
double maria_rtree_overlapping_area(HA_KEYSEG *keyseg, uchar *a, uchar *b,
                                    uint key_length);
double maria_rtree_area_increase(const HA_KEYSEG *keyseg, const uchar *a,
                                 const uchar *b,
                                 uint key_length, double *ab_area);
double maria_rtree_perimeter_increase(HA_KEYSEG *keyseg, uchar* a, uchar* b,
                                      uint key_length, double *ab_perim);
int maria_rtree_page_mbr(const HA_KEYSEG *keyseg, MARIA_PAGE *page,
                         uchar *key, uint key_length);
#endif /*HAVE_RTREE_KEYS*/
#endif /* _rt_mbr_h */
