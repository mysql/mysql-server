/* Copyright (C) 2000,2004 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

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

/* This file is included in all heap-files */

#include <my_base.h>			/* This includes global */
#ifdef THREAD
#include <my_pthread.h>
#endif
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
  byte *ptr_to_rec;
} HASH_INFO;

typedef struct {
  HA_KEYSEG *keyseg;
  uint key_length;
  uint search_flag;
} heap_rb_param;
      
	/* Prototypes for intern functions */

extern HP_SHARE *hp_find_named_heap(const char *name);
extern int hp_rectest(HP_INFO *info,const byte *old);
extern byte *hp_find_block(HP_BLOCK *info,ulong pos);
extern int hp_get_new_block(HP_BLOCK *info, ulong* alloc_length);
extern void hp_free(HP_SHARE *info);
extern byte *hp_free_level(HP_BLOCK *block,uint level,HP_PTRS *pos,
			   byte *last_pos);
extern int hp_write_key(HP_INFO *info, HP_KEYDEF *keyinfo,
			const byte *record, byte *recpos);
extern int hp_rb_write_key(HP_INFO *info, HP_KEYDEF *keyinfo, 
			   const byte *record, byte *recpos);
extern int hp_rb_delete_key(HP_INFO *info,HP_KEYDEF *keyinfo,
			    const byte *record,byte *recpos,int flag);
extern int hp_delete_key(HP_INFO *info,HP_KEYDEF *keyinfo,
			 const byte *record,byte *recpos,int flag);
extern HASH_INFO *_heap_find_hash(HP_BLOCK *block,ulong pos);
extern byte *hp_search(HP_INFO *info,HP_KEYDEF *keyinfo,const byte *key,
		       uint nextflag);
extern byte *hp_search_next(HP_INFO *info, HP_KEYDEF *keyinfo,
			    const byte *key, HASH_INFO *pos);
extern ulong hp_hashnr(HP_KEYDEF *keyinfo,const byte *key);
extern ulong hp_rec_hashnr(HP_KEYDEF *keyinfo,const byte *rec);
extern ulong hp_mask(ulong hashnr,ulong buffmax,ulong maxlength);
extern void hp_movelink(HASH_INFO *pos,HASH_INFO *next_link,
			 HASH_INFO *newlink);
extern int hp_rec_key_cmp(HP_KEYDEF *keydef,const byte *rec1,
			  const byte *rec2,
                          my_bool diff_if_only_endspace_difference);
extern int hp_key_cmp(HP_KEYDEF *keydef,const byte *rec,
		      const byte *key);
extern void hp_make_key(HP_KEYDEF *keydef,byte *key,const byte *rec);
extern uint hp_rb_make_key(HP_KEYDEF *keydef, byte *key, 
			   const byte *rec, byte *recpos);
extern uint hp_rb_key_length(HP_KEYDEF *keydef, const byte *key);
extern uint hp_rb_null_key_length(HP_KEYDEF *keydef, const byte *key);
extern uint hp_rb_var_key_length(HP_KEYDEF *keydef, const byte *key);
extern my_bool hp_if_null_in_key(HP_KEYDEF *keyinfo, const byte *record);
extern int hp_close(register HP_INFO *info);
extern void hp_clear(HP_SHARE *info);
extern void hp_clear_keys(HP_SHARE *info);
extern uint hp_rb_pack_key(HP_KEYDEF *keydef, uchar *key, const uchar *old, 
			   uint k_len);
#ifdef THREAD
extern pthread_mutex_t THR_LOCK_heap;
#else
#define pthread_mutex_lock(A)
#define pthread_mutex_unlock(A)
#endif
