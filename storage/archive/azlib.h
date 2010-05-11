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

#include "../../mysys/mysys_priv.h"
#include <my_dir.h>
#include <zlib.h>

#ifdef  __cplusplus
extern "C" {
#endif
/* Start of MySQL Specific Information */

/*
  ulonglong + ulonglong + ulonglong + ulonglong + uchar
*/
#define AZMETA_BUFFER_SIZE sizeof(unsigned long long) \
  + sizeof(unsigned long long) + sizeof(unsigned long long) + sizeof(unsigned long long) \
  + sizeof(unsigned int) + sizeof(unsigned int) \
  + sizeof(unsigned int) + sizeof(unsigned int) \
  + sizeof(unsigned char)

#define AZHEADER_SIZE 29

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

/*
     The 'zlib' compression library provides in-memory compression and
  decompression functions, including integrity checks of the uncompressed
  data.  This version of the library supports only one compression method
  (deflation) but other algorithms will be added later and will have the same
  stream interface.

     Compression can be done in a single step if the buffers are large
  enough (for example if an input file is mmap'ed), or can be done by
  repeated calls of the compression function.  In the latter case, the
  application must provide more input and/or consume the output
  (providing more output space) before each call.

     The compressed data format used by default by the in-memory functions is
  the zlib format, which is a zlib wrapper documented in RFC 1950, wrapped
  around a deflate stream, which is itself documented in RFC 1951.

     The library also supports reading and writing files in gzip (.gz) format
  with an interface similar to that of stdio using the functions that start
  with "gz".  The gzip format is different from the zlib format.  gzip is a
  gzip wrapper, documented in RFC 1952, wrapped around a deflate stream.

     This library can optionally read and write gzip streams in memory as well.

     The zlib format was designed to be compact and fast for use in memory
  and on communications channels.  The gzip format was designed for single-
  file compression on file systems, has a larger header than zlib to maintain
  directory information, and uses a different, slower check method than zlib.

     The library does not install any signal handler. The decoder checks
  the consistency of the compressed data, so the library should never
  crash even in case of corrupted input.
*/


/*
   The application must update next_in and avail_in when avail_in has
   dropped to zero. It must update next_out and avail_out when avail_out
   has dropped to zero. The application must initialize zalloc, zfree and
   opaque before calling the init function. All other fields are set by the
   compression library and must not be updated by the application.

   The opaque value provided by the application will be passed as the first
   parameter for calls of zalloc and zfree. This can be useful for custom
   memory management. The compression library attaches no meaning to the
   opaque value.

   zalloc must return Z_NULL if there is not enough memory for the object.
   If zlib is used in a multi-threaded application, zalloc and zfree must be
   thread safe.

   On 16-bit systems, the functions zalloc and zfree must be able to allocate
   exactly 65536 bytes, but will not be required to allocate more than this
   if the symbol MAXSEG_64K is defined (see zconf.h). WARNING: On MSDOS,
   pointers returned by zalloc for objects of exactly 65536 bytes *must*
   have their offset normalized to zero. The default allocation function
   provided by this library ensures this (see zutil.c). To reduce memory
   requirements and avoid any allocation of 64K objects, at the expense of
   compression ratio, compile the library with -DMAX_WBITS=14 (see zconf.h).

   The fields total_in and total_out can be used for statistics or
   progress reports. After compression, total_in holds the total size of
   the uncompressed data and may be saved for use in the decompressor
   (particularly if the decompressor wants to decompress everything in
   a single step).
*/

                        /* constants */

#define Z_NO_FLUSH      0
#define Z_PARTIAL_FLUSH 1 /* will be removed, use Z_SYNC_FLUSH instead */
#define Z_SYNC_FLUSH    2
#define Z_FULL_FLUSH    3
#define Z_FINISH        4
#define Z_BLOCK         5
/* Allowed flush values; see deflate() and inflate() below for details */

#define Z_OK            0
#define Z_STREAM_END    1
#define Z_NEED_DICT     2
#define Z_ERRNO        (-1)
#define Z_STREAM_ERROR (-2)
#define Z_DATA_ERROR   (-3)
#define Z_MEM_ERROR    (-4)
#define Z_BUF_ERROR    (-5)
#define Z_VERSION_ERROR (-6)
/* Return codes for the compression/decompression functions. Negative
 * values are errors, positive values are used for special but normal events.
 */

#define Z_NO_COMPRESSION         0
#define Z_BEST_SPEED             1
#define Z_BEST_COMPRESSION       9
#define Z_DEFAULT_COMPRESSION  (-1)
/* compression levels */

#define Z_FILTERED            1
#define Z_HUFFMAN_ONLY        2
#define Z_RLE                 3
#define Z_FIXED               4
#define Z_DEFAULT_STRATEGY    0
/* compression strategy; see deflateInit2() below for details */

#define Z_BINARY   0
#define Z_TEXT     1
#define Z_ASCII    Z_TEXT   /* for compatibility with 1.2.2 and earlier */
#define Z_UNKNOWN  2
/* Possible values of the data_type field (though see inflate()) */

#define Z_DEFLATED   8
/* The deflate compression method (the only one supported in this version) */

#define Z_NULL  0  /* for initializing zalloc, zfree, opaque */
#define AZ_BUFSIZE_READ 32768
#define AZ_BUFSIZE_WRITE 16384


typedef struct azio_stream {
  z_stream stream;
  int      z_err;   /* error code for last stream operation */
  int      z_eof;   /* set if end of input file */
  File     file;   /* .gz file */
  Byte     inbuf[AZ_BUFSIZE_READ];  /* input buffer */
  Byte     outbuf[AZ_BUFSIZE_WRITE]; /* output buffer */
  uLong    crc;     /* crc32 of uncompressed data */
  char     *msg;    /* error message */
  int      transparent; /* 1 if input file is not a .gz file */
  char     mode;    /* 'w' or 'r' */
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
} azio_stream;

                        /* basic functions */

extern int azopen(azio_stream *s, const char *path, int Flags);
/*
     Opens a gzip (.gz) file for reading or writing. The mode parameter
   is as in fopen ("rb" or "wb") but can also include a compression level
   ("wb9") or a strategy: 'f' for filtered data as in "wb6f", 'h' for
   Huffman only compression as in "wb1h", or 'R' for run-length encoding
   as in "wb1R". (See the description of deflateInit2 for more information
   about the strategy parameter.)

     azopen can be used to read a file which is not in gzip format; in this
   case gzread will directly read from the file without decompression.

     azopen returns NULL if the file could not be opened or if there was
   insufficient memory to allocate the (de)compression state; errno
   can be checked to distinguish the two cases (if errno is zero, the
   zlib error is Z_MEM_ERROR).  */

int azdopen(azio_stream *s,File fd, int Flags); 
/*
     azdopen() associates a azio_stream with the file descriptor fd.  File
   descriptors are obtained from calls like open, dup, creat, pipe or
   fileno (in the file has been previously opened with fopen).
   The mode parameter is as in azopen.
     The next call of gzclose on the returned azio_stream will also close the
   file descriptor fd, just like fclose(fdopen(fd), mode) closes the file
   descriptor fd. If you want to keep fd open, use azdopen(dup(fd), mode).
     azdopen returns NULL if there was insufficient memory to allocate
   the (de)compression state.
*/


extern unsigned int azread ( azio_stream *s, voidp buf, size_t len, int *error);
/*
     Reads the given number of uncompressed bytes from the compressed file.
   If the input file was not in gzip format, gzread copies the given number
   of bytes into the buffer.
     gzread returns the number of uncompressed bytes actually read (0 for
   end of file, -1 for error). */

extern unsigned int azwrite (azio_stream *s, const voidp buf, unsigned int len);
/*
     Writes the given number of uncompressed bytes into the compressed file.
   azwrite returns the number of uncompressed bytes actually written
   (0 in case of error).
*/


extern int azflush(azio_stream *file, int flush);
/*
     Flushes all pending output into the compressed file. The parameter
   flush is as in the deflate() function. The return value is the zlib
   error number (see function gzerror below). gzflush returns Z_OK if
   the flush parameter is Z_FINISH and all output could be flushed.
     gzflush should be called only when strictly necessary because it can
   degrade compression.
*/

extern my_off_t azseek (azio_stream *file,
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

extern int azrewind(azio_stream *file);
/*
     Rewinds the given file. This function is supported only for reading.

   gzrewind(file) is equivalent to (int)gzseek(file, 0L, SEEK_SET)
*/

extern my_off_t aztell(azio_stream *file);
/*
     Returns the starting position for the next gzread or gzwrite on the
   given compressed file. This position represents a number of bytes in the
   uncompressed data stream.

   gztell(file) is equivalent to gzseek(file, 0L, SEEK_CUR)
*/

extern int azclose(azio_stream *file);
/*
     Flushes all pending output if necessary, closes the compressed file
   and deallocates all the (de)compression state. The return value is the zlib
   error number (see function gzerror below).
*/

extern int azwrite_frm (azio_stream *s, char *blob, unsigned int length);
extern int azread_frm (azio_stream *s, char *blob);
extern int azwrite_comment (azio_stream *s, char *blob, unsigned int length);
extern int azread_comment (azio_stream *s, char *blob);

#ifdef	__cplusplus
}
#endif
