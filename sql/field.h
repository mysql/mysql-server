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


/*
  Because of the function new_field() all field classes that have static
  variables must declare the size_of() member function.
*/

#ifdef __GNUC__
#pragma interface			/* gcc class implementation */
#endif

#define NOT_FIXED_DEC			31

class Send_field;
class Protocol;
struct st_cache_field;
void field_conv(Field *to,Field *from);

class Field
{
  Field(const Item &);				/* Prevent use of these */
  void operator=(Field &);
public:
  static void *operator new(size_t size) {return (void*) sql_alloc((uint) size); }
  static void operator delete(void *ptr_arg, size_t size) {
#ifdef SAFEMALLOC
    bfill(ptr_arg, size, 0x8F);
#endif
  }

  char		*ptr;			// Position to field in record
  uchar		*null_ptr;		// Byte where null_bit is
  /*
    Note that you can use table->in_use as replacement for current_thd member 
    only inside of val_*() and store() members (e.g. you can't use it in cons)
  */
  struct st_table *table;		// Pointer for table
  struct st_table *orig_table;		// Pointer to original table
  const char	*table_name,*field_name;
  LEX_STRING	comment;
  ulong		query_id;		// For quick test of used fields
  /* Field is part of the following keys */
  key_map	key_start,part_of_key,part_of_sortkey;
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
  enum field_cast_enum
  {
    FIELD_CAST_STOP, FIELD_CAST_DECIMAL, FIELD_CAST_TINY, FIELD_CAST_SHORT,
    FIELD_CAST_MEDIUM, FIELD_CAST_LONG, FIELD_CAST_LONGLONG,
    FIELD_CAST_FLOAT, FIELD_CAST_DOUBLE,
    FIELD_CAST_NULL,
    FIELD_CAST_TIMESTAMP, FIELD_CAST_YEAR, FIELD_CAST_DATE, FIELD_CAST_NEWDATE,
    FIELD_CAST_TIME, FIELD_CAST_DATETIME,
    FIELD_CAST_STRING, FIELD_CAST_VARSTRING, FIELD_CAST_BLOB,
    FIELD_CAST_GEOM, FIELD_CAST_ENUM, FIELD_CAST_SET
  };

  utype		unireg_check;
  uint32	field_length;		// Length of field
  uint16	flags;
  uchar		null_bit;		// Bit used to test null bit

  Field(char *ptr_arg,uint32 length_arg,uchar *null_ptr_arg,uchar null_bit_arg,
	utype unireg_check_arg, const char *field_name_arg,
	struct st_table *table_arg);
  virtual ~Field() {}
  /* Store functions returns 1 on overflow and -1 on fatal error */
  virtual int  store(const char *to,uint length,CHARSET_INFO *cs)=0;
  virtual int  store(double nr)=0;
  virtual int  store(longlong nr)=0;
  virtual void store_time(TIME *ltime,timestamp_type t_type);
  virtual double val_real(void)=0;
  virtual longlong val_int(void)=0;
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
  virtual Item_result result_type () const=0;
  virtual Item_result cmp_type () const { return result_type(); }
  bool eq(Field *field) { return ptr == field->ptr && null_ptr == field->null_ptr; }
  virtual bool eq_def(Field *field);
  virtual uint32 pack_length() const { return (uint32) field_length; }
  virtual void reset(void) { bzero(ptr,pack_length()); }
  virtual void reset_fields() {}
  virtual void set_default()
  {
    my_ptrdiff_t offset = (my_ptrdiff_t) (table->default_values -
					  table->record[0]);
    memcpy(ptr, ptr + offset, pack_length());
    if (null_ptr)
      *null_ptr= ((*null_ptr & (uchar) ~null_bit) |
		  null_ptr[offset] & null_bit);
  }
  virtual bool binary() const { return 1; }
  virtual bool zero_pack() const { return 1; }
  virtual enum ha_base_keytype key_type() const { return HA_KEYTYPE_BINARY; }
  virtual uint32 key_length() const { return pack_length(); }
  virtual enum_field_types type() const =0;
  virtual enum_field_types real_type() const { return type(); }
  inline  int cmp(const char *str) { return cmp(ptr,str); }
  virtual int cmp(const char *,const char *)=0;
  virtual int cmp_binary(const char *a,const char *b, uint32 max_length=~0L)
  { return memcmp(a,b,pack_length()); }
  virtual int cmp_offset(uint row_offset)
  { return memcmp(ptr,ptr+row_offset,pack_length()); }
  virtual int cmp_binary_offset(uint row_offset)
  { return memcmp(ptr,ptr+row_offset,pack_length()); }
  virtual int key_cmp(const byte *a,const byte *b)
  { return cmp((char*) a,(char*) b); }
  virtual int key_cmp(const byte *str, uint length)
  { return cmp(ptr,(char*) str); }
  virtual uint decimals() const { return 0; }
  /*
    Caller beware: sql_type can change str.Ptr, so check
    ptr() to see if it changed if you are using your own buffer
    in str and restore it with set() if needed
  */
  virtual void sql_type(String &str) const =0;
  virtual uint size_of() const =0;		// For new field
  inline bool is_null(uint row_offset=0)
  { return null_ptr ? (null_ptr[row_offset] & null_bit ? 1 : 0) : table->null_row; }
  inline bool is_real_null(uint row_offset=0)
    { return null_ptr ? (null_ptr[row_offset] & null_bit ? 1 : 0) : 0; }
  inline bool is_null_in_record(const uchar *record)
  {
    if (!null_ptr)
      return 0;
    return test(record[(uint) (null_ptr - (uchar*) table->record[0])] &
		null_bit);
  }
  inline void set_null(int row_offset=0)
    { if (null_ptr) null_ptr[row_offset]|= null_bit; }
  inline void set_notnull(int row_offset=0)
    { if (null_ptr) null_ptr[row_offset]&= (uchar) ~null_bit; }
  inline bool maybe_null(void) { return null_ptr != 0 || table->maybe_null; }
  inline bool real_maybe_null(void) { return null_ptr != 0; }
  virtual void make_field(Send_field *)=0;
  virtual void sort_string(char *buff,uint length)=0;
  virtual bool optimize_range(uint idx, uint part);
  virtual bool store_for_compare() { return 0; }
  virtual void free() {}
  Field *new_field(MEM_ROOT *root, struct st_table *new_table)
  {
    Field *tmp= (Field*) memdup_root(root,(char*) this,size_of());
    if (tmp)
    {
      if (tmp->table->maybe_null)
	tmp->flags&= ~NOT_NULL_FLAG;
      tmp->table= new_table;
      tmp->key_start.init(0);
      tmp->part_of_key.init(0);
      tmp->part_of_sortkey.init(0);
      tmp->unireg_check=Field::NONE;
      tmp->flags&= (NOT_NULL_FLAG | BLOB_FLAG | UNSIGNED_FLAG |
		    ZEROFILL_FLAG | BINARY_FLAG | ENUM_FLAG | SET_FLAG);
#ifdef PROBABLY_WRONG
      tmp->table_name= new_table->table_name;
#endif
      tmp->reset_fields();
    }
    return tmp;
  }
  inline void move_field(char *ptr_arg,uchar *null_ptr_arg,uchar null_bit_arg)
  {
    ptr=ptr_arg; null_ptr=null_ptr_arg; null_bit=null_bit_arg;
  }
  inline void move_field(char *ptr_arg) { ptr=ptr_arg; }
  inline void move_field(my_ptrdiff_t ptr_diff)
  {
    ptr=ADD_TO_PTR(ptr,ptr_diff,char*);
    if (null_ptr)
      null_ptr=ADD_TO_PTR(null_ptr,ptr_diff,uchar*);
  }
  inline void get_image(char *buff,uint length, CHARSET_INFO *cs)
    { memcpy(buff,ptr,length); }
  inline void set_image(char *buff,uint length, CHARSET_INFO *cs)
    { memcpy(ptr,buff,length); }
  virtual void get_key_image(char *buff,uint length, CHARSET_INFO *cs,
			     imagetype type)
    { get_image(buff,length,cs); }
  virtual void set_key_image(char *buff,uint length, CHARSET_INFO *cs)
    { set_image(buff,length,cs); }
  inline longlong val_int_offset(uint row_offset)
    {
      ptr+=row_offset;
      longlong tmp=val_int();
      ptr-=row_offset;
      return tmp;
    }
  virtual bool send_binary(Protocol *protocol);
  virtual char *pack(char* to, const char *from, uint max_length=~(uint) 0)
  {
    uint32 length=pack_length();
    memcpy(to,from,length);
    return to+length;
  }
  virtual const char *unpack(char* to, const char *from)
  {
    uint length=pack_length();
    memcpy(to,from,length);
    return from+length;
  }
  virtual char *pack_key(char* to, const char *from, uint max_length)
  {
    return pack(to,from,max_length);
  }
  virtual char *pack_key_from_key_image(char* to, const char *from,
					uint max_length)
  {
    return pack(to,from,max_length);
  }
  virtual const char *unpack_key(char* to, const char *from, uint max_length)
  {
    return unpack(to,from);
  }
  virtual uint packed_col_length(const char *to, uint length)
  { return length;}
  virtual uint max_packed_col_length(uint max_length)
  { return max_length;}

