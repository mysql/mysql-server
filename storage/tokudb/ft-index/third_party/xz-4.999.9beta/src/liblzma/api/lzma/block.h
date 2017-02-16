/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
/**
 * \file        lzma/block.h
 * \brief       .xz Block handling
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
 * \brief       Options for the Block and Block Header encoders and decoders
 *
 * Different Block handling functions use different parts of this structure.
 * Some read some members, other functions write, and some do both. Only the
 * members listed for reading need to be initialized when the specified
 * functions are called. The members marked for writing will be assigned
 * new values at some point either by calling the given function or by
 * later calls to lzma_code().
 */
typedef struct {
	/**
	 * \brief       Block format version
	 *
	 * To prevent API and ABI breakages if new features are needed in
	 * Block, a version number is used to indicate which fields in this
	 * structure are in use. For now, version must always be zero.
	 * With non-zero version, most Block related functions will return
	 * LZMA_OPTIONS_ERROR.
	 *
	 * The decoding functions will always set this to the lowest value
	 * that supports all the features indicated by the Block Header field.
	 * The application must check that the version number set by the
	 * decoding functions is supported by the application. Otherwise it
	 * is possible that the application will decode the Block incorrectly.
	 *
	 * Read by:
	 *  - lzma_block_header_size()
	 *  - lzma_block_header_encode()
	 *  - lzma_block_compressed_size()
	 *  - lzma_block_unpadded_size()
	 *  - lzma_block_total_size()
	 *  - lzma_block_encoder()
	 *  - lzma_block_decoder()
	 *  - lzma_block_buffer_encode()
	 *  - lzma_block_buffer_decode()
	 *
	 * Written by:
	 *  - lzma_block_header_decode()
	 */
	uint32_t version;

	/**
	 * \brief       Size of the Block Header field
	 *
	 * This is always a multiple of four.
	 *
	 * Read by:
	 *  - lzma_block_header_encode()
	 *  - lzma_block_header_decode()
	 *  - lzma_block_compressed_size()
	 *  - lzma_block_unpadded_size()
	 *  - lzma_block_total_size()
	 *  - lzma_block_decoder()
	 *  - lzma_block_buffer_decode()
	 *
	 * Written by:
	 *  - lzma_block_header_size()
	 *  - lzma_block_buffer_encode()
	 */
	uint32_t header_size;
#	define LZMA_BLOCK_HEADER_SIZE_MIN 8
#	define LZMA_BLOCK_HEADER_SIZE_MAX 1024

	/**
	 * \brief       Type of integrity Check
	 *
	 * The Check ID is not stored into the Block Header, thus its value
	 * must be provided also when decoding.
	 *
	 * Read by:
	 *  - lzma_block_header_encode()
	 *  - lzma_block_header_decode()
	 *  - lzma_block_compressed_size()
	 *  - lzma_block_unpadded_size()
	 *  - lzma_block_total_size()
	 *  - lzma_block_encoder()
	 *  - lzma_block_decoder()
	 *  - lzma_block_buffer_encode()
	 *  - lzma_block_buffer_decode()
	 */
	lzma_check check;

	/**
	 * \brief       Size of the Compressed Data in bytes
	 *
	 * Encoding: If this is not LZMA_VLI_UNKNOWN, Block Header encoder
	 * will store this value to the Block Header. Block encoder doesn't
	 * care about this value, but will set it once the encoding has been
	 * finished.
	 *
	 * Decoding: If this is not LZMA_VLI_UNKNOWN, Block decoder will
	 * verify that the size of the Compressed Data field matches
	 * compressed_size.
	 *
	 * Usually you don't know this value when encoding in streamed mode,
	 * and thus cannot write this field into the Block Header.
	 *
	 * In non-streamed mode you can reserve space for this field before
	 * encoding the actual Block. After encoding the data, finish the
	 * Block by encoding the Block Header. Steps in detail:
	 *
	 *  - Set compressed_size to some big enough value. If you don't know
	 *    better, use LZMA_VLI_MAX, but remember that bigger values take
	 *    more space in Block Header.
	 *
	 *  - Call lzma_block_header_size() to see how much space you need to
	 *    reserve for the Block Header.
	 *
	 *  - Encode the Block using lzma_block_encoder() and lzma_code().
	 *    It sets compressed_size to the correct value.
	 *
	 *  - Use lzma_block_header_encode() to encode the Block Header.
	 *    Because space was reserved in the first step, you don't need
	 *    to call lzma_block_header_size() anymore, because due to
	 *    reserving, header_size has to be big enough. If it is "too big",
	 *    lzma_block_header_encode() will add enough Header Padding to
	 *    make Block Header to match the size specified by header_size.
	 *
	 * Read by:
	 *  - lzma_block_header_size()
	 *  - lzma_block_header_encode()
	 *  - lzma_block_compressed_size()
	 *  - lzma_block_unpadded_size()
	 *  - lzma_block_total_size()
	 *  - lzma_block_decoder()
	 *  - lzma_block_buffer_decode()
	 *
	 * Written by:
	 *  - lzma_block_header_decode()
	 *  - lzma_block_compressed_size()
	 *  - lzma_block_encoder()
	 *  - lzma_block_decoder()
	 *  - lzma_block_buffer_encode()
	 *  - lzma_block_buffer_decode()
	 */
	lzma_vli compressed_size;

	/**
	 * \brief       Uncompressed Size in bytes
	 *
	 * This is handled very similarly to compressed_size above.
	 *
	 * uncompressed_size is needed by fewer functions than
	 * compressed_size. This is because uncompressed_size isn't
	 * needed to validate that Block stays within proper limits.
	 *
	 * Read by:
	 *  - lzma_block_header_size()
	 *  - lzma_block_header_encode()
	 *  - lzma_block_decoder()
	 *  - lzma_block_buffer_decode()
	 *
	 * Written by:
	 *  - lzma_block_header_decode()
	 *  - lzma_block_encoder()
	 *  - lzma_block_decoder()
	 *  - lzma_block_buffer_encode()
	 *  - lzma_block_buffer_decode()
	 */
	lzma_vli uncompressed_size;

	/**
	 * \brief       Array of filters
	 *
	 * There can be 1-4 filters. The end of the array is marked with
	 * .id = LZMA_VLI_UNKNOWN.
	 *
	 * Read by:
	 *  - lzma_block_header_size()
	 *  - lzma_block_header_encode()
	 *  - lzma_block_encoder()
	 *  - lzma_block_decoder()
	 *  - lzma_block_buffer_encode()
	 *  - lzma_block_buffer_decode()
	 *
	 * Written by:
	 *  - lzma_block_header_decode(): Note that this does NOT free()
	 *    the old filter options structures. All unused filters[] will
	 *    have .id == LZMA_VLI_UNKNOWN and .options == NULL. If
	 *    decoding fails, all filters[] are guaranteed to be
	 *    LZMA_VLI_UNKNOWN and NULL.
	 *
	 * \note        Because of the array is terminated with
	 *              .id = LZMA_VLI_UNKNOWN, the actual array must
	 *              have LZMA_FILTERS_MAX + 1 members or the Block
	 *              Header decoder will overflow the buffer.
	 */
	lzma_filter *filters;

	/**
	 * \brief       Raw value stored in the Check field
	 *
	 * After successful coding, the first lzma_check_size(check) bytes
	 * of this array contain the raw value stored in the Check field.
	 *
	 * Note that CRC32 and CRC64 are stored in little endian byte order.
	 * Take it into account if you display the Check values to the user.
	 *
	 * Written by:
	 *  - lzma_block_encoder()
	 *  - lzma_block_decoder()
	 *  - lzma_block_buffer_encode()
	 *  - lzma_block_buffer_decode()
	 */
	uint8_t raw_check[LZMA_CHECK_SIZE_MAX];

	/*
	 * Reserved space to allow possible future extensions without
	 * breaking the ABI. You should not touch these, because the names
	 * of these variables may change. These are and will never be used
	 * with the currently supported options, so it is safe to leave these
	 * uninitialized.
	 */
	void *reserved_ptr1;
	void *reserved_ptr2;
	void *reserved_ptr3;
	uint32_t reserved_int1;
	uint32_t reserved_int2;
	lzma_vli reserved_int3;
	lzma_vli reserved_int4;
	lzma_vli reserved_int5;
	lzma_vli reserved_int6;
	lzma_vli reserved_int7;
	lzma_vli reserved_int8;
	lzma_reserved_enum reserved_enum1;
	lzma_reserved_enum reserved_enum2;
	lzma_reserved_enum reserved_enum3;
	lzma_reserved_enum reserved_enum4;
	lzma_bool reserved_bool1;
	lzma_bool reserved_bool2;
	lzma_bool reserved_bool3;
	lzma_bool reserved_bool4;
	lzma_bool reserved_bool5;
	lzma_bool reserved_bool6;
	lzma_bool reserved_bool7;
	lzma_bool reserved_bool8;

} lzma_block;


