#ifndef C4_BASE64_HPP_
#define C4_BASE64_HPP_

/** @file base64.hpp encoding/decoding for base64.
 * @see https://en.wikipedia.org/wiki/Base64
 * @see https://www.base64encode.org/
 * */

#ifndef C4_EXPORT_HPP_
#include "c4/export.hpp"
#endif
#include <stddef.h>

namespace c4 {

/** @defgroup doc_base64 Base64 encoding/decoding
 * @see https://en.wikipedia.org/wiki/Base64
 * @see https://www.base64encode.org/
 * @{ */


/** check that the given buffer is a valid base64 encoding
 * @see https://en.wikipedia.org/wiki/Base64 */
C4CORE_EXPORT bool base64_valid(const char* encoded, size_t encoded_sz);


/** base64-encode binary data. This is a plain implementation with a
 * focus on simplicity and small footprint, such that it runs
 * reasonably well in constrained platforms. On larger platforms it is
 * reasonably fast (reaching 3GB/s and over), but it is not the
 * fastest. If ultimate base64 speed in x64 platforms is your
 * objective, there are faster implementations available. One
 * recommendation is https://github.com/aklomp/base64, which uses a
 * larger Look-Up Table (4096B as compared with 64B in c4core), making
 * it between 1.5x~2x faster than c4core for larger payloads (but also
 * slower for small payloads), and much faster when using AVX2 or
 * AVX512 processing.  But this speed comes at a cost in constrained
 * platforms: eg c4core encodes ~2.5x faster in armv4 and armv5.
 *
 * @param encoded [out] output buffer for encoded data
 *
 * @param encoded_sz [in] size of the output buffer for encoded data
 *
 * @param data [in] the input buffer with the binary data
 *
 * @param data_sz [in] size of the input buffer with the binary data
 *
 * @return the number of bytes required for the output buffer. No
 *         writes occur beyond the end of the output buffer, so it is
 *         safe to do a speculative call where the encoded buffer is
 *         empty, or maybe too small. The caller should ensure that
 *         the returned size is smaller than the size of the encoded
 *         buffer.
 *
 * @note the result depends on endianness. If transfer between
 *       little/big endian systems is desired, the caller should
 *       normalize @p data before encoding.
 *
 * @see https://en.wikipedia.org/wiki/Base64 */
C4CORE_EXPORT size_t base64_encode(char *encoded, size_t encoded_sz,
                                   void const* data, size_t data_sz);


/** decode the base64 encoding in the given buffer. This is a plain
 * implementation with a focus on simplicity and small footprint, such
 * that it runs reasonably well in constrained platforms. On larger
 * platforms it is reasonably fast, but it is not the fastest. If
 * ultimate base64 speed in x64 platforms is your objective, there are
 * faster implementations available. One recommendation is
 * https://github.com/aklomp/base64, which uses up to 16x larger
 * Look-Up Tables, making it between 1.5x~2x faster than c4core (but
 * also slower for small payloads), and much faster when using AVX2 or
 * AVX512 processing. But this x64 speed comes at a cost in
 * constrained platforms: eg c4core decodes ~4x faster in armv4 and
 * armv5.
 *
 * @param encoded [in] the encoded base64
 *
 * @param encoded_sz [in] the size of the encoded buffer
 *
 * @param data [out] the output decoded buffer
 *
 * @param data_sz [in] the size of the output decoded buffer
 *
 * @param data_sz_required [out] the size required for the output
 *        decoded buffer, ie, the number of bytes needed to return the
 *        output (ie the required size for @p data). No writes occur
 *        beyond the end of the output buffer, so it is safe to do a
 *        speculative call where the data buffer is empty, or maybe
 *        too small. The caller should ensure that this value
 *        is smaller than data_sz.
 *
 * @return false if the encoding was invalid or the data size was
 *         too small, and true otherwise.
 *
 * @note the result depends on endianness. If transfer between
 *       little/big endian systems is desired, the caller should
 *       normalize @p data after decoding.
 *
 * @see https://en.wikipedia.org/wiki/Base64 */
C4CORE_EXPORT bool base64_decode(char const* encoded, size_t encoded_sz,
                                 void * data, size_t data_sz,
                                 size_t *data_sz_required);

/** @} */ // base64

} // namespace c4

#endif /* C4_BASE64_HPP_ */
