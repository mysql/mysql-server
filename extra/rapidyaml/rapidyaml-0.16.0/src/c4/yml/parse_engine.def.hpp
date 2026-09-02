#ifndef C4_YML_PARSE_ENGINE_DEF_HPP_
#define C4_YML_PARSE_ENGINE_DEF_HPP_

#ifndef C4_YML_PARSE_ENGINE_HPP_
#include "c4/yml/parse_engine.hpp"
#endif
#ifndef C4_CHARCONV_HPP_
#include "c4/charconv.hpp"
#endif
#ifndef C4_UTF_HPP_
#include "c4/utf.hpp"
#endif
#ifndef C4_YML_FILTER_PROCESSOR_HPP_
#include "c4/yml/filter_processor.hpp"
#endif
#ifndef C4_YML_TAG_HPP_
#include "c4/yml/tag.hpp"
#endif
#ifndef C4_YML_NODE_TYPE_HPP_
#include "c4/yml/node_type.hpp"
#endif

#ifndef C4_YML_DETAIL_DBGPRINT_HPP_
#include "c4/yml/detail/dbgprint.hpp"
#endif

#ifdef RYML_DBG
#ifndef C4_DUMP_HPP_
#include <c4/dump.hpp>
#endif
#define _c4err(...)   \
    do { RYML_DEBUG_BREAK(); this->_err(RYML_LOC_HERE(), __VA_ARGS__); } while(0)
#else
#define _c4err(...)   \
    this->_err(RYML_LOC_HERE(), __VA_ARGS__)
#endif
#define _c4assert(...)   \
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, __VA_ARGS__, m_evt_handler->m_curr->pos)


#if defined(RYML_WITH_TAB_TOKENS)
#define RYML_WITH_TAB_TOKENS_(...) __VA_ARGS__
#define RYML_WITHOUT_TAB_TOKENS_(...)
#define RYML_WITH_OR_WITHOUT_TAB_TOKENS_(with, without) with
#else
#define RYML_WITH_TAB_TOKENS_(...)
#define RYML_WITHOUT_TAB_TOKENS_(...) __VA_ARGS__
#define RYML_WITH_OR_WITHOUT_TAB_TOKENS_(with, without) without
#endif


// scaffold:
#define _c4dbgnextline()                           \
    do {                                           \
       _c4dbgq("\n-----------");                   \
       _c4dbgt("handling line={}, offset={}B",     \
               m_evt_handler->m_curr->pos.line,    \
               m_evt_handler->m_curr->pos.offset); \
    } while(0)


C4_SUPPRESS_WARNING_MSVC_PUSH
C4_SUPPRESS_WARNING_MSVC(4296) // expression is always 'boolean_value'
C4_SUPPRESS_WARNING_MSVC(4702) // unreachable code
C4_SUPPRESS_WARNING_GCC_CLANG_PUSH
C4_SUPPRESS_WARNING_GCC_CLANG("-Wtype-limits") // to remove a warning on an assertion that a size_t >= 0. Later on, this size_t will turn into a template argument, and then it can become < 0.
C4_SUPPRESS_WARNING_GCC_CLANG("-Wformat-nonliteral")
C4_SUPPRESS_WARNING_GCC_CLANG("-Wold-style-cast")
#if defined(__GNUC__) && (__GNUC__ >= 6)
C4_SUPPRESS_WARNING_GCC("-Wnull-dereference")
#endif
#if defined(__GNUC__) && (__GNUC__ >= 7)
C4_SUPPRESS_WARNING_GCC("-Wduplicated-branches")
#endif

// NOLINTBEGIN(hicpp-signed-bitwise,cppcoreguidelines-avoid-goto,hicpp-avoid-goto,hicpp-multiway-paths-covered,modernize-avoid-c-style-cast)

namespace c4 {
namespace yml {

namespace { // NOLINT

C4_HOT C4_ALWAYS_INLINE void _set_first(substr &C4_RESTRICT subject, size_t pos) noexcept
{
    // avoids reassigning the ptr in substr
    subject.len = pos != npos ? pos : subject.len;
}
C4_HOT C4_ALWAYS_INLINE void _set_first(csubstr &C4_RESTRICT subject, size_t pos) noexcept
{
    // avoids reassigning the ptr in substr
    subject.len = pos != npos ? pos : subject.len;
}
C4_HOT C4_ALWAYS_INLINE void _set_first_strict(substr &C4_RESTRICT subject, size_t pos) RYML_NOEXCEPT
{
    // avoids reassigning the ptr in substr
    RYML_ASSERT_BASIC_(pos != npos); // LCOV_EXCL_LINE
    subject.len = pos;
}
C4_HOT C4_ALWAYS_INLINE void _set_first_strict(csubstr &C4_RESTRICT subject, size_t pos) RYML_NOEXCEPT
{
    // avoids reassigning the ptr in substr
    RYML_ASSERT_BASIC_(pos != npos); // LCOV_EXCL_LINE
    subject.len = pos;
}

C4_HOT C4_ALWAYS_INLINE bool _is_blck_token(csubstr s) RYML_NOEXCEPT
{
    RYML_ASSERT_BASIC_(s.len > 0);
    RYML_ASSERT_BASIC_(s.str[0] == '-' || s.str[0] == ':' || s.str[0] == '?');
    return ((s.len == 1) || ((s.str[1] == ' ') RYML_WITH_TAB_TOKENS_( || (s.str[1] == '\t'))));
}

C4_HOT C4_ALWAYS_INLINE bool _is_blck_seq_token_maybe(csubstr const& C4_RESTRICT s) noexcept
{
    return ((s.len >= 1) && (s.str[0] == '-') && ((s.len == 1) || ((s.str[1] == ' ') RYML_WITH_TAB_TOKENS_( || (s.str[1] == '\t')))));
}

inline bool _is_doc_begin_token(csubstr s) RYML_NOEXCEPT
{
    RYML_ASSERT_BASIC_(s.begins_with('-'));
    RYML_ASSERT_BASIC_(!s.ends_with("\n"));
    RYML_ASSERT_BASIC_(!s.ends_with("\r"));
    return (s.len >= 3 && s.str[1] == '-' && s.str[2] == '-')
        && (s.len == 3 || (s.str[3] == ' ' RYML_WITH_TAB_TOKENS_(|| s.str[3] == '\t')));
}

inline bool _is_doc_end_token(csubstr s) RYML_NOEXCEPT
{
    RYML_ASSERT_BASIC_(s.begins_with('.'));
    RYML_ASSERT_BASIC_(!s.ends_with("\n"));
    RYML_ASSERT_BASIC_(!s.ends_with("\r"));
    return (s.len >= 3 && s.str[1] == '.' && s.str[2] == '.')
        && (s.len == 3 || (s.str[3] == ' ' RYML_WITH_TAB_TOKENS_(|| s.str[3] == '\t')));
}

inline bool _is_doc_token(csubstr s) noexcept
{
    if(s.len >= 3)
    {
        switch(s.str[0])
        {
        case '-':
            //return _is_doc_begin_token(s); // this was failing with gcc -O2
            return (s.str[1] == '-' && s.str[2] == '-')
                && (s.len == 3 || (s.str[3] == ' ' RYML_WITH_TAB_TOKENS_(|| s.str[3] == '\t')));
        case '.':
            //return _is_doc_end_token(s); // this was failing with gcc -O2
            return (s.str[1] == '.' && s.str[2] == '.')
                && (s.len == 3 || (s.str[3] == ' ' RYML_WITH_TAB_TOKENS_(|| s.str[3] == '\t')));
        }
    }
    return false;
}

inline size_t _begins_with_special_json_scalar(csubstr s) RYML_NOEXCEPT
{
    RYML_ASSERT_BASIC_(s.len);
    switch(s.str[0])
    {
    case 'f':
        return s.begins_with("false") ? 5u : 0u;
    case 't':
        return s.begins_with("true") ? 4u : 0u;
    case 'n':
        return s.begins_with("null") ? 4u : 0u;
    }
    return 0u;
}


//-----------------------------------------------------------------------------

C4_ALWAYS_INLINE size_t _extend_from_combined_newline(char nl, char following)
{
    return (nl == '\n' && following == '\r') || (nl == '\r' && following == '\n');
}

//! look for the next newline chars, and jump to the right of those
inline substr _from_next_line(substr rem)
{
    size_t nlpos = rem.first_of("\r\n");
    if(nlpos == csubstr::npos)
        return {};
    const char nl = rem[nlpos];
    rem = rem.right_of(nlpos);
    if(rem.empty())
        return {};
    if(_extend_from_combined_newline(nl, rem.front()))
        rem = rem.sub(1);
    return rem;
}


//-----------------------------------------------------------------------------

inline size_t _count_following_newlines(csubstr r, size_t *C4_RESTRICT i)
{
    RYML_ASSERT_BASIC_(r[*i] == '\n');
    size_t numnl_following = 0;
    ++(*i);
    for( ; *i < r.len; ++(*i))
    {
        if(r.str[*i] == '\n')
            ++numnl_following;
        // skip leading whitespace
        else if(r.str[*i] == ' ' || r.str[*i] == '\t' || r.str[*i] == '\r')
            ;
        else
            break;
    }
    return numnl_following;
}

/** @p i is set to the first non whitespace character after the line
 * @return the number of empty lines after the initial position */
inline size_t _count_following_newlines(csubstr r, size_t *C4_RESTRICT i, size_t indentation)
{
    RYML_ASSERT_BASIC_(r[*i] == '\n');
    size_t numnl_following = 0;
    ++(*i);
    if(indentation == 0)
    {
        for( ; *i < r.len; ++(*i))
        {
            const char c = r.str[*i];
            if(c == '\n')
                ++numnl_following;
            // skip leading whitespace
            else if(c != ' ' && c != '\t' && c != '\r')
                break;
        }
    }
    else
    {
        for( ; *i < r.len; ++(*i))
        {
            char c = r.str[*i];
            if(c == '\n')
            {
                ++numnl_following;
                // skip the indentation after the newline
                size_t stop = *i + indentation;
                for( ; *i < r.len; ++(*i))
                {
                    c = r.str[*i];
                    if(c != ' ' && c != '\r')
                        break;
                    RYML_ASSERT_BASIC_(*i < stop); // LCOV_EXCL_LINE
                }
                C4_UNUSED(stop);
            }
            // skip leading whitespace
            else if(c != ' ' && c != '\t' && c != '\r')
            {
                break;
            }
        }
    }
    return numnl_following;
}

} // anon namespace


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

template<class EventHandler>
ParseEngine<EventHandler>::~ParseEngine() noexcept
{
    _free();
    _clr();
}

template<class EventHandler>
ParseEngine<EventHandler>::ParseEngine(EventHandler *evt_handler, ParserOptions const& opts)
    : m_options(opts)
    , m_evt_handler(evt_handler)
    , m_pending_anchors()
    , m_pending_tags()
    , m_has_directives_yaml(false)
    , m_has_directives(false)
    , m_doc_empty(true)
    , m_prev_colon(npos)
    , m_prev_val_end(npos)
    , m_encoding(NOBOM)
    , m_newline_offsets()
    , m_newline_offsets_size(0)
    , m_newline_offsets_capacity(0)
{
    RYML_CHECK_BASIC_(evt_handler);
}

template<class EventHandler>
ParseEngine<EventHandler>::ParseEngine(ParseEngine &&that) noexcept
    : m_options(that.m_options)
    , m_evt_handler(that.m_evt_handler)
    , m_pending_anchors(that.m_pending_anchors)
    , m_pending_tags(that.m_pending_tags)
    , m_has_directives_yaml(that.m_has_directives_yaml)
    , m_has_directives(that.m_has_directives)
    , m_doc_empty(that.m_doc_empty)
    , m_prev_colon(npos)
    , m_prev_val_end(npos)
    , m_encoding(NOBOM)
    , m_newline_offsets(that.m_newline_offsets)
    , m_newline_offsets_size(that.m_newline_offsets_size)
    , m_newline_offsets_capacity(that.m_newline_offsets_capacity)
{
    that._clr();
}

template<class EventHandler>
ParseEngine<EventHandler>::ParseEngine(ParseEngine const& that)
    : m_options(that.m_options)
    , m_evt_handler(that.m_evt_handler)
    , m_pending_anchors(that.m_pending_anchors)
    , m_pending_tags(that.m_pending_tags)
    , m_has_directives_yaml(that.m_has_directives_yaml)
    , m_has_directives(that.m_has_directives)
    , m_doc_empty(that.m_doc_empty)
    , m_prev_colon(npos)
    , m_prev_val_end(npos)
    , m_encoding(NOBOM)
    , m_newline_offsets()
    , m_newline_offsets_size()
    , m_newline_offsets_capacity()
{
    if(that.m_newline_offsets_capacity)
    {
        _resize_locations(that.m_newline_offsets_capacity);
        RYML_CHECK_BASIC_CB_(m_evt_handler->m_stack.m_callbacks, m_newline_offsets_capacity == that.m_newline_offsets_capacity);
        memcpy(m_newline_offsets, that.m_newline_offsets, that.m_newline_offsets_size * sizeof(size_t));
        m_newline_offsets_size = that.m_newline_offsets_size;
    }
}

template<class EventHandler>
ParseEngine<EventHandler>& ParseEngine<EventHandler>::operator=(ParseEngine &&that) noexcept
{
    _free();
    m_options = (that.m_options);
    m_evt_handler = that.m_evt_handler;
    m_pending_anchors = that.m_pending_anchors;
    m_pending_tags = that.m_pending_tags;
    m_has_directives_yaml = that.m_has_directives_yaml;
    m_has_directives = that.m_has_directives;
    m_doc_empty = that.m_doc_empty;
    m_prev_colon = that.m_prev_colon;
    m_prev_val_end = that.m_prev_val_end;
    m_encoding = that.m_encoding;
    m_newline_offsets = (that.m_newline_offsets);
    m_newline_offsets_size = (that.m_newline_offsets_size);
    m_newline_offsets_capacity = (that.m_newline_offsets_capacity);
    that._clr();
    return *this;
}

template<class EventHandler>
ParseEngine<EventHandler>& ParseEngine<EventHandler>::operator=(ParseEngine const& that)
{
    if(&that != this)
    {
        _free();
        m_options = (that.m_options);
        m_evt_handler = that.m_evt_handler;
        m_pending_anchors = that.m_pending_anchors;
        m_pending_tags = that.m_pending_tags;
        m_has_directives_yaml = that.m_has_directives_yaml;
        m_has_directives = that.m_has_directives;
        m_doc_empty = that.m_doc_empty;
        m_prev_colon = that.m_prev_colon;
        m_prev_val_end = that.m_prev_val_end;
        m_encoding = that.m_encoding;
        if(that.m_newline_offsets_capacity > m_newline_offsets_capacity)
            _resize_locations(that.m_newline_offsets_capacity);
        RYML_CHECK_BASIC_CB_(m_evt_handler->m_stack.m_callbacks, m_newline_offsets_capacity >= that.m_newline_offsets_capacity);
        RYML_CHECK_BASIC_CB_(m_evt_handler->m_stack.m_callbacks, m_newline_offsets_capacity >= that.m_newline_offsets_size);
        memcpy(m_newline_offsets, that.m_newline_offsets, that.m_newline_offsets_size * sizeof(size_t));
        m_newline_offsets_size = that.m_newline_offsets_size;
    }
    return *this;
}

template<class EventHandler>
void ParseEngine<EventHandler>::_clr()
{
    m_options = {};
    m_evt_handler = {};
    m_pending_anchors = {};
    m_pending_tags = {};
    m_has_directives_yaml = false;
    m_has_directives = false;
    m_doc_empty = true;
    m_prev_colon = npos;
    m_prev_val_end = npos;
    m_encoding = NOBOM;
    m_newline_offsets = {};
    m_newline_offsets_size = {};
    m_newline_offsets_capacity = {};
}

template<class EventHandler>
void ParseEngine<EventHandler>::_free()
{
    if(m_newline_offsets)
    {
        RYML_CB_FREE_(m_evt_handler->m_stack.m_callbacks, m_newline_offsets, size_t, m_newline_offsets_capacity);
        m_newline_offsets = nullptr;
        m_newline_offsets_size = 0u;
        m_newline_offsets_capacity = 0u;
    }
}


//-----------------------------------------------------------------------------

template<class EventHandler>
void ParseEngine<EventHandler>::_reset()
{
    m_pending_anchors = {};
    m_pending_tags = {};
    m_has_directives_yaml = false;
    m_has_directives = false;
    m_doc_empty = true;
    m_prev_colon = npos;
    m_prev_val_end = npos;
    m_bom_len = 0;
    m_encoding = NOBOM;
    m_bom_line = 0;
    if(m_options.locations())
    {
        _prepare_locations();
    }
}


//-----------------------------------------------------------------------------

template<class EventHandler>
void ParseEngine<EventHandler>::_relocate_arena(csubstr prev_arena, substr next_arena, substr *other)
{
    _c4dbgp("relocate to new arena");
    const char *pb = prev_arena.str;
    const char *pe = prev_arena.str + prev_arena.len;
    #define _ryml_relocate(s)                       \
    if((s).str >= pb && (s).str <= pe)              \
    {                                               \
        (s).str = next_arena.str + ((s).str - pb);  \
    }                                               \
    ((void)0)
    for(ParserState &st : m_evt_handler->m_stack)
    {
        _ryml_relocate(st.line_contents.rem);
        _ryml_relocate(st.line_contents.full);
    }
    _ryml_relocate(m_evt_handler->m_src);
    for(size_t i = 0; i < m_pending_tags.num_entries; ++i)
    {
        _ryml_relocate(m_pending_tags.annotations[i].str);  // LCOV_EXCL_LINE
        _ryml_relocate(m_pending_tags.annotations[i].orig); // LCOV_EXCL_LINE
    }
    for(size_t i = 0; i < m_pending_anchors.num_entries; ++i)
    {
        _ryml_relocate(m_pending_anchors.annotations[i].str);
        _ryml_relocate(m_pending_anchors.annotations[i].orig);
    }
    {
        TagDirectives &tds = m_evt_handler->tag_directives();
        for(size_t i = 0, sz = tds.size(); i < sz; ++i)
        {
            _ryml_relocate(tds.m_directives[i].handle);
            _ryml_relocate(tds.m_directives[i].prefix);
        }
    }
    {
        TagCache &tch = m_evt_handler->tag_cache();
        for(id_type i = 0, sz = tch.m_entries.size(); i < sz; ++i)
        {
            _ryml_relocate(tch.m_entries[i].tag);
            _ryml_relocate(tch.m_entries[i].resolved);
        }
    }
    if(other)
    {
        _ryml_relocate(*other);
    }
    #undef _ryml_relocate
}

/** @cond dev */
template<class EventHandler>
substr ParseEngine<EventHandler>::_alloc_arena(size_t len, substr *other)
{
    csubstr prev = m_evt_handler->arena();
    substr out = m_evt_handler->alloc_arena(len);
    substr curr = m_evt_handler->arena();
    if(curr.str != prev.str)
        _relocate_arena(prev, curr, other);
    return out;
}
/** @endcond */


//-----------------------------------------------------------------------------

#ifdef RYML_DBG
template<class EventHandler>
template<class DumpFn>
C4_NO_INLINE void ParseEngine<EventHandler>::_fmt_msg(DumpFn &&dumpfn) const
{
    ParserState const *const C4_RESTRICT st = m_evt_handler->m_curr;
    LineContents const& C4_RESTRICT lc = st->line_contents;
    csubstr contents = lc.full.first(lc.num_cols);
    if(contents.len)
    {
        // print the yaml src line
        size_t offs = 3u + to_chars(substr{}, st->pos.line) + to_chars(substr{}, st->pos.col);
        csubstr m_file = m_evt_handler->m_curr->pos.name;
        if(m_file.len)
        {
            dbg_dump_(std::forward<DumpFn>(dumpfn), "{}:", m_file);
            offs += m_file.len + 1;
        }
        dbg_dump_(std::forward<DumpFn>(dumpfn), "{}:{}: ", st->pos.line, st->pos.col);
        csubstr maybe_full_content = (contents.len < 80u ? contents : contents.first(80u));
        csubstr maybe_ellipsis = (contents.len < 80u ? csubstr{} : csubstr("..."));
        dbg_dump_(std::forward<DumpFn>(dumpfn), "{}{}  (size={})\n", escaped_scalar(maybe_full_content, /*escape*/true), maybe_ellipsis, contents.len);
        // highlight the remaining portion of the previous line
        size_t firstcol = (size_t)(lc.rem.str - lc.full.str);
        size_t lastcol = firstcol + lc.rem.len;
        size_t firstcol_adj = adjust_pos_with_escapes(lc.full, firstcol);
        size_t len = adjust_pos_with_escapes(lc.rem, lc.rem.len);
        for(size_t i = 0; i < offs + firstcol_adj; ++i)
            std::forward<DumpFn>(dumpfn)(" ");
        std::forward<DumpFn>(dumpfn)("^");
        for(size_t i = 1, e = (len < 80u ? len : 80u); i < e; ++i)
            std::forward<DumpFn>(dumpfn)("~");
        dbg_dump_(std::forward<DumpFn>(dumpfn), "{}  (cols {}-{})\n", maybe_ellipsis, firstcol+1, lastcol+1);
    }
    else
    {
        std::forward<DumpFn>(dumpfn)("\n");
    }
    // next line: print the state flags
    {
        char flagbuf_[128];
        dbg_dump_(std::forward<DumpFn>(dumpfn), "top state: {}\n", detail::_parser_flags_to_str(flagbuf_, m_evt_handler->m_curr->flags));
    }
}

template<class EventHandler>
void ParseEngine<EventHandler>::_print_state_stack(substr buf) const
{
    if(dbg_enabled_())
    {
        for(ParserState const& s : m_evt_handler->m_stack)
            dbg_printf_("state[{}]: ind={} node={} flags={}\n", s.level, s.indref, s.node_id, detail::_parser_flags_to_str(buf, s.flags));
    }
}

template<class EventHandler>
void ParseEngine<EventHandler>::_print_state_stack() const
{
    char buf[128];
    _print_state_stack(buf);
}
#endif


//-----------------------------------------------------------------------------

template<class EventHandler>
template<class ...Args>
C4_NORETURN C4_NO_INLINE void ParseEngine<EventHandler>::_err(Location const& cpploc, Location const& ymlloc, const char* fmt, Args const& ...args) const
{
    m_evt_handler->cancel_parse();
    err_parse(m_evt_handler->m_stack.m_callbacks, ErrorDataParse{cpploc, ymlloc}, fmt, args...);
}

template<class EventHandler>
template<class ...Args>
C4_NORETURN C4_NO_INLINE void ParseEngine<EventHandler>::_err(Location const& cpploc, const char *fmt, Args const& ...args) const
{
    m_evt_handler->cancel_parse();
    err_parse(m_evt_handler->m_stack.m_callbacks, ErrorDataParse{cpploc, m_evt_handler->m_curr->pos}, fmt, args...);
}


//-----------------------------------------------------------------------------
#ifdef RYML_DBG
template<class EventHandler>
template<class ...Args>
void ParseEngine<EventHandler>::_dbg(csubstr fmt, Args const& ...args) const
{
    if(dbg_enabled_())
    {
        dbg_printf_(fmt, args...);
        dbg_dumper_("\n");
        _fmt_msg(dbg_dumper_);
    }
}
#endif


//-----------------------------------------------------------------------------
template<class EventHandler>
bool ParseEngine<EventHandler>::_finished_file() const
{
    bool ret = m_evt_handler->m_curr->pos.offset >= _buf().len;
    #ifdef RYML_DBG
    if(ret)
    {
        _c4dbgp("finished file!!!");
    }
    #endif
    return ret;
}

template<class EventHandler>
C4_HOT C4_ALWAYS_INLINE bool ParseEngine<EventHandler>::_finished_line() const // LCOV_EXCL_LINE
{
    return m_evt_handler->m_curr->line_contents.rem.empty();
}


//-----------------------------------------------------------------------------

template<class EventHandler>
void ParseEngine<EventHandler>::_maybe_skip_whitespace_tokens()
{
    if(m_evt_handler->m_curr->line_contents.rem.len && (m_evt_handler->m_curr->line_contents.rem.str[0] == ' ' RYML_WITH_TAB_TOKENS_(|| m_evt_handler->m_curr->line_contents.rem.str[0] == '\t')))
    {
        size_t pos = m_evt_handler->m_curr->line_contents.rem.first_not_of(RYML_WITH_OR_WITHOUT_TAB_TOKENS_(" \t", ' '));
        if(pos == npos)
            pos = m_evt_handler->m_curr->line_contents.rem.len; // maybe the line is just all whitespace
        _c4dbgpf("skip {} whitespace characters", pos);
        _line_progressed(pos);
    }
}

template<class EventHandler>
void ParseEngine<EventHandler>::_maybe_skipchars(char c)
{
    if(m_evt_handler->m_curr->line_contents.rem.len && m_evt_handler->m_curr->line_contents.rem.str[0] == c)
    {
        size_t pos = m_evt_handler->m_curr->line_contents.rem.first_not_of(c);
        if(pos == npos)
            pos = m_evt_handler->m_curr->line_contents.rem.len; // maybe the line is just all c
        _c4dbgpf("skip {}x'{}'", pos, _c4prc(c));
        _line_progressed(pos);
    }
}

template<class EventHandler>
template<size_t N>
void ParseEngine<EventHandler>::_skipchars(const char (&chars)[N])
{
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, m_evt_handler->m_curr->line_contents.rem.begins_with_any(chars), m_evt_handler->m_curr->pos);
    size_t pos = m_evt_handler->m_curr->line_contents.rem.first_not_of(chars);
    if(pos == npos)
        pos = m_evt_handler->m_curr->line_contents.rem.len; // maybe the line is just whitespace
    _c4dbgpf("skip {} characters", pos);
    _line_progressed(pos);
}

template<class EventHandler>
void ParseEngine<EventHandler>::_skip_comment()
{
    LineContents const& C4_RESTRICT lc = m_evt_handler->m_curr->line_contents;
    const size_t col = m_evt_handler->m_curr->pos.col - 1u;
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, lc.rem.begins_with('#'), m_evt_handler->m_curr->pos);
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, lc.rem.is_sub(lc.full), m_evt_handler->m_curr->pos);
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, m_evt_handler->m_curr->pos.col >= 1, m_evt_handler->m_curr->pos); // 1-based
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, col == ((size_t)(lc.rem.str - lc.full.str)), m_evt_handler->m_curr->pos);
    // raise an error if the comment is not preceded by whitespace
    if(lc.rem.str != lc.full.str) // not at line beginning
    {
        RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, col > 0, m_evt_handler->m_curr->pos);
        const char prev = lc.full.str[col - 1u];
        if C4_UNLIKELY(prev != ' ' && prev != '\t')
            _c4err("comment not preceded by whitespace");
    }
    _c4dbgpf("comment was '{}'", m_evt_handler->m_curr->line_contents.rem);
    _line_progressed(m_evt_handler->m_curr->line_contents.rem.len);
}

template<class EventHandler>
void ParseEngine<EventHandler>::_maybe_skip_comment_strict()
{
    size_t pos = m_evt_handler->m_curr->line_contents.rem.first_not_of(" \t");
    if(pos != npos)
    {
        if('#' == m_evt_handler->m_curr->line_contents.rem[pos])
        {
            _line_progressed(pos);
            _skip_comment();
        }
    }
}

template<class EventHandler>
void ParseEngine<EventHandler>::_maybe_skip_comment()
{
    size_t pos = m_evt_handler->m_curr->line_contents.rem.first_not_of(" \t");
    if(pos != npos)
    {
        if('#' == m_evt_handler->m_curr->line_contents.rem[pos])
        {
            _line_progressed(pos);
            _skip_comment();
        }
    }
    else
    {
        _line_progressed(m_evt_handler->m_curr->line_contents.rem.len);
    }
}

template<class EventHandler>
bool ParseEngine<EventHandler>::_maybe_scan_following_colon() noexcept
{
    size_t pos = m_evt_handler->m_curr->line_contents.rem.first_not_of(" \t");
    if(pos != npos)
    {
        if(':' == m_evt_handler->m_curr->line_contents.rem[pos])
        {
            // bump pos to skip the colon as well, and check the colon
            // is followed by space or tab
            if(++pos < m_evt_handler->m_curr->line_contents.rem.len)
            {
                const char next = m_evt_handler->m_curr->line_contents.rem.str[pos];
                if(next == ' ' RYML_WITH_TAB_TOKENS_(|| next == '\t'))
                    ++pos;
                else
                    return false;
            }
            _line_progressed(pos);
            return true;
        }
    }
    else
    {
        _line_progressed(m_evt_handler->m_curr->line_contents.rem.len);
    }
    return false;
}


//-----------------------------------------------------------------------------

template<class EventHandler>
csubstr ParseEngine<EventHandler>::_scan_anchor()
{
    csubstr s = m_evt_handler->m_curr->line_contents.rem;
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, s.begins_with('&'), m_evt_handler->m_curr->pos);
    csubstr anchor = s.range(1, s.first_of(" ,]}\t"));
    _line_progressed(1u + anchor.len);
    _maybe_skipchars(' ');
    return anchor;
}

template<class EventHandler>
csubstr ParseEngine<EventHandler>::_scan_ref_seq()
{
    csubstr s = m_evt_handler->m_curr->line_contents.rem;
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, s.begins_with('*'), m_evt_handler->m_curr->pos);
    _set_first(s, s.first_of(" ,]\t"));
    _line_progressed(s.len);
    return s;
}

template<class EventHandler>
csubstr ParseEngine<EventHandler>::_scan_ref_map()
{
    csubstr s = m_evt_handler->m_curr->line_contents.rem;
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, s.begins_with('*'), m_evt_handler->m_curr->pos);
    _set_first(s, s.first_of(" ,}\t"));
    _line_progressed(s.len);
    return s;
}

template<class EventHandler>
csubstr ParseEngine<EventHandler>::_scan_tag()
{
    csubstr t = m_evt_handler->m_curr->line_contents.rem;
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, t.begins_with('!'), m_evt_handler->m_curr->pos);
    if(!t.begins_with("!<"))
    {
        _c4dbgp("begins with '!'");
        _set_first(t, t.first_of(" ,]}\t"));
        if C4_UNLIKELY(t.first_of("[{") != npos)
            _c4err("invalid tag");
        _line_progressed(t.len);
        if(m_options.resolve_tags_all() || (m_options.resolve_tags() && is_custom_tag(t)))
            t = _resolve_tag(t);
    }
    else
    {
        _c4dbgp("begins with '!<'");
        size_t pos = t.find('>');
        if C4_UNLIKELY(pos == npos)
            _c4err("invalid tag");
        _set_first_strict(t, pos+1);
        _line_progressed(t.len);
        t = t.sub(1);
    }
    _maybe_skip_whitespace_tokens();
    return t;
}

template<class EventHandler>
csubstr ParseEngine<EventHandler>::_scan_tag(csubstr *orig)
{
    csubstr t = m_evt_handler->m_curr->line_contents.rem;
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, t.begins_with('!'), m_evt_handler->m_curr->pos);
    if(!t.begins_with("!<"))
    {
        _c4dbgp("begins with '!'");
        _set_first(t, t.first_of(" ,\t"));
        if C4_UNLIKELY(t.first_of("[{") != npos)
            _c4err("invalid tag");
        _line_progressed(t.len);
        *orig = t;
        if(m_options.resolve_tags_all() || (m_options.resolve_tags() && is_custom_tag(t)))
            t = _resolve_tag(t);
    }
    else
    {
        _c4dbgp("begins with '!<'");
        size_t pos = t.find('>');
        if C4_UNLIKELY(pos == npos)
            _c4err("invalid tag");
        _set_first_strict(t, pos+1);
        _line_progressed(t.len);
        *orig = t;
        t = t.sub(1);
    }
    _maybe_skip_whitespace_tokens();
    return t;
}


//-----------------------------------------------------------------------------

template<class EventHandler>
bool ParseEngine<EventHandler>::_is_valid_start_scalar_plain_flow_check_block_token(csubstr s)
{
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, s.len > 0, m_evt_handler->m_curr->pos);
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, s.begins_with_any(":-"), m_evt_handler->m_curr->pos);
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, s.count('\n') == 0, m_evt_handler->m_curr->pos);
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, s.count('\r') == 0, m_evt_handler->m_curr->pos);
    if(s.len > 1)
    {
        switch(s.str[1])
        {
        case ' ':
        case ',':
        case '}':
        case ']':
        case '\t':
            if(s.str[0] == ':')
            {
                _c4dbgpf("not a scalar: found non-scalar token '{}{}'", s.str[0], s.str[1]);
                return false;
            }
            else
            {
                _c4err("invalid scalar");
            }
            break;
        case '{':
        case '[':
            _c4err("invalid token \":{}\"", _c4prc(s.str[1]));
            break;
        default:
            break;
        }
    }
    else
    {
        if(s.str[0] == '-')
            _c4err("invalid scalar");
        return false;
    }
    return true;
}

template<class EventHandler>
bool ParseEngine<EventHandler>::_is_valid_start_scalar_plain_flow_check_qmrk(csubstr s)
{
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, s.len > 0, m_evt_handler->m_curr->pos);
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, s[0] == '?', m_evt_handler->m_curr->pos);
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, s.count('\n') == 0, m_evt_handler->m_curr->pos);
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, s.count('\r') == 0, m_evt_handler->m_curr->pos);
    if(s.len > 1)
    {
        switch(s.str[1])
        {
        case ' ':
        case '\t':
                _c4dbgpf("not a scalar: found non-scalar token '?{}'", _c4prc(s.str[1]));
            return false;
        case '{':
        case '}':
        case '[':
        case ']':
            _c4err("invalid token \"?{}\"", _c4prc(s.str[1]));
            break;
        default:
            break;
        }
    }
    else
    {
        return false;
    }
    return true;
}


template<class EventHandler>
bool ParseEngine<EventHandler>::_is_valid_start_scalar_plain_flow(csubstr s)
{
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, !s.empty(), m_evt_handler->m_curr->pos);
    // it's not a scalar if it starts with any of these characters:
    switch(s.str[0])
    {
    // these are all legal tokens which mean no scalar is starting:
    case '[':
    case ']':
    case '{':
    case '}':
    case '&':
    case '*':
    case '!':
    case '|':
    case '>':
    case '#':
    case ',':
        _c4dbgpf("not a scalar: found non-scalar token '{}'", _c4prc(s.str[0]));
        return false;
    // '-' and ':' are illegal at the beginning if not followed by a scalar character
    case '-':
    case ':':
        _c4dbgpf("suspicious token='{}' len={}", _c4prc(s.str[0]), s.len);
        return _is_valid_start_scalar_plain_flow_check_block_token(s);
    case '?':
        _c4dbgpf("qmrk='{}' len={}", _c4prc(s.str[0]), s.len);
        return _is_valid_start_scalar_plain_flow_check_qmrk(s);
    // everything else is a legal starting character
    default:
        return true;
    }
}


template<class EventHandler>
bool ParseEngine<EventHandler>::_scan_scalar_plain_handle_newline(csubstr s, size_t offs)
{
    _c4dbgpf("newl[PLAIN]: found '\\n'. offs={} line={} sofar={}", offs, m_evt_handler->m_curr->pos.line, prs_(s.first(offs), true));
    if(s.len > offs + 1)
    {
        _c4dbgp("newl[PLAIN]: buffer continues");
        csubstr next_line = s.sub(offs + 1);
        size_t next_line_indentation = next_line.first_not_of(' ');
        if(next_line_indentation != npos)
        {
            _c4dbgpf("newl[PLAIN]: line={} indentation={} indref={}", m_evt_handler->m_curr->pos.line + 1, next_line_indentation, m_evt_handler->m_curr->indref);
            next_line = next_line.first(next_line.first_of("\n\r"));
            _c4dbgpf("newl[PLAIN]: has indentation. next_line={}", prs_(next_line));
            RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, next_line_indentation <= next_line.len, m_evt_handler->m_curr->pos);
            if C4_LIKELY(next_line_indentation >= m_evt_handler->m_curr->indref)
            {
                _c4dbgp("newl[PLAIN]: larger indentation");
                next_line = next_line.sub(next_line_indentation);
            }
            else if C4_UNLIKELY(next_line.len && next_line.triml(' ').len)
            {
                _c4dbgp("newl[PLAIN]: err, smaller indentation");
                _line_progressed(m_evt_handler->m_curr->line_contents.rem.len);
                _line_ended();
                _scan_line();
                if(m_evt_handler->m_curr->line_contents.indentation != npos)
                    _line_progressed(m_evt_handler->m_curr->line_contents.indentation);
                _c4err("parse error"); // cannot reduce indentation here
            }
            _c4dbgpf("newl[PLAIN]: next_line.len={}", next_line.len);
            if(next_line.len)
            {
                size_t fno = next_line.first_not_of(" \t");
                if(fno != csubstr::npos)
                {
                    _c4assert(fno < next_line.len);
                    switch(next_line.str[fno])
                    {
                    case ',': case ']': case '#':
                        _c4dbgpf("newl[PLAIN]: found terminating character beginning next line: '{}'", next_line.str[fno]);
                        return false;
                    case ':': // cannot be succeeded by whitespace
                        _c4dbgp("newl[PLAIN]: found :");
                        if(fno + 1 == next_line.len || _is_blck_token(next_line.sub(fno)))
                        {
                            _c4dbgpf("newl[PLAIN]: found terminating character beginning next line: '{}'", next_line.str[fno]);
                            return false;
                        }
                        break;
                    }
                }
            }
        }
    }
    _line_progressed(m_evt_handler->m_curr->line_contents.rem.len);
    _line_ended();
    _scan_line();
    return true;
}

template<class EventHandler>
bool ParseEngine<EventHandler>::_scan_scalar_plain_seq_flow(ScannedScalar *C4_RESTRICT sc)
{
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_none(RMAP), m_evt_handler->m_curr->pos);
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_none(RBLCK), m_evt_handler->m_curr->pos);
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_any(RSEQ|RSEQIMAP), m_evt_handler->m_curr->pos);
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_any(RFLOW), m_evt_handler->m_curr->pos);
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_any(RVAL), m_evt_handler->m_curr->pos);

    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, !m_evt_handler->m_curr->line_contents.rem.begins_with(' '), m_evt_handler->m_curr->pos);
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, !m_evt_handler->m_curr->line_contents.rem.begins_with('\n'), m_evt_handler->m_curr->pos);

    if(!m_evt_handler->m_curr->line_contents.rem.len || !_is_valid_start_scalar_plain_flow(m_evt_handler->m_curr->line_contents.rem))
        return false;

    substr s = _buf().sub(m_evt_handler->m_curr->pos.offset);
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, s.begins_with(m_evt_handler->m_curr->line_contents.rem), m_evt_handler->m_curr->pos);

    _c4dbgp("scanning seqflow scalar...");

    bool needs_filter = false;
    size_t col = 0; // zero-based column
    size_t offs = 0; // offset
    for( ; offs < s.len; ++offs, ++col)
    {
        const char c = s.str[offs];
        switch(c)
        {
        case ',':
        case ']':
            _c4dbgpf("found terminating character at {}: '{}'", offs, c);
            RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, offs > 0, m_evt_handler->m_curr->pos);
            goto ended_scalar;
        case '\n':
            _c4dbgpf("found '\\n' at col={}", col);
            if(!_scan_scalar_plain_handle_newline(s, offs))
                goto ended_scalar;
            col = (size_t)-1; // so that col is 0 in the next loop iteration
            needs_filter = true;
            break;
        case '\r':
            --col; // don't count \r when calling _line_progressed()
            needs_filter = true;
            break;
        case ':':
            _c4dbgp("found suspicious ':'");
            if(s.len > offs + 1)
            {
                char next = s.str[offs + 1];
                _c4dbgpf("next char is '{}'", _c4prc(next));
                if(next == '\r')
                {
                    csubstr after = s.sub(offs + 1).triml('\r');
                    if(after.len)
                    {
                        next = after.str[0];
                        _c4dbgpf("skip \\r to '{}'", _c4prc(next));
                    }
                }
                // no else here.
                if(next == ' ' RYML_WITH_TAB_TOKENS_(|| next == '\t') || next == ',' || next == '\n' || next == ']')
                {
                    _c4dbgp("map starting!");
                    goto ended_scalar;
                }
                else
                {
                    _c4dbgp("':' nothing to see here");
                }
            }
            else
            {
                RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, s.len == offs + 1, m_evt_handler->m_curr->pos);
                _line_progressed(col);
                _c4err("missing termination: '{}'", c); // noreturn
            }
            break;
        case '#':
            {
                _c4dbgp("found suspicious '#'");
                RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, offs > 0, m_evt_handler->m_curr->pos);
                char prev = s.str[offs - 1];
                if(prev == ' ' RYML_WITH_TAB_TOKENS_(|| prev == '\t'))
                {
                    _c4dbgpf("found terminating character at {}: '{}'", offs, c);
                    goto ended_scalar;
                }
            }
            break;
        case '[':
        case '{':
        case '}':
            _line_progressed(col); // advance to report the proper position in the error
            _c4err("invalid character: '{}'", c); // noreturn
        case '-':
        case '.':
            _c4dbgpf("doc token character: '{}', offs={}", c, offs);
            if(offs == 0 && m_evt_handler->m_curr->at_line_beginning())
            {
                _c4dbgp("at line beginning");
                if(s.len >= 3 && s.str[1] == c && s.str[2] == c)
                {
                    _c4err("parse error"); // no return
                }
            }
            break;
        default:
            ;
        }
    }

ended_scalar:

    _line_progressed(col);
    _set_first(s, offs);
    sc->scalar = s.trimr(RYML_WITH_OR_WITHOUT_TAB_TOKENS_(" \t", ' '));
    sc->needs_filter = needs_filter;

    _c4prscalar("scanned plain scalar", sc->scalar, /*keep_newlines*/true);

    return true;
}

template<class EventHandler>
bool ParseEngine<EventHandler>::_scan_scalar_plain_map_flow(ScannedScalar *C4_RESTRICT sc)
{
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_none(RSEQ) || has_any(RSEQIMAP), m_evt_handler->m_curr->pos);
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_none(RBLCK), m_evt_handler->m_curr->pos);
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_any(RMAP|RSEQIMAP), m_evt_handler->m_curr->pos);
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_any(RFLOW), m_evt_handler->m_curr->pos);
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_any(RKEY|RVAL|QMRK), m_evt_handler->m_curr->pos);

    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, !m_evt_handler->m_curr->line_contents.rem.begins_with(' '), m_evt_handler->m_curr->pos);
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, !m_evt_handler->m_curr->line_contents.rem.begins_with('\n'), m_evt_handler->m_curr->pos);

    if(!m_evt_handler->m_curr->line_contents.rem.len || !_is_valid_start_scalar_plain_flow(m_evt_handler->m_curr->line_contents.rem))
        return false;

    substr s = _buf().sub(m_evt_handler->m_curr->pos.offset);
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, s.begins_with(m_evt_handler->m_curr->line_contents.rem), m_evt_handler->m_curr->pos);

    _c4dbgp("scanning mapflow scalar...");

    bool needs_filter = false;
    size_t col = 0; // zero-based column
    size_t offs = 0; // offset
    for( ; offs < s.len; ++offs, ++col)
    {
        const char c = s.str[offs];
        switch(c)
        {
        case ',':
        case '}':
            _c4dbgpf("found terminating character at {}: '{}'", offs, c);
            RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, offs > 0, m_evt_handler->m_curr->pos);
            goto ended_scalar;
        case '\n':
            _c4dbgpf("found '\\n' at col={}", col);
            if(!_scan_scalar_plain_handle_newline(s, offs))
                goto ended_scalar;
            col = (size_t)-1; // so that col is 0 in the next loop iteration
            needs_filter = true;
            break;
        case '\r':
            --col; // don't count \r when calling _line_progressed()
            needs_filter = true;
            break;
        case ':':
            _c4dbgpf("found ':'", c);
            if(s.len == offs+1)
                break;
            {
                const char next = s.str[offs+1];
                _c4dbgpf("next='{}'", c);
                if(next == ' ' || next == ',' || next == '}' || next == '\n'  || next == '\r' RYML_WITH_TAB_TOKENS_(|| next == '\t'))
                {
                    _c4dbgpf("found terminating character: '{}'", c);
                    goto ended_scalar;
                }
            }
            break;
        case '{':
        case '[':
            _line_progressed(col);
            _c4err("invalid character: '{}'", c); // noreturn
            break;
        case ']':
            RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_any(RSEQIMAP), m_evt_handler->m_curr->pos);
            goto ended_scalar;
        default:
            ;
        }
    }

ended_scalar:

    _line_progressed(col);
    s = s.first(offs);
    sc->scalar = s.trimr(RYML_WITH_OR_WITHOUT_TAB_TOKENS_(" \t", ' '));
    sc->needs_filter = needs_filter;

    _c4prscalar("scanned plain scalar", sc->scalar, /*keep_newlines*/true);

    return sc->scalar.len > 0u;
}

template<class EventHandler>
bool ParseEngine<EventHandler>::_scan_scalar_seq_json(ScannedScalar *C4_RESTRICT sc)
{
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_none(RMAP), m_evt_handler->m_curr->pos);
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_none(RBLCK), m_evt_handler->m_curr->pos);
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_any(RSEQ), m_evt_handler->m_curr->pos);
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_any(RFLOW), m_evt_handler->m_curr->pos);

    substr s = m_evt_handler->m_curr->line_contents.rem;
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, !s.begins_with(' '), m_evt_handler->m_curr->pos);
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, s.len > 0, m_evt_handler->m_curr->pos);

    _c4dbgp("seq_json: scanning scalar...");

    switch(s.str[0])
    {
    case ']':
    case '{':
    case ',':
        _c4dbgp("seq_json: not a scalar.");
        return false;
    }

    {
        const size_t len = _begins_with_special_json_scalar(s);
        if(len)
        {
            char c = s.len > len ? s.str[len] : ',';
            if(c == ',' || c == ']' || c == ' ' || c == '\n' || c == '\t' || c == '\r')
            {
                sc->scalar = s.first(len);
                sc->needs_filter = false;
                _c4dbgpf("seq_json: special scalar: '{}'", sc->scalar);
                _line_progressed(len);
                return true;
            }
            else
            {
                return false;
            }
        }
    }

    // must be a number or special scalar
    size_t i = 0;
    for( ; i < s.len; ++i)
    {
        const char c = s.str[i];
        switch(c)
        {
        case ',':
        case ']':
        case ' ':
        case '\t':
            _c4dbgpf("seq_json: found terminating character: '{}'", c);
            goto ended_scalar;
        default:
            ;
        }
    }

ended_scalar:

    _line_progressed(i);
    sc->scalar = s.first(i);
    sc->needs_filter = false;
    _c4dbgpf("seq_json: scalar was {}", prs_(sc->scalar, /*escape*/true));

    return true;
}

template<class EventHandler>
bool ParseEngine<EventHandler>::_scan_scalar_map_json(ScannedScalar *C4_RESTRICT sc)
{
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_none(RSEQ), m_evt_handler->m_curr->pos);
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_none(RBLCK), m_evt_handler->m_curr->pos);
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_any(RMAP), m_evt_handler->m_curr->pos);
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_any(RFLOW), m_evt_handler->m_curr->pos);
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_any(RKEY|RVAL), m_evt_handler->m_curr->pos);

    substr s = m_evt_handler->m_curr->line_contents.rem;
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, !s.begins_with(' '), m_evt_handler->m_curr->pos);
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, s.len > 0, m_evt_handler->m_curr->pos);

    _c4dbgp("scanning scalar...");

    {
        const size_t len = _begins_with_special_json_scalar(s);
        if(len)
        {
            char c = s.len > len ? s.str[len] : ',';
            _c4dbgpf("begins with special scalar: {} next='{}'", s.first(len), _c4prc(c));
            if(c == ',' || c == '}' || c == ' ' || c == '\n' || c == '\t' || c == '\r')
            {
                sc->scalar = s.first(len);
                sc->needs_filter = false;
                _c4dbgpf("special json scalar: '{}'", prs_(sc->scalar));
                _line_progressed(len);
                return true;
            }
            else
            {
                return false;
            }
        }
    }

    // must be a number
    size_t i = 0;
    for( ; i < s.len; ++i)
    {
        const char c = s.str[i];
        switch(c)
        {
        case ',':
        case '}':
        case ' ':
        case '\t':
            _c4dbgpf("found terminating character: '{}'", c);
            goto ended_scalar;
        default:
            ;
        }
    }

ended_scalar:

    if C4_LIKELY(i > 0)
    {
        _line_progressed(i);
        sc->scalar = s.first(i);
        sc->needs_filter = false;
        _c4dbgpf("scalar was {}", prs_(sc->scalar));
        return true;
    }

    return false;
}

template<class EventHandler>
bool ParseEngine<EventHandler>::_is_doc_begin(csubstr s)
{
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, s[0] == '-', m_evt_handler->m_curr->pos);
    return (m_evt_handler->m_curr->line_contents.indentation == 0u && m_evt_handler->m_curr->at_line_beginning() && _is_doc_begin_token(s));
}

template<class EventHandler>
bool ParseEngine<EventHandler>::_is_doc_end(csubstr s)
{
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, s[0] == '.', m_evt_handler->m_curr->pos);
    return (m_evt_handler->m_curr->line_contents.indentation == 0u && m_evt_handler->m_curr->at_line_beginning() && _is_doc_end_token(s));
}

template<class EventHandler>
bool ParseEngine<EventHandler>::_scan_scalar_plain_blck(ScannedScalar *C4_RESTRICT sc, size_t indentation)
{
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_none(RFLOW), m_evt_handler->m_curr->pos);
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_none(RSEQIMAP), m_evt_handler->m_curr->pos);
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_any(RBLCK|RUNK|USTY), m_evt_handler->m_curr->pos);

    substr s = m_evt_handler->m_curr->line_contents.rem;
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, !s.begins_with(' '), m_evt_handler->m_curr->pos);
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, s.len > 0, m_evt_handler->m_curr->pos);

    switch(s.str[0])
    {
    case '-':
        if(_is_blck_token(s))
        {
            return false;
        }
        else if(_is_doc_begin(s))
        {
            _c4dbgp("token is doc start");
            return false;
        }
        break;
    case ':':
    case '?':
        if(_is_blck_token(s))
            return false;
        break;
    case '[':
    case '{':
    case '&':
    case '*':
    case '!':
    case '\t':
    case ',':
    case '%':
        return false;
    case '.':
        if(_is_doc_end(s))
        {
            _c4dbgp("token is doc end");
            return false;
        }
        break;
    }

    _c4dbgpf("plain scalar! indentation={}", indentation);

    const size_t start_offset = m_evt_handler->m_curr->pos.offset;
    const size_t start_line = m_evt_handler->m_curr->pos.line;

    bool needs_filter = false;
    while(true)
    {
        _c4dbgpf("plain scalar line: {}", prs_(s));
        for(size_t i = 0; i < s.len; ++i)
        {
            const char curr = s.str[i];
            //_c4dbgpf("[{}]='{}'", i, _c4prc(curr));
            switch(curr)
            {
            case ':':
                _c4dbgpf("[{}]: got suspicious ':'", i);
                // are there more characters?
                if((i + 1 == s.len) || ((s.str[i+1] == ' ') RYML_WITH_TAB_TOKENS_( || (s.str[i+1] == '\t'))))
                {
                    _c4dbgpf("followed by '{}'", i+1 == s.len ? csubstr("\\n") : _c4prc(s.str[i+1]));
                    _line_progressed(i);
                    // ': ' is accepted only on the first line
                    if C4_LIKELY(m_evt_handler->m_curr->pos.line == start_line)
                    {
                        _c4dbgp("start line. scalar ends here");
                        goto ended_scalar;
                    }
                    else
                    {
                        _c4err("multiline scalars cannot be used as keys");
                    }
                }
                else
                {
                    size_t j = i;
                    while(j + 1 < s.len && s.str[j+1] == ':')
                    {
                        _c4dbgp("skip colon");
                        ++j;
                    }
                    i = j > i ? j-1 : i;
                    _c4dbgp("nothing to see here");
                }
                break;
            case '#':
                _c4dbgp("got suspicious '#'");
                if(!i || (s.str[i-1] == ' ' || s.str[i-1] == '\t'))
                {
                    _c4dbgp("comment! scalar ends here");
                    _line_progressed(i);
                    goto ended_scalar;
                }
                else
                {
                    _c4dbgp("nothing to see here");
                }
                break;
            }
        }
        _line_progressed(s.len);
        csubstr next_peeked = _peek_next_line(m_evt_handler->m_curr->pos.offset);
        next_peeked = next_peeked.trimr("\n\r");
        const size_t next_indentation = next_peeked.first_not_of(' ');
        _c4dbgpf("indentation curr={} next={}", indentation, next_indentation);
        if(next_indentation < indentation)
        {
            _c4dbgp("smaller indentation! scalar ended");
            goto ended_scalar;
        }
        else if(next_indentation == 0 && next_peeked.len > 0)
        {
            const char first = next_peeked.str[0];
            switch(first)
            {
            case '-':
                _c4dbgpf("doc begin? peeked={}", prs_(next_peeked, size_t(3)));
                if(_is_doc_begin_token(next_peeked))
                {
                    _c4dbgp("doc begin! scalar ended");
                    goto ended_scalar;
                }
                break;
            case '.':
                _c4dbgpf("doc end? peeked={}", prs_(next_peeked, size_t(3)));
                if(_is_doc_end_token(next_peeked))
                {
                    _c4dbgp("doc end! scalar ended");
                    goto ended_scalar;
                }
                break;
            }
        }
        // load with next line
        _c4dbgp("next line!");
        if(!_finished_file())
        {
            _c4dbgp("next line!");
            _line_ended();
            _scan_line();
        }
        else
        {
            _c4dbgp("file finished!");
            goto ended_scalar;
        }
        s = m_evt_handler->m_curr->line_contents.rem;
        needs_filter = true;
    }

ended_scalar:

    sc->scalar = _buf().range(start_offset, m_evt_handler->m_curr->pos.offset).trimr(" \n\r\t");
    sc->needs_filter = needs_filter;

    _c4dbgpf("scalar was {}", prs_(sc->scalar));

    return true;
}

template<class EventHandler>
C4_ALWAYS_INLINE bool ParseEngine<EventHandler>::_scan_scalar_plain_seq_blck(ScannedScalar *C4_RESTRICT sc) // LCOV_EXCL_LINE
{
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_none(RMAP), m_evt_handler->m_curr->pos);
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_none(RFLOW), m_evt_handler->m_curr->pos);
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_none(RSEQIMAP), m_evt_handler->m_curr->pos);
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_any(RSEQ), m_evt_handler->m_curr->pos);
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_any(RBLCK), m_evt_handler->m_curr->pos);
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_any(RVAL), m_evt_handler->m_curr->pos);
    return _scan_scalar_plain_blck(sc, m_evt_handler->m_curr->indref + 1u);
}

template<class EventHandler>
C4_ALWAYS_INLINE bool ParseEngine<EventHandler>::_scan_scalar_plain_map_blck(ScannedScalar *C4_RESTRICT sc) // LCOV_EXCL_LINE
{
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_none(RSEQ), m_evt_handler->m_curr->pos);
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_none(RFLOW), m_evt_handler->m_curr->pos);
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_any(RMAP), m_evt_handler->m_curr->pos);
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_any(RBLCK), m_evt_handler->m_curr->pos);
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_any(RKEY|RVAL|QMRK), m_evt_handler->m_curr->pos);
    return _scan_scalar_plain_blck(sc, m_evt_handler->m_curr->indref + 1u);
}

template<class EventHandler>
C4_ALWAYS_INLINE bool ParseEngine<EventHandler>::_scan_scalar_plain_unk(ScannedScalar *C4_RESTRICT sc) // LCOV_EXCL_LINE
{
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks,  has_any(RUNK|USTY), m_evt_handler->m_curr->pos);
    return _scan_scalar_plain_blck(sc, m_evt_handler->m_curr->indref);
}


//-----------------------------------------------------------------------------

template<class EventHandler>
substr ParseEngine<EventHandler>::_peek_next_line(size_t pos) const
{
    substr rem{}; // declare here because of the goto
    size_t nlpos{}; // declare here because of the goto
    pos = pos == npos ? m_evt_handler->m_curr->pos.offset : pos;
    if(pos >= _buf().len)
        goto next_is_empty;

    // look for the next newline chars, and jump to the right of those
    rem = _from_next_line(_buf().sub(pos));
    if(rem.empty())
        goto next_is_empty;

    // now get everything up to and including the following newline chars
    nlpos = rem.first_of("\r\n");
    if((nlpos != csubstr::npos) && (nlpos + 1 < rem.len))
        nlpos += _extend_from_combined_newline(rem[nlpos], rem[nlpos+1]);
    rem = rem.left_of(nlpos, /*include_pos*/true);

    _c4dbgpf("peek next line @ {}: (len={})'{}'", pos, rem.len, rem.trimr("\r\n"));
    return rem;

next_is_empty:
    _c4dbgpf("peek next line @ {}: (len=0)''", pos);
    return rem;
}

//-----------------------------------------------------------------------------

template<class EventHandler>
void ParseEngine<EventHandler>::_scan_line()
{
    if C4_LIKELY(m_evt_handler->m_curr->pos.offset < _buf().len)
        m_evt_handler->m_curr->line_contents.reset_with_next_line(_buf(), m_evt_handler->m_curr->pos.offset);
    else
        m_evt_handler->m_curr->line_contents.reset_with_next_line(_buf().last(0), 0);
}

template<class EventHandler>
void ParseEngine<EventHandler>::_line_progressed(size_t ahead)
{
    _c4dbgpf("line[{}] ({} cols) progressed by {}:  col {}-->{}   offset {}-->{}",
             m_evt_handler->m_curr->pos.line,
             m_evt_handler->m_curr->line_contents.full.len,
             ahead, m_evt_handler->m_curr->pos.col,
             m_evt_handler->m_curr->pos.col+ahead,
             m_evt_handler->m_curr->pos.offset,
             m_evt_handler->m_curr->pos.offset+ahead);
    m_evt_handler->m_curr->pos.offset += ahead;
    m_evt_handler->m_curr->pos.col += ahead;
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, m_evt_handler->m_curr->pos.col <= m_evt_handler->m_curr->line_contents.num_cols+1, m_evt_handler->m_curr->pos);
    m_evt_handler->m_curr->line_contents.rem = m_evt_handler->m_curr->line_contents.rem.sub(ahead);
}

template<class EventHandler>
void ParseEngine<EventHandler>::_line_ended()
{
    _c4dbgpf("line[{}] ({} cols) ended! offset {}-->{} / col {}-->{}",
             m_evt_handler->m_curr->pos.line,
             m_evt_handler->m_curr->line_contents.full.len,
             m_evt_handler->m_curr->pos.offset, m_evt_handler->m_curr->pos.offset + m_evt_handler->m_curr->line_contents.full.len - m_evt_handler->m_curr->line_contents.num_cols,
             m_evt_handler->m_curr->pos.col, 1);
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, m_evt_handler->m_curr->pos.col == m_evt_handler->m_curr->line_contents.num_cols + 1, m_evt_handler->m_curr->pos);
    m_evt_handler->m_curr->pos.offset += m_evt_handler->m_curr->line_contents.full.len - m_evt_handler->m_curr->line_contents.num_cols;
    ++m_evt_handler->m_curr->pos.line;
    m_evt_handler->m_curr->pos.col = 1;
}

template<class EventHandler>
void ParseEngine<EventHandler>::_line_ended_undo()
{
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, m_evt_handler->m_curr->pos.col == 1u, m_evt_handler->m_curr->pos);
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, m_evt_handler->m_curr->pos.line > 0u, m_evt_handler->m_curr->pos);
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, m_evt_handler->m_curr->pos.offset >= m_evt_handler->m_curr->line_contents.full.len - m_evt_handler->m_curr->line_contents.num_cols, m_evt_handler->m_curr->pos);
    const size_t delta = m_evt_handler->m_curr->line_contents.full.len - m_evt_handler->m_curr->line_contents.num_cols;
    _c4dbgpf("line[{}] undo ended! line {}-->{}, offset {}-->{}", m_evt_handler->m_curr->pos.line, m_evt_handler->m_curr->pos.line, m_evt_handler->m_curr->pos.line - 1, m_evt_handler->m_curr->pos.offset, m_evt_handler->m_curr->pos.offset - delta);
    m_evt_handler->m_curr->pos.offset -= delta;
    --m_evt_handler->m_curr->pos.line;
    m_evt_handler->m_curr->pos.col = m_evt_handler->m_curr->line_contents.num_cols + 1u;
    // don't forget to undo also the changes to the remainder of the line
    //RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, m_evt_handler->m_curr->pos.offset >= _buf().len || _buf()[m_evt_handler->m_curr->pos.offset] == '\n' || _buf()[m_evt_handler->m_curr->pos.offset] == '\r', m_evt_handler->m_curr->pos);
    m_evt_handler->m_curr->line_contents.rem = _buf().sub(m_evt_handler->m_curr->pos.offset, 0);
}


//-----------------------------------------------------------------------------
template<class EventHandler>
void ParseEngine<EventHandler>::_set_indentation(size_t indentation) noexcept
{
    m_evt_handler->m_curr->indref = indentation;
    _c4dbgpf("state[{}]: saving indentation: {}", m_evt_handler->m_curr->level, m_evt_handler->m_curr->indref);
}

template<class EventHandler>
void ParseEngine<EventHandler>::_save_indentation()
{
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, m_evt_handler->m_curr->line_contents.rem.is_sub(m_evt_handler->m_curr->line_contents.full), m_evt_handler->m_curr->pos);
    m_evt_handler->m_curr->indref = m_evt_handler->m_curr->line_contents.current_col();
    _c4dbgpf("state[{}]: saving indentation: {}", m_evt_handler->m_curr->level, m_evt_handler->m_curr->indref);
}

template<class EventHandler>
void ParseEngine<EventHandler>::_mark_seqflow_val_end() noexcept
{
    _c4dbgpf("SEQFLOW. mark val end at line={}", m_evt_handler->m_curr->pos.line);
    m_prev_val_end = m_evt_handler->m_curr->pos.line;
}


//-----------------------------------------------------------------------------

template<class EventHandler>
void ParseEngine<EventHandler>::_flow_container_was_a_key(size_t orig_indent)
{
    _c4dbgpf("flow container is followed by colon! orig_indent={}", orig_indent);
    m_evt_handler->actually_val_is_first_key_of_new_map_block();
    addrem_flags(RMAP|RVAL|RBLCK, RKCL|RSEQ|RUNK);
    _set_indentation(orig_indent);
    _maybe_skip_whitespace_tokens();
}

template<class EventHandler>
void ParseEngine<EventHandler>::_end_flow_container(size_t orig_indent, bool multiline)
{
    // this is called AFTER ending the flow container,
    // so now we're at the parent container's scope
    if(has_all(RMAP|RBLCK) && has_none(RKCL|RVAL|RNXT))
    {
        _c4dbgp("flow container: end as vanilla block map key!");
        if C4_UNLIKELY(multiline)
            _c4err("multiline key is invalid");
        if C4_UNLIKELY(!_maybe_scan_following_colon())
            _c4err("could not find ':' colon after key");
        _maybe_skip_whitespace_tokens();
        addrem_flags(RVAL, RKEY|RKCL|RNXT);
    }
    else if(has_none(RFLOW))
    {
        _c4dbgp("end_flow_container: now not in flow!");
        if(has_any(RUNK|RSEQ|RKCL) && _maybe_scan_following_colon())
        {
            if C4_UNLIKELY(multiline)
                _c4err("multiline key is invalid");
            _flow_container_was_a_key(orig_indent);
        }
        else
        {
            _c4dbgp("end_flow_container: end map as key!");
        }
    }
    else if(has_any(RSEQ))
    {
        _c4dbgp("end_flow_container: now in a flow seq");
        RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_any(RFLOW), m_evt_handler->m_curr->pos);
        _mark_seqflow_val_end();
    }
}

template<class EventHandler>
void ParseEngine<EventHandler>::_end_map_flow()
{
    bool multiline = m_evt_handler->m_parent->pos.line < m_evt_handler->m_curr->pos.line;
    size_t orig_indent = m_evt_handler->m_curr->indref;
    _c4dbgpf("mapflow: end, multiline={}", multiline);
    m_evt_handler->end_map_flow(multiline && m_options.detect_flow_ml(), m_options.flow_ml_style().m_bits);
    _end_flow_container(orig_indent, multiline);
}

template<class EventHandler>
void ParseEngine<EventHandler>::_end_seq_flow()
{
    bool multiline = m_evt_handler->m_parent->pos.line < m_evt_handler->m_curr->pos.line;
    size_t orig_indent = m_evt_handler->m_curr->indref;
    _c4dbgpf("seqflow: end, multiline={}", multiline);
    m_evt_handler->end_seq_flow(multiline && m_options.detect_flow_ml(), m_options.flow_ml_style().m_bits);
    _end_flow_container(orig_indent, multiline);
}

template<class EventHandler>
void ParseEngine<EventHandler>::_end_map_blck()
{
    _c4dbgp("mapblck: end");
    if(has_any(RKCL|RVAL))
    {
        _c4dbgp("mapblck: set missing val");
        _handle_annotations_before_blck_val_scalar();
        m_evt_handler->set_val_scalar_plain_empty();
    }
    else if(has_any(QMRK))
    {
        _c4dbgp("mapblck: set missing keyval");
        _handle_annotations_before_blck_key_scalar();
        m_evt_handler->set_key_scalar_plain_empty();
        _handle_annotations_before_blck_val_scalar();
        m_evt_handler->set_val_scalar_plain_empty();
    }
    m_evt_handler->end_map_block();
}

template<class EventHandler>
void ParseEngine<EventHandler>::_end_seq_blck()
{
    if(has_any(RVAL))
    {
        _c4dbgp("seqblck: set missing val");
        _handle_annotations_before_blck_val_scalar();
        m_evt_handler->set_val_scalar_plain_empty();
    }
    m_evt_handler->end_seq_block();
}

template<class EventHandler>
void ParseEngine<EventHandler>::_end2_map()
{
    _c4dbgp("map: end");
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_any(RMAP), m_evt_handler->m_curr->pos);
    if(has_any(RBLCK))
    {
        _end_map_blck();
    }
    else
    {
        RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_none(RFLOW), m_evt_handler->m_curr->pos);
        RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_any(USTY), m_evt_handler->m_curr->pos);
        m_evt_handler->_pop();
    }
}

template<class EventHandler>
void ParseEngine<EventHandler>::_end2_seq()
{
    _c4dbgp("seq: end");
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_any(RSEQ), m_evt_handler->m_curr->pos);
    if(has_any(RBLCK))
    {
        _end_seq_blck();
    }
    else
    {
        RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_none(RFLOW), m_evt_handler->m_curr->pos);
        RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_any(USTY), m_evt_handler->m_curr->pos);
        m_evt_handler->_pop();
    }
}

template<class EventHandler>
void ParseEngine<EventHandler>::_begin2_doc()
{
    _c4dbgp("begin_doc");
    m_has_directives_yaml = false;
    m_has_directives = false;
    m_doc_empty = true;
    add_flags(RDOC);
    m_evt_handler->begin_doc();
    m_evt_handler->m_curr->indref = 0; // ?
}

template<class EventHandler>
void ParseEngine<EventHandler>::_begin2_doc_expl()
{
    _c4dbgp("begin_doc_expl");
    m_has_directives_yaml = false;
    m_has_directives = false;
    m_doc_empty = true;
    add_flags(RDOC);
    m_evt_handler->begin_doc_expl();
    m_evt_handler->m_curr->indref = 0; // ?
}

template<class EventHandler>
void ParseEngine<EventHandler>::_end2_doc()
{
    _c4dbgp("doc: end");
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_any(RDOC), m_evt_handler->m_curr->pos);
    if(m_doc_empty || (m_pending_tags.num_entries || m_pending_anchors.num_entries))
    {
        _c4dbgp("doc was empty; add empty val");
        _handle_annotations_before_blck_val_scalar();
        m_evt_handler->set_val_scalar_plain_empty();
    }
    m_evt_handler->end_doc();
    m_bom_len = 0;
}

template<class EventHandler>
void ParseEngine<EventHandler>::_end2_doc_expl()
{
    _c4dbgp("doc: end");
    if(m_doc_empty || (m_pending_tags.num_entries || m_pending_anchors.num_entries))
    {
        _c4dbgp("doc: no children; add empty val");
        _handle_annotations_before_blck_val_scalar();
        m_evt_handler->set_val_scalar_plain_empty();
    }
    m_evt_handler->end_doc_expl();
    m_bom_len = 0;
}

template<class EventHandler>
void ParseEngine<EventHandler>::_maybe_begin_doc()
{
    if(has_none(RDOC))
    {
        _c4dbgp("doc must be started");
        _begin2_doc();
    }
}
template<class EventHandler>
void ParseEngine<EventHandler>::_maybe_end_doc()
{
    if(has_any(RDOC))
    {
        _c4dbgp("doc must be finished");
        _end2_doc();
    }
    else if(m_doc_empty && (m_pending_tags.num_entries || m_pending_anchors.num_entries))
    {
        _c4dbgp("no doc to finish, but pending annotations");
        m_evt_handler->begin_doc();
        _handle_annotations_before_blck_val_scalar();
        m_evt_handler->set_val_scalar_plain_empty();
        m_evt_handler->end_doc();
    }
}

template<class EventHandler>
void ParseEngine<EventHandler>::_end_doc_suddenly__pop()
{
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, m_evt_handler->m_stack.size() >= 1, m_evt_handler->m_curr->pos);
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, m_evt_handler->m_stack[0].flags & RDOC, m_evt_handler->m_curr->pos);
    _c4dbgp("root is RDOC");
    if(m_evt_handler->m_curr->level != 0)
        _handle_indentation_pop(&m_evt_handler->m_stack[0]);
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_any(RDOC), m_evt_handler->m_curr->pos);
}

/** Check whether the current parse tokens are trailing on the
 * previous doc, and raise an error if they are */
template<class EventHandler>
void ParseEngine<EventHandler>::_check_trailing_doc_token()
{
    const bool is_root = (m_evt_handler->m_stack.size() == 1u);
    const bool isndoc = (m_evt_handler->m_curr->flags & NDOC) != 0;
    const bool suspicious = m_evt_handler->template has_any_<MAP|SEQ|VAL>();
    _c4dbgpf("target={} isroot={} suspicious={} ndoc={}", m_evt_handler->m_curr->node_id, is_root, suspicious, isndoc);
    if((is_root || m_evt_handler->template has_any_<DOC>()) && suspicious && !isndoc)
        _c4err("parse error");
}

template<class EventHandler>
void ParseEngine<EventHandler>::_end_doc_suddenly()
{
    _c4dbgp("end doc suddenly");
    _end_doc_suddenly__pop();
    _end2_doc_expl();
    addrem_flags(RUNK|RTOP|NDOC, RMAP|RSEQ|RDOC);
}

template<class EventHandler>
void ParseEngine<EventHandler>::_check_doc_end_tokens() const
{
    csubstr rem = m_evt_handler->m_curr->line_contents.rem;
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, !rem.begins_with_any(". \t"), m_evt_handler->m_curr->pos);
    if C4_UNLIKELY(rem.len && !rem.begins_with('#'))
    {
        _c4err("parse error");
    }
}

template<class EventHandler>
void ParseEngine<EventHandler>::_start_doc_suddenly()
{
    _c4dbgp("start doc suddenly");
    _end_doc_suddenly__pop();
    _end2_doc();
    _begin2_doc_expl();
}

template<class EventHandler>
void ParseEngine<EventHandler>::_end_stream()
{
    _c4dbgpf("end_stream, level={} node_id={}", m_evt_handler->m_curr->level, m_evt_handler->m_curr->node_id);
    if C4_UNLIKELY(has_all(RSEQ|RFLOW))
        _c4err("missing terminating ]");
    else if C4_UNLIKELY(has_all(RMAP|RFLOW))
        _c4err("missing terminating }");
    if(m_evt_handler->m_stack.size() > 1)
        _handle_indentation_pop(m_evt_handler->m_stack.begin());
    if(has_all(RDOC))
    {
        _end2_doc();
    }
    else if(has_all(RTOP|RUNK))
    {
        if(m_pending_anchors.num_entries || m_pending_tags.num_entries)
        {
            if(m_doc_empty)
            {
                m_evt_handler->begin_doc();
                _handle_annotations_before_blck_val_scalar();
                m_evt_handler->set_val_scalar_plain_empty();
                m_evt_handler->end_doc();
            }
        }
    }
    m_evt_handler->end_stream();
    if C4_UNLIKELY(m_has_directives)
        _c4err("directives cannot be used without a document");
}


template<class EventHandler>
void ParseEngine<EventHandler>::_handle_indentation_pop(ParserState const* popto)
{
    _c4dbgpf("popping {} level{}: from level {}(@ind={}) to level {}(@ind={})", m_evt_handler->m_curr->level - popto->level, (((m_evt_handler->m_curr->level - popto->level) > 1) ? "s" : ""), m_evt_handler->m_curr->level, m_evt_handler->m_curr->indref, popto->level, popto->indref);
    while(m_evt_handler->m_curr != popto)
    {
        if(has_any(RSEQ))
        {
            _c4dbgpf("popping seq at level {} (indentation={},addr={})", m_evt_handler->m_curr->level, m_evt_handler->m_curr->indref, m_evt_handler->m_curr);
            _end2_seq();
        }
        else if(has_any(RMAP))
        {
            _c4dbgpf("popping map at level {} (indentation={},addr={})", m_evt_handler->m_curr->level, m_evt_handler->m_curr->indref, m_evt_handler->m_curr);
            _end2_map();
        }
        else
        {
            break;
        }
    }
    _c4dbgpf("current level is {} (indentation={})", m_evt_handler->m_curr->level, m_evt_handler->m_curr->indref);
}

template<class EventHandler>
void ParseEngine<EventHandler>::_handle_indentation_pop_from_block_seq()
{
    // search the stack frame to jump to based on its indentation
    using state_type = typename EventHandler::state;
    state_type const* popto = nullptr;
    auto &stack = m_evt_handler->m_stack;
    RYML_ASSERT_PARSE_CB_(stack.m_callbacks, stack.is_contiguous(), m_evt_handler->m_curr->pos); // this search relies on the stack being contiguous
    RYML_ASSERT_PARSE_CB_(stack.m_callbacks, m_evt_handler->m_curr >= stack.begin() && m_evt_handler->m_curr < stack.end(), m_evt_handler->m_curr->pos);
    const size_t ind = m_evt_handler->m_curr->line_contents.indentation;
    #ifdef RYML_DBG
    _print_state_stack();
    #endif
    for(state_type const* s = m_evt_handler->m_curr-1; s >= stack.begin(); --s)
    {
        _c4dbgpf("searching for state with indentation {}. curr={} (level={},node={})", ind, s->indref, s->level, s->node_id);
        if(s->indref == ind)
        {
            _c4dbgpf("gotit!!! level={} node={}", s->level, s->node_id);
            popto = s;
            break;
        }
    }
    if(!popto || popto >= m_evt_handler->m_curr || popto->level >= m_evt_handler->m_curr->level)
    {
        _c4err("parse error: incorrect indentation?");
    }
    _handle_indentation_pop(popto);
}

template<class EventHandler>
void ParseEngine<EventHandler>::_handle_indentation_pop_from_block_map()
{
    // search the stack frame to jump to based on its indentation
    using state_type = typename EventHandler::state;
    auto &stack = m_evt_handler->m_stack;
    RYML_ASSERT_PARSE_CB_(stack.m_callbacks, stack.is_contiguous(), m_evt_handler->m_curr->pos); // this search relies on the stack being contiguous
    RYML_ASSERT_PARSE_CB_(stack.m_callbacks, m_evt_handler->m_curr >= stack.begin() && m_evt_handler->m_curr < stack.end(), m_evt_handler->m_curr->pos);
    const size_t ind = m_evt_handler->m_curr->line_contents.indentation;
    state_type const* popto = nullptr;
    #ifdef RYML_DBG
    char flagbuf_[128];
    _print_state_stack(flagbuf_);
    #endif
    for(state_type const* s = m_evt_handler->m_curr-1; s > stack.begin(); --s) // never go to the stack bottom. that's the root
    {
        _c4dbgpf("searching for state with indentation {}. current: ind={},level={},node={},flags={}", ind, s->indref, s->level, s->node_id, detail::_parser_flags_to_str(flagbuf_, s->flags));
        if(s->indref < ind)
        {
            break;
        }
        else if(s->indref == ind)
        {
            _c4dbgpf("same indentation!!! level={} node={}", s->level, s->node_id);
            if(popto && has_any(RTOP, s) && has_none(RMAP|RSEQ, s))
            {
                break;
            }
            popto = s;
            if(has_all(RSEQ|RBLCK, s))
            {
                csubstr rem = m_evt_handler->m_curr->line_contents.rem;
                const size_t first = rem.first_not_of(' ');
                RYML_ASSERT_PARSE_CB_(stack.m_callbacks, first == ind || first == npos, m_evt_handler->m_curr->pos);
                rem = rem.right_of(first, true);
                _c4dbgpf("indentless? rem='{}' first={}", rem, first);
                if(rem.begins_with('-') && _is_blck_token(rem))
                {
                    _c4dbgp("parent was indentless seq");
                    break;
                }
            }
        }
    }
    if(!popto || popto >= m_evt_handler->m_curr || popto->level >= m_evt_handler->m_curr->level)
    {
        _c4err("parse error: incorrect indentation?");
    }
    _handle_indentation_pop(popto);
}


//-----------------------------------------------------------------------------
template<class EventHandler>
void ParseEngine<EventHandler>::_check_valid_newline_in_quoted_scalar()
{
    if C4_UNLIKELY(has_all(RMAP|RBLCK|RKEY))
    {
        _c4err("multiline quoted keys are invalid");
    }
    else // check contextual indentation
    {
        const size_t minindent = m_evt_handler->m_curr->indref + ((has_any(RMAP|RSEQ) && has_any(RBLCK)));
        _c4dbgpf("indent={} vs minindent={} indref={}", m_evt_handler->m_curr->line_contents.indentation, minindent, m_evt_handler->m_curr->indref);
        if(m_evt_handler->m_curr->line_contents.indentation < minindent)
        {
            RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks,
                                m_evt_handler->m_curr->line_contents.indentation == m_evt_handler->m_curr->line_contents.rem.first_not_of(' '),
                                m_evt_handler->m_curr->pos);
            csubstr trimmed = m_evt_handler->m_curr->line_contents.rem.sub(m_evt_handler->m_curr->line_contents.indentation);
            _c4dbgpf("trimmed.len={} line={}", trimmed.len, prs_(m_evt_handler->m_curr->line_contents.rem, true));
            if C4_UNLIKELY(!!trimmed.len)
            {
                _c4err("bad indentation");
            }
        }
    }
}


//-----------------------------------------------------------------------------
template<class EventHandler>
ScannedScalar ParseEngine<EventHandler>::_scan_scalar_squot()
{
    // quoted scalars can spread over multiple lines!
    // nice explanation here: http://yaml-multiline.info/

    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, _buf().sub(m_evt_handler->m_curr->pos.offset).begins_with('\''), m_evt_handler->m_curr->pos);

    // a span to the end of the file, skipping the opening quote
    substr s = _buf().sub(m_evt_handler->m_curr->pos.offset + 1);
    _line_progressed(1); // advance over the opening quote
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, !m_evt_handler->m_curr->at_line_beginning(), m_evt_handler->m_curr->pos);

    bool needs_filter = false;
    size_t pos = npos; // find the pos of the matching quote
    while( ! _finished_file())
    {
        const csubstr line = m_evt_handler->m_curr->line_contents.rem;
        _c4dbgpf("scanning single quoted scalar @ line[{}]: {}", m_evt_handler->m_curr->pos.line, prs_(line));
        if C4_UNLIKELY(m_evt_handler->m_curr->at_line_beginning() && _is_doc_token(line))
            _c4err("token can not appear at line begin");
        for(size_t i = 0; i < line.len; ++i)
        {
            const char curr = line.str[i];
            if(curr == '\'') // single quotes are escaped with two single quotes
            {
                const char next = i+1 < line.len ? line.str[i+1] : '~';
                if(next != '\'') // so just look for the first quote
                {                // without another after it
                    _line_progressed(i + 1); // progress beyond the quote
                    pos = i + (size_t)(line.str - s.str); // set pos to before the quote
                    goto found_close;
                }
                else
                {
                    needs_filter = true; // needs filter to remove escaped quotes
                    ++i; // skip the escaped quote
                }
            }
        }

        needs_filter = true;
        _line_progressed(line.len);
        _line_ended();
        _scan_line();
        _check_valid_newline_in_quoted_scalar();
    }

    _c4err("reached end of file while looking for closing quote");

found_close:

    _c4dbgpf("found closing quote at: {}", pos);
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, pos != npos, m_evt_handler->m_curr->pos);
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, pos >= 0, m_evt_handler->m_curr->pos);
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, s.end() >= _buf().begin() && s.end() <= _buf().end(), m_evt_handler->m_curr->pos);
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, s.end() == _buf().end() || *s.end() == '\'', m_evt_handler->m_curr->pos);
    _set_first_strict(s, pos);

    _c4prscalar("scanned squoted scalar", s, /*keep_newlines*/true);

    return ScannedScalar { s, needs_filter };
}


//-----------------------------------------------------------------------------
template<class EventHandler>
ScannedScalar ParseEngine<EventHandler>::_scan_scalar_dquot()
{
    // quoted scalars can spread over multiple lines!
    // nice explanation here: http://yaml-multiline.info/

    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, _buf().sub(m_evt_handler->m_curr->pos.offset).begins_with('"'), m_evt_handler->m_curr->pos);

    // a span to the end of the file, skipping the opening quote
    substr s = _buf().sub(m_evt_handler->m_curr->pos.offset + 1);
    _line_progressed(1); // advance over the opening quote
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, !m_evt_handler->m_curr->at_line_beginning(), m_evt_handler->m_curr->pos);

    bool needs_filter = false;
    size_t pos = npos; // find the pos of the matching quote
    while( ! _finished_file())
    {
        #if defined(__GNUC__) && (/*__GNUC__ == 12 || */__GNUC__ == 13)
        C4_DONT_OPTIMIZE(m_evt_handler->m_curr->line_contents.rem); // prevent hoisting
        #endif
        csubstr rem = m_evt_handler->m_curr->line_contents.rem;
        _c4dbgpf("scanning double quoted scalar @ line[{}]:  line='{}'", m_evt_handler->m_curr->pos.line, rem);
        if C4_UNLIKELY(m_evt_handler->m_curr->at_line_beginning() && _is_doc_token(rem))
            _c4err("token can not appear at line begin");
        for(size_t i = 0; i < rem.len; ++i)
        {
            const char curr = rem.str[i];
            // every \ is an escape
            if(curr == '\\')
            {
                const char next = i+1 < rem.len ? rem.str[i+1] : '~';
                needs_filter = true;
                if(next == '"' || next == '\\')
                    ++i;
            }
            else if(curr == '"')
            {
                _line_progressed(i + 1); // progress beyond the quote
                pos = i + (size_t)(rem.str - s.str); // set pos to before the quote
                goto found_close;
            }
        }

        // leading whitespace also needs filtering
        needs_filter = true;
        _line_progressed(rem.len);
        _line_ended();
        _scan_line();
        _check_valid_newline_in_quoted_scalar();
    }

    _c4err("reached end of file while looking for closing quote");

found_close:

    _c4dbgpf("found closing quote at: {}", pos);
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, pos != npos, m_evt_handler->m_curr->pos);
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, pos >= 0, m_evt_handler->m_curr->pos);
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, s.end() >= _buf().begin() && s.end() <= _buf().end(), m_evt_handler->m_curr->pos);
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, s.end() == _buf().end() || *s.end() == '"', m_evt_handler->m_curr->pos);
    _set_first_strict(s, pos);

    _c4prscalar("scanned dquoted scalar", s, /*keep_newlines*/true);

    return ScannedScalar{s, needs_filter};
}


//-----------------------------------------------------------------------------
template<class EventHandler>
void ParseEngine<EventHandler>::_scan_block(ScannedBlock *C4_RESTRICT sb, size_t indref)
{
    _c4dbgpf("blck: indref={}", indref);
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, indref != npos, m_evt_handler->m_curr->pos);

    // nice explanation here: http://yaml-multiline.info/
    csubstr s = m_evt_handler->m_curr->line_contents.rem;
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, s.begins_with('|') || s.begins_with('>'), m_evt_handler->m_curr->pos);

    _c4dbgpf("blck: specs={}", prs_(s));

    // parse the spec
    BlockChomp_e chomp = CHOMP_CLIP; // default to clip unless + or - are used
    size_t indentation = npos; // have to find out if no spec is given
    if(s.len > 1)
    {
        RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, s.begins_with_any("|>"), m_evt_handler->m_curr->pos);
        csubstr t = s.sub(1);
        _c4dbgpf("blck: spec is multichar: {}", prs_(t));
        RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, t.len >= 1, m_evt_handler->m_curr->pos);
        size_t pos = t.first_of("-+");
        _c4dbgpf("blck: spec chomp char: pos={}", pos);
        if(pos != npos)
        {
            _c4dbgpf("blck: spec chomp char: {}", _c4prc(t[pos]));
            if(t[pos] == '-')
            {
                _c4dbgp("blck: chomp=STRIP");
                chomp = CHOMP_STRIP;
            }
            else if(t[pos] == '+')
            {
                _c4dbgp("blck: chomp=KEEP");
                chomp = CHOMP_KEEP;
            }
            if(pos == 0)
                t = t.sub(1);
            else
                t = t.first(pos);
            _c4dbgpf("blck: spec is now: {}", prs_(t));
        }
        // from here to the end, only digits are considered
        pos = t.first_not_of("0123456789");
        csubstr rest = t.first(pos);
        if( ! rest.empty())
        {
            _c4dbgpf("blck: parse indentation digits: {}", prs_(rest));
            if C4_UNLIKELY(rest.len > 1)
                _c4err("parse error: invalid indentation");
            if C4_UNLIKELY( ! c4::atou(rest, &indentation))
                _c4err("parse error: could not read indentation as decimal"); // LCOV_EXCL_LINE
            if C4_UNLIKELY( ! indentation)
                _c4err("parse error: null indentation");
            _c4dbgpf("blck: indentation specified: {}. add {} from curr state -> {}", indentation, m_evt_handler->m_curr->indref, indentation+indref);
            indentation += m_evt_handler->m_curr->indref;
        }
        else
        {
            rest = t.triml(" \t");
            _c4dbgpf("blck: digits empty. t={} trimmed={} iscomm={} t.iscomm={}", prs_(t), prs_(rest), rest.begins_with('#'), t.begins_with('#'));
            if C4_UNLIKELY(rest.len && (rest.str[0] != '#' || t.str[0] == '#'))
                _c4err("parse error: invalid token");
        }
    }

    _c4dbgpf("blck: style={}  chomp={}  indentation={}", s.begins_with('>') ? "fold" : "literal", chomp==CHOMP_CLIP ? "clip" : (chomp==CHOMP_STRIP ? "strip" : "keep"), indentation);

    // finish the current line
    _line_progressed(s.len);
    _line_ended();
    _scan_line();

    // start with a zero-length block, already pointing at the right place
    substr raw_block(_buf().data() + m_evt_handler->m_curr->pos.offset, size_t(0));
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, raw_block.begin() == m_evt_handler->m_curr->line_contents.full.str, m_evt_handler->m_curr->pos);

    // read every full line into a raw block,
    // from which newlines are to be stripped as needed.
    //
    // If no explicit indentation was given, pick it from the first
    // non-empty line. See
    // https://yaml.org/spec/1.2.2/#8111-block-indentation-indicator
    size_t num_lines = 0;
    size_t first = m_evt_handler->m_curr->pos.line;
    size_t provisional_indentation = npos;
    LineContents lc;
    while(( ! _finished_file()))
    {
        // peek next line, but do not advance immediately
        lc.reset_with_next_line(_buf(), m_evt_handler->m_curr->pos.offset);
        #if defined(__GNUC__) && (__GNUC__ == 12 || __GNUC__ == 13)
        C4_DONT_OPTIMIZE(lc.rem);
        #endif
        _c4dbgpf("blck: peeking at {}", prs_(lc.rem.trimr("\r\n"), true));
        // evaluate termination conditions
        if(indentation != npos)
        {
            _c4dbgpf("blck: indentation={}", indentation);
            // stop when the line is deindented and not empty
            if(lc.indentation < indentation && ( ! lc.rem.trim(" \t").empty()))
            {
                if(raw_block.len)
                {
                    _c4dbgpf("blck: indentation decreased ref={} thisline={}", indentation, lc.indentation);
                }
                else
                {
                    _c4err("indentation decreased without any scalar");
                }
                break;
            }
            else if(indentation == 0)
            {
                _c4dbgpf("blck: noindent. lc.rem={}", prs_(lc.rem));
                if(_is_doc_token(lc.rem))
                {
                    _c4dbgp("blck: stop. indentation=0 and doc ended");
                    break;
                }
            }
        }
        else
        {
            const size_t fns = lc.rem.first_not_of(' ');
            _c4dbgpf("blck: indentation ref not set. firstnonws={}", fns);
            if(fns != npos) // non-empty line
            {
                _c4dbgpf("blck: line not empty. indref={} indprov={} indentation={}", indref, provisional_indentation, lc.indentation);
                if C4_UNLIKELY(lc.full.begins_with('\t'))
                    _c4err("parse error");
                if(provisional_indentation == npos)
                {
                    if(lc.indentation < indref)
                    {
                        _c4dbgpf("blck: block terminated indentation={} < indref={}", lc.indentation, indref);
                        if(raw_block.len == 0)
                        {
                            _c4dbgp("blck: was empty, undo next line");
                            _line_ended_undo();
                        }
                        break;
                    }
                    else if(lc.indentation == m_evt_handler->m_curr->indref)
                    {
                        if(has_any(RSEQ|RMAP))
                        {
                            _c4dbgpf("blck: block terminated. reading container and indentation={}==indref={}", lc.indentation, m_evt_handler->m_curr->indref);
                            break;
                        }
                    }
                    _c4dbgpf("blck: set indentation ref from this line: ref={}", lc.indentation);
                    indentation = lc.indentation;
                }
                else
                {
                    if(lc.indentation >= provisional_indentation)
                    {
                        _c4dbgpf("blck: set indentation ref from provisional indentation: provisional_ref={}, thisline={}", provisional_indentation, lc.indentation);
                        //indentation = provisional_indentation ? provisional_indentation : lc.indentation;
                        indentation = lc.indentation;
                    }
                    else
                    {
                        if(lc.indentation >= indref)
                            _c4err("parse error: first non-empty block line should have at least the original indentation");
                        _c4dbgp("blck: finished");
                        break;
                    }
                }
            }
            else // empty line
            {
                _c4dbgpf("blck: line empty or {} spaces. line_indentation={} prov_indentation={}", lc.rem.len, lc.indentation, provisional_indentation);
                if(provisional_indentation != npos)
                {
                    if(lc.rem.len >= provisional_indentation)
                    {
                        _c4dbgpf("blck: increase provisional_ref {} -> {}", provisional_indentation, lc.rem.len);
                        provisional_indentation = lc.rem.len;
                    }
                }
                else
                {
                    provisional_indentation = lc.indentation ? lc.indentation : has_any(RSEQ|RVAL);
                    _c4dbgpf("blck: initialize provisional_ref={}", provisional_indentation);
                    if(provisional_indentation == npos)
                    {
                        provisional_indentation = lc.rem.len ? lc.rem.len : has_any(RSEQ|RVAL);
                        _c4dbgpf("blck: initialize provisional_ref={}", provisional_indentation);
                    }
                    if(provisional_indentation < indref)
                    {
                        provisional_indentation = indref;
                        _c4dbgpf("blck: initialize provisional_ref={}", provisional_indentation);
                    }
                }
            }
        }
        // advance now that we know the folded scalar continues
        m_evt_handler->m_curr->line_contents = lc;
        _c4dbgpf("blck: append '{}'", m_evt_handler->m_curr->line_contents.rem);
        raw_block.len += m_evt_handler->m_curr->line_contents.full.len;
        _line_progressed(m_evt_handler->m_curr->line_contents.rem.len);
        _line_ended();
        ++num_lines;
    }
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, m_evt_handler->m_curr->pos.line == (first + num_lines) || (raw_block.len == 0), m_evt_handler->m_curr->pos);
    C4_UNUSED(num_lines);
    C4_UNUSED(first);

    if(indentation == npos)
    {
        _c4dbgpf("blck: set indentation from provisional: {}", provisional_indentation);
        indentation = provisional_indentation;
    }

    if(num_lines)
        _line_ended_undo();

    _c4prscalar("scanned block", raw_block, /*keep_newlines*/true);

    sb->scalar = raw_block;
    sb->indentation = indentation;
    sb->chomp = chomp;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
/** @cond dev */

// a debugging scaffold:
#if 0
#define _c4dbgfws(fmt, ...) _c4dbgpf("filt_ws[{}->{}]: " fmt, proc.rpos, proc.wpos, __VA_ARGS__)
#else
#define _c4dbgfws(...)
#endif

template<class EventHandler>
template<class FilterProcessor>
bool ParseEngine<EventHandler>::_filter_ws_handle_to_first_non_space(FilterProcessor &proc)
{
    _c4dbgfws("found whitespace '{}'", _c4prc(proc.curr()));
    RYML_ASSERT_PARSE_CB_(this->callbacks(), proc.curr() == ' ' || proc.curr() == '\t', m_evt_handler->m_curr->pos);

    const size_t first_pos = proc.rpos > 0 ? proc.src.first_not_of(" \t", proc.rpos) : proc.src.first_not_of(' ', proc.rpos);
    if(first_pos != npos)
    {
        const char first_char = proc.src[first_pos];
        _c4dbgfws("firstnonws='{}'@{}", _c4prc(first_char), first_pos);
        if(first_char == '\n' || first_char == '\r') // skip trailing whitespace
        {
            _c4dbgfws("whitespace is trailing on line", "");
            proc.skip(first_pos - proc.rpos);
        }
        else // a legit whitespace
        {
            proc.copy();
            _c4dbgfws("legit whitespace. sofar={}", prs_(proc.sofar()));
        }
        return true;
    }
    _c4dbgfws("whitespace is trailing on line", "");
    return false;
}

template<class EventHandler>
template<class FilterProcessor>
void ParseEngine<EventHandler>::_filter_ws_copy_trailing(FilterProcessor &proc)
{
    if(!_filter_ws_handle_to_first_non_space(proc))
    {
        _c4dbgfws("... everything else is trailing whitespace - copy {} chars", proc.src.len - proc.rpos);
        proc.copy(proc.src.len - proc.rpos);
    }
}

template<class EventHandler>
template<class FilterProcessor>
void ParseEngine<EventHandler>::_filter_ws_skip_trailing(FilterProcessor &proc)
{
    if(!_filter_ws_handle_to_first_non_space(proc))
    {
        _c4dbgfws("... everything else is trailing whitespace - skip {} chars", proc.src.len - proc.rpos);
        proc.skip(proc.src.len - proc.rpos);
    }
}

#undef _c4dbgfws


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
/* plain scalars */

// a debugging scaffold:
#if 0
#define _c4dbgfps(fmt, ...) _c4dbgpf("filt_plain[{}->{}]: " fmt, proc.rpos, proc.wpos, __VA_ARGS__)
#else
#define _c4dbgfps(fmt, ...)
#endif

template<class EventHandler>
template<class FilterProcessor>
void ParseEngine<EventHandler>::_filter_nl_plain(FilterProcessor &C4_RESTRICT proc, size_t indentation)
{
    RYML_ASSERT_PARSE_CB_(this->callbacks(), proc.curr() == '\n', m_evt_handler->m_curr->pos);

    _c4dbgfps("found newline. sofar={}", prs_(proc.sofar()));
    size_t ii = proc.rpos;
    const size_t numnl_following = _count_following_newlines(proc.src, &ii, indentation);
    if(numnl_following)
    {
        proc.set('\n', numnl_following);
        _c4dbgfps("{} consecutive (empty) lines {}. totalws={}", 1+numnl_following, ii < proc.src.len ? "in the middle" : "at the end", proc.rpos-ii);
    }
    else
    {
        const size_t ret = proc.src.first_not_of(" \t", proc.rpos+1);
        if(ret != npos)
        {
            proc.set(' ');
             _c4dbgfps("single newline. convert to space. ret={}/{}. sofar={}", ii, proc.src.len, prs_(proc.sofar()));
        }
        else
        {
            _c4dbgfps("last newline, everything else is whitespace. ii={}/{}", ii, proc.src.len);
            ii = proc.src.len;
        }
    }
    proc.rpos = ii;
}

template<class EventHandler>
template<class FilterProcessor>
auto ParseEngine<EventHandler>::_filter_plain(FilterProcessor &C4_RESTRICT proc, size_t indentation) -> decltype(proc.result())
{
    RYML_ASSERT_PARSE_CB_(this->callbacks(), indentation != npos, m_evt_handler->m_curr->pos);
    _c4dbgfps("before={}", prs_(proc.src));

    while(proc.has_more_chars())
    {
        const char curr = proc.curr();
        _c4dbgfps("'{}', sofar={}", _c4prc(curr), prs_(proc.sofar()));
        switch(curr)
        {
        case ' ':
        RYML_WITH_TAB_TOKENS_(case '\t':)
            _c4dbgfps("whitespace", curr);
            _filter_ws_skip_trailing(proc);
            break;
        case '\n':
            _c4dbgfps("newline", curr);
            _filter_nl_plain(proc, /*indentation*/indentation);
            break;
        case '\r':  // skip \r --- https://stackoverflow.com/questions/1885900
            _c4dbgfps("carriage return, ignore", curr);
            proc.skip();
            break;
        default:
            proc.copy();
            break;
        }
    }

    _c4dbgfps("after={}", prs_(proc.sofar()));

    return proc.result();
}

#undef _c4dbgfps


template<class EventHandler>
FilterResult ParseEngine<EventHandler>::filter_scalar_plain(csubstr scalar, substr dst, size_t indentation)
{
    FilterProcessorSrcDst proc(scalar, dst);
    return _filter_plain(proc, indentation);
}

template<class EventHandler>
FilterResult ParseEngine<EventHandler>::filter_scalar_plain_in_place(substr dst, size_t cap, size_t indentation)
{
    FilterProcessorInplaceEndExtending proc(dst, cap);
    return _filter_plain(proc, indentation);
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
/* single quoted */

// a debugging scaffold:
#if 0
#define _c4dbgfsq(fmt, ...) _c4dbgpf("filt_squo[{}->{}]: " fmt, proc.rpos, proc.wpos, __VA_ARGS__)
#else
#define _c4dbgfsq(fmt, ...)
#endif

template<class EventHandler>
template<class FilterProcessor>
void ParseEngine<EventHandler>::_filter_nl_squoted(FilterProcessor &C4_RESTRICT proc)
{
    RYML_ASSERT_PARSE_CB_(this->callbacks(), proc.curr() == '\n', m_evt_handler->m_curr->pos);

    _c4dbgfsq("found newline. sofar={}", prs_(proc.sofar()));
    size_t ii = proc.rpos;
    const size_t numnl_following = _count_following_newlines(proc.src, &ii);
    if(numnl_following)
    {
        proc.set('\n', numnl_following);
        _c4dbgfsq("{} consecutive (empty) lines {}. totalws={}", 1+numnl_following, ii < proc.src.len ? "in the middle" : "at the end", proc.rpos-ii);
    }
    else
    {
        const size_t ret = proc.src.first_not_of(" \t", proc.rpos+1);
        if(ret != npos)
        {
            proc.set(' ');
            _c4dbgfsq("single newline. convert to space. ret={}/{}. sofar={}", ii, proc.src.len, prs_(proc.sofar()));
        }
        else
        {
            proc.set(' ');
            _c4dbgfsq("single newline. convert to space. ii={}/{}. sofar={}", ii, proc.src.len, prs_(proc.sofar()));
        }
    }
    proc.rpos = ii;
}

template<class EventHandler>
template<class FilterProcessor>
auto ParseEngine<EventHandler>::_filter_squoted(FilterProcessor &C4_RESTRICT proc) -> decltype(proc.result())
{
    _c4dbgfsq("before={}", prs_(proc.src));

    // from the YAML spec for double-quoted scalars:
    // https://yaml.org/spec/1.2-old/spec.html#style/flow/single-quoted
    while(proc.has_more_chars())
    {
        const char curr = proc.curr();
        _c4dbgfsq("'{}', sofar={}", _c4prc(curr), prs_(proc.sofar()));
        switch(curr)
        {
        case ' ':
        case '\t':
            _c4dbgfsq("whitespace", curr);
            _filter_ws_copy_trailing(proc);
            break;
        case '\n':
            _c4dbgfsq("newline", curr);
            _filter_nl_squoted(proc);
            break;
        case '\r':  // skip \r --- https://stackoverflow.com/questions/1885900
            _c4dbgfsq("skip cr", curr);
            proc.skip();
            break;
        case '\'':
            _c4dbgfsq("squote", curr);
            if(proc.next() == '\'')
            {
                _c4dbgfsq("two consecutive squotes", curr);
                proc.skip();
                proc.copy();
            }
            else
            {
                _c4err("filter error");
            }
            break;
        default:
            proc.copy();
            break;
        }
    }

    _c4dbgfsq(": #filteredchars={} after={}", proc.src.len-proc.sofar().len, prs_(proc.sofar()));

    return proc.result();
}

#undef _c4dbgfsq

template<class EventHandler>
FilterResult ParseEngine<EventHandler>::filter_scalar_squoted(csubstr scalar, substr dst)
{
    FilterProcessorSrcDst proc(scalar, dst);
    return _filter_squoted(proc);
}

template<class EventHandler>
FilterResult ParseEngine<EventHandler>::filter_scalar_squoted_in_place(substr dst, size_t cap)
{
    FilterProcessorInplaceEndExtending proc(dst, cap);
    return _filter_squoted(proc);
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
/* double quoted */

// a debugging scaffold:
#if 0
#define _c4dbgfdq(fmt, ...) _c4dbgpf("filt_dquo[{}->{}]: " fmt, proc.rpos, proc.wpos, __VA_ARGS__)
#else
#define _c4dbgfdq(...)
#endif

template<class EventHandler>
template<class FilterProcessor>
void ParseEngine<EventHandler>::_filter_nl_dquoted(FilterProcessor &C4_RESTRICT proc)
{
    RYML_ASSERT_PARSE_CB_(this->callbacks(), proc.curr() == '\n', m_evt_handler->m_curr->pos);

    _c4dbgfdq("found newline. sofar={}", prs_(proc.sofar()));
    size_t ii = proc.rpos;
    const size_t numnl_following = _count_following_newlines(proc.src, &ii);
    if(numnl_following)
    {
        proc.set('\n', numnl_following);
        _c4dbgfdq("{} consecutive (empty) lines {}. totalws={}", 1+numnl_following, ii < proc.src.len ? "in the middle" : "at the end", proc.rpos-ii);
    }
    else
    {
        const size_t ret = proc.src.first_not_of(" \t", proc.rpos+1);
        if(ret != npos)
        {
            proc.set(' ');
            _c4dbgfdq("single newline. convert to space. ret={}/{}. sofar={}", ii, proc.src.len, prs_(proc.sofar()));
        }
        else
        {
            proc.set(' ');
            _c4dbgfdq("single newline. convert to space. ii={}/{}. sofar={}", ii, proc.src.len, prs_(proc.sofar()));
        }
        if(ii < proc.src.len && proc.src.str[ii] == '\\')
        {
            _c4dbgfdq("backslash at [{}]", ii);
            const char next = ii+1 < proc.src.len ? proc.src.str[ii+1] : '\0';
            if(next == ' ' || next == '\t')
            {
                _c4dbgfdq("extend skip to backslash", "");
                ++ii;
            }
        }
    }
    proc.rpos = ii;
}

template<class EventHandler>
template<class FilterProcessor>
void ParseEngine<EventHandler>::_filter_dquoted_backslash_decode(FilterProcessor &C4_RESTRICT proc, size_t sz)
{
    const size_t szp1 = sz + 1u;
    if C4_UNLIKELY(proc.rpos + szp1 >= proc.src.len)
        _c4err("codepoint requires {} hex digits. scalar pos={}", sz, proc.rpos);
    char readbuf[8];
    csubstr codepoint = proc.src.sub(proc.rpos + 2u, sz);
    _c4dbgfdq("utf8 ~~~{}~~~ rpos={} rem=~~~{}~~~", codepoint, proc.rpos, proc.src.sub(proc.rpos));
    uint32_t codepoint_val = {};
    if C4_UNLIKELY(!read_hex(codepoint, &codepoint_val))
        _c4err("failed to parse codepoint. scalar pos={}", proc.rpos);
    const size_t numbytes = decode_code_point((uint8_t*)readbuf, sizeof(readbuf), codepoint_val);
    if C4_UNLIKELY(numbytes == 0)
        _c4err("failed to decode code point={}", proc.rpos);
    RYML_ASSERT_PARSE_CB_(callbacks(), numbytes <= 4, m_evt_handler->m_curr->pos);
    proc.translate_esc_bulk(readbuf, numbytes, /*nread*/szp1);
    _c4dbgfdq("utf8 after rpos={} rem=~~~{}~~~", proc.rpos, proc.src.sub(proc.rpos));
}

template<class EventHandler>
template<class FilterProcessor>
void ParseEngine<EventHandler>::_filter_dquoted_backslash(FilterProcessor &C4_RESTRICT proc)
{
    char next = proc.next();
    _c4dbgfdq("backslash, next='{}'", _c4prc(next));
    if(next == '\r')
    {
        if(proc.rpos+2 < proc.src.len && proc.src.str[proc.rpos+2] == '\n')
        {
            proc.skip(); // newline escaped with \ -- skip both (add only one as i is loop-incremented)
            next = '\n';
            _c4dbgfdq("[{}]: was \\r\\n, now next='\\n'", proc.rpos);
        }
    }

    if(next == '\n')
    {
        size_t ii = proc.rpos + 2;
        for( ; ii < proc.src.len; ++ii)
        {
            // skip leading whitespace
            if(proc.src.str[ii] == ' ' || proc.src.str[ii] == '\t')
                ;
            else
                break;
        }
        proc.skip(ii - proc.rpos);
    }
    else if(next == '"' || next == '/' || next == ' ' || next == '\t')
    {
        // escapes for json compatibility
        proc.translate_esc(next);
        _c4dbgfdq("here, used '{}'", _c4prc(next));
    }
    else if(next == '\r')
    {
        proc.skip();
    }
    else if(next == 'n')
    {
        proc.translate_esc('\n');
    }
    else if(next == 'r')
    {
        proc.translate_esc('\r');
    }
    else if(next == 't')
    {
        proc.translate_esc('\t');
    }
    else if(next == '\\')
    {
        proc.translate_esc('\\');
    }
    else if(next == 'x') // 2-digit Unicode escape (\xXX), code point 0x00–0xFF
    {
        _filter_dquoted_backslash_decode(proc, 2u);
    }
    else if(next == 'u') // 4-digit Unicode escape (\uXXXX), code point 0x0000–0xFFFF
    {
        _filter_dquoted_backslash_decode(proc, 4u);
    }
    else if(next == 'U') // 8-digit Unicode escape (\UXXXXXXXX), full 32-bit code point
    {
        _filter_dquoted_backslash_decode(proc, 8u);
    }
    // https://yaml.org/spec/1.2.2/#rule-c-ns-esc-char
    else if(next == '0')
    {
        proc.translate_esc('\0');
    }
    else if(next == 'b') // backspace
    {
        proc.translate_esc('\b');
    }
    else if(next == 'f') // form feed
    {
        proc.translate_esc('\f');
    }
    else if(next == 'a') // bell character
    {
        proc.translate_esc('\a');
    }
    else if(next == 'v') // vertical tab
    {
        proc.translate_esc('\v');
    }
    else if(next == 'e') // escape character
    {
        proc.translate_esc('\x1b');
    }
    else if(next == '_') // unicode non breaking space \u00a0
    {
        // https://www.compart.com/en/unicode/U+00a0
        const char payload[] = {
            RYML_CHCONST_(-0x3e, 0xc2),
            RYML_CHCONST_(-0x60, 0xa0),
        };
        proc.translate_esc_bulk(payload, /*nwrite*/2, /*nread*/1);
    }
    else if(next == 'N') // unicode next line \u0085
    {
        // https://www.compart.com/en/unicode/U+0085
        const char payload[] = {
            RYML_CHCONST_(-0x3e, 0xc2),
            RYML_CHCONST_(-0x7b, 0x85),
        };
        proc.translate_esc_bulk(payload, /*nwrite*/2, /*nread*/1);
    }
    else if(next == 'L') // unicode line separator \u2028
    {
        // https://www.utf8-chartable.de/unicode-utf8-table.pl?start=8192&number=1024&names=-&utf8=0x&unicodeinhtml=hex
        const char payload[] = {
            RYML_CHCONST_(-0x1e, 0xe2),
            RYML_CHCONST_(-0x80, 0x80),
            RYML_CHCONST_(-0x58, 0xa8),
        };
        proc.translate_esc_extending(payload, /*nwrite*/3, /*nread*/1);
    }
    else if(next == 'P') // unicode paragraph separator \u2029
    {
        // https://www.utf8-chartable.de/unicode-utf8-table.pl?start=8192&number=1024&names=-&utf8=0x&unicodeinhtml=hex
        const char payload[] = {
            RYML_CHCONST_(-0x1e, 0xe2),
            RYML_CHCONST_(-0x80, 0x80),
            RYML_CHCONST_(-0x57, 0xa9),
        };
        proc.translate_esc_extending(payload, /*nwrite*/3, /*nread*/1);
    }
    else if(next == '\0')
    {
        proc.skip();
    }
    else
    {
        _c4err("unknown character '{}' after '\\' pos={}", _c4prc(next), proc.rpos);
    }
    _c4dbgfdq("backslash...sofar={}", prs_(proc.sofar()));
}


template<class EventHandler>
template<class FilterProcessor>
auto ParseEngine<EventHandler>::_filter_dquoted(FilterProcessor &C4_RESTRICT proc) -> decltype(proc.result())
{
    _c4dbgfdq("before={}", prs_(proc.src));
    // from the YAML spec for double-quoted scalars:
    // https://yaml.org/spec/1.2-old/spec.html#style/flow/double-quoted
    while(proc.has_more_chars())
    {
        const char curr = proc.curr();
        _c4dbgfdq("'{}' sofar={}", _c4prc(curr), prs_(proc.sofar()));
        switch(curr)
        {
        case ' ':
        case '\t':
        {
            _c4dbgfdq("whitespace", curr);
            _filter_ws_copy_trailing(proc);
            break;
        }
        case '\n':
        {
            _c4dbgfdq("newline", curr);
            _filter_nl_dquoted(proc);
            break;
        }
        case '\r':  // skip \r --- https://stackoverflow.com/questions/1885900
        {
            _c4dbgfdq("carriage return, ignore", curr);
            proc.skip();
            break;
        }
        case '\\':
        {
            _filter_dquoted_backslash(proc);
            break;
        }
        default:
        {
            proc.copy();
            break;
        }
        }
    }
    _c4dbgfdq("after={}", prs_(proc.sofar()));
    return proc.result();
}

#undef _c4dbgfdq


template<class EventHandler>
FilterResult ParseEngine<EventHandler>::filter_scalar_dquoted(csubstr scalar, substr dst)
{
    FilterProcessorSrcDst proc(scalar, dst);
    return _filter_dquoted(proc);
}

template<class EventHandler>
FilterResultExtending ParseEngine<EventHandler>::filter_scalar_dquoted_in_place(substr dst, size_t cap)
{
    FilterProcessorInplaceMidExtending proc(dst, cap);
    return _filter_dquoted(proc);
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// block filtering helpers

template<class EventHandler>
template<class FilterProcessor>
void ParseEngine<EventHandler>::_filter_chomp(FilterProcessor &C4_RESTRICT proc, BlockChomp_e chomp, size_t indentation)
{
    RYML_ASSERT_PARSE_CB_(this->callbacks(), chomp == CHOMP_CLIP || chomp == CHOMP_KEEP || chomp == CHOMP_STRIP, m_evt_handler->m_curr->pos);
    RYML_ASSERT_PARSE_CB_(this->callbacks(), proc.rem().first_not_of(" \n\r") == npos, m_evt_handler->m_curr->pos);

    // a debugging scaffold:
    #if 0
    #define _c4dbgchomp(fmt, ...) _c4dbgpf("chomp[{}->{}]: " fmt, proc.rpos, proc.wpos, __VA_ARGS__)
    #else
    #define _c4dbgchomp(...)
    #endif

    // advance to the last line having spaces beyond the indentation
    {
        size_t last = _find_last_newline_and_larger_indentation(proc.rem(), indentation);
        if(last != npos)
        {
            _c4dbgchomp("found newline and larger indentation. last={}", last);
            last = proc.rpos + last + size_t(1) + indentation;  // last started at to-be-read.
            RYML_ASSERT_PARSE_CB_(this->callbacks(), last <= proc.src.len, m_evt_handler->m_curr->pos);
            // remove indentation spaces, copy the rest
            while((proc.rpos < last) && proc.has_more_chars())
            {
                const char curr = proc.curr();
                _c4dbgchomp("curr='{}'", _c4prc(curr));
                switch(curr)
                {
                case '\n':
                    {
                        _c4dbgchomp("newline! remlen={}", proc.rem().len);
                        proc.copy();
                        // are there spaces after the newline?
                        csubstr at_next_line = proc.rem();
                        if(at_next_line.begins_with(' '))
                        {
                            _c4dbgchomp("next line begins with spaces. indentation={}", indentation);
                            // there are spaces.
                            size_t first_non_space = at_next_line.first_not_of(' ');
                            _c4dbgchomp("first_non_space={}", first_non_space);
                            if(first_non_space == npos)
                            {
                                _c4dbgchomp("{} spaces, to the end", at_next_line.len);
                                first_non_space = at_next_line.len;
                            }
                            if(first_non_space <= indentation)
                            {
                                _c4dbgchomp("skip spaces={}<=indentation={}", first_non_space, indentation);
                                proc.skip(first_non_space);
                            }
                            else
                            {
                                _c4dbgchomp("skip indentation={}<spaces={}", indentation, first_non_space);
                                proc.skip(indentation);
                                // copy the spaces after the indentation
                                _c4dbgchomp("copy {}={}-{} spaces", first_non_space - indentation, first_non_space, indentation);
                                proc.copy(first_non_space - indentation);
                            }
                        }
                        break;
                    }
                case '\r':
                    proc.skip();
                    break;
                }
            }
        }
    }

    // from now on, we only have line ends (or indentation spaces)
    switch(chomp)
    {
    case CHOMP_CLIP:
    {
        bool had_one = false;
        while(proc.has_more_chars())
        {
            const char curr = proc.curr();
            _c4dbgchomp("CLIP: '{}'", _c4prc(curr));
            switch(curr)
            {
            case '\n':
            {
                _c4dbgchomp("copy newline!", curr);
                proc.copy();
                proc.set_at_end();
                had_one = true;
                break;
            }
            case ' ':
            case '\r':
                _c4dbgchomp("skip!", curr);
                proc.skip();
                break;
            }
        }
        if(!had_one) // there were no newline characters. add one.
        {
            _c4dbgchomp("chomp=CLIP: add missing newline @{}", proc.wpos);
            proc.set('\n');
        }
        break;
    }
    case CHOMP_KEEP:
    {
        _c4dbgchomp("chomp=KEEP: copy all remaining new lines of {} characters", proc.rem().len);
        while(proc.has_more_chars())
        {
            const char curr = proc.curr();
            _c4dbgchomp("KEEP: '{}'", _c4prc(curr));
            switch(curr)
            {
            case '\n':
                _c4dbgchomp("copy newline!", curr);
                proc.copy();
                break;
            case ' ':
            case '\r':
                _c4dbgchomp("skip!", curr);
                proc.skip();
                break;
            }
        }
        break;
    }
    case CHOMP_STRIP:
    {
        _c4dbgchomp("chomp=STRIP: strip {} characters", proc.rem().len);
        // nothing to do!
        break;
    }
    }

    #undef _c4dbgchomp
}


// a debugging scaffold:
#if 0
#define _c4dbgfb(fmt, ...) _c4dbgpf("filt_block[{}->{}]: " fmt, proc.rpos, proc.wpos, __VA_ARGS__)
#else
#define _c4dbgfb(...)
#endif

template<class EventHandler>
template<class FilterProcessor>
void ParseEngine<EventHandler>::_filter_block_indentation(FilterProcessor &C4_RESTRICT proc, size_t indentation)
{
    csubstr rem = proc.rem(); // remaining
    if(rem.len)
    {
        size_t first = rem.first_not_of(' ');
        if(first != npos)
        {
            _c4dbgfb("{} spaces follow before next nonws character", first);
            if(first < indentation)
            {
                _c4dbgfb("skip {}<{} spaces from indentation", first, indentation);
                proc.skip(first);
            }
            else
            {
                _c4dbgfb("skip {} spaces from indentation", indentation);
                proc.skip(indentation);
            }
        }
        #ifdef RYML_NO_COVERAGE__TO_BE_DELETED
        else
        {
            _c4dbgfb("all spaces to the end: {} spaces", first);
            first = rem.len;
            if(first)
            {
                if(first < indentation)
                {
                    _c4dbgfb("skip everything", first);
                    proc.skip(proc.src.len - proc.rpos);
                }
                else
                {
                    _c4dbgfb("skip {} spaces from indentation", indentation);
                    proc.skip(indentation);
                }
            }
        }
        #endif
    }
}

template<class EventHandler>
template<class FilterProcessor>
size_t ParseEngine<EventHandler>::_handle_all_whitespace(FilterProcessor &C4_RESTRICT proc, BlockChomp_e chomp)
{
    csubstr contents = proc.src.trimr(" \n\r");
    _c4dbgfb("ws: contents_len={} wslen={}", contents.len, proc.src.len-contents.len);
    if(!contents.len)
    {
        _c4dbgfb("ws: all whitespace: len={}", proc.src.len);
        if(chomp == CHOMP_KEEP && proc.src.len)
        {
            _c4dbgfb("ws: chomp=KEEP all {} newlines", proc.src.count('\n'));
            while(proc.has_more_chars())
            {
                const char curr = proc.curr();
                if(curr == '\n')
                    proc.copy();
                else
                    proc.skip();
            }
            if(!proc.wpos)
            {
                proc.set('\n');
            }
        }
    }
    return contents.len;
}

template<class EventHandler>
template<class FilterProcessor>
size_t ParseEngine<EventHandler>::_extend_to_chomp(FilterProcessor &C4_RESTRICT proc, size_t contents_len)
{
    _c4dbgfb("contents_len={}", contents_len);

    RYML_ASSERT_PARSE_CB_(this->callbacks(), contents_len > 0u, m_evt_handler->m_curr->pos);

    // extend contents to just before the first newline at the end,
    // in case it is preceded by spaces
    size_t firstnewl = proc.src.first_of('\n', contents_len);
    if(firstnewl != npos)
    {
        contents_len = firstnewl;
        _c4dbgfb("contents_len={}  <--- firstnewl={}", contents_len, firstnewl);
    }
    else
    {
        contents_len = proc.src.len;
        _c4dbgfb("contents_len={}  <--- src.len={}", contents_len, proc.src.len);
    }

    return contents_len;
}

#undef _c4dbgfb


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

// a debugging scaffold:
#if 0
#define _c4dbgfbl(fmt, ...) _c4dbgpf("filt_block_lit[{}->{}]: " fmt, proc.rpos, proc.wpos, __VA_ARGS__)
#else
#define _c4dbgfbl(...)
#endif

template<class EventHandler>
template<class FilterProcessor>
auto ParseEngine<EventHandler>::_filter_block_literal(FilterProcessor &C4_RESTRICT proc, size_t indentation, BlockChomp_e chomp) -> decltype(proc.result())
{
    _c4dbgfbl("indentation={} before={}", indentation, prs_(proc.src));

    size_t contents_len = _handle_all_whitespace(proc, chomp);
    if(!contents_len)
        return proc.result();

    contents_len = _extend_to_chomp(proc, contents_len);

    _c4dbgfbl("to filter={}", prs_(proc.src.first(contents_len)));

    _filter_block_indentation(proc, indentation);

    // now filter the bulk
    while(proc.has_more_chars(/*maxpos*/contents_len))
    {
        const char curr = proc.curr();
        _c4dbgfbl("'{}' sofar={}",  _c4prc(curr), prs_(proc.sofar()));
        switch(curr)
        {
        case '\n':
        {
            _c4dbgfbl("found newline. skip indentation on the next line", curr);
            proc.copy();  // copy the newline
            _filter_block_indentation(proc, indentation);
            break;
        }
        case '\r':
            proc.skip();
            break;
        default:
            proc.copy();
            break;
        }
    }

    _c4dbgfbl("before chomp: #tochomp={}   sofar={}", proc.rem().len, prs_(proc.sofar()));

    _filter_chomp(proc, chomp, indentation);

    _c4dbgfbl("final={}", prs_(proc.sofar()));

    return proc.result();
}

#undef _c4dbgfbl

template<class EventHandler>
FilterResult ParseEngine<EventHandler>::filter_scalar_block_literal(csubstr scalar, substr dst, size_t indentation, BlockChomp_e chomp)
{
    FilterProcessorSrcDst proc(scalar, dst);
    return _filter_block_literal(proc, indentation, chomp);
}

template<class EventHandler>
FilterResult ParseEngine<EventHandler>::filter_scalar_block_literal_in_place(substr scalar, size_t cap, size_t indentation, BlockChomp_e chomp)
{
    FilterProcessorInplaceEndExtending proc(scalar, cap);
    return _filter_block_literal(proc, indentation, chomp);
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

// a debugging scaffold:
#if 0
#define _c4dbgfbf(fmt, ...) _c4dbgpf("filt_block_folded[{}->{}]: " fmt, proc.rpos, proc.wpos, __VA_ARGS__)
#else
#define _c4dbgfbf(...)
#endif


template<class EventHandler>
template<class FilterProcessor>
void ParseEngine<EventHandler>::_filter_block_folded_newlines_leading(FilterProcessor &C4_RESTRICT proc, size_t indentation, size_t len)
{
    _filter_block_indentation(proc, indentation);
    while(proc.has_more_chars(len))
    {
        const char curr = proc.curr();
        _c4dbgfbf("'{}' sofar={}",  _c4prc(curr), prs_(proc.sofar()));
        switch(curr)
        {
        case '\n':
            _c4dbgfbf("newline.", curr);
            proc.copy();
            _filter_block_indentation(proc, indentation);
            break;
        case '\r':
            proc.skip();
            break;
        case ' ':
        case '\t':
        {
            size_t first = proc.rem().first_not_of(" \t");
            _c4dbgfbf("space. first={}", first);
            if(first == npos)
                first = proc.rem().len;
            _c4dbgfbf("... indentation increased to {}",  first);
            _filter_block_folded_indented_block(proc, indentation, len, first);
            break;
        }
        default:
            _c4dbgfbf("newl leading: not space, not newline. stop.", 0);
            return;
        }
    }
}

template<class EventHandler>
template<class FilterProcessor>
size_t ParseEngine<EventHandler>::_filter_block_folded_newlines_compress(FilterProcessor &C4_RESTRICT proc, size_t num_newl, size_t wpos_at_first_newl)
{
    switch(num_newl)
    {
    case 1u:
        _c4dbgfbf("... this is the first newline. turn into space. wpos={}", proc.wpos);
        wpos_at_first_newl = proc.wpos;
        proc.skip();
        proc.set(' ');
        break;
    case 2u:
        _c4dbgfbf("... this is the second newline. prev space (at wpos={}) must be newline", wpos_at_first_newl);
        RYML_ASSERT_PARSE_CB_(this->callbacks(), wpos_at_first_newl != npos, m_evt_handler->m_curr->pos);
        RYML_ASSERT_PARSE_CB_(this->callbacks(), proc.sofar()[wpos_at_first_newl] == ' ', m_evt_handler->m_curr->pos);
        RYML_ASSERT_PARSE_CB_(this->callbacks(), wpos_at_first_newl + 1u == proc.wpos, m_evt_handler->m_curr->pos);
        proc.skip();
        proc.set_at(wpos_at_first_newl, '\n');
        RYML_ASSERT_PARSE_CB_(this->callbacks(), proc.sofar()[wpos_at_first_newl] == '\n', m_evt_handler->m_curr->pos);
        break;
    default:
        _c4dbgfbf("... subsequent newline (num_newl={}). copy", num_newl);
        proc.copy();
        break;
    }
    return wpos_at_first_newl;
}

template<class EventHandler>
template<class FilterProcessor>
void ParseEngine<EventHandler>::_filter_block_folded_newlines(FilterProcessor &C4_RESTRICT proc, size_t indentation, size_t len)
{
    RYML_ASSERT_PARSE_CB_(this->callbacks(), proc.curr() == '\n', m_evt_handler->m_curr->pos);
    size_t num_newl = 0;
    size_t wpos_at_first_newl = npos;
    while(proc.has_more_chars(len))
    {
        const char curr = proc.curr();
        _c4dbgfbf("'{}' sofar={}",  _c4prc(curr), prs_(proc.sofar()));
        switch(curr)
        {
        case '\n':
        {
            _c4dbgfbf("newline. sofar={}", num_newl);
            // NOTE: vs2022-32bit-release builds were giving wrong
            // results in this block, if it was written as either
            // as a  switch(num_newl) or its equivalent if-form.
            //
            // For this reason, we're using a dedicated function
            // (**_compress), which seems to work around the issue.
            //
            // The manifested problem was that somewhere between the
            // assignment to curr and this point, proc.wpos (the
            // write-position of the processor) jumped to npos, which
            // made the write wrap-around! To make things worse,
            // enabling prints via _c4dbgpf() and _c4dbgfbf() made the
            // problem go away!
            //
            // The only way to make the problem appear with prints
            // enabled was by disabling all prints in this function
            // (including in the block which was moved to the compress
            // function) and then selectively enabling only some of
            // those prints.
            //
            // This may be due to some bug in the cl-x86 optimizer; or
            // it may be triggered by some UB which may be
            // inadvertedly present in this function or in the filter
            // processor. This is despite our best efforts to weed out
            // any such UB problem: neither clang-tidy nor none of the
            // sanitizers, or gcc's -fanalyzer pointed to any problems
            // in this code.
            //
            // In the end, moving this block to a separate function
            // was the only way to bury the problem. But it may
            // resurface again, as The Undead, rising to from the
            // grave to haunt us with his terrible presence.
            //
            // We may have to revisit this. With a stake, and lots of
            // garlic.
            wpos_at_first_newl = _filter_block_folded_newlines_compress(proc, ++num_newl, wpos_at_first_newl);
            _filter_block_indentation(proc, indentation);
            break;
        }
        case ' ':
        case '\t':
            {
                size_t first = proc.rem().first_not_of(" \t");
                _c4dbgfbf("space. first={}", first);
                if(first == npos)
                    first = proc.rem().len;
                _c4dbgfbf("... indentation increased to {}",  first);
                if(num_newl)
                {
                    _c4dbgfbf("... prev space (at wpos={}) must be newline", wpos_at_first_newl);
                    proc.set_at(wpos_at_first_newl, '\n');
                }
                if(num_newl > 1u)
                {
                    _c4dbgfbf("... add missing newline", wpos_at_first_newl);
                    proc.set('\n');
                }
                _filter_block_folded_indented_block(proc, indentation, len, first);
                num_newl = 0;
                wpos_at_first_newl = npos;
                break;
            }
        case '\r':
            proc.skip();
            break;
        default:
            _c4dbgfbf("not space, not newline. stop.", 0);
            return;
        }
    }
}


template<class EventHandler>
template<class FilterProcessor>
void ParseEngine<EventHandler>::_filter_block_folded_indented_block(FilterProcessor &C4_RESTRICT proc, size_t indentation, size_t len, size_t curr_indentation) noexcept
{
    RYML_ASSERT_PARSE_CB_(this->callbacks(), (proc.rem().first_not_of(" \t") == curr_indentation) || (proc.rem().first_not_of(" \t") == npos), m_evt_handler->m_curr->pos);
    if(curr_indentation)
        proc.copy(curr_indentation);
    while(proc.has_more_chars(len))
    {
        const char curr = proc.curr();
        _c4dbgfbf("'{}' sofar={}",  _c4prc(curr), prs_(proc.sofar()));
        switch(curr)
        {
        case '\n':
            {
                proc.copy();
                _filter_block_indentation(proc, indentation);
                csubstr rem = proc.rem();
                const size_t first = rem.first_not_of(' ');
                _c4dbgfbf("newline. firstns={}",  first);
                if(first == 0)
                {
                    const char c = rem[first];
                    _c4dbgfbf("firstns={}='{}'", first, _c4prc(c));
                    if(c != '\n' && c != '\r')
                    {
                        _c4dbgfbf("done with indented block",  first);
                        goto endloop;
                    }
                }
                else if(first != npos)
                {
                    proc.copy(first);
                    _c4dbgfbf("copy all {} spaces",  first);
                }
                break;
            }
            break;
        case '\r':
            proc.skip();
            break;
        default:
            proc.copy();
            break;
        }
    }
 endloop:
    return;
}


template<class EventHandler>
template<class FilterProcessor>
auto ParseEngine<EventHandler>::_filter_block_folded(FilterProcessor &C4_RESTRICT proc, size_t indentation, BlockChomp_e chomp) -> decltype(proc.result())
{
    _c4dbgfbf("indentation={} before={}", indentation, prs_(proc.src));

    size_t contents_len = _handle_all_whitespace(proc, chomp);
    if(!contents_len)
        return proc.result();

    contents_len = _extend_to_chomp(proc, contents_len);

    _c4dbgfbf("to filter={}", prs_(proc.src.first(contents_len)));

    _filter_block_folded_newlines_leading(proc, indentation, contents_len);

    // now filter the bulk
    while(proc.has_more_chars(/*maxpos*/contents_len))
    {
        const char curr = proc.curr();
        _c4dbgfbf("'{}' sofar={}",  _c4prc(curr), prs_(proc.sofar()));
        switch(curr)
        {
        case '\n':
        {
            _c4dbgfbf("found newline", curr);
            _filter_block_folded_newlines(proc, indentation, contents_len);
            break;
        }
        case '\r':
            proc.skip();
            break;
        default:
            proc.copy();
            break;
        }
    }

    _c4dbgfbf("before chomp: #tochomp={}   sofar={}", proc.rem().len, prs_(proc.sofar()));

    _filter_chomp(proc, chomp, indentation);

    _c4dbgfbf("final={}", proc.sofar().len, prs_(proc.sofar()));

    return proc.result();
}

#undef _c4dbgfbf

template<class EventHandler>
FilterResult ParseEngine<EventHandler>::filter_scalar_block_folded(csubstr scalar, substr dst, size_t indentation, BlockChomp_e chomp)
{
    FilterProcessorSrcDst proc(scalar, dst);
    return _filter_block_folded(proc, indentation, chomp);
}

template<class EventHandler>
FilterResult ParseEngine<EventHandler>::filter_scalar_block_folded_in_place(substr scalar, size_t cap, size_t indentation, BlockChomp_e chomp)
{
    FilterProcessorInplaceEndExtending proc(scalar, cap);
    return _filter_block_folded(proc, indentation, chomp);
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

template<class EventHandler>
csubstr ParseEngine<EventHandler>::_filter_scalar_plain(substr s, size_t indentation)
{
    _c4dbgpf("filtering plain scalar: s={}", prs_(s));
    FilterResult r = this->filter_scalar_plain_in_place(s, s.len, indentation);
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, r.valid(), m_evt_handler->m_curr->pos);
    _c4dbgpf("filtering plain scalar: success! s={}", prs_(r.get()));
    return r.get();
}

//-----------------------------------------------------------------------------

template<class EventHandler>
csubstr ParseEngine<EventHandler>::_filter_scalar_squot(substr s)
{
    _c4dbgpf("filtering squo scalar: s={}", prs_(s));
    FilterResult r = this->filter_scalar_squoted_in_place(s, s.len);
    RYML_ASSERT_PARSE_CB_(this->callbacks(), r.valid(), m_evt_handler->m_curr->pos);
    _c4dbgpf("filtering squo scalar: success! s={}", prs_(r.get()));
    return r.get();
}


//-----------------------------------------------------------------------------

template<class EventHandler>
csubstr ParseEngine<EventHandler>::_filter_scalar_dquot(substr s)
{
    _c4dbgpf("filtering dquo scalar: s={}", prs_(s));
    FilterResultExtending r = this->filter_scalar_dquoted_in_place(s, s.len);
    if C4_LIKELY(r.valid())
    {
        _c4dbgpf("filtering dquo scalar: success! s={}", prs_(r.get()));
        return r.get();
    }
    else
    {
        const size_t len = r.required_len();
        _c4dbgpf("filtering dquo scalar: not enough space: needs {}, have {}", len, s.len);
        substr dst = _alloc_arena(len, &s);
        _c4dbgpf("filtering dquo scalar: dst.len={}", dst.len);
        if(dst.str)
        {
            RYML_ASSERT_PARSE_CB_(this->callbacks(), dst.len == len, m_evt_handler->m_curr->pos);
            FilterResult rsd = this->filter_scalar_dquoted(s, dst);
            _c4dbgpf("filtering dquo scalar: ... result now needs {} was {}", rsd.required_len(), len);
            RYML_ASSERT_PARSE_CB_(this->callbacks(), rsd.required_len() <= len, m_evt_handler->m_curr->pos); // may be smaller!
            RYML_CHECK_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, rsd.valid(), m_evt_handler->m_curr->pos);
            _c4dbgpf("filtering dquo scalar: success! s={}", prs_(rsd.get()));
            return rsd.get();
        }
        return dst;
    }
}


//-----------------------------------------------------------------------------

template<class EventHandler>
csubstr ParseEngine<EventHandler>::_move_scalar_left_and_add_newline(substr s)
{
    if(s.is_sub(_buf()))
    {
        RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, s.str > _buf().str, m_evt_handler->m_curr->pos);
        RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, s.str-1 >= _buf().str, m_evt_handler->m_curr->pos);
        if(s.len)
            memmove(s.str - 1, s.str, s.len);
        --s.str;
        s.str[s.len] = '\n';
        ++s.len;
        return s;
    }
    else
    {
        substr dst = _alloc_arena(s.len + 1, &s);
        if(s.len)
            memcpy(dst.str, s.str, s.len);
        dst[s.len] = '\n';
        return dst;
    }
}

template<class EventHandler>
csubstr ParseEngine<EventHandler>::_filter_scalar_literal(substr s, size_t indentation, BlockChomp_e chomp)
{
    _c4dbgpf("filtering block literal scalar: s={}", prs_(s));
    FilterResult r = this->filter_scalar_block_literal_in_place(s, s.len, indentation, chomp);
    csubstr result;
    if C4_LIKELY(r.valid())
    {
        result = r.get();
    }
    else
    {
        _c4dbgpf("filtering block literal scalar: not enough space: needs {}, have {}", r.required_len(), s.len);
        RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, r.required_len() == s.len + 1, m_evt_handler->m_curr->pos);
        // this can only happen when adding a single newline in clip mode.
        // so we shift left the scalar by one place
        result = _move_scalar_left_and_add_newline(s);
    }
    _c4dbgpf("filtering block literal scalar: success! s={}", prs_(result));
    return result;
}


//-----------------------------------------------------------------------------
template<class EventHandler>
csubstr ParseEngine<EventHandler>::_filter_scalar_folded(substr s, size_t indentation, BlockChomp_e chomp)
{
    _c4dbgpf("filtering block folded scalar: s={}", prs_(s));
    FilterResult r = this->filter_scalar_block_folded_in_place(s, s.len, indentation, chomp);
    csubstr result;
    if C4_LIKELY(r.valid())
    {
        result = r.get();
    }
    else
    {
        _c4dbgpf("filtering block folded scalar: not enough space: needs {}, have {}", r.required_len(), s.len);
        RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, r.required_len() == s.len + 1, m_evt_handler->m_curr->pos);
        // this can only happen when adding a single newline in clip mode.
        // so we shift left the scalar by one place
        result = _move_scalar_left_and_add_newline(s);
    }
    _c4dbgpf("filtering block folded scalar: success! s={}", prs_(result));
    return result;
}


//-----------------------------------------------------------------------------

template<class EventHandler>
csubstr ParseEngine<EventHandler>::_maybe_filter_key_scalar_plain(ScannedScalar const& C4_RESTRICT sc, size_t indentation)
{
    if(sc.needs_filter)
    {
        if(m_options.scalar_filtering())
        {
            return _filter_scalar_plain(sc.scalar, indentation);
        }
        else
        {
            _c4dbgp("plain scalar left unfiltered");
            m_evt_handler->mark_key_scalar_unfiltered();
        }
    }
    else
    {
        _c4dbgp("plain scalar doesn't need filtering");
    }
    return sc.scalar;
}

template<class EventHandler>
csubstr ParseEngine<EventHandler>::_maybe_filter_val_scalar_plain(ScannedScalar const& C4_RESTRICT sc, size_t indentation)
{
    if(sc.needs_filter)
    {
        if(m_options.scalar_filtering())
        {
            return _filter_scalar_plain(sc.scalar, indentation);
        }
        else
        {
            _c4dbgp("plain scalar left unfiltered");
            m_evt_handler->mark_val_scalar_unfiltered();
        }
    }
    else
    {
        _c4dbgp("plain scalar doesn't need filtering");
    }
    return sc.scalar;
}


//-----------------------------------------------------------------------------

template<class EventHandler>
csubstr ParseEngine<EventHandler>::_maybe_filter_key_scalar_squot(ScannedScalar const& C4_RESTRICT sc)
{
    if(sc.needs_filter)
    {
        if(m_options.scalar_filtering())
        {
            return _filter_scalar_squot(sc.scalar);
        }
        else
        {
            _c4dbgp("squo key scalar left unfiltered");
            m_evt_handler->mark_key_scalar_unfiltered();
        }
    }
    else
    {
        _c4dbgp("squo key scalar doesn't need filtering");
    }
    return sc.scalar;
}

template<class EventHandler>
csubstr ParseEngine<EventHandler>::_maybe_filter_val_scalar_squot(ScannedScalar const& C4_RESTRICT sc)
{
    if(sc.needs_filter)
    {
        if(m_options.scalar_filtering())
        {
            return _filter_scalar_squot(sc.scalar);
        }
        else
        {
            _c4dbgp("squo val scalar left unfiltered");
            m_evt_handler->mark_val_scalar_unfiltered();
        }
    }
    else
    {
        _c4dbgp("squo val scalar doesn't need filtering");
    }
    return sc.scalar;
}


//-----------------------------------------------------------------------------

template<class EventHandler>
csubstr ParseEngine<EventHandler>::_maybe_filter_key_scalar_dquot(ScannedScalar const& C4_RESTRICT sc)
{
    if(sc.needs_filter)
    {
        if(m_options.scalar_filtering())
        {
            return _filter_scalar_dquot(sc.scalar);
        }
        else
        {
            _c4dbgp("dquo scalar left unfiltered");
            m_evt_handler->mark_key_scalar_unfiltered();
        }
    }
    else
    {
        _c4dbgp("dquo scalar doesn't need filtering");
    }
    return sc.scalar;
}

template<class EventHandler>
csubstr ParseEngine<EventHandler>::_maybe_filter_val_scalar_dquot(ScannedScalar const& C4_RESTRICT sc)
{
    if(sc.needs_filter)
    {
        if(m_options.scalar_filtering())
        {
            return _filter_scalar_dquot(sc.scalar);
        }
        else
        {
            _c4dbgp("dquo scalar left unfiltered");
            m_evt_handler->mark_val_scalar_unfiltered();
        }
    }
    else
    {
        _c4dbgp("dquo scalar doesn't need filtering");
    }
    return sc.scalar;
}


//-----------------------------------------------------------------------------

template<class EventHandler>
csubstr ParseEngine<EventHandler>::_maybe_filter_key_scalar_literal(ScannedBlock const& C4_RESTRICT sb)
{
    if(m_options.scalar_filtering())
    {
        return _filter_scalar_literal(sb.scalar, sb.indentation, sb.chomp);
    }
    else
    {
        _c4dbgp("literal scalar left unfiltered");
        m_evt_handler->mark_key_scalar_unfiltered();
    }
    return sb.scalar;
}

template<class EventHandler>
csubstr ParseEngine<EventHandler>::_maybe_filter_val_scalar_literal(ScannedBlock const& C4_RESTRICT sb)
{
    if(m_options.scalar_filtering())
    {
        return _filter_scalar_literal(sb.scalar, sb.indentation, sb.chomp);
    }
    else
    {
        _c4dbgp("literal scalar left unfiltered");
        m_evt_handler->mark_val_scalar_unfiltered();
    }
    return sb.scalar;
}


//-----------------------------------------------------------------------------

template<class EventHandler>
csubstr ParseEngine<EventHandler>::_maybe_filter_key_scalar_folded(ScannedBlock const& C4_RESTRICT sb)
{
    if(m_options.scalar_filtering())
    {
        return _filter_scalar_folded(sb.scalar, sb.indentation, sb.chomp);
    }
    else
    {
        _c4dbgp("folded scalar left unfiltered");
        m_evt_handler->mark_key_scalar_unfiltered();
    }
    return sb.scalar;
}

template<class EventHandler>
csubstr ParseEngine<EventHandler>::_maybe_filter_val_scalar_folded(ScannedBlock const& C4_RESTRICT sb)
{
    if(m_options.scalar_filtering())
    {
        return _filter_scalar_folded(sb.scalar, sb.indentation, sb.chomp);
    }
    else
    {
        _c4dbgp("folded scalar left unfiltered");
        m_evt_handler->mark_val_scalar_unfiltered();
    }
    return sb.scalar;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

#ifdef RYML_DBG  //   !!! <----------------------------------

template<class EventHandler>
void ParseEngine<EventHandler>::add_flags(ParserFlag_t on)
{
    ParserState *s = m_evt_handler->m_curr;
    char buf1_[64], buf2_[64], buf3_[64];
    csubstr buf1 = detail::_parser_flags_to_str(buf1_, on);
    csubstr buf2 = detail::_parser_flags_to_str(buf2_, s->flags);
    csubstr buf3 = detail::_parser_flags_to_str(buf3_, s->flags|on);
    _c4dbgpf("state[{}]: add {}: before={} after={}", s->level, buf1, buf2, buf3);
    s->flags |= on;
}

template<class EventHandler>
void ParseEngine<EventHandler>::addrem_flags(ParserFlag_t on, ParserFlag_t off)
{
    ParserState *s = m_evt_handler->m_curr;
    char buf1_[64], buf2_[64], buf3_[64], buf4_[64];
    csubstr buf1 = detail::_parser_flags_to_str(buf1_, on);
    csubstr buf2 = detail::_parser_flags_to_str(buf2_, off);
    csubstr buf3 = detail::_parser_flags_to_str(buf3_, s->flags);
    csubstr buf4 = detail::_parser_flags_to_str(buf4_, (~off)&((s->flags|on)));
    _c4dbgpf("state[{}]: add {} / rem {}: before={} after={}", s->level, buf1, buf2, buf3, buf4);
    RYML_ASSERT_BASIC_((on & off) == ParserFlag_t(0));
    s->flags &= ~off;
    s->flags |= on;
}

template<class EventHandler>
void ParseEngine<EventHandler>::rem_flags(ParserFlag_t off)
{
    ParserState *s = m_evt_handler->m_curr;
    char buf1_[64], buf2_[64], buf3_[64];
    csubstr buf1 = detail::_parser_flags_to_str(buf1_, off);
    csubstr buf2 = detail::_parser_flags_to_str(buf2_, s->flags);
    csubstr buf3 = detail::_parser_flags_to_str(buf3_, s->flags&(~off));
    _c4dbgpf("state[{}]: rem {}: before={} after={}", s->level, buf1, buf2, buf3);
    s->flags &= ~off;
}

inline C4_NO_INLINE csubstr detail::_parser_flags_to_str(substr buf, ParserFlag_t flags)
{
    size_t pos = 0;
    bool gotone = false;

    #define _prflag(fl)                                     \
    if((flags & fl) == (fl))                                \
    {                                                       \
        if(gotone)                                          \
        {                                                   \
            if(pos + 1 < buf.len)                           \
                buf[pos] = '|';                             \
            ++pos;                                          \
        }                                                   \
        csubstr fltxt = #fl;                                \
        if(pos + fltxt.len <= buf.len)                      \
            memcpy(buf.str + pos, fltxt.str, fltxt.len);    \
        pos += fltxt.len;                                   \
        gotone = true;                                      \
    }

    _prflag(RTOP);
    _prflag(RUNK);
    _prflag(RMAP);
    _prflag(RSEQ);
    _prflag(RFLOW);
    _prflag(RBLCK);
    _prflag(QMRK);
    _prflag(RKEY);
    _prflag(RVAL);
    _prflag(RKCL);
    _prflag(RNXT);
    _prflag(SSCL);
    _prflag(QSCL);
    _prflag(RSET);
    _prflag(RDOC);
    _prflag(NDOC);
    _prflag(USTY);
    _prflag(RSEQIMAP);

    #undef _prflag

    if(pos == 0)
        if(buf.len > 0)
            buf[pos++] = '0';

    RYML_CHECK_BASIC_(pos <= buf.len);

    return buf.first(pos);
}

#endif // RYML_DBG   !!! <----------------------------------


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

template<class EventHandler>
csubstr ParseEngine<EventHandler>::location_contents(Location const& loc) const
{
    RYML_ASSERT_BASIC_CB_(m_evt_handler->m_stack.m_callbacks, loc.offset < _buf().len);
    return _buf().sub(loc.offset);
}

template<class EventHandler>
Location ParseEngine<EventHandler>::val_location(const char *val) const
{
    if C4_UNLIKELY(val == nullptr)
        return {m_evt_handler->m_curr->pos.name, 0, 0, 0};
    RYML_CHECK_BASIC_CB_(m_evt_handler->m_stack.m_callbacks, m_options.locations());
    // NOTE: if any of these checks fails, the parser needs to be
    // instantiated with locations enabled.
    RYML_ASSERT_BASIC_CB_(m_evt_handler->m_stack.m_callbacks, m_options.locations());
    RYML_ASSERT_BASIC_CB_(m_evt_handler->m_stack.m_callbacks, !_locations_dirty());
    RYML_ASSERT_BASIC_CB_(m_evt_handler->m_stack.m_callbacks, m_newline_offsets != nullptr);
    RYML_ASSERT_BASIC_CB_(m_evt_handler->m_stack.m_callbacks, m_newline_offsets_size > 0);
    // NOTE: the pointer needs to belong to the buffer that was used to parse.
    csubstr src = _buf();
    RYML_CHECK_BASIC_CB_(m_evt_handler->m_stack.m_callbacks, val != nullptr || src.str == nullptr);
    RYML_CHECK_BASIC_CB_(m_evt_handler->m_stack.m_callbacks, (val >= src.begin() && val <= src.end()) || (src.str == nullptr && val == nullptr));
    // ok. search the first stored newline after the given ptr
    using lineptr_type = size_t const* C4_RESTRICT;
    lineptr_type lineptr = nullptr;
    size_t offset = (size_t)(val - src.begin());
    if(m_newline_offsets_size < RYML_LOCATIONS_SMALL_THRESHOLD)
    {
        // just do a linear search if the size is small.
        for(lineptr_type curr = m_newline_offsets, last = m_newline_offsets + m_newline_offsets_size; curr < last; ++curr)
        {
            if(*curr > offset)
            {
                lineptr = curr;
                break;
            }
        }
    }
    else
    {
        // do a bisection search if the size is not small.
        //
        // We could use std::lower_bound but this is simple enough and
        // spares the costly include of <algorithm>.
        size_t count = m_newline_offsets_size;
        lineptr = m_newline_offsets;
        while(count)
        {
            size_t step = count >> 1;
            lineptr_type it = lineptr + step;
            if(*it < offset)
            {
                lineptr = ++it;
                count -= step + 1;
            }
            else
            {
                count = step;
            }
        }
    }
    RYML_ASSERT_BASIC_CB_(m_evt_handler->m_stack.m_callbacks, lineptr);
    RYML_ASSERT_BASIC_CB_(m_evt_handler->m_stack.m_callbacks, lineptr >= m_newline_offsets);
    RYML_ASSERT_BASIC_CB_(m_evt_handler->m_stack.m_callbacks, lineptr <= m_newline_offsets + m_newline_offsets_size);
    RYML_ASSERT_BASIC_CB_(m_evt_handler->m_stack.m_callbacks, lineptr && (*lineptr > offset));
    Location loc;
    loc.name = m_evt_handler->m_curr->pos.name;
    loc.offset = offset;
    loc.line = (size_t)(lineptr - m_newline_offsets);
    if(lineptr > m_newline_offsets)
        loc.col = (offset - *(lineptr-1) - 1u);
    else
        loc.col = offset;
    return loc;
}

template<class EventHandler>
void ParseEngine<EventHandler>::_prepare_locations()
{
    csubstr src = _buf();
    size_t numnewlines = 1u + src.count('\n');
    _resize_locations(numnewlines);
    m_newline_offsets_size = 0;
    for(size_t i = 0; i < src.len; i++)
        if(src.str[i] == '\n')
            m_newline_offsets[m_newline_offsets_size++] = i; // NOLINT
    m_newline_offsets[m_newline_offsets_size++] = src.len; // NOLINT
    RYML_ASSERT_BASIC_CB_(m_evt_handler->m_stack.m_callbacks, m_newline_offsets_size == numnewlines);
}

template<class EventHandler>
void ParseEngine<EventHandler>::_resize_locations(size_t numnewlines)
{
    numnewlines = numnewlines >= 16 ? numnewlines : 16;
    if(numnewlines > m_newline_offsets_capacity)
    {
        if(m_newline_offsets)
            RYML_CB_FREE_(m_evt_handler->m_stack.m_callbacks, m_newline_offsets, size_t, m_newline_offsets_capacity);
        m_newline_offsets = RYML_CB_ALLOC_HINT_(m_evt_handler->m_stack.m_callbacks, size_t, numnewlines, m_newline_offsets);
        m_newline_offsets_capacity = numnewlines;
    }
}

template<class EventHandler>
bool ParseEngine<EventHandler>::_locations_dirty() const
{
    return !m_newline_offsets_size;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

template<class EventHandler>
void ParseEngine<EventHandler>::_handle_flow_skip_whitespace()
{
    // don't assign to csubstr rem: otherwise, gcc12,13,14 -O3 -m32 misbuilds
    if(m_evt_handler->m_curr->line_contents.rem.len > 0)
    {
        if(m_evt_handler->m_curr->line_contents.rem.str[0] == ' ' || m_evt_handler->m_curr->line_contents.rem.str[0] == '\t')
        {
            _c4dbgpf("starts with whitespace: '{}'", _c4prc(m_evt_handler->m_curr->line_contents.rem.str[0]));
            _skipchars(" \t");
        }
        // comments
        if(m_evt_handler->m_curr->line_contents.rem.begins_with('#'))
        {
            _c4dbgpf("it's a comment: {}", m_evt_handler->m_curr->line_contents.rem);
            _line_progressed(m_evt_handler->m_curr->line_contents.rem.len);
        }
    }
}


template<class EventHandler>
void ParseEngine<EventHandler>::_handle_flow_line_beginning()
{
    _c4dbgpf("flow: indref={} indentation={}", m_evt_handler->m_curr->indref, m_evt_handler->m_curr->line_contents.indentation);
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, m_evt_handler->m_curr->at_line_beginning(), m_evt_handler->m_curr->pos);
    if C4_UNLIKELY(m_evt_handler->m_curr->indentation_lt())
    {
        csubstr trimmed = m_evt_handler->m_curr->line_contents.rem.sub(m_evt_handler->m_curr->line_contents.indentation);
        _c4dbgpf("flow: after indentation={}", prs_(trimmed));
        if(trimmed.len && trimmed.triml(" \t").len)
        {
            _line_progressed(m_evt_handler->m_curr->line_contents.indentation);
            _c4err("bad indentation");
        }
    }
}

template<class EventHandler>
size_t ParseEngine<EventHandler>::_handle_block_skip_leading_whitespace()
{
    const size_t mark = m_evt_handler->m_curr->pos.offset;
    const size_t firstpos = m_evt_handler->m_curr->line_contents.rem.first_not_of(" \t");
    _c4dbgpf("block: mark={}  firstpos={}", mark, firstpos);
    if(firstpos != npos)
    {
        _c4dbgp("block: non empty line");
        _line_progressed(firstpos);
        return mark;
    }
    else
    {
        _c4dbgp("block: rest of line is whitespace");
        _line_progressed(m_evt_handler->m_curr->line_contents.rem.len);
        return npos;
    }
}

template<class EventHandler>
void ParseEngine<EventHandler>::_handle_block_check_leading_tabs(size_t start_mark, size_t end_mark)
{
    _c4dbgpf("block: start_mark={}  end_mark={}", start_mark, end_mark);
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, end_mark >= start_mark, m_evt_handler->m_curr->pos);
    if(end_mark != start_mark)
    {
        csubstr leading = _buf().range(start_mark, end_mark);
        _c4dbgpf("block: leading[{}-{}]={}", start_mark, end_mark, prs_(leading, true));
        size_t pos = leading.find('\t');
        if(pos != npos)
        {
            size_t fno = leading.first_not_of(" \t");
            if(fno == npos || pos < fno)
                _c4err("invalid tab character to the left");
        }
        (void)leading;
    }
}


//-----------------------------------------------------------------------------


template<class EventHandler>
void ParseEngine<EventHandler>::_handle_colon()
{
    size_t curr = m_evt_handler->m_curr->pos.line;
    if C4_UNLIKELY(m_prev_colon != npos && curr == m_prev_colon)
    {
        _c4dbgpf("colon: prevline={} currline={}", m_prev_colon, curr);
        _c4err("two colons on same line");
    }
    _c4dbgpf("colon: set prevline={}->{}", m_prev_colon, curr);
    m_prev_colon = curr;
}

template<class EventHandler>
void ParseEngine<EventHandler>::_add_annotation(Annotation *C4_RESTRICT dst, csubstr str)
{
    _c4dbgpf("store annotation[{}]: {}", dst->num_entries, prs_(str));
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, dst->num_entries < C4_COUNTOF(dst->annotations), m_evt_handler->m_curr->pos); // NOLINT(bugprone-sizeof-expression)
    dst->annotations[dst->num_entries].str = str;
    dst->annotations[dst->num_entries].indentation = {};
    dst->annotations[dst->num_entries].line = {};
    dst->annotations[dst->num_entries].orig = {};
    ++dst->num_entries;
}

template<class EventHandler>
void ParseEngine<EventHandler>::_add_annotation(Annotation *C4_RESTRICT dst, csubstr str, size_t indentation, size_t line)
{
    _c4dbgpf("store annotation[{}]: '{}' indentation={} line={}", dst->num_entries, maybe_null_str_(str), indentation, line);
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, dst->num_entries < C4_COUNTOF(dst->annotations), m_evt_handler->m_curr->pos); // NOLINT(bugprone-sizeof-expression)
    if C4_UNLIKELY(dst->num_entries && dst->annotations[0].line == line)
    {
        _c4err("parse error");
    }
    dst->annotations[dst->num_entries].str = str;
    dst->annotations[dst->num_entries].indentation = indentation;
    dst->annotations[dst->num_entries].line = line;
    dst->annotations[dst->num_entries].orig = {};
    ++dst->num_entries;
}

template<class EventHandler>
void ParseEngine<EventHandler>::_add_annotation(Annotation *C4_RESTRICT dst, csubstr str, size_t indentation, size_t line, csubstr orig)
{
    _c4dbgpf("store annotation[{}]: '{}'->'{}' indentation={} line={}", dst->num_entries, orig, maybe_null_str_(str), indentation, line);
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, dst->num_entries < C4_COUNTOF(dst->annotations), m_evt_handler->m_curr->pos); // NOLINT(bugprone-sizeof-expression)
    if C4_UNLIKELY(dst->num_entries && dst->annotations[0].line == line)
    {
        _c4err("parse error");
    }
    dst->annotations[dst->num_entries].str = str;
    dst->annotations[dst->num_entries].indentation = indentation;
    dst->annotations[dst->num_entries].line = line;
    dst->annotations[dst->num_entries].orig = orig;
    ++dst->num_entries;
}

template<class EventHandler>
bool ParseEngine<EventHandler>::_annotations_require_key_container() const
{
    return m_pending_tags.num_entries > 1 || m_pending_anchors.num_entries > 1;
}

template<class EventHandler>
bool ParseEngine<EventHandler>::_handle_annotations_before_unexpected_flow_token_rkey()
{
    if(!(m_pending_tags.num_entries | m_pending_anchors.num_entries))
        return false;
    _c4dbgpf("handle_annotations_before_unexpected_flow_comma_rkey, node={}", m_evt_handler->m_curr->node_id);
    if(m_pending_tags.num_entries)
    {
        _c4dbgpf("handle_annotations_before_unexpected_flow_comma_rkey, #tags={}", m_pending_tags.num_entries);
        if C4_LIKELY(m_pending_tags.num_entries == 1)
        {
             m_evt_handler->set_key_tag(m_pending_tags.annotations[0].str);
            _clear_annotations(&m_pending_tags);
        }
        else
        {
            _c4err("too many tags");
        }
    }
    if(m_pending_anchors.num_entries)
    {
        _c4dbgpf("handle_annotations_before_unexpected_flow_comma, #anchors={}", m_pending_tags.num_entries);
        if C4_LIKELY(m_pending_anchors.num_entries == 1)
        {
            m_evt_handler->set_key_anchor(m_pending_anchors.annotations[0].str);
            _clear_annotations(&m_pending_anchors);
        }
        else
        {
            _c4err("too many anchors");
        }
    }
    m_evt_handler->set_key_scalar_plain_empty();
    m_evt_handler->set_val_scalar_plain_empty();
    return true;
}

template<class EventHandler>
void ParseEngine<EventHandler>::_handle_annotations_before_blck_key_scalar()
{
    _c4dbgpf("annotations_before_blck_key_scalar, node={}", m_evt_handler->m_curr->node_id);
    if(m_pending_tags.num_entries)
    {
        _c4dbgpf("annotations_before_blck_key_scalar, #tags={}", m_pending_tags.num_entries);
        if C4_LIKELY(m_pending_tags.num_entries == 1)
        {
             m_evt_handler->set_key_tag(m_pending_tags.annotations[0].str);
            _clear_annotations(&m_pending_tags);
        }
        else
        {
            _c4err("too many tags"); // LCOV_EXCL_LINE
        }
    }
    if(m_pending_anchors.num_entries)
    {
        _c4dbgpf("annotations_before_blck_key_scalar, #anchors={}", m_pending_anchors.num_entries);
        if C4_LIKELY(m_pending_anchors.num_entries == 1)
        {
            m_evt_handler->set_key_anchor(m_pending_anchors.annotations[0].str);
            _clear_annotations(&m_pending_anchors);
        }
        else
        {
            _c4err("too many anchors"); // LCOV_EXCL_LINE
        }
    }
}

template<class EventHandler>
void ParseEngine<EventHandler>::_handle_annotations_before_blck_val_scalar()
{
    _c4dbgpf("annotations_before_blck_val_scalar, node={}", m_evt_handler->m_curr->node_id);
    if(m_pending_tags.num_entries)
    {
        _c4dbgpf("annotations_before_blck_val_scalar, #tags={}", m_pending_tags.num_entries);
        if C4_LIKELY(m_pending_tags.num_entries == 1)
        {
             m_evt_handler->set_val_tag(m_pending_tags.annotations[0].str);
            _clear_annotations(&m_pending_tags);
        }
        else
        {
            _c4err("too many tags");
        }
    }
    if(m_pending_anchors.num_entries)
    {
        _c4dbgpf("annotations_before_blck_val_scalar, #anchors={}", m_pending_anchors.num_entries);
        if C4_LIKELY(m_pending_anchors.num_entries == 1)
        {
            m_evt_handler->set_val_anchor(m_pending_anchors.annotations[0].str);
            _clear_annotations(&m_pending_anchors);
        }
        else
        {
            _c4err("too many anchors");
        }
    }
}

template<class EventHandler>
void ParseEngine<EventHandler>::_handle_annotations_before_start_mapblck(size_t current_line)
{
    _c4dbgpf("annotations_before_start_mapblck, current_line={}", current_line);
    if(m_pending_tags.num_entries == 2)
    {
        _c4dbgp("2 tags, setting entry 0");
        m_evt_handler->set_val_tag(m_pending_tags.annotations[0].str);
    }
    else if(m_pending_tags.num_entries == 1)
    {
        _c4dbgpf("1 tag. line={}, curr={}", m_pending_tags.annotations[0].line, current_line);
        if(m_pending_tags.annotations[0].line < current_line)
        {
            _c4dbgp("...tag is for the map. setting it.");
             m_evt_handler->set_val_tag(m_pending_tags.annotations[0].str);
            _clear_annotations(&m_pending_tags);
        }
    }
    //
    if(m_pending_anchors.num_entries == 2)
    {
        _c4dbgp("2 anchors, setting entry 0");
        m_evt_handler->set_val_anchor(m_pending_anchors.annotations[0].str);
    }
    else if(m_pending_anchors.num_entries == 1)
    {
        _c4dbgpf("1 anchor. line={}, curr={}", m_pending_anchors.annotations[0].line, current_line);
        if(m_pending_anchors.annotations[0].line < current_line)
        {
            _c4dbgp("...anchor is for the map. setting it.");
            m_evt_handler->set_val_anchor(m_pending_anchors.annotations[0].str);
            _clear_annotations(&m_pending_anchors);
        }
    }
}

template<class EventHandler>
void ParseEngine<EventHandler>::_handle_annotations_before_start_mapblck_as_key()
{
    _c4dbgp("annotations_before_start_mapblck_as_key");
    switch(m_pending_tags.num_entries)
    {
    case 1u:
        _c4dbgpf("annotations_after_start_mapblck_as_key: 1 tag={} line={} currline=", prs_(m_pending_tags.annotations[0].str), m_pending_tags.annotations[0].line, m_evt_handler->m_curr->pos.line);
        if(m_pending_tags.annotations[0].line != m_evt_handler->m_curr->pos.line)
        {
            _c4dbgp("annotations_after_start_mapblck_as_key: is map tag");
            m_evt_handler->set_key_tag(m_pending_tags.annotations[0].str);
            _clear_annotations(&m_pending_tags);
        }
        break;
    case 2u:
        _c4dbgpf("annotations_after_start_mapblck_as_key: 2 tags: {} -> {}", prs_(m_pending_tags.annotations[0].str), prs_(m_pending_tags.annotations[1].str));
         m_evt_handler->set_key_tag(m_pending_tags.annotations[0].str);
        break;
    }
    switch(m_pending_anchors.num_entries)
    {
    case 1u:
        _c4dbgpf("annotations_after_start_mapblck_as_key: 1 anchor={} line={} currline=", m_pending_anchors.annotations[0].str, m_pending_anchors.annotations[0].line, m_evt_handler->m_curr->pos.line);
        if(m_pending_anchors.annotations[0].line != m_evt_handler->m_curr->pos.line)
        {
            _c4dbgp("annotations_after_start_mapblck_as_key: is map anchor");
            m_evt_handler->set_key_anchor(m_pending_anchors.annotations[0].str);
            _clear_annotations(&m_pending_anchors);
        }
        break;
    case 2u:
        _c4dbgpf("annotations_after_start_mapblck_as_key: 2 anchors: {} -> {}", m_pending_anchors.annotations[0].str, m_pending_anchors.annotations[1].str);
        m_evt_handler->set_key_anchor(m_pending_anchors.annotations[0].str);
        break;
    }
}

template<class EventHandler>
void ParseEngine<EventHandler>::_handle_annotations_and_indentation_after_start_mapblck(size_t key_indentation, size_t key_line)
{
    _c4dbgp("annotations_after_start_mapblck");
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, m_pending_tags.num_entries <= 2, m_evt_handler->m_curr->pos);
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, m_pending_anchors.num_entries <= 2, m_evt_handler->m_curr->pos);
    if(m_pending_anchors.num_entries || m_pending_tags.num_entries)
    {
        key_indentation = _select_indentation_from_annotations(key_indentation, key_line);
        switch(m_pending_tags.num_entries)
        {
        case 1u:
            _c4dbgpf("annotations_after_start_mapblck: 1 tag: {}", prs_(m_pending_tags.annotations[0].str));
             m_evt_handler->set_key_tag(m_pending_tags.annotations[0].str);
            _clear_annotations(&m_pending_tags);
            break;
        case 2u:
            _c4dbgpf("annotations_after_start_mapblck: 2 tags: {} -> {}", prs_(m_pending_tags.annotations[0].str), prs_(m_pending_tags.annotations[1].str));
             m_evt_handler->set_key_tag(m_pending_tags.annotations[1].str);
            _clear_annotations(&m_pending_tags);
            break;
        }
        switch(m_pending_anchors.num_entries)
        {
        case 1u:
            _c4dbgpf("annotations_after_start_mapblck: 1 anchors: {} -> {}", m_pending_anchors.annotations[0].str);
            m_evt_handler->set_key_anchor(m_pending_anchors.annotations[0].str);
            _clear_annotations(&m_pending_anchors);
            break;
        case 2u:
            _c4dbgpf("annotations_after_start_mapblck: 2 anchors: {} -> {}", m_pending_anchors.annotations[0].str, m_pending_anchors.annotations[1].str);
            m_evt_handler->set_key_anchor(m_pending_anchors.annotations[1].str);
            _clear_annotations(&m_pending_anchors);
            break;
        }
    }
    _set_indentation(key_indentation);
}

template<class EventHandler>
size_t ParseEngine<EventHandler>::_select_indentation_from_annotations(size_t val_indentation, size_t val_line)
{
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, m_pending_tags.num_entries | m_pending_anchors.num_entries, m_evt_handler->m_curr->pos);
    // select the left-most annotation on the max line
    auto const *C4_RESTRICT curr = m_pending_anchors.num_entries ? &m_pending_anchors.annotations[0] : &m_pending_tags.annotations[0];
    for(size_t i = 0; i < m_pending_anchors.num_entries; ++i)
    {
        auto const& C4_RESTRICT ann = m_pending_anchors.annotations[i];
        if(ann.line > curr->line)
            curr = &ann;
        else if(ann.indentation < curr->indentation)
            curr = &ann;
    }
    for(size_t j = 0; j < m_pending_tags.num_entries; ++j)
    {
        auto const& C4_RESTRICT ann = m_pending_tags.annotations[j];
        if(ann.line > curr->line)
            curr = &ann;
        else if(ann.indentation < curr->indentation)
            curr = &ann;
    }
    return curr->line < val_line ? val_indentation : curr->indentation;
}

template<class EventHandler>
void ParseEngine<EventHandler>::_handle_keyref(csubstr alias)
{
    if C4_LIKELY(!(m_pending_anchors.num_entries | m_pending_tags.num_entries))
        m_evt_handler->set_key_ref(alias);
    else
        _c4err("aliases cannot have anchors or tags");
}

template<class EventHandler>
void ParseEngine<EventHandler>::_handle_valref(csubstr alias)
{
    if C4_LIKELY(!(m_pending_anchors.num_entries | m_pending_tags.num_entries))
        m_evt_handler->set_val_ref(alias);
    else
        _c4err("aliases cannot have anchors or tags");
}

template<class EventHandler>
csubstr ParseEngine<EventHandler>::_resolve_tag(csubstr tag)
{
    _c4dbgpf("resolving tag: {} curr_doc={}", prs_(tag), m_evt_handler->m_curr_doc);
    _c4assert(tag.is_sub(_buf()));
    TagCache::LookupResult ret = m_evt_handler->tag_cache().find(tag, m_evt_handler->m_curr_doc);
    if(ret)
    {
        _c4dbgpf("resolving tag: found in cache[{}]: {}", ret.pos, prs_(ret.resolved));
        return ret.resolved;
    }
    _c4dbgpf("resolving tag: not in cache: {} curr_doc={}", prs_(tag), m_evt_handler->m_curr_doc);
    size_t bufsz = 0;
    substr buf = m_evt_handler->arena_rem();
    TagDirectives const& C4_RESTRICT tds = m_evt_handler->tag_directives();
    csubstr ttag = tds.resolve(buf, &bufsz, tag, m_evt_handler->m_curr_doc,
                               m_evt_handler->m_curr->pos,
                               m_evt_handler->m_stack.m_callbacks);
    _c4dbgpf("resolving tag: bufsz={} ttag.len={} !!ttag.str={}", bufsz, ttag.len, !!ttag.str);
    _c4assert((bufsz > buf.len) == (!ttag.str));
    _c4assert(!!bufsz == (ttag.len == bufsz));
    // try again if the arena size was not enough
    if(!ttag.str)
    {
        _c4dbgpf("tag requires arena, but it was small. arena.len={} arena.slack={} tag.required={}", m_evt_handler->arena_rem().len, m_evt_handler->arena().len, ttag.len);
        _c4assert(ttag.len == bufsz);
        buf = _alloc_arena(bufsz, &tag);
        if(buf.str) // the alloc may fail eg with the ints handler
        {
            ttag = tds.resolve(buf, &bufsz, tag, m_evt_handler->m_curr_doc,
                               m_evt_handler->m_curr->pos,
                               m_evt_handler->m_stack.m_callbacks);
        }
        _c4assert(ttag.len == bufsz);
        _c4assert(!ttag.str || ttag.is_sub(m_evt_handler->arena()));
    }
    else if(bufsz) // if we succeeded writing into the arena, grow it as needed
    {
        _c4dbgp("tag required arena. update size");
        _c4assert(ttag.len == bufsz);
        _c4assert(ttag.is_sub(buf));
        (void)_alloc_arena(bufsz);
    }
    C4_SUPPRESS_WARNING_MSVC_WITH_PUSH(4127) // conditional expression is constant
    if C4_IF_CONSTEXPR (EventHandler::requires_strings_on_buffers) // NOLINT
    {
        _c4dbgpf("handler requires tags in buffers. !!ttag.str={} in_arena={} in_src={}", !!ttag.str, ttag.is_sub(m_evt_handler->arena()), ttag.is_sub(_buf()));
        // is the resolved tag not in any of those buffers?
        if(ttag.str && !ttag.is_sub(m_evt_handler->arena()) && !ttag.is_sub(_buf()))
        {
            _c4dbgpf("copying resolved tag to arena: slack={} required={}", m_evt_handler->arena_rem().len, ttag.len);
            buf = _alloc_arena(ttag.len, &tag);
            if(buf.str) // the alloc may fail eg with the ints handler
                memcpy(buf.str, ttag.str, ttag.len);
            ttag.str = buf.str; // keep the current len!
            _c4assert(!ttag.str || ttag.is_sub(m_evt_handler->arena()));
        }
    }
    C4_SUPPRESS_WARNING_MSVC_POP
    _c4dbgpf("resolved tag: {} -->  [{}]~~~{}~~~", prs_(tag), ttag.len, maybe_null_str_(ttag));
    _c4assert(ttag.len > 0);
    // cache the hard-earned result!
    m_evt_handler->tag_cache().add(tag, ttag, m_evt_handler->m_curr_doc, ret.pos);
    return ttag;
}

template<class EventHandler>
bool ParseEngine<EventHandler>::_validate_directive_yaml(csubstr *C4_RESTRICT directive, csubstr *C4_RESTRICT version) const
{
    _c4assert(directive->begins_with("%YAML"));
    size_t version_start = directive->first_not_of(" \t", 5);
    if(version_start != npos)
    {
        csubstr digits = "0123456789";
        size_t major_end = directive->first_not_of(digits, version_start);
        if(major_end != npos && directive->str[major_end] == '.') // single dot
        {
            size_t minor_end = directive->first_not_of(digits, major_end + 1);
            if(minor_end == npos)
                minor_end = directive->len;
            _set_first_strict(*directive, minor_end);
            *version = directive->range(version_start, minor_end);
            _c4dbgpf("%YAML: version={} full={}", *version, prs_(*directive, true));
            return true;
        }
    }
    return false;
}

template<class EventHandler>
bool ParseEngine<EventHandler>::_validate_directive_tag(csubstr *C4_RESTRICT directive, csubstr *C4_RESTRICT handle, csubstr *C4_RESTRICT prefix) const
{
    _c4assert(directive->begins_with("%TAG"));
    csubstr whitespace = " \t";
    size_t handle_start = directive->first_not_of(whitespace, 4);
    if(handle_start != npos && directive->str[handle_start] == '!')
    {
        size_t handle_end = directive->first_of(whitespace, handle_start);
        if(handle_end != npos)
        {
            size_t prefix_start = directive->first_not_of(whitespace, handle_end);
            if(prefix_start != npos)
            {
                size_t prefix_end = directive->first_of(whitespace, prefix_start);
                if(prefix_end == npos)
                    prefix_end = directive->len;
                _set_first_strict(*directive, prefix_end);
                *handle = directive->range(handle_start, handle_end);
                *prefix = directive->range(prefix_start, prefix_end);
                _c4dbgpf("%TAG: handle={} prefix={} full={}", *handle, *prefix, prs_(*directive, true));
                if(is_valid_tag_handle(*handle))
                    return true;
            }
        }
    }
    return false;
}

template<class EventHandler>
void ParseEngine<EventHandler>::_handle_directive(csubstr directive)
{
    _c4dbgpf("handle_directive: rem={}", prs_(directive, true));
    _c4assert(m_evt_handler->m_curr->line_contents.rem.begins_with('%'));
    _c4assert(directive.str == m_evt_handler->m_curr->line_contents.rem.str);
    const char *err = nullptr;
    csubstr rem;
    size_t pos;
    auto isdirective = [](csubstr str, csubstr dir) {
        if(str.begins_with(dir))
        {
            csubstr rest = str.sub(dir.len);
            return (!rest.len || rest.str[0] == ' ' || rest.str[0] == '\t');
        }
        return false;
    };
    if(isdirective(directive, "%TAG"))
    {
        csubstr handle;
        csubstr prefix;
        if C4_UNLIKELY(!_validate_directive_tag(&directive, &handle, &prefix))
        {
            err = "invalid %TAG directive";
            goto directive_error; // NOLINT
        }
        m_evt_handler->add_directive_tag(handle, prefix);
    }
    else if(isdirective(directive, "%YAML"))
    {
        csubstr version;
        if C4_UNLIKELY(!_validate_directive_yaml(&directive, &version))
        {
            err = "invalid %YAML directive";
            goto directive_error; // NOLINT
        }
        if C4_UNLIKELY(m_has_directives_yaml)
        {
            err = "multiple %YAML directives";
            goto directive_error; // NOLINT
        }
        m_has_directives_yaml = true;
        m_evt_handler->add_directive_yaml(version);
    }
    m_has_directives = true;
    rem = m_evt_handler->m_curr->line_contents.rem;
    pos = rem.first_not_of(" \t", directive.len);
    pos = pos != npos ? pos : rem.len;
    _line_progressed(pos);
    rem = rem.sub(pos);
    _c4dbgpf("handle_directive: rest={}", prs_(rem));
    if C4_UNLIKELY(rem.len && !rem.begins_with('#'))
    {
        err = "invalid tokens after directive";
        goto directive_error; // NOLINT
    }
directive_error:
    if C4_UNLIKELY(err != nullptr)
        _c4err(err);
}

template<class EventHandler>
bool ParseEngine<EventHandler>::_handle_bom()
{
    const csubstr rem = m_evt_handler->m_curr->line_contents.rem;
    if(rem.len)
    {
        const csubstr rest = rem.sub(1);
        // https://yaml.org/spec/1.2.2/#52-character-encodings
        #define _rymlisascii(c) ((c) > '\0' && (c) <= '\x7f') // is the character ASCII?
        if(rem.begins_with(csubstr{"\x00\x00\xfe\xff", 4})
           // no bom:
           || (rem.begins_with(csubstr{"\x00\x00\x00", 3}) && rem.len >= 4u && _rymlisascii(rem.str[3])))
        {
            _c4err("UTF32BE not supported");
        }
        else if(rem.begins_with(csubstr{"\xff\xfe\x00\x00", 4})
                // no bom:
                || (rest.begins_with(csubstr{"\x00\x00\x00", 3}) && rem.len >= 4u && _rymlisascii(rem.str[0])))
        {
            _c4err("UTF32LE not supported");
        }
        else if(rem.begins_with("\xfe\xff") || (rem.begins_with('\x00') && rem.len >= 2u && _rymlisascii(rem.str[1])))
        {
            _c4err("UTF16BE not supported");
        }
        else if(rem.begins_with("\xff\xfe") || (rest.begins_with('\x00') && rem.len >= 2u && _rymlisascii(rem.str[0])))
        {
            _c4err("UTF16LE not supported");
        }
        else if(rem.begins_with("\xef\xbb\xbf"))
        {
            _c4dbgp("byte order mark: UTF8");
            _handle_bom(UTF8);
            _line_progressed(3);
            m_bom_len = 3;
            return true;
        }
        #undef _rymlisascii
    }
    return false;
}

template<class EventHandler>
void ParseEngine<EventHandler>::_handle_bom(Encoding_e enc)
{
    if(m_encoding == NOBOM)
    {
        if(enc == UTF8 || /*beginning of file*/(m_evt_handler->m_curr->line_contents.rem.str == _buf().str))
            m_encoding = enc;
        else
            _c4err("non-UTF8 byte order mark can appear only at the beginning of the file"); // LCOV_EXCL_LINE
    }
    else if(enc != m_encoding)
    {
        _c4err("byte order mark can only be set once"); // LCOV_EXCL_LINE
    }
}


//-----------------------------------------------------------------------------

template<class EventHandler>
void ParseEngine<EventHandler>::_handle_seq_json()
{
seqjson_start:
    _c4dbgpf("handle2_seq_json: node_id={} level={} indentation={}", m_evt_handler->m_curr->node_id, m_evt_handler->m_curr->level, m_evt_handler->m_curr->indref);

    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_none(RKEY), m_evt_handler->m_curr->pos);
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_all(RSEQ), m_evt_handler->m_curr->pos);
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_all(RFLOW), m_evt_handler->m_curr->pos);
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_any(RVAL|RNXT), m_evt_handler->m_curr->pos);
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_all(RVAL) != has_all(RNXT), m_evt_handler->m_curr->pos);

    _handle_flow_skip_whitespace();
    csubstr rem = m_evt_handler->m_curr->line_contents.rem;
    if(!rem.len)
        goto seqjson_again;

    if(has_any(RVAL))
    {
        RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_none(RNXT), m_evt_handler->m_curr->pos);
        const char first = rem.str[0];
        _c4dbgpf("mapjson[RVAL]: '{}'", first);
        switch(first)
        {
        case '"':
        {
            _c4dbgp("seqjson[RVAL]: scanning double-quoted scalar");
            ScannedScalar sc = _scan_scalar_dquot();
            csubstr maybe_filtered = _maybe_filter_val_scalar_dquot(sc);
            m_evt_handler->set_val_scalar_dquoted(maybe_filtered);
            addrem_flags(RNXT, RVAL);
            break;
        }
        case '[':
        {
            _c4dbgp("seqjson[RVAL]: start child seqjson");
            addrem_flags(RNXT, RVAL);
            m_evt_handler->begin_seq_val_flow();
            addrem_flags(RVAL, RNXT);
            _line_progressed(1);
            break;
        }
        case '{':
        {
            _c4dbgp("seqjson[RVAL]: start child mapjson");
            addrem_flags(RNXT, RVAL);
            m_evt_handler->begin_map_val_flow();
            addrem_flags(RMAP|RKEY, RSEQ|RVAL|RNXT);
            _line_progressed(1);
            goto seqjson_finish;
        }
        case ']': // this happens on a trailing comma like ", ]"
        {
            _c4dbgp("seqjson[RVAL]: end!");
            rem_flags(RSEQ);
            _end_seq_flow();
            _line_progressed(1);
            if(!has_all(RSEQ|RFLOW))
                goto seqjson_finish;
            break;
        }
        default:
        {
            ScannedScalar sc;
            if(_scan_scalar_seq_json(&sc))
            {
                _c4dbgp("seqjson[RVAL]: it's a plain scalar.");
                csubstr maybe_filtered = _maybe_filter_val_scalar_plain(sc, m_evt_handler->m_curr->indref);
                m_evt_handler->set_val_scalar_plain(maybe_filtered);
                addrem_flags(RNXT, RVAL);
            }
            else
            {
                _c4err("parse error");
            }
        }
        }
    }
    else // RNXT
    {
        RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_any(RNXT), m_evt_handler->m_curr->pos);
        RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_none(RVAL), m_evt_handler->m_curr->pos);
        const char first = rem.str[0];
        _c4dbgpf("mapjson[RNXT]: '{}'", first);
        switch(first)
        {
        case ',':
        {
            _c4dbgp("seqjson[RNXT]: expect next val");
            addrem_flags(RVAL, RNXT);
            m_evt_handler->add_sibling();
            _line_progressed(1);
            break;
        }
        case ']':
        {
            _c4dbgp("seqjson[RNXT]: end!");
            _end_seq_flow();
            _line_progressed(1);
            goto seqjson_finish;
        }
        default:
            _c4err("parse error");
        }
    }

 seqjson_again:
    _c4dbgt("seqjson: go again", 0);
    if(_finished_line())
    {
        if C4_LIKELY(!_finished_file())
        {
            _line_ended();
            _scan_line();
            _c4dbgnextline();
        }
        else
        {
            _c4err("missing terminating ]");
        }
    }
    goto seqjson_start;

 seqjson_finish:
    _c4dbgp("seqjson: finish");
}


//-----------------------------------------------------------------------------

template<class EventHandler>
void ParseEngine<EventHandler>::_handle_map_json()
{
mapjson_start:
    _c4dbgpf("handle2_map_json: node_id={} level={} indentation={}", m_evt_handler->m_curr->node_id, m_evt_handler->m_curr->level, m_evt_handler->m_curr->indref);

    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_all(RMAP), m_evt_handler->m_curr->pos);
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_all(RFLOW), m_evt_handler->m_curr->pos);
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_none(QMRK), m_evt_handler->m_curr->pos);
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_any(RKEY|RKCL|RVAL|RNXT), m_evt_handler->m_curr->pos);
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, 1 == (has_any(RKEY) + has_any(RKCL) + has_any(RVAL) + has_any(RNXT)), m_evt_handler->m_curr->pos);

    _handle_flow_skip_whitespace();
    csubstr rem = m_evt_handler->m_curr->line_contents.rem;
    if(!rem.len)
        goto mapjson_again;

    if(has_any(RKEY))
    {
        RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_none(RKCL), m_evt_handler->m_curr->pos);
        RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_none(RVAL), m_evt_handler->m_curr->pos);
        RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_none(RNXT), m_evt_handler->m_curr->pos);
        RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_none(QMRK), m_evt_handler->m_curr->pos);
        const char first = rem.str[0];
        _c4dbgpf("mapjson[RKEY]: '{}'", first);
        switch(first)
        {
        case '"':
        {
            _c4dbgp("mapjson[RKEY]: scanning double-quoted scalar");
            ScannedScalar sc = _scan_scalar_dquot();
            csubstr maybe_filtered = _maybe_filter_key_scalar_dquot(sc);
            m_evt_handler->set_key_scalar_dquoted(maybe_filtered);
            addrem_flags(RKCL, RKEY);
            break;
        }
        case '}': // this happens on a trailing comma like ", }"
        {
            _c4dbgp("mapjson[RKEY]: end!");
            _end_map_flow();
            _line_progressed(1);
            goto mapjson_finish;
        }
        default:
            _c4err("parse error");
        }
    }
    else if(has_any(RVAL))
    {
        RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_none(RKEY), m_evt_handler->m_curr->pos);
        RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_none(RKCL), m_evt_handler->m_curr->pos);
        RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_none(RNXT), m_evt_handler->m_curr->pos);
        RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_none(QMRK), m_evt_handler->m_curr->pos);
        const char first = rem.str[0];
        _c4dbgpf("mapjson[RVAL]: '{}'", first);
        switch(first)
        {
        case '"':
        {
            _c4dbgp("mapjson[RVAL]: scanning double-quoted scalar");
            ScannedScalar sc = _scan_scalar_dquot();
            csubstr maybe_filtered = _maybe_filter_val_scalar_dquot(sc);
            m_evt_handler->set_val_scalar_dquoted(maybe_filtered);
            addrem_flags(RNXT, RVAL);
            break;
        }
        case '[':
        {
            _c4dbgp("mapjson[RVAL]: start val seqjson");
            addrem_flags(RNXT, RVAL);
            m_evt_handler->begin_seq_val_flow();
            _set_indentation(m_evt_handler->m_parent->indref);
            addrem_flags(RSEQ|RVAL, RMAP|RNXT);
            _line_progressed(1);
            goto mapjson_finish;
        }
        case '{':
        {
            _c4dbgp("mapjson[RVAL]: start val mapjson");
            addrem_flags(RNXT, RVAL);
            m_evt_handler->begin_map_val_flow();
            _set_indentation(m_evt_handler->m_parent->indref);
            addrem_flags(RKEY, RNXT);
            _line_progressed(1);
            // keep going in this function
            break;
        }
        default:
        {
            ScannedScalar sc;
            if(_scan_scalar_map_json(&sc))
            {
                _c4dbgp("mapjson[RVAL]: plain scalar.");
                csubstr maybe_filtered = _maybe_filter_val_scalar_plain(sc, m_evt_handler->m_curr->indref);
                m_evt_handler->set_val_scalar_plain(maybe_filtered);
                addrem_flags(RNXT, RVAL);
            }
            else
            {
                _c4err("parse error");
            }
            break;
        }
        }
    }
    else if(has_any(RKCL)) // read the key colon
    {
        RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_none(RKEY), m_evt_handler->m_curr->pos);
        RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_none(RVAL), m_evt_handler->m_curr->pos);
        RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_none(RNXT), m_evt_handler->m_curr->pos);
        RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_none(QMRK), m_evt_handler->m_curr->pos);
        const char first = rem.str[0];
        _c4dbgpf("mapjson[RKCL]: '{}'", first);
        if(first == ':')
        {
            _c4dbgp("mapjson[RKCL]: found the colon");
            addrem_flags(RVAL, RKCL);
            _line_progressed(1);
        }
        else
        {
            _c4err("parse error");
        }
    }
    else if(has_any(RNXT))
    {
        RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_none(RKEY), m_evt_handler->m_curr->pos);
        RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_none(RKCL), m_evt_handler->m_curr->pos);
        RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_none(RVAL), m_evt_handler->m_curr->pos);
        RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_none(QMRK), m_evt_handler->m_curr->pos);
        _c4dbgpf("mapjson[RNXT]: '{}'", rem.str[0]);
        if(rem.begins_with(','))
        {
            _c4dbgp("mapjson[RNXT]: expect next keyval");
            m_evt_handler->add_sibling();
            addrem_flags(RKEY, RNXT);
            _line_progressed(1);
        }
        else if(rem.begins_with('}'))
        {
            _c4dbgp("mapjson[RNXT]: end!");
            _end_map_flow();
            _line_progressed(1);
            goto mapjson_finish;
        }
        else
        {
            _c4err("parse error"); // LCOV_EXCL_LINE
        }
    }

 mapjson_again:
    _c4dbgt("mapjson: go again", 0);
    if(_finished_line())
    {
        if C4_LIKELY(!_finished_file())
        {
            _line_ended();
            _scan_line();
            _c4dbgnextline();
        }
        else
        {
            _c4err("missing terminating }");
        }
    }
    goto mapjson_start;

 mapjson_finish:
    _c4dbgp("mapjson: finish");
}


//-----------------------------------------------------------------------------

template<class EventHandler>
void ParseEngine<EventHandler>::_handle_seq_imap()
{
seqimap_start:
    _c4dbgpf("handle2_seq_imap: node_id={} level={} indref={}", m_evt_handler->m_curr->node_id, m_evt_handler->m_curr->level, m_evt_handler->m_curr->indref);

    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_all(RSEQIMAP), m_evt_handler->m_curr->pos);
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_none(RKEY), m_evt_handler->m_curr->pos);
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_any(RVAL|RNXT|QMRK|RKCL), m_evt_handler->m_curr->pos);
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, 1 == has_all(RVAL) + has_all(RNXT) + has_all(QMRK) + has_all(RKCL), m_evt_handler->m_curr->pos);
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, m_evt_handler->m_stack.size() >= 3, m_evt_handler->m_curr->pos);

    _handle_flow_skip_whitespace();
    csubstr rem = m_evt_handler->m_curr->line_contents.rem;
    if(!rem.len)
        goto seqimap_again;

    if(has_any(RVAL))
    {
        RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_any(RVAL), m_evt_handler->m_curr->pos);
        RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_none(RNXT), m_evt_handler->m_curr->pos);
        RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_none(QMRK), m_evt_handler->m_curr->pos);
        RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_none(RKCL), m_evt_handler->m_curr->pos);
        const char first = rem.str[0];
        _c4dbgpf("seqimap[RVAL]: '{}'", _c4prc(first));
        ScannedScalar sc;
        if(first == '\'')
        {
            _c4dbgp("seqimap[RVAL]: scanning single-quoted scalar");
            sc = _scan_scalar_squot();
            csubstr maybe_filtered = _maybe_filter_val_scalar_squot(sc);
            _handle_annotations_before_blck_val_scalar();
            m_evt_handler->set_val_scalar_squoted(maybe_filtered);
            _end_map_flow();
            goto seqimap_finish;
        }
        else if(first == '"')
        {
            _c4dbgp("seqimap[RVAL]: scanning double-quoted scalar");
            sc = _scan_scalar_dquot();
            csubstr maybe_filtered = _maybe_filter_val_scalar_dquot(sc);
            _handle_annotations_before_blck_val_scalar();
            m_evt_handler->set_val_scalar_dquoted(maybe_filtered);
            _end_map_flow();
            goto seqimap_finish;
        }
        // block scalars (ie | and >) cannot appear in flow containers
        else if(_scan_scalar_plain_map_flow(&sc))
        {
            _c4dbgp("seqimap[RVAL]: it's a scalar.");
            csubstr maybe_filtered = _maybe_filter_val_scalar_plain(sc, m_evt_handler->m_curr->indref);
            _handle_annotations_before_blck_val_scalar();
            m_evt_handler->set_val_scalar_plain(maybe_filtered);
            _end_map_flow();
            goto seqimap_finish;
        }
        else if(first == '[')
        {
            _c4dbgp("seqimap[RVAL]: start child seqflow");
            addrem_flags(RNXT, RVAL);
            _handle_annotations_before_blck_val_scalar();
            m_evt_handler->begin_seq_val_flow();
            addrem_flags(RVAL, RNXT|RSEQIMAP);
            _set_indentation(m_evt_handler->m_parent->indref);
            _line_progressed(1);
            goto seqimap_finish;
        }
        else if(first == '{')
        {
            _c4dbgp("seqimap[RVAL]: start child mapflow");
            addrem_flags(RNXT, RVAL);
            _handle_annotations_before_blck_val_scalar();
            m_evt_handler->begin_map_val_flow();
            addrem_flags(RMAP|RKEY, RSEQ|RVAL|RSEQIMAP|RNXT);
            _set_indentation(m_evt_handler->m_parent->indref);
            _line_progressed(1);
            goto seqimap_finish;
        }
        else if(first == ',' || first == ']')
        {
            _c4dbgp("seqimap[RVAL]: finish without val.");
            _handle_annotations_before_blck_val_scalar();
            m_evt_handler->set_val_scalar_plain_empty();
            _end_map_flow();
            goto seqimap_finish;
        }
        else if(first == '*')
        {
            csubstr ref = _scan_ref_seq();
            _c4dbgpf("seqimap[RVAL]: ref! {}", prs_(ref));
            _handle_valref(ref);
            addrem_flags(RNXT, RVAL);
        }
        else if(first == '&')
        {
            csubstr anchor = _scan_anchor();
            _c4dbgpf("seqimap[RVAL]: anchor! {}", prs_(anchor));
            _add_annotation(&m_pending_anchors, anchor);
        }
        else if(first == '!')
        {
            csubstr tag = _scan_tag();
            _c4dbgpf("seqimap[RVAL]: tag! {}", prs_(tag));
            _add_annotation(&m_pending_tags, tag);
        }
        else
        {
            _c4err("parse error"); // LCOV_EXCL_LINE
        }
    }
    else if(has_any(RNXT))
    {
        RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_any(RNXT), m_evt_handler->m_curr->pos);
        RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_none(RVAL), m_evt_handler->m_curr->pos);
        RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_none(QMRK), m_evt_handler->m_curr->pos);
        RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_none(RKCL), m_evt_handler->m_curr->pos);
        const char first = rem.str[0];
        _c4dbgpf("seqimap[RNXT]: '{}'", _c4prc(first));
        if(first == ',' || first == ']')
        {
            // we may get here because a map or a seq started and we
            // return later
            _c4dbgp("seqimap: done");
            _end_map_flow();
            goto seqimap_finish;
        }
        else
        {
            _c4err("parse error"); // LCOV_EXCL_LINE
        }
    }
    else if(has_any(QMRK))
    {
        RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_any(QMRK), m_evt_handler->m_curr->pos);
        RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_none(RVAL), m_evt_handler->m_curr->pos);
        RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_none(RNXT), m_evt_handler->m_curr->pos);
        RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_none(RKCL), m_evt_handler->m_curr->pos);
        const char first = rem.str[0];
        _c4dbgpf("seqimap[QMRK]: '{}'", _c4prc(first));
        ScannedScalar sc;
        if(first == '\'')
        {
            _c4dbgp("seqimap[QMRK]: scanning single-quoted scalar");
            sc = _scan_scalar_squot();
            csubstr maybe_filtered = _maybe_filter_key_scalar_squot(sc);
            m_evt_handler->set_key_scalar_squoted(maybe_filtered);
            addrem_flags(RKCL, QMRK);
            goto seqimap_again;
        }
        else if(first == '"')
        {
            _c4dbgp("seqimap[QMRK]: scanning double-quoted scalar");
            sc = _scan_scalar_dquot();
            csubstr maybe_filtered = _maybe_filter_key_scalar_dquot(sc);
            m_evt_handler->set_key_scalar_dquoted(maybe_filtered);
            addrem_flags(RKCL, QMRK);
            goto seqimap_again;
        }
        // block scalars (ie | and >) cannot appear in flow containers
        else if(_scan_scalar_plain_map_flow(&sc))
        {
            _c4dbgp("seqimap[QMRK]: it's a scalar.");
            csubstr maybe_filtered = _maybe_filter_key_scalar_plain(sc, m_evt_handler->m_curr->indref);
            m_evt_handler->set_key_scalar_plain(maybe_filtered);
            addrem_flags(RKCL, QMRK);
            goto seqimap_again;
        }
        else if(first == '[')
        {
            _c4dbgp("seqimap[QMRK]: start child seqflow");
            addrem_flags(RKCL, QMRK);
            m_evt_handler->begin_seq_key_flow();
            addrem_flags(RSEQ|RVAL, RKCL|RSEQIMAP);
            _set_indentation(m_evt_handler->m_parent->indref);
            _line_progressed(1);
            goto seqimap_finish;
        }
        else if(first == '{')
        {
            _c4dbgp("seqimap[QMRK]: start child mapflow");
            addrem_flags(RKCL, QMRK);
            m_evt_handler->begin_map_key_flow();
            addrem_flags(RMAP|RKEY, RSEQ|RKCL|RSEQIMAP);
            _set_indentation(m_evt_handler->m_parent->indref);
            _line_progressed(1);
            goto seqimap_finish;
        }
        else if(first == ',' || first == ']')
        {
            _c4dbgp("seqimap[QMRK]: finish without key.");
            m_evt_handler->set_key_scalar_plain_empty();
            m_evt_handler->set_val_scalar_plain_empty();
            _end_map_flow();
            goto seqimap_finish;
        }
        else if(first == '&')
        {
            csubstr anchor = _scan_anchor();
            _c4dbgp("seqimap[QMRK]: anchor!");
            m_evt_handler->set_key_anchor(anchor);
        }
        else if(first == '*')
        {
            csubstr ref = _scan_ref_seq();
            _c4dbgp("seqimap[QMRK]: ref!");
            _handle_keyref(ref);
            addrem_flags(RKCL, QMRK);
        }
        else
        {
            _c4err("parse error"); // LCOV_EXCL_LINE
        }
    }
    else if(has_any(RKCL))
    {
        RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_none(RVAL), m_evt_handler->m_curr->pos);
        RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_none(RNXT), m_evt_handler->m_curr->pos);
        RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_none(QMRK), m_evt_handler->m_curr->pos);
        RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_any(RKCL), m_evt_handler->m_curr->pos);
        const char first = rem.str[0];
        _c4dbgpf("seqimap[RKCL]: '{}'", _c4prc(first));
        if(first == ':')
        {
            _c4dbgp("seqimap[RKCL]: found ':'");
            addrem_flags(RVAL, RKCL);
            _line_progressed(1);
            goto seqimap_again;
        }
        else if(first == ',' || first == ']')
        {
            _c4dbgp("seqimap[RKCL]: found ','. finish without val");
            m_evt_handler->set_val_scalar_plain_empty();
            _end_map_flow();
            goto seqimap_finish;
        }
        else
        {
            _c4err("parse error"); // LCOV_EXCL_LINE
        }
    }

 seqimap_again:
    _c4dbgt("seqimap: go again", 0);
    if(_finished_line())
    {
        if C4_LIKELY(!_finished_file())
        {
            _line_ended();
            _scan_line();
            _c4dbgnextline();
        }
        else
        {
            _c4err("parse error");
        }
    }
    goto seqimap_start;

 seqimap_finish:
    _c4dbgp("seqimap: finish");
}


//-----------------------------------------------------------------------------

template<class EventHandler>
void ParseEngine<EventHandler>::_handle_seq_flow()
{
seqflow_start:
    _c4dbgpf("handle_seq_flow: node_id={} level={} indentation={}", m_evt_handler->m_curr->node_id, m_evt_handler->m_curr->level, m_evt_handler->m_curr->indref);

    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_none(RKEY), m_evt_handler->m_curr->pos);
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_all(RSEQ), m_evt_handler->m_curr->pos);
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_all(RFLOW), m_evt_handler->m_curr->pos);
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_any(RVAL|RNXT), m_evt_handler->m_curr->pos);
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_all(RVAL) != has_all(RNXT), m_evt_handler->m_curr->pos);
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, m_evt_handler->m_curr->indref != npos, m_evt_handler->m_curr->pos);

    if(m_evt_handler->m_curr->at_line_beginning())
    {
        _handle_flow_line_beginning();
    }

    _handle_flow_skip_whitespace();
    if(!m_evt_handler->m_curr->line_contents.rem.len)
        goto seqflow_again;

    if(has_any(RVAL))
    {
        RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_none(RNXT), m_evt_handler->m_curr->pos);
        const char first = m_evt_handler->m_curr->line_contents.rem.str[0];
        ScannedScalar sc;
        if(first == '\'')
        {
            _c4dbgp("seqflow[RVAL]: scanning single-quoted scalar");
            sc = _scan_scalar_squot();
            csubstr maybe_filtered = _maybe_filter_val_scalar_squot(sc);
            _handle_annotations_before_blck_val_scalar();
            m_evt_handler->set_val_scalar_squoted(maybe_filtered);
            addrem_flags(RNXT, RVAL);
            _mark_seqflow_val_end();
        }
        else if(first == '"')
        {
            _c4dbgp("seqflow[RVAL]: scanning double-quoted scalar");
            sc = _scan_scalar_dquot();
            csubstr maybe_filtered = _maybe_filter_val_scalar_dquot(sc);
            _handle_annotations_before_blck_val_scalar();
            m_evt_handler->set_val_scalar_dquoted(maybe_filtered);
            addrem_flags(RNXT, RVAL);
            _mark_seqflow_val_end();
        }
        // block scalars (ie | and >) cannot appear in flow containers
        else if(_scan_scalar_plain_seq_flow(&sc))
        {
            _c4dbgp("seqflow[RVAL]: it's a scalar.");
            csubstr maybe_filtered = _maybe_filter_val_scalar_plain(sc, m_evt_handler->m_curr->indref);
            _handle_annotations_before_blck_val_scalar();
            m_evt_handler->set_val_scalar_plain(maybe_filtered);
            addrem_flags(RNXT, RVAL);
            _mark_seqflow_val_end();
        }
        else if(first == '[')
        {
            _c4dbgp("seqflow[RVAL]: start child seqflow");
            addrem_flags(RNXT, RVAL);
            _handle_annotations_before_blck_val_scalar();
            m_evt_handler->begin_seq_val_flow();
            _set_indentation(m_evt_handler->m_parent->indref);
            addrem_flags(RVAL, RNXT);
            _line_progressed(1);
        }
        else if(first == '{')
        {
            _c4dbgp("seqflow[RVAL]: start child mapflow");
            addrem_flags(RNXT, RVAL);
            _handle_annotations_before_blck_val_scalar();
            m_evt_handler->begin_map_val_flow();
            _set_indentation(m_evt_handler->m_parent->indref);
            addrem_flags(RMAP|RKEY, RSEQ|RVAL|RNXT);
            _line_progressed(1);
            goto seqflow_finish;
        }
        else if(first == ']') // this happens on cases such as [] or [.., ]
        {
            _c4dbgp("seqflow[RVAL]: end!");
            if(m_pending_anchors.num_entries | m_pending_tags.num_entries)
            {
                _c4dbgp("seqflow[RVAL]: add pending annotations");
                _handle_annotations_before_blck_val_scalar();
                m_evt_handler->set_val_scalar_plain_empty();
            }
            _line_progressed(1);
            _end_seq_flow();
            goto seqflow_finish;
        }
        else if(first == '*')
        {
            csubstr ref = _scan_ref_seq();
            _c4dbgpf("seqflow[RVAL]: ref! {}", prs_(ref));
            _handle_valref(ref);
            addrem_flags(RNXT, RVAL);
        }
        else if(first == '&')
        {
            csubstr anchor = _scan_anchor();
            _c4dbgpf("seqflow[RVAL]: anchor! {}", prs_(anchor));
            _add_annotation(&m_pending_anchors, anchor);
        }
        else if(first == '!')
        {
            csubstr tag = _scan_tag();
            _c4dbgpf("seqflow[RVAL]: tag! {}", prs_(tag));
            _add_annotation(&m_pending_tags, tag);
        }
        else if(first == ':')
        {
            _c4dbgpf("seqflow[RVAL]: actually seqimap at node[{}], with empty key", m_evt_handler->m_curr->node_id);
            addrem_flags(RNXT, RVAL);
            m_evt_handler->begin_map_val_flow();
            _set_indentation(m_evt_handler->m_parent->indref);
            _handle_annotations_before_blck_key_scalar();
            m_evt_handler->set_key_scalar_plain_empty();
            addrem_flags(RSEQIMAP|RVAL, RSEQ|RNXT);
            _line_progressed(1);
            goto seqflow_finish;
        }
        else if(first == '?')
        {
            _c4dbgp("seqflow[RVAL]: start child mapflow, explicit key");
            addrem_flags(RNXT, RVAL);
            m_evt_handler->begin_map_val_flow();
            _set_indentation(m_evt_handler->m_parent->indref);
            addrem_flags(RSEQIMAP|QMRK, RSEQ|RNXT);
            _line_progressed(1);
            _maybe_skip_whitespace_tokens();
            goto seqflow_finish;
        }
        else if(first == ',')
        {
            if(m_pending_anchors.num_entries || m_pending_tags.num_entries)
            {
                _c4dbgp("seqflow[RVAL]: add pending annotations");
                _handle_annotations_before_blck_val_scalar();
                m_evt_handler->set_val_scalar_plain_empty();
                addrem_flags(RNXT, RVAL);
                _mark_seqflow_val_end();
            }
            else
            {
                _c4err("parse error");
            }
        }
        else
        {
            _c4err("parse error");
        }
    }
    else // RNXT
    {
        RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_any(RNXT), m_evt_handler->m_curr->pos);
        RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_none(RVAL), m_evt_handler->m_curr->pos);
        const char first = m_evt_handler->m_curr->line_contents.rem.str[0];
        if(first == ',')
        {
            _c4dbgp("seqflow[RNXT]: expect next val");
            addrem_flags(RVAL, RNXT);
            m_evt_handler->add_sibling();
            _line_progressed(1);
            if(m_evt_handler->m_curr->line_contents.rem.begins_with('#'))
            {
                _c4err("parse error: invalid comment after comma");
            }
            _mark_seqflow_val_end();
        }
        else if(first == ']')
        {
            _c4dbgp("seqflow[RNXT]: end!");
            _line_progressed(1);
            _end_seq_flow();
            goto seqflow_finish;
        }
        else if(first == ':')
        {
            _c4dbgpf("seqflow[RNXT]: line@valend={} line@now={}", m_prev_val_end, m_evt_handler->m_curr->pos.line);
            if(m_prev_val_end != NONE && m_evt_handler->m_curr->pos.line == m_prev_val_end)
            {
                _c4dbgpf("seqflow[RNXT]: actually seqimap at node[{}]", m_evt_handler->m_curr->node_id);
                m_evt_handler->actually_val_is_first_key_of_new_map_flow();
                _set_indentation(m_evt_handler->m_parent->indref);
                _line_progressed(1);
                addrem_flags(RSEQIMAP|RVAL, RNXT);
                goto seqflow_finish;
            }
            else
            {
                _c4err("parse error");
            }
        }
        else
        {
            _c4err("parse error");
        }
    }

 seqflow_again:
    _c4dbgt("seqflow: go again", 0);
    if(_finished_line())
    {
        if C4_LIKELY(!_finished_file())
        {
            _line_ended();
            _scan_line();
            _c4dbgnextline();
        }
        else
        {
            _c4err("missing terminating ]");
        }
    }
    goto seqflow_start;

 seqflow_finish:
    _c4dbgp("seqflow: finish");
}


//-----------------------------------------------------------------------------

template<class EventHandler>
void ParseEngine<EventHandler>::_handle_map_flow()
{
mapflow_start:
    _c4dbgpf("handle_map_flow: node_id={} level={} indentation={}", m_evt_handler->m_curr->node_id, m_evt_handler->m_curr->level, m_evt_handler->m_curr->indref);

    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_all(RMAP), m_evt_handler->m_curr->pos);
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_all(RFLOW), m_evt_handler->m_curr->pos);
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_any(RKEY|RKCL|RVAL|RNXT|QMRK), m_evt_handler->m_curr->pos);
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, 1 == (has_any(RKEY) + has_any(RKCL) + has_any(RVAL) + has_any(RNXT) + has_any(QMRK)), m_evt_handler->m_curr->pos);

    if(m_evt_handler->m_curr->at_line_beginning())
    {
        _handle_flow_line_beginning();
    }

    _handle_flow_skip_whitespace();
    if(!m_evt_handler->m_curr->line_contents.rem.len)
        goto mapflow_again;

    if(has_any(RKEY))
    {
        RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_none(RKCL), m_evt_handler->m_curr->pos);
        RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_none(RVAL), m_evt_handler->m_curr->pos);
        RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_none(RNXT), m_evt_handler->m_curr->pos);
        RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_none(QMRK), m_evt_handler->m_curr->pos);
        const char first = m_evt_handler->m_curr->line_contents.rem.str[0];
        _c4dbgpf("mapflow[RKEY]: '{}'", first);
        ScannedScalar sc;
        if(first == '\'')
        {
            _c4dbgp("mapflow[RKEY]: scanning single-quoted scalar");
            sc = _scan_scalar_squot();
            csubstr maybe_filtered = _maybe_filter_key_scalar_squot(sc);
            _handle_annotations_before_blck_key_scalar();
            m_evt_handler->set_key_scalar_squoted(maybe_filtered);
            addrem_flags(RKCL, RKEY|QMRK);
        }
        else if(first == '"')
        {
            _c4dbgp("mapflow[RKEY]: scanning double-quoted scalar");
            sc = _scan_scalar_dquot();
            csubstr maybe_filtered = _maybe_filter_key_scalar_dquot(sc);
            _handle_annotations_before_blck_key_scalar();
            m_evt_handler->set_key_scalar_dquoted(maybe_filtered);
            addrem_flags(RKCL, RKEY|QMRK);
        }
        // block scalars (ie | and >) cannot appear in flow containers
        else if(_scan_scalar_plain_map_flow(&sc))
        {
            _c4dbgp("mapflow[RKEY]: plain scalar");
            csubstr maybe_filtered = _maybe_filter_key_scalar_plain(sc, m_evt_handler->m_curr->indref);
            _handle_annotations_before_blck_key_scalar();
            m_evt_handler->set_key_scalar_plain(maybe_filtered);
            addrem_flags(RKCL, RKEY|QMRK);
        }
        else if(first == '?')
        {
            _c4dbgp("mapflow[RKEY]: explicit key");
            _handle_annotations_before_blck_key_scalar();
            addrem_flags(QMRK, RKEY);
            _line_progressed(1);
            _maybe_skip_whitespace_tokens();
        }
        else if(first == ':')
        {
            _c4dbgp("mapflow[RKEY]: setting empty key");
            _handle_annotations_before_blck_key_scalar();
            m_evt_handler->set_key_scalar_plain_empty();
            addrem_flags(RVAL, RKEY|QMRK);
            _line_progressed(1);
            _maybe_skip_whitespace_tokens();
        }
        else if(first == ',')
        {
            _c4dbgp("mapflow[RKEY]: comma!");
            if(!_handle_annotations_before_unexpected_flow_token_rkey())
                _c4err("unexpected comma");
            addrem_flags(RNXT, RKEY|QMRK);
            // keep going in this function
        }
        else if(first == '}') // this happens on a trailing comma like ", }"
        {
            _c4dbgp("mapflow[RKEY]: end!");
            (void)_handle_annotations_before_unexpected_flow_token_rkey();
            _line_progressed(1);
            _end_map_flow();
            goto mapflow_finish;
        }
        else if(first == '&')
        {
            csubstr anchor = _scan_anchor();
            _c4dbgpf("mapflow[RKEY]: key anchor! {}", prs_(anchor));
            _add_annotation(&m_pending_anchors, anchor);
        }
        else if(first == '!')
        {
            csubstr tag = _scan_tag();
            _c4dbgpf("mapflow[RKEY]: tag! {}", prs_(tag));
            _add_annotation(&m_pending_tags, tag);
        }
        else if(first == '*')
        {
            csubstr ref = _scan_ref_map();
            _c4dbgpf("mapflow[RKEY]: key ref! {}", prs_(ref));
            _handle_keyref(ref);
            addrem_flags(RKCL, RKEY);
        }
        else if(first == '[')
        {
            // RYML's tree cannot store container keys, but that's
            // handled inside the tree event handler. Other handler
            // types may be able to handle it.
            _c4dbgp("mapflow[RKEY]: start child seqflow (!)");
            _handle_annotations_before_blck_key_scalar();
            addrem_flags(RKCL, RKEY);
            m_evt_handler->begin_seq_key_flow();
            addrem_flags(RSEQ|RVAL, RMAP|RKCL);
            _set_indentation(m_evt_handler->m_parent->indref);
            _line_progressed(1);
            goto mapflow_finish;
        }
        else if(first == '{')
        {
            // RYML's tree cannot store container keys, but that's
            // handled inside the tree event handler. Other handler
            // types may be able to handle it.
            _c4dbgp("mapflow[RKEY]: start child mapflow (!)");
            _handle_annotations_before_blck_key_scalar();
            addrem_flags(RKCL, RKEY);
            m_evt_handler->begin_map_key_flow();
            addrem_flags(RKEY, RVAL|RKCL);
            _set_indentation(m_evt_handler->m_parent->indref);
            _line_progressed(1);
            // keep going in this function
        }
        else
        {
            _c4err("parse error"); // LCOV_EXCL_LINE
        }
    }
    else if(has_any(RKCL)) // read the key colon
    {
        RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_none(RKEY), m_evt_handler->m_curr->pos);
        RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_none(RVAL), m_evt_handler->m_curr->pos);
        RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_none(RNXT), m_evt_handler->m_curr->pos);
        RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_none(QMRK), m_evt_handler->m_curr->pos);
        const char first = m_evt_handler->m_curr->line_contents.rem.str[0];
        _c4dbgpf("mapflow[RKCL]: '{}'", first);
        if(first == ':')
        {
            _c4dbgp("mapflow[RKCL]: found the colon");
            addrem_flags(RVAL, RKCL);
            _line_progressed(1);
        }
        else if(first == '}')
        {
            _c4dbgp("mapflow[RKCL]: end with missing val!");
            addrem_flags(RVAL, RKCL);
            m_evt_handler->set_val_scalar_plain_empty();
            _line_progressed(1);
            _end_map_flow();
            goto mapflow_finish;
        }
        else if(first == ',')
        {
            _c4dbgp("mapflow[RKCL]: got comma. val is missing");
            m_evt_handler->set_val_scalar_plain_empty();
            m_evt_handler->add_sibling();
            addrem_flags(RKEY, RKCL);
            _line_progressed(1);
            if(m_evt_handler->m_curr->line_contents.rem.begins_with('#'))
            {
                _c4err("parse error: invalid comment after comma");
            }
        }
        else
        {
            _c4err("parse error");
        }
    }
    else if(has_any(RVAL))
    {
        RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_none(RKEY), m_evt_handler->m_curr->pos);
        RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_none(RKCL), m_evt_handler->m_curr->pos);
        RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_none(RNXT), m_evt_handler->m_curr->pos);
        RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_none(QMRK), m_evt_handler->m_curr->pos);
        const char first = m_evt_handler->m_curr->line_contents.rem.str[0];
        _c4dbgpf("mapflow[RVAL]: '{}'", first);
        ScannedScalar sc;
        if(first == '\'')
        {
            _c4dbgp("mapflow[RVAL]: scanning single-quoted scalar");
            sc = _scan_scalar_squot();
            csubstr maybe_filtered = _maybe_filter_val_scalar_squot(sc);
            _handle_annotations_before_blck_val_scalar();
            m_evt_handler->set_val_scalar_squoted(maybe_filtered);
            addrem_flags(RNXT, RVAL);
        }
        else if(first == '"')
        {
            _c4dbgp("mapflow[RVAL]: scanning double-quoted scalar");
            sc = _scan_scalar_dquot();
            csubstr maybe_filtered = _maybe_filter_val_scalar_dquot(sc);
            _handle_annotations_before_blck_val_scalar();
            m_evt_handler->set_val_scalar_dquoted(maybe_filtered);
            addrem_flags(RNXT, RVAL);
        }
        // block scalars (ie | and >) cannot appear in flow containers
        else if(_scan_scalar_plain_map_flow(&sc))
        {
            _c4dbgp("mapflow[RVAL]: plain scalar.");
            csubstr maybe_filtered = _maybe_filter_val_scalar_plain(sc, m_evt_handler->m_curr->indref);
            _handle_annotations_before_blck_val_scalar();
            m_evt_handler->set_val_scalar_plain(maybe_filtered);
            addrem_flags(RNXT, RVAL);
        }
        else if(first == '[')
        {
            _c4dbgp("mapflow[RVAL]: start val seqflow");
            addrem_flags(RNXT, RVAL);
            _handle_annotations_before_blck_val_scalar();
            m_evt_handler->begin_seq_val_flow();
            _set_indentation(m_evt_handler->m_parent->indref);
            addrem_flags(RSEQ|RVAL, RMAP|RNXT);
            _line_progressed(1);
            goto mapflow_finish;
        }
        else if(first == '{')
        {
            _c4dbgp("mapflow[RVAL]: start val mapflow");
            addrem_flags(RNXT, RVAL);
            _handle_annotations_before_blck_val_scalar();
            m_evt_handler->begin_map_val_flow();
            _set_indentation(m_evt_handler->m_parent->indref);
            addrem_flags(RKEY, RNXT);
            _line_progressed(1);
            // keep going in this function
        }
        else if(first == '}')
        {
            _c4dbgp("mapflow[RVAL]: end!");
            _handle_annotations_before_blck_val_scalar();
            m_evt_handler->set_val_scalar_plain_empty();
            _line_progressed(1);
            _end_map_flow();
            goto mapflow_finish;
        }
        else if(first == ',')
        {
            _c4dbgp("mapflow[RVAL]: empty val!");
            _handle_annotations_before_blck_val_scalar();
            m_evt_handler->set_val_scalar_plain_empty();
            addrem_flags(RNXT, RVAL);
            // keep going in this function
        }
        else if(first == '*')
        {
            csubstr ref = _scan_ref_map();
            _c4dbgpf("mapflow[RVAL]: key ref! {}", prs_(ref));
            _handle_valref(ref);
            addrem_flags(RNXT, RVAL);
        }
        else if(first == '&')
        {
            csubstr anchor = _scan_anchor();
            _c4dbgpf("mapflow[RVAL]: key anchor! {}", prs_(anchor));
            _add_annotation(&m_pending_anchors, anchor);
        }
        else if(first == '!')
        {
            csubstr tag = _scan_tag();
            _c4dbgpf("mapflow[RVAL]: tag! {}", prs_(tag));
            _add_annotation(&m_pending_tags, tag);
        }
        else
        {
            _c4err("parse error");
        }
    }
    else if(has_any(RNXT))
    {
        RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_none(RKEY), m_evt_handler->m_curr->pos);
        RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_none(RKCL), m_evt_handler->m_curr->pos);
        RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_none(RVAL), m_evt_handler->m_curr->pos);
        RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_none(QMRK), m_evt_handler->m_curr->pos);
        _c4dbgpf("mapflow[RNXT]: '{}'", m_evt_handler->m_curr->line_contents.rem.str[0]);
        if(m_evt_handler->m_curr->line_contents.rem.begins_with(','))
        {
            _c4dbgp("mapflow[RNXT]: expect next keyval");
            m_evt_handler->add_sibling();
            addrem_flags(RKEY, RNXT);
            _line_progressed(1);
            if(m_evt_handler->m_curr->line_contents.rem.begins_with('#'))
            {
                _c4err("parse error: invalid comment after comma");
            }
        }
        else if(m_evt_handler->m_curr->line_contents.rem.begins_with('}'))
        {
            _c4dbgp("mapflow[RNXT]: end!");
            _line_progressed(1);
            _end_map_flow();
            goto mapflow_finish;
        }
        else
        {
            _c4err("parse error");
        }
    }
    else if(has_any(QMRK))
    {
        RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_none(RKEY), m_evt_handler->m_curr->pos);
        RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_none(RKCL), m_evt_handler->m_curr->pos);
        RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_none(RVAL), m_evt_handler->m_curr->pos);
        RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_none(RNXT), m_evt_handler->m_curr->pos);
        const char first = m_evt_handler->m_curr->line_contents.rem.str[0];
        _c4dbgpf("mapflow[QMRK]: '{}'", first);
        ScannedScalar sc;
        if(first == '\'')
        {
            _c4dbgp("mapflow[QMRK]: scanning single-quoted scalar");
            sc = _scan_scalar_squot();
            csubstr maybe_filtered = _maybe_filter_key_scalar_squot(sc);
            _handle_annotations_before_blck_key_scalar();
            m_evt_handler->set_key_scalar_squoted(maybe_filtered);
            addrem_flags(RKCL, QMRK);
        }
        else if(first == '"')
        {
            _c4dbgp("mapflow[QMRK]: scanning double-quoted scalar");
            sc = _scan_scalar_dquot();
            csubstr maybe_filtered = _maybe_filter_key_scalar_dquot(sc);
            _handle_annotations_before_blck_key_scalar();
            m_evt_handler->set_key_scalar_dquoted(maybe_filtered);
            addrem_flags(RKCL, QMRK);
        }
        // block scalars (ie | and >) cannot appear in flow containers
        else if(_scan_scalar_plain_map_flow(&sc))
        {
            _c4dbgp("mapflow[QMRK]: plain scalar");
            csubstr maybe_filtered = _maybe_filter_key_scalar_plain(sc, m_evt_handler->m_curr->indref);
            _handle_annotations_before_blck_key_scalar();
            m_evt_handler->set_key_scalar_plain(maybe_filtered);
            addrem_flags(RKCL, QMRK);
        }
        else if(first == ':')
        {
            _c4dbgp("mapflow[QMRK]: setting empty key");
            _handle_annotations_before_blck_key_scalar();
            m_evt_handler->set_key_scalar_plain_empty();
            addrem_flags(RVAL, QMRK);
            _line_progressed(1);
            _maybe_skip_whitespace_tokens();
        }
        else if(first == '}') // this happens on a trailing comma like ", }"
        {
            _c4dbgp("mapflow[QMRK]: end!");
            _handle_annotations_before_blck_key_scalar();
            m_evt_handler->set_key_scalar_plain_empty();
            m_evt_handler->set_val_scalar_plain_empty();
            _end_map_flow();
            _line_progressed(1);
            goto mapflow_finish;
        }
        else if(first == ',')
        {
            _c4dbgp("mapflow[QMRK]: empty key+val!");
            _handle_annotations_before_blck_key_scalar();
            m_evt_handler->set_key_scalar_plain_empty();
            m_evt_handler->set_val_scalar_plain_empty();
            addrem_flags(RNXT, QMRK);
        }
        else if(first == '&')
        {
            csubstr anchor = _scan_anchor();
            _c4dbgpf("mapflow[QMRK]: key anchor! {}", prs_(anchor));
            _add_annotation(&m_pending_anchors, anchor);
        }
        else if(first == '*')
        {
            csubstr ref = _scan_ref_map();
            _c4dbgpf("mapflow[QMRK]: key ref! {}", prs_(ref));
            _handle_keyref(ref);
            addrem_flags(RKCL, QMRK);
        }
        else if(first == '[')
        {
            // RYML's tree cannot store container keys, but that's
            // handled inside the tree sink. Other sink types may be
            // able to handle it.
            _c4dbgp("mapflow[QMRK]: start child seqflow (!)");
            addrem_flags(RKCL, QMRK);
            _handle_annotations_before_blck_key_scalar();
            m_evt_handler->begin_seq_key_flow();
            addrem_flags(RSEQ|RVAL, RMAP|RKCL);
            _set_indentation(m_evt_handler->m_parent->indref);
            _line_progressed(1);
            goto mapflow_finish;
        }
        else if(first == '{')
        {
            // RYML's tree cannot store container keys, but that's
            // handled inside the tree sink. Other sink types may be
            // able to handle it.
            _c4dbgp("mapflow[QMRK]: start child mapflow (!)");
            addrem_flags(RKCL, QMRK);
            _handle_annotations_before_blck_key_scalar();
            m_evt_handler->begin_map_key_flow();
            _set_indentation(m_evt_handler->m_parent->indref);
            addrem_flags(RKEY, RKCL);
            _line_progressed(1);
            // keep going in this function
        }
        else if(first == '!')
        {
            csubstr tag = _scan_tag();
            _c4dbgpf("mapflow[QMRK]: tag! {}", prs_(tag));
            _add_annotation(&m_pending_tags, tag);
        }
        else
        {
            _c4err("parse error"); // LCOV_EXCL_LINE
        }
    }

 mapflow_again:
    _c4dbgt("mapflow: go again", 0);
    if(_finished_line())
    {
        if C4_LIKELY(!_finished_file())
        {
            _line_ended();
            _scan_line();
            _c4dbgnextline();
        }
        else
        {
            _c4err("missing terminating }");
        }
    }
    goto mapflow_start;

 mapflow_finish:
    _c4dbgp("mapflow: finish");
}


//-----------------------------------------------------------------------------

template<class EventHandler>
void ParseEngine<EventHandler>::_handle_seq_block()
{
seqblck_start:
    _c4dbgpf("handle_seq_block: seq_id={} node_id={} level={} indent={}", m_evt_handler->m_parent->node_id, m_evt_handler->m_curr->node_id, m_evt_handler->m_curr->level, m_evt_handler->m_curr->indref);

    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_all(RSEQ), m_evt_handler->m_curr->pos);
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_all(RBLCK), m_evt_handler->m_curr->pos);
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_any(RVAL|RNXT), m_evt_handler->m_curr->pos);
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, 1 == (has_any(RVAL) + has_any(RNXT)), m_evt_handler->m_curr->pos);

    _maybe_skip_comment_strict();
    if(!m_evt_handler->m_curr->line_contents.rem.len)
        goto seqblck_again;

    if(has_any(RVAL))
    {
        _c4dbgpf("seqblck[RVAL]: col={}", m_evt_handler->m_curr->pos.col);
        RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_none(RNXT), m_evt_handler->m_curr->pos);
        if(m_evt_handler->m_curr->at_line_beginning())
        {
            _c4dbgpf("seqblck[RVAL]: indref={} indentation={}", m_evt_handler->m_curr->indref+1, m_evt_handler->m_curr->line_contents.indentation);
            if(m_evt_handler->m_curr->indentation_ge_extra())
            {
                _c4dbgpf("seqblck[RVAL]: skip {} from indentation", m_evt_handler->m_curr->line_contents.indentation);
                _line_progressed(m_evt_handler->m_curr->line_contents.indentation);
                if(!m_evt_handler->m_curr->line_contents.rem.len)
                    goto seqblck_again;
            }
            else if(m_evt_handler->m_curr->indentation_lt_extra())
            {
                _c4dbgp("seqblck[RVAL]: smaller indentation than RVAL!");
                if(m_evt_handler->m_curr->indentation_eq())
                {
                    _c4dbgp("seqblck[RVAL]: smaller indentation than RVAL!");
                    _handle_annotations_before_blck_val_scalar();
                    m_evt_handler->set_val_scalar_plain_empty();
                    addrem_flags(RNXT, RVAL);
                    goto seqblck_again;
                }
                else
                {
                    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, m_evt_handler->m_curr->indentation_lt(), m_evt_handler->m_curr->pos);
                    _c4dbgp("seqblck[RVAL]: smaller indentation!");
                    _handle_indentation_pop_from_block_seq();
                    goto seqblck_finish;
                }
            }
            else if(m_evt_handler->m_curr->line_contents.indentation == npos)
            {
                _c4dbgp("seqblck[RVAL]: empty line!");
                _line_progressed(m_evt_handler->m_curr->line_contents.rem.len);
                goto seqblck_again;
            }
        }
        RYML_ASSERT_PARSE_CB_(callbacks(), m_evt_handler->m_curr->line_contents.rem.len, m_evt_handler->m_curr->pos);
        const size_t startmark = _handle_block_skip_leading_whitespace();
        _c4dbgpf("seqblck[RVAL]: startmark={}", startmark);
        if(startmark == npos)
        {
            _c4dbgp("seqblck[RVAL]: whitespace only");
            goto seqblck_again;
        }
        const size_t tabmark = _handle_block_get_whitespace_mark();
        const char first = m_evt_handler->m_curr->line_contents.rem.str[0];
        _c4dbgpf("seqblck[RVAL]: first='{}' currcol={}", first, m_evt_handler->m_curr->pos.col - 1);
        const size_t startline = m_evt_handler->m_curr->pos.line;
        _c4assert(m_evt_handler->m_curr->line_contents.current_col() >= m_bom_len);
        const size_t startindent = m_evt_handler->m_curr->line_contents.current_col() - m_bom_len;
        ScannedScalar sc;
        if(first == '\'')
        {
            _c4dbgp("seqblck[RVAL]: single-quoted scalar");
            sc = _scan_scalar_squot();
            if(!_maybe_scan_following_colon())
            {
                _c4dbgp("seqblck[RVAL]: set as val");
                _handle_annotations_before_blck_val_scalar();
                csubstr maybe_filtered = _maybe_filter_val_scalar_squot(sc); // VAL!
                m_evt_handler->set_val_scalar_squoted(maybe_filtered);
                addrem_flags(RNXT, RVAL);
            }
            else
            {
                _c4dbgp("seqblck[RVAL]: start mapblck, set scalar as key");
                _handle_block_check_leading_tabs(startmark);
                addrem_flags(RNXT, RVAL);
                _handle_annotations_before_start_mapblck(startline);
                _handle_colon();
                m_evt_handler->begin_map_val_block();
                _handle_annotations_and_indentation_after_start_mapblck(startindent, startline);
                csubstr maybe_filtered = _maybe_filter_key_scalar_squot(sc); // KEY!
                m_evt_handler->set_key_scalar_squoted(maybe_filtered);
                addrem_flags(RMAP|RVAL, RSEQ|RNXT);
                _maybe_skip_whitespace_tokens();
                goto seqblck_finish;
            }
        }
        else if(first == '"')
        {
            _c4dbgp("seqblck[RVAL]: double-quoted scalar");
            sc = _scan_scalar_dquot();
            if(!_maybe_scan_following_colon())
            {
                _c4dbgp("seqblck[RVAL]: set as val");
                _handle_annotations_before_blck_val_scalar();
                csubstr maybe_filtered = _maybe_filter_val_scalar_dquot(sc); // VAL!
                m_evt_handler->set_val_scalar_dquoted(maybe_filtered);
                addrem_flags(RNXT, RVAL);
            }
            else
            {
                _c4dbgp("seqblck[RVAL]: start mapblck, set scalar as key");
                addrem_flags(RNXT, RVAL);
                _handle_block_check_leading_tabs(startmark);
                _handle_annotations_before_start_mapblck(startline);
                _handle_colon();
                m_evt_handler->begin_map_val_block();
                _handle_annotations_and_indentation_after_start_mapblck(startindent, startline);
                csubstr maybe_filtered = _maybe_filter_key_scalar_dquot(sc); // KEY!
                m_evt_handler->set_key_scalar_dquoted(maybe_filtered);
                addrem_flags(RMAP|RVAL, RSEQ|RNXT);
                _maybe_skip_whitespace_tokens();
                goto seqblck_finish;
            }
        }
        // block scalars can only appear as keys when in QMRK scope
        // (ie, after ? tokens), so no need to scan following colon in
        // here.
        else if(first == '|')
        {
            _c4dbgp("seqblck[RVAL]: block-literal scalar");
            ScannedBlock sb;
            _scan_block(&sb, m_evt_handler->m_curr->indref + 1);
            _handle_annotations_before_blck_val_scalar();
            csubstr maybe_filtered = _maybe_filter_val_scalar_literal(sb);
            m_evt_handler->set_val_scalar_literal(maybe_filtered);
            addrem_flags(RNXT, RVAL);
        }
        else if(first == '>')
        {
            _c4dbgp("seqblck[RVAL]: block-folded scalar");
            ScannedBlock sb;
            _scan_block(&sb, m_evt_handler->m_curr->indref + 1);
            _handle_annotations_before_blck_val_scalar();
            csubstr maybe_filtered = _maybe_filter_val_scalar_folded(sb);
            m_evt_handler->set_val_scalar_folded(maybe_filtered);
            addrem_flags(RNXT, RVAL);
        }
        else if(_scan_scalar_plain_seq_blck(&sc))
        {
            _c4dbgp("seqblck[RVAL]: plain scalar.");
            if(!_maybe_scan_following_colon())
            {
                _c4dbgp("seqblck[RVAL]: set as val");
                _handle_annotations_before_blck_val_scalar();
                csubstr maybe_filtered = _maybe_filter_val_scalar_plain(sc, m_evt_handler->m_curr->indref);  // VAL!
                m_evt_handler->set_val_scalar_plain(maybe_filtered);
                addrem_flags(RNXT, RVAL);
            }
            else
            {
                RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, m_evt_handler->m_curr->indref != npos, m_evt_handler->m_curr->pos);
                RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, startindent > m_evt_handler->m_curr->indref, m_evt_handler->m_curr->pos);
                _c4dbgp("seqblck[RVAL]: start mapblck, set scalar as key");
                _handle_block_check_leading_tabs(startmark, tabmark);
                addrem_flags(RNXT, RVAL);
                _handle_annotations_before_start_mapblck(startline);
                _handle_colon();
                m_evt_handler->begin_map_val_block();
                _handle_annotations_and_indentation_after_start_mapblck(startindent, startline);
                csubstr maybe_filtered = _maybe_filter_key_scalar_plain(sc, m_evt_handler->m_curr->indref);  // KEY!
                m_evt_handler->set_key_scalar_plain(maybe_filtered);
                addrem_flags(RMAP|RVAL, RSEQ|RNXT);
                _maybe_skip_whitespace_tokens();
                goto seqblck_finish;
            }
        }
        else if(first == '[')
        {
            _c4dbgp("seqblck[RVAL]: start child seqflow");
            addrem_flags(RNXT, RVAL);
            _handle_annotations_before_blck_val_scalar();
            m_evt_handler->begin_seq_val_flow();
            addrem_flags(RFLOW|RVAL, RBLCK|RNXT);
            _line_progressed(1);
            _set_indentation(m_evt_handler->m_parent->indref + 1u);
            goto seqblck_finish;
        }
        else if(first == '{')
        {
            _c4dbgp("seqblck[RVAL]: start child mapflow");
            addrem_flags(RNXT, RVAL);
            _handle_annotations_before_blck_val_scalar();
            m_evt_handler->begin_map_val_flow();
            addrem_flags(RMAP|RKEY|RFLOW, RBLCK|RSEQ|RVAL|RNXT);
            _line_progressed(1);
            _set_indentation(m_evt_handler->m_parent->indref + 1u);
            goto seqblck_finish;
        }
        else if(first == '-')
        {
            _c4dbgp("seqblck[RVAL]: dash");
            _handle_block_check_leading_tabs(startmark);
            RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, m_evt_handler->m_curr->indref != npos, m_evt_handler->m_curr->pos);
            RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, startindent > m_evt_handler->m_curr->indref, m_evt_handler->m_curr->pos);
            _c4dbgp("seqblck[RVAL]: start child seqblck");
            RYML_ASSERT_PARSE_CB_(this->callbacks(), startindent > m_evt_handler->m_curr->indref, m_evt_handler->m_curr->pos);
            addrem_flags(RNXT, RVAL);
            _handle_annotations_before_blck_val_scalar();
            m_evt_handler->begin_seq_val_block();
            addrem_flags(RVAL, RNXT);
            _set_indentation(startindent);
            // keep going on inside this function
            _line_progressed(1);
        }
        else if(first == ':')
        {
            _c4dbgp("seqblck[RVAL]: start child mapblck with empty key");
            addrem_flags(RNXT, RVAL);
            _handle_annotations_before_start_mapblck(startline);
            _handle_colon();
            m_evt_handler->begin_map_val_block();
            _handle_annotations_and_indentation_after_start_mapblck(startindent, startline);
            m_evt_handler->set_key_scalar_plain_empty();
            addrem_flags(RMAP|RVAL, RSEQ|RNXT);
            _line_progressed(1);
            _maybe_skip_whitespace_tokens();
            goto seqblck_finish;
        }
        else if(first == '&')
        {
            const csubstr anchor = _scan_anchor();
            _c4dbgpf("seqblck[RVAL]: anchor! {}", prs_(anchor));
            // we need to buffer the anchors, as there may be two
            // consecutive anchors in here
            _add_annotation(&m_pending_anchors, anchor, startindent, startline);
        }
        else if(first == '*')
        {
            csubstr ref = _scan_ref_seq();
            _c4dbgpf("seqblck[RVAL]: ref! {}", prs_(ref));
            if(!_maybe_scan_following_colon())
            {
                _c4dbgp("seqblck[RVAL]: set ref as val!");
                _handle_valref(ref);
                addrem_flags(RNXT, RVAL);
            }
            else
            {
                _c4dbgp("seqblck[RVAL]: ref is key of map");
                addrem_flags(RNXT, RVAL);
                _handle_annotations_before_start_mapblck(startline);
                m_evt_handler->begin_map_val_block();
                _handle_annotations_and_indentation_after_start_mapblck(startindent, startline);
                _handle_keyref(ref);
                addrem_flags(RMAP|RVAL, RSEQ|RNXT);
                _set_indentation(startindent);
                _maybe_skip_whitespace_tokens();
                goto seqblck_finish;
            }
        }
        else if(first == '!')
        {
            csubstr tag = _scan_tag();
            _c4dbgpf("seqblck[RVAL]: val tag! {}", prs_(tag));
            // we need to buffer the tags, as there may be two
            // consecutive tags in here
            _add_annotation(&m_pending_tags, tag, startindent, startline);
        }
        else if(first == '?')
        {
            _c4dbgp("seqblck[RVAL]: start child mapblck, explicit key");
            addrem_flags(RNXT, RVAL);
            m_evt_handler->begin_map_val_block();
            addrem_flags(RMAP|QMRK, RSEQ|RNXT);
            _set_indentation(startindent);
            _line_progressed(1);
            _maybe_skipchars(' ');
            if(_is_blck_seq_token_maybe(m_evt_handler->m_curr->line_contents.rem))
            {
                _c4dbgp("seqblck[RVAL]: seqblck starts after ?");
                addrem_flags(RKCL, QMRK);
                m_evt_handler->begin_seq_key_block();
                addrem_flags(RSEQ|RVAL, RMAP|RKCL);
                _save_indentation();
                _line_progressed(1);
                _maybe_skipchars(' ');
            }
            goto seqblck_finish;
        }
        else
        {
            _c4err("parse error");
        }
    }
    else // RNXT
    {
        RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_any(RNXT), m_evt_handler->m_curr->pos);
        RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_none(RVAL), m_evt_handler->m_curr->pos);
        //
        // handle indentation
        //
        _c4dbgpf("seqblck[RNXT]: indref={} indentation={}", m_evt_handler->m_curr->indref, m_evt_handler->m_curr->line_contents.indentation);
        if C4_LIKELY(m_evt_handler->m_curr->at_line_beginning())
        {
            _c4dbgp("seqblck[RNXT]: at line begin");
            if(m_evt_handler->m_curr->indentation_ge())
            {
                _c4dbgpf("seqblck[RNXT]: skip {} from indref", m_evt_handler->m_curr->indref);
                _line_progressed(m_evt_handler->m_curr->indref);
                if(!m_evt_handler->m_curr->line_contents.rem.len)
                    goto seqblck_again;
            }
            else if(m_evt_handler->m_curr->indentation_lt())
            {
                _c4dbgp("seqblck[RNXT]: smaller indentation!");
                _handle_indentation_pop_from_block_seq();
                if(has_all(RSEQ|RBLCK))
                {
                    _c4dbgp("seqblck[RNXT]: still seqblck!");
                    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_any(RNXT), m_evt_handler->m_curr->pos);
                    _line_progressed(m_evt_handler->m_curr->line_contents.indentation);
                    if(!m_evt_handler->m_curr->line_contents.rem.len)
                        goto seqblck_again; // LCOV_EXCL_LINE
                }
                else
                {
                    _c4dbgp("seqblck[RNXT]: no longer seqblck!");
                    goto seqblck_finish;
                }
            }
            else if(m_evt_handler->m_curr->line_contents.indentation == npos)
            {
                _c4dbgpf("seqblck[RNXT]: blank line, len={}", m_evt_handler->m_curr->line_contents.rem);
                _line_progressed(m_evt_handler->m_curr->line_contents.rem.len);
                if(!m_evt_handler->m_curr->line_contents.rem.len)
                    goto seqblck_again; // LCOV_EXCL_LINE
            }
        }
        else
        {
            _c4dbgp("seqblck[RNXT]: NOT at line begin");
            if(!m_evt_handler->m_curr->line_contents.rem.begins_with_any(" \t"))
            {
                _c4err("parse error");
            }
            else
            {
                _skipchars(" \t");
                if(!m_evt_handler->m_curr->line_contents.rem.len)
                {
                    _c4dbgp("seqblck[RNXT]: again");
                    goto seqblck_again; // LCOV_EXCL_LINE
                }
            }
        }
        //
        // now handle the tokens
        //
        _c4assert(m_evt_handler->m_curr->line_contents.rem.len > 0);
        const char first = m_evt_handler->m_curr->line_contents.rem.str[0];
        _c4dbgpf("seqblck[RNXT]: '{}' node_id={}", _c4prc(first), m_evt_handler->m_curr->node_id);
        if(first == '-')
        {
            if(m_evt_handler->m_curr->indref > 0
               || m_evt_handler->m_curr->line_contents.indentation > 0
               || !_is_doc_begin_token(m_evt_handler->m_curr->line_contents.rem))
            {
                if C4_LIKELY(_is_blck_seq_token_maybe(m_evt_handler->m_curr->line_contents.rem))
                {
                    _c4dbgp("seqblck[RNXT]: expect next val");
                    addrem_flags(RVAL, RNXT);
                    m_evt_handler->add_sibling();
                    _line_progressed(1);
                }
                else
                {
                    _c4err("parse error");
                }
            }
            else
            {
                _c4dbgp("seqblck[RNXT]: start doc");
                _start_doc_suddenly();
                _line_progressed(3);
                _maybe_skip_whitespace_tokens();
                goto seqblck_finish;
            }
        }
        else if(first == ':')
        {
            // This happens for example in `- [a: b]: c` (after
            // terminating the seq, ie, after `]`). All other cases
            // (ie colon after scalars) are caught elsewhere (ie, in
            // RVAL state).
            if C4_LIKELY(m_evt_handler->m_parent && (m_evt_handler->m_parent->flags & RMAP))
            {
                _c4dbgp("seqblck[RNXT]: actually this seq was '?' key of parent map");
                m_evt_handler->end_seq_block();
                goto seqblck_finish;
            }
            else
            {
                _c4err("parse error");
            }
        }
        else if(first == '.')
        {
            _c4dbgp("seqblck[RNXT]: maybe doc?");
            if(_is_doc_end_token(m_evt_handler->m_curr->line_contents.rem))
            {
                _c4dbgp("seqblck[RNXT]: end doc");
                _end_doc_suddenly();
                _line_progressed(3);
                _maybe_skip_whitespace_tokens();
                _check_doc_end_tokens();
                goto seqblck_finish;
            }
            else
            {
                _c4err("parse error");
            }
        }
        else
        {
            // may be an indentless sequence nested in a map...
            #ifdef RYML_DBG
            _print_state_stack();
            #endif
            if(m_evt_handler->m_parent
               && has_all(RMAP|RBLCK, m_evt_handler->m_parent)
               && m_evt_handler->m_curr->indref == m_evt_handler->m_parent->indref)
            {
                _c4dbgpf("seqblck[RNXT]: end indentless seq, go to parent={}. node={}", m_evt_handler->m_parent->node_id, m_evt_handler->m_curr->node_id);
                RYML_ASSERT_PARSE_CB_(this->callbacks(), m_evt_handler->m_curr != m_evt_handler->m_parent, m_evt_handler->m_curr->pos);
                _handle_indentation_pop(m_evt_handler->m_parent);
                RYML_ASSERT_PARSE_CB_(this->callbacks(), has_all(RMAP|RBLCK), m_evt_handler->m_curr->pos);
                m_evt_handler->add_sibling();
                addrem_flags(RKEY, RNXT);
                goto seqblck_finish;
            }
            else if(first == '\t')
            {
                size_t pos = m_evt_handler->m_curr->line_contents.rem.first_not_of('\t');
                if(pos == npos)
                {
                    _line_progressed(m_evt_handler->m_curr->line_contents.rem.len);
                    goto seqblck_again;
                }
            }
            _c4err("parse error");
        }
    }

 seqblck_again:
    _c4dbgt("seqblck: go again", 0);
    if(_finished_line())
    {
        m_bom_len = 0;
        _line_ended();
        _scan_line();
        if(_finished_file())
        {
            _c4dbgp("seqblck: finish!");
            _end_seq_blck();
            goto seqblck_finish;
        }
        _c4dbgnextline();
    }
    goto seqblck_start;

 seqblck_finish:
    _c4dbgp("seqblck: finish");
}


//-----------------------------------------------------------------------------

template<class EventHandler>
void ParseEngine<EventHandler>::_handle_map_block()
{
mapblck_start:
    _c4dbgpf("handle_map_block: map_id={} node_id={} level={} indref={}", m_evt_handler->m_parent->node_id, m_evt_handler->m_curr->node_id, m_evt_handler->m_curr->level, m_evt_handler->m_curr->indref);

    // states: RKEY -> RVAL -> RNXT
    // states: QMRK -> RKCL -> RVAL -> RNXT
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_all(RMAP), m_evt_handler->m_curr->pos);
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_all(RBLCK), m_evt_handler->m_curr->pos);
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_any(RKEY|RKCL|RVAL|RNXT|QMRK), m_evt_handler->m_curr->pos);
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, 1 == (has_any(RKEY) + has_any(RKCL) + has_any(RVAL) + has_any(RNXT) + has_any(QMRK)), m_evt_handler->m_curr->pos);

    _maybe_skip_comment();
    if(!m_evt_handler->m_curr->line_contents.rem.len)
        goto mapblck_again;

    if(has_any(RKEY))
    {
        RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_none(RKCL), m_evt_handler->m_curr->pos);
        RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_none(QMRK), m_evt_handler->m_curr->pos);
        RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_none(RVAL), m_evt_handler->m_curr->pos);
        RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_none(RNXT), m_evt_handler->m_curr->pos);
        //
        // handle indentation
        //
        if(m_evt_handler->m_curr->at_line_beginning())
        {
            if(m_evt_handler->m_curr->indentation_eq())
            {
                _c4dbgpf("mapblck[RKEY]: skip {} from indref", m_evt_handler->m_curr->indref);
                _line_progressed(m_evt_handler->m_curr->indref);
                if(!m_evt_handler->m_curr->line_contents.rem.len)
                    goto mapblck_again;
            }
            else if(m_evt_handler->m_curr->indentation_lt())
            {
                _c4dbgp("mapblck[RKEY]: smaller indentation!");
                _handle_indentation_pop_from_block_map();
                _line_progressed(m_evt_handler->m_curr->line_contents.indentation);
                if(has_all(RMAP|RBLCK))
                {
                    _c4dbgp("mapblck[RKEY]: still mapblck!");
                    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_any(RKEY), m_evt_handler->m_curr->pos);
                    if(!m_evt_handler->m_curr->line_contents.rem.len)
                        goto mapblck_again;
                }
                else
                {
                    _c4dbgp("mapblck[RKEY]: no longer mapblck!");
                    goto mapblck_finish;
                }
            }
            else
            {
                RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, m_evt_handler->m_curr->indentation_gt(), m_evt_handler->m_curr->pos);
                _c4err("invalid indentation");
            }
        }
        //
        // now handle the tokens
        //
        const char first = m_evt_handler->m_curr->line_contents.rem.str[0];
        const size_t startline = m_evt_handler->m_curr->pos.line;
        const size_t startindent = m_evt_handler->m_curr->line_contents.current_col();
        _c4dbgpf("mapblck[RKEY]: '{}'", _c4prc(first));
        ScannedScalar sc;
        if(first == '\'')
        {
            _c4dbgp("mapblck[RKEY]: scanning single-quoted scalar");
            sc = _scan_scalar_squot();
            csubstr maybe_filtered = _maybe_filter_val_scalar_squot(sc);
            _handle_annotations_before_blck_key_scalar();
            m_evt_handler->set_key_scalar_squoted(maybe_filtered);
            addrem_flags(RVAL, RKEY);
            if(!_maybe_scan_following_colon())
                _c4err("could not find ':' colon after key");
            _handle_colon();
            _maybe_skip_whitespace_tokens();
        }
        else if(first == '"')
        {
            _c4dbgp("mapblck[RKEY]: scanning double-quoted scalar");
            sc = _scan_scalar_dquot();
            csubstr maybe_filtered = _maybe_filter_val_scalar_dquot(sc);
            _handle_annotations_before_blck_key_scalar();
            m_evt_handler->set_key_scalar_dquoted(maybe_filtered);
            addrem_flags(RVAL, RKEY);
            if(!_maybe_scan_following_colon())
                _c4err("could not find ':' colon after key");
            _handle_colon();
            _maybe_skip_whitespace_tokens();
        }
        // block scalars (| and >) can not be used as keys unless they
        // appear in an explicit QMRK scope (ie, after the ? token),
        else if C4_UNLIKELY(first == '|')
        {
            _c4err("block map: literal keys must be enclosed in '?'");
        }
        else if C4_UNLIKELY(first == '>')
        {
            _c4err("block map: folded keys must be enclosed in '?'");
        }
        else if(_scan_scalar_plain_map_blck(&sc))
        {
            _c4dbgp("mapblck[RKEY]: plain scalar");
            csubstr maybe_filtered = _maybe_filter_val_scalar_plain(sc, m_evt_handler->m_curr->indref);
            _handle_annotations_before_blck_key_scalar();
            m_evt_handler->set_key_scalar_plain(maybe_filtered);
            addrem_flags(RVAL, RKEY);
            if(!_maybe_scan_following_colon())
                _c4err("could not find ':' colon after key");
            _handle_colon();
            _maybe_skip_whitespace_tokens();
        }
        else if(first == '?')
        {
            _c4dbgp("mapblck[RKEY]: key token!");
            addrem_flags(QMRK, RKEY);
            _line_progressed(1);
            _maybe_skipchars(' ');
            if(_is_blck_seq_token_maybe(m_evt_handler->m_curr->line_contents.rem))
            {
                _c4dbgp("mapblck[RKEY]: seqblck starts after ?");
                addrem_flags(RKCL, QMRK);
                m_evt_handler->begin_seq_key_block();
                addrem_flags(RSEQ|RVAL, RMAP|RKCL);
                _save_indentation();
                _line_progressed(1);
                _maybe_skipchars(' ');
                goto mapblck_finish;
            }
            goto mapblck_again;
        }
        else if(first == ':')
        {
            _c4dbgp("mapblck[RKEY]: setting empty key");
            _handle_annotations_before_blck_key_scalar();
            m_evt_handler->set_key_scalar_plain_empty();
            addrem_flags(RVAL, RKEY);
            _line_progressed(1);
            _handle_colon();
            _maybe_skip_whitespace_tokens();
        }
        else if(first == '*')
        {
            csubstr ref = _scan_ref_map();
            _c4dbgpf("mapblck[RKEY]: key ref! {}", prs_(ref));
            _handle_keyref(ref);
            addrem_flags(RVAL, RKEY);
            if(!_maybe_scan_following_colon())
                _c4err("could not find ':' colon after key");
            _handle_colon();
            _maybe_skip_whitespace_tokens();
        }
        else if(first == '&')
        {
            csubstr anchor = _scan_anchor();
            _c4dbgpf("mapblck[RKEY]: key anchor! {}", prs_(anchor));
            _add_annotation(&m_pending_anchors, anchor, startindent, startline);
        }
        else if(first == '!')
        {
            csubstr tag = _scan_tag();
            _c4dbgpf("mapblck[RKEY]: key tag! {}", prs_(tag));
            _add_annotation(&m_pending_tags, tag, startindent, startline);
        }
        else if(first == '[')
        {
            // RYML's tree cannot store container keys, but that's
            // handled inside the tree handler. Other handlers may be
            // able to handle it.
            _c4dbgp("mapblck[RKEY]: start child seqflow (!)");
            _handle_annotations_before_blck_key_scalar();
            m_evt_handler->begin_seq_key_flow();
            addrem_flags(RSEQ|RFLOW|RVAL, RKEY|RMAP|RBLCK);
            _line_progressed(1);
            _set_indentation(startindent);
            goto mapblck_finish;
        }
        else if(first == '{')
        {
            // RYML's tree cannot store container keys, but that's
            // handled inside the tree handler. Other handlers may be
            // able to handle it.
            _c4dbgp("mapblck[RKEY]: start child mapflow (!)");
            _handle_annotations_before_blck_key_scalar();
            m_evt_handler->begin_map_key_flow();
            addrem_flags(RFLOW|RKEY, RBLCK);
            _line_progressed(1);
            _set_indentation(startindent);
            goto mapblck_finish;
        }
        else if(first == '-')
        {
            _c4dbgp("mapblck[RKEY]: maybe doc?");
            if(m_evt_handler->m_curr->line_contents.indentation == 0 && _is_doc_begin_token(m_evt_handler->m_curr->line_contents.rem))
            {
                _c4dbgp("mapblck[RKEY]: end+start doc");
                _start_doc_suddenly();
                _line_progressed(3);
                _maybe_skip_whitespace_tokens();
                goto mapblck_finish;
            }
            else
            {
                _c4err("parse error");
            }
        }
        else if(first == '.')
        {
            _c4dbgp("mapblck[RKEY]: maybe end doc?");
            if(m_evt_handler->m_curr->line_contents.indentation == 0 && _is_doc_end_token(m_evt_handler->m_curr->line_contents.rem))
            {
                _c4dbgp("mapblck[RKEY]: end doc");
                _end_doc_suddenly();
                _line_progressed(3);
                _maybe_skip_whitespace_tokens();
                _check_doc_end_tokens();
                goto mapblck_finish;
            }
            else
            {
                _c4err("parse error"); // LCOV_EXCL_LINE
            }
        }
        else
        {
            _c4err("parse error");
        }
    }
    else if(has_any(RVAL))
    {
        RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_none(RKEY), m_evt_handler->m_curr->pos);
        RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_none(RKCL), m_evt_handler->m_curr->pos);
        RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_none(RNXT), m_evt_handler->m_curr->pos);
        RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_none(QMRK), m_evt_handler->m_curr->pos);
        //
        // handle indentation
        //
        if(m_evt_handler->m_curr->at_line_beginning())
        {
            _c4dbgpf("mapblck[RVAL]: indref={} indentation={}", m_evt_handler->m_curr->indref+1, m_evt_handler->m_curr->line_contents.indentation);
            m_evt_handler->m_curr->more_indented = false;
            RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, m_evt_handler->m_curr->indref != npos, m_evt_handler->m_curr->pos);
            if(m_evt_handler->m_curr->indentation_eq_extra())
            {
                _c4dbgp("mapblck[RVAL]: skip indentation!");
                _line_progressed(m_evt_handler->m_curr->indref + 1);
                if(!m_evt_handler->m_curr->line_contents.rem.len)
                    goto mapblck_again;
            }
            else if(m_evt_handler->m_curr->indentation_gt_extra())
            {
                _c4dbgp("mapblck[RVAL]: more indented!");
                m_evt_handler->m_curr->more_indented = true;
                _line_progressed(m_evt_handler->m_curr->line_contents.indentation);
                if(!m_evt_handler->m_curr->line_contents.rem.len)
                    goto mapblck_again; // LCOV_EXCL_LINE
            }
            else if(m_evt_handler->m_curr->indentation_lt_extra())
            {
                if(m_evt_handler->m_curr->indentation_eq())
                {
                    _c4dbgp("mapblck[RVAL]: smaller indentation than RVAL!");
                    // watchout for indentless seqs
                    if(!_is_blck_seq_token_maybe(m_evt_handler->m_curr->line_contents.rem.sub(m_evt_handler->m_curr->line_contents.indentation)))
                    {
                        _c4dbgp("mapblck[RVAL]: smaller indentation than RVAL!");
                        _handle_annotations_before_blck_val_scalar();
                        m_evt_handler->set_val_scalar_plain_empty();
                        addrem_flags(RNXT, RVAL);
                        goto mapblck_again;
                    }
                }
                else
                {
                    _c4dbgp("mapblck[RVAL]: smaller indentation than RKEY!");
                    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, m_evt_handler->m_curr->indentation_lt(), m_evt_handler->m_curr->pos);
                    _handle_indentation_pop_from_block_map();
                    if(has_all(RMAP|RBLCK))
                    {
                        _c4dbgp("mapblck[RVAL]: still mapblck!");
                        _line_progressed(m_evt_handler->m_curr->line_contents.indentation);
                        if(has_any(RNXT))
                        {
                            _c4dbgp("mapblck[RVAL]: speculatively expect next keyval");
                            m_evt_handler->add_sibling();
                            addrem_flags(RKEY, RNXT);
                        }
                        goto mapblck_again;
                    }
                    else
                    {
                        _c4dbgp("mapblck[RVAL]: no longer mapblck!");
                        goto mapblck_finish;
                    }
                }
            }
        }
        const size_t startcol = _handle_block_skip_leading_whitespace();
        if(startcol == npos)
        {
            _c4dbgp("mapblck[RVAL]: whitespace only");
            goto mapblck_again; // LCOV_EXCL_LINE
        }
        const size_t tabmark = _handle_block_get_whitespace_mark();
        //
        // now handle the tokens
        //
        _c4assert(m_evt_handler->m_curr->line_contents.rem.len);
        const char first = m_evt_handler->m_curr->line_contents.rem.str[0];
        const size_t startline = m_evt_handler->m_curr->pos.line;
        const size_t startindent = m_evt_handler->m_curr->line_contents.current_col();
        _c4dbgpf("mapblck[RVAL]: '{}'", _c4prc(first));
        ScannedScalar sc;
        if(first == '\'')
        {
            _c4dbgp("mapblck[RVAL]: scanning single-quoted scalar");
            sc = _scan_scalar_squot();
            if(!_maybe_scan_following_colon())
            {
                _c4dbgp("mapblck[RVAL]: set as val");
                _handle_annotations_before_blck_val_scalar();
                csubstr maybe_filtered = _maybe_filter_val_scalar_squot(sc); // VAL!
                m_evt_handler->set_val_scalar_squoted(maybe_filtered);
                addrem_flags(RNXT, RVAL);
            }
            else
            {
                _c4assert(m_evt_handler->m_curr->indref != npos);
                _c4assert(startindent > m_evt_handler->m_curr->indref);
                _c4dbgp("mapblck[RVAL]: start new block map, set scalar as key");
                _handle_block_check_leading_tabs(startcol);
                _handle_annotations_before_start_mapblck(startline);
                addrem_flags(RNXT, RVAL);
                _handle_colon();
                m_evt_handler->begin_map_val_block();
                _handle_annotations_and_indentation_after_start_mapblck(startindent, startline);
                csubstr maybe_filtered = _maybe_filter_key_scalar_squot(sc); // KEY!
                m_evt_handler->set_key_scalar_squoted(maybe_filtered);
                _maybe_skip_whitespace_tokens();
                // keep the child state on RVAL
                addrem_flags(RVAL, RNXT);
            }
        }
        else if(first == '"')
        {
            _c4dbgp("mapblck[RVAL]: scanning double-quoted scalar");
            sc = _scan_scalar_dquot();
            if(!_maybe_scan_following_colon())
            {
                _c4dbgp("mapblck[RVAL]: set as val");
                _handle_annotations_before_blck_val_scalar();
                csubstr maybe_filtered = _maybe_filter_val_scalar_dquot(sc); // VAL!
                m_evt_handler->set_val_scalar_dquoted(maybe_filtered);
                addrem_flags(RNXT, RVAL);
            }
            else
            {
                _c4assert(m_evt_handler->m_curr->indref != npos);
                _c4assert(startindent > m_evt_handler->m_curr->indref);
                _c4dbgp("mapblck[RVAL]: start new block map, set scalar as key");
                _handle_block_check_leading_tabs(startcol);
                _handle_annotations_before_start_mapblck(startline);
                addrem_flags(RNXT, RVAL);
                _handle_colon();
                m_evt_handler->begin_map_val_block();
                _handle_annotations_and_indentation_after_start_mapblck(startindent, startline);
                csubstr maybe_filtered = _maybe_filter_key_scalar_dquot(sc); // KEY!
                m_evt_handler->set_key_scalar_dquoted(maybe_filtered);
                _maybe_skip_whitespace_tokens();
                // keep the child state on RVAL
                addrem_flags(RVAL, RNXT);
            }
        }
        // block scalars can only appear as keys when in QMRK scope
        // (ie, after ? tokens), so no need to scan following colon
        else if(first == '|')
        {
            _c4dbgp("mapblck[RVAL]: scanning block-literal scalar");
            ScannedBlock sb;
            _scan_block(&sb, m_evt_handler->m_curr->indref + 1);
            _handle_annotations_before_blck_val_scalar();
            csubstr maybe_filtered = _maybe_filter_val_scalar_literal(sb);
            m_evt_handler->set_val_scalar_literal(maybe_filtered);
            addrem_flags(RNXT, RVAL);
        }
        else if(first == '>')
        {
            _c4dbgp("mapblck[RVAL]: scanning block-folded scalar");
            ScannedBlock sb;
            _scan_block(&sb, m_evt_handler->m_curr->indref + 1);
            _handle_annotations_before_blck_val_scalar();
            csubstr maybe_filtered = _maybe_filter_val_scalar_folded(sb);
            m_evt_handler->set_val_scalar_folded(maybe_filtered);
            addrem_flags(RNXT, RVAL);
        }
        else if(_scan_scalar_plain_map_blck(&sc))
        {
            _c4dbgp("mapblck[RVAL]: plain scalar.");
            if(!_maybe_scan_following_colon())
            {
                _c4dbgp("mapblck[RVAL]: set as val");
                _handle_annotations_before_blck_val_scalar();
                csubstr maybe_filtered = _maybe_filter_val_scalar_plain(sc, m_evt_handler->m_curr->indref); // VAL!
                m_evt_handler->set_val_scalar_plain(maybe_filtered);
                addrem_flags(RNXT, RVAL);
            }
            else
            {
                _c4assert(m_evt_handler->m_curr->indref != npos);
                _c4assert(startindent > m_evt_handler->m_curr->indref);
                _c4dbgpf("mapblck[RVAL]: start new block map, set scalar as key {}", m_evt_handler->m_curr->indref);
                _handle_block_check_leading_tabs(startcol, tabmark);
                addrem_flags(RNXT, RVAL);
                _handle_annotations_before_start_mapblck(startline);
                _handle_colon();
                m_evt_handler->begin_map_val_block();
                _handle_annotations_and_indentation_after_start_mapblck(startindent, startline);
                csubstr maybe_filtered = _maybe_filter_key_scalar_plain(sc, m_evt_handler->m_curr->indref); // KEY!
                m_evt_handler->set_key_scalar_plain(maybe_filtered);
                _maybe_skip_whitespace_tokens();
                // keep the child state on RVAL
                addrem_flags(RVAL, RNXT);
            }
        }
        else if(first == '-' && _is_blck_seq_token_maybe(m_evt_handler->m_curr->line_contents.rem))
        {
            if C4_UNLIKELY(!m_evt_handler->m_curr->at_first_token())
                _c4err("parse error");
            _c4dbgp("mapblck[RVAL]: start val seqblck");
            _handle_block_check_leading_tabs(startcol);
            addrem_flags(RNXT, RVAL);
            _handle_annotations_before_blck_val_scalar();
            m_evt_handler->begin_seq_val_block();
            addrem_flags(RSEQ|RVAL, RMAP|RNXT);
            _set_indentation(startindent);
            _line_progressed(1);
            _maybe_skip_whitespace_tokens();
            goto mapblck_finish;
        }
        else if(first == '[')
        {
            _c4dbgp("mapblck[RVAL]: start val seqflow");
            addrem_flags(RNXT, RVAL);
            _handle_annotations_before_blck_val_scalar();
            m_evt_handler->begin_seq_val_flow();
            addrem_flags(RSEQ|RFLOW|RVAL, RMAP|RBLCK|RNXT);
            _set_indentation(m_evt_handler->m_parent->indref + 1u);
            _line_progressed(1);
            goto mapblck_finish;
        }
        else if(first == '{')
        {
            _c4dbgp("mapblck[RVAL]: start val mapflow");
            addrem_flags(RNXT, RVAL);
            _handle_annotations_before_blck_val_scalar();
            m_evt_handler->begin_map_val_flow();
            addrem_flags(RKEY|RFLOW, RBLCK|RVAL|RNXT);
            m_evt_handler->m_curr->scalar_col = m_evt_handler->m_curr->line_contents.indentation;
            _set_indentation(m_evt_handler->m_parent->indref + 1u);
            _line_progressed(1);
            goto mapblck_finish;
        }
        else if(first == '*')
        {
            csubstr ref = _scan_ref_map();
            _c4dbgpf("mapblck[RVAL]: ref! {}", prs_(ref));
            if(_maybe_scan_following_colon())
            {
                _c4dbgp("mapblck[RVAL]: start child map, block");
                addrem_flags(RNXT, RVAL);
                _handle_annotations_before_blck_val_scalar();
                m_evt_handler->begin_map_val_block();
                _handle_keyref(ref);
                _set_indentation(startindent);
                // keep going in RVAL
                addrem_flags(RVAL, RNXT);
            }
            else
            {
                _c4dbgp("mapblck[RVAL]: was val ref");
                _handle_valref(ref);
                addrem_flags(RNXT, RVAL);
            }
            _maybe_skip_whitespace_tokens();
        }
        else if(first == '&')
        {
            csubstr anchor = _scan_anchor();
            _c4dbgpf("mapblck[RVAL]: anchor! {}", prs_(anchor));
            // we need to buffer the anchors, as there may be two
            // consecutive anchors in here
            _add_annotation(&m_pending_anchors, anchor, startindent, startline);
        }
        else if(first == '!')
        {
            csubstr tag = _scan_tag();
            _c4dbgpf("mapblck[RVAL]: tag! {}", prs_(tag));
            // we need to buffer the tags, as there may be two
            // consecutive tags in here
            _add_annotation(&m_pending_tags, tag, startindent, startline);
        }
        else if(first == '?')
        {
            if C4_UNLIKELY(!m_evt_handler->m_curr->at_first_token())
                _c4err("parse error");
            _c4dbgp("mapblck[RVAL]: start val mapblck");
            addrem_flags(RNXT, RVAL);
            _handle_annotations_before_blck_val_scalar();
            m_evt_handler->begin_map_val_block();
            addrem_flags(QMRK, RNXT);
            _set_indentation(startindent);
            _line_progressed(1);
            _maybe_skipchars(' ');
            if(_is_blck_seq_token_maybe(m_evt_handler->m_curr->line_contents.rem))
            {
                _c4dbgp("mapblck[RVAL]: seqblck starts after ?");
                addrem_flags(RKCL, QMRK);
                m_evt_handler->begin_seq_key_block();
                addrem_flags(RSEQ|RVAL, RMAP|RKCL);
                _save_indentation();
                _line_progressed(1);
                _maybe_skipchars(' ');
                goto mapblck_finish;
            }
            goto mapblck_again;
        }
        else if(first == ':')
        {
            _c4dbgp("mapblck[RVAL]: start val mapblck");
            addrem_flags(RNXT, RVAL);
            _handle_annotations_before_start_mapblck(startline);
            _handle_colon();
            m_evt_handler->begin_map_val_block();
            _handle_annotations_and_indentation_after_start_mapblck(startindent, startline);
            m_evt_handler->set_key_scalar_plain_empty();
            // keep the child state on RVAL
            addrem_flags(RVAL, RNXT);
            _line_progressed(1);
            _maybe_skip_whitespace_tokens();
            goto mapblck_again;
        }
        else
        {
            _c4err("parse error"); // LCOV_EXCL_LINE
        }
    }
    else if(has_any(RNXT))
    {
        RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_none(RKEY), m_evt_handler->m_curr->pos);
        RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_none(RKCL), m_evt_handler->m_curr->pos);
        RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_none(RVAL), m_evt_handler->m_curr->pos);
        RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_none(QMRK), m_evt_handler->m_curr->pos);
        //
        // handle indentation
        //
        if(m_evt_handler->m_curr->at_line_beginning())
        {
            _c4dbgpf("mapblck[RNXT]: indref={} indentation={}", m_evt_handler->m_curr->indref, m_evt_handler->m_curr->line_contents.indentation);
            if(m_evt_handler->m_curr->indentation_eq())
            {
                _c4dbgpf("mapblck[RNXT]: skip {} from indref", m_evt_handler->m_curr->indref);
                _line_progressed(m_evt_handler->m_curr->indref);
                _c4dbgp("mapblck[RNXT]: speculatively expect next keyval");
                m_evt_handler->add_sibling();
                addrem_flags(RKEY, RNXT);
                goto mapblck_again;
            }
            else if(m_evt_handler->m_curr->indentation_lt())
            {
                _c4dbgp("mapblck[RNXT]: smaller indentation!");
                _handle_indentation_pop_from_block_map();
                if(has_all(RMAP|RBLCK))
                {
                    _line_progressed(m_evt_handler->m_curr->line_contents.indentation);
                    if(!has_any(RKCL))
                    {
                        _c4dbgp("mapblck[RNXT]: speculatively expect next keyval");
                        m_evt_handler->add_sibling();
                        addrem_flags(RKEY, RNXT);
                    }
                    goto mapblck_again;
                }
                else
                {
                    goto mapblck_finish;
                }
            }
        }
        else
        {
            _c4dbgp("mapblck[RNXT]: NOT at line begin");
            if(!m_evt_handler->m_curr->line_contents.rem.begins_with_any(" \t"))
            {
                _c4err("parse error");
            }
            else
            {
                _skipchars(" \t");
                if(!m_evt_handler->m_curr->line_contents.rem.len)
                {
                    _c4dbgp("seqblck[RNXT]: again");
                    goto mapblck_again; // LCOV_EXCL_LINE
                }
            }
        }
        //
        // handle tokens
        //
        RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, m_evt_handler->m_curr->line_contents.rem.len > 0, m_evt_handler->m_curr->pos);
        const char first = m_evt_handler->m_curr->line_contents.rem.str[0];
        _c4dbgpf("mapblck[RNXT]: '{}'", _c4prc(first));
        if(first == ' ')
        {
            _c4dbgp("mapblck[RNXT]: skip spaces");
            _maybe_skip_whitespace_tokens();
        }
        else
        {
            _c4err("parse error");
        }
    }
    else if(has_any(QMRK))
    {
        RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_none(RKEY), m_evt_handler->m_curr->pos);
        RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_none(RKCL), m_evt_handler->m_curr->pos);
        RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_none(RVAL), m_evt_handler->m_curr->pos);
        RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_none(RNXT), m_evt_handler->m_curr->pos);
        if(_handle_map_block_qmrk())
            goto mapblck_again;
        else
            goto mapblck_finish;
    }
    else if(has_any(RKCL)) // read the key colon (after QMRK)
    {
        RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_none(RKEY), m_evt_handler->m_curr->pos);
        RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_none(RVAL), m_evt_handler->m_curr->pos);
        RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_none(RNXT), m_evt_handler->m_curr->pos);
        RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_none(QMRK), m_evt_handler->m_curr->pos);
        if(_handle_map_block_rkcl())
            goto mapblck_again;
        else
            goto mapblck_finish;
    }

 mapblck_again:
    _c4dbgt("mapblck: again", 0);
    if(_finished_line())
    {
        _line_ended();
        _scan_line();
        if(_finished_file())
        {
            _c4dbgp("mapblck: file finished!");
            _end_map_blck();
            goto mapblck_finish;
        }
        _c4dbgnextline();
    }
    goto mapblck_start;

 mapblck_finish:
    _c4dbgp("mapblck: finish");
}


//-----------------------------------------------------------------------------

// return true if we should remain in map_block
template<class EventHandler>
bool ParseEngine<EventHandler>::_handle_map_block_qmrk()
{
    //
    // handle indentation
    //
    if(m_evt_handler->m_curr->at_line_beginning())
    {
        _c4dbgpf("mapblck[QMRK]: at line beginning. ind={} indref={}", m_evt_handler->m_curr->line_contents.indentation, m_evt_handler->m_curr->indref);
        RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, m_evt_handler->m_curr->line_contents.indentation != npos, m_evt_handler->m_curr->pos);
        if(m_evt_handler->m_curr->indentation_eq_extra())
        {
            _c4dbgpf("mapblck[QMRK]: skip {} from indref", m_evt_handler->m_curr->indref + 1);
            _line_progressed(m_evt_handler->m_curr->indref + 1);
            if(!m_evt_handler->m_curr->line_contents.rem.len)
                return true; // go again
        }
        // indentation can be larger in QMRK state
        else if(m_evt_handler->m_curr->indentation_gt_extra())
        {
            _c4dbgp("mapblck[QMRK]: larger indentation !");
            _line_progressed(m_evt_handler->m_curr->line_contents.indentation);
            if(!m_evt_handler->m_curr->line_contents.rem.len)
                return true; // go again
        }
        else
        {
            _c4dbgp("mapblck[QMRK]: smaller indentation!");
            RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, m_evt_handler->m_curr->indentation_lt_extra(), m_evt_handler->m_curr->pos);
            RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, m_evt_handler->m_curr->line_contents.rem.len > 0, m_evt_handler->m_curr->pos);
            if(m_evt_handler->m_curr->indentation_eq()
               // defend against docs or indentless seqs
               && m_evt_handler->m_curr->line_contents.rem.str[0] != '-')
            {
                _c4dbgp("mapblck[QMRK]: QMRK finished!");
                _handle_annotations_before_blck_key_scalar();
                m_evt_handler->set_key_scalar_plain_empty();
                addrem_flags(RKCL, QMRK);
                return true; // go again
            }
            else if(m_evt_handler->m_curr->indentation_lt())
            {
                _c4dbgp("mapblck[QMRK]: indentation pop!");
                _handle_indentation_pop_from_block_map();
                _line_progressed(m_evt_handler->m_curr->line_contents.indentation);
                if(has_all(RMAP|RBLCK))
                {
                    _c4dbgp("mapblck[QMRK]: still mapblck!");
                    return true; // go again
                }
                else
                {
                    _c4dbgp("mapblck[QMRK]: no longer mapblck!");
                    return false; // finish mapblck
                }
            }
        }
    }
    //
    // now handle the tokens
    //
    _c4assert(m_evt_handler->m_curr->line_contents.rem.len);
    const char first = m_evt_handler->m_curr->line_contents.rem.str[0];
    const size_t startline = m_evt_handler->m_curr->pos.line;
    const size_t startindent = m_evt_handler->m_curr->line_contents.current_col();
    _c4dbgpf("mapblck[QMRK]: '{}'", first);
    ScannedScalar sc;
    if(first == '\'')
    {
        _c4dbgp("mapblck[QMRK]: scanning single-quoted scalar");
        sc = _scan_scalar_squot();
        csubstr maybe_filtered = _maybe_filter_key_scalar_squot(sc); // KEY!
        addrem_flags(RKCL, QMRK);
        if(!_maybe_scan_following_colon())
        {
            _c4dbgp("mapblck[QMRK]: set as key");
            _handle_annotations_before_blck_key_scalar();
            m_evt_handler->set_key_scalar_squoted(maybe_filtered);
        }
        else
        {
            _c4dbgp("mapblck[QMRK]: start new block map as key (!), set scalar as key");
            _handle_annotations_before_start_mapblck_as_key();
            m_evt_handler->begin_map_key_block();
            _handle_annotations_and_indentation_after_start_mapblck(startindent, startline);
            m_evt_handler->set_key_scalar_squoted(maybe_filtered);
            _maybe_skip_whitespace_tokens();
            _set_indentation(startindent);
            // keep the child state on RVAL
            addrem_flags(RVAL, RKCL);
        }
    }
    else if(first == '"')
    {
        _c4dbgp("mapblck[QMRK]: scanning double-quoted scalar");
        sc = _scan_scalar_dquot();
        csubstr maybe_filtered = _maybe_filter_key_scalar_dquot(sc); // KEY!
        addrem_flags(RKCL, QMRK);
        if(!_maybe_scan_following_colon())
        {
            _c4dbgp("mapblck[QMRK]: set as key");
            _handle_annotations_before_blck_key_scalar();
            m_evt_handler->set_key_scalar_dquoted(maybe_filtered);
        }
        else
        {
            _c4dbgp("mapblck[QMRK]: start new block map as key (!), set scalar as key");
            _handle_annotations_before_start_mapblck_as_key();
            m_evt_handler->begin_map_key_block();
            _handle_annotations_and_indentation_after_start_mapblck(startindent, startline);
            m_evt_handler->set_key_scalar_dquoted(maybe_filtered);
            _maybe_skip_whitespace_tokens();
            _set_indentation(startindent);
            // keep the child state on RVAL
            addrem_flags(RVAL, RKCL);
        }
    }
    else if(first == '|')
    {
        _c4dbgp("mapblck[QMRK]: scanning block-literal scalar");
        ScannedBlock sb;
        _scan_block(&sb, m_evt_handler->m_curr->indref + 1);
        csubstr maybe_filtered = _maybe_filter_key_scalar_literal(sb); // KEY!
        _handle_annotations_before_blck_key_scalar();
        m_evt_handler->set_key_scalar_literal(maybe_filtered);
        addrem_flags(RKCL, QMRK);
    }
    else if(first == '>')
    {
        _c4dbgp("mapblck[QMRK]: scanning block-literal scalar");
        ScannedBlock sb;
        _scan_block(&sb, m_evt_handler->m_curr->indref + 1);
        csubstr maybe_filtered = _maybe_filter_key_scalar_folded(sb); // KEY!
        _handle_annotations_before_blck_key_scalar();
        m_evt_handler->set_key_scalar_folded(maybe_filtered);
        addrem_flags(RKCL, QMRK);
    }
    else if(_scan_scalar_plain_map_blck(&sc))
    {
        _c4dbgp("mapblck[QMRK]: plain scalar");
        csubstr maybe_filtered = _maybe_filter_key_scalar_plain(sc, m_evt_handler->m_curr->indref); // KEY!
        addrem_flags(RKCL, QMRK);
        if(!_maybe_scan_following_colon())
        {
            _c4dbgp("mapblck[QMRK]: set as key");
            _handle_annotations_before_blck_key_scalar();
            m_evt_handler->set_key_scalar_plain(maybe_filtered);
        }
        else
        {
            _c4dbgp("mapblck[QMRK]: start new block map as key (!), set scalar as key");
            _handle_annotations_before_start_mapblck_as_key();
            m_evt_handler->begin_map_key_block();
            _handle_annotations_and_indentation_after_start_mapblck(startindent, startline);
            m_evt_handler->set_key_scalar_plain(maybe_filtered);
            _maybe_skip_whitespace_tokens();
            _set_indentation(startindent);
            // keep the child state on RVAL
            addrem_flags(RVAL, RKCL);
        }
    }
    else if(first == ':')
    {
        _c4dbgp("mapblck[QMRK]: start new block map as key (!), empty key");
        addrem_flags(RKCL, QMRK);
        _handle_annotations_before_start_mapblck_as_key();
        m_evt_handler->begin_map_key_block();
        _handle_annotations_and_indentation_after_start_mapblck(startindent, startline);
        m_evt_handler->set_key_scalar_plain_empty();
        _line_progressed(1);
        _maybe_skip_whitespace_tokens();
        _set_indentation(startindent);
        // keep the child state on RVAL
        addrem_flags(RVAL, RKCL);
    }
    else if(first == '*')
    {
        csubstr ref = _scan_ref_map();
        _c4dbgpf("mapblck[QMRK]: key ref! {}", prs_(ref));
        addrem_flags(RKCL, QMRK);
        if(!_maybe_scan_following_colon())
        {
            _c4dbgp("mapblck[QMRK]: set ref as key");
            _handle_keyref(ref);
        }
        else
        {
            _c4dbgp("mapblck[QMRK]: start new block map as key (!), set ref as key");
            _handle_annotations_before_start_mapblck_as_key();
            m_evt_handler->begin_map_key_block();
            _handle_annotations_and_indentation_after_start_mapblck(startindent, startline);
            _handle_keyref(ref);
            _set_indentation(startindent);
            // keep the child state on RVAL
            addrem_flags(RVAL, RKCL|QMRK);
        }
        _maybe_skip_whitespace_tokens();
    }
    else if(first == '&')
    {
        csubstr anchor = _scan_anchor();
        _c4dbgpf("mapblck[QMRK]: key anchor! {}", prs_(anchor));
        _add_annotation(&m_pending_anchors, anchor, startindent, startline);
    }
    else if(first == '!')
    {
        csubstr tag = _scan_tag();
        _c4dbgpf("mapblck[QMRK]: key tag! {}", prs_(tag));
        _add_annotation(&m_pending_tags, tag, startindent, startline);
    }
    else if(first == '-')
    {
        _c4dbgp("mapblck[QMRK]: maybe seq or doc?");
        if(_is_blck_seq_token_maybe(m_evt_handler->m_curr->line_contents.rem))
        {
            _c4dbgp("mapblck[QMRK]: start child seqblck (!)");
            addrem_flags(RKCL, QMRK);
            _handle_annotations_before_blck_key_scalar();
            m_evt_handler->begin_seq_key_block();
            addrem_flags(RVAL|RSEQ, RMAP|RKCL);
            _set_indentation(startindent);
            _line_progressed(1);
        }
        else
        {
            _c4dbgp("mapblck[QMRK]: end+start doc");
            _c4assert(_is_doc_begin_token(m_evt_handler->m_curr->line_contents.rem));
            _start_doc_suddenly();
            _line_progressed(3);
        }
        _maybe_skip_whitespace_tokens();
        return false; // finish mapblck
    }
    else if(first == '[')
    {
        _c4dbgp("mapblck[QMRK]: start child seqflow (!)");
        addrem_flags(RKCL, QMRK);
        _handle_annotations_before_blck_key_scalar();
        m_evt_handler->begin_seq_key_flow();
        addrem_flags(RVAL|RSEQ|RFLOW, RMAP|RKCL|RBLCK);
        _set_indentation(m_evt_handler->m_parent->indref + 1);
        _line_progressed(1);
        return false; // finish mapblck
    }
    else if(first == '{')
    {
        _c4dbgp("mapblck[QMRK]: start child mapflow (!)");
        addrem_flags(RKCL, QMRK);
        _handle_annotations_before_blck_key_scalar();
        m_evt_handler->begin_map_key_flow();
        addrem_flags(RKEY|RFLOW, RVAL|RKCL|RBLCK);
        _set_indentation(m_evt_handler->m_parent->indref + 1);
        _line_progressed(1);
        return false; // finish mapblck
    }
    else if(first == '?')
    {
        _c4dbgpf("mapblck[QMRK]: another QMRK '?'. ind={} indref={}", startindent, m_evt_handler->m_curr->indref);
        RYML_ASSERT_PARSE_CB_(callbacks(), startindent > m_evt_handler->m_curr->indref, m_evt_handler->m_curr->pos);
        _c4dbgp("mapblck[QMRK]: ? indent gt - start child mapblck (!)");
        addrem_flags(RKCL, QMRK);
        _handle_annotations_before_blck_key_scalar();
        m_evt_handler->begin_map_key_block();
        addrem_flags(QMRK, RKCL);
        _set_indentation(startindent);
        // indentation_lt() should be handled elsewhere
        _line_progressed(1);
        _maybe_skipchars(' ');
        if(_is_blck_seq_token_maybe(m_evt_handler->m_curr->line_contents.rem))
        {
            _c4dbgp("mapblck[RVAL]: seqblck starts after ?");
            addrem_flags(RKCL, QMRK);
            m_evt_handler->begin_seq_key_block();
            addrem_flags(RSEQ|RVAL, RMAP|RKCL);
            _save_indentation();
            _line_progressed(1);
            _maybe_skipchars(' ');
            return false;
        }
    }
    else
    {
        _c4err("parse error");
    }
    return true; // continue in mapblck
}


//-----------------------------------------------------------------------------

// return true if we should remain in map_block
template<class EventHandler>
bool ParseEngine<EventHandler>::_handle_map_block_rkcl()
{
    //
    // handle indentation
    //
    if(m_evt_handler->m_curr->at_line_beginning())
    {
        if(m_evt_handler->m_curr->indentation_eq())
        {
            _c4dbgpf("mapblck[RKCL]: skip {} from indref", m_evt_handler->m_curr->indref);
            _line_progressed(m_evt_handler->m_curr->indref);
            if(!m_evt_handler->m_curr->line_contents.rem.len)
                return true; // continue in mapblck
        }
        else if C4_UNLIKELY(m_evt_handler->m_curr->indentation_lt())
        {
            _c4err("invalid indentation");
        }
    }
    const char first = m_evt_handler->m_curr->line_contents.rem.str[0];
    _c4dbgpf("mapblck[RKCL]: '{}'", first);
    if(first == ':')
    {
        _c4dbgp("mapblck[RKCL]: found the colon");
        _line_progressed(1);
        _maybe_skipchars(' ');
        #if defined(__GNUC__) && (                                      \
            ((__GNUC__ >= 12) && ((C4_WORDSIZE == 4) || defined(C4_CPU_S390_X) || defined(C4_CPU_PPC64))) \
            ||                                                          \
            (__GNUC__ == 16 && defined(C4_CPU_X86_64)))
        C4_DONT_OPTIMIZE(m_evt_handler->m_curr->line_contents.rem);
        #endif
        // sequence is valid after the RKCL ':'
        if(!_is_blck_seq_token_maybe(m_evt_handler->m_curr->line_contents.rem))
        {
            addrem_flags(RVAL, RKCL);
            return true; // continue in mapblck
        }
        else
        {
            _c4dbgp("mapblck[RKCL]: start val seqblck");
            addrem_flags(RNXT, RKCL);
            m_evt_handler->begin_seq_val_block();
            addrem_flags(RSEQ|RVAL, RMAP|RNXT);
            _save_indentation();
            _line_progressed(1);
            _maybe_skipchars(' ');
            return false; // finish mapblck
        }
    }
    else if(first == '?')
    {
        _c4dbgp("mapblck[RKCL]: got '?'. val was empty");
        m_evt_handler->set_val_scalar_plain_empty();
        m_evt_handler->add_sibling();
        addrem_flags(QMRK, RKCL);
        _line_progressed(1);
        _maybe_skipchars(' ');
        if(_is_blck_seq_token_maybe(m_evt_handler->m_curr->line_contents.rem))
        {
            _c4dbgp("mapblck[RKCL]: seqblck starts after ?");
            addrem_flags(RKCL, QMRK);
            m_evt_handler->begin_seq_key_block();
            addrem_flags(RSEQ|RVAL, RMAP|QMRK);
            _save_indentation();
            _line_progressed(1);
            _maybe_skipchars(' ');
            return false;
        }
    }
    else if(first == '-')
    {
        if(m_evt_handler->m_curr->indref == 0 || m_evt_handler->m_curr->line_contents.indentation == 0 || _is_doc_begin_token(m_evt_handler->m_curr->line_contents.rem))
        {
            _c4dbgp("mapblck[RKCL]: end+start doc");
            RYML_CHECK_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, _is_doc_begin_token(m_evt_handler->m_curr->line_contents.rem), m_evt_handler->m_curr->pos);
            _start_doc_suddenly();
            _line_progressed(3);
            _maybe_skip_whitespace_tokens();
            return false; // finish mapblck
        }
        else
        {
            _c4err("parse error"); // LCOV_EXCL_LINE
        }
    }
    else if(first == '.')
    {
        _c4dbgp("mapblck[RKCL]: maybe end doc?");
        csubstr rs = m_evt_handler->m_curr->line_contents.rem.sub(1);
        if(rs == ".." || rs.begins_with(".. "))
        {
            _c4dbgp("mapblck[RKCL]: end+start doc");
            _end_doc_suddenly();
            _line_progressed(3);
            _maybe_skip_whitespace_tokens();
            _check_doc_end_tokens();
            return false; // finish mapblck
        }
        else
        {
            _c4err("parse error"); // LCOV_EXCL_LINE
        }
    }
    else/* if(m_was_inside_qmrk) */
    {
        _c4dbgp("mapblck[RKCL]: missing :");
        if C4_UNLIKELY(!m_evt_handler->m_curr->indentation_eq())
            _c4err("parse error"); // LCOV_EXCL_LINE
        m_evt_handler->set_val_scalar_plain_empty();
        m_evt_handler->add_sibling();
        addrem_flags(RKEY, RKCL);
    }
    return true;
}


//-----------------------------------------------------------------------------

template<class EventHandler>
void ParseEngine<EventHandler>::_handle_unk_json()
{
    _c4dbgpf("handle_unk_json indref={} target={}", m_evt_handler->m_curr->indref, m_evt_handler->m_curr->node_id);

    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_none(RNXT|RSEQ|RMAP), m_evt_handler->m_curr->pos);
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_all(RTOP), m_evt_handler->m_curr->pos);

    _maybe_skip_comment();
    csubstr rem = m_evt_handler->m_curr->line_contents.rem;
    if(!rem.len)
        return;

    size_t pos = rem.first_not_of(" \t");
    if(pos)
    {
        pos = pos != npos ? pos : rem.len;
        _c4dbgpf("skipping indentation of {}", pos);
        _line_progressed(pos);
        rem = m_evt_handler->m_curr->line_contents.rem;
        if(!rem.len)
            return;
        _c4dbgpf("rem is now {}", prs_(rem));
    }

    if(rem.begins_with('['))
    {
        _c4dbgp("it's a seq");
        _check_trailing_doc_token();
        _maybe_begin_doc();
        m_evt_handler->begin_seq_val_flow();
        addrem_flags(RSEQ|RFLOW|RVAL, RUNK|RTOP|RDOC);
        _set_indentation(m_evt_handler->m_curr->line_contents.current_col(rem));
        m_doc_empty = false;
        _line_progressed(1);
    }
    else if(rem.begins_with('{'))
    {
        _c4dbgp("it's a map");
        _check_trailing_doc_token();
        _maybe_begin_doc();
        m_evt_handler->begin_map_val_flow();
        addrem_flags(RMAP|RFLOW|RKEY, RVAL|RTOP|RUNK|RDOC);
        m_doc_empty = false;
        _set_indentation(m_evt_handler->m_curr->line_contents.current_col(rem));
        _line_progressed(1);
    }
    else if(_handle_bom())
    {
        _c4dbgp("byte order mark");
    }
    else
    {
        RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks,  ! has_any(SSCL), m_evt_handler->m_curr->pos);
        _maybe_skip_whitespace_tokens();
        csubstr s = m_evt_handler->m_curr->line_contents.rem;
        if(!s.len)
            return;
        const size_t startindent = m_evt_handler->m_curr->line_contents.indentation; // save
        const char first = s.str[0];
        ScannedScalar sc;
        if(first == '"')
        {
            _c4dbgp("runk_json: scanning double-quoted scalar");
            _check_trailing_doc_token();
            _maybe_begin_doc();
            add_flags(RDOC);
            m_doc_empty = false;
            sc = _scan_scalar_dquot();
            csubstr maybe_filtered = _maybe_filter_val_scalar_dquot(sc);
            if(!_maybe_scan_following_colon())
            {
                _c4dbgp("runk_json: set as val");
                _handle_annotations_before_blck_val_scalar();
                m_evt_handler->set_val_scalar_dquoted(maybe_filtered);
            }
            else
            {
                _c4err("parse error");
            }
        }
        else if(_scan_scalar_plain_unk(&sc))
        {
            _c4dbgp("runk_json: got a plain scalar");
            _check_trailing_doc_token();
            _maybe_begin_doc();
            add_flags(RDOC);
            m_doc_empty = false;
            if(!_maybe_scan_following_colon())
            {
                _c4dbgp("runk_json: set as val");
                _handle_annotations_before_blck_val_scalar();
                csubstr maybe_filtered = _maybe_filter_val_scalar_plain(sc, startindent);
                m_evt_handler->set_val_scalar_plain(maybe_filtered);
            }
            else
            {
                _c4err("parse error"); // LCOV_EXCL_LINE
            }
        }
        else
        {
            _c4err("parse error"); // LCOV_EXCL_LINE
        }
    }
}


//-----------------------------------------------------------------------------

template<class EventHandler>
void ParseEngine<EventHandler>::_handle_unk()
{
    _c4dbgpf("handle_unk indref={} target={}", m_evt_handler->m_curr->indref, m_evt_handler->m_curr->node_id);

    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_none(RNXT|RSEQ|RMAP), m_evt_handler->m_curr->pos);
    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_all(RTOP), m_evt_handler->m_curr->pos);

    _maybe_skipchars(' ');
    _maybe_skip_comment();

    if(!m_evt_handler->m_curr->line_contents.rem.len)
        return;

    _c4dbgpf("runk: rem is now {}", prs_(m_evt_handler->m_curr->line_contents.rem));

    if(m_evt_handler->m_curr->line_contents.indentation == 0u && (m_evt_handler->m_curr->at_line_beginning() || (m_bom_len && (m_evt_handler->m_curr->pos.line == m_bom_line))))
    {
        _c4dbgpf("runk: rtop: zero indent + at line begin. offset={}", m_evt_handler->m_curr->pos.offset);
        _c4dbgp("runk: check BOM");
        if(_handle_bom())
        {
            m_bom_line = m_evt_handler->m_curr->pos.line;
            _c4dbgpf("runk: byte order mark! line={} offset={}", m_bom_line, m_evt_handler->m_curr->pos.offset);
            return;
        }
        const char first = m_evt_handler->m_curr->line_contents.rem.str[0];
        _c4dbgpf("runk: rtop: first={}", _c4prc(first));
        if(first == '-')
        {
            _c4dbgp("runk: rtop: suspecting doc");
            if(_is_doc_begin_token(m_evt_handler->m_curr->line_contents.rem))
            {
                _c4dbgp("runk: rtop: begin doc");
                _maybe_end_doc();
                _begin2_doc_expl();
                _set_indentation(0);
                addrem_flags(RDOC|RUNK, NDOC);
                _line_progressed(3u);
                _maybe_skip_whitespace_tokens();
                return;
            }
        }
        else if(first == '.')
        {
            _c4dbgp("runk: rtop: suspecting doc end");
            if(_is_doc_end_token(m_evt_handler->m_curr->line_contents.rem))
            {
                _c4dbgp("runk: rtop: end doc");
                if(has_any(RDOC))
                {
                    _end2_doc_expl();
                }
                else
                {
                    _c4dbgp("runk: rtop: ignore end doc");
                }
                addrem_flags(NDOC|RUNK, RDOC);
                _line_progressed(3u);
                _maybe_skip_whitespace_tokens();
                _check_doc_end_tokens();
                return;
            }
        }
        else if(first == '%')
        {
            _c4dbgpf("directive: {}", m_evt_handler->m_curr->line_contents.rem);
            if C4_UNLIKELY(has_any(RDOC) || (!m_doc_empty && has_none(NDOC)))
                _c4err("need document footer before directives");
            _handle_directive(m_evt_handler->m_curr->line_contents.rem);
            return;
        }
    }

    /* no else-if! */

    size_t startindent = m_evt_handler->m_curr->line_contents.indentation;
    size_t remindent = m_evt_handler->m_curr->line_contents.current_col(m_evt_handler->m_curr->line_contents.rem);
    if(m_bom_len)
    {
        _c4dbgpf("runk: prev BOMlen={}", m_bom_len);
        if(m_evt_handler->m_curr->pos.line == m_bom_line)
        {
            _c4dbgpf("runk: BOM remindent={} offset={}", remindent, m_evt_handler->m_curr->pos.offset);
            RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, remindent >= m_bom_len, m_evt_handler->m_curr->pos);
            remindent -= m_bom_len;
        }
        else
        {
            m_bom_len = 0;
        }
    }

    size_t startcol = _handle_block_skip_leading_whitespace();
    const char first = m_evt_handler->m_curr->line_contents.rem.str[0];

    if(first == '[')
    {
        _c4dbgp("runk: flow seq?");
        _handle_unk_begin_doc();
        if C4_LIKELY( ! _annotations_require_key_container())
        {
            _c4dbgp("runk: it's a seq, flow");
            _handle_annotations_before_blck_val_scalar();
            m_evt_handler->begin_seq_val_flow();
            addrem_flags(RSEQ|RFLOW|RVAL, RUNK|RTOP|RDOC);
            _set_indentation(0);
        }
        else
        {
            _c4dbgp("runk: start new block map, set flow seq as key (!)");
            _handle_annotations_before_start_mapblck(m_evt_handler->m_curr->pos.line);
            m_evt_handler->begin_map_val_block();
            addrem_flags(RMAP|RBLCK|RKEY, RUNK|RTOP|RDOC);
            _handle_annotations_and_indentation_after_start_mapblck(remindent, m_evt_handler->m_curr->pos.line);
            m_evt_handler->begin_seq_key_flow();
            addrem_flags(RSEQ|RFLOW|RVAL, RMAP|RBLCK|RKEY);
            _set_indentation(0);
        }
        _line_progressed(1);
    }
    else if(first == '{')
    {
        _c4dbgp("runk: flow map?");
        _handle_unk_begin_doc();
        if C4_LIKELY( ! _annotations_require_key_container())
        {
            _c4dbgp("runk: it's a map, flow");
            _handle_annotations_before_blck_val_scalar();
            m_evt_handler->begin_map_val_flow();
            addrem_flags(RMAP|RFLOW|RKEY, RVAL|RTOP|RUNK|RDOC);
            _set_indentation(0);
        }
        else
        {
            _c4dbgp("runk: start new block map, set flow map as key (!)");
            _handle_annotations_before_start_mapblck(m_evt_handler->m_curr->pos.line);
            m_evt_handler->begin_map_val_block();
            addrem_flags(RMAP|RBLCK|RKEY, RUNK|RTOP|RDOC);
            _handle_annotations_and_indentation_after_start_mapblck(remindent, m_evt_handler->m_curr->pos.line);
            m_evt_handler->begin_map_key_flow();
            addrem_flags(RMAP|RFLOW, RBLCK);
            _set_indentation(0);
        }
        _line_progressed(1);
    }
    else if(first == '-' && _is_blck_token(m_evt_handler->m_curr->line_contents.rem))
    {
        _c4dbgp("runk: it's a seq, block");
        if C4_UNLIKELY(!m_evt_handler->m_curr->at_first_token())
            startindent = _handle_unk_check_left_tokens(startindent, m_evt_handler->m_curr->pos.col, /*skip_annotations*/false);
        _handle_unk_begin_doc();
        _handle_annotations_before_blck_val_scalar();
        m_evt_handler->begin_seq_val_block();
        addrem_flags(RSEQ|RBLCK|RVAL, RNXT|RTOP|RUNK|RDOC);
        _set_indentation(startindent);
        _line_progressed(1);
        _maybe_skipchars(' ');
    }
    else if(first == '?' && _is_blck_token(m_evt_handler->m_curr->line_contents.rem))
    {
        _c4dbgp("runk: it's a map + this key is complex");
        if C4_UNLIKELY(!m_evt_handler->m_curr->at_first_token())
            startindent = _handle_unk_check_left_tokens(startindent, m_evt_handler->m_curr->pos.col, /*skip_annotations*/false);
        _handle_block_check_leading_tabs(startcol);
        _handle_unk_begin_doc();
        _handle_annotations_before_blck_val_scalar();
        m_evt_handler->begin_map_val_block();
        addrem_flags(RMAP|RBLCK|QMRK, RKEY|RVAL|RTOP|RUNK|RDOC);
        _set_indentation(startindent);
        _line_progressed(1);
        _maybe_skipchars(' ');
        if(_is_blck_seq_token_maybe(m_evt_handler->m_curr->line_contents.rem))
        {
            _c4dbgp("runk: seqblck key starts after ?");
            addrem_flags(RKCL, QMRK);
            m_evt_handler->begin_seq_key_block();
            addrem_flags(RSEQ|RVAL, RMAP|RKCL);
            _save_indentation();
            _line_progressed(1);
            _maybe_skipchars(' ');
        }
    }
    else if(first == ':' && _is_blck_token(m_evt_handler->m_curr->line_contents.rem))
    {
        if(m_doc_empty || (m_pending_anchors.num_entries | m_pending_tags.num_entries))
        {
            _c4dbgp("runk: it's a map with an empty key");
            if C4_UNLIKELY(!m_evt_handler->m_curr->at_first_token())
                startindent = _handle_unk_check_left_tokens(startindent, m_evt_handler->m_curr->pos.col);
            _handle_block_check_leading_tabs(startcol);
            const size_t startline = m_evt_handler->m_curr->pos.line; // save
            _handle_unk_begin_doc();
            _handle_annotations_before_start_mapblck(startline);
            _handle_colon();
            m_evt_handler->begin_map_val_block();
            _handle_annotations_and_indentation_after_start_mapblck(startindent, startline);
            m_evt_handler->set_key_scalar_plain_empty();
            _set_indentation(startindent);
        }
        else
        {
            _c4err("block colon cannot occur on a new line unless ? is used");
        }
        addrem_flags(RMAP|RBLCK|RVAL, RTOP|RUNK|RDOC);
        _line_progressed(1);
        _maybe_skip_whitespace_tokens();
    }
    else if(first == '&')
    {
        csubstr anchor = _scan_anchor();
        _c4dbgpf("anchor! {}", prs_(anchor));
        const size_t line = m_evt_handler->m_curr->pos.line;
        _handle_unk_begin_doc();
        _add_annotation(&m_pending_anchors, anchor, remindent, line);
        _set_indentation(0);
    }
    else if(first == '*')
    {
        csubstr ref = _scan_ref_map();
        _c4dbgpf("runk: ref! {}", prs_(ref));
        _handle_unk_begin_doc();
        if(!_maybe_scan_following_colon())
        {
            _c4dbgp("runk: set val ref");
            _handle_valref(ref);
        }
        else
        {
            _c4dbgp("runk: start new block map, set ref as key");
            _handle_block_check_leading_tabs(startcol);
            const size_t startline = m_evt_handler->m_curr->pos.line; // save
            _handle_annotations_before_start_mapblck(startline);
            m_evt_handler->begin_map_val_block();
            _handle_keyref(ref);
            _maybe_skip_whitespace_tokens();
            _set_indentation(0);
            addrem_flags(RMAP|RBLCK|RVAL, RTOP|RUNK|RDOC);
        }
    }
    else if(first == '!')
    {
        csubstr tag_orig;
        csubstr tag = _scan_tag(&tag_orig);
        _c4dbgpf("runk: val tag! {}", prs_(tag));
        // we need to buffer the tags, as there may be two
        // consecutive tags in here
        const size_t indentation = m_evt_handler->m_curr->line_contents.current_col(m_evt_handler->m_curr->line_contents.rem);
        const size_t line = m_evt_handler->m_curr->pos.line;
        _add_annotation(&m_pending_tags, tag, indentation, line, tag_orig);
    }
    else
    {
        RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks,  ! has_any(SSCL), m_evt_handler->m_curr->pos);
        const size_t startscalar = _handle_block_get_whitespace_mark();
        const size_t startline = m_evt_handler->m_curr->pos.line; // save
        auto beginmap = [&](size_t startindent_){
            if C4_UNLIKELY(m_evt_handler->m_curr->pos.line > startline)
                _c4err("multiline scalars cannot be used as implicit keys");
            _handle_block_check_leading_tabs(startcol, startscalar);
            _handle_annotations_before_start_mapblck(startline);
            _handle_colon();
            m_evt_handler->begin_map_val_block();
            _handle_annotations_and_indentation_after_start_mapblck(startindent_, startline);
        };
        auto after_beginmap = [&](size_t startindent_){
            _maybe_skip_whitespace_tokens();
            _set_indentation(startindent_);
            addrem_flags(RMAP|RBLCK|RVAL, RTOP|RUNK|RDOC);
        };
        if(first == '|')
        {
            _c4dbgp("runk: block-literal scalar");
            _handle_unk_begin_doc();
            ScannedBlock sb;
            _scan_block(&sb, startindent);
            _handle_annotations_before_blck_val_scalar();
            csubstr maybe_filtered = _maybe_filter_val_scalar_literal(sb);
            m_evt_handler->set_val_scalar_literal(maybe_filtered);
        }
        else if(first == '>')
        {
            _c4dbgp("runk: block-folded scalar");
            _handle_unk_begin_doc();
            ScannedBlock sb;
            _scan_block(&sb, startindent);
            _handle_annotations_before_blck_val_scalar();
            csubstr maybe_filtered = _maybe_filter_val_scalar_folded(sb);
            m_evt_handler->set_val_scalar_folded(maybe_filtered);
        }
        else if(first == '\'')
        {
            _c4dbgp("runk: single-quoted scalar");
            _handle_unk_begin_doc();
            bool firsttoken = m_evt_handler->m_curr->at_first_token();
            size_t col = m_evt_handler->m_curr->pos.col;
            ScannedScalar sc = _scan_scalar_squot();
            if(!_maybe_scan_following_colon())
            {
                _c4dbgp("runk: set as val");
                _handle_annotations_before_blck_val_scalar();
                csubstr maybe_filtered = _maybe_filter_val_scalar_squot(sc);
                m_evt_handler->set_val_scalar_squoted(maybe_filtered);
            }
            else
            {
                _c4dbgp("runk: start new block map, set single-quoted scalar as key");
                if C4_UNLIKELY(m_evt_handler->m_curr->pos.line > startline)
                    _c4err("multiline key");
                if(!firsttoken)
                    startindent = _handle_unk_check_left_tokens(startindent, col);
                beginmap(startindent);
                csubstr maybe_filtered = _maybe_filter_val_scalar_squot(sc);
                m_evt_handler->set_key_scalar_squoted(maybe_filtered);
                after_beginmap(startindent);
            }
        }
        else if(first == '"')
        {
            _c4dbgp("runk: double-quoted scalar");
            _handle_unk_begin_doc();
            bool firsttoken = m_evt_handler->m_curr->at_first_token();
            size_t col = m_evt_handler->m_curr->pos.col;
            ScannedScalar sc = _scan_scalar_dquot();
            if(!_maybe_scan_following_colon())
            {
                _c4dbgp("runk: set as val");
                _handle_annotations_before_blck_val_scalar();
                csubstr maybe_filtered = _maybe_filter_val_scalar_dquot(sc);
                m_evt_handler->set_val_scalar_dquoted(maybe_filtered);
            }
            else
            {
                _c4dbgp("runk: start new block map, set double-quoted scalar as key");
                if C4_UNLIKELY(m_evt_handler->m_curr->pos.line > startline)
                    _c4err("multiline key");
                if(!firsttoken)
                    startindent = _handle_unk_check_left_tokens(startindent, col);
                beginmap(startindent);
                csubstr maybe_filtered = _maybe_filter_val_scalar_dquot(sc);
                m_evt_handler->set_key_scalar_dquoted(maybe_filtered);
                after_beginmap(startindent);
            }
        }
        else
        {
            bool firsttoken = m_evt_handler->m_curr->at_first_token();
            size_t col = m_evt_handler->m_curr->pos.col;
            ScannedScalar sc;
            if(_scan_scalar_plain_unk(&sc))
            {
                _c4dbgp("runk: plain scalar");
                _handle_unk_begin_doc();
                if(!_maybe_scan_following_colon())
                {
                    _c4dbgp("runk: set as val");
                    _handle_annotations_before_blck_val_scalar();
                    csubstr maybe_filtered = _maybe_filter_val_scalar_plain(sc, startindent);
                    m_evt_handler->set_val_scalar_plain(maybe_filtered);
                }
                else
                {
                    _c4dbgp("runk: start new block map, set plain scalar as key");
                    // there is already a check to multiline inside
                    // _scan_scalar_plain_unk(), so we don't need to
                    // throw an error here. but let's be safe by
                    // asserting the assumption:
                    _c4assert(m_evt_handler->m_curr->pos.line == startline);
                    if(!firsttoken)
                        startindent = _handle_unk_check_left_tokens(startindent, col);
                    beginmap(startindent);
                    csubstr maybe_filtered = _maybe_filter_val_scalar_plain(sc, startindent);
                    m_evt_handler->set_key_scalar_plain(maybe_filtered);
                    after_beginmap(startindent);
                }
            }
            else
            {
                _c4err("parse error"); // LCOV_EXCL_LINE
            }
        }
    }

    if(m_bom_len && has_none(RUNK))
    {
        _c4dbgpf("runk: BOMlen={} BOMline={} now={} at_end={}", m_bom_len, m_bom_line, m_evt_handler->m_curr->pos.line, !m_evt_handler->m_curr->line_contents.rem.len);
        if(m_evt_handler->m_curr->pos.line != m_bom_line || !m_evt_handler->m_curr->line_contents.rem.len)
        {
            _c4dbgp("runk: clear BOMlen");
            m_bom_len = 0;
        }
    }
}

template<class EventHandler>
void ParseEngine<EventHandler>::_handle_unk_begin_doc()
{
    _c4dbgp("runk: begin doc");
    _check_trailing_doc_token();
    _maybe_begin_doc();
    add_flags(RDOC);
    m_doc_empty = false;
}

template<class EventHandler>
size_t ParseEngine<EventHandler>::_handle_unk_check_left_tokens(size_t realindent, size_t col, bool skip_annotations)
{
    _c4assert(col >= 1);
    col -= 1;
    _c4assert(col >= m_bom_len);
    csubstr s = m_evt_handler->m_curr->line_contents.full.range(m_bom_len, col);
    size_t pos = 0;
    _c4dbgpf("runk: check left tokens: s={}", prs_(s, /*escape*/true));
    if(skip_annotations)
    {
        _handle_unk_get_first_non_pending_token_pos(s, &realindent, &pos);
        _c4dbgpf("runk: skip annotations: realindent={} pos={}", realindent, pos);
    }
    size_t firstns = s.first_not_of(' ', pos);
    if(firstns == npos)
        firstns = s.len;
    _c4dbgpf("runk: check left tokens:\n"
             "  tokens={} skipped={}\n"
             "  bomlen={}  first={} col={}\n"
             "  (bomlen+first)={} vs {}=col\n"
             "  startindent={}  lineindent={}"
             , prs_(s, /*escape*/true), prs_(s.sub(firstns), /*escape*/true)
             , m_bom_len, firstns, col
             , m_bom_len+firstns, col,
             realindent, m_evt_handler->m_curr->line_contents.indentation);
    if(m_bom_len + firstns != col)
        _c4err("parse error");
    if(!skip_annotations)
        realindent = firstns;
    _c4dbgpf("runk: pos={} firstns={}  -> realindent={}", pos, firstns, realindent);
    return realindent;
}


/** skip annotations which are pending on the same line */
template<class EventHandler>
void ParseEngine<EventHandler>::_handle_unk_get_first_non_pending_token_pos(csubstr s, size_t *indent, size_t *first_non_token_pos)
{
    csubstr first, second;
    uint32_t total = _get_annotations_same_line(s, &first, &second);
    _c4dbgpf("runk: before skip: {}", prs_(s, true));
    size_t pos = s.first_not_of(" \t");
    if(pos == npos)
        pos = s.len;
    if(!total)
    {
        *indent = *first_non_token_pos = pos;
        return;
    }
    _c4assert(!s.sub(pos).begins_with_any(" \t"));
    _c4dbgpf("runk: after skip leading {} whitespace: {}", pos, prs_(s.sub(pos), true));
    _c4dbgpf("runk: first annotation: {}", first);
    _c4assert(first.len);
    _c4assert(first.is_sub(s));
    _c4assert(first.is_sub(s.sub(pos)));
    _c4assert(s.sub(pos).begins_with(first));
    *indent = pos;
    pos += first.len;
    _c4dbgpf("runk: after skip first annotation: pos={} {}", pos, prs_(s.sub(pos), true));
    if(total > 1)
    {
        _c4dbgpf("runk: second annotation: {}", second);
        _c4assert(total == 2);
        _c4assert(second.len);
        _c4assert(second.is_sub(s));
        _c4assert(second.is_sub(s.sub(pos)));
        csubstr spos = s.sub(pos);
        size_t more = spos.first_not_of(" \t");
        _c4assert(more != npos); // because the annotations are on the same line
        _c4dbgpf("runk: next nonspace: {}", pos + more);
        pos += more;
        _c4dbgpf("runk: after skip annotation whitespace: pos={} {}", pos, prs_(s.sub(pos), true));
        _c4assert(s.sub(pos).begins_with(second));
        pos += second.len;
        _c4dbgpf("runk: after skip annotation 2: pos={} {}", pos, prs_(s.sub(pos), true));
    }
    *first_non_token_pos = pos;
}


template<class EventHandler>
uint32_t ParseEngine<EventHandler>::_get_annotations_same_line(csubstr token_soup, csubstr *first_, csubstr *second_) const
{
    _c4assert(!m_evt_handler->m_curr->at_first_token());
    (void)token_soup;
    using EntryPtr = typename Annotation::Entry const* C4_RESTRICT;
    EntryPtr first = nullptr;
    EntryPtr second = nullptr;
    uint32_t total = (uint32_t)(m_pending_anchors.num_entries + m_pending_tags.num_entries);
    if(total)
    {
        _c4dbgpf("there are {} pending annotations: {} anchors + {} tags", total, m_pending_anchors.num_entries, m_pending_tags.num_entries);
        auto valid_if_same_line = [this](EntryPtr entry){
            _c4dbgpf("pending: {} indent={} line={} vs currline={}", maybe_null_str_(entry->str), entry->indentation, entry->line, m_evt_handler->m_curr->pos.line);
            return (entry->line == m_evt_handler->m_curr->pos.line) ? entry : nullptr;
        };
        // now select annotations only on the same line
        total = 0;
        for(size_t i = 0; i < m_pending_anchors.num_entries; ++i)
            total += !!valid_if_same_line(&m_pending_anchors.annotations[i]);
        for(size_t i = 0; i < m_pending_tags.num_entries; ++i)
            total += !!valid_if_same_line(&m_pending_tags.annotations[i]);
        _c4dbgpf("{} annotations on same line", total);
        _c4assert(total > 0); // because this function is only called
                              // while not at the first token. That
                              // means we must have same-line
                              // annotations.
        auto get_first_on_same_line = [this](EntryPtr not_this_one){
            for(size_t i = 0; i < m_pending_anchors.num_entries; ++i)
                if(&m_pending_anchors.annotations[i] != not_this_one
                   && m_pending_anchors.annotations[i].line == m_evt_handler->m_curr->pos.line)
                    return &m_pending_anchors.annotations[i];
            for(size_t i = 0; i < m_pending_tags.num_entries; ++i)
                if(&m_pending_tags.annotations[i] != not_this_one
                   && m_pending_tags.annotations[i].line == m_evt_handler->m_curr->pos.line)
                    return &m_pending_tags.annotations[i];
            C4_UNREACHABLE(); // LCOV_EXCL_LINE
            return (EntryPtr)nullptr; // LCOV_EXCL_LINE
        };
        _c4assert(total >= 1);
        // assign to first
        first = get_first_on_same_line(nullptr);
        _c4assert(first);
        _c4dbgpf("first annotation: {} indent={} line={}", maybe_null_str_(first->str), first->indentation, first->line);
        if(total > 1)
        {
            _c4assert(total == 2);
            // assign to second
            second = get_first_on_same_line(first);
            _c4assert(second);
            _c4dbgpf("second annotation: {} indent={} line={}", maybe_null_str_(second->str), second->indentation, second->line);
        }
        auto extract_string = [&](EntryPtr e){
            // tags can be null when the arena ran out of space
            if(!e->str.str || e->str.begins_with_any("!<"))
            {
                csubstr tag = e->orig;
                _c4assert(tag.str);
                _c4assert(tag.len);
                _c4assert(tag.is_sub(token_soup));
                _c4dbgpf("tag: {} -> {}", maybe_null_str_(e->str), tag);
                return tag;
            }
            csubstr anchor = e->str;
            _c4assert(anchor.len);
            _c4assert(anchor.str);
            _c4assert(anchor.is_sub(token_soup));
            _c4assert(!anchor.begins_with('&'));
            _c4assert(anchor.str - token_soup.str > 0);
            // add back the anchor's &
            --anchor.str;
            ++anchor.len;
            _c4assert(anchor.begins_with('&'));
            _c4dbgpf("anchor: {} -> {}", e->str, anchor);
            return anchor;
        };
        *first_ = first ? extract_string(first) : nullptr;
        *second_ = second ? extract_string(second) : nullptr;
        if(total > 1 && (first_->str > second_->str))
        {
            csubstr tmp = *first_;
            *first_ = *second_;
            *second_ = tmp;
            _c4dbgpf("swap first and second: {} -> {}", *first_, *second_);
        }
    }
    return total;
}


//-----------------------------------------------------------------------------

template<class EventHandler>
C4_COLD void ParseEngine<EventHandler>::_handle_usty()
{
    _c4dbgpf("handle_usty target={}", m_evt_handler->m_curr->indref, m_evt_handler->m_curr->node_id);

    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_none(RBLCK|RFLOW), m_evt_handler->m_curr->pos);

    #ifdef RYML_NO_COVERAGE__TO_BE_DELETED
    if(has_any(RNXT))
    {
        _c4dbgp("usty[RNXT]: finishing!");
        _end_stream();
    }
    #endif

    _maybe_skip_comment();
    csubstr rem = m_evt_handler->m_curr->line_contents.rem;
    if(!rem.len)
        return;

    size_t pos = rem.first_not_of(" \t");
    if(pos)
    {
        pos = pos != npos ? pos : rem.len;
        _c4dbgpf("skipping indentation of {}", pos);
        _line_progressed(pos);
        rem = m_evt_handler->m_curr->line_contents.rem;
        if(!rem.len)
            return;
        _c4dbgpf("rem is now {}", prs_(rem));
    }

    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, rem.len > 0, m_evt_handler->m_curr->pos);
    size_t startindent = m_evt_handler->m_curr->line_contents.indentation; // save
    char first = rem.str[0];
    if(has_any(RSEQ)) // destination is a sequence
    {
        RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks,  ! has_any(RMAP), m_evt_handler->m_curr->pos);
        _c4dbgpf("usty[RSEQ]: first='{}'", _c4prc(first));
        if(first == '[')
        {
            _c4dbgp("usty[RSEQ]: it's a flow seq. merging it");
            add_flags(RNXT);
            m_evt_handler->_push();
            addrem_flags(RFLOW|RVAL, RNXT|USTY);
            _set_indentation(startindent);
            _line_progressed(1);
            _maybe_skip_whitespace_tokens();
        }
        else if(first == '-' && _is_blck_token(rem))
        {
            _c4dbgp("usty[RSEQ]: it's a block seq. merging it");
            add_flags(RNXT);
            m_evt_handler->_push();
            addrem_flags(RBLCK|RVAL, RNXT|USTY);
            _set_indentation(startindent);
            _line_progressed(1);
            _maybe_skip_whitespace_tokens();
        }
        else
        {
            _c4err("can only parse a seq into an existing seq");
        }
    }
    else if(has_any(RMAP)) // destination is a map
    {
        RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks,  ! has_any(RSEQ), m_evt_handler->m_curr->pos);
        _c4dbgpf("usty[RMAP]: first='{}'", _c4prc(first));
        if(first == '{')
        {
            _c4dbgp("usty[RMAP]: it's a flow map. merging it");
            add_flags(RNXT);
            _handle_annotations_before_blck_val_scalar();
            m_evt_handler->_push();
            addrem_flags(RMAP|RFLOW|RKEY, RNXT|USTY);
            _set_indentation(startindent);
            _line_progressed(1);
            _maybe_skip_whitespace_tokens();
        }
        else if(first == '?' && _is_blck_token(rem))
        {
            _c4dbgp("usty[RMAP]: it's a block map + this key is complex");
            add_flags(RNXT);
            _handle_annotations_before_blck_val_scalar();
            m_evt_handler->_push();
            addrem_flags(RMAP|RBLCK|QMRK, RNXT|USTY);
            _save_indentation();
            _line_progressed(1);
            _maybe_skip_whitespace_tokens();
        }
        else if(first == ':' && _is_blck_token(rem))
        {
            _c4dbgp("usty[RMAP]: it's a map with an empty key");
            add_flags(RNXT);
            _handle_annotations_before_blck_val_scalar();
            m_evt_handler->_push();
            m_evt_handler->set_key_scalar_plain_empty();
            addrem_flags(RMAP|RBLCK|RVAL, RNXT|USTY);
            _save_indentation();
            _line_progressed(1);
            _maybe_skip_whitespace_tokens();
        }
        else if(rem.begins_with('&'))
        {
            csubstr anchor = _scan_anchor();
            _c4dbgpf("usty[RMAP]: anchor! {}", prs_(anchor));
            const size_t indentation = m_evt_handler->m_curr->line_contents.current_col(rem);
            const size_t line = m_evt_handler->m_curr->pos.line;
            _add_annotation(&m_pending_anchors, anchor, indentation, line);
            _set_indentation(m_evt_handler->m_curr->line_contents.current_col(rem));
        }
        else if(first == '*')
        {
            csubstr ref = _scan_ref_map();
            _c4dbgpf("usty[RMAP]: ref! {}", prs_(ref));
            if(!_maybe_scan_following_colon())
            {
                _c4err("cannot read a VAL to a map");
            }
            else
            {
                _c4dbgp("usty[RMAP]: start new block map, set ref as key");
                const size_t startline = m_evt_handler->m_curr->pos.line; // save
                add_flags(RNXT);
                _handle_annotations_before_start_mapblck(startline);
                m_evt_handler->_push();
                _handle_keyref(ref);
                _maybe_skip_whitespace_tokens();
                _set_indentation(startindent);
                addrem_flags(RMAP|RBLCK|RVAL, RNXT|USTY);
            }
        }
        else if(first == '!')
        {
            csubstr tag = _scan_tag();
            _c4dbgpf("usty[RMAP]: val tag! {}", prs_(tag));
            // we need to buffer the tags, as there may be two
            // consecutive tags in here
            const size_t indentation = m_evt_handler->m_curr->line_contents.current_col(rem);
            const size_t line = m_evt_handler->m_curr->pos.line;
            _add_annotation(&m_pending_tags, tag, indentation, line);
        }
        else if(first == '[' || (first == '-' && _is_blck_token(rem)))
        {
            _c4err("cannot parse a seq into an existing map");
        }
        else
        {
            RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks,  ! has_any(SSCL), m_evt_handler->m_curr->pos);
            startindent = m_evt_handler->m_curr->line_contents.indentation; // save
            const size_t startline = m_evt_handler->m_curr->pos.line; // save
            ScannedScalar sc;
            _c4dbgpf("usty[RMAP]: maybe scalar. first='{}'", _c4prc(first));
            if(first == '\'')
            {
                _c4dbgp("usty[RMAP]: scanning single-quoted scalar");
                sc = _scan_scalar_squot();
                if(!_maybe_scan_following_colon())
                {
                    _c4err("cannot read a VAL to a map");
                }
                else
                {
                    _c4dbgp("usty[RMAP]: start new block map, set scalar as key");
                    add_flags(RNXT);
                    _handle_annotations_before_start_mapblck(startline);
                    m_evt_handler->_push();
                    _handle_annotations_and_indentation_after_start_mapblck(startindent, startline);
                    csubstr maybe_filtered = _maybe_filter_key_scalar_squot(sc);
                    m_evt_handler->set_key_scalar_squoted(maybe_filtered);
                    _set_indentation(startindent);
                    addrem_flags(RMAP|RBLCK|RVAL, RNXT|USTY);
                    _maybe_skip_whitespace_tokens();
                }
            }
            else if(first == '"')
            {
                _c4dbgp("usty[RMAP]: scanning double-quoted scalar");
                sc = _scan_scalar_dquot();
                if(!_maybe_scan_following_colon())
                {
                    _c4err("cannot read a VAL to a map");
                }
                else
                {
                    _c4dbgp("usty[RMAP]: start new block map, set double-quoted scalar as key");
                    add_flags(RNXT);
                    _handle_annotations_before_start_mapblck(startline);
                    m_evt_handler->_push();
                    _handle_annotations_and_indentation_after_start_mapblck(startindent, startline);
                    csubstr maybe_filtered = _maybe_filter_key_scalar_dquot(sc);
                    m_evt_handler->set_key_scalar_dquoted(maybe_filtered);
                    _set_indentation(startindent);
                    addrem_flags(RMAP|RBLCK|RVAL, RNXT|USTY);
                    _maybe_skip_whitespace_tokens();
                }
            }
            else if(first == '|')
            {
                _c4err("block literal keys must be enclosed in '?'");
            }
            else if(first == '>')
            {
                _c4err("block literal keys must be enclosed in '?'");
            }
            else if(_scan_scalar_plain_unk(&sc))
            {
                _c4dbgp("usty[RMAP]: got a plain scalar");
                if(!_maybe_scan_following_colon())
                {
                    _c4err("cannot read a VAL to a map");
                }
                else
                {
                    _c4dbgp("usty[RMAP]: start new block map, set scalar as key");
                    add_flags(RNXT);
                    _handle_annotations_before_start_mapblck(startline);
                    m_evt_handler->_push();
                    _handle_annotations_and_indentation_after_start_mapblck(startindent, startline);
                    csubstr maybe_filtered = _maybe_filter_key_scalar_plain(sc, startindent);
                    m_evt_handler->set_key_scalar_plain(maybe_filtered);
                    _set_indentation(startindent);
                    addrem_flags(RMAP|RBLCK|RVAL, RNXT|USTY);
                    _maybe_skip_whitespace_tokens();
                }
            }
            else
            {
                _c4err("parse error"); // LCOV_EXCL_LINE
            }
        }
    }
    else // destination is unknown
    {
        RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks,  ! has_any(RSEQ), m_evt_handler->m_curr->pos);
        _c4dbgpf("usty[UNK]: first='{}'", _c4prc(first));
        if(first == '[')
        {
            _c4dbgp("usty[UNK]: it's a flow seq");
            add_flags(RNXT);
            _handle_annotations_before_blck_val_scalar();
            m_evt_handler->begin_seq_val_flow();
            addrem_flags(RSEQ|RFLOW|RVAL, RNXT|USTY);
            _set_indentation(startindent);
            _line_progressed(1);
            _maybe_skip_whitespace_tokens();
        }
        else if(first == '-' && _is_blck_token(rem))
        {
            _c4dbgp("usty[UNK]: it's a block seq");
            add_flags(RNXT);
            _handle_annotations_before_blck_val_scalar();
            m_evt_handler->begin_seq_val_block();
            addrem_flags(RSEQ|RBLCK|RVAL, RNXT|USTY);
            _set_indentation(startindent);
            _line_progressed(1);
            _maybe_skip_whitespace_tokens();
        }
        else if(first == '{')
        {
            _c4dbgp("usty[UNK]: it's a flow map");
            add_flags(RNXT);
            _handle_annotations_before_blck_val_scalar();
            m_evt_handler->begin_map_val_flow();
            addrem_flags(RMAP|RFLOW|RKEY, RNXT|USTY);
            _set_indentation(startindent);
            _line_progressed(1);
            _maybe_skip_whitespace_tokens();
        }
        else if(first == '?' && _is_blck_token(rem))
        {
            _c4dbgp("usty[UNK]: it's a map + this key is complex");
            add_flags(RNXT);
            _handle_annotations_before_blck_val_scalar();
            m_evt_handler->begin_map_val_block();
            addrem_flags(RMAP|RBLCK|QMRK, RNXT|USTY);
            _save_indentation();
            _line_progressed(1);
            _maybe_skip_whitespace_tokens();
        }
        else if(first == ':' && _is_blck_token(rem))
        {
            _c4dbgp("usty[UNK]: it's a map with an empty key");
            add_flags(RNXT);
            _handle_annotations_before_blck_val_scalar();
            m_evt_handler->begin_map_val_block();
            m_evt_handler->set_key_scalar_plain_empty();
            addrem_flags(RMAP|RBLCK|RVAL, RNXT|USTY);
            _save_indentation();
            _line_progressed(1);
            _maybe_skip_whitespace_tokens();
        }
        else if(first == '&')
        {
            csubstr anchor = _scan_anchor();
            _c4dbgpf("usty[UNK]: anchor! {}", prs_(anchor));
            const size_t indentation = m_evt_handler->m_curr->line_contents.current_col(rem);
            const size_t line = m_evt_handler->m_curr->pos.line;
            _add_annotation(&m_pending_anchors, anchor, indentation, line);
            _set_indentation(m_evt_handler->m_curr->line_contents.current_col(rem));
        }
        else if(first == '*')
        {
            csubstr ref = _scan_ref_map();
            _c4dbgpf("usty[UNK]: ref! {}", prs_(ref));
            if(!_maybe_scan_following_colon())
            {
                _c4dbgp("usty[UNK]: set val ref");
                _handle_valref(ref);
            }
            else
            {
                _c4dbgp("usty[UNK]: start new block map, set ref as key");
                const size_t startline = m_evt_handler->m_curr->pos.line; // save
                add_flags(RNXT);
                _handle_annotations_before_start_mapblck(startline);
                m_evt_handler->begin_map_val_block();
                _handle_keyref(ref);
                _maybe_skip_whitespace_tokens();
                _set_indentation(startindent);
                addrem_flags(RMAP|RBLCK|RVAL, RNXT|USTY);
            }
        }
        else if(first == '!')
        {
            csubstr tag = _scan_tag();
            _c4dbgpf("usty[UNK]: val tag! {}", prs_(tag));
            // we need to buffer the tags, as there may be two
            // consecutive tags in here
            const size_t indentation = m_evt_handler->m_curr->line_contents.current_col(rem);
            const size_t line = m_evt_handler->m_curr->pos.line;
            _add_annotation(&m_pending_tags, tag, indentation, line);
        }
        else
        {
            RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks,  ! has_any(SSCL), m_evt_handler->m_curr->pos);
            startindent = m_evt_handler->m_curr->line_contents.indentation; // save
            const size_t startline = m_evt_handler->m_curr->pos.line; // save
            first = rem.str[0];
            ScannedScalar sc;
            _c4dbgpf("usty[UNK]: maybe scalar. first='{}'", _c4prc(first));
            if(first == '\'')
            {
                _c4dbgp("usty[UNK]: scanning single-quoted scalar");
                sc = _scan_scalar_squot();
                if(!_maybe_scan_following_colon())
                {
                    _c4dbgp("usty[UNK]: set as val");
                    _handle_annotations_before_blck_val_scalar();
                    csubstr maybe_filtered = _maybe_filter_val_scalar_squot(sc);
                    m_evt_handler->set_val_scalar_squoted(maybe_filtered);
                    _end_stream();
                }
                else
                {
                    _c4dbgp("usty[UNK]: start new block map, set scalar as key");
                    add_flags(RNXT);
                    _handle_annotations_before_start_mapblck(startline);
                    m_evt_handler->begin_map_val_block();
                    _handle_annotations_and_indentation_after_start_mapblck(startindent, startline);
                    csubstr maybe_filtered = _maybe_filter_key_scalar_squot(sc);
                    m_evt_handler->set_key_scalar_squoted(maybe_filtered);
                    _set_indentation(startindent);
                    addrem_flags(RMAP|RBLCK|RVAL, RNXT|USTY);
                    _maybe_skip_whitespace_tokens();
                }
            }
            else if(first == '"')
            {
                _c4dbgp("usty[UNK]: scanning double-quoted scalar");
                sc = _scan_scalar_dquot();
                if(!_maybe_scan_following_colon())
                {
                    _c4dbgp("usty[UNK]: set as val");
                    _handle_annotations_before_blck_val_scalar();
                    csubstr maybe_filtered = _maybe_filter_val_scalar_dquot(sc);
                    m_evt_handler->set_val_scalar_dquoted(maybe_filtered);
                    _end_stream();
                }
                else
                {
                    _c4dbgp("usty[UNK]: start new block map, set double-quoted scalar as key");
                    add_flags(RNXT);
                    _handle_annotations_before_start_mapblck(startline);
                    m_evt_handler->begin_map_val_block();
                    _handle_annotations_and_indentation_after_start_mapblck(startindent, startline);
                    csubstr maybe_filtered = _maybe_filter_key_scalar_dquot(sc);
                    m_evt_handler->set_key_scalar_dquoted(maybe_filtered);
                    _set_indentation(startindent);
                    addrem_flags(RMAP|RBLCK|RVAL, RNXT|USTY);
                    _maybe_skip_whitespace_tokens();
                }
            }
            else if(first == '|')
            {
                _c4dbgp("usty[UNK]: scanning block-literal scalar");
                ScannedBlock sb;
                _scan_block(&sb, startindent);
                _c4dbgp("usty[UNK]: set as val");
                _handle_annotations_before_blck_val_scalar();
                csubstr maybe_filtered = _maybe_filter_val_scalar_literal(sb);
                m_evt_handler->set_val_scalar_literal(maybe_filtered);
                _end_stream();
            }
            else if(first == '>')
            {
                _c4dbgp("usty[UNK]: scanning block-folded scalar");
                ScannedBlock sb;
                _scan_block(&sb, startindent);
                _c4dbgp("usty[UNK]: set as val");
                _handle_annotations_before_blck_val_scalar();
                csubstr maybe_filtered = _maybe_filter_val_scalar_folded(sb);
                m_evt_handler->set_val_scalar_folded(maybe_filtered);
                _end_stream();
            }
            else if(_scan_scalar_plain_unk(&sc))
            {
                _c4dbgp("usty[UNK]: got a plain scalar");
                if(!_maybe_scan_following_colon())
                {
                    _c4dbgp("usty[UNK]: set as val");
                    _handle_annotations_before_blck_val_scalar();
                    csubstr maybe_filtered = _maybe_filter_val_scalar_plain(sc, startindent);
                    m_evt_handler->set_val_scalar_plain(maybe_filtered);
                    _end_stream();
                }
                else
                {
                    _c4dbgp("usty[UNK]: start new block map, set scalar as key");
                    add_flags(RNXT);
                    _handle_annotations_before_start_mapblck(startline);
                    m_evt_handler->begin_map_val_block();
                    _handle_annotations_and_indentation_after_start_mapblck(startindent, startline);
                    csubstr maybe_filtered = _maybe_filter_key_scalar_plain(sc, startindent);
                    m_evt_handler->set_key_scalar_plain(maybe_filtered);
                    _set_indentation(startindent);
                    addrem_flags(RMAP|RBLCK|RVAL, RNXT|USTY);
                    _maybe_skip_whitespace_tokens();
                }
            }
            else
            {
                _c4err("parse error"); // LCOV_EXCL_LINE
            }
        }
    }
}


//-----------------------------------------------------------------------------

template<class EventHandler>
void ParseEngine<EventHandler>::parse_json_in_place_ev(csubstr filename, substr src)
{
    RYML_ASSERT_BASIC_CB_(m_evt_handler->m_stack.m_callbacks, m_evt_handler->m_stack.size() >= 1);
    RYML_SAVE_TEST_JSON_(filename, src);
    m_evt_handler->start_parse(filename.str, src);
    m_evt_handler->begin_stream();
    _reset();
    while( ! _finished_file())
    {
        _scan_line();
        while( ! _finished_line())
        {
            _c4dbgnextline();
            RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks,  ! m_evt_handler->m_curr->line_contents.rem.empty(), m_evt_handler->m_curr->pos);
            if(has_any(RSEQ))
            {
                _handle_seq_json();
            }
            else if(has_any(RMAP))
            {
                _handle_map_json();
            }
            else if(has_any(RUNK))
            {
                _handle_unk_json();
            }
            else
            {
                _c4err("internal error"); // LCOV_EXCL_LINE
            }
        }
        if(_finished_file())
            break; // it may have finished because of multiline blocks
        _line_ended();
    }
    _end_stream();
    m_evt_handler->finish_parse();
}


//-----------------------------------------------------------------------------

template<class EventHandler>
void ParseEngine<EventHandler>::parse_in_place_ev(csubstr filename, substr src)
{
    RYML_ASSERT_BASIC_CB_(m_evt_handler->m_stack.m_callbacks, m_evt_handler->m_stack.size() >= 1);
    RYML_SAVE_TEST_YAML_(filename, src);
    m_evt_handler->start_parse(filename.str, src);
    m_evt_handler->begin_stream();
    _reset();
    while( ! _finished_file())
    {
        _scan_line();
        while( ! _finished_line())
        {
            _c4dbgnextline();
            RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks,  ! m_evt_handler->m_curr->line_contents.rem.empty(), m_evt_handler->m_curr->pos);
            if(has_any(RFLOW))
            {
                if(has_none(RSEQIMAP))
                {
                    if(has_any(RSEQ))
                    {
                        _handle_seq_flow();
                    }
                    else
                    {
                        RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_all(RMAP), m_evt_handler->m_curr->pos);
                        _handle_map_flow();
                    }
                }
                else
                {
                    _handle_seq_imap();
                }
            }
            else if(has_any(RBLCK))
            {
                if(has_any(RSEQ))
                {
                    _handle_seq_block();
                }
                else
                {
                    RYML_ASSERT_PARSE_CB_(m_evt_handler->m_stack.m_callbacks, has_all(RMAP), m_evt_handler->m_curr->pos);
                    _handle_map_block();
                }
            }
            else if(has_any(RUNK))
            {
                _handle_unk();
            }
            else if(has_any(USTY))
            {
                _handle_usty();
            }
            else
            {
                _c4err("internal error"); // LCOV_EXCL_LINE
            }
        }
        if(_finished_file())
            break; // it may have finished because of multiline blocks
        _line_ended();
    }
    _end_stream();
    m_evt_handler->finish_parse();
}
/** @endcond */

} // namespace yml
} // namespace c4

// NOLINTEND(hicpp-signed-bitwise,cppcoreguidelines-avoid-goto,hicpp-avoid-goto,hicpp-multiway-paths-covered,modernize-avoid-c-style-cast)

#undef _c4dbgnextline
#undef _c4assert
#undef _c4err

C4_SUPPRESS_WARNING_MSVC_POP
C4_SUPPRESS_WARNING_GCC_CLANG_POP

#endif // C4_YML_PARSE_ENGINE_DEF_HPP_
