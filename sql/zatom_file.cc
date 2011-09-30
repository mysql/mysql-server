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


#include <mysqld_error.h>
#include <my_dir.h>


const char *Atom_file::OVERWRITE_FILE_SUFFIX= ".overwrite";


enum_return_status Atom_file::open(const char *filename_arg, bool writable_arg)
{
  DBUG_ENTER("Atom_file::open(bool)");
  DBUG_ASSERT(!is_open());

  // generate filename
  size_t len= strlen(filename_arg);
  DBUG_ASSERT(len + sizeof(OVERWRITE_FILE_SUFFIX) < sizeof(filename));
  memcpy(filename, filename_arg, len + 1);
  memcpy(overwrite_filename, filename_arg, len);
  memcpy(overwrite_filename + len, OVERWRITE_FILE_SUFFIX,
         sizeof(OVERWRITE_FILE_SUFFIX + 1));

  // open file
  writable= writable_arg;
  fd= my_open(filename, (writable ? O_RDWR | O_CREAT : O_RDONLY) | O_BINARY,
              MYF(MY_WME));
  if (fd < 0)
    RETURN_REPORTED_ERROR;

  if (recover() != 0)
  {
    my_close(fd, MYF(MY_WME));
    fd= -1;
    RETURN_REPORTED_ERROR;
  }

  RETURN_OK;
}


enum_return_status Atom_file::recover()
{
  DBUG_ENTER("Atom_file::recover()");

  // open file
  ofd= my_open(overwrite_filename, O_RDONLY | O_BINARY, MYF(MY_WME));
  if (ofd < 0)
  {
    if (my_errno == ENOENT)
      // file did not exist: Atom_file was in a clean state and no
      // recovery needed.
      RETURN_OK;
    else
      // other error.
      RETURN_REPORTED_ERROR;
  }

  // check if file is empty
  MY_STAT stat;
  if (my_fstat(ofd, &stat, MYF(MY_WME)) != 0)
    // error doing stat
    goto error_close;
  if (stat.st_size == 0)
    // file has size 0, i.e., is partial
    DBUG_RETURN(rollback());

  // read and check first byte
  uchar b;
  if (my_read(ofd, &b, 1, MYF(MY_WME)) != 1)
    // error reading 1 byte
    goto error_close;
  if (b == 0)
    // file is partial
    DBUG_RETURN(rollback());
  if (b != 1 || stat.st_size < 9)
  {
    // file has invalid value or header is incomplete
    BINLOG_ERROR(("File '%.200s' has an unknown format at position %lld, "
                  "it may be corrupt.",
                  overwrite_filename, 0),
                 (ER_FILE_FORMAT, MYF(0), overwrite_filename, 0));
    goto error_close;
  }

  // read offset
  uchar buf[8];
  if (my_read(ofd, buf, sizeof(buf), MYF(MY_WME)) != sizeof(buf))
    // error reading
    goto error_close;

  DBUG_RETURN(commit(uint8korr(buf), stat.st_size - HEADER_LENGTH));

error_close:
  if (my_close(ofd, MYF(MY_WME)) == 0)
    ofd= -1;
  RETURN_REPORTED_ERROR;
}


enum_return_status Atom_file::commit(my_off_t offset, my_off_t length)
{
  DBUG_ENTER("Atom_file::commit");

  if (writable)
  {
    uchar buf[65536];
    if (my_seek(fd, offset, MY_SEEK_SET, MYF(MY_WME)) != offset)
      goto error_close;
    
    while (length > 0)
    {
      size_t chunk_length= min(length, 65536);
      if (my_read(ofd, buf, chunk_length, MYF(MY_WME)) != chunk_length)
        goto error_close;
      if (my_write(fd, buf, chunk_length, MYF(MY_WME)) != chunk_length)
        goto error_close;
      length -= chunk_length;
      offset += chunk_length;
    }

    if (my_close(ofd, MYF(MY_WME)))
      RETURN_REPORTED_ERROR;
    ofd= -1;

    if (my_chsize(fd, offset, 0, MYF(MY_WME)) ||
        my_delete(overwrite_filename, MYF(MY_WME)))
      RETURN_REPORTED_ERROR;
  }
  else
    overwrite_offset= offset;
  RETURN_OK;

error_close:
  if (my_close(ofd, MYF(MY_WME)) == 0)
    ofd= -1;
  RETURN_REPORTED_ERROR;
}


