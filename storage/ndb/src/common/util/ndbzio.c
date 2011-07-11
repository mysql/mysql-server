/*
   Copyright (c) 2007, 2011, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

/*
  ndbzio is a modified version of azio with a cleaner interface.

    - Magnus Blåudd
*/

/*
  azio is a modified version of gzio. It  makes use of mysys and removes mallocs.
    -Brian Aker
*/

/*
 * This version has been hacked around by Stewart Smith to get the following:
 * - Direct IO (512 byte aligned IO, IO in multibles of 512 bytes)
 * - No memory dynamic memory allocation (all done on startup)
 * - A kinda broken flush (see point 1: O_DIRECT) :)
 */

/* gzio.c -- IO on .gz files
 * Copyright (C) 1995-2005 Jean-loup Gailly.
 * For conditions of distribution and use, see copyright notice in zlib.h
 *
 */

#include <ndb_global.h>

/**
 * This is a casual hack to do static memory allocation
 * (needed by NDB)
 */
#include "../../../../zlib/zutil.h"
#include "../../../../zlib/zconf.h"
#include "../../../../zlib/inftrees.h"
#include "../../../../zlib/inflate.h"
#include "../../../../zlib/deflate.h"

#include <util/ndbzio.h>

#include <my_sys.h>

#ifdef HAVE_VALGRIND
#include <valgrind/memcheck.h>
#else
#define VALGRIND_MAKE_MEM_DEFINED(a,b) do {} while(0);
#define VALGRIND_MAKE_MEM_NOACCESS(a,b) do {} while(0);
#endif

/* Start of MySQL Specific Information */


#define AZHEADER_SIZE 29
#define AZMETA_BUFFER_SIZE 512-AZHEADER_SIZE

#define AZ_MAGIC_POS 0
#define AZ_VERSION_POS 1
#define AZ_MINOR_VERSION_POS 2
#define AZ_BLOCK_POS 3
#define AZ_STRATEGY_POS 4
#define AZ_FRM_POS 5
#define AZ_FRM_LENGTH_POS 9
#define AZ_META_POS 13
#define AZ_META_LENGTH_POS 17
#define AZ_START_POS 21
#define AZ_ROW_POS 29
#define AZ_FLUSH_POS 37
#define AZ_CHECK_POS 45
#define AZ_AUTOINCREMENT_POS 53
#define AZ_LONGEST_POS 61
#define AZ_SHORTEST_POS 65
#define AZ_COMMENT_POS 69
#define AZ_COMMENT_LENGTH_POS 73
#define AZ_DIRTY_POS 77


/*
  Flags for state
*/
#define AZ_STATE_CLEAN 0
#define AZ_STATE_DIRTY 1
#define AZ_STATE_SAVED 2
#define AZ_STATE_CRASHED 3


static int const gz_magic[2] = {0x1f, 0x8b}; /* gzip magic header */
static int const az_magic[3] = {0xfe, 0x03, 0x01}; /* az magic header */

/* gzip flag uchar */
#define ASCII_FLAG   0x01 /* bit 0 set: file probably ascii text */
#define HEAD_CRC     0x02 /* bit 1 set: header CRC present */
#define EXTRA_FIELD  0x04 /* bit 2 set: extra field present */
#define ORIG_NAME    0x08 /* bit 3 set: original file name present */
#define COMMENT      0x10 /* bit 4 set: file comment present */
#define RESERVED     0xE0 /* bits 5..7: reserved */

#define AZ_MEMLEVEL 8

static int ndbz_open(ndbzio_stream *s, const char *path, int Flags, File  fd);
static int do_flush(ndbzio_stream *file, int flush);
static int get_byte(ndbzio_stream *s);
static unsigned char* get_block(ndbzio_stream *s, int blksz);
static int check_header(ndbzio_stream *s);
static int write_header(ndbzio_stream *s);
static int destroy(ndbzio_stream *s);
static void putLong(ndbzio_stream *s, uLong x);
static uLong getLong(ndbzio_stream *s);
static void read_header(ndbzio_stream *s, unsigned char *buffer);

