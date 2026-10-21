#ifndef BOOST_LEAF_DETAIL_CAPTURE_LIST_HPP_INCLUDED
#define BOOST_LEAF_DETAIL_CAPTURE_LIST_HPP_INCLUDED

// Copyright 2018-2025 Emil Dotchevski and Reverge Studios, Inc.
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/leaf/config.hpp>

#if BOOST_LEAF_CFG_CAPTURE

namespace boost { namespace leaf {

class error_id;

namespace detail
{
    class encoder;

    class capture_list
    {
        capture_list( capture_list const & ) = delete;
        capture_list & operator=( capture_list const & ) = delete;

    protected:

        class node
        {
            friend class capture_list;

            virtual void unload( int err_id ) = 0;
            virtual void serialize_to_(encoder &, error_id const &) const = 0;

        protected:

            virtual ~node() noexcept
            {
            };

            node * next_;

            BOOST_LEAF_CONSTEXPR explicit node( node * * & last ) noexcept:
                next_(nullptr)
            {
                BOOST_LEAF_ASSERT(last != nullptr);
                *last = this;
                last = &next_;
            }
        } * first_;

        template <class F>
        BOOST_LEAF_CONSTEXPR void for_each( F f ) const
        {
            for( node * p=first_; p; p=p->next_ )
                f(*p);
        }

    public:

        BOOST_LEAF_CONSTEXPR explicit capture_list( node * first ) noexcept:
            first_(first)
        {
        }

        BOOST_LEAF_CONSTEXPR capture_list( capture_list && other ) noexcept:
            first_(other.first_)
        {
            other.first_ = nullptr;
        }

        ~capture_list() noexcept
        {
            for( node const * p = first_; p; )
            {
                node const * n = p -> next_;
                delete p;
                p = n;
            }
        }

        void unload( int const err_id )
        {
            capture_list moved(first_);
            first_ = nullptr;
            tls::write_current_error_id(unsigned(err_id));
            moved.for_each(
                [err_id]( node & n )
                {
                    n.unload(err_id); // last node may throw
                } );
        }

        void serialize_to(encoder & e, error_id const & id) const
        {
            if( first_ )
            {
                for_each(
                    [&e, &id]( node const & n )
                    {
                        n.serialize_to_(e, id);
                    } );
            }
        }
    }; // class capture_list

} // namespace detail

} } // namespace boost::leaf

#endif // #if BOOST_LEAF_CFG_CAPTURE

#endif // #ifndef BOOST_LEAF_DETAIL_CAPTURE_LIST_HPP_INCLUDED
