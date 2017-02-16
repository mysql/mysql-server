#ifndef ITEM_STRFUNC_INCLUDED
#define ITEM_STRFUNC_INCLUDED

/*
   Copyright (c) 2000, 2011, Oracle and/or its affiliates.
   Copyright (c) 2009, 2013, Monty Program Ab.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA */


/* This file defines all string functions */

#ifdef USE_PRAGMA_INTERFACE
#pragma interface			/* gcc class implementation */
#endif

extern size_t username_char_length;

class MY_LOCALE;

class Item_str_func :public Item_func
{
protected:
  /**
     Sets the result value of the function an empty string, using the current
     character set. No memory is allocated.
     @retval A pointer to the str_value member.
   */
  String *make_empty_result()
  {
    /*
      Reset string length to an empty string. We don't use str_value.set() as
      we don't want to free and potentially have to reallocate the buffer
      for each call.
    */
    str_value.length(0);
    str_value.set_charset(collation.collation);
    return &str_value; 
  }
public:
  Item_str_func() :Item_func() { decimals=NOT_FIXED_DEC; }
  Item_str_func(Item *a) :Item_func(a) {decimals=NOT_FIXED_DEC; }
  Item_str_func(Item *a,Item *b) :Item_func(a,b) { decimals=NOT_FIXED_DEC; }
  Item_str_func(Item *a,Item *b,Item *c) :Item_func(a,b,c) { decimals=NOT_FIXED_DEC; }
  Item_str_func(Item *a,Item *b,Item *c,Item *d) :Item_func(a,b,c,d) {decimals=NOT_FIXED_DEC; }
  Item_str_func(Item *a,Item *b,Item *c,Item *d, Item* e) :Item_func(a,b,c,d,e) {decimals=NOT_FIXED_DEC; }
  Item_str_func(List<Item> &list) :Item_func(list) {decimals=NOT_FIXED_DEC; }
  longlong val_int();
  double val_real();
  my_decimal *val_decimal(my_decimal *);
  enum Item_result result_type () const { return STRING_RESULT; }
  void left_right_max_length();
  bool fix_fields(THD *thd, Item **ref);
};



/*
  Functions that return values with ASCII repertoire
*/
class Item_str_ascii_func :public Item_str_func
{
  String ascii_buf;
public:
  Item_str_ascii_func() :Item_str_func() {}
  Item_str_ascii_func(Item *a) :Item_str_func(a) {}
  Item_str_ascii_func(Item *a,Item *b) :Item_str_func(a,b) {}
  Item_str_ascii_func(Item *a,Item *b,Item *c) :Item_str_func(a,b,c) {}
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
  Item_func_md5(Item *a) :Item_str_ascii_func(a) {}
  String *val_str_ascii(String *);
  void fix_length_and_dec();
  const char *func_name() const { return "md5"; }
};


class Item_func_sha :public Item_str_ascii_func
{
public:
  Item_func_sha(Item *a) :Item_str_ascii_func(a) {}
  String *val_str_ascii(String *);    
  void fix_length_and_dec();      
  const char *func_name() const { return "sha"; }	
};

class Item_func_sha2 :public Item_str_ascii_func
{
public:
  Item_func_sha2(Item *a, Item *b) :Item_str_ascii_func(a, b) {}
  String *val_str_ascii(String *);
  void fix_length_and_dec();
  const char *func_name() const { return "sha2"; }
};

class Item_func_aes_encrypt :public Item_str_func
{
public:
  Item_func_aes_encrypt(Item *a, Item *b) :Item_str_func(a,b) {}
  String *val_str(String *);
  void fix_length_and_dec();
  const char *func_name() const { return "aes_encrypt"; }
};

class Item_func_aes_decrypt :public Item_str_func	
{
public:
  Item_func_aes_decrypt(Item *a, Item *b) :Item_str_func(a,b) {}
  String *val_str(String *);
  void fix_length_and_dec();
  const char *func_name() const { return "aes_decrypt"; }
};


class Item_func_concat :public Item_str_func
{
  String tmp_value;
public:
  Item_func_concat(List<Item> &list) :Item_str_func(list) {}
  Item_func_concat(Item *a,Item *b) :Item_str_func(a,b) {}
  String *val_str(String *);
  void fix_length_and_dec();
  const char *func_name() const { return "concat"; }
};

