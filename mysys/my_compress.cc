/* Copyright (c) 2000, 2023, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   Without limiting anything contained in the foregoing, this file,
   which is part of C Driver for MySQL (Connector/C), is also subject to the
   Universal FOSS Exception, version 1.0, a copy of which can be found at
   http://oss.oracle.com/licenses/universal-foss-exception.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/**
  @file mysys/my_compress.cc
*/
#include "my_compress.h"

#include <string.h>
#include <sys/types.h>
#include <zlib.h>
#include <zstd.h>
#include <algorithm>
#include <cstddef>

#include <mysql_com.h>

#include "my_compiler.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_sys.h"
#include "mysql/service_mysql_alloc.h"
#include "mysys/mysys_priv.h"

/**
  Initialize a compress context object to be associated with a NET object.

  @param cmp_ctx Pointer to compression context.
  @param algorithm Compression algorithm.
  @param compression_level Compression level corresponding to the compression
  algorithm.
*/

void mysql_compress_context_init(mysql_compress_context *cmp_ctx,
                                 enum enum_compression_algorithm algorithm,
                                 unsigned int compression_level) {
  cmp_ctx->algorithm = algorithm;
  if (algorithm == enum_compression_algorithm::MYSQL_ZLIB)
    cmp_ctx->u.zlib_ctx.compression_level = compression_level;
  else if (algorithm == enum_compression_algorithm::MYSQL_ZSTD) {
    cmp_ctx->u.zstd_ctx.compression_level = compression_level;
    // This is set after connect phase during first network i/o.
    cmp_ctx->u.zstd_ctx.cctx = nullptr;
    cmp_ctx->u.zstd_ctx.dctx = nullptr;
  }
}

/**
  Deinitialize the compression context allocated.

  @param mysql_compress_ctx Pointer to Compression context.
*/

void mysql_compress_context_deinit(mysql_compress_context *mysql_compress_ctx) {
  if (mysql_compress_ctx->algorithm == enum_compression_algorithm::MYSQL_ZSTD) {
    if (mysql_compress_ctx->u.zstd_ctx.cctx != nullptr) {
      ZSTD_freeCCtx(mysql_compress_ctx->u.zstd_ctx.cctx);
      mysql_compress_ctx->u.zstd_ctx.cctx = nullptr;
    }

    if (mysql_compress_ctx->u.zstd_ctx.dctx != nullptr) {
      ZSTD_freeDCtx(mysql_compress_ctx->u.zstd_ctx.dctx);
      mysql_compress_ctx->u.zstd_ctx.dctx = nullptr;
    }
  }
}

/**
  Allocate zstd compression contexts if necessary and compress using zstd the
  buffer.

  @param comp_ctx Compression context info relating to zstd.
  @param packet   Data to compress. This is is replaced with the compressed
  data.
  @param len      Length of data to compress at 'packet'
  @param complen  out: 0 if packet was not compressed

  @return nullptr if error (len is not changed) else pointer to buffer.
  size of compressed packet).
*/

uchar *zstd_compress_alloc(mysql_zstd_compress_context *comp_ctx,
                           const uchar *packet, size_t *len, size_t *complen) {
  if (comp_ctx->cctx == nullptr) {
    if (!(comp_ctx->cctx = ZSTD_createCCtx())) {
      return nullptr;
    }
  }

  size_t zstd_len = ZSTD_compressBound(*len);
  void *compbuf;
  size_t zstd_res;

  if (!(compbuf = my_malloc(PSI_NOT_INSTRUMENTED, zstd_len, MYF(MY_WME)))) {
    return nullptr;
  }

  zstd_res =
      ZSTD_compressCCtx(comp_ctx->cctx, compbuf, zstd_len, (const void *)packet,
                        *len, comp_ctx->compression_level);
  if (ZSTD_isError(zstd_res)) {
    DBUG_PRINT("error", ("Can't compress zstd packet, error: %zd, %s", zstd_res,
                         ZSTD_getErrorName(zstd_res)));
    my_free(compbuf);
    return nullptr;
  }

  if (zstd_res > *len) {
    *complen = 0;
    my_free(compbuf);
    DBUG_PRINT("note",
               ("Packet got longer on zstd compression; Not compressed"));
    return nullptr;
  }

  *complen = *len;
  *len = zstd_res;
  return (uchar *)compbuf;
}

