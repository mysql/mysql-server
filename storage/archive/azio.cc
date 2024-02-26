/*
  azio is a modified version of gzio. It  makes use of mysys and removes
  mallocs. -Brian Aker
*/

/* gzio.c -- IO on .gz files
 * Copyright (C) 1995-2005 Jean-loup Gailly.
 * For conditions of distribution and use, see copyright notice in zlib.h
 *
 * This file was modified by Oracle on 2015-01-23.
 * Modifications Copyright (c) 2015, 2023, Oracle and/or its affiliates.
 */

/* @(#) $Id$ */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>

#include "my_byteorder.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_io.h"
#include "my_psi_config.h"
#include "my_thread_local.h"
#include "mysql/psi/mysql_file.h"
#include "storage/archive/azlib.h"

static int const gz_magic[2] = {0x1f, 0x8b};       /* gzip magic header */
static int const az_magic[3] = {0xfe, 0x03, 0x01}; /* az magic header */

/* gzip flag uchar */
#define HEAD_CRC 0x02    /* bit 1 set: header CRC present */
#define EXTRA_FIELD 0x04 /* bit 2 set: extra field present */
#define ORIG_NAME 0x08   /* bit 3 set: original file name present */
#define COMMENT 0x10     /* bit 4 set: file comment present */
#define RESERVED 0xE0    /* bits 5..7: reserved */

int az_open(azio_stream *s, const char *path, int Flags, File fd);
int do_flush(azio_stream *file, int flush);
int get_byte(azio_stream *s);
void check_header(azio_stream *s);
int write_header(azio_stream *s);
int destroy(azio_stream *s);
void putLong(File file, uLong x);
uLong getLong(azio_stream *s);
void read_header(azio_stream *s, unsigned char *buffer);

#ifdef HAVE_PSI_INTERFACE
extern "C" PSI_file_key arch_key_file_data;
#endif

/* ===========================================================================
  Opens a gzip (.gz) file for reading or writing. The mode parameter
  is as in fopen ("rb" or "wb"). The file is given either by file descriptor
  or path name (if fd == -1).
  az_open returns NULL if the file could not be opened or if there was
  insufficient memory to allocate the (de)compression state; errno
  can be checked to distinguish the two cases (if errno is zero, the
  zlib error is Z_MEM_ERROR).
*/
int az_open(azio_stream *s, const char *path, int Flags, File fd) {
  int err;
  int level = Z_DEFAULT_COMPRESSION; /* compression level */
  int strategy = Z_DEFAULT_STRATEGY; /* compression strategy */

  memset(s, 0, sizeof(azio_stream));
  s->stream.next_in = s->inbuf;
  s->stream.next_out = s->outbuf;
  assert(s->z_err == Z_OK);
  s->back = EOF;
  s->crc = crc32(0L, Z_NULL, 0);
  s->mode = 'r';
  /* this needs to be a define to version */
  s->version = (unsigned char)az_magic[1];
  s->minor_version = (unsigned char)az_magic[2]; /* minor version */
  assert(s->dirty == AZ_STATE_CLEAN);

  /*
    We do our own version of append by nature.
    We must always have write access to take card of the header.
  */
  assert(Flags | O_APPEND);
  assert(Flags | O_WRONLY);

  if (Flags & O_RDWR) s->mode = 'w';

  if (s->mode == 'w') {
    err =
        deflateInit2(&(s->stream), level, Z_DEFLATED, -MAX_WBITS, 8, strategy);
    /* windowBits is passed < 0 to suppress zlib header */

    s->stream.next_out = s->outbuf;
    if (err != Z_OK) {
      destroy(s);
      return Z_NULL;
    }
  } else {
    s->stream.next_in = s->inbuf;

    err = inflateInit2(&(s->stream), -MAX_WBITS);
    /* windowBits is passed < 0 to tell that there is no zlib header.
     * Note that in this case inflate *requires* an extra "dummy" byte
     * after the compressed stream in order to complete decompression and
     * return Z_STREAM_END. Here the gzip CRC32 ensures that 4 bytes are
     * present after the compressed stream.
     */
    if (err != Z_OK) {
      destroy(s);
      return Z_NULL;
    }
  }
  s->stream.avail_out = AZ_BUFSIZE_WRITE;

  errno = 0;
  s->file =
      fd < 0 ? mysql_file_open(arch_key_file_data, path, Flags, MYF(0)) : fd;
  DBUG_EXECUTE_IF("simulate_archive_open_failure", {
    if (s->file >= 0) {
      my_close(s->file, MYF(0));
      s->file = -1;
      set_my_errno(EMFILE);
    }
  });

  if (s->file < 0) {
    destroy(s);
    return Z_NULL;
  }

  if (Flags & O_CREAT || Flags & O_TRUNC) {
    s->dirty = 1; /* We create the file dirty */
    s->start = AZHEADER_SIZE + AZMETA_BUFFER_SIZE;
    write_header(s);
    my_seek(s->file, 0, MY_SEEK_END, MYF(0));
  } else if (s->mode == 'w') {
    uchar buffer[AZHEADER_SIZE + AZMETA_BUFFER_SIZE];
    my_pread(s->file, buffer, AZHEADER_SIZE + AZMETA_BUFFER_SIZE, 0, MYF(0));
    read_header(s, buffer); /* skip the .az header */
    my_seek(s->file, 0, MY_SEEK_END, MYF(0));
  } else {
    check_header(s); /* skip the .az header */
  }

  return 1;
}

