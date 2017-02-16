/*
   Copyright (c) 2000, 2013, Oracle and/or its affiliates.
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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

/**
  @file

  @brief
  This file defines all string functions

  @warning
    Some string functions don't always put and end-null on a String.
    (This shouldn't be needed)
*/

#ifdef USE_PRAGMA_IMPLEMENTATION
#pragma implementation				// gcc: Class implementation
#endif

/* May include caustic 3rd-party defs. Use early, so it can override nothing. */
#include "sha2.h"
#include "my_global.h"                          // HAVE_*


#include "sql_priv.h"
/*
  It is necessary to include set_var.h instead of item.h because there
  are dependencies on include order for set_var.h and item.h. This
  will be resolved later.
*/
#include "sql_class.h"                          // set_var.h: THD
#include "set_var.h"
#include "sql_base.h"
#include "sql_time.h"
#include "sql_acl.h"                            // SUPER_ACL
#include "des_key_file.h"       // st_des_keyschedule, st_des_keyblock
#include "password.h"           // my_make_scrambled_password,
                                // my_make_scrambled_password_323
#include <m_ctype.h>
#include "my_md5.h"
#include "sha1.h"
#include "my_aes.h"
#include <zlib.h>
C_MODE_START
#include "../mysys/my_static.h"			// For soundex_map
C_MODE_END

size_t username_char_length= 16;

/*
  For the Items which have only val_str_ascii() method
  and don't have their own "native" val_str(),
  we provide a "wrapper" method to convert from ASCII
  to Item character set when it's necessary.
  Conversion happens only in case of "tricky" Item character set (e.g. UCS2).
  Normally conversion does not happen, and val_str_ascii() is immediately
  returned instead.
*/
String *Item_func::val_str_from_val_str_ascii(String *str, String *str2)
{
  DBUG_ASSERT(fixed == 1);

  if (!(collation.collation->state & MY_CS_NONASCII))
  {
    String *res= val_str_ascii(str);
    if (res)
      res->set_charset(collation.collation);
    return res;
  }
  
  DBUG_ASSERT(str != str2);
  
  uint errors;
  String *res= val_str_ascii(str);
  if (!res)
    return 0;
  
  if ((null_value= str2->copy(res->ptr(), res->length(),
                              &my_charset_latin1, collation.collation,
                              &errors)))
    return 0;
  
  return str2;
}


/*
  Convert an array of bytes to a hexadecimal representation.

  Used to generate a hexadecimal representation of a message digest.
*/
static void array_to_hex(char *to, const unsigned char *str, uint len)
{
  const unsigned char *str_end= str + len;
  for (; str != str_end; ++str)
  {
    *to++= _dig_vec_lower[((uchar) *str) >> 4];
    *to++= _dig_vec_lower[((uchar) *str) & 0x0F];
  }
}


bool Item_str_func::fix_fields(THD *thd, Item **ref)
{
  bool res= Item_func::fix_fields(thd, ref);
  /*
    In Item_str_func::check_well_formed_result() we may set null_value
    flag on the same condition as in test() below.
  */
  maybe_null= (maybe_null ||
               test(thd->variables.sql_mode &
                    (MODE_STRICT_TRANS_TABLES | MODE_STRICT_ALL_TABLES)));
  return res;
}


my_decimal *Item_str_func::val_decimal(my_decimal *decimal_value)
{
  DBUG_ASSERT(fixed == 1);
  char buff[64];
  String *res, tmp(buff,sizeof(buff), &my_charset_bin);
  res= val_str(&tmp);
  if (!res)
    return 0;
  (void)str2my_decimal(E_DEC_FATAL_ERROR, (char*) res->ptr(),
                       res->length(), res->charset(), decimal_value);
  return decimal_value;
}


double Item_str_func::val_real()
{
  DBUG_ASSERT(fixed == 1);
  int err_not_used;
  char *end_not_used, buff[64];
  String *res, tmp(buff,sizeof(buff), &my_charset_bin);
  res= val_str(&tmp);
  return res ? my_strntod(res->charset(), (char*) res->ptr(), res->length(),
			  &end_not_used, &err_not_used) : 0.0;
}


longlong Item_str_func::val_int()
{
  DBUG_ASSERT(fixed == 1);
  int err;
  char buff[22];
  String *res, tmp(buff,sizeof(buff), &my_charset_bin);
  res= val_str(&tmp);
  return (res ?
	  my_strntoll(res->charset(), res->ptr(), res->length(), 10, NULL,
		      &err) :
	  (longlong) 0);
}


String *Item_func_md5::val_str_ascii(String *str)
{
  DBUG_ASSERT(fixed == 1);
  String * sptr= args[0]->val_str(str);
  if (sptr)
  {
    uchar digest[16];

    null_value=0;
    MY_MD5_HASH(digest,(uchar *) sptr->ptr(), sptr->length());
    if (str->alloc(32))				// Ensure that memory is free
    {
      null_value=1;
      return 0;
    }
    array_to_hex((char *) str->ptr(), digest, 16);
    str->set_charset(&my_charset_numeric);
    str->length((uint) 32);
    return str;
  }
  null_value=1;
  return 0;
}


/*
  The MD5()/SHA() functions treat their parameter as being a case sensitive.
  Thus we set binary collation on it so different instances of MD5() will be
  compared properly.
*/
static CHARSET_INFO *get_checksum_charset(const char *csname)
{
  CHARSET_INFO *cs= get_charset_by_csname(csname, MY_CS_BINSORT, MYF(0));
  if (!cs)
  {
    // Charset has no binary collation: use my_charset_bin.
    cs= &my_charset_bin;
  }
  return cs;
}


void Item_func_md5::fix_length_and_dec()
{
  CHARSET_INFO *cs= get_checksum_charset(args[0]->collation.collation->csname);
  args[0]->collation.set(cs, DERIVATION_COERCIBLE);
  fix_length_and_charset(32, default_charset());
}


String *Item_func_sha::val_str_ascii(String *str)
{
  DBUG_ASSERT(fixed == 1);
  String * sptr= args[0]->val_str(str);
  if (sptr)  /* If we got value different from NULL */
  {
    SHA1_CONTEXT context;  /* Context used to generate SHA1 hash */
    /* Temporary buffer to store 160bit digest */
    uint8 digest[SHA1_HASH_SIZE];
    mysql_sha1_reset(&context);  /* We do not have to check for error here */
    /* No need to check error as the only case would be too long message */
    mysql_sha1_input(&context,
                     (const uchar *) sptr->ptr(), sptr->length());

    /* Ensure that memory is free and we got result */
    if (!( str->alloc(SHA1_HASH_SIZE*2) ||
           (mysql_sha1_result(&context,digest))))
    {
      array_to_hex((char *) str->ptr(), digest, SHA1_HASH_SIZE);
      str->set_charset(&my_charset_numeric);
      str->length((uint)  SHA1_HASH_SIZE*2);
      null_value=0;
      return str;
    }
  }
  null_value=1;
  return 0;
}

void Item_func_sha::fix_length_and_dec()
{
  CHARSET_INFO *cs= get_checksum_charset(args[0]->collation.collation->csname);
  args[0]->collation.set(cs, DERIVATION_COERCIBLE);
  // size of hex representation of hash
  fix_length_and_charset(SHA1_HASH_SIZE * 2, default_charset());
}

String *Item_func_sha2::val_str_ascii(String *str)
{
  DBUG_ASSERT(fixed == 1);
#if defined(HAVE_OPENSSL) && !defined(EMBEDDED_LIBRARY)
  unsigned char digest_buf[SHA512_DIGEST_LENGTH];
  String *input_string;
  unsigned char *input_ptr;
  size_t input_len;
  uint digest_length= 0;

  str->set_charset(&my_charset_bin);

  input_string= args[0]->val_str(str);
  if (input_string == NULL)
  {
    null_value= TRUE;
    return (String *) NULL;
  }

  null_value= args[0]->null_value;
  if (null_value)
    return (String *) NULL;

  input_ptr= (unsigned char *) input_string->ptr();
  input_len= input_string->length();

  switch ((uint) args[1]->val_int()) {
#ifndef OPENSSL_NO_SHA512
  case 512:
    digest_length= SHA512_DIGEST_LENGTH;
    (void) SHA512(input_ptr, input_len, digest_buf);
    break;
  case 384:
    digest_length= SHA384_DIGEST_LENGTH;
    (void) SHA384(input_ptr, input_len, digest_buf);
    break;
#endif
#ifndef OPENSSL_NO_SHA256
  case 224:
    digest_length= SHA224_DIGEST_LENGTH;
    (void) SHA224(input_ptr, input_len, digest_buf);
    break;
  case 256:
  case 0: // SHA-256 is the default
    digest_length= SHA256_DIGEST_LENGTH;
    (void) SHA256(input_ptr, input_len, digest_buf);
    break;
#endif
  default:
    if (!args[1]->const_item())
      push_warning_printf(current_thd,
        MYSQL_ERROR::WARN_LEVEL_WARN,
        ER_WRONG_PARAMETERS_TO_NATIVE_FCT,
        ER(ER_WRONG_PARAMETERS_TO_NATIVE_FCT), "sha2");
    null_value= TRUE;
    return NULL;
  }

  /* 
    Since we're subverting the usual String methods, we must make sure that
    the destination has space for the bytes we're about to write.
  */
  str->realloc((uint) digest_length*2 + 1); /* Each byte as two nybbles */

  /* Convert the large number to a string-hex representation. */
  array_to_hex((char *) str->ptr(), digest_buf, digest_length);

  /* We poked raw bytes in.  We must inform the the String of its length. */
  str->length((uint) digest_length*2); /* Each byte as two nybbles */

  null_value= FALSE;
  return str;

#else
  push_warning_printf(current_thd,
    MYSQL_ERROR::WARN_LEVEL_WARN,
    ER_FEATURE_DISABLED,
    ER(ER_FEATURE_DISABLED),
    "sha2", "--with-ssl");
  null_value= TRUE;
  return (String *) NULL;
#endif /* defined(HAVE_OPENSSL) && !defined(EMBEDDED_LIBRARY) */
}


void Item_func_sha2::fix_length_and_dec()
{
  maybe_null= 1;
  max_length = 0;

#if defined(HAVE_OPENSSL) && !defined(EMBEDDED_LIBRARY)
  int sha_variant= args[1]->const_item() ? args[1]->val_int() : 512;

  switch (sha_variant) {
#ifndef OPENSSL_NO_SHA512
  case 512:
    fix_length_and_charset(SHA512_DIGEST_LENGTH * 2, default_charset());
    break;
  case 384:
    fix_length_and_charset(SHA384_DIGEST_LENGTH * 2, default_charset());
    break;
#endif
#ifndef OPENSSL_NO_SHA256
  case 256:
  case 0: // SHA-256 is the default
    fix_length_and_charset(SHA256_DIGEST_LENGTH * 2, default_charset());
    break;
  case 224:
    fix_length_and_charset(SHA224_DIGEST_LENGTH * 2, default_charset());
    break;
#endif
  default:
    push_warning_printf(current_thd,
      MYSQL_ERROR::WARN_LEVEL_WARN,
      ER_WRONG_PARAMETERS_TO_NATIVE_FCT,
      ER(ER_WRONG_PARAMETERS_TO_NATIVE_FCT), "sha2");
  }

  CHARSET_INFO *cs= get_checksum_charset(args[0]->collation.collation->csname);
  args[0]->collation.set(cs, DERIVATION_COERCIBLE);

#else
  push_warning_printf(current_thd,
    MYSQL_ERROR::WARN_LEVEL_WARN,
    ER_FEATURE_DISABLED,
    ER(ER_FEATURE_DISABLED),
    "sha2", "--with-ssl");
#endif /* defined(HAVE_OPENSSL) && !defined(EMBEDDED_LIBRARY) */
}

/* Implementation of AES encryption routines */

String *Item_func_aes_encrypt::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  char key_buff[80];
  String tmp_key_value(key_buff, sizeof(key_buff), system_charset_info);
  String *sptr= args[0]->val_str(str);			// String to encrypt
  String *key=  args[1]->val_str(&tmp_key_value);	// key
  int aes_length;
  if (sptr && key) // we need both arguments to be not NULL
  {
    null_value=0;
    aes_length=my_aes_get_size(sptr->length()); // Calculate result length

    if (!str_value.alloc(aes_length))		// Ensure that memory is free
    {
      // finally encrypt directly to allocated buffer.
      if (my_aes_encrypt(sptr->ptr(),sptr->length(), (char*) str_value.ptr(),
			 key->ptr(), key->length()) == aes_length)
      {
	// We got the expected result length
	str_value.length((uint) aes_length);
	return &str_value;
      }
    }
  }
  null_value=1;
  return 0;
}


void Item_func_aes_encrypt::fix_length_and_dec()
{
  max_length=my_aes_get_size(args[0]->max_length);
}


String *Item_func_aes_decrypt::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  char key_buff[80];
  String tmp_key_value(key_buff, sizeof(key_buff), system_charset_info);
  String *sptr, *key;
  DBUG_ENTER("Item_func_aes_decrypt::val_str");

  sptr= args[0]->val_str(str);			// String to decrypt
  key=  args[1]->val_str(&tmp_key_value);	// Key
  if (sptr && key)  			// Need to have both arguments not NULL
  {
    null_value=0;
    if (!str_value.alloc(sptr->length()))  // Ensure that memory is free
    {
      // finally decrypt directly to allocated buffer.
      int length;
      length=my_aes_decrypt(sptr->ptr(), sptr->length(),
			    (char*) str_value.ptr(),
                            key->ptr(), key->length());
      if (length >= 0)  // if we got correct data data
      {
        str_value.length((uint) length);
        DBUG_RETURN(&str_value);
      }
    }
  }
  // Bad parameters. No memory or bad data will all go here
  null_value=1;
  DBUG_RETURN(0);
}


void Item_func_aes_decrypt::fix_length_and_dec()
{
   max_length=args[0]->max_length;
   maybe_null= 1;
}


/**
  Concatenate args with the following premises:
  If only one arg (which is ok), return value of arg;
  Don't reallocate val_str() if not absolute necessary.
*/

String *Item_func_concat::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  String *res,*res2,*use_as_buff;
  uint i;
  bool is_const= 0;

  null_value=0;
  if (!(res=args[0]->val_str(str)))
    goto null;
  use_as_buff= &tmp_value;
  /* Item_subselect in --ps-protocol mode will state it as a non-const */
  is_const= args[0]->const_item() || !args[0]->used_tables();
  for (i=1 ; i < arg_count ; i++)
  {
    if (res->length() == 0)
    {
      if (!(res=args[i]->val_str(str)))
	goto null;
      /*
       CONCAT accumulates its result in the result of its the first
       non-empty argument. Because of this we need is_const to be 
       evaluated only for it.
      */
      is_const= args[i]->const_item() || !args[i]->used_tables();
    }
    else
    {
      if (!(res2=args[i]->val_str(use_as_buff)))
	goto null;
      if (res2->length() == 0)
	continue;
      if (res->length()+res2->length() >
	  current_thd->variables.max_allowed_packet)
      {
	push_warning_printf(current_thd, MYSQL_ERROR::WARN_LEVEL_WARN,
			    ER_WARN_ALLOWED_PACKET_OVERFLOWED,
			    ER(ER_WARN_ALLOWED_PACKET_OVERFLOWED), func_name(),
			    current_thd->variables.max_allowed_packet);
	goto null;
      }
      if (!is_const && res->alloced_length() >= res->length()+res2->length())
      {						// Use old buffer
	res->append(*res2);
      }
      else if (str->alloced_length() >= res->length()+res2->length())
      {
	if (str->ptr() == res2->ptr())
	  str->replace(0,0,*res);
	else
	{
	  str->copy(*res);
	  str->append(*res2);
	}
        res= str;
        use_as_buff= &tmp_value;
      }
      else if (res == &tmp_value)
      {
	if (res->append(*res2))			// Must be a blob
	  goto null;
      }
      else if (res2 == &tmp_value)
      {						// This can happend only 1 time
	if (tmp_value.replace(0,0,*res))
	  goto null;
	res= &tmp_value;
	use_as_buff=str;			// Put next arg here
      }
      else if (tmp_value.is_alloced() && res2->ptr() >= tmp_value.ptr() &&
	       res2->ptr() <= tmp_value.ptr() + tmp_value.alloced_length())
      {
	/*
	  This happens really seldom:
	  In this case res2 is sub string of tmp_value.  We will
	  now work in place in tmp_value to set it to res | res2
	*/
	/* Chop the last characters in tmp_value that isn't in res2 */
	tmp_value.length((uint32) (res2->ptr() - tmp_value.ptr()) +
			 res2->length());
	/* Place res2 at start of tmp_value, remove chars before res2 */
	if (tmp_value.replace(0,(uint32) (res2->ptr() - tmp_value.ptr()),
			      *res))
	  goto null;
	res= &tmp_value;
	use_as_buff=str;			// Put next arg here
      }
      else
      {						// Two big const strings
        /*
          NOTE: We should be prudent in the initial allocation unit -- the
          size of the arguments is a function of data distribution, which
          can be any. Instead of overcommitting at the first row, we grow
          the allocated amount by the factor of 2. This ensures that no
          more than 25% of memory will be overcommitted on average.
        */

        uint concat_len= res->length() + res2->length();

        if (tmp_value.alloced_length() < concat_len)
        {
          if (tmp_value.alloced_length() == 0)
          {
            if (tmp_value.alloc(concat_len))
              goto null;
          }
          else
          {
            uint new_len = max(tmp_value.alloced_length() * 2, concat_len);

            if (tmp_value.realloc(new_len))
              goto null;
          }
        }

	if (tmp_value.copy(*res) || tmp_value.append(*res2))
	  goto null;

	res= &tmp_value;
	use_as_buff=str;
      }
      is_const= 0;
    }
  }
  res->set_charset(collation.collation);
  return res;

