#ifndef C4_YML_SCALAR_STYLE_HPP_
#include "c4/yml/scalar_style.hpp"
#endif
#ifndef C4_YML_SCALAR_CHARCONV_HPP_
#include "c4/yml/scalar_charconv.hpp"
#endif
#ifndef C4_YML_ERROR_HPP_
#include "c4/yml/error.hpp"
#endif


namespace c4 {
namespace yml {

bool scalar_style_query_squo(csubstr scalar) noexcept
{
    // see https://www.yaml.info/learn/quote.html#noplain
    // cannot have leading whitespace after a newline
    for(size_t i = 0; i < scalar.len; ++i)
    {
        if(scalar.str[i] == '\n' && i + 1 < scalar.len)
        {
            char next = scalar.str[i + 1];
            if(next == ' ' || next == '\t')
                return false;
        }
    }
    return true;
}


namespace {
bool is_wsnl_(char c) noexcept
{
    return c == ' ' || c == '\n' || c == '\t' || c == '\r';
}
bool is_valid_bulk_(csubstr s, size_t i)
{
    C4_ASSERT(i >= 1 && i+1 < s.len);
    C4_ASSERT(s.str[i] == ':' || s.str[i] == '#');
    switch(s.str[i])
    {
    case ':': return !is_wsnl_(s.str[i+1]);
    case '#': return !is_wsnl_(s.str[i-1]);
    }
    C4_UNREACHABLE(); // LCOV_EXCL_LINE
}
} // namespace


bool scalar_style_query_plain_flow(csubstr scalar) noexcept
{
    // see https://www.yaml.info/learn/quote.html#noplain
    if(!scalar.len)
        return !scalar.str;
    // first
    switch(scalar.str[0])
    {
    case ' ': case '\n': case '\t': case '\r':
    case '!': case '&': case '*': case ',':
    case '"': case '\'': case '|': case '>':
    case '{': case '}': case '[': case ']':
    case '#': case '`': case '%': case '@':
        return false;
    case '-': case ':': case '?':
        if(scalar.len == 1 || (scalar.str[1] == ' ' || scalar.str[1] == '\t'))
            return false;
        break;
    }
    // bulk
    for(size_t i = 1; i + 1 < scalar.len; ++i)
    {
        switch(scalar.str[i])
        {
        case ',': case '{': case '}': case '[': case ']':
            return false;
        case ':': case '#':
            if(!is_valid_bulk_(scalar, i))
                return false;
            break;
        }
    }
    // last
    if(scalar.len > 1)
    {
        switch(scalar.back())
        {
        case ' ': case '\n': case '\t': case '\r':
        case ',':
        case '{': case '}':
        case '[': case ']':
        case '#':
        case ':':
            return false;
        }
    }
    return true;
}


bool scalar_style_query_plain_block(csubstr scalar) noexcept
{
    // see https://www.yaml.info/learn/quote.html#noplain
    if(!scalar.len)
        return !scalar.str;
    // first
    switch(scalar.str[0])
    {
    case ' ': case '\n': case '\t': case '\r':
    case '!': case '&': case '*': case ',':
    case '"': case '\'': case '|': case '>':
    case '{': case '}': case '[': case ']':
    case '#': case '`': case '%': case '@':
        return false;
    case '-': case ':': case '?':
        if (scalar.len == 1 || (scalar.str[1] == ' ' || scalar.str[1] == '\t'))
            return false;
        break;
    }
    // bulk
    for(size_t i = 1; i + 1 < scalar.len; ++i)
    {
        switch(scalar.str[i])
        {
        case ':': case '#':
            if(!is_valid_bulk_(scalar, i))
                return false;
            break;
        }
    }
    // last
    if(scalar.len > 1)
    {
        switch(scalar.back())
        {
        case ' ': case '\n': case '\t': case '\r':
        case '#':
        case ':':
            return false;
        }
    }
    return true;
}


NodeType scalar_style_choose_block(csubstr scalar) noexcept
{
    if(scalar.len)
    {
        if(scalar_style_query_plain_block(scalar))
            return SCALAR_PLAIN;
        RYML_ASSERT_BASIC_(scalar_style_query_squo(scalar)
                           && "if this assertion fires, please submit an issue!");
        return SCALAR_SQUO;
    }
    return scalar.str ? SCALAR_SQUO : SCALAR_PLAIN;
}


NodeType scalar_style_choose_json(csubstr scalar) noexcept
{
    // do not quote numbers or special scalars
    return scalar_is_plain_number_json(scalar)
        || scalar_is_special_json(scalar) ? SCALAR_PLAIN : SCALAR_DQUO;
}

} // namespace yml
} // namespace c4
