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

String empty_string("");

uint nr_of_decimals(const char *str)
{
  if ((str=strchr(str,'.')))
  {
    const char *start= ++str;
    for (; isdigit(*str) ; str++) ;
    return (uint) (str-start);
  }
  return 0;
}

double Item_str_func::val()
{
  String *res;
  res=val_str(&str_value);
  return res ? atof(res->c_ptr()) : 0.0;
}

longlong Item_str_func::val_int()
{
  String *res;
  res=val_str(&str_value);
  return res ? strtoll(res->c_ptr(),NULL,10) : (longlong) 0;
}


String *Item_func_md5::val_str(String *str)
{
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
  char key_buff[80];
  String tmp_key_value(key_buff, sizeof(key_buff));
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
  char key_buff[80];
  String tmp_key_value(key_buff, sizeof(key_buff)), *sptr, *key;
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
	goto null;				// Error check
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
	res=str;
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
  return res;

null:
  null_value=1;
  return 0;
}


void Item_func_concat::fix_length_and_dec()
{
  max_length=0;
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
    return &empty_string;

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
  char tmp_str_buff[10];
  String tmp_sep_str(tmp_str_buff, sizeof(tmp_str_buff)),
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
    return &empty_string;

  for (i++; i < arg_count ; i++)
  {
    if (!(res2= args[i]->val_str(use_as_buff)))
      continue;					// Skip NULL

    if (res->length() + sep_str->length() + res2->length() >
	current_thd->variables.max_allowed_packet)
      goto null;				// Error check
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
  return res;

null:
  null_value=1;
  return 0;
}

void Item_func_concat_ws::split_sum_func(List<Item> &fields)
{
  if (separator->with_sum_func && separator->type() != SUM_FUNC_ITEM)
    separator->split_sum_func(fields);
  else if (separator->used_tables() || separator->type() == SUM_FUNC_ITEM)
  {
    fields.push_front(separator);
    separator= new Item_ref((Item**) fields.head_ref(), 0, separator->name);
  }  
  Item_str_func::split_sum_func(fields);
}

void Item_func_concat_ws::fix_length_and_dec()
{
  max_length=separator->max_length*(arg_count-1);
  for (uint i=0 ; i < arg_count ; i++)
    max_length+=args[i]->max_length;
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


String *Item_func_reverse::val_str(String *str)
{
  String *res = args[0]->val_str(str);
  char *ptr,*end;

  if ((null_value=args[0]->null_value))
    return 0;
  /* An empty string is a special case as the string pointer may be null */
  if (!res->length())
    return &empty_string;
  res=copy_if_not_alloced(str,res,res->length());
  ptr = (char *) res->ptr();
  end=ptr+res->length();
#ifdef USE_MB
  if (use_mb(default_charset_info) && !binary)
  {
    String tmpstr;
    tmpstr.copy(*res);
    char *tmp = (char *) tmpstr.ptr() + tmpstr.length();
    register uint32 l;
    while (ptr < end)
    {
      if ((l=my_ismbchar(default_charset_info, ptr,end)))
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
  max_length = args[0]->max_length;
}

/*
** Replace all occurences of string2 in string1 with string3.
** Don't reallocate val_str() if not needed
*/

/* TODO: Fix that this works with binary strings when using USE_MB */

String *Item_func_replace::val_str(String *str)
{
  String *res,*res2,*res3;
  int offset;
  uint from_length,to_length;
  bool alloced=0;
#ifdef USE_MB
  const char *ptr,*end,*strend,*search,*search_end;
  register uint32 l;
  bool binary_str = (args[0]->binary || args[1]->binary ||
		     !use_mb(default_charset_info));
#endif

  null_value=0;
  res=args[0]->val_str(str);
  if (args[0]->null_value)
    goto null;
  res2=args[1]->val_str(&tmp_value);
  if (args[1]->null_value)
    goto null;

  if (res2->length() == 0)
    return res;
#ifndef USE_MB
  if ((offset=res->strstr(*res2)) < 0)
    return res;
#else
  offset=0;
  if (binary_str && (offset=res->strstr(*res2)) < 0)
    return res;
#endif
  if (!(res3=args[2]->val_str(&tmp_value2)))
    goto null;
  from_length= res2->length();
  to_length=   res3->length();

#ifdef USE_MB
  if (!binary_str)
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
            if (*i++ != *j++) goto skipp;
          offset= (int) (ptr-res->ptr());
          if (res->length()-from_length + to_length >
	      current_thd->variables.max_allowed_packet)
            goto null;
          if (!alloced)
          {
            alloced=1;
            res=copy_if_not_alloced(str,res,res->length()+to_length);
          }
          res->replace((uint) offset,from_length,*res3);
	  offset+=(int) to_length;
          goto redo;
        }
skipp:
        if ((l=my_ismbchar(default_charset_info, ptr,strend))) ptr+=l;
        else ++ptr;
    }
  }
  else
#endif /* USE_MB */
    do
    {
      if (res->length()-from_length + to_length >
	  current_thd->variables.max_allowed_packet)
        goto null;
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
    max_length+= max_substrs * (uint)diff;
  }
  if (max_length > MAX_BLOB_WIDTH)
  {
    max_length=MAX_BLOB_WIDTH;
    maybe_null=1;
  }
}


String *Item_func_insert::val_str(String *str)
{
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
#ifdef USE_MB
  if (use_mb(default_charset_info) && !args[0]->binary)
  {
    start=res->charpos(start);
    length=res->charpos(length,start);
  }
#endif
  if (start > res->length()+1)
    return res;					// Wrong param; skip insert
  if (length > res->length()-start)
    length=res->length()-start;
  if (res->length() - length + res2->length() >
      current_thd->variables.max_allowed_packet)
    goto null;					// OOM check
  res=copy_if_not_alloced(str,res,res->length());
  res->replace(start,length,*res2);
  return res;
null:
  null_value=1;
  return 0;
}


void Item_func_insert::fix_length_and_dec()
{
  max_length=args[0]->max_length+args[3]->max_length;
  if (max_length > MAX_BLOB_WIDTH)
  {
    max_length=MAX_BLOB_WIDTH;
    maybe_null=1;
  }
}


String *Item_func_lcase::val_str(String *str)
{
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
  String *res  =args[0]->val_str(str);
  long length  =(long) args[1]->val_int();

  if ((null_value=args[0]->null_value))
    return 0;
  if (length <= 0)
    return &empty_string;
#ifdef USE_MB
  if (use_mb(default_charset_info) && !binary)
    length = res->charpos(length);
#endif
  if (res->length() > (ulong) length)
  {						// Safe even if const arg
    if (!res->alloced_length())
    {						// Don't change const str
      str_value= *res;				// Not malloced string
      res= &str_value;
    }
    res->length((uint) length);
  }
  return res;
}


void Item_str_func::left_right_max_length()
{
  max_length=args[0]->max_length;
  if (args[1]->const_item())
  {
    int length=(int) args[1]->val_int()*default_charset_info->mbmaxlen;
    if (length <= 0)
      max_length=0;
    else
      set_if_smaller(max_length,(uint) length);
  }
}


void Item_func_left::fix_length_and_dec()
{
  left_right_max_length();
}


String *Item_func_right::val_str(String *str)
{
  String *res  =args[0]->val_str(str);
  long length  =(long) args[1]->val_int();

  if ((null_value=args[0]->null_value))
    return 0; /* purecov: inspected */
  if (length <= 0)
    return &empty_string; /* purecov: inspected */
  if (res->length() <= (uint) length)
    return res; /* purecov: inspected */
#ifdef USE_MB
  if (use_mb(default_charset_info) && !binary)
  {
    uint start=res->numchars()-(uint) length;
    if (start<=0) return res;
    start=res->charpos(start);
    tmp_value.set(*res,start,res->length()-start);
  }
  else
#endif
  {
    tmp_value.set(*res,(res->length()- (uint) length),(uint) length);
  }
  return &tmp_value;
}


void Item_func_right::fix_length_and_dec()
{
  left_right_max_length();
}


String *Item_func_substr::val_str(String *str)
{
  String *res  = args[0]->val_str(str);
  int32 start	= (int32) args[1]->val_int()-1;
  int32 length	= arg_count == 3 ? (int32) args[2]->val_int() : INT_MAX32;
  int32 tmp_length;

  if ((null_value=(args[0]->null_value || args[1]->null_value ||
		   (arg_count == 3 && args[2]->null_value))))
    return 0; /* purecov: inspected */
#ifdef USE_MB
  if (use_mb(default_charset_info) && !binary)
  {
    start=res->charpos(start);
    length=res->charpos(length,start);
  }
#endif
  if (start < 0 || (uint) start+1 > res->length() || length <= 0)
    return &empty_string;

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
    int32 length= (int32) args[2]->val_int() * default_charset_info->mbmaxlen;
    if (length <= 0)
      max_length=0; /* purecov: inspected */
    else
      set_if_smaller(max_length,(uint) length);
  }
}


