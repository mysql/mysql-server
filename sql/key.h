/* Copyright (c) 2006, 2010, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#ifndef KEY_INCLUDED
#define KEY_INCLUDED

#include "my_global.h"                          /* uchar */

class Field;
class String;
struct TABLE;
typedef struct st_bitmap MY_BITMAP;
typedef struct st_key KEY;
typedef struct st_key_part_info KEY_PART_INFO;

int find_ref_key(KEY *key, uint key_count, uchar *record, Field *field,
                 uint *key_length, uint *keypart);
void key_copy(uchar *to_key, uchar *from_record, KEY *key_info, uint key_length,
              bool with_zerofill= FALSE);
void key_restore(uchar *to_record, uchar *from_key, KEY *key_info,
                 uint key_length);
bool key_cmp_if_same(TABLE *form,const uchar *key,uint index,uint key_length);
void key_unpack(String *to,TABLE *form,uint index);
void field_unpack(String *to, Field *field, const uchar *rec, uint max_length,
                  bool prefix_key);
bool is_key_used(TABLE *table, uint idx, const MY_BITMAP *fields);
int key_cmp(KEY_PART_INFO *key_part, const uchar *key, uint key_length);
ulong key_hashnr(KEY *key_info, uint used_key_parts, const uchar *key);
bool key_buf_cmp(KEY *key_info, uint used_key_parts,
                 const uchar *key1, const uchar *key2);
extern "C" int key_rec_cmp(void *key_info, uchar *a, uchar *b);
int key_tuple_cmp(KEY_PART_INFO *part, uchar *key1, uchar *key2, uint tuple_length);

#endif /* KEY_INCLUDED */