/**
  Uncompress a zstd compressed data.

  @param      comp_ctx    Pointer to compression context.
  @param      packet      Packet with zstd compressed data.
  @param      len         Length of zstd compressed packet.
  @param[out] complen     Length of uncompressed packet.

  @return true on error else false.
*/

static bool zstd_uncompress(mysql_zstd_compress_context *comp_ctx,
                            uchar *packet, size_t len, size_t *complen) {
  assert(comp_ctx != nullptr);
  size_t zstd_res;
  void *compbuf;

  if (comp_ctx->dctx == nullptr) {
    if (!(comp_ctx->dctx = ZSTD_createDCtx())) {
      return true;
    }
  }

  if (!(compbuf = my_malloc(PSI_NOT_INSTRUMENTED, *complen, MYF(MY_WME)))) {
    return true;
  }

  zstd_res = ZSTD_decompressDCtx(comp_ctx->dctx, compbuf, *complen,
                                 (const void *)packet, len);

  if (ZSTD_isError(zstd_res) || zstd_res != *complen) {
    DBUG_PRINT("error", ("Can't uncompress zstd packet, error: %zd, %s",
                         zstd_res, ZSTD_getErrorName(zstd_res)));
    my_free(compbuf);
    return true;
  }

  memcpy(packet, compbuf, *complen);
  my_free(compbuf);
  return false;
}

/**
  Allocate zlib compression contexts if necessary and compress using zlib the
  buffer.

  @param comp_ctx      Compression context info relating to zlib.
  @param packet        Data to compress. This is is replaced with the compressed
  data.
  @param len           Length of data to compress at 'packet'
  @param [out] complen 0 if packet was not compressed

  @return nullptr if error (len is not changed) else pointer to buffer.
  size of compressed packet).
*/

static uchar *zlib_compress_alloc(mysql_zlib_compress_context *comp_ctx,
                                  const uchar *packet, size_t *len,
                                  size_t *complen) {
  uchar *compbuf;
  uLongf tmp_complen;
  int res;
  *complen = *len * 120 / 100 + 12;

  if (!(compbuf = (uchar *)my_malloc(key_memory_my_compress_alloc, *complen,
                                     MYF(MY_WME))))
    return nullptr; /* Not enough memory */

  tmp_complen = (uint)*complen;
  res = compress2((Bytef *)compbuf, &tmp_complen,
                  (Bytef *)const_cast<uchar *>(packet), (uLong)*len,
                  comp_ctx->compression_level);
  *complen = tmp_complen;

  if (res != Z_OK) {
    my_free(compbuf);
    return nullptr;
  }

  if (*complen >= *len) {
    *complen = 0;
    my_free(compbuf);
    DBUG_PRINT("note", ("Packet got longer on compression; Not compressed"));
    return nullptr;
  }
  /* Store length of compressed packet in *len */
  std::swap(*len, *complen);
  return compbuf;
}

/**
  Uncompress a zlib compressed data.

  @param      packet      Packet which zstd compressed data.
  @param      len         Length of zstd compressed packet.
  @param[out] complen     Length of uncompressed packet.

  @return true on error else false.
*/

