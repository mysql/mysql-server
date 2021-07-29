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
#line 6 "selectParser.yy" // lalr1.cc:397

/** \file Defines the parser for the select clause accepted by FastBit
    IBIS.  The definitions are processed through bison.
*/
#include <iostream>

#line 41 "selectParser.cc" // lalr1.cc:397


// First part of user declarations.

#line 46 "selectParser.cc" // lalr1.cc:404

# ifndef YY_NULLPTR
#  if defined __cplusplus && 201103L <= __cplusplus
#   define YY_NULLPTR nullptr
#  else
#   define YY_NULLPTR 0
#  endif
# endif

#include "selectParser.hh"

// User implementation prologue.
#line 70 "selectParser.yy" // lalr1.cc:412

#include "selectLexer.h"

#undef yylex
#define yylex driver.lexer->lex

#line 66 "selectParser.cc" // lalr1.cc:412


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

#line 23 "selectParser.yy" // lalr1.cc:479
namespace ibis {
#line 152 "selectParser.cc" // lalr1.cc:479

  /* Return YYSTR after stripping away unnecessary quotes and
     backslashes, so that it's suitable for yyerror.  The heuristic is
     that double-quoting is unnecessary unless the string contains an
     apostrophe, a comma, or backslash (other than backslash-backslash).
     YYSTR is taken from yytname.  */
  std::string
  selectParser::yytnamerr_ (const char *yystr)
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
  selectParser::selectParser (class ibis::selectClause& driver_yyarg)
    :
#if YYDEBUG
      yydebug_ (false),
      yycdebug_ (&std::cerr),
#endif
      driver (driver_yyarg)
  {}

  selectParser::~selectParser ()
  {}


  /*---------------.
  | Symbol types.  |
  `---------------*/

  inline
  selectParser::syntax_error::syntax_error (const location_type& l, const std::string& m)
    : std::runtime_error (m)
    , location (l)
  {}

  // basic_symbol.
  template <typename Base>
  inline
  selectParser::basic_symbol<Base>::basic_symbol ()
    : value ()
  {}

  template <typename Base>
  inline
  selectParser::basic_symbol<Base>::basic_symbol (const basic_symbol& other)
    : Base (other)
    , value ()
    , location (other.location)
  {
    value = other.value;
  }


  template <typename Base>
  inline
  selectParser::basic_symbol<Base>::basic_symbol (typename Base::kind_type t, const semantic_type& v, const location_type& l)
    : Base (t)
    , value (v)
    , location (l)
  {}


  /// Constructor for valueless symbols.
  template <typename Base>
  inline
  selectParser::basic_symbol<Base>::basic_symbol (typename Base::kind_type t, const location_type& l)
    : Base (t)
    , value ()
    , location (l)
  {}

  template <typename Base>
  inline
  selectParser::basic_symbol<Base>::~basic_symbol ()
  {
    clear ();
  }

  template <typename Base>
  inline
  void
  selectParser::basic_symbol<Base>::clear ()
  {
    Base::clear ();
  }

  template <typename Base>
  inline
  bool
  selectParser::basic_symbol<Base>::empty () const
  {
    return Base::type_get () == empty_symbol;
  }

  template <typename Base>
  inline
  void
  selectParser::basic_symbol<Base>::move (basic_symbol& s)
  {
    super_type::move(s);
    value = s.value;
    location = s.location;
  }

  // by_type.
  inline
  selectParser::by_type::by_type ()
    : type (empty_symbol)
  {}

  inline
  selectParser::by_type::by_type (const by_type& other)
    : type (other.type)
  {}

  inline
  selectParser::by_type::by_type (token_type t)
    : type (yytranslate_ (t))
  {}

  inline
  void
  selectParser::by_type::clear ()
  {
    type = empty_symbol;
  }

  inline
  void
  selectParser::by_type::move (by_type& that)
  {
    type = that.type;
    that.clear ();
  }

  inline
  int
  selectParser::by_type::type_get () const
  {
    return type;
  }


  // by_state.
  inline
  selectParser::by_state::by_state ()
    : state (empty_state)
  {}

  inline
  selectParser::by_state::by_state (const by_state& other)
    : state (other.state)
  {}

  inline
  void
  selectParser::by_state::clear ()
  {
    state = empty_state;
  }

  inline
  void
  selectParser::by_state::move (by_state& that)
  {
    state = that.state;
    that.clear ();
  }

  inline
  selectParser::by_state::by_state (state_type s)
    : state (s)
  {}

  inline
  selectParser::symbol_number_type
  selectParser::by_state::type_get () const
  {
    if (state == empty_state)
      return empty_symbol;
    else
      return yystos_[state];
  }

  inline
  selectParser::stack_symbol_type::stack_symbol_type ()
  {}


  inline
  selectParser::stack_symbol_type::stack_symbol_type (state_type s, symbol_type& that)
    : super_type (s, that.location)
  {
    value = that.value;
    // that is emptied.
    that.type = empty_symbol;
  }

