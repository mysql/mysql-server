//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
// Copyright (c) 2022 Alan de Freitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/url
//

#ifndef BOOST_URL_GRAMMAR_IMPL_RANGE_HPP
#define BOOST_URL_GRAMMAR_IMPL_RANGE_HPP

#include <boost/url/detail/except.hpp>
#include <boost/url/grammar/error.hpp>
#include <boost/url/grammar/recycled.hpp>
#include <boost/core/empty_value.hpp>
#include <boost/assert.hpp>
#include <boost/core/detail/static_assert.hpp>
#include <exception>
#include <iterator>
#include <new>
#include <utility>
#include <type_traits>

#include <stddef.h> // ::max_align_t

namespace boost {
namespace urls {
namespace grammar {

//------------------------------------------------
//
// any_rule
//
//------------------------------------------------

template<class T>
struct any_rule<T>::impl_base
{
    virtual
    ~impl_base() = default;

    virtual
    void
    move(void* dest) noexcept
    {
        ::new(dest) impl_base(
            std::move(*this));
    }

    virtual
    void
    copy(void* dest) const noexcept
    {
        ::new(dest) impl_base(*this);
    }

    virtual
    system::result<T>
    first(
        char const*&,
        char const*) const noexcept
    {
        return system::error_code{};
    }