size_t ndbz_inflate_mem_size()
{
  return sizeof(struct inflate_state)            /* state */
    + ((1U << MAX_WBITS)*sizeof(unsigned char)); /* window */
}

size_t ndbz_deflate_mem_size()
{
  return sizeof(deflate_state)
    + ((1U << MAX_WBITS)*(2*sizeof(Byte)))      /* window = wsize,2*|Byte| */
    + ((1U << MAX_WBITS)*sizeof(Pos))           /* prev   = wsize,|Pos| */
    + ((1U << (AZ_MEMLEVEL+7))*sizeof(Pos))     /* head   = hashsize,|Pos| */
    + ((1U << (AZ_MEMLEVEL+6))*(sizeof(ush)+2));/* overlay= lit_bufsize,|ush|+2
						*/
}

voidpf ndbz_alloc(voidpf opaque, uInt items, uInt size)
{
  void * retval;
  struct ndbz_alloc_rec *r = (struct ndbz_alloc_rec*)opaque;

  if((items * size) > r->mfree || r->mfree==0)
    abort();

  assert(r->mfree <= r->size);

  retval= (r->mem + r->size) - r->mfree;
  memset(retval, 0, items*size);
  VALGRIND_MAKE_MEM_DEFINED(retval,items*size);
  r->mfree -= items*size;

  return retval;
}
void ndbz_free(voidpf opaque, voidpf address)
{
  struct ndbz_alloc_rec *r;
  (void)address;
  /* Oh how we hack. */
  r = (struct ndbz_alloc_rec*)opaque;
  r->mfree= r->size;
  if(r->mfree==r->size)
    VALGRIND_MAKE_MEM_NOACCESS(r->mem,r->size);
}

#ifdef _WIN32
#ifndef ENOTSUP
/* If Windows doesn't define ENOTSUP, define it same as Solaris */
#define ENOTSUP 48
#endif
#endif

#ifndef HAVE_POSIX_MEMALIGN
static inline int posix_memalign(void **memptr, size_t alignment, size_t size)
{
#ifdef HAVE_MEMALIGN
  /* Solaris 10 has memalign but not posix_memalign */
  *memptr = memalign(alignment,size);
  if(!*memptr)
    return ENOMEM;
  return 0;
#else
  /* But Darwin 7.9.0 doesn't have posix_memalign OR memalign */
  (void)memptr;
  (void)alignment;
  (void)size;
  return ENOTSUP; /* POSIX says we can return EINVAL or ENOMEM...
                     but we cheat here and do ENOTSUP so that code
                     elsewhere can work out if it can fall back to
                     plain malloc() or not as we cannot reasonably do aligned
                     memory allocation and free without leaking memory
                  */
#endif
}
#endif

#define AZ_BUFSIZE_READ 32768
#define AZ_BUFSIZE_WRITE 16384

size_t ndbz_bufsize_read(void)
{
  return AZ_BUFSIZE_READ;
}

size_t ndbz_bufsize_write(void)
{
  return AZ_BUFSIZE_WRITE;
}