null:
  null_value=1;
  return 0;
}


void Item_func_concat::fix_length_and_dec()
{
  ulonglong char_length= 0;

  if (agg_arg_charsets_for_string_result(collation, args, arg_count))
    return;

  for (uint i=0 ; i < arg_count ; i++)
    char_length+= args[i]->max_char_length();

  fix_char_length_ulonglong(char_length);
}

/**
  @details
  Function des_encrypt() by tonu@spam.ee & monty
  Works only if compiled with OpenSSL library support.
  @return
    A binary string where first character is CHAR(128 | key-number).
    If one uses a string key key_number is 127.
    Encryption result is longer than original by formula:
  @code new_length= org_length + (8-(org_length % 8))+1 @endcode
*/

String *Item_func_des_encrypt::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
#if defined(HAVE_OPENSSL) && !defined(EMBEDDED_LIBRARY)
  uint code= ER_WRONG_PARAMETERS_TO_PROCEDURE;
  DES_cblock ivec;
  struct st_des_keyblock keyblock;
  struct st_des_keyschedule keyschedule;
  const char *append_str="********";
  uint key_number, res_length, tail;
  String *res= args[0]->val_str(str);

  if ((null_value= args[0]->null_value))
    return 0;                                   // ENCRYPT(NULL) == NULL
  if ((res_length=res->length()) == 0)
    return make_empty_result();
  if (arg_count == 1)
  {
    /* Protect against someone doing FLUSH DES_KEY_FILE */
    mysql_mutex_lock(&LOCK_des_key_file);
    keyschedule= des_keyschedule[key_number=des_default_key];
    mysql_mutex_unlock(&LOCK_des_key_file);
  }
  else if (args[1]->result_type() == INT_RESULT)
  {
    key_number= (uint) args[1]->val_int();
    if (key_number > 9)
      goto error;
    mysql_mutex_lock(&LOCK_des_key_file);
    keyschedule= des_keyschedule[key_number];
    mysql_mutex_unlock(&LOCK_des_key_file);
  }
  else
  {
    String *keystr=args[1]->val_str(&tmp_value);
    if (!keystr)
      goto error;
    key_number=127;				// User key string

    /* We make good 24-byte (168 bit) key from given plaintext key with MD5 */
    bzero((char*) &ivec,sizeof(ivec));
    EVP_BytesToKey(EVP_des_ede3_cbc(),EVP_md5(),NULL,
		   (uchar*) keystr->ptr(), (int) keystr->length(),
		   1, (uchar*) &keyblock,ivec);
    DES_set_key_unchecked(&keyblock.key1,&keyschedule.ks1);
    DES_set_key_unchecked(&keyblock.key2,&keyschedule.ks2);
    DES_set_key_unchecked(&keyblock.key3,&keyschedule.ks3);
  }

  /*
     The problem: DES algorithm requires original data to be in 8-bytes
     chunks. Missing bytes get filled with '*'s and result of encryption
     can be up to 8 bytes longer than original string. When decrypted,
     we do not know the size of original string :(
     We add one byte with value 0x1..0x8 as the last byte of the padded
     string marking change of string length.
  */

  tail= 8 - (res_length % 8);                   // 1..8 marking extra length
  res_length+=tail;
  if (tmp_arg.realloc(res_length))
    goto error;
  tmp_arg.length(0);
  tmp_arg.append(res->ptr(), res->length());
  code= ER_OUT_OF_RESOURCES;
  if (tmp_arg.append(append_str, tail) || tmp_value.alloc(res_length+1))
    goto error;
  tmp_arg[res_length-1]=tail;                   // save extra length
  tmp_value.realloc(res_length+1);
  tmp_value.length(res_length+1);
  tmp_value.set_charset(&my_charset_bin);
  tmp_value[0]=(char) (128 | key_number);
  // Real encryption
  bzero((char*) &ivec,sizeof(ivec));
  DES_ede3_cbc_encrypt((const uchar*) (tmp_arg.ptr()),
		       (uchar*) (tmp_value.ptr()+1),
		       res_length,
		       &keyschedule.ks1,
		       &keyschedule.ks2,
		       &keyschedule.ks3,
		       &ivec, TRUE);
  return &tmp_value;

error:
  push_warning_printf(current_thd,MYSQL_ERROR::WARN_LEVEL_WARN,
                          code, ER(code),
                          "des_encrypt");
#else
  push_warning_printf(current_thd,MYSQL_ERROR::WARN_LEVEL_WARN,
                      ER_FEATURE_DISABLED, ER(ER_FEATURE_DISABLED),
                      "des_encrypt", "--with-ssl");
#endif /* defined(HAVE_OPENSSL) && !defined(EMBEDDED_LIBRARY) */
  null_value=1;
  return 0;
}


String *Item_func_des_decrypt::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
#if defined(HAVE_OPENSSL) && !defined(EMBEDDED_LIBRARY)
  uint code= ER_WRONG_PARAMETERS_TO_PROCEDURE;
  DES_cblock ivec;
  struct st_des_keyblock keyblock;
  struct st_des_keyschedule keyschedule;
  String *res= args[0]->val_str(str);
  uint length,tail;

  if ((null_value= args[0]->null_value))
    return 0;
  length= res->length();
  if (length < 9 || (length % 8) != 1 || !((*res)[0] & 128))
    return res;				// Skip decryption if not encrypted

  if (arg_count == 1)			// If automatic uncompression
  {
    uint key_number=(uint) (*res)[0] & 127;
    // Check if automatic key and that we have privilege to uncompress using it
    if (!(current_thd->security_ctx->master_access & SUPER_ACL) ||
        key_number > 9)
      goto error;

    mysql_mutex_lock(&LOCK_des_key_file);
    keyschedule= des_keyschedule[key_number];
    mysql_mutex_unlock(&LOCK_des_key_file);
  }
  else
  {
    // We make good 24-byte (168 bit) key from given plaintext key with MD5
    String *keystr=args[1]->val_str(&tmp_value);
    if (!keystr)
      goto error;

    bzero((char*) &ivec,sizeof(ivec));
    EVP_BytesToKey(EVP_des_ede3_cbc(),EVP_md5(),NULL,
		   (uchar*) keystr->ptr(),(int) keystr->length(),
		   1,(uchar*) &keyblock,ivec);
    // Here we set all 64-bit keys (56 effective) one by one
    DES_set_key_unchecked(&keyblock.key1,&keyschedule.ks1);
    DES_set_key_unchecked(&keyblock.key2,&keyschedule.ks2);
    DES_set_key_unchecked(&keyblock.key3,&keyschedule.ks3);
  }
  code= ER_OUT_OF_RESOURCES;
  if (tmp_value.alloc(length-1))
    goto error;

  bzero((char*) &ivec,sizeof(ivec));
  DES_ede3_cbc_encrypt((const uchar*) res->ptr()+1,
		       (uchar*) (tmp_value.ptr()),
		       length-1,
		       &keyschedule.ks1,
		       &keyschedule.ks2,
		       &keyschedule.ks3,
		       &ivec, FALSE);
  /* Restore old length of key */
  if ((tail=(uint) (uchar) tmp_value[length-2]) > 8)
    goto wrong_key;				     // Wrong key
  tmp_value.length(length-1-tail);
  tmp_value.set_charset(&my_charset_bin);
  return &tmp_value;

error:
  push_warning_printf(current_thd,MYSQL_ERROR::WARN_LEVEL_WARN,
                          code, ER(code),
                          "des_decrypt");
wrong_key:
#else
  push_warning_printf(current_thd,MYSQL_ERROR::WARN_LEVEL_WARN,
                      ER_FEATURE_DISABLED, ER(ER_FEATURE_DISABLED),
                      "des_decrypt", "--with-ssl");
#endif /* defined(HAVE_OPENSSL) && !defined(EMBEDDED_LIBRARY) */
  null_value=1;
  return 0;
}


/**
  concat with separator. First arg is the separator
  concat_ws takes at least two arguments.
*/

String *Item_func_concat_ws::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  char tmp_str_buff[10];
  String tmp_sep_str(tmp_str_buff, sizeof(tmp_str_buff),default_charset_info),
         *sep_str, *res, *res2,*use_as_buff;
  uint i;
  bool is_const= 0;

  null_value=0;
  if (!(sep_str= args[0]->val_str(&tmp_sep_str)))
    goto null;

  use_as_buff= &tmp_value;
  str->length(0);				// QQ; Should be removed
  res=str;                                      // If 0 arg_count

  // Skip until non-null argument is found.
  // If not, return the empty string
  for (i=1; i < arg_count; i++)
    if ((res= args[i]->val_str(str)))
    {
      is_const= args[i]->const_item() || !args[i]->used_tables();
      break;
    }

  if (i ==  arg_count)
    return make_empty_result();

  for (i++; i < arg_count ; i++)
  {
    if (!(res2= args[i]->val_str(use_as_buff)))
      continue;					// Skip NULL

    if (res->length() + sep_str->length() + res2->length() >
	current_thd->variables.max_allowed_packet)
    {
      push_warning_printf(current_thd, MYSQL_ERROR::WARN_LEVEL_WARN,
			  ER_WARN_ALLOWED_PACKET_OVERFLOWED,
			  ER(ER_WARN_ALLOWED_PACKET_OVERFLOWED), func_name(),
			  current_thd->variables.max_allowed_packet);
      goto null;
    }
    if (!is_const && res->alloced_length() >=
	res->length() + sep_str->length() + res2->length())
    {						// Use old buffer
      res->append(*sep_str);			// res->length() > 0 always
      res->append(*res2);
    }
    else if (str->alloced_length() >=
	     res->length() + sep_str->length() + res2->length())
    {
      /* We have room in str;  We can't get any errors here */
      if (str->ptr() == res2->ptr())
      {						// This is quite uncommon!
	str->replace(0,0,*sep_str);
	str->replace(0,0,*res);
      }
      else
      {
	str->copy(*res);
	str->append(*sep_str);
	str->append(*res2);
      }
      res=str;
      use_as_buff= &tmp_value;
    }
    else if (res == &tmp_value)
    {
      if (res->append(*sep_str) || res->append(*res2))
	goto null; // Must be a blob
    }
    else if (res2 == &tmp_value)
    {						// This can happend only 1 time
      if (tmp_value.replace(0,0,*sep_str) || tmp_value.replace(0,0,*res))
	goto null;
      res= &tmp_value;
      use_as_buff=str;				// Put next arg here
    }
    else if (tmp_value.is_alloced() && res2->ptr() >= tmp_value.ptr() &&
	     res2->ptr() < tmp_value.ptr() + tmp_value.alloced_length())
    {
      /*
	This happens really seldom:
	In this case res2 is sub string of tmp_value.  We will
	now work in place in tmp_value to set it to res | sep_str | res2
      */
      /* Chop the last characters in tmp_value that isn't in res2 */
      tmp_value.length((uint32) (res2->ptr() - tmp_value.ptr()) +
		       res2->length());
      /* Place res2 at start of tmp_value, remove chars before res2 */
      if (tmp_value.replace(0,(uint32) (res2->ptr() - tmp_value.ptr()),
			    *res) ||
	  tmp_value.replace(res->length(),0, *sep_str))
	goto null;
      res= &tmp_value;
      use_as_buff=str;			// Put next arg here
    }
    else
    {						// Two big const strings
      /*
        NOTE: We should be prudent in the initial allocation unit -- the
        size of the arguments is a function of data distribution, which can
        be any. Instead of overcommitting at the first row, we grow the
        allocated amount by the factor of 2. This ensures that no more than
        25% of memory will be overcommitted on average.
      */

      uint concat_len= res->length() + sep_str->length() + res2->length();

      if (tmp_value.alloced_length() < concat_len)
      {
        if (tmp_value.alloced_length() == 0)
        {
          if (tmp_value.alloc(concat_len))
            goto null;
        }
        else
        {
          uint new_len = max(tmp_value.alloced_length() * 2, concat_len);

          if (tmp_value.realloc(new_len))
            goto null;
        }
      }

      if (tmp_value.copy(*res) ||
	  tmp_value.append(*sep_str) ||
	  tmp_value.append(*res2))
	goto null;
      res= &tmp_value;
      use_as_buff=str;
    }
  }
  res->set_charset(collation.collation);
  return res;

null:
  null_value=1;
  return 0;
}


void Item_func_concat_ws::fix_length_and_dec()
{
  ulonglong char_length;

  if (agg_arg_charsets_for_string_result(collation, args, arg_count))
    return;

  /*
     arg_count cannot be less than 2,
     it is done on parser level in sql_yacc.yy
     so, (arg_count - 2) is safe here.
  */
  char_length= (ulonglong) args[0]->max_char_length() * (arg_count - 2);
  for (uint i=1 ; i < arg_count ; i++)
    char_length+= args[i]->max_char_length();

  fix_char_length_ulonglong(char_length);
}


String *Item_func_reverse::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  String *res = args[0]->val_str(str);
  char *ptr, *end, *tmp;

  if ((null_value=args[0]->null_value))
    return 0;
  /* An empty string is a special case as the string pointer may be null */
  if (!res->length())
    return make_empty_result();
  if (tmp_value.alloced_length() < res->length() &&
      tmp_value.realloc(res->length()))
  {
    null_value= 1;
    return 0;
  }
  tmp_value.length(res->length());
  tmp_value.set_charset(res->charset());
  ptr= (char *) res->ptr();
  end= ptr + res->length();
  tmp= (char *) tmp_value.ptr() + tmp_value.length();
#ifdef USE_MB
  if (use_mb(res->charset()))
  {
    register uint32 l;
    while (ptr < end)
    {
      if ((l= my_ismbchar(res->charset(),ptr,end)))
      {
        tmp-= l;
        DBUG_ASSERT(tmp >= tmp_value.ptr());
        memcpy(tmp,ptr,l);
        ptr+= l;
      }
      else
        *--tmp= *ptr++;
    }
  }
  else
#endif /* USE_MB */
  {
    while (ptr < end)
      *--tmp= *ptr++;
  }
  return &tmp_value;
}


void Item_func_reverse::fix_length_and_dec()
{
  agg_arg_charsets_for_string_result(collation, args, 1);
  DBUG_ASSERT(collation.collation != NULL);
  fix_char_length(args[0]->max_char_length());
}

/**
  Replace all occurences of string2 in string1 with string3.

  Don't reallocate val_str() if not needed.

  @todo
    Fix that this works with binary strings when using USE_MB 
*/

String *Item_func_replace::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  String *res,*res2,*res3;
  int offset;
  uint from_length,to_length;
  bool alloced=0;
#ifdef USE_MB
  const char *ptr,*end,*strend,*search,*search_end;
  register uint32 l;
  bool binary_cmp;
#endif

  null_value=0;
  res=args[0]->val_str(str);
  if (args[0]->null_value)
    goto null;
  res2=args[1]->val_str(&tmp_value);
  if (args[1]->null_value)
    goto null;

  res->set_charset(collation.collation);

#ifdef USE_MB
  binary_cmp = ((res->charset()->state & MY_CS_BINSORT) || !use_mb(res->charset()));
#endif

  if (res2->length() == 0)
    return res;
#ifndef USE_MB
  if ((offset=res->strstr(*res2)) < 0)
    return res;
#else
  offset=0;
  if (binary_cmp && (offset=res->strstr(*res2)) < 0)
    return res;
#endif
  if (!(res3=args[2]->val_str(&tmp_value2)))
    goto null;
  from_length= res2->length();
  to_length=   res3->length();