    virtual
    system::result<T>
    next(
        char const*&,
        char const*) const noexcept
    {
        return system::error_code{};
    }
};

//------------------------------------------------

// small
template<class T>
template<class R, bool Small>
struct any_rule<T>::impl1
    : impl_base
    , private empty_value<R>
{
    explicit
    impl1(R const& next) noexcept
        : empty_value<R>(
            empty_init,
            next)
    {
    }

private:
    impl1(impl1&&) noexcept = default;
    impl1(impl1 const&) noexcept = default;

    void
    move(void* dest
        ) noexcept override
    {
        ::new(dest) impl1(
            std::move(*this));
    }

    void
    copy(void* dest
        ) const noexcept override
    {
        ::new(dest) impl1(*this);
    }

    system::result<T>
    first(
        char const*& it,
        char const* end)
            const noexcept override
    {
        return grammar::parse(
            it, end, this->get());
    }

    system::result<T>
    next(
        char const*& it,
        char const* end)
            const noexcept override
    {
        return grammar::parse(
            it, end, this->get());
    }
};

//------------------------------------------------

// big
template<class T>
template<class R>
struct any_rule<T>::impl1<R, false>
    : impl_base
{
    explicit
    impl1(R const& next) noexcept
    {
        ::new(p_->addr()) impl{next};
    }

private:
    struct impl
    {
        R r;
    };

    recycled_ptr<
        aligned_storage<impl>> p_;

    impl1(impl1&&) noexcept = default;
    impl1(impl1 const&) noexcept = default;

    impl const&
    get() const noexcept
    {
        return *reinterpret_cast<
            impl const*>(p_->addr());
    }

    ~impl1()
    {
        if(p_)
            get().~impl();
    }

    void
    move(void* dest
        ) noexcept override
    {
        ::new(dest) impl1(
            std::move(*this));
    }

    void
    copy(void* dest
        ) const noexcept override
    {
        ::new(dest) impl1(*this);
    }

    system::result<T>
    first(
        char const*& it,
        char const* end)
            const noexcept override
    {
        return grammar::parse(
            it, end, this->get().r);
    }

    system::result<T>
    next(
        char const*& it,
        char const* end)
            const noexcept override
    {
        return grammar::parse(
            it, end, this->get().r);
    }
};

//------------------------------------------------

// small
template<class T>
template<
    class R0, class R1, bool Small>
struct any_rule<T>::impl2
    : impl_base
    , private empty_value<R0, 0>
    , private empty_value<R1, 1>
{
    impl2(
        R0 const& first,
        R1 const& next) noexcept
        : empty_value<R0,0>(
            empty_init, first)
        , empty_value<R1,1>(
            empty_init, next)
    {
    }

private:
    impl2(impl2&&) noexcept = default;
    impl2(impl2 const&) noexcept = default;

    void
    move(void* dest
        ) noexcept override
    {
        ::new(dest) impl2(
            std::move(*this));
    }

    void
    copy(void* dest
        ) const noexcept override
    {
        ::new(dest) impl2(*this);
    }

    system::result<T>
    first(
        char const*& it,
        char const* end)
            const noexcept override
    {
        return grammar::parse(it, end,
            empty_value<
                R0,0>::get());
    }

    system::result<T>
    next(
        char const*& it,
        char const* end)
            const noexcept override
    {
        return grammar::parse(it, end,
            empty_value<
                R1,1>::get());
    }
};

//------------------------------------------------

// big
template<class T>
template<
    class R0, class R1>
struct any_rule<T>::impl2<R0, R1, false>
    : impl_base
{
    impl2(
        R0 const& first,
        R1 const& next) noexcept
    {
        ::new(p_->addr()) impl{
            first, next};
    }

private:
    struct impl
    {
        R0 first;
        R1 next;
    };

    recycled_ptr<
        aligned_storage<impl>> p_;

    impl2(impl2&&) noexcept = default;
    impl2(impl2 const&) noexcept = default;

    impl const&
    get() const noexcept
    {
        return *reinterpret_cast<
            impl const*>(p_->addr());
    }

    ~impl2()
    {
        if(p_)
            get().~impl();
    }

    void
    move(void* dest
        ) noexcept override
    {
        ::new(dest) impl2(
            std::move(*this));
    }

    void
    copy(void* dest
        ) const noexcept override
    {
        ::new(dest) impl2(*this);
    }

    system::result<T>
    first(
        char const*& it,
        char const* end)
            const noexcept override
    {
        return grammar::parse(
            it, end, get().first);
    }

    system::result<T>
    next(
        char const*& it,
        char const* end)
            const noexcept override
    {
        return grammar::parse(
            it, end, get().next);
    }
};

//------------------------------------------------

template<class T>
typename any_rule<T>::impl_base&
any_rule<T>::
get() noexcept
{
    return *reinterpret_cast<
        impl_base*>(sb_.addr());
}

template<class T>
typename any_rule<T>::impl_base const&
any_rule<T>::
get() const noexcept
{
    return *reinterpret_cast<
        impl_base const*>(sb_.addr());
}


template<class T>
any_rule<T>::
any_rule() noexcept
{
    ::new(sb_.addr()) impl_base{};
    char const* it = nullptr;
    get().first(it, nullptr);
    get().next(it, nullptr);
}


template<class T>
any_rule<T>::
any_rule(any_rule&& other) noexcept
{
    other.get().move(sb_.addr());
}


template<class T>
any_rule<T>::
any_rule(any_rule const& other) noexcept
{
    other.get().copy(sb_.addr());
}


template<class T>
any_rule<T>&
any_rule<T>::
operator=(any_rule&& other) noexcept
{
    if(this == &other)
        return *this;
    get().~impl_base();
    other.get().move(sb_.addr());
    return *this;
}


template<class T>
any_rule<T>&
any_rule<T>::
operator=(any_rule const& other) noexcept
{
    if(this == &other)
        return *this;
    get().~impl_base();
    other.get().copy(sb_.addr());
    return *this;
}


template<class T>
any_rule<T>::
~any_rule()
{
    get().~impl_base();
}


template<class T>
template<class R>
any_rule<T>::
any_rule(
    R const& next)
{
    static_assert(
        ::boost::urls::grammar::is_rule<R>::value,
        "Rule requirements not met");
    static_assert(
        std::is_same<typename R::value_type, T>::value,
        "Rule value_type mismatch");

    BOOST_CORE_STATIC_ASSERT(
        sizeof(impl1<R, false>) <=
            BufferSize);

    ::new(sb_.addr()) impl1<R,
        sizeof(impl1<R, true>) <=
            BufferSize>(next);
}

//------------------------------------------------

template<class T>
template<
    class R0, class R1>
any_rule<T>::
any_rule(
    R0 const& first,
    R1 const& next)
{
    static_assert(
        ::boost::urls::grammar::is_rule<R0>::value,
        "Rule requirements not met");
    static_assert(
        ::boost::urls::grammar::is_rule<R1>::value,
        "Rule requirements not met");
    static_assert(
        std::is_same<typename R0::value_type, T>::value,
        "First rule value_type mismatch");
    static_assert(
        std::is_same<typename R1::value_type, T>::value,
        "Next rule value_type mismatch");

    BOOST_CORE_STATIC_ASSERT(
        sizeof(impl2<R0, R1, false>) <=
            BufferSize);

    ::new(sb_.addr()) impl2<R0, R1,
        sizeof(impl2<R0, R1, true>
            ) <= BufferSize>(
                first, next);
}

//------------------------------------------------

template<class T>
system::result<T>
any_rule<T>::
first(
    char const*& it,
    char const* end) const noexcept
{
    return get().first(it, end);
}

//------------------------------------------------

template<class T>
system::result<T>
any_rule<T>::
next(
    char const*& it,
    char const* end) const noexcept
{
    return get().next(it, end);
}

//------------------------------------------------
//
// range
//
//------------------------------------------------

template<class T, class RangeRule>
range<T, RangeRule>::
~range() = default;

template<class T, class RangeRule>
range<T, RangeRule>::
range() noexcept = default;

template<class T, class RangeRule>
range<T, RangeRule>::
range(
    range&& other) noexcept
    : detail::range_base_storage<
        RangeRule>(std::move(other.rule()))
    , s_(other.s_)
    , n_(other.n_)
{
    other.s_ = {};
    other.n_ = 0;
}

template<class T, class RangeRule>
range<T, RangeRule>::
range(
    range const& other) noexcept
    : detail::range_base_storage<
        RangeRule>(other.rule())
    , s_(other.s_)
    , n_(other.n_)
{
}

template<class T, class RangeRule>
auto
range<T, RangeRule>::
operator=(range&& other) noexcept
    -> range&
{
    if(this == &other)
        return *this;
    static_cast<
        detail::range_base_storage<
            RangeRule>&>(*this) =
        std::move(static_cast<
            detail::range_base_storage<
                RangeRule>&>(other));
    s_ = other.s_;
    n_ = other.n_;
    other.s_ = {};
    other.n_ = 0;
    return *this;
}

template<class T, class RangeRule>
auto
range<T, RangeRule>::
operator=(range const& other) noexcept
    -> range&
{
    if(this == &other)
        return *this;
    static_cast<
        detail::range_base_storage<
            RangeRule>&>(*this) =
        static_cast<
            detail::range_base_storage<
                RangeRule> const&>(other);
    s_ = other.s_;
    n_ = other.n_;
    return *this;
}

//------------------------------------------------
//
// iterator
//
//------------------------------------------------

template<class T, class RangeRule>
class range<T, RangeRule>::
    iterator
{
public:
    using value_type = T;
    using reference = T const&;
    using pointer = void const*;
    using difference_type =
        std::ptrdiff_t;
    using iterator_category =
        std::forward_iterator_tag;

    iterator() = default;
    iterator(
        iterator const&) = default;
    iterator& operator=(
        iterator const&) = default;

    reference
    operator*() const noexcept
    {
        return *rv_;
    }

    bool
    operator==(
        iterator const& other) const noexcept
    {
        // can't compare iterators
        // from different containers!
        BOOST_ASSERT(r_ == other.r_);

        return p_ == other.p_;
    }

    bool
    operator!=(
        iterator const& other) const noexcept
    {
        return !(*this == other);
    }

    iterator&
    operator++() noexcept
    {
        BOOST_ASSERT(
            p_ != nullptr);
        auto const end =
            r_->s_.data() +
            r_->s_.size();
        rv_ = r_->rule().next(p_, end);
        if( !rv_ )
            p_ = nullptr;
        return *this;
    }

    iterator
    operator++(int) noexcept
    {
        auto tmp = *this;
        ++*this;
        return tmp;
    }

private:
    friend class range<T, RangeRule>;

    range<T, RangeRule> const* r_ = nullptr;
    char const* p_ = nullptr;
    system::result<T> rv_;

    iterator(
        range<T, RangeRule> const& r) noexcept
        : r_(&r)
        , p_(r.s_.data())
    {
        auto const end =
            r_->s_.data() +
            r_->s_.size();
        rv_ = r_->rule().first(p_, end);
        if( !rv_ )
            p_ = nullptr;
    }

    constexpr
    iterator(
        range<T, RangeRule> const& r,
        int) noexcept
        : r_(&r)
        , p_(nullptr)
    {
    }
};

//------------------------------------------------

template<class T, class RangeRule>
typename range<T, RangeRule>::iterator
range<T, RangeRule>::
begin() const noexcept
{
    return iterator(*this);
}

//------------------------------------------------

template<class T, class RangeRule>
typename range<T, RangeRule>::iterator
range<T, RangeRule>::
end() const noexcept
{
    return iterator(*this, 0);
}

//------------------------------------------------

template<class T, class RangeRule>
range<T, RangeRule>::
range(
    core::string_view s,
    std::size_t n,
    RangeRule const& rule) noexcept
    : detail::range_base_storage<
        RangeRule>(rule)
    , s_(s)
    , n_(n)
{
}

//------------------------------------------------

template<class T, class RangeRule>
range<T, RangeRule>::
range(
    core::string_view s,
    std::size_t n,
    RangeRule&& rule) noexcept
    : detail::range_base_storage<
        RangeRule>(std::move(rule))
    , s_(s)
    , n_(n)
{
}

//------------------------------------------------

template<class R>
BOOST_URL_CXX20_CONSTEXPR
auto
implementation_defined::range_rule_t<R>::
parse(
    char const*& it,
    char const* end) const ->
        system::result<value_type>
{
    using T = typename R::value_type;

    std::size_t n = 0;
    auto const it0 = it;
    auto it1 = it;
    auto rv = (grammar::parse)(
        it, end, next_);
    if( !rv )
    {
        if(rv.error() != error::end_of_range)
        {
            // rewind unless error::end_of_range
            it = it1;
        }
        if(n < N_)
        {
            // too few
            BOOST_URL_CONSTEXPR_RETURN_EC(
                error::mismatch);
        }
        // good
        return range<T>(
            core::string_view(it0, it - it0),
                n, any_rule<T>(next_));
    }
    for(;;)
    {
        ++n;
        it1 = it;
        rv = (grammar::parse)(
            it, end, next_);
        if( !rv )
        {
            if(rv.error() != error::end_of_range)
            {
                // rewind unless error::end_of_range
                it = it1;
            }
            break;
        }
        if(n >= M_)
        {
            // too many
            BOOST_URL_CONSTEXPR_RETURN_EC(
                error::mismatch);
        }
    }
    if(n < N_)
    {
        // too few
        BOOST_URL_CONSTEXPR_RETURN_EC(
            error::mismatch);
    }
    // good
    return range<T>(
        core::string_view(it0, it - it0),
            n, any_rule<T>(next_));
}

//------------------------------------------------

template<class R0, class R1>
BOOST_URL_CXX20_CONSTEXPR
auto
implementation_defined::range_rule_t<R0, R1>::
parse(
    char const*& it,
    char const* end) const ->
        system::result<range<typename
            R0::value_type>>
{
    using T = typename R0::value_type;

    std::size_t n = 0;
    auto const it0 = it;
    auto it1 = it;
    auto rv = (grammar::parse)(
        it, end, first_);
    if( !rv )
    {
        if(rv.error() != error::end_of_range)
        {
            it = it1;
        }
        if(n < N_)
        {
            BOOST_URL_CONSTEXPR_RETURN_EC(
                error::mismatch);
        }
        return range<T>(
            core::string_view(it0, it - it0),
                n, any_rule<T>(first_, next_));
    }
    for(;;)
    {
        ++n;
        it1 = it;
        rv = (grammar::parse)(
            it, end, next_);
        if( !rv )
        {
            if(rv.error() != error::end_of_range)
            {
                // rewind unless error::end_of_range
                it = it1;
            }
            break;
        }
        if(n >= M_)
        {
            // too many
            BOOST_URL_CONSTEXPR_RETURN_EC(
                error::mismatch);
        }
    }
    if(n < N_)
    {
        // too few
        BOOST_URL_CONSTEXPR_RETURN_EC(
            error::mismatch);
    }
    // good
    return range<T>(
        core::string_view(it0, it - it0),
            n, any_rule<T>(first_, next_));
}

} // grammar
} // urls
} // boost

#endif