  inline
  selectParser::stack_symbol_type&
  selectParser::stack_symbol_type::operator= (const stack_symbol_type& that)
  {
    state = that.state;
    value = that.value;
    location = that.location;
    return *this;
  }


  template <typename Base>
  inline
  void
  selectParser::yy_destroy_ (const char* yymsg, basic_symbol<Base>& yysym) const
  {
    if (yymsg)
      YY_SYMBOL_PRINT (yymsg, yysym);

    // User destructor.
    switch (yysym.type_get ())
    {
            case 13: // "name"

#line 67 "selectParser.yy" // lalr1.cc:614
        { delete (yysym.value.stringVal); }
#line 405 "selectParser.cc" // lalr1.cc:614
        break;

      case 14: // "string literal"

#line 66 "selectParser.yy" // lalr1.cc:614
        { delete (yysym.value.stringVal); }
#line 412 "selectParser.cc" // lalr1.cc:614
        break;

      case 23: // mathExpr

#line 68 "selectParser.yy" // lalr1.cc:614
        { delete (yysym.value.selectNode); }
#line 419 "selectParser.cc" // lalr1.cc:614
        break;


      default:
        break;
    }
  }

#if YYDEBUG
  template <typename Base>
  void
  selectParser::yy_print_ (std::ostream& yyo,
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
  selectParser::yypush_ (const char* m, state_type s, symbol_type& sym)
  {
    stack_symbol_type t (s, sym);
    yypush_ (m, t);
  }

  inline
  void
  selectParser::yypush_ (const char* m, stack_symbol_type& s)
  {
    if (m)
      YY_SYMBOL_PRINT (m, s);
    yystack_.push (s);
  }

  inline
  void
  selectParser::yypop_ (unsigned int n)
  {
    yystack_.pop (n);
  }

#if YYDEBUG
  std::ostream&
  selectParser::debug_stream () const
  {
    return *yycdebug_;
  }

  void
  selectParser::set_debug_stream (std::ostream& o)
  {
    yycdebug_ = &o;
  }


  selectParser::debug_level_type
  selectParser::debug_level () const
  {
    return yydebug_;
  }

  void
  selectParser::set_debug_level (debug_level_type l)
  {
    yydebug_ = l;
  }
#endif // YYDEBUG

  inline selectParser::state_type
  selectParser::yy_lr_goto_state_ (state_type yystate, int yysym)
  {
    int yyr = yypgoto_[yysym - yyntokens_] + yystate;
    if (0 <= yyr && yyr <= yylast_ && yycheck_[yyr] == yystate)
      return yytable_[yyr];
    else
      return yydefgoto_[yysym - yyntokens_];
  }

  inline bool
  selectParser::yy_pact_value_is_default_ (int yyvalue)
  {
    return yyvalue == yypact_ninf_;
  }

  inline bool
  selectParser::yy_table_value_is_error_ (int yyvalue)
  {
    return yyvalue == yytable_ninf_;
  }

  int
  selectParser::parse ()
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
    #line 28 "selectParser.yy" // lalr1.cc:741
{ // initialize location object
    yyla.location.begin.filename = yyla.location.end.filename = &(driver.clause_);
}

#line 556 "selectParser.cc" // lalr1.cc:741

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
  case 4:
#line 79 "selectParser.yy" // lalr1.cc:859
    {
    driver.addTerm((yystack_[1].value.selectNode), 0);
}
#line 668 "selectParser.cc" // lalr1.cc:859
    break;

  case 5:
#line 82 "selectParser.yy" // lalr1.cc:859
    {
    driver.addTerm((yystack_[1].value.selectNode), 0);
}
#line 676 "selectParser.cc" // lalr1.cc:859
    break;

  case 6:
#line 85 "selectParser.yy" // lalr1.cc:859
    {
    driver.addTerm((yystack_[2].value.selectNode), (yystack_[1].value.stringVal));
    delete (yystack_[1].value.stringVal);
}
#line 685 "selectParser.cc" // lalr1.cc:859
    break;

  case 7:
#line 89 "selectParser.yy" // lalr1.cc:859
    {
    driver.addTerm((yystack_[2].value.selectNode), (yystack_[1].value.stringVal));
    delete (yystack_[1].value.stringVal);
}
#line 694 "selectParser.cc" // lalr1.cc:859
    break;

  case 8:
#line 93 "selectParser.yy" // lalr1.cc:859
    {
    driver.addTerm((yystack_[3].value.selectNode), (yystack_[1].value.stringVal));
    delete (yystack_[1].value.stringVal);
}
#line 703 "selectParser.cc" // lalr1.cc:859
    break;

  case 9:
#line 97 "selectParser.yy" // lalr1.cc:859
    {
    driver.addTerm((yystack_[3].value.selectNode), (yystack_[1].value.stringVal));
    delete (yystack_[1].value.stringVal);
}
#line 712 "selectParser.cc" // lalr1.cc:859
    break;

  case 10:
#line 104 "selectParser.yy" // lalr1.cc:859
    {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *(yystack_[2].value.selectNode)
	<< " + " << *(yystack_[0].value.selectNode);
#endif
    ibis::math::bediener *opr =
	new ibis::math::bediener(ibis::math::PLUS);
    opr->setRight((yystack_[0].value.selectNode));
    opr->setLeft((yystack_[2].value.selectNode));
    (yylhs.value.selectNode) = opr;
}
#line 729 "selectParser.cc" // lalr1.cc:859
    break;

  case 11:
#line 116 "selectParser.yy" // lalr1.cc:859
    {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *(yystack_[2].value.selectNode)
	<< " - " << *(yystack_[0].value.selectNode);
#endif
    ibis::math::bediener *opr =
	new ibis::math::bediener(ibis::math::MINUS);
    opr->setRight((yystack_[0].value.selectNode));
    opr->setLeft((yystack_[2].value.selectNode));
    (yylhs.value.selectNode) = opr;
}
#line 746 "selectParser.cc" // lalr1.cc:859
    break;

  case 12:
#line 128 "selectParser.yy" // lalr1.cc:859
    {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *(yystack_[2].value.selectNode)
	<< " * " << *(yystack_[0].value.selectNode);
#endif
    ibis::math::bediener *opr =
	new ibis::math::bediener(ibis::math::MULTIPLY);
    opr->setRight((yystack_[0].value.selectNode));
    opr->setLeft((yystack_[2].value.selectNode));
    (yylhs.value.selectNode) = opr;
}
#line 763 "selectParser.cc" // lalr1.cc:859
    break;

  case 13:
#line 140 "selectParser.yy" // lalr1.cc:859
    {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *(yystack_[2].value.selectNode)
	<< " / " << *(yystack_[0].value.selectNode);
#endif
    ibis::math::bediener *opr =
	new ibis::math::bediener(ibis::math::DIVIDE);
    opr->setRight((yystack_[0].value.selectNode));
    opr->setLeft((yystack_[2].value.selectNode));
    (yylhs.value.selectNode) = opr;
}
#line 780 "selectParser.cc" // lalr1.cc:859
    break;

  case 14:
#line 152 "selectParser.yy" // lalr1.cc:859
    {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *(yystack_[2].value.selectNode)
	<< " % " << *(yystack_[0].value.selectNode);
#endif
    ibis::math::bediener *opr =
	new ibis::math::bediener(ibis::math::REMAINDER);
    opr->setRight((yystack_[0].value.selectNode));
    opr->setLeft((yystack_[2].value.selectNode));
    (yylhs.value.selectNode) = opr;
}
#line 797 "selectParser.cc" // lalr1.cc:859
    break;

  case 15:
#line 164 "selectParser.yy" // lalr1.cc:859
    {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *(yystack_[2].value.selectNode)
	<< " ^ " << *(yystack_[0].value.selectNode);
#endif
    ibis::math::bediener *opr =
	new ibis::math::bediener(ibis::math::POWER);
    opr->setRight((yystack_[0].value.selectNode));
    opr->setLeft((yystack_[2].value.selectNode));
    (yylhs.value.selectNode) = opr;
}
#line 814 "selectParser.cc" // lalr1.cc:859
    break;

  case 16:
#line 176 "selectParser.yy" // lalr1.cc:859
    {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *(yystack_[2].value.selectNode)
	<< " & " << *(yystack_[0].value.selectNode);
#endif
    ibis::math::bediener *opr =
	new ibis::math::bediener(ibis::math::BITAND);
    opr->setRight((yystack_[0].value.selectNode));
    opr->setLeft((yystack_[2].value.selectNode));
    (yylhs.value.selectNode) = opr;
}
#line 831 "selectParser.cc" // lalr1.cc:859
    break;

  case 17:
#line 188 "selectParser.yy" // lalr1.cc:859
    {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *(yystack_[2].value.selectNode)
	<< " | " << *(yystack_[0].value.selectNode);
#endif
    ibis::math::bediener *opr =
	new ibis::math::bediener(ibis::math::BITOR);
    opr->setRight((yystack_[0].value.selectNode));
    opr->setLeft((yystack_[2].value.selectNode));
    (yylhs.value.selectNode) = opr;
}
#line 848 "selectParser.cc" // lalr1.cc:859
    break;

  case 18:
#line 200 "selectParser.yy" // lalr1.cc:859
    {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *(yystack_[3].value.stringVal) << "(*)";
#endif
    ibis::math::term *fun = 0;
    if (stricmp((yystack_[3].value.stringVal)->c_str(), "count") == 0) { // aggregation count
	ibis::math::variable *var = new ibis::math::variable("*");
	fun = driver.addAgregado(ibis::selectClause::CNT, var);
    }
    else {
	LOGGER(ibis::gVerbose >= 0)
	    << "Warning -- only operator COUNT supports * as the argument, "
	    "but received " << *(yystack_[3].value.stringVal);
	throw "invalid use of (*)";
    }
    delete (yystack_[3].value.stringVal);
    (yylhs.value.selectNode) = fun;
}
#line 872 "selectParser.cc" // lalr1.cc:859
    break;

  case 19:
#line 219 "selectParser.yy" // lalr1.cc:859
    {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *(yystack_[3].value.stringVal) << "("
	<< *(yystack_[1].value.selectNode) << ")";
#endif
    ibis::math::term *fun = 0;
    if (stricmp((yystack_[3].value.stringVal)->c_str(), "count") == 0) { // aggregation count
	delete (yystack_[1].value.selectNode); // drop the expression, replace it with "*"
	ibis::math::variable *var = new ibis::math::variable("*");
	fun = driver.addAgregado(ibis::selectClause::CNT, var);
    }
    else if (stricmp((yystack_[3].value.stringVal)->c_str(), "max") == 0) { // aggregation max
	fun = driver.addAgregado(ibis::selectClause::MAX, (yystack_[1].value.selectNode));
    }
    else if (stricmp((yystack_[3].value.stringVal)->c_str(), "min") == 0) { // aggregation min
	fun = driver.addAgregado(ibis::selectClause::MIN, (yystack_[1].value.selectNode));
    }
    else if (stricmp((yystack_[3].value.stringVal)->c_str(), "sum") == 0) { // aggregation sum
	fun = driver.addAgregado(ibis::selectClause::SUM, (yystack_[1].value.selectNode));
    }
    else if (stricmp((yystack_[3].value.stringVal)->c_str(), "median") == 0) { // aggregation median
	fun = driver.addAgregado(ibis::selectClause::MEDIAN, (yystack_[1].value.selectNode));
    }
    else if (stricmp((yystack_[3].value.stringVal)->c_str(), "countd") == 0 ||
	     stricmp((yystack_[3].value.stringVal)->c_str(), "countdistinct") == 0) {
	// count distinct values
	fun = driver.addAgregado(ibis::selectClause::DISTINCT, (yystack_[1].value.selectNode));
    }
    else if (stricmp((yystack_[3].value.stringVal)->c_str(), "concat") == 0 ||
	     stricmp((yystack_[3].value.stringVal)->c_str(), "group_concat") == 0) {
	// concatenate all values as ASCII strings
	fun = driver.addAgregado(ibis::selectClause::CONCAT, (yystack_[1].value.selectNode));
    }
    else if (stricmp((yystack_[3].value.stringVal)->c_str(), "avg") == 0) { // aggregation avg
	ibis::math::term *numer =
	    driver.addAgregado(ibis::selectClause::SUM, (yystack_[1].value.selectNode));
	ibis::math::variable *var = new ibis::math::variable("*");
	ibis::math::term *denom =
	    driver.addAgregado(ibis::selectClause::CNT, var);
	ibis::math::bediener *opr =
	    new ibis::math::bediener(ibis::math::DIVIDE);
	opr->setRight(denom);
	opr->setLeft(numer);
	fun = opr;
    }
    else if (stricmp((yystack_[3].value.stringVal)->c_str(), "varp") == 0 ||
	     stricmp((yystack_[3].value.stringVal)->c_str(), "varpop") == 0) {
	// population variance is computed as
	// fabs(sum (x^2) / count(*) - (sum (x) / count(*))^2)
	ibis::math::term *x = (yystack_[1].value.selectNode);
	ibis::math::number *two = new ibis::math::number(2.0);
	ibis::math::variable *star = new ibis::math::variable("*");
	ibis::math::term *t11 = new ibis::math::bediener(ibis::math::POWER);
	t11->setLeft(x);
	t11->setRight(two);
	t11 = driver.addAgregado(ibis::selectClause::SUM, t11);
	ibis::math::term *t12 =
	    driver.addAgregado(ibis::selectClause::CNT, star);
	ibis::math::term *t13 = new ibis::math::bediener(ibis::math::DIVIDE);
	t13->setLeft(t11);
	t13->setRight(t12);
	ibis::math::term *t21 =
	    driver.addAgregado(ibis::selectClause::SUM, x->dup());
	ibis::math::term *t23 = new ibis::math::bediener(ibis::math::DIVIDE);
	t23->setLeft(t21);
	t23->setRight(t12->dup());
	ibis::math::term *t24 = new ibis::math::bediener(ibis::math::POWER);
	t24->setLeft(t23);
	t24->setRight(two->dup());
        ibis::math::term *t0 = new ibis::math::bediener(ibis::math::MINUS);
	t0->setLeft(t13);
	t0->setRight(t24);
        fun = new ibis::math::stdFunction1("fabs");
        fun->setLeft(t0);
	//fun = driver.addAgregado(ibis::selectClause::VARPOP, $3);
    }
    else if (stricmp((yystack_[3].value.stringVal)->c_str(), "var") == 0 ||
	     stricmp((yystack_[3].value.stringVal)->c_str(), "varsamp") == 0 ||
	     stricmp((yystack_[3].value.stringVal)->c_str(), "variance") == 0) {
	// sample variance is computed as
	// fabs((sum (x^2) / count(*) - (sum (x) / count(*))^2) * (count(*) / (count(*)-1)))
	ibis::math::term *x = (yystack_[1].value.selectNode);
	ibis::math::number *two = new ibis::math::number(2.0);
	ibis::math::variable *star = new ibis::math::variable("*");
	ibis::math::term *t11 = new ibis::math::bediener(ibis::math::POWER);
	t11->setLeft(x);
	t11->setRight(two);
	t11 = driver.addAgregado(ibis::selectClause::SUM, t11);
	ibis::math::term *t12 =
	    driver.addAgregado(ibis::selectClause::CNT, star);
	ibis::math::term *t13 = new ibis::math::bediener(ibis::math::DIVIDE);
	t13->setLeft(t11);
	t13->setRight(t12);
	ibis::math::term *t21 =
	    driver.addAgregado(ibis::selectClause::SUM, x->dup());
	ibis::math::term *t23 = new ibis::math::bediener(ibis::math::DIVIDE);
	t23->setLeft(t21);
	t23->setRight(t12->dup());
	ibis::math::term *t24 = new ibis::math::bediener(ibis::math::POWER);
	t24->setLeft(t23);
	t24->setRight(two->dup());
	ibis::math::term *t31 = new ibis::math::bediener(ibis::math::MINUS);
	t31->setLeft(t13);
	t31->setRight(t24);
	ibis::math::term *t32 = new ibis::math::bediener(ibis::math::MINUS);
	ibis::math::number *one = new ibis::math::number(1.0);
	t32->setLeft(t12->dup());
	t32->setRight(one);
	ibis::math::term *t33 = new ibis::math::bediener(ibis::math::DIVIDE);
	t33->setLeft(t12->dup());
	t33->setRight(t32);
        ibis::math::term *t0 = new ibis::math::bediener(ibis::math::MULTIPLY);
	t0->setLeft(t31);
	t0->setRight(t33);
        fun = new ibis::math::stdFunction1("fabs");
        fun->setLeft(t0);
	//fun = driver.addAgregado(ibis::selectClause::VARSAMP, $3);
    }
    else if (stricmp((yystack_[3].value.stringVal)->c_str(), "stdevp") == 0 ||
	     stricmp((yystack_[3].value.stringVal)->c_str(), "stdpop") == 0) {
	// population standard deviation is computed as
	// sqrt(fabs(sum (x^2) / count(*) - (sum (x) / count(*))^2))
	ibis::math::term *x = (yystack_[1].value.selectNode);
	ibis::math::number *two = new ibis::math::number(2.0);
	ibis::math::variable *star = new ibis::math::variable("*");
	ibis::math::term *t11 = new ibis::math::bediener(ibis::math::POWER);
	t11->setLeft(x);
	t11->setRight(two);
	t11 = driver.addAgregado(ibis::selectClause::SUM, t11);
	ibis::math::term *t12 =
	    driver.addAgregado(ibis::selectClause::CNT, star);
	ibis::math::term *t13 = new ibis::math::bediener(ibis::math::DIVIDE);
	t13->setLeft(t11);
	t13->setRight(t12);
	ibis::math::term *t21 =
	    driver.addAgregado(ibis::selectClause::SUM, x->dup());
	ibis::math::term *t23 = new ibis::math::bediener(ibis::math::DIVIDE);
	t23->setLeft(t21);
	t23->setRight(t12->dup());
	ibis::math::term *t24 = new ibis::math::bediener(ibis::math::POWER);
	t24->setLeft(t23);
	t24->setRight(two->dup());
	ibis::math::term *t31 = new ibis::math::bediener(ibis::math::MINUS);
	t31->setLeft(t13);
	t31->setRight(t24);
        ibis::math::term *t0 = new ibis::math::stdFunction1("fabs");
        t0->setLeft(t31);
	fun = new ibis::math::stdFunction1("sqrt");
	fun->setLeft(t0);
	//fun = driver.addAgregado(ibis::selectClause::STDPOP, $3);
    }
    else if (stricmp((yystack_[3].value.stringVal)->c_str(), "std") == 0 ||
	     stricmp((yystack_[3].value.stringVal)->c_str(), "stdev") == 0 ||
	     stricmp((yystack_[3].value.stringVal)->c_str(), "stddev") == 0 ||
	     stricmp((yystack_[3].value.stringVal)->c_str(), "stdsamp") == 0) {
	// sample standard deviation is computed as
	// sqrt(fabs(sum (x^2) / count(*) - (sum (x) / count(*))^2) * (count(*) / (count(*)-1))))
	ibis::math::term *x = (yystack_[1].value.selectNode);
	ibis::math::number *two = new ibis::math::number(2.0);
	ibis::math::variable *star = new ibis::math::variable("*");
	ibis::math::term *t11 = new ibis::math::bediener(ibis::math::POWER);
	t11->setLeft(x);
	t11->setRight(two);
	t11 = driver.addAgregado(ibis::selectClause::SUM, t11);
	ibis::math::term *t12 =
	    driver.addAgregado(ibis::selectClause::CNT, star);
	ibis::math::term *t13 = new ibis::math::bediener(ibis::math::DIVIDE);
	t13->setLeft(t11);
	t13->setRight(t12);
	ibis::math::term *t21 =
	    driver.addAgregado(ibis::selectClause::SUM, x->dup());
	ibis::math::term *t23 = new ibis::math::bediener(ibis::math::DIVIDE);
	t23->setLeft(t21);
	t23->setRight(t12->dup());
	ibis::math::term *t24 = new ibis::math::bediener(ibis::math::POWER);
	t24->setLeft(t23);
	t24->setRight(two->dup());
	ibis::math::term *t31 = new ibis::math::bediener(ibis::math::MINUS);
	t31->setLeft(t13);
	t31->setRight(t24);
	ibis::math::term *t32 = new ibis::math::bediener(ibis::math::MINUS);
	ibis::math::number *one = new ibis::math::number(1.0);
	t32->setLeft(t12->dup());
	t32->setRight(one);
	ibis::math::term *t33 = new ibis::math::bediener(ibis::math::DIVIDE);
	t33->setLeft(t12->dup());
	t33->setRight(t32);
	ibis::math::term *t34 = new ibis::math::bediener(ibis::math::MULTIPLY);
	t34->setLeft(t31);
	t34->setRight(t33);
        ibis::math::term *t0 = new ibis::math::stdFunction1("fabs");
        t0->setLeft(t34);
	fun = new ibis::math::stdFunction1("sqrt");
	fun->setLeft(t0);
	// fun = driver.addAgregado(ibis::selectClause::STDSAMP, $3);
    }
    else { // assume it is a standard math function
	fun = new ibis::math::stdFunction1((yystack_[3].value.stringVal)->c_str());
	fun->setLeft((yystack_[1].value.selectNode));
    }
    delete (yystack_[3].value.stringVal);
    (yylhs.value.selectNode) = fun;
}
#line 1081 "selectParser.cc" // lalr1.cc:859
    break;

  case 20:
#line 423 "selectParser.yy" // lalr1.cc:859
    {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- FORMAT_UNIXTIME_GMT("
	<< *(yystack_[3].value.selectNode) << ", " << *(yystack_[1].value.stringVal) << ")";
#endif
    ibis::math::formatUnixTime fut((yystack_[1].value.stringVal)->c_str(), "GMT");
    ibis::math::stringFunction1 *fun = new ibis::math::stringFunction1(fut);
    fun->setLeft((yystack_[3].value.selectNode));
    (yylhs.value.selectNode) = fun;
    delete (yystack_[1].value.stringVal);
}
#line 1098 "selectParser.cc" // lalr1.cc:859
    break;

  case 21:
#line 435 "selectParser.yy" // lalr1.cc:859
    {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- FORMAT_UNIXTIME_GMT("
	<< *(yystack_[3].value.selectNode) << ", " << *(yystack_[1].value.stringVal) << ")";
#endif
    ibis::math::formatUnixTime fut((yystack_[1].value.stringVal)->c_str(), "GMT");
    ibis::math::stringFunction1 *fun = new ibis::math::stringFunction1(fut);
    fun->setLeft((yystack_[3].value.selectNode));
    (yylhs.value.selectNode) = fun;
    delete (yystack_[1].value.stringVal);
}
#line 1115 "selectParser.cc" // lalr1.cc:859
    break;

  case 22:
#line 447 "selectParser.yy" // lalr1.cc:859
    {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- FORMAT_UNIXTIME_LOCAL("
	<< *(yystack_[3].value.selectNode) << ", " << *(yystack_[1].value.stringVal) << ")";
#endif
    ibis::math::formatUnixTime fut((yystack_[1].value.stringVal)->c_str());
    ibis::math::stringFunction1 *fun = new ibis::math::stringFunction1(fut);
    fun->setLeft((yystack_[3].value.selectNode));
    (yylhs.value.selectNode) = fun;
    delete (yystack_[1].value.stringVal);
}
#line 1132 "selectParser.cc" // lalr1.cc:859
    break;

  case 23:
#line 459 "selectParser.yy" // lalr1.cc:859
    {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- FORMAT_UNIXTIME_LOCAL("
	<< *(yystack_[3].value.selectNode) << ", " << *(yystack_[1].value.stringVal) << ")";
#endif
    ibis::math::formatUnixTime fut((yystack_[1].value.stringVal)->c_str());
    ibis::math::stringFunction1 *fun = new ibis::math::stringFunction1(fut);
    fun->setLeft((yystack_[3].value.selectNode));
    (yylhs.value.selectNode) = fun;
    delete (yystack_[1].value.stringVal);
}
#line 1149 "selectParser.cc" // lalr1.cc:859
    break;

  case 24:
#line 471 "selectParser.yy" // lalr1.cc:859
    {
    /* two-arugment math functions */
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *(yystack_[5].value.stringVal) << "("
	<< *(yystack_[3].value.selectNode) << ", " << *(yystack_[1].value.selectNode) << ")";
#endif
    ibis::math::stdFunction2 *fun =
	new ibis::math::stdFunction2((yystack_[5].value.stringVal)->c_str());
    fun->setRight((yystack_[1].value.selectNode));
    fun->setLeft((yystack_[3].value.selectNode));
    (yylhs.value.selectNode) = fun;
    delete (yystack_[5].value.stringVal);
}
#line 1168 "selectParser.cc" // lalr1.cc:859
    break;

  case 25:
#line 485 "selectParser.yy" // lalr1.cc:859
    {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- - " << *(yystack_[0].value.selectNode);
#endif
    ibis::math::bediener *opr =
	new ibis::math::bediener(ibis::math::NEGATE);
    opr->setRight((yystack_[0].value.selectNode));
    (yylhs.value.selectNode) = opr;
}
#line 1183 "selectParser.cc" // lalr1.cc:859
    break;

  case 26:
#line 495 "selectParser.yy" // lalr1.cc:859
    {
    (yylhs.value.selectNode) = (yystack_[0].value.selectNode);
}
#line 1191 "selectParser.cc" // lalr1.cc:859
    break;

  case 27:
#line 498 "selectParser.yy" // lalr1.cc:859
    {
    (yylhs.value.selectNode) = (yystack_[1].value.selectNode);
}
#line 1199 "selectParser.cc" // lalr1.cc:859
    break;

  case 28:
#line 501 "selectParser.yy" // lalr1.cc:859
    {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " got a variable name " << *(yystack_[0].value.stringVal);
#endif
    (yylhs.value.selectNode) = new ibis::math::variable((yystack_[0].value.stringVal)->c_str());
    delete (yystack_[0].value.stringVal);
}
#line 1212 "selectParser.cc" // lalr1.cc:859
    break;

  case 29:
#line 509 "selectParser.yy" // lalr1.cc:859
    {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " got a string literal " << *(yystack_[0].value.stringVal);
#endif
    (yylhs.value.selectNode) = new ibis::math::literal((yystack_[0].value.stringVal)->c_str());
    delete (yystack_[0].value.stringVal);
}
#line 1225 "selectParser.cc" // lalr1.cc:859
    break;

  case 30:
#line 517 "selectParser.yy" // lalr1.cc:859
    {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " got a number " << (yystack_[0].value.doubleVal);
#endif
    (yylhs.value.selectNode) = new ibis::math::number((yystack_[0].value.doubleVal));
}
#line 1237 "selectParser.cc" // lalr1.cc:859
    break;


#line 1241 "selectParser.cc" // lalr1.cc:859
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
  selectParser::error (const syntax_error& yyexc)
  {
    error (yyexc.location, yyexc.what());
  }

  // Generate an error message.
  std::string
  selectParser::yysyntax_error_ (state_type yystate, const symbol_type& yyla) const
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


  const signed char selectParser::yypact_ninf_ = -13;

  const signed char selectParser::yytable_ninf_ = -1;

  const signed char
  selectParser::yypact_[] =
  {
     126,   126,   126,   -13,   -12,   -13,    -6,    -2,   126,    30,
     126,    29,    20,    20,   113,   126,   126,    61,   -13,   -13,
     -13,    28,   126,   126,   126,   126,   126,   126,   126,   126,
       2,   -13,    24,    45,    93,   107,   -13,     3,    68,    83,
       0,     0,    20,    20,    20,    20,   -13,   -13,   -13,   126,
     -13,    -9,     4,   -13,   -13,    77,    25,    26,    38,    39,
     -13,   -13,   -13,   -13,   -13
  };

  const unsigned char
  selectParser::yydefact_[] =
  {
       0,     0,     0,    30,    28,    29,     0,     0,     0,     0,
       2,     0,    26,    25,     0,     0,     0,     0,     1,     3,
       5,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     4,     0,     0,     0,     0,    27,     0,    17,    16,
      10,    11,    12,    13,    14,    15,     7,     6,    18,     0,
      19,     0,     0,     9,     8,     0,     0,     0,     0,     0,
      24,    20,    21,    22,    23
  };

  const signed char
  selectParser::yypgoto_[] =
  {
     -13,    37,   -13,    -1
  };

  const signed char
  selectParser::yydefgoto_[] =
  {
      -1,     9,    10,    11
  };

  const unsigned char
  selectParser::yytable_[] =
  {
      12,    13,    46,    53,    56,    57,    14,    17,    26,    27,
      28,    29,    15,    33,    34,    35,    16,    58,    59,    47,
      54,    38,    39,    40,    41,    42,    43,    44,    45,    20,
      18,    29,    21,    22,    23,    24,    25,    26,    27,    28,
      29,    37,    30,    48,    61,    62,    31,    19,    55,    22,
      23,    24,    25,    26,    27,    28,    29,    63,    64,     0,
       0,     0,    49,     0,    50,    22,    23,    24,    25,    26,
      27,    28,    29,    23,    24,    25,    26,    27,    28,    29,
      36,    22,    23,    24,    25,    26,    27,    28,    29,    24,
      25,    26,    27,    28,    29,     0,    60,    22,    23,    24,
      25,    26,    27,    28,    29,     0,     0,     0,     0,     0,
      51,    22,    23,    24,    25,    26,    27,    28,    29,     1,
       2,    32,     0,     0,    52,     3,     4,     5,     6,     7,
       0,     8,     1,     2,     0,     0,     0,     0,     3,     4,
       5,     6,     7,     0,     8
  };

  const signed char
  selectParser::yycheck_[] =
  {
       1,     2,     0,     0,    13,    14,    18,     8,     8,     9,
      10,    11,    18,    14,    15,    16,    18,    13,    14,    17,
      17,    22,    23,    24,    25,    26,    27,    28,    29,     0,
       0,    11,     3,     4,     5,     6,     7,     8,     9,    10,
      11,    13,    13,    19,    19,    19,    17,    10,    49,     4,
       5,     6,     7,     8,     9,    10,    11,    19,    19,    -1,
      -1,    -1,    17,    -1,    19,     4,     5,     6,     7,     8,
       9,    10,    11,     5,     6,     7,     8,     9,    10,    11,
      19,     4,     5,     6,     7,     8,     9,    10,    11,     6,
       7,     8,     9,    10,    11,    -1,    19,     4,     5,     6,
       7,     8,     9,    10,    11,    -1,    -1,    -1,    -1,    -1,
      17,     4,     5,     6,     7,     8,     9,    10,    11,     6,
       7,     8,    -1,    -1,    17,    12,    13,    14,    15,    16,
      -1,    18,     6,     7,    -1,    -1,    -1,    -1,    12,    13,
      14,    15,    16,    -1,    18
  };

  const unsigned char
  selectParser::yystos_[] =
  {
       0,     6,     7,    12,    13,    14,    15,    16,    18,    21,
      22,    23,    23,    23,    18,    18,    18,    23,     0,    21,
       0,     3,     4,     5,     6,     7,     8,     9,    10,    11,
      13,    17,     8,    23,    23,    23,    19,    13,    23,    23,
      23,    23,    23,    23,    23,    23,     0,    17,    19,    17,
      19,    17,    17,     0,    17,    23,    13,    14,    13,    14,
      19,    19,    19,    19,    19
  };

  const unsigned char
  selectParser::yyr1_[] =
  {
       0,    20,    21,    21,    22,    22,    22,    22,    22,    22,
      23,    23,    23,    23,    23,    23,    23,    23,    23,    23,
      23,    23,    23,    23,    23,    23,    23,    23,    23,    23,
      23
  };

  const unsigned char
  selectParser::yyr2_[] =
  {
       0,     2,     1,     2,     2,     2,     3,     3,     4,     4,
       3,     3,     3,     3,     3,     3,     3,     3,     4,     4,
       6,     6,     6,     6,     6,     2,     2,     3,     1,     1,
       1
  };



  // YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
  // First, the terminals, then, starting at \a yyntokens_, nonterminals.
  const char*
  const selectParser::yytname_[] =
  {
  "\"end of input\"", "error", "$undefined", "\"as\"", "\"|\"", "\"&\"",
  "\"+\"", "\"-\"", "\"*\"", "\"/\"", "\"%\"", "\"**\"",
  "\"numerical value\"", "\"name\"", "\"string literal\"",
  "\"FORMAT_UNIXTIME_GMT\"", "\"FORMAT_UNIXTIME_LOCAL\"", "','", "'('",
  "')'", "$accept", "slist", "sterm", "mathExpr", YY_NULLPTR
  };

#if YYDEBUG
  const unsigned short int
  selectParser::yyrline_[] =
  {
       0,    78,    78,    78,    79,    82,    85,    89,    93,    97,
     104,   116,   128,   140,   152,   164,   176,   188,   200,   219,
     423,   435,   447,   459,   471,   485,   495,   498,   501,   509,
     517
  };

  // Print the state stack on the debug stream.
  void
  selectParser::yystack_print_ ()
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
  selectParser::yy_reduce_print_ (int yyrule)
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
  selectParser::token_number_type
  selectParser::yytranslate_ (int t)
  {
    static
    const token_number_type
    translate_table[] =
    {
     0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
      18,    19,     2,     2,    17,     2,     2,     2,     2,     2,
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
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16
    };
    const unsigned int user_token_number_max_ = 271;
    const token_number_type undef_token_ = 2;

    if (static_cast<int>(t) <= yyeof_)
      return yyeof_;
    else if (static_cast<unsigned int> (t) <= user_token_number_max_)
      return translate_table[t];
    else
      return undef_token_;
  }

#line 23 "selectParser.yy" // lalr1.cc:1167
} // ibis
#line 1710 "selectParser.cc" // lalr1.cc:1167
#line 526 "selectParser.yy" // lalr1.cc:1168

void ibis::selectParser::error(const ibis::selectParser::location_type& l,
			       const std::string& m) {
    LOGGER(ibis::gVerbose >= 0)
	<< "Warning -- ibis::selectParser encountered " << m << " at location "
	<< l;
}
