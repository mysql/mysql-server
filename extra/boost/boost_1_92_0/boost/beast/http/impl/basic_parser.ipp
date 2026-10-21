//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_HTTP_IMPL_BASIC_PARSER_IPP
#define BOOST_BEAST_HTTP_IMPL_BASIC_PARSER_IPP

#include <boost/beast/http/basic_parser.hpp>
#include <boost/beast/http/error.hpp>
#include <boost/beast/http/rfc7230.hpp>
#include <boost/beast/core/buffer_traits.hpp>
#include <boost/beast/core/detail/clamp.hpp>
#include <boost/beast/core/detail/config.hpp>
#include <boost/beast/core/detail/string.hpp>
#include <boost/asio/buffer.hpp>
#include <algorithm>
#include <utility>

namespace boost {
namespace beast {
namespace http {

template<bool isRequest>
bool
basic_parser<isRequest>::
keep_alive() const
{
    BOOST_ASSERT(is_header_done());
    if(f_ & flagHTTP11)
    {
        if(f_ & flagConnectionClose)
            return false;
    }
    else
    {
        if(! (f_ & flagConnectionKeepAlive))
            return false;
    }
    return (f_ & flagNeedEOF) == 0;
}

template<bool isRequest>
boost::optional<std::uint64_t>
basic_parser<isRequest>::
content_length() const
{
    BOOST_ASSERT(is_header_done());
    return content_length_unchecked();
}

template<bool isRequest>
boost::optional<std::uint64_t>
basic_parser<isRequest>::
content_length_remaining() const
{
    BOOST_ASSERT(is_header_done());
    if(! (f_ & flagContentLength))
        return boost::none;
    return len_;
}

template<bool isRequest>
void
basic_parser<isRequest>::
skip(bool v)
{
    BOOST_ASSERT(! got_some());
    if(v)
        f_ |= flagSkipBody;
    else
        f_ &= ~flagSkipBody;
}

template<bool isRequest>
std::size_t
basic_parser<isRequest>::
put(net::const_buffer buffer,
    error_code& ec)
{
    // If this goes off you have tried to parse more data after the parser
    // has completed. A common cause of this is re-using a parser, which is
    // not supported. If you need to re-use a parser, consider storing it
    // in an optional. Then reset() and emplace() prior to parsing each new
    // message.
    BOOST_ASSERT(!is_done());
    if (is_done())
    {
        BOOST_BEAST_ASSIGN_EC(ec, error::stale_parser);
        return 0;
    }
    auto p = static_cast<char const*>(buffer.data());
    auto n = buffer.size();
    auto const p0 = p;
    auto const p1 = p0 + n;
    ec = {};
loop:
    switch(state_)
    {
    case state::nothing_yet:
        if(n == 0)
        {
            BOOST_BEAST_ASSIGN_EC(ec, error::need_more);
            return 0;
        }
        state_ = state::start_line;
        BOOST_FALLTHROUGH;

    case state::start_line:
        parse_start_line(p, n, ec);
        if(ec)
            goto done;
        BOOST_ASSERT(! is_done());
        n = static_cast<std::size_t>(p1 - p);
        BOOST_FALLTHROUGH;

    case state::fields:
        parse_fields(p, n, ec);
        if(ec)
            goto done;
        finish_header(ec, is_request{});
        if(ec)
            goto done;
        break;

    case state::body0:
        this->on_body_init_impl(content_length(), ec);
        if(ec)
            goto done;
        state_ = state::body;
        BOOST_FALLTHROUGH;

    case state::body:
        parse_body(p, n, ec);
        if(ec)
            goto done;
        break;

    case state::body_to_eof0:
        this->on_body_init_impl(content_length(), ec);
        if(ec)
            goto done;
        state_ = state::body_to_eof;
        BOOST_FALLTHROUGH;

    case state::body_to_eof:
        parse_body_to_eof(p, n, ec);
        if(ec)
            goto done;
        break;

    case state::chunk_header0:
        this->on_body_init_impl(content_length(), ec);
        if(ec)
            goto done;
        state_ = state::chunk_header;
        BOOST_FALLTHROUGH;

    case state::chunk_header:
        parse_chunk_header(p, n, ec);
        if(ec)
            goto done;
        BOOST_ASSERT(! skip_);
        if(state_ != state::trailer_fields)
            break;
        n = static_cast<std::size_t>(p1 - p);
        BOOST_FALLTHROUGH;

    case state::trailer_fields:
        parse_fields(p, n, ec);
        if(ec)
            goto done;
        state_ = state::complete;
        this->on_finish_impl(ec);
        goto done;

    case state::chunk_body:
        parse_chunk_body(p, n, ec);
        if(ec)
            goto done;
        break;

    case state::complete:
        ec = {};
        goto done;
    }
    if(p < p1 && ! is_done() && eager())
    {
        n = static_cast<std::size_t>(p1 - p);
        goto loop;
    }
done:
    return static_cast<std::size_t>(p - p0);
}

template<bool isRequest>
void
basic_parser<isRequest>::
put_eof(error_code& ec)
{
    BOOST_ASSERT(got_some());
    if( state_ == state::start_line ||
        state_ == state::fields)
    {
        BOOST_BEAST_ASSIGN_EC(ec, error::partial_message);
        return;
    }
    if(f_ & (flagContentLength | flagChunked))
    {
        if(state_ != state::complete)
        {
            BOOST_BEAST_ASSIGN_EC(ec, error::partial_message);
            return;
        }
        ec = {};
        return;
    }
    state_ = state::complete;
    ec = {};
    this->on_finish_impl(ec);
}

template<bool isRequest>
void
basic_parser<isRequest>::
inner_parse_start_line(
    char const*& in, char const* last,
    error_code& ec, std::true_type)
{
/*
    request-line   = method SP request-target SP HTTP-version CRLF
    method         = token
*/
    auto p = in;

    string_view method;
    parse_method(p, last, method, ec);
    if(ec)
        return;

    string_view target;
    parse_target(p, last, target, ec);
    if(ec)
        return;

    int version = 0;
    parse_version(p, last, version, ec);
    if(ec)
        return;
    if(version < 10 || version > 11)
    {
        BOOST_BEAST_ASSIGN_EC(ec, error::bad_version);
        return;
    }

    if(p + 2 > last)
    {
        BOOST_BEAST_ASSIGN_EC(ec, error::need_more);
        return;
    }
    if(p[0] != '\r' || p[1] != '\n')
    {
        BOOST_BEAST_ASSIGN_EC(ec, error::bad_version);
        return;
    }
    p += 2;

    if(version >= 11)
        f_ |= flagHTTP11;

    this->on_request_impl(string_to_verb(method),
        method, target, version, ec);
    if(ec)
        return;

    in = p;
    state_ = state::fields;
}

template<bool isRequest>
void
basic_parser<isRequest>::
inner_parse_start_line(
    char const*& in, char const* last,
    error_code& ec, std::false_type)
{
/*
     status-line    = HTTP-version SP status-code SP reason-phrase CRLF
     status-code    = 3*DIGIT
     reason-phrase  = *( HTAB / SP / VCHAR / obs-text )
*/
    auto p = in;

    int version = 0;
    parse_version(p, last, version, ec);
    if(ec)
        return;
    if(version < 10 || version > 11)
    {
        BOOST_BEAST_ASSIGN_EC(ec, error::bad_version);
        return;
    }

    // SP
    if(p + 1 > last)
    {
        BOOST_BEAST_ASSIGN_EC(ec, error::need_more);
        return;
    }
    if(*p++ != ' ')
    {
        BOOST_BEAST_ASSIGN_EC(ec, error::bad_version);
        return;
    }

    parse_status(p, last, status_, ec);
    if(ec)
        return;

    // parse reason CRLF
    string_view reason;
    parse_reason(p, last, reason, ec);
    if(ec)
        return;

    if(version >= 11)
        f_ |= flagHTTP11;

    this->on_response_impl(
        status_, reason, version, ec);
    if(ec)
        return;

    in = p;
    state_ = state::fields;
}

template<bool isRequest>
void
basic_parser<isRequest>::
parse_start_line(
    char const*& in, std::size_t n, error_code& ec)
{
    auto const p0 = in;

    inner_parse_start_line(in, in + (std::min<std::size_t>)
        (n, header_limit_), ec, is_request{});
    if(ec == error::need_more && n >= header_limit_)
    {
        BOOST_BEAST_ASSIGN_EC(ec, error::header_limit);
    }
    header_limit_ -= static_cast<std::uint32_t>(in - p0);
}

template<bool isRequest>
void
basic_parser<isRequest>::
inner_parse_fields(char const*& in,
    char const* last, error_code& ec)
{
    string_view name;
    string_view value;
    // https://stackoverflow.com/questions/686217/maximum-on-http-header-values
    beast::detail::char_buffer<max_obs_fold> buf;
    auto p = in;
    for(;;)
    {
        if(p + 2 > last)
        {
            BOOST_BEAST_ASSIGN_EC(ec, error::need_more);
            return;
        }
        if(p[0] == '\r')
        {
            if(p[1] != '\n')
            {
                BOOST_BEAST_ASSIGN_EC(ec, error::bad_line_ending);
            }
            in = p + 2;
            return;
        }
        parse_field(p, last, name, value, buf, ec);
        if(ec)
            return;
        auto const f = string_to_field(name);
        if(BOOST_UNLIKELY(state_ == state::trailer_fields))
        {
            // do_field() applies header-section semantics
            // (Content-Length, Transfer-Encoding, Connection,
            // Upgrade) which must not be honored when they appear
            // in a chunked trailer, see rfc7230 section 4.1.2.
            this->on_trailer_field_impl(f, name, value, ec);
        }
        else
        {
            do_field(f, value, ec);
            if(ec)
                return;
            this->on_field_impl(f, name, value, ec);
        }
        if(ec)
            return;
        in = p;
    }
}

template<bool isRequest>
void
basic_parser<isRequest>::
parse_fields(char const*& in, std::size_t n, error_code& ec)
{
    auto const p0 = in;

    inner_parse_fields(in, in + (std::min<std::size_t>)
        (n, header_limit_), ec);
    if(ec == error::need_more && n >= header_limit_)
    {
        BOOST_BEAST_ASSIGN_EC(ec, error::header_limit);
    }
    header_limit_ -= static_cast<std::uint32_t>(in - p0);
}

template<bool isRequest>
void
basic_parser<isRequest>::
finish_header(error_code& ec, std::true_type)
{
    // RFC 7230 section 3.3
    // https://tools.ietf.org/html/rfc7230#section-3.3

    // chunked is an HTTP/1.1 transfer coding; an HTTP/1.0 request that
    // resolves to chunked framing cannot be delimited reliably by a
    // recipient that ignores Transfer-Encoding per HTTP/1.0, so reject
    // it here instead of consuming the body as chunks.
    if((f_ & flagChunked) && ! (f_ & flagHTTP11))
    {
        BOOST_BEAST_ASSIGN_EC(ec, error::bad_transfer_encoding);
        return;
    }

    // if a Transfer-Encoding is present in a request and chunked is not the
    // final coding, the body length cannot be determined reliably; reject the
    // request instead of silently treating it as having no body.
    if((f_ & flagTransferEncoding) && ! (f_ & flagChunked))
    {
        BOOST_BEAST_ASSIGN_EC(ec, error::bad_transfer_encoding);
        return;
    }

    if(f_ & flagSkipBody)
    {
        state_ = state::complete;
    }
    else if(f_ & flagContentLength)
    {
        if(body_limit_.has_value() &&
           len_ > body_limit_)
        {
            BOOST_BEAST_ASSIGN_EC(ec, error::body_limit);
            return;
        }
        if(len_ > 0)
        {
            f_ |= flagHasBody;
            state_ = state::body0;
        }
        else
        {
            state_ = state::complete;
        }
    }
    else if(f_ & flagChunked)
    {
        f_ |= flagHasBody;
        state_ = state::chunk_header0;
    }
    else
    {
        len_ = 0;
        len0_ = 0;
        state_ = state::complete;
    }

    ec = {};
    this->on_header_impl(ec);
    if(ec)
        return;
    if(state_ == state::complete)
        this->on_finish_impl(ec);
}

template<bool isRequest>
void
basic_parser<isRequest>::
finish_header(error_code& ec, std::false_type)
{
    // RFC 7230 section 3.3
    // https://tools.ietf.org/html/rfc7230#section-3.3

    if( (f_ & flagSkipBody) ||  // e.g. response to a HEAD request
        status_  / 100 == 1 ||   // 1xx e.g. Continue
        status_ == 204 ||        // No Content
        status_ == 304)          // Not Modified
    {
        // VFALCO Content-Length may be present, but we
        //        treat the message as not having a body.
        //        https://github.com/boostorg/beast/issues/692
        state_ = state::complete;
    }
    else if(f_ & flagContentLength)
    {
        if(len_ > 0)
        {
            f_ |= flagHasBody;
            state_ = state::body0;

            if(body_limit_.has_value() &&
               len_ > body_limit_)
            {
                BOOST_BEAST_ASSIGN_EC(ec, error::body_limit);
                return;
            }
        }
        else
        {
            state_ = state::complete;
        }
    }
    else if(f_ & flagChunked)
    {
        f_ |= flagHasBody;
        state_ = state::chunk_header0;
    }
    else
    {
        f_ |= flagHasBody;
        f_ |= flagNeedEOF;
        state_ = state::body_to_eof0;
    }

    ec = {};
    this->on_header_impl(ec);
    if(ec)
        return;
    if(state_ == state::complete)
        this->on_finish_impl(ec);
}

template<bool isRequest>
void
basic_parser<isRequest>::
parse_body(char const*& p,
    std::size_t n, error_code& ec)
{
    ec = {};
    n = this->on_body_impl(string_view{p,
        beast::detail::clamp(len_, n)}, ec);
    p += n;
    len_ -= n;
    if(ec)
        return;
    if(len_ > 0)
        return;
    state_ = state::complete;
    this->on_finish_impl(ec);
}

template<bool isRequest>
void
basic_parser<isRequest>::
parse_body_to_eof(char const*& p,
    std::size_t n, error_code& ec)
{
    if(body_limit_.has_value())
    {
        if (n > *body_limit_)
        {
            BOOST_BEAST_ASSIGN_EC(ec, error::body_limit);
            return;
        }
        *body_limit_ -= n;
    }
    ec = {};
    n = this->on_body_impl(string_view{p, n}, ec);
    p += n;
    if(ec)
        return;
}

template<bool isRequest>
void
basic_parser<isRequest>::
parse_chunk_header(char const*& in,
    std::size_t n, error_code& ec)
{
/*
    chunked-body   = *chunk last-chunk trailer-part CRLF

    chunk          = chunk-size [ chunk-ext ] CRLF chunk-data CRLF
    last-chunk     = 1*("0") [ chunk-ext ] CRLF
    trailer-part   = *( header-field CRLF )

    chunk-size     = 1*HEXDIG
    chunk-data     = 1*OCTET ; a sequence of chunk-size octets
    chunk-ext      = *( ";" chunk-ext-name [ "=" chunk-ext-val ] )
    chunk-ext-name = token
    chunk-ext-val  = token / quoted-string
*/

    auto p = in;
    auto const pend = p + n;

    if(n < skip_ + 2)
    {
        BOOST_BEAST_ASSIGN_EC(ec, error::need_more);
        return;
    }
    if(f_ & flagExpectCRLF)
    {
        // Treat the last CRLF in a chunk as
        // part of the next chunk, so p can
        // be parsed in one call instead of two.
        if(! parse_crlf(p))
        {
            BOOST_BEAST_ASSIGN_EC(ec, error::bad_chunk);
            return;
        }
    }

    auto const eol = find_eol(p + skip_, pend, ec);
    if(ec)
        return;
    if(! eol)
    {
        if(n + skip_ >= 8192)
        {
            BOOST_BEAST_ASSIGN_EC(ec, error::bad_chunk_extension);
            return;
        }
        BOOST_BEAST_ASSIGN_EC(ec, error::need_more);
        if(p != pend)
            skip_ = pend - p - 1;
        return;
    }
    skip_ = static_cast<std::size_t>(eol - p - 2);

    std::uint64_t size;
    if(! parse_hex(p, size))
    {
        BOOST_BEAST_ASSIGN_EC(ec, error::bad_chunk);
        return;
    }
    if (body_limit_.has_value())
    {
        if (size > *body_limit_)
        {
            BOOST_BEAST_ASSIGN_EC(ec, error::body_limit);
            return;
        }
        *body_limit_ -= size;
    }

    auto const start = p;
    parse_chunk_extensions(p, pend, ec);
    if(ec)
        return;
    if(p != eol - 2)
    {
        BOOST_BEAST_ASSIGN_EC(ec, error::bad_chunk_extension);
        return;
    }
    auto const ext = make_string(start, p);
    this->on_chunk_header_impl(size, ext, ec);
    if(ec)
        return;

    len_ = size;
    skip_ = 0;
    in = eol;
    f_ |= flagExpectCRLF;
    if(size != 0)
    {
        state_ = state::chunk_body;
        return;
    }
    state_ = state::trailer_fields;
    header_limit_ += 2; // for the final chunk's CRLF
}

template<bool isRequest>
void
basic_parser<isRequest>::
parse_chunk_body(char const*& p,
    std::size_t n, error_code& ec)
{
    ec = {};
    n = this->on_chunk_body_impl(
        len_, string_view{p,
            beast::detail::clamp(len_, n)}, ec);
    p += n;
    len_ -= n;
    if(len_ == 0)
        state_ = state::chunk_header;
}

template<bool isRequest>
void
basic_parser<isRequest>::
do_field(field f,
    string_view value, error_code& ec)
{
    using namespace beast::detail::string_literals;
    // Connection
    if(f == field::connection ||
        f == field::proxy_connection)
    {
        auto const list = opt_token_list{value};
        if(! validate_list(list))
        {
            // VFALCO Should this be a field specific error?
            BOOST_BEAST_ASSIGN_EC(ec, error::bad_value);
            return;
        }
        for(auto const& s : list)
        {
            if(beast::iequals("close"_sv, s))
            {
                f_ |= flagConnectionClose;
                continue;
            }

            if(beast::iequals("keep-alive"_sv, s))
            {
                f_ |= flagConnectionKeepAlive;
                continue;
            }

            if(beast::iequals("upgrade"_sv, s))
            {
                f_ |= flagConnectionUpgrade;
                continue;
            }
        }
        ec = {};
        return;
    }

    // Content-Length
    if(f == field::content_length)
    {
        auto bad_content_length = [&ec]
        {
            BOOST_BEAST_ASSIGN_EC(ec, error::bad_content_length);
        };

        auto multiple_content_length = [&ec]
        {
            BOOST_BEAST_ASSIGN_EC(ec, error::multiple_content_length);
        };

        // conflicting field: a Transfer-Encoding was already seen, and a
        // message must not carry both Transfer-Encoding and Content-Length
        // regardless of the transfer coding or the order the two fields
        // arrive in (RFC 7230 3.3.3).
        if(f_ & (flagChunked | flagTransferEncoding))
            return bad_content_length();

        // Content-length may be a comma-separated list of integers
        auto tokens_unprocessed = 1 +
            std::count(value.begin(), value.end(), ',');
        auto tokens = opt_token_list(value);
        if (tokens.begin() == tokens.end() ||
            !validate_list(tokens))
                return bad_content_length();

        auto existing = this->content_length_unchecked();
        for (auto tok : tokens)
        {
            std::uint64_t v;
            if (!parse_dec(tok, v))
                return bad_content_length();
            --tokens_unprocessed;
            if (existing.has_value())
            {
                if (v != *existing)
                    return multiple_content_length();
            }
            else
            {
                existing = v;
            }
        }

        if (tokens_unprocessed)
            return bad_content_length();

        BOOST_ASSERT(existing.has_value());
        ec = {};
        len_ = *existing;
        len0_ = *existing;
        f_ |= flagContentLength;
        return;
    }

    // Transfer-Encoding
    if(f == field::transfer_encoding)
    {
        if(f_ & flagChunked)
        {
            // duplicate
            BOOST_BEAST_ASSIGN_EC(ec, error::bad_transfer_encoding);
            return;
        }

        if(f_ & flagContentLength)
        {
            // conflicting field
            BOOST_BEAST_ASSIGN_EC(ec, error::bad_transfer_encoding);
            return;
        }

        // Record that a Transfer-Encoding was present so a Content-Length
        // arriving afterwards is rejected the same way it would be if the
        // fields arrived in the opposite order.
        f_ |= flagTransferEncoding;

        ec = {};
        auto const v = token_list{value};
        auto const p = std::find_if(v.begin(), v.end(),
            [&](string_view const& s)
            {
                return beast::iequals("chunked"_sv, s);
            });
        if(p == v.end())
            return;
        if(std::next(p) != v.end())
            return;
        len_ = 0;
        f_ |= flagChunked;
        return;
    }

    // Upgrade
    if(f == field::upgrade)
    {
        ec = {};
        f_ |= flagUpgrade;
        return;
    }

    ec = {};
}

#ifdef BOOST_BEAST_SOURCE
template class http::basic_parser<true>;
template class http::basic_parser<false>;
#endif

} // http
} // beast
} // boost

#endif
