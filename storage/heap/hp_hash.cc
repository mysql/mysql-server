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

/* The hash functions used for saving keys */

#include <inttypes.h>
#include <sys/types.h>

#include <algorithm>
#include <cmath>

#include "my_byteorder.h"
#include "my_compiler.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_macros.h"
#include "mysql/strings/m_ctype.h"
#include "storage/heap/heapdef.h"

/*
  Find out how many rows there is in the given range

  SYNOPSIS
    hp_rb_records_in_range()
    info		HEAP handler
    inx			Index to use
    min_key		Min key. Is = 0 if no min range
    max_key		Max key. Is = 0 if no max range

  NOTES
    min_key.flag can have one of the following values:
      HA_READ_KEY_EXACT		Include the key in the range
      HA_READ_AFTER_KEY		Don't include key in range

    max_key.flag can have one of the following values:
      HA_READ_BEFORE_KEY	Don't include key in range
      HA_READ_AFTER_KEY		Include all 'end_key' values in the range

  RETURN
   HA_POS_ERROR		Something is wrong with the index tree.
   0			There is no matching keys in the given range
   number > 0		There is approximately 'number' matching rows in
                        the range.
*/

ha_rows hp_rb_records_in_range(HP_INFO *info, int inx, key_range *min_key,
                               key_range *max_key) {
  ha_rows start_pos, end_pos;
  HP_KEYDEF *keyinfo = info->s->keydef + inx;
  TREE *rb_tree = &keyinfo->rb_tree;
  heap_rb_param custom_arg;
  DBUG_TRACE;

  info->lastinx = inx;
  custom_arg.keyseg = keyinfo->seg;
  custom_arg.search_flag = SEARCH_FIND | SEARCH_SAME;
  if (min_key) {
    custom_arg.key_length = hp_rb_pack_key(keyinfo, info->recbuf, min_key->key,
                                           min_key->keypart_map);
    start_pos =
        tree_record_pos(rb_tree, info->recbuf, min_key->flag, &custom_arg);
  } else {
    start_pos = 0;
  }

  if (max_key) {
    custom_arg.key_length = hp_rb_pack_key(keyinfo, info->recbuf, max_key->key,
                                           max_key->keypart_map);
    end_pos =
        tree_record_pos(rb_tree, info->recbuf, max_key->flag, &custom_arg);
  } else {
    end_pos = rb_tree->elements_in_tree + (ha_rows)1;
  }

  DBUG_PRINT("info", ("start_pos: %lu  end_pos: %lu", (ulong)start_pos,
                      (ulong)end_pos));
  if (start_pos == HA_POS_ERROR || end_pos == HA_POS_ERROR) return HA_POS_ERROR;
  return end_pos < start_pos
             ? (ha_rows)0
             : (end_pos == start_pos ? (ha_rows)1 : end_pos - start_pos);
}

/* Search after a record based on a key */
/* Sets info->current_ptr to found record */
/* next_flag:  Search=0, next=1, prev =2, same =3 */

uchar *hp_search(HP_INFO *info, HP_KEYDEF *keyinfo, const uchar *key,
                 uint nextflag) {
  HASH_INFO *pos, *prev_ptr;
  int flag;
  uint old_nextflag;
  HP_SHARE *share = info->s;
  DBUG_TRACE;
  old_nextflag = nextflag;
  flag = 1;
  prev_ptr = nullptr;

  if (share->records) {
    pos = hp_find_hash(
        &keyinfo->block,
        hp_mask(hp_hashnr(keyinfo, key), share->blength, share->records));
    do {
      if (!hp_key_cmp(keyinfo, pos->ptr_to_rec, key)) {
        switch (nextflag) {
          case 0: /* Search after key */
            DBUG_PRINT("exit", ("found key at %p", pos->ptr_to_rec));
            info->current_hash_ptr = pos;
            return info->current_ptr = pos->ptr_to_rec;
          case 1: /* Search next */
            if (pos->ptr_to_rec == info->current_ptr) nextflag = 0;
            break;
          case 2: /* Search previous */
            if (pos->ptr_to_rec == info->current_ptr) {
              set_my_errno(HA_ERR_KEY_NOT_FOUND); /* If gpos == 0 */
              info->current_hash_ptr = prev_ptr;
              return info->current_ptr =
                         prev_ptr ? prev_ptr->ptr_to_rec : nullptr;
            }
            prev_ptr = pos; /* Prev. record found */
            break;
          case 3: /* Search same */
            if (pos->ptr_to_rec == info->current_ptr) {
              info->current_hash_ptr = pos;
              return info->current_ptr;
            }
        }
      }
      if (flag) {
        flag = 0; /* Reset flag */
        if (hp_find_hash(&keyinfo->block,
                         hp_mask(hp_rec_hashnr(keyinfo, pos->ptr_to_rec),
                                 share->blength, share->records)) != pos)
          break; /* Wrong link */
      }
    } while ((pos = pos->next_key));
  }
  set_my_errno(HA_ERR_KEY_NOT_FOUND);
  if (nextflag == 2 && !info->current_ptr) {
    /* Do a previous from end */
    info->current_hash_ptr = prev_ptr;
    return info->current_ptr = prev_ptr ? prev_ptr->ptr_to_rec : nullptr;
  }

  if (old_nextflag && nextflag)
    set_my_errno(HA_ERR_RECORD_CHANGED); /* Didn't find old record */
  DBUG_PRINT("exit", ("Error: %d", my_errno()));
  info->current_hash_ptr = nullptr;
  return (info->current_ptr = nullptr);
}

