/* Copyright (c) 2000, 2010 Oracle and/or its affiliates. All rights reserved.

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

/*
  Because of the function new_field() all field classes that have static
  variables must declare the size_of() member function.
*/

#ifdef USE_PRAGMA_INTERFACE
#pragma interface			/* gcc class implementation */
#endif

#define NOT_FIXED_DEC			31
#define DATETIME_DEC                     6
const uint32 max_field_size= (uint32) 4294967295U;

class Send_field;
class Protocol;
class Create_field;
class Relay_log_info;

struct st_cache_field;
int field_conv(Field *to,Field *from);

inline uint get_enum_pack_length(int elements)
{
  return elements < 256 ? 1 : 2;
}

inline uint get_set_pack_length(int elements)
{
  uint len= (elements + 7) / 8;
  return len > 4 ? 8 : len;
}

class Field
{
  Field(const Item &);				/* Prevent use of these */
  void operator=(Field &);
public:
  static void *operator new(size_t size) throw ()
  { return sql_alloc(size); }
  static void operator delete(void *ptr_arg, size_t size) { TRASH(ptr_arg, size); }

  uchar		*ptr;			// Position to field in record
  /**
     Byte where the @c NULL bit is stored inside a record. If this Field is a
     @c NOT @c NULL field, this member is @c NULL.
  */
  uchar		*null_ptr;
  /*
    Note that you can use table->in_use as replacement for current_thd member 
    only inside of val_*() and store() members (e.g. you can't use it in cons)
  */
  struct st_table *table;		// Pointer for table
  struct st_table *orig_table;		// Pointer to original table
  const char	**table_name, *field_name;
  LEX_STRING	comment;
  /* Field is part of the following keys */
  key_map	key_start, part_of_key, part_of_key_not_clustered;
  key_map       part_of_sortkey;
  /* 
    We use three additional unireg types for TIMESTAMP to overcome limitation 
    of current binary format of .frm file. We'd like to be able to support 
    NOW() as default and on update value for such fields but unable to hold 
    this info anywhere except unireg_check field. This issue will be resolved
    in more clean way with transition to new text based .frm format.
    See also comment for Field_timestamp::Field_timestamp().
  */
  enum utype  { NONE,DATE,SHIELD,NOEMPTY,CASEUP,PNR,BGNR,PGNR,YES,NO,REL,
		CHECK,EMPTY,UNKNOWN_FIELD,CASEDN,NEXT_NUMBER,INTERVAL_FIELD,
                BIT_FIELD, TIMESTAMP_OLD_FIELD, CAPITALIZE, BLOB_FIELD,
                TIMESTAMP_DN_FIELD, TIMESTAMP_UN_FIELD, TIMESTAMP_DNUN_FIELD};
  enum geometry_type
  {
    GEOM_GEOMETRY = 0, GEOM_POINT = 1, GEOM_LINESTRING = 2, GEOM_POLYGON = 3,
    GEOM_MULTIPOINT = 4, GEOM_MULTILINESTRING = 5, GEOM_MULTIPOLYGON = 6,
    GEOM_GEOMETRYCOLLECTION = 7
  };
  enum imagetype { itRAW, itMBR};

  utype		unireg_check;
  uint32	field_length;		// Length of field
  uint32	flags;
  uint16        field_index;            // field number in fields array
  uchar		null_bit;		// Bit used to test null bit
  /**
     If true, this field was created in create_tmp_field_from_item from a NULL
     value. This means that the type of the field is just a guess, and the type
     may be freely coerced to another type.

     @see create_tmp_field_from_item
     @see Item_type_holder::get_real_type

   */
  bool is_created_from_null_item;

  Field(uchar *ptr_arg,uint32 length_arg,uchar *null_ptr_arg,
        uchar null_bit_arg, utype unireg_check_arg,
        const char *field_name_arg);
  virtual ~Field() {}
  /* Store functions returns 1 on overflow and -1 on fatal error */
  virtual int  store(const char *to, uint length,CHARSET_INFO *cs)=0;
  virtual int  store(double nr)=0;
  virtual int  store(longlong nr, bool unsigned_val)=0;
  virtual int  store_decimal(const my_decimal *d)=0;
  virtual int store_time(MYSQL_TIME *ltime, timestamp_type t_type);
  int store(const char *to, uint length, CHARSET_INFO *cs,
            enum_check_fields check_level);
  virtual double val_real(void)=0;
  virtual longlong val_int(void)=0;
  virtual my_decimal *val_decimal(my_decimal *);
  inline String *val_str(String *str) { return val_str(str, str); }
  /*
     val_str(buf1, buf2) gets two buffers and should use them as follows:
     if it needs a temp buffer to convert result to string - use buf1
       example Field_tiny::val_str()
     if the value exists as a string already - use buf2
       example Field_string::val_str()
     consequently, buf2 may be created as 'String buf;' - no memory
     will be allocated for it. buf1 will be allocated to hold a
     value if it's too small. Using allocated buffer for buf2 may result in
     an unnecessary free (and later, may be an alloc).
     This trickery is used to decrease a number of malloc calls.
  */
  virtual String *val_str(String*,String *)=0;
  String *val_int_as_str(String *val_buffer, my_bool unsigned_flag);
  /*
   str_needs_quotes() returns TRUE if the value returned by val_str() needs
   to be quoted when used in constructing an SQL query.
  */
  virtual bool str_needs_quotes() { return FALSE; }
  virtual Item_result result_type () const=0;
  virtual Item_result cmp_type () const { return result_type(); }
  virtual Item_result cast_to_int_type () const { return result_type(); }
  static bool type_can_have_key_part(enum_field_types);
  static enum_field_types field_type_merge(enum_field_types, enum_field_types);
  static Item_result result_merge_type(enum_field_types);
  virtual bool eq(Field *field)
  {
    return (ptr == field->ptr && null_ptr == field->null_ptr &&
            null_bit == field->null_bit && field->type() == type());
  }
  virtual bool eq_def(Field *field);
  
  /*
    pack_length() returns size (in bytes) used to store field data in memory
    (i.e. it returns the maximum size of the field in a row of the table,
    which is located in RAM).
  */
  virtual uint32 pack_length() const { return (uint32) field_length; }

  /*
    pack_length_in_rec() returns size (in bytes) used to store field data on
    storage (i.e. it returns the maximal size of the field in a row of the
    table, which is located on disk).
  */
  virtual uint32 pack_length_in_rec() const { return pack_length(); }
  virtual int compatible_field_size(uint field_metadata,
                                    const Relay_log_info *, uint16 mflags);
  virtual uint pack_length_from_metadata(uint field_metadata)
  { return field_metadata; }
  /*
    This method is used to return the size of the data in a row-based
    replication row record. The default implementation of returning 0 is
    designed to allow fields that do not use metadata to return TRUE (1)
    from compatible_field_size() which uses this function in the comparison.
    The default value for field metadata for fields that do not have 
    metadata is 0. Thus, 0 == 0 means the fields are compatible in size.

    Note: While most classes that override this method return pack_length(),
    the classes Field_string, Field_varstring, and Field_blob return 
    field_length + 1, field_length, and pack_length_no_ptr() respectfully.
  */
  virtual uint row_pack_length() { return 0; }
  virtual int save_field_metadata(uchar *first_byte)
  { return do_save_field_metadata(first_byte); }

  /*
    data_length() return the "real size" of the data in memory.
  */
  virtual uint32 data_length() { return pack_length(); }
  virtual uint32 sort_length() const { return pack_length(); }

  /**
     Get the maximum size of the data in packed format.

     @return Maximum data length of the field when packed using the
     Field::pack() function.
   */
  virtual uint32 max_data_length() const {
    return pack_length();
  };

  virtual int reset(void) { bzero(ptr,pack_length()); return 0; }
  virtual void reset_fields() {}
  virtual void set_default()
  {
    my_ptrdiff_t l_offset= (my_ptrdiff_t) (table->s->default_values -
					  table->record[0]);
    memcpy(ptr, ptr + l_offset, pack_length());
    if (null_ptr)
      *null_ptr= ((*null_ptr & (uchar) ~null_bit) |
		  (null_ptr[l_offset] & null_bit));
  }
  virtual bool binary() const { return 1; }
  virtual bool zero_pack() const { return 1; }
  virtual enum ha_base_keytype key_type() const { return HA_KEYTYPE_BINARY; }
  virtual uint32 key_length() const { return pack_length(); }
  virtual enum_field_types type() const =0;
  virtual enum_field_types real_type() const { return type(); }
  inline  int cmp(const uchar *str) { return cmp(ptr,str); }
  virtual int cmp_max(const uchar *a, const uchar *b, uint max_len)
    { return cmp(a, b); }
  virtual int cmp(const uchar *,const uchar *)=0;
  virtual int cmp_binary(const uchar *a,const uchar *b, uint32 max_length=~0L)
  { return memcmp(a,b,pack_length()); }
  virtual int cmp_offset(uint row_offset)
  { return cmp(ptr,ptr+row_offset); }
  virtual int cmp_binary_offset(uint row_offset)
  { return cmp_binary(ptr, ptr+row_offset); };
  virtual int key_cmp(const uchar *a,const uchar *b)
  { return cmp(a, b); }
  virtual int key_cmp(const uchar *str, uint length)
  { return cmp(ptr,str); }
  virtual uint decimals() const { return 0; }
  /*
    Caller beware: sql_type can change str.Ptr, so check
    ptr() to see if it changed if you are using your own buffer
    in str and restore it with set() if needed
  */
  virtual void sql_type(String &str) const =0;
  virtual uint size_of() const =0;		// For new field
  inline bool is_null(my_ptrdiff_t row_offset= 0)
  { return null_ptr ? (null_ptr[row_offset] & null_bit ? 1 : 0) : table->null_row; }
  inline bool is_real_null(my_ptrdiff_t row_offset= 0)
    { return null_ptr ? (null_ptr[row_offset] & null_bit ? 1 : 0) : 0; }
  inline bool is_null_in_record(const uchar *record)
  {
    if (!null_ptr)
      return 0;
    return test(record[(uint) (null_ptr -table->record[0])] &
		null_bit);
  }
  inline bool is_null_in_record_with_offset(my_ptrdiff_t offset)
  {
    if (!null_ptr)
      return 0;
    return test(null_ptr[offset] & null_bit);
  }
  inline void set_null(my_ptrdiff_t row_offset= 0)
    { if (null_ptr) null_ptr[row_offset]|= null_bit; }
  inline void set_notnull(my_ptrdiff_t row_offset= 0)
    { if (null_ptr) null_ptr[row_offset]&= (uchar) ~null_bit; }
  inline bool maybe_null(void) { return null_ptr != 0 || table->maybe_null; }
  /**
     Signals that this field is NULL-able.
  */
  inline bool real_maybe_null(void) { return null_ptr != 0; }

  enum {
    LAST_NULL_BYTE_UNDEF= 0
  };

  /*
    Find the position of the last null byte for the field.

    SYNOPSIS
      last_null_byte()

    DESCRIPTION
      Return a pointer to the last byte of the null bytes where the
      field conceptually is placed.

    RETURN VALUE
      The position of the last null byte relative to the beginning of
      the record. If the field does not use any bits of the null
      bytes, the value 0 (LAST_NULL_BYTE_UNDEF) is returned.
   */
  size_t last_null_byte() const {
    size_t bytes= do_last_null_byte();
    DBUG_PRINT("debug", ("last_null_byte() ==> %ld", (long) bytes));
    DBUG_ASSERT(bytes <= table->s->null_bytes);
    return bytes;
  }

