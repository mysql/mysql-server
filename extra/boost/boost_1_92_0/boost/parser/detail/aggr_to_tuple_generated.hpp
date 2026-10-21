// Copyright (c) 2023 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// Warning: This header is auto-generated (see misc/generate_aggr_to_tuple.py).
// The lack of include guards is intentional.

namespace boost::parser::detail {


template<> struct tie_aggregate_impl<1> {
template<typename T> static constexpr auto call(T & x) {
auto & [
    _01
] = x;
return parser::tuple<
    decltype(_01) &
>(
    _01
);
}
};


template<> struct tie_aggregate_impl<2> {
template<typename T> static constexpr auto call(T & x) {
auto & [
    _01, _02
] = x;
return parser::tuple<
    decltype(_01) &, decltype(_02) &
>(
    _01, _02
);
}
};


template<> struct tie_aggregate_impl<3> {
template<typename T> static constexpr auto call(T & x) {
auto & [
    _01, _02, _03
] = x;
return parser::tuple<
    decltype(_01) &, decltype(_02) &, decltype(_03) &
>(
    _01, _02, _03
);
}
};


template<> struct tie_aggregate_impl<4> {
template<typename T> static constexpr auto call(T & x) {
auto & [
    _01, _02, _03, _04
] = x;
return parser::tuple<
    decltype(_01) &, decltype(_02) &, decltype(_03) &, decltype(_04) &
>(
    _01, _02, _03, _04
);
}
};


template<> struct tie_aggregate_impl<5> {
template<typename T> static constexpr auto call(T & x) {
auto & [
    _01, _02, _03, _04, _05
] = x;
return parser::tuple<
    decltype(_01) &, decltype(_02) &, decltype(_03) &, decltype(_04) &, decltype(_05) &
>(
    _01, _02, _03, _04, _05
);
}
};


template<> struct tie_aggregate_impl<6> {
template<typename T> static constexpr auto call(T & x) {
auto & [
    _01, _02, _03, _04, _05, _06
] = x;
return parser::tuple<
    decltype(_01) &, decltype(_02) &, decltype(_03) &, decltype(_04) &, decltype(_05) &,
    decltype(_06) &
>(
    _01, _02, _03, _04, _05, _06
);
}
};


template<> struct tie_aggregate_impl<7> {
template<typename T> static constexpr auto call(T & x) {
auto & [
    _01, _02, _03, _04, _05, _06, _07
] = x;
return parser::tuple<
    decltype(_01) &, decltype(_02) &, decltype(_03) &, decltype(_04) &, decltype(_05) &,
    decltype(_06) &, decltype(_07) &
>(
    _01, _02, _03, _04, _05, _06, _07
);
}
};


template<> struct tie_aggregate_impl<8> {
template<typename T> static constexpr auto call(T & x) {
auto & [
    _01, _02, _03, _04, _05, _06, _07, _08
] = x;
return parser::tuple<
    decltype(_01) &, decltype(_02) &, decltype(_03) &, decltype(_04) &, decltype(_05) &,
    decltype(_06) &, decltype(_07) &, decltype(_08) &
>(
    _01, _02, _03, _04, _05, _06, _07, _08
);
}
};


template<> struct tie_aggregate_impl<9> {
template<typename T> static constexpr auto call(T & x) {
auto & [
    _01, _02, _03, _04, _05, _06, _07, _08, _09
] = x;
return parser::tuple<
    decltype(_01) &, decltype(_02) &, decltype(_03) &, decltype(_04) &, decltype(_05) &,
    decltype(_06) &, decltype(_07) &, decltype(_08) &, decltype(_09) &
>(
    _01, _02, _03, _04, _05, _06, _07, _08, _09
);
}
};


template<> struct tie_aggregate_impl<10> {
template<typename T> static constexpr auto call(T & x) {
auto & [
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a
] = x;
return parser::tuple<
    decltype(_01) &, decltype(_02) &, decltype(_03) &, decltype(_04) &, decltype(_05) &,
    decltype(_06) &, decltype(_07) &, decltype(_08) &, decltype(_09) &, decltype(_0a) &
>(
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a
);
}
};


template<> struct tie_aggregate_impl<11> {
template<typename T> static constexpr auto call(T & x) {
auto & [
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b
] = x;
return parser::tuple<
    decltype(_01) &, decltype(_02) &, decltype(_03) &, decltype(_04) &, decltype(_05) &,
    decltype(_06) &, decltype(_07) &, decltype(_08) &, decltype(_09) &, decltype(_0a) &,
    decltype(_0b) &
>(
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b
);
}
};


template<> struct tie_aggregate_impl<12> {
template<typename T> static constexpr auto call(T & x) {
auto & [
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c
] = x;
return parser::tuple<
    decltype(_01) &, decltype(_02) &, decltype(_03) &, decltype(_04) &, decltype(_05) &,
    decltype(_06) &, decltype(_07) &, decltype(_08) &, decltype(_09) &, decltype(_0a) &,
    decltype(_0b) &, decltype(_0c) &
>(
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c
);
}
};


template<> struct tie_aggregate_impl<13> {
template<typename T> static constexpr auto call(T & x) {
auto & [
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d
] = x;
return parser::tuple<
    decltype(_01) &, decltype(_02) &, decltype(_03) &, decltype(_04) &, decltype(_05) &,
    decltype(_06) &, decltype(_07) &, decltype(_08) &, decltype(_09) &, decltype(_0a) &,
    decltype(_0b) &, decltype(_0c) &, decltype(_0d) &
>(
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d
);
}
};


template<> struct tie_aggregate_impl<14> {
template<typename T> static constexpr auto call(T & x) {
auto & [
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e
] = x;
return parser::tuple<
    decltype(_01) &, decltype(_02) &, decltype(_03) &, decltype(_04) &, decltype(_05) &,
    decltype(_06) &, decltype(_07) &, decltype(_08) &, decltype(_09) &, decltype(_0a) &,
    decltype(_0b) &, decltype(_0c) &, decltype(_0d) &, decltype(_0e) &
>(
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e
);
}
};


template<> struct tie_aggregate_impl<15> {
template<typename T> static constexpr auto call(T & x) {
auto & [
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f
] = x;
return parser::tuple<
    decltype(_01) &, decltype(_02) &, decltype(_03) &, decltype(_04) &, decltype(_05) &,
    decltype(_06) &, decltype(_07) &, decltype(_08) &, decltype(_09) &, decltype(_0a) &,
    decltype(_0b) &, decltype(_0c) &, decltype(_0d) &, decltype(_0e) &, decltype(_0f) &
>(
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f
);
}
};


template<> struct tie_aggregate_impl<16> {
template<typename T> static constexpr auto call(T & x) {
auto & [
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10
] = x;
return parser::tuple<
    decltype(_01) &, decltype(_02) &, decltype(_03) &, decltype(_04) &, decltype(_05) &,
    decltype(_06) &, decltype(_07) &, decltype(_08) &, decltype(_09) &, decltype(_0a) &,
    decltype(_0b) &, decltype(_0c) &, decltype(_0d) &, decltype(_0e) &, decltype(_0f) &,
    decltype(_10) &
>(
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10
);
}
};


template<> struct tie_aggregate_impl<17> {
template<typename T> static constexpr auto call(T & x) {
auto & [
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11
] = x;
return parser::tuple<
    decltype(_01) &, decltype(_02) &, decltype(_03) &, decltype(_04) &, decltype(_05) &,
    decltype(_06) &, decltype(_07) &, decltype(_08) &, decltype(_09) &, decltype(_0a) &,
    decltype(_0b) &, decltype(_0c) &, decltype(_0d) &, decltype(_0e) &, decltype(_0f) &,
    decltype(_10) &, decltype(_11) &
>(
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11
);
}
};


template<> struct tie_aggregate_impl<18> {
template<typename T> static constexpr auto call(T & x) {
auto & [
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12
] = x;
return parser::tuple<
    decltype(_01) &, decltype(_02) &, decltype(_03) &, decltype(_04) &, decltype(_05) &,
    decltype(_06) &, decltype(_07) &, decltype(_08) &, decltype(_09) &, decltype(_0a) &,
    decltype(_0b) &, decltype(_0c) &, decltype(_0d) &, decltype(_0e) &, decltype(_0f) &,
    decltype(_10) &, decltype(_11) &, decltype(_12) &
>(
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12
);
}
};


template<> struct tie_aggregate_impl<19> {
template<typename T> static constexpr auto call(T & x) {
auto & [
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13
] = x;
return parser::tuple<
    decltype(_01) &, decltype(_02) &, decltype(_03) &, decltype(_04) &, decltype(_05) &,
    decltype(_06) &, decltype(_07) &, decltype(_08) &, decltype(_09) &, decltype(_0a) &,
    decltype(_0b) &, decltype(_0c) &, decltype(_0d) &, decltype(_0e) &, decltype(_0f) &,
    decltype(_10) &, decltype(_11) &, decltype(_12) &, decltype(_13) &
>(
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13
);
}
};


template<> struct tie_aggregate_impl<20> {
template<typename T> static constexpr auto call(T & x) {
auto & [
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14
] = x;
return parser::tuple<
    decltype(_01) &, decltype(_02) &, decltype(_03) &, decltype(_04) &, decltype(_05) &,
    decltype(_06) &, decltype(_07) &, decltype(_08) &, decltype(_09) &, decltype(_0a) &,
    decltype(_0b) &, decltype(_0c) &, decltype(_0d) &, decltype(_0e) &, decltype(_0f) &,
    decltype(_10) &, decltype(_11) &, decltype(_12) &, decltype(_13) &, decltype(_14) &
>(
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14
);
}
};


template<> struct tie_aggregate_impl<21> {
template<typename T> static constexpr auto call(T & x) {
auto & [
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15
] = x;
return parser::tuple<
    decltype(_01) &, decltype(_02) &, decltype(_03) &, decltype(_04) &, decltype(_05) &,
    decltype(_06) &, decltype(_07) &, decltype(_08) &, decltype(_09) &, decltype(_0a) &,
    decltype(_0b) &, decltype(_0c) &, decltype(_0d) &, decltype(_0e) &, decltype(_0f) &,
    decltype(_10) &, decltype(_11) &, decltype(_12) &, decltype(_13) &, decltype(_14) &,
    decltype(_15) &
>(
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15
);
}
};


template<> struct tie_aggregate_impl<22> {
template<typename T> static constexpr auto call(T & x) {
auto & [
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16
] = x;
return parser::tuple<
    decltype(_01) &, decltype(_02) &, decltype(_03) &, decltype(_04) &, decltype(_05) &,
    decltype(_06) &, decltype(_07) &, decltype(_08) &, decltype(_09) &, decltype(_0a) &,
    decltype(_0b) &, decltype(_0c) &, decltype(_0d) &, decltype(_0e) &, decltype(_0f) &,
    decltype(_10) &, decltype(_11) &, decltype(_12) &, decltype(_13) &, decltype(_14) &,
    decltype(_15) &, decltype(_16) &
>(
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16
);
}
};


template<> struct tie_aggregate_impl<23> {
template<typename T> static constexpr auto call(T & x) {
auto & [
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17
] = x;
return parser::tuple<
    decltype(_01) &, decltype(_02) &, decltype(_03) &, decltype(_04) &, decltype(_05) &,
    decltype(_06) &, decltype(_07) &, decltype(_08) &, decltype(_09) &, decltype(_0a) &,
    decltype(_0b) &, decltype(_0c) &, decltype(_0d) &, decltype(_0e) &, decltype(_0f) &,
    decltype(_10) &, decltype(_11) &, decltype(_12) &, decltype(_13) &, decltype(_14) &,
    decltype(_15) &, decltype(_16) &, decltype(_17) &
>(
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17
);
}
};


template<> struct tie_aggregate_impl<24> {
template<typename T> static constexpr auto call(T & x) {
auto & [
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18
] = x;
return parser::tuple<
    decltype(_01) &, decltype(_02) &, decltype(_03) &, decltype(_04) &, decltype(_05) &,
    decltype(_06) &, decltype(_07) &, decltype(_08) &, decltype(_09) &, decltype(_0a) &,
    decltype(_0b) &, decltype(_0c) &, decltype(_0d) &, decltype(_0e) &, decltype(_0f) &,
    decltype(_10) &, decltype(_11) &, decltype(_12) &, decltype(_13) &, decltype(_14) &,
    decltype(_15) &, decltype(_16) &, decltype(_17) &, decltype(_18) &
>(
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18
);
}
};


template<> struct tie_aggregate_impl<25> {
template<typename T> static constexpr auto call(T & x) {
auto & [
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19
] = x;
return parser::tuple<
    decltype(_01) &, decltype(_02) &, decltype(_03) &, decltype(_04) &, decltype(_05) &,
    decltype(_06) &, decltype(_07) &, decltype(_08) &, decltype(_09) &, decltype(_0a) &,
    decltype(_0b) &, decltype(_0c) &, decltype(_0d) &, decltype(_0e) &, decltype(_0f) &,
    decltype(_10) &, decltype(_11) &, decltype(_12) &, decltype(_13) &, decltype(_14) &,
    decltype(_15) &, decltype(_16) &, decltype(_17) &, decltype(_18) &, decltype(_19) &
>(
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19
);
}
};


template<> struct tie_aggregate_impl<26> {
template<typename T> static constexpr auto call(T & x) {
auto & [
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a
] = x;
return parser::tuple<
    decltype(_01) &, decltype(_02) &, decltype(_03) &, decltype(_04) &, decltype(_05) &,
    decltype(_06) &, decltype(_07) &, decltype(_08) &, decltype(_09) &, decltype(_0a) &,
    decltype(_0b) &, decltype(_0c) &, decltype(_0d) &, decltype(_0e) &, decltype(_0f) &,
    decltype(_10) &, decltype(_11) &, decltype(_12) &, decltype(_13) &, decltype(_14) &,
    decltype(_15) &, decltype(_16) &, decltype(_17) &, decltype(_18) &, decltype(_19) &,
    decltype(_1a) &
>(
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a
);
}
};


template<> struct tie_aggregate_impl<27> {
template<typename T> static constexpr auto call(T & x) {
auto & [
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b
] = x;
return parser::tuple<
    decltype(_01) &, decltype(_02) &, decltype(_03) &, decltype(_04) &, decltype(_05) &,
    decltype(_06) &, decltype(_07) &, decltype(_08) &, decltype(_09) &, decltype(_0a) &,
    decltype(_0b) &, decltype(_0c) &, decltype(_0d) &, decltype(_0e) &, decltype(_0f) &,
    decltype(_10) &, decltype(_11) &, decltype(_12) &, decltype(_13) &, decltype(_14) &,
    decltype(_15) &, decltype(_16) &, decltype(_17) &, decltype(_18) &, decltype(_19) &,
    decltype(_1a) &, decltype(_1b) &
>(
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b
);
}
};


template<> struct tie_aggregate_impl<28> {
template<typename T> static constexpr auto call(T & x) {
auto & [
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c
] = x;
return parser::tuple<
    decltype(_01) &, decltype(_02) &, decltype(_03) &, decltype(_04) &, decltype(_05) &,
    decltype(_06) &, decltype(_07) &, decltype(_08) &, decltype(_09) &, decltype(_0a) &,
    decltype(_0b) &, decltype(_0c) &, decltype(_0d) &, decltype(_0e) &, decltype(_0f) &,
    decltype(_10) &, decltype(_11) &, decltype(_12) &, decltype(_13) &, decltype(_14) &,
    decltype(_15) &, decltype(_16) &, decltype(_17) &, decltype(_18) &, decltype(_19) &,
    decltype(_1a) &, decltype(_1b) &, decltype(_1c) &
>(
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c
);
}
};


template<> struct tie_aggregate_impl<29> {
template<typename T> static constexpr auto call(T & x) {
auto & [
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d
] = x;
return parser::tuple<
    decltype(_01) &, decltype(_02) &, decltype(_03) &, decltype(_04) &, decltype(_05) &,
    decltype(_06) &, decltype(_07) &, decltype(_08) &, decltype(_09) &, decltype(_0a) &,
    decltype(_0b) &, decltype(_0c) &, decltype(_0d) &, decltype(_0e) &, decltype(_0f) &,
    decltype(_10) &, decltype(_11) &, decltype(_12) &, decltype(_13) &, decltype(_14) &,
    decltype(_15) &, decltype(_16) &, decltype(_17) &, decltype(_18) &, decltype(_19) &,
    decltype(_1a) &, decltype(_1b) &, decltype(_1c) &, decltype(_1d) &
>(
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d
);
}
};


template<> struct tie_aggregate_impl<30> {
template<typename T> static constexpr auto call(T & x) {
auto & [
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e
] = x;
return parser::tuple<
    decltype(_01) &, decltype(_02) &, decltype(_03) &, decltype(_04) &, decltype(_05) &,
    decltype(_06) &, decltype(_07) &, decltype(_08) &, decltype(_09) &, decltype(_0a) &,
    decltype(_0b) &, decltype(_0c) &, decltype(_0d) &, decltype(_0e) &, decltype(_0f) &,
    decltype(_10) &, decltype(_11) &, decltype(_12) &, decltype(_13) &, decltype(_14) &,
    decltype(_15) &, decltype(_16) &, decltype(_17) &, decltype(_18) &, decltype(_19) &,
    decltype(_1a) &, decltype(_1b) &, decltype(_1c) &, decltype(_1d) &, decltype(_1e) &
>(
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e
);
}
};


template<> struct tie_aggregate_impl<31> {
template<typename T> static constexpr auto call(T & x) {
auto & [
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f
] = x;
return parser::tuple<
    decltype(_01) &, decltype(_02) &, decltype(_03) &, decltype(_04) &, decltype(_05) &,
    decltype(_06) &, decltype(_07) &, decltype(_08) &, decltype(_09) &, decltype(_0a) &,
    decltype(_0b) &, decltype(_0c) &, decltype(_0d) &, decltype(_0e) &, decltype(_0f) &,
    decltype(_10) &, decltype(_11) &, decltype(_12) &, decltype(_13) &, decltype(_14) &,
    decltype(_15) &, decltype(_16) &, decltype(_17) &, decltype(_18) &, decltype(_19) &,
    decltype(_1a) &, decltype(_1b) &, decltype(_1c) &, decltype(_1d) &, decltype(_1e) &,
    decltype(_1f) &
>(
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f
);
}
};


template<> struct tie_aggregate_impl<32> {
template<typename T> static constexpr auto call(T & x) {
auto & [
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20
] = x;
return parser::tuple<
    decltype(_01) &, decltype(_02) &, decltype(_03) &, decltype(_04) &, decltype(_05) &,
    decltype(_06) &, decltype(_07) &, decltype(_08) &, decltype(_09) &, decltype(_0a) &,
    decltype(_0b) &, decltype(_0c) &, decltype(_0d) &, decltype(_0e) &, decltype(_0f) &,
    decltype(_10) &, decltype(_11) &, decltype(_12) &, decltype(_13) &, decltype(_14) &,
    decltype(_15) &, decltype(_16) &, decltype(_17) &, decltype(_18) &, decltype(_19) &,
    decltype(_1a) &, decltype(_1b) &, decltype(_1c) &, decltype(_1d) &, decltype(_1e) &,
    decltype(_1f) &, decltype(_20) &
>(
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20
);
}
};


template<> struct tie_aggregate_impl<33> {
template<typename T> static constexpr auto call(T & x) {
auto & [
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21
] = x;
return parser::tuple<
    decltype(_01) &, decltype(_02) &, decltype(_03) &, decltype(_04) &, decltype(_05) &,
    decltype(_06) &, decltype(_07) &, decltype(_08) &, decltype(_09) &, decltype(_0a) &,
    decltype(_0b) &, decltype(_0c) &, decltype(_0d) &, decltype(_0e) &, decltype(_0f) &,
    decltype(_10) &, decltype(_11) &, decltype(_12) &, decltype(_13) &, decltype(_14) &,
    decltype(_15) &, decltype(_16) &, decltype(_17) &, decltype(_18) &, decltype(_19) &,
    decltype(_1a) &, decltype(_1b) &, decltype(_1c) &, decltype(_1d) &, decltype(_1e) &,
    decltype(_1f) &, decltype(_20) &, decltype(_21) &
>(
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21
);
}
};


template<> struct tie_aggregate_impl<34> {
template<typename T> static constexpr auto call(T & x) {
auto & [
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22
] = x;
return parser::tuple<
    decltype(_01) &, decltype(_02) &, decltype(_03) &, decltype(_04) &, decltype(_05) &,
    decltype(_06) &, decltype(_07) &, decltype(_08) &, decltype(_09) &, decltype(_0a) &,
    decltype(_0b) &, decltype(_0c) &, decltype(_0d) &, decltype(_0e) &, decltype(_0f) &,
    decltype(_10) &, decltype(_11) &, decltype(_12) &, decltype(_13) &, decltype(_14) &,
    decltype(_15) &, decltype(_16) &, decltype(_17) &, decltype(_18) &, decltype(_19) &,
    decltype(_1a) &, decltype(_1b) &, decltype(_1c) &, decltype(_1d) &, decltype(_1e) &,
    decltype(_1f) &, decltype(_20) &, decltype(_21) &, decltype(_22) &
>(
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22
);
}
};


template<> struct tie_aggregate_impl<35> {
template<typename T> static constexpr auto call(T & x) {
auto & [
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23
] = x;
return parser::tuple<
    decltype(_01) &, decltype(_02) &, decltype(_03) &, decltype(_04) &, decltype(_05) &,
    decltype(_06) &, decltype(_07) &, decltype(_08) &, decltype(_09) &, decltype(_0a) &,
    decltype(_0b) &, decltype(_0c) &, decltype(_0d) &, decltype(_0e) &, decltype(_0f) &,
    decltype(_10) &, decltype(_11) &, decltype(_12) &, decltype(_13) &, decltype(_14) &,
    decltype(_15) &, decltype(_16) &, decltype(_17) &, decltype(_18) &, decltype(_19) &,
    decltype(_1a) &, decltype(_1b) &, decltype(_1c) &, decltype(_1d) &, decltype(_1e) &,
    decltype(_1f) &, decltype(_20) &, decltype(_21) &, decltype(_22) &, decltype(_23) &
>(
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23
);
}
};


template<> struct tie_aggregate_impl<36> {
template<typename T> static constexpr auto call(T & x) {
auto & [
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24
] = x;
return parser::tuple<
    decltype(_01) &, decltype(_02) &, decltype(_03) &, decltype(_04) &, decltype(_05) &,
    decltype(_06) &, decltype(_07) &, decltype(_08) &, decltype(_09) &, decltype(_0a) &,
    decltype(_0b) &, decltype(_0c) &, decltype(_0d) &, decltype(_0e) &, decltype(_0f) &,
    decltype(_10) &, decltype(_11) &, decltype(_12) &, decltype(_13) &, decltype(_14) &,
    decltype(_15) &, decltype(_16) &, decltype(_17) &, decltype(_18) &, decltype(_19) &,
    decltype(_1a) &, decltype(_1b) &, decltype(_1c) &, decltype(_1d) &, decltype(_1e) &,
    decltype(_1f) &, decltype(_20) &, decltype(_21) &, decltype(_22) &, decltype(_23) &,
    decltype(_24) &
>(
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24
);
}
};


template<> struct tie_aggregate_impl<37> {
template<typename T> static constexpr auto call(T & x) {
auto & [
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25
] = x;
return parser::tuple<
    decltype(_01) &, decltype(_02) &, decltype(_03) &, decltype(_04) &, decltype(_05) &,
    decltype(_06) &, decltype(_07) &, decltype(_08) &, decltype(_09) &, decltype(_0a) &,
    decltype(_0b) &, decltype(_0c) &, decltype(_0d) &, decltype(_0e) &, decltype(_0f) &,
    decltype(_10) &, decltype(_11) &, decltype(_12) &, decltype(_13) &, decltype(_14) &,
    decltype(_15) &, decltype(_16) &, decltype(_17) &, decltype(_18) &, decltype(_19) &,
    decltype(_1a) &, decltype(_1b) &, decltype(_1c) &, decltype(_1d) &, decltype(_1e) &,
    decltype(_1f) &, decltype(_20) &, decltype(_21) &, decltype(_22) &, decltype(_23) &,
    decltype(_24) &, decltype(_25) &
>(
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25
);
}
};


template<> struct tie_aggregate_impl<38> {
template<typename T> static constexpr auto call(T & x) {
auto & [
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26
] = x;
return parser::tuple<
    decltype(_01) &, decltype(_02) &, decltype(_03) &, decltype(_04) &, decltype(_05) &,
    decltype(_06) &, decltype(_07) &, decltype(_08) &, decltype(_09) &, decltype(_0a) &,
    decltype(_0b) &, decltype(_0c) &, decltype(_0d) &, decltype(_0e) &, decltype(_0f) &,
    decltype(_10) &, decltype(_11) &, decltype(_12) &, decltype(_13) &, decltype(_14) &,
    decltype(_15) &, decltype(_16) &, decltype(_17) &, decltype(_18) &, decltype(_19) &,
    decltype(_1a) &, decltype(_1b) &, decltype(_1c) &, decltype(_1d) &, decltype(_1e) &,
    decltype(_1f) &, decltype(_20) &, decltype(_21) &, decltype(_22) &, decltype(_23) &,
    decltype(_24) &, decltype(_25) &, decltype(_26) &
>(
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26
);
}
};


template<> struct tie_aggregate_impl<39> {
template<typename T> static constexpr auto call(T & x) {
auto & [
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27
] = x;
return parser::tuple<
    decltype(_01) &, decltype(_02) &, decltype(_03) &, decltype(_04) &, decltype(_05) &,
    decltype(_06) &, decltype(_07) &, decltype(_08) &, decltype(_09) &, decltype(_0a) &,
    decltype(_0b) &, decltype(_0c) &, decltype(_0d) &, decltype(_0e) &, decltype(_0f) &,
    decltype(_10) &, decltype(_11) &, decltype(_12) &, decltype(_13) &, decltype(_14) &,
    decltype(_15) &, decltype(_16) &, decltype(_17) &, decltype(_18) &, decltype(_19) &,
    decltype(_1a) &, decltype(_1b) &, decltype(_1c) &, decltype(_1d) &, decltype(_1e) &,
    decltype(_1f) &, decltype(_20) &, decltype(_21) &, decltype(_22) &, decltype(_23) &,
    decltype(_24) &, decltype(_25) &, decltype(_26) &, decltype(_27) &
>(
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27
);
}
};


template<> struct tie_aggregate_impl<40> {
template<typename T> static constexpr auto call(T & x) {
auto & [
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28
] = x;
return parser::tuple<
    decltype(_01) &, decltype(_02) &, decltype(_03) &, decltype(_04) &, decltype(_05) &,
    decltype(_06) &, decltype(_07) &, decltype(_08) &, decltype(_09) &, decltype(_0a) &,
    decltype(_0b) &, decltype(_0c) &, decltype(_0d) &, decltype(_0e) &, decltype(_0f) &,
    decltype(_10) &, decltype(_11) &, decltype(_12) &, decltype(_13) &, decltype(_14) &,
    decltype(_15) &, decltype(_16) &, decltype(_17) &, decltype(_18) &, decltype(_19) &,
    decltype(_1a) &, decltype(_1b) &, decltype(_1c) &, decltype(_1d) &, decltype(_1e) &,
    decltype(_1f) &, decltype(_20) &, decltype(_21) &, decltype(_22) &, decltype(_23) &,
    decltype(_24) &, decltype(_25) &, decltype(_26) &, decltype(_27) &, decltype(_28) &
>(
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28
);
}
};


template<> struct tie_aggregate_impl<41> {
template<typename T> static constexpr auto call(T & x) {
auto & [
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29
] = x;
return parser::tuple<
    decltype(_01) &, decltype(_02) &, decltype(_03) &, decltype(_04) &, decltype(_05) &,
    decltype(_06) &, decltype(_07) &, decltype(_08) &, decltype(_09) &, decltype(_0a) &,
    decltype(_0b) &, decltype(_0c) &, decltype(_0d) &, decltype(_0e) &, decltype(_0f) &,
    decltype(_10) &, decltype(_11) &, decltype(_12) &, decltype(_13) &, decltype(_14) &,
    decltype(_15) &, decltype(_16) &, decltype(_17) &, decltype(_18) &, decltype(_19) &,
    decltype(_1a) &, decltype(_1b) &, decltype(_1c) &, decltype(_1d) &, decltype(_1e) &,
    decltype(_1f) &, decltype(_20) &, decltype(_21) &, decltype(_22) &, decltype(_23) &,
    decltype(_24) &, decltype(_25) &, decltype(_26) &, decltype(_27) &, decltype(_28) &,
    decltype(_29) &
>(
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29
);
}
};


template<> struct tie_aggregate_impl<42> {
template<typename T> static constexpr auto call(T & x) {
auto & [
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a
] = x;
return parser::tuple<
    decltype(_01) &, decltype(_02) &, decltype(_03) &, decltype(_04) &, decltype(_05) &,
    decltype(_06) &, decltype(_07) &, decltype(_08) &, decltype(_09) &, decltype(_0a) &,
    decltype(_0b) &, decltype(_0c) &, decltype(_0d) &, decltype(_0e) &, decltype(_0f) &,
    decltype(_10) &, decltype(_11) &, decltype(_12) &, decltype(_13) &, decltype(_14) &,
    decltype(_15) &, decltype(_16) &, decltype(_17) &, decltype(_18) &, decltype(_19) &,
    decltype(_1a) &, decltype(_1b) &, decltype(_1c) &, decltype(_1d) &, decltype(_1e) &,
    decltype(_1f) &, decltype(_20) &, decltype(_21) &, decltype(_22) &, decltype(_23) &,
    decltype(_24) &, decltype(_25) &, decltype(_26) &, decltype(_27) &, decltype(_28) &,
    decltype(_29) &, decltype(_2a) &
>(
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a
);
}
};


template<> struct tie_aggregate_impl<43> {
template<typename T> static constexpr auto call(T & x) {
auto & [
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b
] = x;
return parser::tuple<
    decltype(_01) &, decltype(_02) &, decltype(_03) &, decltype(_04) &, decltype(_05) &,
    decltype(_06) &, decltype(_07) &, decltype(_08) &, decltype(_09) &, decltype(_0a) &,
    decltype(_0b) &, decltype(_0c) &, decltype(_0d) &, decltype(_0e) &, decltype(_0f) &,
    decltype(_10) &, decltype(_11) &, decltype(_12) &, decltype(_13) &, decltype(_14) &,
    decltype(_15) &, decltype(_16) &, decltype(_17) &, decltype(_18) &, decltype(_19) &,
    decltype(_1a) &, decltype(_1b) &, decltype(_1c) &, decltype(_1d) &, decltype(_1e) &,
    decltype(_1f) &, decltype(_20) &, decltype(_21) &, decltype(_22) &, decltype(_23) &,
    decltype(_24) &, decltype(_25) &, decltype(_26) &, decltype(_27) &, decltype(_28) &,
    decltype(_29) &, decltype(_2a) &, decltype(_2b) &
>(
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b
);
}
};


template<> struct tie_aggregate_impl<44> {
template<typename T> static constexpr auto call(T & x) {
auto & [
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c
] = x;
return parser::tuple<
    decltype(_01) &, decltype(_02) &, decltype(_03) &, decltype(_04) &, decltype(_05) &,
    decltype(_06) &, decltype(_07) &, decltype(_08) &, decltype(_09) &, decltype(_0a) &,
    decltype(_0b) &, decltype(_0c) &, decltype(_0d) &, decltype(_0e) &, decltype(_0f) &,
    decltype(_10) &, decltype(_11) &, decltype(_12) &, decltype(_13) &, decltype(_14) &,
    decltype(_15) &, decltype(_16) &, decltype(_17) &, decltype(_18) &, decltype(_19) &,
    decltype(_1a) &, decltype(_1b) &, decltype(_1c) &, decltype(_1d) &, decltype(_1e) &,
    decltype(_1f) &, decltype(_20) &, decltype(_21) &, decltype(_22) &, decltype(_23) &,
    decltype(_24) &, decltype(_25) &, decltype(_26) &, decltype(_27) &, decltype(_28) &,
    decltype(_29) &, decltype(_2a) &, decltype(_2b) &, decltype(_2c) &
>(
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c
);
}
};


template<> struct tie_aggregate_impl<45> {
template<typename T> static constexpr auto call(T & x) {
auto & [
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d
] = x;
return parser::tuple<
    decltype(_01) &, decltype(_02) &, decltype(_03) &, decltype(_04) &, decltype(_05) &,
    decltype(_06) &, decltype(_07) &, decltype(_08) &, decltype(_09) &, decltype(_0a) &,
    decltype(_0b) &, decltype(_0c) &, decltype(_0d) &, decltype(_0e) &, decltype(_0f) &,
    decltype(_10) &, decltype(_11) &, decltype(_12) &, decltype(_13) &, decltype(_14) &,
    decltype(_15) &, decltype(_16) &, decltype(_17) &, decltype(_18) &, decltype(_19) &,
    decltype(_1a) &, decltype(_1b) &, decltype(_1c) &, decltype(_1d) &, decltype(_1e) &,
    decltype(_1f) &, decltype(_20) &, decltype(_21) &, decltype(_22) &, decltype(_23) &,
    decltype(_24) &, decltype(_25) &, decltype(_26) &, decltype(_27) &, decltype(_28) &,
    decltype(_29) &, decltype(_2a) &, decltype(_2b) &, decltype(_2c) &, decltype(_2d) &
>(
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d
);
}
};


template<> struct tie_aggregate_impl<46> {
template<typename T> static constexpr auto call(T & x) {
auto & [
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e
] = x;
return parser::tuple<
    decltype(_01) &, decltype(_02) &, decltype(_03) &, decltype(_04) &, decltype(_05) &,
    decltype(_06) &, decltype(_07) &, decltype(_08) &, decltype(_09) &, decltype(_0a) &,
    decltype(_0b) &, decltype(_0c) &, decltype(_0d) &, decltype(_0e) &, decltype(_0f) &,
    decltype(_10) &, decltype(_11) &, decltype(_12) &, decltype(_13) &, decltype(_14) &,
    decltype(_15) &, decltype(_16) &, decltype(_17) &, decltype(_18) &, decltype(_19) &,
    decltype(_1a) &, decltype(_1b) &, decltype(_1c) &, decltype(_1d) &, decltype(_1e) &,
    decltype(_1f) &, decltype(_20) &, decltype(_21) &, decltype(_22) &, decltype(_23) &,
    decltype(_24) &, decltype(_25) &, decltype(_26) &, decltype(_27) &, decltype(_28) &,
    decltype(_29) &, decltype(_2a) &, decltype(_2b) &, decltype(_2c) &, decltype(_2d) &,
    decltype(_2e) &
>(
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e
);
}
};


template<> struct tie_aggregate_impl<47> {
template<typename T> static constexpr auto call(T & x) {
auto & [
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f
] = x;
return parser::tuple<
    decltype(_01) &, decltype(_02) &, decltype(_03) &, decltype(_04) &, decltype(_05) &,
    decltype(_06) &, decltype(_07) &, decltype(_08) &, decltype(_09) &, decltype(_0a) &,
    decltype(_0b) &, decltype(_0c) &, decltype(_0d) &, decltype(_0e) &, decltype(_0f) &,
    decltype(_10) &, decltype(_11) &, decltype(_12) &, decltype(_13) &, decltype(_14) &,
    decltype(_15) &, decltype(_16) &, decltype(_17) &, decltype(_18) &, decltype(_19) &,
    decltype(_1a) &, decltype(_1b) &, decltype(_1c) &, decltype(_1d) &, decltype(_1e) &,
    decltype(_1f) &, decltype(_20) &, decltype(_21) &, decltype(_22) &, decltype(_23) &,
    decltype(_24) &, decltype(_25) &, decltype(_26) &, decltype(_27) &, decltype(_28) &,
    decltype(_29) &, decltype(_2a) &, decltype(_2b) &, decltype(_2c) &, decltype(_2d) &,
    decltype(_2e) &, decltype(_2f) &
>(
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f
);
}
};


template<> struct tie_aggregate_impl<48> {
template<typename T> static constexpr auto call(T & x) {
auto & [
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30
] = x;
return parser::tuple<
    decltype(_01) &, decltype(_02) &, decltype(_03) &, decltype(_04) &, decltype(_05) &,
    decltype(_06) &, decltype(_07) &, decltype(_08) &, decltype(_09) &, decltype(_0a) &,
    decltype(_0b) &, decltype(_0c) &, decltype(_0d) &, decltype(_0e) &, decltype(_0f) &,
    decltype(_10) &, decltype(_11) &, decltype(_12) &, decltype(_13) &, decltype(_14) &,
    decltype(_15) &, decltype(_16) &, decltype(_17) &, decltype(_18) &, decltype(_19) &,
    decltype(_1a) &, decltype(_1b) &, decltype(_1c) &, decltype(_1d) &, decltype(_1e) &,
    decltype(_1f) &, decltype(_20) &, decltype(_21) &, decltype(_22) &, decltype(_23) &,
    decltype(_24) &, decltype(_25) &, decltype(_26) &, decltype(_27) &, decltype(_28) &,
    decltype(_29) &, decltype(_2a) &, decltype(_2b) &, decltype(_2c) &, decltype(_2d) &,
    decltype(_2e) &, decltype(_2f) &, decltype(_30) &
>(
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30
);
}
};


template<> struct tie_aggregate_impl<49> {
template<typename T> static constexpr auto call(T & x) {
auto & [
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30, _31
] = x;
return parser::tuple<
    decltype(_01) &, decltype(_02) &, decltype(_03) &, decltype(_04) &, decltype(_05) &,
    decltype(_06) &, decltype(_07) &, decltype(_08) &, decltype(_09) &, decltype(_0a) &,
    decltype(_0b) &, decltype(_0c) &, decltype(_0d) &, decltype(_0e) &, decltype(_0f) &,
    decltype(_10) &, decltype(_11) &, decltype(_12) &, decltype(_13) &, decltype(_14) &,
    decltype(_15) &, decltype(_16) &, decltype(_17) &, decltype(_18) &, decltype(_19) &,
    decltype(_1a) &, decltype(_1b) &, decltype(_1c) &, decltype(_1d) &, decltype(_1e) &,
    decltype(_1f) &, decltype(_20) &, decltype(_21) &, decltype(_22) &, decltype(_23) &,
    decltype(_24) &, decltype(_25) &, decltype(_26) &, decltype(_27) &, decltype(_28) &,
    decltype(_29) &, decltype(_2a) &, decltype(_2b) &, decltype(_2c) &, decltype(_2d) &,
    decltype(_2e) &, decltype(_2f) &, decltype(_30) &, decltype(_31) &
>(
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30, _31
);
}
};


template<> struct tie_aggregate_impl<50> {
template<typename T> static constexpr auto call(T & x) {
auto & [
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30, _31, _32
] = x;
return parser::tuple<
    decltype(_01) &, decltype(_02) &, decltype(_03) &, decltype(_04) &, decltype(_05) &,
    decltype(_06) &, decltype(_07) &, decltype(_08) &, decltype(_09) &, decltype(_0a) &,
    decltype(_0b) &, decltype(_0c) &, decltype(_0d) &, decltype(_0e) &, decltype(_0f) &,
    decltype(_10) &, decltype(_11) &, decltype(_12) &, decltype(_13) &, decltype(_14) &,
    decltype(_15) &, decltype(_16) &, decltype(_17) &, decltype(_18) &, decltype(_19) &,
    decltype(_1a) &, decltype(_1b) &, decltype(_1c) &, decltype(_1d) &, decltype(_1e) &,
    decltype(_1f) &, decltype(_20) &, decltype(_21) &, decltype(_22) &, decltype(_23) &,
    decltype(_24) &, decltype(_25) &, decltype(_26) &, decltype(_27) &, decltype(_28) &,
    decltype(_29) &, decltype(_2a) &, decltype(_2b) &, decltype(_2c) &, decltype(_2d) &,
    decltype(_2e) &, decltype(_2f) &, decltype(_30) &, decltype(_31) &, decltype(_32) &
>(
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30, _31, _32
);
}
};


template<> struct tie_aggregate_impl<51> {
template<typename T> static constexpr auto call(T & x) {
auto & [
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30, _31, _32, _33
] = x;
return parser::tuple<
    decltype(_01) &, decltype(_02) &, decltype(_03) &, decltype(_04) &, decltype(_05) &,
    decltype(_06) &, decltype(_07) &, decltype(_08) &, decltype(_09) &, decltype(_0a) &,
    decltype(_0b) &, decltype(_0c) &, decltype(_0d) &, decltype(_0e) &, decltype(_0f) &,
    decltype(_10) &, decltype(_11) &, decltype(_12) &, decltype(_13) &, decltype(_14) &,
    decltype(_15) &, decltype(_16) &, decltype(_17) &, decltype(_18) &, decltype(_19) &,
    decltype(_1a) &, decltype(_1b) &, decltype(_1c) &, decltype(_1d) &, decltype(_1e) &,
    decltype(_1f) &, decltype(_20) &, decltype(_21) &, decltype(_22) &, decltype(_23) &,
    decltype(_24) &, decltype(_25) &, decltype(_26) &, decltype(_27) &, decltype(_28) &,
    decltype(_29) &, decltype(_2a) &, decltype(_2b) &, decltype(_2c) &, decltype(_2d) &,
    decltype(_2e) &, decltype(_2f) &, decltype(_30) &, decltype(_31) &, decltype(_32) &,
    decltype(_33) &
>(
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30, _31, _32, _33
);
}
};


template<> struct tie_aggregate_impl<52> {
template<typename T> static constexpr auto call(T & x) {
auto & [
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30, _31, _32, _33, _34
] = x;
return parser::tuple<
    decltype(_01) &, decltype(_02) &, decltype(_03) &, decltype(_04) &, decltype(_05) &,
    decltype(_06) &, decltype(_07) &, decltype(_08) &, decltype(_09) &, decltype(_0a) &,
    decltype(_0b) &, decltype(_0c) &, decltype(_0d) &, decltype(_0e) &, decltype(_0f) &,
    decltype(_10) &, decltype(_11) &, decltype(_12) &, decltype(_13) &, decltype(_14) &,
    decltype(_15) &, decltype(_16) &, decltype(_17) &, decltype(_18) &, decltype(_19) &,
    decltype(_1a) &, decltype(_1b) &, decltype(_1c) &, decltype(_1d) &, decltype(_1e) &,
    decltype(_1f) &, decltype(_20) &, decltype(_21) &, decltype(_22) &, decltype(_23) &,
    decltype(_24) &, decltype(_25) &, decltype(_26) &, decltype(_27) &, decltype(_28) &,
    decltype(_29) &, decltype(_2a) &, decltype(_2b) &, decltype(_2c) &, decltype(_2d) &,
    decltype(_2e) &, decltype(_2f) &, decltype(_30) &, decltype(_31) &, decltype(_32) &,
    decltype(_33) &, decltype(_34) &
>(
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30, _31, _32, _33, _34
);
}
};


template<> struct tie_aggregate_impl<53> {
template<typename T> static constexpr auto call(T & x) {
auto & [
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30, _31, _32, _33, _34, _35
] = x;
return parser::tuple<
    decltype(_01) &, decltype(_02) &, decltype(_03) &, decltype(_04) &, decltype(_05) &,
    decltype(_06) &, decltype(_07) &, decltype(_08) &, decltype(_09) &, decltype(_0a) &,
    decltype(_0b) &, decltype(_0c) &, decltype(_0d) &, decltype(_0e) &, decltype(_0f) &,
    decltype(_10) &, decltype(_11) &, decltype(_12) &, decltype(_13) &, decltype(_14) &,
    decltype(_15) &, decltype(_16) &, decltype(_17) &, decltype(_18) &, decltype(_19) &,
    decltype(_1a) &, decltype(_1b) &, decltype(_1c) &, decltype(_1d) &, decltype(_1e) &,
    decltype(_1f) &, decltype(_20) &, decltype(_21) &, decltype(_22) &, decltype(_23) &,
    decltype(_24) &, decltype(_25) &, decltype(_26) &, decltype(_27) &, decltype(_28) &,
    decltype(_29) &, decltype(_2a) &, decltype(_2b) &, decltype(_2c) &, decltype(_2d) &,
    decltype(_2e) &, decltype(_2f) &, decltype(_30) &, decltype(_31) &, decltype(_32) &,
    decltype(_33) &, decltype(_34) &, decltype(_35) &
>(
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30, _31, _32, _33, _34, _35
);
}
};


template<> struct tie_aggregate_impl<54> {
template<typename T> static constexpr auto call(T & x) {
auto & [
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30, _31, _32, _33, _34, _35, _36
] = x;
return parser::tuple<
    decltype(_01) &, decltype(_02) &, decltype(_03) &, decltype(_04) &, decltype(_05) &,
    decltype(_06) &, decltype(_07) &, decltype(_08) &, decltype(_09) &, decltype(_0a) &,
    decltype(_0b) &, decltype(_0c) &, decltype(_0d) &, decltype(_0e) &, decltype(_0f) &,
    decltype(_10) &, decltype(_11) &, decltype(_12) &, decltype(_13) &, decltype(_14) &,
    decltype(_15) &, decltype(_16) &, decltype(_17) &, decltype(_18) &, decltype(_19) &,
    decltype(_1a) &, decltype(_1b) &, decltype(_1c) &, decltype(_1d) &, decltype(_1e) &,
    decltype(_1f) &, decltype(_20) &, decltype(_21) &, decltype(_22) &, decltype(_23) &,
    decltype(_24) &, decltype(_25) &, decltype(_26) &, decltype(_27) &, decltype(_28) &,
    decltype(_29) &, decltype(_2a) &, decltype(_2b) &, decltype(_2c) &, decltype(_2d) &,
    decltype(_2e) &, decltype(_2f) &, decltype(_30) &, decltype(_31) &, decltype(_32) &,
    decltype(_33) &, decltype(_34) &, decltype(_35) &, decltype(_36) &
>(
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30, _31, _32, _33, _34, _35, _36
);
}
};


template<> struct tie_aggregate_impl<55> {
template<typename T> static constexpr auto call(T & x) {
auto & [
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30, _31, _32, _33, _34, _35, _36, _37
] = x;
return parser::tuple<
    decltype(_01) &, decltype(_02) &, decltype(_03) &, decltype(_04) &, decltype(_05) &,
    decltype(_06) &, decltype(_07) &, decltype(_08) &, decltype(_09) &, decltype(_0a) &,
    decltype(_0b) &, decltype(_0c) &, decltype(_0d) &, decltype(_0e) &, decltype(_0f) &,
    decltype(_10) &, decltype(_11) &, decltype(_12) &, decltype(_13) &, decltype(_14) &,
    decltype(_15) &, decltype(_16) &, decltype(_17) &, decltype(_18) &, decltype(_19) &,
    decltype(_1a) &, decltype(_1b) &, decltype(_1c) &, decltype(_1d) &, decltype(_1e) &,
    decltype(_1f) &, decltype(_20) &, decltype(_21) &, decltype(_22) &, decltype(_23) &,
    decltype(_24) &, decltype(_25) &, decltype(_26) &, decltype(_27) &, decltype(_28) &,
    decltype(_29) &, decltype(_2a) &, decltype(_2b) &, decltype(_2c) &, decltype(_2d) &,
    decltype(_2e) &, decltype(_2f) &, decltype(_30) &, decltype(_31) &, decltype(_32) &,
    decltype(_33) &, decltype(_34) &, decltype(_35) &, decltype(_36) &, decltype(_37) &
>(
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30, _31, _32, _33, _34, _35, _36, _37
);
}
};


template<> struct tie_aggregate_impl<56> {
template<typename T> static constexpr auto call(T & x) {
auto & [
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30, _31, _32, _33, _34, _35, _36, _37, _38
] = x;
return parser::tuple<
    decltype(_01) &, decltype(_02) &, decltype(_03) &, decltype(_04) &, decltype(_05) &,
    decltype(_06) &, decltype(_07) &, decltype(_08) &, decltype(_09) &, decltype(_0a) &,
    decltype(_0b) &, decltype(_0c) &, decltype(_0d) &, decltype(_0e) &, decltype(_0f) &,
    decltype(_10) &, decltype(_11) &, decltype(_12) &, decltype(_13) &, decltype(_14) &,
    decltype(_15) &, decltype(_16) &, decltype(_17) &, decltype(_18) &, decltype(_19) &,
    decltype(_1a) &, decltype(_1b) &, decltype(_1c) &, decltype(_1d) &, decltype(_1e) &,
    decltype(_1f) &, decltype(_20) &, decltype(_21) &, decltype(_22) &, decltype(_23) &,
    decltype(_24) &, decltype(_25) &, decltype(_26) &, decltype(_27) &, decltype(_28) &,
    decltype(_29) &, decltype(_2a) &, decltype(_2b) &, decltype(_2c) &, decltype(_2d) &,
    decltype(_2e) &, decltype(_2f) &, decltype(_30) &, decltype(_31) &, decltype(_32) &,
    decltype(_33) &, decltype(_34) &, decltype(_35) &, decltype(_36) &, decltype(_37) &,
    decltype(_38) &
>(
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30, _31, _32, _33, _34, _35, _36, _37, _38
);
}
};


template<> struct tie_aggregate_impl<57> {
template<typename T> static constexpr auto call(T & x) {
auto & [
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39
] = x;
return parser::tuple<
    decltype(_01) &, decltype(_02) &, decltype(_03) &, decltype(_04) &, decltype(_05) &,
    decltype(_06) &, decltype(_07) &, decltype(_08) &, decltype(_09) &, decltype(_0a) &,
    decltype(_0b) &, decltype(_0c) &, decltype(_0d) &, decltype(_0e) &, decltype(_0f) &,
    decltype(_10) &, decltype(_11) &, decltype(_12) &, decltype(_13) &, decltype(_14) &,
    decltype(_15) &, decltype(_16) &, decltype(_17) &, decltype(_18) &, decltype(_19) &,
    decltype(_1a) &, decltype(_1b) &, decltype(_1c) &, decltype(_1d) &, decltype(_1e) &,
    decltype(_1f) &, decltype(_20) &, decltype(_21) &, decltype(_22) &, decltype(_23) &,
    decltype(_24) &, decltype(_25) &, decltype(_26) &, decltype(_27) &, decltype(_28) &,
    decltype(_29) &, decltype(_2a) &, decltype(_2b) &, decltype(_2c) &, decltype(_2d) &,
    decltype(_2e) &, decltype(_2f) &, decltype(_30) &, decltype(_31) &, decltype(_32) &,
    decltype(_33) &, decltype(_34) &, decltype(_35) &, decltype(_36) &, decltype(_37) &,
    decltype(_38) &, decltype(_39) &
>(
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39
);
}
};


template<> struct tie_aggregate_impl<58> {
template<typename T> static constexpr auto call(T & x) {
auto & [
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _3a
] = x;
return parser::tuple<
    decltype(_01) &, decltype(_02) &, decltype(_03) &, decltype(_04) &, decltype(_05) &,
    decltype(_06) &, decltype(_07) &, decltype(_08) &, decltype(_09) &, decltype(_0a) &,
    decltype(_0b) &, decltype(_0c) &, decltype(_0d) &, decltype(_0e) &, decltype(_0f) &,
    decltype(_10) &, decltype(_11) &, decltype(_12) &, decltype(_13) &, decltype(_14) &,
    decltype(_15) &, decltype(_16) &, decltype(_17) &, decltype(_18) &, decltype(_19) &,
    decltype(_1a) &, decltype(_1b) &, decltype(_1c) &, decltype(_1d) &, decltype(_1e) &,
    decltype(_1f) &, decltype(_20) &, decltype(_21) &, decltype(_22) &, decltype(_23) &,
    decltype(_24) &, decltype(_25) &, decltype(_26) &, decltype(_27) &, decltype(_28) &,
    decltype(_29) &, decltype(_2a) &, decltype(_2b) &, decltype(_2c) &, decltype(_2d) &,
    decltype(_2e) &, decltype(_2f) &, decltype(_30) &, decltype(_31) &, decltype(_32) &,
    decltype(_33) &, decltype(_34) &, decltype(_35) &, decltype(_36) &, decltype(_37) &,
    decltype(_38) &, decltype(_39) &, decltype(_3a) &
>(
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _3a
);
}
};


template<> struct tie_aggregate_impl<59> {
template<typename T> static constexpr auto call(T & x) {
auto & [
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _3a, _3b
] = x;
return parser::tuple<
    decltype(_01) &, decltype(_02) &, decltype(_03) &, decltype(_04) &, decltype(_05) &,
    decltype(_06) &, decltype(_07) &, decltype(_08) &, decltype(_09) &, decltype(_0a) &,
    decltype(_0b) &, decltype(_0c) &, decltype(_0d) &, decltype(_0e) &, decltype(_0f) &,
    decltype(_10) &, decltype(_11) &, decltype(_12) &, decltype(_13) &, decltype(_14) &,
    decltype(_15) &, decltype(_16) &, decltype(_17) &, decltype(_18) &, decltype(_19) &,
    decltype(_1a) &, decltype(_1b) &, decltype(_1c) &, decltype(_1d) &, decltype(_1e) &,
    decltype(_1f) &, decltype(_20) &, decltype(_21) &, decltype(_22) &, decltype(_23) &,
    decltype(_24) &, decltype(_25) &, decltype(_26) &, decltype(_27) &, decltype(_28) &,
    decltype(_29) &, decltype(_2a) &, decltype(_2b) &, decltype(_2c) &, decltype(_2d) &,
    decltype(_2e) &, decltype(_2f) &, decltype(_30) &, decltype(_31) &, decltype(_32) &,
    decltype(_33) &, decltype(_34) &, decltype(_35) &, decltype(_36) &, decltype(_37) &,
    decltype(_38) &, decltype(_39) &, decltype(_3a) &, decltype(_3b) &
>(
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _3a, _3b
);
}
};


template<> struct tie_aggregate_impl<60> {
template<typename T> static constexpr auto call(T & x) {
auto & [
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _3a, _3b, _3c
] = x;
return parser::tuple<
    decltype(_01) &, decltype(_02) &, decltype(_03) &, decltype(_04) &, decltype(_05) &,
    decltype(_06) &, decltype(_07) &, decltype(_08) &, decltype(_09) &, decltype(_0a) &,
    decltype(_0b) &, decltype(_0c) &, decltype(_0d) &, decltype(_0e) &, decltype(_0f) &,
    decltype(_10) &, decltype(_11) &, decltype(_12) &, decltype(_13) &, decltype(_14) &,
    decltype(_15) &, decltype(_16) &, decltype(_17) &, decltype(_18) &, decltype(_19) &,
    decltype(_1a) &, decltype(_1b) &, decltype(_1c) &, decltype(_1d) &, decltype(_1e) &,
    decltype(_1f) &, decltype(_20) &, decltype(_21) &, decltype(_22) &, decltype(_23) &,
    decltype(_24) &, decltype(_25) &, decltype(_26) &, decltype(_27) &, decltype(_28) &,
    decltype(_29) &, decltype(_2a) &, decltype(_2b) &, decltype(_2c) &, decltype(_2d) &,
    decltype(_2e) &, decltype(_2f) &, decltype(_30) &, decltype(_31) &, decltype(_32) &,
    decltype(_33) &, decltype(_34) &, decltype(_35) &, decltype(_36) &, decltype(_37) &,
    decltype(_38) &, decltype(_39) &, decltype(_3a) &, decltype(_3b) &, decltype(_3c) &
>(
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _3a, _3b, _3c
);
}
};


template<> struct tie_aggregate_impl<61> {
template<typename T> static constexpr auto call(T & x) {
auto & [
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _3a, _3b, _3c,
    _3d
] = x;
return parser::tuple<
    decltype(_01) &, decltype(_02) &, decltype(_03) &, decltype(_04) &, decltype(_05) &,
    decltype(_06) &, decltype(_07) &, decltype(_08) &, decltype(_09) &, decltype(_0a) &,
    decltype(_0b) &, decltype(_0c) &, decltype(_0d) &, decltype(_0e) &, decltype(_0f) &,
    decltype(_10) &, decltype(_11) &, decltype(_12) &, decltype(_13) &, decltype(_14) &,
    decltype(_15) &, decltype(_16) &, decltype(_17) &, decltype(_18) &, decltype(_19) &,
    decltype(_1a) &, decltype(_1b) &, decltype(_1c) &, decltype(_1d) &, decltype(_1e) &,
    decltype(_1f) &, decltype(_20) &, decltype(_21) &, decltype(_22) &, decltype(_23) &,
    decltype(_24) &, decltype(_25) &, decltype(_26) &, decltype(_27) &, decltype(_28) &,
    decltype(_29) &, decltype(_2a) &, decltype(_2b) &, decltype(_2c) &, decltype(_2d) &,
    decltype(_2e) &, decltype(_2f) &, decltype(_30) &, decltype(_31) &, decltype(_32) &,
    decltype(_33) &, decltype(_34) &, decltype(_35) &, decltype(_36) &, decltype(_37) &,
    decltype(_38) &, decltype(_39) &, decltype(_3a) &, decltype(_3b) &, decltype(_3c) &,
    decltype(_3d) &
>(
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _3a, _3b, _3c,
    _3d
);
}
};


template<> struct tie_aggregate_impl<62> {
template<typename T> static constexpr auto call(T & x) {
auto & [
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _3a, _3b, _3c,
    _3d, _3e
] = x;
return parser::tuple<
    decltype(_01) &, decltype(_02) &, decltype(_03) &, decltype(_04) &, decltype(_05) &,
    decltype(_06) &, decltype(_07) &, decltype(_08) &, decltype(_09) &, decltype(_0a) &,
    decltype(_0b) &, decltype(_0c) &, decltype(_0d) &, decltype(_0e) &, decltype(_0f) &,
    decltype(_10) &, decltype(_11) &, decltype(_12) &, decltype(_13) &, decltype(_14) &,
    decltype(_15) &, decltype(_16) &, decltype(_17) &, decltype(_18) &, decltype(_19) &,
    decltype(_1a) &, decltype(_1b) &, decltype(_1c) &, decltype(_1d) &, decltype(_1e) &,
    decltype(_1f) &, decltype(_20) &, decltype(_21) &, decltype(_22) &, decltype(_23) &,
    decltype(_24) &, decltype(_25) &, decltype(_26) &, decltype(_27) &, decltype(_28) &,
    decltype(_29) &, decltype(_2a) &, decltype(_2b) &, decltype(_2c) &, decltype(_2d) &,
    decltype(_2e) &, decltype(_2f) &, decltype(_30) &, decltype(_31) &, decltype(_32) &,
    decltype(_33) &, decltype(_34) &, decltype(_35) &, decltype(_36) &, decltype(_37) &,
    decltype(_38) &, decltype(_39) &, decltype(_3a) &, decltype(_3b) &, decltype(_3c) &,
    decltype(_3d) &, decltype(_3e) &
>(
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _3a, _3b, _3c,
    _3d, _3e
);
}
};


template<> struct tie_aggregate_impl<63> {
template<typename T> static constexpr auto call(T & x) {
auto & [
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _3a, _3b, _3c,
    _3d, _3e, _3f
] = x;
return parser::tuple<
    decltype(_01) &, decltype(_02) &, decltype(_03) &, decltype(_04) &, decltype(_05) &,
    decltype(_06) &, decltype(_07) &, decltype(_08) &, decltype(_09) &, decltype(_0a) &,
    decltype(_0b) &, decltype(_0c) &, decltype(_0d) &, decltype(_0e) &, decltype(_0f) &,
    decltype(_10) &, decltype(_11) &, decltype(_12) &, decltype(_13) &, decltype(_14) &,
    decltype(_15) &, decltype(_16) &, decltype(_17) &, decltype(_18) &, decltype(_19) &,
    decltype(_1a) &, decltype(_1b) &, decltype(_1c) &, decltype(_1d) &, decltype(_1e) &,
    decltype(_1f) &, decltype(_20) &, decltype(_21) &, decltype(_22) &, decltype(_23) &,
    decltype(_24) &, decltype(_25) &, decltype(_26) &, decltype(_27) &, decltype(_28) &,
    decltype(_29) &, decltype(_2a) &, decltype(_2b) &, decltype(_2c) &, decltype(_2d) &,
    decltype(_2e) &, decltype(_2f) &, decltype(_30) &, decltype(_31) &, decltype(_32) &,
    decltype(_33) &, decltype(_34) &, decltype(_35) &, decltype(_36) &, decltype(_37) &,
    decltype(_38) &, decltype(_39) &, decltype(_3a) &, decltype(_3b) &, decltype(_3c) &,
    decltype(_3d) &, decltype(_3e) &, decltype(_3f) &
>(
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _3a, _3b, _3c,
    _3d, _3e, _3f
);
}
};


template<> struct tie_aggregate_impl<64> {
template<typename T> static constexpr auto call(T & x) {
auto & [
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _3a, _3b, _3c,
    _3d, _3e, _3f, _40
] = x;
return parser::tuple<
    decltype(_01) &, decltype(_02) &, decltype(_03) &, decltype(_04) &, decltype(_05) &,
    decltype(_06) &, decltype(_07) &, decltype(_08) &, decltype(_09) &, decltype(_0a) &,
    decltype(_0b) &, decltype(_0c) &, decltype(_0d) &, decltype(_0e) &, decltype(_0f) &,
    decltype(_10) &, decltype(_11) &, decltype(_12) &, decltype(_13) &, decltype(_14) &,
    decltype(_15) &, decltype(_16) &, decltype(_17) &, decltype(_18) &, decltype(_19) &,
    decltype(_1a) &, decltype(_1b) &, decltype(_1c) &, decltype(_1d) &, decltype(_1e) &,
    decltype(_1f) &, decltype(_20) &, decltype(_21) &, decltype(_22) &, decltype(_23) &,
    decltype(_24) &, decltype(_25) &, decltype(_26) &, decltype(_27) &, decltype(_28) &,
    decltype(_29) &, decltype(_2a) &, decltype(_2b) &, decltype(_2c) &, decltype(_2d) &,
    decltype(_2e) &, decltype(_2f) &, decltype(_30) &, decltype(_31) &, decltype(_32) &,
    decltype(_33) &, decltype(_34) &, decltype(_35) &, decltype(_36) &, decltype(_37) &,
    decltype(_38) &, decltype(_39) &, decltype(_3a) &, decltype(_3b) &, decltype(_3c) &,
    decltype(_3d) &, decltype(_3e) &, decltype(_3f) &, decltype(_40) &
>(
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _3a, _3b, _3c,
    _3d, _3e, _3f, _40
);
}
};


template<> struct tie_aggregate_impl<65> {
template<typename T> static constexpr auto call(T & x) {
auto & [
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _3a, _3b, _3c,
    _3d, _3e, _3f, _40, _41
] = x;
return parser::tuple<
    decltype(_01) &, decltype(_02) &, decltype(_03) &, decltype(_04) &, decltype(_05) &,
    decltype(_06) &, decltype(_07) &, decltype(_08) &, decltype(_09) &, decltype(_0a) &,
    decltype(_0b) &, decltype(_0c) &, decltype(_0d) &, decltype(_0e) &, decltype(_0f) &,
    decltype(_10) &, decltype(_11) &, decltype(_12) &, decltype(_13) &, decltype(_14) &,
    decltype(_15) &, decltype(_16) &, decltype(_17) &, decltype(_18) &, decltype(_19) &,
    decltype(_1a) &, decltype(_1b) &, decltype(_1c) &, decltype(_1d) &, decltype(_1e) &,
    decltype(_1f) &, decltype(_20) &, decltype(_21) &, decltype(_22) &, decltype(_23) &,
    decltype(_24) &, decltype(_25) &, decltype(_26) &, decltype(_27) &, decltype(_28) &,
    decltype(_29) &, decltype(_2a) &, decltype(_2b) &, decltype(_2c) &, decltype(_2d) &,
    decltype(_2e) &, decltype(_2f) &, decltype(_30) &, decltype(_31) &, decltype(_32) &,
    decltype(_33) &, decltype(_34) &, decltype(_35) &, decltype(_36) &, decltype(_37) &,
    decltype(_38) &, decltype(_39) &, decltype(_3a) &, decltype(_3b) &, decltype(_3c) &,
    decltype(_3d) &, decltype(_3e) &, decltype(_3f) &, decltype(_40) &, decltype(_41) &
>(
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _3a, _3b, _3c,
    _3d, _3e, _3f, _40, _41
);
}
};


template<> struct tie_aggregate_impl<66> {
template<typename T> static constexpr auto call(T & x) {
auto & [
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _3a, _3b, _3c,
    _3d, _3e, _3f, _40, _41, _42
] = x;
return parser::tuple<
    decltype(_01) &, decltype(_02) &, decltype(_03) &, decltype(_04) &, decltype(_05) &,
    decltype(_06) &, decltype(_07) &, decltype(_08) &, decltype(_09) &, decltype(_0a) &,
    decltype(_0b) &, decltype(_0c) &, decltype(_0d) &, decltype(_0e) &, decltype(_0f) &,
    decltype(_10) &, decltype(_11) &, decltype(_12) &, decltype(_13) &, decltype(_14) &,
    decltype(_15) &, decltype(_16) &, decltype(_17) &, decltype(_18) &, decltype(_19) &,
    decltype(_1a) &, decltype(_1b) &, decltype(_1c) &, decltype(_1d) &, decltype(_1e) &,
    decltype(_1f) &, decltype(_20) &, decltype(_21) &, decltype(_22) &, decltype(_23) &,
    decltype(_24) &, decltype(_25) &, decltype(_26) &, decltype(_27) &, decltype(_28) &,
    decltype(_29) &, decltype(_2a) &, decltype(_2b) &, decltype(_2c) &, decltype(_2d) &,
    decltype(_2e) &, decltype(_2f) &, decltype(_30) &, decltype(_31) &, decltype(_32) &,
    decltype(_33) &, decltype(_34) &, decltype(_35) &, decltype(_36) &, decltype(_37) &,
    decltype(_38) &, decltype(_39) &, decltype(_3a) &, decltype(_3b) &, decltype(_3c) &,
    decltype(_3d) &, decltype(_3e) &, decltype(_3f) &, decltype(_40) &, decltype(_41) &,
    decltype(_42) &
>(
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _3a, _3b, _3c,
    _3d, _3e, _3f, _40, _41, _42
);
}
};


template<> struct tie_aggregate_impl<67> {
template<typename T> static constexpr auto call(T & x) {
auto & [
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _3a, _3b, _3c,
    _3d, _3e, _3f, _40, _41, _42, _43
] = x;
return parser::tuple<
    decltype(_01) &, decltype(_02) &, decltype(_03) &, decltype(_04) &, decltype(_05) &,
    decltype(_06) &, decltype(_07) &, decltype(_08) &, decltype(_09) &, decltype(_0a) &,
    decltype(_0b) &, decltype(_0c) &, decltype(_0d) &, decltype(_0e) &, decltype(_0f) &,
    decltype(_10) &, decltype(_11) &, decltype(_12) &, decltype(_13) &, decltype(_14) &,
    decltype(_15) &, decltype(_16) &, decltype(_17) &, decltype(_18) &, decltype(_19) &,
    decltype(_1a) &, decltype(_1b) &, decltype(_1c) &, decltype(_1d) &, decltype(_1e) &,
    decltype(_1f) &, decltype(_20) &, decltype(_21) &, decltype(_22) &, decltype(_23) &,
    decltype(_24) &, decltype(_25) &, decltype(_26) &, decltype(_27) &, decltype(_28) &,
    decltype(_29) &, decltype(_2a) &, decltype(_2b) &, decltype(_2c) &, decltype(_2d) &,
    decltype(_2e) &, decltype(_2f) &, decltype(_30) &, decltype(_31) &, decltype(_32) &,
    decltype(_33) &, decltype(_34) &, decltype(_35) &, decltype(_36) &, decltype(_37) &,
    decltype(_38) &, decltype(_39) &, decltype(_3a) &, decltype(_3b) &, decltype(_3c) &,
    decltype(_3d) &, decltype(_3e) &, decltype(_3f) &, decltype(_40) &, decltype(_41) &,
    decltype(_42) &, decltype(_43) &
>(
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _3a, _3b, _3c,
    _3d, _3e, _3f, _40, _41, _42, _43
);
}
};


template<> struct tie_aggregate_impl<68> {
template<typename T> static constexpr auto call(T & x) {
auto & [
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _3a, _3b, _3c,
    _3d, _3e, _3f, _40, _41, _42, _43, _44
] = x;
return parser::tuple<
    decltype(_01) &, decltype(_02) &, decltype(_03) &, decltype(_04) &, decltype(_05) &,
    decltype(_06) &, decltype(_07) &, decltype(_08) &, decltype(_09) &, decltype(_0a) &,
    decltype(_0b) &, decltype(_0c) &, decltype(_0d) &, decltype(_0e) &, decltype(_0f) &,
    decltype(_10) &, decltype(_11) &, decltype(_12) &, decltype(_13) &, decltype(_14) &,
    decltype(_15) &, decltype(_16) &, decltype(_17) &, decltype(_18) &, decltype(_19) &,
    decltype(_1a) &, decltype(_1b) &, decltype(_1c) &, decltype(_1d) &, decltype(_1e) &,
    decltype(_1f) &, decltype(_20) &, decltype(_21) &, decltype(_22) &, decltype(_23) &,
    decltype(_24) &, decltype(_25) &, decltype(_26) &, decltype(_27) &, decltype(_28) &,
    decltype(_29) &, decltype(_2a) &, decltype(_2b) &, decltype(_2c) &, decltype(_2d) &,
    decltype(_2e) &, decltype(_2f) &, decltype(_30) &, decltype(_31) &, decltype(_32) &,
    decltype(_33) &, decltype(_34) &, decltype(_35) &, decltype(_36) &, decltype(_37) &,
    decltype(_38) &, decltype(_39) &, decltype(_3a) &, decltype(_3b) &, decltype(_3c) &,
    decltype(_3d) &, decltype(_3e) &, decltype(_3f) &, decltype(_40) &, decltype(_41) &,
    decltype(_42) &, decltype(_43) &, decltype(_44) &
>(
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _3a, _3b, _3c,
    _3d, _3e, _3f, _40, _41, _42, _43, _44
);
}
};


template<> struct tie_aggregate_impl<69> {
template<typename T> static constexpr auto call(T & x) {
auto & [
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _3a, _3b, _3c,
    _3d, _3e, _3f, _40, _41, _42, _43, _44, _45
] = x;
return parser::tuple<
    decltype(_01) &, decltype(_02) &, decltype(_03) &, decltype(_04) &, decltype(_05) &,
    decltype(_06) &, decltype(_07) &, decltype(_08) &, decltype(_09) &, decltype(_0a) &,
    decltype(_0b) &, decltype(_0c) &, decltype(_0d) &, decltype(_0e) &, decltype(_0f) &,
    decltype(_10) &, decltype(_11) &, decltype(_12) &, decltype(_13) &, decltype(_14) &,
    decltype(_15) &, decltype(_16) &, decltype(_17) &, decltype(_18) &, decltype(_19) &,
    decltype(_1a) &, decltype(_1b) &, decltype(_1c) &, decltype(_1d) &, decltype(_1e) &,
    decltype(_1f) &, decltype(_20) &, decltype(_21) &, decltype(_22) &, decltype(_23) &,
    decltype(_24) &, decltype(_25) &, decltype(_26) &, decltype(_27) &, decltype(_28) &,
    decltype(_29) &, decltype(_2a) &, decltype(_2b) &, decltype(_2c) &, decltype(_2d) &,
    decltype(_2e) &, decltype(_2f) &, decltype(_30) &, decltype(_31) &, decltype(_32) &,
    decltype(_33) &, decltype(_34) &, decltype(_35) &, decltype(_36) &, decltype(_37) &,
    decltype(_38) &, decltype(_39) &, decltype(_3a) &, decltype(_3b) &, decltype(_3c) &,
    decltype(_3d) &, decltype(_3e) &, decltype(_3f) &, decltype(_40) &, decltype(_41) &,
    decltype(_42) &, decltype(_43) &, decltype(_44) &, decltype(_45) &
>(
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _3a, _3b, _3c,
    _3d, _3e, _3f, _40, _41, _42, _43, _44, _45
);
}
};


template<> struct tie_aggregate_impl<70> {
template<typename T> static constexpr auto call(T & x) {
auto & [
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _3a, _3b, _3c,
    _3d, _3e, _3f, _40, _41, _42, _43, _44, _45, _46
] = x;
return parser::tuple<
    decltype(_01) &, decltype(_02) &, decltype(_03) &, decltype(_04) &, decltype(_05) &,
    decltype(_06) &, decltype(_07) &, decltype(_08) &, decltype(_09) &, decltype(_0a) &,
    decltype(_0b) &, decltype(_0c) &, decltype(_0d) &, decltype(_0e) &, decltype(_0f) &,
    decltype(_10) &, decltype(_11) &, decltype(_12) &, decltype(_13) &, decltype(_14) &,
    decltype(_15) &, decltype(_16) &, decltype(_17) &, decltype(_18) &, decltype(_19) &,
    decltype(_1a) &, decltype(_1b) &, decltype(_1c) &, decltype(_1d) &, decltype(_1e) &,
    decltype(_1f) &, decltype(_20) &, decltype(_21) &, decltype(_22) &, decltype(_23) &,
    decltype(_24) &, decltype(_25) &, decltype(_26) &, decltype(_27) &, decltype(_28) &,
    decltype(_29) &, decltype(_2a) &, decltype(_2b) &, decltype(_2c) &, decltype(_2d) &,
    decltype(_2e) &, decltype(_2f) &, decltype(_30) &, decltype(_31) &, decltype(_32) &,
    decltype(_33) &, decltype(_34) &, decltype(_35) &, decltype(_36) &, decltype(_37) &,
    decltype(_38) &, decltype(_39) &, decltype(_3a) &, decltype(_3b) &, decltype(_3c) &,
    decltype(_3d) &, decltype(_3e) &, decltype(_3f) &, decltype(_40) &, decltype(_41) &,
    decltype(_42) &, decltype(_43) &, decltype(_44) &, decltype(_45) &, decltype(_46) &
>(
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _3a, _3b, _3c,
    _3d, _3e, _3f, _40, _41, _42, _43, _44, _45, _46
);
}
};


template<> struct tie_aggregate_impl<71> {
template<typename T> static constexpr auto call(T & x) {
auto & [
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _3a, _3b, _3c,
    _3d, _3e, _3f, _40, _41, _42, _43, _44, _45, _46, _47
] = x;
return parser::tuple<
    decltype(_01) &, decltype(_02) &, decltype(_03) &, decltype(_04) &, decltype(_05) &,
    decltype(_06) &, decltype(_07) &, decltype(_08) &, decltype(_09) &, decltype(_0a) &,
    decltype(_0b) &, decltype(_0c) &, decltype(_0d) &, decltype(_0e) &, decltype(_0f) &,
    decltype(_10) &, decltype(_11) &, decltype(_12) &, decltype(_13) &, decltype(_14) &,
    decltype(_15) &, decltype(_16) &, decltype(_17) &, decltype(_18) &, decltype(_19) &,
    decltype(_1a) &, decltype(_1b) &, decltype(_1c) &, decltype(_1d) &, decltype(_1e) &,
    decltype(_1f) &, decltype(_20) &, decltype(_21) &, decltype(_22) &, decltype(_23) &,
    decltype(_24) &, decltype(_25) &, decltype(_26) &, decltype(_27) &, decltype(_28) &,
    decltype(_29) &, decltype(_2a) &, decltype(_2b) &, decltype(_2c) &, decltype(_2d) &,
    decltype(_2e) &, decltype(_2f) &, decltype(_30) &, decltype(_31) &, decltype(_32) &,
    decltype(_33) &, decltype(_34) &, decltype(_35) &, decltype(_36) &, decltype(_37) &,
    decltype(_38) &, decltype(_39) &, decltype(_3a) &, decltype(_3b) &, decltype(_3c) &,
    decltype(_3d) &, decltype(_3e) &, decltype(_3f) &, decltype(_40) &, decltype(_41) &,
    decltype(_42) &, decltype(_43) &, decltype(_44) &, decltype(_45) &, decltype(_46) &,
    decltype(_47) &
>(
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _3a, _3b, _3c,
    _3d, _3e, _3f, _40, _41, _42, _43, _44, _45, _46, _47
);
}
};


template<> struct tie_aggregate_impl<72> {
template<typename T> static constexpr auto call(T & x) {
auto & [
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _3a, _3b, _3c,
    _3d, _3e, _3f, _40, _41, _42, _43, _44, _45, _46, _47, _48
] = x;
return parser::tuple<
    decltype(_01) &, decltype(_02) &, decltype(_03) &, decltype(_04) &, decltype(_05) &,
    decltype(_06) &, decltype(_07) &, decltype(_08) &, decltype(_09) &, decltype(_0a) &,
    decltype(_0b) &, decltype(_0c) &, decltype(_0d) &, decltype(_0e) &, decltype(_0f) &,
    decltype(_10) &, decltype(_11) &, decltype(_12) &, decltype(_13) &, decltype(_14) &,
    decltype(_15) &, decltype(_16) &, decltype(_17) &, decltype(_18) &, decltype(_19) &,
    decltype(_1a) &, decltype(_1b) &, decltype(_1c) &, decltype(_1d) &, decltype(_1e) &,
    decltype(_1f) &, decltype(_20) &, decltype(_21) &, decltype(_22) &, decltype(_23) &,
    decltype(_24) &, decltype(_25) &, decltype(_26) &, decltype(_27) &, decltype(_28) &,
    decltype(_29) &, decltype(_2a) &, decltype(_2b) &, decltype(_2c) &, decltype(_2d) &,
    decltype(_2e) &, decltype(_2f) &, decltype(_30) &, decltype(_31) &, decltype(_32) &,
    decltype(_33) &, decltype(_34) &, decltype(_35) &, decltype(_36) &, decltype(_37) &,
    decltype(_38) &, decltype(_39) &, decltype(_3a) &, decltype(_3b) &, decltype(_3c) &,
    decltype(_3d) &, decltype(_3e) &, decltype(_3f) &, decltype(_40) &, decltype(_41) &,
    decltype(_42) &, decltype(_43) &, decltype(_44) &, decltype(_45) &, decltype(_46) &,
    decltype(_47) &, decltype(_48) &
>(
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _3a, _3b, _3c,
    _3d, _3e, _3f, _40, _41, _42, _43, _44, _45, _46, _47, _48
);
}
};


template<> struct tie_aggregate_impl<73> {
template<typename T> static constexpr auto call(T & x) {
auto & [
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _3a, _3b, _3c,
    _3d, _3e, _3f, _40, _41, _42, _43, _44, _45, _46, _47, _48, _49
] = x;
return parser::tuple<
    decltype(_01) &, decltype(_02) &, decltype(_03) &, decltype(_04) &, decltype(_05) &,
    decltype(_06) &, decltype(_07) &, decltype(_08) &, decltype(_09) &, decltype(_0a) &,
    decltype(_0b) &, decltype(_0c) &, decltype(_0d) &, decltype(_0e) &, decltype(_0f) &,
    decltype(_10) &, decltype(_11) &, decltype(_12) &, decltype(_13) &, decltype(_14) &,
    decltype(_15) &, decltype(_16) &, decltype(_17) &, decltype(_18) &, decltype(_19) &,
    decltype(_1a) &, decltype(_1b) &, decltype(_1c) &, decltype(_1d) &, decltype(_1e) &,
    decltype(_1f) &, decltype(_20) &, decltype(_21) &, decltype(_22) &, decltype(_23) &,
    decltype(_24) &, decltype(_25) &, decltype(_26) &, decltype(_27) &, decltype(_28) &,
    decltype(_29) &, decltype(_2a) &, decltype(_2b) &, decltype(_2c) &, decltype(_2d) &,
    decltype(_2e) &, decltype(_2f) &, decltype(_30) &, decltype(_31) &, decltype(_32) &,
    decltype(_33) &, decltype(_34) &, decltype(_35) &, decltype(_36) &, decltype(_37) &,
    decltype(_38) &, decltype(_39) &, decltype(_3a) &, decltype(_3b) &, decltype(_3c) &,
    decltype(_3d) &, decltype(_3e) &, decltype(_3f) &, decltype(_40) &, decltype(_41) &,
    decltype(_42) &, decltype(_43) &, decltype(_44) &, decltype(_45) &, decltype(_46) &,
    decltype(_47) &, decltype(_48) &, decltype(_49) &
>(
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _3a, _3b, _3c,
    _3d, _3e, _3f, _40, _41, _42, _43, _44, _45, _46, _47, _48, _49
);
}
};


template<> struct tie_aggregate_impl<74> {
template<typename T> static constexpr auto call(T & x) {
auto & [
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _3a, _3b, _3c,
    _3d, _3e, _3f, _40, _41, _42, _43, _44, _45, _46, _47, _48, _49, _4a
] = x;
return parser::tuple<
    decltype(_01) &, decltype(_02) &, decltype(_03) &, decltype(_04) &, decltype(_05) &,
    decltype(_06) &, decltype(_07) &, decltype(_08) &, decltype(_09) &, decltype(_0a) &,
    decltype(_0b) &, decltype(_0c) &, decltype(_0d) &, decltype(_0e) &, decltype(_0f) &,
    decltype(_10) &, decltype(_11) &, decltype(_12) &, decltype(_13) &, decltype(_14) &,
    decltype(_15) &, decltype(_16) &, decltype(_17) &, decltype(_18) &, decltype(_19) &,
    decltype(_1a) &, decltype(_1b) &, decltype(_1c) &, decltype(_1d) &, decltype(_1e) &,
    decltype(_1f) &, decltype(_20) &, decltype(_21) &, decltype(_22) &, decltype(_23) &,
    decltype(_24) &, decltype(_25) &, decltype(_26) &, decltype(_27) &, decltype(_28) &,
    decltype(_29) &, decltype(_2a) &, decltype(_2b) &, decltype(_2c) &, decltype(_2d) &,
    decltype(_2e) &, decltype(_2f) &, decltype(_30) &, decltype(_31) &, decltype(_32) &,
    decltype(_33) &, decltype(_34) &, decltype(_35) &, decltype(_36) &, decltype(_37) &,
    decltype(_38) &, decltype(_39) &, decltype(_3a) &, decltype(_3b) &, decltype(_3c) &,
    decltype(_3d) &, decltype(_3e) &, decltype(_3f) &, decltype(_40) &, decltype(_41) &,
    decltype(_42) &, decltype(_43) &, decltype(_44) &, decltype(_45) &, decltype(_46) &,
    decltype(_47) &, decltype(_48) &, decltype(_49) &, decltype(_4a) &
>(
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _3a, _3b, _3c,
    _3d, _3e, _3f, _40, _41, _42, _43, _44, _45, _46, _47, _48, _49, _4a
);
}
};


template<> struct tie_aggregate_impl<75> {
template<typename T> static constexpr auto call(T & x) {
auto & [
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _3a, _3b, _3c,
    _3d, _3e, _3f, _40, _41, _42, _43, _44, _45, _46, _47, _48, _49, _4a, _4b
] = x;
return parser::tuple<
    decltype(_01) &, decltype(_02) &, decltype(_03) &, decltype(_04) &, decltype(_05) &,
    decltype(_06) &, decltype(_07) &, decltype(_08) &, decltype(_09) &, decltype(_0a) &,
    decltype(_0b) &, decltype(_0c) &, decltype(_0d) &, decltype(_0e) &, decltype(_0f) &,
    decltype(_10) &, decltype(_11) &, decltype(_12) &, decltype(_13) &, decltype(_14) &,
    decltype(_15) &, decltype(_16) &, decltype(_17) &, decltype(_18) &, decltype(_19) &,
    decltype(_1a) &, decltype(_1b) &, decltype(_1c) &, decltype(_1d) &, decltype(_1e) &,
    decltype(_1f) &, decltype(_20) &, decltype(_21) &, decltype(_22) &, decltype(_23) &,
    decltype(_24) &, decltype(_25) &, decltype(_26) &, decltype(_27) &, decltype(_28) &,
    decltype(_29) &, decltype(_2a) &, decltype(_2b) &, decltype(_2c) &, decltype(_2d) &,
    decltype(_2e) &, decltype(_2f) &, decltype(_30) &, decltype(_31) &, decltype(_32) &,
    decltype(_33) &, decltype(_34) &, decltype(_35) &, decltype(_36) &, decltype(_37) &,
    decltype(_38) &, decltype(_39) &, decltype(_3a) &, decltype(_3b) &, decltype(_3c) &,
    decltype(_3d) &, decltype(_3e) &, decltype(_3f) &, decltype(_40) &, decltype(_41) &,
    decltype(_42) &, decltype(_43) &, decltype(_44) &, decltype(_45) &, decltype(_46) &,
    decltype(_47) &, decltype(_48) &, decltype(_49) &, decltype(_4a) &, decltype(_4b) &
>(
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _3a, _3b, _3c,
    _3d, _3e, _3f, _40, _41, _42, _43, _44, _45, _46, _47, _48, _49, _4a, _4b
);
}
};


template<> struct tie_aggregate_impl<76> {
template<typename T> static constexpr auto call(T & x) {
auto & [
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _3a, _3b, _3c,
    _3d, _3e, _3f, _40, _41, _42, _43, _44, _45, _46, _47, _48, _49, _4a, _4b,
    _4c
] = x;
return parser::tuple<
    decltype(_01) &, decltype(_02) &, decltype(_03) &, decltype(_04) &, decltype(_05) &,
    decltype(_06) &, decltype(_07) &, decltype(_08) &, decltype(_09) &, decltype(_0a) &,
    decltype(_0b) &, decltype(_0c) &, decltype(_0d) &, decltype(_0e) &, decltype(_0f) &,
    decltype(_10) &, decltype(_11) &, decltype(_12) &, decltype(_13) &, decltype(_14) &,
    decltype(_15) &, decltype(_16) &, decltype(_17) &, decltype(_18) &, decltype(_19) &,
    decltype(_1a) &, decltype(_1b) &, decltype(_1c) &, decltype(_1d) &, decltype(_1e) &,
    decltype(_1f) &, decltype(_20) &, decltype(_21) &, decltype(_22) &, decltype(_23) &,
    decltype(_24) &, decltype(_25) &, decltype(_26) &, decltype(_27) &, decltype(_28) &,
    decltype(_29) &, decltype(_2a) &, decltype(_2b) &, decltype(_2c) &, decltype(_2d) &,
    decltype(_2e) &, decltype(_2f) &, decltype(_30) &, decltype(_31) &, decltype(_32) &,
    decltype(_33) &, decltype(_34) &, decltype(_35) &, decltype(_36) &, decltype(_37) &,
    decltype(_38) &, decltype(_39) &, decltype(_3a) &, decltype(_3b) &, decltype(_3c) &,
    decltype(_3d) &, decltype(_3e) &, decltype(_3f) &, decltype(_40) &, decltype(_41) &,
    decltype(_42) &, decltype(_43) &, decltype(_44) &, decltype(_45) &, decltype(_46) &,
    decltype(_47) &, decltype(_48) &, decltype(_49) &, decltype(_4a) &, decltype(_4b) &,
    decltype(_4c) &
>(
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _3a, _3b, _3c,
    _3d, _3e, _3f, _40, _41, _42, _43, _44, _45, _46, _47, _48, _49, _4a, _4b,
    _4c
);
}
};


template<> struct tie_aggregate_impl<77> {
template<typename T> static constexpr auto call(T & x) {
auto & [
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _3a, _3b, _3c,
    _3d, _3e, _3f, _40, _41, _42, _43, _44, _45, _46, _47, _48, _49, _4a, _4b,
    _4c, _4d
] = x;
return parser::tuple<
    decltype(_01) &, decltype(_02) &, decltype(_03) &, decltype(_04) &, decltype(_05) &,
    decltype(_06) &, decltype(_07) &, decltype(_08) &, decltype(_09) &, decltype(_0a) &,
    decltype(_0b) &, decltype(_0c) &, decltype(_0d) &, decltype(_0e) &, decltype(_0f) &,
    decltype(_10) &, decltype(_11) &, decltype(_12) &, decltype(_13) &, decltype(_14) &,
    decltype(_15) &, decltype(_16) &, decltype(_17) &, decltype(_18) &, decltype(_19) &,
    decltype(_1a) &, decltype(_1b) &, decltype(_1c) &, decltype(_1d) &, decltype(_1e) &,
    decltype(_1f) &, decltype(_20) &, decltype(_21) &, decltype(_22) &, decltype(_23) &,
    decltype(_24) &, decltype(_25) &, decltype(_26) &, decltype(_27) &, decltype(_28) &,
    decltype(_29) &, decltype(_2a) &, decltype(_2b) &, decltype(_2c) &, decltype(_2d) &,
    decltype(_2e) &, decltype(_2f) &, decltype(_30) &, decltype(_31) &, decltype(_32) &,
    decltype(_33) &, decltype(_34) &, decltype(_35) &, decltype(_36) &, decltype(_37) &,
    decltype(_38) &, decltype(_39) &, decltype(_3a) &, decltype(_3b) &, decltype(_3c) &,
    decltype(_3d) &, decltype(_3e) &, decltype(_3f) &, decltype(_40) &, decltype(_41) &,
    decltype(_42) &, decltype(_43) &, decltype(_44) &, decltype(_45) &, decltype(_46) &,
    decltype(_47) &, decltype(_48) &, decltype(_49) &, decltype(_4a) &, decltype(_4b) &,
    decltype(_4c) &, decltype(_4d) &
>(
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _3a, _3b, _3c,
    _3d, _3e, _3f, _40, _41, _42, _43, _44, _45, _46, _47, _48, _49, _4a, _4b,
    _4c, _4d
);
}
};


template<> struct tie_aggregate_impl<78> {
template<typename T> static constexpr auto call(T & x) {
auto & [
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _3a, _3b, _3c,
    _3d, _3e, _3f, _40, _41, _42, _43, _44, _45, _46, _47, _48, _49, _4a, _4b,
    _4c, _4d, _4e
] = x;
return parser::tuple<
    decltype(_01) &, decltype(_02) &, decltype(_03) &, decltype(_04) &, decltype(_05) &,
    decltype(_06) &, decltype(_07) &, decltype(_08) &, decltype(_09) &, decltype(_0a) &,
    decltype(_0b) &, decltype(_0c) &, decltype(_0d) &, decltype(_0e) &, decltype(_0f) &,
    decltype(_10) &, decltype(_11) &, decltype(_12) &, decltype(_13) &, decltype(_14) &,
    decltype(_15) &, decltype(_16) &, decltype(_17) &, decltype(_18) &, decltype(_19) &,
    decltype(_1a) &, decltype(_1b) &, decltype(_1c) &, decltype(_1d) &, decltype(_1e) &,
    decltype(_1f) &, decltype(_20) &, decltype(_21) &, decltype(_22) &, decltype(_23) &,
    decltype(_24) &, decltype(_25) &, decltype(_26) &, decltype(_27) &, decltype(_28) &,
    decltype(_29) &, decltype(_2a) &, decltype(_2b) &, decltype(_2c) &, decltype(_2d) &,
    decltype(_2e) &, decltype(_2f) &, decltype(_30) &, decltype(_31) &, decltype(_32) &,
    decltype(_33) &, decltype(_34) &, decltype(_35) &, decltype(_36) &, decltype(_37) &,
    decltype(_38) &, decltype(_39) &, decltype(_3a) &, decltype(_3b) &, decltype(_3c) &,
    decltype(_3d) &, decltype(_3e) &, decltype(_3f) &, decltype(_40) &, decltype(_41) &,
    decltype(_42) &, decltype(_43) &, decltype(_44) &, decltype(_45) &, decltype(_46) &,
    decltype(_47) &, decltype(_48) &, decltype(_49) &, decltype(_4a) &, decltype(_4b) &,
    decltype(_4c) &, decltype(_4d) &, decltype(_4e) &
>(
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _3a, _3b, _3c,
    _3d, _3e, _3f, _40, _41, _42, _43, _44, _45, _46, _47, _48, _49, _4a, _4b,
    _4c, _4d, _4e
);
}
};


template<> struct tie_aggregate_impl<79> {
template<typename T> static constexpr auto call(T & x) {
auto & [
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _3a, _3b, _3c,
    _3d, _3e, _3f, _40, _41, _42, _43, _44, _45, _46, _47, _48, _49, _4a, _4b,
    _4c, _4d, _4e, _4f
] = x;
return parser::tuple<
    decltype(_01) &, decltype(_02) &, decltype(_03) &, decltype(_04) &, decltype(_05) &,
    decltype(_06) &, decltype(_07) &, decltype(_08) &, decltype(_09) &, decltype(_0a) &,
    decltype(_0b) &, decltype(_0c) &, decltype(_0d) &, decltype(_0e) &, decltype(_0f) &,
    decltype(_10) &, decltype(_11) &, decltype(_12) &, decltype(_13) &, decltype(_14) &,
    decltype(_15) &, decltype(_16) &, decltype(_17) &, decltype(_18) &, decltype(_19) &,
    decltype(_1a) &, decltype(_1b) &, decltype(_1c) &, decltype(_1d) &, decltype(_1e) &,
    decltype(_1f) &, decltype(_20) &, decltype(_21) &, decltype(_22) &, decltype(_23) &,
    decltype(_24) &, decltype(_25) &, decltype(_26) &, decltype(_27) &, decltype(_28) &,
    decltype(_29) &, decltype(_2a) &, decltype(_2b) &, decltype(_2c) &, decltype(_2d) &,
    decltype(_2e) &, decltype(_2f) &, decltype(_30) &, decltype(_31) &, decltype(_32) &,
    decltype(_33) &, decltype(_34) &, decltype(_35) &, decltype(_36) &, decltype(_37) &,
    decltype(_38) &, decltype(_39) &, decltype(_3a) &, decltype(_3b) &, decltype(_3c) &,
    decltype(_3d) &, decltype(_3e) &, decltype(_3f) &, decltype(_40) &, decltype(_41) &,
    decltype(_42) &, decltype(_43) &, decltype(_44) &, decltype(_45) &, decltype(_46) &,
    decltype(_47) &, decltype(_48) &, decltype(_49) &, decltype(_4a) &, decltype(_4b) &,
    decltype(_4c) &, decltype(_4d) &, decltype(_4e) &, decltype(_4f) &
>(
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _3a, _3b, _3c,
    _3d, _3e, _3f, _40, _41, _42, _43, _44, _45, _46, _47, _48, _49, _4a, _4b,
    _4c, _4d, _4e, _4f
);
}
};


template<> struct tie_aggregate_impl<80> {
template<typename T> static constexpr auto call(T & x) {
auto & [
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _3a, _3b, _3c,
    _3d, _3e, _3f, _40, _41, _42, _43, _44, _45, _46, _47, _48, _49, _4a, _4b,
    _4c, _4d, _4e, _4f, _50
] = x;
return parser::tuple<
    decltype(_01) &, decltype(_02) &, decltype(_03) &, decltype(_04) &, decltype(_05) &,
    decltype(_06) &, decltype(_07) &, decltype(_08) &, decltype(_09) &, decltype(_0a) &,
    decltype(_0b) &, decltype(_0c) &, decltype(_0d) &, decltype(_0e) &, decltype(_0f) &,
    decltype(_10) &, decltype(_11) &, decltype(_12) &, decltype(_13) &, decltype(_14) &,
    decltype(_15) &, decltype(_16) &, decltype(_17) &, decltype(_18) &, decltype(_19) &,
    decltype(_1a) &, decltype(_1b) &, decltype(_1c) &, decltype(_1d) &, decltype(_1e) &,
    decltype(_1f) &, decltype(_20) &, decltype(_21) &, decltype(_22) &, decltype(_23) &,
    decltype(_24) &, decltype(_25) &, decltype(_26) &, decltype(_27) &, decltype(_28) &,
    decltype(_29) &, decltype(_2a) &, decltype(_2b) &, decltype(_2c) &, decltype(_2d) &,
    decltype(_2e) &, decltype(_2f) &, decltype(_30) &, decltype(_31) &, decltype(_32) &,
    decltype(_33) &, decltype(_34) &, decltype(_35) &, decltype(_36) &, decltype(_37) &,
    decltype(_38) &, decltype(_39) &, decltype(_3a) &, decltype(_3b) &, decltype(_3c) &,
    decltype(_3d) &, decltype(_3e) &, decltype(_3f) &, decltype(_40) &, decltype(_41) &,
    decltype(_42) &, decltype(_43) &, decltype(_44) &, decltype(_45) &, decltype(_46) &,
    decltype(_47) &, decltype(_48) &, decltype(_49) &, decltype(_4a) &, decltype(_4b) &,
    decltype(_4c) &, decltype(_4d) &, decltype(_4e) &, decltype(_4f) &, decltype(_50) &
>(
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _3a, _3b, _3c,
    _3d, _3e, _3f, _40, _41, _42, _43, _44, _45, _46, _47, _48, _49, _4a, _4b,
    _4c, _4d, _4e, _4f, _50
);
}
};


template<> struct tie_aggregate_impl<81> {
template<typename T> static constexpr auto call(T & x) {
auto & [
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _3a, _3b, _3c,
    _3d, _3e, _3f, _40, _41, _42, _43, _44, _45, _46, _47, _48, _49, _4a, _4b,
    _4c, _4d, _4e, _4f, _50, _51
] = x;
return parser::tuple<
    decltype(_01) &, decltype(_02) &, decltype(_03) &, decltype(_04) &, decltype(_05) &,
    decltype(_06) &, decltype(_07) &, decltype(_08) &, decltype(_09) &, decltype(_0a) &,
    decltype(_0b) &, decltype(_0c) &, decltype(_0d) &, decltype(_0e) &, decltype(_0f) &,
    decltype(_10) &, decltype(_11) &, decltype(_12) &, decltype(_13) &, decltype(_14) &,
    decltype(_15) &, decltype(_16) &, decltype(_17) &, decltype(_18) &, decltype(_19) &,
    decltype(_1a) &, decltype(_1b) &, decltype(_1c) &, decltype(_1d) &, decltype(_1e) &,
    decltype(_1f) &, decltype(_20) &, decltype(_21) &, decltype(_22) &, decltype(_23) &,
    decltype(_24) &, decltype(_25) &, decltype(_26) &, decltype(_27) &, decltype(_28) &,
    decltype(_29) &, decltype(_2a) &, decltype(_2b) &, decltype(_2c) &, decltype(_2d) &,
    decltype(_2e) &, decltype(_2f) &, decltype(_30) &, decltype(_31) &, decltype(_32) &,
    decltype(_33) &, decltype(_34) &, decltype(_35) &, decltype(_36) &, decltype(_37) &,
    decltype(_38) &, decltype(_39) &, decltype(_3a) &, decltype(_3b) &, decltype(_3c) &,
    decltype(_3d) &, decltype(_3e) &, decltype(_3f) &, decltype(_40) &, decltype(_41) &,
    decltype(_42) &, decltype(_43) &, decltype(_44) &, decltype(_45) &, decltype(_46) &,
    decltype(_47) &, decltype(_48) &, decltype(_49) &, decltype(_4a) &, decltype(_4b) &,
    decltype(_4c) &, decltype(_4d) &, decltype(_4e) &, decltype(_4f) &, decltype(_50) &,
    decltype(_51) &
>(
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _3a, _3b, _3c,
    _3d, _3e, _3f, _40, _41, _42, _43, _44, _45, _46, _47, _48, _49, _4a, _4b,
    _4c, _4d, _4e, _4f, _50, _51
);
}
};


template<> struct tie_aggregate_impl<82> {
template<typename T> static constexpr auto call(T & x) {
auto & [
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _3a, _3b, _3c,
    _3d, _3e, _3f, _40, _41, _42, _43, _44, _45, _46, _47, _48, _49, _4a, _4b,
    _4c, _4d, _4e, _4f, _50, _51, _52
] = x;
return parser::tuple<
    decltype(_01) &, decltype(_02) &, decltype(_03) &, decltype(_04) &, decltype(_05) &,
    decltype(_06) &, decltype(_07) &, decltype(_08) &, decltype(_09) &, decltype(_0a) &,
    decltype(_0b) &, decltype(_0c) &, decltype(_0d) &, decltype(_0e) &, decltype(_0f) &,
    decltype(_10) &, decltype(_11) &, decltype(_12) &, decltype(_13) &, decltype(_14) &,
    decltype(_15) &, decltype(_16) &, decltype(_17) &, decltype(_18) &, decltype(_19) &,
    decltype(_1a) &, decltype(_1b) &, decltype(_1c) &, decltype(_1d) &, decltype(_1e) &,
    decltype(_1f) &, decltype(_20) &, decltype(_21) &, decltype(_22) &, decltype(_23) &,
    decltype(_24) &, decltype(_25) &, decltype(_26) &, decltype(_27) &, decltype(_28) &,
    decltype(_29) &, decltype(_2a) &, decltype(_2b) &, decltype(_2c) &, decltype(_2d) &,
    decltype(_2e) &, decltype(_2f) &, decltype(_30) &, decltype(_31) &, decltype(_32) &,
    decltype(_33) &, decltype(_34) &, decltype(_35) &, decltype(_36) &, decltype(_37) &,
    decltype(_38) &, decltype(_39) &, decltype(_3a) &, decltype(_3b) &, decltype(_3c) &,
    decltype(_3d) &, decltype(_3e) &, decltype(_3f) &, decltype(_40) &, decltype(_41) &,
    decltype(_42) &, decltype(_43) &, decltype(_44) &, decltype(_45) &, decltype(_46) &,
    decltype(_47) &, decltype(_48) &, decltype(_49) &, decltype(_4a) &, decltype(_4b) &,
    decltype(_4c) &, decltype(_4d) &, decltype(_4e) &, decltype(_4f) &, decltype(_50) &,
    decltype(_51) &, decltype(_52) &
>(
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _3a, _3b, _3c,
    _3d, _3e, _3f, _40, _41, _42, _43, _44, _45, _46, _47, _48, _49, _4a, _4b,
    _4c, _4d, _4e, _4f, _50, _51, _52
);
}
};


template<> struct tie_aggregate_impl<83> {
template<typename T> static constexpr auto call(T & x) {
auto & [
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _3a, _3b, _3c,
    _3d, _3e, _3f, _40, _41, _42, _43, _44, _45, _46, _47, _48, _49, _4a, _4b,
    _4c, _4d, _4e, _4f, _50, _51, _52, _53
] = x;
return parser::tuple<
    decltype(_01) &, decltype(_02) &, decltype(_03) &, decltype(_04) &, decltype(_05) &,
    decltype(_06) &, decltype(_07) &, decltype(_08) &, decltype(_09) &, decltype(_0a) &,
    decltype(_0b) &, decltype(_0c) &, decltype(_0d) &, decltype(_0e) &, decltype(_0f) &,
    decltype(_10) &, decltype(_11) &, decltype(_12) &, decltype(_13) &, decltype(_14) &,
    decltype(_15) &, decltype(_16) &, decltype(_17) &, decltype(_18) &, decltype(_19) &,
    decltype(_1a) &, decltype(_1b) &, decltype(_1c) &, decltype(_1d) &, decltype(_1e) &,
    decltype(_1f) &, decltype(_20) &, decltype(_21) &, decltype(_22) &, decltype(_23) &,
    decltype(_24) &, decltype(_25) &, decltype(_26) &, decltype(_27) &, decltype(_28) &,
    decltype(_29) &, decltype(_2a) &, decltype(_2b) &, decltype(_2c) &, decltype(_2d) &,
    decltype(_2e) &, decltype(_2f) &, decltype(_30) &, decltype(_31) &, decltype(_32) &,
    decltype(_33) &, decltype(_34) &, decltype(_35) &, decltype(_36) &, decltype(_37) &,
    decltype(_38) &, decltype(_39) &, decltype(_3a) &, decltype(_3b) &, decltype(_3c) &,
    decltype(_3d) &, decltype(_3e) &, decltype(_3f) &, decltype(_40) &, decltype(_41) &,
    decltype(_42) &, decltype(_43) &, decltype(_44) &, decltype(_45) &, decltype(_46) &,
    decltype(_47) &, decltype(_48) &, decltype(_49) &, decltype(_4a) &, decltype(_4b) &,
    decltype(_4c) &, decltype(_4d) &, decltype(_4e) &, decltype(_4f) &, decltype(_50) &,
    decltype(_51) &, decltype(_52) &, decltype(_53) &
>(
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _3a, _3b, _3c,
    _3d, _3e, _3f, _40, _41, _42, _43, _44, _45, _46, _47, _48, _49, _4a, _4b,
    _4c, _4d, _4e, _4f, _50, _51, _52, _53
);
}
};


template<> struct tie_aggregate_impl<84> {
template<typename T> static constexpr auto call(T & x) {
auto & [
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _3a, _3b, _3c,
    _3d, _3e, _3f, _40, _41, _42, _43, _44, _45, _46, _47, _48, _49, _4a, _4b,
    _4c, _4d, _4e, _4f, _50, _51, _52, _53, _54
] = x;
return parser::tuple<
    decltype(_01) &, decltype(_02) &, decltype(_03) &, decltype(_04) &, decltype(_05) &,
    decltype(_06) &, decltype(_07) &, decltype(_08) &, decltype(_09) &, decltype(_0a) &,
    decltype(_0b) &, decltype(_0c) &, decltype(_0d) &, decltype(_0e) &, decltype(_0f) &,
    decltype(_10) &, decltype(_11) &, decltype(_12) &, decltype(_13) &, decltype(_14) &,
    decltype(_15) &, decltype(_16) &, decltype(_17) &, decltype(_18) &, decltype(_19) &,
    decltype(_1a) &, decltype(_1b) &, decltype(_1c) &, decltype(_1d) &, decltype(_1e) &,
    decltype(_1f) &, decltype(_20) &, decltype(_21) &, decltype(_22) &, decltype(_23) &,
    decltype(_24) &, decltype(_25) &, decltype(_26) &, decltype(_27) &, decltype(_28) &,
    decltype(_29) &, decltype(_2a) &, decltype(_2b) &, decltype(_2c) &, decltype(_2d) &,
    decltype(_2e) &, decltype(_2f) &, decltype(_30) &, decltype(_31) &, decltype(_32) &,
    decltype(_33) &, decltype(_34) &, decltype(_35) &, decltype(_36) &, decltype(_37) &,
    decltype(_38) &, decltype(_39) &, decltype(_3a) &, decltype(_3b) &, decltype(_3c) &,
    decltype(_3d) &, decltype(_3e) &, decltype(_3f) &, decltype(_40) &, decltype(_41) &,
    decltype(_42) &, decltype(_43) &, decltype(_44) &, decltype(_45) &, decltype(_46) &,
    decltype(_47) &, decltype(_48) &, decltype(_49) &, decltype(_4a) &, decltype(_4b) &,
    decltype(_4c) &, decltype(_4d) &, decltype(_4e) &, decltype(_4f) &, decltype(_50) &,
    decltype(_51) &, decltype(_52) &, decltype(_53) &, decltype(_54) &
>(
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _3a, _3b, _3c,
    _3d, _3e, _3f, _40, _41, _42, _43, _44, _45, _46, _47, _48, _49, _4a, _4b,
    _4c, _4d, _4e, _4f, _50, _51, _52, _53, _54
);
}
};


template<> struct tie_aggregate_impl<85> {
template<typename T> static constexpr auto call(T & x) {
auto & [
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _3a, _3b, _3c,
    _3d, _3e, _3f, _40, _41, _42, _43, _44, _45, _46, _47, _48, _49, _4a, _4b,
    _4c, _4d, _4e, _4f, _50, _51, _52, _53, _54, _55
] = x;
return parser::tuple<
    decltype(_01) &, decltype(_02) &, decltype(_03) &, decltype(_04) &, decltype(_05) &,
    decltype(_06) &, decltype(_07) &, decltype(_08) &, decltype(_09) &, decltype(_0a) &,
    decltype(_0b) &, decltype(_0c) &, decltype(_0d) &, decltype(_0e) &, decltype(_0f) &,
    decltype(_10) &, decltype(_11) &, decltype(_12) &, decltype(_13) &, decltype(_14) &,
    decltype(_15) &, decltype(_16) &, decltype(_17) &, decltype(_18) &, decltype(_19) &,
    decltype(_1a) &, decltype(_1b) &, decltype(_1c) &, decltype(_1d) &, decltype(_1e) &,
    decltype(_1f) &, decltype(_20) &, decltype(_21) &, decltype(_22) &, decltype(_23) &,
    decltype(_24) &, decltype(_25) &, decltype(_26) &, decltype(_27) &, decltype(_28) &,
    decltype(_29) &, decltype(_2a) &, decltype(_2b) &, decltype(_2c) &, decltype(_2d) &,
    decltype(_2e) &, decltype(_2f) &, decltype(_30) &, decltype(_31) &, decltype(_32) &,
    decltype(_33) &, decltype(_34) &, decltype(_35) &, decltype(_36) &, decltype(_37) &,
    decltype(_38) &, decltype(_39) &, decltype(_3a) &, decltype(_3b) &, decltype(_3c) &,
    decltype(_3d) &, decltype(_3e) &, decltype(_3f) &, decltype(_40) &, decltype(_41) &,
    decltype(_42) &, decltype(_43) &, decltype(_44) &, decltype(_45) &, decltype(_46) &,
    decltype(_47) &, decltype(_48) &, decltype(_49) &, decltype(_4a) &, decltype(_4b) &,
    decltype(_4c) &, decltype(_4d) &, decltype(_4e) &, decltype(_4f) &, decltype(_50) &,
    decltype(_51) &, decltype(_52) &, decltype(_53) &, decltype(_54) &, decltype(_55) &
>(
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _3a, _3b, _3c,
    _3d, _3e, _3f, _40, _41, _42, _43, _44, _45, _46, _47, _48, _49, _4a, _4b,
    _4c, _4d, _4e, _4f, _50, _51, _52, _53, _54, _55
);
}
};


template<> struct tie_aggregate_impl<86> {
template<typename T> static constexpr auto call(T & x) {
auto & [
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _3a, _3b, _3c,
    _3d, _3e, _3f, _40, _41, _42, _43, _44, _45, _46, _47, _48, _49, _4a, _4b,
    _4c, _4d, _4e, _4f, _50, _51, _52, _53, _54, _55, _56
] = x;
return parser::tuple<
    decltype(_01) &, decltype(_02) &, decltype(_03) &, decltype(_04) &, decltype(_05) &,
    decltype(_06) &, decltype(_07) &, decltype(_08) &, decltype(_09) &, decltype(_0a) &,
    decltype(_0b) &, decltype(_0c) &, decltype(_0d) &, decltype(_0e) &, decltype(_0f) &,
    decltype(_10) &, decltype(_11) &, decltype(_12) &, decltype(_13) &, decltype(_14) &,
    decltype(_15) &, decltype(_16) &, decltype(_17) &, decltype(_18) &, decltype(_19) &,
    decltype(_1a) &, decltype(_1b) &, decltype(_1c) &, decltype(_1d) &, decltype(_1e) &,
    decltype(_1f) &, decltype(_20) &, decltype(_21) &, decltype(_22) &, decltype(_23) &,
    decltype(_24) &, decltype(_25) &, decltype(_26) &, decltype(_27) &, decltype(_28) &,
    decltype(_29) &, decltype(_2a) &, decltype(_2b) &, decltype(_2c) &, decltype(_2d) &,
    decltype(_2e) &, decltype(_2f) &, decltype(_30) &, decltype(_31) &, decltype(_32) &,
    decltype(_33) &, decltype(_34) &, decltype(_35) &, decltype(_36) &, decltype(_37) &,
    decltype(_38) &, decltype(_39) &, decltype(_3a) &, decltype(_3b) &, decltype(_3c) &,
    decltype(_3d) &, decltype(_3e) &, decltype(_3f) &, decltype(_40) &, decltype(_41) &,
    decltype(_42) &, decltype(_43) &, decltype(_44) &, decltype(_45) &, decltype(_46) &,
    decltype(_47) &, decltype(_48) &, decltype(_49) &, decltype(_4a) &, decltype(_4b) &,
    decltype(_4c) &, decltype(_4d) &, decltype(_4e) &, decltype(_4f) &, decltype(_50) &,
    decltype(_51) &, decltype(_52) &, decltype(_53) &, decltype(_54) &, decltype(_55) &,
    decltype(_56) &
>(
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _3a, _3b, _3c,
    _3d, _3e, _3f, _40, _41, _42, _43, _44, _45, _46, _47, _48, _49, _4a, _4b,
    _4c, _4d, _4e, _4f, _50, _51, _52, _53, _54, _55, _56
);
}
};


template<> struct tie_aggregate_impl<87> {
template<typename T> static constexpr auto call(T & x) {
auto & [
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _3a, _3b, _3c,
    _3d, _3e, _3f, _40, _41, _42, _43, _44, _45, _46, _47, _48, _49, _4a, _4b,
    _4c, _4d, _4e, _4f, _50, _51, _52, _53, _54, _55, _56, _57
] = x;
return parser::tuple<
    decltype(_01) &, decltype(_02) &, decltype(_03) &, decltype(_04) &, decltype(_05) &,
    decltype(_06) &, decltype(_07) &, decltype(_08) &, decltype(_09) &, decltype(_0a) &,
    decltype(_0b) &, decltype(_0c) &, decltype(_0d) &, decltype(_0e) &, decltype(_0f) &,
    decltype(_10) &, decltype(_11) &, decltype(_12) &, decltype(_13) &, decltype(_14) &,
    decltype(_15) &, decltype(_16) &, decltype(_17) &, decltype(_18) &, decltype(_19) &,
    decltype(_1a) &, decltype(_1b) &, decltype(_1c) &, decltype(_1d) &, decltype(_1e) &,
    decltype(_1f) &, decltype(_20) &, decltype(_21) &, decltype(_22) &, decltype(_23) &,
    decltype(_24) &, decltype(_25) &, decltype(_26) &, decltype(_27) &, decltype(_28) &,
    decltype(_29) &, decltype(_2a) &, decltype(_2b) &, decltype(_2c) &, decltype(_2d) &,
    decltype(_2e) &, decltype(_2f) &, decltype(_30) &, decltype(_31) &, decltype(_32) &,
    decltype(_33) &, decltype(_34) &, decltype(_35) &, decltype(_36) &, decltype(_37) &,
    decltype(_38) &, decltype(_39) &, decltype(_3a) &, decltype(_3b) &, decltype(_3c) &,
    decltype(_3d) &, decltype(_3e) &, decltype(_3f) &, decltype(_40) &, decltype(_41) &,
    decltype(_42) &, decltype(_43) &, decltype(_44) &, decltype(_45) &, decltype(_46) &,
    decltype(_47) &, decltype(_48) &, decltype(_49) &, decltype(_4a) &, decltype(_4b) &,
    decltype(_4c) &, decltype(_4d) &, decltype(_4e) &, decltype(_4f) &, decltype(_50) &,
    decltype(_51) &, decltype(_52) &, decltype(_53) &, decltype(_54) &, decltype(_55) &,
    decltype(_56) &, decltype(_57) &
>(
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _3a, _3b, _3c,
    _3d, _3e, _3f, _40, _41, _42, _43, _44, _45, _46, _47, _48, _49, _4a, _4b,
    _4c, _4d, _4e, _4f, _50, _51, _52, _53, _54, _55, _56, _57
);
}
};


template<> struct tie_aggregate_impl<88> {
template<typename T> static constexpr auto call(T & x) {
auto & [
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _3a, _3b, _3c,
    _3d, _3e, _3f, _40, _41, _42, _43, _44, _45, _46, _47, _48, _49, _4a, _4b,
    _4c, _4d, _4e, _4f, _50, _51, _52, _53, _54, _55, _56, _57, _58
] = x;
return parser::tuple<
    decltype(_01) &, decltype(_02) &, decltype(_03) &, decltype(_04) &, decltype(_05) &,
    decltype(_06) &, decltype(_07) &, decltype(_08) &, decltype(_09) &, decltype(_0a) &,
    decltype(_0b) &, decltype(_0c) &, decltype(_0d) &, decltype(_0e) &, decltype(_0f) &,
    decltype(_10) &, decltype(_11) &, decltype(_12) &, decltype(_13) &, decltype(_14) &,
    decltype(_15) &, decltype(_16) &, decltype(_17) &, decltype(_18) &, decltype(_19) &,
    decltype(_1a) &, decltype(_1b) &, decltype(_1c) &, decltype(_1d) &, decltype(_1e) &,
    decltype(_1f) &, decltype(_20) &, decltype(_21) &, decltype(_22) &, decltype(_23) &,
    decltype(_24) &, decltype(_25) &, decltype(_26) &, decltype(_27) &, decltype(_28) &,
    decltype(_29) &, decltype(_2a) &, decltype(_2b) &, decltype(_2c) &, decltype(_2d) &,
    decltype(_2e) &, decltype(_2f) &, decltype(_30) &, decltype(_31) &, decltype(_32) &,
    decltype(_33) &, decltype(_34) &, decltype(_35) &, decltype(_36) &, decltype(_37) &,
    decltype(_38) &, decltype(_39) &, decltype(_3a) &, decltype(_3b) &, decltype(_3c) &,
    decltype(_3d) &, decltype(_3e) &, decltype(_3f) &, decltype(_40) &, decltype(_41) &,
    decltype(_42) &, decltype(_43) &, decltype(_44) &, decltype(_45) &, decltype(_46) &,
    decltype(_47) &, decltype(_48) &, decltype(_49) &, decltype(_4a) &, decltype(_4b) &,
    decltype(_4c) &, decltype(_4d) &, decltype(_4e) &, decltype(_4f) &, decltype(_50) &,
    decltype(_51) &, decltype(_52) &, decltype(_53) &, decltype(_54) &, decltype(_55) &,
    decltype(_56) &, decltype(_57) &, decltype(_58) &
>(
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _3a, _3b, _3c,
    _3d, _3e, _3f, _40, _41, _42, _43, _44, _45, _46, _47, _48, _49, _4a, _4b,
    _4c, _4d, _4e, _4f, _50, _51, _52, _53, _54, _55, _56, _57, _58
);
}
};


template<> struct tie_aggregate_impl<89> {
template<typename T> static constexpr auto call(T & x) {
auto & [
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _3a, _3b, _3c,
    _3d, _3e, _3f, _40, _41, _42, _43, _44, _45, _46, _47, _48, _49, _4a, _4b,
    _4c, _4d, _4e, _4f, _50, _51, _52, _53, _54, _55, _56, _57, _58, _59
] = x;
return parser::tuple<
    decltype(_01) &, decltype(_02) &, decltype(_03) &, decltype(_04) &, decltype(_05) &,
    decltype(_06) &, decltype(_07) &, decltype(_08) &, decltype(_09) &, decltype(_0a) &,
    decltype(_0b) &, decltype(_0c) &, decltype(_0d) &, decltype(_0e) &, decltype(_0f) &,
    decltype(_10) &, decltype(_11) &, decltype(_12) &, decltype(_13) &, decltype(_14) &,
    decltype(_15) &, decltype(_16) &, decltype(_17) &, decltype(_18) &, decltype(_19) &,
    decltype(_1a) &, decltype(_1b) &, decltype(_1c) &, decltype(_1d) &, decltype(_1e) &,
    decltype(_1f) &, decltype(_20) &, decltype(_21) &, decltype(_22) &, decltype(_23) &,
    decltype(_24) &, decltype(_25) &, decltype(_26) &, decltype(_27) &, decltype(_28) &,
    decltype(_29) &, decltype(_2a) &, decltype(_2b) &, decltype(_2c) &, decltype(_2d) &,
    decltype(_2e) &, decltype(_2f) &, decltype(_30) &, decltype(_31) &, decltype(_32) &,
    decltype(_33) &, decltype(_34) &, decltype(_35) &, decltype(_36) &, decltype(_37) &,
    decltype(_38) &, decltype(_39) &, decltype(_3a) &, decltype(_3b) &, decltype(_3c) &,
    decltype(_3d) &, decltype(_3e) &, decltype(_3f) &, decltype(_40) &, decltype(_41) &,
    decltype(_42) &, decltype(_43) &, decltype(_44) &, decltype(_45) &, decltype(_46) &,
    decltype(_47) &, decltype(_48) &, decltype(_49) &, decltype(_4a) &, decltype(_4b) &,
    decltype(_4c) &, decltype(_4d) &, decltype(_4e) &, decltype(_4f) &, decltype(_50) &,
    decltype(_51) &, decltype(_52) &, decltype(_53) &, decltype(_54) &, decltype(_55) &,
    decltype(_56) &, decltype(_57) &, decltype(_58) &, decltype(_59) &
>(
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _3a, _3b, _3c,
    _3d, _3e, _3f, _40, _41, _42, _43, _44, _45, _46, _47, _48, _49, _4a, _4b,
    _4c, _4d, _4e, _4f, _50, _51, _52, _53, _54, _55, _56, _57, _58, _59
);
}
};


template<> struct tie_aggregate_impl<90> {
template<typename T> static constexpr auto call(T & x) {
auto & [
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _3a, _3b, _3c,
    _3d, _3e, _3f, _40, _41, _42, _43, _44, _45, _46, _47, _48, _49, _4a, _4b,
    _4c, _4d, _4e, _4f, _50, _51, _52, _53, _54, _55, _56, _57, _58, _59, _5a
] = x;
return parser::tuple<
    decltype(_01) &, decltype(_02) &, decltype(_03) &, decltype(_04) &, decltype(_05) &,
    decltype(_06) &, decltype(_07) &, decltype(_08) &, decltype(_09) &, decltype(_0a) &,
    decltype(_0b) &, decltype(_0c) &, decltype(_0d) &, decltype(_0e) &, decltype(_0f) &,
    decltype(_10) &, decltype(_11) &, decltype(_12) &, decltype(_13) &, decltype(_14) &,
    decltype(_15) &, decltype(_16) &, decltype(_17) &, decltype(_18) &, decltype(_19) &,
    decltype(_1a) &, decltype(_1b) &, decltype(_1c) &, decltype(_1d) &, decltype(_1e) &,
    decltype(_1f) &, decltype(_20) &, decltype(_21) &, decltype(_22) &, decltype(_23) &,
    decltype(_24) &, decltype(_25) &, decltype(_26) &, decltype(_27) &, decltype(_28) &,
    decltype(_29) &, decltype(_2a) &, decltype(_2b) &, decltype(_2c) &, decltype(_2d) &,
    decltype(_2e) &, decltype(_2f) &, decltype(_30) &, decltype(_31) &, decltype(_32) &,
    decltype(_33) &, decltype(_34) &, decltype(_35) &, decltype(_36) &, decltype(_37) &,
    decltype(_38) &, decltype(_39) &, decltype(_3a) &, decltype(_3b) &, decltype(_3c) &,
    decltype(_3d) &, decltype(_3e) &, decltype(_3f) &, decltype(_40) &, decltype(_41) &,
    decltype(_42) &, decltype(_43) &, decltype(_44) &, decltype(_45) &, decltype(_46) &,
    decltype(_47) &, decltype(_48) &, decltype(_49) &, decltype(_4a) &, decltype(_4b) &,
    decltype(_4c) &, decltype(_4d) &, decltype(_4e) &, decltype(_4f) &, decltype(_50) &,
    decltype(_51) &, decltype(_52) &, decltype(_53) &, decltype(_54) &, decltype(_55) &,
    decltype(_56) &, decltype(_57) &, decltype(_58) &, decltype(_59) &, decltype(_5a) &
>(
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _3a, _3b, _3c,
    _3d, _3e, _3f, _40, _41, _42, _43, _44, _45, _46, _47, _48, _49, _4a, _4b,
    _4c, _4d, _4e, _4f, _50, _51, _52, _53, _54, _55, _56, _57, _58, _59, _5a
);
}
};


template<> struct tie_aggregate_impl<91> {
template<typename T> static constexpr auto call(T & x) {
auto & [
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _3a, _3b, _3c,
    _3d, _3e, _3f, _40, _41, _42, _43, _44, _45, _46, _47, _48, _49, _4a, _4b,
    _4c, _4d, _4e, _4f, _50, _51, _52, _53, _54, _55, _56, _57, _58, _59, _5a,
    _5b
] = x;
return parser::tuple<
    decltype(_01) &, decltype(_02) &, decltype(_03) &, decltype(_04) &, decltype(_05) &,
    decltype(_06) &, decltype(_07) &, decltype(_08) &, decltype(_09) &, decltype(_0a) &,
    decltype(_0b) &, decltype(_0c) &, decltype(_0d) &, decltype(_0e) &, decltype(_0f) &,
    decltype(_10) &, decltype(_11) &, decltype(_12) &, decltype(_13) &, decltype(_14) &,
    decltype(_15) &, decltype(_16) &, decltype(_17) &, decltype(_18) &, decltype(_19) &,
    decltype(_1a) &, decltype(_1b) &, decltype(_1c) &, decltype(_1d) &, decltype(_1e) &,
    decltype(_1f) &, decltype(_20) &, decltype(_21) &, decltype(_22) &, decltype(_23) &,
    decltype(_24) &, decltype(_25) &, decltype(_26) &, decltype(_27) &, decltype(_28) &,
    decltype(_29) &, decltype(_2a) &, decltype(_2b) &, decltype(_2c) &, decltype(_2d) &,
    decltype(_2e) &, decltype(_2f) &, decltype(_30) &, decltype(_31) &, decltype(_32) &,
    decltype(_33) &, decltype(_34) &, decltype(_35) &, decltype(_36) &, decltype(_37) &,
    decltype(_38) &, decltype(_39) &, decltype(_3a) &, decltype(_3b) &, decltype(_3c) &,
    decltype(_3d) &, decltype(_3e) &, decltype(_3f) &, decltype(_40) &, decltype(_41) &,
    decltype(_42) &, decltype(_43) &, decltype(_44) &, decltype(_45) &, decltype(_46) &,
    decltype(_47) &, decltype(_48) &, decltype(_49) &, decltype(_4a) &, decltype(_4b) &,
    decltype(_4c) &, decltype(_4d) &, decltype(_4e) &, decltype(_4f) &, decltype(_50) &,
    decltype(_51) &, decltype(_52) &, decltype(_53) &, decltype(_54) &, decltype(_55) &,
    decltype(_56) &, decltype(_57) &, decltype(_58) &, decltype(_59) &, decltype(_5a) &,
    decltype(_5b) &
>(
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _3a, _3b, _3c,
    _3d, _3e, _3f, _40, _41, _42, _43, _44, _45, _46, _47, _48, _49, _4a, _4b,
    _4c, _4d, _4e, _4f, _50, _51, _52, _53, _54, _55, _56, _57, _58, _59, _5a,
    _5b
);
}
};


template<> struct tie_aggregate_impl<92> {
template<typename T> static constexpr auto call(T & x) {
auto & [
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _3a, _3b, _3c,
    _3d, _3e, _3f, _40, _41, _42, _43, _44, _45, _46, _47, _48, _49, _4a, _4b,
    _4c, _4d, _4e, _4f, _50, _51, _52, _53, _54, _55, _56, _57, _58, _59, _5a,
    _5b, _5c
] = x;
return parser::tuple<
    decltype(_01) &, decltype(_02) &, decltype(_03) &, decltype(_04) &, decltype(_05) &,
    decltype(_06) &, decltype(_07) &, decltype(_08) &, decltype(_09) &, decltype(_0a) &,
    decltype(_0b) &, decltype(_0c) &, decltype(_0d) &, decltype(_0e) &, decltype(_0f) &,
    decltype(_10) &, decltype(_11) &, decltype(_12) &, decltype(_13) &, decltype(_14) &,
    decltype(_15) &, decltype(_16) &, decltype(_17) &, decltype(_18) &, decltype(_19) &,
    decltype(_1a) &, decltype(_1b) &, decltype(_1c) &, decltype(_1d) &, decltype(_1e) &,
    decltype(_1f) &, decltype(_20) &, decltype(_21) &, decltype(_22) &, decltype(_23) &,
    decltype(_24) &, decltype(_25) &, decltype(_26) &, decltype(_27) &, decltype(_28) &,
    decltype(_29) &, decltype(_2a) &, decltype(_2b) &, decltype(_2c) &, decltype(_2d) &,
    decltype(_2e) &, decltype(_2f) &, decltype(_30) &, decltype(_31) &, decltype(_32) &,
    decltype(_33) &, decltype(_34) &, decltype(_35) &, decltype(_36) &, decltype(_37) &,
    decltype(_38) &, decltype(_39) &, decltype(_3a) &, decltype(_3b) &, decltype(_3c) &,
    decltype(_3d) &, decltype(_3e) &, decltype(_3f) &, decltype(_40) &, decltype(_41) &,
    decltype(_42) &, decltype(_43) &, decltype(_44) &, decltype(_45) &, decltype(_46) &,
    decltype(_47) &, decltype(_48) &, decltype(_49) &, decltype(_4a) &, decltype(_4b) &,
    decltype(_4c) &, decltype(_4d) &, decltype(_4e) &, decltype(_4f) &, decltype(_50) &,
    decltype(_51) &, decltype(_52) &, decltype(_53) &, decltype(_54) &, decltype(_55) &,
    decltype(_56) &, decltype(_57) &, decltype(_58) &, decltype(_59) &, decltype(_5a) &,
    decltype(_5b) &, decltype(_5c) &
>(
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _3a, _3b, _3c,
    _3d, _3e, _3f, _40, _41, _42, _43, _44, _45, _46, _47, _48, _49, _4a, _4b,
    _4c, _4d, _4e, _4f, _50, _51, _52, _53, _54, _55, _56, _57, _58, _59, _5a,
    _5b, _5c
);
}
};


template<> struct tie_aggregate_impl<93> {
template<typename T> static constexpr auto call(T & x) {
auto & [
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _3a, _3b, _3c,
    _3d, _3e, _3f, _40, _41, _42, _43, _44, _45, _46, _47, _48, _49, _4a, _4b,
    _4c, _4d, _4e, _4f, _50, _51, _52, _53, _54, _55, _56, _57, _58, _59, _5a,
    _5b, _5c, _5d
] = x;
return parser::tuple<
    decltype(_01) &, decltype(_02) &, decltype(_03) &, decltype(_04) &, decltype(_05) &,
    decltype(_06) &, decltype(_07) &, decltype(_08) &, decltype(_09) &, decltype(_0a) &,
    decltype(_0b) &, decltype(_0c) &, decltype(_0d) &, decltype(_0e) &, decltype(_0f) &,
    decltype(_10) &, decltype(_11) &, decltype(_12) &, decltype(_13) &, decltype(_14) &,
    decltype(_15) &, decltype(_16) &, decltype(_17) &, decltype(_18) &, decltype(_19) &,
    decltype(_1a) &, decltype(_1b) &, decltype(_1c) &, decltype(_1d) &, decltype(_1e) &,
    decltype(_1f) &, decltype(_20) &, decltype(_21) &, decltype(_22) &, decltype(_23) &,
    decltype(_24) &, decltype(_25) &, decltype(_26) &, decltype(_27) &, decltype(_28) &,
    decltype(_29) &, decltype(_2a) &, decltype(_2b) &, decltype(_2c) &, decltype(_2d) &,
    decltype(_2e) &, decltype(_2f) &, decltype(_30) &, decltype(_31) &, decltype(_32) &,
    decltype(_33) &, decltype(_34) &, decltype(_35) &, decltype(_36) &, decltype(_37) &,
    decltype(_38) &, decltype(_39) &, decltype(_3a) &, decltype(_3b) &, decltype(_3c) &,
    decltype(_3d) &, decltype(_3e) &, decltype(_3f) &, decltype(_40) &, decltype(_41) &,
    decltype(_42) &, decltype(_43) &, decltype(_44) &, decltype(_45) &, decltype(_46) &,
    decltype(_47) &, decltype(_48) &, decltype(_49) &, decltype(_4a) &, decltype(_4b) &,
    decltype(_4c) &, decltype(_4d) &, decltype(_4e) &, decltype(_4f) &, decltype(_50) &,
    decltype(_51) &, decltype(_52) &, decltype(_53) &, decltype(_54) &, decltype(_55) &,
    decltype(_56) &, decltype(_57) &, decltype(_58) &, decltype(_59) &, decltype(_5a) &,
    decltype(_5b) &, decltype(_5c) &, decltype(_5d) &
>(
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _3a, _3b, _3c,
    _3d, _3e, _3f, _40, _41, _42, _43, _44, _45, _46, _47, _48, _49, _4a, _4b,
    _4c, _4d, _4e, _4f, _50, _51, _52, _53, _54, _55, _56, _57, _58, _59, _5a,
    _5b, _5c, _5d
);
}
};


template<> struct tie_aggregate_impl<94> {
template<typename T> static constexpr auto call(T & x) {
auto & [
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _3a, _3b, _3c,
    _3d, _3e, _3f, _40, _41, _42, _43, _44, _45, _46, _47, _48, _49, _4a, _4b,
    _4c, _4d, _4e, _4f, _50, _51, _52, _53, _54, _55, _56, _57, _58, _59, _5a,
    _5b, _5c, _5d, _5e
] = x;
return parser::tuple<
    decltype(_01) &, decltype(_02) &, decltype(_03) &, decltype(_04) &, decltype(_05) &,
    decltype(_06) &, decltype(_07) &, decltype(_08) &, decltype(_09) &, decltype(_0a) &,
    decltype(_0b) &, decltype(_0c) &, decltype(_0d) &, decltype(_0e) &, decltype(_0f) &,
    decltype(_10) &, decltype(_11) &, decltype(_12) &, decltype(_13) &, decltype(_14) &,
    decltype(_15) &, decltype(_16) &, decltype(_17) &, decltype(_18) &, decltype(_19) &,
    decltype(_1a) &, decltype(_1b) &, decltype(_1c) &, decltype(_1d) &, decltype(_1e) &,
    decltype(_1f) &, decltype(_20) &, decltype(_21) &, decltype(_22) &, decltype(_23) &,
    decltype(_24) &, decltype(_25) &, decltype(_26) &, decltype(_27) &, decltype(_28) &,
    decltype(_29) &, decltype(_2a) &, decltype(_2b) &, decltype(_2c) &, decltype(_2d) &,
    decltype(_2e) &, decltype(_2f) &, decltype(_30) &, decltype(_31) &, decltype(_32) &,
    decltype(_33) &, decltype(_34) &, decltype(_35) &, decltype(_36) &, decltype(_37) &,
    decltype(_38) &, decltype(_39) &, decltype(_3a) &, decltype(_3b) &, decltype(_3c) &,
    decltype(_3d) &, decltype(_3e) &, decltype(_3f) &, decltype(_40) &, decltype(_41) &,
    decltype(_42) &, decltype(_43) &, decltype(_44) &, decltype(_45) &, decltype(_46) &,
    decltype(_47) &, decltype(_48) &, decltype(_49) &, decltype(_4a) &, decltype(_4b) &,
    decltype(_4c) &, decltype(_4d) &, decltype(_4e) &, decltype(_4f) &, decltype(_50) &,
    decltype(_51) &, decltype(_52) &, decltype(_53) &, decltype(_54) &, decltype(_55) &,
    decltype(_56) &, decltype(_57) &, decltype(_58) &, decltype(_59) &, decltype(_5a) &,
    decltype(_5b) &, decltype(_5c) &, decltype(_5d) &, decltype(_5e) &
>(
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _3a, _3b, _3c,
    _3d, _3e, _3f, _40, _41, _42, _43, _44, _45, _46, _47, _48, _49, _4a, _4b,
    _4c, _4d, _4e, _4f, _50, _51, _52, _53, _54, _55, _56, _57, _58, _59, _5a,
    _5b, _5c, _5d, _5e
);
}
};


template<> struct tie_aggregate_impl<95> {
template<typename T> static constexpr auto call(T & x) {
auto & [
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _3a, _3b, _3c,
    _3d, _3e, _3f, _40, _41, _42, _43, _44, _45, _46, _47, _48, _49, _4a, _4b,
    _4c, _4d, _4e, _4f, _50, _51, _52, _53, _54, _55, _56, _57, _58, _59, _5a,
    _5b, _5c, _5d, _5e, _5f
] = x;
return parser::tuple<
    decltype(_01) &, decltype(_02) &, decltype(_03) &, decltype(_04) &, decltype(_05) &,
    decltype(_06) &, decltype(_07) &, decltype(_08) &, decltype(_09) &, decltype(_0a) &,
    decltype(_0b) &, decltype(_0c) &, decltype(_0d) &, decltype(_0e) &, decltype(_0f) &,
    decltype(_10) &, decltype(_11) &, decltype(_12) &, decltype(_13) &, decltype(_14) &,
    decltype(_15) &, decltype(_16) &, decltype(_17) &, decltype(_18) &, decltype(_19) &,
    decltype(_1a) &, decltype(_1b) &, decltype(_1c) &, decltype(_1d) &, decltype(_1e) &,
    decltype(_1f) &, decltype(_20) &, decltype(_21) &, decltype(_22) &, decltype(_23) &,
    decltype(_24) &, decltype(_25) &, decltype(_26) &, decltype(_27) &, decltype(_28) &,
    decltype(_29) &, decltype(_2a) &, decltype(_2b) &, decltype(_2c) &, decltype(_2d) &,
    decltype(_2e) &, decltype(_2f) &, decltype(_30) &, decltype(_31) &, decltype(_32) &,
    decltype(_33) &, decltype(_34) &, decltype(_35) &, decltype(_36) &, decltype(_37) &,
    decltype(_38) &, decltype(_39) &, decltype(_3a) &, decltype(_3b) &, decltype(_3c) &,
    decltype(_3d) &, decltype(_3e) &, decltype(_3f) &, decltype(_40) &, decltype(_41) &,
    decltype(_42) &, decltype(_43) &, decltype(_44) &, decltype(_45) &, decltype(_46) &,
    decltype(_47) &, decltype(_48) &, decltype(_49) &, decltype(_4a) &, decltype(_4b) &,
    decltype(_4c) &, decltype(_4d) &, decltype(_4e) &, decltype(_4f) &, decltype(_50) &,
    decltype(_51) &, decltype(_52) &, decltype(_53) &, decltype(_54) &, decltype(_55) &,
    decltype(_56) &, decltype(_57) &, decltype(_58) &, decltype(_59) &, decltype(_5a) &,
    decltype(_5b) &, decltype(_5c) &, decltype(_5d) &, decltype(_5e) &, decltype(_5f) &
>(
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _3a, _3b, _3c,
    _3d, _3e, _3f, _40, _41, _42, _43, _44, _45, _46, _47, _48, _49, _4a, _4b,
    _4c, _4d, _4e, _4f, _50, _51, _52, _53, _54, _55, _56, _57, _58, _59, _5a,
    _5b, _5c, _5d, _5e, _5f
);
}
};


template<> struct tie_aggregate_impl<96> {
template<typename T> static constexpr auto call(T & x) {
auto & [
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _3a, _3b, _3c,
    _3d, _3e, _3f, _40, _41, _42, _43, _44, _45, _46, _47, _48, _49, _4a, _4b,
    _4c, _4d, _4e, _4f, _50, _51, _52, _53, _54, _55, _56, _57, _58, _59, _5a,
    _5b, _5c, _5d, _5e, _5f, _60
] = x;
return parser::tuple<
    decltype(_01) &, decltype(_02) &, decltype(_03) &, decltype(_04) &, decltype(_05) &,
    decltype(_06) &, decltype(_07) &, decltype(_08) &, decltype(_09) &, decltype(_0a) &,
    decltype(_0b) &, decltype(_0c) &, decltype(_0d) &, decltype(_0e) &, decltype(_0f) &,
    decltype(_10) &, decltype(_11) &, decltype(_12) &, decltype(_13) &, decltype(_14) &,
    decltype(_15) &, decltype(_16) &, decltype(_17) &, decltype(_18) &, decltype(_19) &,
    decltype(_1a) &, decltype(_1b) &, decltype(_1c) &, decltype(_1d) &, decltype(_1e) &,
    decltype(_1f) &, decltype(_20) &, decltype(_21) &, decltype(_22) &, decltype(_23) &,
    decltype(_24) &, decltype(_25) &, decltype(_26) &, decltype(_27) &, decltype(_28) &,
    decltype(_29) &, decltype(_2a) &, decltype(_2b) &, decltype(_2c) &, decltype(_2d) &,
    decltype(_2e) &, decltype(_2f) &, decltype(_30) &, decltype(_31) &, decltype(_32) &,
    decltype(_33) &, decltype(_34) &, decltype(_35) &, decltype(_36) &, decltype(_37) &,
    decltype(_38) &, decltype(_39) &, decltype(_3a) &, decltype(_3b) &, decltype(_3c) &,
    decltype(_3d) &, decltype(_3e) &, decltype(_3f) &, decltype(_40) &, decltype(_41) &,
    decltype(_42) &, decltype(_43) &, decltype(_44) &, decltype(_45) &, decltype(_46) &,
    decltype(_47) &, decltype(_48) &, decltype(_49) &, decltype(_4a) &, decltype(_4b) &,
    decltype(_4c) &, decltype(_4d) &, decltype(_4e) &, decltype(_4f) &, decltype(_50) &,
    decltype(_51) &, decltype(_52) &, decltype(_53) &, decltype(_54) &, decltype(_55) &,
    decltype(_56) &, decltype(_57) &, decltype(_58) &, decltype(_59) &, decltype(_5a) &,
    decltype(_5b) &, decltype(_5c) &, decltype(_5d) &, decltype(_5e) &, decltype(_5f) &,
    decltype(_60) &
>(
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _3a, _3b, _3c,
    _3d, _3e, _3f, _40, _41, _42, _43, _44, _45, _46, _47, _48, _49, _4a, _4b,
    _4c, _4d, _4e, _4f, _50, _51, _52, _53, _54, _55, _56, _57, _58, _59, _5a,
    _5b, _5c, _5d, _5e, _5f, _60
);
}
};


template<> struct tie_aggregate_impl<97> {
template<typename T> static constexpr auto call(T & x) {
auto & [
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _3a, _3b, _3c,
    _3d, _3e, _3f, _40, _41, _42, _43, _44, _45, _46, _47, _48, _49, _4a, _4b,
    _4c, _4d, _4e, _4f, _50, _51, _52, _53, _54, _55, _56, _57, _58, _59, _5a,
    _5b, _5c, _5d, _5e, _5f, _60, _61
] = x;
return parser::tuple<
    decltype(_01) &, decltype(_02) &, decltype(_03) &, decltype(_04) &, decltype(_05) &,
    decltype(_06) &, decltype(_07) &, decltype(_08) &, decltype(_09) &, decltype(_0a) &,
    decltype(_0b) &, decltype(_0c) &, decltype(_0d) &, decltype(_0e) &, decltype(_0f) &,
    decltype(_10) &, decltype(_11) &, decltype(_12) &, decltype(_13) &, decltype(_14) &,
    decltype(_15) &, decltype(_16) &, decltype(_17) &, decltype(_18) &, decltype(_19) &,
    decltype(_1a) &, decltype(_1b) &, decltype(_1c) &, decltype(_1d) &, decltype(_1e) &,
    decltype(_1f) &, decltype(_20) &, decltype(_21) &, decltype(_22) &, decltype(_23) &,
    decltype(_24) &, decltype(_25) &, decltype(_26) &, decltype(_27) &, decltype(_28) &,
    decltype(_29) &, decltype(_2a) &, decltype(_2b) &, decltype(_2c) &, decltype(_2d) &,
    decltype(_2e) &, decltype(_2f) &, decltype(_30) &, decltype(_31) &, decltype(_32) &,
    decltype(_33) &, decltype(_34) &, decltype(_35) &, decltype(_36) &, decltype(_37) &,
    decltype(_38) &, decltype(_39) &, decltype(_3a) &, decltype(_3b) &, decltype(_3c) &,
    decltype(_3d) &, decltype(_3e) &, decltype(_3f) &, decltype(_40) &, decltype(_41) &,
    decltype(_42) &, decltype(_43) &, decltype(_44) &, decltype(_45) &, decltype(_46) &,
    decltype(_47) &, decltype(_48) &, decltype(_49) &, decltype(_4a) &, decltype(_4b) &,
    decltype(_4c) &, decltype(_4d) &, decltype(_4e) &, decltype(_4f) &, decltype(_50) &,
    decltype(_51) &, decltype(_52) &, decltype(_53) &, decltype(_54) &, decltype(_55) &,
    decltype(_56) &, decltype(_57) &, decltype(_58) &, decltype(_59) &, decltype(_5a) &,
    decltype(_5b) &, decltype(_5c) &, decltype(_5d) &, decltype(_5e) &, decltype(_5f) &,
    decltype(_60) &, decltype(_61) &
>(
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _3a, _3b, _3c,
    _3d, _3e, _3f, _40, _41, _42, _43, _44, _45, _46, _47, _48, _49, _4a, _4b,
    _4c, _4d, _4e, _4f, _50, _51, _52, _53, _54, _55, _56, _57, _58, _59, _5a,
    _5b, _5c, _5d, _5e, _5f, _60, _61
);
}
};


template<> struct tie_aggregate_impl<98> {
template<typename T> static constexpr auto call(T & x) {
auto & [
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _3a, _3b, _3c,
    _3d, _3e, _3f, _40, _41, _42, _43, _44, _45, _46, _47, _48, _49, _4a, _4b,
    _4c, _4d, _4e, _4f, _50, _51, _52, _53, _54, _55, _56, _57, _58, _59, _5a,
    _5b, _5c, _5d, _5e, _5f, _60, _61, _62
] = x;
return parser::tuple<
    decltype(_01) &, decltype(_02) &, decltype(_03) &, decltype(_04) &, decltype(_05) &,
    decltype(_06) &, decltype(_07) &, decltype(_08) &, decltype(_09) &, decltype(_0a) &,
    decltype(_0b) &, decltype(_0c) &, decltype(_0d) &, decltype(_0e) &, decltype(_0f) &,
    decltype(_10) &, decltype(_11) &, decltype(_12) &, decltype(_13) &, decltype(_14) &,
    decltype(_15) &, decltype(_16) &, decltype(_17) &, decltype(_18) &, decltype(_19) &,
    decltype(_1a) &, decltype(_1b) &, decltype(_1c) &, decltype(_1d) &, decltype(_1e) &,
    decltype(_1f) &, decltype(_20) &, decltype(_21) &, decltype(_22) &, decltype(_23) &,
    decltype(_24) &, decltype(_25) &, decltype(_26) &, decltype(_27) &, decltype(_28) &,
    decltype(_29) &, decltype(_2a) &, decltype(_2b) &, decltype(_2c) &, decltype(_2d) &,
    decltype(_2e) &, decltype(_2f) &, decltype(_30) &, decltype(_31) &, decltype(_32) &,
    decltype(_33) &, decltype(_34) &, decltype(_35) &, decltype(_36) &, decltype(_37) &,
    decltype(_38) &, decltype(_39) &, decltype(_3a) &, decltype(_3b) &, decltype(_3c) &,
    decltype(_3d) &, decltype(_3e) &, decltype(_3f) &, decltype(_40) &, decltype(_41) &,
    decltype(_42) &, decltype(_43) &, decltype(_44) &, decltype(_45) &, decltype(_46) &,
    decltype(_47) &, decltype(_48) &, decltype(_49) &, decltype(_4a) &, decltype(_4b) &,
    decltype(_4c) &, decltype(_4d) &, decltype(_4e) &, decltype(_4f) &, decltype(_50) &,
    decltype(_51) &, decltype(_52) &, decltype(_53) &, decltype(_54) &, decltype(_55) &,
    decltype(_56) &, decltype(_57) &, decltype(_58) &, decltype(_59) &, decltype(_5a) &,
    decltype(_5b) &, decltype(_5c) &, decltype(_5d) &, decltype(_5e) &, decltype(_5f) &,
    decltype(_60) &, decltype(_61) &, decltype(_62) &
>(
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _3a, _3b, _3c,
    _3d, _3e, _3f, _40, _41, _42, _43, _44, _45, _46, _47, _48, _49, _4a, _4b,
    _4c, _4d, _4e, _4f, _50, _51, _52, _53, _54, _55, _56, _57, _58, _59, _5a,
    _5b, _5c, _5d, _5e, _5f, _60, _61, _62
);
}
};


template<> struct tie_aggregate_impl<99> {
template<typename T> static constexpr auto call(T & x) {
auto & [
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _3a, _3b, _3c,
    _3d, _3e, _3f, _40, _41, _42, _43, _44, _45, _46, _47, _48, _49, _4a, _4b,
    _4c, _4d, _4e, _4f, _50, _51, _52, _53, _54, _55, _56, _57, _58, _59, _5a,
    _5b, _5c, _5d, _5e, _5f, _60, _61, _62, _63
] = x;
return parser::tuple<
    decltype(_01) &, decltype(_02) &, decltype(_03) &, decltype(_04) &, decltype(_05) &,
    decltype(_06) &, decltype(_07) &, decltype(_08) &, decltype(_09) &, decltype(_0a) &,
    decltype(_0b) &, decltype(_0c) &, decltype(_0d) &, decltype(_0e) &, decltype(_0f) &,
    decltype(_10) &, decltype(_11) &, decltype(_12) &, decltype(_13) &, decltype(_14) &,
    decltype(_15) &, decltype(_16) &, decltype(_17) &, decltype(_18) &, decltype(_19) &,
    decltype(_1a) &, decltype(_1b) &, decltype(_1c) &, decltype(_1d) &, decltype(_1e) &,
    decltype(_1f) &, decltype(_20) &, decltype(_21) &, decltype(_22) &, decltype(_23) &,
    decltype(_24) &, decltype(_25) &, decltype(_26) &, decltype(_27) &, decltype(_28) &,
    decltype(_29) &, decltype(_2a) &, decltype(_2b) &, decltype(_2c) &, decltype(_2d) &,
    decltype(_2e) &, decltype(_2f) &, decltype(_30) &, decltype(_31) &, decltype(_32) &,
    decltype(_33) &, decltype(_34) &, decltype(_35) &, decltype(_36) &, decltype(_37) &,
    decltype(_38) &, decltype(_39) &, decltype(_3a) &, decltype(_3b) &, decltype(_3c) &,
    decltype(_3d) &, decltype(_3e) &, decltype(_3f) &, decltype(_40) &, decltype(_41) &,
    decltype(_42) &, decltype(_43) &, decltype(_44) &, decltype(_45) &, decltype(_46) &,
    decltype(_47) &, decltype(_48) &, decltype(_49) &, decltype(_4a) &, decltype(_4b) &,
    decltype(_4c) &, decltype(_4d) &, decltype(_4e) &, decltype(_4f) &, decltype(_50) &,
    decltype(_51) &, decltype(_52) &, decltype(_53) &, decltype(_54) &, decltype(_55) &,
    decltype(_56) &, decltype(_57) &, decltype(_58) &, decltype(_59) &, decltype(_5a) &,
    decltype(_5b) &, decltype(_5c) &, decltype(_5d) &, decltype(_5e) &, decltype(_5f) &,
    decltype(_60) &, decltype(_61) &, decltype(_62) &, decltype(_63) &
>(
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _3a, _3b, _3c,
    _3d, _3e, _3f, _40, _41, _42, _43, _44, _45, _46, _47, _48, _49, _4a, _4b,
    _4c, _4d, _4e, _4f, _50, _51, _52, _53, _54, _55, _56, _57, _58, _59, _5a,
    _5b, _5c, _5d, _5e, _5f, _60, _61, _62, _63
);
}
};


template<> struct tie_aggregate_impl<100> {
template<typename T> static constexpr auto call(T & x) {
auto & [
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _3a, _3b, _3c,
    _3d, _3e, _3f, _40, _41, _42, _43, _44, _45, _46, _47, _48, _49, _4a, _4b,
    _4c, _4d, _4e, _4f, _50, _51, _52, _53, _54, _55, _56, _57, _58, _59, _5a,
    _5b, _5c, _5d, _5e, _5f, _60, _61, _62, _63, _64
] = x;
return parser::tuple<
    decltype(_01) &, decltype(_02) &, decltype(_03) &, decltype(_04) &, decltype(_05) &,
    decltype(_06) &, decltype(_07) &, decltype(_08) &, decltype(_09) &, decltype(_0a) &,
    decltype(_0b) &, decltype(_0c) &, decltype(_0d) &, decltype(_0e) &, decltype(_0f) &,
    decltype(_10) &, decltype(_11) &, decltype(_12) &, decltype(_13) &, decltype(_14) &,
    decltype(_15) &, decltype(_16) &, decltype(_17) &, decltype(_18) &, decltype(_19) &,
    decltype(_1a) &, decltype(_1b) &, decltype(_1c) &, decltype(_1d) &, decltype(_1e) &,
    decltype(_1f) &, decltype(_20) &, decltype(_21) &, decltype(_22) &, decltype(_23) &,
    decltype(_24) &, decltype(_25) &, decltype(_26) &, decltype(_27) &, decltype(_28) &,
    decltype(_29) &, decltype(_2a) &, decltype(_2b) &, decltype(_2c) &, decltype(_2d) &,
    decltype(_2e) &, decltype(_2f) &, decltype(_30) &, decltype(_31) &, decltype(_32) &,
    decltype(_33) &, decltype(_34) &, decltype(_35) &, decltype(_36) &, decltype(_37) &,
    decltype(_38) &, decltype(_39) &, decltype(_3a) &, decltype(_3b) &, decltype(_3c) &,
    decltype(_3d) &, decltype(_3e) &, decltype(_3f) &, decltype(_40) &, decltype(_41) &,
    decltype(_42) &, decltype(_43) &, decltype(_44) &, decltype(_45) &, decltype(_46) &,
    decltype(_47) &, decltype(_48) &, decltype(_49) &, decltype(_4a) &, decltype(_4b) &,
    decltype(_4c) &, decltype(_4d) &, decltype(_4e) &, decltype(_4f) &, decltype(_50) &,
    decltype(_51) &, decltype(_52) &, decltype(_53) &, decltype(_54) &, decltype(_55) &,
    decltype(_56) &, decltype(_57) &, decltype(_58) &, decltype(_59) &, decltype(_5a) &,
    decltype(_5b) &, decltype(_5c) &, decltype(_5d) &, decltype(_5e) &, decltype(_5f) &,
    decltype(_60) &, decltype(_61) &, decltype(_62) &, decltype(_63) &, decltype(_64) &
>(
    _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e,
    _1f, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d,
    _2e, _2f, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _3a, _3b, _3c,
    _3d, _3e, _3f, _40, _41, _42, _43, _44, _45, _46, _47, _48, _49, _4a, _4b,
    _4c, _4d, _4e, _4f, _50, _51, _52, _53, _54, _55, _56, _57, _58, _59, _5a,
    _5b, _5c, _5d, _5e, _5f, _60, _61, _62, _63, _64
);
}
};


}

