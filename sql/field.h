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
** Because of the function new_field all field classes that have static
** variables must declare the size_of() member function.
*/

#ifdef __GNUC__
#pragma interface			/* gcc class implementation */
#endif

#define NOT_FIXED_DEC			31

class Send_field;
struct st_cache_field;
void field_conv(Field *to,Field *from);

class Field {
  Field(const Item &);				/* Prevent use of theese */
  void operator=(Field &);
public:
  static void *operator new(size_t size) {return (void*) sql_alloc((uint) size); }
  static void operator delete(void *ptr_arg, size_t size) {} /*lint -e715 */

  enum utype { NONE,DATE,SHIELD,NOEMPTY,CASEUP,PNR,BGNR,PGNR,YES,NO,REL,
	       CHECK,EMPTY,UNKNOWN,CASEDN,NEXT_NUMBER,INTERVAL_FIELD,BIT_FIELD,
	       TIMESTAMP_FIELD,CAPITALIZE,BLOB_FIELD};
  char	*ptr;				// Position to field in record
  uchar		*null_ptr;		// Byte where null_bit is
  uint8		null_bit;		// And position to it
  struct st_table *table;		// Pointer for table
  ulong query_id;			// For quick test of used fields
  key_map key_start,part_of_key,part_of_sortkey;// Field is part of these keys.
  const char *table_name,*field_name;
  utype unireg_check;
  uint32 field_length;			// Length of field
  uint16 flags;

  Field(char *ptr_arg,uint32 length_arg,uchar *null_ptr_arg,uint null_bit_arg,
	utype unireg_check_arg, const char *field_name_arg,
	struct st_table *table_arg);
  virtual ~Field() {}
  virtual void store(const char *to,uint length)=0;
  virtual void store(double nr)=0;
  virtual void store(longlong nr)=0;
  virtual void store_time(TIME *ltime,timestamp_type t_type);
  virtual double val_real(void)=0;
  virtual longlong val_int(void)=0;
  virtual String *val_str(String*,String *)=0;
  virtual Item_result result_type () const=0;
  virtual Item_result cmp_type () const { return result_type(); }
  bool eq(Field *field) { return ptr == field->ptr; }
  virtual bool eq_def(Field *field);
  virtual uint32 pack_length() const { return (uint32) field_length; }
  virtual void reset(void) { bzero(ptr,pack_length()); }
  virtual void reset_fields() {}
  virtual bool binary() const { return 1; }
  virtual bool zero_pack() const { return 1; }
  virtual enum ha_base_keytype key_type() const { return HA_KEYTYPE_BINARY; }
  virtual uint32 key_length() const { return pack_length(); }
  virtual enum_field_types type() const =0;
  virtual enum_field_types real_type() const { return type(); }
  inline  int cmp(const char *str) { return cmp(ptr,str); }
  virtual int cmp(const char *,const char *)=0;
  virtual int cmp_binary(const char *a,const char *b, ulong max_length=~0L)
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
  virtual void sql_type(String &str) const =0;
  // Caller beware: sql_type can change str.Ptr, so check
  // ptr() to see if it changed if you are using your own buffer
  // in str and restore it with set() if needed
  