  virtual void make_field(Send_field *);
  virtual void sort_string(uchar *buff,uint length)=0;
  virtual bool optimize_range(uint idx, uint part);
  /*
    This should be true for fields which, when compared with constant
    items, can be casted to longlong. In this case we will at 'fix_fields'
    stage cast the constant items to longlongs and at the execution stage
    use field->val_int() for comparison.  Used to optimize clauses like
    'a_column BETWEEN date_const, date_const'.
  */
  virtual bool can_be_compared_as_longlong() const { return FALSE; }
  virtual void free() {}
  virtual Field *new_field(MEM_ROOT *root, struct st_table *new_table,
                           bool keep_type);
  virtual Field *new_key_field(MEM_ROOT *root, struct st_table *new_table,
                               uchar *new_ptr, uchar *new_null_ptr,
                               uint new_null_bit);
  Field *clone(MEM_ROOT *mem_root, struct st_table *new_table);
  inline void move_field(uchar *ptr_arg,uchar *null_ptr_arg,uchar null_bit_arg)
  {
    ptr=ptr_arg; null_ptr=null_ptr_arg; null_bit=null_bit_arg;
  }
  inline void move_field(uchar *ptr_arg) { ptr=ptr_arg; }
  virtual void move_field_offset(my_ptrdiff_t ptr_diff)
  {
    ptr=ADD_TO_PTR(ptr,ptr_diff, uchar*);
    if (null_ptr)
      null_ptr=ADD_TO_PTR(null_ptr,ptr_diff,uchar*);
  }
  virtual void get_image(uchar *buff, uint length, CHARSET_INFO *cs)
    { memcpy(buff,ptr,length); }
  virtual void set_image(const uchar *buff,uint length, CHARSET_INFO *cs)
    { memcpy(ptr,buff,length); }


  /*
    Copy a field part into an output buffer.

    SYNOPSIS
      Field::get_key_image()
      buff   [out] output buffer
      length       output buffer size
      type         itMBR for geometry blobs, otherwise itRAW

    DESCRIPTION
      This function makes a copy of field part of size equal to or
      less than "length" parameter value.
      For fields of string types (CHAR, VARCHAR, TEXT) the rest of buffer
      is padded by zero byte.

    NOTES
      For variable length character fields (i.e. UTF-8) the "length"
      parameter means a number of output buffer bytes as if all field
      characters have maximal possible size (mbmaxlen). In the other words,
      "length" parameter is a number of characters multiplied by
      field_charset->mbmaxlen.

    RETURN
      Number of copied bytes (excluding padded zero bytes -- see above).
  */

  virtual uint get_key_image(uchar *buff, uint length, imagetype type)
  {
    get_image(buff, length, &my_charset_bin);
    return length;
  }
  virtual void set_key_image(const uchar *buff,uint length)
    { set_image(buff,length, &my_charset_bin); }
  inline longlong val_int_offset(uint row_offset)
    {
      ptr+=row_offset;
      longlong tmp=val_int();
      ptr-=row_offset;
      return tmp;
    }
  inline longlong val_int(const uchar *new_ptr)
  {
    uchar *old_ptr= ptr;
    longlong return_value;
    ptr= (uchar*) new_ptr;
    return_value= val_int();
    ptr= old_ptr;
    return return_value;
  }
  inline String *val_str(String *str, const uchar *new_ptr)
  {
    uchar *old_ptr= ptr;
    ptr= (uchar*) new_ptr;
    val_str(str);
    ptr= old_ptr;
    return str;
  }
  virtual bool send_binary(Protocol *protocol);

  virtual uchar *pack(uchar *to, const uchar *from,
                      uint max_length, bool low_byte_first);
  /**
     @overload Field::pack(uchar*, const uchar*, uint, bool)
  */
  uchar *pack(uchar *to, const uchar *from)
  {
    DBUG_ENTER("Field::pack");
    uchar *result= this->pack(to, from, UINT_MAX, table->s->db_low_byte_first);
    DBUG_RETURN(result);
  }

  virtual const uchar *unpack(uchar* to, const uchar *from,
                              uint param_data, bool low_byte_first);
  /**
     @overload Field::unpack(uchar*, const uchar*, uint, bool)
  */
  const uchar *unpack(uchar* to, const uchar *from)
  {
    DBUG_ENTER("Field::unpack");
    const uchar *result= unpack(to, from, 0U, table->s->db_low_byte_first);
    DBUG_RETURN(result);
  }

  virtual uchar *pack_key(uchar* to, const uchar *from,
                          uint max_length, bool low_byte_first)
  {
    return pack(to, from, max_length, low_byte_first);
  }
  virtual uchar *pack_key_from_key_image(uchar* to, const uchar *from,
					uint max_length, bool low_byte_first)
  {
    return pack(to, from, max_length, low_byte_first);
  }
  virtual const uchar *unpack_key(uchar* to, const uchar *from,
                                  uint max_length, bool low_byte_first)
  {
    return unpack(to, from, max_length, low_byte_first);
  }
  virtual uint packed_col_length(const uchar *to, uint length)
  { return length;}
  virtual uint max_packed_col_length(uint max_length)
  { return max_length;}

  virtual int pack_cmp(const uchar *a,const uchar *b, uint key_length_arg,
                       my_bool insert_or_update)
  { return cmp(a,b); }
  virtual int pack_cmp(const uchar *b, uint key_length_arg,
                       my_bool insert_or_update)
  { return cmp(ptr,b); }
  uint offset(uchar *record)
  {
    return (uint) (ptr - record);
  }
  void copy_from_tmp(int offset);
  uint fill_cache_field(struct st_cache_field *copy);
  virtual bool get_date(MYSQL_TIME *ltime,uint fuzzydate);
  virtual bool get_time(MYSQL_TIME *ltime);
  virtual CHARSET_INFO *charset(void) const { return &my_charset_bin; }
  virtual CHARSET_INFO *sort_charset(void) const { return charset(); }
  virtual bool has_charset(void) const { return FALSE; }
  virtual void set_charset(CHARSET_INFO *charset_arg) { }
  virtual enum Derivation derivation(void) const
  { return DERIVATION_IMPLICIT; }
  virtual void set_derivation(enum Derivation derivation_arg) { }
  bool set_warning(MYSQL_ERROR::enum_warning_level, unsigned int code,
                   int cuted_increment);
  void set_datetime_warning(MYSQL_ERROR::enum_warning_level, uint code, 
                            const char *str, uint str_len,
                            timestamp_type ts_type, int cuted_increment);
  void set_datetime_warning(MYSQL_ERROR::enum_warning_level, uint code, 
                            longlong nr, timestamp_type ts_type,
                            int cuted_increment);
  void set_datetime_warning(MYSQL_ERROR::enum_warning_level, const uint code, 
                            double nr, timestamp_type ts_type);
  inline bool check_overflow(int op_result)
  {
    return (op_result == E_DEC_OVERFLOW);
  }
  int warn_if_overflow(int op_result);
  void init(TABLE *table_arg)
  {
    orig_table= table= table_arg;
    table_name= &table_arg->alias;
  }

  /* maximum possible display length */
  virtual uint32 max_display_length()= 0;

  /**
    Whether a field being created is compatible with a existing one.

    Used by the ALTER TABLE code to evaluate whether the new definition
    of a table is compatible with the old definition so that it can
    determine if data needs to be copied over (table data change).
  */
  virtual uint is_equal(Create_field *new_field);
  /* convert decimal to longlong with overflow check */
  longlong convert_decimal2longlong(const my_decimal *val, bool unsigned_flag,
                                    int *err);
  /* The max. number of characters */
  inline uint32 char_length() const
  {
    return field_length / charset()->mbmaxlen;
  }

  virtual geometry_type get_geometry_type()
  {
    /* shouldn't get here. */
    DBUG_ASSERT(0);
    return GEOM_GEOMETRY;
  }
  /* Hash value */
  virtual void hash(ulong *nr, ulong *nr2);
  friend bool reopen_table(THD *,struct st_table *,bool);
  friend int cre_myisam(char * name, register TABLE *form, uint options,
			ulonglong auto_increment_value);
  friend class Copy_field;
  friend class Item_avg_field;
  friend class Item_std_field;
  friend class Item_sum_num;
  friend class Item_sum_sum;
  friend class Item_sum_str;
  friend class Item_sum_count;
  friend class Item_sum_avg;
  friend class Item_sum_std;
  friend class Item_sum_min;
  friend class Item_sum_max;
  friend class Item_func_group_concat;

private:
  /*
    Primitive for implementing last_null_byte().

    SYNOPSIS
      do_last_null_byte()

    DESCRIPTION
      Primitive for the implementation of the last_null_byte()
      function. This represents the inheritance interface and can be
      overridden by subclasses.
   */
  virtual size_t do_last_null_byte() const;

/**
   Retrieve the field metadata for fields.

   This default implementation returns 0 and saves 0 in the metadata_ptr
   value.

   @param   metadata_ptr   First byte of field metadata

   @returns 0 no bytes written.
*/
  virtual int do_save_field_metadata(uchar *metadata_ptr)
  { return 0; }

protected:
  /*
    Helper function to pack()/unpack() int32 values
  */
  static void handle_int32(uchar *to, const uchar *from,
                           bool low_byte_first_from, bool low_byte_first_to)
  {
    int32 val;
#ifdef WORDS_BIGENDIAN
    if (low_byte_first_from)
      val = sint4korr(from);
    else
#endif
      longget(val, from);

#ifdef WORDS_BIGENDIAN
    if (low_byte_first_to)
      int4store(to, val);
    else
#endif
      longstore(to, val);
  }

  /*
    Helper function to pack()/unpack() int64 values
  */
  static void handle_int64(uchar* to, const uchar *from,
                           bool low_byte_first_from, bool low_byte_first_to)
  {
    int64 val;
#ifdef WORDS_BIGENDIAN
    if (low_byte_first_from)
      val = sint8korr(from);
    else
#endif
      longlongget(val, from);

#ifdef WORDS_BIGENDIAN
    if (low_byte_first_to)
      int8store(to, val);
    else
#endif
      longlongstore(to, val);
  }

  uchar *pack_int32(uchar *to, const uchar *from, bool low_byte_first_to)
  {
    handle_int32(to, from, table->s->db_low_byte_first, low_byte_first_to);
    return to  + sizeof(int32);
  }

  const uchar *unpack_int32(uchar* to, const uchar *from,
                            bool low_byte_first_from)
  {
    handle_int32(to, from, low_byte_first_from, table->s->db_low_byte_first);
    return from + sizeof(int32);
  }

  uchar *pack_int64(uchar* to, const uchar *from, bool low_byte_first_to)
  {
    handle_int64(to, from, table->s->db_low_byte_first, low_byte_first_to);
    return to + sizeof(int64);
  }

  const uchar *unpack_int64(uchar* to, const uchar *from,
                            bool low_byte_first_from)
  {
    handle_int64(to, from, low_byte_first_from, table->s->db_low_byte_first);
    return from + sizeof(int64);
  }

  bool field_flags_are_binary()
  {
    return (flags & (BINCMP_FLAG | BINARY_FLAG)) != 0;
  }

};


