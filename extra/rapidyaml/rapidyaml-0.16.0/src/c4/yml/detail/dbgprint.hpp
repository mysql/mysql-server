#ifndef C4_YML_DETAIL_DBGPRINT_HPP_
#define C4_YML_DETAIL_DBGPRINT_HPP_


//-----------------------------------------------------------------------------
// debug prints

#ifndef RYML_DBG
#   define _c4dbgt(fmt, ...)
#   define _c4dbgpf(fmt, ...)
#   define _c4dbgpf_(fmt, ...)
#   define _c4dbgp(msg)
#   define _c4dbgp_(msg)
#   define _c4dbgq(msg)
#   define _c4presc(...)
#   define _c4prscalar(msg, scalar, keep_newlines)
#else
#   define _c4dbgt(fmt, ...)   do {                                     \
                                   if(dbg_enabled_()) {                 \
                                       this->_dbg("{}:{}: "   fmt     , __FILE__, __LINE__, __VA_ARGS__); \
                                   }                                    \
                               } while(0)
#   define _c4dbgpf(fmt, ...)  dbg_printf_("{}:{}: "   fmt "\n", __FILE__, __LINE__, __VA_ARGS__)
#   define _c4dbgpf_(fmt, ...) dbg_printf_("{}:{}: "   fmt     , __FILE__, __LINE__, __VA_ARGS__)
#   define _c4dbgp(msg)        dbg_printf_("{}:{}: "   msg "\n", __FILE__, __LINE__             )
#   define _c4dbgp_(msg)       dbg_printf_("{}:{}: "   msg     , __FILE__, __LINE__             )
#   define _c4dbgq(msg)        dbg_printf_(msg "\n")
#   define _c4presc(...)       c4presc_(__VA_ARGS__)
#   define _c4prscalar(msg, scalar, keep_newlines)                  \
    do {                                                            \
        if(dbg_enabled_()) {                                        \
            _c4dbgpf_("{}: [{}]~~~", msg, scalar.len);              \
            c4presc_((scalar), (keep_newlines));                    \
            _c4dbgq("~~~");                                         \
        }                                                           \
    } while(0)


//-----------------------------------------------------------------------------
// implementation (only with RYML_DBG)

#include <cstdio>

#if defined(C4_MSVC) || defined(C4_MINGW)
#include <malloc.h>
#elif (defined(__clang__) && defined(_MSC_VER)) || \
      defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
#include <stdlib.h>
#else
#include <alloca.h>
#endif

#ifndef C4_YML_ESCAPE_SCALAR_HPP_
#include "c4/yml/escape_scalar.hpp"
#endif

#ifndef C4_DUMP_HPP_
#include "c4/dump.hpp"
#endif
#ifndef C4_FORMAT_HPP_
#include "c4/format.hpp"
#endif


C4_SUPPRESS_WARNING_GCC_WITH_PUSH("-Wattributes")