/**
 * \brief       Decode the Block Header Size field
 *
 * To decode Block Header using lzma_block_header_decode(), the size of the
 * Block Header has to be known and stored into lzma_block.header_size.
 * The size can be calculated from the first byte of a Block using this macro.
 * Note that if the first byte is 0x00, it indicates beginning of Index; use
 * this macro only when the byte is not 0x00.
 *
 * There is no encoding macro, because Block Header encoder is enough for that.
 */
#define lzma_block_header_size_decode(b) (((uint32_t)(b) + 1) * 4)


/**
 * \brief       Calculate Block Header Size
 *
 * Calculate the minimum size needed for the Block Header field using the
 * settings specified in the lzma_block structure. Note that it is OK to
 * increase the calculated header_size value as long as it is a multiple of
 * four and doesn't exceed LZMA_BLOCK_HEADER_SIZE_MAX. Increasing header_size
 * just means that lzma_block_header_encode() will add Header Padding.
 *
 * \return      - LZMA_OK: Size calculated successfully and stored to
 *                block->header_size.
 *              - LZMA_OPTIONS_ERROR: Unsupported version, filters or
 *                filter options.
 *              - LZMA_PROG_ERROR: Invalid values like compressed_size == 0.
 *
 * \note        This doesn't check that all the options are valid i.e. this
 *              may return LZMA_OK even if lzma_block_header_encode() or
 *              lzma_block_encoder() would fail. If you want to validate the
 *              filter chain, consider using lzma_memlimit_encoder() which as
 *              a side-effect validates the filter chain.
 */