class Item_func_concat_ws :public Item_str_func
{
  String tmp_value;
public:
  Item_func_concat_ws(List<Item> &list) :Item_str_func(list) {}
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
  String *val_str(String *);
  void fix_length_and_dec();
  const char *func_name() const { return "reverse"; }
};


class Item_func_replace :public Item_str_func
{
  String tmp_value,tmp_value2;
public:
  Item_func_replace(Item *org,Item *find,Item *replace)
    :Item_str_func(org,find,replace) {}
  String *val_str(String *);
  void fix_length_and_dec();
  const char *func_name() const { return "replace"; }
};


class Item_func_insert :public Item_str_func
{
  String tmp_value;
public:
  Item_func_insert(Item *org,Item *start,Item *length,Item *new_str)
    :Item_str_func(org,start,length,new_str) {}
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
  Item_str_conv(Item *item) :Item_str_func(item) {}
  String *val_str(String *);
};


class Item_func_lcase :public Item_str_conv
{
public:
  Item_func_lcase(Item *item) :Item_str_conv(item) {}
  const char *func_name() const { return "lcase"; }
  void fix_length_and_dec();
};

class Item_func_ucase :public Item_str_conv
{
public:
  Item_func_ucase(Item *item) :Item_str_conv(item) {}
  const char *func_name() const { return "ucase"; }
  void fix_length_and_dec();
};


class Item_func_left :public Item_str_func
{
  String tmp_value;
public:
  Item_func_left(Item *a,Item *b) :Item_str_func(a,b) {}
  String *val_str(String *);
  void fix_length_and_dec();
  const char *func_name() const { return "left"; }
};


class Item_func_right :public Item_str_func
{
  String tmp_value;
public:
  Item_func_right(Item *a,Item *b) :Item_str_func(a,b) {}
  String *val_str(String *);
  void fix_length_and_dec();
  const char *func_name() const { return "right"; }
};


class Item_func_substr :public Item_str_func
{
  String tmp_value;
public:
  Item_func_substr(Item *a,Item *b) :Item_str_func(a,b) {}
  Item_func_substr(Item *a,Item *b,Item *c) :Item_str_func(a,b,c) {}
  String *val_str(String *);
  void fix_length_and_dec();
  const char *func_name() const { return "substr"; }
};


class Item_func_substr_index :public Item_str_func
{
  String tmp_value;
public:
  Item_func_substr_index(Item *a,Item *b,Item *c) :Item_str_func(a,b,c) {}
  String *val_str(String *);
  void fix_length_and_dec();
  const char *func_name() const { return "substring_index"; }
};


class Item_func_trim :public Item_str_func
{
protected:
  String tmp_value;
  String remove;
  String *trimmed_value(String *res, uint32 offset, uint32 length)
  {
    tmp_value.set(*res, offset, length);
    /*
      Make sure to return correct charset and collation:
      TRIM(0x000000 FROM _ucs2 0x0061)
      should set charset to "binary" rather than to "ucs2".
    */
    tmp_value.set_charset(collation.collation);
    return &tmp_value;
  }
  String *non_trimmed_value(String *res)
  {
    return trimmed_value(res, 0, res->length());
  }
public:
  Item_func_trim(Item *a,Item *b) :Item_str_func(a,b) {}
  Item_func_trim(Item *a) :Item_str_func(a) {}
  String *val_str(String *);
  void fix_length_and_dec();
  const char *func_name() const { return "trim"; }
  virtual void print(String *str, enum_query_type query_type);
  virtual const char *mode_name() const { return "both"; }
};


class Item_func_ltrim :public Item_func_trim
{
public:
  Item_func_ltrim(Item *a,Item *b) :Item_func_trim(a,b) {}
  Item_func_ltrim(Item *a) :Item_func_trim(a) {}
  String *val_str(String *);
  const char *func_name() const { return "ltrim"; }
  const char *mode_name() const { return "leading"; }
};


class Item_func_rtrim :public Item_func_trim
{
public:
  Item_func_rtrim(Item *a,Item *b) :Item_func_trim(a,b) {}
  Item_func_rtrim(Item *a) :Item_func_trim(a) {}
  String *val_str(String *);
  const char *func_name() const { return "rtrim"; }
  const char *mode_name() const { return "trailing"; }
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
  char tmp_value[SCRAMBLED_PASSWORD_CHAR_LENGTH+1]; 
public:
  Item_func_password(Item *a) :Item_str_ascii_func(a) {}
  String *val_str_ascii(String *str);
  void fix_length_and_dec()
  {
    fix_length_and_charset(SCRAMBLED_PASSWORD_CHAR_LENGTH, default_charset());
  }
  const char *func_name() const { return "password"; }
  static char *alloc(THD *thd, const char *password, size_t pass_len);
};


