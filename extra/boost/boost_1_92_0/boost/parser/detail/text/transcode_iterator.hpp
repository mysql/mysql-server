// Copyright (C) 2020 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_PARSER_DETAIL_TEXT_TRANSCODE_ITERATOR_HPP
#define BOOST_PARSER_DETAIL_TEXT_TRANSCODE_ITERATOR_HPP

#include <boost/parser/detail/debug_assert.hpp>
#include <boost/parser/detail/text/transcode_iterator_fwd.hpp>
#include <boost/parser/detail/text/concepts.hpp>
#include <boost/parser/detail/text/utf.hpp>
#include <boost/parser/detail/text/detail/algorithm.hpp>

#include <boost/parser/detail/stl_interfaces/iterator_interface.hpp>

#include <array>
#include <iterator>
#include <type_traits>
#include <stdexcept>
#include <string_view>

namespace boost::parser::detail { namespace text {

    namespace {
        constexpr char16_t high_surrogate_base = 0xd7c0;
        constexpr char16_t low_surrogate_base = 0xdc00;
        constexpr char32_t high_surrogate_min = 0xd800;
        constexpr char32_t high_surrogate_max = 0xdbff;
        constexpr char32_t low_surrogate_min = 0xdc00;
        constexpr char32_t low_surrogate_max = 0xdfff;
        constexpr char32_t replacement_character = 0xfffd;
    }

    namespace detail {
        constexpr bool in(unsigned char lo, unsigned char c, unsigned char hi)
        {
            return lo <= c && c <= hi;
        }

        struct throw_on_encoding_error
        {};

        template<typename OutIter>
        inline constexpr OutIter read_into_buf(char32_t cp, OutIter buf)
        {
            if (cp < 0x80) {
                *buf = static_cast<char>(cp);
                ++buf;
            } else if (cp < 0x800) {
                *buf = static_cast<char>(0xC0 + (cp >> 6));
                ++buf;
                *buf = static_cast<char>(0x80 + (cp & 0x3f));
                ++buf;
            } else if (cp < 0x10000) {
                *buf = static_cast<char>(0xe0 + (cp >> 12));
                ++buf;
                *buf = static_cast<char>(0x80 + ((cp >> 6) & 0x3f));
                ++buf;
                *buf = static_cast<char>(0x80 + (cp & 0x3f));
                ++buf;
            } else {
                *buf = static_cast<char>(0xf0 + (cp >> 18));
                ++buf;
                *buf = static_cast<char>(0x80 + ((cp >> 12) & 0x3f));
                ++buf;
                *buf = static_cast<char>(0x80 + ((cp >> 6) & 0x3f));
                ++buf;
                *buf = static_cast<char>(0x80 + (cp & 0x3f));
                ++buf;
            }
            return buf;
        }

        template<typename OutIter>
        constexpr OutIter write_cp_utf8(char32_t cp, OutIter out)
        {
            return detail::read_into_buf(cp, out);
        }

        template<typename OutIter>
        constexpr OutIter write_cp_utf16(char32_t cp, OutIter out)
        {
            if (cp < 0x10000) {
                *out = static_cast<char16_t>(cp);
                ++out;
            } else {
                *out = static_cast<char16_t>(cp >> 10) + high_surrogate_base;
                ++out;
                *out = static_cast<char16_t>(cp & 0x3ff) + low_surrogate_base;
                ++out;
            }
            return out;
        }

        inline constexpr char32_t surrogates_to_cp(char16_t hi, char16_t lo)
        {
            return char32_t((hi - high_surrogate_base) << 10) +
                   (lo - low_surrogate_base);
        }

        template<typename T, typename U>
        using enable_utf8_cp = std::enable_if<is_char_iter_v<T>, U>;
        template<typename T, typename U = T>
        using enable_utf8_cp_t = typename enable_utf8_cp<T, U>::type;

        template<typename T, typename U>
        using enable_utf16_cp = std::enable_if<is_16_iter_v<T>, U>;
        template<typename T, typename U = T>
        using enable_utf16_cp_t = typename enable_utf16_cp<T, U>::type;

        template<typename I>
        auto bidirectional_at_most()
        {
#if BOOST_PARSER_DETAIL_TEXT_USE_CONCEPTS
            if constexpr (std::bidirectional_iterator<I>) {
                return std::bidirectional_iterator_tag{};
            } else if constexpr (std::forward_iterator<I>) {
                return std::forward_iterator_tag{};
            } else if constexpr (std::input_iterator<I>) {
                return std::input_iterator_tag{};
            }
#else
            using category =
                typename std::iterator_traits<I>::iterator_category;
            if constexpr (std::is_base_of_v<
                              std::bidirectional_iterator_tag,
                              category>) {
                return std::bidirectional_iterator_tag{};
            } else {
                return category{};
            }
#endif
        }

        template<typename I>
        using bidirectional_at_most_t = decltype(bidirectional_at_most<I>());
    }

    /** Returns true iff `c` is a Unicode surrogate. */
    inline constexpr bool surrogate(char32_t c)
    {
        return high_surrogate_min <= c && c <= low_surrogate_max;
    }

    /** Returns true iff `c` is a Unicode high surrogate. */
    inline constexpr bool high_surrogate(char32_t c)
    {
        return high_surrogate_min <= c && c <= high_surrogate_max;
    }

    /** Returns true iff `c` is a Unicode low surrogate. */
    inline constexpr bool low_surrogate(char32_t c)
    {
        return low_surrogate_min <= c && c <= low_surrogate_max;
    }

    /** Returns true iff `c` is a Unicode reserved noncharacter.

        \see Unicode 3.4/D14 */
    inline constexpr bool reserved_noncharacter(char32_t c)
    {
        bool const byte01_reserved = (c & 0xffff) >= 0xfffe;
        bool const byte2_at_most_0x10 = ((c & 0xff0000u) >> 16) <= 0x10;
        return (byte01_reserved && byte2_at_most_0x10) ||
               (0xfdd0 <= c && c <= 0xfdef);
    }

    /** Returns true iff `c` is a valid Unicode scalar value.

        \see Unicode 3.9/D90 */
    inline constexpr bool scalar_value(char32_t c)
    {
        return c <= 0x10ffff && !surrogate(c);
    }

    /** Returns true iff `c` is a Unicode scalar value not in the reserved
        range.

        \see Unicode 3.9/D90 */
    inline constexpr bool unreserved_scalar_value(char32_t c)
    {
        return scalar_value(c) && !reserved_noncharacter(c);
    }

    /** Returns true iff `c` is a UTF-8 lead code unit (which must be followed
        by 1-3 following units). */
    constexpr bool lead_code_unit(char8_type c)
    {
        return uint8_t((unsigned char)c - 0xc2) <= 0x32;
    }

    /** Returns true iff `c` is a UTF-8 continuation code unit. */
    constexpr bool continuation(char8_type c) { return (int8_t)c < -0x40; }

    /** Given the first (and possibly only) code unit of a UTF-8-encoded code
        point, returns the number of bytes occupied by that code point (in the
        range `[1, 4]`).  Returns a value < 0 if `first_unit` is not a valid
        initial UTF-8 code unit. */
    inline constexpr int utf8_code_units(char8_type first_unit_)
    {
        auto first_unit = (unsigned int)first_unit_;
        return first_unit <= 0x7f ? 1
               : boost::parser::detail::text::lead_code_unit(first_unit)
                   ? int(0xe0 <= first_unit) + int(0xf0 <= first_unit) + 2
                   : -1;
    }

    /** Given the first (and possibly only) code unit of a UTF-16-encoded code
        point, returns the number of code units occupied by that code point
        (in the range `[1, 2]`).  Returns a negative value if `first_unit` is
        not a valid initial UTF-16 code unit. */
    inline constexpr int utf16_code_units(char16_t first_unit)
    {
        if (boost::parser::detail::text::low_surrogate(first_unit))
            return -1;
        if (boost::parser::detail::text::high_surrogate(first_unit))
            return 2;
        return 1;
    }

    namespace detail {
        // optional is not constexpr friendly.
        template<typename Iter>
        struct optional_iter
        {
            constexpr optional_iter() : it_(), valid_(false) {}
            constexpr optional_iter(Iter it) : it_(it), valid_(true) {}

            constexpr operator bool() const { return valid_; }
            constexpr Iter operator*() const
            {
                BOOST_PARSER_DEBUG_ASSERT(valid_);
                return it_;
            }
            Iter & operator*()
            {
                BOOST_PARSER_DEBUG_ASSERT(valid_);
                return it_;
            }

            friend BOOST_PARSER_CONSTEXPR bool
            operator==(optional_iter lhs, optional_iter rhs)
            {
                return lhs.valid_ == rhs.valid_ &&
                       (!lhs.valid_ || lhs.it_ == rhs.it_);
            }
            friend BOOST_PARSER_CONSTEXPR bool
            operator!=(optional_iter lhs, optional_iter rhs)
            {
                return !(lhs == rhs);
            }

        private:
            Iter it_;
            bool valid_;
        };

        // Follow Table 3-7 in Unicode 3.9/D92
        template<typename Iter>
        constexpr optional_iter<Iter> end_of_invalid_utf8(Iter it)
        {
            BOOST_PARSER_DEBUG_ASSERT(!boost::parser::detail::text::continuation(*it));

            if (detail::in(0, *it, 0x7f))
                return optional_iter<Iter>{};

            if (detail::in(0xc2, *it, 0xdf)) {
                auto next = it;
                if (!boost::parser::detail::text::continuation(*++next))
                    return next;
                return optional_iter<Iter>{};
            }

            if (detail::in(0xe0, *it, 0xe0)) {
                auto next = it;
                if (!detail::in(0xa0, *++next, 0xbf))
                    return next;
                if (!boost::parser::detail::text::continuation(*++next))
                    return next;
                return optional_iter<Iter>{};
            }
            if (detail::in(0xe1, *it, 0xec)) {
                auto next = it;
                if (!boost::parser::detail::text::continuation(*++next))
                    return next;
                if (!boost::parser::detail::text::continuation(*++next))
                    return next;
                return optional_iter<Iter>{};
            }
            if (detail::in(0xed, *it, 0xed)) {
                auto next = it;
                if (!detail::in(0x80, *++next, 0x9f))
                    return next;
                if (!boost::parser::detail::text::continuation(*++next))
                    return next;
                return optional_iter<Iter>{};
            }
            if (detail::in(0xee, *it, 0xef)) {
                auto next = it;
                if (!boost::parser::detail::text::continuation(*++next))
                    return next;
                if (!boost::parser::detail::text::continuation(*++next))
                    return next;
                return optional_iter<Iter>{};
            }

            if (detail::in(0xf0, *it, 0xf0)) {
                auto next = it;
                if (!detail::in(0x90, *++next, 0xbf))
                    return next;
                if (!boost::parser::detail::text::continuation(*++next))
                    return next;
                if (!boost::parser::detail::text::continuation(*++next))
                    return next;
                return optional_iter<Iter>{};
            }
            if (detail::in(0xf1, *it, 0xf3)) {
                auto next = it;
                if (!boost::parser::detail::text::continuation(*++next))
                    return next;
                if (!boost::parser::detail::text::continuation(*++next))
                    return next;
                if (!boost::parser::detail::text::continuation(*++next))
                    return next;
                return optional_iter<Iter>{};
            }
            if (detail::in(0xf4, *it, 0xf4)) {
                auto next = it;
                if (!detail::in(0x80, *++next, 0x8f))
                    return next;
                if (!boost::parser::detail::text::continuation(*++next))
                    return next;
                if (!boost::parser::detail::text::continuation(*++next))
                    return next;
                return optional_iter<Iter>{};
            }

            return it;
        }

        template<typename Iter>
        constexpr Iter decrement(Iter it)
        {
            Iter retval = it;

            int backup = 0;
            while (backup < 4 && boost::parser::detail::text::continuation(*--retval)) {
                ++backup;
            }
            backup = it - retval;

            if (boost::parser::detail::text::continuation(*retval))
                return it - 1;

            optional_iter<Iter> first_invalid = end_of_invalid_utf8(retval);
            if (first_invalid == retval)
                ++*first_invalid;
            while (first_invalid && (*first_invalid - retval) < backup) {
                backup -= *first_invalid - retval;
                retval = *first_invalid;
                first_invalid = end_of_invalid_utf8(retval);
                if (first_invalid == retval)
                    ++*first_invalid;
            }

            if (1 < backup) {
                int const cp_bytes = boost::parser::detail::text::utf8_code_units(*retval);
                if (cp_bytes < backup)
                    retval = it - 1;
            }

            return retval;
        }

        template<typename Iter>
        constexpr Iter decrement(Iter first, Iter it)
        {
            Iter retval = it;

            int backup = 0;
            while (backup < 4 && retval != first &&
                   boost::parser::detail::text::continuation(*--retval)) {
                ++backup;
            }
            backup = (int)std::distance(retval, it);

            if (boost::parser::detail::text::continuation(*retval)) {
                if (it != first)
                    --it;
                return it;
            }

            optional_iter<Iter> first_invalid = end_of_invalid_utf8(retval);
            if (first_invalid == retval)
                ++*first_invalid;
            while (first_invalid &&
                   std::distance(retval, *first_invalid) < backup) {
                backup -= (int)std::distance(retval, *first_invalid);
                retval = *first_invalid;
                first_invalid = end_of_invalid_utf8(retval);
                if (first_invalid == retval)
                    ++*first_invalid;
            }

            if (1 < backup) {
                int const cp_bytes = boost::parser::detail::text::utf8_code_units(*retval);
                if (cp_bytes < backup) {
                    if (it != first)
                        --it;
                    retval = it;
                }
            }

            return retval;
        }

        enum char_class : uint8_t {
            ill = 0,
            asc = 1,
            cr1 = 2,
            cr2 = 3,
            cr3 = 4,
            l2a = 5,
            l3a = 6,
            l3b = 7,
            l3c = 8,
            l4a = 9,
            l4b = 10,
            l4c = 11,
        };

        enum table_state : uint8_t {
            bgn = 0,
            e_d = bgn, // "end"
            err = 12,
            cs1 = 24,
            cs2 = 36,
            cs3 = 48,
            p3a = 60,
            p3b = 72,
            p4a = 84,
            p4b = 96,
            invalid_table_state = 200
        };

        struct first_cu
        {
            unsigned char initial_octet;
            table_state next;
        };

