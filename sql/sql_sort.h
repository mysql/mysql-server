/* Copyright (C) 2000 MySQL AB

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

/* Defines used by filesort and uniques */

#define MERGEBUFF		7
#define MERGEBUFF2		15

typedef struct st_buffpek {		/* Struktur om sorteringsbuffrarna */
  my_off_t file_pos;			/* Where we are in the sort file */
  uchar *base,*key;			/* key pointers */
  ha_rows count;			/* Number of rows in table */
  ulong mem_count;			/* numbers of keys in memory */
  ulong max_keys;			/* Max keys in buffert */
} BUFFPEK;


typedef struct st_sort_param {
  uint sort_length;			/* Length of sort columns */
  uint keys;				/* Max keys / buffert */
  uint ref_length;			/* Length of record ref. */
  ha_rows max_rows,examined_rows;
  TABLE *sort_form;			/* For quicker make_sortkey */
  SORT_FIELD *local_sortorder;
  SORT_FIELD *end;
  uchar *unique_buff;
#ifdef USE_STRCOLL
  char* tmp_buffer;
#endif
} SORTPARAM;


int merge_many_buff(SORTPARAM *param, uchar *sort_buffer,
		    BUFFPEK *buffpek,
		    uint *maxbuffer, IO_CACHE *t_file);
uint read_to_buffer(IO_CACHE *fromfile,BUFFPEK *buffpek,
		    uint sort_length);
int merge_buffers(SORTPARAM *param,IO_CACHE *from_file,
		  IO_CACHE *to_file, uchar *sort_buffer,
		  BUFFPEK *lastbuff,BUFFPEK *Fb,
		  BUFFPEK *Tb,int flag);