enum_return_status Atom_file::rollback()
{
  DBUG_ENTER("Atom_file::rollback");
  if (writable)
  {
    if (my_close(ofd, MYF(MY_WME)))
      RETURN_REPORTED_ERROR;
    ofd= -1;
    if (my_delete(overwrite_filename, MYF(MY_WME)))
      RETURN_REPORTED_ERROR;
  }
  RETURN_OK;
}


enum_return_status Atom_file::close()
{
  DBUG_ENTER("Atom_file::close()");
  DBUG_ASSERT(is_open());
  int ret= my_close(fd, MYF(MY_WME));
  if (my_close(ofd, MYF(MY_WME)) || ret)
    RETURN_REPORTED_ERROR;
  RETURN_OK;
}


size_t Atom_file::pread(my_off_t offset, uchar *buffer, my_off_t length) const
{
  DBUG_ENTER("Atom_file::pread");
  DBUG_ASSERT(is_open());
  if (ofd >= 0 && offset + length > overwrite_offset)
  {
    if (offset < overwrite_offset)
    {
      size_t length_1= overwrite_offset - offset;
      size_t read_bytes_1=
        my_pread(fd, buffer, length_1, offset, MYF(MY_WME));
      if (read_bytes_1 < length_1 || read_bytes_1 == MY_FILE_ERROR)
        DBUG_RETURN(read_bytes_1);
      size_t read_bytes_2=
        my_pread(fd, buffer, length - length_1, HEADER_LENGTH, MYF(MY_WME));
      DBUG_RETURN(read_bytes_2 == MY_FILE_ERROR ? read_bytes_2 :
                  read_bytes_1 + read_bytes_2);
    }
    else
      DBUG_RETURN(my_pread(ofd, buffer, length,
                           HEADER_LENGTH + offset - overwrite_offset,
                           MYF(MY_WME)));
  }
  size_t ret= my_pread(fd, buffer, length, offset, MYF(MY_WME));
  DBUG_RETURN(ret);
}


enum_return_status
Atom_file::truncate_and_append(my_off_t offset,
                               const uchar *data, my_off_t length)
{
  DBUG_ENTER("Atom_file::truncate_and_append");
  File ofd= my_open(overwrite_filename, O_WRONLY | O_BINARY | O_CREAT | O_EXCL,
                    MYF(MY_WME));
  if (ofd < 0)
    RETURN_REPORTED_ERROR;
  uchar buf[9];
  buf[0]= 0;
  int8store(buf + 1, offset);
  uchar one= 1;
  if (my_write(ofd, buf, 9, MYF(MY_WAIT_IF_FULL | MY_WME | MY_NABP)) == 9 &&
      my_write(ofd, data, length,
               MYF(MY_WAIT_IF_FULL | MY_WME | MY_NABP)) == length &&
      my_sync(ofd, MYF(MY_WME)) == 0 &&
      my_pwrite(ofd, &one, 1, 0,
                MYF(MY_WAIT_IF_FULL | MY_WME | MY_NABP)) == 1 &&
      my_sync(ofd, MYF(MY_WME)) == 0)
  {
    if (my_close(ofd, MYF(MY_WME)) == 0 &&
        my_pwrite(fd, data, length, offset,
                  MYF(MY_WAIT_IF_FULL | MY_WME | MY_NABP)) == length &&
        my_chsize(fd, offset + length, 0, MYF(MY_WME)) == 0 &&
        my_sync(fd, MYF(MY_WME)) == 0)
    {
      if (my_delete(overwrite_filename, MYF(MY_WME)) != 0)
        RETURN_REPORTED_ERROR;
      RETURN_OK;
    }
  }
  else
    my_close(ofd, MYF(MY_WME));
  my_delete(overwrite_filename, MYF(MY_WME));
  RETURN_REPORTED_ERROR;
}


#endif

