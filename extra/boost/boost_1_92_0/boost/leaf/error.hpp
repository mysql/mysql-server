#ifndef BOOST_LEAF_ERROR_HPP_INCLUDED
#define BOOST_LEAF_ERROR_HPP_INCLUDED

// Copyright 2018-2025 Emil Dotchevski and Reverge Studios, Inc.
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/leaf/config.hpp>
#include <boost/leaf/detail/optional.hpp>
#include <boost/leaf/detail/function_traits.hpp>
#include <boost/leaf/detail/capture_list.hpp>
#include <boost/leaf/detail/diagnostics_writer.hpp>

////////////////////////////////////////

#if BOOST_LEAF_CFG_STD_SYSTEM_ERROR

#include <system_error>

namespace boost { namespace leaf {

namespace serialization
{
    template <class Encoder>
    void output(Encoder & e, std::error_code const & x)
    {
        output_at(e, x.category().name(), "category");
        output_at(e, x.value(), "value");
        output_at(e, x.message(), "message");
    }

    template <class Encoder>
    void output(Encoder & e, std::error_condition const & x)
    {
        output_at(e, x.category().name(), "category");
        output_at(e, x.value(), "value");
        output_at(e, x.message(), "message");
    }
}

} }

#endif

////////////////////////////////////////

#define BOOST_LEAF_TOKEN_PASTE(x, y) x ## y
#define BOOST_LEAF_TOKEN_PASTE2(x, y) BOOST_LEAF_TOKEN_PASTE(x, y)
#define BOOST_LEAF_TMP BOOST_LEAF_TOKEN_PASTE2(boost_leaf_tmp_, __LINE__)

#define BOOST_LEAF_ASSIGN(v,r)\
    auto && BOOST_LEAF_TMP = r;\
    static_assert(::boost::leaf::is_result_type<typename std::decay<decltype(BOOST_LEAF_TMP)>::type>::value,\
        "BOOST_LEAF_ASSIGN/BOOST_LEAF_AUTO requires a result object as the second argument (see is_result_type)");\
    if( !BOOST_LEAF_TMP )\
        return std::forward<decltype(BOOST_LEAF_TMP)>(BOOST_LEAF_TMP).error();\
    v = std::forward<decltype(BOOST_LEAF_TMP)>(BOOST_LEAF_TMP).value()

#define BOOST_LEAF_AUTO(v, r)\
    BOOST_LEAF_ASSIGN(auto v, r)

#if BOOST_LEAF_CFG_GNUC_STMTEXPR

#   define BOOST_LEAF_CHECK(r)\
        ({\
            auto && BOOST_LEAF_TMP = (r);\
            static_assert(::boost::leaf::is_result_type<typename std::decay<decltype(BOOST_LEAF_TMP)>::type>::value,\
                "BOOST_LEAF_CHECK requires a result object (see is_result_type)");\
            if( !BOOST_LEAF_TMP )\
                return std::forward<decltype(BOOST_LEAF_TMP)>(BOOST_LEAF_TMP).error();\
            std::move(BOOST_LEAF_TMP);\
        }).value()

#else // #if BOOST_LEAF_CFG_GNUC_STMTEXPR

#   define BOOST_LEAF_CHECK(r)\
        {\
            auto && BOOST_LEAF_TMP = (r);\
            static_assert(::boost::leaf::is_result_type<typename std::decay<decltype(BOOST_LEAF_TMP)>::type>::value,\
                "BOOST_LEAF_CHECK requires a result object (see is_result_type)");\
            if( !BOOST_LEAF_TMP )\
                return std::forward<decltype(BOOST_LEAF_TMP)>(BOOST_LEAF_TMP).error();\
        }

#endif // #else (#if BOOST_LEAF_CFG_GNUC_STMTEXPR)

#define BOOST_LEAF_NEW_ERROR ::boost::leaf::detail::inject_loc{__FILE__,__LINE__,__FUNCTION__}+::boost::leaf::new_error

namespace boost { namespace leaf {

struct e_source_location
{
    char const * file;
    int line;
    char const * function;

    template <class CharT, class Traits>
    friend std::ostream & operator<<(std::basic_ostream<CharT, Traits> & os, e_source_location const & x)
    {
        return os << x.file << '(' << x.line << ") in function " << x.function;
    }

