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


/* This file defines all string functions
** Warning: Some string functions doesn't always put and end-null on a String
** (This shouldn't be needed)
*/

#ifdef __GNUC__
#pragma implementation				// gcc: Class implementation
#endif

#include "mysql_priv.h"
#include "sql_acl.h"
#include <m_ctype.h>
#ifdef HAVE_OPENSSL
#include <openssl/des.h>
#endif /* HAVE_OPENSSL */
#include "md5.h"
#include "sha1.h"
#include "my_aes.h"
C_MODE_START
#include "../mysys/my_static.h"			// For soundex_map
C_MODE_END

String my_empty_string("",default_charset_info);

static void my_coll_agg_error(DTCollation &c1, DTCollation &c2,
                              const char *fname)
{
  my_error(ER_CANT_AGGREGATE_2COLLATIONS,MYF(0),
  	   c1.collation->name,c1.derivation_name(),
	   c2.collation->name,c2.derivation_name(),
	   fname);
}

uint nr_of_decimals(const char *str)
{
  if ((str=strchr(str,'.')))
  {
    const char *start= ++str;
    for (; my_isdigit(system_charset_info,*str) ; str++) ;
    return (uint) (str-start);
  }
  return 0;
}

double Item_str_func::val()
{
  DBUG_ASSERT(fixed == 1);
  int err;
  char buff[64];
  String *res, tmp(buff,sizeof(buff), &my_charset_bin);
  res= val_str(&tmp);
  return res ? my_strntod(res->charset(), (char*) res->ptr(),res->length(),
			  NULL, &err) : 0.0;
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


String *Item_func_md5::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  String * sptr= args[0]->val_str(str);
  if (sptr)
  {
    my_MD5_CTX context;
    unsigned char digest[16];

    null_value=0;
    my_MD5Init (&context);
    my_MD5Update (&context,(unsigned char *) sptr->ptr(), sptr->length());
    my_MD5Final (digest, &context);
    if (str->alloc(32))				// Ensure that memory is free
    {
      null_value=1;
      return 0;
    }
    sprintf((char *) str->ptr(),
	    "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
	    digest[0], digest[1], digest[2], digest[3],
	    digest[4], digest[5], digest[6], digest[7],
	    digest[8], digest[9], digest[10], digest[11],
	    digest[12], digest[13], digest[14], digest[15]);
    str->length((uint) 32);
    return str;
  }
  null_value=1;
  return 0;
}


void Item_func_md5::fix_length_and_dec()
{
   max_length=32;
}


