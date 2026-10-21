//
// Copyright (c) 2024 Dmitry Arkhipov (grisumbras@yandex.ru)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/json
//

#ifndef BOOST_JSON_IMPL_SERIALIZER_HPP
#define BOOST_JSON_IMPL_SERIALIZER_HPP

#include <boost/core/detail/static_assert.hpp>
#include <boost/describe/enum_to_string.hpp>
#include <boost/json/conversion.hpp>
#include <cstddef>

namespace boost {
namespace json {
namespace detail {

enum class writer::state : char
{
    str1, str2, str3, esc1, utf1,
    utf2, utf3, utf4, utf5,
    lit,
    arr1, arr2, arr3, arr4,
    obj1, obj2, obj3, obj4, obj5, obj6
};

bool
writer::
suspend(state st)
{
    st_.push(st);
    return false;
}

template<class U, class T>
bool
writer::
suspend(state st, U u, T const* pt)
{
    st_.push(pt);
    st_.push(u);
    st_.push(st);
    return false;
}

template<class T, bool StackEmpty>
bool
write_impl(writer& w, stream& ss);

template<class T, bool StackEmpty>
BOOST_FORCEINLINE
bool
write_impl(null_like_conversion_tag, writer& w, stream& ss)
{
#if defined(_MSC_VER)
# pragma warning( push )
# pragma warning( disable : 4127 )
#endif
    if( StackEmpty || w.st_.empty() )
        return write_null(w, ss);
#if defined(_MSC_VER)
# pragma warning( pop )
#endif
    return resume_buffer(w, ss);
}

template<class T, bool StackEmpty>
BOOST_FORCEINLINE
bool
write_impl(bool_conversion_tag, writer& w, stream& ss)
{
    BOOST_ASSERT( w.p_ );
    auto const t = *reinterpret_cast<T const*>(w.p_);

#if defined(_MSC_VER)
# pragma warning( push )
# pragma warning( disable : 4127 )
#endif
    if( StackEmpty || w.st_.empty() )
#if defined(_MSC_VER)
# pragma warning( pop )
#endif
    {
        if( t )
            return write_true(w, ss);
        else
            return write_false(w, ss);
    }

    return resume_buffer(w, ss);
}

template<class T, bool StackEmpty>
BOOST_FORCEINLINE
bool
write_impl(integral_conversion_tag, writer& w, stream& ss0)
{
#if defined(_MSC_VER)
# pragma warning( push )
# pragma warning( disable : 4127 )
#endif
    if( StackEmpty || w.st_.empty() )
#if defined(_MSC_VER)
# pragma warning( pop )
#endif
    {
        auto const& t = *reinterpret_cast<T const*>(w.p_);

#if defined(__clang__)
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wsign-compare"
#elif defined(__GNUC__)
# pragma GCC diagnostic push
# pragma GCC  diagnostic ignored "-Wsign-compare"
#elif defined(_MSC_VER)
# pragma warning( push )
# pragma warning( disable : 4018 )
# pragma warning( disable : 4127 )
#endif

        if( t < 0 )
        {
            // T is obviously signed, so this comparison is safe
            if( t >= (std::numeric_limits<std::int64_t>::min)() )
            {
                std::int64_t i = t;
                return write_int64(w, ss0, i);
            }
        }
        else if( t <= (std::numeric_limits<std::uint64_t>::max)() )
        {
            std::uint64_t u = t;
            return write_uint64(w, ss0, u);
        }
#if defined(__clang__)
# pragma clang diagnostic pop
#elif defined(__GNUC__)
# pragma GCC diagnostic pop
#elif defined(_MSC_VER)
# pragma warning( pop )
#endif

#if defined(_MSC_VER)
# pragma warning( push )
# pragma warning( disable : 4244 )
#endif
        double d = t;
        return write_double(w, ss0, d);
#if defined(_MSC_VER)
# pragma warning( pop )
#endif
    }

    return resume_buffer(w, ss0);
}

template<class T, bool StackEmpty>
BOOST_FORCEINLINE
bool
write_impl(floating_point_conversion_tag, writer& w, stream& ss0)
{
#if defined(_MSC_VER)
# pragma warning( push )
# pragma warning( disable : 4127 )
#endif
    if( StackEmpty || w.st_.empty() )
#if defined(_MSC_VER)
# pragma warning( pop )
#endif
    {
        double d = *reinterpret_cast<T const*>(w.p_);
        return write_double(w, ss0, d);
    }

    return resume_buffer(w, ss0);
}

template<class T, bool StackEmpty>
BOOST_FORCEINLINE
bool
write_impl(string_like_conversion_tag, writer& w, stream& ss0)
{
#if defined(_MSC_VER)
# pragma warning( push )
# pragma warning( disable : 4127 )
#endif
    if( StackEmpty || w.st_.empty() )
#if defined(_MSC_VER)
# pragma warning( pop )
#endif
    {
        string_view const sv = *reinterpret_cast<T const*>(w.p_);
        w.cs0_ = { sv.data(), sv.size() };
        return write_string(w, ss0);
    }

    return resume_string(w, ss0);
}

template<class T, bool StackEmpty>
BOOST_FORCEINLINE
bool
write_impl(sequence_conversion_tag, writer& w, stream& ss0)
{
    using It = iterator_type<T const>;
    using Elem = value_type<T>;

    T const* pt;
    local_stream ss(ss0);
    It it;
    It end;
#if defined(_MSC_VER)
# pragma warning( push )
# pragma warning( disable : 4127 )
#endif
    if(StackEmpty || w.st_.empty())
    {
#if defined(_MSC_VER)
# pragma warning( pop )
#endif
        BOOST_ASSERT( w.p_ );
        pt = reinterpret_cast<T const*>(w.p_);
        it = std::begin(*pt);
        end = std::end(*pt);
    }
    else
    {
        writer::state st;
        w.st_.pop(st);
        w.st_.pop(it);
        w.st_.pop(pt);
        end = std::end(*pt);
        switch(st)
        {
        default:
        case writer::state::arr1: goto do_arr1;
        case writer::state::arr2: goto do_arr2;
        case writer::state::arr3: goto do_arr3;
        case writer::state::arr4: goto do_arr4;
            break;
        }
    }
do_arr1:
    if(BOOST_JSON_LIKELY(ss))
        ss.append('[');
    else
        return w.suspend(writer::state::arr1, it, pt);
    if(it == end)
        goto do_arr4;
    for(;;)
    {
        w.p_ = std::addressof(*it);
do_arr2:
        if( !write_impl<Elem, StackEmpty>(w, ss) )
            return w.suspend(writer::state::arr2, it, pt);
        if(BOOST_JSON_UNLIKELY( ++it == end ))
            break;
do_arr3:
        if(BOOST_JSON_LIKELY(ss))
            ss.append(',');
        else
            return w.suspend(writer::state::arr3, it, pt);
    }
do_arr4:
    if(BOOST_JSON_LIKELY(ss))
        ss.append(']');
    else
        return w.suspend(writer::state::arr4, it, pt);
    return true;
}

template<class T, bool StackEmpty>
BOOST_FORCEINLINE
bool
write_impl(map_like_conversion_tag, writer& w, stream& ss0)
{
    using It = iterator_type<T const>;
    using Mapped = mapped_type<T>;

    T const* pt;
    local_stream ss(ss0);
    It it;
    It end;
#if defined(_MSC_VER)
# pragma warning( push )
# pragma warning( disable : 4127 )
#endif
    if(StackEmpty || w.st_.empty())
#if defined(_MSC_VER)
# pragma warning( pop )
#endif
    {
        BOOST_ASSERT( w.p_ );
        pt = reinterpret_cast<T const*>(w.p_);
        it = std::begin(*pt);
        end = std::end(*pt);
    }
    else
    {
        writer::state st;
        w.st_.pop(st);
        w.st_.pop(it);
        w.st_.pop(pt);
        end = std::end(*pt);
        switch(st)
        {
        default:
        case writer::state::obj1: goto do_obj1;
        case writer::state::obj2: goto do_obj2;
        case writer::state::obj3: goto do_obj3;
        case writer::state::obj4: goto do_obj4;
        case writer::state::obj5: goto do_obj5;
        case writer::state::obj6: goto do_obj6;
            break;
        }
    }
do_obj1:
    if(BOOST_JSON_LIKELY( ss ))
        ss.append('{');
    else
        return w.suspend(writer::state::obj1, it, pt);
    if(BOOST_JSON_UNLIKELY( it == end ))
        goto do_obj6;
    for(;;)
    {
        {
            using std::get;
            string_view const sv = get<0>(*it);
            w.cs0_ = { sv.data(), sv.size() };
        }
        if( true )
        {
            if(BOOST_JSON_UNLIKELY( !write_string(w, ss) ))
                return w.suspend(writer::state::obj2, it, pt);
        }
        else
        {
do_obj2:
            if(BOOST_JSON_UNLIKELY( !resume_string(w, ss) ))
                return w.suspend(writer::state::obj2, it, pt);
        }
do_obj3:
        if(BOOST_JSON_LIKELY(ss))
            ss.append(':');
        else
            return w.suspend(writer::state::obj3, it, pt);
do_obj4:
        {
            using std::get;
            w.p_ = std::addressof( get<1>(*it) );
        }
        if(BOOST_JSON_UNLIKELY(( !write_impl<Mapped, StackEmpty>(w, ss) )))
            return w.suspend(writer::state::obj4, it, pt);
        ++it;
        if(BOOST_JSON_UNLIKELY(it == end))
            break;
do_obj5:
        if(BOOST_JSON_LIKELY(ss))
            ss.append(',');
        else
            return w.suspend(writer::state::obj5, it, pt);
    }
do_obj6:
    if(BOOST_JSON_LIKELY( ss ))
    {
        ss.append('}');
        return true;
    }
    return w.suspend(writer::state::obj6, it, pt);
}

template< class T, bool StackEmpty >
struct serialize_tuple_elem_helper
{
    writer& w;
    stream& ss;
    T const* pt;

