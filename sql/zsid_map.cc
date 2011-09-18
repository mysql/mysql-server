/* Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.

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


#include "zgroups.h"


#ifdef HAVE_UGID


#include "hash.h"
#include "mysqld_error.h"


Sid_map::Sid_map(Checkable_rwlock *_sid_lock)
  : sid_lock(_sid_lock), fd(-1)
{
  DBUG_ENTER("Sid_map::Sid_map");
  my_init_dynamic_array(&_sidno_to_sid, sizeof(Node *), 8, 8);
  my_init_dynamic_array(&_sorted, sizeof(rpl_sidno), 8, 8);
  my_hash_init(&_sid_to_sidno, &my_charset_bin, 20,
               offsetof(Node, sid.bytes), Uuid::BYTE_LENGTH, NULL,
               my_free, 0);
  filename[0]= 0;
  DBUG_VOID_RETURN;
}


Sid_map::~Sid_map()
{
  DBUG_ENTER("Sid_map::~Sid_map");
  delete_dynamic(&_sidno_to_sid);
  delete_dynamic(&_sorted);
  my_hash_free(&_sid_to_sidno);
  DBUG_VOID_RETURN;
}


int Sid_map::clear()
{
  DBUG_ENTER("Sid_map::clear");
  my_hash_free(&_sid_to_sidno);
  my_hash_init(&_sid_to_sidno, &my_charset_bin, 20,
               offsetof(Node, sid.bytes), Uuid::BYTE_LENGTH, NULL,
               my_free, 0);
  reset_dynamic(&_sidno_to_sid);
  reset_dynamic(&_sorted);
  if (my_chsize(fd, 0, 0, MYF(MY_WME)) != 0)  // 1 on error, 0 on success
  {
    close();
    DBUG_RETURN(1);
  }
  DBUG_RETURN(0);
}


enum_return_status Sid_map::open(const char *_filename)
{
  DBUG_ENTER("Sid_map::open(const char *)");
  if (is_open())
    RETURN_OK;
  DBUG_ASSERT(get_max_sidno() == 0);
  strcpy(filename, _filename);
  DBUG_ASSERT(strlen(_filename) < FN_REFLEN);

  fd= my_open(filename, O_RDWR | O_CREAT | O_BINARY, MYF(MY_WME));
  int pos= 0;
  rpl_sidno sidno= 0;
  uchar type_code;
  rpl_sid sid;
  // read each block in the file
  while (true)
  {
    size_t read_bytes= my_read(fd, &type_code, 1, MYF(0));
    if (read_bytes == 0)
      RETURN_OK;
    if (read_bytes == MY_FILE_ERROR)
    {
      my_error(ER_ERROR_ON_READ, MYF(0), filename, errno);
      goto error;
    }
    DBUG_ASSERT(read_bytes == 1);
    if (type_code == 0)
    {
      // type code 0: 16-byte uuid
      enum_read_status read_status= sid.read(fd, MYF(0));
      if (read_status == READ_ERROR_IO)
      {
        my_error(ER_ERROR_ON_READ, MYF(0), filename, errno);
        goto error;
      }
      if (read_status == READ_EOF || read_status == READ_TRUNCATED)
        goto truncate;
      DBUG_ASSERT(read_status == READ_OK);
      sidno++;
      if (add_node(sidno, &sid) != 0)
      {
        my_error(ER_OUT_OF_RESOURCES, MYF(0));
        goto error;
      }
      pos+= 1 + Uuid::BYTE_LENGTH;
    }
    else
    {
      // unknown type code
      if ((type_code & 1) == 0)
      {
        my_error(ER_FILE_FORMAT, MYF(0));
        // even type code: fatal
        goto error;
      }
      else
      {
        // odd type code: ignorable
        ulonglong skip_len;
        int ret= Compact_encoding::read_unsigned(fd, &skip_len, MYF(MY_WME));
        if (ret <= 0)
        {
          if (skip_len == 1)
            goto truncate;
          else
            goto error;
        }
        /// @todo: if file is truncated in middle of block, truncate file
        if (my_seek(fd, skip_len, SEEK_CUR, MYF(MY_WME)) != 0)
          goto error;
      }
    }
  }

truncate:
  if (my_chsize(fd, pos, 0, MYF(MY_WME)) != 0)  // 1 on error, 0 on success
    goto error;
  RETURN_OK;

error:
  close();
  RETURN_REPORTED_ERROR;
}


rpl_sidno Sid_map::add_permanent(const rpl_sid *sid, bool _sync)
{
  DBUG_ENTER("Sid_map::add_permanent");
  sid_lock->assert_some_rdlock();
  Node *node= (Node *)my_hash_search(&_sid_to_sidno, sid->bytes,
                                     rpl_sid::BYTE_LENGTH);
  if (node != NULL)
    DBUG_RETURN(node->sidno);

  if (fd == -1)
    DBUG_RETURN(-1);

  sid_lock->unlock();
  sid_lock->wrlock();
  rpl_sidno sidno;
  node= (Node *)my_hash_search(&_sid_to_sidno, sid->bytes,
                               rpl_sid::BYTE_LENGTH);
  if (node != NULL)
    sidno= node->sidno;
  else
  {
    sidno= get_max_sidno() + 1;
    if (add_node(sidno, sid) != 0 ||
        write_to_disk(sidno, sid) != 0 ||
        (_sync && sync() != 0))
      sidno= -1;
  }

  sid_lock->unlock();
  sid_lock->rdlock();
  DBUG_RETURN(sidno);
}


enum_return_status Sid_map::write_to_disk(rpl_sidno sidno, const rpl_sid *sid)
{
  DBUG_ENTER("Sid_map::write_to_disk");
  sid_lock->assert_some_lock();
  if (fd == -1)
  {
    my_error(ER_ERROR_ON_WRITE, MYF(0));
    RETURN_REPORTED_ERROR;
  }
  uchar type_code= 0;
  my_off_t old_pos= my_tell(fd, MYF(MY_WME));
  if (my_write(fd, &type_code, 1, MYF(MY_WME | MY_WAIT_IF_FULL)) != 1)
    RETURN_REPORTED_ERROR;
  if (sid->write(fd, MYF(MY_WME | MY_WAIT_IF_FULL)) != 0)
  {
    if (my_chsize(fd, old_pos, 0, MYF(MY_WME)) != 0)
      close(); // fatal error, sid file is corrupt
    RETURN_REPORTED_ERROR;
  }
  RETURN_OK;
}


enum_return_status Sid_map::add_node(rpl_sidno sidno, const rpl_sid *sid)
{
  DBUG_ENTER("Sid_map::add_node(rpl_sidno, const rpl_sid *)");
  sid_lock->assert_some_lock();
  Node *node= (Node *)malloc(sizeof(Node));
  if (node != NULL)
  {
    node->sidno= sidno;
    node->sid= *sid;
    if (insert_dynamic(&_sidno_to_sid, &node) == 0)
    {
      if (insert_dynamic(&_sorted, &sidno) == 0)
      {
        if (my_hash_insert(&_sid_to_sidno, (uchar *)node) == 0)
        {
          // We have added one element to the end of _sorted.  Now we
          // bubble it down to the sorted position.
          int sorted_i= sidno - 1;
          rpl_sidno *prev_sorted_p= dynamic_element(&_sorted, sorted_i,
                                                    rpl_sidno *);
          sorted_i--;
          while (sorted_i >= 0)
          {
            rpl_sidno *sorted_p= dynamic_element(&_sorted, sorted_i,
                                                 rpl_sidno *);
            const rpl_sid *other_sid= sidno_to_sid(*sorted_p);
            if (memcmp(sid->bytes, other_sid->bytes,
                       rpl_sid::BYTE_LENGTH) >= 0)
              break;
            memcpy(prev_sorted_p, sorted_p, sizeof(rpl_sidno));
            sorted_i--;
            prev_sorted_p= sorted_p;
          }
          memcpy(prev_sorted_p, &sidno, sizeof(rpl_sidno));
          RETURN_OK;
        }
        pop_dynamic(&_sorted);
      }
      pop_dynamic(&_sidno_to_sid);
    }
    free(node);
  }
  my_error(ER_OUT_OF_RESOURCES, MYF(0));
  RETURN_REPORTED_ERROR;
}


enum_return_status Sid_map::sync()
{
  DBUG_ENTER("Sid_map::flush()");
  if (my_sync(fd, MYF(MY_WME)) != 0)
  {
    close(); // this is a fatal error, file may be corrupt
    RETURN_REPORTED_ERROR;
  }
  RETURN_OK;
}


enum_return_status Sid_map::close()
{
  DBUG_ENTER("Sid_map::close()");
  int ret= my_close(fd, MYF(MY_WME));
  fd= -1;
  if (ret != 0)
    RETURN_REPORTED_ERROR;
  RETURN_OK;
}


#endif /* HAVE_UGID */
