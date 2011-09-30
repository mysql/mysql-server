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


#ifndef RPL_GROUPS_H_INCLUDED
#define RPL_GROUPS_H_INCLUDED


#include <m_string.h>
#include <my_base.h>
#include <mysqld_error.h>


/*
  In the current version, enable UGID only in debug builds.  We will
  enable it fully when it is more complete.
*/
//#ifndef DBUG_OFF
/*
  The group log can only be correctly truncated if my_chsize actually
  truncates the file. So disable UGIDs on platforms that don't support
  truncate.
*/
#if defined(_WIN32) || defined(HAVE_FTRUNCATE) || defined(HAVE_CHSIZE)
#define HAVE_UGID
#endif
//#endif


/**
  Report an error from code that can be linked into either the server
  or mysqlbinlog.  There is no common error reporting mechanism, so we
  have to duplicate the error message (write it out in the source file
  for mysqlbinlog, write it in share/errmsg-utf8.txt for the server).

  @param MYSQLBINLOG_ERROR arguments to mysqlbinlog's 'error'
  function, including the function call parentheses
  @param SERVER_ERROR arguments to my_error, including the function
  call parentheses.
*/
#ifdef MYSQL_CLIENT
#define BINLOG_ERROR(MYSQLBINLOG_ERROR, SERVER_ERROR) error MYSQLBINLOG_ERROR
#else
#define BINLOG_ERROR(MYSQLBINLOG_ERROR, SERVER_ERROR) my_error SERVER_ERROR
#endif


#ifdef HAVE_UGID

//#include "mysqld.h"
//#include "sql_string.h"
#include "hash.h"
#include "lf.h"
#include "my_atomic.h"


#ifndef MYSQL_CLIENT
class String;
class Group_log;
class THD;
#endif // ifndef MYSQL_CLIENT


/// Type of SIDNO (source ID number, first component of UGID)
typedef int32 rpl_sidno;
/// Type for GNO (group number, second component of UGID)
typedef int64 rpl_gno;
/// Type of binlog_no (binlog number)
typedef int64 rpl_binlog_no;
/// Type of binlog_pos (positions in binary log)
typedef int64 rpl_binlog_pos;
/// Type of LGIC (local group identifier)
typedef int64 rpl_lgid;


/**
  Generic return type for functions that read from disk.
*/
enum enum_read_status
{
  /// Read succeeded.
  READ_OK,
  /**
    The file position was at end of file before the read. my_error
    has NOT been called.
  */
  READ_EOF,
  /**
    End of file was reached in the middle of the read.  It is not
    specified whether anything was actually read or not, or whether
    the read position has moved or not.  my_error has NOT been called.
  */
  READ_TRUNCATED,
  /**
    An error occurred when reading - either IO error or wrong file
    format or something else.  my_error has been called.
  */  
  READ_ERROR
};
#define PROPAGATE_READ_STATUS(STATUS)                                   \
  do                                                                    \
  {                                                                     \
    enum_read_status _propagate_read_status_status= (STATUS);           \
    if (_propagate_read_status_status != READ_OK)                       \
      DBUG_RETURN(_propagate_read_status_status);                       \
  } while (0)

#define PROPAGATE_READ_STATUS_NOEOF(STATUS)                             \
  do                                                                    \
  {                                                                     \
    enum_read_status _propagate_read_status_noeof_status= (STATUS);     \
    if (_propagate_read_status_noeof_status != READ_OK)                 \
      DBUG_RETURN(_propagate_read_status_noeof_status == READ_EOF ?     \
                  READ_TRUNCATED : _propagate_read_status_noeof_status); \
  } while (0)


/**
  Generic return type for functions that append to a file.
*/
enum enum_append_status
{
  /// Write succeeded.
  APPEND_OK,
  /**
    An error occurred and it is impossible to continue writing.
    Examples:
     - Part of the data has been written and it was impossible to
       truncate the file back to the previous size.
     - An IO error occurred.
    This also means that my_error has been called.
  */
  APPEND_ERROR,
  /**
    The write failed, but the state has returned to what it was before
    the write attempt so it may be possible to write more later.
    Examples:
     - The write did not change the file at all.
     - Part of the data was written, the disk got full, and the file
       was truncated back to the previous size.
    This also means that my_error has been called.
  */
  APPEND_NONE
};
#define PROPAGATE_APPEND_STATUS(STATUS)                                 \
  do                                                                    \
  {                                                                     \
    enum_append_status __propagate_append_status_status= (STATUS);      \
    if (__propagate_append_status_status != APPEND_OK)                  \
      DBUG_RETURN(__propagate_append_status_status);                    \
  } while (0)


/**
  Generic return type for many functions that can succeed or fail.

  This is used in conjuction with the macros below for functions where
  the return status either indicates "success" or "failure".  It
  provides the following features:

   - The macros can be used to conveniently propagate errors from
     called functions back to the caller.

   - If a function is expected to print an error using my_error before
     it returns an error status, then the macros assert that my_error
     has been called.

   - Does a DBUG_PRINT before returning failure.
*/
enum enum_return_status
{
  /// The function completed successfully.
  RETURN_STATUS_OK= 0,
  /// The function completed with error but did not report it.
  RETURN_STATUS_UNREPORTED_ERROR= 1,
  /// The function completed with error and has called my_error.
  RETURN_STATUS_REPORTED_ERROR= 2
};

/**
  Lowest level macro used in the PROPAGATE_* and RETURN_* macros
  below.

  If DBUG_OFF is defined, does nothing. Otherwise, if STATUS is
  RETURN_STATUS_OK, does nothing; otherwise, make a dbug printout and
  (if ALLOW_UNREPORTED==0) assert that STATUS !=
  RETURN_STATUS_UNREPORTED.

  @param STATUS The status to return.
  @param ACTION A text that describes what we are doing: either
  "Returning" or "Propagating" (used in DBUG_PRINT macros)
  @param STATUS_NAME The stringified version of the STATUS (used in
  DBUG_PRINT macros).
  @param ALLOW_UNREPORTED If false, the macro asserts that STATUS is
  not RETURN_STATUS_UNREPORTED_ERROR.
*/
#ifdef DBUG_OFF
#define __CHECK_RETURN_STATUS(STATUS, ACTION, STATUS_NAME, ALLOW_UNREPORTED)
#else
extern void check_return_status(enum_return_status status,
                                const char *action, const char *status_name,
                                int allow_unreported);
#define __CHECK_RETURN_STATUS(STATUS, ACTION, STATUS_NAME, ALLOW_UNREPORTED) \
  check_return_status(STATUS, ACTION, STATUS_NAME, ALLOW_UNREPORTED);