#ifdef USE_MB
  if (!binary_cmp)
  {
    search=res2->ptr();
    search_end=search+from_length;
redo:
    DBUG_ASSERT(res->ptr() || !offset);
    ptr=res->ptr()+offset;
    strend=res->ptr()+res->length();
    /*
      In some cases val_str() can return empty string
      with ptr() == NULL and length() == 0.
      Let's check strend to avoid overflow.
    */
    end= strend ? strend - from_length + 1 : NULL;
    while (ptr < end)
    {
        if (*ptr == *search)
        {
          register char *i,*j;
          i=(char*) ptr+1; j=(char*) search+1;
          while (j != search_end)
            if (*i++ != *j++) goto skip;
          offset= (int) (ptr-res->ptr());
          if (res->length()-from_length + to_length >
	      current_thd->variables.max_allowed_packet)
	  {
	    push_warning_printf(current_thd, MYSQL_ERROR::WARN_LEVEL_WARN,
				ER_WARN_ALLOWED_PACKET_OVERFLOWED,
				ER(ER_WARN_ALLOWED_PACKET_OVERFLOWED),
				func_name(),
				current_thd->variables.max_allowed_packet);

            goto null;
	  }
          if (!alloced)
          {
            alloced=1;
            res=copy_if_not_alloced(str,res,res->length()+to_length);
          }
          res->replace((uint) offset,from_length,*res3);
	  offset+=(int) to_length;
          goto redo;
        }
skip:
        if ((l=my_ismbchar(res->charset(), ptr,strend))) ptr+=l;
        else ++ptr;
    }
  }
  else
#endif /* USE_MB */
    do
    {
      if (res->length()-from_length + to_length >
	  current_thd->variables.max_allowed_packet)
      {
	push_warning_printf(current_thd, MYSQL_ERROR::WARN_LEVEL_WARN,
			    ER_WARN_ALLOWED_PACKET_OVERFLOWED,
			    ER(ER_WARN_ALLOWED_PACKET_OVERFLOWED), func_name(),
			    current_thd->variables.max_allowed_packet);
        goto null;
      }
      if (!alloced)
      {
        alloced=1;
        res=copy_if_not_alloced(str,res,res->length()+to_length);
      }
      res->replace((uint) offset,from_length,*res3);
      offset+=(int) to_length;
    }
    while ((offset=res->strstr(*res2,(uint) offset)) >= 0);
  return res;

null:
  null_value=1;
  return 0;
}


void Item_func_replace::fix_length_and_dec()
{
  ulonglong char_length= (ulonglong) args[0]->max_char_length();
  int diff=(int) (args[2]->max_char_length() - args[1]->max_char_length());
  if (diff > 0 && args[1]->max_char_length())
  {						// Calculate of maxreplaces
    ulonglong max_substrs= char_length / args[1]->max_char_length();
    char_length+= max_substrs * (uint) diff;
  }

  if (agg_arg_charsets_for_string_result_with_comparison(collation, args, 3))
    return;
  fix_char_length_ulonglong(char_length);
}


String *Item_func_insert::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  String *res,*res2;
  longlong start, length;  /* must be longlong to avoid truncation */

  null_value=0;
  res=args[0]->val_str(str);
  res2=args[3]->val_str(&tmp_value);
  start= args[1]->val_int() - 1;
  length= args[2]->val_int();

  if (args[0]->null_value || args[1]->null_value || args[2]->null_value ||
      args[3]->null_value)
    goto null; /* purecov: inspected */

  if ((start < 0) || (start > res->length()))
    return res;                                 // Wrong param; skip insert
  if ((length < 0) || (length > res->length()))
    length= res->length();

  /*
    There is one exception not handled (intentionaly) by the character set
    aggregation code. If one string is strong side and is binary, and
    another one is weak side and is a multi-byte character string,
    then we need to operate on the second string in terms on bytes when
    calling ::numchars() and ::charpos(), rather than in terms of characters.
    Lets substitute its character set to binary.
  */
  if (collation.collation == &my_charset_bin)
  {
    res->set_charset(&my_charset_bin);
    res2->set_charset(&my_charset_bin);
  }

  /* start and length are now sufficiently valid to pass to charpos function */
   start= res->charpos((int) start);
   length= res->charpos((int) length, (uint32) start);

  /* Re-testing with corrected params */
  if (start + 1 > res->length()) // remember, start = args[1].val_int() - 1
    return res; /* purecov: inspected */        // Wrong param; skip insert
  if (length > res->length() - start)
    length= res->length() - start;

  if ((ulonglong) (res->length() - length + res2->length()) >
      (ulonglong) current_thd->variables.max_allowed_packet)
  {
    push_warning_printf(current_thd, MYSQL_ERROR::WARN_LEVEL_WARN,
			ER_WARN_ALLOWED_PACKET_OVERFLOWED,
			ER(ER_WARN_ALLOWED_PACKET_OVERFLOWED),
			func_name(), current_thd->variables.max_allowed_packet);
    goto null;
  }
  res=copy_if_not_alloced(str,res,res->length());
  res->replace((uint32) start,(uint32) length,*res2);
  return res;
null:
  null_value=1;
  return 0;
}


void Item_func_insert::fix_length_and_dec()
{
  ulonglong char_length;

  // Handle character set for args[0] and args[3].
  if (agg_arg_charsets_for_string_result(collation, args, 2, 3))
    return;
  char_length= ((ulonglong) args[0]->max_char_length() +
                (ulonglong) args[3]->max_char_length());
  fix_char_length_ulonglong(char_length);
}


String *Item_str_conv::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  String *res;
  if (!(res=args[0]->val_str(str)))
  {
    null_value=1; /* purecov: inspected */
    return 0; /* purecov: inspected */
  }
  null_value=0;
  if (multiply == 1)
  {
    uint len;
    res= copy_if_not_alloced(&tmp_value, res, res->length());
    len= converter(collation.collation, (char*) res->ptr(), res->length(),
                                        (char*) res->ptr(), res->length());
    DBUG_ASSERT(len <= res->length());
    res->length(len);
  }
  else
  {
    uint len= res->length() * multiply;
    tmp_value.alloc(len);
    tmp_value.set_charset(collation.collation);
    len= converter(collation.collation, (char*) res->ptr(), res->length(),
                                        (char*) tmp_value.ptr(), len);
    tmp_value.length(len);
    res= &tmp_value;
  }
  return res;
}


void Item_func_lcase::fix_length_and_dec()
{
  agg_arg_charsets_for_string_result(collation, args, 1);
  DBUG_ASSERT(collation.collation != NULL);
  multiply= collation.collation->casedn_multiply;
  converter= collation.collation->cset->casedn;
  fix_char_length_ulonglong((ulonglong) args[0]->max_char_length() * multiply);
}

void Item_func_ucase::fix_length_and_dec()
{
  agg_arg_charsets_for_string_result(collation, args, 1);
  DBUG_ASSERT(collation.collation != NULL);
  multiply= collation.collation->caseup_multiply;
  converter= collation.collation->cset->caseup;
  fix_char_length_ulonglong((ulonglong) args[0]->max_char_length() * multiply);
}


String *Item_func_left::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  String *res= args[0]->val_str(str);

  /* must be longlong to avoid truncation */
  longlong length= args[1]->val_int();
  uint char_pos;

  if ((null_value=(args[0]->null_value || args[1]->null_value)))
    return 0;

  /* if "unsigned_flag" is set, we have a *huge* positive number. */
  if ((length <= 0) && (!args[1]->unsigned_flag))
    return make_empty_result();
  if ((res->length() <= (ulonglong) length) ||
      (res->length() <= (char_pos= res->charpos((int) length))))
    return res;

  tmp_value.set(*res, 0, char_pos);
  return &tmp_value;
}


void Item_str_func::left_right_max_length()
{
  uint32 char_length= args[0]->max_char_length();
  if (args[1]->const_item())
  {
    int length= (int) args[1]->val_int();
    if (args[1]->null_value || length <= 0)
      char_length=0;
    else
      set_if_smaller(char_length, (uint) length);
  }
  fix_char_length(char_length);
}


void Item_func_left::fix_length_and_dec()
{
  agg_arg_charsets_for_string_result(collation, args, 1);
  DBUG_ASSERT(collation.collation != NULL);
  left_right_max_length();
}


String *Item_func_right::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  String *res= args[0]->val_str(str);
  /* must be longlong to avoid truncation */
  longlong length= args[1]->val_int();

  if ((null_value=(args[0]->null_value || args[1]->null_value)))
    return 0; /* purecov: inspected */

  /* if "unsigned_flag" is set, we have a *huge* positive number. */
  if ((length <= 0) && (!args[1]->unsigned_flag))
    return make_empty_result(); /* purecov: inspected */

  if (res->length() <= (ulonglong) length)
    return res; /* purecov: inspected */

  uint start=res->numchars();
  if (start <= (uint) length)
    return res;
  start=res->charpos(start - (uint) length);
  tmp_value.set(*res,start,res->length()-start);
  return &tmp_value;
}


void Item_func_right::fix_length_and_dec()
{
  agg_arg_charsets_for_string_result(collation, args, 1);
  DBUG_ASSERT(collation.collation != NULL);
  left_right_max_length();
}


String *Item_func_substr::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  String *res  = args[0]->val_str(str);
  /* must be longlong to avoid truncation */
  longlong start= args[1]->val_int();
  /* Assumes that the maximum length of a String is < INT_MAX32. */
  /* Limit so that code sees out-of-bound value properly. */
  longlong length= arg_count == 3 ? args[2]->val_int() : INT_MAX32;
  longlong tmp_length;

  if ((null_value=(args[0]->null_value || args[1]->null_value ||
		   (arg_count == 3 && args[2]->null_value))))
    return 0; /* purecov: inspected */

  /* Negative or zero length, will return empty string. */
  if ((arg_count == 3) && (length <= 0) && 
      (length == 0 || !args[2]->unsigned_flag))
    return make_empty_result();

  /* Assumes that the maximum length of a String is < INT_MAX32. */
  /* Set here so that rest of code sees out-of-bound value as such. */
  if ((length <= 0) || (length > INT_MAX32))
    length= INT_MAX32;

  /* if "unsigned_flag" is set, we have a *huge* positive number. */
  /* Assumes that the maximum length of a String is < INT_MAX32. */
  if ((!args[1]->unsigned_flag && (start < INT_MIN32 || start > INT_MAX32)) ||
      (args[1]->unsigned_flag && ((ulonglong) start > INT_MAX32)))
    return make_empty_result();

  start= ((start < 0) ? res->numchars() + start : start - 1);
  start= res->charpos((int) start);
  if ((start < 0) || ((uint) start + 1 > res->length()))
    return make_empty_result();

  length= res->charpos((int) length, (uint32) start);
  tmp_length= res->length() - start;
  length= min(length, tmp_length);

  if (!start && (longlong) res->length() == length)
    return res;
  tmp_value.set(*res, (uint32) start, (uint32) length);
  return &tmp_value;
}


void Item_func_substr::fix_length_and_dec()
{
  max_length=args[0]->max_length;

  agg_arg_charsets_for_string_result(collation, args, 1);
  DBUG_ASSERT(collation.collation != NULL);
  if (args[1]->const_item())
  {
    int32 start= (int32) args[1]->val_int();
    if (args[1]->null_value)
      max_length= 0;
    else if (start < 0)
      max_length= ((uint)(-start) > max_length) ? 0 : (uint)(-start);
    else
      max_length-= min((uint)(start - 1), max_length);
  }
  if (arg_count == 3 && args[2]->const_item())
  {
    int32 length= (int32) args[2]->val_int();
    if (args[2]->null_value || length <= 0)
      max_length=0; /* purecov: inspected */
    else
      set_if_smaller(max_length,(uint) length);
  }
  max_length*= collation.collation->mbmaxlen;
}


void Item_func_substr_index::fix_length_and_dec()
{ 
  if (agg_arg_charsets_for_string_result_with_comparison(collation, args, 2))
    return;
  fix_char_length(args[0]->max_char_length());
}


String *Item_func_substr_index::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  char buff[MAX_FIELD_WIDTH];
  String tmp(buff,sizeof(buff),system_charset_info);
  String *res= args[0]->val_str(str);
  String *delimiter= args[1]->val_str(&tmp);
  int32 count= (int32) args[2]->val_int();
  uint offset;

  if (args[0]->null_value || args[1]->null_value || args[2]->null_value)
  {					// string and/or delim are null
    null_value=1;
    return 0;
  }
  null_value=0;
  uint delimiter_length= delimiter->length();
  if (!res->length() || !delimiter_length || !count)
    return make_empty_result();		// Wrong parameters

  res->set_charset(collation.collation);

#ifdef USE_MB
  if (use_mb(res->charset()))
  {
    const char *ptr= res->ptr();
    const char *strend= ptr+res->length();
    const char *end= strend-delimiter_length+1;
    const char *search= delimiter->ptr();
    const char *search_end= search+delimiter_length;
    int32 n=0,c=count,pass;
    register uint32 l;
    for (pass=(count>0);pass<2;++pass)
    {
      while (ptr < end)
      {
        if (*ptr == *search)
        {
	  register char *i,*j;
	  i=(char*) ptr+1; j=(char*) search+1;
	  while (j != search_end)
	    if (*i++ != *j++) goto skip;
	  if (pass==0) ++n;
	  else if (!--c) break;
	  ptr+= delimiter_length;
	  continue;
	}
    skip:
        if ((l=my_ismbchar(res->charset(), ptr,strend))) ptr+=l;
        else ++ptr;
      } /* either not found or got total number when count<0 */
      if (pass == 0) /* count<0 */
      {
        c+=n+1;
        if (c<=0) return res; /* not found, return original string */
        ptr=res->ptr();
      }
      else
      {
        if (c) return res; /* Not found, return original string */
        if (count>0) /* return left part */
        {
	  tmp_value.set(*res,0,(ulong) (ptr-res->ptr()));
        }
        else /* return right part */
        {
	  ptr+= delimiter_length;
	  tmp_value.set(*res,(ulong) (ptr-res->ptr()), (ulong) (strend-ptr));
        }
      }
    }
  }
  else
#endif /* USE_MB */
  {
    if (count > 0)
    {					// start counting from the beginning
      for (offset=0; ; offset+= delimiter_length)
      {
	if ((int) (offset= res->strstr(*delimiter, offset)) < 0)
	  return res;			// Didn't find, return org string
	if (!--count)
	{
	  tmp_value.set(*res,0,offset);
	  break;
	}
      }
    }
    else
    {
      /*
        Negative index, start counting at the end
      */
      for (offset=res->length(); offset ;)
      {
        /* 
          this call will result in finding the position pointing to one 
          address space less than where the found substring is located
          in res
        */
	if ((int) (offset= res->strrstr(*delimiter, offset)) < 0)
	  return res;			// Didn't find, return org string
        /*
          At this point, we've searched for the substring
          the number of times as supplied by the index value
        */
	if (!++count)
	{
	  offset+= delimiter_length;
	  tmp_value.set(*res,offset,res->length()- offset);
	  break;
	}
      }
      if (count)
        return res;                     // Didn't find, return org string
    }
  }
  /*
    We always mark tmp_value as const so that if val_str() is called again
    on this object, we don't disrupt the contents of tmp_value when it was
    derived from another String.
  */
  tmp_value.mark_as_const();
  return (&tmp_value);
}

/*
** The trim functions are extension to ANSI SQL because they trim substrings
** They ltrim() and rtrim() functions are optimized for 1 byte strings
** They also return the original string if possible, else they return
** a substring that points at the original string.
*/


String *Item_func_ltrim::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  char buff[MAX_FIELD_WIDTH], *ptr, *end;
  String tmp(buff,sizeof(buff),system_charset_info);
  String *res, *remove_str;
  uint remove_length;
  LINT_INIT(remove_length);

  res= args[0]->val_str(str);
  if ((null_value=args[0]->null_value))
    return 0;
  remove_str= &remove;                          /* Default value. */
  if (arg_count == 2)
  {
    remove_str= args[1]->val_str(&tmp);
    if ((null_value= args[1]->null_value))
      return 0;
  }

  if ((remove_length= remove_str->length()) == 0 ||
      remove_length > res->length())
    return non_trimmed_value(res);

  ptr= (char*) res->ptr();
  end= ptr+res->length();
  if (remove_length == 1)
  {
    char chr=(*remove_str)[0];
    while (ptr != end && *ptr == chr)
      ptr++;
  }
  else
  {
    const char *r_ptr=remove_str->ptr();
    end-=remove_length;
    while (ptr <= end && !memcmp(ptr, r_ptr, remove_length))
      ptr+=remove_length;
    end+=remove_length;
  }
  if (ptr == res->ptr())
    return non_trimmed_value(res);
  return trimmed_value(res, (uint32) (ptr - res->ptr()), (uint32) (end - ptr));
}


String *Item_func_rtrim::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  char buff[MAX_FIELD_WIDTH], *ptr, *end;
  String tmp(buff, sizeof(buff), system_charset_info);
  String *res, *remove_str;
  uint remove_length;
  LINT_INIT(remove_length);

  res= args[0]->val_str(str);
  if ((null_value=args[0]->null_value))
    return 0;
  remove_str= &remove;                          /* Default value. */
  if (arg_count == 2)
  {
    remove_str= args[1]->val_str(&tmp);
    if ((null_value= args[1]->null_value))
      return 0;
  }

  if ((remove_length= remove_str->length()) == 0 ||
      remove_length > res->length())
    return non_trimmed_value(res);

  ptr= (char*) res->ptr();
  end= ptr+res->length();