        namespace {
            constexpr first_cu first_cus[256] = {
                {0x00, bgn}, {0x01, bgn}, {0x02, bgn}, {0x03, bgn}, {0x04, bgn},
                {0x05, bgn}, {0x06, bgn}, {0x07, bgn}, {0x08, bgn}, {0x09, bgn},
                {0x0a, bgn}, {0x0b, bgn}, {0x0c, bgn}, {0x0d, bgn}, {0x0e, bgn},
                {0x0f, bgn}, {0x10, bgn}, {0x11, bgn}, {0x12, bgn}, {0x13, bgn},
                {0x14, bgn}, {0x15, bgn}, {0x16, bgn}, {0x17, bgn}, {0x18, bgn},
                {0x19, bgn}, {0x1a, bgn}, {0x1b, bgn}, {0x1c, bgn}, {0x1d, bgn},
                {0x1e, bgn}, {0x1f, bgn}, {0x20, bgn}, {0x21, bgn}, {0x22, bgn},
                {0x23, bgn}, {0x24, bgn}, {0x25, bgn}, {0x26, bgn}, {0x27, bgn},
                {0x28, bgn}, {0x29, bgn}, {0x2a, bgn}, {0x2b, bgn}, {0x2c, bgn},
                {0x2d, bgn}, {0x2e, bgn}, {0x2f, bgn}, {0x30, bgn}, {0x31, bgn},
                {0x32, bgn}, {0x33, bgn}, {0x34, bgn}, {0x35, bgn}, {0x36, bgn},
                {0x37, bgn}, {0x38, bgn}, {0x39, bgn}, {0x3a, bgn}, {0x3b, bgn},
                {0x3c, bgn}, {0x3d, bgn}, {0x3e, bgn}, {0x3f, bgn}, {0x40, bgn},
                {0x41, bgn}, {0x42, bgn}, {0x43, bgn}, {0x44, bgn}, {0x45, bgn},
                {0x46, bgn}, {0x47, bgn}, {0x48, bgn}, {0x49, bgn}, {0x4a, bgn},
                {0x4b, bgn}, {0x4c, bgn}, {0x4d, bgn}, {0x4e, bgn}, {0x4f, bgn},
                {0x50, bgn}, {0x51, bgn}, {0x52, bgn}, {0x53, bgn}, {0x54, bgn},
                {0x55, bgn}, {0x56, bgn}, {0x57, bgn}, {0x58, bgn}, {0x59, bgn},
                {0x5a, bgn}, {0x5b, bgn}, {0x5c, bgn}, {0x5d, bgn}, {0x5e, bgn},
                {0x5f, bgn}, {0x60, bgn}, {0x61, bgn}, {0x62, bgn}, {0x63, bgn},
                {0x64, bgn}, {0x65, bgn}, {0x66, bgn}, {0x67, bgn}, {0x68, bgn},
                {0x69, bgn}, {0x6a, bgn}, {0x6b, bgn}, {0x6c, bgn}, {0x6d, bgn},
                {0x6e, bgn}, {0x6f, bgn}, {0x70, bgn}, {0x71, bgn}, {0x72, bgn},
                {0x73, bgn}, {0x74, bgn}, {0x75, bgn}, {0x76, bgn}, {0x77, bgn},
                {0x78, bgn}, {0x79, bgn}, {0x7a, bgn}, {0x7b, bgn}, {0x7c, bgn},
                {0x7d, bgn}, {0x7e, bgn}, {0x7f, bgn}, {0x00, err}, {0x01, err},
                {0x02, err}, {0x03, err}, {0x04, err}, {0x05, err}, {0x06, err},
                {0x07, err}, {0x08, err}, {0x09, err}, {0x0a, err}, {0x0b, err},
                {0x0c, err}, {0x0d, err}, {0x0e, err}, {0x0f, err}, {0x10, err},
                {0x11, err}, {0x12, err}, {0x13, err}, {0x14, err}, {0x15, err},
                {0x16, err}, {0x17, err}, {0x18, err}, {0x19, err}, {0x1a, err},
                {0x1b, err}, {0x1c, err}, {0x1d, err}, {0x1e, err}, {0x1f, err},
                {0x20, err}, {0x21, err}, {0x22, err}, {0x23, err}, {0x24, err},
                {0x25, err}, {0x26, err}, {0x27, err}, {0x28, err}, {0x29, err},
                {0x2a, err}, {0x2b, err}, {0x2c, err}, {0x2d, err}, {0x2e, err},
                {0x2f, err}, {0x30, err}, {0x31, err}, {0x32, err}, {0x33, err},
                {0x34, err}, {0x35, err}, {0x36, err}, {0x37, err}, {0x38, err},
                {0x39, err}, {0x3a, err}, {0x3b, err}, {0x3c, err}, {0x3d, err},
                {0x3e, err}, {0x3f, err}, {0xc0, err}, {0xc1, err}, {0x02, cs1},
                {0x03, cs1}, {0x04, cs1}, {0x05, cs1}, {0x06, cs1}, {0x07, cs1},
                {0x08, cs1}, {0x09, cs1}, {0x0a, cs1}, {0x0b, cs1}, {0x0c, cs1},
                {0x0d, cs1}, {0x0e, cs1}, {0x0f, cs1}, {0x10, cs1}, {0x11, cs1},
                {0x12, cs1}, {0x13, cs1}, {0x14, cs1}, {0x15, cs1}, {0x16, cs1},
                {0x17, cs1}, {0x18, cs1}, {0x19, cs1}, {0x1a, cs1}, {0x1b, cs1},
                {0x1c, cs1}, {0x1d, cs1}, {0x1e, cs1}, {0x1f, cs1}, {0x00, p3a},
                {0x01, cs2}, {0x02, cs2}, {0x03, cs2}, {0x04, cs2}, {0x05, cs2},
                {0x06, cs2}, {0x07, cs2}, {0x08, cs2}, {0x09, cs2}, {0x0a, cs2},
                {0x0b, cs2}, {0x0c, cs2}, {0x0d, p3b}, {0x0e, cs2}, {0x0f, cs2},
                {0x00, p4a}, {0x01, cs3}, {0x02, cs3}, {0x03, cs3}, {0x04, p4b},
                {0xf5, err}, {0xf6, err}, {0xf7, err}, {0xf8, err}, {0xf9, err},
                {0xfa, err}, {0xfb, err}, {0xfc, err}, {0xfd, err}, {0xfe, err},
                {0xff, err},
            };

            constexpr char_class octet_classes[256] = {
                asc, asc, asc, asc, asc, asc, asc, asc, asc, asc, asc, asc, asc,
                asc, asc, asc, asc, asc, asc, asc, asc, asc, asc, asc, asc, asc,
                asc, asc, asc, asc, asc, asc, asc, asc, asc, asc, asc, asc, asc,
                asc, asc, asc, asc, asc, asc, asc, asc, asc, asc, asc, asc, asc,
                asc, asc, asc, asc, asc, asc, asc, asc, asc, asc, asc, asc, asc,
                asc, asc, asc, asc, asc, asc, asc, asc, asc, asc, asc, asc, asc,
                asc, asc, asc, asc, asc, asc, asc, asc, asc, asc, asc, asc, asc,
                asc, asc, asc, asc, asc, asc, asc, asc, asc, asc, asc, asc, asc,
                asc, asc, asc, asc, asc, asc, asc, asc, asc, asc, asc, asc, asc,
                asc, asc, asc, asc, asc, asc, asc, asc, asc, asc, asc, cr1, cr1,
                cr1, cr1, cr1, cr1, cr1, cr1, cr1, cr1, cr1, cr1, cr1, cr1, cr1,
                cr1, cr2, cr2, cr2, cr2, cr2, cr2, cr2, cr2, cr2, cr2, cr2, cr2,
                cr2, cr2, cr2, cr2, cr3, cr3, cr3, cr3, cr3, cr3, cr3, cr3, cr3,
                cr3, cr3, cr3, cr3, cr3, cr3, cr3, cr3, cr3, cr3, cr3, cr3, cr3,
                cr3, cr3, cr3, cr3, cr3, cr3, cr3, cr3, cr3, cr3, ill, ill, l2a,
                l2a, l2a, l2a, l2a, l2a, l2a, l2a, l2a, l2a, l2a, l2a, l2a, l2a,
                l2a, l2a, l2a, l2a, l2a, l2a, l2a, l2a, l2a, l2a, l2a, l2a, l2a,
                l2a, l2a, l2a, l3a, l3b, l3b, l3b, l3b, l3b, l3b, l3b, l3b, l3b,
                l3b, l3b, l3b, l3c, l3b, l3b, l4a, l4b, l4b, l4b, l4c, ill, ill,
                ill, ill, ill, ill, ill, ill, ill, ill, ill,
            };

            constexpr table_state transitions[108] = {
                err, e_d, err, err, err, cs1, p3a, cs2, p3b, p4a, cs3, p4b,
                err, err, err, err, err, err, err, err, err, err, err, err,
                err, err, e_d, e_d, e_d, err, err, err, err, err, err, err,
                err, err, cs1, cs1, cs1, err, err, err, err, err, err, err,
                err, err, cs2, cs2, cs2, err, err, err, err, err, err, err,
                err, err, err, err, cs1, err, err, err, err, err, err, err,
                err, err, cs1, cs1, err, err, err, err, err, err, err, err,
                err, err, err, cs2, cs2, err, err, err, err, err, err, err,
                err, err, cs2, err, err, err, err, err, err, err, err, err,
            };
        }

        template<typename InputIter, typename Sentinel>
        char32_t advance(InputIter & first, Sentinel last)
        {
            char32_t retval = 0;

            first_cu const info = first_cus[(unsigned char)*first];
            ++first;

            retval = info.initial_octet;
            int state = info.next;

            while (state != bgn) {
                if (first != last) {
                    unsigned char const cu = *first;
                    retval = (retval << 6) | (cu & 0x3f);
                    char_class const class_ = octet_classes[cu];
                    state = transitions[state + class_];
                    if (state == err)
                        return replacement_character;
                    ++first;
                } else {
                    return replacement_character;
                }
            }

            return retval;
        }

        template<typename Derived, typename Iter>
        struct trans_ins_iter
        {
            using value_type = void;
            using difference_type =
#if BOOST_PARSER_DETAIL_TEXT_USE_CONCEPTS
                std::ptrdiff_t;
#else
                void;
#endif
            using pointer = void;
            using reference = void;
            using iterator_category = std::output_iterator_tag;

            constexpr trans_ins_iter() {}
            constexpr trans_ins_iter(Iter it) : it_(it) {}
            constexpr Derived & operator*() { return derived(); }
            constexpr Derived & operator++() { return derived(); }
            constexpr Derived operator++(int) { return derived(); }
            constexpr Iter base() const { return it_; }

        protected:
            constexpr Iter & iter() { return it_; }

        private:
            constexpr Derived & derived()
            {
                return static_cast<Derived &>(*this);
            }
            Iter it_;
        };

        template<typename Derived, typename I, typename ValueType>
        using trans_iter = stl_interfaces::iterator_interface<
            Derived,
            bidirectional_at_most_t<I>,
            ValueType,
            ValueType>;
    }

}}

namespace boost::parser::detail { namespace text { BOOST_PARSER_DETAIL_TEXT_NAMESPACE_V1 {

#if defined(BOOST_TEXT_DOXYGEN)

    /** Returns the first code unit in `[r.begin(), r.end())` that is not
        properly UTF-8 encoded, or `r.begin() + std::distance(r)` if no such
        code unit is found. */
    template<utf8_range R>
        requires std::ranges::forward_range<R>
    constexpr std::ranges::borrowed_iterator_t<R> find_invalid_encoding(R && r);

    /** Returns the first code unit in `[r.begin(), r.end())` that is not
        properly UTF-16 encoded, or `r.begin() + std::distance(r)` if no such
        code unit is found. */
    template<utf16_range R>
        requires std::ranges::forward_range<R>
    constexpr std::ranges::borrowed_iterator_t<R> find_invalid_encoding(R && r);

    /** Returns true iff `r` is properly UTF-8 encoded. */
    template<utf8_range R>
        requires std::ranges::forward_range<R>
    constexpr bool encoded(R && r);

    /** Returns true iff `r` is properly UTF-16 encoded */
    template<utf16_range R>
        requires std::ranges::forward_range<R>
    constexpr bool encoded(R && r);

    /** Returns true iff `r` is empty or the initial UTF-8 code units in `r`
        form a valid Unicode code point. */
    template<utf8_range R>
        requires std::ranges::forward_range<R>
    constexpr bool starts_encoded(R && r);

    /** Returns true iff `r` is empty or the initial UTF-16 code units in `r`
        form a valid Unicode code point. */
    template<utf16_range R>
        requires std::ranges::forward_range<R>
    constexpr bool starts_encoded(R && r);

    /** Returns true iff `r` is empty or the final UTF-8 code units in `r`
        form a valid Unicode code point. */
    template<utf8_range R>
        requires std::ranges::bidirectional_range<R> &&
                 std::ranges::common_range<R>
    constexpr bool ends_encoded(R && r);

    /** Returns true iff `r` is empty or the final UTF-16 code units in `r`
        form a valid Unicode code point. */
    template<utf8_range R>
        requires std::ranges::bidirectional_range<R> &&
                 std::ranges::common_range<R>
    constexpr bool ends_encoded(R && r);

#endif

    template<typename Iter>
    constexpr detail::enable_utf8_cp_t<Iter>
    find_invalid_encoding(Iter first, Iter last)
    {
        while (first != last) {
            int const cp_bytes = boost::parser::detail::text::utf8_code_units(*first);
            if (cp_bytes == -1 || last - first < cp_bytes)
                return first;

            if (detail::end_of_invalid_utf8(first))
                return first;

            first += cp_bytes;
        }

        return last;
    }

    template<typename Iter>
    constexpr detail::enable_utf16_cp_t<Iter>
    find_invalid_encoding(Iter first, Iter last)
    {
        while (first != last) {
            int const cp_units = boost::parser::detail::text::utf16_code_units(*first);
            if (cp_units == -1 || last - first < cp_units)
                return first;

            if (cp_units == 2 && !boost::parser::detail::text::low_surrogate(*(first + 1)))
                return first;

            first += cp_units;
        }

        return last;
    }

    template<typename Iter>
    constexpr detail::enable_utf8_cp_t<Iter, bool> encoded(
        Iter first, Iter last)
    {
        return v1::find_invalid_encoding(first, last) == last;
    }

    template<typename Iter>
    constexpr detail::enable_utf16_cp_t<Iter, bool> encoded(
        Iter first, Iter last)
    {
        return v1::find_invalid_encoding(first, last) == last;
    }

    template<typename Iter>
    constexpr detail::enable_utf8_cp_t<Iter, bool>
    starts_encoded(Iter first, Iter last)
    {
        if (first == last)
            return true;

        int const cp_bytes = boost::parser::detail::text::utf8_code_units(*first);
        if (cp_bytes == -1 || last - first < cp_bytes)
            return false;

        return !detail::end_of_invalid_utf8(first);
    }

    template<typename Iter>
    constexpr detail::enable_utf16_cp_t<Iter, bool>
    starts_encoded(Iter first, Iter last)
    {
        if (first == last)
            return true;

        int const cp_units = boost::parser::detail::text::utf16_code_units(*first);
        if (cp_units == -1 || last - first < cp_units)
            return false;

        return cp_units == 1 || boost::parser::detail::text::low_surrogate(*(first + 1));
    }

    template<typename Iter>
    constexpr detail::enable_utf8_cp_t<Iter, bool>
    ends_encoded(Iter first, Iter last)
    {
        if (first == last)
            return true;

        auto it = last;
        while (first != --it && boost::parser::detail::text::continuation(*it))
            ;

        return v1::starts_encoded(it, last);
    }

