/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
/**
 * \file        lzma/filter.h
 * \brief       Common filter related types
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
 * \brief       Maximum number of filters in a chain
 *
 * A filter chain can have 1-4 filters, of which three are allowed to change
 * the size of the data. Usually only one or two filters are needed.
 */
#define LZMA_FILTERS_MAX 4


/**
 * \brief       Filter options
 *
 * This structure is used to pass Filter ID and a pointer filter's
 * options to liblzma. A few functions work with a single lzma_filter
 * structure, while most functions expect a filter chain.
 *
 * A filter chain is indicated with an array of lzma_filter structures.
 * The array is terminated with .id = LZMA_VLI_UNKNOWN. Thus, the filter
 * array must have LZMA_FILTERS_MAX + 1 elements (that is, five) to
 * be able to hold any arbitrary filter chain. This is important when
 * using lzma_block_header_decode() from block.h, because too small
 * array would make liblzma write past the end of the filters array.
 */
typedef struct {
	/**
	 * \brief       Filter ID
	 *
	 * Use constants whose name begin with `LZMA_FILTER_' to specify
	 * different filters. In an array of lzma_filter structures, use
	 * LZMA_VLI_UNKNOWN to indicate end of filters.
	 *
	 * \note        This is not an enum, because on some systems enums
	 *              cannot be 64-bit.
	 */
	lzma_vli id;

	/**
	 * \brief       Pointer to filter-specific options structure
	 *
	 * If the filter doesn't need options, set this to NULL. If id is
	 * set to LZMA_VLI_UNKNOWN, options is ignored, and thus
	 * doesn't need be initialized.
	 *
	 * Some filters support changing the options in the middle of
	 * the encoding process. These filters store the pointer of the
	 * options structure and communicate with the application via
	 * modifications of the options structure.
	 */
	void *options;

} lzma_filter;


/**
 * \brief       Test if the given Filter ID is supported for encoding
 *
 * Return true if the give Filter ID is supported for encoding by this
 * liblzma build. Otherwise false is returned.
 *
 * There is no way to list which filters are available in this particular
 * liblzma version and build. It would be useless, because the application
 * couldn't know what kind of options the filter would need.
 */
extern LZMA_API(lzma_bool) lzma_filter_encoder_is_supported(lzma_vli id)
		lzma_nothrow lzma_attr_const;


/**
 * \brief       Test if the given Filter ID is supported for decoding
 *
 * Return true if the give Filter ID is supported for decoding by this
 * liblzma build. Otherwise false is returned.
 */
extern LZMA_API(lzma_bool) lzma_filter_decoder_is_supported(lzma_vli id)
		lzma_nothrow lzma_attr_const;


/**
 * \brief       Calculate rough memory requirements for raw encoder
 *
 * Because the calculation is rough, this function can be used to calculate
 * the memory requirements for Block and Stream encoders too.
 *
 * \param       filters     Array of filters terminated with
 *                          .id == LZMA_VLI_UNKNOWN.
 *
 * \return      Rough number of bytes of memory required for the given
 *              filter chain when encoding.
 */
extern LZMA_API(uint64_t) lzma_raw_encoder_memusage(const lzma_filter *filters)
		lzma_nothrow lzma_attr_pure;


/**
 * \brief       Calculate rough memory requirements for raw decoder
 *
 * Because the calculation is rough, this function can be used to calculate
 * the memory requirements for Block and Stream decoders too.
 *
 * \param       filters     Array of filters terminated with
 *                          .id == LZMA_VLI_UNKNOWN.
 *
 * \return      Rough number of bytes of memory required for the given
 *              filter chain when decoding.
 */
extern LZMA_API(uint64_t) lzma_raw_decoder_memusage(const lzma_filter *filters)
		lzma_nothrow lzma_attr_pure;


