/* Copyright (c) 2000, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/* Write a record to heap-databas */

#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <sys/types.h>

#include "my_dbug.h"
#include "my_inttypes.h"
#include "storage/heap/heapdef.h"
#ifdef _WIN32
#include <fcntl.h>
#endif

#define LOWFIND 1
#define LOWUSED 2
#define HIGHFIND 4
#define HIGHUSED 8

static uchar *next_free_record_pos(HP_SHARE *info);
static HASH_INFO *hp_find_free_hash(HP_SHARE *info, HP_BLOCK *block,
                                    ulong records);

int heap_write(HP_INFO *info, const uchar *record) {
  HP_KEYDEF *keydef, *end;
  uchar *pos;
  HP_SHARE *share = info->s;
  DBUG_TRACE;
#ifndef NDEBUG
  if (info->mode & O_RDONLY) {
    set_my_errno(EACCES);
    return EACCES;
  }
#endif
  if (!(pos = next_free_record_pos(share))) return my_errno();
  share->changed = 1;

  for (keydef = share->keydef, end = keydef + share->keys; keydef < end;
       keydef++) {
    if ((*keydef->write_key)(info, keydef, record, pos)) goto err;
  }

  memcpy(pos, record, (size_t)share->reclength);
  pos[share->reclength] = 1; /* Mark record as not deleted */
  if (++share->records == share->blength) share->blength += share->blength;
  info->current_ptr = pos;
  info->current_hash_ptr = nullptr;
  info->update |= HA_STATE_AKTIV;
#if !defined(NDEBUG) && defined(EXTRA_HEAP_DEBUG)
  DBUG_EXECUTE("check_heap", heap_check_heap(info, 0););
#endif
  if (share->auto_key) heap_update_auto_increment(info, record);
  return 0;

err:
  if (my_errno() == HA_ERR_FOUND_DUPP_KEY)
    DBUG_PRINT("info", ("Duplicate key: %d", (int)(keydef - share->keydef)));
  info->errkey = (int)(keydef - share->keydef);
  /*
    We don't need to delete non-inserted key from rb-tree.  Also, if
    we got ENOMEM, the key wasn't inserted, so don't try to delete it
    either.  Otherwise for HASH index on HA_ERR_FOUND_DUPP_KEY the key
    was inserted and we have to delete it.
  */
  if (keydef->algorithm == HA_KEY_ALG_BTREE || my_errno() == ENOMEM) {
    keydef--;
  }
  while (keydef >= share->keydef) {
    if ((*keydef->delete_key)(info, keydef, record, pos, 0)) break;
    keydef--;
  }

  share->deleted++;
  *((uchar **)pos) = share->del_link;
  share->del_link = pos;
  pos[share->reclength] = 0; /* Record deleted */

  return my_errno();
} /* heap_write */

/*
  Write a key to rb_tree-index
*/

int hp_rb_write_key(HP_INFO *info, HP_KEYDEF *keyinfo, const uchar *record,
                    uchar *recpos) {
  heap_rb_param custom_arg;
  uint old_allocated;

  custom_arg.keyseg = keyinfo->seg;
  custom_arg.key_length = hp_rb_make_key(keyinfo, info->recbuf, record, recpos);
  if (keyinfo->flag & HA_NOSAME) {
    custom_arg.search_flag = SEARCH_FIND | SEARCH_UPDATE;
    keyinfo->rb_tree.flag = TREE_NO_DUPS;
  } else {
    custom_arg.search_flag = SEARCH_SAME;
    keyinfo->rb_tree.flag = 0;
  }
  old_allocated = keyinfo->rb_tree.allocated;
  if (!tree_insert(&keyinfo->rb_tree, (void *)info->recbuf,
                   custom_arg.key_length, &custom_arg)) {
    set_my_errno(HA_ERR_FOUND_DUPP_KEY);
    return 1;
  }
  info->s->index_length += (keyinfo->rb_tree.allocated - old_allocated);
  return 0;
}

/* Find where to place new record */