  virtual int pack_cmp(const char *a,const char *b, uint key_length_arg)
  { return cmp(a,b); }
  virtual int pack_cmp(const char *b, uint key_length_arg)
  { return cmp(ptr,b); }
  uint offset();			// Should be inline ...
  void copy_from_tmp(int offset);
  uint fill_cache_field(struct st_cache_field *copy);
  virtual bool get_date(TIME *ltime,uint fuzzydate);
  virtual bool get_time(TIME *ltime);
  virtual CHARSET_INFO *charset(void) const { return &my_charset_bin; }
  virtual bool has_charset(void) const { return FALSE; }
  virtual void set_charset(CHARSET_INFO *charset) { }
  bool set_warning(const unsigned int level, const unsigned int code, 
                   int cuted_increment);
  bool check_int(const char *str, int length, const char *int_end,
                 CHARSET_INFO *cs);
  void set_datetime_warning(const uint level, const uint code, 
                            const char *str, uint str_len,
                            timestamp_type ts_type, int cuted_increment);
  void set_datetime_warning(const uint level, const uint code, 
                            longlong nr, timestamp_type ts_type,
                            int cuted_increment);
  void set_datetime_warning(const uint level, const uint code, 
                            double nr, timestamp_type ts_type);
  virtual field_cast_enum field_cast_type()= 0;
  bool field_cast_compatible(field_cast_enum type);
  /* maximum possible display length */
  virtual uint32 max_length()= 0;
  friend bool reopen_table(THD *,struct st_table *,bool);
  friend int cre_myisam(my_string name, register TABLE *form, uint options,
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
};


class Field_num :public Field {
public:
  const uint8 dec;
  bool zerofill,unsigned_flag;	// Purify cannot handle bit fields
  Field_num(char *ptr_arg,uint32 len_arg, uchar *null_ptr_arg,
	    uchar null_bit_arg, utype unireg_check_arg,
	    const char *field_name_arg,
	    struct st_table *table_arg,
	    uint8 dec_arg,bool zero_arg,bool unsigned_arg)
    :Field(ptr_arg, len_arg, null_ptr_arg, null_bit_arg,
	   unireg_check_arg, field_name_arg, table_arg),
     dec(dec_arg),zerofill(zero_arg),unsigned_flag(unsigned_arg)
    {
      if (zerofill)
	flags|=ZEROFILL_FLAG;
      if (unsigned_flag)
	flags|=UNSIGNED_FLAG;
    }
  Item_result result_type () const { return REAL_RESULT; }
  void prepend_zeros(String *value);
  void add_zerofill_and_unsigned(String &res) const;
  friend class create_field;
  void make_field(Send_field *);
  uint decimals() const { return (uint) dec; }
  uint size_of() const { return sizeof(*this); }
  bool eq_def(Field *field);
};


class Field_str :public Field {
protected:
  CHARSET_INFO *field_charset;
public:
  Field_str(char *ptr_arg,uint32 len_arg, uchar *null_ptr_arg,
	    uchar null_bit_arg, utype unireg_check_arg,
	    const char *field_name_arg,
	    struct st_table *table_arg,CHARSET_INFO *charset)
    :Field(ptr_arg, len_arg, null_ptr_arg, null_bit_arg,
	   unireg_check_arg, field_name_arg, table_arg)
    {
      field_charset=charset;
      if (charset->state & MY_CS_BINSORT)
        flags|=BINARY_FLAG;
    }
  Item_result result_type () const { return STRING_RESULT; }
  uint decimals() const { return NOT_FIXED_DEC; }
  int  store(double nr);
  int  store(longlong nr)=0;
  int  store(const char *to,uint length,CHARSET_INFO *cs)=0;
  void make_field(Send_field *);
  uint size_of() const { return sizeof(*this); }
  CHARSET_INFO *charset(void) const { return field_charset; }
  void set_charset(CHARSET_INFO *charset) { field_charset=charset; }
  bool binary() const { return field_charset == &my_charset_bin; }
  uint32 max_length() { return field_length; }
  friend class create_field;
};


class Field_decimal :public Field_num {
public:
  Field_decimal(char *ptr_arg, uint32 len_arg, uchar *null_ptr_arg,
		uchar null_bit_arg,
		enum utype unireg_check_arg, const char *field_name_arg,
		struct st_table *table_arg,
		uint8 dec_arg,bool zero_arg,bool unsigned_arg)
    :Field_num(ptr_arg, len_arg, null_ptr_arg, null_bit_arg,
	       unireg_check_arg, field_name_arg, table_arg,
	       dec_arg, zero_arg,unsigned_arg)
    {}
  enum_field_types type() const { return FIELD_TYPE_DECIMAL;}
  enum ha_base_keytype key_type() const
  { return zerofill ? HA_KEYTYPE_BINARY : HA_KEYTYPE_NUM; }
  void reset(void);
  int  store(const char *to,uint length,CHARSET_INFO *charset);
  int  store(double nr);
  int  store(longlong nr);
  double val_real(void);
  longlong val_int(void);
  String *val_str(String*,String *);
  int cmp(const char *,const char*);
  void sort_string(char *buff,uint length);
  void overflow(bool negative);
  bool zero_pack() const { return 0; }
  void sql_type(String &str) const;
  uint32 max_length() { return field_length; }
  field_cast_enum field_cast_type() { return FIELD_CAST_DECIMAL; }
};


class Field_tiny :public Field_num {
public:
  Field_tiny(char *ptr_arg, uint32 len_arg, uchar *null_ptr_arg,
	     uchar null_bit_arg,
	     enum utype unireg_check_arg, const char *field_name_arg,
	     struct st_table *table_arg,
	     bool zero_arg, bool unsigned_arg)
    :Field_num(ptr_arg, len_arg, null_ptr_arg, null_bit_arg,
	       unireg_check_arg, field_name_arg, table_arg,
	       0, zero_arg,unsigned_arg)
    {}
  enum Item_result result_type () const { return INT_RESULT; }
  enum_field_types type() const { return FIELD_TYPE_TINY;}
  enum ha_base_keytype key_type() const
    { return unsigned_flag ? HA_KEYTYPE_BINARY : HA_KEYTYPE_INT8; }
  int  store(const char *to,uint length,CHARSET_INFO *charset);
  int  store(double nr);
  int  store(longlong nr);
  void reset(void) { ptr[0]=0; }
  double val_real(void);
  longlong val_int(void);
  String *val_str(String*,String *);
  bool send_binary(Protocol *protocol);
  int cmp(const char *,const char*);
  void sort_string(char *buff,uint length);
  uint32 pack_length() const { return 1; }
  void sql_type(String &str) const;
  uint32 max_length() { return 4; }
  field_cast_enum field_cast_type() { return FIELD_CAST_TINY; }
};


class Field_short :public Field_num {
public:
  Field_short(char *ptr_arg, uint32 len_arg, uchar *null_ptr_arg,
	      uchar null_bit_arg,
	      enum utype unireg_check_arg, const char *field_name_arg,
	      struct st_table *table_arg,
	      bool zero_arg, bool unsigned_arg)
    :Field_num(ptr_arg, len_arg, null_ptr_arg, null_bit_arg,
	       unireg_check_arg, field_name_arg, table_arg,
	       0, zero_arg,unsigned_arg)
    {}
  Field_short(uint32 len_arg,bool maybe_null_arg, const char *field_name_arg,
	      struct st_table *table_arg,bool unsigned_arg)
    :Field_num((char*) 0, len_arg, maybe_null_arg ? (uchar*) "": 0,0,
	       NONE, field_name_arg, table_arg,0,0,unsigned_arg)
    {}
  enum Item_result result_type () const { return INT_RESULT; }
  enum_field_types type() const { return FIELD_TYPE_SHORT;}
  enum ha_base_keytype key_type() const
    { return unsigned_flag ? HA_KEYTYPE_USHORT_INT : HA_KEYTYPE_SHORT_INT;}
  int  store(const char *to,uint length,CHARSET_INFO *charset);
  int  store(double nr);
  int  store(longlong nr);
  void reset(void) { ptr[0]=ptr[1]=0; }
  double val_real(void);
  longlong val_int(void);
  String *val_str(String*,String *);
  bool send_binary(Protocol *protocol);
  int cmp(const char *,const char*);
  void sort_string(char *buff,uint length);
  uint32 pack_length() const { return 2; }
  void sql_type(String &str) const;
  uint32 max_length() { return 6; }
  field_cast_enum field_cast_type() { return FIELD_CAST_SHORT; }
};


class Field_medium :public Field_num {
public:
  Field_medium(char *ptr_arg, uint32 len_arg, uchar *null_ptr_arg,
	      uchar null_bit_arg,
	      enum utype unireg_check_arg, const char *field_name_arg,
	      struct st_table *table_arg,
	      bool zero_arg, bool unsigned_arg)
    :Field_num(ptr_arg, len_arg, null_ptr_arg, null_bit_arg,
	       unireg_check_arg, field_name_arg, table_arg,
	       0, zero_arg,unsigned_arg)
    {}
  enum Item_result result_type () const { return INT_RESULT; }
  enum_field_types type() const { return FIELD_TYPE_INT24;}
  enum ha_base_keytype key_type() const
    { return unsigned_flag ? HA_KEYTYPE_UINT24 : HA_KEYTYPE_INT24; }
  int  store(const char *to,uint length,CHARSET_INFO *charset);
  int  store(double nr);
  int store(longlong nr);
  void reset(void) { ptr[0]=ptr[1]=ptr[2]=0; }
  double val_real(void);
  longlong val_int(void);
  String *val_str(String*,String *);
  bool send_binary(Protocol *protocol);
  int cmp(const char *,const char*);
  void sort_string(char *buff,uint length);
  uint32 pack_length() const { return 3; }
  void sql_type(String &str) const;
  uint32 max_length() { return 8; }
  field_cast_enum field_cast_type() { return FIELD_CAST_MEDIUM; }
};


class Field_long :public Field_num {
public:
  Field_long(char *ptr_arg, uint32 len_arg, uchar *null_ptr_arg,
	     uchar null_bit_arg,
	     enum utype unireg_check_arg, const char *field_name_arg,
	     struct st_table *table_arg,
	     bool zero_arg, bool unsigned_arg)
    :Field_num(ptr_arg, len_arg, null_ptr_arg, null_bit_arg,
	       unireg_check_arg, field_name_arg, table_arg,
	       0, zero_arg,unsigned_arg)
    {}
  Field_long(uint32 len_arg,bool maybe_null_arg, const char *field_name_arg,
	     struct st_table *table_arg,bool unsigned_arg)
    :Field_num((char*) 0, len_arg, maybe_null_arg ? (uchar*) "": 0,0,
	       NONE, field_name_arg, table_arg,0,0,unsigned_arg)
    {}
  enum Item_result result_type () const { return INT_RESULT; }
  enum_field_types type() const { return FIELD_TYPE_LONG;}
  enum ha_base_keytype key_type() const
    { return unsigned_flag ? HA_KEYTYPE_ULONG_INT : HA_KEYTYPE_LONG_INT; }
  int  store(const char *to,uint length,CHARSET_INFO *charset);
  int  store(double nr);
  int  store(longlong nr);
  void reset(void) { ptr[0]=ptr[1]=ptr[2]=ptr[3]=0; }
  double val_real(void);
  longlong val_int(void);
  bool send_binary(Protocol *protocol);
  String *val_str(String*,String *);
  int cmp(const char *,const char*);
  void sort_string(char *buff,uint length);
  uint32 pack_length() const { return 4; }
  void sql_type(String &str) const;
  uint32 max_length() { return 11; }
  field_cast_enum field_cast_type() { return FIELD_CAST_LONG; }
};


#ifdef HAVE_LONG_LONG
class Field_longlong :public Field_num {
public:
  Field_longlong(char *ptr_arg, uint32 len_arg, uchar *null_ptr_arg,
	      uchar null_bit_arg,
	      enum utype unireg_check_arg, const char *field_name_arg,
	      struct st_table *table_arg,
	      bool zero_arg, bool unsigned_arg)
    :Field_num(ptr_arg, len_arg, null_ptr_arg, null_bit_arg,
	       unireg_check_arg, field_name_arg, table_arg,
	       0, zero_arg,unsigned_arg)
    {}
  Field_longlong(uint32 len_arg,bool maybe_null_arg,
		 const char *field_name_arg,
		 struct st_table *table_arg, bool unsigned_arg)
    :Field_num((char*) 0, len_arg, maybe_null_arg ? (uchar*) "": 0,0,
	       NONE, field_name_arg, table_arg,0,0,unsigned_arg)
    {}
  enum Item_result result_type () const { return INT_RESULT; }
  enum_field_types type() const { return FIELD_TYPE_LONGLONG;}
  enum ha_base_keytype key_type() const
    { return unsigned_flag ? HA_KEYTYPE_ULONGLONG : HA_KEYTYPE_LONGLONG; }
  int  store(const char *to,uint length,CHARSET_INFO *charset);
  int  store(double nr);
  int  store(longlong nr);
  void reset(void) { ptr[0]=ptr[1]=ptr[2]=ptr[3]=ptr[4]=ptr[5]=ptr[6]=ptr[7]=0; }
  double val_real(void);
  longlong val_int(void);
  String *val_str(String*,String *);
  bool send_binary(Protocol *protocol);
  int cmp(const char *,const char*);
  void sort_string(char *buff,uint length);
  uint32 pack_length() const { return 8; }
  void sql_type(String &str) const;
  bool store_for_compare() { return 1; }
  uint32 max_length() { return 20; }
  field_cast_enum field_cast_type() { return FIELD_CAST_LONGLONG; }
};
#endif

class Field_float :public Field_num {
public:
  Field_float(char *ptr_arg, uint32 len_arg, uchar *null_ptr_arg,
	      uchar null_bit_arg,
	      enum utype unireg_check_arg, const char *field_name_arg,
	      struct st_table *table_arg,
	      uint8 dec_arg,bool zero_arg,bool unsigned_arg)
    :Field_num(ptr_arg, len_arg, null_ptr_arg, null_bit_arg,
	       unireg_check_arg, field_name_arg, table_arg,
	       dec_arg, zero_arg,unsigned_arg)
    {}
  Field_float(uint32 len_arg, bool maybe_null_arg, const char *field_name_arg,
	      struct st_table *table_arg, uint8 dec_arg)
    :Field_num((char*) 0, len_arg, maybe_null_arg ? (uchar*) "": 0, (uint) 0,
	       NONE, field_name_arg, table_arg,dec_arg,0,0)
    {}
  enum_field_types type() const { return FIELD_TYPE_FLOAT;}
  enum ha_base_keytype key_type() const { return HA_KEYTYPE_FLOAT; }
  int  store(const char *to,uint length,CHARSET_INFO *charset);
  int  store(double nr);
  int  store(longlong nr);
  void reset(void) { bzero(ptr,sizeof(float)); }
  double val_real(void);
  longlong val_int(void);
  String *val_str(String*,String *);
  bool send_binary(Protocol *protocol);
  int cmp(const char *,const char*);
  void sort_string(char *buff,uint length);
  uint32 pack_length() const { return sizeof(float); }
  void sql_type(String &str) const;
  uint32 max_length() { return 24; }
  field_cast_enum field_cast_type() { return FIELD_CAST_FLOAT; }
};


class Field_double :public Field_num {
public:
  Field_double(char *ptr_arg, uint32 len_arg, uchar *null_ptr_arg,
	       uchar null_bit_arg,
	       enum utype unireg_check_arg, const char *field_name_arg,
	       struct st_table *table_arg,
	       uint8 dec_arg,bool zero_arg,bool unsigned_arg)
    :Field_num(ptr_arg, len_arg, null_ptr_arg, null_bit_arg,
	       unireg_check_arg, field_name_arg, table_arg,
	       dec_arg, zero_arg,unsigned_arg)
    {}
  Field_double(uint32 len_arg, bool maybe_null_arg, const char *field_name_arg,
	       struct st_table *table_arg, uint8 dec_arg)
    :Field_num((char*) 0, len_arg, maybe_null_arg ? (uchar*) "": 0, (uint) 0,
	       NONE, field_name_arg, table_arg,dec_arg,0,0)
    {}
  enum_field_types type() const { return FIELD_TYPE_DOUBLE;}
  enum ha_base_keytype key_type() const { return HA_KEYTYPE_DOUBLE; }
  int  store(const char *to,uint length,CHARSET_INFO *charset);
  int  store(double nr);
  int  store(longlong nr);
  void reset(void) { bzero(ptr,sizeof(double)); }
  double val_real(void);
  longlong val_int(void);
  String *val_str(String*,String *);
  bool send_binary(Protocol *protocol);
  int cmp(const char *,const char*);
  void sort_string(char *buff,uint length);
  uint32 pack_length() const { return sizeof(double); }
  void sql_type(String &str) const;
  uint32 max_length() { return 53; }
  field_cast_enum field_cast_type() { return FIELD_CAST_DOUBLE; }
};


/* Everything saved in this will disapper. It will always return NULL */

class Field_null :public Field_str {
  static uchar null[1];
public:
  Field_null(char *ptr_arg, uint32 len_arg,
	     enum utype unireg_check_arg, const char *field_name_arg,
	     struct st_table *table_arg, CHARSET_INFO *cs)
    :Field_str(ptr_arg, len_arg, null, 1,
	       unireg_check_arg, field_name_arg, table_arg, cs)
    {}
  enum_field_types type() const { return FIELD_TYPE_NULL;}
  int  store(const char *to, uint length, CHARSET_INFO *cs)
  { null[0]=1; return 0; }
  int  store(double nr)   { null[0]=1; return 0; }
  int  store(longlong nr) { null[0]=1; return 0; }
  void reset(void)	  {}
  double val_real(void)		{ return 0.0;}
  longlong val_int(void)	{ return 0;}
  String *val_str(String *value,String *value2)
  { value2->length(0); return value2;}
  int cmp(const char *a, const char *b) { return 0;}
  void sort_string(char *buff, uint length)  {}
  uint32 pack_length() const { return 0; }
  void sql_type(String &str) const;
  uint size_of() const { return sizeof(*this); }
  uint32 max_length() { return 4; }
  field_cast_enum field_cast_type() { return FIELD_CAST_NULL; }
};


class Field_timestamp :public Field_str {
public:
  Field_timestamp(char *ptr_arg, uint32 len_arg,
		  enum utype unireg_check_arg, const char *field_name_arg,
		  struct st_table *table_arg,
		  CHARSET_INFO *cs);
  enum_field_types type() const { return FIELD_TYPE_TIMESTAMP;}
  enum ha_base_keytype key_type() const { return HA_KEYTYPE_ULONG_INT; }
  enum Item_result cmp_type () const { return INT_RESULT; }
  int  store(const char *to,uint length,CHARSET_INFO *charset);
  int  store(double nr);
  int  store(longlong nr);
  void reset(void) { ptr[0]=ptr[1]=ptr[2]=ptr[3]=0; }
  double val_real(void);
  longlong val_int(void);
  String *val_str(String*,String *);
  bool send_binary(Protocol *protocol);
  int cmp(const char *,const char*);
  void sort_string(char *buff,uint length);
  uint32 pack_length() const { return 4; }
  void sql_type(String &str) const;
  bool store_for_compare() { return 1; }
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
  inline long get_timestamp()
  {
#ifdef WORDS_BIGENDIAN
    if (table->db_low_byte_first)
      return sint4korr(ptr);
#endif
    long tmp;
    longget(tmp,ptr);
    return tmp;
  }
  bool get_date(TIME *ltime,uint fuzzydate);
  bool get_time(TIME *ltime);
  field_cast_enum field_cast_type() { return FIELD_CAST_TIMESTAMP; }
  void set_timestamp_offsets();
};


class Field_year :public Field_tiny {
public:
  Field_year(char *ptr_arg, uint32 len_arg, uchar *null_ptr_arg,
	     uchar null_bit_arg,
	     enum utype unireg_check_arg, const char *field_name_arg,
	     struct st_table *table_arg)
    :Field_tiny(ptr_arg, len_arg, null_ptr_arg, null_bit_arg,
		unireg_check_arg, field_name_arg, table_arg, 1, 1)
    {}
  enum_field_types type() const { return FIELD_TYPE_YEAR;}
  int  store(const char *to,uint length,CHARSET_INFO *charset);
  int  store(double nr);
  int  store(longlong nr);
  double val_real(void);
  longlong val_int(void);
  String *val_str(String*,String *);
  bool send_binary(Protocol *protocol);
  void sql_type(String &str) const;
  bool store_for_compare() { return 1; }
  field_cast_enum field_cast_type() { return FIELD_CAST_YEAR; }
};


class Field_date :public Field_str {
public:
  Field_date(char *ptr_arg, uchar *null_ptr_arg, uchar null_bit_arg,
	     enum utype unireg_check_arg, const char *field_name_arg,
	     struct st_table *table_arg, CHARSET_INFO *cs)
    :Field_str(ptr_arg, 10, null_ptr_arg, null_bit_arg,
	       unireg_check_arg, field_name_arg, table_arg, cs)
    {}
  Field_date(bool maybe_null_arg, const char *field_name_arg,
		 struct st_table *table_arg, CHARSET_INFO *cs)
    :Field_str((char*) 0,10, maybe_null_arg ? (uchar*) "": 0,0,
	       NONE, field_name_arg, table_arg, cs) {}
  enum_field_types type() const { return FIELD_TYPE_DATE;}
  enum ha_base_keytype key_type() const { return HA_KEYTYPE_ULONG_INT; }
  enum Item_result cmp_type () const { return INT_RESULT; }
  int  store(const char *to,uint length,CHARSET_INFO *charset);
  int  store(double nr);
  int  store(longlong nr);
  void reset(void) { ptr[0]=ptr[1]=ptr[2]=ptr[3]=0; }
  double val_real(void);
  longlong val_int(void);
  String *val_str(String*,String *);
  bool send_binary(Protocol *protocol);
  int cmp(const char *,const char*);
  void sort_string(char *buff,uint length);
  uint32 pack_length() const { return 4; }
  void sql_type(String &str) const;
  bool store_for_compare() { return 1; }
  bool zero_pack() const { return 1; }
  field_cast_enum field_cast_type() { return FIELD_CAST_DATE; }
};

class Field_newdate :public Field_str {
public:
  Field_newdate(char *ptr_arg, uchar *null_ptr_arg, uchar null_bit_arg,
		enum utype unireg_check_arg, const char *field_name_arg,
		struct st_table *table_arg, CHARSET_INFO *cs)
    :Field_str(ptr_arg, 10, null_ptr_arg, null_bit_arg,
	       unireg_check_arg, field_name_arg, table_arg, cs)
    {}
  enum_field_types type() const { return FIELD_TYPE_DATE;}
  enum_field_types real_type() const { return FIELD_TYPE_NEWDATE; }
  enum ha_base_keytype key_type() const { return HA_KEYTYPE_UINT24; }
  enum Item_result cmp_type () const { return INT_RESULT; }
  int  store(const char *to,uint length,CHARSET_INFO *charset);
  int  store(double nr);
  int  store(longlong nr);
  void store_time(TIME *ltime,timestamp_type type);
  void reset(void) { ptr[0]=ptr[1]=ptr[2]=0; }
  double val_real(void);
  longlong val_int(void);
  String *val_str(String*,String *);
  bool send_binary(Protocol *protocol);
  int cmp(const char *,const char*);
  void sort_string(char *buff,uint length);
  uint32 pack_length() const { return 3; }
  void sql_type(String &str) const;
  bool store_for_compare() { return 1; }
  bool zero_pack() const { return 1; }
  bool get_date(TIME *ltime,uint fuzzydate);
  bool get_time(TIME *ltime);
  field_cast_enum field_cast_type() { return FIELD_CAST_NEWDATE; }
};


class Field_time :public Field_str {
public:
  Field_time(char *ptr_arg, uchar *null_ptr_arg, uchar null_bit_arg,
	     enum utype unireg_check_arg, const char *field_name_arg,
	     struct st_table *table_arg, CHARSET_INFO *cs)
    :Field_str(ptr_arg, 8, null_ptr_arg, null_bit_arg,
	       unireg_check_arg, field_name_arg, table_arg, cs)
    {}
  Field_time(bool maybe_null_arg, const char *field_name_arg,
		 struct st_table *table_arg, CHARSET_INFO *cs)
    :Field_str((char*) 0,8, maybe_null_arg ? (uchar*) "": 0,0,
	       NONE, field_name_arg, table_arg, cs) {}
  enum_field_types type() const { return FIELD_TYPE_TIME;}
  enum ha_base_keytype key_type() const { return HA_KEYTYPE_INT24; }
  enum Item_result cmp_type () const { return INT_RESULT; }
  int  store(const char *to,uint length,CHARSET_INFO *charset);
  int  store(double nr);
  int  store(longlong nr);
  void reset(void) { ptr[0]=ptr[1]=ptr[2]=0; }
  double val_real(void);
  longlong val_int(void);
  String *val_str(String*,String *);
  bool get_date(TIME *ltime, uint fuzzydate);
  bool send_binary(Protocol *protocol);
  bool get_time(TIME *ltime);
  int cmp(const char *,const char*);
  void sort_string(char *buff,uint length);
  uint32 pack_length() const { return 3; }
  void sql_type(String &str) const;
  bool store_for_compare() { return 1; }
  bool zero_pack() const { return 1; }
  field_cast_enum field_cast_type() { return FIELD_CAST_TIME; }
};


class Field_datetime :public Field_str {
public:
  Field_datetime(char *ptr_arg, uchar *null_ptr_arg, uchar null_bit_arg,
		 enum utype unireg_check_arg, const char *field_name_arg,
		 struct st_table *table_arg, CHARSET_INFO *cs)
    :Field_str(ptr_arg, 19, null_ptr_arg, null_bit_arg,
	       unireg_check_arg, field_name_arg, table_arg, cs)
    {}
  Field_datetime(bool maybe_null_arg, const char *field_name_arg,
		 struct st_table *table_arg, CHARSET_INFO *cs)
    :Field_str((char*) 0,19, maybe_null_arg ? (uchar*) "": 0,0,
	       NONE, field_name_arg, table_arg, cs) {}
  enum_field_types type() const { return FIELD_TYPE_DATETIME;}
#ifdef HAVE_LONG_LONG
  enum ha_base_keytype key_type() const { return HA_KEYTYPE_ULONGLONG; }
#endif
  enum Item_result cmp_type () const { return INT_RESULT; }
  int  store(const char *to,uint length,CHARSET_INFO *charset);
  int  store(double nr);
  int  store(longlong nr);
  void store_time(TIME *ltime,timestamp_type type);
  void reset(void) { ptr[0]=ptr[1]=ptr[2]=ptr[3]=ptr[4]=ptr[5]=ptr[6]=ptr[7]=0; }
  double val_real(void);
  longlong val_int(void);
  String *val_str(String*,String *);
  bool send_binary(Protocol *protocol);
  int cmp(const char *,const char*);
  void sort_string(char *buff,uint length);
  uint32 pack_length() const { return 8; }
  void sql_type(String &str) const;
  bool store_for_compare() { return 1; }
  bool zero_pack() const { return 1; }
  bool get_date(TIME *ltime,uint fuzzydate);
  bool get_time(TIME *ltime);
  field_cast_enum field_cast_type() { return FIELD_CAST_DATETIME; }
};


class Field_string :public Field_str {
public:
  Field_string(char *ptr_arg, uint32 len_arg,uchar *null_ptr_arg,
	       uchar null_bit_arg,
	       enum utype unireg_check_arg, const char *field_name_arg,
	       struct st_table *table_arg, CHARSET_INFO *cs)
    :Field_str(ptr_arg, len_arg, null_ptr_arg, null_bit_arg,
	       unireg_check_arg, field_name_arg, table_arg,cs) {};
  Field_string(uint32 len_arg,bool maybe_null_arg, const char *field_name_arg,
	       struct st_table *table_arg, CHARSET_INFO *cs)
    :Field_str((char*) 0,len_arg, maybe_null_arg ? (uchar*) "": 0,0,
	       NONE, field_name_arg, table_arg, cs) {};