#ifdef USE_MB
  char *p=ptr;
  register uint32 l;
#endif
  if (remove_length == 1)
  {
    char chr=(*remove_str)[0];
#ifdef USE_MB
    if (use_mb(collation.collation))
    {
      while (ptr < end)
      {
	if ((l= my_ismbchar(collation.collation, ptr, end))) ptr+= l, p=ptr;
	else ++ptr;
      }
      ptr=p;
    }
#endif
    while (ptr != end  && end[-1] == chr)
      end--;
  }
  else
  {
    const char *r_ptr=remove_str->ptr();
#ifdef USE_MB
    if (use_mb(collation.collation))
    {
  loop:
      while (ptr + remove_length < end)
      {
	if ((l= my_ismbchar(collation.collation, ptr, end))) ptr+= l;
	else ++ptr;
      }
      if (ptr + remove_length == end && !memcmp(ptr,r_ptr,remove_length))
      {
	end-=remove_length;
	ptr=p;
	goto loop;
      }
    }
    else
#endif /* USE_MB */
    {
      while (ptr + remove_length <= end &&
	     !memcmp(end-remove_length, r_ptr, remove_length))
	end-=remove_length;
    }
  }
  if (end == res->ptr()+res->length())
    return non_trimmed_value(res);
  return trimmed_value(res, 0, (uint32) (end - res->ptr()));
}


String *Item_func_trim::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  char buff[MAX_FIELD_WIDTH], *ptr, *end;
  const char *r_ptr;
  String tmp(buff, sizeof(buff), system_charset_info);
  String *res, *remove_str;
  uint remove_length;
  LINT_INIT(remove_length);

  res= args[0]->val_str(str);
  if ((null_value=args[0]->null_value))
    return 0;
  remove_str= &remove;                          /* Default value. */
  if (arg_count == 2)
  {
    remove_str= args[1]->val_str(&tmp);
    if ((null_value= args[1]->null_value))
      return 0;
  }

  if ((remove_length= remove_str->length()) == 0 ||
      remove_length > res->length())
    return non_trimmed_value(res);

  ptr= (char*) res->ptr();
  end= ptr+res->length();
  r_ptr= remove_str->ptr();
  while (ptr+remove_length <= end && !memcmp(ptr,r_ptr,remove_length))
    ptr+=remove_length;
#ifdef USE_MB
  if (use_mb(collation.collation))
  {
    char *p=ptr;
    register uint32 l;
 loop:
    while (ptr + remove_length < end)
    {
      if ((l= my_ismbchar(collation.collation, ptr, end)))
        ptr+= l;
      else
        ++ptr;
    }
    if (ptr + remove_length == end && !memcmp(ptr,r_ptr,remove_length))
    {
      end-=remove_length;
      ptr=p;
      goto loop;
    }
    ptr=p;
  }
  else
#endif /* USE_MB */
  {
    while (ptr + remove_length <= end &&
	   !memcmp(end-remove_length,r_ptr,remove_length))
      end-=remove_length;
  }
  if (ptr == res->ptr() && end == ptr+res->length())
    return non_trimmed_value(res);
  return trimmed_value(res, (uint32) (ptr - res->ptr()), (uint32) (end - ptr));
}

void Item_func_trim::fix_length_and_dec()
{
  if (arg_count == 1)
  {
    agg_arg_charsets_for_string_result(collation, args, 1);
    DBUG_ASSERT(collation.collation != NULL);
    remove.set_charset(collation.collation);
    remove.set_ascii(" ",1);
  }
  else
  {
    // Handle character set for args[1] and args[0].
    // Note that we pass args[1] as the first item, and args[0] as the second.
    if (agg_arg_charsets_for_string_result_with_comparison(collation,
                                                           &args[1], 2, -1))
      return;
  }
  fix_char_length(args[0]->max_char_length());
}

void Item_func_trim::print(String *str, enum_query_type query_type)
{
  if (arg_count == 1)
  {
    Item_func::print(str, query_type);
    return;
  }
  str->append(Item_func_trim::func_name());
  str->append('(');
  str->append(mode_name());
  str->append(' ');
  args[1]->print(str, query_type);
  str->append(STRING_WITH_LEN(" from "));
  args[0]->print(str, query_type);
  str->append(')');
}


/* Item_func_password */

String *Item_func_password::val_str_ascii(String *str)
{
  DBUG_ASSERT(fixed == 1);
  String *res= args[0]->val_str(str); 
  if ((null_value=args[0]->null_value))
    return 0;
  if (res->length() == 0)
    return make_empty_result();
  my_make_scrambled_password(tmp_value, res->ptr(), res->length());
  str->set(tmp_value, SCRAMBLED_PASSWORD_CHAR_LENGTH, &my_charset_latin1);
  return str;
}

char *Item_func_password::alloc(THD *thd, const char *password,
                                size_t pass_len)
{
  char *buff= (char *) thd->alloc(SCRAMBLED_PASSWORD_CHAR_LENGTH+1);
  if (buff)
    my_make_scrambled_password(buff, password, pass_len);
  return buff;
}

/* Item_func_old_password */

String *Item_func_old_password::val_str_ascii(String *str)
{
  DBUG_ASSERT(fixed == 1);
  String *res= args[0]->val_str(str);
  if ((null_value=args[0]->null_value))
    return 0;
  if (res->length() == 0)
    return make_empty_result();
  my_make_scrambled_password_323(tmp_value, res->ptr(), res->length());
  str->set(tmp_value, SCRAMBLED_PASSWORD_CHAR_LENGTH_323, &my_charset_latin1);
  return str;
}

char *Item_func_old_password::alloc(THD *thd, const char *password,
                                    size_t pass_len)
{
  char *buff= (char *) thd->alloc(SCRAMBLED_PASSWORD_CHAR_LENGTH_323+1);
  if (buff)
    my_make_scrambled_password_323(buff, password, pass_len);
  return buff;
}


#define bin_to_ascii(c) ((c)>=38?((c)-38+'a'):(c)>=12?((c)-12+'A'):(c)+'.')

String *Item_func_encrypt::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  String *res  =args[0]->val_str(str);

#ifdef HAVE_CRYPT
  char salt[3],*salt_ptr;
  if ((null_value=args[0]->null_value))
    return 0;
  if (res->length() == 0)
    return make_empty_result();
  if (arg_count == 1)
  {					// generate random salt
    time_t timestamp=current_thd->query_start();
    salt[0] = bin_to_ascii( (ulong) timestamp & 0x3f);
    salt[1] = bin_to_ascii(( (ulong) timestamp >> 5) & 0x3f);
    salt[2] = 0;
    salt_ptr=salt;
  }
  else
  {					// obtain salt from the first two bytes
    String *salt_str=args[1]->val_str(&tmp_value);
    if ((null_value= (args[1]->null_value || salt_str->length() < 2)))
      return 0;
    salt_ptr= salt_str->c_ptr_safe();
  }
  mysql_mutex_lock(&LOCK_crypt);
  char *tmp= crypt(res->c_ptr_safe(),salt_ptr);
  if (!tmp)
  {
    mysql_mutex_unlock(&LOCK_crypt);
    null_value= 1;
    return 0;
  }
  str->set(tmp, (uint) strlen(tmp), &my_charset_bin);
  str->copy();
  mysql_mutex_unlock(&LOCK_crypt);
  return str;
#else
  null_value=1;
  return 0;
#endif	/* HAVE_CRYPT */
}

bool Item_func_encode::seed()
{
  char buf[80];
  ulong rand_nr[2];
  String *key, tmp(buf, sizeof(buf), system_charset_info);

  if (!(key= args[1]->val_str(&tmp)))
    return TRUE;

  hash_password(rand_nr, key->ptr(), key->length());
  sql_crypt.init(rand_nr);

  return FALSE;
}

void Item_func_encode::fix_length_and_dec()
{
  max_length=args[0]->max_length;
  maybe_null=args[0]->maybe_null || args[1]->maybe_null;
  collation.set(&my_charset_bin);
  /* Precompute the seed state if the item is constant. */
  seeded= args[1]->const_item() &&
          (args[1]->result_type() == STRING_RESULT) && !seed();
}

String *Item_func_encode::val_str(String *str)
{
  String *res;
  DBUG_ASSERT(fixed == 1);

  if (!(res=args[0]->val_str(str)))
  {
    null_value= 1;
    return NULL;
  }

  if (!seeded && seed())
  {
    null_value= 1;
    return NULL;
  }

  null_value= 0;
  res= copy_if_not_alloced(str, res, res->length());
  crypto_transform(res);
  sql_crypt.reinit();

  return res;
}

void Item_func_encode::crypto_transform(String *res)
{
  sql_crypt.encode((char*) res->ptr(),res->length());
  res->set_charset(&my_charset_bin);
}

void Item_func_decode::crypto_transform(String *res)
{
  sql_crypt.decode((char*) res->ptr(),res->length());
}


Item *Item_func_sysconst::safe_charset_converter(CHARSET_INFO *tocs)
{
  Item_string *conv;
  uint conv_errors;
  String tmp, cstr, *ostr= val_str(&tmp);
  if (null_value)
  {
    Item *null_item= new Item_null((char *) fully_qualified_func_name());
    null_item->collation.set (tocs);
    return null_item;
  }
  cstr.copy(ostr->ptr(), ostr->length(), ostr->charset(), tocs, &conv_errors);
  if (conv_errors ||
      !(conv= new Item_static_string_func(fully_qualified_func_name(),
                                          cstr.ptr(), cstr.length(),
                                          cstr.charset(),
                                          collation.derivation)))
  {
    return NULL;
  }
  conv->str_value.copy();
  conv->str_value.mark_as_const();
  return conv;
}


String *Item_func_database::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  THD *thd= current_thd;
  if (thd->db == NULL)
  {
    null_value= 1;
    return 0;
  }
  else
    str->copy(thd->db, thd->db_length, system_charset_info);
  return str;
}


/**
  @note USER() is replicated correctly if binlog_format=ROW or (as of
  BUG#28086) binlog_format=MIXED, but is incorrectly replicated to ''
  if binlog_format=STATEMENT.
*/
bool Item_func_user::init(const char *user, const char *host)
{
  DBUG_ASSERT(fixed == 1);

  // For system threads (e.g. replication SQL thread) user may be empty
  if (user)
  {
    CHARSET_INFO *cs= str_value.charset();
    size_t res_length= (strlen(user)+strlen(host)+2) * cs->mbmaxlen;

    if (str_value.alloc((uint) res_length))
    {
      null_value=1;
      return TRUE;
    }

    res_length=cs->cset->snprintf(cs, (char*)str_value.ptr(), (uint) res_length,
                                  "%s@%s", user, host);
    str_value.length((uint) res_length);
    str_value.mark_as_const();
  }
  return FALSE;
}


bool Item_func_user::fix_fields(THD *thd, Item **ref)
{
  return (Item_func_sysconst::fix_fields(thd, ref) ||
          init(thd->main_security_ctx.user,
               thd->main_security_ctx.host_or_ip));
}


bool Item_func_current_user::fix_fields(THD *thd, Item **ref)
{
  if (Item_func_sysconst::fix_fields(thd, ref))
    return TRUE;

  Security_context *ctx=
#ifndef NO_EMBEDDED_ACCESS_CHECKS
                         (context->security_ctx
                          ? context->security_ctx : thd->security_ctx);
#else
                         thd->security_ctx;
#endif /*NO_EMBEDDED_ACCESS_CHECKS*/
  return init(ctx->priv_user, ctx->priv_host);
}


void Item_func_soundex::fix_length_and_dec()
{
  uint32 char_length= args[0]->max_char_length();
  agg_arg_charsets_for_string_result(collation, args, 1);
  DBUG_ASSERT(collation.collation != NULL);
  set_if_bigger(char_length, 4);
  fix_char_length(char_length);
  tmp_value.set_charset(collation.collation);
}


/**
  If alpha, map input letter to soundex code.
  If not alpha and remove_garbage is set then skip to next char
  else return 0
*/

static int soundex_toupper(int ch)
{
  return (ch >= 'a' && ch <= 'z') ? ch - 'a' + 'A' : ch;
}


static char get_scode(int wc)
{
  int ch= soundex_toupper(wc);
  if (ch < 'A' || ch > 'Z')
  {
					// Thread extended alfa (country spec)
    return '0';				// as vokal
  }
  return(soundex_map[ch-'A']);
}


static bool my_uni_isalpha(int wc)
{
  /*
    Return true for all Basic Latin letters: a..z A..Z.
    Return true for all Unicode characters with code higher than U+00C0:
    - characters between 'z' and U+00C0 are controls and punctuations.
    - "U+00C0 LATIN CAPITAL LETTER A WITH GRAVE" is the first letter after 'z'.
  */
  return (wc >= 'a' && wc <= 'z') ||
         (wc >= 'A' && wc <= 'Z') ||
         (wc >= 0xC0);
}


String *Item_func_soundex::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  String *res  =args[0]->val_str(str);
  char last_ch,ch;
  CHARSET_INFO *cs= collation.collation;
  my_wc_t wc;
  uint nchars;
  int rc;

  if ((null_value= args[0]->null_value))
    return 0; /* purecov: inspected */

  if (tmp_value.alloc(max(res->length(), 4 * cs->mbminlen)))
    return str; /* purecov: inspected */
  char *to= (char *) tmp_value.ptr();
  char *to_end= to + tmp_value.alloced_length();
  char *from= (char *) res->ptr(), *end= from + res->length();
  
  for ( ; ; ) /* Skip pre-space */
  {
    if ((rc= cs->cset->mb_wc(cs, &wc, (uchar*) from, (uchar*) end)) <= 0)
      return make_empty_result(); /* EOL or invalid byte sequence */
    
    if (rc == 1 && cs->ctype)
    {
      /* Single byte letter found */
      if (my_isalpha(cs, *from))
      {
        last_ch= get_scode(*from);       // Code of the first letter
        *to++= soundex_toupper(*from++); // Copy first letter
        break;
      }
      from++;
    }
    else
    {
      from+= rc;
      if (my_uni_isalpha(wc))
      {
        /* Multibyte letter found */
        wc= soundex_toupper(wc);
        last_ch= get_scode(wc);     // Code of the first letter
        if ((rc= cs->cset->wc_mb(cs, wc, (uchar*) to, (uchar*) to_end)) <= 0)
        {
          /* Extra safety - should not really happen */
          DBUG_ASSERT(false);
          return make_empty_result();
        }
        to+= rc;
        break;
      }
    }
  }
  
  /*
     last_ch is now set to the first 'double-letter' check.
     loop on input letters until end of input
  */
  for (nchars= 1 ; ; )
  {
    if ((rc= cs->cset->mb_wc(cs, &wc, (uchar*) from, (uchar*) end)) <= 0)
      break; /* EOL or invalid byte sequence */

    if (rc == 1 && cs->ctype)
    {
      if (!my_isalpha(cs, *from++))
        continue;
    }
    else
    {
      from+= rc;
      if (!my_uni_isalpha(wc))
        continue;
    }
    
    ch= get_scode(wc);
    if ((ch != '0') && (ch != last_ch)) // if not skipped or double
    {
      // letter, copy to output
      if ((rc= cs->cset->wc_mb(cs, (my_wc_t) ch,
                               (uchar*) to, (uchar*) to_end)) <= 0)
      {
        // Extra safety - should not really happen
        DBUG_ASSERT(false);
        break;
      }
      to+= rc;
      nchars++;
      last_ch= ch;  // save code of last input letter
    }               // for next double-letter check
  }
  
  /* Pad up to 4 characters with DIGIT ZERO, if the string is shorter */
  if (nchars < 4) 
  {
    uint nbytes= (4 - nchars) * cs->mbminlen;
    cs->cset->fill(cs, to, nbytes, '0');
    to+= nbytes;
  }

  tmp_value.length((uint) (to-tmp_value.ptr()));
  return &tmp_value;
}


/**
  Change a number to format '3,333,333,333.000'.

  This should be 'internationalized' sometimes.
*/

const int FORMAT_MAX_DECIMALS= 30;


MY_LOCALE *Item_func_format::get_locale(Item *item)
{
  DBUG_ASSERT(arg_count == 3);
  String tmp, *locale_name= args[2]->val_str_ascii(&tmp);
  MY_LOCALE *lc;
  if (!locale_name ||
      !(lc= my_locale_by_name(locale_name->c_ptr_safe())))
  {
    push_warning_printf(current_thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                        ER_UNKNOWN_LOCALE,
                        ER(ER_UNKNOWN_LOCALE),
                        locale_name ? locale_name->c_ptr_safe() : "NULL");
    lc= &my_locale_en_US;
  }
  return lc;
}

