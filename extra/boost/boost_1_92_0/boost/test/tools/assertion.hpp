//  (C) Copyright Gennadiy Rozental 2001.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.
//
//!@file
//!@brief Defines framework for automated assertion construction
// ***************************************************************************

#ifndef BOOST_TEST_TOOLS_ASSERTION_HPP_100911GER
#define BOOST_TEST_TOOLS_ASSERTION_HPP_100911GER

// Boost.Test
#include <boost/test/tools/assertion_result.hpp>
#include <boost/test/tools/detail/print_helper.hpp>
#include <boost/test/tools/detail/fwd.hpp>

// Boost
#include <boost/type.hpp>
#include <boost/type_traits/decay.hpp>
#include <boost/mpl/assert.hpp>
#include <boost/utility/declval.hpp>
#include <boost/type_traits/remove_reference.hpp>
#include <boost/type_traits/remove_const.hpp>

// STL
#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES
#include <utility>
#endif

// GCC < 10 workaround for std::optional comparison ambiguity (see below)
#if ((defined(__GNUC__) && !defined(__clang__) && (__GNUC__ < 10)) || (defined(__clang__) && __clang_major__ < 11)) && \
    (__cplusplus >= 201703L || (defined(_MSVC_LANG) && _MSVC_LANG >= 201703L))
#include <optional>
#endif

#include <boost/test/detail/suppress_warnings.hpp>

//____________________________________________________________________________//

