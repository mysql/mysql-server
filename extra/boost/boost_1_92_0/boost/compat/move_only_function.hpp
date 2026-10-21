#ifndef BOOST_COMPAT_MOVE_ONLY_FUNCTION_HPP_INCLUDED
#define BOOST_COMPAT_MOVE_ONLY_FUNCTION_HPP_INCLUDED

// Copyright 2025 Christian Mazakas.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/compat/invoke.hpp>
#include <boost/compat/type_traits.hpp>
#include <boost/assert.hpp>

#include <cstddef>
#include <initializer_list>
#include <type_traits>

#include <boost/config/workaround.hpp>

#if BOOST_WORKAROUND(BOOST_GCC, >= 6 * 10000)
#   pragma GCC diagnostic push
#   pragma GCC diagnostic ignored "-Wnonnull-compare"
#   pragma GCC diagnostic ignored "-Waddress"
#endif

namespace boost {
namespace compat {

template<class... S>
class move_only_function;

template< class T >
struct in_place_type_t { explicit in_place_type_t() = default; };

namespace detail
{

union pointers
{
    void* pobj_;
    void ( *pfn_ )();
};

struct storage
{
    // we want SBO to be large enough to store a type which can be used for delegation purposes
    struct delegate { void( storage::*pmfn_ )(); storage* pobj_; };

    union
    {
        void* pobj_;
        void ( *pfn_ )();
        alignas(delegate) unsigned char buf_[ sizeof(delegate) ];
    };

    template<class T>
    constexpr static bool use_sbo() noexcept
    {
        return sizeof( T ) <= sizeof( storage ) && alignof( T ) <= alignof( storage ) && std::is_nothrow_move_constructible<T>::value;
    }

    void* addr() noexcept
    {
        return buf_;
    }
};

template<class>
struct is_polymorphic_function : std::false_type
{
};

template<class R, class ...Args>
struct is_polymorphic_function<move_only_function<R( Args... )>> : std::true_type
{
};

template<class R, class ...Args>
struct is_polymorphic_function<move_only_function<R( Args... ) &>> : std::true_type
{
};

template<class R, class ...Args>
struct is_polymorphic_function<move_only_function<R( Args... ) &&>> : std::true_type
{
};

template<class R, class ...Args>
struct is_polymorphic_function<move_only_function<R( Args... ) const>> : std::true_type
{
};

template<class R, class ...Args>
struct is_polymorphic_function<move_only_function<R( Args... ) const&>> : std::true_type
{
};

template<class R, class ...Args>
struct is_polymorphic_function<move_only_function<R( Args... ) const&&>> : std::true_type
{
};

#if defined(__cpp_noexcept_function_type)

template<class R, class ...Args>
struct is_polymorphic_function<move_only_function<R( Args... ) noexcept>> : std::true_type
{
};

template<class R, class ...Args>
struct is_polymorphic_function<move_only_function<R( Args... ) & noexcept>> : std::true_type
{
};

template<class R, class ...Args>
struct is_polymorphic_function<move_only_function<R( Args... ) && noexcept>> : std::true_type
{
};

template<class R, class ...Args>
struct is_polymorphic_function<move_only_function<R( Args... ) const noexcept>> : std::true_type
{
};

template<class R, class ...Args>
struct is_polymorphic_function<move_only_function<R( Args... ) const& noexcept>> : std::true_type
{
};

template<class R, class ...Args>
struct is_polymorphic_function<move_only_function<R( Args... ) const&& noexcept>> : std::true_type
{
};

#endif

template<class T>
using is_move_only_function = is_polymorphic_function<T>;

template<class T>
struct is_in_place_type_t : std::false_type
{
};

template<class T>
struct is_in_place_type_t<in_place_type_t<T>> : std::true_type
{
};

template<class T, class ...Args>
struct nothrow_init
{
    constexpr static bool const value = ( storage::use_sbo<T>() && std::is_nothrow_constructible<T, Args...>::value ) || false;
};

enum class ref_quals { none, lvalue, rvalue };

template<ref_quals RQ, bool Const, bool NoEx, class VT, class R, class ...Args>
struct is_callable_from;

template<ref_quals RQ, bool Const, class VT, class R, class ...Args>
struct is_callable_from<RQ, Const, true, VT, R, Args...>
{
    using cv_VT = conditional_t<Const, add_const_t<VT>, VT>;

    using cv_ref_VT = conditional_t<
        RQ == ref_quals::none, cv_VT,
        conditional_t<
            RQ == ref_quals::rvalue, add_rvalue_reference_t<cv_VT>, add_lvalue_reference_t<cv_VT>
        >
    >;

    using inv_quals_VT = conditional_t<
        RQ == ref_quals::none, add_lvalue_reference_t<cv_VT>,
        conditional_t<
            RQ == ref_quals::rvalue, add_rvalue_reference_t<cv_VT>, add_lvalue_reference_t<cv_VT>
        >
    >;

    constexpr static bool const value =
        is_nothrow_invocable_r<R, cv_ref_VT, Args...>::value &&
        is_nothrow_invocable_r<R, inv_quals_VT, Args...>::value;
};

template<ref_quals RQ, bool Const, class VT, class R, class ...Args>
struct is_callable_from<RQ, Const, false, VT, R, Args...>
{
    using cv_VT = conditional_t<Const, add_const_t<VT>, VT>;

    using cv_ref_VT = conditional_t<
        RQ == ref_quals::none, cv_VT,
        conditional_t<
            RQ == ref_quals::rvalue, add_rvalue_reference_t<cv_VT>, add_lvalue_reference_t<cv_VT>
        >
    >;

    using inv_quals_VT = conditional_t<
        RQ == ref_quals::none, add_lvalue_reference_t<cv_VT>,
        conditional_t<
            RQ == ref_quals::rvalue, add_rvalue_reference_t<cv_VT>, add_lvalue_reference_t<cv_VT>
        >
    >;

    constexpr static bool const value =
        is_invocable_r<R, cv_ref_VT, Args...>::value &&
        is_invocable_r<R, inv_quals_VT, Args...>::value;
};

inline std::nullptr_t get_first_arg()
{
    return nullptr;
}

template<class T, class ...CArgs>
T&& get_first_arg( T&& t, CArgs&& ... )
{
    return std::forward<T>( t );
}

template<class ...Ts>
bool is_nullary_arg( Ts&&... )
{
    return false;
}

template<
    class F, class VT = decay_t<F>,
    enable_if_t<
        std::is_member_pointer<VT>::value ||
        is_move_only_function<VT>::value,
        int> = 0
>
bool is_nullary_arg( F&& f )
{
    return f == nullptr;
}

template<
    class F, class VT = decay_t<F>,
    enable_if_t<
        std::is_function<remove_pointer_t<VT>>::value,
        int> = 0
>
bool is_nullary_arg( F f )
{
    return f == nullptr;
}

template<bool NoEx, class R, class ...Args>
struct mo_invoke_function_holder
{
    static R invoke_function( storage const& s, Args&&... args) noexcept( NoEx )
    {
        auto f = reinterpret_cast<R(*)( Args... )>( s.pfn_ );
        return compat::invoke_r<R>( f, std::forward<Args>( args )... );
    }
};

template<ref_quals RQ, bool Const, bool NoEx, class F, class R, class ...Args>
struct mo_invoke_object_holder
{
    static R invoke_object( storage const& s, Args&&... args ) noexcept( NoEx )
    {
        using T = remove_reference_t<F>;
        using cv_T = conditional_t<Const, add_const_t<T>, T>;
        using cv_ref_T = conditional_t<
            RQ == ref_quals::none, add_lvalue_reference_t<cv_T>,
            conditional_t<
                RQ == ref_quals::rvalue, add_rvalue_reference_t<cv_T>, add_lvalue_reference_t<cv_T>
            >
        >;
        return compat::invoke_r<R>( static_cast<cv_ref_T>( *static_cast<cv_T*>( s.pobj_ ) ), std::forward<Args>( args )... );
    }
};

template<ref_quals RQ, bool Const, bool NoEx, class F, class R, class ...Args>
struct mo_invoke_local_holder
{
    static R invoke_local( storage const& s, Args&&... args ) noexcept( NoEx )
    {
        using T = remove_reference_t<F>;
        using cv_T = conditional_t<Const, add_const_t<T>, T>;
        using cv_ref_T = conditional_t<
            RQ == ref_quals::none, add_lvalue_reference_t<cv_T>,
            conditional_t<
                RQ == ref_quals::rvalue, add_rvalue_reference_t<cv_T>, add_lvalue_reference_t<cv_T>
            >
        >;

        return compat::invoke_r<R>( static_cast<cv_ref_T>( *static_cast<cv_T*>( const_cast<storage&>( s ).addr() ) ), std::forward<Args>( args )... );
    }
};

enum class op_type { move, destroy };

template <ref_quals RQ, bool Const, bool NoEx, class R, class ...Args>
struct move_only_function_base
{
    move_only_function_base() = default;