  virtual uint size_of() const =0;			// For new field
  inline bool is_null(uint row_offset=0)
  { return null_ptr ? (null_ptr[row_offset] & null_bit ? 1 : 0) : table->null_row; }
  inline bool is_real_null(uint row_offset=0)
    { return null_ptr ? (null_ptr[row_offset] & null_bit ? 1 : 0) : 0; }
  inline void set_null(int row_offset=0)
    { if (null_ptr) null_ptr[row_offset]|= null_bit; }
  inline void set_notnull(int row_offset=0)
    { if (null_ptr) null_ptr[row_offset]&= ~null_bit; }
  inline bool maybe_null(void) { return null_ptr != 0 || table->maybe_null; }
  inline bool real_maybe_null(void) { return null_ptr != 0; }
  virtual void make_field(Send_field *)=0;
  virtual void sort_string(char *buff,uint length)=0;
  virtual bool optimize_range();
  virtual bool store_for_compare() { return 0; }
  inline Field *new_field(struct st_table *new_table)
    {
      Field *tmp= (Field*) sql_memdup((char*) this,size_of());
      if (tmp)
      {
	tmp->table=new_table;
	tmp->key_start=tmp->part_of_key=tmp->part_of_sortkey=0;
	tmp->unireg_check=Field::NONE;
	tmp->flags&= (NOT_NULL_FLAG | BLOB_FLAG | UNSIGNED_FLAG | ZEROFILL_FLAG | BINARY_FLAG | ENUM_FLAG | SET_FLAG);
	tmp->reset_fields();
      }
      return tmp;
    }
  inline void move_field(char *ptr_arg,uchar *null_ptr_arg,uint null_bit_arg)
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
  inline void get_image(char *buff,uint length)
    { memcpy(buff,ptr,length); }
  inline void set_image(char *buff,uint length)
    { memcpy(ptr,buff,length); }
  virtual void get_key_image(char *buff,uint length)
    { get_image(buff,length); }
  virtual void set_key_image(char *buff,uint length)
    { set_image(buff,length); }
  inline int cmp_image(char *buff,uint length)
    {
      if (binary())
	return memcmp(ptr,buff,length);
      else
	return my_casecmp(ptr,buff,length);
    }
  inline longlong val_int_offset(uint row_offset)
    {
      ptr+=row_offset;
      longlong tmp=val_int();
      ptr-=row_offset;
      return tmp;
    }
  bool send(String *packet);
  virtual char *pack(char* to, const char *from, uint max_length=~(uint) 0)
  {
    uint length=pack_length();
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
  virtual uint packed_col_length(const char *to)
  { return pack_length();}
  virtual uint max_packed_col_length(uint max_length)
  { return pack_length();}

  virtual int pack_cmp(const char *a,const char *b, uint key_length_arg)
  { return cmp(a,b); }
  virtual int pack_cmp(const char *b, uint key_length_arg)
  { return cmp(ptr,b); }
  uint offset();				// Should be inline ...
  void copy_from_tmp(int offset);
  uint fill_cache_field(struct st_cache_field *copy);
  virtual bool get_date(TIME *ltime,bool fuzzydate);
  virtual bool get_time(TIME *ltime);
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
};


class Field_num :public Field {
public:
  const uint8 dec;
  bool zerofill,unsigned_flag;		// Purify cannot handle bit fields
  Field_num(char *ptr_arg,uint32 len_arg, uchar *null_ptr_arg,
	    uint null_bit_arg, utype unireg_check_arg,
	    const char *field_name_arg,
	    struct st_table *table_arg,
	    uint dec_arg,bool zero_arg,bool unsigned_arg)
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
  uint decimals() const { return dec; }
  uint size_of() const { return sizeof(*this); }
  bool eq_def(Field *field);
};


class Field_str :public Field {
public:
  Field_str(char *ptr_arg,uint32 len_arg, uchar *null_ptr_arg,
	    uint null_bit_arg, utype unireg_check_arg,
	    const char *field_name_arg,
	    struct st_table *table_arg)
    :Field(ptr_arg, len_arg, null_ptr_arg, null_bit_arg,
	   unireg_check_arg, field_name_arg, table_arg)
    {}
  Item_result result_type () const { return STRING_RESULT; }
  uint decimals() const { return NOT_FIXED_DEC; }
  friend class create_field;
  void make_field(Send_field *);
  uint size_of() const { return sizeof(*this); }
};


class Field_decimal :public Field_num {
public:
  Field_decimal(char *ptr_arg, uint32 len_arg, uchar *null_ptr_arg,
		uint null_bit_arg,
		enum utype unireg_check_arg, const char *field_name_arg,
		struct st_table *table_arg,
		uint dec_arg,bool zero_arg,bool unsigned_arg)
    :Field_num(ptr_arg, len_arg, null_ptr_arg, null_bit_arg,
	       unireg_check_arg, field_name_arg, table_arg,
	       dec_arg, zero_arg,unsigned_arg)
    {}
  enum_field_types type() const { return FIELD_TYPE_DECIMAL;}
  enum ha_base_keytype key_type() const
    { return zerofill ? HA_KEYTYPE_BINARY : HA_KEYTYPE_NUM; }
  void reset(void);
  void store(const char *to,uint length);
  void store(double nr);
  void store(longlong nr);
  double val_real(void);
  longlong val_int(void);
  String *val_str(String*,String *);
  int cmp(const char *,const char*);
  void sort_string(char *buff,uint length);
  void overflow(bool negative);
  bool zero_pack() const { return 0; }
  void sql_type(String &str) const;
};


class Field_tiny :public Field_num {
public:
  Field_tiny(char *ptr_arg, uint32 len_arg, uchar *null_ptr_arg,
	     uint null_bit_arg,
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
  void store(const char *to,uint length);
  void store(double nr);
  void store(longlong nr);
  double val_real(void);
  longlong val_int(void);
  String *val_str(String*,String *);
  int cmp(const char *,const char*);
  void sort_string(char *buff,uint length);
  uint32 pack_length() const { return 1; }
  void sql_type(String &str) const;
};


class Field_short :public Field_num {
public:
  Field_short(char *ptr_arg, uint32 len_arg, uchar *null_ptr_arg,
	      uint null_bit_arg,
	      enum utype unireg_check_arg, const char *field_name_arg,
	      struct st_table *table_arg,
	      bool zero_arg, bool unsigned_arg)
    :Field_num(ptr_arg, len_arg, null_ptr_arg, null_bit_arg,
	       unireg_check_arg, field_name_arg, table_arg,
	       0, zero_arg,unsigned_arg)
    {}
  enum Item_result result_type () const { return INT_RESULT; }
  enum_field_types type() const { return FIELD_TYPE_SHORT;}
  enum ha_base_keytype key_type() const
    { return unsigned_flag ? HA_KEYTYPE_USHORT_INT : HA_KEYTYPE_SHORT_INT;}
  void store(const char *to,uint length);
  void store(double nr);
  void store(longlong nr);
  double val_real(void);
  longlong val_int(void);
  String *val_str(String*,String *);
  int cmp(const char *,const char*);
  void sort_string(char *buff,uint length);
  uint32 pack_length() const { return 2; }
  void sql_type(String &str) const;
};


class Field_medium :public Field_num {
public:
  Field_medium(char *ptr_arg, uint32 len_arg, uchar *null_ptr_arg,
	      uint null_bit_arg,
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
  void store(const char *to,uint length);
  void store(double nr);
  void store(longlong nr);
  double val_real(void);
  longlong val_int(void);
  String *val_str(String*,String *);
  int cmp(const char *,const char*);
  void sort_string(char *buff,uint length);
  uint32 pack_length() const { return 3; }
  void sql_type(String &str) const;
};


class Field_long :public Field_num {
public:
  Field_long(char *ptr_arg, uint32 len_arg, uchar *null_ptr_arg,
	     uint null_bit_arg,
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
  void store(const char *to,uint length);
  void store(double nr);
  void store(longlong nr);
  double val_real(void);
  longlong val_int(void);
  String *val_str(String*,String *);
  int cmp(const char *,const char*);
  void sort_string(char *buff,uint length);
  uint32 pack_length() const { return 4; }
  void sql_type(String &str) const;
};


#ifdef HAVE_LONG_LONG
class Field_longlong :public Field_num {
public:
  Field_longlong(char *ptr_arg, uint32 len_arg, uchar *null_ptr_arg,
	      uint null_bit_arg,
	      enum utype unireg_check_arg, const char *field_name_arg,
	      struct st_table *table_arg,
	      bool zero_arg, bool unsigned_arg)
    :Field_num(ptr_arg, len_arg, null_ptr_arg, null_bit_arg,
	       unireg_check_arg, field_name_arg, table_arg,
	       0, zero_arg,unsigned_arg)
    {}
  Field_longlong(uint32 len_arg,bool maybe_null_arg, const char *field_name_arg,
		 struct st_table *table_arg)
    :Field_num((char*) 0, len_arg, maybe_null_arg ? (uchar*) "": 0,0,
	       NONE, field_name_arg, table_arg,0,0,0)
    {}
  enum Item_result result_type () const { return INT_RESULT; }
  enum_field_types type() const { return FIELD_TYPE_LONGLONG;}
  enum ha_base_keytype key_type() const
    { return unsigned_flag ? HA_KEYTYPE_ULONGLONG : HA_KEYTYPE_LONGLONG; }
  void store(const char *to,uint length);
  void store(double nr);
  void store(longlong nr);
  double val_real(void);
  longlong val_int(void);
  String *val_str(String*,String *);
  int cmp(const char *,const char*);
  void sort_string(char *buff,uint length);
  uint32 pack_length() const { return 8; }
  void sql_type(String &str) const;
};
#endif

class Field_float :public Field_num {
public:
  Field_float(char *ptr_arg, uint32 len_arg, uchar *null_ptr_arg,
	      uint null_bit_arg,
	      enum utype unireg_check_arg, const char *field_name_arg,
	      struct st_table *table_arg,
	       uint dec_arg,bool zero_arg,bool unsigned_arg)
    :Field_num(ptr_arg, len_arg, null_ptr_arg, null_bit_arg,
	       unireg_check_arg, field_name_arg, table_arg,
	       dec_arg, zero_arg,unsigned_arg)
    {}
  enum_field_types type() const { return FIELD_TYPE_FLOAT;}
  enum ha_base_keytype key_type() const { return HA_KEYTYPE_FLOAT; }
  void store(const char *to,uint length);
  void store(double nr);
  void store(longlong nr);
  double val_real(void);
  longlong val_int(void);
  String *val_str(String*,String *);
  int cmp(const char *,const char*);
  void sort_string(char *buff,uint length);
  uint32 pack_length() const { return sizeof(float); }
  void sql_type(String &str) const;
};


class Field_double :public Field_num {
public:
  Field_double(char *ptr_arg, uint32 len_arg, uchar *null_ptr_arg,
	       uint null_bit_arg,
	       enum utype unireg_check_arg, const char *field_name_arg,
	       struct st_table *table_arg,
	       uint dec_arg,bool zero_arg,bool unsigned_arg)
    :Field_num(ptr_arg, len_arg, null_ptr_arg, null_bit_arg,
	       unireg_check_arg, field_name_arg, table_arg,
	       dec_arg, zero_arg,unsigned_arg)
    {}
  Field_double(uint32 len_arg, bool maybe_null_arg, const char *field_name_arg,
	       struct st_table *table_arg, uint dec_arg)
    :Field_num((char*) 0, len_arg, maybe_null_arg ? (uchar*) "": 0, (uint) 0,
	       NONE, field_name_arg, table_arg,dec_arg,0,0)
    {}
  enum_field_types type() const { return FIELD_TYPE_DOUBLE;}
  enum ha_base_keytype key_type() const { return HA_KEYTYPE_DOUBLE; }
  void store(const char *to,uint length);
  void store(double nr);
  void store(longlong nr);
  double val_real(void);
  longlong val_int(void);
  String *val_str(String*,String *);
  int cmp(const char *,const char*);
  void sort_string(char *buff,uint length);
  uint32 pack_length() const { return sizeof(double); }
  void sql_type(String &str) const;
};


/* Everything saved in this will disapper. It will always return NULL */

class Field_null :public Field_str {
  static uchar null[1];
public:
  Field_null(char *ptr_arg, uint32 len_arg,
	     enum utype unireg_check_arg, const char *field_name_arg,
	     struct st_table *table_arg)
    :Field_str(ptr_arg, len_arg, null, 1,
	       unireg_check_arg, field_name_arg, table_arg)
    {}
  enum_field_types type() const { return FIELD_TYPE_NULL;}
  void store(const char *to, uint length) { null[0]=1; }
  void store(double nr)   { null[0]=1; }
  void store(longlong nr) { null[0]=1; }
  double val_real(void)		{ return 0.0;}
  longlong val_int(void)	{ return 0;}
  String *val_str(String *value,String *value2)
  { value2->length(0); return value2;}
  int cmp(const char *a, const char *b) { return 0;}
  void sort_string(char *buff, uint length)  {}
  uint32 pack_length() const { return 0; }
  void sql_type(String &str) const { str.set("null",4); }
  uint size_of() const { return sizeof(*this); }
};


class Field_timestamp :public Field_num {
public:
  Field_timestamp(char *ptr_arg, uint32 len_arg,
		  enum utype unireg_check_arg, const char *field_name_arg,
		  struct st_table *table_arg);
  enum Item_result result_type () const { return field_length == 8 || field_length == 14 ? INT_RESULT : STRING_RESULT; }
  enum_field_types type() const { return FIELD_TYPE_TIMESTAMP;}
  enum ha_base_keytype key_type() const { return HA_KEYTYPE_ULONG_INT; }
  void store(const char *to,uint length);
  void store(double nr);
  void store(longlong nr);
  double val_real(void);
  longlong val_int(void);
  String *val_str(String*,String *);
  int cmp(const char *,const char*);
  void sort_string(char *buff,uint length);
  uint32 pack_length() const { return 4; }
  void sql_type(String &str) const;
  bool store_for_compare() { return 1; }
  bool zero_pack() const { return 0; }
  void set_time();
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
  void fill_and_store(char *from,uint len);
  bool get_date(TIME *ltime,bool fuzzydate);
  bool get_time(TIME *ltime);
};


class Field_year :public Field_tiny {
public:
  Field_year(char *ptr_arg, uint32 len_arg, uchar *null_ptr_arg,
	     uint null_bit_arg,
	     enum utype unireg_check_arg, const char *field_name_arg,
	     struct st_table *table_arg)
    :Field_tiny(ptr_arg, len_arg, null_ptr_arg, null_bit_arg,
		unireg_check_arg, field_name_arg, table_arg, 1, 1)
    {}
  enum_field_types type() const { return FIELD_TYPE_YEAR;}
  void store(const char *to,uint length);
  void store(double nr);
  void store(longlong nr);
  double val_real(void);
  longlong val_int(void);
  String *val_str(String*,String *);
  void sql_type(String &str) const;
};


class Field_date :public Field_str {
public:
  Field_date(char *ptr_arg, uchar *null_ptr_arg, uint null_bit_arg,
	     enum utype unireg_check_arg, const char *field_name_arg,
	     struct st_table *table_arg)
    :Field_str(ptr_arg, 10, null_ptr_arg, null_bit_arg,
	       unireg_check_arg, field_name_arg, table_arg)
    {}
  enum_field_types type() const { return FIELD_TYPE_DATE;}
  enum ha_base_keytype key_type() const { return HA_KEYTYPE_ULONG_INT; }
  enum Item_result cmp_type () const { return INT_RESULT; }
  void store(const char *to,uint length);
  void store(double nr);
  void store(longlong nr);
  double val_real(void);
  longlong val_int(void);
  String *val_str(String*,String *);
  int cmp(const char *,const char*);
  void sort_string(char *buff,uint length);
  uint32 pack_length() const { return 4; }
  void sql_type(String &str) const;
  bool store_for_compare() { return 1; }
  bool zero_pack() const { return 1; }
};

class Field_newdate :public Field_str {
public:
  Field_newdate(char *ptr_arg, uchar *null_ptr_arg, uint null_bit_arg,
		enum utype unireg_check_arg, const char *field_name_arg,
		struct st_table *table_arg)
    :Field_str(ptr_arg, 10, null_ptr_arg, null_bit_arg,
	       unireg_check_arg, field_name_arg, table_arg)
    {}
  enum_field_types type() const { return FIELD_TYPE_DATE;}
  enum_field_types real_type() const { return FIELD_TYPE_NEWDATE; }
  enum ha_base_keytype key_type() const { return HA_KEYTYPE_UINT24; }
  enum Item_result cmp_type () const { return INT_RESULT; }
  void store(const char *to,uint length);
  void store(double nr);
  void store(longlong nr);
  void store_time(TIME *ltime,timestamp_type type);
  double val_real(void);
  longlong val_int(void);
  String *val_str(String*,String *);
  int cmp(const char *,const char*);
  void sort_string(char *buff,uint length);
  uint32 pack_length() const { return 3; }
  void sql_type(String &str) const;
  bool store_for_compare() { return 1; }
  bool zero_pack() const { return 1; }
  bool get_date(TIME *ltime,bool fuzzydate);
  bool get_time(TIME *ltime);
};


class Field_time :public Field_str {
public:
  Field_time(char *ptr_arg, uchar *null_ptr_arg, uint null_bit_arg,
	     enum utype unireg_check_arg, const char *field_name_arg,
	     struct st_table *table_arg)
    :Field_str(ptr_arg, 8, null_ptr_arg, null_bit_arg,
	       unireg_check_arg, field_name_arg, table_arg)
    {}
  enum_field_types type() const { return FIELD_TYPE_TIME;}
  enum ha_base_keytype key_type() const { return HA_KEYTYPE_INT24; }
  enum Item_result cmp_type () const { return INT_RESULT; }
  void store(const char *to,uint length);
  void store(double nr);
  void store(longlong nr);
  double val_real(void);
  longlong val_int(void);
  String *val_str(String*,String *);
  bool get_time(TIME *ltime);
  int cmp(const char *,const char*);
  void sort_string(char *buff,uint length);
  uint32 pack_length() const { return 3; }
  void sql_type(String &str) const;
  bool store_for_compare() { return 1; }
  bool zero_pack() const { return 1; }
};


class Field_datetime :public Field_str {
public:
  Field_datetime(char *ptr_arg, uchar *null_ptr_arg, uint null_bit_arg,
		 enum utype unireg_check_arg, const char *field_name_arg,
		 struct st_table *table_arg)
    :Field_str(ptr_arg, 19, null_ptr_arg, null_bit_arg,
	       unireg_check_arg, field_name_arg, table_arg)
    {}
  enum_field_types type() const { return FIELD_TYPE_DATETIME;}
#ifdef HAVE_LONG_LONG
  enum ha_base_keytype key_type() const { return HA_KEYTYPE_ULONGLONG; }
#endif
  enum Item_result cmp_type () const { return INT_RESULT; }
  void store(const char *to,uint length);
  void store(double nr);
  void store(longlong nr);
  void store_time(TIME *ltime,timestamp_type type);
  double val_real(void);
  longlong val_int(void);
  String *val_str(String*,String *);
  int cmp(const char *,const char*);
  void sort_string(char *buff,uint length);
  uint32 pack_length() const { return 8; }
  void sql_type(String &str) const;
  bool store_for_compare() { return 1; }
  bool zero_pack() const { return 1; }
  bool get_date(TIME *ltime,bool fuzzydate);
  bool get_time(TIME *ltime);
};


class Field_string :public Field_str {
  bool binary_flag;
public:
  Field_string(char *ptr_arg, uint32 len_arg,uchar *null_ptr_arg,
	       uint null_bit_arg,
	       enum utype unireg_check_arg, const char *field_name_arg,
	       struct st_table *table_arg,bool binary_arg)
    :Field_str(ptr_arg, len_arg, null_ptr_arg, null_bit_arg,
	       unireg_check_arg, field_name_arg, table_arg),
    binary_flag(binary_arg)
    {
      if (binary_arg)
	flags|=BINARY_FLAG;
    }
  Field_string(uint32 len_arg,bool maybe_null_arg, const char *field_name_arg,
	       struct st_table *table_arg, bool binary_arg)
    :Field_str((char*) 0,len_arg, maybe_null_arg ? (uchar*) "": 0,0,
	       NONE, field_name_arg, table_arg),
    binary_flag(binary_arg)
    {
      if (binary_arg)
	flags|=BINARY_FLAG;
    }