String *Item_func_sha::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  String * sptr= args[0]->val_str(str);
  if (sptr)  /* If we got value different from NULL */
  {
    SHA1_CONTEXT context;  /* Context used to generate SHA1 hash */
    /* Temporary buffer to store 160bit digest */
    uint8 digest[SHA1_HASH_SIZE];
    sha1_reset(&context);  /* We do not have to check for error here */
    /* No need to check error as the only case would be too long message */
    sha1_input(&context,(const unsigned char *) sptr->ptr(), sptr->length());
    /* Ensure that memory is free and we got result */
    if (!( str->alloc(SHA1_HASH_SIZE*2) || (sha1_result(&context,digest))))
    {
      sprintf((char *) str->ptr(),
      "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x\
%02x%02x%02x%02x%02x%02x%02x%02x",
           digest[0], digest[1], digest[2], digest[3],
           digest[4], digest[5], digest[6], digest[7],
           digest[8], digest[9], digest[10], digest[11],
           digest[12], digest[13], digest[14], digest[15],
           digest[16], digest[17], digest[18], digest[19]);

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
   max_length=SHA1_HASH_SIZE*2; // size of hex representation of hash
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
}


/*
  Concatenate args with the following premises:
  If only one arg (which is ok), return value of arg
  Don't reallocate val_str() if not absolute necessary.
*/

String *Item_func_concat::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  String *res,*res2,*use_as_buff;
  uint i;

  null_value=0;
  if (!(res=args[0]->val_str(str)))
    goto null;
  use_as_buff= &tmp_value;
  for (i=1 ; i < arg_count ; i++)
  {
    if (res->length() == 0)
    {
      if (!(res=args[i]->val_str(str)))
	goto null;
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
      if (res->alloced_length() >= res->length()+res2->length())
      {						// Use old buffer
	res->append(*res2);
      }
      else if (str->alloced_length() >= res->length()+res2->length())
      {
	if (str == res2)
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
	if (tmp_value.alloc(max_length) ||
	    tmp_value.copy(*res) ||
	    tmp_value.append(*res2))
	  goto null;
	res= &tmp_value;
	use_as_buff=str;
      }
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
  max_length=0;

  if (agg_arg_collations(collation, args, arg_count))
    return;

  for (uint i=0 ; i < arg_count ; i++)
    max_length+=args[i]->max_length;

  if (max_length > MAX_BLOB_WIDTH)
  {
    max_length=MAX_BLOB_WIDTH;
    maybe_null=1;
  }
}

/*
  Function des_encrypt() by tonu@spam.ee & monty
  Works only if compiled with OpenSSL library support.
  This returns a binary string where first character is CHAR(128 | key-number).
  If one uses a string key key_number is 127.
  Encryption result is longer than original by formula:
  new_length= org_length + (8-(org_length % 8))+1
*/

String *Item_func_des_encrypt::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
#ifdef HAVE_OPENSSL
  DES_cblock ivec;
  struct st_des_keyblock keyblock;
  struct st_des_keyschedule keyschedule;
  const char *append_str="********";
  uint key_number, res_length, tail;
  String *res= args[0]->val_str(str);

  if ((null_value=args[0]->null_value))
    return 0;
  if ((res_length=res->length()) == 0)
    return &my_empty_string;

  if (arg_count == 1)
  {
    /* Protect against someone doing FLUSH DES_KEY_FILE */
    VOID(pthread_mutex_lock(&LOCK_des_key_file));
    keyschedule= des_keyschedule[key_number=des_default_key];
    VOID(pthread_mutex_unlock(&LOCK_des_key_file));
  }
  else if (args[1]->result_type() == INT_RESULT)
  {
    key_number= (uint) args[1]->val_int();
    if (key_number > 9)
      goto error;
    VOID(pthread_mutex_lock(&LOCK_des_key_file));
    keyschedule= des_keyschedule[key_number];
    VOID(pthread_mutex_unlock(&LOCK_des_key_file));
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

  tail=  (8-(res_length) % 8);			// 1..8 marking extra length
  res_length+=tail;
  if (tail && res->append(append_str, tail) || tmp_value.alloc(res_length+1))
    goto error;
  (*res)[res_length-1]=tail;			// save extra length
  tmp_value.length(res_length+1);
  tmp_value[0]=(char) (128 | key_number);
  // Real encryption
  bzero((char*) &ivec,sizeof(ivec));
  DES_ede3_cbc_encrypt((const uchar*) (res->ptr()),
		       (uchar*) (tmp_value.ptr()+1),
		       res_length,
		       &keyschedule.ks1,
		       &keyschedule.ks2,
		       &keyschedule.ks3,
		       &ivec, TRUE);
  return &tmp_value;

error:
#endif	/* HAVE_OPENSSL */
  null_value=1;
  return 0;
}


String *Item_func_des_decrypt::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
#ifdef HAVE_OPENSSL
  DES_key_schedule ks1, ks2, ks3;
  DES_cblock ivec;
  struct st_des_keyblock keyblock;
  struct st_des_keyschedule keyschedule;
  String *res= args[0]->val_str(str);
  uint length=res->length(),tail;

  if ((null_value=args[0]->null_value))
    return 0;
  length=res->length();
  if (length < 9 || (length % 8) != 1 || !((*res)[0] & 128))
    return res;				// Skip decryption if not encrypted

  if (arg_count == 1)			// If automatic uncompression
  {
    uint key_number=(uint) (*res)[0] & 127;
    // Check if automatic key and that we have privilege to uncompress using it
    if (!(current_thd->master_access & SUPER_ACL) || key_number > 9)
      goto error;
    VOID(pthread_mutex_lock(&LOCK_des_key_file));
    keyschedule= des_keyschedule[key_number];
    VOID(pthread_mutex_unlock(&LOCK_des_key_file));
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
    goto error;					// Wrong key
  tmp_value.length(length-1-tail);
  return &tmp_value;

error:
#endif	/* HAVE_OPENSSL */
  null_value=1;
  return 0;
}


/*
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

  null_value=0;
  if (!(sep_str= separator->val_str(&tmp_sep_str)))
    goto null;

  use_as_buff= &tmp_value;
  str->length(0);				// QQ; Should be removed
  res=str;

  // Skip until non-null argument is found.
  // If not, return the empty string
  for (i=0; i < arg_count; i++)
    if ((res= args[i]->val_str(str)))
      break;
  if (i ==  arg_count)
    return &my_empty_string;

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
    if (res->alloced_length() >=
	res->length() + sep_str->length() + res2->length())
    {						// Use old buffer
      res->append(*sep_str);			// res->length() > 0 always
      res->append(*res2);
    }
    else if (str->alloced_length() >=
	     res->length() + sep_str->length() + res2->length())
    {
      /* We have room in str;  We can't get any errors here */
      if (str == res2)
      {						// This is quote uncommon!
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
      if (tmp_value.alloc(max_length) ||
	  tmp_value.copy(*res) ||
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

void Item_func_concat_ws::split_sum_func(THD *thd, Item **ref_pointer_array,
					 List<Item> &fields)
{
  if (separator->with_sum_func && separator->type() != SUM_FUNC_ITEM)
    separator->split_sum_func(thd, ref_pointer_array, fields);
  else if (separator->used_tables() || separator->type() == SUM_FUNC_ITEM)
  {
    uint el= fields.elements;
    Item *new_item= new Item_ref(ref_pointer_array + el, 0, separator->name);
    fields.push_front(separator);
    ref_pointer_array[el]= separator;
    thd->change_item_tree(&separator, new_item);
  }
  Item_str_func::split_sum_func(thd, ref_pointer_array, fields);
}

void Item_func_concat_ws::fix_length_and_dec()
{
  collation.set(separator->collation);
  max_length=separator->max_length*(arg_count-1);
  for (uint i=0 ; i < arg_count ; i++)
  {
    DTCollation tmp(collation.collation, collation.derivation);
    max_length+=args[i]->max_length;
    if (collation.aggregate(args[i]->collation))
    {
      collation.set(tmp); // Restore the previous value
      my_coll_agg_error(collation, args[i]->collation, func_name());
      break;
    }
  }
  if (max_length > MAX_BLOB_WIDTH)
  {
    max_length=MAX_BLOB_WIDTH;
    maybe_null=1;
  }
  used_tables_cache|=     separator->used_tables();
  not_null_tables_cache&= separator->not_null_tables();
  const_item_cache&=	  separator->const_item();
  with_sum_func=	  with_sum_func || separator->with_sum_func;
}

void Item_func_concat_ws::update_used_tables()
{
  Item_func::update_used_tables();
  separator->update_used_tables();
  used_tables_cache|=separator->used_tables();
  const_item_cache&=separator->const_item();
}

void Item_func_concat_ws::print(String *str)
{
  str->append("concat_ws(", 10);
  separator->print(str);
  if (arg_count)
  {
    str->append(',');
    print_args(str, 0);
  }
  str->append(')');
}

String *Item_func_reverse::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  String *res = args[0]->val_str(str);
  char *ptr,*end;

  if ((null_value=args[0]->null_value))
    return 0;
  /* An empty string is a special case as the string pointer may be null */
  if (!res->length())
    return &my_empty_string;
  res=copy_if_not_alloced(str,res,res->length());
  ptr = (char *) res->ptr();
  end=ptr+res->length();
#ifdef USE_MB
  if (use_mb(res->charset()))
  {
    String tmpstr;
    tmpstr.copy(*res);
    char *tmp = (char *) tmpstr.ptr() + tmpstr.length();
    register uint32 l;
    while (ptr < end)
    {
      if ((l=my_ismbchar(res->charset(), ptr,end)))
        tmp-=l, memcpy(tmp,ptr,l), ptr+=l;
      else
        *--tmp=*ptr++;
    }
    memcpy((char *) res->ptr(),(char *) tmpstr.ptr(), res->length());
  }
  else
#endif /* USE_MB */
  {
    char tmp;
    while (ptr < end)
    {
      tmp=*ptr;
      *ptr++=*--end;
      *end=tmp;
    }
  }
  return res;
}


void Item_func_reverse::fix_length_and_dec()
{
  collation.set(args[0]->collation);
  max_length = args[0]->max_length;
}

/*
** Replace all occurences of string2 in string1 with string3.
** Don't reallocate val_str() if not needed
*/

/* TODO: Fix that this works with binary strings when using USE_MB */

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
    ptr=res->ptr()+offset;
    strend=res->ptr()+res->length();
    end=strend-from_length+1;
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
  max_length=args[0]->max_length;
  int diff=(int) (args[2]->max_length - args[1]->max_length);
  if (diff > 0 && args[1]->max_length)
  {						// Calculate of maxreplaces
    uint max_substrs= max_length/args[1]->max_length;
    max_length+= max_substrs * (uint) diff;
  }
  if (max_length > MAX_BLOB_WIDTH)
  {
    max_length=MAX_BLOB_WIDTH;
    maybe_null=1;
  }
  
  if (agg_arg_collations_for_comparison(collation, args, 3))
    return;
}


String *Item_func_insert::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  String *res,*res2;
  uint start,length;

  null_value=0;
  res=args[0]->val_str(str);
  res2=args[3]->val_str(&tmp_value);
  start=(uint) args[1]->val_int()-1;
  length=(uint) args[2]->val_int();
  if (args[0]->null_value || args[1]->null_value || args[2]->null_value ||
      args[3]->null_value)
    goto null; /* purecov: inspected */
  start=res->charpos(start);
  length=res->charpos(length,start);
  if (start > res->length()+1)
    return res;					// Wrong param; skip insert
  if (length > res->length()-start)
    length=res->length()-start;
  if (res->length() - length + res2->length() >
      current_thd->variables.max_allowed_packet)
  {
    push_warning_printf(current_thd, MYSQL_ERROR::WARN_LEVEL_WARN,
			ER_WARN_ALLOWED_PACKET_OVERFLOWED,
			ER(ER_WARN_ALLOWED_PACKET_OVERFLOWED),
			func_name(), current_thd->variables.max_allowed_packet);
    goto null;
  }
  res=copy_if_not_alloced(str,res,res->length());
  res->replace(start,length,*res2);
  return res;
null:
  null_value=1;
  return 0;
}


void Item_func_insert::fix_length_and_dec()
{
  if (collation.set(args[0]->collation, args[3]->collation))
  {
      my_coll_agg_error(args[0]->collation, args[3]->collation, func_name());
      return;
  }
  max_length=args[0]->max_length+args[3]->max_length;
  if (max_length > MAX_BLOB_WIDTH)
  {
    max_length=MAX_BLOB_WIDTH;
    maybe_null=1;
  }
}


String *Item_func_lcase::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  String *res;
  if (!(res=args[0]->val_str(str)))
  {
    null_value=1; /* purecov: inspected */
    return 0; /* purecov: inspected */
  }
  null_value=0;
  res=copy_if_not_alloced(str,res,res->length());
  res->casedn();
  return res;
}