/*
  Search next after last read;  Assumes that the table hasn't changed
  since last read !
*/

uchar *hp_search_next(HP_INFO *info, HP_KEYDEF *keyinfo, const uchar *key,
                      HASH_INFO *pos) {
  DBUG_TRACE;

  while ((pos = pos->next_key)) {
    if (!hp_key_cmp(keyinfo, pos->ptr_to_rec, key)) {
      info->current_hash_ptr = pos;
      return info->current_ptr = pos->ptr_to_rec;
    }
  }
  set_my_errno(HA_ERR_KEY_NOT_FOUND);
  DBUG_PRINT("exit", ("Error: %d", my_errno()));
  info->current_hash_ptr = nullptr;
  return (info->current_ptr = nullptr);
}

/*
  Calculate position number for hash value.
  SYNOPSIS
    hp_mask()
      hashnr     Hash value
      buffmax    Value such that
                 2^(n-1) < maxlength <= 2^n = buffmax
      maxlength

  RETURN
    Array index, in [0..maxlength)
*/

uint64 hp_mask(uint64 hashnr, uint64 buffmax, uint64 maxlength) {
  if ((hashnr & (buffmax - 1)) < maxlength) return (hashnr & (buffmax - 1));
  return (hashnr & ((buffmax >> 1) - 1));
}

/*
  Change
    next_link -> ... -> X -> pos
  to
    next_link -> ... -> X -> newlink
*/

void hp_movelink(HASH_INFO *pos, HASH_INFO *next_link, HASH_INFO *newlink) {
  HASH_INFO *old_link;
  do {
    old_link = next_link;
  } while ((next_link = next_link->next_key) != pos);
  old_link->next_key = newlink;
  return;
}

/* Calc hashvalue for a key */

uint64 hp_hashnr(HP_KEYDEF *keydef, const uchar *key) {
  /*register*/
  uint64 nr = 1, nr2 = 4;
  HA_KEYSEG *seg, *endseg;

  for (seg = keydef->seg, endseg = seg + keydef->keysegs; seg < endseg; seg++) {
    const uchar *pos = key;
    key += seg->length;
    if (seg->null_bit) {
      key++;    /* Skip null byte */
      if (*pos) /* Found null */
      {
        nr ^= (nr << 1) | 1;
        /* Add key pack length (2) to key for VARCHAR segments */
        if (seg->type == HA_KEYTYPE_VARTEXT1) key += 2;
        continue;
      }
      pos++;
    }
    if (seg->type == HA_KEYTYPE_TEXT) {
      const CHARSET_INFO *cs = seg->charset;
      size_t length = seg->length;
      if (cs->mbmaxlen > 1 && (seg->flag & HA_PART_KEY_SEG)) {
        size_t char_length;
        char_length = my_charpos(cs, pos, pos + length, length / cs->mbmaxlen);
        length = std::min(length, char_length);
      }
      if (cs->pad_attribute == NO_PAD) {
        /*
          MySQL specifies that CHAR fields are stripped of
          trailing spaces before being returned from the database.
          Normally this is done in Field_string::val_str(),
          but since we don't involve the Field classes for
          hashing, we need to do the same thing here
          for NO PAD collations. (If not, hash_sort will ignore
          the spaces for us, so we don't need to do it here.)
        */
        length = cs->cset->lengthsp(cs, (const char *)pos, length);
      }
      cs->coll->hash_sort(cs, pos, length, &nr, &nr2);
    } else if (seg->type == HA_KEYTYPE_VARTEXT1) /* Any VARCHAR segments */
    {
      const CHARSET_INFO *cs = seg->charset;
      uint pack_length = 2; /* Key packing is constant */
      size_t length = uint2korr(pos);
      if (cs->mbmaxlen > 1 && (seg->flag & HA_PART_KEY_SEG)) {
        size_t char_length;
        char_length =
            my_charpos(cs, pos + pack_length, pos + pack_length + length,
                       seg->length / cs->mbmaxlen);
        length = std::min(length, char_length);
      }
      cs->coll->hash_sort(cs, pos + pack_length, length, &nr, &nr2);
      key += pack_length;
    } else {
      for (; pos < key; pos++) {
        nr ^= (uint64)((((uint)nr & 63) + nr2) * ((uint)*pos)) + (nr << 8);
        nr2 += 3;
      }
    }
  }
  DBUG_PRINT("exit", ("hash: 0x%" PRIx64, nr));
  return nr;
}