class Field_num :public Field {
public:
  const uint8 dec;
  bool zerofill,unsigned_flag;	// Purify cannot handle bit fields
  Field_num(uchar *ptr_arg,uint32 len_arg, uchar *null_ptr_arg,
	    uchar null_bit_arg, utype unireg_check_arg,
	    const char *field_name_arg,
            uint8 dec_arg, bool zero_arg, bool unsigned_arg);
  Item_result result_type () const { return REAL_RESULT; }
  void prepend_zeros(String *value);
  void add_zerofill_and_unsigned(String &res) const;
  friend class Create_field;
  void make_field(Send_field *);
  uint decimals() const { return (uint) dec; }
  uint size_of() const { return sizeof(*this); }
  bool eq_def(Field *field);
  int store_decimal(const my_decimal *);
  my_decimal *val_decimal(my_decimal *);
  uint is_equal(Create_field *new_field);
  int check_int(CHARSET_INFO *cs, const char *str, int length,
                const char *int_end, int error);
  bool get_int(CHARSET_INFO *cs, const char *from, uint len, 
               longlong *rnd, ulonglong unsigned_max, 
               longlong signed_min, longlong signed_max);
};


class Field_str :public Field {
protected:
  CHARSET_INFO *field_charset;
  enum Derivation field_derivation;
public:
  Field_str(uchar *ptr_arg,uint32 len_arg, uchar *null_ptr_arg,
	    uchar null_bit_arg, utype unireg_check_arg,
	    const char *field_name_arg, CHARSET_INFO *charset);
  Item_result result_type () const { return STRING_RESULT; }
  uint decimals() const { return NOT_FIXED_DEC; }
  int  store(double nr);
  int  store(longlong nr, bool unsigned_val)=0;
  int  store_decimal(const my_decimal *);
  int  store(const char *to,uint length,CHARSET_INFO *cs)=0;
  uint size_of() const { return sizeof(*this); }
  CHARSET_INFO *charset(void) const { return field_charset; }
  void set_charset(CHARSET_INFO *charset_arg) { field_charset= charset_arg; }
  enum Derivation derivation(void) const { return field_derivation; }
  virtual void set_derivation(enum Derivation derivation_arg)
  { field_derivation= derivation_arg; }
  bool binary() const { return field_charset == &my_charset_bin; }
  uint32 max_display_length() { return field_length; }
  friend class Create_field;
  my_decimal *val_decimal(my_decimal *);
  virtual bool str_needs_quotes() { return TRUE; }
  uint is_equal(Create_field *new_field);
};


/* base class for Field_string, Field_varstring and Field_blob */

class Field_longstr :public Field_str
{
protected:
  int report_if_important_data(const char *ptr, const char *end,
                               bool count_spaces);
public:
  Field_longstr(uchar *ptr_arg, uint32 len_arg, uchar *null_ptr_arg,
                uchar null_bit_arg, utype unireg_check_arg,
                const char *field_name_arg, CHARSET_INFO *charset_arg)
    :Field_str(ptr_arg, len_arg, null_ptr_arg, null_bit_arg, unireg_check_arg,
               field_name_arg, charset_arg)
    {}

  int store_decimal(const my_decimal *d);
  uint32 max_data_length() const;
};

/* base class for float and double and decimal (old one) */
class Field_real :public Field_num {
public:
  my_bool not_fixed;

  Field_real(uchar *ptr_arg, uint32 len_arg, uchar *null_ptr_arg,
             uchar null_bit_arg, utype unireg_check_arg,
             const char *field_name_arg,
             uint8 dec_arg, bool zero_arg, bool unsigned_arg)
    :Field_num(ptr_arg, len_arg, null_ptr_arg, null_bit_arg, unireg_check_arg,
               field_name_arg, dec_arg, zero_arg, unsigned_arg),
    not_fixed(dec_arg >= NOT_FIXED_DEC)
    {}
  int store_decimal(const my_decimal *);
  my_decimal *val_decimal(my_decimal *);
  int truncate(double *nr, double max_length);
  uint32 max_display_length() { return field_length; }
  uint size_of() const { return sizeof(*this); }
  virtual const uchar *unpack(uchar* to, const uchar *from,
                              uint param_data, bool low_byte_first);
  virtual uchar *pack(uchar* to, const uchar *from,
                      uint max_length, bool low_byte_first);
};


class Field_decimal :public Field_real {
public:
  Field_decimal(uchar *ptr_arg, uint32 len_arg, uchar *null_ptr_arg,
		uchar null_bit_arg,
		enum utype unireg_check_arg, const char *field_name_arg,
		uint8 dec_arg,bool zero_arg,bool unsigned_arg)
    :Field_real(ptr_arg, len_arg, null_ptr_arg, null_bit_arg,
                unireg_check_arg, field_name_arg,
                dec_arg, zero_arg, unsigned_arg)
    {}
  enum_field_types type() const { return MYSQL_TYPE_DECIMAL;}
  enum ha_base_keytype key_type() const
  { return zerofill ? HA_KEYTYPE_BINARY : HA_KEYTYPE_NUM; }
  int reset(void);
  int store(const char *to,uint length,CHARSET_INFO *charset);
  int store(double nr);
  int store(longlong nr, bool unsigned_val);
  double val_real(void);
  longlong val_int(void);
  String *val_str(String*,String *);
  int cmp(const uchar *,const uchar *);
  void sort_string(uchar *buff,uint length);
  void overflow(bool negative);
  bool zero_pack() const { return 0; }
  void sql_type(String &str) const;
  virtual const uchar *unpack(uchar* to, const uchar *from,
                              uint param_data, bool low_byte_first)
  {
    return Field::unpack(to, from, param_data, low_byte_first);
  }
  virtual uchar *pack(uchar* to, const uchar *from,
                      uint max_length, bool low_byte_first)
  {
    return Field::pack(to, from, max_length, low_byte_first);
  }
};


/* New decimal/numeric field which use fixed point arithmetic */
class Field_new_decimal :public Field_num {
private:
  int do_save_field_metadata(uchar *first_byte);
public:
  /* The maximum number of decimal digits can be stored */
  uint precision;
  uint bin_size;
  /*
    Constructors take max_length of the field as a parameter - not the
    precision as the number of decimal digits allowed.
    So for example we need to count length from precision handling
    CREATE TABLE ( DECIMAL(x,y)) 
  */
  Field_new_decimal(uchar *ptr_arg, uint32 len_arg, uchar *null_ptr_arg,
                    uchar null_bit_arg,
                    enum utype unireg_check_arg, const char *field_name_arg,
                    uint8 dec_arg, bool zero_arg, bool unsigned_arg);
  Field_new_decimal(uint32 len_arg, bool maybe_null_arg,
                    const char *field_name_arg, uint8 dec_arg,
                    bool unsigned_arg);
  enum_field_types type() const { return MYSQL_TYPE_NEWDECIMAL;}
  enum ha_base_keytype key_type() const { return HA_KEYTYPE_BINARY; }
  Item_result result_type () const { return DECIMAL_RESULT; }
  int  reset(void);
  bool store_value(const my_decimal *decimal_value);
  void set_value_on_overflow(my_decimal *decimal_value, bool sign);
  int  store(const char *to, uint length, CHARSET_INFO *charset);
  int  store(double nr);
  int  store(longlong nr, bool unsigned_val);
  int store_time(MYSQL_TIME *ltime, timestamp_type t_type);
  int  store_decimal(const my_decimal *);
  double val_real(void);
  longlong val_int(void);
  my_decimal *val_decimal(my_decimal *);
  String *val_str(String*, String *);
  int cmp(const uchar *, const uchar *);
  void sort_string(uchar *buff, uint length);
  bool zero_pack() const { return 0; }
  void sql_type(String &str) const;
  uint32 max_display_length() { return field_length; }
  uint size_of() const { return sizeof(*this); } 
  uint32 pack_length() const { return (uint32) bin_size; }
  uint pack_length_from_metadata(uint field_metadata);
  uint row_pack_length() { return pack_length(); }
  int compatible_field_size(uint field_metadata,
                            const Relay_log_info *rli, uint16 mflags);
  uint is_equal(Create_field *new_field);
  virtual const uchar *unpack(uchar* to, const uchar *from,
                              uint param_data, bool low_byte_first);
  static Field *create_from_item (Item *);
};


class Field_tiny :public Field_num {
public:
  Field_tiny(uchar *ptr_arg, uint32 len_arg, uchar *null_ptr_arg,
	     uchar null_bit_arg,
	     enum utype unireg_check_arg, const char *field_name_arg,
	     bool zero_arg, bool unsigned_arg)
    :Field_num(ptr_arg, len_arg, null_ptr_arg, null_bit_arg,
	       unireg_check_arg, field_name_arg,
	       0, zero_arg,unsigned_arg)
    {}
  enum Item_result result_type () const { return INT_RESULT; }
  enum_field_types type() const { return MYSQL_TYPE_TINY;}
  enum ha_base_keytype key_type() const
    { return unsigned_flag ? HA_KEYTYPE_BINARY : HA_KEYTYPE_INT8; }
  int store(const char *to,uint length,CHARSET_INFO *charset);
  int store(double nr);
  int store(longlong nr, bool unsigned_val);
  int reset(void) { ptr[0]=0; return 0; }
  double val_real(void);
  longlong val_int(void);
  String *val_str(String*,String *);
  bool send_binary(Protocol *protocol);
  int cmp(const uchar *,const uchar *);
  void sort_string(uchar *buff,uint length);
  uint32 pack_length() const { return 1; }
  void sql_type(String &str) const;
  uint32 max_display_length() { return 4; }

  virtual uchar *pack(uchar* to, const uchar *from,
                      uint max_length, bool low_byte_first)
  {
    *to= *from;
    return to + 1;
  }

  virtual const uchar *unpack(uchar* to, const uchar *from,
                              uint param_data, bool low_byte_first)
  {
    *to= *from;
    return from + 1;
  }
};


class Field_short :public Field_num {
public:
  Field_short(uchar *ptr_arg, uint32 len_arg, uchar *null_ptr_arg,
	      uchar null_bit_arg,
	      enum utype unireg_check_arg, const char *field_name_arg,
	      bool zero_arg, bool unsigned_arg)
    :Field_num(ptr_arg, len_arg, null_ptr_arg, null_bit_arg,
	       unireg_check_arg, field_name_arg,
	       0, zero_arg,unsigned_arg)
    {}
  Field_short(uint32 len_arg,bool maybe_null_arg, const char *field_name_arg,
	      bool unsigned_arg)
    :Field_num((uchar*) 0, len_arg, maybe_null_arg ? (uchar*) "": 0,0,
	       NONE, field_name_arg, 0, 0, unsigned_arg)
    {}
  enum Item_result result_type () const { return INT_RESULT; }
  enum_field_types type() const { return MYSQL_TYPE_SHORT;}
  enum ha_base_keytype key_type() const
    { return unsigned_flag ? HA_KEYTYPE_USHORT_INT : HA_KEYTYPE_SHORT_INT;}
  int store(const char *to,uint length,CHARSET_INFO *charset);
  int store(double nr);
  int store(longlong nr, bool unsigned_val);
  int reset(void) { ptr[0]=ptr[1]=0; return 0; }
  double val_real(void);
  longlong val_int(void);
  String *val_str(String*,String *);
  bool send_binary(Protocol *protocol);
  int cmp(const uchar *,const uchar *);
  void sort_string(uchar *buff,uint length);
  uint32 pack_length() const { return 2; }
  void sql_type(String &str) const;
  uint32 max_display_length() { return 6; }

