/*****************************************************************************

Copyright (c) 2016, 2017, Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA

*****************************************************************************/
#ifndef lob0fit_h
#define lob0fit_h

#include <mem0mem.h>
#include <stdlib.h>
#include <sys/types.h>
#include <zlib.h>
#include <iostream>

#include "univ.i"

namespace lob {

/** The maximum input length per zlib stream. */
const uint MAX_INPUT_LEN_PER_STREAM = 128 * 1024;

/** The FitBlock class is used to fit compressed LOB data into a sequence
of LOB page frames. */
struct FitBlock {
  /** Default constructor. */
  FitBlock()
      : m_input(0), m_inlen(0), m_output(0), m_outlen(0), m_total_in(0),
        m_total_out(0), m_heap(nullptr), m_max_inlen(MAX_INPUT_LEN_PER_STREAM) {
  }

  /** Destructor. */
  ~FitBlock() {}

  /** Set the output buffer.  The length of this buffer must
  be less than or equal to the value set by setMaxOutputBufferSize().
  @param[in]	output	the output buffer.
  @param[in]	size	size of output buffer in bytes. */
  void setOutputBuffer(byte *output, uint size) {
    m_output = output;
    m_outlen = size;

    m_def.next_out = m_output;
    m_def.avail_out = m_outlen;
  }

  /** Set the input buffer.
  @param[in]	input	the input buffer.
  @param[in]	inlen	length of input buffer. */
  void setInputBuffer(byte *input, uint inlen) {
    m_input = input;
    m_inlen = inlen;

    m_total_in = 0;
    m_total_out = 0;

    deflateReset(&m_def);

    int use_len = (m_inlen > m_max_inlen ? m_max_inlen : m_inlen);

    m_def.next_in = m_input;
    m_def.avail_in = use_len;
  }

  /** Initialize the zlib streams.
  @param[in]	level	the compression level.
  @return 0 on success, -1 on failure. */
  int init(int level);

  /** Fit the given uncompressed data into the output buffer.*/
  void fit(byte *output, uint size);

  /** Get the number of uncompressed data bytes consumed.
  @return number of uncompressed data bytes consumed */
  uint getInputBytes() const { return (m_total_in); }

  /** Get the number of compressed data bytes written out.
  @return number of compressed data bytes written. */
  uint getOutputBytes() const { return (m_total_out); }

  /** Close the two zlib streams and free the two internal buffers. */
  void destroy();

private:
  /** Free internally allocated memory. */
  void free_mem();

  /** uncompressed input is available here. */
  byte *m_input;

  /** total uncompressed input length in bytes. */
  uint m_inlen;

  /** compressed output will be written here. */
  byte *m_output;

  /** the output buffer size */
  uint m_outlen;

  /* uncompressed bytes consumed from input buffer. */
  uint m_total_in;

  /* compressed bytes written into output buffer. */
  uint m_total_out;

  /** zlib stream for compression*/
  z_stream m_def;

  /** Memory is allocated from this heap. */
  mem_heap_t *m_heap;

  /** Maximum allowed input length. */
  uint m_max_inlen;
};

/** Uncompress the given zlib stream from a sequence of LOB page frames. */
struct UnfitBlock {
public:
  /** Default constructor */
  UnfitBlock() : m_total_out(0), m_heap(nullptr) {}

  /** Initialize the zlib streams.
  @return 0 on success, -1 on failure. */
  int init();

  /** Free the resources. */
  void destroy() {
    if (m_heap != nullptr) {
      mem_heap_free(m_heap);
      m_heap = nullptr;
    }
  }

  /** Set the output buffer for the zlib stream.
  @param[in]	output	the output buffer.
  @param[in]	size	size of buffer. */
  void setOutput(byte *output, uint size) {
    inflateReset(&m_inf);

    m_inf.next_out = output;
    m_inf.avail_out = size;

    m_output = output;
    m_outlen = size;
  }

  /** Decompress LOB data from the given input buffer.
  @param[in]	out	the input buffer.
  @param[in]	size	the buffer size.*/
  void unfit(byte *out, uint size);

  /** The zlib stream used for decompression. */
  z_stream m_inf;

  /** The total amount of uncompressed bytes read. */
  uint m_total_out;

  /** The length of the output buffer. */
  uint m_outlen;

  /** The output buffer. */
  byte *m_output;

private:
  /** The memory heap used by the zlib stream */
  mem_heap_t *m_heap;
};

} // namespace lob

#endif /* lob0fit_h */