    template<typename Iter>
    constexpr detail::enable_utf16_cp_t<Iter, bool>
    ends_encoded(Iter first, Iter last)
    {
        if (first == last)
            return true;

        auto it = last;
        if (boost::parser::detail::text::low_surrogate(*--it))
            --it;

        return v1::starts_encoded(it, last);
    }

}}}

#if BOOST_PARSER_DETAIL_TEXT_USE_CONCEPTS

namespace boost::parser::detail { namespace text { BOOST_PARSER_DETAIL_TEXT_NAMESPACE_V2 {

    template<utf8_range R>
        requires std::ranges::forward_range<R>
    constexpr std::ranges::borrowed_iterator_t<R> find_invalid_encoding(R && r)
    {
        auto first = std::ranges::begin(r);
        auto last = std::ranges::end(r);

        while (first != last) {
            int const cp_bytes = boost::parser::detail::text::utf8_code_units(*first);
            if (cp_bytes == -1 || last - first < cp_bytes)
                return first;

            if (detail::end_of_invalid_utf8(first))
                return first;

            first += cp_bytes;
        }

        if constexpr (std::ranges::borrowed_range<R>) {
            return last;
        } else {
            return std::ranges::dangling{};
        }
    }

    template<utf16_range R>
        requires std::ranges::forward_range<R>
    constexpr std::ranges::borrowed_iterator_t<R> find_invalid_encoding(R && r)
    {
        auto first = std::ranges::begin(r);
        auto last = std::ranges::end(r);

        while (first != last) {
            int const cp_units = boost::parser::detail::text::utf16_code_units(*first);
            if (cp_units == -1 || last - first < cp_units)
                return first;

            if (cp_units == 2 && !boost::parser::detail::text::low_surrogate(*(first + 1)))
                return first;

            first += cp_units;
        }

        if constexpr (std::ranges::borrowed_range<R>) {
            return last;
        } else {
            return std::ranges::dangling{};
        }
    }

    template<utf8_range R>
        requires std::ranges::forward_range<R>
    constexpr bool encoded(R && r)
    {
        return boost::parser::detail::text::v1::find_invalid_encoding(r.begin(), r.end()) ==
               r.end();
    }

    template<utf16_range R>
        requires std::ranges::forward_range<R>
    constexpr bool encoded(R && r)
    {
        return boost::parser::detail::text::v1::find_invalid_encoding(r.begin(), r.end()) ==
               r.end();
    }

    template<utf8_range R>
        requires std::ranges::forward_range<R>
    constexpr bool starts_encoded(R && r)
    {
        auto first = std::ranges::begin(r);
        auto last = std::ranges::end(r);

        if (first == last)
            return true;

        int const cp_bytes = boost::parser::detail::text::utf8_code_units(*first);
        if (cp_bytes == -1 || last - first < cp_bytes)
            return false;

        return !detail::end_of_invalid_utf8(first);
    }

    template<utf16_range R>
        requires std::ranges::forward_range<R>
    constexpr bool starts_encoded(R && r)
    {
        auto first = std::ranges::begin(r);
        auto last = std::ranges::end(r);

        if (first == last)
            return true;

        int const cp_units = boost::parser::detail::text::utf16_code_units(*first);
        if (cp_units == -1 || last - first < cp_units)
            return false;

        return cp_units == 1 || boost::parser::detail::text::low_surrogate(*(first + 1));
    }

    template<utf8_range R>
        requires std::ranges::bidirectional_range<R> &&
                 std::ranges::common_range<R>
    constexpr bool ends_encoded(R && r)
    {
        auto first = std::ranges::begin(r);
        auto last = std::ranges::end(r);

        if (first == last)
            return true;

        auto it = last;
        while (first != --it && boost::parser::detail::text::continuation(*it))
            ;

        return boost::parser::detail::text::starts_encoded(it, last);
    }

    template<utf16_range R>
        requires std::ranges::bidirectional_range<R> &&
                 std::ranges::common_range<R>
    constexpr bool ends_encoded(R && r)
    {
        auto first = std::ranges::begin(r);
        auto last = std::ranges::end(r);

        if (first == last)
            return true;

        auto it = last;
        if (boost::parser::detail::text::low_surrogate(*--it))
            --it;

        return boost::parser::detail::text::starts_encoded(it, last);
    }

}}}

#endif

namespace boost::parser::detail { namespace text {

    /** An error handler type that can be used with the converting iterators;
        provides the Unicode replacement character on errors. */
    struct use_replacement_character
    {
        constexpr char32_t operator()(std::string_view) const noexcept
        {
            return replacement_character;
        }
    };

    /** A sentinel type that compares equal to a pointer to a 1-, 2-, or
        4-byte integral value, iff the pointer is null. */
    struct null_sentinel_t
    {
#if BOOST_PARSER_DETAIL_TEXT_USE_CONCEPTS
        template<std::input_iterator I>
            requires std::default_initializable<std::iter_value_t<I>> &&
                     std::equality_comparable_with<std::iter_reference_t<I>, std::iter_value_t<I>>
#else
        template<typename I>
#endif
        friend constexpr bool operator==(I it, null_sentinel_t)
        {
            return *it == detail::iter_value_t<I>{};
        }
#if !defined(__cpp_impl_three_way_comparison)
        template<typename I>
        friend constexpr bool operator==(null_sentinel_t, I it)
        {
            return *it == detail::iter_value_t<I>{};
        }
        template<typename I>
        friend constexpr bool operator!=(I it, null_sentinel_t)
        {
            return *it != detail::iter_value_t<I>{};
        }
        template<typename I>
        friend constexpr bool operator!=(null_sentinel_t, I it)
        {
            return *it != detail::iter_value_t<I>{};
        }
#endif
    };

#if defined(__cpp_inline_variables)
    inline constexpr null_sentinel_t null_sentinel;
#else
    namespace {
        constexpr null_sentinel_t null_sentinel;
    }
#endif

    /** An out iterator that converts UTF-32 to UTF-8. */
#if BOOST_PARSER_DETAIL_TEXT_USE_CONCEPTS
    template<std::output_iterator<char8_t> Iter>
#else
    template<typename Iter>
#endif
    struct utf_32_to_8_out_iterator
        : detail::trans_ins_iter<utf_32_to_8_out_iterator<Iter>, Iter>
    {
        constexpr utf_32_to_8_out_iterator() {}
        explicit constexpr utf_32_to_8_out_iterator(Iter it) :
            detail::trans_ins_iter<utf_32_to_8_out_iterator<Iter>, Iter>(it)
        {}

        constexpr utf_32_to_8_out_iterator & operator=(char32_t cp)
        {
            auto & out = this->iter();
            out = detail::write_cp_utf8(cp, out);
            return *this;
        }
    };

    /** An insert-iterator analogous to std::insert_iterator, that also
        converts UTF-32 to UTF-8. */
    template<typename Cont>
    struct utf_32_to_8_insert_iterator : detail::trans_ins_iter<
                                             utf_32_to_8_insert_iterator<Cont>,
                                             std::insert_iterator<Cont>>
    {
        constexpr utf_32_to_8_insert_iterator() {}
        constexpr utf_32_to_8_insert_iterator(
            Cont & c, typename Cont::iterator it) :
            detail::trans_ins_iter<
                utf_32_to_8_insert_iterator<Cont>,
                std::insert_iterator<Cont>>(std::insert_iterator<Cont>(c, it))
        {}

        constexpr utf_32_to_8_insert_iterator & operator=(char32_t cp)
        {
            auto & out = this->iter();
            out = detail::write_cp_utf8(cp, out);
            return *this;
        }
    };

    /** An insert-iterator analogous to std::front_insert_iterator, that also
        converts UTF-32 to UTF-8. */
    template<typename Cont>
    struct utf_32_to_8_front_insert_iterator
        : detail::trans_ins_iter<
              utf_32_to_8_front_insert_iterator<Cont>,
              std::front_insert_iterator<Cont>>
    {
        constexpr utf_32_to_8_front_insert_iterator() {}
        explicit constexpr utf_32_to_8_front_insert_iterator(Cont & c) :
            detail::trans_ins_iter<
                utf_32_to_8_front_insert_iterator<Cont>,
                std::front_insert_iterator<Cont>>(
                std::front_insert_iterator<Cont>(c))
        {}

        constexpr utf_32_to_8_front_insert_iterator & operator=(char32_t cp)
        {
            auto & out = this->iter();
            out = detail::write_cp_utf8(cp, out);
            return *this;
        }
    };

    /** An insert-iterator analogous to std::back_insert_iterator, that also
        converts UTF-32 to UTF-8. */
    template<typename Cont>
    struct utf_32_to_8_back_insert_iterator
        : detail::trans_ins_iter<
              utf_32_to_8_back_insert_iterator<Cont>,
              std::back_insert_iterator<Cont>>
    {
        constexpr utf_32_to_8_back_insert_iterator() {}
        explicit constexpr utf_32_to_8_back_insert_iterator(Cont & c) :
            detail::trans_ins_iter<
                utf_32_to_8_back_insert_iterator<Cont>,
                std::back_insert_iterator<Cont>>(
                std::back_insert_iterator<Cont>(c))
        {}

        constexpr utf_32_to_8_back_insert_iterator & operator=(char32_t cp)
        {
            auto & out = this->iter();
            out = detail::write_cp_utf8(cp, out);
            return *this;
        }
    };


    namespace detail {
        template<typename OutIter>
        OutIter assign_8_to_32_insert(
            unsigned char cu, char32_t & cp, int & state, OutIter out)
        {
            auto write = [&] {
                *out = cp;
                ++out;
                state = invalid_table_state;
            };
            auto start_cp = [&] {
                first_cu const info = first_cus[cu];
                state = info.next;
                cp = info.initial_octet;
                if (state == bgn)
                    write();
            };
            if (state == invalid_table_state) {
                start_cp();
            } else {
                cp = (cp << 6) | (cu & 0x3f);
                char_class const class_ = octet_classes[cu];
                state = transitions[state + class_];
                if (state == bgn) {
                    write();
                } else if (state == err) {
                    *out = replacement_character;
                    ++out;
                    start_cp();
                }
            }
            return out;
        }
    }

    /** An out iterator that converts UTF-8 to UTF-32. */
#if BOOST_PARSER_DETAIL_TEXT_USE_CONCEPTS
    template<std::output_iterator<char32_t> Iter>
#else
    template<typename Iter>
#endif
    struct utf_8_to_32_out_iterator
        : detail::trans_ins_iter<utf_8_to_32_out_iterator<Iter>, Iter>
    {
        constexpr utf_8_to_32_out_iterator() {}
        explicit constexpr utf_8_to_32_out_iterator(Iter it) :
            detail::trans_ins_iter<utf_8_to_32_out_iterator<Iter>, Iter>(it),
            state_(detail::invalid_table_state)
        {}

        constexpr utf_8_to_32_out_iterator & operator=(char8_type cu)
        {
            auto & out = this->iter();
            out = detail::assign_8_to_32_insert(cu, cp_, state_, out);
            return *this;
        }

#ifndef BOOST_TEXT_DOXYGEN
    private:
        int state_;
        char32_t cp_;
#endif
    };

    /** An insert-iterator analogous to std::insert_iterator, that also
        converts UTF-8 to UTF-32. */
    template<typename Cont>
    struct utf_8_to_32_insert_iterator : detail::trans_ins_iter<
                                             utf_8_to_32_insert_iterator<Cont>,
                                             std::insert_iterator<Cont>>
    {
        constexpr utf_8_to_32_insert_iterator() {}
        constexpr utf_8_to_32_insert_iterator(
            Cont & c, typename Cont::iterator it) :
            detail::trans_ins_iter<
                utf_8_to_32_insert_iterator<Cont>,
                std::insert_iterator<Cont>>(std::insert_iterator<Cont>(c, it)),
            state_(detail::invalid_table_state)
        {}

        constexpr utf_8_to_32_insert_iterator & operator=(char16_t cu)
        {
            auto & out = this->iter();
            out = detail::assign_8_to_32_insert(cu, cp_, state_, out);
            return *this;
        }

#ifndef BOOST_TEXT_DOXYGEN
    private:
        int state_;
        char32_t cp_;
#endif
    };

    /** An insert-iterator analogous to std::front_insert_iterator, that also
        converts UTF-8 to UTF-32. */
    template<typename Cont>
    struct utf_8_to_32_front_insert_iterator
        : detail::trans_ins_iter<
              utf_8_to_32_front_insert_iterator<Cont>,
              std::front_insert_iterator<Cont>>
    {
        constexpr utf_8_to_32_front_insert_iterator() {}
        explicit constexpr utf_8_to_32_front_insert_iterator(Cont & c) :
            detail::trans_ins_iter<
                utf_8_to_32_front_insert_iterator<Cont>,
                std::front_insert_iterator<Cont>>(
                std::front_insert_iterator<Cont>(c)),
            state_(detail::invalid_table_state)
        {}

        constexpr utf_8_to_32_front_insert_iterator & operator=(char16_t cu)
        {
            auto & out = this->iter();
            out = detail::assign_8_to_32_insert(cu, cp_, state_, out);
            return *this;
        }

#ifndef BOOST_TEXT_DOXYGEN
    private:
        int state_;
        char32_t cp_;
#endif
    };

    /** An insert-iterator analogous to std::back_insert_iterator, that also
        converts UTF-8 to UTF-32. */
    template<typename Cont>
    struct utf_8_to_32_back_insert_iterator
        : detail::trans_ins_iter<
              utf_8_to_32_back_insert_iterator<Cont>,
              std::back_insert_iterator<Cont>>
    {
        constexpr utf_8_to_32_back_insert_iterator() {}
        explicit constexpr utf_8_to_32_back_insert_iterator(Cont & c) :
            detail::trans_ins_iter<
                utf_8_to_32_back_insert_iterator<Cont>,
                std::back_insert_iterator<Cont>>(
                std::back_insert_iterator<Cont>(c)),
            state_(detail::invalid_table_state)
        {}

        constexpr utf_8_to_32_back_insert_iterator & operator=(char16_t cu)
        {
            auto & out = this->iter();
            out = detail::assign_8_to_32_insert(cu, cp_, state_, out);
            return *this;
        }

#ifndef BOOST_TEXT_DOXYGEN
    private:
        int state_;
        char32_t cp_;
#endif
    };


    /** An out iterator that converts UTF-8 to UTF-16. */
#if BOOST_PARSER_DETAIL_TEXT_USE_CONCEPTS
    template<std::output_iterator<char16_t> Iter>
#else
    template<typename Iter>
#endif
    struct utf_32_to_16_out_iterator
        : detail::trans_ins_iter<utf_32_to_16_out_iterator<Iter>, Iter>
    {
        constexpr utf_32_to_16_out_iterator() {}
        explicit constexpr utf_32_to_16_out_iterator(Iter it) :
            detail::trans_ins_iter<utf_32_to_16_out_iterator<Iter>, Iter>(it)
        {}

        constexpr utf_32_to_16_out_iterator & operator=(char32_t cp)
        {
            auto & out = this->iter();
            out = detail::write_cp_utf16(cp, out);
            return *this;
        }
    };