void Item_func_format::fix_length_and_dec()
{
  uint32 char_length= args[0]->max_char_length();
  uint32 max_sep_count= (char_length / 3) + (decimals ? 1 : 0) + /*sign*/1;
  collation.set(default_charset());
  fix_char_length(char_length + max_sep_count + decimals);
  if (arg_count == 3)
    locale= args[2]->basic_const_item() ? get_locale(args[2]) : NULL;
  else
    locale= &my_locale_en_US; /* Two arguments */
}


/**
  @todo
  This needs to be fixed for multi-byte character set where numbers
  are stored in more than one byte
*/

String *Item_func_format::val_str_ascii(String *str)
{
  uint32 str_length;
  /* Number of decimal digits */
  int dec;
  /* Number of characters used to represent the decimals, including '.' */
  uint32 dec_length;
  MY_LOCALE *lc;
  DBUG_ASSERT(fixed == 1);

  dec= (int) args[1]->val_int();
  if (args[1]->null_value)
  {
    null_value=1;
    return NULL;
  }

  lc= locale ? locale : get_locale(args[2]);

  dec= set_zone(dec, 0, FORMAT_MAX_DECIMALS);
  dec_length= dec ? dec+1 : 0;
  null_value=0;

  if (args[0]->result_type() == DECIMAL_RESULT ||
      args[0]->result_type() == INT_RESULT)
  {
    my_decimal dec_val, rnd_dec, *res;
    res= args[0]->val_decimal(&dec_val);
    if ((null_value=args[0]->null_value))
      return 0; /* purecov: inspected */
    my_decimal_round(E_DEC_FATAL_ERROR, res, dec, false, &rnd_dec);
    my_decimal2string(E_DEC_FATAL_ERROR, &rnd_dec, 0, 0, 0, str);
    str_length= str->length();
  }
  else
  {
    double nr= args[0]->val_real();
    if ((null_value=args[0]->null_value))
      return 0; /* purecov: inspected */
    nr= my_double_round(nr, (longlong) dec, FALSE, FALSE);
    str->set_real(nr, dec, &my_charset_numeric);
    if (isnan(nr) || my_isinf(nr))
      return str;
    str_length=str->length();
  }
  /* We need this test to handle 'nan' and short values */
  if (lc->grouping[0] > 0 &&
      str_length >= dec_length + 1 + lc->grouping[0])
  {
    /* We need space for ',' between each group of digits as well. */
    char buf[2 * FLOATING_POINT_BUFFER];
    int count;
    const char *grouping= lc->grouping;
    char sign_length= *str->ptr() == '-' ? 1 : 0;
    const char *src= str->ptr() + str_length - dec_length - 1;
    const char *src_begin= str->ptr() + sign_length;
    char *dst= buf + sizeof(buf);
    
    /* Put the fractional part */
    if (dec)
    {
      dst-= (dec + 1);
      *dst= lc->decimal_point;
      memcpy(dst + 1, src + 2, dec);
    }
    
    /* Put the integer part with grouping */
    for (count= *grouping; src >= src_begin; count--)
    {
      /*
        When *grouping==0x80 (which means "end of grouping")
        count will be initialized to -1 and
        we'll never get into this "if" anymore.
      */
      if (count == 0)
      {
        *--dst= lc->thousand_sep;
        if (grouping[1])
          grouping++;
        count= *grouping;
      }
      DBUG_ASSERT(dst > buf);
      *--dst= *src--;
    }
    
    if (sign_length) /* Put '-' */
      *--dst= *str->ptr();
    
    /* Put the rest of the integer part without grouping */
    str->copy(dst, buf + sizeof(buf) - dst, &my_charset_latin1);
  }
  else if (dec_length && lc->decimal_point != '.')
  {
    /*
      For short values without thousands (<1000)
      replace decimal point to localized value.
    */
    DBUG_ASSERT(dec_length <= str_length);
    ((char*) str->ptr())[str_length - dec_length]= lc->decimal_point;
  }
  return str;
}


void Item_func_format::print(String *str, enum_query_type query_type)
{
  str->append(STRING_WITH_LEN("format("));
  args[0]->print(str, query_type);
  str->append(',');
  args[1]->print(str, query_type);
  if(arg_count > 2)
  {
    str->append(',');
    args[2]->print(str,query_type);
  }
  str->append(')');
}

void Item_func_elt::fix_length_and_dec()
{
  uint32 char_length= 0;
  decimals=0;

  if (agg_arg_charsets_for_string_result(collation, args + 1, arg_count - 1))
    return;

  for (uint i= 1 ; i < arg_count ; i++)
  {
    set_if_bigger(char_length, args[i]->max_char_length());
    set_if_bigger(decimals,args[i]->decimals);
  }
  fix_char_length(char_length);
  maybe_null=1;					// NULL if wrong first arg
}


double Item_func_elt::val_real()
{
  DBUG_ASSERT(fixed == 1);
  uint tmp;
  null_value=1;
  if ((tmp=(uint) args[0]->val_int()) == 0 || tmp >= arg_count)
    return 0.0;
  double result= args[tmp]->val_real();
  null_value= args[tmp]->null_value;
  return result;
}


longlong Item_func_elt::val_int()
{
  DBUG_ASSERT(fixed == 1);
  uint tmp;
  null_value=1;
  if ((tmp=(uint) args[0]->val_int()) == 0 || tmp >= arg_count)
    return 0;

  longlong result= args[tmp]->val_int();
  null_value= args[tmp]->null_value;
  return result;
}


String *Item_func_elt::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  uint tmp;
  null_value=1;
  if ((tmp=(uint) args[0]->val_int()) == 0 || tmp >= arg_count)
    return NULL;

  String *result= args[tmp]->val_str(str);
  if (result)
    result->set_charset(collation.collation);
  null_value= args[tmp]->null_value;
  return result;
}


void Item_func_make_set::fix_length_and_dec()
{
  uint32 char_length= arg_count - 2; /* Separators */

  if (agg_arg_charsets_for_string_result(collation, args + 1, arg_count - 1))
    return;
  
  for (uint i=1 ; i < arg_count ; i++)
    char_length+= args[i]->max_char_length();
  fix_char_length(char_length);
}


String *Item_func_make_set::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  ulonglong bits;
  bool first_found=0;
  Item **ptr=args+1;
  String *result= make_empty_result();

  bits=args[0]->val_int();
  if ((null_value=args[0]->null_value))
    return NULL;

  if (arg_count < 65)
    bits &= ((ulonglong) 1 << (arg_count-1))-1;

  for (; bits; bits >>= 1, ptr++)
  {
    if (bits & 1)
    {
      String *res= (*ptr)->val_str(str);
      if (res)					// Skip nulls
      {
	if (!first_found)
	{					// First argument
	  first_found=1;
	  if (res != str)
	    result=res;				// Use original string
	  else
	  {
	    if (tmp_str.copy(*res))		// Don't use 'str'
              return make_empty_result();
	    result= &tmp_str;
	  }
	}
	else
	{
	  if (result != &tmp_str)
	  {					// Copy data to tmp_str
	    if (tmp_str.alloc(result->length()+res->length()+1) ||
		tmp_str.copy(*result))
              return make_empty_result();
	    result= &tmp_str;
	  }
	  if (tmp_str.append(STRING_WITH_LEN(","), &my_charset_bin) || tmp_str.append(*res))
            return make_empty_result();
	}
      }
    }
  }
  return result;
}


String *Item_func_char::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  str->length(0);
  str->set_charset(collation.collation);
  for (uint i=0 ; i < arg_count ; i++)
  {
    int32 num=(int32) args[i]->val_int();
    if (!args[i]->null_value)
    {
      char tmp[4];
      if (num & 0xFF000000L)
      {
        mi_int4store(tmp, num);
        str->append(tmp, 4, &my_charset_bin);
      }
      else if (num & 0xFF0000L)
      {
        mi_int3store(tmp, num);
        str->append(tmp, 3, &my_charset_bin);
      }
      else if (num & 0xFF00L)
      {
        mi_int2store(tmp, num);
        str->append(tmp, 2, &my_charset_bin);
      }
      else
      {
        tmp[0]= (char) num;
        str->append(tmp, 1, &my_charset_bin);
      }
    }
  }
  str->realloc(str->length());			// Add end 0 (for Purify)
  return check_well_formed_result(str);
}


inline String* alloc_buffer(String *res,String *str,String *tmp_value,
			    ulong length)
{
  if (res->alloced_length() < length)
  {
    if (str->alloced_length() >= length)
    {
      (void) str->copy(*res);
      str->length(length);
      return str;
    }
    if (tmp_value->alloc(length))
      return 0;
    (void) tmp_value->copy(*res);
    tmp_value->length(length);
    return tmp_value;
  }
  res->length(length);
  return res;
}


void Item_func_repeat::fix_length_and_dec()
{
  agg_arg_charsets_for_string_result(collation, args, 1);
  DBUG_ASSERT(collation.collation != NULL);
  if (args[1]->const_item())
  {
    /* must be longlong to avoid truncation */
    longlong count= args[1]->val_int();

    /* Assumes that the maximum length of a String is < INT_MAX32. */
    /* Set here so that rest of code sees out-of-bound value as such. */
    if (args[1]->null_value)
      count= 0;
    else if (count > INT_MAX32)
      count= INT_MAX32;

    ulonglong char_length= (ulonglong) args[0]->max_char_length() * count;
    fix_char_length_ulonglong(char_length);
  }
  else
  {
    max_length= MAX_BLOB_WIDTH;
    maybe_null= 1;
  }
}

/**
  Item_func_repeat::str is carefully written to avoid reallocs
  as much as possible at the cost of a local buffer
*/

String *Item_func_repeat::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  uint length,tot_length;
  char *to;
  /* must be longlong to avoid truncation */
  longlong count= args[1]->val_int();
  String *res= args[0]->val_str(str);

  if (args[0]->null_value || args[1]->null_value)
    goto err;				// string and/or delim are null
  null_value= 0;

  if (count <= 0 && (count == 0 || !args[1]->unsigned_flag))
    return make_empty_result();

  /* Assumes that the maximum length of a String is < INT_MAX32. */
  /* Bounds check on count:  If this is triggered, we will error. */
  if ((ulonglong) count > INT_MAX32)
    count= INT_MAX32;
  if (count == 1)			// To avoid reallocs
    return res;
  length=res->length();
  // Safe length check
  if (length > current_thd->variables.max_allowed_packet / (uint) count)
  {
    push_warning_printf(current_thd, MYSQL_ERROR::WARN_LEVEL_WARN,
			ER_WARN_ALLOWED_PACKET_OVERFLOWED,
			ER(ER_WARN_ALLOWED_PACKET_OVERFLOWED),
			func_name(), current_thd->variables.max_allowed_packet);
    goto err;
  }
  tot_length= length*(uint) count;
  if (!(res= alloc_buffer(res,str,&tmp_value,tot_length)))
    goto err;

  to=(char*) res->ptr()+length;
  while (--count)
  {
    memcpy(to,res->ptr(),length);
    to+=length;
  }
  return (res);

err:
  null_value=1;
  return 0;
}


void Item_func_rpad::fix_length_and_dec()
{
  // Handle character set for args[0] and args[2].
  if (agg_arg_charsets_for_string_result(collation, &args[0], 2, 2))
    return;
  if (args[1]->const_item())
  {
    ulonglong char_length= (ulonglong) args[1]->val_int();
    DBUG_ASSERT(collation.collation->mbmaxlen > 0);
    /* Assumes that the maximum length of a String is < INT_MAX32. */
    /* Set here so that rest of code sees out-of-bound value as such. */
    if (args[1]->null_value)
      char_length= 0;
    else if (char_length > INT_MAX32)
      char_length= INT_MAX32;
    fix_char_length_ulonglong(char_length);
  }
  else
  {
    max_length= MAX_BLOB_WIDTH;
    maybe_null= 1;
  }
}


String *Item_func_rpad::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  uint32 res_byte_length,res_char_length,pad_char_length,pad_byte_length;
  char *to;
  const char *ptr_pad;
  /* must be longlong to avoid truncation */
  longlong count= args[1]->val_int();
  longlong byte_count;
  String *res= args[0]->val_str(str);
  String *rpad= args[2]->val_str(&rpad_str);

  if (!res || args[1]->null_value || !rpad || 
      ((count < 0) && !args[1]->unsigned_flag))
    goto err;
  null_value=0;
  /* Assumes that the maximum length of a String is < INT_MAX32. */
  /* Set here so that rest of code sees out-of-bound value as such. */
  if ((ulonglong) count > INT_MAX32)
    count= INT_MAX32;
  /*
    There is one exception not handled (intentionaly) by the character set
    aggregation code. If one string is strong side and is binary, and
    another one is weak side and is a multi-byte character string,
    then we need to operate on the second string in terms on bytes when
    calling ::numchars() and ::charpos(), rather than in terms of characters.
    Lets substitute its character set to binary.
  */
  if (collation.collation == &my_charset_bin)
  {
    res->set_charset(&my_charset_bin);
    rpad->set_charset(&my_charset_bin);
  }
#if MARIADB_VERSION_ID < 1000000
  /*
    Well-formedness is handled on a higher level in 10.0,
    no needs to check it here again.
  */
  else
  {
    // This will chop off any trailing illegal characters from rpad.
    String *well_formed_pad= args[2]->check_well_formed_result(rpad, false);
    if (!well_formed_pad)
      goto err;
  }
#endif

  if (count <= (res_char_length= res->numchars()))
  {						// String to pad is big enough
    res->length(res->charpos((int) count));	// Shorten result if longer
    return (res);
  }
  pad_char_length= rpad->numchars();

  byte_count= count * collation.collation->mbmaxlen;
  if ((ulonglong) byte_count > current_thd->variables.max_allowed_packet)
  {
    push_warning_printf(current_thd, MYSQL_ERROR::WARN_LEVEL_WARN,
			ER_WARN_ALLOWED_PACKET_OVERFLOWED,
			ER(ER_WARN_ALLOWED_PACKET_OVERFLOWED),
			func_name(), current_thd->variables.max_allowed_packet);
    goto err;
  }
  if (args[2]->null_value || !pad_char_length)
    goto err;
  res_byte_length= res->length();	/* Must be done before alloc_buffer */
  if (!(res= alloc_buffer(res,str,&tmp_value, (ulong) byte_count)))
    goto err;

  to= (char*) res->ptr()+res_byte_length;
  ptr_pad=rpad->ptr();
  pad_byte_length= rpad->length();
  count-= res_char_length;
  for ( ; (uint32) count > pad_char_length; count-= pad_char_length)
  {
    memcpy(to,ptr_pad,pad_byte_length);
    to+= pad_byte_length;
  }
  if (count)
  {
    pad_byte_length= rpad->charpos((int) count);
    memcpy(to,ptr_pad,(size_t) pad_byte_length);
    to+= pad_byte_length;
  }
  res->length((uint) (to- (char*) res->ptr()));
  return (res);

 err:
  null_value=1;
  return 0;
}


void Item_func_lpad::fix_length_and_dec()
{
  // Handle character set for args[0] and args[2].
  if (agg_arg_charsets_for_string_result(collation, &args[0], 2, 2))
    return;
  
  if (args[1]->const_item())
  {
    ulonglong char_length= (ulonglong) args[1]->val_int();
    DBUG_ASSERT(collation.collation->mbmaxlen > 0);
    /* Assumes that the maximum length of a String is < INT_MAX32. */
    /* Set here so that rest of code sees out-of-bound value as such. */
    if (args[1]->null_value)
      char_length= 0;
    else if (char_length > INT_MAX32)
      char_length= INT_MAX32;
    fix_char_length_ulonglong(char_length);
  }
  else
  {
    max_length= MAX_BLOB_WIDTH;
    maybe_null= 1;
  }
}


String *Item_func_lpad::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  uint32 res_char_length,pad_char_length;
  /* must be longlong to avoid truncation */
  longlong count= args[1]->val_int();
  longlong byte_count;
  String *res= args[0]->val_str(&tmp_value);
  String *pad= args[2]->val_str(&lpad_str);

  if (!res || args[1]->null_value || !pad ||  
      ((count < 0) && !args[1]->unsigned_flag))
    goto err;  
  null_value=0;
  /* Assumes that the maximum length of a String is < INT_MAX32. */
  /* Set here so that rest of code sees out-of-bound value as such. */
  if ((ulonglong) count > INT_MAX32)
    count= INT_MAX32;

  /*
    There is one exception not handled (intentionaly) by the character set
    aggregation code. If one string is strong side and is binary, and
    another one is weak side and is a multi-byte character string,
    then we need to operate on the second string in terms on bytes when
    calling ::numchars() and ::charpos(), rather than in terms of characters.
    Lets substitute its character set to binary.
  */
  if (collation.collation == &my_charset_bin)
  {
    res->set_charset(&my_charset_bin);
    pad->set_charset(&my_charset_bin);
  }