    move_only_function_base( move_only_function_base&& rhs ) noexcept
    {
        manager_ = rhs.manager_;
        manager_( op_type::move, s_, &rhs.s_ );

        invoke_ = rhs.invoke_;
        rhs.invoke_ = nullptr;
        rhs.manager_ = &manage_empty;
    }

    ~move_only_function_base()
    {
        destroy();
    }

    void swap( move_only_function_base& rhs ) noexcept
    {
        // to properly swap with storages, we need to treat the destination storage
        // the same as the source storage, which means that we need to use the
        // source manager_'s move operation

        storage s;
        rhs.manager_( op_type::move, s, &rhs.s_ );
        manager_( op_type::move, rhs.s_, &s_ );
        rhs.manager_( op_type::move, s_, &s );

        std::swap( manager_, rhs.manager_ );
        std::swap( invoke_, rhs.invoke_ );
    }

    move_only_function_base& operator=( move_only_function_base&& rhs )
    {
        destroy();

        manager_ = rhs.manager_;
        manager_( op_type::move, s_, &rhs.s_ );
        invoke_ = rhs.invoke_;

        rhs.invoke_ = nullptr;
        rhs.manager_ = &manage_empty;
        return *this;
    }

    move_only_function_base& operator=( std::nullptr_t ) noexcept
    {
        destroy();
        invoke_ = nullptr;
        manager_ = &manage_empty;
        return *this;
    }

    static void manage_empty( op_type, detail::storage&, detail::storage* )
    {
    }

    static void manage_function( op_type op, detail::storage& s, detail::storage* src )
    {
        switch( op )
        {
            case op_type::move:
                s.pfn_ = src->pfn_;
                src->pfn_ = nullptr;
                break;

            default:
                break;
        }

    }

    template<class VT>
    static void manage_object( op_type op, detail::storage& s, detail::storage* src )
    {
        switch( op )
        {
            case op_type::destroy:
                delete static_cast<VT*>( s.pobj_ );
                break;

            case op_type::move:
                s.pobj_ = src->pobj_;
                src->pobj_ = nullptr;
                break;

            default:
                break;
        }
    }

    template<class VT>
    static void manage_local( op_type op, detail::storage& s, detail::storage* src )
    {
        switch( op )
        {
            case op_type::destroy:
                static_cast<VT*>( s.addr() )->~VT();
                break;

            case op_type::move:
            {
                VT* p = static_cast<VT*>( src->addr() );
                ::new( s.addr() ) VT( std::move( *p ) );
                // destruct the element here because move construction will leave the container empty
                // outside of this function
                p->~VT();
                break;
            }

            default:
                break;
        }
    }

    template <ref_quals RQ2, bool Const2, bool NoEx2, class R2, class ...Args2>
    void
    move_from_compatible_base( move_only_function_base<RQ2, Const2, NoEx2, R2, Args2...>& base )
    {
        using polymorphic_base = move_only_function_base<RQ2, Const2, NoEx2, R2, Args2...>;

        manager_ = base.manager_;

        manager_( op_type::move, s_, &base.s_ );
        invoke_ = base.invoke_;

        base.invoke_ = nullptr;
        base.manager_ = &polymorphic_base::manage_empty;
    }

    template<class F>
    void base_init( std::true_type, F&& f )
    {
        move_from_compatible_base( f );
    }

    template<class ...F>
    void base_init( std::false_type, F&&... )
    {
    }

    template<class VT, class ...CArgs>
    void init_object( std::false_type /* use_sbo */, CArgs&& ...args )
    {
        s_.pobj_ = new VT( std::forward<CArgs>( args )... );
        invoke_ = &mo_invoke_object_holder<RQ, Const, NoEx, VT, R, Args...>::invoke_object;
        manager_ = &manage_object<VT>;
    }

    template<class VT, class ...CArgs>
    void init_object( std::true_type /* use_sbo */, CArgs&& ...args )
    {
        ::new( s_.addr() ) VT( std::forward<CArgs>( args )... );
        invoke_ = &mo_invoke_local_holder<RQ, Const, NoEx, VT, R, Args...>::invoke_local;
        manager_ = &manage_local<VT>;
    }

    template<class VT, class ...CArgs>
    void init( std::false_type /* is_function */, CArgs&& ...args )
    {
        if( is_polymorphic_function<VT>::value )
        {
            base_init( is_polymorphic_function<VT>{}, std::forward<CArgs>( args )... );
            return;
        }

        init_object<VT>( std::integral_constant<bool, storage::use_sbo<VT>()>{}, std::forward<CArgs>( args )... );
    }

    template<class VT, class ...CArgs>
    void init( std::true_type /* is_function */, CArgs ...args )
    {
        R (*pfn)( Args... ) = get_first_arg( args... );
        s_.pfn_ = reinterpret_cast<void(*)()>( pfn );
        invoke_ = &detail::mo_invoke_function_holder<NoEx, R, Args...>::invoke_function;
        manager_ = &manage_function;
    }

    template <class VT, class ...CArgs>
    void init( type_identity<VT>, CArgs&& ...args )
    {
        init<VT>( std::is_function<remove_pointer_t<VT>>(), std::forward<CArgs>( args )... );
    }

    void destroy()
    {
        manager_( op_type::destroy, s_, nullptr );
    }

    explicit operator bool() const noexcept
    {
        return invoke_ != nullptr;
    }

    detail::storage s_;
#if defined(__cpp_noexcept_function_type)
    R ( *invoke_ )( detail::storage const&, Args&&... ) noexcept( NoEx ) = nullptr;
#else
    R ( *invoke_ )( detail::storage const&, Args&&... ) = nullptr;
#endif
    void ( *manager_ )( op_type, detail::storage&, detail::storage* ) = &manage_empty;
};

} // namespace detail

template<class R, class ...Args>
class move_only_function<R( Args... )> : detail::move_only_function_base<detail::ref_quals::none, false, false, R, Args...>
{
private:

    template<detail::ref_quals, bool, bool, class, class ...>
    friend struct detail::move_only_function_base;

    using base = detail::move_only_function_base<detail::ref_quals::none, false, false, R, Args...>;

public:

    move_only_function() noexcept
    {
    }

    move_only_function( std::nullptr_t ) noexcept
        : move_only_function()
    {
    }