static bool zlib_uncompress(uchar *packet, size_t len, size_t *complen) {
  uLongf tmp_complen;
  uchar *compbuf =
      (uchar *)my_malloc(key_memory_my_compress_alloc, *complen, MYF(MY_WME));
  int error;
  if (!compbuf) return true; /* Not enough memory */

  tmp_complen = (uint)*complen;
  error =
      uncompress((Bytef *)compbuf, &tmp_complen, (Bytef *)packet, (uLong)len);
  *complen = tmp_complen;
  if (error != Z_OK) { /* Probably wrong packet */
    DBUG_PRINT("error", ("Can't uncompress packet, error: %d", error));
    my_free(compbuf);
    return true;
  }
  memcpy(packet, compbuf, *complen);
  my_free(compbuf);
  return false;
}

/**
   This replaces the packet with a compressed packet

   @param [in] comp_ctx     Compress context
   @param [in, out] packet  Data to compress. This is replaced with the
                            compressed data.
   @param [in] len          Length of data to compress at 'packet'
   @param [out] complen     Compressed packet length. 0, if packet was not
                            compressed
   @retval 1   error. 'len' is not changed
   @retval 0   ok.    'len' contains the size of the compressed packet
*/

bool my_compress(mysql_compress_context *comp_ctx, uchar *packet, size_t *len,
                 size_t *complen) {
  DBUG_ENTER("my_compress");
  if (*len < MIN_COMPRESS_LENGTH) {
    *complen = 0;
    DBUG_PRINT("note", ("Packet too short: Not compressed"));
  } else {
    uchar *compbuf = my_compress_alloc(comp_ctx, packet, len, complen);
    if (!compbuf) DBUG_RETURN(*complen ? 0 : 1);
    memcpy(packet, compbuf, *len);
    my_free(compbuf);
  }
  DBUG_RETURN(0);
}

uchar *my_compress_alloc(mysql_compress_context *comp_ctx, const uchar *packet,
                         size_t *len, size_t *complen) {
  if (comp_ctx->algorithm == enum_compression_algorithm::MYSQL_ZSTD)
    return zstd_compress_alloc(&comp_ctx->u.zstd_ctx, packet, len, complen);

  if (comp_ctx->algorithm == enum_compression_algorithm::MYSQL_UNCOMPRESSED) {
    // If compression algorithm is set to none do not compress, even if compress
    // flag was set.
    *complen = 0;
    return nullptr;
  }

  assert(comp_ctx->algorithm == enum_compression_algorithm::MYSQL_ZLIB);
  return zlib_compress_alloc(&comp_ctx->u.zlib_ctx, packet, len, complen);
}

/**
  Uncompress packet

  @param comp_ctx      Pointer to compression context.
  @param packet        Compressed data. This is is replaced with the original
                       data.
  @param len           Length of compressed data
  @param[out] complen  Length of the packet buffer after uncompression (must be
                       enough for the original data)

  @return true on error else false on success
*/

bool my_uncompress(mysql_compress_context *comp_ctx, uchar *packet, size_t len,
                   size_t *complen) {
  DBUG_ENTER("my_uncompress");
  assert(comp_ctx != nullptr);

  if (*complen) /* If compressed */
  {
    if (comp_ctx->algorithm == enum_compression_algorithm::MYSQL_ZSTD)
      DBUG_RETURN(zstd_uncompress(&comp_ctx->u.zstd_ctx, packet, len, complen));
    else if (comp_ctx->algorithm == enum_compression_algorithm::MYSQL_ZLIB)
      DBUG_RETURN(zlib_uncompress(packet, len, complen));
  }

  *complen = len;
  DBUG_RETURN(0);
}

/**
  Get default compression level corresponding to a given compression method.

  @param algorithm Compression Method. Possible values are zlib or zstd.

  @return an unsigned int representing default compression level.
          6 is the default compression level for zlib and 3 is the
          default compression level for zstd.
*/

unsigned int mysql_default_compression_level(
    enum enum_compression_algorithm algorithm) {
  switch (algorithm) {
    case MYSQL_ZLIB:
      return 6;
    case MYSQL_ZSTD:
      return 3;
    default:
      assert(0);  // should not reach here.
      return 0;   // To make compiler happy.
  }
}
