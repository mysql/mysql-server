/*
   Copyright (c) 2000, 2018, Oracle and/or its affiliates. All rights reserved.

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

/* May include caustic 3rd-party defs. Use early, so it can override nothing. */
#include "sha2.h"

#include "item_strfunc.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_CRYPT_H
#include <crypt.h>
#endif

#include "base64.h"                  // base64_encode_max_arg_length
#include "my_aes.h"                  // MY_AES_IV_SIZE
#include "my_md5.h"                  // MD5_HASH_SIZE
#include "my_rnd.h"                  // my_rand_buffer
#include "sha1.h"                    // SHA1_HASH_SIZE
#include "auth_common.h"             // check_password_policy
#include "des_key_file.h"            // st_des_keyblock
#include "item_geofunc.h"            // Item_func_geomfromgeojson
#include "password.h"                // my_make_scrambled_password
#include "sql_class.h"               // THD
#include "strfunc.h"                 // hexchar_to_int

C_MODE_START
#include "../mysys/my_static.h"			// For soundex_map
C_MODE_END

#include "template_utils.h"

#include "pfs_file_provider.h"
#include "mysql/psi/mysql_file.h"

using std::min;
using std::max;


/*
  For the Items which have only val_str_ascii() method
  and don't have their own "native" val_str(),
  we provide a "wrapper" method to convert from ASCII
  to Item character set when it's necessary.
  Conversion happens only in case of "tricky" Item character set (e.g. UCS2).
  Normally conversion does not happen, and val_str_ascii() is immediately
  returned instead.
*/
String *Item_str_func::val_str_from_val_str_ascii(String *str, String *str2)
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

