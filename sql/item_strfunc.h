#ifndef ITEM_STRFUNC_INCLUDED
#define ITEM_STRFUNC_INCLUDED

/* Copyright (c) 2000, 2017, Oracle and/or its affiliates. All rights reserved.

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


/* This file defines all string functions */
#include "crypt_genhash_impl.h"       // CRYPT_MAX_PASSWORD_SIZE
#include "item_func.h"                // Item_func

class MY_LOCALE;

class Item_str_func :public Item_func
{
  typedef Item_func super;

public:
  Item_str_func() :Item_func() { decimals=NOT_FIXED_DEC; }
  explicit Item_str_func(const POS &pos) :super(pos) { decimals=NOT_FIXED_DEC; }

  Item_str_func(Item *a) :Item_func(a) {decimals=NOT_FIXED_DEC; }
  Item_str_func(const POS &pos, Item *a) :Item_func(pos, a)
  {decimals=NOT_FIXED_DEC; }

  Item_str_func(Item *a,Item *b) :Item_func(a,b) { decimals=NOT_FIXED_DEC; }
  Item_str_func(const POS &pos, Item *a,Item *b) :Item_func(pos, a,b)
  { decimals=NOT_FIXED_DEC; }

  Item_str_func(Item *a, Item *b, Item *c) :Item_func(a, b, c)
  { decimals=NOT_FIXED_DEC; }
  Item_str_func(const POS &pos, Item *a, Item *b, Item *c)
    :Item_func(pos, a,b,c)
  { decimals=NOT_FIXED_DEC; }

  Item_str_func(Item *a, Item *b, Item *c, Item *d) :Item_func(a, b, c, d)
  {decimals=NOT_FIXED_DEC; }
  Item_str_func(const POS &pos, Item *a, Item *b, Item *c, Item *d)
    :Item_func(pos, a,b,c,d)
  {decimals=NOT_FIXED_DEC; }

  Item_str_func(Item *a, Item *b, Item *c, Item *d, Item* e)
    :Item_func(a, b, c, d, e)
  {decimals=NOT_FIXED_DEC; }
  Item_str_func(const POS &pos, Item *a, Item *b, Item *c, Item *d, Item* e)
    :Item_func(pos, a, b, c, d, e)
  {decimals=NOT_FIXED_DEC; }

  Item_str_func(List<Item> &list) :Item_func(list) {decimals=NOT_FIXED_DEC; }
  Item_str_func(const POS &pos, PT_item_list *opt_list)
    :Item_func(pos, opt_list)
  {decimals=NOT_FIXED_DEC; }

  longlong val_int();
  double val_real();
  my_decimal *val_decimal(my_decimal *);
  bool get_date(MYSQL_TIME *ltime, my_time_flags_t fuzzydate)
  {
    return get_date_from_string(ltime, fuzzydate);
  }
  bool get_time(MYSQL_TIME *ltime)
  {
    return get_time_from_string(ltime);
  }
  enum Item_result result_type () const { return STRING_RESULT; }
  void left_right_max_length();
  bool fix_fields(THD *thd, Item **ref);
  String *val_str_from_val_str_ascii(String *str, String *str2);
};



/*
  Functions that return values with ASCII repertoire
*/
class Item_str_ascii_func :public Item_str_func
{
  String ascii_buf;
public:
  Item_str_ascii_func() :Item_str_func()
  { collation.set_repertoire(MY_REPERTOIRE_ASCII); }

  Item_str_ascii_func(Item *a) :Item_str_func(a)
  { collation.set_repertoire(MY_REPERTOIRE_ASCII); }
  Item_str_ascii_func(const POS &pos, Item *a) :Item_str_func(pos, a)
  { collation.set_repertoire(MY_REPERTOIRE_ASCII); }

  Item_str_ascii_func(Item *a,Item *b) :Item_str_func(a,b)
  { collation.set_repertoire(MY_REPERTOIRE_ASCII); }
  Item_str_ascii_func(const POS &pos, Item *a,Item *b) :Item_str_func(pos, a,b)
  { collation.set_repertoire(MY_REPERTOIRE_ASCII); }

  Item_str_ascii_func(Item *a,Item *b,Item *c) :Item_str_func(a,b,c)
  { collation.set_repertoire(MY_REPERTOIRE_ASCII); }
  Item_str_ascii_func(const POS &pos, Item *a,Item *b,Item *c)
    :Item_str_func(pos, a,b,c)
  { collation.set_repertoire(MY_REPERTOIRE_ASCII); }

  String *val_str(String *str)
  {
    return val_str_from_val_str_ascii(str, &ascii_buf);
  }
  virtual String *val_str_ascii(String *)= 0;
};


class Item_func_md5 :public Item_str_ascii_func
{
  String tmp_value;
public:
  Item_func_md5(const POS &pos, Item *a) :Item_str_ascii_func(pos, a) {}
  String *val_str_ascii(String *);
  void fix_length_and_dec();
  const char *func_name() const { return "md5"; }
};


class Item_func_sha :public Item_str_ascii_func
{
public:
  Item_func_sha(const POS &pos, Item *a) :Item_str_ascii_func(pos, a) {}
  String *val_str_ascii(String *);    
  void fix_length_and_dec();      
  const char *func_name() const { return "sha"; }	
};

