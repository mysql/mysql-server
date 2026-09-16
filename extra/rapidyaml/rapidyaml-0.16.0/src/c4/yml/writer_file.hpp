#ifndef C4_YML_WRITER_FILE_HPP_
#define C4_YML_WRITER_FILE_HPP_

/** @file writer_file.hpp */

#ifndef C4_YML_ERROR_HPP_
#include "c4/yml/error.hpp"
#endif

#include <stdio.h>  // fwrite(), fputc()

namespace c4 {
namespace yml {

/** A writer that outputs to a C file handle, defaulting to
 * stdout. This writer is *much* faster than @ref WriterOStream and
 * should be preferred to it.
 * @ingroup doc_writers
 * @ingroup doc_emit_to_file
 */
struct RYML_EXPORT WriterFile
{
    FILE * m_file;

    WriterFile(FILE *f = nullptr) noexcept : m_file(f ? f : stdout) {}

    template<size_t N>
    C4_ALWAYS_INLINE void append(const char (&a)[N]) noexcept // NOLINT(*-make-member-function-const)
    {
        static_assert(N > 1, "empty string");
        (void)fwrite(a, sizeof(char), N - 1, m_file);
    }

    C4_ALWAYS_INLINE void append(csubstr s) noexcept // NOLINT(*-make-member-function-const)
    {
        if(s.len)
        {
            C4_SUPPRESS_WARNING_GCC_CLANG_WITH_PUSH("-Wsign-conversion")
            (void)fwrite(s.str, sizeof(csubstr::char_type), s.len, m_file);
            C4_SUPPRESS_WARNING_GCC_CLANG_POP
        }
    }

    C4_ALWAYS_INLINE void append(const char c) noexcept // NOLINT(*-make-member-function-const)
    {
        (void)fputc(c, m_file);
    }

    C4_ALWAYS_INLINE void append(const char c, size_t num_times) noexcept // NOLINT(*-make-member-function-const)
    {
        for(size_t i = 0; i < num_times; ++i)
            (void)fputc(c, m_file);
    }
};

} // namespace yml
} // namespace c4

#endif /* C4_YML_WRITER_FILE_HPP_ */
