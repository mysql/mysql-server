/* Copyright (c) 2000, 2011, Oracle and/or its affiliates. All rights reserved.

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

/* This file is included in all heap-files */

#include <my_base.h>			/* This includes global */
C_MODE_START
#include <my_pthread.h>
#include "heap.h"			/* Structs & some defines */
#include "my_tree.h"

/*
  When allocating keys /rows in the internal block structure, do it
  within the following boundaries.

  The challenge is to find the balance between allocate as few blocks
  as possible and keep memory consumption down.
*/

#define HP_MIN_RECORDS_IN_BLOCK 16
#define HP_MAX_RECORDS_IN_BLOCK 8192

	/* Some extern variables */

extern LIST *heap_open_list,*heap_share_list;

#define test_active(info) \
if (!(info->update & HA_STATE_AKTIV))\
{ my_errno=HA_ERR_NO_ACTIVE_RECORD; DBUG_RETURN(-1); }
#define hp_find_hash(A,B) ((HASH_INFO*) hp_find_block((A),(B)))

	/* Find pos for record and update it in info->current_ptr */
#define hp_find_record(info,pos) (info)->current_ptr= hp_find_block(&(info)->s->block,pos)

typedef struct st_hp_hash_info
{
  struct st_hp_hash_info *next_key;
  uchar *ptr_to_rec;
  ulong hash_of_key;
} HASH_INFO;

typedef struct {
  HA_KEYSEG *keyseg;
  uint key_length;
  uint search_flag;
} heap_rb_param;
      
	/* Prototypes for intern functions */

extern HP_SHARE *hp_find_named_heap(const char *name);
extern int hp_rectest(HP_INFO *info,const uchar *old);
extern uchar *hp_find_block(HP_BLOCK *info,ulong pos);
extern int hp_get_new_block(HP_BLOCK *info, size_t* alloc_length);
extern void hp_free(HP_SHARE *info);
extern uchar *hp_free_level(HP_BLOCK *block,uint level,HP_PTRS *pos,
			   uchar *last_pos);
extern int hp_write_key(HP_INFO *info, HP_KEYDEF *keyinfo,
			const uchar *record, uchar *recpos);
extern int hp_rb_write_key(HP_INFO *info, HP_KEYDEF *keyinfo, 
			   const uchar *record, uchar *recpos);
extern int hp_rb_delete_key(HP_INFO *info,HP_KEYDEF *keyinfo,
			    const uchar *record,uchar *recpos,int flag);
extern int hp_delete_key(HP_INFO *info,HP_KEYDEF *keyinfo,
			 const uchar *record,uchar *recpos,int flag);
extern HASH_INFO *_heap_find_hash(HP_BLOCK *block,ulong pos);
extern uchar *hp_search(HP_INFO *info,HP_KEYDEF *keyinfo,const uchar *key,
		       uint nextflag);
extern uchar *hp_search_next(HP_INFO *info, HP_KEYDEF *keyinfo,
			    const uchar *key, HASH_INFO *pos);
extern ulong hp_hashnr(HP_KEYDEF *keyinfo,const uchar *key);
extern ulong hp_rec_hashnr(HP_KEYDEF *keyinfo,const uchar *rec);
extern ulong hp_mask(ulong hashnr,ulong buffmax,ulong maxlength);
extern void hp_movelink(HASH_INFO *pos,HASH_INFO *next_link,
			 HASH_INFO *newlink);
extern int hp_rec_key_cmp(HP_KEYDEF *keydef,const uchar *rec1,
			  const uchar *rec2,
                          my_bool diff_if_only_endspace_difference);
extern int hp_key_cmp(HP_KEYDEF *keydef,const uchar *rec,
		      const uchar *key);
extern void hp_make_key(HP_KEYDEF *keydef,uchar *key,const uchar *rec);
extern uint hp_rb_make_key(HP_KEYDEF *keydef, uchar *key,
			   const uchar *rec, uchar *recpos);
extern uint hp_rb_key_length(HP_KEYDEF *keydef, const uchar *key);
extern uint hp_rb_null_key_length(HP_KEYDEF *keydef, const uchar *key);
extern uint hp_rb_var_key_length(HP_KEYDEF *keydef, const uchar *key);
extern my_bool hp_if_null_in_key(HP_KEYDEF *keyinfo, const uchar *record);
extern int hp_close(register HP_INFO *info);
extern void hp_clear(HP_SHARE *info);
extern void hp_clear_keys(HP_SHARE *info);
extern uint hp_rb_pack_key(HP_KEYDEF *keydef, uchar *key, const uchar *old,
                           key_part_map keypart_map);

extern mysql_mutex_t THR_LOCK_heap;

#ifdef HAVE_PSI_INTERFACE
extern PSI_mutex_key hp_key_mutex_HP_SHARE_intern_lock;
void init_heap_psi_keys();
#endif /* HAVE_PSI_INTERFACE */

C_MODE_END