namespace boost {
namespace test_tools {
namespace assertion {

// ************************************************************************** //
// **************         assertion::is_std_optional            ************** //
// ************************************************************************** //
// Trait to detect std::optional. Used for GCC < 10 workaround.

template<typename T>
struct is_std_optional : std::false_type {};

#if ((defined(__GNUC__) && !defined(__clang__) && (__GNUC__ < 10)) || (defined(__clang__) && __clang_major__ < 11)) && \
    (__cplusplus >= 201703L || (defined(_MSVC_LANG) && _MSVC_LANG >= 201703L))

template<typename T>
struct is_std_optional<std::optional<T>> : std::true_type {};
template<typename T>
struct is_std_optional<std::optional<T>&> : std::true_type {};
template<typename T>
struct is_std_optional<std::optional<T> const&> : std::true_type {};
template<typename T>
struct is_std_optional<std::optional<T>&&> : std::true_type {};

#endif

// ************************************************************************** //
// **************             assertion::operators             ************** //
// ************************************************************************** //
// precedence 4: ->*, .*
// precedence 5: *, /, %
// precedence 6: +, -
// precedence 7: << , >>
// precedence 8: <, <=, > and >=
// precedence 9: == and !=
// precedence 10: bitwise AND
// precedence 11: bitwise XOR
// precedence 12: bitwise OR
// precedence 13: logical AND
//  disabled
// precedence 14: logical OR
//  disabled
// precedence 15: ternary conditional
//  disabled
// precedence 16: = and OP= operators
// precedence 17: throw operator
//  not supported
// precedence 18: comma
//  not supported

namespace op {

#define BOOST_TEST_FOR_EACH_COMP_OP(action) \
    action( < , LT, >=, GE )                \
    action( <=, LE, > , GT )                \
    action( > , GT, <=, LE )                \
    action( >=, GE, < , LT )                \
    action( ==, EQ, !=, NE )                \
    action( !=, NE, ==, EQ )                \
/**/

//____________________________________________________________________________//

#ifndef BOOST_NO_CXX11_DECLTYPE

// Non-comparison operators (never need SFINAE workaround)
#define BOOST_TEST_FOR_EACH_NONCOMP_OP(action)\
    action(->*, MEMP, ->*, MEMP )           \
                                            \
    action( * , MUL , *  , MUL  )           \
    action( / , DIV , /  , DIV  )           \
    action( % , MOD , %  , MOD  )           \
                                            \
    action( + , ADD , +  , ADD  )           \
    action( - , SUB , -  , SUB  )           \
                                            \
    action( <<, LSH , << , LSH  )           \
    action( >>, RSH , >> , RSH  )           \
                                            \
    action( & , BAND, &  , BAND )           \
    action( ^ , XOR , ^  , XOR  )           \
    action( | , BOR , |  , BOR  )           \
/**/

#define BOOST_TEST_FOR_EACH_CONST_OP(action)\
    BOOST_TEST_FOR_EACH_NONCOMP_OP(action)  \
    BOOST_TEST_FOR_EACH_COMP_OP(action)     \
/**/

#else

#define BOOST_TEST_FOR_EACH_NONCOMP_OP(action)

#define BOOST_TEST_FOR_EACH_CONST_OP(action)\
    BOOST_TEST_FOR_EACH_COMP_OP(action)     \
/**/

#endif

//____________________________________________________________________________//

#define BOOST_TEST_FOR_EACH_MUT_OP(action)  \
    action( = , SET , =  , SET  )           \
    action( +=, IADD, += , IADD )           \
    action( -=, ISUB, -= , ISUB )           \
    action( *=, IMUL, *= , IMUL )           \
    action( /=, IDIV, /= , IDIV )           \
    action( %=, IMOD, %= , IMOD )           \
    action(<<=, ILSH, <<=, ILSH )           \
    action(>>=, IRSH, >>=, IRSH )           \
    action( &=, IAND, &= , IAND )           \
    action( ^=, IXOR, ^= , IXOR )           \
    action( |=, IOR , |= , IOR  )           \
/**/

//____________________________________________________________________________//

#ifndef BOOST_NO_CXX11_DECLTYPE
#   define DEDUCE_RESULT_TYPE( oper )                                   \
    decltype(boost::declval<Lhs>() oper boost::declval<Rhs>() ) optype; \
    typedef typename boost::remove_reference<optype>::type              \
/**/
#else
#   define DEDUCE_RESULT_TYPE( oper ) bool
#endif

#define DEFINE_CONST_OPER_FWD_DECL( oper, name, rev, name_inverse ) \
template<typename Lhs, typename Rhs,                \
         typename Enabler=void>                     \
struct name;                                        \
/**/

BOOST_TEST_FOR_EACH_CONST_OP( DEFINE_CONST_OPER_FWD_DECL )

#define DEFINE_CONST_OPER( oper, name, rev, name_inverse ) \
template<typename Lhs, typename Rhs,                \
         typename Enabler>                          \
struct name {                                       \
    typedef DEDUCE_RESULT_TYPE( oper ) result_type; \
    typedef name_inverse<Lhs, Rhs> inverse;         \
                                                    \
    static result_type                              \
    eval( Lhs const& lhs, Rhs const& rhs )          \
    {                                               \
        return lhs oper rhs;                        \
    }                                               \
                                                    \
    template<typename PrevExprType>                 \
    static void                                     \
    report( std::ostream&       ostr,               \
            PrevExprType const& lhs,                \
            Rhs const&          rhs)                \
    {                                               \
        lhs.report( ostr );                         \
        ostr << revert()                            \
             << tt_detail::print_helper( rhs );     \
    }                                               \
                                                    \
    static char const* forward()                    \
    { return " " #oper " "; }                       \
    static char const* revert()                     \
    { return " " #rev " "; }                        \
};                                                  \
/**/

BOOST_TEST_FOR_EACH_CONST_OP( DEFINE_CONST_OPER )

#undef DEDUCE_RESULT_TYPE
#undef DEFINE_CONST_OPER

//____________________________________________________________________________//

} // namespace op

// ************************************************************************** //
// **************    assertion::optional_friends_base          ************** //
// ************************************************************************** //
// GCC < 10 workaround: Base class that conditionally provides hidden friend
// comparison operators only when ValType is std::optional. Hidden friends are
// found via ADL and as non-templates beat std::optional's template operators.
// See: https://github.com/boostorg/test/issues/475

template<typename Lhs, typename Rhs, typename OP> class binary_expr;

#if ((defined(__GNUC__) && !defined(__clang__) && (__GNUC__ < 10)) || (defined(__clang__) && __clang_major__ < 11)) && \
    (__cplusplus >= 201703L || (defined(_MSVC_LANG) && _MSVC_LANG >= 201703L))

// Primary template - empty (for non-optional types)
template<typename ExprType, typename ValType, bool IsOptional = is_std_optional<ValType>::value>
struct optional_friends_base {};

// Specialization for std::optional types - provides hidden friend operators
template<typename ExprType, typename ValType>
struct optional_friends_base<ExprType, ValType, true> {
    friend binary_expr<ExprType, ValType, op::EQ<ValType, ValType>>
    operator==( ExprType lhs, ValType rhs )
    {
        return binary_expr<ExprType, ValType, op::EQ<ValType, ValType>>(
            std::move(lhs), std::move(rhs));
    }

    friend binary_expr<ExprType, ValType, op::NE<ValType, ValType>>
    operator!=( ExprType lhs, ValType rhs )
    {
        return binary_expr<ExprType, ValType, op::NE<ValType, ValType>>(
            std::move(lhs), std::move(rhs));
    }

    friend binary_expr<ExprType, ValType, op::LT<ValType, ValType>>
    operator<( ExprType lhs, ValType rhs )
    {
        return binary_expr<ExprType, ValType, op::LT<ValType, ValType>>(
            std::move(lhs), std::move(rhs));
    }

    friend binary_expr<ExprType, ValType, op::LE<ValType, ValType>>
    operator<=( ExprType lhs, ValType rhs )
    {
        return binary_expr<ExprType, ValType, op::LE<ValType, ValType>>(
            std::move(lhs), std::move(rhs));
    }

