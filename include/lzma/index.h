/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
/**
 * \file        lzma/index.h
 * \brief       Handling of .xz Index lists
 */

/*
 * Author: Lasse Collin
 *
 * This file has been put into the public domain.
 * You can do whatever you want with this file.
 *
 * See ../lzma.h for information about liblzma as a whole.
 */

#ifndef LZMA_H_INTERNAL
#	error Never include this file directly. Use <lzma.h> instead.
#endif


/**
 * \brief       Opaque data type to hold the Index
 */
typedef struct lzma_index_s lzma_index;


/**
 * \brief       Index Record and its location
 */
typedef struct {
	/**
	 * \brief       Total encoded size of a Block including Block Padding
	 *
	 * This value is useful if you need to know the actual size of the
	 * Block that the Block decoder will read.
	 */
	lzma_vli total_size;

	/**
	 * \brief       Encoded size of a Block excluding Block Padding
	 *
	 * This value is stored in the Index. When doing random-access
	 * reading, you should give this value to the Block decoder along
	 * with uncompressed_size.
	 */
	lzma_vli unpadded_size;

	/**
	 * \brief       Uncompressed Size of a Block
	 */
	lzma_vli uncompressed_size;

	/**
	 * \brief       Compressed offset in the Stream(s)
	 *
	 * This is the offset of the first byte of the Block, that is,
	 * where you need to seek to decode the Block. The offset
	 * is relative to the beginning of the Stream, or if there are
	 * multiple Indexes combined, relative to the beginning of the
	 * first Stream.
	 */
	lzma_vli stream_offset;

	/**
	 * \brief       Uncompressed offset
	 *
	 * When doing random-access reading, it is possible that the target
	 * offset is not exactly at Block boundary. One will need to compare
	 * the target offset against uncompressed_offset, and possibly decode
	 * and throw away some amount of data before reaching the target
	 * offset.
	 */
	lzma_vli uncompressed_offset;

} lzma_index_record;


/**
 * \brief       Calculate memory usage for Index with given number of Records
 *
 * On disk, the size of the Index field depends on both the number of Records
 * stored and how big values the Records store (due to variable-length integer
 * encoding). When the Index is kept in lzma_index structure, the memory usage
 * depends only on the number of Records stored in the Index. The size in RAM
 * is almost always a lot bigger than in encoded form on disk.
 *
 * This function calculates an approximate amount of memory needed hold the
 * given number of Records in lzma_index structure. This value may vary
 * between liblzma versions if the internal implementation is modified.
 *
 * If you want to know how much memory an existing lzma_index structure is
 * using, use lzma_index_memusage(lzma_index_count(i)).
 */
extern LZMA_API(uint64_t) lzma_index_memusage(lzma_vli record_count)
		lzma_nothrow;


/**
 * \brief       Allocate and initialize a new lzma_index structure
 *
 * If i is NULL, a new lzma_index structure is allocated, initialized,
 * and a pointer to it returned. If allocation fails, NULL is returned.
 *
 * If i is non-NULL, it is reinitialized and the same pointer returned.
 * In this case, return value cannot be NULL or a different pointer than
 * the i that was given as an argument.
 */
extern LZMA_API(lzma_index *) lzma_index_init(
		lzma_index *i, lzma_allocator *allocator) lzma_nothrow;


/**
 * \brief       Deallocate the Index
 *
 * If i is NULL, this does nothing.
 */
extern LZMA_API(void) lzma_index_end(lzma_index *i, lzma_allocator *allocator)
		lzma_nothrow;


/**
 * \brief       Add a new Record to an Index
 *
 * \param       i                 Pointer to a lzma_index structure
 * \param       allocator         Pointer to lzma_allocator, or NULL to
 *                                use malloc()
 * \param       unpadded_size     Unpadded Size of a Block. This can be
 *                                calculated with lzma_block_unpadded_size()
 *                                after encoding or decoding the Block.
 * \param       uncompressed_size Uncompressed Size of a Block. This can be
 *                                taken directly from lzma_block structure
 *                                after encoding or decoding the Block.
 *
 * Appending a new Record does not affect the read position.
 *
 * \return      - LZMA_OK
 *              - LZMA_MEM_ERROR
 *              - LZMA_DATA_ERROR: Compressed or uncompressed size of the
 *                Stream or size of the Index field would grow too big.
 *              - LZMA_PROG_ERROR
 */
extern LZMA_API(lzma_ret) lzma_index_append(
		lzma_index *i, lzma_allocator *allocator,
		lzma_vli unpadded_size, lzma_vli uncompressed_size)
		lzma_nothrow lzma_attr_warn_unused_result;


/**
 * \brief       Get the number of Records
 */
extern LZMA_API(lzma_vli) lzma_index_count(const lzma_index *i)
		lzma_nothrow lzma_attr_pure;


