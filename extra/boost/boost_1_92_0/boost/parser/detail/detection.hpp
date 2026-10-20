// Copyright (C) 2020 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_PARSER_DETAIL_DETECTION_HPP
#define BOOST_PARSER_DETAIL_DETECTION_HPP

#include <type_traits>


namespace boost::parser::detail {

    template<typename...>
    struct void_
    {
        using type = void;
        static constexpr bool value = true;
    };

    template<typename... T>
    using void_t = typename void_<T...>::type;

    template<typename T>
    struct fixup_ptr
    {
        using type = T;
    };

    template<typename T>
    using remove_v_t = typename std::remove_volatile<T>::type;

    template<typename T>
    struct fixup_ptr<T *>
    {
        using type = remove_v_t<T> const *;
    };

    template<typename T>
    using fixup_ptr_t = typename fixup_ptr<T>::type;

    template<typename T>
    using remove_cv_ref_t =
        typename std::remove_cv<typename std::remove_reference<T>::type>::type;

    struct nonesuch
    {};

    template<
        typename Default,
        typename AlwaysVoid,
        template<typename...> class Template,
        typename... Args>
    struct detector
    {
        using value_t = std::false_type;
        using type = Default;
    };

    template<
        typename Default,
        template<typename...> class Template,
        typename... Args>
    struct detector<Default, void_t<Template<Args...>>, Template, Args...>
    {
        using value_t = std::true_type;
        using type = Template<Args...>;
    };

    template<template<typename...> class Template, typename... Args>
    using is_detected =
        typename detector<nonesuch, void, Template, Args...>::value_t;

    template<template<typename...> class Template, typename... Args>
    constexpr bool is_detected_v = is_detected<Template, Args...>::value;

    template<template<typename...> class Template, typename... Args>
    using detected_t =
        typename detector<nonesuch, void, Template, Args...>::type;

    template<
        typename Default,
        template<typename...> class Template,
        typename... Args>
    using detected_or_t =
        typename detector<Default, void, Template, Args...>::type;

}

#endif
