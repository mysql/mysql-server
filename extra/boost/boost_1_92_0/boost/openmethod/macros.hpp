// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_OPENMETHOD_MACROS_HPP
#define BOOST_OPENMETHOD_MACROS_HPP

#include <boost/preprocessor/cat.hpp>

#include <boost/openmethod/core.hpp>

namespace boost::openmethod::detail {

template<typename, class Method, typename ReturnType, typename... Parameters>
struct enable_forwarder;

template<class Method, typename ReturnType, typename... Parameters>
struct enable_forwarder<
    std::void_t<decltype(Method::fn(std::declval<Parameters>()...))>, Method,
    ReturnType, Parameters...> {
    using type = ReturnType;
};

template<class...>
struct va_args;

template<class ReturnType, class Registry>
struct va_args<ReturnType, Registry> {
    using return_type = ReturnType;
    using registry = Registry;
};

template<class ReturnType>
struct va_args<ReturnType> {
    using return_type = ReturnType;
    using registry = macro_default_registry;
};

template<typename...>
inline constexpr bool method_not_found = false;

} // namespace boost::openmethod::detail

#define BOOST_OPENMETHOD_GENSYM BOOST_PP_CAT(openmethod_gensym_, __COUNTER__)

#define BOOST_OPENMETHOD_REGISTER(...)                                         \
    static __VA_ARGS__ BOOST_OPENMETHOD_GENSYM

#define BOOST_OPENMETHOD_ID(NAME) NAME##_boost_openmethod

#define BOOST_OPENMETHOD_OVERRIDERS(NAME)                                      \
    BOOST_PP_CAT(BOOST_OPENMETHOD_ID(NAME), _overriders)

#define BOOST_OPENMETHOD_OVERRIDER(NAME, ARGS, ...)                            \
    BOOST_OPENMETHOD_OVERRIDERS(NAME)<__VA_ARGS__ ARGS>

#define BOOST_OPENMETHOD_GUIDE(NAME)                                           \
    BOOST_PP_CAT(BOOST_OPENMETHOD_ID(NAME), _guide)

#define BOOST_OPENMETHOD_TYPE(NAME, ARGS, ...)                                 \
    ::boost::openmethod::method<                                               \
        BOOST_OPENMETHOD_ID(NAME),                                             \
        ::boost::openmethod::detail::va_args<__VA_ARGS__>::return_type ARGS,   \
        ::boost::openmethod::detail::va_args<__VA_ARGS__>::registry>

#define BOOST_OPENMETHOD(NAME, ARGS, ...)                                      \
    struct BOOST_OPENMETHOD_ID(NAME);                                          \
    template<typename... ForwarderParameters>                                  \
    typename ::boost::openmethod::detail::enable_forwarder<                    \
        void, BOOST_OPENMETHOD_TYPE(NAME, ARGS, __VA_ARGS__),                  \
        typename BOOST_OPENMETHOD_TYPE(NAME, ARGS, __VA_ARGS__),               \
        ForwarderParameters...>::type                                          \
        BOOST_OPENMETHOD_GUIDE(NAME)(ForwarderParameters && ... args);         \
    template<typename... ForwarderParameters>                                  \
    inline auto NAME(ForwarderParameters&&... args) ->                         \
        typename ::boost::openmethod::detail::enable_forwarder<                \
            void, BOOST_OPENMETHOD_TYPE(NAME, ARGS, __VA_ARGS__),              \
            ::boost::openmethod::detail::va_args<__VA_ARGS__>::return_type,    \
            ForwarderParameters...>::type {                                    \
        return BOOST_OPENMETHOD_TYPE(NAME, ARGS, __VA_ARGS__)::fn(             \
            std::forward<ForwarderParameters>(args)...);                       \
    }                                                                          \
    template<typename...>                                                      \
    struct BOOST_OPENMETHOD_OVERRIDERS(NAME)

