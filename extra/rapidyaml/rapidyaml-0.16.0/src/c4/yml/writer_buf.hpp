#ifndef C4_YML_WRITER_BUF_HPP_
#define C4_YML_WRITER_BUF_HPP_

#ifndef C4_YML_ERROR_HPP_
#include "c4/yml/error.hpp"
#endif

#include <string.h> // memcpy(), memset()

namespace c4 {
namespace yml {

C4_SUPPRESS_WARNING_MSVC_WITH_PUSH(4251) // substr needs to have dll-interface to be used by clients of WriterBuf

/** A writer to a memory buffer, in the form of a @ref substr . No
 * overflow occurs; the buffer size is strictly respected.
 *
 * @ingroup doc_writers
 * @ingroup doc_emit_to_buffer */
struct RYML_EXPORT WriterBuf
{
    substr m_buf;
    size_t m_pos;

    WriterBuf(substr sp) noexcept : m_buf(sp), m_pos(0) {}

public:

    /** Return the buffer written so far, or optionally throw an error
     * if the buffer was too small.  */
    substr get_result(bool error_on_excess) const
    {
        if(m_pos <= m_buf.len)
        {
            return m_buf.first(m_pos);
        }
        else if(!error_on_excess)
        {
            substr sp;
            sp.str = nullptr;
            sp.len = m_pos;
            return sp;
        }
        else
        {
            RYML_ERR_BASIC_("not enough space in the given buffer");
        }
    }

public:

    template<size_t N>
    C4_ALWAYS_INLINE void append(const char (&a)[N]) noexcept
    {
        static_assert(N > 1, "empty string");
        RYML_ASSERT_BASIC_( ! m_buf.overlaps(a));
        if(m_pos + N-1 <= m_buf.len)
            memcpy(m_buf.str + m_pos, a, N-1);
        m_pos += N-1;
    }

    C4_ALWAYS_INLINE void append(csubstr s) noexcept
    {
        RYML_ASSERT_BASIC_( ! s.overlaps(m_buf));
        if(s.len && m_pos + s.len <= m_buf.len)
            memcpy(m_buf.str + m_pos, s.str, s.len);
        m_pos += s.len;
    }

    C4_ALWAYS_INLINE void append(const char c) noexcept
    {
        if(m_pos + 1 <= m_buf.len)
            m_buf.str[m_pos] = c;
        ++m_pos;
    }

    C4_ALWAYS_INLINE void append(const char c, size_t num_times) noexcept
    {
        if(m_pos + num_times <= m_buf.len)
            memset(m_buf.str + m_pos, c, num_times);
        m_pos += num_times;
    }
};

C4_SUPPRESS_WARNING_MSVC_POP

} // namespace yml
} // namespace c4

#endif /* C4_YML_WRITER_BUF_HPP_ */
