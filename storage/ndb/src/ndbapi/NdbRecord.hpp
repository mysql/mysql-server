/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef NdbRecord_H
#define NdbRecord_H

class NdbRecord {
public:
  /* Flag bits for the entire NdbRecord. */
  enum RecFlags
  {
    /*
      This flag tells whether this NdbRecord is a PK record for the table,
      ie. that it describes _exactly_ the primary key attributes, no more and
      no less.
    */
    RecIsKeyRecord= 0x1,

    /*
      This flag tells whether this NdbRecord includes _at least_ all PK columns
      (and possibly other columns). This is a requirement for many key-based
      operations.
    */
    RecHasAllKeys= 0x2,

    /* This NdbRecord is for an ordered index, not a table. */
    RecIsIndex= 0x4,

    /* This NdbRecord has at least one blob. */
    RecHasBlob= 0x8,

    /*
      The table has at least one blob (though the NdbRecord may not include
      it). This is needed so that deleteTuple() can know to delete all blob
      parts.
    */
    RecTableHasBlob= 0x10
  };

  /* Flag bits for individual columns in the NdbRecord. */
  enum ColFlags
  {
    /*
      This flag tells whether the column is part of the primary key, used
      for insert.
    */
    IsKey=   0x1,
    /* This flag is true if column is disk based. */
    IsDisk= 0x2,
    /* True if column can be NULL and has a NULL bit. */
    IsNullable= 0x04,
    /*
      Flags for determining the actual length of data (which for varsize
      columns is different from the maximum size.
      The flags are mutually exclusive.
    */
    IsVar1ByteLen= 0x08,
    IsVar2ByteLen= 0x10,
    /* Flag for column that is a part of the distribution key. */
    IsDistributionKey= 0x20,
    /* Flag for blob columns. */
    IsBlob= 0x40,
    /* 
       Flag for special handling of short varchar for index keys, which is
       used by mysqld to avoid converting index key rows.
    */
    IsMysqldShrinkVarchar= 0x80,
    /* Bitfield stored in the internal mysqld format. */
    IsMysqldBitfield= 0x100
  };

  struct Attr
  {
    Uint32 attrId;
    Uint32 column_no;
    /*
      The index_attrId member is the attribute id in the index table object,
      which is used to specify ordered index bounds in KEYINFO signal.
      Note that this is different from the normal attribute id in the main
      table, unless the ordered index is on columns (0..N).
    */
    Uint32 index_attrId;
    /* Offset of data from the start of a row. */
    Uint32 offset;
    /*
      Maximum size of the attribute. This is duplicated here to avoid having
      to dig into Table object for every attribute fetch/store.
    */
    Uint32 maxSize;
    /* Number of bits in a bitfield. */
    Uint32 bitCount;

    /* Flags, or-ed from enum ColFlags. */
    Uint32 flags;

    /* Character set information, for ordered index merge sort. */
    CHARSET_INFO *charset_info;
    /* Function used to compare attributes during merge sort. */
    NdbSqlUtil::Cmp *compare_function;


    /* NULL bit location (only for nullable columns, ie. flags&IsNullable). */
    Uint32 nullbit_byte_offset;
    Uint32 nullbit_bit_in_byte;

    bool get_var_length(const char *row, Uint32& len) const
    {
      if (flags & IsVar1ByteLen)
        len= 1 + *((Uint8*)(row+offset));
      else if (flags & IsVar2ByteLen)
        len= 2 + uint2korr(row+offset);
      else
        len= maxSize;
      return len <= maxSize;
    }
    bool is_null(const char *row) const
    {
      return (flags & IsNullable) &&
             (row[nullbit_byte_offset] & (1 << nullbit_bit_in_byte));
    }
    /*
      Mysqld uses a slightly different format for storing varchar in
      index keys; the length is always two bytes little endian, even
      for max size < 256.
      This converts to the usual format expected by NDB kernel.
    */
    bool shrink_varchar(const char *row, Uint32& out_len, char *buf) const
    {
      const char *p= row + offset;
      Uint32 len= uint2korr(p);
      if (len >= 256 || len >= maxSize)
        return false;
      buf[0]= (unsigned char)len;
      memcpy(buf+1, p+2, len);
      out_len= len + 1;
      return true;
    }
    /*
      Accessing mysqld format bitfields.
      For internal use in myqsld.
      In mysqld, fractional bytes of each bit field are stored inside the
      null bytes area.
    */
    void get_mysqld_bitfield(const char *src_row, char *dst_buffer) const;
    void put_mysqld_bitfield(char *dst_row, const char *src_buffer) const;
  };

  /*
    ToDo: For now we need to hang on to the Table *, since lots of the
    existing code (class NdbOperation*, class NdbScanFilter) depends
    on having access to it.
    Long-term, we want to eliminate it (instead relying only on copying
    tableId, fragmentCount etc. into the NdbRecord.
  */
  const NdbTableImpl *table;
  const NdbTableImpl *base_table;

  Uint32 tableId;
  Uint32 tableVersion;
  /* Copy of table->m_keyLenInWords. */
  Uint32 m_keyLenInWords;
  /* Total maximum size of TRANSID_AI data (for computing batch size). */
  Uint32 m_max_transid_ai_bytes;
  /* Number of distribution keys (usually == number of primary keys). */
  Uint32 m_no_of_distribution_keys;
  /* Flags, or-ed from enum RecFlags. */
  Uint32 flags;
  /* Size of row (really end of right-most defined attribute in row). */
  Uint32 m_row_size;

  /*
    Array of index (into columns[]) of primary key columns, in order.
    Physical storage for these is after columns[] array.
    This array is only fully initialised if flags&RecHasAllKeys.
  */
  const Uint32 *key_indexes;
  /* Length of key_indexes array. */
  Uint32 key_index_length;
  /*
    Array of index (into columns[]) of distribution keys, in attrId order.
    This is used to build the distribution key, which is the concatenation
    of key values in attrId order.
  */
  const Uint32 *distkey_indexes;
  /* Length of distkey_indexes array. */
  Uint32 distkey_index_length;

  /*
    m_min_distkey_prefix_length is the minimum lenght of an index prefix
    needed to include all distribution keys. In other words, it is one more
    that the index of the last distribution key in the index order.
    If the index does not include all distribution keys, it is set to 0.
    This member is only valid for an index NdbRecord.
  */
  Uint32 m_min_distkey_prefix_length;
  /* The real size of the array at the end of this struct. */
  Uint32 noOfColumns;
  struct Attr columns[1];

  /* Copy a user-supplied mask to internal mask. */
  void copyMask(Uint32 *dst, const unsigned char *src) const;

  /* Clear internal mask. */
  void clearMask(Uint32 *dst) const
  {
    BitmaskImpl::clear((NDB_MAX_ATTRIBUTES_IN_TABLE+31)>>5, dst);
  }
};

#endif
