#include "c4/yml/tag.hpp"
#include "c4/yml/error.hpp"
#include "c4/yml/detail/dbgprint.hpp"


namespace c4 {
namespace yml {

bool is_custom_tag(csubstr tag)
{
    if((tag.len > 2) && (tag.str[0] == '!'))
    {
        size_t pos = tag.find('!', 1);
        return pos != npos && pos > 1 && tag.str[1] != '<';
    }
    return false;
}

csubstr normalize_tag(csubstr tag)
{
    YamlTag_e t = to_tag(tag);
    if(t != TAG_NONE)
        return from_tag(t);
    if(tag.begins_with("!<"))
        tag = tag.sub(1);
    if(tag.begins_with("<!"))
        return tag;
    return tag;
}

csubstr normalize_tag_long(csubstr tag)
{
    YamlTag_e t = to_tag(tag);
    if(t != TAG_NONE)
        return from_tag_long(t);
    if(tag.begins_with("!<"))
        tag = tag.sub(1);
    if(tag.begins_with("<!"))
        return tag;
    return tag;
}

csubstr normalize_tag_long(csubstr tag, substr output)
{
    csubstr result = normalize_tag_long(tag);
    if(result.begins_with("!!"))
    {
        RYML_CHECK_BASIC_(!output.overlaps(tag));
        tag = tag.sub(2);
        const csubstr pfx = "<tag:yaml.org,2002:";
        const size_t len = pfx.len + tag.len + 1;
        if(len <= output.len)
        {
            memcpy(output.str          , pfx.str, pfx.len);
            memcpy(output.str + pfx.len, tag.str, tag.len);
            output[pfx.len + tag.len] = '>';
            result = output.first(len);
        }
        else
        {
            result.str = nullptr;
            result.len = len;
        }
    }
    return result;
}

YamlTag_e to_tag(csubstr tag)
{
    if(tag.begins_with("!<"))
        tag = tag.sub(1);
    if(tag.begins_with("!!"))
    {
        tag = tag.sub(2);
    }
    else if(tag.begins_with('!'))
    {
        return TAG_NONE;
    }
    else
    {
        csubstr pfx = "<tag:yaml.org,2002:";
        csubstr pfx2 = pfx.sub(1);
        if(tag.begins_with(pfx2))
        {
            tag = tag.sub(pfx2.len);
        }
        else if(tag.begins_with(pfx))
        {
            tag = tag.sub(pfx.len);
            if(!tag.len)
                return TAG_NONE;
            tag = tag.offs(0, 1);
        }
    }
    if(tag == "map")
        return TAG_MAP;
    else if(tag == "omap")
        return TAG_OMAP;
    else if(tag == "pairs")
        return TAG_PAIRS;
    else if(tag == "set")
        return TAG_SET;
    else if(tag == "seq")
        return TAG_SEQ;
    else if(tag == "binary")
        return TAG_BINARY;
    else if(tag == "bool")
        return TAG_BOOL;
    else if(tag == "float")
        return TAG_FLOAT;
    else if(tag == "int")
        return TAG_INT;
    else if(tag == "merge")
        return TAG_MERGE;
    else if(tag == "null")
        return TAG_NULL;
    else if(tag == "str")
        return TAG_STR;
    else if(tag == "timestamp")
        return TAG_TIMESTAMP;
    else if(tag == "value")
        return TAG_VALUE;
    else if(tag == "yaml")
        return TAG_YAML;

    return TAG_NONE;
}

csubstr from_tag_long(YamlTag_e tag)
{
    switch(tag)
    {
    case TAG_MAP:
        return {"<tag:yaml.org,2002:map>"};
    case TAG_OMAP:
        return {"<tag:yaml.org,2002:omap>"};
    case TAG_PAIRS:
        return {"<tag:yaml.org,2002:pairs>"};
    case TAG_SET:
        return {"<tag:yaml.org,2002:set>"};
    case TAG_SEQ:
        return {"<tag:yaml.org,2002:seq>"};
    case TAG_BINARY:
        return {"<tag:yaml.org,2002:binary>"};
    case TAG_BOOL:
        return {"<tag:yaml.org,2002:bool>"};
    case TAG_FLOAT:
        return {"<tag:yaml.org,2002:float>"};
    case TAG_INT:
        return {"<tag:yaml.org,2002:int>"};
    case TAG_MERGE:
        return {"<tag:yaml.org,2002:merge>"};
    case TAG_NULL:
        return {"<tag:yaml.org,2002:null>"};
    case TAG_STR:
        return {"<tag:yaml.org,2002:str>"};
    case TAG_TIMESTAMP:
        return {"<tag:yaml.org,2002:timestamp>"};
    case TAG_VALUE:
        return {"<tag:yaml.org,2002:value>"};
    case TAG_YAML:
        return {"<tag:yaml.org,2002:yaml>"};
    case TAG_NONE:
    default:
        return {""};
    }
}

csubstr from_tag(YamlTag_e tag)
{
    switch(tag)
    {
    case TAG_MAP:
        return {"!!map"};
    case TAG_OMAP:
        return {"!!omap"};
    case TAG_PAIRS:
        return {"!!pairs"};
    case TAG_SET:
        return {"!!set"};
    case TAG_SEQ:
        return {"!!seq"};
    case TAG_BINARY:
        return {"!!binary"};
    case TAG_BOOL:
        return {"!!bool"};
    case TAG_FLOAT:
        return {"!!float"};
    case TAG_INT:
        return {"!!int"};
    case TAG_MERGE:
        return {"!!merge"};
    case TAG_NULL:
        return {"!!null"};
    case TAG_STR:
        return {"!!str"};
    case TAG_TIMESTAMP:
        return {"!!timestamp"};
    case TAG_VALUE:
        return {"!!value"};
    case TAG_YAML:
        return {"!!yaml"};
    case TAG_NONE:
    default:
        return {""};
    }
}

bool is_valid_tag_handle(csubstr handle)
{
    if(handle.begins_with('!') && handle.ends_with('!'))
    {
        _c4dbgpf("handle={}", prs_(handle, true));
        csubstr trimmed = handle.sub(1);
        if(trimmed.ends_with('!'))
            trimmed = trimmed.offs(0, 1);
        _c4dbgpf("handle_trimmed={}", prs_(trimmed, true));
        // https://yaml.org/spec/1.2.2/#rule-ns-word-char
        for(char c : trimmed)
        {
            bool ok = (c >= '0' && c <= '9')
                || (c >= 'a' && c <= 'z')
                || (c >= 'A' && c <= 'Z')
                || c == '-';
            if(!ok)
            {
                _c4dbgpf("invalid handle character: '{}'", _c4prc(c));
                return false;
            }
        }
        return true;
    }
    return false;
}

namespace {
bool is_valid_tag_char(char c)
{
    // https://yaml.org/spec/1.2.2/#691-node-tags
    bool ok = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
    if(!ok)
    {
        switch(c)
        {
        case '-':
        case '#':
        case ';':
        case '/':
        case '?':
        case ':':
        case '@':
        case '&':
        case '=':
        case '+':
        case '$':
        case '_':
        case '.':
        case '~':
        case '*':
        case '\'':
        case '(':
        case ')':
        case '%':
            break;
        default:
            return false;
        }
    }
    return true;
}
bool read_hex_char(csubstr suffix, size_t pos, char *out)
{
    // must be succeeded by 2 hex digits
    if(pos + 3 > suffix.len)
        return false;
    suffix = suffix.range(pos + 1, pos + 3);
    uint8_t val = 0;
    if C4_UNLIKELY(!read_hex(suffix, &val) || val > 127)
        return false;
    *out = static_cast<char>(val);
    return true;
}
} // namespace


size_t transform_tag(substr output, csubstr handle, csubstr prefix, csubstr tag,
                     Callbacks const& callbacks, Location const& ymlloc,
                     bool with_brackets)
{
    RYML_ASSERT_BASIC_CB_(callbacks, tag.len >= handle.len);
    RYML_ASSERT_BASIC_CB_(callbacks, !output.overlaps(tag));
    RYML_ASSERT_BASIC_CB_(callbacks, prefix.len > 0);
    csubstr rest = tag.sub(handle.len);
    _c4dbgpf("%TAG: rest={}", prs_(rest));
    size_t rpos = 0, wpos = 0;
    auto appendstr = [&](csubstr s) {
        if(s.len && wpos + s.len <= output.len)
            memcpy(output.str + wpos, s.str, s.len);
        wpos += s.len;
    };
    auto appendchar = [&](char c) {
        if(wpos < output.len)
            output.str[wpos] = c;
        ++wpos;
    };
    if(with_brackets)
        appendchar('<');
    appendstr(prefix);
    const char *errmsg = nullptr;
    for(size_t pos = 0; pos < rest.len; ++pos)
    {
        char c = rest.str[pos];
        if C4_LIKELY(is_valid_tag_char(c))
        {
            if(c != '%')
            {
                continue;
            }
            else if(read_hex_char(rest, pos, &c))
            {
                appendstr(rest.range(rpos, pos));
                appendchar(c);
                pos += 2;
                rpos = pos + 1;
                continue;
            }
        }
        errmsg = "invalid tag";
        goto err; // NOLINT
    }
    appendstr(rest.sub(rpos));
    if(with_brackets)
        appendchar('>');
    return wpos;
err:
    if(ymlloc)
    {
        RYML_ERR_PARSE_CB_(callbacks, ymlloc, errmsg);
    }
    else
    {
        RYML_ERR_BASIC_CB_(callbacks, errmsg);
    }
}


//-----------------------------------------------------------------------------

id_type TagDirectives::size() const noexcept
{
    // this assumes we have a very small number of tag directives
    id_type i = 0;
    for(; i < RYML_MAX_TAG_DIRECTIVES; ++i)
        if(m_directives[i].handle.empty())
            break;
    return i;
}

TagDirective const* TagDirectives::add(csubstr handle, csubstr prefix, id_type doc_id) noexcept
{
    id_type pos = size();
    TagDirective *C4_RESTRICT td = nullptr;
    if(pos < RYML_MAX_TAG_DIRECTIVES)
    {
        td = &m_directives[pos];
        td->handle = handle;
        td->prefix = prefix;
        td->doc_id = doc_id;
        _c4dbgpf("tagd[{}]: added! handle={} prefix={} doc={}", pos, td->handle, td->prefix, td->doc_id);
    }
    return td;
}

void TagDirectives::clear() noexcept
{
    for(TagDirective &td : m_directives)
    {
        td.handle = {};
        td.prefix = {};
        td.doc_id = NONE;
    }
}

TagDirectiveRange TagDirectives::lookup_range(id_type doc_id) const noexcept
{
    TagDirective const* first = nullptr;
    TagDirective const* last = nullptr;
    for(id_type i = 0; i < RYML_MAX_TAG_DIRECTIVES; ++i)
    {
        TagDirective const& C4_RESTRICT td = m_directives[i];
        if(doc_id == td.doc_id)
        {
            first = m_directives + i;
            break;
        }
        else if(td.handle.empty())
        {
            break;
        }
    }
    if(first)
    {
        last = m_directives + RYML_MAX_TAG_DIRECTIVES;
        for(TagDirective const* C4_RESTRICT td = first; td < last; ++td)
        {
            if(doc_id != td->doc_id || td->handle.empty())
            {
                last = td;
                break;
            }
        }
    }
    else
    {
        first = last = m_directives;
    }
    return TagDirectiveRange{first, last};
}

TagDirective const* TagDirectives::lookup(csubstr tag, id_type doc_id) const noexcept
{
    _c4dbgpf("tagd: searching for {}, doc_id={}", prs_(tag), doc_id);
    for(id_type i = 0; i < RYML_MAX_TAG_DIRECTIVES; ++i)
    {
        TagDirective const& C4_RESTRICT td = m_directives[i];
        if(td.handle.empty())
        {
            continue;
        }
        _c4dbgpf("tagd[{}]: handle={} prefix={} doc_id={}", i, td.handle, td.prefix, td.doc_id);
        if(tag.begins_with(td.handle))
        {
            if(td.handle == '!' && (
                   tag.begins_with("!!")
                   || tag.begins_with('<')
                   || tag.begins_with("!<")
                   || is_custom_tag(tag)))
                continue;
            _c4dbgpf("tagd[{}]: matches handle!", i);
            if(doc_id == td.doc_id)
            {
                _c4dbgpf("tagd[{}]: matches doc={}!", i, doc_id);
                return &td;
            }
        }
    }
    return nullptr;
}

csubstr TagDirectives::resolve(substr buf, size_t *bufsz, csubstr tag, id_type id, Location const& ymlloc, Callbacks const& callbacks, bool with_brackets) const
{
    RYML_ASSERT_BASIC_CB_(callbacks, !buf.overlaps(tag));
    TagDirective const* C4_RESTRICT td = lookup(tag, id);
    *bufsz = 0;
    csubstr handle, prefix, ret;
    const char *errmsg = nullptr;
    size_t len;
    if(td)
    {
        handle = td->handle;
        prefix = td->prefix;
    }
    else
    {
        _c4dbgp("tagd: no directive found");
        if(tag.begins_with('<'))
        {
            _c4dbgp("tagd: already resolved");
            if C4_UNLIKELY(!tag.ends_with('>'))
            {
                errmsg = "malformed tag";
                goto err; // NOLINT
            }
            return tag;
        }
        else if(tag.begins_with("!<"))
        {
            _c4dbgp("tagd: already resolved");
            if C4_UNLIKELY(!tag.ends_with('>'))
            {
                errmsg = "malformed tag";
                goto err; // NOLINT
            }
            return tag.sub(1);
        }
        else if(tag.begins_with("!!"))
        {
            _c4dbgp("tagd: !!");
            YamlTag_e tagenum = to_tag(tag);
            if(tagenum != TAG_NONE)
            {
                _c4dbgpf("tagd: standard tag: {} -> {}", tag, from_tag_long(tagenum));
                tag = from_tag_long(tagenum);
                return with_brackets ? tag : tag.offs(1, 1);
            }
            handle = "!!";
            prefix = "tag:yaml.org,2002:";
        }
        else if C4_UNLIKELY(is_custom_tag(tag))
        {
            _c4dbgp("tagd: custom_tag");
            _c4dbgpf("tag '{}' at id={}: no matching directive was found", tag, id);
            errmsg = "tag without matching directive";
            goto err; // NOLINT
        }
        else
        {
            _c4dbgp("tagd: !");
            handle = prefix = "!";
        }
    }
    len = transform_tag(buf, handle, prefix, tag, callbacks, ymlloc, with_brackets);
    *bufsz = len;
    if(len <= buf.len)
    {
        ret = buf.first(len);
    }
    else
    {
        _c4dbgp("tagd: not enough room");
        ret.str = nullptr;
        ret.len = len;
    }
    return ret;
err:
    if(ymlloc)
    {
        RYML_ERR_PARSE_CB_(callbacks, ymlloc, errmsg);
    }
    else
    {
        RYML_ERR_BASIC_CB_(callbacks, errmsg);
    }
}


//-----------------------------------------------------------------------------
TagCache::LookupResult TagCache::find(csubstr tag, id_type doc_id, id_type linear_threshold) const noexcept
{
    LookupResult ret = {};
    id_type sz = m_entries.size();
    if(sz < linear_threshold) // do a linear search on small size
    {
        for(size_t i = 0; i < sz; ++i)
        {
            Entry const& C4_RESTRICT e = m_entries[i];
            if(e.tag == tag && e.doc_id == doc_id)
            {
                ret.resolved = e.resolved;
                ret.pos = i;
                return ret;
            }
            else if(e.tag > tag || ((e.tag == tag) && e.doc_id > doc_id))
            {
                ret.pos = i;
                return ret;
            }
        }
        ret.pos = sz;
    }
    else // do a binary search on larger size
    {
        id_type first = 0;
        id_type count = sz;
        while(count)
        {
            id_type halfsz = count / id_type(2); // NOLINT(*avoid-c-style-cast)
            id_type mid = first + halfsz;
            RYML_ASSERT_BASIC_CB_(m_entries.m_callbacks, mid < sz);
            Entry const& C4_RESTRICT e = m_entries[mid];
            if(e.tag < tag || (e.tag == tag && e.doc_id < doc_id))
            {
                first = mid + 1;
                RYML_ASSERT_BASIC_CB_(m_entries.m_callbacks, count >= halfsz + 1);
                count -= halfsz + 1;
            }
            else
            {
                count = halfsz;
            }
        }
        ret.pos = first;
        if(first < sz)
        {
            Entry const& C4_RESTRICT e = m_entries[first];
            if(e.tag == tag && e.doc_id == doc_id)
            {
                ret.resolved = m_entries[first].resolved;
            }
        }
    }
    return ret;
}

void TagCache::add(csubstr tag, csubstr resolved, id_type doc_id, const_iterator pos) RYML_NOEXCEPT
{
    const id_type sz = m_entries.size();
    RYML_ASSERT_BASIC_CB_(m_entries.m_callbacks, pos <= sz);
    RYML_ASSERT_BASIC_CB_(m_entries.m_callbacks, pos == sz || tag < m_entries[pos].tag || (tag == m_entries[pos].tag && doc_id < m_entries[pos].doc_id));
    m_entries.resize(sz + 1);
    if(pos < sz)
        memmove(m_entries.m_stack + pos + 1, m_entries.m_stack + pos, (sz - pos) * sizeof(Entry));
    m_entries.m_stack[pos].tag = tag;
    m_entries.m_stack[pos].resolved = resolved;
    m_entries.m_stack[pos].doc_id = doc_id;
    _c4dbgpf("tagcache: add entry @pos={}:  docid={}  {} -> {}", pos, doc_id, tag, maybe_null_str_(resolved));
}

} // namespace yml
} // namespace c4