    template <class Encoder>
    friend void output( Encoder & e, e_source_location const & x )
    {
        output_at(e, x.file, "file");
        output_at(e, x.line, "line");
        output_at(e, x.function, "function");
    }
};

template <>
struct show_in_diagnostics<e_source_location>: std::false_type
{
};

////////////////////////////////////////

namespace serialization
{
    template <class Encoder, class T, class... Unused>
    typename std::enable_if<std::is_base_of<detail::encoder, Encoder>::value>::type
    serialize(Encoder &, T const &, char const *, Unused && ...)
    {
    }
}

namespace detail
{
    template <class T>
    void serialize_(diagnostics_writer & e, T const & x)
    {
        e.write(x);
    }

    template <class T>
    void serialize_(encoder & e, T const & x)
    {
        using namespace serialization;
        char zstr[1024];
        serialize(e, x, to_zstr(zstr, get_type_name<T>()));
        if( diagnostics_writer * dw = e.get<diagnostics_writer>() )
            dw->write(x);
    }
}

////////////////////////////////////////

namespace detail
{
    template <class E>
    class slot:
        optional<E>
    {
        static_assert(std::is_same<E, typename std::decay<E>::type>::value, "E must be decayed for slot<E>");
        slot( slot const & ) = delete;
        slot & operator=( slot const & ) = delete;

        using impl = optional<E>;
        slot<E> * prev_;

    public:

        BOOST_LEAF_CONSTEXPR slot():
            prev_(nullptr)
        {
            tls::reserve_ptr<slot<E>>();
        }

        template <class T>
        BOOST_LEAF_CONSTEXPR slot( int key, T && e ):
            optional<E>(key, std::forward<T>(e)),
            prev_(nullptr)
        {
            tls::reserve_ptr<slot<E>>();
        }

        BOOST_LEAF_CONSTEXPR slot( slot && x ) noexcept:
            optional<E>(std::move(x)),
            prev_(nullptr)
        {
            BOOST_LEAF_ASSERT(x.prev_ == nullptr);
        }

        ~slot() noexcept
        {
            BOOST_LEAF_ASSERT(tls::read_ptr<slot<E>>() != this);
        }

        void activate() noexcept
        {
            prev_ = tls::read_ptr<slot<E>>();
            tls::write_ptr<slot<E>>(this);
        }

        void deactivate() const noexcept
        {
            tls::write_ptr<slot<E>>(prev_);
        }

        void unload( int err_id ) noexcept(!BOOST_LEAF_CFG_CAPTURE);

        template <class Encoder,class ErrorID>
        void serialize_to(Encoder & e, ErrorID id) const
        {
            static_assert(std::is_base_of<encoder, Encoder>::value, "Encoder must derive from detail::encoder");
            if( int k = this->key() )
            {
                if( id && id.value() != k )
                    return;
                serialize_(e, value(k));
            }
        }

        using impl::load;
        using impl::has_value;
        using impl::value;
    }; // template slot
} // namespace detail

////////////////////////////////////////

#if BOOST_LEAF_CFG_CAPTURE

namespace detail
{
    template <class E>
    struct capturing_slot_node_allocator;