/*
  Item_func_old_password -- PASSWORD() implementation used in MySQL 3.21 - 4.0
  compatibility mode. This item is created in sql_yacc.yy when
  'old_passwords' session variable is set, and to handle OLD_PASSWORD()
  function.
*/

class Item_func_old_password :public Item_str_ascii_func
{
  char tmp_value[SCRAMBLED_PASSWORD_CHAR_LENGTH_323+1];
public:
  Item_func_old_password(Item *a) :Item_str_ascii_func(a) {}
  String *val_str_ascii(String *str);
  void fix_length_and_dec()
  {
    fix_length_and_charset(SCRAMBLED_PASSWORD_CHAR_LENGTH_323, default_charset());
  } 
  const char *func_name() const { return "old_password"; }
  static char *alloc(THD *thd, const char *password, size_t pass_len);
};


class Item_func_des_encrypt :public Item_str_func
{
  String tmp_value,tmp_arg;
public:
  Item_func_des_encrypt(Item *a) :Item_str_func(a) {}
  Item_func_des_encrypt(Item *a, Item *b): Item_str_func(a,b) {}
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
  Item_func_des_decrypt(Item *a) :Item_str_func(a) {}
  Item_func_des_decrypt(Item *a, Item *b): Item_str_func(a,b) {}
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
  String tmp_value;

  /* Encapsulate common constructor actions */
  void constructor_helper()
  {
    collation.set(&my_charset_bin);
  }
public:
  Item_func_encrypt(Item *a) :Item_str_func(a)
  {
    constructor_helper();
  }
  Item_func_encrypt(Item *a, Item *b): Item_str_func(a,b)
  {
    constructor_helper();
  }
  String *val_str(String *);
  void fix_length_and_dec() { maybe_null=1; max_length = 13; }
  const char *func_name() const { return "encrypt"; }
  bool check_vcol_func_processor(uchar *int_arg) 
  {
    return trace_unsupported_by_check_vcol_func_processor(func_name());
  }
};

#include "sql_crypt.h"


class Item_func_encode :public Item_str_func
{
private:
  /** Whether the PRNG has already been seeded. */
  bool seeded;
protected:
  SQL_CRYPT sql_crypt;
public:
  Item_func_encode(Item *a, Item *seed):
    Item_str_func(a, seed) {}
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
  Item_func_decode(Item *a, Item *seed): Item_func_encode(a, seed) {}
  const char *func_name() const { return "decode"; }
protected:
  void crypto_transform(String *);
};


class Item_func_sysconst :public Item_str_func
{
public:
  Item_func_sysconst()
  { collation.set(system_charset_info,DERIVATION_SYSCONST); }
  Item *safe_charset_converter(CHARSET_INFO *tocs);
  /*
    Used to create correct Item name in new converted item in
    safe_charset_converter, return string representation of this function
    call
  */
  virtual const char *fully_qualified_func_name() const = 0;
  bool check_vcol_func_processor(uchar *int_arg) 
  {
    return trace_unsupported_by_check_vcol_func_processor(
                                           fully_qualified_func_name());
  }
};


class Item_func_database :public Item_func_sysconst
{
public:
  Item_func_database() :Item_func_sysconst() {}
  String *val_str(String *);
  void fix_length_and_dec()
  {
    max_length= MAX_FIELD_NAME * system_charset_info->mbmaxlen;
    maybe_null=1;
  }
  const char *func_name() const { return "database"; }
  const char *fully_qualified_func_name() const { return "database()"; }
};


class Item_func_user :public Item_func_sysconst
{
protected:
  bool init (const char *user, const char *host);

public:
  Item_func_user()
  {
    str_value.set("", 0, system_charset_info);
  }
  String *val_str(String *)
  {
    DBUG_ASSERT(fixed == 1);
    return (null_value ? 0 : &str_value);
  }
  bool fix_fields(THD *thd, Item **ref);
  void fix_length_and_dec()
  {
    max_length= (username_char_length +
                 HOSTNAME_LENGTH + 1) * SYSTEM_CHARSET_MBMAXLEN;
  }
  const char *func_name() const { return "user"; }
  const char *fully_qualified_func_name() const { return "user()"; }
  int save_in_field(Field *field, bool no_conversions)
  {
    return save_str_value_in_field(field, &str_value);
  }
};