extern LZMA_API(lzma_ret) lzma_block_header_size(lzma_block *block)
		lzma_nothrow lzma_attr_warn_unused_result;


/**
 * \brief       Encode Block Header
 *
 * The caller must have calculated the size of the Block Header already with
 * lzma_block_header_size(). If a value larger than the one calculated by
 * lzma_block_header_size() is used, the Block Header will be padded to the
 * specified size.
 *
 * \param       out         Beginning of the output buffer. This must be
 *                          at least block->header_size bytes.
 * \param       block       Block options to be encoded.
 *
 * \return      - LZMA_OK: Encoding was successful. block->header_size
 *                bytes were written to output buffer.
 *              - LZMA_OPTIONS_ERROR: Invalid or unsupported options.
 *              - LZMA_PROG_ERROR: Invalid arguments, for example
 *                block->header_size is invalid or block->filters is NULL.
 */
extern LZMA_API(lzma_ret) lzma_block_header_encode(
		const lzma_block *block, uint8_t *out)
		lzma_nothrow lzma_attr_warn_unused_result;


/**
 * \brief       Decode Block Header
 *
 * The size of the Block Header must have already been decoded with
 * lzma_block_header_size_decode() macro and stored to block->header_size.
 * block->filters must have been allocated, but not necessarily initialized.
 * Possible existing filter options are _not_ freed.
 *
 * \param       block       Destination for block options with header_size
 *                          properly initialized.
 * \param       allocator   lzma_allocator for custom allocator functions.
 *                          Set to NULL to use malloc() (and also free()
 *                          if an error occurs).
 * \param       in          Beginning of the input buffer. This must be
 *                          at least block->header_size bytes.
 *
 * \return      - LZMA_OK: Decoding was successful. block->header_size
 *                bytes were read from the input buffer.
 *              - LZMA_OPTIONS_ERROR: The Block Header specifies some
 *                unsupported options such as unsupported filters.
 *              - LZMA_DATA_ERROR: Block Header is corrupt, for example,
 *                the CRC32 doesn't match.
 *              - LZMA_PROG_ERROR: Invalid arguments, for example
 *                block->header_size is invalid or block->filters is NULL.
 */