String *Item_func_ucase::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  String *res;
  if (!(res=args[0]->val_str(str)))
  {
    null_value=1; /* purecov: inspected */
    return 0; /* purecov: inspected */
  }
  null_value=0;
  res=copy_if_not_alloced(str,res,res->length());
  res->caseup();
  return res;
}


String *Item_func_left::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  String *res  =args[0]->val_str(str);
  long length  =(long) args[1]->val_int();
  uint char_pos;

  if ((null_value=args[0]->null_value))
    return 0;
  if (length <= 0)
    return &my_empty_string;
  if (res->length() <= (uint) length ||
      res->length() <= (char_pos= res->charpos(length)))
    return res;
  str_value.set(*res, 0, char_pos);
  return &str_value;
}


void Item_str_func::left_right_max_length()
{
  max_length=args[0]->max_length;
  if (args[1]->const_item())
  {
    int length=(int) args[1]->val_int()*collation.collation->mbmaxlen;
    if (length <= 0)
      max_length=0;
    else
      set_if_smaller(max_length,(uint) length);
  }
}


void Item_func_left::fix_length_and_dec()
{
  collation.set(args[0]->collation);
  left_right_max_length();
}


String *Item_func_right::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  String *res  =args[0]->val_str(str);
  long length  =(long) args[1]->val_int();

  if ((null_value=args[0]->null_value))
    return 0; /* purecov: inspected */
  if (length <= 0)
    return &my_empty_string; /* purecov: inspected */
  if (res->length() <= (uint) length)
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
  collation.set(args[0]->collation);
  left_right_max_length();
}


String *Item_func_substr::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  String *res  = args[0]->val_str(str);
  int32 start	= (int32) args[1]->val_int();
  int32 length	= arg_count == 3 ? (int32) args[2]->val_int() : INT_MAX32;
  int32 tmp_length;

  if ((null_value=(args[0]->null_value || args[1]->null_value ||
		   (arg_count == 3 && args[2]->null_value))))
    return 0; /* purecov: inspected */
  start= (int32)((start < 0) ? res->length() + start : start -1);
  start=res->charpos(start);
  length=res->charpos(length,start);
  if (start < 0 || (uint) start+1 > res->length() || length <= 0)
    return &my_empty_string;

  tmp_length=(int32) res->length()-start;
  length=min(length,tmp_length);

  if (!start && res->length() == (uint) length)
    return res;
  tmp_value.set(*res,(uint) start,(uint) length);
  return &tmp_value;
}


void Item_func_substr::fix_length_and_dec()
{
  max_length=args[0]->max_length;

  collation.set(args[0]->collation);
  if (args[1]->const_item())
  {
    int32 start=(int32) args[1]->val_int()-1;
    if (start < 0 || start >= (int32) max_length)
      max_length=0; /* purecov: inspected */
    else
      max_length-= (uint) start;
  }
  if (arg_count == 3 && args[2]->const_item())
  {
    int32 length= (int32) args[2]->val_int() * collation.collation->mbmaxlen;
    if (length <= 0)
      max_length=0; /* purecov: inspected */
    else
      set_if_smaller(max_length,(uint) length);
  }
}


void Item_func_substr_index::fix_length_and_dec()
{ 
  max_length= args[0]->max_length;

  if (agg_arg_collations_for_comparison(collation, args, 2))
    return;
}


String *Item_func_substr_index::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  String *res =args[0]->val_str(str);
  String *delimeter =args[1]->val_str(&tmp_value);
  int32 count = (int32) args[2]->val_int();
  uint offset;

  if (args[0]->null_value || args[1]->null_value || args[2]->null_value)
  {					// string and/or delim are null
    null_value=1;
    return 0;
  }
  null_value=0;
  uint delimeter_length=delimeter->length();
  if (!res->length() || !delimeter_length || !count)
    return &my_empty_string;		// Wrong parameters

  res->set_charset(collation.collation);