/**
 * \brief       Get the size of the Index field as bytes
 *
 * This is needed to verify the Backward Size field in the Stream Footer.
 */
extern LZMA_API(lzma_vli) lzma_index_size(const lzma_index *i)
		lzma_nothrow lzma_attr_pure;


/**
 * \brief       Get the total size of the Blocks
 *
 * This doesn't include the Stream Header, Stream Footer, Stream Padding,
 * or Index fields.
 */
extern LZMA_API(lzma_vli) lzma_index_total_size(const lzma_index *i)
		lzma_nothrow lzma_attr_pure;


/**
 * \brief       Get the total size of the Stream
 *
 * If multiple Indexes have been combined, this works as if the Blocks
 * were in a single Stream.
 */
extern LZMA_API(lzma_vli) lzma_index_stream_size(const lzma_index *i)
		lzma_nothrow lzma_attr_pure;


/**
 * \brief       Get the total size of the file
 *
 * When no Indexes have been combined with lzma_index_cat(), this function is
 * identical to lzma_index_stream_size(). If multiple Indexes have been
 * combined, this includes also the headers of each separate Stream and the
 * possible Stream Padding fields.
 */
extern LZMA_API(lzma_vli) lzma_index_file_size(const lzma_index *i)
		lzma_nothrow lzma_attr_pure;


/**
 * \brief       Get the uncompressed size of the Stream
 */
extern LZMA_API(lzma_vli) lzma_index_uncompressed_size(const lzma_index *i)
		lzma_nothrow lzma_attr_pure;


/**
 * \brief       Get the next Record from the Index
 */
extern LZMA_API(lzma_bool) lzma_index_read(
		lzma_index *i, lzma_index_record *record)
		lzma_nothrow lzma_attr_warn_unused_result;


/**
 * \brief       Rewind the Index
 *
 * Rewind the Index so that next call to lzma_index_read() will return the
 * first Record.
 */
extern LZMA_API(void) lzma_index_rewind(lzma_index *i) lzma_nothrow;


/**
 * \brief       Locate a Record
 *
 * When the Index is available, it is possible to do random-access reading
 * with granularity of Block size.
 *
 * \param       i       Pointer to lzma_index structure
 * \param       record  Pointer to a structure to hold the search results
 * \param       target  Uncompressed target offset which the caller would
 *                      like to locate from the Stream
 *
 * If the target is smaller than the uncompressed size of the Stream (can be
 * checked with lzma_index_uncompressed_size()):
 *  - Information about the Record containing the requested uncompressed
 *    offset is stored into *record.
 *  - Read offset will be adjusted so that calling lzma_index_read() can be
 *    used to read subsequent Records.
 *  - This function returns false.
 *
 * If target is greater than the uncompressed size of the Stream, *record
 * and the read position are not modified, and this function returns true.
 */
extern LZMA_API(lzma_bool) lzma_index_locate(
		lzma_index *i, lzma_index_record *record, lzma_vli target)
		lzma_nothrow;


/**
 * \brief       Concatenate Indexes of two Streams
 *
 * Concatenating Indexes is useful when doing random-access reading in
 * multi-Stream .xz file, or when combining multiple Streams into single
 * Stream.
 *
 * \param       dest      Destination Index after which src is appended
 * \param       src       Source Index. If this function succeeds, the
 *                        memory allocated for src is freed or moved to
 *                        be part of dest.
 * \param       allocator Custom memory allocator; can be NULL to use
 *                        malloc() and free().
 * \param       padding   Size of the Stream Padding field between Streams.
 *                        This must be a multiple of four.
 *
 * \return      - LZMA_OK: Indexes concatenated successfully. src is now
 *                a dangling pointer.
 *              - LZMA_DATA_ERROR: *dest would grow too big.
 *              - LZMA_MEM_ERROR
 *              - LZMA_PROG_ERROR
 */
extern LZMA_API(lzma_ret) lzma_index_cat(lzma_index *lzma_restrict dest,
		lzma_index *lzma_restrict src,
		lzma_allocator *allocator, lzma_vli padding)
		lzma_nothrow lzma_attr_warn_unused_result;


/**
 * \brief       Duplicate an Index list
 *
 * Makes an identical copy of the Index. Also the read position is copied.
 *
 * \return      A copy of the Index, or NULL if memory allocation failed.
 */
extern LZMA_API(lzma_index *) lzma_index_dup(
		const lzma_index *i, lzma_allocator *allocator)
		lzma_nothrow lzma_attr_warn_unused_result;


/**
 * \brief       Compare if two Index lists are identical
 *
 * Read positions are not compared.
 *
 * \return      True if *a and *b are equal, false otherwise.
 */