    /** An insert-iterator analogous to std::insert_iterator, that also
        converts UTF-32 to UTF-16. */
    template<typename Cont>
    struct utf_32_to_16_insert_iterator
        : detail::trans_ins_iter<
              utf_32_to_16_insert_iterator<Cont>,
              std::insert_iterator<Cont>>
    {
        constexpr utf_32_to_16_insert_iterator() {}
        constexpr utf_32_to_16_insert_iterator(
            Cont & c, typename Cont::iterator it) :
            detail::trans_ins_iter<
                utf_32_to_16_insert_iterator<Cont>,
                std::insert_iterator<Cont>>(std::insert_iterator<Cont>(c, it))
        {}

        constexpr utf_32_to_16_insert_iterator & operator=(char32_t cp)
        {
            auto & out = this->iter();
            out = detail::write_cp_utf16(cp, out);
            return *this;
        }
    };

    /** An insert-iterator analogous to std::front_insert_iterator, that also
        converts UTF-32 to UTF-16. */
    template<typename Cont>
    struct utf_32_to_16_front_insert_iterator
        : detail::trans_ins_iter<
              utf_32_to_16_front_insert_iterator<Cont>,
              std::front_insert_iterator<Cont>>
    {
        constexpr utf_32_to_16_front_insert_iterator() {}
        explicit constexpr utf_32_to_16_front_insert_iterator(Cont & c) :
            detail::trans_ins_iter<
                utf_32_to_16_front_insert_iterator<Cont>,
                std::front_insert_iterator<Cont>>(
                std::front_insert_iterator<Cont>(c))
        {}

        constexpr utf_32_to_16_front_insert_iterator & operator=(char32_t cp)
        {
            auto & out = this->iter();
            out = detail::write_cp_utf16(cp, out);
            return *this;
        }
    };

    /** An insert-iterator analogous to std::back_insert_iterator, that also
        converts UTF-32 to UTF-16. */
    template<typename Cont>
    struct utf_32_to_16_back_insert_iterator
        : detail::trans_ins_iter<
              utf_32_to_16_back_insert_iterator<Cont>,
              std::back_insert_iterator<Cont>>
    {
        constexpr utf_32_to_16_back_insert_iterator() {}
        explicit constexpr utf_32_to_16_back_insert_iterator(Cont & c) :
            detail::trans_ins_iter<
                utf_32_to_16_back_insert_iterator<Cont>,
                std::back_insert_iterator<Cont>>(
                std::back_insert_iterator<Cont>(c))
        {}

        constexpr utf_32_to_16_back_insert_iterator & operator=(char32_t cp)
        {
            auto & out = this->iter();
            out = detail::write_cp_utf16(cp, out);
            return *this;
        }
    };


    namespace detail {
        template<typename OutIter>
        OutIter
        assign_16_to_32_insert(char16_t & prev_cu, char16_t cu, OutIter out)
        {
            if (high_surrogate(cu)) {
                if (prev_cu) {
                    *out = replacement_character;
                    ++out;
                }
                prev_cu = cu;
            } else if (low_surrogate(cu)) {
                if (prev_cu) {
                    *out = detail::surrogates_to_cp(prev_cu, cu);
                    ++out;
                } else {
                    *out = replacement_character;
                    ++out;
                }
                prev_cu = 0;
            } else {
                if (prev_cu) {
                    *out = replacement_character;
                    ++out;
                }
                *out = cu;
                ++out;
                prev_cu = 0;
            }
            return out;
        }
    }

    /** An out iterator that converts UTF-16 to UTF-32. */
#if BOOST_PARSER_DETAIL_TEXT_USE_CONCEPTS
    template<std::output_iterator<char32_t> Iter>
#else
    template<typename Iter>
#endif
    struct utf_16_to_32_out_iterator
        : detail::trans_ins_iter<utf_16_to_32_out_iterator<Iter>, Iter>
    {
        constexpr utf_16_to_32_out_iterator() {}
        explicit constexpr utf_16_to_32_out_iterator(Iter it) :
            detail::trans_ins_iter<utf_16_to_32_out_iterator<Iter>, Iter>(it),
            prev_cu_(0)
        {}

        constexpr utf_16_to_32_out_iterator & operator=(char16_t cu)
        {
            auto & out = this->iter();
            out = detail::assign_16_to_32_insert(prev_cu_, cu, out);
            return *this;
        }

#ifndef BOOST_TEXT_DOXYGEN
    private:
        char16_t prev_cu_;
#endif
    };

    /** An insert-iterator analogous to std::insert_iterator, that also
        converts UTF-16 to UTF-32. */
    template<typename Cont>
    struct utf_16_to_32_insert_iterator
        : detail::trans_ins_iter<
              utf_16_to_32_insert_iterator<Cont>,
              std::insert_iterator<Cont>>
    {
        constexpr utf_16_to_32_insert_iterator() {}
        constexpr utf_16_to_32_insert_iterator(
            Cont & c, typename Cont::iterator it) :
            detail::trans_ins_iter<
                utf_16_to_32_insert_iterator<Cont>,
                std::insert_iterator<Cont>>(std::insert_iterator<Cont>(c, it)),
            prev_cu_(0)
        {}

        constexpr utf_16_to_32_insert_iterator & operator=(char16_t cu)
        {
            auto & out = this->iter();
            out = detail::assign_16_to_32_insert(prev_cu_, cu, out);
            return *this;
        }

#ifndef BOOST_TEXT_DOXYGEN
    private:
        char16_t prev_cu_;
#endif
    };

    /** An insert-iterator analogous to std::front_insert_iterator, that also
        converts UTF-16 to UTF-32. */
    template<typename Cont>
    struct utf_16_to_32_front_insert_iterator
        : detail::trans_ins_iter<
              utf_16_to_32_front_insert_iterator<Cont>,
              std::front_insert_iterator<Cont>>
    {
        constexpr utf_16_to_32_front_insert_iterator() {}
        explicit constexpr utf_16_to_32_front_insert_iterator(Cont & c) :
            detail::trans_ins_iter<
                utf_16_to_32_front_insert_iterator<Cont>,
                std::front_insert_iterator<Cont>>(
                std::front_insert_iterator<Cont>(c)),
            prev_cu_(0)
        {}

        constexpr utf_16_to_32_front_insert_iterator & operator=(char16_t cu)
        {
            auto & out = this->iter();
            out = detail::assign_16_to_32_insert(prev_cu_, cu, out);
            return *this;
        }

#ifndef BOOST_TEXT_DOXYGEN
    private:
        char16_t prev_cu_;
#endif
    };

    /** An insert-iterator analogous to std::back_insert_iterator, that also
        converts UTF-16 to UTF-32. */
    template<typename Cont>
    struct utf_16_to_32_back_insert_iterator
        : detail::trans_ins_iter<
              utf_16_to_32_back_insert_iterator<Cont>,
              std::back_insert_iterator<Cont>>
    {
        constexpr utf_16_to_32_back_insert_iterator() {}
        explicit constexpr utf_16_to_32_back_insert_iterator(Cont & c) :
            detail::trans_ins_iter<
                utf_16_to_32_back_insert_iterator<Cont>,
                std::back_insert_iterator<Cont>>(
                std::back_insert_iterator<Cont>(c)),
            prev_cu_(0)
        {}

        constexpr utf_16_to_32_back_insert_iterator & operator=(char16_t cu)
        {
            auto & out = this->iter();
            out = detail::assign_16_to_32_insert(prev_cu_, cu, out);
            return *this;
        }

#ifndef BOOST_TEXT_DOXYGEN
    private:
        char16_t prev_cu_;
#endif
    };


    namespace detail {
        template<typename OutIter>
        OutIter
        assign_16_to_8_insert(char16_t & prev_cu, char16_t cu, OutIter out)
        {
            if (high_surrogate(cu)) {
                if (prev_cu)
                    out = detail::write_cp_utf8(replacement_character, out);
                prev_cu = cu;
            } else if (low_surrogate(cu)) {
                if (prev_cu) {
                    auto const cp = detail::surrogates_to_cp(prev_cu, cu);
                    out = detail::write_cp_utf8(cp, out);
                } else {
                    out = detail::write_cp_utf8(replacement_character, out);
                }
                prev_cu = 0;
            } else {
                if (prev_cu)
                    out = detail::write_cp_utf8(replacement_character, out);
                out = detail::write_cp_utf8(cu, out);
                prev_cu = 0;
            }
            return out;
        }
    }

    /** An out iterator that converts UTF-16 to UTF-8. */
#if BOOST_PARSER_DETAIL_TEXT_USE_CONCEPTS
    template<std::output_iterator<char8_t> Iter>
#else
    template<typename Iter>
#endif
    struct utf_16_to_8_out_iterator
        : detail::trans_ins_iter<utf_16_to_8_out_iterator<Iter>, Iter>
    {
        constexpr utf_16_to_8_out_iterator() {}
        explicit constexpr utf_16_to_8_out_iterator(Iter it) :
            detail::trans_ins_iter<utf_16_to_8_out_iterator<Iter>, Iter>(it),
            prev_cu_(0)
        {}

        constexpr utf_16_to_8_out_iterator & operator=(char16_t cu)
        {
            auto & out = this->iter();
            out = detail::assign_16_to_8_insert(prev_cu_, cu, out);
            return *this;
        }

#ifndef BOOST_TEXT_DOXYGEN
    private:
        char16_t prev_cu_;
#endif
    };

    /** An insert-iterator analogous to std::insert_iterator, that also
        converts UTF-16 to UTF-8. */
    template<typename Cont>
    struct utf_16_to_8_insert_iterator : detail::trans_ins_iter<
                                             utf_16_to_8_insert_iterator<Cont>,
                                             std::insert_iterator<Cont>>
    {
        constexpr utf_16_to_8_insert_iterator() {}
        constexpr utf_16_to_8_insert_iterator(
            Cont & c, typename Cont::iterator it) :
            detail::trans_ins_iter<
                utf_16_to_8_insert_iterator<Cont>,
                std::insert_iterator<Cont>>(std::insert_iterator<Cont>(c, it)),
            prev_cu_(0)
        {}

        constexpr utf_16_to_8_insert_iterator & operator=(char16_t cu)
        {
            auto & out = this->iter();
            out = detail::assign_16_to_8_insert(prev_cu_, cu, out);
            return *this;
        }

#ifndef BOOST_TEXT_DOXYGEN
    private:
        char16_t prev_cu_;
#endif
    };

    /** An insert-iterator analogous to std::front_insert_iterator, that also
        converts UTF-16 to UTF-8. */
    template<typename Cont>
    struct utf_16_to_8_front_insert_iterator
        : detail::trans_ins_iter<
              utf_16_to_8_front_insert_iterator<Cont>,
              std::front_insert_iterator<Cont>>
    {
        constexpr utf_16_to_8_front_insert_iterator() {}
        explicit constexpr utf_16_to_8_front_insert_iterator(Cont & c) :
            detail::trans_ins_iter<
                utf_16_to_8_front_insert_iterator<Cont>,
                std::front_insert_iterator<Cont>>(
                std::front_insert_iterator<Cont>(c)),
            prev_cu_(0)
        {}

        constexpr utf_16_to_8_front_insert_iterator & operator=(char16_t cu)
        {
            auto & out = this->iter();
            out = detail::assign_16_to_8_insert(prev_cu_, cu, out);
            return *this;
        }

#ifndef BOOST_TEXT_DOXYGEN
    private:
        char16_t prev_cu_;
#endif
    };

    /** An insert-iterator analogous to std::back_insert_iterator, that also
        converts UTF-16 to UTF-8. */
    template<typename Cont>
    struct utf_16_to_8_back_insert_iterator
        : detail::trans_ins_iter<
              utf_16_to_8_back_insert_iterator<Cont>,
              std::back_insert_iterator<Cont>>
    {
        constexpr utf_16_to_8_back_insert_iterator() {}
        explicit constexpr utf_16_to_8_back_insert_iterator(Cont & c) :
            detail::trans_ins_iter<
                utf_16_to_8_back_insert_iterator<Cont>,
                std::back_insert_iterator<Cont>>(
                std::back_insert_iterator<Cont>(c)),
            prev_cu_(0)
        {}

        constexpr utf_16_to_8_back_insert_iterator & operator=(char16_t cu)
        {
            auto & out = this->iter();
            out = detail::assign_16_to_8_insert(prev_cu_, cu, out);
            return *this;
        }

#ifndef BOOST_TEXT_DOXYGEN
    private:
        char16_t prev_cu_;
#endif
    };


    namespace detail {
        template<typename OutIter>
        OutIter assign_8_to_16_insert(
            unsigned char cu, char32_t & cp, int & state, OutIter out)
        {
            auto write = [&] {
                out = detail::write_cp_utf16(cp, out);
                state = invalid_table_state;
            };
            auto start_cp = [&] {
                first_cu const info = first_cus[cu];
                state = info.next;
                cp = info.initial_octet;
                if (state == bgn)
                    write();
            };
            if (state == invalid_table_state) {
                start_cp();
            } else {
                cp = (cp << 6) | (cu & 0x3f);
                char_class const class_ = octet_classes[cu];
                state = transitions[state + class_];
                if (state == bgn) {
                    write();
                } else if (state == err) {
                    out = detail::write_cp_utf16(replacement_character, out);
                    start_cp();
                }
            }
            return out;
        }
    }

    /** An out iterator that converts UTF-8 to UTF-16. */
#if BOOST_PARSER_DETAIL_TEXT_USE_CONCEPTS
    template<std::output_iterator<char16_t> Iter>
#else
    template<typename Iter>
#endif
    struct utf_8_to_16_out_iterator
        : detail::trans_ins_iter<utf_8_to_16_out_iterator<Iter>, Iter>
    {
        constexpr utf_8_to_16_out_iterator() {}
        explicit constexpr utf_8_to_16_out_iterator(Iter it) :
            detail::trans_ins_iter<utf_8_to_16_out_iterator<Iter>, Iter>(it),
            state_(detail::invalid_table_state)
        {}

        constexpr utf_8_to_16_out_iterator & operator=(char8_type cu)
        {
            auto & out = this->iter();
            out = detail::assign_8_to_16_insert(cu, cp_, state_, out);
            return *this;
        }

#ifndef BOOST_TEXT_DOXYGEN
    private:
        int state_;
        char32_t cp_;
#endif
    };