    template< std::size_t I >
    bool
    operator()( std::integral_constant<std::size_t, I> ) const
    {
        using std::get;
        w.p_ = std::addressof( get<I>(*pt) );

        using Elem = tuple_element_t<I, T>;
        return write_impl<Elem, StackEmpty>(w, ss);
    }
};

template<class T, bool StackEmpty>
BOOST_FORCEINLINE
bool
write_impl(tuple_conversion_tag, writer& w, stream& ss0)
{
    T const* pt;
    local_stream ss(ss0);
    std::size_t cur;
    constexpr std::size_t N = std::tuple_size<T>::value;
#if defined(_MSC_VER)
# pragma warning( push )
# pragma warning( disable : 4127 )
#endif
    if(StackEmpty || w.st_.empty())
    {
#if defined(_MSC_VER)
# pragma warning( pop )
#endif
        BOOST_ASSERT( w.p_ );
        pt = reinterpret_cast<T const*>(w.p_);
        cur = 0;
    }
    else
    {
        writer::state st;
        w.st_.pop(st);
        w.st_.pop(cur);
        w.st_.pop(pt);
        switch(st)
        {
        default:
        case writer::state::arr1: goto do_arr1;
        case writer::state::arr2: goto do_arr2;
        case writer::state::arr3: goto do_arr3;
        case writer::state::arr4: goto do_arr4;
            break;
        }
    }
do_arr1:
    if(BOOST_JSON_LIKELY(ss))
        ss.append('[');
    else
        return w.suspend(writer::state::arr1, cur, pt);
    for(;;)
    {
do_arr2:
        {
            bool const stop = !mp11::mp_with_index<N>(
                cur,
                serialize_tuple_elem_helper<T, StackEmpty>{w, ss, pt});
            if(BOOST_JSON_UNLIKELY( stop ))
                return w.suspend(writer::state::arr2, cur, pt);
        }
        if(BOOST_JSON_UNLIKELY( ++cur == N ))
            break;
do_arr3:
        if(BOOST_JSON_LIKELY(ss))
            ss.append(',');
        else
            return w.suspend(writer::state::arr3, cur, pt);
    }
do_arr4:
    if(BOOST_JSON_LIKELY(ss))
        ss.append(']');
    else
        return w.suspend(writer::state::arr4, cur, pt);
    return true;
}

template< class T, bool StackEmpty >
struct serialize_struct_elem_helper
{
    static_assert(
        uniquely_named_members<T>::value,
        "The type has several described members with the same name.");