class Item_func_sha2 :public Item_str_ascii_func
{
public:
  Item_func_sha2(const POS &pos, Item *a, Item *b)
    :Item_str_ascii_func(pos, a, b)
  {}
  String *val_str_ascii(String *);
  void fix_length_and_dec();
  const char *func_name() const { return "sha2"; }
};

class Item_func_to_base64 :public Item_str_ascii_func
{
  String tmp_value;
public:
  Item_func_to_base64(const POS &pos, Item *a) :Item_str_ascii_func(pos, a) {}
  String *val_str_ascii(String *);
  void fix_length_and_dec();
  const char *func_name() const { return "to_base64"; }
};

class Item_func_from_base64 :public Item_str_func
{
  String tmp_value;
public:
  Item_func_from_base64(const POS &pos, Item *a) :Item_str_func(pos, a) {}
  String *val_str(String *);
  void fix_length_and_dec();
  const char *func_name() const { return "from_base64"; }
};


class Item_func_aes_encrypt :public Item_str_func
{
  typedef Item_str_func super;
public:
  Item_func_aes_encrypt(const POS &pos, Item *a, Item *b)
    :Item_str_func(pos, a, b)
  {}
  Item_func_aes_encrypt(const POS &pos, Item *a, Item *b, Item *c)
    :Item_str_func(pos, a, b, c)
  {}

  virtual bool itemize(Parse_context *pc, Item **res);
  String *val_str(String *);
  void fix_length_and_dec();
  const char *func_name() const { return "aes_encrypt"; }
};

class Item_func_aes_decrypt :public Item_str_func	
{
  typedef Item_str_func super;
public:
  Item_func_aes_decrypt(const POS &pos, Item *a, Item *b)
    :Item_str_func(pos, a, b)
  {}
  Item_func_aes_decrypt(const POS &pos, Item *a, Item *b, Item *c)
    :Item_str_func(pos, a, b, c)
  {}

  virtual bool itemize(Parse_context *pc, Item **res);
  String *val_str(String *);
  void fix_length_and_dec();
  const char *func_name() const { return "aes_decrypt"; }
};


class Item_func_random_bytes : public Item_str_func
{
  typedef Item_str_func super;

  /** limitation from the SSL library */
  static const longlong MAX_RANDOM_BYTES_BUFFER;
public:
  Item_func_random_bytes(const POS &pos, Item *a) : Item_str_func(pos, a)
  {}

  virtual bool itemize(Parse_context *pc, Item **res);
  void fix_length_and_dec();
  String *val_str(String *a);

  const char *func_name() const
  {
    return "random_bytes";
  }

};

class Item_func_concat :public Item_str_func
{
  String tmp_value;
public:
  Item_func_concat(const POS &pos, PT_item_list *opt_list)
    :Item_str_func(pos, opt_list)
  {}

  Item_func_concat(Item *a,Item *b) :Item_str_func(a,b) {}
  Item_func_concat(const POS &pos, Item *a,Item *b) :Item_str_func(pos, a,b) {}

  String *val_str(String *);
  void fix_length_and_dec();
  const char *func_name() const { return "concat"; }
};

class Item_func_concat_ws :public Item_str_func
{
  String tmp_value;
public:
  Item_func_concat_ws(List<Item> &list) :Item_str_func(list) {}
  Item_func_concat_ws(const POS &pos, PT_item_list *opt_list)
    :Item_str_func(pos, opt_list)
  {}
  String *val_str(String *);
  void fix_length_and_dec();
  const char *func_name() const { return "concat_ws"; }
  table_map not_null_tables() const { return 0; }
};

class Item_func_reverse :public Item_str_func
{
  String tmp_value;
public:
  Item_func_reverse(Item *a) :Item_str_func(a) {}
  Item_func_reverse(const POS &pos, Item *a) :Item_str_func(pos, a) {}

  String *val_str(String *);
  void fix_length_and_dec();
  const char *func_name() const { return "reverse"; }
};


class Item_func_replace :public Item_str_func
{
  String tmp_value,tmp_value2;
  /// Holds result in case we need to allocate our own result buffer.
  String tmp_value_res;
public:
  Item_func_replace(const POS &pos, Item *org,Item *find,Item *replace)
    :Item_str_func(pos, org,find,replace)
  {}
  String *val_str(String *);
  void fix_length_and_dec();
  const char *func_name() const { return "replace"; }
};


class Item_func_insert :public Item_str_func
{
  String tmp_value;
  /// Holds result in case we need to allocate our own result buffer.
  String tmp_value_res;
public:
  Item_func_insert(const POS &pos,
                   Item *org, Item *start, Item *length, Item *new_str)
    :Item_str_func(pos, org,start,length,new_str)
  {}
  String *val_str(String *);
  void fix_length_and_dec();
  const char *func_name() const { return "insert"; }
};


class Item_str_conv :public Item_str_func
{
protected:
  uint multiply;
  my_charset_conv_case converter;
  String tmp_value;
public:
  Item_str_conv(const POS &pos, Item *item) :Item_str_func(pos, item) {}
  String *val_str(String *);
};