/* ===========================================================================
  Opens a gzip (.gz) file for reading or writing. The mode parameter
  is as in fopen ("rb" or "wb"). The file is given either by file descriptor
  or path name (if fd == -1).
  ndbz_open returns NULL if the file could not be opened or if there was
  insufficient memory to allocate the (de)compression state; errno
  can be checked to distinguish the two cases (if errno is zero, the
  zlib error is Z_MEM_ERROR).

  IO errors in my_errno.

  NOTE: If called without a fd, my_open *WILL* malloc()
*/
int ndbz_open (ndbzio_stream *s, const char *path, int Flags, File fd)
{
  int err;
  int level = Z_DEFAULT_COMPRESSION; /* compression level */
  int strategy = Z_DEFAULT_STRATEGY; /* compression strategy */

  if(s->stream.opaque)
  {
    s->stream.zalloc = (alloc_func)ndbz_alloc;
    s->stream.zfree = (free_func)ndbz_free;
  }
  s->bufalloced = 0;
  if(!s->inbuf)
  {
    err= posix_memalign((void**)&(s->inbuf),512,AZ_BUFSIZE_READ);
    if(err==ENOTSUP)
      s->inbuf= malloc(AZ_BUFSIZE_READ);
    else if(err)
      return -err;
    err= posix_memalign((void**)&(s->outbuf),512,AZ_BUFSIZE_WRITE);
    if(err==ENOTSUP)
      s->outbuf= malloc(AZ_BUFSIZE_WRITE);
    else if(err)
      return -err;
    s->bufalloced = 1;
  }
  memset(s->inbuf, 0, AZ_BUFSIZE_READ);
  memset(s->outbuf, 0, AZ_BUFSIZE_WRITE);
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
  s->version = (unsigned char)az_magic[1]; /* this needs to be a define to version */
  s->version = (unsigned char)az_magic[2]; /* minor version */

  /*
    We do our own version of append by nature. 
    We must always have write access to take card of the header.
  */
  assert(Flags | O_APPEND);
  assert(Flags | O_WRONLY);

  if (Flags & O_RDWR || Flags & O_WRONLY)
    s->mode = 'w';

  if (s->mode == 'w') 
  {
    err = deflateInit2(&(s->stream), level,
                       Z_DEFLATED, -MAX_WBITS, AZ_MEMLEVEL, strategy);
    /* windowBits is passed < 0 to suppress zlib header */

    s->stream.next_out = s->outbuf;
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
  s->stream.avail_out = AZ_BUFSIZE_WRITE;

  errno = 0;
  s->file = fd < 0 ? my_open(path, Flags, MYF(0)) : fd;

  if (s->file < 0 ) 
  {
    destroy(s);
    return Z_NULL;
  }

  if (Flags & O_CREAT || Flags & O_TRUNC) 
  {
    s->rows= 0;
    s->forced_flushes= 0;
    s->shortest_row= 0;
    s->longest_row= 0;
    s->auto_increment= 0;
    s->check_point= 0;
    s->comment_start_pos= 0;
    s->comment_length= 0;
    s->frm_start_pos= 0;
    s->frm_length= 0;
    s->dirty= 1; /* We create the file dirty */
    s->start = AZHEADER_SIZE + AZMETA_BUFFER_SIZE;
    if(write_header(s))
      return Z_NULL;
    if(my_seek(s->file, 0, MY_SEEK_END, MYF(0)) == MY_FILEPOS_ERROR)
      return Z_NULL;
  }
  else if (s->mode == 'w')
  {
    if(my_pread(s->file, s->inbuf, AZHEADER_SIZE + AZMETA_BUFFER_SIZE, 0,
                MYF(0)) < AZHEADER_SIZE + AZMETA_BUFFER_SIZE)
      return Z_NULL;
    read_header(s, s->inbuf); /* skip the .az header */
    if(my_seek(s->file, 0, MY_SEEK_END, MYF(0)) == MY_FILEPOS_ERROR)
      return Z_NULL;
  }
  else
  {
    if(check_header(s)!=0) /* skip the .az header */
      return Z_NULL;
  }

  return 1;
}


int write_header(ndbzio_stream *s)
{
  char *buffer= (char*)s->outbuf;
  char *ptr= buffer;

  s->block_size= AZ_BUFSIZE_WRITE;
  s->version = (unsigned char)az_magic[1];
  s->minor_version = (unsigned char)az_magic[2];


  /* Write a very simple .az header: */
  memset(buffer, 0, AZHEADER_SIZE + AZMETA_BUFFER_SIZE);
  *(ptr + AZ_MAGIC_POS)= az_magic[0];
  *(ptr + AZ_VERSION_POS)= (unsigned char)s->version;
  *(ptr + AZ_MINOR_VERSION_POS)= (unsigned char)s->minor_version;
  *(ptr + AZ_BLOCK_POS)= (unsigned char)(s->block_size/1024); /* Reserved for block size */
  *(ptr + AZ_STRATEGY_POS)= (unsigned char)Z_DEFAULT_STRATEGY; /* Compression Type */

  int4store(ptr + AZ_FRM_POS, s->frm_start_pos); /* FRM Block */
  int4store(ptr + AZ_FRM_LENGTH_POS, s->frm_length); /* FRM Block */
  int4store(ptr + AZ_COMMENT_POS, s->comment_start_pos); /* COMMENT Block */
  int4store(ptr + AZ_COMMENT_LENGTH_POS, s->comment_length); /* COMMENT Block */
  int4store(ptr + AZ_META_POS, 0); /* Meta Block */
  int4store(ptr + AZ_META_LENGTH_POS, 0); /* Meta Block */
  int8store(ptr + AZ_START_POS, (unsigned long long)s->start); /* Start of Data Block Index Block */
  int8store(ptr + AZ_ROW_POS, (unsigned long long)s->rows); /* Start of Data Block Index Block */
  int8store(ptr + AZ_FLUSH_POS, (unsigned long long)s->forced_flushes); /* Start of Data Block Index Block */
  int8store(ptr + AZ_CHECK_POS, (unsigned long long)s->check_point); /* Start of Data Block Index Block */
  int8store(ptr + AZ_AUTOINCREMENT_POS, (unsigned long long)s->auto_increment); /* Start of Data Block Index Block */
  int4store(ptr+ AZ_LONGEST_POS , s->longest_row); /* Longest row */
  int4store(ptr+ AZ_SHORTEST_POS, s->shortest_row); /* Shorest row */
  int4store(ptr+ AZ_FRM_POS, 
            AZHEADER_SIZE + AZMETA_BUFFER_SIZE); /* FRM position */
  *(ptr + AZ_DIRTY_POS)= (unsigned char)s->dirty; /* Start of Data Block Index Block */

  /* Always begin at the begining, and end there as well */
  if(my_pwrite(s->file, (uchar*) buffer, AZHEADER_SIZE + AZMETA_BUFFER_SIZE, 0,
               MYF(0)) == (size_t)-1)
    return -1;

  return 0;
}

/* ===========================================================================
  Opens a gzip (.gz) file for reading or writing.
*/
int ndbzopen(ndbzio_stream *s, const char *path, int Flags)
{
  return ndbz_open(s, path, Flags, -1);
}

/* ===========================================================================
  Associate a gzFile with the file descriptor fd. fd is not dup'ed here
  to mimic the behavio(u)r of fdopen.
*/
int ndbzdopen(ndbzio_stream *s, File fd, int Flags)
{
  if (fd < 0) return 0;

  return ndbz_open (s, NULL, Flags, fd);
}

/*
  Read from ndbzio_stream into buffer.
  Reads up to AZ_BUFSIZE_READ bytes.

  Number of Bytes read is in: s->stream.avail_in

  return 0 on success
 */
int read_buffer(ndbzio_stream *s)
{
  if (s->z_eof) return EOF;
  my_errno= 0;
  if (s->stream.avail_in == 0)
  {
    s->stream.avail_in = my_read(s->file, (uchar *)s->inbuf, AZ_BUFSIZE_READ, MYF(0));
    if(s->stream.avail_in > 0)
      my_errno= 0;
    if (s->stream.avail_in == 0)
    {
      s->z_eof = 1;
    }
    s->stream.next_in = s->inbuf;
  }
  return my_errno;
}

/*
  Read a byte from a ndbzio_stream; update next_in and avail_in.

  returns EOF on error;
*/
int get_byte(ndbzio_stream *s)
{
  if (s->stream.avail_in == 0)
    if(read_buffer(s))
      return EOF;
  s->stream.avail_in--;
  return *(s->stream.next_in)++;
}

/*
 * Gets block of size blksz
 * *MUST* be < buffer size
 * *MUST* be aligned to IO size (i.e. not be only partially in buffer)
 */
unsigned char* get_block(ndbzio_stream *s,int blksz)
{
  unsigned char *r= s->stream.next_in;
  if (s->stream.avail_in == 0)
    if(read_buffer(s))
      return NULL;
  s->stream.avail_in-=blksz;
  s->stream.next_in+=blksz;

  return r;
}

/* ===========================================================================
  Check the gzip header of a ndbzio_stream opened for reading. Set the stream
  mode to transparent if the gzip magic header is not present; set s->err
  to Z_DATA_ERROR if the magic header is present but the rest of the header
  is incorrect.
  IN assertion: the stream s has already been created sucessfully;
  s->stream.avail_in is zero for the first time, but may be non-zero
  for concatenated .gz files.
*/
int check_header(ndbzio_stream *s)
{
  int method; /* method uchar */
  int flags;  /* flags uchar */
  uInt len;
  int c;

  if(s->stream.avail_in==0)
    if((c= read_buffer(s)))
      return 0;

  /* Peek ahead to check the gzip magic header */
  if ( s->stream.next_in[0] == gz_magic[0]  && s->stream.next_in[1] == gz_magic[1])
  {
    abort(); /* FIXME: stewart broke it, massively */
    s->stream.avail_in -= 2;
    s->stream.next_in += 2;
    s->version= (unsigned char)2;

    /* Check the rest of the gzip header */
    method = get_byte(s);
    flags = get_byte(s);
    if (method != Z_DEFLATED || (flags & RESERVED) != 0) {
      s->z_err = Z_DATA_ERROR;
      return 0;
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
    s->start = my_tell(s->file, MYF(0));
    if(s->start == (my_off_t)-1)
      return my_errno;
    s->start-= s->stream.avail_in;
  }
  else if (    s->stream.next_in[0] == az_magic[0]
            && s->stream.next_in[1] == az_magic[1])
  {
    unsigned char *header_block;
    if(s->stream.avail_in < AZHEADER_SIZE + AZMETA_BUFFER_SIZE)
    {
      s->z_err = Z_DATA_ERROR;
      return s->z_err;
    }
    header_block = get_block(s,512);
    if(!header_block)
      return my_errno;
    read_header(s, header_block);
  }
  else
  {
    s->transparent = 1;
    ndbzseek(s,0,SEEK_SET);
    s->z_err = Z_OK;

    return 0;
  }

  return 0;
}

void read_header(ndbzio_stream *s, unsigned char *buffer)
{
  if (buffer[0] == az_magic[0]  && buffer[1] == az_magic[1])
  {
    s->version= (unsigned int)buffer[AZ_VERSION_POS];
    s->minor_version= (unsigned int)buffer[AZ_MINOR_VERSION_POS];
    s->block_size= 1024 * buffer[AZ_BLOCK_POS];
    s->start= (unsigned long long)uint8korr(buffer + AZ_START_POS);
    s->rows= (unsigned long long)uint8korr(buffer + AZ_ROW_POS);
    s->check_point= (unsigned long long)uint8korr(buffer + AZ_CHECK_POS);
    s->forced_flushes= (unsigned long long)uint8korr(buffer + AZ_FLUSH_POS);
    s->auto_increment= (unsigned long long)uint8korr(buffer + AZ_AUTOINCREMENT_POS);
    s->longest_row= (unsigned int)uint4korr(buffer + AZ_LONGEST_POS);
    s->shortest_row= (unsigned int)uint4korr(buffer + AZ_SHORTEST_POS);
    s->frm_start_pos= (unsigned int)uint4korr(buffer + AZ_FRM_POS);
    s->frm_length= (unsigned int)uint4korr(buffer + AZ_FRM_LENGTH_POS);
    s->comment_start_pos= (unsigned int)uint4korr(buffer + AZ_COMMENT_POS);
    s->comment_length= (unsigned int)uint4korr(buffer + AZ_COMMENT_LENGTH_POS);
    s->dirty= (unsigned int)buffer[AZ_DIRTY_POS];
  }
  else
  {
    assert(buffer[0] == az_magic[0]  && buffer[1] == az_magic[1]);
    return;
  }
}

/* ===========================================================================
 * Cleanup then free the given ndbzio_stream. Return a zlib error code.
 Try freeing in the reverse order of allocations.
 */
int destroy (ndbzio_stream *s)
{
  int err = Z_OK;

  if (s->stream.state != NULL) 
  {
    if (s->mode == 'w') 
      err = deflateEnd(&(s->stream));
    else if (s->mode == 'r') 
      err = inflateEnd(&(s->stream));
  }

  if (s->file > 0 && my_close(s->file, MYF(0))) 
      err = Z_ERRNO;

  s->file= -1;

  if (s->z_err < 0) err = s->z_err;

  if(s->bufalloced)
  {
    free(s->inbuf);
    free(s->outbuf);
  }

  return err;
}

/* ===========================================================================
  Reads the given number of uncompressed bytes from the compressed file.
  ndbzread returns the number of bytes actually read (0 for end of file).
*/
unsigned int ZEXPORT ndbzread ( ndbzio_stream *s, voidp buf, unsigned int len, int *error)
{
  Bytef *start = (Bytef*)buf; /* starting point for crc computation */
  Byte  *next_out; /* == stream.next_out but not forced far (for MSDOS) */
  *error= 0;

  if (s->mode != 'r')
  {
    *error= Z_STREAM_ERROR;
    return 0;
  }

  if (s->z_err == Z_DATA_ERROR || s->z_err == Z_ERRNO)
  {
    *error= s->z_err;
    return 0;
  }

  if (s->z_err == Z_STREAM_END)  /* EOF */
  {
    return 0;
  }

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
      {
        return 1;
      }
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
        size_t bytes_read;
        bytes_read= my_read(s->file, (uchar *)next_out, s->stream.avail_out,
                            MYF(0));
        if(bytes_read>0)
          s->stream.avail_out -= bytes_read;
        if (bytes_read == 0)
        {
          s->z_eof = 1;
          return 0;
        }
      }
      len -= s->stream.avail_out;
      s->in  += len;
      s->out += len;
      return len;
    }


    if (s->stream.avail_in == 0 && !s->z_eof) {
      read_buffer(s);
      if (s->stream.avail_in == 0)
      {
        s->z_eof = 1;
      }
    }
    s->in += s->stream.avail_in;
    s->out += s->stream.avail_out;
    s->z_err = inflate(&(s->stream), Z_NO_FLUSH);
    s->in -= s->stream.avail_in;
    s->out -= s->stream.avail_out;

    if (s->z_err == Z_STREAM_END) {
      /* Check CRC and original size */
      uInt gotcrc;
      s->crc = crc32(s->crc, start, (uInt)(s->stream.next_out - start));
      start = s->stream.next_out;

      gotcrc = getLong(s);
      if (gotcrc != s->crc) {
        s->z_err = Z_DATA_ERROR;
      } else {
        (void)getLong(s);
        /* The uncompressed length returned by above getlong() may be
         * different from s->out in case of concatenated .gz files.
         * Check for such files:
         *//*
        check_header(s);
        if (s->z_err == Z_OK) 
        {
          inflateReset(&(s->stream));
          s->crc = crc32(0L, Z_NULL, 0);
          }*/
      }
    }
    if (s->z_err != Z_OK || s->z_eof) break;
  }
  s->crc = crc32(s->crc, start, (uInt)(s->stream.next_out - start));

  if (len == s->stream.avail_out &&
      (s->z_err == Z_DATA_ERROR || s->z_err == Z_ERRNO))
  {
    *error= s->z_err;
    return 0;
  }

  return (len - s->stream.avail_out);
}