  enum_field_types type() const
  {
    return ((table && table->db_create_options & HA_OPTION_PACK_RECORD &&
	     field_length >= 4) ?
	    FIELD_TYPE_VAR_STRING : FIELD_TYPE_STRING);
  }
  enum ha_base_keytype key_type() const
    { return binary() ? HA_KEYTYPE_BINARY : HA_KEYTYPE_TEXT; }
  bool zero_pack() const { return 0; }
  void reset(void) { charset()->cset->fill(charset(),ptr,field_length,' '); }
  int  store(const char *to,uint length,CHARSET_INFO *charset);
  int  store(longlong nr);
  int store(double nr) { return Field_str::store(nr); } /* QQ: To be deleted */
  double val_real(void);
  longlong val_int(void);
  String *val_str(String*,String *);
  int cmp(const char *,const char*);
  void sort_string(char *buff,uint length);
  void sql_type(String &str) const;
  char *pack(char *to, const char *from, uint max_length=~(uint) 0);
  char *pack_key(char *to, const char *from, uint max_length);
  const char *unpack(char* to, const char *from);
  int pack_cmp(const char *a,const char *b,uint key_length);
  int pack_cmp(const char *b,uint key_length);
  uint packed_col_length(const char *to, uint length);
  uint max_packed_col_length(uint max_length);
  uint size_of() const { return sizeof(*this); }
  enum_field_types real_type() const { return FIELD_TYPE_STRING; }
  bool has_charset(void) const
  { return charset() == &my_charset_bin ? FALSE : TRUE; }
  field_cast_enum field_cast_type() { return FIELD_CAST_STRING; }
};


class Field_varstring :public Field_str {
public:
  Field_varstring(char *ptr_arg, uint32 len_arg,uchar *null_ptr_arg,
		  uchar null_bit_arg,
		  enum utype unireg_check_arg, const char *field_name_arg,
		  struct st_table *table_arg, CHARSET_INFO *cs)
    :Field_str(ptr_arg, len_arg, null_ptr_arg, null_bit_arg,
	       unireg_check_arg, field_name_arg, table_arg, cs)
  {}
  Field_varstring(uint32 len_arg,bool maybe_null_arg,
		  const char *field_name_arg,
		  struct st_table *table_arg, CHARSET_INFO *cs)
    :Field_str((char*) 0,len_arg, maybe_null_arg ? (uchar*) "": 0,0,
	       NONE, field_name_arg, table_arg, cs)
  {}