class Item_func_lower :public Item_str_conv
{
public:
  Item_func_lower(const POS &pos, Item *item) :Item_str_conv(pos, item) {}
  const char *func_name() const { return "lower"; }
  void fix_length_and_dec();
};

class Item_func_upper :public Item_str_conv
{
public:
  Item_func_upper(const POS &pos, Item *item) :Item_str_conv(pos, item) {}
  const char *func_name() const { return "upper"; }
  void fix_length_and_dec();
};


class Item_func_left :public Item_str_func
{
  String tmp_value;
public:
  Item_func_left(const POS &pos, Item *a,Item *b) :Item_str_func(pos, a,b) {}
  String *val_str(String *);
  void fix_length_and_dec();
  const char *func_name() const { return "left"; }
};


class Item_func_right :public Item_str_func
{
  String tmp_value;
public:
  Item_func_right(const POS &pos, Item *a,Item *b) :Item_str_func(pos, a,b) {}
  String *val_str(String *);
  void fix_length_and_dec();
  const char *func_name() const { return "right"; }
};


class Item_func_substr :public Item_str_func
{
  typedef Item_str_func super;

  String tmp_value;
public:
  Item_func_substr(Item *a,Item *b) :Item_str_func(a,b) {}
  Item_func_substr(const POS &pos, Item *a,Item *b) :super(pos, a,b) {}

  Item_func_substr(Item *a,Item *b,Item *c) :Item_str_func(a,b,c) {}
  Item_func_substr(const POS &pos, Item *a,Item *b,Item *c) :super(pos, a, b, c)
  {}

  String *val_str(String *);
  void fix_length_and_dec();
  const char *func_name() const { return "substr"; }
};


class Item_func_substr_index :public Item_str_func
{
  String tmp_value;
public:
  Item_func_substr_index(const POS &pos, Item *a,Item *b, Item *c)
    :Item_str_func(pos, a, b, c)
  {}
  String *val_str(String *);
  void fix_length_and_dec();
  const char *func_name() const { return "substring_index"; }
};


class Item_func_trim :public Item_str_func
{
public:
  /**
    Why all the trim modes in this enum?
    We need to maintain parsing information, so that our print() function
    can reproduce correct messages and view definitions.
   */
  enum TRIM_MODE
  {
    TRIM_BOTH_DEFAULT,
    TRIM_BOTH,
    TRIM_LEADING,
    TRIM_TRAILING,
    TRIM_LTRIM,
    TRIM_RTRIM
  };

private:
  String tmp_value;
  String remove;
  const TRIM_MODE m_trim_mode;
  const bool m_trim_leading;
  const bool m_trim_trailing;

public:
  Item_func_trim(Item *a, Item *b, TRIM_MODE tm)
    : Item_str_func(a,b), m_trim_mode(tm),
      m_trim_leading(trim_leading()), m_trim_trailing(trim_trailing())
  {}

  Item_func_trim(const POS &pos, Item *a, Item *b, TRIM_MODE tm)
    : Item_str_func(pos, a,b), m_trim_mode(tm),
      m_trim_leading(trim_leading()), m_trim_trailing(trim_trailing())
  {}

  Item_func_trim(Item *a, TRIM_MODE tm)
    : Item_str_func(a), m_trim_mode(tm),
      m_trim_leading(trim_leading()), m_trim_trailing(trim_trailing())
  {}

  Item_func_trim(const POS &pos, Item *a, TRIM_MODE tm)
    : Item_str_func(pos, a), m_trim_mode(tm),
      m_trim_leading(trim_leading()), m_trim_trailing(trim_trailing())
  {}

  bool trim_leading() const
  {
    return
      m_trim_mode == TRIM_BOTH_DEFAULT ||
      m_trim_mode == TRIM_BOTH ||
      m_trim_mode == TRIM_LEADING ||
      m_trim_mode == TRIM_LTRIM;
  }

  bool trim_trailing() const
  {
    return
      m_trim_mode == TRIM_BOTH_DEFAULT ||
      m_trim_mode == TRIM_BOTH ||
      m_trim_mode == TRIM_TRAILING ||
      m_trim_mode == TRIM_RTRIM;
  }

  String *val_str(String *);
  void fix_length_and_dec();
  const char *func_name() const
  {
    switch(m_trim_mode) {
    case TRIM_BOTH_DEFAULT: return "trim";
    case TRIM_BOTH:         return "trim";
    case TRIM_LEADING:      return "ltrim";
    case TRIM_TRAILING:     return "rtrim";
    case TRIM_LTRIM:        return "ltrim";
    case TRIM_RTRIM:        return "rtrim";
    }
    return NULL;
  }
  virtual void print(String *str, enum_query_type query_type);
};


/*
  Item_func_password -- new (4.1.1) PASSWORD() function implementation.
  Returns strcat('*', octet2hex(sha1(sha1(password)))). '*' stands for new
  password format, sha1(sha1(password) is so-called hash_stage2 value.
  Length of returned string is always 41 byte. To find out how entire
  authentication procedure works, see comments in password.c.
*/

