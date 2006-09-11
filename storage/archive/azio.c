/*
  azio is a modified version of gzio. It  makes use of mysys and removes mallocs.
*/

/* gzio.c -- IO on .gz files
 * Copyright (C) 1995-2005 Jean-loup Gailly.
 * For conditions of distribution and use, see copyright notice in zlib.h
 *
 * Compile this file with -DNO_GZCOMPRESS to avoid the compression code.
 */

/* @(#) $Id$ */

#include "azlib.h"

#include <stdio.h>
#include <string.h>

static int const gz_magic[2] = {0x1f, 0x8b}; /* gzip magic header */

/* gzip flag byte */
#define ASCII_FLAG   0x01 /* bit 0 set: file probably ascii text */
#define HEAD_CRC     0x02 /* bit 1 set: header CRC present */
#define EXTRA_FIELD  0x04 /* bit 2 set: extra field present */
#define ORIG_NAME    0x08 /* bit 3 set: original file name present */
#define COMMENT      0x10 /* bit 4 set: file comment present */
#define RESERVED     0xE0 /* bits 5..7: reserved */

int az_open(azio_stream *s, const char *path, int Flags, File  fd);
int do_flush(azio_stream *file, int flush);
int    get_byte(azio_stream *s);
void   check_header(azio_stream *s);
int    destroy(azio_stream *s);
void putLong(File file, uLong x);
uLong  getLong(azio_stream *s);

/* ===========================================================================
  Opens a gzip (.gz) file for reading or writing. The mode parameter
  is as in fopen ("rb" or "wb"). The file is given either by file descriptor
  or path name (if fd == -1).
  az_open returns NULL if the file could not be opened or if there was
  insufficient memory to allocate the (de)compression state; errno
  can be checked to distinguish the two cases (if errno is zero, the
  zlib error is Z_MEM_ERROR).
*/
int az_open (azio_stream *s, const char *path, int Flags, File fd)
{
  int err;
  int level = Z_DEFAULT_COMPRESSION; /* compression level */
  int strategy = Z_DEFAULT_STRATEGY; /* compression strategy */

  s->stream.zalloc = (alloc_func)0;
  s->stream.zfree = (free_func)0;
  s->stream.opaque = (voidpf)0;
  memset(s->inbuf, 0, Z_BUFSIZE);
  memset(s->outbuf, 0, Z_BUFSIZE);
  s->stream.next_in = s->inbuf;
  s->stream.next_out = s->outbuf;
  s->stream.avail_in = s->stream.avail_out = 0;
  s->z_err = Z_OK;
  s->z_eof = 0;
  s->in = 0;
  s->out = 0;
  s->back = EOF;
  s->crc = crc32(0L, Z_NULL, 0);
  s->transparent = 0;
  s->mode = 'r';

  if (Flags & O_WRONLY || Flags & O_APPEND) 
    s->mode = 'w';

  if (s->mode == 'w') {
#ifdef NO_GZCOMPRESS
    err = Z_STREAM_ERROR;
#else
    err = deflateInit2(&(s->stream), level,
                       Z_DEFLATED, -MAX_WBITS, 8, strategy);
    /* windowBits is passed < 0 to suppress zlib header */

    s->stream.next_out = s->outbuf;
#endif
    if (err != Z_OK)
    {
      destroy(s);
      return Z_NULL;
    }
  } else {
    s->stream.next_in  = s->inbuf;

    err = inflateInit2(&(s->stream), -MAX_WBITS);
    /* windowBits is passed < 0 to tell that there is no zlib header.
     * Note that in this case inflate *requires* an extra "dummy" byte
     * after the compressed stream in order to complete decompression and
     * return Z_STREAM_END. Here the gzip CRC32 ensures that 4 bytes are
     * present after the compressed stream.
   */
    if (err != Z_OK)
    {
      destroy(s);
      return Z_NULL;
    }
  }
  s->stream.avail_out = Z_BUFSIZE;

  errno = 0;
  s->file = fd < 0 ? my_open(path, Flags, MYF(0)) : fd;

  if (s->file < 0 ) 
  {
    destroy(s);
    return Z_NULL;
  }
  if (s->mode == 'w') {
    char buffer[10];
    /* Write a very simple .gz header:
  */
    buffer[0] = gz_magic[0];
    buffer[1] = gz_magic[1];
    buffer[2] = Z_DEFLATED;
    buffer[3] = 0 /*flags*/;
    buffer[4] = 0;
    buffer[5] = 0;
    buffer[6] = 0;
    buffer[7] = 0 /*time*/;
    buffer[8] = 0 /*xflags*/;
    buffer[9] = 0x03;
    s->start = 10L;
    my_write(s->file, buffer, (uint)s->start, MYF(0));
    /* We use 10L instead of ftell(s->file) to because ftell causes an
     * fflush on some systems. This version of the library doesn't use
     * start anyway in write mode, so this initialization is not
     * necessary.
   */
  } else {
    check_header(s); /* skip the .gz header */
    s->start = my_tell(s->file, MYF(0)) - s->stream.avail_in;
  }

  return 1;
}