int write_header(azio_stream *s) {
  uchar buffer[AZHEADER_SIZE + AZMETA_BUFFER_SIZE];
  uchar *ptr = buffer;

  if (s->version == 1) return 0;

  s->block_size = AZ_BUFSIZE_WRITE;
  s->version = (unsigned char)az_magic[1];
  s->minor_version = (unsigned char)az_magic[2];

  /* Write a very simple .az header: */
  memset(buffer, 0, AZHEADER_SIZE + AZMETA_BUFFER_SIZE);
  *(ptr + AZ_MAGIC_POS) = az_magic[0];
  *(ptr + AZ_VERSION_POS) = (unsigned char)s->version;
  *(ptr + AZ_MINOR_VERSION_POS) = (unsigned char)s->minor_version;
  *(ptr + AZ_BLOCK_POS) =
      (unsigned char)(s->block_size / 1024); /* Reserved for block size */
  *(ptr + AZ_STRATEGY_POS) =
      (unsigned char)Z_DEFAULT_STRATEGY; /* Compression Type */

  int4store(ptr + AZ_FRM_POS, s->frm_start_pos);             /* FRM Block */
  int4store(ptr + AZ_FRM_LENGTH_POS, s->frm_length);         /* FRM Block */
  int4store(ptr + AZ_COMMENT_POS, s->comment_start_pos);     /* COMMENT Block */
  int4store(ptr + AZ_COMMENT_LENGTH_POS, s->comment_length); /* COMMENT Block */
  int4store(ptr + AZ_META_POS, 0);                           /* Meta Block */
  int4store(ptr + AZ_META_LENGTH_POS, 0);                    /* Meta Block */
  int8store(ptr + AZ_START_POS,
            (unsigned long long)s->start); /* Start of Data Block Index Block */
  int8store(ptr + AZ_ROW_POS,
            (unsigned long long)s->rows); /* Start of Data Block Index Block */
  int8store(ptr + AZ_FLUSH_POS,
            (unsigned long long)
                s->forced_flushes); /* Start of Data Block Index Block */
  int8store(
      ptr + AZ_CHECK_POS,
      (unsigned long long)s->check_point); /* Start of Data Block Index Block */
  int8store(ptr + AZ_AUTOINCREMENT_POS,
            (unsigned long long)
                s->auto_increment); /* Start of Data Block Index Block */
  int4store(ptr + AZ_LONGEST_POS, s->longest_row);   /* Longest row */
  int4store(ptr + AZ_SHORTEST_POS, s->shortest_row); /* Shorest row */
  int4store(ptr + AZ_FRM_POS,
            AZHEADER_SIZE + AZMETA_BUFFER_SIZE); /* FRM position */
  *(ptr + AZ_DIRTY_POS) =
      (unsigned char)s->dirty; /* Start of Data Block Index Block */

  /* Always begin at the beginning, and end there as well */
  return my_pwrite(s->file, (uchar *)buffer, AZHEADER_SIZE + AZMETA_BUFFER_SIZE,
                   0, MYF(MY_NABP))
             ? 1
             : 0;
}

/* ===========================================================================
  Opens a gzip (.gz) file for reading or writing.
*/
int azopen(azio_stream *s, const char *path, int Flags) {
  return az_open(s, path, Flags, -1);
}