/* Calc hashvalue for a key in a record */

uint64 hp_rec_hashnr(HP_KEYDEF *keydef, const uchar *rec) {
  uint64 nr = 1, nr2 = 4;
  HA_KEYSEG *seg, *endseg;

  for (seg = keydef->seg, endseg = seg + keydef->keysegs; seg < endseg; seg++) {
    const uchar *pos = rec + seg->start, *end = pos + seg->length;
    if (seg->null_bit) {
      if (rec[seg->null_pos] & seg->null_bit) {
        nr ^= (nr << 1) | 1;
        continue;
      }
    }
    if (seg->type == HA_KEYTYPE_TEXT) {
      const CHARSET_INFO *cs = seg->charset;
      size_t char_length = seg->length;
      if (cs->mbmaxlen > 1 && (seg->flag & HA_PART_KEY_SEG)) {
        char_length =
            my_charpos(cs, pos, pos + char_length, char_length / cs->mbmaxlen);
        char_length =
            std::min(char_length, size_t(seg->length)); /* QQ: ok to remove? */
      }
      if (cs->pad_attribute == NO_PAD) {
        /*
          MySQL specifies that CHAR fields are stripped of
          trailing spaces before being returned from the database.
          Normally this is done in Field_string::val_str(),
          but since we don't involve the Field classes for
          hashing, we need to do the same thing here
          for NO PAD collations. (If not, hash_sort will ignore
          the spaces for us, so we don't need to do it here.)
        */
        char_length = cs->cset->lengthsp(cs, (const char *)pos, char_length);
      }
      cs->coll->hash_sort(cs, pos, char_length, &nr, &nr2);
    } else if (seg->type == HA_KEYTYPE_VARTEXT1) /* Any VARCHAR segments */
    {
      const CHARSET_INFO *cs = seg->charset;
      uint pack_length = seg->bit_start;
      size_t length = (pack_length == 1 ? (uint)*pos : uint2korr(pos));
      if (cs->mbmaxlen > 1 && (seg->flag & HA_PART_KEY_SEG)) {
        size_t char_length;
        char_length =
            my_charpos(cs, pos + pack_length, pos + pack_length + length,
                       seg->length / cs->mbmaxlen);
        length = std::min(length, char_length);
      }
      cs->coll->hash_sort(cs, pos + pack_length, length, &nr, &nr2);
    } else {
      for (; pos < end; pos++) {
        nr ^= (uint64)((((uint)nr & 63) + nr2) * ((uint)*pos)) + (nr << 8);
        nr2 += 3;
      }
    }
  }
  DBUG_PRINT("exit", ("hash: 0x%" PRIx64, nr));
  return (nr);
}

/*
  Compare keys for two records. Returns 0 if they are identical

  SYNOPSIS
    hp_rec_key_cmp()
    keydef		Key definition
    rec1		Record to compare
    rec2		Other record to compare

  RETURN
    0		Key is identical
    <> 0 	Key differs
*/