  enum_field_types type() const
  {
    return ((table && table->db_create_options & HA_OPTION_PACK_RECORD &&
	     field_length >= 4) ?
	    FIELD_TYPE_VAR_STRING : FIELD_TYPE_STRING);
  }
  enum ha_base_keytype key_type() const
    { return binary_flag ? HA_KEYTYPE_BINARY : HA_KEYTYPE_TEXT; }
  bool zero_pack() const { return 0; }
  bool binary() const { return binary_flag; }
  void reset(void) { bfill(ptr,field_length,' '); }
  void store(const char *to,uint length);
  void store(double nr);
  void store(longlong nr);
  double val_real(void);
  longlong val_int(void);
  String *val_str(String*,String *);
  int cmp(const char *,const char*);
  void sort_string(char *buff,uint length);
  void sql_type(String &str) const;
  char *pack(char *to, const char *from, uint max_length=~(uint) 0);
  const char *unpack(char* to, const char *from);
  int pack_cmp(const char *a,const char *b,uint key_length);
  int pack_cmp(const char *b,uint key_length);
  uint packed_col_length(const char *to);
  uint max_packed_col_length(uint max_length);
  uint size_of() const { return sizeof(*this); }
  enum_field_types real_type() const { return FIELD_TYPE_STRING; }
};


class Field_varstring :public Field_str {
  bool binary_flag;
public:
  Field_varstring(char *ptr_arg, uint32 len_arg,uchar *null_ptr_arg,
		  uint null_bit_arg,
		  enum utype unireg_check_arg, const char *field_name_arg,
		  struct st_table *table_arg,bool binary_arg)
    :Field_str(ptr_arg, len_arg, null_ptr_arg, null_bit_arg,
	       unireg_check_arg, field_name_arg, table_arg),
    binary_flag(binary_arg)
    {
      if (binary_arg)
	flags|=BINARY_FLAG;
    }
  Field_varstring(uint32 len_arg,bool maybe_null_arg, const char *field_name_arg,
		  struct st_table *table_arg, bool binary_arg)
    :Field_str((char*) 0,len_arg, maybe_null_arg ? (uchar*) "": 0,0,
	       NONE, field_name_arg, table_arg),
    binary_flag(binary_arg)
    {
      if (binary_arg)
	flags|=BINARY_FLAG;
    }