#ifdef USE_MB
  if (use_mb(res->charset()))
  {
    const char *ptr=res->ptr();
    const char *strend = ptr+res->length();
    const char *end=strend-delimeter_length+1;
    const char *search=delimeter->ptr();
    const char *search_end=search+delimeter_length;
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
	  ptr+=delimeter_length;
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
	  ptr+=delimeter_length;
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
      for (offset=0 ;; offset+=delimeter_length)
      {
	if ((int) (offset=res->strstr(*delimeter,offset)) < 0)
	  return res;			// Didn't find, return org string
	if (!--count)
	{
	  tmp_value.set(*res,0,offset);
	  break;
	}
      }
    }
    else
    {					// Start counting at end
      for (offset=res->length() ; ; offset-=delimeter_length-1)
      {
	if ((int) (offset=res->strrstr(*delimeter,offset)) < 0)
	  return res;			// Didn't find, return org string
	if (!++count)
	{
	  offset+=delimeter_length;
	  tmp_value.set(*res,offset,res->length()- offset);
	  break;
	}
      }
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


String *Item_func_ltrim::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  String *res  =args[0]->val_str(str);
  if ((null_value=args[0]->null_value))
    return 0;					/* purecov: inspected */
  char buff[MAX_FIELD_WIDTH];
  String tmp(buff,sizeof(buff),res->charset());
  String *remove_str= (arg_count==2) ? args[1]->val_str(&tmp) : &remove;
  uint remove_length;
  LINT_INIT(remove_length);

  if (!remove_str || (remove_length=remove_str->length()) == 0 ||
      remove_length > res->length())
    return res;

  char *ptr=(char*) res->ptr();
  char *end=ptr+res->length();
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
    return res;
  tmp_value.set(*res,(uint) (ptr - res->ptr()),(uint) (end-ptr));
  return &tmp_value;
}


String *Item_func_rtrim::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  String *res  =args[0]->val_str(str);
  if ((null_value=args[0]->null_value))
    return 0; /* purecov: inspected */
  char buff[MAX_FIELD_WIDTH];
  String tmp(buff,sizeof(buff),res->charset());
  String *remove_str= (arg_count==2) ? args[1]->val_str(&tmp) : &remove;
  uint remove_length;
  LINT_INIT(remove_length);

  if (!remove_str || (remove_length=remove_str->length()) == 0 ||
      remove_length > res->length())
    return res;

  char *ptr=(char*) res->ptr();
  char *end=ptr+res->length();
#ifdef USE_MB
  char *p=ptr;
  register uint32 l;
#endif
  if (remove_length == 1)
  {
    char chr=(*remove_str)[0];
#ifdef USE_MB
    if (use_mb(res->charset()))
    {
      while (ptr < end)
      {
	if ((l=my_ismbchar(res->charset(), ptr,end))) ptr+=l,p=ptr;
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
    if (use_mb(res->charset()))
    {
  loop:
      while (ptr + remove_length < end)
      {
	if ((l=my_ismbchar(res->charset(), ptr,end))) ptr+=l;
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
    return res;
  tmp_value.set(*res,0,(uint) (end-res->ptr()));
  return &tmp_value;
}


String *Item_func_trim::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  String *res  =args[0]->val_str(str);
  if ((null_value=args[0]->null_value))
    return 0;					/* purecov: inspected */
  char buff[MAX_FIELD_WIDTH];
  String tmp(buff,sizeof(buff),res->charset());
  String *remove_str= (arg_count==2) ? args[1]->val_str(&tmp) : &remove;
  uint remove_length;
  LINT_INIT(remove_length);

  if (!remove_str || (remove_length=remove_str->length()) == 0 ||
      remove_length > res->length())
    return res;

  char *ptr=(char*) res->ptr();
  char *end=ptr+res->length();
  const char *r_ptr=remove_str->ptr();
  while (ptr+remove_length <= end && !memcmp(ptr,r_ptr,remove_length))
    ptr+=remove_length;
#ifdef USE_MB
  if (use_mb(res->charset()))
  {
    char *p=ptr;
    register uint32 l;
 loop:
    while (ptr + remove_length < end)
    {
      if ((l=my_ismbchar(res->charset(), ptr,end))) ptr+=l;
      else ++ptr;
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
    return res;
  tmp_value.set(*res,(uint) (ptr - res->ptr()),(uint) (end-ptr));
  return &tmp_value;
}

void Item_func_trim::fix_length_and_dec()
{
  max_length= args[0]->max_length;
  if (arg_count == 1)
  {
    collation.set(args[0]->collation);
    remove.set_charset(collation.collation);
    remove.set_ascii(" ",1);
  }
  else
  if (collation.set(args[1]->collation, args[0]->collation) ||
      collation.derivation == DERIVATION_NONE)
  {
    my_coll_agg_error(args[1]->collation, args[0]->collation, func_name());
  }
}


/* Item_func_password */

String *Item_func_password::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  String *res= args[0]->val_str(str); 
  if ((null_value=args[0]->null_value))
    return 0;
  if (res->length() == 0)
    return &my_empty_string;
  make_scrambled_password(tmp_value, res->c_ptr());
  str->set(tmp_value, SCRAMBLED_PASSWORD_CHAR_LENGTH, res->charset());
  return str;
}

char *Item_func_password::alloc(THD *thd, const char *password)
{
  char *buff= (char *) thd->alloc(SCRAMBLED_PASSWORD_CHAR_LENGTH+1);
  if (buff)
    make_scrambled_password(buff, password);
  return buff;
}

/* Item_func_old_password */

String *Item_func_old_password::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  String *res= args[0]->val_str(str);
  if ((null_value=args[0]->null_value))
    return 0;
  if (res->length() == 0)
    return &my_empty_string;
  make_scrambled_password_323(tmp_value, res->c_ptr());
  str->set(tmp_value, SCRAMBLED_PASSWORD_CHAR_LENGTH_323, res->charset());
  return str;
}

char *Item_func_old_password::alloc(THD *thd, const char *password)
{
  char *buff= (char *) thd->alloc(SCRAMBLED_PASSWORD_CHAR_LENGTH_323+1);
  if (buff)
    make_scrambled_password_323(buff, password);
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
    return &my_empty_string;

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
    salt_ptr= salt_str->c_ptr();
  }
  pthread_mutex_lock(&LOCK_crypt);
  char *tmp=crypt(res->c_ptr(),salt_ptr);
  str->set(tmp,(uint) strlen(tmp),res->charset());
  str->copy();
  pthread_mutex_unlock(&LOCK_crypt);
  return str;
#else
  null_value=1;
  return 0;
#endif	/* HAVE_CRYPT */
}

void Item_func_encode::fix_length_and_dec()
{
  max_length=args[0]->max_length;
  maybe_null=args[0]->maybe_null;
  collation.set(&my_charset_bin);
}

String *Item_func_encode::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  String *res;
  if (!(res=args[0]->val_str(str)))
  {
    null_value=1; /* purecov: inspected */
    return 0; /* purecov: inspected */
  }
  null_value=0;
  res=copy_if_not_alloced(str,res,res->length());
  sql_crypt.init();
  sql_crypt.encode((char*) res->ptr(),res->length());
  res->set_charset(&my_charset_bin);
  return res;
}

String *Item_func_decode::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  String *res;
  if (!(res=args[0]->val_str(str)))
  {
    null_value=1; /* purecov: inspected */
    return 0; /* purecov: inspected */
  }
  null_value=0;
  res=copy_if_not_alloced(str,res,res->length());
  sql_crypt.init();
  sql_crypt.decode((char*) res->ptr(),res->length());
  return res;
}