static uchar *next_free_record_pos(HP_SHARE *info) {
  int block_pos;
  uchar *pos;
  size_t length;
  DBUG_TRACE;

  if (info->del_link) {
    pos = info->del_link;
    info->del_link = *((uchar **)pos);
    info->deleted--;
    DBUG_PRINT("exit", ("Used old position: %p", pos));
    return pos;
  }
  if (!(block_pos = (info->records % info->block.records_in_block))) {
    if ((info->records > info->max_records && info->max_records) ||
        (info->data_length + info->index_length >= info->max_table_size)) {
      set_my_errno(HA_ERR_RECORD_FILE_FULL);
      return nullptr;
    }
    if (hp_get_new_block(&info->block, &length)) return nullptr;
    info->data_length += length;
  }
  DBUG_PRINT("exit", ("Used new position: %p",
                      ((uchar *)info->block.level_info[0].last_blocks +
                       block_pos * info->block.recbuffer)));
  return (uchar *)info->block.level_info[0].last_blocks +
         block_pos * info->block.recbuffer;
}

/**
  Populate HASH_INFO structure.

  @param key           Pointer to a HASH_INFO key to be populated
  @param next_key      HASH_INFO next_key value
  @param ptr_to_rec    HASH_INFO ptr_to_rec value
  @param hash          HASH_INFO hash value
*/

static inline void set_hash_key(HASH_INFO *key, HASH_INFO *next_key,
                                uchar *ptr_to_rec, ulong hash) {
  key->next_key = next_key;
  key->ptr_to_rec = ptr_to_rec;
  key->hash = hash;
}

/*
  Write a hash-key to the hash-index
  SYNOPSIS
    info     Heap table info
    keyinfo  Key info
    record   Table record to added
    recpos   Memory buffer where the table record will be stored if added
             successfully
  NOTE
    Hash index uses HP_BLOCK structure as a 'growable array' of HASH_INFO
    structs. Array size == number of entries in hash index.
    hp_mask(hp_rec_hashnr()) maps hash entries values to hash array positions.
    If there are several hash entries with the same hash array position P,
    they are connected in a linked list via HASH_INFO::next_key. The first
    list element is located at position P, next elements are located at
    positions for which there is no record that should be located at that
    position. The order of elements in the list is arbitrary.

  RETURN
    0  - OK
    -1 - Out of memory
    HA_ERR_FOUND_DUPP_KEY - Duplicate record on unique key. The record was
    still added and the caller must call hp_delete_key for it.
*/