/* ===========================================================================
  Opens a gzip (.gz) file for reading or writing.
*/
int azopen(azio_stream *s, const char *path, int Flags)
{
  return az_open(s, path, Flags, -1);
}

/* ===========================================================================
  Associate a gzFile with the file descriptor fd. fd is not dup'ed here
  to mimic the behavio(u)r of fdopen.
*/
int azdopen(azio_stream *s, File fd, int Flags)
{
  if (fd < 0) return 0;

  return az_open (s, NULL, Flags, fd);
}

/* ===========================================================================
  Read a byte from a azio_stream; update next_in and avail_in. Return EOF
  for end of file.
  IN assertion: the stream s has been sucessfully opened for reading.
*/
int get_byte(s)
  azio_stream *s;
{
  if (s->z_eof) return EOF;
  if (s->stream.avail_in == 0) 
  {
    errno = 0;
    s->stream.avail_in = my_read(s->file, (byte *)s->inbuf, Z_BUFSIZE, MYF(0));
    if (s->stream.avail_in == 0) 
    {
      s->z_eof = 1;
      /* if (ferror(s->file)) s->z_err = Z_ERRNO; */
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
  IN assertion: the stream s has already been created sucessfully;
  s->stream.avail_in is zero for the first time, but may be non-zero
  for concatenated .gz files.
*/
void check_header(azio_stream *s)
{
  int method; /* method byte */
  int flags;  /* flags byte */
  uInt len;
  int c;

  /* Assure two bytes in the buffer so we can peek ahead -- handle case
    where first byte of header is at the end of the buffer after the last
    gzip segment */
  len = s->stream.avail_in;
  if (len < 2) {
    if (len) s->inbuf[0] = s->stream.next_in[0];
    errno = 0;
    len = (uInt)my_read(s->file, (byte *)s->inbuf + len, Z_BUFSIZE >> len, MYF(0));
    if (len == 0) s->z_err = Z_ERRNO;
    s->stream.avail_in += len;
    s->stream.next_in = s->inbuf;
    if (s->stream.avail_in < 2) {
      s->transparent = s->stream.avail_in;
      return;
    }
  }

  /* Peek ahead to check the gzip magic header */
  if (s->stream.next_in[0] != gz_magic[0] ||
      s->stream.next_in[1] != gz_magic[1]) {
    s->transparent = 1;
    return;
  }
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
    len  =  (uInt)get_byte(s);
    len += ((uInt)get_byte(s))<<8;
    /* len is garbage if EOF but the loop below will quit anyway */
    while (len-- != 0 && get_byte(s) != EOF) ;
  }
  if ((flags & ORIG_NAME) != 0) { /* skip the original file name */
    while ((c = get_byte(s)) != 0 && c != EOF) ;
  }
  if ((flags & COMMENT) != 0) {   /* skip the .gz file comment */
    while ((c = get_byte(s)) != 0 && c != EOF) ;
  }
  if ((flags & HEAD_CRC) != 0) {  /* skip the header crc */
    for (len = 0; len < 2; len++) (void)get_byte(s);
  }
  s->z_err = s->z_eof ? Z_DATA_ERROR : Z_OK;
}

/* ===========================================================================
 * Cleanup then free the given azio_stream. Return a zlib error code.
 Try freeing in the reverse order of allocations.
 */
int destroy (s)
  azio_stream *s;
{
  int err = Z_OK;

  if (s->stream.state != NULL) {
    if (s->mode == 'w') {
#ifdef NO_GZCOMPRESS
      err = Z_STREAM_ERROR;
#else
      err = deflateEnd(&(s->stream));
#endif
    } 
    else if (s->mode == 'r') 
    {
      err = inflateEnd(&(s->stream));
    }
  }
  if (s->file > 0 && my_close(s->file, MYF(0))) 
  {
#ifdef ESPIPE
    if (errno != ESPIPE) /* fclose is broken for pipes in HP/UX */
#endif
      err = Z_ERRNO;
  }
  if (s->z_err < 0) err = s->z_err;

  return err;
}

/* ===========================================================================
  Reads the given number of uncompressed bytes from the compressed file.
  azread returns the number of bytes actually read (0 for end of file).
*/
int ZEXPORT azread ( azio_stream *s, voidp buf, unsigned len)
{
  Bytef *start = (Bytef*)buf; /* starting point for crc computation */
  Byte  *next_out; /* == stream.next_out but not forced far (for MSDOS) */

  if (s->mode != 'r') return Z_STREAM_ERROR;

  if (s->z_err == Z_DATA_ERROR || s->z_err == Z_ERRNO) return -1;
  if (s->z_err == Z_STREAM_END) return 0;  /* EOF */

  next_out = (Byte*)buf;
  s->stream.next_out = (Bytef*)buf;
  s->stream.avail_out = len;

  if (s->stream.avail_out && s->back != EOF) {
    *next_out++ = s->back;
    s->stream.next_out++;
    s->stream.avail_out--;
    s->back = EOF;
    s->out++;
    start++;
    if (s->last) {
      s->z_err = Z_STREAM_END;
      return 1;
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
        s->stream.next_in   += n;
        s->stream.avail_out -= n;
        s->stream.avail_in  -= n;
      }
      if (s->stream.avail_out > 0) 
      {
        s->stream.avail_out -=
          (uInt)my_read(s->file, (byte *)next_out, s->stream.avail_out, MYF(0));
      }
      len -= s->stream.avail_out;
      s->in  += len;
      s->out += len;
      if (len == 0) s->z_eof = 1;
      return (int)len;
    }
    if (s->stream.avail_in == 0 && !s->z_eof) {

      errno = 0;
      s->stream.avail_in = (uInt)my_read(s->file, (byte *)s->inbuf, Z_BUFSIZE, MYF(0));
      if (s->stream.avail_in == 0) 
      {
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
      (s->z_err == Z_DATA_ERROR || s->z_err == Z_ERRNO))
    return -1;
  return (int)(len - s->stream.avail_out);
}


#ifndef NO_GZCOMPRESS
/* ===========================================================================
  Writes the given number of uncompressed bytes into the compressed file.
  azwrite returns the number of bytes actually written (0 in case of error).
*/
int azwrite (azio_stream *s, voidpc buf, unsigned len)
{

  s->stream.next_in = (Bytef*)buf;
  s->stream.avail_in = len;

  while (s->stream.avail_in != 0) 
  {
    if (s->stream.avail_out == 0) 
    {

      s->stream.next_out = s->outbuf;
      if (my_write(s->file, (byte *)s->outbuf, Z_BUFSIZE, MYF(0)) != Z_BUFSIZE) 
      {
        s->z_err = Z_ERRNO;
        break;
      }
      s->stream.avail_out = Z_BUFSIZE;
    }
    s->in += s->stream.avail_in;
    s->out += s->stream.avail_out;
    s->z_err = deflate(&(s->stream), Z_NO_FLUSH);
    s->in -= s->stream.avail_in;
    s->out -= s->stream.avail_out;
    if (s->z_err != Z_OK) break;
  }
  s->crc = crc32(s->crc, (const Bytef *)buf, len);

  return (int)(len - s->stream.avail_in);
}

#endif


/* ===========================================================================
  Flushes all pending output into the compressed file. The parameter
  flush is as in the deflate() function.
*/
int do_flush (s, flush)
  azio_stream *s;
  int flush;
{
  uInt len;
  int done = 0;

  if (s == NULL || s->mode != 'w') return Z_STREAM_ERROR;

  s->stream.avail_in = 0; /* should be zero already anyway */

  for (;;) {
    len = Z_BUFSIZE - s->stream.avail_out;

    if (len != 0) {
      if ((uInt)my_write(s->file, (byte *)s->outbuf, len, MYF(0)) != len) 
      {
        s->z_err = Z_ERRNO;
        return Z_ERRNO;
      }
      s->stream.next_out = s->outbuf;
      s->stream.avail_out = Z_BUFSIZE;
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
  return  s->z_err == Z_STREAM_END ? Z_OK : s->z_err;
}

int ZEXPORT azflush (s, flush)
  azio_stream *s;
  int flush;
{
  int err = do_flush (s, flush);

  if (err) return err;
  my_sync(s->file, MYF(0));
  return  s->z_err == Z_STREAM_END ? Z_OK : s->z_err;
}

/* ===========================================================================
  Rewinds input file.
*/
int azrewind (s)
  azio_stream *s;
{
  if (s == NULL || s->mode != 'r') return -1;

  s->z_err = Z_OK;
  s->z_eof = 0;
  s->back = EOF;
  s->stream.avail_in = 0;
  s->stream.next_in = (Bytef *)s->inbuf;
  s->crc = crc32(0L, Z_NULL, 0);
  if (!s->transparent) (void)inflateReset(&s->stream);
  s->in = 0;
  s->out = 0;
  return my_seek(s->file, (int)s->start, MY_SEEK_SET, MYF(0));
}

/* ===========================================================================
  Sets the starting position for the next azread or azwrite on the given
  compressed file. The offset represents a number of bytes in the
  azseek returns the resulting offset location as measured in bytes from
  the beginning of the uncompressed stream, or -1 in case of error.
  SEEK_END is not implemented, returns error.
  In this version of the library, azseek can be extremely slow.
*/
my_off_t azseek (s, offset, whence)
  azio_stream *s;
  my_off_t offset;
  int whence;
{

  if (s == NULL || whence == SEEK_END ||
      s->z_err == Z_ERRNO || s->z_err == Z_DATA_ERROR) {
    return -1L;
  }

  if (s->mode == 'w') {
#ifdef NO_GZCOMPRESS
    return -1L;
#else
    if (whence == SEEK_SET) {
      offset -= s->in;
    }

    /* At this point, offset is the number of zero bytes to write. */
    /* There was a zmemzero here if inbuf was null -Brian */
    while (offset > 0)  {
      uInt size = Z_BUFSIZE;
      if (offset < Z_BUFSIZE) size = (uInt)offset;

      size = azwrite(s, s->inbuf, size);
      if (size == 0) return -1L;

      offset -= size;
    }
    return s->in;
#endif
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
    if (my_seek(s->file, offset, MY_SEEK_SET, MYF(0)) == MY_FILEPOS_ERROR) return -1L;

    s->in = s->out = offset;
    return offset;
  }

  /* For a negative seek, rewind and use positive seek */
  if (offset >= s->out) {
    offset -= s->out;
  } else if (azrewind(s) < 0) {
    return -1L;
  }
  /* offset is now the number of bytes to skip. */

  if (offset && s->back != EOF) {
    s->back = EOF;
    s->out++;
    offset--;
    if (s->last) s->z_err = Z_STREAM_END;
  }
  while (offset > 0)  {
    int size = Z_BUFSIZE;
    if (offset < Z_BUFSIZE) size = (int)offset;

    size = azread(s, s->outbuf, (uInt)size);
    if (size <= 0) return -1L;
    offset -= size;
  }
  return s->out;
}

/* ===========================================================================
  Returns the starting position for the next azread or azwrite on the
  given compressed file. This position represents a number of bytes in the
  uncompressed data stream.
*/
my_off_t ZEXPORT aztell (file)
  azio_stream *file;
{
  return azseek(file, 0L, SEEK_CUR);
}


/* ===========================================================================
  Outputs a long in LSB order to the given file
*/
void putLong (File file, uLong x)
{
  int n;
  byte buffer[1];

  for (n = 0; n < 4; n++) 
  {
    buffer[0]= (int)(x & 0xff);
    my_write(file, buffer, 1, MYF(0));
    x >>= 8;
  }
}

/* ===========================================================================
  Reads a long in LSB order from the given azio_stream. Sets z_err in case
  of error.
*/
uLong getLong (azio_stream *s)
{
  uLong x = (uLong)get_byte(s);
  int c;

  x += ((uLong)get_byte(s))<<8;
  x += ((uLong)get_byte(s))<<16;
  c = get_byte(s);
  if (c == EOF) s->z_err = Z_DATA_ERROR;
  x += ((uLong)c)<<24;
  return x;
}

/* ===========================================================================
  Flushes all pending output if necessary, closes the compressed file
  and deallocates all the (de)compression state.
*/
int azclose (azio_stream *s)
{

  if (s == NULL) return Z_STREAM_ERROR;

  if (s->mode == 'w') {
#ifdef NO_GZCOMPRESS
    return Z_STREAM_ERROR;
#else
    if (do_flush (s, Z_FINISH) != Z_OK)
      return destroy(s);

    putLong(s->file, s->crc);
    putLong(s->file, (uLong)(s->in & 0xffffffff));
#endif
  }
  return destroy(s);
}
