#ifndef BOOST_LEAF_EXCEPTION_HPP_INCLUDED
#define BOOST_LEAF_EXCEPTION_HPP_INCLUDED

// Copyright 2018-2026 Emil Dotchevski and Reverge Studios, Inc.
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/leaf/config.hpp>
#include <boost/leaf/error.hpp>
#include <boost/leaf/detail/exception_base.hpp>

#ifndef BOOST_LEAF_NO_EXCEPTIONS
#   include <typeinfo>
#endif

namespace boost { namespace leaf {

namespace serialization
{
    template <class Encoder>
    void output(Encoder & e, std::exception const & x)
    {
        char const dynamic_type[] = "dynamic_type";
        char const what[] = "what";
#ifdef BOOST_LEAF_NO_EXCEPTIONS
        output_at(e, "<<unknown>>", dynamic_type);
#else
        output_at(e, detail::demangler(typeid(x).name()).get(), dynamic_type);
#endif
        if( char const * wh = x.what() )
            output_at(e, wh, what);
        else
            output_at(e, "<<nullptr>>", what);
    }

    template <class Encoder>
    void output(Encoder & e, std::exception_ptr const & x)
    {
        if( x )
        {
#ifndef BOOST_LEAF_NO_EXCEPTIONS
            try
            {
                std::rethrow_exception(x);
            }
            catch( std::exception const & ex )
            {
                output(e, ex);
                return;
            }
            catch( ... )
            {
            }
#endif
            output_at(e, "<<unknown>>", "dynamic_type");
        }
        else
            output_at(e, "<<empty>>", "dynamic_type");
        output_at(e, "N/A", "what");
    }
}

} }

////////////////////////////////////////

#define BOOST_LEAF_THROW_EXCEPTION ::boost::leaf::detail::throw_with_loc{__FILE__,__LINE__,__FUNCTION__}+::boost::leaf::detail::make_exception

namespace boost { namespace leaf {

namespace detail
{
    struct throw_with_loc
    {
        char const * const file;
        int const line;
        char const * const fn;

        template <class Ex>
        [[noreturn]] friend void operator+( throw_with_loc loc, Ex && ex )
        {
            ex.load_source_location_(loc.file, loc.line, loc.fn);
            ::boost::leaf::throw_exception_(std::move(ex));
        }
    };
}

////////////////////////////////////////

namespace detail
{
    inline void enforce_std_exception( std::exception const & ) noexcept { }

    template <class Ex>
    class exception final:
        public Ex,
        public exception_base,
        public error_id
    {
        mutable bool clear_current_error_;

        bool is_current_exception() const noexcept
        {
            return tls::read_current_error_id() == unsigned(error_id::value());
        }

        error_id get_error_id() const noexcept override
        {
            clear_current_error_ = false;
            return *this;
        }

        detail::type_name get_type_name() const override
        {
            return detail::get_type_name<Ex>();
        }

    public:

        exception( exception const & other ):
            Ex(other),
            exception_base(other),
            error_id(other),
            clear_current_error_(other.clear_current_error_)
        {
            other.clear_current_error_ = false;
        }

        exception( exception && other ) noexcept:
            Ex(std::move(other)),
            exception_base(std::move(other)),
            error_id(std::move(other)),
            clear_current_error_(std::move(other.clear_current_error_))
        {
            other.clear_current_error_ = false;
        }

        exception( error_id id, Ex const & ex ) noexcept:
            Ex(ex),
            error_id(id),
            clear_current_error_(true)
        {
            enforce_std_exception(*this);
        }

        exception( error_id id, Ex && ex ) noexcept:
            Ex(std::move(ex)),
            error_id(id),
            clear_current_error_(true)
        {
            enforce_std_exception(*this);
        }

        explicit exception( error_id id ) noexcept:
            error_id(id),
            clear_current_error_(true)
        {
            enforce_std_exception(*this);
        }

        ~exception() noexcept
        {
            if( clear_current_error_ && is_current_exception() )
                tls::write_current_error_id(0);
        }
    }; // template exception

    template <class... T>
    struct at_least_one_derives_from_std_exception;

    template <>
    struct at_least_one_derives_from_std_exception<>: std::false_type { };

    template <class T, class... Rest>
    struct at_least_one_derives_from_std_exception<T, Rest...>
    {
        constexpr static const bool value = std::is_base_of<std::exception,typename std::remove_reference<T>::type>::value || at_least_one_derives_from_std_exception<Rest...>::value;
    };