class Item_func_password :public Item_str_ascii_func
{
  char m_hashed_password_buffer[CRYPT_MAX_PASSWORD_SIZE + 1];
  unsigned int m_hashed_password_buffer_len;
  bool m_recalculate_password;
public:
  Item_func_password(Item *a) : Item_str_ascii_func(a)
  {
    m_hashed_password_buffer_len= 0;
    m_recalculate_password= false;
  }
  String *val_str_ascii(String *str);
  void fix_length_and_dec();
  const char *func_name() const { return "password"; }
};


class Item_func_des_encrypt :public Item_str_func
{
  String tmp_value,tmp_arg;
public:
  Item_func_des_encrypt(const POS &pos, Item *a) :Item_str_func(pos, a) {}
  Item_func_des_encrypt(const POS &pos, Item *a, Item *b)
    : Item_str_func(pos, a, b)
  {}
  String *val_str(String *);
  void fix_length_and_dec()
  {
    maybe_null=1;
    /* 9 = MAX ((8- (arg_len % 8)) + 1) */
    max_length = args[0]->max_length + 9;
  }
  const char *func_name() const { return "des_encrypt"; }
};

class Item_func_des_decrypt :public Item_str_func
{
  String tmp_value;
public:
  Item_func_des_decrypt(const POS &pos, Item *a) :Item_str_func(pos, a) {}
  Item_func_des_decrypt(const POS &pos, Item *a, Item *b)
    : Item_str_func(pos, a, b)
  {}
  String *val_str(String *);
  void fix_length_and_dec()
  {
    maybe_null=1;
    /* 9 = MAX ((8- (arg_len % 8)) + 1) */
    max_length= args[0]->max_length;
    if (max_length >= 9U)
      max_length-= 9U;
  }
  const char *func_name() const { return "des_decrypt"; }
};

class Item_func_encrypt :public Item_str_func
{
  typedef Item_str_func super;

  String tmp_value;

  /* Encapsulate common constructor actions */
  void constructor_helper()
  {
    collation.set(&my_charset_bin);
  }
public:
  Item_func_encrypt(const POS &pos, Item *a) :Item_str_func(pos, a)
  {
    constructor_helper();
  }
  Item_func_encrypt(const POS &pos, Item *a, Item *b): Item_str_func(pos, a, b)
  {
    constructor_helper();
  }

  virtual bool itemize(Parse_context *pc, Item **res);
  String *val_str(String *);
  void fix_length_and_dec() { maybe_null=1; max_length = 13; }
  const char *func_name() const { return "encrypt"; }
  bool check_gcol_func_processor(uchar *int_arg)
  { return true; }
};

#include "sql_crypt.h"


class Item_func_encode :public Item_str_func
{
private:
  /** Whether the PRNG has already been seeded. */
  bool seeded;
  /// Holds result in case we need to allocate our own result buffer.
  String tmp_value_res;
protected:
  SQL_CRYPT sql_crypt;
public:
  Item_func_encode(const POS &pos, Item *a, Item *seed)
    :Item_str_func(pos, a, seed)
  {}
  String *val_str(String *);
  void fix_length_and_dec();
  const char *func_name() const { return "encode"; }
protected:
  virtual void crypto_transform(String *);
private:
  /** Provide a seed for the PRNG sequence. */
  bool seed();
};


class Item_func_decode :public Item_func_encode
{
public:
  Item_func_decode(const POS &pos, Item *a, Item *seed)
    :Item_func_encode(pos, a, seed)
  {}
  const char *func_name() const { return "decode"; }
protected:
  void crypto_transform(String *);
};


class Item_func_sysconst :public Item_str_func
{
  typedef Item_str_func super;

public:
  Item_func_sysconst()
  { collation.set(system_charset_info,DERIVATION_SYSCONST); }
  explicit Item_func_sysconst(const POS &pos) : super(pos)
  { collation.set(system_charset_info,DERIVATION_SYSCONST); }

  Item *safe_charset_converter(const CHARSET_INFO *tocs);
  /*
    Used to create correct Item name in new converted item in
    safe_charset_converter, return string representation of this function
    call
  */
  virtual const Name_string fully_qualified_func_name() const = 0;
  bool check_gcol_func_processor(uchar *int_arg)
  { return true; }
};


class Item_func_database :public Item_func_sysconst
{
  typedef Item_func_sysconst super;

public:
  explicit Item_func_database(const POS &pos) :Item_func_sysconst(pos) {}

  virtual bool itemize(Parse_context *pc, Item **res);

  String *val_str(String *);
  void fix_length_and_dec()
  {
    max_length= MAX_FIELD_NAME * system_charset_info->mbmaxlen;
    maybe_null=1;
  }
  const char *func_name() const { return "database"; }
  const Name_string fully_qualified_func_name() const
  { return NAME_STRING("database()"); }
};


class Item_func_user :public Item_func_sysconst
{
  typedef Item_func_sysconst super;

protected:
  bool init (const char *user, const char *host);
  type_conversion_status save_in_field_inner(Field *field, bool no_conversions)
  {
    return save_str_value_in_field(field, &str_value);
  }

public:
  Item_func_user()
  {
    str_value.set("", 0, system_charset_info);
  }
  explicit Item_func_user(const POS &pos) : super(pos)
  {
    str_value.set("", 0, system_charset_info);
  }

  virtual bool itemize(Parse_context *pc, Item **res);