    template<
        class F,
        class VT = decay_t<F>,
        enable_if_t<
            !std::is_same<move_only_function, remove_cvref_t<F>>::value &&
            !detail::is_in_place_type_t<VT>::value &&
            detail::is_callable_from<detail::ref_quals::none, false, false, VT, R, Args...>::value,
            int> = 0
    >
    move_only_function( F&& f ) noexcept( detail::nothrow_init<VT, F>::value )
    {
        if( detail::is_nullary_arg( std::forward<F>( f ) ) ) return;
        base::init( type_identity<VT>{}, std::forward<F>( f ) );
    }

    template<
        class T, class ...CArgs,
        enable_if_t<
            std::is_constructible<T, CArgs...>::value &&
            detail::is_callable_from<detail::ref_quals::none, false, false, T, R, Args...>::value,
            int> = 0
    >
    explicit move_only_function( in_place_type_t<T>, CArgs&& ... args ) noexcept( detail::nothrow_init<T, CArgs...>::value )
    {
        static_assert( std::is_same<T, decay_t<T>>::value, "T and `decay_t<T>` must be the same" );
        base::init( type_identity<T>{}, std::forward<CArgs>( args )... );
    }

    template<
        class T, class U, class ...CArgs,
        enable_if_t<
            std::is_constructible<T, std::initializer_list<U>&, CArgs...>::value &&
            detail::is_callable_from<detail::ref_quals::none, false, false, T, R, Args...>::value,
            int> = 0
    >
    explicit move_only_function( in_place_type_t<T>, std::initializer_list<U> il, CArgs&& ... args ) noexcept( detail::nothrow_init<T, std::initializer_list<U>&, CArgs...>::value )
    {
        static_assert( std::is_same<T, decay_t<T>>::value, "T and `decay_t<T>` must be the same" );
        base::init( type_identity<T>{}, il, std::forward<CArgs>( args )... );
    }

    move_only_function( move_only_function const& ) = delete;
    move_only_function( move_only_function&& ) = default;

    ~move_only_function() = default;

    move_only_function& operator=( move_only_function&& rhs )
    {
        if( this != &rhs )
        {
            this->base::operator=( static_cast<base&&>( rhs ) );
        }
        return *this;
    }

    move_only_function& operator=( std::nullptr_t ) noexcept
    {
        this->base::operator=( nullptr );
        return *this;
    }

    template<class F> move_only_function& operator=( F&& f )
    {
        move_only_function( std::forward<F>( f ) ).swap( *this );
        return *this;
    }

    friend bool operator==( move_only_function const& fn, std::nullptr_t ) noexcept
    {
        return fn.invoke_ == nullptr;
    }

    friend bool operator!=( move_only_function const& fn, std::nullptr_t ) noexcept
    {
        return !( fn == nullptr );
    }

    void swap( move_only_function& rhs ) noexcept
    {
        if( this != &rhs )
        {
            this->base::swap( rhs );
        }
    }

    friend void swap( move_only_function& lhs, move_only_function& rhs ) noexcept
    {
        lhs.swap( rhs );
    }

    explicit operator bool() const noexcept
    {
        return static_cast<bool>( *static_cast<base const*>( this ) );
    }

    R operator()( Args... args )
    {
        return this->invoke_( this->s_, std::forward<Args>( args )... );
    }
};

template<class R, class ...Args>
class move_only_function<R( Args... ) &> : detail::move_only_function_base<detail::ref_quals::lvalue, false, false, R, Args...>
{
private:

    template<detail::ref_quals, bool, bool, class, class ...>
    friend struct detail::move_only_function_base;

    using base = detail::move_only_function_base<detail::ref_quals::lvalue, false, false, R, Args...>;

public:

    move_only_function() noexcept
    {
    }

    move_only_function( std::nullptr_t ) noexcept
        : move_only_function()
    {
    }

    template<
        class F,
        class VT = decay_t<F>,
        enable_if_t<
            !std::is_same<move_only_function, remove_cvref_t<F>>::value &&
            !detail::is_in_place_type_t<VT>::value &&
            detail::is_callable_from<detail::ref_quals::lvalue, false, false, VT, R, Args...>::value,
            int> = 0
    >
    move_only_function( F&& f ) noexcept( detail::nothrow_init<VT, F>::value )
    {
        if( detail::is_nullary_arg( std::forward<F>( f ) ) ) return;
        base::init( type_identity<VT>{}, std::forward<F>( f ) );
    }

    template<
        class T, class ...CArgs,
        enable_if_t<
            std::is_constructible<T, CArgs...>::value &&
            detail::is_callable_from<detail::ref_quals::lvalue, false, false, T, R, Args...>::value,
            int> = 0
    >
    explicit move_only_function( in_place_type_t<T>, CArgs&& ... args ) noexcept( detail::nothrow_init<T, CArgs...>::value )
    {
        static_assert( std::is_same<T, decay_t<T>>::value, "T and `decay_t<T>` must be the same" );
        base::init( type_identity<T>{}, std::forward<CArgs>( args )... );
    }

    template<
        class T, class U, class ...CArgs,
        enable_if_t<
            std::is_constructible<T, std::initializer_list<U>&, CArgs...>::value &&
            detail::is_callable_from<detail::ref_quals::lvalue, false, false, T, R, Args...>::value,
            int> = 0
    >
    explicit move_only_function( in_place_type_t<T>, std::initializer_list<U> il, CArgs&& ... args ) noexcept( detail::nothrow_init<T, std::initializer_list<U>&, CArgs...>::value )
    {
        static_assert( std::is_same<T, decay_t<T>>::value, "T and `decay_t<T>` must be the same" );
        base::init( type_identity<T>{}, il, std::forward<CArgs>( args )... );
    }

    move_only_function( move_only_function const& ) = delete;
    move_only_function( move_only_function&& ) = default;

    ~move_only_function() = default;

    move_only_function& operator=( move_only_function&& rhs )
    {
        if( this != &rhs )
        {
            this->base::operator=( static_cast<base&&>( rhs ) );
        }
        return *this;
    }

    move_only_function& operator=( std::nullptr_t ) noexcept
    {
        this->base::operator=( nullptr );
        return *this;
    }

    template<class F> move_only_function& operator=( F&& f )
    {
        move_only_function( std::forward<F>( f ) ).swap( *this );
        return *this;
    }

    friend bool operator==( move_only_function const& fn, std::nullptr_t ) noexcept
    {
        return fn.invoke_ == nullptr;
    }

    friend bool operator!=( move_only_function const& fn, std::nullptr_t ) noexcept
    {
        return !( fn == nullptr );
    }

    void swap( move_only_function& rhs ) noexcept
    {
        if( this != &rhs )
        {
            this->base::swap( rhs );
        }
    }

    friend void swap( move_only_function& lhs, move_only_function& rhs ) noexcept
    {
        lhs.swap( rhs );
    }

    explicit operator bool() const noexcept
    {
        return static_cast<bool>( *static_cast<base const*>( this ) );
    }

    R operator()( Args... args ) &
    {
        return this->invoke_( this->s_, std::forward<Args>( args )... );
    }
};

template<class R, class ...Args>
class move_only_function<R( Args... ) &&> : detail::move_only_function_base<detail::ref_quals::rvalue, false, false, R, Args...>
{
private:

    template<detail::ref_quals, bool, bool, class, class ...>
    friend struct detail::move_only_function_base;

    using base = detail::move_only_function_base<detail::ref_quals::rvalue, false, false, R, Args...>;

public:

    move_only_function() noexcept
    {
    }

    move_only_function( std::nullptr_t ) noexcept
        : move_only_function()
    {
    }

