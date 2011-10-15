/* -*- mode: C; c-basic-offset: 4 -*- */
#ifndef TOKU_COMPRESS_H
#define TOKU_COMPRESS_H

#ident "$Id$"

#include <zlib.h>

// The following provides an abstraction of quicklz and zlib. 
// We offer two compression methods: ZLIB and QUICKLZ.
// The resulting byte string includes enough information for us to decompress it.  That is, we can tell whether it's z-compressed or qz-compressed.

enum toku_compression_method {
    TOKU_NO_COMPRESSION = 0, // "identity" compression
    TOKU_ZLIB_METHOD    = 8, // RFC 1950 says use 8 for zlib.  It reserves 15 to allow more bytes.  
    TOKU_QUICKLZ_METHOD = 9  // We use 9 for QUICKLZ with compression level = 3.  I couldn't find any standard for any other numbers, so I just use 9. -Bradley
};

size_t toku_compress_bound (enum toku_compression_method a, size_t size);
// Effect:  Return the number of bytes needed to compress a buffer of size SIZE using compression method A.
//  Typically, the result is a little bit larger than SIZE, since some data cannot be compressed.
// Usage note: It may help to know roughly how much space is involved.
//    zlib's bound is something like (size + (size>>12) + (size>>14) + (size>>25) + 13.
//    quicklz's bound is something like size+400.

void toku_compress (enum toku_compression_method a,
		    // the following types and naming conventions come from zlib.h
		    Bytef       *dest,   uLongf *destLen,
		    const Bytef *source, uLong   sourceLen);
// Effect: Using compression method A, compress SOURCE into DEST.   The number of bytes to compress is passed in SOURCELEN.
//  On input: *destLen is the size of the buffer.
//  On output: *destLen is the size of the actual compressed data.
// Usage note: sourceLen may be be zero (unlike for quicklz, which requires sourceLen>0).
// Requires: The buffer must be big enough to hold the compressed data.  (That is *destLen >= compressBound(a, sourceLen))
// Requires: sourceLen < 2^32.
// Usage note: Although we *try* to assert if the DESTLEN isn't big enough, it's possible that it's too late by then (in the case of quicklz which offers
//   no way to avoid a buffer overrun.)  So we require that that DESTLEN is big enough.
// Rationale:  zlib's argument order is DEST then SOURCE with the size of the buffer passed in *destLen, and the size of the result returned in *destLen.
//             quicklz's argument order is SOURCE then DEST with the size returned (and it has no way to verify that an overright didn't happen).
//     We use zlib's calling conventions partly because it is safer, and partly because it is more established.
//     We also use zlib's ugly camel case convention for destLen and sourceLen.
//     Unlike zlib, we return no error codes.  Instead, we require that the data be OK and the size of the buffers is OK, and assert if there's a problem.

void toku_decompress (Bytef       *dest,   uLongf destLen,
		      const Bytef *source, uLongf sourceLen);
// Effect: Decompress source (length sourceLen) into dest (length destLen)
//  This function can decompress data compressed with either zlib or quicklz compression methods (calling toku_compress(), which puts an appropriate header on so we know which it is.)
// Requires: destLen is equal to the actual decompressed size of the data.
// Requires: The source must have been properly compressed.

#endif