  String *val_str(String *)
  {
    DBUG_ASSERT(fixed == 1);
    return (null_value ? 0 : &str_value);
  }
  bool fix_fields(THD *thd, Item **ref);
  void fix_length_and_dec()
  {
    max_length= (USERNAME_LENGTH +
                 (HOSTNAME_LENGTH + 1) * SYSTEM_CHARSET_MBMAXLEN);
  }
  const char *func_name() const { return "user"; }
  const Name_string fully_qualified_func_name() const
  { return NAME_STRING("user()"); }
};


class Item_func_current_user :public Item_func_user
{
  typedef Item_func_user super;

  Name_resolution_context *context;

public:
  explicit Item_func_current_user(const POS &pos) : super(pos) {}
  
  virtual bool itemize(Parse_context *pc, Item **res);

  bool fix_fields(THD *thd, Item **ref);
  const char *func_name() const { return "current_user"; }
  const Name_string fully_qualified_func_name() const
  { return NAME_STRING("current_user()"); }
};


class Item_func_soundex :public Item_str_func
{
  String tmp_value;
public:
  Item_func_soundex(Item *a) :Item_str_func(a) {}
  Item_func_soundex(const POS &pos, Item *a) :Item_str_func(pos, a) {}
  String *val_str(String *);
  void fix_length_and_dec();
  const char *func_name() const { return "soundex"; }
};


/**
  This class handles two forms of the same function:

  <string> = ST_GEOHASH(<point>, <maxlength>);
  <string> = ST_GEOHASH(<longitude>, <latitude>, <maxlength>)

  It returns an encoded geohash string, no longer than <maxlength> characters
  long. Note that it might be shorter than <maxlength>.
*/
class Item_func_geohash :public Item_str_ascii_func
{
private:
  /// The latitude argument supplied by the user (directly or by a POINT).
  double latitude;
  /// The longitude argument supplied by the user (directly or by a POINT).
  double longitude;
  /// The maximum output length of the geohash, supplied by the user.
  uint geohash_max_output_length;

  /** 
    The maximum input latitude. For now, this is set to 90.0. It can be
    changed to support a different range than the normal [90, -90].
  */
  const double max_latitude;

  /**
    The minimum input latitude. For now, this is set to -90.0. It can be
    changed to support a different range than the normal [90, -90].
  */
  const double min_latitude;

  /**
    The maximum input longitude. For now, this is set to 180.0. It can be
    changed to support a different range than the normal [180, -180].
  */
  const double max_longitude;

  /**
    The minimum input longitude. For now, this is set to -180.0. It can be
    changed to support a different range than the normal [180, -180].
  */
  const double min_longitude;

  /**
    The absolute upper limit of geohash output length. User will get an error
    if they supply a max geohash length argument greater than this.
  */
  const uint upper_limit_output_length;
public:
  Item_func_geohash(const POS &pos, Item *point, Item *length)
    :Item_str_ascii_func(pos, point, length), max_latitude(90.0),
    min_latitude(-90.0), max_longitude(180.0), min_longitude(-180.0),
    upper_limit_output_length(100)
  {}
  Item_func_geohash(const POS &pos, Item *longitude, Item *latitude,
                    Item *length)
    :Item_str_ascii_func(pos, longitude, latitude, length), max_latitude(90.0),
    min_latitude(-90.0), max_longitude(180.0), min_longitude(-180.0),
    upper_limit_output_length(100)
  {}
  String *val_str_ascii(String *);
  void fix_length_and_dec();
  bool fix_fields(THD *thd, Item **ref);
  const char *func_name() const { return "st_geohash"; }
  char char_to_base32(char char_input);
  void encode_bit(double *upper_value, double *lower_value,
                  double target_value, char *char_value, int bit_number);
  bool fill_and_check_fields();
  bool check_valid_latlong_type(Item *ref);
  static bool is_item_null(Item *item);
};


class Item_func_elt :public Item_str_func
{
public:
  Item_func_elt(const POS &pos, PT_item_list *opt_list)
    :Item_str_func(pos, opt_list)
  {}
  double val_real();
  longlong val_int();
  String *val_str(String *str);
  void fix_length_and_dec();
  const char *func_name() const { return "elt"; }
};


class Item_func_make_set :public Item_str_func
{
  typedef Item_str_func super;

  Item *item;
  String tmp_str;

public:
  Item_func_make_set(const POS &pos, Item *a, PT_item_list *opt_list)
    :Item_str_func(pos, opt_list), item(a)
  {}

  virtual bool itemize(Parse_context *pc, Item **res);
  String *val_str(String *str);
  bool fix_fields(THD *thd, Item **ref)
  {
    DBUG_ASSERT(fixed == 0);
    bool res= ((!item->fixed && item->fix_fields(thd, &item)) ||
               item->check_cols(1) ||
               Item_func::fix_fields(thd, ref));
    maybe_null|= item->maybe_null;
    return res;
  }
  void split_sum_func(THD *thd, Ref_ptr_array ref_pointer_array,
                      List<Item> &fields);
  void fix_length_and_dec();
  void update_used_tables();
  const char *func_name() const { return "make_set"; }