int hp_rec_key_cmp(HP_KEYDEF *keydef, const uchar *rec1, const uchar *rec2) {
  HA_KEYSEG *seg, *endseg;

  for (seg = keydef->seg, endseg = seg + keydef->keysegs; seg < endseg; seg++) {
    if (seg->null_bit) {
      if ((rec1[seg->null_pos] & seg->null_bit) !=
          (rec2[seg->null_pos] & seg->null_bit))
        return 1;
      if (rec1[seg->null_pos] & seg->null_bit) continue;
    }
    if (seg->type == HA_KEYTYPE_TEXT) {
      const CHARSET_INFO *cs = seg->charset;
      size_t char_length1;
      size_t char_length2;
      const uchar *pos1 = rec1 + seg->start;
      const uchar *pos2 = rec2 + seg->start;
      if (cs->mbmaxlen > 1 && (seg->flag & HA_PART_KEY_SEG)) {
        size_t char_length = seg->length / cs->mbmaxlen;
        char_length1 = my_charpos(cs, pos1, pos1 + seg->length, char_length);
        char_length1 = std::min(char_length1, size_t(seg->length));
        char_length2 = my_charpos(cs, pos2, pos2 + seg->length, char_length);
        char_length2 = std::min(char_length2, size_t(seg->length));
      } else {
        char_length1 = char_length2 = seg->length;
      }
      if (cs->pad_attribute == NO_PAD) {
        /*
          MySQL specifies that CHAR fields are stripped of
          trailing spaces before being returned from the database.
          Normally this is done in Field_string::val_str(),
          but since we don't involve the Field classes for
          internal comparisons, we need to do the same thing here
          for NO PAD collations. (If not, strnncollsp will ignore
          the spaces for us, so we don't need to do it here.)
        */
        char_length1 = cs->cset->lengthsp(cs, (const char *)pos1, char_length1);
        char_length2 = cs->cset->lengthsp(cs, (const char *)pos2, char_length2);
      }
      if (cs->coll->strnncollsp(cs, pos1, char_length1, pos2, char_length2))
        return 1;
    } else if (seg->type == HA_KEYTYPE_VARTEXT1) /* Any VARCHAR segments */
    {
      const uchar *pos1 = rec1 + seg->start;
      const uchar *pos2 = rec2 + seg->start;
      uint char_length1, char_length2;
      uint pack_length = seg->bit_start;
      const CHARSET_INFO *cs = seg->charset;
      if (pack_length == 1) {
        char_length1 = (uint) * (pos1++);
        char_length2 = (uint) * (pos2++);
      } else {
        char_length1 = uint2korr(pos1);
        char_length2 = uint2korr(pos2);
        pos1 += 2;
        pos2 += 2;
      }
      if (cs->mbmaxlen > 1 && (seg->flag & HA_PART_KEY_SEG)) {
        uint safe_length1 = char_length1;
        uint safe_length2 = char_length2;
        uint char_length = seg->length / cs->mbmaxlen;
        char_length1 = my_charpos(cs, pos1, pos1 + char_length1, char_length);
        char_length1 = std::min(char_length1, safe_length1);
        char_length2 = my_charpos(cs, pos2, pos2 + char_length2, char_length);
        char_length2 = std::min(char_length2, safe_length2);
      }

      if (cs->coll->strnncollsp(seg->charset, pos1, char_length1, pos2,
                                char_length2))
        return 1;
    } else {
      if (memcmp(rec1 + seg->start, rec2 + seg->start, seg->length)) return 1;
    }
  }
  return 0;
}

/* Compare a key in a record to a whole key */

