/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
/**
 * \file        lzma/subblock.h
 * \brief       Subblock filter
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
 * \brief       Filter ID
 *
 * Filter ID of the Subblock filter. This is used as lzma_filter.id.
 */
#define LZMA_FILTER_SUBBLOCK    LZMA_VLI_C(0x01)


/**
 * \brief       Subfilter mode
 *
 * See lzma_options_subblock.subfilter_mode for details.
 */
typedef enum {
	LZMA_SUBFILTER_NONE,
		/**<
		 * No Subfilter is in use.
		 */

	LZMA_SUBFILTER_SET,
		/**<
		 * New Subfilter has been requested to be initialized.
		 */

	LZMA_SUBFILTER_RUN,
		/**<
		 * Subfilter is active.
		 */

	LZMA_SUBFILTER_FINISH
		/**<
		 * Subfilter has been requested to be finished.
		 */
} lzma_subfilter_mode;


/**
 * \brief       Options for the Subblock filter
 *
 * Specifying options for the Subblock filter is optional: if the pointer
 * options is NULL, no subfilters are allowed and the default value is used
 * for subblock_data_size.
 */
typedef struct {
	/* Options for encoder and decoder */

	/**
	 * \brief       Allowing subfilters
	 *
	 * If this true, subfilters are allowed.
	 *
	 * In the encoder, if this is set to false, subfilter_mode and
	 * subfilter_options are completely ignored.
	 */
	lzma_bool allow_subfilters;

	/* Options for encoder only */

	/**
	 * \brief       Alignment
	 *
	 * The Subblock filter encapsulates the input data into Subblocks.
	 * Each Subblock has a header which takes a few bytes of space.
	 * When the output of the Subblock encoder is fed to another filter
	 * that takes advantage of the alignment of the input data (e.g. LZMA),
	 * the Subblock filter can add padding to keep the actual data parts
	 * in the Subblocks aligned correctly.
	 *
	 * The alignment should be a positive integer. Subblock filter will
	 * add enough padding between Subblocks so that this is true for
	 * every payload byte:
	 * input_offset % alignment == output_offset % alignment
	 *
	 * The Subblock filter assumes that the first output byte will be
	 * written to a position in the output stream that is properly
	 * aligned. This requirement is automatically met when the start
	 * offset of the Stream or Block is correctly told to Block or
	 * Stream encoder.
	 */
	uint32_t alignment;
#	define LZMA_SUBBLOCK_ALIGNMENT_MIN 1
#	define LZMA_SUBBLOCK_ALIGNMENT_MAX 32
#	define LZMA_SUBBLOCK_ALIGNMENT_DEFAULT 4

	/**
	 * \brief       Size of the Subblock Data part of each Subblock
	 *
	 * This value is re-read every time a new Subblock is started.
	 *
	 * Bigger values
	 *   - save a few bytes of space;
	 *   - increase latency in the encoder (but no effect for decoding);
	 *   - decrease memory locality (increased cache pollution) in the
	 *     encoder (no effect in decoding).
	 */
	uint32_t subblock_data_size;
#	define LZMA_SUBBLOCK_DATA_SIZE_MIN 1
#	define LZMA_SUBBLOCK_DATA_SIZE_MAX (UINT32_C(1) << 28)
#	define LZMA_SUBBLOCK_DATA_SIZE_DEFAULT 4096

	/**
	 * \brief       Run-length encoder remote control
	 *
	 * The Subblock filter has an internal run-length encoder (RLE). It
	 * can be useful when the data includes byte sequences that repeat
	 * very many times. The RLE can be used also when a Subfilter is
	 * in use; the RLE will be applied to the output of the Subfilter.
	 *
	 * Note that in contrast to traditional RLE, this RLE is intended to
	 * be used only when there's a lot of data to be repeated. If the
	 * input data has e.g. 500 bytes of NULs now and then, this RLE
	 * is probably useless, because plain LZMA should provide better
	 * results.
	 *
	 * Due to above reasons, it was decided to keep the implementation
	 * of the RLE very simple. When the rle variable is non-zero, it
	 * subblock_data_size must be a multiple of rle. Once the Subblock
	 * encoder has got subblock_data_size bytes of input, it will check
	 * if the whole buffer of the last subblock_data_size can be
	 * represented with repeats of chunks having size of rle bytes.
	 *
	 * If there are consecutive identical buffers of subblock_data_size
	 * bytes, they will be encoded using a single repeat entry if
	 * possible.
	 *
	 * If need arises, more advanced RLE can be implemented later
	 * without breaking API or ABI.
	 */
	uint32_t rle;
#	define LZMA_SUBBLOCK_RLE_OFF 0
#	define LZMA_SUBBLOCK_RLE_MIN 1
#	define LZMA_SUBBLOCK_RLE_MAX 256

	/**
	 * \brief       Subfilter remote control
	 *
	 * When the Subblock filter is initialized, this variable must be
	 * LZMA_SUBFILTER_NONE or LZMA_SUBFILTER_SET.
	 *
	 * When subfilter_mode is LZMA_SUBFILTER_NONE, the application may
	 * put Subfilter options to subfilter_options structure, and then
	 * set subfilter_mode to LZMA_SUBFILTER_SET. No new input data will
	 * be read until the Subfilter has been enabled. Once the Subfilter
	 * has been enabled, liblzma will set subfilter_mode to
	 * LZMA_SUBFILTER_RUN.
	 *
	 * When subfilter_mode is LZMA_SUBFILTER_RUN, the application may
	 * set subfilter_mode to LZMA_SUBFILTER_FINISH. All the input
	 * currently available will be encoded before unsetting the
	 * Subfilter. Application must not change the amount of available
	 * input until the Subfilter has finished. Once the Subfilter has
	 * finished, liblzma will set subfilter_mode to LZMA_SUBFILTER_NONE.
	 *
	 * If the intent is to have Subfilter enabled to the very end of
	 * the data, it is not needed to separately disable Subfilter with
	 * LZMA_SUBFILTER_FINISH. Using LZMA_FINISH as the second argument
	 * of lzma_code() will make the Subblock encoder to disable the
	 * Subfilter once all the data has been ran through the Subfilter.
	 *
	 * After the first call with LZMA_SYNC_FLUSH or LZMA_FINISH, the
	 * application must not change subfilter_mode until LZMA_STREAM_END.
	 * Setting LZMA_SUBFILTER_SET/LZMA_SUBFILTER_FINISH and
	 * LZMA_SYNC_FLUSH/LZMA_FINISH _at the same time_ is fine.
	 *
	 * \note        This variable is ignored if allow_subfilters is false.
	 */
	lzma_subfilter_mode subfilter_mode;

	/**
	 * \brief       Subfilter and its options
	 *
	 * When no Subfilter is used, the data is copied as is into Subblocks.
	 * Setting a Subfilter allows encoding some parts of the data with
	 * an additional filter. It is possible to many different Subfilters
	 * in the same Block, although only one can be used at once.
	 *
	 * \note        This variable is ignored if allow_subfilters is false.
	 */
	lzma_filter subfilter_options;

} lzma_options_subblock;