  bool walk(Item_processor processor, enum_walk walk, uchar *arg)
  {
    if ((walk & WALK_PREFIX) && (this->*processor)(arg))
      return true;
    if (item->walk(processor, walk, arg))
      return true;
    for (uint i= 0; i < arg_count; i++)
    {
      if (args[i]->walk(processor, walk, arg))
        return true;
    }
    return ((walk & WALK_POSTFIX) && (this->*processor)(arg));
  }

  Item *transform(Item_transformer transformer, uchar *arg);
  virtual void print(String *str, enum_query_type query_type);
};


class Item_func_format :public Item_str_ascii_func
{
  String tmp_str;
  MY_LOCALE *locale;
public:
  Item_func_format(const POS &pos, Item *org, Item *dec)
    : Item_str_ascii_func(pos, org, dec)
  {}
  Item_func_format(const POS &pos, Item *org, Item *dec, Item *lang)
    : Item_str_ascii_func(pos, org, dec, lang)
  {}
  
  MY_LOCALE *get_locale(Item *item);
  String *val_str_ascii(String *);
  void fix_length_and_dec();
  const char *func_name() const { return "format"; }
  virtual void print(String *str, enum_query_type query_type);
};


class Item_func_char :public Item_str_func
{
public:
  Item_func_char(const POS &pos, PT_item_list *list) :Item_str_func(pos, list)
  { collation.set(&my_charset_bin); }
  Item_func_char(const POS &pos, PT_item_list *list, const CHARSET_INFO *cs)
    : Item_str_func(pos, list)
  { collation.set(cs); }  
  String *val_str(String *);
  void fix_length_and_dec() 
  {
    max_length= arg_count * 4;
  }
  const char *func_name() const { return "char"; }
};


class Item_func_repeat :public Item_str_func
{
  String tmp_value;
public:
  Item_func_repeat(const POS &pos, Item *arg1,Item *arg2)
    :Item_str_func(pos, arg1,arg2)
  {}
  String *val_str(String *);
  void fix_length_and_dec();
  const char *func_name() const { return "repeat"; }
};


class Item_func_space :public Item_str_func
{
public:
  Item_func_space(const POS &pos, Item *arg1) :Item_str_func(pos, arg1) {}
  String *val_str(String *);
  void fix_length_and_dec();
  const char *func_name() const { return "space"; }
};


class Item_func_rpad :public Item_str_func
{
  String tmp_value, rpad_str;
public:
  Item_func_rpad(const POS &pos, Item *arg1, Item *arg2, Item *arg3)
    :Item_str_func(pos, arg1, arg2, arg3)
  {}
  String *val_str(String *);
  void fix_length_and_dec();
  const char *func_name() const { return "rpad"; }
};


class Item_func_lpad :public Item_str_func
{
  String tmp_value, lpad_str;
public:
  Item_func_lpad(const POS &pos, Item *arg1, Item *arg2, Item *arg3)
    :Item_str_func(pos , arg1, arg2, arg3)
  {}
  String *val_str(String *);
  void fix_length_and_dec();
  const char *func_name() const { return "lpad"; }
};


class Item_func_conv :public Item_str_func
{
public:
  Item_func_conv(const POS &pos, Item *a,Item *b,Item *c)
    :Item_str_func(pos, a,b,c)
  {}
  const char *func_name() const { return "conv"; }
  String *val_str(String *);
  void fix_length_and_dec();
};


class Item_func_hex :public Item_str_ascii_func
{
  String tmp_value;
public:
  Item_func_hex(const POS &pos, Item *a) :Item_str_ascii_func(pos, a) {}
  const char *func_name() const { return "hex"; }
  String *val_str_ascii(String *);
  void fix_length_and_dec()
  {
    collation.set(default_charset());
    decimals=0;
    fix_char_length(args[0]->max_length * 2);
  }
};

class Item_func_unhex :public Item_str_func
{
  String tmp_value;
public:
  Item_func_unhex(const POS &pos, Item *a) :Item_str_func(pos, a) 
  { 
    /* there can be bad hex strings */
    maybe_null= 1; 
  }
  const char *func_name() const { return "unhex"; }
  String *val_str(String *);
  void fix_length_and_dec()
  {
    collation.set(&my_charset_bin);
    decimals=0;
    max_length=(1+args[0]->max_length)/2;
  }
};


#ifndef DBUG_OFF
class Item_func_like_range :public Item_str_func
{
protected:
  String min_str;
  String max_str;
  const bool is_min;
public:
  Item_func_like_range(const POS &pos, Item *a, Item *b, bool is_min_arg)
    :Item_str_func(pos, a, b), is_min(is_min_arg)
  { maybe_null= 1; }
  String *val_str(String *);
  void fix_length_and_dec()
  {
    collation.set(args[0]->collation);
    decimals=0;
    max_length= MAX_BLOB_WIDTH;
  }
};


class Item_func_like_range_min :public Item_func_like_range
{
public:
  Item_func_like_range_min(const POS &pos, Item *a, Item *b) 
    :Item_func_like_range(pos, a, b, true)
  { }
  const char *func_name() const { return "like_range_min"; }
};


class Item_func_like_range_max :public Item_func_like_range
{
public:
  Item_func_like_range_max(const POS &pos, Item *a, Item *b)
    :Item_func_like_range(pos, a, b, false)
  { }
  const char *func_name() const { return "like_range_max"; }
};
#endif