int hp_key_cmp(HP_KEYDEF *keydef, const uchar *rec, const uchar *key) {
  HA_KEYSEG *seg, *endseg;

  for (seg = keydef->seg, endseg = seg + keydef->keysegs; seg < endseg;
       key += (seg++)->length) {
    if (seg->null_bit) {
      bool found_null = (rec[seg->null_pos] & seg->null_bit);
      assert(*key == 0x00 || *key == 0x01);
      if (found_null != (bool)*key++) return 1;
      if (found_null) {
        /* Add key pack length (2) to key for VARCHAR segments */
        if (seg->type == HA_KEYTYPE_VARTEXT1) key += 2;
        continue;
      }
    }
    if (seg->type == HA_KEYTYPE_TEXT) {
      const CHARSET_INFO *cs = seg->charset;
      uint char_length_key;
      uint char_length_rec;
      const uchar *pos = rec + seg->start;
      if (cs->mbmaxlen > 1 && (seg->flag & HA_PART_KEY_SEG)) {
        uint char_length = seg->length / cs->mbmaxlen;
        char_length_key = my_charpos(cs, key, key + seg->length, char_length);
        char_length_key = std::min(char_length_key, uint(seg->length));
        char_length_rec = my_charpos(cs, pos, pos + seg->length, char_length);
        char_length_rec = std::min(char_length_rec, uint(seg->length));
      } else {
        char_length_key = seg->length;
        char_length_rec = seg->length;
      }

      if (cs->pad_attribute == NO_PAD) {
        /*
          MySQL specifies that CHAR fields are stripped of
          trailing spaces before being returned from the database.
          Normally this is done in Field_string::val_str(),
          but since we don't involve the Field classes for
          internal comparisons, we need to do the same thing here
          for NO PAD collations. (If not, strnncollsp will ignore
          the spaces for us, so we don't need to do it here.)
        */
        char_length_rec =
            cs->cset->lengthsp(cs, (const char *)pos, char_length_rec);
        char_length_key =
            cs->cset->lengthsp(cs, (const char *)key, char_length_key);
      }

      if (cs->coll->strnncollsp(cs, pos, char_length_rec, key, char_length_key))
        return 1;
    } else if (seg->type == HA_KEYTYPE_VARTEXT1) /* Any VARCHAR segments */
    {
      const uchar *pos = rec + seg->start;
      const CHARSET_INFO *cs = seg->charset;
      uint pack_length = seg->bit_start;
      uint char_length_rec = (pack_length == 1 ? (uint)*pos : uint2korr(pos));
      /* Key segments are always packed with 2 bytes */
      uint char_length_key = uint2korr(key);
      pos += pack_length;
      key += 2; /* skip key pack length */
      if (cs->mbmaxlen > 1 && (seg->flag & HA_PART_KEY_SEG)) {
        uint char_length1, char_length2;
        char_length1 = char_length2 = seg->length / cs->mbmaxlen;
        char_length1 = my_charpos(cs, key, key + char_length_key, char_length1);
        char_length_key = std::min(char_length_key, char_length1);
        char_length2 = my_charpos(cs, pos, pos + char_length_rec, char_length2);
        char_length_rec = std::min(char_length_rec, char_length2);
      } else {
        char_length_rec = std::min(char_length_rec, uint(seg->length));
      }

      if (cs->coll->strnncollsp(seg->charset, pos, char_length_rec, key,
                                char_length_key))
        return 1;
    } else {
      if (memcmp(rec + seg->start, key, seg->length)) return 1;
    }
  }
  return 0;
}

/* Copy a key from a record to a keybuffer */

void hp_make_key(HP_KEYDEF *keydef, uchar *key, const uchar *rec) {
  HA_KEYSEG *seg, *endseg;

  for (seg = keydef->seg, endseg = seg + keydef->keysegs; seg < endseg; seg++) {
    const CHARSET_INFO *cs = seg->charset;
    uint char_length = seg->length;
    const uchar *pos = rec + seg->start;
    if (seg->null_bit) {
      bool rec_is_null = rec[seg->null_pos] & seg->null_bit;
      *key++ = (rec_is_null ? 1 : 0);
    }
    if (cs->mbmaxlen > 1 && (seg->flag & HA_PART_KEY_SEG)) {
      char_length =
          my_charpos(cs, pos, pos + seg->length, char_length / cs->mbmaxlen);
      char_length =
          std::min(char_length, uint(seg->length)); /* QQ: ok to remove? */
    }
    if (seg->type == HA_KEYTYPE_VARTEXT1)
      char_length += seg->bit_start; /* Copy also length */
    memcpy(key, rec + seg->start, (size_t)char_length);
    key += char_length;
  }
}

#define FIX_LENGTH(cs, pos, length, char_length)                    \
  do {                                                              \
    if (length > char_length)                                       \
      char_length = my_charpos(cs, pos, pos + length, char_length); \
    char_length = std::min(char_length, size_t(length));            \
  } while (0)