#if MARIADB_VERSION_ID < 1000000
  /*
    Well-formedness is handled on a higher level in 10.0,
    no needs to check it here again.
  */  else
  {
    // This will chop off any trailing illegal characters from pad.
    String *well_formed_pad= args[2]->check_well_formed_result(pad, false);
    if (!well_formed_pad)
      goto err;
  }
#endif

  res_char_length= res->numchars();

  if (count <= res_char_length)
  {
    res->length(res->charpos((int) count));
    return res;
  }
  
  pad_char_length= pad->numchars();
  byte_count= count * collation.collation->mbmaxlen;
  
  if ((ulonglong) byte_count > current_thd->variables.max_allowed_packet)
  {
    push_warning_printf(current_thd, MYSQL_ERROR::WARN_LEVEL_WARN,
			ER_WARN_ALLOWED_PACKET_OVERFLOWED,
			ER(ER_WARN_ALLOWED_PACKET_OVERFLOWED),
			func_name(), current_thd->variables.max_allowed_packet);
    goto err;
  }

  if (args[2]->null_value || !pad_char_length ||
      str->alloc((uint32) byte_count))
    goto err;
  
  str->length(0);
  str->set_charset(collation.collation);
  count-= res_char_length;
  while (count >= pad_char_length)
  {
    str->append(*pad);
    count-= pad_char_length;
  }
  if (count > 0)
    str->append(pad->ptr(), pad->charpos((int) count), collation.collation);

  str->append(*res);
  null_value= 0;
  return str;

err:
  null_value= 1;
  return 0;
}


String *Item_func_conv::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  String *res= args[0]->val_str(str);
  char *endptr,ans[65],*ptr;
  longlong dec;
  int from_base= (int) args[1]->val_int();
  int to_base= (int) args[2]->val_int();
  int err;

  // Note that abs(INT_MIN) is undefined.
  if (args[0]->null_value || args[1]->null_value || args[2]->null_value ||
      from_base == INT_MIN || to_base == INT_MIN ||
      abs(to_base) > 36 || abs(to_base) < 2 ||
      abs(from_base) > 36 || abs(from_base) < 2 || !(res->length()))
  {
    null_value= 1;
    return NULL;
  }
  null_value= 0;
  unsigned_flag= !(from_base < 0);

  if (args[0]->field_type() == MYSQL_TYPE_BIT) 
  {
    /* 
     Special case: The string representation of BIT doesn't resemble the
     decimal representation, so we shouldn't change it to string and then to
     decimal. 
    */
    dec= args[0]->val_int();
  }
  else
  {
    if (from_base < 0)
      dec= my_strntoll(res->charset(), res->ptr(), res->length(),
                       -from_base, &endptr, &err);
    else
      dec= (longlong) my_strntoull(res->charset(), res->ptr(), res->length(),
                                   from_base, &endptr, &err);
  }

  if (!(ptr= longlong2str(dec, ans, to_base)) ||
      str->copy(ans, (uint32) (ptr - ans), default_charset()))
  {
    null_value= 1;
    return NULL;
  }
  return str;
}


String *Item_func_conv_charset::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  if (use_cached_value)
    return null_value ? 0 : &str_value;
  String *arg= args[0]->val_str(str);
  uint dummy_errors;
  if (args[0]->null_value)
  {
    null_value=1;
    return 0;
  }
  null_value= tmp_value.copy(arg->ptr(), arg->length(), arg->charset(),
                             conv_charset, &dummy_errors);
  return null_value ? 0 : check_well_formed_result(&tmp_value);
}

void Item_func_conv_charset::fix_length_and_dec()
{
  collation.set(conv_charset, DERIVATION_IMPLICIT);
  fix_char_length(args[0]->max_char_length());
}

void Item_func_conv_charset::print(String *str, enum_query_type query_type)
{
  str->append(STRING_WITH_LEN("convert("));
  args[0]->print(str, query_type);
  str->append(STRING_WITH_LEN(" using "));
  str->append(conv_charset->csname);
  str->append(')');
}

String *Item_func_set_collation::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  str=args[0]->val_str(str);
  if ((null_value=args[0]->null_value))
    return 0;
  str->set_charset(collation.collation);
  return str;
}

void Item_func_set_collation::fix_length_and_dec()
{
  CHARSET_INFO *set_collation;
  const char *colname;
  String tmp, *str= args[1]->val_str(&tmp);
  colname= str->c_ptr();
  if (colname == binary_keyword)
    set_collation= get_charset_by_csname(args[0]->collation.collation->csname,
					 MY_CS_BINSORT,MYF(0));
  else
  {
    if (!(set_collation= get_charset_by_name(colname,MYF(0))))
    {
      my_error(ER_UNKNOWN_COLLATION, MYF(0), colname);
      return;
    }
  }

  if (!set_collation || 
      !my_charset_same(args[0]->collation.collation,set_collation))
  {
    my_error(ER_COLLATION_CHARSET_MISMATCH, MYF(0),
             colname, args[0]->collation.collation->csname);
    return;
  }
  collation.set(set_collation, DERIVATION_EXPLICIT,
                args[0]->collation.repertoire);
  max_length= args[0]->max_length;
}


bool Item_func_set_collation::eq(const Item *item, bool binary_cmp) const
{
  /* Assume we don't have rtti */
  if (this == item)
    return 1;
  if (item->type() != FUNC_ITEM)
    return 0;
  Item_func *item_func=(Item_func*) item;
  if (arg_count != item_func->arg_count ||
      functype() != item_func->functype())
    return 0;
  Item_func_set_collation *item_func_sc=(Item_func_set_collation*) item;
  if (collation.collation != item_func_sc->collation.collation)
    return 0;
  for (uint i=0; i < arg_count ; i++)
    if (!args[i]->eq(item_func_sc->args[i], binary_cmp))
      return 0;
  return 1;
}


void Item_func_set_collation::print(String *str, enum_query_type query_type)
{
  str->append('(');
  args[0]->print(str, query_type);
  str->append(STRING_WITH_LEN(" collate "));
  DBUG_ASSERT(args[1]->basic_const_item() &&
              args[1]->type() == Item::STRING_ITEM);
  args[1]->str_value.print(str);
  str->append(')');
}

String *Item_func_charset::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  uint dummy_errors;

  CHARSET_INFO *cs= args[0]->charset_for_protocol(); 
  null_value= 0;
  str->copy(cs->csname, (uint) strlen(cs->csname),
	    &my_charset_latin1, collation.collation, &dummy_errors);
  return str;
}

String *Item_func_collation::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  uint dummy_errors;
  CHARSET_INFO *cs= args[0]->charset_for_protocol(); 

  null_value= 0;
  str->copy(cs->name, (uint) strlen(cs->name),
	    &my_charset_latin1, collation.collation, &dummy_errors);
  return str;
}


String *Item_func_hex::val_str_ascii(String *str)
{
  String *res;
  DBUG_ASSERT(fixed == 1);
  if (args[0]->result_type() != STRING_RESULT)
  {
    ulonglong dec;
    char ans[65],*ptr;
    /* Return hex of unsigned longlong value */
    if (args[0]->result_type() == REAL_RESULT ||
        args[0]->result_type() == DECIMAL_RESULT)
    {
      double val= args[0]->val_real();
      if ((val <= (double) LONGLONG_MIN) || 
          (val >= (double) (ulonglong) ULONGLONG_MAX))
        dec=  ~(longlong) 0;
      else
        dec= (ulonglong) (val + (val > 0 ? 0.5 : -0.5));
    }
    else
      dec= (ulonglong) args[0]->val_int();

    if ((null_value= args[0]->null_value))
      return 0;
    
    if (!(ptr= longlong2str(dec, ans, 16)) ||
        str->copy(ans,(uint32) (ptr - ans),
        &my_charset_numeric))
      return make_empty_result();		// End of memory
    return str;
  }

  /* Convert given string to a hex string, character by character */
  res= args[0]->val_str(str);
  if (!res || tmp_value.alloc(res->length()*2+1))
  {
    null_value=1;
    return 0;
  }
  null_value=0;
  tmp_value.length(res->length()*2);
  tmp_value.set_charset(&my_charset_latin1);

  octet2hex((char*) tmp_value.ptr(), res->ptr(), res->length());
  return &tmp_value;
}

  /** Convert given hex string to a binary string. */

String *Item_func_unhex::val_str(String *str)
{
  const char *from, *end;
  char *to;
  String *res;
  uint length;
  DBUG_ASSERT(fixed == 1);

  res= args[0]->val_str(str);
  if (!res || tmp_value.alloc(length= (1+res->length())/2))
  {
    null_value=1;
    return 0;
  }

  from= res->ptr();
  null_value= 0;
  tmp_value.length(length);
  to= (char*) tmp_value.ptr();
  if (res->length() % 2)
  {
    int hex_char;
    *to++= hex_char= hexchar_to_int(*from++);
    if ((null_value= (hex_char == -1)))
      return 0;
  }
  for (end=res->ptr()+res->length(); from < end ; from+=2, to++)
  {
    int hex_char;
    *to= (hex_char= hexchar_to_int(from[0])) << 4;
    if ((null_value= (hex_char == -1)))
      return 0;
    *to|= hex_char= hexchar_to_int(from[1]);
    if ((null_value= (hex_char == -1)))
      return 0;
  }
  return &tmp_value;
}


#ifndef DBUG_OFF
String *Item_func_like_range::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  longlong nbytes= args[1]->val_int();
  String *res= args[0]->val_str(str);
  size_t min_len, max_len;
  CHARSET_INFO *cs= collation.collation;

  if (!res || args[0]->null_value || args[1]->null_value ||
      nbytes < 0 || nbytes > MAX_BLOB_WIDTH ||
      min_str.alloc(nbytes) || max_str.alloc(nbytes))
    goto err;
  null_value=0;

  if (cs->coll->like_range(cs, res->ptr(), res->length(),
                           '\\', '_', '%', nbytes,
                           (char*) min_str.ptr(), (char*) max_str.ptr(),
                           &min_len, &max_len))
    goto err;

  min_str.set_charset(collation.collation);
  max_str.set_charset(collation.collation);
  min_str.length(min_len);
  max_str.length(max_len);

  return is_min ? &min_str : &max_str;

err:
  null_value= 1;
  return 0;
}
#endif


void Item_func_binary::print(String *str, enum_query_type query_type)
{
  str->append(STRING_WITH_LEN("cast("));
  args[0]->print(str, query_type);
  str->append(STRING_WITH_LEN(" as binary)"));
}


#include <my_dir.h>				// For my_stat

String *Item_load_file::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  String *file_name;
  File file;
  MY_STAT stat_info;
  char path[FN_REFLEN];
  DBUG_ENTER("load_file");

  if (!(file_name= args[0]->val_str(str))
#ifndef NO_EMBEDDED_ACCESS_CHECKS
      || !(current_thd->security_ctx->master_access & FILE_ACL)
#endif
      )
    goto err;

  (void) fn_format(path, file_name->c_ptr_safe(), mysql_real_data_home, "",
		   MY_RELATIVE_PATH | MY_UNPACK_FILENAME);

  /* Read only allowed from within dir specified by secure_file_priv */
  if (!is_secure_file_path(path))
    goto err;

  if (!mysql_file_stat(key_file_loadfile, path, &stat_info, MYF(0)))
    goto err;

  if (!(stat_info.st_mode & S_IROTH))
  {
    /* my_error(ER_TEXTFILE_NOT_READABLE, MYF(0), file_name->c_ptr()); */
    goto err;
  }
  if (stat_info.st_size > (long) current_thd->variables.max_allowed_packet)
  {
    push_warning_printf(current_thd, MYSQL_ERROR::WARN_LEVEL_WARN,
			ER_WARN_ALLOWED_PACKET_OVERFLOWED,
			ER(ER_WARN_ALLOWED_PACKET_OVERFLOWED),
			func_name(), current_thd->variables.max_allowed_packet);
    goto err;
  }
  if (tmp_value.alloc((size_t)stat_info.st_size))
    goto err;
  if ((file= mysql_file_open(key_file_loadfile,
                             file_name->ptr(), O_RDONLY, MYF(0))) < 0)
    goto err;
  if (mysql_file_read(file, (uchar*) tmp_value.ptr(), stat_info.st_size,
                      MYF(MY_NABP)))
  {
    mysql_file_close(file, MYF(0));
    goto err;
  }
  tmp_value.length((uint32)stat_info.st_size);
  mysql_file_close(file, MYF(0));
  null_value = 0;
  DBUG_RETURN(&tmp_value);

err:
  null_value = 1;
  DBUG_RETURN(0);
}


String* Item_func_export_set::val_str(String* str)
{
  DBUG_ASSERT(fixed == 1);
  String yes_buf, no_buf, sep_buf;
  const ulonglong the_set = (ulonglong) args[0]->val_int();
  const String *yes= args[1]->val_str(&yes_buf);
  const String *no= args[2]->val_str(&no_buf);
  const String *sep= NULL;

  uint num_set_values = 64;
  str->length(0);
  str->set_charset(collation.collation);

  /* Check if some argument is a NULL value */
  if (args[0]->null_value || args[1]->null_value || args[2]->null_value)
  {
    null_value= true;
    return NULL;
  }
  /*
    Arg count can only be 3, 4 or 5 here. This is guaranteed from the
    grammar for EXPORT_SET()
  */
  switch(arg_count) {
  case 5:
    num_set_values = (uint) args[4]->val_int();
    if (num_set_values > 64)
      num_set_values=64;
    if (args[4]->null_value)
    {
      null_value= true;
      return NULL;
    }
    /* Fall through */
  case 4:
    if (!(sep = args[3]->val_str(&sep_buf)))	// Only true if NULL
    {
      null_value= true;
      return NULL;
    }
    break;
  case 3:
    {
      /* errors is not checked - assume "," can always be converted */
      uint errors;
      sep_buf.copy(STRING_WITH_LEN(","), &my_charset_bin,
                   collation.collation, &errors);
      sep = &sep_buf;
    }
    break;
  default:
    DBUG_ASSERT(0); // cannot happen
  }
  null_value= false;

  const ulong max_allowed_packet= current_thd->variables.max_allowed_packet;
  const uint num_separators= num_set_values > 0 ? num_set_values - 1 : 0;
  const ulonglong max_total_length=
    num_set_values * max(yes->length(), no->length()) +
    num_separators * sep->length();

  if (unlikely(max_total_length > max_allowed_packet))
  {
    push_warning_printf(current_thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                        ER_WARN_ALLOWED_PACKET_OVERFLOWED,
                        ER(ER_WARN_ALLOWED_PACKET_OVERFLOWED),
                        func_name(), max_allowed_packet);
    null_value= true;
    return NULL;
  }

  uint ix;
  ulonglong mask;
  for (ix= 0, mask=0x1; ix < num_set_values; ++ix, mask = (mask << 1))
  {
    if (the_set & mask)
      str->append(*yes);
    else
      str->append(*no);
    if (ix != num_separators)
      str->append(*sep);
  }
  return str;
}

void Item_func_export_set::fix_length_and_dec()
{
  uint32 length= max(args[1]->max_char_length(), args[2]->max_char_length());
  uint32 sep_length= (arg_count > 3 ? args[3]->max_char_length() : 1);

  if (agg_arg_charsets_for_string_result(collation,
                                         args + 1, min(4, arg_count) - 1))
    return;
  fix_char_length(length * 64 + sep_length * 63);
}

String* Item_func_inet_ntoa::val_str(String* str)
{
  DBUG_ASSERT(fixed == 1);
  uchar buf[8], *p;
  ulonglong n = (ulonglong) args[0]->val_int();
  char num[4];

  /*
    We do not know if args[0] is NULL until we have called
    some val function on it if args[0] is not a constant!

    Also return null if n > 255.255.255.255
  */
  if ((null_value= (args[0]->null_value || n > (ulonglong) LL(4294967295))))
    return 0;					// Null value

  str->set_charset(collation.collation);
  str->length(0);
  int4store(buf,n);

  /* Now we can assume little endian. */

  num[3]='.';
  for (p=buf+4 ; p-- > buf ; )
  {
    uint c = *p;
    uint n1,n2;					// Try to avoid divisions
    n1= c / 100;				// 100 digits
    c-= n1*100;
    n2= c / 10;					// 10 digits
    c-=n2*10;					// last digit
    num[0]=(char) n1+'0';
    num[1]=(char) n2+'0';
    num[2]=(char) c+'0';
    uint length= (n1 ? 4 : n2 ? 3 : 2);         // Remove pre-zero
    uint dot_length= (p <= buf) ? 1 : 0;
    (void) str->append(num + 4 - length, length - dot_length,
                       &my_charset_latin1);
  }
  return str;
}


#define get_esc_bit(mask, num) (1 & (*((mask) + ((num) >> 3))) >> ((num) & 7))