  enum_field_types type() const { return FIELD_TYPE_VAR_STRING; }
  enum ha_base_keytype key_type() const
    { return binary_flag ? HA_KEYTYPE_VARBINARY : HA_KEYTYPE_VARTEXT; }
  bool zero_pack() const { return 0; }
  bool binary() const { return binary_flag; }
  void reset(void) { bzero(ptr,field_length+2); }
  uint32 pack_length() const { return (uint32) field_length+2; }
  uint32 key_length() const { return (uint32) field_length; }
  void store(const char *to,uint length);
  void store(double nr);
  void store(longlong nr);
  double val_real(void);
  longlong val_int(void);
  String *val_str(String*,String *);
  int cmp(const char *,const char*);
  void sort_string(char *buff,uint length);
  void sql_type(String &str) const;
  char *pack(char *to, const char *from, uint max_length=~(uint) 0);
  const char *unpack(char* to, const char *from);
  int pack_cmp(const char *a, const char *b, uint key_length);
  int pack_cmp(const char *b, uint key_length);
  uint packed_col_length(const char *to);
  uint max_packed_col_length(uint max_length);
  uint size_of() const { return sizeof(*this); }
  enum_field_types real_type() const { return FIELD_TYPE_VAR_STRING; }
};


class Field_blob :public Field_str {
  uint packlength;
  String value;					// For temporaries
  bool binary_flag;
public:
  Field_blob(char *ptr_arg, uchar *null_ptr_arg, uint null_bit_arg,
	     enum utype unireg_check_arg, const char *field_name_arg,
	     struct st_table *table_arg,uint blob_pack_length,
	     bool binary_arg);
  Field_blob(uint32 len_arg,bool maybe_null_arg, const char *field_name_arg,
	     struct st_table *table_arg, bool binary_arg)
    :Field_str((char*) 0,len_arg, maybe_null_arg ? (uchar*) "": 0,0,
	       NONE, field_name_arg, table_arg),
    packlength(3),binary_flag(binary_arg)
    {
      flags|= BLOB_FLAG;
      if (binary_arg)
	flags|=BINARY_FLAG;
    }
  enum_field_types type() const { return FIELD_TYPE_BLOB;}
  enum ha_base_keytype key_type() const
    { return binary_flag ? HA_KEYTYPE_VARBINARY : HA_KEYTYPE_VARTEXT; }
  void store(const char *to,uint length);
  void store(double nr);
  void store(longlong nr);
  double val_real(void);
  longlong val_int(void);
  String *val_str(String*,String *);
  int cmp(const char *,const char*);
  int cmp(const char *a, ulong a_length, const char *b, ulong b_length);
  int cmp_offset(uint offset);
  int cmp_binary(const char *a,const char *b, ulong max_length=~0L);
  int cmp_binary_offset(uint row_offset);
  int key_cmp(const byte *,const byte*);
  int key_cmp(const byte *str, uint length);
  uint32 key_length() const { return 0; }
  void sort_string(char *buff,uint length);
  uint32 pack_length() const { return (uint32) (packlength+table->blob_ptr_size); }
  void reset(void) { bzero(ptr,packlength+sizeof(char*)); }
  void reset_fields() { bzero((char*) &value,sizeof(value)); }
  void store_length(ulong number);
  inline ulong get_length(uint row_offset=0)
  { return get_length(ptr+row_offset); }
  ulong get_length(const char *ptr);
  bool binary() const { return binary_flag; }
  inline void get_ptr(char **str)
    {
      memcpy_fixed(str,ptr+packlength,sizeof(char*));
    }
  inline void set_ptr(char *length,char *data)
    {
      memcpy(ptr,length,packlength);
      memcpy_fixed(ptr+packlength,&data,sizeof(char*));
    }
  inline void set_ptr(ulong length,char *data)
    {
      store_length(length);
      memcpy_fixed(ptr+packlength,&data,sizeof(char*));
    }
  void get_key_image(char *buff,uint length);
  void set_key_image(char *buff,uint length);
  void sql_type(String &str) const;
  inline bool copy()
  { char *tmp;
    get_ptr(&tmp);
    if (value.copy(tmp,get_length()))
    {
      Field_blob::reset();
      return 1;
    }
    tmp=(char*) value.ptr(); memcpy_fixed(ptr+packlength,&tmp,sizeof(char*));
    return 0;
  }
  char *pack(char *to, const char *from, uint max_length= ~(uint) 0);
  const char *unpack(char *to, const char *from);
#ifdef HAVE_GEMINI_DB
  char *pack_id(char *to, const char *from, ulonglong id, 
                uint max_length= ~(uint) 0);
  ulonglong get_id(const char *from);
  const char *unpack_id(char *to, const char *from, const char *bdata);
  inline void get_ptr_from_key_image(char **str,char *key_str)
  {
     *str = key_str + sizeof(uint16);
  }
  inline uint get_length_from_key_image(char *key_str)
  {
    return uint2korr(key_str);
  }
  enum_field_types blobtype() { return (packlength == 1 ? FIELD_TYPE_TINY_BLOB : FIELD_TYPE_BLOB);}
#endif
  char *pack_key(char *to, const char *from, uint max_length);
  char *pack_key_from_key_image(char* to, const char *from, uint max_length);
  int pack_cmp(const char *a, const char *b, uint key_length);
  int pack_cmp(const char *b, uint key_length);
  uint packed_col_length(const char *col_ptr)
  { return get_length(col_ptr)+packlength;}
  virtual uint max_packed_col_length(uint max_length)
  { return packlength+max_length; }

