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

#include <mysql_priv.h>

#ifdef HAVE_OPENSSL
/*
 Function which loads DES keys from plaintext file
 into memory on MySQL server startup and on command
 FLUSH DES_KEYS. Blame tonu@spam.ee on bugs ;)
*/
void 
load_des_key_file(const char *file_name)
{
  FILE *file;
  int ret=0;
  char offset;
  char buf[1024];
  des_cblock ivec={0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
  st_des_keyblock keyblock;
  DBUG_ENTER("load_des_key_file");
  VOID(pthread_mutex_lock(&LOCK_open));
  DBUG_PRINT("enter",("name: %s",file_name));
  if (!(file=my_fopen(file_name,O_RDONLY,MYF(MY_WME))))
  {
    goto error_noclose;
  }
  while(!feof(file))
  {
    if ((my_fread(file, &offset, 1, MY_WME)) != 1)
      goto error_close;
    fgets(buf,sizeof(buf),file);
    int len=strlen(buf);
    if (len-->=1) 
      buf[len]='\0';
    /* We make good 24-byte (168 bit) key from given plaintext key with MD5 */
    offset-='0';
    if (offset >= 0 && offset <=9)
    {
      EVP_BytesToKey(EVP_des_ede3_cbc(),EVP_md5(),NULL,
        (uchar *)buf,
        strlen(buf),1,(uchar *)&keyblock,ivec);
      des_set_key_unchecked(&keyblock.key1,des_keyschedule[(int)offset].ks1);
      des_set_key_unchecked(&keyblock.key2,des_keyschedule[(int)offset].ks2);
      des_set_key_unchecked(&keyblock.key3,des_keyschedule[(int)offset].ks3);
    } 
    else
    {
      DBUG_PRINT("des",("wrong offset: %d",offset));
    }
  }
error_close:
  (void) my_fclose(file,MYF(MY_WME));
error_noclose:
  VOID(pthread_mutex_unlock(&LOCK_open));
  /* if (ret)
    do something; */
  DBUG_VOID_RETURN;
}

/* 
 This function is used to load right key with DES_ENCRYPT(text,integer)
*/
st_des_keyschedule *
des_key(int key)
{
  DBUG_ENTER("des_key");
  DBUG_PRINT("exit",("return: %x",&des_keyschedule[key]));
  DBUG_RETURN(&des_keyschedule[key]);
}

#endif /* HAVE_OPENSSL */