/**
  QUOTE() function returns argument string in single quotes suitable for
  using in a SQL statement.

  Adds a \\ before all characters that needs to be escaped in a SQL string.
  We also escape '^Z' (END-OF-FILE in windows) to avoid probelms when
  running commands from a file in windows.

  This function is very useful when you want to generate SQL statements.

  @note
    QUOTE(NULL) returns the string 'NULL' (4 letters, without quotes).

  @retval
    str	   Quoted string
  @retval
    NULL	   Out of memory.
*/

String *Item_func_quote::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  /*
    Bit mask that has 1 for set for the position of the following characters:
    0, \, ' and ^Z
  */

  static uchar escmask[32]=
  {
    0x01, 0x00, 0x00, 0x04, 0x80, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
  };

  char *from, *to, *end, *start;
  String *arg= args[0]->val_str(str);
  uint arg_length, new_length;
  if (!arg)					// Null argument
  {
    /* Return the string 'NULL' */
    str->copy(STRING_WITH_LEN("NULL"), collation.collation);
    null_value= 0;
    return str;
  }

  arg_length= arg->length();

  if (collation.collation->mbmaxlen == 1)
  {
    new_length= arg_length + 2; /* for beginning and ending ' signs */
    for (from= (char*) arg->ptr(), end= from + arg_length; from < end; from++)
      new_length+= get_esc_bit(escmask, (uchar) *from);
  }
  else
  {
    new_length= (arg_length * 2) +  /* For string characters */
                (2 * collation.collation->mbmaxlen); /* For quotes */
  }

  if (tmp_value.alloc(new_length))
    goto null;

  if (collation.collation->mbmaxlen > 1)
  {
    CHARSET_INFO *cs= collation.collation;
    int mblen;
    uchar *to_end;
    to= (char*) tmp_value.ptr();
    to_end= (uchar*) to + new_length;

    /* Put leading quote */
    if ((mblen= cs->cset->wc_mb(cs, '\'', (uchar *) to, to_end)) <= 0)
      goto null;
    to+= mblen;

    for (start= (char*) arg->ptr(), end= start + arg_length; start < end; )
    {
      my_wc_t wc;
      bool escape;
      if ((mblen= cs->cset->mb_wc(cs, &wc, (uchar*) start, (uchar*) end)) <= 0)
        goto null;
      start+= mblen;
      switch (wc) {
        case 0:      escape= 1; wc= '0'; break;
        case '\032': escape= 1; wc= 'Z'; break;
        case '\'':   escape= 1; break;
        case '\\':   escape= 1; break;
        default:     escape= 0; break;
      }
      if (escape)
      {
        if ((mblen= cs->cset->wc_mb(cs, '\\', (uchar*) to, to_end)) <= 0)
          goto null;
        to+= mblen;
      }
      if ((mblen= cs->cset->wc_mb(cs, wc, (uchar*) to, to_end)) <= 0)
        goto null;
      to+= mblen;
    }

    /* Put trailing quote */
    if ((mblen= cs->cset->wc_mb(cs, '\'', (uchar *) to, to_end)) <= 0)
      goto null;
    to+= mblen;
    new_length= to - tmp_value.ptr();
    goto ret;
  }

  /*
    We replace characters from the end to the beginning
  */
  to= (char*) tmp_value.ptr() + new_length - 1;
  *to--= '\'';
  for (start= (char*) arg->ptr(),end= start + arg_length; end-- != start; to--)
  {
    /*
      We can't use the bitmask here as we want to replace \O and ^Z with 0
      and Z
    */
    switch (*end)  {
    case 0:
      *to--= '0';
      *to=   '\\';
      break;
    case '\032':
      *to--= 'Z';
      *to=   '\\';
      break;
    case '\'':
    case '\\':
      *to--= *end;
      *to=   '\\';
      break;
    default:
      *to= *end;
      break;
    }
  }
  *to= '\'';

ret:
  tmp_value.length(new_length);
  tmp_value.set_charset(collation.collation);
  null_value= 0;
  return &tmp_value;

null:
  null_value= 1;
  return 0;
}

longlong Item_func_uncompressed_length::val_int()
{
  DBUG_ASSERT(fixed == 1);
  String *res= args[0]->val_str(&value);
  if (!res)
  {
    null_value=1;
    return 0; /* purecov: inspected */
  }
  null_value=0;
  if (res->is_empty()) return 0;

  /*
    If length is <= 4 bytes, data is corrupt. This is the best we can do
    to detect garbage input without decompressing it.
  */
  if (res->length() <= 4)
  {
    push_warning_printf(current_thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                        ER_ZLIB_Z_DATA_ERROR,
                        ER(ER_ZLIB_Z_DATA_ERROR));
    null_value= 1;
    return 0;
  }

 /*
    res->ptr() using is safe because we have tested that string is at least
    5 bytes long.
    res->c_ptr() is not used because:
      - we do not need \0 terminated string to get first 4 bytes
      - c_ptr() tests simbol after string end (uninitialiozed memory) which
        confuse valgrind
  */
  return uint4korr(res->ptr()) & 0x3FFFFFFF;
}

longlong Item_func_crc32::val_int()
{
  DBUG_ASSERT(fixed == 1);
  String *res=args[0]->val_str(&value);
  if (!res)
  {
    null_value=1;
    return 0; /* purecov: inspected */
  }
  null_value=0;
  return (longlong) crc32(0L, (uchar*)res->ptr(), res->length());
}

#ifdef HAVE_COMPRESS
#include "zlib.h"

String *Item_func_compress::val_str(String *str)
{
  int err= Z_OK, code;
  size_t new_size;
  String *res;
  Byte *body;
  char *tmp, *last_char;
  DBUG_ASSERT(fixed == 1);

  if (!(res= args[0]->val_str(str)))
  {
    null_value= 1;
    return 0;
  }
  null_value= 0;
  if (res->is_empty()) return res;

  /*
    Citation from zlib.h (comment for compress function):

    Compresses the source buffer into the destination buffer.  sourceLen is
    the byte length of the source buffer. Upon entry, destLen is the total
    size of the destination buffer, which must be at least 0.1% larger than
    sourceLen plus 12 bytes.
    We assume here that the buffer can't grow more than .25 %.
  */
  new_size= res->length() + res->length() / 5 + 12;

  // Check new_size overflow: new_size <= res->length()
  if (((uint32) (new_size+5) <= res->length()) || 
      buffer.realloc((uint32) new_size + 4 + 1))
  {
    null_value= 1;
    return 0;
  }

  body= ((Byte*)buffer.ptr()) + 4;

  // As far as we have checked res->is_empty() we can use ptr()
  if ((err= my_compress_buffer(body, &new_size, (const uchar *)res->ptr(),
                               res->length())) != Z_OK)
  {
    code= err==Z_MEM_ERROR ? ER_ZLIB_Z_MEM_ERROR : ER_ZLIB_Z_BUF_ERROR;
    push_warning(current_thd,MYSQL_ERROR::WARN_LEVEL_WARN,code,ER(code));
    null_value= 1;
    return 0;
  }

  tmp= (char*)buffer.ptr(); // int4store is a macro; avoid side effects
  int4store(tmp, res->length() & 0x3FFFFFFF);

  /* This is to ensure that things works for CHAR fields, which trim ' ': */
  last_char= ((char*)body)+new_size-1;
  if (*last_char == ' ')
  {
    *++last_char= '.';
    new_size++;
  }

  buffer.length((uint32)new_size + 4);
  return &buffer;
}


String *Item_func_uncompress::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  String *res= args[0]->val_str(str);
  ulong new_size;
  int err;
  uint code;

  if (!res)
    goto err;
  null_value= 0;
  if (res->is_empty())
    return res;

  /* If length is less than 4 bytes, data is corrupt */
  if (res->length() <= 4)
  {
    push_warning_printf(current_thd,MYSQL_ERROR::WARN_LEVEL_WARN,
			ER_ZLIB_Z_DATA_ERROR,
			ER(ER_ZLIB_Z_DATA_ERROR));
    goto err;
  }

  /* Size of uncompressed data is stored as first 4 bytes of field */
  new_size= uint4korr(res->ptr()) & 0x3FFFFFFF;
  if (new_size > current_thd->variables.max_allowed_packet)
  {
    push_warning_printf(current_thd,MYSQL_ERROR::WARN_LEVEL_WARN,
			ER_TOO_BIG_FOR_UNCOMPRESS,
			ER(ER_TOO_BIG_FOR_UNCOMPRESS),
                        static_cast<int>(current_thd->variables.
                                         max_allowed_packet));
    goto err;
  }
  if (buffer.realloc((uint32)new_size))
    goto err;

  if ((err= uncompress((Byte*)buffer.ptr(), &new_size,
		       ((const Bytef*)res->ptr())+4,res->length()-4)) == Z_OK)
  {
    buffer.length((uint32) new_size);
    return &buffer;
  }

  code= ((err == Z_BUF_ERROR) ? ER_ZLIB_Z_BUF_ERROR :
	 ((err == Z_MEM_ERROR) ? ER_ZLIB_Z_MEM_ERROR : ER_ZLIB_Z_DATA_ERROR));
  push_warning(current_thd,MYSQL_ERROR::WARN_LEVEL_WARN,code,ER(code));

err:
  null_value= 1;
  return 0;
}
#endif


String *Item_func_uuid::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  uchar guid[MY_UUID_SIZE];

  str->realloc(MY_UUID_STRING_LENGTH+1);
  str->length(MY_UUID_STRING_LENGTH);
  str->set_charset(system_charset_info);
  my_uuid(guid);
  my_uuid2str(guid, (char *)str->ptr());

  return str;
}


Item_func_dyncol_create::Item_func_dyncol_create(List<Item> &args,
                                                 DYNCALL_CREATE_DEF *dfs)
  : Item_str_func(args), defs(dfs), vals(0), nums(0)
{
  DBUG_ASSERT((args.elements & 0x1) == 0); // even number of arguments
}


bool Item_func_dyncol_create::fix_fields(THD *thd, Item **ref)
{
  bool res= Item_func::fix_fields(thd, ref); // no need Item_str_func here
  vals= (DYNAMIC_COLUMN_VALUE *) alloc_root(thd->mem_root,
                                            sizeof(DYNAMIC_COLUMN_VALUE) *
                                            (arg_count / 2));
  nums= (uint *) alloc_root(thd->mem_root,
                            sizeof(uint) * (arg_count / 2));
  status_var_increment(thd->status_var.feature_dynamic_columns);
  return res || vals == 0 || nums == 0;
}


void Item_func_dyncol_create::fix_length_and_dec()
{
  maybe_null= TRUE;
  collation.set(&my_charset_bin);
  decimals= 0;
}

void Item_func_dyncol_create::prepare_arguments()
{
  char buff[STRING_BUFFER_USUAL_SIZE];
  String *res, tmp(buff, sizeof(buff), &my_charset_bin);
  uint column_count= (arg_count / 2);
  uint i;
  my_decimal dtmp, *dres;

  /* get values */
  for (i= 0; i < column_count; i++)
  {
    uint valpos= i * 2 + 1;
    DYNAMIC_COLUMN_TYPE type= defs[i].type;
    if (type == DYN_COL_NULL) // auto detect
    {
      /*
        We don't have a default here to ensure we get a warning if
        one adds a new not handled MYSQL_TYPE_...
      */
      switch (args[valpos]->field_type()) {
      case MYSQL_TYPE_DECIMAL:
      case MYSQL_TYPE_NEWDECIMAL:
        type= DYN_COL_DECIMAL;
        break;
      case MYSQL_TYPE_TINY:
      case MYSQL_TYPE_SHORT:
      case MYSQL_TYPE_LONG:
      case MYSQL_TYPE_LONGLONG:
      case MYSQL_TYPE_INT24:
      case MYSQL_TYPE_YEAR:
      case MYSQL_TYPE_BIT:
        type= args[valpos]->unsigned_flag ? DYN_COL_UINT : DYN_COL_INT;
        break;
      case MYSQL_TYPE_FLOAT:
      case MYSQL_TYPE_DOUBLE:
        type= DYN_COL_DOUBLE;
        break;
      case MYSQL_TYPE_NULL:
        type= DYN_COL_NULL;
        break;
      case MYSQL_TYPE_TIMESTAMP:
      case MYSQL_TYPE_DATETIME:
        type= DYN_COL_DATETIME;
	break;
      case MYSQL_TYPE_DATE:
      case MYSQL_TYPE_NEWDATE:
        type= DYN_COL_DATE;
        break;
      case MYSQL_TYPE_TIME:
        type= DYN_COL_TIME;
        break;
      case MYSQL_TYPE_VARCHAR:
      case MYSQL_TYPE_ENUM:
      case MYSQL_TYPE_SET:
      case MYSQL_TYPE_TINY_BLOB:
      case MYSQL_TYPE_MEDIUM_BLOB:
      case MYSQL_TYPE_LONG_BLOB:
      case MYSQL_TYPE_BLOB:
      case MYSQL_TYPE_VAR_STRING:
      case MYSQL_TYPE_STRING:
      case MYSQL_TYPE_GEOMETRY:
        type= DYN_COL_STRING;
        break;
      }
    }
    nums[i]= (uint) args[i * 2]->val_int();
    vals[i].type= type;
    switch (type) {
    case DYN_COL_NULL:
      DBUG_ASSERT(args[valpos]->field_type() == MYSQL_TYPE_NULL);
      break;
    case DYN_COL_INT:
      vals[i].x.long_value= args[valpos]->val_int();
      break;
    case DYN_COL_UINT:
      vals[i].x.ulong_value= args[valpos]->val_int();
      break;
    case DYN_COL_DOUBLE:
      vals[i].x.double_value= args[valpos]->val_real();
      break;
    case DYN_COL_STRING:
      res= args[valpos]->val_str(&tmp);
      if (res &&
          (vals[i].x.string.value.str= my_strndup(res->ptr(), res->length(),
                                                MYF(MY_WME))))
      {
	vals[i].x.string.value.length= res->length();
	vals[i].x.string.charset= res->charset();
      }
      else
      {
        args[valpos]->null_value= 1;            // In case of out of memory
        vals[i].x.string.value.str= NULL;
        vals[i].x.string.value.length= 0;         // just to be safe
      }
      break;
    case DYN_COL_DECIMAL:
      if ((dres= args[valpos]->val_decimal(&dtmp)))
      {
	dynamic_column_prepare_decimal(&vals[i]);
        DBUG_ASSERT(vals[i].x.decimal.value.len == dres->len);
        vals[i].x.decimal.value.intg= dres->intg;
        vals[i].x.decimal.value.frac= dres->frac;
        vals[i].x.decimal.value.sign= dres->sign();
        memcpy(vals[i].x.decimal.buffer, dres->buf,
               sizeof(vals[i].x.decimal.buffer));
      }
      else
      {
	dynamic_column_prepare_decimal(&vals[i]); // just to be safe
        DBUG_ASSERT(args[valpos]->null_value);
      }
      break;
    case DYN_COL_DATETIME:
      args[valpos]->get_date(&vals[i].x.time_value, 0);
      break;
    case DYN_COL_DATE:
      args[valpos]->get_date(&vals[i].x.time_value, 0);
      break;
    case DYN_COL_TIME:
      args[valpos]->get_time(&vals[i].x.time_value);
      break;
    default:
      DBUG_ASSERT(0);
      vals[i].type= DYN_COL_NULL;
    }
    if (vals[i].type != DYN_COL_NULL && args[valpos]->null_value)
    {
      if (vals[i].type == DYN_COL_STRING)
        my_free(vals[i].x.string.value.str);
      vals[i].type= DYN_COL_NULL;
    }
  }
}

void Item_func_dyncol_create::cleanup_arguments()
{
  uint column_count= (arg_count / 2);
  uint i;

  for (i= 0; i < column_count; i++)
  {
    if (vals[i].type == DYN_COL_STRING)
      my_free(vals[i].x.string.value.str);
  }
}

String *Item_func_dyncol_create::val_str(String *str)
{
  DYNAMIC_COLUMN col;
  String *res;
  uint column_count= (arg_count / 2);
  enum enum_dyncol_func_result rc;
  DBUG_ASSERT((arg_count & 0x1) == 0); // even number of arguments

  prepare_arguments();

  if ((rc= dynamic_column_create_many(&col, column_count, nums, vals)))
  {
    dynamic_column_error_message(rc);
    dynamic_column_column_free(&col);
    res= NULL;
    null_value= TRUE;
  }
  else
  {
    /* Move result from DYNAMIC_COLUMN to str_value */
    char *ptr;
    size_t length, alloc_length;
    dynamic_column_reassociate(&col, &ptr, &length, &alloc_length);
    str_value.reassociate(ptr, (uint32) length, (uint32) alloc_length,
                          &my_charset_bin);
    res= &str_value;
    null_value= FALSE;
  }

  /* cleanup */
  cleanup_arguments();

  return res;
}