  virtual uchar *pack(uchar* to, const uchar *from,
                      uint max_length, bool low_byte_first)
  {
    int16 val;
#ifdef WORDS_BIGENDIAN
    if (table->s->db_low_byte_first)
      val = sint2korr(from);
    else
#endif
      shortget(val, from);

#ifdef WORDS_BIGENDIAN
    if (low_byte_first)
      int2store(to, val);
    else
#endif
      shortstore(to, val);
    return to + sizeof(val);
  }

  virtual const uchar *unpack(uchar* to, const uchar *from,
                              uint param_data, bool low_byte_first)
  {
    int16 val;
#ifdef WORDS_BIGENDIAN
    if (low_byte_first)
      val = sint2korr(from);
    else
#endif
      shortget(val, from);

#ifdef WORDS_BIGENDIAN
    if (table->s->db_low_byte_first)
      int2store(to, val);
    else
#endif
      shortstore(to, val);
    return from + sizeof(val);
  }
};

class Field_medium :public Field_num {
public:
  Field_medium(uchar *ptr_arg, uint32 len_arg, uchar *null_ptr_arg,
	      uchar null_bit_arg,
	      enum utype unireg_check_arg, const char *field_name_arg,
	      bool zero_arg, bool unsigned_arg)
    :Field_num(ptr_arg, len_arg, null_ptr_arg, null_bit_arg,
	       unireg_check_arg, field_name_arg,
	       0, zero_arg,unsigned_arg)
    {}
  enum Item_result result_type () const { return INT_RESULT; }
  enum_field_types type() const { return MYSQL_TYPE_INT24;}
  enum ha_base_keytype key_type() const
    { return unsigned_flag ? HA_KEYTYPE_UINT24 : HA_KEYTYPE_INT24; }
  int store(const char *to,uint length,CHARSET_INFO *charset);
  int store(double nr);
  int store(longlong nr, bool unsigned_val);
  int reset(void) { ptr[0]=ptr[1]=ptr[2]=0; return 0; }
  double val_real(void);
  longlong val_int(void);
  String *val_str(String*,String *);
  bool send_binary(Protocol *protocol);
  int cmp(const uchar *,const uchar *);
  void sort_string(uchar *buff,uint length);
  uint32 pack_length() const { return 3; }
  void sql_type(String &str) const;
  uint32 max_display_length() { return 8; }

  virtual uchar *pack(uchar* to, const uchar *from,
                      uint max_length, bool low_byte_first)
  {
    return Field::pack(to, from, max_length, low_byte_first);
  }

  virtual const uchar *unpack(uchar* to, const uchar *from,
                              uint param_data, bool low_byte_first)
  {
    return Field::unpack(to, from, param_data, low_byte_first);
  }
};


class Field_long :public Field_num {
public:
  Field_long(uchar *ptr_arg, uint32 len_arg, uchar *null_ptr_arg,
	     uchar null_bit_arg,
	     enum utype unireg_check_arg, const char *field_name_arg,
	     bool zero_arg, bool unsigned_arg)
    :Field_num(ptr_arg, len_arg, null_ptr_arg, null_bit_arg,
	       unireg_check_arg, field_name_arg,
	       0, zero_arg,unsigned_arg)
    {}
  Field_long(uint32 len_arg,bool maybe_null_arg, const char *field_name_arg,
	     bool unsigned_arg)
    :Field_num((uchar*) 0, len_arg, maybe_null_arg ? (uchar*) "": 0,0,
	       NONE, field_name_arg,0,0,unsigned_arg)
    {}
  enum Item_result result_type () const { return INT_RESULT; }
  enum_field_types type() const { return MYSQL_TYPE_LONG;}
  enum ha_base_keytype key_type() const
    { return unsigned_flag ? HA_KEYTYPE_ULONG_INT : HA_KEYTYPE_LONG_INT; }
  int store(const char *to,uint length,CHARSET_INFO *charset);
  int store(double nr);
  int store(longlong nr, bool unsigned_val);
  int reset(void) { ptr[0]=ptr[1]=ptr[2]=ptr[3]=0; return 0; }
  double val_real(void);
  longlong val_int(void);
  bool send_binary(Protocol *protocol);
  String *val_str(String*,String *);
  int cmp(const uchar *,const uchar *);
  void sort_string(uchar *buff,uint length);
  uint32 pack_length() const { return 4; }
  void sql_type(String &str) const;
  uint32 max_display_length() { return MY_INT32_NUM_DECIMAL_DIGITS; }
  virtual uchar *pack(uchar* to, const uchar *from,
                      uint max_length __attribute__((unused)),
                      bool low_byte_first)
  {
    return pack_int32(to, from, low_byte_first);
  }
  virtual const uchar *unpack(uchar* to, const uchar *from,
                              uint param_data __attribute__((unused)),
                              bool low_byte_first)
  {
    return unpack_int32(to, from, low_byte_first);
  }
};


#ifdef HAVE_LONG_LONG
class Field_longlong :public Field_num {
public:
  Field_longlong(uchar *ptr_arg, uint32 len_arg, uchar *null_ptr_arg,
	      uchar null_bit_arg,
	      enum utype unireg_check_arg, const char *field_name_arg,
	      bool zero_arg, bool unsigned_arg)
    :Field_num(ptr_arg, len_arg, null_ptr_arg, null_bit_arg,
	       unireg_check_arg, field_name_arg,
	       0, zero_arg,unsigned_arg)
    {}
  Field_longlong(uint32 len_arg,bool maybe_null_arg,
		 const char *field_name_arg,
		  bool unsigned_arg)
    :Field_num((uchar*) 0, len_arg, maybe_null_arg ? (uchar*) "": 0,0,
	       NONE, field_name_arg,0,0,unsigned_arg)
    {}
  enum Item_result result_type () const { return INT_RESULT; }
  enum_field_types type() const { return MYSQL_TYPE_LONGLONG;}
  enum ha_base_keytype key_type() const
    { return unsigned_flag ? HA_KEYTYPE_ULONGLONG : HA_KEYTYPE_LONGLONG; }
  int store(const char *to,uint length,CHARSET_INFO *charset);
  int store(double nr);
  int store(longlong nr, bool unsigned_val);
  int reset(void)
  {
    ptr[0]=ptr[1]=ptr[2]=ptr[3]=ptr[4]=ptr[5]=ptr[6]=ptr[7]=0;
    return 0;
  }
  double val_real(void);
  longlong val_int(void);
  String *val_str(String*,String *);
  bool send_binary(Protocol *protocol);
  int cmp(const uchar *,const uchar *);
  void sort_string(uchar *buff,uint length);
  uint32 pack_length() const { return 8; }
  void sql_type(String &str) const;
  bool can_be_compared_as_longlong() const { return TRUE; }
  uint32 max_display_length() { return 20; }
  virtual uchar *pack(uchar* to, const uchar *from,
                      uint max_length  __attribute__((unused)),
                      bool low_byte_first)
  {
    return pack_int64(to, from, low_byte_first);
  }
  virtual const uchar *unpack(uchar* to, const uchar *from,
                              uint param_data __attribute__((unused)),
                              bool low_byte_first)
  {
    return unpack_int64(to, from, low_byte_first);
  }
};
#endif


class Field_float :public Field_real {
public:
  Field_float(uchar *ptr_arg, uint32 len_arg, uchar *null_ptr_arg,
	      uchar null_bit_arg,
	      enum utype unireg_check_arg, const char *field_name_arg,
              uint8 dec_arg,bool zero_arg,bool unsigned_arg)
    :Field_real(ptr_arg, len_arg, null_ptr_arg, null_bit_arg,
                unireg_check_arg, field_name_arg,
                dec_arg, zero_arg, unsigned_arg)
    {}
  Field_float(uint32 len_arg, bool maybe_null_arg, const char *field_name_arg,
	      uint8 dec_arg)
    :Field_real((uchar*) 0, len_arg, maybe_null_arg ? (uchar*) "": 0, (uint) 0,
                NONE, field_name_arg, dec_arg, 0, 0)
    {}
  enum_field_types type() const { return MYSQL_TYPE_FLOAT;}
  enum ha_base_keytype key_type() const { return HA_KEYTYPE_FLOAT; }
  int store(const char *to,uint length,CHARSET_INFO *charset);
  int store(double nr);
  int store(longlong nr, bool unsigned_val);
  int reset(void) { bzero(ptr,sizeof(float)); return 0; }
  double val_real(void);
  longlong val_int(void);
  String *val_str(String*,String *);
  bool send_binary(Protocol *protocol);
  int cmp(const uchar *,const uchar *);
  void sort_string(uchar *buff,uint length);
  uint32 pack_length() const { return sizeof(float); }
  uint row_pack_length() { return pack_length(); }
  void sql_type(String &str) const;
private:
  int do_save_field_metadata(uchar *first_byte);
};


class Field_double :public Field_real {
public:
  Field_double(uchar *ptr_arg, uint32 len_arg, uchar *null_ptr_arg,
	       uchar null_bit_arg,
	       enum utype unireg_check_arg, const char *field_name_arg,
	       uint8 dec_arg,bool zero_arg,bool unsigned_arg)
    :Field_real(ptr_arg, len_arg, null_ptr_arg, null_bit_arg,
                unireg_check_arg, field_name_arg,
                dec_arg, zero_arg, unsigned_arg)
    {}
  Field_double(uint32 len_arg, bool maybe_null_arg, const char *field_name_arg,
	       uint8 dec_arg)
    :Field_real((uchar*) 0, len_arg, maybe_null_arg ? (uchar*) "" : 0, (uint) 0,
                NONE, field_name_arg, dec_arg, 0, 0)
    {}
  Field_double(uint32 len_arg, bool maybe_null_arg, const char *field_name_arg,
	       uint8 dec_arg, my_bool not_fixed_arg)
    :Field_real((uchar*) 0, len_arg, maybe_null_arg ? (uchar*) "" : 0, (uint) 0,
                NONE, field_name_arg, dec_arg, 0, 0)
    {not_fixed= not_fixed_arg; }
  enum_field_types type() const { return MYSQL_TYPE_DOUBLE;}
  enum ha_base_keytype key_type() const { return HA_KEYTYPE_DOUBLE; }
  int  store(const char *to,uint length,CHARSET_INFO *charset);
  int  store(double nr);
  int  store(longlong nr, bool unsigned_val);
  int reset(void) { bzero(ptr,sizeof(double)); return 0; }
  double val_real(void);
  longlong val_int(void);
  String *val_str(String*,String *);
  bool send_binary(Protocol *protocol);
  int cmp(const uchar *,const uchar *);
  void sort_string(uchar *buff,uint length);
  uint32 pack_length() const { return sizeof(double); }
  uint row_pack_length() { return pack_length(); }
  void sql_type(String &str) const;
private:
  int do_save_field_metadata(uchar *first_byte);
};


/* Everything saved in this will disappear. It will always return NULL */

class Field_null :public Field_str {
  static uchar null[1];
public:
  Field_null(uchar *ptr_arg, uint32 len_arg,
	     enum utype unireg_check_arg, const char *field_name_arg,
	     CHARSET_INFO *cs)
    :Field_str(ptr_arg, len_arg, null, 1,
	       unireg_check_arg, field_name_arg, cs)
    {}
  enum_field_types type() const { return MYSQL_TYPE_NULL;}
  int  store(const char *to, uint length, CHARSET_INFO *cs)
  { null[0]=1; return 0; }
  int store(double nr)   { null[0]=1; return 0; }
  int store(longlong nr, bool unsigned_val) { null[0]=1; return 0; }
  int store_decimal(const my_decimal *d)  { null[0]=1; return 0; }
  int reset(void)	  { return 0; }
  double val_real(void)		{ return 0.0;}
  longlong val_int(void)	{ return 0;}
  my_decimal *val_decimal(my_decimal *) { return 0; }
  String *val_str(String *value,String *value2)
  { value2->length(0); return value2;}
  int cmp(const uchar *a, const uchar *b) { return 0;}
  void sort_string(uchar *buff, uint length)  {}
  uint32 pack_length() const { return 0; }
  void sql_type(String &str) const;
  uint size_of() const { return sizeof(*this); }
  uint32 max_display_length() { return 4; }
};


