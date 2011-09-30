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
  : sid_lock(_sid_lock), fd(-1), status(CLOSED_OK)
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


enum_return_status Sid_map::clear()
{
  DBUG_ENTER("Sid_map::clear");
  my_hash_free(&_sid_to_sidno);
  my_hash_init(&_sid_to_sidno, &my_charset_bin, 20,
               offsetof(Node, sid.bytes), Uuid::BYTE_LENGTH, NULL,
               my_free, 0);
  reset_dynamic(&_sidno_to_sid);
  reset_dynamic(&_sorted);
  if (is_open())
    PROPAGATE_REPORTED_ERROR(appender.truncate(0));
  RETURN_OK;
}


enum_return_status Sid_map::open(const char *_filename, bool writable)
{
  DBUG_ENTER("Sid_map::open(const char *)");
  if (is_open())
    RETURN_OK;
  DBUG_ASSERT(get_max_sidno() == 0);
  strcpy(filename, _filename);
  DBUG_ASSERT(strlen(_filename) < FN_REFLEN);

  fd= my_open(filename, (writable ? O_RDWR | O_CREAT : O_RDONLY) | O_BINARY,
              MYF(MY_WME));
  if (fd == -1)
  {
    status= CLOSED_ERROR;
    RETURN_REPORTED_ERROR;
  }
  File_reader reader;
  reader.set_file(fd);
  if (writable)
    appender.set_file(fd);
  my_off_t pos= 0;
  rpl_sidno sidno= 0;
  uchar type_code;
  rpl_sid sid;
  // read each block in the file
  while (true)
  {
    if (reader.tell(&pos) != RETURN_STATUS_OK)
      goto error;
    switch (Compact_coder::read_type_code(&reader, 2, 1, &type_code))
    {
    case READ_ERROR: goto error;
    case READ_TRUNCATED: goto truncate;
    case READ_EOF: goto ok;
    case READ_OK: break;
    }
    DBUG_ASSERT(type_code == 0);
    switch (sid.read(&reader))
    {
    case READ_ERROR: goto error;
    case READ_EOF: case READ_TRUNCATED: goto truncate;
    case READ_OK: break;
    }
    sidno++;
    if (add_node(sidno, &sid) != RETURN_STATUS_OK)
      goto error;
  }

truncate:
  if (writable)
    if (appender.truncate(pos) != RETURN_STATUS_OK)
      goto error;
  /*FALLTHROUGH*/
ok:
  if (writable)
    status= OPEN;
  else
  {
    close();
    status= CLOSED_OK;
  }
  RETURN_OK;

error:
  status= CLOSED_ERROR;
  close();
  RETURN_REPORTED_ERROR;
}


rpl_sidno Sid_map::add_permanent(const rpl_sid *sid, bool _sync)
{
  DBUG_ENTER("Sid_map::add_permanent");
#ifndef DBUG_OFF
  char buf[Uuid::TEXT_LENGTH + 1];
  sid->to_string(buf);
  DBUG_PRINT("info", ("SID=%s", buf));
#endif
  sid_lock->assert_some_rdlock();
  Node *node= (Node *)my_hash_search(&_sid_to_sidno, sid->bytes,
                                     rpl_sid::BYTE_LENGTH);
  if (node != NULL)
  {
    DBUG_PRINT("info", ("existed as sidno=%d", node->sidno));
    DBUG_RETURN(node->sidno);
  }

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
    if (add_node(sidno, sid) != RETURN_STATUS_OK ||
        write_to_disk(sidno, sid) != RETURN_STATUS_OK ||
        (_sync && sync() != RETURN_STATUS_OK))
      sidno= -1;
    /// @todo: remove node on write error /sven
  }

  sid_lock->unlock();
  sid_lock->rdlock();
  DBUG_RETURN(sidno);
}


enum_return_status Sid_map::write_to_disk(rpl_sidno sidno, const rpl_sid *sid)
{
  DBUG_ENTER("Sid_map::write_to_disk");
  if (status == CLOSED_OK)
    RETURN_OK;
  else if (status == CLOSED_ERROR)
  {
    /// @todo : make error message correct
    BINLOG_ERROR(("Error writing file '%-.200s' (errno: %d)",
                  appender.get_source_name(), 0),
                 (ER_ERROR_ON_WRITE, MYF(0), appender.get_source_name(), 0));
    RETURN_REPORTED_ERROR;
  }
  sid_lock->assert_some_lock();
  char buf[Uuid::TEXT_LENGTH + 1];
  sid->to_string(buf);
  DBUG_PRINT("info", ("Writing sidno=%d sid=%s\n", sidno, buf));
  if (!appender.is_open())
  {
    BINLOG_ERROR(("Error writing file '%-.200s' (errno: %d)",
                  appender.get_source_name(), errno),
                 (ER_ERROR_ON_WRITE, MYF(0), appender.get_source_name(),
                  errno));
    RETURN_REPORTED_ERROR;
  }
  my_off_t position;
  appender.tell(&position);
  DBUG_PRINT("info", ("At position %llu\n", position));
  uchar type_code= 0;
  if (appender.append(&type_code, 1) != APPEND_OK ||
      sid->append(&appender) != APPEND_OK)
  {
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
  BINLOG_ERROR(("Out of memory."), (ER_OUT_OF_RESOURCES, MYF(0)));
  RETURN_REPORTED_ERROR;
}


enum_return_status Sid_map::sync()
{
  DBUG_ENTER("Sid_map::flush()");
  if (status == CLOSED_OK)
    RETURN_OK;
  else if (status == CLOSED_ERROR)
  {
    /// @todo : make error message correct
    BINLOG_ERROR(("Error writing file '%-.200s' (errno: %d)",
                  appender.get_source_name(), 0),
                 (ER_ERROR_ON_WRITE, MYF(0), appender.get_source_name(), 0));
    RETURN_REPORTED_ERROR;
  }
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
  appender.set_file(-1);
  if (ret != 0)
  {
    status= CLOSED_ERROR;
    RETURN_REPORTED_ERROR;
  }
  status= CLOSED_OK;
  RETURN_OK;
}


#endif /* HAVE_UGID */
