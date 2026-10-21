//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_CORE_IMPL_SAVED_HANDLER_HPP
#define BOOST_BEAST_CORE_IMPL_SAVED_HANDLER_HPP

#include <boost/beast/core/detail/allocator.hpp>
#include <boost/asio/associated_allocator.hpp>
#include <boost/asio/associated_cancellation_slot.hpp>
#include <boost/asio/associated_executor.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <boost/assert.hpp>
#include <boost/core/empty_value.hpp>
#include <boost/core/exchange.hpp>
#include <utility>

namespace boost {
namespace beast {

//------------------------------------------------------------------------------

class saved_handler::base
{
protected:
    ~base() = default;
    saved_handler * owner_;
public:
    base(saved_handler * owner) : owner_(owner){}
    void set_owner(saved_handler * new_owner) { owner_ = new_owner;}
    virtual void destroy() = 0;
    virtual void invoke() = 0;
};

//------------------------------------------------------------------------------

template<class Handler, class Alloc>
class saved_handler::impl final : public base
{
    friend class saved_handler;

    using alloc_type = typename
        beast::detail::allocator_traits<
            Alloc>::template rebind_alloc<impl>;

    using alloc_traits =
        beast::detail::allocator_traits<alloc_type>;

    class cancel_op
    {
        impl* p_;
        net::cancellation_type accepted_ct_;

    public:
        cancel_op(impl* p, net::cancellation_type accepted_ct)
            : p_(p)
            , accepted_ct_(accepted_ct)
        {
        }

        void
        operator()(net::cancellation_type ct)
        {
            if((ct & accepted_ct_) != net::cancellation_type::none)
                p_->self_complete();
        }
    };

    class storage
    {
        alloc_type a_;
        impl* p_;
        bool c_;

    public:
        explicit
        storage(Alloc const& a)
            : a_(a)
            , p_(alloc_traits::allocate(a_, 1))
            , c_(false)
        {
        }

        template<class Handler_>
        void
        construct(Handler_&& h, saved_handler* owner)
        {
            alloc_traits::construct(
                a_, p_, a_, std::forward<Handler_>(h), owner);
            c_ = true;
        }

        impl*
        get() noexcept
        {
            return p_;
        }

        impl*
        release() noexcept
        {
            return boost::exchange(p_, nullptr);
        }

        ~storage()
        {
            if(p_)
            {
                if(c_)
                    alloc_traits::destroy(a_, p_);
                alloc_traits::deallocate(a_, p_, 1);
            }
        }
    };

    struct ebo_pair : boost::empty_value<alloc_type>
    {
        Handler h;

        template<class Handler_>
        ebo_pair(
            alloc_type const& a,
            Handler_&& h_)
            : boost::empty_value<alloc_type>(
                boost::empty_init_t{}, a)
            , h(std::forward<Handler_>(h_))
        {
        }
    };

    ebo_pair v_;
    net::executor_work_guard<
        net::associated_executor_t<Handler>> wg2_;

public:
    template<class Handler_>
    impl(alloc_type const& a, Handler_&& h, saved_handler* owner)
        : base(owner)
        , v_(a, std::forward<Handler_>(h))
        , wg2_(net::get_associated_executor(v_.h))
    {
    }

    ~impl()
    {
    }

    void
    destroy() override
    {
        net::get_associated_cancellation_slot(v_.h).clear();
        auto v = std::move(v_);
        alloc_traits::destroy(v.get(), this);
        alloc_traits::deallocate(v.get(), this, 1);
    }

    void
    invoke() override
    {
        net::get_associated_cancellation_slot(v_.h).clear();
        auto v = std::move(v_);
        alloc_traits::destroy(v.get(), this);
        alloc_traits::deallocate(v.get(), this, 1);
        v.h();
    }

    void self_complete()
    {
        net::get_associated_cancellation_slot(v_.h).clear();
        owner_->p_ = nullptr;
        auto v = std::move(v_);
        alloc_traits::destroy(v.get(), this);
        alloc_traits::deallocate(v.get(), this, 1);
        v.h(net::error::operation_aborted);
    }
};

//------------------------------------------------------------------------------

template<class Handler, class Allocator>
void
saved_handler::
emplace(
    Handler&& handler,
    Allocator const& alloc,
    net::cancellation_type cancel_type)
{
    // Can't delete a handler before invoking
    BOOST_ASSERT(! has_value());
    using impl_type =
        impl<typename std::decay<Handler>::type, Allocator>;

    typename impl_type::storage s(alloc);
    auto c_slot = net::get_associated_cancellation_slot(handler);

    s.construct(std::forward<Handler>(handler), this);

    if(c_slot.is_connected())
        c_slot.template emplace<
            typename impl_type::cancel_op>(s.get(), cancel_type);

    p_ = s.release();
}

template<class Handler>
void
saved_handler::
emplace(Handler&& handler, net::cancellation_type cancel_type)
{
    // Can't delete a handler before invoking
    BOOST_ASSERT(! has_value());
    emplace(
        std::forward<Handler>(handler),
        net::get_associated_allocator(handler),
        cancel_type);
}

} // beast
} // boost

#endif