class Field_timestamp :public Field_str {
public:
  Field_timestamp(uchar *ptr_arg, uint32 len_arg,
                  uchar *null_ptr_arg, uchar null_bit_arg,
		  enum utype unireg_check_arg, const char *field_name_arg,
		  TABLE_SHARE *share, CHARSET_INFO *cs);
  Field_timestamp(bool maybe_null_arg, const char *field_name_arg,
		  CHARSET_INFO *cs);
  enum_field_types type() const { return MYSQL_TYPE_TIMESTAMP;}
  enum ha_base_keytype key_type() const { return HA_KEYTYPE_ULONG_INT; }
  enum Item_result cmp_type () const { return INT_RESULT; }
  int  store(const char *to,uint length,CHARSET_INFO *charset);
  int  store(double nr);
  int  store(longlong nr, bool unsigned_val);
  int  reset(void) { ptr[0]=ptr[1]=ptr[2]=ptr[3]=0; return 0; }
  double val_real(void);
  longlong val_int(void);
  String *val_str(String*,String *);
  bool send_binary(Protocol *protocol);
  int cmp(const uchar *,const uchar *);
  void sort_string(uchar *buff,uint length);
  uint32 pack_length() const { return 4; }
  void sql_type(String &str) const;
  bool can_be_compared_as_longlong() const { return TRUE; }
  bool zero_pack() const { return 0; }
  void set_time();
  virtual void set_default()
  {
    if (table->timestamp_field == this &&
        unireg_check != TIMESTAMP_UN_FIELD)
      set_time();
    else
      Field::set_default();
  }
  /* Get TIMESTAMP field value as seconds since begging of Unix Epoch */
  inline long get_timestamp(my_bool *null_value)
  {
    if ((*null_value= is_null()))
      return 0;
#ifdef WORDS_BIGENDIAN
    if (table && table->s->db_low_byte_first)
      return sint4korr(ptr);
#endif
    long tmp;
    longget(tmp,ptr);
    return tmp;
  }
  inline void store_timestamp(my_time_t timestamp)
  {
#ifdef WORDS_BIGENDIAN
    if (table && table->s->db_low_byte_first)
    {
      int4store(ptr,timestamp);
    }
    else
#endif
      longstore(ptr,(uint32) timestamp);
  }
  bool get_date(MYSQL_TIME *ltime,uint fuzzydate);
  bool get_time(MYSQL_TIME *ltime);
  timestamp_auto_set_type get_auto_set_type() const;
  uchar *pack(uchar *to, const uchar *from,
              uint max_length __attribute__((unused)), bool low_byte_first)
  {
    return pack_int32(to, from, low_byte_first);
  }
  const uchar *unpack(uchar* to, const uchar *from,
                      uint param_data __attribute__((unused)),
                      bool low_byte_first)
  {
    return unpack_int32(to, from, low_byte_first);
  }
};


class Field_year :public Field_tiny {
public:
  Field_year(uchar *ptr_arg, uint32 len_arg, uchar *null_ptr_arg,
	     uchar null_bit_arg,
	     enum utype unireg_check_arg, const char *field_name_arg)
    :Field_tiny(ptr_arg, len_arg, null_ptr_arg, null_bit_arg,
		unireg_check_arg, field_name_arg, 1, 1)
    {}
  enum_field_types type() const { return MYSQL_TYPE_YEAR;}
  int  store(const char *to,uint length,CHARSET_INFO *charset);
  int  store(double nr);
  int  store(longlong nr, bool unsigned_val);
  double val_real(void);
  longlong val_int(void);
  String *val_str(String*,String *);
  bool send_binary(Protocol *protocol);
  void sql_type(String &str) const;
  bool can_be_compared_as_longlong() const { return TRUE; }
};


class Field_date :public Field_str {
public:
  Field_date(uchar *ptr_arg, uchar *null_ptr_arg, uchar null_bit_arg,
	     enum utype unireg_check_arg, const char *field_name_arg,
	     CHARSET_INFO *cs)
    :Field_str(ptr_arg, MAX_DATE_WIDTH, null_ptr_arg, null_bit_arg,
	       unireg_check_arg, field_name_arg, cs)
    {}
  Field_date(bool maybe_null_arg, const char *field_name_arg,
             CHARSET_INFO *cs)
    :Field_str((uchar*) 0, MAX_DATE_WIDTH, maybe_null_arg ? (uchar*) "": 0,0,
	       NONE, field_name_arg, cs) {}
  enum_field_types type() const { return MYSQL_TYPE_DATE;}
  enum ha_base_keytype key_type() const { return HA_KEYTYPE_ULONG_INT; }
  enum Item_result cmp_type () const { return INT_RESULT; }
  int store(const char *to,uint length,CHARSET_INFO *charset);
  int store(double nr);
  int store(longlong nr, bool unsigned_val);
  int reset(void) { ptr[0]=ptr[1]=ptr[2]=ptr[3]=0; return 0; }
  double val_real(void);
  longlong val_int(void);
  String *val_str(String*,String *);
  bool get_time(MYSQL_TIME *ltime);
  bool send_binary(Protocol *protocol);
  int cmp(const uchar *,const uchar *);
  void sort_string(uchar *buff,uint length);
  uint32 pack_length() const { return 4; }
  void sql_type(String &str) const;
  bool can_be_compared_as_longlong() const { return TRUE; }
  bool zero_pack() const { return 1; }
  uchar *pack(uchar* to, const uchar *from,
              uint max_length __attribute__((unused)), bool low_byte_first)
  {
    return pack_int32(to, from, low_byte_first);
  }
  const uchar *unpack(uchar* to, const uchar *from,
                      uint param_data __attribute__((unused)),
                      bool low_byte_first)
  {
    return unpack_int32(to, from, low_byte_first);
  }
};


class Field_newdate :public Field_str {
public:
  Field_newdate(uchar *ptr_arg, uchar *null_ptr_arg, uchar null_bit_arg,
		enum utype unireg_check_arg, const char *field_name_arg,
		CHARSET_INFO *cs)
    :Field_str(ptr_arg, 10, null_ptr_arg, null_bit_arg,
	       unireg_check_arg, field_name_arg, cs)
    {}
  Field_newdate(bool maybe_null_arg, const char *field_name_arg,
                CHARSET_INFO *cs)
    :Field_str((uchar*) 0,10, maybe_null_arg ? (uchar*) "": 0,0,
               NONE, field_name_arg, cs) {}
  enum_field_types type() const { return MYSQL_TYPE_DATE;}
  enum_field_types real_type() const { return MYSQL_TYPE_NEWDATE; }
  enum ha_base_keytype key_type() const { return HA_KEYTYPE_UINT24; }
  enum Item_result cmp_type () const { return INT_RESULT; }
  int  store(const char *to,uint length,CHARSET_INFO *charset);
  int  store(double nr);
  int  store(longlong nr, bool unsigned_val);
  int store_time(MYSQL_TIME *ltime, timestamp_type type);
  int reset(void) { ptr[0]=ptr[1]=ptr[2]=0; return 0; }
  double val_real(void);
  longlong val_int(void);
  String *val_str(String*,String *);
  bool send_binary(Protocol *protocol);
  int cmp(const uchar *,const uchar *);
  void sort_string(uchar *buff,uint length);
  uint32 pack_length() const { return 3; }
  void sql_type(String &str) const;
  bool can_be_compared_as_longlong() const { return TRUE; }
  bool zero_pack() const { return 1; }
  bool get_date(MYSQL_TIME *ltime,uint fuzzydate);
  bool get_time(MYSQL_TIME *ltime);
};


class Field_time :public Field_str {
public:
  Field_time(uchar *ptr_arg, uchar *null_ptr_arg, uchar null_bit_arg,
	     enum utype unireg_check_arg, const char *field_name_arg,
	     CHARSET_INFO *cs)
    :Field_str(ptr_arg, 8, null_ptr_arg, null_bit_arg,
	       unireg_check_arg, field_name_arg, cs)
    {}
  Field_time(bool maybe_null_arg, const char *field_name_arg,
             CHARSET_INFO *cs)
    :Field_str((uchar*) 0,8, maybe_null_arg ? (uchar*) "": 0,0,
	       NONE, field_name_arg, cs) {}
  enum_field_types type() const { return MYSQL_TYPE_TIME;}
  enum ha_base_keytype key_type() const { return HA_KEYTYPE_INT24; }
  enum Item_result cmp_type () const { return INT_RESULT; }
  int store_time(MYSQL_TIME *ltime, timestamp_type type);
  int store(const char *to,uint length,CHARSET_INFO *charset);
  int store(double nr);
  int store(longlong nr, bool unsigned_val);
  int reset(void) { ptr[0]=ptr[1]=ptr[2]=0; return 0; }
  double val_real(void);
  longlong val_int(void);
  String *val_str(String*,String *);
  bool get_date(MYSQL_TIME *ltime, uint fuzzydate);
  bool send_binary(Protocol *protocol);
  bool get_time(MYSQL_TIME *ltime);
  int cmp(const uchar *,const uchar *);
  void sort_string(uchar *buff,uint length);
  uint32 pack_length() const { return 3; }
  void sql_type(String &str) const;
  bool can_be_compared_as_longlong() const { return TRUE; }
  bool zero_pack() const { return 1; }
};


class Field_datetime :public Field_str {
public:
  Field_datetime(uchar *ptr_arg, uchar *null_ptr_arg, uchar null_bit_arg,
		 enum utype unireg_check_arg, const char *field_name_arg,
		 CHARSET_INFO *cs)
    :Field_str(ptr_arg, MAX_DATETIME_WIDTH, null_ptr_arg, null_bit_arg,
	       unireg_check_arg, field_name_arg, cs)
    {}
  Field_datetime(bool maybe_null_arg, const char *field_name_arg,
		 CHARSET_INFO *cs)
    :Field_str((uchar*) 0, MAX_DATETIME_WIDTH, maybe_null_arg ? (uchar*) "": 0,0,
	       NONE, field_name_arg, cs) {}
  enum_field_types type() const { return MYSQL_TYPE_DATETIME;}
#ifdef HAVE_LONG_LONG
  enum ha_base_keytype key_type() const { return HA_KEYTYPE_ULONGLONG; }
#endif
  enum Item_result cmp_type () const { return INT_RESULT; }
  uint decimals() const { return DATETIME_DEC; }
  int  store(const char *to,uint length,CHARSET_INFO *charset);
  int  store(double nr);
  int  store(longlong nr, bool unsigned_val);
  int store_time(MYSQL_TIME *ltime, timestamp_type type);
  int reset(void)
  {
    ptr[0]=ptr[1]=ptr[2]=ptr[3]=ptr[4]=ptr[5]=ptr[6]=ptr[7]=0;
    return 0;
  }
  double val_real(void);
  longlong val_int(void);
  String *val_str(String*,String *);
  bool send_binary(Protocol *protocol);
  int cmp(const uchar *,const uchar *);
  void sort_string(uchar *buff,uint length);
  uint32 pack_length() const { return 8; }
  void sql_type(String &str) const;
  bool can_be_compared_as_longlong() const { return TRUE; }
  bool zero_pack() const { return 1; }
  bool get_date(MYSQL_TIME *ltime,uint fuzzydate);
  bool get_time(MYSQL_TIME *ltime);
  uchar *pack(uchar* to, const uchar *from,
              uint max_length __attribute__((unused)), bool low_byte_first)
  {
    return pack_int64(to, from, low_byte_first);
  }
  const uchar *unpack(uchar* to, const uchar *from,
                      uint param_data __attribute__((unused)),
                      bool low_byte_first)
  {
    return unpack_int64(to, from, low_byte_first);
  }
};


