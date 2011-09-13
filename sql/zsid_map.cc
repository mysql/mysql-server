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
#include "hash.h"


#ifdef HAVE_UGID


Sid_map::Sid_map(Checkable_rwlock *_sid_lock)
  : sid_lock(_sid_lock)
{
  DBUG_ENTER("Sid_map::Sid_map");
  my_init_dynamic_array(&_sidno_to_sid, sizeof(Node *), 8, 8);
  my_init_dynamic_array(&_sorted, sizeof(rpl_sidno), 8, 8);
  my_hash_init(&_sid_to_sidno, &my_charset_bin, 20,
               offsetof(Node, sid.bytes), Uuid::BYTE_LENGTH, NULL,
               my_free, 0);
  
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


int Sid_map::open(const char *base_filename)
{
  size_t len= strlen(base_filename);
  DBUG_ASSERT(get_max_sidno() == 0);
  DBUG_ASSERT(strlen(filename) + strlen("-sids") < FN_REFLEN);
  memcpy(filename, base_filename, len);
  strcpy(filename + len, "-sids");

  fd= my_open(file_name, O_RDWR | O_CREAT | O_BINARY, MYF(MY_WME));
  uchar sid_buf[Uuid::BYTE_LENGTH];
  int pos= 0;
  rpl_sidno sidno= 0;
  uchar type_code;
  rpl_sid sid;
  while (true)
  {
    if (my_read(fd, &type_code, 1, MYF(0)) != 1)
      goto truncate;
    if (type == 0)
    {
      // type code 0: 16-byte uuid
      if (my_read(fd, sid_buf, Uuid::BYTE_LENGTH, MYF(0)) < Uuid::BYTE_LENGTH)
        goto truncate;
      sid.copy_from(sid_buf);
      sidno++;
      if (add_node(sidno, &sid) != GS_SUCCESS)
        goto error;
      pos+= 1 + Uuid::BYTE_LENGTH;
    }
    else
    {
      // unknown type code
      if ((type & 1) == 0)
        // even type code: fatal
        goto error;
      else
      {
        // odd type code: ignorable
        ulonglong skip_len;
        ret= read_compact_unsigned(fd, &skip_len, MYF(MY_WME));
        if (ret < 0 && ret > -0x10000)
          goto truncate;
        else if (ret <= -0x10000)
          goto error;
        /// @todo: if file is truncated in middle of block, truncate file
        if (my_seek(fd, n, SEEK_CUR, MYF(MY_WME)) != 0)
          goto error;
      }
    }
  }

truncate:
  if (my_chsize(fd, pos, 0, MYF(MY_WME)) != 0)  // 1 on error, 0 on success
    goto error;
  DBUG_RETURN(0);

error:
  my_close(fd);
  fd= -1;
  DBUG_RETURN(1);
}


rpl_sidno Sid_map::add_permanent(const rpl_sid *sid, bool _flush)
{
  DBUG_ENTER("Sid_map::add_permanent");
  sid_lock->assert_some_rdlock();
  Node *node= (Node *)my_hash_search(&_sid_to_sidno, sid->bytes,
                                     rpl_sid::BYTE_LENGTH);
  if (node != NULL)
    DBUG_RETURN(node->sidno);

  sid_lock->unlock();
  sid_lock->wrlock();
  node= (Node *)my_hash_search(&_sid_to_sidno, sid->bytes,
                               rpl_sid::BYTE_LENGTH);
  if (node != NULL)
  {
    sid_lock->unlock();
    sid_lock->rdlock();
    DBUG_RETURN(node->sidno);
  }

  rpl_sidno sidno= get_max_sidno() + 1;
  enum_group_status status= add_node(sidno, sid);
  rpl_sidno ret= (status == GS_SUCCESS) ? sidno : (rpl_sidno)status;

  sid_lock->unlock();
  sid_lock->rdlock();
  DBUG_RETURN(ret);
}


enum_group_status Sid_map::add_node(rpl_sidno sidno, rpl_sid *sid)
{
  enum_group_status status= GS_ERROR_OUT_OF_MEMORY;
  node= (Node *)malloc(sizeof(Node));
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
          status= _flush ? flush() : GS_SUCCESS;
          if (status == GS_SUCCESS)
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
            DBUG_RETURN(GS_SUCCESS);
          }
          my_hash_delete(&_sid_to_sidno, (uchar *)node);
        }
        pop_dynamic(&_sorted);
      }
      pop_dynamic(&_sidno_to_sid);
    }
    free(node);
  }
  DBUG_RETURN(GS_ERROR_OUT_OF_MEMORY);
}


enum_group_status Sid_map::flush()
{
  DBUG_ENTER("Sid_map::flush()");
  /* do nothing for now */
  DBUG_RETURN(GS_SUCCESS);
}


#endif /* HAVE_UGID */