  enum_field_types type() const { return FIELD_TYPE_VAR_STRING; }
  enum ha_base_keytype key_type() const
    { return binary() ? HA_KEYTYPE_VARBINARY : HA_KEYTYPE_VARTEXT; }
  bool zero_pack() const { return 0; }
  void reset(void) { bzero(ptr,field_length+2); }
  uint32 pack_length() const { return (uint32) field_length+2; }
  uint32 key_length() const { return (uint32) field_length; }
  int  store(const char *to,uint length,CHARSET_INFO *charset);
  int  store(longlong nr);
  int  store(double nr) { return Field_str::store(nr); } /* QQ: To be deleted */
  double val_real(void);
  longlong val_int(void);
  String *val_str(String*,String *);
  int cmp(const char *,const char*);
  void sort_string(char *buff,uint length);
  void get_key_image(char *buff,uint length, CHARSET_INFO *cs, imagetype type);
  void set_key_image(char *buff,uint length, CHARSET_INFO *cs);
  void sql_type(String &str) const;
  char *pack(char *to, const char *from, uint max_length=~(uint) 0);
  char *pack_key(char *to, const char *from, uint max_length);
  const char *unpack(char* to, const char *from);
  int pack_cmp(const char *a, const char *b, uint key_length);
  int pack_cmp(const char *b, uint key_length);
  uint packed_col_length(const char *to, uint length);
  uint max_packed_col_length(uint max_length);
  uint size_of() const { return sizeof(*this); }
  enum_field_types real_type() const { return FIELD_TYPE_VAR_STRING; }
  bool has_charset(void) const
  { return charset() == &my_charset_bin ? FALSE : TRUE; }
  field_cast_enum field_cast_type() { return FIELD_CAST_VARSTRING; }
};


class Field_blob :public Field_str {
protected:
  uint packlength;
  String value;				// For temporaries
public:
  Field_blob(char *ptr_arg, uchar *null_ptr_arg, uchar null_bit_arg,
	     enum utype unireg_check_arg, const char *field_name_arg,
	     struct st_table *table_arg,uint blob_pack_length,
	     CHARSET_INFO *cs);
  Field_blob(uint32 len_arg,bool maybe_null_arg, const char *field_name_arg,
	     struct st_table *table_arg, CHARSET_INFO *cs)
    :Field_str((char*) 0,len_arg, maybe_null_arg ? (uchar*) "": 0,0,
	       NONE, field_name_arg, table_arg, cs),
    packlength(4)
  {
    flags|= BLOB_FLAG;
  }
  enum_field_types type() const { return FIELD_TYPE_BLOB;}
  enum ha_base_keytype key_type() const
    { return binary() ? HA_KEYTYPE_VARBINARY : HA_KEYTYPE_VARTEXT; }
  int  store(const char *to,uint length,CHARSET_INFO *charset);
  int  store(double nr);
  int  store(longlong nr);
  double val_real(void);
  longlong val_int(void);
  String *val_str(String*,String *);
  int cmp(const char *,const char*);
  int cmp(const char *a, uint32 a_length, const char *b, uint32 b_length);
  int cmp_offset(uint offset);
  int cmp_binary(const char *a,const char *b, uint32 max_length=~0L);
  int cmp_binary_offset(uint row_offset);
  int key_cmp(const byte *,const byte*);
  int key_cmp(const byte *str, uint length);
  uint32 key_length() const { return 0; }
  void sort_string(char *buff,uint length);
  uint32 pack_length() const
  { return (uint32) (packlength+table->blob_ptr_size); }
  inline uint32 max_data_length() const
  {
    return (uint32) (((ulonglong) 1 << (packlength*8)) -1);
  }
  void reset(void) { bzero(ptr, packlength+sizeof(char*)); }
  void reset_fields() { bzero((char*) &value,sizeof(value)); }
  void store_length(uint32 number);
  inline uint32 get_length(uint row_offset=0)
  { return get_length(ptr+row_offset); }
  uint32 get_length(const char *ptr);
  void put_length(char *pos, uint32 length);
  inline void get_ptr(char **str)
    {
      memcpy_fixed(str,ptr+packlength,sizeof(char*));
    }
  inline void set_ptr(char *length,char *data)
    {
      memcpy(ptr,length,packlength);
      memcpy_fixed(ptr+packlength,&data,sizeof(char*));
    }
  inline void set_ptr(uint32 length,char *data)
    {
      store_length(length);
      memcpy_fixed(ptr+packlength,&data,sizeof(char*));
    }
  void get_key_image(char *buff,uint length, CHARSET_INFO *cs, imagetype type);
  void set_key_image(char *buff,uint length, CHARSET_INFO *cs);
  void sql_type(String &str) const;
  inline bool copy()
  { char *tmp;
    get_ptr(&tmp);
    if (value.copy(tmp,get_length(),charset()))
    {
      Field_blob::reset();
      return 1;
    }
    tmp=(char*) value.ptr(); memcpy_fixed(ptr+packlength,&tmp,sizeof(char*));
    return 0;
  }
  char *pack(char *to, const char *from, uint max_length= ~(uint) 0);
  const char *unpack(char *to, const char *from);
  char *pack_key(char *to, const char *from, uint max_length);
  char *pack_key_from_key_image(char* to, const char *from, uint max_length);
  const char *unpack_key(char* to, const char *from, uint max_length);
  int pack_cmp(const char *a, const char *b, uint key_length);
  int pack_cmp(const char *b, uint key_length);
  uint packed_col_length(const char *col_ptr, uint length);
  uint max_packed_col_length(uint max_length);
  void free() { value.free(); }
  inline void clear_temporary() { bzero((char*) &value,sizeof(value)); }
  friend void field_conv(Field *to,Field *from);
  uint size_of() const { return sizeof(*this); }
  bool has_charset(void) const
  { return charset() == &my_charset_bin ? FALSE : TRUE; }
  field_cast_enum field_cast_type() { return FIELD_CAST_BLOB; }
  uint32 max_length();
};

#ifdef HAVE_SPATIAL
class Field_geom :public Field_blob {
public:
  enum geometry_type geom_type;