class Item_func_current_user :public Item_func_user
{
  Name_resolution_context *context;

public:
  Item_func_current_user(Name_resolution_context *context_arg)
    : context(context_arg) {}
  bool fix_fields(THD *thd, Item **ref);
  const char *func_name() const { return "current_user"; }
  const char *fully_qualified_func_name() const { return "current_user()"; }
};


class Item_func_soundex :public Item_str_func
{
  String tmp_value;
public:
  Item_func_soundex(Item *a) :Item_str_func(a) {}
  String *val_str(String *);
  void fix_length_and_dec();
  const char *func_name() const { return "soundex"; }
};


class Item_func_elt :public Item_str_func
{
public:
  Item_func_elt(List<Item> &list) :Item_str_func(list) {}
  double val_real();
  longlong val_int();
  String *val_str(String *str);
  void fix_length_and_dec();
  const char *func_name() const { return "elt"; }
};


class Item_func_make_set :public Item_str_func
{
  String tmp_str;

public:
  Item_func_make_set(List<Item> &list) :Item_str_func(list) {}
  String *val_str(String *str);
  void fix_length_and_dec();
  const char *func_name() const { return "make_set"; }
};


class Item_func_format :public Item_str_ascii_func
{
  String tmp_str;
  MY_LOCALE *locale;
public:
  Item_func_format(Item *org, Item *dec): Item_str_ascii_func(org, dec) {}
  Item_func_format(Item *org, Item *dec, Item *lang):
  Item_str_ascii_func(org, dec, lang) {}
  
  MY_LOCALE *get_locale(Item *item);
  String *val_str_ascii(String *);
  void fix_length_and_dec();
  const char *func_name() const { return "format"; }
  virtual void print(String *str, enum_query_type query_type);
};


class Item_func_char :public Item_str_func
{
public:
  Item_func_char(List<Item> &list) :Item_str_func(list)
  { collation.set(&my_charset_bin); }
  Item_func_char(List<Item> &list, CHARSET_INFO *cs) :Item_str_func(list)
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
  Item_func_repeat(Item *arg1,Item *arg2) :Item_str_func(arg1,arg2) {}
  String *val_str(String *);
  void fix_length_and_dec();
  const char *func_name() const { return "repeat"; }
};


class Item_func_rpad :public Item_str_func
{
  String tmp_value, rpad_str;
public:
  Item_func_rpad(Item *arg1,Item *arg2,Item *arg3)
    :Item_str_func(arg1,arg2,arg3) {}
  String *val_str(String *);
  void fix_length_and_dec();
  const char *func_name() const { return "rpad"; }
};


class Item_func_lpad :public Item_str_func
{
  String tmp_value, lpad_str;
public:
  Item_func_lpad(Item *arg1,Item *arg2,Item *arg3)
    :Item_str_func(arg1,arg2,arg3) {}
  String *val_str(String *);
  void fix_length_and_dec();
  const char *func_name() const { return "lpad"; }
};


class Item_func_conv :public Item_str_func
{
public:
  Item_func_conv(Item *a,Item *b,Item *c) :Item_str_func(a,b,c) {}
  const char *func_name() const { return "conv"; }
  String *val_str(String *);
  void fix_length_and_dec()
  {
    collation.set(default_charset());
    max_length=64;
    maybe_null= 1;
  }
};


class Item_func_hex :public Item_str_ascii_func
{
  String tmp_value;
public:
  Item_func_hex(Item *a) :Item_str_ascii_func(a) {}
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
  Item_func_unhex(Item *a) :Item_str_func(a) 
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
  Item_func_like_range(Item *a, Item *b, bool is_min_arg)
    :Item_str_func(a, b), is_min(is_min_arg)
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
  Item_func_like_range_min(Item *a, Item *b) 
    :Item_func_like_range(a, b, true) { }
  const char *func_name() const { return "like_range_min"; }
};


class Item_func_like_range_max :public Item_func_like_range
{
public:
  Item_func_like_range_max(Item *a, Item *b)
    :Item_func_like_range(a, b, false) { }
  const char *func_name() const { return "like_range_max"; }
};
#endif


class Item_func_binary :public Item_str_func
{
public:
  Item_func_binary(Item *a) :Item_str_func(a) {}
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
};