String *Item_func_database::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  THD *thd= current_thd;
  if (!thd->db)
  {
    null_value= 1;
    return 0;
  }
  else
    str->copy((const char*) thd->db,(uint) strlen(thd->db),system_charset_info);
  return str;
}

// TODO: make USER() replicate properly (currently it is replicated to "")

String *Item_func_user::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  THD          *thd=current_thd;
  CHARSET_INFO *cs= system_charset_info;
  const char   *host= thd->host_or_ip;
  uint		res_length;

  // For system threads (e.g. replication SQL thread) user may be empty
  if (!thd->user)
    return &my_empty_string;
  res_length= (strlen(thd->user)+strlen(host)+2) * cs->mbmaxlen;

  if (str->alloc(res_length))
  {
    null_value=1;
    return 0;
  }
  res_length=cs->cset->snprintf(cs, (char*)str->ptr(), res_length, "%s@%s",
			  thd->user, host);
  str->length(res_length);
  str->set_charset(cs);
  return str;
}

void Item_func_soundex::fix_length_and_dec()
{
  collation.set(args[0]->collation);
  max_length=args[0]->max_length;
  set_if_bigger(max_length,4);
}


/*
  If alpha, map input letter to soundex code.
  If not alpha and remove_garbage is set then skip to next char
  else return 0
*/

static char get_scode(CHARSET_INFO *cs,char *ptr)
{
  uchar ch=my_toupper(cs,*ptr);
  if (ch < 'A' || ch > 'Z')
  {
					// Thread extended alfa (country spec)
    return '0';				// as vokal
  }
  return(soundex_map[ch-'A']);
}


String *Item_func_soundex::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  String *res  =args[0]->val_str(str);
  char last_ch,ch;
  CHARSET_INFO *cs= collation.collation;

  if ((null_value=args[0]->null_value))
    return 0; /* purecov: inspected */

  if (tmp_value.alloc(max(res->length(),4)))
    return str; /* purecov: inspected */
  char *to= (char *) tmp_value.ptr();
  char *from= (char *) res->ptr(), *end=from+res->length();
  tmp_value.set_charset(cs);
  
  while (from != end && !my_isalpha(cs,*from)) // Skip pre-space
    from++; /* purecov: inspected */
  if (from == end)
    return &my_empty_string;		// No alpha characters.
  *to++ = my_toupper(cs,*from);		// Copy first letter
  last_ch = get_scode(cs,from);		// code of the first letter
					// for the first 'double-letter check.
					// Loop on input letters until
					// end of input (null) or output
					// letter code count = 3
  for (from++ ; from < end ; from++)
  {
    if (!my_isalpha(cs,*from))
      continue;
    ch=get_scode(cs,from);
    if ((ch != '0') && (ch != last_ch)) // if not skipped or double
    {
       *to++ = ch;			// letter, copy to output
       last_ch = ch;			// save code of last input letter
    }					// for next double-letter check
  }
  for (end=(char*) tmp_value.ptr()+4 ; to < end ; to++)
    *to = '0';
  *to=0;				// end string
  tmp_value.length((uint) (to-tmp_value.ptr()));
  return &tmp_value;
}


/*
** Change a number to format '3,333,333,333.000'
** This should be 'internationalized' sometimes.
*/

Item_func_format::Item_func_format(Item *org,int dec) :Item_str_func(org)
{
  decimals=(uint) set_zone(dec,0,30);
}


/*
  TODO: This needs to be fixed for multi-byte character set where numbers
  are stored in more than one byte
*/

String *Item_func_format::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  double nr	=args[0]->val();
  uint32 diff,length,str_length;
  uint dec;
  if ((null_value=args[0]->null_value))
    return 0; /* purecov: inspected */
  dec= decimals ? decimals+1 : 0;
  /* Here default_charset() is right as this is not an automatic conversion */
  str->set(nr,decimals, default_charset());
  if (isnan(nr))
    return str;
  str_length=str->length();
  if (nr < 0)
    str_length--;				// Don't count sign

  /* We need this test to handle 'nan' values */
  if (str_length >= dec+4)
  {
    char *tmp,*pos;
    length= str->length()+(diff=(str_length- dec-1)/3);
    str= copy_if_not_alloced(&tmp_str,str,length);
    str->length(length);
    tmp= (char*) str->ptr()+length - dec-1;
    for (pos= (char*) str->ptr()+length-1; pos != tmp; pos--)
      pos[0]= pos[-(int) diff];
    while (diff)
    {
      pos[0]=pos[-(int) diff]; pos--;
      pos[0]=pos[-(int) diff]; pos--;
      pos[0]=pos[-(int) diff]; pos--;
      pos[0]=',';
      pos--;
      diff--;
    }
  }
  return str;
}


void Item_func_format::print(String *str)
{
  str->append("format(", 7);
  args[0]->print(str);
  str->append(',');  
  // my_charset_bin is good enough for numbers
  char buffer[20];
  String st(buffer, sizeof(buffer), &my_charset_bin);
  st.set((ulonglong)decimals, &my_charset_bin);
  str->append(st);
  str->append(')');
}

void Item_func_elt::fix_length_and_dec()
{
  max_length=0;
  decimals=0;

  if (agg_arg_collations(collation, args+1, arg_count-1))
    return;

  for (uint i= 1 ; i < arg_count ; i++)
  {
    set_if_bigger(max_length,args[i]->max_length);
    set_if_bigger(decimals,args[i]->decimals);
  }
  maybe_null=1;					// NULL if wrong first arg
}