    /** An insert-iterator analogous to std::insert_iterator, that also
        converts UTF-8 to UTF-16. */
    template<typename Cont>
    struct utf_8_to_16_insert_iterator : detail::trans_ins_iter<
                                             utf_8_to_16_insert_iterator<Cont>,
                                             std::insert_iterator<Cont>>
    {
        constexpr utf_8_to_16_insert_iterator() {}
        constexpr utf_8_to_16_insert_iterator(
            Cont & c, typename Cont::iterator it) :
            detail::trans_ins_iter<
                utf_8_to_16_insert_iterator<Cont>,
                std::insert_iterator<Cont>>(std::insert_iterator<Cont>(c, it)),
            state_(detail::invalid_table_state)
        {}

        constexpr utf_8_to_16_insert_iterator & operator=(char16_t cu)
        {
            auto & out = this->iter();
            out = detail::assign_8_to_16_insert(cu, cp_, state_, out);
            return *this;
        }

#ifndef BOOST_TEXT_DOXYGEN
    private:
        int state_;
        char32_t cp_;
#endif
    };

    /** An insert-iterator analogous to std::front_insert_iterator, that also
        converts UTF-8 to UTF-16. */
    template<typename Cont>
    struct utf_8_to_16_front_insert_iterator
        : detail::trans_ins_iter<
              utf_8_to_16_front_insert_iterator<Cont>,
              std::front_insert_iterator<Cont>>
    {
        constexpr utf_8_to_16_front_insert_iterator() {}
        explicit constexpr utf_8_to_16_front_insert_iterator(Cont & c) :
            detail::trans_ins_iter<
                utf_8_to_16_front_insert_iterator<Cont>,
                std::front_insert_iterator<Cont>>(
                std::front_insert_iterator<Cont>(c)),
            state_(detail::invalid_table_state)
        {}

        constexpr utf_8_to_16_front_insert_iterator & operator=(char16_t cu)
        {
            auto & out = this->iter();
            out = detail::assign_8_to_16_insert(cu, cp_, state_, out);
            return *this;
        }

#ifndef BOOST_TEXT_DOXYGEN
    private:
        int state_;
        char32_t cp_;
#endif
    };

    /** An insert-iterator analogous to std::back_insert_iterator, that also
        converts UTF-8 to UTF-16. */
    template<typename Cont>
    struct utf_8_to_16_back_insert_iterator
        : detail::trans_ins_iter<
              utf_8_to_16_back_insert_iterator<Cont>,
              std::back_insert_iterator<Cont>>
    {
        constexpr utf_8_to_16_back_insert_iterator() {}
        explicit constexpr utf_8_to_16_back_insert_iterator(Cont & c) :
            detail::trans_ins_iter<
                utf_8_to_16_back_insert_iterator<Cont>,
                std::back_insert_iterator<Cont>>(
                std::back_insert_iterator<Cont>(c)),
            state_(detail::invalid_table_state)
        {}

        constexpr utf_8_to_16_back_insert_iterator & operator=(char16_t cu)
        {
            auto & out = this->iter();
            out = detail::assign_8_to_16_insert(cu, cp_, state_, out);
            return *this;
        }

#ifndef BOOST_TEXT_DOXYGEN
    private:
        int state_;
        char32_t cp_;
#endif
    };

}}

#include <boost/parser/detail/text/unpack.hpp>

namespace boost::parser::detail { namespace text { namespace detail {

    template<format Tag>
    struct make_utf8_dispatch;

    template<>
    struct make_utf8_dispatch<format::utf8>
    {
        template<typename Iter, typename Sentinel>
        static constexpr Iter call(Iter first, Iter it, Sentinel last)
        {
            return it;
        }
    };

    template<>
    struct make_utf8_dispatch<format::utf16>
    {
        template<typename Iter, typename Sentinel>
        static constexpr utf_iterator<
            format::utf16,
            format::utf8,
            Iter,
            Sentinel>
        call(Iter first, Iter it, Sentinel last)
        {
            return {first, it, last};
        }
    };

    template<>
    struct make_utf8_dispatch<format::utf32>
    {
        template<typename Iter, typename Sentinel>
        static constexpr utf_iterator<
            format::utf32,
            format::utf8,
            Iter,
            Sentinel>
        call(Iter first, Iter it, Sentinel last)
        {
            return {first, it, last};
        }
    };

    template<format Tag>
    struct make_utf16_dispatch;

    template<>
    struct make_utf16_dispatch<format::utf8>
    {
        template<typename Iter, typename Sentinel>
        static constexpr utf_iterator<
            format::utf8,
            format::utf16,
            Iter,
            Sentinel>
        call(Iter first, Iter it, Sentinel last)
        {
            return {first, it, last};
        }
    };

    template<>
    struct make_utf16_dispatch<format::utf16>
    {
        template<typename Iter, typename Sentinel>
        static constexpr Iter call(Iter first, Iter it, Sentinel last)
        {
            return it;
        }
    };

    template<>
    struct make_utf16_dispatch<format::utf32>
    {
        template<typename Iter, typename Sentinel>
        static constexpr utf_iterator<
            format::utf32,
            format::utf16,
            Iter,
            Sentinel>
        call(Iter first, Iter it, Sentinel last)
        {
            return {first, it, last};
        }
    };

    template<format Tag>
    struct make_utf32_dispatch;

    template<>
    struct make_utf32_dispatch<format::utf8>
    {
        template<typename Iter, typename Sentinel>
        static constexpr utf_iterator<
            format::utf8,
            format::utf32,
            Iter,
            Sentinel>
        call(Iter first, Iter it, Sentinel last)
        {
            return {first, it, last};
        }
    };

    template<>
    struct make_utf32_dispatch<format::utf16>
    {
        template<typename Iter, typename Sentinel>
        static constexpr utf_iterator<
            format::utf16,
            format::utf32,
            Iter,
            Sentinel>
        call(Iter first, Iter it, Sentinel last)
        {
            return {first, it, last};
        }
    };

    template<>
    struct make_utf32_dispatch<format::utf32>
    {
        template<typename Iter, typename Sentinel>
        static constexpr Iter call(Iter first, Iter it, Sentinel last)
        {
            return it;
        }
    };

    template<
        typename Cont,
        typename UTF8,
        typename UTF16,
        typename UTF32,
        int Bytes = sizeof(typename Cont::value_type)>
    struct from_utf8_dispatch
    {
        using type = UTF8;
    };

    template<typename Cont, typename UTF8, typename UTF16, typename UTF32>
    struct from_utf8_dispatch<Cont, UTF8, UTF16, UTF32, 2>
    {
        using type = UTF16;
    };

    template<typename Cont, typename UTF8, typename UTF16, typename UTF32>
    struct from_utf8_dispatch<Cont, UTF8, UTF16, UTF32, 4>
    {
        using type = UTF32;
    };

    template<typename Cont, typename UTF8, typename UTF16, typename UTF32>
    using from_utf8_dispatch_t =
        typename from_utf8_dispatch<Cont, UTF8, UTF16, UTF32>::type;

    template<
        typename Cont,
        typename UTF8,
        typename UTF16,
        typename UTF32,
        int Bytes = sizeof(typename Cont::value_type)>
    struct from_utf16_dispatch
    {
        using type = UTF16;
    };

    template<typename Cont, typename UTF8, typename UTF16, typename UTF32>
    struct from_utf16_dispatch<Cont, UTF8, UTF16, UTF32, 1>
    {
        using type = UTF8;
    };

    template<typename Cont, typename UTF8, typename UTF16, typename UTF32>
    struct from_utf16_dispatch<Cont, UTF8, UTF16, UTF32, 4>
    {
        using type = UTF32;
    };

    template<typename Cont, typename UTF8, typename UTF16, typename UTF32>
    using from_utf16_dispatch_t =
        typename from_utf16_dispatch<Cont, UTF8, UTF16, UTF32>::type;

    template<
        typename Cont,
        typename UTF8,
        typename UTF16,
        typename UTF32,
        int Bytes = sizeof(typename Cont::value_type)>
    struct from_utf32_dispatch
    {
        using type = UTF32;
    };

    template<typename Cont, typename UTF8, typename UTF16, typename UTF32>
    struct from_utf32_dispatch<Cont, UTF8, UTF16, UTF32, 1>
    {
        using type = UTF8;
    };

    template<typename Cont, typename UTF8, typename UTF16, typename UTF32>
    struct from_utf32_dispatch<Cont, UTF8, UTF16, UTF32, 2>
    {
        using type = UTF16;
    };

    template<typename Cont, typename UTF8, typename UTF16, typename UTF32>
    using from_utf32_dispatch_t =
        typename from_utf32_dispatch<Cont, UTF8, UTF16, UTF32>::type;

}}}

namespace boost::parser::detail { namespace text { BOOST_PARSER_DETAIL_TEXT_NAMESPACE_V1 {

#if defined(BOOST_TEXT_DOXYGEN)

    /** Returns a `utf_32_to_8_out_iterator<O>` constructed from the given
        iterator. */
    template<std::output_iterator<char8_t> O>
    utf_32_to_8_out_iterator<O> utf_32_to_8_out(O it);

    /** Returns a `utf_8_to_32_out_iterator<O>` constructed from the given
        iterator. */
    template<std::output_iterator<char32_t> O>
    utf_8_to_32_out_iterator<O> utf_8_to_32_out(O it);

    /** Returns a `utf_32_to_16_out_iterator<O>` constructed from the given
        iterator. */
    template<std::output_iterator<char16_t> O>
    utf_32_to_16_out_iterator<O> utf_32_to_16_out(O it);

    /** Returns a `utf_16_to_32_out_iterator<O>` constructed from the given
        iterator. */
    template<std::output_iterator<char32_t> O>
    utf_16_to_32_out_iterator<O> utf_16_to_32_out(O it);

    /** Returns a `utf_16_to_8_out_iterator<O>` constructed from the given
        iterator. */
    template<std::output_iterator<char8_t> O>
    utf_16_to_8_out_iterator<O> utf_16_to_8_out(O it);

    /** Returns a `utf_8_to_16_out_iterator<O>` constructed from the given
        iterator. */
    template<std::output_iterator<char16_t> O>
    utf_8_to_16_out_iterator<O> utf_8_to_16_out(O it);

    /** Returns an iterator equivalent to `it` that transcodes `[first, last)`
        to UTF-8. */
    template<std::input_iterator I, std::sentinel_for<I> S>
    auto utf8_iterator(I first, I it, S last);

    /** Returns an iterator equivalent to `it` that transcodes `[first, last)`
        to UTF-16. */
    template<std::input_iterator I, std::sentinel_for<I> S>
    auto utf16_iterator(I first, I it, S last);

    /** Returns an iterator equivalent to `it` that transcodes `[first, last)`
        to UTF-32. */
    template<std::input_iterator I, std::sentinel_for<I> S>
    auto utf32_iterator(I first, I it, S last);

    /** Returns a inserting iterator that transcodes from UTF-8 to UTF-8,
        UTF-16, or UTF-32.  Which UTF the iterator transcodes to depends on
        `sizeof(Cont::value_type)`: `1` implies UTF-8; `2` implies UTF-16; and
        any other size implies UTF-32. */
    template<typename Cont>
    requires requires { typename Cont::value_type; } &&
             std::is_integral_v<typename Cont::value_type>
    auto from_utf8_inserter(Cont & c, typename Cont::iterator it);

    /** Returns a inserting iterator that transcodes from UTF-16 to UTF-8,
        UTF-16, or UTF-32.  Which UTF the iterator transcodes to depends on
        `sizeof(Cont::value_type)`: `1` implies UTF-8; `2` implies UTF-16; and
        any other size implies UTF-32. */
    template<typename Cont>
    requires requires { typename Cont::value_type; } &&
             std::is_integral_v<typename Cont::value_type>
    auto from_utf16_inserter(Cont & c, typename Cont::iterator it);

    /** Returns a inserting iterator that transcodes from UTF-32 to UTF-8,
        UTF-16, or UTF-32.  Which UTF the iterator transcodes to depends on
        `sizeof(Cont::value_type)`: `1` implies UTF-8; `2` implies UTF-16; and
        any other size implies UTF-32. */
    template<typename Cont>
    requires requires { typename Cont::value_type; } &&
             std::is_integral_v<typename Cont::value_type>
    auto from_utf32_inserter(Cont & c, typename Cont::iterator it);

    /** Returns a back-inserting iterator that transcodes from UTF-8 to UTF-8,
        UTF-16, or UTF-32.  Which UTF the iterator transcodes to depends on
        `sizeof(Cont::value_type)`: `1` implies UTF-8; `2` implies UTF-16; and
        any other size implies UTF-32. */
    template<typename Cont>
    requires requires { typename Cont::value_type; } &&
             std::is_integral_v<typename Cont::value_type>
    auto from_utf8_back_inserter(Cont & c);

    /** Returns a back-inserting iterator that transcodes from UTF-16 to UTF-8,
        UTF-16, or UTF-32.  Which UTF the iterator transcodes to depends on
        `sizeof(Cont::value_type)`: `1` implies UTF-8; `2` implies UTF-16; and
        any other size implies UTF-32. */
    template<typename Cont>
    requires requires { typename Cont::value_type; } &&
             std::is_integral_v<typename Cont::value_type>
    auto from_utf16_back_inserter(Cont & c);

    /** Returns a back-inserting iterator that transcodes from UTF-32 to UTF-8,
        UTF-16, or UTF-32.  Which UTF the iterator transcodes to depends on
        `sizeof(Cont::value_type)`: `1` implies UTF-8; `2` implies UTF-16; and
        any other size implies UTF-32. */
    template<typename Cont>
    requires requires { typename Cont::value_type; } &&
             std::is_integral_v<typename Cont::value_type>
    auto from_utf32_back_inserter(Cont & c);

    /** Returns a front-inserting iterator that transcodes from UTF-8 to UTF-8,
        UTF-16, or UTF-32.  Which UTF the iterator transcodes to depends on
        `sizeof(Cont::value_type)`: `1` implies UTF-8; `2` implies UTF-16; and
        any other size implies UTF-32. */
    template<typename Cont>
    requires requires { typename Cont::value_type; } &&
             std::is_integral_v<typename Cont::value_type>
    auto from_utf8_front_inserter(Cont & c);

    /** Returns a front-inserting iterator that transcodes from UTF-16 to UTF-8,
        UTF-16, or UTF-32.  Which UTF the iterator transcodes to depends on
        `sizeof(Cont::value_type)`: `1` implies UTF-8; `2` implies UTF-16; and
        any other size implies UTF-32. */
    template<typename Cont>
    requires requires { typename Cont::value_type; } &&
             std::is_integral_v<typename Cont::value_type>
    auto from_utf16_front_inserter(Cont & c);

    /** Returns a front-inserting iterator that transcodes from UTF-32 to UTF-8,
        UTF-16, or UTF-32.  Which UTF the iterator transcodes to depends on
        `sizeof(Cont::value_type)`: `1` implies UTF-8; `2` implies UTF-16; and
        any other size implies UTF-32. */
    template<typename Cont>
    requires requires { typename Cont::value_type; } &&
             std::is_integral_v<typename Cont::value_type>
    auto from_utf32_front_inserter(Cont & c);

#endif

    template<typename Iter>
    utf_32_to_8_out_iterator<Iter> utf_32_to_8_out(Iter it)
    {
        return utf_32_to_8_out_iterator<Iter>(it);
    }

    template<typename Iter>
    utf_8_to_32_out_iterator<Iter> utf_8_to_32_out(Iter it)
    {
        return utf_8_to_32_out_iterator<Iter>(it);
    }