    writer& w;
    local_stream& ss;
    T const* pt;
    writer::state st;

    template< std::size_t I >
    writer::state
    operator()( std::integral_constant<std::size_t, I> ) const
    {
        using Ds = described_members<T>;
        using D = mp11::mp_at_c<Ds, I>;
        using M = described_member_t<T, D>;

        switch(st)
        {
        case writer::state::obj2: goto do_obj2;
        case writer::state::obj3: goto do_obj3;
        case writer::state::obj4: goto do_obj4;
        default: break;
        }

        {
            string_view const sv = D::name;
            w.cs0_ = { sv.data(), sv.size() };
        }
        if( true )
        {
            if(BOOST_JSON_UNLIKELY( !write_string(w, ss) ))
                return writer::state::obj2;
        }
        else
        {
do_obj2:
            if(BOOST_JSON_UNLIKELY( !resume_string(w, ss) ))
                return writer::state::obj2;
        }
do_obj3:
        if(BOOST_JSON_LIKELY(ss))
            ss.append(':');
        else
            return writer::state::obj3;
do_obj4:
        w.p_ = std::addressof( pt->* D::pointer );
        if(BOOST_JSON_UNLIKELY((
                !write_impl<M, StackEmpty>(w, ss) )))
            return writer::state::obj4;

        return writer::state{};
    }
};

template<class T, bool StackEmpty>
BOOST_FORCEINLINE
bool
write_impl(described_class_conversion_tag, writer& w, stream& ss0)
{
    using Ds = described_members<T>;

    T const* pt;
    local_stream ss(ss0);
    std::size_t cur;
    constexpr std::size_t N = mp11::mp_size<Ds>::value;
    writer::state st;
#if defined(_MSC_VER)
# pragma warning( push )
# pragma warning( disable : 4127 )
#endif
    if(StackEmpty || w.st_.empty())
#if defined(_MSC_VER)
# pragma warning( pop )
#endif
    {
        BOOST_ASSERT( w.p_ );
        pt = reinterpret_cast<T const*>(w.p_);
        cur = 0;
    }
    else
    {
        w.st_.pop(st);
        w.st_.pop(cur);
        w.st_.pop(pt);
        switch(st)
        {
        default:
        case writer::state::obj1: goto do_obj1;
        case writer::state::obj2: // fall through
        case writer::state::obj3: // fall through
        case writer::state::obj4: goto do_obj2;
        case writer::state::obj5: goto do_obj5;
        case writer::state::obj6: goto do_obj6;
            break;
        }
    }
do_obj1:
    if(BOOST_JSON_LIKELY( ss ))
        ss.append('{');
    else
        return w.suspend(writer::state::obj1, cur, pt);
    if(BOOST_JSON_UNLIKELY( cur == N ))
        goto do_obj6;
    for(;;)
    {
        st = {};
do_obj2:
        st = mp11::mp_with_index<N>(
            cur,
            serialize_struct_elem_helper<T, StackEmpty>{w, ss, pt, st});
        if(BOOST_JSON_UNLIKELY( st != writer::state{} ))
            return w.suspend(st, cur, pt);
        ++cur;
        if(BOOST_JSON_UNLIKELY(cur == N))
            break;
do_obj5:
        if(BOOST_JSON_LIKELY(ss))
            ss.append(',');
        else
            return w.suspend(writer::state::obj5, cur, pt);
    }
do_obj6:
    if(BOOST_JSON_LIKELY( ss ))
    {
        ss.append('}');
        return true;
    }
    return w.suspend(writer::state::obj6, cur, pt);
}

template<class T, bool StackEmpty>
BOOST_FORCEINLINE
bool
write_impl(described_enum_conversion_tag, writer& w, stream& ss)
{
#ifdef BOOST_DESCRIBE_CXX14
    using Integer = typename std::underlying_type<T>::type;

#if defined(_MSC_VER)
# pragma warning( push )
# pragma warning( disable : 4127 )
#endif
    if(StackEmpty || w.st_.empty())
#if defined(_MSC_VER)
# pragma warning( pop )
#endif
    {
        BOOST_ASSERT( w.p_ );
        T const* pt = reinterpret_cast<T const*>(w.p_);
        char const* const name = describe::enum_to_string(*pt, nullptr);
        if( name )
        {
            string_view const sv = name;
            w.cs0_ = { sv.data(), sv.size() };
            return write_string(w, ss);
        }
        else
        {
            Integer n = static_cast<Integer>(*pt);
            w.p_ = &n;
            return write_impl<Integer, true>(w, ss);
        }
    }
    else
    {
        writer::state st;
        w.st_.peek(st);
        if( st == writer::state::lit )
            return write_impl<Integer, false>(w, ss);
        else
            return resume_string(w, ss);
    }
#else // BOOST_DESCRIBE_CXX14
    (void)w;
    (void)ss;
    static_assert(
        !std::is_same<T, T>::value,
        "described enums require C++14 support");
    return false;
#endif // BOOST_DESCRIBE_CXX14
}

template< class T, bool StackEmpty >
struct serialize_variant_elem_helper
{
    writer& w;
    stream& ss;

