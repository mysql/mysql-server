//
// Copyright (c) 2022 Dmitry Arkhipov (grisumbras@yandex.ru)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/json
//

#ifndef BOOST_JSON_CONVERSION_HPP
#define BOOST_JSON_CONVERSION_HPP

#include <boost/json/detail/config.hpp>
#include <boost/json/fwd.hpp>

#include <type_traits>

namespace boost {
namespace json {

namespace detail {

template< class Ctx, class T, class Dir >
struct supported_context;

} // namespace detail

/** Customization point tag.

    This tag type is used by the function @ref value_from to select overloads
    of `tag_invoke`.

    @note This type is empty; it has no members.

    @see
        @ref value_from, @ref value_to, @ref value_to_tag,
        [tag_invoke: A general pattern for supporting customisable functions](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2019/p1895r0.pdf).
*/
struct value_from_tag { };

/** Customization point tag type.

    This tag type is used by the function @ref value_to to select overloads of
    `tag_invoke`.

    @note This type is empty; it has no members.

    @see
        @ref value_from, @ref value_from_tag, @ref value_to,
        [tag_invoke: A general pattern for supporting customisable functions](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2019/p1895r0.pdf).
*/
template<class T>
struct value_to_tag { };

/** Customization point tag type.

    This tag type is used by the function @ref try_value_to to select overloads
    of `tag_invoke`.

    @note This type is empty; it has no members.

    @see
        @ref value_to, @ref value_to_tag,
        [tag_invoke: A general pattern for supporting customisable functions](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2019/p1895r0.pdf).
*/
template<class T>
struct try_value_to_tag { };

/** Determine if `T` can be treated like a string during conversions.

    Provides the member constant `value` that is equal to `true`, if `T` is
    convertible to @ref string_view. Otherwise, `value` is equal to `false`.

    Users can specialize the trait for their own types if they don't want them
    to be treated like strings. For example:

    @code
    namespace boost {
    namespace json {

    template <>
    struct is_string_like<your::string> : std::false_type
    { };

    } // namespace boost
    } // namespace json
    @endcode

    @par Types satisfying the trait
    @ref string, @ref string_view, @ref std::string, @ref std::string_view.

    @see @ref value_from, @ref value_to.
*/
template<class T>
struct is_string_like;

/** Determine if `T` can be treated like `std::filesystem::path` during conversions.

    Given `t`, a glvalue of type `T`, if

    - given `It`, the type denoted by `decltype(std::begin(t))`,
      `std::iterator_traits<It>::iterator_category` is well-formed and denotes
      a type; and

    - `std::iterator_traits<It>::value_type` is `T`; and

    - `T::value_type` is well-formed and denotes a type; and

    - `T::string_type` is well-formed, denotes a type, and is an alias for
    `std::basic_string< T::value_type >`;

    then the trait provides the member constant `value` that is equal to
    `true`. Otherwise, `value` is equal to `false`.

    Users can specialize the trait for their own types if they don't want them
    to be treated like filesystem paths. For example:

    @code
    namespace boost {
    namespace json {

    template <>
    struct is_path_like<your::path> : std::false_type
    { };

    } // namespace boost
    } // namespace json
    @endcode

    @par Types Satisfying the Trait
    @ref std::filesystem::path, @ref boost::filesystem::path.

    @see @ref value_from, @ref value_to.
*/
template<class T>
struct is_path_like;

/** Determine if `T` can be treated like a sequence during conversions.

    Given `t`, a glvalue of type `T`, if

    - given `It`, the type denoted by `decltype(std::begin(t))`,
      `std::iterator_traits<It>::iterator_category` is well-formed and denotes
      a type; and

    - `decltype(std::end(t))` also denotes the type `It`; and

    - `std::iterator_traits<It>::value_type` is not `T`; and

    then the trait provides the member constant `value` that is equal to
    `true`. Otherwise, `value` is equal to `false`.

    Users can specialize the trait for their own types if they don't want them
    to be treated like sequences. For example:

    @code
    namespace boost {
    namespace json {

    template <>
    struct is_sequence_like<your::container> : std::false_type
    { };

    } // namespace boost
    } // namespace json
    @endcode


    @par Types satisfying the trait
    Any {req_SequenceContainer}, array types.

    @see @ref value_from, @ref value_to
*/
template<class T>
struct is_sequence_like;

/** Determine if `T` can be treated like a 1-to-1 mapping during
    conversions.

    Given `t`, a glvalue of type `T`, if

    - `is_sequence_like<T>::value` is `true`; and

    - given type `It` denoting `decltype(std::begin(t))`, and types `K` and
      `M`,  `std::iterator_traits<It>::value_type` denotes `std::pair<K, M>`;
      and

    - `std::is_string_like<K>::value` is `true`; and

    - given `v`, a glvalue of type `V`, and `E`, the type denoted by
      `decltype(t.emplace(v))`, `std::is_tuple_like<E>::value` is `true`;

    then the trait provides the member constant `value` that is equal to
    `true`. Otherwise, `value` is equal to `false`.

    Users can specialize the trait for their own types if they don't want them
    to be treated like mappings. For example:

    @code
    namespace boost {
    namespace json {

    template <>
    struct is_map_like<your::map> : std::false_type
    { };

    } // namespace boost
    } // namespace json
    @endcode

    @note The restriction for `t.emplace()` return type ensures that the
    container does not accept duplicate keys.

    @par Types Satisfying The Trait
    @ref std::map, @ref std::unordered_map.

    @see @ref value_from, @ref value_to.
*/
template<class T>
struct is_map_like;

/** Determine if `T` can be treated like a tuple during conversions.

    Provides the member constant `value` that is equal to `true`, if
    `std::tuple_size<T>::value` is a positive number. Otherwise, `value`
    is equal to `false`.

    Users can specialize the trait for their own types if they don't want them
    to be treated like tuples. For example:

    @code
    namespace boost {
    namespace json {

    template <>
    struct is_tuple_like<your::tuple> : std::false_type
    { };

    } // namespace boost
    } // namespace json
    @endcode


    @par Types satisfying the trait
    @ref std::tuple, @ref std::pair.

    @see @ref value_from, @ref value_to.
*/
template<class T>
struct is_tuple_like;

/** Determine if `T` can be treated like null during conversions.

    Primary template instantiations provide the member constant `value` that is
    equal to `false`. Users can specialize the trait for their own types if
    they **do** want them to be treated as nulls. For example:

    @code
    namespace boost {
    namespace json {

    template <>
    struct is_null_like<your::null_type> : std::true_type
    { };

    } // namespace boost
    } // namespace json
    @endcode

    @par Types Satisfying the Trait
    @ref std::nullptr_t.

    @see @ref value_from, @ref value_to
*/
template<class T>
struct is_null_like
    : std::false_type
{ };

/** Determine if `T` should be treated as a described class.

    Described classes are serialised as objects with an element for each
    described data member. Described bases are serialized in a flattened way,
    that is members of bases are serialized as direct elements of the object,
    and no nested objects are created for bases.

    A described class should not have non-public described members (including
    inherited members) or non-public non-empty described bases. Or more
    formally, given `L`, a class template of the form
    `template<class...> struct L {}`, if

    - `boost::describe::has_describe_members<T>::value` is `true`; and

    - `boost::describe::describe_members<T, boost::describe::mod_private |
      boost::describe::mod_protected | boost::describe::mod_inherited>` denotes
      `L<>`; and

    - `std::is_union<T>::value` is `false`;

    then the trait provides the member constant `value` that is equal to
    `true`. Otherwise, `value` is equal to `false`.

    @note Shadowed members are ignored both for requirements checking and for
    performing conversions.

    Users can specialize the trait for their own types if they don't want them
    to be treated as described classes. For example:

    @code
    namespace boost {
    namespace json {

    template <>
    struct is_described_class<your::described_class> : std::false_type
    { };

    } // namespace boost
    } // namespace json
    @endcode

    Users can also specialize the trait for their own types _with_ described
    non-public data members to enable this conversion implementation. Note that
    non-public bases are not supported regardless.

    @see [Boost.Describe](https://www.boost.org/doc/libs/latest/libs/describe/doc/html/describe.html).
*/
template<class T>
struct is_described_class;

/** Determine if `T` should be treated as a described enum.

    Described enums are serialised as strings when their value equals to a
    described enumerator, and as integers otherwise. The reverse operation
    does not convert numbers to enums values, though, and instead produces
    an error.

    If `boost::describe::has_describe_enumerators<T>::value` is `true`, then
    the trait provides the member constant `value` that is equal to `true`.
    Otherwise, `value` is equal to `false`.

    Users can specialize the trait for their own enums if they don't want them
    to be treated as described enums. For example:

    @code
    namespace boost {
    namespace json {

    template <>
    struct is_described_enum<your::described_enum> : std::false_type
    { };

    } // namespace boost
    } // namespace json
    @endcode

    @see [Boost.Describe](https://www.boost.org/doc/libs/latest/libs/describe/doc/html/describe.html).
*/
template<class T>
struct is_described_enum;

/** Determine if `T` should be treated as a variant.

    Variants are serialised the same way their active alternative is
    serialised. The opposite conversion selects the first alternative for which
    conversion succeeds.

    Given `t`, a glvalue of type ` const T`, if `t.valueless_by_exception()` is
    well-formed, then the trait provides the member constant `value` that is
    equal to `true`. Otherwise, `value` is equal to `false`.

    Users can specialize the trait for their own types if they don't want them
    to be treated as variants. For example:

    @code
    namespace boost {
    namespace json {

    template <>
    struct is_variant_like<your::variant> : std::false_type
    { };

    } // namespace boost
    } // namespace json
    @endcode

    @par Types Satisfying the Trait
    @ref std::variant, @ref boost::variant2::variant.
*/
template<class T>
struct is_variant_like;

/** Determine if `T` should be treated as an optional

    Optionals are serialised as `null` if empty, or as the stored type
    otherwise.

    Given `t`, a glvalue of type `T`, if

    - `decltype( t.value() )` is well-formed and isn't a void type; and
    - `t.reset()` is well-formed;

    then the trait provides the member constant `value` that is equal to
    `true`. Otherwise, `value` is equal to `false`.

    Users can specialize the trait for their own types if they don't want them
    to be treated as optionals. For example:

    @code
    namespace boost {
    namespace json {

    template <>
    struct is_optional_like<your::optional> : std::false_type
    { };

    } // namespace boost
    } // namespace json
    @endcode

    @par Types Satisfying the Trait
    @ref std::optional, @ref boost::optional.
*/
template<class T>
struct is_optional_like;

} // namespace json
} // namespace boost

#include <boost/json/impl/conversion.hpp>

#endif // BOOST_JSON_CONVERSION_HPP