    template<typename Iter>
    utf_32_to_16_out_iterator<Iter> utf_32_to_16_out(Iter it)
    {
        return utf_32_to_16_out_iterator<Iter>(it);
    }

    template<typename Iter>
    utf_16_to_32_out_iterator<Iter> utf_16_to_32_out(Iter it)
    {
        return utf_16_to_32_out_iterator<Iter>(it);
    }

    template<typename Iter>
    utf_16_to_8_out_iterator<Iter> utf_16_to_8_out(Iter it)
    {
        return utf_16_to_8_out_iterator<Iter>(it);
    }

    template<typename Iter>
    utf_8_to_16_out_iterator<Iter> utf_8_to_16_out(Iter it)
    {
        return utf_8_to_16_out_iterator<Iter>(it);
    }

    template<typename Iter, typename Sentinel>
    auto utf8_iterator(Iter first, Iter it, Sentinel last)
    {
        auto const unpacked = text::unpack_iterator_and_sentinel(first, last);
        auto const unpacked_it =
            text::unpack_iterator_and_sentinel(it, last).first;
        constexpr format tag = unpacked.format_tag;
        return detail::make_utf8_dispatch<tag>::call(
            unpacked.first, unpacked_it, unpacked.last);
    }

    template<typename Iter, typename Sentinel>
    auto utf16_iterator(Iter first, Iter it, Sentinel last)
    {
        auto const unpacked = text::unpack_iterator_and_sentinel(first, last);
        auto const unpacked_it =
            text::unpack_iterator_and_sentinel(it, last).first;
        constexpr format tag = unpacked.format_tag;
        return detail::make_utf16_dispatch<tag>::call(
            unpacked.first, unpacked_it, unpacked.last);
    }

    template<typename Iter, typename Sentinel>
    auto utf32_iterator(Iter first, Iter it, Sentinel last)
    {
        auto const unpacked = text::unpack_iterator_and_sentinel(first, last);
        auto const unpacked_it =
            text::unpack_iterator_and_sentinel(it, last).first;
        constexpr format tag = unpacked.format_tag;
        return detail::make_utf32_dispatch<tag>::call(
            unpacked.first, unpacked_it, unpacked.last);
    }

    template<typename Cont>
    auto from_utf8_inserter(Cont & c, typename Cont::iterator it)
    {
        using result_type = detail::from_utf8_dispatch_t<
            Cont,
            std::insert_iterator<Cont>,
            utf_8_to_16_insert_iterator<Cont>,
            utf_8_to_32_insert_iterator<Cont>>;
        return result_type(c, it);
    }

    template<typename Cont>
    auto from_utf16_inserter(Cont & c, typename Cont::iterator it)
    {
        using result_type = detail::from_utf16_dispatch_t<
            Cont,
            utf_16_to_8_insert_iterator<Cont>,
            std::insert_iterator<Cont>,
            utf_16_to_32_insert_iterator<Cont>>;
        return result_type(c, it);
    }

    template<typename Cont>
    auto from_utf32_inserter(Cont & c, typename Cont::iterator it)
    {
        using result_type = detail::from_utf32_dispatch_t<
            Cont,
            utf_32_to_8_insert_iterator<Cont>,
            utf_32_to_16_insert_iterator<Cont>,
            std::insert_iterator<Cont>>;
        return result_type(c, it);
    }

    template<typename Cont>
    auto from_utf8_back_inserter(Cont & c)
    {
        using result_type = detail::from_utf8_dispatch_t<
            Cont,
            std::back_insert_iterator<Cont>,
            utf_8_to_16_back_insert_iterator<Cont>,
            utf_8_to_32_back_insert_iterator<Cont>>;
        return result_type(c);
    }

    template<typename Cont>
    auto from_utf16_back_inserter(Cont & c)
    {
        using result_type = detail::from_utf16_dispatch_t<
            Cont,
            utf_16_to_8_back_insert_iterator<Cont>,
            std::back_insert_iterator<Cont>,
            utf_16_to_32_back_insert_iterator<Cont>>;
        return result_type(c);
    }

    template<typename Cont>
    auto from_utf32_back_inserter(Cont & c)
    {
        using result_type = detail::from_utf32_dispatch_t<
            Cont,
            utf_32_to_8_back_insert_iterator<Cont>,
            utf_32_to_16_back_insert_iterator<Cont>,
            std::back_insert_iterator<Cont>>;
        return result_type(c);
    }

    template<typename Cont>
    auto from_utf8_front_inserter(Cont & c)
    {
        using result_type = detail::from_utf8_dispatch_t<
            Cont,
            std::front_insert_iterator<Cont>,
            utf_8_to_16_front_insert_iterator<Cont>,
            utf_8_to_32_front_insert_iterator<Cont>>;
        return result_type(c);
    }

    template<typename Cont>
    auto from_utf16_front_inserter(Cont & c)
    {
        using result_type = detail::from_utf16_dispatch_t<
            Cont,
            utf_16_to_8_front_insert_iterator<Cont>,
            std::front_insert_iterator<Cont>,
            utf_16_to_32_front_insert_iterator<Cont>>;
        return result_type(c);
    }

    template<typename Cont>
    auto from_utf32_front_inserter(Cont & c)
    {
        using result_type = detail::from_utf32_dispatch_t<
            Cont,
            utf_32_to_8_front_insert_iterator<Cont>,
            utf_32_to_16_front_insert_iterator<Cont>,
            std::front_insert_iterator<Cont>>;
        return result_type(c);
    }

}}}

#if BOOST_PARSER_DETAIL_TEXT_USE_CONCEPTS

namespace boost::parser::detail { namespace text { BOOST_PARSER_DETAIL_TEXT_NAMESPACE_V2 {

    template<std::output_iterator<char8_t> O>
    constexpr utf_32_to_8_out_iterator<O> utf_32_to_8_out(O it)
    {
        return utf_32_to_8_out_iterator<O>(it);
    }

    template<std::output_iterator<char32_t> O>
    constexpr utf_8_to_32_out_iterator<O> utf_8_to_32_out(O it)
    {
        return utf_8_to_32_out_iterator<O>(it);
    }

    template<std::output_iterator<char16_t> O>
    constexpr utf_32_to_16_out_iterator<O> utf_32_to_16_out(O it)
    {
        return utf_32_to_16_out_iterator<O>(it);
    }

    template<std::output_iterator<char32_t> O>
    constexpr utf_16_to_32_out_iterator<O> utf_16_to_32_out(O it)
    {
        return utf_16_to_32_out_iterator<O>(it);
    }

    template<std::output_iterator<char8_t> O>
    constexpr utf_16_to_8_out_iterator<O> utf_16_to_8_out(O it)
    {
        return utf_16_to_8_out_iterator<O>(it);
    }

    template<std::output_iterator<char16_t> O>
    constexpr utf_8_to_16_out_iterator<O> utf_8_to_16_out(O it)
    {
        return utf_8_to_16_out_iterator<O>(it);
    }

    template<std::input_iterator I, std::sentinel_for<I> S>
    constexpr auto utf8_iterator(I first, I it, S last)
    {
        return v1::utf8_iterator(first, it, last);
    }

    template<std::input_iterator I, std::sentinel_for<I> S>
    constexpr auto utf16_iterator(I first, I it, S last)
    {
        return v1::utf16_iterator(first, it, last);
    }

    template<std::input_iterator I, std::sentinel_for<I> S>
    constexpr auto utf32_iterator(I first, I it, S last)
    {
        return v1::utf32_iterator(first, it, last);
    }

    template<typename Cont>
        requires requires { typename Cont::value_type; } &&
                 utf_code_unit<typename Cont::value_type>
    constexpr auto from_utf8_inserter(Cont & c, typename Cont::iterator it)
    {
        if constexpr (sizeof(typename Cont::value_type) == 1) {
            return std::insert_iterator<Cont>(c, it);
        } else if constexpr (sizeof(typename Cont::value_type) == 2) {
            return utf_8_to_16_insert_iterator<Cont>(c, it);
        } else {
            return utf_8_to_32_insert_iterator<Cont>(c, it);
        }
    }

    template<typename Cont>
        requires requires { typename Cont::value_type; } &&
                 utf_code_unit<typename Cont::value_type>
    constexpr auto from_utf16_inserter(Cont & c, typename Cont::iterator it)
    {
        if constexpr (sizeof(typename Cont::value_type) == 1) {
            return utf_16_to_8_insert_iterator<Cont>(c, it);
        } else if constexpr (sizeof(typename Cont::value_type) == 2) {
            return std::insert_iterator<Cont>(c, it);
        } else {
            return utf_16_to_32_insert_iterator<Cont>(c, it);
        }
    }

    template<typename Cont>
        requires requires { typename Cont::value_type; } &&
                 utf_code_unit<typename Cont::value_type>
    constexpr auto from_utf32_inserter(Cont & c, typename Cont::iterator it)
    {
        if constexpr (sizeof(typename Cont::value_type) == 1) {
            return utf_32_to_8_insert_iterator<Cont>(c, it);
        } else if constexpr (sizeof(typename Cont::value_type) == 2) {
            return utf_32_to_16_insert_iterator<Cont>(c, it);
        } else {
            return std::insert_iterator<Cont>(c, it);
        }
    }

    template<typename Cont>
        requires requires { typename Cont::value_type; } &&
                 utf_code_unit<typename Cont::value_type>
    constexpr auto from_utf8_back_inserter(Cont & c)
    {
        if constexpr (sizeof(typename Cont::value_type) == 1) {
            return std::back_insert_iterator<Cont>(c);
        } else if constexpr (sizeof(typename Cont::value_type) == 2) {
            return utf_8_to_16_back_insert_iterator<Cont>(c);
        } else {
            return utf_8_to_32_back_insert_iterator<Cont>(c);
        }
    }

    template<typename Cont>
        requires requires { typename Cont::value_type; } &&
                 utf_code_unit<typename Cont::value_type>
    constexpr auto from_utf16_back_inserter(Cont & c)
    {
        if constexpr (sizeof(typename Cont::value_type) == 1) {
            return utf_16_to_8_back_insert_iterator<Cont>(c);
        } else if constexpr (sizeof(typename Cont::value_type) == 2) {
            return std::back_insert_iterator<Cont>(c);
        } else {
            return utf_16_to_32_back_insert_iterator<Cont>(c);
        }
    }

    template<typename Cont>
        requires requires { typename Cont::value_type; } &&
                 utf_code_unit<typename Cont::value_type>
    constexpr auto from_utf32_back_inserter(Cont & c)
    {
        if constexpr (sizeof(typename Cont::value_type) == 1) {
            return utf_32_to_8_back_insert_iterator<Cont>(c);
        } else if constexpr (sizeof(typename Cont::value_type) == 2) {
            return utf_32_to_16_back_insert_iterator<Cont>(c);
        } else {
            return std::back_insert_iterator<Cont>(c);
        }
    }

    template<typename Cont>
        requires requires { typename Cont::value_type; } &&
                 utf_code_unit<typename Cont::value_type>
    constexpr auto from_utf8_front_inserter(Cont & c)
    {
        if constexpr (sizeof(typename Cont::value_type) == 1) {
            return std::front_insert_iterator<Cont>(c);
        } else if constexpr (sizeof(typename Cont::value_type) == 2) {
            return utf_8_to_16_front_insert_iterator<Cont>(c);
        } else {
            return utf_8_to_32_front_insert_iterator<Cont>(c);
        }
    }

    template<typename Cont>
        requires requires { typename Cont::value_type; } &&
                 utf_code_unit<typename Cont::value_type>
    constexpr auto from_utf16_front_inserter(Cont & c)
    {
        if constexpr (sizeof(typename Cont::value_type) == 1) {
            return utf_16_to_8_front_insert_iterator<Cont>(c);
        } else if constexpr (sizeof(typename Cont::value_type) == 2) {
            return std::front_insert_iterator<Cont>(c);
        } else {
            return utf_16_to_32_front_insert_iterator<Cont>(c);
        }
    }

    template<typename Cont>
        requires requires { typename Cont::value_type; } &&
                 utf_code_unit<typename Cont::value_type>
    constexpr auto from_utf32_front_inserter(Cont & c)
    {
        if constexpr (sizeof(typename Cont::value_type) == 1) {
            return utf_32_to_8_front_insert_iterator<Cont>(c);
        } else if constexpr (sizeof(typename Cont::value_type) == 2) {
            return utf_32_to_16_front_insert_iterator<Cont>(c);
        } else {
            return std::front_insert_iterator<Cont>(c);
        }
    }
}}}

#endif

namespace boost::parser::detail { namespace text {
    namespace detail {
        template<format Format>
        constexpr auto format_to_type()
        {
            if constexpr (Format == format::utf8) {
                return char8_type{};
            } else if constexpr (Format == format::utf16) {
                return char16_t{};
            } else {
                return char32_t{};
            }
        }

        template<typename I>
        constexpr bool is_bidi =
#if BOOST_PARSER_DETAIL_TEXT_USE_CONCEPTS
            std::bidirectional_iterator<I>
#else
            std::is_base_of_v<
                std::bidirectional_iterator_tag,
                typename std::iterator_traits<I>::iterator_category>
#endif
            ;

        template<typename I, bool SupportReverse = is_bidi<I>>
        struct first_and_curr
        {
            first_and_curr() = default;
            first_and_curr(I curr) : curr{curr} {}
            first_and_curr(const first_and_curr & other) = default;
            template<
                class I2
#if !BOOST_PARSER_DETAIL_TEXT_USE_CONCEPTS
                ,
                typename Enable = std::enable_if_t<std::is_convertible_v<I2, I>>
#endif
                >
#if BOOST_PARSER_DETAIL_TEXT_USE_CONCEPTS
            requires std::convertible_to<I2, I>
#endif
            first_and_curr(const first_and_curr<I2> & other) : curr{other.curr}
            {}

            I curr;
        };
        template<typename I>
        struct first_and_curr<I, true>
        {
            first_and_curr() = default;
            first_and_curr(I first, I curr) : first{first}, curr{curr} {}
            first_and_curr(const first_and_curr & other) = default;
            template<
                class I2
#if !BOOST_PARSER_DETAIL_TEXT_USE_CONCEPTS
                ,
                typename Enable = std::enable_if_t<std::is_convertible_v<I2, I>>
#endif
                >
#if BOOST_PARSER_DETAIL_TEXT_USE_CONCEPTS
            requires std::convertible_to<I2, I>
#endif
            first_and_curr(const first_and_curr<I2> & other) :
                first{other.first}, curr{other.curr}
            {}

            I first;
            I curr;
        };
    }

