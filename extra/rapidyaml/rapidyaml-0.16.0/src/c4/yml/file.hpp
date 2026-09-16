#ifndef C4_YML_FILE_HPP_
#define C4_YML_FILE_HPP_

/** @name file.hpp Helpers to read/write files from disk */


#ifndef C4_YML_ERROR_HPP_
#include <c4/yml/error.hpp>
#endif
#include <stddef.h>
#include <stdio.h>


namespace c4 {
namespace yml {

C4_SUPPRESS_WARNING_MSVC_WITH_PUSH(4996) // fopen: this function may be unsafe
C4_SUPPRESS_WARNING_CLANG_WITH_PUSH("-Wdeprecated-declarations") // fopen is deprecated

/** @cond dev */
namespace detail {
struct ScopedFILE
{
    FILE *file;
    inline ScopedFILE(const char *filename, const char *access) // NOLINT
    {
        file = std::fopen(filename, access); // NOLINT
        if(file == nullptr)
            RYML_ERR_BASIC_("{}: could not open file", filename);
    }
    inline ~ScopedFILE() noexcept // NOLINT
    {
        std::fclose(file); // NOLINT
    }
    ScopedFILE(const ScopedFILE&) = delete;
    ScopedFILE(      ScopedFILE&&) = delete;
    ScopedFILE& operator=(const ScopedFILE&) = delete;
    ScopedFILE& operator=(      ScopedFILE&&) = delete;
};
} // detail
/** @endcond */


//-----------------------------------------------------------------------------

/** @defgroup doc_file_put_contents file_put_contents()
 *
 * Save a buffer to disk
 *
 * @ingroup doc_file_utils
 *
 * @addtogroup doc_file_put_contents
 * @{
 */

/** save a contiguous buffer into a file */
inline void file_put_contents(void const* buf, size_t sz, FILE *file, const char* filename=nullptr)
{
    size_t written = std::fwrite(buf, 1, sz, file); // NOLINT
    if C4_UNLIKELY(written != sz)
        RYML_ERR_BASIC_("{}: failed file write: expected={}B actual={}B", filename ? filename : "file", sz, written); // LCOV_EXCL_LINE
}

/** save a contiguous buffer into a file */
template<class ContiguousContainer>
void file_put_contents(ContiguousContainer const& v, FILE *file, const char *filename=nullptr)
{
    size_t vsz = static_cast<size_t>(v.size()) * sizeof(typename ContiguousContainer::value_type);
    void const* vbuf = v.empty() ? nullptr : &v[0];
    file_put_contents(vbuf, vsz, file, filename);
}

/** save a contiguous buffer into a file */
inline void file_put_contents(void const* buf, size_t sz, const char *filename, const char* access="wb")
{
    detail::ScopedFILE f(filename, access);
    file_put_contents(buf, sz, f.file, filename);
}

/** save a contiguous buffer into a file */
template<class ContiguousContainer>
void file_put_contents(ContiguousContainer const& v, const char *filename, const char* access="wb")
{
    size_t vsz = static_cast<size_t>(v.size()) * sizeof(typename ContiguousContainer::value_type);
    void const* vbuf = v.empty() ? nullptr : &v[0];
    file_put_contents(vbuf, vsz, filename, access);
}

/** @} */


//-----------------------------------------------------------------------------

/** @defgroup doc_file_get_contents file_get_contents()
 *
 * Load a file from disk into a buffer.
 *
 * @ingroup doc_file_utils
 *
 * @addtogroup doc_file_get_contents
 * @{
 */

/** load a file of specified size from disk into an existing contiguous buffer.
 */
inline void file_get_contents(const char *filename, FILE *fp, size_t filesz, void *buf, size_t bufsz)
{
    RYML_ASSERT_BASIC_(filesz <= bufsz);(void)bufsz;
    size_t read = std::fread(buf, 1, filesz, fp); // NOLINT(clang-analyzer-unix.Errno)
    if C4_UNLIKELY(read != filesz)
        RYML_ERR_BASIC_("{}: failed file read: expected={}B actual={}B", filename, filesz, read); // LCOV_EXCL_LINE
}


/** load a file from disk into an existing contiguous buffer.
 *
 * @return true if the file was successfully read and the buffer was
 * large enough to fit the file size */
C4_NODISCARD inline size_t file_get_contents(const char *filename, FILE *fp, void *buf, size_t bufsz)
{
    std::fseek(fp, 0, SEEK_END); // NOLINT
    size_t filesz = static_cast<size_t>(std::ftell(fp)); // NOLINT
    std::rewind(fp); // NOLINT
    if(filesz <= bufsz)
        file_get_contents(filename, fp, filesz, buf, bufsz);
    return filesz;
}


/** load a file from disk into an existing contiguous buffer.
 *
 * @return the size required for the buffer. It is up to the caller to
 * check that the returned size is smaller than the buffer's size.
 */
C4_NODISCARD inline size_t file_get_contents(const char *filename, void *buf, size_t bufsz, const char *access="rb")
{
    detail::ScopedFILE f(filename, access);
    return file_get_contents(filename, f.file, buf, bufsz);
}


/** load a file from disk into an existing ContiguousContainer,
 * resizing it to fit the file's contents */
template<class ContiguousContainer>
void file_get_contents(ContiguousContainer *v, const char *filename, const char *access="rb")
{
    using value_type = typename ContiguousContainer::value_type;
    using size_type = typename ContiguousContainer::size_type;
    detail::ScopedFILE f(filename, access);
    void * dat = !v->empty() ? &(*v)[0] : nullptr;
    size_t vsz = static_cast<size_t>(v->size());
    size_t fsz = file_get_contents(filename, f.file, dat, vsz);
    size_t num_elms = fsz / sizeof(value_type);
    if C4_UNLIKELY(fsz != num_elms * sizeof(value_type))
        RYML_ERR_BASIC_("{}: file size ({}B) not a multiple of element size ({}B)", filename, fsz, sizeof(value_type));
    v->resize(static_cast<size_type>(num_elms));
    if(fsz > vsz * sizeof(value_type))
        file_get_contents(filename, f.file, fsz, &(*v)[0], fsz);
}


/** load a file from disk and return a newly created
 * ContiguousContainer with the file contents */
template<class ContiguousContainer>
ContiguousContainer file_get_contents(const char *filename, const char *access="rb")
{
    ContiguousContainer cc;
    file_get_contents(&cc, filename, access);
    return cc;
}


/** @} */



//-----------------------------------------------------------------------------

/** @defgroup doc_stdin_get_contents stdin_get_contents()
 *
 * Load a file from stdin (or similar stream-like file) into a buffer.
 *
 * @ingroup doc_file_utils
 *
 * @addtogroup doc_stdin_get_contents
 * @{
 */

/** load a file from stdin (or similar stream-like file) and
 * return a newly created ContiguousContainer with the file
 * contents */
template<class ContiguousContainer>
void stdin_get_contents(ContiguousContainer *cont, FILE *f=stdin)
{
    using I = typename ContiguousContainer::size_type;
    using T = typename ContiguousContainer::value_type;
    int c;
    I pos = 0;
    I num_bytes = 128;
    I elmsz = static_cast<I>(sizeof(T));
    I sz = (num_bytes + elmsz - 1) / elmsz; // round up to next multiple of elmsz
    num_bytes = sz * elmsz;
    cont->resize(sz);
    unsigned char *buf = reinterpret_cast<unsigned char*>(&(*cont)[0]); // NOLINT
    while((c = fgetc(f)) != EOF)
    {
        if(pos == num_bytes)
        {
            num_bytes = 2 * num_bytes;
            if(num_bytes % sizeof(T)) goto errsize; // NOLINT // LCOV_EXCL_LINE
            cont->resize(num_bytes / sizeof(T));
            buf = reinterpret_cast<unsigned char*>(&(*cont)[0]); // NOLINT
        }
        buf[pos++] = static_cast<unsigned char>(c);
    }
    if(pos % sizeof(T))
        goto errsize; // NOLINT
    cont->resize(pos / sizeof(T));
    return;
errsize:
    RYML_ERR_BASIC_("file size is not multiple of element size");
}

/** load a file from stdin and return a newly created
 * ContiguousContainer with the file contents */
template<class ContiguousContainer>
ContiguousContainer stdin_get_contents(FILE *f=stdin)
{
    ContiguousContainer cc;
    stdin_get_contents(&cc, f);
    return cc;
}

/** @} */

C4_SUPPRESS_WARNING_CLANG_POP
C4_SUPPRESS_WARNING_MSVC_POP


} // namespace yml
} // namespace c4

#endif /* C4_YML_FILE_HPP_ */