  Field_geom(char *ptr_arg, uchar *null_ptr_arg, uint null_bit_arg,
	     enum utype unireg_check_arg, const char *field_name_arg,
	     struct st_table *table_arg,uint blob_pack_length,
	     enum geometry_type geom_type_arg)
     :Field_blob(ptr_arg, null_ptr_arg, null_bit_arg, unireg_check_arg, 
                 field_name_arg, table_arg, blob_pack_length,&my_charset_bin)
  { geom_type= geom_type_arg; }
  Field_geom(uint32 len_arg,bool maybe_null_arg, const char *field_name_arg,
	     struct st_table *table_arg, enum geometry_type geom_type_arg)
     :Field_blob(len_arg, maybe_null_arg, field_name_arg,
                 table_arg, &my_charset_bin)
  { geom_type= geom_type_arg; }
  enum ha_base_keytype key_type() const { return HA_KEYTYPE_VARBINARY; }
  enum_field_types type() const { return FIELD_TYPE_GEOMETRY; }
  void sql_type(String &str) const;
  int  store(const char *to, uint length, CHARSET_INFO *charset);
  int  store(double nr) { return 1; }
  int  store(longlong nr) { return 1; }

  void get_key_image(char *buff,uint length, CHARSET_INFO *cs,imagetype type);
  void set_key_image(char *buff,uint length, CHARSET_INFO *cs);
  field_cast_enum field_cast_type() { return FIELD_CAST_GEOM; }
};
#endif /*HAVE_SPATIAL*/

class Field_enum :public Field_str {
protected:
  uint packlength;
public:
  TYPELIB *typelib;
  Field_enum(char *ptr_arg, uint32 len_arg, uchar *null_ptr_arg,
		 uchar null_bit_arg,
		 enum utype unireg_check_arg, const char *field_name_arg,
		 struct st_table *table_arg,uint packlength_arg,
		 TYPELIB *typelib_arg,
		 CHARSET_INFO *charset_arg)
    :Field_str(ptr_arg, len_arg, null_ptr_arg, null_bit_arg,
	       unireg_check_arg, field_name_arg, table_arg, charset_arg),
    packlength(packlength_arg),typelib(typelib_arg)
  {
      flags|=ENUM_FLAG;
  }
  enum_field_types type() const { return FIELD_TYPE_STRING; }
  enum Item_result cmp_type () const { return INT_RESULT; }
  enum ha_base_keytype key_type() const;
  int  store(const char *to,uint length,CHARSET_INFO *charset);
  int  store(double nr);
  int  store(longlong nr);
  void reset() { bzero(ptr,packlength); }
  double val_real(void);
  longlong val_int(void);
  String *val_str(String*,String *);
  int cmp(const char *,const char*);
  void sort_string(char *buff,uint length);
  uint32 pack_length() const { return (uint32) packlength; }
  void store_type(ulonglong value);
  void sql_type(String &str) const;
  uint size_of() const { return sizeof(*this); }
  enum_field_types real_type() const { return FIELD_TYPE_ENUM; }
  virtual bool zero_pack() const { return 0; }
  bool optimize_range(uint idx, uint part) { return 0; }
  bool eq_def(Field *field);
  bool has_charset(void) const { return TRUE; }
  field_cast_enum field_cast_type() { return FIELD_CAST_ENUM; }
};


class Field_set :public Field_enum {
public:
  Field_set(char *ptr_arg, uint32 len_arg, uchar *null_ptr_arg,
	    uchar null_bit_arg,
	    enum utype unireg_check_arg, const char *field_name_arg,
	    struct st_table *table_arg,uint32 packlength_arg,
	    TYPELIB *typelib_arg, CHARSET_INFO *charset_arg)
    :Field_enum(ptr_arg, len_arg, null_ptr_arg, null_bit_arg,
		    unireg_check_arg, field_name_arg,
		    table_arg, packlength_arg,
		    typelib_arg,charset_arg)
    {
      flags=(flags & ~ENUM_FLAG) | SET_FLAG;
    }
  int  store(const char *to,uint length,CHARSET_INFO *charset);
  int  store(double nr) { return Field_set::store((longlong) nr); }
  int  store(longlong nr);
  virtual bool zero_pack() const { return 1; }
  String *val_str(String*,String *);
  void sql_type(String &str) const;
  enum_field_types real_type() const { return FIELD_TYPE_SET; }
  bool has_charset(void) const { return TRUE; }
  field_cast_enum field_cast_type() { return FIELD_CAST_SET; }
};


/*
  Create field class for CREATE TABLE
*/

class create_field :public Sql_alloc {
public:
  const char *field_name;
  const char *change;			// If done with alter table
  const char *after;			// Put column after this one
  LEX_STRING comment;			// Comment for field
  Item	*def;				// Default value
  enum	enum_field_types sql_type;
  uint32 length;
  uint decimals,flags,pack_length;
  Field::utype unireg_check;
  TYPELIB *interval;			// Which interval to use
  CHARSET_INFO *charset;
  Field::geometry_type geom_type;
  Field *field;				// For alter table