double Item_func_elt::val()
{
  DBUG_ASSERT(fixed == 1);
  uint tmp;
  null_value=1;
  if ((tmp=(uint) args[0]->val_int()) == 0 || tmp >= arg_count)
    return 0.0;
  double result= args[tmp]->val();
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


void Item_func_make_set::split_sum_func(THD *thd, Item **ref_pointer_array,
					List<Item> &fields)
{
  if (item->with_sum_func && item->type() != SUM_FUNC_ITEM)
    item->split_sum_func(thd, ref_pointer_array, fields);
  else if (item->used_tables() || item->type() == SUM_FUNC_ITEM)
  {
    uint el= fields.elements;
    Item *new_item= new Item_ref(ref_pointer_array + el, 0, item->name);
    fields.push_front(item);
    ref_pointer_array[el]= item;
    thd->change_item_tree(&item, new_item);
  }
  Item_str_func::split_sum_func(thd, ref_pointer_array, fields);
}


void Item_func_make_set::fix_length_and_dec()
{
  max_length=arg_count-1;

  if (agg_arg_collations(collation, args, arg_count))
    return;
  
  for (uint i=0 ; i < arg_count ; i++)
    max_length+=args[i]->max_length;
  
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
}


String *Item_func_make_set::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  ulonglong bits;
  bool first_found=0;
  Item **ptr=args;
  String *result=&my_empty_string;

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
	      return &my_empty_string;
	    result= &tmp_str;
	  }
	}
	else
	{
	  if (result != &tmp_str)
	  {					// Copy data to tmp_str
	    if (tmp_str.alloc(result->length()+res->length()+1) ||
		tmp_str.copy(*result))
	      return &my_empty_string;
	    result= &tmp_str;
	  }
	  if (tmp_str.append(',') || tmp_str.append(*res))
	    return &my_empty_string;
	}
      }
    }
  }
  return result;
}


void Item_func_make_set::print(String *str)
{
  str->append("make_set(", 9);
  item->print(str);
  if (arg_count)
  {
    str->append(',');
    print_args(str, 0);
  }
  str->append(')');
}


String *Item_func_char::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  str->length(0);
  for (uint i=0 ; i < arg_count ; i++)
  {
    int32 num=(int32) args[i]->val_int();
    if (!args[i]->null_value)
#ifdef USE_MB
      if (use_mb(collation.collation))
      {
        if (num&0xFF000000L) {
           str->append((char)(num>>24));
           goto b2;
        } else if (num&0xFF0000L) {
b2:        str->append((char)(num>>16));
           goto b1;
        } else if (num&0xFF00L) {
b1:        str->append((char)(num>>8));
        }
      }
#endif
      str->append((char)num);
  }
  str->realloc(str->length());			// Add end 0 (for Purify)
  return str;
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
  collation.set(args[0]->collation);
  if (args[1]->const_item())
  {
    max_length=(long) (args[0]->max_length * args[1]->val_int());
    if (max_length >= MAX_BLOB_WIDTH)
    {
      max_length=MAX_BLOB_WIDTH;
      maybe_null=1;
    }
  }
  else
  {
    max_length=MAX_BLOB_WIDTH;
    maybe_null=1;
  }
}

/*
** Item_func_repeat::str is carefully written to avoid reallocs
** as much as possible at the cost of a local buffer
*/

String *Item_func_repeat::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  uint length,tot_length;
  char *to;
  long count= (long) args[1]->val_int();
  String *res =args[0]->val_str(str);

  if (args[0]->null_value || args[1]->null_value)
    goto err;				// string and/or delim are null
  null_value=0;
  if (count <= 0)			// For nicer SQL code
    return &my_empty_string;
  if (count == 1)			// To avoid reallocs
    return res;
  length=res->length();
  // Safe length check
  if (length > current_thd->variables.max_allowed_packet/count)
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
  if (collation.set(args[0]->collation, args[2]->collation))
  {
    my_coll_agg_error(args[0]->collation, args[2]->collation, func_name());
    return;
  }
  
  if (args[1]->const_item())
  {
    uint32 length= (uint32) args[1]->val_int() * collation.collation->mbmaxlen;
    max_length=max(args[0]->max_length,length);
    if (max_length >= MAX_BLOB_WIDTH)
    {
      max_length=MAX_BLOB_WIDTH;
      maybe_null=1;
    }
  }
  else
  {
    max_length=MAX_BLOB_WIDTH;
    maybe_null=1;
  }
}


String *Item_func_rpad::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  uint32 res_byte_length,res_char_length,pad_char_length,pad_byte_length;
  char *to;
  const char *ptr_pad;
  int32 count= (int32) args[1]->val_int();
  int32 byte_count= count * collation.collation->mbmaxlen;
  String *res =args[0]->val_str(str);
  String *rpad = args[2]->val_str(&rpad_str);

  if (!res || args[1]->null_value || !rpad || count < 0)
    goto err;
  null_value=0;
  if (count <= (int32) (res_char_length=res->numchars()))
  {						// String to pad is big enough
    res->length(res->charpos(count));		// Shorten result if longer
    return (res);
  }
  pad_char_length= rpad->numchars();
  if ((ulong) byte_count > current_thd->variables.max_allowed_packet)
  {
    push_warning_printf(current_thd, MYSQL_ERROR::WARN_LEVEL_WARN,
			ER_WARN_ALLOWED_PACKET_OVERFLOWED,
			ER(ER_WARN_ALLOWED_PACKET_OVERFLOWED),
			func_name(), current_thd->variables.max_allowed_packet);
    goto err;
  }
  if(args[2]->null_value || !pad_char_length)
    goto err;
  res_byte_length= res->length();	/* Must be done before alloc_buffer */
  if (!(res= alloc_buffer(res,str,&tmp_value,byte_count)))
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
    pad_byte_length= rpad->charpos(count);
    memcpy(to,ptr_pad,(size_t) pad_byte_length);
    to+= pad_byte_length;
  }
  res->length(to- (char*) res->ptr());
  return (res);

 err:
  null_value=1;
  return 0;
}


void Item_func_lpad::fix_length_and_dec()
{
  if (collation.set(args[0]->collation, args[2]->collation))
  {
    my_coll_agg_error(args[0]->collation, args[2]->collation, func_name());
    return;
  }
  
  if (args[1]->const_item())
  {
    uint32 length= (uint32) args[1]->val_int() * collation.collation->mbmaxlen;
    max_length=max(args[0]->max_length,length);
    if (max_length >= MAX_BLOB_WIDTH)
    {
      max_length=MAX_BLOB_WIDTH;
      maybe_null=1;
    }
  }
  else
  {
    max_length=MAX_BLOB_WIDTH;
    maybe_null=1;
  }
}


