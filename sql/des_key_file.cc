/* Copyright (c) 2001, 2015, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#include "my_global.h"          // HAVE_*
#include "des_key_file.h"       // st_des_keyschedule, st_des_keyblock
#include "log.h"                // sql_print_error
#include <m_ctype.h>

#include "pfs_file_provider.h"
#include "mysql/psi/mysql_file.h"

#ifdef HAVE_OPENSSL

struct st_des_keyschedule des_keyschedule[10];
uint   des_default_key;

#define des_cs	&my_charset_latin1

/**
  Load DES keys from plaintext file into
  memory on MySQL server startup and on command FLUSH DES_KEY_FILE.

  @retval
    0  ok
  @retval
    1  Error   
*/


bool
load_des_key_file(const char *file_name)
{
  bool result=1;
  File file;
  IO_CACHE io;
  DBUG_ENTER("load_des_key_file");
  DBUG_PRINT("enter",("name: %s",file_name));

  mysql_mutex_lock(&LOCK_des_key_file);
  if ((file= mysql_file_open(key_file_des_key_file, file_name,
                             O_RDONLY | O_BINARY, MYF(MY_WME))) < 0 ||
      init_io_cache(&io, file, IO_SIZE*2, READ_CACHE, 0, 0, MYF(MY_WME)))
    goto error;

  memset(des_keyschedule, 0, sizeof(struct st_des_keyschedule) * 10);
  des_default_key=15;				// Impossible key
  for (;;)
  {
    char *start, *end;
    char buf[1024], offset;
    st_des_keyblock keyblock;
    size_t length;

    if (!(length=my_b_gets(&io,buf,sizeof(buf)-1)))
      break;					// End of file
    offset=buf[0];
    if (offset >= '0' && offset <= '9')		// If ok key
    {
      offset=(char) (offset - '0');
      // Remove newline and possible other control characters
      for (start=buf+1 ; my_isspace(des_cs, *start) ; start++) ;
      end=buf+length;
      for  (end=strend(buf) ; 
            end > start && !my_isgraph(des_cs, end[-1]) ; end--) ;

      if (start != end)
      {
	DES_cblock ivec;
	memset(&ivec, 0, sizeof(ivec));
	// We make good 24-byte (168 bit) key from given plaintext key with MD5
	EVP_BytesToKey(EVP_des_ede3_cbc(),EVP_md5(),NULL,
		       (uchar *) start, (int) (end-start),1,
		       (uchar *) &keyblock,
		       ivec);
	DES_set_key_unchecked(&keyblock.key1,&(des_keyschedule[(int)offset].ks1));
	DES_set_key_unchecked(&keyblock.key2,&(des_keyschedule[(int)offset].ks2));
	DES_set_key_unchecked(&keyblock.key3,&(des_keyschedule[(int)offset].ks3));
	if (des_default_key == 15)
	  des_default_key= (uint) offset;		// use first as def.
      }
    }
    else if (offset != '#')
      sql_print_error("load_des_file:  Found wrong key_number: %c",offset);
  }
  result=0;

error:
  if (file >= 0)
  {
    mysql_file_close(file, MYF(0));
    end_io_cache(&io);
  }
  mysql_mutex_unlock(&LOCK_des_key_file);
  DBUG_RETURN(result);
}
#endif /* HAVE_OPENSSL */