  inline void free() { value.free(); }
  inline void clear_temporary() { bzero((char*) &value,sizeof(value)); }
  friend void field_conv(Field *to,Field *from);
  uint size_of() const { return sizeof(*this); }
};


class Field_enum :public Field_str {
protected:
  uint packlength;
public:
  TYPELIB *typelib;
  Field_enum(char *ptr_arg, uint32 len_arg, uchar *null_ptr_arg,
		 uint null_bit_arg,
		 enum utype unireg_check_arg, const char *field_name_arg,
		 struct st_table *table_arg,uint packlength_arg,
		 TYPELIB *typelib_arg)
    :Field_str(ptr_arg, len_arg, null_ptr_arg, null_bit_arg,
	       unireg_check_arg, field_name_arg, table_arg),
    packlength(packlength_arg),typelib(typelib_arg)
    {
      flags|=ENUM_FLAG;
    }
  enum_field_types type() const { return FIELD_TYPE_STRING; }
  enum Item_result cmp_type () const { return INT_RESULT; }
  enum ha_base_keytype key_type() const;
  void store(const char *to,uint length);
  void store(double nr);
  void store(longlong nr);
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
  bool optimize_range() { return 0; }
  bool binary() const { return 0; }
  bool eq_def(Field *field);
};


class Field_set :public Field_enum {
public:
  Field_set(char *ptr_arg, uint32 len_arg, uchar *null_ptr_arg,
	    uint null_bit_arg,
	    enum utype unireg_check_arg, const char *field_name_arg,
	    struct st_table *table_arg,uint32 packlength_arg,
	    TYPELIB *typelib_arg)
    :Field_enum(ptr_arg, len_arg, null_ptr_arg, null_bit_arg,
		    unireg_check_arg, field_name_arg,
		    table_arg, packlength_arg,
		    typelib_arg)
    {
      flags=(flags & ~ENUM_FLAG) | SET_FLAG;
    }
  void store(const char *to,uint length);
  void store(double nr) { Field_set::store((longlong) nr); }
  void store(longlong nr);
  virtual bool zero_pack() const { return 1; }
  String *val_str(String*,String *);
  void sql_type(String &str) const;
  enum_field_types real_type() const { return FIELD_TYPE_SET; }
};


/*
** Create field class for CREATE TABLE
*/

class create_field :public Sql_alloc {
public:
  const char *field_name;
  const char *change;				// If done with alter table
  const char *after;				// Put column after this one
  Item	*def;					// Default value
  enum	enum_field_types sql_type;
  uint32 length;
  uint decimals,flags,pack_length;
  Field::utype unireg_check;
  TYPELIB *interval;				// Which interval to use
  Field *field;					// For alter table

