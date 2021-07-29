// A Bison parser, made by GNU Bison 3.0.4.

// Skeleton implementation for Bison LALR(1) parsers in C++

// Copyright (C) 2002-2015 Free Software Foundation, Inc.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

// As a special exception, you may create a larger work that contains
// part or all of the Bison parser skeleton and distribute that work
// under terms of your choice, so long as that work isn't itself a
// parser generator using the skeleton or a modified version thereof
// as a parser skeleton.  Alternatively, if you modify or redistribute
// the parser skeleton itself, you may (at your option) remove this
// special exception, which will cause the skeleton and the resulting
// Bison output files to be licensed under the GNU General Public
// License without this special exception.

// This special exception was added by the Free Software Foundation in
// version 2.2 of Bison.
// //                    "%code top" blocks.
#line 6 "whereParser.yy" // lalr1.cc:397

/** \file Defines the parser for the where clause accepted by FastBit IBIS.
    The definitions are processed through bison.
*/

#include <iostream>

#line 42 "whereParser.cc" // lalr1.cc:397


// First part of user declarations.

#line 47 "whereParser.cc" // lalr1.cc:404

# ifndef YY_NULLPTR
#  if defined __cplusplus && 201103L <= __cplusplus
#   define YY_NULLPTR nullptr
#  else
#   define YY_NULLPTR 0
#  endif
# endif

#include "whereParser.hh"

// User implementation prologue.
#line 106 "whereParser.yy" // lalr1.cc:412

#include "whereLexer.h"

#undef yylex
#define yylex driver.lexer->lex

#line 67 "whereParser.cc" // lalr1.cc:412


#ifndef YY_
# if defined YYENABLE_NLS && YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> // FIXME: INFRINGES ON USER NAME SPACE.
#   define YY_(msgid) dgettext ("bison-runtime", msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(msgid) msgid
# endif
#endif

#define YYRHSLOC(Rhs, K) ((Rhs)[K].location)
/* YYLLOC_DEFAULT -- Set CURRENT to span from RHS[1] to RHS[N].
   If N is 0, then set CURRENT to the empty location which ends
   the previous symbol: RHS[0] (always defined).  */

# ifndef YYLLOC_DEFAULT
#  define YYLLOC_DEFAULT(Current, Rhs, N)                               \
    do                                                                  \
      if (N)                                                            \
        {                                                               \
          (Current).begin  = YYRHSLOC (Rhs, 1).begin;                   \
          (Current).end    = YYRHSLOC (Rhs, N).end;                     \
        }                                                               \
      else                                                              \
        {                                                               \
          (Current).begin = (Current).end = YYRHSLOC (Rhs, 0).end;      \
        }                                                               \
    while (/*CONSTCOND*/ false)
# endif


// Suppress unused-variable warnings by "using" E.
#define YYUSE(E) ((void) (E))

// Enable debugging if requested.
#if YYDEBUG

// A pseudo ostream that takes yydebug_ into account.
# define YYCDEBUG if (yydebug_) (*yycdebug_)

# define YY_SYMBOL_PRINT(Title, Symbol)         \
  do {                                          \
    if (yydebug_)                               \
    {                                           \
      *yycdebug_ << Title << ' ';               \
      yy_print_ (*yycdebug_, Symbol);           \
      *yycdebug_ << std::endl;                  \
    }                                           \
  } while (false)

# define YY_REDUCE_PRINT(Rule)          \
  do {                                  \
    if (yydebug_)                       \
      yy_reduce_print_ (Rule);          \
  } while (false)

# define YY_STACK_PRINT()               \
  do {                                  \
    if (yydebug_)                       \
      yystack_print_ ();                \
  } while (false)

#else // !YYDEBUG

# define YYCDEBUG if (false) std::cerr
# define YY_SYMBOL_PRINT(Title, Symbol)  YYUSE(Symbol)
# define YY_REDUCE_PRINT(Rule)           static_cast<void>(0)
# define YY_STACK_PRINT()                static_cast<void>(0)

#endif // !YYDEBUG

#define yyerrok         (yyerrstatus_ = 0)
#define yyclearin       (yyla.clear ())

#define YYACCEPT        goto yyacceptlab
#define YYABORT         goto yyabortlab
#define YYERROR         goto yyerrorlab
#define YYRECOVERING()  (!!yyerrstatus_)

#line 25 "whereParser.yy" // lalr1.cc:479
namespace ibis {
#line 153 "whereParser.cc" // lalr1.cc:479

  /* Return YYSTR after stripping away unnecessary quotes and
     backslashes, so that it's suitable for yyerror.  The heuristic is
     that double-quoting is unnecessary unless the string contains an
     apostrophe, a comma, or backslash (other than backslash-backslash).
     YYSTR is taken from yytname.  */
  std::string
  whereParser::yytnamerr_ (const char *yystr)
  {
    if (*yystr == '"')
      {
        std::string yyr = "";
        char const *yyp = yystr;

        for (;;)
          switch (*++yyp)
            {
            case '\'':
            case ',':
              goto do_not_strip_quotes;

            case '\\':
              if (*++yyp != '\\')
                goto do_not_strip_quotes;
              // Fall through.
            default:
              yyr += *yyp;
              break;

            case '"':
              return yyr;
            }
      do_not_strip_quotes: ;
      }

    return yystr;
  }


  /// Build a parser object.
  whereParser::whereParser (class ibis::whereClause& driver_yyarg)
    :
#if YYDEBUG
      yydebug_ (false),
      yycdebug_ (&std::cerr),
#endif
      driver (driver_yyarg)
  {}

  whereParser::~whereParser ()
  {}


  /*---------------.
  | Symbol types.  |
  `---------------*/

  inline
  whereParser::syntax_error::syntax_error (const location_type& l, const std::string& m)
    : std::runtime_error (m)
    , location (l)
  {}

  // basic_symbol.
  template <typename Base>
  inline
  whereParser::basic_symbol<Base>::basic_symbol ()
    : value ()
  {}

  template <typename Base>
  inline
  whereParser::basic_symbol<Base>::basic_symbol (const basic_symbol& other)
    : Base (other)
    , value ()
    , location (other.location)
  {
    value = other.value;
  }


  template <typename Base>
  inline
  whereParser::basic_symbol<Base>::basic_symbol (typename Base::kind_type t, const semantic_type& v, const location_type& l)
    : Base (t)
    , value (v)
    , location (l)
  {}


  /// Constructor for valueless symbols.
  template <typename Base>
  inline
  whereParser::basic_symbol<Base>::basic_symbol (typename Base::kind_type t, const location_type& l)
    : Base (t)
    , value ()
    , location (l)
  {}

  template <typename Base>
  inline
  whereParser::basic_symbol<Base>::~basic_symbol ()
  {
    clear ();
  }

  template <typename Base>
  inline
  void
  whereParser::basic_symbol<Base>::clear ()
  {
    Base::clear ();
  }

  template <typename Base>
  inline
  bool
  whereParser::basic_symbol<Base>::empty () const
  {
    return Base::type_get () == empty_symbol;
  }

  template <typename Base>
  inline
  void
  whereParser::basic_symbol<Base>::move (basic_symbol& s)
  {
    super_type::move(s);
    value = s.value;
    location = s.location;
  }

  // by_type.
  inline
  whereParser::by_type::by_type ()
    : type (empty_symbol)
  {}

  inline
  whereParser::by_type::by_type (const by_type& other)
    : type (other.type)
  {}

  inline
  whereParser::by_type::by_type (token_type t)
    : type (yytranslate_ (t))
  {}

  inline
  void
  whereParser::by_type::clear ()
  {
    type = empty_symbol;
  }

  inline
  void
  whereParser::by_type::move (by_type& that)
  {
    type = that.type;
    that.clear ();
  }

  inline
  int
  whereParser::by_type::type_get () const
  {
    return type;
  }


  // by_state.
  inline
  whereParser::by_state::by_state ()
    : state (empty_state)
  {}

  inline
  whereParser::by_state::by_state (const by_state& other)
    : state (other.state)
  {}

  inline
  void
  whereParser::by_state::clear ()
  {
    state = empty_state;
  }

  inline
  void
  whereParser::by_state::move (by_state& that)
  {
    state = that.state;
    that.clear ();
  }

  inline
  whereParser::by_state::by_state (state_type s)
    : state (s)
  {}

  inline
  whereParser::symbol_number_type
  whereParser::by_state::type_get () const
  {
    if (state == empty_state)
      return empty_symbol;
    else
      return yystos_[state];
  }

  inline
  whereParser::stack_symbol_type::stack_symbol_type ()
  {}


  inline
  whereParser::stack_symbol_type::stack_symbol_type (state_type s, symbol_type& that)
    : super_type (s, that.location)
  {
    value = that.value;
    // that is emptied.
    that.type = empty_symbol;
  }

  inline
  whereParser::stack_symbol_type&
  whereParser::stack_symbol_type::operator= (const stack_symbol_type& that)
  {
    state = that.state;
    value = that.value;
    location = that.location;
    return *this;
  }


  template <typename Base>
  inline
  void
  whereParser::yy_destroy_ (const char* yymsg, basic_symbol<Base>& yysym) const
  {
    if (yymsg)
      YY_SYMBOL_PRINT (yymsg, yysym);

    // User destructor.
    switch (yysym.type_get ())
    {
            case 38: // "signed integer sequence"

#line 103 "whereParser.yy" // lalr1.cc:614
        { delete (yysym.value.stringVal); }
#line 406 "whereParser.cc" // lalr1.cc:614
        break;

      case 39: // "unsigned integer sequence"

#line 103 "whereParser.yy" // lalr1.cc:614
        { delete (yysym.value.stringVal); }
#line 413 "whereParser.cc" // lalr1.cc:614
        break;

      case 40: // "name string"

#line 103 "whereParser.yy" // lalr1.cc:614
        { delete (yysym.value.stringVal); }
#line 420 "whereParser.cc" // lalr1.cc:614
        break;

      case 41: // "number sequence"

#line 103 "whereParser.yy" // lalr1.cc:614
        { delete (yysym.value.stringVal); }
#line 427 "whereParser.cc" // lalr1.cc:614
        break;

      case 42: // "string sequence"

#line 103 "whereParser.yy" // lalr1.cc:614
        { delete (yysym.value.stringVal); }
#line 434 "whereParser.cc" // lalr1.cc:614
        break;

      case 43: // "string literal"

#line 103 "whereParser.yy" // lalr1.cc:614
        { delete (yysym.value.stringVal); }
#line 441 "whereParser.cc" // lalr1.cc:614
        break;

      case 50: // qexpr

#line 104 "whereParser.yy" // lalr1.cc:614
        { delete (yysym.value.whereNode); }
#line 448 "whereParser.cc" // lalr1.cc:614
        break;

      case 51: // simpleRange

#line 104 "whereParser.yy" // lalr1.cc:614
        { delete (yysym.value.whereNode); }
#line 455 "whereParser.cc" // lalr1.cc:614
        break;

      case 52: // compRange2

#line 104 "whereParser.yy" // lalr1.cc:614
        { delete (yysym.value.whereNode); }
#line 462 "whereParser.cc" // lalr1.cc:614
        break;

      case 53: // compRange3

#line 104 "whereParser.yy" // lalr1.cc:614
        { delete (yysym.value.whereNode); }
#line 469 "whereParser.cc" // lalr1.cc:614
        break;

      case 54: // mathExpr

#line 104 "whereParser.yy" // lalr1.cc:614
        { delete (yysym.value.whereNode); }
#line 476 "whereParser.cc" // lalr1.cc:614
        break;


      default:
        break;
    }
  }

#if YYDEBUG
  template <typename Base>
  void
  whereParser::yy_print_ (std::ostream& yyo,
                                     const basic_symbol<Base>& yysym) const
  {
    std::ostream& yyoutput = yyo;
    YYUSE (yyoutput);
    symbol_number_type yytype = yysym.type_get ();
    // Avoid a (spurious) G++ 4.8 warning about "array subscript is
    // below array bounds".
    if (yysym.empty ())
      std::abort ();
    yyo << (yytype < yyntokens_ ? "token" : "nterm")
        << ' ' << yytname_[yytype] << " ("
        << yysym.location << ": ";
    YYUSE (yytype);
    yyo << ')';
  }
#endif

  inline
  void
  whereParser::yypush_ (const char* m, state_type s, symbol_type& sym)
  {
    stack_symbol_type t (s, sym);
    yypush_ (m, t);
  }

  inline
  void
  whereParser::yypush_ (const char* m, stack_symbol_type& s)
  {
    if (m)
      YY_SYMBOL_PRINT (m, s);
    yystack_.push (s);
  }

  inline
  void
  whereParser::yypop_ (unsigned int n)
  {
    yystack_.pop (n);
  }

#if YYDEBUG
  std::ostream&
  whereParser::debug_stream () const
  {
    return *yycdebug_;
  }

  void
  whereParser::set_debug_stream (std::ostream& o)
  {
    yycdebug_ = &o;
  }


  whereParser::debug_level_type
  whereParser::debug_level () const
  {
    return yydebug_;
  }

