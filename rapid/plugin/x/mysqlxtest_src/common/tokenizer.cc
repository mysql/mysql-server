/*
 * Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#include "tokenizer.h"

#include <stdexcept>
#include <memory>
#include <cstdlib>
#include <cctype>
#include <cstring>
#include <cstdlib>

#ifndef WIN32
#  include <strings.h>
#  define _stricmp strcasecmp
#endif

// Avoid warnings from protobuf and rapidjson
#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wconversion"
// TODO: Once expr parser does not have deps on std::auto_ptr
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#ifndef __has_warning
#define __has_warning(x) 0
#endif
#if __has_warning("-Wunused-local-typedefs")
#pragma GCC diagnostic ignored "-Wunused-local-typedefs"
#endif
#elif defined _MSC_VER
#pragma warning (push)
#pragma warning (disable : 4018 4996) //TODO: add MSVC code for pedantic
#endif

#include <boost/format.hpp>
/*#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/case_conv.hpp>*/

#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)
#pragma GCC diagnostic pop
#elif defined _MSC_VER
#pragma warning (pop)
#endif


using namespace mysqlx;

struct Tokenizer::Maps Tokenizer::map;

Tokenizer::Maps::Maps()
{
  reserved_words["and"] = Token::AND;
  reserved_words["or"] = Token::OR;
  reserved_words["xor"] = Token::XOR;
  reserved_words["is"] = Token::IS;
  reserved_words["not"] = Token::NOT;
  reserved_words["like"] = Token::LIKE;
  reserved_words["in"] = Token::IN_;
  reserved_words["regexp"] = Token::REGEXP;
  reserved_words["between"] = Token::BETWEEN;
  reserved_words["interval"] = Token::INTERVAL;
  reserved_words["escape"] = Token::ESCAPE;
  reserved_words["div"] = Token::DIV;
  reserved_words["hex"] = Token::HEX;
  reserved_words["bin"] = Token::BIN;
  reserved_words["true"] = Token::TRUE_;
  reserved_words["false"] = Token::FALSE_;
  reserved_words["null"] = Token::T_NULL;
  reserved_words["second"] = Token::SECOND;
  reserved_words["minute"] = Token::MINUTE;
  reserved_words["hour"] = Token::HOUR;
  reserved_words["day"] = Token::DAY;
  reserved_words["week"] = Token::WEEK;
  reserved_words["month"] = Token::MONTH;
  reserved_words["quarter"] = Token::QUARTER;
  reserved_words["year"] = Token::YEAR;
  reserved_words["microsecond"] = Token::MICROSECOND;
  reserved_words["as"] = Token::AS;
  reserved_words["asc"] = Token::ASC;
  reserved_words["desc"] = Token::DESC;
  reserved_words["cast"] = Token::CAST;
  reserved_words["character"] = Token::CHARACTER;
  reserved_words["set"] = Token::SET;
  reserved_words["charset"] = Token::CHARSET;
  reserved_words["ascii"] = Token::ASCII;
  reserved_words["unicode"] = Token::UNICODE;
  reserved_words["byte"] = Token::BYTE;
  reserved_words["binary"] = Token::BINARY;
  reserved_words["char"] = Token::CHAR;
  reserved_words["nchar"] = Token::NCHAR;
  reserved_words["date"] = Token::DATE;
  reserved_words["datetime"] = Token::DATETIME;
  reserved_words["time"] = Token::TIME;
  reserved_words["decimal"] = Token::DECIMAL;
  reserved_words["signed"] = Token::SIGNED;
  reserved_words["unsigned"] = Token::UNSIGNED;
  reserved_words["integer"] = Token::INTEGER;
  reserved_words["int"] = Token::INTEGER;
  reserved_words["json"] = Token::JSON;

  interval_units.insert(Token::MICROSECOND);
  interval_units.insert(Token::SECOND);
  interval_units.insert(Token::MINUTE);
  interval_units.insert(Token::HOUR);
  interval_units.insert(Token::DAY);
  interval_units.insert(Token::WEEK);
  interval_units.insert(Token::MONTH);
  interval_units.insert(Token::QUARTER);
  interval_units.insert(Token::YEAR);

  operator_names["="] = "==";
  operator_names["and"] = "&&";
  operator_names["or"] = "||";
  operator_names["not"] = "not";
  operator_names["xor"] = "xor";
  operator_names["is"] = "is";
  operator_names["between"] = "between";
  operator_names["in"] = "in";
  operator_names["like"] = "like";
  operator_names["!="] = "!=";
  operator_names["<>"] = "!=";
  operator_names[">"] = ">";
  operator_names[">="] = ">=";
  operator_names["<"] = "<";
  operator_names["<="] = "<=";
  operator_names["&"] = "&";
  operator_names["|"] = "|";
  operator_names["<<"] = "<<";
  operator_names[">>"] = ">>";
  operator_names["+"] = "+";
  operator_names["-"] = "-";
  operator_names["*"] = "*";
  operator_names["/"] = "/";
  operator_names["~"] = "~";
  operator_names["%"] = "%";

  unary_operator_names["+"] = "sign_plus";
  unary_operator_names["-"] = "sign_minus";
  unary_operator_names["~"] = "~";
  unary_operator_names["not"] = "not";
}

