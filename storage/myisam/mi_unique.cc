/* Copyright (c) 2000, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/* Functions to check if a row is unique */

#include <sys/types.h>

#include "m_ctype.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_macros.h"
#include "storage/myisam/myisamdef.h"

bool mi_check_unique(MI_INFO *info, MI_UNIQUEDEF *def, uchar *record,
                     ha_checksum unique_hash, my_off_t disk_pos) {
  my_off_t lastpos = info->lastpos;
  MI_KEYDEF *key = &info->s->keyinfo[def->key];
  uchar *key_buff = info->lastkey2;
  DBUG_ENTER("mi_check_unique");

  mi_unique_store(record + key->seg->start, unique_hash);
  _mi_make_key(info, def->key, key_buff, record, 0);

  if (_mi_search(info, info->s->keyinfo + def->key, key_buff,
                 MI_UNIQUE_HASH_LENGTH, SEARCH_FIND,
                 info->s->state.key_root[def->key])) {
    info->page_changed = 1; /* Can't optimize read next */
    info->lastpos = lastpos;
    DBUG_RETURN(0); /* No matching rows */
  }

  for (;;) {
    if (info->lastpos != disk_pos &&
        !(*info->s->compare_unique)(info, def, record, info->lastpos)) {
      set_my_errno(HA_ERR_FOUND_DUPP_UNIQUE);
      info->errkey = (int)def->key;
      info->dupp_key_pos = info->lastpos;
      info->page_changed = 1; /* Can't optimize read next */
      info->lastpos = lastpos;
      DBUG_PRINT("info", ("Found duplicate"));
      DBUG_RETURN(1); /* Found identical  */
    }
    if (_mi_search_next(info, info->s->keyinfo + def->key, info->lastkey,
                        MI_UNIQUE_HASH_LENGTH, SEARCH_BIGGER,
                        info->s->state.key_root[def->key]) ||
        memcmp(info->lastkey, key_buff, MI_UNIQUE_HASH_LENGTH)) {
      info->page_changed = 1; /* Can't optimize read next */
      info->lastpos = lastpos;
      DBUG_RETURN(0); /* end of tree */
    }
  }
}

/*
  Calculate a hash for a row

  TODO
    Add support for bit fields
*/

ha_checksum mi_unique_hash(MI_UNIQUEDEF *def, const uchar *record) {
  uchar *pos;
  const uchar *end;
  ha_checksum crc = 0;
  ulong seed1 = 0, seed2 = 4;
  HA_KEYSEG *keyseg;

  for (keyseg = def->seg; keyseg < def->end; keyseg++) {
    enum ha_base_keytype type = (enum ha_base_keytype)keyseg->type;
    uint length = keyseg->length;

    if (keyseg->null_bit) {
      if (record[keyseg->null_pos] & keyseg->null_bit) {
        /*
          Change crc in a way different from an empty string or 0.
          (This is an optimisation;  The code will work even if this isn't
          done)
        */
        crc = ((crc << 8) + 511 + (crc >> (8 * sizeof(ha_checksum) - 8)));
        continue;
      }
    }
    pos = (uchar *)record + keyseg->start;
    if (keyseg->flag & HA_VAR_LENGTH_PART) {
      uint pack_length = keyseg->bit_start;
      uint tmp_length = (pack_length == 1 ? (uint)*pos : uint2korr(pos));
      pos += pack_length; /* Skip VARCHAR length */
      set_if_smaller(length, tmp_length);
    } else if (keyseg->flag & HA_BLOB_PART) {
      uint tmp_length = _mi_calc_blob_length(keyseg->bit_start, pos);
      memcpy(&pos, pos + keyseg->bit_start, sizeof(char *));
      if (!length || length > tmp_length)
        length = tmp_length; /* The whole blob */
    }
    end = pos + length;
    if (type == HA_KEYTYPE_TEXT || type == HA_KEYTYPE_VARTEXT1 ||
        type == HA_KEYTYPE_VARTEXT2) {
      keyseg->charset->coll->hash_sort(keyseg->charset, (const uchar *)pos,
                                       length, &seed1, &seed2);
      crc ^= seed1;
    } else
      while (pos != end)
        crc = ((crc << 8) + (((uchar) * (uchar *)pos++))) +
              (crc >> (8 * sizeof(ha_checksum) - 8));
  }
  return crc;
}

/*
  compare unique key for two rows

  TODO
    Add support for bit fields

  RETURN
    0   if both rows have equal unique value
    #   Rows are different
*/

int mi_unique_comp(MI_UNIQUEDEF *def, const uchar *a, const uchar *b,
                   bool null_are_equal) {
  uchar *pos_a, *pos_b;
  const uchar *end;
  HA_KEYSEG *keyseg;

  for (keyseg = def->seg; keyseg < def->end; keyseg++) {
    enum ha_base_keytype type = (enum ha_base_keytype)keyseg->type;
    uint a_length, b_length;
    a_length = b_length = keyseg->length;

    /* If part is NULL it's regarded as different */
    if (keyseg->null_bit) {
      uint tmp;
      if ((tmp = (a[keyseg->null_pos] & keyseg->null_bit)) !=
          (uint)(b[keyseg->null_pos] & keyseg->null_bit))
        return 1;
      if (tmp) {
        if (!null_are_equal) return 1;
        continue;
      }
    }
    pos_a = (uchar *)a + keyseg->start;
    pos_b = (uchar *)b + keyseg->start;
    if (keyseg->flag & HA_VAR_LENGTH_PART) {
      uint pack_length = keyseg->bit_start;
      if (pack_length == 1) {
        a_length = (uint)*pos_a++;
        b_length = (uint)*pos_b++;
      } else {
        a_length = uint2korr(pos_a);
        b_length = uint2korr(pos_b);
        pos_a += 2; /* Skip VARCHAR length */
        pos_b += 2;
      }
      set_if_smaller(a_length, keyseg->length); /* Safety */
      set_if_smaller(b_length, keyseg->length); /* safety */
    } else if (keyseg->flag & HA_BLOB_PART) {
      /* Only compare 'length' characters if length != 0 */
      a_length = _mi_calc_blob_length(keyseg->bit_start, pos_a);
      b_length = _mi_calc_blob_length(keyseg->bit_start, pos_b);
      /* Check that a and b are of equal length */
      if (keyseg->length) {
        /*
          This is used in some cases when we are not interested in comparing
          the whole length of the blob.
        */
        set_if_smaller(a_length, keyseg->length);
        set_if_smaller(b_length, keyseg->length);
      }
      memcpy(&pos_a, pos_a + keyseg->bit_start, sizeof(char *));
      memcpy(&pos_b, pos_b + keyseg->bit_start, sizeof(char *));
    }
    if (type == HA_KEYTYPE_TEXT || type == HA_KEYTYPE_VARTEXT1 ||
        type == HA_KEYTYPE_VARTEXT2) {
      if (ha_compare_text(keyseg->charset, pos_a, a_length, pos_b, b_length, 0))
        return 1;
    } else {
      if (a_length != b_length) return 1;
      end = pos_a + a_length;
      while (pos_a != end) {
        if (*pos_a++ != *pos_b++) return 1;
      }
    }
  }
  return 0;
}
