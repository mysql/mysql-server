/*
   Copyright (c) 2011, 2012, Oracle and/or its affiliates. All rights reserved.

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
  This library has been brought into the 21st century.
   - Magnus Blåudd
*/

/*
  This libary has been modified for use by the MySQL Archive Engine.
     -Brian Aker
*/

/* zlib.h -- interface of the 'zlib' general purpose compression library
  version 1.2.3, July 18th, 2005

  Copyright (C) 1995-2005 Jean-loup Gailly and Mark Adler

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.

  Jean-loup Gailly        Mark Adler
  jloup@gzip.org          madler@alumni.caltech.edu


  The data format used by the zlib library is described by RFCs (Request for
  Comments) 1950 to 1952 in the files http://www.ietf.org/rfc/rfc1950.txt
  (zlib format), rfc1951.txt (deflate format) and rfc1952.txt (gzip format).
*/

#include <zlib.h>

#ifdef  __cplusplus
extern "C" {
#endif

size_t ndbz_inflate_mem_size();
size_t ndbz_deflate_mem_size();
struct ndbz_alloc_rec {
  size_t size;
  size_t mfree;
  char *mem;
};

typedef struct ndbzio_stream {
  z_stream stream;
  int      z_err;   /* error code for last stream operation */
  int      z_eof;   /* set if end of input file */
  File     file;   /* .gz file */
  Byte     *inbuf;  /* input buffer */
  Byte     *outbuf; /* output buffer */
  uLong    crc;     /* crc32 of uncompressed data */
  char     *msg;    /* error message */
  int      transparent; /* 1 if input file is not a .gz file */
  char     mode;    /* 'w' or 'r' */
  char     bufalloced; /* true if ndbzio allocated buffers */
  my_off_t  start;   /* start of compressed data in file (header skipped) */
  my_off_t  in;      /* bytes into deflate or inflate */
  my_off_t  out;     /* bytes out of deflate or inflate */
  int      back;    /* one character push-back */
  int      last;    /* true if push-back is last character */
  unsigned char version;   /* Version */
  unsigned char minor_version;   /* Version */
  unsigned int block_size;   /* Block Size */
  unsigned long long check_point;   /* Last position we checked */
  unsigned long long forced_flushes;   /* Forced Flushes */
  unsigned long long rows;   /* rows */
  unsigned long long auto_increment;   /* auto increment field */
  unsigned int longest_row;   /* Longest row */
  unsigned int shortest_row;   /* Shortest row */
  unsigned char dirty;   /* State of file */
  unsigned int frm_start_pos;   /* Position for start of FRM */
  unsigned int frm_length;   /* Position for start of FRM */
  unsigned int comment_start_pos;   /* Position for start of comment */
  unsigned int comment_length;   /* Position for start of comment */
} ndbzio_stream;

/* Return the size in bytes used for reading */
size_t ndbz_bufsize_read(void);

/* Return the size in bytes used for writing */
size_t ndbz_bufsize_write(void);

                        /* basic functions */
extern int ndbzopen(ndbzio_stream *s, const char *path, int Flags);
/*
     Opens a gzip (.gz) file for reading or writing. The mode parameter
   is as in fopen ("rb" or "wb") but can also include a compression level
   ("wb9") or a strategy: 'f' for filtered data as in "wb6f", 'h' for
   Huffman only compression as in "wb1h", or 'R' for run-length encoding
   as in "wb1R". (See the description of deflateInit2 for more information
   about the strategy parameter.)

     ndbzopen can be used to read a file which is not in gzip format; in this
   case gzread will directly read from the file without decompression.

     ndbzopen returns NULL if the file could not be opened or if there was
   insufficient memory to allocate the (de)compression state; errno
   can be checked to distinguish the two cases (if errno is zero, the
   zlib error is Z_MEM_ERROR).  */

int ndbzdopen(ndbzio_stream *s, File fd, int Flags);
/*
     ndbzdopen() associates a ndbzio_stream with the file descriptor fd.  File
   descriptors are obtained from calls like open, dup, creat, pipe or
   fileno (in the file has been previously opened with fopen).
   The mode parameter is as in ndbzopen.
     The next call of gzclose on the returned ndbzio_stream will also close the
   file descriptor fd, just like fclose(fdopen(fd), mode) closes the file
   descriptor fd. If you want to keep fd open, use ndbzdopen(dup(fd), mode).
     ndbzdopen returns NULL if there was insufficient memory to allocate
   the (de)compression state.
*/


extern unsigned int ndbzread (ndbzio_stream *s, voidp buf,
                              unsigned int len, int *error);
/*
     Reads the given number of uncompressed bytes from the compressed file.
   If the input file was not in gzip format, gzread copies the given number
   of bytes into the buffer.
     gzread returns the number of uncompressed bytes actually read (0 for
   end of file, -1 for error). */

extern unsigned int ndbzwrite (ndbzio_stream *s, const void* buf,
                               unsigned int len);
/*
     Writes the given number of uncompressed bytes into the compressed file.
   ndbzwrite returns the number of uncompressed bytes actually written
   (0 in case of error).
*/


extern int ndbzflush(ndbzio_stream *file, int flush);
/*
     Flushes all pending output into the compressed file. The parameter
   flush is as in the deflate() function. The return value is the zlib
   error number (see function gzerror below). gzflush returns Z_OK if
   the flush parameter is Z_FINISH and all output could be flushed.
     gzflush should be called only when strictly necessary because it can
   degrade compression.
*/

extern my_off_t ndbzseek (ndbzio_stream *file,
                          my_off_t offset, int whence);
/*
      Sets the starting position for the next gzread or gzwrite on the
   given compressed file. The offset represents a number of bytes in the
   uncompressed data stream. The whence parameter is defined as in lseek(2);
   the value SEEK_END is not supported.
     If the file is opened for reading, this function is emulated but can be
   extremely slow. If the file is opened for writing, only forward seeks are
   supported; gzseek then compresses a sequence of zeroes up to the new
   starting position.

      gzseek returns the resulting offset location as measured in bytes from
   the beginning of the uncompressed stream, or -1 in case of error, in
   particular if the file is opened for writing and the new starting position
   would be before the current position.
*/

extern int ndbzrewind(ndbzio_stream *file);
/*
     Rewinds the given file. This function is supported only for reading.

   gzrewind(file) is equivalent to (int)gzseek(file, 0L, SEEK_SET)
*/

extern my_off_t ndbztell(ndbzio_stream *file);
/*
     Returns the starting position for the next gzread or gzwrite on the
   given compressed file. This position represents a number of bytes in the
   uncompressed data stream.

   gztell(file) is equivalent to gzseek(file, 0L, SEEK_CUR)
*/

extern int ndbzclose(ndbzio_stream *file);
/*
     Flushes all pending output if necessary, closes the compressed file
   and deallocates all the (de)compression state. The return value is the zlib
   error number (see function gzerror below).
*/

/*
  Return file size of the open ndbzio_stream
*/
extern int ndbz_file_size(ndbzio_stream *file, size_t *size);

#ifdef	__cplusplus
}
#endif
