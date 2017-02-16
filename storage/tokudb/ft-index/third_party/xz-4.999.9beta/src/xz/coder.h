/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
///////////////////////////////////////////////////////////////////////////////
//
/// \file       coder.h
/// \brief      Compresses or uncompresses a file
//
//  Author:     Lasse Collin
//
//  This file has been put into the public domain.
//  You can do whatever you want with this file.
//
///////////////////////////////////////////////////////////////////////////////

enum operation_mode {
	MODE_COMPRESS,
	MODE_DECOMPRESS,
	MODE_TEST,
	MODE_LIST,
};


// NOTE: The order of these is significant in suffix.c.
enum format_type {
	FORMAT_AUTO,
	FORMAT_XZ,
	FORMAT_LZMA,
	// HEADER_GZIP,
	FORMAT_RAW,
};


/// Operation mode of the command line tool. This is set in args.c and read
/// in several files.
extern enum operation_mode opt_mode;

/// File format to use when encoding or what format(s) to accept when
/// decoding. This is a global because it's needed also in suffix.c.
/// This is set in args.c.
extern enum format_type opt_format;


/// Set the integrity check type used when compressing
extern void coder_set_check(lzma_check check);

/// Set preset number
extern void coder_set_preset(size_t new_preset);

/// Enable extreme mode
extern void coder_set_extreme(void);

/// Add a filter to the custom filter chain
extern void coder_add_filter(lzma_vli id, void *options);

///
extern void coder_set_compression_settings(void);

/// Compress or decompress the given file
extern void coder_run(const char *filename);
