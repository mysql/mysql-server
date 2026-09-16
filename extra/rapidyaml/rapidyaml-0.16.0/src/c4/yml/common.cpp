#include "c4/yml/common.hpp"
#include "c4/yml/error.hpp"
#include "c4/yml/error.def.hpp"

#ifndef RYML_NO_DEFAULT_CALLBACKS
#   include <stdlib.h>
#   include <stdio.h>
#endif // RYML_NO_DEFAULT_CALLBACKS
#ifdef _RYML_EXCEPTIONS
#       include <stdexcept>
#endif


namespace c4 {
namespace yml {

C4_SUPPRESS_WARNING_GCC_CLANG_WITH_PUSH("-Wold-style-cast")
C4_SUPPRESS_WARNING_MSVC_WITH_PUSH(4702/*unreachable code*/) // on the call to the unreachable macro

namespace {
Callbacks s_default_callbacks;

#ifndef RYML_NO_DEFAULT_CALLBACKS

C4_NO_INLINE void dump2stderr(csubstr s)
{
    // using fwrite() is more portable than using fprintf("%.*s") which
    // is not available in some embedded platforms
    if(s.len)
        fwrite(s.str, 1, s.len, stderr); // NOLINT
}
C4_NO_INLINE void endmsg()
{
    fputc('\n', stderr); // NOLINT
    fflush(stderr); // NOLINT
}

[[noreturn]] C4_NO_INLINE void error_basic_impl(csubstr msg, ErrorDataBasic const& errdata, void * /*user_data*/)
{
    err_basic_format(dump2stderr, msg, errdata);
    endmsg();
    #ifdef RYML_WITH_EXCEPTIONS_
    throw ExceptionBasic(msg, errdata);
    #else
    abort();
    #endif
}

[[noreturn]] C4_NO_INLINE void error_parse_impl(csubstr msg, ErrorDataParse const& errdata, void * /*user_data*/)
{
    err_parse_format(dump2stderr, msg, errdata);
    endmsg();
    #ifdef RYML_WITH_EXCEPTIONS_
    throw ExceptionParse(msg, errdata);
    #else
    abort();
    #endif
}

[[noreturn]] C4_NO_INLINE void error_visit_impl(csubstr msg, ErrorDataVisit const& errdata, void * /*user_data*/)
{
    err_visit_format(dump2stderr, msg, errdata);
    endmsg();
    #ifdef RYML_WITH_EXCEPTIONS_
    throw ExceptionVisit(msg, errdata);
    #else
    abort();
    #endif
}

void* allocate_impl(size_t length, void * /*hint*/, void * /*user_data*/)
{
    void *mem = ::malloc(length);
    if(mem == nullptr)
        error_basic_impl("could not allocate memory", ErrorDataBasic{RYML_LOC_HERE()}, nullptr); // LCOV_EXCL_LINE
    return mem;
}

void free_impl(void *mem, size_t /*length*/, void * /*user_data*/)
{
    ::free(mem);
}

#endif // RYML_NO_DEFAULT_CALLBACKS

} // anon namespace


void set_callbacks(Callbacks const& c)
{
    s_default_callbacks = c;
}

Callbacks const& get_callbacks()
{
    return s_default_callbacks;
}

void reset_callbacks()
{
    set_callbacks(Callbacks());
}


Callbacks::Callbacks() noexcept
    :
    m_user_data(nullptr),
    #ifndef RYML_NO_DEFAULT_CALLBACKS
    m_allocate(allocate_impl),
    m_free(free_impl),
    m_error_basic(error_basic_impl),
    m_error_parse(error_parse_impl),
    m_error_visit(error_visit_impl)
    #else
    m_allocate(nullptr),
    m_free(nullptr),
    m_error_basic(nullptr),
    m_error_parse(nullptr),
    m_error_visit(nullptr)
    #endif
{
}

Callbacks::Callbacks(void *user_data, pfn_allocate alloc_, pfn_free free_, pfn_error_basic error_basic_)
    :
    m_user_data(user_data),
    #ifndef RYML_NO_DEFAULT_CALLBACKS
    m_allocate(alloc_ ? alloc_ : allocate_impl),
    m_free(free_ ? free_ : free_impl),
    m_error_basic(error_basic_ ? error_basic_ : error_basic_impl),
    m_error_parse(error_parse_impl),
    m_error_visit(error_visit_impl)
    #else
    m_allocate(alloc_),
    m_free(free_),
    m_error_basic(error_basic_),
    m_error_parse(nullptr),
    m_error_visit(nullptr)
    #endif
{
}


Callbacks& Callbacks::set_user_data(void* user_data)
{
    m_user_data = user_data;
    return *this;
}

Callbacks& Callbacks::set_allocate(pfn_allocate allocate)
{
    m_allocate = allocate;
    #ifndef RYML_NO_DEFAULT_CALLBACKS
    m_allocate = m_allocate ? m_allocate : allocate_impl;
    #endif
    return *this;
}

Callbacks& Callbacks::set_free(pfn_free free)
{
    m_free = free;
    #ifndef RYML_NO_DEFAULT_CALLBACKS
    m_free = m_free ? m_free : free_impl;
    #endif
    return *this;
}

Callbacks& Callbacks::set_error_basic(pfn_error_basic error_basic)
{
    m_error_basic = error_basic;
    #ifndef RYML_NO_DEFAULT_CALLBACKS
    m_error_basic = m_error_basic ? m_error_basic : error_basic_impl;
    #endif
    return *this;
}

Callbacks& Callbacks::set_error_parse(pfn_error_parse error_parse)
{
    m_error_parse = error_parse;
    #ifndef RYML_NO_DEFAULT_CALLBACKS
    m_error_parse = m_error_parse ? m_error_parse : error_parse_impl;
    #endif
    return *this;
}

Callbacks& Callbacks::set_error_visit(pfn_error_visit error_visit)
{
    m_error_visit = error_visit;
    #ifndef RYML_NO_DEFAULT_CALLBACKS
    m_error_visit = m_error_visit ? m_error_visit : error_visit_impl;
    #endif
    return *this;
}


C4_NORETURN C4_NO_INLINE void err_basic(ErrorDataBasic const& errdata, const char* msg)
{
    err_basic(get_callbacks(), errdata, msg);
    C4_UNREACHABLE_AFTER_ERR();
}
C4_NORETURN C4_NO_INLINE void err_basic(Callbacks const& callbacks, ErrorDataBasic const& errdata, const char* msg_)
{
    csubstr msg = to_csubstr(msg_);
    callbacks.m_error_basic(msg, errdata, callbacks.m_user_data);
    abort(); // the call above should not return, so force it here in case it does // LCOV_EXCL_LINE
    C4_UNREACHABLE_AFTER_ERR();
}


C4_NORETURN C4_NO_INLINE void err_parse(ErrorDataParse const& errdata, const char *msg)
{
    err_parse(get_callbacks(), errdata, msg);
    C4_UNREACHABLE_AFTER_ERR();
}
C4_NORETURN C4_NO_INLINE void err_parse(Callbacks const& callbacks, ErrorDataParse const& errdata, const char *msg_)
{
    csubstr msg = to_csubstr(msg_);
    if(callbacks.m_error_parse)
        callbacks.m_error_parse(msg, errdata, callbacks.m_user_data);
    // fall to basic error if there is no parse handler set
    else if(callbacks.m_error_basic)
        callbacks.m_error_basic(msg, errdata.ymlloc, callbacks.m_user_data);
    abort(); // the call above should not return, so force it here in case it does // LCOV_EXCL_LINE
    C4_UNREACHABLE_AFTER_ERR();
}


C4_NORETURN C4_NO_INLINE void err_visit(ErrorDataVisit const& errdata, const char *msg)
{
    err_visit(get_callbacks(), errdata, msg);
    C4_UNREACHABLE_AFTER_ERR();
}
C4_NORETURN C4_NO_INLINE void err_visit(Callbacks const& callbacks, ErrorDataVisit const& errdata, const char *msg_)
{
    csubstr msg = to_csubstr(msg_);
    if(callbacks.m_error_visit)
        callbacks.m_error_visit(msg, errdata, callbacks.m_user_data);
    // fall to basic error if there is no visit handler set
    else if(callbacks.m_error_basic)
        callbacks.m_error_basic(msg, errdata.cpploc, callbacks.m_user_data);
    abort(); // the call above should not return, so force it here in case it does // LCOV_EXCL_LINE
    C4_UNREACHABLE_AFTER_ERR();
}



#ifdef RYML_WITH_EXCEPTIONS_
ExceptionBasic::ExceptionBasic(csubstr msg_, ErrorDataBasic const& errdata_) noexcept
    : errdata_basic(errdata_)
    , msg()
{
    msg[0] = '\0';
    if(msg_.len)
    {
        if(msg_.len >= sizeof(msg))
        {
            static_assert(sizeof(msg) > 6u, "message buffer too small");
            msg_.len = sizeof(msg) - 6u;
            msg[msg_.len     ] = '[';
            msg[msg_.len + 1u] = '.';
            msg[msg_.len + 2u] = '.';
            msg[msg_.len + 3u] = '.';
            msg[msg_.len + 4u] = ']';
            msg[msg_.len + 5u] = '\0';
        }
        memcpy(msg, msg_.str, msg_.len);
    }
}
ExceptionParse::ExceptionParse(csubstr msg_, ErrorDataParse const& errdata_) noexcept
    : ExceptionBasic(msg_, {errdata_.ymlloc})
    , errdata_parse(errdata_)
{
}
ExceptionVisit::ExceptionVisit(csubstr msg_, ErrorDataVisit const& errdata_) noexcept
    : ExceptionBasic(msg_, {errdata_.cpploc})
    , errdata_visit(errdata_)
{
}
#endif // RYML_WITH_EXCEPTIONS_


namespace detail {
RYML_EXPORT csubstr _get_text_region(csubstr text, size_t pos, size_t num_lines_before, size_t num_lines_after)
{
    if(pos > text.len)
        return text.last(0);
    size_t before = text.first(pos).last_of('\n');
    size_t before_count = 0;
    while((before != npos) && (++before_count <= num_lines_before))
    {
        if(before == 0)
            break;
        before = text.first(--before).last_of('\n');
    }
    if(before < text.len || before == npos)
        ++before;
    size_t after = text.first_of('\n', pos);
    size_t after_count = 0;
    while((after != npos) && (++after_count <= num_lines_after))
    {
        ++after;
        if(after >= text.len)
            break;
        after = text.first_of('\n', after);
    }
    return before <= after ? text.range(before, after) : text.first(0);
}
} // namespace detail

C4_SUPPRESS_WARNING_MSVC_POP
C4_SUPPRESS_WARNING_GCC_CLANG_POP

} // namespace yml
} // namespace c4