String *Item_func_substr_index::val_str(String *str)
{
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
    return &empty_string;		// Wrong parameters

#ifdef USE_MB
  if (use_mb(default_charset_info) && !binary)
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
	    if (*i++ != *j++) goto skipp;
	  if (pass==0) ++n;
	  else if (!--c) break;
	  ptr+=delimeter_length;
	  continue;
	}
    skipp:
        if ((l=my_ismbchar(default_charset_info, ptr,strend))) ptr+=l;
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
  String *res  =args[0]->val_str(str);
  if ((null_value=args[0]->null_value))
    return 0;					/* purecov: inspected */
  char buff[MAX_FIELD_WIDTH];
  String tmp(buff,sizeof(buff));
  String *remove_str=args[1]->val_str(&tmp);
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
  String *res  =args[0]->val_str(str);
  if ((null_value=args[0]->null_value))
    return 0; /* purecov: inspected */
  char buff[MAX_FIELD_WIDTH];
  String tmp(buff,sizeof(buff));
  String *remove_str=args[1]->val_str(&tmp);
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
    if (use_mb(default_charset_info) && !binary)
    {
      while (ptr < end)
      {
	if ((l=my_ismbchar(default_charset_info, ptr,end))) ptr+=l,p=ptr;
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
    if (use_mb(default_charset_info) && !binary)
    {
  loop:
      while (ptr + remove_length < end)
      {
	if ((l=my_ismbchar(default_charset_info, ptr,end))) ptr+=l;
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
  String *res  =args[0]->val_str(str);
  if ((null_value=args[0]->null_value))
    return 0;					/* purecov: inspected */
  char buff[MAX_FIELD_WIDTH];
  String tmp(buff,sizeof(buff));
  String *remove_str=args[1]->val_str(&tmp);
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
  if (use_mb(default_charset_info) && !binary)
  {
    char *p=ptr;
    register uint32 l;
 loop:
    while (ptr + remove_length < end)
    {
      if ((l=my_ismbchar(default_charset_info, ptr,end))) ptr+=l;
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


String *Item_func_password::val_str(String *str)
{
  String *res  =args[0]->val_str(str);
  if ((null_value=args[0]->null_value))
    return 0;
  if (res->length() == 0)
    return &empty_string;
  make_scrambled_password(tmp_value,res->c_ptr());
  str->set(tmp_value,16);
  return str;
}

#define bin_to_ascii(c) ((c)>=38?((c)-38+'a'):(c)>=12?((c)-12+'A'):(c)+'.')

String *Item_func_encrypt::val_str(String *str)
{
  String *res  =args[0]->val_str(str);

#ifdef HAVE_CRYPT
  char salt[3],*salt_ptr;
  if ((null_value=args[0]->null_value))
    return 0;
  if (res->length() == 0)
    return &empty_string;

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
  str->set(tmp,(uint) strlen(tmp));
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
}

String *Item_func_encode::val_str(String *str)
{
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
  return res;
}

String *Item_func_decode::val_str(String *str)
{
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
  if (!current_thd->db)
    str->length(0);
  else
    str->set((const char*) current_thd->db,(uint) strlen(current_thd->db));
  return str;
}

String *Item_func_user::val_str(String *str)
{
  // TODO: make USER() replicate properly (currently it is replicated to "")
  THD *thd=current_thd;
  if (!(thd->user) || // for system threads (e.g. replication SQL thread)
      str->copy((const char*) thd->user,(uint) strlen(thd->user)) ||
      str->append('@') ||
      str->append(thd->host ? thd->host : thd->ip ? thd->ip : ""))
    return &empty_string;
  return str;
}

void Item_func_soundex::fix_length_and_dec()
{
  max_length=args[0]->max_length;
  set_if_bigger(max_length,4);
}


  /*
    If alpha, map input letter to soundex code.
    If not alpha and remove_garbage is set then skip to next char
    else return 0
    */

extern "C" {
extern const char *soundex_map;		// In mysys/static.c
}

static char get_scode(char *ptr)
{
  uchar ch=toupper(*ptr);
  if (ch < 'A' || ch > 'Z')
  {
					// Thread extended alfa (country spec)
    return '0';				// as vokal
  }
  return(soundex_map[ch-'A']);
}


String *Item_func_soundex::val_str(String *str)
{
  String *res  =args[0]->val_str(str);
  char last_ch,ch;
  if ((null_value=args[0]->null_value))
    return 0; /* purecov: inspected */

  if (tmp_value.alloc(max(res->length(),4)))
    return str; /* purecov: inspected */
  char *to= (char *) tmp_value.ptr();
  char *from= (char *) res->ptr(), *end=from+res->length();

  while (from != end && !isalpha(*from)) // Skip pre-space
    from++; /* purecov: inspected */
  if (from == end)
    return &empty_string;		// No alpha characters.
  *to++ = toupper(*from);		// Copy first letter
  last_ch = get_scode(from);		// code of the first letter
					// for the first 'double-letter check.
					// Loop on input letters until
					// end of input (null) or output
					// letter code count = 3
  for (from++ ; from < end ; from++)
  {
    if (!isalpha(*from))
      continue;
    ch=get_scode(from);
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


String *Item_func_format::val_str(String *str)
{
  double nr	=args[0]->val();
  uint32 diff,length,str_length;
  uint dec;
  if ((null_value=args[0]->null_value))
    return 0; /* purecov: inspected */
  dec= decimals ? decimals+1 : 0;
  str->set(nr,decimals);
#ifdef HAVE_ISNAN
  if (isnan(nr))
    return str;
#endif
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


void Item_func_elt::fix_length_and_dec()
{
  max_length=0;
  decimals=0;
#if MYSQL_VERSION_ID < 40100
  for (uint i= 0; i < arg_count ; i++)
#else
  for (uint i= 1; i < arg_count ; i++)
#endif
  {
    set_if_bigger(max_length,args[i]->max_length);
    set_if_bigger(decimals,args[i]->decimals);
  }
  maybe_null=1;					// NULL if wrong first arg
  with_sum_func= with_sum_func || item->with_sum_func;
  used_tables_cache|=	  item->used_tables();
  not_null_tables_cache&= item->not_null_tables();
  const_item_cache&=	  item->const_item();
}


void Item_func_elt::split_sum_func(List<Item> &fields)
{
  if (item->with_sum_func && item->type() != SUM_FUNC_ITEM)
    item->split_sum_func(fields);
  else if (item->used_tables() || item->type() == SUM_FUNC_ITEM)
  {
    fields.push_front(item);
    item= new Item_ref((Item**) fields.head_ref(), 0, item->name);
  }  
  Item_str_func::split_sum_func(fields);
}


void Item_func_elt::update_used_tables()
{
  Item_func::update_used_tables();
  item->update_used_tables();
  used_tables_cache|=item->used_tables();
  const_item_cache&=item->const_item();
}


double Item_func_elt::val()
{
  uint tmp;
  null_value=1;
  if ((tmp=(uint) item->val_int()) == 0 || tmp > arg_count)
    return 0.0;

  double result= args[tmp-1]->val();
  null_value= args[tmp-1]->null_value;
  return result;
}


longlong Item_func_elt::val_int()
{
  uint tmp;
  null_value=1;
  if ((tmp=(uint) item->val_int()) == 0 || tmp > arg_count)
    return 0;
  
  longlong result= args[tmp-1]->val_int();
  null_value= args[tmp-1]->null_value;
  return result;
}


String *Item_func_elt::val_str(String *str)
{
  uint tmp;
  null_value=1;
  if ((tmp=(uint) item->val_int()) == 0 || tmp > arg_count)
    return NULL;

  String *result= args[tmp-1]->val_str(str);
  null_value= args[tmp-1]->null_value;
  return result;
}


void Item_func_make_set::split_sum_func(List<Item> &fields)
{
  if (item->with_sum_func && item->type() != SUM_FUNC_ITEM)
    item->split_sum_func(fields);
  else if (item->used_tables() || item->type() == SUM_FUNC_ITEM)
  {
    fields.push_front(item);
    item= new Item_ref((Item**) fields.head_ref(), 0, item->name);
  }  
  Item_str_func::split_sum_func(fields);
}


void Item_func_make_set::fix_length_and_dec()
{
  max_length=arg_count-1;
  for (uint i=1 ; i < arg_count ; i++)
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
  ulonglong bits;
  bool first_found=0;
  Item **ptr=args;
  String *result=&empty_string;

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
	      return &empty_string;
	    result= &tmp_str;
	  }
	}
	else
	{
	  if (result != &tmp_str)
	  {					// Copy data to tmp_str
	    if (tmp_str.alloc(result->length()+res->length()+1) ||
		tmp_str.copy(*result))
	      return &empty_string;
	    result= &tmp_str;
	  }
	  if (tmp_str.append(',') || tmp_str.append(*res))
	    return &empty_string;
	}
      }
    }
  }
  return result;
}


String *Item_func_char::val_str(String *str)
{
  str->length(0);
  for (uint i=0 ; i < arg_count ; i++)
  {
    int32 num=(int32) args[i]->val_int();
    if (!args[i]->null_value)
#ifdef USE_MB
      if (use_mb(default_charset_info))
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
  uint length,tot_length;
  char *to;
  long count= (long) args[1]->val_int();
  String *res =args[0]->val_str(str);

  if (args[0]->null_value || args[1]->null_value)
    goto err;				// string and/or delim are null
  null_value=0;
  if (count <= 0)			// For nicer SQL code
    return &empty_string;
  if (count == 1)			// To avoid reallocs
    return res;
  length=res->length();
  // Safe length check
  if (length > current_thd->variables.max_allowed_packet/count)
    goto err;				// Probably an error
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
  if (args[1]->const_item())
  {
    uint32 length= (uint32) args[1]->val_int();
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
  uint32 res_length,length_pad;
  char *to;
  const char *ptr_pad;
  int32 count= (int32) args[1]->val_int();
  String *res =args[0]->val_str(str);
  String *rpad = args[2]->val_str(&rpad_str);

  if (!res || args[1]->null_value || !rpad || count < 0)
    goto err;
  null_value=0;
  if (count <= (int32) (res_length=res->length()))
  {						// String to pad is big enough
    res->length(count);				// Shorten result if longer
    return (res);
  }
  length_pad= rpad->length();
  if ((ulong) count > current_thd->variables.max_allowed_packet ||
      args[2]->null_value || !length_pad)
    goto err;
  if (!(res= alloc_buffer(res,str,&tmp_value,count)))
    goto err;

  to= (char*) res->ptr()+res_length;
  ptr_pad=rpad->ptr();
  for (count-= res_length; (uint32) count > length_pad; count-= length_pad)
  {
    memcpy(to,ptr_pad,length_pad);
    to+= length_pad;
  }
  memcpy(to,ptr_pad,(size_t) count);
  return (res);

 err:
  null_value=1;
  return 0;
}


void Item_func_lpad::fix_length_and_dec()
{
  if (args[1]->const_item())
  {
    uint32 length= (uint32) args[1]->val_int();
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
  uint32 res_length,length_pad;
  char *to;
  const char *ptr_pad;
  ulong count= (long) args[1]->val_int();
  String *res= args[0]->val_str(str);
  String *lpad= args[2]->val_str(&lpad_str);

  if (!res || args[1]->null_value || !lpad)
    goto err;
  null_value=0;
  if (count <= (res_length=res->length()))
  {						// String to pad is big enough
    res->length(count);				// Shorten result if longer
    return (res);
  }
  length_pad= lpad->length();
  if (count > current_thd->variables.max_allowed_packet ||
      args[2]->null_value || !length_pad)
    goto err;

  if (res->alloced_length() < count)
  {
    if (str->alloced_length() >= count)
    {
      memcpy((char*) str->ptr()+(count-res_length),res->ptr(),res_length);
      res=str;
    }
    else
    {
      if (tmp_value.alloc(count))
	goto err;
      memcpy((char*) tmp_value.ptr()+(count-res_length),res->ptr(),res_length);
      res=&tmp_value;
    }
  }
  else
    bmove_upp((char*) res->ptr()+count,res->ptr()+res_length,res_length);
  res->length(count);

  to= (char*) res->ptr();
  ptr_pad= lpad->ptr();
  for (count-= res_length; count > length_pad; count-= length_pad)
  {
    memcpy(to,ptr_pad,length_pad);
    to+= length_pad;
  }
  memcpy(to,ptr_pad,(size_t) count);
  return (res);

 err:
  null_value=1;
  return 0;
}


String *Item_func_conv::val_str(String *str)
{
  String *res= args[0]->val_str(str);
  char *endptr,ans[65],*ptr;
  longlong dec;
  int from_base= (int) args[1]->val_int();
  int to_base= (int) args[2]->val_int();

  if (args[0]->null_value || args[1]->null_value || args[2]->null_value ||
      abs(to_base) > 36 || abs(to_base) < 2 ||
      abs(from_base) > 36 || abs(from_base) < 2 || !(res->length()))
  {
    null_value=1;
    return 0;
  }
  null_value=0;
  if (from_base < 0)
    dec= strtoll(res->c_ptr(),&endptr,-from_base);
  else
    dec= (longlong) strtoull(res->c_ptr(),&endptr,from_base);
  ptr= longlong2str(dec,ans,to_base);
  if (str->copy(ans,(uint32) (ptr-ans)))
    return &empty_string;
  return str;
}


String *Item_func_hex::val_str(String *str)
{
  if (args[0]->result_type() != STRING_RESULT)
  {
    /* Return hex of unsigned longlong value */
    longlong dec= args[0]->val_int();
    char ans[65],*ptr;
    if ((null_value= args[0]->null_value))
      return 0;
    ptr= longlong2str(dec,ans,16);
    if (str->copy(ans,(uint32) (ptr-ans)))
      return &empty_string;			// End of memory
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
       from != end ;
       from++, to+=2)
  {
    uint tmp=(uint) (uchar) *from;
    to[0]=_dig_vec[tmp >> 4];
    to[1]=_dig_vec[tmp & 15];
  }
  return &tmp_value;
}


#include <my_dir.h>				// For my_stat

String *Item_load_file::val_str(String *str)
{
  String *file_name;
  File file;
  MY_STAT stat_info;
  DBUG_ENTER("load_file");

  if (!(file_name= args[0]->val_str(str)) ||
      !(current_thd->master_access & FILE_ACL) ||
      !my_stat(file_name->c_ptr(), &stat_info, MYF(MY_WME)))
    goto err;
  if (!(stat_info.st_mode & S_IROTH))
  {
    /* my_error(ER_TEXTFILE_NOT_READABLE, MYF(0), file_name->c_ptr()); */
    goto err;
  }
  if (stat_info.st_size > (long) current_thd->variables.max_allowed_packet)
  {
    /* my_error(ER_TOO_LONG_STRING, MYF(0), file_name->c_ptr()); */
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
    sep_buf.set(",", 1);
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
}

String* Item_func_inet_ntoa::val_str(String* str)
{
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
  null_value= 0;
  return str;

null:
  null_value= 1;
  return 0;
}