/* ===========================================================================
  Associate a gzFile with the file descriptor fd. fd is not dup'ed here
  to mimic the behavio(u)r of fdopen.
*/
int azdopen(azio_stream *s, File fd, int Flags) {
  if (fd < 0) return 0;

  return az_open(s, nullptr, Flags, fd);
}

/* ===========================================================================
  Read a byte from a azio_stream; update next_in and avail_in. Return EOF
  for end of file.
  IN assertion: the stream s has been successfully opened for reading.
*/
int get_byte(azio_stream *s) {
  if (s->z_eof) return EOF;
  if (s->stream.avail_in == 0) {
    errno = 0;
    s->stream.avail_in = (uInt)mysql_file_read(s->file, (uchar *)s->inbuf,
                                               AZ_BUFSIZE_READ, MYF(0));
    if (s->stream.avail_in == 0) {
      s->z_eof = 1;
      return EOF;
    } else if (s->stream.avail_in == (uInt)-1) {
      s->z_eof = 1;
      s->z_err = Z_ERRNO;
      return EOF;
    }
    s->stream.next_in = s->inbuf;
  }
  s->stream.avail_in--;
  return *(s->stream.next_in)++;
}

/* ===========================================================================
  Check the gzip header of a azio_stream opened for reading. Set the stream
  mode to transparent if the gzip magic header is not present; set s->err
  to Z_DATA_ERROR if the magic header is present but the rest of the header
  is incorrect.
  IN assertion: the stream s has already been created successfully;
  s->stream.avail_in is zero for the first time, but may be non-zero
  for concatenated .gz files.
*/
void check_header(azio_stream *s) {
  int method; /* method uchar */
  int flags;  /* flags uchar */
  uInt len;
  int c;

  /* Assure two bytes in the buffer so we can peek ahead -- handle case
    where first byte of header is at the end of the buffer after the last
    gzip segment */
  len = s->stream.avail_in;
  if (len < 2) {
    if (len) s->inbuf[0] = s->stream.next_in[0];
    errno = 0;
    len = (uInt)mysql_file_read(s->file, (uchar *)s->inbuf + len,
                                AZ_BUFSIZE_READ >> len, MYF(0));
    if (len == (uInt)-1) s->z_err = Z_ERRNO;
    s->stream.avail_in += len;
    s->stream.next_in = s->inbuf;
    if (s->stream.avail_in < 2) {
      s->transparent = s->stream.avail_in;
      return;
    }
  }

  /* Peek ahead to check the gzip magic header */
  if (s->stream.next_in[0] == gz_magic[0] &&
      s->stream.next_in[1] == gz_magic[1]) {
    read_header(s, s->stream.next_in);
    s->stream.avail_in -= 2;
    s->stream.next_in += 2;

    /* Check the rest of the gzip header */
    method = get_byte(s);
    flags = get_byte(s);
    if (method != Z_DEFLATED || (flags & RESERVED) != 0) {
      s->z_err = Z_DATA_ERROR;
      return;
    }

    /* Discard time, xflags and OS code: */
    for (len = 0; len < 6; len++) (void)get_byte(s);

    if ((flags & EXTRA_FIELD) != 0) { /* skip the extra field */
      len = (uInt)get_byte(s);
      len += ((uInt)get_byte(s)) << 8;
      /* len is garbage if EOF but the loop below will quit anyway */
      while (len-- != 0 && get_byte(s) != EOF)
        ;
    }
    if ((flags & ORIG_NAME) != 0) { /* skip the original file name */
      while ((c = get_byte(s)) != 0 && c != EOF)
        ;
    }
    if ((flags & COMMENT) != 0) { /* skip the .gz file comment */
      while ((c = get_byte(s)) != 0 && c != EOF)
        ;
    }
    if ((flags & HEAD_CRC) != 0) { /* skip the header crc */
      for (len = 0; len < 2; len++) (void)get_byte(s);
    }
    s->z_err = s->z_eof ? Z_DATA_ERROR : Z_OK;
    if (!s->start) s->start = my_tell(s->file, MYF(0)) - s->stream.avail_in;
  } else if (s->stream.next_in[0] == az_magic[0] &&
             s->stream.next_in[1] == az_magic[1]) {
    unsigned char buffer[AZHEADER_SIZE + AZMETA_BUFFER_SIZE];

    for (len = 0; len < (AZHEADER_SIZE + AZMETA_BUFFER_SIZE); len++)
      buffer[len] = get_byte(s);
    s->z_err = s->z_eof ? Z_DATA_ERROR : Z_OK;
    read_header(s, buffer);
    for (; len < s->start; len++) get_byte(s);
  } else {
    s->z_err = Z_OK;

    return;
  }
}

