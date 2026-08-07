//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/url
//

#ifndef BOOST_URL_DETAIL_ANY_PARAMS_ITER_HPP
#define BOOST_URL_DETAIL_ANY_PARAMS_ITER_HPP

#include <boost/url/param.hpp>
#include <boost/url/pct_string_view.hpp>
#include <boost/core/detail/static_assert.hpp>
#include <cstddef>
#include <iterator>
#include <type_traits>

namespace boost {
namespace urls {
namespace detail {

//------------------------------------------------
//
// any_params_iter
//
//------------------------------------------------

/*  An iterator to a type-erased,
    possibly encoded sequence of
    query params_ref.
*/  
struct BOOST_SYMBOL_VISIBLE
    any_params_iter
{
protected:
    any_params_iter(
        bool empty_,
        core::string_view s0_ = {},
        core::string_view s1_ = {}) noexcept
        : s0(s0_)
        , s1(s1_)
        , empty(empty_)
    {
    }

public:
    // these are adjusted
    // when self-intersecting
    core::string_view s0;
    core::string_view s1;

    // True if the sequence is empty
    bool empty = false;

    virtual
    ~any_params_iter() noexcept = 0;

    // Rewind the iterator to the beginning
    virtual
    void
    rewind() noexcept = 0;

    // Measure and increment current element.
    // Returns false on end of range.
    // n is increased by encoded size.
    // Can throw on bad percent-escape
    virtual
    bool
    measure(std::size_t& n) = 0;

    // Copy and increment the current
    // element. encoding is performed
    // if needed.
    virtual
    void
    copy(
        char*& dest,
        char const* end) noexcept = 0;
};

//------------------------------------------------
//
// query_string_iter
//
//------------------------------------------------

// A string of plain query params
struct BOOST_SYMBOL_VISIBLE
    query_string_iter
    : any_params_iter
{
    // ne = never empty
    explicit
    query_string_iter(
        core::string_view s,
        bool ne = false) noexcept;

private:
    core::string_view s_;
    std::size_t n_;
    char const* p_;
    bool at_end_;

    void rewind() noexcept override;
    bool measure(std::size_t&) noexcept override;
    void copy(char*&, char const*) noexcept override;
    void increment() noexcept;
};

//------------------------------------------------
//
// param_iter
//
//------------------------------------------------

// A 1-param range allowing
// self-intersection
struct BOOST_SYMBOL_VISIBLE
    single_param_iter
    : any_params_iter
{
    explicit
    single_param_iter(
        param_view const&,
        bool space_as_plus) noexcept;

private:
    bool has_value_;
    bool at_end_ = false;
    bool space_as_plus_ = false;

    void rewind() noexcept override;
    bool measure(std::size_t&) noexcept override;
    void copy(char*&, char const*) noexcept override;
};

//------------------------------------------------
//
// params_iter_base
//
//------------------------------------------------

struct params_iter_base
{
    bool space_as_plus_ = true;
protected:
    explicit params_iter_base(
        bool space_as_plus) noexcept
        : space_as_plus_(space_as_plus)
        {}

    // return encoded size
    void
    measure_impl(
        std::size_t& n,
        param_view const& p) noexcept;