    friend binary_expr<ExprType, ValType, op::GT<ValType, ValType>>
    operator>( ExprType lhs, ValType rhs )
    {
        return binary_expr<ExprType, ValType, op::GT<ValType, ValType>>(
            std::move(lhs), std::move(rhs));
    }

    friend binary_expr<ExprType, ValType, op::GE<ValType, ValType>>
    operator>=( ExprType lhs, ValType rhs )
    {
        return binary_expr<ExprType, ValType, op::GE<ValType, ValType>>(
            std::move(lhs), std::move(rhs));
    }
};

#else // Not GCC < 10
template<typename ExprType, typename ValType, bool IsOptional = false>
struct optional_friends_base {};
#endif // GCC < 10 && C++17

// ************************************************************************** //
// **************          assertion::expression_base          ************** //
// ************************************************************************** //
// Defines expression operators

template<typename ExprType,typename ValType>
class expression_base : public optional_friends_base<ExprType, ValType> {
public:

#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES
    template<typename T>
    struct RhsT : remove_const<typename remove_reference<T>::type> {};

// Regular operator support (non-comparison operators)
#define ADD_OP_SUPPORT( oper, name, _, _i )                     \
    template<typename T>                                        \
    binary_expr<ExprType,T,                                     \
        op::name<ValType,typename RhsT<T>::type> >              \
    operator oper( T&& rhs )                                    \
    {                                                           \
        return binary_expr<ExprType,T,                          \
         op::name<ValType,typename RhsT<T>::type> >             \
            ( std::forward<ExprType>(                           \
                *static_cast<ExprType*>(this) ),                \
              std::forward<T>(rhs) );                           \
    }                                                           \
/**/

// GCC < 10 workaround: comparison operators with SFINAE to exclude std::optional
// when ValType is also std::optional. The hidden friend operators in the base class
// optional_friends_base will handle the optional==optional case instead.
#if ((defined(__GNUC__) && !defined(__clang__) && (__GNUC__ < 10)) || (defined(__clang__) && __clang_major__ < 11)) && \
    (__cplusplus >= 201703L || (defined(_MSVC_LANG) && _MSVC_LANG >= 201703L))
#define ADD_OP_SUPPORT_COMP( oper, name, _, _i )                              \
    template<typename T,                                                      \
             typename std::enable_if<                                         \
                 !is_std_optional<ValType>::value ||                          \
                 !is_std_optional<typename RhsT<T>::type>::value, int>::type = 0> \
    binary_expr<ExprType,T,                                                   \
        op::name<ValType,typename RhsT<T>::type> >                            \
    operator oper( T&& rhs )                                                  \
    {                                                                         \
        return binary_expr<ExprType,T,                                        \
         op::name<ValType,typename RhsT<T>::type> >                           \
            ( std::forward<ExprType>(                                         \
                *static_cast<ExprType*>(this) ),                              \
              std::forward<T>(rhs) );                                         \
    }                                                                         \
/**/
#else
#define ADD_OP_SUPPORT_COMP ADD_OP_SUPPORT
#endif

#else // BOOST_NO_CXX11_RVALUE_REFERENCES

#define ADD_OP_SUPPORT( oper, name, _, _i )                     \
    template<typename T>                                        \
    binary_expr<ExprType,typename boost::decay<T const>::type,  \
        op::name<ValType,typename boost::decay<T const>::type> >\
    operator oper( T const& rhs ) const                         \
    {                                                           \
        typedef typename boost::decay<T const>::type Rhs;       \
        return binary_expr<ExprType,Rhs,op::name<ValType,Rhs> > \
            ( *static_cast<ExprType const*>(this),              \
              rhs );                                            \
    }                                                           \
/**/
#define ADD_OP_SUPPORT_COMP ADD_OP_SUPPORT

#endif // BOOST_NO_CXX11_RVALUE_REFERENCES

    BOOST_TEST_FOR_EACH_NONCOMP_OP( ADD_OP_SUPPORT )
    BOOST_TEST_FOR_EACH_COMP_OP( ADD_OP_SUPPORT_COMP )
    #undef ADD_OP_SUPPORT
    #undef ADD_OP_SUPPORT_COMP

#ifndef BOOST_NO_CXX11_AUTO_DECLARATIONS
    // Disabled operators
    template<typename T>
    ExprType&
    operator ||( T const& /*rhs*/ )
    {
        BOOST_MPL_ASSERT_MSG(false, CANT_USE_LOGICAL_OPERATOR_OR_WITHIN_THIS_TESTING_TOOL, () );

        return *static_cast<ExprType*>(this);
    }

    template<typename T>
    ExprType&
    operator &&( T const& /*rhs*/ )
    {
        BOOST_MPL_ASSERT_MSG(false, CANT_USE_LOGICAL_OPERATOR_AND_WITHIN_THIS_TESTING_TOOL, () );

        return *static_cast<ExprType*>(this);
    }

