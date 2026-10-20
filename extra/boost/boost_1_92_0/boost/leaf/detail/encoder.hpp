#ifndef BOOST_LEAF_DETAIL_ENCODER_HPP_INCLUDED
#define BOOST_LEAF_DETAIL_ENCODER_HPP_INCLUDED

// Copyright 2018-2026 Emil Dotchevski and Reverge Studios, Inc.
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/leaf/config.hpp>
#include <boost/leaf/detail/type_name.hpp>
#include <boost/leaf/detail/function_traits.hpp>
#include <type_traits>
#include <utility>

namespace boost { namespace leaf {

namespace serialization
{
    struct encoder_adl {};

    template <class Encoder, class T>
    typename std::enable_if<
        sizeof(T) == sizeof(decltype(std::declval<T const &>().value)),
        decltype(output(std::declval<Encoder &>(), std::declval<T const &>().value), void())>::type
    output(Encoder & e, T const & x)
    {
        output(e, x.value);
    }
}

namespace detail
{
    class encoder:
        serialization::encoder_adl
    {
        encoder(encoder const &) = delete;
        encoder & operator=(encoder const &) = delete;

        type_name const type_;
        void * const e_;

        bool dispatch_()
        {
            return false;
        }

        template <class F1, class... Fn>
        bool dispatch_(F1 && f1, Fn && ... fn)
        {
            using encoder_type = typename std::decay<fn_arg_type<F1, 0>>::type;
            if (encoder_type * e = get<encoder_type>())
            {
                std::forward<F1>(f1)(*e);
                return true;
            }
            return dispatch_(std::forward<Fn>(fn)...);
        }

    protected:

        template <class Encoder>
        explicit encoder(Encoder * e) noexcept:
            type_(get_type_name<Encoder>()),
            e_(e)
        {
        }

    public:

        template <class Encoder>
        Encoder * get() noexcept
        {
            return type_ == get_type_name<Encoder>() ? static_cast<Encoder *>(e_) : nullptr;
        }

        template <class... Fn>
        bool dispatch(Fn && ... fn)
        {
            using encoder_types = leaf_detail_mp11::mp_list<typename std::decay<fn_arg_type<Fn, 0>>::type...>;
            static_assert(std::is_same<encoder_types, leaf_detail_mp11::mp_unique<encoder_types>>::value, "Duplicate encoder types in dispatch");
            return dispatch_(std::forward<Fn>(fn)...);
        }
    };

    template <class Encoder>
    struct encoder_adaptor:
        encoder
    {
        explicit encoder_adaptor(Encoder & e) noexcept:
            encoder(&e)
        {
        }
    };
} // namespace detail

} } // namespace boost::leaf

#endif // #ifndef BOOST_LEAF_DETAIL_ENCODER_HPP_INCLUDED