    // encode to dest
    void
    copy_impl(
        char*& dest,
        char const* end,
        param_view const& v) noexcept;
};

//------------------------------------------------

// A range of plain query params_ref
template<class FwdIt>
struct params_iter
    : any_params_iter
    , private params_iter_base
{
    BOOST_CORE_STATIC_ASSERT(
        std::is_convertible<
            typename std::iterator_traits<
                FwdIt>::reference,
            param_view>::value);

    params_iter(
        FwdIt first,
        FwdIt last,
        bool space_as_plus) noexcept
        : any_params_iter(
            first == last)
        , params_iter_base(space_as_plus)
        , it0_(first)
        , it_(first)
        , end_(last)
    {
    }

private:
    FwdIt it0_;
    FwdIt it_;
    FwdIt end_;

    void
    rewind() noexcept override
    {
        it_ = it0_;
    }

    bool
    measure(
        std::size_t& n) noexcept override
    {
        if(it_ == end_)
            return false;
       measure_impl(n,
           param_view(*it_++));
        return true;
    }

    void
    copy(
        char*& dest,
        char const* end) noexcept override
    {
        copy_impl(dest, end,
            param_view(*it_++));
    }
};

//------------------------------------------------
//
// param_encoded_iter
//
//------------------------------------------------

// A 1-param encoded range
// allowing self-intersection
struct BOOST_SYMBOL_VISIBLE
    param_encoded_iter
    : any_params_iter
{
    explicit
    param_encoded_iter(
        param_pct_view const&) noexcept;

private:
    bool has_value_;
    bool at_end_ = false;

    void rewind() noexcept override;
    bool measure(std::size_t&) noexcept override;
    void copy(char*&, char const*) noexcept override;
};

//------------------------------------------------
//
// params_encoded_iter
//
//------------------------------------------------

// Validating and copying from
// a string of encoded params
struct params_encoded_iter_base
{
protected:
    static
    void
    measure_impl(
        std::size_t& n,
        param_view const& v) noexcept;

    static
    void
    copy_impl(
        char*& dest,
        char const* end,
        param_view const& v) noexcept;
};

//------------------------------------------------

// A range of encoded query params_ref
template<class FwdIt>
struct params_encoded_iter
    : any_params_iter
    , private params_encoded_iter_base
{
    BOOST_CORE_STATIC_ASSERT(
        std::is_convertible<
            typename std::iterator_traits<
                FwdIt>::reference,
            param_view>::value);

    params_encoded_iter(
        FwdIt first,
        FwdIt last) noexcept
        : any_params_iter(
            first == last)
        , it0_(first)
        , it_(first)
        , end_(last)
    {
    }

private:
    FwdIt it0_;
    FwdIt it_;
    FwdIt end_;

    void
    rewind() noexcept override
    {
        it_ = it0_;
    }

    bool
    measure(
        std::size_t& n) override
    {
        if(it_ == end_)
            return false;
        // throw on invalid input
        measure_impl(n,
            param_pct_view(
                param_view(*it_++)));
        return true;
    }

    void
    copy(
        char*& dest,
        char const* end
            ) noexcept override
    {
        copy_impl(dest, end,
            param_view(*it_++));
    }
};

//------------------------------------------------
//
// param_value_iter
//
//------------------------------------------------

// An iterator which outputs
// one value on an existing key
struct param_value_iter
    : any_params_iter
{
    param_value_iter(
        std::size_t nk,
        core::string_view const& value,
        bool has_value) noexcept
        : any_params_iter(
            false,
            value)
        , nk_(nk)
        , has_value_(has_value)
    {
    }

private:
    std::size_t nk_ = 0;
    bool has_value_ = false;
    bool at_end_ = false;

    void rewind() noexcept override;
    bool measure(std::size_t&) noexcept override;
    void copy(char*&, char const*) noexcept override;
};

//------------------------------------------------
//
// param_encoded_value_iter
//
//------------------------------------------------

// An iterator which outputs one
// encoded value on an existing key
struct param_encoded_value_iter
    : any_params_iter
{
    param_encoded_value_iter(
        std::size_t nk,
        pct_string_view const& value,
        bool has_value) noexcept
        : any_params_iter(
            false,
            value)
        , nk_(nk)
        , has_value_(has_value)
    {
    }

private:
    std::size_t nk_ = 0;
    bool has_value_ = false;
    bool at_end_ = false;

    void rewind() noexcept override;
    bool measure(std::size_t&) noexcept override;
    void copy(char*&, char const*) noexcept override;
};

//------------------------------------------------

template<class FwdIt>
params_iter<FwdIt>
make_params_iter(
    FwdIt first, FwdIt last, bool space_as_plus)
{
    return params_iter<
        FwdIt>(first, last, space_as_plus);
}

template<class FwdIt>
params_encoded_iter<FwdIt>
make_params_encoded_iter(
    FwdIt first, FwdIt last)
{
    return params_encoded_iter<
        FwdIt>(first, last);
}

} // detail
} // urls
} // boost

#include <boost/url/detail/impl/any_params_iter.hpp>

#endif