class Field_string :public Field_longstr {
public:
  bool can_alter_field_type;
  Field_string(uchar *ptr_arg, uint32 len_arg,uchar *null_ptr_arg,
	       uchar null_bit_arg,
	       enum utype unireg_check_arg, const char *field_name_arg,
	       CHARSET_INFO *cs)
    :Field_longstr(ptr_arg, len_arg, null_ptr_arg, null_bit_arg,
                   unireg_check_arg, field_name_arg, cs),
     can_alter_field_type(1) {};
  Field_string(uint32 len_arg,bool maybe_null_arg, const char *field_name_arg,
               CHARSET_INFO *cs)
    :Field_longstr((uchar*) 0, len_arg, maybe_null_arg ? (uchar*) "": 0, 0,
                   NONE, field_name_arg, cs),
     can_alter_field_type(1) {};

  enum_field_types type() const
  {
    return ((can_alter_field_type && orig_table &&
             orig_table->s->db_create_options & HA_OPTION_PACK_RECORD &&
	     field_length >= 4) &&
            orig_table->s->frm_version < FRM_VER_TRUE_VARCHAR ?
	    MYSQL_TYPE_VAR_STRING : MYSQL_TYPE_STRING);
  }
  enum ha_base_keytype key_type() const
    { return binary() ? HA_KEYTYPE_BINARY : HA_KEYTYPE_TEXT; }
  bool zero_pack() const { return 0; }
  int reset(void)
  {
    charset()->cset->fill(charset(),(char*) ptr, field_length,
                          (has_charset() ? ' ' : 0));
    return 0;
  }
  int store(const char *to,uint length,CHARSET_INFO *charset);
  int store(longlong nr, bool unsigned_val);
  int store(double nr) { return Field_str::store(nr); } /* QQ: To be deleted */
  double val_real(void);
  longlong val_int(void);
  String *val_str(String*,String *);
  my_decimal *val_decimal(my_decimal *);
  int cmp(const uchar *,const uchar *);
  void sort_string(uchar *buff,uint length);
  void sql_type(String &str) const;
  virtual uchar *pack(uchar *to, const uchar *from,
                      uint max_length, bool low_byte_first);
  virtual const uchar *unpack(uchar* to, const uchar *from,
                              uint param_data, bool low_byte_first);
  uint pack_length_from_metadata(uint field_metadata)
  {
    DBUG_PRINT("debug", ("field_metadata: 0x%04x", field_metadata));
    if (field_metadata == 0)
      return row_pack_length();
    return (((field_metadata >> 4) & 0x300) ^ 0x300) + (field_metadata & 0x00ff);
  }
  int compatible_field_size(uint field_metadata,
                            const Relay_log_info *rli, uint16 mflags);
  uint row_pack_length() { return (field_length + 1); }
  int pack_cmp(const uchar *a,const uchar *b,uint key_length,
               my_bool insert_or_update);
  int pack_cmp(const uchar *b,uint key_length,my_bool insert_or_update);
  uint packed_col_length(const uchar *to, uint length);
  uint max_packed_col_length(uint max_length);
  uint size_of() const { return sizeof(*this); }
  enum_field_types real_type() const { return MYSQL_TYPE_STRING; }
  bool has_charset(void) const
  { return charset() == &my_charset_bin ? FALSE : TRUE; }
  Field *new_field(MEM_ROOT *root, struct st_table *new_table, bool keep_type);
  virtual uint get_key_image(uchar *buff,uint length, imagetype type);
private:
  int do_save_field_metadata(uchar *first_byte);
};


class Field_varstring :public Field_longstr {
public:
  /*
    The maximum space available in a Field_varstring, in bytes. See
    length_bytes.
  */
  static const uint MAX_SIZE;
  /* Store number of bytes used to store length (1 or 2) */
  uint32 length_bytes;
  Field_varstring(uchar *ptr_arg,
                  uint32 len_arg, uint length_bytes_arg,
                  uchar *null_ptr_arg, uchar null_bit_arg,
		  enum utype unireg_check_arg, const char *field_name_arg,
		  TABLE_SHARE *share, CHARSET_INFO *cs)
    :Field_longstr(ptr_arg, len_arg, null_ptr_arg, null_bit_arg,
                   unireg_check_arg, field_name_arg, cs),
     length_bytes(length_bytes_arg)
  {
    share->varchar_fields++;
  }
  Field_varstring(uint32 len_arg,bool maybe_null_arg,
                  const char *field_name_arg,
                  TABLE_SHARE *share, CHARSET_INFO *cs)
    :Field_longstr((uchar*) 0,len_arg, maybe_null_arg ? (uchar*) "": 0, 0,
                   NONE, field_name_arg, cs),
     length_bytes(len_arg < 256 ? 1 :2)
  {
    share->varchar_fields++;
  }

  enum_field_types type() const { return MYSQL_TYPE_VARCHAR; }
  enum ha_base_keytype key_type() const;
  uint row_pack_length() { return field_length; }
  bool zero_pack() const { return 0; }
  int  reset(void) { bzero(ptr,field_length+length_bytes); return 0; }
  uint32 pack_length() const { return (uint32) field_length+length_bytes; }
  uint32 key_length() const { return (uint32) field_length; }
  uint32 sort_length() const
  {
    return (uint32) field_length + (field_charset == &my_charset_bin ?
                                    length_bytes : 0);
  }
  int  store(const char *to,uint length,CHARSET_INFO *charset);
  int  store(longlong nr, bool unsigned_val);
  int  store(double nr) { return Field_str::store(nr); } /* QQ: To be deleted */
  double val_real(void);
  longlong val_int(void);
  String *val_str(String*,String *);
  my_decimal *val_decimal(my_decimal *);
  int cmp_max(const uchar *, const uchar *, uint max_length);
  int cmp(const uchar *a,const uchar *b)
  {
    return cmp_max(a, b, ~0L);
  }
  void sort_string(uchar *buff,uint length);
  uint get_key_image(uchar *buff,uint length, imagetype type);
  void set_key_image(const uchar *buff,uint length);
  void sql_type(String &str) const;
  virtual uchar *pack(uchar *to, const uchar *from,
                      uint max_length, bool low_byte_first);
  uchar *pack_key(uchar *to, const uchar *from, uint max_length, bool low_byte_first);
  uchar *pack_key_from_key_image(uchar* to, const uchar *from,
                                 uint max_length, bool low_byte_first);
  virtual const uchar *unpack(uchar* to, const uchar *from,
                              uint param_data, bool low_byte_first);
  const uchar *unpack_key(uchar* to, const uchar *from,
                          uint max_length, bool low_byte_first);
  int pack_cmp(const uchar *a, const uchar *b, uint key_length,
               my_bool insert_or_update);
  int pack_cmp(const uchar *b, uint key_length,my_bool insert_or_update);
  int cmp_binary(const uchar *a,const uchar *b, uint32 max_length=~0L);
  int key_cmp(const uchar *,const uchar*);
  int key_cmp(const uchar *str, uint length);
  uint packed_col_length(const uchar *to, uint length);
  uint max_packed_col_length(uint max_length);
  uint32 data_length();
  uint size_of() const { return sizeof(*this); }
  enum_field_types real_type() const { return MYSQL_TYPE_VARCHAR; }
  bool has_charset(void) const
  { return charset() == &my_charset_bin ? FALSE : TRUE; }
  Field *new_field(MEM_ROOT *root, struct st_table *new_table, bool keep_type);
  Field *new_key_field(MEM_ROOT *root, struct st_table *new_table,
                       uchar *new_ptr, uchar *new_null_ptr,
                       uint new_null_bit);
  uint is_equal(Create_field *new_field);
  void hash(ulong *nr, ulong *nr2);
private:
  int do_save_field_metadata(uchar *first_byte);
};


class Field_blob :public Field_longstr {
protected:
  /**
    The number of bytes used to represent the length of the blob.
  */
  uint packlength;
  
  /**
    The 'value'-object is a cache fronting the storage engine.
  */
  String value;
  
public:
  Field_blob(uchar *ptr_arg, uchar *null_ptr_arg, uchar null_bit_arg,
	     enum utype unireg_check_arg, const char *field_name_arg,
	     TABLE_SHARE *share, uint blob_pack_length, CHARSET_INFO *cs);
  Field_blob(uint32 len_arg,bool maybe_null_arg, const char *field_name_arg,
             CHARSET_INFO *cs)
    :Field_longstr((uchar*) 0, len_arg, maybe_null_arg ? (uchar*) "": 0, 0,
                   NONE, field_name_arg, cs),
    packlength(4)
  {
    flags|= BLOB_FLAG;
  }
  Field_blob(uint32 len_arg,bool maybe_null_arg, const char *field_name_arg,
	     CHARSET_INFO *cs, bool set_packlength)
    :Field_longstr((uchar*) 0,len_arg, maybe_null_arg ? (uchar*) "": 0, 0,
                   NONE, field_name_arg, cs)
  {
    flags|= BLOB_FLAG;
    packlength= 4;
    if (set_packlength)
    {
      uint32 l_char_length= len_arg/cs->mbmaxlen;
      packlength= l_char_length <= 255 ? 1 :
                  l_char_length <= 65535 ? 2 :
                  l_char_length <= 16777215 ? 3 : 4;
    }
  }
  Field_blob(uint32 packlength_arg)
    :Field_longstr((uchar*) 0, 0, (uchar*) "", 0, NONE, "temp", system_charset_info),
    packlength(packlength_arg) {}
  enum_field_types type() const { return MYSQL_TYPE_BLOB;}
  enum ha_base_keytype key_type() const
    { return binary() ? HA_KEYTYPE_VARBINARY2 : HA_KEYTYPE_VARTEXT2; }
  int  store(const char *to,uint length,CHARSET_INFO *charset);
  int  store(double nr);
  int  store(longlong nr, bool unsigned_val);
  double val_real(void);
  longlong val_int(void);
  String *val_str(String*,String *);
  my_decimal *val_decimal(my_decimal *);
  int cmp_max(const uchar *, const uchar *, uint max_length);
  int cmp(const uchar *a,const uchar *b)
    { return cmp_max(a, b, ~0L); }
  int cmp(const uchar *a, uint32 a_length, const uchar *b, uint32 b_length);
  int cmp_binary(const uchar *a,const uchar *b, uint32 max_length=~0L);
  int key_cmp(const uchar *,const uchar*);
  int key_cmp(const uchar *str, uint length);
  uint32 key_length() const { return 0; }
  void sort_string(uchar *buff,uint length);
  uint32 pack_length() const
  { return (uint32) (packlength+table->s->blob_ptr_size); }