/**
 * \brief       Initialize raw encoder
 *
 * This function may be useful when implementing custom file formats.
 *
 * \param       strm    Pointer to properly prepared lzma_stream
 * \param       filters Array of lzma_filter structures. The end of the
 *                      array must be marked with .id = LZMA_VLI_UNKNOWN.
 *
 * The `action' with lzma_code() can be LZMA_RUN, LZMA_SYNC_FLUSH (if the
 * filter chain supports it), or LZMA_FINISH.
 *
 * \return      - LZMA_OK
 *              - LZMA_MEM_ERROR
 *              - LZMA_OPTIONS_ERROR
 *              - LZMA_PROG_ERROR
 */
extern LZMA_API(lzma_ret) lzma_raw_encoder(
		lzma_stream *strm, const lzma_filter *filters)
		lzma_nothrow lzma_attr_warn_unused_result;


/**
 * \brief       Initialize raw decoder
 *
 * The initialization of raw decoder goes similarly to raw encoder.
 *
 * The `action' with lzma_code() can be LZMA_RUN or LZMA_FINISH. Using
 * LZMA_FINISH is not required, it is supported just for convenience.
 *
 * \return      - LZMA_OK
 *              - LZMA_MEM_ERROR
 *              - LZMA_OPTIONS_ERROR
 *              - LZMA_PROG_ERROR
 */
extern LZMA_API(lzma_ret) lzma_raw_decoder(
		lzma_stream *strm, const lzma_filter *filters)
		lzma_nothrow lzma_attr_warn_unused_result;


/**
 * \brief       Single-call raw encoder
 *
 * \param       filters     Array of lzma_filter structures. The end of the
 *                          array must be marked with .id = LZMA_VLI_UNKNOWN.
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
 *
 * \note        There is no function to calculate how big output buffer
 *              would surely be big enough. (lzma_stream_buffer_bound()
 *              works only for lzma_stream_buffer_encode().)
 */
extern LZMA_API(lzma_ret) lzma_raw_buffer_encode(
		const lzma_filter *filters, lzma_allocator *allocator,
		const uint8_t *in, size_t in_size, uint8_t *out,
		size_t *out_pos, size_t out_size) lzma_nothrow;


/**
 * \brief       Single-call raw decoder
 *
 * \param       filters     Array of lzma_filter structures. The end of the
 *                          array must be marked with .id = LZMA_VLI_UNKNOWN.
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
 */
extern LZMA_API(lzma_ret) lzma_raw_buffer_decode(
		const lzma_filter *filters, lzma_allocator *allocator,
		const uint8_t *in, size_t *in_pos, size_t in_size,
		uint8_t *out, size_t *out_pos, size_t out_size) lzma_nothrow;


/**
 * \brief       Get the size of the Filter Properties field
 *
 * This function may be useful when implementing custom file formats
 * using the raw encoder and decoder.
 *
 * \param       size    Pointer to uint32_t to hold the size of the properties
 * \param       filter  Filter ID and options (the size of the propeties may
 *                      vary depending on the options)
 *
 * \return      - LZMA_OK
 *              - LZMA_OPTIONS_ERROR
 *              - LZMA_PROG_ERROR
 *
 * \note        This function validates the Filter ID, but does not
 *              necessarily validate the options. Thus, it is possible
 *              that this returns LZMA_OK while the following call to
 *              lzma_properties_encode() returns LZMA_OPTIONS_ERROR.
 */
extern LZMA_API(lzma_ret) lzma_properties_size(
		uint32_t *size, const lzma_filter *filter) lzma_nothrow;


/**
 * \brief       Encode the Filter Properties field
 *
 * \param       filter  Filter ID and options
 * \param       props   Buffer to hold the encoded options. The size of
 *                      buffer must have been already determined with
 *                      lzma_properties_size().
 *
 * \return      - LZMA_OK
 *              - LZMA_OPTIONS_ERROR
 *              - LZMA_PROG_ERROR
 *
 * \note        Even this function won't validate more options than actually
 *              necessary. Thus, it is possible that encoding the properties
 *              succeeds but using the same options to initialize the encoder
 *              will fail.
 *
 * \note        It is OK to skip calling this function if
 *              lzma_properties_size() indicated that the size
 *              of the Filter Properties field is zero.
 */