    template<
        class F,
        class VT = decay_t<F>,
        enable_if_t<
            !std::is_same<move_only_function, remove_cvref_t<F>>::value &&
            !detail::is_in_place_type_t<VT>::value &&
            detail::is_callable_from<detail::ref_quals::rvalue, false, false, VT, R, Args...>::value,
            int> = 0
    >
    move_only_function( F&& f ) noexcept( detail::nothrow_init<VT, F>::value )
    {
        if( detail::is_nullary_arg( std::forward<F>( f ) ) ) return;
        base::init( type_identity<VT>{}, std::forward<F>( f ) );
    }

    template<
        class T, class ...CArgs,
        enable_if_t<
            std::is_constructible<T, CArgs...>::value &&
            detail::is_callable_from<detail::ref_quals::rvalue, false, false, T, R, Args...>::value,
            int> = 0
    >
    explicit move_only_function( in_place_type_t<T>, CArgs&& ... args ) noexcept( detail::nothrow_init<T, CArgs...>::value )
    {
        static_assert( std::is_same<T, decay_t<T>>::value, "T and `decay_t<T>` must be the same" );
        base::init( type_identity<T>{}, std::forward<CArgs>( args )... );
    }

    template<
        class T, class U, class ...CArgs,
        enable_if_t<
            std::is_constructible<T, std::initializer_list<U>&, CArgs...>::value &&
            detail::is_callable_from<detail::ref_quals::rvalue, false, false, T, R, Args...>::value,
            int> = 0
    >
    explicit move_only_function( in_place_type_t<T>, std::initializer_list<U> il, CArgs&& ... args ) noexcept( detail::nothrow_init<T, std::initializer_list<U>&, CArgs...>::value )
    {
        static_assert( std::is_same<T, decay_t<T>>::value, "T and `decay_t<T>` must be the same" );
        base::init( type_identity<T>{}, il, std::forward<CArgs>( args )... );
    }

    move_only_function( move_only_function const& ) = delete;
    move_only_function( move_only_function&& ) = default;

    ~move_only_function() = default;

    move_only_function& operator=( move_only_function&& rhs )
    {
        if( this != &rhs )
        {
            this->base::operator=( static_cast<base&&>( rhs ) );
        }
        return *this;
    }

    move_only_function& operator=( std::nullptr_t ) noexcept
    {
        this->base::operator=( nullptr );
        return *this;
    }

    template<class F> move_only_function& operator=( F&& f )
    {
        move_only_function( std::forward<F>( f ) ).swap( *this );
        return *this;
    }

    friend bool operator==( move_only_function const& fn, std::nullptr_t ) noexcept
    {
        return fn.invoke_ == nullptr;
    }

    friend bool operator!=( move_only_function const& fn, std::nullptr_t ) noexcept
    {
        return !( fn == nullptr );
    }

    void swap( move_only_function& rhs ) noexcept
    {
        if( this != &rhs )
        {
            this->base::swap( rhs );
        }
    }

    friend void swap( move_only_function& lhs, move_only_function& rhs ) noexcept
    {
        lhs.swap( rhs );
    }

    explicit operator bool() const noexcept
    {
        return static_cast<bool>( *static_cast<base const*>( this ) );
    }

    R operator()( Args... args ) &&
    {
        return this->invoke_( this->s_, std::forward<Args>( args )... );
    }
};

template<class R, class ...Args>
class move_only_function<R( Args... ) const> : detail::move_only_function_base<detail::ref_quals::none, true, false, R, Args...>
{
private:

    template<detail::ref_quals, bool, bool, class, class ...>
    friend struct detail::move_only_function_base;

    using base = detail::move_only_function_base<detail::ref_quals::none, true, false, R, Args...>;

public:

    move_only_function() noexcept
    {
    }

    move_only_function( std::nullptr_t ) noexcept
        : move_only_function()
    {
    }

    template<
        class F,
        class VT = decay_t<F>,
        enable_if_t<
            !std::is_same<move_only_function, remove_cvref_t<F>>::value &&
            !detail::is_in_place_type_t<VT>::value &&
            detail::is_callable_from<detail::ref_quals::none, true, false, VT, R, Args...>::value,
            int> = 0
    >
    move_only_function( F&& f ) noexcept( detail::nothrow_init<VT, F>::value )
    {
        if( detail::is_nullary_arg( std::forward<F>( f ) ) ) return;
        base::init( type_identity<VT>{}, std::forward<F>( f ) );
    }

    template<
        class T, class ...CArgs,
        enable_if_t<
            std::is_constructible<T, CArgs...>::value &&
            detail::is_callable_from<detail::ref_quals::none, true, false, T, R, Args...>::value,
            int> = 0
    >
    explicit move_only_function( in_place_type_t<T>, CArgs&& ... args ) noexcept( detail::nothrow_init<T, CArgs...>::value )
    {
        static_assert( std::is_same<T, decay_t<T>>::value, "T and `decay_t<T>` must be the same" );
        base::init( type_identity<T>{}, std::forward<CArgs>( args )... );
    }

    template<
        class T, class U, class ...CArgs,
        enable_if_t<
            std::is_constructible<T, std::initializer_list<U>&, CArgs...>::value &&
            detail::is_callable_from<detail::ref_quals::none, true, false, T, R, Args...>::value,
            int> = 0
    >
    explicit move_only_function( in_place_type_t<T>, std::initializer_list<U> il, CArgs&& ... args ) noexcept( detail::nothrow_init<T, std::initializer_list<U>&, CArgs...>::value )
    {
        static_assert( std::is_same<T, decay_t<T>>::value, "T and `decay_t<T>` must be the same" );
        base::init( type_identity<T>{}, il, std::forward<CArgs>( args )... );
    }

    move_only_function( move_only_function const& ) = delete;
    move_only_function( move_only_function&& ) = default;

    ~move_only_function() = default;

    move_only_function& operator=( move_only_function&& rhs )
    {
        if( this != &rhs )
        {
            this->base::operator=( static_cast<base&&>( rhs ) );
        }
        return *this;
    }

    move_only_function& operator=( std::nullptr_t ) noexcept
    {
        this->base::operator=( nullptr );
        return *this;
    }

    template<class F> move_only_function& operator=( F&& f )
    {
        move_only_function( std::forward<F>( f ) ).swap( *this );
        return *this;
    }

    friend bool operator==( move_only_function const& fn, std::nullptr_t ) noexcept
    {
        return fn.invoke_ == nullptr;
    }

    friend bool operator!=( move_only_function const& fn, std::nullptr_t ) noexcept
    {
        return !( fn == nullptr );
    }

    void swap( move_only_function& rhs ) noexcept
    {
        if( this != &rhs )
        {
            this->base::swap( rhs );
        }
    }

    friend void swap( move_only_function& lhs, move_only_function& rhs ) noexcept
    {
        lhs.swap( rhs );
    }

    explicit operator bool() const noexcept
    {
        return static_cast<bool>( *static_cast<base const*>( this ) );
    }

    R operator()( Args... args ) const
    {
        return this->invoke_( this->s_, std::forward<Args>( args )... );
    }
};

template<class R, class ...Args>
class move_only_function<R( Args... ) const&> : detail::move_only_function_base<detail::ref_quals::lvalue, true, false, R, Args...>
{
private:

    template<detail::ref_quals, bool, bool, class, class ...>
    friend struct detail::move_only_function_base;

    using base = detail::move_only_function_base<detail::ref_quals::lvalue, true, false, R, Args...>;

public:

    move_only_function() noexcept
    {
    }

    move_only_function( std::nullptr_t ) noexcept
        : move_only_function()
    {
    }