class Item_load_file :public Item_str_func
{
  String tmp_value;
public:
  Item_load_file(Item *a) :Item_str_func(a) {}
  String *val_str(String *);
  const char *func_name() const { return "load_file"; }
  void fix_length_and_dec()
  {
    collation.set(&my_charset_bin, DERIVATION_COERCIBLE);
    maybe_null=1;
    max_length=MAX_BLOB_WIDTH;
  }
  bool check_vcol_func_processor(uchar *int_arg) 
  {
    return trace_unsupported_by_check_vcol_func_processor(func_name());
  }
};


class Item_func_export_set: public Item_str_func
{
 public:
  Item_func_export_set(Item *a,Item *b,Item* c) :Item_str_func(a,b,c) {}
  Item_func_export_set(Item *a,Item *b,Item* c,Item* d) :Item_str_func(a,b,c,d) {}
  Item_func_export_set(Item *a,Item *b,Item* c,Item* d,Item* e) :Item_str_func(a,b,c,d,e) {}
  String  *val_str(String *str);
  void fix_length_and_dec();
  const char *func_name() const { return "export_set"; }
};

class Item_func_inet_ntoa : public Item_str_func
{
public:
  Item_func_inet_ntoa(Item *a) :Item_str_func(a)
    {
    }
  String* val_str(String* str);
  const char *func_name() const { return "inet_ntoa"; }
  void fix_length_and_dec() 
  { 
    decimals= 0; 
    fix_length_and_charset(3 * 8 + 7, default_charset()); 
    maybe_null= 1;
  }
};

class Item_func_quote :public Item_str_func
{
  String tmp_value;
public:
  Item_func_quote(Item *a) :Item_str_func(a) {}
  const char *func_name() const { return "quote"; }
  String *val_str(String *);
  void fix_length_and_dec()
  {
    collation.set(args[0]->collation);
    ulonglong max_result_length= (ulonglong) args[0]->max_length * 2 +
                                  2 * collation.collation->mbmaxlen;
    max_length= (uint32) min(max_result_length, MAX_BLOB_WIDTH);
  }
};