void read_header(azio_stream *s, unsigned char *buffer) {
  if (buffer[0] == az_magic[0] && buffer[1] == az_magic[1]) {
    s->version = (unsigned int)buffer[AZ_VERSION_POS];
    s->minor_version = (unsigned int)buffer[AZ_MINOR_VERSION_POS];
    s->block_size = 1024 * buffer[AZ_BLOCK_POS];
    s->start = (unsigned long long)uint8korr(buffer + AZ_START_POS);
    s->rows = (unsigned long long)uint8korr(buffer + AZ_ROW_POS);
    s->check_point = (unsigned long long)uint8korr(buffer + AZ_CHECK_POS);
    s->forced_flushes = (unsigned long long)uint8korr(buffer + AZ_FLUSH_POS);
    s->auto_increment =
        (unsigned long long)uint8korr(buffer + AZ_AUTOINCREMENT_POS);
    s->longest_row = (unsigned int)uint4korr(buffer + AZ_LONGEST_POS);
    s->shortest_row = (unsigned int)uint4korr(buffer + AZ_SHORTEST_POS);
    s->frm_start_pos = (unsigned int)uint4korr(buffer + AZ_FRM_POS);
    s->frm_length = (unsigned int)uint4korr(buffer + AZ_FRM_LENGTH_POS);
    s->comment_start_pos = (unsigned int)uint4korr(buffer + AZ_COMMENT_POS);
    s->comment_length = (unsigned int)uint4korr(buffer + AZ_COMMENT_LENGTH_POS);
    s->dirty = (unsigned int)buffer[AZ_DIRTY_POS];
  } else if (buffer[0] == gz_magic[0] && buffer[1] == gz_magic[1]) {
    /*
      Set version number to previous version (1).
    */
    s->version = 1;
    s->auto_increment = 0;
    s->frm_length = 0;
  } else {
    /*
      Unknown version.
      Most probably due to a corrupt archive.
    */
    s->dirty = AZ_STATE_DIRTY;
    s->z_err = Z_VERSION_ERROR;
  }
}

/* ===========================================================================
 * Cleanup then free the given azio_stream. Return a zlib error code.
 Try freeing in the reverse order of allocations.
 */
int destroy(azio_stream *s) {
  int err = Z_OK;

  if (s->stream.state != nullptr) {
    if (s->mode == 'w')
      err = deflateEnd(&(s->stream));
    else if (s->mode == 'r')
      err = inflateEnd(&(s->stream));
  }

  if (s->file > 0 && my_close(s->file, MYF(0))) err = Z_ERRNO;

  s->file = -1;

  if (s->z_err < 0) err = s->z_err;

  return err;
}