    template<
        class F,
        class VT = decay_t<F>,
        enable_if_t<
            !std::is_same<move_only_function, remove_cvref_t<F>>::value &&
            !detail::is_in_place_type_t<VT>::value &&
            detail::is_callable_from<detail::ref_quals::lvalue, true, false, VT, R, Args...>::value,
            int> = 0
    >
    move_only_function( F&& f ) noexcept( detail::nothrow_init<VT, F>::value )
    {
        if( detail::is_nullary_arg( std::forward<F>( f ) ) ) return;
        base::init( type_identity<VT>{}, std::forward<F>( f ) );
    }

    template<
        class T, class ...CArgs,
        enable_if_t<
            std::is_constructible<T, CArgs...>::value &&
            detail::is_callable_from<detail::ref_quals::lvalue, true, false, T, R, Args...>::value,
            int> = 0
    >
    explicit move_only_function( in_place_type_t<T>, CArgs&& ... args ) noexcept( detail::nothrow_init<T, CArgs...>::value )
    {
        static_assert( std::is_same<T, decay_t<T>>::value, "T and `decay_t<T>` must be the same" );
        base::init( type_identity<T>{}, std::forward<CArgs>( args )... );
    }

    template<
        class T, class U, class ...CArgs,
        enable_if_t<
            std::is_constructible<T, std::initializer_list<U>&, CArgs...>::value &&
            detail::is_callable_from<detail::ref_quals::lvalue, true, false, T, R, Args...>::value,
            int> = 0
    >
    explicit move_only_function( in_place_type_t<T>, std::initializer_list<U> il, CArgs&& ... args ) noexcept( detail::nothrow_init<T, std::initializer_list<U>&, CArgs...>::value )
    {
        static_assert( std::is_same<T, decay_t<T>>::value, "T and `decay_t<T>` must be the same" );
        base::init( type_identity<T>{}, il, std::forward<CArgs>( args )... );
    }

    move_only_function( move_only_function const& ) = delete;
    move_only_function( move_only_function&& ) = default;

    ~move_only_function() = default;

    move_only_function& operator=( move_only_function&& rhs )
    {
        if( this != &rhs )
        {
            this->base::operator=( static_cast<base&&>( rhs ) );
        }
        return *this;
    }

    move_only_function& operator=( std::nullptr_t ) noexcept
    {
        this->base::operator=( nullptr );
        return *this;
    }

    template<class F> move_only_function& operator=( F&& f )
    {
        move_only_function( std::forward<F>( f ) ).swap( *this );
        return *this;
    }

    friend bool operator==( move_only_function const& fn, std::nullptr_t ) noexcept
    {
        return fn.invoke_ == nullptr;
    }

    friend bool operator!=( move_only_function const& fn, std::nullptr_t ) noexcept
    {
        return !( fn == nullptr );
    }

    void swap( move_only_function& rhs ) noexcept
    {
        if( this != &rhs )
        {
            this->base::swap( rhs );
        }
    }

    friend void swap( move_only_function& lhs, move_only_function& rhs ) noexcept
    {
        lhs.swap( rhs );
    }

    explicit operator bool() const noexcept
    {
        return static_cast<bool>( *static_cast<base const*>( this ) );
    }

    R operator()( Args... args ) const &
    {
        return this->invoke_( this->s_, std::forward<Args>( args )... );
    }
};

template<class R, class ...Args>
class move_only_function<R( Args... ) const&&> : detail::move_only_function_base<detail::ref_quals::rvalue, true, false, R, Args...>
{
private:

    template<detail::ref_quals, bool, bool, class, class ...>
    friend struct detail::move_only_function_base;

    using base = detail::move_only_function_base<detail::ref_quals::rvalue, true, false, R, Args...>;

public:

    move_only_function() noexcept
    {
    }

    move_only_function( std::nullptr_t ) noexcept
        : move_only_function()
    {
    }

    template<
        class F,
        class VT = decay_t<F>,
        enable_if_t<
            !std::is_same<move_only_function, remove_cvref_t<F>>::value &&
            !detail::is_in_place_type_t<VT>::value &&
            detail::is_callable_from<detail::ref_quals::rvalue, true, false, VT, R, Args...>::value,
            int> = 0
    >
    move_only_function( F&& f ) noexcept( detail::nothrow_init<VT, F>::value )
    {
        if( detail::is_nullary_arg( std::forward<F>( f ) ) ) return;
        base::init( type_identity<VT>{}, std::forward<F>( f ) );
    }

    template<
        class T, class ...CArgs,
        enable_if_t<
            std::is_constructible<T, CArgs...>::value &&
            detail::is_callable_from<detail::ref_quals::rvalue, true, false, T, R, Args...>::value,
            int> = 0
    >
    explicit move_only_function( in_place_type_t<T>, CArgs&& ... args ) noexcept( detail::nothrow_init<T, CArgs...>::value )
    {
        static_assert( std::is_same<T, decay_t<T>>::value, "T and `decay_t<T>` must be the same" );
        base::init( type_identity<T>{}, std::forward<CArgs>( args )... );
    }

    template<
        class T, class U, class ...CArgs,
        enable_if_t<
            std::is_constructible<T, std::initializer_list<U>&, CArgs...>::value &&
            detail::is_callable_from<detail::ref_quals::rvalue, true, false, T, R, Args...>::value,
            int> = 0
    >
    explicit move_only_function( in_place_type_t<T>, std::initializer_list<U> il, CArgs&& ... args ) noexcept( detail::nothrow_init<T, std::initializer_list<U>&, CArgs...>::value )
    {
        static_assert( std::is_same<T, decay_t<T>>::value, "T and `decay_t<T>` must be the same" );
        base::init( type_identity<T>{}, il, std::forward<CArgs>( args )... );
    }

    move_only_function( move_only_function const& ) = delete;
    move_only_function( move_only_function&& ) = default;

    ~move_only_function() = default;

    move_only_function& operator=( move_only_function&& rhs )
    {
        if( this != &rhs )
        {
            this->base::operator=( static_cast<base&&>( rhs ) );
        }
        return *this;
    }

    move_only_function& operator=( std::nullptr_t ) noexcept
    {
        this->base::operator=( nullptr );
        return *this;
    }

    template<class F> move_only_function& operator=( F&& f )
    {
        move_only_function( std::forward<F>( f ) ).swap( *this );
        return *this;
    }

    friend bool operator==( move_only_function const& fn, std::nullptr_t ) noexcept
    {
        return fn.invoke_ == nullptr;
    }

    friend bool operator!=( move_only_function const& fn, std::nullptr_t ) noexcept
    {
        return !( fn == nullptr );
    }

    void swap( move_only_function& rhs ) noexcept
    {
        if( this != &rhs )
        {
            this->base::swap( rhs );
        }
    }

    friend void swap( move_only_function& lhs, move_only_function& rhs ) noexcept
    {
        lhs.swap( rhs );
    }

    explicit operator bool() const noexcept
    {
        return static_cast<bool>( *static_cast<base const*>( this ) );
    }

    R operator()( Args... args ) const &&
    {
        return this->invoke_( this->s_, std::forward<Args>( args )... );
    }
};

#if defined(__cpp_noexcept_function_type)

template<class R, class ...Args>
class move_only_function<R( Args... ) noexcept> : detail::move_only_function_base<detail::ref_quals::none, false, true, R, Args...>
{
private:

    template<detail::ref_quals, bool, bool, class, class ...>
    friend struct detail::move_only_function_base;

    using base = detail::move_only_function_base<detail::ref_quals::none, false, true, R, Args...>;

public:

    move_only_function() noexcept
    {
    }