class Item_char_typecast :public Item_str_func
{
  int cast_length;
  const CHARSET_INFO *cast_cs, *from_cs;
  bool charset_conversion;
  String tmp_value;
public:
  Item_char_typecast(Item *a, int length_arg, const CHARSET_INFO *cs_arg)
    :Item_str_func(a), cast_length(length_arg), cast_cs(cs_arg) {}
  Item_char_typecast(const POS &pos, Item *a, int length_arg,
                     const CHARSET_INFO *cs_arg)
    :Item_str_func(pos, a), cast_length(length_arg), cast_cs(cs_arg) 
  {}
  enum Functype functype() const { return TYPECAST_FUNC; }
  bool eq(const Item *item, bool binary_cmp) const;
  const char *func_name() const { return "cast_as_char"; }
  String *val_str(String *a);
  void fix_length_and_dec();
  virtual void print(String *str, enum_query_type query_type);
};


class Item_func_binary :public Item_str_func
{
public:
  Item_func_binary(const POS &pos, Item *a) :Item_str_func(pos, a) {}
  String *val_str(String *a)
  {
    DBUG_ASSERT(fixed == 1);
    String *tmp=args[0]->val_str(a);
    null_value=args[0]->null_value;
    if (tmp)
      tmp->set_charset(&my_charset_bin);
    return tmp;
  }
  void fix_length_and_dec()
  {
    collation.set(&my_charset_bin);
    max_length=args[0]->max_length;
  }
  virtual void print(String *str, enum_query_type query_type);
  const char *func_name() const { return "cast_as_binary"; }
  enum Functype functype() const { return TYPECAST_FUNC; }
};


class Item_load_file :public Item_str_func
{
  typedef Item_str_func super;

  String tmp_value;
public:
  Item_load_file(const POS &pos, Item *a) :Item_str_func(pos, a) {}

  virtual bool itemize(Parse_context *pc, Item **res);
  String *val_str(String *);
  const char *func_name() const { return "load_file"; }
  void fix_length_and_dec()
  {
    collation.set(&my_charset_bin, DERIVATION_COERCIBLE);
    maybe_null=1;
    max_length=MAX_BLOB_WIDTH;
  }
  bool check_gcol_func_processor(uchar *int_arg)
  { return true; }
};


class Item_func_export_set: public Item_str_func
{
 public:
  Item_func_export_set(const POS &pos, Item *a, Item *b, Item* c)
    :Item_str_func(pos, a, b, c)
  {}
  Item_func_export_set(const POS &pos, Item *a, Item *b, Item* c, Item* d)
    :Item_str_func(pos, a, b, c, d)
  {}
  Item_func_export_set(const POS &pos,
                       Item *a, Item *b, Item* c, Item* d, Item* e)
    :Item_str_func(pos, a, b, c, d, e)
  {}
  String  *val_str(String *str);
  void fix_length_and_dec();
  const char *func_name() const { return "export_set"; }
};

class Item_func_quote :public Item_str_func
{
  String tmp_value;
public:
  Item_func_quote(const POS &pos, Item *a) :Item_str_func(pos, a) {}
  const char *func_name() const { return "quote"; }
  String *val_str(String *);
  void fix_length_and_dec()
  {
    collation.set(args[0]->collation);
    ulong max_result_length= (ulong) args[0]->max_length * 2 +
                                  2 * collation.collation->mbmaxlen;
    max_length= std::min<ulong>(max_result_length, MAX_BLOB_WIDTH);
  }
};

class Item_func_conv_charset :public Item_str_func
{
  bool use_cached_value;
  String tmp_value;
public:
  bool safe;
  const CHARSET_INFO *conv_charset; // keep it public
  Item_func_conv_charset(const POS &pos, Item *a, const CHARSET_INFO *cs)
  : Item_str_func(pos, a) 
  { conv_charset= cs; use_cached_value= 0; safe= 0; }
  Item_func_conv_charset(Item *a, const CHARSET_INFO *cs,
                         bool cache_if_const) :Item_str_func(a)
  {
    DBUG_ASSERT(is_fixed_or_outer_ref(args[0]));

    conv_charset= cs;
    if (cache_if_const && args[0]->const_item())
    {
      uint errors= 0;
      String tmp, *str= args[0]->val_str(&tmp);
      if (!str || str_value.copy(str->ptr(), str->length(),
                                 str->charset(), conv_charset, &errors))
        null_value= 1;
      use_cached_value= 1;
      str_value.mark_as_const();
      safe= (errors == 0);
    }
    else
    {
      use_cached_value= 0;
      /*
        Conversion from and to "binary" is safe.
        Conversion to Unicode is safe.
        Other kind of conversions are potentially lossy.
      */
      safe= (args[0]->collation.collation == &my_charset_bin ||
             cs == &my_charset_bin ||
             (cs->state & MY_CS_UNICODE));
    }
  }
  String *val_str(String *);
  void fix_length_and_dec();
  const char *func_name() const { return "convert"; }
  virtual void print(String *str, enum_query_type query_type);
};

class Item_func_set_collation :public Item_str_func
{
  typedef Item_str_func super;