    namespace detail {
        struct iter_access
        {
            template<typename T>
            static auto & buf(T & it)
            {
                return it.buf_;
            }
            template<typename T>
            static auto & first_and_curr(T & it)
            {
                return it.first_and_curr_;
            }
            template<typename T>
            static auto & buf_index(T & it)
            {
                return it.buf_index_;
            }
            template<typename T>
            static auto & buf_last(T & it)
            {
                return it.buf_last_;
            }
            template<typename T>
            static auto & to_increment(T & it)
            {
                return it.to_increment_;
            }
            template<typename T>
            static auto & last(T & it)
            {
                return it.last_;
            }
        };
    }

#if BOOST_PARSER_DETAIL_TEXT_USE_CONCEPTS
    template<
        format FromFormat,
        format ToFormat,
        std::input_iterator I,
        std::sentinel_for<I> S,
        transcoding_error_handler ErrorHandler>
        requires std::convertible_to<std::iter_value_t<I>, detail::format_to_type_t<FromFormat>>
#else
    template<
        format FromFormat,
        format ToFormat,
        typename I,
        typename S,
        typename ErrorHandler>
#endif
    class utf_iterator
        : public stl_interfaces::iterator_interface<
              utf_iterator<FromFormat, ToFormat, I, S, ErrorHandler>,
              detail::bidirectional_at_most_t<I>,
              detail::format_to_type_t<ToFormat>,
              detail::format_to_type_t<ToFormat>>
    {
        static_assert(
            FromFormat == format::utf8 || FromFormat == format::utf16 ||
            FromFormat == format::utf32);
        static_assert(
            ToFormat == format::utf8 || ToFormat == format::utf16 ||
            ToFormat == format::utf32);

#if !BOOST_PARSER_DETAIL_TEXT_USE_CONCEPTS
        template<typename T>
        constexpr static bool is_bidirectional = std::is_base_of_v<
            std::bidirectional_iterator_tag,
            detail::bidirectional_at_most_t<T>>;
        template<typename T>
        constexpr static bool is_forward = std::is_base_of_v<
            std::forward_iterator_tag,
            detail::bidirectional_at_most_t<T>>;
        template<typename T>
        constexpr static bool is_input = !is_bidirectional<T> && !is_forward<T>;
#endif

        static_assert(
#if BOOST_PARSER_DETAIL_TEXT_USE_CONCEPTS
            std::forward_iterator<I>
#else
            is_forward<I>
#endif
            || noexcept(ErrorHandler{}("")));

    public:
        using value_type = detail::format_to_type_t<ToFormat>;

        constexpr utf_iterator() = default;

#if !BOOST_PARSER_DETAIL_TEXT_USE_CONCEPTS
        template<
            typename J = I,
            typename Enable = std::enable_if_t<is_bidirectional<J>>>
#endif
        constexpr utf_iterator(I first, I it, S last)
#if BOOST_PARSER_DETAIL_TEXT_USE_CONCEPTS
            requires std::bidirectional_iterator<I>
#endif
            : first_and_curr_{first, it}, last_(last)
        {
            if (curr() != last_)
                read();
        }
#if !BOOST_PARSER_DETAIL_TEXT_USE_CONCEPTS
        template<
            typename J = I,
            typename Enable = std::enable_if_t<!is_bidirectional<J>>>
#endif
        constexpr utf_iterator(I it, S last)
#if BOOST_PARSER_DETAIL_TEXT_USE_CONCEPTS
            requires(!std::bidirectional_iterator<I>)
#endif
            :
            first_and_curr_{it}, last_(last)
        {
            if (curr() != last_)
                read();
        }

        template<
            class I2,
            class S2
#if !BOOST_PARSER_DETAIL_TEXT_USE_CONCEPTS
            ,
            typename Enable = std::enable_if_t<
                std::is_convertible_v<I2, I> && std::is_convertible_v<S2, S>>
#endif
            >
#if BOOST_PARSER_DETAIL_TEXT_USE_CONCEPTS
        requires std::convertible_to<I2, I> && std::convertible_to<S2, S>
#endif
        constexpr utf_iterator(
            utf_iterator<FromFormat, ToFormat, I2, S2, ErrorHandler> const &
                other) :
            buf_(detail::iter_access::buf(other)),
            first_and_curr_(detail::iter_access::first_and_curr(other)),
            buf_index_(detail::iter_access::buf_index(other)),
            buf_last_(detail::iter_access::buf_last(other)),
            to_increment_(detail::iter_access::to_increment(other)),
            last_(detail::iter_access::last(other))
        {}

#if !BOOST_PARSER_DETAIL_TEXT_USE_CONCEPTS
        template<
            typename J = I,
            typename Enable = std::enable_if_t<is_bidirectional<J>>>
#endif
        constexpr I begin() const
#if BOOST_PARSER_DETAIL_TEXT_USE_CONCEPTS
            requires std::bidirectional_iterator<I>
#endif
        {
            return first();
        }
        constexpr S end() const { return last_; }

#if !BOOST_PARSER_DETAIL_TEXT_USE_CONCEPTS
        template<
            typename J = I,
            typename Enable = std::enable_if_t<is_forward<J>>>
#endif
        constexpr I base() const
#if BOOST_PARSER_DETAIL_TEXT_USE_CONCEPTS
            requires std::forward_iterator<I>
#endif
        {
            return curr();
        }

        constexpr value_type operator*() const
        {
            BOOST_PARSER_DEBUG_ASSERT(buf_index_ < buf_last_);
            return buf_[buf_index_];
        }

        constexpr utf_iterator & operator++()
        {
            BOOST_PARSER_DEBUG_ASSERT(buf_index_ != buf_last_ || curr() != last_);
            if (buf_index_ + 1 == buf_last_ && curr() != last_) {
                if constexpr (
#if BOOST_PARSER_DETAIL_TEXT_USE_CONCEPTS
                    std::forward_iterator<I>
#else
                    is_forward<I>
#endif
                ) {
                    std::advance(curr(), to_increment_);
                }
                if (curr() == last_)
                    buf_index_ = 0;
                else
                    read();
            } else if (buf_index_ + 1 <= buf_last_) {
                ++buf_index_;
            }
            return *this;
        }

#if !BOOST_PARSER_DETAIL_TEXT_USE_CONCEPTS
        template<
            typename J = I,
            typename Enable = std::enable_if_t<is_bidirectional<J>>>
#endif
        constexpr utf_iterator & operator--()
#if BOOST_PARSER_DETAIL_TEXT_USE_CONCEPTS
            requires std::bidirectional_iterator<I>
#endif
        {
            BOOST_PARSER_DEBUG_ASSERT(buf_index_ || curr() != first());
            if (!buf_index_ && curr() != first())
                read_reverse();
            else if (buf_index_)
                --buf_index_;
            return *this;
        }

        friend constexpr bool operator==(
#if BOOST_PARSER_DETAIL_TEXT_USE_CONCEPTS
            utf_iterator
#else
            std::enable_if_t<is_forward<I>, utf_iterator>
#endif
            lhs, utf_iterator rhs)
#if BOOST_PARSER_DETAIL_TEXT_USE_CONCEPTS
            requires std::forward_iterator<I> || requires(I i) { i == i; }
#endif
        {
            if constexpr (
#if BOOST_PARSER_DETAIL_TEXT_USE_CONCEPTS
                std::forward_iterator<I>
#else
                is_forward<I>
#endif
            ) {
                return lhs.curr() == rhs.curr() && lhs.buf_index_ == rhs.buf_index_;
            } else {
                if (lhs.curr() != rhs.curr())
                    return false;

                if (lhs.buf_index_ == rhs.buf_index_ &&
                    lhs.buf_last_ == rhs.buf_last_) {
                    return true;
                }

                return lhs.buf_index_ == lhs.buf_last_ &&
                       rhs.buf_index_ == rhs.buf_last_;
            }
        }

#if !defined(__cpp_impl_three_way_comparison)
        friend BOOST_PARSER_CONSTEXPR bool operator!=(
            std::enable_if_t<is_forward<I>, utf_iterator> lhs, utf_iterator rhs)
        { return !(lhs == rhs); }
#endif

        friend constexpr bool operator==(utf_iterator lhs, S rhs)
        {
            if constexpr (
#if BOOST_PARSER_DETAIL_TEXT_USE_CONCEPTS
                std::forward_iterator<I>
#else
                is_forward<I>
#endif
            ) {
                return lhs.curr() == rhs;
            } else {
                return lhs.curr() == rhs && lhs.buf_index_ == lhs.buf_last_;
            }
        }

#if !defined(__cpp_impl_three_way_comparison)
        friend BOOST_PARSER_CONSTEXPR bool operator!=(utf_iterator lhs, S rhs)
        { return !(lhs == rhs); }
#endif

        // exposition only
        using base_type = stl_interfaces::iterator_interface<
            utf_iterator<FromFormat, ToFormat, I, S, ErrorHandler>,
            detail::bidirectional_at_most_t<I>,
            value_type,
            value_type>;
        using base_type::operator++;
        using base_type::operator--;

    private:
        constexpr char32_t decode_code_point()
        {
            if constexpr (FromFormat == format::utf8) {
                char32_t cp = *curr();
                ++curr();
                to_increment_ = 1;
                if (cp < 0x80)
                    return cp;

                // clang-format off

                // It turns out that this naive implementation is faster than
                // the table implementation for the converting iterators.

            /*
                Unicode 3.9/D92
                Table 3-7. Well-Formed UTF-8 Byte Sequences

                Code Points        First Byte Second Byte Third Byte Fourth Byte
                ===========        ========== =========== ========== ===========
                U+0000..U+007F     00..7F
                U+0080..U+07FF     C2..DF     80..BF
                U+0800..U+0FFF     E0         A0..BF      80..BF
                U+1000..U+CFFF     E1..EC     80..BF      80..BF
                U+D000..U+D7FF     ED         80..9F      80..BF
                U+E000..U+FFFF     EE..EF     80..BF      80..BF
                U+10000..U+3FFFF   F0         90..BF      80..BF     80..BF
                U+40000..U+FFFFF   F1..F3     80..BF      80..BF     80..BF
                U+100000..U+10FFFF F4         80..8F      80..BF     80..BF
            */
                // clang-format on

                unsigned char curr_c = (unsigned char)cp;

                auto error = [&]() {
                    return ErrorHandler{}("Ill-formed UTF-8.");
                };
                auto next = [&]() {
                    ++curr();
                    ++to_increment_;
                };

                // One-byte case handled above

                // Two-byte
                if (detail::in(0xc2, curr_c, 0xdf)) {
                    cp = curr_c & 0b00011111;
                    if (curr() == last_)
                        return error();
                    curr_c = *curr();
                    if (!detail::in(0x80, curr_c, 0xbf))
                        return error();
                    cp = (cp << 6) + (curr_c & 0b00111111);
                    next();
                    // Three-byte
                } else if (curr_c == 0xe0) {
                    cp = curr_c & 0b00001111;
                    if (curr() == last_)
                        return error();
                    curr_c = *curr();
                    if (!detail::in(0xa0, curr_c, 0xbf))
                        return error();
                    cp = (cp << 6) + (curr_c & 0b00111111);
                    next();
                    if (curr() == last_)
                        return error();
                    curr_c = *curr();
                    if (!detail::in(0x80, curr_c, 0xbf))
                        return error();
                    cp = (cp << 6) + (curr_c & 0b00111111);
                    next();
                } else if (detail::in(0xe1, curr_c, 0xec)) {
                    cp = curr_c & 0b00001111;
                    if (curr() == last_)
                        return error();
                    curr_c = *curr();
                    if (!detail::in(0x80, curr_c, 0xbf))
                        return error();
                    cp = (cp << 6) + (curr_c & 0b00111111);
                    next();
                    if (curr() == last_)
                        return error();
                    curr_c = *curr();
                    if (!detail::in(0x80, curr_c, 0xbf))
                        return error();
                    cp = (cp << 6) + (curr_c & 0b00111111);
                    next();
                } else if (curr_c == 0xed) {
                    cp = curr_c & 0b00001111;
                    if (curr() == last_)
                        return error();
                    curr_c = *curr();
                    if (!detail::in(0x80, curr_c, 0x9f))
                        return error();
                    cp = (cp << 6) + (curr_c & 0b00111111);
                    next();
                    if (curr() == last_)
                        return error();
                    curr_c = *curr();
                    if (!detail::in(0x80, curr_c, 0xbf))
                        return error();
                    cp = (cp << 6) + (curr_c & 0b00111111);
                    next();
                } else if (detail::in(0xee, curr_c, 0xef)) {
                    cp = curr_c & 0b00001111;
                    if (curr() == last_)
                        return error();
                    curr_c = *curr();
                    if (!detail::in(0x80, curr_c, 0xbf))
                        return error();
                    cp = (cp << 6) + (curr_c & 0b00111111);
                    next();
                    if (curr() == last_)
                        return error();
                    curr_c = *curr();
                    if (!detail::in(0x80, curr_c, 0xbf))
                        return error();
                    cp = (cp << 6) + (curr_c & 0b00111111);
                    next();
                    // Four-byte
                } else if (curr_c == 0xf0) {
                    cp = curr_c & 0b00000111;
                    if (curr() == last_)
                        return error();
                    curr_c = *curr();
                    if (!detail::in(0x90, curr_c, 0xbf))
                        return error();
                    cp = (cp << 6) + (curr_c & 0b00111111);
                    next();
                    if (curr() == last_)
                        return error();
                    curr_c = *curr();
                    if (!detail::in(0x80, curr_c, 0xbf))
                        return error();
                    cp = (cp << 6) + (curr_c & 0b00111111);
                    next();
                    if (curr() == last_)
                        return error();
                    curr_c = *curr();
                    if (!detail::in(0x80, curr_c, 0xbf))
                        return error();
                    cp = (cp << 6) + (curr_c & 0b00111111);
                    next();
                } else if (detail::in(0xf1, curr_c, 0xf3)) {
                    cp = curr_c & 0b00000111;
                    if (curr() == last_)
                        return error();
                    curr_c = *curr();
                    if (!detail::in(0x80, curr_c, 0xbf))
                        return error();
                    cp = (cp << 6) + (curr_c & 0b00111111);
                    next();
                    if (curr() == last_)
                        return error();
                    curr_c = *curr();
                    if (!detail::in(0x80, curr_c, 0xbf))
                        return error();
                    cp = (cp << 6) + (curr_c & 0b00111111);
                    next();
                    if (curr() == last_)
                        return error();
                    curr_c = *curr();
                    if (!detail::in(0x80, curr_c, 0xbf))
                        return error();
                    cp = (cp << 6) + (curr_c & 0b00111111);
                    next();
                } else if (curr_c == 0xf4) {
                    cp = curr_c & 0b00000111;
                    if (curr() == last_)
                        return error();
                    curr_c = *curr();
                    if (!detail::in(0x80, curr_c, 0x8f))
                        return error();
                    cp = (cp << 6) + (curr_c & 0b00111111);
                    next();
                    if (curr() == last_)
                        return error();
                    curr_c = *curr();
                    if (!detail::in(0x80, curr_c, 0xbf))
                        return error();
                    cp = (cp << 6) + (curr_c & 0b00111111);
                    next();
                    if (curr() == last_)
                        return error();
                    curr_c = *curr();
                    if (!detail::in(0x80, curr_c, 0xbf))
                        return error();
                    cp = (cp << 6) + (curr_c & 0b00111111);
                    next();
                } else {
                    return error();
                }
                return cp;
            } else if constexpr (FromFormat == format::utf16) {
                char16_t hi = *curr();
                ++curr();
                to_increment_ = 1;
                if (!boost::parser::detail::text::surrogate(hi))
                    return hi;

                if (boost::parser::detail::text::low_surrogate(hi)) {
                    return ErrorHandler{}(
                        "Invalid UTF-16 sequence; lone trailing surrogate.");
                }

                // high surrogate
                if (curr() == last_) {
                    return ErrorHandler{}(
                        "Invalid UTF-16 sequence; lone leading surrogate.");
                }

                char16_t lo = *curr();
                ++curr();
                ++to_increment_;
                if (!boost::parser::detail::text::low_surrogate(lo)) {
                    return ErrorHandler{}(
                        "Invalid UTF-16 sequence; lone leading surrogate.");
                }

                return char32_t((hi - high_surrogate_base) << 10) +
                       (lo - low_surrogate_base);
            } else {
                char32_t retval = *curr();
                ++curr();
                to_increment_ = 1;
                return retval;
            }
        }

#if !BOOST_PARSER_DETAIL_TEXT_USE_CONCEPTS
        template<
            typename J = I,
            typename Enable = std::enable_if_t<is_bidirectional<J>>>
#endif
        constexpr char32_t decode_code_point_reverse()
#if BOOST_PARSER_DETAIL_TEXT_USE_CONCEPTS
            requires std::bidirectional_iterator<I>
#endif
        {
            if constexpr (FromFormat == format::utf8) {
                curr() = detail::decrement(first(), curr());
                auto initial = curr();
                char32_t cp = decode_code_point();
                curr() = initial;
                return cp;
            } else if constexpr (FromFormat == format::utf16) {
                char16_t lo = *--curr();
                if (!boost::parser::detail::text::surrogate(lo))
                    return lo;

                if (boost::parser::detail::text::high_surrogate(lo)) {
                    return ErrorHandler{}(
                        "Invalid UTF-16 sequence; lone leading surrogate.");
                }

                // low surrogate
                if (curr() == first()) {
                    return ErrorHandler{}(
                        "Invalid UTF-16 sequence; lone trailing surrogate.");
                }

                char16_t hi = *detail::prev(curr());
                if (!boost::parser::detail::text::high_surrogate(hi)) {
                    return ErrorHandler{}(
                        "Invalid UTF-16 sequence; lone trailing surrogate.");
                }
                --curr();

                return char32_t((hi - high_surrogate_base) << 10) +
                       (lo - low_surrogate_base);
            } else {
                return *--curr();
            }
        }

        template<class Out>
        static constexpr Out encode_code_point(char32_t cp, Out out)
        {
            if constexpr (ToFormat == format::utf8) {
                if (cp < 0x80) {
                    *out++ = static_cast<char8_type>(cp);
                } else if (cp < 0x800) {
                    *out++ = static_cast<char8_type>(0xC0 + (cp >> 6));
                    *out++ = static_cast<char8_type>(0x80 + (cp & 0x3f));
                } else if (cp < 0x10000) {
                    *out++ = static_cast<char8_type>(0xe0 + (cp >> 12));
                    *out++ = static_cast<char8_type>(0x80 + ((cp >> 6) & 0x3f));
                    *out++ = static_cast<char8_type>(0x80 + (cp & 0x3f));
                } else {
                    *out++ = static_cast<char8_type>(0xf0 + (cp >> 18));
                    *out++ = static_cast<char8_type>(0x80 + ((cp >> 12) & 0x3f));
                    *out++ = static_cast<char8_type>(0x80 + ((cp >> 6) & 0x3f));
                    *out++ = static_cast<char8_type>(0x80 + (cp & 0x3f));
                }
            } else if constexpr (ToFormat == format::utf16) {
                if (cp < 0x10000) {
                    *out++ = static_cast<char16_t>(cp);
                } else {
                    *out++ =
                        static_cast<char16_t>(cp >> 10) + high_surrogate_base;
                    *out++ =
                        static_cast<char16_t>(cp & 0x3ff) + low_surrogate_base;
                }
            } else {
                *out++ = cp;
            }
            return out;
        }

        constexpr void read()
        {
            I initial;
            if constexpr (
#if BOOST_PARSER_DETAIL_TEXT_USE_CONCEPTS
                std::forward_iterator<I>
#else
                is_forward<I>
#endif
            ) {
                initial = curr();
            }
            if constexpr (noexcept(ErrorHandler{}(""))) {
                char32_t cp = decode_code_point();
                auto it = encode_code_point(cp, buf_.begin());
                buf_index_ = 0;
                buf_last_ = uint8_t(it - buf_.begin());
            } else {
#if BOOST_PARSER_DETAIL_TEXT_USE_CONCEPTS
                auto buf = buf_;
                try {
#endif
                    char32_t cp = decode_code_point();
                    auto it = encode_code_point(cp, buf_.begin());
                    buf_index_ = 0;
                    buf_last_ = it - buf_.begin();
#if BOOST_PARSER_DETAIL_TEXT_USE_CONCEPTS
                } catch (...) {
                    buf_ = buf;
                    curr() = initial;
                    throw;
                }
#endif
            }
            if constexpr (
#if BOOST_PARSER_DETAIL_TEXT_USE_CONCEPTS
                std::forward_iterator<I>
#else
            is_forward<I>
#endif
            ) {
                curr() = initial;
            }
        }

        constexpr void read_reverse()
        {
            auto initial = curr();
            if constexpr (noexcept(ErrorHandler{}(""))) {
                char32_t cp = decode_code_point_reverse();
                auto it = encode_code_point(cp, buf_.begin());
                buf_last_ = uint8_t(it - buf_.begin());
                buf_index_ = buf_last_ - 1;
                to_increment_ = (int)std::distance(curr(), initial);
            } else {
#if BOOST_PARSER_DETAIL_TEXT_USE_CONCEPTS
                auto buf = buf_;
                try {
#endif
                    char32_t cp = decode_code_point_reverse();
                    auto it = encode_code_point(cp, buf_.begin());
                    buf_last_ = it - buf_.begin();
                    buf_index_ = buf_last_ - 1;
                    to_increment_ = std::distance(curr(), initial);
#if BOOST_PARSER_DETAIL_TEXT_USE_CONCEPTS
                } catch (...) {
                    buf_ = buf;
                    curr() = initial;
                    throw;
                }
#endif
            }
        }

#if !BOOST_PARSER_DETAIL_TEXT_USE_CONCEPTS
        template<
            typename J = I,
            typename Enable = std::enable_if_t<is_bidirectional<J>>>
#endif
        constexpr I first() const
#if BOOST_PARSER_DETAIL_TEXT_USE_CONCEPTS
            requires std::bidirectional_iterator<I>
#endif
        {
            return first_and_curr_.first;
        }
        constexpr I & curr() { return first_and_curr_.curr; }
        constexpr I curr() const { return first_and_curr_.curr; }

        std::array<value_type, 4 / static_cast<int>(ToFormat)> buf_ = {};

        detail::first_and_curr<I> first_and_curr_ = {};

        uint8_t buf_index_ = 0;
        uint8_t buf_last_ = 0;
        uint8_t to_increment_ = 0;

        [[no_unique_address]] S last_ = {};

        friend struct detail::iter_access;
    };

}}