  /**
     Return the packed length without the pointer size added. 

     This is used to determine the size of the actual data in the row
     buffer.

     @returns The length of the raw data itself without the pointer.
  */
  uint32 pack_length_no_ptr() const
  { return (uint32) (packlength); }
  uint row_pack_length() { return pack_length_no_ptr(); }
  uint32 sort_length() const;
  virtual uint32 max_data_length() const
  {
    return (uint32) (((ulonglong) 1 << (packlength*8)) -1);
  }
  int reset(void) { bzero(ptr, packlength+sizeof(uchar*)); return 0; }
  void reset_fields() { bzero((uchar*) &value,sizeof(value)); }
  uint32 get_field_buffer_size(void) { return value.alloced_length(); }
#ifndef WORDS_BIGENDIAN
  static
#endif
  void store_length(uchar *i_ptr, uint i_packlength, uint32 i_number, bool low_byte_first);
  void store_length(uchar *i_ptr, uint i_packlength, uint32 i_number)
  {
    store_length(i_ptr, i_packlength, i_number, table->s->db_low_byte_first);
  }
  inline void store_length(uint32 number)
  {
    store_length(ptr, packlength, number);
  }

  /**
     Return the packed length plus the length of the data. 

     This is used to determine the size of the data plus the 
     packed length portion in the row data.

     @returns The length in the row plus the size of the data.
  */
  uint32 get_packed_size(const uchar *ptr_arg, bool low_byte_first)
    {return packlength + get_length(ptr_arg, packlength, low_byte_first);}

  inline uint32 get_length(uint row_offset= 0)
  { return get_length(ptr+row_offset, this->packlength, table->s->db_low_byte_first); }
  uint32 get_length(const uchar *ptr, uint packlength, bool low_byte_first);
  uint32 get_length(const uchar *ptr_arg)
  { return get_length(ptr_arg, this->packlength, table->s->db_low_byte_first); }
  void put_length(uchar *pos, uint32 length);
  inline void get_ptr(uchar **str)
    {
      memcpy_fixed((uchar*) str,ptr+packlength,sizeof(uchar*));
    }
  inline void get_ptr(uchar **str, uint row_offset)
    {
      memcpy_fixed((uchar*) str,ptr+packlength+row_offset,sizeof(char*));
    }
  inline void set_ptr(uchar *length, uchar *data)
    {
      memcpy(ptr,length,packlength);
      memcpy_fixed(ptr+packlength,&data,sizeof(char*));
    }
  void set_ptr_offset(my_ptrdiff_t ptr_diff, uint32 length, uchar *data)
    {
      uchar *ptr_ofs= ADD_TO_PTR(ptr,ptr_diff,uchar*);
      store_length(ptr_ofs, packlength, length);
      memcpy_fixed(ptr_ofs+packlength,&data,sizeof(char*));
    }
  inline void set_ptr(uint32 length, uchar *data)
    {
      set_ptr_offset(0, length, data);
    }
  uint get_key_image(uchar *buff,uint length, imagetype type);
  void set_key_image(const uchar *buff,uint length);
  void sql_type(String &str) const;
  inline bool copy()
  {
    uchar *tmp;
    get_ptr(&tmp);
    if (value.copy((char*) tmp, get_length(), charset()))
    {
      Field_blob::reset();
      return 1;
    }
    tmp=(uchar*) value.ptr();
    memcpy_fixed(ptr+packlength,&tmp,sizeof(char*));
    return 0;
  }
  virtual uchar *pack(uchar *to, const uchar *from,
                      uint max_length, bool low_byte_first);
  uchar *pack_key(uchar *to, const uchar *from,
                  uint max_length, bool low_byte_first);
  uchar *pack_key_from_key_image(uchar* to, const uchar *from,
                                 uint max_length, bool low_byte_first);
  virtual const uchar *unpack(uchar *to, const uchar *from,
                              uint param_data, bool low_byte_first);
  const uchar *unpack_key(uchar* to, const uchar *from,
                          uint max_length, bool low_byte_first);
  int pack_cmp(const uchar *a, const uchar *b, uint key_length,
               my_bool insert_or_update);
  int pack_cmp(const uchar *b, uint key_length,my_bool insert_or_update);
  uint packed_col_length(const uchar *col_ptr, uint length);
  uint max_packed_col_length(uint max_length);
  void free() { value.free(); }
  inline void clear_temporary() { bzero((uchar*) &value,sizeof(value)); }
  friend int field_conv(Field *to,Field *from);
  uint size_of() const { return sizeof(*this); }
  bool has_charset(void) const
  { return charset() == &my_charset_bin ? FALSE : TRUE; }
  uint32 max_display_length();
  uint is_equal(Create_field *new_field);
  inline bool in_read_set() { return bitmap_is_set(table->read_set, field_index); }
  inline bool in_write_set() { return bitmap_is_set(table->write_set, field_index); }
private:
  int do_save_field_metadata(uchar *first_byte);
};


#ifdef HAVE_SPATIAL
class Field_geom :public Field_blob {
public:
  enum geometry_type geom_type;

  Field_geom(uchar *ptr_arg, uchar *null_ptr_arg, uint null_bit_arg,
	     enum utype unireg_check_arg, const char *field_name_arg,
	     TABLE_SHARE *share, uint blob_pack_length,
	     enum geometry_type geom_type_arg)
     :Field_blob(ptr_arg, null_ptr_arg, null_bit_arg, unireg_check_arg, 
                 field_name_arg, share, blob_pack_length, &my_charset_bin)
  { geom_type= geom_type_arg; }
  Field_geom(uint32 len_arg,bool maybe_null_arg, const char *field_name_arg,
	     TABLE_SHARE *share, enum geometry_type geom_type_arg)
    :Field_blob(len_arg, maybe_null_arg, field_name_arg, &my_charset_bin)
  { geom_type= geom_type_arg; }
  enum ha_base_keytype key_type() const { return HA_KEYTYPE_VARBINARY2; }
  enum_field_types type() const { return MYSQL_TYPE_GEOMETRY; }
  void sql_type(String &str) const;
  int  store(const char *to, uint length, CHARSET_INFO *charset);
  int  store(double nr);
  int  store(longlong nr, bool unsigned_val);
  int  store_decimal(const my_decimal *);
  uint size_of() const { return sizeof(*this); }
  int  reset(void) { return !maybe_null() || Field_blob::reset(); }
  geometry_type get_geometry_type() { return geom_type; };
};
#endif /*HAVE_SPATIAL*/


class Field_enum :public Field_str {
protected:
  uint packlength;
public:
  TYPELIB *typelib;
  Field_enum(uchar *ptr_arg, uint32 len_arg, uchar *null_ptr_arg,
             uchar null_bit_arg,
             enum utype unireg_check_arg, const char *field_name_arg,
             uint packlength_arg,
             TYPELIB *typelib_arg,
             CHARSET_INFO *charset_arg)
    :Field_str(ptr_arg, len_arg, null_ptr_arg, null_bit_arg,
	       unireg_check_arg, field_name_arg, charset_arg),
    packlength(packlength_arg),typelib(typelib_arg)
  {
      flags|=ENUM_FLAG;
  }
  Field *new_field(MEM_ROOT *root, struct st_table *new_table, bool keep_type);
  enum_field_types type() const { return MYSQL_TYPE_STRING; }
  enum Item_result cmp_type () const { return INT_RESULT; }
  enum Item_result cast_to_int_type () const { return INT_RESULT; }
  enum ha_base_keytype key_type() const;
  int  store(const char *to,uint length,CHARSET_INFO *charset);
  int  store(double nr);
  int  store(longlong nr, bool unsigned_val);
  double val_real(void);
  longlong val_int(void);
  String *val_str(String*,String *);
  int cmp(const uchar *,const uchar *);
  void sort_string(uchar *buff,uint length);
  uint32 pack_length() const { return (uint32) packlength; }
  void store_type(ulonglong value);
  void sql_type(String &str) const;
  uint size_of() const { return sizeof(*this); }
  enum_field_types real_type() const { return MYSQL_TYPE_ENUM; }
  uint pack_length_from_metadata(uint field_metadata)
  { return (field_metadata & 0x00ff); }
  uint row_pack_length() { return pack_length(); }
  virtual bool zero_pack() const { return 0; }
  bool optimize_range(uint idx, uint part) { return 0; }
  bool eq_def(Field *field);
  bool has_charset(void) const { return TRUE; }
  /* enum and set are sorted as integers */
  CHARSET_INFO *sort_charset(void) const { return &my_charset_bin; }
private:
  int do_save_field_metadata(uchar *first_byte);
  uint is_equal(Create_field *new_field);
};


class Field_set :public Field_enum {
public:
  Field_set(uchar *ptr_arg, uint32 len_arg, uchar *null_ptr_arg,
	    uchar null_bit_arg,
	    enum utype unireg_check_arg, const char *field_name_arg,
	    uint32 packlength_arg,
	    TYPELIB *typelib_arg, CHARSET_INFO *charset_arg)
    :Field_enum(ptr_arg, len_arg, null_ptr_arg, null_bit_arg,
		    unireg_check_arg, field_name_arg,
                packlength_arg,
                typelib_arg,charset_arg)
    {
      flags=(flags & ~ENUM_FLAG) | SET_FLAG;
    }
  int  store(const char *to,uint length,CHARSET_INFO *charset);
  int  store(double nr) { return Field_set::store((longlong) nr, FALSE); }
  int  store(longlong nr, bool unsigned_val);

  virtual bool zero_pack() const { return 1; }
  String *val_str(String*,String *);
  void sql_type(String &str) const;
  enum_field_types real_type() const { return MYSQL_TYPE_SET; }
  bool has_charset(void) const { return TRUE; }
};