  LEX_STRING collation_string;
public:
  Item_func_set_collation(const POS &pos, Item *a,
                          const LEX_STRING &collation_string_arg)
    :super(pos, a, NULL), collation_string(collation_string_arg)
  {}

  virtual bool itemize(Parse_context *pc, Item **res);
  String *val_str(String *);
  void fix_length_and_dec();
  bool eq(const Item *item, bool binary_cmp) const;
  const char *func_name() const { return "collate"; }
  enum Functype functype() const { return COLLATE_FUNC; }
  virtual void print(String *str, enum_query_type query_type);
  Item_field *field_for_view_update()
  {
    /* this function is transparent for view updating */
    return args[0]->field_for_view_update();
  }
};

class Item_func_charset :public Item_str_func
{
public:
  Item_func_charset(const POS &pos, Item *a) :Item_str_func(pos, a) {}
  String *val_str(String *);
  const char *func_name() const { return "charset"; }
  void fix_length_and_dec()
  {
     collation.set(system_charset_info);
     max_length= 64 * collation.collation->mbmaxlen; // should be enough
     maybe_null= 0;
  };
  table_map not_null_tables() const { return 0; }
};

class Item_func_collation :public Item_str_func
{
public:
  Item_func_collation(const POS &pos, Item *a) :Item_str_func(pos, a) {}
  String *val_str(String *);
  const char *func_name() const { return "collation"; }
  void fix_length_and_dec()
  {
     collation.set(system_charset_info);
     max_length= 64 * collation.collation->mbmaxlen; // should be enough
     maybe_null= 0;
  };
  table_map not_null_tables() const { return 0; }
};

class Item_func_weight_string :public Item_str_func
{
  typedef Item_str_func super;

  String tmp_value;
  uint flags;
  uint nweights;
  uint result_length;
  Field *field;
  bool as_binary;
public:
  Item_func_weight_string(const POS &pos, Item *a, uint result_length_arg,
                          uint nweights_arg, uint flags_arg,
                          bool as_binary_arg= false)
  :Item_str_func(pos, a), field(NULL), as_binary(as_binary_arg)
  {
    nweights= nweights_arg;
    flags= flags_arg;
    result_length= result_length_arg;
  }

  virtual bool itemize(Parse_context *pc, Item **res);

  const char *func_name() const { return "weight_string"; }
  bool eq(const Item *item, bool binary_cmp) const;
  String *val_str(String *);
  void fix_length_and_dec();
  virtual void print(String *str, enum_query_type query_type);
};

class Item_func_crc32 :public Item_int_func
{
  String value;
public:
  Item_func_crc32(const POS &pos, Item *a) :Item_int_func(pos, a)
  { unsigned_flag= 1; }
  const char *func_name() const { return "crc32"; }
  void fix_length_and_dec() { max_length=10; }
  longlong val_int();
};

class Item_func_uncompressed_length : public Item_int_func
{
  String value;
public:
  Item_func_uncompressed_length(const POS &pos, Item *a) :Item_int_func(pos, a)
  {}
  const char *func_name() const{return "uncompressed_length";}
  void fix_length_and_dec() { max_length=10; }
  longlong val_int();
};

#ifdef HAVE_COMPRESS
#define ZLIB_DEPENDED_FUNCTION ;
#else
#define ZLIB_DEPENDED_FUNCTION { null_value=1; return 0; }
#endif

class Item_func_compress: public Item_str_func
{
  String buffer;
public:
  Item_func_compress(const POS &pos, Item *a):Item_str_func(pos, a){}
  void fix_length_and_dec(){max_length= (args[0]->max_length*120)/100+12;}
  const char *func_name() const{return "compress";}
  String *val_str(String *) ZLIB_DEPENDED_FUNCTION
};

class Item_func_uncompress: public Item_str_func
{
  String buffer;
public:
  Item_func_uncompress(const POS &pos, Item *a): Item_str_func(pos, a) {}
  void fix_length_and_dec(){ maybe_null= 1; max_length= MAX_BLOB_WIDTH; }
  const char *func_name() const{return "uncompress";}
  String *val_str(String *) ZLIB_DEPENDED_FUNCTION
};

class Item_func_uuid: public Item_str_func
{
  typedef Item_str_func super;
public:
  Item_func_uuid(): Item_str_func() {}
  explicit
  Item_func_uuid(const POS &pos): Item_str_func(pos) {}

  virtual bool itemize(Parse_context *pc, Item **res);
  void fix_length_and_dec()
  {
    collation.set(system_charset_info,
                  DERIVATION_COERCIBLE, MY_REPERTOIRE_ASCII);
    fix_char_length(UUID_LENGTH);
  }
  const char *func_name() const{ return "uuid"; }
  String *val_str(String *);
  bool check_gcol_func_processor(uchar *int_arg)
  { return true; }
};

class Item_func_gtid_subtract: public Item_str_ascii_func
{
  String buf1, buf2;
public:
  Item_func_gtid_subtract(const POS &pos, Item *a, Item *b)
    :Item_str_ascii_func(pos, a, b)
  {}
  void fix_length_and_dec();
  const char *func_name() const{ return "gtid_subtract"; }
  String *val_str_ascii(String *);
};

#endif /* ITEM_STRFUNC_INCLUDED */