/*
  Write last remaining 512 byte block of write buffer, 0 padded.
 */
int flush_write_buffer(ndbzio_stream *s)
{
  uInt real_len = AZ_BUFSIZE_WRITE - s->stream.avail_out;
  uInt len = ((real_len+0x1FF)>>9)<<9;

  memset(s->outbuf+real_len, 0, s->stream.avail_out);

  s->check_point= my_tell(s->file, MYF(0));

  my_write(s->file,(uchar*)s->outbuf,len,MYF(0));

  s->dirty= AZ_STATE_CLEAN;

  return 0;
}

int write_buffer(ndbzio_stream *s)
{
  if (s->stream.avail_out == 0)
  {
    s->stream.next_out = s->outbuf;
    if (my_write(s->file, (uchar *)s->outbuf, AZ_BUFSIZE_WRITE,
                 MYF(0)) != AZ_BUFSIZE_WRITE)
    {
      s->z_err = Z_ERRNO;
      return my_errno;
    }
    s->stream.avail_out = AZ_BUFSIZE_WRITE;
  }
  return 0;
}

/* ===========================================================================
  Writes the given number of uncompressed bytes into the compressed file.
  ndbzwrite returns the number of bytes actually written (0 in case of error).
*/
unsigned int ndbzwrite (ndbzio_stream *s, const void*  buf, unsigned int len)
{
  unsigned int i;
  s->stream.next_in = (Bytef*)buf;
  s->stream.avail_in = len;

  s->rows++;

  for(i=0;i<len;i++)
    memcmp(buf,s,1);

  while (s->stream.avail_in != 0)
  {
    if(write_buffer(s))
      return 0;
    s->in += s->stream.avail_in;
    s->out += s->stream.avail_out;
    s->z_err = deflate(&(s->stream), Z_NO_FLUSH);
    s->in -= s->stream.avail_in;
    s->out -= s->stream.avail_out;
    if (s->z_err != Z_OK) break;
  }
  s->crc = crc32(s->crc, (const Bytef *)buf, len);

  if (len > s->longest_row)
    s->longest_row= len;

  if (len < s->shortest_row || !(s->shortest_row))
    s->shortest_row= len;

  return (unsigned int)(len - s->stream.avail_in);
}