void Item_func_dyncol_create::print_arguments(String *str,
                                              enum_query_type query_type)
{
  uint i;
  uint column_count= (arg_count / 2);
  for (i= 0; i < column_count; i++)
  {
    args[i*2]->print(str, query_type);
    str->append(',');
    args[i*2 + 1]->print(str, query_type);
    switch (defs[i].type) {
    case DYN_COL_NULL: // automatic type => write nothing
      break;
    case DYN_COL_INT:
      str->append(STRING_WITH_LEN(" AS int"));
      break;
    case DYN_COL_UINT:
      str->append(STRING_WITH_LEN(" AS unsigned int"));
      break;
    case DYN_COL_DOUBLE:
      str->append(STRING_WITH_LEN(" AS double"));
      break;
    case DYN_COL_STRING:
      str->append(STRING_WITH_LEN(" AS char"));
      if (defs[i].cs)
      {
        str->append(STRING_WITH_LEN(" charset "));
        str->append(defs[i].cs->csname);
        str->append(' ');
      }
      break;
    case DYN_COL_DECIMAL:
      str->append(STRING_WITH_LEN(" AS decimal"));
      break;
    case DYN_COL_DATETIME:
      str->append(STRING_WITH_LEN(" AS datetime"));
      break;
    case DYN_COL_DATE:
      str->append(STRING_WITH_LEN(" AS date"));
      break;
    case DYN_COL_TIME:
      str->append(STRING_WITH_LEN(" AS time"));
      break;
    }
    if (i < column_count - 1)
      str->append(',');
  }
}


void Item_func_dyncol_create::print(String *str,
                                    enum_query_type query_type)
{
  DBUG_ASSERT((arg_count & 0x1) == 0); // even number of arguments
  str->append(STRING_WITH_LEN("column_create("));
  print_arguments(str, query_type);
  str->append(')');
}


String *Item_func_dyncol_add::val_str(String *str)
{
  DYNAMIC_COLUMN col;
  String *res;
  uint column_count=  (arg_count / 2);
  enum enum_dyncol_func_result rc;
  DBUG_ASSERT((arg_count & 0x1) == 1); // odd number of arguments

  /* We store the packed data last */
  res= args[arg_count - 1]->val_str(str);
  if (args[arg_count - 1]->null_value)
    goto null;
  init_dynamic_string(&col, NULL, res->length() + STRING_BUFFER_USUAL_SIZE,
                      STRING_BUFFER_USUAL_SIZE);

  col.length= res->length();
  memcpy(col.str, res->ptr(), col.length);

  prepare_arguments();

  if ((rc= dynamic_column_update_many(&col, column_count, nums, vals)))
  {
    dynamic_column_error_message(rc);
    dynamic_column_column_free(&col);
    cleanup_arguments();
    goto null;
  }

  {
    /* Move result from DYNAMIC_COLUMN to str */
    char *ptr;
    size_t length, alloc_length;
    dynamic_column_reassociate(&col, &ptr, &length, &alloc_length);
    str->reassociate(ptr, (uint32) length, (uint32) alloc_length,
                     &my_charset_bin);
    null_value= FALSE;
  }

  /* cleanup */
  dynamic_column_column_free(&col);
  cleanup_arguments();

  return str;

null:
  null_value= TRUE;
  return NULL;
}


void Item_func_dyncol_add::print(String *str,
                                 enum_query_type query_type)
{
  DBUG_ASSERT((arg_count & 0x1) == 1); // odd number of arguments
  str->append(STRING_WITH_LEN("column_create("));
  args[arg_count - 1]->print(str, query_type);
  str->append(',');
  print_arguments(str, query_type);
  str->append(')');
}


/**
  Get value for a column stored in a dynamic column

  @notes
  This function ensures that null_value is set correctly
*/

bool Item_dyncol_get::get_dyn_value(DYNAMIC_COLUMN_VALUE *val, String *tmp)
{
  DYNAMIC_COLUMN dyn_str;
  String *res;
  longlong num;
  enum enum_dyncol_func_result rc;

  num= args[1]->val_int();
  if (args[1]->null_value || num < 0 || num > INT_MAX)
  {
    null_value= 1;
    return 1;
  }

  res= args[0]->val_str(tmp);
  if (args[0]->null_value)
  {
    null_value= 1;
    return 1;
  }

  dyn_str.str=   (char*) res->ptr();
  dyn_str.length= res->length();
  if ((rc= dynamic_column_get(&dyn_str, (uint) num, val)))
  {
    dynamic_column_error_message(rc);
    null_value= 1;
    return 1;
  }

  null_value= 0;
  return 0;                                     // ok
}


String *Item_dyncol_get::val_str(String *str_result)
{
  DYNAMIC_COLUMN_VALUE val;
  char buff[STRING_BUFFER_USUAL_SIZE];
  String tmp(buff, sizeof(buff), &my_charset_bin);

  if (get_dyn_value(&val, &tmp))
    return NULL;

  switch (val.type) {
  case DYN_COL_NULL:
    goto null;
  case DYN_COL_INT:
  case DYN_COL_UINT:
    str_result->set_int(val.x.long_value, test(val.type == DYN_COL_UINT),
                       &my_charset_latin1);
    break;
  case DYN_COL_DOUBLE:
    str_result->set_real(val.x.double_value, NOT_FIXED_DEC, &my_charset_latin1);
    break;
  case DYN_COL_STRING:
    if ((char*) tmp.ptr() <= val.x.string.value.str &&
        (char*) tmp.ptr() + tmp.length() >= val.x.string.value.str)
    {
      /* value is allocated in tmp buffer; We have to make a copy */
      str_result->copy(val.x.string.value.str, val.x.string.value.length,
                      val.x.string.charset);
    }
    else
    {
      /*
        It's safe to use the current value because it's either pointing
        into a field or in a buffer for another item and this buffer
        is not going to be deleted during expression evaluation
      */
      str_result->set(val.x.string.value.str, val.x.string.value.length,
                      val.x.string.charset);
    }
    break;
  case DYN_COL_DECIMAL:
  {
    int res;
    int length= decimal_string_size(&val.x.decimal.value);
    if (str_result->alloc(length))
      goto null;
    if ((res= decimal2string(&val.x.decimal.value, (char*) str_result->ptr(),
                             &length, 0, 0, ' ')) != E_DEC_OK)
    {
      char buff[40];
      int len= sizeof(buff);
      DBUG_ASSERT(length < (int)sizeof(buff));
      decimal2string(&val.x.decimal.value, buff, &len, 0, 0, ' ');
      decimal_operation_results(res, buff, "CHAR");
    }
    str_result->set_charset(&my_charset_latin1);
    str_result->length(length);
    break;
  }
  case DYN_COL_DATETIME:
  case DYN_COL_DATE:
  case DYN_COL_TIME:
  {
    int length;
    /*
      We use AUTO_SEC_PART_DIGITS here to ensure that we do not loose
      any microseconds from the data. This is safe to do as we are
      asked to return the time argument as a string.
    */
    if (str_result->alloc(MAX_DATE_STRING_REP_LENGTH) ||
        !(length= my_TIME_to_str(&val.x.time_value, (char*) str_result->ptr(),
                                 AUTO_SEC_PART_DIGITS)))
      goto null;
    str_result->set_charset(&my_charset_latin1);
    str_result->length(length);
    break;
  }
  }
  return str_result;

null:
  null_value= TRUE;
  return 0;
}


longlong Item_dyncol_get::val_int()
{
  DYNAMIC_COLUMN_VALUE val;
  char buff[STRING_BUFFER_USUAL_SIZE];
  String tmp(buff, sizeof(buff), &my_charset_bin);

  if (get_dyn_value(&val, &tmp))
    return 0;

  switch (val.type) {
  case DYN_COL_NULL:
    goto null;
  case DYN_COL_UINT:
    unsigned_flag= 1;            // Make it possible for caller to detect sign
    return val.x.long_value;
  case DYN_COL_INT:
    unsigned_flag= 0;            // Make it possible for caller to detect sign
    return val.x.long_value;
  case DYN_COL_DOUBLE:
  {
    bool error;
    longlong num;

    num= double_to_longlong(val.x.double_value, unsigned_flag, &error);
    if (error)
    {
      char buff[30];
      sprintf(buff, "%lg", val.x.double_value);
      push_warning_printf(current_thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                          ER_DATA_OVERFLOW,
                          ER(ER_DATA_OVERFLOW),
                          buff,
                          unsigned_flag ? "UNSIGNED INT" : "INT");
    }
    return num;
  }
  case DYN_COL_STRING:
  {
    int error;
    longlong num;
    char *end= val.x.string.value.str + val.x.string.value.length, *org_end= end;

    num= my_strtoll10(val.x.string.value.str, &end, &error);
    if (end != org_end || error > 0)
    {
      char buff[80];
      strmake(buff, val.x.string.value.str, min(sizeof(buff)-1,
                                              val.x.string.value.length));
      push_warning_printf(current_thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                          ER_BAD_DATA,
                          ER(ER_BAD_DATA),
                          buff,
                          unsigned_flag ? "UNSIGNED INT" : "INT");
    }
    unsigned_flag= error >= 0;
    return num;
  }
  case DYN_COL_DECIMAL:
  {
    longlong num;
    my_decimal2int(E_DEC_FATAL_ERROR, &val.x.decimal.value, unsigned_flag,
                   &num);
    return num;
  }
  case DYN_COL_DATETIME:
  case DYN_COL_DATE:
  case DYN_COL_TIME:
    unsigned_flag= !val.x.time_value.neg;
    if (unsigned_flag)
      return TIME_to_ulonglong(&val.x.time_value);
    else
      return -(longlong)TIME_to_ulonglong(&val.x.time_value);
  }

null:
  null_value= TRUE;
  return 0;
}


double Item_dyncol_get::val_real()
{
  DYNAMIC_COLUMN_VALUE val;
  char buff[STRING_BUFFER_USUAL_SIZE];
  String tmp(buff, sizeof(buff), &my_charset_bin);

  if (get_dyn_value(&val, &tmp))
    return 0.0;

  switch (val.type) {
  case DYN_COL_NULL:
    goto null;
  case DYN_COL_UINT:
    return ulonglong2double(val.x.ulong_value);
  case DYN_COL_INT:
    return (double) val.x.long_value;
  case DYN_COL_DOUBLE:
    return (double) val.x.double_value;
  case DYN_COL_STRING:
  {
    int error;
    char *end;
    double res= my_strntod(val.x.string.charset, (char*) val.x.string.value.str,
                           val.x.string.value.length, &end, &error);

    if (end != (char*) val.x.string.value.str + val.x.string.value.length ||
        error)
    {
      char buff[80];
      strmake(buff, val.x.string.value.str, min(sizeof(buff)-1,
                                              val.x.string.value.length));
      push_warning_printf(current_thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                          ER_BAD_DATA,
                          ER(ER_BAD_DATA),
                          buff, "DOUBLE");
    }
    return res;
  }
  case DYN_COL_DECIMAL:
  {
    double res;
    /* This will always succeed */
    decimal2double(&val.x.decimal.value, &res);
    return res;
  }
  case DYN_COL_DATETIME:
  case DYN_COL_DATE:
  case DYN_COL_TIME:
    return TIME_to_double(&val.x.time_value);
  }

null:
  null_value= TRUE;
  return 0.0;
}


my_decimal *Item_dyncol_get::val_decimal(my_decimal *decimal_value)
{
  DYNAMIC_COLUMN_VALUE val;
  char buff[STRING_BUFFER_USUAL_SIZE];
  String tmp(buff, sizeof(buff), &my_charset_bin);

  if (get_dyn_value(&val, &tmp))
    return NULL;

  switch (val.type) {
  case DYN_COL_NULL:
    goto null;
  case DYN_COL_UINT:
    int2my_decimal(E_DEC_FATAL_ERROR, val.x.long_value, TRUE, decimal_value);
    break;
  case DYN_COL_INT:
    int2my_decimal(E_DEC_FATAL_ERROR, val.x.long_value, FALSE, decimal_value);
    break;
  case DYN_COL_DOUBLE:
    double2my_decimal(E_DEC_FATAL_ERROR, val.x.double_value, decimal_value);
    break;
  case DYN_COL_STRING:
  {
    int rc;
    rc= str2my_decimal(0, val.x.string.value.str, val.x.string.value.length,
                       val.x.string.charset, decimal_value);
    char buff[80];
    strmake(buff, val.x.string.value.str, min(sizeof(buff)-1,
                                            val.x.string.value.length));
    if (rc != E_DEC_OK)
    {
      push_warning_printf(current_thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                          ER_BAD_DATA,
                          ER(ER_BAD_DATA),
                          buff, "DECIMAL");
    }
    break;
  }
  case DYN_COL_DECIMAL:
    decimal2my_decimal(&val.x.decimal.value, decimal_value);
    break;
  case DYN_COL_DATETIME:
  case DYN_COL_DATE:
  case DYN_COL_TIME:
    decimal_value= seconds2my_decimal(val.x.time_value.neg,
                                      TIME_to_ulonglong(&val.x.time_value),
                                      val.x.time_value.second_part,
                                      decimal_value);
    break;
  }
  return decimal_value;

null:
  null_value= TRUE;
  return 0;
}


bool Item_dyncol_get::get_date(MYSQL_TIME *ltime, ulonglong fuzzy_date)
{
  DYNAMIC_COLUMN_VALUE val;
  char buff[STRING_BUFFER_USUAL_SIZE];
  String tmp(buff, sizeof(buff), &my_charset_bin);
  bool signed_value= 0;

  if (get_dyn_value(&val, &tmp))
    return 1;                                   // Error

  switch (val.type) {
  case DYN_COL_NULL:
    goto null;
  case DYN_COL_INT:
    signed_value= 1;                                  // For error message
    /* fall_trough */
  case DYN_COL_UINT:
    if (signed_value || val.x.ulong_value <= LONGLONG_MAX)
    {
      bool neg= val.x.ulong_value > LONGLONG_MAX;
      if (int_to_datetime_with_warn(neg, neg ? -val.x.ulong_value :
                                                val.x.ulong_value,
                                    ltime, fuzzy_date, 0 /* TODO */))
        goto null;
      return 0;
    }
    /* let double_to_datetime_with_warn() issue the warning message */
    val.x.double_value= static_cast<double>(ULONGLONG_MAX);
    /* fall_trough */
  case DYN_COL_DOUBLE:
    if (double_to_datetime_with_warn(val.x.double_value, ltime, fuzzy_date,
                                     0 /* TODO */))
      goto null;
    return 0;
  case DYN_COL_DECIMAL:
    if (decimal_to_datetime_with_warn((my_decimal*)&val.x.decimal.value, ltime,
                                      fuzzy_date, 0 /* TODO */))
      goto null;
    return 0;
  case DYN_COL_STRING:
    if (str_to_datetime_with_warn(&my_charset_numeric,
                                  val.x.string.value.str,
                                  val.x.string.value.length,
                                  ltime, fuzzy_date) <= MYSQL_TIMESTAMP_ERROR)
      goto null;
    return 0;
  case DYN_COL_DATETIME:
  case DYN_COL_DATE:
  case DYN_COL_TIME:
    *ltime= val.x.time_value;
    return 0;
  }

null:
  null_value= TRUE;
  return 1;
}


void Item_dyncol_get::print(String *str, enum_query_type query_type)
{
  /*
    Parent cast doesn't exist yet, only print dynamic column name. This happens
    when called from create_func_cast() / wrong_precision_error().
  */
  if (!str->length())
  {
    args[1]->print(str, query_type);
    return;
  }

  /* see create_func_dyncol_get */
  DBUG_ASSERT(str->length() >= 5);
  DBUG_ASSERT(strncmp(str->ptr() + str->length() - 5, "cast(", 5) == 0);

  str->length(str->length() - 5);    // removing "cast("
  str->append(STRING_WITH_LEN("column_get("));
  args[0]->print(str, query_type);
  str->append(',');
  args[1]->print(str, query_type);
  /* let the parent cast item add " as <type>)" */
}


String *Item_func_dyncol_list::val_str(String *str)
{
  uint i;
  enum enum_dyncol_func_result rc;
  DYNAMIC_ARRAY arr;
  DYNAMIC_COLUMN col;
  String *res= args[0]->val_str(str);

  if (args[0]->null_value)
    goto null;
  col.length= res->length();
  /* We do not change the string, so could do this trick */
  col.str= (char *)res->ptr();
  if ((rc= dynamic_column_list(&col, &arr)))
  {
    dynamic_column_error_message(rc);
    delete_dynamic(&arr);
    goto null;
  }

  /*
    We support elements from 0 - 65536, so max size for one element is
    6 (including ,).
  */
  if (str->alloc(arr.elements * 6))
    goto null;

  str->length(0);
  for (i= 0; i < arr.elements; i++)
  {
    str->qs_append(*dynamic_element(&arr, i, uint*));
    if (i < arr.elements - 1)
      str->qs_append(',');
  }

  null_value= FALSE;
  delete_dynamic(&arr);
  return str;

null:
  null_value= TRUE;
  return NULL;
}
