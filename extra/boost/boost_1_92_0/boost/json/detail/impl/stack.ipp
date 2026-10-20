//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/json
//

#ifndef BOOST_JSON_DETAIL_IMPL_STACK_IPP
#define BOOST_JSON_DETAIL_IMPL_STACK_IPP

#include <boost/json/detail/stack.hpp>

namespace boost {
namespace json {
namespace detail {

stack::non_trivial<>*
stack::non_trivial<>::destroy() noexcept
{
    non_trivial* const result = next;
    rel(this, nullptr);
    return result;
}

stack::non_trivial<>*
stack::non_trivial<>::relocate(void* dst) noexcept
{
    return rel(this, dst);
}


stack::
~stack()
{
    clear();
    if(base_ != buf_)
        sp_->deallocate(
            base_, cap_);
}

stack::
stack(
    storage_ptr sp,
    unsigned char* buf,
    std::size_t buf_size) noexcept
    : sp_(std::move(sp))
    , cap_(buf_size)
    , base_(buf)
    , buf_(buf)
{
}

void
stack::
clear() noexcept
{
    while(head_)
        head_ = head_->destroy();
    size_ = 0;
}

void
stack::
reserve_impl(std::size_t n)
{
    // caller checks this
    BOOST_ASSERT(n > cap_);

    auto const base = static_cast<unsigned char*>( sp_->allocate(n) );
    if(base_)
    {
        // copy trivials
        std::memcpy(base, base_, size_);

        // copy non-trivials
        non_trivial<>* src = head_;
        non_trivial<>** prev = &head_;
        while(src)
        {
            std::size_t const buf_offset =
                reinterpret_cast<unsigned char*>(src) - base_;
            non_trivial<>* dest = src->relocate(base + buf_offset);
            *prev = dest;
            prev = &dest->next;
            src = dest->next;
        }

        if(base_ != buf_)
            sp_->deallocate(base_, cap_);
    }
    base_ = base;
    cap_ = n;
}

} // detail
} // namespace json
} // namespace boost

#endif