#endif  
/**
  Low-level macro that checks if STATUS is RETURN_STATUS_OK; if it is
  not, then RETURN_VALUE is returned.
  @see __DO_RETURN_STATUS
*/
#define __PROPAGATE_ERROR(STATUS, RETURN_VALUE, ALLOW_UNREPORTED)       \
  do                                                                    \
  {                                                                     \
    enum_return_status __propagate_error_status= STATUS;                \
    if (__propagate_error_status != RETURN_STATUS_OK) {                 \
      __CHECK_RETURN_STATUS(__propagate_error_status, "Propagating",    \
                            #STATUS, ALLOW_UNREPORTED);                 \
      DBUG_RETURN(RETURN_VALUE);                                        \
    }                                                                   \
  } while (0)
/// Low-level macro that returns STATUS. @see __DO_RETURN_STATUS
#define __RETURN_STATUS(STATUS, ALLOW_UNREPORTED)                       \
  do                                                                    \
  {                                                                     \
    enum_return_status __return_status_status= STATUS;                  \
    __CHECK_RETURN_STATUS(__return_status_status, "Returning",          \
                          #STATUS, ALLOW_UNREPORTED);                   \
    DBUG_RETURN(__return_status_status);                                \
  } while (0)
/**
  If STATUS (of type enum_return_status) returns RETURN_STATUS_OK,
  does nothing; otherwise, does a DBUG_PRINT and returns STATUS.
*/
#define PROPAGATE_ERROR(STATUS)                                 \
  __PROPAGATE_ERROR(STATUS, __propagate_error_status, true)
/**
  If STATUS (of type enum_return_status) returns RETURN_STATUS_OK,
  does nothing; otherwise asserts that STATUS ==
  RETURN_STATUS_REPORTED_ERROR, does a DBUG_PRINT, and returns STATUS.
*/
#define PROPAGATE_REPORTED_ERROR(STATUS)                        \
  __PROPAGATE_ERROR(STATUS, __propagate_error_status, false)
/**
  If STATUS (of type enum_return_status) returns RETURN_STATUS_OK,
  does nothing; otherwise asserts that STATUS ==
  RETURN_STATUS_REPORTED_ERROR, does a DBUG_PRINT, and returns 1.
*/
#define PROPAGATE_REPORTED_ERROR_INT(STATUS)    \
  __PROPAGATE_ERROR(STATUS, 1, false)
/**
  If STATUS returns something else than RETURN_STATUS_OK, does a
  DBUG_PRINT.  Then, returns STATUS.
*/
#define RETURN_STATUS(STATUS) __RETURN_STATUS(STATUS, true)
/**
  Asserts that STATUS is not RETURN_STATUS_UNREPORTED_ERROR.  Then, if
  STATUS is RETURN_STATUS_REPORTED_ERROR, does a DBUG_PRINT.  Then,
  returns STATUS.
*/
#define RETURN_REPORTED_STATUS(STATUS) __RETURN_STATUS(STATUS, false)
/// Returns RETURN_STATUS_OK.
#define RETURN_OK DBUG_RETURN(RETURN_STATUS_OK)
/// Does a DBUG_PRINT and returns RETURN_STATUS_REPORTED_ERROR.
#define RETURN_REPORTED_ERROR RETURN_STATUS(RETURN_STATUS_REPORTED_ERROR)
/// Does a DBUG_PRINT and returns RETURN_STATUS_UNREPORTED_ERROR.
#define RETURN_UNREPORTED_ERROR RETURN_STATUS(RETURN_STATUS_UNREPORTED_ERROR)


/// The maximum value of GNO
const rpl_gno MAX_GNO= LONGLONG_MAX;
/// The length of MAX_GNO when printed in decimal.
const int MAX_GNO_TEXT_LENGTH= 19;


/**
  Parse a GNO from a string.

  @param s Pointer to the string. *s will advance to the end of the
  parsed GNO, if a correct GNO is found.
  @retval GNO if a correct GNO was found.
  @retval 0 otherwise.
*/
rpl_gno parse_gno(const char **s);
/**
  Formats a GNO as a string.

  @param s The buffer.
  @param gno The GNO.
  @return Length of the generated string.
*/
int format_gno(char *s, rpl_gno gno);


class Reader
{
public:
  virtual ~Reader() {};
  /**
    Read length characters into buffer, or leave the read position
    where it is and return an error status code.

    @param length The number of bytes to read.
    @param buffer The buffer to read data into.
    @retval READ_OK Success; the read position has moved forwards.
    @retval READ_ERROR There was an error; the read position
    has not changed; my_error has been called.
    @retval READ_EOF The file ended at the read position; the read
    position has not changed; my_error has NOT been called.
    @retval READ_TRUNCATED The file ended in the middle of the block
    to be read; the read position has not changed; my_error has NOT been called.
  */
  virtual enum_read_status read(uchar *buffer, size_t length)= 0;
  /**
    Read, and if the return status is READ_EOF or READ_TRUNCATED,
    return the position to what it was before.

    @param READ_OP Arbitrary expression that performs the read
    operation.  The expression does not even have to read from the
    READER object; the only requirement is that it returns a value of
    type enum_read_status.
    @param READER Reader* object in which the read position will be
    rewound if the READ_OP returns READ_EOF or READ_TRUNCATED.
    @param REWIND_POSITION Position to rewind to if READ_OP returns
    READ_EOF or READ_TRUNCATED.  If this parameter is set to
    PREVIOUS_POSITION, the position will be rewound to where it was
    before this call.
    @param NOEOF If true, and the read operation returns READ_EOF,
    will return READ_TRUNCATED instead.  This is useful if the calling
    function needs to rewind an already succeeded read operation.
  */
#define READ_OR_REWIND(READ_OP, READER, REWIND_POSITION, NOEOF)         \
  do {                                                                  \
    Reader *_read_or_rewind_reader= (READER);                           \
    my_off_t _read_or_rewind_position= (REWIND_POSITION);               \
    if (_read_or_rewind_position == Reader::PREVIOUS_POSITION)          \
    {                                                                   \
      if (_read_or_rewind_reader->tell(&_read_or_rewind_position) !=    \
          RETURN_STATUS_OK)                                             \
        DBUG_RETURN(READ_ERROR);                                        \
    }                                                                   \
    enum_read_status _read_or_rewind_status= (READ_OP);                 \
    if (_read_or_rewind_status == READ_TRUNCATED ||                     \
        _read_or_rewind_status == READ_EOF)                             \
    {                                                                   \
      if (_read_or_rewind_reader->seek(rewind_position) != READ_OK)     \
        DBUG_RETURN(READ_ERROR);                                        \
      DBUG_RETURN((NOEOF) && _read_or_rewind_status == READ_EOF ?       \
                  READ_TRUNCATED : _read_or_rewind_status);             \
    }                                                                   \
  } while (0)
  /**
    Same as read(uchar *, size_t), but rewinds the read position to
    where it was before this call if the return status is READ_EOF or
    READ_TRUNCATED.

    @param buffer Buffer to read into.
    @param length Number of bytes to read.
    @param rewind_position If the read operation returns READ_EOF or
    READ_TRUNCATED, the read position will be rewound to this
    position.  If this parameters is omitted or equal to
    PREVIOUS_POSITION, the read position will be rewound to where it
    was before this call.
  */
  enum_read_status read_or_rewind(uchar *buffer, size_t length,
                                  my_off_t rewind_position= PREVIOUS_POSITION)
  {
    DBUG_ENTER("Reader::read_or_rewind");
    READ_OR_REWIND(read(buffer, length), this, rewind_position, false);
    DBUG_RETURN(READ_OK);
  }
  /**
    Set the read position to the given position.
    An assertion may be raised if the new position is not valid.
    @param position New position.
    @return Same as for function 'read' above.
  */
  virtual enum_read_status seek(my_off_t position)= 0;
  /**
    Get the current read position.
    @param[out] position If successful, the position will be stored here.
    @return RETURN_STATUS_OK or RETURN_STATUS_REPORTED_ERROR.
  */
  virtual enum_return_status tell(my_off_t *position) const = 0;
  /// Return the name of the resource we read from (e.g., filename).
  virtual const char *get_source_name() const= 0;
  /**
    Same as read(uchar *, size_t), but calls my_error if return status
    is READ_EOF or READ_TRUNCATED.
  */
  enum_read_status read_report(size_t length, uchar *buffer,
                               my_off_t rewind_position= NO_REWIND)
  {
    DBUG_ENTER("Reader::read_report");
    enum_read_status ret= read(buffer, length);
    if (ret != READ_OK)
    {
      if (ret == READ_EOF)
        BINLOG_ERROR(("Unexpected EOF found when reading file '%-.192s' (errno: %d)",
                      get_source_name(), my_errno),
                     (ER_UNEXPECTED_EOF, MYF(0), get_source_name(), my_errno));
      else if (ret == READ_TRUNCATED)
        BINLOG_ERROR(("File '%.200s' was truncated in the middle.",
                      get_source_name()),
                     (ER_FILE_TRUNCATED, MYF(0), get_source_name()));
    }
    DBUG_RETURN(ret);
  }
  static const my_off_t PREVIOUS_POSITION= ~(my_off_t)0;
  static const my_off_t NO_REWIND= (~(my_off_t)0) - 1;
protected:
  /**
    Reads length bytes starting at offset from the given File into
    buffer, and on success advances the read position of this Reader
    by the length.

    This auxiliary function is intended to be used by subclasses that
    use a File as (part of) the back-end.

    @param fd File descriptor to read from.
    @param buffer Buffer to write to.
    @param length Number of bytes to read.
    @param offset Offset to start reading from.
    @return READ_OK or READ_TRUNCATED or READ_EOF or READ_ERROR.
  */
  enum_read_status file_pread(File fd, uchar *buffer,
                              size_t length, my_off_t offset);
  /**
    Check if the given position is before the end of the file.

    This auxiliary function is intended to be used by subclasses that
    use a File as (part of) the back-end.  It does not actually move
    the read position (normally, subclasses of Reader use pread() and
    keep track of the reading position manually).

    @param fd File to check.
    @param old_position Current position in the file.
    @param new_position New position in the file.
    @return RETURN_STATUS_REPORTED_ERROR if there is an error or if
    the position is beyond the end of the file; otherwise
    RETURN_STATUS_OK
  */
  enum_read_status file_seek(File fd, my_off_t old_position,
                             my_off_t new_position);


#define READER_CHECK_FORMAT(READER, CONDITION)                          \
    do                                                                  \
    {                                                                   \
      if (!(CONDITION))                                                 \
      {                                                                 \
        Reader *_reader_check_format_reader= (READER);                  \
        my_off_t ofs;                                                   \
        _reader_check_format_reader->tell(&ofs);                        \
        BINLOG_ERROR(("File '%.200s' has an unknown format at position %lld, " \
                      "it may be corrupt.",                             \
                      (READER)->get_source_name(), ofs),                \
                     (ER_FILE_FORMAT, MYF(0),                           \
                      (READER)->get_source_name(), ofs));               \
        DBUG_RETURN(READ_ERROR);                                        \
      }                                                                 \
    } while (0)
};


class Appender
{
public:
  virtual ~Appender() {};
protected:
  /**
    Append the given data.
    @param buffer Data to append.
    @param length Number of bytes to append.
    @return RETURN_STATUS_OK or RETURN_STATUS_REPORTED_ERROR.
  */
  virtual enum_return_status do_append(const uchar *buffer, size_t length)= 0;
  /**
    Truncate to the given position.
    @param position Position to truncate to.
    @return RETURN_STATUS_OK or RETURN_STATUS_REPORTED_ERROR.
  */
  virtual enum_return_status do_truncate(my_off_t new_position)= 0;
  /**
    Get the current size of this Appender.
    @param[out] position If successful, the position will be stored here.
    @return RETURN_STATUS_OK or RETURN_STATUS_REPORTED_ERROR.
  */
  virtual enum_return_status do_tell(my_off_t *position) const= 0;
  /// Return the name of the resource we append to (e.g., filename).
  virtual const char *do_get_source_name() const= 0;
public:
  /**
    Try to append the given data.  If that fails, try to revert the
    state by calling truncate() with the position before this call.
    @param buffer Data to write.
    @param length Number of bytes to write.
    @param truncate_position Position to truncate the file to if the
    write fails.  If omitted or set to PREVIOUS_POSITION, the position
    before this call is used.
    @return APPEND_OK or APPEND_NONE or APPEND_ERROR.
  */
  enum_append_status append(const uchar *buffer, size_t length,
                            my_off_t truncate_position= PREVIOUS_POSITION)
  {
    DBUG_ENTER("Appender::append(const uchar *, size_t)");
    if (truncate_position == PREVIOUS_POSITION)
      if (tell(&truncate_position) != RETURN_STATUS_OK)
        DBUG_RETURN(APPEND_NONE);
    enum_return_status return_status= do_append(buffer, length);
    if (return_status != RETURN_STATUS_OK)
    {
      if (truncate(truncate_position) != RETURN_STATUS_OK)
        DBUG_RETURN(APPEND_ERROR);
      DBUG_RETURN(APPEND_NONE);
    }
    DBUG_RETURN(APPEND_OK);
  }
  /**
    Truncate the Appender to the given size.

    This checks that the position is before the current write position
    before it calls do_truncate(). If the positions are equal, does
    nothing; if the new position is past the current position, returns
    error without calling do_truncate().
    @param new_position Position to truncate to.
    @return RETURN_STATUS_OK or RETURN_STATUS_REPORTED_ERROR.
  */
  enum_return_status truncate(my_off_t new_position);
  /**
    Get the current read position.
    @param[out] position If successful, the position will be stored here.
    @return RETURN_STATUS_OK or RETURN_STATUS_REPORTED_ERROR.
  */
  virtual enum_return_status tell(my_off_t *position)
  {
    DBUG_ENTER("Appender::tell");
    PROPAGATE_REPORTED_ERROR(do_tell(position));
    RETURN_OK;
  }
  /// Return the name of the resource we append to (e.g., filename)
  const char *get_source_name() const { return do_get_source_name(); }
  /// Used in last argument of append().
  static const my_off_t PREVIOUS_POSITION= ~(my_off_t)0;
protected:
  /**
    Gets the current file position of the given file handler.

    This auxiliary function is intended to be used by sub-classes that
    use a File as (part) of the back-end.

    @param fd File descriptor to get position from.
    @param position[out] If successful, the position will be stored here.
    @return RETURN_STATUS_OK or RETURN_STATUS_REPORTED_ERROR.
  */
  enum_return_status file_tell(File fd, my_off_t *position) const
  {
    DBUG_ENTER("Appender::file_tell");
    my_off_t ret= my_tell(fd, MYF(MY_WME));
    if (ret == (my_off_t)(os_off_t)-1)
      RETURN_REPORTED_ERROR;
    *position= ret;
    RETURN_OK;
  };
  /**
    Truncates the given file to the specified position.

    This auxiliary function is intended to be used by sub-classes that
    use a File as (part) of the back-end.

    @param fd File descriptor to truncate.
    @param logical_position The logical position (as would be reported
    by tell()) to truncate to.
    @param absolute_position The file position in fd that we should
    truncate to.
    @return RETURN_STATUS_OK or RETURN_STATUS_REPORTED_ERROR.
  */
  enum_return_status file_truncate(File fd, my_off_t position)
  {
    DBUG_ENTER("Appender::file_truncate");
    if (my_chsize(fd, position, 0, MYF(MY_WME)) != 0)
      RETURN_REPORTED_ERROR;
    if (my_seek(fd, position, SEEK_SET, MYF(MY_WME)) != 0)
      RETURN_REPORTED_ERROR;
    RETURN_OK;
  }
};


class Memory_reader : public Reader
{
public:
  Memory_reader(size_t _length, const uchar *_data,
                const char *_source_name= "<Memory buffer>")
    : source_name(_source_name), data_length(_length), data(_data), pos(0) {}
  enum_read_status read(uchar *buffer, size_t length)
  {
    DBUG_ENTER("Memory_reader::read");
    if (pos + length > data_length)
      DBUG_RETURN(pos == data_length ? READ_EOF : READ_TRUNCATED);
    memcpy(buffer, data + pos, length);
    pos+= length;
    DBUG_RETURN(READ_OK);
  }
  enum_read_status seek(my_off_t new_pos)
  {
    DBUG_ENTER("Memory_reader::seek");
    if (new_pos > data_length)
      DBUG_RETURN(pos == data_length ? READ_EOF : READ_TRUNCATED);
    pos= (size_t)new_pos;
    DBUG_RETURN(READ_OK);
  }
  enum_return_status tell(my_off_t *position) const
  { *position= pos; return RETURN_STATUS_OK; }
  const char *get_source_name() const { return source_name; }
private:
  const char *source_name;
  const size_t data_length;
  const uchar *data;
  size_t pos;
};


class File_reader : public Reader
{
public:
  File_reader(File _fd= -1) : fd(_fd), pos(0) {}
  enum_return_status open(const char *filename)
  {
    DBUG_ENTER("File_reader::open");
    fd= my_open(filename, O_RDONLY, MYF(MY_WME));
    if (fd == -1)
      RETURN_REPORTED_ERROR;
    RETURN_OK;
  }
  enum_return_status close()
  {
    DBUG_ENTER("File_reader::close");
    if (my_close(fd, MYF(MY_WME)) != 0)
      RETURN_REPORTED_ERROR;
    RETURN_OK;
  }
  void set_file(File _fd= -1) { fd= _fd; pos= 0; }
  enum_read_status read(uchar *buffer, size_t length)
  {
    DBUG_ENTER("File_reader::read");
    PROPAGATE_READ_STATUS(file_pread(fd, buffer, length, pos));
    pos+= length;
    DBUG_RETURN(READ_OK);
  }
  enum_read_status seek(my_off_t new_position)
  {
    DBUG_ENTER("File_reader::seek");
    PROPAGATE_READ_STATUS(file_seek(fd, pos, new_position));
    pos= new_position;
    DBUG_RETURN(READ_OK);
  }
  enum_return_status tell(my_off_t *position) const
  { *position= pos; return RETURN_STATUS_OK; }
  const char *get_source_name() const { return my_filename(fd); }
private:
  File fd;
  my_off_t pos;
};


/**
  Appender object where the output is stored in a File.
*/
class File_appender : public Appender
{
public:
  /**
    Create a new File_reader

    @param _fd File descriptor. If this is not given, you have to call
    open() or set_file() before using this File_appender. You have to
    call set_file() with no arguments before destroying this object.
  */
  File_appender(File _fd= -1) : fd(_fd) {}
  /**
    Open the back-end file.
    @param filename Name of the file to open.
    @return RETURN_STATUS_OK or RETURN_STATUS_REPORTED_ERROR.
  */
  enum_return_status open(const char *filename)
  {
    DBUG_ENTER("File_appender::open");
    fd= my_open(filename, O_WRONLY, MYF(MY_WME));
    if (fd == -1)
      RETURN_REPORTED_ERROR;
    RETURN_OK;
  }
  /**
    Set the back-end file to an existing, open file.
    @param _fd File descriptor of the open file.
  */
  void set_file(File _fd) { fd= _fd; }
  /// Unsets the back-end file.
  void unset_file() { fd= -1; }
  /**
    Close the back-end file.
    @return RETURN_STATUS_OK or RETURN_STATUS_REPORTED_ERROR.
  */
  enum_return_status close()
  {
    DBUG_ENTER("File_appender::close");
    if (my_close(fd, MYF(MY_WME)) != 0)
      RETURN_REPORTED_ERROR;
    RETURN_OK;
  }
protected:
  const char *do_get_source_name() const { return my_filename(fd); }
  enum_return_status do_tell(my_off_t *position) const
  { return file_tell(fd, position); }
  enum_return_status do_append(const uchar *buf, size_t length)
  {
    DBUG_ENTER("File_appender::append");
    if (my_write(fd, buf, length, MYF(MY_WME | MY_WAIT_IF_FULL)) != length)
      RETURN_REPORTED_ERROR;
    RETURN_OK;
  }
  enum_return_status do_truncate(my_off_t new_position)
  { return file_truncate(fd, new_position); }
public:
  enum_return_status sync()
  {
    DBUG_ENTER("File_appender::sync");
    if (my_sync(fd, MYF(MY_WME)) != 0)
      RETURN_REPORTED_ERROR;
    RETURN_OK;
  }
  /**
    Return true if there is a back-end file associated with this
    File_appender.
  */
  bool is_open() const { return fd != -1; }
  //~File_appender() { DBUG_ASSERT(!is_open()); }
private:
  File fd;
};


#ifndef MYSQL_CLIENT
/**
  Appender class where the output is stored in a String object.
*/
class String_appender : public Appender
{
public:
  String_appender(String *_str) : str(_str) {}
protected:
  enum_return_status do_append(const uchar *buf, size_t length);
  enum_return_status do_truncate(my_off_t new_position);
  enum_return_status do_tell(my_off_t *position) const;
private:
  String *str;
};
#endif // ifndef MYSQL_CLIENT


/**
  Represents the owner of a group.

  This is a POD.  It has to be a POD because it is a member of
  Owned_groups::Node which is stored in a HASH.
*/
struct Rpl_owner_id
{
  uint32 owner_type;
  uint32 thread_id;
  void set_to_dead_client() { owner_type= thread_id= 0; }
  void set_to_none() { owner_type= NO_OWNER_TYPE; thread_id= NO_THREAD_ID; }
#ifndef MYSQL_CLIENT
  void copy_from(const THD *thd);
  bool equals(const THD *thd) const;
#endif // ifndef MYSQL_CLIENT
  bool is_sql_thread() const
  { return owner_type >= 1 && owner_type != NO_OWNER_TYPE; }
  bool is_none() const { return owner_type == NO_OWNER_TYPE; }
  bool is_client() const { return owner_type == 0; }
  bool is_very_old_client() const { return owner_type == 0 && thread_id == 0; }
  bool is_live_client() const;
  bool is_dead_client() const
  { return is_client() && !is_very_old_client() && !is_live_client(); }
  static const uint32 NO_OWNER_TYPE= ~0;
  static const uint32 NO_THREAD_ID= ~0;
};


/**
  Represents a UUID.

  This is a POD.  It has to be a POD because it is a member of
  Sid_map::Node which is stored in both HASH and DYNAMIC_ARRAY.
*/
struct Uuid
{
  /**
    Stores the UUID represented by a string on the form
    XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXXXXXX in this object.
    @return RETURN_STATUS_OK or RETURN_STATUS_UNREPORTED_ERROR.
  */
  enum_return_status parse(const char *string);
  /**
    Copies the given 16-byte data to this UUID.
  */
  void copy_from(const unsigned char *data)
  { memcpy(bytes, data, BYTE_LENGTH); }
  /**
    Copies the given UUID object to this UUID.
  */
  void copy_from(const Uuid *data) { copy_from(data->bytes); }
  /**
    Returns true if this UUID is equal the given UUID.
  */
  bool equals(const Uuid *other) const
  {
    return memcmp(bytes, other->bytes, BYTE_LENGTH) == 0;
  }
  /**
    Generates a 36+1 character long representation of this UUID object
    in the given string buffer.

    @retval 36 - the length of the resulting string.
  */
  size_t to_string(char *buf) const;
#ifndef DBUG_OFF
  void print() const
  {
    char buf[TEXT_LENGTH + 1];
    to_string(buf);
    printf("%s\n", buf);
  }
#endif
  /**
    Write this UUID to the given file.
    @param appender Appender object to write to.
    @param truncate_to_position @see Appender::append.
    @return APPEND_OK, APPEND_NONE, or APPEND_ERROR.
  */
  enum_append_status append(
    Appender *appender,
    my_off_t truncate_to_position= Appender::PREVIOUS_POSITION) const
  { return appender->append(bytes, Uuid::BYTE_LENGTH, truncate_to_position); }
  /**
    Read this UUID from the given file.
    @param Reader
    @return READ_OK, READ_ERROR, READ_EOF, or READ_TRUNCATED.
  */
  enum_read_status read(Reader *reader)
  { return reader->read(bytes, Uuid::BYTE_LENGTH); }
  /**
    Returns true if the given string contains a valid UUID, false otherwise.
  */
  static bool is_valid(const char *string);
  unsigned char bytes[16];
  static const size_t TEXT_LENGTH= 36;
  static const size_t BYTE_LENGTH= 16;
  static const size_t BIT_LENGTH= 128;
private:
  static const int NUMBER_OF_SECTIONS= 5;
  static const int bytes_per_section[NUMBER_OF_SECTIONS];
  static const int hex_to_byte[256];
};


typedef Uuid rpl_sid;


/**
  This has the functionality of mysql_rwlock_t, with two differences:
  1. It has additional operations to check if the read and/or write lock
     is held at the moment.
  2. It is wrapped in an object-oriented interface.

  Note that the assertions do not check whether *this* thread has
  taken the lock (that would be more complicated as it would require a
  dynamic data structure).  Luckily, it is still likely that the
  assertions find bugs where a thread forgot to take a lock, because
  most of the time most locks are only used by one thread at a time.

  The assertions are no-ops when DBUG is off.
*/
class Checkable_rwlock
{
public:
  /// Initialize this Checkable_rwlock.
  Checkable_rwlock()
  {
#ifndef DBUG_OFF
    my_atomic_rwlock_init(&atomic_lock);
    lock_state= 0;
#endif
    mysql_rwlock_init(0, &rwlock);
  }
  /// Destroy this Checkable_lock.
  ~Checkable_rwlock()
  {
#ifndef DBUG_OFF
    my_atomic_rwlock_destroy(&atomic_lock);
#endif
    mysql_rwlock_destroy(&rwlock);
  }

  /// Acquire the read lock.
  inline void rdlock()
  {
    mysql_rwlock_rdlock(&rwlock);
    assert_no_wrlock();
#ifndef DBUG_OFF
    my_atomic_rwlock_wrlock(&atomic_lock);
    my_atomic_add32(&lock_state, 1);
    my_atomic_rwlock_wrunlock(&atomic_lock);
#endif
  }
  /// Acquire the write lock.
  inline void wrlock()
  {
    mysql_rwlock_wrlock(&rwlock);
    assert_no_lock();
#ifndef DBUG_OFF
    my_atomic_rwlock_wrlock(&atomic_lock);
    my_atomic_store32(&lock_state, -1);
    my_atomic_rwlock_wrunlock(&atomic_lock);
#endif
  }
  /// Release the lock (whether it is a write or read lock).
  inline void unlock()
  {
    assert_some_lock();
#ifndef DBUG_OFF
    my_atomic_rwlock_wrlock(&atomic_lock);
    int val= my_atomic_load32(&lock_state);
    if (val > 0)
      my_atomic_add32(&lock_state, -1);
    else if (val == -1)
      my_atomic_store32(&lock_state, 0);
    else
      DBUG_ASSERT(0);
    my_atomic_rwlock_wrunlock(&atomic_lock);
#endif
    mysql_rwlock_unlock(&rwlock);
  }

  /// Assert that some thread holds either the read or the write lock.
  inline void assert_some_lock() const
  { DBUG_ASSERT(get_state() != 0); }
  /// Assert that some thread holds the read lock.
  inline void assert_some_rdlock() const
  { DBUG_ASSERT(get_state() > 0); }
  /// Assert that some thread holds the write lock.
  inline void assert_some_wrlock() const
  { DBUG_ASSERT(get_state() == -1); }
  /// Assert that no thread holds the write lock.
  inline void assert_no_wrlock() const
  { DBUG_ASSERT(get_state() >= 0); }
  /// Assert that no thread holds the read lock.
  inline void assert_no_rdlock() const
  { DBUG_ASSERT(get_state() <= 0); }
  /// Assert that no thread holds read or write lock.
  inline void assert_no_lock() const
  { DBUG_ASSERT(get_state() == 0); }

private:
#ifndef DBUG_OFF
  /**
    The state of the lock:
    0 - not locked
    -1 - write locked
    >0 - read locked by that many threads
  */
  volatile int32 lock_state;
  /// Lock to protect my_atomic_* operations on lock_state.
  mutable my_atomic_rwlock_t atomic_lock;
  /// Read lock_state atomically and return the value.
  inline int32 get_state() const
  {
    int32 ret;
    my_atomic_rwlock_rdlock(&atomic_lock);
    ret= my_atomic_load32(const_cast<volatile int32*>(&lock_state));
    my_atomic_rwlock_rdunlock(&atomic_lock);
    return ret;
  }
#endif
  /// The rwlock.
  mysql_rwlock_t rwlock;
};


/**
  Represents a bidirectional map between SID and SIDNO.

  SIDNOs are always numbers greater or equal to 1.

  This data structure has a read-write lock that protects the number
  of SIDNOs.  The lock is provided by the invoker of the constructor
  and it is generally the caller's responsibility to acquire the read
  lock.  Access methods assert that the caller already holds the read
  (or write) lock.  If a method of this class grows the number of
  SIDNOs, then the method temporarily upgrades this lock to a write
  lock and then degrades it to a read lock again; there will be a
  short period when the lock is not held at all.
*/
class Sid_map
{
public:
  /**
    Create this Sid_map.

    @param sid_lock Read-write lock that protects updates to the
    number of SIDNOs.
  */
  Sid_map(Checkable_rwlock *sid_lock);
  /// Destroy this Sid_map.
  ~Sid_map();
  /**
    Clears this Sid_map (for RESET MASTER)

    @return RETURN_STATUS_OK or RETURN_STAUTS_REPORTED_ERROR
  */
  enum_return_status clear();
  /**
    Open the disk file if it is not already open.
    @param base_filename The base of the filename, i.e., without "-sids".
    @return RETURN_STATUS_OK or RETURN_STATUS_REPORTED_ERROR.
  */
  enum_return_status open(const char *base_filename, bool writable= true);
  /**
    Close the disk file.
    @return RETURN_STATUS_OK or RETURN_STATUS_REPORTED_ERROR.
  */
  enum_return_status close();
  /**
    Sync changes on disk.
    @return RETURN_STATUS_OK or RETURN_STATUS_REPORTED_ERROR.
  */
  enum_return_status sync();
  /**
    Permanently add the given SID to this map if it does not already
    exist.

    The caller must hold the read lock on sid_lock before invoking
    this function.  If the SID does not exist in this map, it will
    release the read lock, take a write lock, update the map, release
    the write lock, and take the read lock again.

    @param sid The SID.
    @param _sync If true, the sid_map will be synced.
    @retval SIDNO The SIDNO for the SID (a new SIDNO if the SID did
    not exist, an existing if it did exist).
    @retval negative Error. This function calls my_error.

    @note The SID is stored on disk forever.  This is needed if the
    SID is written to the binary log.  If the SID will not be written
    to the binary log, it is a waste of disk space.  If this becomes a
    problem, we may add add_temporary(), which would only store the
    sid in memory, and return a negative sidno.
  */
  rpl_sidno add_permanent(const rpl_sid *sid, bool _sync= true);
  /**
    Get the SIDNO for a given SID

    The caller must hold the read lock on sid_lock before invoking
    this function.

    @param sid The SID.
    @retval SIDNO if the given SID exists in this map.
    @retval 0 if the given SID does not exist in this map.
  */
  rpl_sidno sid_to_sidno(const rpl_sid *sid) const
  {
    sid_lock->assert_some_lock();
    Node *node= (Node *)my_hash_search(&_sid_to_sidno, sid->bytes,
                                       rpl_sid::BYTE_LENGTH);
    if (node == NULL)
      return 0;
    return node->sidno;
  }
  /**
    Get the SID for a given SIDNO.

    An assertion is raised if the caller does not hold a lock on
    sid_lock, or if the SIDNO is not valid.

    @param sidno The SIDNO.
    @retval NULL The SIDNO does not exist in this map.
    @retval pointer Pointer to the SID.  The data is shared with this
    Sid_map, so should not be modified.  It is safe to read the data
    even after this Sid_map is modified, but not if this Sid_map is
    destroyed.
  */
  const rpl_sid *sidno_to_sid(rpl_sidno sidno) const
  {
    sid_lock->assert_some_lock();
    DBUG_ASSERT(sidno >= 1 && sidno <= get_max_sidno());
    return &(*dynamic_element(&_sidno_to_sid, sidno - 1, Node **))->sid;
  }
  /**
    Return the n'th smallest sidno, in the order of the SID's UUID.

    The caller must hold the read lock on sid_lock before invoking
    this function.

    @param n A number in the interval [0, get_max_sidno()-1], inclusively.
  */
  rpl_sidno get_sorted_sidno(rpl_sidno n) const
  {
    sid_lock->assert_some_lock();
    rpl_sidno ret= *dynamic_element(&_sorted, n, rpl_sidno *);
    return ret;
  }
  /**
    Return the biggest sidno in this Sid_map.

    The caller must hold the read or write lock on sid_lock before
    invoking this function.
  */
  rpl_sidno get_max_sidno() const
  {
    sid_lock->assert_some_lock();
    return _sidno_to_sid.elements;
  }
  /**
    Return true iff open() has been (successfully) called.
  */
  bool is_open() { return status == OPEN; }

private:
  /// Node pointed to by both the hash and the array.
  struct Node
  {
    rpl_sidno sidno;
    rpl_sid sid;
  };

  /**
    Create a Node from the given SIDNO and SID and add it to
    _sidno_to_sid, _sid_to_sidno, and _sorted.

    @param sidno The SIDNO to add.
    @param sid The SID to add.
    @return RETURN_STATUS_OK or RETURN_STATUS_REPORTED_ERROR.
  */
  enum_return_status add_node(rpl_sidno sidno, const rpl_sid *sid);
  /**
    Write changes to disk.

    @return RETURN_STATUS_OK or RETURN_STATUS_REPORTED_ERROR.
  */
  enum_return_status write_to_disk(rpl_sidno sidno, const rpl_sid *sid);

  /// Read-write lock that protects updates to the number of SIDNOs.
  mutable Checkable_rwlock *sid_lock;

  /**
    Array that maps SIDNO to SID; the element at index N points to a
    Node with SIDNO N-1.
  */
  DYNAMIC_ARRAY _sidno_to_sid;
  /**
    Hash that maps SID to SIDNO.  The keys in this array are of type
    rpl_sid.
  */
  HASH _sid_to_sidno;
  /**
    Array that maps numbers in the interval [0, get_max_sidno()-1] to
    SIDNOs, in order of increasing SID.

    @see Sid_map::get_sorted_sidno.
  */
  DYNAMIC_ARRAY _sorted;
  /// Filename.
  char filename[FN_REFLEN];
  /// File descriptor for the back-end file.
  File fd;
  /// Appender object to write to the file.
  File_appender appender;
  enum sid_map_status
  {
    CLOSED_OK, CLOSED_ERROR, OPEN
  };
  sid_map_status status;
};


/**
  Represents a growable array where each element contains a mutex and
  a condition variable.

  Each element can be locked, unlocked, broadcast, or waited for, and
  it is possible to call "THD::enter_cond" for the condition.

  This data structure has a read-write lock that protects the number
  of elements.  The lock is provided by the invoker of the constructor
  and it is generally the caller's responsibility to acquire the read
  lock.  Access methods assert that the caller already holds the read
  (or write) lock.  If a method of this class grows the number of
  elements, then the method temporarily upgrades this lock to a write
  lock and then degrades it to a read lock again; there will be a
  short period when the lock is not held at all.
*/
class Mutex_cond_array
{
public:
  /**
    Create a new Mutex_cond_array.

    @param global_lock Read-write lock that protects updates to the
    number of elements.
  */
  Mutex_cond_array(Checkable_rwlock *global_lock);
  /// Destroy this object.
  ~Mutex_cond_array();
  /// Lock the n'th mutex.
  inline void lock(int n) const
  {
    assert_not_owner(n);
    mysql_mutex_lock(&get_mutex_cond(n)->mutex);
  }
  /// Unlock the n'th mutex.
  inline void unlock(int n) const
  {
    assert_owner(n);
    mysql_mutex_unlock(&get_mutex_cond(n)->mutex);
  }
  /// Broadcast the n'th condition.
  inline void broadcast(int n) const
  {
    mysql_cond_broadcast(&get_mutex_cond(n)->cond);
  }
  /**
    Assert that this thread owns the n'th mutex.
    This is a no-op if DBUG_OFF is on.
  */
  inline void assert_owner(int n) const
  {
#ifndef DBUG_OFF
    mysql_mutex_assert_owner(&get_mutex_cond(n)->mutex);
#endif
  }
  /**
    Assert that this thread does not own the n'th mutex.
    This is a no-op if DBUG_OFF is on.
  */
  inline void assert_not_owner(int n) const
  {
#ifndef DBUG_OFF
    mysql_mutex_assert_not_owner(&get_mutex_cond(n)->mutex);
#endif
  }
  /// Wait for signal on the n'th condition variable.
  inline void wait(int n) const
  {
    DBUG_ENTER("Mutex_cond_array::wait");
    Mutex_cond *mutex_cond= get_mutex_cond(n);
    mysql_mutex_assert_owner(&mutex_cond->mutex);
    mysql_cond_wait(&mutex_cond->cond, &mutex_cond->mutex);
    DBUG_VOID_RETURN;
  }
#ifndef MYSQL_CLIENT
  /// Execute THD::enter_cond for the n'th condition variable.
  void enter_cond(THD *thd, int n, PSI_stage_info *stage,
                  PSI_stage_info *old_stage) const;
#endif // ifndef MYSQL_CLIENT
  /// Return the greatest addressable index in this Mutex_cond_array.
  inline int get_max_index() const
  {
    global_lock->assert_some_lock();
    return array.elements - 1;
  }
  /**
    Grows the array so that the given index fits.

    If the array is grown, the global_lock is temporarily upgraded to
    a write lock and then degraded again; there will be a
    short period when the lock is not held at all.

    @param n The index.
    @return RETURN_OK or RETURN_REPORTED_ERROR
  */
  enum_return_status ensure_index(int n);
private:
  /// A mutex/cond pair.
  struct Mutex_cond
  {
    mysql_mutex_t mutex;
    mysql_cond_t cond;
  };
  /// Return the Nth Mutex_cond object
  inline Mutex_cond *get_mutex_cond(int n) const
  {
    global_lock->assert_some_lock();
    DBUG_ASSERT(n <= get_max_index());
    Mutex_cond *ret= *dynamic_element(&array, n, Mutex_cond **);
    DBUG_ASSERT(ret);
    return ret;
  }
  /// Read-write lock that protects updates to the number of elements.
  mutable Checkable_rwlock *global_lock;
  DYNAMIC_ARRAY array;
};


/**
  Holds information about a group: the sidno and the gno.
*/
struct Group
{
  rpl_sidno sidno;
  rpl_gno gno;

  static const int MAX_TEXT_LENGTH= Uuid::TEXT_LENGTH + 1 + MAX_GNO_TEXT_LENGTH;
  static bool is_valid(const char *text);
  int to_string(const Sid_map *sid_map, char *buf) const;
  /**
    Parses the given string and stores in this Group.

    @param text The text to parse
    @return RETURN_STATUS_OK or RETURN_STATUS_REPORTED_ERROR.
  */
  enum_return_status parse(Sid_map *sid_map, const char *text);

#ifndef DBUG_OFF
  void print(const Sid_map *sid_map) const
  {
    char buf[MAX_TEXT_LENGTH + 1];
    to_string(sid_map, buf);
    printf("%s\n", buf);
  }
#endif
};


/**
  Represents a set of groups.

  This is structured as an array, indexed by SIDNO, where each element
  contains a linked list of intervals.

  This data structure OPTIONALLY has a read-write lock that protects
  the number of SIDNOs.  The lock is provided by the invoker of the
  constructor and it is generally the caller's responsibility to
  acquire the read lock.  If the lock is not NULL, access methods
  assert that the caller already holds the read (or write) lock.  If
  the lock is not NULL and a method of this class grows the number of
  SIDNOs, then the method temporarily upgrades this lock to a write
  lock and then degrades it to a read lock again; there will be a
  short period when the lock is not held at all.
*/
class Group_set
{
public:
  /**
    Constructs a new, empty Group_set.

    @param sid_map The Sid_map to use.
    @param sid_lock Read-write lock that protects updates to the
    number of SIDs. This may be NULL if such changes do not need to be
    protected.
  */
  Group_set(Sid_map *sid_map, Checkable_rwlock *sid_lock= NULL);
  /**
    Constructs a new Group_set that contains the groups in the given string, in the same format as add(char *).

    @param sid_map The Sid_map to use for SIDs.
    @param text The text to parse.
    @param status Will be set GS_SUCCESS or GS_ERROR_PARSE or
    GS_ERROR_OUT_OF_MEMORY.
    @param sid_lock Read/write lock to protect changes in the number
    of SIDs with. This may be NULL if such changes do not need to be
    protected.

    If sid_lock != NULL, then the read lock on sid_lock must be held
    before calling this function. If the array is grown, sid_lock is
    temporarily upgraded to a write lock and then degraded again;
    there will be a short period when the lock is not held at all.
  */
  Group_set(Sid_map *sid_map, const char *text, enum_return_status *status,
            Checkable_rwlock *sid_lock= NULL);
  /**
    Constructs a new Group_set that shares the same sid_map and
    sid_lock objects and contains a copy of all groups.

    @param other The Group_set to copy.
    @param status Will be set to GS_SUCCESS or GS_ERROR_OUT_OF_MEMORY.
  */
  Group_set(Group_set *other, enum_return_status *status);
  //Group_set(Sid_map *sid_map, Group_set *relative_to, Sid_map *sid_map_enc, const unsigned char *encoded, int length, enum_return_status *status);
  /// Destroy this Group_set.
  ~Group_set();
  //static int encode(Group_set *relative_to, Sid_map *sid_map_enc, unsigned char *buf);
  //static int encoded_length(Group_set *relative_to, Sid_map *sid_map_enc);
  /**
    Removes all groups from this Group_set.

    This does not deallocate anything: if groups are added later,
    existing allocated memory will be re-used.
  */
  void clear();
  /**
    Adds the given group to this Group_set.

    The SIDNO must exist in the Group_set before this function is called.

    @param sidno SIDNO of the group to add.
    @param gno GNO of the group to add.
    @return RETURN_STATUS_OK or RETURN_STATUS_REPORTED_ERROR.
  */
  enum_return_status _add(rpl_sidno sidno, rpl_gno gno)
  {
    Interval_iterator ivit(this, sidno);
    return add(&ivit, gno, gno + 1);
  }
  /**
    Adds all groups from the given Group_set to this Group_set.

    If sid_lock != NULL, then the read lock must be held before
    calling this function. If a new sidno is added so that the array
    of lists of intervals is grown, sid_lock is temporarily upgraded
    to a write lock and then degraded again; there will be a short
    period when the lock is not held at all.

    @param other The group set to add.
    @return RETURN_STATUS_OK or RETURN_STATUS_REPORTED_ERROR.
  */
  enum_return_status add(const Group_set *other);
  /**
    Removes all groups in the given Group_set from this Group_set.

    @param other The group set to remove.
    @return RETURN_STATUS_OK or RETURN_STATUS_REPORTED_ERROR.
  */
  enum_return_status remove(const Group_set *other);
  /**
    Adds the set of groups represented by the given string to this Group_set.

    The string must have the format of a comma-separated list of zero
    or more of the following:

       XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXXXXXX(:NUMBER+(-NUMBER)?)*
       | ANONYMOUS

       Each X is a hexadecimal digit (upper- or lowercase).
       NUMBER is a decimal, 0xhex, or 0oct number.

    If sid_lock != NULL, then the read lock on sid_lock must be held
    before calling this function. If a new sidno is added so that the
    array of lists of intervals is grown, sid_lock is temporarily
    upgraded to a write lock and then degraded again; there will be a
    short period when the lock is not held at all.

    @param text The string to parse.
    @param anonymous[in,out] If this is NULL, ANONYMOUS is not
    allowed.  If this is not NULL, it will be set to true if the
    anonymous group was found; false otherwise.
    @return RETURN_STATUS_OK or RETURN_STATUS_REPORTED_ERROR.
  */
  enum_return_status add(const char *text, bool *anonymous= NULL);
  /**
    Decodes a Group_set from the given string.

    @param string The string to parse.
    @param length The number of bytes.
    @return GS_SUCCESS or GS_ERROR_PARSE or GS_ERROR_OUT_OF_MEMORY
  */
  //int add(const unsigned char *encoded, int length);
  /// Return true iff the given group exists in this set.
  bool contains_group(rpl_sidno sidno, rpl_gno gno) const;
  /// Returns the maximal sidno that this Group_set currently has space for.
  rpl_sidno get_max_sidno() const
  {
    if (sid_lock)
      sid_lock->assert_some_lock();
    return intervals.elements;
  }
  /**
    Allocates space for all sidnos up to the given sidno in the array of intervals.
    The sidno must exist in the Sid_map associated with this Group_set.

    If sid_lock != NULL, then the read lock on sid_lock must be held
    before calling this function. If the array is grown, sid_lock is
    temporarily upgraded to a write lock and then degraded again;
    there will be a short period when the lock is not held at all.

    @param sidno The SIDNO.
    @return RETURN_STATUS_OK or RETURN_STATUS_REPORTED_ERROR.
  */
  enum_return_status ensure_sidno(rpl_sidno sidno);
  /// Returns true if this Group_set is equal to the other Group_set.
  bool equals(const Group_set *other) const;
  /// Returns true if this Group_set is a subset of the other Group_set.
  bool is_subset(const Group_set *super) const;
  /// Returns true if this Group_set is empty.
  bool is_empty() const
  {
    Group_iterator git(this);
    return git.get().sidno == 0;
  }
  /**
    Returns 0 if this Group_set is empty, 1 if it contains exactly one
    group, and 2 if it contains more than one group.

    This can be useful to check if the group is a singleton set or not.
  */
  int zero_one_or_many() const
  {
    Group_iterator git(this);
    if (git.get().sidno == 0)
      return 0;
    git.next();
    if (git.get().sidno == 0)
      return 1;
    return 2;
  }
  /**
    Returns true if this Group_set contains at least one group with
    the given SIDNO.

    @param sidno The SIDNO to test.
    @retval true The SIDNO is less than or equal to the max SIDNO, and
    there is at least one group with this SIDNO.
    @retval false The SIDNO is greater than the max SIDNO, or there is
    no group with this SIDNO.
  */
  bool contains_sidno(rpl_sidno sidno) const
  {
    DBUG_ASSERT(sidno >= 1);
    if (sidno > get_max_sidno())
      return false;
    Const_interval_iterator ivit(this, sidno);
    return ivit.get() != NULL;
  }
  /**
    Returns true if the given string is a valid specification of a Group_set, false otherwise.
  */
  static bool is_valid(const char *text);
#ifndef DBUG_OFF
  char *to_string() const
  {
    char *str= (char *)malloc(get_string_length() + 1);
    DBUG_ASSERT(str != NULL);
    to_string(str);
    return str;
  }
  /// Print this group set to stdout.
  void print() const
  {
    char *str= to_string();
    printf("%s\n", str);
    free(str);
  }
#endif
  //bool is_intersection_nonempty(Group_set *other);
  //Group_set in_place_intersection(Group_set other);
  //Group_set in_place_complement(Sid_map map);


  /**
    Class Group_set::String_format defines the separators used by
    Group_set::to_string.
  */
  struct String_format
  {
    const char *begin;
    const char *end;
    const char *sid_gno_separator;
    const char *gno_start_end_separator;
    const char *gno_gno_separator;
    const char *gno_sid_separator;
    const int begin_length;
    const int end_length;
    const int sid_gno_separator_length;
    const int gno_start_end_separator_length;
    const int gno_gno_separator_length;
    const int gno_sid_separator_length;
  };
  /**
    Returns the length of the output from to_string.

    @param string_format String_format object that specifies
    separators in the resulting text.
  */
  int get_string_length(const String_format *string_format= NULL) const;
  /**
    Encodes this Group_set as a string.

    @param buf[out] Pointer to the buffer where the string should be
    stored. This should have size at least get_string_length()+1.
    @param string_format String_format object that specifies
    separators in the resulting text.
    @return Length of the generated string.
  */
  int to_string(char *buf, const String_format *string_format= NULL) const;
  /// The default String_format: the format understood by add(const char *).
  static const String_format default_string_format;
  /**
    String_format useful to generate an SQL string: the string is
    wrapped in single quotes and there is a newline between SIDs.
  */
  static const String_format sql_string_format;

  /// Return the Sid_map associated with this Group_set.
  Sid_map *get_sid_map() const { return sid_map; }

  /**
    Represents one element in the linked list of intervals associated
    with a SIDNO.
  */
  struct Interval
  {
  public:
    /// The first GNO of this interval.
    rpl_gno start;
    /// The first GNO after this interval.
    rpl_gno end;
    /// Return true iff this interval is equal to the given interval.
    bool equals(const Interval *other) const
    {
      return start == other->start && end == other->end;
    }
    /// Pointer to next interval in list.
    Interval *next;
  };

  /**
    Provides an array of Intervals that this Group_set can use when
    groups are subsequently added.  This can be used as an
    optimization, to reduce allocation for sets that have a known
    number of intervals.

    @param n_intervals The number of intervals to add.
    @param intervals Array of n_intervals intervals.
  */
  void add_interval_memory(int n_intervals, Interval *intervals);

  /**
    Iterator over intervals for a given SIDNO.

    This is an abstract template class, used as a common base class
    for Const_interval_iterator and Interval_iterator.

    The iterator always points to an interval pointer.  The interval
    pointer is either the initial pointer into the list, or the next
    pointer of one of the intervals in the list.
  */
  template<typename Group_set_t, typename Interval_p> class Interval_iterator_base
  {
  public:
    /**
      Construct a new iterator over the GNO intervals for a given Group_set.

      @param group_set The Group_set.
      @param sidno The SIDNO.
    */
    Interval_iterator_base(Group_set_t *group_set, rpl_sidno sidno)
    {
      DBUG_ASSERT(sidno >= 1 && sidno <= group_set->get_max_sidno());
      init(group_set, sidno);
    }
    /// Construct a new iterator over the free intervals of a Group_set.
    Interval_iterator_base(Group_set_t *group_set)
    { p= &group_set->free_intervals; }
    /// Reset this iterator.
    inline void init(Group_set_t *group_set, rpl_sidno sidno)
    { p= dynamic_element(&group_set->intervals, sidno - 1, Interval_p *); }
    /// Advance current_elem one step.
    inline void next()
    {
      DBUG_ASSERT(*p != NULL);
      p= &(*p)->next;
    }
    /// Return current_elem.
    inline Interval_p get() const { return *p; }
  protected:
    /**
      Holds the address of the 'next' pointer of the previous element,
      or the address of the initial pointer into the list, if the
      current element is the first element.
    */
    Interval_p *p;
  };

  /**
    Iterator over intervals of a const Group_set.
  */
  class Const_interval_iterator
    : public Interval_iterator_base<const Group_set, Interval *const>
  {
  public:
    /// Create this Const_interval_iterator.
    Const_interval_iterator(const Group_set *group_set, rpl_sidno sidno)
      : Interval_iterator_base<const Group_set, Interval *const>(group_set, sidno) {}
    /// Destroy this Const_interval_iterator.
    Const_interval_iterator(const Group_set *group_set)
      : Interval_iterator_base<const Group_set, Interval *const>(group_set) {}
  };

  /**
    Iterator over intervals of a non-const Group_set, with additional
    methods to modify the Group_set.
  */
  class Interval_iterator
    : public Interval_iterator_base<Group_set, Interval *>
  {
  public:
    /// Create this Interval_iterator.
    Interval_iterator(Group_set *group_set, rpl_sidno sidno)
      : Interval_iterator_base<Group_set, Interval *>(group_set, sidno) {}
    /// Destroy this Interval_iterator.
    Interval_iterator(Group_set *group_set)
      : Interval_iterator_base<Group_set, Interval *>(group_set) {}
    /**
      Set current_elem to the given Interval but do not touch the
      next pointer of the given Interval.
    */
    inline void set(Interval *iv) { *p= iv; }
    /// Insert the given element before current_elem.
    inline void insert(Interval *iv) { iv->next= *p; set(iv); }
    /// Remove current_elem.
    inline void remove(Group_set *group_set)
    {
      DBUG_ASSERT(get() != NULL);
      Interval *next= (*p)->next;
      group_set->put_free_interval(*p);
      set(next);
    }
  };


  /**
    Iterator over all groups in a Group_set.  This is a const
    iterator; it does not allow modification of the Group_set.
  */
  class Group_iterator
  {
  public:
    Group_iterator(const Group_set *gs)
      : group_set(gs), sidno(0), ivit(gs) { next_sidno(); }
    /// Advance to next group.
    inline void next()
    {
      DBUG_ASSERT(gno > 0 && sidno > 0);
      // go to next group in current interval
      gno++;
      // end of interval? then go to next interval for this sidno
      if (gno == ivit.get()->end)
      {
        ivit.next();
        Interval *iv= ivit.get();
        // last interval for this sidno? then go to next sidno
        if (iv == NULL)
        {
          next_sidno();
          // last sidno? then don't try more
          if (sidno == 0)
            return;
          iv= ivit.get();
        }
        gno= iv->start;
      }
    }
    /// Return next group, or {0,0} if we reached the end.
    inline Group get() const
    {
      Group ret= { sidno, gno };
      return ret;
    }
  private:
    /// Find the next sidno that has one or more intervals.
    inline void next_sidno()
    {
      Interval *iv;
      do {
        sidno++;
        if (sidno > group_set->get_max_sidno())
        {
          sidno= 0;
          gno= 0;
          return;
        }
        ivit.init(group_set, sidno);
        iv= ivit.get();
      } while (iv == NULL);
      gno= iv->start;
    }
    /// The Group_set we iterate over.
    const Group_set *group_set;
    /**
      The SIDNO of the current element, or 0 if the iterator is past
      the last element.
    */
    rpl_sidno sidno;
    /**
      The GNO of the current element, or 0 if the iterator is past the
      last element.
    */
    rpl_gno gno;
    /// Iterator over the intervals for the current SIDNO.
    Const_interval_iterator ivit;
  };


private:
  /**
    Contains a list of intervals allocated by this Group_set.  When a
    method of this class needs a new interval and there are no more
    free intervals, a new Interval_chunk is allocated and the
    intervals of it are added to the list of free intervals.
  */
  struct Interval_chunk
  {
    Interval_chunk *next;
    Interval intervals[1];
  };
  /// The default number of intervals in an Interval_chunk.
  static const int CHUNK_GROW_SIZE= 8;

  /**
    Adds a list of intervals to the given SIDNO.

    The SIDNO must exist in the Group_set before this function is called.

    @param sidno The SIDNO to which intervals will be added.
    @param ivit Iterator over the intervals to add. This is typically
    an iterator over some other Group_set.
    @return RETURN_STATUS_OK or RETURN_STATUS_REPORTED_ERROR.
  */
  enum_return_status add(rpl_sidno sidno, Const_interval_iterator ivit);
  /**
    Removes a list of intervals to the given SIDNO.

    It is not required that the intervals exist in this Group_set.

    @param sidno The SIDNO from which intervals will be removed.
    @param ivit Iterator over the intervals to remove. This is typically
    an iterator over some other Group_set.
    @return RETURN_STATUS_OK or RETURN_STATUS_REPORTED_ERROR.
  */
  enum_return_status remove(rpl_sidno sidno, Const_interval_iterator ivit);
  /**
    Adds the interval (start, end) to the given Interval_iterator.

    This is the lowest-level function that adds groups; this is where
    Interval objects are added, grown, or merged.

    @param ivitp Pointer to iterator.  After this function returns,
    the current_element of the iterator will be the interval that
    contains start and end.
    @param start The first GNO in the interval.
    @param end The first GNO after the interval.
    @return RETURN_STATUS_OK or RETURN_STATUS_REPORTED_ERROR.
  */
  enum_return_status add(Interval_iterator *ivitp, rpl_gno start, rpl_gno end);
  /**
    Removes the interval (start, end) from the given
    Interval_iterator. This is the lowest-level function that removes
    groups; this is where Interval objects are removed, truncated, or
    split.

    It is not required that the groups in the interval exist in this
    Group_set.

    @param ivitp Pointer to iterator.  After this function returns,
    the current_element of the iterator will be the next interval
    after end.
    @param start The first GNO in the interval.
    @param end The first GNO after the interval.
    @return RETURN_STATUS_OK or RETURN_STATUS_REPORTED_ERROR.
  */
  enum_return_status remove(Interval_iterator *ivitp,
                            rpl_gno start, rpl_gno end);
  /**
    Allocates a new chunk of Intervals and adds them to the list of
    unused intervals.

    @param size The number of intervals in this chunk
    @return RETURN_STATUS_OK or RETURN_STATUS_REPORTED_ERROR.
  */
  enum_return_status create_new_chunk(int size);
  /**
    Returns a fresh new Interval object.

    This usually does not require any real allocation, it only pops
    the first interval from the list of free intervals.  If there are
    no free intervals, it calls create_new_chunk.

    @param out The resulting Interval* will be stored here.
    @return RETURN_STATUS_OK or RETURN_STATUS_REPORTED_ERROR.
  */
  enum_return_status get_free_interval(Interval **out);
  /**
    Puts the given interval in the list of free intervals.  Does not
    unlink it from its place in any other list.
  */
  void put_free_interval(Interval *iv);
  /// Worker for the constructor.
  void init(Sid_map *_sid_map, Checkable_rwlock *_sid_lock);

  /// Read-write lock that protects updates to the number of SIDs.
  mutable Checkable_rwlock *sid_lock;
  /// Sid_map associated with this Group_set.
  Sid_map *sid_map;
  /**
    Array where the N'th element contains the head pointer to the
    intervals of SIDNO N+1.
  */
  DYNAMIC_ARRAY intervals;
  /// Linked list of free intervals.
  Interval *free_intervals;
  /// Linked list of chunks.
  Interval_chunk *chunks;
  /// The string length.
  mutable int cached_string_length;
  /// The String_format that was used when cached_string_length was computed.
  mutable const String_format *cached_string_format;
#ifndef DBUG_OFF
  /**
    The number of chunks.  Used only to check some invariants when
    DBUG is on.
  */
  int n_chunks;
#endif

  /// Used by unit tests that need to access private members.
#ifdef FRIEND_OF_GROUP_SET
  friend FRIEND_OF_GROUP_SET;
#endif
};


/**
  Holds information about a group set.  Can also be NULL.

  This is used as backend storage for @@session.ugid_next_list.  The
  idea is that we allow the user to set this to NULL, but we keep the
  Group_set object so that we can re-use the allocated memory and
  avoid costly allocations later.

  This is stored in struct system_variables (defined in sql_class.h),
  which is cleared using memset(0); hence the negated form of
  is_non_null.

  The convention is: if is_non_null is false, then the value of the
  session variable is NULL, and the field group_set may be NULL or
  non-NULL.  If is_non_null is true, then the value of the session
  variable is not NULL, and the field group_set has to be non-NULL.
*/
struct Group_set_or_null
{
  /// Pointer to the Group_set.
  Group_set *group_set;
  /// True if this Group_set is NULL.
  bool is_non_null;
  /// Return NULL if this is NULL, otherwise return the Group_set.
  inline Group_set *get_group_set() const
  {
    DBUG_ASSERT(!(is_non_null && group_set == NULL));
    return is_non_null ? group_set : NULL;
  }
  /**
    Do nothing if this object is non-null; set to empty set otherwise.

    @return NULL if out of memory; Group_set otherwise.
  */
  Group_set *set_non_null(Sid_map *sm)
  {
    if (!is_non_null)
    {
      if (group_set == NULL)
        group_set= new Group_set(sm);
      else
        group_set->clear();
    }
    is_non_null= (group_set != NULL);
    return group_set;
  }
  /// Set this Group_set to NULL.
  inline void set_null() { is_non_null= false; }
};


/**
  Represents the set of groups that are owned by some thread.

  This consists of all partial groups and a subset of the unlogged
  groups.  Each group has a flag that indicates whether it is partial
  or not.

  This data structure has a read-write lock that protects the number
  of SIDNOs.  The lock is provided by the invoker of the constructor
  and it is generally the caller's responsibility to acquire the read
  lock.  Access methods assert that the caller already holds the read
  (or write) lock.  If a method of this class grows the number of
  SIDNOs, then the method temporarily upgrades this lock to a write
  lock and then degrades it to a read lock again; there will be a
  short period when the lock is not held at all.

  The internal representation is a DYNAMIC_ARRAY that maps SIDNO to
  HASH, where each HASH maps GNO to (Rpl_owner_id, bool).
*/
class Owned_groups
{
public:
  /**
    Constructs a new, empty Owned_groups object.

    @param sid_lock Read-write lock that protects updates to the
    number of SIDs.
  */
  Owned_groups(Checkable_rwlock *sid_lock);
  /// Destroys this Owned_groups.
  ~Owned_groups();
  /// Mark all owned groups for all SIDs as non-partial.
  void clear();
  /**
    Add a group to this Owned_groups.

    The group will be marked as non-partial, i.e., it has not yet been
    written to the binary log.

    @param sidno The SIDNO of the group to add.
    @param gno The GNO of the group to add.
    @param owner_id The Owner_id of the group to add.
    @return RETURN_STATUS_OK or RETURN_STATUS_REPORTED_ERROR.
  */
  enum_return_status add(rpl_sidno sidno, rpl_gno gno, Rpl_owner_id owner_id);
  /**
    Returns the owner of the given group.

    If the group does not exist in this Owned_groups object, returns
    an Rpl_owner_id object that contains 'no owner'.

    @param sidno The group's SIDNO
    @param gno The group's GNO
    @return Owner of the group.
  */
  Rpl_owner_id get_owner(rpl_sidno sidno, rpl_gno gno) const;
  /**
    Changes owner of the given group.

    Throws an assertion if the group does not exist in this
    Owned_groups object.

    @param sidno The group's SIDNO.
    @param gno The group's GNO.
    @param owner_id The group's new owner_id.
  */
  void change_owner(rpl_sidno sidno, rpl_gno gno,
                    Rpl_owner_id owner_id) const;
  /**
    Removes the given group.

    If the group does not exist in this Owned_groups object, does
    nothing.

    @param sidno The group's SIDNO.
    @param gno The group's GNO.
  */
  void remove(rpl_sidno sidno, rpl_gno gno);
  /**
    Marks the given group as partial.

    Throws an assertion if the group does not exist in this
    Owned_groups object.

    @param sidno The group's SIDNO.
    @param gno The group's GNO.
  */
  bool mark_partial(rpl_sidno sidno, rpl_gno gno);
  /**
    Returns true iff the given group is partial.

    Throws an assertion if the group does not exist in this
    Owned_groups object.

    @param sidno The group's SIDNO.
    @param gno The group's GNO.
  */
  bool is_partial(rpl_sidno sidno, rpl_gno gno) const;
  /**
    Ensures that this Owned_groups object can accomodate SIDNOs up to
    the given SIDNO.

    If this Owned_groups object needs to be resized, then the lock
    will be temporarily upgraded to a write lock and then degraded to
    a read lock again; there will be a short period when the lock is
    not held at all.

    @param sidno The SIDNO.
    @return RETURN_STATUS_OK or RETURN_STATUS_REPORTED_ERROR.
  */
  enum_return_status ensure_sidno(rpl_sidno sidno);
  /// Returns the maximal sidno that this Owned_groups currently has space for.
  rpl_sidno get_max_sidno() const
  {
    sid_lock->assert_some_lock();
    return sidno_to_hash.elements;
  }
  /**
    Adds all partial groups in this Owned_groups object to the given Group_set.

    @param gs Group_set that will be updated.
    @return RETURN_STATUS_OK or RETURN_STATUS_REPORTED_ERROR.
  */
  enum_return_status get_partial_groups(Group_set *gs) const;

#ifndef DBUG_OFF
  int to_string(const Sid_map *sm, char *out) const
  {
    char *p= out;
    rpl_sidno max_sidno= get_max_sidno();
    for (rpl_sidno sidno= 1; sidno <= max_sidno; sidno++)
    {
      HASH *hash= get_hash(sidno);
      for (uint i= 0; i < hash->records; i++)
      {
        Node *node= (Node *)my_hash_element(hash, i);
        DBUG_ASSERT(node != NULL);
        p+= sm->sidno_to_sid(sidno)->to_string(p);
        p+= sprintf(p, "/%d:%lld owned by %d-%d is %spartial\n",
                    sidno, node->gno,
                    node->owner.owner_type, node->owner.thread_id,
                    node->is_partial ? "" : "not ");
      }
    }
    return p - out;
  }
  size_t get_string_length() const
  {
    rpl_sidno max_sidno= get_max_sidno();
    size_t ret= 0;
    for (rpl_sidno sidno= 1; sidno <= max_sidno; sidno++)
    {
      HASH *hash= get_hash(sidno);
      ret+= hash->records * 256; // should be enough
    }
    return ret;
  }
  char *to_string(const Sid_map *sm) const
  {
    char *str= (char *)malloc(get_string_length());
    DBUG_ASSERT(str != NULL);
    to_string(sm, str);
    return str;
  }
  void print(const Sid_map *sm) const
  {
    char *str= to_string(sm);
    printf("%s\n", str);
    free(str);
  }
#endif
private:
  /// Represents one owned group.
  struct Node
  {
    /// GNO of the group.
    rpl_gno gno;
    /// Owner of the group.
    Rpl_owner_id owner;
    /// If true, this group is partial; i.e., written to the binary log.
    bool is_partial;
  };
  /// Read-write lock that protects updates to the number of SIDs.
  mutable Checkable_rwlock *sid_lock;
  /// Returns the HASH for the given SIDNO.
  HASH *get_hash(rpl_sidno sidno) const
  {
    DBUG_ASSERT(sidno >= 1 && sidno <= get_max_sidno());
    sid_lock->assert_some_lock();
    return *dynamic_element(&sidno_to_hash, sidno - 1, HASH **);
  }
  /**
    Returns the Node for the given HASH and GNO, or NULL if the GNO
    does not exist in the HASH.
  */
  Node *get_node(const HASH *hash, rpl_gno gno) const
  {
    sid_lock->assert_some_lock();
    return (Node *)my_hash_search(hash, (const uchar *)&gno, sizeof(rpl_gno));
  }
  /**
    Returns the Node for the given group, or NULL if the group does
    not exist in this Owned_groups object.
  */
  Node *get_node(rpl_sidno sidno, rpl_gno gno) const
  {
    return get_node(get_hash(sidno), gno);
  };
  /// Return true iff this Owned_groups object contains the given group.
  bool contains_group(rpl_sidno sidno, rpl_gno gno) const
  {
    return get_node(sidno, gno) != NULL;
  }
  /// Growable array of hashes.
  DYNAMIC_ARRAY sidno_to_hash;
};


/**
  Represents the state of the group log: the set of ended groups and
  the set of owned groups, the owner of each owned group, and a
  Mutex_cond_array that protects updates to groups of each SIDNO.

  This data structure has a read-write lock that protects the number
  of SIDNOs.  The lock is provided by the invoker of the constructor
  and it is generally the caller's responsibility to acquire the read
  lock.  Access methods assert that the caller already holds the read
  (or write) lock.  If a method of this class grows the number of
  SIDNOs, then the method temporarily upgrades this lock to a write
  lock and then degrades it to a read lock again; there will be a
  short period when the lock is not held at all.
*/
class Group_log_state
{
public:
  /**
    Constructs a new Group_log_state object.

    @param _sid_lock Read-write lock that protects updates to the
    number of SIDs.
    @param _sid_map Sid_map used by this group log.
  */
  Group_log_state(Checkable_rwlock *_sid_lock, Sid_map *_sid_map)
    : sid_lock(_sid_lock), sid_locks(_sid_lock),
    sid_map(_sid_map),
    ended_groups(_sid_map), owned_groups(_sid_lock) {}
  /**
    Reset the state after RESET MASTER: remove all ended groups and
    mark all owned groups as non-partial.
  */
  void clear();
  /**
    Returns true if the given group is ended.

    @param sidno The SIDNO to check.
    @param gno The GNO to check.
    @retval true The group is ended in the group log.

    @retval false The group is partial or unlogged in the group log.
  */
  bool is_ended(rpl_sidno sidno, rpl_gno gno) const
  { return ended_groups.contains_group(sidno, gno); }
  /**
    Returns true if the given group is partial.

    @param sidno The SIDNO to check.
    @param gno The GNO to check.
    @retval true The group is partial in the group log.
    @retval false The group is ended or unlogged in the group log.
  */
  bool is_partial(rpl_sidno sidno, rpl_gno gno) const
  { return owned_groups.is_partial(sidno, gno); }
  /**
    Returns the owner of the given group.

    @param sidno The SIDNO to check.
    @param gno The GNO to check.
    @return Rpl_owner_id of the thread that owns the group (possibly
    none, if the group is not owned).
  */
  Rpl_owner_id get_owner(rpl_sidno sidno, rpl_gno gno) const
  { return owned_groups.get_owner(sidno, gno); }
  /**
    Marks the given group as partial.
    
    Raises an assertion if the group is not owned.

    @param sidno The SIDNO of the group.
    @param gno The GNO of the group.
    @return The value of is_partial() before this call.
  */
  bool mark_partial(rpl_sidno sidno, rpl_gno gno)
  { return owned_groups.mark_partial(sidno, gno); }
  /**
    Marks the group as not owned any more.

    If the group is not owned, does nothing.

    @param sidno The SIDNO of the group
    @param gno The GNO of the group.
  */
  /*UNUSED
  void mark_not_owned(rpl_sidno sidno, rpl_gno gno)
  { owned_groups.remove(sidno, gno); }
  */
  /**
    Acquires ownership of the given group, on behalf of the given thread.

    @param sidno The group's SIDNO.
    @param gno The group's GNO.
    @param owner The thread that will own the group.
    @return RETURN_STATUS_OK or RETURN_STATUS_REPORTED_ERROR.
  */
#ifndef MYSQL_CLIENT
  enum_return_status acquire_ownership(rpl_sidno sidno, rpl_gno gno,
                                       const THD *thd);
#endif // ifndef MYSQL_CLIENT
  /**
    Ends the given group, i.e., moves it from the set of 'owned
    groups' to the set of 'ended groups'.

    @param sidno The group's SIDNO.
    @param gno The group's GNO.
    @return RETURN_STATUS_OK or RETURN_STATUS_REPORTED_ERROR.
  */
  enum_return_status end_group(rpl_sidno sidno, rpl_gno gno);
  /**
    Allocates a GNO for an automatically numbered group.

    @param sidno The group's SIDNO.

    @retval negative the numeric value of GS_ERROR_OUT_OF_MEMORY
    @retval other The GNO for the group.
  */
  rpl_gno get_automatic_gno(rpl_sidno sidno) const;
  /// Locks a mutex for the given SIDNO.
  void lock_sidno(rpl_sidno sidno) { sid_locks.lock(sidno); }
  /// Unlocks a mutex for the given SIDNO.
  void unlock_sidno(rpl_sidno sidno) { sid_locks.unlock(sidno); }
  /// Broadcasts updates for the given SIDNO.
  void broadcast_sidno(rpl_sidno sidno) { sid_locks.broadcast(sidno); }
  /// Waits for updates on the given SIDNO.
#ifndef MYSQL_CLIENT
  void wait_for_sidno(THD *thd, const Sid_map *sm, Group g, Rpl_owner_id owner);
#endif // ifndef MYSQL_CLIENT
  /**
    Locks one mutex for each SIDNO where the given Group_set has at
    least one group. If the Group_set is not given, locks all
    mutexes.  Locks are acquired in order of increasing SIDNO.
  */
  void lock_sidnos(const Group_set *set= NULL);
  /**
    Unlocks the mutex for each SIDNO where the given Group_set has at
    least one group.  If the Group_set is not given, unlocks all mutexes.
  */
  void unlock_sidnos(const Group_set *set= NULL);
  /**
    Waits for the condition variable for each SIDNO where the given
    Group_set has at least one group.
  */
  void broadcast_sidnos(const Group_set *set);
  /**
    Ensure that owned_groups, ended_groups, and sid_locks have room
    for at least as many SIDNOs as sid_map.

    Requires that the read lock on sid_locks is held.  If any object
    needs to be resized, then the lock will be temporarily upgraded to
    a write lock and then degraded to a read lock again; there will be
    a short period when the lock is not held at all.

    @return RETURN_STATUS_OK or RETURN_STATUS_REPORTED_ERROR.
  */
  enum_return_status ensure_sidno();
  /// Return a pointer to the Group_set that contains the ended groups.
  const Group_set *get_ended_groups() { return &ended_groups; }
  /// Return a pointer to the Owned_groups that contains the owned groups.
  const Owned_groups *get_owned_groups() { return &owned_groups; }
  /// Return Sid_map used by this Group_log_state.
  const Sid_map *get_sid_map() { return sid_map; }
#ifndef DBUG_OFF
  size_t get_string_length() const
  {
    return owned_groups.get_string_length() +
      ended_groups.get_string_length() + 100;
  }
  int to_string(char *buf) const
  {
    char *p= buf;
    p+= sprintf(p, "Ended groups:\n");
    p+= ended_groups.to_string(p);
    p+= sprintf(p, "\nOwned groups:\n");
    p+= owned_groups.to_string(sid_map, p);
    return p - buf;
  }
  char *to_string() const
  {
    char *str= (char *)malloc(get_string_length());
    DBUG_ASSERT(str != NULL);
    to_string(str);
    return str;
  }
  void print() const
  {
    char *str= to_string();
    printf("%s", str);
    free(str);
  }
#endif
private:
  /// Read-write lock that protects updates to the number of SIDs.
  mutable Checkable_rwlock *sid_lock;
  /// Contains one mutex/cond pair for every SIDNO.
  Mutex_cond_array sid_locks;
  /// The Sid_map used by this Group_log_state.
  Sid_map *sid_map;
  /// The set of groups that are ended in the group log.
  Group_set ended_groups;
  /// The set of groups that are owned by some thread.
  Owned_groups owned_groups;

  /// Used by unit tests that need to access private members.
#ifdef FRIEND_OF_GROUP_LOG_STATE
  friend FRIEND_OF_GROUP_LOG_STATE;
#endif
};


/**
  Enumeration of subgroup types.
*/
enum enum_subgroup_type
{
  NORMAL_SUBGROUP= 0, ANONYMOUS_SUBGROUP= 1, DUMMY_SUBGROUP= 2
};


/**
  This struct represents a specification of a UGID for a statement to
  be executed: either "AUTOMATIC", "ANONYMOUS", or "SID:GNO".
*/
struct Ugid_specification
{
  /// The type of group.
  enum enum_type
  {
    AUTOMATIC, ANONYMOUS, UGID, INVALID
  };
  enum_type type;
  /**
    The UGID:
    { SIDNO, GNO } if type == UGID;
    { 0, 0 } if type == AUTOMATIC or ANONYMOUS.
  */
  Group group;
  /**
    Parses the given string and stores in this Ugid_specification.

    @param text The text to parse
    @return RETURN_STATUS_OK or RETURN_STATUS_REPORTED_ERROR.
  */
  enum_return_status parse(const char *text);
  static const int MAX_TEXT_LENGTH= Uuid::TEXT_LENGTH + 1 + MAX_GNO_TEXT_LENGTH;
  /**
    Writes this Ugid_specification to the given string buffer.

    @param buf[out] The buffer
    @retval The number of characters written.
  */
  int to_string(char *buf) const;
  /**
    Returns the type of the group, if the given string is a valid Ugid_specification; INVALID otherwise.
  */
  static enum_type get_type(const char *text);
  /// Returns true if the given string is a valid Ugid_specification.
  static bool is_valid(const char *text) { return get_type(text) != INVALID; }
#ifndef DBUG_OFF
  void print() const
  {
    char buf[MAX_TEXT_LENGTH + 1];
    to_string(buf);
    printf("%s\n", buf);
  }
#endif
};


/**
   Holds information about a sub-group.

   This can be a normal sub-group, an anonymous sub-group, or a dummy
   sub-group.
*/
struct Subgroup
{
  enum_subgroup_type type;
  rpl_sidno sidno;
  rpl_gno gno;
  rpl_binlog_no binlog_no;
  rpl_binlog_pos binlog_pos;
  rpl_binlog_pos binlog_length;
  rpl_binlog_pos binlog_offset_after_last_statement;
  rpl_lgid lgid;
  bool group_commit;
  bool group_end;
  size_t to_string(char *buf, Sid_map *sm= NULL) const
  {
    char *s= buf;
    s+= sprintf(s, "Subgroup(#%lld, ", lgid);
    if (type == ANONYMOUS_SUBGROUP)
      s+= sprintf(s, "ANONYMOUS");
    else
    {
      char sid_buf[Uuid::TEXT_LENGTH + 1];
      if (sm == NULL)
        sprintf(sid_buf, "%d", sidno);
      else
        sm->sidno_to_sid(sidno)->to_string(sid_buf);
      s+= sprintf(s, "%s:%lld%s", sid_buf, gno, group_end ? ", END" : "");
    }
    if (group_commit)
      s+= sprintf(s, ", COMMIT");
    if (type == DUMMY_SUBGROUP)
      s+= sprintf(s, " DUMMY");
    else
      s+= sprintf(s, ", binlog(no=%lld, pos=%lld, len=%lld, oals=%lld))",
                  binlog_no, binlog_pos, binlog_length,
                  binlog_offset_after_last_statement);
    return s - buf;
  }
  static const size_t MAX_TEXT_LENGTH= 1024;
#ifndef DBUG_OFF
  void print(Sid_map *sm= NULL) const
  {
    char buf[MAX_TEXT_LENGTH];
    to_string(buf, sm);
    printf("%s\n", buf);
  }
  char *to_string(Sid_map *sm= NULL) const
  {
    char *ret= (char *)malloc(MAX_TEXT_LENGTH);
    DBUG_ASSERT(ret != NULL);
    to_string(ret, sm);
    return ret;
  }
#endif
};


/**
  Represents a sub-group in the group cache.

  Groups in the group cache are slightly different from other
  sub-groups, because not all information about them is known.

  Automatic sub-groups are marked as such by setting gno<=0.
*/
struct Cached_subgroup
{
  enum_subgroup_type type;
  rpl_sidno sidno;
  rpl_gno gno;
  rpl_binlog_pos binlog_length;
  bool group_end;
};


/**
  Represents a group cache: either the statement group cache or the
  transaction group cache.
*/
class Group_cache
{
public:
  /// Constructs a new Group_cache.
  Group_cache();
  /// Deletes a Group_cache.
  ~Group_cache();
  /// Removes all sub-groups from this cache.
  void clear();
  /// Return the number of sub-groups in this group cache.
  inline int get_n_subgroups() const { return subgroups.elements; }
  /// Return true iff the group cache contains zero sub-groups.
  inline bool is_empty() const { return get_n_subgroups() == 0; }
  /**
    Adds a sub-group to this Group_cache.  The sub-group should
    already have been written to the stmt or trx cache.  The SIDNO and
    GNO fields are taken from @@SESSION.UDIG_NEXT.  The GROUP_END
    field is taken from @@SESSION.UGID_END.

    @param thd The THD object from which we read session variables.
    @param binlog_length Length of group in binary log.
    @return RETURN_STATUS_OK or RETURN_STATUS_REPORTED_ERROR.
  */
#ifndef MYSQL_CLIENT
  enum_return_status add_logged_subgroup(const THD *thd,
                                         my_off_t binlog_length);
#endif // ifndef MYSQL_CLIENT
  /**
    Adds a dummy group with the given SIDNO, GNO, and GROUP_END to this cache.

    @param sidno The SIDNO of the group.
    @param gno The GNO of the group.
    @param group_end The GROUP_END of the group.
    @return RETURN_STATUS_OK or RETURN_STATUS_REPORTED_ERROR.
  */
  enum_return_status add_dummy_subgroup(rpl_sidno sidno, rpl_gno gno,
                                        bool group_end);
  /**
    Add the given group to this cache, unended, unless the cache or
    the Group_log_state already contains it.

    @param gls Group_log_state, used to determine if the group is
    unlogged or not.
    @param sidno The SIDNO of the group to add.
    @param gno The GNO of the group to add.
    @return RETURN_STATUS_OK or RETURN_STATUS_REPORTED_ERROR.
  */
  enum_return_status
    add_dummy_subgroup_if_missing(const Group_log_state *gls,
                                  rpl_sidno sidno, rpl_gno gno);
  /**
    Add all groups in the given Group_set to this cache, unended,
    except groups that exist in this cache or in the Group_log_state.

    @param gls Group_log_state, used to determine if the group is
    unlogged or not.
    @param group_set The set of groups to possibly add.
    @return RETURN_STATUS_OK or RETURN_STATUS_REPORTED_ERROR.
  */
  enum_return_status
    add_dummy_subgroups_if_missing(const Group_log_state *gls,
                                   const Group_set *group_set);
#ifndef MYSQL_CLIENT
  /**
    Update the binary log's Group_log_state to the state after this
    cache has been flushed.

    @param thd The THD that this Group_log_state belongs to.
    @param gls The binary log's Group_log_state
    @return RETURN_STATUS_OK or RETURN_STATUS_REPORTED_ERROR.
  */
  enum_return_status
    update_group_log_state(const THD *thd, Group_log_state *gls) const;
  /**
    Writes all sub-groups in the cache to the group log.

    @todo The group log is not yet implemented. /Sven

    @param trx_group_cache Should be set to the transaction group
    cache. If trx_group_cache is different from 'this', then it is
    assumed that 'this' is the statement group cache.  In that
    case, if a group is ended in 'this' and the group exists in
    trx_group_cache, then the end flag is removed from 'this', and if
    the group is not ended in trx_group_cache, an ending dummy group
    is appended to trx_group_cache.  This operation is necessary to
    prevent sub-groups of the group from being logged after ended
    sub-groups of the group.
    @return RETURN_STATUS_OK or RETURN_STATUS_REPORTED_ERROR.
  */
  enum_return_status
    write_to_log(const THD *thd, Group_log *group_log,
                 Group_cache *trx_group_cache,
                 rpl_binlog_no binlog_no, rpl_binlog_pos binlog_pos,
                 rpl_binlog_pos offset_after_last_statement, bool group_commit);
  /**
    Generates GNO for all groups that are committed for the first time
    in this Group_cache.

    This acquires ownership of all groups.  After this call, this
    Group_cache does not contain any Cached_subgroups that have
    type==NORMAL_SUBGROUP and gno<=0.

    @param thd The THD that this Group_log_state belongs to.
    @param gls The Group_log_state where group ownership is acquired.
    @return RETURN_STATUS_OK or RETURN_STATUS_REPORTED_ERROR
  */
  enum_return_status generate_automatic_gno(const THD *thd,
                                            Group_log_state *gls);
#endif // ifndef MYSQL_CLIENT
  /**
    Return true if this Group_cache contains the given group.

    @param sidno The SIDNO of the group to check.
    @param gno The GNO of the group to check.
    @retval true The group exists in this cache.
    @retval false The group does not exist in this cache.
  */
  bool contains_group(rpl_sidno sidno, rpl_gno gno) const;
  /**
    Return true if the given group is ended in this Group_cache.

    @param sidno SIDNO of the group to check.
    @param gno GNO of the group to check.
    @retval true The group is ended in this Group_cache.
    @retval false The group is not ended in this Group_cache.
  */
  bool group_is_ended(rpl_sidno sidno, rpl_gno gno) const;
  /**
    Add all groups that exist but are unended in this Group_cache to the given Group_set.

    If this Owned_groups contains SIDNOs that do not exist in the
    Group_set, then the Group_set's array of lists of intervals will
    be grown.  If the Group_set has a sid_lock, then the method
    temporarily upgrades the lock to a write lock and then degrades it
    to a read lock again; there will be a short period when the lock
    is not held at all.

    @param gs The Group_set to which groups are added.
    @return RETURN_STATUS_OK or RETURN_STATUS_REPORTED_ERROR.
  */
  enum_return_status get_partial_groups(Group_set *gs) const;
  /**
    Add all groups that exist and are ended in this Group_cache to the given Group_set.

    @param gs The Group_set to which groups are added.
    @return RETURN_STATUS_OK or RETURN_STATUS_REPORTED_ERROR.
  */
  enum_return_status get_ended_groups(Group_set *gs) const;

#ifndef DBUG_OFF
  size_t to_string(const Sid_map *sm, char *buf) const
  {
    int n_subgroups= get_n_subgroups();
    char *s= buf;

    s += sprintf(s, "%d sub-groups = {\n", n_subgroups);
    for (int i= 0; i < n_subgroups; i++)
    {
      Cached_subgroup *cs= get_unsafe_pointer(i);
      char uuid[Uuid::TEXT_LENGTH + 1]= "[]";
      if (cs->sidno)
        sm->sidno_to_sid(cs->sidno)->to_string(uuid);
      s += sprintf(s, "  %s:%lld%s [%lld bytes] - %s\n",
                   uuid, cs->gno, cs->group_end ? "-END":"", cs->binlog_length,
                   cs->type == NORMAL_SUBGROUP ? "NORMAL" :
                   cs->type == ANONYMOUS_SUBGROUP ? "ANON" :
                   cs->type == DUMMY_SUBGROUP ? "DUMMY" :
                   "INVALID-SUBGROUP-TYPE");
    }
    sprintf(s, "}\n");
    return s - buf;
  }
  size_t get_string_length() const
  {
    return (2 + Uuid::TEXT_LENGTH + 1 + MAX_GNO_TEXT_LENGTH + 4 + 2 +
            40 + 10 + 21 + 1 + 100/*margin*/) * get_n_subgroups() + 100/*margin*/;
  }
  char *to_string(const Sid_map *sm) const
  {
    char *str= (char *)malloc(get_string_length());
    to_string(sm, str);
    return str;
  }
  void print(const Sid_map *sm) const
  {
    char *str= to_string(sm);
    printf("%s\n", str);
    free(str);
  }
#endif

private:

  /// List of all subgroups in this cache, of type Cached_subgroup.
  DYNAMIC_ARRAY subgroups;

  /**
    Returns a pointer to the given subgroup.  The pointer is only
    valid until the next time a sub-group is added or removed.

    @param index Index of the element: 0 <= index < get_n_subgroups().
  */
  inline Cached_subgroup *get_unsafe_pointer(int index) const
  {
    DBUG_ASSERT(index >= 0 && index < get_n_subgroups());
    return dynamic_element(&subgroups, index, Cached_subgroup *);
  }
  /**
    Adds the given sub-group to this group cache, or merges it with the
    last existing sub-group in the cache if they are compatible.

    @param subgroup The subgroup to add.
    @return RETURN_STATUS_OK or RETURN_STATUS_REPORTED_ERROR.
  */
  enum_return_status add_subgroup(const Cached_subgroup *subgroup);
  /**
    Prepare the cache to be written to the group log.

    @todo The group log is not yet implemented. /Sven

    @param trx_group_cache @see write_to_log.
    @return RETURN_STATUS_OK or RETURN_STATUS_REPORTED_ERROR.
  */
  enum_return_status
    write_to_log_prepare(Group_cache *trx_group_cache,
                         rpl_binlog_pos offset_after_last_statement,
                         Cached_subgroup **last_non_dummy_subgroup);

  /// Used by unit tests that need to access private members.
#ifdef FRIEND_OF_GROUP_CACHE
  friend FRIEND_OF_GROUP_CACHE;
#endif
};


/**
  Represents a bidirectional map between binlog file names and
  binlog_no.
*/
class Binlog_map
{
public:
  rpl_binlog_no filename_to_binlog_no(const char *filename) const;
  void binlog_no_to_filename(rpl_sid sid, char *buf) const;
private:
  rpl_binlog_no number_offset;
  DYNAMIC_ARRAY binlog_no_to_filename_map;
  HASH filename_to_binlog_no_map;
};


/**
  Indicates if a statement should be skipped or not. Used as return
  value from ugid_before_statement.
*/
enum enum_ugid_statement_status
{
  /// Statement can execute.
  UGID_STATEMENT_EXECUTE,
  /// Statement should be cancelled.
  UGID_STATEMENT_CANCEL,
  /**
    Statement should be skipped, but there may be an implicit commit
    after the statement if ugid_commit is set.
  */
  UGID_STATEMENT_SKIP
};


#ifndef MYSQL_CLIENT
/**
  Before a loggable statement begins, this function:

   - checks that the various @@session.ugid_* variables are consistent
     with each other

   - starts the super-group (if no super-group is active) and acquires
     ownership of all groups in the super-group

   - starts the group (if no group is active)
*/
enum_ugid_statement_status
ugid_before_statement(THD *thd, Checkable_rwlock *lock,
                      Group_log_state *gls,
                      Group_cache *gsc, Group_cache *gtc);
/**
  Before the transaction cache is flushed, this function checks if we
  need to add an ending dummy groups sub-groups.
*/
int ugid_before_flush_trx_cache(THD *thd, Checkable_rwlock *lock,
                                Group_log_state *gls, Group_cache *gc);
int ugid_flush_group_cache(THD *thd, Checkable_rwlock *lock,
                           Group_log_state *gls,
                           Group_log *gl,
                           Group_cache *gc, Group_cache *trx_cache,
                           rpl_binlog_no binlog_no, rpl_binlog_pos binlog_pos,
                           rpl_binlog_pos offset_after_last_statement);
#endif // ifndef MYSQL_CLIENT


/**
  File class that supports the following operations:

   - pread: read from given position in file
   - append: append data to end of file
   - truncate_and_append: atomically truncate the file and then append
     data to the end of it

  This class does not have any built-in concurrency control; the
  truncate_and_append operation is only atomic in the sense that it is
  crash-safe.

  To achieve crash-safeness, this class uses a second file internally
  (not visible to the user of the class).
*/
class Atom_file
{
public:
  Atom_file() : fd(-1), ofd(-1) {}
  /**
    Open the given file.
    @param write If true, the file will be writable, and it will be
    recovered if the 'overwrite' file exists.  Otherwise it is
    read-only and the 'overwrite' file will not be touched.
    @return RETURN_STATUS_OK or RETURN_STATUS_REPORTED_ERROR.
  */
  enum_return_status open(const char *filename, bool write);
  /**
    Close the current file.
    @return RETURN_STATUS_OK or RETURN_STATUS_REPORTED_ERROR.
  */
  enum_return_status close();
  /// Return true iff this file has been (successfully) opened by open().
  bool is_open() const { return fd != -1; }
  /// Return true iff this file is open and writable.
  bool is_writable() const { return fd != -1 && writable; }
  /**
    Non-atomically append the given data to the end of this Atom_file.
    @param length The number of bytes to write.
    @param data The bytes to write.
    @return The number of bytes written.  This function calls my_error on error.
  */
  size_t append(const uchar *data, my_off_t length)
  {
    DBUG_ENTER("Atom_file::append");
    DBUG_ASSERT(is_writable());
    my_off_t ret= my_write(fd, data, length, MYF(MY_WME));
    DBUG_RETURN(ret);
  }
  /**
    Read data from a given position in the file.
    @param offset The offset to start reading from.
    @param length The number of bytes to read.
    @param buffer Buffer to save data in.
    @return The number of bytes read. This function calls my_error on error.
  */
  size_t pread(my_off_t offset, uchar *buffer, my_off_t length) const;
  /**
    Atomically truncate the file to the given offset, then append data
    at the end.
    @return RETURN_STATUS_OK or RETURN_STATUS_REPORTED_ERROR.
  */
  enum_return_status truncate_and_append(my_off_t offset,
                                         const uchar *data, my_off_t length);
  /**
    Sync the file to disk.
    @return RETURN_STATUS_OK or RETURN_STATUS_REPORTED_ERROR.
  */
  enum_return_status sync()
  {
    DBUG_ENTER("Atom_file::sync()");
    DBUG_ASSERT(is_writable());
    if (my_sync(fd, MYF(MY_WME)))
      RETURN_REPORTED_ERROR;
    RETURN_OK;
  }
  /// Destroy this Atom_file.
  ~Atom_file() { DBUG_ASSERT(!is_open()); }
private:
  static const int HEADER_LENGTH= 9;
  /**
    Close and remove the 'overwrite' file.
    @return RETURN_STATUS_OK or RETURN_STATUS_REPORTED_ERROR.
  */
  enum_return_status rollback();
  /**
    Copy the 'overwrite' file to the given position of the main file,
    then truncate the main file.
    The 'overwrite' file is supposed to be open and positioned after the header.
    @param offset Offset in main file to write to.
    @param length The length of the 'overwrite' file, minus the header.
    @return RETURN_STATUS_OK or RETURN_STATUS_REPORTED_ERROR.
  */
  enum_return_status commit(my_off_t offset, my_off_t length);
  /**
    Perform 'recovery' when the file is opened.
    @return RETURN_STATUS_OK or RETURN_STATUS_REPORTED_ERROR.
  */
  enum_return_status recover();
  /// Suffix of 'overwrite' file: contains the string ".overwrite".
  static const char *OVERWRITE_FILE_SUFFIX;
  /// Name of main file.
  char filename[FN_REFLEN];
  /// Name of 'overwrite' file.
  char overwrite_filename[FN_REFLEN];
  /// File descriptor of main file.
  File fd;
  /// True if this Atom_file has been open in writable mode.
  bool writable;
  /// File descriptor of 'overwrite' file.
  File ofd;
  /**
    If this file is read-only, but the 'overwrite' file existed when
    the file was opened, then this caches the position in the main
    file where the 'overwrite' file starts.
  */
  my_off_t overwrite_offset;
};


/**
  Class that implements a file with fifo-like characteristics:

   - The front-end looks like a single file where you can append,
     pread, truncate, and purge.  The append, pread, and truncate
     operations are as for normal files.  The purge operation removes
     the beginning of the file, but leaves the remainder of the file
     at the same logical position.  E.g., if the first 1000 bytes are
     purged, then it becomes invalid to pread from position 999, while
     a read from position 1000 returns the same data as it would have
     done before the purge.

   - The back-end is a sequence of files with hexadecimal file name
     suffixes.  The purge operation removes the first few such files,
     until the file that contains the first position after the purge.
*/
class Rot_file : public Appender
{
public:
  /// Create a new Rot_file that is not open.
  Rot_file() { sub_file.fd= -1; }
  /**
    Open the Rot_file.

    @param filename Base name for the files.
    @param write If true, this Rot_file is writable.
    @return RETURN_STATUS_OK or RETURN_STATUS_REPORTED_ERROR.
  */
  enum_return_status open(const char *filename, bool writable);
  /**
    Close the Rot_file.
    @return RETURN_STATUS_OK or RETURN_STATUS_REPORTED_ERROR.
  */
  enum_return_status close();
protected:
  /// @see Appender::do_append
  enum_return_status do_append(const uchar *data, size_t length)
  {
    DBUG_ENTER("Rot_file::append");
    DBUG_ASSERT(is_writable());
    if (my_write(sub_file.fd, data, length, MYF(MY_WME | MY_WAIT_IF_FULL)) !=
        length)
      RETURN_REPORTED_ERROR;
    RETURN_OK;
  }
  /// @see Appender::do_truncate
  enum_return_status do_truncate(my_off_t offset);
  /// @see Appender::do_tell
  enum_return_status do_tell(my_off_t *position) const
  {
    DBUG_ENTER("Rot_file::tell");
    PROPAGATE_REPORTED_ERROR(file_tell(sub_file.fd, position));
    *position-= sub_file.header_length;
    RETURN_OK;
  }
  /// Return real filename of the last file (the one being written).
  const char *do_get_source_name() const { return sub_file.filename; }
public:
  /**
    Sync any pending data.
    @return RETURN_STATUS_OK or RETURN_STATUS_REPORTED_ERROR.
  */
  enum_return_status sync()
  {
    DBUG_ENTER("Rot_file::sync");
    DBUG_ASSERT(is_writable());
    if (my_sync(sub_file.fd, MYF(MY_WME)) != 0)
      RETURN_REPORTED_ERROR;
    RETURN_OK;
  }
  /**
    Set the limit at which files will be rotated.

    This does not retroactively change the limit of already existing
    files.  It only affects subsequenct calls to append().

    @param limit The new limit.
  */
  void set_rotation_limit(my_off_t limit) { rotation_limit= limit; }
  /// Return the limit previously set by set_rotation_limit().
  my_off_t get_rotation_limit() const { return rotation_limit; }
  /**
    Remove back-end files up to the given position.
    @param offset The first valid position in the file after this call.
    @return RETURN_STATUS_OK or RETURN_STATUS_REPORTED_ERROR.
  */
  enum_return_status purge(my_off_t offset);
  /// Return true iff this file is open and was opened in read mode.
  bool is_writable() const { return is_open() && _is_writable; }
  /// Return true iff this file is open.
  bool is_open() const { return _is_open; }
  /**
    Destroy this Rot_file object.  The file must be closed before this
    function is called.
  */
  ~Rot_file();
  
  /**
    Iterator class that sequentially reads from the file.

    @todo In the future, when we have implemented real, rotatable
    files, each iterator object blocks the parent Rot_file from
    purging the underlying file.  It also forces the underlying file
    to stay open until the iterator object iterates to the next file
    or is closed. /sven
  */
  class Rot_file_reader : public Reader
  {
  public:
    /**
      Create a new Reader for the given Rot_file.
      @param rot_file The parent Rot_file.
      @param position The initial read position. 
    */
    Rot_file_reader(Rot_file *_rot_file, my_off_t position)
      : rot_file(_rot_file)
    { seek(position); }
    ~Rot_file_reader() {};
    enum_read_status read(uchar *buffer, size_t length)
    {
      DBUG_ENTER("Rot_file_reader::read");
      PROPAGATE_READ_STATUS(file_pread(rot_file->sub_file.fd, buffer, length,
                                       rot_file->sub_file.header_length + pos));
      pos+= length;
      DBUG_RETURN(READ_OK);
    }
    enum_read_status seek(my_off_t new_position)
    {
      DBUG_ENTER("Rot_file_reader::seek");
      PROPAGATE_READ_STATUS(file_seek(rot_file->sub_file.fd,
                                      pos, new_position));
      pos= new_position;
      DBUG_RETURN(READ_OK);
    }
    enum_return_status tell(my_off_t *position) const
    { *position= pos; return RETURN_STATUS_OK; }
    const char *get_source_name() const { return rot_file->sub_file.filename; }
  private:
    my_off_t pos;
    Rot_file *rot_file;
  };
private:
  /// Number of bytes of this file's header (fake implementation)
  int header_length;
  /// Base filename, the logical filename specified by the constructor.
  char base_filename[FN_REFLEN];
  my_off_t rotation_limit;
  bool _is_writable;
  bool _is_open;
  struct Sub_file
  {
    /// File descriptor.
    File fd;
    /// Length of the header.
    my_off_t header_length;
    /// Filename of the real file.
    char filename[FN_REFLEN];
    /// Offset (to be implemented)
    //my_off_t offset;
    /// Number of this file (to be implemented)
    //int index;
    /**
      Number of Reader objects that access this file, plus 1 if this
      is the last file and the Rotfile is writable.
    */
    //int ref_count;
  };
  /**
    The one and only file in this fake implementation.  The final
    version should have a DYNAMIC_ARRAY of files.
  */
  Sub_file sub_file;
  File_appender appender;
/*
  enum enum_state { CLOSED, OPEN_READ, OPEN_READ_WRITE };
  char filename[FN_REFLEN];
  int fd;
  enum_state state;
  my_off_t limit;
*/
};


/**
  Class to encode/decode sub-groups.
*/
class Subgroup_coder
{
public:
  Subgroup_coder() : lgid(0), offset(0) {}
  enum_append_status append(Appender *appender, const Cached_subgroup *cs,
                            rpl_binlog_no binlog_no, rpl_binlog_pos binlog_pos,
                            rpl_binlog_pos offset_after_last_statement,
                            bool group_commit, uint32 owner_type);
  enum_read_status read(Reader *reader, Subgroup *s, uint32 *owner_type);
private:
  rpl_lgid lgid;
  my_off_t offset;
  /// Constants used to encode groups.
  static const int TINY_SUBGROUP_MAX= 246, TINY_SUBGROUP_MIN= -123;
  static const int NORMAL_SUBGROUP_MIN= 247,
    NORMAL_SUBGROUP_SIDNO_BIT= 0, NORMAL_SUBGROUP_GNO_BIT= 1,
    NORMAL_SUBGROUP_BINLOG_LENGTH_BIT= 2;
  static const int SPECIAL_TYPE= 255;
  static const int DUMMY_SUBGROUP_MAX= 15;
  static const int ANONYMOUS_SUBGROUP_COMMIT= 16,
    ANONYMOUS_SUBGROUP_NO_COMMIT= 17;
  static const int BINLOG_ROTATE= 18;
  static const int BINLOG_OFFSET_AFTER_LAST_STATEMENT= 19;
  static const int BINLOG_GAP= 20;
  static const int FLIP_OWNER= 21, SET_OWNER= 22;
  static const int FULL_SUBGROUP= 26;
  static const int MIN_IGNORABLE_TYPE= 25;
  static const int MIN_FATAL_TYPE= 28;
  static const my_off_t FULL_SUBGROUP_SIZE=
    1 + 4 + 8 + 8 + 8 + 8 + 8 + 4 + 1 + 1;
};


class Group_log
{
public:
  Group_log(Sid_map *_sid_map) : sid_map(_sid_map), rot_file() {}
  /**
    Open the disk file.
    @param filename Name of file to open.
    @param writable If false, the group log is opened in read-only mode.
    @return RETURN_STATUS_OK or RETURN_STATUS_REPORTED_ERROR.
  */
  enum_return_status open(const char *filename, bool writable= true);
  /**
    Close the disk file.
    @return RETURN_STATUS_OK or RETURN_STATUS_REPORTED_ERROR.
  */
  enum_return_status close();
  /**
    Return true iff this Group_log has been (successfully) opened.
  */
  bool is_open() { return rot_file.is_open(); }
#ifndef MYSQL_CLIENT
  /**
    Writes the given Subgroup to the log.
    @param Subgroup Subgroup to write.
    @return RETURN_STATUS_OK or RETURN_STATUS_REPORTED_ERROR.
  */
  enum_return_status write_subgroup(const THD *thd,
                                    const Cached_subgroup *subgroup,
                                    rpl_binlog_no binlog_no,
                                    rpl_binlog_pos binlog_pos,
                                    rpl_binlog_pos offset_after_last_statement,
                                    bool group_commit);
#endif // ifndef MYSQL_CLIENT

  /**
    Iterator class that sequentially reads groups from the log.
  */
  class Group_log_reader
  {
  public:
    /**
      Construct a new Reader object, capable of reading this Group_log.

      @param group_log The group log to read from.  
      @param group_set Group_set to start from.  When reading
      sub-groups, SIDs will be stored using the Sid_map of this
      Group_set.
      @param binlog_no binlog to start from.
      @param binlog_pos position to start from in binlog.
      @param status Will be set to 0 on success, 1 on error. This
      function calls my_error.
    */
    Group_log_reader(Group_log *group_log, Sid_map *sid_map);
    /// Destroy this Group_log_reader.
    ~Group_log_reader() {}
    /**
      Move the reading position forwards to the given position.

      @param group_set If group_set is not NULL, seek and subsequent
      calls to read_subgroup will ignore groups that are NOT IN
      group_set (if exclude_group_set == false) respectively groups
      that are IN group_set (if exclude_group_set == true.)
      @param exclude_group_set Determines if groups in group_set are
      to be included or excluded; see above.
      @param include_anonymous If true, anonymous subgroups will be
      included.
      @param first_lgid First LGID to include (or -1 to start from the
      beginning).
      @param last_lgid Last LGID to include (or -1 to continue to the
      end).
      @param binlog_no If this is not 0, will seek to this binary log
      (or longer, depending on group_set).
      @param binlog_pos If binlog_no is not 0, will seek to this
      position within the binary log (or longer, depending on
      group_set).
      my_b_read(). This is used in mysqlbinlog if the binary log is
      stdin.
    */
    enum_read_status seek(const Group_set *group_set, bool exclude_group_set,
                          bool include_anonymous,
                          rpl_lgid first_lgid, rpl_lgid last_lgid,
                          rpl_binlog_no binlog_no, rpl_binlog_pos binlog_pos);
    /**
      Read the next subgroup from this Group_log.
      @param[out] subgroup Subgroup object where the subgroup will be stored.
      @param[out] owner_type If this is not NULL, the owner_type will
      be stored here.
      @retval READ_EOF End of file.
      @retval READ_SUCCES Ok.
      @retval READ_ERROR IO error. This function calls my_error.
    */
    enum_read_status read_subgroup(Subgroup *subgroup,
                                   uint32 *owner_type= NULL);
    /**
      Read the next subgroup from this Group_log but do not advance
      the read position.  The subgroup is stored in an internal buffer
      and the caller is given the pointer to this buffer.  The
      internal buffer will be overwritten by subsequent calls to
      peek_subgroup() or read_subgroup() or seek().

      @param[out] subgroup Subgroup* object where the subgroup will be stored.
      @param[out] owner_type If this is not NULL, the owner_type will
      be stored here.
      @retval READ_EOF End of file.
      @retval READ_SUCCES Ok.
      @retval READ_ERROR IO error. This function calls my_error.
    */
    enum_read_status peek_subgroup(Subgroup **subgroup,
                                   uint32 *owner_type= NULL);
    const char *get_current_filename() const
    { return rot_file_reader.get_source_name(); }
  private:
    /**
      Return true if the given subgroup is in the set specified by the
      arguments to seek().
    */
    bool subgroup_in_valid_set(Subgroup *subgroup);
    /**
      Unconditionally read subgroup from file.

      This is different from read_subgroup(), because read_subgroup()
      will re-use the subgroup from peeked_subgroup if has_peeked is
      true.
    */
    enum_read_status do_read_subgroup(Subgroup *subgroup,
                                      uint32 *owner_type= NULL);

    /// Sid_map to use in returned Sub_groups.
    Sid_map *output_sid_map;
    /// Sid_map used by Sub_groups in the log.
    Group_log *group_log;
    /**
      Only include groups in this set, or only include groups outside
      this set if exclude_group_set == false.  If this is NULL,
      include all groups.
    */
    const Group_set *group_set;
    /**
      If false, this reader only returns groups IN group_set.  If
      true, this reader only returns groups NOT IN group_set.
    */
    bool exclude_group_set;
    /// If true, this reader will return any anonymous groups it finds.
    bool include_anonymous;
    /// First LGID to include, or -1 to start from the beginning.
    rpl_lgid first_lgid;
    /// Last LGID to include, or -1 to continue to the end.
    rpl_lgid last_lgid;
    /// Reader object from which we read raw bytes.
    Rot_file::Rot_file_reader rot_file_reader;
    /// State needed to decode groups.
    Subgroup_coder decoder;
    /// Max size required by any subgroup.
    static const int READ_BUF_SIZE= 256;
    /// Internal buffer to decode subgroups. 
    uchar read_buf[READ_BUF_SIZE];
    /// Next subgroup in the group log.
    Subgroup peeked_subgroup;
    /// true iff peeked_subgroup contains anything meaningful.
    bool has_peeked;
  };

  Sid_map *get_sid_map() const { return sid_map; }

private:
  /// Lock protecting the Sid_map.
  mutable Checkable_rwlock *rwlock;
  /// Sid_map used for this log.
  Sid_map *sid_map;
  /// Rot_file to which groups will be written.
  Rot_file rot_file;
  /// State needed to encode groups.
  Subgroup_coder encoder;
};


/**
  Auxiliary class for reading and writing compact-encoded numbers to
  file.
*/
class Compact_coder
{
public:
  static const int MAX_ENCODED_LENGTH= 10;
  /**
    Compute the number of bytes needed to encode the given number.
    @param n The number
    @return The number of bytes needed to encode n.
  */
  static int get_unsigned_encoded_length(ulonglong n);
  /**
    Write a compact-encoded unsigned integer to the given buffer.
    @param n The number to write.
    @param buf[out] The buffer to use.
    @return The number of bytes used.
  */
  static int write_unsigned(uchar *buf, ulonglong n);
  /**
    Write a compact-encoded unsigned integer to the given file.

    @param fd File to write to.
    @param n Number to write.
    @param myf MYF() flags.

    @return On success, returns number of bytes written (1...10).  On
    failure, returns the negative number of bytes written (-9..0).
  */
  static enum_append_status append_unsigned(Appender *appender, ulonglong n);
  /**
    Read a compact-encoded unsigned integer from the given file.
    
    @param fd File to write to.
    @param out[out] On success, the number is stored in *out.
    On failure, the reason is stored in *out:
     - On IO error, *out is set to 0.
     - If the file ended abruptly in the middle of the number, *out is set to 1.
     - If the file format was not valid, *out is set to 2.
    @param myf MYF() flags passed to my_read() and used by this
    function.  In particular, this function generates an error in all
    error cases if MY_WME is set.
    @return On success, returns the number of bytes read (1...10).  On
    error, returns the negative number of bytes read (-10...0).
  */
  static enum_read_status read_unsigned(Reader *reader, ulonglong *out);
  /**
    Like read_unsigned(Reader *, ulonglong *), but throws an error if
    the value is greater than max_value.
  */
  static enum_read_status read_unsigned(Reader *reader, ulonglong *out,
                                        ulonglong max_value)
  {
    DBUG_ENTER("Compact_coder::read_unsigned(Reader *, ulonglong *, ulonglong)");
    PROPAGATE_READ_STATUS(read_unsigned(reader, out));
    if (*out > max_value)
    {
      my_off_t ofs;
      reader->tell(&ofs);
      BINLOG_ERROR(("File '%.200s' has an unknown format at position %lld, "
                    "it may be corrupt.",
                    reader->get_source_name(), ofs),
                   (ER_FILE_FORMAT, MYF(0), reader->get_source_name(), ofs));
      DBUG_RETURN(READ_ERROR);
    }
    DBUG_RETURN(READ_OK);
  }
  /// Like read_unsigned(Reader *, ulonglong *), but optimized for 32-bit.
  static enum_read_status read_unsigned(Reader *reader, ulong *out);
  /// Like read_unsigned(Reader *, ulong *, ulong), but optimized for 32-bit.
  static enum_read_status read_unsigned(Reader *reader, ulong *out,
                                        ulong max_value)
  {
    DBUG_ENTER("Compact_coder::read_unsigned(Reader *, ulong *, ulong)");
    PROPAGATE_READ_STATUS(read_unsigned(reader, out));
    if (*out > max_value)
    {
      my_off_t ofs;
      reader->tell(&ofs);
      BINLOG_ERROR(("File '%.200s' has an unknown format at position %lld, "
                    "it may be corrupt.",
                    reader->get_source_name(), ofs),
                   (ER_FILE_FORMAT, MYF(0), reader->get_source_name(), ofs));
      DBUG_RETURN(READ_ERROR);
    }
    DBUG_RETURN(READ_OK);
  }
  /**
    Compute the number of bytes needed to encode the given number.
    @param n The number
    @return The number of bytes needed to encode n.
  */
  static int get_signed_encoded_length(longlong n)
  { return get_unsigned_encoded_length(signed_to_unsigned(n)); }
  /// Encode a signed number as unsigned.
  static ulonglong signed_to_unsigned(longlong n)
  { return n >= 0 ? 2 * (ulonglong)n : 1 + 2 * (ulonglong)-(n + 1); }
  /// Decode a signed number from an unsigned.
  static longlong unsigned_to_signed(ulonglong n)
  { return (n & 1) ? n >> 1 : -1 - (longlong)(n >> 1); }
  /**
    Write a compact-encoded signed integer to the given file.
    @see write_unsigned.
  */
  static enum_append_status append_signed(Appender *appender, longlong n);
  /**
    Read a compact-encoded signed integer from the given file.
    @see read_unsigned.
  */
  static enum_read_status read_signed(Reader *reader, longlong *out);
  /**
    Read a type code from the given file.

    This is used for type codes in, e.g., sid_map files, binlog_map
    files, group_log files, group_index files.

    The type code is one byte.  Callers specify which type codes are
    known and which are unknown.  Even unknown type codes are fatal:
    an error is generated if such a type code is found.  Odd unknown
    type codes are ignorable: the type code is followed by an unsigned
    compact-encoded integer, N, followed by N bytes of data.  If an
    odd unknown type code is found, this function seeks past the data
    and returns ok.

    @param reader Reader object to read from.
    @param min_fatal The smallest even number that is a fatal error.
    @param min_ignorable The smallest odd number that is not a
    recognized type code and shall be ignored.
    @param[out] out The type code will be stored here.
    @param code By default, the code is read from the first byte of
    the Reader.  If this parameter is given, the code is instead the
    value of this parameter and it is assumed that the Reader is
    positioned after the type code.
  */
  static enum_read_status read_type_code(Reader *reader,
                                         int min_fatal, int min_ignorable,
                                         uchar *out, int code= -1);
  /**
    Read a compact-encoded integer - the string length - followed by a
    string of that length.

    @param reader Reader object to read from.
    @param buf[out] Buffer to save string in.
    @param length[out] The string length will be stored here.
    @param max_length The maximal number of characters of buf that may
    be touched.
    @param null_terminated If true, the string will be null-terminated
    and the maximal number of allowed non-null characters will be
    max_length - 1.
  */
  static enum_read_status read_string(Reader *reader,
                                      uchar *buf, size_t *length,
                                      size_t max_length, bool null_terminated);
  /**
    Append a compact-encoded string, in the form of a compact-encoded
    integer followed by the string characters.

    @param reader Reader object to read from.
    @param string String to write.
    @param length Number of characters to write.  If omitted,
    strlen(string) characters will be written.
  */
  static enum_append_status append_string(Appender *appender,
                                          const uchar *string, size_t length);
  static enum_append_status append_string(Appender *appender,
                                          const uchar *string)
  { return append_string(appender, string, strlen((const char *)string)); }
};


#endif /* HAVE_UGID */

#endif /* RPL_GROUPS_H_INCLUDED */