  uint8 row,col,sc_length,interval_id;		// For rea_create_table
  uint	offset,pack_flag;
  create_field() :after(0) {}
  create_field(Field *field, Field *orig_field);
};


/*
** A class for sending info to the client
*/

class Send_field {
 public:
  const char *table_name,*col_name;
  uint length,flags,decimals;
  enum_field_types type;
  Send_field() {}
};


/*
** A class for quick copying data to fields
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
		  uchar *null_pos, uint null_bit,
		  uint pack_flag, Field::utype unireg_check,
		  TYPELIB *interval, const char *field_name,
		  struct st_table *table);
uint pack_length_to_packflag(uint type);
uint32 calc_pack_length(enum_field_types type,uint32 length);
bool set_field_to_null(Field *field);
uint find_enum(TYPELIB *typelib,const char *x, uint length);
ulonglong find_set(TYPELIB *typelib,const char *x, uint length);
bool test_if_int(const char *str,int length);

/*
** The following are for the interface with the .frm file
*/

#define FIELDFLAG_DECIMAL		1
#define FIELDFLAG_BINARY		1	// Shares same flag
#define FIELDFLAG_NUMBER		2
#define FIELDFLAG_ZEROFILL		4
#define FIELDFLAG_PACK			120	// Bits used for packing
#define FIELDFLAG_INTERVAL		256
#define FIELDFLAG_BITFIELD		512	// mangled with dec!
#define FIELDFLAG_BLOB			1024	// mangled with dec!
#define FIELDFLAG_LEFT_FULLSCREEN	8192
#define FIELDFLAG_RIGHT_FULLSCREEN	16384
#define FIELDFLAG_FORMAT_NUMBER		16384	// predit: ###,,## in output
#define FIELDFLAG_SUM			((uint) 32768)// predit: +#fieldflag
#define FIELDFLAG_MAYBE_NULL		((uint) 32768)// sql
#define FIELDFLAG_PACK_SHIFT		3
#define FIELDFLAG_DEC_SHIFT		8
#define FIELDFLAG_MAX_DEC		31
#define FIELDFLAG_NUM_SCREEN_TYPE	0x7F01
#define FIELDFLAG_ALFA_SCREEN_TYPE	0x7800

#define FIELD_SORT_REVERSE		16384

#define MTYP_TYPENR(type) (type & 127)	/* Remove bits from type */

#define f_is_dec(x)		((x) & FIELDFLAG_DECIMAL)
#define f_is_num(x)		((x) & FIELDFLAG_NUMBER)
#define f_is_zerofill(x)	((x) & FIELDFLAG_ZEROFILL)
#define f_is_packed(x)		((x) & FIELDFLAG_PACK)
#define f_packtype(x)		(((x) >> FIELDFLAG_PACK_SHIFT) & 15)
#define f_decimals(x)		(((x) >> FIELDFLAG_DEC_SHIFT) & FIELDFLAG_MAX_DEC)
#define f_is_alpha(x)		(!f_is_num(x))
#define f_is_binary(x)		((x) & FIELDFLAG_BINARY)
#define f_is_enum(x)	((x) & FIELDFLAG_INTERVAL)
#define f_is_bitfield(x)	((x) & FIELDFLAG_BITFIELD)
#define f_is_blob(x)		(((x) & (FIELDFLAG_BLOB | FIELDFLAG_NUMBER)) == FIELDFLAG_BLOB)
#define f_is_equ(x)		((x) & (1+2+FIELDFLAG_PACK+31*256))
#define f_settype(x)		(((int) x) << FIELDFLAG_PACK_SHIFT)
#define f_maybe_null(x)		(x & FIELDFLAG_MAYBE_NULL)