    template<class Elem>
    bool
    operator()(Elem const& x) const
    {
        w.p_ = std::addressof(x);
        return write_impl<Elem, true>(w, ss);
    }
};

template< class T >
struct serialize_variant_elem_helper<T, false>
{
    writer& w;
    stream& ss;

    template< std::size_t I >
    bool
    operator()( std::integral_constant<std::size_t, I> ) const
    {
        using std::get;
        using Elem = remove_cvref<decltype(get<I>(
            std::declval<T const&>() ))>;
        return write_impl<Elem, false>(w, ss);
    }
};

template<class T, bool StackEmpty>
BOOST_FORCEINLINE
bool
write_impl(variant_conversion_tag, writer& w, stream& ss)
{
    T const* pt;

    using Index = remove_cvref<decltype( pt->index() )>;

#if defined(_MSC_VER)
# pragma warning( push )
# pragma warning( disable : 4127 )
#endif
    if(StackEmpty || w.st_.empty())
#if defined(_MSC_VER)
# pragma warning( pop )
#endif
    {
        BOOST_ASSERT( w.p_ );
        pt = reinterpret_cast<T const*>(w.p_);
        if(BOOST_JSON_LIKELY((
                visit(serialize_variant_elem_helper<T, true>{w, ss}, *pt))))
            return true;

        Index const ix = pt->index();
        w.st_.push(ix);
        return false;
    }
    else
    {
        Index ix;
        w.st_.pop(ix);

        constexpr std::size_t N = mp11::mp_size<T>::value;
        if(BOOST_JSON_LIKELY(( mp11::mp_with_index<N>(
                ix,
                serialize_variant_elem_helper<T, false>{w, ss}))))
            return true;

        w.st_.push(ix);
        return false;
    }
}

template<class T, bool StackEmpty>
BOOST_FORCEINLINE
bool
write_impl(optional_conversion_tag, writer& w, stream& ss)
{
    using Elem = value_result_type<T>;

    bool done;
    bool has_value;

#if defined(_MSC_VER)
# pragma warning( push )
# pragma warning( disable : 4127 )
#endif
    if(StackEmpty || w.st_.empty())
#if defined(_MSC_VER)
# pragma warning( pop )
#endif
    {
        BOOST_ASSERT( w.p_ );
        T const* pt = reinterpret_cast<T const*>(w.p_);
        has_value = static_cast<bool>(*pt);
        if( has_value )
        {
            w.p_ = std::addressof( *(*pt) );
            done = write_impl<Elem, true>(w, ss);
        }
        else
        {
            w.p_ = nullptr;
            done = write_impl<std::nullptr_t, true>(w, ss);;
        }
    }
    else
    {
        w.st_.pop(has_value);

        if( has_value )
            done = write_impl<Elem, false>(w, ss);
        else
            done = write_impl<std::nullptr_t, false>(w, ss);
    }

    if(BOOST_JSON_UNLIKELY( !done ))
        w.st_.push(has_value);

    return done;
}

template<class T, bool StackEmpty>
BOOST_FORCEINLINE
bool
write_impl(path_conversion_tag, writer& w, stream& ss)
{
#if defined(_MSC_VER)
# pragma warning( push )
# pragma warning( disable : 4127 )
#endif
    if(StackEmpty || w.st_.empty())
#if defined(_MSC_VER)
# pragma warning( pop )
#endif
    {
        BOOST_ASSERT( w.p_ );
        T const* pt = reinterpret_cast<T const*>(w.p_);

        std::string const s = pt->generic_string();
        w.cs0_ = { s.data(), s.size() };
        if(BOOST_JSON_LIKELY( write_string(w, ss) ))
            return true;

        std::size_t const used = w.cs0_.used( s.data() );
        w.st_.push( used );
        w.st_.push( std::move(s) );
        return false;
    }
    else
    {
        std::string s;
        std::size_t used;
        w.st_.pop( s );
        w.st_.pop( used );

        w.cs0_ = { s.data(), s.size() };
        w.cs0_.skip(used);

        if(BOOST_JSON_LIKELY( resume_string(w, ss) ))
            return true;

        used = w.cs0_.used( s.data() );
        w.st_.push( used );
        w.st_.push( std::move(s) );
        return false;
    }
}

template<class T, bool StackEmpty>
bool
write_impl(writer& w, stream& ss)
{
    using cat = detail::generic_conversion_category<T>;
    return write_impl<T, StackEmpty>( cat(), w, ss );
}

} // namespace detail

template<class T>
void
serializer::reset(T const* p) noexcept
{
    BOOST_CORE_STATIC_ASSERT( !std::is_pointer<T>::value );
    BOOST_CORE_STATIC_ASSERT( std::is_object<T>::value );

    p_ = p;
    fn0_ = &detail::write_impl<T, true>;
    fn1_ = &detail::write_impl<T, false>;
    st_.clear();
    done_ = false;
}

} // namespace json
} // namespace boost

#endif // BOOST_JSON_IMPL_SERIALIZER_HPP