uint hp_rb_make_key(HP_KEYDEF *keydef, uchar *key, const uchar *rec,
                    uchar *recpos) {
  uchar *start_key = key;
  HA_KEYSEG *seg, *endseg;

  for (seg = keydef->seg, endseg = seg + keydef->keysegs; seg < endseg; seg++) {
    size_t char_length;
    if (seg->null_bit) {
      bool rec_is_null = rec[seg->null_pos] & seg->null_bit;
      if (!(*key++ = 1 - (rec_is_null ? 1 : 0))) continue;
    }
    if (seg->flag & HA_SWAP_KEY) {
      uint length = seg->length;
      const uchar *pos = rec + seg->start;
      if (seg->type == HA_KEYTYPE_FLOAT) {
        float nr = float4get(pos);
        if (std::isnan(nr)) {
          /* Replace NAN with zero */
          memset(key, 0, length);
          key += length;
          continue;
        }
      } else if (seg->type == HA_KEYTYPE_DOUBLE) {
        double nr = float8get(pos);
        if (std::isnan(nr)) {
          memset(key, 0, length);
          key += length;
          continue;
        }
      }
      pos += length;
      while (length--) {
        *key++ = *--pos;
      }
      continue;
    }

    if (seg->flag & (HA_VAR_LENGTH_PART | HA_BLOB_PART)) {
      const uchar *pos = rec + seg->start;
      uint length = seg->length;
      uint pack_length = seg->bit_start;
      uint tmp_length = (pack_length == 1 ? (uint)*pos : uint2korr(pos));
      const CHARSET_INFO *cs = seg->charset;
      char_length = length / cs->mbmaxlen;

      pos += pack_length; /* Skip VARCHAR length */
      length = std::min(length, tmp_length);
      FIX_LENGTH(cs, pos, length, char_length);
      store_key_length_inc(key, char_length);
      memcpy(key, pos, char_length);
      key += char_length;
      continue;
    }

    char_length = seg->length;
    if (seg->charset->mbmaxlen > 1) {
      char_length = my_charpos(seg->charset, rec + seg->start,
                               rec + seg->start + char_length,
                               char_length / seg->charset->mbmaxlen);
      char_length =
          std::min(char_length, size_t(seg->length)); /* QQ: ok to remove? */
      if (char_length < seg->length)
        seg->charset->cset->fill(seg->charset, (char *)key + char_length,
                                 seg->length - char_length, ' ');
    }
    memcpy(key, rec + seg->start, (size_t)char_length);
    key += seg->length;
  }
  memcpy(key, &recpos, sizeof(uchar *));
  return (uint)(key - start_key);
}

uint hp_rb_pack_key(const HP_KEYDEF *keydef, uchar *key, const uchar *old,
                    key_part_map keypart_map) {
  HA_KEYSEG *seg, *endseg;
  uchar *start_key = key;

  for (seg = keydef->seg, endseg = seg + keydef->keysegs;
       seg < endseg && keypart_map; old += seg->length, seg++) {
    size_t char_length;
    keypart_map >>= 1;
    if (seg->null_bit) {
      /* Convert NULL from MySQL representation into HEAP's. */
      if (!(*key++ = (char)1 - *old++)) {
        /*
          Skip length part of a variable length field.
          Length of key-part used with heap_rkey() always 2.
          See also hp_hashnr().
        */
        if (seg->flag & (HA_VAR_LENGTH_PART | HA_BLOB_PART)) old += 2;
        continue;
      }
    }
    if (seg->flag & HA_SWAP_KEY) {
      uint length = seg->length;
      const uchar *pos = old + length;

      while (length--) {
        *key++ = *--pos;
      }
      continue;
    }
    if (seg->flag & (HA_VAR_LENGTH_PART | HA_BLOB_PART)) {
      /* Length of key-part used with heap_rkey() always 2 */
      uint tmp_length = uint2korr(old);
      uint length = seg->length;
      const CHARSET_INFO *cs = seg->charset;
      char_length = length / cs->mbmaxlen;

      old += 2;
      length = std::min(length, tmp_length); /* Safety */
      FIX_LENGTH(cs, old, length, char_length);
      store_key_length_inc(key, char_length);
      memcpy((uchar *)key, old, (size_t)char_length);
      key += char_length;
      continue;
    }
    char_length = seg->length;
    if (seg->charset->mbmaxlen > 1) {
      char_length = my_charpos(seg->charset, old, old + char_length,
                               char_length / seg->charset->mbmaxlen);
      char_length =
          std::min(char_length, size_t(seg->length)); /* QQ: ok to remove? */
      if (char_length < seg->length)
        seg->charset->cset->fill(seg->charset, (char *)key + char_length,
                                 seg->length - char_length, ' ');
    }
    memcpy(key, old, (size_t)char_length);
    key += seg->length;
  }
  return (uint)(key - start_key);
}