/* ===========================================================================
  Flushes all pending output into the compressed file. The parameter
  flush is as in the deflate() function.
*/
int do_flush (ndbzio_stream *s, int flush)
{
  uInt len;
  int done = 0;

  if (s == NULL || s->mode != 'w') return Z_STREAM_ERROR;

  s->stream.avail_in = 0; /* should be zero already anyway */

  for (;;)
  {
    len = AZ_BUFSIZE_WRITE - s->stream.avail_out;
    write_buffer(s);

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
    s->dirty= AZ_STATE_CLEAN; /* Mark it clean, we should be good now */
  else
    s->dirty= AZ_STATE_SAVED; /* Mark it clean, we should be good now */

  return  s->z_err == Z_STREAM_END ? Z_OK : s->z_err;
}

int ZEXPORT ndbzflush (ndbzio_stream *s, int flush)
{
  int err;

  if (s->mode == 'r')
  {
    unsigned char buffer[AZHEADER_SIZE + AZMETA_BUFFER_SIZE];
    if(my_pread(s->file, (uchar*) buffer, AZHEADER_SIZE + AZMETA_BUFFER_SIZE, 0,
                MYF(0)) == (size_t)-1)
      return Z_ERRNO;
    read_header(s, buffer); /* skip the .az header */

    return Z_OK;
  }
  else
  {
    s->forced_flushes++;
    err= do_flush(s, flush);

    if (err) return err;
    if(my_sync(s->file, MYF(0)) == -1)
      return Z_ERRNO;
    return  s->z_err == Z_STREAM_END ? Z_OK : s->z_err;
  }
}

/* ===========================================================================
  Rewinds input file.
*/
int ndbzrewind (ndbzio_stream *s)
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
  return my_seek(s->file, (int)s->start, MY_SEEK_SET, MYF(0)) == MY_FILEPOS_ERROR;
}