    class dynamic_allocator:
        capture_list
    {
        dynamic_allocator( dynamic_allocator const & ) = delete;
        dynamic_allocator & operator=( dynamic_allocator const & ) = delete;

        template <class>
        friend struct capturing_slot_node_allocator;

        class capturing_node:
            public capture_list::node
        {
        protected:
            BOOST_LEAF_CONSTEXPR explicit capturing_node( capture_list::node * * & last ) noexcept:
                node(last)
            {
                BOOST_LEAF_ASSERT(last == &next_);
                BOOST_LEAF_ASSERT(next_ == nullptr);
            }
        public:
            virtual void deactivate() const noexcept = 0;
        };

        template <class E>
        class capturing_slot_node final:
            public slot<E>,
            public capturing_node
        {
            using impl = slot<E>;
            capturing_slot_node( capturing_slot_node const & ) = delete;
            capturing_slot_node & operator=( capturing_slot_node const & ) = delete;
            void deactivate() const noexcept override
            {
                impl::deactivate();
            }
            void unload( int err_id ) override
            {
                impl::unload(err_id);
            }
            void serialize_to_(encoder & e, error_id const & id) const override
            {
                impl::serialize_to(e, id);
            }
        public:
            BOOST_LEAF_CONSTEXPR explicit capturing_slot_node( capture_list::node * * & last ):
                capturing_node(last)
            {
                BOOST_LEAF_ASSERT(last == &next_);
                BOOST_LEAF_ASSERT(next_ == nullptr);
            }
            template <class T>
            BOOST_LEAF_CONSTEXPR capturing_slot_node( capture_list::node * * & last, int err_id, T && e ):
                slot<E>(err_id, std::forward<T>(e)),
                capturing_node(last)
            {
                BOOST_LEAF_ASSERT(last == &next_);
                BOOST_LEAF_ASSERT(next_ == nullptr);
            }
        };

#ifndef BOOST_LEAF_NO_EXCEPTIONS
        class capturing_exception_node final:
            public capturing_node
        {
            capturing_exception_node( capturing_exception_node const & ) = delete;
            capturing_exception_node & operator=( capturing_exception_node const & ) = delete;
            void deactivate() const noexcept override
            {
                BOOST_LEAF_ASSERT(0);
            }
            void unload( int ) override
            {
                std::rethrow_exception(ex_);
            }
            void serialize_to_(encoder &, error_id const &) const override
            {
            }
            std::exception_ptr const ex_;
        public:
            capturing_exception_node( capture_list::node * * & last, std::exception_ptr && ex ) noexcept:
                capturing_node(last),
                ex_(std::move(ex))
            {
                BOOST_LEAF_ASSERT(last == &next_);
                BOOST_LEAF_ASSERT(ex_);
            }
        };
#endif // #ifndef BOOST_LEAF_NO_EXCEPTIONS

        node * * last_;

    public:

        dynamic_allocator() noexcept:
            capture_list(nullptr),
            last_(&first_)
        {
            BOOST_LEAF_ASSERT(first_ == nullptr);
        }

        dynamic_allocator( dynamic_allocator && other ) noexcept:
            capture_list(std::move(other)),
            last_(other.last_ == &other.first_? &first_ : other.last_)
        {
            BOOST_LEAF_ASSERT(last_ != nullptr);
            BOOST_LEAF_ASSERT(*last_ == nullptr);
            BOOST_LEAF_ASSERT(other.first_ == nullptr);
            other.last_ = &other.first_;
        }

        template <class E>
        slot<E> * alloc()
        {
            BOOST_LEAF_ASSERT(last_ != nullptr);
            BOOST_LEAF_ASSERT(*last_ == nullptr);
            BOOST_LEAF_ASSERT(tls::read_ptr<slot<E>>() == nullptr);
            capturing_slot_node<E> * csn = capturing_slot_node_allocator<E>::new_(last_);
            csn->activate();
            return csn;
        }

        template <class E>
        slot<E> * reserve()
        {
            if( slot<E> * p = tls::read_ptr<slot<E>>() )
                return p;
            else
                return alloc<E>();
        }

        void deactivate() const noexcept
        {
            for_each(
                []( capture_list::node const & n )
                {
                    static_cast<capturing_node const &>(n).deactivate();
                } );
        }

        template <class LeafResult>
        LeafResult extract_capture_list(int err_id)
        {
#ifndef BOOST_LEAF_NO_EXCEPTIONS
            if( std::exception_ptr ex = std::current_exception() )
                (void) new capturing_exception_node(last_, std::move(ex));
#endif
            detail::capture_list::node * const f = first_;
            first_ = nullptr;
            last_ = &first_;
            return { err_id, capture_list(f) };
        }

        using capture_list::unload;
        using capture_list::serialize_to;
    }; // class dynamic_allocator

    template <class E>
    struct capturing_slot_node_allocator
    {
        template <class... A>
        static dynamic_allocator::capturing_slot_node<E> * new_( A && ... a )
        {
            return new dynamic_allocator::capturing_slot_node<E>(std::forward<A>(a)...);
        }

        static void delete_( dynamic_allocator::capturing_slot_node<E> * p ) noexcept
        {
            delete p;
        }
    };

    template <>
    class slot<dynamic_allocator>
    {
        slot( slot const & ) = delete;
        slot & operator=( slot const & ) = delete;