extern LZMA_API(lzma_ret) lzma_block_header_decode(lzma_block *block,
		lzma_allocator *allocator, const uint8_t *in)
		lzma_nothrow lzma_attr_warn_unused_result;


/**
 * \brief       Validate and set Compressed Size according to Unpadded Size
 *
 * Block Header stores Compressed Size, but Index has Unpadded Size. If the
 * application has already parsed the Index and is now decoding Blocks,
 * it can calculate Compressed Size from Unpadded Size. This function does
 * exactly that with error checking:
 *
 *  - Compressed Size calculated from Unpadded Size must be positive integer,
 *    that is, Unpadded Size must be big enough that after Block Header and
 *    Check fields there's still at least one byte for Compressed Size.
 *
 *  - If Compressed Size was present in Block Header, the new value
 *    calculated from Unpadded Size is compared against the value
 *    from Block Header.
 *
 * \note        This function must be called _after_ decoding the Block Header
 *              field so that it can properly validate Compressed Size if it
 *              was present in Block Header.
 *
 * \return      - LZMA_OK: block->compressed_size was set successfully.
 *              - LZMA_DATA_ERROR: unpadded_size is too small compared to
 *                block->header_size and lzma_check_size(block->check).
 *              - LZMA_PROG_ERROR: Some values are invalid. For example,
 *                block->header_size must be a multiple of four and
 *                between 8 and 1024 inclusive.
 */
extern LZMA_API(lzma_ret) lzma_block_compressed_size(
		lzma_block *block, lzma_vli unpadded_size)
		lzma_nothrow lzma_attr_warn_unused_result;


/**
 * \brief       Calculate Unpadded Size
 *
 * The Index field stores Unpadded Size and Uncompressed Size. The latter
 * can be taken directly from the lzma_block structure after coding a Block,
 * but Unpadded Size needs to be calculated from Block Header Size,
 * Compressed Size, and size of the Check field. This is where this function
 * is needed.
 *
 * \return      Unpadded Size on success, or zero on error.
 */
extern LZMA_API(lzma_vli) lzma_block_unpadded_size(const lzma_block *block)
		lzma_nothrow lzma_attr_pure;


/**
 * \brief       Calculate the total encoded size of a Block
 *
 * This is equivalent to lzma_block_unpadded_size() except that the returned
 * value includes the size of the Block Padding field.
 *
 * \return      On success, total encoded size of the Block. On error,
 *              zero is returned.
 */
extern LZMA_API(lzma_vli) lzma_block_total_size(const lzma_block *block)
		lzma_nothrow lzma_attr_pure;


/**
 * \brief       Initialize .xz Block encoder
 *
 * Valid actions for lzma_code() are LZMA_RUN, LZMA_SYNC_FLUSH (only if the
 * filter chain supports it), and LZMA_FINISH.
 *
 * \return      - LZMA_OK: All good, continue with lzma_code().
 *              - LZMA_MEM_ERROR
 *              - LZMA_OPTIONS_ERROR
 *              - LZMA_UNSUPPORTED_CHECK: block->check specfies a Check ID
 *                that is not supported by this buid of liblzma. Initializing
 *                the encoder failed.
 *              - LZMA_PROG_ERROR
 */
extern LZMA_API(lzma_ret) lzma_block_encoder(
		lzma_stream *strm, lzma_block *block)
		lzma_nothrow lzma_attr_warn_unused_result;


/**
 * \brief       Initialize .xz Block decoder
 *
 * Valid actions for lzma_code() are LZMA_RUN and LZMA_FINISH. Using
 * LZMA_FINISH is not required. It is supported only for convenience.
 *
 * \return      - LZMA_OK: All good, continue with lzma_code().
 *              - LZMA_UNSUPPORTED_CHECK: Initialization was successful, but
 *                the given Check ID is not supported, thus Check will be
 *                ignored.
 *              - LZMA_PROG_ERROR
 *              - LZMA_MEM_ERROR
 */