/* ===========================================================================
  Sets the starting position for the next ndbzread or ndbzwrite on the given
  compressed file. The offset represents a number of bytes in the
  ndbzseek returns the resulting offset location as measured in bytes from
  the beginning of the uncompressed stream, or -1 in case of error.
  SEEK_END is not implemented, returns error.
  In this version of the library, ndbzseek can be extremely slow.
*/
my_off_t ndbzseek (ndbzio_stream *s, my_off_t offset, int whence)
{

  if (s == NULL || whence == SEEK_END ||
      s->z_err == Z_ERRNO || s->z_err == Z_DATA_ERROR) {
    return -1L;
  }

  if (s->mode == 'w') 
  {
    if (whence == SEEK_SET) 
      offset -= s->in;

    /* At this point, offset is the number of zero bytes to write. */
    /* There was a zmemzero here if inbuf was null -Brian */
    while (offset > 0)  
    {
      uInt size = AZ_BUFSIZE_WRITE;
      if (offset < AZ_BUFSIZE_WRITE) size = (uInt)offset;

      size = ndbzwrite(s, s->inbuf, size);
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
    if (my_seek(s->file, offset, MY_SEEK_SET, MYF(0)) == MY_FILEPOS_ERROR) return -1L;

    s->in = s->out = offset;
    return offset;
  }

  /* For a negative seek, rewind and use positive seek */
  if (offset >= s->out) {
    offset -= s->out;
  } else if (ndbzrewind(s)) {
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
    int error;
    unsigned int size = AZ_BUFSIZE_READ;
    if (offset < AZ_BUFSIZE_READ) size = (int)offset;

    size = ndbzread(s, s->outbuf, size, &error);
    if (error <= 0) return -1L;
    offset -= size;
  }
  return s->out;
}

/* ===========================================================================
  Returns the starting position for the next ndbzread or ndbzwrite on the
  given compressed file. This position represents a number of bytes in the
  uncompressed data stream.
*/
my_off_t ZEXPORT ndbztell (ndbzio_stream *file)
{
  return ndbzseek(file, 0L, SEEK_CUR);
}


/* ===========================================================================
  Outputs a long in LSB order to the given ndbzio_stream
*/
void putLong (ndbzio_stream *s, uLong x)
{
  int n;

  for (n = 0; n < 4; n++)
  {
    s->stream.avail_out--;
    *(s->stream.next_out) = (Bytef)(x & 0xff);
    s->stream.next_out++;
    write_buffer(s);
    x >>= 8;
  }
}

/* ===========================================================================
  Reads a long in LSB order from the given ndbzio_stream. Sets z_err in case
  of error.
*/
uLong getLong (ndbzio_stream *s)
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
int ndbzclose (ndbzio_stream *s)
{

  if (s == NULL) return Z_STREAM_ERROR;
  
  if (s->file < 1) return Z_OK;

  if (s->mode == 'w') 
  {
    int r= do_flush(s, Z_FINISH);
    if(r!= Z_OK)
    {
      return destroy(s);
    }

    putLong(s, s->crc);
    putLong(s, (uLong)(s->in & 0xffffffff));
    putLong(s, 0x4E444244);

    flush_write_buffer(s);
  }

  return destroy(s);
}

#include <my_dir.h>  // MY_STAT, my_fstat

int ndbz_file_size(ndbzio_stream *s, size_t *size)
{
  MY_STAT stat_buf;

  if (s == NULL || size == NULL)
    return -1;

  if (my_fstat(s->file, &stat_buf, 0) != 0)
    return -1;

  *size = stat_buf.st_size;
  return 0; /* OK */
}