    operator bool()
    {
        BOOST_MPL_ASSERT_MSG(false, CANT_USE_TERNARY_OPERATOR_WITHIN_THIS_TESTING_TOOL, () );

        return false;
    }
#endif
};

// ************************************************************************** //
// **************            assertion::value_expr             ************** //
// ************************************************************************** //
// simple value expression

template<typename T>
class value_expr : public expression_base<value_expr<T>,typename remove_const<typename remove_reference<T>::type>::type> {
public:
    // Public types
    typedef T                   result_type;

    // Constructor
#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES
    value_expr( value_expr&& ve )
    : m_value( std::forward<T>(ve.m_value) )
    {}
    explicit                    value_expr( T&& val )
    : m_value( std::forward<T>(val) )
    {}
#else
    explicit                    value_expr( T const& val )
    : m_value( val )
    {}
#endif

    // Specific expression interface
    T const&                    value() const
    {
        return m_value;
    }
    void                        report( std::ostream& ostr ) const
    {
        ostr << tt_detail::print_helper( value() );
    }

    // Mutating operators
#define ADD_OP_SUPPORT( OPER, ID, _, _i)\
    template<typename U>                \
    value_expr<T>&                      \
    operator OPER( U const& rhs )       \
    {                                   \
        m_value OPER rhs;               \
                                        \
        return *this;                   \
    }                                   \
/**/

    BOOST_TEST_FOR_EACH_MUT_OP( ADD_OP_SUPPORT )
#undef ADD_OP_SUPPORT

    // expression interface
    assertion_result            evaluate( bool no_message = false ) const
    {
        assertion_result res( value() );
        if( no_message || res )
            return res;

        format_message( res.message(), value() );

        return tt_detail::format_assertion_result( "", res.message().str() );
    }

private:
    template<typename U>
    static void format_message( wrap_stringstream& ostr, U const& v )
    {
        ostr << "['" << tt_detail::print_helper(v) << "' evaluates to false]";
    }
    static void format_message( wrap_stringstream& /*ostr*/, bool /*v*/ ) {}
    static void format_message( wrap_stringstream& /*ostr*/, assertion_result const& /*v*/ ) {}

    // Data members
    T                           m_value;
};

// ************************************************************************** //
// **************            assertion::binary_expr            ************** //
// ************************************************************************** //
// binary expression

template<typename LExpr, typename Rhs, typename OP>
class binary_expr : public expression_base<binary_expr<LExpr,Rhs,OP>,typename OP::result_type> {
public:
    // Public types
    typedef typename OP::result_type result_type;

    // Constructor
#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES
    binary_expr( binary_expr&& be )
    : m_lhs( std::forward<LExpr>(be.m_lhs) )
    , m_rhs( std::forward<Rhs>(be.m_rhs) )
    {}
    binary_expr( LExpr&& lhs, Rhs&& rhs )
    : m_lhs( std::forward<LExpr>(lhs) )
    , m_rhs( std::forward<Rhs>(rhs) )
    {}
#else
    binary_expr( LExpr const& lhs, Rhs const& rhs )
    : m_lhs( lhs )
    , m_rhs( rhs )
    {}
#endif

    // Specific expression interface
    result_type                 value() const
    {
        return OP::eval( m_lhs.value(), m_rhs );
    }
    void                        report( std::ostream& ostr ) const
    {
        return OP::report( ostr, m_lhs, m_rhs );
    }

    assertion_result            evaluate( bool no_message = false ) const
    {
        assertion_result const expr_res( value() );
        if( no_message || expr_res )
            return expr_res;

        wrap_stringstream buff;
        report( buff.stream() );

        return tt_detail::format_assertion_result( buff.stream().str(), expr_res.message() );
    }

    // To support custom manipulators
    LExpr const&                lhs() const     { return m_lhs; }
    Rhs const&                  rhs() const     { return m_rhs; }
private:
    // Data members
    LExpr                       m_lhs;
    Rhs                         m_rhs;
};

// ************************************************************************** //
// **************               assertion::seed                ************** //
// ************************************************************************** //
// seed added ot the input expression to form an assertion expression

class seed {
public:
    // ->* is highest precedence left to right operator
    template<typename T>
    value_expr<T>
#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES
    operator->*( T&& v ) const
    {
        return value_expr<T>( std::forward<T>( v ) );
    }
#else
    operator->*( T const& v )  const
    {
        return value_expr<T>( v );
    }
#endif
};

#undef BOOST_TEST_FOR_EACH_CONST_OP

} // namespace assertion
} // namespace test_tools
} // namespace boost

#include <boost/test/detail/enable_warnings.hpp>

#endif // BOOST_TEST_TOOLS_ASSERTION_HPP_100911GER
