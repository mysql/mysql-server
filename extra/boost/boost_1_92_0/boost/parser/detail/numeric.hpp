/*=============================================================================
    Copyright (c) 2001-2014 Joel de Guzman
    Copyright (c) 2001-2011 Hartmut Kaiser
    Copyright (c) 2011 Jan Frederick Eick
    Copyright (c) 2011 Christopher Jefferson
    Copyright (c) 2006 Stephen Nutt
    Copyright (c) 2019 T. Zachary Laine

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/
#ifndef BOOST_PARSER_DETAIL_NUMERIC_HPP
#define BOOST_PARSER_DETAIL_NUMERIC_HPP

#include <boost/parser/detail/text/unpack.hpp>

#if __has_include(<charconv>)
#include <charconv>
#endif

#if defined(__cpp_lib_to_chars)
#define BOOST_PARSER_HAVE_STD_CHARCONV
#define BOOST_PARSER_NUMERIC_NS std_charconv
#elif __has_include(<boost/charconv.hpp>)
#include <boost/charconv.hpp>
#define BOOST_PARSER_HAVE_BOOST_CHARCONV
#define BOOST_PARSER_NUMERIC_NS boost_charconv
#else
#define BOOST_PARSER_NUMERIC_NS spirit_parsers
#endif

#include <type_traits>
#include <cmath>

#if defined(BOOST_PARSER_HAVE_STD_CHARCONV) ||                                 \
    defined(BOOST_PARSER_HAVE_BOOST_CHARCONV)
#define BOOST_PARSER_HAVE_CHARCONV
#endif


namespace boost::parser::detail_spirit_x3 {

    struct unused_type
    {};

    // Copied from boost/spirit/home/support/char_class.hpp (Boost 1.71), and
    // modified not to use Boost.TypeTraits.

    template <typename TargetChar, typename SourceChar>
    TargetChar cast_char(SourceChar ch)
    {
        if (std::is_signed_v<TargetChar> != std::is_signed_v<SourceChar>)
        {
            if (std::is_signed<SourceChar>::value)
            {
                // source is signed, target is unsigned
                typedef std::make_unsigned_t<SourceChar> USourceChar;
                return TargetChar(USourceChar(ch));
            }
            else
            {
                // source is unsigned, target is signed
                typedef std::make_signed_t<SourceChar> SSourceChar;
                return TargetChar(SSourceChar(ch));
            }
        }
        else
        {
            // source and target has same signedness
            return TargetChar(ch); // just cast
        }
    }

    // Copied from
    // boost/spirit/home/x3/support/numeric_utils/detail/extract_int.hpp
    // (Boost 1.67), and modified not to use Boost.MPL or Boost.PP.

    inline constexpr int log2_table[] = {
        0,       0,       1000000, 1584960, 2000000, 2321920, 2584960, 2807350,
        3000000, 3169920, 3321920, 3459430, 3584960, 3700430, 3807350, 3906890,
        4000000, 4087460, 4169920, 4247920, 4321920, 4392310, 4459430, 4523560,
        4584960, 4643850, 4700430, 4754880, 4807350, 4857980, 4906890, 4954190,
        5000000, 5044390, 5087460, 5129280, 5169925};

    template<typename T, unsigned Radix>
    struct digits_traits
    {
        static_assert(std::numeric_limits<T>::radix == 2, "");
        constexpr static int value =
            int((std::numeric_limits<T>::digits * 1000000) / log2_table[Radix]);
    };

    template <typename T>
    struct digits_traits<T, 10>
    {
        static int constexpr value = std::numeric_limits<T>::digits10;
    };

    template<unsigned Radix>
    struct radix_traits
    {
        template <typename Char>
        inline static bool is_valid(Char ch)
        {
            if (Radix <= 10)
                return (ch >= '0' && ch <= static_cast<Char>('0' + Radix -1));
            return (ch >= '0' && ch <= '9')
                || (ch >= 'a' && ch <= static_cast<Char>('a' + Radix -10 -1))
                || (ch >= 'A' && ch <= static_cast<Char>('A' + Radix -10 -1));
        }

        template <typename Char>
        inline static unsigned digit(Char ch)
        {
            if (Radix <= 10 || (ch >= '0' && ch <= '9'))
                return ch - '0';
            return std::tolower(detail_spirit_x3::cast_char<char>(ch)) - 'a' + 10;
        }
    };


    template <unsigned Radix>
    struct positive_accumulator
    {
        template <typename T, typename Char>
        inline static void add(T& n, Char ch, std::false_type) // unchecked add
        {
            const int digit = radix_traits<Radix>::digit(ch);
            n = n * T(Radix) + T(digit);
        }

        template <typename T, typename Char>
        inline static bool add(T& n, Char ch, std::true_type) // checked add
        {
            // Ensure n *= Radix will not overflow
            T const max = (std::numeric_limits<T>::max)();
            T const val = max / Radix;
            if (n > val)
                return false;

            T tmp = n * Radix;

            // Ensure n += digit will not overflow
            const int digit = radix_traits<Radix>::digit(ch);
            if (tmp > max - digit)
                return false;

            n = tmp + static_cast<T>(digit);
            return true;
        }
    };

    template <unsigned Radix>
    struct negative_accumulator
    {
        template <typename T, typename Char>
        inline static void add(T& n, Char ch, std::false_type) // unchecked subtract
        {
            const int digit = radix_traits<Radix>::digit(ch);
            n = n * T(Radix) - T(digit);
        }

        template <typename T, typename Char>
        inline static bool add(T& n, Char ch, std::true_type) // checked subtract
        {
            // Ensure n *= Radix will not underflow
            T const min = (std::numeric_limits<T>::min)();
            T const val = min / T(Radix);
            if (n < val)
                return false;

            T tmp = n * Radix;

            // Ensure n -= digit will not underflow
            int const digit = radix_traits<Radix>::digit(ch);
            if (tmp < min + digit)
                return false;

            n = tmp - static_cast<T>(digit);
            return true;
        }
    };

    template <unsigned Radix, typename Accumulator, int MaxDigits>
    struct int_extractor
    {
        template <typename Char, typename T>
        inline static bool
        call(Char ch, std::size_t count, T& n, std::true_type)
        {
            std::size_t constexpr
                overflow_free = digits_traits<T, Radix>::value - 1;

            if (count < overflow_free)
            {
                Accumulator::add(n, ch, std::false_type{});
            }
            else
            {
                if (!Accumulator::add(n, ch, std::true_type{}))
                    return false; //  over/underflow!
            }
            return true;
        }

        template <typename Char, typename T>
        inline static bool
        call(Char ch, std::size_t /*count*/, T& n, std::false_type)
        {
            // no need to check for overflow
            Accumulator::add(n, ch, std::false_type{});
            return true;
        }

        template <typename Char>
        inline static bool
        call(Char /*ch*/, std::size_t /*count*/, unused_type, std::false_type)
        {
            return true;
        }

        template <typename Char, typename T>
        inline static bool
        call(Char ch, std::size_t count, T& n)
        {
            return call(ch, count, n
              , std::integral_constant<bool,
                    (   (MaxDigits < 0)
                    ||  (MaxDigits > digits_traits<T, Radix>::value)
                    )
                  && std::numeric_limits<T>::is_bounded
                >()
            );
        }
    };

    template <int MaxDigits>
    struct check_max_digits
    {
        inline static bool
        call(std::size_t count)
        {
            return count < MaxDigits; // bounded
        }
    };

    template <>
    struct check_max_digits<-1>
    {
        inline static bool
        call(std::size_t /*count*/)
        {
            return true; // unbounded
        }
    };

    template <
        typename T, unsigned Radix, unsigned MinDigits, int MaxDigits
      , typename Accumulator = positive_accumulator<Radix>
      , bool Accumulate = false
    >
    struct extract_int_impl
    {
        template <typename Iterator, typename Sentinel, typename Attribute>
        inline static bool
        parse_main(
            Iterator& first
          , Sentinel last
          , Attribute& attr)
        {
            typedef radix_traits<Radix> radix_check;
            typedef int_extractor<Radix, Accumulator, MaxDigits> extractor;
            typedef
                typename std::iterator_traits<Iterator>::value_type char_type;

            Iterator it = first;
            std::size_t leading_zeros = 0;
            if (!Accumulate)
            {
                // skip leading zeros
                while (it != last && *it == '0' && leading_zeros < MaxDigits)
                {
                    ++it;
                    ++leading_zeros;
                }
            }

            typedef Attribute attribute_type;

            attribute_type val = Accumulate ? attr : attribute_type(0);
            std::size_t count = 0;
            char_type ch;

            while (true)
            {
                if (!check_max_digits<MaxDigits>::call(count + leading_zeros) ||
                    it == last)
                    break;
                ch = *it;
                if (!radix_check::is_valid(ch) ||
                    !extractor::call(ch, count, val))
                    break;
                ++it;
                ++count;
                if (!check_max_digits<MaxDigits>::call(count + leading_zeros) ||
                    it == last)
                    break;
                ch = *it;
                if (!radix_check::is_valid(ch) ||
                    !extractor::call(ch, count, val))
                    break;
                ++it;
                ++count;
                if (!check_max_digits<MaxDigits>::call(count + leading_zeros) ||
                    it == last)
                    break;
                ch = *it;
                if (!radix_check::is_valid(ch) ||
                    !extractor::call(ch, count, val))
                    break;
                ++it;
                ++count;
            }

            if (count + leading_zeros >= MinDigits)
            {
                attr = val;
                first = it;
                return true;
            }
            return false;
        }

        template <typename Iterator, typename Sentinel>
        inline static bool
        parse(
            Iterator& first
          , Sentinel last
          , unused_type)
        {
            T n = 0; // must calculate value to detect over/underflow
            return parse_main(first, last, n);
        }

        template <typename Iterator, typename Sentinel, typename Attribute>
        inline static bool
        parse(
            Iterator& first
          , Sentinel last
          , Attribute& attr)
        {
            return parse_main(first, last, attr);
        }
    };

    template <typename T, unsigned Radix, typename Accumulator, bool Accumulate>
    struct extract_int_impl<T, Radix, 1, -1, Accumulator, Accumulate>
    {
        template <typename Iterator, typename Sentinel, typename Attribute>
        inline static bool
        parse_main(
            Iterator& first
          , Sentinel last
          , Attribute& attr)
        {
            typedef radix_traits<Radix> radix_check;
            typedef int_extractor<Radix, Accumulator, -1> extractor;
            typedef
                typename std::iterator_traits<Iterator>::value_type char_type;

            Iterator it = first;
            std::size_t count = 0;
            if (!Accumulate)
            {
                // skip leading zeros
                while (it != last && *it == '0')
                {
                    ++it;
                    ++count;
                }

                if (it == last)
                {
                    if (count == 0) // must have at least one digit
                        return false;
                    attr = 0;
                    first = it;
                    return true;
                }
            }

            typedef Attribute attribute_type;

            attribute_type val = Accumulate ? attr : attribute_type(0);
            char_type ch = *it;

            if (!radix_check::is_valid(ch) || !extractor::call(ch, 0, val))
            {
                if (count == 0) // must have at least one digit
                    return false;
                attr = val;
                first = it;
                return true;
            }

            count = 0;
            ++it;
            while (true)
            {
                if (it == last)
                    break;
                ch = *it;
                if (!radix_check::is_valid(ch))
                    break;
                if (!extractor::call(ch, count, val))
                    return false;
                ++it;
                ++count;
                if (it == last)
                    break;
                ch = *it;
                if (!radix_check::is_valid(ch))
                    break;
                if (!extractor::call(ch, count, val))
                    return false;
                ++it;
                ++count;
                if (it == last)
                    break;
                ch = *it;
                if (!radix_check::is_valid(ch))
                    break;
                if (!extractor::call(ch, count, val))
                    return false;
                ++it;
                ++count;
            }

            attr = val;
            first = it;
            return true;
        }

        template <typename Iterator, typename Sentinel>
        inline static bool
        parse(
            Iterator& first
          , Sentinel last
          , unused_type)
        {
            T n = 0; // must calculate value to detect over/underflow
            return parse_main(first, last, n);
        }

        template <typename Iterator, typename Sentinel, typename Attribute>
        inline static bool
        parse(
            Iterator& first
          , Sentinel last
          , Attribute& attr)
        {
            return parse_main(first, last, attr);
        }
    };


    // Copied from boost/spirit/home/x3/support/numeric_utils/extract_int.hpp
    // (Boost 1.67), and modified for use with iterator, sentinel pairs:

    ///////////////////////////////////////////////////////////////////////////
    //  Extract the prefix sign (- or +), return true if a '-' was found
    ///////////////////////////////////////////////////////////////////////////
    template<typename Iterator, typename Sentinel>
    inline bool extract_sign(Iterator & first, Sentinel last)
    {
        (void)last;                  // silence unused warnings
        BOOST_PARSER_DEBUG_ASSERT(first != last); // precondition

        // Extract the sign
        bool neg = *first == '-';
        if (neg || (*first == '+'))
        {
            ++first;
            return neg;
        }
        return false;
    }

    ///////////////////////////////////////////////////////////////////////////
    // Low level unsigned integer parser
    ///////////////////////////////////////////////////////////////////////////
    template <typename T, unsigned Radix, unsigned MinDigits, int MaxDigits
      , bool Accumulate = false>
    struct extract_uint
    {
        // check template parameter 'Radix' for validity
        static_assert(
            (Radix >= 2 && Radix <= 36),
            "Error Unsupported Radix");

        template <typename Iterator, typename Sentinel>
        inline static bool call(Iterator& first, Sentinel last, T& attr)
        {
            if (first == last)
                return false;

            typedef extract_int_impl<
                T
              , Radix
              , MinDigits
              , MaxDigits
              , positive_accumulator<Radix>
              , Accumulate>
            extract_type;

            Iterator save = first;
            if (!extract_type::parse(first, last, attr))
            {
                first = save;
                return false;
            }
            return true;
        }
    };

    ///////////////////////////////////////////////////////////////////////////
    // Low level signed integer parser
    ///////////////////////////////////////////////////////////////////////////
    template <typename T, unsigned Radix, unsigned MinDigits, int MaxDigits>
    struct extract_int
    {
        // check template parameter 'Radix' for validity
        static_assert(
            (Radix == 2 || Radix == 8 || Radix == 10 || Radix == 16),
            "Error Unsupported Radix");

        template <typename Iterator, typename Sentinel>
        inline static bool call(Iterator& first, Sentinel last, T& attr)
        {
            if (first == last)
                return false;

            typedef extract_int_impl<
                T, Radix, MinDigits, MaxDigits>
            extract_pos_type;

            typedef extract_int_impl<
                T, Radix, MinDigits, MaxDigits, negative_accumulator<Radix> >
            extract_neg_type;

            Iterator save = first;
            bool hit = detail_spirit_x3::extract_sign(first, last);
            if (hit)
                hit = extract_neg_type::parse(first, last, attr);
            else
                hit = extract_pos_type::parse(first, last, attr);

            if (!hit)
            {
                first = save;
                return false;
            }
            return true;
        }
    };

    // Copied from boost/spirit/home/x3/support/numeric_utils/extract_real.hpp
    // (Boost 1.71), and modified for use with iterator, sentinel pairs:

    template <typename T, typename Enable = void>
    struct pow10_helper
    {
        static T call(unsigned dim)
        {
            return std::pow(T(10), T(dim));
        }
    };

    template <typename T>
    struct pow10_table
    {
        constexpr static std::size_t size =
            std::numeric_limits<T>::max_exponent10;
    
        constexpr pow10_table()
         : exponents()
        {
            exponents[0] = T(1);
            for (auto i = 1; i != size; ++i)
                exponents[i] = exponents[i-1] * T(10);
        }
    
        T exponents[size];
    };

    template <typename T>
    struct native_pow10_helper
    {
        constexpr static auto table = pow10_table<T>();
        static T call(unsigned dim)
        {
            return table.exponents[dim];
        }
    };

    template <>
    struct pow10_helper<float>
      : native_pow10_helper<float> {};

    template <>
    struct pow10_helper<double>
      : native_pow10_helper<double> {};

    template <>
    struct pow10_helper<long double>
      : native_pow10_helper<long double> {};

    template <typename T>
    inline T pow10(unsigned dim)
    {
        return detail_spirit_x3::pow10_helper<T>::call(dim);
    }

    template<typename T>
    inline bool scale(int exp, T & n)
    {
        constexpr auto max_exp = std::numeric_limits<T>::max_exponent10;
        constexpr auto min_exp = std::numeric_limits<T>::min_exponent10;

        if (exp >= 0)
        {
            // return false if exp exceeds the max_exp
            // do this check only for primitive types!
            if (std::is_floating_point_v<T> && exp > max_exp)
                return false;
            n *= detail_spirit_x3::pow10<T>(exp);
        }
        else
        {
            if (exp < min_exp)
            {
                n /= detail_spirit_x3::pow10<T>(-min_exp);

                // return false if exp still exceeds the min_exp
                // do this check only for primitive types!
                exp += -min_exp;
                if (std::is_floating_point_v<T> && exp < min_exp)
                    return false;

                n /= detail_spirit_x3::pow10<T>(-exp);
            }
            else
            {
                n /= detail_spirit_x3::pow10<T>(-exp);
            }
        }
        return true;
    }

    template<typename T>
    bool scale(int exp, int frac, T & n)
    {
        return detail_spirit_x3::scale(exp - frac, n);
    }

    template<typename T>
    T negate(bool neg, T n)
    {
        return neg ? -n : n;
    }

    template <typename T, typename RealPolicies>
    struct extract_real
    {
        template <typename Iterator, typename Sentinel, typename Attribute>
        static bool
        parse(Iterator& first, Sentinel last, Attribute& attr,
            RealPolicies const& p)
        {
            if (first == last)
                return false;
            Iterator save = first;

            // Start by parsing the sign. neg will be true if
            // we got a "-" sign, false otherwise.
            bool neg = p.parse_sign(first, last);

            // Now attempt to parse an integer
            T n = 0;
            bool got_a_number = p.parse_n(first, last, n);

            // If we did not get a number it might be a NaN, Inf or a leading
            // dot.
            if (!got_a_number)
            {
                // Check whether the number to parse is a NaN or Inf
                if (p.parse_nan(first, last, n) ||
                    p.parse_inf(first, last, n))
                {
                    // If we got a negative sign, negate the number
                    attr = detail_spirit_x3::negate(neg, n);
                    return true;    // got a NaN or Inf, return early
                }

                // If we did not get a number and our policies do not
                // allow a leading dot, fail and return early (no-match)
                if (!p.allow_leading_dot)
                {
                    first = save;
                    return false;
                }
            }

            bool e_hit = false;
            Iterator e_pos;
            int frac_digits = 0;

            // Try to parse the dot ('.' decimal point)
            if (p.parse_dot(first, last))
            {
                // We got the decimal point. Now we will try to parse
                // the fraction if it is there. If not, it defaults
                // to zero (0) only if we already got a number.
                Iterator savef = first;
                if (p.parse_frac_n(first, last, n))
                {
                    // Optimization note: don't compute frac_digits if T is
                    // an unused_type. This should be optimized away by the compiler.
                    if (!std::is_same_v<T, unused_type>)
                        frac_digits =
                            static_cast<int>(std::distance(savef, first));
                    BOOST_PARSER_DEBUG_ASSERT(frac_digits >= 0);
                }
                else if (!got_a_number || !p.allow_trailing_dot)
                {
                    // We did not get a fraction. If we still haven't got a
                    // number and our policies do not allow a trailing dot,
                    // return no-match.
                    first = save;
                    return false;
                }

                // Now, let's see if we can parse the exponent prefix
                e_pos = first;
                e_hit = p.parse_exp(first, last);
            }
            else
            {
                // No dot and no number! Return no-match.
                if (!got_a_number)
                {
                    first = save;
                    return false;
                }

                // If we must expect a dot and we didn't see an exponent
                // prefix, return no-match.
                e_pos = first;
                e_hit = p.parse_exp(first, last);
                if (p.expect_dot && !e_hit)
                {
                    first = save;
                    return false;
                }
            }

            if (e_hit)
            {
                // We got the exponent prefix. Now we will try to parse the
                // actual exponent. It is an error if it is not there.
                int exp = 0;
                if (p.parse_exp_n(first, last, exp))
                {
                    // Got the exponent value. Scale the number by
                    // exp-frac_digits.
                    if (!detail_spirit_x3::scale(exp, frac_digits, n))
                        return false;
                }
                else
                {
                    // If there is no number, disregard the exponent altogether.
                    // by resetting 'first' prior to the exponent prefix (e|E)
                    first = e_pos;

                    // Scale the number by -frac_digits.
                    if (!detail_spirit_x3::scale(-frac_digits, n))
                        return false;
                }
            }
            else if (frac_digits)
            {
                // No exponent found. Scale the number by -frac_digits.
                if (!detail_spirit_x3::scale(-frac_digits, n))
                    return false;
            }

            // If we got a negative sign, negate the number
            attr = detail_spirit_x3::negate(neg, n);

            // Success!!!
            return true;
        }
    };

    // Copied from
    // boost/spirit/home/x3/string/detail/string_parse.hpp
    // (Boost 1.47),and modified for use with iterator, sentinel pairs:

    struct common_type_equal
    {
        template<typename T, typename U>
        bool operator()(T x, U y)
        {
            using common_t = std::common_type_t<decltype(x), decltype(y)>;
            return (common_t)x == (common_t)y;
        }
    };

    template <typename Char, typename Iterator, typename Sentinel>
    inline bool string_parse(
        Char const* uc_i, Char const* lc_i
      , Iterator& first, Sentinel const& last)
    {
        Iterator i = first;

        common_type_equal eq;

        for (; *uc_i && *lc_i; ++uc_i, ++lc_i, ++i)
            if (i == last || (!eq(*uc_i, *i) && !eq(*lc_i, *i)))
                return false;
        first = i;
        return true;
    }

    // Copied from
    // boost/spirit/home/x3/numeric/real_policies.hpp
    // (Boost 1.47),and modified for use with iterator, sentinel pairs:

    ///////////////////////////////////////////////////////////////////////////
    //  Default (unsigned) real number policies
    ///////////////////////////////////////////////////////////////////////////
    template <typename T>
    struct ureal_policies
    {
        // trailing dot policy suggested by Gustavo Guerra
        static bool const allow_leading_dot = true;
        static bool const allow_trailing_dot = true;
        static bool const expect_dot = false;

        template <typename Iterator, typename Sentinel>
        static bool
        parse_sign(Iterator& /*first*/, Iterator const& /*last*/)
        {
            return false;
        }

        template <typename Iterator, typename Sentinel, typename Attribute>
        static bool
        parse_n(Iterator& first, Sentinel const& last, Attribute& attr_)
        {
            return extract_uint<T, 10, 1, -1>::call(first, last, attr_);
        }

        template <typename Iterator, typename Sentinel>
        static bool
        parse_dot(Iterator& first, Sentinel const& last)
        {
            if (first == last || *first != '.')
                return false;
            ++first;
            return true;
        }

        template <typename Iterator, typename Sentinel, typename Attribute>
        static bool
        parse_frac_n(Iterator& first, Sentinel const& last, Attribute& attr_)
        {
            return extract_uint<T, 10, 1, -1, true>::call(first, last, attr_);
        }

        template <typename Iterator, typename Sentinel>
        static bool
        parse_exp(Iterator& first, Sentinel const& last)
        {
            if (first == last || (*first != 'e' && *first != 'E'))
                return false;
            ++first;
            return true;
        }

        template <typename Iterator, typename Sentinel>
        static bool
        parse_exp_n(Iterator& first, Sentinel const& last, int& attr_)
        {
            return extract_int<int, 10, 1, -1>::call(first, last, attr_);
        }

        ///////////////////////////////////////////////////////////////////////
        //  The parse_nan() and parse_inf() functions get called whenever
        //  a number to parse does not start with a digit (after having
        //  successfully parsed an optional sign).
        //
        //  The functions should return true if a Nan or Inf has been found. In
        //  this case the attr should be set to the matched value (NaN or
        //  Inf). The optional sign will be automatically applied afterwards.
        //
        //  The default implementation below recognizes representations of NaN
        //  and Inf as mandated by the C99 Standard and as proposed for
        //  inclusion into the C++0x Standard: nan, nan(...), inf and infinity
        //  (the matching is performed case-insensitively).
        ///////////////////////////////////////////////////////////////////////
        template <typename Iterator, typename Sentinel, typename Attribute>
        static bool
        parse_nan(Iterator& first, Sentinel const& last, Attribute& attr_)
        {
            if (first == last)
                return false;   // end of input reached

            if (*first != 'n' && *first != 'N')
                return false;   // not "nan"

            // nan[(...)] ?
            if (detail_spirit_x3::string_parse("nan", "NAN", first, last))
            {
                if (first != last && *first == '(')
                {
                    // skip trailing (...) part
                    Iterator i = first;

                    while (++i != last && *i != ')')
                        ;
                    if (i == last)
                        return false;     // no trailing ')' found, give up

                    first = ++i;
                }
                attr_ = std::numeric_limits<T>::quiet_NaN();
                return true;
            }
            return false;
        }

        template <typename Iterator, typename Sentinel, typename Attribute>
        static bool
        parse_inf(Iterator& first, Sentinel const& last, Attribute& attr_)
        {
            if (first == last)
                return false;   // end of input reached

            if (*first != 'i' && *first != 'I')
                return false;   // not "inf"

            // inf or infinity ?
            if (detail_spirit_x3::string_parse("inf", "INF", first, last))
            {
                // skip allowed 'inity' part of infinity
                detail_spirit_x3::string_parse("inity", "INITY", first, last);
                attr_ = std::numeric_limits<T>::infinity();
                return true;
            }
            return false;
        }
    };

    ///////////////////////////////////////////////////////////////////////////
    //  Default (signed) real number policies
    ///////////////////////////////////////////////////////////////////////////
    template <typename T>
    struct real_policies : ureal_policies<T>
    {
        template <typename Iterator, typename Sentinel>
        static bool
        parse_sign(Iterator& first, Sentinel const& last)
        {
            return detail_spirit_x3::extract_sign(first, last);
        }
    };

    template <typename T>
    struct strict_ureal_policies : ureal_policies<T>
    {
        static bool const expect_dot = true;
    };

    template <typename T>
    struct strict_real_policies : real_policies<T>
    {
        static bool const expect_dot = true;
    };
}