  void
  whereParser::set_debug_level (debug_level_type l)
  {
    yydebug_ = l;
  }
#endif // YYDEBUG

  inline whereParser::state_type
  whereParser::yy_lr_goto_state_ (state_type yystate, int yysym)
  {
    int yyr = yypgoto_[yysym - yyntokens_] + yystate;
    if (0 <= yyr && yyr <= yylast_ && yycheck_[yyr] == yystate)
      return yytable_[yyr];
    else
      return yydefgoto_[yysym - yyntokens_];
  }

  inline bool
  whereParser::yy_pact_value_is_default_ (int yyvalue)
  {
    return yyvalue == yypact_ninf_;
  }

  inline bool
  whereParser::yy_table_value_is_error_ (int yyvalue)
  {
    return yyvalue == yytable_ninf_;
  }

  int
  whereParser::parse ()
  {
    // State.
    int yyn;
    /// Length of the RHS of the rule being reduced.
    int yylen = 0;

    // Error handling.
    int yynerrs_ = 0;
    int yyerrstatus_ = 0;

    /// The lookahead symbol.
    symbol_type yyla;

    /// The locations where the error started and ended.
    stack_symbol_type yyerror_range[3];

    /// The return value of parse ().
    int yyresult;

    // FIXME: This shoud be completely indented.  It is not yet to
    // avoid gratuitous conflicts when merging into the master branch.
    try
      {
    YYCDEBUG << "Starting parse" << std::endl;


    // User initialization code.
    #line 30 "whereParser.yy" // lalr1.cc:741
{ // initialize location object
    yyla.location.begin.filename = yyla.location.end.filename = &(driver.clause_);
}

#line 613 "whereParser.cc" // lalr1.cc:741

    /* Initialize the stack.  The initial state will be set in
       yynewstate, since the latter expects the semantical and the
       location values to have been already stored, initialize these
       stacks with a primary value.  */
    yystack_.clear ();
    yypush_ (YY_NULLPTR, 0, yyla);

    // A new symbol was pushed on the stack.
  yynewstate:
    YYCDEBUG << "Entering state " << yystack_[0].state << std::endl;

    // Accept?
    if (yystack_[0].state == yyfinal_)
      goto yyacceptlab;

    goto yybackup;

    // Backup.
  yybackup:

    // Try to take a decision without lookahead.
    yyn = yypact_[yystack_[0].state];
    if (yy_pact_value_is_default_ (yyn))
      goto yydefault;

    // Read a lookahead token.
    if (yyla.empty ())
      {
        YYCDEBUG << "Reading a token: ";
        try
          {
            yyla.type = yytranslate_ (yylex (&yyla.value, &yyla.location));
          }
        catch (const syntax_error& yyexc)
          {
            error (yyexc);
            goto yyerrlab1;
          }
      }
    YY_SYMBOL_PRINT ("Next token is", yyla);

    /* If the proper action on seeing token YYLA.TYPE is to reduce or
       to detect an error, take that action.  */
    yyn += yyla.type_get ();
    if (yyn < 0 || yylast_ < yyn || yycheck_[yyn] != yyla.type_get ())
      goto yydefault;

    // Reduce or error.
    yyn = yytable_[yyn];
    if (yyn <= 0)
      {
        if (yy_table_value_is_error_ (yyn))
          goto yyerrlab;
        yyn = -yyn;
        goto yyreduce;
      }

    // Count tokens shifted since error; after three, turn off error status.
    if (yyerrstatus_)
      --yyerrstatus_;

    // Shift the lookahead token.
    yypush_ ("Shifting", yyn, yyla);
    goto yynewstate;

  /*-----------------------------------------------------------.
  | yydefault -- do the default action for the current state.  |
  `-----------------------------------------------------------*/
  yydefault:
    yyn = yydefact_[yystack_[0].state];
    if (yyn == 0)
      goto yyerrlab;
    goto yyreduce;

  /*-----------------------------.
  | yyreduce -- Do a reduction.  |
  `-----------------------------*/
  yyreduce:
    yylen = yyr2_[yyn];
    {
      stack_symbol_type yylhs;
      yylhs.state = yy_lr_goto_state_(yystack_[yylen].state, yyr1_[yyn]);
      /* If YYLEN is nonzero, implement the default value of the
         action: '$$ = $1'.  Otherwise, use the top of the stack.

         Otherwise, the following line sets YYLHS.VALUE to garbage.
         This behavior is undocumented and Bison users should not rely
         upon it.  */
      if (yylen)
        yylhs.value = yystack_[yylen - 1].value;
      else
        yylhs.value = yystack_[0].value;

      // Compute the default @$.
      {
        slice<stack_symbol_type, stack_type> slice (yystack_, yylen);
        YYLLOC_DEFAULT (yylhs.location, slice, yylen);
      }

      // Perform the reduction.
      YY_REDUCE_PRINT (yyn);
      try
        {
          switch (yyn)
            {
  case 2:
#line 115 "whereParser.yy" // lalr1.cc:859
    {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *(yystack_[2].value.whereNode)
	<< " || " << *(yystack_[0].value.whereNode);
#endif
    (yylhs.value.whereNode) = new ibis::qExpr(ibis::qExpr::LOGICAL_OR);
    (yylhs.value.whereNode)->setRight((yystack_[0].value.whereNode));
    (yylhs.value.whereNode)->setLeft((yystack_[2].value.whereNode));
}
#line 732 "whereParser.cc" // lalr1.cc:859
    break;

  case 3:
#line 125 "whereParser.yy" // lalr1.cc:859
    {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *(yystack_[2].value.whereNode)
	<< " ^ " << *(yystack_[0].value.whereNode);
#endif
    (yylhs.value.whereNode) = new ibis::qExpr(ibis::qExpr::LOGICAL_XOR);
    (yylhs.value.whereNode)->setRight((yystack_[0].value.whereNode));
    (yylhs.value.whereNode)->setLeft((yystack_[2].value.whereNode));
}
#line 747 "whereParser.cc" // lalr1.cc:859
    break;

  case 4:
#line 135 "whereParser.yy" // lalr1.cc:859
    {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *(yystack_[2].value.whereNode)
	<< " && " << *(yystack_[0].value.whereNode);
#endif
    (yylhs.value.whereNode) = new ibis::qExpr(ibis::qExpr::LOGICAL_AND);
    (yylhs.value.whereNode)->setRight((yystack_[0].value.whereNode));
    (yylhs.value.whereNode)->setLeft((yystack_[2].value.whereNode));
}
#line 762 "whereParser.cc" // lalr1.cc:859
    break;

  case 5:
#line 145 "whereParser.yy" // lalr1.cc:859
    {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *(yystack_[2].value.whereNode)
	<< " &~ " << *(yystack_[0].value.whereNode);
#endif
    (yylhs.value.whereNode) = new ibis::qExpr(ibis::qExpr::LOGICAL_MINUS);
    (yylhs.value.whereNode)->setRight((yystack_[0].value.whereNode));
    (yylhs.value.whereNode)->setLeft((yystack_[2].value.whereNode));
}
#line 777 "whereParser.cc" // lalr1.cc:859
    break;

  case 6:
#line 155 "whereParser.yy" // lalr1.cc:859
    {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- ! " << *(yystack_[0].value.whereNode);
#endif
    (yylhs.value.whereNode) = new ibis::qExpr(ibis::qExpr::LOGICAL_NOT);
    (yylhs.value.whereNode)->setLeft((yystack_[0].value.whereNode));
}
#line 790 "whereParser.cc" // lalr1.cc:859
    break;

  case 7:
#line 163 "whereParser.yy" // lalr1.cc:859
    {
    (yylhs.value.whereNode) = (yystack_[1].value.whereNode);
}
#line 798 "whereParser.cc" // lalr1.cc:859
    break;

  case 11:
#line 172 "whereParser.yy" // lalr1.cc:859
    {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- EXISTS(" << *(yystack_[0].value.stringVal) << ')';
#endif
    (yylhs.value.whereNode) = new ibis::qExists((yystack_[0].value.stringVal)->c_str());
    delete (yystack_[0].value.stringVal);
}
#line 811 "whereParser.cc" // lalr1.cc:859
    break;

  case 12:
#line 180 "whereParser.yy" // lalr1.cc:859
    {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- EXISTS(" << *(yystack_[0].value.stringVal) << ')';
#endif
    (yylhs.value.whereNode) = new ibis::qExists((yystack_[0].value.stringVal)->c_str());
    delete (yystack_[0].value.stringVal);
}
#line 824 "whereParser.cc" // lalr1.cc:859
    break;

  case 13:
#line 188 "whereParser.yy" // lalr1.cc:859
    {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- EXISTS(" << *(yystack_[1].value.stringVal) << ')';
#endif
    (yylhs.value.whereNode) = new ibis::qExists((yystack_[1].value.stringVal)->c_str());
    delete (yystack_[1].value.stringVal);
}
#line 837 "whereParser.cc" // lalr1.cc:859
    break;

  case 14:
#line 196 "whereParser.yy" // lalr1.cc:859
    {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- EXISTS(" << *(yystack_[1].value.stringVal) << ')';
#endif
    (yylhs.value.whereNode) = new ibis::qExists((yystack_[1].value.stringVal)->c_str());
    delete (yystack_[1].value.stringVal);
}
#line 850 "whereParser.cc" // lalr1.cc:859
    break;

  case 15:
#line 204 "whereParser.yy" // lalr1.cc:859
    {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *(yystack_[2].value.stringVal) << " IN ("
	<< *(yystack_[0].value.stringVal) << ")";
#endif
    (yylhs.value.whereNode) = new ibis::qDiscreteRange((yystack_[2].value.stringVal)->c_str(), (yystack_[0].value.stringVal)->c_str());
    delete (yystack_[0].value.stringVal);
    delete (yystack_[2].value.stringVal);
}
#line 865 "whereParser.cc" // lalr1.cc:859
    break;

  case 16:
#line 214 "whereParser.yy" // lalr1.cc:859
    {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *(yystack_[6].value.stringVal) << " IN ("
	<< (yystack_[3].value.doubleVal) << ", " << (yystack_[1].value.doubleVal) << ")";
#endif
    std::vector<double> vals(2);
    vals[0] = (yystack_[3].value.doubleVal);
    vals[1] = (yystack_[1].value.doubleVal);
    (yylhs.value.whereNode) = new ibis::qDiscreteRange((yystack_[6].value.stringVal)->c_str(), vals);
    delete (yystack_[6].value.stringVal);
}
#line 882 "whereParser.cc" // lalr1.cc:859
    break;

  case 17:
#line 226 "whereParser.yy" // lalr1.cc:859
    {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *(yystack_[4].value.stringVal) << " IN ("
	<< (yystack_[1].value.doubleVal) << ")";
#endif
    (yylhs.value.whereNode) = new ibis::qContinuousRange((yystack_[4].value.stringVal)->c_str(), ibis::qExpr::OP_EQ, (yystack_[1].value.doubleVal));
    delete (yystack_[4].value.stringVal);
}
#line 896 "whereParser.cc" // lalr1.cc:859
    break;

  case 18:
#line 235 "whereParser.yy" // lalr1.cc:859
    {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *(yystack_[2].value.stringVal) << " NOT NULL";
#endif
    (yylhs.value.whereNode) = new ibis::qContinuousRange((yystack_[2].value.stringVal)->c_str(), ibis::qExpr::OP_UNDEFINED, 0U);
}
#line 908 "whereParser.cc" // lalr1.cc:859
    break;

  case 19:
#line 242 "whereParser.yy" // lalr1.cc:859
    {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *(yystack_[3].value.stringVal) << " NOT IN ("
	<< *(yystack_[0].value.stringVal) << ")";
#endif
    (yylhs.value.whereNode) = new ibis::qExpr(ibis::qExpr::LOGICAL_NOT);
    (yylhs.value.whereNode)->setLeft(new ibis::qDiscreteRange((yystack_[3].value.stringVal)->c_str(), (yystack_[0].value.stringVal)->c_str()));
    delete (yystack_[0].value.stringVal);
    delete (yystack_[3].value.stringVal);
}
#line 924 "whereParser.cc" // lalr1.cc:859
    break;

  case 20:
#line 253 "whereParser.yy" // lalr1.cc:859
    {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *(yystack_[7].value.stringVal) << " NOT IN ("
	<< (yystack_[3].value.doubleVal) << ", " << (yystack_[1].value.doubleVal) << ")";
#endif
    std::vector<double> vals(2);
    vals[0] = (yystack_[3].value.doubleVal);
    vals[1] = (yystack_[1].value.doubleVal);
    (yylhs.value.whereNode) = new ibis::qExpr(ibis::qExpr::LOGICAL_NOT);
    (yylhs.value.whereNode)->setLeft(new ibis::qDiscreteRange((yystack_[7].value.stringVal)->c_str(), vals));
    delete (yystack_[7].value.stringVal);
}
#line 942 "whereParser.cc" // lalr1.cc:859
    break;

  case 21:
#line 266 "whereParser.yy" // lalr1.cc:859
    {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *(yystack_[5].value.stringVal) << " NOT IN ("
	<< (yystack_[1].value.doubleVal) << ")";
#endif
    (yylhs.value.whereNode) = new ibis::qExpr(ibis::qExpr::LOGICAL_NOT);
    (yylhs.value.whereNode)->setLeft(new ibis::qContinuousRange((yystack_[5].value.stringVal)->c_str(), ibis::qExpr::OP_EQ, (yystack_[1].value.doubleVal)));
    delete (yystack_[5].value.stringVal);
}
#line 957 "whereParser.cc" // lalr1.cc:859
    break;

  case 22:
#line 276 "whereParser.yy" // lalr1.cc:859
    {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *(yystack_[2].value.stringVal) << " IN ("
	<< *(yystack_[0].value.stringVal) << ")";
#endif
    (yylhs.value.whereNode) = new ibis::qAnyString((yystack_[2].value.stringVal)->c_str(), (yystack_[0].value.stringVal)->c_str());
    delete (yystack_[0].value.stringVal);
    delete (yystack_[2].value.stringVal);
}
#line 972 "whereParser.cc" // lalr1.cc:859
    break;

  case 23:
#line 286 "whereParser.yy" // lalr1.cc:859
    {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *(yystack_[6].value.stringVal) << " IN ("
	<< *(yystack_[3].value.stringVal) << ", " << *(yystack_[1].value.stringVal) << ")";
#endif
    std::string val;
    val = '"'; /* add quote to keep strings intact */
    val += *(yystack_[3].value.stringVal);
    val += "\", \"";
    val += *(yystack_[1].value.stringVal);
    val += '"';
    (yylhs.value.whereNode) = new ibis::qAnyString((yystack_[6].value.stringVal)->c_str(), val.c_str());
    delete (yystack_[1].value.stringVal);
    delete (yystack_[3].value.stringVal);
    delete (yystack_[6].value.stringVal);
}
#line 994 "whereParser.cc" // lalr1.cc:859
    break;

  case 24:
#line 303 "whereParser.yy" // lalr1.cc:859
    {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *(yystack_[6].value.stringVal) << " IN ("
	<< *(yystack_[3].value.stringVal) << ", " << *(yystack_[1].value.stringVal) << ")";
#endif
    std::string val;
    val = '"'; /* add quote to keep strings intact */
    val += *(yystack_[3].value.stringVal);
    val += "\", \"";
    val += *(yystack_[1].value.stringVal);
    val += '"';
    (yylhs.value.whereNode) = new ibis::qAnyString((yystack_[6].value.stringVal)->c_str(), val.c_str());
    delete (yystack_[1].value.stringVal);
    delete (yystack_[3].value.stringVal);
    delete (yystack_[6].value.stringVal);
}
#line 1016 "whereParser.cc" // lalr1.cc:859
    break;

  case 25:
#line 320 "whereParser.yy" // lalr1.cc:859
    {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *(yystack_[6].value.stringVal) << " IN ("
	<< *(yystack_[3].value.stringVal) << ", " << *(yystack_[1].value.stringVal) << ")";
#endif
    std::string val;
    val = '"'; /* add quote to keep strings intact */
    val += *(yystack_[3].value.stringVal);
    val += "\", \"";
    val += *(yystack_[1].value.stringVal);
    val += '"';
    (yylhs.value.whereNode) = new ibis::qAnyString((yystack_[6].value.stringVal)->c_str(), val.c_str());
    delete (yystack_[1].value.stringVal);
    delete (yystack_[3].value.stringVal);
    delete (yystack_[6].value.stringVal);
}
#line 1038 "whereParser.cc" // lalr1.cc:859
    break;

  case 26:
#line 337 "whereParser.yy" // lalr1.cc:859
    {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *(yystack_[6].value.stringVal) << " IN ("
	<< *(yystack_[3].value.stringVal) << ", " << *(yystack_[1].value.stringVal) << ")";
#endif
    std::string val;
    val = '"'; /* add quote to keep strings intact */
    val += *(yystack_[3].value.stringVal);
    val += "\", \"";
    val += *(yystack_[1].value.stringVal);
    val += '"';
    (yylhs.value.whereNode) = new ibis::qAnyString((yystack_[6].value.stringVal)->c_str(), val.c_str());
    delete (yystack_[1].value.stringVal);
    delete (yystack_[3].value.stringVal);
    delete (yystack_[6].value.stringVal);
}
#line 1060 "whereParser.cc" // lalr1.cc:859
    break;

  case 27:
#line 354 "whereParser.yy" // lalr1.cc:859
    {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *(yystack_[4].value.stringVal) << " IN ("
	<< *(yystack_[1].value.stringVal) << ")";
#endif
    std::string val;
    val = '"'; /* add quote to keep strings intact */
    val += *(yystack_[1].value.stringVal);
    val += '"';
    (yylhs.value.whereNode) = new ibis::qAnyString((yystack_[4].value.stringVal)->c_str(), val.c_str());
    delete (yystack_[1].value.stringVal);
    delete (yystack_[4].value.stringVal);
}
#line 1079 "whereParser.cc" // lalr1.cc:859
    break;

  case 28:
#line 368 "whereParser.yy" // lalr1.cc:859
    {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *(yystack_[4].value.stringVal) << " IN ("
	<< *(yystack_[1].value.stringVal) << ")";
#endif
    std::string val;
    val = '"'; /* add quote to keep strings intact */
    val += *(yystack_[1].value.stringVal);
    val += '"';
    (yylhs.value.whereNode) = new ibis::qAnyString((yystack_[4].value.stringVal)->c_str(), val.c_str());
    delete (yystack_[1].value.stringVal);
    delete (yystack_[4].value.stringVal);
}
#line 1098 "whereParser.cc" // lalr1.cc:859
    break;

  case 29:
#line 382 "whereParser.yy" // lalr1.cc:859
    {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *(yystack_[2].value.stringVal) << " LIKE "
	<< *(yystack_[0].value.stringVal);
#endif
    (yylhs.value.whereNode) = new ibis::qLike((yystack_[2].value.stringVal)->c_str(), (yystack_[0].value.stringVal)->c_str());
    delete (yystack_[0].value.stringVal);
    delete (yystack_[2].value.stringVal);
}
#line 1113 "whereParser.cc" // lalr1.cc:859
    break;

  case 30:
#line 392 "whereParser.yy" // lalr1.cc:859
    {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *(yystack_[2].value.stringVal) << " LIKE "
	<< *(yystack_[0].value.stringVal);
#endif
    (yylhs.value.whereNode) = new ibis::qLike((yystack_[2].value.stringVal)->c_str(), (yystack_[0].value.stringVal)->c_str());
    delete (yystack_[0].value.stringVal);
    delete (yystack_[2].value.stringVal);
}
#line 1128 "whereParser.cc" // lalr1.cc:859
    break;

  case 31:
#line 402 "whereParser.yy" // lalr1.cc:859
    {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *(yystack_[3].value.stringVal) << " NOT IN ("
	<< *(yystack_[0].value.stringVal) << ")";
#endif
    (yylhs.value.whereNode) = new ibis::qExpr(ibis::qExpr::LOGICAL_NOT);
    (yylhs.value.whereNode)->setLeft(new ibis::qAnyString((yystack_[3].value.stringVal)->c_str(), (yystack_[0].value.stringVal)->c_str()));
    delete (yystack_[0].value.stringVal);
    delete (yystack_[3].value.stringVal);
}
#line 1144 "whereParser.cc" // lalr1.cc:859
    break;

  case 32:
#line 413 "whereParser.yy" // lalr1.cc:859
    {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *(yystack_[7].value.stringVal) << " NOT IN ("
	<< *(yystack_[3].value.stringVal) << ", " << *(yystack_[1].value.stringVal) << ")";
#endif
    std::string val;
    val = '"'; /* add quote to keep strings intact */
    val += *(yystack_[3].value.stringVal);
    val += "\", \"";
    val += *(yystack_[1].value.stringVal);
    val += '"';
    (yylhs.value.whereNode) = new ibis::qExpr(ibis::qExpr::LOGICAL_NOT);
    (yylhs.value.whereNode)->setLeft(new ibis::qAnyString((yystack_[7].value.stringVal)->c_str(), val.c_str()));
    delete (yystack_[1].value.stringVal);
    delete (yystack_[3].value.stringVal);
    delete (yystack_[7].value.stringVal);
}
#line 1167 "whereParser.cc" // lalr1.cc:859
    break;

  case 33:
#line 431 "whereParser.yy" // lalr1.cc:859
    {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *(yystack_[7].value.stringVal) << " NOT IN ("
	<< *(yystack_[3].value.stringVal) << ", " << *(yystack_[1].value.stringVal) << ")";
#endif
    std::string val;
    val = '"'; /* add quote to keep strings intact */
    val += *(yystack_[3].value.stringVal);
    val += "\", \"";
    val += *(yystack_[1].value.stringVal);
    val += '"';
    (yylhs.value.whereNode) = new ibis::qExpr(ibis::qExpr::LOGICAL_NOT);
    (yylhs.value.whereNode)->setLeft(new ibis::qAnyString((yystack_[7].value.stringVal)->c_str(), val.c_str()));
    delete (yystack_[1].value.stringVal);
    delete (yystack_[3].value.stringVal);
    delete (yystack_[7].value.stringVal);
}
#line 1190 "whereParser.cc" // lalr1.cc:859
    break;

  case 34:
#line 449 "whereParser.yy" // lalr1.cc:859
    {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *(yystack_[7].value.stringVal) << " NOT IN ("
	<< *(yystack_[3].value.stringVal) << ", " << *(yystack_[1].value.stringVal) << ")";
#endif
    std::string val;
    val = '"'; /* add quote to keep strings intact */
    val += *(yystack_[3].value.stringVal);
    val += "\", \"";
    val += *(yystack_[1].value.stringVal);
    val += '"';
    (yylhs.value.whereNode) = new ibis::qExpr(ibis::qExpr::LOGICAL_NOT);
    (yylhs.value.whereNode)->setLeft(new ibis::qAnyString((yystack_[7].value.stringVal)->c_str(), val.c_str()));
    delete (yystack_[1].value.stringVal);
    delete (yystack_[3].value.stringVal);
    delete (yystack_[7].value.stringVal);
}
#line 1213 "whereParser.cc" // lalr1.cc:859
    break;

  case 35:
#line 467 "whereParser.yy" // lalr1.cc:859
    {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *(yystack_[7].value.stringVal) << " NOT IN ("
	<< *(yystack_[3].value.stringVal) << ", " << *(yystack_[1].value.stringVal) << ")";
#endif
    std::string val;
    val = '"'; /* add quote to keep strings intact */
    val += *(yystack_[3].value.stringVal);
    val += "\", \"";
    val += *(yystack_[1].value.stringVal);
    val += '"';
    (yylhs.value.whereNode) = new ibis::qExpr(ibis::qExpr::LOGICAL_NOT);
    (yylhs.value.whereNode)->setLeft(new ibis::qAnyString((yystack_[7].value.stringVal)->c_str(), val.c_str()));
    delete (yystack_[1].value.stringVal);
    delete (yystack_[3].value.stringVal);
    delete (yystack_[7].value.stringVal);
}
#line 1236 "whereParser.cc" // lalr1.cc:859
    break;

  case 36:
#line 485 "whereParser.yy" // lalr1.cc:859
    {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *(yystack_[5].value.stringVal) << " NOT IN ("
	<< *(yystack_[1].value.stringVal) << ")";
#endif
    std::string val;
    val = '"'; /* add quote to keep strings intact */
    val += *(yystack_[1].value.stringVal);
    val += '"';
    (yylhs.value.whereNode) = new ibis::qExpr(ibis::qExpr::LOGICAL_NOT);
    (yylhs.value.whereNode)->setLeft(new ibis::qAnyString((yystack_[5].value.stringVal)->c_str(), val.c_str()));
    delete (yystack_[1].value.stringVal);
    delete (yystack_[5].value.stringVal);
}
#line 1256 "whereParser.cc" // lalr1.cc:859
    break;

  case 37:
#line 500 "whereParser.yy" // lalr1.cc:859
    {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *(yystack_[5].value.stringVal) << " NOT IN ("
	<< *(yystack_[1].value.stringVal) << ")";
#endif
    std::string val;
    val = '"'; /* add quote to keep strings intact */
    val += *(yystack_[1].value.stringVal);
    val += '"';
    (yylhs.value.whereNode) = new ibis::qExpr(ibis::qExpr::LOGICAL_NOT);
    (yylhs.value.whereNode)->setLeft(new ibis::qAnyString((yystack_[5].value.stringVal)->c_str(), val.c_str()));
    delete (yystack_[1].value.stringVal);
    delete (yystack_[5].value.stringVal);
}
#line 1276 "whereParser.cc" // lalr1.cc:859
    break;

  case 38:
#line 515 "whereParser.yy" // lalr1.cc:859
    {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *(yystack_[2].value.stringVal) << " in ("
	<< *(yystack_[0].value.stringVal) << ")";
#endif
    (yylhs.value.whereNode) = new ibis::qIntHod((yystack_[2].value.stringVal)->c_str(), (yystack_[0].value.stringVal)->c_str());
    delete (yystack_[0].value.stringVal);
    delete (yystack_[2].value.stringVal);
}
#line 1291 "whereParser.cc" // lalr1.cc:859
    break;

  case 39:
#line 525 "whereParser.yy" // lalr1.cc:859
    {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *(yystack_[3].value.stringVal) << " not in ("
	<< *(yystack_[0].value.stringVal) << ")";
#endif
    (yylhs.value.whereNode) = new ibis::qExpr(ibis::qExpr::LOGICAL_NOT);
    (yylhs.value.whereNode)->setLeft(new ibis::qIntHod((yystack_[3].value.stringVal)->c_str(), (yystack_[0].value.stringVal)->c_str()));
    delete (yystack_[0].value.stringVal);
    delete (yystack_[3].value.stringVal);
}
#line 1307 "whereParser.cc" // lalr1.cc:859
    break;

  case 40:
#line 536 "whereParser.yy" // lalr1.cc:859
    {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *(yystack_[2].value.stringVal) << " in ("
	<< *(yystack_[0].value.stringVal) << ")";
#endif
    (yylhs.value.whereNode) = new ibis::qUIntHod((yystack_[2].value.stringVal)->c_str(), (yystack_[0].value.stringVal)->c_str());
    delete (yystack_[0].value.stringVal);
    delete (yystack_[2].value.stringVal);
}
#line 1322 "whereParser.cc" // lalr1.cc:859
    break;

  case 41:
#line 546 "whereParser.yy" // lalr1.cc:859
    {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *(yystack_[3].value.stringVal) << " not in ("
	<< *(yystack_[0].value.stringVal) << ")";
#endif
    (yylhs.value.whereNode) = new ibis::qExpr(ibis::qExpr::LOGICAL_NOT);
    (yylhs.value.whereNode)->setLeft(new ibis::qUIntHod((yystack_[3].value.stringVal)->c_str(), (yystack_[0].value.stringVal)->c_str()));
    delete (yystack_[0].value.stringVal);
    delete (yystack_[3].value.stringVal);
}
#line 1338 "whereParser.cc" // lalr1.cc:859
    break;

  case 42:
#line 557 "whereParser.yy" // lalr1.cc:859
    {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *(yystack_[2].value.stringVal)
	<< " CONTAINS " << *(yystack_[0].value.stringVal);
#endif
    (yylhs.value.whereNode) = new ibis::qKeyword((yystack_[2].value.stringVal)->c_str(), (yystack_[0].value.stringVal)->c_str());
    delete (yystack_[2].value.stringVal);
    delete (yystack_[0].value.stringVal);
}
#line 1353 "whereParser.cc" // lalr1.cc:859
    break;

  case 43:
#line 567 "whereParser.yy" // lalr1.cc:859
    {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << (yystack_[2].value.stringVal)
	<< " CONTAINS " << *(yystack_[0].value.stringVal);
#endif
    (yylhs.value.whereNode) = new ibis::qKeyword((yystack_[2].value.stringVal)->c_str(), (yystack_[0].value.stringVal)->c_str());
    delete (yystack_[0].value.stringVal);
    delete (yystack_[2].value.stringVal);
}
#line 1368 "whereParser.cc" // lalr1.cc:859
    break;

  case 44:
#line 577 "whereParser.yy" // lalr1.cc:859
    {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *(yystack_[4].value.stringVal)
	<< " CONTAINS " << *(yystack_[1].value.stringVal);
#endif
    (yylhs.value.whereNode) = new ibis::qKeyword((yystack_[4].value.stringVal)->c_str(), (yystack_[1].value.stringVal)->c_str());
    delete (yystack_[4].value.stringVal);
    delete (yystack_[1].value.stringVal);
}
#line 1383 "whereParser.cc" // lalr1.cc:859
    break;

  case 45:
#line 587 "whereParser.yy" // lalr1.cc:859
    {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << (yystack_[4].value.stringVal)
	<< " CONTAINS " << *(yystack_[1].value.stringVal);
#endif
    (yylhs.value.whereNode) = new ibis::qKeyword((yystack_[4].value.stringVal)->c_str(), (yystack_[1].value.stringVal)->c_str());
    delete (yystack_[1].value.stringVal);
    delete (yystack_[4].value.stringVal);
}
#line 1398 "whereParser.cc" // lalr1.cc:859
    break;

  case 46:
#line 597 "whereParser.yy" // lalr1.cc:859
    {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << (yystack_[6].value.stringVal)
	<< " CONTAINS (" << *(yystack_[3].value.stringVal) << ", " << *(yystack_[1].value.stringVal) << ')';
#endif
    (yylhs.value.whereNode) = new ibis::qAllWords((yystack_[6].value.stringVal)->c_str(), (yystack_[3].value.stringVal)->c_str(), (yystack_[1].value.stringVal)->c_str());
    delete (yystack_[1].value.stringVal);
    delete (yystack_[3].value.stringVal);
    delete (yystack_[6].value.stringVal);
}
#line 1414 "whereParser.cc" // lalr1.cc:859
    break;

  case 47:
#line 608 "whereParser.yy" // lalr1.cc:859
    {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << (yystack_[6].value.stringVal)
	<< " CONTAINS (" << *(yystack_[3].value.stringVal) << ", " << *(yystack_[1].value.stringVal) << ')';
#endif
    (yylhs.value.whereNode) = new ibis::qAllWords((yystack_[6].value.stringVal)->c_str(), (yystack_[3].value.stringVal)->c_str(), (yystack_[1].value.stringVal)->c_str());
    delete (yystack_[1].value.stringVal);
    delete (yystack_[3].value.stringVal);
    delete (yystack_[6].value.stringVal);
}
#line 1430 "whereParser.cc" // lalr1.cc:859
    break;

  case 48:
#line 619 "whereParser.yy" // lalr1.cc:859
    {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << (yystack_[6].value.stringVal)
	<< " CONTAINS (" << *(yystack_[3].value.stringVal) << ", " << *(yystack_[1].value.stringVal) << ')';
#endif
    (yylhs.value.whereNode) = new ibis::qAllWords((yystack_[6].value.stringVal)->c_str(), (yystack_[3].value.stringVal)->c_str(), (yystack_[1].value.stringVal)->c_str());
    delete (yystack_[1].value.stringVal);
    delete (yystack_[3].value.stringVal);
    delete (yystack_[6].value.stringVal);
}
#line 1446 "whereParser.cc" // lalr1.cc:859
    break;

  case 49:
#line 630 "whereParser.yy" // lalr1.cc:859
    {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << (yystack_[6].value.stringVal)
	<< " CONTAINS (" << *(yystack_[3].value.stringVal) << ", " << *(yystack_[1].value.stringVal) << ')';
#endif
    (yylhs.value.whereNode) = new ibis::qAllWords((yystack_[6].value.stringVal)->c_str(), (yystack_[3].value.stringVal)->c_str(), (yystack_[1].value.stringVal)->c_str());
    delete (yystack_[1].value.stringVal);
    delete (yystack_[3].value.stringVal);
    delete (yystack_[6].value.stringVal);
}
#line 1462 "whereParser.cc" // lalr1.cc:859
    break;

  case 50:
#line 641 "whereParser.yy" // lalr1.cc:859
    {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << (yystack_[2].value.stringVal)
	<< " CONTAINS (" << *(yystack_[0].value.stringVal) << ')';
#endif
    (yylhs.value.whereNode) = new ibis::qAllWords((yystack_[2].value.stringVal)->c_str(), (yystack_[0].value.stringVal)->c_str());
    delete (yystack_[0].value.stringVal);
    delete (yystack_[2].value.stringVal);
}
#line 1477 "whereParser.cc" // lalr1.cc:859
    break;

  case 51:
#line 651 "whereParser.yy" // lalr1.cc:859
    {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- ANY(" << *(yystack_[3].value.stringVal) << ") = "
	<< (yystack_[0].value.doubleVal) << ")";
#endif
    (yylhs.value.whereNode) = new ibis::qAnyAny((yystack_[3].value.stringVal)->c_str(), (yystack_[0].value.doubleVal));
    delete (yystack_[3].value.stringVal);
}
#line 1491 "whereParser.cc" // lalr1.cc:859
    break;

  case 52:
#line 660 "whereParser.yy" // lalr1.cc:859
    {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- ANY(" << *(yystack_[3].value.stringVal) << ") = "
	<< *(yystack_[0].value.stringVal) << ")";
#endif
    (yylhs.value.whereNode) = new ibis::qAnyAny((yystack_[3].value.stringVal)->c_str(), (yystack_[0].value.stringVal)->c_str());
    delete (yystack_[0].value.stringVal);
    delete (yystack_[3].value.stringVal);
}
#line 1506 "whereParser.cc" // lalr1.cc:859
    break;

  case 53:
#line 670 "whereParser.yy" // lalr1.cc:859
    {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *(yystack_[2].value.stringVal) << " = " << *(yystack_[0].value.int64Val);
#endif
    (yylhs.value.whereNode) = new ibis::qIntHod((yystack_[2].value.stringVal)->c_str(), (yystack_[0].value.int64Val));
    delete (yystack_[2].value.stringVal);
}
#line 1519 "whereParser.cc" // lalr1.cc:859
    break;

  case 54:
#line 678 "whereParser.yy" // lalr1.cc:859
    {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *(yystack_[2].value.stringVal) << " != " << *(yystack_[0].value.int64Val);
#endif
    (yylhs.value.whereNode) = new ibis::qExpr(ibis::qExpr::LOGICAL_NOT);
    (yylhs.value.whereNode)->setLeft(new ibis::qIntHod((yystack_[2].value.stringVal)->c_str(), (yystack_[0].value.int64Val)));
    delete (yystack_[2].value.stringVal);
}
#line 1533 "whereParser.cc" // lalr1.cc:859
    break;

  case 55:
#line 687 "whereParser.yy" // lalr1.cc:859
    {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *(yystack_[2].value.stringVal) << " = " << *(yystack_[0].value.uint64Val);
#endif
    (yylhs.value.whereNode) = new ibis::qUIntHod((yystack_[2].value.stringVal)->c_str(), (yystack_[0].value.uint64Val));
    delete (yystack_[2].value.stringVal);
}
#line 1546 "whereParser.cc" // lalr1.cc:859
    break;

  case 56:
#line 695 "whereParser.yy" // lalr1.cc:859
    {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *(yystack_[2].value.stringVal) << " != " << *(yystack_[0].value.uint64Val);
#endif
    (yylhs.value.whereNode) = new ibis::qExpr(ibis::qExpr::LOGICAL_NOT);
    (yylhs.value.whereNode)->setLeft(new ibis::qUIntHod((yystack_[2].value.stringVal)->c_str(), (yystack_[0].value.uint64Val)));
    delete (yystack_[2].value.stringVal);
}
#line 1560 "whereParser.cc" // lalr1.cc:859
    break;

  case 57:
#line 704 "whereParser.yy" // lalr1.cc:859
    {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *(yystack_[0].value.stringVal) << " = "
	<< *(yystack_[2].value.stringVal);
#endif
    (yylhs.value.whereNode) = new ibis::qString((yystack_[0].value.stringVal)->c_str(), (yystack_[2].value.stringVal)->c_str());
    delete (yystack_[0].value.stringVal);
    delete (yystack_[2].value.stringVal);
}
#line 1575 "whereParser.cc" // lalr1.cc:859
    break;

  case 58:
#line 714 "whereParser.yy" // lalr1.cc:859
    {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *(yystack_[0].value.stringVal) << " = "
	<< *(yystack_[2].value.stringVal);
#endif
    (yylhs.value.whereNode) = new ibis::qExpr(ibis::qExpr::LOGICAL_NOT);
    (yylhs.value.whereNode)->setLeft(new ibis::qString((yystack_[0].value.stringVal)->c_str(), (yystack_[2].value.stringVal)->c_str()));
    delete (yystack_[0].value.stringVal);
    delete (yystack_[2].value.stringVal);
}
#line 1591 "whereParser.cc" // lalr1.cc:859
    break;

  case 59:
#line 725 "whereParser.yy" // lalr1.cc:859
    {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *(yystack_[2].value.stringVal) << " = "
	<< *(yystack_[0].value.stringVal);
#endif
    (yylhs.value.whereNode) = new ibis::qString((yystack_[2].value.stringVal)->c_str(), (yystack_[0].value.stringVal)->c_str());
    delete (yystack_[0].value.stringVal);
    delete (yystack_[2].value.stringVal);
}
#line 1606 "whereParser.cc" // lalr1.cc:859
    break;

  case 60:
#line 735 "whereParser.yy" // lalr1.cc:859
    {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *(yystack_[2].value.stringVal) << " != "
	<< *(yystack_[0].value.stringVal);
#endif
    (yylhs.value.whereNode) = new ibis::qExpr(ibis::qExpr::LOGICAL_NOT);
    (yylhs.value.whereNode)->setLeft(new ibis::qString((yystack_[2].value.stringVal)->c_str(), (yystack_[0].value.stringVal)->c_str()));
    delete (yystack_[0].value.stringVal);
    delete (yystack_[2].value.stringVal);
}
#line 1622 "whereParser.cc" // lalr1.cc:859
    break;

  case 61:
#line 746 "whereParser.yy" // lalr1.cc:859
    {
    ibis::math::term *me2 = static_cast<ibis::math::term*>((yystack_[0].value.whereNode));
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *(yystack_[2].value.stringVal) << " = "
	<< *me2;
#endif
    if (me2->termType() == ibis::math::NUMBER) {
	(yylhs.value.whereNode) = new ibis::qContinuousRange((yystack_[2].value.stringVal)->c_str(), ibis::qExpr::OP_EQ, me2->eval());
	delete (yystack_[0].value.whereNode);
    }
    else {
	ibis::math::variable *me1 = new ibis::math::variable((yystack_[2].value.stringVal)->c_str());
	(yylhs.value.whereNode) = new ibis::compRange(me1, ibis::qExpr::OP_EQ, me2);
    }
    delete (yystack_[2].value.stringVal);
}
#line 1644 "whereParser.cc" // lalr1.cc:859
    break;

  case 62:
#line 763 "whereParser.yy" // lalr1.cc:859
    {
    ibis::math::term *me2 = static_cast<ibis::math::term*>((yystack_[0].value.whereNode));
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *(yystack_[2].value.stringVal) << " = "
	<< *me2;
#endif
    ibis::qExpr*tmp = 0;
    if (me2->termType() == ibis::math::NUMBER) {
	tmp = new ibis::qContinuousRange((yystack_[2].value.stringVal)->c_str(), ibis::qExpr::OP_EQ, me2->eval());
	delete (yystack_[0].value.whereNode);
    }
    else {
	ibis::math::variable *me1 = new ibis::math::variable((yystack_[2].value.stringVal)->c_str());
	tmp = new ibis::compRange(me1, ibis::qExpr::OP_EQ, me2);
    }
    delete (yystack_[2].value.stringVal);
    (yylhs.value.whereNode) = new ibis::qExpr(ibis::qExpr::LOGICAL_NOT);
    (yylhs.value.whereNode)->setLeft(tmp);
}
#line 1669 "whereParser.cc" // lalr1.cc:859
    break;

  case 63:
#line 786 "whereParser.yy" // lalr1.cc:859
    {
    ibis::math::term *me2 = static_cast<ibis::math::term*>((yystack_[0].value.whereNode));
    ibis::math::term *me1 = static_cast<ibis::math::term*>((yystack_[2].value.whereNode));
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *me1 << " = "
	<< *me2;
#endif
    (yylhs.value.whereNode) = new ibis::compRange(me1, ibis::qExpr::OP_EQ, me2);
}
#line 1684 "whereParser.cc" // lalr1.cc:859
    break;

  case 64:
#line 796 "whereParser.yy" // lalr1.cc:859
    {
    ibis::math::term *me2 = static_cast<ibis::math::term*>((yystack_[0].value.whereNode));
    ibis::math::term *me1 = static_cast<ibis::math::term*>((yystack_[2].value.whereNode));
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *me1 << " != "
	<< *me2;
#endif
    (yylhs.value.whereNode) = new ibis::qExpr(ibis::qExpr::LOGICAL_NOT);
    (yylhs.value.whereNode)->setLeft(new ibis::compRange(me1, ibis::qExpr::OP_EQ, me2));
}
#line 1700 "whereParser.cc" // lalr1.cc:859
    break;

  case 65:
#line 807 "whereParser.yy" // lalr1.cc:859
    {
    ibis::math::term *me2 = static_cast<ibis::math::term*>((yystack_[0].value.whereNode));
    ibis::math::term *me1 = static_cast<ibis::math::term*>((yystack_[2].value.whereNode));
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *me1 << " < "
	<< *me2;
#endif
    (yylhs.value.whereNode) = new ibis::compRange(me1, ibis::qExpr::OP_LT, me2);
}
#line 1715 "whereParser.cc" // lalr1.cc:859
    break;

  case 66:
#line 817 "whereParser.yy" // lalr1.cc:859
    {
    ibis::math::term *me2 = static_cast<ibis::math::term*>((yystack_[0].value.whereNode));
    ibis::math::term *me1 = static_cast<ibis::math::term*>((yystack_[2].value.whereNode));
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *me1 << " <= "
	<< *me2;
#endif
    (yylhs.value.whereNode) = new ibis::compRange(me1, ibis::qExpr::OP_LE, me2);
}
#line 1730 "whereParser.cc" // lalr1.cc:859
    break;

  case 67:
#line 827 "whereParser.yy" // lalr1.cc:859
    {
    ibis::math::term *me2 = static_cast<ibis::math::term*>((yystack_[0].value.whereNode));
    ibis::math::term *me1 = static_cast<ibis::math::term*>((yystack_[2].value.whereNode));
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *me1 << " > "
	<< *me2;
#endif
    (yylhs.value.whereNode) = new ibis::compRange(me1, ibis::qExpr::OP_GT, me2);
}
#line 1745 "whereParser.cc" // lalr1.cc:859
    break;

  case 68:
#line 837 "whereParser.yy" // lalr1.cc:859
    {
    ibis::math::term *me2 = static_cast<ibis::math::term*>((yystack_[0].value.whereNode));
    ibis::math::term *me1 = static_cast<ibis::math::term*>((yystack_[2].value.whereNode));
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *me1 << " >= "
	<< *me2;
#endif
    (yylhs.value.whereNode) = new ibis::compRange(me1, ibis::qExpr::OP_GE, me2);
}
#line 1760 "whereParser.cc" // lalr1.cc:859
    break;

  case 69:
#line 899 "whereParser.yy" // lalr1.cc:859
    {
    ibis::math::term *me3 = static_cast<ibis::math::term*>((yystack_[0].value.whereNode));
    ibis::math::term *me2 = static_cast<ibis::math::term*>((yystack_[2].value.whereNode));
    ibis::math::term *me1 = static_cast<ibis::math::term*>((yystack_[4].value.whereNode));
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *me1 << " < "
	<< *me2 << " < " << *me3;
#endif
    (yylhs.value.whereNode) = new ibis::compRange(me1, ibis::qExpr::OP_LT, me2,
			     ibis::qExpr::OP_LT, me3);
}
#line 1777 "whereParser.cc" // lalr1.cc:859
    break;

  case 70:
#line 911 "whereParser.yy" // lalr1.cc:859
    {
    ibis::math::term *me3 = static_cast<ibis::math::term*>((yystack_[0].value.whereNode));
    ibis::math::term *me2 = static_cast<ibis::math::term*>((yystack_[2].value.whereNode));
    ibis::math::term *me1 = static_cast<ibis::math::term*>((yystack_[4].value.whereNode));
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *me1 << " < "
	<< *me2 << " <= " << *me3;
#endif
    (yylhs.value.whereNode) = new ibis::compRange(me1, ibis::qExpr::OP_LT, me2,
			     ibis::qExpr::OP_LE, me3);
}
#line 1794 "whereParser.cc" // lalr1.cc:859
    break;

  case 71:
#line 923 "whereParser.yy" // lalr1.cc:859
    {
    ibis::math::term *me3 = static_cast<ibis::math::term*>((yystack_[0].value.whereNode));
    ibis::math::term *me2 = static_cast<ibis::math::term*>((yystack_[2].value.whereNode));
    ibis::math::term *me1 = static_cast<ibis::math::term*>((yystack_[4].value.whereNode));
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *me1 << " <= "
	<< *me2 << " < " << *me3;
#endif
    (yylhs.value.whereNode) = new ibis::compRange(me1, ibis::qExpr::OP_LE, me2,
			     ibis::qExpr::OP_LT, me3);
}
#line 1811 "whereParser.cc" // lalr1.cc:859
    break;

  case 72:
#line 935 "whereParser.yy" // lalr1.cc:859
    {
    ibis::math::term *me3 = static_cast<ibis::math::term*>((yystack_[0].value.whereNode));
    ibis::math::term *me2 = static_cast<ibis::math::term*>((yystack_[2].value.whereNode));
    ibis::math::term *me1 = static_cast<ibis::math::term*>((yystack_[4].value.whereNode));
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *me1 << " <= "
	<< *me2 << " <= " << *me3;
#endif
    (yylhs.value.whereNode) = new ibis::compRange(me1, ibis::qExpr::OP_LE, me2,
			     ibis::qExpr::OP_LE, me3);
}
#line 1828 "whereParser.cc" // lalr1.cc:859
    break;

  case 73:
#line 947 "whereParser.yy" // lalr1.cc:859
    {
    ibis::math::term *me3 = static_cast<ibis::math::term*>((yystack_[0].value.whereNode));
    ibis::math::term *me2 = static_cast<ibis::math::term*>((yystack_[2].value.whereNode));
    ibis::math::term *me1 = static_cast<ibis::math::term*>((yystack_[4].value.whereNode));
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *me1 << " > "
	<< *me2 << " > " << *me3;
#endif
    (yylhs.value.whereNode) = new ibis::compRange(me3, ibis::qExpr::OP_LT, me2,
			     ibis::qExpr::OP_LT, me1);
}
#line 1845 "whereParser.cc" // lalr1.cc:859
    break;

  case 74:
#line 959 "whereParser.yy" // lalr1.cc:859
    {
    ibis::math::term *me3 = static_cast<ibis::math::term*>((yystack_[0].value.whereNode));
    ibis::math::term *me2 = static_cast<ibis::math::term*>((yystack_[2].value.whereNode));
    ibis::math::term *me1 = static_cast<ibis::math::term*>((yystack_[4].value.whereNode));
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *me1 << " > "
	<< *me2 << " >= " << *me3;
#endif
    (yylhs.value.whereNode) = new ibis::compRange(me3, ibis::qExpr::OP_LE, me2,
			     ibis::qExpr::OP_LT, me1);
}
#line 1862 "whereParser.cc" // lalr1.cc:859
    break;

  case 75:
#line 971 "whereParser.yy" // lalr1.cc:859
    {
    ibis::math::term *me3 = static_cast<ibis::math::term*>((yystack_[0].value.whereNode));
    ibis::math::term *me2 = static_cast<ibis::math::term*>((yystack_[2].value.whereNode));
    ibis::math::term *me1 = static_cast<ibis::math::term*>((yystack_[4].value.whereNode));
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *me1 << " >= "
	<< *me2 << " > " << *me3;
#endif
    (yylhs.value.whereNode) = new ibis::compRange(me3, ibis::qExpr::OP_LT, me2,
			     ibis::qExpr::OP_LE, me1);
}
#line 1879 "whereParser.cc" // lalr1.cc:859
    break;

  case 76:
#line 983 "whereParser.yy" // lalr1.cc:859
    {
    ibis::math::term *me3 = static_cast<ibis::math::term*>((yystack_[0].value.whereNode));
    ibis::math::term *me2 = static_cast<ibis::math::term*>((yystack_[2].value.whereNode));
    ibis::math::term *me1 = static_cast<ibis::math::term*>((yystack_[4].value.whereNode));
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *me1 << " >= "
	<< *me2 << " >= " << *me3;
#endif
    (yylhs.value.whereNode) = new ibis::compRange(me3, ibis::qExpr::OP_LE, me2,
			     ibis::qExpr::OP_LE, me1);
}
#line 1896 "whereParser.cc" // lalr1.cc:859
    break;

  case 77:
#line 995 "whereParser.yy" // lalr1.cc:859
    {
    ibis::math::term *me3 = static_cast<ibis::math::term*>((yystack_[0].value.whereNode));
    ibis::math::term *me2 = static_cast<ibis::math::term*>((yystack_[2].value.whereNode));
    ibis::math::term *me1 = static_cast<ibis::math::term*>((yystack_[4].value.whereNode));
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *me1 << " BETWEEN "
	<< *me2 << " AND " << *me3;
#endif
    (yylhs.value.whereNode) = new ibis::compRange(me2, ibis::qExpr::OP_LE, me1,
			     ibis::qExpr::OP_LE, me3);
}
#line 1913 "whereParser.cc" // lalr1.cc:859
    break;

  case 78:
#line 1010 "whereParser.yy" // lalr1.cc:859
    {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *(yystack_[2].value.whereNode)
	<< " + " << *(yystack_[0].value.whereNode);
#endif
    ibis::math::bediener *opr =
	new ibis::math::bediener(ibis::math::PLUS);
    opr->setRight((yystack_[0].value.whereNode));
    opr->setLeft((yystack_[2].value.whereNode));
    (yylhs.value.whereNode) = static_cast<ibis::qExpr*>(opr);
}
#line 1930 "whereParser.cc" // lalr1.cc:859
    break;

  case 79:
#line 1022 "whereParser.yy" // lalr1.cc:859
    {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *(yystack_[2].value.whereNode)
	<< " - " << *(yystack_[0].value.whereNode);
#endif
    ibis::math::bediener *opr =
	new ibis::math::bediener(ibis::math::MINUS);
    opr->setRight((yystack_[0].value.whereNode));
    opr->setLeft((yystack_[2].value.whereNode));
    (yylhs.value.whereNode) = static_cast<ibis::qExpr*>(opr);
}
#line 1947 "whereParser.cc" // lalr1.cc:859
    break;

  case 80:
#line 1034 "whereParser.yy" // lalr1.cc:859
    {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *(yystack_[2].value.whereNode)
	<< " * " << *(yystack_[0].value.whereNode);
#endif
    ibis::math::bediener *opr =
	new ibis::math::bediener(ibis::math::MULTIPLY);
    opr->setRight((yystack_[0].value.whereNode));
    opr->setLeft((yystack_[2].value.whereNode));
    (yylhs.value.whereNode) = static_cast<ibis::qExpr*>(opr);
}
#line 1964 "whereParser.cc" // lalr1.cc:859
    break;

  case 81:
#line 1046 "whereParser.yy" // lalr1.cc:859
    {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *(yystack_[2].value.whereNode)
	<< " / " << *(yystack_[0].value.whereNode);
#endif
    ibis::math::bediener *opr =
	new ibis::math::bediener(ibis::math::DIVIDE);
    opr->setRight((yystack_[0].value.whereNode));
    opr->setLeft((yystack_[2].value.whereNode));
    (yylhs.value.whereNode) = static_cast<ibis::qExpr*>(opr);
}
#line 1981 "whereParser.cc" // lalr1.cc:859
    break;

  case 82:
#line 1058 "whereParser.yy" // lalr1.cc:859
    {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *(yystack_[2].value.whereNode)
	<< " % " << *(yystack_[0].value.whereNode);
#endif
    ibis::math::bediener *opr =
	new ibis::math::bediener(ibis::math::REMAINDER);
    opr->setRight((yystack_[0].value.whereNode));
    opr->setLeft((yystack_[2].value.whereNode));
    (yylhs.value.whereNode) = static_cast<ibis::qExpr*>(opr);
}
#line 1998 "whereParser.cc" // lalr1.cc:859
    break;

  case 83:
#line 1070 "whereParser.yy" // lalr1.cc:859
    {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *(yystack_[2].value.whereNode)
	<< " ^ " << *(yystack_[0].value.whereNode);
#endif
    ibis::math::bediener *opr =
	new ibis::math::bediener(ibis::math::POWER);
    opr->setRight((yystack_[0].value.whereNode));
    opr->setLeft((yystack_[2].value.whereNode));
    (yylhs.value.whereNode) = static_cast<ibis::qExpr*>(opr);
}
#line 2015 "whereParser.cc" // lalr1.cc:859
    break;

  case 84:
#line 1082 "whereParser.yy" // lalr1.cc:859
    {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *(yystack_[2].value.whereNode)
	<< " & " << *(yystack_[0].value.whereNode);
#endif
    ibis::math::bediener *opr =
	new ibis::math::bediener(ibis::math::BITAND);
    opr->setRight((yystack_[0].value.whereNode));
    opr->setLeft((yystack_[2].value.whereNode));
    (yylhs.value.whereNode) = static_cast<ibis::qExpr*>(opr);
}
#line 2032 "whereParser.cc" // lalr1.cc:859
    break;

  case 85:
#line 1094 "whereParser.yy" // lalr1.cc:859
    {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *(yystack_[2].value.whereNode)
	<< " | " << *(yystack_[0].value.whereNode);
#endif
    ibis::math::bediener *opr =
	new ibis::math::bediener(ibis::math::BITOR);
    opr->setRight((yystack_[0].value.whereNode));
    opr->setLeft((yystack_[2].value.whereNode));
    (yylhs.value.whereNode) = static_cast<ibis::qExpr*>(opr);
}
#line 2049 "whereParser.cc" // lalr1.cc:859
    break;

  case 86:
#line 1106 "whereParser.yy" // lalr1.cc:859
    {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *(yystack_[3].value.stringVal) << "("
	<< *(yystack_[1].value.whereNode) << ")";
#endif
    ibis::math::stdFunction1 *fun =
	new ibis::math::stdFunction1((yystack_[3].value.stringVal)->c_str());
    delete (yystack_[3].value.stringVal);
    fun->setLeft((yystack_[1].value.whereNode));
    (yylhs.value.whereNode) = static_cast<ibis::qExpr*>(fun);
}
#line 2066 "whereParser.cc" // lalr1.cc:859
    break;

  case 87:
#line 1118 "whereParser.yy" // lalr1.cc:859
    {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *(yystack_[5].value.stringVal) << "("
	<< *(yystack_[3].value.whereNode) << ", " << *(yystack_[1].value.whereNode) << ")";
#endif
    ibis::math::stdFunction2 *fun =
	new ibis::math::stdFunction2((yystack_[5].value.stringVal)->c_str());
    fun->setRight((yystack_[1].value.whereNode));
    fun->setLeft((yystack_[3].value.whereNode));
    (yylhs.value.whereNode) = static_cast<ibis::qExpr*>(fun);
    delete (yystack_[5].value.stringVal);
}
#line 2084 "whereParser.cc" // lalr1.cc:859
    break;

  case 88:
#line 1131 "whereParser.yy" // lalr1.cc:859
    {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- FROM_UNIXTIME_LOCAL("
	<< *(yystack_[3].value.whereNode) << ", " << *(yystack_[1].value.stringVal) << ")";
#endif
    ibis::math::fromUnixTime fut((yystack_[1].value.stringVal)->c_str());
    ibis::math::customFunction1 *fun =
	new ibis::math::customFunction1(fut);
    fun->setLeft((yystack_[3].value.whereNode));
    (yylhs.value.whereNode) = static_cast<ibis::qExpr*>(fun);
    delete (yystack_[1].value.stringVal);
}
#line 2102 "whereParser.cc" // lalr1.cc:859
    break;

  case 89:
#line 1144 "whereParser.yy" // lalr1.cc:859
    {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- FROM_UNIXTIME_GMT("
	<< *(yystack_[3].value.whereNode) << ", " << *(yystack_[1].value.stringVal) << ")";
#endif

    ibis::math::fromUnixTime fut((yystack_[1].value.stringVal)->c_str(), "GMT");
    ibis::math::customFunction1 *fun =
	new ibis::math::customFunction1(fut);
    fun->setLeft((yystack_[3].value.whereNode));
    (yylhs.value.whereNode) = static_cast<ibis::qExpr*>(fun);
    delete (yystack_[1].value.stringVal);
}
#line 2121 "whereParser.cc" // lalr1.cc:859
    break;

  case 90:
#line 1158 "whereParser.yy" // lalr1.cc:859
    {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- ISO_TO_UNIXTIME_LOCAL("
	<< *(yystack_[1].value.whereNode) << ")";
#endif

    ibis::math::toUnixTime fut;
    ibis::math::customFunction1 *fun =
	new ibis::math::customFunction1(fut);
    fun->setLeft((yystack_[1].value.whereNode));
    (yylhs.value.whereNode) = static_cast<ibis::qExpr*>(fun);
}
#line 2139 "whereParser.cc" // lalr1.cc:859
    break;

  case 91:
#line 1171 "whereParser.yy" // lalr1.cc:859
    {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- ISO_TO_UNIXTIME_GMT("
	<< *(yystack_[1].value.whereNode) << ")";
#endif

    ibis::math::toUnixTime fut("GMT0");
    ibis::math::customFunction1 *fun =
	new ibis::math::customFunction1(fut);
    fun->setLeft((yystack_[1].value.whereNode));
    (yylhs.value.whereNode) = static_cast<ibis::qExpr*>(fun);
}
#line 2157 "whereParser.cc" // lalr1.cc:859
    break;

  case 92:
#line 1184 "whereParser.yy" // lalr1.cc:859
    {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- TO_UNIXTIME_LOCAL("
	<< *(yystack_[3].value.stringVal) << ", " << *(yystack_[1].value.stringVal)  << ")";
#endif
#if defined(HAVE_STRPTIME)
    struct tm mytm;
    memset(&mytm, 0, sizeof(mytm));
    const char *ret = strptime((yystack_[3].value.stringVal)->c_str(), (yystack_[1].value.stringVal)->c_str(), &mytm);
    if (ret != 0) {
        // A negative value for tm_isdst causes mktime() to attempt to
        // determine whether Daylight Saving Time is in effect for the
        // specified time.
        mytm.tm_isdst = -1;
        if (mytm.tm_mday == 0) {
            // This can happen if we are using a format without '%d'
            // e.g. "%Y%m" as tm_mday is day of the month (in the range 1
            // through 31).
            mytm.tm_mday = 1;
        }
        (yylhs.value.whereNode) = new ibis::math::number(mktime(&mytm));
    }
    delete (yystack_[3].value.stringVal);
    delete (yystack_[1].value.stringVal);

    if (ret == 0) {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- " << __FILE__ << ':' << __LINE__
            << " failed to parse \"" << *(yystack_[3].value.stringVal) << "\" using format string \""
            << *(yystack_[1].value.stringVal) << "\", errno = " << errno;
        throw "Failed to parse string value in TO_UNIXTIME_LOCAL";
    }
#else
    LOGGER(ibis::gVerbose >= 0)
        << "Warning -- " << __FILE__ << ':' << __LINE__
        << " failed to parse \"" << *(yystack_[3].value.stringVal) << "\" using format string \""
        << *(yystack_[1].value.stringVal) << "\" because there is no strptime";
    throw "No strptime to parse string value in TO_UNIXTIME_LOCAL";
#endif
}
#line 2203 "whereParser.cc" // lalr1.cc:859
    break;

  case 93:
#line 1225 "whereParser.yy" // lalr1.cc:859
    {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- TO_UNIXTIME_GMT("
	<< *(yystack_[3].value.stringVal) << ", " << *(yystack_[1].value.stringVal)  << ")";
#endif
#if defined(HAVE_STRPTIME)
    struct tm mytm;
    memset(&mytm, 0, sizeof(mytm));
    const char *ret = strptime((yystack_[3].value.stringVal)->c_str(), (yystack_[1].value.stringVal)->c_str(), &mytm);
    if (ret != 0) {
        if (mytm.tm_mday == 0) {
            // This can happen if we are using a format without '%d'
            // e.g. "%Y%m" as tm_mday is day of the month (in the range 1
            // through 31).
            mytm.tm_mday = 1;
        }
        (yylhs.value.whereNode) = new ibis::math::number(timegm(&mytm));
    }
    delete (yystack_[3].value.stringVal);
    delete (yystack_[1].value.stringVal);

    if (ret == 0) {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- " << __FILE__ << ':' << __LINE__
            << " failed to parse \"" << *(yystack_[3].value.stringVal) << "\" using format string \""
            << *(yystack_[1].value.stringVal) << "\", errno = " << errno;
        throw "Failed to parse string value in TO_UNIXTIME_GMT";
    }
#else
    LOGGER(ibis::gVerbose >= 0)
        << "Warning -- " << __FILE__ << ':' << __LINE__
        << " failed to parse \"" << *(yystack_[3].value.stringVal) << "\" using format string \""
        << *(yystack_[1].value.stringVal) << "\" because there is no strptime";
    throw "No strptime to parse string value in TO_UNIXTIME_GMT";
#endif
}
#line 2245 "whereParser.cc" // lalr1.cc:859
    break;

  case 94:
#line 1262 "whereParser.yy" // lalr1.cc:859
    {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- - " << *(yystack_[0].value.whereNode);
#endif
    ibis::math::bediener *opr =
	new ibis::math::bediener(ibis::math::NEGATE);
    opr->setRight((yystack_[0].value.whereNode));
    (yylhs.value.whereNode) = static_cast<ibis::qExpr*>(opr);
}
#line 2260 "whereParser.cc" // lalr1.cc:859
    break;

  case 95:
#line 1272 "whereParser.yy" // lalr1.cc:859
    {
    (yylhs.value.whereNode) = (yystack_[0].value.whereNode);
}
#line 2268 "whereParser.cc" // lalr1.cc:859
    break;

  case 96:
#line 1275 "whereParser.yy" // lalr1.cc:859
    {
    (yylhs.value.whereNode) = (yystack_[1].value.whereNode);
}
#line 2276 "whereParser.cc" // lalr1.cc:859
    break;

  case 97:
#line 1278 "whereParser.yy" // lalr1.cc:859
    {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " got a variable name " << *(yystack_[0].value.stringVal);
#endif
    ibis::math::variable *var =
	new ibis::math::variable((yystack_[0].value.stringVal)->c_str());
    (yylhs.value.whereNode) = static_cast<ibis::qExpr*>(var);
    delete (yystack_[0].value.stringVal);
}
#line 2291 "whereParser.cc" // lalr1.cc:859
    break;

  case 98:
#line 1288 "whereParser.yy" // lalr1.cc:859
    {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " got a number " << (yystack_[0].value.doubleVal);
#endif
    ibis::math::number *num = new ibis::math::number((yystack_[0].value.doubleVal));
    (yylhs.value.whereNode) = static_cast<ibis::qExpr*>(num);
}
#line 2304 "whereParser.cc" // lalr1.cc:859
    break;

  case 99:
#line 1298 "whereParser.yy" // lalr1.cc:859
    { /* pass qexpr to the driver */
    driver.expr_ = (yystack_[1].value.whereNode);
}
#line 2312 "whereParser.cc" // lalr1.cc:859
    break;

  case 100:
#line 1301 "whereParser.yy" // lalr1.cc:859
    { /* pass qexpr to the driver */
    driver.expr_ = (yystack_[1].value.whereNode);
}
#line 2320 "whereParser.cc" // lalr1.cc:859
    break;


#line 2324 "whereParser.cc" // lalr1.cc:859
            default:
              break;
            }
        }
      catch (const syntax_error& yyexc)
        {
          error (yyexc);
          YYERROR;
        }
      YY_SYMBOL_PRINT ("-> $$ =", yylhs);
      yypop_ (yylen);
      yylen = 0;
      YY_STACK_PRINT ();

      // Shift the result of the reduction.
      yypush_ (YY_NULLPTR, yylhs);
    }
    goto yynewstate;

  /*--------------------------------------.
  | yyerrlab -- here on detecting error.  |
  `--------------------------------------*/
  yyerrlab:
    // If not already recovering from an error, report this error.
    if (!yyerrstatus_)
      {
        ++yynerrs_;
        error (yyla.location, yysyntax_error_ (yystack_[0].state, yyla));
      }


    yyerror_range[1].location = yyla.location;
    if (yyerrstatus_ == 3)
      {
        /* If just tried and failed to reuse lookahead token after an
           error, discard it.  */

        // Return failure if at end of input.
        if (yyla.type_get () == yyeof_)
          YYABORT;
        else if (!yyla.empty ())
          {
            yy_destroy_ ("Error: discarding", yyla);
            yyla.clear ();
          }
      }

    // Else will try to reuse lookahead token after shifting the error token.
    goto yyerrlab1;


  /*---------------------------------------------------.
  | yyerrorlab -- error raised explicitly by YYERROR.  |
  `---------------------------------------------------*/
  yyerrorlab:

    /* Pacify compilers like GCC when the user code never invokes
       YYERROR and the label yyerrorlab therefore never appears in user
       code.  */
    if (false)
      goto yyerrorlab;
    yyerror_range[1].location = yystack_[yylen - 1].location;
    /* Do not reclaim the symbols of the rule whose action triggered
       this YYERROR.  */
    yypop_ (yylen);
    yylen = 0;
    goto yyerrlab1;

  /*-------------------------------------------------------------.
  | yyerrlab1 -- common code for both syntax error and YYERROR.  |
  `-------------------------------------------------------------*/
  yyerrlab1:
    yyerrstatus_ = 3;   // Each real token shifted decrements this.
    {
      stack_symbol_type error_token;
      for (;;)
        {
          yyn = yypact_[yystack_[0].state];
          if (!yy_pact_value_is_default_ (yyn))
            {
              yyn += yyterror_;
              if (0 <= yyn && yyn <= yylast_ && yycheck_[yyn] == yyterror_)
                {
                  yyn = yytable_[yyn];
                  if (0 < yyn)
                    break;
                }
            }

          // Pop the current state because it cannot handle the error token.
          if (yystack_.size () == 1)
            YYABORT;

          yyerror_range[1].location = yystack_[0].location;
          yy_destroy_ ("Error: popping", yystack_[0]);
          yypop_ ();
          YY_STACK_PRINT ();
        }

      yyerror_range[2].location = yyla.location;
      YYLLOC_DEFAULT (error_token.location, yyerror_range, 2);

      // Shift the error token.
      error_token.state = yyn;
      yypush_ ("Shifting", error_token);
    }
    goto yynewstate;

    // Accept.
  yyacceptlab:
    yyresult = 0;
    goto yyreturn;

    // Abort.
  yyabortlab:
    yyresult = 1;
    goto yyreturn;

  yyreturn:
    if (!yyla.empty ())
      yy_destroy_ ("Cleanup: discarding lookahead", yyla);

    /* Do not reclaim the symbols of the rule whose action triggered
       this YYABORT or YYACCEPT.  */
    yypop_ (yylen);
    while (1 < yystack_.size ())
      {
        yy_destroy_ ("Cleanup: popping", yystack_[0]);
        yypop_ ();
      }

    return yyresult;
  }
    catch (...)
      {
        YYCDEBUG << "Exception caught: cleaning lookahead and stack"
                 << std::endl;
        // Do not try to display the values of the reclaimed symbols,
        // as their printer might throw an exception.
        if (!yyla.empty ())
          yy_destroy_ (YY_NULLPTR, yyla);

        while (1 < yystack_.size ())
          {
            yy_destroy_ (YY_NULLPTR, yystack_[0]);
            yypop_ ();
          }
        throw;
      }
  }

  void
  whereParser::error (const syntax_error& yyexc)
  {
    error (yyexc.location, yyexc.what());
  }

  // Generate an error message.
  std::string
  whereParser::yysyntax_error_ (state_type yystate, const symbol_type& yyla) const
  {
    // Number of reported tokens (one for the "unexpected", one per
    // "expected").
    size_t yycount = 0;
    // Its maximum.
    enum { YYERROR_VERBOSE_ARGS_MAXIMUM = 5 };
    // Arguments of yyformat.
    char const *yyarg[YYERROR_VERBOSE_ARGS_MAXIMUM];

    /* There are many possibilities here to consider:
       - If this state is a consistent state with a default action, then
         the only way this function was invoked is if the default action
         is an error action.  In that case, don't check for expected
         tokens because there are none.
       - The only way there can be no lookahead present (in yyla) is
         if this state is a consistent state with a default action.
         Thus, detecting the absence of a lookahead is sufficient to
         determine that there is no unexpected or expected token to
         report.  In that case, just report a simple "syntax error".
       - Don't assume there isn't a lookahead just because this state is
         a consistent state with a default action.  There might have
         been a previous inconsistent state, consistent state with a
         non-default action, or user semantic action that manipulated
         yyla.  (However, yyla is currently not documented for users.)
       - Of course, the expected token list depends on states to have
         correct lookahead information, and it depends on the parser not
         to perform extra reductions after fetching a lookahead from the
         scanner and before detecting a syntax error.  Thus, state
         merging (from LALR or IELR) and default reductions corrupt the
         expected token list.  However, the list is correct for
         canonical LR with one exception: it will still contain any
         token that will not be accepted due to an error action in a
         later state.
    */
    if (!yyla.empty ())
      {
        int yytoken = yyla.type_get ();
        yyarg[yycount++] = yytname_[yytoken];
        int yyn = yypact_[yystate];
        if (!yy_pact_value_is_default_ (yyn))
          {
            /* Start YYX at -YYN if negative to avoid negative indexes in
               YYCHECK.  In other words, skip the first -YYN actions for
               this state because they are default actions.  */
            int yyxbegin = yyn < 0 ? -yyn : 0;
            // Stay within bounds of both yycheck and yytname.
            int yychecklim = yylast_ - yyn + 1;
            int yyxend = yychecklim < yyntokens_ ? yychecklim : yyntokens_;
            for (int yyx = yyxbegin; yyx < yyxend; ++yyx)
              if (yycheck_[yyx + yyn] == yyx && yyx != yyterror_
                  && !yy_table_value_is_error_ (yytable_[yyx + yyn]))
                {
                  if (yycount == YYERROR_VERBOSE_ARGS_MAXIMUM)
                    {
                      yycount = 1;
                      break;
                    }
                  else
                    yyarg[yycount++] = yytname_[yyx];
                }
          }
      }

    char const* yyformat = YY_NULLPTR;
    switch (yycount)
      {
#define YYCASE_(N, S)                         \
        case N:                               \
          yyformat = S;                       \
        break
        YYCASE_(0, YY_("syntax error"));
        YYCASE_(1, YY_("syntax error, unexpected %s"));
        YYCASE_(2, YY_("syntax error, unexpected %s, expecting %s"));
        YYCASE_(3, YY_("syntax error, unexpected %s, expecting %s or %s"));
        YYCASE_(4, YY_("syntax error, unexpected %s, expecting %s or %s or %s"));
        YYCASE_(5, YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s"));
#undef YYCASE_
      }

    std::string yyres;
    // Argument number.
    size_t yyi = 0;
    for (char const* yyp = yyformat; *yyp; ++yyp)
      if (yyp[0] == '%' && yyp[1] == 's' && yyi < yycount)
        {
          yyres += yytnamerr_ (yyarg[yyi++]);
          ++yyp;
        }
      else
        yyres += *yyp;
    return yyres;
  }


  const signed char whereParser::yypact_ninf_ = -44;

  const signed char whereParser::yytable_ninf_ = -1;

  const short int
  whereParser::yypact_[] =
  {
      76,    76,   -13,   -43,   -41,   -23,   -14,    -7,    -2,    25,
     240,   240,   -44,    65,    68,    76,    23,   -44,   -44,   -44,
     118,    26,   -44,   -44,   -44,   140,   240,   240,    29,    60,
     240,   240,    67,    73,   240,   -44,   -44,     0,   188,   214,
     132,    70,   176,   240,    82,    89,    -5,    58,   -44,    76,
      76,    76,    76,   -44,   240,   240,   240,   240,   240,   240,
     240,   240,   240,   240,   240,   240,   240,   240,   240,   -44,
     107,   117,   276,   297,   129,   131,   305,   325,   222,   333,
     -44,   234,   -44,   -44,   -44,   361,   -44,   -44,   -44,   361,
     -44,   -44,   -44,   186,   -44,   -44,   -44,   -44,    39,   -44,
     -44,   268,   -44,   -44,   -44,   -44,   -44,   -44,    28,    83,
     127,   165,   157,   173,   361,   361,   260,   288,   136,   -19,
     -19,   148,   148,   148,   148,   -44,   -44,   179,   189,   205,
     215,   -44,   -44,     1,   -44,   -44,   -44,   -44,    77,    84,
     168,   174,   194,   200,   -44,   240,   240,   240,   240,   240,
     240,   240,   240,   240,   240,   228,   236,   267,   296,   247,
     302,   220,   265,   294,   -44,   187,   -44,   202,   -44,   308,
     -44,   212,   -44,   213,   353,   361,   361,   361,   361,   361,
     361,   361,   361,   361,   -44,   -44,   -44,   -44,   -44,   -44,
     -44,   309,   -44,   238,   -44,   243,   301,   303,   304,   322,
     323,   324,   326,   327,   328,   -44,   329,   330,   331,   332,
     350,   -44,   -44,   -44,   -44,   -44,   -44,   -44,   -44,   -44,
     -44,   -44,   -44,   -44,   -44
  };

  const unsigned char
  whereParser::yydefact_[] =
  {
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    98,    97,     0,     0,     0,     8,     9,    10,
       0,     0,     6,    11,    12,     0,     0,     0,     0,     0,
       0,     0,     0,    97,     0,    95,    94,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    99,     0,
       0,     0,     0,   100,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     1,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      18,     0,    53,    55,    59,    61,    54,    56,    60,    62,
      42,    50,    43,     0,    38,    40,    15,    22,     0,    29,
      30,     0,    57,    58,     7,    96,     4,     5,     2,     3,
      66,    68,    65,    67,    63,    64,     0,    85,    84,    78,
      79,    80,    81,    82,    83,    13,    14,     0,     0,     0,
       0,    91,    90,     0,    39,    41,    19,    31,     0,     0,
       0,     0,     0,     0,    86,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    44,     0,    45,     0,    17,     0,
      27,     0,    28,     0,     0,    72,    71,    76,    75,    70,
      69,    74,    73,    77,    89,    88,    93,    92,    51,    52,
      21,     0,    36,     0,    37,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    87,     0,     0,     0,     0,
       0,    49,    48,    47,    46,    16,    23,    25,    24,    26,
      20,    32,    34,    33,    35
  };

  const signed char
  whereParser::yypgoto_[] =
  {
     -44,    10,   -44,   -44,   -44,   -10,   -44
  };

  const signed char
  whereParser::yydefgoto_[] =
  {
      -1,    16,    17,    18,    19,    20,    21
  };

  const unsigned char
  whereParser::yytable_[] =
  {
      35,    36,    26,    80,    27,    47,    49,    50,    51,    52,
     159,    22,    65,    66,    67,    68,    72,    73,    81,   160,
      76,    77,    28,    48,    79,    46,    69,    23,    85,    89,
      24,    29,    25,   101,    49,    50,    51,    52,    30,    49,
      50,   104,    52,    31,   110,   111,   112,   113,   114,   115,
     116,   117,   118,   119,   120,   121,   122,   123,   124,   106,
     107,   108,   109,    54,    55,    56,    57,    58,    59,    37,
      32,    53,    74,    60,    38,    39,   141,    44,    45,   142,
       1,    40,   143,    41,    42,    61,    62,    63,    64,    65,
      66,    67,    68,     2,    49,    50,     3,     4,     5,     6,
       7,     8,     9,    75,   105,    10,    11,    78,    94,    95,
      43,    96,    97,    12,   161,    98,    13,   162,    43,    14,
     163,    15,   102,    54,    55,    56,    57,    58,    59,   103,
     164,   165,   146,    60,   147,   174,   175,   176,   177,   178,
     179,   180,   181,   182,   183,    61,    62,    63,    64,    65,
      66,    67,    68,   125,    61,    62,    63,    64,    65,    66,
      67,    68,   150,   126,   151,    63,    64,    65,    66,    67,
      68,   148,    90,   149,    91,    92,   129,    93,   130,   152,
      70,   153,    68,    71,    61,    62,    63,    64,    65,    66,
      67,    68,    61,    62,    63,    64,    65,    66,    67,    68,
      61,    62,    63,    64,    65,    66,    67,    68,     3,     4,
       5,     6,     7,     8,   166,   167,    99,    10,    11,   100,
     168,   169,   155,    82,    83,    12,   139,   196,    33,   140,
     197,    84,   156,    34,     3,     4,     5,     6,     7,     8,
     170,   171,   198,    10,    11,   199,   172,   173,   157,    86,
      87,    12,   201,   203,    33,   202,   204,    88,   158,    34,
       3,     4,     5,     6,     7,     8,   190,   191,   133,    10,
      11,   154,   134,   135,   184,   136,   137,    12,   207,   138,
      33,   208,   185,   209,   188,    34,   210,    61,    62,    63,
      64,    65,    66,    67,    68,    61,    62,    63,    64,    65,
      66,    67,    68,    61,    62,    63,    64,    65,    66,    67,
      68,   192,   193,   186,   144,   145,    62,    63,    64,    65,
      66,    67,    68,   127,    61,    62,    63,    64,    65,    66,
      67,    68,    61,    62,    63,    64,    65,    66,    67,    68,
     194,   195,   187,   189,   128,   200,   206,   211,     0,   212,
     213,   131,    61,    62,    63,    64,    65,    66,    67,    68,
      61,    62,    63,    64,    65,    66,    67,    68,   214,   215,
     216,   132,   217,   218,   219,   220,   221,   222,   223,   105,
      61,    62,    63,    64,    65,    66,    67,    68,    61,    62,
      63,    64,    65,    66,    67,    68,   224,     0,     0,   205
  };

  const short int
  whereParser::yycheck_[] =
  {
      10,    11,    45,     3,    45,    15,    11,    12,    13,    14,
       9,     1,    31,    32,    33,    34,    26,    27,    18,    18,
      30,    31,    45,     0,    34,    15,     0,    40,    38,    39,
      43,    45,    45,    43,    11,    12,    13,    14,    45,    11,
      12,    46,    14,    45,    54,    55,    56,    57,    58,    59,
      60,    61,    62,    63,    64,    65,    66,    67,    68,    49,
      50,    51,    52,     5,     6,     7,     8,     9,    10,     4,
      45,    48,    43,    15,     9,    10,    37,     9,    10,    40,
       4,    16,    43,    18,    19,    27,    28,    29,    30,    31,
      32,    33,    34,    17,    11,    12,    20,    21,    22,    23,
      24,    25,    26,    43,    46,    29,    30,    40,    38,    39,
      45,    41,    42,    37,    37,    45,    40,    40,    45,    43,
      43,    45,    40,     5,     6,     7,     8,     9,    10,    40,
      46,    47,     5,    15,     7,   145,   146,   147,   148,   149,
     150,   151,   152,   153,   154,    27,    28,    29,    30,    31,
      32,    33,    34,    46,    27,    28,    29,    30,    31,    32,
      33,    34,     5,    46,     7,    29,    30,    31,    32,    33,
      34,     6,    40,     8,    42,    43,    47,    45,    47,     6,
      40,     8,    34,    43,    27,    28,    29,    30,    31,    32,
      33,    34,    27,    28,    29,    30,    31,    32,    33,    34,
      27,    28,    29,    30,    31,    32,    33,    34,    20,    21,
      22,    23,    24,    25,    46,    47,    40,    29,    30,    43,
      46,    47,    43,    35,    36,    37,    40,    40,    40,    43,
      43,    43,    43,    45,    20,    21,    22,    23,    24,    25,
      46,    47,    40,    29,    30,    43,    46,    47,    43,    35,
      36,    37,    40,    40,    40,    43,    43,    43,    43,    45,
      20,    21,    22,    23,    24,    25,    46,    47,    46,    29,
      30,    11,    38,    39,    46,    41,    42,    37,    40,    45,
      40,    43,    46,    40,    37,    45,    43,    27,    28,    29,
      30,    31,    32,    33,    34,    27,    28,    29,    30,    31,
      32,    33,    34,    27,    28,    29,    30,    31,    32,    33,
      34,    46,    47,    46,    46,    47,    28,    29,    30,    31,
      32,    33,    34,    47,    27,    28,    29,    30,    31,    32,
      33,    34,    27,    28,    29,    30,    31,    32,    33,    34,
      46,    47,    46,    41,    47,    37,    37,    46,    -1,    46,
      46,    46,    27,    28,    29,    30,    31,    32,    33,    34,
      27,    28,    29,    30,    31,    32,    33,    34,    46,    46,
      46,    46,    46,    46,    46,    46,    46,    46,    46,    46,
      27,    28,    29,    30,    31,    32,    33,    34,    27,    28,
      29,    30,    31,    32,    33,    34,    46,    -1,    -1,    46
  };

  const unsigned char
  whereParser::yystos_[] =
  {
       0,     4,    17,    20,    21,    22,    23,    24,    25,    26,
      29,    30,    37,    40,    43,    45,    50,    51,    52,    53,
      54,    55,    50,    40,    43,    45,    45,    45,    45,    45,
      45,    45,    45,    40,    45,    54,    54,     4,     9,    10,
      16,    18,    19,    45,     9,    10,    50,    54,     0,    11,
      12,    13,    14,    48,     5,     6,     7,     8,     9,    10,
      15,    27,    28,    29,    30,    31,    32,    33,    34,     0,
      40,    43,    54,    54,    43,    43,    54,    54,    40,    54,
       3,    18,    35,    36,    43,    54,    35,    36,    43,    54,
      40,    42,    43,    45,    38,    39,    41,    42,    45,    40,
      43,    54,    40,    40,    46,    46,    50,    50,    50,    50,
      54,    54,    54,    54,    54,    54,    54,    54,    54,    54,
      54,    54,    54,    54,    54,    46,    46,    47,    47,    47,
      47,    46,    46,    46,    38,    39,    41,    42,    45,    40,
      43,    37,    40,    43,    46,    47,     5,     7,     6,     8,
       5,     7,     6,     8,    11,    43,    43,    43,    43,     9,
      18,    37,    40,    43,    46,    47,    46,    47,    46,    47,
      46,    47,    46,    47,    54,    54,    54,    54,    54,    54,
      54,    54,    54,    54,    46,    46,    46,    46,    37,    41,
      46,    47,    46,    47,    46,    47,    40,    43,    40,    43,
      37,    40,    43,    40,    43,    46,    37,    40,    43,    40,
      43,    46,    46,    46,    46,    46,    46,    46,    46,    46,
      46,    46,    46,    46,    46
  };

  const unsigned char
  whereParser::yyr1_[] =
  {
       0,    49,    50,    50,    50,    50,    50,    50,    50,    50,
      50,    51,    51,    51,    51,    51,    51,    51,    51,    51,
      51,    51,    51,    51,    51,    51,    51,    51,    51,    51,
      51,    51,    51,    51,    51,    51,    51,    51,    51,    51,
      51,    51,    51,    51,    51,    51,    51,    51,    51,    51,
      51,    51,    51,    51,    51,    51,    51,    51,    51,    51,
      51,    51,    51,    52,    52,    52,    52,    52,    52,    53,
      53,    53,    53,    53,    53,    53,    53,    53,    54,    54,
      54,    54,    54,    54,    54,    54,    54,    54,    54,    54,
      54,    54,    54,    54,    54,    54,    54,    54,    54,    55,
      55
  };

  const unsigned char
  whereParser::yyr2_[] =
  {
       0,     2,     3,     3,     3,     3,     2,     3,     1,     1,
       1,     2,     2,     4,     4,     3,     7,     5,     3,     4,
       8,     6,     3,     7,     7,     7,     7,     5,     5,     3,
       3,     4,     8,     8,     8,     8,     6,     6,     3,     4,
       3,     4,     3,     3,     5,     5,     7,     7,     7,     7,
       3,     6,     6,     3,     3,     3,     3,     3,     3,     3,
       3,     3,     3,     3,     3,     3,     3,     3,     3,     5,
       5,     5,     5,     5,     5,     5,     5,     5,     3,     3,
       3,     3,     3,     3,     3,     3,     4,     6,     6,     6,
       4,     4,     6,     6,     2,     2,     3,     1,     1,     2,
       2
  };



  // YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
  // First, the terminals, then, starting at \a yyntokens_, nonterminals.
  const char*
  const whereParser::yytname_[] =
  {
  "\"end of input\"", "error", "$undefined", "\"null\"", "\"not\"",
  "\"<=\"", "\">=\"", "\"<\"", "\">\"", "\"==\"", "\"!=\"", "\"and\"",
  "\"&!\"", "\"or\"", "\"xor\"", "\"between\"", "\"contains\"",
  "\"exists\"", "\"in\"", "\"like\"", "\"FROM_UNIXTIME_GMT\"",
  "\"FROM_UNIXTIME_LOCAL\"", "\"TO_UNIXTIME_GMT\"",
  "\"TO_UNIXTIME_LOCAL\"", "\"ISO_TO_UNIXTIME_GMT\"",
  "\"ISO_TO_UNIXTIME_LOCAL\"", "\"any\"", "\"|\"", "\"&\"", "\"+\"",
  "\"-\"", "\"*\"", "\"/\"", "\"%\"", "\"**\"", "\"integer value\"",
  "\"unsigned integer value\"", "\"floating-point number\"",
  "\"signed integer sequence\"", "\"unsigned integer sequence\"",
  "\"name string\"", "\"number sequence\"", "\"string sequence\"",
  "\"string literal\"", "CONSTAINSOP", "'('", "')'", "','", "';'",
  "$accept", "qexpr", "simpleRange", "compRange2", "compRange3",
  "mathExpr", "START", YY_NULLPTR
  };

#if YYDEBUG
  const unsigned short int
  whereParser::yyrline_[] =
  {
       0,   115,   115,   125,   135,   145,   155,   163,   166,   167,
     168,   172,   180,   188,   196,   204,   214,   226,   235,   242,
     253,   266,   276,   286,   303,   320,   337,   354,   368,   382,
     392,   402,   413,   431,   449,   467,   485,   500,   515,   525,
     536,   546,   557,   567,   577,   587,   597,   608,   619,   630,
     641,   651,   660,   670,   678,   687,   695,   704,   714,   725,
     735,   746,   763,   786,   796,   807,   817,   827,   837,   899,
     911,   923,   935,   947,   959,   971,   983,   995,  1010,  1022,
    1034,  1046,  1058,  1070,  1082,  1094,  1106,  1118,  1131,  1144,
    1158,  1171,  1184,  1225,  1262,  1272,  1275,  1278,  1288,  1298,
    1301
  };

  // Print the state stack on the debug stream.
  void
  whereParser::yystack_print_ ()
  {
    *yycdebug_ << "Stack now";
    for (stack_type::const_iterator
           i = yystack_.begin (),
           i_end = yystack_.end ();
         i != i_end; ++i)
      *yycdebug_ << ' ' << i->state;
    *yycdebug_ << std::endl;
  }

  // Report on the debug stream that the rule \a yyrule is going to be reduced.
  void
  whereParser::yy_reduce_print_ (int yyrule)
  {
    unsigned int yylno = yyrline_[yyrule];
    int yynrhs = yyr2_[yyrule];
    // Print the symbols being reduced, and their result.
    *yycdebug_ << "Reducing stack by rule " << yyrule - 1
               << " (line " << yylno << "):" << std::endl;
    // The symbols being reduced.
    for (int yyi = 0; yyi < yynrhs; yyi++)
      YY_SYMBOL_PRINT ("   $" << yyi + 1 << " =",
                       yystack_[(yynrhs) - (yyi + 1)]);
  }
#endif // YYDEBUG

  // Symbol number corresponding to token number t.
  inline
  whereParser::token_number_type
  whereParser::yytranslate_ (int t)
  {
    static
    const token_number_type
    translate_table[] =
    {
     0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
      45,    46,     2,     2,    47,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,    48,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44
    };
    const unsigned int user_token_number_max_ = 299;
    const token_number_type undef_token_ = 2;

    if (static_cast<int>(t) <= yyeof_)
      return yyeof_;
    else if (static_cast<unsigned int> (t) <= user_token_number_max_)
      return translate_table[t];
    else
      return undef_token_;
  }

#line 25 "whereParser.yy" // lalr1.cc:1167
} // ibis
#line 2923 "whereParser.cc" // lalr1.cc:1167
#line 1306 "whereParser.yy" // lalr1.cc:1168

void ibis::whereParser::error(const ibis::whereParser::location_type& l,
			      const std::string& m) {
    LOGGER(ibis::gVerbose >= 0)
	<< "Warning -- ibis::whereParser encountered " << m
	<< " at location " << l;
}