extern LZMA_API(lzma_ret) lzma_properties_encode(
		const lzma_filter *filter, uint8_t *props) lzma_nothrow;


/**
 * \brief       Decode the Filter Properties field
 *
 * \param       filter      filter->id must have been set to the correct
 *                          Filter ID. filter->options doesn't need to be
 *                          initialized (it's not freed by this function). The
 *                          decoded options will be stored to filter->options.
 *                          filter->options is set to NULL if there are no
 *                          properties or if an error occurs.
 * \param       allocator   Custom memory allocator used to allocate the
 *                          options. Set to NULL to use the default malloc(),
 *                          and in case of an error, also free().
 * \param       props       Input buffer containing the properties.
 * \param       props_size  Size of the properties. This must be the exact
 *                          size; giving too much or too little input will
 *                          return LZMA_OPTIONS_ERROR.
 *
 * \return      - LZMA_OK
 *              - LZMA_OPTIONS_ERROR
 *              - LZMA_MEM_ERROR
 */
extern LZMA_API(lzma_ret) lzma_properties_decode(
		lzma_filter *filter, lzma_allocator *allocator,
		const uint8_t *props, size_t props_size) lzma_nothrow;


/**
 * \brief       Calculate encoded size of a Filter Flags field
 *
 * Knowing the size of Filter Flags is useful to know when allocating
 * memory to hold the encoded Filter Flags.
 *
 * \param       size    Pointer to integer to hold the calculated size
 * \param       filters Filter ID and associated options whose encoded
 *                      size is to be calculted
 *
 * \return      - LZMA_OK: *size set successfully. Note that this doesn't
 *                guarantee that filters->options is valid, thus
 *                lzma_filter_flags_encode() may still fail.
 *              - LZMA_OPTIONS_ERROR: Unknown Filter ID or unsupported options.
 *              - LZMA_PROG_ERROR: Invalid options
 *
 * \note        If you need to calculate size of List of Filter Flags,
 *              you need to loop over every lzma_filter entry.
 */
extern LZMA_API(lzma_ret) lzma_filter_flags_size(
		uint32_t *size, const lzma_filter *filters)
		lzma_nothrow lzma_attr_warn_unused_result;


/**
 * \brief       Encode Filter Flags into given buffer
 *
 * In contrast to some functions, this doesn't allocate the needed buffer.
 * This is due to how this function is used internally by liblzma.
 *
 * \param       filters     Filter ID and options to be encoded
 * \param       out         Beginning of the output buffer
 * \param       out_pos     out[*out_pos] is the next write position. This
 *                          is updated by the encoder.
 * \param       out_size    out[out_size] is the first byte to not write.
 *
 * \return      - LZMA_OK: Encoding was successful.
 *              - LZMA_OPTIONS_ERROR: Invalid or unsupported options.
 *              - LZMA_PROG_ERROR: Invalid options or not enough output
 *                buffer space (you should have checked it with
 *                lzma_filter_flags_size()).
 */
extern LZMA_API(lzma_ret) lzma_filter_flags_encode(const lzma_filter *filters,
		uint8_t *out, size_t *out_pos, size_t out_size)
		lzma_nothrow lzma_attr_warn_unused_result;


/**
 * \brief       Decode Filter Flags from given buffer
 *
 * The decoded result is stored into *filters. filters->options is
 * initialized but the old value is NOT free()d.
 *
 * \return      - LZMA_OK
 *              - LZMA_OPTIONS_ERROR
 *              - LZMA_MEM_ERROR
 *              - LZMA_PROG_ERROR
 */
extern LZMA_API(lzma_ret) lzma_filter_flags_decode(
		lzma_filter *filters, lzma_allocator *allocator,
		const uint8_t *in, size_t *in_pos, size_t in_size)
		lzma_nothrow lzma_attr_warn_unused_result;