    move_only_function( std::nullptr_t ) noexcept
        : move_only_function()
    {
    }

    template<
        class F,
        class VT = decay_t<F>,
        enable_if_t<
            !std::is_same<move_only_function, remove_cvref_t<F>>::value &&
            !detail::is_in_place_type_t<VT>::value &&
            detail::is_callable_from<detail::ref_quals::none, false, true, VT, R, Args...>::value,
            int> = 0
    >
    move_only_function( F&& f ) noexcept( detail::nothrow_init<VT, F>::value )
    {
        if( detail::is_nullary_arg( std::forward<F>( f ) ) ) return;
        base::init( type_identity<VT>{}, std::forward<F>( f ) );
    }

    template<
        class T, class ...CArgs,
        enable_if_t<
            std::is_constructible<T, CArgs...>::value &&
            detail::is_callable_from<detail::ref_quals::none, false, true, T, R, Args...>::value,
            int> = 0
    >
    explicit move_only_function( in_place_type_t<T>, CArgs&& ... args ) noexcept( detail::nothrow_init<T, CArgs...>::value )
    {
        static_assert( std::is_same<T, decay_t<T>>::value, "T and `decay_t<T>` must be the same" );
        base::init( type_identity<T>{}, std::forward<CArgs>( args )... );
    }

    template<
        class T, class U, class ...CArgs,
        enable_if_t<
            std::is_constructible<T, std::initializer_list<U>&, CArgs...>::value &&
            detail::is_callable_from<detail::ref_quals::none, false, true, T, R, Args...>::value,
            int> = 0
    >
    explicit move_only_function( in_place_type_t<T>, std::initializer_list<U> il, CArgs&& ... args ) noexcept( detail::nothrow_init<T, std::initializer_list<U>&, CArgs...>::value )
    {
        static_assert( std::is_same<T, decay_t<T>>::value, "T and `decay_t<T>` must be the same" );
        base::init( type_identity<T>{}, il, std::forward<CArgs>( args )... );
    }

    move_only_function( move_only_function const& ) = delete;
    move_only_function( move_only_function&& ) = default;

    ~move_only_function() = default;

    move_only_function& operator=( move_only_function&& rhs )
    {
        if( this != &rhs )
        {
            this->base::operator=( static_cast<base&&>( rhs ) );
        }
        return *this;
    }

    move_only_function& operator=( std::nullptr_t ) noexcept
    {
        this->base::operator=( nullptr );
        return *this;
    }

    template<class F> move_only_function& operator=( F&& f )
    {
        move_only_function( std::forward<F>( f ) ).swap( *this );
        return *this;
    }

    friend bool operator==( move_only_function const& fn, std::nullptr_t ) noexcept
    {
        return fn.invoke_ == nullptr;
    }

    friend bool operator!=( move_only_function const& fn, std::nullptr_t ) noexcept
    {
        return !( fn == nullptr );
    }

    void swap( move_only_function& rhs ) noexcept
    {
        if( this != &rhs )
        {
            this->base::swap( rhs );
        }
    }

    friend void swap( move_only_function& lhs, move_only_function& rhs ) noexcept
    {
        lhs.swap( rhs );
    }

    explicit operator bool() const noexcept
    {
        return static_cast<bool>( *static_cast<base const*>( this ) );
    }

    R operator()( Args... args ) noexcept
    {
        return this->invoke_( this->s_, std::forward<Args>( args )... );
    }
};

template<class R, class ...Args>
class move_only_function<R( Args... ) & noexcept> : detail::move_only_function_base<detail::ref_quals::lvalue, false, true, R, Args...>
{
private:

    template<detail::ref_quals, bool, bool, class, class ...>
    friend struct detail::move_only_function_base;

    using base = detail::move_only_function_base<detail::ref_quals::lvalue, false, true, R, Args...>;

public:

    move_only_function() noexcept
    {
    }

    move_only_function( std::nullptr_t ) noexcept
        : move_only_function()
    {
    }

    template<
        class F,
        class VT = decay_t<F>,
        enable_if_t<
            !std::is_same<move_only_function, remove_cvref_t<F>>::value &&
            !detail::is_in_place_type_t<VT>::value &&
            detail::is_callable_from<detail::ref_quals::lvalue, false, true, VT, R, Args...>::value,
            int> = 0
    >
    move_only_function( F&& f ) noexcept( detail::nothrow_init<VT, F>::value )
    {
        if( detail::is_nullary_arg( std::forward<F>( f ) ) ) return;
        base::init( type_identity<VT>{}, std::forward<F>( f ) );
    }

    template<
        class T, class ...CArgs,
        enable_if_t<
            std::is_constructible<T, CArgs...>::value &&
            detail::is_callable_from<detail::ref_quals::lvalue, false, true, T, R, Args...>::value,
            int> = 0
    >
    explicit move_only_function( in_place_type_t<T>, CArgs&& ... args ) noexcept( detail::nothrow_init<T, CArgs...>::value )
    {
        static_assert( std::is_same<T, decay_t<T>>::value, "T and `decay_t<T>` must be the same" );
        base::init( type_identity<T>{}, std::forward<CArgs>( args )... );
    }

    template<
        class T, class U, class ...CArgs,
        enable_if_t<
            std::is_constructible<T, std::initializer_list<U>&, CArgs...>::value &&
            detail::is_callable_from<detail::ref_quals::lvalue, false, true, T, R, Args...>::value,
            int> = 0
    >
    explicit move_only_function( in_place_type_t<T>, std::initializer_list<U> il, CArgs&& ... args ) noexcept( detail::nothrow_init<T, std::initializer_list<U>&, CArgs...>::value )
    {
        static_assert( std::is_same<T, decay_t<T>>::value, "T and `decay_t<T>` must be the same" );
        base::init( type_identity<T>{}, il, std::forward<CArgs>( args )... );
    }

    move_only_function( move_only_function const& ) = delete;
    move_only_function( move_only_function&& ) = default;

    ~move_only_function() = default;

    move_only_function& operator=( move_only_function&& rhs )
    {
        if( this != &rhs )
        {
            this->base::operator=( static_cast<base&&>( rhs ) );
        }
        return *this;
    }

    move_only_function& operator=( std::nullptr_t ) noexcept
    {
        this->base::operator=( nullptr );
        return *this;
    }

    template<class F> move_only_function& operator=( F&& f )
    {
        move_only_function( std::forward<F>( f ) ).swap( *this );
        return *this;
    }

    friend bool operator==( move_only_function const& fn, std::nullptr_t ) noexcept
    {
        return fn.invoke_ == nullptr;
    }

    friend bool operator!=( move_only_function const& fn, std::nullptr_t ) noexcept
    {
        return !( fn == nullptr );
    }

    void swap( move_only_function& rhs ) noexcept
    {
        if( this != &rhs )
        {
            this->base::swap( rhs );
        }
    }

    friend void swap( move_only_function& lhs, move_only_function& rhs ) noexcept
    {
        lhs.swap( rhs );
    }

    explicit operator bool() const noexcept
    {
        return static_cast<bool>( *static_cast<base const*>( this ) );
    }

    R operator()( Args... args ) & noexcept
    {
        return this->invoke_( this->s_, std::forward<Args>( args )... );
    }
};

template<class R, class ...Args>
class move_only_function<R( Args... ) && noexcept> : detail::move_only_function_base<detail::ref_quals::rvalue, false, true, R, Args...>
{
private:

    template<detail::ref_quals, bool, bool, class, class ...>
    friend struct detail::move_only_function_base;

    using base = detail::move_only_function_base<detail::ref_quals::rvalue, false, true, R, Args...>;

public:

    move_only_function() noexcept
    {
    }

    move_only_function( std::nullptr_t ) noexcept
        : move_only_function()
    {
    }

    template<
        class F,
        class VT = decay_t<F>,
        enable_if_t<
            !std::is_same<move_only_function, remove_cvref_t<F>>::value &&
            !detail::is_in_place_type_t<VT>::value &&
            detail::is_callable_from<detail::ref_quals::rvalue, false, true, VT, R, Args...>::value,
            int> = 0
    >
    move_only_function( F&& f ) noexcept( detail::nothrow_init<VT, F>::value )
    {
        if( detail::is_nullary_arg( std::forward<F>( f ) ) ) return;
        base::init( type_identity<VT>{}, std::forward<F>( f ) );
    }

    template<
        class T, class ...CArgs,
        enable_if_t<
            std::is_constructible<T, CArgs...>::value &&
            detail::is_callable_from<detail::ref_quals::rvalue, false, true, T, R, Args...>::value,
            int> = 0
    >
    explicit move_only_function( in_place_type_t<T>, CArgs&& ... args ) noexcept( detail::nothrow_init<T, CArgs...>::value )
    {
        static_assert( std::is_same<T, decay_t<T>>::value, "T and `decay_t<T>` must be the same" );
        base::init( type_identity<T>{}, std::forward<CArgs>( args )... );
    }

    template<
        class T, class U, class ...CArgs,
        enable_if_t<
            std::is_constructible<T, std::initializer_list<U>&, CArgs...>::value &&
            detail::is_callable_from<detail::ref_quals::rvalue, false, true, T, R, Args...>::value,
            int> = 0
    >
    explicit move_only_function( in_place_type_t<T>, std::initializer_list<U> il, CArgs&& ... args ) noexcept( detail::nothrow_init<T, std::initializer_list<U>&, CArgs...>::value )
    {
        static_assert( std::is_same<T, decay_t<T>>::value, "T and `decay_t<T>` must be the same" );
        base::init( type_identity<T>{}, il, std::forward<CArgs>( args )... );
    }

    move_only_function( move_only_function const& ) = delete;
    move_only_function( move_only_function&& ) = default;

    ~move_only_function() = default;

    move_only_function& operator=( move_only_function&& rhs )
    {
        if( this != &rhs )
        {
            this->base::operator=( static_cast<base&&>( rhs ) );
        }
        return *this;
    }

    move_only_function& operator=( std::nullptr_t ) noexcept
    {
        this->base::operator=( nullptr );
        return *this;
    }

    template<class F> move_only_function& operator=( F&& f )
    {
        move_only_function( std::forward<F>( f ) ).swap( *this );
        return *this;
    }

    friend bool operator==( move_only_function const& fn, std::nullptr_t ) noexcept
    {
        return fn.invoke_ == nullptr;
    }

    friend bool operator!=( move_only_function const& fn, std::nullptr_t ) noexcept
    {
        return !( fn == nullptr );
    }

    void swap( move_only_function& rhs ) noexcept
    {
        if( this != &rhs )
        {
            this->base::swap( rhs );
        }
    }

    friend void swap( move_only_function& lhs, move_only_function& rhs ) noexcept
    {
        lhs.swap( rhs );
    }

    explicit operator bool() const noexcept
    {
        return static_cast<bool>( *static_cast<base const*>( this ) );
    }

    R operator()( Args... args ) && noexcept
    {
        return this->invoke_( this->s_, std::forward<Args>( args )... );
    }
};

template<class R, class ...Args>
class move_only_function<R( Args... ) const noexcept> : detail::move_only_function_base<detail::ref_quals::none, true, true, R, Args...>
{
private:

    template<detail::ref_quals, bool, bool, class, class ...>
    friend struct detail::move_only_function_base;

    using base = detail::move_only_function_base<detail::ref_quals::none, true, true, R, Args...>;

public:

    move_only_function() noexcept
    {
    }

    move_only_function( std::nullptr_t ) noexcept
        : move_only_function()
    {
    }

    template<
        class F,
        class VT = decay_t<F>,
        enable_if_t<
            !std::is_same<move_only_function, remove_cvref_t<F>>::value &&
            !detail::is_in_place_type_t<VT>::value &&
            detail::is_callable_from<detail::ref_quals::none, true, true, VT, R, Args...>::value,
            int> = 0
    >
    move_only_function( F&& f ) noexcept( detail::nothrow_init<VT, F>::value )
    {
        if( detail::is_nullary_arg( std::forward<F>( f ) ) ) return;
        base::init( type_identity<VT>{}, std::forward<F>( f ) );
    }

    template<
        class T, class ...CArgs,
        enable_if_t<
            std::is_constructible<T, CArgs...>::value &&
            detail::is_callable_from<detail::ref_quals::none, true, true, T, R, Args...>::value,
            int> = 0
    >
    explicit move_only_function( in_place_type_t<T>, CArgs&& ... args ) noexcept( detail::nothrow_init<T, CArgs...>::value )
    {
        static_assert( std::is_same<T, decay_t<T>>::value, "T and `decay_t<T>` must be the same" );
        base::init( type_identity<T>{}, std::forward<CArgs>( args )... );
    }

    template<
        class T, class U, class ...CArgs,
        enable_if_t<
            std::is_constructible<T, std::initializer_list<U>&, CArgs...>::value &&
            detail::is_callable_from<detail::ref_quals::none, true, true, T, R, Args...>::value,
            int> = 0
    >
    explicit move_only_function( in_place_type_t<T>, std::initializer_list<U> il, CArgs&& ... args ) noexcept( detail::nothrow_init<T, std::initializer_list<U>&, CArgs...>::value )
    {
        static_assert( std::is_same<T, decay_t<T>>::value, "T and `decay_t<T>` must be the same" );
        base::init( type_identity<T>{}, il, std::forward<CArgs>( args )... );
    }

    move_only_function( move_only_function const& ) = delete;
    move_only_function( move_only_function&& ) = default;

    ~move_only_function() = default;

    move_only_function& operator=( move_only_function&& rhs )
    {
        if( this != &rhs )
        {
            this->base::operator=( static_cast<base&&>( rhs ) );
        }
        return *this;
    }

    move_only_function& operator=( std::nullptr_t ) noexcept
    {
        this->base::operator=( nullptr );
        return *this;
    }

    template<class F> move_only_function& operator=( F&& f )
    {
        move_only_function( std::forward<F>( f ) ).swap( *this );
        return *this;
    }

    friend bool operator==( move_only_function const& fn, std::nullptr_t ) noexcept
    {
        return fn.invoke_ == nullptr;
    }

    friend bool operator!=( move_only_function const& fn, std::nullptr_t ) noexcept
    {
        return !( fn == nullptr );
    }

    void swap( move_only_function& rhs ) noexcept
    {
        if( this != &rhs )
        {
            this->base::swap( rhs );
        }
    }

    friend void swap( move_only_function& lhs, move_only_function& rhs ) noexcept
    {
        lhs.swap( rhs );
    }

    explicit operator bool() const noexcept
    {
        return static_cast<bool>( *static_cast<base const*>( this ) );
    }

    R operator()( Args... args ) const noexcept
    {
        return this->invoke_( this->s_, std::forward<Args>( args )... );
    }
};

template<class R, class ...Args>
class move_only_function<R( Args... ) const& noexcept> : detail::move_only_function_base<detail::ref_quals::lvalue, true, true, R, Args...>
{
private:

    template<detail::ref_quals, bool, bool, class, class ...>
    friend struct detail::move_only_function_base;

    using base = detail::move_only_function_base<detail::ref_quals::lvalue, true, true, R, Args...>;

public:

    move_only_function() noexcept
    {
    }

    move_only_function( std::nullptr_t ) noexcept
        : move_only_function()
    {
    }

    template<
        class F,
        class VT = decay_t<F>,
        enable_if_t<
            !std::is_same<move_only_function, remove_cvref_t<F>>::value &&
            !detail::is_in_place_type_t<VT>::value &&
            detail::is_callable_from<detail::ref_quals::lvalue, true, true, VT, R, Args...>::value,
            int> = 0
    >
    move_only_function( F&& f ) noexcept( detail::nothrow_init<VT, F>::value )
    {
        if( detail::is_nullary_arg( std::forward<F>( f ) ) ) return;
        base::init( type_identity<VT>{}, std::forward<F>( f ) );
    }

    template<
        class T, class ...CArgs,
        enable_if_t<
            std::is_constructible<T, CArgs...>::value &&
            detail::is_callable_from<detail::ref_quals::lvalue, true, true, T, R, Args...>::value,
            int> = 0
    >
    explicit move_only_function( in_place_type_t<T>, CArgs&& ... args ) noexcept( detail::nothrow_init<T, CArgs...>::value )
    {
        static_assert( std::is_same<T, decay_t<T>>::value, "T and `decay_t<T>` must be the same" );
        base::init( type_identity<T>{}, std::forward<CArgs>( args )... );
    }

    template<
        class T, class U, class ...CArgs,
        enable_if_t<
            std::is_constructible<T, std::initializer_list<U>&, CArgs...>::value &&
            detail::is_callable_from<detail::ref_quals::lvalue, true, true, T, R, Args...>::value,
            int> = 0
    >
    explicit move_only_function( in_place_type_t<T>, std::initializer_list<U> il, CArgs&& ... args ) noexcept( detail::nothrow_init<T, std::initializer_list<U>&, CArgs...>::value )
    {
        static_assert( std::is_same<T, decay_t<T>>::value, "T and `decay_t<T>` must be the same" );
        base::init( type_identity<T>{}, il, std::forward<CArgs>( args )... );
    }

    move_only_function( move_only_function const& ) = delete;
    move_only_function( move_only_function&& ) = default;

    ~move_only_function() = default;

    move_only_function& operator=( move_only_function&& rhs )
    {
        if( this != &rhs )
        {
            this->base::operator=( static_cast<base&&>( rhs ) );
        }
        return *this;
    }

    move_only_function& operator=( std::nullptr_t ) noexcept
    {
        this->base::operator=( nullptr );
        return *this;
    }

    template<class F> move_only_function& operator=( F&& f )
    {
        move_only_function( std::forward<F>( f ) ).swap( *this );
        return *this;
    }

    friend bool operator==( move_only_function const& fn, std::nullptr_t ) noexcept
    {
        return fn.invoke_ == nullptr;
    }

    friend bool operator!=( move_only_function const& fn, std::nullptr_t ) noexcept
    {
        return !( fn == nullptr );
    }

    void swap( move_only_function& rhs ) noexcept
    {
        if( this != &rhs )
        {
            this->base::swap( rhs );
        }
    }

    friend void swap( move_only_function& lhs, move_only_function& rhs ) noexcept
    {
        lhs.swap( rhs );
    }

    explicit operator bool() const noexcept
    {
        return static_cast<bool>( *static_cast<base const*>( this ) );
    }

    R operator()( Args... args ) const& noexcept
    {
        return this->invoke_( this->s_, std::forward<Args>( args )... );
    }
};

template<class R, class ...Args>
class move_only_function<R( Args... ) const&& noexcept> : detail::move_only_function_base<detail::ref_quals::rvalue, true, true, R, Args...>
{
private:

    template<detail::ref_quals, bool, bool, class, class ...>
    friend struct detail::move_only_function_base;

    using base = detail::move_only_function_base<detail::ref_quals::rvalue, true, true, R, Args...>;

public:

    move_only_function() noexcept
    {
    }

    move_only_function( std::nullptr_t ) noexcept
        : move_only_function()
    {
    }

    template<
        class F,
        class VT = decay_t<F>,
        enable_if_t<
            !std::is_same<move_only_function, remove_cvref_t<F>>::value &&
            !detail::is_in_place_type_t<VT>::value &&
            detail::is_callable_from<detail::ref_quals::rvalue, true, true, VT, R, Args...>::value,
            int> = 0
    >
    move_only_function( F&& f ) noexcept( detail::nothrow_init<VT, F>::value )
    {
        if( detail::is_nullary_arg( std::forward<F>( f ) ) ) return;
        base::init( type_identity<VT>{}, std::forward<F>( f ) );
    }

    template<
        class T, class ...CArgs,
        enable_if_t<
            std::is_constructible<T, CArgs...>::value &&
            detail::is_callable_from<detail::ref_quals::rvalue, true, true, T, R, Args...>::value,
            int> = 0
    >
    explicit move_only_function( in_place_type_t<T>, CArgs&& ... args ) noexcept( detail::nothrow_init<T, CArgs...>::value )
    {
        static_assert( std::is_same<T, decay_t<T>>::value, "T and `decay_t<T>` must be the same" );
        base::init( type_identity<T>{}, std::forward<CArgs>( args )... );
    }

    template<
        class T, class U, class ...CArgs,
        enable_if_t<
            std::is_constructible<T, std::initializer_list<U>&, CArgs...>::value &&
            detail::is_callable_from<detail::ref_quals::rvalue, true, true, T, R, Args...>::value,
            int> = 0
    >
    explicit move_only_function( in_place_type_t<T>, std::initializer_list<U> il, CArgs&& ... args ) noexcept( detail::nothrow_init<T, std::initializer_list<U>&, CArgs...>::value )
    {
        static_assert( std::is_same<T, decay_t<T>>::value, "T and `decay_t<T>` must be the same" );
        base::init( type_identity<T>{}, il, std::forward<CArgs>( args )... );
    }

    move_only_function( move_only_function const& ) = delete;
    move_only_function( move_only_function&& ) = default;

    ~move_only_function() = default;

    move_only_function& operator=( move_only_function&& rhs )
    {
        if( this != &rhs )
        {
            this->base::operator=( static_cast<base&&>( rhs ) );
        }
        return *this;
    }

    move_only_function& operator=( std::nullptr_t ) noexcept
    {
        this->base::operator=( nullptr );
        return *this;
    }

    template<class F> move_only_function& operator=( F&& f )
    {
        move_only_function( std::forward<F>( f ) ).swap( *this );
        return *this;
    }

    friend bool operator==( move_only_function const& fn, std::nullptr_t ) noexcept
    {
        return fn.invoke_ == nullptr;
    }

    friend bool operator!=( move_only_function const& fn, std::nullptr_t ) noexcept
    {
        return !( fn == nullptr );
    }

    void swap( move_only_function& rhs ) noexcept
    {
        if( this != &rhs )
        {
            this->base::swap( rhs );
        }
    }

    friend void swap( move_only_function& lhs, move_only_function& rhs ) noexcept
    {
        lhs.swap( rhs );
    }

    explicit operator bool() const noexcept
    {
        return static_cast<bool>( *static_cast<base const*>( this ) );
    }

    R operator()( Args... args ) const&& noexcept
    {
        return this->invoke_( this->s_, std::forward<Args>( args )... );
    }
};

#endif

} // namespace compat
} // namespace boost

#if BOOST_WORKAROUND(BOOST_GCC, >= 6 * 10000)
#   pragma GCC diagnostic pop
#endif

#endif // #ifndef BOOST_COMPAT_MOVE_ONLY_FUNCTION_HPP_INCLUDED
