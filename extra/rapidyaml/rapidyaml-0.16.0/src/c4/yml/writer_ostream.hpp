#ifndef C4_YML_WRITER_OSTREAM_HPP_
#define C4_YML_WRITER_OSTREAM_HPP_

/** @file writer_ostream.hpp */

#ifndef C4_YML_ERROR_HPP_
#include "c4/yml/error.hpp"
#endif

namespace c4 {
namespace yml {

/** A writer that outputs to an STL-like ostream.
 *
 * @ingroup doc_writers
 * @ingroup doc_emit_to_ostream
 */
template<class OStream>
struct WriterOStream
{
    OStream* m_stream;

    WriterOStream(OStream *s) noexcept : m_stream(s) {}

    template<size_t N>
    C4_ALWAYS_INLINE void append(const char (&a)[N]) noexcept
    {
        static_assert(N > 1, "empty string");
        m_stream->write(a, N - 1);
    }

    C4_ALWAYS_INLINE void append(csubstr s) noexcept
    {
        if(s.len)
        {
            C4_SUPPRESS_WARNING_GCC_CLANG_WITH_PUSH("-Wsign-conversion")
            m_stream->write(s.str, s.len);
            C4_SUPPRESS_WARNING_GCC_CLANG_POP
        }
    }

    C4_ALWAYS_INLINE void append(const char c) noexcept
    {
        m_stream->put(c);
    }

    C4_ALWAYS_INLINE void append(const char c, size_t num_times) noexcept
    {
        for(size_t i = 0; i < num_times; ++i)
            m_stream->put(c);
    }
};

} // namespace yml
} // namespace c4

#endif /* C4_YML_WRITER_OSTREAM_HPP_ */