String *Item_func_lpad::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  uint32 res_char_length,pad_char_length;
  ulong count= (long) args[1]->val_int(), byte_count;
  String *res= args[0]->val_str(&tmp_value);
  String *pad= args[2]->val_str(&lpad_str);

  if (!res || args[1]->null_value || !pad)
    goto err;

  null_value=0;
  res_char_length= res->numchars();

  if (count <= res_char_length)
  {
    res->length(res->charpos(count));
    return res;
  }
  
  pad_char_length= pad->numchars();
  byte_count= count * collation.collation->mbmaxlen;
  
  if (byte_count > current_thd->variables.max_allowed_packet)
  {
    push_warning_printf(current_thd, MYSQL_ERROR::WARN_LEVEL_WARN,
			ER_WARN_ALLOWED_PACKET_OVERFLOWED,
			ER(ER_WARN_ALLOWED_PACKET_OVERFLOWED),
			func_name(), current_thd->variables.max_allowed_packet);
    goto err;
  }

  if (args[2]->null_value || !pad_char_length || str->alloc(byte_count))
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
    str->append(pad->ptr(), pad->charpos(count), collation.collation);

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

  if (args[0]->null_value || args[1]->null_value || args[2]->null_value ||
      abs(to_base) > 36 || abs(to_base) < 2 ||
      abs(from_base) > 36 || abs(from_base) < 2 || !(res->length()))
  {
    null_value=1;
    return 0;
  }
  null_value=0;
  if (from_base < 0)
    dec= my_strntoll(res->charset(),res->ptr(),res->length(),-from_base,&endptr,&err);
  else
    dec= (longlong) my_strntoull(res->charset(),res->ptr(),res->length(),from_base,&endptr,&err);
  ptr= longlong2str(dec,ans,to_base);
  if (str->copy(ans,(uint32) (ptr-ans), default_charset()))
    return &my_empty_string;
  return str;
}


String *Item_func_conv_charset::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  String *arg= args[0]->val_str(str);
  if (!arg)
  {
    null_value=1;
    return 0;
  }
  null_value= str_value.copy(arg->ptr(),arg->length(),arg->charset(),
                             conv_charset);
  return null_value ? 0 : &str_value;
}

void Item_func_conv_charset::fix_length_and_dec()
{
  collation.set(conv_charset, DERIVATION_IMPLICIT);
  max_length = args[0]->max_length*conv_charset->mbmaxlen;
}

void Item_func_conv_charset::print(String *str)
{
  str->append("convert(", 8);
  args[0]->print(str);
  str->append(" using ", 7);
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
      colname,args[0]->collation.collation->csname);
    return;
  }
  collation.set(set_collation, DERIVATION_EXPLICIT);
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
      func_name() != item_func->func_name())
    return 0;
  Item_func_set_collation *item_func_sc=(Item_func_set_collation*) item;
  if (collation.collation != item_func_sc->collation.collation)
    return 0;
  for (uint i=0; i < arg_count ; i++)
    if (!args[i]->eq(item_func_sc->args[i], binary_cmp))
      return 0;
  return 1;
}

String *Item_func_charset::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  String *res = args[0]->val_str(str);

  if ((null_value=(args[0]->null_value || !res->charset())))
    return 0;
  str->copy(res->charset()->csname,strlen(res->charset()->csname),
	    &my_charset_latin1, collation.collation);
  return str;
}

String *Item_func_collation::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  String *res = args[0]->val_str(str);

  if ((null_value=(args[0]->null_value || !res->charset())))
    return 0;
  str->copy(res->charset()->name,strlen(res->charset()->name),
	    &my_charset_latin1, collation.collation);
  return str;
}


String *Item_func_hex::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  if (args[0]->result_type() != STRING_RESULT)
  {
    /* Return hex of unsigned longlong value */
    longlong dec= args[0]->val_int();
    char ans[65],*ptr;
    if ((null_value= args[0]->null_value))
      return 0;
    ptr= longlong2str(dec,ans,16);
    if (str->copy(ans,(uint32) (ptr-ans),default_charset()))
      return &my_empty_string;			// End of memory
    return str;
  }

  /* Convert given string to a hex string, character by character */
  String *res= args[0]->val_str(str);
  const char *from, *end;
  char *to;
  if (!res || tmp_value.alloc(res->length()*2))
  {
    null_value=1;
    return 0;
  }
  null_value=0;
  tmp_value.length(res->length()*2);
  for (from=res->ptr(), end=from+res->length(), to= (char*) tmp_value.ptr();
       from < end ;
       from++, to+=2)
  {
    uint tmp=(uint) (uchar) *from;
    to[0]=_dig_vec_upper[tmp >> 4];
    to[1]=_dig_vec_upper[tmp & 15];
  }
  return &tmp_value;
}

inline int hexchar_to_int(char c)
{
  if (c <= '9' && c >= '0')
    return c-'0';
  c|=32;
  if (c <= 'f' && c >= 'a')
    return c-'a'+10;
  return -1;
}


  /* Convert given hex string to a binary string */

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


void Item_func_binary::print(String *str)
{
  str->append("cast(", 5);
  args[0]->print(str);
  str->append(" as binary)", 11);
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
      || !(current_thd->master_access & FILE_ACL)
#endif
      )
    goto err;

  (void) fn_format(path, file_name->c_ptr(), mysql_real_data_home, "",
		   MY_RELATIVE_PATH | MY_UNPACK_FILENAME);

  if (!my_stat(path, &stat_info, MYF(MY_WME)))
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
  if (tmp_value.alloc(stat_info.st_size))
    goto err;
  if ((file = my_open(file_name->c_ptr(), O_RDONLY, MYF(0))) < 0)
    goto err;
  if (my_read(file, (byte*) tmp_value.ptr(), stat_info.st_size, MYF(MY_NABP)))
  {
    my_close(file, MYF(0));
    goto err;
  }
  tmp_value.length(stat_info.st_size);
  my_close(file, MYF(0));
  null_value = 0;
  return &tmp_value;

err:
  null_value = 1;
  DBUG_RETURN(0);
}


String* Item_func_export_set::val_str(String* str)
{
  DBUG_ASSERT(fixed == 1);
  ulonglong the_set = (ulonglong) args[0]->val_int();
  String yes_buf, *yes;
  yes = args[1]->val_str(&yes_buf);
  String no_buf, *no;
  no = args[2]->val_str(&no_buf);
  String *sep = NULL, sep_buf ;

  uint num_set_values = 64;
  ulonglong mask = 0x1;
  str->length(0);

  /* Check if some argument is a NULL value */
  if (args[0]->null_value || args[1]->null_value || args[2]->null_value)
  {
    null_value=1;
    return 0;
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
      null_value=1;
      return 0;
    }
    /* Fall through */
  case 4:
    if (!(sep = args[3]->val_str(&sep_buf)))	// Only true if NULL
    {
      null_value=1;
      return 0;
    }
    break;
  case 3:
    sep_buf.set(",", 1, default_charset());
    sep = &sep_buf;
  }
  null_value=0;

  for (uint i = 0; i < num_set_values; i++, mask = (mask << 1))
  {
    if (the_set & mask)
      str->append(*yes);
    else
      str->append(*no);
    if (i != num_set_values - 1)
      str->append(*sep);
  }
  return str;
}