/* ===========================================================================
  Reads the given number of uncompressed bytes from the compressed file.
  azread returns the number of bytes actually read (0 for end of file).
*/
size_t ZEXPORT azread(azio_stream *s, voidp buf, size_t len, int *error) {
  Bytef *start = (Bytef *)buf; /* starting point for crc computation */
  Byte *next_out; /* == stream.next_out but not forced far (for MSDOS) */
  *error = 0;

  if (s->mode != 'r') {
    *error = Z_STREAM_ERROR;
    return 0;
  }

  if (s->z_err == Z_DATA_ERROR || s->z_err == Z_ERRNO) {
    *error = s->z_err;
    return 0;
  }

  if (s->z_err == Z_STREAM_END) /* EOF */
  {
    return 0;
  }

  next_out = (Byte *)buf;
  s->stream.next_out = (Bytef *)buf;
  s->stream.avail_out = (uInt)len;

  if (s->stream.avail_out && s->back != EOF) {
    *next_out++ = s->back;
    s->stream.next_out++;
    s->stream.avail_out--;
    s->back = EOF;
    s->out++;
    start++;
    if (s->last) {
      s->z_err = Z_STREAM_END;
      { return 1; }
    }
  }

  while (s->stream.avail_out != 0) {
    if (s->transparent) {
      /* Copy first the lookahead bytes: */
      uInt n = s->stream.avail_in;
      if (n > s->stream.avail_out) n = s->stream.avail_out;
      if (n > 0) {
        memcpy(s->stream.next_out, s->stream.next_in, n);
        next_out += n;
        s->stream.next_out = (Bytef *)next_out;
        s->stream.next_in += n;
        s->stream.avail_out -= n;
        s->stream.avail_in -= n;
      }
      if (s->stream.avail_out > 0) {
        s->stream.avail_out -= (uInt)mysql_file_read(
            s->file, (uchar *)next_out, s->stream.avail_out, MYF(0));
      }
      len -= s->stream.avail_out;
      s->in += len;
      s->out += len;
      if (len == 0) s->z_eof = 1;
      { return len; }
    }
    if (s->stream.avail_in == 0 && !s->z_eof) {
      errno = 0;
      s->stream.avail_in = (uInt)mysql_file_read(s->file, (uchar *)s->inbuf,
                                                 AZ_BUFSIZE_READ, MYF(0));
      if (s->stream.avail_in == 0) {
        s->z_eof = 1;
      }
      s->stream.next_in = (Bytef *)s->inbuf;
    }
    s->in += s->stream.avail_in;
    s->out += s->stream.avail_out;
    s->z_err = inflate(&(s->stream), Z_NO_FLUSH);
    s->in -= s->stream.avail_in;
    s->out -= s->stream.avail_out;

    if (s->z_err == Z_STREAM_END) {
      /* Check CRC and original size */
      s->crc = crc32(s->crc, start, (uInt)(s->stream.next_out - start));
      start = s->stream.next_out;

      if (getLong(s) != s->crc) {
        s->z_err = Z_DATA_ERROR;
      } else {
        (void)getLong(s);
        /* The uncompressed length returned by above getlong() may be
         * different from s->out in case of concatenated .gz files.
         * Check for such files:
         */
        check_header(s);
        if (s->z_err == Z_OK) {
          inflateReset(&(s->stream));
          s->crc = crc32(0L, Z_NULL, 0);
        }
      }
    }
    if (s->z_err != Z_OK || s->z_eof) break;
  }
  s->crc = crc32(s->crc, start, (uInt)(s->stream.next_out - start));

  if (len == s->stream.avail_out &&
      (s->z_err == Z_DATA_ERROR || s->z_err == Z_ERRNO)) {
    *error = s->z_err;

    return 0;
  }

  return (len - s->stream.avail_out);
}

/* ===========================================================================
  Writes the given number of uncompressed bytes into the compressed file.
  azwrite returns the number of bytes actually written (0 in case of error).
*/
unsigned int azwrite(azio_stream *s, const voidp buf, unsigned int len) {
  s->stream.next_in = (Bytef *)buf;
  s->stream.avail_in = len;

  s->rows++;

  while (s->stream.avail_in != 0) {
    if (s->stream.avail_out == 0) {
      s->stream.next_out = s->outbuf;
      if (mysql_file_write(s->file, (uchar *)s->outbuf, AZ_BUFSIZE_WRITE,
                           MYF(0)) != AZ_BUFSIZE_WRITE) {
        s->z_err = Z_ERRNO;
        break;
      }
      s->stream.avail_out = AZ_BUFSIZE_WRITE;
    }
    s->in += s->stream.avail_in;
    s->out += s->stream.avail_out;
    s->z_err = deflate(&(s->stream), Z_NO_FLUSH);
    s->in -= s->stream.avail_in;
    s->out -= s->stream.avail_out;
    if (s->z_err != Z_OK) break;
  }
  s->crc = crc32(s->crc, (const Bytef *)buf, len);

  if (len > s->longest_row) s->longest_row = len;

  if (len < s->shortest_row || !(s->shortest_row)) s->shortest_row = len;

  return (unsigned int)(len - s->stream.avail_in);
}