        dynamic_allocator da_;
        slot * prev_;

    public:

        slot() noexcept:
            prev_(nullptr)
        {
            tls::reserve_ptr<slot<dynamic_allocator>>();
        }

        slot( slot && x ) noexcept:
            da_(std::move(x.da_)),
            prev_(nullptr)
        {
            BOOST_LEAF_ASSERT(x.prev_ == nullptr);
        }

        ~slot() noexcept
        {
            BOOST_LEAF_ASSERT(tls::read_ptr<slot<dynamic_allocator>>() != this);
        }

        dynamic_allocator const & get() const noexcept
        {
            return da_;
        }

        dynamic_allocator & get() noexcept
        {
            return da_;
        }

        void activate() noexcept
        {
            prev_ = tls::read_ptr<slot<dynamic_allocator>>();
            tls::write_ptr<slot<dynamic_allocator>>(this);
        }

        void deactivate() const noexcept
        {
            da_.deactivate();
            tls::write_ptr<slot<dynamic_allocator>>(prev_);
        }

        void unload( int err_id )
        {
            BOOST_LEAF_ASSERT(err_id);
            da_.unload(err_id);
        }

        template <class ErrorID>
        void serialize_to(encoder &, ErrorID) const
        {
        }
    }; // slot specialization for dynamic_allocator
} // namespace detail

#endif // #if BOOST_LEAF_CFG_CAPTURE

////////////////////////////////////////

namespace detail
{
#if BOOST_LEAF_CFG_CAPTURE
    inline dynamic_allocator * get_dynamic_allocator() noexcept
    {
        if( slot<dynamic_allocator> * sl = tls::read_ptr<slot<dynamic_allocator>>() )
            return &sl->get();
        return nullptr;
    }
#endif

    template <class E>
    inline slot<E> * get_slot() noexcept(!BOOST_LEAF_CFG_CAPTURE)
    {
        static_assert(!std::is_pointer<E>::value, "Error objects of pointer types are not allowed");
        static_assert(!std::is_same<E, error_id>::value, "Error objects of type error_id are not allowed");
        if( slot<E> * p = tls::read_ptr<slot<E>>() )
            return p;
#if BOOST_LEAF_CFG_CAPTURE
        if( dynamic_allocator * da = get_dynamic_allocator() )
            return da->alloc<E>();
#endif
        return nullptr;
    }

    template <class E>
    inline void slot<E>::unload( int err_id ) noexcept(!BOOST_LEAF_CFG_CAPTURE)
    {
        BOOST_LEAF_ASSERT(err_id);
        if( this->key() != err_id )
            return;
        if( impl * p = get_slot<E>() )
            if( !p->has_value(err_id) )
                *p = std::move(*this);
    }

    template <class E>
    BOOST_LEAF_CONSTEXPR inline int load_slot( int err_id, E && e ) noexcept(!BOOST_LEAF_CFG_CAPTURE)
    {
        using E_decayed = typename std::decay<E>::type;
        BOOST_LEAF_ASSERT((err_id&3) == 1);
        if( slot<E_decayed> * p = get_slot<E_decayed>() )
            (void) p->load(err_id, std::forward<E>(e));
        return 0;
    }

    template <class F>
    BOOST_LEAF_CONSTEXPR inline int load_slot_deferred( int err_id, F && f )
    {
        using E = typename function_traits<F>::return_type;
        using E_decayed = typename std::decay<E>::type;
        BOOST_LEAF_ASSERT((err_id&3) == 1);
        if( slot<E_decayed> * p = get_slot<E_decayed>() )
            (void) p->load(err_id, std::forward<F>(f)());
        return 0;
    }

    template <class F>
    BOOST_LEAF_CONSTEXPR inline int load_slot_accumulate( int err_id, F && f )
    {
        static_assert(function_traits<F>::arity == 1, "Lambdas passed to accumulate must take a single e-type argument by reference");
        using E = fn_arg_type<F,0>;
        using E_decayed = typename std::decay<E>::type;
        BOOST_LEAF_ASSERT((err_id&3) == 1);
        if( slot<E_decayed> * p = get_slot<E_decayed>() )
            if( E_decayed * v = p->has_value(err_id) )
                (void) std::forward<F>(f)(*v);
            else
                (void) std::forward<F>(f)(p->load(err_id, E_decayed()));
        return 0;
    }
} // namespace detail

////////////////////////////////////////

namespace detail
{
    template <class T, int Arity = function_traits<T>::arity>
    struct load_item
    {
        static_assert(Arity == 0 || Arity == 1, "If a functions is passed to new_error or load, it must take zero or one argument");
    };