void Item_func_export_set::fix_length_and_dec()
{
  uint length=max(args[1]->max_length,args[2]->max_length);
  uint sep_length=(arg_count > 3 ? args[3]->max_length : 1);
  max_length=length*64+sep_length*63;

  if (agg_arg_collations(collation, args+1, min(4,arg_count)-1))
    return;
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
    uint length=(n1 ? 4 : n2 ? 3 : 2);		// Remove pre-zero

    (void) str->append(num+4-length,length);
  }
  str->length(str->length()-1);			// Remove last '.';
  return str;
}


/*
  QUOTE() function returns argument string in single quotes suitable for
  using in a SQL statement.

  DESCRIPTION
    Adds a \ before all characters that needs to be escaped in a SQL string.
    We also escape '^Z' (END-OF-FILE in windows) to avoid probelms when
    running commands from a file in windows.

    This function is very useful when you want to generate SQL statements

    RETURN VALUES
    str		Quoted string
    NULL	Argument to QUOTE() was NULL or out of memory.
*/

#define get_esc_bit(mask, num) (1 & (*((mask) + ((num) >> 3))) >> ((num) & 7))

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
    goto null;
  arg_length= arg->length();
  new_length= arg_length+2; /* for beginning and ending ' signs */

  for (from= (char*) arg->ptr(), end= from + arg_length; from < end; from++)
    new_length+= get_esc_bit(escmask, (uchar) *from);

  /*
    We have to use realloc() instead of alloc() as we want to keep the
    old result in str
  */
  if (str->realloc(new_length))
    goto null;

  /*
    As 'arg' and 'str' may be the same string, we must replace characters
    from the end to the beginning
  */
  to= (char*) str->ptr() + new_length - 1;
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
  str->length(new_length);
  str->set_charset(collation.collation);
  null_value= 0;
  return str;

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
    res->ptr() using is safe because we have tested that string is not empty,
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
  char *tmp, *last_char;
  DBUG_ASSERT(fixed == 1);

  if (!(res= args[0]->val_str(str)))
  {
    null_value= 1;
    return 0;
  }
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
  if ((err= compress(body, &new_size,
		     (const Bytef*)res->ptr(), res->length())) != Z_OK)
  {
    code= err==Z_MEM_ERROR ? ER_ZLIB_Z_MEM_ERROR : ER_ZLIB_Z_BUF_ERROR;
    push_warning(current_thd,MYSQL_ERROR::WARN_LEVEL_ERROR,code,ER(code));
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
  if (res->is_empty())
    return res;

  new_size= uint4korr(res->ptr()) & 0x3FFFFFFF;
  if (new_size > current_thd->variables.max_allowed_packet)
  {
    push_warning_printf(current_thd,MYSQL_ERROR::WARN_LEVEL_ERROR,
			ER_TOO_BIG_FOR_UNCOMPRESS,
			ER(ER_TOO_BIG_FOR_UNCOMPRESS),
                        current_thd->variables.max_allowed_packet);
    goto err;
  }
  if (buffer.realloc((uint32)new_size))
    goto err;

  if ((err= uncompress((Byte*)buffer.ptr(), &new_size,
		       ((const Bytef*)res->ptr())+4,res->length())) == Z_OK)
  {
    buffer.length((uint32) new_size);
    return &buffer;
  }

  code= ((err == Z_BUF_ERROR) ? ER_ZLIB_Z_BUF_ERROR :
	 ((err == Z_MEM_ERROR) ? ER_ZLIB_Z_MEM_ERROR : ER_ZLIB_Z_DATA_ERROR));
  push_warning(current_thd,MYSQL_ERROR::WARN_LEVEL_ERROR,code,ER(code));

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

/* number of 100-nanosecond intervals between
   1582-10-15 00:00:00.00 and 1970-01-01 00:00:00.00 */
#define UUID_TIME_OFFSET ((ulonglong) 141427 * 24 * 60 * 60 * 1000 * 10 )

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

String *Item_func_uuid::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  char *s;
  pthread_mutex_lock(&LOCK_uuid_generator);
  if (! uuid_time) /* first UUID() call. initializing data */
  {
    ulong tmp=sql_rnd_with_mutex();
    uchar mac[6];
    int i;
    if (my_gethwaddr(mac))
    {
      /*
        generating random "hardware addr"
        and because specs explicitly specify that it should NOT correlate
        with a clock_seq value (initialized random below), we use a separate
        randominit() here
      */
      randominit(&uuid_rand, tmp + (ulong)current_thd, tmp + query_id);
      for (i=0; i < (int)sizeof(mac); i++)
        mac[i]=(uchar)(my_rnd(&uuid_rand)*255);
    }
    s=clock_seq_and_node_str+sizeof(clock_seq_and_node_str)-1;
    for (i=sizeof(mac)-1 ; i>=0 ; i--)
    {
      *--s=_dig_vec_lower[mac[i] & 15];
      *--s=_dig_vec_lower[mac[i] >> 4];
    }
    randominit(&uuid_rand, tmp + (ulong)start_time, tmp + bytes_sent);
    set_clock_seq_str();
  }

  ulonglong tv=my_getsystime() + UUID_TIME_OFFSET + nanoseq;
  if (unlikely(tv < uuid_time))
    set_clock_seq_str();
  else
  if (unlikely(tv == uuid_time))
  { /* special protection from low-res system clocks */
    nanoseq++;
    tv++;
  }
  else
  {
    if (nanoseq)
    {
      tv-=nanoseq;
      nanoseq=0;
    }
    DBUG_ASSERT(tv > uuid_time);
  }
  uuid_time=tv;
  pthread_mutex_unlock(&LOCK_uuid_generator);

  uint32 time_low=            (uint32) (tv & 0xFFFFFFFF);
  uint16 time_mid=            (uint16) ((tv >> 32) & 0xFFFF);
  uint16 time_hi_and_version= (uint16) ((tv >> 48) | UUID_VERSION);

  str->realloc(UUID_LENGTH+1);
  str->length(UUID_LENGTH);
  str->set_charset(system_charset_info);
  s=(char *) str->ptr();
  s[8]=s[13]='-';
  tohex(s, time_low, 8);
  tohex(s+9, time_mid, 4);
  tohex(s+14, time_hi_and_version, 4);
  strmov(s+18, clock_seq_and_node_str);
  return str;
}