/* ===========================================================================
  Flushes all pending output into the compressed file. The parameter
  flush is as in the deflate() function.
*/
int do_flush(azio_stream *s, int flush) {
  uInt len;
  int done = 0;
  my_off_t afterwrite_pos;

  if (s == nullptr || s->mode != 'w') return Z_STREAM_ERROR;

  s->stream.avail_in = 0; /* should be zero already anyway */

  for (;;) {
    len = AZ_BUFSIZE_WRITE - s->stream.avail_out;

    if (len != 0) {
      s->check_point = my_tell(s->file, MYF(0));
      if ((uInt)mysql_file_write(s->file, (uchar *)s->outbuf, len, MYF(0)) !=
          len) {
        s->z_err = Z_ERRNO;
        return Z_ERRNO;
      }
      s->stream.next_out = s->outbuf;
      s->stream.avail_out = AZ_BUFSIZE_WRITE;
    }
    if (done) break;
    s->out += s->stream.avail_out;
    s->z_err = deflate(&(s->stream), flush);
    s->out -= s->stream.avail_out;

    /* Ignore the second of two consecutive flushes: */
    if (len == 0 && s->z_err == Z_BUF_ERROR) s->z_err = Z_OK;

    /* deflate has finished flushing only when it hasn't used up
     * all the available space in the output buffer:
     */
    done = (s->stream.avail_out != 0 || s->z_err == Z_STREAM_END);

    if (s->z_err != Z_OK && s->z_err != Z_STREAM_END) break;
  }

  if (flush == Z_FINISH)
    s->dirty = AZ_STATE_CLEAN; /* Mark it clean, we should be good now */
  else
    s->dirty = AZ_STATE_SAVED; /* Mark it clean, we should be good now */

  afterwrite_pos = my_tell(s->file, MYF(0));
  write_header(s);
  my_seek(s->file, afterwrite_pos, SEEK_SET, MYF(0));

  return s->z_err == Z_STREAM_END ? Z_OK : s->z_err;
}

int ZEXPORT azflush(azio_stream *s, int flush) {
  int err;

  if (s->mode == 'r') {
    unsigned char buffer[AZHEADER_SIZE + AZMETA_BUFFER_SIZE];
    my_pread(s->file, (uchar *)buffer, AZHEADER_SIZE + AZMETA_BUFFER_SIZE, 0,
             MYF(0));
    read_header(s, buffer); /* skip the .az header */

    return Z_OK;
  } else {
    s->forced_flushes++;
    err = do_flush(s, flush);

    if (err) return err;
    my_sync(s->file, MYF(0));
    return s->z_err == Z_STREAM_END ? Z_OK : s->z_err;
  }
}

/* ===========================================================================
  Rewinds input file.
*/
int azrewind(azio_stream *s) {
  if (s == nullptr || s->mode != 'r') return -1;

  s->z_err = Z_OK;
  s->z_eof = 0;
  s->back = EOF;
  s->stream.avail_in = 0;
  s->stream.next_in = (Bytef *)s->inbuf;
  s->crc = crc32(0L, Z_NULL, 0);
  if (!s->transparent) (void)inflateReset(&s->stream);
  s->in = 0;
  s->out = 0;
  return my_seek(s->file, (int)s->start, MY_SEEK_SET, MYF(0)) ==
         MY_FILEPOS_ERROR;
}

/* ===========================================================================
  Sets the starting position for the next azread or azwrite on the given
  compressed file. The offset represents a number of bytes in the
  azseek returns the resulting offset location as measured in bytes from
  the beginning of the uncompressed stream, or -1 in case of error.
  SEEK_END is not implemented, returns error.
  In this version of the library, azseek can be extremely slow.
*/
my_off_t azseek(azio_stream *s, my_off_t offset, int whence) {
  if (s == nullptr || whence == SEEK_END || s->z_err == Z_ERRNO ||
      s->z_err == Z_DATA_ERROR) {
    return -1L;
  }

  if (s->mode == 'w') {
    if (whence == SEEK_SET) offset -= s->in;

    /* At this point, offset is the number of zero bytes to write. */
    /* There was a zmemzero here if inbuf was null -Brian */
    while (offset > 0) {
      uInt size = AZ_BUFSIZE_READ;
      if (offset < AZ_BUFSIZE_READ) size = (uInt)offset;

      size = azwrite(s, s->inbuf, size);
      if (size == 0) return -1L;

      offset -= size;
    }
    return s->in;
  }
  /* Rest of function is for reading only */

  /* compute absolute position */
  if (whence == SEEK_CUR) {
    offset += s->out;
  }

  if (s->transparent) {
    /* map to my_seek */
    s->back = EOF;
    s->stream.avail_in = 0;
    s->stream.next_in = (Bytef *)s->inbuf;
    if (my_seek(s->file, offset, MY_SEEK_SET, MYF(0)) == MY_FILEPOS_ERROR)
      return -1L;

    s->in = s->out = offset;
    return offset;
  }

  /* For a negative seek, rewind and use positive seek */
  if (offset >= s->out) {
    offset -= s->out;
  } else if (azrewind(s)) {
    return -1L;
  }
  /* offset is now the number of bytes to skip. */

  if (offset && s->back != EOF) {
    s->back = EOF;
    s->out++;
    offset--;
    if (s->last) s->z_err = Z_STREAM_END;
  }
  while (offset > 0) {
    int error;
    size_t size = AZ_BUFSIZE_WRITE;
    if (offset < AZ_BUFSIZE_WRITE) size = (int)offset;

    size = azread(s, s->outbuf, size, &error);
    if (error < 0) return -1L;
    offset -= size;
  }
  return s->out;
}