/*
  Note:
    To use Field_bit::cmp_binary() you need to copy the bits stored in
    the beginning of the record (the NULL bytes) to each memory you
    want to compare (where the arguments point).

    This is the reason:
    - Field_bit::cmp_binary() is only implemented in the base class
      (Field::cmp_binary()).
    - Field::cmp_binary() currenly use pack_length() to calculate how
      long the data is.
    - pack_length() includes size of the bits stored in the NULL bytes
      of the record.
*/
class Field_bit :public Field {
public:
  uchar *bit_ptr;     // position in record where 'uneven' bits store
  uchar bit_ofs;      // offset to 'uneven' high bits
  uint bit_len;       // number of 'uneven' high bits
  uint bytes_in_rec;
  Field_bit(uchar *ptr_arg, uint32 len_arg, uchar *null_ptr_arg,
            uchar null_bit_arg, uchar *bit_ptr_arg, uchar bit_ofs_arg,
            enum utype unireg_check_arg, const char *field_name_arg);
  enum_field_types type() const { return MYSQL_TYPE_BIT; }
  enum ha_base_keytype key_type() const { return HA_KEYTYPE_BIT; }
  uint32 key_length() const { return (uint32) (field_length + 7) / 8; }
  uint32 max_data_length() const { return (field_length + 7) / 8; }
  uint32 max_display_length() { return field_length; }
  uint size_of() const { return sizeof(*this); }
  Item_result result_type () const { return INT_RESULT; }
  int reset(void) { 
    bzero(ptr, bytes_in_rec); 
    if (bit_ptr && (bit_len > 0))  // reset odd bits among null bits
      clr_rec_bits(bit_ptr, bit_ofs, bit_len);
    return 0; 
  }
  int store(const char *to, uint length, CHARSET_INFO *charset);
  int store(double nr);
  int store(longlong nr, bool unsigned_val);
  int store_decimal(const my_decimal *);
  double val_real(void);
  longlong val_int(void);
  String *val_str(String*, String *);
  virtual bool str_needs_quotes() { return TRUE; }
  my_decimal *val_decimal(my_decimal *);
  int cmp(const uchar *a, const uchar *b)
  { 
    DBUG_ASSERT(ptr == a);
    return Field_bit::key_cmp(b, bytes_in_rec+test(bit_len));
  }
  int cmp_binary_offset(uint row_offset)
  { return cmp_offset(row_offset); }
  int cmp_max(const uchar *a, const uchar *b, uint max_length);
  int key_cmp(const uchar *a, const uchar *b)
  { return cmp_binary((uchar *) a, (uchar *) b); }
  int key_cmp(const uchar *str, uint length);
  int cmp_offset(uint row_offset);
  void get_image(uchar *buff, uint length, CHARSET_INFO *cs)
  { get_key_image(buff, length, itRAW); }   
  void set_image(const uchar *buff,uint length, CHARSET_INFO *cs)
  { Field_bit::store((char *) buff, length, cs); }
  uint get_key_image(uchar *buff, uint length, imagetype type);
  void set_key_image(const uchar *buff, uint length)
  { Field_bit::store((char*) buff, length, &my_charset_bin); }
  void sort_string(uchar *buff, uint length)
  { get_key_image(buff, length, itRAW); }
  uint32 pack_length() const { return (uint32) (field_length + 7) / 8; }
  uint32 pack_length_in_rec() const { return bytes_in_rec; }
  uint pack_length_from_metadata(uint field_metadata);
  uint row_pack_length()
  { return (bytes_in_rec + ((bit_len > 0) ? 1 : 0)); }
  int compatible_field_size(uint field_metadata,
                            const Relay_log_info *rli, uint16 mflags);
  void sql_type(String &str) const;
  virtual uchar *pack(uchar *to, const uchar *from,
                      uint max_length, bool low_byte_first);
  virtual const uchar *unpack(uchar *to, const uchar *from,
                              uint param_data, bool low_byte_first);
  virtual void set_default();

  Field *new_key_field(MEM_ROOT *root, struct st_table *new_table,
                       uchar *new_ptr, uchar *new_null_ptr,
                       uint new_null_bit);
  void set_bit_ptr(uchar *bit_ptr_arg, uchar bit_ofs_arg)
  {
    bit_ptr= bit_ptr_arg;
    bit_ofs= bit_ofs_arg;
  }
  bool eq(Field *field)
  {
    return (Field::eq(field) &&
            bit_ptr == ((Field_bit *)field)->bit_ptr &&
            bit_ofs == ((Field_bit *)field)->bit_ofs);
  }
  uint is_equal(Create_field *new_field);
  void move_field_offset(my_ptrdiff_t ptr_diff)
  {
    Field::move_field_offset(ptr_diff);
    bit_ptr= ADD_TO_PTR(bit_ptr, ptr_diff, uchar*);
  }
  void hash(ulong *nr, ulong *nr2);

private:
  virtual size_t do_last_null_byte() const;
  int do_save_field_metadata(uchar *first_byte);
};


/**
  BIT field represented as chars for non-MyISAM tables.

  @todo The inheritance relationship is backwards since Field_bit is
  an extended version of Field_bit_as_char and not the other way
  around. Hence, we should refactor it to fix the hierarchy order.
 */
class Field_bit_as_char: public Field_bit {
public:
  Field_bit_as_char(uchar *ptr_arg, uint32 len_arg, uchar *null_ptr_arg,
                    uchar null_bit_arg,
                    enum utype unireg_check_arg, const char *field_name_arg);
  enum ha_base_keytype key_type() const { return HA_KEYTYPE_BINARY; }
  uint size_of() const { return sizeof(*this); }
  int store(const char *to, uint length, CHARSET_INFO *charset);
  int store(double nr) { return Field_bit::store(nr); }
  int store(longlong nr, bool unsigned_val)
  { return Field_bit::store(nr, unsigned_val); }
  void sql_type(String &str) const;
};


/*
  Create field class for CREATE TABLE
*/

class Create_field :public Sql_alloc
{
public:
  const char *field_name;
  const char *change;			// If done with alter table
  const char *after;			// Put column after this one
  LEX_STRING comment;			// Comment for field
  Item	*def;				// Default value
  enum	enum_field_types sql_type;
  /*
    At various stages in execution this can be length of field in bytes or
    max number of characters. 
  */
  ulong length;
  /*
    The value of `length' as set by parser: is the number of characters
    for most of the types, or of bytes for BLOBs or numeric types.
  */
  uint32 char_length;
  uint  decimals, flags, pack_length, key_length;
  Field::utype unireg_check;
  TYPELIB *interval;			// Which interval to use
  TYPELIB *save_interval;               // Temporary copy for the above
                                        // Used only for UCS2 intervals
  List<String> interval_list;
  CHARSET_INFO *charset;
  Field::geometry_type geom_type;
  Field *field;				// For alter table

  uint8 row,col,sc_length,interval_id;	// For rea_create_table
  uint	offset,pack_flag;
  Create_field() :after(0) {}
  Create_field(Field *field, Field *orig_field);
  /* Used to make a clone of this object for ALTER/CREATE TABLE */
  Create_field *clone(MEM_ROOT *mem_root) const
    { return new (mem_root) Create_field(*this); }
  void create_length_to_internal_length(void);

  /* Init for a tmp table field. To be extended if need be. */
  void init_for_tmp_table(enum_field_types sql_type_arg,
                          uint32 max_length, uint32 decimals,
                          bool maybe_null, bool is_unsigned);

  bool init(THD *thd, char *field_name, enum_field_types type, char *length,
            char *decimals, uint type_modifier, Item *default_value,
            Item *on_update_value, LEX_STRING *comment, char *change,
            List<String> *interval_list, CHARSET_INFO *cs,
            uint uint_geom_type);

  bool field_flags_are_binary()
  {
    return (flags & (BINCMP_FLAG | BINARY_FLAG)) != 0;
  }
};


/*
  A class for sending info to the client
*/

class Send_field {
 public:
  const char *db_name;
  const char *table_name,*org_table_name;
  const char *col_name,*org_col_name;
  ulong length;
  uint charsetnr, flags, decimals;
  enum_field_types type;
  Send_field() {}
};


/*
  A class for quick copying data to fields
*/

class Copy_field :public Sql_alloc {
  /**
    Convenience definition of a copy function returned by
    get_copy_func.
  */
  typedef void Copy_func(Copy_field*);
  Copy_func *get_copy_func(Field *to, Field *from);
public:
  uchar *from_ptr,*to_ptr;
  uchar *from_null_ptr,*to_null_ptr;
  my_bool *null_row;
  uint	from_bit,to_bit;
  uint from_length,to_length;
  Field *from_field,*to_field;
  String tmp;					// For items

  Copy_field() {}
  ~Copy_field() {}
  void set(Field *to,Field *from,bool save);	// Field to field 
  void set(uchar *to,Field *from);		// Field to string
  void (*do_copy)(Copy_field *);
  void (*do_copy2)(Copy_field *);		// Used to handle null values
};


Field *make_field(TABLE_SHARE *share, uchar *ptr, uint32 field_length,
		  uchar *null_pos, uchar null_bit,
		  uint pack_flag, enum_field_types field_type,
		  CHARSET_INFO *cs,
		  Field::geometry_type geom_type,
		  Field::utype unireg_check,
		  TYPELIB *interval, const char *field_name);
uint pack_length_to_packflag(uint type);
enum_field_types get_blob_type_from_length(ulong length);
uint32 calc_pack_length(enum_field_types type,uint32 length);
int set_field_to_null(Field *field);
int set_field_to_null_with_conversions(Field *field, bool no_conversions);

/*
  The following are for the interface with the .frm file
*/

#define FIELDFLAG_DECIMAL		1
#define FIELDFLAG_BINARY		1	// Shares same flag
#define FIELDFLAG_NUMBER		2
#define FIELDFLAG_ZEROFILL		4
#define FIELDFLAG_PACK			120	// Bits used for packing
#define FIELDFLAG_INTERVAL		256     // mangled with decimals!
#define FIELDFLAG_BITFIELD		512	// mangled with decimals!
#define FIELDFLAG_BLOB			1024	// mangled with decimals!
#define FIELDFLAG_GEOM			2048    // mangled with decimals!

#define FIELDFLAG_TREAT_BIT_AS_CHAR     4096    /* use Field_bit_as_char */

#define FIELDFLAG_LEFT_FULLSCREEN	8192
#define FIELDFLAG_RIGHT_FULLSCREEN	16384
#define FIELDFLAG_FORMAT_NUMBER		16384	// predit: ###,,## in output
#define FIELDFLAG_NO_DEFAULT		16384   /* sql */
#define FIELDFLAG_SUM			((uint) 32768)// predit: +#fieldflag
#define FIELDFLAG_MAYBE_NULL		((uint) 32768)// sql
#define FIELDFLAG_HEX_ESCAPE		((uint) 0x10000)
#define FIELDFLAG_PACK_SHIFT		3
#define FIELDFLAG_DEC_SHIFT		8
#define FIELDFLAG_MAX_DEC		31
#define FIELDFLAG_NUM_SCREEN_TYPE	0x7F01
#define FIELDFLAG_ALFA_SCREEN_TYPE	0x7800

#define MTYP_TYPENR(type) (type & 127)	/* Remove bits from type */

#define f_is_dec(x)		((x) & FIELDFLAG_DECIMAL)
#define f_is_num(x)		((x) & FIELDFLAG_NUMBER)
#define f_is_zerofill(x)	((x) & FIELDFLAG_ZEROFILL)
#define f_is_packed(x)		((x) & FIELDFLAG_PACK)
#define f_packtype(x)		(((x) >> FIELDFLAG_PACK_SHIFT) & 15)
#define f_decimals(x)		((uint8) (((x) >> FIELDFLAG_DEC_SHIFT) & FIELDFLAG_MAX_DEC))
#define f_is_alpha(x)		(!f_is_num(x))
#define f_is_binary(x)          ((x) & FIELDFLAG_BINARY) // 4.0- compatibility
#define f_is_enum(x)            (((x) & (FIELDFLAG_INTERVAL | FIELDFLAG_NUMBER)) == FIELDFLAG_INTERVAL)
#define f_is_bitfield(x)        (((x) & (FIELDFLAG_BITFIELD | FIELDFLAG_NUMBER)) == FIELDFLAG_BITFIELD)
#define f_is_blob(x)		(((x) & (FIELDFLAG_BLOB | FIELDFLAG_NUMBER)) == FIELDFLAG_BLOB)
#define f_is_geom(x)		(((x) & (FIELDFLAG_GEOM | FIELDFLAG_NUMBER)) == FIELDFLAG_GEOM)
#define f_is_equ(x)		((x) & (1+2+FIELDFLAG_PACK+31*256))
#define f_settype(x)		(((int) x) << FIELDFLAG_PACK_SHIFT)
#define f_maybe_null(x)		(x & FIELDFLAG_MAYBE_NULL)
#define f_no_default(x)		(x & FIELDFLAG_NO_DEFAULT)
#define f_bit_as_char(x)        ((x) & FIELDFLAG_TREAT_BIT_AS_CHAR)
#define f_is_hex_escape(x)      ((x) & FIELDFLAG_HEX_ESCAPE)