namespace c4 {
namespace yml {
inline bool& dbg_enabled_() { static bool enabled = true; return enabled; }
inline C4_NO_INLINE void dbg_set_enabled_(bool yes) { dbg_enabled_() = yes; }
inline C4_NO_INLINE void dbg_dumper_(csubstr s)
{
    RYML_ASSERT_BASIC_(s.str || !s.len);
    if(s.len)
        fwrite(s.str, 1, s.len, stdout);
}
template<class DumpFn, class ...Args>
C4_NO_INLINE void dbg_dump_(DumpFn &&dumpfn, csubstr fmt, Args&& ...args)
{
    DumpResults results;
    // try writing everything:
    {
        // buffer for converting individual arguments. it is defined
        // in a child scope to free it in case the buffer is too small
        // for any of the arguments.
        char writebuf[RYML_LOGBUF_SIZE];
        results = format_dump_resume(std::forward<DumpFn>(dumpfn), writebuf, fmt, std::forward<Args>(args)...);
    }
    // if any of the arguments failed to fit the buffer, allocate a
    // larger buffer (with alloca(), up to a limit) and resume writing.
    //
    // results.bufsize is set to the size of the largest element
    // serialized. Eg int(1) will require 1 byte.
    if(C4_UNLIKELY(results.bufsize > RYML_LOGBUF_SIZE))
    {
        const size_t bufsize = results.bufsize <= RYML_LOGBUF_SIZE_MAX ? results.bufsize : RYML_LOGBUF_SIZE_MAX;
        #ifdef C4_MSVC
        substr largerbuf = {static_cast<char*>(_alloca(bufsize)), bufsize};
        #else
        substr largerbuf = {static_cast<char*>(alloca(bufsize)), bufsize};
        #endif
        results = format_dump_resume(std::forward<DumpFn>(dumpfn), results, largerbuf, fmt, std::forward<Args>(args)...);
    }
}
template<class ...Args>
C4_NO_INLINE void dbg_printf_(csubstr fmt, Args const& ...args)
{
    if(dbg_enabled_())
        dbg_dump_(&dbg_dumper_, fmt, args...);
}
inline C4_NO_INLINE void c4presc_(csubstr s, bool keep_newlines=false)
{
    if(dbg_enabled_())
        escape_scalar_fn(dbg_dumper_, s, keep_newlines);
}
inline C4_NO_INLINE void c4presc_(const char *s, size_t len, bool keep_newlines=false)
{
    c4presc_(csubstr(s, len), keep_newlines);
}

struct maybe_null_str_
{
    csubstr s;
    maybe_null_str_(csubstr s_) noexcept : s(s_) {}
};
// LCOV_EXCL_START
inline C4_NO_INLINE size_t to_chars(substr buf, maybe_null_str_ const& v)
{
    return c4::format(buf, v.s.str ? v.s : csubstr("(out of size)"));
}
// LCOV_EXCL_STOP
template<class SinkPfn>
C4_NO_INLINE size_t dump(SinkPfn &&sinkfn, substr /*buf*/, maybe_null_str_ const& v)
{
    std::forward<SinkPfn>(sinkfn)(v.s.str ? v.s : csubstr("(out of size)"));
    return 0; // no space needed in the buffer
}

/** print string as [{s.len}]~~~{s}~~~ */
struct prs_
{
    csubstr subject;
    size_t maxsize;
    bool escape;
    bool keep_newlines; ///< keep newlines when escaping
    prs_(csubstr s) noexcept
        : subject(s)
        , maxsize(s.len)
        , escape(false)
        , keep_newlines(true)
    {
    }
    explicit prs_(csubstr s, size_t maxsz, bool esc=false, bool newl=false) noexcept
        : subject(s)
        , maxsize(maxsz == npos ? s.len : maxsz)
        , escape(esc)
        , keep_newlines(newl)
    {
    }
    explicit prs_(csubstr s, bool esc, bool newl=true) noexcept
        : subject(s)
        , maxsize(s.len)
        , escape(esc)
        , keep_newlines(newl)
    {
    }
};
// LCOV_EXCL_START
inline C4_NO_INLINE size_t to_chars(substr buf, prs_ const& v)
{
    csubstr s = v.subject;
    if(C4_LIKELY(s.str != nullptr))
    {
        csubstr ellipsis = "";
        if(v.maxsize < s.len)
        {
            s = s.first(v.maxsize);
            ellipsis = "...";
        }
        return !v.escape ?
            c4::format(buf, "[{}]~~~{}{}~~~", v.subject.len, s, ellipsis)
            :
            c4::format(buf, "[{}]~~~{}{}~~~", v.subject.len, escaped_scalar(s, v.keep_newlines), ellipsis);
    }
    return c4::format(buf, "[{}](out of size)", v.subject.len);
}
// LCOV_EXCL_STOP
template<class SinkPfn>
C4_NO_INLINE size_t dump(SinkPfn &&sinkfn, substr buf, prs_ const& v)
{
    csubstr s = v.subject;
    size_t sz = to_chars(buf, s.len);
    if(sz <= buf.len)
    {
        if(C4_LIKELY(s.str != nullptr))
        {
            csubstr ellipsis = "";
            if(v.maxsize < s.len)
            {
                s = s.first(v.maxsize);
                ellipsis = "...";
            }
            std::forward<SinkPfn>(sinkfn)("[");
            std::forward<SinkPfn>(sinkfn)(buf.first(sz));
            std::forward<SinkPfn>(sinkfn)("]~~~");
            if(!v.escape)
            {
                std::forward<SinkPfn>(sinkfn)(s);
            }
            else
            {
                dump(std::forward<SinkPfn>(sinkfn), buf, escaped_scalar(s, v.keep_newlines));
            }
            std::forward<SinkPfn>(sinkfn)(ellipsis);
            std::forward<SinkPfn>(sinkfn)("~~~");
        }
        else
        {
            std::forward<SinkPfn>(sinkfn)("[");
            std::forward<SinkPfn>(sinkfn)(buf.first(sz));
            std::forward<SinkPfn>(sinkfn)("](out of size)");
        }
    }
    return sz; // we require this space in the buffer
}

} // namespace yml
} // namespace c4

C4_SUPPRESS_WARNING_GCC_POP

#endif // RYML_DBG



// helper to export cases to the YAML test suite
#ifndef RYML_SAVE_TEST_YAML
#define RYML_SAVE_TEST_YAML_(filename, src)
#define RYML_SAVE_TEST_JSON_(filename, src)
#define RYML_SAVE_TEST_EXPFAIL_()
#else
#define RYML_SAVE_TEST_YAML_(filename, src) c4::yml::ryml_save_test_yaml(filename, src)
#define RYML_SAVE_TEST_JSON_(filename, src) c4::yml::ryml_save_test_json(filename, src)
#define RYML_SAVE_TEST_EXPFAIL_() c4::yml::ryml_save_test_expfail()
namespace c4 {
namespace yml {
void ryml_save_test_yaml(csubstr filename, csubstr src);
void ryml_save_test_json(csubstr filename, csubstr src);
void ryml_save_test_expfail();
} // namespace yml
} // namespace c4
#endif // RYML_SAVE_TEST_YAML


#endif /* C4_YML_DETAIL_DBGPRINT_HPP_ */