    template <class E>
    struct load_item<E, -1>
    {
        BOOST_LEAF_CONSTEXPR static int load_( int err_id, E && e )
        {
            return load_slot(err_id, std::forward<E>(e));
        }
    };

    template <class F>
    struct load_item<F, 0>
    {
        BOOST_LEAF_CONSTEXPR static int load_( int err_id, F && f )
        {
            return load_slot_deferred(err_id, std::forward<F>(f));
        }
    };

    template <class F>
    struct load_item<F, 1>
    {
        BOOST_LEAF_CONSTEXPR static int load_( int err_id, F && f )
        {
            return load_slot_accumulate(err_id, std::forward<F>(f));
        }
    };
}

////////////////////////////////////////

namespace detail
{
    inline int current_id() noexcept
    {
        unsigned id = tls::read_current_error_id();
        BOOST_LEAF_ASSERT(id == 0 || (id&3) == 1);
        return int(id);
    }

    inline int new_id() noexcept
    {
        unsigned id = tls::generate_next_error_id();
        tls::write_current_error_id(id);
        return int(id);
    }

    inline int start_new_error() noexcept
    {
        return new_id();
    }

    struct inject_loc
    {
        char const * file;
        int line;
        char const * fn;

        template <class T>
        friend T operator+( inject_loc loc, T && x ) noexcept(!BOOST_LEAF_CFG_CAPTURE)
        {
            x.load_source_location_(loc.file, loc.line, loc.fn);
            return std::move(x);
        }
    };
} // namespace detail

#if BOOST_LEAF_CFG_STD_SYSTEM_ERROR

namespace detail
{
    class leaf_error_category final:
        public std::error_category
    {
        bool equivalent( int,  std::error_condition const & ) const noexcept override { return false; }
        bool equivalent( std::error_code const &, int ) const noexcept override { return false; }
        char const * name() const noexcept override { return "LEAF error"; }
        std::string message( int ) const override { return name(); }
    public:
        ~leaf_error_category() noexcept override { }
    };

    template <class=void>
    struct get_leaf_error_category
    {
        static leaf_error_category cat;
    };

    template <class T>
    leaf_error_category get_leaf_error_category<T>::cat;

    inline int import_error_code( std::error_code const & ec ) noexcept(!BOOST_LEAF_CFG_CAPTURE)
    {
        if( int err_id = ec.value() )
        {
            std::error_category const & cat = get_leaf_error_category<>::cat;
            if( &ec.category() == &cat )
            {
                BOOST_LEAF_ASSERT((err_id&3) == 1);
                return (err_id&~3)|1;
            }
            else
            {
                err_id = start_new_error();
                (void) load_slot(err_id, ec);
                return (err_id&~3)|1;
            }
        }
        else
            return 0;
    }
} // namespace detail

inline bool is_error_id( std::error_code const & ec ) noexcept
{
    bool res = (&ec.category() == &detail::get_leaf_error_category<>::cat);
    BOOST_LEAF_ASSERT(!res || !ec.value() || ((ec.value()&3) == 1));
    return res;
}

#endif // #if BOOST_LEAF_CFG_STD_SYSTEM_ERROR

////////////////////////////////////////

namespace detail
{
    BOOST_LEAF_CONSTEXPR error_id make_error_id(int) noexcept;
}

class error_id
{
    friend error_id BOOST_LEAF_CONSTEXPR detail::make_error_id(int) noexcept;

    int value_;

    BOOST_LEAF_CONSTEXPR explicit error_id( int value ) noexcept:
        value_(value)
    {
        BOOST_LEAF_ASSERT(value_ == 0 || ((value_&3) == 1));
    }

public:

    BOOST_LEAF_CONSTEXPR error_id() noexcept:
        value_(0)
    {
    }

#if BOOST_LEAF_CFG_STD_SYSTEM_ERROR
    explicit error_id( std::error_code const & ec ) noexcept(!BOOST_LEAF_CFG_CAPTURE):
        value_(detail::import_error_code(std::error_code(ec)))
    {
        BOOST_LEAF_ASSERT(!value_ || ((value_&3) == 1));
    }

    template <class Enum>
    error_id( Enum e, typename std::enable_if<std::is_error_code_enum<Enum>::value, int>::type = 0 ) noexcept(!BOOST_LEAF_CFG_CAPTURE):
        value_(detail::import_error_code(e))
    {
    }

    template <class T, typename std::enable_if<std::is_constructible<T, std::error_code>::value, int>::type = 0>
    operator T() const noexcept
    {
        return std::error_code(value_, detail::get_leaf_error_category<>::cat);
    }
#endif // #if BOOST_LEAF_CFG_STD_SYSTEM_ERROR

    BOOST_LEAF_CONSTEXPR error_id load() const noexcept
    {
        return *this;
    }

    template <class Item>
    BOOST_LEAF_CONSTEXPR error_id load(Item && item) const
    {
        if (int err_id = value())
        {
            int const unused[] = { 42, detail::load_item<Item>::load_(err_id, std::forward<Item>(item)) };
            (void)unused;
        }
        return *this;
    }

    template <class... Item>
    BOOST_LEAF_CONSTEXPR error_id load( Item && ... item ) const
    {
        if( int err_id = value() )
        {
            int const unused[] = { 42, detail::load_item<Item>::load_(err_id, std::forward<Item>(item))... };
            (void) unused;
        }
        return *this;
    }

    BOOST_LEAF_CONSTEXPR int value() const noexcept
    {
        BOOST_LEAF_ASSERT(value_ == 0 || ((value_&3) == 1));
        return value_;
    }

    BOOST_LEAF_CONSTEXPR explicit operator bool() const noexcept
    {
        return value_ != 0;
    }

    BOOST_LEAF_CONSTEXPR friend bool operator==( error_id a, error_id b ) noexcept
    {
        return a.value_ == b.value_;
    }

    BOOST_LEAF_CONSTEXPR friend bool operator!=( error_id a, error_id b ) noexcept
    {
        return !(a == b);
    }

    BOOST_LEAF_CONSTEXPR friend bool operator<( error_id a, error_id b ) noexcept
    {
        return a.value_ < b.value_;
    }

    template <class CharT, class Traits>
    friend std::ostream & operator<<( std::basic_ostream<CharT, Traits> & os, error_id x )
    {
        return os << (x.value_ / 4);
    }

    template <class Encoder>
    friend void output( Encoder & e, error_id x )
    {
        output(e, x.value_ / 4);
    }

    BOOST_LEAF_CONSTEXPR void load_source_location_( char const * file, int line, char const * function ) const noexcept(!BOOST_LEAF_CFG_CAPTURE)
    {
        BOOST_LEAF_ASSERT(file&&*file);
        BOOST_LEAF_ASSERT(line>0);
        BOOST_LEAF_ASSERT(function&&*function);
        BOOST_LEAF_ASSERT(value_);
        (void) load(e_source_location {file,line,function});
    }
}; // class error_id

namespace detail
{
    BOOST_LEAF_CONSTEXPR inline error_id make_error_id( int err_id ) noexcept
    {
        BOOST_LEAF_ASSERT(err_id == 0 || (err_id&3) == 1);
        return error_id((err_id&~3)|1);
    }
}

BOOST_LEAF_ATTRIBUTE_NODISCARD inline error_id new_error()
{
    return detail::make_error_id(detail::start_new_error());
}

template <class... Item>
BOOST_LEAF_ATTRIBUTE_NODISCARD inline error_id new_error( Item && ... item )
{
    return detail::make_error_id(detail::start_new_error()).load(std::forward<Item>(item)...);
}

inline error_id current_error() noexcept
{
    return detail::make_error_id(detail::current_id());
}

////////////////////////////////////////

template <class R>
struct is_result_type: std::false_type
{
};

template <class R>
struct is_result_type<R const>: is_result_type<R>
{
};

} } // namespace boost::leaf

#endif // #ifndef BOOST_LEAF_ERROR_HPP_INCLUDED