Token::Token(Token::TokenType type, const std::string& text, int cur_pos) : _type(type), _text(text), _pos(cur_pos)
{
}

struct Tokenizer::Maps map;

Tokenizer::Tokenizer(const std::string& input) : _input(input)
{
  _pos = 0;
}

bool Tokenizer::next_char_is(tokens_t::size_type i, int tok)
{
  return (i + 1) < _input.size() && _input[i + 1] == tok;
}

void Tokenizer::assert_cur_token(Token::TokenType type)
{
  assert_tok_position();
  const Token& tok = _tokens.at(_pos);
  Token::TokenType tok_type = tok.get_type();
  if (tok_type != type)
    throw Parser_error((boost::format("Expected token type %d at position %d but found type %d (%s).") % type % tok.get_pos() % tok_type % tok.get_text()).str());
}

bool Tokenizer::cur_token_type_is(Token::TokenType type)
{
  return pos_token_type_is(_pos, type);
}

bool Tokenizer::next_token_type(Token::TokenType type)
{
  return pos_token_type_is(_pos + 1, type);
}

bool Tokenizer::pos_token_type_is(tokens_t::size_type pos, Token::TokenType type)
{
  return (pos < _tokens.size()) && (_tokens[pos].get_type() == type);
}

const std::string& Tokenizer::consume_token(Token::TokenType type)
{
  assert_cur_token(type);
  const std::string& v = _tokens[_pos++].get_text();
  return v;
}

const Token& Tokenizer::peek_token()
{
  assert_tok_position();
  Token& t = _tokens[_pos];
  return t;
}

void Tokenizer::unget_token()
{
  if (_pos == 0)
    throw Parser_error("Attempt to get back a token when already at first token (position 0).");
  --_pos;
}

