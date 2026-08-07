#ifndef BOOST_LEAF_DIAGNOSTICS_HPP_INCLUDED
#define BOOST_LEAF_DIAGNOSTICS_HPP_INCLUDED

// Copyright 2018-2025 Emil Dotchevski and Reverge Studios, Inc.
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/leaf/config.hpp>
#include <boost/leaf/handle_errors.hpp>

namespace boost { namespace leaf {

class diagnostic_info: public error_info
{
    detail::context_base const & ctx_;
    void (*serialize_ctx_to_)(detail::encoder &, detail::context_base const &, error_id);

protected:

    diagnostic_info( diagnostic_info const & ) noexcept = default;

    template <class Context>
    BOOST_LEAF_CONSTEXPR diagnostic_info( error_info const & ei, Context const & ctx ) noexcept:
        error_info(ei),
        ctx_(ctx),
        serialize_ctx_to_(&detail::serialize_context_to<Context>)
    {
    }

    template <class Encoder>
    void serialize_to_(Encoder & e) const
    {
        static_assert(std::is_base_of<detail::encoder, Encoder>::value, "Encoder must derive from detail::encoder");
        serialize_ctx_to_(e, ctx_, error());
    }

public:

    template <class Encoder>
    void serialize_to(Encoder & e) const
    {
        detail::encoder_adaptor<Encoder> ea(e);
        error_info::serialize_to_(ea);
        serialize_to_(ea);
    }

    template <class CharT, class Traits>
    friend std::ostream & operator<<( std::basic_ostream<CharT, Traits> & os, diagnostic_info const & x )
    {
        detail::diagnostics_writer w(os, x.error(), x.source_location(), x.exception());
#if BOOST_LEAF_CFG_DIAGNOSTICS
        x.serialize_to_(w);
#else
        os << "\nboost::leaf::diagnostic_info N/A due to BOOST_LEAF_CFG_DIAGNOSTICS=0";
#endif
        return os;
    }
}; // class diagnostic_info

namespace detail
{
    struct diagnostic_info_: diagnostic_info
    {
        template <class Context>
        BOOST_LEAF_CONSTEXPR diagnostic_info_( error_info const & ei, Context const & ctx ) noexcept:
            diagnostic_info(ei, ctx)
        {
        }
    };

    template <>
    struct handler_argument_traits<diagnostic_info const &>: handler_argument_always_available<e_source_location>
    {
        template <class Context>
        BOOST_LEAF_CONSTEXPR static diagnostic_info_ get( Context const & ctx, error_info const & ei ) noexcept
        {
            return diagnostic_info_(ei, ctx);
        }
    };
}

////////////////////////////////////////

#if BOOST_LEAF_CFG_CAPTURE

class diagnostic_details: public diagnostic_info
{
    detail::dynamic_allocator const * const da_;

protected:

    diagnostic_details( diagnostic_details const & ) noexcept = default;

    template <class Context>
    BOOST_LEAF_CONSTEXPR diagnostic_details( error_info const & ei, Context const & ctx, detail::dynamic_allocator const * da ) noexcept:
        diagnostic_info(ei, ctx),
        da_(da)
    {
    }

    template <class Encoder>
    void serialize_to_(Encoder & e) const
    {
        static_assert(std::is_base_of<detail::encoder, Encoder>::value, "Encoder must derive from detail::encoder");
        if( da_ )
            da_->serialize_to(e, error());
    }

public:

    template <class Encoder>
    void serialize_to(Encoder & e) const
    {
        detail::encoder_adaptor<Encoder> ea(e);
        error_info::serialize_to_(ea);
        diagnostic_info::serialize_to_(ea);
        serialize_to_(ea);
    }

    template <class CharT, class Traits>
    friend std::ostream & operator<<( std::basic_ostream<CharT, Traits> & os, diagnostic_details const & x )
    {
        detail::diagnostics_writer w(os, x.error(), x.source_location(), x.exception());
#if BOOST_LEAF_CFG_DIAGNOSTICS
        x.diagnostic_info::serialize_to_(w);
        w.set_prefix("\nDiagnostic details:" BOOST_LEAF_CFG_DIAGNOSTICS_FIRST_DELIMITER);
        x.serialize_to_(w);
#else
        os << "\nboost::leaf::diagnostic_details N/A due to BOOST_LEAF_CFG_DIAGNOSTICS=0";
#endif
        return os;
    }
}; // class diagnostic_details

namespace detail
{
    struct diagnostic_details_: diagnostic_details
    {
        template <class Context>
        BOOST_LEAF_CONSTEXPR diagnostic_details_( error_info const & ei, Context const & ctx, dynamic_allocator const * da ) noexcept:
            diagnostic_details(ei, ctx, da)
        {
        }
    };

    template <>
    struct handler_argument_traits<diagnostic_details const &>: handler_argument_always_available<e_source_location, dynamic_allocator>
    {
        template <class Context>
        BOOST_LEAF_CONSTEXPR static diagnostic_details_ get( Context const & ctx, error_info const & ei ) noexcept
        {
            slot<dynamic_allocator> const * da = find_in_tuple<slot<dynamic_allocator>>(ctx.tup());
            return diagnostic_details_(ei, ctx, da ? &da->get() : nullptr );
        }
    };
}

#else // #if BOOST_LEAF_CFG_CAPTURE

class diagnostic_details: public diagnostic_info
{
protected:

    diagnostic_details( diagnostic_details const & ) noexcept = default;

    template <class Context>
    BOOST_LEAF_CONSTEXPR diagnostic_details( error_info const & ei, Context const & ctx ) noexcept:
        diagnostic_info(ei, ctx)
    {
    }

public:

    template <class Encoder>
    void serialize_to(Encoder & e) const
    {
        detail::encoder_adaptor<Encoder> ea(e);
        error_info::serialize_to_(ea);
        diagnostic_info::serialize_to_(ea);
    }

    template <class CharT, class Traits>
    friend std::ostream & operator<<( std::basic_ostream<CharT, Traits> & os, diagnostic_details const & x )
    {
        detail::diagnostics_writer w(os, x.error(), x.source_location(), x.exception());
#if BOOST_LEAF_CFG_DIAGNOSTICS
        x.diagnostic_info::serialize_to_(w);
        os << "\nboost::leaf::diagnostic_details N/A due to BOOST_LEAF_CFG_CAPTURE=0";
#else
        os << "\nboost::leaf::diagnostic_details N/A due to BOOST_LEAF_CFG_DIAGNOSTICS=0";
#endif
        return os;
    }
}; // class diagnostic_details

namespace detail
{
    struct diagnostic_details_: diagnostic_details
    {
        template <class Context>
        BOOST_LEAF_CONSTEXPR diagnostic_details_( error_info const & ei, Context const & ctx ) noexcept:
            diagnostic_details(ei, ctx)
        {
        }
    };

    template <>
    struct handler_argument_traits<diagnostic_details const &>: handler_argument_always_available<e_source_location>
    {
        template <class Context>
        BOOST_LEAF_CONSTEXPR static diagnostic_details_ get( Context const & ctx, error_info const & ei ) noexcept
        {
            return diagnostic_details_(ei, ctx);
        }
    };
}

#endif // #else (#if BOOST_LEAF_CFG_CAPTURE)

} } // namespace boost::leaf

#endif // #ifndef BOOST_LEAF_DIAGNOSTICS_HPP_INCLUDED