class Item_func_conv_charset :public Item_str_func
{
  bool use_cached_value;
  String tmp_value;
public:
  bool safe;
  CHARSET_INFO *conv_charset; // keep it public
  Item_func_conv_charset(Item *a, CHARSET_INFO *cs) :Item_str_func(a) 
  { conv_charset= cs; use_cached_value= 0; safe= 0; }
  Item_func_conv_charset(Item *a, CHARSET_INFO *cs, bool cache_if_const) 
    :Item_str_func(a) 
  {
    conv_charset= cs;
    if (cache_if_const && args[0]->const_item() && !args[0]->is_expensive())
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
  longlong val_int()
  {
    if (args[0]->result_type() == STRING_RESULT)
      return Item_str_func::val_int();
    longlong res= args[0]->val_int();
    if ((null_value= args[0]->null_value))
      return 0;
    return res;
  }
  double val_real()
  {
    if (args[0]->result_type() == STRING_RESULT)
      return Item_str_func::val_real();
    double res= args[0]->val_real();
    if ((null_value= args[0]->null_value))
      return 0;
    return res;
  }
  my_decimal *val_decimal(my_decimal *d)
  {
    if (args[0]->result_type() == STRING_RESULT)
      return Item_str_func::val_decimal(d);
    my_decimal *res= args[0]->val_decimal(d);
    if ((null_value= args[0]->null_value))
      return NULL;
    return res;
  }
  bool get_date(MYSQL_TIME *ltime, ulonglong fuzzydate)
  {
    if (args[0]->result_type() == STRING_RESULT)
      return Item_str_func::get_date(ltime, fuzzydate);
    bool res= args[0]->get_date(ltime, fuzzydate);
    if ((null_value= args[0]->null_value))
      return 1;
    return res;
  }
  void fix_length_and_dec();
  const char *func_name() const { return "convert"; }
  virtual void print(String *str, enum_query_type query_type);
};

class Item_func_set_collation :public Item_str_func
{
public:
  Item_func_set_collation(Item *a, Item *b) :Item_str_func(a,b) {};
  String *val_str(String *);
  void fix_length_and_dec();
  bool eq(const Item *item, bool binary_cmp) const;
  const char *func_name() const { return "collate"; }
  enum Functype functype() const { return COLLATE_FUNC; }
  virtual void print(String *str, enum_query_type query_type);
  Item_field *filed_for_view_update()
  {
    /* this function is transparent for view updating */
    return args[0]->filed_for_view_update();
  }
};

class Item_func_charset :public Item_str_func
{
public:
  Item_func_charset(Item *a) :Item_str_func(a) {}
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
  Item_func_collation(Item *a) :Item_str_func(a) {}
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

class Item_func_crc32 :public Item_int_func
{
  String value;
public:
  Item_func_crc32(Item *a) :Item_int_func(a) { unsigned_flag= 1; }
  const char *func_name() const { return "crc32"; }
  void fix_length_and_dec() { max_length=10; }
  longlong val_int();
};

class Item_func_uncompressed_length : public Item_int_func
{
  String value;
public:
  Item_func_uncompressed_length(Item *a):Item_int_func(a){}
  const char *func_name() const{return "uncompressed_length";}
  void fix_length_and_dec() { max_length=10; maybe_null= true; }
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
  Item_func_compress(Item *a):Item_str_func(a){}
  void fix_length_and_dec(){max_length= (args[0]->max_length*120)/100+12;}
  const char *func_name() const{return "compress";}
  String *val_str(String *) ZLIB_DEPENDED_FUNCTION
};

class Item_func_uncompress: public Item_str_func
{
  String buffer;
public:
  Item_func_uncompress(Item *a): Item_str_func(a){}
  void fix_length_and_dec(){ maybe_null= 1; max_length= MAX_BLOB_WIDTH; }
  const char *func_name() const{return "uncompress";}
  String *val_str(String *) ZLIB_DEPENDED_FUNCTION
};


class Item_func_uuid: public Item_str_func
{
public:
  Item_func_uuid(): Item_str_func() {}
  void fix_length_and_dec()
  {
    collation.set(system_charset_info,
                  DERIVATION_COERCIBLE, MY_REPERTOIRE_ASCII);
    fix_char_length(MY_UUID_STRING_LENGTH);
  }
  const char *func_name() const{ return "uuid"; }
  String *val_str(String *);
  bool check_vcol_func_processor(uchar *int_arg) 
  {
    return trace_unsupported_by_check_vcol_func_processor(func_name());
  }
};


class Item_func_dyncol_create: public Item_str_func
{
protected:
  DYNCALL_CREATE_DEF *defs;
  DYNAMIC_COLUMN_VALUE *vals;
  uint *nums;
  void prepare_arguments();
  void cleanup_arguments();
  void print_arguments(String *str, enum_query_type query_type);
public:
  Item_func_dyncol_create(List<Item> &args, DYNCALL_CREATE_DEF *dfs);
  bool fix_fields(THD *thd, Item **ref);
  void fix_length_and_dec();
  const char *func_name() const{ return "column_create"; }
  String *val_str(String *);
  virtual void print(String *str, enum_query_type query_type);
};


class Item_func_dyncol_add: public Item_func_dyncol_create
{
public:
  Item_func_dyncol_add(List<Item> &args, DYNCALL_CREATE_DEF *dfs)
    :Item_func_dyncol_create(args, dfs)
  {}
  const char *func_name() const{ return "column_add"; }
  String *val_str(String *);
  virtual void print(String *str, enum_query_type query_type);
};


/*
  The following functions is always called from an Item_cast function
*/

class Item_dyncol_get: public Item_str_func
{
public:
  Item_dyncol_get(Item *str, Item *num)
    :Item_str_func(str, num)
  {
    max_length= MAX_DYNAMIC_COLUMN_LENGTH;
  }
  void fix_length_and_dec()
  { maybe_null= 1; }
  /* Mark that collation can change between calls */
  bool dynamic_result() { return 1; }

  const char *func_name() const { return "column_get"; }
  String *val_str(String *);
  longlong val_int();
  double val_real();
  my_decimal *val_decimal(my_decimal *);
  bool get_dyn_value(DYNAMIC_COLUMN_VALUE *val, String *tmp);
  bool get_date(MYSQL_TIME *ltime, ulonglong fuzzydate);
  void print(String *str, enum_query_type query_type);
};


class Item_func_dyncol_list: public Item_str_func
{
public:
  Item_func_dyncol_list(Item *str) :Item_str_func(str) {};
  void fix_length_and_dec() { maybe_null= 1; max_length= MAX_BLOB_WIDTH; };
  const char *func_name() const{ return "column_list"; }
  String *val_str(String *);
};

#endif /* ITEM_STRFUNC_INCLUDED */