namespace boost::parser::detail { namespace text { namespace detail {

    template<class T>
    constexpr bool is_utf_iter = false;
    template<
        format FromFormat,
        format ToFormat,
        class I,
        class S,
        class ErrorHandler>
    constexpr bool
        is_utf_iter<utf_iterator<FromFormat, ToFormat, I, S, ErrorHandler>> =
            true;

    // These are here because so many downstream views that use
    // utf_iterator use them.

#if BOOST_PARSER_DETAIL_TEXT_USE_CONCEPTS

    template<typename V>
    constexpr bool common_range_v = std::ranges::common_range<V>;
    template<typename V>
    constexpr bool forward_range_v = std::ranges::forward_range<V>;
    template<typename V>
    constexpr bool bidirectional_range_v = std::ranges::bidirectional_range<V>;
    template<typename T>
    constexpr bool default_initializable_v = std::default_initializable<T>;

    template<typename V>
    constexpr bool utf32_range_v = utf32_range<V>;

#else

    template<typename T>
    using range_expr =
        decltype(detail::begin(std::declval<T &>()) == detail::end(std::declval<T &>()));
    template<typename T>
    constexpr bool is_range_v = is_detected_v<range_expr, T>;

    template<typename V>
    constexpr bool common_range_v =
        is_range_v<V> && std::is_same_v<iterator_t<V>, sentinel_t<V>>;
    template<typename V>
    constexpr bool input_range_v = is_range_v<V> && std::is_base_of_v<
        std::input_iterator_tag,
        typename std::iterator_traits<iterator_t<V>>::iterator_category>;
    template<typename V>
    constexpr bool forward_range_v = is_range_v<V> && std::is_base_of_v<
        std::forward_iterator_tag,
        typename std::iterator_traits<iterator_t<V>>::iterator_category>;
    template<typename V>
    constexpr bool bidirectional_range_v = is_range_v<V> && std::is_base_of_v<
        std::bidirectional_iterator_tag,
        typename std::iterator_traits<iterator_t<V>>::iterator_category>;
    template<typename T>
    constexpr bool default_initializable_v = std::is_default_constructible_v<T>;

    template<typename V>
    constexpr bool utf_range_v = is_range_v<V> && code_unit_v<range_value_t<V>>;

    template<typename V>
    constexpr bool
        utf32_range_v = is_range_v<V> &&
                        (
#if !defined(_MSC_VER)
                            std::is_same_v<range_value_t<V>, wchar_t> ||
#endif
                            std::is_same_v<range_value_t<V>, char32_t>);

#endif

    template<typename I>
    constexpr bool random_access_iterator_v = std::is_base_of_v<
        std::random_access_iterator_tag,
        typename std::iterator_traits<I>::iterator_category>;
    template<typename I>
    constexpr bool bidirectional_iterator_v = std::is_base_of_v<
        std::bidirectional_iterator_tag,
        typename std::iterator_traits<I>::iterator_category>;
    template<typename I>
    constexpr bool forward_iterator_v = std::is_base_of_v<
        std::forward_iterator_tag,
        typename std::iterator_traits<I>::iterator_category>;

    template<
        class V,
        bool StoreFirst = !is_utf_iter<iterator_t<V>> && common_range_v<V> &&
                          bidirectional_range_v<V>,
        bool StoreLast = !is_utf_iter<iterator_t<V>>>
    struct first_last_storage
    {
#if !BOOST_PARSER_DETAIL_TEXT_USE_CONCEPTS
        template<
            typename Enable = std::enable_if_t<
                default_initializable_v<iterator_t<V>> &&
                default_initializable_v<sentinel_t<V>>>>
#endif
        constexpr first_last_storage()
#if BOOST_PARSER_DETAIL_TEXT_USE_CONCEPTS
            requires default_initializable_v<iterator_t<V>> &&
            default_initializable_v<sentinel_t<V>>
#endif
        {}
        constexpr first_last_storage(V & base) :
            first_{detail::begin(base)}, last_{detail::end(base)}
        {}

        constexpr auto begin(iterator_t<V> & it) const { return first_; }
        constexpr auto end(iterator_t<V> & it) const { return last_; }

        iterator_t<V> first_;
        sentinel_t<V> last_;
    };

    template<typename I>
    using trinary_iter_ctor = decltype(I(
        std::declval<I>().begin(),
        std::declval<I>().end(),
        std::declval<I>().end()));

    template<class V>
    struct first_last_storage<V, true, false>
    {
#if !BOOST_PARSER_DETAIL_TEXT_USE_CONCEPTS
        template<
            typename Enable =
                std::enable_if_t<default_initializable_v<iterator_t<V>>>>
#endif
        constexpr first_last_storage()
#if BOOST_PARSER_DETAIL_TEXT_USE_CONCEPTS
            requires default_initializable_v<iterator_t<V>>
#endif
        {}
        constexpr first_last_storage(V & base) : first_{detail::begin(base)} {}

        constexpr auto begin(iterator_t<V> & it) const { return first_; }
        constexpr auto end(iterator_t<V> & it) const {
            if constexpr (
#if BOOST_PARSER_DETAIL_TEXT_USE_CONCEPTS
                requires { iterator_t<V>(it.begin(), it.end(), it.end()); }
#else
                is_detected_v<trinary_iter_ctor, iterator_t<V>>
#endif
            ) {
                return iterator_t<V>(it.begin(), it.end(), it.end());
            } else {
                return it.end();
            }
        }

        iterator_t<V> first_;
    };

    template<class V>
    struct first_last_storage<V, false, true>
    {
#if !BOOST_PARSER_DETAIL_TEXT_USE_CONCEPTS
        template<
            typename Enable =
                std::enable_if_t<default_initializable_v<sentinel_t<V>>>>
#endif
        constexpr first_last_storage()
#if BOOST_PARSER_DETAIL_TEXT_USE_CONCEPTS
            requires default_initializable_v<sentinel_t<V>>
#endif
        {}
        constexpr first_last_storage(V & base) : last_{detail::end(base)} {}

        constexpr auto begin(iterator_t<V> & it) const {
            if constexpr (is_utf_iter<iterator_t<V>>) {
                return iterator_t<V>(it.begin(), it.begin(), it.end());
            } else {
                return;
            }
        }
        constexpr auto end(iterator_t<V> & it) const { return last_; }

        sentinel_t<V> last_;
    };

    template<class V>
    struct first_last_storage<V, false, false>
    {
        constexpr first_last_storage() = default;
        constexpr first_last_storage(V & base) {}

        constexpr auto begin(iterator_t<V> & it) const {
            if constexpr (is_utf_iter<iterator_t<V>>) {
                return iterator_t<V>(it.begin(), it.begin(), it.end());
            } else {
                return;
            }
        }
        constexpr auto end(iterator_t<V> & it) const {
            if constexpr (
#if BOOST_PARSER_DETAIL_TEXT_USE_CONCEPTS
                requires { iterator_t<V>(it.begin(), it.end(), it.end()); }
#else
                is_detected_v<trinary_iter_ctor, iterator_t<V>>
#endif
            ) {
                return iterator_t<V>(it.begin(), it.end(), it.end());
            } else {
                return it.end();
            }
        }
    };


    template<class V>
    constexpr auto uc_view_category() {
        if constexpr (common_range_v<V> && bidirectional_range_v<V>) {
            return std::bidirectional_iterator_tag{};
        } else {
            return std::forward_iterator_tag{};
        }
    }

    template<class V>
    using uc_view_category_t = decltype(uc_view_category<V>());

    template<bool Const, class T>
    using maybe_const = std::conditional_t<Const, const T, T>;

    template<class T>
    constexpr bool is_empty_view = false;
#if BOOST_PARSER_DETAIL_TEXT_USE_CONCEPTS
    template<class T>
    constexpr bool is_empty_view<std::ranges::empty_view<T>> = true;
#endif

}}}

#endif