extern LZMA_API(lzma_ret) lzma_block_decoder(
		lzma_stream *strm, lzma_block *block)
		lzma_nothrow lzma_attr_warn_unused_result;


/**
 * \brief       Calculate maximum output size for single-call Block encoding
 *
 * This is equivalent to lzma_stream_buffer_bound() but for .xz Blocks.
 * See the documentation of lzma_stream_buffer_bound().
 */
extern LZMA_API(size_t) lzma_block_buffer_bound(size_t uncompressed_size)
		lzma_nothrow;


/**
 * \brief       Single-call .xz Block encoder
 *
 * In contrast to the multi-call encoder initialized with
 * lzma_block_encoder(), this function encodes also the Block Header. This
 * is required to make it possible to write appropriate Block Header also
 * in case the data isn't compressible, and different filter chain has to be
 * used to encode the data in uncompressed form using uncompressed chunks
 * of the LZMA2 filter.
 *
 * When the data isn't compressible, header_size, compressed_size, and
 * uncompressed_size are set just like when the data was compressible, but
 * it is possible that header_size is too small to hold the filter chain
 * specified in block->filters, because that isn't necessarily the filter
 * chain that was actually used to encode the data. lzma_block_unpadded_size()
 * still works normally, because it doesn't read the filters array.
 *
 * \param       block       Block options: block->version, block->check,
 *                          and block->filters must have been initialized.
 * \param       allocator   lzma_allocator for custom allocator functions.
 *                          Set to NULL to use malloc() and free().
 * \param       in          Beginning of the input buffer
 * \param       in_size     Size of the input buffer
 * \param       out         Beginning of the output buffer
 * \param       out_pos     The next byte will be written to out[*out_pos].
 *                          *out_pos is updated only if encoding succeeds.
 * \param       out_size    Size of the out buffer; the first byte into
 *                          which no data is written to is out[out_size].
 *
 * \return      - LZMA_OK: Encoding was successful.
 *              - LZMA_BUF_ERROR: Not enough output buffer space.
 *              - LZMA_OPTIONS_ERROR
 *              - LZMA_MEM_ERROR
 *              - LZMA_DATA_ERROR
 *              - LZMA_PROG_ERROR
 */
extern LZMA_API(lzma_ret) lzma_block_buffer_encode(
		lzma_block *block, lzma_allocator *allocator,
		const uint8_t *in, size_t in_size,
		uint8_t *out, size_t *out_pos, size_t out_size)
		lzma_nothrow lzma_attr_warn_unused_result;


/**
 * \brief       Single-call .xz Block decoder
 *
 * This is single-call equivalent of lzma_block_decoder(), and requires that
 * the caller has already decoded Block Header and checked its memory usage.
 *
 * \param       block       Block options just like with lzma_block_decoder().
 * \param       allocator   lzma_allocator for custom allocator functions.
 *                          Set to NULL to use malloc() and free().
 * \param       in          Beginning of the input buffer
 * \param       in_pos      The next byte will be read from in[*in_pos].
 *                          *in_pos is updated only if decoding succeeds.
 * \param       in_size     Size of the input buffer; the first byte that
 *                          won't be read is in[in_size].
 * \param       out         Beginning of the output buffer
 * \param       out_pos     The next byte will be written to out[*out_pos].
 *                          *out_pos is updated only if encoding succeeds.
 * \param       out_size    Size of the out buffer; the first byte into
 *                          which no data is written to is out[out_size].
 *
 * \return      - LZMA_OK: Decoding was successful.
 *              - LZMA_OPTIONS_ERROR
 *              - LZMA_DATA_ERROR
 *              - LZMA_MEM_ERROR
 *              - LZMA_BUF_ERROR: Output buffer was too small.
 *              - LZMA_PROG_ERROR
 */
extern LZMA_API(lzma_ret) lzma_block_buffer_decode(
		lzma_block *block, lzma_allocator *allocator,
		const uint8_t *in, size_t *in_pos, size_t in_size,
		uint8_t *out, size_t *out_pos, size_t out_size)
		lzma_nothrow;