namespace boost::parser::detail::numeric {

    template<typename I, typename S>
    constexpr bool common_range = std::is_same_v<I, S>;

    template<typename I, typename S>
    using unpacked_iter = decltype(text::unpack_iterator_and_sentinel(
                                       std::declval<I>(), std::declval<S>())
                                       .first);

    template<typename I, typename S>
    constexpr bool unpacks_to_chars =
        std::is_pointer_v<unpacked_iter<I, S>> && std::is_same_v<
            std::remove_cv_t<std::remove_reference_t<
                std::remove_pointer_t<unpacked_iter<I, S>>>>,
            char>;

    inline namespace BOOST_PARSER_NUMERIC_NS {

        template<int MinDigits, int MaxDigits, typename I, typename S>
#if defined(BOOST_PARSER_HAVE_CHARCONV)
        constexpr bool use_charconv_int =
            MinDigits == 1 && MaxDigits == -1 &&
            common_range<I, S> && unpacks_to_chars<I, S>;
#else
        constexpr bool use_charconv_int = false;
#endif

        template<
            bool Signed,
            int Radix,
            int MinDigits,
            int MaxDigits,
            typename I,
            typename S,
            typename T>
        bool parse_int(I & first, S last, T & attr)
        {
            if constexpr (use_charconv_int<MinDigits, MaxDigits, I, S>) {
#if defined(BOOST_PARSER_HAVE_CHARCONV)
                auto unpacked = text::unpack_iterator_and_sentinel(first, last);
#if defined(BOOST_PARSER_HAVE_STD_CHARCONV)
                std::from_chars_result const result = std::from_chars(
#else
                charconv::from_chars_result const result = charconv::from_chars(
#endif
                    unpacked.first, unpacked.last, attr, Radix);
                if (result.ec == std::errc()) {
                    first = unpacked.repack(result.ptr);
                    return true;
                }
                return false;
#endif
            } else if constexpr (Signed) {
                using extract = detail_spirit_x3::
                    extract_int<T, Radix, MinDigits, MaxDigits>;
                return extract::call(first, last, attr);
            } else {
                using extract = detail_spirit_x3::
                    extract_uint<T, Radix, MinDigits, MaxDigits>;
                return extract::call(first, last, attr);
            }
        }

        template<typename I, typename S>
#if defined(BOOST_PARSER_HAVE_CHARCONV)
        constexpr bool use_charconv_real =
            common_range<I, S> && unpacks_to_chars<I, S>;
#else
        constexpr bool use_charconv_real = false;
#endif

        template<typename I, typename S, typename T>
        bool parse_real(I & first, S last, T & attr)
        {
            if constexpr (use_charconv_real<I, S>) {
#if defined(BOOST_PARSER_HAVE_CHARCONV)
                auto unpacked = text::unpack_iterator_and_sentinel(first, last);
#if defined(BOOST_PARSER_HAVE_STD_CHARCONV)
                std::from_chars_result const result = std::from_chars(
#else
                charconv::from_chars_result const result = charconv::from_chars(
#endif
                    unpacked.first, unpacked.last, attr);
                if (result.ec == std::errc()) {
                    first = unpacked.repack(result.ptr);
                    return true;
                }
                return false;
#endif
            } else {
                detail_spirit_x3::real_policies<T> policies;
                using extract = detail_spirit_x3::
                    extract_real<T, detail_spirit_x3::real_policies<T>>;
                return extract::parse(first, last, attr, policies);
            }
        }
    }
}

#endif