  uint8 row,col,sc_length,interval_id;	// For rea_create_table
  uint	offset,pack_flag;
  create_field() :after(0) {}
  create_field(Field *field, Field *orig_field);
  void create_length_to_internal_length(void);
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
  void (*get_copy_func(Field *to,Field *from))(Copy_field *);
public:
  char *from_ptr,*to_ptr;
  uchar *from_null_ptr,*to_null_ptr;
  my_bool *null_row;
  uint	from_bit,to_bit;
  uint from_length,to_length;
  Field *from_field,*to_field;
  String tmp;					// For items

  Copy_field() {}
  ~Copy_field() {}
  void set(Field *to,Field *from,bool save);	// Field to field 
  void set(char *to,Field *from);		// Field to string
  void (*do_copy)(Copy_field *);
  void (*do_copy2)(Copy_field *);		// Used to handle null values
};


Field *make_field(char *ptr, uint32 field_length,
		  uchar *null_pos, uchar null_bit,
		  uint pack_flag, enum_field_types field_type,
		  CHARSET_INFO *cs,
		  Field::geometry_type geom_type,
		  Field::utype unireg_check,
		  TYPELIB *interval, const char *field_name,
		  struct st_table *table);
uint pack_length_to_packflag(uint type);
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
#define FIELDFLAG_INTERVAL		256
#define FIELDFLAG_BITFIELD		512	// mangled with dec!
#define FIELDFLAG_BLOB			1024	// mangled with dec!
#define FIELDFLAG_GEOM			2048

#define FIELDFLAG_LEFT_FULLSCREEN	8192
#define FIELDFLAG_RIGHT_FULLSCREEN	16384
#define FIELDFLAG_FORMAT_NUMBER		16384	// predit: ###,,## in output
#define FIELDFLAG_NO_DEFAULT		16384   /* sql */
#define FIELDFLAG_SUM			((uint) 32768)// predit: +#fieldflag
#define FIELDFLAG_MAYBE_NULL		((uint) 32768)// sql
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
#define f_is_enum(x)            ((x) & FIELDFLAG_INTERVAL)
#define f_is_bitfield(x)	((x) & FIELDFLAG_BITFIELD)
#define f_is_blob(x)		(((x) & (FIELDFLAG_BLOB | FIELDFLAG_NUMBER)) == FIELDFLAG_BLOB)
#define f_is_geom(x)		((x) & FIELDFLAG_GEOM)
#define f_is_equ(x)		((x) & (1+2+FIELDFLAG_PACK+31*256))
#define f_settype(x)		(((int) x) << FIELDFLAG_PACK_SHIFT)
#define f_maybe_null(x)		(x & FIELDFLAG_MAYBE_NULL)
#define f_no_default(x)		(x & FIELDFLAG_NO_DEFAULT)