uint hp_rb_key_length(HP_KEYDEF *keydef, const uchar *key [[maybe_unused]]) {
  return keydef->length;
}

uint hp_rb_null_key_length(HP_KEYDEF *keydef, const uchar *key) {
  const uchar *start_key = key;
  HA_KEYSEG *seg, *endseg;

  for (seg = keydef->seg, endseg = seg + keydef->keysegs; seg < endseg; seg++) {
    if (seg->null_bit && !*key++) continue;
    key += seg->length;
  }
  return (uint)(key - start_key);
}

uint hp_rb_var_key_length(HP_KEYDEF *keydef, const uchar *key) {
  const uchar *start_key = key;
  HA_KEYSEG *seg, *endseg;

  for (seg = keydef->seg, endseg = seg + keydef->keysegs; seg < endseg; seg++) {
    uint length = seg->length;
    if (seg->null_bit && !*key++) continue;
    if (seg->flag & (HA_VAR_LENGTH_PART | HA_BLOB_PART)) {
      length = get_key_length(&key);
    }
    key += length;
  }
  return (uint)(key - start_key);
}

/*
  Test if any of the key parts are NULL.
  Return:
    1 if any of the key parts was NULL
    0 otherwise
*/

bool hp_if_null_in_key(HP_KEYDEF *keydef, const uchar *record) {
  HA_KEYSEG *seg, *endseg;
  for (seg = keydef->seg, endseg = seg + keydef->keysegs; seg < endseg; seg++) {
    if (seg->null_bit && (record[seg->null_pos] & seg->null_bit)) return true;
  }
  return false;
}

/*
  Update auto_increment info

  SYNOPSIS
    update_auto_increment()
    info			MyISAM handler
    record			Row to update

  IMPLEMENTATION
    Only replace the auto_increment value if it is higher than the previous
    one. For signed columns we don't update the auto increment value if it's
    less than zero.
*/

void heap_update_auto_increment(HP_INFO *info, const uchar *record) {
  ulonglong value = 0;  /* Store unsigned values here */
  longlong s_value = 0; /* Store signed values here */

  HA_KEYSEG *keyseg = info->s->keydef[info->s->auto_key - 1].seg;
  const uchar *key = record + keyseg->start;

  switch (info->s->auto_key_type) {
    case HA_KEYTYPE_INT8:
      s_value = (longlong) static_cast<char>(*key);
      break;
    case HA_KEYTYPE_BINARY:
      value = (ulonglong)*key;
      break;
    case HA_KEYTYPE_SHORT_INT:
      s_value = (longlong)sint2korr(key);
      break;
    case HA_KEYTYPE_USHORT_INT:
      value = (ulonglong)uint2korr(key);
      break;
    case HA_KEYTYPE_LONG_INT:
      s_value = (longlong)sint4korr(key);
      break;
    case HA_KEYTYPE_ULONG_INT:
      value = (ulonglong)uint4korr(key);
      break;
    case HA_KEYTYPE_INT24:
      s_value = (longlong)sint3korr(key);
      break;
    case HA_KEYTYPE_UINT24:
      value = (ulonglong)uint3korr(key);
      break;
    case HA_KEYTYPE_FLOAT: /* This shouldn't be used */
    {
      float f_1 = float4get(key);
      /* Ignore negative values */
      value = (f_1 < (float)0.0) ? 0 : (ulonglong)f_1;
      break;
    }
    case HA_KEYTYPE_DOUBLE: /* This shouldn't be used */
    {
      double f_1 = float8get(key);
      /* Ignore negative values */
      value = (f_1 < 0.0) ? 0 : (ulonglong)f_1;
      break;
    }
    case HA_KEYTYPE_LONGLONG:
      s_value = sint8korr(key);
      break;
    case HA_KEYTYPE_ULONGLONG:
      value = uint8korr(key);
      break;
    default:
      assert(0);
      value = 0; /* Error */
      break;
  }

  /*
    The following code works because if s_value < 0 then value is 0
    and if s_value == 0 then value will contain either s_value or the
    correct value.
  */
  info->s->auto_increment = std::max(
      info->s->auto_increment, (s_value > 0) ? ulonglong(s_value) : value);
}
