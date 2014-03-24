/* Copyright (c) 2002, 2004-2006 MySQL AB
   Use is subject to license terms
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA */

#ifndef _rt_mbr_h
#define _rt_mbr_h

#ifdef HAVE_RTREE_KEYS

int rtree_key_cmp(HA_KEYSEG *keyseg, uchar *a, uchar *b, uint key_length,
                  uint nextflag);
int rtree_combine_rect(HA_KEYSEG *keyseg,uchar *, uchar *, uchar*, 
                       uint key_length);
double rtree_rect_volume(HA_KEYSEG *keyseg, uchar*, uint key_length);
int rtree_d_mbr(HA_KEYSEG *keyseg, uchar *a, uint key_length, double *res);
double rtree_overlapping_area(HA_KEYSEG *keyseg, uchar *a, uchar *b, 
                              uint key_length);
double rtree_area_increase(HA_KEYSEG *keyseg, uchar *a, uchar *b, 
                           uint key_length, double *ab_area);
double rtree_perimeter_increase(HA_KEYSEG *keyseg, uchar* a, uchar* b, 
				uint key_length, double *ab_perim);
int rtree_page_mbr(MI_INFO *info, HA_KEYSEG *keyseg, uchar *page_buf, 
                   uchar* c, uint key_length);
#endif /*HAVE_RTREE_KEYS*/
#endif /* _rt_mbr_h */