void Tokenizer::get_tokens()
{
  bool arrow_last = false;
  bool inside_arrow = false;
  for (size_t i = 0; i < _input.size(); ++i)
  {
    char c = _input[i];
    if (std::isspace(c))
    {
      // do nothing
      continue;
    }
    else if (std::isdigit(c))
    {
      // numerical literal
      int start = i;
      // floating grammar is
      // float -> int '.' (int | (int expo[sign] int))
      // int -> digit +
      // expo -> 'E' | 'e'
      // sign -> '-' | '+'
      while (i < _input.size() && std::isdigit(c = _input[i]))
        ++i;
      if (i < _input.size() && _input[i] == '.')
      {
        ++i;
        while (i < _input.size() && std::isdigit(_input[i]))
          ++i;
        if (i < _input.size() && std::toupper(_input[i]) == 'E')
        {
          ++i;
          if (i < _input.size() && (((c = _input[i]) == '-') || (c == '+')))
            ++i;
          size_t  j = i;
          while (i < _input.size() && std::isdigit(_input[i]))
            i++;
          if (i == j)
            throw Parser_error((boost::format("Tokenizer: Missing exponential value for floating point at char %d") % i).str());
        }
        _tokens.push_back(Token(Token::LNUM, std::string(_input, start, i - start), i));
      }
      else
      {
        _tokens.push_back(Token(Token::LINTEGER, std::string(_input, start, i - start), i));
      }
      if (i < _input.size())
        --i;
    }
    else if (!std::isalpha(c) && c != '_')
    {
      // # non-identifier, e.g. operator or quoted literal
      if (c == '?')
      {
        _tokens.push_back(Token(Token::PLACEHOLDER, std::string(1, c), i));
      }
      else if (c == '+')
      {
        _tokens.push_back(Token(Token::PLUS, std::string(1, c), i));
      }
      else if (c == '-')
      {
        if (!arrow_last && next_char_is(i, '>'))
        {
          ++i;
          _tokens.push_back(Token(Token::ARROW, "->", i));
          arrow_last = true;
          continue;
        }
        else
          _tokens.push_back(Token(Token::MINUS, std::string(1, c), i));
      }
      else if (c == '*')
      {
        if (next_char_is(i, '*'))
        {
          ++i;
          _tokens.push_back(Token(Token::DOUBLESTAR, std::string("**"), i));
        }
        else
        {
          _tokens.push_back(Token(Token::MUL, std::string(1, c), i));
        }
      }
      else if (c == '/')
      {
        _tokens.push_back(Token(Token::DIV, std::string(1, c), i));
      }
      else if (c == '$')
      {
        _tokens.push_back(Token(Token::DOLLAR, std::string(1, c), i));
      }
      else if (c == '%')
      {
        _tokens.push_back(Token(Token::MOD, std::string(1, c), i));
      }
      else if (c == '=')
      {
        _tokens.push_back(Token(Token::EQ, std::string(1, c), i));
      }
      else if (c == '&')
      {
        _tokens.push_back(Token(Token::BITAND, std::string(1, c), i));
      }
      else if (c == '|')
      {
        _tokens.push_back(Token(Token::BITOR, std::string(1, c), i));
      }
      else if (c == '(')
      {
        _tokens.push_back(Token(Token::LPAREN, std::string(1, c), i));
      }
      else if (c == ')')
      {
        _tokens.push_back(Token(Token::RPAREN, std::string(1, c), i));
      }
      else if (c == '[')
      {
        _tokens.push_back(Token(Token::LSQBRACKET, std::string(1, c), i));
      }
      else if (c == ']')
      {
        _tokens.push_back(Token(Token::RSQBRACKET, std::string(1, c), i));
      }
      else if (c == '{')
      {
        _tokens.push_back(Token(Token::LCURLY, std::string(1, c), i));
      }
      else if (c == '}')
      {
        _tokens.push_back(Token(Token::RCURLY, std::string(1, c), i));
      }
      else if (c == '~')
      {
        _tokens.push_back(Token(Token::NEG, std::string(1, c), i));
      }
      else if (c == ',')
      {
        _tokens.push_back(Token(Token::COMMA, std::string(1, c), i));
      }
      else if (c == ':')
      {
        _tokens.push_back(Token(Token::COLON, std::string(1, c), i));
      }
      else if (c == '!')
      {
        if (next_char_is(i, '='))
        {
          ++i;
          _tokens.push_back(Token(Token::NE, std::string("!="), i));
        }
        else
        {
          _tokens.push_back(Token(Token::BANG, std::string(1, c), i));
        }
      }
      else if (c == '<')
      {
        if (next_char_is(i, '<'))
        {
          ++i;
          _tokens.push_back(Token(Token::LSHIFT, std::string("<<"), i));
        }
        else if (next_char_is(i, '='))
        {
          ++i;
          _tokens.push_back(Token(Token::LE, std::string("<="), i));
        }
        else if (next_char_is(i, '>'))
        {
          ++i;
          _tokens.push_back(Token(Token::NE, std::string("!="), i));
        }
        else
        {
          _tokens.push_back(Token(Token::LT, std::string("<"), i));
        }
      }
      else if (c == '>')
      {
        if (next_char_is(i, '>'))
        {
          ++i;
          _tokens.push_back(Token(Token::RSHIFT, std::string(">>"), i));
        }
        else if (next_char_is(i, '='))
        {
          ++i;
          _tokens.push_back(Token(Token::GE, std::string(">="), i));
        }
        else
        {
          _tokens.push_back(Token(Token::GT, std::string(1, c), i));
        }
      }
      else if (c == '.')
      {
        if ((i + 1) < _input.size() && std::isdigit(_input[i + 1]))
        {
          size_t start = i;
          ++i;
          // floating grammar is
          // float -> '.' (int | (int expo[sign] int))
          // nint->digit +
          // expo -> 'E' | 'e'
          // sign -> '-' | '+'
          while (i < _input.size() && std::isdigit(_input[i]))
            ++i;
          if (i < _input.size() && std::toupper(_input[i]) == 'E')
          {
            ++i;
            if (i < _input.size() && (((c = _input[i]) == '+') || (c == '-')))
              ++i;
            size_t j = i;
            while (i < _input.size() && std::isdigit(_input[i]))
              ++i;
            if (i == j)
              throw Parser_error((boost::format("Tokenizer: Missing exponential value for floating point at char %d") % i).str());
          }
          _tokens.push_back(Token(Token::LNUM, std::string(_input, start, i - start), i));
          if (i < _input.size())
            --i;
        }
        else
        {
          _tokens.push_back(Token(Token::DOT, std::string(1, c), i));
        }
      }
      else if (c == '\'' && arrow_last)
      {
        _tokens.push_back(Token(Token::QUOTE, "'", i));
        if (!inside_arrow)
          inside_arrow = true;
        else
        {
          arrow_last = false;
          inside_arrow = false;
        }
      }
      else if (c == '"' || c == '\'' || c == '`')
      {
        char quote_char = c;
        std::string val;
        size_t start = ++i;

        while (i < _input.size())
        {
          c = _input[i];
          if ((c == quote_char) && ((i + 1) < _input.size()) && (_input[i + 1] != quote_char))
          {
            // break if we have a quote char that's not double
            break;
          }
          else if ((c == quote_char) || (c == '\\'  && quote_char != '`'))
          {
            // && quote_char != '`'
            // this quote char has to be doubled
            if ((i + 1) >= _input.size())
              break;
            val.append(1, _input[++i]);
          }
          else
            val.append(1, c);
          ++i;
        }
        if ((i >= _input.size()) && (_input[i] != quote_char))
        {
          throw Parser_error((boost::format("Unterminated quoted string starting at position %d") % start).str());
        }
        if (quote_char == '`')
        {
          _tokens.push_back(Token(Token::IDENT, val, i));
        }
        else
        {
          _tokens.push_back(Token(Token::LSTRING, val, i));
        }
      }
      else
      {
        throw Parser_error((boost::format("Unknown character at %d") % i).str());
      }
    }
    else
    {
      size_t start = i;
      while (i < _input.size() && (std::isalnum(_input[i]) || _input[i] == '_'))
        ++i;
      std::string val(_input, start, i - start);
      Maps::reserved_words_t::const_iterator it = map.reserved_words.find(val);
      if (it != map.reserved_words.end())
      {
        _tokens.push_back(Token(it->second, val, i));
      }
      else
      {
        _tokens.push_back(Token(Token::IDENT, val, i));
      }
      --i;
    }
  }
}

void Tokenizer::inc_pos_token()
{
  ++_pos;
}

const Token& Tokenizer::consume_any_token()
{
  assert_tok_position();
  Token& tok = _tokens[_pos];
  ++_pos;
  return tok;
}

void Tokenizer::assert_tok_position()
{
  if (_pos >= _tokens.size())
    throw Parser_error((boost::format("Expected token at position %d but no tokens left.") % _pos).str());
}

bool Tokenizer::tokens_available()
{
  return _pos < _tokens.size();
}

bool Tokenizer::is_interval_units_type()
{
  assert_tok_position();
  Token::TokenType type = _tokens[_pos].get_type();
  return map.interval_units.find(type) != map.interval_units.end();
}

bool Tokenizer::is_type_within_set(const std::set<Token::TokenType>& types)
{
  assert_tok_position();
  Token::TokenType type = _tokens[_pos].get_type();
  return types.find(type) != types.end();
}

bool Tokenizer::Cmp_icase::operator()(const std::string& lhs, const std::string& rhs) const
{
  const char *c_lhs = lhs.c_str();
  const char *c_rhs = rhs.c_str();

  return _stricmp(c_lhs, c_rhs) < 0;
}