    template <class Ex, class... E>
    inline
    typename std::enable_if<std::is_base_of<std::exception,typename std::remove_reference<Ex>::type>::value, exception<typename std::remove_reference<Ex>::type>>::type
    make_exception( error_id err, Ex && ex, E && ... e ) noexcept(!BOOST_LEAF_CFG_CAPTURE)
    {
        static_assert(!at_least_one_derives_from_std_exception<E...>::value, "Error objects passed to leaf::exception may not derive from std::exception");
        return exception<typename std::remove_reference<Ex>::type>( err.load(std::forward<E>(e)...), std::forward<Ex>(ex) );
    }

    template <class E1, class... E>
    inline
    typename std::enable_if<!std::is_base_of<std::exception,typename std::remove_reference<E1>::type>::value, exception<std::exception>>::type
    make_exception( error_id err, E1 && car, E && ... cdr ) noexcept(!BOOST_LEAF_CFG_CAPTURE)
    {
        static_assert(!at_least_one_derives_from_std_exception<E...>::value, "Error objects passed to leaf::exception may not derive from std::exception");
        return exception<std::exception>( err.load(std::forward<E1>(car), std::forward<E>(cdr)...) );
    }

    inline exception<std::exception> make_exception( error_id err ) noexcept
    {
        return exception<std::exception>(err);
    }

    template <class Ex, class... E>
    inline
    typename std::enable_if<std::is_base_of<std::exception,typename std::remove_reference<Ex>::type>::value, exception<typename std::remove_reference<Ex>::type>>::type
    make_exception( Ex && ex, E && ... e ) noexcept(!BOOST_LEAF_CFG_CAPTURE)
    {
        static_assert(!at_least_one_derives_from_std_exception<E...>::value, "Error objects passed to leaf::exception may not derive from std::exception");
        return exception<typename std::remove_reference<Ex>::type>( new_error().load(std::forward<E>(e)...), std::forward<Ex>(ex) );
    }

    template <class E1, class... E>
    inline
    typename std::enable_if<!std::is_base_of<std::exception,typename std::remove_reference<E1>::type>::value, exception<std::exception>>::type
    make_exception( E1 && car, E && ... cdr ) noexcept(!BOOST_LEAF_CFG_CAPTURE)
    {
        static_assert(!at_least_one_derives_from_std_exception<E...>::value, "Error objects passed to leaf::exception may not derive from std::exception");
        return exception<std::exception>( new_error().load(std::forward<E1>(car), std::forward<E>(cdr)...) );
    }

    inline exception<std::exception> make_exception() noexcept
    {
        return exception<std::exception>(leaf::new_error());
    }
} // namespace detail

template <class... E>
[[noreturn]] void throw_exception( E && ... e )
{
    // Warning: setting a breakpoint here will not intercept exceptions thrown
    // via BOOST_LEAF_THROW_EXCEPTION or originating in the few other throw
    // points elsewhere in LEAF. To intercept all of those exceptions as well,
    // set a breakpoint inside boost::leaf::throw_exception_.
    throw_exception_(detail::make_exception(std::forward<E>(e)...));
}

////////////////////////////////////////

#ifndef BOOST_LEAF_NO_EXCEPTIONS

namespace detail
{
    inline error_id catch_exceptions_helper( std::exception const &, leaf_detail_mp11::mp_list<> )
    {
        return leaf::new_error(std::current_exception());
    }

    template <class Ex1, class... Ex>
    inline error_id catch_exceptions_helper( std::exception const & ex, leaf_detail_mp11::mp_list<Ex1,Ex...> )
    {
        if( Ex1 const * p = dynamic_cast<Ex1 const *>(&ex) )
            return catch_exceptions_helper(ex, leaf_detail_mp11::mp_list<Ex...>{ }).load(*p);
        else
            return catch_exceptions_helper(ex, leaf_detail_mp11::mp_list<Ex...>{ });
    }

    template <class T>
    struct deduce_exception_to_result_return_type_impl
    {
        using type = result<T>;
    };

    template <class T>
    struct deduce_exception_to_result_return_type_impl<result<T>>
    {
        using type = result<T>;
    };

    template <class T>
    using deduce_exception_to_result_return_type = typename deduce_exception_to_result_return_type_impl<T>::type;
} // namespace detail

template <class... Ex, class F>
inline
detail::deduce_exception_to_result_return_type<detail::fn_return_type<F>>
exception_to_result( F && f ) noexcept(!BOOST_LEAF_CFG_CAPTURE)
{
    try
    {
        return std::forward<F>(f)();
    }
    catch( std::exception const & ex )
    {
        return detail::catch_exceptions_helper(ex, leaf_detail_mp11::mp_list<Ex...>());
    }
    catch(...)
    {
        return leaf::new_error(std::current_exception());
    }
}

#endif // #ifndef BOOST_LEAF_NO_EXCEPTIONS

} } // namespace boost::leaf

#endif // #ifndef BOOST_LEAF_EXCEPTION_HPP_INCLUDED