#define BOOST_OPENMETHOD_DETAIL_LOCATE_METHOD(NAME, ARGS)                      \
    template<typename T, typename = void>                                      \
    struct boost_openmethod_detail_locate_method_aux {                         \
        static_assert(                                                         \
            ::boost::openmethod::detail::method_not_found<T>,                  \
            "BOOST_OPENMETHOD_OVERRIDE: cannot find '" #NAME                   \
            "' method that accepts the same arguments as the overrider"); \
    };                                                                         \
    template<typename... A>                                                    \
    struct boost_openmethod_detail_locate_method_aux<                          \
        void(A...),                                                            \
        std::void_t<decltype(BOOST_OPENMETHOD_GUIDE(NAME)(                     \
            std::declval<A>()...))>> {                                         \
        using type =                                                           \
            decltype(BOOST_OPENMETHOD_GUIDE(NAME)(std::declval<A>()...));      \
    }

#define BOOST_OPENMETHOD_DECLARE_OVERRIDER(NAME, ARGS, ...)                    \
    template<typename...>                                                      \
    struct BOOST_OPENMETHOD_OVERRIDERS(NAME);                                  \
    template<>                                                                 \
    struct BOOST_OPENMETHOD_OVERRIDERS(NAME)<__VA_ARGS__ ARGS> {               \
        BOOST_OPENMETHOD_DETAIL_LOCATE_METHOD(NAME, ARGS);                     \
        static auto fn ARGS->__VA_ARGS__;                                      \
        static auto has_next() -> bool;                                        \
        template<typename... Args>                                             \
        static auto next(Args&&... args) -> decltype(auto);                    \
    };                                                                         \
    inline auto BOOST_OPENMETHOD_OVERRIDERS(                                   \
        NAME)<__VA_ARGS__ ARGS>::has_next() -> bool {                          \
        return boost_openmethod_detail_locate_method_aux<                      \
            void ARGS>::type::has_next<fn>();                                  \
    }                                                                          \
    template<typename... Args>                                                 \
    inline auto BOOST_OPENMETHOD_OVERRIDERS(NAME)<__VA_ARGS__ ARGS>::next(     \
        Args&&... args) -> decltype(auto) {                                    \
        return boost_openmethod_detail_locate_method_aux<                      \
            void ARGS>::type::next<fn>(std::forward<Args>(args)...);           \
    }

#define BOOST_OPENMETHOD_DETAIL_REGISTER_OVERRIDER(NAME, ARGS, ...)            \
    BOOST_OPENMETHOD_REGISTER(                                                 \
        BOOST_OPENMETHOD_OVERRIDERS(NAME) < __VA_ARGS__ ARGS >                 \
        ::boost_openmethod_detail_locate_method_aux<void ARGS>::type::         \
            override<                                                          \
                BOOST_OPENMETHOD_OVERRIDERS(NAME) < __VA_ARGS__ ARGS>::fn >);

#define BOOST_OPENMETHOD_DEFINE_OVERRIDER(NAME, ARGS, ...)                     \
    BOOST_OPENMETHOD_DETAIL_REGISTER_OVERRIDER(NAME, ARGS, __VA_ARGS__)        \
    auto BOOST_OPENMETHOD_OVERRIDER(NAME, ARGS, __VA_ARGS__)::fn ARGS          \
        -> boost::mp11::mp_back<boost::mp11::mp_list<__VA_ARGS__>>

#define BOOST_OPENMETHOD_OVERRIDE(NAME, ARGS, ...)                             \
    BOOST_OPENMETHOD_DECLARE_OVERRIDER(NAME, ARGS, __VA_ARGS__)                \
    BOOST_OPENMETHOD_DEFINE_OVERRIDER(NAME, ARGS, __VA_ARGS__)

#define BOOST_OPENMETHOD_INLINE_OVERRIDE(NAME, ARGS, ...)                      \
    BOOST_OPENMETHOD_DECLARE_OVERRIDER(NAME, ARGS, __VA_ARGS__)                \
    BOOST_OPENMETHOD_DETAIL_REGISTER_OVERRIDER(NAME, ARGS, __VA_ARGS__)        \
    inline auto BOOST_OPENMETHOD_OVERRIDER(NAME, ARGS, __VA_ARGS__)::fn ARGS   \
        -> boost::mp11::mp_back<boost::mp11::mp_list<__VA_ARGS__>>

#define BOOST_OPENMETHOD_CLASSES(...)                                          \
    BOOST_OPENMETHOD_REGISTER(::boost::openmethod::use_classes<__VA_ARGS__>)

#endif
