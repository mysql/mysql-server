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

/* This file is included in all heap-files */

#include <my_base.h>			/* This includes global */
#ifdef THREAD
#include <my_pthread.h>
#endif
#include "heap.h"			/* Structs & some defines */

	/* Some extern variables */

extern LIST *heap_open_list,*heap_share_list;

#define test_active(info) \
if (!(info->update & HA_STATE_AKTIV))\
{ my_errno=HA_ERR_NO_ACTIVE_RECORD; DBUG_RETURN(-1); }
#define hp_find_hash(A,B) ((HASH_INFO*) _hp_find_block((A),(B)))

	/* Find pos for record and update it in info->current_ptr */
#define _hp_find_record(info,pos) (info)->current_ptr= _hp_find_block(&(info)->s->block,pos)

typedef struct st_hp_hash_info
{
  struct st_hp_hash_info *next_key;
  byte *ptr_to_rec;
} HASH_INFO;

	/* Prototypes for intern functions */

extern HP_SHARE *_hp_find_named_heap(const char *name);
extern int _hp_rectest(HP_INFO *info,const byte *old);
extern void _hp_delete_file_from_open_list(HP_INFO *info);
extern byte *_hp_find_block(HP_BLOCK *info,ulong pos);
extern int _hp_get_new_block(HP_BLOCK *info, ulong* alloc_length);
extern void _hp_free(HP_SHARE *info);
extern byte *_hp_free_level(HP_BLOCK *block,uint level,HP_PTRS *pos,
				byte *last_pos);
extern int _hp_write_key(HP_SHARE *info,HP_KEYDEF *keyinfo,
			 const byte *record,byte *recpos);
extern int _hp_delete_key(HP_INFO *info,HP_KEYDEF *keyinfo,
			  const byte *record,byte *recpos,int flag);
extern HASH_INFO *_heap_find_hash(HP_BLOCK *block,ulong pos);
extern byte *_hp_search(HP_INFO *info,HP_KEYDEF *keyinfo,const byte *key,
			    uint nextflag);
extern byte *_hp_search_next(HP_INFO *info, HP_KEYDEF *keyinfo,
			     const byte *key,
			     HASH_INFO *pos);
extern ulong _hp_hashnr(HP_KEYDEF *keyinfo,const byte *key);
extern ulong _hp_rec_hashnr(HP_KEYDEF *keyinfo,const byte *rec);
extern ulong _hp_mask(ulong hashnr,ulong buffmax,ulong maxlength);
extern void _hp_movelink(HASH_INFO *pos,HASH_INFO *next_link,
			 HASH_INFO *newlink);
extern int _hp_rec_key_cmp(HP_KEYDEF *keydef,const byte *rec1,
			       const byte *rec2);
extern int _hp_key_cmp(HP_KEYDEF *keydef,const byte *rec,
			   const byte *key);
extern void _hp_make_key(HP_KEYDEF *keydef,byte *key,const byte *rec);
extern int _hp_close(register HP_INFO *info);
extern void _hp_clear(HP_SHARE *info);

#ifdef THREAD
extern pthread_mutex_t THR_LOCK_heap;
#else
#define pthread_mutex_lock(A)
#define pthread_mutex_unlock(A)
#endif