/* ===========================================================================
  Returns the starting position for the next azread or azwrite on the
  given compressed file. This position represents a number of bytes in the
  uncompressed data stream.
*/
my_off_t ZEXPORT aztell(azio_stream *file) {
  return azseek(file, 0L, SEEK_CUR);
}

/* ===========================================================================
  Outputs a long in LSB order to the given file
*/
void putLong(File file, uLong x) {
  int n;
  uchar buffer[1];

  for (n = 0; n < 4; n++) {
    buffer[0] = (int)(x & 0xff);
    mysql_file_write(file, buffer, 1, MYF(0));
    x >>= 8;
  }
}

/* ===========================================================================
  Reads a long in LSB order from the given azio_stream. Sets z_err in case
  of error.
*/
uLong getLong(azio_stream *s) {
  uLong x = (uLong)get_byte(s);
  int c;

  x += ((uLong)get_byte(s)) << 8;
  x += ((uLong)get_byte(s)) << 16;
  c = get_byte(s);
  if (c == EOF) s->z_err = Z_DATA_ERROR;
  x += ((uLong)c) << 24;
  return x;
}

/* ===========================================================================
  Flushes all pending output if necessary, closes the compressed file
  and deallocates all the (de)compression state.
*/
int azclose(azio_stream *s) {
  if (s == nullptr) return Z_STREAM_ERROR;

  if (s->file < 1) return Z_OK;

  if (s->mode == 'w') {
    if (do_flush(s, Z_FINISH) != Z_OK) return destroy(s);

    putLong(s->file, s->crc);
    putLong(s->file, (uLong)(s->in & 0xffffffff));
    s->dirty = AZ_STATE_CLEAN;
    s->check_point = my_tell(s->file, MYF(0));
    write_header(s);
  }

  return destroy(s);
}

/*
  Though this was added to support MySQL's FRM file, anything can be
  stored in this location.
*/
int azwrite_frm(azio_stream *s, char *blob, size_t length) {
  if (s->mode == 'r') return 1;

  if (s->rows > 0) return 1;

  s->frm_start_pos = (uint)s->start;
  s->frm_length = length;
  s->start += length;

  if (my_pwrite(s->file, (uchar *)blob, s->frm_length, s->frm_start_pos,
                MYF(MY_NABP)) ||
      write_header(s) ||
      (my_seek(s->file, 0, MY_SEEK_END, MYF(0)) == MY_FILEPOS_ERROR))
    return 1;

  return 0;
}

int azread_frm(azio_stream *s, char *blob) {
  return my_pread(s->file, (uchar *)blob, s->frm_length, s->frm_start_pos,
                  MYF(MY_NABP))
             ? 1
             : 0;
}

/*
  Simple comment field
*/
int azwrite_comment(azio_stream *s, char *blob, size_t length) {
  if (s->mode == 'r') return 1;

  if (s->rows > 0) return 1;

  s->comment_start_pos = (uint)s->start;
  s->comment_length = length;
  s->start += length;

  my_pwrite(s->file, (uchar *)blob, s->comment_length, s->comment_start_pos,
            MYF(0));

  write_header(s);
  my_seek(s->file, 0, MY_SEEK_END, MYF(0));

  return 0;
}

int azread_comment(azio_stream *s, char *blob) {
  my_pread(s->file, (uchar *)blob, s->comment_length, s->comment_start_pos,
           MYF(0));

  return 0;
}