extern LZMA_API(lzma_bool) lzma_index_equal(
		const lzma_index *a, const lzma_index *b)
		lzma_nothrow lzma_attr_pure;


/**
 * \brief       Initialize .xz Index encoder
 *
 * \param       strm        Pointer to properly prepared lzma_stream
 * \param       i           Pointer to lzma_index which should be encoded.
 *                          The read position will be at the end of the Index
 *                          after lzma_code() has returned LZMA_STREAM_END.
 *
 * The only valid action value for lzma_code() is LZMA_RUN.
 *
 * \return      - LZMA_OK: Initialization succeeded, continue with lzma_code().
 *              - LZMA_MEM_ERROR
 *              - LZMA_PROG_ERROR
 */
extern LZMA_API(lzma_ret) lzma_index_encoder(lzma_stream *strm, lzma_index *i)
		lzma_nothrow lzma_attr_warn_unused_result;


/**
 * \brief       Initialize .xz Index decoder
 *
 * \param       strm        Pointer to properly prepared lzma_stream
 * \param       i           Pointer to a pointer that will be made to point
 *                          to the final decoded Index once lzma_code() has
 *                          returned LZMA_STREAM_END. That is,
 *                          lzma_index_decoder() always takes care of
 *                          allocating a new lzma_index structure, and *i
 *                          doesn't need to be initialized by the caller.
 * \param       memlimit    How much memory the resulting Index is allowed
 *                          to require.
 *
 * The only valid action value for lzma_code() is LZMA_RUN.
 *
 * \return      - LZMA_OK: Initialization succeeded, continue with lzma_code().
 *              - LZMA_MEM_ERROR
 *              - LZMA_MEMLIMIT_ERROR
 *              - LZMA_PROG_ERROR
 *
 * \note        The memory usage limit is checked early in the decoding
 *              (within the first dozen input bytes or so). The actual memory
 *              is allocated later in smaller pieces. If the memory usage
 *              limit is modified with lzma_memlimit_set() after a part
 *              of the Index has already been decoded, the new limit may
 *              get ignored.
 */
extern LZMA_API(lzma_ret) lzma_index_decoder(
		lzma_stream *strm, lzma_index **i, uint64_t memlimit)
		lzma_nothrow lzma_attr_warn_unused_result;


/**
 * \brief       Single-call .xz Index encoder
 *
 * \param       i         Index to be encoded. The read position will be at
 *                        the end of the Index if encoding succeeds, or at
 *                        unspecified position in case an error occurs.
 * \param       out       Beginning of the output buffer
 * \param       out_pos   The next byte will be written to out[*out_pos].
 *                        *out_pos is updated only if encoding succeeds.
 * \param       out_size  Size of the out buffer; the first byte into
 *                        which no data is written to is out[out_size].
 *
 * \return      - LZMA_OK: Encoding was successful.
 *              - LZMA_BUF_ERROR: Output buffer is too small. Use
 *                lzma_index_size() to find out how much output
 *                space is needed.
 *              - LZMA_PROG_ERROR
 *
 * \note        This function doesn't take allocator argument since all
 *              the internal data is allocated on stack.
 */
extern LZMA_API(lzma_ret) lzma_index_buffer_encode(lzma_index *i,
		uint8_t *out, size_t *out_pos, size_t out_size) lzma_nothrow;


/**
 * \brief       Single-call .xz Index decoder
 *
 * \param       i           Pointer to a pointer that will be made to point
 *                          to the final decoded Index if decoding is
 *                          successful. That is, lzma_index_buffer_decode()
 *                          always takes care of allocating a new
 *                          lzma_index structure, and *i doesn't need to be
 *                          initialized by the caller.
 * \param       memlimit    Pointer to how much memory the resulting Index
 *                          is allowed to require. The value pointed by
 *                          this pointer is modified if and only if
 *                          LZMA_MEMLIMIT_ERROR is returned.
 * \param       allocator   Pointer to lzma_allocator, or NULL to use malloc()
 * \param       in          Beginning of the input buffer
 * \param       in_pos      The next byte will be read from in[*in_pos].
 *                          *in_pos is updated only if decoding succeeds.
 * \param       in_size     Size of the input buffer; the first byte that
 *                          won't be read is in[in_size].
 *
 * \return      - LZMA_OK: Decoding was successful.
 *              - LZMA_MEM_ERROR
 *              - LZMA_MEMLIMIT_ERROR: Memory usage limit was reached.
 *                The minimum required memlimit value was stored to *memlimit.
 *              - LZMA_DATA_ERROR
 *              - LZMA_PROG_ERROR
 */
extern LZMA_API(lzma_ret) lzma_index_buffer_decode(lzma_index **i,
		uint64_t *memlimit, lzma_allocator *allocator,
		const uint8_t *in, size_t *in_pos, size_t in_size)
		lzma_nothrow;
