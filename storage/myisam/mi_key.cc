/* Copyright (c) 2000, 2022, Oracle and/or its affiliates.

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

/* Functions to handle keys */

#include <sys/types.h>
#include <algorithm>
#include <cmath>

#include "m_ctype.h"
#include "my_byteorder.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_macros.h"
#include "storage/myisam/myisamdef.h"
#include "storage/myisam/sp_defs.h"

#define FIX_LENGTH(cs, pos, length, char_length)                    \
  do {                                                              \
    if (length > char_length)                                       \
      char_length = my_charpos(cs, pos, pos + length, char_length); \
    char_length = std::min(char_length, length);                    \
  } while (0)

static int _mi_put_key_in_record(MI_INFO *info, uint keynr, bool unpack_blobs,
                                 uchar *record);

/*
  Make a intern key from a record

  SYNOPSIS
    _mi_make_key()
    info		MyiSAM handler
    keynr		key number
    key			Store created key here
    record		Record
    filepos		Position to record in the data file

  RETURN
    Length of key
*/

uint _mi_make_key(MI_INFO *info, uint keynr, uchar *key, const uchar *record,
                  my_off_t filepos) {
  const uchar *pos;
  uchar *start;
  HA_KEYSEG *keyseg;
  bool is_ft = info->s->keyinfo[keynr].flag & HA_FULLTEXT;
  DBUG_TRACE;

  if (info->s->keyinfo[keynr].flag & HA_SPATIAL) {
    /*
      TODO: nulls processing
    */
    return sp_make_key(info, keynr, key, record, filepos);
  }

  start = key;
  for (keyseg = info->s->keyinfo[keynr].seg; keyseg->type; keyseg++) {
    enum ha_base_keytype type = (enum ha_base_keytype)keyseg->type;
    uint length = keyseg->length;
    uint char_length;
    const CHARSET_INFO *cs = keyseg->charset;

    if (keyseg->null_bit) {
      if (record[keyseg->null_pos] & keyseg->null_bit) {
        *key++ = 0; /* NULL in key */
        continue;
      }
      *key++ = 1; /* Not NULL */
    }

    char_length =
        ((!is_ft && cs && cs->mbmaxlen > 1) ? length / cs->mbmaxlen : length);

    pos = record + keyseg->start;
    if (type == HA_KEYTYPE_BIT) {
      if (keyseg->bit_length) {
        uchar bits = get_rec_bits(record + keyseg->bit_pos, keyseg->bit_start,
                                  keyseg->bit_length);
        *key++ = bits;
        length--;
      }
      memcpy((uchar *)key, pos, length);
      key += length;
      continue;
    }
    if (keyseg->flag & HA_SPACE_PACK) {
      if (type != HA_KEYTYPE_NUM) {
        length =
            cs->cset->lengthsp(cs, pointer_cast<const char *>(pos), length);
      } else {
        const uchar *end = pos + length;
        while (pos < end && pos[0] == ' ') pos++;
        length = (uint)(end - pos);
      }
      FIX_LENGTH(cs, pos, length, char_length);
      store_key_length_inc(key, char_length);
      memcpy((uchar *)key, pos, (size_t)char_length);
      key += char_length;
      continue;
    }
    if (keyseg->flag & HA_VAR_LENGTH_PART) {
      uint pack_length = (keyseg->bit_start == 1 ? 1 : 2);
      uint tmp_length = (pack_length == 1 ? (uint)*pos : uint2korr(pos));
      pos += pack_length; /* Skip VARCHAR length */
      length = std::min(length, tmp_length);
      FIX_LENGTH(cs, pos, length, char_length);
      store_key_length_inc(key, char_length);
      memcpy((uchar *)key, pos, (size_t)char_length);
      key += char_length;
      continue;
    } else if (keyseg->flag & HA_BLOB_PART) {
      uint tmp_length = _mi_calc_blob_length(keyseg->bit_start, pos);
      memcpy(&pos, pos + keyseg->bit_start, sizeof(char *));
      length = std::min(length, tmp_length);
      FIX_LENGTH(cs, pos, length, char_length);
      store_key_length_inc(key, char_length);
      if (char_length > 0) memcpy((uchar *)key, pos, (size_t)char_length);
      key += char_length;
      continue;
    } else if (keyseg->flag & HA_SWAP_KEY) { /* Numerical column */
      if (type == HA_KEYTYPE_FLOAT) {
        float nr = float4get(pos);
        if (std::isnan(nr)) {
          /* Replace NAN with zero */
          memset(key, 0, length);
          key += length;
          continue;
        }
      } else if (type == HA_KEYTYPE_DOUBLE) {
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
    FIX_LENGTH(cs, pos, length, char_length);
    memcpy((uchar *)key, pos, char_length);
    if (length > char_length)
      cs->cset->fill(cs, (char *)key + char_length, length - char_length, ' ');
    key += length;
  }
  _mi_dpointer(info, key, filepos);
  DBUG_PRINT("exit", ("keynr: %d", keynr));
  DBUG_DUMP("key", (uchar *)start, (uint)(key - start) + keyseg->length);
  DBUG_EXECUTE("key", _mi_print_key(DBUG_FILE, info->s->keyinfo[keynr].seg,
                                    start, (uint)(key - start)););
  return (uint)(key - start); /* Return keylength */
} /* _mi_make_key */

/*
  Pack a key to intern format from given format (c_rkey)

  SYNOPSIS
    _mi_pack_key()
    info		MyISAM handler
    uint keynr		key number
    key			Store packed key here
    old			Not packed key
    keypart_map         bitmap of used keyparts
    last_used_keyseg	out parameter.  May be NULL

   RETURN
     length of packed key

     last_use_keyseg    Store pointer to the keyseg after the last used one
*/

uint _mi_pack_key(MI_INFO *info, uint keynr, uchar *key, const uchar *old,
                  key_part_map keypart_map, HA_KEYSEG **last_used_keyseg) {
  uchar *start_key = key;
  HA_KEYSEG *keyseg;
  bool is_ft = info->s->keyinfo[keynr].flag & HA_FULLTEXT;
  DBUG_TRACE;

  /* "one part" rtree key is 2*SPDIMS part key in MyISAM */
  if (info->s->keyinfo[keynr].key_alg == HA_KEY_ALG_RTREE)
    keypart_map = (((key_part_map)1) << (2 * SPDIMS)) - 1;

  /* only key prefixes are supported */
  assert(((keypart_map + 1) & keypart_map) == 0);

  for (keyseg = info->s->keyinfo[keynr].seg; keyseg->type && keypart_map;
       old += keyseg->length, keyseg++) {
    enum ha_base_keytype type = (enum ha_base_keytype)keyseg->type;
    uint length = keyseg->length;
    uint char_length;
    const uchar *pos;

    const CHARSET_INFO *cs = keyseg->charset;
    keypart_map >>= 1;
    if (keyseg->null_bit) {
      if (!(*key++ = (char)1 - *old++)) /* Copy null marker */
      {
        if (keyseg->flag & (HA_VAR_LENGTH_PART | HA_BLOB_PART)) old += 2;
        continue; /* Found NULL */
      }
    }
    char_length =
        (!is_ft && cs && cs->mbmaxlen > 1) ? length / cs->mbmaxlen : length;
    pos = old;
    if (keyseg->flag & HA_SPACE_PACK) {
      if (type == HA_KEYTYPE_NUM) {
        const uchar *end = pos + length;
        while (pos < end && pos[0] == ' ') pos++;
        length = (uint)(end - pos);
      } else if (type != HA_KEYTYPE_BINARY) {
        length =
            cs->cset->lengthsp(cs, pointer_cast<const char *>(pos), length);
      }
      FIX_LENGTH(cs, pos, length, char_length);
      store_key_length_inc(key, char_length);
      memcpy((uchar *)key, pos, (size_t)char_length);
      key += char_length;
      continue;
    } else if (keyseg->flag & (HA_VAR_LENGTH_PART | HA_BLOB_PART)) {
      /* Length of key-part used with mi_rkey() always 2 */
      uint tmp_length = uint2korr(pos);
      pos += 2;
      length = std::min(length, tmp_length); /* Safety */
      FIX_LENGTH(cs, pos, length, char_length);
      store_key_length_inc(key, char_length);
      old += 2; /* Skip length */
      memcpy((uchar *)key, pos, (size_t)char_length);
      key += char_length;
      continue;
    } else if (keyseg->flag & HA_SWAP_KEY) { /* Numerical column */
      pos += length;
      while (length--) *key++ = *--pos;
      continue;
    }
    FIX_LENGTH(cs, pos, length, char_length);
    memcpy((uchar *)key, pos, char_length);
    if (length > char_length)
      cs->cset->fill(cs, (char *)key + char_length, length - char_length, ' ');
    key += length;
  }
  if (last_used_keyseg) *last_used_keyseg = keyseg;

  return (uint)(key - start_key);
} /* _mi_pack_key */

/*
  Store found key in record

  SYNOPSIS
    _mi_put_key_in_record()
    info		MyISAM handler
    keynr		Key number that was used
    unpack_blobs        true  <=> Unpack blob columns
                        false <=> Skip them. This is used by index condition
                                  pushdown check function
    record 		Store key here

    Last read key is in info->lastkey

 NOTES
   Used when only-keyread is wanted

 RETURN
   0   ok
   1   error
*/

static int _mi_put_key_in_record(MI_INFO *info, uint keynr, bool unpack_blobs,
                                 uchar *record) {
  uchar *pos;
  HA_KEYSEG *keyseg;
  uchar *blob_ptr;
  DBUG_TRACE;

  blob_ptr = info->lastkey2;        /* Place to put blob parts */
  const uchar *key = info->lastkey; /* KEy that was read */
  const uchar *key_end = key + info->lastkey_length;
  for (keyseg = info->s->keyinfo[keynr].seg; keyseg->type; keyseg++) {
    if (keyseg->null_bit) {
      if (!*key++) {
        record[keyseg->null_pos] |= keyseg->null_bit;
        continue;
      }
      record[keyseg->null_pos] &= ~keyseg->null_bit;
    }
    if (keyseg->type == HA_KEYTYPE_BIT) {
      uint length = keyseg->length;

      if (keyseg->bit_length) {
        uchar bits = *key++;
        set_rec_bits(bits, record + keyseg->bit_pos, keyseg->bit_start,
                     keyseg->bit_length);
        length--;
      } else {
        clr_rec_bits(record + keyseg->bit_pos, keyseg->bit_start,
                     keyseg->bit_length);
      }
      memcpy(record + keyseg->start, key, length);
      key += length;
      continue;
    }
    if (keyseg->flag & HA_SPACE_PACK) {
      uint length = get_key_length(&key);
      if (length > keyseg->length || key + length > key_end) goto err;
      pos = record + keyseg->start;
      if (keyseg->type != (int)HA_KEYTYPE_NUM) {
        memcpy(pos, key, (size_t)length);
        keyseg->charset->cset->fill(keyseg->charset, (char *)pos + length,
                                    keyseg->length - length, ' ');
      } else {
        memset(pos, ' ', keyseg->length - length);
        memcpy(pos + keyseg->length - length, key, (size_t)length);
      }
      key += length;
      continue;
    }

    if (keyseg->flag & HA_VAR_LENGTH_PART) {
      uint length = get_key_length(&key);
      if (length > keyseg->length || key + length > key_end) goto err;
      /* Store key length */
      if (keyseg->bit_start == 1)
        *(uchar *)(record + keyseg->start) = (uchar)length;
      else
        int2store(record + keyseg->start, length);
      /* And key data */
      memcpy(record + keyseg->start + keyseg->bit_start, key, length);
      key += length;
    } else if (keyseg->flag & HA_BLOB_PART) {
      uint length = get_key_length(&key);
      if (length > keyseg->length || key + length > key_end) goto err;
      if (unpack_blobs) {
        memcpy(record + keyseg->start + keyseg->bit_start, &blob_ptr,
               sizeof(char *));
        memcpy(blob_ptr, key, length);
        blob_ptr += length;
        _mi_store_blob_length(record + keyseg->start, (uint)keyseg->bit_start,
                              length);
      }
      key += length;
    } else if (keyseg->flag & HA_SWAP_KEY) {
      uchar *to = record + keyseg->start + keyseg->length;
      const uchar *end = key + keyseg->length;
      if (end > key_end) goto err;
      do {
        *--to = *key++;
      } while (key != end);
      continue;
    } else {
      if (key + keyseg->length > key_end) goto err;
      memcpy(record + keyseg->start, key, (size_t)keyseg->length);
      key += keyseg->length;
    }
  }
  return 0;

err:
  return 1; /* Crashed row */
} /* _mi_put_key_in_record */

/* Here when key reads are used */

int _mi_read_key_record(MI_INFO *info, my_off_t filepos, uchar *buf) {
  fast_mi_writeinfo(info);
  if (filepos != HA_OFFSET_ERROR) {
    if (info->lastinx >= 0) { /* Read only key */
      if (_mi_put_key_in_record(info, (uint)info->lastinx, true, buf)) {
        mi_print_error(info->s, HA_ERR_CRASHED);
        set_my_errno(HA_ERR_CRASHED);
        return -1;
      }
      info->update |= HA_STATE_AKTIV; /* We should find a record */
      return 0;
    }
    set_my_errno(HA_ERR_WRONG_INDEX);
  }
  return (-1); /* Wrong data to read */
}

/*
  Save current key tuple to record and call index condition check function

  SYNOPSIS
    mi_check_index_cond()
      info    MyISAM handler
      keynr   Index we're running a scan on
      record  Record buffer to use (it is assumed that index check function
              will look for column values there)

  RETURN
    -1  Error
    0   Index condition is not satisfied, continue scanning
    1   Index condition is satisfied
    2   Index condition is not satisfied, end the scan.
*/

int mi_check_index_cond(MI_INFO *info, uint keynr, uchar *record) {
  if (_mi_put_key_in_record(info, keynr, false, record)) {
    mi_print_error(info->s, HA_ERR_CRASHED);
    set_my_errno(HA_ERR_CRASHED);
    return -1;
  }
  return info->index_cond_func(info->index_cond_func_arg);
}

/*
  Retrieve auto_increment info

  SYNOPSIS
    retrieve_auto_increment()
    info			MyISAM handler
    record			Row to update

  IMPLEMENTATION
    For signed columns we don't retrieve the auto increment value if it's
    less than zero.
*/

ulonglong retrieve_auto_increment(MI_INFO *info, const uchar *record) {
  ulonglong value = 0;  /* Store unsigned values here */
  longlong s_value = 0; /* Store signed values here */
  HA_KEYSEG *keyseg = info->s->keyinfo[info->s->base.auto_key - 1].seg;
  const uchar *key = record + keyseg->start;

  switch (keyseg->type) {
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
  return (s_value > 0) ? (ulonglong)s_value : value;
}