bool Item_str_func::fix_fields(THD *thd, Item **ref)
{
  bool res= Item_func::fix_fields(thd, ref);
  /*
    In Item_str_func::check_well_formed_result() we may set null_value
    flag on the same condition as in test() below.
  */
  maybe_null= (maybe_null || thd->is_strict_mode());
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
  str->set_charset(&my_charset_bin);
  if (sptr)
  {
    uchar digest[MD5_HASH_SIZE];

    null_value=0;
    compute_md5_hash((char *) digest, sptr->ptr(), sptr->length());
    if (str->alloc(32))				// Ensure that memory is free
    {
      null_value=1;
      return 0;
    }
    array_to_hex((char *) str->ptr(), digest, MD5_HASH_SIZE);
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
  str->set_charset(&my_charset_bin);
  if (sptr)  /* If we got value different from NULL */
  {
    /* Temporary buffer to store 160bit digest */
    uint8 digest[SHA1_HASH_SIZE];
    compute_sha1_hash(digest, sptr->ptr(), sptr->length());
    /* Ensure that memory is free */
    if (!(str->alloc(SHA1_HASH_SIZE * 2)))
    {
      array_to_hex((char *) str->ptr(), digest, SHA1_HASH_SIZE);
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

/*
  SHA2(str, hash_length)
  The second argument indicates the desired bit length of the
  result, which must have a value of 224, 256, 384, 512, or 0 
  (which is equivalent to 256).
*/
String *Item_func_sha2::val_str_ascii(String *str)
{
  DBUG_ASSERT(fixed == 1);
#if defined(HAVE_OPENSSL) && !defined(EMBEDDED_LIBRARY)
  unsigned char digest_buf[SHA512_DIGEST_LENGTH];
  uint digest_length= 0;

  String *input_string= args[0]->val_str(str);
  str->set_charset(&my_charset_bin);

  if (input_string == NULL)
  {
    null_value= TRUE;
    return (String *) NULL;
  }

  null_value= args[0]->null_value;
  if (null_value)
    return NULL;

  const unsigned char *input_ptr=
    pointer_cast<const unsigned char*>(input_string->ptr());
  size_t input_len= input_string->length();

  longlong hash_length= args[1]->val_int();
  null_value= args[1]->null_value;
  // Give error message in switch below.
  if (null_value)
    hash_length= -1;

  switch (hash_length) {
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
    // For const values we have already warned in fix_length_and_dec.
    if (!args[1]->const_item())
      push_warning_printf(current_thd,
        Sql_condition::SL_WARNING,
        ER_WRONG_PARAMETERS_TO_NATIVE_FCT,
        ER(ER_WRONG_PARAMETERS_TO_NATIVE_FCT), "sha2");
    null_value= TRUE;
    return NULL;
  }

  /* 
    Since we're subverting the usual String methods, we must make sure that
    the destination has space for the bytes we're about to write.
  */
  str->mem_realloc(digest_length*2 + 1); /* Each byte as two nybbles */

  /* Convert the large number to a string-hex representation. */
  array_to_hex((char *) str->ptr(), digest_buf, digest_length);

  /* We poked raw bytes in.  We must inform the the String of its length. */
  str->length(digest_length*2); /* Each byte as two nybbles */

  null_value= FALSE;
  return str;

#else
  push_warning_printf(current_thd,
    Sql_condition::SL_WARNING,
    ER_FEATURE_DISABLED,
    ER(ER_FEATURE_DISABLED),
    "sha2", "--with-ssl");
  null_value= TRUE;
  return (String *) NULL;
#endif /* defined(HAVE_OPENSSL) && !defined(EMBEDDED_LIBRARY) */
}


void Item_func_sha2::fix_length_and_dec()
{
  maybe_null = 1;
  max_length = 0;

#if defined(HAVE_OPENSSL) && !defined(EMBEDDED_LIBRARY)
  longlong sha_variant;
  if (args[1]->const_item())
  {
    sha_variant= args[1]->val_int();
    // Give error message in switch below.
    if (args[1]->null_value)
      sha_variant= -1;
  }
  else
  {
    sha_variant= 512;
  }

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
    fix_length_and_charset(SHA256_DIGEST_LENGTH * 2, default_charset());
    push_warning_printf(current_thd,
      Sql_condition::SL_WARNING,
      ER_WRONG_PARAMETERS_TO_NATIVE_FCT,
      ER(ER_WRONG_PARAMETERS_TO_NATIVE_FCT), "sha2");
  }

  CHARSET_INFO *cs= get_checksum_charset(args[0]->collation.collation->csname);
  args[0]->collation.set(cs, DERIVATION_COERCIBLE);

#else
  push_warning_printf(current_thd,
    Sql_condition::SL_WARNING,
    ER_FEATURE_DISABLED,
    ER(ER_FEATURE_DISABLED),
    "sha2", "--with-ssl");
#endif /* defined(HAVE_OPENSSL) && !defined(EMBEDDED_LIBRARY) */
}

/* Implementation of AES encryption routines */

/** helper class to process an IV argument to aes_encrypt/aes_decrypt */
class iv_argument
{
  char iv_buff[MY_AES_IV_SIZE + 1];  // +1 to cater for the terminating NULL
  String tmp_iv_value;
public:
  iv_argument() :
    tmp_iv_value(iv_buff, sizeof(iv_buff), system_charset_info)
  {}

  /**
    Validate the arguments and retrieve the IV value.

    Processes a 3d optional IV argument to an Item_func function.
    Contains all the necessary stack buffers etc.

    @param aes_opmode  the encryption mode
    @param arg_count   number of parameters passed to the function
    @param args        array of arguments passed to the function
    @param func_name   the name of the function (for errors)
    @param thd         the current thread (for errors)
    @param [out] error_generated  set to true if error was generated.

    @return a pointer to the retrived validated IV or NULL
  */
  const unsigned char *retrieve_iv_ptr(enum my_aes_opmode aes_opmode,
                                       uint arg_count,
                                       Item **args,
                                       const char *func_name,
                                       THD *thd,
                                       my_bool *error_generated)
  {
    const unsigned char *iv_str= NULL;

    *error_generated= FALSE;

    if (my_aes_needs_iv(aes_opmode))
    {
      /* we only enforce the need for IV */
      if (arg_count == 3)
      {
        String *iv= args[2]->val_str(&tmp_iv_value);
        if (!iv || iv->length() < MY_AES_IV_SIZE)
        {
          my_error(ER_AES_INVALID_IV, MYF(0), func_name, (long long) MY_AES_IV_SIZE);
          *error_generated= TRUE;
          return NULL;
        }
        iv_str= (unsigned char *) iv->ptr();
      }
      else
      {
        my_error(ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT, MYF(0), func_name);
        *error_generated= TRUE;
        return NULL;
      }
    }
    else
    {
      if (arg_count == 3)
      {
        push_warning_printf(thd, Sql_condition::SL_WARNING,
                            WARN_OPTION_IGNORED,
                            ER(WARN_OPTION_IGNORED), "IV");
      }
    }
    return iv_str;
  }
};


bool Item_func_aes_encrypt::itemize(Parse_context *pc, Item **res)
{
  if (skip_itemize(res))
    return false;
  if (super::itemize(pc, res))
    return true;
  /* Unsafe for SBR since result depends on a session variable */
  pc->thd->lex->set_stmt_unsafe(LEX::BINLOG_STMT_UNSAFE_SYSTEM_FUNCTION);
  /* Not safe to cache either */
  pc->thd->lex->set_uncacheable(pc->select, UNCACHEABLE_SIDEEFFECT);
  return false;
}


String *Item_func_aes_encrypt::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  char key_buff[80];
  String tmp_key_value(key_buff, sizeof(key_buff), system_charset_info);
  String *sptr, *key;
  int aes_length;
  THD *thd= current_thd;
  ulong aes_opmode;
  iv_argument iv_arg;
  DBUG_ENTER("Item_func_aes_encrypt::val_str");

  sptr= args[0]->val_str(str);			// String to encrypt
  key=  args[1]->val_str(&tmp_key_value);	// key
  aes_opmode= thd->variables.my_aes_mode;

  DBUG_ASSERT(aes_opmode <= MY_AES_END);

  if (sptr && key) // we need both arguments to be not NULL
  {
    const unsigned char *iv_str= 
      iv_arg.retrieve_iv_ptr((enum my_aes_opmode) aes_opmode, arg_count, args,
                             func_name(), thd, &null_value);
    if (null_value)
      DBUG_RETURN(NULL);

    // Calculate result length
    aes_length= my_aes_get_size(sptr->length(),
                                (enum my_aes_opmode) aes_opmode);

    str_value.set_charset(&my_charset_bin);
    if (!str_value.alloc(aes_length))		// Ensure that memory is free
    {
      // finally encrypt directly to allocated buffer.
      if (my_aes_encrypt((unsigned char *) sptr->ptr(), sptr->length(),
                         (unsigned char *) str_value.ptr(),
                         (unsigned char *) key->ptr(), key->length(),
                         (enum my_aes_opmode) aes_opmode,
                         iv_str) == aes_length)
      {
	// We got the expected result length
	str_value.length((uint) aes_length);
        DBUG_RETURN(&str_value);
      }
    }
  }
  null_value=1;
  DBUG_RETURN(0);
}


void Item_func_aes_encrypt::fix_length_and_dec()
{
  ulong aes_opmode= current_thd->variables.my_aes_mode;
  DBUG_ASSERT(aes_opmode <= MY_AES_END);

  max_length=my_aes_get_size(args[0]->max_length,
                             (enum my_aes_opmode) aes_opmode);
}


bool Item_func_aes_decrypt::itemize(Parse_context *pc, Item **res)
{
  if (skip_itemize(res))
    return false;
  if (super::itemize(pc, res))
    return true;
  /* Unsafe for SBR since result depends on a session variable */
  pc->thd->lex->set_stmt_unsafe(LEX::BINLOG_STMT_UNSAFE_SYSTEM_FUNCTION);
  /* Not safe to cache either */
  pc->thd->lex->set_uncacheable(pc->select, UNCACHEABLE_SIDEEFFECT);
  return false;
}


String *Item_func_aes_decrypt::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  char key_buff[80];
  String tmp_key_value(key_buff, sizeof(key_buff), system_charset_info);
  String *sptr, *key;
  THD *thd= current_thd;
  ulong aes_opmode;
  iv_argument iv_arg;
  DBUG_ENTER("Item_func_aes_decrypt::val_str");

  sptr= args[0]->val_str(str);			// String to decrypt
  key=  args[1]->val_str(&tmp_key_value);	// Key
  aes_opmode= thd->variables.my_aes_mode;
  DBUG_ASSERT(aes_opmode <= MY_AES_END);

  if (sptr && key)  			// Need to have both arguments not NULL
  {
    const unsigned char *iv_str=
      iv_arg.retrieve_iv_ptr((enum my_aes_opmode) aes_opmode, arg_count, args,
      func_name(), thd, &null_value);
    if (null_value)
      DBUG_RETURN(NULL);
    str_value.set_charset(&my_charset_bin);
    if (!str_value.alloc(sptr->length()))  // Ensure that memory is free
    {
      // finally decrypt directly to allocated buffer.
      int length;
      length= my_aes_decrypt((unsigned char *) sptr->ptr(), sptr->length(),
                             (unsigned char *) str_value.ptr(),
                             (unsigned char *) key->ptr(), key->length(),
                             (enum my_aes_opmode) aes_opmode, iv_str);
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


bool Item_func_random_bytes::itemize(Parse_context *pc, Item **res)
{
  if (skip_itemize(res))
    return false;
  if (super::itemize(pc, res))
    return true;

  /* it is unsafe for SBR since it uses crypto random from the ssl library */
  pc->thd->lex->set_stmt_unsafe(LEX::BINLOG_STMT_UNSAFE_SYSTEM_FUNCTION);
  /* Not safe to cache either */
  pc->thd->lex->set_uncacheable(pc->select, UNCACHEABLE_RAND);
  return false;
}


/*
  Artificially limited to 1k to avoid excessive memory usage.
  The SSL lib supports up to INT_MAX.
*/
const longlong Item_func_random_bytes::MAX_RANDOM_BYTES_BUFFER= 1024;


void Item_func_random_bytes::fix_length_and_dec()
{
  collation.set(&my_charset_bin);
  max_length= MAX_RANDOM_BYTES_BUFFER;
}


String *Item_func_random_bytes::val_str(String *a)
{
  DBUG_ASSERT(fixed == 1);
  longlong n_bytes= args[0]->val_int();
  null_value= args[0]->null_value;

  if (null_value)
    return NULL;

  str_value.set_charset(&my_charset_bin);

  if (n_bytes <= 0 || n_bytes > MAX_RANDOM_BYTES_BUFFER)
  {
    my_error(ER_DATA_OUT_OF_RANGE, MYF(0), "length", func_name());
    null_value= TRUE;
    return NULL;
  }

  if (str_value.alloc(n_bytes))
  {
    my_error(ER_OUTOFMEMORY, n_bytes);
    null_value= TRUE;
    return NULL;
  }

  str_value.set_charset(&my_charset_bin);

  if (my_rand_buffer((unsigned char *) str_value.ptr(), n_bytes))
  {
    my_error(ER_ERROR_WHEN_EXECUTING_COMMAND, MYF(0), func_name(),
             "SSL library can't generate random bytes");
    null_value= TRUE;
    return NULL;
  }

  str_value.length(n_bytes);
  return &str_value;
}


void Item_func_to_base64::fix_length_and_dec()
{
  maybe_null= args[0]->maybe_null;
  collation.set(default_charset(), DERIVATION_COERCIBLE, MY_REPERTOIRE_ASCII);
  if (args[0]->max_length > (uint) base64_encode_max_arg_length())
  {
    maybe_null= 1;
    fix_char_length_ulonglong((ulonglong) base64_encode_max_arg_length());
  }
  else
  {
    uint64 length= base64_needed_encoded_length((uint64) args[0]->max_length);
    DBUG_ASSERT(length > 0);
    fix_char_length_ulonglong((ulonglong) length - 1);
  }
}


String *Item_func_to_base64::val_str_ascii(String *str)
{
  String *res= args[0]->val_str(str);
  bool too_long= false;
  uint64 length;
  if (!res ||
      res->length() > (uint) base64_encode_max_arg_length() ||
      (too_long=
       ((length= base64_needed_encoded_length((uint64) res->length())) >
        current_thd->variables.max_allowed_packet)) ||
      tmp_value.alloc((uint) length))
  {
    null_value= 1; // NULL input, too long input, or OOM.
    if (too_long)
    {
      push_warning_printf(current_thd, Sql_condition::SL_WARNING,
                          ER_WARN_ALLOWED_PACKET_OVERFLOWED,
                          ER(ER_WARN_ALLOWED_PACKET_OVERFLOWED), func_name(),
                          current_thd->variables.max_allowed_packet);
    }
    return 0;
  }
  base64_encode(res->ptr(), (int) res->length(), (char*) tmp_value.ptr());
  DBUG_ASSERT(length > 0);
  tmp_value.length((uint) length - 1); // Without trailing '\0'
  null_value= 0;
  return &tmp_value;
}


void Item_func_from_base64::fix_length_and_dec()
{
  if (args[0]->max_length > (uint) base64_decode_max_arg_length())
  {
    fix_char_length_ulonglong((ulonglong) base64_decode_max_arg_length());
  }
  else
  {
    uint64 length= base64_needed_decoded_length((uint64) args[0]->max_length);
    fix_char_length_ulonglong((ulonglong) length);
  }
  maybe_null= 1; // Can be NULL, e.g. in case of badly formed input string
}


String *Item_func_from_base64::val_str(String *str)
{
  String *res= args[0]->val_str_ascii(str);
  bool too_long= false;
  int64 length;
  const char *end_ptr;

  if (!res ||
      res->length() > (uint) base64_decode_max_arg_length() ||
      (too_long=
       ((uint64) (length= base64_needed_decoded_length((uint64) res->length())) >
        current_thd->variables.max_allowed_packet)) ||
      tmp_value.alloc((uint) length) ||
      (length= base64_decode(res->ptr(), (uint64) res->length(),
                             (char *) tmp_value.ptr(), &end_ptr, 0)) < 0 ||
      end_ptr < res->ptr() + res->length())
  {
    null_value= 1; // NULL input, too long input, OOM, or badly formed input
    if (too_long)
    {
      push_warning_printf(current_thd, Sql_condition::SL_WARNING,
                          ER_WARN_ALLOWED_PACKET_OVERFLOWED,
                          ER(ER_WARN_ALLOWED_PACKET_OVERFLOWED), func_name(),
                          current_thd->variables.max_allowed_packet);
    }
    return 0;
  }
  tmp_value.length((uint) length);
  null_value= 0;
  return &tmp_value;
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
	push_warning_printf(current_thd, Sql_condition::SL_WARNING,
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
          // If res2 is a substring of str, then clone it first.
          char buff[STRING_BUFFER_USUAL_SIZE];
          String res2_clone(buff, sizeof(buff), system_charset_info);
          if (res2->uses_buffer_owned_by(str))
          {
            if (res2_clone.copy(*res2))
              goto null;
            res2= &res2_clone;
          }
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

        size_t concat_len= res->length() + res2->length();

        if (tmp_value.alloced_length() < concat_len)
        {
          if (tmp_value.alloced_length() == 0)
          {
            if (tmp_value.alloc(concat_len))
              goto null;
          }
          else
          {
            size_t new_len = max(tmp_value.alloced_length() * 2, concat_len);

            if (tmp_value.mem_realloc(new_len))
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
  uint key_number, tail;
  size_t res_length;
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
    memset(&ivec, 0, sizeof(ivec));
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
  tmp_arg.mem_realloc(res_length);
  tmp_arg.length(0);
  tmp_arg.append(res->ptr(), res->length());
  code= ER_OUT_OF_RESOURCES;
  if (tmp_arg.append(append_str, tail) || tmp_value.alloc(res_length+1))
    goto error;
  tmp_arg[res_length-1]=tail;                   // save extra length
  tmp_value.mem_realloc(res_length+1);
  tmp_value.length(res_length+1);
  tmp_value.set_charset(&my_charset_bin);
  tmp_value[0]=(char) (128 | key_number);
  // Real encryption
  memset(&ivec, 0, sizeof(ivec));
  DES_ede3_cbc_encrypt((const uchar*) (tmp_arg.ptr()),
		       (uchar*) (tmp_value.ptr()+1),
		       res_length,
		       &keyschedule.ks1,
		       &keyschedule.ks2,
		       &keyschedule.ks3,
		       &ivec, TRUE);
  return &tmp_value;

error:
  push_warning_printf(current_thd, Sql_condition::SL_WARNING,
                          code, ER(code),
                          "des_encrypt");
#else
  push_warning_printf(current_thd, Sql_condition::SL_WARNING,
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
  size_t length;
  uint tail;

  if ((null_value= args[0]->null_value))
    return 0;
  length= res->length();
  if (length < 9 || (length % 8) != 1 || !((*res)[0] & 128))
    return res;				// Skip decryption if not encrypted

  if (arg_count == 1)			// If automatic uncompression
  {
    uint key_number=(uint) (*res)[0] & 127;
    // Check if automatic key and that we have privilege to uncompress using it
    if (!(current_thd->security_context()->check_access(SUPER_ACL)) ||
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

    memset(&ivec, 0, sizeof(ivec));
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

  memset(&ivec, 0, sizeof(ivec));
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
  push_warning_printf(current_thd, Sql_condition::SL_WARNING,
                          code, ER(code),
                          "des_decrypt");
wrong_key:
#else
  push_warning_printf(current_thd, Sql_condition::SL_WARNING,
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
  res=str;

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
      push_warning_printf(current_thd, Sql_condition::SL_WARNING,
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
        // If res2 is a substring of str, then clone it first.
        char buff[STRING_BUFFER_USUAL_SIZE];
        String res2_clone(buff, sizeof(buff), system_charset_info);
        if (res2->uses_buffer_owned_by(str))
        {
          if (res2_clone.copy(*res2))
            goto null;
          res2= &res2_clone;
        }
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

      size_t concat_len= res->length() + sep_str->length() + res2->length();

      if (tmp_value.alloced_length() < concat_len)
      {
        if (tmp_value.alloced_length() == 0)
        {
          if (tmp_value.alloc(concat_len))
            goto null;
        }
        else
        {
          size_t new_len = max(tmp_value.alloced_length() * 2, concat_len);

          if (tmp_value.mem_realloc(new_len))
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
      tmp_value.mem_realloc(res->length()))
  {
    null_value= 1;
    return 0;
  }
  tmp_value.length(res->length());
  tmp_value.set_charset(res->charset());
  ptr= (char *) res->ptr();
  end= ptr + res->length();
  tmp= (char *) tmp_value.ptr() + tmp_value.length();
  if (use_mb(res->charset()))
  {
    uint32 l;
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
    Fix that this works with binary strings
*/

String *Item_func_replace::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  String *res,*res2,*res3;
  int offset;
  size_t from_length, to_length;
  bool alloced=0;
  const char *ptr,*end,*strend,*search,*search_end;
  uint32 l;
  bool binary_cmp;

  null_value=0;
  res=args[0]->val_str(str);
  if (args[0]->null_value)
    goto null;
  res2=args[1]->val_str(&tmp_value);
  if (args[1]->null_value)
    goto null;

  res->set_charset(collation.collation);

  binary_cmp = ((res->charset()->state & MY_CS_BINSORT) || !use_mb(res->charset()));

  if (res2->length() == 0)
    return res;
  offset=0;
  if (binary_cmp && (offset=res->strstr(*res2)) < 0)
    return res;
  if (!(res3=args[2]->val_str(&tmp_value2)))
    goto null;
  from_length= res2->length();
  to_length=   res3->length();

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
          char *i,*j;
          i=(char*) ptr+1; j=(char*) search+1;
          while (j != search_end)
            if (*i++ != *j++) goto skip;
          offset= (int) (ptr-res->ptr());
          if (res->length()-from_length + to_length >
	      current_thd->variables.max_allowed_packet)
	  {
	    push_warning_printf(current_thd, Sql_condition::SL_WARNING,
				ER_WARN_ALLOWED_PACKET_OVERFLOWED,
				ER(ER_WARN_ALLOWED_PACKET_OVERFLOWED),
				func_name(),
				current_thd->variables.max_allowed_packet);

            goto null;
	  }
          if (!alloced)
          {
            alloced=1;
            if (res->uses_buffer_owned_by(str))
            {
              if (tmp_value_res.alloc(res->length() + to_length) ||
                  tmp_value_res.copy(*res))
                goto null;
              res= &tmp_value_res;
            }
            else
              res= copy_if_not_alloced(str, res, res->length() + to_length);
          }
          res->replace((uint) offset,from_length,*res3);
	  offset+=(int) to_length;
          goto redo;
        }
skip:
        if ((l= my_ismbchar(res->charset(), ptr,strend)))
          ptr+= l;
        else
          ++ptr;
    }
  }
  else
    do
    {
      if (res->length()-from_length + to_length >
	  current_thd->variables.max_allowed_packet)
      {
	push_warning_printf(current_thd, Sql_condition::SL_WARNING,
			    ER_WARN_ALLOWED_PACKET_OVERFLOWED,
			    ER(ER_WARN_ALLOWED_PACKET_OVERFLOWED), func_name(),
			    current_thd->variables.max_allowed_packet);
        goto null;
      }
      if (!alloced)
      {
        alloced=1;
        if (res->uses_buffer_owned_by(str))
        {
          if (tmp_value_res.alloc(res->length() + to_length) ||
              tmp_value_res.copy(*res))
            goto null;
          res= &tmp_value_res;
        }
        else
          res= copy_if_not_alloced(str, res, res->length() + to_length);
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
  longlong start, length, orig_len;  /* must be longlong to avoid truncation */

  null_value=0;
  res=args[0]->val_str(str);
  res2=args[3]->val_str(&tmp_value);
  start= args[1]->val_int();
  length= args[2]->val_int();

  if (args[0]->null_value || args[1]->null_value || args[2]->null_value ||
      args[3]->null_value)
    goto null; /* purecov: inspected */

  orig_len= static_cast<longlong>(res->length());

  if ((start < 1) || (start > orig_len))
    return res;                                 // Wrong param; skip insert

  --start;    // Internal start from '0'

  if ((length < 0) || (length > orig_len))
    length= orig_len;

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
  if (start > orig_len)
    return res; /* purecov: inspected */        // Wrong param; skip insert
  if (length > orig_len - start)
    length= orig_len - start;

  if ((ulonglong) (orig_len - length + res2->length()) >
      (ulonglong) current_thd->variables.max_allowed_packet)
  {
    push_warning_printf(current_thd, Sql_condition::SL_WARNING,
			ER_WARN_ALLOWED_PACKET_OVERFLOWED,
			ER(ER_WARN_ALLOWED_PACKET_OVERFLOWED),
			func_name(), current_thd->variables.max_allowed_packet);
    goto null;
  }
  if (res->uses_buffer_owned_by(str))
  {
    if (tmp_value_res.alloc(orig_len) ||
        tmp_value_res.copy(*res))
      goto null;
    res= &tmp_value_res;
  }
  else
    res= copy_if_not_alloced(str, res, orig_len);

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
    size_t len;
    if (res->uses_buffer_owned_by(str))
    {
       if (tmp_value.copy(*res))
         return error_str();
       res= &tmp_value;
    }
    else
      res= copy_if_not_alloced(str, res, res->length());

    len= converter(collation.collation, (char*) res->ptr(), res->length(),
                                        (char*) res->ptr(), res->length());
    DBUG_ASSERT(len <= res->length());
    res->length(len);
  }
  else
  {
    size_t len= res->length() * multiply;
    tmp_value.alloc(len);
    tmp_value.set_charset(collation.collation);
    len= converter(collation.collation, (char*) res->ptr(), res->length(),
                                        (char*) tmp_value.ptr(), len);
    tmp_value.length(len);
    res= &tmp_value;
  }
  return res;
}


void Item_func_lower::fix_length_and_dec()
{
  agg_arg_charsets_for_string_result(collation, args, 1);
  DBUG_ASSERT(collation.collation != NULL);
  multiply= collation.collation->casedn_multiply;
  converter= collation.collation->cset->casedn;
  fix_char_length_ulonglong((ulonglong) args[0]->max_char_length() * multiply);
}

void Item_func_upper::fix_length_and_dec()
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
  size_t char_pos;

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
    if (args[1]->null_value)
      goto end;

    if (length <= 0)
      char_length=0;
    else
      set_if_smaller(char_length, (uint) length);
  }

end:
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

  size_t start=res->numchars();
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
  if ((start < 0) || (start + 1 > static_cast<longlong>(res->length())))
    return make_empty_result();

  length= res->charpos((int) length, (uint32) start);
  tmp_length= static_cast<longlong>(res->length()) - start;
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
      goto end;
    if (start < 0)
      max_length= ((uint)(-start) > max_length) ? 0 : (uint)(-start);
    else
      max_length-= min((uint)(start - 1), max_length);
  }
  if (arg_count == 3 && args[2]->const_item())
  {
    int32 length= (int32) args[2]->val_int();
    if (args[2]->null_value)
      goto end;
    if (length <= 0)
      max_length=0; /* purecov: inspected */
    else
      set_if_smaller(max_length,(uint) length);
  }

end:
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
  int offset;

  if (args[0]->null_value || args[1]->null_value || args[2]->null_value)
  {					// string and/or delim are null
    null_value=1;
    return 0;
  }
  null_value=0;
  size_t delimiter_length= delimiter->length();
  if (!res->length() || !delimiter_length || !count)
    return make_empty_result();		// Wrong parameters

  res->set_charset(collation.collation);

  if (use_mb(res->charset()))
  {
    const char *ptr= res->ptr();
    const char *strend= ptr+res->length();
    const char *end= strend-delimiter_length+1;
    const char *search= delimiter->ptr();
    const char *search_end= search+delimiter_length;
    int32 n=0,c=count,pass;
    uint32 l;
    for (pass=(count>0);pass<2;++pass)
    {
      while (ptr < end)
      {
        if (*ptr == *search)
        {
	  char *i,*j;
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
  {
    if (count > 0)
    {					// start counting from the beginning
      for (offset=0; ; offset+= delimiter_length)
      {
	if ((offset= res->strstr(*delimiter, offset)) < 0)
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
      for (offset=res->length(); offset; )
      {
        /* 
          this call will result in finding the position pointing to one 
          address space less than where the found substring is located
          in res
        */
	if ((offset= res->strrstr(*delimiter, offset)) < 0)
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
        return res;			// Didn't find, return org string
    }
  }
  return (&tmp_value);
}

/*
** The trim functions are extension to ANSI SQL because they trim substrings
** They ltrim() and rtrim() functions are optimized for 1 byte strings
** They also return the original string if possible, else they return
** a substring that points at the original string.
*/

String *Item_func_trim::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);

  String *res= args[0]->val_str(str);
  if ((null_value = args[0]->null_value))
    return NULL;

  char buff[MAX_FIELD_WIDTH];
  String tmp(buff, sizeof(buff), system_charset_info);
  const String *remove_str= &remove;            // Default value.

  if (arg_count == 2)
  {
    remove_str= args[1]->val_str(&tmp);
    if ((null_value= args[1]->null_value))
      return NULL;
  }

  const size_t remove_length= remove_str->length();
  if (remove_length == 0 ||
      remove_length > res->length())
    return res;

  const char *ptr= res->ptr();
  const char *end= ptr + res->length();
  const char *const r_ptr= remove_str->ptr();

  if (use_mb(res->charset()))
  {
    if (m_trim_leading)
    {
      while (ptr + remove_length <= end)
      {
        uint num_bytes= 0;
        while (num_bytes < remove_length)
        {
          uint len;
          if ((len= my_ismbchar(res->charset(), ptr + num_bytes, end)))
            num_bytes+= len;
          else
            ++num_bytes;
        }
        if (num_bytes != remove_length)
          break;
        if (memcmp(ptr, r_ptr, remove_length))
          break;
        ptr+= remove_length;
      }
    }
    if (m_trim_trailing)
    {
      // Optimize a common case, removing 0x20
      if (remove_length == 1)
      {
        const char *save_ptr= ptr;
        const char *new_end= ptr;
        const char chr= (*remove_str)[0];
        while (ptr < end)
        {
          uint32 l;
          if ((l= my_ismbchar(res->charset(), ptr, end)))
          {
            ptr+= l;
            new_end= ptr;
          }
          else if (*ptr++ != chr)
            new_end= ptr;
        }
        end= new_end;
        ptr= save_ptr;
      }
      else
      {
        bool found;
        const char *save_ptr= ptr;
        do
        {
          found= false;
          while (ptr + remove_length < end)
          {
            uint32 l;
            if ((l= my_ismbchar(res->charset(), ptr, end)))
              ptr+= l;
            else
              ++ptr;
          }
          if (ptr + remove_length == end && !memcmp(ptr, r_ptr, remove_length))
          {
            end-= remove_length;
            found= true;
          }
          ptr= save_ptr;
        }
        while (found);
      }
    }
  }
  else
  {
    if (m_trim_leading)
    {
      while (ptr + remove_length <= end && !memcmp(ptr, r_ptr, remove_length))
        ptr+= remove_length;
    }
    if (m_trim_trailing)
    {
      while (ptr + remove_length <= end &&
             !memcmp(end-remove_length, r_ptr, remove_length))
        end-=remove_length;
    }
  }
  if (ptr == res->ptr() && end == ptr + res->length())
    return res;
  tmp_value.set(*res, static_cast<uint>(ptr - res->ptr()),
                static_cast<uint>(end - ptr));
  return &tmp_value;
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

/*
  We need a separate function for print(), in order to do correct printing.
  The function func_name() is also used e.g. by Item_func::eq() to
  distinguish between different functions, and we do not want
  trim(leading) to match trim(trailing) for eq()
 */
static const char *trim_func_name(Item_func_trim::TRIM_MODE mode)
{
  switch(mode)
  {
  case Item_func_trim::TRIM_BOTH_DEFAULT:
  case Item_func_trim::TRIM_BOTH:
  case Item_func_trim::TRIM_LEADING:
  case Item_func_trim::TRIM_TRAILING:     return "trim";
  case Item_func_trim::TRIM_LTRIM:        return "ltrim";
  case Item_func_trim::TRIM_RTRIM:        return "rtrim";
  }
  return NULL;
}

void Item_func_trim::print(String *str, enum_query_type query_type)
{
  str->append(trim_func_name(m_trim_mode));
  str->append('(');
  const char *mode_name;
  switch(m_trim_mode) {
  case TRIM_BOTH:
    mode_name= "both ";
    break;
  case TRIM_LEADING:
    mode_name= "leading ";
    break;
  case TRIM_TRAILING:
    mode_name= "trailing ";
    break;
  default:
    mode_name= NULL;
    break;
  }
  if (mode_name)
  {
    str->append(mode_name);
  }
  if (arg_count == 2)
  {
    args[1]->print(str, query_type);
    str->append(STRING_WITH_LEN(" from "));
  }
  args[0]->print(str, query_type);
  str->append(')');
}


/**
  Helper function for calculating a new password. Used in 
  Item_func_password::fix_length_and_dec() for const parameters and in 
  Item_func_password::val_str_ascii() for non-const parameters.
  @param str The plain text password which should be digested
  @param buffer a pointer to the buffer where the digest will be stored.

  @note The buffer must be of at least CRYPT_MAX_PASSWORD_SIZE size.

  @return Size of the password.
*/

static size_t calculate_password(String *str, char *buffer)
{
  DBUG_ASSERT(str);
  if (str->length() == 0) // PASSWORD('') returns ''
    return 0;

  size_t buffer_len= 0;
  THD *thd= current_thd;
  int old_passwords= 0;
  if (thd)
    old_passwords= thd->variables.old_passwords;

  push_deprecated_warn_no_replacement(current_thd, "PASSWORD");
#if defined(HAVE_OPENSSL)
  if (old_passwords == 2)
  {
    if (str->length() > MAX_PLAINTEXT_LENGTH)
    {
      my_error(ER_NOT_VALID_PASSWORD, MYF(0));
      return 0;
    }

    my_make_scrambled_password(buffer, str->ptr(),
                               str->length());
    buffer_len= strlen(buffer) + 1;
  }
  else
#endif
  if (old_passwords == 0)
  {
    my_make_scrambled_password_sha1(buffer, str->ptr(),
                                    str->length());
    buffer_len= SCRAMBLED_PASSWORD_CHAR_LENGTH;
  }
  return buffer_len;
}

/* Item_func_password */
void Item_func_password::fix_length_and_dec()
{
  maybe_null= false; // PASSWORD() never returns NULL
  
  if (args[0]->const_item())
  {
    String str;
    String *res= args[0]->val_str(&str);
    if (!args[0]->null_value)
    {
      m_hashed_password_buffer_len=
        calculate_password(res, m_hashed_password_buffer);
      fix_length_and_charset(m_hashed_password_buffer_len, default_charset());
      m_recalculate_password= false;
      return;
    }
  }

  m_recalculate_password= true;
  fix_length_and_charset(CRYPT_MAX_PASSWORD_SIZE, default_charset());
}

String *Item_func_password::val_str_ascii(String *str)
{
  DBUG_ASSERT(fixed == 1);

  String *res= args[0]->val_str(str);

  if (args[0]->null_value)
    res= make_empty_result();

  /* we treat NULLs as equal to empty string when calling the plugin */
  my_validate_password_policy(res->ptr(), res->length());

  null_value= 0;
  if (args[0]->null_value)  // PASSWORD(NULL) returns ''
    return res;
  
  if (m_recalculate_password)
    m_hashed_password_buffer_len= calculate_password(res,
                                                     m_hashed_password_buffer);

  if (m_hashed_password_buffer_len == 0)
    return make_empty_result();

  str->set(m_hashed_password_buffer, m_hashed_password_buffer_len,
           default_charset());

  return str;
}

bool Item_func_encrypt::itemize(Parse_context *pc, Item **res)
{
  if (skip_itemize(res))
    return false;
  if (super::itemize(pc, res))
    return true;
  DBUG_ASSERT(arg_count == 1 || arg_count == 2);
  if (arg_count == 1)
    pc->thd->lex->set_uncacheable(pc->select, UNCACHEABLE_RAND);
  return false;
}


#define bin_to_ascii(c) ((c)>=38?((c)-38+'a'):(c)>=12?((c)-12+'A'):(c)+'.')

String *Item_func_encrypt::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
#ifdef HAVE_CRYPT
  String *res = args[0]->val_str(str);
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
  str->set(tmp, strlen(tmp), &my_charset_bin);
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
  if (res->uses_buffer_owned_by(str))
  {
    if (tmp_value_res.copy(*res))
      return error_str();
    res= &tmp_value_res;
  }
  else
    res= copy_if_not_alloced(str, res, res->length());

  crypto_transform(res);
  sql_crypt.reinit();

  return res;
}

void Item_func_encode::crypto_transform(String *res)
{
  push_deprecated_warn(current_thd, "ENCODE", "AES_ENCRYPT");
  sql_crypt.encode((char*) res->ptr(),res->length());
  res->set_charset(&my_charset_bin);
}

void Item_func_decode::crypto_transform(String *res)
{
  push_deprecated_warn(current_thd, "DECODE", "AES_DECRYPT");
  sql_crypt.decode((char*) res->ptr(),res->length());
}


Item *Item_func_sysconst::safe_charset_converter(const CHARSET_INFO *tocs)
{
  Item_string *conv;
  uint conv_errors;
  String tmp, cstr, *ostr= val_str(&tmp);
  if (null_value)
  {
    Item *null_item= new Item_null(fully_qualified_func_name());
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


bool Item_func_database::itemize(Parse_context *pc, Item **res)
{
  if (skip_itemize(res))
    return false;
  if (super::itemize(pc, res))
    return true;
 
  pc->thd->lex->safe_to_cache_query=0;
  return false;
}


String *Item_func_database::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  THD *thd= current_thd;
  if (thd->db().str == NULL)
  {
    null_value= 1;
    return 0;
  }
  else
    str->copy(thd->db().str, thd->db().length, system_charset_info);
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
    const CHARSET_INFO *cs= str_value.charset();
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


bool Item_func_user::itemize(Parse_context *pc, Item **res)
{
  if (skip_itemize(res))
    return false;
  if (super::itemize(pc, res))
    return true;

  LEX *lex= pc->thd->lex;
  lex->set_stmt_unsafe(LEX::BINLOG_STMT_UNSAFE_SYSTEM_FUNCTION);
  lex->safe_to_cache_query= 0;
  return false;
}

bool Item_func_user::fix_fields(THD *thd, Item **ref)
{
  return (Item_func_sysconst::fix_fields(thd, ref) ||
          init(thd->m_main_security_ctx.user().str,
               thd->m_main_security_ctx.host_or_ip().str));
}


bool Item_func_current_user::itemize(Parse_context *pc, Item **res)
{
  if (skip_itemize(res))
    return false;
  if (super::itemize(pc, res))
    return true;

  context= pc->thd->lex->current_context();
  return false;
}


bool Item_func_current_user::fix_fields(THD *thd, Item **ref)
{
  if (Item_func_sysconst::fix_fields(thd, ref))
    return TRUE;

  Security_context *ctx=
#ifndef NO_EMBEDDED_ACCESS_CHECKS
                         (context->security_ctx
                          ? context->security_ctx : thd->security_context());
#else
                         thd->security_context();
#endif /*NO_EMBEDDED_ACCESS_CHECKS*/
  return init(ctx->priv_user().str, ctx->priv_host().str);
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
  const CHARSET_INFO *cs= collation.collation;
  my_wc_t wc;
  uint nchars;
  int rc;

  if ((null_value= args[0]->null_value))
    return 0; /* purecov: inspected */

  if (tmp_value.alloc(max(res->length(), static_cast<size_t>(4 * cs->mbminlen))))
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


void Item_func_geohash::fix_length_and_dec()
{
  fix_length_and_charset(Item_func_geohash::upper_limit_output_length,
                         default_charset());
}


/**
  Here we check for valid types. We have to accept geometry of any type,
  and determine if it's really a POINT in val_str(). 
*/
bool Item_func_geohash::fix_fields(THD *thd, Item **ref)
{
  if (Item_str_func::fix_fields(thd, ref))
    return true;

  int geohash_length_arg_index;
  if (arg_count == 2)
  {
    /*
      First argument expected to be a point and second argument is expected
      to be geohash output length.

      PARAM_ITEM and the binary charset checks are to allow prepared statements
      and usage of user-defined variables.
    */
    geohash_length_arg_index= 1;
    maybe_null= (args[0]->maybe_null || args[1]->maybe_null);
    if (!is_item_null(args[0]) &&
        args[0]->field_type() != MYSQL_TYPE_GEOMETRY &&
        args[0]->type() != PARAM_ITEM &&
        args[0]->collation.collation != &my_charset_bin)
    {
      my_error(ER_INCORRECT_TYPE, MYF(0), "point", func_name());
      return true;
    }
  }
  else if (arg_count == 3)
  {
    /*
      First argument is expected to be longitude, second argument is expected
      to be latitude and third argument is expected to be geohash
      output length.
    */
    geohash_length_arg_index= 2;
    maybe_null= (args[0]->maybe_null || args[1]->maybe_null ||
                 args[2]->maybe_null);
    if (!check_valid_latlong_type(args[0]))
    {
      my_error(ER_INCORRECT_TYPE, MYF(0), "longitude", func_name());
      return true;
    }
    else if (!check_valid_latlong_type(args[1]))
    {
      my_error(ER_INCORRECT_TYPE, MYF(0), "latitude", func_name());
      return true;
    }
  }
  else
  {
    /*
      This should never happen, since function
      only supports two or three arguments.
    */
    DBUG_ASSERT(false);
    return true;
  }


  /*
    Check if geohash length argument is of valid type.

    PARAM_ITEM is to allow parameter marker during PREPARE, and INT_ITEM is to
    allow EXECUTE of prepared statements and usage of user-defined variables.
  */
  if (is_item_null(args[geohash_length_arg_index]))
    return false;

  bool is_binary_charset=
    (args[geohash_length_arg_index]->collation.collation == &my_charset_bin);
  bool is_parameter=
    (args[geohash_length_arg_index]->type() == PARAM_ITEM ||
     args[geohash_length_arg_index]->type() == INT_ITEM);

  switch (args[geohash_length_arg_index]->field_type())
  {
  case MYSQL_TYPE_TINY:
  case MYSQL_TYPE_SHORT:
  case MYSQL_TYPE_LONG:
  case MYSQL_TYPE_LONGLONG:
  case MYSQL_TYPE_INT24:
  case MYSQL_TYPE_VARCHAR:
  case MYSQL_TYPE_STRING:
  case MYSQL_TYPE_VAR_STRING:
  case MYSQL_TYPE_TINY_BLOB:
  case MYSQL_TYPE_MEDIUM_BLOB:
  case MYSQL_TYPE_LONG_BLOB:
  case MYSQL_TYPE_BLOB:
    if (is_binary_charset && !is_parameter)
    {
      my_error(ER_INCORRECT_TYPE, MYF(0), "geohash max length", func_name());
      return true;
    }
    break;
  default:
    my_error(ER_INCORRECT_TYPE, MYF(0), "geohash max length", func_name());
    return true;
  }
  return false;
}


/**
  Checks if supplied item is a valid latitude or longitude, based on which
  type it is. Implemented as a whitelist of allowed types, where binary data is
  not allowed.

  @param ref Item to check for valid latitude/longitude.
  @return false if item is not valid, true otherwise.
*/
bool Item_func_geohash::check_valid_latlong_type(Item *arg)
{
  if (is_item_null(arg))
    return true;

  /*
    is_field_type_valid will be true if the item is a constant or a field of
    valid type.
  */
  bool is_binary_charset= (arg->collation.collation == &my_charset_bin);
  bool is_field_type_valid= false;
  switch (arg->field_type())
  {
  case MYSQL_TYPE_DECIMAL:
  case MYSQL_TYPE_NEWDECIMAL:
  case MYSQL_TYPE_TINY:
  case MYSQL_TYPE_SHORT:
  case MYSQL_TYPE_LONG:
  case MYSQL_TYPE_FLOAT:
  case MYSQL_TYPE_DOUBLE:
  case MYSQL_TYPE_LONGLONG:
  case MYSQL_TYPE_INT24:
  case MYSQL_TYPE_VARCHAR:
  case MYSQL_TYPE_STRING:
  case MYSQL_TYPE_VAR_STRING:
  case MYSQL_TYPE_TINY_BLOB:
  case MYSQL_TYPE_MEDIUM_BLOB:
  case MYSQL_TYPE_LONG_BLOB:
  case MYSQL_TYPE_BLOB:
    is_field_type_valid= !is_binary_charset;
    break;
  default:
    is_field_type_valid= false;
    break;
  }

  /*
    Parameters and parameter markers always have
    field_type() == MYSQL_TYPE_VARCHAR. type() is dependent on if it's a
    parameter marker or parameter (PREPARE or EXECUTE, respectively).
  */
  bool is_parameter= (arg->type() == INT_ITEM || arg->type() == DECIMAL_ITEM ||
                      arg->type() == REAL_ITEM || arg->type() == STRING_ITEM) &&
                     (arg->field_type() == MYSQL_TYPE_VARCHAR);
  bool is_parameter_marker= (arg->type() == PARAM_ITEM &&
                             arg->field_type() == MYSQL_TYPE_VARCHAR);

  if (is_field_type_valid || is_parameter_marker || is_parameter)
    return true;
  return false;
}


/**
  Check if a Item is NULL. This includes NULL in the form of literal
  NULL, NULL in a user-defined variable and NULL in prepared statements.

  Note that it will return true for MEDIUM_BLOB for FUNC_ITEM as well, in order
  to allow NULL in user-defined variables.

  @param item The item to check for NULL.

  @return true if the item is NULL, false otherwise.
*/
bool Item_func_geohash::is_item_null(Item *item)
{
  if (item->field_type() == MYSQL_TYPE_NULL || item->type() == NULL_ITEM)
    return true;

  // The following will allow the usage of NULL in user-defined variables.
  bool is_binary_charset= (item->collation.collation == &my_charset_bin);
  if (is_binary_charset && item->type() == FUNC_ITEM &&
      item->field_type() == MYSQL_TYPE_MEDIUM_BLOB)
  {
    return true;
  }
  return false;
}


/**
  Populate member variables with values from arguments.

  In this function we populate the member variables 'latitude', 'longitude'
  and 'geohash_length' with values from the arguments supplied by the user.
  We also do type checking on the geometry object, as well as out-of-range
  check for both longitude, latitude and geohash length.

  If an expection is raised, null_value will not be set. If a null argument
  was detected, null_value will be set to true.

  @return false if class variables was populated, or true if the function
          failed to populate them.
*/
bool Item_func_geohash::fill_and_check_fields()
{
  longlong geohash_length_arg= -1;
  if (arg_count == 2)
  {
    // First argument is point, second argument is geohash output length.
    String string_buffer;
    String *swkb= args[0]->val_str(&string_buffer);
    geohash_length_arg= args[1]->val_int();

    if ((null_value= args[0]->null_value || args[1]->null_value || !swkb))
    {
      return true;
    }
    else
    {
      Geometry *geom;
      Geometry_buffer geometry_buffer;
      if (!(geom= Geometry::construct(&geometry_buffer, swkb)))
      {
        my_error(ER_GIS_INVALID_DATA, MYF(0), func_name());
        return true;
      }
      else if (geom->get_type() != Geometry::wkb_point ||
               geom->get_x(&longitude) || geom->get_y(&latitude))
      {
        my_error(ER_INCORRECT_TYPE, MYF(0), "point", func_name());
        return true;
      }
    }
  }
  else if (arg_count == 3)
  {
    /*
      First argument is longitude, second argument is latitude
      and third argument is geohash output length.
    */
    longitude= args[0]->val_real();
    latitude= args[1]->val_real();
    geohash_length_arg= args[2]->val_int();

    if ((null_value= args[0]->null_value || args[1]->null_value || 
         args[2]->null_value))
      return true;
  }

  // Check if supplied arguments are within allowed range.
  if (longitude > max_longitude || longitude < min_longitude)
  {
    my_error(ER_DATA_OUT_OF_RANGE, MYF(0), "longitude", func_name());
    return true;
  }
  else if (latitude > max_latitude || latitude < min_latitude)
  {
    my_error(ER_DATA_OUT_OF_RANGE, MYF(0), "latitude", func_name());
    return true;
  }

  if (geohash_length_arg <= 0 ||
      geohash_length_arg > upper_limit_output_length)
  {
    char geohash_length_string[MAX_BIGINT_WIDTH + 1];
    llstr(geohash_length_arg, geohash_length_string);
    my_error(ER_DATA_OUT_OF_RANGE, MYF(0), "max geohash length", func_name());
    return true;
  }

  geohash_max_output_length= static_cast<uint>(geohash_length_arg);
  return false;
}


/**
  Encodes a pair of longitude and latitude values into a geohash string.
  The length of the output string will be no longer than the value of
  geohash_max_output_length member variable, but it might be shorter. The stop
  condition is the following:

  After appending a character to the output string, check if the encoded values
  of latitude and longitude matches the input arguments values. If so, return
  the result to the user.

  It does exist latitudes/longitude values which will cause the algorithm to
  loop until MAX(max_geohash_length, upper_geohash_limit), no matter how large
  these arguments are (eg. SELECT GEOHASH(0.01, 1, 100); ). It is thus
  important to supply an reasonable max geohash length argument.
*/
String *Item_func_geohash::val_str_ascii(String *str)
{
  DBUG_ASSERT(fixed == TRUE);

  if (fill_and_check_fields())
  {
    if (null_value)
    {
      return NULL;
    }
    else
    {
      /*
        Since null_value == false, my_error() was raised inside
        fill_and_check_fields().
      */
      return error_str();
    }
  }

  // Allocate one extra byte, for trailing '\0'.
  if (str->alloc(geohash_max_output_length + 1))
    return make_empty_result();
  str->length(0);

  double upper_latitude= max_latitude;
  double lower_latitude= min_latitude;
  double upper_longitude= max_longitude;
  double lower_longitude= min_longitude;
  bool even_bit= true;

  for (uint i= 0; i < geohash_max_output_length; i++)
  {
    /*
      We must encode in blocks of five bits, so we don't risk stopping
      in the middle of a character. If we stop in the middle of a character,
      some encoded geohash values will differ from geohash.org.
    */
    char current_char= 0;
    for (uint bit_number= 0; bit_number < 5; bit_number++)
    {
      if (even_bit)
      {
        // Encode one longitude bit.
        encode_bit(&upper_longitude, &lower_longitude, longitude,
                   &current_char, bit_number);
      }
      else
      {
        // Encode one latitude bit.
        encode_bit(&upper_latitude, &lower_latitude, latitude,
                   &current_char, bit_number);
      }
      even_bit = !even_bit;
    }
    str->q_append(char_to_base32(current_char));

    /*
      If encoded values of latitude and longitude matches the supplied
      arguments, there is no need to do more calculation.
    */
    if (latitude == (lower_latitude + upper_latitude) / 2.0 &&
        longitude == (lower_longitude + upper_longitude) / 2.0)
      break;
  }
  return str;
}


/**
  Sets the bit number in char_value, determined by following formula:

  IF target_value < (middle between lower_value and upper_value)
  set bit to 0
  ELSE
  set bit to 1

  When the function returns, upper_value OR lower_value are adjusted
  to the middle value between lower and upper.

  @param upper_value The upper error range for latitude or longitude.
  @param lower_value The lower error range for latitude or longitude.
  @param target_value Latitude or longitude value supplied as argument
  by the user.
  @param char_value The character we want to set the bit on.
  @param bit_number Wich bit number in char_value to set.
*/
void Item_func_geohash::encode_bit(double *upper_value, double *lower_value,
                                   double target_value, char *char_value,
                                   int bit_number)
{
  DBUG_ASSERT(bit_number >= 0 && bit_number <= 4);

  double middle_value= (*upper_value + *lower_value) / 2.0;
  if (target_value < middle_value)
  {
    *upper_value= middle_value;
    *char_value |= 0 << (4 - bit_number);
  }
  else
  {
    *lower_value= middle_value;
    *char_value |= 1 << (4 - bit_number);
  }
}

/**
  Converts a char value to it's base32 representation, where 0 = a,
  1 = b, ... , 30 = y, 31 = z.

  The function expects that the input character is within allowed range.

  @param char_input The value to convert.

  @return the ASCII equivalent.
*/
char Item_func_geohash::char_to_base32(char char_input)
{
  DBUG_ASSERT(char_input <= 31);

  if (char_input < 10)
    return char_input + '0';
  else if (char_input < 17)
    return char_input + ('b' - 10);
  else if (char_input < 19)
    return char_input + ('b' - 10 + 1);
  else if (char_input < 21)
    return char_input + ('b' - 10 + 2);
  else
    return char_input + ('b' - 10 + 3);
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
    push_warning_printf(current_thd, Sql_condition::SL_WARNING,
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
  reject_geometry_args(arg_count, args, this);
}


/**
  @todo
  This needs to be fixed for multi-byte character set where numbers
  are stored in more than one byte
*/

String *Item_func_format::val_str_ascii(String *str)
{
  size_t str_length;
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
    if (my_isnan(nr) || my_isinf(nr))
      return str;
    str_length=str->length();
  }
  /* We need this test to handle 'nan' and short values */
  if (lc->grouping[0] > 0 &&
      str_length >= dec_length + 1 + lc->grouping[0])
  {
    /* We need space for ',' between each group of digits as well. */
    char buf[2 * FLOATING_POINT_BUFFER + 2] = {0};
    int count;
    const char *grouping= lc->grouping;
    char sign_length= *str->ptr() == '-' ? 1 : 0;
    const char *src= str->ptr() + str_length - dec_length - 1;
    const char *src_begin= str->ptr() + sign_length;
    char *dst= buf + 2 * FLOATING_POINT_BUFFER;
    char *start_dst = dst;

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
    size_t result_length = start_dst - dst;
    str->copy(dst, result_length, &my_charset_latin1);
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
  if ((tmp= (uint) args[0]->val_int()) == 0 || args[0]->null_value
      || tmp >= arg_count)
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
  if ((tmp= (uint) args[0]->val_int()) == 0 || args[0]->null_value
      || tmp >= arg_count)
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
  if ((tmp= (uint) args[0]->val_int()) == 0 || args[0]->null_value
      || tmp >= arg_count)
    return NULL;

  String *result= args[tmp]->val_str(str);
  if (result)
    result->set_charset(collation.collation);
  null_value= args[tmp]->null_value;
  return result;
}


bool Item_func_make_set::itemize(Parse_context *pc, Item **res)
{
  if (skip_itemize(res))
    return false;
  /*
    We have to itemize() the "item" before the super::itemize() call there since
    this reflects the "natural" order of former semantic action code execution
    in the original parser:
  */
  return item->itemize(pc, &item) || super::itemize(pc, res);
}


void Item_func_make_set::split_sum_func(THD *thd,
                                        Ref_ptr_array ref_pointer_array,
					List<Item> &fields)
{
  item->split_sum_func2(thd, ref_pointer_array, fields, &item, TRUE);
  Item_str_func::split_sum_func(thd, ref_pointer_array, fields);
}


void Item_func_make_set::fix_length_and_dec()
{
  uint32 char_length= arg_count - 1; /* Separators */

  if (agg_arg_charsets_for_string_result(collation, args, arg_count))
    return;
  
  for (uint i=0 ; i < arg_count ; i++)
    char_length+= args[i]->max_char_length();
  fix_char_length(char_length);
  used_tables_cache|=	  item->used_tables();
  not_null_tables_cache&= item->not_null_tables();
  const_item_cache&=	  item->const_item();
  with_sum_func= with_sum_func || item->with_sum_func;
}


void Item_func_make_set::update_used_tables()
{
  Item_func::update_used_tables();
  item->update_used_tables();
  used_tables_cache|=item->used_tables();
  const_item_cache&=item->const_item();
  with_subselect|= item->has_subquery();
  with_stored_program|= item->has_stored_program();
}


String *Item_func_make_set::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  ulonglong bits;
  bool first_found=0;
  Item **ptr=args;
  String *result= NULL;

  bits=item->val_int();
  if ((null_value=item->null_value))
    return NULL;

  if (arg_count < 64)
    bits &= ((ulonglong) 1 << arg_count)-1;

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
            if (tmp_str.alloc((result != NULL ? result->length() : 0) +
                              res->length() + 1) ||
		tmp_str.copy(*result))
              return make_empty_result();
	    result= &tmp_str;
	  }
	  if (tmp_str.append(STRING_WITH_LEN(","), &my_charset_bin) ||
              tmp_str.append(*res))
            return make_empty_result();
	}
      }
    }
  }
  if (result == NULL)
    return make_empty_result();
  return result;
}


Item *Item_func_make_set::transform(Item_transformer transformer, uchar *arg)
{
  DBUG_ASSERT(!current_thd->stmt_arena->is_stmt_prepare());

  Item *new_item= item->transform(transformer, arg);
  if (!new_item)
    return 0;

  /*
    THD::change_item_tree() should be called only if the tree was
    really transformed, i.e. when a new item has been created.
    Otherwise we'll be allocating a lot of unnecessary memory for
    change records at each execution.
  */
  if (item != new_item)
    current_thd->change_item_tree(&item, new_item);
  return Item_str_func::transform(transformer, arg);
}


void Item_func_make_set::print(String *str, enum_query_type query_type)
{
  str->append(STRING_WITH_LEN("make_set("));
  item->print(str, query_type);
  if (arg_count)
  {
    str->append(',');
    print_args(str, 0, query_type);
  }
  str->append(')');
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
  str->mem_realloc(str->length());			// Add end 0 (for Purify)
  return check_well_formed_result(str,
                                  false,  // send warning
                                  true);  // truncate
}


inline String* alloc_buffer(String *res,String *str,String *tmp_value,
                            size_t length)
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
    if (args[1]->null_value)
      goto end;

    /* Assumes that the maximum length of a String is < INT_MAX32. */
    /* Set here so that rest of code sees out-of-bound value as such. */
    if (count > INT_MAX32)
      count= INT_MAX32;

    ulonglong char_length= (ulonglong) args[0]->max_char_length() * count;
    fix_char_length_ulonglong(char_length);
    return;
  }

end:
  max_length= MAX_BLOB_WIDTH;
  maybe_null= 1;
}

/**
  Item_func_repeat::str is carefully written to avoid reallocs
  as much as possible at the cost of a local buffer
*/

String *Item_func_repeat::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  size_t length, tot_length;
  char *to;
  /* must be longlong to avoid truncation */
  longlong count= args[1]->val_int();
  String *res= args[0]->val_str(str);

  if (args[0]->null_value || args[1]->null_value)
    goto err;				// string and/or delim are null
  null_value= 0;

  if (count <= 0 && (count == 0 || !args[1]->unsigned_flag))
    return make_empty_result();

  // Avoid looping, concatenating the empty string.
  if (res->length() == 0)
    return res;

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
    push_warning_printf(current_thd, Sql_condition::SL_WARNING,
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



void Item_func_space::fix_length_and_dec()
{
  collation.set(default_charset(), DERIVATION_COERCIBLE, MY_REPERTOIRE_ASCII); 
  if (args[0]->const_item())
  {
    /* must be longlong to avoid truncation */
    longlong count= args[0]->val_int();
    if (args[0]->null_value)
      goto end;
    /*
     Assumes that the maximum length of a String is < INT_MAX32. 
     Set here so that rest of code sees out-of-bound value as such. 
    */
    if (count > INT_MAX32)
      count= INT_MAX32;
    fix_char_length_ulonglong(count); 
    return;
  }

end:
  max_length= MAX_BLOB_WIDTH;
  maybe_null= 1;
}


String *Item_func_space::val_str(String *str)
{
  uint tot_length;
  longlong count= args[0]->val_int();
  const CHARSET_INFO *cs= collation.collation;
   
  if (args[0]->null_value)
    goto err;				// string and/or delim are null
  null_value= 0;

  if (count <= 0 && (count == 0 || !args[0]->unsigned_flag))
    return make_empty_result();
  /*
   Assumes that the maximum length of a String is < INT_MAX32. 
   Bounds check on count:  If this is triggered, we will error. 
  */
  if ((ulonglong) count > INT_MAX32)
    count= INT_MAX32;

  // Safe length check
  tot_length= (uint) count * cs->mbminlen;
  if (tot_length > current_thd->variables.max_allowed_packet)
  {
    push_warning_printf(current_thd, Sql_condition::SL_WARNING,
                        ER_WARN_ALLOWED_PACKET_OVERFLOWED,
                        ER(ER_WARN_ALLOWED_PACKET_OVERFLOWED),
                        func_name(),
                        current_thd->variables.max_allowed_packet);
    goto err;
   }

  if (str->alloc(tot_length))
    goto err;
  str->length(tot_length);
  str->set_charset(cs);
  cs->cset->fill(cs, (char*) str->ptr(), tot_length, ' ');
  return str; 

err:
  null_value= 1;
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
    if (args[1]->null_value)
      goto end;
    DBUG_ASSERT(collation.collation->mbmaxlen > 0);
    /* Assumes that the maximum length of a String is < INT_MAX32. */
    /* Set here so that rest of code sees out-of-bound value as such. */
    if (char_length > INT_MAX32)
      char_length= INT_MAX32;
    fix_char_length_ulonglong(char_length);
    return;
  }

end:
  max_length= MAX_BLOB_WIDTH;
  maybe_null= 1;
}


String *Item_func_rpad::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  char *to;
  /* must be longlong to avoid truncation */
  longlong count= args[1]->val_int();
  /* Avoid modifying this string as it may affect args[0] */
  String *res= args[0]->val_str(str);
  String *rpad= args[2]->val_str(&rpad_str);

  if (!res || args[1]->null_value || !rpad || 
      ((count < 0) && !args[1]->unsigned_flag))
  {
    null_value= true;
    return NULL;
  }
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

  if (use_mb(rpad->charset()))
  {
    // This will chop off any trailing illegal characters from rpad.
    String *well_formed_pad= args[2]->check_well_formed_result(rpad,
                                                               false, //send warning
                                                               true); //truncate
    if (!well_formed_pad)
    {
      null_value= true;
      return NULL;
    }
  }

  const size_t res_char_length= res->numchars();

  // String to pad is big enough
  if (count <= static_cast<longlong>(res_char_length))
  {
    int res_charpos= res->charpos((int)count);
    if (tmp_value.alloc(res_charpos))
      return NULL;
    (void)tmp_value.copy(*res);
    tmp_value.length(res_charpos); // Shorten result if longer
    return &tmp_value;
  }
  const size_t pad_char_length= rpad->numchars();

  // Must be ulonglong to avoid overflow
  const ulonglong byte_count= count * collation.collation->mbmaxlen;
  if (byte_count > current_thd->variables.max_allowed_packet)
  {
    push_warning_printf(current_thd, Sql_condition::SL_WARNING,
			ER_WARN_ALLOWED_PACKET_OVERFLOWED,
			ER(ER_WARN_ALLOWED_PACKET_OVERFLOWED),
			func_name(), current_thd->variables.max_allowed_packet);
    null_value= true;
    return NULL;
  }
  if (args[2]->null_value || !pad_char_length)
  {
    null_value= true;
    return NULL;
  }
  /* Must be done before alloc_buffer */
  const size_t res_byte_length= res->length();
  /*
    alloc_buffer() doesn't modify 'res' because 'res' is guaranteed too short
    at this stage.
  */
  if (!(res= alloc_buffer(res, str, &tmp_value,
                          static_cast<size_t>(byte_count))))
  {
    null_value= true;
    return NULL;
  }

  to= (char*) res->ptr()+res_byte_length;
  const char *ptr_pad=rpad->ptr();
  const size_t pad_byte_length= rpad->length();
  count-= res_char_length;
  for ( ; (uint32) count > pad_char_length; count-= pad_char_length)
  {
    memcpy(to,ptr_pad,pad_byte_length);
    to+= pad_byte_length;
  }
  if (count)
  {
    const size_t pad_charpos= rpad->charpos((int) count);
    memcpy(to, ptr_pad, pad_charpos);
    to+= pad_charpos;
  }
  res->length((uint) (to- (char*) res->ptr()));
  return (res);
}


void Item_func_lpad::fix_length_and_dec()
{
  // Handle character set for args[0] and args[2].
  if (agg_arg_charsets_for_string_result(collation, &args[0], 2, 2))
    return;
  
  if (args[1]->const_item())
  {
    ulonglong char_length= (ulonglong) args[1]->val_int();
    if (args[1]->null_value)
      goto end;
    DBUG_ASSERT(collation.collation->mbmaxlen > 0);
    /* Assumes that the maximum length of a String is < INT_MAX32. */
    /* Set here so that rest of code sees out-of-bound value as such. */
    if (char_length > INT_MAX32)
      char_length= INT_MAX32;
    fix_char_length_ulonglong(char_length);
    return;
  }

end:
  max_length= MAX_BLOB_WIDTH;
  maybe_null= 1;
}


String *Item_func_lpad::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  size_t res_char_length, pad_char_length;
  /* must be longlong to avoid truncation */
  longlong count= args[1]->val_int();
  size_t byte_count;
  /* Avoid modifying this string as it may affect args[0] */
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

  if (use_mb(pad->charset()))
  {
    // This will chop off any trailing illegal characters from pad.
    String *well_formed_pad= args[2]->check_well_formed_result(pad,
                                                               false, // send warning
                                                               true); // truncate
    if (!well_formed_pad)
      goto err;
  }

  res_char_length= res->numchars();

  if (count <= static_cast<longlong>(res_char_length))
  {
    int res_charpos= res->charpos((int)count);
   if (tmp_value.alloc(res_charpos))
     return NULL;
   (void)tmp_value.copy(*res);
   tmp_value.length(res_charpos); // Shorten result if longer
   return &tmp_value;
  }
  
  pad_char_length= pad->numchars();
  byte_count= count * collation.collation->mbmaxlen;
  
  if (byte_count > current_thd->variables.max_allowed_packet)
  {
    push_warning_printf(current_thd, Sql_condition::SL_WARNING,
			ER_WARN_ALLOWED_PACKET_OVERFLOWED,
			ER(ER_WARN_ALLOWED_PACKET_OVERFLOWED),
			func_name(), current_thd->variables.max_allowed_packet);
    goto err;
  }

  if (args[2]->null_value || !pad_char_length ||
      str->alloc(byte_count))
    goto err;

  str->length(0);
  str->set_charset(collation.collation);
  count-= res_char_length;
  while (count >= static_cast<longlong>(pad_char_length))
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


void Item_func_conv::fix_length_and_dec()
{
  collation.set(default_charset());
  max_length=64;
  maybe_null= 1;
  reject_geometry_args(arg_count, args, this);
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
  if (!arg)
  {
    null_value=1;
    return 0;
  }
  null_value= tmp_value.copy(arg->ptr(), arg->length(), arg->charset(),
                             conv_charset, &dummy_errors);
  return null_value ? 0 : check_well_formed_result(&tmp_value,
                                                   false, // send warning
                                                   true); // truncate
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

bool Item_func_set_collation::itemize(Parse_context *pc, Item **res)
{
  if (skip_itemize(res))
    return false;
  THD *thd= pc->thd;
  args[1]= new (pc->mem_root) Item_string(collation_string.str,
                                          collation_string.length,
                                          thd->charset());
  if (args[1] == NULL)
    return true;

  return super::itemize(pc, res);
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
    if (!(set_collation= mysqld_collation_get_by_name(colname)))
      return;
  }

  if (!set_collation || 
      (!my_charset_same(args[0]->collation.collation,set_collation) &&
      args[0]->collation.derivation != DERIVATION_NUMERIC))
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

  const CHARSET_INFO *cs= args[0]->charset_for_protocol(); 
  null_value= 0;
  str->copy(cs->csname, strlen(cs->csname),
	    &my_charset_latin1, collation.collation, &dummy_errors);
  return str;
}

String *Item_func_collation::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  uint dummy_errors;
  const CHARSET_INFO *cs= args[0]->charset_for_protocol(); 

  null_value= 0;
  str->copy(cs->name, strlen(cs->name),
	    &my_charset_latin1, collation.collation, &dummy_errors);
  return str;
}


bool Item_func_weight_string::itemize(Parse_context *pc, Item **res)
{
  if (skip_itemize(res))
    return false;
  if (as_binary)
  {
    if (args[0]->itemize(pc, &args[0]))
      return true;
    args[0]= new (pc->mem_root) Item_char_typecast(args[0], nweights,
                                                   &my_charset_bin);
    if (args[0] == NULL)
      return true;
  }
  return super::itemize(pc, res);
}

void Item_func_weight_string::print(String *str, enum_query_type query_type)
{
  str->append(func_name());
  str->append('(');
  args[0]->print(str, query_type);
  if (nweights && !as_binary)
  {
    str->append(" as char");
    str->append_parenthesized(nweights);
  }
  // The flags is already normalized
  uint flag_lev = flags & MY_STRXFRM_LEVEL_ALL;
  uint flag_dsc = (flags >> MY_STRXFRM_DESC_SHIFT) & MY_STRXFRM_LEVEL_ALL;
  uint flag_rev = (flags >> MY_STRXFRM_REVERSE_SHIFT) & MY_STRXFRM_LEVEL_ALL;
  if (flag_lev)
  {
    str->append(" level ");
    uint level= 1;
    while (flag_lev)
    {
      if (flag_lev & 1)
      {
        str->append_longlong(level);
        if (flag_lev >> 1)
          str->append(',');
      }
      flag_lev>>= 1;
      level++;
    }
  }
  if (flag_dsc)
  {
    // ASC is default
    str->append(" desc");
  }
  if (flag_rev)
  {
    str->append(" reverse");
  }

  str->append(')');
}

void Item_func_weight_string::fix_length_and_dec()
{
  const CHARSET_INFO *cs= args[0]->collation.collation;
  collation.set(&my_charset_bin, args[0]->collation.derivation);
  flags= my_strxfrm_flag_normalize(flags, cs->levels_for_order);
  field= args[0]->type() == FIELD_ITEM && args[0]->is_temporal() ?
         ((Item_field *) (args[0]))->field : (Field *) NULL;
  /* 
    Use result_length if it was given explicitly in constructor,
    otherwise calculate max_length using argument's max_length
    and "nweights".
  */  
  max_length= field ? field->pack_length() :
              result_length ? result_length :
              cs->mbmaxlen * max(args[0]->max_length, nweights);
  maybe_null= 1;
}

bool Item_func_weight_string::eq(const Item *item, bool binary_cmp) const
{
  if (this == item)
    return 1;
  if (item->type() != FUNC_ITEM ||
      functype() != ((Item_func*)item)->functype() ||
      func_name() != ((Item_func*)item)->func_name())
    return 0;

  Item_func_weight_string *wstr= (Item_func_weight_string*)item;
  if (nweights != wstr->nweights ||
      flags != wstr->flags)
    return 0;

  if (!args[0]->eq(wstr->args[0], binary_cmp))
      return 0;
  return 1;
}


/* Return a weight_string according to collation */
String *Item_func_weight_string::val_str(String *str)
{
  String *res;
  const CHARSET_INFO *cs= args[0]->collation.collation;
  size_t tmp_length, frm_length;
  DBUG_ASSERT(fixed == 1);

  if (args[0]->result_type() != STRING_RESULT ||
      !(res= args[0]->val_str(str)))
    goto nl;
  
  /*
    Use result_length if it was given in constructor
    explicitly, otherwise calculate result length
    from argument and "nweights".
  */
  tmp_length= field ? field->pack_length() :
              result_length ? result_length :
              cs->coll->strnxfrmlen(cs, cs->mbmaxlen *
                                    max<size_t>(res->length(), nweights));

  if(tmp_length > current_thd->variables.max_allowed_packet)
  {
    push_warning_printf(current_thd, Sql_condition::SL_WARNING,
                        ER_WARN_ALLOWED_PACKET_OVERFLOWED,
                        ER(ER_WARN_ALLOWED_PACKET_OVERFLOWED), func_name(),
                        current_thd->variables.max_allowed_packet);
    goto nl;
  }

  if (tmp_value.alloc(tmp_length))
    goto nl;

  if (field)
  {
    frm_length= field->pack_length();
    field->make_sort_key((uchar *) tmp_value.ptr(), tmp_length);
  }
  else
    frm_length= cs->coll->strnxfrm(cs,
                                   (uchar *) tmp_value.ptr(), tmp_length,
                                   nweights ? nweights : tmp_length,
                                   (const uchar *) res->ptr(), res->length(),
                                   flags);
  DBUG_ASSERT(frm_length <= tmp_length);

  tmp_value.length(frm_length);
  null_value= 0;
  return &tmp_value;

nl:
  null_value= 1;
  return 0;
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
      if ((val <= (double) LLONG_MIN) || 
          (val >= (double) (ulonglong) ULLONG_MAX))
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
  size_t length;
  null_value= true;
  DBUG_ASSERT(fixed == 1);

  res= args[0]->val_str(str);
  // For a NULL input value return NULL without any warning
  if (args[0]->null_value)
    return NULL;
  if (!res || tmp_value.alloc(length= (1+res->length())/2))
    goto err;

  from= res->ptr();
  tmp_value.length(length);
  to= const_cast<char*>(tmp_value.ptr());
  if (res->length() % 2)
  {
    int hex_char= hexchar_to_int(*from++);
    if (hex_char == -1)
      goto err;
    *to++= static_cast<char>(hex_char);
  }
  for (end= res->ptr() + res->length(); from < end ; from+= 2, to++)
  {
    int hex_char= hexchar_to_int(from[0]);
    if (hex_char == -1)
      goto err;
    *to= static_cast<char>(hex_char << 4);
    hex_char= hexchar_to_int(from[1]);
    if (hex_char == -1)
      goto err;
    *to|= hex_char;
  }
  null_value= false;
  return &tmp_value;

err:
  char buf[256];
  String err(buf, sizeof(buf), system_charset_info);
  err.length(0);
  args[0]->print(&err, QT_NO_DATA_EXPANSION);
  push_warning_printf(current_thd, Sql_condition::SL_WARNING,
                      ER_WRONG_VALUE_FOR_TYPE,
                      ER_THD(current_thd, ER_WRONG_VALUE_FOR_TYPE),
                      "string", err.c_ptr_safe(), func_name());

  return NULL;
}


#ifndef DBUG_OFF
String *Item_func_like_range::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  longlong nbytes= args[1]->val_int();
  String *res= args[0]->val_str(str);
  size_t min_len, max_len;
  const CHARSET_INFO *cs= collation.collation;

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


bool Item_char_typecast::eq(const Item *item, bool binary_cmp) const
{
  if (this == item)
    return 1;
  if (item->type() != FUNC_ITEM ||
      functype() != ((Item_func*)item)->functype() ||
      strcmp(func_name(), ((Item_func*)item)->func_name()))
    return 0;

  Item_char_typecast *cast= (Item_char_typecast*)item;
  if (cast_length != cast->cast_length ||
      cast_cs     != cast->cast_cs)
    return 0;

  if (!args[0]->eq(cast->args[0], binary_cmp))
      return 0;
  return 1;
}


void Item_char_typecast::print(String *str, enum_query_type query_type)
{
  str->append(STRING_WITH_LEN("cast("));
  args[0]->print(str, query_type);
  str->append(STRING_WITH_LEN(" as char"));
  if (cast_length >= 0)
    str->append_parenthesized(cast_length);
  if (cast_cs)
  {
    str->append(STRING_WITH_LEN(" charset "));
    str->append(cast_cs->csname);
  }
  str->append(')');
}


String *Item_char_typecast::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  String *res;
  uint32 length;

  if (cast_length >= 0 &&
      ((unsigned) cast_length) > current_thd->variables.max_allowed_packet)
  {
    push_warning_printf(current_thd, Sql_condition::SL_WARNING,
			ER_WARN_ALLOWED_PACKET_OVERFLOWED,
			ER(ER_WARN_ALLOWED_PACKET_OVERFLOWED),
			cast_cs == &my_charset_bin ?
                        "cast_as_binary" : func_name(),
                        current_thd->variables.max_allowed_packet);
    null_value= 1;
    return 0;
  }

  if (!charset_conversion)
  {
    if (!(res= args[0]->val_str(str)))
    {
      null_value= 1;
      return 0;
    }
  }
  else
  {
    // Convert character set if differ
    uint dummy_errors;
    if (!(res= args[0]->val_str(str)) ||
        tmp_value.copy(res->ptr(), res->length(), from_cs,
                       cast_cs, &dummy_errors))
    {
      null_value= 1;
      return 0;
    }
    res= &tmp_value;
  }

  res->set_charset(cast_cs);

  /*
    Cut the tail if cast with length
    and the result is longer than cast length, e.g.
    CAST('string' AS CHAR(1))
  */
  if (cast_length >= 0)
  {
    if (res->length() > (length= (uint32) res->charpos(cast_length)))
    {                                           // Safe even if const arg
      char char_type[40];
      my_snprintf(char_type, sizeof(char_type), "%s(%lu)",
                  cast_cs == &my_charset_bin ? "BINARY" : "CHAR",
                  (ulong) length);

      if (!res->alloced_length())
      {                                         // Don't change const str
        str_value= *res;                        // Not malloced string
        res= &str_value;
      }
      ErrConvString err(res);
      push_warning_printf(current_thd, Sql_condition::SL_WARNING,
                          ER_TRUNCATED_WRONG_VALUE,
                          ER(ER_TRUNCATED_WRONG_VALUE), char_type,
                          err.ptr());
      res->length(length);
    }
    else if (cast_cs == &my_charset_bin && res->length() < (uint) cast_length)
    {
      if (res->alloced_length() < (uint) cast_length)
      {
        str_value.alloc(cast_length);
        str_value.copy(*res);
        res= &str_value;
      }
      memset(const_cast<char*>(res->ptr() + res->length()), 0,
             cast_length - res->length());
      res->length(cast_length);
    }
  }
  null_value= 0;
  return res;
}


void Item_char_typecast::fix_length_and_dec()
{
  /*
    If we convert between two ASCII compatible character sets and the
    argument repertoire is MY_REPERTOIRE_ASCII then from_cs is set to cast_cs.
    This allows just to take over the args[0]->val_str() result
    and thus avoid unnecessary character set conversion.
  */
  from_cs= args[0]->collation.repertoire == MY_REPERTOIRE_ASCII &&
           my_charset_is_ascii_based(cast_cs) &&
           my_charset_is_ascii_based(args[0]->collation.collation) ?
           cast_cs : args[0]->collation.collation;


  collation.set(cast_cs, DERIVATION_IMPLICIT);
  fix_char_length(cast_length >= 0 ? cast_length :
                  cast_cs == &my_charset_bin ? args[0]->max_length :
                  args[0]->max_char_length());

  /* 
     We always force character set conversion if cast_cs
     is a multi-byte character set. It garantees that the
     result of CAST is a well-formed string.
     For single-byte character sets we allow just to copy from the argument.
     A single-byte character sets string is always well-formed. 
  */
  charset_conversion= (cast_cs->mbmaxlen > 1) ||
                      (!my_charset_same(from_cs, cast_cs) &&
                       from_cs != &my_charset_bin &&
                       cast_cs != &my_charset_bin);
}


void Item_func_binary::print(String *str, enum_query_type query_type)
{
  str->append(STRING_WITH_LEN("cast("));
  args[0]->print(str, query_type);
  str->append(STRING_WITH_LEN(" as binary)"));
}


bool Item_load_file::itemize(Parse_context *pc, Item **res)
{
  if (skip_itemize(res))
    return false;
  if (super::itemize(pc, res))
    return true;
  pc->thd->lex->set_stmt_unsafe(LEX::BINLOG_STMT_UNSAFE_SYSTEM_FUNCTION);
  pc->thd->lex->set_uncacheable(pc->select, UNCACHEABLE_SIDEEFFECT);
  return false;
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
      || !(current_thd->security_context()->check_access(FILE_ACL))
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
    push_warning_printf(current_thd, Sql_condition::SL_WARNING,
			ER_WARN_ALLOWED_PACKET_OVERFLOWED,
			ER(ER_WARN_ALLOWED_PACKET_OVERFLOWED),
			func_name(), current_thd->variables.max_allowed_packet);
    goto err;
  }
  if (tmp_value.alloc(stat_info.st_size))
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
  tmp_value.length(stat_info.st_size);
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

  ulonglong num_set_values = 64;
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
    num_set_values = static_cast<ulonglong>(args[4]->val_int());
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

  const ulonglong max_allowed_packet= current_thd->variables.max_allowed_packet;
  const ulonglong num_separators= num_set_values > 0 ? num_set_values - 1 : 0;
  const ulonglong max_total_length=
    num_set_values * max(yes->length(), no->length()) +
    num_separators * sep->length();

  if (unlikely(max_total_length > max_allowed_packet))
  {
    push_warning_printf(current_thd, Sql_condition::SL_WARNING,
                        ER_WARN_ALLOWED_PACKET_OVERFLOWED,
                        ER(ER_WARN_ALLOWED_PACKET_OVERFLOWED),
                        func_name(), static_cast<long>(max_allowed_packet));
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

  if (str->ptr() == NULL)
    return make_empty_result();

  return str;
}

void Item_func_export_set::fix_length_and_dec()
{
  uint32 length= max(args[1]->max_char_length(), args[2]->max_char_length());
  uint32 sep_length= (arg_count > 3 ? args[3]->max_char_length() : 1);

  if (agg_arg_charsets_for_string_result(collation,
                                         args + 1, min(4U, arg_count) - 1))
    return;
  fix_char_length(length * 64 + sep_length * 63);
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
  size_t arg_length, new_length;
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
    const CHARSET_INFO *cs= collation.collation;
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
  if (new_length > current_thd->variables.max_allowed_packet)
  {
    push_warning_printf(current_thd, Sql_condition::SL_WARNING,
                        ER_WARN_ALLOWED_PACKET_OVERFLOWED,
                        ER_THD(current_thd, ER_WARN_ALLOWED_PACKET_OVERFLOWED),
                        func_name(),
                        current_thd->variables.max_allowed_packet);
    return error_str();
  }

  tmp_value.length(new_length);
  tmp_value.set_charset(collation.collation);
  null_value= 0;
  return &tmp_value;

null:
  null_value= 1;
  return 0;
}

/**
  @returns The length that the compressed string args[0] had before
  being compressed.

  @note This function is supposed to handle this case:
  SELECT UNCOMPRESSED_LENGTH(COMPRESS(<some string>))
  However, in mysql tradition, the input argument can be *anything*.

  We return NULL if evaluation of the input argument returns NULL.
  If the input string does not look like something produced by
  Item_func_compress::val_str, we issue a warning and return 0.
 */
longlong Item_func_uncompressed_length::val_int()
{
  DBUG_ASSERT(fixed == 1);
  String *res= args[0]->val_str(&value);

  if ((null_value= args[0]->null_value))
    return 0;

  if (!res || res->is_empty())
    return 0;

  /*
    If length is <= 4 bytes, data is corrupt. This is the best we can do
    to detect garbage input without decompressing it.
  */
  if (res->length() <= 4)
  {
    push_warning(current_thd, Sql_condition::SL_WARNING,
                 ER_ZLIB_Z_DATA_ERROR, ER(ER_ZLIB_Z_DATA_ERROR));
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
  ulong new_size;
  String *res;
  Byte *body;
  char *last_char;
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
  if (((new_size+5) <= res->length()) || 
      buffer.mem_realloc(new_size + 4 + 1))
  {
    null_value= 1;
    return 0;
  }

  body= ((Byte*)buffer.ptr()) + 4;

  // As far as we have checked res->is_empty() we can use ptr()
  if ((err= compress(body, &new_size,
		     (const Bytef*)res->ptr(), res->length())) != Z_OK)
  {
    code= err==Z_MEM_ERROR ? ER_ZLIB_Z_MEM_ERROR : ER_ZLIB_Z_BUF_ERROR;
    push_warning(current_thd, Sql_condition::SL_WARNING, code, ER(code));
    null_value= 1;
    return 0;
  }

  int4store(const_cast<char*>(buffer.ptr()), res->length() & 0x3FFFFFFF);

  /* This is to ensure that things works for CHAR fields, which trim ' ': */
  last_char= ((char*)body)+new_size-1;
  if (*last_char == ' ')
  {
    *++last_char= '.';
    new_size++;
  }

  buffer.length(new_size + 4);
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
    push_warning(current_thd, Sql_condition::SL_WARNING,
                 ER_ZLIB_Z_DATA_ERROR, ER(ER_ZLIB_Z_DATA_ERROR));
    goto err;
  }

  /* Size of uncompressed data is stored as first 4 bytes of field */
  new_size= uint4korr(res->ptr()) & 0x3FFFFFFF;
  if (new_size > current_thd->variables.max_allowed_packet)
  {
    push_warning_printf(current_thd, Sql_condition::SL_WARNING,
			ER_TOO_BIG_FOR_UNCOMPRESS,
			ER(ER_TOO_BIG_FOR_UNCOMPRESS),
                        static_cast<int>(current_thd->variables.
                                         max_allowed_packet));
    goto err;
  }
  if (buffer.mem_realloc((uint32)new_size))
    goto err;

  if ((err= uncompress(pointer_cast<Byte*>(const_cast<char*>(buffer.ptr())),
                       &new_size,
                       pointer_cast<const Bytef*>(res->ptr()) + 4,
                       res->length() - 4)) == Z_OK)
  {
    buffer.length((uint32) new_size);
    return &buffer;
  }

  code= ((err == Z_BUF_ERROR) ? ER_ZLIB_Z_BUF_ERROR :
	 ((err == Z_MEM_ERROR) ? ER_ZLIB_Z_MEM_ERROR : ER_ZLIB_Z_DATA_ERROR));
  push_warning(current_thd, Sql_condition::SL_WARNING, code, ER(code));

err:
  null_value= 1;
  return 0;
}
#endif

/*
  UUID, as in
    DCE 1.1: Remote Procedure Call,
    Open Group Technical Standard Document Number C706, October 1997,
    (supersedes C309 DCE: Remote Procedure Call 8/1994,
    which was basis for ISO/IEC 11578:1996 specification)
*/

static struct rand_struct uuid_rand;
static uint nanoseq;
static ulonglong uuid_time=0;
static char clock_seq_and_node_str[]="-0000-000000000000";

/**
  number of 100-nanosecond intervals between
  1582-10-15 00:00:00.00 and 1970-01-01 00:00:00.00.
*/
#define UUID_TIME_OFFSET ((ulonglong) 141427 * 24 * 60 * 60 * \
                          1000 * 1000 * 10)

#define UUID_VERSION      0x1000
#define UUID_VARIANT      0x8000

static void tohex(char *to, uint from, uint len)
{
  to+= len;
  while (len--)
  {
    *--to= _dig_vec_lower[from & 15];
    from >>= 4;
  }
}

static void set_clock_seq_str()
{
  uint16 clock_seq= ((uint)(my_rnd(&uuid_rand)*16383)) | UUID_VARIANT;
  tohex(clock_seq_and_node_str+1, clock_seq, 4);
  nanoseq= 0;
}


bool Item_func_uuid::itemize(Parse_context *pc, Item **res)
{
  if (skip_itemize(res))
    return false;
  if (super::itemize(pc, res))
    return true;
  pc->thd->lex->set_stmt_unsafe(LEX::BINLOG_STMT_UNSAFE_SYSTEM_FUNCTION);
  pc->thd->lex->safe_to_cache_query= 0;
  return false;
}


String *Item_func_uuid::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  char *s;
  THD *thd= current_thd;

  mysql_mutex_lock(&LOCK_uuid_generator);
  if (! uuid_time) /* first UUID() call. initializing data */
  {
    ulong tmp=sql_rnd_with_mutex();
    uchar mac[6];
    int i;
    if (my_gethwaddr(mac))
    {
      /* purecov: begin inspected */
      /*
        generating random "hardware addr"
        and because specs explicitly specify that it should NOT correlate
        with a clock_seq value (initialized random below), we use a separate
        randominit() here
      */
      randominit(&uuid_rand, tmp + (ulong) thd, tmp + (ulong)global_query_id);
      for (i=0; i < (int)sizeof(mac); i++)
        mac[i]=(uchar)(my_rnd(&uuid_rand)*255);
      /* purecov: end */    
    }
    s=clock_seq_and_node_str+sizeof(clock_seq_and_node_str)-1;
    for (i=sizeof(mac)-1 ; i>=0 ; i--)
    {
      *--s=_dig_vec_lower[mac[i] & 15];
      *--s=_dig_vec_lower[mac[i] >> 4];
    }
    randominit(&uuid_rand, tmp + (ulong) server_start_time,
	       tmp + (ulong) thd->status_var.bytes_sent);
    set_clock_seq_str();
  }

  ulonglong tv= my_getsystime() + UUID_TIME_OFFSET + nanoseq;

  if (likely(tv > uuid_time))
  {
    /*
      Current time is ahead of last timestamp, as it should be.
      If we "borrowed time", give it back, just as long as we
      stay ahead of the previous timestamp.
    */
    if (nanoseq)
    {
      DBUG_ASSERT((tv > uuid_time) && (nanoseq > 0));
      /*
        -1 so we won't make tv= uuid_time for nanoseq >= (tv - uuid_time)
      */
      ulong delta= min<ulong>(nanoseq, (ulong) (tv - uuid_time -1));
      tv-= delta;
      nanoseq-= delta;
    }
  }
  else
  {
    if (unlikely(tv == uuid_time))
    {
      /*
        For low-res system clocks. If several requests for UUIDs
        end up on the same tick, we add a nano-second to make them
        different.
        ( current_timestamp + nanoseq * calls_in_this_period )
        may end up > next_timestamp; this is OK. Nonetheless, we'll
        try to unwind nanoseq when we get a chance to.
        If nanoseq overflows, we'll start over with a new numberspace
        (so the if() below is needed so we can avoid the ++tv and thus
        match the follow-up if() if nanoseq overflows!).
      */
      if (likely(++nanoseq))
        ++tv;
    }

    if (unlikely(tv <= uuid_time))
    {
      /*
        If the admin changes the system clock (or due to Daylight
        Saving Time), the system clock may be turned *back* so we
        go through a period once more for which we already gave out
        UUIDs.  To avoid duplicate UUIDs despite potentially identical
        times, we make a new random component.
        We also come here if the nanoseq "borrowing" overflows.
        In either case, we throw away any nanoseq borrowing since it's
        irrelevant in the new numberspace.
      */
      set_clock_seq_str();
      tv= my_getsystime() + UUID_TIME_OFFSET;
      nanoseq= 0;
      DBUG_PRINT("uuid",("making new numberspace"));
    }
  }

  uuid_time=tv;
  mysql_mutex_unlock(&LOCK_uuid_generator);

  uint32 time_low=            (uint32) (tv & 0xFFFFFFFF);
  uint16 time_mid=            (uint16) ((tv >> 32) & 0xFFFF);
  uint16 time_hi_and_version= (uint16) ((tv >> 48) | UUID_VERSION);

  str->mem_realloc(UUID_LENGTH+1);
  str->length(UUID_LENGTH);
  str->set_charset(system_charset_info);
  s=(char *) str->ptr();
  s[8]=s[13]='-';
  tohex(s, time_low, 8);
  tohex(s+9, time_mid, 4);
  tohex(s+14, time_hi_and_version, 4);
  my_stpcpy(s+18, clock_seq_and_node_str);
  DBUG_EXECUTE_IF("force_fake_uuid",
                  my_stpcpy(s, "a2d00942-b69c-11e4-a696-0020ff6fcbe6");
                  );
  return str;
}


void Item_func_gtid_subtract::fix_length_and_dec()
{
  maybe_null= args[0]->maybe_null || args[1]->maybe_null;
  collation.set(default_charset(), DERIVATION_COERCIBLE, MY_REPERTOIRE_ASCII);
  /*
    In the worst case, the string grows after subtraction. This
    happens when a GTID in args[0] is split by a GTID in args[1],
    e.g., UUID:1-6 minus UUID:3-4 becomes UUID:1-2,5-6.  The worst
    case is UUID:1-100 minus UUID:9, where the two characters ":9" in
    args[1] yield the five characters "-8,10" in the result.
  */
  fix_char_length_ulonglong(args[0]->max_length +
                            max<ulonglong>(args[1]->max_length - 
                                           binary_log::Uuid::TEXT_LENGTH, 0) * 5 / 2);
}


String *Item_func_gtid_subtract::val_str_ascii(String *str)
{
  DBUG_ENTER("Item_func_gtid_subtract::val_str_ascii");
  String *str1, *str2;
  const char *charp1, *charp2;
  enum_return_status status;
  /*
    We must execute args[*]->val_str_ascii() before checking
    args[*]->null_value to ensure that them are updated when
    this function is executed inside a stored procedure.
  */
  if ((str1= args[0]->val_str_ascii(&buf1)) != NULL &&
      (charp1= str1->c_ptr_safe()) != NULL &&
      (str2= args[1]->val_str_ascii(&buf2)) != NULL &&
      (charp2= str2->c_ptr_safe()) != NULL &&
      !args[0]->null_value && !args[1]->null_value)
  {
    Sid_map sid_map(NULL/*no rwlock*/);
    // compute sets while holding locks
    Gtid_set set1(&sid_map, charp1, &status);
    if (status == RETURN_STATUS_OK)
    {
      Gtid_set set2(&sid_map, charp2, &status);
      size_t length;
      // subtract, save result, return result
      if (status == RETURN_STATUS_OK)
      {
        set1.remove_gtid_set(&set2);
        if (!str->mem_realloc((length= set1.get_string_length()) + 1))
        {
          null_value= false;
          set1.to_string((char *)str->ptr());
          str->length(length);
          DBUG_RETURN(str);
        }
      }
    }
  }
  null_value= true;
  DBUG_RETURN(NULL);
}