int hp_write_key(HP_INFO *info, HP_KEYDEF *keyinfo, const uchar *record,
                 uchar *recpos) {
  HP_SHARE *share = info->s;
  int flag;
  ulong halfbuff, hashnr, first_index;
  uchar *ptr_to_rec = nullptr, *ptr_to_rec2 = nullptr;
  ulong hash1 = 0, hash2 = 0;
  HASH_INFO *empty, *gpos = nullptr, *gpos2 = nullptr, *pos;
  DBUG_TRACE;

  flag = 0;
  if (!(empty = hp_find_free_hash(share, &keyinfo->block, share->records)))
    return -1; /* No more memory */
  halfbuff = (long)share->blength >> 1;
  pos =
      hp_find_hash(&keyinfo->block, (first_index = share->records - halfbuff));

  /*
    We're about to add one more hash array position, with hash_mask=#records.
    The number of hash positions will change and some entries might need to
    be relocated to the newly added position. Those entries are currently
    members of the list that starts at #first_index position (this is
    guaranteed by properties of hp_mask(hp_rec_hashnr(X)) mapping function)
    At #first_index position currently there may be either:
    a) An entry with hashnr != first_index. We don't need to move it.
    or
    b) A list of items with hash_mask=first_index. The list contains entries
       of 2 types:
       1) entries that should be relocated to the list that starts at new
          position we're adding ('uppper' list)
       2) entries that should be left in the list starting at #first_index
          position ('lower' list)
  */
  if (pos != empty) /* If some records */
  {
    do {
      hashnr = pos->hash;
      if (flag == 0) {
        /*
          First loop, bail out if we're dealing with case a) from above
          comment
        */
        if (hp_mask(hashnr, share->blength, share->records) != first_index)
          break;
      }
      /*
        flag & LOWFIND - found a record that should be put into lower position
        flag & LOWUSED - lower position occupied by the record
        Same for HIGHFIND and HIGHUSED and 'upper' position

        gpos  - ptr to last element in lower position's list
        gpos2 - ptr to last element in upper position's list

        ptr_to_rec - ptr to last entry that should go into lower list.
        ptr_to_rec2 - same for upper list.
      */
      if (!(hashnr & halfbuff)) {
        /* Key should be put into 'lower' list */
        if (!(flag & LOWFIND)) {
          /* key is the first element to go into lower position */
          if (flag & HIGHFIND) {
            flag = LOWFIND | HIGHFIND;
            /* key shall be moved to the current empty position */
            gpos = empty;
            ptr_to_rec = pos->ptr_to_rec;
            empty = pos; /* This place is now free */
          } else {
            /*
              We can only get here at first iteration: key is at 'lower'
              position pos and should be left here.
            */
            flag = LOWFIND | LOWUSED;
            gpos = pos;
            ptr_to_rec = pos->ptr_to_rec;
          }
        } else {
          /* Already have another key for lower position */
          if (!(flag & LOWUSED)) {
            /* Change link of previous lower-list key */
            set_hash_key(gpos, pos, ptr_to_rec, hash1);
            flag = (flag & HIGHFIND) | (LOWFIND | LOWUSED);
          }
          gpos = pos;
          ptr_to_rec = pos->ptr_to_rec;
        }
        hash1 = pos->hash;
      } else {
        /* key will be put into 'higher' list */
        if (!(flag & HIGHFIND)) {
          flag = (flag & LOWFIND) | HIGHFIND;
          /* key shall be moved to the last (empty) position */
          gpos2 = empty;
          empty = pos;
          ptr_to_rec2 = pos->ptr_to_rec;
        } else {
          if (!(flag & HIGHUSED)) {
            /* Change link of previous upper-list key and save */
            set_hash_key(gpos2, pos, ptr_to_rec2, hash2);
            flag = (flag & LOWFIND) | (HIGHFIND | HIGHUSED);
          }
          gpos2 = pos;
          ptr_to_rec2 = pos->ptr_to_rec;
        }
        hash2 = pos->hash;
      }
    } while ((pos = pos->next_key));

    if ((flag & (LOWFIND | HIGHFIND)) == (LOWFIND | HIGHFIND)) {
      /*
        If both 'higher' and 'lower' list have at least one element, now
        there are two hash buckets instead of one.
      */
      keyinfo->hash_buckets++;
    }

    if ((flag & (LOWFIND | LOWUSED)) == LOWFIND) {
      set_hash_key(gpos, nullptr, ptr_to_rec, hash1);
    }
    if ((flag & (HIGHFIND | HIGHUSED)) == HIGHFIND) {
      set_hash_key(gpos2, nullptr, ptr_to_rec2, hash2);
    }
  }
  /* Check if we are at the empty position */
  hash1 = hp_rec_hashnr(keyinfo, record);
  pos = hp_find_hash(&keyinfo->block,
                     hp_mask(hash1, share->blength, share->records + 1));
  if (pos == empty) {
    set_hash_key(pos, nullptr, recpos, hash1);
    keyinfo->hash_buckets++;
  } else {
    /* Check if more records in same hash-nr family */
    empty[0] = pos[0];
    gpos = hp_find_hash(&keyinfo->block,
                        hp_mask(pos->hash, share->blength, share->records + 1));
    if (pos == gpos) {
      set_hash_key(pos, empty, recpos, hash1);
    } else {
      set_hash_key(pos, nullptr, recpos, hash1);
      keyinfo->hash_buckets++;
      hp_movelink(pos, gpos, empty);
    }

    /* Check if duplicated keys */
    if ((keyinfo->flag & HA_NOSAME) && pos == gpos &&
        (!(keyinfo->flag & HA_NULL_PART_KEY) ||
         !hp_if_null_in_key(keyinfo, record))) {
      pos = empty;
      do {
        if (hash1 == pos->hash &&
            !hp_rec_key_cmp(keyinfo, record, pos->ptr_to_rec)) {
          set_my_errno(HA_ERR_FOUND_DUPP_KEY);
          return HA_ERR_FOUND_DUPP_KEY;
        }
      } while ((pos = pos->next_key));
    }
  }
  return 0;
}

/* Returns ptr to block, and allocates block if needed */

static HASH_INFO *hp_find_free_hash(HP_SHARE *info, HP_BLOCK *block,
                                    ulong records) {
  uint block_pos;
  size_t length;

  if (records < block->last_allocated) return hp_find_hash(block, records);
  if (!(block_pos = (records % block->records_in_block))) {
    if (hp_get_new_block(block, &length)) return (nullptr);
    info->index_length += length;
  }
  block->last_allocated = records + 1;
  return ((HASH_INFO *)((uchar *)block->level_info[0].last_blocks +
                        block_pos * block->recbuffer));
}
